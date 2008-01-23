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
#include <linux/stm/soc.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/info.h>

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

struct snd_card __init *snd_stm_cards_default(const char **id)
{
	if (snd_stm_default && id)
		*id = snd_stm_default->id;

	return snd_stm_default;
}



/*
 * Components management
 */

static struct snd_stm_component *snd_stm_components;
static int snd_stm_num_components;

int __init snd_stm_components_init(struct snd_stm_component *components,
		int num_components)
{
	snd_stm_components = components;
	snd_stm_num_components = num_components;

	return 0;
}

struct snd_stm_component __init *snd_stm_components_get(const char *bus_id)
{
	int i;

	for (i = 0; i < snd_stm_num_components; i++)
		if (strcmp(snd_stm_components[i].bus_id, bus_id) == 0)
			return &snd_stm_components[i];

	return NULL;
}

static int snd_stm_bus_id_match(struct device *device, void *bus_id)
{
	return strcmp(device->bus_id, bus_id) == 0;
}

struct device __init *snd_stm_device_get(const char *bus_id)
{
	return bus_find_device(&platform_bus_type, NULL,
			(void *)bus_id, snd_stm_bus_id_match);
}



/*
 * Component capabilities access
 */

int __init snd_stm_cap_set(struct snd_stm_component *component,
		const char *name, union snd_stm_value value)
{
	int result = -1;
	int i;

	for (i = 0; i < component->num_caps; i++)
		if (strcmp(name, component->caps[i].name) == 0) {
			component->caps[i].value = value;
			result = 0;
			break;
		}

	return result;
}

int __init snd_stm_cap_get(struct snd_stm_component *component,
		const char *name, union snd_stm_value *value)
{
	int result = -1;
	int i;

	for (i = 0; i < component->num_caps; i++)
		if (strcmp(name, component->caps[i].name) == 0) {
			*value = component->caps[i].value;
			result = 0;
			break;
		}

	return result;
}

int __init snd_stm_cap_get_number(struct snd_stm_component *component,
		const char *name, int *number)
{
	union snd_stm_value value;
	int result = snd_stm_cap_get(component, name, &value);

	*number = value.number;
	return result;
}

int __init snd_stm_cap_get_string(struct snd_stm_component *component,
		const char *name, const char **string)
{
	union snd_stm_value value;
	int result = snd_stm_cap_get(component, name, &value);

	*string = value.string;
	return result;
}

int __init snd_stm_cap_get_range(struct snd_stm_component *component,
		const char *name, int *from, int *to)
{
	union snd_stm_value value;
	int result = snd_stm_cap_get(component, name, &value);

	*from = value.range.from;
	*to = value.range.to;
	return result;
}

int __init snd_stm_cap_get_list(struct snd_stm_component *component,
		const char *name, int **numbers, int *len)
{
	union snd_stm_value value;
	int result = snd_stm_cap_get(component, name, &value);

	*numbers = value.list.numbers;
	*len = value.list.len;
	return result;
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
		int *channel, struct stm_dma_req **request,
		struct stm_dma_req_config *config)
{
	static const char *fdmac_id[] = { STM_DMAC_ID, NULL };
	static const char *fdma_cap_lb[] = { STM_DMA_CAP_LOW_BW, NULL };
	static const char *fdma_cap_hb[] = { STM_DMA_CAP_HIGH_BW, NULL };
	struct resource *resource;

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

	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
			"initiator");
	if (!resource) {
		snd_stm_printe("Failed to platform_get_resource"
				"(IORESOURCE_DMA, initiator)!\n");
		return -ENODEV;
	}
	snd_printd("FDMA initiator: %u\n", resource->start);
	config->initiator = resource->start;

	resource = platform_get_resource_byname(pdev, IORESOURCE_DMA,
			"request_line");
	if (!resource) {
		snd_stm_printe("Failed to platform_get_resource"
				"(IORESOURCE_DMA, request_line)!\n");
		return -ENODEV;
	}
	snd_printd("FDMA request line: %u\n", resource->start);

	*request = dma_req_config(*channel, resource->start, config);
	if (!*request) {
		snd_stm_printe("Failed to dma_req_config!\n");
		return -EINVAL;
	}

	return 0;
}

void snd_stm_fdma_release(unsigned int channel,
		struct stm_dma_req *request)
{
	dma_req_free(channel, request);
	free_dma(channel);
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
 * PCM buffer memory mapping
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
