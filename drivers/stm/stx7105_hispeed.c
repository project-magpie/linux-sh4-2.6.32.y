#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx7105_ethernet_pad_configs[] = {
	[stx7105_ethernet_mode_mii] = {
		.labels_num = 3,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO7", 4, 7),
			STM_PAD_LABEL_RANGE("PIO8", 0, 7),
			STM_PAD_LABEL_RANGE("PIO9", 0, 6),
		},
		.sysconf_values_num = 11,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* ethernet_interface_on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* enMII: 0 = reverse MII mode, 1 = MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
			/* mii_mdio_select:
			 * 0 = from GMAC, 1 = miim_dio from external input */
			STM_PAD_SYS_CFG(7, 17, 17, 0),
			/* rmii_mode: 0 = MII, 1 = RMII interface activated */
			/* CUT 1: This register wasn't connected,
			 * so only MII available!!! */
			STM_PAD_SYS_CFG(7, 18, 18, 0),
			/* phy_intf_select:
			 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
			STM_PAD_SYS_CFG(7, 25, 26, 0),
			/* TXD[0-1] = PIO7.6-7 - alt. func 1 */
			STM_PAD_SYS_CFG(37, 6, 7, 0),
			STM_PAD_SYS_CFG(37, 14, 15, 0),
			/* TXD[2-3], TX_EN, MDIO, MDC = PIO8.0-4 -
			 * alt. func. 1 */
			STM_PAD_SYS_CFG(46, 0, 4, 0),
			STM_PAD_SYS_CFG(46, 8, 12, 0),
			/* PHYCLK = PIO9.5 - alt. func. 1 */
			STM_PAD_SYS_CFG(47, 5, 5, 0),
			STM_PAD_SYS_CFG(47, 13, 13, 0),
		},
		.gpio_values_num = 19,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_IN(7, 4),		/* RXDV */
			STM_PAD_PIO_IN(7, 5),		/* RXERR */
			STM_PAD_PIO_ALT_OUT(7, 6),	/* TXD.0 */
			STM_PAD_PIO_ALT_OUT(7, 7),	/* TXD.1 */
			STM_PAD_PIO_ALT_OUT(8, 0),	/* TXD.2 */
			STM_PAD_PIO_ALT_OUT(8, 1),	/* TXD.3 */
			STM_PAD_PIO_ALT_OUT(8, 2),	/* TXEN */
			STM_PAD_PIO_ALT_BIDIR(8, 3),	/* MDIO */
			STM_PAD_PIO_ALT_OUT(8, 4),	/* MDC */
			STM_PAD_PIO_IN(8, 5),		/* RXCLK */
			STM_PAD_PIO_IN(8, 6),		/* RXD.0 */
			STM_PAD_PIO_IN(8, 7),		/* RXD.1 */
			STM_PAD_PIO_IN(9, 0),		/* RXD.2 */
			STM_PAD_PIO_IN(9, 1),		/* RXD.3 */
			STM_PAD_PIO_IN(9, 2),		/* TXCLK */
			STM_PAD_PIO_IN(9, 3),		/* COL */
			STM_PAD_PIO_IN(9, 4),		/* CRS */
			STM_PAD_PIO_IN(9, 6),		/* MDINT */
			STM_PAD_PIO_UNKNOWN(9, 5),	/* PHYCLK */
			/* ^ direction set by stx7105_configure_ethernet() */
		},
	},
	[stx7105_ethernet_mode_gmii] = {
		.labels_num = 5,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO4", 4, 7),
			STM_PAD_LABEL_RANGE("PIO5", 0, 3),
			STM_PAD_LABEL_RANGE("PIO7", 4, 7),
			STM_PAD_LABEL_RANGE("PIO8", 0, 7),
			STM_PAD_LABEL_RANGE("PIO9", 0, 6),
		},
		.sysconf_values_num = 11,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* ethernet_interface_on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* enMII: 0 = reverse MII mode, 1 = MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
			/* mii_mdio_select:
			 * 0 = from GMAC, 1 = miim_dio from external input */
			STM_PAD_SYS_CFG(7, 17, 17, 0),
			/* rmii_mode: 0 = MII, 1 = RMII interface activated */
			/* CUT 1: This register wasn't connected,
			 * so only MII available!!! */
			STM_PAD_SYS_CFG(7, 18, 18, 0),
			/* phy_intf_select:
			 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
			STM_PAD_SYS_CFG(7, 25, 26, 0),
			/* TXD[4-7] = PIO4.4-7 - alt. func. 2 */
			STM_PAD_SYS_CFG(34, 4, 7, 0xf),
			STM_PAD_SYS_CFG(34, 12, 15, 0),
			/* TXD[0-1] = PIO7.6-7 - alt. func. 3 */
			STM_PAD_SYS_CFG(37, 6, 7, 0),
			STM_PAD_SYS_CFG(37, 14, 15, 3),
			/* TXD[2-3], TX_EN, MDIO, MDC = PIO8.0-4 -
			 * alt. func. 3 */
			STM_PAD_SYS_CFG(46, 8, 12, 0x1f),
			/* PHYCLK = PIO9.5 - alt. func. 3 */
			STM_PAD_SYS_CFG(47, 13, 13, 1),
		},
		.gpio_values_num = 27,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_ALT_OUT(4, 4),	/* TXD.4 */
			STM_PAD_PIO_ALT_OUT(4, 5),	/* TXD.5 */
			STM_PAD_PIO_ALT_OUT(4, 6),	/* TXD.6 */
			STM_PAD_PIO_ALT_OUT(4, 7),	/* TXD.7 */
			STM_PAD_PIO_IN(5, 0),		/* RXD.4 */
			STM_PAD_PIO_IN(5, 1),		/* RXD.5 */
			STM_PAD_PIO_IN(5, 2),		/* RXD.6 */
			STM_PAD_PIO_IN(5, 3),		/* RXD.7 */
			STM_PAD_PIO_IN(7, 4),		/* RXDV */
			STM_PAD_PIO_IN(7, 5),		/* RXERR */
			STM_PAD_PIO_ALT_OUT(7, 6),	/* TXD.0 */
			STM_PAD_PIO_ALT_OUT(7, 7),	/* TXD.1 */
			STM_PAD_PIO_ALT_OUT(8, 0),	/* TXD.2 */
			STM_PAD_PIO_ALT_OUT(8, 1),	/* TXD.3 */
			STM_PAD_PIO_ALT_OUT(8, 2),	/* TXEN */
			STM_PAD_PIO_ALT_BIDIR(8, 3),	/* MDIO */
			STM_PAD_PIO_ALT_OUT(8, 4),	/* MDC */
			STM_PAD_PIO_IN(8, 5),		/* RXCLK */
			STM_PAD_PIO_IN(8, 6),		/* RXD.0 */
			STM_PAD_PIO_IN(8, 7),		/* RXD.1 */
			STM_PAD_PIO_IN(9, 0),		/* RXD.2 */
			STM_PAD_PIO_IN(9, 1),		/* RXD.3 */
			STM_PAD_PIO_IN(9, 2),		/* TXCLK */
			STM_PAD_PIO_IN(9, 3),		/* COL */
			STM_PAD_PIO_IN(9, 4),		/* CRS */
			STM_PAD_PIO_IN(9, 6),		/* MDINT */
			STM_PAD_PIO_UNKNOWN(9, 5),	/* PHYCLK */
			/* ^ direction set by stx7105_configure_ethernet() */
		},
	},
	[stx7105_ethernet_mode_rgmii] = { /* TODO */ },
	[stx7105_ethernet_mode_sgmii] = { /* TODO */ },
	[stx7105_ethernet_mode_rmii] = {
		.labels_num = 4,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO7", 4, 7),
			STM_PAD_LABEL_RANGE("PIO8", 2, 4),
			STM_PAD_LABEL_RANGE("PIO8", 6, 7),
			STM_PAD_LABEL_RANGE("PIO9", 5, 6),
		},
		.sysconf_values_num = 11,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Ethernet ON */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* enMII: 0 = reverse MII mode, 1 = MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
			/* mii_mdio_select:
			 * 1 = miim_dio from external input, 0 = from GMAC */
			STM_PAD_SYS_CFG(7, 17, 17, 0),
			/* rmii_mode: 0 = MII, 1 = RMII interface activated */
			/* CUT 1: This register wasn't connected,
			 * so only MII available!!! */
			STM_PAD_SYS_CFG(7, 18, 18, 1),
			/* phy_intf_select:
			 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
			STM_PAD_SYS_CFG(7, 25, 26, 0),
			/* TXD[0-1] = PIO7.6-7 - alt. func 1 */
			STM_PAD_SYS_CFG(37, 6, 7, 0),
			STM_PAD_SYS_CFG(37, 14, 15, 0),
			/* TX_EN, MDIO, MDC = PIO8.2-4 - alt. func. 2 */
			STM_PAD_SYS_CFG(46, 2, 4, 7),
			STM_PAD_SYS_CFG(46, 10, 12, 0),
			/* REF_CLK = PIO9.5 - alt. func. 2 */
			STM_PAD_SYS_CFG(47, 5, 5, 1),
			STM_PAD_SYS_CFG(47, 13, 13, 0),
		},
		.gpio_values_num = 11,
		.gpio_values = (struct stm_pad_gpio_value []) {
			STM_PAD_PIO_IN(7, 4),		/* RXDV */
			STM_PAD_PIO_IN(7, 5),		/* RXERR */
			STM_PAD_PIO_ALT_OUT(7, 6),	/* TXD.0 */
			STM_PAD_PIO_ALT_OUT(7, 7),	/* TXD.1 */
			STM_PAD_PIO_ALT_OUT(8, 2),	/* TXEN */
			STM_PAD_PIO_ALT_BIDIR(8, 3),	/* MDIO */
			STM_PAD_PIO_ALT_OUT(8, 4),	/* MDC */
			STM_PAD_PIO_IN(8, 6),		/* RXD.0 */
			STM_PAD_PIO_IN(8, 7),		/* RXD.1 */
			STM_PAD_PIO_IN(9, 6),		/* MDINT */
			STM_PAD_PIO_ALT_BIDIR(9, 5),	/* PHYCLK */
		},
	},
	[stx7105_ethernet_mode_reverse_mii] = {
		.labels_num = 3,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO7", 4, 7),
			STM_PAD_LABEL_RANGE("PIO8", 0, 7),
			STM_PAD_LABEL_RANGE("PIO9", 0, 6),
		},
		.sysconf_values_num = 11,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Ethernet ON */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* enMII: 0 = reverse MII mode, 1 = MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 0),
			/* mii_mdio_select:
			 * 1 = miim_dio from external input, 0 = from GMAC */
			STM_PAD_SYS_CFG(7, 17, 17, 0),
			/* rmii_mode: 0 = MII, 1 = RMII interface activated */
			/* CUT 1: This register wasn't connected,
			 * so only MII available!!! */
			STM_PAD_SYS_CFG(7, 18, 18, 0),
			/* phy_intf_select:
			 * 00 = GMII/MII, 01 = RGMII, 1x = SGMII */
			STM_PAD_SYS_CFG(7, 25, 26, 0),
			/* TXD[0-1] = PIO7.6-7 - alt. func 1 */
			STM_PAD_SYS_CFG(37, 6, 7, 0),
			STM_PAD_SYS_CFG(37, 14, 15, 0),
			/* TXD[2-3], TX_EN, MDIO, MDC = PIO8.0-4 -
			 * alt. func. 1 */
			STM_PAD_SYS_CFG(46, 0, 4, 0),
			STM_PAD_SYS_CFG(46, 8, 12, 0),
			/* PHYCLK = PIO9.5 - alt. func. 1 */
			STM_PAD_SYS_CFG(47, 5, 5, 0),
			STM_PAD_SYS_CFG(47, 13, 13, 0),
		},
		.gpio_values_num = 19,
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* TODO: check what about EXCRS output */
			STM_PAD_PIO_IN(7, 4),		/* RXDV */
			/* TODO: check what about EXCOL output */
			STM_PAD_PIO_IN(7, 5),		/* RXERR */
			STM_PAD_PIO_ALT_OUT(7, 6),	/* TXD.0 */
			STM_PAD_PIO_ALT_OUT(7, 7),	/* TXD.1 */
			STM_PAD_PIO_ALT_OUT(8, 0),	/* TXD.2 */
			STM_PAD_PIO_ALT_OUT(8, 1),	/* TXD.3 */
			STM_PAD_PIO_ALT_OUT(8, 2),	/* TXEN */
			STM_PAD_PIO_ALT_BIDIR(8, 3),	/* MDIO */
			STM_PAD_PIO_ALT_OUT(8, 4),	/* MDC */
			STM_PAD_PIO_IN(8, 5),		/* RXCLK */
			STM_PAD_PIO_IN(8, 6),		/* RXD.0 */
			STM_PAD_PIO_IN(8, 7),		/* RXD.1 */
			STM_PAD_PIO_IN(9, 0),		/* RXD.2 */
			STM_PAD_PIO_IN(9, 1),		/* RXD.3 */
			STM_PAD_PIO_IN(9, 2),		/* TXCLK */
			STM_PAD_PIO_IN(9, 3),		/* COL */
			STM_PAD_PIO_IN(9, 4),		/* CRS */
			STM_PAD_PIO_IN(9, 6),		/* MDINT */
			STM_PAD_PIO_UNKNOWN(9, 5),	/* PHYCLK */
			/* ^ direction set by stx7105_configure_ethernet() */
		},
	},
};

static void stx7105_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	if (mac_speed_sel)
		sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct stm_plat_stmmacenet_data stx7105_ethernet_platform_data = {
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = stx7105_ethernet_fix_mac_speed,
	/* .pad_config set in stx7105_configure_ethernet() */
};

static struct platform_device stx7105_ethernet_device = {
	.name = "stmmaceth",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd110000, 0x8000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", evt2irq(0x12c0), -1),
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &stx7105_ethernet_platform_data,
	}
};

void __init stx7105_configure_ethernet(struct stx7105_ethernet_config *config)
{
	static int configured;
	struct stx7105_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	/* TODO: RGMII and SGMII configurations */
	BUG_ON(config->mode == stx7105_ethernet_mode_rgmii);
	BUG_ON(config->mode == stx7105_ethernet_mode_sgmii);

	pad_config = &stx7105_ethernet_pad_configs[config->mode];

	if (config->mode != stx7105_ethernet_mode_rmii) {
		int last_gpio = pad_config->gpio_values_num - 1;

		pad_config->gpio_values[last_gpio].direction =
				config->ext_clk ? STM_GPIO_DIRECTION_IN :
				STM_GPIO_DIRECTION_ALT_OUT;
	}

	stx7105_ethernet_platform_data.pad_config = pad_config;
	stx7105_ethernet_platform_data.bus_id = config->phy_bus;

	/* mac_speed */
	stx7105_ethernet_platform_data.bsp_priv = sysconf_claim(SYS_CFG,
			7, 20, 20, "stmmac");

	platform_device_register(&stx7105_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7105_usb_dma_mask = DMA_32BIT_MASK;

static struct stm_plat_usb_data stx7105_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		/* .pad_config created in stx7105_configure_usb() */
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		/* .pad_config created in stx7105_configure_usb() */
	},
};

static struct platform_device stx7105_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7105_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7105_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			/* EHCI */
			STM_PLAT_RESOURCE_MEM(0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1720), -1),
			/* OHCI */
			STM_PLAT_RESOURCE_MEM(0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1700), -1),
			/* Wrapper glue */
			STM_PLAT_RESOURCE_MEM(0xfe100000, 0x100),
			/* Protocol converter */
			STM_PLAT_RESOURCE_MEM(0xfe1fff00, 0x100),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stx7105_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7105_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			/* EHCI */
			STM_PLAT_RESOURCE_MEM(0xfeaffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x13e0), -1),
			/* OHCI */
			STM_PLAT_RESOURCE_MEM(0xfeaffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x13c0), -1),
			/* Wrapper glue */
			STM_PLAT_RESOURCE_MEM(0xfea00000, 0x100),
			/* Protocol converter */
			STM_PLAT_RESOURCE_MEM(0xfeafff00, 0x100),
		},
	},
};

void __init stx7105_configure_usb(int port, struct stx7105_usb_config *config)
{
	static int configured[ARRAY_SIZE(stx7105_usb_devices)];
	struct stm_pad_config *pad_config;

	BUG_ON(port < 0 || port > ARRAY_SIZE(stx7105_usb_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	pad_config = stm_pad_config_alloc(2, 8, 2);
	BUG_ON(!pad_config);
	stx7105_usb_platform_data[port].pad_config = pad_config;

	/* USB PHY clock from alternate pad? */
	/* sysconf_claim(SYS_CFG, 40, 2, 2, "USB"); */

	/* Power up USB PHY */
	stm_pad_config_add_sys_cfg(pad_config, 32, 6 + port, 6 + port, 0);

	/* Power up USB host */
	stm_pad_config_add_sys_cfg(pad_config, 32, 4 + port, 4 + port, 0);

	if (config->ovrcur_mode == stx7105_usb_ovrcur_disabled) {
		/* cfg_usbX_ovrcurr_enable */
		stm_pad_config_add_sys_cfg(pad_config,
				4, 11 + port, 11 + port, 0);
	} else {
		/* cfg_usbX_ovrcurr_enable */
		stm_pad_config_add_sys_cfg(pad_config,
				4, 11 + port, 11 + port, 1);

		if (config->ovrcur_mode == stx7105_usb_ovrcur_active_high)
			/* usbX_prt_ovrcurr_pol */
			stm_pad_config_add_sys_cfg(pad_config,
					4, 3 + port, 3 + port, 0);
		else if (config->ovrcur_mode == stx7105_usb_ovrcur_active_low)
			/* usbX_prt_ovrcurr_pol */
			stm_pad_config_add_sys_cfg(pad_config,
					4, 3 + port, 3 + port, 1);
		else
			BUG();

		if (port == 0) {
			switch (config->routing.usb0.ovrcur) {
			case stx7105_usb0_ovrcur_pio4_4:
				stm_pad_config_add_label_number(pad_config,
						"PIO4", 4);
				/* usb0_prt_ovrcurr_sel: 0 = PIO4.4 */
				stm_pad_config_add_sys_cfg(pad_config,
						4, 5, 5, 0);
				stm_pad_config_add_pio(pad_config, 4, 4,
						STM_GPIO_DIRECTION_IN);
				break;
			case stx7105_usb0_ovrcur_pio12_5:
				stm_pad_config_add_label_number(pad_config,
						"PIO12", 5);
				/* usb0_prt_ovrcurr_sel: 1 = PIO12.5 */
				stm_pad_config_add_sys_cfg(pad_config,
						4, 5, 5, 1);
				stm_pad_config_add_pio(pad_config, 12, 5,
						STM_GPIO_DIRECTION_IN);
				break;
			default:
				BUG();
				break;
			}
		} else {
			switch (config->routing.usb1.ovrcur) {
			case stx7105_usb1_ovrcur_pio4_6:
				stm_pad_config_add_label_number(pad_config,
						"PIO4", 6);
				/* usb1_prt_ovrcurr_sel: 0 = PIO4.6 */
				stm_pad_config_add_sys_cfg(pad_config,
						4, 6, 6, 0);
				stm_pad_config_add_pio(pad_config, 4, 6,
						STM_GPIO_DIRECTION_IN);
				break;
			case stx7105_usb1_ovrcur_pio14_6:
				stm_pad_config_add_label_number(pad_config,
						"PIO14", 6);
				/* usb1_prt_ovrcurr_sel: 1 = PIO14.6 */
				stm_pad_config_add_sys_cfg(pad_config,
						4, 6, 6, 1);
				stm_pad_config_add_pio(pad_config, 14, 6,
						STM_GPIO_DIRECTION_IN);
				break;
			default:
				BUG();
				break;
			}
		}
	}

	if (config->pwr_enabled) {
		if (port == 0) {
			switch (config->routing.usb0.pwr) {
			case stx7105_usb0_pwr_pio4_5:
				stm_pad_config_add_label_number(pad_config,
						"PIO4", 5);
				stm_pad_config_add_pio(pad_config, 4, 5,
						STM_GPIO_DIRECTION_ALT_OUT);
				/* Alt. func. 4 */
				stm_pad_config_add_sys_cfg(pad_config,
						34, 5, 5, 1);
				stm_pad_config_add_sys_cfg(pad_config,
						34, 13, 13, 1);
				break;
			case stx7105_usb0_pwr_pio12_6:
				stm_pad_config_add_label_number(pad_config,
						"PIO12", 6);
				stm_pad_config_add_pio(pad_config, 12, 6,
						STM_GPIO_DIRECTION_ALT_OUT);
				/* Alt. func. 3 */
				stm_pad_config_add_sys_cfg(pad_config,
						48, 6, 6, 0);
				stm_pad_config_add_sys_cfg(pad_config,
						48, 14, 14, 1);
				stm_pad_config_add_sys_cfg(pad_config,
						48, 22, 22, 0);
				break;
			default:
				BUG();
				break;
			}
		} else {
			switch (config->routing.usb1.pwr) {
			case stx7105_usb1_pwr_pio4_7:
				stm_pad_config_add_label_number(pad_config,
						"PIO4", 7);
				stm_pad_config_add_pio(pad_config, 4, 7,
						STM_GPIO_DIRECTION_ALT_OUT);
				/* Alt. func. 4 */
				stm_pad_config_add_sys_cfg(pad_config,
						34, 7, 7, 1);
				stm_pad_config_add_sys_cfg(pad_config,
						34, 15, 15, 1);
				break;
			case stx7105_usb1_pwr_pio14_7:
				stm_pad_config_add_label_number(pad_config,
						"PIO14", 7);
				stm_pad_config_add_pio(pad_config, 14, 7,
						STM_GPIO_DIRECTION_ALT_OUT);
				break;
			default:
				BUG();
				break;
			}
		}

	}

	platform_device_register(&stx7105_usb_devices[port]);
}



/* SATA resources --------------------------------------------------------- */

static struct platform_device stx7105_sata_device = {
	.name = "sata-stm",
	.id = 0,
	.dev.platform_data = &(struct stm_plat_sata_data) {
		.phy_init = 0,
		.pc_glue_logic_init = 0,
		.only_32bit = 0,
	},
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe209000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("hostc", evt2irq(0xb00), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("dmac", evt2irq(0xa80), -1),
	},
};

void __init stx7105_configure_sata(void)
{
	static int configured;
	struct sysconf_field *sc;

	BUG_ON(configured);
	configured = 1;

        BUG_ON(cpu_data->cut_major < 3);

	/* Power up SATA phy */
	sc = sysconf_claim(SYS_CFG, 32, 9, 9, "SATA");
	sysconf_write(sc, 0);

	/* Apply the PHY reset work around */
	sc = sysconf_claim(SYS_CFG, 33, 6, 6, "SATA");
	sysconf_write(sc, 1);
	stm_sata_miphy_init();

	/* Power up SATA host */
	sc = sysconf_claim(SYS_CFG, 32, 11, 11, "SATA");
	sysconf_write(sc, 0);

	platform_device_register(&stx7105_sata_device);
}
