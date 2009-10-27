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
			.labels_num = 5,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO8", 0, 6),
				STM_PAD_LABEL_RANGE("PIO9", 3, 7),
				STM_PAD_LABEL_RANGE("PIO10", 0, 1),
				STM_PAD_LABEL_RANGE("PIO10", 6, 7),
				STM_PAD_LABEL_RANGE("PIO11", 0, 3),
			},
			.sysconf_values_num = 10,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(25, 28, 30, 1),
				STM_PAD_SYS_CFG(35, 0, 17, 0x9249),
				STM_PAD_SYS_CFG(35, 27, 30, 5),
				STM_PAD_SYS_CFG(46, 0, 9, 0x155),
				STM_PAD_SYS_CFG(46, 18, 29, 0x255),
				STM_PAD_SYS_CFG(47, 0, 2, 1),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(8, 0),		/* TXCLK */
				STM_PAD_PIO_OUT(8, 1),		/* TXEN */
				STM_PAD_PIO_OUT(8, 2),		/* TXERR */
				STM_PAD_PIO_OUT(8, 3),		/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4),		/* TXD.1 */
				STM_PAD_PIO_OUT(8, 5),		/* TXD.2 */
				STM_PAD_PIO_OUT(8, 6),		/* TXD.3 */
				STM_PAD_PIO_IN(9, 3),		/* RXCLK */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_IN(10, 0),		/* RXD.2 */
				STM_PAD_PIO_IN(10, 1),		/* RXD.3 */
				STM_PAD_PIO_IN(10, 6),		/* CRS */
				STM_PAD_PIO_IN(10, 7),		/* COL */
				STM_PAD_PIO_OUT(11, 0),		/* MDC */
				STM_PAD_PIO_BIDIR(11, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT(11, 3),		/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_gmii] = {
			.labels_num = 4,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO8", 0, 7),
				STM_PAD_LABEL_RANGE("PIO9", 0, 7),
				STM_PAD_LABEL_RANGE("PIO10", 0, 7),
				STM_PAD_LABEL_RANGE("PIO11", 0, 3),
			},
			.sysconf_values_num = 8,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(25, 28, 30, 1),
				STM_PAD_SYS_CFG(35, 0, 30, 0x2aa49249),
				STM_PAD_SYS_CFG(46, 0, 29, 0x9555555),
				STM_PAD_SYS_CFG(47, 0, 2, 1),
			},
			.gpio_values_num = 28,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(8, 0),		/* TXCLK */
				STM_PAD_PIO_OUT(8, 1),		/* TXEN */
				STM_PAD_PIO_OUT(8, 2),		/* TXERR */
				STM_PAD_PIO_OUT(8, 3),		/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4),		/* TXD.1 */
				STM_PAD_PIO_OUT(8, 5),		/* TXD.2 */
				STM_PAD_PIO_OUT(8, 6),		/* TXD.3 */
				STM_PAD_PIO_OUT(8, 7),		/* TXD.4 */
				STM_PAD_PIO_OUT(9, 0),		/* TXD.5 */
				STM_PAD_PIO_OUT(9, 1),		/* TXD.6 */
				STM_PAD_PIO_OUT(9, 2),		/* TXD.7 */
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
				STM_PAD_PIO_OUT(11, 0),		/* MDC */
				STM_PAD_PIO_BIDIR(11, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT(11, 3),		/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_rmii] = {
			.labels_num = 3,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO8", 1, 4),
				STM_PAD_LABEL_RANGE("PIO9", 4, 7),
				STM_PAD_LABEL_RANGE("PIO11", 0, 3),
			},
			.sysconf_values_num = 9,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(35, 0, 11, 0x249),
				STM_PAD_SYS_CFG(35, 29, 30, 1),
				STM_PAD_SYS_CFG(46, 0, 5, 0x15),
				STM_PAD_SYS_CFG(46, 22, 29, 0x25),
				STM_PAD_SYS_CFG(47, 0, 2, 1),
			},
			.gpio_values_num = 12,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_OUT(8, 1),		/* TXEN */
				STM_PAD_PIO_OUT(8, 2),		/* TXERR */
				STM_PAD_PIO_OUT(8, 3),		/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4),		/* TXD.1 */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_OUT(11, 0),		/* MDC */
				STM_PAD_PIO_BIDIR(11, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT(11, 3),		/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_reverse_mii] = {
			.labels_num = 5,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO8", 0, 6),
				STM_PAD_LABEL_RANGE("PIO9", 3, 7),
				STM_PAD_LABEL_RANGE("PIO10", 0, 1),
				STM_PAD_LABEL_RANGE("PIO10", 6, 7),
				STM_PAD_LABEL_RANGE("PIO11", 0, 3),
			},
			.sysconf_values_num = 10,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(25, 28, 30, 1),
				STM_PAD_SYS_CFG(35, 0, 17, 0x9249),
				STM_PAD_SYS_CFG(35, 27, 30, 5),
				STM_PAD_SYS_CFG(46, 0, 9, 0x155),
				STM_PAD_SYS_CFG(46, 18, 29, 0x255),
				STM_PAD_SYS_CFG(47, 0, 2, 1),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(8, 0),		/* TXCLK */
				STM_PAD_PIO_OUT(8, 1),		/* TXEN */
				STM_PAD_PIO_OUT(8, 2),		/* TXERR */
				STM_PAD_PIO_OUT(8, 3),		/* TXD.0 */
				STM_PAD_PIO_OUT(8, 4),		/* TXD.1 */
				STM_PAD_PIO_OUT(8, 5),		/* TXD.2 */
				STM_PAD_PIO_OUT(8, 6),		/* TXD.3 */
				STM_PAD_PIO_IN(9, 3),		/* RXCLK */
				STM_PAD_PIO_IN(9, 4),		/* RXDV */
				STM_PAD_PIO_IN(9, 5),		/* RXERR */
				STM_PAD_PIO_IN(9, 6),		/* RXD.0 */
				STM_PAD_PIO_IN(9, 7),		/* RXD.1 */
				STM_PAD_PIO_IN(10, 0),		/* RXD.2 */
				STM_PAD_PIO_IN(10, 1),		/* RXD.3 */
				STM_PAD_PIO_IN(10, 6),		/* CRS */
				STM_PAD_PIO_IN(10, 7),		/* COL */
				STM_PAD_PIO_OUT(11, 0),		/* MDC */
				STM_PAD_PIO_BIDIR(11, 1),	/* MDIO */
				STM_PAD_PIO_IN(11, 2),		/* MDINT */
				STM_PAD_PIO_OUT(11, 3),		/* PHYCLK */
			},
		},
	},
	[1] = (struct stm_pad_config []) {
		[stx7141_ethernet_mode_mii] = {
			.labels_num = 5,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO11", 4, 7),
				STM_PAD_LABEL_RANGE("PIO12", 0, 2),
				STM_PAD_LABEL("PIO12.7"),
				STM_PAD_LABEL_RANGE("PIO13", 0, 5),
				STM_PAD_LABEL_RANGE("PIO14", 2, 7),
			},
			.sysconf_values_num = 7,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(47, 3, 23, 0x49249),
				STM_PAD_SYS_CFG(48, 3, 23, 0x49249),
				STM_PAD_SYS_CFG(49, 3, 19, 0x9249),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(11, 4), 		/* TXCLK */
				STM_PAD_PIO_OUT(11, 5),		/* TXEN */
				STM_PAD_PIO_OUT(11, 6),		/* TXERR */
				STM_PAD_PIO_OUT(11, 7),		/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0),		/* TXD.1 */
				STM_PAD_PIO_OUT(12, 1),		/* TXD.2 */
				STM_PAD_PIO_OUT(12, 2),		/* TXD.3 */
				STM_PAD_PIO_IN(12, 7),		/* RXCLK */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_IN(13, 4),		/* RXD.2 */
				STM_PAD_PIO_IN(13, 5),		/* RXD.3 */
				STM_PAD_PIO_IN(14, 2),		/* CRS */
				STM_PAD_PIO_IN(14, 3),		/* COL */
				STM_PAD_PIO_OUT(14, 4),		/* MDC */
				STM_PAD_PIO_BIDIR(14, 5),	/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT(14, 7),		/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_gmii] = {
			.labels_num = 4,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO11", 4, 7),
				STM_PAD_LABEL_RANGE("PIO12", 0, 7),
				STM_PAD_LABEL_RANGE("PIO13", 0, 7),
				STM_PAD_LABEL_RANGE("PIO14", 0, 7),
			},
			.sysconf_values_num = 7,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(47, 3, 29, 0x2a49249),
				STM_PAD_SYS_CFG(48, 0, 30, 0x15249249),
				STM_PAD_SYS_CFG(49, 0, 19, 0x49249),
			},
			.gpio_values_num = 28,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(11, 4), 		/* TXCLK */
				STM_PAD_PIO_OUT(11, 5),		/* TXEN */
				STM_PAD_PIO_OUT(11, 6),		/* TXERR */
				STM_PAD_PIO_OUT(11, 7),		/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0),		/* TXD.1 */
				STM_PAD_PIO_OUT(12, 1),		/* TXD.2 */
				STM_PAD_PIO_OUT(12, 2),		/* TXD.3 */
				STM_PAD_PIO_OUT(12, 3),		/* TXD.4 */
				STM_PAD_PIO_OUT(12, 4),		/* TXD.5 */
				STM_PAD_PIO_OUT(12, 5),		/* TXD.6 */
				STM_PAD_PIO_OUT(12, 6),		/* TXD.7 */
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
				STM_PAD_PIO_OUT(14, 4),		/* MDC */
				STM_PAD_PIO_BIDIR(14, 5),	/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT(14, 7),		/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_rgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_sgmii] = { /* TODO */ },
		[stx7141_ethernet_mode_rmii] = {
			.labels_num = 4,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO11", 5, 7),
				STM_PAD_LABEL("PIO12.0"),
				STM_PAD_LABEL_RANGE("PIO13", 0, 3),
				STM_PAD_LABEL_RANGE("PIO14", 4, 7),
			},
			.sysconf_values_num = 7,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(47, 6, 17, 0x249),
				STM_PAD_SYS_CFG(48, 6, 17, 0x249),
				STM_PAD_SYS_CFG(49, 9, 19, 0x249),
			},
			.gpio_values_num = 12,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_OUT(11, 5),		/* TXEN */
				STM_PAD_PIO_OUT(11, 6),		/* TXERR */
				STM_PAD_PIO_OUT(11, 7),		/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0),		/* TXD.1 */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_OUT(14, 4),		/* MDC */
				STM_PAD_PIO_BIDIR(14, 5),	/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT(14, 7),		/* PHYCLK */
			},
		},
		[stx7141_ethernet_mode_reverse_mii] = {
			.labels_num = 5,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO11", 4, 7),
				STM_PAD_LABEL_RANGE("PIO12", 0, 2),
				STM_PAD_LABEL("PIO12.7"),
				STM_PAD_LABEL_RANGE("PIO13", 0, 5),
				STM_PAD_LABEL_RANGE("PIO14", 2, 7),
			},
			.sysconf_values_num = 7,
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
				/* Alternative function 1 for all the PIOs */
				STM_PAD_SYS_CFG(47, 3, 23, 0x49249),
				STM_PAD_SYS_CFG(48, 3, 23, 0x49249),
				STM_PAD_SYS_CFG(49, 3, 19, 0x9249),
			},
			.gpio_values_num = 20,
			.gpio_values = (struct stm_pad_gpio_value []) {
				STM_PAD_PIO_IN(11, 4), 		/* TXCLK */
				STM_PAD_PIO_OUT(11, 5),		/* TXEN */
				STM_PAD_PIO_OUT(11, 6),		/* TXERR */
				STM_PAD_PIO_OUT(11, 7),		/* TXD.0 */
				STM_PAD_PIO_OUT(12, 0),		/* TXD.1 */
				STM_PAD_PIO_OUT(12, 1),		/* TXD.2 */
				STM_PAD_PIO_OUT(12, 2),		/* TXD.3 */
				STM_PAD_PIO_IN(12, 7),		/* RXCLK */
				STM_PAD_PIO_IN(13, 0),		/* RXDV */
				STM_PAD_PIO_IN(13, 1),		/* RXERR */
				STM_PAD_PIO_IN(13, 2),		/* RXD.0 */
				STM_PAD_PIO_IN(13, 3),		/* RXD.1 */
				STM_PAD_PIO_IN(13, 4),		/* RXD.2 */
				STM_PAD_PIO_IN(13, 5),		/* RXD.3 */
				STM_PAD_PIO_IN(14, 2),		/* CRS */
				STM_PAD_PIO_IN(14, 3),		/* COL */
				STM_PAD_PIO_OUT(14, 4),		/* MDC */
				STM_PAD_PIO_BIDIR(14, 5),	/* MDIO */
				STM_PAD_PIO_IN(14, 6),		/* MDINT */
				STM_PAD_PIO_OUT(14, 7),		/* PHYCLK */
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

	platform_device_register(&stx7141_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7141_usb_dma_mask = DMA_32BIT_MASK;

static int stx7141_usb_enable(void *priv);
static int stx7141_usb_disable(void *priv);

static struct stm_plat_usb_data stx7141_usb_platform_data[] = {
	[0] = { /* USB 2.0 port */
		.flags = STM_PLAT_USB_FLAGS_STRAP_16BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256,
		.pad_config = &(struct stm_pad_config) {
			.labels_num = 1,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO4", 6, 7),
			},
			.sysconf_values_num = 1,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alt. function 1 on PIO4.6 & PIO4.7 */
				STM_PAD_SYS_CFG(20, 13, 14, 3),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN(4, 6),
				/* USB power enable */
				STM_PAD_PIO_OUT(4, 7),
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
			.labels_num = 1,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO5", 0, 1),
			},
			.sysconf_values_num = 1,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alt. function 1 on PIO5.0 & PIO5.1 */
				STM_PAD_SYS_CFG(20, 15, 18, 5),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN(5, 0),
				/* USB power enable */
				STM_PAD_PIO_OUT(5, 1),
			},
			.custom_claim = stx7141_usb_enable,
			.custom_release = stx7141_usb_disable,
			.custom_priv = (void *)1, /* Port 1 */
		},
	},
	[2] = { /* USB 1.1 port */
		.flags = STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.labels_num = 1,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO4", 2, 3),
			},
			.sysconf_values_num = 1,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alt. function 1 on PIO4.2 & PIO4.3 */
				STM_PAD_SYS_CFG(20, 5, 8, 5),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN(4, 2),
				/* USB power enable */
				STM_PAD_PIO_OUT(4, 3),
			},
			.custom_claim = stx7141_usb_enable,
			.custom_release = stx7141_usb_disable,
			.custom_priv = (void *)2, /* Port 2 */
		},
	},
	[3] = { /* USB 1.1 port */
		.flags = STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.labels_num = 1,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_RANGE("PIO4", 4, 5),
			},
			.sysconf_values_num = 1,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* Alt. function 1 on PIO4.4 & PIO4.5 */
				STM_PAD_SYS_CFG(20, 9, 12, 5),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection */
				STM_PAD_PIO_IN(4, 4),
				/* USB power enable */
				STM_PAD_PIO_OUT(4, 5),
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
			/* EHCI */
			STM_PLAT_RESOURCE_MEM(0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(93), -1),
			/* OHCI */
			STM_PLAT_RESOURCE_MEM(0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(94), -1),
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
			.dma_mask = &stx7141_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7141_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			/* EHCI */
			STM_PLAT_RESOURCE_MEM(0xfeaffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(95), -1),
			/* OHCI */
			STM_PLAT_RESOURCE_MEM(0xfeaffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(96), -1),
			/* Wrapper glue */
			STM_PLAT_RESOURCE_MEM(0xfea00000, 0x100),
			/* Protocol converter */
			STM_PLAT_RESOURCE_MEM(0xfeafff00, 0x100),
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
		.num_resources = 6,
		.resource = (struct resource[]) {
			/* no EHCI */
			{ .flags = 0 },
			{ .flags = 0 },
			/* OHCI */
			STM_PLAT_RESOURCE_MEM(0xfebffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(97), -1),
			/* Wrapper glue */
			STM_PLAT_RESOURCE_MEM(0xfeb00000, 0x100),
			/* Protocol converter */
			STM_PLAT_RESOURCE_MEM(0xfebfff00, 0x100),
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
		.num_resources = 6,
		.resource = (struct resource[]) {
			/* no EHCI */
			{ .flags = 0 },
			{ .flags = 0 },
			/* OHCI */
			STM_PLAT_RESOURCE_MEM(0xfecffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(98), -1),
			/* Wrapper glue */
			STM_PLAT_RESOURCE_MEM(0xfec00000, 0x100),
			/* Protocol converter */
			STM_PLAT_RESOURCE_MEM(0xfecfff00, 0x100),
		},
	},
};

static int stx7141_usb_enable(void *priv)
{
	int port = (int)priv;
	static int first = 1;
	struct sysconf_field *sc;
	unsigned gpio;

	if (first) {
		/* ENABLE_USB48_CLK: Enable 48 MHz clock */
		sc = sysconf_claim(SYS_CFG, 4, 5, 5, "USB");
		sysconf_write(sc, 1);
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
	gpio = stx7141_usb_platform_data[0].pad_config->gpio_values[0].gpio;

	return gpio_direction_output(gpio, 0);
}

static int stx7141_usb_disable(void *priv)
{
	int port = (int)priv;
	struct sysconf_field *sc;
	unsigned gpio;

	/* Power down USB */
	sc = sysconf_claim(SYS_CFG, 32, 7 + port, 7 + port, "USB");
	sysconf_write(sc, 1);

	gpio = stx7141_usb_platform_data[0].pad_config->gpio_values[0].gpio;

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

	platform_device_register(&stx7141_sata_device);
}
