/*
 * arch/sh/mm/consistent.c
 *
 * Copyright (C) 2004  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/addrspace.h>
#include <asm/io.h>


void *consistent_alloc(gfp_t gfp, size_t size, dma_addr_t *handle)
{
	struct page *page, *end;
	void *ret;
	int order;
	unsigned long phys_addr;
	void* kernel_addr;
#ifdef CONFIG_32BIT
	struct vm_struct * area;
#endif

	/* ignore region specifiers */
        gfp &= ~(__GFP_DMA | __GFP_HIGHMEM);

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;

	kernel_addr = page_address(page);
	phys_addr = virt_to_phys(kernel_addr);

#ifdef CONFIG_32BIT
	area = get_vm_area_node(size, VM_IOREMAP, -1, gfp);
	if (!area) {
		free_pages(gfp, order);
		return NULL;
	}

	ret = area->addr;
	if (ioremap_page_range(ret, ret+size, phys_addr, PAGE_KERNEL_NOCACHE)) {
		free_pages(gfp, order);
		remove_vm_area(ret);
		return NULL;
	}

	area->phys_addr = phys_addr;
#else
	ret = P2SEGADDR(kernel_addr);
#endif

	memset(kernel_addr, 0, size);

	/*
	 * We must flush the cache before we pass it on to the device
	 */
	dma_cache_wback_inv(kernel_addr, size);

	/* Free the otherwise unused pages */
	split_page(page, order);
	end = page + (1 << order);
	for (page += size >> PAGE_SHIFT; page < end; page++) {
		__free_page(page);
	}

	*handle = phys_addr;
	return ret;
}

void consistent_free(void *vaddr, size_t size)
{
	unsigned long addr;
	struct page *page;
	int num_pages=(size+PAGE_SIZE-1) >> PAGE_SHIFT;
	int i;

#ifdef CONFIG_32BIT
	struct vm_struct * area;

	read_lock(&vmlist_lock);
	for (area = vmlist; area; area = area->next) {
		if (area->addr == vaddr)
			break;
	}
        read_unlock(&vmlist_lock);

	if (!area) {
		printk("%s: bad address %p\n", __FUNCTION__, vaddr);
                dump_stack();
                return;
        }

	addr = phys_to_virt(area->phys_addr);
#else
	addr = P1SEGADDR((unsigned long)vaddr);
#endif

	BUG_ON(!virt_addr_valid(addr));
	page = virt_to_page(addr);

	for(i=0;i<num_pages;i++) {
		__free_page((page+i));
	}
#ifdef CONFIG_32BIT
	remove_vm_area(vaddr);
#endif
}

void consistent_sync(void *vaddr, size_t size, int direction)
{
	switch (direction) {
	case DMA_FROM_DEVICE:		/* invalidate only */
		dma_cache_inv(vaddr, size);
		break;
	case DMA_TO_DEVICE:		/* writeback only */
		dma_cache_wback(vaddr, size);
		break;
	case DMA_BIDIRECTIONAL:		/* writeback and invalidate */
		dma_cache_wback_inv(vaddr, size);
		break;
	default:
		BUG();
	}
}

EXPORT_SYMBOL(consistent_alloc);
EXPORT_SYMBOL(consistent_free);
EXPORT_SYMBOL(consistent_sync);

