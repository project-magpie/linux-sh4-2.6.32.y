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

#define REG_SPACE_SIZE	0x1054

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

	spin_lock(&lp->lock);
	rc = phy_ethtool_sset(phy, cmd);
	spin_unlock(&lp->lock);

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
	return (REG_SPACE_SIZE);
}

void stmmac_ethtool_gregs(struct net_device *dev,
			  struct ethtool_regs *regs, void *space)
{
	int i;
	u32 *reg_space = (u32 *) space;

	memset(reg_space, 0x0, REG_SPACE_SIZE);
	/* MAC registers */
	for (i = 0; i < 12; i++) {
		reg_space[i] = readl(dev->base_addr + (i * 4));
	}
	/* DMA registers */
	for (i = 0; i < 9; i++) {
		reg_space[i + 12] =
		    readl(dev->base_addr + (DMA_BUS_MODE + (i * 4)));
	}
	reg_space[22] = readl(dev->base_addr + DMA_CUR_TX_BUF_ADDR);
	reg_space[23] = readl(dev->base_addr + DMA_CUR_RX_BUF_ADDR);

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

static void
stmmac_get_pauseparam(struct net_device *netdev,
		      struct ethtool_pauseparam *pause)
{
	struct eth_driver_local *lp = netdev_priv(netdev);

	spin_lock(&lp->lock);

	pause->rx_pause = pause->tx_pause = 0;
	pause->autoneg = lp->phydev->autoneg;

	if (lp->flow_ctrl & FLOW_RX)
		pause->rx_pause = 1;
	if (lp->flow_ctrl & FLOW_TX)
		pause->tx_pause = 1;

	spin_unlock(&lp->lock);
	return;
}

static int
stmmac_set_pauseparam(struct net_device *netdev,
		      struct ethtool_pauseparam *pause)
{
	struct eth_driver_local *lp = netdev_priv(netdev);
	struct phy_device *phy = lp->phydev;
	int new_pause = FLOW_OFF;
	int ret = 0;

	spin_lock(&lp->lock);

	if (pause->rx_pause)
		new_pause |= FLOW_RX;
	if (pause->tx_pause)
		new_pause |= FLOW_TX;

	lp->flow_ctrl = new_pause;

	if (phy->autoneg) {
		if (netif_running(netdev)) {
			struct ethtool_cmd cmd;
			/* auto-negotiation automatically restarted */
			cmd.cmd = ETHTOOL_NWAY_RST;
			cmd.supported = phy->supported;
			cmd.advertising = phy->advertising;
			cmd.autoneg = phy->autoneg;
			cmd.speed = phy->speed;
			cmd.duplex = phy->duplex;
			cmd.phy_address = phy->addr;
			ret = phy_ethtool_sset(phy, &cmd);
		}
	} else {
		unsigned long ioaddr = netdev->base_addr;
		lp->mac_type->ops->flow_ctrl(ioaddr, phy->duplex,
					     lp->flow_ctrl, lp->pause);
	}
	spin_unlock(&lp->lock);
	return ret;
}

static struct {
	const char str[ETH_GSTRING_LEN];
} ethtool_stats_keys[] = {
	{
	"tx_underflow"}, {
	"tx_carrier"}, {
	"tx_losscarrier"}, {
	"tx_heartbeat"}, {
	"tx_deferred"}, {
	"tx_vlan"}, {
	"tx_jabber"}, {
	"tx_frame_flushed"}, {
	"rx_desc"}, {
	"rx_partial"}, {
	"rx_runt"}, {
	"rx_toolong"}, {
	"rx_collision"}, {
	"rx_crc"}, {
	"rx_lenght"}, {
	"rx_mii"}, {
	"rx_multicast"}, {
	"rx_gmac_overflow"}, {
	"rx_watchdog"}, {
	"rx_filter"}, {
	"rx_dropped"}, {
	"rx_bytes"}, {
	"tx_bytes"}, {
	"tx_irq_n"}, {
	"rx_irq_n"}, {
	"tx_undeflow_irq"}, {
	"threshold"}, {
	"tx_process_stopped_irq"}, {
	"tx_jabber_irq"}, {
	"rx_overflow_irq"}, {
	"rx_buf_unav_irq"}, {
	"rx_process_stopped_irq"}, {
	"rx_watchdog_irq"}, {
	"tx_early_irq"}, {
	"fatal_bus_error_irq"}, {
	"rx_poll_n"}, {
	"tx_payload_error"}, {
	"tx_ip_header_error"}, {
	"rx_missed_cntr"}, {
	"rx_overflow_cntr"},};

static int stmmac_stats_count(struct net_device *dev)
{
	return EXTRA_STATS;
}

static void stmmac_ethtool_stats(struct net_device *dev,
				 struct ethtool_stats *dummy, u64 * buf)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	u32 *extra;
	int i;

	lp->mac_type->ops->dma_diagnostic_fr(&lp->stats, &lp->xstats, ioaddr);

	extra = (u32 *) & lp->xstats;

	for (i = 0; i < EXTRA_STATS; i++)
		buf[i] = extra[i];
	return;
}

static void stmmac_get_strings(struct net_device *dev, u32 stringset, u8 * buf)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(buf, &ethtool_stats_keys, sizeof(ethtool_stats_keys));
		break;
	default:
		WARN_ON(1);
		break;
	}
	return;
}

static void stmmac_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock_irq(&lp->lock);
	if (lp->wolenabled == PMT_SUPPORTED) {
		wol->supported = WAKE_MAGIC | WAKE_UCAST;
		wol->wolopts = lp->wolopts;
	}
	spin_unlock_irq(&lp->lock);
}

static int stmmac_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	u32 support = WAKE_MAGIC;

	if (lp->wolenabled == PMT_NOT_SUPPORTED)
		return -EINVAL;

	if (wol->wolopts & ~support)
		return -EINVAL;

	spin_lock_irq(&lp->lock);
	lp->wolopts = wol->wolopts;
	spin_unlock_irq(&lp->lock);

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
	.get_pauseparam = stmmac_get_pauseparam,
	.set_pauseparam = stmmac_set_pauseparam,
	.get_ethtool_stats = stmmac_ethtool_stats,
	.get_stats_count = stmmac_stats_count,
	.get_strings = stmmac_get_strings,
	.get_wol = stmmac_get_wol,
	.set_wol = stmmac_set_wol,
};
