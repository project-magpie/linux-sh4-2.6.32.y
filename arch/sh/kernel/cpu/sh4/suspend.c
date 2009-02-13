/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/suspend-st40.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
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
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/kobject.h>
#include <linux/stat.h>
#include <linux/clk.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm-generic/bug.h>
#include <asm/pm.h>

#include <linux/stm/pm.h>

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)		\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

static int sh4_suspend_enter(suspend_state_t state);

static struct sh4_suspend_t pdata __cacheline_aligned =
{
	.ops.enter = sh4_suspend_enter,
};

unsigned int wokenup_by ;
static struct clk *sh4_clk;

unsigned long sh4_suspend(struct sh4_suspend_t *pdata,
	unsigned long instr_tbl, unsigned long instr_tbl_end);

static inline unsigned long _10_ms_lpj(void)
{
	return clk_get_rate(sh4_clk) / (100 * 2);
}

static int sh4_suspend_enter(suspend_state_t state)
{
	unsigned long flags;
	unsigned long instr_tbl, instr_tbl_end;

	pdata.l_p_j = _10_ms_lpj();

	/* Must wait for serial buffers to clear */
	mdelay(500);

	local_irq_save(flags);

	/* sets the right instruction table */
	if (state == PM_SUSPEND_STANDBY) {
		instr_tbl     = pdata.stby_tbl;
		instr_tbl_end = pdata.stby_size;
	} else {
		instr_tbl     = pdata.mem_tbl;
		instr_tbl_end = pdata.mem_size;
	}

	BUG_ON(in_irq());

	wokenup_by = sh4_suspend(&pdata, instr_tbl, instr_tbl_end);

/*
 *  without the evt_to_irq function the INTEVT is returned
 */
	if (pdata.evt_to_irq)
		wokenup_by = pdata.evt_to_irq(wokenup_by);

	BUG_ON(in_irq());

	local_irq_restore(flags);

	printk(KERN_INFO "sh4 woken up by: 0x%x\n", wokenup_by);

	return 0;
}

static void sleep_on_idle(void)
{
	asm volatile ("sleep	\n":::"memory");
}

static ssize_t power_wokenupby_show(struct kset *subsys, char *buf)
{
	return sprintf(buf, "%d\n", wokenup_by);
}

static struct subsys_attribute wokenup_by_attr =
__ATTR(wokenup-by, S_IRUGO, power_wokenupby_show, NULL);

static int __init suspend_init(void)
{
	int dummy;
	sh4_clk = clk_get(NULL, "sh4_clk");
	if (!sh4_clk) {
		printk(KERN_ERR "ERROR: on clk_get(sh4_clk)\n");
		return -1;
	}

/*	the idle loop calls the sleep instruction
 *	but platform specific code (in the suspend_platform_setup
 *	implementation) could set a different 'on idle' action
 */
	pm_idle = sleep_on_idle;

	suspend_platform_setup(&pdata);

	pm_set_ops(&pdata.ops);

	dummy = subsys_create_file(&power_subsys, &wokenup_by_attr);

	printk(KERN_INFO "sh4 suspend support registered\n");

	return 0;
}

late_initcall(suspend_init);
