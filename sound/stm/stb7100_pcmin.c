/*
 *  STb710x Digitial PCM Reader Sound Driver
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/timer.h>

static snd_pcm_hardware_t stb7100_pcmin_hw =
{
	.info =		(SNDRV_PCM_INFO_MMAP           |
			 SNDRV_PCM_INFO_INTERLEAVED    |
			 SNDRV_PCM_INFO_BLOCK_TRANSFER |
			 SNDRV_PCM_INFO_MMAP_VALID),

	.formats =	SNDRV_PCM_FMTBIT_S32_LE,

	.rates =	(SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_96000 |
			 SNDRV_PCM_RATE_192000 ),

	.rate_min	  = 32000,
	.rate_max	  = 192000,
	.channels_min	  = 2,
	.channels_max	  = 2,
	.buffer_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,2),
	.period_bytes_min = FRAMES_TO_BYTES(1,2),
	.period_bytes_max = FRAMES_TO_BYTES(PCM_MAX_FRAMES,2),
	.periods_min	  = 1,
	.periods_max	  = PCM_MAX_FRAMES
};

void stb7100_reset_pcmin(snd_pcm_substream_t *substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	writel(1,chip->pcm_player);
	writel(0,chip->pcm_player);
}

static u32 get_target_time(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
	u32 period_samples = bytes_to_samples(runtime,frames_to_bytes(runtime,runtime->period_size))/ runtime->channels;
	u32 rate =runtime->rate;
	/*TODO :- we may suffer from rounding err for 44.1 case,
	 * but the 1ms overhead obviates the discrepency. */
	u32 period_data =  (rate / period_samples);
	u32 period_time = (PCMIN_MILLIS_PSEC / period_data) + PCMIN_TMR_OVRHD_MILLIS;
	return  ( jiffies + ((period_time  * HZ)/ PCMIN_MILLIS_PSEC)) ;
}

static void stb7100_pcmin_timer_irq(unsigned long handle)
{
	snd_pcm_substream_t *substream;
	pcm_hw_t          *chip;
	snd_pcm_runtime_t *runtime;
	u32 pos,irqflags;
	static u32 last_jiff;

	substream =(snd_pcm_substream_t *) handle;
	runtime = substream->runtime;
	chip     = snd_pcm_substream_chip(substream);

	spin_lock_irqsave(&chip->lock,irqflags);
	if(chip->pcmin.timer_halt){
		chip->pcmin.timer_halt=0;
		return;
	}

	pos = substream->runtime->dma_bytes - get_dma_residue(chip->fdma_channel);
	if(pos < chip->pcmin.last_fr)
		chip->pcmin.fr_delta = pos  + (snd_pcm_lib_buffer_bytes(substream) - chip->pcmin.last_fr);
	else
		chip->pcmin.fr_delta  += (pos - chip->pcmin.last_fr);

	chip->pcmin.last_fr = pos;

	if(chip->pcmin.fr_delta >=  frames_to_bytes(runtime,runtime->period_size)){
		snd_pcm_period_elapsed(substream);
		chip->pcmin.fr_delta=0;
	}
	else printk("%s Period Not elapsed\n 	Frame delta Actual %x expected %x\n	Timer delta Actual %d expected %d\n",
			__FUNCTION__,
			chip->pcmin.fr_delta,
			frames_to_bytes(runtime,runtime->period_size),
			((jiffies - last_jiff) *1000) /HZ,
			get_target_time(substream)-jiffies);

	/*wait for *about a sample period in time*/
	mod_timer(&chip->pcmin.period_timer,get_target_time(substream));
	last_jiff = jiffies;
	spin_unlock_irqrestore(&chip->lock,irqflags);
}

static void stb7100_pcmin_stop_read(snd_pcm_substream_t *substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long irqflags;


	if(chip->fifo_check_mode)
		writel(PCMIN_INT_OVF, chip->pcm_player + STM_PCMIN_ITS_EN_CLR);

	dma_stop_channel(chip->fdma_channel);
	dma_free_descriptor(&chip->dmap);

	spin_lock_irqsave(&chip->lock,irqflags);
	writel(AUD_PCMIN_CTRL_OFF_MODE,chip->pcm_player + STM_PCMIN_CTRL);
	stb7100_reset_pcmin(substream);
	chip->pcmin.timer_halt=1;
	spin_unlock_irqrestore(&chip->lock,irqflags);

}

static void stb7100_pcmin_start_read(snd_pcm_substream_t *substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long irqflags=0;
	int res = dma_xfer_list(chip->fdma_channel,&chip->dmap);
	if(res !=0)
		printk("%s FDMA_CH %d failed to start %d\n",__FUNCTION__,chip->fdma_channel,res);

	writel(chip->pcmplayer_control | AUD_PCMIN_CTRL_PCM_MODE,chip->pcm_player + STM_PCMIN_CTRL);
	stb7100_reset_pcmin(substream);

	if(chip->fifo_check_mode)
		writel(PCMIN_INT_OVF | PCMIN_INT_VSYNC	,chip->pcm_player + STM_PCMIN_ITS_EN);

	spin_lock_irqsave(&chip->lock,irqflags);

	chip->pcmin.fr_delta =0;
	chip->pcmin.last_fr =0;
	chip->pcmin.period_timer.data = (u32)substream;
	chip->pcmin.period_timer.function = &stb7100_pcmin_timer_irq;
	mod_timer(&(chip->pcmin.period_timer),get_target_time(substream));
	chip->pcmin.timer_halt=0;
	spin_unlock_irqrestore(&chip->lock,irqflags);
}



static irqreturn_t stb7100_pcmin_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long val;
	pcm_hw_t *stb7100 = dev_id;
	irqreturn_t res =IRQ_NONE;

	/* Read and clear interrupt status */
	spin_lock(&stb7100->lock);
	val =  readl(stb7100->pcm_player + STM_PCMIN_ITS);
	writel(val,stb7100->pcm_player + STM_PCMIN_ITS_CLR);
	spin_unlock(&stb7100->lock);

	if(unlikely(val & PCMIN_INT_OVF) == PCMIN_INT_OVF){
		printk("%s PCM Reader FIFO Overflow detected\n",__FUNCTION__);
		res=IRQ_HANDLED;
	}
	return res;
}

static int stb7100_pcmin_program_fdma(snd_pcm_substream_t *substream)
{
	pcm_hw_t          *chip    = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	unsigned long irqflags=0;
	int err=0;
	struct stm_dma_params dmap;

	if(!chip->out_pipe || ! chip->pcm_player)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock,irqflags);

	declare_dma_parms(	&dmap,
				MODE_PACED,
				STM_DMA_LIST_CIRC,
				STM_DMA_SETUP_CONTEXT_ISR,
				STM_DMA_NOBLOCK_MODE,
			       	(char*)STM_DMAC_ID);

	dma_parms_paced(&dmap,
			snd_pcm_lib_buffer_bytes(substream),
			chip->fdma_req);

	dma_parms_addrs(&dmap,
			virt_to_phys(chip->pcm_player+STM_PCMP_DATA_FIFO),
			runtime->dma_addr,
			snd_pcm_lib_buffer_bytes(substream));

	dma_compile_list(&dmap);
	chip->dmap = dmap;
	spin_unlock_irqrestore(&chip->lock,irqflags);
	return err;
}

static int stb7100_program_pcmin(snd_pcm_substream_t *substream)
{

	pcm_hw_t          *chip = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	unsigned long ctrlreg, fmtreg;

	/*The real SLCK format is to set data stable on falling edge*/
	fmtreg= AUD_PCMIN_FMT_ORDER_MSB | AUD_PCMIN_FMT_ALIGN_LR |
		AUD_PCMIN_FMT_PADDING_ON |  AUD_PCMIN_FMT_SLCK_EDGE_RISING |
		AUD_PCMIN_FMT_LR_POLARITY_HIGH | AUD_PCMIN_FMT_DATA_SZ_24 |
		AUD_PCMIN_FMT_NBIT_32 ;

	ctrlreg =  (runtime->period_size * runtime->channels) << AUD_PCMIN_CTRL_SAMPLES_SHIFT;
	ctrlreg = AUD_PCMIN_CTRL_DATA_ROUND | AUD_PCMIN_CTRL_MEM_FMT_16_0;
	writel(fmtreg,chip->pcm_player + STM_PCMIN_FMT	);
	chip->pcmplayer_control = ctrlreg;
	return 0;
}

static int stb7100_pcmin_program_hw(snd_pcm_substream_t *substream)
{
	int err=0;
	if((err = stb7100_program_pcmin(substream)) < 0)
		return err;

	if((err = stb7100_pcmin_program_fdma(substream)) < 0)
		return err;
	return 0;
}

static int stb7100_pcmin_open(snd_pcm_substream_t *substream)
{
	pcm_hw_t          *chip = snd_pcm_substream_chip(substream);
	int err=0;
	const char * dmac_id =STM_DMAC_ID;
	const char * lb_cap_channel = STM_DMA_CAP_LOW_BW;
	const char * hb_cap_channel = STM_DMA_CAP_HIGH_BW;

	if(chip->fdma_channel <0){
		err=request_dma_bycap(&dmac_id,&hb_cap_channel,"STB710x_PCMIN_DMA");
		if(err <0){
			err=request_dma_bycap(&dmac_id,&lb_cap_channel,	"STB710x_PCMIN_DMA");
			if(err <0){
				printk(" %s error in DMA request %d\n",__FUNCTION__,err);
				return err;
			}
		}
		chip->fdma_channel= err;
		init_timer(&chip->pcmin.period_timer);
		chip->pcmin.period_timer.data = (u32)substream;
		chip->pcmin.period_timer.function = &stb7100_pcmin_timer_irq;
		add_timer(&chip->pcmin.period_timer);
	}
	return 0;
}

static int stb7100_pcmin_free(pcm_hw_t *card)
{
	del_timer(&card->pcmin.period_timer);
	stb7100_pcm_free(card);
	return 0;
}


static stm_playback_ops_t stb7100_pcmin_ops = {
	.free_device      = stb7100_pcmin_free,
	.open_device      = stb7100_pcmin_open,
	.program_hw       = stb7100_pcmin_program_hw,

	.playback_pointer = stb7100_fdma_playback_pointer,
	.start_playback   = stb7100_pcmin_start_read,
	.stop_playback    = stb7100_pcmin_stop_read,
};

static struct platform_device *pcmin_platform_device;

static int __init stb710x_alsa_pcmin_probe(struct device *dev)
{
	pcmin_platform_device = to_platform_device(dev);
	return 0;
}

static struct device_driver alsa_pcmin_driver = {
	.name  = "710x_ALSA_PCMIN",
	.owner = THIS_MODULE,
	.bus   = &platform_bus_type,
	.probe = stb710x_alsa_pcmin_probe,
};

static struct device alsa_pcmin_device = {
	.bus_id="alsa_710x_pcmin",
	.driver = &alsa_pcmin_driver,
	.parent   = &platform_bus ,
	.bus      = &platform_bus_type,
};


static int __init snd_pcmin_stb710x_probe(pcm_hw_t *in_chip,snd_card_t *card,int dev)
{
	unsigned err=0;
	pcm_hw_t * chip={0};

	static snd_device_ops_t ops = {
    		.dev_free = snd_pcm_dev_free,
	};
	if(driver_register(&alsa_pcmin_driver)==0){
		if(device_register(&alsa_pcmin_device)!=0)
			return -ENOSYS;
	}
	else return -ENOSYS;

	if((chip = kcalloc(1,sizeof(pcm_hw_t), GFP_KERNEL)) == NULL)
        	return -ENOMEM;

	spin_lock_init(&chip->lock);
	chip->irq 		= -1;
	chip->fdma_channel 	= -1;

	chip->card         	= card;
	chip->card_data = &card_list[dev];

	chip->hw           = stb7100_pcmin_hw;
	chip->playback_ops  = &stb7100_pcmin_ops;

	chip->oversampling_frequency = 256;
	chip->pcm_clock_reg = ioremap(AUD_CFG_BASE, 0);
	chip->out_pipe      = ioremap(FDMA2_BASE_ADDRESS, 0);
	chip->pcm_player    = ioremap(PCMIN_BASE,0);

	if(request_irq(	LINUX_PCMREADER_ALLREAD_IRQ,
			stb7100_pcmin_interrupt,
			SA_INTERRUPT,
			"STB7100_PCMIN",
			(void*)chip)){

               	printk(">>> failed to get IRQ %d\n",LINUX_PCMREADER_ALLREAD_IRQ);
	        stb7100_pcm_free(chip);
        	return -EBUSY;
        }
	else chip->irq = LINUX_PCMREADER_ALLREAD_IRQ;

	if((err = snd_card_pcm_allocate(chip,chip->card_data->minor,card->longname)) < 0){
        	printk(">>> Failed to create PCM stream \n");
	        stb7100_pcm_free(chip);
	}

	if((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL,chip, &ops)) < 0){
		printk(">>> creating sound device :%d,%d failed\n",
			chip->card_data->major,chip->card_data->minor);
		stb7100_pcm_free(chip);
		return err;
	}

	if ((err = snd_card_register(card)) < 0) {
		printk("%s snd_card_registration() failed !\n",__FUNCTION__);
		stb7100_pcm_free(chip);
		return err;
	}
	if(register_platform_driver(	pcmin_platform_device,
					chip,
					card_list[PCMIN_DEVICE].major)!=0){

		printk("%s Error Registering PCM Reader\n",__FUNCTION__);
		return -ENODEV;
	}
	in_chip = chip;
	return 0;
}
