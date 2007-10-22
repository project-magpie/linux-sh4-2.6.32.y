/*
 * stmmac main header file
 */
#define ETH_RESOURCE_NAME         "stmmaceth"
#define PHY_RESOURCE_NAME	"stmmacphy"
#define DRV_MODULE_VERSION     "1.0"
#include "mac_hw.h"

/* This structure is common for both receive and transmit DMA descriptors.
 * A descriptor should not be used for storing more than one frame. */
struct dma_desc_t {
	unsigned int des0;	/* Status */
	unsigned int des1;	/* Ctrl bits, Buffer 2 length, Buffer 1 length */
	unsigned int des2;	/* Buffer 1 */
	unsigned int des3;	/* Buffer 2 */
};
typedef struct dma_desc_t dma_desc;

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
	unsigned int ip_header_len;
	struct mii_bus *mii;
	struct net_device_stats stats;
	u32 msg_enable;
	spinlock_t lock;
	spinlock_t tx_lock;

	dma_desc *dma_tx;	/* virtual DMA TX addr */
	dma_addr_t dma_tx_phy;	/* bus DMA TX addr */
	unsigned int cur_tx, dirty_tx;	/* Producer/consumer ring indices */
	struct sk_buff **tx_skbuff;

	dma_desc *dma_rx;	/* virtual DMA RX addr */
	dma_addr_t dma_rx_phy;	/* bus DMA RX addr */
	int dma_buf_sz;
	unsigned int rx_buff;	/* it contains the last rx buf owned by
				   the DMA */
	int rx_csum;
	unsigned int cur_rx, dirty_rx;	/* Producer/consumer ring indices */
	/* The addresses of receive-in-place skbuffs. */
	struct sk_buff **rx_skbuff;
	dma_addr_t *rx_skbuff_dma;

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	struct vlan_group *vlgrp;
#endif
	struct device *device;
	struct stmmmac_driver *mac;
	int pause_time;
	unsigned int dma_tx_size;
	unsigned int dma_rx_size;
};
