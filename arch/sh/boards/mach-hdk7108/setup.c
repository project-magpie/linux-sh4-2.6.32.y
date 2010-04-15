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
#include <linux/stm/platform.h>
#include <linux/stm/stx7108.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



#define HDK7108_PIO_POWER_ON_ETHERNET stm_gpio(15, 4)
#define HDK7108_PIO_POWER_ON stm_gpio(4, 3)



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



static struct platform_device *hdk7108_devices[] __initdata = {
	&hdk7108_leds,
	&hdk7108_front_panel,
	&hdk7108_phy_devices[0],
	&hdk7108_phy_devices[1],
};



static int __init device_init(void)
{
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(HDK7108_PIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(HDK7108_PIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK7108_PIO_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK7108_PIO_POWER_ON, 1);

	/* Serial flash */
	stx7108_configure_ssc_spi(0, NULL);
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
};

