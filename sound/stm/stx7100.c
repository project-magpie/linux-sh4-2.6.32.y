/*
 *   STMicrolectronics STx7100/STx7109 audio subsystem driver
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
#include <linux/io.h>
#include <sound/core.h>

#define COMPONENT stx7100
#include "common.h"
#include "reg_7100_audcfg.h"



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7100"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7100/STx7109 "
		"audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7100/STx7109 audio subsystem card.");



/*
 * Audio glue driver implementation
 */

struct snd_stm_stx7100_glue {
	int ver;

	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7100_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7100_glue *stx7100_glue = entry->private_data;

	if (snd_BUG_ON(!stx7100_glue))
		return;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7100_glue)))
		return;

	snd_iprintf(buffer, "--- snd_stx7100_glue ---\n");
	snd_iprintf(buffer, "base = 0x%p\n", stx7100_glue->base);

	snd_iprintf(buffer, "AUDCFG_IO_CTRL (offset 0x00) = 0x%08x\n",
			get__7100_AUDCFG_IO_CTRL(stx7100_glue));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7100_glue_register(struct snd_device *snd_device)
{
	struct snd_stm_stx7100_glue *stx7100_glue = snd_device->device_data;

	if (snd_BUG_ON(!stx7100_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7100_glue)))
		return -EINVAL;

	/* Enable audio outputs */

	set__7100_AUDCFG_IO_CTRL(stx7100_glue,
		mask__7100_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(stx7100_glue) |
		mask__7100_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(stx7100_glue) |
		mask__7100_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(stx7100_glue) |
		mask__7100_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(stx7100_glue));

	/* Additional procfs info */

	snd_stm_info_register(&stx7100_glue->proc_entry, "stx7100_glue",
			snd_stm_stx7100_glue_dump_registers, stx7100_glue);

	return 0;
}

static int __exit snd_stm_stx7100_glue_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_stx7100_glue *stx7100_glue = snd_device->device_data;

	if (snd_BUG_ON(!stx7100_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7100_glue)))
		return -EINVAL;

	/* Remove procfs entry */

	snd_stm_info_unregister(stx7100_glue->proc_entry);

	/* Disable audio outputs */

	set__7100_AUDCFG_IO_CTRL(stx7100_glue,
		mask__7100_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(stx7100_glue) |
		mask__7100_AUDCFG_IO_CTRL__DATA1_EN__INPUT(stx7100_glue) |
		mask__7100_AUDCFG_IO_CTRL__DATA0_EN__INPUT(stx7100_glue) |
		mask__7100_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(stx7100_glue));

	return 0;
}

static struct snd_device_ops snd_stm_stx7100_glue_snd_device_ops = {
	.dev_register = snd_stm_stx7100_glue_register,
	.dev_disconnect = snd_stm_stx7100_glue_disconnect,
};

static int __init snd_stm_stx7100_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7100_glue *stx7100_glue;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	stx7100_glue = kzalloc(sizeof(*stx7100_glue), GFP_KERNEL);
	if (!stx7100_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7100_glue);

	result = snd_stm_memory_request(pdev, &stx7100_glue->mem_region,
			&stx7100_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* ALSA component */

	result = snd_device_new(snd_stm_card_get(), SNDRV_DEV_LOWLEVEL,
			stx7100_glue, &snd_stm_stx7100_glue_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, stx7100_glue);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return result;

error_device:
	snd_stm_memory_release(stx7100_glue->mem_region, stx7100_glue->base);
error_memory_request:
	snd_stm_magic_clear(stx7100_glue);
	kfree(stx7100_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7100_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7100_glue *stx7100_glue =
			platform_get_drvdata(pdev);

	if (snd_BUG_ON(!stx7100_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7100_glue)))
		return -EINVAL;

	snd_stm_memory_release(stx7100_glue->mem_region, stx7100_glue->base);

	snd_stm_magic_clear(stx7100_glue);
	kfree(stx7100_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7100_glue_driver = {
	.driver = {
		.name = "snd_stx7100_glue",
	},
	.probe = snd_stm_stx7100_glue_probe,
	.remove = snd_stm_stx7100_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7100_init(void)
{
	int result;
	const char *soc_type;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx7100_init()\n");

	switch (cpu_data->type) {
	case CPU_STX7100:
		soc_type = "STx7100";
		break;

	case CPU_STX7109:
		soc_type = "STx7109";
		break;

	default:
		snd_stm_printe("Not supported (other than STx7100 or STx7109)"
				" SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
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

	result = platform_driver_register(&snd_stm_stx7100_glue_driver);
	if (result != 0) {
		snd_stm_printe("Failed to register audio glue driver!\n");
		goto error_glue_driver_register;
	}

	result = snd_stm_drivers_register();
	if (result != 0) {
		snd_stm_printe("Drivers registration failed!\n");
		goto error_drivers_register;
	}

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	snd_stm_drivers_unregister();
error_drivers_register:
	platform_driver_unregister(&snd_stm_stx7100_glue_driver);
error_glue_driver_register:
	snd_stm_card_free();
error_card_new:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7100_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7100_exit()\n");

	snd_stm_card_free();

	snd_stm_drivers_unregister();
	platform_driver_unregister(&snd_stm_stx7100_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7100/STx7109 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7100_init);
module_exit(snd_stm_stx7100_exit);
