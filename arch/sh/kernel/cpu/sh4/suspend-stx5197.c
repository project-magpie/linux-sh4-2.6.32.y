/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx5197.c
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
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/stm/pm.h>
#include <linux/stm/sysconf.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pm.h>
#include <asm/irq.h>
#include <asm/irq-ilc.h>

#include "./soc-stx5197.h"

#define _SYS_MON_J                      (0)
#define _SYS_MON_J_MASK                 (1)
#define _SYS_CFG_H                      (2)
#define _SYS_CFG_H_MASK                 (3)

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx5197_standby_table[] __cacheline_aligned = {
CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */

CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_X1),

/* 1. Move all the clock on OSC */
CLK_OR_LONG(CLK_REDUCED_PM_CTRL, CLK_REDUCED_ON_XTAL_STDBY),
CLK_OR_LONG(CLK_PLL_CONFIG1(0), CLK_PLL_CONFIG1_POFF),
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */
/* END. */
_END(),

CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_X1),
CLK_AND_LONG(CLK_REDUCED_PM_CTRL, ~CLK_REDUCED_ON_XTAL_STDBY),
CLK_AND_LONG(CLK_PLL_CONFIG1(0), ~CLK_PLL_CONFIG1_POFF),
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */
_DELAY(),
_DELAY(),
_DELAY(),
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx5197_mem_table[] __cacheline_aligned = {
DATA_OR_LONG(_SYS_CFG_H, _SYS_CFG_H_MASK),
DATA_WHILE_NEQ(_SYS_MON_J, _SYS_MON_J_MASK, _SYS_MON_J_MASK),

CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */

CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_X1),
/* on exetrnal Xtal */
CLK_OR_LONG(CLK_REDUCED_PM_CTRL, CLK_REDUCED_ON_XTAL_MEMSTDBY),
CLK_OR_LONG(CLK_PLL_CONFIG1(0), CLK_PLL_CONFIG1_POFF),
CLK_OR_LONG(CLK_PLL_CONFIG1(1), CLK_PLL_CONFIG1_POFF),
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */

_END(),

CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_X1),
CLK_AND_LONG(CLK_PLL_CONFIG1(0), ~CLK_PLL_CONFIG1_POFF),
CLK_AND_LONG(CLK_PLL_CONFIG1(1), ~CLK_PLL_CONFIG1_POFF),
CLK_AND_LONG(CLK_REDUCED_PM_CTRL, ~CLK_REDUCED_ON_XTAL_MEMSTDBY), /* on PLLs */
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */

_DELAY(),
_DELAY(),
DATA_AND_NOT_LONG(_SYS_CFG_H, _SYS_CFG_H_MASK),
DATA_WHILE_EQ(_SYS_MON_J, _SYS_MON_J_MASK, _SYS_MON_J_MASK),

_DELAY(),
_DELAY(),
_DELAY(),
_DELAY(),

_END()
};

static unsigned long stx5197_wrt_table[8] __cacheline_aligned;

static int stx5197_suspend_prepare(suspend_state_t state)
{
	int ret = -EINVAL;
	pm_message_t pms = {.event = PM_EVENT_SUSPEND, };
	emi_pm_state(pms);
/*	clk_pm_state(pms);*/
	sysconf_pm_state(pms);

	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = 0;
		break;
	}
	return ret;
}

static int stx5197_suspend_valid(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		return 1;
	};
	return 0;
}

/*
 * The xxxx_finish function is called after the resume
 * sysdev devices (i.e.: timer, cpufreq)
 * But it isn't a big issue in our platform
 */
static int stx5197_suspend_finish(suspend_state_t state)
{
	pm_message_t pms = {.event = PM_EVENT_ON, };
	sysconf_pm_state(pms);
/*	clk_pm_state(pms);*/
	emi_pm_state(pms);
	return 0;
}

static unsigned long stx5197_iomem[2] __cacheline_aligned = {
		stx5197_wrt_table,
		SYS_SERV_BASE_ADDR,};

static int stx5197_evt_to_irq(unsigned long evt)
{
	return ilc2irq(evt);
}

int __init suspend_platform_setup(struct sh4_suspend_t *st40data)
{

	struct sysconf_field* sc;
	st40data->iobase = stx5197_iomem;
	st40data->ops.valid = stx5197_suspend_valid;
	st40data->ops.finish = stx5197_suspend_finish;
	st40data->ops.prepare = stx5197_suspend_prepare;

	st40data->evt_to_irq = stx5197_evt_to_irq;

	st40data->stby_tbl = (unsigned long)stx5197_standby_table;
	st40data->stby_size = DIV_ROUND_UP(
		ARRAY_SIZE(stx5197_standby_table) * sizeof(long), L1_CACHE_BYTES);

	st40data->mem_tbl = (unsigned long)stx5197_mem_table;
	st40data->mem_size = DIV_ROUND_UP(
		ARRAY_SIZE(stx5197_mem_table) * sizeof(long), L1_CACHE_BYTES);

	st40data->wrt_tbl = (unsigned long)stx5197_wrt_table;
	st40data->wrt_size = DIV_ROUND_UP(
		ARRAY_SIZE(stx5197_wrt_table) * sizeof(long), L1_CACHE_BYTES);

	sc = sysconf_claim(SYS_DEV, CFG_MONITOR_J, 24, 24, "LMI pwd ack");
	stx5197_wrt_table[_SYS_MON_J] = (unsigned long)sysconf_address(sc);
	stx5197_wrt_table[_SYS_MON_J_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, CFG_CONTROL_H, 26, 26, "LMI pwd req");
	stx5197_wrt_table[_SYS_CFG_H] = (unsigned long)sysconf_address(sc);
	stx5197_wrt_table[_SYS_CFG_H_MASK] = sysconf_mask(sc);

	return 0;
}
