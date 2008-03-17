/*
 *   Helpful ;-) routines for STMicroelectronics' SoCs audio drivers
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
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/pcm_params.h>
#include <sound/asoundef.h>

#undef TRACE
#include "common.h"



/*
 * Cards management
 */

static struct snd_card *snd_stm_cards[SNDRV_CARDS];
static struct snd_card *snd_stm_default;

/* Card list initialization/cleanup */

int __init snd_stm_cards_init(const char *driver, struct snd_stm_card *cards,
		int num_cards)
{
	int result = 0;
	int i;

	for (i = 0; i < num_cards; i++) {
		int card = cards[i].index;

		snd_stm_cards[card] = snd_card_new(card, cards[i].id,
				THIS_MODULE, 0);
		if (snd_stm_cards[card] == NULL) {
			snd_stm_cards_free();
			result = -ENOMEM;
			break;
		}

		if (snd_stm_default == NULL)
			snd_stm_default = snd_stm_cards[i];

		strcpy(snd_stm_cards[card]->driver, driver);
		strcpy(snd_stm_cards[card]->shortname, cards[i].short_name);
		strcpy(snd_stm_cards[card]->longname, cards[i].long_name);

		snd_printd("Card %d ('%s') created:\n", card, cards[i].id);
		snd_printd("- driver: %s,\n", snd_stm_cards[card]->driver);
		snd_printd("- short name: %s,\n",
				snd_stm_cards[card]->shortname);
		snd_printd("- long name: %s.\n", snd_stm_cards[card]->longname);
	}

	return result;
}

int __init snd_stm_cards_register(void)
{
	int result = -ENODEV;
	int i;

	for (i = 0; i < SNDRV_CARDS; i++) {
		if (snd_stm_cards[i]) {
			result = snd_card_register(snd_stm_cards[i]);
			if (result < 0) {
				snd_stm_cards_free();
				break;
			}
		}
	}

	return result;
}

void snd_stm_cards_free(void)
{
	int i;

	for (i = 0; i < SNDRV_CARDS; i++) {
		if (snd_stm_cards[i]) {
			snd_card_free(snd_stm_cards[i]);
			snd_stm_cards[i] = NULL;
		}
	}
}

/* Card list access */

struct snd_card __init *snd_stm_cards_get(const char *id)
{
	int i;

	for (i = 0; i < SNDRV_CARDS; i++)
		if (snd_stm_cards[i] &&
				strcmp(snd_stm_cards[i]->id, id) == 0)
			return snd_stm_cards[i];

	snd_stm_printe("Unknown card %s requested!\n", id);
	return NULL;
}

struct snd_card __init *snd_stm_cards_default(void)
{
	return snd_stm_default;
}



/*
 * Device management
 */

static void dummy_release(struct device *dev)
{
}

int __init snd_stm_add_plaform_devices(struct platform_device **devices,
		int cnt)
{
	int result = 0;
	int i;

	for (i = 0; i < cnt; i++) {
		devices[i]->dev.release = dummy_release;
		result = platform_device_register(devices[i]);
		if (result != 0) {
			while (--i >= 0)
				platform_device_unregister(devices[i]);
			break;
		}
	}

	return result;
}

void __exit snd_stm_remove_plaform_devices(struct platform_device **devices,
		int cnt)
{
	int i;

	for (i = 0; i < cnt; i++)
		platform_device_unregister(devices[i]);
}

static int snd_stm_bus_id_match(struct device *device, void *bus_id)
{
	return strcmp(device->bus_id, bus_id) == 0;
}

struct device *snd_stm_find_device(struct bus_type *bus,
		const char *bus_id)
{
	if (bus == NULL)
		bus = &platform_bus_type;
	return bus_find_device(bus, NULL, (void *)bus_id, snd_stm_bus_id_match);
}




/*
 * Resources management
 */

int __init snd_stm_memory_request(struct platform_device *pdev,
		struct resource **mem_region, void **base_address)
{
	struct resource *resource;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		snd_stm_printe("Failed to"
				" platform_get_resource(IORESOURCE_MEM)!\n");
		return -ENODEV;
	}

	*mem_region = request_mem_region(resource->start,
			resource->end - resource->start + 1, pdev->name);
	if (!*mem_region) {
		snd_stm_printe("Failed request_mem_region(0x%08x,"
				" 0x%08x, '%s')!\n", resource->start,
				resource->end - resource->start + 1,
				pdev->name);
		return -EBUSY;
	}
	snd_printd("Memory region: 0x%08x-0x%08x\n",
			(*mem_region)->start, (*mem_region)->end);

	*base_address = ioremap(resource->start,
			resource->end - resource->start + 1);
	if (!*base_address) {
		release_resource(*mem_region);
		snd_stm_printe("Failed ioremap!\n");
		return -EINVAL;
	}

	snd_stm_printt("Base address is 0x%p.\n", base_address);

	return 0;
}

void snd_stm_memory_release(struct resource *mem_region,
		void *base_address)
{
	iounmap(base_address);
	release_resource(mem_region);
}

int  __init snd_stm_irq_request(struct platform_device *pdev,
		unsigned int *irq, irq_handler_t handler, void *dev_id)
{
	struct resource *resource;
	int result;

	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!resource) {
		snd_stm_printe("Failed to "
				"platform_get_resource(IORESOURCE_IRQ)!\n");
		return -ENODEV;
	}
	snd_printd("IRQ: %u\n", resource->start);

	*irq = resource->start;

	result = request_irq(*irq, handler, IRQF_DISABLED, pdev->name, dev_id);
	if (result != 0) {
		snd_stm_printe("Failed request_irq!\n");
		return -EINVAL;
	}

	return 0;
}

int __init snd_stm_fdma_request(struct platform_device *pdev,
		unsigned int *channel)
{
	static const char *fdmac_id[] = { STM_DMAC_ID, NULL };
	static const char *fdma_cap_lb[] = { STM_DMA_CAP_LOW_BW, NULL };
	static const char *fdma_cap_hb[] = { STM_DMA_CAP_HIGH_BW, NULL };

	*channel = request_dma_bycap(fdmac_id, fdma_cap_lb, pdev->name);
	if (*channel < 0) {
		*channel = request_dma_bycap(fdmac_id, fdma_cap_hb, pdev->name);
		if (*channel < 0) {
			snd_stm_printe("Failed to request_dma_bycap()==%d!\n",
					*channel);
			return -ENODEV;
		}
	}
	snd_printd("FDMA channel: %d\n", *channel);

	return 0;
}



/*
 * ALSA procfs additional entries
 */

static struct snd_info_entry *snd_stm_info_root;

int __init snd_stm_info_init(void)
{
	int result = 0;

	snd_stm_info_root = snd_info_create_module_entry(THIS_MODULE,
			"stm", NULL);
	if (snd_stm_info_root) {
		snd_stm_info_root->mode = S_IFDIR | S_IRUGO | S_IXUGO;
		if (snd_info_register(snd_stm_info_root) < 0) {
			result = -EINVAL;
			snd_info_free_entry(snd_stm_info_root);
		}
	} else {
		result = -ENOMEM;
	}

	return result;
}

void snd_stm_info_cleanup(void)
{
	if (snd_stm_info_root)
		snd_info_free_entry(snd_stm_info_root);
}

int snd_stm_info_register(struct snd_info_entry **entry,
		const char *name,
		void (read)(struct snd_info_entry *, struct snd_info_buffer *),
		void *private_data)
{
	int result = 0;

	*entry = snd_info_create_module_entry(THIS_MODULE, name,
			snd_stm_info_root);
	if (*entry) {
		(*entry)->c.text.read = read;
		(*entry)->private_data = private_data;
		if (snd_info_register(*entry) < 0) {
			result = -EINVAL;
			snd_info_free_entry(*entry);
		}
	} else {
		result = -EINVAL;
	}
	return result;
}

void snd_stm_info_unregister(struct snd_info_entry *entry)
{
	if (entry)
		snd_info_free_entry(entry);
}



/*
 * ALSA PCM buffer memory mapping
 */

static struct page *snd_stm_mmap_nopage(struct vm_area_struct *area,
		unsigned long address, int *type)
{
	/* No VMA expanding here! */
	return NOPAGE_SIGBUS;
}

static struct vm_operations_struct snd_stm_mmap_vm_ops = {
	.open =   snd_pcm_mmap_data_open,
	.close =  snd_pcm_mmap_data_close,
	.nopage = snd_stm_mmap_nopage,
};

int snd_stm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *area)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long map_offset = area->vm_pgoff << PAGE_SHIFT;
	unsigned long phys_addr = runtime->dma_addr + map_offset;
	unsigned long map_size = area->vm_end - area->vm_start;
	unsigned long phys_size = runtime->dma_bytes + PAGE_SIZE -
			runtime->dma_bytes % PAGE_SIZE;

	snd_stm_printt("snd_stm_pcm_mmap(substream=0x%p)\n",
			substream);

	snd_stm_printt("Mmaping %lu bytes starting from 0x%08lx "
			"(dma_addr=0x%08x, dma_size=%u, vm_pgoff=%lu, "
			"vm_start=0x%lx, vm_end=0x%lx)...\n", map_size,
			phys_addr, runtime->dma_addr, runtime->dma_bytes,
			area->vm_pgoff, area->vm_start, area->vm_end);

	if (map_size > phys_size) {
		snd_stm_printe("Trying to perform mmap larger than buffer!\n");
		return -EINVAL;
	}

	area->vm_ops = &snd_stm_mmap_vm_ops;
	area->vm_private_data = substream;
	area->vm_flags |= VM_RESERVED;
	area->vm_page_prot = pgprot_noncached(area->vm_page_prot);

	if (remap_pfn_range(area, area->vm_start, phys_addr >> PAGE_SHIFT,
			map_size, area->vm_page_prot) != 0) {
		snd_stm_printe("Can't remap buffer!\n");
		return -EAGAIN;
	}

	/* Must be called implicitly here... */
	snd_pcm_mmap_data_open(area);

	return 0;
}



/*
 * Common ALSA parameters constraints
 */

/*
#define FIXED_TRANSFER_BYTES max_transfer_bytes > 16 ? 16 : max_transfer_bytes
#define FIXED_TRANSFER_BYTES max_transfer_bytes
*/

#ifdef FIXED_TRANSFER_BYTES

int snd_stm_pcm_transfer_bytes(unsigned int bytes_per_frame,
		unsigned int max_transfer_bytes)
{
	int transfer_bytes = FIXED_TRANSFER_BYTES;

	snd_stm_printt("snd_stm_pcm_transfer_bytes(bytes_per_frame=%u, "
			"max_transfer_bytes=%u) = %u (FIXED)\n",
			bytes_per_frame, max_transfer_bytes, transfer_bytes);

	return transfer_bytes;
}

int snd_stm_pcm_hw_constraint_transfer_bytes(struct snd_pcm_runtime *runtime,
		unsigned int max_transfer_bytes)
{
	return snd_pcm_hw_constraint_step(runtime, 0,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			snd_stm_pcm_transfer_bytes(0, max_transfer_bytes));
}

#else

int snd_stm_pcm_transfer_bytes(unsigned int bytes_per_frame,
		unsigned int max_transfer_bytes)
{
	unsigned int transfer_bytes;

	for (transfer_bytes = bytes_per_frame;
			transfer_bytes * 2 < max_transfer_bytes;
			transfer_bytes *= 2)
		;

	snd_stm_printt("snd_stm_pcm_transfer_bytes(bytes_per_frame=%u, "
			"max_transfer_bytes=%u) = %u\n", bytes_per_frame,
			max_transfer_bytes, transfer_bytes);

	return transfer_bytes;
}

static int snd_stm_pcm_hw_rule_transfer_bytes(struct snd_pcm_hw_params *params,
		struct snd_pcm_hw_rule *rule)
{
	int changed = 0;
	unsigned int max_transfer_bytes = (unsigned int)rule->private;
	struct snd_interval *period_bytes = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES);
	struct snd_interval *frame_bits = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_FRAME_BITS);
	unsigned int transfer_bytes, n;

	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bits->min / 8,
			max_transfer_bytes);
	n = period_bytes->min % transfer_bytes;
	if (n != 0 || period_bytes->openmin) {
		period_bytes->min += transfer_bytes - n;
		changed = 1;
	}

	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bits->max / 8,
			max_transfer_bytes);
	n = period_bytes->max % transfer_bytes;
	if (n != 0 || period_bytes->openmax) {
		period_bytes->max -= n;
		changed = 1;
	}

	if (snd_interval_checkempty(period_bytes)) {
		period_bytes->empty = 1;
		return -EINVAL;
	}

	return changed;
}

int snd_stm_pcm_hw_constraint_transfer_bytes(struct snd_pcm_runtime *runtime,
		unsigned int max_transfer_bytes)
{
	return snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			snd_stm_pcm_hw_rule_transfer_bytes,
			(void *)max_transfer_bytes,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			SNDRV_PCM_HW_PARAM_FRAME_BITS, -1);
}

#endif



/*
 * Common ALSA controls routines
 */

int snd_stm_ctl_boolean_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

int snd_stm_ctl_iec958_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}


int snd_stm_ctl_iec958_mask_get_con(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
			IEC958_AES0_NONAUDIO |
			IEC958_AES0_CON_NOT_COPYRIGHT |
			IEC958_AES0_CON_EMPHASIS |
			IEC958_AES0_CON_MODE;
	ucontrol->value.iec958.status[1] = IEC958_AES1_CON_CATEGORY |
			IEC958_AES1_CON_ORIGINAL;
	ucontrol->value.iec958.status[2] = IEC958_AES2_CON_SOURCE |
			IEC958_AES2_CON_CHANNEL;
	ucontrol->value.iec958.status[3] = IEC958_AES3_CON_FS |
			IEC958_AES3_CON_CLOCK;
	ucontrol->value.iec958.status[4] = IEC958_AES4_CON_MAX_WORDLEN_24 |
			IEC958_AES4_CON_WORDLEN;

	return 0;
}

int snd_stm_ctl_iec958_mask_get_pro(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
			IEC958_AES0_NONAUDIO |
			IEC958_AES0_PRO_EMPHASIS |
			IEC958_AES0_PRO_FREQ_UNLOCKED |
			IEC958_AES0_PRO_FS;
	ucontrol->value.iec958.status[1] = IEC958_AES1_PRO_MODE |
			IEC958_AES1_PRO_USERBITS;
	ucontrol->value.iec958.status[2] = IEC958_AES2_PRO_SBITS |
			IEC958_AES2_PRO_WORDLEN;

	return 0;
}

int snd_stm_iec958_cmp(const struct snd_aes_iec958 *a,
		const struct snd_aes_iec958 *b)
{
	int result;

	snd_assert(a != NULL, return -EINVAL);
	snd_assert(b != NULL, return -EINVAL);

	result = memcmp(a->status, b->status, sizeof(a->status));
	if (result == 0)
		result = memcmp(a->subcode, b->subcode, sizeof(a->subcode));
	if (result == 0)
		result = memcmp(a->dig_subframe, b->dig_subframe,
				sizeof(a->dig_subframe));

	return result;
}


/*
 * Debug features
 */

/* Memory dump function */

void snd_stm_hex_dump(void *data, int size)
{
	unsigned char *buffer = data;
	char line[57];
	int i;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			sprintf(line, "%p", data + i);
		sprintf(line + 8 + ((i % 16) * 3), " %02x", *buffer++);
		if (i % 16 == 15 || i == size - 1)
			printk(KERN_DEBUG "%s\n", line);
	}
}

/* IEC958 structure dump */
void snd_stm_iec958_dump(const struct snd_aes_iec958 *vuc)
{
	int i;
	char line[54];
	const unsigned char *data;

	printk(KERN_DEBUG "                        "
			"0  1  2  3  4  5  6  7  8  9\n");
	data = vuc->status;
	for (i = 0; i < 24; i++) {
		if (i % 10 == 0)
			sprintf(line, "%p status    %02d:",
					(unsigned char *)vuc + i, i);
		sprintf(line + 22 + ((i % 10) * 3), " %02x", *data++);
		if (i % 10 == 9 || i == 23)
			printk(KERN_DEBUG "%s\n", line);
	}

	data = vuc->subcode;
	for (i = 0; i < 147; i++) {
		if (i % 10 == 0)
			sprintf(line, "%p subcode  %03d:",
					(unsigned char *)vuc +
					offsetof(struct snd_aes_iec958,
					dig_subframe) + i, i);
		sprintf(line + 22 + ((i % 10) * 3), " %02x", *data++);
		if (i % 10 == 9 || i == 146)
			printk(KERN_DEBUG "%s\n", line);
	}

	printk(KERN_DEBUG "%p dig_subframe: %02x %02x %02x %02x\n",
			(unsigned char *)vuc +
			offsetof(struct snd_aes_iec958, dig_subframe),
			vuc->dig_subframe[0], vuc->dig_subframe[1],
			vuc->dig_subframe[2], vuc->dig_subframe[3]);
}
