/*
 * STMicroelectronics' System-on-Chips audio subsystem commons
 */

#ifndef __SOUND_STM_COMMON_H
#define __SOUND_STM_COMMON_H

#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/stm-dma.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/control.h>
#include <sound/stm.h>



/*
 * Audio frequency synthesizer description (platform data)
 */

struct snd_stm_fsynth_info {
	const char *card_id;

	int channels_from, channels_to;
};

int snd_stm_fsynth_set_frequency(struct device *device, int channel,
		int frequency);

int snd_stm_fsynth_add_adjustement_ctl(struct device *device, int channel,
		struct snd_card *card, int card_device);



/*
 * Converters (DAC, ADC, I2S-SPDIF etc.) control interface
 */

struct snd_stm_conv *snd_stm_conv_get_attached(struct device *source);
int snd_stm_conv_add_route_ctl(struct device *source,
		struct snd_card *card, int card_device);

unsigned int snd_stm_conv_get_format(struct snd_stm_conv *conv);
int snd_stm_conv_get_oversampling(struct snd_stm_conv *conv);

int snd_stm_conv_enable(struct snd_stm_conv *conv);
int snd_stm_conv_disable(struct snd_stm_conv *conv);
int snd_stm_conv_mute(struct snd_stm_conv *conv);
int snd_stm_conv_unmute(struct snd_stm_conv *conv);



/*
 * Internal audio DAC description (platform data)
 */

struct snd_stm_conv_internal_dac_info {
	const char *name;

	const char *card_id;
	int card_device;

	const char *source_bus_id;
};


/*
 * I2S to SPDIF converter description (platform data)
 */

struct snd_stm_conv_i2s_spdif_info {
	const char *name;

	const char *card_id;
	int card_device;

	const char *source_bus_id;

	int full_channel_status;
};



/*
 * PCM Player description (platform data)
 */

struct snd_stm_pcm_player_info {
	const char *name;

	const char *card_id;
	int card_device;

	const char *fsynth_bus_id;
	int fsynth_output;

	unsigned int channels_num;
	unsigned int *channels;

	unsigned char fdma_initiator;
	unsigned int fdma_request_line;
	int fdma_max_transfer_size;

	int invert_sclk_edge_falling;
};



/*
 * PCM Reader description (platform data)
 */

struct snd_stm_pcm_reader_info {
	const char *name;

	const char *card_id;
	int card_device;

	int channels_num;
	int *channels;

	unsigned char fdma_initiator;
	unsigned int fdma_request_line;
	int fdma_max_transfer_size;
};



/*
 * SPDIF Player description (platform data)
 */

struct snd_stm_spdif_player_info {
	const char *name;

	const char *card_id;
	int card_device;

	const char *fsynth_bus_id;
	int fsynth_output;

	unsigned char fdma_initiator;
	unsigned int fdma_request_line;
	int fdma_max_transfer_size;
};



/*
 * Buffer memory mapping operation
 */

int snd_stm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *area);



/*
 * Common ALSA controls routines
 */

int snd_stm_ctl_boolean_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo);

int snd_stm_ctl_iec958_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo);

int snd_stm_ctl_iec958_mask_get_con(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

int snd_stm_ctl_iec958_mask_get_pro(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

int snd_stm_iec958_cmp(const struct snd_aes_iec958 *a,
		const struct snd_aes_iec958 *b);



/*
 * Common ALSA parameters constraints
 */

int snd_stm_pcm_transfer_bytes(unsigned int bytes_per_frame,
		unsigned int max_transfer_bytes);
int snd_stm_pcm_hw_constraint_transfer_bytes(struct snd_pcm_runtime *runtime,
		unsigned int max_transfer_bytes);



/*
 * Device management
 */

/* Add/remove a list of platform devices */
int __init snd_stm_add_plaform_devices(struct platform_device **devices,
		int cnt);
void __exit snd_stm_remove_plaform_devices(struct platform_device **devices,
		int cnt);

/* Leave bus NULL to use default (platform) bus */
struct device *snd_stm_find_device(struct bus_type *bus,
		const char *bus_id);



/*
 * Components management
 */

/* Card description */

struct snd_stm_card {
	int index;              /* card number */
	const char *id;         /* unique, 15 chars max (plus '\0' as 16th) */
	const char *short_name; /* 31 chars max (plus '\0' as 32nd) */
	const char *long_name;  /* 79 chars max (plus '\0' as 80th) */
};

/* Card list initialization/cleanup */

int __init snd_stm_cards_init(const char *device, struct snd_stm_card *cards,
		int num_cards);
int __init snd_stm_cards_register(void);
void snd_stm_cards_free(void);

/* Card list access */

struct snd_card __init *snd_stm_cards_get(const char *id);
struct snd_card __init *snd_stm_cards_default(void);



/*
 * ALSA procfs additional entries
 */

int __init snd_stm_info_init(void);
void snd_stm_info_cleanup(void);

int snd_stm_info_register(struct snd_info_entry **entry,
		const char *name,
		void (read)(struct snd_info_entry *, struct snd_info_buffer *),
		void *private_data);
void snd_stm_info_unregister(struct snd_info_entry *entry);



/*
 * Resources management
 */

int __init snd_stm_memory_request(struct platform_device *pdev,
		struct resource **mem_region, void **base_address);
void snd_stm_memory_release(struct resource *mem_region,
		void *base_address);

int __init snd_stm_irq_request(struct platform_device *pdev,
		unsigned int *irq, irq_handler_t handler, void *dev_id);
#define snd_stm_irq_release(irq, dev_id) free_irq(irq, dev_id)

int __init snd_stm_fdma_request(struct platform_device *pdev,
		unsigned int *channel);
#define snd_stm_fdma_release(channel) free_dma(channel)



/*
 * Drivers initialization/cleanup
 */

#if defined(CONFIG_CPU_SUBTYPE_STB7100)
int __init snd_stm_stx710x_init(void);
void __exit snd_stm_stx710x_cleanup(void);
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7111)
int __init snd_stm_stx7111_init(void);
void __exit snd_stm_stx7111_cleanup(void);
#endif
#if defined(CONFIG_CPU_SUBTYPE_STX7200)
int __init snd_stm_stx7200_init(void);
void __exit snd_stm_stx7200_cleanup(void);
#endif

int __init snd_stm_audio_outputs_init(void);
void snd_stm_audio_outputs_cleanup(void);

int __init snd_stm_fsynth_init(void);
void snd_stm_fsynth_cleanup(void);

int __init snd_stm_conv_init(void);
void snd_stm_conv_cleanup(void);

int __init snd_stm_conv_dummy_init(void);
void snd_stm_conv_dummy_cleanup(void);

int __init snd_stm_conv_internal_dac_init(void);
void snd_stm_conv_internal_dac_cleanup(void);

int __init snd_stm_conv_i2s_spdif_init(void);
void snd_stm_conv_i2s_spdif_cleanup(void);

int __init snd_stm_pcm_player_init(void);
void snd_stm_pcm_player_cleanup(void);

int __init snd_stm_pcm_reader_init(void);
void snd_stm_pcm_reader_cleanup(void);

int __init snd_stm_spdif_player_init(void);
void snd_stm_spdif_player_cleanup(void);

int __init snd_stm_synchro_init(void);
void snd_stm_synchro_cleanup(void);



/*
 * Debug features
 */

/* Data dump functions */

void snd_stm_hex_dump(void *data, int size);
void snd_stm_iec958_dump(const struct snd_aes_iec958 *vuc);



/* Trace debug messages
 * - even more debugs than with CONFIG_SND_DEBUG ;-)
 * - enables snd_printd when CONFIG_SND_DEBUG is not defined :-)
 * - define TRACE _before_ including common.h to enable in selected
 *   submodule; alternatively you can change following "#ifdef TRACE"
 *   to "#if 1" to force verbose output in all STM submodules. */

#ifdef TRACE

#	ifndef CONFIG_SND_DEBUG
#		undef snd_printd
#		define snd_printd(format, args...) \
				snd_printk(KERN_INFO format, ## args)
#	endif

#	define snd_stm_printt(format, args...) \
			snd_printd(format, ## args)

#else

#	define snd_stm_printt(...)

#endif

/* Error debug messages */

#define snd_stm_printe(format, args...) \
		snd_printk(KERN_ERR format, ## args)

/* Magic value checking in device structures
 * - define MAGIC as a unique value _before_ including
 *   common.h to enable in selected submodule; alternatively you can
 *   change following "#ifdef MAGIC" to "#if 1" to force magic
 *   checking in all STM submodules. */

#ifdef MAGIC

	enum snd_stm_magic {
		snd_stm_magic_good = 0x600da15a + MAGIC,
		snd_stm_magic_bad  = 0xbaada15a + MAGIC
	};

#	define snd_stm_magic_field \
			enum snd_stm_magic __magic
#	define snd_stm_magic_set(object) \
			(object)->__magic = snd_stm_magic_good
#	define snd_stm_magic_clear(object) \
			(object)->__magic = snd_stm_magic_bad
#	define snd_stm_magic_assert(object, args...) \
			snd_assert((object)->__magic == snd_stm_magic_good, \
					## args)

#else

#	define snd_stm_magic_field
#	define snd_stm_magic_set(object)
#	define snd_stm_magic_clear(object)
#	define snd_stm_magic_assert(object, args...)

#endif



#endif
