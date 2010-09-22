/*
 * Copyright (C) 2010 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the arch clocks on the STx7105.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init arch_clk_init(void)
{
	int ret;

	ret = plat_clk_init();
	if (ret)
		return ret;

	clk_add_alias("sh4_clk", NULL, "CLKA_SH4L2_ICK", NULL);
	clk_add_alias("module_clk", NULL, "CLKA_IC_REG_LP_ON", NULL);
	clk_add_alias("comms_clk", NULL, "CLKA_IC_REG_LP_ON", NULL);

	return ret;
}
