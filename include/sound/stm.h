/*
 *   STMicrolectronics SoCs audio driver
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

#ifndef __SOUND_STM_H
#define __SOUND_STM_H

#include <sound/driver.h>
#include <sound/core.h>



/*
 * Converters (DAC, ADC, I2S to SPDIF, SPDIF to I2S, etc.)
 */

/* Link type (format) description
 * Please note, that 0 value means I2S with 32 bits per
 * subframe (channel) and is a default setting. */
#define SND_STM_FORMAT__I2S              0x00000000
#define SND_STM_FORMAT__LEFT_JUSTIFIED   0x00000001
#define SND_STM_FORMAT__RIGHT_JUSTIFIED  0x00000002
#define SND_STM_FORMAT__SPDIF            0x00000003
#define SND_STM_FORMAT__MASK             0x0000000f

/* Following values are valid only for I2S, Left Justified and
 * Right justified formats and can be bit-added to format;
 * they define size of one subframe (channel) transmitted.
 * For SPDIF the frame size is fixed and defined by standard. */
#define SND_STM_FORMAT__OUTPUT_SUBFRAME_32_BITS 0x00000000
#define SND_STM_FORMAT__OUTPUT_SUBFRAME_16_BITS 0x00000010
#define SND_STM_FORMAT__OUTPUT_SUBFRAME_MASK    0x000000f0

/* Converter handle */
struct snd_stm_conv {
	const char *name;

	/* Configuration */
	unsigned int (*get_format)(struct snd_stm_conv *conv);
	int (*get_oversampling)(struct snd_stm_conv *conv);

	/* Operations */
	int (*enable)(struct snd_stm_conv *conv);
	int (*disable)(struct snd_stm_conv *conv);
	int (*mute)(struct snd_stm_conv *conv);
	int (*unmute)(struct snd_stm_conv *conv);

	/* Master (must be enabled prior to this one) */
	struct snd_stm_conv *master;
};

int snd_stm_conv_attach(struct snd_stm_conv *conv, struct device *source);



/*
 * Generic conv implementations
 */

/* I2C-controlled DAC/ADC generic implementation
 *
 * Define a "struct i2c_board_info" with "struct snd_stm_conv_i2c_info"
 * as a platform data:
 *
 * static struct i2c_board_info external_dac __initdata = {
 * 	.driver_name = "snd_conv_i2c",
 * 	.type = "<i.e. chip model>",
 * 	.addr = <I2C address>
 * 	.platform_data = &(struct snd_stm_conv_i2c_info) {
 * 		<see below>
 * 	},
 * };
 *
 * and add it using:
 *
 * i2c_new_device(i2c_get_adapter(<i2c adapter (bus) id>), &external_dac);
 */

struct snd_stm_conv_i2c_info {
	const char *name;
	const char *card_id;

	const char *enable_cmd;
	int enable_cmd_len;
	const char *disable_cmd;
	int disable_cmd_len;
	const char *mute_cmd;
	int mute_cmd_len;
	const char *unmute_cmd;
	int unmute_cmd_len;
};

/* GPIO-controlled (STPIO interface) DAC/ADC generic implementation
 *
 * Define platform device named "snd_conv_stpio", pass
 * following structure as platform_data and add it in normal way :-) */

struct snd_stm_conv_stpio_info {
	const char *name;
	const char *card_id;

	struct stpio_pin *enable_pin;
	unsigned int enable_value;
	struct stpio_pin *disable_pin;
	unsigned int disable_value;
	struct stpio_pin *mute_pin;
	unsigned int mute_value;
	struct stpio_pin *unmute_pin;
	unsigned int unmute_value;
};

#endif
