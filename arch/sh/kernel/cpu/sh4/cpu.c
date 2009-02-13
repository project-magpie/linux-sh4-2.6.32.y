/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/cpu.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <asm/cacheflush.h>

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)			\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

/*
 * saved registers:
 * - 8 gpr
 * -   pr
 * -   sr
 * -   r6_bank
 * -   r7_bank
 */
unsigned long saved_context_reg[12] __cacheline_aligned;

void (*arch_swsusp_processor_state)(int suspend) = NULL;

void save_processor_state(void)
{
	if (arch_swsusp_processor_state)
		arch_swsusp_processor_state(PM_EVENT_FREEZE);

	pmb_pm_state(PM_EVENT_FREEZE);
	return;
}

void restore_processor_state(void)
{
	int i;
	unsigned long flags;
	struct irq_desc *desc;
	void (*irq_func)(unsigned int irq);

	/* restore the (hw) pmb setting */
	pmb_pm_state(PM_EVENT_ON);

	if (arch_swsusp_processor_state)
		arch_swsusp_processor_state(PM_EVENT_ON);

	/* now restore the hw irq setting */
	local_irq_save(flags);
	for (i = 0; i < NR_IRQS; ++i) {
		desc = &irq_desc[i];
		if (desc->chip != &no_irq_chip && desc->action) {
			irq_func = (desc->status & IRQ_DISABLED) ?
			    desc->chip->disable : desc->chip->enable;
			spin_lock(&desc->lock);
			desc->chip->startup(i);
			irq_func(i);
			spin_unlock(&desc->lock);
		}	/* if.. */
	}		/* for... */
	local_irq_restore(flags);
	return;
}

/* References to section boundaries */
int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = __pa(&__nosave_begin) >> PAGE_SHIFT;
	unsigned long nosave_end_pfn =
	    PAGE_ALIGN(__pa(&__nosave_end)) >> PAGE_SHIFT;
	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);

}
