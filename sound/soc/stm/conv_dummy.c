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
#include <sound/stm.h>

#include "common.h"



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_dummy_get_format(struct snd_stm_conv
		*conv)
{
	return (SND_STM_FORMAT__RIGHT_JUSTIFIED |
			SND_STM_FORMAT__OUTPUT_SUBFRAME_16_BITS);
}

static int snd_stm_conv_dummy_get_oversampling(struct snd_stm_conv *conv)
{
	return 64;
}

static int snd_stm_conv_dummy_enable(struct snd_stm_conv *conv)
{
	snd_printk("Waking up dummy DAC '%s'.\n", conv->name);

	return 0;
}

static int snd_stm_conv_dummy_disable(struct snd_stm_conv *conv)
{
	snd_printk("Setting dummy DAC '%s' into reset mode.\n", conv->name);

	return 0;
}

static int snd_stm_conv_dummy_mute(struct snd_stm_conv *conv)
{
	snd_printk("Muting dummy DAC '%s'.\n", conv->name);

	return 0;
}

static int snd_stm_conv_dummy_unmute(struct snd_stm_conv *conv)
{
	snd_printk("Unmuting dummy DAC '%s'.\n", conv->name);

	return 0;
}



/*
 * Platform driver routines
 */

static int __init snd_stm_conv_dummy_probe(struct platform_device *pdev)
{
	int result;
	struct snd_stm_conv *conv;
	const char *source_bus_id = pdev->dev.platform_data;
	struct device *player_device;
	static int index;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	conv = kzalloc(sizeof(*conv) + 25, GFP_KERNEL);
	if (!conv) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		return -ENOMEM;
	}

	conv->name = (char *)conv + sizeof(*conv);
	sprintf((char *)conv->name, "Dummy converter %x", index++);

	conv->get_format =
		snd_stm_conv_dummy_get_format;
	conv->get_oversampling =
		snd_stm_conv_dummy_get_oversampling;
	conv->enable = snd_stm_conv_dummy_enable;
	conv->disable = snd_stm_conv_dummy_disable;
	conv->mute = snd_stm_conv_dummy_mute;
	conv->unmute = snd_stm_conv_dummy_unmute;

	snd_printd("This dummy DAC is attached to PCM player '%s'.\n",
			source_bus_id);
	player_device = snd_stm_find_device(NULL, source_bus_id);
	snd_assert(player_device != NULL, return -EINVAL);
	result = snd_stm_conv_attach(conv, player_device);
	if (result < 0) {
		snd_stm_printe("Can't attach to PCM player!\n");
		return -EINVAL;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv);

	snd_printd("--- Probed successfully!\n");

	return 0;
}

static int snd_stm_conv_dummy_remove(struct platform_device *pdev)
{
	kfree(platform_get_drvdata(pdev));

	return 0;
}

static struct platform_driver snd_stm_conv_dummy_driver = {
	.driver = {
		.name = "conv_dummy",
	},
	.probe = snd_stm_conv_dummy_probe,
	.remove = snd_stm_conv_dummy_remove,
};



/*
 * Initialization
 */

int __init snd_stm_conv_dummy_init(void)
{
	return platform_driver_register(&snd_stm_conv_dummy_driver);
}

void snd_stm_conv_dummy_cleanup(void)
{
	platform_driver_unregister(&snd_stm_conv_dummy_driver);
}
