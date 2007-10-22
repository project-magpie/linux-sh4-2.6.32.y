/*
 *  STb7100 PCM->SPDIF protocol converter setup
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


/*
 * Default HW template for PCM player 0 when used with the I2S->SPDIF
 * protocol converter.
 */
static snd_pcm_hardware_t stb7100_converter_hw =
{
	.info =		(SNDRV_PCM_INFO_MMAP           |
			 SNDRV_PCM_INFO_INTERLEAVED    |
			 SNDRV_PCM_INFO_BLOCK_TRANSFER |
			 SNDRV_PCM_INFO_MMAP_VALID     |
			 SNDRV_PCM_INFO_PAUSE),

	.formats =	(SNDRV_PCM_FMTBIT_S32_LE),

	.rates =	(SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000),

	.rate_min	  = 32000,
	.rate_max	  = 48000,
	.channels_min	  = 10,
	.channels_max	  = 10,
	.buffer_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,10),
	.period_bytes_min = FRAMES_TO_BYTES(1,10),
	.period_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,10),
	/*The above 5 parms will be overidden in stb7100_pcm_open once
	 * we have loaded the channel configs for this cpu - we still need
	 * to provide defaults however*/
	.periods_min	  = 1,
	.periods_max	  = PCM_MAX_FRAMES
};


DECLARE_WAIT_QUEUE_HEAD(software_reset_wq);
static volatile int software_reset_complete = 0;

static void reset_pcm_converter(snd_pcm_substream_t * substream)
{
	pcm_hw_t * chip = snd_pcm_substream_chip(substream);
	u32 reg;

	software_reset_complete = 0;
	reg = readl(chip->pcm_converter+AUD_SPDIF_PR_CFG);
	writel((reg|PR_CFG_CONV_SW_RESET),chip->pcm_converter+AUD_SPDIF_PR_CFG);
	wait_event(software_reset_wq, (software_reset_complete != 0));
	writel(reg,chip->pcm_converter+AUD_SPDIF_PR_CFG);
}

static void reset_converter_fifo(snd_pcm_substream_t * substream)
{
	pcm_hw_t * chip = snd_pcm_substream_chip(substream);
	unsigned long reg =readl(chip->pcm_converter+AUD_SPDIF_PR_CFG);
	writel((reg & ~PR_CFG_FIFO_ENABLE),chip->pcm_converter+AUD_SPDIF_PR_CFG);
	writel(reg |=PR_CFG_FIFO_ENABLE ,chip->pcm_converter+AUD_SPDIF_PR_CFG);
}

static inline void bit_duplicate(u32 bits, u32 *word1, u32 *word2)
{
	int i,test_bit;

	*word1 = 0;
	*word2 = 0;
	test_bit = 1;

	for(i=0;i<16;i++) {
		if(bits & test_bit) {
			*word1 |= (1<<(i*2));
			*word1 |= (1<<(i*2+1));
		}

		test_bit <<= 1;
	}
	/*
	 * Note that test bit keeps going!
	 */
	for(i=0;i<16;i++) {
		if(bits & test_bit) {
			*word2 |= (1<<(i*2));
			*word2 |= (1<<(i*2+1));
		}

		test_bit <<= 1;
	}

}


static void stb7100_converter_write_channel_status(pcm_hw_t *chip)
{
	u32 chstatus,word1,word2;

	chstatus = chip->current_spdif_control.channel.status[0]        |
		   (chip->current_spdif_control.channel.status[1] <<8)  |
		   (chip->current_spdif_control.channel.status[2] <<16) |
		   (chip->current_spdif_control.channel.status[3] <<24);

	bit_duplicate(chstatus, &word1, &word2);

	writel(	word1, chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE);

	writel(	word2, chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE + 4);

	chstatus = chip->current_spdif_control.channel.status[4]        |
		   (chip->current_spdif_control.channel.status[5] <<8)  |
		   (chip->current_spdif_control.channel.status[6] <<16) |
		   (chip->current_spdif_control.channel.status[7] <<24);

	bit_duplicate(chstatus, &word1, &word2);

	writel(	word1, chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE + 8);
	writel(	word2, chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE + 12);

	writel(	0, chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE + 16);
	writel(	0, chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE + 20);
}


static irqreturn_t stb7100_converter_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long val;
	unsigned long handled= IRQ_NONE;
	pcm_hw_t *chip = dev_id;

        /* Read and clear interrupt status */
	spin_lock(&chip->lock);
	val = readl(chip->pcm_converter + AUD_SPDIF_PR_INT_STA);
	writel(val,chip->pcm_converter + AUD_SPDIF_PR_INT_CLR);
	spin_unlock(&chip->lock);

	if(val & PR_SOFT_RESET_INT_ENABLE){
		software_reset_complete = 1;
		wake_up(&software_reset_wq);
		handled = IRQ_HANDLED;
	}
	if(val & PR_UNDERFLOW_INT){
		printk("%s I2S Converter PLayer FIFO Underflow detected\n",__FUNCTION__);
		handled = IRQ_HANDLED;
	}
	if(val & PR_I2S_FIFO_OVERRUN_INT){
		printk("%s I2S Converter PLayer FIFO Overflow detected\n",__FUNCTION__);
		handled = IRQ_HANDLED;
	}
	if(val & PR_AUDIO_SAMPLES_FULLY_READ_INT){
            /* Inform higher layer that we have completed a period */
		snd_pcm_period_elapsed(chip->current_substream);
		handled =IRQ_HANDLED;
	}
	return handled;
}

static void stb7100_converter_unpause_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	writel((chip->pcmplayer_control|PCMP_ON), chip->pcm_player+STM_PCMP_CONTROL);
}

static void stb7100_converter_pause_playback(snd_pcm_substream_t *substream)
{
        pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	writel((chip->pcmplayer_control|PCMP_MUTE),chip->pcm_player+STM_PCMP_CONTROL);
}

static void stb7100_converter_stop_playback(snd_pcm_substream_t *substream)
{
 	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long reg=0;

	spin_lock(&chip->lock);

	reg = readl(chip->pcm_converter + AUD_SPDIF_PR_CFG) & ~PR_CFG_DEVICE_ENABLE;
	writel(reg, chip->pcm_converter + AUD_SPDIF_PR_CFG );

	reg = readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CTRL) & ~0x7L; /* mask bottom three bits */
	writel((reg|PR_CTRL_OFF), chip->pcm_converter+AUD_SPDIF_PR_SPDIF_CTRL);

	reset_converter_fifo(substream);

	writel(0         , chip->pcm_converter + AUD_SPDIF_PR_INT_EN);
	writel(0xffffffff, chip->pcm_converter + AUD_SPDIF_PR_INT_CLR);

	/*
	 * Stop PCM Player0 with mute, see the stm7100_pcm.c for an explanation
	 */
	writel((chip->pcmplayer_control|PCMP_MUTE),chip->pcm_player+STM_PCMP_CONTROL);

	spin_unlock(&chip->lock);
	dma_stop_channel(chip->fdma_channel);
	dma_free_descriptor(&chip->dmap);
}


static void stb7100_converter_start_playback(snd_pcm_substream_t *substream)
{
	pcm_hw_t     *chip = snd_pcm_substream_chip(substream);
	unsigned long cfg_reg;
	unsigned long ctrl_reg;
	int res = 0;
	/*
	 * We appear to need to reset the PCM player otherwise we end up
	 * with channel data sent to the wrong channels when starting up for
	 * the second time.
	 */
	stb7100_reset_pcm_player(chip);

	res=dma_xfer_list(chip->fdma_channel,&chip->dmap);
	if(res !=0)
		printk("%s FDMA_CH %d failed to start %d\n",__FUNCTION__,chip->fdma_channel,res);

	cfg_reg = readl(chip->pcm_converter + AUD_SPDIF_PR_CFG) ;
	ctrl_reg = readl(chip->pcm_converter +AUD_SPDIF_PR_SPDIF_CTRL) & ~0x7L; /* mask bottom three bits */

	writel(ctrl_reg | PR_CTRL_AUDIO_DATA_MODE,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CTRL);
	writel(cfg_reg  | PR_CFG_DEVICE_ENABLE, chip->pcm_converter + AUD_SPDIF_PR_CFG );

	writel((chip->pcmplayer_control | PCMP_ON), chip->pcm_player + STM_PCMP_CONTROL);

}

static int stb7100_converter_program_player(snd_pcm_substream_t * substream)
{
	unsigned long cfg_reg = 0;
	unsigned long ctl_reg = 0;
	unsigned long interrupt_list = (PR_INTERRUPT_ENABLE             |
					PR_SOFT_RESET_INT_ENABLE        |
					PR_AUDIO_SAMPLES_FULLY_READ_INT);
	unsigned long flags=0;

	snd_pcm_runtime_t * runtime = substream->runtime;
	pcm_hw_t          * chip    = snd_pcm_substream_chip(substream);
	int val =0;

	if(chip->fifo_check_mode)
		interrupt_list |= (PR_I2S_FIFO_OVERRUN_INT | PR_UNDERFLOW_INT);

	/*we only ever call from the stm7100_pcm program func,
	 * therefore we assume we already own the chip lock*/
	spin_lock_irqsave(&chip->lock,flags);
	/*
	 * Clear then enable the protocol converter interrupts.
	 */
	writel(0xffffffff,    chip->pcm_converter + AUD_SPDIF_PR_INT_CLR);
	writel(interrupt_list,chip->pcm_converter + AUD_SPDIF_PR_INT_EN);

	cfg_reg = (PR_CFG_FIFO_ENABLE | PR_CFG_REQ_ACK_ENABLE);

	if(runtime->format == SNDRV_PCM_FORMAT_S16_LE)
	  cfg_reg |= PR_CFG_WORD_SZ_16BIT;
	else
	  cfg_reg |= PR_CFG_WORD_SZ_24BIT;

	writel(cfg_reg, chip->pcm_converter + AUD_SPDIF_PR_CFG );

	/*
	 * Setup initial channel status data for the hardware mode and
	 * program for the new data burst.
	 */
	chip->pending_spdif_control = chip->default_spdif_control;
	iec60958_set_runtime_status(substream);
	chip->iec60958_output_count = 0;

	chip->current_spdif_control = chip->pending_spdif_control;
	stb7100_converter_write_channel_status(chip);

	val = 	chip->current_spdif_control.validity_l[0]        |
	 	(chip->current_spdif_control.validity_l[1] <<8)  |
	 	(chip->current_spdif_control.validity_l[2] <<16) |
	 	(chip->current_spdif_control.validity_l[3] <<24);
	/*TODO need a way to set up and expose the channel status/user & validity to the user*/
	writel(val,chip->pcm_converter + AUD_SPDIF_PR_VALIDITY);

	val = 	chip->current_spdif_control.user[0]        |
		(chip->current_spdif_control.user[1] <<8)  |
		(chip->current_spdif_control.user[2] <<16) |
		(chip->current_spdif_control.user[3] <<24);

	writel(val,chip->pcm_converter + AUD_SPDIF_PR_USER_DATA);

	/*
	 * These following writes refer to the IEC encoded mode - which is part
	 * of the converter block but not implemented in the instance of the
	 * 7100, so make sure it is all swithced off
	 */
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_PAUSE);
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_DATA_BURST);
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_PA_PB);
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_PC_PD);
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CL1);
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CR1);
	writel(0,chip->pcm_converter + AUD_SPDIF_PR_SPDIF_SUV);

	/*
	 * Setup the control register, but don't start it all of just yet.
	 */
	ctl_reg   = PR_CTRL_SW_STUFFING | PR_CTRL_16BIT_DATA_NOROUND | PR_CTRL_OFF;
	ctl_reg  |= SPDIF_FSYNTH_DIVIDE32_128;
        ctl_reg  |= ((runtime->period_size * 2) << PR_CTRL_SAMPLES_SHIFT);

	writel(ctl_reg, chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CTRL);
	spin_unlock_irqrestore(&chip->lock,flags);
	/*this reset will cause us to de-schedule, then well get an IRQ when
	 * the reset has completed, so make sure we dont hold any locks by now*/
	reset_pcm_converter(substream);
	return 0;
}


static int stb7100_converter_free(pcm_hw_t *card)
{
	writel(0, card->pcm_converter + AUD_SPDIF_PR_CFG );
	writel(0, card->pcm_converter + AUD_SPDIF_PR_SPDIF_CTRL);
	writel(PCMP_OFF, card->pcm_player + STM_PCMP_CONTROL);

	if(card->fdma_channel)
		free_dma(card->fdma_channel);

	iounmap(card->pcm_clock_reg);
	iounmap(card->out_pipe);
	iounmap(card->pcm_player);
	iounmap(card->pcm_converter);

	if(card->irq > 0)
		free_irq(card->irq,(void *)card);

	kfree(card);
	return 0;
}


static stm_playback_ops_t stb7100_converter_ops = {
	.free_device      = stb7100_converter_free,
	.open_device      = stb7100_pcm_open,
	.program_hw       = stb7100_pcm_program_hw,
	.playback_pointer = stb7100_fdma_playback_pointer,
	.start_playback   = stb7100_converter_start_playback,
	.stop_playback    = stb7100_converter_stop_playback,
	.pause_playback   = stb7100_converter_pause_playback,
	.unpause_playback = stb7100_converter_unpause_playback
};


static int stb7100_create_converter_device(pcm_hw_t *chip,snd_card_t  **this_card)
{
	int err = 0;
	int irq = linux_pcm_irq[chip->card_data->major];

	strcpy((*this_card)->shortname, "STb7100_CNV");
	strcpy((*this_card)->longname,  "STb7100_CNV");
	sprintf((*this_card)->driver,   "%d",chip->card_data->major);
        /*
         * In this case we need the base address of pcm0 for the player +
         * the base address of the IEC60958 device for the conversion block
         */
	chip->pcm_player    = ioremap(pcm_base_addr[0],0);
	chip->pcm_converter = ioremap(pcm_base_addr[chip->card_data->major],0);
	chip->hw            = stb7100_converter_hw;
	chip->oversampling_frequency = 128;

	chip->playback_ops  = &stb7100_converter_ops;

	if(request_irq(irq, stb7100_converter_interrupt, SA_INTERRUPT, "STB7100_CNV",(void*)chip)){
               		printk(">>> failed to get IRQ %d\n",irq);
	                stb7100_converter_free(chip);
        	        return -EBUSY;
        }

	chip->irq = irq;

	iec60958_default_channel_status(chip);

	/*
	 * For the converter device we rely on the PCM0 clock setup to drive
	 * the IEC block.
	 */
	set_default_device_clock(chip);

	stb7100_reset_pcm_player(chip);

	if((err = snd_card_pcm_allocate(chip,chip->card_data->minor,(*this_card)->longname)) < 0) {
        	printk(" >>> Failed to create PCM-SPDIF converter Stream\n");
        	stb7100_converter_free(chip);
    	}

    	if ((err = snd_iec60958_create_controls(chip)) < 0){
		stb7100_pcm_free(chip);
		return err;
	}
	if((err = snd_generic_create_controls(chip)) < 0){
		stb7100_pcm_free(chip);
		return err;
	}

	if((err = snd_device_new((*this_card), SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0){
        	printk(">>> creating sound device :%d,%d failed\n",chip->card_data->major,chip->card_data->minor);
        	stb7100_converter_free(chip);
		return err;
    	}

	if((err = snd_card_register((*this_card))) < 0){
        	stb7100_converter_free(chip);
		return err;
    	}

    	return 0;
}

