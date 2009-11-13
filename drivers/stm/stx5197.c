#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/ata_platform.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx5197.h>
#include <asm/irq-ilc.h>


/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stx5197_asc_pad_configs[] = {
	[0] = {
		.sysconf_values_num = 4, /* !!! see stx5197_configure_asc() */
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alt 0 for PIO0.0 & PIO0.1 */
			STM_PAD_CFG(CFG_CTRL_F, 0, 1, 0),
			STM_PAD_CFG(CFG_CTRL_F, 8, 9, 0),
			/* Alt 2 for PIO0.4 & PIO0.5 - HW flow control */
			STM_PAD_CFG(CFG_CTRL_F, 4, 5, 0),
			STM_PAD_CFG(CFG_CTRL_F, 12, 13, 3),
		},
		.gpio_values_num = 4, /* !!! see stx5197_configure_asc() */
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* TX */
			STM_PAD_PIO_ALT_OUT(0, 0),
			/* RX */
			STM_PAD_PIO_IN(0, 1),
			/* CTS (claimed for HW flow control only) */
			STM_PAD_PIO_IN(0, 5),
			/* RTS (claimed for HW flow control only) */
			STM_PAD_PIO_ALT_OUT(0, 4),
		},
	},
	[1] = {
		.sysconf_values_num = 4, /* !!! see stx5197_configure_asc() */
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alt 2 for PIO4.0 & PIO4.1 */
			STM_PAD_CFG(CFG_CTRL_O, 0, 1, 3),
			STM_PAD_CFG(CFG_CTRL_O, 8, 9, 3),
			/* Alt 2 for PIO4.2 & alt 3 PIO4.3 - HW flow control */
			STM_PAD_CFG(CFG_CTRL_O, 2, 3, 2),
			STM_PAD_CFG(CFG_CTRL_O, 10, 11, 3),
		},
		.gpio_values_num = 4, /* !!! see stx5197_configure_asc() */
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* TX */
			STM_PAD_PIO_ALT_OUT(4, 0),
			/* RX */
			STM_PAD_PIO_IN(4, 1),
			/* CTS (claimed for HW flow control only) */
			STM_PAD_PIO_IN(4, 3),
			/* RTS (claimed for HW flow control only) */
			STM_PAD_PIO_ALT_OUT(4, 2),
		},
	},
	[2] = {
		.sysconf_values_num = 4, /* !!! see stx5197_configure_asc() */
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alt 1 for PIO1.2 & PIO1.3 */
			STM_PAD_CFG(CFG_CTRL_F, 18, 19, 3),
			STM_PAD_CFG(CFG_CTRL_F, 26, 27, 0),
			/* Alt 1 for PIO1.4 & PIO1.5 - HW flow control */
			STM_PAD_CFG(CFG_CTRL_F, 20, 21, 3),
			STM_PAD_CFG(CFG_CTRL_F, 28, 29, 0),
		},
		.gpio_values_num = 4, /* !!! see stx5197_configure_asc() */
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* TX */
			STM_PAD_PIO_ALT_OUT(1, 2),
			/* RX */
			STM_PAD_PIO_IN(1, 3),
			/* CTS (claimed for HW flow control only) */
			STM_PAD_PIO_IN(1, 5),
			/* RTS (claimed for HW flow control only) */
			STM_PAD_PIO_ALT_OUT(1, 4),
		},
	},
	[3] = {
		.sysconf_values_num = 4, /* !!! see stx5197_configure_asc() */
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Alt 1 for PIO2.0 & PIO2.1 */
			STM_PAD_CFG(CFG_CTRL_G, 0, 1, 3),
			STM_PAD_CFG(CFG_CTRL_G, 8, 9, 0),
			/* Alt 1 for PIO2.2 & PIO2.5 - HW flow control */
			STM_PAD_CFG(CFG_CTRL_G, 2, 2, 1),
			STM_PAD_CFG(CFG_CTRL_G, 10, 10, 0),
			STM_PAD_CFG(CFG_CTRL_G, 5, 5, 1),
			STM_PAD_CFG(CFG_CTRL_G, 13, 13, 0),
		},
		.gpio_values_num = 4, /* !!! see stx5197_configure_asc() */
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* TX */
			STM_PAD_PIO_ALT_OUT(2, 0),
			/* RX */
			STM_PAD_PIO_IN(2, 1),
			/* CTS (claimed for HW flow control only) */
			STM_PAD_PIO_IN(2, 2),
			/* RTS (claimed for HW flow control only) */
			STM_PAD_PIO_ALT_OUT(2, 5),
		},
	},
};

static struct platform_device stx5197_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd130000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(7), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 8),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 10),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[0],
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd131000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(8), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 9),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 11),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[1],
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd132000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(12), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 3),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 5),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[2],
		},
	},
	[3] = {
		.name		= "stm-asc",
		/* .id set in stx5197_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd133000, 0x2c),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(13), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 4),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 6),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stx5197_asc_pad_configs[3],
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
		*stm_asc_configured_devices[ARRAY_SIZE(stx5197_asc_devices)];

void __init stx5197_configure_asc(int asc, struct stx5197_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx5197_asc_devices)];
	static int tty_id;
	struct stx5197_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx5197_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	if (!config->hw_flow_control) {
		/* Don't claim RTS/CTS pads */
		stx5197_asc_pad_configs[asc].gpio_values_num -= 2;
		/* sysconf values responsible for RTS/CTS routing
		 * are defined as the last 2 or 4 ones... */
		if (asc == 3)
			stx5197_asc_pad_configs[asc].sysconf_values_num -= 4;
		else
			stx5197_asc_pad_configs[asc].sysconf_values_num -= 2;
		/* gpio direction values for RTS/CTS are given as the
		 * last two ones... */
		stx5197_asc_pad_configs[asc].gpio_values_num -= 2;
	}

	pdev = &stx5197_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx5197_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stx5197_add_asc);



/* SSC resources ---------------------------------------------------------- */

/* Pad configuration for SSC0 in I2C/SSC mode on PIO1.6/7 pads */
static struct stm_pad_config stx5197_ssc0_i2c_ssc_pio1_pad_config = {
	.sysconf_values_num = 4,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* SPI_BOOTNOTCOMMS
		 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
		STM_PAD_CFG(CFG_CTRL_M, 14, 14, 0),
		/* PIO_FUNCTIONALITY_ON_PIO1_7
		 * 0: QAM validation, 1: Normal PIO */
		STM_PAD_CFG(CFG_CTRL_I, 2, 2, 1),
		/* Alt 2 for PIO1.6 & PIO1.6 */
		STM_PAD_CFG(CFG_CTRL_F, 22, 23, 0),
		STM_PAD_CFG(CFG_CTRL_F, 30, 31, 3),
	},
	.gpio_values_num = 2,
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* SCL */
		STM_PAD_PIO_ALT_BIDIR(1, 6),
		/* SDA */
		STM_PAD_PIO_ALT_BIDIR(1, 7),
	},
};

/* Pad configuration for SSC0 in I2C/GPIO (temporary) mode (on PIO1) */
static struct stm_pad_config stx5197_ssc0_i2c_gpio_pad1_pad_config = {
	.sysconf_values_num = 2,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt 1 for PIO1.6 & PIO1.6 */
		STM_PAD_CFG(CFG_CTRL_F, 22, 23, 3),
		STM_PAD_CFG(CFG_CTRL_F, 30, 31, 0),
	},
	.gpio_values_num = 2,
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* SCL - in I2C mode on PIO1.6 only! */
		STM_PAD_PIO_BIDIR(1, 6),
		/* SDA - in I2C mode on PIO1.7 only!*/
		STM_PAD_PIO_BIDIR(1, 7),
	},
};

/* Pad configuration for SSC0 in I2C/SSC mode on SPI pads */
static struct stm_pad_config stx5197_ssc0_i2c_ssc_spi_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_STRINGS("SPI", "CLK", "DATAIN"),
	},
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* SPI_BOOTNOTCOMMS
		 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
		STM_PAD_CFG(CFG_CTRL_M, 14, 14, 1),
	},
};

/* Pad configuration for SSC0 in SPI mode (dedicated SPI pads) */
static struct stm_pad_config stx5197_ssc0_spi_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_STRINGS("SPI", "CLK", "DATAIN",
				"DATAOUT", "NOTCS"),
	},
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* SPI_BOOTNOTCOMMS
		 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
		STM_PAD_CFG(CFG_CTRL_M, 14, 14, 1),
	},
};

/* Pad configuration for SSC1 in I2C/SSC mode as internal QPSK bus */
static struct stm_pad_config stx5197_ssc1_i2c_ssc_qpsk_config = {
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* QPSK_DEBUG_CONFIG
		 * 0: IP289 I2C input from PIO1[0:1],
		 * 1: IP289 input from BE COMMS SSC1 */
		STM_PAD_CFG(CFG_CTRL_C, 1, 1, 1),
	},
};

/* Pad configuration for SSC1 in I2C/SSC mode on QAM_SCLT/QAM_SDAT pads */
static struct stm_pad_config stx5197_ssc1_i2c_ssc_qam_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL_STRINGS("QAM", "SCLT", "SDAT"),
	},
	.sysconf_values_num = 1,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* 0: QPSK repeater interface is routed to QAM_SCLT/SDAT,
		 * 1: SSC1 is routed to QAM_SCLT/SDAT. */
		STM_PAD_CFG(CFG_CTRL_K, 27, 27, 1),
	},
};

/* Pad configuration for SSC2 in I2C/SSC mode (always PIO3.3/2 pads) */
static struct stm_pad_config stx5197_ssc2_i2c_ssc_pad_config = {
	.sysconf_values_num = 2,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt 1 for PIO3.2 & PIO3.3 */
		STM_PAD_CFG(CFG_CTRL_G, 18, 19, 3),
		STM_PAD_CFG(CFG_CTRL_G, 26, 27, 0),
	},
	.gpio_values_num = 2,
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* SCL */
		STM_PAD_PIO_ALT_BIDIR(3, 3),
		/* SDA */
		STM_PAD_PIO_ALT_BIDIR(3, 2),
	},
};

/* Pad configuration for SSC2 in I2C GPIO (temporary) mode */
static struct stm_pad_config stx5197_ssc2_i2c_gpio_pad_config = {
	.sysconf_values_num = 2,
	.sysconf_values = (struct stm_pad_sysconf_value []) {
		/* Alt 0 for PIO3.2 & PIO3.3 */
		STM_PAD_CFG(CFG_CTRL_G, 18, 19, 0),
		STM_PAD_CFG(CFG_CTRL_G, 26, 27, 0),
	},
	.gpio_values_num = 2,
	.gpio_values = (struct stm_pad_gpio_value []) {
		/* SCL */
		STM_PAD_PIO_BIDIR(3, 3),
		/* SDA */
		STM_PAD_PIO_BIDIR(3, 2),
	},
};

static struct platform_device stx5197_ssc_devices[] = {
	[0] = {
		/* .name & .id set in stx5197_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd140000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(5), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5197_configure_ssc_*() */
		},
	},
	[1] = {
		/* .name & .id set in stx5197_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd141000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(6), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			/* .pad_config_* set in stx5197_configure_ssc_*() */
		},
	},
	[2] = {
		/* .name & .id set in stx5197_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd142000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(17), -1),
		},
		.dev.platform_data = &(struct stm_plat_ssc_data) {
			.pad_config_ssc = &stx5197_ssc2_i2c_ssc_pad_config,
			.pad_config_gpio = &stx5197_ssc2_i2c_gpio_pad_config,
		},
	},
};

static int __initdata stx5197_ssc_configured[ARRAY_SIZE(stx5197_ssc_devices)];

int __init stx5197_configure_ssc_i2c(int ssc,
		struct stx5197_ssc_i2c_config *config)
{
	static int i2c_busnum;
	struct stx5197_ssc_i2c_config default_config;
	struct stm_plat_ssc_data *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx5197_ssc_devices));

	BUG_ON(stx5197_ssc_configured[ssc]);
	stx5197_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx5197_ssc_devices[ssc].name = "i2c-stm";
	stx5197_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx5197_ssc_devices[ssc].dev.platform_data;

	switch (ssc) {
	case 0:
		switch (config->routing.ssc0) {
		case stx5197_ssc0_i2c_pio1:
			plat_data->pad_config_ssc =
					&stx5197_ssc0_i2c_ssc_pio1_pad_config;
			plat_data->pad_config_gpio =
					&stx5197_ssc0_i2c_gpio_pad1_pad_config;
			break;
		case stx5197_ssc0_i2c_spi:
			plat_data->pad_config_ssc =
					&stx5197_ssc0_i2c_ssc_spi_pad_config;
			/* No GPIO pad config for obvious reasons ;-) */
			break;
		default:
			BUG();
			break;
		}
		break;
	case 1:
		switch (config->routing.ssc1) {
		case stx5197_ssc1_i2c_qpsk:
			plat_data->pad_config_ssc =
					&stx5197_ssc1_i2c_ssc_qpsk_config;
			/* No GPIO pad config for obvious reasons ;-) */
			break;
		case stx5197_ssc1_i2c_qam:
			plat_data->pad_config_ssc =
					&stx5197_ssc1_i2c_ssc_qam_config;
			/* No GPIO pad config for obvious reasons ;-) */
			break;
		default:
			BUG();
			break;
		}
		break;
	case 2:
		if (config->routing.ssc2 != stx5197_ssc2_i2c_pio3)
			BUG();
		/* Only one possible configuration, already set
		 * in device definition - nothing to do then. */
		break;
	default:
		BUG();
		break;
	}

	/* I2C bus number reservation (to prevent any hot-plug device
	 * from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);

	platform_device_register(&stx5197_ssc_devices[ssc]);

	return i2c_busnum++;
}

static struct sysconf_field *stx5197_ssc0_spi_cs_sc;

static void stx5197_ssc0_spi_cs(struct spi_device *spi, int is_on)
{
	sysconf_write(stx5197_ssc0_spi_cs_sc, is_on ? 0 : 1);
}

int __init stx5197_configure_ssc_spi(int ssc)
{
	struct stm_plat_ssc_data *plat_data;

	/* Only SSC0 is SPI-capable */
	BUG_ON(ssc != 0);

	BUG_ON(stx5197_ssc_configured[0]);
	stx5197_ssc_configured[0] = 1;

	stx5197_ssc_devices[0].name = "spi-stm-ssc";
	stx5197_ssc_devices[0].id = 0;

	plat_data = stx5197_ssc_devices[ssc].dev.platform_data;
	plat_data->pad_config_ssc = &stx5197_ssc0_spi_pad_config;
	plat_data->spi_chipselect = stx5197_ssc0_spi_cs;
	stx5197_ssc0_spi_cs_sc = sysconf_claim(CFG_CTRL_M,
			13, 13, "ssc");
	sysconf_write(stx5197_ssc0_spi_cs_sc, 1); /* CS not active (yet) */

	platform_device_register(&stx5197_ssc_devices[0]);

	return 0;
}



/* LiRC resources --------------------------------------------------------- */

static struct platform_device stx5197_lirc_device = {
	.name = "lirc-stm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd118000, 0x234),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(19), -1),
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

void __init stx5197_configure_lirc(struct stx5197_lirc_config *config)
{
	static int configured;
	struct stx5197_lirc_config default_config = {};
	struct stm_plat_lirc_data *plat_data =
			stx5197_lirc_device.dev.platform_data;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = stm_pad_config_alloc(2, 4, 2);
	BUG_ON(!pad_config);

	plat_data->txenabled = config->tx_enabled;
	plat_data->pads = pad_config;

	switch (config->rx_mode) {
	case stx5197_lirc_rx_disabled:
		/* Nothing to do */
		break;
	case stx5197_lirc_rx_mode_ir:
		plat_data->rxuhfmode = 0;
		stm_pad_config_add_label_number(pad_config, "PIO2", 5);
		/* Alt. 2 for PIO2.5 (IRB_PPM_IN) */
		stm_pad_config_add_sysconf(pad_config, CFG_CTRL_G, 5, 5, 0);
		stm_pad_config_add_sysconf(pad_config,
				CFG_CTRL_G, 13, 13, 1);
		stm_pad_config_add_pio(pad_config, 2, 5, STM_GPIO_DIRECTION_IN);
		break;
	case stx5197_lirc_rx_mode_uhf:
		plat_data->rxuhfmode = 1;
		stm_pad_config_add_label_number(pad_config, "PIO2", 6);
		/* Alt. 1 for PIO2.6 (IRB_UHF_IN) */
		stm_pad_config_add_sysconf(pad_config, CFG_CTRL_G, 6, 6, 1);
		stm_pad_config_add_sysconf(pad_config,
				CFG_CTRL_G, 14, 14, 0);
		stm_pad_config_add_pio(pad_config, 2, 6, STM_GPIO_DIRECTION_IN);
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled) {
		stm_pad_config_add_label_number(pad_config, "PIO2", 7);
		/* Alt. 1 for PIO2.7 (IRB_PPM_OUT) */
		stm_pad_config_add_sysconf(pad_config, CFG_CTRL_G, 7, 7, 1);
		stm_pad_config_add_sysconf(pad_config,
				CFG_CTRL_G, 15, 15, 0);
		stm_pad_config_add_pio(pad_config, 2, 7,
				STM_GPIO_DIRECTION_ALT_OUT);
	};

	platform_device_register(&stx5197_lirc_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct stm_plat_pwm_data stx5197_pwm_platform_data = {
	.channel_pad_config = {
		[0] = &(struct stm_pad_config) {
			.sysconf_values_num = 2,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alt. 1 for PIO2.4 */
				STM_PAD_CFG(CFG_CTRL_G, 4, 4, 1),
				STM_PAD_CFG(CFG_CTRL_G, 12, 12, 0),
			},
			.gpio_values_num = 1,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_ALT_OUT(2, 4),
			},
		},
	},
};

static struct platform_device stx5197_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd110000, 0x68),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(43), -1),
	},
	.dev.platform_data = &stx5197_pwm_platform_data,
};

void __init stx5197_configure_pwm(struct stx5197_pwm_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config)
		stx5197_pwm_platform_data.channel_enabled[0] =
				config->out0_enabled;

	platform_device_register(&stx5197_pwm_device);
}



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx5197_ethernet_pad_configs[] = {
	[stx5197_ethernet_mode_mii] = {
		.labels_num = 3,
		.labels = (struct stm_pad_label []) {
			/* MII is multiplexed with some
			 * transport stream pads... */
			STM_PAD_LABEL_STRINGS("TS0.OUT", "PACKETCLK", "ERROR",
					"BITORBYTECLKVALID", "BITORBYTECLK"),
			STM_PAD_LABEL_RANGE("TS0.OUT.DATA", 0, 7),
			STM_PAD_LABEL_RANGE("TS0.IN.DATA", 1, 7),
		},
		.sysconf_values_num = 4,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Ethernet interface on */
			STM_PAD_CFG(CFG_CTRL_E, 0, 0, 1),
			/* RMII/MII pin mode */
			STM_PAD_CFG(CFG_CTRL_E, 7, 8, 3),
			/* MII mode */
			STM_PAD_CFG(CFG_CTRL_E, 2, 2, 1),
			/* MII phyclk out enable: 0=output, 1=input,
			 * set in stx5197_configure_ethernet() */
			STM_PAD_CFG(CFG_CTRL_E, 6, 6, -1),

		},
	},
	[stx5197_ethernet_mode_rmii] = {
		.labels_num = 2,
		.labels = (struct stm_pad_label []) {
			/* RMII is multiplexed with some
			 * transport stream pads... */
			STM_PAD_LABEL_STRINGS("TS0.OUT", "PACKETCLK", "ERROR",
					"BITORBYTECLKVALID", "BITORBYTECLK"),
			STM_PAD_LABEL_RANGE("TS0.OUT.DATA", 0, 5),
		},
		.sysconf_values_num = 4,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Ethernet interface on */
			STM_PAD_CFG(CFG_CTRL_E, 0, 0, 1),
			/* RMII/MII pin mode */
			STM_PAD_CFG(CFG_CTRL_E, 7, 8, 2),
			/* MII mode */
			STM_PAD_CFG(CFG_CTRL_E, 2, 2, 0),
			/* MII phyclk out enable: 0=output, 1=input,
			 * set in stx5197_configure_ethernet() */
			STM_PAD_CFG(CFG_CTRL_E, 6, 6, -1),
		},
	},
};

static void stx5197_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct stm_plat_stmmacenet_data stx5197_ethernet_platform_data = {
	.pbl = 32,
	.fix_mac_speed = stx5197_ethernet_fix_mac_speed,
	/* .pad_config set in stx5197_configure_ethernet() */
};

static struct platform_device stx5197_ethernet_device = {
	.name = "stmmaceth",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfde00000, 0x10000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(24), -1),
	},
	.dev.platform_data = &stx5197_ethernet_platform_data,
};

void __init stx5197_configure_ethernet(struct stx5197_ethernet_config *config)
{
	static int configured;
	struct stx5197_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx5197_ethernet_pad_configs[config->mode];

	pad_config->sysconf_values[3].value = (config->ext_clk ? 1 : 0);

	stx5197_ethernet_platform_data.pad_config = pad_config;
	stx5197_ethernet_platform_data.bus_id = config->phy_bus;

	/* MAC_SPEED_SEL */
	stx5197_ethernet_platform_data.bsp_priv = sysconf_claim(CFG_CTRL_E,
		1, 1, "stmmac");

	platform_device_register(&stx5197_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx5197_usb_dma_mask = DMA_32BIT_MASK;

static struct stm_plat_usb_data stx5197_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
		STM_PLAT_USB_FLAGS_STRAP_PLL |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
	.pad_config = &(struct stm_pad_config) {
		.labels_num = 2,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_STRINGS("USB",
					"RESETB", "CLK", "STP", "DIR", "NXT"),
			STM_PAD_LABEL_RANGE("USB.DATA", 0, 7),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* USB power down */
			STM_PAD_CFG(CFG_CTRL_H, 8, 8, 0),
			/* DDR enable for ULPI:
			 * 0 = 8 bit SDR ULPI, 1 = 4 bit DDR ULPI */
			STM_PAD_CFG(CFG_CTRL_M, 12, 12, 0),
		},
	},
};

static struct platform_device stx5197_usb_device = {
	.name = "stm-usb",
	.id = -1,
	.dev = {
		.dma_mask = &stx5197_usb_dma_mask,
		.coherent_dma_mask = DMA_32BIT_MASK,
		.platform_data = &stx5197_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		/* EHCI */
		STM_PLAT_RESOURCE_MEM(0xfddffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(29), -1),
		/* OHCI */
		STM_PLAT_RESOURCE_MEM(0xfddffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(28), -1),
		/* Wrapper glue */
		STM_PLAT_RESOURCE_MEM(0xfdd00000, 0x100),
		STM_PLAT_RESOURCE_MEM(0xfddfff00, 0x100),
	},
};

void __init stx5197_configure_usb(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	platform_device_register(&stx5197_usb_device);
}



/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include "fdma_firmware_7200.h"

static struct stm_plat_fdma_hw stx5197_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.periph_regs = {
		.sync_reg = 0x8000 + (0xfe2 << 2), /* 0xbf88 */
		.cmd_sta  = 0x8000 + (0xff0 << 2), /* 0xbfc0 */
		.cmd_set  = 0x8000 + (0xff1 << 2), /* 0xbfc4 */
		.cmd_clr  = 0x8000 + (0xff2 << 2), /* 0xbfc8 */
		.cmd_mask = 0x8000 + (0xff3 << 2), /* 0xbfcc */
		.int_sta  = 0x8000 + (0xff4 << 2), /* 0xbfd0 */
		.int_set  = 0x8000 + (0xff5 << 2), /* 0xbfd4 */
		.int_clr  = 0x8000 + (0xff6 << 2), /* 0xbfd8 */
		.int_mask = 0x8000 + (0xff7 << 2), /* 0xbfdc */
	},
	.dmem_offset = 0x8000,
	.dmem_size   = 0x800 << 2, /* 2048 * 4 = 8192 */
	.imem_offset = 0xc000,
	.imem_size   = 0x1000 << 2, /* 4096 * 4 = 16384 */
};

static struct stm_plat_fdma_data stx5197_fdma_platform_data = {
	.hw = &stx5197_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx5197_fdma_platform_data_addr &stx5197_fdma_platform_data

#else

#define stx5197_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx5197_fdma_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfdb00000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(34), -1),
	},
	.dev.platform_data = stx5197_fdma_platform_data_addr,
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx5197_pio_devices[] = {
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd120000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(0), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(0),
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd121000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(1), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(1),
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd122000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(2), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(2),
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd123000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(3), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(3),
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd124000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(4), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(4),
	},
};

static void __init stx5197_pio_late_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx5197_pio_devices); i++)
		platform_device_register(&stx5197_pio_devices[i]);
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx5197_sysconf_devices[] = {
	{
		.name		= "stm-sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd901000, 0x80),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				PLAT_SYSCONF_GROUP(HD_CFG, 0x000),
			},
		}
	}, {
		.name		= "stm-sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd902000, 0x10),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				PLAT_SYSCONF_GROUP(HS_CFG, 0x000),
			},
		}
	},
};


/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stx5197_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(stx5197_sysconf_devices, 2);
	stm_gpio_early_init(stx5197_pio_devices,
			ARRAY_SIZE(stx5197_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(CFG_MONITOR_H, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx5197 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static struct platform_device emi =  {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128*1024*1024),
		STM_PLAT_RESOURCE_MEM(0xfde30000, 0x874),
	},
};

static int __init stx5197_postcore_setup(void)
{
	return platform_device_register(&emi);
}
postcore_initcall(stx5197_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx5197_devices[] __initdata = {
	&stx5197_fdma_device,
	&stx5197_sysconf_devices[0],
	&stx5197_sysconf_devices[1],
};

static int __init stx5197_devices_setup(void)
{
	stx5197_pio_late_setup();

	return platform_add_devices(stx5197_devices,
			ARRAY_SIZE(stx5197_devices));
}
device_initcall(stx5197_devices_setup);

