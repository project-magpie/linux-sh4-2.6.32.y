/*
 *   STMicrolectronics STx7100 SoC description
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

static struct snd_stm_card __initdata snd_stm_stx710x_cards[] = {
	{
		.index = 0,
		.id = "PCM",
		.short_name = "PCM output",
		.long_name = "Digital audio output (PCM player 0)",
	},
	{
		.index = 1,
		.id = "ANALOG",
		.short_name = "Analog output",
		.long_name = "Analog audio output (PCM player 1)",
	},
	{
		.index = 2,
		.id = "SPDIF",
		.short_name = "SPDIF output",
		.long_name = "SPDIF audio output",
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

enum { audio_outputs, fsynth, pcm_reader, dac_internal,
	pcm_player_0, pcm_player_1,
	spdif_player, i2s_spdif_converter };

static struct snd_stm_component __initdata snd_stm_stx710x_components[] = {

	/* Audio outputs control */

	[audio_outputs] = {
		.bus_id = "audio_outputs",
		.short_name = "Audio outputs control",
		.num_caps = 0,
	},

	/* Frequency synthesizer */

	[fsynth] = {
		.bus_id = "fsynth",
		.short_name = "Frequency synthesizer",
		.num_caps = 1,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				.value.range.from = 0,
				.value.range.to = 2,
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

	[dac_internal] = {
		.bus_id = "dac_internal",
		.short_name = "Internal DAC",
		.num_caps = 0,
	},

	/* PCM players */

	[pcm_player_0] = {
		.bus_id = "pcm_player.0",
		.short_name = "PCM player #0",
		.num_caps = 6,
		.caps = (struct snd_stm_cap[]) {
			{
				.name = "channels",
				/* SOC version dependant, see function below */
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
				.value.string = "fsynth",
			},
			{
				.name = "fsynth_channel",
				.value.number = 0,
			},
			{
				.name = "sclk_edge_falling",
				/* SOC version dependant, see function below */
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
				.value.list.numbers = (int []) { 2 },
				.value.list.len = 1,
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
				.value.string = "fsynth",
			},
			{
				.name = "fsynth_channel",
				.value.number = 1,
			},
			{
				.name = "dac_bus_id",
				.value.string = "dac_internal",
			},
		},
	},

	/* SPDIF player */

	[spdif_player] = {
		.bus_id = "spdif_player",
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
				.name         = "fsynth_bus_id",
				.value.string = "fsynth",
			},
			{
				.name         = "fsynth_channel",
				.value.number = 2,
			},
		},
	},

	/* HDMI output */

	[i2s_spdif_converter] = {
		.bus_id = "i2s-spdif_conv",
		.short_name = "I2S to SPDIF converter",
		.num_caps = 0,
	},
};



/*
 * Initialization and runtime configuration
 */

static union snd_stm_value __initdata number_0 = { .number = 0 };
static union snd_stm_value __initdata number_1 = { .number = 1 };
static union snd_stm_value __initdata list_2  = {
	.list.len = 1,
	.list.numbers = (int []) { 2 }
};
static union snd_stm_value __initdata list_2_10 = {
	.list.len = 5,
	.list.numbers = (int []) { 2, 4, 6, 8, 10 }
};

int __init snd_stm_stx710x_init(void)
{
	int result;
	const char *soc_type;
	/* To have lines shorter than 80 chars... */
	struct snd_stm_component *components = snd_stm_stx710x_components;

	switch (cpu_data->type) {
	case CPU_STB7100:
		soc_type = "STx7100";

		/* STx7100 PCM players have small hardware bug - bit SCLK_EDGE
		 * in AUD_PCMOUT_FMT register has opposite meaning than stated
		 * in datasheet - 0 means that PCM serial output is clocked
		 * (changed) during falling SCLK edge (which is usually what
		 * we want ;-) */
		snd_stm_cap_set(&components[pcm_player_0],
				"sclk_edge_falling", number_0);
		snd_stm_cap_set(&components[pcm_player_1],
				"sclk_edge_falling", number_0);

		if (cpu_data->cut_major < 3) {

			/* STx7100 cut < 3.0 */

			/* Hardware bug again - in early 7100s player ignored
			 * NUM_CH setting in AUD_PCMOUT_FMT register */
			snd_stm_cap_set(&components[pcm_player_0],
					"channels", list_2_10);
			snd_stm_cap_set(&components[pcm_player_1],
					"channels", list_2_10);
		} else {

			/* STx7100 cut >= 3.0 */

			snd_stm_cap_set(&components[pcm_player_0],
					"channels", list_2_10);
			snd_stm_cap_set(&components[pcm_player_1],
					"channels", list_2);
		}
		break;

	case CPU_STB7109:
		soc_type = "STx7109";

		snd_stm_cap_set(&components[pcm_player_0],
				"channels", list_2_10);
		snd_stm_cap_set(&components[pcm_player_1],
				"channels", list_2);

		if (cpu_data->cut_major < 3) {

			/* STx7109 cut < 3.0 */

			/* PCM players of early 7109s have small hardware bug -
			 * bit SCLK_EDGE in AUD_PCMOUT_FMT register has
			 * opposite meaning than stated in datasheet - 0 means
			 * that PCM serial output is clocked (changed) during
			 * falling SCLK edge (which is usually what we
			 * want ;-) */
			snd_stm_cap_set(&components[pcm_player_0],
					"sclk_edge_falling", number_0);
			snd_stm_cap_set(&components[pcm_player_1],
					"sclk_edge_falling", number_0);
		} else {

			/* STx7109 cut >= 3.0 */

			snd_stm_cap_set(&components[pcm_player_0],
					"sclk_edge_falling", number_1);
			snd_stm_cap_set(&components[pcm_player_1],
					"sclk_edge_falling", number_1);
		}

		break;

	default:
		/* Unknown CPU! */
		soc_type = NULL; /* To avoid a -Os compilation warning */
		snd_assert(0, return -EINVAL);
		break;
	}

	result = snd_stm_cards_init(soc_type,
			snd_stm_stx710x_cards,
			ARRAY_SIZE(snd_stm_stx710x_cards));

	if (result == 0) {
		result = snd_stm_components_init(snd_stm_stx710x_components,
				ARRAY_SIZE(snd_stm_stx710x_components));

		if (result < 0)
			snd_stm_cards_free();
	}

	return result;
}
