/*
 *  STb710x SPDIF player setup
 *  Copyright (c) 2005 STMicroelectronics Limited
 *
 *  *  Authors:  Mark Glaisher <Mark.Glaisher@st.com>
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

#include <asm/dma.h>
#include <linux/stm/stm-dma.h>
#if (STM_USE_BIGPHYS_AREA == 0)
#define SPDIF_MAX_FRAMES	((128*1024)/8)  /* <128k, max slab allocation */
#else
#define SPDIF_MAX_FRAMES	48000           /* 1s @ 48KHz */
#endif

/*
 * Default HW template for SPDIF player.
 */
static snd_pcm_hardware_t stb7100_spdif_hw =
{
	.info =		(SNDRV_PCM_INFO_INTERLEAVED |
			 SNDRV_PCM_INFO_PAUSE),

	.formats =	(SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE),

	.rates =	(SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000),

	.rate_min	  = 32000,
	.rate_max	  = 48000,
	.channels_min	  = 2,
	.channels_max	  = 2,
	.buffer_bytes_max = FRAMES_TO_BYTES(SPDIF_MAX_FRAMES,2),
	.period_bytes_min = FRAMES_TO_BYTES(1,2),
	.period_bytes_max = FRAMES_TO_BYTES(SPDIF_MAX_FRAMES,2),
	.periods_min	  = 1,
	.periods_max	  = SPDIF_MAX_FRAMES
};


static inline void reset_spdif_on(pcm_hw_t  *chip)
{
	writel(1,chip->pcm_player);
}

static inline void reset_spdif_off(pcm_hw_t  *chip)
{
	writel(0,chip->pcm_player);
}

static void stb7100_iec61937_deferred_unpause(pcm_hw_t * chip)
{
	spin_lock(&chip->lock);
	writel(SPDIF_INT_STATUS_EODBURST ,chip->pcm_player +STM_PCMP_IRQ_EN_CLR);
	writel((chip->pcmplayer_control|chip->spdif_player_mode),
		chip->pcm_player+STM_PCMP_CONTROL);
	spin_unlock(&chip->lock);
	chip->iec61937.pause_count=0;
	/*throw an xrun to flush the buffer of invalidated data bursts and re-align the
	 * next burst with a block boundary*/
	snd_pcm_kernel_ioctl(chip->current_substream, SNDRV_PCM_IOCTL_XRUN, NULL);
}

static irqreturn_t stb7100_spdif_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long int_status;
	pcm_hw_t *stb7100 = dev_id;
	irqreturn_t status =IRQ_NONE;;
	/* Read and clear interrupt status */
	spin_lock(&stb7100->lock);
	int_status = readl(stb7100->pcm_player + STM_PCMP_IRQ_STATUS);
	writel(int_status,stb7100->pcm_player + STM_PCMP_ITS_CLR);
	spin_unlock(&stb7100->lock);

	if((int_status & SPDIF_INT_STATUS_ALLREAD) == SPDIF_INT_STATUS_ALLREAD ){
		snd_pcm_period_elapsed(stb7100->current_substream);
		status =  IRQ_HANDLED;
	}
	if((int_status & SPDIF_INT_STATUS_UNF)==SPDIF_INT_STATUS_UNF) {
		printk("%s SPDIF PLayer FIFO Underflow detected\n",__FUNCTION__);
		status = IRQ_HANDLED;
	}
	if((int_status & SPDIF_INT_STATUS_EOLATENCY) == SPDIF_INT_STATUS_EOLATENCY){
		status =  IRQ_HANDLED;
	}
	if((int_status & SPDIF_INT_STATUS_EODBURST) == SPDIF_INT_STATUS_EODBURST){
		stb7100->iec61937.pause_count = ((stb7100->iec61937.pause_count+1)
						%stb7100->iec61937.frame_size);
		/*we have to wait until we have completed an entire iec91637 burst length
		 * before we stop emitting bursts, so we have to wait for mod(iec61937_frame_size)*/
		if((stb7100->iec61937.pause_count==0) && (stb7100->iec61937.unpause_flag==1)){
			stb7100->iec61937.unpause_flag=0;
			stb7100_iec61937_deferred_unpause(stb7100);
		}
		status =  IRQ_HANDLED;
	}
	return status;
}


static inline void stb7100_spdif_pause_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);

	if (chip->iec_encoding_mode != ENCODING_IEC60958) {
		/*here we turn on iec61937 pause bursts, modulated by the
		 * frames of latency value*/
		chip->iec61937.pause_count = chip->iec60958_output_count;
		chip->iec60958_output_count=0;
		writel(chip->irq_mask | SPDIF_INT_STATUS_EODBURST,chip->pcm_player+STM_PCMP_IRQ_EN_SET);
    	}
	else{
		 /*the SPDIF IP will always at least complete the next 192 frame
		  * burst - this gives an audible delay between an analogue and digital
		  * pause, so here we want to flush out that buffer, the only way to do this
		  * is throw a reset.*/
		 reset_spdif_on(chip);
		 reset_spdif_off(chip);
	}
	spin_lock(&chip->lock);
	writel((chip->pcmplayer_control|chip->iec61937.pause_mode),
		chip->pcm_player+STM_PCMP_CONTROL);
	spin_unlock(&chip->lock);

}


static inline void stb7100_spdif_unpause_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	/*we are doing pause burst, must count %frame_size*/
	if(chip->iec_encoding_mode != ENCODING_IEC60958){
		/*first we need to check if pause burst are enable,
		 * otherwise we will deadlock here
		 * */
		if(readl(chip->pcm_player+STM_PCMP_IRQ_ENABLE) & ENABLE_INT_EODBURST){
			chip->iec61937.unpause_flag=1;
			return;
		}
	}

	spin_lock(&chip->lock);
	writel((chip->pcmplayer_control|chip->spdif_player_mode),
		chip->pcm_player+STM_PCMP_CONTROL);
	spin_unlock(&chip->lock);
}


static inline void stb7100_spdif_start_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);

	dma_xfer_list(chip->fdma_channel,&chip->dmap);
	spin_lock(&chip->lock);
	reset_spdif_off(chip);
	writel((chip->pcmplayer_control|chip->spdif_player_mode),chip->pcm_player + STM_PCMP_CONTROL);
	spin_unlock(&chip->lock);
}


static inline void stb7100_spdif_stop_playback(snd_pcm_substream_t *substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	spin_lock(&chip->lock);
	writel(0,chip->pcm_player+STM_PCMP_CONTROL);
	reset_spdif_on(chip);
	spin_unlock(&chip->lock);
	dma_stop_channel(chip->fdma_channel);
	dma_params_free(&chip->dmap);
}


static void stb7100_spdif_set_iec_mode(snd_pcm_substream_t *substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned int decode_lat=0;
	unsigned int rate =substream->runtime->rate;

	switch(	chip->iec_encoding_mode){

		case ENCODING_IEC61937_AC3:
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep =3;
			chip->iec61937.latency=256;
			chip->iec61937.frame_size=1536;
			break;
		case ENCODING_IEC61937_DTS_1:
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep = 3;
			chip->iec61937.latency=768;
			chip->iec61937.frame_size=512;
			break;
		case ENCODING_IEC61937_DTS_2:
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep = 3;
			chip->iec61937.latency=1280;
			chip->iec61937.frame_size=1024;
			break;
		case ENCODING_IEC61937_DTS_3:
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep = 3;
			chip->iec61937.latency=2304;
			chip->iec61937.frame_size=2048;
			break;
			{
		case ENCODING_IEC61937_MPEG_384_FRAME:
			chip->iec61937.frame_size = 384;
			goto generic_mpeg_encoding;
		case ENCODING_IEC61937_MPEG_1152_FRAME:
			chip->iec61937.frame_size = 1152;
			goto generic_mpeg_encoding;
		case ENCODING_IEC61937_MPEG_1024_FRAME:
			chip->iec61937.frame_size = 1024;
			goto generic_mpeg_encoding;
		case ENCODING_IEC61937_MPEG_2304_FRAME:
			chip->iec61937.frame_size = 2304;
			goto generic_mpeg_encoding;
		case ENCODING_IEC61937_MPEG_768_FRAME:
			chip->iec61937.frame_size = 768;
			/*fallthrough*/
generic_mpeg_encoding:
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep = 32;
			switch(rate){
				case 32000:
					decode_lat = MPEG_DECODE_LAT_32KHZ;
					break;
				case 44100:
					decode_lat = MPEG_DECODE_LAT_441KHZ;
					break;
				case 48000:
					decode_lat = MPEG_DECODE_LAT_48KHZ;
					break;
				default:
					printk("%s Unsupported Sample Freq\n",__FUNCTION__);
					break;
			}
			chip->iec61937.latency=TIME_TO_FRAMES(rate,decode_lat);
			break;
			}
		case ENCODING_IEC61937_MPEG_2304_FRAME_LSF:
			chip->iec61937.frame_size=2304;
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep = 64;
			chip->iec61937.latency=TIME_TO_FRAMES(rate,decode_lat);
			break;
		case ENCODING_IEC61937_MPEG_768_FRAME_LSF:
			chip->iec61937.frame_size=768;
			chip->iec61937.pause_mode=SPDIF_MUTE_BURST;
			chip->iec61937.mute_rep = 64;
			chip->iec61937.latency=TIME_TO_FRAMES(rate,decode_lat);
			break;
		default:
			printk("%s unrecognised IEC61937 mode\n",__FUNCTION__);
			/*fallthorugh*/
		case ENCODING_IEC60958:
			chip->iec61937.pause_mode=SPDIF_MUTE_NULL_DATA;
			chip->iec61937.mute_rep =1;
			chip->iec61937.latency=256;
			break;
	}
}


static int stb7100_program_spdifplayer(snd_pcm_substream_t *substream){

	unsigned long reg;
	snd_pcm_runtime_t *runtime = substream->runtime;
	pcm_hw_t          *chip = snd_pcm_substream_chip(substream);
	u32 val=0;
	unsigned long flags=0;
	u32 irq_enable = ENABLE_INT_NSAMPLE;

	reg =(runtime->period_size * runtime->channels) << SPDIF_SAMPLES_SHIFT;
	reg |= SPDIF_SW_STUFFING | SPDIF_BIT16_DATA_NOROUND;

	if(chip->fifo_check_mode)
		irq_enable |= SPDIF_INT_STATUS_UNF;

	spin_lock_irqsave(&chip->lock,flags);
	switch(chip->oversampling_frequency)
	{
		case 128:
			reg |= SPDIF_FSYNTH_DIVIDE32_128;
			break;
		case 256:
			reg |= SPDIF_FSYNTH_DIVIDE32_256;
			break;
		default:
			printk("stb7100_program_spdifplayer: unsupported oversampling frequency %d\n",chip->oversampling_frequency);
			break;
	}

	reset_spdif_on(chip);
	reset_spdif_off(chip);

	/*
	 * Setup channel status bits for the hardware mode and prepare for
	 * starting a new data burst. Also setup the hardware pause burst
	 * registers with the channel status as well.
	 */
	chip->pending_spdif_control = chip->default_spdif_control;
	iec60958_set_runtime_status(substream);
	chip->iec60958_output_count = 0;
	stb7100_spdif_set_iec_mode(substream);

	val = chip->pending_spdif_control.channel.status[0]	   |
	      (chip->pending_spdif_control.channel.status[1] <<8)  |
	      (chip->pending_spdif_control.channel.status[2] <<16) |
	      (chip->pending_spdif_control.channel.status[3] <<24);

	writel(val, chip->pcm_player + AUD_SPDIF_CL1);
	writel(val, chip->pcm_player + AUD_SPDIF_CR1);

	val = chip->pending_spdif_control.channel.status[4];
	val |= (val << 8); /* Right channel status always = left channel status */
	writel(val, chip->pcm_player + AUD_SPDIF_CL2_CR2_UV);

	/*enable the latency irq*/
	if(get_spdif_syncing_status()){
		irq_enable |= ENABLE_INT_EOLATENCY;
		chip->spdif_player_mode = SPDIF_ENCODED_ON;
	}
	if(chip->iec_encoding_mode != ENCODING_IEC60958){

		int pause_data_type=-0;
		reg |= SPDIF_HW_STUFFING;
		switch(chip->iec_encoding_mode){
			case ENCODING_IEC61937_AC3:
				pause_data_type	= IEC61937_AC3_STREAM;
				break;
			case ENCODING_IEC61937_DTS_1:
				pause_data_type	= IEC61937_DTS_TYPE_1;
				break;
			case ENCODING_IEC61937_DTS_2:
				pause_data_type= IEC61937_DTS_TYPE_2;
				break;
			case ENCODING_IEC61937_DTS_3:
				pause_data_type= IEC61937_DTS_TYPE_3;
				break;
			default:
				printk("%s Uncrecognised Encoded Data Stream %d\n",
					__FUNCTION__,chip->iec_encoding_mode);
				break;
		}
		val =( 1 <<16)  | (chip->iec61937.mute_rep & 0x0000ffff);
		writel(val,chip->pcm_player + AUD_SPDIF_FRA_LEN_BST);

		val = (IEC61937_PA <<16)| IEC61937_PB ;
		writel(val ,chip->pcm_player +AUD_SPDIF_PA_PB );

		val = 	(pause_data_type <<16) | chip->iec61937.mute_rep;
		writel(val,chip->pcm_player +AUD_SPDIF_PC_PD);

		chip->spdif_player_mode = SPDIF_ENCODED_ON;
	}
	else{
		reg |= SPDIF_SW_STUFFING;
		chip->spdif_player_mode =SPDIF_ENCODED_ON;
	}

	val =((chip->iec61937.mute_rep <<16) |
			(chip->iec61937.latency & 0x0000ffff)) ;
	writel(val,chip->pcm_player + AUD_SPDIF_PAU_LAT);

	chip->pcmplayer_control = reg;
	chip->irq_mask = irq_enable;
	writel(0,chip->pcm_player +STM_PCMP_IRQ_EN_SET);
	writel(chip->irq_mask,chip->pcm_player +STM_PCMP_IRQ_EN_SET);
	spin_unlock_irqrestore(&chip->lock,flags);

	return 0;
}


static int stb7100_spdif_program_hw(snd_pcm_substream_t *substream)
{
	int err;
	if((err = stb7100_program_fsynth(substream)) < 0)
		return err;

	if((err = stb7100_program_spdifplayer(substream)) < 0)
		return err;

	if((err = stb7100_program_fdma(substream)) < 0)
		return err;

	return 0;
}


static int stb7100_spdif_open(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
	pcm_hw_t          *chip = snd_pcm_substream_chip(substream);
	int err=0;
	const char * dmac_id =STM_DMAC_ID;
	const char * lb_cap_channel = STM_DMA_CAP_LOW_BW;
	const char * hb_cap_channel = STM_DMA_CAP_HIGH_BW;

	if(chip->iec60958_rawmode){
		runtime->hw.info   |= SNDRV_PCM_INFO_MMAP;
		runtime->hw.formats = SNDRV_PCM_FMTBIT_S32_LE; /* Only 32bit in RAW mode */
	}

	if(chip->fdma_channel <0){
		err=request_dma_bycap(&dmac_id,&hb_cap_channel,"STB710x_SPDIF_DMA");
		if(err <0){
			err=request_dma_bycap(&dmac_id,&lb_cap_channel,	"STB710x_SPDIF_DMA");
			if(err <0){
				printk(" %s error in DMA request %d\n",__FUNCTION__,err);
				return err;
			}
		}
		chip->fdma_channel= err;
	}
	return 0;
}


static stm_playback_ops_t stb7100_spdif_ops = {
	.free_device      = stb7100_pcm_free,
	.open_device      = stb7100_spdif_open,
	.program_hw       = stb7100_spdif_program_hw,
	.playback_pointer = stb7100_fdma_playback_pointer,
	.start_playback   = stb7100_spdif_start_playback,
	.stop_playback    = stb7100_spdif_stop_playback,
	.pause_playback   = stb7100_spdif_pause_playback,
	.unpause_playback = stb7100_spdif_unpause_playback
};

static struct platform_device *spdif_platform_device;

static int __init stb710x_alsa_spdif_probe(struct device *dev)
{
	spdif_platform_device = to_platform_device(dev);
	return 0;
}

static struct device_driver alsa_spdif_driver = {
	.name  = "710x_ALSA_SPD",
	.owner = THIS_MODULE,
	.bus   = &platform_bus_type,
	.probe = stb710x_alsa_spdif_probe,
};

static struct device alsa_spdif_device = {
	.bus_id="alsa_710x_spdif",
	.driver = &alsa_spdif_driver,
	.parent   = &platform_bus ,
	.bus      = &platform_bus_type,
};



int snd_spdif_stb710x_probe(pcm_hw_t **in_chip,snd_card_t **card,int dev)
{

	int err=0;
	pcm_hw_t *chip={0};
	static snd_device_ops_t ops = {
		.dev_free = snd_pcm_dev_free,
	};

	if(driver_register(&alsa_spdif_driver)==0){
		if(device_register(&alsa_spdif_device)!=0)
			return -ENOSYS;
	}
	else return -ENOSYS;

	if((chip = kcalloc(1,sizeof(pcm_hw_t), GFP_KERNEL)) == NULL)
        	return -ENOMEM;

	*card = snd_card_new(index[card_list[dev].major],id[card_list[dev].major], THIS_MODULE, 0);
        if (card == NULL){
      		printk(" cant allocate new card of %d\n",card_list[dev].major);
      		return -ENOMEM;
        }

	sprintf((*card)->driver,   "%d",card_list[dev].major);
	strcpy((*card)->shortname, "STb7100_SPDIF");
	sprintf((*card)->longname, "STb7100_SPDIF");

	spin_lock_init(&chip->lock);
	chip->irq          = -1;
	chip->fdma_channel = -1;

	chip->card         = *card;
	chip->card_data = &card_list[dev];

	chip->hw           = stb7100_spdif_hw;
	chip->playback_ops  = &stb7100_spdif_ops;

	chip->oversampling_frequency = 128; /* This is for HDMI compatibility */
	chip->pcm_clock_reg = ioremap(AUD_CFG_BASE, 0);
	chip->out_pipe      = ioremap(FDMA2_BASE_ADDRESS, 0);
	chip->pcm_player    = ioremap(SPDIF_BASE,0);



	iec60958_default_channel_status(chip);
	chip->iec_encoding_mode = ENCODING_IEC60958;

	if(request_irq(LINUX_SPDIFPLAYER_ALLREAD_IRQ,
                       stb7100_spdif_interrupt, SA_INTERRUPT,
                       "STB7100 SPDIF Player",(void*)chip))
	{
		printk((">>> failed to get IRQ\n"));
		stb7100_pcm_free(chip);
		return -EBUSY;
	}
	else
		chip->irq = LINUX_SPDIFPLAYER_ALLREAD_IRQ;

	if(register_platform_driver(spdif_platform_device,chip,card_list[dev].major)!=0)
		return -ENODEV;

	if ((err = snd_card_pcm_allocate(chip, chip->card_data->minor, (*card)->longname)) < 0){
		printk(">>> failed to create PCM stream\n");
		stb7100_pcm_free(chip);
		return err;
	}

	if ((err = snd_iec60958_create_controls(chip)) < 0){
		printk(">>> failed to create SPDIF ctls\n");
		stb7100_pcm_free(chip);
		return err;
	}
	if((err = snd_generic_create_controls(chip)) < 0){
		printk(">>> failed to create generic ctls\n");
		stb7100_pcm_free(chip);
		return err;
	}

	if ((err = snd_device_new((*card), SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0){
		printk(">>> creating sound device failed\n");
		stb7100_pcm_free(chip);
		return err;
	}

	if ((err = snd_card_register((*card))) < 0) {
		printk(">>> cant register card\n");
		snd_card_free(*card);
		return err;
	}
	*in_chip = chip;
	return 0;
}
