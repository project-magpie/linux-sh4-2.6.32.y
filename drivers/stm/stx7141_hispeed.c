#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7141.h>
#include <asm/irq-ilc.h>

/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config *stx7141_ethernet_pad_configs[] = {
	[0] = (struct stm_pad_config []) {
		[stx7141_ethernet_mode_mii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 24, 26, 0),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(8, 0),		/* TXCLK */
				STM_PAD_PIO_OUT_MUX(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT_MUX(8, 5, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT_MUX(8, 6, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(9, 3),		/* RXCLK */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_IN(10, 0),		/* RXD.2 */
				STM_PAD_PIO_IN(10, 1),		/* RXD.3 */
				STM_PAD_PIO_IN(10, 6),		/* CRS */
				STM_PAD_PIO_IN(10, 7),		/* COL */
				STM_PAD_PIO_OUT_MUX(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(11, 1, 1),/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(11, 3, 1),	/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_gmii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 24, 26, 0),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
			},
			.gpio_values_num = 28,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(8, 0),		/* TXCLK */
				STM_PAD_PIO_OUT_MUX(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT_MUX(8, 5, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT_MUX(8, 6, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT_MUX(8, 7, 1),	/* TXD.4 */
				STM_PAD_PIO_OUT_MUX(9, 0, 1),	/* TXD.5 */
				STM_PAD_PIO_OUT_MUX(9, 1, 1),	/* TXD.6 */
				STM_PAD_PIO_OUT_MUX(9, 2, 1),	/* TXD.7 */
				STM_PAD_PIO_IN(9, 3),		/* RXCLK */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_IN(10, 0),		/* RXD.2 */
				STM_PAD_PIO_IN(10, 1),		/* RXD.3 */
				STM_PAD_PIO_IN(10, 2),		/* RXD.4 */
				STM_PAD_PIO_IN(10, 3),		/* RXD.5 */
				STM_PAD_PIO_IN(10, 4),		/* RXD.6 */
				STM_PAD_PIO_IN(10, 5),		/* RXD.7 */
				STM_PAD_PIO_IN(10, 6),		/* CRS */
				STM_PAD_PIO_IN(10, 7),		/* COL */
				STM_PAD_PIO_OUT_MUX(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(11, 1, 1),/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(11, 3, 1),	/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_rmii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 4 = RMII */
				STM_PAD_SYS_CFG(7, 24, 26, 4),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 1),
			},
			.gpio_values_num = 12,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_OUT_MUX(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_OUT_MUX(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(11, 1, 1),/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(11, 3, 1),	/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_reverse_mii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII0_CLOCK_OUT:
				 * 0 = PIO11.3 is controlled by PIO muxing,
				 * 1 = PIO11.3 is delayed version of PIO8.0
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 13, 13, 1),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(7, 16, 16, 1),
				/* PHY_INTF_SEL0: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 24, 26, 0),
				/* ENMII0: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 27, 27, 0),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(8, 0),		/* TXCLK */
				STM_PAD_PIO_OUT_MUX(8, 1, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(8, 2, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(8, 3, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(8, 4, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT_MUX(8, 5, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT_MUX(8, 6, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(9, 3),		/* RXCLK */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_IN(10, 0),		/* RXD.2 */
				STM_PAD_PIO_IN(10, 1),		/* RXD.3 */
				STM_PAD_PIO_IN(10, 6),		/* CRS */
				STM_PAD_PIO_IN(10, 7),		/* COL */
				STM_PAD_PIO_OUT_MUX(11, 0, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(11, 1, 1),/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(11, 3, 1),	/* PHYCLK */
			},
		},
	},
	[1] = (struct stm_pad_config []) {
		[stx7141_ethernet_mode_mii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 1),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(11, 4), 		/* TXCLK */
				STM_PAD_PIO_OUT_MUX(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT_MUX(12, 1, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT_MUX(12, 2, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(12, 7),		/* RXCLK */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_IN(13, 4),		/* RXD.2 */
				STM_PAD_PIO_IN(13, 5),		/* RXD.3 */
				STM_PAD_PIO_IN(14, 2),		/* CRS */
				STM_PAD_PIO_IN(14, 3),		/* COL */
				STM_PAD_PIO_OUT_MUX(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(14, 5, 1),/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(14, 7, 1),	/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_gmii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 1),
			},
			.gpio_values_num = 28,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(11, 4), 		/* TXCLK */
				STM_PAD_PIO_OUT_MUX(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT_MUX(12, 1, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT_MUX(12, 2, 1),	/* TXD.3 */
				STM_PAD_PIO_OUT_MUX(12, 3, 1),	/* TXD.4 */
				STM_PAD_PIO_OUT_MUX(12, 4, 1),	/* TXD.5 */
				STM_PAD_PIO_OUT_MUX(12, 5, 1),	/* TXD.6 */
				STM_PAD_PIO_OUT_MUX(12, 6, 1),	/* TXD.7 */
				STM_PAD_PIO_IN(12, 7),		/* RXCLK */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_IN(13, 4),		/* RXD.2 */
				STM_PAD_PIO_IN(13, 5),		/* RXD.3 */
				STM_PAD_PIO_IN(13, 6),		/* RXD.4 */
				STM_PAD_PIO_IN(13, 7),		/* RXD.5 */
				STM_PAD_PIO_IN(14, 0),		/* RXD.6 */
				STM_PAD_PIO_IN(14, 1),		/* RXD.7 */
				STM_PAD_PIO_IN(14, 2),		/* CRS */
				STM_PAD_PIO_IN(14, 3),		/* COL */
				STM_PAD_PIO_OUT_MUX(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(14, 5, 1),/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(14, 7, 1),	/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_rmii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 1),
			},
			.gpio_values_num = 12,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_OUT_MUX(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_OUT_MUX(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(14, 5, 1),/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(14, 7, 1),	/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_reverse_mii] = {
			.sysconf_values_num = 4,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_GMII1_CLOCK_OUT:
				 * 0 = PIO14.7 is controlled by PIO muxing,
				 * 1 = PIO14.7 is delayed version of PIO11.4
				 *     (ETHGMII0_TXCLK) */
				STM_PAD_SYS_CFG(7, 15, 15, 1),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(7, 17, 17, 1),
				/* PHY_INTF_SEL1: 0 = GMII/MII */
				STM_PAD_SYS_CFG(7, 28, 30, 0),
				/* ENMII1: 0 = reverse MII, 1 = MII mode */
				STM_PAD_SYS_CFG(7, 31, 31, 0),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(11, 4), 		/* TXCLK */
				STM_PAD_PIO_OUT_MUX(11, 5, 1),	/* TXEN */
				STM_PAD_PIO_OUT_MUX(11, 6, 1),	/* TXERR */
				STM_PAD_PIO_OUT_MUX(11, 7, 1),	/* TXD.0 */
				STM_PAD_PIO_OUT_MUX(12, 0, 1),	/* TXD.1 */
				STM_PAD_PIO_OUT_MUX(12, 1, 1),	/* TXD.2 */
				STM_PAD_PIO_OUT_MUX(12, 2, 1),	/* TXD.3 */
				STM_PAD_PIO_IN(12, 7),		/* RXCLK */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_IN(13, 4),		/* RXD.2 */
				STM_PAD_PIO_IN(13, 5),		/* RXD.3 */
				STM_PAD_PIO_IN(14, 2),		/* CRS */
				STM_PAD_PIO_IN(14, 3),		/* COL */
				STM_PAD_PIO_OUT_MUX(14, 4, 1),	/* MDC */
				STM_PAD_PIO_BIDIR_MUX(14, 5, 1),/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT_MUX(14, 7, 1),	/* PHYCLK */
			},
		},
	},
};

static void stx7141_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct stm_plat_stmmacenet_data stx7141_ethernet_platform_data[] = {
	[0] = {
		.pbl = 32,
		.has_gmac = 1,
		.fix_mac_speed = stx7141_ethernet_fix_mac_speed,
		/* .pad_config set in stx7141_configure_ethernet() */
	},
	[1] = {
		.pbl = 32,
		.has_gmac = 1,
		.fix_mac_speed = stx7141_ethernet_fix_mac_speed,
		/* .pad_config set in stx7141_configure_ethernet() */
	},
};

static struct platform_device stx7141_ethernet_devices[] = {
	[0] = {
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd110000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(40), -1),
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7141_ethernet_platform_data[0],
		}
	},
	[1] = {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd118000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(47), -1),
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7141_ethernet_platform_data[1],
		}
	},
};

void __init stx7141_configure_ethernet(int port,
		struct stx7141_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stx7141_ethernet_devices)];
	struct stx7141_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7141_ethernet_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	if (!config)
		config = &default_config;

	/* TODO: RGMII and SGMII configurations */
	BUG_ON(config->mode == stx7141_ethernet_mode_rgmii);
	BUG_ON(config->mode == stx7141_ethernet_mode_sgmii);

	pad_config = &stx7141_ethernet_pad_configs[port][config->mode];

	/* TODO: ext_clk configuration */

	stx7141_ethernet_platform_data[port].pad_config = pad_config;
	stx7141_ethernet_platform_data[port].bus_id = config->phy_bus;

	/* mac_speed */
	stx7141_ethernet_platform_data[port].bsp_priv = sysconf_claim(SYS_CFG,
			7, 20 + port, 20 + port, "stmmac");

	/* Cut 2 of 7141 has AHB wrapper bug for ethernet gmac */
	/* Need to disable read-ahead - performance impact     */
	if (cpu_data->cut_major == 2)
		stx7141_ethernet_platform_data[port].disable_readahead = 1;

	platform_device_register(&stx7141_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7141_usb_dma_mask = DMA_32BIT_MASK;

static int stx7141_usb_enable(struct stm_pad_config *config, void *priv);
static int stx7141_usb_disable(struct stm_pad_config *config, void *priv);

static struct stm_plat_usb_data stx7141_usb_platform_data[] = {
	[0] = { /* USB 2.0 port */
		.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
		.pad_config = &(struct stm_pad_config) {
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN_NAME(4, 6, "USB_OVRCUR"),
				/* USB power enable */
				STM_PAD_PIO_OUT_MUX(4, 7, 1),
			},
			.custom_claim = stx7141_usb_enable,
			.custom_release = stx7141_usb_disable,
			.custom_priv = (void *)0, /* Port 0 */
		},
	},
	[1] = { /* USB 2.0 port */
		.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
		.pad_config = &(struct stm_pad_config) {
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN_NAME(5, 0, "USB_OVRCUR"),
				/* USB power enable */
				STM_PAD_PIO_OUT_MUX(5, 1, 1),
			},
			.custom_claim = stx7141_usb_enable,
			.custom_release = stx7141_usb_disable,
			.custom_priv = (void *)1, /* Port 1 */
		},
	},
	[2] = { /* USB 1.1 port */
		.flags = STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN_NAME(4, 2, "USB_OVRCUR"),
				/* USB power enable */
				STM_PAD_PIO_OUT_MUX(4, 3, 1),
			},
			.custom_claim = stx7141_usb_enable,
			.custom_release = stx7141_usb_disable,
			.custom_priv = (void *)2, /* Port 2 */
		},
	},
	[3] = { /* USB 1.1 port */
		.flags = STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN_NAME(4, 4, "USB_OVRCUR"),
				/* USB power enable */
				STM_PAD_PIO_OUT_MUX(4, 5, 1),
			},
			.custom_claim = stx7141_usb_enable,
			.custom_release = stx7141_usb_disable,
			.custom_priv = (void *)3, /* Port 3 */
		},
	},
};

static struct platform_device stx7141_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7141_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(93), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(94), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfe100000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfe1fff00,
						    0x100),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7141_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfeaffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(95), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfeaffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(96), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfea00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfeafff00,
						    0x100),
		},
	},
	[2] = {
		.name = "stm-usb",
		.id = 2,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7141_usb_platform_data[2],
		},
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfebffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(97), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfeb00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfebfff00,
						    0x100),
		},
	},
	[3] = {
		.name = "stm-usb",
		.id = 3,
		.dev = {
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7141_usb_platform_data[3],
		},
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfecffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(98), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfec00000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfecfff00,
						    0x100),
		},
	},
};

static int stx7141_usb_enable(struct stm_pad_config *config, void *priv)
{
	int port = (int)priv;
	static int first = 1;
	struct sysconf_field *sc;
	unsigned gpio;

	if (first) {
		if (cpu_data->cut_major < 2) {
			/* ENABLE_USB48_CLK: Enable 48 MHz clock */
			sc = sysconf_claim(SYS_CFG, 4, 5, 5, "USB");
			sysconf_write(sc, 1);
		} else {
			/* Enable 48 MHz clock */
			sc = sysconf_claim(SYS_CFG, 4, 4, 5, "USB");
			sysconf_write(sc, 3);
			sc = sysconf_claim(SYS_CFG, 4, 10, 10, "USB");
			sysconf_write(sc, 1);

			/* Set overcurrent polarities */
			sc = sysconf_claim(SYS_CFG, 4, 6, 7, "USB");
			sysconf_write(sc, 2);

			/* enable resets  */
			sc = sysconf_claim(SYS_CFG, 4, 8, 8, "USB"); /* 1_0 */
			sysconf_write(sc, 1);
			sc = sysconf_claim(SYS_CFG, 4, 13, 13, "USB"); /* 1_1 */
			sysconf_write(sc, 1);
			sc = sysconf_claim(SYS_CFG, 4, 1, 1, "USB"); /*2_0 */
			sysconf_write(sc, 1);
			sc = sysconf_claim(SYS_CFG, 4, 14, 14, "USB"); /* 2_1 */
			sysconf_write(sc, 1);
		}

		first = 0;
	}

	/* Power up USB */
	sc = sysconf_claim(SYS_CFG, 32, 7 + port, 7 + port, "USB");
	sysconf_write(sc, 0);
	sc = sysconf_claim(SYS_STA, 15, 7 + port, 7 + port, "USB");
	do {
	} while (sysconf_read(sc));

	/* And yes, we have it again - overcurrent detection
	 * problem... (at least in cut 1) */
	gpio = stm_pad_gpio(config, "USB_OVRCUR");
	return gpio_direction_output(gpio, 0);
}

static int stx7141_usb_disable(struct stm_pad_config *config, void *priv)
{
	int port = (int)priv;
	struct sysconf_field *sc;
	unsigned gpio;

	/* Power down USB */
	sc = sysconf_claim(SYS_CFG, 32, 7 + port, 7 + port, "USB");
	sysconf_write(sc, 1);

	gpio = stm_pad_gpio(config, "USB_OVRCUR");
	return gpio_direction_input(gpio);
}

void __init stx7141_configure_usb(int port)
{
	static int configured[ARRAY_SIZE(stx7141_usb_devices)];

	BUG_ON(port < 0 || port > ARRAY_SIZE(stx7141_usb_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	platform_device_register(&stx7141_usb_devices[port]);
}



/* SATA resources --------------------------------------------------------- */

static struct platform_device stx7141_sata_device = {
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
		STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(89), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(88), -1),
	},
};

void __init stx7141_configure_sata(void)
{
	static int configured;

	BUG_ON(configured);
	configured = 1;

	if (cpu_data->cut_major >= 2) {
		struct sysconf_field *sc;

		/* enable reset  */
		sc = sysconf_claim(SYS_CFG, 4, 9, 9, "SATA");
		sysconf_write(sc, 1);

		sc = sysconf_claim(SYS_CFG, 32, 6, 6, "SATA");
		sysconf_write(sc, 1);

		sc = sysconf_claim(SYS_CFG, 33, 6, 6, "SATA");
		sysconf_write(sc, 0);

                stm_sata_miphy_init();
	}

	platform_device_register(&stx7141_sata_device);
}
