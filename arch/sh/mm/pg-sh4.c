/*
 * arch/sh/mm/pg-sh4.c
 *
 * Copyright (C) 1999, 2000, 2002  Niibe Yutaka
 * Copyright (C) 2002 - 2007  Paul Mundt
 *
 * Released under the terms of the GNU GPL v2.0.
 */
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>

#define CACHE_ALIAS (current_cpu_data.dcache.alias_mask)

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

	return (void *)vaddr;
}

static inline void kunmap_coherent(struct page *page)
{
	dec_preempt_count();
	preempt_check_resched();
}

/*
 * clear_user_page
 * @to: P1 address
 * @address: U0 address to be mapped
 * @page: page (virt_to_page(to))
 */
void clear_user_page(void *to, unsigned long address, struct page *page)
{
	void __clear_page_wb(void *to);

	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0)
		__clear_page_wb(to);
	else {
		void *vto = kmap_coherent(page, address);
		__clear_user_page(vto, to);
		kunmap_coherent(vto);
	}
}

/*
 * copy_user_page
 * @to: P1 address
 * @from: P1 address
 * @address: U0 address to be mapped
 * @page: page (virt_to_page(to))
 */
void copy_user_page(void *to, void *from, unsigned long address,
		    struct page *page)
{
	extern void __copy_page_wb(void *to, void *from);

	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0)
		__copy_page_wb(to, from);
	else {
		void *vfrom = kmap_coherent(page, address);
		__copy_user_page(vfrom, from, to);
		kunmap_coherent(vfrom);
	}
}
