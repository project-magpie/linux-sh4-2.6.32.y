/*
 * arch/sh/boards/mach-hmp7105/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Chris Smith <chris.smith@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HMP7105 support.
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
#include <linux/stm/pci-synopsys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <asm/irq-ilc.h>

#define HMP7105_PIO_PHY_RESET stm_gpio(11, 0)
#define HMP7105_PIO_NAND_ENABLE stm_gpio(10, 7)

/*
 * Comment out this line to use NAND through the EMI bit-banging driver
 * instead of the Flex driver.
 */
#define NAND_USES_FLEX

static void __init hmp7105_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HMP7105 board initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

static struct platform_device hmp7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 4,
		.leds = (struct gpio_led[]) {
			{
				.name = "AmberLED0",
				.gpio = stm_gpio(16, 1),
			},
			{
				.name = "AmberLED1",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(16, 2),
				.active_low = 1,
			},
			{
				.name = "RedLED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(16, 3),
				.active_low = 1,
			},
			{
				.name = "GreenLED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(16, 4),
				.active_low = 1,
			},
		},
	},
};

static int hmp7105_phy_reset(void *bus)
{
	gpio_set_value(HMP7105_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(HMP7105_PIO_PHY_RESET, 1);

	return 0;
}

static struct stm_plat_stmmacphy_data hmp7105_phy_private_data = {
	/* SMSC 8700 */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &hmp7105_phy_reset,
};

static struct platform_device hmp7105_phy_device = {
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
		.platform_data = &hmp7105_phy_private_data,
	}
};

static struct platform_device hmp7105_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 32*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
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

static struct stm_nand_bank_data nand_bank_data = {
	.csn		= 1,
	.nr_partitions	= ARRAY_SIZE(nand_parts),
	.partitions	= nand_parts,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.timing_data = &(struct stm_nand_timing_data) {
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

static struct platform_device *hmp7105_devices[] __initdata = {
	&hmp7105_leds,
	&hmp7105_physmap_flash,
	&hmp7105_phy_device,
#ifndef NAND_USES_FLEX
	&nand_device,
#endif
};

static struct stm_plat_pci_config pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_DEFAULT,
		[3] = PCI_PIN_DEFAULT
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

static int __init hmp7105_devices_init(void)
{
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

	gpio_request(HMP7105_PIO_PHY_RESET, "notPioResetMII");
	gpio_direction_output(HMP7105_PIO_PHY_RESET, 1);
	stx7105_configure_ethernet(&(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = 0, });

	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
			.rx_mode = stx7105_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	gpio_request(HMP7105_PIO_NAND_ENABLE, "NANDEnable");
	gpio_direction_output(HMP7105_PIO_NAND_ENABLE, 0);
#ifdef NAND_USES_FLEX
	stx7105_configure_nand_flex(1, &nand_bank_data, 1);
#endif

	return platform_add_devices(hmp7105_devices,
						ARRAY_SIZE(hmp7105_devices));
}
arch_initcall(hmp7105_devices_init);


static void __iomem *hmp7105_ioport_map(unsigned long port, unsigned int size)
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

struct sh_machine_vector mv_hmp7105 __initmv = {
	.mv_name		= "STx7105-HMP",
	.mv_setup		= hmp7105_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= hmp7105_ioport_map,
#ifdef CONFIG_SH_ST_SYNOPSYS_PCI
	STM_PCI_IO_MACHINE_VEC
#endif
};
