/*
 * linux/drivers/leds/leds-mb519.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This actually controls the heartbeat LED (LD12T) on the mb520 application
 * board, as the mb519 has no software controllable LEDs itself.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <asm/io.h>

static struct stpio_pin *led;

static void mb519_led_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	stpio_set_pin(led, !brightness);
}

static struct led_classdev mb519_led = {
	.name = "mb519-led",
	.brightness_set = mb519_led_set,
	.default_trigger = "heartbeat",
};

static int __init mb519_led_init(void)
{
	led = stpio_request_pin(4, 7, "LED", STPIO_OUT);
	if (led != NULL)
		led_classdev_register(NULL, &mb519_led);
}

static void __exit mb519_led_exit(void)
{
	led_classdev_unregister(&mb519_led);
}

module_init(mb519_led_init);
module_exit(mb519_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED support for STMicroelectronics mb519");
MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
