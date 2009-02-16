/*
 * Copyright (C) 2005 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STb7100.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>
#include <asm-generic/div64.h>

#include "./clock-common.h"
#include "./soc-stb7100.h"

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
#include <linux/stm/pio.h>
#define dgb_print(fmt, args...)  	\
	printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

void __iomem *clkgena_base;

#define CLOCKGEN_PLL0_CFG	0x08
#define CLOCKGEN_PLL0_CLK1_CTRL	0x14
#define CLOCKGEN_PLL0_CLK2_CTRL	0x18
#define CLOCKGEN_PLL0_CLK3_CTRL	0x1c
#define CLOCKGEN_PLL0_CLK4_CTRL	0x20
#define CLOCKGEN_PLL1_CFG	0x24

/* to enable/disable and reduce the coprocessor clock*/
#define CLOCKGEN_CLK_DIV	0x30
#define CLOCKGEN_CLK_EN		0x34

			       /* 0  1  2  3  4  5  6  7  */
static unsigned char ratio1[] = { 1, 2, 3, 4, 6, 8 , NO_MORE_RATIO};
static unsigned char ratio2[] = { 1, 2, 3, 4, 6, 8 , NO_MORE_RATIO};
static unsigned char ratio3[] = { 4, 2, 4, 4, 6, 8 , NO_MORE_RATIO};
static unsigned char ratio4[] = { 1, 2, 3, 4, 6, 8 , NO_MORE_RATIO};

static int pll_freq(unsigned long addr)
{
	unsigned long freq, data, ndiv, pdiv, mdiv;

	data = readl(clkgena_base+addr);
	mdiv = (data >>  0) & 0xff;
	ndiv = (data >>  8) & 0xff;
	pdiv = (data >> 16) & 0x7;
	freq = (((2 * (CONFIG_SH_EXTERNAL_CLOCK / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static void pll0_clk_init(struct clk *clk)
{
	clk->rate = pll_freq(CLOCKGEN_PLL0_CFG);
}

static struct clk_ops pll0_clk_ops = {
	.init = pll0_clk_init,
};

static struct clk pll0_clk = {
	.name		= "pll0_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll0_clk_ops,
};

static void pll1_clk_init(struct clk *clk)
{
	clk->rate = pll_freq(CLOCKGEN_PLL1_CFG);
}

static struct clk_ops pll1_clk_ops = {
	.init = pll1_clk_init,
};

static struct clk pll1_clk = {
	.name		= "pll1_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll1_clk_ops,
};

struct clokgenA
{
	unsigned long ctrl_reg;
	unsigned int div;
	unsigned char *ratio;
};


enum clockgenA_ID {
	SH4_CLK_ID = 0,
	SH4IC_CLK_ID,
	MODULE_ID,
	SLIM_ID,
	LX_AUD_ID,
	LX_VID_ID,
	LMISYS_ID,
	LMIVID_ID,
	IC_ID,
	IC_100_ID,
	EMI_ID
};

static void clockgenA_clk_recalc(struct clk *clk)
{
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;
	clk->rate = clk->parent->rate / cga->div;
	return;
}

static int clockgenA_clk_set_rate(struct clk *clk, unsigned long value)
{
	unsigned long data = readl(clkgena_base + CLOCKGEN_CLK_DIV);
	unsigned long val = 1 << (clk->id -5);

	if (clk->id != LMISYS_ID && clk->id != LMIVID_ID)
		return -1;
	writel(0xc0de, clkgena_base);
	if (clk->rate > value) {/* downscale */
		writel(data | val, clkgena_base + CLOCKGEN_CLK_DIV);
		clk->rate /= 1024;
	} else {/* upscale */
		writel(data & ~val, clkgena_base + CLOCKGEN_CLK_DIV);
		clk->rate *= 1024;
	}
	writel(0x0, clkgena_base);
	return 0;
}

static void clockgenA_clk_init(struct clk *clk)
{
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;
	if (cga->ratio) {
		unsigned long data = readl(clkgena_base + cga->ctrl_reg) & 0x7;
		cga->div = 2*cga->ratio[data];
	}
	clk->rate = clk->parent->rate / cga->div;
}

static void clockgenA_clk_XXable(struct clk *clk, int enable)
{
	unsigned long tmp, value;
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;

	if (clk->id != LMISYS_ID && clk->id != LMIVID_ID)
		return ;

	tmp   = readl(clkgena_base+cga->ctrl_reg) ;
	value = 1 << (clk->id -5);
	writel(0xc0de, clkgena_base);
	if (enable) {
		writel(tmp | value, clkgena_base + cga->ctrl_reg);
		clockgenA_clk_init(clk); /* to evaluate the rate */
	} else {
		writel(tmp & ~value, clkgena_base + cga->ctrl_reg);
		clk->rate = 0;
	}
	writel(0x0, clkgena_base);
}
static void clockgenA_clk_enable(struct clk *clk)
{
	clockgenA_clk_XXable(clk, 1);
}

static void clockgenA_clk_disable(struct clk *clk)
{
	clockgenA_clk_XXable(clk, 0);
}

static struct clk_ops clokgenA_ops = {
	.init		= clockgenA_clk_init,
	.recalc		= clockgenA_clk_recalc,
	.set_rate	= clockgenA_clk_set_rate,
	.enable		= clockgenA_clk_enable,
	.disable	= clockgenA_clk_disable,
};

#define CLKGENA(_id, clock, pll, _ctrl_reg, _div, _ratio)	\
[_id] = {							\
	.name	= #clock "_clk",				\
	.flags	= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,	\
	.parent	= &(pll),					\
	.ops	= &clokgenA_ops,				\
	.id	= (_id),					\
	.private_data = &(struct clokgenA){			\
		.div = (_div),					\
		.ctrl_reg = (_ctrl_reg),			\
		.ratio = (_ratio)				\
		},						\
	}

struct clk clkgena_clks[] = {
CLKGENA(SH4_CLK_ID,	sh4, pll0_clk, CLOCKGEN_PLL0_CLK1_CTRL, 1, ratio1),
CLKGENA(SH4IC_CLK_ID, sh4_ic, pll0_clk, CLOCKGEN_PLL0_CLK2_CTRL, 1, ratio2),
CLKGENA(MODULE_ID,	module,   pll0_clk, CLOCKGEN_PLL0_CLK3_CTRL, 1, ratio3),
CLKGENA(SLIM_ID,	slim,     pll0_clk, CLOCKGEN_PLL0_CLK4_CTRL, 1, ratio4),

CLKGENA(LX_AUD_ID,	st231aud, pll1_clk, CLOCKGEN_CLK_EN, 1, NULL),
CLKGENA(LX_VID_ID,	st231vid, pll1_clk, CLOCKGEN_CLK_EN, 1, NULL),
CLKGENA(LMISYS_ID,	lmisys,   pll1_clk, 0, 1, NULL),
CLKGENA(LMIVID_ID,	lmivid,   pll1_clk, 0, 1, NULL),
CLKGENA(IC_ID,	ic,	  pll1_clk, 0, 2, NULL),
CLKGENA(IC_100_ID,	ic_100,   pll1_clk, 0, 4, NULL),
CLKGENA(EMI_ID,	emi,      pll1_clk, 0, 4, NULL)
};

static void comms_clk_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
}

static struct clk_ops comms_clk_ops = {
	.recalc	= comms_clk_recalc,
};

struct clk comms_clk = {
	.name		= "comms_clk",
	.parent		= &clkgena_clks[IC_100_ID],
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &comms_clk_ops
};

static struct clk *onchip_clocks[] = {
	&pll0_clk,
	&pll1_clk,
};

#ifdef CONFIG_PM
int clk_pm_state(pm_message_t state)
{
	switch (state.event) {
	case PM_EVENT_ON:
		clockgenA_clk_set_rate(&clkgena_clks[LX_AUD_ID],
			clkgena_clks[LX_AUD_ID].rate * 1024);
		clockgenA_clk_set_rate(&clkgena_clks[LX_VID_ID],
			clkgena_clks[LX_VID_ID].rate * 1024);
			/* enables the analog parts for ClockGenB*/
		break;
	case PM_EVENT_SUSPEND:
		clockgenA_clk_set_rate(&clkgena_clks[LX_AUD_ID],
			clkgena_clks[LX_AUD_ID].rate / 1024);
		clockgenA_clk_set_rate(&clkgena_clks[LX_VID_ID],
			clkgena_clks[LX_VID_ID].rate / 1024);
		break;
	case PM_EVENT_FREEZE:
		break;
	}
	return 0;
}
#endif

int __init clk_init(void)
{
	int i, ret = 0;

	/**************/
	/* Clockgen A */
	/**************/
	clkgena_base = ioremap(CLOCKGEN_BASE_ADDR, 0x100);

	for (i = 0; i < ARRAY_SIZE(onchip_clocks); i++) {
		struct clk *clk = onchip_clocks[i];

		ret |= clk_register(clk);
		clk_enable(clk);
	}

	for (i = 0; i < ARRAY_SIZE(clkgena_clks); i++) {
		struct clk *clk = &clkgena_clks[i];
		ret |= clk_register(clk);
		clk_enable(clk);
	}
	clk_register(&comms_clk);
	clk_enable(&comms_clk);
	/* Propogate the PLL values down */
	clk_set_rate(&pll0_clk, clk_get_rate(&pll0_clk));
	clk_put(&pll0_clk);
	clk_set_rate(&pll1_clk, clk_get_rate(&pll1_clk));
	clk_put(&pll1_clk);

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
	iowrite32(0xc0de, clkgena_base);
	iowrite32(13, clkgena_base + 0x38);   /* routed on SYSCLK_OUT */
	iowrite32(0, clkgena_base);
	stpio_request_set_pin(5, 2, "clkB dbg", STPIO_ALT_OUT, 1);
#endif
	return ret;
}
