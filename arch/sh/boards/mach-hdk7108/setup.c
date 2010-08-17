/*
 * arch/sh/boards/mach-hdk7108/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
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
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/flash.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/pci-synopsys.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>

/*
 * The FLASH devices are configured according to the boot-mode:
 *
 *                                    boot-from-XXX
 * --------------------------------------------------------------------
 *                         NOR             NAND            SPI
 * Mode Pins              (x16)        (x8, LP, LA)        (ST)
 * --------------------------------------------------------------------
 * JH2 1 (M2)             1 (E)            0 (W)          0 (W)
 *     2 (M3)             0 (W)            0 (W)          1 (E)
 * JH4 1 (M4)             1 (E)            1 (E)          0 (W)
 *     2 (M5)             0 (W)            0 (W)          1 (E)
 *
 * CS Routing
 * --------------------------------------------------------------------
 * JF-2                  2-1 (E)           2-3 (W)         2-3 (W)
 * JF-3                  2-1 (E)           2-3 (W)         2-3 (W)
 *
 * Post-boot Access
 * --------------------------------------------------------------------
 * NOR (limit)         EMIA (64MB)     EMIB (8MB)[1]    EMIB (8MB)[2]
 * NAND                EMIB/FLEXB          FLEXA           FLEXA [2]
 * SPI                   SPI_PIO          SPI_PIO         SPI_PIO
 * --------------------------------------------------------------------
 *
 *
 * Switch positions are given in terms of (N)orth, (E)ast, (S)outh, and (W)est,
 * when viewing the board with the LED display to the South and the power
 * connector to the North.
 *
 * [1] It is also possible to map NOR Flash onto EMIC.  This gives access to
 *     40MB, but has the side-effect of setting A26 which imposes a 64MB offset
 *     from the start of the Flash device.
 *
 * [2] An alternative configuration is map NAND onto EMIB/FLEXB, and NOR onto
 *     EMIC (using the boot-from-NOR "CS Routing").  This allows the EMI
 *     bit-banging driver to be used for NAND Flash, and gives access to 40MB
 *     NOR Flash, subject to the conditions in note [1].
 */


#define HDK7108_PIO_POWER_ON stm_gpio(4, 3)
#define HDK7108_PIO_PCI_RESET stm_gpio(6, 4)
#define HDK7108_PIO_POWER_ON_ETHERNET stm_gpio(15, 4)
#define HDK7108_GPIO_FLASH_WP stm_gpio(5, 5)


static void __init hdk7108_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7108 board initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7108_configure_asc(1, &(struct stx7108_asc_config) {
			.hw_flow_control = 1, });
}



static struct platform_device hdk7108_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 0),
			}, {
				.name = "RED",
				.gpio = stm_gpio(3, 1),
			},
		},
	},
};

static struct tm1668_key hdk7108_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7108_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7108_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(2, 5),
		.gpio_sclk = stm_gpio(2, 4),
		.gpio_stb = stm_gpio(2, 6),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7108_front_panel_keys),
		.keys = hdk7108_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7108_front_panel_characters),
		.characters = hdk7108_front_panel_characters,
		.text = "7108",
	},
};



static int hdk7108_phy_reset(void *bus)
{
	static int done;

	/* This line is shared between both MII interfaces */
	if (!done) {
		gpio_set_value(HDK7108_PIO_POWER_ON_ETHERNET, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(HDK7108_PIO_POWER_ON_ETHERNET, 1);
		done = 1;
	}

	return 1;
}

static struct platform_device hdk7108_phy_devices[] = {
	{ /* MII connector JP2 */
		.name = "stmmacphy",
		.id = 0,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 1,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_MII,
			.phy_reset = &hdk7108_phy_reset,
		},
	}, { /* On-board ICplus IP1001 */
		.name = "stmmacphy",
		.id = 1,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 0,
			.phy_addr = 1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_MII,
			.phy_reset = &hdk7108_phy_reset,
		},
	},
};

/* NOR FLASH */
static struct mtd_partition hdk7108_nor_flash_parts[3] = {
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

static struct physmap_flash_data hdk7108_nor_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(hdk7108_nor_flash_parts),
	.parts		= hdk7108_nor_flash_parts,
};

static struct platform_device hdk7108_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 128*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &hdk7108_nor_flash_data,
	},
};

/* Serial FLASH */
static struct platform_device hdk7108_serial_flash_bus = {
	.name           = "spi_gpio",
	.id             = 8,
	.num_resources  = 0,
	.dev            = {
		.platform_data =
		&(struct spi_gpio_platform_data) {
			.sck = stm_gpio(1, 6),
			.mosi = stm_gpio(2, 1),
			.miso = stm_gpio(2, 0),
			.num_chipselect = 1,
		}
	},
};

static struct mtd_partition hdk7108_serial_flash_parts[] = {
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

static struct flash_platform_data hdk7108_serial_flash_data = {
	.name = "m25p80",
	.parts = hdk7108_serial_flash_parts,
	.nr_parts = ARRAY_SIZE(hdk7108_serial_flash_parts),
	.type = "m25p128",	/* Check device on individual board! */
};

static struct spi_board_info hdk7108_serial_flash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num        = 8,
		.chip_select    = 0,
		.controller_data = (void *)stm_gpio(1, 7),
		.max_speed_hz   = 1000000,
		.platform_data  = &hdk7108_serial_flash_data,
		.mode           = SPI_MODE_0,
	},
};


/* NAND FLASH */
static struct mtd_partition hdk7108_nand_flash_parts[] = {
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

static struct stm_nand_bank_data hdk7108_nand_flash_data = {
	.csn		= 1,
	.nr_partitions	= ARRAY_SIZE(hdk7108_nand_flash_parts),
	.partitions	= hdk7108_nand_flash_parts,
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

static struct platform_device *hdk7108_devices[] __initdata = {
	&hdk7108_leds,
	&hdk7108_front_panel,
	&hdk7108_phy_devices[0],
	&hdk7108_phy_devices[1],
	&hdk7108_serial_flash_bus,
	&hdk7108_nor_flash,
};



static struct stm_plat_pci_config hdk7108_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED,
	},
	.serr_irq = PCI_PIN_DEFAULT,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED,
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = HDK7108_PIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7108_pcibios_map_platform_irq(&hdk7108_pci_config, pin);
}



static int __init device_init(void)
{
	u32 bank1_start;
	u32 bank2_start;
	u32 bank3_start;
	u32 boot_device;
	int phy_bus;

	bank1_start = emi_bank_base(1);
	bank2_start = emi_bank_base(2);
	bank3_start = emi_bank_base(3);

	boot_device = gpio_get_value(stm_gpio(25, 4));
	boot_device |= gpio_get_value(stm_gpio(25, 5)) << 1;
	boot_device |= gpio_get_value(stm_gpio(25, 6)) << 2;
	boot_device |= gpio_get_value(stm_gpio(25, 7)) << 3;

	BUG_ON(boot_device > 0xA);
	switch (boot_device) {
	case 0x0:
	case 0x5:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		hdk7108_nor_flash.resource[0].start = 0x00000000;
		hdk7108_nor_flash.resource[0].end = bank1_start - 1;
		hdk7108_nand_flash_data.csn = 1;
		break;
	case 0xA:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		hdk7108_nor_flash.resource[0].start = bank1_start;
		hdk7108_nor_flash.resource[0].end = bank2_start - 1;
		hdk7108_nand_flash_data.csn = 0;
		break;
	default:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		hdk7108_nor_flash.resource[0].start = bank1_start;
		hdk7108_nor_flash.resource[0].end = bank2_start - 1;
		hdk7108_nand_flash_data.csn = 0;
		break;
	}

	/* Limit NOR FLASH to addressable range, regardless of actual EMI
	 * configuration! */
	if (hdk7108_nor_flash.resource[0].end -
	    hdk7108_nor_flash.resource[0].start > 0x4000000)
		hdk7108_nor_flash.resource[0].end =
			hdk7108_nor_flash.resource[0].start + 0x4000000-1;

	stx7108_configure_pci(&hdk7108_pci_config);

	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(HDK7108_PIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(HDK7108_PIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK7108_PIO_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK7108_PIO_POWER_ON, 1);

	/* NIM */
	stx7108_configure_ssc_i2c(1, NULL);
	/* AV */
	stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio14_4,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio14_5, });
	/* SYS - EEPROM & MII0 */
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
	stx7108_configure_ethernet(0, (&(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0, });
#else
	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 1, });
#endif

	/*
	 * FLASH_WP is shared between between NOR and NAND FLASH.  However,
	 * since NAND MTD has no concept of write-protect, we permanently
	 * disable WP.
	 */
	gpio_request(HDK7108_GPIO_FLASH_WP, "FLASH_WP");
	gpio_direction_output(HDK7108_GPIO_FLASH_WP, 1);

	stx7108_configure_nand(&(struct stx7108_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &hdk7108_nand_flash_data,
			.rbn.flex_connected = 1,});

	spi_register_board_info(hdk7108_serial_flash,
				ARRAY_SIZE(hdk7108_serial_flash));

	stx7108_configure_mmc();

	return platform_add_devices(hdk7108_devices,
			ARRAY_SIZE(hdk7108_devices));
}
arch_initcall(device_init);


static void __iomem *hdk7108_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hdk7108 __initmv = {
	.mv_name = "hdk7108",
	.mv_setup = hdk7108_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hdk7108_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};

