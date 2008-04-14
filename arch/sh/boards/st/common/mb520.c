/*
 * arch/sh/boards/st/common/mb520.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel MOLL <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STB peripherals board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <linux/bug.h>
#include <asm/processor.h>

static struct platform_device mb520_led = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(4, 7),
				.active_low = 1,
			},
		},
	},
};

static struct i2c_board_info pio_extender_ic23 = {
	I2C_BOARD_INFO("pcf857x", 0x27),
	.type = "pcf8575",
	.platform_data = &(struct pcf857x_platform_data) {
		.gpio_base = 200,
	},
};

static int __init device_init(void)
{
	struct sysconf_field *sc;

	/* So far valid only for 7200 processor board! */
	BUG_ON(cpu_data->type != CPU_STX7200);

	/* Heartbeat led */
	platform_device_register(&mb520_led);

	/* CONF_PAD_AUD[0] = 1
	 * AUDDIG* are connected PCMOUT3_* - 10-channels PCM player #3
	 * ("scenario 1", but only one channel is available) */
	sc = sysconf_claim(SYS_CFG, 20, 0, 0, "pcm_player.3");
	sysconf_write(sc, 1);

	/* I2C PIO extender (IC23), connected do SSC4 (third I2C device
	 * in case of MB519...) */
	i2c_register_board_info(2, &pio_extender_ic23, 1);

	return 0;
}
arch_initcall(device_init);
