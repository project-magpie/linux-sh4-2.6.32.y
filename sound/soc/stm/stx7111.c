/*
 *   STMicrolectronics STx7111 SoC description
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
#include <linux/irq.h>
#include <sound/driver.h>
#include <sound/core.h>

#undef TRACE
#include "common.h"



/*
 * ALSA cards list and descriptions
 */

struct snd_stm_card __initdata snd_stm_stx7111_cards[] = {
	{
		.index = 0,
		.id = "PCM",
		.short_name = "PCM outputs",
		.long_name = "Digital audio outputs (PCM player 0)",
	},
	{
		.index = 1,
		.id = "ANALOG",
		.short_name = "Analog outputs",
		.long_name = "Analog audio outputs (PCM player 1)",
	},
	{
		.index = 2,
		.id = "SPDIF",
		.short_name = "SPDIF output",
		.long_name = "SPDIF audio output",
	},
	{
		.index = 3,
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
			.start = 0xfe210200,
			.end   = 0xfe21020b,
		},
	}
};

/* Frequency synthesizers */

static struct platform_device fsynth = {
	.name          = "fsynth",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfe210000,
			.end   = 0xfe21004f,
		},
	},
	.dev.platform_data = &(struct snd_stm_fsynth_info) {
		.channels_from = 0,
		.channels_to = 2,
	},
};

/* PCM reader */

#if 0 /* MB618 has no audio input, so there is no way to test it... */
static struct platform_device pcm_reader = {
	.name          = "pcm_reader",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd102000,
			.end   = 0xfd102027,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1440),
			.end   = evt2irq(0x1440),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM reader",
		.card_id = "INPUT",
		.card_device = 0,
		.channels_num = 5,
		.channels = (int []) { 2, 4, 6, 8, 10 },
		.fdma_initiator = 0,
		.fdma_request_line = 29,
		.fdma_max_transfer_size = 30,
	},
};
#endif

/* Internal DACs */

static struct platform_device conv_internal_dac = {
	.name          = "conv_internal_dac",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfe210100,
			.end   = 0xfe210103,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_internal_dac_info) {
		.name = "Internal audio DAC",
		.card_id = "ANALOG",
		.card_device = 0,
		.source_bus_id = "pcm_player.1",
	},
};

/* PCM players  */

static struct platform_device pcm_player_0 = {
	.name          = "pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104d00,
			.end   = 0xfd104d27,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1400),
			.end   = evt2irq(0x1400),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0",
		.card_id = "PCM",
		.card_device = 0,
		.fsynth_bus_id = "fsynth",
		.fsynth_output = 0,
		.channels_num = 4,
		.channels = (int []) { 2, 4, 6, 8 },
		.fdma_initiator = 0,
		.fdma_request_line = 27,
		.fdma_max_transfer_size = 30,
	},
};

static struct platform_device pcm_player_1 = {
	.name          = "pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd101800,
			.end   = 0xfd101827,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1420),
			.end   = evt2irq(0x1420),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		.card_id = "ANALOG",
		.card_device = 0,
		.fsynth_bus_id = "fsynth",
		.fsynth_output = 1,
		.channels_num = 1,
		.channels = (int []) { 2 },
		.fdma_initiator = 0,
		.fdma_request_line = 28,
		.fdma_max_transfer_size = 30,
	},
};

/*
 * SPDIF player
 */

static struct platform_device spdif_player = {
	.name          = "spdif_player",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104c00,
			.end   = 0xfd104c43,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1460),
			.end   = evt2irq(0x1460),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player",
		.card_id = "SPDIF",
		.card_device = 0,
		.fsynth_bus_id = "fsynth",
		.fsynth_output = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 30,
		.fdma_max_transfer_size = 20,
	},
};

/* I2S to SPDIF converters */

static struct platform_device conv_i2s_spdif_0 = {
	.name          = "conv_i2s-spdif",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105000,
			.end   = 0xfd105223,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x13c0),
			.end   = evt2irq(0x13c0),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2s_spdif_info) {
		.name = "I2S to SPDIF converter #0",
		.card_id = "PCM",
		.card_device = 0,
		.source_bus_id = "pcm_player.0",
		.full_channel_status = 1,
	},
};

static struct platform_device conv_i2s_spdif_1 = {
	.name          = "conv_i2s-spdif",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105400,
			.end   = 0xfd105623,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0a80),
			.end   = evt2irq(0x0a80),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2s_spdif_info) {
		.name = "I2S to SPDIF converter #1",
		.card_id = "PCM",
		.card_device = 0,
		.source_bus_id = "pcm_player.0",
		.full_channel_status = 1,
	},
};

static struct platform_device conv_i2s_spdif_2 = {
	.name          = "conv_i2s-spdif",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105800,
			.end   = 0xfd105a23,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0b00),
			.end   = evt2irq(0x0b00),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2s_spdif_info) {
		.name = "I2S to SPDIF converter #2",
		.card_id = "PCM",
		.card_device = 0,
		.source_bus_id = "pcm_player.0",
		.full_channel_status = 1,
	},
};

static struct platform_device conv_i2s_spdif_3 = {
	.name          = "conv_i2s-spdif",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105c00,
			.end   = 0xfd105e23,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0b20),
			.end   = evt2irq(0x0b20),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2s_spdif_info) {
		.name = "I2S to SPDIF converter #3",
		.card_id = "PCM",
		.card_device = 0,
		.source_bus_id = "pcm_player.0",
		.full_channel_status = 1,
	},
};



/*
 * Initialization and runtime configuration
 */

static struct platform_device *snd_stm_stx7111_devices[] = {
	&audio_outputs,
	&fsynth,
#if 0 /* MB618 has no audio input, so there is no way to test it... */
	&pcm_reader,
#endif
	&conv_internal_dac,
	&pcm_player_0,
	&pcm_player_1,
	&spdif_player,
	&conv_i2s_spdif_0,
	&conv_i2s_spdif_1,
	&conv_i2s_spdif_2,
	&conv_i2s_spdif_3,
};

int __init snd_stm_stx7111_init(void)
{
	int result = 0;

	/* TODO: 7111 is identified now as ST40-300... */
	if (cpu_data->type != CPU_ST40_300) {
		/* Unknown CPU! */
		snd_stm_printe("Not supported CPU detected!\n");
		result = -EINVAL;
	}

	/* Ugly but quick hack to have SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout = ioremap(0xfd104020, 4);
		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	if (result == 0) {
		/* Cut 2.0 presumably will bring something new into the
		 * matter, so above configuration must be checked!
		 * - transfer_sizes (FIFO sizes has changed) */
		WARN_ON(cpu_data->cut_major > 1);

		result = snd_stm_cards_init("STx7111",
				snd_stm_stx7111_cards,
				ARRAY_SIZE(snd_stm_stx7111_cards));
	}

	if (result == 0) {
		result = snd_stm_add_plaform_devices(snd_stm_stx7111_devices,
				ARRAY_SIZE(snd_stm_stx7111_devices));

		if (result != 0)
			snd_stm_cards_free();
	}

	return result;
}

void __exit snd_stm_stx7111_cleanup(void)
{
	snd_stm_remove_plaform_devices(snd_stm_stx7111_devices,
			ARRAY_SIZE(snd_stm_stx7111_devices));
}
