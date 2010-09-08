/*******************************************************************************
 *
 * File name   : clock-utils.h
 * Description : Utility functions not related to the Low Level API
 *
 * COPYRIGHT (C) 2010 STMicroelectronics - All Rights Reserved
 * This file is under the GPL 2 License.
 *
 ******************************************************************************/

#include <linux/clkdev.h>

static inline int clk_register_table(struct clk *clks, int num, int enable)
{
	int i;

	for (i = 0; i < num; i++) {
		struct clk *clk = &clks[i];
		int ret;
		struct clk_lookup *cl;

		ret = clk_register(clk);
		if (ret)
			return ret;
		ret = clk_enable(clk);
		if (ret)
			return ret;
		cl = clkdev_alloc(clk, clk->name, NULL);
		if (!cl)
			return -ENOMEM;
		clkdev_add(cl);
	}

	return 0;
}
