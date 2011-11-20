/*
 * arch/sh/boards/mach-b2039/setup.c
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

#define B2039_MII1_NOTRESET		stm_gpio(3, 0)

static void __init b2039_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics B2039 board initialisation\n");

	stxh205_early_device_init();

	/*
	 * UART0: CN43
	 * UART1: CN41
	 * UART2: CN29
	 * UART10: CN22
	 * UART11: CN21
	 */
	stxh205_configure_asc(STXH205_ASC(11), &(struct stxh205_asc_config) {
			/*
			 * Enabling hw flow control conflicts with FP_LED
			 * and keyscan.
			 */
			.hw_flow_control = 0,
			.is_console = 1, });
}

static struct platform_device b2039_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				/* Need to fit J29 1-2 and disable UART11 RTS */
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(3, 1),
			},
		},
	},
};

/* Neet to fit J35 1-2 for this, disable UART11 CTS and keyscan and sysclkin */
static int b2039_phy_reset(void *bus)
{
	gpio_set_value(B2039_MII1_NOTRESET, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(B2039_MII1_NOTRESET, 1);

	return 1;
}

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = b2039_phy_reset,
	.phy_mask = 0,
	.probed_phy_irq = ILC_IRQ(25), /* MDINT */
};

static struct platform_device *b2039_devices[] __initdata = {
	&b2039_leds,
};

static int __init device_init(void)
{
	gpio_request(B2039_MII1_NOTRESET, "MII1_NORESET");
	gpio_direction_output(B2039_MII1_NOTRESET, 0);

	stxh205_configure_ethernet(&(struct stxh205_ethernet_config) {
			.mode = stxh205_ethernet_mode_rmii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	/* Need to set J17 1-2 and J19 1-2 */
	stxh205_configure_usb(0);

	/* Need to set J12 1-2 and J22 1-2 */
	stxh205_configure_usb(1);

	return platform_add_devices(b2039_devices,
			ARRAY_SIZE(b2039_devices));
}
arch_initcall(device_init);

static void __iomem *b2039_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_b2039 __initmv = {
	.mv_name = "b2039",
	.mv_setup = b2039_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = b2039_ioport_map,
};
