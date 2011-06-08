/*
 * arch/sh/boards/st/mach-hdk7111/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: John Boddie (john.boddie@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7111 HDK board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/input.h>
#include <linux/stm/platform.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/stm/stx7111.h>
#include <linux/stm/emi.h>
#include <linux/stm/pci-synopsys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi_gpio.h>
#include <asm/irq-ilc.h>


#define HDK7111_PIO_PHY_RESET stm_gpio(1, 6)


static void __init hdk7111_setup(char** cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7111 HDK initialisation\n");

	stx7111_early_device_init();

	stx7111_configure_asc(2, &(struct stx7111_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7111_configure_asc(3, &(struct stx7111_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

static struct platform_device hdk7111_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB red",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 0),
			},
		},
	},
};

static struct gpio_keys_button hdk7111_buttons[] = {
	{
		.code = BTN_0,
		.gpio = stm_gpio(6, 4),
		.desc = "SW1",
	},
	{
		.code = BTN_1,
		.gpio = stm_gpio(6, 5),
		.desc = "SW2",
	},
	{
		.code = BTN_2,
		.gpio = stm_gpio(6, 6),
		.desc = "SW3",
	},
};

static struct gpio_keys_platform_data hdk7111_button_data = {
	.buttons = hdk7111_buttons,
	.nbuttons = ARRAY_SIZE(hdk7111_buttons),
};

static struct platform_device hdk7111_button_device = {
	.name = "gpio-keys",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &hdk7111_button_data,
	}
};

static struct mtd_partition hdk7111_physmap_flash_mtd_partitions[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	},
	{
		.name = "Kernel",
		.size = 0x00200000,
		.offset = 0x00040000,
	},
	{
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00240000,
	}
};

static struct physmap_flash_data hdk7111_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(hdk7111_physmap_flash_mtd_partitions),
	.parts		= hdk7111_physmap_flash_mtd_partitions
};

static struct platform_device hdk7111_physmap_device = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 64*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &hdk7111_physmap_flash_data,
	},
};

static struct mtd_partition hdk7111_nand_flash_mtd_partitions[] = {
	{
		.name   = "NAND root",
		.offset = 0,
		.size   = 0x00800000
	},
	{
		.name   = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct stm_nand_bank_data hdk7111_nand_flash_data = {
	.csn		= 0,
	.nr_partitions	= ARRAY_SIZE(hdk7111_nand_flash_mtd_partitions),
	.partitions	= hdk7111_nand_flash_mtd_partitions,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.timing_data	= &(struct stm_nand_timing_data) {
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

static struct platform_device hdk7111_nand_flash_device = {
	.name = "stm-nand-emi",
	.dev.platform_data = &(struct stm_plat_nand_emi_data){
		.nr_banks	= 1,
		.banks		= &hdk7111_nand_flash_data,
		.emi_rbn_gpio	= -1,
	},
};

static struct mtd_partition hdk7111_serial_flash_mtd_partitions[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	},
	{
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data hdk7111_serial_flash_data = {
	.name = "m25p80",
	.parts = hdk7111_serial_flash_mtd_partitions,
	.nr_parts = ARRAY_SIZE(hdk7111_serial_flash_mtd_partitions),
	.type = "m25p16",
};

static struct spi_board_info hdk7111_serial_flash_board_info =  {
	.modalias       = "m25p80",
	.bus_num	= 8,
	.max_speed_hz   = 500000,
	.platform_data  = &hdk7111_serial_flash_data,
	.mode	   = SPI_MODE_3,
	.chip_select    = 0,
	.controller_data = (void *)stm_gpio(6, 7),
};

static struct platform_device hdk7111_serial_flash_device = {
	.name		= "spi_gpio",
	.id		= 8,
	.num_resources  = 0,
	.dev		= {
			.platform_data = &(struct spi_gpio_platform_data) {
				.sck = stm_gpio(2, 0),
				.mosi = stm_gpio(2, 1),
				.miso = stm_gpio(2, 2),
				.num_chipselect = 1,
			},
	},
};

static int hdk7111_phy_reset(void *bus)
{
	gpio_set_value(HDK7111_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(HDK7111_PIO_PHY_RESET, 1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = hdk7111_phy_reset,
	.phy_mask = 0,
};

static struct stm_plat_pci_config hdk7111_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_DEFAULT,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = -EINVAL,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return stx7111_pcibios_map_platform_irq(&hdk7111_pci_config, pin);
}

static struct platform_device *hdk7111_devices[] __initdata = {
	&hdk7111_leds,
	&hdk7111_button_device,
	&hdk7111_physmap_device,
	&hdk7111_nand_flash_device,
	&hdk7111_serial_flash_device,
};

static int __init hdk7111_devices_init(void)
{
	struct sysconf_field *sc;
	u32 boot_mode;

	/* Configure FLASH devices */
	sc = sysconf_claim(SYS_STA, 1, 15, 16, "boot_mode");
	boot_mode = sysconf_read(sc);
	switch (boot_mode) {
	case 0x0:
		/* Boot-from-NOR: */
		/* NOR mapped to EMIA + EMIB (FMI_A26 = EMI_CSA#) */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		hdk7111_physmap_device.resource[0].start = 0x00000000;
		hdk7111_physmap_device.resource[0].end = emi_bank_base(1) - 1;
		hdk7111_nand_flash_data.csn = 1;
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		hdk7111_physmap_device.resource[0].start = emi_bank_base(1);
		hdk7111_physmap_device.resource[0].end = emi_bank_base(2) - 1;
		hdk7111_nand_flash_data.csn = 1;
		break;
	case 0x2:
		/* Boot-from-SPI */
		/* NOR mapped to EMIB, with physical offset of 0x06000000! */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		hdk7111_physmap_device.resource[0].start = emi_bank_base(1);
		hdk7111_physmap_device.resource[0].end = emi_bank_base(2) - 1;
		hdk7111_nand_flash_data.csn = 1;
		break;
	}

	spi_register_board_info(&hdk7111_serial_flash_board_info, 1);

	stx7111_configure_pci(&hdk7111_pci_config);

	stx7111_configure_pwm(&(struct stx7111_pwm_config) {
				.out0_enabled = 1,
				.out1_enabled = 0,
				});

	stx7111_configure_ssc_spi(0, NULL);
	stx7111_configure_ssc_i2c(1);
	stx7111_configure_ssc_i2c(2);
	stx7111_configure_ssc_i2c(3);

	stx7111_configure_usb(&(struct stx7111_usb_config) {
				.invert_ovrcur = 1,
				});

	gpio_request(HDK7111_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(HDK7111_PIO_PHY_RESET, 1);

	stx7111_configure_ethernet(&(struct stx7111_ethernet_config) {
				.mode = stx7111_ethernet_mode_mii,
				.ext_clk = 0,
				.phy_bus = 0,
				.phy_addr = 0,
				.mdio_bus_data = &stmmac_mdio_bus,
	      });

	stx7111_configure_lirc(&(struct stx7111_lirc_config) {
				.rx_mode = stx7111_lirc_rx_mode_ir,
				.tx_enabled = 1,
				.tx_od_enabled = 0,
				});


	return platform_add_devices(hdk7111_devices,
				    ARRAY_SIZE(hdk7111_devices));
}
arch_initcall(hdk7111_devices_init);

static void __iomem *hdk7111_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called.
	 */
	BUG();
	return (void __iomem *)CCN_PVR;
}

static void __init hdk7111_init_irq(void)
{
}

struct sh_machine_vector mv_hdk7111 __initmv = {
	.mv_name		= "STx7111 HDK",
	.mv_setup		= hdk7111_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= hdk7111_init_irq,
	.mv_ioport_map		= hdk7111_ioport_map,
};
