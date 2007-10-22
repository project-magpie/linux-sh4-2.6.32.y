/*
 * linux/arch/sh/boards/st/mb411/led.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * This file contains code to drive the LED on the STb7100 Validation board.
 */

#include <asm/io.h>
#include <asm/led.h>
#include <asm/mb411/harp.h>

void mach_led(int position, int value)
{
	if (value) {
		ctrl_outb(EPLD_LED_ON, EPLD_POD_LED);
	} else {
		ctrl_outb(EPLD_LED_OFF, EPLD_POD_LED);
	}
}
