/*
 *  Definitions for ST PCM Player Sound Driver
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

#ifndef _PCM_PLAYER_HW_H
#define _PCM_PLAYER_HW_H

#include <sound/asound.h>
#include <linux/stm/stm-dma.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INCR_FSYNTH    0x1
#define DECR_FSYNTH    0x2
#define SDIV_SHIFT_VAL 0x4000
#define MD_SHIFT_VAL   0x8000

/*make some named definitions to refer to each output device so we dont need to rely on the card number*/
/*card majors*/
#define PCM0_DEVICE			0
#define PCM1_DEVICE			1
#define SPDIF_DEVICE			2
#define PROTOCOL_CONVERTER_DEVICE	3
#define PCMIN_DEVICE			4
/*card minors*/
#define MAIN_DEVICE			0
#define SUB_DEVICE1			1
struct pcm_hw_t;

typedef enum {
	STM_DATA_TYPE_LPCM,
	STM_DATA_TYPE_IEC60958,
	STM_DATA_TYPE_I2S
} stm_snd_data_type_t;


typedef struct {
        int                  major;
        int                  minor;
        stm_snd_data_type_t  input_type;
        stm_snd_data_type_t  output_type;
        snd_card_t          *device;
        int                  in_use;
} stm_snd_output_device_t;

#define PCM_SAMPLE_SIZE_BYTES		4

#if (STM_USE_BIGPHYS_AREA == 0)
#define PCM_MAX_FRAMES			3276  /* <128k, max slab allocation */
#define PCM_PREALLOC_SIZE		(128*1024)
#define PCM_PREALLOC_MAX		(128*1024)
#else
#define PCM_MAX_FRAMES			48000 /* 1s @ 48KHz */
/* Note: we cannot preallocate a buffer from ALSA if we want to
 * use bigphysmem for large buffers and the standard page
 * alocation for small buffers. The preallocation is spotted by
 * the generic ALSA driver layer and the size is used to limit
 * the buffer size requests before they even get to this driver.
 * This overrides the buffer_bytes_max value in the hardware
 * capabilities structure we set up.
 */
#define PCM_PREALLOC_SIZE		0
#define PCM_PREALLOC_MAX		0
#endif

/*
 * Buffers larger than 128k should come from bigphysmem to avoid
 * page fragmentation and random resource starvation in the rest of
 * the system. Buffers <=128k come from the ALSA dma memory allocation
 * system, which uses get_free_pages directly, not a slab memory cache.
 */
#define PCM_BIGALLOC_SIZE		(128*1024)

#define FRAMES_TO_BYTES(x,channels) (( (x) * (channels) ) * PCM_SAMPLE_SIZE_BYTES)
#define BYTES_TO_FRAMES(x,channels) (( (x) / (channels) ) / PCM_SAMPLE_SIZE_BYTES)

/*
 * Common PCM Player Control register definitions
 */
#define PCMP_OPERATION_MASK		0x3
#define PCMP_OFF			0x0
#define PCMP_MUTE			0x1
#define PCMP_ON				0x2

#define PCMP_MEM_FMT_16_0		0
#define PCMP_MEM_FMT_16_16		(1L<<2)
#define PCMP_NO_ROUNDING		0
#define PCMP_ROUNDING			(1L<<3)

#define PCMP_DIV_MASK			0x00000ff0
#define PCMP_FSYNTH_DIVIDE32_1		(0L<<4)
#define PCMP_FSYNTH_DIVIDE32_128	(1L<<4)
#define PCMP_FSYNTH_DIVIDE32_192	(6L<<4)
#define PCMP_FSYNTH_DIVIDE32_256	(2L<<4)
#define PCMP_FSYNTH_DIVIDE32_384	(3L<<4)
#define PCMP_FSYNTH_DIVIDE32_512	(4L<<4)
#define PCMP_FSYNTH_DIVIDE32_784	(6L<<4)

#define PCMP_IGNORE_SPDIF_LATENCY	0
#define PCMP_WAIT_SPDIF_LATENCY		(1L<<12)

#define PCMP_SAMPLES_SHIFT		13
/*
 * The sample count field is 19bits wide, so the maximum size
 * is 2^20-1 = 104,857. However becuase we count a sample for every
 * channel and we usually have 10 channel PCM Players it is helpful
 * to make this a nice round number that is a multiple of 10.
 */
#define PCMP_MAX_SAMPLES		(104000)

/*
 * PCM Player Format register definitions
 */
#define PCMP_FORMAT_16			1
#define PCMP_FORMAT_32			0
#define PCMP_LENGTH_24			0
#define PCMP_LENGTH_20			(1L<<1)
#define PCMP_LENGTH_18			(2L<<1)
#define PCMP_LENGTH_16			(3L<<1)

#define PCMP_LRLEVEL_LEFT_LOW		0
#define PCMP_LRLEVEL_LEFT_HIGH		(1L<<3)
#define PCMP_CLK_RISING			0
#define PCMP_CLK_FALLING		(1L<<4)

/* Danger Will Robinson Danger Danger!!!
 * the data delay by one bit logic is inverted
 */
#define PCMP_PADDING_ON			0
#define PCMP_PADDING_OFF		(1L<<5)

#define PCMP_ALIGN_START		0
#define PCMP_ALIGN_END			(1L<<6)
#define PCMP_LSB_FIRST			0
#define PCMP_MSB_FIRST			(1L<<7)

#define PCMP_STATUS_RUNNING		(1L<<0)
#define PCMP_STATUS_UNDERFLOW		(1L<<1)
#define PCMP_STATUS_ALLREAD		(1L<<2)

/*
 * DVD category code definition for convenience.
 */
#define IEC958_AES1_CON_NON_IEC908_DVD (IEC958_AES1_CON_LASEROPT_ID|0x018)

/*
 * Extension to the ALSA channel status definitions for consumer mode 24bit
 * wordlength.
 */
#define IEC958_AES4_CON_WORDLEN_MAX_24 (1<<0)
#define IEC958_AES4_CON_WORDLEN_24_20  (5<<1)

typedef enum iec_encodings {
	ENCODING_IEC60958 = 0,
	ENCODING_IEC61937_AC3,
	ENCODING_IEC61937_DTS_1,
	ENCODING_IEC61937_DTS_2,
	ENCODING_IEC61937_DTS_3,
	ENCODING_IEC61937_MPEG_384_FRAME,
	ENCODING_IEC61937_MPEG_1152_FRAME,
	ENCODING_IEC61937_MPEG_1024_FRAME,
	ENCODING_IEC61937_MPEG_2304_FRAME,
	ENCODING_IEC61937_MPEG_768_FRAME,
	ENCODING_IEC61937_MPEG_2304_FRAME_LSF,
	ENCODING_IEC61937_MPEG_768_FRAME_LSF,
}iec_encodings_t;

#define ENCODED_STREAM_TYPES  12

typedef struct IEC60958 {
	/* Channel status bits are the same for L/R subframes */
       	snd_aes_iec958_t  channel;

        /* Validity bits can be different on L and R e.g. in
         * professional applications
         */
        u8                validity_l[24];
        u8                validity_r[24];
        /* User bits are considered contiguous across L/R subframes */
        u8                user[48];
}IEC60958_t;


typedef struct {
	int			(*free_device)     (struct pcm_hw_t *card);
	int			(*open_device)     (snd_pcm_substream_t *substream);
	int			(*program_hw)      (snd_pcm_substream_t *substream);
	snd_pcm_uframes_t	(*playback_pointer)(snd_pcm_substream_t *substream);

	void			(*start_playback)  (snd_pcm_substream_t *substream);
	void			(*stop_playback)   (snd_pcm_substream_t *substream);
	void			(*pause_playback)  (snd_pcm_substream_t *substream);
	void			(*unpause_playback)(snd_pcm_substream_t *substream);
} stm_playback_ops_t;

typedef struct _IEC61937 {
	int pause_mode; /*do we attmept a pause burst of mute with null ? */
	int mute_rep;	/**/
	int pause_count;
	int frame_size;/*frames per burst*/
	int latency;/*61937 defined maximum decode latency*/
	int unpause_flag;
}IEC61937_t;

typedef struct pcmin_ctx{
	struct 	timer_list period_timer;
	int 	timer_halt;
	int 	fr_delta;
	int 	last_fr;
}pcmin_ctx;

typedef struct pcm_hw_t {
	snd_card_t		*card;

	spinlock_t		lock;
	int			irq;
        stm_snd_output_device_t *card_data;
	unsigned long		buffer_start_addr;
	unsigned long		pcmplayer_control;
	unsigned long		irq_mask;
	snd_pcm_hardware_t      hw;

	snd_pcm_uframes_t    	hwbuf_current_addr;
	snd_pcm_substream_t 	*current_substream;
	char		   	*out_pipe;
	char		    	*pcm_clock_reg;
	char 			*pcm_player;
	char                    *pcm_converter;
	int		     	are_clocks_active;
	int                     oversampling_frequency;

	stm_playback_ops_t	*playback_ops;

        IEC60958_t              current_spdif_control;
        IEC60958_t              pending_spdif_control;
        IEC60958_t              default_spdif_control;

	int                     iec60958_output_count;
	char			iec60958_rawmode;
	char 			iec_encoding_mode;

	IEC61937_t 		iec61937;
	int  			min_ch;
	int 			max_ch;
	int 			fdma_req;
	struct 	stm_dma_params  dmap;
	int 			i2s_sampling_edge;
	int			fifo_check_mode;
	struct 	pcmin_ctx	pcmin;
#if defined(CONFIG_CPU_SUBTYPE_STB7100)
	int 			spdif_player_mode;
	int			fdma_channel;
#endif
} pcm_hw_t;

struct stm_freq_s {
	u32 freq;
	u32 sdiv_val;
	u32 pe_val;
	u32 md_val;
	u32 pe_quantum;
};

#define NUM_CLOCK_SETTINGS	5

#define PCM0_SYNC_ID		2
#define PCM1_SYNC_ID		4
#define SPDIF_SYNC_MODE_ON	1

#define chip_t pcm_hw_t

static int snd_pcm_dev_free(snd_device_t *dev);

static int __devinit snd_card_pcm_allocate(pcm_hw_t *stm8000, int device,char* name);
static int __devinit snd_iec60958_create_controls(pcm_hw_t *chip);
static int __devinit snd_generic_create_controls(pcm_hw_t *chip);
static int __devinit register_platform_driver(	struct platform_device *platform_dev,
						pcm_hw_t *chip,
						int dev_nr);
void set_spdif_syncing_status(int enable);

extern void iec60958_default_channel_status(pcm_hw_t *chip);
extern void iec60958_set_runtime_status(snd_pcm_substream_t *substream);


#define DEBUG_PRINT(_x)

#ifdef __cplusplus
}
#endif

#endif  /* _ST_PCM_H */
