/*
 *   STMicrolectronics System-on-Chips' audio subsystem driver
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
#include <linux/module.h>
#include <sound/driver.h>
#include <sound/core.h>

#undef TRACE
#include "common.h"

MODULE_AUTHOR("Pawel MOLL <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics System-on-Chips' audio subsystem");
MODULE_LICENSE("GPL");

static int __init alsa_card_stm_init(void)
{
	int result;

	snd_printd("=== STM ALSA driver is initializing...\n");

#ifdef CONFIG_CPU_SUBTYPE_STB7100
	result = snd_stm_stx710x_init();
#endif
#ifdef CONFIG_CPU_SUBTYPE_STX7200
	result = snd_stm_stx7200_init();
#endif
	if (result != 0)
		goto error_soc;

	result = snd_stm_info_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize procfs info entries!\n");
		goto error_info;
	}
	result = snd_stm_audio_outputs_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize audio outputs!\n");
		goto error_audio_outputs;
	}
	result = snd_stm_fsynth_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize frequency synthesizer!\n");
		goto error_fsynth;
	}
	result = snd_stm_dac_internal_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize internal DACs!\n");
		goto error_dac_internal;
	}
	result = snd_stm_synchro_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize synchronisation routines!\n");
		goto error_synchro;
	}
	result = snd_stm_i2s_spdif_converter_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize I2S to SPDIF converter!\n");
		goto error_i2s_spdif_converter;
	}
	result = snd_stm_pcm_player_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize PCM player!\n");
		goto error_pcm_player;
	}
	result = snd_stm_pcm_reader_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize PCM reader!\n");
		goto error_pcm_reader;
	}
	result = snd_stm_spdif_player_init();
	if (result != 0) {
		snd_stm_printe("Can't initialize SPDIF player!\n");
		goto error_spdif_player;
	}

	/* Cards should be created by SoC-specific initialization
	 * function (snd_stm_stxXXXX_init) */
	result = snd_stm_cards_register();
	if (result != 0) {
		snd_stm_printe("Can't register ALSA cards!\n");
		goto error_cards;
	}

	snd_printd("=== Success!\n");

	return result;

error_cards:
	snd_stm_spdif_player_cleanup();
error_spdif_player:
	snd_stm_pcm_reader_cleanup();
error_pcm_reader:
	snd_stm_pcm_player_cleanup();
error_pcm_player:
	snd_stm_i2s_spdif_converter_cleanup();
error_i2s_spdif_converter:
	snd_stm_synchro_cleanup();
error_synchro:
	snd_stm_dac_internal_cleanup();
error_dac_internal:
	snd_stm_fsynth_cleanup();
error_fsynth:
	snd_stm_audio_outputs_cleanup();
error_audio_outputs:
	snd_stm_info_cleanup();
error_info:
	snd_stm_cards_free();
error_soc:
	return result;
}

static void __exit alsa_card_stm_exit(void)
{
	snd_printd("=== STM ALSA driver cleanup.\n");

	snd_stm_cards_free();

	snd_stm_spdif_player_cleanup();
	snd_stm_pcm_reader_cleanup();
	snd_stm_pcm_player_cleanup();
	snd_stm_i2s_spdif_converter_cleanup();
	snd_stm_synchro_cleanup();
	snd_stm_dac_internal_cleanup();
	snd_stm_fsynth_cleanup();
	snd_stm_audio_outputs_cleanup();
	snd_stm_info_cleanup();
}

module_init(alsa_card_stm_init)
module_exit(alsa_card_stm_exit)
