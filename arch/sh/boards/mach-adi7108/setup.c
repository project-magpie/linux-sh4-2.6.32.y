/*
 * arch/sh/boards/mach-adi7108/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Srinivas Kandagatla (srinivas.kandagatla@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/flash.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>


#define ADI7108_PIO_POWER_ON_ETHERNET0 stm_gpio(19, 7)
#define ADI7108_PIO_POWER_ON_ETHERNET1 stm_gpio(15, 4)
#define ADI7108_GPIO_FLASH_WP stm_gpio(2, 3)
#define ADI7108_GPIO_FLASH_HOLD stm_gpio(2, 2)
#define ADI7108_GPIO_MII1_SPEED_SEL stm_gpio(21, 7)


static void __init adi7108_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi7108-ADI Board "
			"initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			.hw_flow_control = 0,
			.is_console = 1, });

	stx7108_configure_asc(1, &(struct stx7108_asc_config) {
			.hw_flow_control = 1, });
}


static int adi7108_phy1_reset(void *bus)
{
	static int done;
	if (!done) {
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 1);
		done = 1;
	}

	return 1;
}


static int adi7108_phy0_reset(void *bus)
{
	static int done;

	if (!done) {
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET0, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET0, 1);
		done = 1;
	}

	return 1;
}

static void adi7108_mii_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO21[7]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(ADI7108_GPIO_MII1_SPEED_SEL, 1);
	else
		gpio_set_value(ADI7108_GPIO_MII1_SPEED_SEL, 0);
}

static struct platform_device adi7108_phy_devices[] = {
	{ /* RANLINK WIFI connection */
		.name = "stmmacphy",
		.id = 0,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 0,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_GMII,
			.phy_reset = &adi7108_phy0_reset,
		},
	}, { /* On-board ICplus IP1001 */
		.name = "stmmacphy",
		.id = 1,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 1,
			.phy_addr = 1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_GMII,
			.phy_reset = &adi7108_phy1_reset,
		},
	},
};

/* NOR FLASH */
static struct mtd_partition adi7108_nor_flash_parts[3] = {
	{
		.name = "NOR 1",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "NOR 2",
		.size = 0x00200000,
		.offset = 0x00040000,
	}, {
		.name = "NOR 3",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00240000,
	}
};

static struct physmap_flash_data adi7108_nor_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(adi7108_nor_flash_parts),
	.parts		= adi7108_nor_flash_parts,
};

static struct platform_device adi7108_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 256*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &adi7108_nor_flash_data,
	},
};

/* Serial FLASH */
static struct platform_device adi7108_serial_flash_bus = {
	.name           = "spi_gpio",
	.id             = 0,
	.num_resources  = 0,
	.dev            = {
		.platform_data =
		&(struct spi_gpio_platform_data) {
			.sck = stm_gpio(1, 6),
			.mosi = stm_gpio(2, 0),
			.miso = stm_gpio(2, 1),
			.num_chipselect = 1,
		}
	},
};

static struct mtd_partition adi7108_serial_flash_parts[] = {
	{
		.name = "Serial 1",
		.size = 0x00400000,
		.offset = 0,
	}, {
		.name = "Serial 2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data adi7108_serial_flash_data = {
	.name = "m25p80",
	.parts = adi7108_serial_flash_parts,
	.nr_parts = ARRAY_SIZE(adi7108_serial_flash_parts),
	.type = "m25p128",	/* Check device on individual board! */
};

static struct spi_board_info adi7108_serial_flash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num        = 0,
		.chip_select    = 0,
		.controller_data = (void *)stm_gpio(1, 7),
		.max_speed_hz   = 500000,
		.platform_data  = &adi7108_serial_flash_data,
		.mode           = SPI_MODE_3,
	},
};


/* NAND FLASH */
static struct mtd_partition adi7108_nand_flash_parts[] = {
	{
		.name   = "NAND 1",
		.offset = 0,
		.size   = 0x00800000
	}, {
		.name   = "NAND 2",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct stm_nand_bank_data adi7108_nand_flash_data = {
	.csn		= 1,
	.nr_partitions	= ARRAY_SIZE(adi7108_nand_flash_parts),
	.partitions	= adi7108_nand_flash_parts,
	.options	= NAND_NO_AUTOINCR || NAND_USE_FLASH_BBT,
	.timing_data = &(struct stm_nand_timing_data) {
		.sig_setup      = 10,           /* times in ns */
		.sig_hold       = 10,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 30,
		.rd_on          = 10,
		.rd_off         = 30,
		.chip_delay     = 30,           /* in us */
	},
	.emi_withinbankoffset	= 0,
};

static struct platform_device *adi7108_devices[] __initdata = {
	&adi7108_phy_devices[0],
	&adi7108_phy_devices[1],
	&adi7108_serial_flash_bus,
	&adi7108_nor_flash,
};

static int __init device_init(void)
{
	u32 bank1_start;
	u32 bank2_start;
	u32 bank3_start;
	u32 boot_device;

	bank1_start = emi_bank_base(1);
	bank2_start = emi_bank_base(2);
	bank3_start = emi_bank_base(3);

	boot_device = gpio_get_value(stm_gpio(25, 4));
	boot_device |= gpio_get_value(stm_gpio(25, 5)) << 1;
	boot_device |= gpio_get_value(stm_gpio(25, 6)) << 2;
	boot_device |= gpio_get_value(stm_gpio(25, 7)) << 3;

	/*
	 *
	 * BootUp	RE32 & RE35	RE33 & RE34
	 * NOR		    0R		    NC
	 * NAND		    NC		    0R
	 *
	 */

	BUG_ON(boot_device > 0xA);
	switch (boot_device) {
	case 0x0:
	case 0x5:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		adi7108_nor_flash.resource[0].start = 0x00000000;
		adi7108_nor_flash.resource[0].end = bank1_start - 1;
		adi7108_nand_flash_data.csn = 1;
		break;
	case 0xA:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		adi7108_nor_flash.resource[0].start = bank1_start;
		adi7108_nor_flash.resource[0].end = bank2_start - 1;
		adi7108_nand_flash_data.csn = 1;
		break;
	default:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		adi7108_nor_flash.resource[0].start = bank1_start;
		adi7108_nor_flash.resource[0].end = bank2_start - 1;
		adi7108_nand_flash_data.csn = 0;
		break;
	}

	/* Limit NOR FLASH to addressable range, regardless of actual EMI
	 * configuration! */
	if (adi7108_nor_flash.resource[0].end -
	    adi7108_nor_flash.resource[0].start > 0x4000000)
		adi7108_nor_flash.resource[0].end =
			adi7108_nor_flash.resource[0].start + 0x4000000-1;

	/* NIM */
	stx7108_configure_ssc_i2c(1, NULL);

	/* AV */
	stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio14_4,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio14_5, });

	/* EEPROM */
	stx7108_configure_ssc_i2c(5, NULL);

	/* HDMI */
	stx7108_configure_ssc_i2c(6, NULL);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
			.rx_mode = stx7108_lirc_rx_mode_ir, });

	stx7108_configure_usb(0);
	stx7108_configure_usb(1);
	stx7108_configure_usb(2);

	stx7108_configure_sata(0);
	stx7108_configure_sata(1);

#if 0
	gpio_request(ADI7108_PIO_POWER_ON_ETHERNET0, "POWER_ON_ETHERNET");
	gpio_direction_output(ADI7108_PIO_POWER_ON_ETHERNET0, 0);


	stx7108_configure_ethernet(0, (&(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0, });
#else
	/* To use the MII/GMII mode.
	 *
	 *		RP1 	MII1_EN
	 *	GMII    NC      1
	 *	MII     51R     0
	 *
	 */
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(ADI7108_PIO_POWER_ON_ETHERNET1, "POWER_ON_ETHERNET");
	gpio_direction_output(ADI7108_PIO_POWER_ON_ETHERNET1, 0);

	gpio_request(ADI7108_GPIO_MII1_SPEED_SEL, "stmmac");
	gpio_direction_output(ADI7108_GPIO_MII1_SPEED_SEL, 0);

	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_gmii_gtx,
			.ext_clk = 0,
			.phy_bus = 1,
			.txclk_select = adi7108_mii_txclk_select, });

#endif

	stx7108_configure_nand(&(struct stx7108_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &adi7108_nand_flash_data,
			.rbn.flex_connected = 1,});
#if 0
	gpio_request(ADI7108_GPIO_FLASH_WP, "FLASH_WP");
	gpio_direction_output(ADI7108_GPIO_FLASH_WP, 1);

	gpio_request(ADI7108_GPIO_FLASH_HOLD, "FLASH_HOLD");
	gpio_direction_output(ADI7108_GPIO_FLASH_HOLD, 1);


	/* Serial Flash */
	spi_register_board_info(adi7108_serial_flash,
				ARRAY_SIZE(adi7108_serial_flash));
#endif
	stx7108_configure_mmc();

	return platform_add_devices(adi7108_devices,
			ARRAY_SIZE(adi7108_devices));
}
arch_initcall(device_init);


static void __iomem *adi7108_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_adi7108 __initmv = {
	.mv_name = "adi7108",
	.mv_setup = adi7108_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = adi7108_ioport_map,
};

#ifdef CONFIG_HIBERNATION_ON_MEMORY
int stm_defrost_board(void *data)
{
	gpio_direction_output(ADI7108_PIO_POWER_ON_ETHERNET1, 0);

	gpio_direction_output(ADI7108_GPIO_MII1_SPEED_SEL, 0);

	/*
	 * adi7108_phy_reset(...);
	 */
	gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(ADI7108_PIO_POWER_ON_ETHERNET1, 1);

	return 0;
}
#endif
