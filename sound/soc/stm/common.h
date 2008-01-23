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



/*
 * Audio DAC control interface
 */

int snd_stm_dac_get_config(struct device *device,
		unsigned long *pcm_format, unsigned int *oversampling);
int snd_stm_dac_shut_down(struct device *device);
int snd_stm_dac_wake_up(struct device *device);
int snd_stm_dac_mute(struct device *device);
int snd_stm_dac_unmute(struct device *device);



/*
 * Audio frequency synthesizer interface
 */

int snd_stm_fsynth_set_frequency(struct device *device, int channel,
		int frequency);

int snd_stm_fsynth_add_adjustement_ctl(struct device *device, int channel,
		struct snd_card *card, int card_device);



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



/*
 * Cards management
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
struct snd_card __init *snd_stm_cards_default(const char **id);



/*
 * Components
 *
 * Note that all component data and functions are marked
 * as a __init/__datainit, so are automatically cleaned
 * after initialization. THEY ARE NOT STATIC DATA!
 */

/* Component description */

struct snd_stm_component {
	const char *bus_id;
	const char *short_name;
	int num_caps;
	struct snd_stm_cap *caps;
};

/* Components list initialization */

int __init snd_stm_components_init(struct snd_stm_component *components,
		int num_components);

/* Component & device access */

struct snd_stm_component __init *snd_stm_components_get(const char *bus_id);
struct device __init *snd_stm_device_get(const char *bus_id);

/* Component capabilities description */

union snd_stm_value {
	int number;
	const char *string;
	struct {
		int from;
		int to;
	} range;
	struct {
		int *numbers;
		int len;
	} list;
};

struct snd_stm_cap {
	const char *name;
	union snd_stm_value value;
};

/* Capabilities access */

int __init snd_stm_cap_set(struct snd_stm_component *component,
		const char *name, union snd_stm_value value);
int __init snd_stm_cap_get(struct snd_stm_component *component,
		const char *name, union snd_stm_value *value);

int __init snd_stm_cap_get_number(struct snd_stm_component *component,
		const char *name, int *number);
int __init snd_stm_cap_get_string(struct snd_stm_component *component,
		const char *name, const char **string);
int __init snd_stm_cap_get_range(struct snd_stm_component *component,
		const char *name, int *from, int *to);
int __init snd_stm_cap_get_list(struct snd_stm_component *component,
		const char *name, int **numbers, int *len);



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
		int *channel, struct stm_dma_req **request,
		struct stm_dma_req_config *config);
void snd_stm_fdma_release(unsigned int channel,
		struct stm_dma_req *request);



/*
 * Drivers initialization/cleanup
 */

#ifdef CONFIG_CPU_SUBTYPE_STB7100
int __init snd_stm_stx710x_init(void);
#endif
#ifdef CONFIG_CPU_SUBTYPE_STX7200
int __init snd_stm_stx7200_init(void);
#endif

int __init snd_stm_audio_outputs_init(void);
void snd_stm_audio_outputs_cleanup(void);

int __init snd_stm_fsynth_init(void);
void snd_stm_fsynth_cleanup(void);

int __init snd_stm_dac_internal_init(void);
void snd_stm_dac_internal_cleanup(void);

int __init snd_stm_i2s_spdif_converter_init(void);
void snd_stm_i2s_spdif_converter_cleanup(void);

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

/* Memory dump function */

void snd_stm_hex_dump(void *data, int size);



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
