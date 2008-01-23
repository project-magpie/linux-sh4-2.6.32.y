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

#undef TRACE /* See common.h debug features */
#define MAGIC 5 /* See common.h debug features */
#include "common.h"



/*
 * Some hardware-related definitions
 */

#define INIT_SAMPLING_RATE 32000

#define DEFAULT_OVERSAMPLING 256
#define DEFAULT_FORMAT \
		(PLAT_STM_AUDIO__FORMAT_I2S | \
		PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS | \
		PLAT_STM_AUDIO__DATA_SIZE_24_BITS)

/* The sample count field (NSAMPLES in CTRL register) is 19 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 19) - 1)

#define MAX_CHANNELS 10



/*
 * PCM player instance definition
 */

struct snd_stm_pcm_player {
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
	struct device *dac;
	struct snd_pcm_hw_constraint_list channels_constraint;
	unsigned int channels_constraint_list[MAX_CHANNELS / 2];

	/* Board-specific settings */
	unsigned long format;
	unsigned int oversampling;

	/* Value of SCLK_EDGE bit in AUD_PCMOUT_FMT register that
	 * actually means "data clocking on the falling edge" -
	 * STx7100 and _some_ cuts of STx7109 have this value
	 * inverted than datasheets claim... (specs say 1) */
	int sclk_edge_falling;

	/* Workaround for L/R swap problem (see further) */
	int lr_pol;

	/* Runtime data */
	void *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	struct stm_dma_params fdma_params;

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
				pcm_player->bus_id);
		result = IRQ_HANDLED;
	}

	/* Period successfully played */
	if (likely(status & REGFIELD_VALUE(AUD_PCMOUT_ITS, NSAMPLE, PENDING)))
		do {
			snd_assert(pcm_player->substream, break);

			snd_stm_printt("Period elapsed ('%s')\n",
					pcm_player->bus_id);
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
				SNDRV_PCM_RATE_96000 |
				SNDRV_PCM_RATE_192000),
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 1,     /* TODO: I would say 2... */
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

static int snd_stm_pcm_player_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes;

	snd_stm_printt("snd_stm_pcm_player_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	pcm_player->buffer = bigphysarea_alloc(buffer_bytes);
	/* TODO: move to BPA2, use pcm lib as fallback... */
	if (!pcm_player->buffer) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
				buffer_bytes, pcm_player->bus_id);
		return -ENOMEM;
	}

	runtime->dma_addr = virt_to_phys(pcm_player->buffer);
	runtime->dma_area = ioremap_nocache(runtime->dma_addr, buffer_bytes);
	runtime->dma_bytes = buffer_bytes;

	snd_stm_printt("Allocated buffer for %s: buffer=0x%p, "
			"dma_addr=0x%08x, dma_area=0x%p, "
			"dma_bytes=%u\n", pcm_player->bus_id,
			pcm_player->buffer, runtime->dma_addr,
			runtime->dma_area, runtime->dma_bytes);

	/* Configure FDMA transfer */

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
				" '%s'!\n", pcm_player->bus_id);
		return -EINVAL;
	}

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
				"dma_bytes=%u\n", pcm_player->bus_id,
				pcm_player->buffer, runtime->dma_addr,
				runtime->dma_area, runtime->dma_bytes);

		iounmap(runtime->dma_area);
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;

		/* TODO: symmetrical to the above (BPA2 etc.) */
		bigphysarea_free(pcm_player->buffer, runtime->dma_bytes);
		pcm_player->buffer = NULL;

		/* Dispose FDMA parameters */

		dma_params_free(&pcm_player->fdma_params);
	}

	return 0;
}

static int snd_stm_pcm_player_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_player *pcm_player =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bits_in_output_frame;

	snd_stm_printt("snd_stm_pcm_player_prepare(substream=0x%p)\n",
			substream);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);
	snd_assert(runtime, return -EINVAL);
	snd_assert(runtime->period_size * runtime->channels <
			MAX_SAMPLES_PER_PERIOD, return -EINVAL);

	/* Configure SPDIF synchronisation */

	/* TODO */

	/* Set up frequency synthesizer */

	snd_stm_fsynth_set_frequency(pcm_player->fsynth,
			pcm_player->fsynth_channel,
			runtime->rate * pcm_player->oversampling);

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

	switch (pcm_player->format & PLAT_STM_AUDIO__OUTPUT_SUBFRAME_MASK) {
	case PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS:
		bits_in_output_frame = 64;
		break;
	case PLAT_STM_AUDIO__OUTPUT_SUBFRAME_16_BITS:
		bits_in_output_frame = 32;
		break;
	default:
		snd_assert(0, return -EINVAL);
		break;
	}

	REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_CTRL, CLK_DIV,
			pcm_player->oversampling / (2 * bits_in_output_frame));

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
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT, LR_POL,
				!pcm_player->lr_pol);

		/* One word if fifo is two samples (two channels...) */

		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_CTRL, NSAMPLE,
				runtime->period_size * runtime->channels / 2);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "24/20/18/16 bits on the
		 * left than zeros"... ;-) */
		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL,
				MEM_FMT, 16_BITS_0_BITS);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT, LR_POL,
				pcm_player->lr_pol);

		/* One word of data is one sample, so period size
		 * times channels */

		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_CTRL, NSAMPLE,
				runtime->period_size * runtime->channels);
		break;

	default:
		snd_assert(0, return -EINVAL);
		break;
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
				pcm_player->bus_id);
		return -EINVAL;
	}

	/* Launch PCM player */

	pcm_player->substream = substream;
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, MODE, PCM);

	/* Enable player interrupts */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_IT_EN_SET, NSAMPLE, SET);
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_IT_EN_SET, UNF, SET);

	/* Wake up & unmute DAC */

	if (pcm_player->dac) {
		snd_stm_dac_wake_up(pcm_player->dac);
		snd_stm_dac_unmute(pcm_player->dac);
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

	if (pcm_player->dac) {
		snd_stm_dac_mute(pcm_player->dac);
		snd_stm_dac_shut_down(pcm_player->dac);
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
	struct snd_stm_pcm_player *pcm_player = snd_device->device_data;

	snd_stm_printt("snd_stm_pcm_player_register(snd_device=0x%p)\n",
			snd_device);

	snd_assert(pcm_player, return -EINVAL);
	snd_stm_magic_assert(pcm_player, return -EINVAL);

	/* Set a default clock frequency running for each device.
	 * Not doing this can lead to clocks not starting correctly later,
	 * for reasons that cannot be explained at this time. */
	/* TODO: Check it, maybe obsolete now */
	snd_stm_fsynth_set_frequency(pcm_player->fsynth,
			pcm_player->fsynth_channel,
			INIT_SAMPLING_RATE * pcm_player->oversampling);

	/* Initialize hardware (format etc.) */

	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_RST, SRSTP, RESET);

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */
	REGFIELD_SET(pcm_player->base, AUD_PCMOUT_CTRL, RND, NO_ROUNDING);

	if (pcm_player->format & PLAT_STM_AUDIO__CUSTOM) {
		/* Custom format settings... Well, you asked for it! ;-) */
		REGISTER_POKE(pcm_player->base, AUD_PCMOUT_CTRL,
				pcm_player->format & !PLAT_STM_AUDIO__CUSTOM);
	} else {
		/* Number of bits per subframe (which is one channel sample)
		 * on output - it determines serial clock frequency, which is
		 * 64 times sampling rate for 32 bits subframe (2 channels 32
		 * bits each means 64 bits per frame) and 32 times sampling
		 * rate for 16 bits subframe
		 * (you know why now, don't you? :-) */

		switch (pcm_player->format &
				PLAT_STM_AUDIO__OUTPUT_SUBFRAME_MASK) {
		case PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					NBIT, 32_BITS);
			break;
		case PLAT_STM_AUDIO__OUTPUT_SUBFRAME_16_BITS:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					NBIT, 16_BITS);
			break;
		default:
			snd_assert(0, return -EINVAL);
			break;
		}

		/* Datasheet says: "The recommended configuration is to set
		 * the PCM player fifo threshold that triggers the FDMA
		 * request to 40 bytes (at least 80 bytes available) and
		 * to configured the FDMA to perform a 80 bytes store
		 * operation when servicing a dma request." My understanding
		 * of "FIFO cell" is "4 bytes" ;-), so the value should be 20.
		 * Surprisingly experiments suggest using something
		 * like 10... */
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT,
				DMA_REQ_TRIG_LMT, 10);

		/* Number of meaningful bits in subframe -
		 * the rest are just zeros */

		switch (pcm_player->format & PLAT_STM_AUDIO__DATA_SIZE_MASK) {
		case PLAT_STM_AUDIO__DATA_SIZE_24_BITS:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					DATA_SIZE, 24_BITS);
			break;
		case PLAT_STM_AUDIO__DATA_SIZE_20_BITS:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					DATA_SIZE, 20_BITS);
			break;
		case PLAT_STM_AUDIO__DATA_SIZE_18_BITS:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					DATA_SIZE, 18_BITS);
			break;
		case PLAT_STM_AUDIO__DATA_SIZE_16_BITS:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					DATA_SIZE, 16_BITS);
			break;
		default:
			snd_assert(0, return -EINVAL);
			break;
		}

		/* Serial audio interface format - for detailed explanation
		 * see ie.:
		 * http://www.cirrus.com/en/pubs/appNote/AN282REV1.pdf */

		REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
				ORDER, MSB_FIRST);
		REGFIELD_POKE(pcm_player->base, AUD_PCMOUT_FMT,
				SCLK_EDGE, pcm_player->sclk_edge_falling);
		switch (pcm_player->format & PLAT_STM_AUDIO__FORMAT_MASK) {
		case PLAT_STM_AUDIO__FORMAT_I2S:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					ALIGN, LEFT);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					PADDING, 1_CYCLE_DELAY);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					LR_POL, LEFT_LOW);
			break;
		case PLAT_STM_AUDIO__FORMAT_LEFT_JUSTIFIED:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					ALIGN, LEFT);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					PADDING, NO_DELAY);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					LR_POL, LEFT_HIGH);
			break;
		case PLAT_STM_AUDIO__FORMAT_RIGHT_JUSTIFIED:
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					ALIGN, RIGHT);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					PADDING, NO_DELAY);
			REGFIELD_SET(pcm_player->base, AUD_PCMOUT_FMT,
					LR_POL, LEFT_HIGH);
			break;
		default:
			snd_assert(0, return -EINVAL);
			break;
		}
	}

	/* Workaround for 16/16 memory format L/R channels swap (see above) */
	pcm_player->lr_pol = REGFIELD_PEEK(pcm_player->base, AUD_PCMOUT_FMT,
			LR_POL);

	/* This combination is forbidden - please use 384 * Fs oversampling
	 * frequency instead */
	snd_assert(!(pcm_player->oversampling == 192 && (pcm_player->format &
			PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS)),
			return -EINVAL);

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&pcm_player->proc_entry, pcm_player->bus_id,
			snd_stm_pcm_player_dump_registers, pcm_player);

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

static struct snd_device_ops snd_stm_pcm_player_ops = {
	.dev_register = snd_stm_pcm_player_register,
	.dev_disconnect = snd_stm_pcm_player_disconnect,
};



/*
 * Platform driver routines
 */

#define FORMAT_STRING(f) \
	((f & PLAT_STM_AUDIO__CUSTOM) == \
		PLAT_STM_AUDIO__CUSTOM ? "custom" : \
	(f & PLAT_STM_AUDIO__FORMAT_MASK) == \
		PLAT_STM_AUDIO__FORMAT_I2S ? "I2S, " : \
	(f & PLAT_STM_AUDIO__FORMAT_MASK) == \
		PLAT_STM_AUDIO__FORMAT_LEFT_JUSTIFIED ? \
		"left justified, " : \
	(f & PLAT_STM_AUDIO__FORMAT_MASK) == \
		PLAT_STM_AUDIO__FORMAT_RIGHT_JUSTIFIED ? \
		"right justified, " : \
	"")

#define DATA_SIZE_STRING(f) \
	((f & PLAT_STM_AUDIO__CUSTOM) == \
		PLAT_STM_AUDIO__CUSTOM ? "" : \
	(f & PLAT_STM_AUDIO__DATA_SIZE_MASK) == \
		PLAT_STM_AUDIO__DATA_SIZE_24_BITS ? \
		"24 bits data, " : \
	(f & PLAT_STM_AUDIO__DATA_SIZE_MASK) == \
		PLAT_STM_AUDIO__DATA_SIZE_20_BITS ? \
		"20 bits data, " : \
	(f & PLAT_STM_AUDIO__DATA_SIZE_MASK) == \
		PLAT_STM_AUDIO__DATA_SIZE_18_BITS ? \
		"18 bits data, " : \
	(f & PLAT_STM_AUDIO__DATA_SIZE_MASK) == \
		PLAT_STM_AUDIO__DATA_SIZE_16_BITS ? \
		"16 bits data, " : \
	"")

#define OUTPUT_SUBFRAME_STRING(f) \
	((f & PLAT_STM_AUDIO__CUSTOM) == \
		PLAT_STM_AUDIO__CUSTOM ? "" : \
	(f & PLAT_STM_AUDIO__OUTPUT_SUBFRAME_MASK) == \
		PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS ? \
		"32 bits output subframe" : \
	(f & PLAT_STM_AUDIO__OUTPUT_SUBFRAME_MASK) == \
		PLAT_STM_AUDIO__OUTPUT_SUBFRAME_16_BITS ? \
		"16 bits output subframe" : \
	"")

static struct stm_dma_req_config snd_stm_pcm_player_fdma_request_config = {
	.rw        = REQ_CONFIG_WRITE,
	.opcode    = REQ_CONFIG_OPCODE_4,
	.count     = 1,
	.increment = 0,
	.hold_off  = 0,
	/* .initiator value is defined in platform device resources */
};

static int __init snd_stm_pcm_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct plat_audio_config *config = pdev->dev.platform_data;
	struct snd_stm_component *component;
	struct snd_stm_pcm_player *pcm_player;
	struct snd_card *card;
	int card_device;
	int *channels_list;
	int channels_list_len;
	const char *card_id;
	const char *fsynth_bus_id;
	const char *dac_bus_id;
	int i;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	component = snd_stm_components_get(pdev->dev.bus_id);
	snd_assert(component, return -EINVAL);

	pcm_player = kzalloc(sizeof(*pcm_player), GFP_KERNEL);
	if (!pcm_player) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(pcm_player);
	pcm_player->bus_id = pdev->dev.bus_id;

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

	result = snd_stm_fdma_request(pdev, &pcm_player->fdma_channel,
			&pcm_player->fdma_request,
			&snd_stm_pcm_player_fdma_request_config);
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

	result = snd_stm_cap_get_list(component, "channels", &channels_list,
			&channels_list_len);
	snd_assert(result == 0, return -EINVAL);
	memcpy(pcm_player->channels_constraint_list, channels_list,
			sizeof(*channels_list) * channels_list_len);
	pcm_player->channels_constraint.list =
		pcm_player->channels_constraint_list;
	pcm_player->channels_constraint.count =
		(unsigned int)channels_list_len;
	pcm_player->channels_constraint.mask = 0;
	for (i = 0; i < channels_list_len; i++)
		snd_printd("Player capable of playing %d-channels PCM.\n",
				channels_list[i]);

	result = snd_stm_cap_get_string(component, "fsynth_bus_id",
			&fsynth_bus_id);
	snd_assert(result == 0, return -EINVAL);
	pcm_player->fsynth = snd_stm_device_get(fsynth_bus_id);
	snd_assert(pcm_player->fsynth, return -EINVAL);
	result = snd_stm_cap_get_number(component, "fsynth_channel",
			&pcm_player->fsynth_channel);
	snd_assert(result == 0, return -EINVAL);
	snd_printd("Player clocked by channel %d of synthesizer %s.\n",
			pcm_player->fsynth_channel, fsynth_bus_id);

	if (snd_stm_cap_get_string(component, "dac_bus_id", &dac_bus_id) == 0) {
		pcm_player->dac = snd_stm_device_get(dac_bus_id);
		snd_assert(pcm_player->dac, return -EINVAL);
		snd_printd("Player connected to DAC %s.\n", dac_bus_id);
	} else {
		pcm_player->dac = NULL;
	}

	if (snd_stm_cap_get_number(component, "sclk_edge_falling",
				&pcm_player->sclk_edge_falling) < 0)
		pcm_player->sclk_edge_falling =
			AUD_PCMOUT_FMT__SCLK_EDGE__VALUE__FALLING;

	snd_printd("Player's SCLK_EDGE == %d means falling edge...\n",
			pcm_player->sclk_edge_falling);

	/* Board-specific configuration */

	if (pcm_player->dac) {
		/* If player is connected to an internal DAC just
		 * ask it about required format instead of looking
		 * for user-specified one */
		result = snd_stm_dac_get_config(pcm_player->dac,
				&pcm_player->format,
				&pcm_player->oversampling);
		snd_assert(result == 0, return -EINVAL);
		snd_printd("Using DAC-defined PCM format (%s%s%s)"
				" and oversampling (%u).\n",
				FORMAT_STRING(pcm_player->format),
				DATA_SIZE_STRING(pcm_player->format),
				OUTPUT_SUBFRAME_STRING(pcm_player->format),
				pcm_player->oversampling);
	} else if (config) {
		pcm_player->format = config->pcm_format;
		pcm_player->oversampling = config->oversampling;
		snd_printd("Using board specific PCM format (%s%s%s, 0x%08lx)"
				" and oversampling (%u).\n",
				FORMAT_STRING(pcm_player->format),
				DATA_SIZE_STRING(pcm_player->format),
				OUTPUT_SUBFRAME_STRING(pcm_player->format),
				pcm_player->format,
				pcm_player->oversampling);
	} else {
		pcm_player->format = DEFAULT_FORMAT;
		pcm_player->oversampling = DEFAULT_OVERSAMPLING;
		snd_printd("Using default PCM format (%s%s%s)"
				" and oversampling (%u).\n",
				FORMAT_STRING(pcm_player->format),
				DATA_SIZE_STRING(pcm_player->format),
				OUTPUT_SUBFRAME_STRING(pcm_player->format),
				pcm_player->oversampling);
	}
	/* Allowed oversampling values */
	snd_assert(pcm_player->oversampling == 128 ||
			pcm_player->oversampling == 192 ||
			pcm_player->oversampling == 256 ||
			pcm_player->oversampling == 384 ||
			pcm_player->oversampling == 512 ||
			pcm_player->oversampling == 768,
			return -EINVAL);

	/* Preallocate buffer */

	/* TODO */

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pcm_player,
			&snd_stm_pcm_player_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, card_device, 1, 0, &pcm_player->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	pcm_player->pcm->private_data = pcm_player;
	strcpy(pcm_player->pcm->name, component->short_name);

	snd_pcm_set_ops(pcm_player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_stm_pcm_player_pcm_ops);

	/* Create ALSA controls */

	result = snd_stm_fsynth_add_adjustement_ctl(pcm_player->fsynth,
			pcm_player->fsynth_channel, card, card_device);
	if (result < 0) {
		snd_stm_printe("Failed to add ALSA control!\n");
		goto error_controls;
	}

	/* Done now */

	platform_set_drvdata(pdev, pcm_player);

	snd_printd("--- Probed successfully!\n");

	return result;

error_controls:
error_pcm:
	snd_device_free(card, pcm_player);
error_device:
	snd_stm_fdma_release(pcm_player->fdma_channel,
			pcm_player->fdma_request);
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

	snd_stm_fdma_release(pcm_player->fdma_channel,
			pcm_player->fdma_request);
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
