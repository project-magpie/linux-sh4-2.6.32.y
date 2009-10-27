/*
 * arch/sh/mm/pg-sh4.c
 *
 * Copyright (C) 1999, 2000, 2002  Niibe Yutaka
 * Copyright (C) 2002 - 2007  Paul Mundt
 *
 * Released under the terms of the GNU GPL v2.0.
 */
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>

#define CACHE_ALIAS (current_cpu_data.dcache.alias_mask)

#define kmap_get_fixmap_pte(vaddr)                                     \
	pte_offset_kernel(pmd_offset(pud_offset(pgd_offset_k(vaddr), (vaddr)), (vaddr)), (vaddr))

static pte_t *kmap_coherent_pte;

void __init kmap_coherent_init(void)
{
	unsigned long vaddr;

	/* cache the first coherent kmap pte */
	vaddr = __fix_to_virt(FIX_CMAP_BEGIN);
	kmap_coherent_pte = kmap_get_fixmap_pte(vaddr);
}

static inline void *kmap_coherent(struct page *page, unsigned long addr)
{
	enum fixed_addresses idx;
	unsigned long vaddr, flags;
	pte_t pte;

	inc_preempt_count();

	idx = (addr & current_cpu_data.dcache.alias_mask) >> PAGE_SHIFT;
	vaddr = __fix_to_virt(FIX_CMAP_END - idx);
	pte = mk_pte(page, PAGE_KERNEL);

	local_irq_save(flags);
	flush_tlb_one(get_asid(), vaddr);
	local_irq_restore(flags);

	update_mmu_cache(NULL, vaddr, pte);

	set_pte(kmap_coherent_pte - (FIX_CMAP_END - idx), pte);

	return (void *)vaddr;
}

static inline void kunmap_coherent(struct page *page)
{
	dec_preempt_count();
	preempt_check_resched();
}

/*
 * clear_user_page
 * @to: address of page in kernel space (possibly from kmap)
 * @address: user space address
 * @page: struct page
 */
void clear_user_page(void *to, unsigned long address, struct page *page)
{
	void *vto;

	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0) {
                clear_page(to);
		return;
	}

	/* Kernel alias may have modified data in the cache. */
	__flush_invalidate_region(page_address(page), PAGE_SIZE);

	vto = kmap_coherent(page, address);
	clear_page(vto);
	kunmap_coherent(vto);
}

/*
 * copy_to_user_page
 * @vma: vm_area_struct holding the pages
 * @page: struct page
 * @vaddr: user space address
 * @dst: address of page in kernel space (possibly from kmap)
 * @src: source address in kernel logical memory
 * @len: length of data in bytes (may be less than PAGE_SIZE)
 *
 * Copy data into the address space of a process other than the current
 * process (eg for ptrace).
 */
void copy_to_user_page(struct vm_area_struct *vma, struct page *page,
		       unsigned long vaddr, void *dst, const void *src,
		       unsigned long len)
{
	void *vto;

	vto = kmap_coherent(page, vaddr) + (vaddr & ~PAGE_MASK);
	memcpy(vto, src, len);
	kunmap_coherent(vto);

	if (vma->vm_flags & VM_EXEC)
		flush_cache_page(vma, vaddr, page_to_pfn(page));
}

void copy_from_user_page(struct vm_area_struct *vma, struct page *page,
			 unsigned long vaddr, void *dst, const void *src,
			 unsigned long len)
{
	void *vfrom;

	vfrom = kmap_coherent(page, vaddr) + (vaddr & ~PAGE_MASK);
	memcpy(dst, vfrom, len);
	kunmap_coherent(vfrom);
}

/*
 * copy_user_highpage
 * @to: destination page
 * @from: source page
 * @vaddr: address of pages in user address space
 * @vma: vm_area_struct holding the pages
 *
 * This is used in COW implementation to copy data from page @from to
 * page @to. @from was previousl mapped at @vaddr, and @to will be.
 * As this is used only in the COW implementation, this means that the
 * source is unmodified, and so we don't have to worry about cache
 * aliasing on that side.
 */
#ifdef CONFIG_HIGHMEM
/*
 * If we ever have a real highmem system, this code will need fixing
 * (as will clear_user/clear_user_highmem), because the kmap potentitally
 * creates another alias risk.
 */
#error This code is broken with real HIGHMEM
#endif
void copy_user_highpage(struct page *to, struct page *from,
			unsigned long vaddr, struct vm_area_struct *vma)
{
	void *vfrom, *vto;

	vfrom = page_address(from);
	vto = page_address(to);

	if (((vaddr ^ (unsigned long)vto) & CACHE_ALIAS) == 0) {
                copy_page(vto, vfrom);
		return;
	}

	/* Kernel alias may have modified data in the cache. */
	__flush_invalidate_region(page_address(to), PAGE_SIZE);

	vto = kmap_coherent(to, vaddr);
	copy_page(vto, vfrom);
	kunmap_coherent(vto);

	/* Make sure this page is cleared on other CPU's too before using it */
	smp_wmb();
}
EXPORT_SYMBOL(copy_user_highpage);
