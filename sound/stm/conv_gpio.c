/*
 *   STMicroelectronics System-on-Chips' GPIO-controlled ADC/DAC driver
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
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#define COMPONENT conv_gpio
#include "common.h"



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_gpio {
	/* Generic converter interface */
	struct snd_stm_conv conv;

	/* System informations */
	const char *bus_id;
	struct snd_stm_conv_gpio_info *info;

	/* Runtime data */
	int enabled;
	int muted_by_source;
	int muted_by_user;
	spinlock_t status_lock; /* Protects enabled & muted_by_* */
	int may_sleep;
	struct work_struct work; /* Used if may_sleep */
	int work_enable_value;
	int work_mute_value;
	spinlock_t work_lock; /* Protects work_*_value */

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Sleeping-safe GPIO access implementation
 */

static void snd_stm_conv_gpio_work(struct work_struct *work)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(work,
			struct snd_stm_conv_gpio, work);
	int enable_value, mute_value;

	snd_stm_printd(1, "snd_stm_conv_gpio_work(work=%p)\n", work);

	snd_assert(conv_gpio, return);
	snd_stm_magic_assert(conv_gpio, return);

	spin_lock(&conv_gpio->work_lock);

	enable_value = conv_gpio->work_enable_value;
	conv_gpio->work_enable_value = -1;

	mute_value = conv_gpio->work_mute_value;
	conv_gpio->work_mute_value = -1;

	spin_unlock(&conv_gpio->work_lock);

	if (enable_value != -1)
		gpio_set_value(conv_gpio->info->enable_gpio, enable_value);

	if (mute_value != -1)
		gpio_set_value(conv_gpio->info->mute_gpio, mute_value);
}

static void snd_stm_conv_gpio_set_value(struct snd_stm_conv_gpio *conv_gpio,
		int enable_not_mute, int value)
{
	snd_stm_printd(1, "snd_stm_conv_gpio_set_value(conv_gpio=%p, "
			"enable_not_mute=%d, value=%d)\n",
			conv_gpio, enable_not_mute, value);

	snd_assert(conv_gpio, return);
	snd_stm_magic_assert(conv_gpio, return);

	if (conv_gpio->may_sleep) {
		spin_lock(&conv_gpio->work_lock);
		if (enable_not_mute)
			conv_gpio->work_enable_value = value;
		else
			conv_gpio->work_mute_value = value;
		schedule_work(&conv_gpio->work);
		spin_unlock(&conv_gpio->work_lock);
	} else {
		gpio_set_value(enable_not_mute ? conv_gpio->info->enable_gpio :
				conv_gpio->info->mute_gpio, value);
	}
}



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_gpio_get_format(struct snd_stm_conv
		*conv)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(conv,
			struct snd_stm_conv_gpio, conv);

	snd_stm_printd(1, "snd_stm_conv_gpio_get_format(conv=%p)\n", conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);

	return conv_gpio->info->format;
}

static int snd_stm_conv_gpio_get_oversampling(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(conv,
			struct snd_stm_conv_gpio, conv);

	snd_stm_printd(1, "snd_stm_conv_gpio_get_oversampling(conv=%p)\n",
			conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);

	return conv_gpio->info->oversampling;
}

static int snd_stm_conv_gpio_enable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(conv,
			struct snd_stm_conv_gpio, conv);

	snd_stm_printd(1, "snd_stm_conv_gpio_enable(conv=%p)\n", conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);
	snd_assert(!conv_gpio->enabled, return -EINVAL);

	snd_stm_printd(1, "Enabling DAC %s's digital part. (still muted)\n",
			conv_gpio->bus_id);

	spin_lock(&conv_gpio->status_lock);

	snd_stm_conv_gpio_set_value(conv_gpio, 1,
			conv_gpio->info->enable_value);
	conv_gpio->enabled = 1;

	spin_unlock(&conv_gpio->status_lock);

	return 0;
}

static int snd_stm_conv_gpio_disable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(conv,
			struct snd_stm_conv_gpio, conv);

	snd_stm_printd(1, "snd_stm_conv_gpio_disable(conv=%p)\n", conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);
	snd_assert(conv_gpio->enabled, return -EINVAL);

	snd_stm_printd(1, "Disabling DAC %s's digital part.\n",
			conv_gpio->bus_id);

	spin_lock(&conv_gpio->status_lock);

	snd_stm_conv_gpio_set_value(conv_gpio, 1,
			!conv_gpio->info->enable_value);
	conv_gpio->enabled = 0;

	spin_unlock(&conv_gpio->status_lock);

	return 0;
}

static int snd_stm_conv_gpio_mute(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(conv,
			struct snd_stm_conv_gpio, conv);

	snd_stm_printd(1, "snd_stm_conv_gpio_mute(conv=%p)\n", conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);
	snd_assert(conv_gpio->enabled, return -EINVAL);

	if (conv_gpio->info->mute_supported) {
		snd_stm_printd(1, "Muting DAC %s.\n", conv_gpio->bus_id);

		spin_lock(&conv_gpio->status_lock);

		conv_gpio->muted_by_source = 1;
		if (!conv_gpio->muted_by_user)
			snd_stm_conv_gpio_set_value(conv_gpio, 0,
					conv_gpio->info->mute_value);

		spin_unlock(&conv_gpio->status_lock);
	}

	return 0;
}

static int snd_stm_conv_gpio_unmute(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_gpio *conv_gpio = container_of(conv,
			struct snd_stm_conv_gpio, conv);

	snd_stm_printd(1, "snd_stm_conv_gpio_unmute(conv=%p)\n", conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);
	snd_assert(conv_gpio->enabled, return -EINVAL);

	if (conv_gpio->info->mute_supported) {
		snd_stm_printd(1, "Unmuting DAC %s.\n", conv_gpio->bus_id);

		spin_lock(&conv_gpio->status_lock);

		conv_gpio->muted_by_source = 0;
		if (!conv_gpio->muted_by_user)
			snd_stm_conv_gpio_set_value(conv_gpio, 0,
					!conv_gpio->info->mute_value);

		spin_unlock(&conv_gpio->status_lock);
	}

	return 0;
}



/*
 * ALSA controls
 */

static int snd_stm_conv_gpio_ctl_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_gpio *conv_gpio =
			snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_conv_gpio_ctl_mute_get(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);

	spin_lock(&conv_gpio->status_lock);

	ucontrol->value.integer.value[0] = !conv_gpio->muted_by_user;

	spin_unlock(&conv_gpio->status_lock);

	return 0;
}

static int snd_stm_conv_gpio_ctl_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_gpio *conv_gpio =
			snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_conv_gpio_ctl_mute_put(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);

	spin_lock(&conv_gpio->status_lock);

	if (ucontrol->value.integer.value[0] !=
			!conv_gpio->muted_by_user) {
		changed = 1;

		conv_gpio->muted_by_user =
				!ucontrol->value.integer.value[0];

		if (conv_gpio->enabled &&
				conv_gpio->muted_by_user &&
				!conv_gpio->muted_by_source)
			snd_stm_conv_gpio_set_value(conv_gpio, 0,
					conv_gpio->info->mute_value);
		else if (conv_gpio->enabled &&
				!conv_gpio->muted_by_user &&
				!conv_gpio->muted_by_source)
			snd_stm_conv_gpio_set_value(conv_gpio, 0,
					!conv_gpio->info->mute_value);
	}

	spin_unlock(&conv_gpio->status_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_conv_gpio_ctl_mute = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master Playback Switch",
	.info = snd_stm_ctl_boolean_info,
	.get = snd_stm_conv_gpio_ctl_mute_get,
	.put = snd_stm_conv_gpio_ctl_mute_put,
};



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_conv_gpio_read_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_gpio *conv_gpio =
		entry->private_data;

	snd_assert(conv_gpio, return);
	snd_stm_magic_assert(conv_gpio, return);

	snd_iprintf(buffer, "enable_gpio(%d) = %d\n",
			conv_gpio->info->enable_gpio,
			gpio_get_value(conv_gpio->info->enable_gpio));
	if (conv_gpio->info->mute_supported)
		snd_iprintf(buffer, "mute_gpio(%d) = %d\n",
				conv_gpio->info->mute_gpio,
				gpio_get_value(conv_gpio->info->mute_gpio));

	snd_iprintf(buffer, "enabled = %d\n", conv_gpio->enabled);
	snd_iprintf(buffer, "muted_by_source = %d\n",
			conv_gpio->muted_by_source);
	snd_iprintf(buffer, "muted_by_user = %d\n", conv_gpio->muted_by_user);
}

static int snd_stm_conv_gpio_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_gpio *conv_gpio =
			snd_device->device_data;

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);
	snd_assert(!conv_gpio->enabled, return -EINVAL);

	/* Initialize DAC disabled and mute */

	conv_gpio->enabled = 0;
	conv_gpio->muted_by_source = 1;
	gpio_set_value(conv_gpio->info->enable_gpio,
			!conv_gpio->info->enable_value);
	if (conv_gpio->info->mute_supported)
		gpio_set_value(conv_gpio->info->mute_gpio,
				conv_gpio->info->mute_value);

	/* Additional procfs info */

	snd_stm_info_register(&conv_gpio->proc_entry,
			conv_gpio->bus_id,
			snd_stm_conv_gpio_read_info,
			conv_gpio);

	return 0;
}

static int snd_stm_conv_gpio_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_gpio *conv_gpio =
			snd_device->device_data;

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);
	snd_assert(!conv_gpio->enabled, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_gpio->proc_entry);

	/* Muting and disabling - just to be sure ;-) */

	if (conv_gpio->info->mute_supported)
		gpio_set_value(conv_gpio->info->mute_gpio,
				conv_gpio->info->mute_value);
	gpio_set_value(conv_gpio->info->enable_gpio,
			!conv_gpio->info->enable_value);

	return 0;
}

static struct snd_device_ops snd_stm_conv_gpio_snd_device_ops = {
	.dev_register = snd_stm_conv_gpio_register,
	.dev_disconnect = snd_stm_conv_gpio_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_gpio_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_gpio *conv_gpio;
	struct snd_card *card = snd_stm_card_get();
	int index;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	snd_assert(card, return -EINVAL);
	snd_assert(pdev->dev.platform_data != NULL, return -EINVAL);

	conv_gpio = kzalloc(sizeof(*conv_gpio), GFP_KERNEL);
	if (!conv_gpio) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_gpio);
	conv_gpio->bus_id = pdev->dev.bus_id;
	conv_gpio->info = pdev->dev.platform_data;
	spin_lock_init(&conv_gpio->status_lock);

	/* Converter interface initialization */

	conv_gpio->conv.name = conv_gpio->info->name;
	conv_gpio->conv.get_format =
			snd_stm_conv_gpio_get_format;
	conv_gpio->conv.get_oversampling =
			snd_stm_conv_gpio_get_oversampling;
	conv_gpio->conv.enable = snd_stm_conv_gpio_enable;
	conv_gpio->conv.disable = snd_stm_conv_gpio_disable;
	conv_gpio->conv.mute = snd_stm_conv_gpio_mute;
	conv_gpio->conv.unmute = snd_stm_conv_gpio_unmute;

	/* Get connections */

	snd_assert(conv_gpio->info->source_bus_id != NULL,
			return -EINVAL);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			conv_gpio->info->source_bus_id);
	index = snd_stm_conv_attach(&conv_gpio->conv, &platform_bus_type,
			conv_gpio->info->source_bus_id);
	if (index < 0) {
		snd_stm_printe("Can't attach to PCM player!\n");
		result = index;
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_gpio,
			&snd_stm_conv_gpio_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Reserve GPIO lines */

	result = gpio_request(conv_gpio->info->enable_gpio, conv_gpio->bus_id);
	if (result != 0) {
		snd_stm_printe("Can't reserve 'enable' GPIO line!\n");
		goto error_gpio_request_enable;
	}

	if (conv_gpio->info->mute_supported) {
		result = gpio_request(conv_gpio->info->mute_gpio,
				conv_gpio->bus_id);
		if (result != 0) {
			snd_stm_printe("Can't reserve 'mute' GPIO line!\n");
			goto error_gpio_request_mute;
		}
	}
	if (gpio_cansleep(conv_gpio->info->enable_gpio) ||
			(conv_gpio->info->mute_supported &&
			gpio_cansleep(conv_gpio->info->mute_gpio))) {
		conv_gpio->may_sleep = 1;
		INIT_WORK(&conv_gpio->work, snd_stm_conv_gpio_work);
		spin_lock_init(&conv_gpio->work_lock);
		conv_gpio->work_enable_value = -1;
		conv_gpio->work_mute_value = -1;
	}

	/* Create ALSA control */

	if (conv_gpio->info->mute_supported) {
		snd_stm_conv_gpio_ctl_mute.device =
			conv_gpio->info->card_device;
		snd_stm_conv_gpio_ctl_mute.index = index;
		result = snd_ctl_add(card,
				snd_ctl_new1(&snd_stm_conv_gpio_ctl_mute,
				conv_gpio));
		if (result < 0) {
			snd_stm_printe("Failed to add all ALSA control!\n");
			goto error_control;
		}
	}

	/* Done now */

	platform_set_drvdata(pdev, &conv_gpio->conv);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return 0;

error_control:
	if (conv_gpio->info->mute_supported)
		gpio_free(conv_gpio->info->mute_gpio);
error_gpio_request_mute:
	gpio_free(conv_gpio->info->enable_gpio);
error_gpio_request_enable:
error_device:
error_attach:
	snd_stm_magic_clear(conv_gpio);
	kfree(conv_gpio);
error_alloc:
	return result;
}

static int snd_stm_conv_gpio_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_gpio *conv_gpio =
			container_of(platform_get_drvdata(pdev),
			struct snd_stm_conv_gpio, conv);

	snd_assert(conv_gpio, return -EINVAL);
	snd_stm_magic_assert(conv_gpio, return -EINVAL);

	if (conv_gpio->info->mute_supported)
		gpio_free(conv_gpio->info->mute_gpio);
	gpio_free(conv_gpio->info->enable_gpio);

	snd_stm_magic_clear(conv_gpio);
	kfree(conv_gpio);

	return 0;
}

static struct platform_driver snd_stm_conv_gpio_driver = {
	.driver = {
		.name = "snd_conv_gpio",
	},
	.probe = snd_stm_conv_gpio_probe,
	.remove = snd_stm_conv_gpio_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_gpio_init(void)
{
	return platform_driver_register(&snd_stm_conv_gpio_driver);
}

static void __exit snd_stm_conv_gpio_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_gpio_driver);
}

MODULE_AUTHOR("Pawel MOLL <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics GPIO-controlled audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_gpio_init);
module_exit(snd_stm_conv_gpio_exit);
