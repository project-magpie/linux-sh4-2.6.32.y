/*
 * Copyright (c) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Derived from mm/bigphysarea.c which was:
 * Copyright (c) 1996 by Matt Welsh.
 * Extended by Roger Butenuth (butenuth@uni-paderborn.de), October 1997
 * Extended for linux-2.1.121 till 2.4.0 (June 2000)
 *     by Pauline Middelink <middelink@polyware.nl>
 *
 * This is a set of routines which allow you to reserve a large (?)
 * amount of physical memory at boot-time, which can be allocated/deallocated
 * by drivers. This memory is intended to be used for devices such as
 * video framegrabbers which need a lot of physical RAM (above the amount
 * allocated by kmalloc). This is by no means efficient or recommended;
 * to be used only in extreme circumstances.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/ptrace.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pfn.h>
#include <linux/bpa2.h>

struct range {
	struct range *next;
	unsigned long base;			/* base of allocated block */
	unsigned long size;			/* size in bytes */
};

struct bpa2_part {
	char res_name[20];
	struct resource res;
	const char* name;
	const char** aka;
	struct range *free_list;
	struct range *used_list;
	struct range initial_free_list;
	int low_mem;
	struct list_head next;
};

static LIST_HEAD(bpa2_parts);
static struct bpa2_part *bpa2_bigphysarea_part;
static DEFINE_SPINLOCK(bpa2_lock);

static void __init bpa2_init_failure(struct bpa2_part* bp, const char* msg)
{
	printk(KERN_ERR "bpa2: %s ignored: %s\n", bp->res_name, msg);
}

static int __init bpa2_alloc_low(struct bpa2_part* bp)
{
	void* addr;
	unsigned long size = bp->res.end - bp->res.start + 1;

	addr = alloc_bootmem_low_pages(size);
	if (addr == NULL) {
		bpa2_init_failure(bp, "could not allocate");
		return 0;
	}

	bp->res.start = virt_to_phys(addr);
	bp->res.end = virt_to_phys(addr) + size - 1;
	bp->low_mem = 1;

	return 1;
}

static int __init bpa2_init_low(struct bpa2_part* bp)
{
	void* addr;
	unsigned long size = bp->res.end - bp->res.start + 1;

	/* Can't use reserve_bootmem() because there is no return code to
	 * indicate success or failure. So use __alloc_bootmem_core(),
	 * specifying a goal, which must be available. */
	addr = __alloc_bootmem_core(NODE_DATA(0)->bdata, size, PAGE_SIZE,
				    bp->res.start, 0);

	if (addr != phys_to_virt(bp->res.start)) {
		bpa2_init_failure(bp, "could not allocate");
		if (addr) {
			free_bootmem((unsigned long)addr, size);
		}
		return 0;
	}

	bp->low_mem = 1;

	return 1;
}

static int __init bpa2_init_ext(struct bpa2_part* bp)
{
	return 1;
}

/**
 * bpa2_init - initialize bpa2 partitions
 * @partdescs: description of the partitions
 * @nparts: number of partitions
 *
 * This function initialises the bpa2 internal data structures
 * based on the partition descriptions which are passed in.
 *
 * This must be called from early in the platform initialisation
 * sequence, while bootmem is still active.
 */
void __init bpa2_init(struct bpa2_partition_desc* partdescs, int nparts)
{
	struct bpa2_part *new_parts;
	struct bpa2_part* bp;

	new_parts = alloc_bootmem(sizeof(*new_parts) * nparts);
	if (! new_parts) {
		printk(KERN_ERR "bpa2: could not allocate part table\n");
		return;
	}

	bp = new_parts;
	for ( ; nparts; nparts--) {
		unsigned long start_pfn, end_pfn;
		struct range *free_list = &bp->initial_free_list;
		int ok;

		start_pfn = PFN_UP(partdescs->start);
		end_pfn = PFN_DOWN(partdescs->start + partdescs->size);

		snprintf(bp->res_name, sizeof(bp->res_name),
			 "BPA2 (%s)", partdescs->name);
		bp->res.name = bp->res_name;
		bp->res.start = PFN_PHYS(start_pfn);
		bp->res.end = PFN_PHYS(end_pfn) - 1;
		bp->res.flags = IORESOURCE_BUSY | IORESOURCE_MEM;
		bp->name = partdescs->name;
		bp->aka = partdescs->aka;

		if (partdescs->start == 0) {
			ok = bpa2_alloc_low(bp);
		} else if ((start_pfn >= min_low_pfn) && (end_pfn <= max_low_pfn)) {
			ok = bpa2_init_low(bp);
		} else if ((start_pfn > max_low_pfn) || (end_pfn > min_low_pfn)) {
			ok = bpa2_init_ext(bp);
		} else {
			bpa2_init_failure(bp, "spans low memory boundary");
			ok = 0;
		}

		if (!ok)
			continue;

		if (insert_resource(&iomem_resource, &bp->res)) {
			bpa2_init_failure(bp, "could not reserve");
			continue;
		}

		free_list->next = NULL;
		free_list->base = bp->res.start;
		free_list->size = (bp->res.end + 1) - bp->res.start;
		bp->free_list = free_list;

		list_add_tail(&bp->next, &bpa2_parts);

		bp++;
		partdescs++;
	}

	if (bpa2_bigphysarea_part == NULL) {
		bp = bpa2_find_part("bigphysarea");

		if (bp->low_mem) {
			bpa2_bigphysarea_part = bp;
		} else {
			/* Should rate limit this I suppose */
			printk(KERN_ERR "bpa2: bigphysarea not in logical memory\n");
		}
	}
}

static int __init bpa2_setup(char *str)
{
	int par;
	struct bpa2_partition_desc partdesc = {
		.name   = "bigphysarea",
		.start  = 0,
		.size   = 0,
		.flags  = 0,
		.aka    = NULL,
	};

	if (get_option(&str,&par) == 0)
                return -EINVAL;

	partdesc.size = par << PAGE_SHIFT;
	bpa2_init(&partdesc, 1);

	return 1;
}
__setup("bigphysarea=", bpa2_setup);

/**
 * bpa2_find_part - find a bpa2 partition based on its name
 * @name: name of the partition to find
 *
 * Return the bpa2 partition corrisponding to the requested name.
 */
struct bpa2_part* bpa2_find_part(const char* name)
{
	struct bpa2_part* bp;
	const char** p;

	list_for_each_entry(bp, &bpa2_parts, next) {
		if (! strcmp(bp->name, name))
			return bp;
		if (bp->aka) {
			for (p=bp->aka; *p; p++) {
				if (! strcmp(*p, name))
					return bp;
			}
		}
	}

	return NULL;
}
EXPORT_SYMBOL(bpa2_find_part);

/**
 * bpa2_low_part - return whether a partition resides in low memory
 * @part: partition to query
 *
 * Return whether the specified patrition resides in low (that is,
 * kernel logical) memory. If it does, then functions such as
 * phys_to_virt() can be used to convert the allocated memory into
 * a virtual address which can be directly dereferenced.
 *
 * If this is not true, then the region will not be mapped into
 * the kernel's address space, and so if access is required it will
 * need to be mapped using ioremap() and accessed using readl() etc.
 */
int bpa2_low_part(struct bpa2_part* part)
{
	return part->low_mem;
}
EXPORT_SYMBOL(bpa2_low_part);

/**
 * bpa2_alloc_pages - allocate pages from a bpa2 partition
 * @bp: partition to allocate from
 * @count: number of pages to allocate
 * @align: required alinment
 * @priority: GFP_* flags to use
 *
 * Allocate `count' pages from the partition. Pages are aligned to
 * a multiple of `align'. `priority' has the same meaning in kmalloc, and
 * is used for partition management information, it does not influence the
 * memory returned.
 *
 *
 *
 * This function may not be called from an interrupt.
 */
unsigned long bpa2_alloc_pages(struct bpa2_part* bp, int count, int align, int priority)
{
	struct range *range, **range_ptr, *new_range, *align_range, *used_range;
	unsigned long aligned_base=0;
	unsigned long result = 0;

	/* Allocate the data structures we might need here so that we
	 * don't have problems inside the spinlock.
	 * Free at the end if not used. */
	new_range = kmalloc(sizeof(struct range), priority);
	align_range = kmalloc(sizeof(struct range), priority);
	if ((new_range == NULL) || (align_range == NULL))
		goto fail;

	if (align == 0)
		align = PAGE_SIZE;
	else
		align = align * PAGE_SIZE;

	spin_lock(&bpa2_lock);

	/*
	 * Search a free block which is large enough, even with alignment.
	 */
	range_ptr = &bp->free_list;
	while (*range_ptr != NULL) {
		range = *range_ptr;
		aligned_base = ((range->base + align - 1) / align) * align;
		if (aligned_base + count * PAGE_SIZE <=
		    range->base + range->size)
			break;
	     range_ptr = &range->next;
	}
	if (*range_ptr == NULL)
		goto fail_unlock;
	range = *range_ptr;

	/*
	 * When we have to align, the pages needed for alignment can
	 * be put back to the free pool.
	 */
	if (aligned_base != range->base) {
		align_range->base = range->base;
		align_range->size = aligned_base - range->base;
		range->base = aligned_base;
		range->size -= align_range->size;
		align_range->next = range;
		*range_ptr = align_range;
		range_ptr = &align_range->next;
		align_range = NULL;
	}

	if (count * PAGE_SIZE < range->size) {
		/*
		 * Range is larger than needed, create a new list element for
		 * the used list and shrink the element in the free list.
		 */
		new_range->base        = range->base;
		new_range->size        = count * PAGE_SIZE;
		range->base = new_range->base + new_range->size;
		range->size = range->size - new_range->size;
		used_range = new_range;
		new_range = NULL;
	} else {
		/*
		 * Range fits perfectly, remove it from free list.
		 */
		*range_ptr = range->next;
		used_range = range;
	}
	/*
	 * Insert block into used list
	 */
	used_range->next = bp->used_list;
	bp->used_list = used_range;
	result = used_range->base;

fail_unlock:
	spin_unlock(&bpa2_lock);
fail:
	if (new_range)
		kfree(new_range);
	if (align_range)
		kfree(align_range);

	return result;
}
EXPORT_SYMBOL(bpa2_alloc_pages);

/**
 * bpa2_free_pages - free pages allocated from a bpa2 partition
 * @bp: partition to free pages back to
 * @base:
 * @align: required alinment
 * @priority: GFP_* flags to use
 *
 * Free pages allocated with `bigphysarea_alloc_pages'. `base' must be an
 * address returned by `bigphysarea_alloc_pages'.
 * This function my not be called from an interrupt!
 */
void bpa2_free_pages(struct bpa2_part* bp, unsigned long base)
{
	struct range *prev, *next, *range, **range_ptr;

	spin_lock(&bpa2_lock);

	/*
	 * Search the block in the used list.
	 */
	for (range_ptr = &bp->used_list;
	     *range_ptr != NULL;
	     range_ptr = &(*range_ptr)->next)
		if ((*range_ptr)->base == base)
			break;
	if (*range_ptr == NULL) {
		printk("%s: 0x%08x, not allocated!\n", __FUNCTION__,
		       (unsigned)base);
		spin_unlock(&bpa2_lock);
		return;
	}
	range = *range_ptr;
	/*
	 * Remove range from the used list:
	 */
	*range_ptr = (*range_ptr)->next;
	/*
	 * The free-list is sorted by address, search insertion point
	 * and insert block in free list.
	 */
	for (range_ptr = &bp->free_list, prev = NULL;
	     *range_ptr != NULL;
	     prev = *range_ptr, range_ptr = &(*range_ptr)->next)
		if ((*range_ptr)->base >= base)
			break;
	range->next  = *range_ptr;
	*range_ptr   = range;
	/*
	 * Concatenate free range with neighbors, if possible.
	 * Try for upper neighbor (next in list) first, then
	 * for lower neighbor (predecessor in list).
	 */
	next = NULL;
	if (range->next != NULL &&
	    range->base + range->size == range->next->base) {
		next = range->next;
		range->size += range->next->size;
		range->next = next->next;
	}
	if (prev != NULL &&
	    prev->base + prev->size == range->base) {
		prev->size += prev->next->size;
		prev->next = range->next;
	} else {
		range = NULL;
	}
	spin_unlock(&bpa2_lock);

	if (next && (next != &bp->initial_free_list))
		kfree(next);
	if (range && (range != &bp->initial_free_list))
		kfree(range);
}
EXPORT_SYMBOL(bpa2_free_pages);

caddr_t	bigphysarea_alloc_pages(int count, int align, int priority)
{
	unsigned long addr;

	if (! bpa2_bigphysarea_part)
		return NULL;

	addr = bpa2_alloc_pages(bpa2_bigphysarea_part, count, align, priority);
	if (addr == 0)
		return NULL;

	return phys_to_virt(addr);
}
EXPORT_SYMBOL(bigphysarea_alloc_pages);

void bigphysarea_free_pages(caddr_t mapped_addr)
{
	unsigned long addr = virt_to_phys(mapped_addr);

	bpa2_free_pages(bpa2_bigphysarea_part, addr);
}
EXPORT_SYMBOL(bigphysarea_free_pages);

caddr_t bigphysarea_alloc(int size)
{
	int pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	return bigphysarea_alloc_pages(pages, 1, GFP_KERNEL);
}
EXPORT_SYMBOL(bigphysarea_alloc);

void bigphysarea_free(caddr_t addr, int size)
{
	(void)size;
	bigphysarea_free_pages(addr);
}
EXPORT_SYMBOL(bigphysarea_free);

#ifdef CONFIG_PROC_FS

static char* get_part_info(char *p, struct bpa2_part *bp)
{
	struct range *ptr;
	int     free_count, free_total, free_max;
	int     used_count, used_total, used_max;

	free_count = 0;
	free_total = 0;
	free_max   = 0;
	for (ptr = bp->free_list; ptr != NULL; ptr = ptr->next) {
		free_count++;
		free_total += ptr->size;
		if (ptr->size > free_max)
			free_max = ptr->size;
	}

	used_count = 0;
	used_total = 0;
	used_max   = 0;
	for (ptr = bp->used_list; ptr != NULL; ptr = ptr->next) {
		used_count++;
		used_total += ptr->size;
		if (ptr->size > used_max)
			used_max = ptr->size;
	}

	p += sprintf(p, "Partition: %s, size %ld kB\n", bp->name,
		     (bp->res.end - bp->res.start + 1) / 1024);
	if (bp->aka) {
		const char** aka;
		p += sprintf(p, "AKA: ");
		for (aka=bp->aka; *aka; aka++)
			p += sprintf(p, "%s, ", *aka);
		p -= 2;
		p += sprintf(p, "\n");
	}
	p += sprintf(p, "                       free list:             used list:\n");
	p += sprintf(p, "number of blocks:      %8d               %8d\n",
		     free_count, used_count);
	p += sprintf(p, "size of largest block: %8d kB            %8d kB\n",
		     free_max / 1024, used_max / 1024);
	p += sprintf(p, "total:                 %8d kB            %8d kB\n",
		     free_total / 1024, used_total /1024);

	return  p;
}

static int get_info(char *buffer, char **addr, off_t offset, int count)
{
	struct bpa2_part* bp;
	char* p = buffer;

	spin_lock(&bpa2_lock);

	list_for_each_entry(bp, &bpa2_parts, next) {
		p = get_part_info(p, bp);
		if (bpa2_parts.prev != &bp->next) {
			*p++ = '\n';
			*p++ = '\0';
		}
	}

	spin_unlock(&bpa2_lock);

	return p-buffer;
}

/*
 * Called from late in the kernel initialisation sequence, once the
 * normal memory allocator is available.
 */
static int __init bpa2_proc_init(void)
{
	create_proc_info_entry("bpa2", 0444, &proc_root, get_info);

	return 0;
}
__initcall(bpa2_proc_init);

#endif /* CONFIG_PROC_FS */

void bpa2_memory(struct bpa2_part *part, unsigned long *base,
		 unsigned long *size)
{
	if (base)
		*base = (unsigned long)phys_to_virt(part->res.start);
	if (size)
		*size = part->res.end - part->res.start + 1;
}

void bigphysarea_memory(unsigned long *base, unsigned long *size)
{
	bpa2_memory(bpa2_bigphysarea_part, base, size);
}
EXPORT_SYMBOL(bigphysarea_memory);
