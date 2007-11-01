/*
 *  STPCM Player Sound Driver
 *  Copyright (c) 2005 STMicroelectronics Limited
 *  Authors: Stephen Gallimore <Stephen.Gallimore@st.com> and
 *  Mark Glaisher <Mark.Glaisher@st.com>
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

#include <sound/driver.h>
#include <asm/cpu/cacheflush.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/types.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>


#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#define SNDRV_GET_ID
#include <sound/initval.h>

#include <sound/asoundef.h>

#if defined(CONFIG_BIGPHYS_AREA)

#include <linux/bigphysarea.h>
#define STM_USE_BIGPHYS_AREA 1

#elif defined(CONFIG_BPA2)

#include <linux/bpa2.h>
#define STM_USE_BIGPHYS_AREA 1

#else

/* Private dummy defines so we do not have to ifdef the code */
static caddr_t  bigphysarea_alloc(int size) { return NULL; }
static void     bigphysarea_free(caddr_t addr, int size) {}
#define STM_USE_BIGPHYS_AREA 0

#endif /* CONFIG_BIGPHYS_AREA */


#include <asm/dma.h>
#include "st_pcm.h"

static int index[SNDRV_CARDS] = {SNDRV_DEFAULT_IDX1,SNDRV_DEFAULT_IDX1,SNDRV_DEFAULT_IDX1,SNDRV_DEFAULT_IDX1,SNDRV_DEFAULT_IDX1};
        	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = {SNDRV_DEFAULT_STR1,SNDRV_DEFAULT_STR1,SNDRV_DEFAULT_STR1,SNDRV_DEFAULT_STR1,SNDRV_DEFAULT_STR1};	/* ID for this card */

static u8 global_spdif_sync_status=0;

static int get_spdif_syncing_status(void)
{
	return global_spdif_sync_status;
}


void set_spdif_syncing_status(int enable)
{
	global_spdif_sync_status = enable;
}


#if defined (CONFIG_CPU_SUBTYPE_STB7100)

#define SND_DRV_CARDS  5

static stm_snd_output_device_t  card_list[SND_DRV_CARDS]= {
        /*major                      minor             input type          output type          */
        {PCM0_DEVICE,               MAIN_DEVICE, STM_DATA_TYPE_LPCM,     STM_DATA_TYPE_LPCM},
        {PCM1_DEVICE,               MAIN_DEVICE, STM_DATA_TYPE_LPCM,     STM_DATA_TYPE_LPCM},
        {SPDIF_DEVICE,              MAIN_DEVICE, STM_DATA_TYPE_IEC60958, STM_DATA_TYPE_IEC60958},
        {PROTOCOL_CONVERTER_DEVICE, MAIN_DEVICE, STM_DATA_TYPE_LPCM,     STM_DATA_TYPE_IEC60958},
        {PCM0_DEVICE,	   	    SUB_DEVICE1, STM_DATA_TYPE_I2S,	 STM_DATA_TYPE_LPCM}
};

#include "stb7100_snd.h"
#include "stm7100_pcm.c"
#include "stb7100_i2s_spdif.c"
#include "stb7100_spdif.c"
#include "stb7100_pcmin.c"
#define DEVICE_NAME "STb7100"

#else
	#error "BAD cpu arhitecture defined - PCM player is not supported"
#endif

MODULE_AUTHOR("Mark Glaisher <mark.glaisher@st.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DEVICE_NAME " ALSA driver");
MODULE_SUPPORTED_DEVICE("{{STM," DEVICE_NAME "}}");

static int snd_pcm_playback_hwfree(struct snd_pcm_substream * substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

        chip->card_data->in_use = 0;

	if(runtime->dma_area == NULL)
		return 0;

        if(STM_USE_BIGPHYS_AREA &&
           runtime->dma_bytes > PCM_BIGALLOC_SIZE) {

		bigphysarea_free(runtime->dma_area,
				 runtime->dma_bytes);

		runtime->dma_area    = 0;
		runtime->dma_addr    = 0;
		runtime->dma_bytes   = 0;

		return 0;
	}
	else
		return snd_pcm_lib_free_pages(substream);
}


static snd_pcm_uframes_t snd_pcm_playback_pointer(struct snd_pcm_substream * substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	return chip->playback_ops->playback_pointer(substream);
}


static int snd_pcm_playback_prepare(struct snd_pcm_substream * substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	unsigned long flags=0;
	/* Chip isn't running at this point so we don't have to disable interrupts*/
	spin_lock_irqsave(&chip->lock,flags);

#if defined (CONFIG_CPU_SUBTYPE_STB7100)
	/*
	 * On the STb7100 we can only use either the PCM0 device or the protocol
	 * converter as they physically use the same hardware. As we have no
	 * device specific hook for prepare, we do this here for the moment.
	 */
	if((card_list[PCM0_DEVICE].in_use               && (chip->card_data->major == PROTOCOL_CONVERTER_DEVICE)) ||
	   (card_list[PROTOCOL_CONVERTER_DEVICE].in_use && (chip->card_data->major == PCM0_DEVICE)))
	{
		int converter_enable;

		if(chip->card_data->minor == SUB_DEVICE1)
			goto setup;

		converter_enable = (chip->card_data->major==PROTOCOL_CONVERTER_DEVICE ? 1:0);
		printk("%s: device (%d,%d) is in use by (%d,%d)\n",
                	__FUNCTION__,
                	chip->card_data->major,
                	chip->card_data->minor,
                	(converter_enable ? 	PCM0_DEVICE:
                				PROTOCOL_CONVERTER_DEVICE),
                	(converter_enable ? 	card_list[PCM0_DEVICE].minor:
                				card_list[PROTOCOL_CONVERTER_DEVICE].minor));

        	return -EBUSY;
        }
#endif
setup:
	chip->card_data->in_use = 1;
	spin_unlock_irqrestore(&chip->lock,flags);

	if(chip->playback_ops->program_hw(substream) < 0)
		return -EIO;

	return 0;
}


static int snd_pcm_dev_free(struct snd_device *dev)
{
	pcm_hw_t *snd_card = dev->device_data;

	DEBUG_PRINT(("snd_pcm_dev_free(dev = 0x%08lx)\n",dev));
	DEBUG_PRINT((">>> snd_card = 0x%08lx\n",snd_card));

	if(snd_card->playback_ops->free_device)
		return snd_card->playback_ops->free_device(snd_card);

	return 0;
}


static int snd_playback_trigger(struct snd_pcm_substream * substream, int cmd)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	switch(cmd)
	{
		case SNDRV_PCM_TRIGGER_START:
			chip->playback_ops->start_playback(substream);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
			chip->playback_ops->stop_playback(substream);
		        chip->card_data->in_use = 0;
			break;
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			chip->playback_ops->pause_playback(substream);
			break;
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			chip->playback_ops->unpause_playback(substream);
			break;
		default:
			return -EINVAL;
	}
	snd_pcm_trigger_done(substream,substream);
	return 0;
}


static int snd_pcm_playback_close(struct snd_pcm_substream * substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);

	DEBUG_PRINT(("snd_pcm_playback_close(substream = 0x%08lx)\n",substream));
	DEBUG_PRINT((">>> chip = 0x%08lx\n",chip));

	/*
	 * If the PCM clocks are programmed then ensure the playback is
	 * stopped. If not do nothing otherwise we can end up with the
	 * DAC in a bad state.
	 */
	if(chip->are_clocks_active)
		chip->playback_ops->stop_playback(substream);

	spin_lock(&chip->lock);
	chip->current_substream = 0;
	spin_unlock(&chip->lock);

	return 0;
}


static int snd_pcm_playback_hwparams(struct snd_pcm_substream* substream,
					 struct snd_pcm_hw_params* hw_params)
{
	int   err  = 0;
	int   size = 0;
	char* addr = 0;

	struct snd_pcm_runtime *runtime = substream->runtime;
	size = params_buffer_bytes(hw_params);

	if (STM_USE_BIGPHYS_AREA && size > PCM_BIGALLOC_SIZE){
		/*
		 * This routine can be called multiple times without a free
		 * in between, so we need to make sure we don't overallocate.
		 */
		if(runtime->dma_area) {
			if(runtime->dma_bytes >= size) {
				err = 0; /* Not changed */
				goto exit;
			}
			else {
				/* Use this to make sure we do the right free */
				snd_pcm_playback_hwfree(substream);
			}
		}

		addr = bigphysarea_alloc(size);

		if(addr == 0) {
			printk(KERN_WARNING "ALSA driver: sound buffer allocation from bigphysmem failed.\n");
			printk(KERN_WARNING "ALSA driver: either increase bigphysmem pages with 'bigphyspages=xxxx' on the kernel command line\n");
			printk(KERN_WARNING "ALSA driver: or reduce the requested buffer size to <=128k (3276 audio frames)\n");
			err = -ENOMEM;
			goto exit;
		}
		else{
			dma_cache_wback(&addr, size);
			runtime->dma_area    = addr;
			runtime->dma_addr    = virt_to_phys(addr);
			runtime->dma_bytes   = size;
			err = 1; /* Changed buffer */
		}
	}
	else {
		err = snd_pcm_lib_malloc_pages(substream, size);
		if(err >= 0) {
			runtime->dma_addr = virt_to_phys(runtime->dma_area);
		}
	}

exit:
	DEBUG_PRINT((">>> dma_area = 0x%08lx err = %d\n",substream->runtime->dma_area, err));

	return err;
}


/*
 * This is a constraint rule which limits the period size to the capabilities
 * of the ST PCM Players. These only have a 19bit count register which
 * counts individual samples, i.e. for a 10-channel player it will count 10
 * for each alsa 10 channel frame. This means we also need to ensure that
 * the number of samples is an exact multiple of the number of channels.
 */
static int snd_pcm_period_size_rule(struct snd_pcm_hw_params *params,
				     struct snd_pcm_hw_rule *rule)
{
	struct snd_interval *periodsize;
	struct snd_interval *channels;
	struct snd_interval  newperiodsize;

	int refine = 0;

	periodsize    = hw_param_interval(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
	newperiodsize = *periodsize;
	channels      = hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	if((periodsize->max * channels->min) > PCMP_MAX_SAMPLES) {
		newperiodsize.max = PCMP_MAX_SAMPLES / channels->min;
		refine = 1;
	}

	if((periodsize->min * channels->min) > PCMP_MAX_SAMPLES) {
		newperiodsize.min = PCMP_MAX_SAMPLES / channels->min;
		refine = 1;
	}

	if(refine) {
		DEBUG_PRINT(("snd_pcm_period_size_rule: refining (%d,%d) to (%d,%d)\n",periodsize->min,periodsize->max,newperiodsize.min,newperiodsize.max));
		return snd_interval_refine(periodsize, &newperiodsize);
	}

	return 0;
}


static int snd_pcm_playback_open(struct snd_pcm_substream * substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);
	int err = 0;

	DEBUG_PRINT(("snd_pcm_playback_open(substream = 0x%08lx)\n",substream));
	DEBUG_PRINT((">>> chip = 0x%08lx\n",chip));

	snd_pcm_set_sync(substream);

	snd_pcm_hw_rule_add(substream->runtime,
			    0, SNDRV_PCM_HW_PARAM_CHANNELS,
			    snd_pcm_period_size_rule,
			    0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			    -1);

	snd_pcm_hw_rule_add(substream->runtime,
			    0, SNDRV_PCM_HW_PARAM_PERIOD_TIME,
			    snd_pcm_period_size_rule,
			    0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			    -1);

	snd_pcm_hw_rule_add(substream->runtime,
			    0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			    snd_pcm_period_size_rule,
			    0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			    -1);

	spin_lock(&chip->lock);

	chip->current_substream = substream;
        runtime->hw = chip->hw;

        if(chip->playback_ops->open_device)
		err = chip->playback_ops->open_device(substream);

	spin_unlock(&chip->lock);

	return err;
}

/*
 * nopage callback for mmapping a RAM page
 */

static struct page *snd_pcm_mmap_data_nopage(struct vm_area_struct *area, unsigned long address, int *type)
{
        struct snd_pcm_substream *substream = (struct snd_pcm_substream *)area->vm_private_data;
        struct snd_pcm_runtime *runtime;
        unsigned long offset;
        struct page * page;
        void *vaddr;
        size_t dma_bytes;

        if (substream == NULL)
                return NOPAGE_OOM;
        runtime = substream->runtime;
        offset = area->vm_pgoff << PAGE_SHIFT;
        offset += address - area->vm_start;
        snd_assert((offset % PAGE_SIZE) == 0, return NOPAGE_OOM);
        dma_bytes = PAGE_ALIGN(runtime->dma_bytes);
        if (offset > dma_bytes - PAGE_SIZE)
                return NOPAGE_SIGBUS;

        if (substream->ops->page) {
                page = substream->ops->page(substream, offset);
                if (! page)
                        return NOPAGE_OOM;
        } else {
                vaddr = runtime->dma_area + offset;
                page = virt_to_page(vaddr);
        }
        get_page(page);
        if (type)
                *type = VM_FAULT_MINOR;
        return page;
}


static struct vm_operations_struct snd_pcm_vm_ops_data =
{
        .open =         snd_pcm_mmap_data_open,
        .close =        snd_pcm_mmap_data_close,
        .nopage =       snd_pcm_mmap_data_nopage,
};

/*
 * mmap the DMA buffer on RAM
 */

static int snd_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *area)
{
        area->vm_ops = &snd_pcm_vm_ops_data;
        area->vm_private_data = substream;
        area->vm_flags |= VM_RESERVED;

        area->vm_page_prot = pgprot_noncached(area->vm_page_prot);

        atomic_inc(&substream->mmap_count);
        return 0;
}


static int snd_pcm_silence(struct snd_pcm_substream *substream, int channel,
                            snd_pcm_uframes_t    pos,       snd_pcm_uframes_t count)
{
        struct snd_pcm_runtime *runtime = substream->runtime;
        char *hwbuf;
	int   totalbytes;

        if(channel != -1)
                return -EINVAL;

        hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);

	totalbytes = frames_to_bytes(runtime, count);

        snd_pcm_format_set_silence(runtime->format, hwbuf, totalbytes);
        dma_cache_wback(hwbuf, totalbytes);
        return 0;
}


static int snd_pcm_copy(struct snd_pcm_substream	*substream,
			 int			 channel,
			 snd_pcm_uframes_t	 pos,
			 void __user		*buf,
			 snd_pcm_uframes_t	 count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char		  *hwbuf;
	int                totalbytes;

	if(channel != -1)
		return -EINVAL;

	hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);

	totalbytes = frames_to_bytes(runtime, count);

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){

		if(copy_from_user(hwbuf, buf, totalbytes))
			return -EFAULT;

		dma_cache_wback(hwbuf, totalbytes);
	}
	else{
		dma_cache_inv(hwbuf,totalbytes);

		if(copy_to_user(buf,hwbuf,totalbytes))
			return -EFAULT;
	}
	return 0;
}

/*
 * IEC60958 channel status and format handling for SPDIF and I2S->SPDIF
 * protocol converters
 */
void iec60958_default_channel_status(pcm_hw_t *chip)
{
	chip->default_spdif_control.channel.status[0]  = (IEC958_AES0_CON_NOT_COPYRIGHT |
							  IEC958_AES0_CON_EMPHASIS_NONE);

	chip->default_spdif_control.channel.status[1] |= (IEC958_AES1_CON_NON_IEC908_DVD |
							  IEC958_AES1_CON_ORIGINAL) ;


	chip->default_spdif_control.channel.status[2] |= (IEC958_AES2_CON_SOURCE_UNSPEC |
							  IEC958_AES2_CON_CHANNEL_UNSPEC);

	chip->default_spdif_control.channel.status[3] |= (IEC958_AES3_CON_FS_44100 |
							  IEC958_AES3_CON_CLOCK_VARIABLE);

	chip->default_spdif_control.channel.status[4]  = (IEC958_AES4_CON_WORDLEN_MAX_24 |
							  IEC958_AES4_CON_WORDLEN_24_20);

	memset(chip->default_spdif_control.user,      0x0,sizeof(u8)*48);
	memset(chip->default_spdif_control.validity_l,0x0,sizeof(u8)*24);
	memset(chip->default_spdif_control.validity_r,0x0,sizeof(u8)*24);
}


void iec60958_set_runtime_status(struct snd_pcm_substream *substream)
{
	pcm_hw_t *chip = snd_pcm_substream_chip(substream);

	if(chip->pending_spdif_control.channel.status[0] & IEC958_AES0_PROFESSIONAL) {
		chip->pending_spdif_control.channel.status[0] &= ~IEC958_AES0_PRO_FS;
		switch(substream->runtime->rate){
			case 32000:
				chip->pending_spdif_control.channel.status[0]
					 |= IEC958_AES0_PRO_FS_32000;
				break;
			case 48000:
				chip->pending_spdif_control.channel.status[0]
					 |= IEC958_AES0_PRO_FS_48000;
				break;
			default:
				chip->pending_spdif_control.channel.status[0]
					 |= IEC958_AES0_PRO_FS_44100;
				break;
		}

		chip->pending_spdif_control.channel.status[2]
			&= ~((IEC958_AES2_PRO_SBITS | IEC958_AES2_PRO_WORDLEN));

		chip->pending_spdif_control.channel.status[2]
			|= (IEC958_AES2_PRO_SBITS_24 | IEC958_AES2_PRO_WORDLEN_24_20);

		chip->pending_spdif_control.channel.status[4] = 0;

	} else {
		chip->pending_spdif_control.channel.status[3] &=
				 ~((IEC958_AES3_CON_FS|IEC958_AES3_CON_CLOCK));
		switch(substream->runtime->rate){
			case 32000:
				chip->pending_spdif_control.channel.status[3]
					|= IEC958_AES3_CON_FS_32000;
				break;
			case 48000:
				chip->pending_spdif_control.channel.status[3]
					 |= IEC958_AES3_CON_FS_48000;
				break;
			default:
				chip->pending_spdif_control.channel.status[3]
					 |= IEC958_AES3_CON_FS_44100;
				break;
		}

		chip->pending_spdif_control.channel.status[3] |= IEC958_AES3_CON_CLOCK_VARIABLE;

		if(chip->pending_spdif_control.channel.status[0] & IEC958_AES0_NONAUDIO) {
			DEBUG_PRINT(("iec60958_set_runtime_status: NON LPCM Setup\n",dev));
			chip->pending_spdif_control.channel.status[4] = 0;
			/*
			 * Force all validity bits to 1 as specified in the spec
			 * to prevent accidental interpretation as LPCM.
			 */
			memset(chip->default_spdif_control.validity_l,0xff,sizeof(u8)*24);
			memset(chip->default_spdif_control.validity_r,0xff,sizeof(u8)*24);
		} else {
			DEBUG_PRINT(("iec60958_set_runtime_status: 24bit LPCM Setup\n",dev));
			chip->pending_spdif_control.channel.status[4]  = (IEC958_AES4_CON_WORDLEN_MAX_24 |
									  IEC958_AES4_CON_WORDLEN_24_20);
		}
	}

}


static int snd_iec60958_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}


static int snd_iec60958_default_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pcm_hw_t *chip = snd_kcontrol_chip(kcontrol);

	ucontrol->value.iec958.status[0] = chip->default_spdif_control.channel.status[0];
	ucontrol->value.iec958.status[1] = chip->default_spdif_control.channel.status[1];
	ucontrol->value.iec958.status[2] = chip->default_spdif_control.channel.status[2];
	ucontrol->value.iec958.status[3] = chip->default_spdif_control.channel.status[3];

	return 0;
}


static int snd_iec60958_default_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pcm_hw_t *chip = snd_kcontrol_chip(kcontrol);
	u32 val, old;

	val =  ucontrol->value.iec958.status[0]        |
	      (ucontrol->value.iec958.status[1] << 8)  |
	      (ucontrol->value.iec958.status[2] << 16) |
	      (ucontrol->value.iec958.status[3] << 24);

	old =  chip->default_spdif_control.channel.status[0] 	    |
	      (chip->default_spdif_control.channel.status[1] << 8)  |
	      (chip->default_spdif_control.channel.status[2] << 16) |
	      (chip->default_spdif_control.channel.status[3] << 24);

	if(val == old)
		return 0;

	spin_lock_irq(&chip->lock);
	chip->default_spdif_control.channel = ucontrol->value.iec958;
	spin_unlock_irq(&chip->lock);
	return (val != old);
}


static struct snd_kcontrol_new snd_iec60958_default __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,DEFAULT),
	.info =		snd_iec60958_info,
	.get =		snd_iec60958_default_get,
	.put =		snd_iec60958_default_put
};


static struct snd_kcontrol_new snd_iec60958_stream __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,PCM_STREAM),
	.info =		snd_iec60958_info,
	.get =		snd_iec60958_default_get,
	.put =		snd_iec60958_default_put
};

static int snd_iec60958_maskc_get(struct snd_kcontrol * kcontrol,
				  struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_NONAUDIO          |
					   IEC958_AES0_PROFESSIONAL      |
					   IEC958_AES0_CON_NOT_COPYRIGHT |
					   IEC958_AES0_CON_EMPHASIS;

	ucontrol->value.iec958.status[1] = IEC958_AES1_CON_ORIGINAL |
					   IEC958_AES1_CON_CATEGORY;

	ucontrol->value.iec958.status[2] = 0;

	ucontrol->value.iec958.status[3] = 0;
	return 0;
}


static int snd_iec60958_maskp_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_NONAUDIO     |
					   IEC958_AES0_PROFESSIONAL |
					   IEC958_AES0_PRO_EMPHASIS;

	ucontrol->value.iec958.status[1] = IEC958_AES1_PRO_MODE |
					   IEC958_AES1_PRO_USERBITS;

	return 0;
}


static struct snd_kcontrol_new snd_iec60958_maskc __devinitdata =
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,CON_MASK),
	.info =		snd_iec60958_info,
	.get =		snd_iec60958_maskc_get,
};


static struct snd_kcontrol_new snd_iec60958_mask __devinitdata =
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,MASK),
	.info =		snd_iec60958_info,
	.get =		snd_iec60958_maskc_get,
};


static struct snd_kcontrol_new snd_iec60958_maskp __devinitdata =
{
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =         SNDRV_CTL_NAME_IEC958("",PLAYBACK,PRO_MASK),
	.info =		snd_iec60958_info,
	.get =		snd_iec60958_maskp_get,
};


static int snd_iec60958_raw_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info * uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}


static int snd_iec60958_raw_get(struct snd_kcontrol * kcontrol, struct snd_ctl_elem_value * ucontrol)
{
	pcm_hw_t *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = chip->iec60958_rawmode;
	return 0;
}


static int snd_iec60958_raw_put(struct snd_kcontrol * kcontrol, struct snd_ctl_elem_value * ucontrol)
{
	pcm_hw_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned char old, val;

	spin_lock_irq(&chip->lock);
	old = chip->iec60958_rawmode;
	val = ucontrol->value.integer.value[0];
	chip->iec60958_rawmode = val;
	spin_unlock_irq(&chip->lock);
	return old != val;
}


static struct snd_kcontrol_new snd_iec60958_raw __devinitdata = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "RAW",
	.info =		snd_iec60958_raw_info,
	.get =		snd_iec60958_raw_get,
	.put =		snd_iec60958_raw_put
};


static int snd_iec60958_sync_get(struct snd_kcontrol * kcontrol, struct snd_ctl_elem_value * ucontrol)
{
	ucontrol->value.integer.value[0] = global_spdif_sync_status;
	return 0;
}


static int snd_iec60958_sync_put(struct snd_kcontrol * kcontrol, struct snd_ctl_elem_value * ucontrol)
{

	unsigned char old, val;

	old = get_spdif_syncing_status();
	val = ucontrol->value.integer.value[0];
	set_spdif_syncing_status(val);
	return old != val;
}


static struct snd_kcontrol_new snd_iec60958_sync __devinitdata = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "PCM Sync",
	.info =		snd_iec60958_raw_info, /* Reuse from the RAW switch */
	.get =		snd_iec60958_sync_get,
	.put =		snd_iec60958_sync_put
};


/*IEC61937 encoding mode status -  when transmitting a surround encoded data
 * stream the repitition period of iec61937 pause bursts and external decode latency
 *  are dependant on stream type*/


typedef struct iec_encoding_mode_tbl {
	char name[30];
	iec_encodings_t id_flag;
}iec_encoding_mode_tbl_t;

static iec_encoding_mode_tbl_t iec_xfer_modes[12]=
	{
		{"IEC60958"	,ENCODING_IEC60958},
		{"IEC61937_AC3"	,ENCODING_IEC61937_AC3},
		{"IEC61937_DTS1",ENCODING_IEC61937_DTS_1},
		{"IEC61937_DTS2",ENCODING_IEC61937_DTS_2},
		{"IEC61937_DTS3",ENCODING_IEC61937_DTS_3},
		{"IEC61937_MPEG_384",ENCODING_IEC61937_MPEG_384_FRAME},
		{"IEC61937_MPEG_1152",ENCODING_IEC61937_MPEG_1152_FRAME},
		{"IEC61937_MPEG_1024",ENCODING_IEC61937_MPEG_1024_FRAME},
		{"IEC61937_MPEG_2304",ENCODING_IEC61937_MPEG_2304_FRAME},
		{"IEC61937_MPEG_768",ENCODING_IEC61937_MPEG_768_FRAME},
		{"IEC61937_MPEG_2304_LSF",ENCODING_IEC61937_MPEG_2304_FRAME_LSF},
		{"IEC61937_MPEG_768_LSF",ENCODING_IEC61937_MPEG_768_FRAME_LSF},
	};



static int snd_iec_encoding_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info * uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = ENCODED_STREAM_TYPES;
	if (uinfo->value.enumerated.item > (ENCODED_STREAM_TYPES-1))
		uinfo->value.enumerated.item = (ENCODED_STREAM_TYPES);
	strcpy(uinfo->value.enumerated.name,iec_xfer_modes[uinfo->value.enumerated.item].name);

	return 0;
}

static int snd_iec_encoding_get(struct snd_kcontrol* kcontrol,struct snd_ctl_elem_value* ucontrol)
{
	int i;

	for(i=0; i< ENCODED_STREAM_TYPES; i++)
		ucontrol->value.integer.value[i] = iec_xfer_modes[i].id_flag;

	return 0;
}

static int snd_iec_encoding_put(	 struct snd_kcontrol * kcontrol,
					 struct snd_ctl_elem_value * ucontrol)
{
	pcm_hw_t *chip = snd_kcontrol_chip(kcontrol);
	spin_lock_irq(&chip->lock);
	chip->iec_encoding_mode = ucontrol->value.integer.value[0];
	spin_unlock_irq(&chip->lock);

	return 0;
}

static struct snd_kcontrol_new snd_iec_encoding __devinitdata = {
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE)"Encoding",
	.info =		snd_iec_encoding_info,
	.get =		snd_iec_encoding_get,
	.put =		snd_iec_encoding_put,
};

static int snd_clock_put(struct snd_kcontrol * kcontrol,struct snd_ctl_elem_value * ucontrol)
{

	pcm_hw_t *chip = snd_kcontrol_chip(kcontrol);

	int direction = (((int)ucontrol->value.integer.value[1]) > 0) ? 1:0;
	int adjusts=ucontrol->value.integer.value[0];

	spin_lock_irq(&chip->lock);
	adjust_audio_clock(chip->current_substream,adjusts,direction);

	spin_unlock_irq(&chip->lock);

	return 0;
}



static int snd_clock_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info * uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = -10000;
	uinfo->value.integer.max = 10000;

	return 0;
}

static struct snd_kcontrol_new snd_clock_adjust  __devinitdata = {
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"PLAYBACK Clock Adjust",
	.info =		snd_clock_info,
	.put =		snd_clock_put,
};

/*now three controls to specify the available encoding modes */

static int __devinit snd_generic_create_controls(pcm_hw_t *chip)
{
	int err;
	struct snd_kcontrol *kctl;

	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_clock_adjust,chip));
	if(err < 0)
		return err;

	return 0;
}


static int __devinit snd_iec60958_create_controls(pcm_hw_t *chip)
{
	int err;
	struct snd_kcontrol *kctl;

	if(chip->card_data->input_type == STM_DATA_TYPE_IEC60958)
	{
		err = snd_ctl_add(chip->card, snd_ctl_new1(&snd_iec60958_raw, chip));
		if (err < 0)
			return err;

		err = snd_ctl_add(chip->card, snd_ctl_new1(&snd_iec60958_sync, chip));
		if (err < 0)
			return err;
	}

	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_iec60958_default, chip));
	if (err < 0)
		return err;

	/*
	 * stream is a copy of default for the moment for application
	 * compatibility, more investigation required
	 */
	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_iec60958_stream, chip));
	if (err < 0)
		return err;

	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_iec60958_maskc, chip));
	if (err < 0)
		return err;

	/*
	 * Mask is a copy of the consumer mask.
	 */
	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_iec60958_mask, chip));
	if (err < 0)
		return err;

	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_iec60958_maskp, chip));
	if (err < 0)
		return err;

	err = snd_ctl_add(chip->card, kctl = snd_ctl_new1(&snd_iec_encoding,chip));
	if(err < 0)
		return err;

	return 0;
}


static void format_iec60958_frame(struct snd_pcm_substream *substream,
				  u32                 *left_subframe,
				  u32                 *right_subframe)
{
	/*
	 * Format for the SPDIF player. Note that the ordering of CUV is
	 * the reverse of that specified in the SPDIF player specification,
	 * but is correct with regards to the ST reference driver documentation.
	 *
     	 * 31 ..16 bit data16| 15 ext data 8 | 7 zero  4 | 3 ctrl bits 0
    	 * |xxxxxxxxxxxxxxxx | ????????        0000      | VUC0
    	 */
	static const u32 channel_status_bit = (1 << 1);
	static const u32 user_bit           = (1 << 2);
	static const u32 validity_bit       = (1 << 3);

        pcm_hw_t   *chip       = snd_pcm_substream_chip(substream);

        u8 *channel_status = &chip->current_spdif_control.channel.status[0];
	u8 *user           = &chip->current_spdif_control.user[0];
        u8 *validity_l     = &chip->current_spdif_control.validity_l[0];
        u8 *validity_r     = &chip->current_spdif_control.validity_r[0];
	/*
	 * Index and test bit for channel status and validity
	 */
        int word_index      = chip->iec60958_output_count/8;
        u32 test_bit        = 1 << (chip->iec60958_output_count%8);
        /*
         * Index and test bits for the user bits, which are contiguous across
         * L/R subframes.
         */
        int user_word_index = (chip->iec60958_output_count*2)/8;
	u32 u_test_bit_l    = 1 << (chip->iec60958_output_count*2)%8;
	u32 u_test_bit_r    = u_test_bit_l << 1;
	u32 format_word_l   = 0;
	u32 format_word_r   = 0;

	if(chip->iec60958_output_count == 0) {
		/*
		 * Start of a new burst, so update the control bits
		 */
		chip->current_spdif_control = chip->pending_spdif_control;
	}

#if defined(CONFIG_STB7100_IEC_DEBUG)
	static int print_debug;
	if(chip->iec60958_output_count == 0){
		if(*left_subframe == 0xf8720000 && *right_subframe == 0x4e1f0000)
			print_debug = 1;
	}

	if(chip->iec60958_output_count == 8) {
		print_debug = 0;
	}

	if(print_debug) {
	  	printk("%03d: in(0x%08x,0x%08x) ",chip->iec60958_output_count,*left_subframe,*right_subframe);
	}
#endif
    	/*
    	 * channel status is only ever 35 bits long , so we can ingnore the
	 * remaining 157 frames
	 */
    	if((word_index <5) && (channel_status[word_index] & test_bit))
    	{
    		format_word_l |= channel_status_bit;
    		format_word_r |= channel_status_bit;
    	}

	if(user[user_word_index] & u_test_bit_l)
		format_word_l |= user_bit;

	if(user[user_word_index] & u_test_bit_r)
		format_word_r |= user_bit;

	if(validity_l[word_index] & test_bit)
		format_word_l |= validity_bit;

	if(validity_r[word_index] & test_bit)
		format_word_r |= validity_bit;

	*left_subframe  = (*left_subframe  & 0xffffff00) | format_word_l;
	*right_subframe = (*right_subframe & 0xffffff00) | format_word_r;

#if defined(CONFIG_STB7100_IEC_DEBUG)
	if(print_debug) {
		printk("%03d: out(0x%08x,0x%08x)\n",chip->iec60958_output_count,*left_subframe,*right_subframe);
	}
#endif
	chip->iec60958_output_count = (chip->iec60958_output_count+1)%192;
}

/*
 * Internal function which can be called independently by other modules
 * to get IEC60958 formatting. Note the interface is slightly manipulated
 * to allow channels to be skipped in the buffer.
 */
int snd_pcm_format_iec60958_copy(struct snd_pcm_substream	*substream,
				 int			data_channels,
			 	 snd_pcm_uframes_t	pos,
			 	 void	__user		*buffer,
			 	 snd_pcm_uframes_t	count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	u32 __user        *buf32   = (u32 __user *) buffer;
	int i;

	/*
	 * Note that runtime->channels must be 2 for an SPDIF device
	 * which maps to the two subframes in an SPDIF frame. Each subframe
	 * is 32bits, so each "output frame" is 8 bytes (2x4).
	 */
	int dstwidth  = sizeof(u32)*2;
	int srcwidth  = samples_to_bytes(runtime, data_channels);
	int bit_width = snd_pcm_format_physical_width(substream->runtime->format);

        u32 *hwbuf    = (u32*)(runtime->dma_area + (pos*dstwidth));

	if(!access_ok(VERIFY_READ, buffer, (count * srcwidth)))
		return -EFAULT;


	for(i=0;i<count;i++)
	{
		u32 left_subframe;
		u32 right_subframe;

		__get_user(left_subframe, buf32);
		__get_user(right_subframe, buf32+1);

		if(bit_width == 24)
		{
		  /*
		   * We can support S24_LE (24bits in the bottom 3bytes of
		   * a 32bit word) by shifting the audio bits into position
		   */
		  left_subframe  <<= 8;
		  right_subframe <<= 8;
		}

		format_iec60958_frame(substream, &left_subframe, &right_subframe);

		*hwbuf     = left_subframe;
		*(hwbuf+1) = right_subframe;

		buf32  += data_channels;
	    	hwbuf  += 2;
	}

	dma_cache_wback((void*)(runtime->dma_area + (pos*dstwidth)), (count*dstwidth));

	return 0;
}


/*
 * This is the ALSA interface for the card "ops" structure
 */
static int snd_iec60958_silence(struct snd_pcm_substream *substream,
				int                  channel,
				snd_pcm_uframes_t    pos,
				snd_pcm_uframes_t    count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	u32               *hwbuf;
	static const int dstwidth  = sizeof(u32)*2;
	int i;

        if(channel != -1)
                return -EINVAL;

        hwbuf = (u32*)(runtime->dma_area + (pos*dstwidth));

	for(i=0;i<count;i++)
	{
		u32 left_subframe  = 0;
		u32 right_subframe = 0;

		format_iec60958_frame(substream, &left_subframe, &right_subframe);

		*hwbuf     = left_subframe;
		*(hwbuf+1) = right_subframe;

	    	hwbuf += 2;
	}

	dma_cache_wback((void*)(runtime->dma_area + (pos*dstwidth)), (count*dstwidth));

        return 0;
}


static int snd_iec60958_copy(struct snd_pcm_substream  *substream,
			     int                   channel,
			     snd_pcm_uframes_t     pos,
			     void __user          *buf,
			     snd_pcm_uframes_t     count)
{
        pcm_hw_t *chip = snd_pcm_substream_chip(substream);

	if(channel != -1)
		return -EINVAL;

	if(chip->iec60958_rawmode)
		return snd_pcm_copy(substream,channel,pos,buf,count);
	else
		return snd_pcm_format_iec60958_copy(substream,substream->runtime->channels,pos,buf,count);
}


static void snd_card_pcm_free(struct snd_pcm *pcm)
{
	DEBUG_PRINT(("snd_card_pcm_free(pcm = 0x%08lx)\n",pcm));

	snd_pcm_lib_preallocate_free_for_all(pcm);
}


static struct snd_pcm_ops  snd_card_playback_ops_pcm = {
	.open      =            snd_pcm_playback_open,
        .close     =            snd_pcm_playback_close,
        .mmap      =            snd_pcm_mmap,
        .silence   =            snd_pcm_silence,
	.copy      =            snd_pcm_copy,
        .ioctl     =            snd_pcm_lib_ioctl,
        .hw_params =            snd_pcm_playback_hwparams,
        .hw_free   =            snd_pcm_playback_hwfree,
        .prepare   =            snd_pcm_playback_prepare,
        .trigger   =            snd_playback_trigger,
        .pointer   =            snd_pcm_playback_pointer,
};


static struct snd_pcm_ops  snd_card_playback_ops_iec60958 = {
	.open      =            snd_pcm_playback_open,
        .close     =            snd_pcm_playback_close,
        .mmap      =            snd_pcm_mmap,
        .silence   =            snd_iec60958_silence,
	.copy      =            snd_iec60958_copy,
        .ioctl     =            snd_pcm_lib_ioctl,
        .hw_params =            snd_pcm_playback_hwparams,
        .hw_free   =            snd_pcm_playback_hwfree,
        .prepare   =            snd_pcm_playback_prepare,
        .trigger   =            snd_playback_trigger,
        .pointer   =            snd_pcm_playback_pointer,
};


static int __devinit snd_card_pcm_allocate(pcm_hw_t *snd_card, int device,char* name)
{
	int err;
	struct snd_pcm *pcm;

	if(snd_card->card_data->input_type == STM_DATA_TYPE_IEC60958){

		err = snd_pcm_new(snd_card->card,name, snd_card->card_data->minor,1, 0, &pcm);
		snd_pcm_set_ops(pcm,SNDRV_PCM_STREAM_PLAYBACK,&snd_card_playback_ops_iec60958);
	}
	else if(snd_card->card_data->major == PCM0_DEVICE &&
		snd_card->card_data->minor == SUB_DEVICE1){
			err = snd_pcm_new(snd_card->card,name,snd_card->card_data->minor,0,1 , &pcm);
			snd_pcm_set_ops(pcm,SNDRV_PCM_STREAM_CAPTURE,&snd_card_playback_ops_pcm);
	}
	else{
		err = snd_pcm_new(snd_card->card,name, snd_card->card_data->minor,1, 0, &pcm);
		snd_pcm_set_ops(pcm,SNDRV_PCM_STREAM_PLAYBACK,&snd_card_playback_ops_pcm);
	}

	if (err < 0)
		return err;

	pcm->private_data = snd_card;
	pcm->private_free = snd_card_pcm_free;
	pcm->info_flags   = 0;
	strcpy(pcm->name, name);

	snd_pcm_lib_preallocate_pages_for_all(pcm,
					SNDRV_DMA_TYPE_CONTINUOUS,
					snd_dma_continuous_data(GFP_KERNEL),
					PCM_PREALLOC_SIZE,
					PCM_PREALLOC_MAX);
	return 0;
}

static int register_platform_driver(struct platform_device *platform_dev,pcm_hw_t *chip, int dev_nr)
{
	static struct resource *res;
	if (!platform_dev){
       		printk("%s Failed. Check your kernel SoC config\n",__FUNCTION__);
         	return -EINVAL;
       	}

	res = platform_get_resource(platform_dev, IORESOURCE_IRQ,0);    /*resource 0 */
	if(res!=NULL){
		chip->min_ch = res->start;
		chip->max_ch = res->end;
	}
	else return -ENOSYS;

	res = platform_get_resource(platform_dev, IORESOURCE_IRQ,1);
	if(res!=NULL)
		chip->fdma_req = res->start;
	else return -ENOSYS;

	/*we only care about this var for the analogue devices*/
	if(dev_nr < SPDIF_DEVICE  || dev_nr == PCMIN_DEVICE)  {
		res = platform_get_resource(platform_dev, IORESOURCE_IRQ,2);
		if(res!=NULL)
			chip->i2s_sampling_edge =
				(res->start ==1 ? PCMP_CLK_FALLING:PCMP_CLK_RISING);
		else return -ENOSYS;
	}
	return 0;
}

static int __init alsa_card_init(void)
{
	int i=0;
	for(;i<SND_DRV_CARDS-1;i++){
		if (snd_pcm_card_probe(i) < 0){
			DEBUG_PRINT(("STm PCM Player not found or device busy\n"));
			return -ENODEV;
		}
	}
	return 0;
}

static void __exit alsa_card_exit(void)
{
	int i=0;

	for(i=0;i<SND_DRV_CARDS-1;i++){
		if(card_list[i].device)
			snd_card_free(card_list[i].device);
	}
}

EXPORT_SYMBOL(format_iec60958_frame);
EXPORT_SYMBOL(snd_pcm_format_iec60958_copy);

module_init(alsa_card_init)
module_exit(alsa_card_exit)

