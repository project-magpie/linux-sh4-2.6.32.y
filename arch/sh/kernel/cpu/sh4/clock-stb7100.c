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
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>

#define CLOCKGEN_BASE_ADDR	0x19213000	/* Clockgen A */

void __iomem *clkgen_base;

#define CLOCKGEN_PLL0_CFG	0x08
#define CLOCKGEN_PLL0_CLK1_CTRL	0x14
#define CLOCKGEN_PLL0_CLK2_CTRL	0x18
#define CLOCKGEN_PLL0_CLK3_CTRL	0x1c
#define CLOCKGEN_PLL0_CLK4_CTRL	0x20
#define CLOCKGEN_PLL1_CFG	0x24

                               /* 0  1  2  3  4  5  6  7  */
static unsigned char ratio1[] = { 1, 2, 3, 4, 6, 8 };
static unsigned char ratio2[] = { 1, 2, 3, 4, 6, 8 };
static unsigned char ratio3[] = { 4, 2, 4, 4, 6, 8 };
static unsigned char ratio4[] = { 1, 2, 3, 4, 6, 8 };

static int pll_freq(unsigned long addr)
{
	unsigned long freq, data, ndiv, pdiv, mdiv;

	data = readl(clkgen_base+addr);
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
	.init		= pll0_clk_init,
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
	.init		= pll1_clk_init,
};

static struct clk pll1_clk = {
	.name		= "pll1_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll1_clk_ops,
};

#define DEFINE_CLKGEN_CLK(clock, pll, div_first, div)		\
static void clock##_clk_recalc(struct clk *clk)			\
{								\
	div_first;						\
	clk->rate = clk->parent->rate / (div);			\
}								\
								\
static struct clk_ops clock##_clk_ops = {			\
	.recalc		= clock##_clk_recalc,			\
};								\
								\
static struct clk clock##_clk = {				\
	.name		= #clock "_clk",				\
	.parent		= &pll,					\
	.flags		= CLK_ALWAYS_ENABLED,			\
	.ops		= &clock##_clk_ops,			\
};

#define DEFINE_CLKGEN_RATIO_CLK(clock, pll, register, ratio)	\
DEFINE_CLKGEN_CLK(clock, pll,					\
		  unsigned long data = readl(clkgen_base+register) & 0x7, 2*ratio[data])

DEFINE_CLKGEN_RATIO_CLK(sh4,    pll0_clk, CLOCKGEN_PLL0_CLK1_CTRL, ratio1)
DEFINE_CLKGEN_RATIO_CLK(sh4_ic, pll0_clk, CLOCKGEN_PLL0_CLK2_CTRL, ratio2)
DEFINE_CLKGEN_RATIO_CLK(module, pll0_clk, CLOCKGEN_PLL0_CLK3_CTRL, ratio3)
DEFINE_CLKGEN_RATIO_CLK(slim,   pll0_clk, CLOCKGEN_PLL0_CLK4_CTRL, ratio4)

DEFINE_CLKGEN_CLK(comms, pll1_clk, , 4)

static struct clk *onchip_clocks[] = {
	&pll0_clk,
	&pll1_clk,
	&sh4_clk,
	&sh4_ic_clk,
	&module_clk,
	&slim_clk,
	&comms_clk,
};

void* clk_get_iomem(void)
{
	return clkgen_base;
}

int __init clk_init(void)
{
	int i, ret = 0;

	clkgen_base = ioremap(CLOCKGEN_BASE_ADDR, 0x100);

	for (i = 0; i < ARRAY_SIZE(onchip_clocks); i++) {
		struct clk *clk = onchip_clocks[i];

		ret |= clk_register(clk);
		clk_enable(clk);
	}

	/* Propogate the PLL values down */
	clk_set_rate(&pll0_clk, clk_get_rate(&pll0_clk));
	clk_put(&pll0_clk);
	clk_set_rate(&pll1_clk, clk_get_rate(&pll1_clk));
	clk_put(&pll1_clk);

	return ret;
}
