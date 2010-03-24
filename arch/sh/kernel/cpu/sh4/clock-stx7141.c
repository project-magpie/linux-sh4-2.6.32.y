/*
 * Copyright (C) 2010 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 * Code to handle the arch clocks on the STx7141.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

static int generic_clk_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
	return 0;
}

static struct clk_ops generic_clk_ops = {
	.init = generic_clk_recalc,
	.recalc = generic_clk_recalc,
};

static struct clk stm_clk[] = {
	{
		.name = "sh4_clk",
		.ops = &generic_clk_ops,
	}, {
		.name = "module_clk",
		.ops = &generic_clk_ops,
	}, {
		.name = "comms_clk",
		.ops = &generic_clk_ops,
	}
};


int __init arch_clk_init(void)
{
	int i, ret = 0;

	stm_clk[0].parent = clk_get(NULL, "CLKA_SH4_ICK");
	stm_clk[1].parent = clk_get(NULL, "CLKA_IC_IF_100");
	stm_clk[2].parent = stm_clk[1].parent;

	for (i = 0; i < ARRAY_SIZE(stm_clk); ++i)
		if (!clk_register(&stm_clk[i]))
			clk_enable(&stm_clk[i]);

	return ret;
}
