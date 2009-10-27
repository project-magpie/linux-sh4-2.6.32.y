#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7141.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx7141_asc0_mcard_pad_config = {
	.labels_num = 2, /* !!! see stx7141_configure_asc() */
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_RANGE("MCARDMDI", 0, 1),
		STM_PAD_LABEL_RANGE("MCARDMDI", 3, 4),
	},
	.sysconf_values_num = 4, /* !!! see stx7141_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* MCARD_*: M-Card (0) or glue (1) */
		STM_PAD_SYS_CFG(36, 16, 16, 1),
		/* ASC0_TXD: output enabled, no pull-up, not open drain */
		STM_PAD_SYS_CFG(36, 19, 21, 2),
		/* ASC0_RXD: input */
		STM_PAD_SYS_CFG(36, 22, 24, 0),
		/* ASC0_CTS & ASC0_RTS: ??? */
		STM_PAD_SYS_CFG(36, 17, 18, 2),
	},
};

static struct stm_pad_config stx7141_asc1_pio10_pad_config = {
	.labels_num = 2, /* !!! see stx7141_configure_asc() */
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_RANGE("PIO10", 0, 1),
		STM_PAD_LABEL_RANGE("PIO10", 2, 3),
	},
	.sysconf_values_num = 5, /* !!! see stx7141_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* ASC1_RXD/ASC1_CTS: from PIO10 (0) or from MCard (1) */
		STM_PAD_SYS_CFG(36, 29, 29, 0),
		/* PIO10[0]: alternative function 3 (ASC1_TXD) */
		STM_PAD_SYS_CFG(46, 6, 7, 3),
		/* PIO10[1]: alternative function 3 (ASC1_RXD) */
		STM_PAD_SYS_CFG(46, 8, 9, 3),
		/* PIO10[2]: alternative function 3 (ASC1_CTS) */
		STM_PAD_SYS_CFG(46, 10, 11, 3),
		/* PIO10[3]: alternative function 3 (ASC1_RTS) */
		STM_PAD_SYS_CFG(46, 12, 13, 3),
	},
	.gpio_values_num = 4, /* !!! see stx7141_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* TX */
		STM_PAD_PIO_OUT(10, 0),
		/* RX */
		STM_PAD_PIO_IN(10, 1),
		/* CTS */
		STM_PAD_PIO_IN(10, 2),
		/* RTS */
		STM_PAD_PIO_OUT(10, 3),
	},
};

static struct stm_pad_config stx7141_asc1_mcard_pad_config = {
	.labels_num = 2,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_STRINGS("MCARD", "MDO1", "MDO2", "MDO4", "MDO5"),
	},
	.sysconf_values_num = 5,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* MCARD_*: M-Card (0) or glue (1) */
		STM_PAD_SYS_CFG(36, 16, 16, 1),
		/* ASC1_TXD/ASC1_RTS: output enabled */
		STM_PAD_SYS_CFG(36, 15, 15, 1),
		/* ASC1_RXD/ASC1_CTS: from PIO10 (0) or from MCard (1) */
		STM_PAD_SYS_CFG(36, 29, 29, 1),
		/* ASC1_RXD: input */
		STM_PAD_SYS_CFG(36, 14, 14, 0),
		/* ASC1_RTS: input */
		STM_PAD_SYS_CFG(36, 13, 13, 0),
	},
};

static struct stm_pad_config stx7141_asc2_pio1_pad_config = {
	.labels_num = 2, /* !!! see stx7141_configure_asc() */
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_RANGE("PIO1", 0, 1),
		STM_PAD_LABEL_RANGE("PIO1", 2, 3),
	},
	.sysconf_values_num = 5, /* !!! see stx7141_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* ASC2_RXD/ASC2_CTS: from PIO1 (0) or from PIO6 (3) */
		STM_PAD_SYS_CFG(36, 30, 31, 0),
		/* PIO1[0]: alternative function 3 (ASC2_TXD) */
		STM_PAD_SYS_CFG(19, 0, 1, 3),
		/* PIO1[1]: alternative function 3 (ASC2_RXD) */
		STM_PAD_SYS_CFG(19, 2, 3, 3),
		/* PIO1[2]: alternative function 3 (ASC2_CTS) */
		STM_PAD_SYS_CFG(19, 4, 5, 3),
		/* PIO1[3]: alternative function 3 (ASC2_RTS) */
		STM_PAD_SYS_CFG(19, 6, 7, 3),
	},
	.gpio_values_num = 4, /* !!! see stx7141_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* TX */
		STM_PAD_PIO_OUT(1, 0),
		/* RX */
		STM_PAD_PIO_IN(1, 1),
		/* CTS */
		STM_PAD_PIO_IN(1, 2),
		/* RTS */
		STM_PAD_PIO_OUT(1, 3),
	},
};

static struct stm_pad_config stx7141_asc2_pio6_pad_config = {
	.labels_num = 2, /* !!! see stx7141_configure_asc() */
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_RANGE("PIO6", 0, 1),
		STM_PAD_LABEL_RANGE("PIO6", 2, 3),
	},
	.sysconf_values_num = 2, /* !!! see stx7141_configure_asc() */
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* ASC2_RXD/ASC2_CTS: from PIO1 (0) or from PIO6 (3) */
		STM_PAD_SYS_CFG(36, 30, 31, 3),
		/* PIO6[0]: alternative function 3 (ASC2_TXD) */
		STM_PAD_SYS_CFG(20, 27, 28, 3),
		/* PIO6[1]: alternative function 3 (ASC2_RXD) */
		STM_PAD_SYS_CFG(20, 29, 30, 3),
		/* PIO6[2]: alternative function 3 (ASC2_CTS) */
		STM_PAD_SYS_CFG(25, 0, 1, 3),
		/* PIO6[3]: alternative function 3 (ASC2_RTS) */
		STM_PAD_SYS_CFG(25, 2, 3, 3),
	},
	.gpio_values_num = 4, /* !!! see stx7141_configure_asc() */
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* TX */
		STM_PAD_PIO_OUT(6, 0),
		/* RX */
		STM_PAD_PIO_IN(6, 1),
		/* CTS */
		STM_PAD_PIO_IN(6, 2),
		/* RTS */
		STM_PAD_PIO_OUT(6, 3),
	},
};

static struct platform_device stx7141_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx7141_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd030000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(76), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx7141_asc0_mcard_pad_config,
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx7141_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd031000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(77), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			/* .pad_config set in stx7141_configure_asc() */
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx7141_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd032000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(78), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			/* .pad_config set in stx7141_configure_asc() */
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
		*stm_asc_configured_devices[ARRAY_SIZE(stx7141_asc_devices)];

void __init stx7141_configure_asc(int asc, struct stx7141_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7141_asc_devices)];
	static int tty_id;
	struct stx7141_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;
	struct stm_pad_config *pad_config = NULL;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7141_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stx7141_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	switch (asc) {
	case 0:
		pad_config = &stx7141_asc0_mcard_pad_config;
		if (!config->hw_flow_control) {
			pad_config->labels_num--;
			pad_config->sysconf_values_num--;
		}
		BUG_ON(config->routing.asc0 != stx7141_asc0_mcard);
		break;
	case 1:
		switch (config->routing.asc1) {
		case stx7141_asc1_pio10:
			pad_config = &stx7141_asc1_pio10_pad_config;
			if (!config->hw_flow_control) {
				pad_config->labels_num--;
				pad_config->sysconf_values_num -= 2;
				pad_config->gpio_values_num -= 2;
			}
			break;
		case stx7141_asc1_mcard:
			pad_config = &stx7141_asc1_mcard_pad_config;
			/* The HW flow control will be available anyway */
			WARN_ON(!config->hw_flow_control);
			break;
		default:
			BUG();
			break;
		}
		break;
	case 2:
		switch (config->routing.asc2) {
		case stx7141_asc2_pio1:
			pad_config = &stx7141_asc2_pio1_pad_config;
			break;
		case stx7141_asc2_pio6:
			pad_config = &stx7141_asc2_pio6_pad_config;
			break;
		default:
			BUG();
			break;
		}
		if (!config->hw_flow_control) {
			pad_config->labels_num--;
			pad_config->sysconf_values_num -= 2;
			pad_config->gpio_values_num -= 2;
		}
		break;
	default:
		BUG();
		break;
	}

	plat_data->pad_config = pad_config;

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7141_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx7141_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* WARNING! SSCs were numbered starting from 1 in early documents.
 * Later it was changed and this approach is used below,
 * so the first SSC is SSC0 (zero). */

/* Pad configuration for I2C/SSC mode */
static struct stm_pad_config stx7141_ssc_i2c_ssc_pad_configs[] = {
	[0] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 0, 1),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO2.0 & PIO2.1 */
			STM_PAD_SYS_CFG(19, 12, 13, 3),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(2, 0), /* SCL */
			STM_PAD_PIO_BIDIR(2, 1), /* SDA */
		},
	},
	[1] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 3, 4),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO2.3 & PIO2.4 */
			STM_PAD_SYS_CFG(19, 15, 16, 3),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(2, 3), /* SCL */
			STM_PAD_PIO_BIDIR(2, 4), /* SDA */
		},
	},
	[2] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 6, 7),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO2.6 & PIO2.7 */
			STM_PAD_SYS_CFG(19, 18, 19, 3),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(2, 6), /* SCL */
			STM_PAD_PIO_BIDIR(2, 7), /* SDA */
		},
	},
	[3] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 0, 1),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO3.0 & PIO3.1 */
			STM_PAD_SYS_CFG(19, 20, 21, 3),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(3, 0), /* SCL */
			STM_PAD_PIO_BIDIR(3, 1), /* SDA */
		},
	},
	[4] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 2, 3),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO3.2 & PIO3.3 */
			STM_PAD_SYS_CFG(19, 22, 25, 5),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(2, 2), /* SCL */
			STM_PAD_PIO_BIDIR(2, 3), /* SDA */
		},
	},
	[5] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 4, 5),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO3.4 & PIO3.5 */
			STM_PAD_SYS_CFG(19, 26, 29, 5),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(3, 4), /* SCL */
			STM_PAD_PIO_BIDIR(3, 5), /* SDA */
		},
	},
	[6] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO4", 0, 1),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO4.0 & PIO4.1 */
			STM_PAD_SYS_CFG(20, 1, 4, 5),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_BIDIR(4, 0), /* SCL */
			STM_PAD_PIO_BIDIR(4, 1), /* SDA */
		},
	},
};

/* Pad configuration for I2C/GPIO (temporary) mode */
static struct stm_pad_config stx7141_ssc_i2c_gpio_pad_configs[] = {
	[0] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 0, 1),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO2.0 & PIO2.1 */
			STM_PAD_SYS_CFG(19, 12, 13, 0),
		},
	},
	[1] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 3, 4),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO2.3 & PIO2.4 */
			STM_PAD_SYS_CFG(19, 15, 16, 0),
		},
	},
	[2] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 6, 7),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO2.6 & PIO2.7 */
			STM_PAD_SYS_CFG(19, 18, 19, 0),
		},
	},
	[3] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 0, 1),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO3.0 & PIO3.1 */
			STM_PAD_SYS_CFG(19, 20, 21, 0),
		},
	},
	[4] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 2, 3),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO3.2 & PIO3.3 */
			STM_PAD_SYS_CFG(19, 22, 25, 0),
		},
	},
	[5] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 4, 5),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO3.4 & PIO3.5 */
			STM_PAD_SYS_CFG(19, 26, 29, 0),
		},
	},
	[6] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO4", 0, 1),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* PIO mode for PIO4.0 & PIO4.1 */
			STM_PAD_SYS_CFG(20, 1, 4, 0),
		},
	},
};

/* Pad configuration for SPI/SSC mode */
static struct stm_pad_config stx7141_ssc_spi_pad_configs[] = {
	[0] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 0, 2),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO2.0-2*/
			STM_PAD_SYS_CFG(19, 12, 14, 7),
		},
		.gpio_values_num = 3,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_OUT(2, 0), /* SCK */
			STM_PAD_PIO_OUT(2, 1), /* MOSI */
			STM_PAD_PIO_IN(2, 2),  /* MISO */
		},
	},
	[1] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO2", 3, 5),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO2.3-5*/
			STM_PAD_SYS_CFG(19, 15, 17, 7),
		},
		.gpio_values_num = 3,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_OUT(2, 3), /* SCK */
			STM_PAD_PIO_OUT(2, 4), /* MOSI */
			STM_PAD_PIO_IN(2, 5),  /* MISO */
		},
	},
	[5] = {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO3", 4, 6),
		},
		.sysconf_values_num = 1,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alternative functions 1 for PIO3.4-6*/
			STM_PAD_SYS_CFG(19, 26, 31, 0x15),
		},
		.gpio_values_num = 3,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_OUT(3, 4), /* SCK */
			STM_PAD_PIO_OUT(3, 5), /* MOSI */
			STM_PAD_PIO_IN(3, 6),  /* MISO */
		},
	},
};

static struct platform_device stx7141_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd040000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(69), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(2, 0),
			.gpio_mtsr = stm_gpio(2, 1),
			.gpio_mrst = stm_gpio(2, 2),
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd041000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(70), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(2, 3),
			.gpio_mtsr = stm_gpio(2, 4),
			.gpio_mrst = stm_gpio(2, 5),
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd042000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(71), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(2, 6),
			.gpio_mtsr = stm_gpio(2, 7),
			.gpio_mrst = STM_GPIO_INVALID,
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
	[3] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd043000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(72), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(3, 0),
			.gpio_mtsr = stm_gpio(3, 1),
			.gpio_mrst = STM_GPIO_INVALID,
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
	[4] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd044000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(73), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(3, 2),
			.gpio_mtsr = stm_gpio(3, 3),
			.gpio_mrst = STM_GPIO_INVALID,
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
	[5] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd045000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(74), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(3, 4),
			.gpio_mtsr = stm_gpio(3, 5),
			.gpio_mrst = stm_gpio(3, 6),
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
	[6] = {
		/* .name & .id set in stx7141_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd046000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(75), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.gpio_sclk = stm_gpio(4, 0),
			.gpio_mtsr = stm_gpio(4, 1),
			.gpio_mrst = STM_GPIO_INVALID,
			/* .pad_config_* set in stx7141_configure_ssc_*() */
		},
	},
};

static int __initdata stx7141_ssc_configured[ARRAY_SIZE(stx7141_ssc_devices)];

int __init stx7141_configure_ssc_i2c(int ssc)
{
	static int i2c_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7141_ssc_devices));

	BUG_ON(stx7141_ssc_configured[ssc]);
	stx7141_ssc_configured[ssc] = 1;

	stx7141_ssc_devices[ssc].name = "i2c-stm";
	stx7141_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7141_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config_ssc = &stx7141_ssc_i2c_ssc_pad_configs[ssc];
	plat_data->pad_config_gpio = &stx7141_ssc_i2c_gpio_pad_configs[ssc];

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx7141_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7141_configure_ssc_spi(int ssc,
		struct stx7141_ssc_spi_config *config)
{
	static int spi_busnum;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7141_ssc_devices));

	BUG_ON(stx7141_ssc_configured[ssc]);
	stx7141_ssc_configured[ssc] = 1;

	/* These two SSC can't be used in SPI mode - there is no
	 * MRST pin available */
	BUG_ON(ssc == 2 || ssc == 4);

	stx7141_ssc_devices[ssc].name = "spi-stm-ssc";
	stx7141_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7141_ssc_devices[ssc].dev.platform_data;
	if (config)
		plat_data->spi_chipselect = config->chipselect;
	plat_data->pad_config_ssc = &stx7141_ssc_spi_pad_configs[ssc];

	platform_device_register(&stx7141_ssc_devices[ssc]);

	return spi_busnum++;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx7141_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd018000, 0x234),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(81), -1),
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

void __init stx7141_configure_lirc(struct stx7141_lirc_config *config)
{
	static int configured;
	struct stx7141_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx7141_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(3, 3, 3);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled || config->tx_od_enabled;
	plat_data->pads = pad_config;

	switch (config->rx_mode) {
	case stx7141_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx7141_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_label_number(pad_config, "PIO5", 2);
		stm_pad_config_add_sys_cfg(pad_config, 20, 19, 19, 1);
		stm_pad_config_add_pio(pad_config, 5, 2, STM_GPIO_DIRECTION_IN);
		break;
	case stx7141_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_label_number(pad_config, "PIO3", 7);
		stm_pad_config_add_sys_cfg(pad_config, 20, 0, 0, 1);
		stm_pad_config_add_pio(pad_config, 3, 7, STM_GPIO_DIRECTION_IN);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled) {
		stm_pad_config_add_label_number(pad_config, "PIO5", 3);
		stm_pad_config_add_sys_cfg(pad_config, 20, 20, 20, 1);
		stm_pad_config_add_pio(pad_config, 5, 3,
				STM_GPIO_DIRECTION_OUT);
	};

	if (config->tx_od_enabled) {
		stm_pad_config_add_label_number(pad_config, "PIO5", 4);
		stm_pad_config_add_sys_cfg(pad_config, 20, 21, 21, 1);
		stm_pad_config_add_pio(pad_config, 5, 4,
				STM_GPIO_DIRECTION_OUT);
	};

	platform_device_register(&stx7141_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx7141_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.labels_num = 1,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL("PIO4.3"),
			},
			.sysconf_values_num = 1,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alternative function 2 for PIO3.4 */
				STM_PAD_SYS_CFG(19, 26, 27, 2),
			},
			.gpio_values_num = 1,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_OUT(4, 3),
			},
		},
		[1] = &(struct stm_pad_config) {
			.labels_num = 1,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL("PIO4.2"),
			},
			.sysconf_values_num = 1,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alternative function 2 for PIO4.2 */
				STM_PAD_SYS_CFG(20, 5, 6, 2),
			},
			.gpio_values_num = 1,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_OUT(4, 2),
			},
		},
	},
};

static struct platform_device stx7141_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd010000, 0x68),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(85), -1),
	},
	.dev.platform_data = &stx7141_pwm_platform_data,
};

void __init stx7141_configure_pwm(struct stx7141_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config) {
		stx7141_pwm_platform_data.channel_enabled[0] =
			config->out0_enabled;
		stx7141_pwm_platform_data.channel_enabled[1] =
			config->out1_enabled;
	}

	platform_device_register(&stx7141_pwm_device);
}
