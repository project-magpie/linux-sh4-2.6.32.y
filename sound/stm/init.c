/*
 *   STMicrolectronics System-on-Chips' audio subsystem core driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
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
#include <linux/module.h>
#include <sound/driver.h>
#include <sound/core.h>

#define COMPONENT init
#include "common.h"

/* General debug level */
#if defined(CONFIG_SND_STM_DEBUG_LEVEL)
static int debug = CONFIG_SND_STM_DEBUG_LEVEL;
module_param(debug, int, S_IRUGO | S_IWUSR);
int *snd_stm_debug_level = &debug;
EXPORT_SYMBOL(snd_stm_debug_level);
#endif

static int __init snd_stm_init(void)
{
	int result;

	snd_stm_printd(0, "snd_stm_init()\n");

	result = snd_stm_info_create();
	if (result != 0) {
		snd_stm_printe("Procfs info creation failed!\n");
		goto error_info;
	}
	result = snd_stm_fsynth_init();
	if (result != 0) {
		snd_stm_printe("Frequency synthesizer driver initialization"
				" failed!\n");
		goto error_fsynth;
	}
	result = snd_stm_conv_init();
	if (result != 0) {
		snd_stm_printe("Converters infrastructure initialization"
				" failed!\n");
		goto error_conv;
	}
	result = snd_stm_conv_int_dac_init();
	if (result != 0) {
		snd_stm_printe("Internal DACs driver initialization failed!\n");
		goto error_conv_int_dac;
	}
	result = snd_stm_conv_i2sspdif_init();
	if (result != 0) {
		snd_stm_printe("I2S to SPDIF converter driver initialization"
				" failed!\n");
		goto error_conv_i2sspdif;
	}
	result = snd_stm_pcm_player_init();
	if (result != 0) {
		snd_stm_printe("PCM player driver initialization failed!\n");
		goto error_pcm_player;
	}
	result = snd_stm_pcm_reader_init();
	if (result != 0) {
		snd_stm_printe("PCM reader driver initialization failed!\n");
		goto error_pcm_reader;
	}
	result = snd_stm_spdif_player_init();
	if (result != 0) {
		snd_stm_printe("SPDIF player driver initialization failed!\n");
		goto error_spdif_player;
	}

	return result;

error_spdif_player:
	snd_stm_pcm_reader_exit();
error_pcm_reader:
	snd_stm_pcm_player_exit();
error_pcm_player:
	snd_stm_conv_i2sspdif_exit();
error_conv_i2sspdif:
	snd_stm_conv_int_dac_exit();
error_conv_int_dac:
	snd_stm_conv_exit();
error_conv:
	snd_stm_fsynth_exit();
error_fsynth:
	snd_stm_info_dispose();
error_info:
	return result;
}

static void __exit snd_stm_exit(void)
{
	snd_stm_printd(0, "snd_stm_exit()\n");

	snd_stm_spdif_player_exit();
	snd_stm_pcm_reader_exit();
	snd_stm_pcm_player_exit();
	snd_stm_conv_i2sspdif_exit();
	snd_stm_conv_int_dac_exit();
	snd_stm_conv_exit();
	snd_stm_fsynth_exit();
	snd_stm_info_dispose();
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics System-on-Chips' audio core driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_init)
module_exit(snd_stm_exit)
