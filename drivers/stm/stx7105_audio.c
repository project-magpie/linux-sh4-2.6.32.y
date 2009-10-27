#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <sound/stm.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Audio subsystem glue */

static struct platform_device stx7105_glue = {
	.name          = "snd_stx7105_glue",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210200, 0xc),
	}
};

/* Frequency synthesizers */

static struct platform_device stx7105_fsynth = {
	.name          = "snd_fsynth",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210000, 0x50),
	},
	.dev.platform_data = &(struct snd_stm_fsynth_info) {
		.ver = 5,
		.channels_from = 0,
		.channels_to = 2,
	},
};

/* Internal DAC */

static struct platform_device stx7105_conv_int_dac = {
	.name          = "snd_conv_int_dac",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210100, 0x4),
	},
	.dev.platform_data = &(struct snd_stm_conv_int_dac_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* PCM players  */

static struct snd_stm_pcm_player_info stx7105_pcm_player_0_info = {
	.name = "PCM player #0 (HDMI)",
	.ver = 6,
	.card_device = 0,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 0,
	.channels = 8,
	.fdma_initiator = 0,
	.fdma_request_line = 39,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7105_pcm_player_0_pad_config = {
	.labels_num = 4,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_RANGE("PIO10", 3, 5),
		STM_PAD_LABEL("PIO10.0"),
		STM_PAD_LABEL("PIO10.1"),
		STM_PAD_LABEL("PIO10.2"),
	},
	.gpio_values_num = 6,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(10, 3),	/* MCLK */
		STM_PAD_PIO_ALT_OUT(10, 4),	/* LRCLK */
		STM_PAD_PIO_ALT_OUT(10, 5),	/* SCLK */
		STM_PAD_PIO_ALT_OUT(10, 0),	/* DATA0 */
		STM_PAD_PIO_ALT_OUT(10, 1),	/* DATA1 */
		STM_PAD_PIO_ALT_OUT(10, 2),	/* DATA2 */
	},
};

static struct platform_device stx7105_pcm_player_0 = {
	.name          = "snd_pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104d00, 0x28),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1400), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0 (HDMI)",
		.ver = 6,
		.card_device = 0,
		.fsynth_bus_id = "snd_fsynth",
		.fsynth_output = 0,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 39,
	},
};

static struct platform_device stx7105_pcm_player_1 = {
	.name          = "snd_pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd101800, 0x28),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1420), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		.ver = 6,
		.card_device = 1,
		.fsynth_bus_id = "snd_fsynth",
		.fsynth_output = 1,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 34,
	},
};

/* SPDIF player */

static struct snd_stm_spdif_player_info stx7105_spdif_player_info = {
	.name = "SPDIF player (HDMI)",
	.ver = 4,
	.card_device = 2,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 40,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7105_spdif_player_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL("PIO10.6"),
	},
	.gpio_values_num = 1,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(10, 6),
	},
};

static struct platform_device stx7105_spdif_player = {
	.name          = "snd_spdif_player",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104c00, 0x44),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1460), -1),
	},
	.dev.platform_data = &stx7105_spdif_player_info,
};

/* I2S to SPDIF converters */

static struct platform_device stx7105_conv_i2sspdif_0 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105000, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a00), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7105_conv_i2sspdif_1 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105400, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a20), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7105_conv_i2sspdif_2 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105800, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a40), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7105_conv_i2sspdif_3 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105c00, 0x224),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x0a60), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct snd_stm_pcm_reader_info stx7105_pcm_reader_info = {
	.name = "PCM Reader",
	.ver = 5,
	.card_device = 3,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 37,
	/* .pad_config set by stx7105_configure_audio() */
};

static struct stm_pad_config stx7105_pcm_reader_pad_config = {
	.labels_num = 2,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL("PIO10.7"),
		STM_PAD_LABEL_RANGE("PIO11", 0, 1),
	},
	.gpio_values_num = 3,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_IN(10, 0),	/* DATA */
		STM_PAD_PIO_IN(11, 0),	/* SCLK */
		STM_PAD_PIO_IN(11, 1),	/* LRCLK */
	},
};

static struct platform_device stx7105_pcm_reader = {
	.name          = "snd_pcm_reader",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd102000, 0x28),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1440), -1),
	},
	.dev.platform_data = &stx7105_pcm_reader_info,
};

/* Devices */

static struct platform_device *stx7105_audio_devices[] __initdata = {
	&stx7105_glue,
	&stx7105_fsynth,
	&stx7105_conv_int_dac,
	&stx7105_pcm_player_0,
	&stx7105_pcm_player_1,
	&stx7105_spdif_player,
	&stx7105_conv_i2sspdif_0,
	&stx7105_conv_i2sspdif_1,
	&stx7105_conv_i2sspdif_2,
	&stx7105_conv_i2sspdif_3,
	&stx7105_pcm_reader,
};

static int __init stx7105_audio_devices_setup(void)
{
	if (cpu_data->type != CPU_STX7105) {
		BUG();
		return -ENODEV;
	}

	/* Ugly but quick hack to have SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout = ioremap(0xfd104020, 4);
		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	return platform_add_devices(stx7105_audio_devices,
			ARRAY_SIZE(stx7105_audio_devices));
}
device_initcall(stx7105_audio_devices_setup);

/* Configuration */

void __init stx7105_configure_audio(struct stx7105_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->pcm_player_0_output >
			stx7105_pcm_player_0_output_disabled) {
		int unused = 3 - config->pcm_player_0_output;

		stx7105_pcm_player_0_info.pad_config =
				&stx7105_pcm_player_0_pad_config;

		stx7105_pcm_player_0_pad_config.labels_num -= unused;
		stx7105_pcm_player_0_pad_config.gpio_values_num -= unused;
	}

	if (config->spdif_player_output_enabled)
		stx7105_spdif_player_info.pad_config =
				&stx7105_spdif_player_pad_config;

	if (config->pcm_reader_input_enabled)
		stx7105_pcm_reader_info.pad_config =
				&stx7105_pcm_reader_pad_config;
}
