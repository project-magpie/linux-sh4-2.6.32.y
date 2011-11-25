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
			 * keyscan and PWM10.
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
#ifdef CONFIG_STM_B2039_J35_PHY_RESET
	/*
	 * IC+ IP101 datasheet specifies 10mS low period and device
	 * usable 2.5mS after rising edge of notReset. However
	 * experimentally it appear 10mS is required for reliable
	 * functioning.
	 */
	gpio_set_value(B2039_MII1_NOTRESET, 0);
	mdelay(10);
	gpio_set_value(B2039_MII1_NOTRESET, 1);
	mdelay(10);
#endif

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
#ifdef CONFIG_STM_B2039_J35_PHY_RESET
	/* This conflicts with PWM10, keyscan and ASC11 */
	gpio_request(B2039_MII1_NOTRESET, "MII1_NORESET");
	gpio_direction_output(B2039_MII1_NOTRESET, 0);
#endif

#ifndef CONFIG_STM_B2039_CN14_NONE
	stxh205_configure_ethernet(&(struct stxh205_ethernet_config) {
#if defined(CONFIG_STM_B2039_CN14_B2032)
			.mode = stxh205_ethernet_mode_mii,
			.ext_clk = 1,
#elif defined(CONFIG_STM_B2039_CN14_B2035)
			.mode = stxh205_ethernet_mode_rmii,
			.ext_clk = 0,
#else
#error Unknown PHY daughterboard
#endif
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});
#endif

	/* Need to set J17 1-2 and J19 1-2 */
	stxh205_configure_usb(0);

	/* Need to set J12 1-2 and J22 1-2 */
	stxh205_configure_usb(1);

	/* 1: FRONTEND (NIM), CN19, HDMI */
	stxh205_configure_ssc_i2c(1, &(struct stxh205_ssc_config) {
			.routing.ssc1.sclk = stxh205_ssc1_sclk_pio4_6,
			.routing.ssc1.mtsr = stxh205_ssc1_mtsr_pio4_7, });
	/* 2: FRONTEND_EXT (VPAV), CN28 */
	stxh205_configure_ssc_i2c(2, &(struct stxh205_ssc_config) {
			.routing.ssc1.sclk = stxh205_ssc2_sclk_pio9_4,
			.routing.ssc1.mtsr = stxh205_ssc2_mtsr_pio9_5, });
	/* 3: BACKEND (GMII (CN14), CN10, CN37), CN27 */
	/* Fit jumpers J45 2-3, J46 2-3, J47 2-3 */
	stxh205_configure_ssc_i2c(3, &(struct stxh205_ssc_config) {
			.routing.ssc1.sclk = stxh205_ssc3_sclk_pio15_0,
			.routing.ssc1.mtsr = stxh205_ssc3_mtsr_pio15_1, });

	stxh205_configure_lirc(&(struct stxh205_lirc_config) {
#ifdef CONFIG_LIRC_STM_UHF
			.rx_mode = stxh205_lirc_rx_mode_uhf, });
#else
			.rx_mode = stxh205_lirc_rx_mode_ir, });
#endif

	stxh205_configure_pwm(&(struct stxh205_pwm_config) {
			/*
			 * 10: conflicts with SBC_SYS_CLKINALT, UART11 CTS
			 *    keyscan and MII1 notReset.
			 * 11: conflicts with ETH_RXCLK
			 */
			.out10_enabled = 0,
			.out11_enabled = 0, });

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
