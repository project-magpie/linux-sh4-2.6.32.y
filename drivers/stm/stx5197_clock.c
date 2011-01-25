/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx5197.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	clk_add_alias("cpu_clk", NULL, "PLL_ST40_ICK", NULL);
	clk_add_alias("module_clk", NULL, "PLL_ST40_PCK", NULL);
	clk_add_alias("comms_clk", NULL, "PLL_SYS", NULL);

	return 0;
}
