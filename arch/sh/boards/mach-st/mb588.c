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
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/emi.h>
#include <mach/stem.h>

/*
 * Comment out this line to use NAND through the EMI bit-banging driver
 * instead of the Flex driver.
 */
#define NAND_USES_FLEX

static struct mtd_partition nand_parts[] = {
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

static struct stm_nand_bank_data nand_bank_data = {
	.csn			= STEM_CS0_BANK,
	.nr_partitions		= ARRAY_SIZE(nand_parts),
	.partitions		= nand_parts,
	.options		= 0,
	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30		/* in us */
	},
	.emi_withinbankoffset	= STEM_CS0_OFFSET,
};

#ifndef NAND_USES_FLEX
static struct platform_device nand_device = {
	.name		= "stm-nand-emi",
	.dev.platform_data = &(struct stm_plat_nand_emi_data){
		.nr_banks	= 1,
		.banks		= &nand_bank_data,
		.emi_rbn_gpio	= -1,
	},
};
#endif

static int __init mb588_init(void)
{
#ifdef NAND_USES_FLEX
/* Use this block if using Flex controller */
#if defined(CONFIG_CPU_SUBTYPE_STX7105)
	stx7105_configure_nand_flex(&nand_bank_data);
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
	stx7111_configure_nand_flex(&nand_bank_data);
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	stx7200_configure_nand_flex(&nand_bank_data);
#else
#	error Unsupported SOC.
#endif
	return 0;
#else
	platform_add_device(&nand_device);
#endif
}
arch_initcall(mb588_init);

