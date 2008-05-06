/*
 * Copyright (C) 2008 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STx7105.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stm/sysconf.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>

/* Values for mb618 */
#define SYSACLKIN	30000000
#define SYSBCLKIN	30000000
#define SYSAALTCLKIN	0

#define CLOCKGENA_BASE_ADDR	0xfe213000	/* Clockgen A */
#define CLOCKGENB_BASE_ADDR	0xfe000000	/* Clockgen B */

/* Definitions taken from targetpack sti7105_clockgena_regs.xml */
#define CKGA_PLL0_CFG			0x000
#define CKGA_PLL1_CFG			0x004
#define CKGA_POWER_CFG			0x010
#define CKGA_CLKOPSRC_SWITCH_CFG(x)	(0x014+((x)*0x10))
#define CKGA_CLKOBS_MUX1_CFG		0x030
#define CKGA_CLKOBS_MUX2_CFG		0x048
/* All the following appear to be offsets into clkgen B, despite the name */
#define CKGA_OSC_DIV_CFG(x)		(0x800+((x)*4))
#define CKGA_PLL0HS_DIV_CFG(x)		(0x900+((x)*4))
#define CKGA_PLL0LS_DIV_CFG(x)		(0xa10+(((x)-4)*4))
#define CKGA_PLL1_DIV_CFG(x)		(0xb00+((x)*4))

/* Definitions taken from targetpack sti7105_clockgenb_regs.xml */
#define CLOCKGENB_FS0_CTRL		0x14
#define CLOCKGENB_FS0_MD1		0x18
#define CLOCKGENB_FS0_PE1		0x1c
#define CLOCKGENB_FS0_EN_PRG1		0x20
#define CLOCKGENB_FS0_SDIV1		0x24
#define CLOCKGENB_FS0_MD2		0x28
#define CLOCKGENB_FS0_PE2		0x2c
#define CLOCKGENB_FS0_EN_PRG2		0x30
#define CLOCKGENB_FS0_SDIV2		0x34
#define CLOCKGENB_FS0_MD3		0x38
#define CLOCKGENB_FS0_PE3		0x3c
#define CLOCKGENB_FS0_EN_PRG3		0x40
#define CLOCKGENB_FS0_SDIV3		0x44
#define CLOCKGENB_FS0_CLOCKOUT_CTRL	0x58
#define CLOCKGENB_FS1_CTRL		0x5c
#define CLOCKGENB_FS1_MD1		0x60
#define CLOCKGENB_FS1_PE1		0x64
#define CLOCKGENB_FS1_EN_PRG1		0x68
#define CLOCKGENB_FS1_SDIV1		0x6c
#define CLOCKGENB_FS1_MD2		0x70
#define CLOCKGENB_FS1_PE2		0x74
#define CLOCKGENB_FS1_EN_PRG2		0x78
#define CLOCKGENB_FS1_SDIV2		0x7c
#define CLOCKGENB_FS1_MD3		0x80
#define CLOCKGENB_FS1_PE3		0x84
#define CLOCKGENB_FS1_EN_PRG3		0x88
#define CLOCKGENB_FS1_SDIV3		0x8c
#define CLOCKGENB_FS1_MD4		0x90
#define CLOCKGENB_FS1_PE4		0x94
#define CLOCKGENB_FS1_EN_PRG4		0x98
#define CLOCKGENB_FS1_SDIV4		0x9c
#define CLOCKGENB_FS1_CLOCKOUT_CTRL	0xa0
#define CLOCKGENB_DISPLAY_CFG		0xa4
#define CLOCKGENB_FS_SELECT		0xa8
#define CLOCKGENB_POWER_DOWN		0xac
#define CLOCKGENB_POWER_ENABLE		0xb0
#define CLOCKGENB_OUT_CTRL		0xb4
#define CLOCKGENB_CRISTAL_SEL		0xb8


#if 0
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
#define CLOCKGENB_OUT_MUX_CFG	(CLOCKGENB_BASE_ADDR + 0x48)
#define   CLOCKGENB_OUT_MUX_CFG_DIV_SRC		(1<<0)
#define CLOCKGENB_DIV2_CFG	(CLOCKGENB_BASE_ADDR + 0x50)

#endif

static unsigned long clkin[4] = {
	SYSACLKIN,	/* clk_osc_a */
	SYSBCLKIN,	/* clk_osc_b */
	SYSAALTCLKIN,	/* clk_osc_c */
	0		/* clk_osc_d */
};

static struct sysconf_field *clkgena_clkosc_sel_sc;

static void __iomem *clkgena_base, *clkgenb_base;

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

struct pllclk
{
	struct clk clk;
	unsigned long pll_num;
};

static void pll_clk_recalc(struct clk *clk)
{
	struct pllclk *pllclk = container_of(clk, struct pllclk, clk);

	clk->rate = clkgena_pll_freq(clk->parent->rate, pllclk->pll_num);
}

static struct clk_ops pll_clk_ops = {
	.recalc		= pll_clk_recalc,
};

static struct pllclk pllclks[2] = {
	{
		.clk = {
			.name		= "clkgena_pll0_clk",
			.parent		= &clkgena_clk_osc,
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &pll_clk_ops,
		},
		.pll_num = 0
	}, {
		.clk = {
			.name		= "clkgena_pll1_clk",
			.parent		= &clkgena_clk_osc,
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &pll_clk_ops,
		},
		.pll_num = 1
	}
};

/* Clkgen A clocks --------------------------------------------------------- */

struct clkgenaclk
{
	struct clk clk;
	unsigned long num;
};

static void clkgena_clk_init(struct clk *clk)
{
	struct clkgenaclk *clkgenaclk = container_of(clk, struct clkgenaclk, clk);
	unsigned long num = clkgenaclk->num;
	unsigned long data;
	unsigned long src_sel;

	data = readl(clkgena_base + CKGA_CLKOPSRC_SWITCH_CFG(num >> 4));
	src_sel = (data >> ((num & 0xf) * 2)) & 3;

	switch (src_sel) {
	case 0:
		clk->parent = &clkgena_clk_osc;
		break;
	case 1:
		clk->parent = &pllclks[0].clk;
		break;
	case 2:
		clk->parent = &pllclks[1].clk;
		break;
	case 3:
		/* clock is stopped */
		clk->parent = NULL;
		break;
	}
}

static void clkgena_clk_recalc(struct clk *clk)
{
	struct clkgenaclk *clkgenaclk = container_of(clk, struct clkgenaclk, clk);
	unsigned long num = clkgenaclk->num;
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

static struct clk_ops clkgena_clk_ops = {
	.init		= clkgena_clk_init,
	.recalc		= clkgena_clk_recalc,
};

#define CLKGENA_CLK(_num, _name)				\
	{							\
		.clk = {					\
			.name		= _name,		\
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,	\
			.ops		= &clkgena_clk_ops,	\
		},						\
		.num = _num,					\
	 }

static struct clkgenaclk clkgenaclks[] = {
	CLKGENA_CLK(0, "ic_STNOC"),
	CLKGENA_CLK(1, "fdma0"),
	CLKGENA_CLK(2, "fdma1"),
	/* 3 not used */
	CLKGENA_CLK(4, "sh4_clk"),
	CLKGENA_CLK(5, "ic_if_100"),
	CLKGENA_CLK(6, "lx_dmu_cpu"),
	CLKGENA_CLK(7, "lx_aud_cpu"),
	CLKGENA_CLK(8, "ic_bdisp_200"),
	CLKGENA_CLK(9, "ic_disp_200"),
	CLKGENA_CLK(10, "ic_ts_200"),
	CLKGENA_CLK(11, "disp_pipe_200"),
	CLKGENA_CLK(12, "blit_proc"),	/* Note duplicate clock 12 */
	CLKGENA_CLK(12, "ic_delta_200"),/* Note duplicate clock 12 */
	CLKGENA_CLK(13, "ethernet_phy"),
	CLKGENA_CLK(14, "pci"),
	CLKGENA_CLK(15, "emi_master"),
	CLKGENA_CLK(16, "ic_compo_200"),
	CLKGENA_CLK(17, "ic_if_200"),
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
	.parent		= &clkgenaclks[4].clk, /* ic_if_100 */
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &generic_clk_ops,
};

static struct clk generic_comms_clk = {
	.name		= "comms_clk",
	.parent		= &clkgenaclks[4].clk, /* ic_if_100 */
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &generic_clk_ops,
};

int __init clk_init(void)
{
	int i, ret;

	/* Clockgen A */

	clkgena_clkosc_sel_sc = sysconf_claim(SYS_STA, 1, 0, 1, "clkgena");
	clkgena_base = ioremap(CLOCKGENA_BASE_ADDR, 0x50);
	clkgenb_base = ioremap(CLOCKGENB_BASE_ADDR, 0xc00);

	ret = clk_register(&clkgena_clk_osc);
	clk_enable(&clkgena_clk_osc);

	for (i=0; i<2; i++) {
		ret |= clk_register(&pllclks[i].clk);
		clk_enable(&pllclks[i].clk);
	}

	for (i=0; i<ARRAY_SIZE(clkgenaclks); i++) {
		ret |= clk_register(&clkgenaclks[i].clk);
		clk_enable(&clkgenaclks[i].clk);
	}

	ret = clk_register(&generic_module_clk);
	clk_enable(&generic_module_clk);
	ret = clk_register(&generic_comms_clk);
	clk_enable(&generic_comms_clk);

	/* Propagate the clk osc value down */
	clk_set_rate(&clkgena_clk_osc, clk_get_rate(&clkgena_clk_osc));
	clk_put(&clkgena_clk_osc);

	return ret;
}
