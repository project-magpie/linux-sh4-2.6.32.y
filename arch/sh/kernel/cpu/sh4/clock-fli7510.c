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
#include <asm/clock.h>
#include <asm/freq.h>

#include "clock-common.h"



/* SH4 generic clocks ----------------------------------------------------- */

static struct clk generic_module_clk = {
	.name = "module_clk",
	.rate = 100000000,
};

static struct clk generic_comms_clk = {
	.name = "comms_clk",
	.rate = 100000000,
};



/* ------------------------------------------------------------------------ */

int __init arch_clk_init(void)
{
	int err;

	/* Generic SH-4 clocks */

	err = clk_register(&generic_module_clk);
	if (err != 0)
		goto error;

	err = clk_register(&generic_comms_clk);

error:
	return err;
}
