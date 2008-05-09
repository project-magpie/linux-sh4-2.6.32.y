/*
 *   STMicroelectronics System-on-Chips' I2C-controlled ADC/DAC driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Author: Pawel MOLL <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#define COMPONENT conv_i2c
#include "common.h"



/*
 * Implementation
 */

/* TODO */



/*
 * Initialization
 */

static int __init snd_stm_conv_i2c_init(void)
{
	return 0;
}

static void __exit snd_stm_conv_i2c_exit(void)
{
}

MODULE_AUTHOR("Pawel MOLL <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics I2C-controlled audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_i2c_init);
module_exit(snd_stm_conv_i2c_exit);
