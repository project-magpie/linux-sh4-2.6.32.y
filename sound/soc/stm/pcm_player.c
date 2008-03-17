/*
 *   STMicroelectronics System-on-Chips' PCM player driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Author: Pawel MOLL <pawel.moll@st.com>
 *           Mark Glaisher
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
#include <sound/control.h>
#include <sound/info.h>
#include <sound/pcm_params.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 6 /* See common.h debug features */
#include "common.h"



/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT (SND_STM_FORMAT__I2S | \
		SND_STM_FORMAT__OUTPUT_SUBFRAME_32_BITS)
#define DEFAULT_OVERSAMPLING 256

/* The sample count field (NSAMPLES in CTRL register) is 19 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 19) - 1)

#define MAX_CHANNELS 10



/*
 * PCM player instance definition
 */

struct snd_stm_pcm_player {
	/* System informations */
	struct snd_stm_pcm_player_info *info;
	struct device *device;
	struct snd_pcm *pcm;

	/* Resources */
	struct resource *mem_region;
	void *base;
	unsigned long fifo_phys_address;
	unsigned int irq;
	unsigned int fdma_channel;

	/* Environment settings */
	struct device *fsynth_device;
	int fsynth_channel;
	struct snd_stm_conv *conv;
	struct snd_pcm_hw_constraint_list channels_constraint;

	/* Runtime data */
	void *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	struct stm_dma_params fdma_params;
	struct stm_dma_req *fdma_request;

	snd_stm_magic_field;
};



/*
 * Playing engine implementation
 */

static irqreturn_t snd_stm_pcm_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_pcm_player *pcm_player = dev_id;
	unsigned int status;

	snd_stm_printt("snd_stm_pcm_player_irq_handler(irq=%d, dev_id=0x%p)\n",
			irq, dev_id);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = REGISTER_PEEK(pcm_player->base, AUD_PCMOUT_ITS);
	REGISTER_POKE(pcm_player->base, AUD_PCMOUT_ITS_CLR, status);
	preempt_enable();

	/* Underflow? */
	if (unlikely(status & REGFIELD_VALUE(AUD_PCMOUT_ITS, UNF, PENDING))) {
		snd_stm_printe("Underflow detected in PCM player '%s'!\n",
				pcm_player->device->bus_id);
		result = IRQ_HANDLED;
	}

	/* Period successfully played */
	if (likely(status & REGFIELD_VALUE(AUD_PCMOUT_ITS, NSAMPLE, PENDING)))
		do {
			snd_assert(pcm_player->substream, break);

			snd_stm_printt("Period elapsed ('%s')\n",
					pcm_player->device->bus_id);
			snd_pcm_period_elapsed(pcm_player->substream);

			result = IRQ_HANDLED;
		} while (0);

	/* Some alien interrupt??? */
	snd_assert(result == IRQ_HANDLED);

	return result;
}

static struct snd_pcm_hardware snd_stm_pcm_player_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S16_LE),

	.rates		= (SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_64000 |
				SNDRV_PCM_RATE_88200 |
				SNDRV_PCM_RATE_96000 |
				SNDRV_PCM_RATE_176400 |
				SNDRV_PCM_RATE_192000),
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note 1: that these value must be also calculated not to exceed
	 * NSAMPLE interrupt counter size (19 bits) - MAX_SAMPLES_PER_PERIOD.
	 * Note 2: for 16/16-bits data this counter is a "frames counter",
	 * not "samples counter" (two channels are read as one word).
	 * Note 3: period_bytes_min defines minimum time between period
	 * (NSAMPLE) interrupts... Keep it large enough not to kill
	 * the system... */
	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static int snd_stm_pcm_player_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_pcm_player_open(substream=0x%p)\n", substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	snd_pcm_set_sync(substream);  /* TODO: ??? */

	/* Get attached converter handle */

	pcm_player->conv = snd_stm_conv_get_attached(pcm_player->device);
	if (pcm_player->conv)
		snd_stm_printt("Converter '%s' attached to '%s'...\n",
				pcm_player->conv->name,
				pcm_player->device->bus_id);
	else
		snd_stm_printt("Warning! No converter attached to '%s'!\n",
				pcm_player->device->bus_id);

	/* Set up constraints & pass hardware capabilities info to ALSA */

	result = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&pcm_player->channels_constraint);
	if (result < 0) {
		snd_stm_printe("Can't set channels constraint!\n");
		return result;
	}

	/* It is better when buffer size is an integer multiple of period
	 * size... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		snd_stm_printe("Can't set periods constraint!\n");
		return result;
	}

	/* Make the period (so buffer as well) length (in bytes) a multiply
	 * of a FDMA transfer bytes (which varies depending on channels
	 * number and sample bytes) */
	result = snd_stm_pcm_hw_constraint_transfer_bytes(runtime,
			pcm_player->info->fdma_max_transfer_size * 4);
	if (result < 0) {
		snd_stm_printe("Can't set buffer bytes constraint!\n");
		return result;
	}

	runtime->hw = snd_stm_pcm_player_hw;

	return 0;
}

static int snd_stm_pcm_player_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_player_close(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	return 0;
}

static int snd_stm_pcm_player_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_pcm_player_hw_free(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* This callback may be called more than once... */

	if (pcm_player->buffer) {
		/* Dispose buffer */

		snd_stm_printt("Freeing buffer for %s: buffer=0x%p, "
				"dma_addr=0x%08x, dma_area=0x%p, "
				"dma_bytes=%u\n", pcm_player->device->bus_id,
				pcm_player->buffer, runtime->dma_addr,
				runtime->dma_area, runtime->dma_bytes);

		iounmap(runtime->dma_area);

		/* TODO: symmetrical to the above (BPA2 etc.) */
		bigphysarea_free(pcm_player->buffer, runtime->dma_bytes);

		pcm_player->buffer = NULL;
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;

		/* Dispose FDMA parameters & configuration */

		dma_params_free(&pcm_player->fdma_params);
		dma_req_free(pcm_player->fdma_channel,
				pcm_player->fdma_request);
	}

	return 0;
}

static int snd_stm_pcm_player_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, frame_bytes, transfer_bytes;
	unsigned int transfer_size;
	struct stm_dma_req_config fdma_req_config = {
		.rw        = REQ_CONFIG_WRITE,
		.opcode    = REQ_CONFIG_OPCODE_4,
		.increment = 0,
		.hold_off  = 0,
		.initiator = pcm_player->info->fdma_initiator,
	};

	snd_stm_printt("snd_stm_pcm_player_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* This function may be called many times, so let's be prepared... */
	if (pcm_player->buffer)
		snd_stm_pcm_player_hw_free(substream);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	pcm_player->buffer = bigphysarea_alloc(buffer_bytes);
	/* TODO: move to BPA2, use pcm lib as fallback... */
	if (!pcm_player->buffer) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
				buffer_bytes, pcm_player->device->bus_id);
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	runtime->dma_addr = virt_to_phys(pcm_player->buffer);
	runtime->dma_area = ioremap_nocache(runtime->dma_addr, buffer_bytes);
	runtime->dma_bytes = buffer_bytes;

	snd_stm_printt("Allocated buffer for %s: buffer=0x%p, "
			"dma_addr=0x%08x, dma_area=0x%p, "
			"dma_bytes=%u\n", pcm_player->device->bus_id,
			pcm_player->buffer, runtime->dma_addr,
			runtime->dma_area, runtime->dma_bytes);

	/* Set FDMA transfer size (number of opcodes generated
	 * after request line assertion) */

	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;
	transfer_bytes = snd_stm_pcm_transfer_bytes(frame_bytes,
			pcm_player->info->fdma_max_transfer_size * 4);
	transfer_size = transfer_bytes / 4;
	snd_stm_printt("FDMA request trigger limit and transfer size set to "
			"%d.\n", transfer_size);

	snd_assert(buffer_bytes % transfer_bytes == 0, return -EINVAL);
	snd_assert(transfer_size <= pcm_player->info->fdma_max_transfer_size,
			return -EINVAL);
	fdma_req_config.count = transfer_size;

	snd_assert(transfer_size == 1 || transfer_size % 2 == 0,
			return -EINVAL);
	snd_assert(transfer_size <= AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT__MASK,
			return -EINVAL);
	REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT,
			DMA_REQ_TRIG_LMT, transfer_size);

	/* Configure FDMA transfer */

	pcm_player->fdma_request = dma_req_config(pcm_player->fdma_channel,
			pcm_player->info->fdma_request_line, &fdma_req_config);
	if (!pcm_player->fdma_request) {
		snd_stm_printe("Can't configure FDMA pacing channel for player"
				" '%s'!\n", pcm_player->device->bus_id);
		result = -EINVAL;
		goto error_req_config;
	}

	dma_params_init(&pcm_player->fdma_params, MODE_PACED,
			STM_DMA_LIST_CIRC);

	dma_params_DIM_1_x_0(&pcm_player->fdma_params);

	dma_params_req(&pcm_player->fdma_params, pcm_player->fdma_request);

	dma_params_addrs(&pcm_player->fdma_params, runtime->dma_addr,
			pcm_player->fifo_phys_address, buffer_bytes);

	result = dma_compile_list(pcm_player->fdma_channel,
				&pcm_player->fdma_params, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for player"
				" '%s'!\n", pcm_player->device->bus_id);
		goto error_compile_list;
	}

	return 0;

error_compile_list:
	dma_req_free(pcm_player->fdma_channel,
			pcm_player->fdma_request);
error_req_config:
	iounmap(runtime->dma_area);
	/* TODO: symmetrical to the above (BPA2 etc.) */
	bigphysarea_free(pcm_player->buffer, runtime->dma_bytes);
	pcm_player->buffer = NULL;
	runtime->dma_area = NULL;
	runtime->dma_addr = 0;
	runtime->dma_bytes = 0;
error_buf_alloc:
	return result;
}

static int snd_stm_pcm_player_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int format, lr_pol;
	int oversampling, bits_in_output_frame;

	snd_stm_printt("snd_stm_pcm_player_prepare(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);
	snd_assert(runtime->period_size * runtime->channels <
			MAX_SAMPLES_PER_PERIOD, return -EINVAL);

	/* Configure SPDIF synchronisation */

	/* TODO */

	/* Get format & oversampling value from connected converter */

	if (pcm_player->conv) {
		format = snd_stm_conv_get_format(pcm_player->conv);
		oversampling = snd_stm_conv_get_oversampling(pcm_player->conv);
		if (oversampling == 0)
			oversampling = DEFAULT_OVERSAMPLING;
	} else {
		format = DEFAULT_FORMAT;
		oversampling = DEFAULT_OVERSAMPLING;
	}

	snd_stm_printt("Player %s: sampling frequency %d, oversampling %d\n",
			pcm_player->device->bus_id, runtime->rate,
			oversampling);

	snd_assert(oversampling > 0, return -EINVAL);

	/* For 32 bits subframe oversampling must be a multiple of 128,
	 * for 16 bits - of 64 */
	snd_assert(((format & SND_STM_FORMAT__OUTPUT_SUBFRAME_32_BITS) &&
				(oversampling % 128 == 0)) ||
				(oversampling % 64 == 0), return -EINVAL);

	/* Set up frequency synthesizer */

	snd_stm_fsynth_set_frequency(pcm_player->fsynth_device,
			pcm_player->fsynth_channel,
			runtime->rate * oversampling);

	/* Set up player hardware */

	snd_stm_printt("Player %s format configuration:\n",
			pcm_player->device->bus_id);

	/* Number of bits per subframe (which is one channel sample)
	 * on output - it determines serial clock frequency, which is
	 * 64 times sampling rate for 32 bits subframe (2 channels 32
	 * bits each means 64 bits per frame) and 32 times sampling
	 * rate for 16 bits subframe
	 * (you know why, don't you? :-) */

	switch (format & SND_STM_FORMAT__OUTPUT_SUBFRAME_MASK) {
	case SND_STM_FORMAT__OUTPUT_SUBFRAME_32_BITS:
		snd_stm_printt("- 32 bits per subframe\n");
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT, NBIT, 32_BITS);
#if defined(CONFIG_CPU_SUBTYPE_STX7111)
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				DATA_SIZE, 32_BITS);
#else
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				DATA_SIZE, 24_BITS);
#endif
		bits_in_output_frame = 64; /* frame = 2 * subframe */
		break;
	case SND_STM_FORMAT__OUTPUT_SUBFRAME_16_BITS:
		snd_stm_printt("- 16 bits per subframe\n");
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT, NBIT, 16_BITS);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					DATA_SIZE, 16_BITS);
			bits_in_output_frame = 32; /* frame = 2 * subframe */
			break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Serial audio interface format - for detailed explanation
	 * see ie.:
	 * http://www.cirrus.com/en/pubs/appNote/AN282REV1.pdf */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
			ORDER, MSB_FIRST);

	/* Value of SCLK_EDGE bit in AUD_PCMOUT_FMT register that
	 * actually means "data clocking on the falling edge" -
	 * STx7100 and _some_ cuts of STx7109 have this value
	 * inverted than datasheets claim... (specs say 1) */

	if (pcm_player->info->invert_sclk_edge_falling) {
		snd_stm_printt("Inverted SCLK_EDGE!\n");
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				SCLK_EDGE, RISING);
	} else {
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				SCLK_EDGE, FALLING);
	}

	switch (format & SND_STM_FORMAT__MASK) {
	case SND_STM_FORMAT__I2S:
		snd_stm_printt("- I2S\n");
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT, ALIGN, LEFT);
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				PADDING, 1_CYCLE_DELAY);
		lr_pol = AUD_PCMOUT_FMT__LR_POL__VALUE__LEFT_LOW;
		break;
	case SND_STM_FORMAT__LEFT_JUSTIFIED:
		snd_stm_printt("- left justified\n");
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT, ALIGN, LEFT);
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				PADDING, NO_DELAY);
		lr_pol = AUD_PCMOUT_FMT__LR_POL__VALUE__LEFT_HIGH;
		break;
	case SND_STM_FORMAT__RIGHT_JUSTIFIED:
		snd_stm_printt("- right justified\n");
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT, ALIGN, RIGHT);
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				PADDING, NO_DELAY);
		lr_pol = AUD_PCMOUT_FMT__LR_POL__VALUE__LEFT_HIGH;
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Configure PCM player frequency divider
	 *
	 *             Fdacclk             Fs * oversampling
	 * divider = ----------- = ------------------------------- =
	 *            2 * Fsclk     2 * Fs * bits_in_output_frame
	 *
	 *                  oversampling
	 *         = --------------------------
	 *            2 * bits_in_output_frame
	 * where:
	 *   - Fdacclk - frequency of DAC clock signal, known also as PCMCLK,
	 *               MCLK (master clock), "system clock" etc.
	 *   - Fsclk - frequency of SCLK (serial clock) aka BICK (bit clock)
	 *   - Fs - sampling rate (frequency)
	 *   - bits_in_output_frame - number of bits in output signal _frame_
	 *                (32 or 64, depending on NBIT field of FMT register)
	 */

	REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_CTRL, CLK_DIV,
			oversampling / (2 * bits_in_output_frame));

	/* Configure data memory format & NSAMPLE interrupt */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		/* One data word contains two samples */
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL,
				MEM_FMT, 16_BITS_16_BITS);

		/* Workaround for a problem with L/R channels swap in case of
		 * 16/16 memory model: PCM player expects left channel data in
		 * word's upper two bytes, but due to little endianess
		 * character of our memory there is right channel data there;
		 * the workaround is to invert L/R signal, however it is
		 * cheating, because in such case channel phases are shifted
		 * by one sample...
		 * (ask me for more details if above is not clear ;-)
		 * TODO this somehow better... */
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT,
				LR_POL, !lr_pol);

		/* One word of data is two samples (two channels...) */
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_CTRL, NSAMPLE,
				runtime->period_size * runtime->channels / 2);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bites)"... ;-) */
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL,
				MEM_FMT, 16_BITS_0_BITS);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT, LR_POL,
				lr_pol);

		/* One word of data is one sample, so period size
		 * times channels */
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_CTRL, NSAMPLE,
				runtime->period_size * runtime->channels);
		break;

	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Number of channels... */

	snd_assert(runtime->channels % 2 == 0, return -EINVAL);
	snd_assert(runtime->channels >= 2 && runtime->channels <= MAX_CHANNELS,
			return -EINVAL);

	REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT, NUM_CH,
			runtime->channels / 2);

	return 0;
}

static inline int snd_stm_pcm_player_start(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_player_start(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* Un-reset PCM player */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_RST, SRSTP, RUNNING);

	/* Launch FDMA transfer */

	result = dma_xfer_list(pcm_player->fdma_channel,
			&pcm_player->fdma_params);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for player '%s'!\n",
				pcm_player->device->bus_id);
		return -EINVAL;
	}

	/* Launch PCM player */

	pcm_player->substream = substream;
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, MODE, PCM);

	/* Enable player interrupts */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_IT_EN_SET, NSAMPLE, SET);
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_IT_EN_SET, UNF, SET);

	/* Wake up & unmute DAC */

	if (pcm_player->conv) {
		snd_stm_conv_enable(pcm_player->conv);
		snd_stm_conv_unmute(pcm_player->conv);
	}

	return 0;
}

static inline int snd_stm_pcm_player_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_player_stop(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* Mute & shutdown DAC */

	if (pcm_player->conv) {
		snd_stm_conv_mute(pcm_player->conv);
		snd_stm_conv_disable(pcm_player->conv);
	}

	/* Disable interrupts */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_IT_EN_CLR, NSAMPLE, CLEAR);
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_IT_EN_CLR, UNF, CLEAR);

	/* Stop PCM player */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, MODE, OFF);
	pcm_player->substream = NULL;

	/* Stop FDMA transfer */

	dma_stop_channel(pcm_player->fdma_channel);

	/* Reset PCM player */
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_RST, SRSTP, RESET);

	return 0;
}

static inline int snd_stm_pcm_player_pause(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_player_pause(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* "Mute" player
	 * Documentation describes this mode in a wrong way - data is _not_
	 * consumed in the "mute" mode, so it is actually a "pause" mode */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, MODE, MUTE);

	return 0;
}

static inline int snd_stm_pcm_player_release(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_pcm_player *pcm_player =
		snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_player_release(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* "Unmute" player */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, MODE, PCM);

	return 0;
}

static int snd_stm_pcm_player_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printt("snd_stm_pcm_player_trigger(substream=0x%p,"
			" command=%d)\n", substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_pcm_player_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_pcm_player_stop(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return snd_stm_pcm_player_pause(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return snd_stm_pcm_player_release(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_pcm_player_pointer(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_pcm_player *pcm_player =
		snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue;
	snd_pcm_uframes_t pointer;

	snd_stm_printt("snd_stm_pcm_player_pointer(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	residue = get_dma_residue(pcm_player->fdma_channel);
	pointer = bytes_to_frames(runtime, runtime->dma_bytes - residue);

	snd_stm_printt("FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printt("... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

static struct snd_pcm_ops snd_stm_pcm_player_pcm_ops = {
	.open =      snd_stm_pcm_player_open,
	.close =     snd_stm_pcm_player_close,
	.mmap =      snd_stm_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_pcm_player_hw_params,
	.hw_free =   snd_stm_pcm_player_hw_free,
	.prepare =   snd_stm_pcm_player_prepare,
	.trigger =   snd_stm_pcm_player_trigger,
	.pointer =   snd_stm_pcm_player_pointer,
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_PCMOUT_%s (offset 0x%02x) =" \
				" 0x%08x\n", __stringify(r), \
				AUD_PCMOUT_##r, \
				REGISTER_PEEK(pcm_player->base, \
				AUD_PCMOUT_##r))

static void snd_stm_pcm_player_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_pcm_player *pcm_player = entry->private_data;

	snd_assert(pcm_player, return);
	snd_stm_magic_assert(pcm_player, return);

	DUMP_REGISTER(RST);
	DUMP_REGISTER(DATA);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(ITS_CLR);
	DUMP_REGISTER(IT_EN);
	DUMP_REGISTER(IT_EN_SET);
	DUMP_REGISTER(IT_EN_CLR);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(FMT);
}

static int snd_stm_pcm_player_register(struct snd_device *snd_device)
{
	int result;
	struct snd_stm_pcm_player *pcm_player = snd_device->device_data;

	snd_stm_printt("snd_stm_pcm_player_register(snd_device=0x%p)\n",
			snd_device);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* Set reset mode */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_RST, SRSTP, RESET);

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */

#if defined(CONFIG_CPU_SUBTYPE_STX7111)
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT, BACK_STALLING, DISABLED);
#endif
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, RND, NO_ROUNDING);

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&pcm_player->proc_entry,
			pcm_player->device->bus_id,
			snd_stm_pcm_player_dump_registers, pcm_player);

	/* Create ALSA controls */

	result = snd_stm_conv_add_route_ctl(pcm_player->device,
			snd_device->card, pcm_player->info->card_device);
	if (result < 0) {
		snd_stm_printe("Failed to add converter route control!\n");
		return result;
	}

	result = snd_stm_fsynth_add_adjustement_ctl(pcm_player->fsynth_device,
			pcm_player->fsynth_channel,
			snd_device->card, pcm_player->info->card_device);
	if (result < 0) {
		snd_stm_printe("Failed to add fsynth adjustment control!\n");
		return result;
	}

	return 0;
}

static int snd_stm_pcm_player_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_pcm_player *pcm_player = snd_device->device_data;

	snd_stm_printt("snd_stm_pcm_player_unregister(snd_device=0x%p)\n",
			snd_device);

	snd_stm_info_unregister(pcm_player->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_pcm_player_snd_device_ops = {
	.dev_register = snd_stm_pcm_player_register,
	.dev_disconnect = snd_stm_pcm_player_disconnect,
};



/*
 * Platform driver routines
 */

static int __init snd_stm_pcm_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_pcm_player *pcm_player;
	struct snd_card *card;
	int i;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	pcm_player = kzalloc(sizeof(*pcm_player), GFP_KERNEL);
	if (!pcm_player) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(pcm_player);
	pcm_player->info = pdev->dev.platform_data;
	snd_assert(pcm_player->info != NULL, return -EINVAL);
	pcm_player->device = &pdev->dev;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &pcm_player->mem_region,
			&pcm_player->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	pcm_player->fifo_phys_address = pcm_player->mem_region->start +
		AUD_PCMOUT_DATA;
	snd_printd("FIFO physical address: 0x%lx.\n",
			pcm_player->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &pcm_player->irq,
			snd_stm_pcm_player_irq_handler, pcm_player);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request(pdev, &pcm_player->fdma_channel);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	/* Get player capabilities */

	snd_printd("Player's name is '%s'\n", pcm_player->info->name);

	card = snd_stm_cards_get(pcm_player->info->card_id);
	snd_assert(card != NULL, return -EINVAL);
	snd_printd("Player will be a member of a card '%s' as a PCM device "
			"no. %d.\n", card->id, pcm_player->info->card_device);

	snd_assert(pcm_player->info->channels != NULL, return -EINVAL);
	snd_assert(pcm_player->info->channels_num > 0, return -EINVAL);
	pcm_player->channels_constraint.list = pcm_player->info->channels;
	pcm_player->channels_constraint.count = pcm_player->info->channels_num;
	pcm_player->channels_constraint.mask = 0;
	for (i = 0; i < pcm_player->info->channels_num; i++)
		snd_printd("Player capable of playing %u-channels PCM.\n",
				pcm_player->info->channels[i]);

	/* Get fsynth device */

	snd_assert(pcm_player->info->fsynth_bus_id != NULL, return -EINVAL);
	snd_printd("Player connected to %s's output %d.\n",
			pcm_player->info->fsynth_bus_id,
			pcm_player->info->fsynth_output);
	pcm_player->fsynth_device = snd_stm_find_device(NULL,
			pcm_player->info->fsynth_bus_id);
	snd_assert(pcm_player->fsynth_device != NULL, return -EINVAL);
	pcm_player->fsynth_channel = pcm_player->info->fsynth_output;

	/* Preallocate buffer */

	/* TODO */

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pcm_player,
			&snd_stm_pcm_player_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, pcm_player->info->card_device, 1, 0,
			&pcm_player->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	pcm_player->pcm->private_data = pcm_player;
	strcpy(pcm_player->pcm->name, pcm_player->info->name);

	snd_pcm_set_ops(pcm_player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_stm_pcm_player_pcm_ops);

	/* Done now */

	platform_set_drvdata(pdev, pcm_player);

	snd_printd("--- Probed successfully!\n");

	return 0;

error_pcm:
	snd_device_free(card, pcm_player);
error_device:
	snd_stm_fdma_release(pcm_player->fdma_channel);
error_fdma_request:
	snd_stm_irq_release(pcm_player->irq, pcm_player);
error_irq_request:
	snd_stm_memory_release(pcm_player->mem_region, pcm_player->base);
error_memory_request:
	snd_stm_magic_clear(pcm_player);
	kfree(pcm_player);
error_alloc:
	return result;
}

static int snd_stm_pcm_player_remove(struct platform_device *pdev)
{
	struct snd_stm_pcm_player *pcm_player = platform_get_drvdata(pdev);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	snd_stm_fdma_release(pcm_player->fdma_channel);
	snd_stm_irq_release(pcm_player->irq, pcm_player);
	snd_stm_memory_release(pcm_player->mem_region, pcm_player->base);

	snd_stm_magic_clear(pcm_player);
	kfree(pcm_player);

	return 0;
}

static struct platform_driver snd_stm_pcm_player_driver = {
	.driver = {
		.name = "pcm_player",
	},
	.probe = snd_stm_pcm_player_probe,
	.remove = snd_stm_pcm_player_remove,
};



/*
 * Initialization
 */

int __init snd_stm_pcm_player_init(void)
{
	return platform_driver_register(&snd_stm_pcm_player_driver);
}

void snd_stm_pcm_player_cleanup(void)
{
	platform_driver_unregister(&snd_stm_pcm_player_driver);
}
