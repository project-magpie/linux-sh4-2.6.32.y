/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stb7100.c
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
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pm.h>

#include "./soc-stb7100.h"

#define _SYS_STA12			(2)
#define _SYS_STA12_MASK			(3)
#define _SYS_STA13			(4)
#define _SYS_STA13_MASK			(5)
#define _SYS_CFG11			(6)
#define _SYS_CFG11_MASK			(7)

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */

static unsigned long stb7100_standby_table[] __cacheline_aligned = {
/* 1. PLL0 at the minimum frequency */
	/* Unlock the clocks */
CLK_POKE(CLKA_LOCK, 0xc0de),
	/* enables the bypass */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
	/* disable the pll0 */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_ENABLE)),
	/* set to zero mdiv ndiv pdiv */
CLK_AND_LONG(CLKA_PLL0, ~(0x7ffff)),
	/* set new mdiv ndiv pdiv */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_SUSPEND),
	/* enables the pll0 */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_ENABLE),
	/* removes the bypass */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
	/* 0 4 5 - 1:4 1:6 1:8	*/
	/* 3 4 5 - 1:4 1:6 1:8	*/
	/* 1 2 3 - 1:2 1:3 1:4	*/
CLK_POKE(CLKA_ST40_PER, 0x5),
CLK_POKE(CLKA_ST40_IC, 0x5),
CLK_POKE(CLKA_ST40, 0x3),
/* END. */
_END(),

/* 1.  Restore the highest frequency cpu/bus/per ratios */
CLK_POKE(CLKA_ST40, 0x0),
CLK_POKE(CLKA_ST40_IC, 0x1),
CLK_POKE(CLKA_ST40_PER, 0x0),

/* 2.  PLL0 at the standard frequency */
	/* enables bypass */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
	/* disables the pll0 */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_ENABLE)),
DATA_LOAD(0x0),
IMMEDIATE_SRC0(CLKA_PLL0_BYPASS),
_OR(),
	/* save the current r2 in PLL0 */
CLK_STORE(CLKA_PLL0),
	/* enables the pll0 */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_ENABLE),
	/* removes the bypass */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
	/* Lock the clocks */
CLK_POKE(CLKA_LOCK, 0x0),
/* END. */
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stb7100_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
DATA_OR_LONG(_SYS_CFG11, _SYS_CFG11_MASK),
	/* waits until the ack bit is zero */
DATA_WHILE_NEQ(_SYS_STA12, _SYS_STA12_MASK, _SYS_STA12_MASK),
	/* waits until the ack bit is zero */
DATA_WHILE_NEQ(_SYS_STA13, _SYS_STA13_MASK, _SYS_STA13_MASK),

/* 2. PLL0 at the minimum frequency */
	/* unlock the clocks */
CLK_POKE(CLKA_LOCK, 0xc0de),
	/* enables the bypass */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
	/* disable the pll0 */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_ENABLE)),
	/* set to zero mdiv ndiv pdiv */
CLK_AND_LONG(CLKA_PLL0, ~(0x7ffff)),
	/* set new mdiv ndiv pdiv */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_SUSPEND),
	/* enables the pll0 */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_ENABLE),
	/* removes the bypass */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),

/* 3. PLL1 at the minimum frequency */
	/* enables the bypass */
CLK_OR_LONG(CLKA_PLL1_BYPASS, 2),
	/* disable the pll1 */
CLK_AND_LONG(CLKA_PLL1, ~(CLKA_PLL1_ENABLE)),
	/* set to zero mdiv ndiv pdiv */
CLK_AND_LONG(CLKA_PLL1, ~(0x7ffff)),
	/* set new mdiv ndiv pdiv */
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_SUSPEND),
	/* enables the pll1 */
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_ENABLE),
CLK_AND_LONG(CLKA_PLL1_BYPASS, ~(2)),		/* removes the bypass */

/* 4. Turn-off the LMI clocks and the ST231 clocks */
CLK_AND_LONG(CLKA_CLK_EN, ~(CLKA_CLK_EN_DEFAULT)),
CLK_POKE(CLKA_ST40_PER, 0x5),			/* 0 4 5 - 1:4 1:6 1:8 */
CLK_POKE(CLKA_ST40_IC,  0x5),			/* 3 4 5 - 1:4 1:6 1:8 */
CLK_POKE(CLKA_ST40, 0x3),			/* 1 2 3 - 1:2 1:3 1:4 */
 /* END. */
_END(),

/* 1.  Restore the highest frequency cpu/bus/per ratios */
CLK_POKE(CLKA_ST40, 0x0),
CLK_POKE(CLKA_ST40_IC, 0x1),
CLK_POKE(CLKA_ST40_PER, 0x0),

/* 2. Turn-on the LMI clocks and the ST231 clocks*/
CLK_OR_LONG(CLKA_CLK_EN, CLKA_CLK_EN_DEFAULT),

/* 3. PLL1 at the standard frequency */
CLK_OR_LONG(CLKA_PLL1_BYPASS, 2), 		/* enables the bypass */
CLK_AND_LONG(CLKA_PLL1, ~(CLKA_PLL1_ENABLE)),	/* disable the pll1 */
DATA_LOAD(0x1),
CLK_STORE(CLKA_PLL1),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_ENABLE),	/* enables the pll1 */
CLK_AND_LONG(CLKA_PLL1_BYPASS, ~(2)),		/* removes the bypass */

/* 4. Disables the DDR self refresh mode */
DATA_AND_NOT_LONG(_SYS_CFG11, _SYS_CFG11_MASK),

/* wait until theack bit is high */
DATA_WHILE_EQ(_SYS_STA12, _SYS_STA12_MASK, _SYS_STA12_MASK),
DATA_WHILE_EQ(_SYS_STA13, _SYS_STA13_MASK, _SYS_STA12_MASK),

/* 5.  PLL0 at the standard frequency */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),	/* enables bypass */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_ENABLE)),	/* disables the pll0 */
DATA_LOAD(0x0),
IMMEDIATE_SRC0(CLKA_PLL0_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL0),				/* save the r2 in PLL0 */
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_ENABLE),	/* enables the pll0 */
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),	/* removes the bypass */
CLK_POKE(CLKA_LOCK, 0x0),

/* Lock the clocks  */
_DELAY(),
_DELAY(),
_DELAY(),
/* END. */
_END()
};

static unsigned long stb7100_wrt_table[8] __cacheline_aligned;

static int stb7100_suspend_prepare(suspend_state_t state)
{
	int ret = -EINVAL;
	pm_message_t pms = {.event = PM_EVENT_SUSPEND, };
	emi_pm_state(pms);
/*	clk_pm_state(pms);*/
	sysconf_pm_state(pms);
	switch (state) {
	case PM_SUSPEND_STANDBY:
		stb7100_wrt_table[0] = readl(clkgena_base + CLKA_PLL0) & 0x7ffff;
		ret = 0;
	break;
	case PM_SUSPEND_MEM:
		stb7100_wrt_table[0] = readl(clkgena_base + CLKA_PLL0) & 0x7ffff;
		stb7100_wrt_table[1] = readl(clkgena_base + CLKA_PLL1) & 0x7ffff;
		ret = 0;
	break;
	}
	return ret;
}

static int stb7100_suspend_valid(suspend_state_t state)
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
static int stb7100_suspend_finish(suspend_state_t state)
{
	pm_message_t pms = {.event = PM_EVENT_ON, };
	sysconf_pm_state(pms);
/*	clk_pm_state(pms);*/
	emi_pm_state(pms);
	return 0;
}

static int stb7100_evttoirq(unsigned long evt)
{
	return evt2irq(evt);
}

static unsigned long stb7100_iomem[2] __cacheline_aligned = {
	stb7100_wrt_table,
};

int __init suspend_platform_setup(struct sh4_suspend_t *st40data)
{
	struct sysconf_field* sc;

	stb7100_iomem[1] = (unsigned long) clkgena_base;

	st40data->iobase = stb7100_iomem;
	st40data->ops.valid  = stb7100_suspend_valid;
	st40data->ops.finish = stb7100_suspend_finish;
	st40data->ops.prepare = stb7100_suspend_prepare;

	st40data->evt_to_irq = stb7100_evttoirq;

	st40data->stby_tbl = (unsigned long)stb7100_standby_table;
	st40data->stby_size = DIV_ROUND_UP(
		ARRAY_SIZE(stb7100_standby_table)*sizeof(long), L1_CACHE_BYTES);;

	st40data->mem_tbl = (unsigned long)stb7100_mem_table;
	st40data->mem_size = DIV_ROUND_UP(
		ARRAY_SIZE(stb7100_mem_table)*sizeof(long), L1_CACHE_BYTES);

	st40data->wrt_tbl = (unsigned long)stb7100_wrt_table;
	st40data->wrt_size = DIV_ROUND_UP(
		ARRAY_SIZE(stb7100_wrt_table)*sizeof(long), L1_CACHE_BYTES);

	sc = sysconf_claim(SYS_STA, 12, 28, 28, "pm");
	stb7100_wrt_table[_SYS_STA12] = (unsigned long)sysconf_address(sc);
	stb7100_wrt_table[_SYS_STA12_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 13, 28, 28, "pm");
	stb7100_wrt_table[_SYS_STA13] = (unsigned long)sysconf_address(sc);
	stb7100_wrt_table[_SYS_STA13_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 11, 28, 28, "pm");
	stb7100_wrt_table[_SYS_CFG11] = (unsigned long)sysconf_address(sc);
	stb7100_wrt_table[_SYS_CFG11_MASK] = sysconf_mask(sc);
	sc = sysconf_claim(SYS_CFG, 11, 30, 30, "pm");
	stb7100_wrt_table[_SYS_CFG11_MASK] |= sysconf_mask(sc);

	return 0;
}
