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
#include <asm/cpu/cacheflush.h>
#include <asm/io.h>
#include <asm-generic/bug.h>
#include <asm/pm.h>

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)		\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

extern struct kset power_subsys;

unsigned int wokenup_by ;

unsigned long sh4_suspend(struct sh4_suspend_t *pdata,
	unsigned long instr_tbl, unsigned long instr_tbl_end);

static inline unsigned long _10_ms_lpj(void)
{
	static struct clk *sh4_clk;
	if (!sh4_clk)
		sh4_clk = clk_get(NULL, "sh4_clk");
	return clk_get_rate(sh4_clk) / (100 * 2);
}

static struct sh4_suspend_t *data;
static int sh4_suspend_enter(suspend_state_t state)
{
	unsigned long flags;
	unsigned long instr_tbl, instr_tbl_end;

	data->l_p_j = _10_ms_lpj();

	/* Must wait for serial buffers to clear */
	mdelay(500);

	local_irq_save(flags);

	/* sets the right instruction table */
	if (state == PM_SUSPEND_STANDBY) {
		instr_tbl     = data->stby_tbl;
		instr_tbl_end = data->stby_size;
	} else {
		instr_tbl     = data->mem_tbl;
		instr_tbl_end = data->mem_size;
	}

	BUG_ON(in_irq());

	wokenup_by = sh4_suspend(data, instr_tbl, instr_tbl_end);

/*
 *  without the evt_to_irq function the INTEVT is returned
 */
	if (data->evt_to_irq)
		wokenup_by = data->evt_to_irq(wokenup_by);

	BUG_ON(in_irq());

	local_irq_restore(flags);

	printk(KERN_INFO "sh4 woken up by: 0x%x\n", wokenup_by);

	return 0;
}

static ssize_t power_wokenupby_show(struct kset *subsys, char *buf)
{
	return sprintf(buf, "%d\n", wokenup_by);
}

static struct subsys_attribute wokenup_by_attr =
__ATTR(wokenup-by, S_IRUGO, power_wokenupby_show, NULL);

static int sh4_suspend_valid_both(suspend_state_t state)
{
	return 1;
}

int __init sh4_suspend_register(struct sh4_suspend_t *pdata)
{
	int dummy;

	if (!pdata)
		return -EINVAL;
	data = pdata;
	data->ops.enter = sh4_suspend_enter;
	if (data->stby_tbl && data->stby_size)
		data->ops.valid = sh4_suspend_valid_both;
	else
		data->ops.valid = pm_valid_only_mem;

	pm_set_ops(&data->ops);

	dummy = subsys_create_file(&power_subsys, &wokenup_by_attr);

	printk(KERN_INFO "sh4 suspend support registered\n");

	return 0;
}
