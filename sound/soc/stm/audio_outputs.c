/*
 *   STx7200 System-on-Chip audio outputs control driver
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
#include <linux/stm/soc.h>
#include <linux/stm/registers.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 1 /* See common.h debug features */
#include "common.h"



/*
 * Audio control instance structure
 */

struct snd_stm_audio_outputs {
	struct resource *mem_region;

	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_audio_outputs_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_audio_outputs *audio_outputs = entry->private_data;

	snd_assert(audio_outputs, return);
	snd_stm_magic_assert(audio_outputs, return);

#if defined(CONFIG_CPU_SUBTYPE_STB7100) || defined(CONFIG_CPU_SUBTYPE_STX7111)
	snd_iprintf(buffer, "AUDCFG_IO_CTRL (offset 0x00) = 0x%08x\n",
			REGISTER_PEEK(audio_outputs->base, AUDCFG_IO_CTRL));
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7200)
	snd_iprintf(buffer, "AUDCFG_IOMUX_CTRL (offset 0x00) = 0x%08x\n",
			REGISTER_PEEK(audio_outputs->base, AUDCFG_IOMUX_CTRL));
	snd_iprintf(buffer, "AUDCFG_HDMI_CTRL (offset 0x04) = 0x%08x\n",
			REGISTER_PEEK(audio_outputs->base, AUDCFG_HDMI_CTRL));
	snd_iprintf(buffer, "AUDCFG_RECOVERY_CTRL (offset 0x08) = 0x%08x\n",
			REGISTER_PEEK(audio_outputs->base,
			AUDCFG_RECOVERY_CTRL));
#endif
}

static int snd_stm_audio_outputs_register(struct snd_device *snd_device)
{
	struct snd_stm_audio_outputs *audio_outputs = snd_device->device_data;

	snd_assert(audio_outputs, return -EINVAL);
	snd_stm_magic_assert(audio_outputs, return -EINVAL);

	/* Enable audio outputs */

#if defined(CONFIG_CPU_SUBTYPE_STB7100)
	REGISTER_POKE(audio_outputs->base, AUDCFG_IO_CTRL,
			REGFIELD_VALUE(AUDCFG_IO_CTRL, SPDIF_EN, ENABLE) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, DATA1_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, DATA0_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, PCM_CLK_EN, OUTPUT));
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7111)
	REGISTER_POKE(audio_outputs->base, AUDCFG_IO_CTRL,
			REGFIELD_VALUE(AUDCFG_IO_CTRL, PCMPLHDMI_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, SPDIFHDMI_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, PCM_CLK_EN, OUTPUT));
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7200)
	REGISTER_POKE(audio_outputs->base, AUDCFG_IOMUX_CTRL,
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, SPDIF_EN, ENABLE) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, DATA2_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, DATA1_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, DATA0_EN, OUTPUT) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, PCM_CLK_EN, OUTPUT));
#endif

	/* Additional procfs info */

	snd_stm_info_register(&audio_outputs->proc_entry, "audio_outputs",
			snd_stm_audio_outputs_dump_registers, audio_outputs);

	return 0;
}

static int snd_stm_audio_outputs_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_audio_outputs *audio_outputs = snd_device->device_data;

	snd_assert(audio_outputs, return -EINVAL);
	snd_stm_magic_assert(audio_outputs, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(audio_outputs->proc_entry);

	/* Disable audio outputs */

#if defined(CONFIG_CPU_SUBTYPE_STB7100)
	REGISTER_POKE(audio_outputs->base, AUDCFG_IO_CTRL,
			REGFIELD_VALUE(AUDCFG_IO_CTRL, SPDIF_EN, DISABLE) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, DATA1_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, DATA0_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, PCM_CLK_EN, INPUT));
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7111)
	REGISTER_POKE(audio_outputs->base, AUDCFG_IO_CTRL,
			REGFIELD_VALUE(AUDCFG_IO_CTRL, PCMPLHDMI_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, SPDIFHDMI_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IO_CTRL, PCM_CLK_EN, INPUT));
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7200)
	REGISTER_POKE(audio_outputs->base, AUDCFG_IOMUX_CTRL,
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, SPDIF_EN, DISABLE) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, DATA2_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, DATA1_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, DATA0_EN, INPUT) |
			REGFIELD_VALUE(AUDCFG_IOMUX_CTRL, PCM_CLK_EN, INPUT));
#endif

	return 0;
}

static struct snd_device_ops snd_stm_audio_outputs_snd_device_ops = {
	.dev_register = snd_stm_audio_outputs_register,
	.dev_disconnect = snd_stm_audio_outputs_disconnect,
};



/*
 * Platform driver routines
 */

static int __init snd_stm_audio_outputs_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_audio_outputs *audio_outputs;
	struct snd_card *card;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	audio_outputs = kzalloc(sizeof(*audio_outputs), GFP_KERNEL);
	if (!audio_outputs) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(audio_outputs);

	result = snd_stm_memory_request(pdev, &audio_outputs->mem_region,
			&audio_outputs->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	card = snd_stm_cards_default();
	snd_assert(card, return -EINVAL);
	snd_printd("Audio output controls will be a member of a card '%s'\n",
		card->id);

	/* Register HDMI route control */

	/* TODO */


	/* ALSA component */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, audio_outputs,
			&snd_stm_audio_outputs_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, audio_outputs);

	snd_printd("--- Probed successfully!\n");

	return result;

error_device:
	snd_stm_memory_release(audio_outputs->mem_region, audio_outputs->base);
error_memory_request:
	snd_stm_magic_clear(audio_outputs);
	kfree(audio_outputs);
error_alloc:
	return result;
}

static int snd_stm_audio_outputs_remove(struct platform_device *pdev)
{
	struct snd_stm_audio_outputs *audio_outputs =
			platform_get_drvdata(pdev);

	snd_assert(audio_outputs, return -EINVAL);
	snd_stm_magic_assert(audio_outputs, return -EINVAL);

	snd_stm_memory_release(audio_outputs->mem_region, audio_outputs->base);

	snd_stm_magic_clear(audio_outputs);
	kfree(audio_outputs);

	return 0;
}

static struct platform_driver snd_stm_audio_outputs_driver = {
	.driver = {
		.name = "audio_outputs",
	},
	.probe = snd_stm_audio_outputs_probe,
	.remove = snd_stm_audio_outputs_remove,
};



/*
 * Initialization
 */

int __init snd_stm_audio_outputs_init(void)
{
	return platform_driver_register(&snd_stm_audio_outputs_driver);
}

void snd_stm_audio_outputs_cleanup(void)
{
	platform_driver_unregister(&snd_stm_audio_outputs_driver);
}
