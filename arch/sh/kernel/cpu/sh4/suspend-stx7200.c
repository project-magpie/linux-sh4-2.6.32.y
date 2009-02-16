/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7200.c
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
#include <linux/stm/sysconf.h>
#include <linux/stm/pm.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pm.h>
#include <asm/irq-ilc.h>

#include "./soc-stx7200.h"

#define _SYS_STA4			(3)
#define _SYS_STA4_MASK			(4)
#define _SYS_STA6			(5)
#define _SYS_STA6_MASK			(6)

/* To powerdown the LMIs */
#define _SYS_CFG38			(7)
#define _SYS_CFG38_MASK			(8)
#define _SYS_CFG39			(9)
#define _SYS_CFG39_MASK			(10)

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7200_standby_table[] __cacheline_aligned = {
/* Down scale the GenA.Pll0 and GenA.Pll2*/
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF),
#if 0
CLK_AND_LONG(CLKA_PLL0, ~(0x7ffff)),
CLK_AND_LONG(CLKA_PLL2, ~(0x7ffff)),

CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_SUSPEND),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_SUSPEND),

CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF)),

CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),
#endif
/* END. */
_END(),

/* Restore the GenA.Pll0 and GenA.PLL2 original frequencies */
#if 0
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF),

DATA_LOAD(0x0),
IMMEDIATE_SRC0(CLKA_PLL0_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL0),

DATA_LOAD(0x1),
IMMEDIATE_SRC0(CLKA_PLL2_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL2),
#endif
CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF)),
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),

_DELAY(),
/* END. */
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */

static unsigned long stx7200_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
DATA_OR_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
DATA_OR_LONG(_SYS_CFG39, _SYS_CFG39_MASK),
	/* waits until the ack bit is zero */
DATA_WHILE_NEQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),
DATA_WHILE_NEQ(_SYS_STA6, _SYS_STA6_MASK, _SYS_STA6_MASK),

 /* waits until the ack bit is zero */
/* 2. Down scale the GenA.Pll0, GenA.Pll1 and GenA.Pll2*/
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF),
#if 0
CLK_AND_LONG(CLKA_PLL0, ~(0x7ffff)),
CLK_AND_LONG(CLKA_PLL1, ~(0x7ffff)),
CLK_AND_LONG(CLKA_PLL2, ~(0x7ffff)),

CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_SUSPEND),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_SUSPEND),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_SUSPEND),

CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF)),

CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL1, ~(CLKA_PLL1_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),
#endif
/* END. */
_END() ,

/* Restore the GenA.Pll0 and GenA.PLL2 original frequencies */
#if 0
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF),

DATA_LOAD(0x0),
IMMEDIATE_SRC0(CLKA_PLL0_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL0),

DATA_LOAD(0x1),
IMMEDIATE_SRC0(CLKA_PLL1_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL1),

DATA_LOAD(0x2),
IMMEDIATE_SRC0(CLKA_PLL2_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL2),
#endif
CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF)),

CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL1, ~(CLKA_PLL1_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),

DATA_AND_NOT_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
DATA_AND_NOT_LONG(_SYS_CFG39, _SYS_CFG39_MASK),
DATA_WHILE_EQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),

/* wait until the ack bit is high        */
DATA_WHILE_EQ(_SYS_STA6, _SYS_STA6_MASK, _SYS_STA6_MASK),

_DELAY(),
_DELAY(),
_DELAY(),
/* END. */
_END()
};

static unsigned long stx7200_wrt_table[16] __cacheline_aligned;

static int stx7200_suspend_prepare(suspend_state_t state)
{
	pm_message_t pm = {.event = PM_EVENT_SUSPEND, };
	emi_pm_state(pm);
	clk_pm_state(pm);
	sysconf_pm_state(pm);

	switch (state) {
	case PM_SUSPEND_STANDBY:
		stx7200_wrt_table[0] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL0) & 0x7ffff;
		stx7200_wrt_table[1] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL2) & 0x7ffff;
		return 0;
	case PM_SUSPEND_MEM:
		stx7200_wrt_table[0] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL0) & 0x7ffff;
		stx7200_wrt_table[1] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL1) & 0x7ffff;
		stx7200_wrt_table[2] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL2) & 0x7ffff;
	   return 0;
	}
	return -EINVAL;
}

static int stx7200_suspend_valid(suspend_state_t state)
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
static int stx7200_suspend_finish(suspend_state_t state)
{
	pm_message_t pm = {.event = PM_EVENT_ON, };
	sysconf_pm_state(pm);
	clk_pm_state(pm);
	emi_pm_state(pm);
	return 0;
}

static unsigned long stx7200_iomem[2] __cacheline_aligned = {
		stx7200_wrt_table,	/* To access Sysconf    */
		CLOCKGEN_BASE_ADDR};	/* Clockgen A */

static int stx7200_evttoirq(unsigned long evt)
{
	return ilc2irq(evt);
}

int __init suspend_platform_setup(struct sh4_suspend_t *st40data)
{
	struct sysconf_field* sc;

	st40data->iobase = stx7200_iomem;
	st40data->ops.valid = stx7200_suspend_valid;
	st40data->ops.finish = stx7200_suspend_finish;
	st40data->ops.prepare = stx7200_suspend_prepare;

	st40data->evt_to_irq = stx7200_evttoirq;

	st40data->stby_tbl = (unsigned long)stx7200_standby_table;
	st40data->stby_size = DIV_ROUND_UP(
		ARRAY_SIZE(stx7200_standby_table)*sizeof(long), L1_CACHE_BYTES);

	st40data->mem_tbl = (unsigned long)stx7200_mem_table;
	st40data->mem_size = DIV_ROUND_UP(
		ARRAY_SIZE(stx7200_mem_table)*sizeof(long), L1_CACHE_BYTES);

	st40data->wrt_tbl = (unsigned long)stx7200_wrt_table;
	st40data->wrt_size = DIV_ROUND_UP(
		ARRAY_SIZE(stx7200_wrt_table) * sizeof(long), L1_CACHE_BYTES);

	sc = sysconf_claim(SYS_STA, 4, 0, 0, "pm");
	stx7200_wrt_table[_SYS_STA4] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_STA4_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 6, 0, 0, "pm");
	stx7200_wrt_table[_SYS_STA6] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_STA6_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 38, 20, 20, "pm");
	stx7200_wrt_table[_SYS_CFG38] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_CFG38_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 39, 20, 20, "pm");
	stx7200_wrt_table[_SYS_CFG39] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_CFG39_MASK] = sysconf_mask(sc);

#ifdef CONFIG_PM_DEBUG
	ctrl_outl(0xc, CKGA_CLKOUT_SEL +
		CLOCKGEN_BASE_ADDR); /* sh4:2 routed on SYSCLK_OUT */
#endif
	return 0;
}
