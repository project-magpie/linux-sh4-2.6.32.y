#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7141.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>



/* Audio subsystem resources ---------------------------------------------- */

/* Audio subsystem glue */

static struct platform_device stx7141_glue = {
	.name          = "snd_stx7141_glue",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210200, 0xc),
	}
};

/* Frequency synthesizers */

static struct platform_device stx7141_fsynth = {
	.name          = "snd_fsynth",
	.id            = -1,
	.num_resources = 1,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe210000, 0x50),
	},
	.dev.platform_data = &(struct snd_stm_fsynth_info) {
		.ver = 4,
		.channels_from = 0,
		.channels_to = 3,
	},
};

/* Internal DAC */

static struct platform_device stx7141_conv_int_dac = {
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

static struct snd_stm_pcm_player_info stx7141_pcm_player_0_info = {
	.name = "PCM player #0",
	.ver = 6,
	.card_device = 0,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 3,
	.channels = 10,
	.fdma_initiator = 0,
	.fdma_request_line = 39, /* TODO: CHECK THIS */
	/* .pad_config set by stx7141_configure_audio() */
};

static struct stm_pad_config stx7141_pcm_player_0_pad_config = {
	.gpio_values_num = 8,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT_MUX(15, 4, 1),	/* MCLK */
		STM_PAD_PIO_OUT_MUX(15, 5, 1),	/* LRCLK */
		STM_PAD_PIO_OUT_MUX(15, 6, 1),	/* SCLK */
		STM_PAD_PIO_OUT_MUX(15, 3, 1),	/* DATA0 */
		STM_PAD_PIO_OUT_MUX(15, 7, 2),	/* DATA1 */
		STM_PAD_PIO_OUT_MUX(16, 0, 2),	/* DATA2 */
		STM_PAD_PIO_OUT_MUX(16, 1, 2),	/* DATA3 */
		STM_PAD_PIO_OUT_MUX(16, 2, 2),	/* DATA4 */
	},
};

static struct platform_device stx7141_pcm_player_0 = {
	.name          = "snd_pcm_player",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd101000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(101), -1),
	},
	.dev.platform_data = &stx7141_pcm_player_0_info,
};

static struct snd_stm_pcm_player_info stx7141_pcm_player_1_info = {
	.name = "PCM player #1",
	.ver = 6,
	.card_device = 1,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 1,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 40,
	/* .pad_config set by stx7141_configure_audio() */
};

static struct stm_pad_config stx7141_pcm_player_1_pad_config = {
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 1 for PIO15.7 & PIO16.0-2 */
		STM_PAD_SYS_CFG(50, 0, 7, 0x55),
	},
	.gpio_values_num = 4,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT(15, 0),	/* MCLK */
		STM_PAD_PIO_OUT(16, 1),	/* LRCLK */
		STM_PAD_PIO_OUT(16, 2),	/* SCLK */
		STM_PAD_PIO_OUT(15, 7),	/* DATA */
	},
};

static struct platform_device stx7141_pcm_player_1 = {
	.name          = "snd_pcm_player",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd101800, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(102), -1),
	},
	.dev.platform_data = &stx7141_pcm_player_1_info,
};

static struct platform_device stx7141_pcm_player_2 = {
	.name          = "snd_pcm_player",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104d00, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(137), -1),
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HDMI",
		.ver = 6,
		.card_device = 2,
		.fsynth_bus_id = "snd_fsynth",
		.fsynth_output = 0,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 47,
	},
};

/* SPDIF player */

static struct snd_stm_spdif_player_info stx7141_spdif_player_info = {
	.name = "SPDIF player (HDMI)",
	.ver = 4,
	.card_device = 3,
	.fsynth_bus_id = "snd_fsynth",
	.fsynth_output = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 48,
	/* .pad_config set by stx7141_configure_audio() */
};

static struct stm_pad_config stx7141_spdif_player_pad_config = {
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 1 for PIO16.3 */
		STM_PAD_SYS_CFG(50, 8, 8, 1),
	},
	.gpio_values_num = 1,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT(16, 3),
	},
};

static struct platform_device stx7141_spdif_player = {
	.name          = "snd_spdif_player",
	.id            = -1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd104c00, 0x44),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(136), -1),
	},
	.dev.platform_data = &stx7141_spdif_player_info,
};

/* I2S to SPDIF converters */

static struct platform_device stx7141_conv_i2sspdif_0 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105000, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(135), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7141_conv_i2sspdif_1 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105400, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(134), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7141_conv_i2sspdif_2 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 2,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105800, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(133), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7141_conv_i2sspdif_3 = {
	.name          = "snd_conv_i2sspdif",
	.id            = 3,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd105c00, 0x224),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(132), -1),
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct snd_stm_pcm_reader_info stx7141_pcm_reader_0_info = {
	.name = "PCM Reader #0",
	.ver = 4,
	.card_device = 4,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 41,
};

static struct stm_pad_config stx7141_pcm_reader_0_pad_config = {
	.gpio_values_num = 3,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_IN_MUX(15, 0, 1),	/* DATA */
		STM_PAD_PIO_IN_MUX(15, 1, 1),	/* LRCLK */
		STM_PAD_PIO_IN_MUX(15, 2, 1),	/* SCLK */
	},
};

static struct platform_device stx7141_pcm_reader_0 = {
	.name          = "snd_pcm_reader",
	.id            = 0,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd102000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(103), -1),
	},
	.dev.platform_data = &stx7141_pcm_reader_0_info,
};

static struct snd_stm_pcm_reader_info stx7141_pcm_reader_1_info = {
	.name = "PCM Reader #1",
	.ver = 4,
	.card_device = 5,
	.channels = 2,
	.fdma_initiator = 0,
	.fdma_request_line = 42,
};

static struct stm_pad_config stx7141_pcm_reader_1_pad_config = {
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 1 for PIO16.4-6 */
		STM_PAD_SYS_CFG(50, 9, 11, 7),
	},
	.gpio_values_num = 3,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_IN(16, 4),	/* DATA */
		STM_PAD_PIO_IN(16, 5),	/* LRCLK */
		STM_PAD_PIO_IN(16, 6),	/* SCLK */
	},
};

static struct platform_device stx7141_pcm_reader_1 = {
	.name          = "snd_pcm_reader",
	.id            = 1,
	.num_resources = 2,
	.resource      = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd103000, 0x28),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(104), -1),
	},
	.dev.platform_data = &stx7141_pcm_reader_1_info,
};

/* Devices */

static struct platform_device *stx7141_audio_devices[] __initdata = {
	&stx7141_glue,
	&stx7141_fsynth,
	&stx7141_conv_int_dac,
	&stx7141_pcm_player_0,
	&stx7141_pcm_player_1,
	&stx7141_pcm_player_2,
	&stx7141_spdif_player,
	&stx7141_conv_i2sspdif_0,
	&stx7141_conv_i2sspdif_1,
	&stx7141_conv_i2sspdif_2,
	&stx7141_conv_i2sspdif_3,
	&stx7141_pcm_reader_0,
	&stx7141_pcm_reader_1,
};

static int __init stx7141_audio_devices_setup(void)
{
	if (cpu_data->type != CPU_STX7141) {
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

	return platform_add_devices(stx7141_audio_devices,
			ARRAY_SIZE(stx7141_audio_devices));
}
device_initcall(stx7141_audio_devices_setup);

/* Configuration */

void __init stx7141_configure_audio(struct stx7141_audio_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config->pcm_player_0_output >
			stx7141_pcm_player_0_output_disabled) {
		int unused = 5 - config->pcm_player_0_output;

		stx7141_pcm_player_0_info.pad_config =
				&stx7141_pcm_player_0_pad_config;

		stx7141_pcm_player_0_pad_config.sysconf_values_num -= unused;
		stx7141_pcm_player_0_pad_config.gpio_values_num -= unused;
	}

	if (config->pcm_player_1_output_enabled) {
		/* PCM Player 0 outputs DATA1-4 are multiplexed with
		 * PCM Player 1's ones... */
		BUG_ON(config->pcm_player_0_output >
				stx7141_pcm_player_0_output_2_channels);

		stx7141_pcm_player_1_info.pad_config =
				&stx7141_pcm_player_1_pad_config;
	}

	if (config->spdif_player_output_enabled)
		stx7141_spdif_player_info.pad_config =
				&stx7141_spdif_player_pad_config;

	if (config->pcm_reader_0_input_enabled)
		stx7141_pcm_reader_0_info.pad_config =
				&stx7141_pcm_reader_0_pad_config;

	if (config->pcm_reader_1_input_enabled)
		stx7141_pcm_reader_1_info.pad_config =
				&stx7141_pcm_reader_1_pad_config;
}
