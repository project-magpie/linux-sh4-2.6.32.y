/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/stm/miphy.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7108.h>
#include <linux/delay.h>
#include <asm/irq-ilc.h>

/* --------------------------------------------------------------------
 *           Ethernet MAC resources (PAD and Retiming)
 * --------------------------------------------------------------------*/

/* MII Default Retiming Configuration */
static struct stx7108_pio_retime_config mii_retime_bypass = {
	.retime = 0,
	.clk1notclk0 = -1,
	.clknotdata = 0,
	.double_edge = -1,
	.invertclk = -1,
	.delay_input = -1,

};

static struct stx7108_pio_retime_config mii_retime_clock[] = {
	[0] = {
		.retime = -1,
		.clk1notclk0 = 0,
		.clknotdata = 1,
		.double_edge = -1,
		.invertclk = -1,
		.delay_input = -1,
	},
	[1] = {
		.retime = -1,
		.clk1notclk0 = 1,
		.clknotdata = 1,
		.double_edge = -1,
		.invertclk = -1,
		.delay_input = -1,
	}
};

static struct stx7108_pio_retime_config mii_retime_phy_clock = {
	.retime = -1,
	.clk1notclk0 = 0,
	.clknotdata = 1,
	.double_edge = -1,
	.invertclk = -1,
	.delay_input = -1,
};

static struct stx7108_pio_retime_config mii_retime_data[] = {
	[0] = {
		.retime = 1,
		.clk1notclk0 = 0,
		.clknotdata = 0,
		.double_edge = -1,
		.invertclk = -1,
		.delay_input = -1,
	},
	[1] = {
		.retime = 1,
		.clk1notclk0 = 1,
		.clknotdata = 0,
		.double_edge = -1,
		.invertclk = -1,
		.delay_input = -1,
	}
};

/* GMII (GTX) Default Retiming Configuration */
static struct stx7108_pio_retime_config gmii_gtx_retime_clock = {
	.retime = 1,
	.clk1notclk0 = 1,
	.clknotdata = 1,
	.double_edge = 0,
	.invertclk = -1,
	.delay_input = 1,
};

/* RGMII (GTX) Default Retiming Configuration */
static struct stx7108_pio_retime_config rgmii_retime_bypass = {
	.retime = 0,
	.clk1notclk0 = -1,
	.clknotdata = 0,
	.double_edge = 1,
	.invertclk = -1,
	.delay_input = 0,

};

static struct stx7108_pio_retime_config rgmii_retime_clock[] = {
	[0] = {
		.retime = -1,
		.clk1notclk0 = 0,
		.clknotdata = 1,
		.double_edge = 1,
		.invertclk = -1,
		.delay_input = 0,
	},
	[1] = {
		.retime = -1,
		.clk1notclk0 = 1,
		.clknotdata = 1,
		.double_edge = 1,
		.invertclk = -1,
		.delay_input = 0,
	}
};

static struct stx7108_pio_retime_config rgmii_gtx_retime_clock = {
	.retime = 1,
	.clk1notclk0 = 1,
	.clknotdata = 1,
	.double_edge = 1,
	.invertclk = -1,
	.delay_input = 0,
};

static struct stx7108_pio_retime_config rgmii_retime_data[] = {
	[0] = {
		.retime = 1,
		.clk1notclk0 = 0,
		.clknotdata = 0,
		.double_edge = 1,
		.invertclk = -1,
		.delay_input = 0,
	},
	[1] = {
		.retime = 1,
		.clk1notclk0 = 1,
		.clknotdata = 0,
		.double_edge = 1,
		.invertclk = -1,
		.delay_input = 0,
	}
};

#define DATA_IN(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming[_gmac], \
		}, \
	}

#define DATA_OUT(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming[_gmac], \
		}, \
	}

#define CLOCK_IN(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming[_gmac], \
		}, \
	}

#define CLOCK_OUT(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming[_gmac], \
		}, \
	}

#define BYPASS_IN(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming, \
		}, \
	}

#define BYPASS_OUT(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming, \
		}, \
	}

#define PHY_CLOCK(_gmac, _port, _pin, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_unknown, \
		.name = "PHYCLK", \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &_retiming, \
		}, \
	}

static struct stm_pad_config stx7108_ethernet_mii_pad_configs[] = {
	[0] =  {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 6, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(0, 6, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(0, 6, 2, mii_retime_data),	/* TXD[2] */
			DATA_OUT(0, 6, 3, mii_retime_data),	/* TXD[3] */
			DATA_OUT(0, 7, 0, mii_retime_data),	/* TXER */
			DATA_OUT(0, 7, 1, mii_retime_data),	/* TXEN */
			CLOCK_IN(0, 7, 2, mii_retime_clock),	/* TXCLK */
			BYPASS_IN(0, 7, 3, mii_retime_bypass),	/* COL */
			BYPASS_OUT(0, 7, 4, mii_retime_bypass),	/* MDIO*/
			CLOCK_OUT(0, 7, 5, mii_retime_clock),	/* MDC */
			BYPASS_IN(0, 7, 6, mii_retime_bypass),	/* CRS */
			BYPASS_IN(0, 7, 7, mii_retime_bypass),	/* MDINT */
			DATA_IN(0, 8, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(0, 8, 1, mii_retime_data),	/* RXD[1] */
			DATA_IN(0, 8, 2, mii_retime_data),	/* RXD[2] */
			DATA_IN(0, 8, 3, mii_retime_data),	/* RXD[3] */
			DATA_IN(0, 9, 0, mii_retime_data),	/* RXDV */
			DATA_IN(0, 9, 1, mii_retime_data),	/* RX_ER */
			CLOCK_IN(0, 9, 2, mii_retime_clock),	/* RXCLK */
			PHY_CLOCK(0, 9, 3, mii_retime_phy_clock),/* PHYCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYS_CFG_BANK(2, 53, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(2, 27, 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(2, 27, 5, 5, 1),
		},
	},
	[1] =  {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(1, 15, 5, mii_retime_phy_clock),/* PHYCLK */
			BYPASS_IN(1, 15, 6, mii_retime_bypass),	/* MDINT */
			DATA_OUT(1, 15, 7, mii_retime_data),	/* TXEN */
			DATA_OUT(1, 16, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(1, 16, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(1, 16, 2, mii_retime_data),	/* TXD[2] */
			DATA_OUT(1, 16, 3, mii_retime_data),	/* TXD[3] */
			CLOCK_IN(1, 17, 0, mii_retime_clock),	/* TXCLK */
			DATA_OUT(1, 17, 1, mii_retime_data),	/* TXER */
			BYPASS_IN(1, 17, 2, mii_retime_bypass),	/* CRS */
			BYPASS_IN(1, 17, 3, mii_retime_bypass),	/* COL */
			BYPASS_OUT(1, 17, 4, mii_retime_bypass),/* MDIO */
			CLOCK_OUT(1, 17, 5, mii_retime_clock),	/* MDC */
			DATA_IN(1, 17, 6, mii_retime_data),	/* RXDV */
			DATA_IN(1, 17, 7, mii_retime_data),	/* RX_ER */
			DATA_IN(1, 18, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(1, 18, 1, mii_retime_data),	/* RXD[1] */
			DATA_IN(1, 18, 2, mii_retime_data),	/* RXD[2] */
			DATA_IN(1, 18, 3, mii_retime_data),	/* RXD[3] */
			CLOCK_IN(1, 19, 0, mii_retime_clock),	/* RXCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYS_CFG_BANK(4, 67, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(4, 23, 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(4, 23, 5, 5, 1),
		},
	},
};

static struct stm_pad_config stx7108_ethernet_gmii_pad_configs[] = {
	[0] =  {
		.gpios_num = 28,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 6, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(0, 6, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(0, 6, 2, mii_retime_data),	/* TXD[2] */
			DATA_OUT(0, 6, 3, mii_retime_data),	/* TXD[3] */
			DATA_OUT(0, 6, 4, mii_retime_data),	/* TXD[4] */
			DATA_OUT(0, 6, 5, mii_retime_data),	/* TXD[5] */
			DATA_OUT(0, 6, 6, mii_retime_data),	/* TXD[6] */
			DATA_OUT(0, 6, 7, mii_retime_data),	/* TXD[7] */
			DATA_OUT(0, 7, 0, mii_retime_data),	/* TXER */
			DATA_OUT(0, 7, 1, mii_retime_data),	/* TXEN */
			CLOCK_IN(0, 7, 2, mii_retime_clock),	/* TXCLK */
			BYPASS_IN(0, 7, 3, mii_retime_bypass),	/* COL */
			BYPASS_OUT(0, 7, 4, mii_retime_bypass),	/* MDIO */
			CLOCK_OUT(0, 7, 5, mii_retime_clock),	/* MDC */
			BYPASS_IN(0, 7, 6, mii_retime_bypass),	/* CRS */
			BYPASS_IN(0, 7, 7, mii_retime_bypass),	/* MDINT */
			DATA_IN(0, 8, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(0, 8, 1, mii_retime_data),	/* RXD[1] */
			DATA_IN(0, 8, 2, mii_retime_data),	/* RXD[2] */
			DATA_IN(0, 8, 3, mii_retime_data),	/* RXD[3] */
			DATA_IN(0, 8, 4, mii_retime_data),	/* RXD[4] */
			DATA_IN(0, 8, 5, mii_retime_data),	/* RXD[5] */
			DATA_IN(0, 8, 6, mii_retime_data),	/* RXD[6] */
			DATA_IN(0, 8, 7, mii_retime_data),	/* RXD[7] */
			DATA_IN(0, 9, 0, mii_retime_data),	/* RXDV */
			DATA_IN(0, 9, 1, mii_retime_data),	/* RX_ER */
			CLOCK_IN(0, 9, 2, mii_retime_clock),	/* RXCLK */
			PHY_CLOCK(0, 9, 3, gmii_gtx_retime_clock),/* PHYCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYS_CFG_BANK(2, 53, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(2, 27, 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(2, 27, 5, 5, 1),
		},
	},
	[1] =  {
		.gpios_num = 28,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(1, 15, 5, gmii_gtx_retime_clock),/* PHYCLK */
			BYPASS_IN(1, 15, 6, mii_retime_bypass),	/* MDINT */
			DATA_OUT(1, 15, 7, mii_retime_data),	/* TXEN */
			DATA_OUT(1, 16, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(1, 16, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(1, 16, 2, mii_retime_data),	/* TXD[2] */
			DATA_OUT(1, 16, 3, mii_retime_data),	/* TXD[3] */
			DATA_OUT(1, 16, 4, mii_retime_data),	/* TXD[4] */
			DATA_OUT(1, 16, 5, mii_retime_data),	/* TXD[5] */
			DATA_OUT(1, 16, 6, mii_retime_data),	/* TXD[6] */
			DATA_OUT(1, 16, 7, mii_retime_data),	/* TXD[7] */
			CLOCK_IN(1, 17, 0, mii_retime_clock),	/* TXCLK */
			DATA_OUT(1, 17, 1, mii_retime_data),	/* TXER */
			BYPASS_IN(1, 17, 2, mii_retime_bypass),	/* CRS */
			BYPASS_IN(1, 17, 3, mii_retime_bypass),	/* COL */
			BYPASS_OUT(1, 17, 4, mii_retime_bypass),/* MDIO */
			CLOCK_OUT(1, 17, 5, mii_retime_clock),	/* MDC */
			DATA_IN(1, 17, 6, mii_retime_data),	/* RXDV */
			DATA_IN(1, 17, 7, mii_retime_data),	/* RX_ER */
			DATA_IN(1, 18, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(1, 18, 1, mii_retime_data),	/* RXD[1] */
			DATA_IN(1, 18, 2, mii_retime_data),	/* RXD[2] */
			DATA_IN(1, 18, 3, mii_retime_data),	/* RXD[3] */
			DATA_IN(1, 18, 4, mii_retime_data),	/* RXD[4] */
			DATA_IN(1, 18, 5, mii_retime_data),	/* RXD[5] */
			DATA_IN(1, 18, 6, mii_retime_data),	/* RXD[6] */
			DATA_IN(1, 18, 7, mii_retime_data),	/* RXD[7] */
			CLOCK_IN(1, 19, 0, mii_retime_clock),	/* RXCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYS_CFG_BANK(4, 67, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(4, 23, 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(4, 23, 5, 5, 1),
		},
	},
};

static struct stm_pad_config stx7108_ethernet_rgmii_pad_configs[] = {
	[0] =  {
		.gpios_num = 18,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 6, 0, rgmii_retime_data),	/* TXD[0] */
			DATA_OUT(0, 6, 1, rgmii_retime_data),	/* TXD[1] */
			DATA_OUT(0, 6, 2, rgmii_retime_data),	/* TXD[2] */
			DATA_OUT(0, 6, 3, rgmii_retime_data),	/* TXD[3] */
			DATA_OUT(0, 7, 1, rgmii_retime_data),	/* TXEN */
			CLOCK_IN(0, 7, 2, rgmii_retime_clock),	/* TXCLK */
			BYPASS_IN(0, 7, 3, rgmii_retime_bypass),/* COL */
			BYPASS_OUT(0, 7, 4, rgmii_retime_bypass),/* MDIO */
			CLOCK_OUT(0, 7, 5, rgmii_retime_clock),	/* MDC */
			BYPASS_IN(0, 7, 6, rgmii_retime_bypass),/* CRS */
			BYPASS_IN(0, 7, 7, rgmii_retime_bypass),/* MDINT */
			DATA_IN(0, 8, 0, rgmii_retime_data),	/* RXD[0] */
			DATA_IN(0, 8, 1, rgmii_retime_data),	/* RXD[1] */
			DATA_IN(0, 8, 2, rgmii_retime_data),	/* RXD[2] */
			DATA_IN(0, 8, 3, rgmii_retime_data),	/* RXD[3] */
			DATA_IN(0, 9, 0, rgmii_retime_data),	/* RXDV */
			CLOCK_IN(0, 9, 2, rgmii_retime_clock),	/* RXCLK */
			PHY_CLOCK(0, 9, 3, rgmii_gtx_retime_clock),/* PHYCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYS_CFG_BANK(2, 53, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(2, 27, 2, 4, 1),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(2, 27, 5, 5, 1),
		},
	},
	[1] =  {
		.gpios_num = 18,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(1, 15, 5, rgmii_gtx_retime_clock),/* PHYCLK */
			BYPASS_IN(1, 15, 6, rgmii_retime_bypass),/* MDINT */
			DATA_OUT(1, 15, 7, rgmii_retime_data),	/* TXEN */
			DATA_OUT(1, 16, 0, rgmii_retime_data),	/* TXD[0] */
			DATA_OUT(1, 16, 1, rgmii_retime_data),	/* TXD[1] */
			DATA_OUT(1, 16, 2, rgmii_retime_data),	/* TXD[2] */
			DATA_OUT(1, 16, 3, rgmii_retime_data),	/* TXD[3] */
			CLOCK_IN(1, 17, 0, rgmii_retime_clock),	/* TXCLK */
			BYPASS_IN(1, 17, 2, rgmii_retime_bypass),/* CRS */
			BYPASS_IN(1, 17, 3, rgmii_retime_bypass),/* COL */
			BYPASS_OUT(1, 17, 4, rgmii_retime_bypass),/* MDIO */
			CLOCK_OUT(1, 17, 5, rgmii_retime_clock),/* MDC */
			DATA_IN(1, 17, 6, rgmii_retime_data),	/* RXDV */
			DATA_IN(1, 18, 0, rgmii_retime_data),	/* RXD[0] */
			DATA_IN(1, 18, 1, rgmii_retime_data),	/* RXD[1] */
			DATA_IN(1, 18, 2, rgmii_retime_data),	/* RXD[2] */
			DATA_IN(1, 18, 3, rgmii_retime_data),	/* RXD[3] */
			CLOCK_IN(1, 19, 0, rgmii_retime_clock),	/* RXCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYS_CFG_BANK(4, 67, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(4, 23, 2, 4, 1),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(4, 23, 5, 5, 1),
		},
	},
};
static struct stm_pad_config stx7108_ethernet_rmii_pad_configs[] = {
	[0] = {
		.gpios_num = 12,
		.gpios = (struct stm_pad_gpio []) {
			BYPASS_OUT(0, 6, 0, mii_retime_bypass),	/* TXD[0] */
			BYPASS_OUT(0, 6, 1, mii_retime_bypass),	/* TXD[1] */
			BYPASS_OUT(0, 7, 0, mii_retime_bypass),	/* TXER */
			BYPASS_OUT(0, 7, 1, mii_retime_bypass),	/* TXEN */
			BYPASS_OUT(0, 7, 4, mii_retime_bypass),	/* MDIO */
			BYPASS_OUT(0, 7, 5, mii_retime_bypass),	/* MDC */
			BYPASS_IN(0, 7, 7, mii_retime_bypass),	/* MDINT */
			BYPASS_IN(0, 8, 0, mii_retime_bypass),	/* RXD.0 */
			BYPASS_IN(0, 8, 1, mii_retime_bypass),	/* RXD.1 */
			BYPASS_IN(0, 9, 0, mii_retime_bypass),	/* RXDV */
			BYPASS_IN(0, 9, 1, mii_retime_bypass),	/* RX_ER */
			PHY_CLOCK(0, 9, 3, mii_retime_phy_clock),/* PHYCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYS_CFG_BANK(2, 53, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(2, 27, 2, 4, 4),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(2, 27, 5, 5, 1),
		},
	},
	[1] =  {
		.gpios_num = 12,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(1, 15, 5, mii_retime_phy_clock),/* PHYCLK */
			BYPASS_IN(1, 15, 6, mii_retime_bypass),	/* MDINT */
			DATA_OUT(1, 15, 7, mii_retime_data),	/* TXEN */
			DATA_OUT(1, 16, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(1, 16, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(1, 17, 1, mii_retime_data),	/* TXER */
			BYPASS_OUT(1, 17, 4, mii_retime_bypass),/* MDIO */
			CLOCK_OUT(1, 17, 5, mii_retime_clock),	/* MDC */
			DATA_IN(1, 17, 6, mii_retime_data),	/* RXDV */
			DATA_IN(1, 17, 7, mii_retime_data),	/* RX_ER */
			DATA_IN(1, 18, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(1, 18, 1, mii_retime_data),	/* RXD[1] */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYS_CFG_BANK(4, 67, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(4, 23, 2, 4, 4),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(4, 23, 5, 5, 1),
		},
	},
};

static struct stm_pad_config stx7108_ethernet_reverse_mii_pad_configs[] = {
	[0] = {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 6, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(0, 6, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(0, 6, 2, mii_retime_data),	/* TXD[2] */
			DATA_OUT(0, 6, 3, mii_retime_data),	/* TXD[3] */
			DATA_OUT(0, 7, 0, mii_retime_data),	/* TXER */
			DATA_OUT(0, 7, 1, mii_retime_data),	/* TXEN */
			CLOCK_IN(0, 7, 2, mii_retime_clock),	/* TXCLK */
			BYPASS_OUT(0, 7, 3, mii_retime_bypass),	/* COL */
			BYPASS_OUT(0, 7, 4, mii_retime_bypass),	/* MDIO*/
			CLOCK_IN(0, 7, 5, mii_retime_clock),	/* MDC */
			BYPASS_OUT(0, 7, 6, mii_retime_bypass),	/* CRS */
			BYPASS_IN(0, 7, 7, mii_retime_bypass),	/* MDINT */
			DATA_IN(0, 8, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(0, 8, 1, mii_retime_data),	/* RXD[1] */
			DATA_IN(0, 8, 2, mii_retime_data),	/* RXD[2] */
			DATA_IN(0, 8, 3, mii_retime_data),	/* RXD[3] */
			DATA_IN(0, 9, 0, mii_retime_data),	/* RXDV */
			DATA_IN(0, 9, 1, mii_retime_data),	/* RX_ER */
			CLOCK_IN(0, 9, 2, mii_retime_clock),	/* RXCLK */
			PHY_CLOCK(0, 9, 3, mii_retime_phy_clock),/* PHYCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYS_CFG_BANK(2, 53, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(2, 27, 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(2, 27, 5, 5, 0),
		},
	},
	[1] =  {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(1, 15, 5, mii_retime_phy_clock),/* PHYCLK */
			BYPASS_IN(1, 15, 6, mii_retime_bypass),	/* MDINT */
			DATA_OUT(1, 15, 7, mii_retime_data),	/* TXEN */
			DATA_OUT(1, 16, 0, mii_retime_data),	/* TXD[0] */
			DATA_OUT(1, 16, 1, mii_retime_data),	/* TXD[1] */
			DATA_OUT(1, 16, 2, mii_retime_data),	/* TXD[2] */
			DATA_OUT(1, 16, 3, mii_retime_data),	/* TXD[3] */
			CLOCK_IN(1, 17, 0, mii_retime_clock),	/* TXCLK */
			DATA_OUT(1, 17, 1, mii_retime_data),	/* TXER */
			BYPASS_OUT(1, 17, 2, mii_retime_bypass),/* CRS */
			BYPASS_OUT(1, 17, 3, mii_retime_bypass),/* COL */
			BYPASS_OUT(1, 17, 4, mii_retime_bypass),/* MDIO */
			CLOCK_IN(1, 17, 5, mii_retime_clock),	/* MDC */
			DATA_IN(1, 17, 6, mii_retime_data),	/* RXDV */
			DATA_IN(1, 17, 7, mii_retime_data),	/* RX_ER */
			DATA_IN(1, 18, 0, mii_retime_data),	/* RXD[0] */
			DATA_IN(1, 18, 1, mii_retime_data),	/* RXD[1] */
			DATA_IN(1, 18, 2, mii_retime_data),	/* RXD[2] */
			DATA_IN(1, 18, 3, mii_retime_data),	/* RXD[3] */
			CLOCK_IN(1, 19, 0, mii_retime_clock),	/* RXCLK */
		},
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYS_CFG_BANK(4, 67, 0, 0, 1),
			/* MIIx_PHY_SEL */
			STM_PAD_SYS_CFG_BANK(4, 23, 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYS_CFG_BANK(4, 23, 5, 5, 1),
		},
	},
};

static void stx7108_ethernet_rmii_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static void stx7108_ethernet_gtx_speed(void *priv, unsigned int speed)
{
	void (*txclk_select)(int txclk_250_not_25_mhz) = priv;

	if (txclk_select)
		txclk_select(speed == SPEED_1000);
}

static struct plat_stmmacenet_data stx7108_ethernet_platform_data[] = {
	{
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		.tx_coe = 1,
		.bugged_jumbo =1,
		.pmt = 1,
		.init = &stmmac_claim_resource,
	}, {
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		.tx_coe = 1,
		.bugged_jumbo =1,
		.pmt = 1,
		.init = &stmmac_claim_resource,
	}
};

static struct platform_device stx7108_ethernet_devices[] = {
	{
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda88000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(21), -1),
		},
		.dev.platform_data = &stx7108_ethernet_platform_data[0],
	}, {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe730000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(23), -1),
		},
		.dev.platform_data = &stx7108_ethernet_platform_data[1],
	}
};

void __init stx7108_configure_ethernet(int port,
		struct stx7108_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stx7108_ethernet_devices)];
	struct stx7108_ethernet_config default_config;
	struct plat_stmmacenet_data *plat_data;
	struct stm_pad_config *pad_config;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7108_ethernet_devices));
	BUG_ON(configured[port]++);

	if (!config)
		config = &default_config;

	plat_data = &stx7108_ethernet_platform_data[port];

	switch (config->mode) {
	case stx7108_ethernet_mode_mii:
		pad_config = &stx7108_ethernet_mii_pad_configs[port];
		if (config->ext_clk)
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		else
			stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		break;
	case stx7108_ethernet_mode_gmii:
		pad_config = &stx7108_ethernet_gmii_pad_configs[port];
		stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		break;
	case stx7108_ethernet_mode_gmii_gtx:
		pad_config = &stx7108_ethernet_gmii_pad_configs[port];
		stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		plat_data->fix_mac_speed = stx7108_ethernet_gtx_speed;
		plat_data->bsp_priv = config->txclk_select;
		break;
	case stx7108_ethernet_mode_rgmii_gtx:
		/* This mode is similar to GMII (GTX) except the data
		 * buses are reduced down to 4 bits and the 2 error
		 * signals are removed. The data rate is maintained by
		 * using both edges of the clock. This also explains
		 * the different retiming configuration for this mode.
		 */
		pad_config = &stx7108_ethernet_rgmii_pad_configs[port];
		stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		plat_data->fix_mac_speed = stx7108_ethernet_gtx_speed;
		plat_data->bsp_priv = config->txclk_select;
		break;
	case stx7108_ethernet_mode_rmii:
		pad_config = &stx7108_ethernet_rmii_pad_configs[port];
		if (config->ext_clk)
			stm_pad_set_pio_in(pad_config, "PHYCLK", 2 + port);
		else
			stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		plat_data->fix_mac_speed = stx7108_ethernet_rmii_speed;
		/* MIIx_MAC_SPEED_SEL */
		if (port == 0)
			plat_data->bsp_priv = sysconf_claim(SYS_CFG_BANK2,
					27, 1, 1, "stmmac");
		else
			plat_data->bsp_priv = sysconf_claim(SYS_CFG_BANK4,
					23, 1, 1, "stmmac");
		break;
	case stx7108_ethernet_mode_reverse_mii:
		pad_config = &stx7108_ethernet_reverse_mii_pad_configs[port];
		if (config->ext_clk)
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		else
			stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		break;
	default:
		BUG();
		return;
	}

	plat_data->custom_cfg = (void *) pad_config;
	plat_data->bus_id = config->phy_bus;

	platform_device_register(&stx7108_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7108_usb_dma_mask = DMA_BIT_MASK(32);

#define USB_HOST_PWR	"USB_HOST_PWR"
#define USB_PHY_PWR	"USB_PHY_PWR"
#define USB_PHY_SH_CTL	"USB_PHY_SH_CTL"
#define USB_PWR_ACK	"USB_PWR_ACK"

static void stx7108_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;
	int phy_value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, USB_HOST_PWR, value);
	stm_device_sysconf_write(device_state, USB_PHY_PWR, phy_value);
	stm_device_sysconf_write(device_state, USB_PHY_SH_CTL, phy_value);

	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_PWR_ACK)
			== value)
			break;
		mdelay(10);
	}
}

static struct stm_plat_usb_data stx7108_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			.power = stx7108_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG_BANK(4, 46, 0, 0,
					USB_HOST_PWR),
				STM_DEVICE_SYS_CFG_BANK(4, 44, 0, 0,
					USB_PHY_PWR),
				STM_DEVICE_SYS_CFG_BANK(4, 44, 3, 3,
					USB_PHY_SH_CTL),
				STM_DEVICE_SYS_STA_BANK(4,  2, 0, 0,
					USB_PWR_ACK),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(23, 6, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(23, 7, 1),
				},
			},
		},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			.power = stx7108_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG_BANK(4, 46, 1, 1,
					USB_HOST_PWR),
				STM_DEVICE_SYS_CFG_BANK(4, 44, 1, 1,
					USB_PHY_PWR),
				STM_DEVICE_SYS_CFG_BANK(4, 44, 4, 4,
					USB_PHY_SH_CTL),
				STM_DEVICE_SYS_STA_BANK(4,  2, 1, 1,
					USB_PWR_ACK),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(24, 0, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(24, 1, 1),
				},
			},
		},
	},
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
		.device_config = &(struct stm_device_config){
			.power = stx7108_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYS_CFG_BANK(4, 46, 2, 2,
					USB_HOST_PWR),
				STM_DEVICE_SYS_CFG_BANK(4, 44, 2, 2,
					USB_PHY_PWR),
				STM_DEVICE_SYS_CFG_BANK(4, 44, 5, 5,
					USB_PHY_SH_CTL),
				STM_DEVICE_SYS_STA_BANK(4,  2, 2, 2,
					USB_PWR_ACK),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(24, 2, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(24, 3, 1),
				},
			},
		},
	},
};

static struct platform_device stx7108_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7108_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7108_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",
					0xfe000000, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",
					0xfe0ffc00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",
					0xfe0ffe00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",
					0xfe0fff00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(59), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(62), -1),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stx7108_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7108_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",
					0xfe100000, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",
					0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",
					0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",
					0xfe1fff00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(60), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(63), -1),
		},
	},
	[2] = {
		.name = "stm-usb",
		.id = 2,
		.dev = {
			.dma_mask = &stx7108_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stx7108_usb_platform_data[2],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",
					0xfe200000, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",
					0xfe2ffc00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",
					0xfe2ffe00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",
					0xfe2fff00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(61), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(64), -1),
		},
	},
};

void __init stx7108_configure_usb(int port)
{
	static int osc_initialized;
	static int configured[ARRAY_SIZE(stx7108_usb_devices)];
	struct sysconf_field *sc;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7108_usb_devices));

	BUG_ON(configured[port]++);

	if (!osc_initialized++) {
		/* USB2TRIPPHY_OSCIOK */
		sc = sysconf_claim(SYS_CFG_BANK4, 44, 6, 6, "USB");
		sysconf_write(sc, 1);
	}

	platform_device_register(&stx7108_usb_devices[port]);
}



/* SATA resources --------------------------------------------------------- */
static void stx7108_sata_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 0 : 1;
	int i;

	stm_device_sysconf_write(device_state, "SATA_PWR", value);

	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "SATA_ACK")
				== value)
			break;
		mdelay(10);
	}

	return ;
}

static struct platform_device stx7108_sata_devices[] = {
	[0] = {
		.name = "sata-stm",
		.id = 0,
		.dev.platform_data = &(struct stm_plat_sata_data) {
			.phy_init = 0,
			.pc_glue_logic_init = 0,
			.only_32bit = 0,
			.device_config = &(struct stm_device_config){
				.power = stx7108_sata_power,
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYS_CFG_BANK(4, 46, 3, 3,
						"SATA_PWR"),
					STM_DEVICE_SYS_STA_BANK(4, 2, 3, 3,
						"SATA_ACK"),
				},
			},
		},
		.num_resources = 3,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe768000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(57), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(55), -1),
		},
	},
	[1] = {
		.name = "sata-stm",
		.id = 1,
		.dev.platform_data = &(struct stm_plat_sata_data) {
			.phy_init = 0,
			.pc_glue_logic_init = 0,
			.only_32bit = 0,
			.device_config = &(struct stm_device_config){
				.power = stx7108_sata_power,
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYS_CFG_BANK(4, 46, 4, 4,
						"SATA_PWR"),
					STM_DEVICE_SYS_STA_BANK(4, 2, 4, 4,
						"SATA_ACK"),
				},
			},
		},
		.num_resources = 3,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe769000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(58), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(56), -1),
		},
	}
};

void __init stx7108_configure_sata(int port)
{
	static int configured[ARRAY_SIZE(stx7108_sata_devices)];
	static int phys_initialized;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7108_sata_devices));
	BUG_ON(configured[port]++);

	/* PHYs require this horrible initialization to be done now... */
	if (!phys_initialized++) {
		struct stm_miphy_sysconf_soft_jtag jtag;
		struct stm_miphy miphy = {
			.ports_num = 2,
			.jtag_tick = stm_miphy_sysconf_jtag_tick,
			.jtag_priv = &jtag,
		};
		struct sysconf_field *sc;

		jtag.tck = sysconf_claim(SYS_CFG_BANK1, 3, 20, 20, "SATA");
		BUG_ON(!jtag.tck);
		jtag.tms = sysconf_claim(SYS_CFG_BANK1, 3, 23, 23, "SATA");
		BUG_ON(!jtag.tms);
		jtag.tdi = sysconf_claim(SYS_CFG_BANK1, 3, 22, 22, "SATA");
		BUG_ON(!jtag.tdi);
		jtag.tdo = sysconf_claim(SYS_STA_BANK1, 4, 1, 1, "SATA");
		BUG_ON(!jtag.tdo);

		/* Shut down both PHYs first, using SATA_x_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 3, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 3);
		sysconf_release(sc);

		/* conf_sata_tap_en */
		sc = sysconf_claim(SYS_CFG_BANK1, 3, 13, 13, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);

		/* TMS should be set to 1 when taking the TAP
		 * machine out of reset... */
		sysconf_write(jtag.tms, 1);

		/* sata_trst_fromconf */
		sc = sysconf_claim(SYS_CFG_BANK1, 3, 21, 21, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
		udelay(100);

		/* Power up & initialize PHY(s) (one by one) */

		/* SATA_0_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 3, 3, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 0);
		stm_miphy_init(&miphy, 0);
		sysconf_release(sc);

		/* SATA_1_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 4, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 0);
		stm_miphy_init(&miphy, 1);
		sysconf_release(sc);
	}

	platform_device_register(&stx7108_sata_devices[port]);
}
