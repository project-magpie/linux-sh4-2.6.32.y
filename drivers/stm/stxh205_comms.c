/*
 * (c) 2011 STMicroelectronics Limited
 *
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/emi.h>
#include <linux/stm/stxh205.h>
#include <asm/irq-ilc.h>



/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stxh205_asc_pad_configs[] = {
	[0] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(10, 0, 2),	/* TX */
			STM_PAD_PIO_IN(10, 1, 2),	/* RX */
			STM_PAD_PIO_IN_NAMED(10, 2, 2, "CTS"),
			STM_PAD_PIO_OUT_NAMED(10, 3, 2, "RTS"),
		},
	},
	[1] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(11, 0, 3),	/* TX */
			STM_PAD_PIO_IN(11, 1, 3),	/* RX */
			STM_PAD_PIO_IN_NAMED(11, 4, 3, "CTS"),
			STM_PAD_PIO_OUT_NAMED(11, 2, 3, "RTS"),
		},
	},
	[2] = {
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(15, 4, 1),	/* TX */
			STM_PAD_PIO_IN(15, 5, 1),	/* RX */
			STM_PAD_PIO_IN_NAMED(15, 6, 1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(15, 7, 1, "RTS"),
		},
	},
	[3] = {
		/* ASC10 / UART10 */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(3, 5, 2),	/* TX */
			STM_PAD_PIO_IN(3, 6, 2),	/* RX */
			STM_PAD_PIO_IN_NAMED(3, 7, 2, "CTS"),
			STM_PAD_PIO_OUT_NAMED(3, 4, 2, "RTS"),
		},
	},
	[4] = {
		/* ASC11 / UART11 */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 6, 3),	/* TX */
			STM_PAD_PIO_IN(2, 7, 3),	/* RX */
			STM_PAD_PIO_IN_NAMED(3, 0, 3, "CTS"),
			STM_PAD_PIO_OUT_NAMED(3, 1, 3, "RTS"),
		},
	},
};

static struct platform_device stxh205_asc_devices[] = {
	[0] = {
		.name		= "stm-asc",
		/* .id set in stxh205_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd730000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(40), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stxh205_asc_pad_configs[0],
		},
	},
	[1] = {
		.name		= "stm-asc",
		/* .id set in stxh205_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd731000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(41), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stxh205_asc_pad_configs[1],
		},
	},
	[2] = {
		.name		= "stm-asc",
		/* .id set in stxh205_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd732000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(42), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 40),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 43),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stxh205_asc_pad_configs[2],
		},
	},
	[3] = {
		/* ASC10 / UART10 */
		.name		= "stm-asc",
		/* .id set in stxh205_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe530000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(43), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stxh205_asc_pad_configs[3],
			.clk_id = "sbc_comms_clk",
		},
	},
	[4] = {
		/* ASC11 / UART11 */
		.name		= "stm-asc",
		/* .id set in stxh205_configure_asc() */
		.num_resources	= 4,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe531000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(44), -1),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stxh205_asc_pad_configs[4],
			.clk_id = "sbc_comms_clk",
		},
	},
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
int __initdata stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(stxh205_asc_devices)];

void __init stxh205_configure_asc(int asc, struct stxh205_asc_config *config)
{
	static int configured[ARRAY_SIZE(stxh205_asc_devices)];
	static int tty_id;
	struct stxh205_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stxh205_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stxh205_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	plat_data->txfifo_bug = 1;

	if (!config->hw_flow_control) {
		/* Don't claim RTS/CTS pads */
		struct stm_pad_config *pad_config;
		pad_config = &stxh205_asc_pad_configs[asc];
		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stxh205_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stxh205_add_asc);
