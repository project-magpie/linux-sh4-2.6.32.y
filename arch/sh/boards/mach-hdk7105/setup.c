/*
 * arch/sh/boards/mach-hdk7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HDK7105-SDK support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/gpio.h>
#include <linux/phy.h>
#include <linux/tm1668.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/pci-synopsys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <asm/irq-ilc.h>


#define HDK7105_PIO_PCI_SERR  stm_gpio(15, 4)
#define HDK7105_PIO_PHY_RESET stm_gpio(15, 5)
#define HDK7105_PIO_PCI_RESET stm_gpio(15, 7)



static void __init hdk7105_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7105 "
			"board initialisation\n");

	stx7105_early_device_init();

	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

/* PCI configuration */
static struct stm_plat_pci_config hdk7105_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* Modified in hdk7105_device_init() */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = HDK7105_PIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
        /* We can use the standard function on this board */
	return stx7105_pcibios_map_platform_irq(&hdk7105_pci_config, pin);
}

static struct platform_device hdk7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			/* The schematics actually describes these PIOs
			 * the other way round, but all tested boards
			 * had the bi-colour LED fitted like below... */
			{
				.name = "RED", /* This is also frontpanel LED */
				.gpio = stm_gpio(7, 0),
				.active_low = 1,
			}, {
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(7, 1),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key hdk7105_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7105_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7105_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(11, 2),
		.gpio_sclk = stm_gpio(11, 3),
		.gpio_stb = stm_gpio(11, 4),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7105_front_panel_keys),
		.keys = hdk7105_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7105_front_panel_characters),
		.characters = hdk7105_front_panel_characters,
		.text = "7105",
	},
};



static int hdk7105_phy_reset(void *bus)
{
	gpio_set_value(HDK7105_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(HDK7105_PIO_PHY_RESET, 1);

	return 1;
}

static struct stm_plat_stmmacphy_data hdk7105_phy_private_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &hdk7105_phy_reset,
};

static struct platform_device hdk7105_phy_device = {
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
		.platform_data = &hdk7105_phy_private_data,
	}
};



static struct platform_device *hdk7105_devices[] __initdata = {
	&hdk7105_leds,
	&hdk7105_front_panel,
	&hdk7105_phy_device,
};

static int __init hdk7105_device_init(void)
{
	/* Setup the PCI_SERR# PIO */
	if (gpio_request(HDK7105_PIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(HDK7105_PIO_PCI_SERR);
		hdk7105_pci_config.serr_irq =
				gpio_to_irq(HDK7105_PIO_PCI_SERR);
		set_irq_type(hdk7105_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "hdk7105: Failed to claim PCI SERR PIO!\n");
	}
	stx7105_configure_pci(&hdk7105_pci_config);

	stx7105_configure_sata();

	stx7105_configure_pwm(&(struct stx7105_pwm_config) {
			.out0 = stx7105_pwm_out0_pio13_0,
			.out1 = stx7105_pwm_out1_disabled, });

	/* I2C_xxxA - HDMI */
	stx7105_configure_ssc_i2c(0, &(struct stx7105_ssc_config) {
			.routing.ssc0.sclk = stx7105_ssc0_sclk_pio2_2,
			.routing.ssc0.mtsr = stx7105_ssc0_mtsr_pio2_3, });
	/* I2C_xxxB - JD3 (SCART switch on SCART board) */
	stx7105_configure_ssc_i2c(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6, });
	/* I2C_xxxC - JN1 (NIM), JN3, UT1 (CI chip), US2 (EEPROM) */
	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	/* I2C_xxxD - JN2 (NIM), JN4 */
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

	gpio_request(HDK7105_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(HDK7105_PIO_PHY_RESET, 1);

	stx7105_configure_ethernet(&(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0, });

	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
			.rx_mode = stx7105_lirc_rx_mode_ir,
			.tx_enabled = 0,
			.tx_od_enabled = 0, });

	return platform_add_devices(hdk7105_devices,
			ARRAY_SIZE(hdk7105_devices));
}
arch_initcall(hdk7105_device_init);

static void __iomem *hdk7105_ioport_map(unsigned long port, unsigned int size)
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

struct sh_machine_vector mv_hdk7105 __initmv = {
	.mv_name		= "hdk7105",
	.mv_setup		= hdk7105_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= hdk7105_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
