/*
 *   STMicrolectronics STx7200 SoC description
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



/*
 * ALSA cards list and descriptions
 */

struct snd_stm_card __initdata snd_stm_stx7200_cards[] = {
	{
		.index = 0,
		.id = "PCM",
		.short_name = "PCM outputs",
		.long_name = "Digital audio outputs (PCM players 2 and 3)",
	},
	{
		.index = 1,
		.id = "ANALOG",
		.short_name = "Analog outputs",
		.long_name = "Analog audio outputs (PCM players 0 and 1)",
	},
	{
		.index = 2,
		.id = "SPDIF",
		.short_name = "SPDIF output",
		.long_name = "SPDIF audio output",
	},
	{
		.index = 3,
		.id = "HDMI",
		.short_name = "HDMI output",
		.long_name = "HDMI audio output (dedicated PCM "
				"and SPDIF players)",
	},
	{
		.index = 4,
		.id = "INPUT",
		.short_name = "PCM input",
		.long_name = "Digital audio input (PCM reader)",
	}
};



/*
 * SoC audio components description
 */

enum { audio_outputs, fsynth_0, fsynth_1, pcm_reader,
	dac_internal_0, dac_internal_1, pcm_player_0,
	pcm_player_1, pcm_player_2, pcm_player_3, spdif_player,
	hdmi_pcm_player, hdmi_spdif_player, hdmi_i2s_spdif_converter_0,
	hdmi_i2s_spdif_converter_1, hdmi_i2s_spdif_converter_2,
	hdmi_i2s_spdif_converter_3 };

struct snd_stm_component __initdata snd_stm_stx7200_components[] = {

	/* Audio outputs control */

	[audio_outputs] = {
		.bus_id = "audio_outputs",
		.short_name = "Audio outputs control",
		.num_caps = 0,
	},

	/* Frequency synthesizers */

	[fsynth_0] = {
		.bus_id = "fsynth.0",
		.short_name = "Frequency synthesizer #0",
		.num_caps = 1,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.range.from = 0,
				.value.range.to = 3,
			},
		},
	},

	[fsynth_1] = {
		.bus_id = "fsynth.1",
		.short_name = "Frequency synthesizer #1",
		.num_caps = 1,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.range.from = 2,
				.value.range.to = 3,
			},
		},
	},

	/* PCM reader */

	[pcm_reader] = {
		.bus_id = "pcm_reader",
		.short_name = "PCM reader",
		.num_caps = 3,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.list.numbers = (int []) { 2 },
				.value.list.len     = 1,
			},
			{
				.name = "card_id",
				.value.string = "INPUT",
			},
			{
				.name = "card_device",
				.value.number = 0,
			},
		},
	},

	/* Internal audio DACs */

	[dac_internal_0] = {
		.bus_id = "dac_internal.0",
		.short_name = "Internal DAC (master)",
		.num_caps = 0,
	},

	[dac_internal_1] = {
		.bus_id = "dac_internal.1",
		.short_name = "Internal DAC (slave)",
		.num_caps = 1,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "master_bus_id",
				.value.string = "dac_internal.0",
			},
		},
	},

	/* PCM players */

	[pcm_player_0] = {
		.bus_id = "pcm_player.0",
		.short_name = "PCM player #0",
		.num_caps = 6,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.list.numbers = (int []) { 2 },
				.value.list.len     = 1,
			},
			{
				.name = "card_id",
				.value.string = "ANALOG",
			},
			{
				.name = "card_device",
				.value.number = 0,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.0",
			},
			{
				.name = "fsynth_channel",
				.value.number = 0,
			},
			{
				.name = "dac_bus_id",
				.value.string = "dac_internal.0",
			},
		},
	},

	[pcm_player_1] = {
		.bus_id = "pcm_player.1",
		.short_name = "PCM player #1",
		.num_caps = 6,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.list.numbers = (int[]) { 2, 4, 6 },
				.value.list.len = 3,
			},
			{
				.name = "card_id",
				.value.string = "ANALOG",
			},
			{
				.name = "card_device",
				.value.number = 1,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.0",
			},
			{
				.name = "fsynth_channel",
				.value.number = 1,
			},
			{
				.name = "dac_bus_id",
				.value.string = "dac_internal.1",
			},
		},
	},

	[pcm_player_2] = {
		.bus_id = "pcm_player.2",
		.short_name = "PCM player #2",
		.num_caps = 5,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.list.numbers = (int[]) { 2, 4, 6, 8 },
				.value.list.len     = 4,
			},
			{
				.name = "card_id",
				.value.string = "PCM",
			},
			{
				.name = "card_device",
				.value.number = 0,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.0",
			},
			{
				.name = "fsynth_channel",
				.value.number = 2,
			},
		},
	},

	[pcm_player_3] = {
		.bus_id = "pcm_player.3",
		.short_name = "PCM player #3",
		.num_caps = 5,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.list.numbers = (int[]){ 2, 4, 6, 8, 10 },
				.value.list.len = 5,
			},
			{
				.name = "card_id",
				.value.string = "PCM",
			},
			{
				.name = "card_device",
				.value.number = 1,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.0",
			},
			{
				.name = "fsynth_channel",
				.value.number = 3,
			},
		},
	},

	/* SPDIF player */

	[spdif_player] = {
		.bus_id = "spdif_player.0",
		.short_name = "SPDIF player",
		.num_caps = 4,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "card_id",
				.value.string = "SPDIF",
			},
			{
				.name = "card_device",
				.value.number = 0,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.1",
			},
			{
				.name = "fsynth_channel",
				.value.number = 3,
			},
		},
	},

	/* HDMI players */

	[hdmi_pcm_player] = {
		.bus_id = "pcm_player.4",
		.short_name = "PCM player HDMI",
		.num_caps = 5,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.list.numbers = (int[]) { 2, 4, 6, 8 },
				.value.list.len     = 4,
			},
			{
				.name = "card_id",
				.value.string = "HDMI",
			},
			{
				.name = "card_device",
				.value.number = 0,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.1",
			},
			{
				.name = "fsynth_channel",
				.value.number = 2,
			},
		},
	},

	[hdmi_spdif_player] = {
		.bus_id = "spdif_player.1",
		.short_name = "SPDIF player HDMI",
		.num_caps = 4,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "card_id",
				.value.string = "HDMI",
			},
			{
				.name = "card_device",
				.value.number = 1,
			},
			{
				.name = "fsynth_bus_id",
				.value.string = "fsynth.1",
			},
			{
				.name = "fsynth_channel",
				.value.number = 2,
			},
		},
	},

	/* I2S to SPDIF converters (HDMI output) */

#if 0 /* Disabled till cut 2.0 */
	[hdmi_i2s_spdif_converter_0] = {
		.bus_id = "i2s-spdif_conv.0",
		.short_name = "I2S to SPDIF converter",
		.num_caps = 0,
	},

	[hdmi_i2s_spdif_converter_1] = {
		.bus_id = "i2s-spdif_conv.1",
		.short_name = "I2S to SPDIF converter",
		.num_caps = 0,
	},

	[hdmi_i2s_spdif_converter_2] = {
		.bus_id = "i2s-spdif_conv.2",
		.short_name = "I2S to SPDIF converter",
		.num_caps = 0,
	},

	[hdmi_i2s_spdif_converter_3] = {
		.bus_id = "i2s-spdif_conv.3",
		.short_name = "I2S to SPDIF converter",
		.num_caps = 0,
	},
#endif
};



/*
 * Initialization and runtime configuration
 */

int __init snd_stm_stx7200_init(void)
{
	int result = 0;

	result = snd_stm_cards_init("STx7200",
			snd_stm_stx7200_cards,
			ARRAY_SIZE(snd_stm_stx7200_cards));

	if (result == 0) {
		result = snd_stm_components_init(snd_stm_stx7200_components,
				ARRAY_SIZE(snd_stm_stx7200_components));

		if (result < 0)
			snd_stm_cards_free();
	}

	return result;
}
