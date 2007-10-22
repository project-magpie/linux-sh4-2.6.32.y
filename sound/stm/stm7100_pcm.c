/*
 *  STb710x PCM Player Sound Driver
 *  Copyright (c)   (c) 2005 STMicroelectronics Limited
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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>

static unsigned long pcm_base_addr[SND_DRV_CARDS] =
{
	PCMP0_BASE,
	PCMP1_BASE,
	SPDIF_BASE,
	PCM0_CONVERTER_BASE,
	PCMIN_BASE
};

static unsigned long linux_pcm_irq[SND_DRV_CARDS] =
{
	LINUX_PCMPLAYER0_ALLREAD_IRQ,
	LINUX_PCMPLAYER1_ALLREAD_IRQ,
    	LINUX_SPDIFPLAYER_ALLREAD_IRQ,
    	LINUX_SPDIFCONVERTER_ALLREAD_IRQ,
    	LINUX_PCMREADER_ALLREAD_IRQ,
};
/*
 * Extra PCM Player format regsiter define for 7100 Cut2/3
 */
#define PCMP_CHANNELS_SHIFT     (8)
/*
 * On Cut2/3 7100 DMA requests can be triggered when 2,4,6,8 or 10 cells
 * are available in the PCMP Player FIFO. For the moment pick the middle
 * value.
 */
#define PCMP_DREQ_TRIGGER       (6L)
#define PCMP_DREQ_TRIGGER_SHIFT (12)

#define AUD_ADAC_CTL_REG		0x100
#define AUD_IO_CTL_REG			0x200

/*
 * AUD_ FSYNTH_CFG control vals
 */
#define AUD_FSYNTH_SATA_PHY_30MHZ_REF		(0<<23)
#define AUD_FSYNTH_SYSBCLKINALT_REF		(1<<23)
#define AUD_FSYNTH_VGOOD_REF_SOURCE		(0<<16)
#define AUD_FSYNTH_GOOD_REF_SOURCE		(1<<16)
#define AUD_FSYNTH_BAD_REF_SOURCE		(2<<16)
#define AUD_FSYNTH_VBAD_REF_SOURCE		(3<<16)
#define AUD_FSYNTH_FS_REF_CLK_27_30MHZ		(0<<15)
#define AUD_FSYNTH_FS_REF_CLK_54_60MHZ		(1<<15)
#define AUD_FSYNTH_NPDA_POWER_DOWN		(0<<14)
#define AUD_FSYNTH_NPDA_POWER_UP		(1<<14)

#define AUD_FSYNTH_UNKNOWN_STANDBY		(0<<13)
#define AUD_FSYNTH_UNKNOWN_ACTIVE		(1<<13)
#define AUD_FSYNTH_FSYNTH2_STANDBY		(0<<12)
#define AUD_FSYNTH_FSYNTH2_ACTIVE		(1<<12)
#define AUD_FSYNTH_FSYNTH1_STANDBY		(0<<11)
#define AUD_FSYNTH_FSYNTH1_ACTIVE		(1<<11)
#define AUD_FSYNTH_FSYNTH0_STANDBY		(0<<10)
#define AUD_FSYNTH_FSYNTH0_ACTIVE		(1<<10)

#define AUD_FSYNTH_RESERVED_9			(1<<9)
#define AUD_FSYNTH_RESERVED_8			(1<<8)
#define AUD_FSYNTH_RESERVED_7			(1<<7)
#define AUD_FSYNTH_RESERVED_6			(1<<6)
#define AUD_FSYNTH_RESERVED_5			(1<<5)

#define AUD_FSYNTH_FSYNTH2_BYPASS		(0<<4)
#define AUD_FSYNTH_FSYNTH2_ENABLE		(1<<4)
#define AUD_FSYNTH_FSYNTH1_BYPASS		(0<<3)
#define AUD_FSYNTH_FSYNTH1_ENABLE		(1<<3)
#define AUD_FSYNTH_FSYNTH0_BYPASS		(0<<2)
#define AUD_FSYNTH_FSYNTH0_ENABLE		(1<<2)
#define AUD_FSYNTH_RESERVED_1			(1<<1)
#define AUD_FSYNTH_RESET_ON			(1<<0)


/*AUD_IO_CTL reg vals*/
#define PCM_DATA_IN	0
#define PCM_DATA_OUT	1

#define PCM_CLK_OUT	0
#define PCM0_OUT	1
#define PCM1_OUT	2
#define SPDIF_ENABLE	3

/*AUD_ADAC_CTL vals*/

#define DAC_NRST     0x1
#define DAC_SOFTMUTE 0x10
/* here we bring the dac sub-blocks out of powerdown these are
 * -DAC BANDGAP  (bit 6)
 * -ANALOUG PART (bit 5)
 * -DIGITAL PART (bit 3)
 * then we reset with bit 1
 */
#define DAC_POWERUP_VAL (1 << 3 | 1 <<5 | 1 <<6)


/*here we define the block offsets for both pcm players that is fysnth0 & 1
 * We must specify the pe/md/ and sdiv offsets
 * */
 typedef enum {
 	SDIV=0x0,
 	PE,
 	MD,
 	PROG_EN
 }clk_offsets;


 /*unfortunatley for pcm0/1 the reg offsets for the fsynth programming are different
  * hence we select from the table according to the current output*/
static unsigned long dev_fsynth_regs[4][SND_DRV_CARDS]= {
			/*PCM0*/		/*PCM1*/	/*spdif*/
/*SDIV*/	{AUD_FSYN0_SDIV,	AUD_FSYN1_SDIV,		AUD_FSYN2_SDIV},
/*PE*/		{AUD_FSYN0_PE,		AUD_FSYN1_PE,		AUD_FSYN2_PE},
/*MD*/		{AUD_FSYN0_MD,		AUD_FSYN1_MD,		AUD_FSYN2_MD},
/*PROG_EN*/	{AUD_FSYN0_PROG_EN,	AUD_FSYN1_PROG_EN,	AUD_FSYN2_PROG_EN}};


#define MEM_FULL_READIRQ	0x02
#define SELECT_PROG_FSYN	0x01
#define SELECT_RSTP		0x01
#define SELECT_PCM_FSYN_CLK	(0x01 << 2)
#define SELECT_SYSBCLKINALT	(0x01 << 23)


static snd_pcm_hardware_t stb7100_pcm_hw =
{
	.info =		(SNDRV_PCM_INFO_MMAP           |
			 SNDRV_PCM_INFO_INTERLEAVED    |
			 SNDRV_PCM_INFO_BLOCK_TRANSFER |
			 SNDRV_PCM_INFO_MMAP_VALID     |
			 SNDRV_PCM_INFO_PAUSE),

	/*.formats   now defined at runtime on card - card basis due
	to reliability problems when switching between modes on cards utilising an external DAC*/	
	

	.rates =	(SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_96000 |
			 SNDRV_PCM_RATE_192000 ),

	.rate_min	  = 32000,
	.rate_max	  = 192000,
	.channels_min	  = 10,/*vals now taken from setup.c for platform*/
	.channels_max	  = 10,/*specific channel availability -but we must still provide a default*/
	.buffer_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,10),
	.period_bytes_min = FRAMES_TO_BYTES(1,10),
	.period_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,10),
	.periods_min	  = 1,
	.periods_max	  = PCM_MAX_FRAMES
};


/*
 * The following FSynth programming has been provided by ST validation
 * teams for STb7100 Cut1.3. They are for an oversampling frequency of 256*Fs.
 *
 *  peq is the value for a 0.001 % adjustment of the current output freq
 *  which is defined by the forumlae in the manual.
 */

static struct stm_freq_s gClockSettings[NUM_CLOCK_SETTINGS] =
{
     	/*             freq(Fs) sdiv  pe      md   peq */
	/*8.1920 == */{ 32000,  0x4, 0x5a00, 0xfd, 0xa},
	/*11.2895Mhz*/{ 44100,  0x4, 0x5EE9, 0xF5 ,0x7},
	/*12.2880MHz*/{ 48000,  0x4, 0x3C00, 0xF3 ,0x7},
	/*24.5760MHz*/{ 96000,  0x3, 0x3C00, 0xF3 ,0x6},
	/*36.8640MHz*/{ 192000, 0x2, 0x7AAB ,0xFA, 0x8}
};


/*Here we can  dynamically adjust the sampling frequencies of playback on the
 * pcm0/1 players and the SPDIF. The function allows a +/- 10 % adjustment of
 * frequency in 0.001% increments of current playback frequency.
 *
 * The adjusts parm indicates the number of .001% adjustments to apply,
 *  up to a maximum of 10000, or 10%
 *
 * There is an error  in the order of 1 % due to relying on integer
 * approximations of float values, not available in the kernel.
 * Additionally the PE solutions of the equation do not offer a linear response,
 * here we assume a linear response and discard the remainder as error.
 * We can never calculate the exact output frequency from this driver,
 * so we utilise a quanta value which represents a % adjustment of each frequency
 * */
int adjust_audio_clock(snd_pcm_substream_t *substream,int adjusts,int dir)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;

	int i=0,total_shift=0;
	unsigned long new_pe=0, peq=0,new_md=0,new_sdiv=0;

	unsigned long pg_en_offset = dev_fsynth_regs[PROG_EN][chip->card_data->major];
	unsigned long pe_offset    = dev_fsynth_regs[PE][chip->card_data->major];
	unsigned long md_offset    = dev_fsynth_regs[MD][chip->card_data->major];
 	unsigned long sdiv_offset  = dev_fsynth_regs[SDIV][chip->card_data->major];

	unsigned long cur_pe   = readl(chip->pcm_clock_reg + pe_offset);
	unsigned long cur_md   = readl(chip->pcm_clock_reg + md_offset);
	unsigned long cur_sdiv = readl(chip->pcm_clock_reg + sdiv_offset);

	if( ((adjusts <=0) || (adjusts >10000)) || !substream)
		return -EINVAL;

	/*get the correct fsynth settings for FS*/
	while(i < NUM_CLOCK_SETTINGS && runtime->rate != gClockSettings[i].freq)
		i++;

	if(!(i<=NUM_CLOCK_SETTINGS))
		return -ENODEV;

	peq = 	gClockSettings[i].pe_quantum;
	new_md = cur_md;
	new_pe = cur_pe;
	new_sdiv = cur_sdiv;

	if(INCR_FSYNTH == dir){
		if(((adjusts*peq )+cur_pe) > 0xffff){
			while(total_shift<=adjusts){
				while(new_pe < 0xffff){
					if(total_shift>=adjusts)
						goto write_fsynth;
					new_pe+=peq;
					total_shift++;
				}
				/*overflow- incr the md and set the
				 * pe down to maintain current FS*/
				new_md--;
				/*now we have crossed the sdiv
				 *md is a 5 bit signed term, leaving
				 * 1-16 available*/
				if(new_md  < 0x10){
					new_sdiv--;
					new_md = 0x1f;
					new_pe +=SDIV_SHIFT_VAL;
				}
				else new_pe -= MD_SHIFT_VAL;
			}
		}
		else new_pe += adjusts *peq;

	}
	else if(DECR_FSYNTH == dir){
		if( (int)(cur_pe-(adjusts*peq )) < (int)0x000){
			while(total_shift <=adjusts){
				while((int)new_pe > (int)0){
					if(total_shift >= adjusts)
						goto write_fsynth;
					new_pe -= peq;
					total_shift++;
				}
				new_md++;

				if(new_md  >0x1f) {
					new_sdiv++;
					new_md =0x10;
					new_pe +=SDIV_SHIFT_VAL;
				}
				else new_pe+=MD_SHIFT_VAL;
			}
		}
		else new_pe -= adjusts * peq;
	}
	else return -EINVAL;



write_fsynth:
	writel(0,chip->pcm_clock_reg+pg_en_offset);
	writel(new_pe,chip->pcm_clock_reg + pe_offset);
	writel(new_md,chip->pcm_clock_reg + md_offset);
	writel(new_sdiv,chip->pcm_clock_reg + sdiv_offset);
	writel(1,chip->pcm_clock_reg+pg_en_offset);
	return 0;
}


void stb7100_reset_pcm_player(pcm_hw_t  *chip)
{
	/* Give the pcm player a soft reset */
	writel(1,chip->pcm_player );
	writel(0,chip->pcm_player );
}


static void stb7100_reset_internal_DAC(pcm_hw_t *chip)
{
	writel(0,chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
	writel((DAC_POWERUP_VAL|DAC_SOFTMUTE),chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
}


static void stb7100_pcm_stop_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long reg=0;

	spin_lock(&chip->lock);

	/*if we have fifo checking turned on we must also stop this interrupt first to
	 * prevent an error condition being generated as the player is disabled*/
	if(chip->fifo_check_mode)
		writel( ENABLE_INT_UNDERFLOW,chip->pcm_player + STM_PCMP_IRQ_EN_CLR);

	if(chip->card_data->major == PCM1_DEVICE){
		reg = readl(chip->pcm_clock_reg+AUD_ADAC_CTL_REG) | DAC_SOFTMUTE;
		writel(reg, chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
		udelay(100);
		reg = reg & ~DAC_NRST; /* Reset active low */
		writel(reg, chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
	}

	/*
	 * Disable PCM Player IRQ, this is important when switching
	 * between PCM0 and the protocol converter as there is
	 * nowhere else to turn the interrupts off, and the
	 * PCM0 interrupt will fire with the wrong card structure!
	 */
	writel(MEM_FULL_READIRQ, chip->pcm_player + STM_PCMP_IRQ_EN_CLR);

	/*
	 * We use "mute" to stop the PCM player, which in fact is implemented
	 * as a pause, rather than "off" becuase it keeps the L/R clocks
	 * running to the DACs. This avoids noise on the analogue output
	 * and occasional DAC failures, due to the DACs being intolerant of
	 * losing their clocks unless in reset or powerdown modes. At the
	 * moment we have no control over external DACs in this code.
	 *
	 * Note: the internal DAC doesn't absolutely need this (as it
	 * can be reset).
	 */
	 /* It is  possible for us to effect a mute at the internal DAC
	 * rather than in the player.  For the externally connected DAC, this is not
	 * possible.
	 */
	if( chip->card_data->major == PCM1_DEVICE){
		writel((chip->pcmplayer_control|PCMP_OFF),chip->pcm_player+STM_PCMP_CONTROL);
	}
	else writel((chip->pcmplayer_control|PCMP_MUTE),chip->pcm_player+STM_PCMP_CONTROL);

	spin_unlock(&chip->lock);

	dma_stop_channel(chip->fdma_channel);
	dma_params_free(&chip->dmap);
	chip->dma_req = NULL; /* free'd by the above call to dma_params_free() */
}


static void stb7100_pcm_start_playback(snd_pcm_substream_t *substream)
{
	pcm_hw_t     *chip = snd_pcm_substream_chip(substream);
	unsigned long reg=0;
	int res = dma_xfer_list(chip->fdma_channel,&chip->dmap);
	if(res !=0)
		printk("%s FDMA_CH %d failed to start %d\n",__FUNCTION__,chip->fdma_channel,res);

	spin_lock(&chip->lock);

	/*
	 * We appear to need to reset the PCM player otherwise we end up
	 * with channel data sent to the wrong channels when starting up for
	 * the second time.
	 */
	stb7100_reset_pcm_player(chip);

	writel((chip->pcmplayer_control | PCMP_ON), chip->pcm_player + STM_PCMP_CONTROL);

	if(chip->card_data->major == PCM1_DEVICE){
		reg = readl(chip->pcm_clock_reg+AUD_ADAC_CTL_REG) | DAC_NRST; /* Bring DAC out of Reset */
		writel(reg, chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
		udelay(100);
		writel((reg & ~DAC_SOFTMUTE),chip->pcm_clock_reg+AUD_ADAC_CTL_REG); /* Unmute */
	}
	/*again we enable this err-checking interrput last to prevent an err condiftion
	 * as the device is programmed */
	if(chip->fifo_check_mode)
		writel( ENABLE_INT_UNDERFLOW,chip->pcm_player + STM_PCMP_IRQ_EN_SET);

	

	spin_unlock(&chip->lock);
}


static void stb7100_pcm_unpause_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long reg=0;

        spin_lock(&chip->lock);
	if(chip->card_data->major == PCM1_DEVICE){
		reg = readl(chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
		writel((reg & ~DAC_SOFTMUTE),chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
        }
	if(chip->fifo_check_mode)
		writel( ENABLE_INT_UNDERFLOW,chip->pcm_player + STM_PCMP_IRQ_EN_SET);

	writel((chip->pcmplayer_control|PCMP_ON),chip->pcm_player+STM_PCMP_CONTROL);
	spin_unlock(&chip->lock);
}


static void stb7100_pcm_pause_playback(snd_pcm_substream_t *substream)
{
        pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long reg=0;

	spin_lock(&chip->lock);
	if(chip->card_data->major == PCM1_DEVICE){
	        reg = readl(chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
	        writel((reg | DAC_SOFTMUTE),chip->pcm_clock_reg+AUD_ADAC_CTL_REG);
	}
	writel((chip->pcmplayer_control|PCMP_MUTE),chip->pcm_player+STM_PCMP_CONTROL);

	if(chip->fifo_check_mode)
		writel( ENABLE_INT_UNDERFLOW,chip->pcm_player + STM_PCMP_IRQ_EN_CLR);

	spin_unlock(&chip->lock);
}

static snd_pcm_uframes_t stb7100_fdma_playback_pointer(snd_pcm_substream_t * substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	/*
	 * Calculate our current playback position, using the number of bytes
	 * left for the DMA engine needs to transfer to complete a full
	 * iteration of the buffer. This is common to all STb7100 audio players
	 * using the FDMA (including SPDIF).
	 */
	u32 pos = substream->runtime->dma_bytes - get_dma_residue(chip->fdma_channel);
	return bytes_to_frames(substream->runtime,pos);
}


static irqreturn_t stb7100_pcm_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long val;
	pcm_hw_t *stb7100 = dev_id;
	irqreturn_t res =IRQ_NONE;

	/* Read and clear interrupt status */
	spin_lock(&stb7100->lock);
	val = readl(stb7100->pcm_player + STM_PCMP_IRQ_STATUS);
	writel(val,stb7100->pcm_player + STM_PCMP_ITS_CLR);
	spin_unlock(&stb7100->lock);

	if(unlikely((val & ENABLE_INT_UNDERFLOW ) == ENABLE_INT_UNDERFLOW)){
		printk("%s PCM PLayer #%d FIFO Underflow detected\n",
			__FUNCTION__,
			stb7100->current_substream->pcm->card->number);
		res = IRQ_HANDLED;
	}
	if(likely(val & PCMP_INT_STATUS_ALLREAD)){
		/*Inform higher layer that we have completed a period */
		snd_pcm_period_elapsed(stb7100->current_substream);
		res=  IRQ_HANDLED;
	}
	return  res;
}

static struct stm_dma_req_config req_config = {
	.rw		= REQ_CONFIG_WRITE,
	.opcode		= REQ_CONFIG_OPCODE_4,
	.count		= 1,
	.increment	= 0,
	.hold_off	= 0,
	.initiator	= 0, /* This was 1 for 7100, do we need to fix? */
};

static int stb7100_program_fdma(snd_pcm_substream_t *substream)
{
	pcm_hw_t          *chip    = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	unsigned long irqflags=0;
	int err=0;

	if(!chip->out_pipe || ! chip->pcm_player)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock,irqflags);

	if (chip->dma_req == NULL) {
		chip->dma_req = dma_req_config(chip->fdma_channel, chip->fdma_req, &req_config);
		if (chip->dma_req == NULL) {
			spin_unlock_irqrestore(&chip->lock,irqflags);
			return -EBUSY;
		}
	}

	dma_params_init(&chip->dmap,
			MODE_PACED,
			STM_DMA_LIST_CIRC);

	chip->buffer_start_addr = (unsigned long)runtime->dma_addr;

	dma_params_DIM_1_x_0(&chip->dmap);

	dma_params_req(&chip->dmap, chip->dma_req);

	dma_params_addrs(&chip->dmap,runtime->dma_addr,
			virt_to_phys(chip->pcm_player+STM_PCMP_DATA_FIFO),
			snd_pcm_lib_buffer_bytes(substream));

	dma_compile_list(chip->fdma_channel, &chip->dmap, GFP_KERNEL);

	spin_unlock_irqrestore(&chip->lock,irqflags);
	return err;
}


static int stb7100_program_fsynth(snd_pcm_substream_t *substream)
{
	int i;
	snd_pcm_runtime_t *runtime = substream->runtime;
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
        unsigned long flags=0;
        int err=0, dev_num=0,sdiv=0;

	spin_lock_irqsave(&chip->lock,flags);
        dev_num = chip->card_data->major == PROTOCOL_CONVERTER_DEVICE ?
        			0:
        			chip->card_data->major;

	if(! runtime->rate  || ! chip->pcm_clock_reg){
		err= -EINVAL;
		goto exit;
	}

	else for(i=0; i < NUM_CLOCK_SETTINGS; i++) {
		if (runtime->rate == gClockSettings[i].freq){

			writel(0,chip->pcm_clock_reg +dev_fsynth_regs[PROG_EN][dev_num]);

                        /*if we are using the PCM converter we require a divisor of 128 not 256
                           therefore our SDIV must be incremented to account for this
                         */
			switch(chip->oversampling_frequency){
				case 128:
					/*
					 * FSynth setting are for 256xFs, adding
					 * one to sdiv changes this to 128xFs
					 */
					sdiv = gClockSettings[i].sdiv_val +1;
					break;
				case 256:
					sdiv = gClockSettings[i].sdiv_val;
					break;
				default:
					printk("snd_pcm_program_freq: unsupported oversampling frequency %d\n",chip->oversampling_frequency);
					err= -EINVAL;
					goto exit;
			}

                        writel(sdiv,chip->pcm_clock_reg +dev_fsynth_regs[SDIV][dev_num]);

			writel(gClockSettings[i].md_val ,
				chip->pcm_clock_reg + dev_fsynth_regs[MD][dev_num]);

			writel(gClockSettings[i].pe_val,
				chip->pcm_clock_reg + dev_fsynth_regs[PE][dev_num]);

			writel(SELECT_PROG_FSYN,chip->pcm_clock_reg +dev_fsynth_regs[PROG_EN][dev_num]);
			writel(0,chip->pcm_clock_reg +dev_fsynth_regs[PROG_EN][dev_num]);
			err = 0;
			goto exit;
		}
	}
	err =  -1;
exit:
	spin_unlock_irqrestore(&chip->lock,flags);
	return err;

}


static int stb7100_program_pcmplayer(snd_pcm_substream_t *substream)
{
	unsigned long ctrlreg, fmtreg;
	snd_pcm_runtime_t *runtime = substream->runtime;
	pcm_hw_t          *chip = snd_pcm_substream_chip(substream);
	unsigned long irqmask = MEM_FULL_READIRQ;
	unsigned long flags=0;

	fmtreg = PCMP_FORMAT_32  | PCMP_ALIGN_START  | PCMP_MSB_FIRST  |
		 chip->i2s_sampling_edge |PCMP_PADDING_ON;

	ctrlreg = (runtime->period_size * runtime->channels) << PCMP_SAMPLES_SHIFT;

	/*
	 * The PCM data format is set to be I2S.
	 * External DACs must be configured to expect this format and
	 * an oversampling frequency of 256*Fs. Please see the documentation
	 * on http://www.stlinux.com for board configuration information.
	 *
         * Except when we are running the PCM0 with the spdif converter, in
         * which case the HDMI expects an oversampling frequency of 128*FS,
         * as defined in the spec.
         */

        ctrlreg |= PCMP_NO_ROUNDING;
	
	/*We will always clock out I2s from the DAC's, however, when in 16bit
	 * mode we can flip the L/R ordering bit to give the correct LR ordering for
	 * 16 bit packed into I2s.  With the caviat that */
        if(runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
		ctrlreg |= PCMP_MEM_FMT_16_16;
		fmtreg  |= PCMP_LENGTH_16 | PCMP_LRLEVEL_LEFT_HIGH;
        } else {
		ctrlreg |= PCMP_MEM_FMT_16_0;
		fmtreg  |= PCMP_LENGTH_24| PCMP_LRLEVEL_LEFT_LOW;
        }


	/*
	 * Note that the frequency divide is the same for both 32bit and 16bit
	 * data input, because the number of _output_ bits per subframe is
	 * always 32.
	 */
	spin_lock_irqsave(&chip->lock,flags);
	switch(chip->oversampling_frequency){
		case 128:
			ctrlreg |= PCMP_FSYNTH_DIVIDE32_128;
			break;
		case 256:
			ctrlreg |= PCMP_FSYNTH_DIVIDE32_256;
			break;
		default:
			printk("snd_pcm_program_pcmplayer: unsupported oversampling frequency %d\n",chip->oversampling_frequency);
			break;
	}

	if(PROTOCOL_CONVERTER_DEVICE==chip->card_data->major){
		/*this call will result in a reset and sleep of the
		 * converter, so we abandon locks now.*/
	 	spin_unlock_irqrestore(&chip->lock,flags);
		stb7100_converter_program_player(substream);
		spin_lock_irqsave(&chip->lock,flags);
	}

	if(get_spdif_syncing_status()==SPDIF_SYNC_MODE_ON)
		ctrlreg |= PCMP_WAIT_SPDIF_LATENCY;
	else
		ctrlreg |= PCMP_IGNORE_SPDIF_LATENCY;

	chip->pcmplayer_control = ctrlreg;
        /*
         * The 7100 cut >=3 can use 2-10 channels, cut < 3 is like the
         * stm8000 and is fixed to 5 stereo channels. 7109 is always dynamic
         * channel programmable.
         */

	fmtreg |= (runtime->channels/2) << PCMP_CHANNELS_SHIFT;
	fmtreg |= PCMP_DREQ_TRIGGER << PCMP_DREQ_TRIGGER_SHIFT;
	writel(fmtreg, chip->pcm_player + STM_PCMP_FORMAT);

	/*enable the allread irq - but only for the pcm players, the pcm
	 * converter takes this interrupt during I2s->IEC60958 mode*/
	if(PROTOCOL_CONVERTER_DEVICE != chip->card_data->major){
		writel(irqmask,chip->pcm_player + STM_PCMP_IRQ_EN_SET);
	}
	spin_unlock_irqrestore(&chip->lock,flags);
	return 0;
}


static int stb7100_pcm_program_hw(snd_pcm_substream_t *substream)
{
	int err=0;
	if((err = stb7100_program_fsynth(substream)) < 0)
		return err;

	if((err = stb7100_program_pcmplayer(substream)) < 0)
		return err;

	if((err = stb7100_program_fdma(substream)) < 0)
		return err;

	return 0;
}


static int stb7100_pcm_free(pcm_hw_t *card)
{
	writel(PCMP_OFF, card->pcm_player + STM_PCMP_CONTROL);
	iounmap(card->pcm_clock_reg);
	iounmap(card->out_pipe);
	iounmap(card->pcm_player);

	if(card->irq > 0)
		free_irq(card->irq,(void *)card);

	if(card->fdma_channel>=0)
		free_dma(card->fdma_channel);

	kfree(card);

	return 0;
}


static void set_default_device_clock(pcm_hw_t * chip)
{
/*
 * Set a default clock frequency running for each device. Not doing this
 * can lead to clocks not starting correctly later, for reasons that
 * cannot be explained at this time.
 */

 /*the protocol converter clocks from the pcm0 clock(fsynth0)*/
 	int dev = (PROTOCOL_CONVERTER_DEVICE == chip->card_data->major) ?
 					0:
 					chip->card_data->major;

	writel(0,chip->pcm_clock_reg +dev_fsynth_regs[PROG_EN][dev]);

        writel(gClockSettings[0].sdiv_val ,
               chip->pcm_clock_reg +dev_fsynth_regs[SDIV][dev]);

        writel(gClockSettings[0].md_val ,
               chip->pcm_clock_reg + dev_fsynth_regs[MD][dev]);

        writel(gClockSettings[0].pe_val,
               chip->pcm_clock_reg + dev_fsynth_regs[PE][dev]);

        writel(SELECT_PROG_FSYN,chip->pcm_clock_reg +
               dev_fsynth_regs[PROG_EN][dev]);

        writel(0,chip->pcm_clock_reg +
               dev_fsynth_regs[PROG_EN][dev]);
        writel(0,chip->pcm_player + STM_PCMP_CONTROL);
}


static void stb7100_pcm0_create(pcm_hw_t *stb7100)
{
	unsigned long reg;
	/*
	 * Do a one time setup of the audio clock system
	 *
	 * First put the audio FSynth block into reset
	 */
	reg =	AUD_FSYNTH_SATA_PHY_30MHZ_REF	|
		AUD_FSYNTH_VGOOD_REF_SOURCE	|
		AUD_FSYNTH_FS_REF_CLK_27_30MHZ	|
		AUD_FSYNTH_NPDA_POWER_UP	|
		AUD_FSYNTH_FSYNTH2_ACTIVE	|
		AUD_FSYNTH_FSYNTH1_ACTIVE	|
		AUD_FSYNTH_FSYNTH0_ACTIVE	|
		/*
		 * Each of these reserved bits relates to one of the
		 * Fsynth's (6-0 7-1 8-2).  However at the present time
		 * they are not documented.  The datasheets
		 * are currently being updated.  Each must be set to
		 * enable correct playback at 256Khz DAC freq */
		AUD_FSYNTH_RESERVED_6		|
		AUD_FSYNTH_RESERVED_7		|
		AUD_FSYNTH_RESERVED_8		|
		AUD_FSYNTH_FSYNTH2_ENABLE	|
		AUD_FSYNTH_FSYNTH1_ENABLE	|
		AUD_FSYNTH_FSYNTH0_ENABLE;

	writel((reg | AUD_FSYNTH_RESET_ON),stb7100->pcm_clock_reg);
	/*
	 * Now bring it out of reset, powering up the analogue
	 * part and bringing the digital parts out of standby.
	 */
	writel(reg,stb7100->pcm_clock_reg);

	/*
	 * Set all the audio pins to be outputs
	 */
	reg =	PCM_DATA_OUT << PCM_CLK_OUT |
		PCM_DATA_OUT << PCM0_OUT    |
		PCM_DATA_OUT << PCM1_OUT    |
		PCM_DATA_OUT << SPDIF_ENABLE;

	writel(reg,stb7100->pcm_clock_reg+AUD_IO_CTL_REG);
}


static unsigned int stb7100_pcm_channels[] = { 2,4,6,8,10 };


static snd_pcm_hw_constraint_list_t stb7100_constraints_channels = {
		.count = ARRAY_SIZE(stb7100_pcm_channels),
		.list = stb7100_pcm_channels,
		.mask = 0
};

static int stb7100_pcm_open(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
    	int                err=0;
	pcm_hw_t          *chip = snd_pcm_substream_chip(substream);
	const char * dmac_id =STM_DMAC_ID;
	const char * lb_cap_channel = STM_DMA_CAP_LOW_BW;
	const char * hb_cap_channel = STM_DMA_CAP_HIGH_BW;

	if(chip->fdma_channel <0){
		if((err=request_dma_bycap(
					&dmac_id,
					&hb_cap_channel,
					"STB7100_PCM_DMA"))<0){
			if((err=request_dma_bycap(
						&dmac_id,
						&lb_cap_channel,
						"STB7100_PCM_DMA"))<0){
				return -ENODEV;
			}
		}
		chip->fdma_channel= err;
	}
	BUG_ON(chip->fdma_channel <0);
	/*PCMP IP's prior to 7100C3 are fixed to 10 channels, later
	 * revisions and 7109's can program for 2-10 channels - here we
	 * expose the number of programmable channels*/

	runtime->hw.channels_min = chip->min_ch;
	runtime->hw.channels_max = chip->max_ch;
	
	/*
	 *Here we disable 16b mode for PCM0/PCM_CNV.
	 * There is a catch 22 in that both protocol converter & PCM0 will fail to
	 * reinitialise correctly if their clock source is removed, and we cannot
	 * guarantee correct L/R ordering after a 16/32 - 32/16b mode switch
	 * unless we remove the clocks after playback*/
	if(	(chip->card_data->major == PCM0_DEVICE) || 
		(chip->card_data->major == PROTOCOL_CONVERTER_DEVICE))
	
		runtime->hw.formats = SNDRV_PCM_FMTBIT_S32_LE;
	else
		runtime->hw.formats = (SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S16_LE);
	
	runtime->hw.buffer_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,chip->max_ch),
	runtime->hw.period_bytes_min = FRAMES_TO_BYTES(1,chip->min_ch),
	runtime->hw.period_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,chip->max_ch),

	err = snd_pcm_hw_constraint_list(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_CHANNELS,
					 &stb7100_constraints_channels);
	return err;
}


static stm_playback_ops_t stb7100_pcm_ops = {
	.free_device      = stb7100_pcm_free,
	.open_device      = stb7100_pcm_open,
	.program_hw       = stb7100_pcm_program_hw,
	.playback_pointer = stb7100_fdma_playback_pointer,
	.start_playback   = stb7100_pcm_start_playback,
	.stop_playback    = stb7100_pcm_stop_playback,
	.pause_playback   = stb7100_pcm_pause_playback,
	.unpause_playback = stb7100_pcm_unpause_playback
};

static snd_device_ops_t ops = {
    .dev_free = snd_pcm_dev_free,
};


static int stb7100_create_lpcm_device(pcm_hw_t *in_chip,snd_card_t **this_card,int dev)
{
	int err = 0;
	int irq = linux_pcm_irq[dev];

	pcm_hw_t * chip  = in_chip;
	snd_card_t *card={0};

	card = snd_card_new(index[card_list[dev].major],id[card_list[dev].major], THIS_MODULE, 0);
        if (this_card == NULL){
      		printk(" cant allocate new card of %d\n",card_list[dev].major);
      		return -ENOMEM;
        }

	chip->fdma_channel =-1;
	chip->card_data = &card_list[dev];
	spin_lock_init(&chip->lock);



        chip->card          = card;
	chip->irq           = -1;
	chip->pcm_clock_reg = ioremap(AUD_CFG_BASE, 0);
	chip->out_pipe      = ioremap(FDMA2_BASE_ADDRESS,0);
	chip->pcm_converter = 0;
	chip->pcm_player    = ioremap(pcm_base_addr[chip->card_data->major],0);
        chip->hw            = stb7100_pcm_hw;
	chip->oversampling_frequency = 256;

	chip->playback_ops  = &stb7100_pcm_ops;

	sprintf(card->shortname, "STb7100_PCM%d",chip->card_data->major);
	sprintf(card->longname,  "STb7100_PCM%d",chip->card_data->major );
	sprintf(card->driver,    "%d",chip->card_data->major);

	if(request_irq(irq, stb7100_pcm_interrupt, SA_INTERRUPT, "STB7100_PCM", (void*)chip)){
               		printk(">>> failed to get IRQ %d\n",irq);
	                stb7100_pcm_free(chip);
        	        return -EBUSY;
        }
	chip->irq = irq;

    	switch(chip->card_data->major){
	        case PCM0_DEVICE:
        	    	stb7100_pcm0_create(chip);
			break;
		case PCM1_DEVICE:
			stb7100_reset_internal_DAC(chip);
			break;
    	}

	set_default_device_clock(chip);
	stb7100_reset_pcm_player(chip);

	if((err = snd_card_pcm_allocate(chip,chip->card_data->minor,card->longname)) < 0){
        	printk(">>> Failed to create PCM stream \n");
	        stb7100_pcm_free(chip);
    	}
    	if((err = snd_generic_create_controls(chip)) < 0){
		stb7100_pcm_free(chip);
		return err;
	}

	if((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL,chip, &ops)) < 0){
		printk(">>> creating sound device :%d,%d failed\n",chip->card_data->major,chip->card_data->minor);
		stb7100_pcm_free(chip);
		return err;
	}

	if ((err = snd_card_register(card)) < 0) {
		printk("%s snd_card_registration() failed !\n",__FUNCTION__);
		stb7100_pcm_free(chip);
		return err;
	}
	*this_card = card;
	return 0;
}
static struct platform_device *pcm0_platform_device;
static struct platform_device *pcm1_platform_device;
static struct platform_device *cnv_platform_device;

static int stb710x_platform_alsa_probe(struct device *dev);

static struct device_driver alsa_cnv_driver = {
	.name  = "710x_ALSA_CNV",
	.owner = THIS_MODULE,
	.bus   = &platform_bus_type,
	.probe = stb710x_platform_alsa_probe,
};

static struct device_driver alsa_pcm0_driver = {
	.name  = "710x_ALSA_PCM0",
	.owner = THIS_MODULE,
	.bus   = &platform_bus_type,
	.probe = stb710x_platform_alsa_probe,
};

static struct device_driver alsa_pcm1_driver = {
	.name  = "710x_ALSA_PCM1",
	.owner = THIS_MODULE,
	.bus   = &platform_bus_type,
	.probe = stb710x_platform_alsa_probe,
};
static struct device alsa_pcm1_device = {
	.bus_id="alsa_710x_pcm1",
	.driver = &alsa_pcm1_driver,
	.parent   = &platform_bus ,
	.bus      = &platform_bus_type,
};

static struct device alsa_pcm0_device = {
	.bus_id="alsa_710x_pcm0",
	.driver = &alsa_pcm0_driver,
	.parent   = &platform_bus ,
	.bus      = &platform_bus_type,
};
static struct device alsa_cnv_device = {
	.bus_id="alsa_710x_cnv",
	.driver = &alsa_cnv_driver,
	.parent   = &platform_bus ,
	.bus      = &platform_bus_type,
};


static int __init stb710x_platform_alsa_probe(struct device *dev)
{
	if(strcmp(dev->bus_id,alsa_pcm0_driver.name)==0)
	        pcm0_platform_device = to_platform_device(dev);

	else if(strcmp(dev->bus_id,alsa_pcm1_driver.name)==0)
	        pcm1_platform_device = to_platform_device(dev);

	else if(strcmp(dev->bus_id,alsa_cnv_driver.name)==0)
	        cnv_platform_device = to_platform_device(dev);

	else return -EINVAL;

        return 0;
}

static int snd_pcm_card_generic_probe( int dev)
{
	struct device_driver *  dev_driver;
	struct device * device;

	switch(dev){
		case PCM0_DEVICE:
			dev_driver= 	&alsa_pcm0_driver;
			device =  	&alsa_pcm0_device;
			break;
		case PCM1_DEVICE:
			dev_driver= 	&alsa_pcm1_driver;
			device =  	&alsa_pcm1_device;
			break;
		case PROTOCOL_CONVERTER_DEVICE:
			dev_driver= 	&alsa_cnv_driver;
			device =  	&alsa_cnv_device;
			break;
		default:
			return -EINVAL;
	}
	if(driver_register(dev_driver)==0){
		if(device_register(device)!=0)
			return -ENOSYS;
	}
	else return -ENOSYS;
	return 0;
}


static int snd_pcm_stb710x_probe(pcm_hw_t **chip,snd_card_t **card,int dev)
{
	unsigned long err=0;
	if( (err= snd_pcm_card_generic_probe(dev))<0){
               	snd_card_free(*card);
               	return -ENOSYS;
	}

	if((*chip =(pcm_hw_t *) kcalloc(1,sizeof(pcm_hw_t), GFP_KERNEL)) == NULL)
        	return -ENOMEM;

	switch(card_list[dev].major){
       		case PROTOCOL_CONVERTER_DEVICE:
			if(register_platform_driver(cnv_platform_device,*chip,card_list[dev].major)!=0){
				printk("%s Error Registering Protocol Converter\n",__FUNCTION__);
				return -ENODEV;
			}
	      		if((err=  stb7100_create_converter_device(*chip,card,dev))<0){
	       		 	printk("%s Error Creating protocol Converter\n",__FUNCTION__);
	       		       	snd_card_free(*card);
	      		}
			return err;
	        case PCM0_DEVICE:
	        	{
			pcm_hw_t *ip_chip={0};

			if(register_platform_driver(pcm0_platform_device,*chip,card_list[dev].major)!=0){
				printk("%s Error Registering PCM0 player\n",__FUNCTION__);
				return -ENODEV;
			}
	        	if((err = stb7100_create_lpcm_device(*chip,card,PCM0_DEVICE)) <0){
				printk("%s Error Creating PCM0 player\n",__FUNCTION__);
	                       	snd_card_free(*card);
	        	}

	 		if((err = snd_pcmin_stb710x_probe(ip_chip,*card,PCMIN_DEVICE))<0)
				return -ENODEV;

	              	return err;
	        	}
	        case PCM1_DEVICE:

	        	if(register_platform_driver(pcm1_platform_device,*chip,card_list[dev].major)!=0){
				printk("%s Error Registering PCM1 player\n",__FUNCTION__);
				return -ENODEV;
	        	}
	               	if((err = stb7100_create_lpcm_device(*chip,card,PCM1_DEVICE)) <0){
	               		printk("%s Error Creating PCM1 player\n",__FUNCTION__);
	                      	snd_card_free(*card);
	               	}
	                return err;
	        default:
	        	printk("%s Cant Recognise Alsa Card %d\n",__FUNCTION__,dev);
	              	return -ENODEV;
	        }
	return 0;
}

static int __init snd_pcm_card_probe(int dev)
{
	snd_card_t card={0};
	snd_card_t * ptr  = &card;
	pcm_hw_t *chip={0};
	int err=0;

	if(SPDIF_DEVICE == dev){
		if((err = snd_spdif_stb710x_probe(&chip,&ptr,dev))<0)
			goto err_exit;
	}
	else switch(dev){
		case PCM0_DEVICE:
		case PCM1_DEVICE:
		case PROTOCOL_CONVERTER_DEVICE:
	 		err = snd_pcm_stb710x_probe(&chip,&ptr,dev);
	 		if(err < 0)
	 			goto err_exit;
	 		break;
	 	default:
	 		printk("%s Bad Alsa Card vector- %d\n",__FUNCTION__,dev);
	 		goto err_exit;
	}
#if defined(CONFIG_STB7100_FIFO_DEBUG)
	chip->fifo_check_mode=1;
#else
	chip->fifo_check_mode=0;
#endif
        return 0;
err_exit:
	printk("%s Error Initialising Audio Device %d - err %d\n",__FUNCTION__,dev,err);
	return err;
}
