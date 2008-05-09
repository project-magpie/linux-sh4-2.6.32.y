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
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#define COMPONENT conv_int_dac
#include "common.h"
#include "reg_audcfg_adac.h"



/*
 * Hardware-related definitions
 */

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 256



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_int_dac {
	/* Generic converter interface */
	struct snd_stm_conv conv;

	/* System informations */
	const char *bus_id;
	int ver; /* IP version, used by register access macros */

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

static unsigned int snd_stm_conv_int_dac_get_format(struct snd_stm_conv
		*conv)
{
	snd_stm_printd(1, "snd_stm_conv_int_dac_get_format(conv=%p)\n", conv);

	return FORMAT;
}

static int snd_stm_conv_int_dac_get_oversampling(struct snd_stm_conv *conv)
{
	snd_stm_printd(1, "snd_stm_conv_int_dac_get_oversampling(conv=%p)\n",
			conv);

	return OVERSAMPLING;
}

static int snd_stm_conv_int_dac_enable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_int_dac *conv_int_dac = container_of(conv,
			struct snd_stm_conv_int_dac, conv);

	snd_stm_printd(1, "snd_stm_conv_int_dac_enable(conv=%p)\n", conv);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);
	snd_assert(!conv_int_dac->enabled, return -EINVAL);

	snd_stm_printd(1, "Enabling DAC %s's digital part. (still muted)\n",
			conv_int_dac->bus_id);

	spin_lock(&conv_int_dac->status_lock);

	set__AUDCFG_ADAC_CTRL__NSB__NORMAL(conv_int_dac);
	set__AUDCFG_ADAC_CTRL__NRST__NORMAL(conv_int_dac);

	conv_int_dac->enabled = 1;

	spin_unlock(&conv_int_dac->status_lock);

	return 0;
}

static int snd_stm_conv_int_dac_disable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_int_dac *conv_int_dac = container_of(conv,
			struct snd_stm_conv_int_dac, conv);

	snd_stm_printd(1, "snd_stm_conv_int_dac_disable(conv=%p)\n", conv);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);
	snd_assert(conv_int_dac->enabled, return -EINVAL);

	snd_stm_printd(1, "Disabling DAC %s's digital part.\n",
			conv_int_dac->bus_id);

	spin_lock(&conv_int_dac->status_lock);

	set__AUDCFG_ADAC_CTRL__NRST__RESET(conv_int_dac);
	set__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(conv_int_dac);

	conv_int_dac->enabled = 0;

	spin_unlock(&conv_int_dac->status_lock);

	return 0;
}

static int snd_stm_conv_int_dac_mute(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_int_dac *conv_int_dac = container_of(conv,
			struct snd_stm_conv_int_dac, conv);

	snd_stm_printd(1, "snd_stm_conv_int_dac_mute(conv=%p)\n", conv);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);
	snd_assert(conv_int_dac->enabled, return -EINVAL);

	snd_stm_printd(1, "Muting DAC %s.\n", conv_int_dac->bus_id);

	spin_lock(&conv_int_dac->status_lock);

	conv_int_dac->muted_by_source = 1;
	if (!conv_int_dac->muted_by_user)
		set__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(conv_int_dac);

	spin_unlock(&conv_int_dac->status_lock);

	return 0;
}

static int snd_stm_conv_int_dac_unmute(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_int_dac *conv_int_dac = container_of(conv,
			struct snd_stm_conv_int_dac, conv);

	snd_stm_printd(1, "snd_stm_conv_int_dac_unmute(conv=%p)\n", conv);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);
	snd_assert(conv_int_dac->enabled, return -EINVAL);

	snd_stm_printd(1, "Unmuting DAC %s.\n", conv_int_dac->bus_id);

	spin_lock(&conv_int_dac->status_lock);

	conv_int_dac->muted_by_source = 0;
	if (!conv_int_dac->muted_by_user)
		set__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(conv_int_dac);

	spin_unlock(&conv_int_dac->status_lock);

	return 0;
}



/*
 * ALSA controls
 */

static int snd_stm_conv_int_dac_ctl_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
			snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_conv_int_dac_ctl_mute_get(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	spin_lock(&conv_int_dac->status_lock);

	ucontrol->value.integer.value[0] = !conv_int_dac->muted_by_user;

	spin_unlock(&conv_int_dac->status_lock);

	return 0;
}

static int snd_stm_conv_int_dac_ctl_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
			snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_conv_int_dac_ctl_mute_put(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	spin_lock(&conv_int_dac->status_lock);

	if (ucontrol->value.integer.value[0] !=
			!conv_int_dac->muted_by_user) {
		changed = 1;

		conv_int_dac->muted_by_user =
				!ucontrol->value.integer.value[0];

		if (conv_int_dac->enabled &&
				conv_int_dac->muted_by_user &&
				!conv_int_dac->muted_by_source)
			set__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(conv_int_dac);
		else if (conv_int_dac->enabled &&
				!conv_int_dac->muted_by_user &&
				!conv_int_dac->muted_by_source)
			set__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(conv_int_dac);
	}

	spin_unlock(&conv_int_dac->status_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_conv_int_dac_ctl_mute = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master Playback Switch",
	.info = snd_stm_ctl_boolean_info,
	.get = snd_stm_conv_int_dac_ctl_mute_get,
	.put = snd_stm_conv_int_dac_ctl_mute_put,
};



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_conv_int_dac_read_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
		entry->private_data;

	snd_assert(conv_int_dac, return);
	snd_stm_magic_assert(conv_int_dac, return);

	snd_iprintf(buffer, "AUDCFG_ADAC_CTRL (offset 0x00) = 0x%08x\n",
			get__AUDCFG_ADAC_CTRL(conv_int_dac));

	snd_iprintf(buffer, "enabled = %d\n", conv_int_dac->enabled);
	snd_iprintf(buffer, "muted_by_source = %d\n",
			conv_int_dac->muted_by_source);
	snd_iprintf(buffer, "muted_by_user = %d\n",
			conv_int_dac->muted_by_user);
}

static int snd_stm_conv_int_dac_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
			snd_device->device_data;

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);
	snd_assert(!conv_int_dac->enabled, return -EINVAL);

	/* Initialize DAC with digital part down, analog up and muted */

	conv_int_dac->enabled = 0;
	conv_int_dac->muted_by_source = 1;
	set__AUDCFG_ADAC_CTRL(conv_int_dac,
			mask__AUDCFG_ADAC_CTRL__NRST__RESET(conv_int_dac) |
			mask__AUDCFG_ADAC_CTRL__MODE__DEFAULT(conv_int_dac) |
			mask__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(conv_int_dac) |
			mask__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(conv_int_dac) |
			mask__AUDCFG_ADAC_CTRL__PDNANA__NORMAL(conv_int_dac) |
			mask__AUDCFG_ADAC_CTRL__PDNBG__NORMAL(conv_int_dac));

	/* Additional procfs info */

	snd_stm_info_register(&conv_int_dac->proc_entry,
			conv_int_dac->bus_id,
			snd_stm_conv_int_dac_read_info,
			conv_int_dac);

	return 0;
}

static int __exit snd_stm_conv_int_dac_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
			snd_device->device_data;

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);
	snd_assert(!conv_int_dac->enabled, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_int_dac->proc_entry);

	/* Global power done & mute mode */

	set__AUDCFG_ADAC_CTRL(conv_int_dac,
		mask__AUDCFG_ADAC_CTRL__NRST__RESET(conv_int_dac) |
		mask__AUDCFG_ADAC_CTRL__MODE__DEFAULT(conv_int_dac) |
		mask__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(conv_int_dac) |
		mask__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(conv_int_dac) |
		mask__AUDCFG_ADAC_CTRL__PDNANA__POWER_DOWN(conv_int_dac) |
		mask__AUDCFG_ADAC_CTRL__PDNBG__POWER_DOWN(conv_int_dac));

	return 0;
}

static struct snd_device_ops snd_stm_conv_int_dac_snd_device_ops = {
	.dev_register = snd_stm_conv_int_dac_register,
	.dev_disconnect = snd_stm_conv_int_dac_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_int_dac_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_int_dac_info *conv_int_dac_info =
			pdev->dev.platform_data;
	struct snd_stm_conv_int_dac *conv_int_dac;
	struct snd_card *card = snd_stm_card_get();
	int index;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	snd_assert(card != NULL, return -EINVAL);
	snd_assert(conv_int_dac_info != NULL, return -EINVAL);

	conv_int_dac = kzalloc(sizeof(*conv_int_dac), GFP_KERNEL);
	if (!conv_int_dac) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_int_dac);
	conv_int_dac->ver = conv_int_dac_info->ver;
	snd_assert(conv_int_dac->ver > 0, return -EINVAL);
	conv_int_dac->bus_id = pdev->dev.bus_id;
	spin_lock_init(&conv_int_dac->status_lock);

	/* Converter interface initialization */

	conv_int_dac->conv.name = conv_int_dac_info->name;
	conv_int_dac->conv.get_format =
			snd_stm_conv_int_dac_get_format;
	conv_int_dac->conv.get_oversampling =
			snd_stm_conv_int_dac_get_oversampling;
	conv_int_dac->conv.enable = snd_stm_conv_int_dac_enable;
	conv_int_dac->conv.disable = snd_stm_conv_int_dac_disable;
	conv_int_dac->conv.mute = snd_stm_conv_int_dac_mute;
	conv_int_dac->conv.unmute = snd_stm_conv_int_dac_unmute;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv_int_dac->mem_region,
			&conv_int_dac->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Get connections */

	snd_assert(conv_int_dac_info->source_bus_id != NULL,
			return -EINVAL);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			conv_int_dac_info->source_bus_id);
	index = snd_stm_conv_attach(&conv_int_dac->conv, &platform_bus_type,
			conv_int_dac_info->source_bus_id);
	if (index < 0) {
		snd_stm_printe("Can't attach to PCM player!\n");
		result = index;
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_int_dac,
			&snd_stm_conv_int_dac_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA control */

	snd_stm_conv_int_dac_ctl_mute.device =
			conv_int_dac_info->card_device;
	snd_stm_conv_int_dac_ctl_mute.index = index;
	result = snd_ctl_add(card,
			snd_ctl_new1(&snd_stm_conv_int_dac_ctl_mute,
			conv_int_dac));
	if (result < 0) {
		snd_stm_printe("Failed to add all ALSA control!\n");
		goto error_control;
	}

	/* Done now */

	platform_set_drvdata(pdev, &conv_int_dac->conv);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return 0;

error_control:
error_device:
error_attach:
	snd_stm_memory_release(conv_int_dac->mem_region,
			conv_int_dac->base);
error_memory_request:
	snd_stm_magic_clear(conv_int_dac);
	kfree(conv_int_dac);
error_alloc:
	return result;
}

static int snd_stm_conv_int_dac_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
			container_of(platform_get_drvdata(pdev),
			struct snd_stm_conv_int_dac, conv);

	snd_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	snd_stm_memory_release(conv_int_dac->mem_region,
			conv_int_dac->base);

	snd_stm_magic_clear(conv_int_dac);
	kfree(conv_int_dac);

	return 0;
}

static struct platform_driver snd_stm_conv_int_dac_driver = {
	.driver = {
		.name = "snd_conv_int_dac",
	},
	.probe = snd_stm_conv_int_dac_probe,
	.remove = snd_stm_conv_int_dac_remove,
};



/*
 * Initialization
 */

int __init snd_stm_conv_int_dac_init(void)
{
	return platform_driver_register(&snd_stm_conv_int_dac_driver);
}

void snd_stm_conv_int_dac_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_int_dac_driver);
}
