/*
 *   STMicroelectronics System-on-Chips' dummy DAC driver
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

#define COMPONENT conv_dummy
#include "common.h"



/*
 * Dummy converter instance structure
 */

struct snd_stm_conv_dummy {
	/* Generic converter interface */
	struct snd_stm_conv conv;

	struct snd_stm_conv_dummy_info *info;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_dummy_get_format(struct snd_stm_conv
		*conv)
{
	struct snd_stm_conv_dummy *conv_dummy = container_of(conv,
			struct snd_stm_conv_dummy, conv);

	snd_stm_printd(1, "snd_stm_conv_dummy_get_format(conv=%p)\n", conv);

	snd_assert(conv_dummy, return -EINVAL);
	snd_stm_magic_assert(conv_dummy, return -EINVAL);

	return conv_dummy->info->format;
}

static int snd_stm_conv_dummy_get_oversampling(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_dummy *conv_dummy = container_of(conv,
			struct snd_stm_conv_dummy, conv);

	snd_stm_printd(1, "snd_stm_conv_dummy_get_oversampling(conv=%p)\n",
			conv);

	snd_assert(conv_dummy, return -EINVAL);
	snd_stm_magic_assert(conv_dummy, return -EINVAL);

	return conv_dummy->info->oversampling;
}

static int snd_stm_conv_dummy_enable(struct snd_stm_conv *conv)
{
	snd_stm_printd(1, "snd_stm_conv_dummy_enable(conv=%p)\n", conv);

	return 0;
}

static int snd_stm_conv_dummy_disable(struct snd_stm_conv *conv)
{
	snd_stm_printd(1, "snd_stm_conv_dummy_disable(conv=%p)\n", conv);

	return 0;
}

static int snd_stm_conv_dummy_mute(struct snd_stm_conv *conv)
{
	snd_stm_printd(1, "snd_stm_conv_dummy_mute(conv=%p)\n", conv);

	return 0;
}

static int snd_stm_conv_dummy_unmute(struct snd_stm_conv *conv)
{
	snd_stm_printd(1, "snd_stm_conv_dummy_unmute(conv=%p)\n", conv);

	return 0;
}



/*
 * Platform driver routines
 */

static int snd_stm_conv_dummy_probe(struct platform_device *pdev)
{
	int result;
	struct snd_stm_conv_dummy *conv_dummy;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	snd_assert(pdev->dev.platform_data != NULL, return -EINVAL);

	conv_dummy = kzalloc(sizeof(*conv_dummy), GFP_KERNEL);
	if (!conv_dummy) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		return -ENOMEM;
	}
	snd_stm_magic_set(conv_dummy);
	conv_dummy->info = pdev->dev.platform_data;

	conv_dummy->conv.name = conv_dummy->info->name;
	conv_dummy->conv.get_format = snd_stm_conv_dummy_get_format;
	conv_dummy->conv.get_oversampling = snd_stm_conv_dummy_get_oversampling;
	conv_dummy->conv.enable = snd_stm_conv_dummy_enable;
	conv_dummy->conv.disable = snd_stm_conv_dummy_disable;
	conv_dummy->conv.mute = snd_stm_conv_dummy_mute;
	conv_dummy->conv.unmute = snd_stm_conv_dummy_unmute;

	snd_stm_printd(0, "This dummy DAC is attached to PCM player '%s'.\n",
			conv_dummy->info->source_bus_id);
	result = snd_stm_conv_attach(&conv_dummy->conv, &platform_bus_type,
			conv_dummy->info->source_bus_id);
	if (result < 0) {
		snd_stm_printe("Can't attach to PCM player!\n");
		return -EINVAL;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_dummy);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return 0;
}

static int snd_stm_conv_dummy_remove(struct platform_device *pdev)
{
	kfree(platform_get_drvdata(pdev));

	return 0;
}

static struct platform_driver snd_stm_conv_dummy_driver = {
	.driver = {
		.name = "snd_conv_dummy",
	},
	.probe = snd_stm_conv_dummy_probe,
	.remove = snd_stm_conv_dummy_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_dummy_init(void)
{
	return platform_driver_register(&snd_stm_conv_dummy_driver);
}

static void __exit snd_stm_conv_dummy_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_dummy_driver);
}

MODULE_AUTHOR("Pawel MOLL <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics dummy audio converter driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_dummy_init);
module_exit(snd_stm_conv_dummy_exit);
