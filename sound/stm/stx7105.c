/*
 *   STMicrolectronics STx7105 audio subsystem driver
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
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <sound/core.h>

#define COMPONENT stx7105
#include "common.h"
#include "reg_7105_audcfg.h"



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7105"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7105 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7105 audio subsystem card.");



/*
 * Audio glue driver implementation
 */

struct snd_stm_stx7105_glue {
	int ver;

	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7105_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7105_glue *stx7105_glue = entry->private_data;

	if (snd_BUG_ON(!stx7105_glue))
		return;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7105_glue)))
		return;

	snd_iprintf(buffer, "--- snd_stx7105_glue ---\n");
	snd_iprintf(buffer, "base = 0x%p\n", stx7105_glue->base);

	snd_iprintf(buffer, "AUDCFG_IO_CTRL (offset 0x00) = 0x%08x\n",
			get__7105_AUDCFG_IO_CTRL(stx7105_glue));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7105_glue_register(struct snd_device *snd_device)
{
	struct snd_stm_stx7105_glue *stx7105_glue = snd_device->device_data;

	if (snd_BUG_ON(!stx7105_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7105_glue)))
		return -EINVAL;

	/* Enable audio outputs */

	set__7105_AUDCFG_IO_CTRL(stx7105_glue,
		mask__7105_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(stx7105_glue) |
		mask__7105_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(stx7105_glue) |
		mask__7105_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(stx7105_glue));

	/* Additional procfs info */

	snd_stm_info_register(&stx7105_glue->proc_entry, "stx7105_glue",
			snd_stm_stx7105_glue_dump_registers, stx7105_glue);

	return 0;
}

static int __exit snd_stm_stx7105_glue_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_stx7105_glue *stx7105_glue = snd_device->device_data;

	if (snd_BUG_ON(!stx7105_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7105_glue)))
		return -EINVAL;

	/* Remove procfs entry */

	snd_stm_info_unregister(stx7105_glue->proc_entry);

	/* Disable audio outputs */

	set__7105_AUDCFG_IO_CTRL(stx7105_glue,
		mask__7105_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(stx7105_glue) |
		mask__7105_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(stx7105_glue) |
		mask__7105_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(stx7105_glue));

	return 0;
}

static struct snd_device_ops snd_stm_stx7105_glue_snd_device_ops = {
	.dev_register = snd_stm_stx7105_glue_register,
	.dev_disconnect = snd_stm_stx7105_glue_disconnect,
};

static int __init snd_stm_stx7105_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7105_glue *stx7105_glue;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	stx7105_glue = kzalloc(sizeof(*stx7105_glue), GFP_KERNEL);
	if (!stx7105_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7105_glue);

	result = snd_stm_memory_request(pdev, &stx7105_glue->mem_region,
			&stx7105_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* ALSA component */

	result = snd_device_new(snd_stm_card_get(), SNDRV_DEV_LOWLEVEL,
			stx7105_glue, &snd_stm_stx7105_glue_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, stx7105_glue);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return result;

error_device:
	snd_stm_memory_release(stx7105_glue->mem_region, stx7105_glue->base);
error_memory_request:
	snd_stm_magic_clear(stx7105_glue);
	kfree(stx7105_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7105_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7105_glue *stx7105_glue =
			platform_get_drvdata(pdev);

	if (snd_BUG_ON(!stx7105_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7105_glue)))
		return -EINVAL;

	snd_stm_memory_release(stx7105_glue->mem_region, stx7105_glue->base);

	snd_stm_magic_clear(stx7105_glue);
	kfree(stx7105_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7105_glue_driver = {
	.driver = {
		.name = "snd_stx7105_glue",
	},
	.probe = snd_stm_stx7105_glue_probe,
	.remove = snd_stm_stx7105_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7105_init(void)
{
	int result;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx7105_init()\n");

	if (cpu_data->type != CPU_STX7105) {
		snd_stm_printe("Not supported (other than STx7105) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	card = snd_stm_card_new(index, id, THIS_MODULE);
	if (card == NULL) {
		snd_stm_printe("ALSA card creation failed!\n");
		result = -ENOMEM;
		goto error_card_new;
	}
	strcpy(card->driver, "STx7105");
	strcpy(card->shortname, "STx7105 audio subsystem");
	snprintf(card->longname, 79, "STMicroelectronics STx7105 cut %d "
			"SOC audio subsystem", cpu_data->cut_major);

	result = platform_driver_register(&snd_stm_stx7105_glue_driver);
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
	platform_driver_unregister(&snd_stm_stx7105_glue_driver);
error_glue_driver_register:
	snd_stm_card_free();
error_card_new:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7105_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7105_exit()\n");

	snd_stm_card_free();

	snd_stm_drivers_unregister();
	platform_driver_unregister(&snd_stm_stx7105_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7105 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7105_init);
module_exit(snd_stm_stx7105_exit);
