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
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7108.h>
#include <linux/delay.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stx7108_pio_retime_config stx7108_ethernet_retime_bypass = {
	.retime = 0,
	.clk1notclk0 = -1,
	.clknotdata = 0,
	.double_edge = -1,
	.invertclk = -1,
	.delay_input = -1,

};

static struct stx7108_pio_retime_config stx7108_ethernet_retime_clock[] = {
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

static struct stx7108_pio_retime_config stx7108_ethernet_retime_phy_clock = {
	.retime = -1,
	.clk1notclk0 = 0,
	.clknotdata = 1,
	.double_edge = -1,
	.invertclk = -1,
	.delay_input = -1,
};

static struct stx7108_pio_retime_config stx7108_ethernet_retime_gtx_clock = {
	.retime = -1,
	.clk1notclk0 = 1,
	.clknotdata = 1,
	.double_edge = 0,
	.invertclk = -1,
	.delay_input = -1,
};

static struct stx7108_pio_retime_config stx7108_ethernet_retime_data[] = {
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

#define STX7108_PIO_ETH_DATA_IN(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_data[_gmac], \
		}, \
	}

#define STX7108_PIO_ETH_DATA_OUT(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_data[_gmac], \
		}, \
	}

#define STX7108_PIO_ETH_CLOCK_IN(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_clock[_gmac], \
		}, \
	}

#define STX7108_PIO_ETH_CLOCK_OUT(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_clock[_gmac], \
		}, \
	}

#define STX7108_PIO_ETH_BYPASS_IN(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_bypass, \
		}, \
	}

#define STX7108_PIO_ETH_BYPASS_OUT(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _gmac + 1, \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_bypass, \
		}, \
	}

#define STX7108_PIO_ETH_PHY_CLOCK(_gmac, _port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_unknown, \
		.name = "PHYCLK", \
		.priv = &(struct stx7108_pio_config) { \
			.retime = &stx7108_ethernet_retime_phy_clock, \
		}, \
	}

static struct stm_pad_config stx7108_ethernet_mii_pad_configs[] = {
	[0] =  {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			STX7108_PIO_ETH_DATA_OUT(0, 6, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 2),	/* TXD[2] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 3),	/* TXD[3] */
			STX7108_PIO_ETH_DATA_OUT(0, 7, 0),	/* TXER */  
			STX7108_PIO_ETH_DATA_OUT(0, 7, 1),	/* TXEN */  
			STX7108_PIO_ETH_CLOCK_IN(0, 7, 2),	/* TXCLK */ 
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 3),	/* COL */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 4),	/* MDIO*/
			STX7108_PIO_ETH_CLOCK_OUT(0, 7, 5),	/* MDC */  
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 6),	/* CRS */  
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 7),	/* MDINT */
			STX7108_PIO_ETH_DATA_IN(0, 8, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 1),	/* RXD[1] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 2),	/* RXD[2] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 3),	/* RXD[3] */
			STX7108_PIO_ETH_DATA_IN(0, 9, 0),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(0, 9, 1),	/* RX_ER */ 
			STX7108_PIO_ETH_CLOCK_IN(0, 9, 2),	/* RXCLK */
			STX7108_PIO_ETH_PHY_CLOCK(0, 9, 3),	/* PHYCLK */
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
			STX7108_PIO_ETH_PHY_CLOCK(1, 15, 5),	/* PHYCLK */
			STX7108_PIO_ETH_BYPASS_IN(1, 15, 6),	/* MDINT */
			STX7108_PIO_ETH_DATA_OUT(1, 15, 7),	/* TXEN */  
			STX7108_PIO_ETH_DATA_OUT(1, 16, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 2),	/* TXD[2] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 3),	/* TXD[3] */
			STX7108_PIO_ETH_CLOCK_IN(1, 17, 0),	/* TXCLK */ 
			STX7108_PIO_ETH_DATA_OUT(1, 17, 1),	/* TXER */  
			STX7108_PIO_ETH_BYPASS_IN(1, 17, 2),	/* CRS */  
			STX7108_PIO_ETH_BYPASS_IN(1, 17, 3),	/* COL */
			STX7108_PIO_ETH_BYPASS_OUT(1, 17, 4),	/* MDIO */
			STX7108_PIO_ETH_CLOCK_OUT(1, 17, 5),	/* MDC */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 6),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 7),	/* RX_ER */ 
			STX7108_PIO_ETH_DATA_IN(1, 18, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 1),	/* RXD[1] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 2),	/* RXD[2] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 3),	/* RXD[3] */
			STX7108_PIO_ETH_CLOCK_IN(1, 19, 0),	/* RXCLK */
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
			STX7108_PIO_ETH_DATA_OUT(0, 6, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 2),	/* TXD[2] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 3),	/* TXD[3] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 4),	/* TXD[4] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 5),	/* TXD[5] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 6),	/* TXD[6] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 7),	/* TXD[7] */
			STX7108_PIO_ETH_DATA_OUT(0, 7, 0),	/* TXER */  
			STX7108_PIO_ETH_DATA_OUT(0, 7, 1),	/* TXEN */  
			STX7108_PIO_ETH_CLOCK_IN(0, 7, 2),	/* TXCLK */ 
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 3),	/* COL */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 4),	/* MDIO */
			STX7108_PIO_ETH_CLOCK_OUT(0, 7, 5),	/* MDC */  
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 6),	/* CRS */  
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 7),	/* MDINT */
			STX7108_PIO_ETH_DATA_IN(0, 8, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 1),	/* RXD[1] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 2),	/* RXD[2] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 3),	/* RXD[3] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 4),	/* RXD[4] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 5),	/* RXD[5] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 6),	/* RXD[6] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 7),	/* RXD[7] */
			STX7108_PIO_ETH_DATA_IN(0, 9, 0),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(0, 9, 1),	/* RX_ER */ 
			STX7108_PIO_ETH_CLOCK_IN(0, 9, 2),	/* RXCLK */
			STX7108_PIO_ETH_PHY_CLOCK(0, 9, 3),	/* PHYCLK */
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
			STX7108_PIO_ETH_PHY_CLOCK(1, 15, 5),	/* PHYCLK */
			STX7108_PIO_ETH_BYPASS_IN(1, 15, 6),	/* MDINT */
			STX7108_PIO_ETH_DATA_OUT(1, 15, 7),	/* TXEN */  
			STX7108_PIO_ETH_DATA_OUT(1, 16, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 2),	/* TXD[2] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 3),	/* TXD[3] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 4),	/* TXD[4] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 5),	/* TXD[5] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 6),	/* TXD[6] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 7),	/* TXD[7] */
			STX7108_PIO_ETH_CLOCK_IN(1, 17, 0),	/* TXCLK */ 
			STX7108_PIO_ETH_DATA_OUT(1, 17, 1),	/* TXER */  
			STX7108_PIO_ETH_BYPASS_IN(1, 17, 2),	/* CRS */  
			STX7108_PIO_ETH_BYPASS_IN(1, 17, 3),	/* COL */
			STX7108_PIO_ETH_BYPASS_OUT(1, 17, 4),	/* MDIO */
			STX7108_PIO_ETH_CLOCK_OUT(1, 17, 5),	/* MDC */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 6),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 7),	/* RX_ER */ 
			STX7108_PIO_ETH_DATA_IN(1, 18, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 1),	/* RXD[1] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 2),	/* RXD[2] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 3),	/* RXD[3] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 4),	/* RXD[4] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 5),	/* RXD[5] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 6),	/* RXD[6] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 7),	/* RXD[7] */
			STX7108_PIO_ETH_CLOCK_IN(1, 19, 0),	/* RXCLK */
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

static struct stm_pad_config stx7108_ethernet_rmii_pad_configs[] = {
	[0] = {
		.gpios_num = 12,
		.gpios = (struct stm_pad_gpio []) {
			STX7108_PIO_ETH_BYPASS_OUT(0, 6, 0),	/* TXD[0] */
			STX7108_PIO_ETH_BYPASS_OUT(0, 6, 1),	/* TXD[1] */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 0),	/* TXER */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 1),	/* TXEN */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 4),	/* MDIO */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 5),	/* MDC */
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 7),	/* MDINT */
			STX7108_PIO_ETH_BYPASS_IN(0, 8, 0),	/* RXD.0 */
			STX7108_PIO_ETH_BYPASS_IN(0, 8, 1),	/* RXD.1 */
			STX7108_PIO_ETH_BYPASS_IN(0, 9, 0),	/* RXDV */
			STX7108_PIO_ETH_BYPASS_IN(0, 9, 1),	/* RX_ER */
			STX7108_PIO_ETH_PHY_CLOCK(0, 9, 3),	/* PHYCLK */
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
			STX7108_PIO_ETH_PHY_CLOCK(1, 15, 5),	/* PHYCLK */
			STX7108_PIO_ETH_BYPASS_IN(1, 15, 6),	/* MDINT */
			STX7108_PIO_ETH_DATA_OUT(1, 15, 7),	/* TXEN */  
			STX7108_PIO_ETH_DATA_OUT(1, 16, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(1, 17, 1),	/* TXER */  
			STX7108_PIO_ETH_BYPASS_OUT(1, 17, 4),	/* MDIO */
			STX7108_PIO_ETH_CLOCK_OUT(1, 17, 5),	/* MDC */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 6),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 7),	/* RX_ER */ 
			STX7108_PIO_ETH_DATA_IN(1, 18, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 1),	/* RXD[1] */
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
			STX7108_PIO_ETH_DATA_OUT(0, 6, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 2),	/* TXD[2] */
			STX7108_PIO_ETH_DATA_OUT(0, 6, 3),	/* TXD[3] */
			STX7108_PIO_ETH_DATA_OUT(0, 7, 0),	/* TXER */  
			STX7108_PIO_ETH_DATA_OUT(0, 7, 1),	/* TXEN */  
			STX7108_PIO_ETH_CLOCK_IN(0, 7, 2),	/* TXCLK */ 
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 3),	/* COL */
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 4),	/* MDIO*/
			STX7108_PIO_ETH_CLOCK_IN(0, 7, 5),	/* MDC */  
			STX7108_PIO_ETH_BYPASS_OUT(0, 7, 6),	/* CRS */  
			STX7108_PIO_ETH_BYPASS_IN(0, 7, 7),	/* MDINT */
			STX7108_PIO_ETH_DATA_IN(0, 8, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 1),	/* RXD[1] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 2),	/* RXD[2] */
			STX7108_PIO_ETH_DATA_IN(0, 8, 3),	/* RXD[3] */
			STX7108_PIO_ETH_DATA_IN(0, 9, 0),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(0, 9, 1),	/* RX_ER */ 
			STX7108_PIO_ETH_CLOCK_IN(0, 9, 2),	/* RXCLK */
			STX7108_PIO_ETH_PHY_CLOCK(0, 9, 3),	/* PHYCLK */
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
			STX7108_PIO_ETH_PHY_CLOCK(1, 15, 5),	/* PHYCLK */
			STX7108_PIO_ETH_BYPASS_IN(1, 15, 6),	/* MDINT */
			STX7108_PIO_ETH_DATA_OUT(1, 15, 7),	/* TXEN */  
			STX7108_PIO_ETH_DATA_OUT(1, 16, 0),	/* TXD[0] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 1),	/* TXD[1] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 2),	/* TXD[2] */
			STX7108_PIO_ETH_DATA_OUT(1, 16, 3),	/* TXD[3] */
			STX7108_PIO_ETH_CLOCK_IN(1, 17, 0),	/* TXCLK */ 
			STX7108_PIO_ETH_DATA_OUT(1, 17, 1),	/* TXER */  
			STX7108_PIO_ETH_BYPASS_OUT(1, 17, 2),	/* CRS */  
			STX7108_PIO_ETH_BYPASS_OUT(1, 17, 3),	/* COL */
			STX7108_PIO_ETH_BYPASS_OUT(1, 17, 4),	/* MDIO */
			STX7108_PIO_ETH_CLOCK_IN(1, 17, 5),	/* MDC */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 6),	/* RXDV */  
			STX7108_PIO_ETH_DATA_IN(1, 17, 7),	/* RX_ER */ 
			STX7108_PIO_ETH_DATA_IN(1, 18, 0),	/* RXD[0] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 1),	/* RXD[1] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 2),	/* RXD[2] */
			STX7108_PIO_ETH_DATA_IN(1, 18, 3),	/* RXD[3] */
			STX7108_PIO_ETH_CLOCK_IN(1, 19, 0),	/* RXCLK */
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

static void stx7108_ethernet_gmii_gtx_speed(void *priv, unsigned int speed)
{
	void (*txclk_select)(int txclk_250_not_25_mhz) = priv;

	txclk_select(speed == SPEED_1000);
}

static struct plat_stmmacenet_data stx7108_ethernet_platform_data[] = {
	{
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		/* .fix_mac_speed set in stx7108_configure_ethernet() */
		/* .pad_config set in stx7108_configure_ethernet() */
	}, {
		.pbl = 32,
		.has_gmac = 1,
		.enh_desc = 1,
		/* .fix_mac_speed set in stx7108_configure_ethernet() */
		/* .pad_config set in stx7108_configure_ethernet() */
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
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7108_ethernet_platform_data[0],
		}
	}, {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe730000, 0x8000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(23), -1),
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7108_ethernet_platform_data[1],
		}
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
		stm_pad_set_priv(pad_config, "PHYCLK",
				&stx7108_ethernet_retime_gtx_clock);
		plat_data->fix_mac_speed = stx7108_ethernet_gmii_gtx_speed;
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

	plat_data->pad_config = pad_config;
	plat_data->bus_id = config->phy_bus;

	platform_device_register(&stx7108_ethernet_devices[port]);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7108_usb_dma_mask = DMA_BIT_MASK(32);

static struct stm_plat_usb_data stx7108_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
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
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
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
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
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

	/* Power up USB */
#if !defined(CONFIG_PM)
	sc = sysconf_claim(SYS_CFG_BANK4, 46, port, port, "USB");
	sysconf_write(sc, 0);
	sc = sysconf_claim(SYS_STA_BANK4, 2, port, port, "USB");
	while (sysconf_read(sc))
		cpu_relax();
#endif

	platform_device_register(&stx7108_usb_devices[port]);
}



/* SATA resources --------------------------------------------------------- */

static struct platform_device stx7108_sata_devices[] = {
	[0] = {
		.name = "sata-stm",
		.id = 0,
		.dev.platform_data = &(struct stm_plat_sata_data) {
			.phy_init = 0,
			.pc_glue_logic_init = 0,
			.only_32bit = 0,
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

		/* SATA_1_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 4, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 0);
		stm_miphy_init(&miphy, 1);
	}

	platform_device_register(&stx7108_sata_devices[port]);
}
