/*
 *   STMicrolectronics STx7100 SoC description & audio glue driver
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

#define COMPONENT stx710x
#include "common.h"
#include "reg_710x_audcfg.h"



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx710x"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx710x audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx710x audio subsystem card.");



/*
 * Audio subsystem components & platform devices
 */

/* STx710x audio glue */

static struct platform_device stx710x_glue = {
	.name          = "snd_stx710x_glue",
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

static struct snd_stm_fsynth_info fsynth_info = {
	/* .ver = see snd_stm_stx710x_init() */
	.channels_from = 0,
	.channels_to = 2,
};

static struct platform_device fsynth = {
	.name          = "snd_fsynth",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210000,
			.end   = 0x1921003f,
		},
	},
	.dev.platform_data = &fsynth_info,
};

/* Internal DAC */

static struct snd_stm_conv_int_dac_info conv_int_dac_info = {
	/* .ver = see snd_stm_stx710x_init() */
	.source_bus_id = "snd_pcm_player.1",
	.channel_from = 0,
	.channel_to = 1,
};

static struct platform_device conv_int_dac = {
	.name          = "snd_conv_int_dac",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210100,
			.end   = 0x19210103,
		},
	},
	.dev.platform_data = &conv_int_dac_info,
};

/* PCM players */

struct snd_stm_pcm_player_info pcm_player_0_info = {
	.name = "PCM player #0 (HDMI)",
	/* .ver = see snd_stm_stx710x_init() */
	.card_device = 0,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 0,
	.channels = 10,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
};

static struct platform_device pcm_player_0 = {
	.name          = "snd_pcm_player",
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
	/* .ver = see snd_stm_stx710x_init() */
	.card_device = 1,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 1,
	.channels = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
};

static struct platform_device pcm_player_1 = {
	.name          = "snd_pcm_player",
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
	.name = "SPDIF player (HDMI)",
	/* .ver = see snd_stm_stx710x_init() */
	.card_device = 2,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
};

static struct platform_device spdif_player = {
	.name          = "snd_spdif_player",
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

static struct snd_stm_conv_i2sspdif_info conv_i2sspdif_info = {
	/* .ver = see snd_stm_stx710x_init() */
	.source_bus_id = "snd_pcm_player.0",
	.channel_from = 0,
	.channel_to = 1,
};

static struct platform_device conv_i2sspdif = {
	.name          = "snd_conv_i2sspdif",
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
	.dev.platform_data = &conv_i2sspdif_info,
};

/* PCM reader */

struct snd_stm_pcm_reader_info pcm_reader_info = {
	.name = "PCM Reader",
	/* .ver = see snd_stm_stx710x_init() */
	.card_device = 3,
	.channels = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx710x_init() */
};

static struct platform_device pcm_reader = {
	.name          = "snd_pcm_reader",
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

static struct platform_device *snd_stm_stx710x_devices[] = {
	&stx710x_glue,
	&fsynth,
	&pcm_player_0,
	&pcm_player_1,
	&conv_int_dac,
	&spdif_player,
	&conv_i2sspdif,
	&pcm_reader,
};



/*
 * Audio glue driver implementation
 */

struct snd_stm_stx710x_glue {
	int ver;

	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx710x_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx710x_glue *stx710x_glue = entry->private_data;

	snd_stm_assert(stx710x_glue, return);
	snd_stm_magic_assert(stx710x_glue, return);

	snd_iprintf(buffer, "--- snd_stx710x_glue ---\n");
	snd_iprintf(buffer, "base = 0x%p\n", stx710x_glue->base);

	snd_iprintf(buffer, "AUDCFG_IO_CTRL (offset 0x00) = 0x%08x\n",
			get__710X_AUDCFG_IO_CTRL(stx710x_glue));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx710x_glue_register(struct snd_device *snd_device)
{
	struct snd_stm_stx710x_glue *stx710x_glue = snd_device->device_data;

	snd_stm_assert(stx710x_glue, return -EINVAL);
	snd_stm_magic_assert(stx710x_glue, return -EINVAL);

	/* Enable audio outputs */

	set__710X_AUDCFG_IO_CTRL(stx710x_glue,
		mask__710X_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(stx710x_glue) |
		mask__710X_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(stx710x_glue) |
		mask__710X_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(stx710x_glue) |
		mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(stx710x_glue));

	/* Additional procfs info */

	snd_stm_info_register(&stx710x_glue->proc_entry, "stx710x_glue",
			snd_stm_stx710x_glue_dump_registers, stx710x_glue);

	return 0;
}

static int __exit snd_stm_stx710x_glue_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_stx710x_glue *stx710x_glue = snd_device->device_data;

	snd_stm_assert(stx710x_glue, return -EINVAL);
	snd_stm_magic_assert(stx710x_glue, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(stx710x_glue->proc_entry);

	/* Disable audio outputs */

	set__710X_AUDCFG_IO_CTRL(stx710x_glue,
		mask__710X_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(stx710x_glue) |
		mask__710X_AUDCFG_IO_CTRL__DATA1_EN__INPUT(stx710x_glue) |
		mask__710X_AUDCFG_IO_CTRL__DATA0_EN__INPUT(stx710x_glue) |
		mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(stx710x_glue));

	return 0;
}

static struct snd_device_ops snd_stm_stx710x_glue_snd_device_ops = {
	.dev_register = snd_stm_stx710x_glue_register,
	.dev_disconnect = snd_stm_stx710x_glue_disconnect,
};

static int __init snd_stm_stx710x_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx710x_glue *stx710x_glue;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	stx710x_glue = kzalloc(sizeof(*stx710x_glue), GFP_KERNEL);
	if (!stx710x_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx710x_glue);

	result = snd_stm_memory_request(pdev, &stx710x_glue->mem_region,
			&stx710x_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* ALSA component */

	result = snd_device_new(snd_stm_card_get(), SNDRV_DEV_LOWLEVEL,
			stx710x_glue, &snd_stm_stx710x_glue_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, stx710x_glue);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return result;

error_device:
	snd_stm_memory_release(stx710x_glue->mem_region, stx710x_glue->base);
error_memory_request:
	snd_stm_magic_clear(stx710x_glue);
	kfree(stx710x_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx710x_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx710x_glue *stx710x_glue =
			platform_get_drvdata(pdev);

	snd_stm_assert(stx710x_glue, return -EINVAL);
	snd_stm_magic_assert(stx710x_glue, return -EINVAL);

	snd_stm_memory_release(stx710x_glue->mem_region, stx710x_glue->base);

	snd_stm_magic_clear(stx710x_glue);
	kfree(stx710x_glue);

	return 0;
}

static struct platform_driver snd_stm_stx710x_glue_driver = {
	.driver = {
		.name = "snd_stx710x_glue",
	},
	.probe = snd_stm_stx710x_glue_probe,
	.remove = snd_stm_stx710x_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx710x_init(void)
{
	int result;
	const char *soc_type;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx710x_init()\n");

	switch (cpu_data->type) {
	case CPU_STB7100:
		soc_type = "STx7100";

		/* FDMA request line configuration */
		pcm_player_0_info.fdma_request_line = 26;
		pcm_player_1_info.fdma_request_line = 27;
		spdif_player_info.fdma_request_line = 29;
		pcm_reader_info.fdma_request_line = 28;

		/* IP versions */
		fsynth_info.ver = 1;
		pcm_reader_info.ver = 1;
		if (cpu_data->cut_major < 3) {
			/* STx7100 cut < 3.0 */
			pcm_player_0_info.ver = 1;
			pcm_player_1_info.ver = 1;
		} else {
			/* STx7100 cut >= 3.0 */
			pcm_player_0_info.ver = 2;
			pcm_player_1_info.ver = 2;
		}
		conv_int_dac_info.ver = 1;
		spdif_player_info.ver = 1;
		conv_i2sspdif_info.ver = 1;

		break;

	case CPU_STB7109:
		soc_type = "STx7109";

		/* FDMA request line configuration */
		pcm_player_0_info.fdma_request_line = 24;
		pcm_player_1_info.fdma_request_line = 25;
		spdif_player_info.fdma_request_line = 27;
		pcm_reader_info.fdma_request_line = 26;

		/* IP versions */
		fsynth_info.ver = 2;
		pcm_reader_info.ver = 2;
		if (cpu_data->cut_major < 3) {
			/* STx7109 cut < 3.0 */
			pcm_player_0_info.ver = 3;
			pcm_player_1_info.ver = 3;
		} else {
			/* STx7109 cut >= 3.0 */
			pcm_player_0_info.ver = 4;
			pcm_player_1_info.ver = 4;
		}
		conv_int_dac_info.ver = 2;
		spdif_player_info.ver = 2;
		conv_i2sspdif_info.ver = 2;

		break;

	default:
		snd_stm_printe("Not supported (other than STx7100 or STx7109)"
				" SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_stx710x_glue_driver);
	if (result != 0) {
		snd_stm_printe("Failed to register audio glue driver!\n");
		goto error_driver_register;
	}

	card = snd_stm_card_new(index, id, THIS_MODULE);
	if (card == NULL) {
		snd_stm_printe("ALSA card creation failed!\n");
		result = -ENOMEM;
		goto error_card_new;
	}
	strcpy(card->driver, soc_type);
	snprintf(card->shortname, 31, "%s audio subsystem", soc_type);
	snprintf(card->longname, 79, "STMicroelectronics %s cut %d.%d SOC "
			"audio subsystem", soc_type, cpu_data->cut_major,
			cpu_data->cut_minor);

	result = snd_stm_add_platform_devices(snd_stm_stx710x_devices,
			ARRAY_SIZE(snd_stm_stx710x_devices));
	if (result != 0) {
		snd_stm_printe("Failed to add platform devices!\n");
		goto error_add_devices;
	}

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	snd_stm_remove_platform_devices(snd_stm_stx710x_devices,
			ARRAY_SIZE(snd_stm_stx710x_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
	platform_driver_unregister(&snd_stm_stx710x_glue_driver);
error_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx710x_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx710x_exit()\n");

	snd_stm_card_free();

	snd_stm_remove_platform_devices(snd_stm_stx710x_devices,
			ARRAY_SIZE(snd_stm_stx710x_devices));

	platform_driver_unregister(&snd_stm_stx710x_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx710x audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx710x_init);
module_exit(snd_stm_stx710x_exit);
