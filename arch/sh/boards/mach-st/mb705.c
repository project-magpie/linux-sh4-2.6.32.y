/*
 * arch/sh/boards/st/common/mb705.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STB peripherals board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/bug.h>
#include <linux/irq.h>
#include <asm/processor.h>
#include <asm/irq-ilc.h>
#include <mach/common.h>
#include "mb705-epld.h"

/*
 * Comment out this line to use NAND through the EMI bit-banging driver
 * instead of the Flex driver.
 */
#define NAND_USES_FLEX

static DEFINE_SPINLOCK(misc_lock);

static struct platform_device mb705_gpio_led = {
	.name = "leds-gpio",
	.id = 1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(2, 0),
				.active_low = 1,
			},
		},
	},
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x07000000,
			.end	= 0x070002ff,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 16,
	},
};


static struct platform_device mb705_display_device = {
	.name = "mb705-display",
	.id = -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x07000140,
			.end	= 0x070001bf,
			.flags	= IORESOURCE_MEM,
		}
	},
};

static struct platform_device mb705_fpbutton_device = {
	.name = "mb705-fpbutton",
	.id = -1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= ILC_EXT_IRQ(0),
			.end	= ILC_EXT_IRQ(0),
			.flags	= IORESOURCE_IRQ,
		}, {
			/* Mask for the EPLD status register */
			.name	= "mask",
			.start	= 1<<9,
			.end	= 1<<9,
			.flags	= IORESOURCE_IRQ,
		}
	},
};

static void set_vpp(struct map_info * info, int enable)
{
	u16 reg;

	spin_lock(&misc_lock);

	reg = epld_read(EPLD_EMI_MISC);
	if (enable)
		reg |= EPLD_EMI_MISC_NORFLASHVPPEN;
	else
		reg &= ~EPLD_EMI_MISC_NORFLASHVPPEN;
	epld_write(reg, EPLD_EMI_MISC);

	spin_unlock(&misc_lock);
}

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 32*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.set_vpp	= set_vpp,
	},
};


/* MTD partitions for Serial FLASH device */
static struct mtd_partition serialflash_partitions[] = {
	{
		.name = "sflash_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "sflash_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

/* Serial FLASH is type 'm25p32', handled by 'm25p80' SPI Protocol driver */
static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p32",
};

/* SPI 'board_info' to register serial FLASH protocol driver */
static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias	= "m25p80",
		.bus_num	= 8,
		.chip_select	= stm_gpio(15, 2),
		.max_speed_hz	= 500000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};

/* GPIO based SPI */
static struct platform_device spi_gpio_device = {
	.name           = "spi-stm-gpio",
	.id             = 8,
	.num_resources  = 0,
	.dev            = {
		.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(15, 0),
			.gpio_mtsr = stm_gpio(15, 1),
			.gpio_mrst = stm_gpio(15, 3),
		},
	},
};

/* NAND Device */
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

struct stm_nand_bank_data nand_bank_data = {
	.csn		= 1,
	.nr_partitions	= ARRAY_SIZE(nand_parts),
	.partitions	= nand_parts,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.timing_data		= &(struct stm_nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},

	.emi_withinbankoffset	= 0,
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

static struct platform_device *mb705_devices[] __initdata = {
	&epld_device,
	&mb705_gpio_led,
	&mb705_display_device,
	&mb705_fpbutton_device,
	&physmap_flash,
	&spi_gpio_device,
#ifndef NAND_USES_FLEX
	&nand_device,
#endif
};

static DEFINE_SPINLOCK(mb705_reset_lock);

void mb705_reset(int mask, unsigned long usdelay)
{
	u16 reg;

	spin_lock(&mb705_reset_lock);
	reg = epld_read(EPLD_EMI_RESET);
	reg |= mask;
	epld_write(reg, EPLD_EMI_RESET);
	spin_unlock(&mb705_reset_lock);

	udelay(usdelay);

	spin_lock(&mb705_reset_lock);
	reg = epld_read(EPLD_EMI_RESET);
	reg &= ~mask;
	epld_write(reg, EPLD_EMI_RESET);
	spin_unlock(&mb705_reset_lock);
}

static int __init mb705_init(void)
{
	int i;

	/* So far valid only for 7105 processor board! */
	BUG_ON(cpu_data->type != CPU_STX7105);

	/* We are't actually doing this early here... */
	epld_early_init(&epld_device);

	/* Check out the EPLDs */
	for (i=0; i<3; i++) {
		int ident_offset = (0x100 * i) + 0;
		int test_offset = (0x100 * i) + 2;
		u16 ident;
		u16 test;
		u16 mask = (i==0) ? 0xffff : 0xff;

		ident = epld_read(ident_offset);
		epld_write(0xab12+i, test_offset);
		test = epld_read(test_offset);

		printk(KERN_INFO
		       "mb705 %s_EPLD: board rev %c, EPLD rev %d, test %s\n",
		       (char*[3]){"EMI", "TS", "AUD" }[i],
		       ((ident >> 4) & 0xf) - 1 + 'A', ident & 0xf,
		       (((test ^ (0xab12+i)) & mask) == mask) ?
		        "passed" : "failed");
	}

	/* Determine whether NOR and NAND devices are swapped. */
	i = epld_read(EPLD_EMI_SWITCH);
	if (i & EPLD_EMI_SWITCH_BOOTFROMNOR) {
		u32 bank1_start = emi_bank_base(1);
		u32 bank2_start = emi_bank_base(2);
		physmap_flash.resource[0].start = bank1_start;
		physmap_flash.resource[0].end = bank2_start - 1;
		nand_bank_data.csn = 0;
	}

	/*
	 * The MTD NAND code doesn't understand the concept of VPP,
	 * (or hardware write protect) so permanently enable it.
	 * Also disable NOR VPP enable just in case.
	 */
	i = epld_read(EPLD_EMI_MISC);
	i &= ~EPLD_EMI_MISC_NORFLASHVPPEN;
	i |= EPLD_EMI_MISC_NOTNANDFLASHWP;
	epld_write(i, EPLD_EMI_MISC);

#ifdef NAND_USES_FLEX
	stx7105_configure_nand_flex(1, &nand_bank_data, 1);
#endif

	/* Interrupt routing.
	 * At the moment we only care about a small number of
	 * interrupts, so simply set up a static one-to-one routing.
	 *
	 * Interrupt sources:
	 *  0 : MAFE
	 *  1 : VOIP
	 *  2 : SPDIF out
	 *  3 : STRec status
	 *  4 : STEM0 (-> SysIrq(2) this matches the mb680 but active high)
	 *  5 : STEM1 (-> SysIrq(1) this matches the mb680 but active high))
	 *  6 : DVB
	 *  7 : DVB CD1
	 *  8 : DVB CD2
	 *  9 : FButton (-> SysIrq(0))
	 * 10 : EPLD intr in
	 */
	epld_write(0, EPLD_EMI_INT_PRI(9));
	epld_write(1, EPLD_EMI_INT_PRI(5));
	epld_write(2, EPLD_EMI_INT_PRI(4));
	epld_write((1<<4)|(1<<5)|(1<<9), EPLD_EMI_INT_MASK);

	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(mb705_devices, ARRAY_SIZE(mb705_devices));
}
arch_initcall(mb705_init);
