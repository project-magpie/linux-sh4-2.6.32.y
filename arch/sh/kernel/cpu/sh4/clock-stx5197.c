/*
 * Copyright (C) 2008 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STx5197.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stm/sysconf.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/mb704/clocks.h>

#include "./soc-stx5197.h"

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
#include <linux/stm/pio.h>
#define KERN_NULL
#define dbg_print(fmt, args...)		\
		printk(KERN_NULL "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

static void __iomem *ss_base;

/* External XTAL ----------------------------------------------------------- */

static void xtal_init(struct clk *clk)
{
	clk->rate = XTAL;
}

static struct clk_ops xtal_ops = {
	.init		= xtal_init,
};

static struct clk xtal_osc = {
	.name		= "xtal",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &xtal_ops,
	.id		= CLK_XTAL_ID,
};

/* PLLs -------------------------------------------------------------------- */
static unsigned long pll_freq(unsigned long input, int id)
{
	unsigned long config0, config1;
	unsigned long freq, ndiv, pdiv, mdiv;
	int pll_num = id - CLK_PLLA_ID;

	config0 = readl(ss_base + CLK_PLL_CONFIG0(pll_num));
	config1 = readl(ss_base + CLK_PLL_CONFIG1(pll_num));

	if (config1 & CLK_PLL_CONFIG1_POFF)
		return 0;

	mdiv = (config0 >> 0) & 0xff;
	ndiv = (config0 >> 8) & 0xff;
	pdiv = (config1 >> 0) & 0x7;

	freq = (((2 * (input / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static void pll_clk_recalc(struct clk *clk)
{
	clk->rate = pll_freq(clk->parent->rate, clk->id);
}

static struct clk_ops pll_clk_ops = {
	.recalc		= pll_clk_recalc,
};

static struct clk pllclks[2] = {
{
	.name		= "PLLA",
	.parent		= &xtal_osc,
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll_clk_ops,
	.id		= CLK_PLLA_ID,
},
{
	.name		= "PLLB",
	.parent		= &xtal_osc,
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll_clk_ops,
	.id		= CLK_PLLB_ID,
}};

/* Divided PLL clocks ------------------------------------------------------ */

/*
 * The divider is implemented as a variable length shift register
 * preloaded with a bit sequence, which is clocked by the input clock,
 * plus some additional combinatorial logic. Rather than try and work
 * out what this represents, we simply use a look up table with the
 * recommended values.
 *
 * Table bits:
 * 25     half_not_odd
 * 24     even
 * 23:20  depth[3:0]
 * 19: 0 clkdiv_seq[19:0]
 */

#define FRAC(whole, half) .ratio2 = (((whole)*2) + (half ? 1 : 0))
#define DIVIDER(depth, seq, hno, even)	\
	((hno << 25) | (even << 24) | (depth << 20) | (seq << 0))

#define COMBINE_DIVIDER(depth, seq, hno, even)		\
	.value = DIVIDER(depth, seq, hno, even),	\
	.cfg_0 = (seq & 0xffff),			\
	.cfg_1 = (seq >> 16),				\
	.cfg_2 = (depth | (even << 5) | (hno << 6) )

static const struct {
	unsigned long ratio2, value;
	unsigned short cfg_0;
	unsigned char cfg_1, cfg_2;
} divide_table[] = {
	{ FRAC(2 , 0), COMBINE_DIVIDER(0x01, 0x00AAA, 0x1, 0x1) },
	{ FRAC(2 , 5), COMBINE_DIVIDER(0x04, 0x05AD6, 0x1, 0x0) },
	{ FRAC(3 , 0), COMBINE_DIVIDER(0x01, 0x00DB6, 0x0, 0x0) },
	{ FRAC(3 , 5), COMBINE_DIVIDER(0x03, 0x0366C, 0x1, 0x0) },
	{ FRAC(4 , 0), COMBINE_DIVIDER(0x05, 0x0CCCC, 0x1, 0x1) },
	{ FRAC(4 , 5), COMBINE_DIVIDER(0x07, 0x3399C, 0x1, 0x0) },
	{ FRAC(5 , 0), COMBINE_DIVIDER(0x04, 0x0739C, 0x0, 0x0) },
	{ FRAC(5 , 5), COMBINE_DIVIDER(0x00, 0x0071C, 0x1, 0x0) },
	{ FRAC(6 , 0), COMBINE_DIVIDER(0x01, 0x00E38, 0x1, 0x1) },
	{ FRAC(6 , 5), COMBINE_DIVIDER(0x02, 0x01C78, 0x1, 0x0) },
	{ FRAC(7 , 0), COMBINE_DIVIDER(0x03, 0x03C78, 0x0, 0x0) },
	{ FRAC(7 , 5), COMBINE_DIVIDER(0x04, 0x07878, 0x1, 0x0) },
	{ FRAC(8 , 0), COMBINE_DIVIDER(0x05, 0x0F0F0, 0x1, 0x1) },
	{ FRAC(8 , 5), COMBINE_DIVIDER(0x06, 0x1E1F0, 0x1, 0x0) },
	{ FRAC(9 , 0), COMBINE_DIVIDER(0x07, 0x3E1F0, 0x0, 0x0) },
	{ FRAC(9 , 5), COMBINE_DIVIDER(0x08, 0x7C1F0, 0x1, 0x0) },
	{ FRAC(10, 0), COMBINE_DIVIDER(0x09, 0xF83E0, 0x1, 0x1) },
	{ FRAC(11, 0), COMBINE_DIVIDER(0x00, 0x007E0, 0x0, 0x0) },
	{ FRAC(12, 0), COMBINE_DIVIDER(0x01, 0x00FC0, 0x1, 0x1) },
	{ FRAC(13, 0), COMBINE_DIVIDER(0x02, 0x01FC0, 0x0, 0x0) },
	{ FRAC(14, 0), COMBINE_DIVIDER(0x03, 0x03F80, 0x1, 0x1) },
	{ FRAC(15, 0), COMBINE_DIVIDER(0x04, 0x07F80, 0x0, 0x0) },
	{ FRAC(16, 0), COMBINE_DIVIDER(0x05, 0x0FF00, 0x1, 0x1) },
	{ FRAC(17, 0), COMBINE_DIVIDER(0x06, 0x1FF00, 0x0, 0x0) },
	{ FRAC(18, 0), COMBINE_DIVIDER(0x07, 0x3FE00, 0x1, 0x1) },
	{ FRAC(19, 0), COMBINE_DIVIDER(0x08, 0x7FE00, 0x0, 0x0) },
	{ FRAC(20, 0), COMBINE_DIVIDER(0x09, 0xFFC00, 0x1, 0x1) },
#if 0
FMV: Commented because currently in the clk API there is no
 way to ask for 'Semi-synchronous operation'
	/* Semi-synchronous operation */
	{ FRAC(2, 0), COMBINE_DIVIDER(0x01, 0x00555, 0x1, 0x1) },
	{ FRAC(4, 0), COMBINE_DIVIDER(0x05, 0x03333, 0x1, 0x1) },
	{ FRAC(6, 0), COMBINE_DIVIDER(0x01, 0x001C7, 0x1, 0x1) },
#endif
};

static unsigned long divider_freq(unsigned long input, int div_num)
{
	int offset;
	unsigned long config0, config1, config2;
	unsigned long seq, depth, hno, even;
	unsigned long combined;
	int i;

	switch (div_num) {
	case 0:
		offset = CLKDIV0_CONFIG0;
		break;
	case 1 ... 4:
		offset = CLKDIV1_4_CONFIG0(div_num);
		break;
	case 6 ... 10:
		offset = CLKDIV6_10_CONFIG0(div_num);
		break;
	default:
		BUG();
		return 0;
	}

	config0 = readl(ss_base + offset + 0x0);
	config1 = readl(ss_base + offset + 0x4);
	config2 = readl(ss_base + offset + 0x8);

	seq = (config0 & 0xffff) | ((config1 & 0xf) << 16);
	depth = config2 & 0xf;
	hno = (config2 & (1<<6)) ? 1 : 0;
	even = (config2 & (1<<5)) ? 1 : 0;
	combined = DIVIDER(depth, seq, hno, even);

	for (i = 0; i < ARRAY_SIZE(divide_table); i++) {
		if (divide_table[i].value == combined)
			return (input*2)/divide_table[i].ratio2;
	}

	printk(KERN_DEBUG "Unrecognised value in divide table %lx\n", combined);

	return 0;
}

static void dividedpll_clk_init(struct clk *clk)
{
	unsigned long num = clk->id - CLK_DDR_ID;
	unsigned long data;

	data = readl(ss_base + CLK_PLL_SELECT_CFG);
	clk->parent = &pllclks[(data & (1<<(num+1))) ? 1 : 0];
}

static void dividedpll_hw_set(unsigned long addr,
		unsigned long cfg0, unsigned long cfg1,
		unsigned long cfg2)
{
	unsigned long flag;

	addr += ss_base;

	local_irq_save(flag);
	writel(0xf0, ss_base + CLK_LOCK_CFG);
	writel(0x0f, ss_base + CLK_LOCK_CFG); /* UnLock */

/*
 * On the 5197 platform it's mandatory change the clock setting with an
 * asm code because in X1 mode all the clocks are routed on Xtal
 * and it could be dangerous a memory access
 *
 * All the code is self-contained in a single icache line
 */
        asm volatile (".balign  32      \n"
		      "mov.l    %5, @%4 \n" /* in X1 mode */
		      "mov.l    %1, @(0,%0)\n" /* set     */
		      "mov.l    %2, @(4,%0)\n" /*  the    */
		      "mov.l    %3, @(8,%0)\n" /*   ratio */
		      "mov.l    %6, @%4 \n" /* in Prog mode */

		      "tst	%7, %7	\n" /* a delay to wait stable signal */
		      "2:		\n"
		      "bf/s	2b	\n"
		      " dt	%7	\n"
		::    "r" (addr),
		      "r" (cfg0),
		      "r" (cfg1),
		      "r" (cfg2), /* with enable */
		      "r" (ss_base + CLK_MODE_CTRL),
		      "r" (CLK_MODE_CTRL_X1),
		      "r" (CLK_MODE_CTRL_PROG),
		      "r" (1000000)
		:     "memory");
	writel(0x100, ss_base + CLK_LOCK_CFG); /* UnLock */
	local_irq_restore(flag);
}

static int dividedpll_clk_XXable(struct clk *clk, int enable)
{
	unsigned long num = clk->id-CLK_DDR_ID;
	unsigned long offset = CLKDIV_CONF0(num);
	unsigned long flag;
	unsigned long reg_cfg0, reg_cfg1, reg_cfg2;

	dbg_print("\n");
	reg_cfg0 = readl(offset + ss_base);
	reg_cfg1 = readl(offset + ss_base + 4);
	reg_cfg2 = readl(offset + ss_base + 8);

	if (enable)
		reg_cfg2 |= (1<<4);
	else
		reg_cfg2 &= ~(1<<4);

	dividedpll_hw_set(offset, reg_cfg0, reg_cfg1, reg_cfg2);

	clk->rate = (enable ? divider_freq(clk->parent->rate, num) : 0);
	return 0;
}

static int dividedpll_clk_disable(struct clk *clk)
{
	dbg_print("\n");
	return dividedpll_clk_XXable(clk, 0);
}

static int dividedpll_clk_enable(struct clk *clk)
{
	dbg_print("\n");
	return dividedpll_clk_XXable(clk, 1);
}

static int dividedpll_clk_set_rate(struct clk *clk, unsigned long rate)
{
	int i;
	unsigned long offset = CLKDIV_CONF0(clk->id - CLK_DDR_ID);
	unsigned long flag;

	for (i = 0; i < ARRAY_SIZE(divide_table); i++)
		if ((clk_get_rate(clk->parent)*2) / divide_table[i].ratio2 == rate)
			break;

	if (i == ARRAY_SIZE(divide_table)) /* not found! */
		return -EINVAL;

	dbg_print("clock: %s from %uMHz to %u MHz\n", clk->name,
			clk->rate, rate);
	dbg_print("offset = 0x%x divider = %d\n", offset, divide_table[i].ratio2/2);

	dividedpll_hw_set(offset, divide_table[i].cfg_0,
		divide_table[i].cfg_1, divide_table[i].cfg_2 | (1<<4));

	clk->rate = rate;
	return 0;
}

static void dividedpll_clk_recalc(struct clk *clk)
{
	unsigned long num = clk->id - CLK_DDR_ID;

	clk->rate = divider_freq(clk->parent->rate, num);
}

static struct clk_ops dividedpll_clk_ops = {
	.init		= dividedpll_clk_init,
	.recalc		= dividedpll_clk_recalc,
	.enable		= dividedpll_clk_enable,
	.disable	= dividedpll_clk_disable,
	.set_rate	= dividedpll_clk_set_rate,
};

#define DIVIDEDPLL_CLK(_num, _name)					\
{									\
		.name	= _name,					\
		.flags	= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,	\
		.ops	= &dividedpll_clk_ops,				\
		.id	= _num,						\
}

struct clk dividedpll_clks[] = {
	DIVIDEDPLL_CLK(CLK_DDR_ID, "ddr"), /* or spare? */
	DIVIDEDPLL_CLK(CLK_LMI_ID, "lmi"),
	DIVIDEDPLL_CLK(CLK_BLT_ID, "blt"),
	DIVIDEDPLL_CLK(CLK_SYS_ID, "sys"),
	DIVIDEDPLL_CLK(CLK_FDMA_ID, "fdma"), /* can also be a freq synth */
	/* 5: DDR */
/*	More probably the DDR clk is that!...
 *	because it seems compliant with the CLK_PLL_SELECT_CFG
 *	value! (routed from PLLxB)
 */
	DIVIDEDPLL_CLK(CLK_AV_ID, "av"),
	/* 7: Spare */
	DIVIDEDPLL_CLK(CLK_ETH_ID, "eth"),
	DIVIDEDPLL_CLK(CLK_ST40_ID, "sh4_clk"),
	DIVIDEDPLL_CLK(CLK_ST40P_ID, "st40_pck"),
};

/* SH4 generic clocks ------------------------------------------------------ */

static void generic_clk_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
}

static struct clk_ops generic_clk_ops = {
	.recalc		= generic_clk_recalc,
};

static struct clk generic_module_clk = {
	.name		= "module_clk",
	.parent		= &dividedpll_clks[8], /* st40_pck */
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &generic_clk_ops,
};

static struct clk generic_comms_clk = {
	.name		= "comms_clk",
	.parent		= &dividedpll_clks[3], /* clk_sys */
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &generic_clk_ops,
};

int __init clk_init(void)
{
	int i, ret;

	ss_base = ioremap(SYS_SERV_BASE_ADDR, 1024);
	if (! ss_base)
		panic("Unable to remap system services");

	ret = clk_register(&xtal_osc);
	clk_enable(&xtal_osc);

	for (i = 0; i < 2; i++) {
		ret |= clk_register(&pllclks[i]);
		clk_enable(&pllclks[i]);
	}

	for (i = 0; i < ARRAY_SIZE(dividedpll_clks); i++) {
		ret |= clk_register(&dividedpll_clks[i]);
		clk_enable(&dividedpll_clks[i]);
	}

	ret = clk_register(&generic_module_clk);
	clk_enable(&generic_module_clk);
	ret = clk_register(&generic_comms_clk);
	clk_enable(&generic_comms_clk);


	/* Propagate the clk osc value down */
	clk_set_rate(&xtal_osc, clk_get_rate(&xtal_osc));
	clk_put(&xtal_osc);

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
	CLK_UNLOCK();
	writel(0x23, CLK_OBSERVE + SYS_SERV_BASE_ADDR);
	CLK_LOCK();
#endif

	return ret;
}
