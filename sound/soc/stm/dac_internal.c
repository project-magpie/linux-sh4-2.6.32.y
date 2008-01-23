/*
 *   STMicroelectronics System-on-Chips' internal audio DAC driver
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
#include <linux/list.h>
#include <linux/stm/soc.h>
#include <linux/stm/registers.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>

#undef TRACE   /* See common.h debug features */
#define MAGIC 3 /* See common.h debug features */
#include "common.h"



/*
 * Hardware-related definitions
 */

#define PCM_FORMAT \
		(PLAT_STM_AUDIO__FORMAT_I2S | \
		PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS | \
		PLAT_STM_AUDIO__DATA_SIZE_24_BITS)

#define OVERSAMPLING 256



/*
 * Audio DAC instance structure
 */

struct snd_stm_dac_internal {
	const char *bus_id;

	struct resource *mem_region;

	void *base;

	/* Master DAC - must be waked up before the slave is etc. */
	struct device *master;

	/* TODO: add "waked up" counter */

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};






/*
 * Audio DAC public interface implementation
 */

/* Gets PCM format required by DAC as described in <linux/stm/soc.h> */
int snd_stm_dac_internal_get_config(struct device *device,
		unsigned long *pcm_format, unsigned int *oversampling)
{
	struct snd_stm_dac_internal *dac_internal = dev_get_drvdata(device);

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	*pcm_format = PCM_FORMAT;
	*oversampling = OVERSAMPLING;

	return 0;
}

int snd_stm_dac_internal_wake_up(struct device *device)
{
	struct snd_stm_dac_internal *dac_internal = dev_get_drvdata(device);

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	if (dac_internal->master)
		snd_stm_dac_wake_up(dac_internal->master);

	snd_stm_printt("Waking up DAC '%s' (still muted)\n",
			dac_internal->bus_id);
	REGISTER_POKE(dac_internal->base, AUDCFG_ADAC_CTRL,
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NRST, NORMAL) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, MODE, DEFAULT) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NSB, NORMAL) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNANA, NORMAL) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNBG, NORMAL));

	return 0;
}

int snd_stm_dac_internal_shut_down(struct device *device)
{
	struct snd_stm_dac_internal *dac_internal = dev_get_drvdata(device);

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	if (dac_internal->master)
		snd_stm_dac_shut_down(dac_internal->master);

	snd_stm_printt("Setting DAC '%s' into reset mode.\n",
			dac_internal->bus_id);
	REGISTER_POKE(dac_internal->base, AUDCFG_ADAC_CTRL,
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NRST, RESET) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, MODE, DEFAULT) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NSB, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNANA, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNBG, POWER_DOWN));

	return 0;
}

int snd_stm_dac_internal_mute(struct device *device)
{
	struct snd_stm_dac_internal *dac_internal = dev_get_drvdata(device);

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	snd_stm_printt("Muting DAC '%s'.\n", dac_internal->bus_id);

	REGFIELD_SET(dac_internal->base, AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE);

	return 0;
}

int snd_stm_dac_internal_unmute(struct device *device)
{
	struct snd_stm_dac_internal *dac_internal = dev_get_drvdata(device);

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	snd_stm_printt("Unmuting DAC '%s'.\n", dac_internal->bus_id);

	REGFIELD_SET(dac_internal->base, AUDCFG_ADAC_CTRL, SOFTMUTE, NORMAL);

	return 0;
}



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_dac_internal_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_dac_internal *dac_internal = entry->private_data;

	snd_assert(dac_internal, return);
	snd_stm_magic_assert(dac_internal, return);

	snd_iprintf(buffer, "AUDCFG_ADAC_CTRL (offset 0x00) = 0x%08x\n",
			REGISTER_PEEK(dac_internal->base, AUDCFG_ADAC_CTRL));
}

static int snd_stm_dac_internal_register(struct snd_device *snd_device)
{
	struct snd_stm_dac_internal *dac_internal = snd_device->device_data;

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	/* Initialize DAC as muted and shut down */

	REGISTER_POKE(dac_internal->base, AUDCFG_ADAC_CTRL,
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NRST, RESET) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, MODE, DEFAULT) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NSB, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNANA, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNBG, POWER_DOWN));

	/* Additional procfs info */

	snd_stm_info_register(&dac_internal->proc_entry, dac_internal->bus_id,
			snd_stm_dac_internal_dump_registers, dac_internal);

	return 0;
}

static int snd_stm_dac_internal_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_dac_internal *dac_internal = snd_device->device_data;

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(dac_internal->proc_entry);

	/* Power done & mute mode, just to be sure :-) */

	REGISTER_POKE(dac_internal->base, AUDCFG_ADAC_CTRL,
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NRST, RESET) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, MODE, DEFAULT) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NSB, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNANA, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNBG, POWER_DOWN));

	return 0;
}

static struct snd_device_ops snd_stm_dac_internal_ops = {
	.dev_register = snd_stm_dac_internal_register,
	.dev_disconnect = snd_stm_dac_internal_disconnect,
};



/*
 * Platform driver routines
 */

static int __init snd_stm_dac_internal_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_component *component;
	struct snd_stm_dac_internal *dac_internal;
	const char *card_id;
	struct snd_card *card;
	const char *master_bus_id = NULL;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	component = snd_stm_components_get(pdev->dev.bus_id);
	snd_assert(component, return -EINVAL);

	dac_internal = kzalloc(sizeof(*dac_internal), GFP_KERNEL);
	if (!dac_internal) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(dac_internal);
	dac_internal->bus_id = pdev->dev.bus_id;

	result = snd_stm_memory_request(pdev, &dac_internal->mem_region,
			&dac_internal->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	result = snd_stm_cap_get_string(component, "card_id", &card_id);
	if (result == 0)
		card = snd_stm_cards_get(card_id);
	else
		card = snd_stm_cards_default(&card_id);
	snd_assert(card, return -EINVAL);
	snd_printd("DAC will be a member of a card '%s'\n", card_id);

	result = snd_stm_cap_get_string(component, "master_bus_id",
			&master_bus_id);
	if (result == 0) {
		dac_internal->master = snd_stm_device_get(master_bus_id);

		snd_assert(dac_internal->master, return -EINVAL);
		snd_printd("This DAC is %s's slave.\n", master_bus_id);
	}

	/* ALSA component */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, dac_internal,
			&snd_stm_dac_internal_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, dac_internal);

	snd_printd("--- Probed successfully!\n");

	return result;

error_device:
	snd_stm_memory_release(dac_internal->mem_region, dac_internal->base);
error_memory_request:
	snd_stm_magic_clear(dac_internal);
	kfree(dac_internal);
error_alloc:
	return result;
}

static int snd_stm_dac_internal_remove(struct platform_device *pdev)
{
	struct snd_stm_dac_internal *dac_internal = platform_get_drvdata(pdev);

	snd_assert(dac_internal, return -EINVAL);
	snd_stm_magic_assert(dac_internal, return -EINVAL);

	snd_stm_memory_release(dac_internal->mem_region, dac_internal->base);

	snd_stm_magic_clear(dac_internal);
	kfree(dac_internal);

	return 0;
}

static struct platform_driver snd_stm_dac_internal_driver = {
	.driver = {
		.name = "dac_internal",
	},
	.probe = snd_stm_dac_internal_probe,
	.remove = snd_stm_dac_internal_remove,
};



/*
 * Initialization
 */

int __init snd_stm_dac_internal_init(void)
{
	return platform_driver_register(&snd_stm_dac_internal_driver);
}

void snd_stm_dac_internal_cleanup(void)
{
	platform_driver_unregister(&snd_stm_dac_internal_driver);
}
