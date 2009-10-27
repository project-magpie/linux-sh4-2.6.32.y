/*
 *   STMicrolectronics STx7141 audio subsystem driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Author: Stephen Gallimore <stephen.gallimore@st.com>
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
#include <asm/irq-ilc.h>
#include <sound/core.h>

#define COMPONENT stx7141
#include "common.h"
#include "reg_7141_audcfg.h"



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7141"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7141 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7141 audio subsystem card.");



/*
 * Audio glue driver implementation
 */

struct snd_stm_stx7141_glue {
	int ver;

	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7141_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7141_glue *stx7141_glue = entry->private_data;

	if (snd_BUG_ON(!stx7141_glue))
		return;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7141_glue)))
		return;

	snd_iprintf(buffer, "--- snd_stx7141_glue ---\n");
	snd_iprintf(buffer, "base = 0x%p\n", stx7141_glue->base);

	snd_iprintf(buffer, "AUDCFG_IO_CTRL (offset 0x00) = 0x%08x\n",
			get__7141_AUDCFG_IO_CTRL(stx7141_glue));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7141_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7141_glue *stx7141_glue;

	snd_stm_printd(0, "--- Probing device '%s'...\n", pdev->dev.bus_id);

	stx7141_glue = kzalloc(sizeof(*stx7141_glue), GFP_KERNEL);
	if (!stx7141_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7141_glue);

	result = snd_stm_memory_request(pdev, &stx7141_glue->mem_region,
			&stx7141_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Additional procfs info */

	snd_stm_info_register(&stx7141_glue->proc_entry, "stx7141_glue",
			snd_stm_stx7141_glue_dump_registers, stx7141_glue);

	/* Done now */

	platform_set_drvdata(pdev, stx7141_glue);

	snd_stm_printd(0, "--- Probed successfully!\n");

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7141_glue);
	kfree(stx7141_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7141_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7141_glue *stx7141_glue =
			platform_get_drvdata(pdev);

	if (snd_BUG_ON(!stx7141_glue))
		return -EINVAL;
	if (snd_BUG_ON(!snd_stm_magic_valid(stx7141_glue)))
		return -EINVAL;

	snd_stm_info_unregister(stx7141_glue->proc_entry);

	snd_stm_memory_release(stx7141_glue->mem_region, stx7141_glue->base);

	snd_stm_magic_clear(stx7141_glue);
	kfree(stx7141_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7141_glue_driver = {
	.driver = {
		.name = "snd_stx7141_glue",
	},
	.probe = snd_stm_stx7141_glue_probe,
	.remove = snd_stm_stx7141_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7141_init(void)
{
	int result;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx7141_init()\n");

	if (cpu_data->type != CPU_STX7141) {
		snd_stm_printe("Not supported (other than STx7141) SOC "
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
	strcpy(card->driver, "STx7141");
	strcpy(card->shortname, "STx7141 audio subsystem");
	snprintf(card->longname, 79, "STMicroelectronics STx7141 cut %d "
			"SOC audio subsystem", cpu_data->cut_major);

	result = platform_driver_register(&snd_stm_stx7141_glue_driver);
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
	platform_driver_unregister(&snd_stm_stx7141_glue_driver);
error_glue_driver_register:
	snd_stm_card_free();
error_card_new:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7141_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7141_exit()\n");

	snd_stm_card_free();

	snd_stm_drivers_unregister();
	platform_driver_unregister(&snd_stm_stx7141_glue_driver);
}

MODULE_AUTHOR("Stephen Gallimore <stephen.gallimore@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7141 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7141_init);
module_exit(snd_stm_stx7141_exit);
