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
#include <linux/stm/fdma-reqs.h>
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
 * Audio subsystem components & platform devices
 */

/* Audio IO controls */

static struct platform_device audio_outputs = {
	.name          = "audio_outputs",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210200,
			.end   = 0x19210203,
		},
	},
};

/* Frequency synthesizer */

static struct platform_device fsynth = {
	.name          = "fsynth",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210000,
			.end   = 0x1921003f,
		},
	},
	.dev.platform_data = &(struct snd_stm_fsynth_info) {
		.channels_from = 0,
		.channels_to = 2,
	},
};

/* Internal DAC */

static struct platform_device conv_internal_dac = {
	.name          = "conv_internal_dac",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210100,
			.end   = 0x19210103,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_internal_dac_info) {
		.name = "Internal audio DAC",
		.card_id = "ANALOG",
		.card_device = 0,
		.source_bus_id = "pcm_player.1",
	},
};

/* PCM reader */

struct snd_stm_pcm_reader_info pcm_reader_info = {
	.name = "PCM reader",
	.card_id = "INPUT",
	.card_device = 0,
	.channels_num = 1,
	.channels = (int []) { 2 },
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
	.fdma_max_transfer_size = 2,
};

static struct platform_device pcm_reader = {
	.name          = "pcm_reader",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18102000,
			.end   = 0x18102027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = 146,
			.end   = 146,
		},
	},
	.dev.platform_data = &pcm_reader_info,
};

/* PCM players */

struct snd_stm_pcm_player_info pcm_player_0_info = {
	.name = "PCM player #0",
	.card_id = "PCM",
	.card_device = 0,
	.fsynth_bus_id = "fsynth",
	.fsynth_output = 0,
	/* .channels_num = see snd_stm_stx710x_init() */
	/* .channels = see snd_stm_stx710x_init() */
	/* .invert_sclk_edge_falling = see snd_stm_stx710x_init() */
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
	.fdma_max_transfer_size = 2,
};

static struct platform_device pcm_player_0 = {
	.name          = "pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18101000,
			.end   = 0x18101027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = 144,
			.end   = 144,
		},
	},
	.dev.platform_data = &pcm_player_0_info,
};

struct snd_stm_pcm_player_info pcm_player_1_info = {
	.name = "PCM player #1",
	.card_id = "ANALOG",
	.card_device = 0,
	.fsynth_bus_id = "fsynth",
	.fsynth_output = 1,
	/* .channels_num = see snd_stm_stx710x_init() */
	/* .channels = see snd_stm_stx710x_init() */
	/* .invert_sclk_edge_falling = see snd_stm_stx710x_init() */
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
	.fdma_max_transfer_size = 2,
};

static struct platform_device pcm_player_1 = {
	.name          = "pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18101800,
			.end   = 0x18101827,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = 145,
			.end   = 145,
		},
	},
	.dev.platform_data = &pcm_player_1_info,
};

/* SPDIF player */

struct snd_stm_spdif_player_info spdif_player_info = {
	.name = "SPDIF player",
	.card_id = "SPDIF",
	.card_device = 0,
	.fsynth_bus_id = "fsynth",
	.fsynth_output = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
	.fdma_max_transfer_size = 2,
};

static struct platform_device spdif_player = {
	.name          = "spdif_player",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18103000,
			.end   = 0x1810303f,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = 147,
			.end   = 147,
		},
	},
	.dev.platform_data = &spdif_player_info,
};

/* HDMI-connected I2S to SPDIF converter */

static struct platform_device conv_i2s_spdif = {
	.name          = "conv_i2s-spdif",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18103800,
			.end   = 0x18103a23,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = 142,
			.end   = 142,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_i2s_spdif_info) {
		.name = "I2S to SPDIF converter",
		.card_id = "PCM",
		.card_device = 0,
		.source_bus_id = "pcm_player.0",
	},
};



/*
 * Initialization and runtime configuration
 */

static struct platform_device *snd_stm_stx710x_devices[] = {
	&audio_outputs,
	&fsynth,
	&pcm_reader,
	&pcm_player_0,
	&pcm_player_1,
	&conv_internal_dac,
	&spdif_player,
	&conv_i2s_spdif,
};

static int channels_2[] = { 2 };
static int channels_10[] = { 10 };
static int channels_2_10[] = { 2, 4, 6, 8, 10 };

int __init snd_stm_stx710x_init(void)
{
	int result = 0;
	const char *soc_type;

	switch (cpu_data->type) {
	case CPU_STB7100:
		soc_type = "STx7100";

		/* FDMA request line configuration */
		pcm_player_0_info.fdma_request_line = STB7100_FDMA_REQ_PCM_0;
		pcm_player_1_info.fdma_request_line = STB7100_FDMA_REQ_PCM_1;
		spdif_player_info.fdma_request_line = STB7100_FDMA_REQ_SPDIF;
		pcm_reader_info.fdma_request_line = STB7100_FDMA_REQ_PCM_READ;

		/* STx7100 PCM players have small hardware bug - bit SCLK_EDGE
		 * in AUD_PCMOUT_FMT register has opposite meaning than stated
		 * in datasheet - 0 means that PCM serial output is clocked
		 * (changed) during falling SCLK edge (which is usually what
		 * we want ;-) */
		pcm_player_0_info.invert_sclk_edge_falling = 1;
		pcm_player_1_info.invert_sclk_edge_falling = 1;

		if (cpu_data->cut_major < 3) {
			/* STx7100 cut < 3.0 */
			/* Hardware bug again - in early 7100s player ignored
			 * NUM_CH setting in AUD_PCMOUT_FMT register */
			pcm_player_0_info.channels_num =
				ARRAY_SIZE(channels_10);
			pcm_player_0_info.channels = channels_10;
			pcm_player_1_info.channels_num =
				ARRAY_SIZE(channels_10);
			pcm_player_1_info.channels = channels_10;
		} else {
			/* STx7100 cut >= 3.0 */
			pcm_player_0_info.channels_num =
				ARRAY_SIZE(channels_2_10);
			pcm_player_0_info.channels = channels_2_10;
			pcm_player_1_info.channels_num = ARRAY_SIZE(channels_2);
			pcm_player_1_info.channels = channels_2;
		}
		break;

	case CPU_STB7109:
		soc_type = "STx7109";

		/* FDMA request line configuration */
		pcm_player_0_info.fdma_request_line = STB7109_FDMA_REQ_PCM_0;
		pcm_player_1_info.fdma_request_line = STB7109_FDMA_REQ_PCM_1;
		spdif_player_info.fdma_request_line = STB7109_FDMA_REQ_SPDIF;
		pcm_reader_info.fdma_request_line = STB7109_FDMA_REQ_PCM_READ;

		pcm_player_0_info.channels_num = ARRAY_SIZE(channels_2_10);
		pcm_player_0_info.channels = channels_2_10;
		pcm_player_1_info.channels_num = ARRAY_SIZE(channels_2);
		pcm_player_1_info.channels = channels_2;

		if (cpu_data->cut_major < 3) {
			/* STx7109 cut < 3.0 */
			/* PCM players of early 7109s have small hardware
			 * bug - bit SCLK_EDGE in AUD_PCMOUT_FMT register has
			 * opposite meaning than stated in datasheet - 0 means
			 * that PCM serial output is clocked (changed) during
			 * falling SCLK edge (which is usually what we
			 * want ;-) */
			pcm_player_0_info.invert_sclk_edge_falling = 1;
			pcm_player_1_info.invert_sclk_edge_falling = 1;
		}
		break;

	default:
		/* Unknown CPU! */
		snd_stm_printe("Not supported CPU detected!\n");
		result = -EINVAL;
		break;
	}

	if (result == 0)
		result = snd_stm_cards_init(soc_type,
				snd_stm_stx710x_cards,
				ARRAY_SIZE(snd_stm_stx710x_cards));

	if (result == 0) {
		result = snd_stm_add_plaform_devices(snd_stm_stx710x_devices,
				ARRAY_SIZE(snd_stm_stx710x_devices));

		if (result != 0)
			snd_stm_cards_free();
	}

	return result;
}

void __exit snd_stm_stx710x_cleanup(void)
{
	snd_stm_remove_plaform_devices(snd_stm_stx710x_devices,
			ARRAY_SIZE(snd_stm_stx710x_devices));
}
