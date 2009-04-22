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

static struct plat_stmnand_data nand_config = {
	.emi_bank		= STEM_CS0_BANK,
	.emi_withinbankoffset	= STEM_CS0_OFFSET,
	.chip_delay		= 30,
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
	.rbn_port		= -1,
	.rbn_pin		= -1,

#if defined(CONFIG_CPU_SUBTYPE_STX7200)
	/* Timing data for SoCs using STM_NAND_EMI/FLEX/AFM drivers */
	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
	},
#else
	/* Legacy Timing data for generic plat_nand driver */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	 = 50,		 /* times in ns */
		.rd_oee_start	 = 0,
		.rd_oee_end	 = 10,
		.rd_latchpoint	 = 10,
		.busreleasetime  = 10,

		.wr_cycle_time	 = 50,
		.wr_oee_start	 = 0,
		.wr_oee_end	 = 10,
		.wait_active_low = 0,
	},
#endif
};

#if defined(CONFIG_CPU_SUBTYPE_STX7200)
/* For SoCs migrated to STM_NAND_EMI/FLEX/AFM drivers, setup template platform
 * device structure.  SoC setup will configure SoC specific data.
 */
static const char *nand_part_probes[] = { "cmdlinepart", NULL };

static struct platform_device nand_device = {
	.name		= "stm-nand",
	.id		= STEM_CS0_BANK,
	.num_resources	= 2,	/* Note: EMI mem configured by driver */
	.resource	= (struct resource[]) {
		[0] = {
			/* NAND controller base address (FLEX/AFM) */
			.name		= "flex_mem",
			.flags		= IORESOURCE_MEM,
		},
		[1] = {
			/* NAND controller IRQ (FLEX/AFM) */
			.name		= "flex_irq",
			.flags		= IORESOURCE_IRQ,
		},
		[2] = {
			/* EMI Bank base address */
			.name		= "emi_mem",
			.flags		= IORESOURCE_MEM,
		},

	},

	.dev		= {
		.platform_data = &(struct platform_nand_data) {
			.chip =
			{
				.chip_delay	= 30,
				.partitions	= nand_partitions,
				.nr_partitions	= ARRAY_SIZE(nand_partitions),
				.part_probe_types = nand_part_probes,
			},
			.ctrl =
			{
				.priv = &nand_config,
			},
		},
	},
};



#endif

static int __init mb588_init(void)
{
#if defined(CONFIG_CPU_SUBTYPE_STX7105)
	stx7105_configure_nand(&nand_config);
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
	stx7111_configure_nand(&nand_config);
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	stx7200_configure_nand(&nand_device);
#else
#	error Unsupported SOC.
#endif
	return 0;
}
arch_initcall(mb588_init);

