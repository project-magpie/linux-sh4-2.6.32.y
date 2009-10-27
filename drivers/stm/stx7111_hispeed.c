#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7111.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx7111_ethernet_pad_configs[] = {
	[stx7111_ethernet_mode_mii] = {
		.labels_num = 3,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_STRINGS("MII", "COL", "CRS", "MDC",
					"MDINT", "MDIO", "PHYCLK", "RXCLK",
					"RXDV", "RXERR", "TXCLK", "TXEN"),
			STM_PAD_LABEL_RANGE("MII.RXD", 0, 3),
			STM_PAD_LABEL_RANGE("MII.TXD", 0, 3),
		},
		.sysconf_values_num = 4,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* ETHERNET_INTERFACE_ON: 0 = off, 1 = on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* ETHERNET_PHY_CLK_EXT:
			 * 0 = MII_PHYCLK is an output,
			 * 1 = MII_PHYCLK is an input
			 * set by stx7111_configure_ethernet() */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* ETHERNET_PHY_INTF_SEL: 0 = MII, 4 = RMII */
			STM_PAD_SYS_CFG(7, 24, 26, 0),
			/* ENMII: 0 = reverse MII, 1 = MII */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
		},
	},
	[stx7111_ethernet_mode_rmii] = {
		.labels_num = 3,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_STRINGS("MII0", "MDC", "MDINT", "MDIO",
					"PHYCLK", "RXDV", "RXERR", "TXEN"),
			STM_PAD_LABEL_RANGE("MII.RXD", 0, 1),
			STM_PAD_LABEL_RANGE("MII.TXD", 0, 1),
		},
		.sysconf_values_num = 4,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* ETHERNET_INTERFACE_ON: 0 = off, 1 = on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* ETHERNET_PHY_CLK_EXT:
			 * 0 = MII_PHYCLK is an output,
			 * 1 = MII_PHYCLK is an input
			 * set by stx7111_configure_ethernet() */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* ETHERNET_PHY_INTF_SEL: 0 = MII, 4 = RMII */
			STM_PAD_SYS_CFG(7, 24, 26, 4),
			/* ENMII: 0 = reverse MII, 1 = MII */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
		},
	},
	[stx7111_ethernet_mode_reverse_mii] = {
		.labels_num = 3,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_STRINGS("MII", "COL", "CRS", "MDC",
					"MDINT", "MDIO", "PHYCLK", "RXCLK",
					"RXDV", "RXERR", "TXCLK", "TXEN"),
			STM_PAD_LABEL_RANGE("MII.RXD", 0, 3),
			STM_PAD_LABEL_RANGE("MII.TXD", 0, 3),
		},
		.sysconf_values_num = 4,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* ETHERNET_INTERFACE_ON: 0 = off, 1 = on */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* ETHERNET_PHY_CLK_EXT:
			 * 0 = MII_PHYCLK is an output,
			 * 1 = MII_PHYCLK is an input
			 * set by stx7111_configure_ethernet() */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* ETHERNET_PHY_INTF_SEL: 0 = MII, 4 = RMII */
			STM_PAD_SYS_CFG(7, 24, 26, 0),
			/* ENMII: 0 = reverse MII, 1 = MII */
			STM_PAD_SYS_CFG(7, 27, 27, 0),
		},
	},
};

static void stx7111_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct stm_plat_stmmacenet_data stx7111_ethernet_platform_data = {
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = stx7111_ethernet_fix_mac_speed,
};

static struct platform_device stx7111_ethernet_device = {
	.name = "stmmaceth",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd110000, 0x08000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", evt2irq(0x12a0), -1),
	},
	.dev.platform_data = &stx7111_ethernet_platform_data,
};

void __init stx7111_configure_ethernet(struct stx7111_ethernet_config *config)
{
	static int configured;
	struct stx7111_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx7111_ethernet_pad_configs[config->mode];

	stx7111_ethernet_platform_data.pad_config = pad_config;
	stx7111_ethernet_platform_data.bus_id = config->phy_bus;

	pad_config->sysconf_values[1].value = (config->ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	stx7111_ethernet_platform_data.bsp_priv = sysconf_claim(SYS_CFG,
			7, 20, 20, "stmmac");

	platform_device_register(&stx7111_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7111_usb_dma_mask = DMA_32BIT_MASK;

static struct stm_plat_usb_data stx7111_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
		STM_PLAT_USB_FLAGS_STRAP_PLL |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
	.pad_config = &(struct stm_pad_config) {
		.labels_num = 1,
		.labels = (struct stm_pad_label []) {
			STM_PAD_LABEL_RANGE("PIO5", 6, 7),
		},
		.sysconf_values_num = 2,
		.sysconf_values = (struct stm_pad_sysconf_value []) {
			/* Power on USB */
			STM_PAD_SYS_CFG(32, 4, 4, 0),
			/* Work around for USB over-current detection chip
			 * being active low, and the 7111 being active high.
			 * Note this is an undocumented bit, which apparently
			 * enables an inverter on the overcurrent signal.
			 * Set in stx7111_configure_usb(). */
			STM_PAD_SYS_CFG(6, 29, 29, 0),
		},
		.gpio_values_num = 2,
		.gpio_values = (struct stm_pad_gpio_value []) {
			/* Overcurrent detection */
			STM_PAD_PIO_IN(5, 6),
			/* USB power enable */
			STM_PAD_PIO_ALT_OUT(5, 7),
		},
	},
};

static struct platform_device stx7111_usb_device = {
	.name = "stm-usb",
	.id = 0,
	.dev = {
		.dma_mask = &stx7111_usb_dma_mask,
		.coherent_dma_mask = DMA_32BIT_MASK,
		.platform_data = &stx7111_usb_platform_data,
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
		STM_PLAT_RESOURCE_MEM(0xfe1fff00, 0x100),
	},
};

void __init stx7111_configure_usb(struct stx7111_usb_config *config)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (config)
		stx7111_usb_platform_data.pad_config->sysconf_values[1].value =
				(config->invert_ovrcur ? 1 : 0);

	platform_device_register(&stx7111_usb_device);
}

