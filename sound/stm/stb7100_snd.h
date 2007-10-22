/*
 *  STb7100 ALSA Sound Driver
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

#ifndef STB7100_SND_H_
#define STB7100_SND_H_

#define FDMA2_BASE_ADDRESS			0x19220000
#define AUD_CFG_BASE				0x19210000
#define SPDIF_BASE				0x18103000
#define PCMP0_BASE				0x18101000
#define PCMP1_BASE				0x18101800
#define PCM0_CONVERTER_BASE     		0x18103800

#define FDMA2_BASE_ADDRESS			0x19220000
#define AUD_CFG_BASE				0x19210000

#define LINUX_PCMPLAYER0_ALLREAD_IRQ		144
#define LINUX_PCMPLAYER1_ALLREAD_IRQ		145
#define LINUX_SPDIFPLAYER_ALLREAD_IRQ		147
#define LINUX_SPDIFCONVERTER_ALLREAD_IRQ	142

/*
 * Thankfully the block register offsets for PCM0/1 & spdif
 * are the same, only with a differing base address.
 *
 * Alas this is not the case for the Fsynth's, so we must specify those seperately
 */
#define STM_PCMP_DATA_FIFO			0x04
#define STM_PCMP_IRQ_STATUS			0x08
#define STM_PCMP_IRQ_ENABLE			0x10
#define STM_PCMP_ITS_CLR 			0x0C
#define STM_PCMP_IRQ_EN_SET			0x14
#define STM_PCMP_IRQ_EN_CLR			0x18
#define STM_PCMP_CONTROL        		0x1C
#define STM_PCMP_STATUS         		0x20
#define STM_PCMP_FORMAT         		0x24

/*
 * The STb7100 PCM Player has an interrupt status, which inconveniently doesn't
 * have the bits laid out in the same position as the PCM Player Status register.
 */
#define PCMP_INT_STATUS_ALLREAD			(1<<1)
#define PCMP_INT_STATUS_UNDERFLOW		(1<<0)


#define AUD_FSYN0_MD				0x10
#define AUD_FSYN0_PE 				0x14
#define AUD_FSYN0_SDIV				0x18
#define AUD_FSYN0_PROG_EN      			0x1c

#define AUD_FSYN1_MD				0x20
#define AUD_FSYN1_PE				0x24
#define AUD_FSYN1_SDIV				0x28
#define AUD_FSYN1_PROG_EN			0x2c

#define AUD_FSYN2_MD				0x30
#define AUD_FSYN2_PE 				0x34
#define AUD_FSYN2_SDIV				0x38
#define AUD_FSYN2_PROG_EN			0x3c


/*spdif control reg operators*/

#define SPDIF_OFF				0x00
#define SPDIF_MUTE_NULL_DATA			0x01
#define SPDIF_MUTE_BURST			0x02
#define SPDIF_PCM_ON				0x03
#define SPDIF_ENCODED_ON			0x04
#define SPDIF_IDLE 				(1L<<3)
#define SPDIF_BIT16_DATA_ROUND 			(1L<<4)
#define SPDIF_BIT16_DATA_NOROUND 		(0L<<4)
#define SPDIF_FSYNTH_DIVIDE32_1			(0L<<5)
#define SPDIF_FSYNTH_DIVIDE32_128		(1L<<5)
#define SPDIF_FSYNTH_DIVIDE32_192		(6L<<5)
#define SPDIF_FSYNTH_DIVIDE32_256		(2L<<5)
#define SPDIF_FSYNTH_DIVIDE32_384		(3L<<5)
#define SPDIF_FSYNTH_DIVIDE32_512		(4L<<5)
#define SPDIF_FSYNTH_DIVIDE32_784		(6L<<5)
#define SPDIF_BYTE_SWAP 			(1L<<13)
#define SPDIF_HW_STUFFING 			(1L<<14)
#define SPDIF_SW_STUFFING 			(0L<<14)
#define SPDIF_SAMPLES_SHIFT			15


#define AUD_SPDIF_STA 				0x20
#define AUD_SPDIF_PA_PB 			0x24
#define AUD_SPDIF_PC_PD 			0x28
#define AUD_SPDIF_CL1   			0x2c/*left subframe status 31-0*/
#define AUD_SPDIF_CR1   			0x30 /*right subframe status 31-0*/
#define AUD_SPDIF_CL2_CR2_UV 			0x34
#define AUD_SPDIF_PAU_LAT 			0x38
#define AUD_SPDIF_FRA_LEN_BST 			0x3c


/*spdif int generation vals*/
#define ENABLE_INT_UNDERFLOW 			(1L<<0)
#define ENABLE_INT_EODBURST 			(1L<<1)
#define ENABLE_INT_EOBLOCK 			(1L<<2)
#define ENABLE_INT_EOLATENCY 			(1L<<3)
#define ENABLE_INT_EOPD				(1L<<4)
#define ENABLE_INT_NSAMPLE 			(1L<<5)

#define SPDIF_INT_STATUS_UNF			(1L<<0)
#define SPDIF_INT_STATUS_EODBURST  		(1L<<1)
#define SPDIF_INT_STATUS_EOBLOCK 		(1L<<2)
#define SPDIF_INT_STATUS_EOLATENCY 		(1L<<3)
#define SPDIF_INT_STATUS_EOPD	 		(1L<<4)
#define SPDIF_INT_STATUS_ALLREAD 		(1L<<5)


#define MPEG_DECODE_LAT_48KHZ 21 /*really 20.9 ms*/
#define MPEG_DECODE_LAT_441KHZ 23 /*really 22.75 ms*/
#define MPEG_DECODE_LAT_32KHZ 31 /*really 30.35 ms*/

#define TIME_TO_FRAMES(freq,time)((time * freq) /1000)

#define  IEC61937_PA  		 0xF872
#define  IEC61937_PB  		 0x4E1F
#define  IEC61937_DTS_TYPE_1	 11
#define  IEC61937_DTS_TYPE_2	 12
#define  IEC61937_DTS_TYPE_3	 13
#define  IEC61937_AC3_STREAM     0x1

#define DUMP_CONVERTER_STATE(chip) (\
	printk("%s\n\
	AUD_SPDIF_PR_CFG %x\n\
	AUD_SPDIF_PR_STAT %x\n\
	AUD_SPDIF_PR_INT_EN %x\n\
	AUD_SPDIF_PR_INT_STA %x\n\
	AUD_SPDIF_PR_INT_CLR %x\n\
	AUD_SPDIF_PR_VALIDITY %x\n\
	AUD_SPDIF_PR_USER_DATA %x\n\
	AUD_SPDIF_PR_CHANNEL_STA_BASE %x\n\
	AUD_SPDIF_PR_SPDIF_CTRL	%x\n\
	AUD_SPDIF_PR_SPDIF_STA	%x\n\
	AUD_SPDIF_PR_SPDIF_PAUSE %x\n\
	AUD_SPDIF_PR_SPDIF_DATA_BURST %x\n\
	AUD_SPDIF_PR_SPDIF_PA_PB %x\n\
	AUD_SPDIF_PR_SPDIF_PC_PD %x\n\
	AUD_SPDIF_PR_SPDIF_CL1	%x\n\
	AUD_SPDIF_PR_SPDIF_CR1	%x\n\
	AUD_SPDIF_PR_SPDIF_SUV	%x\n",\
	__FUNCTION__,\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_CFG),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_STAT ),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_INT_EN),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_INT_STA),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_INT_CLR),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_VALIDITY ),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_USER_DATA),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_CHANNEL_STA_BASE),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CTRL),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_STA),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_PAUSE),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_DATA_BURST),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_PA_PB),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_PC_PD),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CL1),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_CR1	),\
	(int)readl(chip->pcm_converter + AUD_SPDIF_PR_SPDIF_SUV)));

#define DUMP_SPDIF_STATE()(\
	printk("%s\n\
	AUD_SPDIF_RST  %x\n\
	AUD_SPDIF_DATA %x\n\
	AUD_SPDIF_ITS  %x\n\
	AUD_SPDIF_ITS_CLR %x\n\
	AUD_SPDIF_ITS_EN %x\n\
	AUD_SPDIF_ITS_EN_SET %x\n\
	AUD_SPDIF_ITS_EN_CLR %x\n\
	AUD_SPDIF_CTL %x\n\
	AUD_SPDIF_STA %x\n\
	AUD_SPDIF_PA_PB %x\n\
	AUD_SPDIF_PC_PD %x\n \
	AUD_SPDIF_CL1 %x\n\
	AUD_SPDIF_CR1 %x\n\
	AUD_SPDIF_CL2CR2UV %x\n\
	AUD_SPDIF_FRA_LEN_BST %x\n\
	AUD_SPDIF_PAU_LAT %x \n",\
		__FUNCTION__,\
		(int)readl(chip->pcm_player+0x00),\
		(int)readl(chip->pcm_player+STM_PCMP_DATA_FIFO),\
		(int)readl(chip->pcm_player+STM_PCMP_IRQ_STATUS),\
		(int)readl(chip->pcm_player+STM_PCMP_ITS_CLR ),\
		(int)readl(chip->pcm_player+STM_PCMP_IRQ_ENABLE),\
		(int)readl(chip->pcm_player+STM_PCMP_IRQ_EN_SET),\
		(int)readl(chip->pcm_player+STM_PCMP_IRQ_EN_CLR),\
		(int)readl(chip->pcm_player+STM_PCMP_CONTROL),\
		(int)readl(chip->pcm_player+STM_PCMP_STATUS ),\
		(int)readl(chip->pcm_player+AUD_SPDIF_PA_PB),\
		(int)readl(chip->pcm_player+AUD_SPDIF_PC_PD),\
		(int)readl(chip->pcm_player+AUD_SPDIF_CL1),\
		(int)readl(chip->pcm_player+AUD_SPDIF_CR1),\
		(int)readl(chip->pcm_player+AUD_SPDIF_CL2_CR2_UV),\
		(int)readl(chip->pcm_player+AUD_SPDIF_FRA_LEN_BST),\
		(int)readl(chip->pcm_player+AUD_SPDIF_PAU_LAT)))

#define DUMP_PCM_STATE(chip)(\
	printk("%s\n\
	STM_PCMP_RST %x\n\
	STM_PCMP_DATA_FIFO %x\n\
	STM_PCMP_IRQ_STATUS %x\n\
	STM_PCMP_IRQ_ENABLE %x\n\
	STM_PCMP_ITS_CLR  %x\n\
	STM_PCMP_IRQ_EN_SET %x\n\
	STM_PCMP_IRQ_EN_CLR %x\n\
	STM_PCMP_CONTROL %x\n\
	STM_PCMP_STATUS  %x\n\
	STM_PCMP_FORMAT %x\n",\
	__FUNCTION__,\
	(int)readl(chip->pcm_player+0x00),\
	(int)readl(chip->pcm_player+STM_PCMP_DATA_FIFO),\
	(int)readl(chip->pcm_player+STM_PCMP_IRQ_STATUS),\
	(int)readl(chip->pcm_player+STM_PCMP_IRQ_ENABLE),\
	(int)readl(chip->pcm_player+STM_PCMP_ITS_CLR ),\
	(int)readl(chip->pcm_player+STM_PCMP_IRQ_EN_SET),\
	(int)readl(chip->pcm_player+STM_PCMP_IRQ_EN_CLR),\
	(int)readl(chip->pcm_player+STM_PCMP_CONTROL),\
	(int)readl(chip->pcm_player+STM_PCMP_STATUS ),\
	(int)readl(chip->pcm_player+STM_PCMP_FORMAT )));
/*
 * I2S to SPDIF Protocol converter defines
 */
#define AUD_SPDIF_PR_CFG			0x00
#define AUD_SPDIF_PR_STAT			0x04
#define AUD_SPDIF_PR_INT_EN			0x08
#define AUD_SPDIF_PR_INT_STA			0x0c
#define AUD_SPDIF_PR_INT_CLR			0x10
#define AUD_SPDIF_PR_VALIDITY			0x100
#define AUD_SPDIF_PR_USER_DATA			0x104
#define AUD_SPDIF_PR_CHANNEL_STA_BASE		0x108
#define AUD_SPDIF_PR_SPDIF_CTRL			0x200
#define AUD_SPDIF_PR_SPDIF_STA			0x204
#define AUD_SPDIF_PR_SPDIF_PAUSE		0x208
#define AUD_SPDIF_PR_SPDIF_DATA_BURST		0x20c
#define AUD_SPDIF_PR_SPDIF_PA_PB		0x210
#define AUD_SPDIF_PR_SPDIF_PC_PD		0x214
#define AUD_SPDIF_PR_SPDIF_CL1			0x218
#define AUD_SPDIF_PR_SPDIF_CR1			0x21c
#define AUD_SPDIF_PR_SPDIF_SUV			0x220

#define PR_CFG_DEVICE_ENABLE			(1L<<0)
#define PR_CFG_CONV_SW_RESET			(1L<<1)
#define PR_CFG_FIFO_ENABLE			(1L<<2)
#define PR_CFG_WORD_SZ_16BIT			(0L<<3)
#define PR_CFG_WORD_SZ_20BIT			(1L<<3)
#define PR_CFG_WORD_SZ_24BIT			(2L<<3)
#define PR_CFG_REQ_ACK_ENABLE			(1L<<5)


#define PR_PD_PAUSE_BURST_INT			(1L<<31)
#define PR_AUDIO_SAMPLES_FULLY_READ_INT		(1L<<22)
#define PR_PD_DATA_BURST_INT			(1L<<21)
#define PR_LATENCY_INT				(1L<<20)
#define PR_EOBLOCK_INT				(1L<<19)
#define PR_EODATABURST_INT			(1L<<18)
#define PR_UNDERFLOW_INT			(1L<<17)
#define PR_RUN_STOP_INT				(1L<<16)
#define PR_I2S_FIFO_OVERRUN_INT			(1L<<8)
#define PR_CHLL_STS_UNDERRUN_INT		(1L<<7)
#define PR_CHLL_STS_EMPTY_INT			(1L<<6)
#define PR_USER_DATA_UNDERRUN_INT		(1L<<5)
#define PR_USER_DATA_EMPTY_INT			(1L<<4)
#define PR_VALIDITY_UNDERRUN_INT		(1L<<3)
#define PR_VALIDITY_EMPTY_INT			(1L<<2)
#define PR_SOFT_RESET_INT_ENABLE		(1L<<1)
#define PR_INTERRUPT_ENABLE			(1L<<0)


#define PR_CTRL_OFF				0
#define PR_CTRL_MUTE_PCM_NULL_DATA		1
#define PR_CTRL_MUTE_PAUSE_BURST		2
#define PR_CTRL_AUDIO_DATA_MODE			3
#define PR_CTRL_16BIT_DATA_NOROUND		0
#define PR_CTRL_16BIT_DATA_ROUND		(1L<<4)
#define PR_CTRL_SW_STUFFING			0
#define PR_CTRL_HW_STUFFING			(1L<<14)
#define PR_CTRL_SAMPLES_SHIFT			15

#endif /*STB7100_SND_H_*/

static int stb7100_create_spdif_device(pcm_hw_t * chip,snd_card_t **card);
static int stb7100_create_converter_device(pcm_hw_t *chip,snd_card_t **this_card);
static int stb7100_converter_program_player(snd_pcm_substream_t *substream);
static void stb7100_reset_pcm_player(pcm_hw_t *chip);
