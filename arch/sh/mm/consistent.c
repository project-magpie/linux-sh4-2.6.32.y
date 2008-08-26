/*
 * arch/sh/mm/consistent.c
 *
 * Copyright (C) 2004 - 2007  Paul Mundt
 *
 * Declared coherent memory functions based on arch/x86/kernel/pci-dma_32.c
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

#ifdef CONFIG_32BIT

/*
 * This is yet another copy of the ARM (and powerpc) VM region allocation
 * code (which is Copyright (C) 2000-2004 Russell King).
 *
 * We have to do this (rather than use get_vm_area()) because
 * dma_alloc_coherent() can be (and is) called from interrupt level.
 */

static DEFINE_SPINLOCK(consistent_lock);

/*
 * VM region handling support.
 *
 * This should become something generic, handling VM region allocations for
 * vmalloc and similar (ioremap, module space, etc).
 *
 * I envisage vmalloc()'s supporting vm_struct becoming:
 *
 *  struct vm_struct {
 *    struct vm_region	region;
 *    unsigned long	flags;
 *    struct page	**pages;
 *    unsigned int	nr_pages;
 *    unsigned long	phys_addr;
 *  };
 *
 * get_vm_area() would then call vm_region_alloc with an appropriate
 * struct vm_region head (eg):
 *
 *  struct vm_region vmalloc_head = {
 *	.vm_list	= LIST_HEAD_INIT(vmalloc_head.vm_list),
 *	.vm_start	= VMALLOC_START,
 *	.vm_end		= VMALLOC_END,
 *  };
 *
 * However, vmalloc_head.vm_start is variable (typically, it is dependent on
 * the amount of RAM found at boot time.)  I would imagine that get_vm_area()
 * would have to initialise this each time prior to calling vm_region_alloc().
 */
struct vm_region {
	struct list_head	vm_list;
	unsigned long		vm_start;
	unsigned long		vm_end;
	struct page		*vm_pages;
};

static struct vm_region consistent_head = {
	.vm_list	= LIST_HEAD_INIT(consistent_head.vm_list),
	.vm_start	= CONSISTENT_BASE,
	.vm_end		= CONSISTENT_END,
};

static struct vm_region *
vm_region_alloc(struct vm_region *head, size_t size, gfp_t gfp)
{
	unsigned long addr = head->vm_start, end = head->vm_end - size;
	unsigned long flags;
	struct vm_region *c, *new;

	new = kmalloc(sizeof(struct vm_region), gfp);
	if (!new)
		goto out;

	spin_lock_irqsave(&consistent_lock, flags);

	list_for_each_entry(c, &head->vm_list, vm_list) {
		if ((addr + size) < addr)
			goto nospc;
		if ((addr + size) <= c->vm_start)
			goto found;
		addr = c->vm_end;
		if (addr > end)
			goto nospc;
	}

found:
	/*
	 * Insert this entry _before_ the one we found.
	 */
	list_add_tail(&new->vm_list, &c->vm_list);
	new->vm_start = addr;
	new->vm_end = addr + size;

	spin_unlock_irqrestore(&consistent_lock, flags);
	return new;

nospc:
	spin_unlock_irqrestore(&consistent_lock, flags);
	kfree(new);
out:
	return NULL;
}

static struct vm_region *vm_region_find(struct vm_region *head,
					 unsigned long addr)
{
	struct vm_region *c;

	list_for_each_entry(c, &head->vm_list, vm_list) {
		if (c->vm_start == addr)
			goto out;
	}
	c = NULL;
out:
	return c;
}

static void *__consistent_map(struct page *page, size_t size, gfp_t gfp)
{
	struct vm_region *c;
	unsigned long vaddr;
	unsigned long paddr;

	c = vm_region_alloc(&consistent_head, size,
			    gfp & ~(__GFP_DMA | __GFP_HIGHMEM));
	if (!c)
		return NULL;

	vaddr = c->vm_start;
	paddr = page_to_phys(page);
	if (ioremap_page_range(vaddr, vaddr+size, paddr, PAGE_KERNEL_NOCACHE)) {
		list_del(&c->vm_list);
		return NULL;
	}

	c->vm_pages = page;

	return (void *)vaddr;
}

static struct page *__consistent_unmap(void *vaddr, size_t size)
{
	unsigned long flags;
	struct vm_region *c;
	struct page *page;

	spin_lock_irqsave(&consistent_lock, flags);
	c = vm_region_find(&consistent_head, (unsigned long)vaddr);
	spin_unlock_irqrestore(&consistent_lock, flags);
	if (!c)
		goto no_area;

	if ((c->vm_end - c->vm_start) != size) {
		printk(KERN_ERR "%s: freeing wrong coherent size (%ld != %d)\n",
		       __func__, c->vm_end - c->vm_start, size);
		dump_stack();
		size = c->vm_end - c->vm_start;
	}

	page = c->vm_pages;

	unmap_kernel_range(c->vm_start, size);

	spin_lock_irqsave(&consistent_lock, flags);
	list_del(&c->vm_list);
	spin_unlock_irqrestore(&consistent_lock, flags);

	kfree(c);

	return page;

no_area:
	printk(KERN_ERR "%s: trying to free invalid coherent area: %p\n",
	       __func__, vaddr);
	dump_stack();

	return NULL;
}

#else

static void *__consistent_map(struct page *page, size_t size, gfp_t gfp)
{
	return P2SEGADDR(page_address(page));
}

static struct page *__consistent_unmap(void *vaddr, size_t size)
{
	unsigned long addr;

	addr = P1SEGADDR((unsigned long)vaddr);
	BUG_ON(!virt_addr_valid(addr));
	return virt_to_page(addr);
}

#endif

struct dma_coherent_mem {
	void		*virt_base;
	u32		device_base;
	int		size;
	int		flags;
	unsigned long	*bitmap;
};

void *dma_alloc_coherent(struct device *dev, size_t size,
			   dma_addr_t *dma_handle, gfp_t gfp)
{
	void *ret;
	struct dma_coherent_mem *mem = dev ? dev->dma_mem : NULL;
	int order = get_order(size);
	struct page *page, *end;
	unsigned long phys_addr;
	void* kernel_addr;

	if (mem) {
		int page = bitmap_find_free_region(mem->bitmap, mem->size,
						     order);
		if (page >= 0) {
			*dma_handle = mem->device_base + (page << PAGE_SHIFT);
			ret = mem->virt_base + (page << PAGE_SHIFT);
			memset(ret, 0, size);
			return ret;
		}
		if (mem->flags & DMA_MEMORY_EXCLUSIVE)
			return NULL;
	}

	/* ignore region specifiers */
	gfp &= ~(__GFP_DMA | __GFP_HIGHMEM);

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;

	kernel_addr = page_address(page);
	phys_addr = virt_to_phys(kernel_addr);
	ret = __consistent_map(page, size, gfp);
	if (!ret) {
		__free_pages(page, order);
		return NULL;
	}

	memset(kernel_addr, 0, size);
	/*
	 * Pages from the page allocator may have data present in
	 * cache. So flush the cache before using uncached memory.
	 */
	dma_cache_sync(dev, kernel_addr, size, DMA_BIDIRECTIONAL);

	/* Free the otherwise unused pages */
	split_page(page, order);
	end = page + (1 << order);
	for (page += size >> PAGE_SHIFT; page < end; page++) {
		__free_page(page);
	}

	*dma_handle = phys_addr;
	return ret;
}
EXPORT_SYMBOL(dma_alloc_coherent);

void dma_free_coherent(struct device *dev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	struct dma_coherent_mem *mem = dev ? dev->dma_mem : NULL;
	int order = get_order(size);
	struct page *page;
	int i;

	if (mem && vaddr >= mem->virt_base && vaddr < (mem->virt_base + (mem->size << PAGE_SHIFT))) {
		int page = (vaddr - mem->virt_base) >> PAGE_SHIFT;

		bitmap_release_region(mem->bitmap, page, order);
		return;
	}

	BUG_ON(mem && mem->flags & DMA_MEMORY_EXCLUSIVE);

	size = PAGE_ALIGN(size);
	page = __consistent_unmap(vaddr, size);
	if (page)
		for (i = 0; i < (size>>PAGE_SHIFT); i++)
			__free_page(page+i);
}
EXPORT_SYMBOL(dma_free_coherent);

int dma_declare_coherent_memory(struct device *dev, dma_addr_t bus_addr,
				dma_addr_t device_addr, size_t size, int flags)
{
	void __iomem *mem_base = NULL;
	int pages = size >> PAGE_SHIFT;
	int bitmap_size = BITS_TO_LONGS(pages) * sizeof(long);

	if ((flags & (DMA_MEMORY_MAP | DMA_MEMORY_IO)) == 0)
		goto out;
	if (!size)
		goto out;
	if (dev->dma_mem)
		goto out;

	/* FIXME: this routine just ignores DMA_MEMORY_INCLUDES_CHILDREN */

	mem_base = ioremap_nocache(bus_addr, size);
	if (!mem_base)
		goto out;

	dev->dma_mem = kmalloc(sizeof(struct dma_coherent_mem), GFP_KERNEL);
	if (!dev->dma_mem)
		goto out;
	dev->dma_mem->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!dev->dma_mem->bitmap)
		goto free1_out;

	dev->dma_mem->virt_base = mem_base;
	dev->dma_mem->device_base = device_addr;
	dev->dma_mem->size = pages;
	dev->dma_mem->flags = flags;

	if (flags & DMA_MEMORY_MAP)
		return DMA_MEMORY_MAP;

	return DMA_MEMORY_IO;

 free1_out:
	kfree(dev->dma_mem);
 out:
	if (mem_base)
		iounmap(mem_base);
	return 0;
}
EXPORT_SYMBOL(dma_declare_coherent_memory);

void dma_release_declared_memory(struct device *dev)
{
	struct dma_coherent_mem *mem = dev->dma_mem;

	if (!mem)
		return;
	dev->dma_mem = NULL;
	iounmap(mem->virt_base);
	kfree(mem->bitmap);
	kfree(mem);
}
EXPORT_SYMBOL(dma_release_declared_memory);

void *dma_mark_declared_memory_occupied(struct device *dev,
					dma_addr_t device_addr, size_t size)
{
	struct dma_coherent_mem *mem = dev->dma_mem;
	int pages = (size + (device_addr & ~PAGE_MASK) + PAGE_SIZE - 1) >> PAGE_SHIFT;
	int pos, err;

	if (!mem)
		return ERR_PTR(-EINVAL);

	pos = (device_addr - mem->device_base) >> PAGE_SHIFT;
	err = bitmap_allocate_region(mem->bitmap, pos, get_order(pages));
	if (err != 0)
		return ERR_PTR(err);
	return mem->virt_base + (pos << PAGE_SHIFT);
}
EXPORT_SYMBOL(dma_mark_declared_memory_occupied);

void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
		    enum dma_data_direction direction)
{
	switch (direction) {
	case DMA_FROM_DEVICE:		/* invalidate only */
		__flush_invalidate_region(vaddr, size);
		break;
	case DMA_TO_DEVICE:		/* writeback only */
		__flush_wback_region(vaddr, size);
		break;
	case DMA_BIDIRECTIONAL:		/* writeback and invalidate */
		__flush_purge_region(vaddr, size);
		break;
	default:
		BUG();
	}
}
EXPORT_SYMBOL(dma_cache_sync);
