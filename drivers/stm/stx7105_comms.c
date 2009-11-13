#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/stm/pad.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx7105_asc0_pio0_pad_config = {
	.sysconf_values_num = 4, /* !!! see stx7105_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 4 for PIO0.0 (TXD) */
		STM_PAD_SYS_CFG(19, 0, 0, 1),
		STM_PAD_SYS_CFG(19, 8, 8, 1),
		/* Alt. func. 4 for PIO0.3 (RTS) */
		STM_PAD_SYS_CFG(19, 3, 3, 1),
		STM_PAD_SYS_CFG(19, 11, 11, 1),
	},
	.gpio_values_num = 4, /* !!! see stx7105_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(0, 0),	/* TX */
		STM_PAD_PIO_IN(0, 1),		/* RX */
		STM_PAD_PIO_IN(0, 4),		/* CTS */
		STM_PAD_PIO_ALT_OUT(0, 3),	/* RTS */
	},
};

static struct stm_pad_config stx7105_asc1_pio1_pad_config = {
	.sysconf_values_num = 4, /* !!! see stx7105_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 4 for PIO1.0 (TXD) */
		STM_PAD_SYS_CFG(20, 0, 0, 1),
		STM_PAD_SYS_CFG(20, 8, 8, 1),
		/* Alt. func. 4 for PIO1.3 (RTS) */
		STM_PAD_SYS_CFG(20, 3, 3, 1),
		STM_PAD_SYS_CFG(20, 11, 11, 1),
	},
	.gpio_values_num = 4, /* !!! see stx7105_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(1, 0),	/* TX */
		STM_PAD_PIO_IN(1, 1),		/* RX */
		STM_PAD_PIO_IN(1, 4),		/* CTS */
		STM_PAD_PIO_ALT_OUT(1, 3),	/* RTS */
	},
};

static struct stm_pad_config stx7105_asc2_pio4_pad_config = {
	.sysconf_values_num = 6, /* !!! see stx7105_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 3 for PIO4.0 (TXD) */
		STM_PAD_SYS_CFG(34, 0, 0, 0),
		STM_PAD_SYS_CFG(34, 8, 8, 1),
		/* uart2_rxd_src_select: 0 = PIO4.1, 1 = PIO12.1 */
		STM_PAD_SYS_CFG(7, 1, 1, 0),
		/* Alt. func. 3 for PIO4.3 (RTS) */
		STM_PAD_SYS_CFG(34, 3, 3, 0),
		STM_PAD_SYS_CFG(34, 11, 11, 1),
		/* uart2_cts_src_select: 0 = PIO4.2, 1 = PIO12.2 */
		STM_PAD_SYS_CFG(7, 2, 2, 0),
	},
	.gpio_values_num = 4, /* !!! see stx7105_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(4, 0),	/* TX */
		STM_PAD_PIO_IN(4, 1),		/* RX */
		STM_PAD_PIO_IN(4, 2),		/* CTS */
		STM_PAD_PIO_ALT_OUT(4, 3),	/* RTS */
	},
};

static struct stm_pad_config stx7105_asc2_pio12_pad_config = {
	.sysconf_values_num = 4, /* !!! see stx7105_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 4 for PIO12.0 (TXD) */
		STM_PAD_SYS_CFG(48, 16, 16, 1),
		/* uart2_rxd_src_select: 0 = PIO4.1, 1 = PIO12.1 */
		STM_PAD_SYS_CFG(7, 1, 1, 1),
		/* Alt. func. 4 for PIO12.3 (RTS) */
		STM_PAD_SYS_CFG(48, 19, 19, 1),
		/* uart2_cts_src_select: 0 = PIO4.2, 1 = PIO12.2 */
		STM_PAD_SYS_CFG(7, 2, 2, 1),
	},
	.gpio_values_num = 4, /* !!! see stx7105_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(12, 0),	/* TX */
		STM_PAD_PIO_IN(12, 1),		/* RX */
		STM_PAD_PIO_IN(12, 2),		/* CTS */
		STM_PAD_PIO_ALT_OUT(12, 3),	/* RTS */
	},
};

static struct stm_pad_config stx7105_asc3_pio5_pad_config = {
	.sysconf_values_num = 4, /* !!! see stx7105_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt. func. 3 for PIO5.0 (TXD) */
		STM_PAD_SYS_CFG(35, 0, 0, 1),
		STM_PAD_SYS_CFG(35, 8, 8, 0),
		/* Alt. func. 3 for PIO5.2 (RTS) */
		STM_PAD_SYS_CFG(35, 2, 2, 1),
		STM_PAD_SYS_CFG(35, 10, 10, 0),
	},
	.gpio_values_num = 4, /* !!! see stx7105_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_ALT_OUT(5, 0),	/* TX */
		STM_PAD_PIO_IN(5, 1),		/* RX */
		STM_PAD_PIO_IN(5, 3),		/* CTS */
		STM_PAD_PIO_ALT_OUT(5, 2),	/* RTS */
	},
};

static struct platform_device stx7105_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd030000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1160), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7105_asc0_pio0_pad_config,
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd031000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1140), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7105_asc1_pio1_pad_config
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd032000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1120), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			/* .pad_config set in stx7105_configure_asc() */
		},
	},
	[3] = {
		.name		= "stm-asc",
		/* .id set in stx7105_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd033000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1100), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7105_asc3_pio5_pad_config,
		},
	},
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
int __initdata stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num = 0;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(stx7105_asc_devices)];

void __init stx7105_configure_asc(int asc, struct stx7105_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7105_asc_devices)];
	static int tty_id;
	struct stx7105_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7105_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stx7105_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (asc == 2) {
		switch (config->routing.asc2) {
		case stx7105_asc2_pio4:
			plat_data->pad_config = &stx7105_asc2_pio4_pad_config;
			break;
		case stx7105_asc2_pio12:
			plat_data->pad_config = &stx7105_asc2_pio12_pad_config;
			break;
		default:
			BUG();
			break;
		}
	}

	if (!config->hw_flow_control) {
		struct stm_pad_config *pad_config = plat_data->pad_config;

		if (asc == 2 && config->routing.asc2 == stx7105_asc2_pio4)
			pad_config->sysconf_values_num -= 3;
		else
			pad_config->sysconf_values_num -= 2;
		pad_config->gpio_values_num -= 2;
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7105_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx7105_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for I2C/SSC mode */
static struct stm_pad_config stx7105_ssc_i2c_pad_configs[] = {
	[0] = {
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alt. functions 3 for PIO2.2 & PIO2.3 */
			STM_PAD_SYS_CFG(21, 2, 3, 0),
			STM_PAD_SYS_CFG(21, 10, 11, 3),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_BIDIR(2, 2),	/* SCL */
			STM_PAD_PIO_ALT_BIDIR(2, 3),	/* SDA */
		},
	},
	[1] = {
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 3 for PIO2.5 & PIO2.6 */
			STM_PAD_SYS_CFG(21, 5, 6, 0),
			STM_PAD_SYS_CFG(21, 13, 14, 3),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_BIDIR(2, 5),	/* SCL */
			STM_PAD_PIO_ALT_BIDIR(2, 6),	/* SDA */
		},
	},
	/* Configurations for SSC2 & SSC3 are created in
	 * stx7105_configure_ssc_*(), according to passed routing
	 * information */
};

/* Pad configuration for I2C/GPIO (temporary) mode */
static struct stm_pad_config stx7105_ssc_i2c_gpio_pad_configs[] = {
	[0] = {
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(2, 2),	/* SCL */
			STM_PAD_PIO_BIDIR(2, 3),	/* SDA */
		},
	},
	[1] = {
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(2, 5),	/* SCL */
			STM_PAD_PIO_BIDIR(2, 6),	/* SDA */
		},
	},
	/* Configurations for SSC2 & SSC3 are created in
	 * stx7105_configure_ssc_*(), according to passed routing
	 * information */
};

/* Pad configuration to revert to I2C/SSC mode from I2C/GPIO mode */
static struct stm_pad_config stx7105_ssc_i2c_ssc_pad_configs[] = {
	[0] = {
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_BIDIR(2, 2),	/* SCL */
			STM_PAD_PIO_ALT_BIDIR(2, 3),	/* SDA */
		},
	},
	[1] = {
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_BIDIR(2, 5),	/* SCL */
			STM_PAD_PIO_ALT_BIDIR(2, 6),	/* SDA */
		},
	},
	/* Configurations for SSC2 & SSC3 are created in
	 * stx7105_configure_ssc_*(), according to passed routing
	 * information */
};


/* Pad configuration for SPI/SSC mode */
static struct stm_pad_config stx7105_ssc_spi_pad_configs[] = {
	[0] = {
		.sysconf_values_num = 3,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 3 for PIO2.2-4 */
			STM_PAD_SYS_CFG(21, 2, 4, 0),
			STM_PAD_SYS_CFG(21, 10, 12, 3),
			/* ssc0_mrst_in_sel: 0 = PIO2.3, 1 = PIO2.4 */
			STM_PAD_SYS_CFG(16, 0, 0, 1),
		},
		.gpio_values_num = 3,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_BIDIR(2, 2),	/* SCK */
			STM_PAD_PIO_ALT_BIDIR(2, 3),	/* MOSI */
			STM_PAD_PIO_IN(2, 4),		/* MISO */
		},
	},
	[1] = {
		.sysconf_values_num = 3,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 3 for PIO2.2-5 */
			STM_PAD_SYS_CFG(21, 5, 7, 0),
			STM_PAD_SYS_CFG(21, 13, 15, 3),
			/* ssc1_mrst_in_sel: 0 = PIO2.6, 1 = PIO2.7 */
			STM_PAD_SYS_CFG(16, 3, 3, 1),
		},
		.gpio_values_num = 3,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_BIDIR(2, 5),	/* SCK */
			STM_PAD_PIO_ALT_BIDIR(2, 6),	/* MOSI */
			STM_PAD_PIO_IN(2, 7),		/* MISO */
		},
	},
	/* Configurations for SSC2 & SSC3 are created in
	 * stx7105_configure_ssc_*(), according to passed routing
	 * information */
};

static struct platform_device stx7105_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd040000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10e0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7105_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd041000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10c0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx7105_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd042000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x10a0), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .gpio_* & .pad_config_* set in
			 * stx7105_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stx7105_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd043000, 0x110),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1080), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .gpio_* & .pad_config_* set in
			 * stx7105_configure_ssc_*() */
		},
	},
};

static int __initdata stx7105_ssc_configured[ARRAY_SIZE(stx7105_ssc_devices)];

int __init stx7105_configure_ssc_i2c(int ssc, struct stx7105_ssc_config *config)
{
	static int i2c_busnum;
	struct stx7105_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;
	struct stm_pad_config *pad_config_ssc;
	struct stm_pad_config *pad_config_gpio;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7105_ssc_devices));

	BUG_ON(stx7105_ssc_configured[ssc]);
	stx7105_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7105_ssc_devices[ssc].name = "i2c-stm";
	stx7105_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7105_ssc_devices[ssc].dev.platform_data;

	switch (ssc) {
	case 0:
	case 1:
		pad_config = &stx7105_ssc_i2c_pad_configs[ssc];
		pad_config_ssc = &stx7105_ssc_i2c_ssc_pad_configs[ssc];
		pad_config_gpio = &stx7105_ssc_i2c_gpio_pad_configs[ssc];
		break;
	case 2:
		pad_config = stm_pad_config_alloc(0, 8, 2);
		pad_config_ssc = stm_pad_config_alloc(0, 0, 2);
		pad_config_gpio = stm_pad_config_alloc(0, 0, 2);

		/* SCL */
		switch (config->routing.ssc2.sclk) {
		case stx7105_ssc2_sclk_pio3_4:
			/* ssc2_sclk_in: 01 = PIO3.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 1);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 25, 4, 4, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 12, 12, 0);

			stm_pad_config_add_pio(pad_config, 3, 4,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 3, 4,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 3, 4,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc2_sclk_pio12_0:
			/* ssc2_sclk_in: 10 = PIO12.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 2);
			/* Alt. func. 3 */
			stm_pad_config_add_sys_cfg(pad_config, 48, 0, 0, 0);
			stm_pad_config_add_sys_cfg(pad_config, 48, 8, 8, 1);
			stm_pad_config_add_sys_cfg(pad_config, 48, 16, 16, 0);

			stm_pad_config_add_pio(pad_config, 12, 0,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 12, 0,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 12, 0,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc2_sclk_pio13_4:
			/* ssc2_sclk_in: 11 = PIO13.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 3);
			/* Alt. func. 2bis */
			stm_pad_config_add_sys_cfg(pad_config, 49, 4, 4, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 12, 12, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 20, 20, 0);

			stm_pad_config_add_pio(pad_config, 13, 4,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 4,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 13, 4,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		}

		/* SDA */
		switch (config->routing.ssc2.mtsr) {
		case stx7105_ssc2_mtsr_pio2_0:
			/* ssc2_mtsr_in: 00 = PIO2.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 0);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 21, 0, 0, 0);
			stm_pad_config_add_sys_cfg(pad_config, 21, 8, 8, 1);

			stm_pad_config_add_pio(pad_config, 2, 0,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 2, 0,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 2, 0,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc2_mtsr_pio3_5:
			/* ssc2_mtsr_in: 01 = PIO3.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 1);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 25, 5, 5, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 13, 13, 0);

			stm_pad_config_add_pio(pad_config, 3, 5,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 3, 5,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 3, 5,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc2_mtsr_pio12_1:
			/* ssc2_mtsr_in: 10 = PIO12.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 2);
			/* Alt. func. 3 */
			stm_pad_config_add_sys_cfg(pad_config, 48, 1, 1, 0);
			stm_pad_config_add_sys_cfg(pad_config, 48, 9, 9, 1);
			stm_pad_config_add_sys_cfg(pad_config, 48, 17, 17, 0);

			stm_pad_config_add_pio(pad_config, 12, 1,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 12, 1,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 12, 1,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc2_mtsr_pio13_5:
			/* ssc2_mtsr_in: 11 = PIO13.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 9, 10, 3);
			/* Alt. func. 2bis */
			stm_pad_config_add_sys_cfg(pad_config, 49, 5, 5, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 13, 13, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 21, 21, 0);

			stm_pad_config_add_pio(pad_config, 13, 5,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 5,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 13, 5,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		}

		break;
	case 3:
		pad_config = stm_pad_config_alloc(0, 8, 2);
		pad_config_ssc = stm_pad_config_alloc(0, 0, 2);
		pad_config_gpio = stm_pad_config_alloc(0, 0, 2);

		/* SCL */
		switch (config->routing.ssc3.sclk) {
		case stx7105_ssc3_sclk_pio3_6:
			/* ssc3_sclk_in: 01 = PIO3.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 1);
			/* Alt. func. 3bis */
			stm_pad_config_add_sys_cfg(pad_config, 25, 6, 6, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 14, 14, 0);

			stm_pad_config_add_pio(pad_config, 3, 6,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 4,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 3, 6,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc3_sclk_pio13_2:
			/* ssc3_sclk_in: 10 = PIO13.2 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 2);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 18, 18, 1);

			stm_pad_config_add_pio(pad_config, 13, 2,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 2,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 13, 2,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc3_sclk_pio13_6:
			/* ssc3_sclk_in: 11 = PIO13.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 3);
			/* Alt. func. 2ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 6, 6, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 14, 14, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 22, 22, 0);

			stm_pad_config_add_pio(pad_config, 13, 6,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 6,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 13, 6,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		}

		/* SDA */
		switch (config->routing.ssc3.mtsr) {
		case stx7105_ssc3_mtsr_pio2_1:
			/* ssc3_mtsr_in: 00 = PIO2.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 0);
			/* Alt. func. 3#4 */
			stm_pad_config_add_sys_cfg(pad_config, 21, 1, 1, 0);
			stm_pad_config_add_sys_cfg(pad_config, 21, 9, 9, 1);

			stm_pad_config_add_pio(pad_config, 2, 1,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 2, 1,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 2, 1,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc3_mtsr_pio3_7:
			/* ssc3_mtsr_in: 01 = PIO3.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 1);
			/* Alt. func. 3bis */
			stm_pad_config_add_sys_cfg(pad_config, 25, 7, 7, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 15, 15, 0);

			stm_pad_config_add_pio(pad_config, 3, 7,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 3, 7,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 3, 7,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc3_mtsr_pio13_3:
			/* ssc3_mtsr_in: 10 = PIO13.3 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 2);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 3, 3, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 11, 11, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 19, 19, 0);

			stm_pad_config_add_pio(pad_config, 13, 3,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 3,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 13, 3,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		case stx7105_ssc3_mtsr_pio13_7:
			/* ssc3_mtsr_in: 11 = PIO13.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 16, 17, 3);
			/* Alt. func. 2ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 7, 7, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 15, 15, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 23, 23, 0);

			stm_pad_config_add_pio(pad_config, 13, 7,
					STM_GPIO_DIRECTION_ALT_BIDIR);

			stm_pad_config_add_pio(pad_config_gpio, 13, 7,
					STM_GPIO_DIRECTION_BIDIR);

			stm_pad_config_add_pio(pad_config_ssc, 13, 7,
					STM_GPIO_DIRECTION_ALT_BIDIR);
			break;
		}

		break;
	default:
		BUG();
		pad_config_ssc = NULL; /* Keep the compiler happy ;-) */
		pad_config_gpio = NULL; /* Keep the compiler happy ;-) */
		break;
	}

	plat_data->pad_config = pad_config;
	plat_data->pad_config_ssc = pad_config_ssc;
	plat_data->pad_config_gpio = pad_config_gpio;

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx7105_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7105_configure_ssc_spi(int ssc, struct stx7105_ssc_config *config)
{
	static int spi_busnum;
	struct stx7105_ssc_config default_config = {};
	struct stm_plat_ssc_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7105_ssc_devices));

	BUG_ON(stx7105_ssc_configured[ssc]);
	stx7105_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7105_ssc_devices[ssc].name = "spi-stm-ssc";
	stx7105_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7105_ssc_devices[ssc].dev.platform_data;

	switch (ssc) {
	case 0:
	case 1:
		pad_config = &stx7105_ssc_spi_pad_configs[ssc];
		break;
	case 2:
		pad_config = stm_pad_config_alloc(0, 8, 3);

		/* SCK */
		switch (config->routing.ssc2.sclk) {
		case stx7105_ssc2_sclk_pio3_4:
			/* ssc2_sclk_in: 01 = PIO3.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 1);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 25, 4, 4, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 12, 12, 0);
			stm_pad_config_add_pio(pad_config, 3, 4,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc2_sclk_pio12_0:
			/* ssc2_sclk_in: 10 = PIO12.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 2);
			/* Alt. func. 3 */
			stm_pad_config_add_sys_cfg(pad_config, 48, 0, 0, 0);
			stm_pad_config_add_sys_cfg(pad_config, 48, 8, 8, 1);
			stm_pad_config_add_sys_cfg(pad_config, 48, 16, 16, 0);
			stm_pad_config_add_pio(pad_config, 12, 0,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc2_sclk_pio13_4:
			/* ssc2_sclk_in: 11 = PIO13.4 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 11, 12, 3);
			/* Alt. func. 2bis */
			stm_pad_config_add_sys_cfg(pad_config, 49, 4, 4, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 12, 12, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 20, 20, 0);
			stm_pad_config_add_pio(pad_config, 13, 4,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		}

		/* MOSI */
		switch (config->routing.ssc2.mtsr) {
		case stx7105_ssc2_mtsr_pio2_0:
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 21, 0, 0, 0);
			stm_pad_config_add_sys_cfg(pad_config, 21, 8, 8, 1);
			stm_pad_config_add_pio(pad_config, 2, 0,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc2_mtsr_pio3_5:
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 25, 5, 5, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 13, 13, 0);
			stm_pad_config_add_pio(pad_config, 3, 5,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc2_mtsr_pio12_1:
			/* Alt. func. 3 */
			stm_pad_config_add_sys_cfg(pad_config, 48, 1, 1, 0);
			stm_pad_config_add_sys_cfg(pad_config, 48, 9, 9, 1);
			stm_pad_config_add_sys_cfg(pad_config, 48, 17, 17, 0);
			stm_pad_config_add_pio(pad_config, 12, 1,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc2_mtsr_pio13_5:
			/* Alt. func. 2bis */
			stm_pad_config_add_sys_cfg(pad_config, 49, 5, 5, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 13, 13, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 21, 21, 0);
			stm_pad_config_add_pio(pad_config, 13, 5,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		}

		/* MISO */
		switch (config->routing.ssc2.mrst) {
		case stx7105_ssc2_mrst_pio2_0:
			/* ssc2_mrst_in: 00 = PIO2.0 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 0);
			stm_pad_config_add_pio(pad_config, 2, 0,
					STM_GPIO_DIRECTION_IN);
			break;
		case stx7105_ssc2_mrst_pio3_5:
			/* ssc2_mrst_in: 01 = PIO3.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 1);
			stm_pad_config_add_pio(pad_config, 3, 5,
					STM_GPIO_DIRECTION_IN);
			break;
		case stx7105_ssc2_mrst_pio12_1:
			/* ssc2_mrst_in: 10 = PIO12.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 2);
			stm_pad_config_add_pio(pad_config, 12, 1,
					STM_GPIO_DIRECTION_IN);
			break;
		case stx7105_ssc2_mrst_pio13_5:
			/* ssc2_mrst_in: 11 = PIO13.5 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 7, 8, 3);
			stm_pad_config_add_pio(pad_config, 13, 5,
					STM_GPIO_DIRECTION_IN);
			break;
		}

		break;
	case 3:
		pad_config = stm_pad_config_alloc(0, 8, 3);

		/* SCK */
		switch (config->routing.ssc3.sclk) {
		case stx7105_ssc3_sclk_pio3_6:
			/* ssc3_sclk_in: 00 = PIO3.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 0);
			/* Alt. func. 3bis */
			stm_pad_config_add_sys_cfg(pad_config, 25, 6, 6, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 14, 14, 0);
			stm_pad_config_add_pio(pad_config, 3, 6,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc3_sclk_pio13_2:
			/* ssc3_sclk_in: 01 = PIO13.2 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 1);
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 18, 18, 1);
			stm_pad_config_add_pio(pad_config, 13, 2,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc3_sclk_pio13_6:
			/* ssc3_sclk_in: 1x = PIO13.6 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 18, 19, 2);
			/* Alt. func. 2ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 6, 6, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 14, 14, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 22, 22, 0);
			stm_pad_config_add_pio(pad_config, 13, 6,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		}

		/* MOSI */
		switch (config->routing.ssc3.mtsr) {
		case stx7105_ssc3_mtsr_pio2_1:
			/* Alt. func. 3#4 */
			stm_pad_config_add_sys_cfg(pad_config, 21, 1, 1, 0);
			stm_pad_config_add_sys_cfg(pad_config, 21, 9, 9, 1);
			stm_pad_config_add_pio(pad_config, 2, 1,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc3_mtsr_pio3_7:
			/* Alt. func. 3bis */
			stm_pad_config_add_sys_cfg(pad_config, 25, 7, 7, 1);
			stm_pad_config_add_sys_cfg(pad_config, 25, 15, 15, 0);
			stm_pad_config_add_pio(pad_config, 3, 7,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc3_mtsr_pio13_3:
			/* Alt. func. 3ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 3, 3, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 11, 11, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 19, 19, 0);
			stm_pad_config_add_pio(pad_config, 13, 3,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		case stx7105_ssc3_mtsr_pio13_7:
			/* Alt. func. 2ter */
			stm_pad_config_add_sys_cfg(pad_config, 49, 7, 7, 1);
			stm_pad_config_add_sys_cfg(pad_config, 49, 15, 15, 0);
			stm_pad_config_add_sys_cfg(pad_config, 49, 23, 23, 0);
			stm_pad_config_add_pio(pad_config, 13, 7,
					STM_GPIO_DIRECTION_ALT_OUT);
			break;
		}

		/* MISO */
		switch (config->routing.ssc3.mrst) {
		case stx7105_ssc3_mrst_pio2_1:
			/* ssc3_mrst_in: 00 = PIO2.1 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 0);
			stm_pad_config_add_pio(pad_config, 2, 1,
					STM_GPIO_DIRECTION_IN);
			break;
		case stx7105_ssc3_mrst_pio3_7:
			/* ssc3_mrst_in: 01 = PIO3.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 1);
			stm_pad_config_add_pio(pad_config, 3, 7,
					STM_GPIO_DIRECTION_IN);
			break;
		case stx7105_ssc3_mrst_pio13_3:
			/* ssc3_mrst_in: 10 = PIO13.3 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 2);
			stm_pad_config_add_pio(pad_config, 13, 3,
					STM_GPIO_DIRECTION_IN);
			break;
		case stx7105_ssc3_mrst_pio13_7:
			/* ssc3_mrst_in: 11 = PIO13.7 */
			stm_pad_config_add_sys_cfg(pad_config, 16, 14, 15, 3);
			stm_pad_config_add_pio(pad_config, 13, 7,
					STM_GPIO_DIRECTION_IN);
			break;
		}

		break;
	default:
		BUG();
		pad_config = NULL; /* Keep the compiler happy ;-) */
		break;
	}

	plat_data->spi_chipselect = config->spi_chipselect;
	plat_data->pad_config_ssc = pad_config;

	platform_device_register(&stx7105_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx7105_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd018000, 0x234),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x11a0), -1),
	},
	.dev.platform_data = &(struct stm_plat_lirc_data) {
		/* The clock settings will be calculated by
		 * the driver from the system clock */
		.irbclock	= 0, /* use current_cpu data */
		.irbclkdiv	= 0, /* automatically calculate */
		.irbperiodmult	= 0,
		.irbperioddiv	= 0,
		.irbontimemult	= 0,
		.irbontimediv	= 0,
		.irbrxmaxperiod = 0x5000,
		.sysclkdiv	= 1,
		.rxpolarity	= 1,
	},
};

void __init stx7105_configure_lirc(struct stx7105_lirc_config *config)
{
	static int configured;
	struct stx7105_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx7105_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(3, 4, 3);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled || config->tx_od_enabled;
	plat_data->pads = pad_config;

	switch (config->rx_mode) {
	case stx7105_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx7105_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_label_number(pad_config, "PIO3", 0);
		stm_pad_config_add_pio(pad_config, 3, 0, STM_GPIO_DIRECTION_IN);
		break;
	case stx7105_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_label_number(pad_config, "PIO3", 1);
		stm_pad_config_add_pio(pad_config, 3, 1, STM_GPIO_DIRECTION_IN);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled) {
		stm_pad_config_add_label_number(pad_config, "PIO3", 2);
		/* Alternative function 3 (IRB_DATA_OUT) */
		stm_pad_config_add_sys_cfg(pad_config, 25, 2, 2, 0);
		stm_pad_config_add_sys_cfg(pad_config, 25, 10, 10, 1);
		stm_pad_config_add_pio(pad_config, 3, 2,
				STM_GPIO_DIRECTION_ALT_OUT);
	};

	if (config->tx_od_enabled) {
		stm_pad_config_add_label_number(pad_config, "PIO3", 3);
		/* Alternative function 3 (IRB_DATA_OUT_OD) */
		stm_pad_config_add_sys_cfg(pad_config, 25, 3, 3, 0);
		stm_pad_config_add_sys_cfg(pad_config, 25, 11, 11, 1);
		stm_pad_config_add_pio(pad_config, 3, 3,
				STM_GPIO_DIRECTION_ALT_OUT);
	};

	platform_device_register(&stx7105_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_pad_config stx7105_pwm_out0_pio4_4_pad_config = {
	.sysconf_values_num = 2,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alternative function 3 for PIO4.4 */
		STM_PAD_SYS_CFG(34, 4, 4, 0),
		STM_PAD_SYS_CFG(34, 12, 12, 1),
	},
	.gpio_values_num = 1,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT(4, 4),
	},
};

static struct stm_pad_config stx7105_pwm_out0_pio13_0_pad_config = {
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alternative function 3 for PIO13.0 */
		STM_PAD_SYS_CFG(49, 16, 16, 1),
	},
	.gpio_values_num = 1,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT(13, 0),
	},
};

static struct stm_pad_config stx7105_pwm_out1_pio4_5_pad_config = {
	.sysconf_values_num = 2,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alternative function 3 for PIO4.5 */
		STM_PAD_SYS_CFG(34, 5, 5, 0),
		STM_PAD_SYS_CFG(34, 13, 13, 1),
	},
	.gpio_values_num = 1,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT(4, 5),
	},
};

static struct stm_pad_config stx7105_pwm_out1_pio13_1_pad_config = {
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alternative function 3 for PIO13.1 */
		STM_PAD_SYS_CFG(49, 17, 17, 1),
	},
	.gpio_values_num = 1,
	.gpio_values = (struct stm_pad_gpio_value []) {
		STM_PAD_PIO_OUT(13, 1),
	},
};

/* Set in stx7105_configure_pwm() */
static struct stm_plat_pwm_data stx7105_pwm_platform_data;

static struct platform_device stx7105_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x11c0), -1),
	},
	.dev.platform_data = &stx7105_pwm_platform_data,
};

void __init stx7105_configure_pwm(struct stx7105_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		switch (config->out0) {
		case stx7105_pwm_out0_disabled:
			/* Nothing to do... */
			break;
		case stx7105_pwm_out0_pio4_4:
			stx7105_pwm_platform_data.channel_enabled[0] = 1;
			stx7105_pwm_platform_data.channel_pad_config[0]	=
					&stx7105_pwm_out0_pio4_4_pad_config;
			break;
		case stx7105_pwm_out0_pio13_0:
			stx7105_pwm_platform_data.channel_enabled[0] = 1;
			stx7105_pwm_platform_data.channel_pad_config[0]	=
					&stx7105_pwm_out0_pio13_0_pad_config;
			break;
		default:
			BUG();
			break;
		}

		switch (config->out1) {
		case stx7105_pwm_out1_disabled:
			/* Nothing to do... */
			break;
		case stx7105_pwm_out1_pio4_5:
			stx7105_pwm_platform_data.channel_enabled[1] = 1;
			stx7105_pwm_platform_data.channel_pad_config[1]	=
					&stx7105_pwm_out1_pio4_5_pad_config;
			break;
		case stx7105_pwm_out1_pio13_1:
			stx7105_pwm_platform_data.channel_enabled[1] = 1;
			stx7105_pwm_platform_data.channel_pad_config[1]	=
					&stx7105_pwm_out1_pio13_1_pad_config;
			break;
		default:
			BUG();
			break;
		}
	}

	platform_device_register(&stx7105_pwm_device);
}
