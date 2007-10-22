/*
 * linux/arch/sh/boards/mb374/led.c
 *
 * Copyright (C) 2000 Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * This file contains ST40RA/ST40STB1 Starter code.
 */

#include <linux/stm/pio.h>
#include <asm/io.h>
#include <asm/led.h>
#include <asm/mb374/harp.h>

void mach_led(int position, int value)
{
	static struct stpio_pin *led6 = NULL, *led7 = NULL;

	if(led6 == NULL) {
		led6 = stpio_request_pin(0,3, "LED", STPIO_OUT);
		led7 = stpio_request_pin(0,0, "LED", STPIO_OUT);
	}

	switch(position) {
	case 6:
		stpio_set_pin(led6, value);
		break;
	case 7:
		stpio_set_pin(led7, value);
		break;
	default:
		ctrl_outl(1<<position,(value) ? EPLD_LED_SET : EPLD_LED_CLR);
		break;
	}
}
