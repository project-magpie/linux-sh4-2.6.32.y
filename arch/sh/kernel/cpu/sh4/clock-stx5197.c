/*
 * Copyright (C) 2010 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the arch clocks on the STx5197.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init arch_clk_init(void)
{
	int ret;

	ret = plat_clk_init();
	if (ret)
		return ret;

	clk_add_alias("cpu_clk", NULL, "PLL_ST40_ICK", NULL);
	clk_add_alias("module_clk", NULL, "PLL_ST40_PCK", NULL);
	clk_add_alias("comms_clk", NULL, "PLL_SYS", NULL);

	return ret;
}
