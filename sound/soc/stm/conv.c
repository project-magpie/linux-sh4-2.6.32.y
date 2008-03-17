/*
 *   STMicroelectronics System-on-Chips' generic converters infrastructure
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
#include <linux/list.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 2 /* See common.h debug features */
#include "common.h"



/*
 * Converters infrastructure interface implementation
 */

#define CONVS_MAX 5 /* TODO: dynamic structure (really necessary???) */

struct snd_stm_conv_links_list {
	struct list_head list;

	struct device *device;

	int convs_num;
	struct snd_stm_conv *convs[CONVS_MAX];

	int conv_attached;

	snd_stm_magic_field;
};

LIST_HEAD(snd_stm_conv_links); /* "Device->Converter" links list */
DEFINE_SPINLOCK(snd_stm_conv_links_lock); /* Synchronises the links list */

static inline struct snd_stm_conv_links_list *snd_stm_conv_find_link(
		struct device *source) {
	struct snd_stm_conv_links_list *entry;

	list_for_each_entry(entry, &snd_stm_conv_links, list)
		if (source == entry->device)
			return entry;

	return NULL;
}

int snd_stm_conv_attach(struct snd_stm_conv *conv, struct device *source)
{
	struct snd_stm_conv_links_list *link = snd_stm_conv_find_link(source);

	snd_stm_printt("snd_stm_conv_attach(conv=%p, source=%p)\n",
			conv, source);

	/* Not synchronised intentionally (doesn't have to be...) */

	if (link) { /* Known device */
		snd_stm_magic_assert(link, return -EINVAL);
		snd_assert(link->convs_num < CONVS_MAX,
				return -ENOMEM);
		link->convs[link->convs_num++] = conv;
	} else { /* New device */
		link = kzalloc(sizeof(*link), GFP_KERNEL);
		if (link == NULL)
			return -ENOMEM;
		snd_stm_magic_set(link);

		link->device = source;
		link->convs_num = 1;
		link->convs[0] = conv;

		list_add_tail(&link->list, &snd_stm_conv_links);
	}

	return 0;
}

struct snd_stm_conv *snd_stm_conv_get_attached(struct device *source)
{
	struct snd_stm_conv_links_list *link;
	struct snd_stm_conv *conv = NULL;

	snd_stm_printt("snd_stm_conv_attach(source=%p)\n", source);

	spin_lock(&snd_stm_conv_links_lock);

	link = snd_stm_conv_find_link(source);
	conv = link ? link->convs[link->conv_attached] : NULL;

	spin_unlock(&snd_stm_conv_links_lock);

	return conv;
}

static int snd_stm_conv_route_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct snd_stm_conv_links_list *link = snd_kcontrol_chip(kcontrol);
	struct snd_stm_conv *conv;

	snd_stm_magic_assert(link, return -EINVAL);

	/* Not synchronised intentionally (doesn't have to be...) */

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = link->convs_num;

	if (uinfo->value.enumerated.item >= link->convs_num)
		uinfo->value.enumerated.item = link->convs_num - 1;

	conv = link->convs[uinfo->value.enumerated.item];
	snprintf(uinfo->value.enumerated.name, 64, "%s", conv->name);

	return 0;
}

static int snd_stm_conv_route_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_links_list *link = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_conv_route_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_stm_magic_assert(link, return -EINVAL);

	spin_lock(&snd_stm_conv_links_lock);

	ucontrol->value.enumerated.item[0] = link->conv_attached;

	spin_unlock(&snd_stm_conv_links_lock);

	return 0;
}

static int snd_stm_conv_route_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int changed;
	struct snd_stm_conv_links_list *link = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_conv_route_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_stm_magic_assert(link, return -EINVAL);

	spin_lock(&snd_stm_conv_links_lock);

	changed = (ucontrol->value.enumerated.item[0] != link->conv_attached);
	link->conv_attached = ucontrol->value.enumerated.item[0];

	spin_unlock(&snd_stm_conv_links_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_conv_route_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "PCM Playback Route",
	.info = snd_stm_conv_route_info,
	.get = snd_stm_conv_route_get,
	.put = snd_stm_conv_route_put,
};

int snd_stm_conv_add_route_ctl(struct device *source,
		struct snd_card *card, int card_device)
{
	int result = 0;
	struct snd_stm_conv_links_list *link = snd_stm_conv_find_link(source);

	snd_stm_printt("snd_stm_conv_add_route_ctl(source=%p, card=%p, "
			"cards_device=%d)\n", source, card, card_device);

	/* Not synchronised intentionally (doesn't have to be...) */

	if (link != NULL) {
		snd_stm_magic_assert(link, return -EINVAL);

		if (link->convs_num > 1) {
			snd_stm_conv_route_ctl.device = card_device;
			result = snd_ctl_add(card,
					snd_ctl_new1(&snd_stm_conv_route_ctl,
					link));

			/* TODO: index per card */
			snd_stm_conv_route_ctl.index++;
		}
	} else {
		snd_stm_printt("Source device '%s' not found...\n",
				source->bus_id);
	}

	return result;
}



/*
 * Converter control interface implementation
 */

unsigned int snd_stm_conv_get_format(struct snd_stm_conv *conv)
{
	snd_stm_printt("snd_stm_conv_get_format(conv=%p)\n", conv);

	snd_assert(conv->get_format != NULL, return -EINVAL);

	return conv->get_format(conv);
}

int snd_stm_conv_get_oversampling(struct snd_stm_conv *conv)
{
	snd_stm_printt("snd_stm_conv_get_oversampling(conv=%p)\n", conv);

	snd_assert(conv->get_oversampling != NULL, return -EINVAL);

	return conv->get_oversampling(conv);
}

int snd_stm_conv_enable(struct snd_stm_conv *conv)
{
	int result = 0;

	snd_stm_printt("snd_stm_conv_enable(conv=%p)\n", conv);

	snd_assert(conv->enable != NULL, return -EINVAL);

	if (conv->master)
		result = snd_stm_conv_enable(conv->master);

	return result ? result : conv->enable(conv);
}

int snd_stm_conv_disable(struct snd_stm_conv *conv)
{
	int result = 0;

	snd_stm_printt("snd_stm_conv_disable(conv=%p)\n", conv);

	snd_assert(conv->disable != NULL, return -EINVAL);

	if (conv->master)
		result = snd_stm_conv_disable(conv->master);

	return result ? result : conv->disable(conv);
}

int snd_stm_conv_mute(struct snd_stm_conv *conv)
{
	snd_stm_printt("snd_stm_conv_mute(conv=%p)\n", conv);

	snd_assert(conv->mute != NULL, return -EINVAL);

	return conv->mute(conv);
}

int snd_stm_conv_unmute(struct snd_stm_conv *conv)
{
	snd_stm_printt("snd_stm_conv_unmute(conv=%p)\n", conv);

	snd_assert(conv->unmute != NULL, return -EINVAL);

	return conv->unmute(conv);
}



/*
 * Initialization
 */

int __init snd_stm_conv_init(void)
{
	return 0;
}

void snd_stm_conv_cleanup(void)
{
	struct snd_stm_conv_links_list *entry, *next;

	list_for_each_entry_safe(entry, next, &snd_stm_conv_links, list) {
		snd_stm_magic_clear(entry);
		list_del(&entry->list);
		kfree(entry);
	};
}
