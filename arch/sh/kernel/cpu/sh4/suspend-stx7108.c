/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>

#include <asm/irq-ilc.h>

#include "stm_suspend.h"

#define CGA0			0xFDE98000
#define CGA1			0xFDAB8000
#define SYS_1_BASE_ADDRESS	0xFDE20000
#define DDR3SS0_REG		0xFDE50000
#define DDR3SS1_REG		0xFDE70000

#define CKGA_PLL_CFG(x)			(0x4 * (x))
#define   CKGA_PLL_CFG_LOCK		(1 << 31)
#define CKGA_POWER_CFG			0x010
#define CKGA_CLKOPSRC_SWITCH_CFG	0x014
#define CKGA_CLKOPSRC_SWITCH_CFG2	0x024
#define CKGA_OSC_DIV_CFG(x)		(0x800 + (x) * 4)
#define CKGA_PLL0HS_DIV_CFG(x)		(0x900 + (x) * 4)
#define CKGA_PLL0LS_DIV_CFG(x)		(0xA00 + (x) * 4)
#define CKGA_PLL1_DIV_CFG(x)		(0xB00 + (x) * 4)


/*
 * The Stx7108 uses the Synopsys IP Dram Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM Protocol - Controller -
 *  Databook - Version 2.10a - February 4, 2009'
 */
#define DDR_SCTL		0x4
#define  DDR_SCTL_CFG			0x1
#define  DDR_SCTL_GO			0x2
#define  DDR_SCTL_SLEEP			0x3
#define  DDR_SCTL_WAKEUP		0x4

#define DDR_STAT		0x8
#define  DDR_STAT_CONFIG		0x1
#define  DDR_STAT_ACCESS		0x3
#define  DDR_STAT_LOW_POWER		0x5

#define DDR_PHY_IOCRV1		0x31C
#define DDR_PHY_PIR		0x404
#define DDR_PHY_DXCCR		0x434
/*
 * the following macros are valid only for SYSConf_Bank_1
  * where there are the ClockGen_D management registers
 */
#define SYS_BNK1_STA(x)		(0x4 * (x))
#define SYS_BNK1_CFG(x)		(0x4 * (x) + 0x3C)


static void __iomem *cga0;
static void __iomem *cga1;
static struct clk *comms_clk;


/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7108_standby_table[] __cacheline_aligned = {

POKE32(CGA1 + CKGA_OSC_DIV_CFG(11), 29),  /* CLKA_IC_REG_LP_ON @ 1 MHz to
				       * be safe for lirc
				       */
POKE32(CGA1 + CKGA_OSC_DIV_CFG(10), 31),	/* CLKA_IC_REG_LP_OFF */

POKE32(CGA0 + CKGA_OSC_DIV_CFG(16), 31),	/* STNoc */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(5), 31),	/* ST40 */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(4), 31),	/* ST40 C-L2 */

END_MARKER,

POKE32(CGA0 + CKGA_OSC_DIV_CFG(16), 0),	/* STNoc */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(5), 0),	/* ST40 */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(4), 0),	/* ST40 C-L2 */

/* CLKA_IC_REG_LP_ON @ 1 MHz to be safe for lirc*/
POKE32(CGA1 + CKGA_OSC_DIV_CFG(11), 0),
POKE32(CGA1 + CKGA_OSC_DIV_CFG(10), 0),	/* CLKA_IC_REG_LP_OFF */

END_MARKER
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx7108_mem_table[] __cacheline_aligned = {
POKE32(CGA1 + CKGA_OSC_DIV_CFG(11), 29),  /* CLKA_IC_REG_LP_ON @ 1 MHz to
				       * be safe for lirc
				       */
POKE32(CGA1 + CKGA_OSC_DIV_CFG(10), 31),	/* CLKA_IC_REG_LP_OFF */

POKE32(CGA0 + CKGA_OSC_DIV_CFG(16), 31),	/* STNoc */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(5), 31),	/* ST40 */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(4), 31),	/* ST40 C-L2 */

/* 1. Enables the DDR self refresh mode based on paraghaph. 7.1.4
 *    -> from ACCESS to LowPower
 */
POKE32(DDR3SS0_REG + DDR_SCTL, DDR_SCTL_SLEEP),
WHILE_NE32(DDR3SS0_REG + DDR_STAT, DDR_STAT_LOW_POWER, DDR_STAT_LOW_POWER),

OR32(DDR3SS0_REG + DDR_PHY_IOCRV1, 1),
OR32(DDR3SS0_REG + DDR_PHY_DXCCR, 1),

POKE32(DDR3SS1_REG + DDR_SCTL, DDR_SCTL_SLEEP),
WHILE_NE32(DDR3SS1_REG + DDR_STAT, DDR_STAT_LOW_POWER, DDR_STAT_LOW_POWER),

OR32(DDR3SS1_REG + DDR_PHY_IOCRV1, 1),
OR32(DDR3SS1_REG + DDR_PHY_DXCCR, 1),

OR32(DDR3SS0_REG + DDR_PHY_PIR, 1 << 7),
OR32(DDR3SS1_REG + DDR_PHY_PIR, 1 << 7),

/*WHILE_NE32(SYS_BNK1_STA(5), 1, 0),*/

 /* END. */
END_MARKER,

POKE32(CGA0 + CKGA_OSC_DIV_CFG(16), 0),	/* STNoc */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(5), 0),	/* ST40 */
POKE32(CGA0 + CKGA_OSC_DIV_CFG(4), 0),	/* ST40 C-L2 */

POKE32(CGA1 + CKGA_OSC_DIV_CFG(11), 0),  /* CLKA_IC_REG_LP_ON @ 1 MHz to
				       * be safe for lirc
				       */
POKE32(CGA1 + CKGA_OSC_DIV_CFG(10), 0),	/* CLKA_IC_REG_LP_OFF */


UPDATE32(DDR3SS0_REG + DDR_PHY_PIR, ~(1 << 7), 0),
UPDATE32(DDR3SS1_REG + DDR_PHY_PIR, ~(1 << 7), 0),
/*WHILE_NE32(SYS_BNK1_STA(5), 1, 1),*/

UPDATE32(DDR3SS0_REG + DDR_PHY_IOCRV1, ~1, 0),
UPDATE32(DDR3SS0_REG + DDR_PHY_DXCCR, ~1, 0),

UPDATE32(DDR3SS1_REG + DDR_PHY_IOCRV1, ~1, 0),
UPDATE32(DDR3SS1_REG + DDR_PHY_DXCCR, ~1, 0),




/* 2. Disables the DDR self refresh mode based on paraghaph 7.1.3
 *    -> from LowPower to Access
 */
POKE32(DDR3SS0_REG + DDR_SCTL, DDR_SCTL_WAKEUP),
WHILE_NE32(DDR3SS0_REG + DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),

POKE32(DDR3SS0_REG + DDR_SCTL, DDR_SCTL_CFG),
WHILE_NE32(DDR3SS0_REG + DDR_STAT, DDR_STAT_CONFIG, DDR_STAT_CONFIG),

POKE32(DDR3SS0_REG + DDR_SCTL, DDR_SCTL_GO),
WHILE_NE32(DDR3SS0_REG + DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),

POKE32(DDR3SS1_REG + DDR_SCTL, DDR_SCTL_WAKEUP),
WHILE_NE32(DDR3SS1_REG + DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),

POKE32(DDR3SS1_REG + DDR_SCTL, DDR_SCTL_CFG),
WHILE_NE32(DDR3SS1_REG + DDR_STAT, DDR_STAT_CONFIG, DDR_STAT_CONFIG),

POKE32(DDR3SS1_REG + DDR_SCTL, DDR_SCTL_GO),
WHILE_NE32(DDR3SS1_REG + DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),


END_MARKER
};

static int stx7108_suspend_begin(suspend_state_t state)
{

	printk(KERN_INFO"[STM][PM] Analyzing the wakeup devices\n");

	comms_clk->rate = 1000000;	/* 1 MHz */

	return 0;
}

static int stx7108_suspend_core(suspend_state_t state, int suspending)
{
	static char *osc_regs;
	static char *pll0_regs;
	static char *pll1_regs;
	static long *switch_cfg;
	int i;

	if (suspending)
		goto on_suspending;

	if (!(osc_regs && pll0_regs && pll1_regs && switch_cfg))
		return 0;

	iowrite32(0, cga0 + CKGA_POWER_CFG);
	iowrite32(0, cga1 + CKGA_POWER_CFG);

	for (i = 0; i < 2; ++i)
		while (!(ioread32(cga0 + CKGA_PLL_CFG(i)) & CKGA_PLL_CFG_LOCK));
	for (i = 0; i < 2; ++i)
		while (!(ioread32(cga1 + CKGA_PLL_CFG(i)) & CKGA_PLL_CFG_LOCK));

	/* apply the original parents */
	iowrite32(switch_cfg[0], cga0 + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(switch_cfg[1], cga0 + CKGA_CLKOPSRC_SWITCH_CFG2);
	iowrite32(switch_cfg[2], cga1 + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(switch_cfg[3], cga1 + CKGA_CLKOPSRC_SWITCH_CFG2);

	for (i = 0; i < 18; ++i) {
		iowrite32(osc_regs[i], cga0 + CKGA_OSC_DIV_CFG(i));
		iowrite32(pll0_regs[i], cga0 +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		iowrite32(pll1_regs[i], cga0 + CKGA_PLL1_DIV_CFG(i));

		iowrite32(osc_regs[i + 18], cga1 + CKGA_OSC_DIV_CFG(i));
		iowrite32(pll0_regs[i + 18], cga1 +
			((i < 4) ? CKGA_PLL0HS_DIV_CFG(i) :
				CKGA_PLL0LS_DIV_CFG(i)));
		iowrite32(pll1_regs[i + 18], cga0 + CKGA_PLL1_DIV_CFG(i));
	}
	kfree(osc_regs);
	kfree(pll0_regs);
	kfree(pll1_regs);
	kfree(switch_cfg);

	switch_cfg = NULL;
	pll0_regs = NULL;
	pll1_regs = NULL;
	osc_regs = NULL;

	comms_clk->rate = comms_clk->parent->rate;
	pr_debug("[STM][PM] ClockGens A: restored\n");
	return 0;

on_suspending:
	osc_regs = kmalloc(2 * 18, GFP_ATOMIC);
	pll0_regs = kmalloc(2 * 18, GFP_ATOMIC);
	pll1_regs = kmalloc(2 * 18, GFP_ATOMIC);
	switch_cfg = kmalloc(sizeof(long) * 2 * 2, GFP_ATOMIC);

	if (!(osc_regs && pll0_regs && pll1_regs && switch_cfg))
		goto error;
	/* Save the original parents */
	switch_cfg[0] = ioread32(cga0 + CKGA_CLKOPSRC_SWITCH_CFG);
	switch_cfg[1] = ioread32(cga0 + CKGA_CLKOPSRC_SWITCH_CFG2);
	switch_cfg[2] = ioread32(cga1 + CKGA_CLKOPSRC_SWITCH_CFG);
	switch_cfg[3] = ioread32(cga1 + CKGA_CLKOPSRC_SWITCH_CFG2);

	/* 18 OSC register for each bank */
	for (i = 0; i < 18; ++i) {
		/* CGA 0 */
		osc_regs[i] = (char)ioread32(cga0 + CKGA_OSC_DIV_CFG(i));
		pll0_regs[i] = (char)ioread32(cga0 + ((i < 4) ?
			CKGA_PLL0HS_DIV_CFG(i) : CKGA_PLL0LS_DIV_CFG(i)));
		pll1_regs[i] = (char)ioread32(cga0 + CKGA_PLL1_DIV_CFG(i));
		/* CGA 1 */
		osc_regs[i + 18] = (char)ioread32(cga1 + CKGA_OSC_DIV_CFG(i));
		pll0_regs[i + 18] = (char)ioread32(cga1 + ((i < 4) ?
			CKGA_PLL0HS_DIV_CFG(i) : CKGA_PLL0LS_DIV_CFG(i)));
		pll1_regs[i + 18] = (char)ioread32(cga1 + CKGA_PLL1_DIV_CFG(i));

		iowrite32(0x1f , cga0 + CKGA_OSC_DIV_CFG(i));
		iowrite32(0x1f , cga1 + CKGA_OSC_DIV_CFG(i));
	}

	iowrite32(0xFFC3FCFF, cga0 + CKGA_CLKOPSRC_SWITCH_CFG);
	iowrite32(0xF3FFFFFF , cga1 + CKGA_CLKOPSRC_SWITCH_CFG);

	if (state == PM_SUSPEND_MEM) {
		/* all the clocks on xtal */
		iowrite32(0xF, cga0 + CKGA_CLKOPSRC_SWITCH_CFG2);
		iowrite32(0xF, cga1 + CKGA_CLKOPSRC_SWITCH_CFG2);
		/* turn-off the PLLs*/
		iowrite32(3, cga0 + CKGA_POWER_CFG);
		iowrite32(3, cga1 + CKGA_POWER_CFG);
	}

	pr_debug("[STM][PM] ClockGens A: saved\n");
	return 0;
error:
	kfree(osc_regs);
	kfree(pll1_regs);
	kfree(pll0_regs);
	kfree(switch_cfg);

	switch_cfg = NULL;
	pll0_regs = NULL;
	pll1_regs = NULL;
	osc_regs = NULL;

	return -ENOMEM;
}

static int stx7108_suspend_pre_enter(suspend_state_t state)
{
	return stx7108_suspend_core(state, 1);
}

static int stx7108_suspend_post_enter(suspend_state_t state)
{
	return stx7108_suspend_core(state, 0);
}

static int stx7108_evttoirq(unsigned long evt)
{
	return ((evt == 0xa00) ? ilc2irq(evt) : evt2irq(evt));
}

static struct stm_platform_suspend_t stx7108_suspend __cacheline_aligned = {
	.ops.begin = stx7108_suspend_begin,

	.evt_to_irq = stx7108_evttoirq,
	.pre_enter = stx7108_suspend_pre_enter,
	.post_enter = stx7108_suspend_post_enter,

	.stby_tbl = (unsigned long)stx7108_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7108_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_mem_table) * sizeof(long),
			L1_CACHE_BYTES),

};

static int __init stx7108_suspend_setup(void)
{
	struct sysconf_field *sc[2];
	int i;

	/* ClockGen_D.Pll power up/down*/
	sc[0] = sysconf_claim(SYS_CFG_BANK1, 4, 0, 0, "PM");
	/* ClockGen_D.Pll lock status */
	sc[1] = sysconf_claim(SYS_STA_BANK1, 5, 0, 0, "PM");


	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (!sc[i])
			goto error;

	comms_clk = clk_get(NULL, "comms_clk");

	cga0 = ioremap(CGA0, 0x1000);
	cga1 = ioremap(CGA1, 0x1000);

	return stm_suspend_register(&stx7108_suspend);

error:
	pr_err("[STM][PM] Error to acquire the sysconf registers\n");
	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (sc[i])
			sysconf_release(sc[i]);

	return -EBUSY;
}

late_initcall(stx7108_suspend_setup);