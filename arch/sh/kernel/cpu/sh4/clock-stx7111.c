/*
 * Copyright (C) 2008 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STx7111.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stm/sysconf.h>
#include <linux/pm.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>

#include "./clock-common.h"
#include "./soc-stx7111.h"

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
#include <linux/stm/pio.h>
#define dgb_print(fmt, args...)		\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

#define CLOCKGENB_PLL_MULTIPLIER		8
#define CLOCKGENC_PLL_MULTIPLIER		8
/* Definitions taken from targetpack sti7111_clockgenb_regs.xml */
#define CLOCKGENB_LOCK			0x10
#define CLOCKGENB_FS0_CTRL		0x14
#define CLOCKGENB_FS1_CTRL		0x5c
#define CLOCKGENB_FS0_CLOCKOUT_CTRL	0x58
#define CLOCKGENB_FS1_CLOCKOUT_CTRL	0xa0
#define CLKGENB_FSx_BASE(_x)		(CLOCKGENB_BASE_ADDR + 0x18 + \
					0x10*(_x) + (((_x)/4) * 0x8))

/* CLKGENC_FSx_BASE not tested */
#define CLKGENC_FSx_BASE(_x)		(CLOCKGENC_BASE_ADDR + 0x10*((_x)+1))

#define fsclkB_lock()			iowrite32(0x0, CLOCKGENB_LOCK + \
						CLOCKGENB_BASE_ADDR)

#define fsclkB_unlock()			iowrite32(0xc0de, CLOCKGENB_LOCK + \
						CLOCKGENB_BASE_ADDR)

#define fsclk_store32(_fs, _offset, _value)   iowrite32((_value), (_offset) + \
						((_fs)->cfg_addr))
#define fsclk_load32(_fs, _offset)	ioread32((_offset) + (_fs)->cfg_addr)

#define CLOCKGENB_DISPLAY_CFG		0xa4
#define CLOCKGENB_FS_SELECT		0xa8
#define CLOCKGENB_POWER_DOWN		0xac
#define CLOCKGENB_POWER_ENABLE		0xb0
#define CLOCKGENB_OUT_CTRL		0xb4
#define CLOCKGENB_CRISTAL_SEL		0xb8


static unsigned long clkin[4] = {
	SYSACLKIN,	/* clk_osc_a */
	SYSBCLKIN,	/* clk_osc_b */
	SYSAALTCLKIN,	/* clk_osc_c */
	0		/* clk_osc_d */
};

static struct sysconf_field *clkgena_clkosc_sel_sc;

static void __iomem *clkgena_base;

#if 0

				    /* 0  1  2  3  4  5  6     7  */
static const unsigned int ratio1[] = { 1, 2, 3, 4, 6, 8, 1024, 1 };

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

#endif


/* Clkgen A clk_osc -------------------------------------------------------- */
static void clkgena_clk_osc_init(struct clk *clk)
{
	clk->rate = clkin[sysconf_read(clkgena_clkosc_sel_sc)];
}

static struct clk_ops clkgena_clk_osc_ops = {
	.init		= clkgena_clk_osc_init,
};

static struct clk clkgena_clk_osc = {
	.name		= "clkgena_clk_osc",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &clkgena_clk_osc_ops,
};

/* Clkgen A PLLs ----------------------------------------------------------- */

static unsigned long pll800_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, pdiv, mdiv;

	mdiv = (cfg >>  0) & 0xff;
	ndiv = (cfg >>  8) & 0xff;
	pdiv = (cfg >> 16) & 0x7;
	freq = (((2 * (input / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static unsigned long pll1600_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, mdiv;

	mdiv = (cfg >>  0) & 0x7;
	ndiv = (cfg >>  8) & 0xff;
	freq = (((input / 1000) * ndiv) / mdiv) * 1000;

	return freq;
}

static unsigned long clkgena_pll_freq(unsigned long clk_osc, int pll_num)
{
	unsigned long data;

	switch (pll_num) {
	case 0:
		data = readl(clkgena_base + CKGA_PLL0_CFG);
		return pll1600_freq(clk_osc, data);
	case 1:
		data = readl(clkgena_base + CKGA_PLL1_CFG);
		return pll800_freq(clk_osc, data);
	}

	return 0;
}

static void pll_clk_recalc(struct clk *clk)
{
	unsigned long pll_num = (unsigned long) clk->private_data;

	clk->rate = clkgena_pll_freq(clk->parent->rate, pll_num);
}

static struct clk_ops pll_clk_ops = {
	.recalc		= pll_clk_recalc,
};

static struct clk pllclks[2] = {
	{
		.name		= "clkgena_pll0_clk",
		.parent		= &clkgena_clk_osc,
		.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
		.ops		= &pll_clk_ops,
		.private_data	= (void *)0,
	},
	{
		.name		= "clkgena_pll1_clk",
		.parent		= &clkgena_clk_osc,
		.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
		.ops		= &pll_clk_ops,
		.private_data	= (void *)1,
	}
};

/* Clkgen A clocks --------------------------------------------------------- */
enum clockgenA_ID {
	IC_STNOC_ID     = 0,
	FDMA0_ID,
	FDMA1_ID,
	SH4_CLK_ID,
	IC_IF_100_ID,
	LX_DMU_ID,
	LD_AUD_ID,
	IC_BDISP_200_ID,
	IC_DISP_200_ID,
	IC_TS_200_ID,
	DISP_PIPE_200_ID,
	BLIT_PROC_ID,
	ID_DELTA_200_ID,
	ETH_PHY_ID,
	PCI_ID,
	EMI_ID,
	IC_COMPO_200_ID,
	IC_IF_200_ID
};

struct clkgen_a {
	int shift;
};


static void clkgena_clk_recalc(struct clk *clk)
{
	struct clkgen_a *clkgen = (struct clkgen_a *)clk->private_data;
	unsigned long num = clkgen->shift;
	unsigned long data;
	unsigned long src_sel;
	unsigned long div_cfg = 0;
	unsigned long ratio;

	data = readl(clkgena_base + CKGA_CLKOPSRC_SWITCH_CFG(num >> 4));
	src_sel = (data >> ((num & 0xf) * 2)) & 3;

	switch (src_sel) {
	case 0:
		div_cfg = readl(clkgena_base + CKGA_OSC_DIV_CFG(num));
		break;
	case 1:
		div_cfg = readl(clkgena_base +
				((num <= 3) ? CKGA_PLL0HS_DIV_CFG(num) :
					      CKGA_PLL0LS_DIV_CFG(num)));
		break;
	case 2:
		div_cfg = readl(clkgena_base + CKGA_PLL1_DIV_CFG(num));
		break;
	case 3:
		clk->rate = 0;
		return;
	}

	if (div_cfg & 0x10000)
		ratio = 1;
	else
		ratio = (div_cfg & 0x1F) + 1;

	clk->rate = clk->parent->rate / ratio;
}

static void clkgena_clk_init(struct clk *clk)
{
	struct clkgen_a *clkgen = (struct clkgen_a *)clk->private_data;
	unsigned long num = clkgen->shift;
	unsigned long data;
	unsigned long src_sel;

	data = readl(clkgena_base + CKGA_CLKOPSRC_SWITCH_CFG(num >> 4));
	src_sel = (data >> ((num & 0xf) * 2)) & 3;

	switch (src_sel) {
	case 0:
		clk->parent = &clkgena_clk_osc;
		break;
	case 1:
		clk->parent = &pllclks[0];
		break;
	case 2:
		clk->parent = &pllclks[1];
		break;
	case 3:
		/* clock is stopped */
		clk->parent = NULL;
		break;
	}
	clkgena_clk_recalc(clk);
}

static const struct xratio ratios [] = {{1,  0x10000 },
					{2,  0x1 },
					{4,  0x3 },
					{8,  0x7 },
					{16, 0xf },
					{32, 0x1f },
					{NO_MORE_RATIO, }
};

static int clkgena_clk_setrate(struct clk *clk, unsigned long value)
{
	struct clkgen_a *clkgen = (struct clkgen_a *)clk->private_data;
	unsigned long num = clkgen->shift;
	unsigned long data;
	unsigned long src_sel;
	int idx;

	switch (num) {
	case SH4_CLK_ID: return -1;/* the cpu clock managed via cpufreq-API */
	}

	idx = get_xratio_field(value, clk->parent->rate, ratios);
	if (idx == NO_MORE_RATIO) {
		dgb_print("No More Ratios for %d vs %d\n",
			value, clk->parent->rate);
		return -1;
	}
	dgb_print("Using ratio %d\n", ratios[idx].ratio);
	data = readl(clkgena_base + CKGA_CLKOPSRC_SWITCH_CFG(num >> 4));
	src_sel = (data >> ((num & 0xf) * 2)) & 3;
	switch (src_sel) {
	case 0: writel(ratios[idx].field, clkgena_base +
			CKGA_OSC_DIV_CFG(num));
		break;
	case 1: writel(ratios[idx].field, clkgena_base +
		 ((num <= 3) ? CKGA_PLL0HS_DIV_CFG(num) :
			CKGA_PLL0LS_DIV_CFG(num)));
		break;
	case 2: writel(ratios[idx].field, clkgena_base +
			CKGA_PLL1_DIV_CFG(num));
		break;
	case 3:	clk->rate = 0;
		return 0;
	}
	clk->rate = clk->parent->rate / ratios[idx].ratio ;
	return 0;
}

static struct clk_ops clkgena_clk_ops = {
	.init		= clkgena_clk_init,
	.recalc		= clkgena_clk_recalc,
	.set_rate	= clkgena_clk_setrate,
};

#define CLKGENA_CLK(_id, _name, _shift)					\
[_id] = {								\
	.name		= _name,					\
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,	\
	.ops		= &clkgena_clk_ops,				\
	.id		= _id,						\
	.private_data	= (void *) &(struct clkgen_a)			\
		{							\
		.shift = (_shift),					\
		},							\
 }

struct clk clkgena_clks[] = {
	CLKGENA_CLK(IC_STNOC_ID,     "ic_STNOC",      0),
	CLKGENA_CLK(FDMA0_ID,	     "fdma0",         1),
	CLKGENA_CLK(FDMA1_ID, 	     "fdma1",         2),
	CLKGENA_CLK(SH4_CLK_ID,	     "sh4_clk",       4),
	CLKGENA_CLK(IC_IF_100_ID,    "ic_if_100",     5),
	CLKGENA_CLK(LX_DMU_ID,	     "lx_dmu_cpu",    6),
	CLKGENA_CLK(LD_AUD_ID,       "lx_aud_cpu",    7),
	CLKGENA_CLK(IC_BDISP_200_ID, "ic_bdisp_200",  8),
	CLKGENA_CLK(IC_DISP_200_ID,  "ic_disp_200",   9),
	CLKGENA_CLK(IC_TS_200_ID,    "ic_ts_200",    10),
	CLKGENA_CLK(DISP_PIPE_200_ID, "disp_pipe_200", 11),
	CLKGENA_CLK(BLIT_PROC_ID,    "blit_proc",    12),/* duplicate clk 12 */
	CLKGENA_CLK(ID_DELTA_200_ID, "ic_delta_200", 12),/* duplicate clk 12 */
	CLKGENA_CLK(ETH_PHY_ID,      "ethernet_phy", 13),
	CLKGENA_CLK(PCI_ID,	     "pci",          14),
	CLKGENA_CLK(EMI_ID, 	     "emi_master",   15),
	CLKGENA_CLK(IC_COMPO_200_ID, "ic_compo_200", 16),
	CLKGENA_CLK(IC_IF_200_ID,    "ic_if_200",    17),
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
	.parent		= &clkgena_clks[IC_IF_100_ID], /* ic_if_100 */
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &generic_clk_ops,
};

static struct clk generic_comms_clk = {
	.name		= "comms_clk",
	.parent		= &clkgena_clks[IC_IF_100_ID], /* ic_if_100 */
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &generic_clk_ops,
};


#ifdef CONFIG_PM
int clk_pm_state(pm_message_t state)
{
	static int prev_state = PM_EVENT_ON;
	int i;
	switch (state.event) {
	case PM_EVENT_ON:
	if (prev_state == PM_EVENT_FREEZE) {
	/* osc */
	clkgena_clk_osc_init(&clkgena_clk_osc);
	/* PLLs */
	for (i = 0; i< ARRAY_SIZE(pllclks); ++i)
		pll_clk_recalc(&pllclks[i]);
	/* Clocn Gen A */
	for (i = 0; i< ARRAY_SIZE(clkgena_clks); ++i) {
		clkgena_clk_init(&clkgena_clks[i]);
		if (clkgena_clk_setrate(&clkgena_clks[i],
			clkgena_clks[i].rate) < 0 )
				clkgena_clk_recalc(&clkgena_clks[i]);
		}
	}

	case PM_EVENT_SUSPEND:
	case PM_EVENT_FREEZE:
		prev_state = state.event;
		break;
	}
	return 0;
}
#endif

int __init clk_init(void)
{
	int i, ret;

	clkgena_clkosc_sel_sc = sysconf_claim(SYS_STA, 1, 0, 1, "clkgena");
	clkgena_base = ioremap(CLOCKGENA_BASE_ADDR, 0x50);

	ret = clk_register(&clkgena_clk_osc);
	clk_enable(&clkgena_clk_osc);

	/* Clockgen A */
	for (i = 0; i < 2; i++) {
		ret |= clk_register(&pllclks[i]);
		clk_enable(&pllclks[i]);
	}

	for (i = 0; i < ARRAY_SIZE(clkgena_clks); i++) {
		ret |= clk_register(&clkgena_clks[i]);
		clk_enable(&clkgena_clks[i]);
	}

	ret = clk_register(&generic_module_clk);
	clk_enable(&generic_module_clk);
	ret = clk_register(&generic_comms_clk);
	clk_enable(&generic_comms_clk);

	/* Propagate the clk osc value down */
	clk_set_rate(&clkgena_clk_osc, clk_get_rate(&clkgena_clk_osc));
	clk_put(&clkgena_clk_osc);

	/* routes the ic_if_100:2 */
	writel(0xd, clkgena_base + CKGA_CLKOBS_MUX1_CFG);

#ifdef CONFIG_CLK_LDM_LLA_DEBUG
	stpio_request_set_pin(5, 2, "clkB dbg", STPIO_ALT_OUT, 1);
	fsclkB_unlock();
	iowrite32(0x1, CLOCKGENB_OUT_CTRL+CLOCKGENB_BASE_ADDR);
	fsclkB_lock();
#endif
	return ret;
}
