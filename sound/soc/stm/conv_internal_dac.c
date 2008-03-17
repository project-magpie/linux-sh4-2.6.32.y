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
#include <linux/spinlock.h>
#include <linux/stm/soc.h>
#include <linux/stm/registers.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#undef TRACE   /* See common.h debug features */
#define MAGIC 3 /* See common.h debug features */
#include "common.h"



/*
 * Hardware-related definitions
 */

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__OUTPUT_SUBFRAME_32_BITS)
#define OVERSAMPLING 256



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_internal_dac {
	/* Generic converter interface */
	struct snd_stm_conv conv;

	/* System informations */
	const char *bus_id;

	/* Resources */
	struct resource *mem_region;
	void *base;

	/* Runtime data */
	int enabled;
	int muted_by_source;
	int muted_by_user;
	spinlock_t status_lock; /* Protects enabled & muted_by_* */

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_internal_dac_get_format(struct snd_stm_conv
		*conv)
{
	snd_stm_printt("snd_stm_conv_internal_dac_get_format(conv=%p)\n", conv);

	return FORMAT;
}

static int snd_stm_conv_internal_dac_get_oversampling(struct snd_stm_conv *conv)
{
	snd_stm_printt("snd_stm_conv_internal_dac_get_oversampling(conv=%p)\n",
			conv);

	return OVERSAMPLING;
}

static int snd_stm_conv_internal_dac_enable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac = container_of(conv,
			struct snd_stm_conv_internal_dac, conv);

	snd_stm_printt("snd_stm_conv_internal_dac_enable(conv=%p)\n", conv);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);
	snd_assert(!conv_internal_dac->enabled, return -EINVAL);

	snd_stm_printt("Enabling DAC %s's digital part. (still muted)\n",
			conv_internal_dac->bus_id);

	spin_lock(&conv_internal_dac->status_lock);

	REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL, NSB, NORMAL);
	REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL, NRST, NORMAL);

	conv_internal_dac->enabled = 1;

	spin_unlock(&conv_internal_dac->status_lock);

	return 0;
}

static int snd_stm_conv_internal_dac_disable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac = container_of(conv,
			struct snd_stm_conv_internal_dac, conv);

	snd_stm_printt("snd_stm_conv_internal_dac_disable(conv=%p)\n", conv);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);
	snd_assert(conv_internal_dac->enabled, return -EINVAL);

	snd_stm_printt("Disabling DAC %s's digital part.\n",
			conv_internal_dac->bus_id);

	spin_lock(&conv_internal_dac->status_lock);

	REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL, NRST, RESET);
	REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
			NSB, POWER_DOWN);

	conv_internal_dac->enabled = 0;

	spin_unlock(&conv_internal_dac->status_lock);

	return 0;
}

static int snd_stm_conv_internal_dac_mute(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac = container_of(conv,
			struct snd_stm_conv_internal_dac, conv);

	snd_stm_printt("snd_stm_conv_internal_dac_mute(conv=%p)\n", conv);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);
	snd_assert(conv_internal_dac->enabled, return -EINVAL);

	snd_stm_printt("Muting DAC %s.\n", conv_internal_dac->bus_id);

	spin_lock(&conv_internal_dac->status_lock);

	conv_internal_dac->muted_by_source = 1;
	if (!conv_internal_dac->muted_by_user)
		REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
				SOFTMUTE, MUTE);

	spin_unlock(&conv_internal_dac->status_lock);

	return 0;
}

static int snd_stm_conv_internal_dac_unmute(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac = container_of(conv,
			struct snd_stm_conv_internal_dac, conv);

	snd_stm_printt("snd_stm_conv_internal_dac_unmute(conv=%p)\n", conv);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);
	snd_assert(conv_internal_dac->enabled, return -EINVAL);

	snd_stm_printt("Unmuting DAC %s.\n", conv_internal_dac->bus_id);

	spin_lock(&conv_internal_dac->status_lock);

	conv_internal_dac->muted_by_source = 0;
	if (!conv_internal_dac->muted_by_user)
		REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
				SOFTMUTE, NORMAL);

	spin_unlock(&conv_internal_dac->status_lock);

	return 0;
}



/*
 * ALSA controls
 */

static int snd_stm_conv_internal_dac_ctl_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac =
			snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_conv_internal_dac_ctl_mute_get(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);

	spin_lock(&conv_internal_dac->status_lock);

	ucontrol->value.integer.value[0] = !conv_internal_dac->muted_by_user;

	spin_unlock(&conv_internal_dac->status_lock);

	return 0;
}

static int snd_stm_conv_internal_dac_ctl_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac =
			snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printt("snd_stm_conv_internal_dac_ctl_mute_put(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);

	spin_lock(&conv_internal_dac->status_lock);

	if (ucontrol->value.integer.value[0] !=
			!conv_internal_dac->muted_by_user) {
		changed = 1;

		conv_internal_dac->muted_by_user =
				!ucontrol->value.integer.value[0];

		if (conv_internal_dac->enabled &&
				conv_internal_dac->muted_by_user &&
				!conv_internal_dac->muted_by_source)
			REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
					SOFTMUTE, MUTE);
		else if (conv_internal_dac->enabled &&
				!conv_internal_dac->muted_by_user &&
				!conv_internal_dac->muted_by_source)
			REGFIELD_SET(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
					SOFTMUTE, NORMAL);
	}

	spin_unlock(&conv_internal_dac->status_lock);

	return changed;
}

static struct snd_kcontrol_new __initdata snd_stm_conv_dac_internal_ctl_mute = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master Playback Switch",
	.info = snd_stm_ctl_boolean_info,
	.get = snd_stm_conv_internal_dac_ctl_mute_get,
	.put = snd_stm_conv_internal_dac_ctl_mute_put,
};



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_conv_internal_dac_dump_registers(
		struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac =
		entry->private_data;

	snd_assert(conv_internal_dac, return);
	snd_stm_magic_assert(conv_internal_dac, return);

	snd_iprintf(buffer, "AUDCFG_ADAC_CTRL (offset 0x00) = 0x%08x\n",
			REGISTER_PEEK(conv_internal_dac->base,
			AUDCFG_ADAC_CTRL));
}

static int snd_stm_conv_internal_dac_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac =
			snd_device->device_data;

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);
	snd_assert(!conv_internal_dac->enabled, return -EINVAL);

	/* Initialize DAC with digital part down, analog up and muted */

	REGISTER_POKE(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NRST, RESET) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, MODE, DEFAULT) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NSB, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNANA, NORMAL) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNBG, NORMAL));

	/* Additional procfs info */

	snd_stm_info_register(&conv_internal_dac->proc_entry,
			conv_internal_dac->bus_id,
			snd_stm_conv_internal_dac_dump_registers,
			conv_internal_dac);

	return 0;
}

static int snd_stm_conv_internal_dac_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac =
			snd_device->device_data;

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);
	snd_assert(!conv_internal_dac->enabled, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_internal_dac->proc_entry);

	/* Global power done & mute mode */

	REGISTER_POKE(conv_internal_dac->base, AUDCFG_ADAC_CTRL,
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NRST, RESET) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, MODE, DEFAULT) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, NSB, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, SOFTMUTE, MUTE) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNANA, POWER_DOWN) |
			REGFIELD_VALUE(AUDCFG_ADAC_CTRL, PDNBG, POWER_DOWN));

	return 0;
}

static struct snd_device_ops snd_stm_conv_internal_dac_snd_device_ops = {
	.dev_register = snd_stm_conv_internal_dac_register,
	.dev_disconnect = snd_stm_conv_internal_dac_disconnect,
};



/*
 * Platform driver routines
 */

static int __init snd_stm_conv_internal_dac_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_internal_dac_info *conv_internal_dac_info =
			pdev->dev.platform_data;
	struct snd_stm_conv_internal_dac *conv_internal_dac;
	struct snd_card *card;
	struct device *player_device;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	snd_assert(conv_internal_dac_info != NULL, return -EINVAL);

	conv_internal_dac = kzalloc(sizeof(*conv_internal_dac), GFP_KERNEL);
	if (!conv_internal_dac) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_internal_dac);
	conv_internal_dac->bus_id = pdev->dev.bus_id;
	spin_lock_init(&conv_internal_dac->status_lock);

	/* Converter interface initialization */

	conv_internal_dac->conv.name = conv_internal_dac_info->name;
	conv_internal_dac->conv.get_format =
			snd_stm_conv_internal_dac_get_format;
	conv_internal_dac->conv.get_oversampling =
			snd_stm_conv_internal_dac_get_oversampling;
	conv_internal_dac->conv.enable = snd_stm_conv_internal_dac_enable;
	conv_internal_dac->conv.disable = snd_stm_conv_internal_dac_disable;
	conv_internal_dac->conv.mute = snd_stm_conv_internal_dac_mute;
	conv_internal_dac->conv.unmute = snd_stm_conv_internal_dac_unmute;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv_internal_dac->mem_region,
			&conv_internal_dac->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Get connections */

	snd_assert(conv_internal_dac_info->card_id, return -EINVAL);
	card = snd_stm_cards_get(conv_internal_dac_info->card_id);
	snd_assert(card, return -EINVAL);
	snd_printd("This DAC will be a member of a card '%s'.\n", card->id);

	snd_assert(conv_internal_dac_info->source_bus_id != NULL,
			return -EINVAL);
	snd_printd("This DAC is attached to PCM player '%s'.\n",
			conv_internal_dac_info->source_bus_id);
	player_device = snd_stm_find_device(NULL,
			conv_internal_dac_info->source_bus_id);
	snd_assert(player_device != NULL, return -EINVAL);
	result = snd_stm_conv_attach(&conv_internal_dac->conv, player_device);
	if (result < 0) {
		snd_stm_printe("Can't attach to PCM player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_internal_dac,
			&snd_stm_conv_internal_dac_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA control */

	snd_stm_conv_dac_internal_ctl_mute.device =
			conv_internal_dac_info->card_device;
	result = snd_ctl_add(card,
			snd_ctl_new1(&snd_stm_conv_dac_internal_ctl_mute,
			conv_internal_dac));
	if (result < 0) {
		snd_stm_printe("Failed to add all ALSA control!\n");
		goto error_control;
	}

	/* Done now */

	platform_set_drvdata(pdev, &conv_internal_dac->conv);

	snd_printd("--- Probed successfully!\n");

	return 0;

error_control:
error_device:
error_attach:
	snd_stm_memory_release(conv_internal_dac->mem_region,
			conv_internal_dac->base);
error_memory_request:
	snd_stm_magic_clear(conv_internal_dac);
	kfree(conv_internal_dac);
error_alloc:
	return result;
}

static int snd_stm_conv_internal_dac_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_internal_dac *conv_internal_dac =
			container_of(platform_get_drvdata(pdev),
			struct snd_stm_conv_internal_dac, conv);

	snd_assert(conv_internal_dac, return -EINVAL);
	snd_stm_magic_assert(conv_internal_dac, return -EINVAL);

	snd_stm_memory_release(conv_internal_dac->mem_region,
			conv_internal_dac->base);

	snd_stm_magic_clear(conv_internal_dac);
	kfree(conv_internal_dac);

	return 0;
}

static struct platform_driver snd_stm_conv_internal_dac_driver = {
	.driver = {
		.name = "conv_internal_dac",
	},
	.probe = snd_stm_conv_internal_dac_probe,
	.remove = snd_stm_conv_internal_dac_remove,
};



/*
 * Initialization
 */

int __init snd_stm_conv_internal_dac_init(void)
{
	return platform_driver_register(&snd_stm_conv_internal_dac_driver);
}

void snd_stm_conv_internal_dac_cleanup(void)
{
	platform_driver_unregister(&snd_stm_conv_internal_dac_driver);
}
