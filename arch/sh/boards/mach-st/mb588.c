/*
 * arch/sh/boards/st/common/mb588.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics NAND Flash STEM board
 *
 * This code assumes that STEM_notCS0 line is used (J1 = 1-2).
 *
 * J2 may be left totally unfitted.
 *
 * If J3 is closed NAND chip is write protected, so if you wish to modify
 * its content...
 *
 * Some additional main board setup may be required to use proper CS signal
 * signal - see "arch/sh/include/mach-<board>/mach/stem.h" for more
 * information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <mach/stem.h>

static struct mtd_partition nand_partitions[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct nand_config_data nand_config = {
	.emi_bank		= STEM_CS0_BANK,
	.emi_withinbankoffset	= STEM_CS0_OFFSET,

	/* Timing data for ST-NAND512W3A2C */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	 = 40,		 /* times in ns */
		.rd_oee_start	 = 0,
		.rd_oee_end	 = 10,
		.rd_latchpoint	 = 10,
		.busreleasetime  = 10,

		.wr_cycle_time	 = 40,
		.wr_oee_start	 = 0,
		.wr_oee_end	 = 10,
		.wait_active_low = 0,
	},

	.chip_delay		= 20,
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
	.rbn_port		= -1,
	.rbn_pin		= -1,
};

static int __init mb588_init(void)
{
#if defined(CONFIG_CPU_SUBTYPE_STX7105)
	stx7105_configure_nand(&nand_config);
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
	stx7111_configure_nand(&nand_config);
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	stx7200_configure_nand(&nand_config);
#else
#	error Unsupported SOC.
#endif
	return 0;
}
arch_initcall(mb588_init);

