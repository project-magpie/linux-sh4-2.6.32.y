#define ETH_RESOURCE_NAME	"stmmaceth"
#define PHY_RESOURCE_NAME	"stmmacphy"
#define DRV_MODULE_VERSION	"April_08"

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define STMMAC_VLAN_TAG_USED
#endif

#include "common.h"

/* Struct private for the STMMAC driver */
struct eth_driver_local {
	int bus_id;
	int phy_addr;
	int phy_irq;
	int phy_mask;
	phy_interface_t phy_interface;
	int (*phy_reset) (void *priv);
	void (*fix_mac_speed) (void *priv, unsigned int speed);
	void *bsp_priv;
	int oldlink;
	int speed;
	int oldduplex;
	struct phy_device *phydev;
	int pbl;
	int is_gmac;
	unsigned int ip_header_len;
	struct mii_bus *mii;
	u32 msg_enable;
	spinlock_t lock;
	spinlock_t tx_lock;
	dma_desc *dma_tx;
	dma_desc *dma_rx;
	dma_addr_t dma_tx_phy;
	unsigned int cur_tx, dirty_tx;
	struct sk_buff **tx_skbuff;
	dma_addr_t dma_rx_phy;
	int dma_buf_sz;
	unsigned int rx_buff;
	int rx_csum;
	unsigned int cur_rx, dirty_rx;
	struct sk_buff **rx_skbuff;
	dma_addr_t *rx_skbuff_dma;
	struct device *device;
	unsigned int dma_tx_size;
	unsigned int dma_rx_size;
	struct device_info_t *mac_type;
	unsigned int flow_ctrl;
	unsigned int pause;
#ifdef STMMAC_VLAN_TAG_USED
	struct vlan_group *vlgrp;
#endif
	struct net_device *dev;
	struct stmmac_extra_stats xstats;
	int wolopts;
	int wolenabled;
	int tx_aggregation;
	int has_timer;
	struct timer_list timer;
};
