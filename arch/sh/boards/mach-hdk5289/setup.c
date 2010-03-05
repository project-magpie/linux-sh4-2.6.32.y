/*
 * arch/sh/boards/mach-hdk5289/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx5289 reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/tm1668.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx5206.h>
#include <asm/irq.h>



#define HDK5289_GPIO_RST stm_gpio(2, 2)
#define HDK5289_POWER_ON stm_gpio(2, 3)



static void __init hdk5289_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK5289 board initialisation\n");

	stx5206_early_device_init();

	stx5206_configure_asc(2, &(struct stx5206_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
}



static struct tm1668_key hdk5289_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk5289_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk5289_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stm_gpio(3, 5),
		.gpio_sclk = stm_gpio(3, 4),
		.gpio_stb = stm_gpio(2, 7),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk5289_front_panel_keys),
		.keys = hdk5289_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk5289_front_panel_characters),
		.characters = hdk5289_front_panel_characters,
		.text = "5289",
	},
};



static struct plat_stmmacphy_data hdk5289_phy_plat_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device hdk5289_phy_device = {
	.name = "stmmacphy",
	.id = -1,
	.dev.platform_data = &hdk5289_phy_plat_data,
};



static struct platform_device *hdk5289_devices[] __initdata = {
	&hdk5289_front_panel,
	&hdk5289_phy_device,
};



static int __init hdk5289_devices_init(void)
{
	/* This board has a one PIO line used to reset almost everything:
	 * ethernet PHY, HDMI transmitter chip, PCI... */
	gpio_request(HDK5289_GPIO_RST, "GPIO_RST#");
	gpio_direction_output(HDK5289_GPIO_RST, 0);
	udelay(1000);
	gpio_set_value(HDK5289_GPIO_RST, 1);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK5289_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK5289_POWER_ON, 1);

	/* Internal I2C link */
	stx5206_configure_ssc_i2c(0);
	/* "FE": UC1 (LNB), NIM, JD2 */
	stx5206_configure_ssc_i2c(1);
	/* "AV": UI2 (EEPROM), UN1 (AV buffer & filter), HDMI,
	 *       JN4 (SCARD board connector) */
	stx5206_configure_ssc_i2c(3);

	stx5206_configure_usb();

	stx5206_configure_ethernet(&(struct stx5206_ethernet_config) {
			.mode = stx5206_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0, });

	stx5206_configure_lirc(&(struct stx5206_lirc_config) {
			.rx_mode = stx5206_lirc_rx_mode_ir, });

	return platform_add_devices(hdk5289_devices,
			ARRAY_SIZE(hdk5289_devices));
}
arch_initcall(hdk5289_devices_init);



static void __iomem *hdk5289_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might
	 * think.  I used to use external ROM, but that can cause
	 * problems if you are in the middle of updating Flash. So I'm
	 * now using the processor core version register, which is
	 * guaranteed to be available, and non-writable. */
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_hdk5289 __initmv = {
	.mv_name		= "hdk5289",
	.mv_setup		= hdk5289_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= hdk5289_ioport_map,
};
