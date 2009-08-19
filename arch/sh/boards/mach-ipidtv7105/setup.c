/*
 * arch/sh/boards/mach-ipidtv7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics IPTV PLUGGIN board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/nand.h>
#include <linux/stm/pci-synopsys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/irq-ilc.h>

#define IPIDTV7105_PIO_PHY_RESET stm_gpio(15, 5)
#define IPIDTV7105_PIO_PCI_IDSEL stm_gpio(10, 2)
#define IPIDTV7105_PIO_FLASH_VPP stm_gpio(6, 5)

/*
 * Comment out this line to use NAND through the EMI bit-banging driver
 * instead of the Flex driver.
 */
#define NAND_USES_FLEX

static void __init ipidtv7105_setup(char **cmdline_p)
{
	printk("STMicroelectronics STx7105 IPTVPluggin board initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(0, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

static struct platform_device ipidtv7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "LD5",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(2, 4),
			},
			{
				.name = "LD6",
				.gpio = stm_gpio(2, 3),
			},
		},
	},
};

static int ipidtv7105_phy_reset(void *bus)
{
	gpio_set_value(IPIDTV7105_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(IPIDTV7105_PIO_PHY_RESET, 1);
	udelay(1);

	return 1;
}

static struct stm_plat_stmmacphy_data ipidtv7105_phy_private_data = {
	/* Micrel KSZ8041FTL */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &ipidtv7105_phy_reset,
};

static struct platform_device ipidtv7105_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/*FIXME, should be ILC_EXT_IRQ(6), */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &ipidtv7105_phy_private_data,
	}
};

/* Configuration for Serial Flash */
static struct mtd_partition serialflash_partitions[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

#if 0
static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p32",
};

static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias	= "m25p80",
		.bus_num	= 8,
		.chip_select	= stm_gpio(15, 2),
		.max_speed_hz	= 50000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};

static struct platform_device spi_pio_device[] = {
	{
		.name	   = "spi_st_pio",
		.id	     = 8,
		.num_resources  = 0,
		.dev	    = {
			.platform_data =
				&(struct ssc_pio_t) {
					.pio = {{15, 0}, {15, 1}, {15, 3} },
				},
		},
	},
};
#endif

/* Configuration for NAND Flash */
static struct mtd_partition nand_parts[] = {
	{
		.name   = "Boot firmware",
		.offset = 0,
		.size   = 0x00800000
	}, {
		.name   = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

struct stm_nand_bank_data nand_bank_data = {
	.csn		= 1,
	.nr_partitions	= ARRAY_SIZE(nand_parts),
	.partitions	= nand_parts,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.timing_data		= &(struct stm_nand_timing_data) {
		.sig_setup      = 50,	   /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay     = 50,	   /* in us */
	},
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

static struct platform_device *ipidtv7105_devices[] __initdata = {
	&ipidtv7105_leds,
	&ipidtv7105_phy_device,
#if 0
	&spi_pio_device[0],
#endif
#ifndef NAND_USES_FLEX
	&nand_device,
#endif
};

static struct stm_plat_pci_config pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_pio = stm_gpio(15, 7)
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return  stx7105_pcibios_map_platform_irq(&pci_config, pin);
}

static int __init ipidtv7105_devices_init(void)
{
	gpio_request(IPIDTV7105_PIO_PCI_IDSEL, "pci_idsel");
	gpio_direction_output(IPIDTV7105_PIO_PCI_IDSEL, 1);
	stx7105_configure_pci(&pci_config);

	stx7105_configure_sata();

	stx7105_configure_pwm(&(struct stx7105_pwm_config) {
			.out0 = stx7105_pwm_out0_pio13_0,
			.out1 = stx7105_pwm_out1_disabled, });

	stx7105_configure_ssc_i2c(0, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc0_sclk_pio2_2,
			.routing.ssc1.mtsr = stx7105_ssc0_mtsr_pio2_3, });
	stx7105_configure_ssc_i2c(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6, });
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	stx7105_configure_ssc_i2c(3, &(struct stx7105_ssc_config) {
			.routing.ssc3.sclk = stx7105_ssc3_sclk_pio3_6,
			.routing.ssc3.mtsr = stx7105_ssc3_mtsr_pio3_7, });

	stx7105_configure_usb(0, &(struct stx7105_usb_config) {
			.ovrcur_mode = stx7105_usb_ovrcur_active_high,
			.pwr_enabled = 1,
			.routing.usb0.ovrcur = stx7105_usb0_ovrcur_pio4_4,
			.routing.usb0.pwr = stx7105_usb0_pwr_pio4_5, });
	stx7105_configure_usb(1, &(struct stx7105_usb_config) {
			.ovrcur_mode = stx7105_usb_ovrcur_active_high,
			.pwr_enabled = 1,
			.routing.usb1.ovrcur = stx7105_usb1_ovrcur_pio4_6,
			.routing.usb1.pwr = stx7105_usb1_pwr_pio4_7, });

	gpio_request(IPIDTV7105_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(IPIDTV7105_PIO_PHY_RESET, 1);

	stx7105_configure_ethernet(&(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0, });

	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
			.rx_mode = stx7105_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	/* just permanetly enable the flash*/
	gpio_request(IPIDTV7105_PIO_FLASH_VPP, "eth_phy_reset");
	gpio_direction_output(IPIDTV7105_PIO_FLASH_VPP, 1);
#ifdef NAND_USES_FLEX
	stx7105_configure_nand_flex(1, &nand_bank_data, 1);
#endif

#if 0
	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));
#endif

	return platform_add_devices(ipidtv7105_devices,
				    ARRAY_SIZE(ipidtv7105_devices));
}
arch_initcall(ipidtv7105_devices_init);

static void __iomem *ipidtv7105_ioport_map(unsigned long port,
					   unsigned int size)
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

struct sh_machine_vector mv_ipidtv7105 __initmv = {
	.mv_name	= "ipidtv7105",
	.mv_setup	= ipidtv7105_setup,
	.mv_nr_irqs	= NR_IRQS,
	.mv_ioport_map	= ipidtv7105_ioport_map,
#ifdef CONFIG_SH_ST_SYNOPSYS_PCI
	STM_PCI_IO_MACHINE_VEC
#endif
};
