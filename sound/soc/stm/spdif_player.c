/*
 *   STMicroelectronics System-on-Chips' SPDIF player driver
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
#include <linux/interrupt.h>
#include <linux/bpa2.h>
#include <asm/cacheflush.h>
#include <linux/stm/soc.h>
#include <linux/stm/stm-dma.h>
#include <linux/stm/registers.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/asoundef.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 8 /* See common.h debug features */
#include "common.h"



/*
 * Some hardware-related definitions
 */

#define INIT_SAMPLING_RATE 32000

#define DEFAULT_OVERSAMPLING 128

/* The sample count field (MEMREAD in CTRL register) is 17 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 17) - 1)



/*
 * SPDIF player instance definition
 */

enum snd_stm_spdif_player_input_mode {
	SNDRV_STM_SPDIF_INPUT_MODE_NORMAL,
	SNDRV_STM_SPDIF_INPUT_MODE_RAW
};

enum snd_stm_spdif_player_encoding_mode {
	SNDRV_STM_SPDIF_ENCODING_MODE_PCM,
	SNDRV_STM_SPDIF_ENCODING_MODE_ENCODED
};

struct snd_stm_spdif_player {
	/* System informations */
	const char *name;
	const char *bus_id;
	struct snd_pcm *pcm;

	/* Resources */
	struct resource *mem_region;
	void *base;
	unsigned long fifo_phys_address;
	unsigned int irq;
	int fdma_channel;
	struct stm_dma_req *fdma_request;

	/* Environment settings */
	struct device *fsynth;
	int fsynth_channel;

	/* Board-specific settings */
	unsigned int oversampling;

	/* Default configuration */
	enum snd_stm_spdif_player_input_mode input_mode_default;
	enum snd_stm_spdif_player_encoding_mode encoding_mode_default;
	spinlock_t modes_default_lock; /* Protects above two enums */
	struct snd_aes_iec958 vuc_default;
	spinlock_t vuc_default_lock; /* Protects vuc_default */

	/* Runtime data */
	void *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	struct stm_dma_params fdma_params;
	struct snd_aes_iec958 vuc_stream;
	spinlock_t vuc_stream_lock; /* Protects vuc_stream */
	enum snd_stm_spdif_player_input_mode input_mode;
	enum snd_stm_spdif_player_encoding_mode encoding_mode;
	struct snd_aes_iec958 vuc;

	snd_stm_magic_field;
};



/*
 * Playing engine implementation
 */

static irqreturn_t snd_stm_spdif_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_spdif_player *spdif_player = dev_id;
	unsigned int status;

	snd_stm_printt("snd_stm_spdif_player_irq_handler(irq=%d, "
			"dev_id=0x%p)\n", irq, dev_id);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = REGISTER_PEEK(spdif_player->base, AUD_SPDIF_ITS);
	REGISTER_POKE(spdif_player->base, AUD_SPDIF_ITS_CLR, status);
	preempt_enable();

	/* Underflow? */
	if (unlikely(status & REGFIELD_VALUE(AUD_SPDIF_ITS, UNF, PENDING))) {
		snd_stm_printe("Underflow detected in SPDIF player '%s'!\n",
				spdif_player->bus_id);
		result = IRQ_HANDLED;
	}

	/* Period successfully played */
	if (likely(status & REGFIELD_VALUE(AUD_SPDIF_ITS, NSAMPLE, PENDING)))
		do {
			snd_assert(spdif_player->substream, break);

			snd_stm_printt("Period elapsed ('%s')\n",
					spdif_player->bus_id);
			snd_pcm_period_elapsed(spdif_player->substream);

			result = IRQ_HANDLED;
		} while (0);

	/* Some alien interrupt??? */
	snd_assert(result == IRQ_HANDLED);

	return result;
}

/* In normal mode we are preparing SPDIF formating "manually".
 * It means:
 * 1. A lot of parsing...
 * 2. MMAPing is impossible...
 * 3. We can handle different formats and use ALSA standard
 *    structure for channel status & user data: snd_aes_iec958 */
static struct snd_pcm_hardware snd_stm_spdif_player_hw_normal = {
	.info		= (SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S24_LE),

	.rates		= (SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000),
	.rate_min	= 32000,
	.rate_max	= 48000,

	.channels_min	= 2,
	.channels_max	= 2,

	.periods_min	= 1,     /* TODO: I would say 2... */
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note 1: that these value must be also calculated not to exceed
	 * NSAMPLE interrupt counter size (19 bits) - MAX_SAMPLES_PER_PERIOD.
	 * Note 2: period_bytes_min defines minimum time between period
	 * (NSAMPLE) interrupts... Keep it large enough not to kill
	 * the system... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

/* In raw mode SPDIF formatting must be prepared by user. Every sample
 * (one channel) is a 32 bits word containing up to 24 bits of data
 * and 4 SPDIF control bits: V(alidty flag), U(ser data), C(hannel status),
 * P(arity bit):
 *
 *      +---------------+---------------+---------------+---------------+
 *      |3|3|2|2|2|2|2|2|2|2|2|2|1|1|1|1|1|1|1|1|1|1|0|0|0|0|0|0|0|0|0|0|
 * bit: |1|0|9|8|6|7|5|4|3|2|1|0|9|8|7|6|5|4|3|2|1|0|9|8|7|6|5|4|3|2|1|0|
 *      +---------------+---------------+---------------+-------+-------+
 *      |M                                             L|       |       |
 *      |S          sample data (up to 24 bits)        S|0|0|0|0|V|U|C|0|
 *      |B                                             B|       |       |
 *      +-----------------------------------------------+-------+-------+
 *
 * SPDIF player sends subframe's sync preamble first (thanks at least
 * for this ;-)), than data starting from LSB (so samples smaller than
 * 24 bits should be aligned to MSB and have zeros as LSBs), than VUC bits
 * and finally adds a parity bit (thanks again ;-).
 */
static struct snd_pcm_hardware snd_stm_spdif_player_hw_raw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),

	.rates		= (SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000),
	.rate_min	= 32000,
	.rate_max	= 48000,

	.channels_min	= 2,
	.channels_max	= 2,

	.periods_min	= 1,     /* TODO: I would say 2... */
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* See above... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static int snd_stm_spdif_player_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_spdif_player_open(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get default data */

	spin_lock(&spdif_player->vuc_default_lock);
	spdif_player->vuc_stream = spdif_player->vuc_default;
	spin_unlock(&spdif_player->vuc_default_lock);

	spin_lock(&spdif_player->modes_default_lock);
	spdif_player->encoding_mode = spdif_player->encoding_mode_default;
	spdif_player->input_mode = spdif_player->input_mode_default;
	spin_unlock(&spdif_player->modes_default_lock);

	/* Set up constraints & pass hardware capabilities info to ALSA */

	/* It is better when buffer size is an integer multiple of period
	 * size... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		snd_stm_printe("Can't set periods constraint!\n");
		return result;
	}

	if (spdif_player->input_mode == SNDRV_STM_SPDIF_INPUT_MODE_NORMAL)
		runtime->hw = snd_stm_spdif_player_hw_normal;
	else
		runtime->hw = snd_stm_spdif_player_hw_raw;

	return 0;
}

static int snd_stm_spdif_player_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_spdif_player_close(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	return 0;
}

static int snd_stm_spdif_player_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes;

	snd_stm_printt("snd_stm_spdif_player_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	spdif_player->buffer = bigphysarea_alloc(buffer_bytes);
	/* TODO: move to BPA2, use pcm lib as fallback... */
	if (!spdif_player->buffer) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
				buffer_bytes, spdif_player->bus_id);
		return -ENOMEM;
	}

	runtime->dma_addr = virt_to_phys(spdif_player->buffer);
	runtime->dma_area = ioremap_nocache(runtime->dma_addr, buffer_bytes);
	runtime->dma_bytes = buffer_bytes;

	snd_stm_printt("Allocated buffer for %s: buffer=0x%p, "
			"dma_addr=0x%08x, dma_area=0x%p, "
			"dma_bytes=%u\n", spdif_player->bus_id,
			spdif_player->buffer, runtime->dma_addr,
			runtime->dma_area, runtime->dma_bytes);

	/* Configure FDMA transfer */

	/* TODO: try to use SPDIF FMDA channel */

	dma_params_init(&spdif_player->fdma_params, MODE_PACED,
			STM_DMA_LIST_CIRC);

	dma_params_DIM_1_x_0(&spdif_player->fdma_params);

	dma_params_req(&spdif_player->fdma_params, spdif_player->fdma_request);

	dma_params_addrs(&spdif_player->fdma_params, runtime->dma_addr,
			spdif_player->fifo_phys_address, buffer_bytes);

	result = dma_compile_list(spdif_player->fdma_channel,
				&spdif_player->fdma_params, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for player"
				" '%s'!\n", spdif_player->bus_id);
		return -EINVAL;
	}

	return 0;
}

static int snd_stm_spdif_player_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_spdif_player_hw_free(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* This callback may be called more than once... */

	if (spdif_player->buffer) {
		/* Dispose buffer */

		snd_stm_printt("Freeing buffer for %s: buffer=0x%p, "
				"dma_addr=0x%08x, dma_area=0x%p, "
				"dma_bytes=%u\n", spdif_player->bus_id,
				spdif_player->buffer, runtime->dma_addr,
				runtime->dma_area, runtime->dma_bytes);

		iounmap(runtime->dma_area);
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;

		/* TODO: symmetrical to the above (BPA2 etc.) */
		bigphysarea_free(spdif_player->buffer, runtime->dma_bytes);
		spdif_player->buffer = NULL;

		/* Dispose FDMA parameters */

		dma_params_free(&spdif_player->fdma_params);
	}

	return 0;
}

static int snd_stm_spdif_player_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_spdif_player_prepare(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);
	snd_assert(runtime->period_size * runtime->channels <
			MAX_SAMPLES_PER_PERIOD, return -EINVAL);

	/* Configure SPDIF-PCM synchronisation */

	/* TODO */

	/* Set up frequency synthesizer */

	snd_stm_fsynth_set_frequency(spdif_player->fsynth,
			spdif_player->fsynth_channel,
			runtime->rate * spdif_player->oversampling);

	/* Configure SPDIF player frequency divider
	 *
	 *                        Fdacclk
	 * divider = ------------------------------- =
	 *            2 * Fs * bits_in_output_frame
	 *
	 *            Fs * oversampling     oversampling
	 *         = ------------------- = --------------
	 *            2 * Fs * (32 * 2)         128
	 * where:
	 *   - Fdacclk - frequency of DAC clock signal, known also as PCMCLK,
	 *               MCLK (master clock), "system clock" etc.
	 *   - Fs - sampling rate (frequency)
	 */

	REGFIELD_POKE(spdif_player->base, AUD_SPDIF_CTRL, CLK_DIV,
			spdif_player->oversampling / 128);

	/* Configure NSAMPLE interrupt (in samples,
	 * so period size times channels) */

	REGFIELD_POKE(spdif_player->base, AUD_SPDIF_CTRL, MEMREAD,
			runtime->period_size * 2);

	return 0;
}

static inline int snd_stm_spdif_player_start(struct snd_pcm_substream
		*substream)
{
	int result;
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_spdif_player_start(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* Un-reset SPDIF player */

	REGFIELD_SET(spdif_player->base, AUD_SPDIF_RST, SRSTP, RUNNING);

	/* Launch FDMA transfer */

	result = dma_xfer_list(spdif_player->fdma_channel,
			&spdif_player->fdma_params);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for player '%s'!\n",
				spdif_player->bus_id);
		return -EINVAL;
	}

	/* Launch SPDIF player */

	spdif_player->substream = substream;

	if (spdif_player->encoding_mode == SNDRV_STM_SPDIF_ENCODING_MODE_PCM)
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE, PCM);
	else
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE, ENCODED);

	/* Enable player interrupts */

	REGFIELD_SET(spdif_player->base, AUD_SPDIF_IT_EN_SET, NSAMPLE, SET);
	REGFIELD_SET(spdif_player->base, AUD_SPDIF_IT_EN_SET, UNF, SET);

	return 0;
}

static inline int snd_stm_spdif_player_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_spdif_player_stop(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* Disable interrupts */

	REGFIELD_SET(spdif_player->base, AUD_SPDIF_IT_EN_CLR, NSAMPLE, CLEAR);
	REGFIELD_SET(spdif_player->base, AUD_SPDIF_IT_EN_CLR, UNF, CLEAR);

	/* Stop SPDIF player */

	REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE, OFF);
	spdif_player->substream = NULL;

	/* Stop FDMA transfer */

	dma_stop_channel(spdif_player->fdma_channel);

	/* Reset SPDIF player */
	REGFIELD_SET(spdif_player->base, AUD_SPDIF_RST, SRSTP, RESET);

	return 0;
}

static inline int snd_stm_spdif_player_pause(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_spdif_player *spdif_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_spdif_player_pause(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* "Mute" player
	 * Documentation describes this mode in a wrong way - data is _not_
	 * consumed in the "mute" mode, so it is actually a "pause" mode */

	if (spdif_player->encoding_mode == SNDRV_STM_SPDIF_ENCODING_MODE_PCM)
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE,
				MUTE_PCM_NULL);
	else
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE,
				MUTE_PAUSE_BURSTS);

	return 0;
}

static inline int snd_stm_spdif_player_release(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_spdif_player_release(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* "Unmute" player */

	if (spdif_player->encoding_mode == SNDRV_STM_SPDIF_ENCODING_MODE_PCM)
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE, PCM);
	else
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, MODE, ENCODED);

	return 0;
}

static int snd_stm_spdif_player_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printt("snd_stm_spdif_player_trigger(substream=0x%p,"
			" command=%d)\n", substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_spdif_player_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_spdif_player_stop(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return snd_stm_spdif_player_pause(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return snd_stm_spdif_player_release(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_spdif_player_pointer(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue;
	snd_pcm_uframes_t pointer;

	snd_stm_printt("snd_stm_spdif_player_pointer(substream=0x%p)\n",
			substream);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	residue = get_dma_residue(spdif_player->fdma_channel);
	pointer = bytes_to_frames(runtime, runtime->dma_bytes - residue);

	snd_stm_printt("FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printt("... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

#define GET_SAMPLE(kernel_var, user_ptr, memory_format) \
	do { \
		__get_user(kernel_var, (memory_format __user *)user_ptr); \
		(*((memory_format __user **)&user_ptr))++; \
	} while (0);

static int snd_stm_spdif_player_copy(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos,
		void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_spdif_player_copy(substream=0x%p, channel=%d,"
			" pos=%lu, buf=0x%p, count=%lu)\n", substream,
			channel, pos, buf, count);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);
	snd_assert(channel == -1, return -EINVAL); /* Interleaved buffer */

	if (spdif_player->input_mode == SNDRV_STM_SPDIF_INPUT_MODE_NORMAL) {
		unsigned long *hwbuf = (unsigned long *)(runtime->dma_area +
				frames_to_bytes(runtime, pos));
		int i;

		if (!access_ok(VERIFY_READ, buf, frames_to_bytes(runtime,
						count)))
			return -EFAULT;

		snd_stm_printt("Formatting SPDIF frame (format=%d)\n",
				runtime->format);

#if 0
		{
			unsigned char data[64];

			copy_from_user(data, buf, 64);

			snd_stm_printt("Input:\n");
			snd_stm_hex_dump(data, 64);
		}
#endif

		for (i = 0; i < count; i++) {
			unsigned long left_subframe, right_subframe;

			switch (runtime->format) {
			case SNDRV_PCM_FORMAT_S32_LE:
				GET_SAMPLE(left_subframe, buf, u32);
				GET_SAMPLE(right_subframe, buf, u32);
				break;
			case SNDRV_PCM_FORMAT_S24_LE:
				/* 24-bits sample are in lower 3 bytes,
				 * while we want them in upper 3... ;-) */
				GET_SAMPLE(left_subframe, buf, u32);
				left_subframe <<= 8;
				GET_SAMPLE(right_subframe, buf, u32);
				right_subframe <<= 8;
				break;
			default:
				left_subframe = 0;  /* To avoid -Os */
				right_subframe = 0; /* compilation warnings */
				snd_assert(0, return -EINVAL);
				break;
			}

			/* TODO: VUC bits, now just 000... */
			left_subframe &= ~0x03;
			right_subframe &= ~0x03;

			*(hwbuf++) = left_subframe;
			*(hwbuf++) = right_subframe;
		}

#if 0
		snd_stm_printt("Output:\n");
		snd_stm_hex_dump(runtime->dma_area +
				frames_to_bytes(runtime, pos), 64);
#endif
		dma_cache_wback(runtime->dma_area +
				frames_to_bytes(runtime, pos),
				frames_to_bytes(runtime, count));
	} else {
		/* RAW mode */
		if (copy_from_user(runtime->dma_area +
				frames_to_bytes(runtime, pos), buf,
				frames_to_bytes(runtime, count)) != 0)
			return -EFAULT;
	}

	return 0;
}

static int snd_stm_spdif_player_silence(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	struct snd_stm_spdif_player *spdif_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_spdif_player_silence(substream=0x%p, "
			"channel=%d, pos=%lu, count=%lu)\n",
			substream, channel, pos, count);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);
	snd_assert(channel == -1, return -EINVAL); /* Interleaved buffer */

	snd_assert(0, return -EINVAL); /* Not implemented yet */
	/* TODO	*/

	return 0;
}

static struct snd_pcm_ops snd_stm_spdif_player_spdif_ops = {
	.open =      snd_stm_spdif_player_open,
	.close =     snd_stm_spdif_player_close,
	.mmap =      snd_stm_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_spdif_player_hw_params,
	.hw_free =   snd_stm_spdif_player_hw_free,
	.prepare =   snd_stm_spdif_player_prepare,
	.trigger =   snd_stm_spdif_player_trigger,
	.pointer =   snd_stm_spdif_player_pointer,
	.copy =      snd_stm_spdif_player_copy,
	.silence =   snd_stm_spdif_player_silence,
};



/*
 * ALSA controls
 */

/* "Main switch" - controls IDLE mode of SPDIF player */

static int snd_stm_spdif_player_ctl_switch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_spdif_player_ctl_switch_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	ucontrol->value.integer.value[0] =
			(REGFIELD_PEEK(spdif_player->base, AUD_SPDIF_CTRL,
			IDLE) == AUD_SPDIF_CTRL__IDLE__VALUE__NORMAL);

	return 0;
}

static int snd_stm_spdif_player_ctl_switch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printt("snd_stm_spdif_player_ctl_switch_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	if (ucontrol->value.integer.value[0])
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, IDLE, NORMAL);
	else
		REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, IDLE, IDLE);

	return changed;
}

/* "Raw Data" switch controls data input mode - "RAW" means that played
 * data are already properly formated (VUC bits); in "normal" mode
 * this data will be added by driver according to setting passed in\
 * following controls */

static int snd_stm_spdif_player_ctl_raw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_spdif_player_ctl_raw_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	spin_lock(&spdif_player->modes_default_lock);
	ucontrol->value.integer.value[0] = (spdif_player->input_mode_default
			== SNDRV_STM_SPDIF_INPUT_MODE_RAW);
	spin_unlock(&spdif_player->modes_default_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_raw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	enum snd_stm_spdif_player_input_mode input_mode;

	snd_stm_printt("snd_stm_spdif_player_ctl_raw_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	if (ucontrol->value.integer.value[0])
		input_mode = SNDRV_STM_SPDIF_INPUT_MODE_RAW;
	else
		input_mode = SNDRV_STM_SPDIF_INPUT_MODE_NORMAL;

	spin_lock(&spdif_player->modes_default_lock);
	changed = (input_mode != spdif_player->input_mode_default);
	spdif_player->input_mode_default = input_mode;
	spin_unlock(&spdif_player->modes_default_lock);

	return changed;
}

/* "Encoded Data" switch selects linear PCM or encoded operation of
 * SPDIF player - the difference is in generating mute data; PCM mode
 * will generate NULL data, encoded - pause bursts */

static int snd_stm_spdif_player_ctl_encoded_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_spdif_player_ctl_encoded_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	spin_lock(&spdif_player->modes_default_lock);
	ucontrol->value.integer.value[0] = (spdif_player->encoding_mode_default
			== SNDRV_STM_SPDIF_ENCODING_MODE_ENCODED);
	spin_unlock(&spdif_player->modes_default_lock);

	return 0;
}

static int snd_stm_spdif_player_ctl_encoded_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	enum snd_stm_spdif_player_encoding_mode encoding_mode;

	snd_stm_printt("snd_stm_spdif_player_ctl_encoded_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	if (ucontrol->value.integer.value[0])
		encoding_mode = SNDRV_STM_SPDIF_ENCODING_MODE_ENCODED;
	else
		encoding_mode = SNDRV_STM_SPDIF_ENCODING_MODE_PCM;

	spin_lock(&spdif_player->modes_default_lock);
	changed = (encoding_mode != spdif_player->encoding_mode_default);
	spdif_player->encoding_mode_default = encoding_mode;
	spin_unlock(&spdif_player->modes_default_lock);

	return changed;
}

/* Three following controls are valid for encoded mode only - they
 * control IEC 61937 preamble and data burst periods (see mentioned
 * standard for more informations) */

static int snd_stm_spdif_player_ctl_preamble_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 16;
	return 0;
}

static int snd_stm_spdif_player_ctl_preamble_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_spdif_player_ctl_preamble_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* TODO */

	return 0;
}

static int snd_stm_spdif_player_ctl_preamble_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printt("snd_stm_spdif_player_ctl_preamble_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* TODO */

	return changed;
}

static int snd_stm_spdif_player_ctl_repetition_info(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;
	return 0;
}

static int snd_stm_spdif_player_ctl_audio_repetition_get(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_spdif_player_ctl_audio_repetition_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* TODO */

	return 0;
}

static int snd_stm_spdif_player_ctl_audio_repetition_put(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printt("snd_stm_spdif_player_ctl_audio_repetition_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* TODO */

	return changed;
}

static int snd_stm_spdif_player_ctl_pause_repetition_get(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_spdif_player_ctl_pause_repetition_get("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* TODO */

	return 0;
}

static int snd_stm_spdif_player_ctl_pause_repetition_put(struct snd_kcontrol
		*kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_spdif_player *spdif_player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printt("snd_stm_spdif_player_ctl_pause_repetition_put("
			"kcontrol=0x%p, ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* TODO */

	return changed;
}

static struct snd_kcontrol_new __initdata snd_stm_spdif_player_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, SWITCH),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_spdif_player_ctl_switch_get,
		.put = snd_stm_spdif_player_ctl_switch_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Raw Data ", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_spdif_player_ctl_raw_get,
		.put = snd_stm_spdif_player_ctl_raw_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Encoded Data ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = snd_stm_spdif_player_ctl_encoded_get,
		.put = snd_stm_spdif_player_ctl_encoded_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Preamble ", PLAYBACK, DEFAULT),
		.info = snd_stm_spdif_player_ctl_preamble_info,
		.get = snd_stm_spdif_player_ctl_preamble_get,
		.put = snd_stm_spdif_player_ctl_preamble_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Audio Burst Period ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_spdif_player_ctl_repetition_info,
		.get = snd_stm_spdif_player_ctl_audio_repetition_get,
		.put = snd_stm_spdif_player_ctl_audio_repetition_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Pause Burst Period ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_spdif_player_ctl_repetition_info,
		.get = snd_stm_spdif_player_ctl_pause_repetition_get,
		.put = snd_stm_spdif_player_ctl_pause_repetition_put,
	}
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_SPDIF_%s (offset 0x%02x) =" \
				" 0x%08x\n", __stringify(r), \
				AUD_SPDIF_##r, \
				REGISTER_PEEK(spdif_player->base, \
				AUD_SPDIF_##r))

static void snd_stm_spdif_player_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_spdif_player *spdif_player = entry->private_data;

	snd_assert(spdif_player, return);
	snd_stm_magic_assert(spdif_player, return);

	DUMP_REGISTER(RST);
	DUMP_REGISTER(DATA);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(ITS_CLR);
	DUMP_REGISTER(IT_EN);
	DUMP_REGISTER(IT_EN_SET);
	DUMP_REGISTER(IT_EN_CLR);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(PA_PB);
	DUMP_REGISTER(PC_PD);
	DUMP_REGISTER(CL1);
	DUMP_REGISTER(CR1);
	DUMP_REGISTER(CL2_CR2_UV);
	DUMP_REGISTER(PAU_LAT);
	DUMP_REGISTER(BST_FL);
}

static int snd_stm_spdif_player_register(struct snd_device *snd_device)
{
	struct snd_stm_spdif_player *spdif_player = snd_device->device_data;

	snd_stm_printt("snd_stm_spdif_player_register(snd_device=0x%p)\n",
			snd_device);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	/* Set a default clock frequency running for each device.
	 * Not doing this can lead to clocks not starting correctly later,
	 * for reasons that cannot be explained at this time. */
	/* TODO: Check it, maybe obsolete now */
	snd_stm_fsynth_set_frequency(spdif_player->fsynth,
			spdif_player->fsynth_channel,
			INIT_SAMPLING_RATE * spdif_player->oversampling);

	/* Initialize hardware (format etc.) */

	REGFIELD_SET(spdif_player->base, AUD_SPDIF_RST, SRSTP, RESET);

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */
	REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, RND, NO_ROUNDING);

	REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, IDLE, NORMAL);

	/* Hardware stuffing is not implemented yet... */
	REGFIELD_SET(spdif_player->base, AUD_SPDIF_CTRL, STUFFING, SOFTWARE);

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&spdif_player->proc_entry, spdif_player->bus_id,
			snd_stm_spdif_player_dump_registers, spdif_player);

	return 0;
}

static int snd_stm_spdif_player_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_spdif_player *spdif_player = snd_device->device_data;

	snd_stm_printt("snd_stm_spdif_player_unregister(snd_device=0x%p)\n",
			snd_device);

	snd_stm_info_unregister(spdif_player->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_spdif_player_ops = {
	.dev_register = snd_stm_spdif_player_register,
	.dev_disconnect = snd_stm_spdif_player_disconnect,
};



/*
 * Platform driver routines
 */

static struct stm_dma_req_config snd_stm_spdif_player_fdma_request_config = {
	.rw        = REQ_CONFIG_WRITE,
	.opcode    = REQ_CONFIG_OPCODE_4,
	.count     = 1,
	.increment = 0,
	.hold_off  = 0,
	/* .initiator value is defined in platform device resources */
};

static int __init snd_stm_spdif_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct plat_audio_config *config = pdev->dev.platform_data;
	struct snd_stm_component *component;
	struct snd_stm_spdif_player *spdif_player;
	struct snd_card *card;
	int card_device;
	const char *card_id;
	const char *fsynth_bus_id;
	int i;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	component = snd_stm_components_get(pdev->dev.bus_id);
	snd_assert(component, return -EINVAL);

	spdif_player = kzalloc(sizeof(*spdif_player), GFP_KERNEL);
	if (!spdif_player) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(spdif_player);
	spdif_player->bus_id = pdev->dev.bus_id;

	spin_lock_init(&spdif_player->modes_default_lock);
	spin_lock_init(&spdif_player->vuc_default_lock);
	spin_lock_init(&spdif_player->vuc_stream_lock);

	/* Get resources */

	result = snd_stm_memory_request(pdev, &spdif_player->mem_region,
			&spdif_player->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	spdif_player->fifo_phys_address = spdif_player->mem_region->start +
		AUD_SPDIF_DATA;
	snd_printd("FIFO physical address: 0x%lx.\n",
			spdif_player->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &spdif_player->irq,
			snd_stm_spdif_player_irq_handler, spdif_player);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request(pdev, &spdif_player->fdma_channel,
			&spdif_player->fdma_request,
			&snd_stm_spdif_player_fdma_request_config);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	/* Get component caps */

	snd_printd("Player's name is '%s'\n", component->short_name);

	result = snd_stm_cap_get_string(component, "card_id", &card_id);
	snd_assert(result == 0, return -EINVAL);
	card = snd_stm_cards_get(card_id);
	snd_assert(card != NULL, return -EINVAL);
	snd_printd("Player will be a member of a card '%s'...\n", card_id);

	result = snd_stm_cap_get_number(component, "card_device",
			&card_device);
	snd_assert(result == 0, return -EINVAL);
	snd_printd("... as a PCM device no %d.\n", card_device);

	result = snd_stm_cap_get_string(component, "fsynth_bus_id",
			&fsynth_bus_id);
	snd_assert(result == 0, return -EINVAL);
	spdif_player->fsynth = snd_stm_device_get(fsynth_bus_id);
	snd_assert(spdif_player->fsynth, return -EINVAL);
	result = snd_stm_cap_get_number(component, "fsynth_channel",
			&spdif_player->fsynth_channel);
	snd_assert(result == 0, return -EINVAL);
	snd_printd("Player clocked by channel %d of synthesizer %s.\n",
			spdif_player->fsynth_channel, fsynth_bus_id);

	/* Board-specific configuration */

	if (config) {
		spdif_player->oversampling = config->oversampling;
	} else {
		spdif_player->oversampling = DEFAULT_OVERSAMPLING;
	}
	/* Allowed oversampling values (SPDIF subframe is 32 bits long,
	 * so oversampling 192x is forbidden, use ie. 384x instead) */
	snd_assert(spdif_player->oversampling == 128 ||
			spdif_player->oversampling == 256 ||
			spdif_player->oversampling == 384 ||
			spdif_player->oversampling == 512 ||
			spdif_player->oversampling == 768,
			return -EINVAL);

	/* Default VUC data - consumer, PCM linear, no copyright */
	/* TODO: make it configurable per board */
	spdif_player->vuc_default.status[0] = IEC958_AES0_CON_NOT_COPYRIGHT;
	/* All the rest is zeros, which is fine for us :-) */

	/* Preallocate buffer */

	/* TODO */

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, spdif_player,
			&snd_stm_spdif_player_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, card_device, 1, 0,
			&spdif_player->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	spdif_player->pcm->private_data = spdif_player;
	strcpy(spdif_player->pcm->name, component->short_name);

	snd_pcm_set_ops(spdif_player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_stm_spdif_player_spdif_ops);

	/* Create ALSA controls */

	result = 0;
	for (i = 0; i < ARRAY_SIZE(snd_stm_spdif_player_ctls); i++) {
		snd_stm_spdif_player_ctls[i].device = card_device;
		result |= snd_ctl_add(card,
				snd_ctl_new1(&snd_stm_spdif_player_ctls[i],
				spdif_player));
		/* TODO: index per card */
		snd_stm_spdif_player_ctls[i].index++;
	}
	result |= snd_stm_fsynth_add_adjustement_ctl(spdif_player->fsynth,
			spdif_player->fsynth_channel, card, card_device);
	if (result < 0) {
		snd_stm_printe("Failed to add all ALSA controls!\n");
		goto error_controls;
	}

	/* Done now */

	platform_set_drvdata(pdev, spdif_player);

	snd_printd("--- Probed successfully!\n");

	return result;

error_controls:
error_pcm:
	snd_device_free(card, spdif_player);
error_device:
	snd_stm_fdma_release(spdif_player->fdma_channel,
			spdif_player->fdma_request);
error_fdma_request:
	snd_stm_irq_release(spdif_player->irq, spdif_player);
error_irq_request:
	snd_stm_memory_release(spdif_player->mem_region, spdif_player->base);
error_memory_request:
	snd_stm_magic_clear(spdif_player);
	kfree(spdif_player);
error_alloc:
	return result;
}

static int snd_stm_spdif_player_remove(struct platform_device *pdev)
{
	struct snd_stm_spdif_player *spdif_player = platform_get_drvdata(pdev);

	snd_assert(spdif_player, return -EINVAL);
	snd_stm_magic_assert(spdif_player, return -EINVAL);

	snd_stm_fdma_release(spdif_player->fdma_channel,
			spdif_player->fdma_request);
	snd_stm_irq_release(spdif_player->irq, spdif_player);
	snd_stm_memory_release(spdif_player->mem_region, spdif_player->base);

	snd_stm_magic_clear(spdif_player);
	kfree(spdif_player);

	return 0;
}

static struct platform_driver snd_stm_spdif_player_driver = {
	.driver = {
		.name = "spdif_player",
	},
	.probe = snd_stm_spdif_player_probe,
	.remove = snd_stm_spdif_player_remove,
};



/*
 * Initialization
 */

int __init snd_stm_spdif_player_init(void)
{
	return platform_driver_register(&snd_stm_spdif_player_driver);
}

void snd_stm_spdif_player_cleanup(void)
{
	platform_driver_unregister(&snd_stm_spdif_player_driver);
}
