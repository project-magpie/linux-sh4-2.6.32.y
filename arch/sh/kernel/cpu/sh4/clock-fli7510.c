/*
 * Copyright (C) 2009 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Clocking framework stub.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/clkdev.h>
#include <asm/clock.h>
#include <asm/freq.h>


/* SH4 generic clocks ----------------------------------------------------- */

static struct clk clocks[] = {
	{
		.name = "module_clk",
		.rate = 100000000,
	}, {
		.name = "comms_clk",
		.rate = 100000000,
	}
};



/* ------------------------------------------------------------------------ */

int __init arch_clk_init(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(clocks); ++i) {
		struct clk *clk = &clocks[i];
		struct clk_lookup *cl;

		ret = clk_register(clk);
		if (ret)
			return ret;
		cl = clkdev_alloc(clk, clk->name, NULL);
		if (!cl)
			return -ENOMEM;
		clkdev_add(cl);
	}

	return ret;
}
