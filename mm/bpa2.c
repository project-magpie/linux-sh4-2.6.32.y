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
 * 17th Jan 2007 : STMicroelectronics Ltd. <carl.shaw@st.com>
 * 	Added kernel bpa2 command line parameter support:
 * 	bpa2parts=<partdef>[,<partdef>]
 * 	 <partdef> := <name>:<size>:[<base physical address>]:[flags]
 * 	 <name>    := name (<= 20 bytes length)
 * 	 <size>    := standard linux memory size (e.g. 4M)
 *       <flags>   := currently unused
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
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pfn.h>
#include <linux/bpa2.h>

#define MAX_NAME_LEN 20

struct range {
	struct range *next;
	unsigned long base;			/* base of allocated block */
	unsigned long size;			/* size in bytes */
#if defined(CONFIG_BPA2_ALLOC_TRACE)
	const char *trace_file;
	int trace_line;
#endif
};

struct bpa2_part {
	char res_name[MAX_NAME_LEN];
	struct resource res;
	const char* name;
	const char** aka;
	struct range *free_list;
	struct range *used_list;
	struct range initial_free_list;
	int flags;
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
		bp->flags = partdescs->flags;

		if (partdescs->start == 0) {
			ok = bpa2_alloc_low(bp);
		} else if ((start_pfn >= min_low_pfn) && (end_pfn <= max_low_pfn)) {
			ok = bpa2_init_low(bp);
		} else if ((start_pfn > max_low_pfn) || (end_pfn < min_low_pfn)) {
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

		printk(KERN_INFO "%s @ 0x%08x size 0x%08x\n",
			bp->res.name,
			bp->res.start,
			(bp->res.end - bp->res.start) );

		free_list->next = NULL;
		free_list->base = bp->res.start;
		free_list->size = (bp->res.end + 1) - bp->res.start;
		bp->free_list = free_list;

		list_add_tail(&bp->next, &bpa2_parts);

		bp++;
		partdescs++;
	}

	if ((bpa2_bigphysarea_part == NULL) &&
	    ((bp = bpa2_find_part("bigphysarea")) != NULL)) {
		if (bp->low_mem) {
			bpa2_bigphysarea_part = bp;
		} else {
			/* Should rate limit this I suppose */
			printk(KERN_ERR "bpa2: bigphysarea not in logical memory\n");
		}
	}
}

static int __init bpa2_bigphys_setup(char *str)
{
	int par;
	struct bpa2_partition_desc partdesc = {
		.name   = "bigphysarea",
		.start  = 0,
		.size   = 0,
		.flags  = BPA2_NORMAL,
		.aka    = NULL,
	};

	if (get_option(&str,&par) == 0)
                return -EINVAL;

	partdesc.size = par << PAGE_SHIFT;
	bpa2_init(&partdesc, 1);

	return 1;
}
__setup("bigphysarea=", bpa2_bigphys_setup);

/*
 * Check for the new bpa2parts parameter
 */
static int __init bpa2_parts_setup(char *str)
{
	char *opt;
	struct bpa2_partition_desc partdesc;
	char *name;

	if (!str || !*str)
		return -EINVAL;

	while ((opt = strsep(&str, ",")) != NULL){
		char *p;

		memset(&partdesc, 0, sizeof(partdesc));

		/* Allocate memory for partition name, but we can't use kmalloc yet */
		name = alloc_bootmem(MAX_NAME_LEN);
		memset(name, 0, MAX_NAME_LEN);
		partdesc.name = name;

		/* Get name */
		if ((p = strsep(&opt, ":")) == NULL)
			goto invalid;

		if (strlcpy(name, p, MAX_NAME_LEN) == 0){
			printk(KERN_ERR "Invalid bpa2 partition name\n");
			return -EINVAL;
		}

		/* Get size */
		if ((p = strsep(&opt, ":")) == NULL)
			goto invalid;

		partdesc.size = memparse(p,&p);

		if (partdesc.size < PAGE_SIZE){
			printk(KERN_ERR "Invalid bpa2 partition size\n");
                	return -EINVAL;
		}

		/* round size up to whole number of pages */
		partdesc.size = ((partdesc.size+(PAGE_SIZE-1)) >> PAGE_SHIFT) << PAGE_SHIFT;

		/* Get start address (optional) */
		if ((p = strsep(&opt, ":")) == NULL)
			goto invalid;

		if (strlen(p) > 0){
			if ((partdesc.start = memparse(p, &p)) == 0){
				printk(KERN_ERR "Invalid bpa2 base address\n");
                		return -EINVAL;
			}
		}

		/* Get flags (optional) */
		partdesc.flags = BPA2_NORMAL;

		/* Add it to the list... */
		bpa2_init(&partdesc, 1);
	}

	return 1;

invalid:
	printk(KERN_ERR "Invalid bpa2 partition definition\n");
	return -EINVAL;
}

__setup("bpa2parts=", bpa2_parts_setup);

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
unsigned long __bpa2_alloc_pages(struct bpa2_part *bp, int count, int align,
		int priority, const char *trace_file, int trace_line)
{
	struct range *range, **range_ptr, *new_range, *align_range, *used_range;
	unsigned long aligned_base=0;
	unsigned long result = 0;

	if (count == 0)
		return 0;

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
#if defined(CONFIG_BPA2_ALLOC_TRACE)
	/* Save the caller data */
	used_range->trace_file = trace_file;
	used_range->trace_line = trace_line;
#endif
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
EXPORT_SYMBOL(__bpa2_alloc_pages);

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



caddr_t	__bigphysarea_alloc_pages(int count, int align, int priority,
		const char *trace_file, int trace_line)
{
	unsigned long addr;

	if (! bpa2_bigphysarea_part)
		return NULL;

	addr = __bpa2_alloc_pages(bpa2_bigphysarea_part, count,
			align, priority, trace_file, trace_line);

	if (addr == 0)
		return NULL;

	return phys_to_virt(addr);
}
EXPORT_SYMBOL(__bigphysarea_alloc_pages);

void bigphysarea_free_pages(caddr_t mapped_addr)
{
	unsigned long addr = virt_to_phys(mapped_addr);

	bpa2_free_pages(bpa2_bigphysarea_part, addr);
}
EXPORT_SYMBOL(bigphysarea_free_pages);



#ifdef CONFIG_PROC_FS

static void *bpa2_seq_start(struct seq_file *s, loff_t *pos)
{
	struct list_head *node;
	loff_t i;

	spin_lock(&bpa2_lock);

	for (i = 0, node = bpa2_parts.next;
			i < *pos && node != &bpa2_parts;
			i++, node = node->next)
		;

	if (node == &bpa2_parts)
		return NULL;

	return node;
}

static void bpa2_seq_stop(struct seq_file *s, void *v)
{
	spin_unlock(&bpa2_lock);
}

static void *bpa2_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *node = v;

	(*pos)++;
	node = node->next;

	if (node == &bpa2_parts)
		return NULL;

	seq_printf(s, "\n");

	return node;
}

static int bpa2_seq_show(struct seq_file *s, void *v)
{
	struct bpa2_part *part = list_entry(v, struct bpa2_part, next);
	struct range *range;
	int free_count, free_total, free_max;
	int used_count, used_total, used_max;
	const char **aka;

	free_count = 0;
	free_total = 0;
	free_max = 0;
	for (range = part->free_list; range != NULL; range = range->next) {
		free_count++;
		free_total += range->size;
		if (range->size > free_max)
			free_max = range->size;
	}

	used_count = 0;
	used_total = 0;
	used_max = 0;
	for (range = part->used_list; range != NULL; range = range->next) {
		used_count++;
		used_total += range->size;
		if (range->size > used_max)
			used_max = range->size;
	}

	seq_printf(s, "Partition: '%s'", part->name);
	for (aka = part->aka; *aka; aka++)
		seq_printf(s, " aka '%s'", *aka);
	seq_printf(s, "\n");
	seq_printf(s, "Size: %d kB, base address: 0x%08x\n",
			(part->res.end - part->res.start + 1) / 1024,
			part->res.start);
	seq_printf(s, "Statistics:                  free       "
			"    used\n");
	seq_printf(s, "- number of blocks:      %8d       %8d\n",
			free_count, used_count);
	seq_printf(s, "- size of largest block: %8d kB    %8d kB\n",
			free_max / 1024, used_max / 1024);
	seq_printf(s, "- total:                 %8d kB    %8d kB\n",
			free_total / 1024, used_total / 1024);

	if (used_count) {
		seq_printf(s, "Allocations:\n");
		for (range = part->used_list; range != NULL;
				range = range->next) {
			seq_printf(s, "- %lu B at 0x%.8lx",
					range->size, range->base);
#if defined(CONFIG_BPA2_ALLOC_TRACE)
			if (range->trace_file)
				seq_printf(s, " (%s:%d)", range->trace_file,
						range->trace_line);
#endif
			seq_printf(s, "\n");
		}
	}

	return 0;
}

static struct seq_operations bpa2_seq_ops = {
	.start = bpa2_seq_start,
	.next = bpa2_seq_next,
	.stop = bpa2_seq_stop,
	.show = bpa2_seq_show,
};

static int bpa2_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &bpa2_seq_ops);
}

static struct file_operations bpa2_proc_ops = {
	.owner = THIS_MODULE,
	.open = bpa2_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* Called from late in the kernel initialisation sequence, once the
 * normal memory allocator is available. */
static int __init bpa2_proc_init(void)
{
	struct proc_dir_entry *entry = create_proc_entry("bpa2", 0, NULL);

	if (entry)
		entry->proc_fops = &bpa2_proc_ops;

	return 0;
}
__initcall(bpa2_proc_init);

#endif /* CONFIG_PROC_FS */

void bpa2_memory(struct bpa2_part *part, unsigned long *base,
		 unsigned long *size)
{
	if (base)
		*base = part?
			(unsigned long)phys_to_virt(part->res.start)
			: 0;
	if (size)
		*size = part?
			part->res.end - part->res.start + 1
			: 0;
}

void bigphysarea_memory(unsigned long *base, unsigned long *size)
{
	bpa2_memory(bpa2_bigphysarea_part, base, size);
}
EXPORT_SYMBOL(bigphysarea_memory);
