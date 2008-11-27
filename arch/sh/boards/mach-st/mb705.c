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
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/bug.h>
#include <asm/processor.h>
#include <asm/irq-ilc.h>
#include "common.h"
#include "mb705-epld.h"

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
				.gpio = stpio_to_gpio(2, 0),
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

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,	/* Can be overridden */
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
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
		.offset = 0x20000
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
		.chip_select	= spi_set_cs(15, 2),
		.max_speed_hz	= 500000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};

/* GPIO based SPI */
static struct platform_device spi_pio_device[] = {
	{
		.name           = "spi_st_pio",
		.id             = 8,
		.num_resources  = 0,
		.dev            = {
			.platform_data =
			&(struct ssc_pio_t) {
				.pio = {{15, 0}, {15, 1}, {15, 3} },
			},
		},
	},
};



static struct platform_device *mb705_devices[] __initdata = {
	&epld_device,
	&mb705_gpio_led,
	&mb705_display_device,
	&mb705_fpbutton_device,
	&physmap_flash,
	&spi_pio_device[0],
};

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
	.emi_bank		= 1,	/* Can be overridden */
	.emi_withinbankoffset	= 0,

	/* Timings for NAND512W3A */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	 = 40,		 /* times in ns */
		.rd_oee_start	 = 0,
		.rd_oee_end	 = 10,
		.rd_latchpoint	 = 10,
		.busreleasetime  = 0,

		.wr_cycle_time	 = 40,
		.wr_oee_start	 = 0,
		.wr_oee_end	 = 10,

		.wait_active_low = 0,
	},

	.chip_delay		= 40,		/* time in us */
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
};

#include <linux/delay.h>

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
		nand_config.emi_bank = 0;
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

	stx7105_configure_nand(&nand_config);

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
