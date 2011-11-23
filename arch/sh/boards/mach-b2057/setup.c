/*
 * arch/sh/boards/mach-b2057/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
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
#include <linux/stm/platform.h>
#include <linux/stm/stxh205.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>

#define B2057_GPIO_POWER_ON_ETH		stm_gpio(2, 5)

static void __init b2057_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics B2057 board initialisation\n");

	stxh205_early_device_init();

	/* Socket JM5 DB9 connector */
	stxh205_configure_asc(STXH205_ASC(10), &(struct stxh205_asc_config) {
			.hw_flow_control = 0,
			.is_console = 1, });
	/*
	 * Header JK1 to UART daughter board (no h/w flow control) or
	 * socket JB7 to MoCA (with h/w flow control).
	 */
	stxh205_configure_asc(STXH205_ASC(1), &(struct stxh205_asc_config) {
			.hw_flow_control = 0, });
}

static struct platform_device b2057_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 2),
			}, {
				.name = "RED",
				.gpio = stm_gpio(3, 1),
			},
		},
	},
};

static int b2057_phy_reset(void *bus)
{
	gpio_set_value(B2057_GPIO_POWER_ON_ETH, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(B2057_GPIO_POWER_ON_ETH, 1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = b2057_phy_reset,
	.phy_mask = 0,
	.probed_phy_irq = ILC_IRQ(25), /* MDINT */
};

static struct platform_device *b2057_devices[] __initdata = {
	&b2057_leds,
};

static int __init device_init(void)
{
	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(B2057_GPIO_POWER_ON_ETH, "POWER_ON_ETH");
	gpio_direction_output(B2057_GPIO_POWER_ON_ETH, 0);

	stxh205_configure_ethernet(&(struct stxh205_ethernet_config) {
			.mode = stxh205_ethernet_mode_rmii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	stxh205_configure_usb(0);
	stxh205_configure_usb(1);

	return platform_add_devices(b2057_devices,
			ARRAY_SIZE(b2057_devices));
}
arch_initcall(device_init);

static void __iomem *b2057_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_b2057 __initmv = {
	.mv_name = "b2057",
	.mv_setup = b2057_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = b2057_ioport_map,
};
