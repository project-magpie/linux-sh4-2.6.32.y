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
#include <linux/platform_device.h>
#include <linux/stm/fdma-reqs.h>
#include <asm/irq-ilc.h>
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
 * Audio subsystem components & platform devices
 */

/* Audio outputs control */

static struct platform_device audio_outputs = {
	.name          = "audio_outputs",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601200,
			.end   = 0xfd60120b,
		},
	}
};

/* Frequency synthesizers */

static struct platform_device fsynth_0 = {
	.name          = "fsynth",
	.id            = 0,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601000,
			.end   = 0xfd60104f,
		},
	},
	.dev.platform_data = &(struct snd_stm_fsynth_info) {
		.channels_from = 0,
		.channels_to = 3,
	},
};

static struct platform_device fsynth_1 = {
	.name          = "fsynth",
	.id            = 1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601100,
			.end   = 0xfd60114f,
		},
	},
	.dev.platform_data = &(struct snd_stm_fsynth_info) {
		.channels_from = 2,
		.channels_to = 3,
	},
};

/* PCM reader */

static struct platform_device pcm_reader = {
	.name          = "pcm_reader",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd100000,
			.end   = 0xfd100027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(38),
			.end   = ILC_IRQ(38),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM reader",
		.card_id = "INPUT",
		.card_device = 0,
		.channels_num = 1,
		.channels = (int []) { 2 },
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_PCMIN,
		.fdma_max_transfer_size = 2,
	},
};

/* Internal DACs */

static struct platform_device conv_internal_dac_0 = {
	.name          = "conv_internal_dac",
	.id            = 0,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601400,
			.end   = 0xfd601403,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_internal_dac_info) {
		.name = "Internal audio DAC #0",
		.card_id = "ANALOG",
		.card_device = 0,
		.source_bus_id = "pcm_player.0",
	},
};

static struct platform_device conv_internal_dac_1 = {
	.name          = "conv_internal_dac",
	.id            = 1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601500,
			.end   = 0xfd601503,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_internal_dac_info) {
		.name = "Internal audio DAC #1 (slave)",
		.card_id = "ANALOG",
		.card_device = 1,
		.source_bus_id = "pcm_player.1",
	},
};

/* PCM players connected to internal DACs */

static struct platform_device pcm_player_0 = {
	.name          = "pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd101000,
			.end   = 0xfd101027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(39),
			.end   = ILC_IRQ(39),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0",
		.card_id = "ANALOG",
		.card_device = 0,
		.fsynth_bus_id = "fsynth.0",
		.fsynth_output = 0,
		.channels_num = 1,
		.channels = (int []) { 2 },
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_PCM0,
		.fdma_max_transfer_size = 20,
	},
};

static struct platform_device pcm_player_1 = {
	.name          = "pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd102000,
			.end   = 0xfd102027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(40),
			.end   = ILC_IRQ(40),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		.card_id = "ANALOG",
		.card_device = 1,
		.fsynth_bus_id = "fsynth.0",
		.fsynth_output = 1,
		.channels_num = 3,
		.channels = (int []) { 2, 4, 6 },
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_PCM1,
		.fdma_max_transfer_size = 20,
	},
};

/* PCM players with digital outputs */

static struct platform_device pcm_player_2 = {
	.name          = "pcm_player",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd103000,
			.end   = 0xfd103027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(41),
			.end   = ILC_IRQ(41),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #2",
		.card_id = "PCM",
		.card_device = 0,
		.fsynth_bus_id = "fsynth.0",
		.fsynth_output = 2,
		.channels_num = 4,
		.channels = (int []) { 2, 4, 6, 8 },
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_PCM2,
		.fdma_max_transfer_size = 20,
	},
};

static struct platform_device pcm_player_3 = {
	.name          = "pcm_player",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104000,
			.end   = 0xfd104027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(42),
			.end   = ILC_IRQ(42),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #3",
		.card_id = "PCM",
		.card_device = 1,
		.fsynth_bus_id = "fsynth.0",
		.fsynth_output = 3,
		.channels_num = 5,
		.channels = (int []) { 2, 4, 6, 8, 10 },
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_PCM3,
		.fdma_max_transfer_size = 20,
	},
};

/* SPDIF player */

static struct platform_device spdif_player = {
	.name          = "spdif_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105000,
			.end   = 0xfd10503f,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(37),
			.end   = ILC_IRQ(37),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player",
		.card_id = "SPDIF",
		.card_device = 0,
		.fsynth_bus_id = "fsynth.1",
		.fsynth_output = 3,
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_SPDIF,
		.fdma_max_transfer_size = 4,
	},
};

/* HDMI output devices
 * Please note that "HDTVOutBaseAddress" (0xFD10C000) from page 54 of
 * "7200 Programming Manual, Volume 2" is wrong. The correct HDMI players
 * subsystem base address is "HDMIPlayerBaseAddress" (0xFD106000) from
 * page 488 of the manual. */

static struct platform_device hdmi_pcm_player = {
	.name          = "pcm_player",
	.id            = 4, /* HDMI PCM player is no. 4 */
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd106d00,
			.end   = 0xfd106d27,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(62),
			.end   = ILC_IRQ(62),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HDMI",
		.card_id = "HDMI",
		.card_device = 0,
		.fsynth_bus_id = "fsynth.1",
		.fsynth_output = 2,
		.channels_num = 4,
		.channels = (int []) { 2, 4, 6, 8 },
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_HDMI_PCM,
		.fdma_max_transfer_size = 20,
	},
};

static struct platform_device hdmi_spdif_player = {
	.name          = "spdif_player",
	.id            = 1, /* HDMI SPDIF player is no. 1 */
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd106c00,
			.end   = 0xfd106c3f,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(63),
			.end   = ILC_IRQ(63),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player HDMI",
		.card_id = "HDMI",
		.card_device = 1,
		.fsynth_bus_id = "fsynth.1",
		.fsynth_output = 2,
		.fdma_initiator = 0,
		.fdma_request_line = STB7200_FDMA_REQ_HDMI_SPDIF,
		.fdma_max_transfer_size = 4,
	},
};

#if 0 /* Disabled till cut 2.0 */
static struct platform_device hdmi_conv_i2s_spdif_0 = {
	.name          = "conv_i2s-spdif",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd107000,
			.end   = 0xfd107223,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(64),
			.end   = ILC_IRQ(64),
		}
	},
};

static struct platform_device hdmi_conv_i2s_spdif_1 = {
	.name          = "conv_i2s-spdif",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd107400,
			.end   = 0xfd107623,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(65),
			.end   = ILC_IRQ(65),
		}
	},
};

static struct platform_device hdmi_conv_i2s_spdif_2 = {
	.name          = "conv_i2s-spdif",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd107800,
			.end   = 0xfd107a23,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(66),
			.end   = ILC_IRQ(66),
		}
	},
};

static struct platform_device hdmi_conv_i2s_spdif_3 = {
	.name          = "conv_i2s-spdif",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd107c00,
			.end   = 0xfd107e23,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(67),
			.end   = ILC_IRQ(67),
		}
	},
};
#endif



/*
 * Initialization and runtime configuration
 */

static struct platform_device *snd_stm_stx7200_devices[] = {
	&audio_outputs,
	&fsynth_0,
	&fsynth_1,
	&pcm_reader,
	&conv_internal_dac_0,
	&conv_internal_dac_1,
	&pcm_player_0,
	&pcm_player_1,
	&pcm_player_2,
	&pcm_player_3,
	&spdif_player,
	&hdmi_pcm_player,
	&hdmi_spdif_player,
#if 0 /* Disabled till cut 2.0 */
	&hdmi_conv_i2s_spdif_0,
	&hdmi_conv_i2s_spdif_1,
	&hdmi_conv_i2s_spdif_2,
	&hdmi_conv_i2s_spdif_3,
#endif
};

int __init snd_stm_stx7200_init(void)
{
	int result = 0;

	if (cpu_data->type != CPU_STX7200) {
		/* Unknown CPU! */
		snd_stm_printe("Not supported CPU detected!\n");
		result = -EINVAL;
	}

	if (result == 0) {
		/* Cut 2.0 presumably will bring something new into the
		 * matter, so above configuration must be checked!
		 * - transfer_sizes (FIFO sizes has changed) */
		WARN_ON(cpu_data->cut_major > 1);

		result = snd_stm_cards_init("STx7200",
				snd_stm_stx7200_cards,
				ARRAY_SIZE(snd_stm_stx7200_cards));
	}

	if (result == 0) {
		result = snd_stm_add_plaform_devices(snd_stm_stx7200_devices,
				ARRAY_SIZE(snd_stm_stx7200_devices));

		if (result != 0)
			snd_stm_cards_free();
	}


	return result;
}

void __exit snd_stm_stx7200_cleanup(void)
{
	snd_stm_remove_plaform_devices(snd_stm_stx7200_devices,
			ARRAY_SIZE(snd_stm_stx7200_devices));
}
