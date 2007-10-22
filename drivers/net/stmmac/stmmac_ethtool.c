/*
 * drivers/net/stmmac/stmmac_ethtool.c
 *
 * STMMAC Ethernet Driver
 * Ethtool support for STMMAC Ethernet Driver
 *
 * Author: Giuseppe Cavallaro
 *
 * Copyright (c) 2006-2007 STMicroelectronics
 *
 */
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <asm/io.h>

#include "stmmac.h"

#define REGDUMP_LEN         (32 * 1024)

void stmmac_ethtool_getdrvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, ETH_RESOURCE_NAME);
	strcpy(info->version, DRV_MODULE_VERSION);
	info->fw_version[0] = '\0';
	return;
}

int stmmac_ethtool_getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	struct phy_device *phy = lp->phydev;
	int rc;
	if (phy == NULL) {
		printk(KERN_ERR "%s: %s: PHY is not registered\n",
		       __FUNCTION__, dev->name);
		return -ENODEV;
	}

	if (!netif_running(dev)) {
		printk(KERN_ERR "%s: interface is disabled: we cannot track "
		       "link speed / duplex setting\n", dev->name);
		return -EBUSY;
	}

	cmd->transceiver = XCVR_INTERNAL;
	spin_lock_irq(&lp->lock);
	rc = phy_ethtool_gset(phy, cmd);
	spin_unlock_irq(&lp->lock);
	return rc;
}

int stmmac_ethtool_setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct eth_driver_local *lp = dev->priv;
	struct phy_device *phy = lp->phydev;
	int rc;

	spin_lock_irq(&lp->lock);
	rc = phy_ethtool_sset(phy, cmd);
	spin_unlock_irq(&lp->lock);

	return rc;
}

u32 stmmac_ethtool_getmsglevel(struct net_device * dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	return lp->msg_enable;
}

void stmmac_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	lp->msg_enable = level;

}

int stmmac_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;
	return (0);
}

int stmmac_ethtool_get_regs_len(struct net_device *dev)
{
	return (REGDUMP_LEN);
}

void stmmac_ethtool_gregs(struct net_device *dev,
			  struct ethtool_regs *regs, void *space)
{
	int i;
	u32 reg;
	u32 *reg_space = (u32 *) space;

	memset(reg_space, 0x0, REGDUMP_LEN);

	/* MAC registers */
	for (i = 0; i < 11; i++) {
		reg = readl(dev->base_addr + i * 4);
		memcpy((reg_space + i * 4), &reg, sizeof(u32));
	}

	/* DMA registers */
	for (i = 0; i < 9; i++) {
		reg = readl(dev->base_addr + (DMA_BUS_MODE + i * 4));
		memcpy((reg_space + (DMA_BUS_MODE + i * 4)), &reg, sizeof(u32));
	}
	reg = readl(dev->base_addr + DMA_CUR_TX_BUF_ADDR);
	memcpy((reg_space + DMA_CUR_TX_BUF_ADDR), &reg, sizeof(u32));
	reg = readl(dev->base_addr + DMA_CUR_RX_BUF_ADDR);
	memcpy((reg_space + DMA_CUR_RX_BUF_ADDR), &reg, sizeof(u32));

	return;
}

int stmmac_ethtool_set_tx_csum(struct net_device *dev, u32 data)
{
	if (data)
		dev->features |= NETIF_F_HW_CSUM;
	else
		dev->features &= ~NETIF_F_HW_CSUM;

	return 0;
}

u32 stmmac_ethtool_get_rx_csum(struct net_device * dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	return (lp->rx_csum);
}

int stmmac_ethtool_set_rx_csum(struct net_device *dev, u32 data)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	if (data)
		lp->rx_csum = 1;
	else
		lp->rx_csum = 0;

	return 0;
}

struct ethtool_ops stmmac_ethtool_ops = {
	.begin = stmmac_check_if_running,
	.get_drvinfo = stmmac_ethtool_getdrvinfo,
	.get_settings = stmmac_ethtool_getsettings,
	.set_settings = stmmac_ethtool_setsettings,
	.get_msglevel = stmmac_ethtool_getmsglevel,
	.set_msglevel = stmmac_ethtool_setmsglevel,
	.get_regs = stmmac_ethtool_gregs,
	.get_regs_len = stmmac_ethtool_get_regs_len,
	.get_link = ethtool_op_get_link,
	.get_rx_csum = stmmac_ethtool_get_rx_csum,
	.set_rx_csum = stmmac_ethtool_set_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = stmmac_ethtool_set_tx_csum,
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso = ethtool_op_get_tso,
	.set_tso = ethtool_op_set_tso,
#endif
	.get_ufo = ethtool_op_get_ufo,
	.set_ufo = ethtool_op_set_ufo,
};
