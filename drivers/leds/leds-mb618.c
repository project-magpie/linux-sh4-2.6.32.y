/*
 * linux/drivers/leds/leds-mb618.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <asm/io.h>

struct mb618_led {
	struct led_classdev cdev;
	struct stpio_pin *pio;
};

static void mb618_led_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct mb618_led *led_dev =
		container_of(led_cdev, struct mb618_led, cdev);
	stpio_set_pin(led_dev->pio, brightness);
}

static struct mb618_led mb618_leds[2] = {
	{
		.cdev = {
			.name = "mb618-led:green",
			.brightness_set = mb618_led_set,
			.default_trigger = "heartbeat",
		}
	}, {
		.cdev = {
			.name = "mb618-led:red",
			.brightness_set = mb618_led_set,
		}
	}
};

static int __init mb618_led_init(void)
{
	int i;
	for (i=0; i<ARRAY_SIZE(mb618_leds); i++) {
		mb618_leds[i].pio = stpio_request_set_pin(6, i, "LED",
							  STPIO_OUT, 0);
		if (mb618_leds[i].pio != NULL)
			led_classdev_register(NULL, &mb618_leds[i].cdev);
	}
}

static void __exit mb618_led_exit(void)
{
	int i;
	for (i=0; i<ARRAY_SIZE(mb618_leds); i++) {
		led_classdev_unregister(&mb618_leds[i].cdev);
	}
}

module_init(mb618_led_init);
module_exit(mb618_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED support for STMicroelectronics mb618");
MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
