/*
 * Copyright (C) 2007 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STx7200.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>

#include "./clock-common.h"
#include "./soc-stx7200.h"

/* Alternate clock for clockgen A, B and C respectivly */
/* B & C come from SYSCLKINALT pin, SYSCLKINALT2 from PIO2[2] */
unsigned long sysclkinalt[3] = { 0,0,0};

#define CLOCKGEN_PLL_CFG(pll)	(CLOCKGEN_BASE_ADDR + ((pll)*0x4))
#define   CLOCKGEN_PLL_CFG_BYPASS		(1<<20)
#define CLOCKGEN_MUX_CFG	(CLOCKGEN_BASE_ADDR + 0x0c)
#define   CLOCKGEN_MUX_CFG_SYSCLK_SRC		(1<<0)
#define   CLOCKGEN_MUX_CFG_PLL_SRC(pll)		(1<<((pll)+1))
#define   CLOCKGEN_MUX_CFG_DIV_SRC(pll)		(1<<((pll)+4))
#define   CLOCKGEN_MUX_CFG_FDMA_SRC(fdma)	(1<<((fdma)+7))
#define   CLOCKGEN_MUX_CFG_IC_REG_SRC		(1<<9)
#define CLOCKGEN_DIV_CFG	(CLOCKGEN_BASE_ADDR + 0x10)
#define CLOCKGEN_DIV2_CFG	(CLOCKGEN_BASE_ADDR + 0x14)
#define CLOCKGEN_CLKOBS_MUX_CFG	(CLOCKGEN_BASE_ADDR + 0x18)
#define CLOCKGEN_POWER_CFG	(CLOCKGEN_BASE_ADDR + 0x1c)

#define CLOCKGENB_PLL0_CFG	(CLOCKGENB_BASE_ADDR + 0x3c)
#define CLOCKGENB_IN_MUX_CFG	(CLOCKGENB_BASE_ADDR + 0x44)
#define   CLOCKGENB_IN_MUX_CFG_PLL_SRC		(1<<0)
#define CLOCKGENB_DIV_CFG	(CLOCKGENB_BASE_ADDR + 0x4c)
#define CLOCKGENB_OUT_MUX_CFG	(CLOCKGENB_BASE_ADDR + 0x48)
#define   CLOCKGENB_OUT_MUX_CFG_DIV_SRC		(1<<0)
#define CLOCKGENB_DIV2_CFG	(CLOCKGENB_BASE_ADDR + 0x50)
#define CLOCKGENB_CLKOBS_MUX_CFG (CLOCKGENB_BASE_ADDR + 0x54)
#define CLOCKGENB_POWER_CFG	(CLOCKGENB_BASE_ADDR + 0x58)

                                    /* 0  1  2  3  4  5  6     7  */
static const unsigned int ratio1[] = { 1, 2, 3, 4, 6, 8, 1024, 1 };
static const unsigned int ratio2[] = { 0, 1, 2, 1024, 3, 3, 3, 3 };

static unsigned long final_divider(unsigned long input, int div_ratio, int div)
{
	switch (div_ratio) {
	case 1:
		return input / 1024;
	case 2:
	case 3:
		return input / div;
	}

	return 0;
}

static unsigned long pll02_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, pdiv, mdiv;

	mdiv = (cfg >>  0) & 0xff;
	ndiv = (cfg >>  8) & 0xff;
	pdiv = (cfg >> 16) & 0x7;
	freq = (((2 * (input / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static unsigned long pll1_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, mdiv;

	mdiv = (cfg >>  0) & 0x7;
	ndiv = (cfg >>  8) & 0xff;
	freq = (((input / 1000) * ndiv) / mdiv) * 1000;

	return freq;
}

/* Note this returns the PLL frequency _after_ the bypass logic. */
static unsigned long pll_freq(int pll_num)
{
	unsigned long sysabclkin, input, output;
	unsigned long mux_cfg, pll_cfg;

	mux_cfg = ctrl_inl(CLOCKGEN_MUX_CFG);
	if ((mux_cfg & CLOCKGEN_MUX_CFG_SYSCLK_SRC) == 0) {
		sysabclkin = SYSACLKIN;
	} else {
		sysabclkin = SYSBCLKIN;
	}

	if (mux_cfg & CLOCKGEN_MUX_CFG_PLL_SRC(pll_num)) {
		input = sysclkinalt[0];
	} else {
		input = sysabclkin;
	}

	pll_cfg = ctrl_inl(CLOCKGEN_PLL_CFG(pll_num));
	if (pll_num == 1) {
		output = pll1_freq(input, pll_cfg);
	} else {
		output = pll02_freq(input, pll_cfg);
	}

	if ((pll_cfg & CLOCKGEN_PLL_CFG_BYPASS) == 0) {
		return output;
	} else if ((mux_cfg & CLOCKGEN_MUX_CFG_DIV_SRC(pll_num)) == 0) {
		return input;
	} else {
		return sysabclkin;
	}
}

static void pll_clk_init(struct clk *clk)
{
	clk->rate = pll_freq((int)clk->private_data);
}

static struct clk_ops pll_clk_ops = {
	.init		= pll_clk_init,
};

#define CLK_PLL(_name, _id)					\
{	.name = _name,						\
	.flags  = CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,	\
	.ops    = &pll_clk_ops,					\
	.private_data = (void *)(_id),				\
}

static struct clk pllclks[3] = {
	CLK_PLL("pll0_clk", 0),
	CLK_PLL("pll1_clk", 1),
	CLK_PLL("pll2_clk", 2),
};

/* Note we ignore the possibility that we are in SH4 mode.
 * Should check DIV_CFG.sh4_clk_ctl and switch to FRQCR mode. */
static void sh4_clk_recalc(struct clk *clk)
{
	unsigned long shift = (unsigned long)clk->private_data;
	unsigned long div_cfg = ctrl_inl(CLOCKGEN_DIV_CFG);
	unsigned long div1 = 1, div2;

	switch ((div_cfg >> 20) & 3) {
	case 0:
		clk->rate = 0;
		return;
	case 1:
		div1 = 1;
		break;
	case 2:
	case 3:
		div1 = 2;
		break;
	}
	if (cpu_data->cut_major < 2)
		div2 = ratio1[(div_cfg >> shift) & 7];
	else
		div2 = ratio2[(div_cfg >> shift) & 7];
	clk->rate = (clk->parent->rate / div1) / div2;

	/* Note clk_sh4 and clk_sh4_ic have an extra clock gating
	 * stage here based on DIV2_CFG bits 0 and 1. clk_sh4_per (aka
	 * module_clock) doesn't.
	 *
	 * However if we ever implement this, remember that fdma0/1
	 * may use clk_sh4 prior to the clock gating.
	 */
}

static struct clk_ops sh4_clk_ops = {
	.recalc		= sh4_clk_recalc,
};

#define SH4_CLK(_name, _shift)					\
{	.name = _name,						\
	.flags  = CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,	\
	.parent = &pllclks[0],					\
	.ops    = &sh4_clk_ops,					\
	.private_data = (void *)(_shift),			\
}

static struct clk sh4clks[3] = {
	SH4_CLK("sh4_clk", 1),
	SH4_CLK("sh4_ic_clk", 4),
	SH4_CLK("module_clk", 7),
};

struct fdmalxclk {
	char fdma_num;
	char div_cfg_reg;
	char div_cfg_shift;
	char normal_div;
};

static void fdma_clk_init(struct clk *clk)
{
	struct fdmalxclk *fdmaclk =  (struct fdmalxclk *)clk->private_data;
	unsigned long mux_cfg = ctrl_inl(CLOCKGEN_MUX_CFG);

	if ((mux_cfg & CLOCKGEN_MUX_CFG_FDMA_SRC(fdmaclk->fdma_num)) == 0)
		clk->parent = &sh4clks[0];
	else
		clk->parent = &pllclks[1];
}

static void fdmalx_clk_recalc(struct clk *clk)
{
	struct fdmalxclk *fdmalxclk =  (struct fdmalxclk *)clk->private_data;
	unsigned long div_cfg;
	unsigned long div_ratio;
	unsigned long normal_div;

	div_cfg = ctrl_inl(CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
	div_ratio = (div_cfg >> fdmalxclk->div_cfg_shift) & 3;
	normal_div = fdmalxclk->normal_div;
	clk->rate = final_divider(clk->parent->rate, div_ratio, normal_div);
}

static int lx_clk_XXable(struct clk *clk, int enable)
{
	struct fdmalxclk *fdmalxclk = (struct fdmalxclk *)clk->private_data;
	unsigned long div_cfg = ctrl_inl(CLOCKGEN_DIV_CFG +
			fdmalxclk->div_cfg_reg);
	if (enable) {
		ctrl_outl(div_cfg |
			(fdmalxclk->normal_div << fdmalxclk->div_cfg_shift),
			CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
		fdmalx_clk_recalc(clk); /* to evaluate the rate */
	} else {
		ctrl_outl(div_cfg & ~(0x3<<fdmalxclk->div_cfg_shift),
			CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
		clk->rate = 0;
	}

	return 0;
}
static int lx_clk_enable(struct clk *clk)
{
	return lx_clk_XXable(clk, 1);
}
static int lx_clk_disable(struct clk *clk)
{
	return lx_clk_XXable(clk, 0);
}

static struct clk_ops fdma_clk_ops = {
	.init		= fdma_clk_init,
	.recalc		= fdmalx_clk_recalc,
};

static struct clk_ops lx_clk_ops = {
	.recalc		= fdmalx_clk_recalc,
	.enable		= lx_clk_enable,
	.disable	= lx_clk_disable,
};

static void ic266_clk_recalc(struct clk *clk)
{
	unsigned long div_cfg;
	unsigned long div_ratio;

	div_cfg = ctrl_inl(CLOCKGEN_DIV2_CFG);
	div_ratio = ((div_cfg & (1<<5)) == 0) ? 1024 : 3;
	clk->rate = clk->parent->rate / div_ratio;
}

static struct clk_ops ic266_clk_ops = {
	.recalc		= ic266_clk_recalc,
};

#define CLKGENA(_name, _parent, _ops, _flags)			\
	{							\
		.name		= #_name,			\
		.parent		= _parent,			\
		.flags		= CLK_ALWAYS_ENABLED | _flags,	\
		.ops		= &_ops,			\
	}

static struct clk miscclks[1] = {
	CLKGENA(ic_266, &pllclks[2], ic266_clk_ops, 0),
};

#define CLKGENA_FDMALX(_name, _parent, _ops, _fdma_num, _div_cfg_reg, _div_cfg_shift, _normal_div) \
{								\
	.name		= #_name,				\
	.parent		= _parent,				\
	.flags		= CLK_ALWAYS_ENABLED,			\
	.ops		= &_ops,				\
	.private_data = (void *) &(struct fdmalxclk)		\
	{							\
		.fdma_num = _fdma_num,				\
		.div_cfg_reg = _div_cfg_reg - CLOCKGEN_DIV_CFG,	\
		.div_cfg_shift = _div_cfg_shift,		\
		.normal_div = _normal_div,			\
	}							\
}

#define CLKGENA_FDMA(name, num)					\
	CLKGENA_FDMALX(name, NULL, fdma_clk_ops, num,		\
			CLOCKGEN_DIV_CFG, 10, 1)

#define CLKGENA_LX(name, shift)				\
	CLKGENA_FDMALX(name, &pllclks[1], lx_clk_ops, 0,	\
			CLOCKGEN_DIV_CFG, shift, 1)

#define CLKGENA_MISCDIV(name, shift, ratio)		\
	CLKGENA_FDMALX(name, &pllclks[2], lx_clk_ops, 0,	\
			CLOCKGEN_DIV2_CFG, shift, ratio)

static struct clk fdma_lx_miscdiv_clks[] = {
	CLKGENA_FDMA(fdma_clk0, 0),
	CLKGENA_FDMA(fdma_clk1, 1),

	CLKGENA_LX(lx_aud0_cpu_clk, 12),
	CLKGENA_LX(lx_aud1_cpu_clk, 14),
	CLKGENA_LX(lx_dmu0_cpu_clk, 16),
	CLKGENA_LX(lx_dmu1_cpu_clk, 18),

	CLKGENA_MISCDIV(dmu0_266, 18, 3),
	CLKGENA_MISCDIV(disp_266, 22, 3),
	CLKGENA_MISCDIV(bdisp_200, 6, 4),
	CLKGENA_MISCDIV(fdma_200, 14, 4)
};

static struct clk *clockgena_clocks[] = {
	&pllclks[0],
	&pllclks[1],
	&pllclks[2],
	&sh4clks[0],
	&fdma_lx_miscdiv_clks[0],
	&fdma_lx_miscdiv_clks[1],
	&fdma_lx_miscdiv_clks[2],
	&fdma_lx_miscdiv_clks[3],
	&fdma_lx_miscdiv_clks[4],
	&fdma_lx_miscdiv_clks[5],
	&fdma_lx_miscdiv_clks[6],
	&fdma_lx_miscdiv_clks[7],
	&fdma_lx_miscdiv_clks[8],
	&fdma_lx_miscdiv_clks[9],
	&miscclks[0],
};


enum clockgen2B_ID {
	DIV2_B_BDISP266_ID = 0,
	DIV2_B_COMPO200_ID,
	DIV2_B_DISP200_ID,
	DIV2_B_VDP200_ID,
	DIV2_B_DMU1266_ID,
};

enum clockgen3B_ID{
	MISC_B_ICREG_ID = 0,
	MISC_B_ETHERNET_ID,
	MISC_B_EMIMASTER_ID
};

static void pll_clkB_init(struct clk *clk)
{
	unsigned long input, output;
	unsigned long mux_cfg, pll_cfg;

	/* FIXME: probably needs more work! */

	mux_cfg = ctrl_inl(CLOCKGENB_IN_MUX_CFG);
	if (mux_cfg & CLOCKGENB_IN_MUX_CFG_PLL_SRC) {
		input = sysclkinalt[1];
	} else {
		input = SYSBCLKIN;
	}

	pll_cfg = ctrl_inl(CLOCKGENB_PLL0_CFG);
	output = pll02_freq(input, pll_cfg);

	if (!(pll_cfg & CLOCKGEN_PLL_CFG_BYPASS)) {
		clk->rate = output;
	} else if (!(mux_cfg & CLOCKGENB_OUT_MUX_CFG_DIV_SRC)) {
		clk->rate = input;
	} else {
		clk->rate = SYSBCLKIN;
	}
}

static int pll_clkB_XXable(struct clk *clk, int enable)
{
	unsigned long bps = ctrl_inl(CLOCKGENB_PLL0_CFG);
	unsigned long pwr = ctrl_inl(CLOCKGENB_POWER_CFG);

	if (enable) {
		ctrl_outl(pwr & ~(1<<15), CLOCKGENB_POWER_CFG);	 /* turn-on  */
		mdelay(1);
		ctrl_outl(bps & ~(1<<20), CLOCKGENB_PLL0_CFG);	/* bypass off*/
		pll_clkB_init(clk); /* to evaluate the rate */
	} else {
		ctrl_outl(bps | 1<<20, CLOCKGENB_PLL0_CFG);	/* bypass on */
		ctrl_outl(pwr | 1<<15, CLOCKGENB_POWER_CFG); 	/* turn-off  */
		clk->rate = 0;
	}
	return 0;
}

static int pll_clkB_enable(struct clk *clk)
{
	return pll_clkB_XXable(clk, 1);
}

static int pll_clkB_disable(struct clk *clk)
{
	return pll_clkB_XXable(clk, 0);
}

static struct clk_ops pll_clkB_ops = {
	.init		= pll_clkB_init,
	.enable		= pll_clkB_enable,
	.disable	= pll_clkB_disable,
};

static struct clk clkB_pllclks[1] =
{
	{
	.name		= "b_pll0_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll_clkB_ops,
	.private_data	= NULL,
	}
};


struct clkgenBdiv2 {
	char   div_cfg_shift;
	char   normal_div;
};

#define CLKGENB(_id, _name, _ops, _flags)			\
	{							\
		.name		= #_name,			\
		.parent		= &clkB_pllclks[0],		\
		.flags		= CLK_ALWAYS_ENABLED | _flags,	\
		.ops		= &_ops,			\
		.id		= (_id),			\
	}

#define CLKGENB_DIV2(_id, _name, _div_cfg_shift, _normal_div)	\
{								\
	.name		= #_name,				\
	.parent		= &clkB_pllclks[0],			\
	.flags		= CLK_ALWAYS_ENABLED,			\
	.ops		= &clkgenb_div2_ops,			\
	.id		= _id,					\
	.private_data   = (void *) &(struct clkgenBdiv2)	\
	 {							\
		.div_cfg_shift = _div_cfg_shift,		\
		.normal_div = _normal_div,			\
	}							\
}

static void clkgenb_div2_recalc(struct clk *clk)
{
	struct clkgenBdiv2 *clkgenBdiv2 = (struct clkgenBdiv2 *)clk->private_data;
	unsigned long div_cfg;
	unsigned long div_ratio;

	div_cfg = ctrl_inl(CLOCKGENB_DIV2_CFG);
	div_ratio = (div_cfg >> clkgenBdiv2->div_cfg_shift) & 3;
	clk->rate = final_divider(clk->parent->rate, div_ratio,
				  clkgenBdiv2->normal_div);
}

static int clkgenb_div2_XXable(struct clk *clk, int enable)
{
	struct clkgenBdiv2 *clkgenBdiv2 =
			(struct clkgenBdiv2 *)clk->private_data;
	unsigned long div_cfg = ctrl_inl(CLOCKGENB_DIV2_CFG);
	unsigned long div_ratio = (div_cfg >> clkgenBdiv2->div_cfg_shift) & 3;

	if (enable) {
		div_cfg |= clkgenBdiv2->normal_div <<
			clkgenBdiv2->div_cfg_shift;
		ctrl_outl(div_cfg, CLOCKGENB_DIV2_CFG);
		final_divider(clk->parent->rate, div_ratio,
			clkgenBdiv2->normal_div); /* to evaluate the rate */
	} else {
		div_cfg &= ~(0x3<<clkgenBdiv2->div_cfg_shift);
		ctrl_outl(div_cfg, CLOCKGENB_DIV2_CFG);
		clk->rate = 0;
	}
	return 0;
}

static int clkgenb_div2_enable(struct clk *clk)
{
	return clkgenb_div2_XXable(clk, 1);
}
static int clkgenb_div2_disable(struct clk *clk)
{
	return clkgenb_div2_XXable(clk, 0);
}

static struct clk_ops clkgenb_div2_ops = {
	.enable		= clkgenb_div2_enable,
	.disable	= clkgenb_div2_disable,
	.recalc		= clkgenb_div2_recalc,
};

struct clk clkB_div2clks[5] = {
	CLKGENB_DIV2(DIV2_B_BDISP266_ID, bdisp_266,  6, 3),
	CLKGENB_DIV2(DIV2_B_COMPO200_ID, compo_200,  8, 4),
	CLKGENB_DIV2(DIV2_B_DISP200_ID, disp_200,  10, 4),
	CLKGENB_DIV2(DIV2_B_VDP200_ID, vdp_200,   12, 4),
	CLKGENB_DIV2(DIV2_B_DMU1266_ID, dmu1_266,  20, 3)
};

static void icreg_emi_eth_clk_recalc(struct clk *clk)
{
	unsigned long mux_cfg;
	unsigned long div_ratio;

	mux_cfg = ctrl_inl(CLOCKGEN_MUX_CFG);
	div_ratio = ((mux_cfg & (CLOCKGEN_MUX_CFG_IC_REG_SRC)) == 0) ? 8 : 6;
	clk->rate = clk->parent->rate / div_ratio;
}

static int icreg_emi_eth_clk_XXable(struct clk *clk, int enable)
{
	unsigned long id = (unsigned long)clk->private_data;
	unsigned long tmp = ctrl_inl(CLOCKGENB_DIV2_CFG);
	if (enable) {
		ctrl_outl(tmp & ~(1<<(id+2)), CLOCKGENB_DIV2_CFG);
		icreg_emi_eth_clk_recalc(clk); /* to evaluate the rate */
	} else {
		ctrl_outl(tmp | (1<<(id+2)), CLOCKGENB_DIV2_CFG);
		clk->rate = 0;
	}
	return 0;
}
static int icreg_emi_eth_clk_enable(struct clk *clk)
{
	return icreg_emi_eth_clk_XXable(clk, 1);
}
static int icreg_emi_eth_clk_disable(struct clk *clk)
{
	return icreg_emi_eth_clk_XXable(clk, 0);
}

static struct clk_ops icreg_emi_eth_clk_ops = {
	.recalc		= icreg_emi_eth_clk_recalc,
#if 0
/* I have to check why the following function have problem on cut 2 */
	.enable		= icreg_emi_eth_clk_enable,
	.disable	= icreg_emi_eth_clk_disable
#endif
};

struct clk clkB_miscclks[3] = {
	/* Propages to comms_clk */
	CLKGENB(MISC_B_ICREG_ID, ic_reg, icreg_emi_eth_clk_ops, CLK_RATE_PROPAGATES),
	CLKGENB(MISC_B_ETHERNET_ID, ethernet,   icreg_emi_eth_clk_ops, 0),
	CLKGENB(MISC_B_EMIMASTER_ID, emi_master, icreg_emi_eth_clk_ops, 0),
};

static struct clk *clockgenb_clocks[] = {
	&clkB_pllclks[0],

	&clkB_div2clks[DIV2_B_BDISP266_ID],
	&clkB_div2clks[DIV2_B_COMPO200_ID],
	&clkB_div2clks[DIV2_B_DISP200_ID],
	&clkB_div2clks[DIV2_B_VDP200_ID],
	&clkB_div2clks[DIV2_B_DMU1266_ID],

	&clkB_miscclks[MISC_B_ICREG_ID],
	&clkB_miscclks[MISC_B_ETHERNET_ID],
	&clkB_miscclks[MISC_B_EMIMASTER_ID]
};

static void comms_clk_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
}

static struct clk_ops comms_clk_ops = {
	.recalc		= comms_clk_recalc,
};

struct clk comms_clk = {
	.name		= "comms_clk",
	.parent		= &clkB_miscclks[MISC_B_ICREG_ID],
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &comms_clk_ops
};

static struct clk new_module_clk = {
	.name		= "module_clk",
	.parent		= &clkB_miscclks[0],
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &comms_clk_ops
};

#ifdef CONFIG_PM
int clk_pm_state(pm_message_t state)
{
	static int prev_state = PM_EVENT_ON;
	unsigned long tmp;
	switch (state.event) {
	case PM_EVENT_ON:
	if (prev_state == PM_EVENT_FREEZE) {
	} else {
		/* Restore the GenB.Pll0 frequency */
		tmp = readl(CLOCKGENB_BASE_ADDR + CLKB_PWR_CFG);
		writel(tmp & ~CLKB_PLL0_OFF,
			CLOCKGENB_BASE_ADDR + CLKB_PWR_CFG);

		mdelay(10); /* wait for stable signal */
		tmp = readl(CLOCKGENB_BASE_ADDR + CLKB_PLL0_CFG);
		writel(tmp & ~CLKB_PLL0_BYPASS,
			CLOCKGENB_BASE_ADDR + CLKB_PLL0_CFG);
		mdelay(10); /* wait for stable signal */
	}
	break;
	case PM_EVENT_SUSPEND:
		/* Reduce the GenB.Pll0 frequency */
		tmp = readl(CLOCKGENB_BASE_ADDR + CLKB_PLL0_CFG);
		writel(tmp | CLKB_PLL0_BYPASS,
			CLOCKGENB_BASE_ADDR + CLKB_PLL0_CFG);

		tmp = readl(CLOCKGENB_BASE_ADDR + CLKB_PWR_CFG);
		writel(tmp | CLKB_PLL0_OFF,
			CLOCKGENB_BASE_ADDR + CLKB_PWR_CFG);
		break;
	case PM_EVENT_FREEZE:
		break;
	}
	prev_state = state.event;
	return 0;
}
#endif

int __init clk_init(void)
{
	int i, ret = 0;

	/* Clockgen A */
	for (i = 0; i < ARRAY_SIZE(clockgena_clocks); i++) {
		struct clk *clk = clockgena_clocks[i];

		ret |= clk_register(clk);
		clk_enable(clk);
	}
	if (cpu_data->cut_major < 2) {
		/* module clock */
		ret |= clk_register(&sh4clks[2]);
		clk_enable(&sh4clks[2]);

		/* interconnect clock */
		ret |= clk_register(&sh4clks[1]);
		clk_enable(&sh4clks[1]);
	}

	/* Propagate the PLL values down */
	for (i=0; i<3; i++) {
		clk_set_rate(&pllclks[i], clk_get_rate(&pllclks[i]));
		clk_put(&pllclks[i]);
	}

	/* Clockgen B */
	ctrl_outl(ctrl_inl(CLOCKGENB_IN_MUX_CFG) & ~0xf, CLOCKGENB_IN_MUX_CFG);
	for (i = 0; i < ARRAY_SIZE(clockgenb_clocks); i++) {
		struct clk *clk = clockgenb_clocks[i];
		ret |= clk_register(clk);
		clk_enable(clk);
	}

	ret |= clk_register(&comms_clk);
	clk_enable(&comms_clk);

	/* Cut 2 uses clockgen B for module clock so we need to detect chip
	 * type  and use the correct source. Also cut 2 no longer has the
	 * interconnect clock so don't register it */

	if (cpu_data->cut_major > 1) {
		ret |= clk_register(&new_module_clk);
		clk_enable(&new_module_clk);
	}

	for (i = 0; i < ARRAY_SIZE(clkB_pllclks); ++i) {
		clk_set_rate(&clkB_pllclks[i], clk_get_rate(&clkB_pllclks[i]));
		clk_put(&clkB_pllclks[i]);
	}

	return ret;
}
