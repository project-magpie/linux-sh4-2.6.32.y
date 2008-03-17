/*
 *   STMicroelectronics System-on-Chips' I2S to SPDIF converter driver
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
#include <linux/stm/soc.h>
#include <linux/stm/registers.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 3 /* See common.h debug features */
#include "common.h"



/*
 * Hardware-related definitions
 */

#define DEFAULT_OVERSAMPLING 128



/*
 * Converter instance structure
 */

struct snd_stm_conv_i2s_spdif {
	/* Generic converter interface */
	struct snd_stm_conv conv;

	/* System informations */
	struct snd_stm_conv_i2s_spdif_info *info;
	struct device *device;

	/* Resources */
	struct resource *mem_region;
	void *base;

	/* Default configuration */
	struct snd_aes_iec958 iec958_default;
	spinlock_t iec958_default_lock; /* Protects iec958_default */

	/* Runtime data */
	int enabled;
	struct snd_stm_conv *attached_conv;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Internal routines
 */

/* Such a empty (zeroed) structure is pretty useful later... ;-) */
static struct snd_aes_iec958 snd_stm_conv_i2s_spdif_iec958_zeroed;



#define CHA_STA_TRIES 50000

static int snd_stm_conv_i2s_spdif_iec958_set(struct snd_stm_conv_i2s_spdif
		*conv_i2s_spdif, struct snd_aes_iec958 *iec958)
{
	int i, j, ok;
	unsigned long status[6];

	snd_stm_printt("snd_stm_conv_i2s_spdif_iec958_set(conv_i2s_spdif=%p, "
			"iec958=%p)\n", conv_i2s_spdif, iec958);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);

	/* I2S to SPDIF converter should be used only for playing
	 * PCM (non compressed) data, so validity bit should be always
	 * zero... (it means "valid linear PCM data") */
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_VAL, VALIDITY_BITS, 0);

	/* Well... User data bit... Frankly speaking there is no way
	 * of correctly setting them with a mechanism provided by
	 * converter hardware, so it is better not to do this at all... */
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_DATA,
			USER_DATA_BITS, 0);
	snd_assert(memcmp(snd_stm_conv_i2s_spdif_iec958_zeroed.subcode,
			iec958->subcode, sizeof(iec958->subcode)) == 0);

	if (conv_i2s_spdif->info->full_channel_status == 0) {
		/* Converter hardware by default puts every single bit of
		 * status to separate SPDIF subframe (instead of putting
		 * the same bit to both left and right subframes).
		 * So we have to prepare a "duplicated" version of
		 * status bits... Note that in such way status will be
		 * transmitted twice in every block! This is definitely
		 * out of spec, but fortunately most of receivers pay
		 * attention only to first 36 bits... */

		for (i = 0; i < 6; i++) {
			unsigned long word = 0;

			for (j = 1; j >= 0; j--) {
				unsigned char byte = iec958->status[i * 2 + j];
				int k;

				for (k = 0; k < 8; k++) {
					word |= ((byte & 0x80) != 0);
					if (!(j == 0 && k == 7)) {
						word <<= 2;
						byte <<= 1;
					}
				}
			}

			status[i] = word | (word << 1);
		}
	} else {
		/* Fortunately in some hardware there is a "sane" mode
		 * of channel status registers operation... :-) */

		for (i = 0; i < 6; i++)
			status[i] = iec958->status[i * 4] |
					iec958->status[i * 4 + 1] << 8 |
					iec958->status[i * 4 + 2] << 16 |
					iec958->status[i * 4 + 3] << 24;
	}

	/* Set converter's channel status registers - they are realised
	 * in such a ridiculous way that write to them is enabled only
	 * in (about) 300us time window after CHL_STS_BUFF_EMPTY bit
	 * is asserted... And this happens once every 2ms (only when
	 * converter is enabled and gets data...) */

	ok = 0;
	for (i = 0; i < CHA_STA_TRIES; i++) {
		if (REGFIELD_PEEK(conv_i2s_spdif->base,
				AUD_SPDIFPC_STA, CHL_STS_BUFF_EMPTY)) {
			for (j = 0; j < 6; j++)
				REGISTER_POKE_N(conv_i2s_spdif->base,
						AUD_SPDIFPC_CHA_STA,
						j, status[j]);
			ok = 1;
			for (j = 0; j < 6; j++)
				if (REGISTER_PEEK_N(conv_i2s_spdif->base,
						AUD_SPDIFPC_CHA_STA,
						j) != status[j]) {
					ok = 0;
					break;
				}
			if (ok)
				break;
		}
	}
	if (!ok) {
		snd_stm_printe("WARNING! Failed to set channel status registers"
				" for converter %s! (tried %d times)\n",
				conv_i2s_spdif->device->bus_id, i);
		return -EINVAL;
	}

	snd_stm_printt("Channel status registers set successfully "
			"in %i tries.", i);

	/* Set SPDIF player's VUC registers (these are used only
	 * for mute data formatting, and it should never happen ;-) */

	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_SUV,
			VAL_LEFT, 0);
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_SUV,
			VAL_RIGHT, 0);

	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_SUV,
			DATA_LEFT, 0);
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_SUV,
			DATA_RIGHT, 0);

	/* And this time the problem is that SPDIF player lets
	 * to set only first 36 bits of channel status bits...
	 * Hopefully no one needs more ever ;-) And well - at least
	 * it puts channel status bits to both subframes :-) */
	status[0] = iec958->status[0] | iec958->status[1] << 8 |
		iec958->status[2] << 16 | iec958->status[3] << 24;
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CL1,
			CHANNEL_STATUS, status[0]);
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_SUV,
			CH_STA_LEFT, iec958->status[4] & 0xf);
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CR1,
			CH_STA, status[0]);
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_SUV,
			CH_STA_RIGHT, iec958->status[4] & 0xf);

	return 0;
}




/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_i2s_spdif_get_format(struct snd_stm_conv
		*conv)
{
	snd_stm_printt("snd_stm_conv_i2s_spdif_get_format(conv=%p)\n", conv);

	return (SND_STM_FORMAT__I2S | SND_STM_FORMAT__OUTPUT_SUBFRAME_32_BITS);
}

static int snd_stm_conv_i2s_spdif_get_oversampling(struct snd_stm_conv *conv)
{
	int oversampling = 0;
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = container_of(conv,
			struct snd_stm_conv_i2s_spdif, conv);

	snd_stm_printt("snd_stm_conv_i2s_spdif_get_oversampling(conv=%p)\n",
			conv);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);

	if (conv_i2s_spdif->attached_conv)
		oversampling = snd_stm_conv_get_oversampling(
				conv_i2s_spdif->attached_conv);

	if (oversampling == 0)
		oversampling = DEFAULT_OVERSAMPLING;

	return oversampling;
}

static int snd_stm_conv_i2s_spdif_enable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = container_of(conv,
			struct snd_stm_conv_i2s_spdif, conv);
	int oversampling;
	struct snd_aes_iec958 iec958;

	snd_stm_printt("snd_stm_conv_i2s_spdif_enable(conv=%p)\n", conv);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);
	snd_assert(!conv_i2s_spdif->enabled, return -EINVAL);

	snd_stm_printt("Enabling I2S to SPDIF converter '%s'.\n",
			conv_i2s_spdif->device->bus_id);

	conv_i2s_spdif->attached_conv =
			snd_stm_conv_get_attached(conv_i2s_spdif->device);
	if (conv_i2s_spdif->attached_conv) {
		int result = snd_stm_conv_enable(conv_i2s_spdif->attached_conv);
		if (result != 0) {
			snd_stm_printe("Can't enable attached converter!\n");
			return result;
		}
	}

	oversampling = snd_stm_conv_i2s_spdif_get_oversampling(conv);
	snd_assert(oversampling > 0, return -EINVAL);
	snd_assert((oversampling % 128) == 0, return -EINVAL);

	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CFG,
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, DEVICE_EN, ENABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, SW_RESET, RUNNING) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, FIFO_EN, ENABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, AUDIO_WORD_SIZE,
					24_BITS) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, REQ_ACK_EN, ENABLED));
	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CTRL,
			REGFIELD_VALUE(AUD_SPDIFPC_CTRL, OPERATION, PCM) |
			REGFIELD_VALUE(AUD_SPDIFPC_CTRL, ROUNDING,
					NO_ROUNDING));
	REGFIELD_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CTRL, DIVIDER,
			oversampling / 128);

	/* Full channel status processing - an undocumented feature that
	 * exists in some hardware... Normally channel status registers
	 * provides bits for each subframe, so only for 96 frames (a half
	 * of SPDIF block) - pathetic! ;-) Setting bit 6 of config register
	 * enables a mode in which channel status bits in L/R subframes
	 * are identical, and whole block is served... */
	if (conv_i2s_spdif->info->full_channel_status)
		REGFIELD_SET(conv_i2s_spdif->base, AUD_SPDIFPC_CFG,
				CHA_STA_BITS, FRAME);

	spin_lock(&conv_i2s_spdif->iec958_default_lock);
	iec958 = conv_i2s_spdif->iec958_default;
	spin_unlock(&conv_i2s_spdif->iec958_default_lock);
	if (snd_stm_conv_i2s_spdif_iec958_set(conv_i2s_spdif, &iec958) != 0)
		snd_stm_printe("WARNING! Can't set channel status "
				"registers!\n");

	conv_i2s_spdif->enabled = 1;

	return 0;
}

static int snd_stm_conv_i2s_spdif_disable(struct snd_stm_conv *conv)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = container_of(conv,
			struct snd_stm_conv_i2s_spdif, conv);

	snd_stm_printt("snd_stm_conv_i2s_spdif_disable(conv=%p)\n", conv);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);
	snd_assert(conv_i2s_spdif->enabled, return -EINVAL);

	snd_stm_printt("Disabling I2S to SPDIF converter '%s'\n",
			conv_i2s_spdif->device->bus_id);

	if (conv_i2s_spdif->attached_conv) {
		int result = snd_stm_conv_disable(
				conv_i2s_spdif->attached_conv);

		if (result != 0) {
			snd_stm_printe("Can't disable attached converter!\n");
			return result;
		}
	}

	if (snd_stm_conv_i2s_spdif_iec958_set(conv_i2s_spdif,
			&snd_stm_conv_i2s_spdif_iec958_zeroed) != 0)
		snd_stm_printe("WARNING! Failed to clear channel status "
				"registers!\n");

	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CFG,
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, DEVICE_EN, DISABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, SW_RESET, RESET) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, FIFO_EN, DISABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, REQ_ACK_EN, DISABLED));
	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CTRL,
			REGFIELD_VALUE(AUD_SPDIFPC_CTRL, OPERATION, OFF));

	conv_i2s_spdif->enabled = 0;

	return 0;
}

static int snd_stm_conv_i2s_spdif_mute(struct snd_stm_conv *conv)
{
	int result = 0;
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = container_of(conv,
			struct snd_stm_conv_i2s_spdif, conv);

	snd_stm_printt("snd_stm_conv_i2s_spdif_mute(conv=%p)\n", conv);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);
	snd_assert(conv_i2s_spdif->enabled, return -EINVAL);

	if (conv_i2s_spdif->attached_conv)
		result = snd_stm_conv_mute(conv_i2s_spdif->attached_conv);

	return result;
}

static int snd_stm_conv_i2s_spdif_unmute(struct snd_stm_conv *conv)
{
	int result = 0;
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = container_of(conv,
			struct snd_stm_conv_i2s_spdif, conv);

	snd_stm_printt("snd_stm_conv_i2s_spdif_unmute(conv=%p)\n", conv);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);
	snd_assert(conv_i2s_spdif->enabled, return -EINVAL);

	if (conv_i2s_spdif->attached_conv)
		result = snd_stm_conv_unmute(conv_i2s_spdif->attached_conv);

	return result;
}



/*
 * ALSA controls
 */

static int snd_stm_conv_i2s_spdif_ctl_default_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif =
			snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_conv_i2s_spdif_ctl_default_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);

	spin_lock(&conv_i2s_spdif->iec958_default_lock);
	ucontrol->value.iec958 = conv_i2s_spdif->iec958_default;
	spin_unlock(&conv_i2s_spdif->iec958_default_lock);

	return 0;
}

static int snd_stm_conv_i2s_spdif_ctl_default_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif =
			snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printt("snd_stm_conv_i2s_spdif_ctl_default_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);

	spin_lock(&conv_i2s_spdif->iec958_default_lock);
	if (snd_stm_iec958_cmp(&conv_i2s_spdif->iec958_default,
				&ucontrol->value.iec958) != 0) {
		conv_i2s_spdif->iec958_default = ucontrol->value.iec958;
		changed = 1;
	}
	spin_unlock(&conv_i2s_spdif->iec958_default_lock);

	return changed;
}

static struct snd_kcontrol_new __initdata snd_stm_conv_i2s_spdif_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_iec958_info,
		.get = snd_stm_conv_i2s_spdif_ctl_default_get,
		.put = snd_stm_conv_i2s_spdif_ctl_default_put,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, CON_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_con,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, PRO_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_pro,
	},
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_SPDIFPC_%s (offset 0x%03x) =" \
				" 0x%08x\n", __stringify(r), \
				AUD_SPDIFPC_##r, \
				REGISTER_PEEK(conv_i2s_spdif->base, \
				AUD_SPDIFPC_##r))

static void snd_stm_conv_i2s_spdif_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif =
		entry->private_data;
	int i;

	snd_assert(conv_i2s_spdif, return);
	snd_stm_magic_assert(conv_i2s_spdif, return);

	DUMP_REGISTER(CFG);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(IT_EN);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(IT_CLR);
	DUMP_REGISTER(VAL);
	DUMP_REGISTER(DATA);
	for (i = 0; i <= 5; i++)
		snd_iprintf(buffer, "AUD_SPDIFPC_CHA_STA%d_CHANNEL_STATUS_BITS"
				" (offset 0x%03x) = 0x%08x\n",
				i, AUD_SPDIFPC_CHA_STA(i),
				REGISTER_PEEK_N(conv_i2s_spdif->base,
				AUD_SPDIFPC_CHA_STA, i));
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(SPDIFSTA);
	DUMP_REGISTER(PAUSE);
	DUMP_REGISTER(DATA_BURST);
	DUMP_REGISTER(PA_PB);
	DUMP_REGISTER(PC_PD);
	DUMP_REGISTER(CL1);
	DUMP_REGISTER(CR1);
	DUMP_REGISTER(SUV);
}

static int snd_stm_conv_i2s_spdif_register(struct snd_device *snd_device)
{
	int result;
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = snd_device->device_data;

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);
	snd_assert(!conv_i2s_spdif->enabled, return -EINVAL);

	/* Initialize converter's input & SPDIF player as disabled */

	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CFG,
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, DEVICE_EN, DISABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, SW_RESET, RESET) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, FIFO_EN, DISABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, REQ_ACK_EN, DISABLED));
	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CTRL,
			REGFIELD_VALUE(AUD_SPDIFPC_CTRL, OPERATION, OFF));

	/* Additional procfs info */

	snd_stm_info_register(&conv_i2s_spdif->proc_entry,
			conv_i2s_spdif->device->bus_id,
			snd_stm_conv_i2s_spdif_dump_registers,
			conv_i2s_spdif);

	/* Create ALSA controls */

	result = snd_stm_conv_add_route_ctl(conv_i2s_spdif->device,
			snd_device->card, conv_i2s_spdif->info->card_device);
	if (result < 0) {
		snd_stm_printe("Failed to add converter route control!\n");
		return result;
	}

	return 0;
}

static int snd_stm_conv_i2s_spdif_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif = snd_device->device_data;

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);
	snd_assert(!conv_i2s_spdif->enabled, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(conv_i2s_spdif->proc_entry);

	/* Power done mode, just to be sure :-) */

	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CFG,
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, DEVICE_EN, DISABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, SW_RESET, RESET) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, FIFO_EN, DISABLED) |
			REGFIELD_VALUE(AUD_SPDIFPC_CFG, REQ_ACK_EN, DISABLED));
	REGISTER_POKE(conv_i2s_spdif->base, AUD_SPDIFPC_CTRL,
			REGFIELD_VALUE(AUD_SPDIFPC_CTRL, OPERATION, OFF));

	return 0;
}

static struct snd_device_ops snd_stm_conv_i2s_spdif_snd_device_ops = {
	.dev_register = snd_stm_conv_i2s_spdif_register,
	.dev_disconnect = snd_stm_conv_i2s_spdif_disconnect,
};



/*
 * Platform driver routines
 */

static int __init snd_stm_conv_i2s_spdif_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_i2s_spdif_info *conv_i2s_spdif_info =
			pdev->dev.platform_data;
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif;
	struct snd_card *card;
	struct device *player_device;
	int i;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	snd_assert(conv_i2s_spdif_info != NULL, return -EINVAL);

	conv_i2s_spdif = kzalloc(sizeof(*conv_i2s_spdif), GFP_KERNEL);
	if (!conv_i2s_spdif) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_i2s_spdif);
	conv_i2s_spdif->info = conv_i2s_spdif_info;
	conv_i2s_spdif->device = &pdev->dev;
	spin_lock_init(&conv_i2s_spdif->iec958_default_lock);

	/* Converter interface initialization */

	conv_i2s_spdif->conv.name = conv_i2s_spdif_info->name;
	conv_i2s_spdif->conv.get_format = snd_stm_conv_i2s_spdif_get_format;
	conv_i2s_spdif->conv.get_oversampling =
			snd_stm_conv_i2s_spdif_get_oversampling;
	conv_i2s_spdif->conv.enable = snd_stm_conv_i2s_spdif_enable;
	conv_i2s_spdif->conv.disable = snd_stm_conv_i2s_spdif_disable;
	conv_i2s_spdif->conv.mute = snd_stm_conv_i2s_spdif_mute;
	conv_i2s_spdif->conv.unmute = snd_stm_conv_i2s_spdif_unmute;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv_i2s_spdif->mem_region,
			&conv_i2s_spdif->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Get connections */

	snd_assert(conv_i2s_spdif_info->card_id, return -EINVAL);
	card = snd_stm_cards_get(conv_i2s_spdif_info->card_id);
	snd_assert(card, return -EINVAL);
	snd_printd("This I2S-SPDIF converter will be a member of a card "
			"'%s'.\n", card->id);

	snd_assert(conv_i2s_spdif_info->source_bus_id != NULL,
			return -EINVAL);
	snd_printd("This I2S-SPDIF converter is attached to PCM player '%s'.\n",
			conv_i2s_spdif_info->source_bus_id);
	player_device = snd_stm_find_device(NULL,
			conv_i2s_spdif_info->source_bus_id);
	snd_assert(player_device != NULL, return -EINVAL);
	result = snd_stm_conv_attach(&conv_i2s_spdif->conv, player_device);
	if (result < 0) {
		snd_stm_printe("Can't attach to PCM player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_i2s_spdif,
			&snd_stm_conv_i2s_spdif_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA controls */

	result = 0;
	for (i = 0; i < ARRAY_SIZE(snd_stm_conv_i2s_spdif_ctls); i++) {
		snd_stm_conv_i2s_spdif_ctls[i].device =
				conv_i2s_spdif->info->card_device;
		result |= snd_ctl_add(card,
				snd_ctl_new1(&snd_stm_conv_i2s_spdif_ctls[i],
				conv_i2s_spdif));
		/* TODO: index per card */
		snd_stm_conv_i2s_spdif_ctls[i].index++;
	}
	if (result < 0) {
		snd_stm_printe("Failed to add all ALSA controls!\n");
		goto error_controls;
	}

	/* Done now */

	platform_set_drvdata(pdev, &conv_i2s_spdif->conv);

	snd_printd("--- Probed successfully!\n");

	return result;

error_controls:
error_device:
error_attach:
	snd_stm_memory_release(conv_i2s_spdif->mem_region,
			conv_i2s_spdif->base);
error_memory_request:
	snd_stm_magic_clear(conv_i2s_spdif);
	kfree(conv_i2s_spdif);
error_alloc:
	return result;
}

static int snd_stm_conv_i2s_spdif_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_i2s_spdif *conv_i2s_spdif =
			container_of(platform_get_drvdata(pdev),
			struct snd_stm_conv_i2s_spdif, conv);

	snd_assert(conv_i2s_spdif, return -EINVAL);
	snd_stm_magic_assert(conv_i2s_spdif, return -EINVAL);

	snd_stm_memory_release(conv_i2s_spdif->mem_region,
			conv_i2s_spdif->base);

	snd_stm_magic_clear(conv_i2s_spdif);
	kfree(conv_i2s_spdif);

	return 0;
}

static struct platform_driver snd_stm_conv_i2s_spdif_driver = {
	.driver = {
		.name = "conv_i2s-spdif",
	},
	.probe = snd_stm_conv_i2s_spdif_probe,
	.remove = snd_stm_conv_i2s_spdif_remove,
};



/*
 * Initialization
 */

int __init snd_stm_conv_i2s_spdif_init(void)
{
	return platform_driver_register(&snd_stm_conv_i2s_spdif_driver);
}

void snd_stm_conv_i2s_spdif_cleanup(void)
{
	platform_driver_unregister(&snd_stm_conv_i2s_spdif_driver);
}
