/*
 *   STMicroelectronics System-on-Chips' PCM reader driver
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
#include <sound/control.h>
#include <sound/info.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 6 /* See common.h debug features */
#include "common.h"



/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT \
		(PLAT_STM_AUDIO__FORMAT_I2S | \
		PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS | \
		PLAT_STM_AUDIO__DATA_SIZE_24_BITS)

#define MAX_CHANNELS 10



/*
 * PCM reader instance definition
 */

struct snd_stm_pcm_reader {
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
	struct snd_pcm_hw_constraint_list channels_constraint;
	unsigned int channels_constraint_list[MAX_CHANNELS / 2];

	/* Board-specific settings */
	unsigned long format;
	unsigned int oversampling;

	/* Workaround for L/R swap problem (see further) */
	int lr_pol;

	/* Runtime data */
	void *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;
	struct stm_dma_params *fdma_params_list;

	snd_stm_magic_field;
};



/*
 * Capturing engine implementation
 */

static irqreturn_t snd_stm_pcm_reader_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct snd_stm_pcm_reader *pcm_reader = dev_id;
	unsigned int status;

	snd_stm_printt("snd_stm_pcm_reader_irq_handler(irq=%d, dev_id=0x%p)\n",
			irq, dev_id);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = REGISTER_PEEK(pcm_reader->base, AUD_PCMIN_ITS);
	REGISTER_POKE(pcm_reader->base, AUD_PCMIN_ITS_CLR, status);
	preempt_enable();

	/* Overflow? */
	if (unlikely(status & REGFIELD_VALUE(AUD_PCMIN_ITS, OVF, PENDING))) {
		snd_stm_printe("Overflow detected in PCM reader '%s'!\n",
				pcm_reader->bus_id);
		get_dma_residue(pcm_reader->fdma_channel);
		result = IRQ_HANDLED;
	}

	/* Period successfully played */
	if (likely(status & REGFIELD_VALUE(AUD_PCMIN_ITS, VSYNC, PENDING))) {
		snd_stm_printt("Vsync interrupt detected by '%s'!\n",
				pcm_reader->bus_id);
		/* TODO: Calculate sampling frequency */
		result = IRQ_HANDLED;
	}

	/* Some alien interrupt??? */
	snd_assert(result == IRQ_HANDLED);

	return result;
}

static void snd_stm_pcm_reader_callback_node_done(unsigned long param)
{
	struct snd_stm_pcm_reader *pcm_reader =
			(struct snd_stm_pcm_reader *)param;

	snd_stm_printt("snd_stm_pcm_reader_callback_node_done(param=0x%lx)\n",
			param);

	snd_assert(pcm_reader, return);
	snd_stm_magic_assert(pcm_reader, return);

	/* This function will be called after stopping FDMA as well
	 * and in this moment ALSA is already shut down... */
	if (pcm_reader->substream) {
		snd_stm_printt("Period elapsed ('%s')\n", pcm_reader->bus_id);
		snd_pcm_period_elapsed(pcm_reader->substream);
	}
}

static void snd_stm_pcm_reader_callback_node_error(unsigned long param)
{
	struct snd_stm_pcm_reader *pcm_reader =
			(struct snd_stm_pcm_reader *)param;

	snd_stm_printt("snd_stm_pcm_reader_callback_node_error(param=0x%lx)\n",
			param);

	snd_assert(pcm_reader, return);
	snd_stm_magic_assert(pcm_reader, return);

	snd_stm_printe("Error during FDMA transfer in reader '%s'!\n",
			pcm_reader->bus_id);
}

static struct snd_pcm_hardware snd_stm_pcm_reader_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER),
#if 0
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S16_LE),
#else
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),
#endif

	/* Keep in mind that we are working in slave mode, so sampling
	 * rate is determined by external components... */
	.rates		= (SNDRV_PCM_RATE_CONTINUOUS),
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 1,     /* TODO: I would say 2... */
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note: period_bytes_min defines minimum time between FDMA transfer
	 * interrupts... Keep it large enough not to kill the system... */

	.period_bytes_min = 4096, /* 1024 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

static int snd_stm_pcm_reader_open(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_pcm_reader_open(substream=0x%p)\n", substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* Set up constraints & pass hardware capabilities info to ALSA */

	result = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&pcm_reader->channels_constraint);
	if (result < 0) {
		snd_stm_printe("Can't set channels constraint!\n");
		return result;
	}

	/* Buffer size must be an integer multiple of a period size to use
	 * FDMA nodes as periods... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		snd_stm_printe("Can't set periods constraint!\n");
		return result;
	}

	runtime->hw = snd_stm_pcm_reader_hw;

	return 0;
}

static int snd_stm_pcm_reader_close(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_reader_close(substream=0x%p)\n",
			substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);

	return 0;
}

static int snd_stm_pcm_reader_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	int result;
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, period_bytes, periods;
	int i;

	snd_stm_printt("snd_stm_pcm_reader_hw_params(substream=0x%p,"
			" hw_params=0x%p)\n", substream, hw_params);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* Get the numbers... */

	buffer_bytes = params_buffer_bytes(hw_params);
	periods = params_periods(hw_params);
	period_bytes = buffer_bytes / periods;
	snd_assert(periods * period_bytes == buffer_bytes, return -EINVAL);

	/* Allocate buffer */

	pcm_reader->buffer = bigphysarea_alloc(buffer_bytes);
	/* TODO: move to BPA2, use pcm lib as fallback... */
	if (!pcm_reader->buffer) {
		snd_stm_printe("Can't allocate %d bytes buffer for '%s'!\n",
				buffer_bytes, pcm_reader->bus_id);
		return -ENOMEM;
	}

	runtime->dma_addr = virt_to_phys(pcm_reader->buffer);
	runtime->dma_area = ioremap_nocache(runtime->dma_addr, buffer_bytes);
	runtime->dma_bytes = buffer_bytes;

	snd_stm_printt("Allocated buffer for %s: buffer=0x%p, "
			"dma_addr=0x%08x, dma_area=0x%p, "
			"dma_bytes=%u\n", pcm_reader->bus_id,
			pcm_reader->buffer, runtime->dma_addr,
			runtime->dma_area, runtime->dma_bytes);

	/* Configure FDMA transfer (one node per period) */

	pcm_reader->fdma_params_list =
			kmalloc(sizeof(*pcm_reader->fdma_params_list) *
					periods, GFP_KERNEL);
	if (!pcm_reader->fdma_params_list) {
		/* TODO: move to BPA2 (see above) */
		snd_stm_printe("Can't allocate %d bytes for FDMA parameters "
				"list!\n", sizeof(*pcm_reader->fdma_params_list)
				* periods);
		bigphysarea_free(runtime->dma_area, runtime->dma_bytes);
		return -ENOMEM;
	}

	snd_stm_printt("Configuring FDMA transfer nodes:\n");

	for (i = 0; i < periods; i++) {
		dma_params_init(&pcm_reader->fdma_params_list[i], MODE_PACED,
				STM_DMA_LIST_CIRC);

		if (i > 0)
			dma_params_link(&pcm_reader->fdma_params_list[i - 1],
					(&pcm_reader->fdma_params_list[i]));

		dma_params_comp_cb(&pcm_reader->fdma_params_list[i],
				snd_stm_pcm_reader_callback_node_done,
				(unsigned long)pcm_reader,
				STM_DMA_CB_CONTEXT_ISR);

		dma_params_err_cb(&pcm_reader->fdma_params_list[i],
				snd_stm_pcm_reader_callback_node_error,
				(unsigned long)pcm_reader,
				STM_DMA_CB_CONTEXT_ISR);

		/* Get callback every time a node is completed */
		dma_params_interrupts(&pcm_reader->fdma_params_list[i],
				STM_DMA_NODE_COMP_INT);

		dma_params_DIM_0_x_1(&pcm_reader->fdma_params_list[i]);

		dma_params_req(&pcm_reader->fdma_params_list[i],
				pcm_reader->fdma_request);

		snd_stm_printt("- %d: %d bytes from 0x%08x\n", i, period_bytes,
				runtime->dma_addr + i * period_bytes);

		dma_params_addrs(&pcm_reader->fdma_params_list[i],
				pcm_reader->fifo_phys_address,
				runtime->dma_addr + i * period_bytes,
				period_bytes);
	}

	result = dma_compile_list(pcm_reader->fdma_channel,
				pcm_reader->fdma_params_list, GFP_KERNEL);
	if (result < 0) {
		snd_stm_printe("Can't compile FDMA parameters for"
				" reader '%s'!\n", pcm_reader->bus_id);
		bigphysarea_free(runtime->dma_area, runtime->dma_bytes);
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;
		kfree(pcm_reader->fdma_params_list);
		/* TODO: symmetrical to the above (BPA2 etc.) */
		return -EINVAL;
	}

	return 0;
}

static int snd_stm_pcm_reader_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_pcm_reader_hw_free(substream=0x%p)\n",
			substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* This callback may be called more than once... */

	if (pcm_reader->buffer) {
		/* Dispose buffer */

		snd_stm_printt("Freeing buffer for %s: buffer=0x%p, "
				"dma_addr=0x%08x, dma_area=0x%p, "
				"dma_bytes=%u\n", pcm_reader->bus_id,
				pcm_reader->buffer, runtime->dma_addr,
				runtime->dma_area, runtime->dma_bytes);

		iounmap(runtime->dma_area);
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;

		/* TODO: symmetrical to the above (BPA2 etc.) */
		bigphysarea_free(pcm_reader->buffer, runtime->dma_bytes);
		pcm_reader->buffer = NULL;

		/* Dispose FDMA parameters (whole list) */
		dma_params_free(pcm_reader->fdma_params_list);
		kfree(pcm_reader->fdma_params_list);
	}

	return 0;
}

static int snd_stm_pcm_reader_prepare(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_stm_printt("snd_stm_pcm_reader_prepare(substream=0x%p)\n",
			substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	/* Configure data memory format */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		REGFIELD_SET(pcm_reader->base, AUD_PCMIN_CTRL,
				MEM_FMT, 16_BITS_16_BITS);

		/* Workaround for a problem with L/R channels swap in case of
		 * 16/16 memory model: PCM puts left channel data in
		 * word's upper two bytes, but due to little endianess
		 * character of our memory it will be interpreted as right
		 * channel data...  The workaround is to invert L/R signal,
		 * however it is cheating, because in such case channel
		 * phases are shifted by one sample...
		 * (ask me for more details if above is not clear ;-)
		 * TODO this somehow better... */
		REGFIELD_POKE(pcm_reader->base, AUD_PCMIN_FMT, LR_POL,
				!pcm_reader->lr_pol);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "24/20/18/16 bits on the
		 * left than zeros"... ;-) */
		REGFIELD_SET(pcm_reader->base, AUD_PCMIN_CTRL,
				MEM_FMT, 16_BITS_0_BITS);

		/* In x/0 bits memory mode there is no problem with
		 * L/R polarity */
		REGFIELD_POKE(pcm_reader->base, AUD_PCMIN_FMT, LR_POL,
				pcm_reader->lr_pol);
			break;

	default:
		snd_assert(0, return -EINVAL);
		break;
	}

	/* Number of channels... */

	snd_assert(runtime->channels % 2 == 0, return -EINVAL);
	snd_assert(runtime->channels >= 2 && runtime->channels <= MAX_CHANNELS,
			return -EINVAL);

	/* Will be here in 7200 cut 2.0... */
#if 0
	REGFIELD_POKE(pcm_reader->base, AUD_PCMIN_FMT, NUM_CH,
			runtime->channels / 2);
#endif

	return 0;
}

static inline int snd_stm_pcm_reader_start(struct snd_pcm_substream *substream)
{
	int result;
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_reader_start(substream=0x%p)\n",
			substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);

	/* Un-reset PCM reader */

	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_RST, RSTP, RUNNING);

	/* Launch FDMA transfer */

	result = dma_xfer_list(pcm_reader->fdma_channel,
			pcm_reader->fdma_params_list);
	if (result != 0) {
		snd_stm_printe("Can't launch FDMA transfer for reader '%s'!\n",
				pcm_reader->bus_id);
		return -EINVAL;
	}

	/* Launch PCM reader */

	pcm_reader->substream = substream;
	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_CTRL, MODE, PCM);

	/* Enable reader interrupts */

	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_IT_EN_SET, VSYNC, SET);
	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_IT_EN_SET, OVF, SET);

	return 0;
}

static inline int snd_stm_pcm_reader_stop(struct snd_pcm_substream *substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);

	snd_stm_printt("snd_stm_pcm_reader_stop(substream=0x%p)\n",
			substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);

	/* Disable interrupts */

	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_IT_EN_CLR, VSYNC, CLEAR);
	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_IT_EN_CLR, OVF, CLEAR);

	/* Stop PCM reader */

	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_CTRL, MODE, OFF);
	pcm_reader->substream = NULL;

	/* Stop FDMA transfer */

	dma_stop_channel(pcm_reader->fdma_channel);

	/* Reset PCM reader */

	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_RST, RSTP, RESET);

	return 0;
}

static int snd_stm_pcm_reader_trigger(struct snd_pcm_substream *substream,
		int command)
{
	snd_stm_printt("snd_stm_pcm_reader_trigger(substream=0x%p,"
		       "command=%d)\n", substream, command);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_stm_pcm_reader_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stm_pcm_reader_stop(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t snd_stm_pcm_reader_pointer(struct snd_pcm_substream
		*substream)
{
	struct snd_stm_pcm_reader *pcm_reader =
			snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue;
	snd_pcm_uframes_t pointer;

	snd_stm_printt("snd_stm_pcm_reader_pointer(substream=0x%p)\n",
			substream);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);
	snd_assert(runtime, return -EINVAL);

	residue = get_dma_residue(pcm_reader->fdma_channel);
	pointer = bytes_to_frames(runtime, runtime->dma_bytes - residue);

	snd_stm_printt("FDMA residue value is %i and buffer size is %u"
			" bytes...\n", residue, runtime->dma_bytes);
	snd_stm_printt("... so HW pointer in frames is %lu (0x%lx)!\n",
			pointer, pointer);

	return pointer;
}

static struct snd_pcm_ops snd_stm_pcm_reader_pcm_ops = {
	.open =      snd_stm_pcm_reader_open,
	.close =     snd_stm_pcm_reader_close,
	.mmap =      snd_stm_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = snd_stm_pcm_reader_hw_params,
	.hw_free =   snd_stm_pcm_reader_hw_free,
	.prepare =   snd_stm_pcm_reader_prepare,
	.trigger =   snd_stm_pcm_reader_trigger,
	.pointer =   snd_stm_pcm_reader_pointer,
};



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_PCMIN_%s (offset 0x%02x) = 0x%08x\n", \
				__stringify(r), AUD_PCMIN_##r, \
				REGISTER_PEEK(pcm_reader->base, AUD_PCMIN_##r))

static void snd_stm_pcm_reader_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_pcm_reader *pcm_reader = entry->private_data;

	snd_assert(pcm_reader, return);
	snd_stm_magic_assert(pcm_reader, return);

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

static int snd_stm_pcm_reader_register(struct snd_device *snd_device)
{
	struct snd_stm_pcm_reader *pcm_reader = snd_device->device_data;

	snd_stm_printt("snd_stm_pcm_reader_register(snd_device=0x%p)\n",
			snd_device);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);

	/* Initialize hardware (format etc.) */

	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_RST, RSTP, RESET);

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */
	REGFIELD_SET(pcm_reader->base, AUD_PCMIN_CTRL, RND, NO_ROUNDING);

	if (pcm_reader->format & PLAT_STM_AUDIO__CUSTOM) {
		/* Custom format settings... Well, you asked for it! ;-) */
		REGISTER_POKE(pcm_reader->base, AUD_PCMIN_CTRL,
				pcm_reader->format & !PLAT_STM_AUDIO__CUSTOM);
	} else {
		/* Number of bits per subframe (which is one channel sample)
		 * on input. */

		switch (pcm_reader->format &
				PLAT_STM_AUDIO__OUTPUT_SUBFRAME_MASK) {
		case PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					NBIT, 32_BITS);
			break;
		case PLAT_STM_AUDIO__OUTPUT_SUBFRAME_16_BITS:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					NBIT, 16_BITS);
			break;
		default:
			snd_assert(0, return -EINVAL);
			break;
		}

		/* Number of meaningful bits in subframe -
		 * the rest are ignored */

		switch (pcm_reader->format & PLAT_STM_AUDIO__DATA_SIZE_MASK) {
		case PLAT_STM_AUDIO__DATA_SIZE_24_BITS:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					DATA_SIZE, 24_BITS);
			break;
		case PLAT_STM_AUDIO__DATA_SIZE_20_BITS:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					DATA_SIZE, 20_BITS);
			break;
		case PLAT_STM_AUDIO__DATA_SIZE_18_BITS:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					DATA_SIZE, 18_BITS);
			break;
		case PLAT_STM_AUDIO__DATA_SIZE_16_BITS:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					DATA_SIZE, 16_BITS);
			break;
		default:
			snd_assert(0, return -EINVAL);
			break;
		}

		/* Serial audio interface format -
		 * for detailed explanation see ie.
		 * http://www.cirrus.com/en/pubs/appNote/AN282REV1.pdf */

		REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
				ORDER, MSB_FIRST);
		REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
				SCLK_EDGE, RISING);
		switch (pcm_reader->format & PLAT_STM_AUDIO__FORMAT_MASK) {
		case PLAT_STM_AUDIO__FORMAT_I2S:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					ALIGN, LEFT);
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					PADDING, 1_CYCLE_DELAY);
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					LR_POL, LEFT_LOW);
			break;
		case PLAT_STM_AUDIO__FORMAT_LEFT_JUSTIFIED:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					ALIGN, LEFT);
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					PADDING, NO_DELAY);
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					LR_POL, LEFT_HIGH);
			break;
		case PLAT_STM_AUDIO__FORMAT_RIGHT_JUSTIFIED:
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					ALIGN, RIGHT);
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					PADDING, NO_DELAY);
			REGFIELD_SET(pcm_reader->base, AUD_PCMIN_FMT,
					LR_POL, LEFT_HIGH);
			break;
		default:
			snd_assert(0, return -EINVAL);
			break;
		}
	}

	/* Workaround for 16/16 memory format L/R channels swap (see above) */
	pcm_reader->lr_pol = REGFIELD_PEEK(pcm_reader->base,
			AUD_PCMIN_FMT, LR_POL);

	/* Registers view in ALSA's procfs */

	snd_stm_info_register(&pcm_reader->proc_entry, pcm_reader->bus_id,
			snd_stm_pcm_reader_dump_registers, pcm_reader);

	return 0;
}

static int snd_stm_pcm_reader_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_pcm_reader *pcm_reader = snd_device->device_data;

	snd_stm_printt("snd_stm_pcm_reader_unregister(snd_device=0x%p)\n",
			snd_device);

	snd_stm_info_unregister(pcm_reader->proc_entry);

	return 0;
}

static struct snd_device_ops snd_stm_pcm_reader_ops = {
	.dev_register = snd_stm_pcm_reader_register,
	.dev_disconnect = snd_stm_pcm_reader_disconnect,
};



/*
 * Driver initialization
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

static struct stm_dma_req_config snd_stm_pcm_reader_fdma_request_config = {
	.rw        = REQ_CONFIG_READ,
	.opcode    = REQ_CONFIG_OPCODE_4,
	.count     = 1,
	.increment = 0,
	.hold_off  = 0,
	/* .initiator value is defined in platform device resources */
};

static int __init snd_stm_pcm_reader_probe(struct platform_device *pdev)
{
	int result = 0;
	struct plat_audio_config *config = pdev->dev.platform_data;
	struct snd_stm_component *component;
	struct snd_stm_pcm_reader *pcm_reader;
	struct snd_card *card;
	int card_device;
	int *channels_list;
	int channels_list_len;
	const char *card_id;
	int i;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	component = snd_stm_components_get(pdev->dev.bus_id);
	snd_assert(component, return -EINVAL);

	pcm_reader = kzalloc(sizeof(*pcm_reader), GFP_KERNEL);
	if (!pcm_reader) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(pcm_reader);
	pcm_reader->bus_id = pdev->dev.bus_id;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &pcm_reader->mem_region,
			&pcm_reader->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}
	pcm_reader->fifo_phys_address = pcm_reader->mem_region->start +
		AUD_PCMIN_DATA;
	snd_printd("FIFO physical address: 0x%lx.\n",
			pcm_reader->fifo_phys_address);

	result = snd_stm_irq_request(pdev, &pcm_reader->irq,
			snd_stm_pcm_reader_irq_handler, pcm_reader);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	result = snd_stm_fdma_request(pdev, &pcm_reader->fdma_channel,
			&pcm_reader->fdma_request,
			&snd_stm_pcm_reader_fdma_request_config);
	if (result < 0) {
		snd_stm_printe("FDMA request failed!\n");
		goto error_fdma_request;
	}

	/* Get component caps */

	snd_printd("Reader's name is '%s'\n", component->short_name);

	result = snd_stm_cap_get_string(component, "card_id", &card_id);
	snd_assert(result == 0, return -EINVAL);
	card = snd_stm_cards_get(card_id);
	snd_assert(card != NULL, return -EINVAL);
	snd_printd("Reader will be a member of a card '%s'...\n", card_id);

	result = snd_stm_cap_get_number(component, "card_device",
			&card_device);
	snd_assert(result == 0, return -EINVAL);
	snd_printd("... as a PCM device no %d.\n", card_device);

	result = snd_stm_cap_get_list(component, "channels", &channels_list,
			&channels_list_len);
	snd_assert(result == 0, return -EINVAL);
	memcpy(pcm_reader->channels_constraint_list, channels_list,
			sizeof(*channels_list) * channels_list_len);
	pcm_reader->channels_constraint.list =
			pcm_reader->channels_constraint_list;
	pcm_reader->channels_constraint.count =
		(unsigned int)channels_list_len;
	pcm_reader->channels_constraint.mask = 0;
	for (i = 0; i < channels_list_len; i++)
		snd_printd("Reader capable of capturing %d-channels PCM.\n",
				channels_list[i]);

	/* Board-specific configuration */

	if (config) {
		pcm_reader->format = config->pcm_format;
		snd_printd("Using board specific PCM format"
				" (%s%s%s, 0x%08lx).\n",
				FORMAT_STRING(pcm_reader->format),
				DATA_SIZE_STRING(pcm_reader->format),
				OUTPUT_SUBFRAME_STRING(pcm_reader->format),
				pcm_reader->format);
	} else {
		pcm_reader->format = DEFAULT_FORMAT;
		snd_printd("Using default PCM format (%s%s%s).\n",
				FORMAT_STRING(pcm_reader->format),
				DATA_SIZE_STRING(pcm_reader->format),
				OUTPUT_SUBFRAME_STRING(pcm_reader->format));
	}

	/* Preallocate buffer */

	/* TODO */

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pcm_reader,
			&snd_stm_pcm_reader_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, card_device, 0, 1, &pcm_reader->pcm);
	if (result < 0) {
		snd_stm_printe("ALSA PCM instance creation failed!\n");
		goto error_pcm;
	}
	pcm_reader->pcm->private_data = pcm_reader;
	strcpy(pcm_reader->pcm->name, component->short_name);

	snd_pcm_set_ops(pcm_reader->pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_stm_pcm_reader_pcm_ops);

	/* Done now */

	platform_set_drvdata(pdev, pcm_reader);

	snd_printd("--- Probed successfully!\n");

	return result;

error_pcm:
	snd_device_free(card, pcm_reader);
error_device:
	snd_stm_fdma_release(pcm_reader->fdma_channel,
			pcm_reader->fdma_request);
error_fdma_request:
	snd_stm_irq_release(pcm_reader->irq, pcm_reader);
error_irq_request:
	snd_stm_memory_release(pcm_reader->mem_region, pcm_reader->base);
error_memory_request:
	snd_stm_magic_clear(pcm_reader);
	kfree(pcm_reader);
error_alloc:
	return result;
}

static int snd_stm_pcm_reader_remove(struct platform_device *pdev)
{
	struct snd_stm_pcm_reader *pcm_reader = platform_get_drvdata(pdev);

	snd_assert(pcm_reader, return -EINVAL);
	snd_stm_magic_assert(pcm_reader, return -EINVAL);

	snd_stm_fdma_release(pcm_reader->fdma_channel,
			pcm_reader->fdma_request);
	snd_stm_irq_release(pcm_reader->irq, pcm_reader);
	snd_stm_memory_release(pcm_reader->mem_region, pcm_reader->base);

	snd_stm_magic_clear(pcm_reader);
	kfree(pcm_reader);

	return 0;
}

static struct platform_driver snd_stm_pcm_reader_driver = {
	.driver = {
		.name = "pcm_reader",
	},
	.probe = snd_stm_pcm_reader_probe,
	.remove = snd_stm_pcm_reader_remove,
};

/*
 * Initialization
 */

int __init snd_stm_pcm_reader_init(void)
{
	return platform_driver_register(&snd_stm_pcm_reader_driver);
}

void snd_stm_pcm_reader_cleanup(void)
{
	platform_driver_unregister(&snd_stm_pcm_reader_driver);
}
