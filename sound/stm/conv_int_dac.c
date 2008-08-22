/*
 *   STMicroelectronics System-on-Chips' internal audio DAC driver
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
	/* System informations */
	struct snd_stm_conv_converter *converter;
	const char *bus_id;
	int ver; /* IP version, used by register access macros */

	/* Resources */
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_int_dac_get_format(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_int_dac_get_format(priv=%p)\n", priv);

	return FORMAT;
}

static int snd_stm_conv_int_dac_get_oversampling(void *priv)
{
	snd_stm_printd(1, "snd_stm_conv_int_dac_get_oversampling(priv=%p)\n",
			priv);

	return OVERSAMPLING;
}

static int snd_stm_conv_int_dac_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_int_dac *conv_int_dac = priv;

	snd_stm_printd(1, "snd_stm_conv_int_dac_set_enabled(enabled=%d, "
			"priv=%p)\n", enabled, priv);

	snd_stm_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	snd_stm_printd(1, "%sabling DAC %s's digital part.\n",
			enabled ? "En" : "Dis", conv_int_dac->bus_id);

	if (enabled) {
		set__AUDCFG_ADAC_CTRL__NSB__NORMAL(conv_int_dac);
		set__AUDCFG_ADAC_CTRL__NRST__NORMAL(conv_int_dac);
	} else {
		set__AUDCFG_ADAC_CTRL__NRST__RESET(conv_int_dac);
		set__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(conv_int_dac);
	}

	return 0;
}

static int snd_stm_conv_int_dac_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_int_dac *conv_int_dac = priv;

	snd_stm_printd(1, "snd_stm_conv_int_dac_set_muted(muted=%d, priv=%p)\n",
		       muted, priv);

	snd_stm_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			conv_int_dac->bus_id);

	if (muted)
		set__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(conv_int_dac);
	else
		set__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(conv_int_dac);

	return 0;
}

static struct snd_stm_conv_ops snd_stm_conv_int_dac_ops = {
	.get_format = snd_stm_conv_int_dac_get_format,
	.get_oversampling = snd_stm_conv_int_dac_get_oversampling,
	.set_enabled = snd_stm_conv_int_dac_set_enabled,
	.set_muted = snd_stm_conv_int_dac_set_muted,
};



/*
 * ALSA lowlevel device implementation
 */

static void snd_stm_conv_int_dac_read_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
		entry->private_data;

	snd_stm_assert(conv_int_dac, return);
	snd_stm_magic_assert(conv_int_dac, return);

	snd_iprintf(buffer, "--- %s ---\n", conv_int_dac->bus_id);
	snd_iprintf(buffer, "base = 0x%p\n", conv_int_dac->base);

	snd_iprintf(buffer, "AUDCFG_ADAC_CTRL (offset 0x00) = 0x%08x\n",
			get__AUDCFG_ADAC_CTRL(conv_int_dac));

	snd_iprintf(buffer, "\n");
}

static int snd_stm_conv_int_dac_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_int_dac *conv_int_dac =
			snd_device->device_data;

	snd_stm_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	/* Initialize DAC with digital part down, analog up and muted */

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

	snd_stm_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

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

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	snd_stm_assert(card != NULL, return -EINVAL);
	snd_stm_assert(conv_int_dac_info != NULL, return -EINVAL);

	conv_int_dac = kzalloc(sizeof(*conv_int_dac), GFP_KERNEL);
	if (!conv_int_dac) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_int_dac);
	conv_int_dac->ver = conv_int_dac_info->ver;
	snd_stm_assert(conv_int_dac->ver > 0, return -EINVAL);
	conv_int_dac->bus_id = pdev->dev.bus_id;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv_int_dac->mem_region,
			&conv_int_dac->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Get connections */

	snd_stm_assert(conv_int_dac_info->source_bus_id != NULL,
			return -EINVAL);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			conv_int_dac_info->source_bus_id);
	conv_int_dac->converter = snd_stm_conv_register_converter(
			"Analog Output",
			&snd_stm_conv_int_dac_ops, conv_int_dac,
			&platform_bus_type, conv_int_dac_info->source_bus_id,
			conv_int_dac_info->channel_from,
			conv_int_dac_info->channel_to, NULL);
	if (!conv_int_dac->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_int_dac,
			&snd_stm_conv_int_dac_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_int_dac);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return 0;

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
	struct snd_stm_conv_int_dac *conv_int_dac = platform_get_drvdata(pdev);

	snd_stm_assert(conv_int_dac, return -EINVAL);
	snd_stm_magic_assert(conv_int_dac, return -EINVAL);

	snd_stm_conv_unregister_converter(conv_int_dac->converter);
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
