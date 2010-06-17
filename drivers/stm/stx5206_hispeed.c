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
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx5206.h>
#include <asm/irq-ilc.h>



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config stx5206_ethernet_pad_configs[] = {
	[stx5206_ethernet_mode_mii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ETHERNET_INTERFACE_ON */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* PHY_CLK_EXT: MII_PHYCLK pad function:
			 * 0: PHY clock is provided by device (output)
			 * 1: PHY clock is external (input) */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* PHY_INTF_SEL */
			STM_PAD_SYS_CFG(7, 24, 26, 0),
			/* ENMII:
			 * 0: Reverse MII mode
			 * 1: MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
		},
	},
	[stx5206_ethernet_mode_rmii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ETHERNET_INTERFACE_ON */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* PHY_CLK_EXT: MII_PHYCLK pad function:
			 * 0: PHY clock is provided by device (output)
			 * 1: PHY clock is external (input) */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* PHY_INTF_SEL */
			STM_PAD_SYS_CFG(7, 24, 26, 4),
			/* ENMII:
			 * 0: Reverse MII mode
			 * 1: MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 1),
		},
	},
	[stx5206_ethernet_mode_reverse_mii] = {
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* ETHERNET_INTERFACE_ON */
			STM_PAD_SYS_CFG(7, 16, 16, 1),
			/* PHY_CLK_EXT: MII_PHYCLK pad function:
			 * 0: PHY clock is provided by device (output)
			 * 1: PHY clock is external (input) */
			STM_PAD_SYS_CFG(7, 19, 19, -1),
			/* PHY_INTF_SEL */
			STM_PAD_SYS_CFG(7, 24, 26, 0),
			/* ENMII:
			 * 0: Reverse MII mode
			 * 1: MII mode */
			STM_PAD_SYS_CFG(7, 27, 27, 0),
		},
	},
};

static void stx5206_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx5206_ethernet_platform_data = {
	.pbl = 32,
	.has_gmac = 1,
	.enh_desc = 1,
	.fix_mac_speed = stx5206_ethernet_fix_mac_speed,
	/* .pad_config set in stx5206_configure_ethernet() */
};

static struct platform_device stx5206_ethernet_device = {
	.name = "stmmaceth",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd110000, 0x8000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", evt2irq(0x12a0), -1),
	},
	.dev.platform_data = &stx5206_ethernet_platform_data,
};

void __init stx5206_configure_ethernet(struct stx5206_ethernet_config *config)
{
	static int configured;
	struct stx5206_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	BUG_ON(configured);
	configured = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx5206_ethernet_pad_configs[config->mode];

	pad_config->sysconfs[1].value = (config->ext_clk ? 1 : 0);

	stx5206_ethernet_platform_data.pad_config = pad_config;
	stx5206_ethernet_platform_data.bus_id = config->phy_bus;

	/* MAC_SPEED_SEL */
	stx5206_ethernet_platform_data.bsp_priv =
			sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* FIXME this is a dirty hack to Set the PHY CLK to 25MHz (MII)
	 * in case it is not well provided by the target pack. */
	iowrite32(0x11, 0xfe213a34);

	platform_device_register(&stx5206_ethernet_device);
}



/* USB resources ---------------------------------------------------------- */

static u64 stx5206_usb_dma_mask = DMA_BIT_MASK(32);

static struct stm_plat_usb_data stx5206_usb_platform_data = {
	.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
		STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128,
	.pad_config = &(struct stm_pad_config) {
		.sysconfs_num = 3,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* USB_HOST_SOFT_RESET:
			 * active low usb host soft reset */
			STM_PAD_SYS_CFG(4, 1, 1, 1),
			/* suspend_from_config:
			 * Signal to suspend USB PHY */
			STM_PAD_SYS_CFG(10, 5, 5, 0),
			/* usb_power_down_req:
			 * power down request for USB Host module */
			STM_PAD_SYS_CFG(32, 4, 4, 0),
		},
	},
};

static struct platform_device stx5206_usb_device = {
	.name = "stm-usb",
	.id = -1,
	.dev = {
		.dma_mask = &stx5206_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &stx5206_usb_platform_data,
	},
	.num_resources = 6,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfe1ffe00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ehci", evt2irq(0x1720), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfe1ffc00, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("ohci", evt2irq(0x1700), -1),
		STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfe100000, 0x100),
		STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfe1fff00, 0x100),
	},
};

void __init stx5206_configure_usb(void)
{
	static int configured;

	BUG_ON(configured++);

	platform_device_register(&stx5206_usb_device);
}
