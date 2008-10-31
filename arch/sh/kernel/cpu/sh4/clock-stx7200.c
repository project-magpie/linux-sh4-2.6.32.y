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
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>

/* Values for mb519 */
#define SYSACLKIN	27000000
#define SYSBCLKIN	30000000

/* Alternate clock for clockgen A, B and C respectivly */
/* B & C come from SYSCLKINALT pin, SYSCLKINALT2 from PIO2[2] */
unsigned long sysclkinalt[3] = { 0,0,0};

#define CLOCKGEN_BASE_ADDR	0xfd700000	/* Clockgen A */
#define CLOCKGENB_BASE_ADDR	0xfd701000	/* Clockgen B */

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

struct pllclk
{
	struct clk clk;
	unsigned long pll_num;
};

static void pll_clk_init(struct clk *clk)
{
	struct pllclk *pllclk = container_of(clk, struct pllclk, clk);

	clk->rate = pll_freq(pllclk->pll_num);
}

static struct clk_ops pll_clk_ops = {
	.init		= pll_clk_init,
};

static struct pllclk pllclks[3] = {
	{
		.clk = {
			.name		= "pll0_clk",
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &pll_clk_ops,
		},
		.pll_num = 0
	}, {
		.clk = {
			.name		= "pll1_clk",
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &pll_clk_ops,
		},
		.pll_num = 1
	}, {
		.clk = {
			.name		= "pll2_clk",
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &pll_clk_ops,
		},
		.pll_num = 2
	}
};

struct sh4clk
{
	struct clk clk;
	unsigned long shift;
};

/* Note we ignore the possibility that we are in SH4 mode.
 * Should check DIV_CFG.sh4_clk_ctl and switch to FRQCR mode. */
static void sh4_clk_recalc(struct clk *clk)
{
	struct sh4clk *sh4clk = container_of(clk, struct sh4clk, clk);
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
		div2 = ratio1[(div_cfg >> sh4clk->shift) & 7];
	else
		div2 = ratio2[(div_cfg >> sh4clk->shift) & 7];
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

static struct sh4clk sh4clks[3] = {
	{
		.clk = {
			.name		= "sh4_clk",
			.parent		= &pllclks[0].clk,
			/* May propagate to FDMA */
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &sh4_clk_ops,
		},
		.shift = 1
	}, {
		.clk = {
			.name		= "sh4_ic_clk",
			.parent		= &pllclks[0].clk,
			.flags		= CLK_ALWAYS_ENABLED,
			.ops		= &sh4_clk_ops,
		},
		.shift = 4
	}, {
		.clk = {
			.name		= "module_clk",
			.parent		= &pllclks[0].clk,
			.flags		= CLK_ALWAYS_ENABLED,
			.ops		= &sh4_clk_ops,
		},
		.shift = 7
	}
};

struct fdmalxclk {
	struct clk clk;
	char fdma_num;
	char div_cfg_reg;
	char div_cfg_shift;
	char normal_div;
};

static void fdma_clk_init(struct clk *clk)
{
	struct fdmalxclk *fdmaclk = container_of(clk, struct fdmalxclk, clk);
	unsigned long mux_cfg = ctrl_inl(CLOCKGEN_MUX_CFG);

	if ((mux_cfg & CLOCKGEN_MUX_CFG_FDMA_SRC(fdmaclk->fdma_num)) == 0)
		clk->parent = &sh4clks[0].clk;
	else
		clk->parent = &pllclks[1].clk;
}

static void fdmalx_clk_recalc(struct clk *clk)
{
	struct fdmalxclk *fdmalxclk = container_of(clk, struct fdmalxclk, clk);
	unsigned long div_cfg;
	unsigned long div_ratio;
	unsigned long normal_div;

	div_cfg = ctrl_inl(CLOCKGEN_DIV_CFG + fdmalxclk->div_cfg_reg);
	div_ratio = (div_cfg >> fdmalxclk->div_cfg_shift) & 3;
	normal_div = fdmalxclk->normal_div;
	clk->rate = final_divider(clk->parent->rate, div_ratio, normal_div);
}

static struct clk_ops fdma_clk_ops = {
	.init		= fdma_clk_init,
	.recalc		= fdmalx_clk_recalc,
};

static struct clk_ops lx_clk_ops = {
	.recalc		= fdmalx_clk_recalc,
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
	CLKGENA(ic_266, &pllclks[2].clk, ic266_clk_ops, 0),
};

#define CLKGENA_FDMALX(_name, _parent, _ops, _fdma_num, _div_cfg_reg, _div_cfg_shift, _normal_div) \
	{							\
		.clk = {					\
			.name		= #_name,		\
			.parent		= _parent,		\
			.flags		= CLK_ALWAYS_ENABLED,	\
			.ops		= &_ops,		\
		},						\
		.fdma_num = _fdma_num,				\
		.div_cfg_reg = _div_cfg_reg - CLOCKGEN_DIV_CFG,	\
		.div_cfg_shift = _div_cfg_shift,		\
		.normal_div = _normal_div,			\
	}

#define CLKGENA_FDMA(name, num)					\
	CLKGENA_FDMALX(name, NULL, fdma_clk_ops, num,		\
			CLOCKGEN_DIV_CFG, 10, 1)

#define CLKGENA_LX(name, shift)				\
	CLKGENA_FDMALX(name, &pllclks[1].clk, lx_clk_ops, 0,	\
			CLOCKGEN_DIV_CFG, shift, 1)

#define CLKGENA_MISCDIV(name, shift, ratio)		\
	CLKGENA_FDMALX(name, &pllclks[2].clk, lx_clk_ops, 0,	\
			CLOCKGEN_DIV2_CFG, shift, ratio)

static struct fdmalxclk fdmaclks[2] = {
	CLKGENA_FDMA(fdma_clk0, 0),
	CLKGENA_FDMA(fdma_clk1, 1)
};

static struct fdmalxclk lxclks[4] = {
	CLKGENA_LX(lx_aud0_cpu_clk, 12),
	CLKGENA_LX(lx_aud1_cpu_clk, 14),
	CLKGENA_LX(lx_dmu0_cpu_clk, 16),
	CLKGENA_LX(lx_dmu1_cpu_clk, 18)
};

static struct fdmalxclk miscdivclks[4] = {
	CLKGENA_MISCDIV(dmu0_266, 18, 3),
	CLKGENA_MISCDIV(disp_266, 22, 3),
	CLKGENA_MISCDIV(bdisp_200, 6, 4),
	CLKGENA_MISCDIV(fdma_200, 14, 4)
};

static struct clk *clockgena_clocks[] = {
	&pllclks[0].clk,
	&pllclks[1].clk,
	&pllclks[2].clk,
	&sh4clks[0].clk,
	&fdmaclks[0].clk,
	&fdmaclks[1].clk,
	&lxclks[0].clk,
	&lxclks[1].clk,
	&lxclks[2].clk,
	&lxclks[3].clk,
	&miscclks[0],
	&miscdivclks[0].clk,
	&miscdivclks[1].clk,
	&miscdivclks[2].clk,
	&miscdivclks[3].clk,
};

#define CLKGENB_BASE		0xfd701000
#define CLKGENB_FS0_SETUP	(CLKGENB_BASE + 0x00)
#define CLKGENB_FS1_SETUP	(CLKGENB_BASE + 0x04)
#define CLKGENB_FS2_SETUP	(CLKGENB_BASE + 0x08)
#define CLKGENB_FSx_CLKy_CFG(x,y)	\
	(CLKGENB_BASE + ((x)*0x10) + (((y)-1)*4) + 0x00c)

static unsigned long fsynth(unsigned long refclk, signed long md,
			    unsigned long pe, unsigned long sdiv)
{
	  /*
	   * The values in the registers need some 'interpretation'.
	   * Note this is not documented in the 7100 datasheet, only the
	   * 8000 architecture manual (vol 2):
	   *   md: integer value of MD<4:0> range [-16, -1]
	   *   pe: integer value of PE>15:0> range [0, 2^15-1]
	   *   sdiv: value of the output divider as follows:
	   *     SDIV<2:0> = '000' -> sidv = 2
	   *     SDIV<2:0> = '001' -> sidv = 4
	   *     ...
	   *     SDIV<2:0> = '111' -> sidv = 256
	   *
	   *                                2^15 * Fpll
	   * Fout = ------------------------------------------------------
           *                           md                          md+1
	   *        sdiv * [(pe * (1 + -- )) - ((pe - 2^15) * (1 + ---- ))]
           *                           32                           32
	   */

	unsigned long f_pll = refclk * 8;
	signed long part1;
	signed long part2;
	unsigned long long freq;

	md = md - 32;
	sdiv = 2 << sdiv;

	part1 = (pe * (32+md)) / 32;
	part2 = (((signed)pe - (1<<15)) * (32 + (md+1))) / 32;

	freq = ((1ULL << 15) * f_pll) / (sdiv * (part1 - part2));
	return freq;
}

struct fsclk
{
	struct clk clk;
	unsigned long cfg_addr;
	char name[8];
};

static void fs_clk_init(struct clk *clk)
{
	struct fsclk *fsclk = container_of(clk, struct fsclk, clk);
	unsigned long data = ctrl_inl(fsclk->cfg_addr);
	unsigned long pe, sdiv;
	signed long md;

	pe = (data >> 0) & 0xffff;
	md = (data >> 16) & 0x1f;
	sdiv = (data >> 22) & 7;
	clk->rate = fsynth(CONFIG_SH_EXTERNAL_CLOCK, md, pe, sdiv);
}

static struct clk_ops fs_clk_ops = {
	.init		= fs_clk_init,
};

static struct fsclk fsclks[12];


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

static struct clk_ops pll_clkB_ops = {
	.init		= pll_clkB_init,
};

static struct pllclk clkB_pllclks[1] = {
	{
		.clk = {
			.name		= "b_pll0_clk",
			.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
			.ops		= &pll_clkB_ops,
		},
		.pll_num = 0
	}
};

#define CLKGENB(_name, _ops, _flags)				\
	{							\
		.name		= #_name,			\
		.parent		= &clkB_pllclks[0].clk,		\
		.flags		= CLK_ALWAYS_ENABLED | _flags,	\
		.ops		= &_ops				\
	}

#define CLKGENB_DIV2(_name, _div_cfg_shift, _normal_div)	\
	{							\
		.clk = {					\
			.name		= #_name,		\
			.parent		= &clkB_pllclks[0].clk,	\
			.flags		= CLK_ALWAYS_ENABLED,	\
			.ops		= &clkgenb_div2_ops,	\
		},						\
		.div_cfg_shift = _div_cfg_shift,		\
		.normal_div = _normal_div,			\
	}


struct clkgenBdiv2 {
	struct clk clk;
	char   div_cfg_shift;
	char   normal_div;
};

static void clkgenb_div2_recalc(struct clk *clk)
{
	struct clkgenBdiv2 *clkgenBdiv2 = container_of(clk, struct clkgenBdiv2, clk);
	unsigned long div_cfg;
	unsigned long div_ratio;

	div_cfg = ctrl_inl(CLOCKGENB_DIV2_CFG);
	div_ratio = (div_cfg >> clkgenBdiv2->div_cfg_shift) & 3;
	clk->rate = final_divider(clk->parent->rate, div_ratio,
				  clkgenBdiv2->normal_div);
}

static struct clk_ops clkgenb_div2_ops = {
	.recalc		= clkgenb_div2_recalc,
};

static struct clkgenBdiv2 clkB_div2clks[5] = {
#define DIV2_B_BDISP266_CLOCK 0
	CLKGENB_DIV2(bdisp_266, 16, 3),
#define DIV2_B_COMPO200_CLOCK 1
	CLKGENB_DIV2(compo_200,  8, 4),
#define DIV2_B_DISP200_CLOCK 2
	CLKGENB_DIV2(disp_200,  10, 4),
#define DIV2_B_VDP200_CLOCK 3
	CLKGENB_DIV2(vdp_200,   12, 4),
#define DIV2_B_DMU1266_CLOCK 4
	CLKGENB_DIV2(dmu1_266,  20, 3)
};

static void icreg_emi_eth_clk_recalc(struct clk *clk)
{
	unsigned long mux_cfg;
	unsigned long div_ratio;

	mux_cfg = ctrl_inl(CLOCKGEN_MUX_CFG);
	div_ratio = ((mux_cfg & (CLOCKGEN_MUX_CFG_IC_REG_SRC)) == 0) ? 8 : 6;
	clk->rate = clk->parent->rate / div_ratio;
}

static struct clk_ops icreg_emi_eth_clk_ops = {
	.recalc		= icreg_emi_eth_clk_recalc,
};

static struct clk clkB_miscclks[3] = {
	/* Propages to comms_clk */
#define MISC_B_ICREG_CLOCK 0
	CLKGENB(ic_reg,     icreg_emi_eth_clk_ops, CLK_RATE_PROPAGATES),
#define MISC_B_ETHERNET_CLOCK 1
	CLKGENB(ethernet,   icreg_emi_eth_clk_ops, 0),
#define MISC_B_EMIMASTER_CLOCK 2
	CLKGENB(emi_master, icreg_emi_eth_clk_ops, 0),
};

static struct clk *clockgenb_clocks[] = {
	&clkB_pllclks[0].clk,

	&clkB_div2clks[DIV2_B_BDISP266_CLOCK].clk,
	&clkB_div2clks[DIV2_B_COMPO200_CLOCK].clk,
	&clkB_div2clks[DIV2_B_DISP200_CLOCK].clk,
	&clkB_div2clks[DIV2_B_VDP200_CLOCK].clk,
	&clkB_div2clks[DIV2_B_DMU1266_CLOCK].clk,

	&clkB_miscclks[MISC_B_ICREG_CLOCK],
	&clkB_miscclks[MISC_B_ETHERNET_CLOCK],
	&clkB_miscclks[MISC_B_EMIMASTER_CLOCK]
};


static void comms_clk_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
}

static struct clk_ops comms_clk_ops = {
	.recalc		= comms_clk_recalc,
};

static struct clk comms_clk = {
	.name		= "comms_clk",
	.parent		= &clkB_miscclks[MISC_B_ICREG_CLOCK],
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &comms_clk_ops
};

static struct clk new_module_clk = {
	.name		= "module_clk",
	.parent		= &clkB_miscclks[0],
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &comms_clk_ops
};

void* clk_get_iomem()
{
        return (void*)0xfd700000;
}

int __init clk_init(void)
{
	int i, ret = 0;
	int fs, clk;

	/* Clockgen A */

	for (i = 0; i < ARRAY_SIZE(clockgena_clocks); i++) {
		struct clk *clk = clockgena_clocks[i];

		ret |= clk_register(clk);
		clk_enable(clk);
	}
	if (cpu_data->cut_major < 2) {
		/* module clock */
		ret |= clk_register(&sh4clks[2].clk);
		clk_enable(&sh4clks[2].clk);

		/* interconnect clock */
		ret |= clk_register(&sh4clks[1].clk);
		clk_enable(&sh4clks[1].clk);
	}

	/* Propagate the PLL values down */
	for (i=0; i<3; i++) {
		clk_set_rate(&pllclks[i].clk, clk_get_rate(&pllclks[i].clk));
		clk_put(&pllclks[i].clk);
	}

	/* Clockgen B */

	for (fs=0; fs<3; fs++) {
		for (clk=1; clk<5; clk++) {
			struct fsclk *fsclk = &fsclks[(fs*4)+(clk-1)];

			sprintf(fsclk->name, "fs%dclk%d", fs, clk);
			fsclk->cfg_addr = CLKGENB_FSx_CLKy_CFG(fs, clk);
			fsclk->clk.name = fsclk->name;
			fsclk->clk.flags = CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES;
			fsclk->clk.ops = &fs_clk_ops;

			ret |= clk_register(&fsclk->clk);
			clk_enable(&fsclk->clk);
		}
	}

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

	/* Propagate the PLL values down */
	for (fs=0; fs<3; fs++) {
		for (clk=1; clk<5; clk++) {
			struct fsclk *fsclk = &fsclks[(fs*4)+(clk-1)];
			struct clk *clk = &fsclk->clk;

			clk_set_rate(clk, clk_get_rate(clk));
			clk_put(clk);
		}
	}
	for (i = 0; i < ARRAY_SIZE(clkB_pllclks); ++i) {
		clk_set_rate(&clkB_pllclks[i].clk, clk_get_rate(&clkB_pllclks[i].clk));
		clk_put(&clkB_pllclks[i].clk);
	}

	return ret;
}
