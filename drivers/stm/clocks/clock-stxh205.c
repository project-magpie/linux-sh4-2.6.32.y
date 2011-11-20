/*****************************************************************************
 *
 * File name   : clock-stxh205.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#include <linux/stm/clk.h>
#include "clock-common.h"
#include "clock-utils.h"

static struct clk clocks[] = {
	{
		.name = "module_clk",
		.rate = 100000000,
	}, {
		.name = "comms_clk",
		.rate = 100000000,
	}, {
		.name = "sbc_comms_clk",
		.rate = 30000000,
	}
};

int __init plat_clk_init(void)
{
	return clk_register_table(clocks, ARRAY_SIZE(clocks), 1);
}
