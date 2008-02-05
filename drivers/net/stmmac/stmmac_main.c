/* ============================================================================
 *
 * drivers/net/stmmac/stmmac_main.c
 *
 * This is the driver for the MAC 10/100/1000 on-chip Ethernet controllers.
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * Copyright (C) 2007 by STMicroelectronics
 *
 * ----------------------------------------------------------------------------
 *
 * Changelog:
 * Jan 2008:
 *	- First GMAC working.
 *	- Reviewed stmmac_poll in order to make easier the NAPI v5 porting.
 *	- Reviewed the xmit method in order to support large frames.
 *	- Removed self locking in the xmit (NETIF_F_LLTX).
 *	- Reviwed the software interrupt mitigation (as experimental code).
 *	- Reviewed supend and resume functions.
 * Dec 2007:
 *	- Reviewed the xmit method.
 *	- Fixed transmit errors detection.
 *	- Fixed "dma_[tx/rx]_size_param" module parameters.
 *	- Removed "dma_buffer_size" as module parameter.
 *	- Reviewed ethtool support and added extra statistics.
 * Oct 2007:
 *	- The driver completely merges the new GMAC code and the previous 
 *	  stmmac Ethernet driver (tested on the 7109/7200 STM platforms).
 * ===========================================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/stm/soc.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include "stmmac.h"

#undef STMMAC_DEBUG
//#define STMMAC_DEBUG
#ifdef STMMAC_DEBUG
#define DBG(nlevel, klevel, fmt, args...) \
		(void)(netif_msg_##nlevel(lp) && \
		printk(KERN_##klevel fmt, ## args))
#else
#define DBG(nlevel, klevel, fmt, args...)  do { } while(0)
#endif

#undef STMMAC_RX_DEBUG
//#define STMMAC_RX_DEBUG
#ifdef STMMAC_RX_DEBUG
#define RX_DBG(fmt,args...)  printk(fmt, ## args)
#else
#define RX_DBG(fmt, args...)  do { } while(0)
#endif

#undef STMMAC_XMIT_DEBUG
//#define STMMAC_XMIT_DEBUG

#define MIN_MTU 46
#define MAX_MTU ETH_DATA_LEN

#define DMA_BUFFER_SIZE	2048

#define STMMAC_ALIGN(x)	ALIGN((x), dma_get_cache_alignment())
#define STMMAC_IP_ALIGN NET_IP_ALIGN

/* Module Arguments */
#define TX_TIMEO (5*HZ)
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit Timeout (in milliseconds)");

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, S_IRUGO);
MODULE_PARM_DESC(debug, "Message Level (0: no output, 16: all)");

static int rx_copybreak = 0;
module_param(rx_copybreak, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rx_copybreak, "Copy only tiny-frames");

static int phy_n = -1;
module_param(phy_n, int, S_IRUGO);
MODULE_PARM_DESC(phy_n, "Physical device address");

#define DMA_TX_SIZE 64
static int dma_tx_size_param = DMA_TX_SIZE;
module_param(dma_tx_size_param, int, S_IRUGO);
MODULE_PARM_DESC(dma_tx_size_param, "Number of descriptors in the TX list");

#define DMA_RX_SIZE 128
static int dma_rx_size_param = DMA_RX_SIZE;
module_param(dma_rx_size_param, int, S_IRUGO);
MODULE_PARM_DESC(dma_rx_size_param, "Number of descriptors in the RX list");

static int flow_ctrl = FLOW_OFF;
module_param(flow_ctrl, int, S_IRUGO);
MODULE_PARM_DESC(flow_ctrl, "Flow control ability [on/off]");

static int pause = PAUSE_TIME;
module_param(pause, int, S_IRUGO);
MODULE_PARM_DESC(pause, "Flow Control Pause Time");

static int tx_aggregation = -1; /* No mitigtion by default */
module_param(tx_aggregation, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rx_copybreak, "Tx aggregation threshold");

#define TX_BUFFS_AVAIL(lp) \
	(lp->dirty_tx + lp->dma_tx_size - lp->cur_tx - 1)

static const char version[] = "STMMAC - (C) STMicroelectronics\n";

static const u32 default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_LINK | NETIF_MSG_IFUP |
				      NETIF_MSG_IFDOWN | NETIF_MSG_TIMER);

extern int stmmac_mdio_unregister(struct net_device *ndev);
extern int stmmac_mdio_register(struct net_device *ndev);
extern struct ethtool_ops stmmac_ethtool_ops;
static irqreturn_t stmmac_interrupt(int irq, void *dev_id);
/* STb7109 embedded MAC / GMAC device setup */
extern struct device_info_t *gmac_setup(unsigned long addr);
extern struct device_info_t *mac100_setup(unsigned long addr);

static __inline__ int validate_buffer_size(unsigned int size)
{
	unsigned int tbs = size;

	/* According to the TBS1/2 RBS1/2 bits the maximum 
		 * buffer size is 0x7ff */
	if (unlikely(tbs >= DMA_BUFFER_SIZE))
		tbs = 0x7ff;

	return tbs;

}

static int tdes1_buf1_size(unsigned int len)
{
	return ((validate_buffer_size(len) << DES1_RBS1_SIZE_SHIFT) 
		& DES1_RBS1_SIZE_MASK);
}

static int tdes1_buf2_size(unsigned int len)
{
	return ((validate_buffer_size(len) << DES1_RBS2_SIZE_SHIFT) 
		& DES1_RBS2_SIZE_MASK);
}

static __inline__ void stmmac_verify_args(void)
{
	/* Wrong parameters are forced with the default values */
	if (watchdog < 0)
		watchdog = TX_TIMEO;
	if (rx_copybreak < 0)
		rx_copybreak = ETH_FRAME_LEN;
	if (dma_rx_size_param < 0)
		dma_rx_size_param = DMA_RX_SIZE;
	if (dma_tx_size_param < 0)
		dma_tx_size_param = DMA_TX_SIZE;
	if (flow_ctrl > 1) {
		flow_ctrl = FLOW_AUTO;
	if (tx_aggregation >= (dma_tx_size_param))
		tx_aggregation = -1;
	} else if (flow_ctrl < 0) {
		flow_ctrl = FLOW_OFF;
	}
	if ((pause < 0) || (pause > 0xffff))
		pause = PAUSE_TIME;
	return;
}

#ifdef STMMAC_DEBUG
static __inline__ void print_pkt(unsigned char *buf, int len)
{
	int j;
	printk("len = %d byte, buf addr: 0x%p", len, buf);
	for (j = 0; j < len; j++) {
		if ((j % 16) == 0)
			printk("\n %03x:", j);
		printk(" %02x", buf[j]);
	}
	printk("\n");
	return;
}
#endif

/**
 * stmmac_adjust_link
 * @dev: net device structure
 * Description: it adjusts the link parameters.
 */
static void stmmac_adjust_link(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	struct phy_device *phydev = lp->phydev;
	unsigned long ioaddr = dev->base_addr;
	unsigned long flags;
	int new_state = 0;
	unsigned int fc = lp->flow_ctrl, pause_time = lp->pause;

	DBG(probe, DEBUG, "stmmac_adjust_link: called.  address %d link %d\n",
	    phydev->addr, phydev->link);

	spin_lock_irqsave(&lp->lock, flags);
	if (phydev->link) {
		unsigned int ctrl =
		    (unsigned int)readl(ioaddr + MAC_CTRL_REG);

		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (phydev->duplex != lp->oldduplex) {
			new_state = 1;
			if (!(phydev->duplex)) {
				ctrl &= ~lp->mac->hw.link.duplex;
			} else {
				ctrl |= lp->mac->hw.link.duplex;
			}
			lp->oldduplex = phydev->duplex;
		}
		/* Flow Control operation */
		if (phydev->pause)
			lp->mac->ops->flow_ctrl(ioaddr, phydev->duplex,
						fc, pause_time);

		if (phydev->speed != lp->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case 1000:
				if (likely(lp->is_gmac))
					ctrl &= lp->mac->hw.link.port;/* GMII */
			case 100:
			case 10:
				if (lp->is_gmac) {
					ctrl |= lp->mac->hw.link.port;/* MII */
					if (phydev->speed == SPEED_100) {
						ctrl |= lp->mac->hw.link.speed;
					} else {
						ctrl &= ~(lp->mac->hw.link.speed);
					}
				} else {
					ctrl &= ~lp->mac->hw.link.port;/* MII */
#if 0
					lp->fix_mac_speed(lp->bsp_priv, 
						phydev->speed); /*RMII*/
#endif
				}
				break;
			default:
				if (netif_msg_link(lp))
					printk(KERN_WARNING
					       "%s: Ack!  Speed (%d) is not 10 or 100!\n",
					       dev->name, phydev->speed);
				break;
			}

			lp->speed = phydev->speed;
		}

		writel(ctrl, ioaddr + MAC_CTRL_REG);

		if (!lp->oldlink) {
			new_state = 1;
			lp->oldlink = 1;
			netif_schedule(dev);
		}
	} else if (lp->oldlink) {
		new_state = 1;
		lp->oldlink = 0;
		lp->speed = 0;
		lp->oldduplex = -1;
	}

	if (new_state && netif_msg_link(lp))
		phy_print_status(phydev);

	spin_unlock_irqrestore(&lp->lock, flags);

	DBG(probe, DEBUG, "stmmac_adjust_link: exiting\n");
}

/**
 * stmmac_init_phy - PHY initialization
 * @dev: net device structure
 * Description: it initializes driver's PHY state, and attaches to the PHY.
 *  Return value:
 *  0 on success
 */
static int stmmac_init_phy(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	struct phy_device *phydev;
	char phy_id[BUS_ID_SIZE];

	lp->oldlink = 0;
	lp->speed = 0;
	lp->oldduplex = -1;

	snprintf(phy_id, BUS_ID_SIZE, PHY_ID_FMT, lp->bus_id, lp->phy_addr);
	DBG(probe, DEBUG, "stmmac_init_phy:  trying to attach to %s\n", phy_id);

	phydev =
	    phy_connect(dev, phy_id, &stmmac_adjust_link, 0, lp->phy_interface);

	if (IS_ERR(phydev)) {
		printk(KERN_ERR "%s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	DBG(probe, DEBUG,
	    "stmmac_init_phy:  %s: attached to PHY. Link = %d\n",
	    dev->name, phydev->link);

	lp->phydev = phydev;

	return 0;
}

/**
 * set_mac_addr
 * @ioaddr: device I/O address
 * @Addr: new MAC address
 * @high: High register offset
 * @low: low register offset
 * Description: the function sets the hardware MAC address
 */
static void set_mac_addr(unsigned long ioaddr, u8 Addr[6],
			 unsigned int high, unsigned int low)
{
	unsigned long data;

	data = (Addr[5] << 8) | Addr[4];
	writel(data, ioaddr + high);
	data = (Addr[3] << 24) | (Addr[2] << 16) | (Addr[1] << 8) | Addr[0];
	writel(data, ioaddr + low);

	return;
}

/**
 * get_mac_addr
 * @ioaddr: device I/O address
 * @addr: mac address
 * @high: High register offset
 * @low: low register offset
 * Description: the function gets the hardware MAC address
 */
static void get_mac_address(unsigned long ioaddr, unsigned char *addr,
			    unsigned int high, unsigned int low)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = (unsigned int)readl(ioaddr + high);
	lo_addr = (unsigned int)readl(ioaddr + low);

	/* Extract the MAC address from the high and low words */
	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;

	return;
}

/**
 * stmmac_mac_enable_rx
 * @dev: net device structure
 * Description: the function enables the RX MAC process
 */
static void stmmac_mac_enable_rx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CTRL_REG);

	/* set the RE (receive enable, bit 2) */
	value |= MAC_RNABLE_RX;
	writel(value, ioaddr + MAC_CTRL_REG);
	return;
}

/**
 * stmmac_mac_enable_rx
 * @dev: net device structure
 * Description: the function enables the TX MAC process
 */
static void stmmac_mac_enable_tx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CTRL_REG);

	/* set: TE (transmitter enable, bit 3) */
	value |= MAC_ENABLE_TX;
	writel(value, ioaddr + MAC_CTRL_REG);
	return;
}

/**
 * stmmac_mac_disable_rx
 * @dev: net device structure
 * Description: the function disables the RX MAC process
 */
static void stmmac_mac_disable_rx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CTRL_REG);

	value &= ~MAC_RNABLE_RX;
	writel(value, ioaddr + MAC_CTRL_REG);
	return;
}

/**
 * stmmac_mac_disable_tx
 * @dev: net device structure
 * Description: the function disables the TX MAC process
 */
static void stmmac_mac_disable_tx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CTRL_REG);

	value &= ~MAC_ENABLE_TX;
	writel(value, ioaddr + MAC_CTRL_REG);
	return;
}

static void display_dma_desc_ring(dma_desc * p, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		printk("\t%d [0x%x]: "
			"desc0=0x%x desc1=0x%x buffer1=0x%x, buffer2=0x%x", i,
			(unsigned int)virt_to_phys(&p[i].des0), p[i].des0,
			p[i].des1, (unsigned int)p[i].des2,
			(unsigned int)p[i].des3);
		printk("\n");
	}
}

/*
 * This function clears both RX and TX descriptors (MAC 10/100).
 * Note that the driver uses the 'implicit' scheme for implementing
 * the TX/RX DMA linked lists. So the second buffer doesn't point
 * to the next descriptor.  */
static void reset_mac_descs(dma_desc * p, unsigned int ring_size,
			    unsigned int own_bit)
{
	int i;
	for (i = 0; i < ring_size; i++) {
		p->des0 = own_bit;
		if (!(own_bit)) {
			p->des1 = 0;
		} else {
			p->des1 = tdes1_buf1_size(DMA_BUFFER_SIZE);
			/*p->des1 |= RDES1_CONTROL_DIC;*/
		}
		if (i == ring_size - 1) {
			p->des1 |= MAC_CTRL_DESC_TER;
		}
		p++;
	}
	return;
}

/* This function clears both RX and TX GMAC descriptors.*/
static void reset_gmac_descs(dma_desc * p, unsigned int ring_size,
			    unsigned int own_bit)
{
	int i;
	for (i = 0; i < ring_size; i++) {
		p->des0 = own_bit;
		if (!(own_bit)) { // TX 
			p->des1 = 0;
			if (i == ring_size - 1)
				p->des0 |= GMAC_TX_CONTROL_TER;
		} else { // RX
			p->des1 = tdes1_buf1_size(DMA_BUFFER_SIZE);
			if (i == ring_size - 1)
				p->des1 |= GMAC_RX_CONTROL_TER;
		}
		p++;
	}
	return;
}

/**
 * init_dma_desc_rings - init the RX/TX descriptor rings
 * @dev: net device structure
 * Description:  this function initializes the DMA RX/TX descriptors
 */
static void init_dma_desc_rings(struct net_device *dev)
{
	int i;
	struct eth_driver_local *lp = netdev_priv(dev);
	struct sk_buff *skb;
	unsigned int txsize = lp->dma_tx_size;
	unsigned int rxsize = lp->dma_rx_size;
	int bfsize = lp->dma_buf_sz;

	DBG(probe, INFO, "%s: allocate and init the DMA RX/TX\n"
	    "(txsize %d, rxsize %d, bfsize %d)\n",
	    ETH_RESOURCE_NAME, txsize, rxsize, bfsize);

	lp->rx_skbuff_dma =
	    (dma_addr_t *) kmalloc(rxsize * sizeof(dma_addr_t), GFP_KERNEL);
	lp->rx_skbuff =
	    (struct sk_buff **)kmalloc(sizeof(struct sk_buff *) * rxsize,
				       GFP_KERNEL);
	lp->dma_rx = (dma_desc *) dma_alloc_coherent(lp->device,
						     rxsize *
						     sizeof(struct dma_desc_t),
						     &lp->dma_rx_phy,
						     GFP_KERNEL);
	lp->tx_skbuff =
	    (struct sk_buff **)kmalloc(sizeof(struct sk_buff *) * txsize,
				       GFP_KERNEL);
	lp->dma_tx = (dma_desc *) dma_alloc_coherent(lp->device,
						     txsize *
						     sizeof(struct dma_desc_t),
						     &lp->dma_tx_phy,
						     GFP_KERNEL);

	if ((lp->dma_rx == NULL) || (lp->dma_tx == NULL)) {
		printk(KERN_ERR "%s:ERROR allocating the DMA Tx/Rx desc\n",
		       __FUNCTION__);
		return;
	}

	DBG(probe, DEBUG, "%s: DMA desc rings: virt addr (Rx 0x%08x, "
	    "Tx 0x%08x) DMA phy addr (Rx 0x%08x,Tx 0x%08x)\n",
	    dev->name, (unsigned int)lp->dma_rx, (unsigned int)lp->dma_tx,
	    (unsigned int)lp->dma_rx_phy, (unsigned int)lp->dma_tx_phy);

	/* ---- RX INITIALIZATION */
	DBG(probe, DEBUG, "[RX skb data]   [DMA RX skb data] "
	    "(buff size: %d)\n", bfsize);

	for (i = 0; i < rxsize; i++) {
		dma_desc *p = lp->dma_rx + i;

		skb = netdev_alloc_skb(dev, bfsize);
		if (unlikely(skb == NULL)) {
			printk(KERN_ERR "%s: Rx init fails; skb is NULL\n",
			       __FUNCTION__);
			break;
		}
		skb_reserve(skb, STMMAC_IP_ALIGN);

		lp->rx_skbuff[i] = skb;
		lp->rx_skbuff_dma[i] = dma_map_single(lp->device, skb->data,
						      bfsize, DMA_FROM_DEVICE);
		p->des2 = lp->rx_skbuff_dma[i];
		DBG(probe, DEBUG, "[0x%08x]\t[0x%08x]\n",
		    (unsigned int)lp->rx_skbuff[i],
		    (unsigned int)lp->rx_skbuff[i]->data);
	}
	lp->cur_rx = 0;
	lp->dirty_rx = (unsigned int)(i - rxsize);

	/* ---- TX INITIALIZATION */
	for (i = 0; i < txsize; i++) {
		lp->tx_skbuff[i] = NULL;
		lp->dma_tx[i].des2 = 0;
		lp->dma_tx[i].des3 = 0;
	}
	lp->dirty_tx = lp->cur_tx = 0;

	/* Clear the Rx/Tx descriptors */
	if (lp->is_gmac) {
		reset_gmac_descs(lp->dma_rx, rxsize, OWN_BIT);
		reset_gmac_descs(lp->dma_tx, txsize, 0);
	} else {
		reset_mac_descs(lp->dma_rx, rxsize, OWN_BIT);
		reset_mac_descs(lp->dma_tx, txsize, 0);
	}

	if (netif_msg_hw(lp)) {
		printk("RX descriptor ring:\n");
		display_dma_desc_ring(lp->dma_rx, rxsize);
		printk("TX descriptor ring:\n");
		display_dma_desc_ring(lp->dma_tx, txsize);
	}
	return;
}

/**
 * dma_free_rx_skbufs
 * @dev: net device structure
 * Description:  this function frees all the skbuffs in the Rx queue
 */
static void dma_free_rx_skbufs(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < lp->dma_rx_size; i++) {
		if (lp->rx_skbuff[i]) {
			dma_unmap_single(lp->device, lp->rx_skbuff_dma[i],
					 lp->dma_buf_sz, DMA_FROM_DEVICE);
			dev_kfree_skb(lp->rx_skbuff[i]);
		}
		lp->rx_skbuff[i] = NULL;
	}
	return;
}

/**
 * dma_free_tx_skbufs
 * @dev: net device structure
 * Description:  this function frees all the skbuffs in the Tx queue
 */
static void dma_free_tx_skbufs(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < lp->dma_tx_size; i++) {
		if (lp->tx_skbuff[i] != NULL) {
			if ((lp->dma_tx + i)->des2) {
				dma_unmap_single(lp->device, p->des2,
						 ((p->
						   des1 & DES1_RBS1_SIZE_MASK)
						  >> DES1_RBS1_SIZE_SHIFT),
						 DMA_TO_DEVICE);
			}
			if ((lp->dma_tx + i)->des3) {
				dma_unmap_single(lp->device, p->des3,
						 ((p->
						   des1 & DES1_RBS1_SIZE_MASK)
						  >> DES1_RBS1_SIZE_SHIFT),
						 DMA_TO_DEVICE);
			}
			dev_kfree_skb_any(lp->tx_skbuff[i]);
			lp->tx_skbuff[i] = NULL;
		}
	}
	return;
}

/**
 * free_dma_desc_resources
 * @dev: net device structure
 * Description:  this function releases and free ALL the DMA resources
 */
static void free_dma_desc_resources(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Release the DMA TX/RX socket buffers */
	dma_free_rx_skbufs(dev);
	dma_free_tx_skbufs(dev);

	/* Free the region of consistent memory previously allocated for 
	 * the DMA */
	dma_free_coherent(lp->device,
			  lp->dma_tx_size * sizeof(struct dma_desc_t),
			  lp->dma_tx, lp->dma_tx_phy);
	dma_free_coherent(lp->device,
			  lp->dma_rx_size * sizeof(struct dma_desc_t),
			  lp->dma_rx, lp->dma_rx_phy);
	kfree(lp->rx_skbuff_dma);
	kfree(lp->rx_skbuff);
	kfree(lp->tx_skbuff);

	return;
}

/**
 * stmmac_dma_reset - STMAC DMA SW reset
 * @ioaddr: device I/O address
 * Description:  this function performs the DMA SW reset.
 *  NOTE1: the MII_TxClk and the MII_RxClk must be active before this
 *	   SW reset otherwise the MAC core won't exit the reset state.
 *  NOTE2: after a SW reset all interrupts are disabled
 */
static void stmmac_dma_reset(unsigned long ioaddr)
{
	unsigned int value;

	value = (unsigned int)readl(ioaddr + DMA_BUS_MODE);
	value |= DMA_BUS_MODE_SFT_RESET;
	writel(value, ioaddr + DMA_BUS_MODE);
	while ((readl(ioaddr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET)) {
	}
	return;
}

/**
 * stmmac_dma_start_tx
 * @ioaddr: device I/O address
 * Description:  this function starts the DMA tx process
 */
static void stmmac_dma_start_tx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CONTROL);
	return;
}

static void stmmac_dma_stop_tx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CONTROL);
	return;
}

/**
 * stmmac_dma_start_rx
 * @ioaddr: device I/O address
 * Description:  this function starts the DMA rx process
 */
static void stmmac_dma_start_rx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value |= DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CONTROL);

	return;
}

static void stmmac_dma_stop_rx(unsigned long ioaddr)
{
	unsigned int value;

	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CONTROL);

	return;
}

static __inline__ void stmmac_dma_enable_irq_rx(unsigned long ioaddr)
{
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_INTR_ENA);
	return;
}

static __inline__ void stmmac_dma_disable_irq_rx(unsigned long ioaddr)
{
	writel(DMA_INTR_NO_RX, ioaddr + DMA_INTR_ENA);
	return;
}

/**
 * stmmac_dma_init - DMA init function
 * @dev: net device structure
 * Description: the DMA init function performs:
 * - the DMA RX/TX SW descriptors initialization
 * - the DMA HW controller initialization
 * NOTE: the DMA TX/RX processes will be started in the 'open' method.
 */
static int stmmac_dma_init(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);

	DBG(probe, DEBUG, "STMMAC: DMA Core setup\n");

	/* DMA SW reset */
	stmmac_dma_reset(ioaddr);

	/* Enable Application Access by writing to DMA CSR0 */
	DBG(probe, DEBUG, "\t(PBL: %d)\n", lp->pbl);
	writel(DMA_BUS_MODE_DEFAULT | ((lp->pbl) << DMA_BUS_MODE_PBL_SHIFT),
	       ioaddr + DMA_BUS_MODE);

	/* Mask interrupts by writing to CSR7 */
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_INTR_ENA);

	/* The base address of the RX/TX descriptor lists must be written into
	 * DMA CSR3 and CSR4, respectively. */
	writel((unsigned long)lp->dma_tx_phy, ioaddr + DMA_TX_BASE_ADDR);
	writel((unsigned long)lp->dma_rx_phy, ioaddr + DMA_RCV_BASE_ADDR);

	return 0;
}

#ifdef STMMAC_DEBUG
/**
 * show_tx_process_state
 * @status: tx descriptor status field
 * Description: it shows the Transmit Process State for CSR5[22:20]
 */
static void show_tx_process_state(unsigned int status)
{
	unsigned int state;
	state = (status & DMA_STATUS_TS_MASK) >> DMA_STATUS_TS_SHIFT;

	switch (state) {
	case 0:
		printk("- TX (Stopped): Reset or Stop command\n");
		break;
	case 1:
		printk("- TX (Running):Fetching the Tx desc\n");
		break;
	case 2:
		printk("- TX (Running): Waiting for end of tx\n");
		break;
	case 3:
		printk("- TX (Running): Reading the data "
		       "and queuing the data into the Tx buf\n");
		break;
	case 6:
		printk("- TX (Suspended): Tx Buff Underflow "
		       "or an unavailable Transmit descriptor\n");
		break;
	case 7:
		printk("- TX (Running): Closing Tx descriptor\n");
		break;
	default:
		break;
	}
	return;
}

/**
 * show_rx_process_state
 * @status: rx descriptor status field
 * Description: it shows the  Receive Process State for CSR5[19:17]
 */
static void show_rx_process_state(unsigned int status)
{
	unsigned int state;
	state = (status & DMA_STATUS_RS_MASK) >> DMA_STATUS_RS_SHIFT;

	switch (state) {
	case 0:
		printk("- RX (Stopped): Reset or Stop command\n");
		break;
	case 1:
		printk("- RX (Running): Fetching the Rx desc\n");
		break;
	case 2:
		printk("- RX (Running):Checking for end of pkt\n");
		break;
	case 3:
		printk("- RX (Running): Waiting for Rx pkt\n");
		break;
	case 4:
		printk("- RX (Suspended): Unavailable Rx buf\n");
		break;
	case 5:
		printk("- RX (Running): Closing Rx descriptor\n");
		break;
	case 6:
		printk("- RX(Running): Flushing the current frame"
		       " from the Rx buf\n");
		break;
	case 7:
		printk("- RX (Running): Queuing the Rx frame"
		       " from the Rx buf into memory\n");
		break;
	default:
		break;
	}
	return;
}
#endif

/**
 * stmmac_tx:
 * @dev: net device structure
 * Description: reclaim resources after transmit completes.
 */
static __inline__ void stmmac_tx(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int txsize = lp->dma_tx_size;
	int entry = lp->dirty_tx % txsize, is_gmac = lp->is_gmac;

	while (lp->dirty_tx != lp->cur_tx) {
		dma_desc *p = lp->dma_tx + entry;
		int txstatus = p->des0, last;

		if (txstatus & OWN_BIT)
			break;

		if (is_gmac) /* GMAC*/
			last = (p->des0 & GMAC_TX_LAST_SEGMENT);
		else
			last = (p->des1 & TDES1_CONTROL_LS);

		if (likely(last)) {
			int tx_error = lp->mac->ops->tx_err(&lp->stats,
							    &lp->xstats,
							    txstatus);
			if (likely(tx_error == 0)) {
				lp->stats.tx_packets++;
			} else {
				lp->stats.tx_errors++;
				DBG(intr, ERR,
				    "Tx Error (%d):des0 0x%x, des1 0x%x,"
				    "[buf: 0x%08x] [buf: 0x%08x]\n",
				    entry, p->des0, p->des1, p->des2, p->des3);
			}
		}
		DBG(intr, DEBUG, "stmmac_tx: curr %d, dirty %d\n", lp->cur_tx,
		    lp->dirty_tx);

		/* clear descriptors */
		if (is_gmac) { /* GMAC*/
			p->des0 &= GMAC_TX_CONTROL_TER;
			p->des1 = 0;
		} else {
			p->des0 = 0;
			p->des1 &= MAC_CTRL_DESC_TER;
		}
		if (likely(p->des2)) {
			dma_unmap_single(lp->device, p->des2,
					 (p->des1 & DES1_RBS1_SIZE_MASK) >>
					 DES1_RBS1_SIZE_SHIFT, DMA_TO_DEVICE);
			p->des2 = 0;
		}
		if (likely(p->des3)) {
			dma_unmap_single(lp->device, p->des3,
					 (p->des1 & DES1_RBS2_SIZE_MASK) >>
					 DES1_RBS2_SIZE_SHIFT, DMA_TO_DEVICE);
			p->des3 = 0;
		}
		if (likely(lp->tx_skbuff[entry] != NULL)) {
			dev_kfree_skb_irq(lp->tx_skbuff[entry]);
			lp->tx_skbuff[entry] = NULL;
		}
		entry = (++lp->dirty_tx) % txsize;
	}
	spin_lock(&lp->tx_lock);
	if (unlikely(netif_queue_stopped(dev) &&
	    TX_BUFFS_AVAIL(lp) > (MAX_SKB_FRAGS + 1)))
		netif_wake_queue(dev);

	spin_unlock(&lp->tx_lock);
	return;
}

/**
 * stmmac_restart_tx:
 * @dev: net device structure
 * Description: although this should be a rare event, will try
 * to bump up the tx threshold in the DMA ctrl register (only 
 * for the TLI) and restart transmission again.
 */
static __inline__ void stmmac_restart_tx(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int txsize = lp->dma_tx_size;
	unsigned int ioaddr = dev->base_addr;
	int entry = (lp->dirty_tx % txsize), curr = lp->cur_tx % txsize;
	dma_desc *p = lp->dma_tx + entry;
	dma_desc *n = lp->dma_tx + curr;

	/* Bump up the threshold */
	if (lp->ttc <= 0x100) {
		lp->ttc += 0x20;
		lp->xstats.tx_threshold = lp->ttc;
		lp->mac->ops->dma_ttc(ioaddr, lp->ttc);
	}

	spin_lock(&lp->tx_lock);

	/* Keep the frame and try retransmitting it again */
	while (!(p->des0 & 0x2)) {
		entry++;
		entry %= txsize;
		p = lp->dma_tx + entry;
	}

	DBG(intr, INFO,
	    "stmmac: Tx Underflow: (%d) des0 0x%x, des1 0x%x"
	    " [buf: 0x%08x] (current=%d, dirty=%d)\n",
	    entry, p->des0, p->des1, readl(ioaddr + DMA_CUR_TX_BUF_ADDR),
	    curr, (lp->dirty_tx % txsize));

	/* Place the frame in the next free position
	 * and clean the descriptor where the underflow happened */
	if (!lp->is_gmac){
		n->des0 = OWN_BIT;
		n->des1 |= (p->des1 & ~MAC_CTRL_DESC_TER);
		n->des1 |= TDES1_CONTROL_IC;
	} else { /*GMAC -- written but (fortunately) never met and test! */
		n->des0 |= (p->des0 & GMAC_TX_CONTROL_TER);
		n->des0 |= (OWN_BIT | GMAC_TX_IC);
		n->des1 = p->des1;
	}
	n->des2 = p->des2;
	p->des0 = 0;
	p->des2 = 0;
	lp->cur_tx++;

	writel(1, ioaddr + DMA_XMT_POLL_DEMAND);
	/* stop the queue as well */
	netif_stop_queue(dev);
	spin_unlock(&lp->tx_lock);
	return;
}

/**
 * stmmac_tx_err: 
 * @dev: net device structure
 * Description: clean descriptors and restart the transmission.
 */
static __inline__ void stmmac_tx_err(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	stmmac_dma_stop_tx(dev->base_addr);
	if (lp->is_gmac) {
		reset_gmac_descs(lp->dma_rx, lp->dma_rx_size, OWN_BIT);
		reset_gmac_descs(lp->dma_tx, lp->dma_tx_size, 0);
	} else {
		reset_mac_descs(lp->dma_rx, lp->dma_rx_size, OWN_BIT);
		reset_mac_descs(lp->dma_tx, lp->dma_tx_size, 0);
	}
	lp->stats.tx_errors++;
	stmmac_dma_start_tx(dev->base_addr);
	return;
}

/**
 * stmmac_dma_interrupt - Interrupt handler for the STMMAC DMA
 * @dev: net device structure
 * Description: it determines if we have to call either the Rx or the Tx
 * interrupt handler.
 */
static void stmmac_dma_interrupt(struct net_device *dev)
{
	unsigned int intr_status;
	unsigned int ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);

	/* read the status register (CSR5) */
	intr_status = (unsigned int)readl(ioaddr + DMA_STATUS);

	DBG(intr, INFO, "%s: [CSR5: 0x%08x]\n", __FUNCTION__, intr_status);

#ifdef STMMAC_DEBUG
	/* It displays the DMA transmit process state (CSR5 register) */
	if (netif_msg_tx_done(lp))
		show_tx_process_state(intr_status);
	if (netif_msg_rx_status(lp))
		show_rx_process_state(intr_status);
#endif
	/* Clear the interrupt by writing a logic 1 to the CSR5[15-0] */
	writel((intr_status & 0x1ffff), ioaddr + DMA_STATUS);

	/* ABNORMAL interrupts */
	if (unlikely(intr_status & DMA_STATUS_AIS)) {
		DBG(intr, INFO, "CSR5[15] DMA ABNORMAL IRQ: ");
		if (unlikely(intr_status & DMA_STATUS_UNF)) {
			stmmac_restart_tx(dev);
			lp->xstats.tx_undeflow_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_TJT))
			lp->xstats.tx_jabber_irq++;
		if (unlikely(intr_status & DMA_STATUS_OVF))
			lp->xstats.rx_overflow_irq++;
		if (unlikely(intr_status & DMA_STATUS_RU))
			lp->xstats.rx_buf_unav_irq++;
		if (unlikely(intr_status & DMA_STATUS_RPS))
			lp->xstats.rx_process_stopped_irq++;
		if (unlikely(intr_status & DMA_STATUS_RWT))
			lp->xstats.rx_watchdog_irq++;
		if (unlikely(intr_status & DMA_STATUS_ETI)) {
			lp->xstats.tx_early_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_TPS)) {
			lp->xstats.tx_process_stopped_irq++;
			stmmac_tx_err(dev);
		}
		if (unlikely(intr_status & DMA_STATUS_FBI)) {
			lp->xstats.fatal_bus_error_irq++;
			stmmac_tx_err(dev);
		}
	}

	/* NORMAL interrupts */
	if (likely(intr_status & DMA_STATUS_NIS)) {
		DBG(intr, INFO, " CSR5[16]: DMA NORMAL IRQ: ");
		if (likely(intr_status & (DMA_STATUS_RI | DMA_STATUS_ERI))) {

			RX_DBG("Receive irq [buf: 0x%08x]\n",
			       readl(ioaddr + DMA_CUR_RX_BUF_ADDR));
			lp->xstats.rx_irq_n++;
			stmmac_dma_disable_irq_rx(ioaddr);
			if (likely(netif_rx_schedule_prep(dev))) {
				__netif_rx_schedule(dev);
			}
		}
		if (likely(intr_status & (DMA_STATUS_TI))) {
			DBG(intr, INFO, " Transmit irq [buf: 0x%08x]\n",
			    readl(ioaddr + DMA_CUR_TX_BUF_ADDR));
			lp->xstats.tx_irq_n++;
			stmmac_tx(dev);
		}
	}
#if 0
	if ((lp->wolenabled == PMT_SUPPORTED) && 
		(intr_status & DMA_STATUS_PMT)){
		readl(ioaddr + MAC_PMT);
	}
#endif
	DBG(intr, INFO, "\n\n");

	return;
}

/**
 *  stmmac_enable - MAC/DMA initialization
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function inits both the DMA and the MAC core and starts the Rx/Tx
 *  processes.
 *  It also copies the MAC addr into the HW (in case we have set it with nwhw).
 */
static int stmmac_enable(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	int ret;

	/* Request the IRQ lines */
	ret = request_irq(dev->irq, &stmmac_interrupt,
			  IRQF_SHARED, dev->name, dev);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: ERROR: allocating the IRQ %d (error: %d)\n",
		       __FUNCTION__, dev->irq, ret);
		return ret;
	}

	/* Create and initialize the TX/RX descriptors rings */
	init_dma_desc_rings(dev);

	/* Note: the DMA initialization (and SW reset) 
	 * must be after we have successfully initialised the PHY
	 * (see comment in stmmac_dma_reset). */
	if (stmmac_dma_init(dev) < 0) {
		printk(KERN_ERR "%s: DMA initialization failed\n",
		       __FUNCTION__);
		return -1;
	}

	/* Copy the MAC addr into the HW (in case we have set it with nwhw) */
	set_mac_addr(ioaddr, dev->dev_addr, lp->mac->hw.addr_high,
		     lp->mac->hw.addr_low);

	/* Initialize the MAC Core */
	lp->mac->ops->core_init(ioaddr);
	lp->tx_aggregation = 0;

	/* Enable the MAC Rx/Tx */
	stmmac_mac_enable_rx(dev);
	stmmac_mac_enable_tx(dev);

	/* Extra statistics */
	memset(&lp->xstats, 0, sizeof(struct stmmac_extra_stats));
	lp->xstats.tx_threshold = lp->ttc = 0x20;	/* 32 DWORDS */
	lp->mac->ops->dma_ttc(ioaddr, lp->ttc);

	/* Start the ball rolling... */
	DBG(probe, DEBUG, "%s: DMA RX/TX processes started...\n",
	    ETH_RESOURCE_NAME);
	stmmac_dma_start_rx(ioaddr);
	stmmac_dma_start_tx(ioaddr);

	/* Dump registers */
	if (netif_msg_hw(lp)) {
		lp->mac->ops->mac_registers((unsigned int)ioaddr);
		lp->mac->ops->dma_registers(ioaddr);
	}
	return 0;
}

/**
 *  stmmac_open - open entry point of the driver
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function is the open entry point of the driver.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */
static int stmmac_open(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int ret;

	/* Check that the MAC address is valid.  If its not, refuse
	 * to bring the device up. The user must specify an
	 * address using the following linux command:
	 *      ifconfig eth0 hw ether xx:xx:xx:xx:xx:xx  */
	if (!is_valid_ether_addr(dev->dev_addr)) {
		DBG(probe, ERR, "%s: no valid eth hw addr\n", __FUNCTION__);
		return -EINVAL;
	}

	/* Attach the PHY */
	ret = stmmac_init_phy(dev);
	if (ret) {
		printk(KERN_ERR "%s: Cannot attach to PHY (error: %d)\n",
		       __FUNCTION__, ret);
		return -ENODEV;
	}

	/* Enable MAC/DMA, call irq_request and allocate the rings */
	stmmac_enable(dev);

	phy_start(lp->phydev);

	netif_start_queue(dev);
	return 0;
}

static int stmmac_shutdown(struct net_device *dev)
{
	netif_stop_queue(dev);
	/* Free the IRQ lines */
	free_irq(dev->irq, dev);

	/* Stop TX/RX DMA and clear the descriptors */
	stmmac_dma_stop_tx(dev->base_addr);
	stmmac_dma_stop_rx(dev->base_addr);

	/* Release and free the Rx/Tx resources */
	free_dma_desc_resources(dev);

	/* Disable the MAC core */
	stmmac_mac_disable_tx(dev);
	stmmac_mac_disable_rx(dev);

	return 0;
}

/**
 *  stmmac_release - close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */
static int stmmac_release(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Stop and disconnect the PHY */
	phy_stop(lp->phydev);
	phy_disconnect(lp->phydev);
	lp->phydev = NULL;

	stmmac_shutdown(dev);

	netif_carrier_off(dev);

	return 0;
}

/**
 *  stmmac_xmit:
 *  @skb : the socket buffer
 *  @dev : device pointer
 *  Description : Tx entry point of the driver.
 */
static int stmmac_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned long flags;
	unsigned int txsize = lp->dma_tx_size,
	    nfrags = skb_shinfo(skb)->nr_frags,
	    entry = lp->cur_tx % txsize, i, nopaged_len, first = entry;
	dma_desc *p = lp->dma_tx;
	int is_gmac;

	/* This is a hard error log it. */
	if (unlikely(TX_BUFFS_AVAIL(lp) < nfrags + 1)) {
		netif_stop_queue(dev);
		printk(KERN_ERR "%s: BUG! Tx Ring full when queue awake!\n",
		       dev->name);
		return NETDEV_TX_BUSY;
	}

#ifdef STMMAC_XMIT_DEBUG
	if (nfrags>0)
		printk("stmmac xmit: len: %d, nopaged_len: %d n_frags: %d\n", 
			skb->len, nopaged_len, nfrags);
#endif
	spin_lock_irqsave(&lp->tx_lock, flags);

	if (unlikely((lp->tx_skbuff[entry] != NULL))) {
		/* This should never happen! */
		printk(KERN_ERR "%s: BUG! Inconsistent Tx skb utilization!\n",
		       dev->name);
		dev_kfree_skb(skb);
		return -1;
	}
	lp->tx_skbuff[entry] = skb;

	/* Verify the checksum */
	lp->mac->ops->tx_checksum(skb);

	/* Get the amount of non-paged data (skb->data). */
	nopaged_len = skb_headlen(skb);

	is_gmac = lp->is_gmac;

	/* Handle non-paged data (skb->data) */
	if (!is_gmac) {
		p[entry].des1 = (p[entry].des1 & MAC_CTRL_DESC_TER);
		p[entry].des1 |= (TDES1_CONTROL_FS | tdes1_buf1_size(nopaged_len));
	} else { /* GMAC*/
		p[entry].des0 &= (p[entry].des0 & GMAC_TX_CONTROL_TER);
		p[entry].des0 |= GMAC_TX_FIRST_SEGMENT;
		p[entry].des1 = nopaged_len; // size buf 1 (FIXME)
	}
	/* Fill buffer 1 */
	p[entry].des2 = dma_map_single(lp->device, skb->data,
					STMMAC_ALIGN(nopaged_len), 
					DMA_TO_DEVICE);
	/* Handle paged fragments */
	for (i = 0; i < nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		int len = frag->size;

		lp->cur_tx++;
		entry = lp->cur_tx % txsize;

#ifdef STMMAC_XMIT_DEBUG
		if (nfrags>0)
			printk("\t[entry %d] segment len: %d\n", entry, len);
#endif
		p[entry].des0 = OWN_BIT;
		if (!is_gmac) {
			p[entry].des1 = (p[entry].des1 & MAC_CTRL_DESC_TER);
			p[entry].des1 |= tdes1_buf1_size(len);
		} else {
			p[entry].des0 &= (p[entry].des0 & GMAC_TX_CONTROL_TER);
			p[entry].des1 =  tdes1_buf1_size(len);
		}
		p[entry].des2 = dma_map_page(lp->device, frag->page,
					     frag->page_offset, 
					     lp->dma_buf_sz,
					     DMA_TO_DEVICE);

		if (len > DMA_BUFFER_SIZE) {	/* Fill the second buffer */
			int b2size = len - DMA_BUFFER_SIZE;
			p[entry].des1 |= tdes1_buf2_size(b2size);
			p[entry].des3 = dma_map_page(lp->device, 
					     frag->page,
					     frag->page_offset+DMA_BUFFER_SIZE+1, 
					     lp->dma_buf_sz, DMA_TO_DEVICE);
		}
		lp->tx_skbuff[entry] = NULL;
	}

	/* If there are more than one fragment, we set the interrupt
	 * on completition field in the latest fragment (where we also set 
	 * the LS bit. */
	if (!is_gmac)
		p[entry].des1 |= TDES1_CONTROL_LS | TDES1_CONTROL_IC;
	else
		p[entry].des0 |= GMAC_TX_LAST_SEGMENT | GMAC_TX_IC;

	p[first].des0 |= OWN_BIT;	/* to avoid race condition */

	lp->cur_tx++;

#ifdef STMMAC_XMIT_DEBUG
	if (netif_msg_pktdata(lp)) {
		printk("stmmac xmit: current=%d, dirty=%d, entry=%d, "
		       "first=%d, nfrags=%d\n",
		       (lp->cur_tx % txsize), (lp->dirty_tx % txsize), entry,
		       first, nfrags);
		display_dma_desc_ring(lp->dma_tx, txsize);
		printk(">>> frame to be transmitted: ");
		print_pkt(skb->data, skb->len);
	}
#endif

	if (TX_BUFFS_AVAIL(lp) <= (MAX_SKB_FRAGS + 1)){
		netif_stop_queue(dev);
	} else {
		/* Allow aggregation of Tx interrupts and clear 
		   TDES1[31] */
		if (lp->tx_aggregation <= tx_aggregation) {
			lp->tx_aggregation++;
			p[entry].des1 &= ~TDES1_CONTROL_IC;
		} else {
			lp->tx_aggregation = 0;
		}
	}

	lp->stats.tx_bytes += skb->len;
	lp->xstats.tx_bytes += skb->len;

	/* CSR1 enables the transmit DMA to check for new descriptor */
	writel(1, dev->base_addr + DMA_XMT_POLL_DEMAND);

	dev->trans_start = jiffies;
	spin_unlock_irqrestore(&lp->tx_lock, flags);

	return NETDEV_TX_OK;
}

static __inline__ void stmmac_rx_refill(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int rxsize = lp->dma_rx_size;
	int bfsize = lp->dma_buf_sz;
	dma_desc *drx = lp->dma_rx;

	for (; lp->cur_rx - lp->dirty_rx > 0; lp->dirty_rx++) {
		int entry = lp->dirty_rx % rxsize;
		if (lp->rx_skbuff[entry] == NULL) {
			struct sk_buff *skb = netdev_alloc_skb(dev, bfsize);
			if (unlikely(skb == NULL)) {
				printk(KERN_ERR "%s: skb is NULL\n",
					__FUNCTION__);
				break;
			}
			skb_reserve(skb, STMMAC_IP_ALIGN);
			lp->rx_skbuff[entry] = skb;
			lp->rx_skbuff_dma[entry] = dma_map_single(lp->device,
							  skb->data, bfsize,
							  DMA_FROM_DEVICE);
			(drx + entry)->des2 = lp->rx_skbuff_dma[entry];
			RX_DBG("\trefill entry #%d\n", entry);
		}
		(drx + entry)->des0 = OWN_BIT;
	}
	return;
}

static int stmmac_rx(struct net_device *dev, int limit)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int rxsize = lp->dma_rx_size;
	int entry = lp->cur_rx % rxsize, count;

#ifdef STMMAC_RX_DEBUG
	printk(">>> stmmac_rx: descriptor ring:\n");
	display_dma_desc_ring(lp->dma_rx, rxsize);
#endif
	lp->xstats.rx_poll_n++;

	for (count = 0; count < limit; ++count) {
		dma_desc *drx = lp->dma_rx + entry;
		unsigned int status = drx->des0;

		if (status & OWN_BIT)
			break;

		if (unlikely (((lp->mac->ops->rx_err(&lp->stats, 
			&lp->xstats, status) < 0)) ||
				(!(status & RDES0_STATUS_LS)))) {

			lp->stats.rx_errors++;

			if (unlikely(!(status & RDES0_STATUS_LS))) {
				printk(KERN_WARNING "%s: Oversized Ethernet "
				       "frame spanned multiple buffers, entry "
				       "%#x status %8.8x!\n", dev->name, entry,
				       status);
				lp->stats.rx_length_errors++;
			}
		} else {
			struct sk_buff *skb;
			/* Length should omit the CRC */
			int frame_len = (((status & RDES0_STATUS_FL_MASK) >>
					  RDES0_STATUS_FL_SHIFT) - 4);

			RX_DBG
			    ("\tquota %d, desc: 0x%0x [entry %d] buff=0x%x\n",
			     limit, (unsigned int)drx, entry,
			     drx->des2);

			/* Check if the packet is long enough to accept without
			   copying to a minimally-sized skbuff. */
			if (unlikely(frame_len < rx_copybreak) &&
			    (skb = netdev_alloc_skb(dev,
					    STMMAC_ALIGN(frame_len + 2))) != NULL) {

				skb_reserve(skb, STMMAC_IP_ALIGN);
				dma_sync_single_for_cpu(lp->device,
							lp->rx_skbuff_dma[entry],
							frame_len,
							DMA_FROM_DEVICE);
				skb_copy_to_linear_data(skb, lp->rx_skbuff[entry]->data, 
							frame_len);

				skb_put(skb, frame_len);
				dma_sync_single_for_device(lp->device,
							   lp->rx_skbuff_dma[entry],
							   frame_len,
							   DMA_FROM_DEVICE);
			} else {	/* zero-copy */
				skb = lp->rx_skbuff[entry];
				if (unlikely(!skb)) {
					printk(KERN_ERR "%s: Inconsistent Rx "
					       "descriptor chain.\n",
					       dev->name);
					lp->stats.rx_dropped++;
					lp->xstats.rx_dropped++;
					break;
				}
				lp->rx_skbuff[entry] = NULL;
				skb_put(skb, frame_len);
				dma_unmap_single(lp->device,
						 lp->rx_skbuff_dma[entry],
						 lp->dma_buf_sz,
						 DMA_FROM_DEVICE);
			}
#ifdef STMMAC_RX_DEBUG
			if (netif_msg_pktdata(lp)) {
				printk(KERN_DEBUG " - frame received: ");
				print_pkt(skb->data, frame_len);
			}
#endif
			skb->protocol = eth_type_trans(skb, dev);
			lp->mac->ops->rx_checksum(skb, status);

			netif_receive_skb(skb);

			lp->stats.rx_packets++;
			lp->stats.rx_bytes += frame_len;
			lp->xstats.rx_bytes += frame_len;
			dev->last_rx = jiffies;
		}
		entry = (++lp->cur_rx) % rxsize;
		drx = lp->dma_rx + entry;
	}

	stmmac_rx_refill(dev);

	return count;
}

/**
 *  stmmac_poll - stmmac poll method (NAPI)
 *  @dev : pointer to the netdev structure.
 *  @budget : maximum number of packets that the current CPU can receive from
 *	      all interfaces.
 *  Description :
 *   This function implements the the reception process.
 *   It is based on NAPI which provides a "inherent mitigation" in order
 *   to improve network performance.
 */
static int stmmac_poll(struct net_device *dev, int *budget)
{
	int work_done;

	work_done = stmmac_rx(dev, dev->quota);
	dev->quota -= work_done;
	*budget -= work_done;

	if (work_done < *budget) {
		RX_DBG(">>> rx work completed.\n");
		__netif_rx_complete(dev);
		stmmac_dma_enable_irq_rx(dev->base_addr);
		return 0;
	}
	return 1;
}

/**
 *  stmmac_tx_timeout
 *  @dev : Pointer to net device structure
 *  Description: this function is called when a packet transmission fails to
 *   complete within a reasonable period. The driver will mark the error in the
 *   netdev structure and arrange for the device to be reset to a sane state
 *   in order to transmit a new packet.
 */
static void stmmac_tx_timeout(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	printk(KERN_WARNING "%s: Tx timeout at %ld, latency %ld\n",
	       dev->name, jiffies, (jiffies - dev->trans_start));

#ifdef STMMAC_DEBUG
	printk("(current=%d, dirty=%d)\n", (lp->cur_tx % lp->dma_tx_size),
	       (lp->dirty_tx % lp->dma_tx_size));
	printk("DMA tx ring status: \n");
	display_dma_desc_ring(lp->dma_tx, lp->dma_tx_size);
#endif
	netif_stop_queue(dev);

	spin_lock(&lp->tx_lock);
	tx_aggregation = -1;
	lp->tx_aggregation = 0;
	/* Clear Tx resources */
	stmmac_tx_err(dev);
	dev->trans_start = jiffies;
	netif_wake_queue(dev);

	spin_unlock(&lp->tx_lock);

	return;
}

/**
 *  stmmac_stats
 *  @dev : Pointer to net device structure
 *  Description: this function returns statistics to the caller application
 */
struct net_device_stats *stmmac_stats(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	return &lp->stats;
}

/* Configuration changes (passed on by ifconfig) */
static int stmmac_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP)	/* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk(KERN_WARNING "%s: can't change I/O address\n",
		       dev->name);
		return -EOPNOTSUPP;
	}

	/* Don't allow changing the IRQ */
	if (map->irq != dev->irq) {
		printk(KERN_WARNING "%s: can't change IRQ number %d\n",
		       dev->name, dev->irq);
		return -EOPNOTSUPP;
	}

	/* ignore other fields */
	return 0;
}

/**
 *  stmmac_multicast_list - entry point for multicast addressing
 *  @dev : pointer to the device structure
 *  Description:
 *  This function is a driver entry point which gets called by the kernel
 *  whenever multicast addresses must be enabled/disabled.
 *  Return value:
 *  void.
 */
static void stmmac_multicast_list(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Calling the hw function. */
	lp->mac->ops->set_filter(dev);

	return;
}

/**
 *  stmmac_change_mtu - entry point to change MTU size for the device.
 *   @dev : device pointer.
 *   @new_mtu : the new MTU size for the device.
 *   Description: the Maximum Transfer Unit (MTU) is used by the network layer
 *     to drive packet transmission. Ethernet has an MTU of 1500 octets 
 *     (ETH_DATA_LEN). This value can be changed with ifconfig.
 *  Return value:
 *   0 on success and an appropriate (-)ve integer as defined in errno.h
 *   file on failure.
 */
static int stmmac_change_mtu(struct net_device *dev, int new_mtu)
{
	if (netif_running(dev)) {
		printk(KERN_ERR "%s: must be stopped to change its MTU\n",
		       dev->name);
		return -EBUSY;
	}

	if ((new_mtu < MIN_MTU) || (new_mtu > MAX_MTU))
		return -EINVAL;

	dev->mtu = new_mtu;

	return 0;
}

static irqreturn_t stmmac_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;

	if (unlikely(!dev)) {
		printk(KERN_ERR "%s: invalid dev pointer\n", __FUNCTION__);
		return IRQ_NONE;
	}

	stmmac_dma_interrupt(dev);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/* Polling receive - used by NETCONSOLE and other diagnostic tools
 * to allow network I/O with interrupts disabled. */
static void stmmac_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	stmmac_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

/**
 *  stmmac_ioctl - Entry point for the Ioctl
 *  @dev :  Device pointer.
 *  @rq :  An IOCTL specefic structure, that can contain a pointer to
 *  a proprietary structure used to pass information to the driver.
 *  @cmd :  IOCTL command
 *  Description:
 *  Currently there are no special functionality supported in IOCTL, just the
 *  phy_mii_ioctl(...) can be invoked.
 */
static int stmmac_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int ret = -EOPNOTSUPP;

	if (!netif_running(dev))
		return -EINVAL;

	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		if (!lp->phydev)
			return -EINVAL;

		spin_lock(&lp->lock);
		ret = phy_mii_ioctl(lp->phydev, if_mii(rq), cmd);
		spin_unlock(&lp->lock);
	default:
		break;
	}
	return ret;
}

#ifdef STMMAC_VLAN_TAG_USED
static void stmmac_vlan_rx_register(struct net_device *dev,
				    struct vlan_group *grp)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	/* VLAN Tag identifier register already contains the VLAN tag ID. 
	   (see hw mac initialization). */
	lp->vlgrp = grp;
	spin_unlock(&lp->lock);
}
#endif

/**
 *  stmmac_probe - Initialization of the adapter .
 *  @dev : device pointer
 *  Description: The function initializes the network device structure for
 *		the STMMAC driver. It also calls the low level routines 
 *		 in order to init the HW (i.e. the DMA engine)
 */
static int stmmac_probe(struct net_device *dev)
{
	int ret = 0;
	struct eth_driver_local *lp = netdev_priv(dev);

	stmmac_verify_args();

	ether_setup(dev);

	dev->open = stmmac_open;
	dev->stop = stmmac_release;
	dev->set_config = stmmac_config;

	dev->hard_start_xmit = stmmac_xmit;
	if (!lp->is_gmac) /*FIXME*/
		dev->features |= (NETIF_F_SG | NETIF_F_HW_CSUM | 
					NETIF_F_HIGHDMA);

	dev->get_stats = stmmac_stats;
	dev->tx_timeout = stmmac_tx_timeout;
	dev->watchdog_timeo = msecs_to_jiffies(watchdog);
	dev->set_multicast_list = stmmac_multicast_list;
	dev->change_mtu = stmmac_change_mtu;
	dev->ethtool_ops = &stmmac_ethtool_ops;
	dev->do_ioctl = &stmmac_ioctl;
#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = stmmac_poll_controller;
#endif
#ifdef STMMAC_VLAN_TAG_USED
	/* Supports IEEE 802.1Q VLAN tag detection for reception frames */
	dev->features |= NETIF_F_HW_VLAN_RX;
	dev->vlan_rx_register = stmmac_vlan_rx_register;
#endif

	lp->msg_enable = netif_msg_init(debug, default_msg_level);

	lp->rx_csum = 0;

	/* Just to keep aligned values. */
	lp->dma_tx_size = STMMAC_ALIGN(dma_tx_size_param);
	lp->dma_rx_size = STMMAC_ALIGN(dma_rx_size_param);
	lp->dma_buf_sz = STMMAC_ALIGN(DMA_BUFFER_SIZE);

	if (flow_ctrl)
		lp->flow_ctrl = FLOW_AUTO;	/* RX/TX pause on */

	lp->pause = pause;

	dev->poll = stmmac_poll;
	dev->weight = lp->dma_rx_size;

	/* Get the MAC address */
	get_mac_address(dev->base_addr, dev->dev_addr,
			lp->mac->hw.addr_high, lp->mac->hw.addr_low);

	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk(KERN_WARNING "\tno valid MAC address; "
		       "please, set using ifconfig or nwhwconfig!\n");
	}

	if ((ret = register_netdev(dev))) {
		printk(KERN_ERR "%s: ERROR %i registering the device\n",
		       __FUNCTION__, ret);
		return -ENODEV;
	}

	spin_lock_init(&lp->lock);
	spin_lock_init(&lp->tx_lock);

	return ret;
}

/**
 * stmmac_mac_device_setup
 * @dev : device pointer
 * Description: it detects and inits either 
 *  the mac 10/100 or the Gmac.
 */
static __inline__ void stmmac_mac_device_setup(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;

	struct device_info_t *device;

	if (lp->is_gmac)
		device = gmac_setup(ioaddr);
	else
		device = mac100_setup(ioaddr);
	lp->mac = device;
	lp->wolenabled = lp->mac->hw.pmt; // PMT supported

	return;
}

 /**
 * stmmacphy_dvr_probe
 * @pdev: platform device pointer
 * Description: The driver is initialized through the platform_device
 * 		structures which define the configuration needed by the SoC.
 *		These are defined in arch/sh/kernel/cpu/sh4
 */
static int stmmacphy_dvr_probe(struct platform_device *pdev)
{
	struct plat_stmmacphy_data *plat_dat;
	plat_dat = (struct plat_stmmacphy_data *)((pdev->dev).platform_data);

	printk(KERN_DEBUG "stmmacphy_dvr_probe: added phy for bus %d\n",
	       plat_dat->bus_id);

	return 0;
}

static int stmmacphy_dvr_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver stmmacphy_driver = {
	.driver = {
		   .name = PHY_RESOURCE_NAME,
		   },
	.probe = stmmacphy_dvr_probe,
	.remove = stmmacphy_dvr_remove,
};

/**
 * stmmac_associate_phy
 * @dev: pointer to device structure
 * @data: points to the private structure.
 * Description: Scans through all the PHYs we have registered and checks if
 *		any are associated with our MAC.  If so, then just fill in
 *		the blanks in our local context structure
 */
static int stmmac_associate_phy(struct device *dev, void *data)
{
	struct eth_driver_local *lp = (struct eth_driver_local *)data;
	struct plat_stmmacphy_data *plat_dat;

	plat_dat = (struct plat_stmmacphy_data *)(dev->platform_data);

	DBG(probe, DEBUG,
	    "stmmacphy_dvr_probe: checking phy for bus %d\n", plat_dat->bus_id);

	/* Check that this phy is for the MAC being initialised */
	if (lp->bus_id != plat_dat->bus_id)
		return 0;

	/* OK, this PHY is connected to the MAC.  Go ahead and get the parameters */
	DBG(probe, DEBUG, "stmmacphy_dvr_probe: OK. Found PHY config\n");
	lp->phy_irq =
	    platform_get_irq_byname(to_platform_device(dev), "phyirq");
	DBG(probe, DEBUG,
	    "stmmacphy_dvr_probe: PHY irq on bus %d is %d\n",
	    plat_dat->bus_id, lp->phy_irq);

	/* Override with kernel parameters if supplied XXX CRS XXX 
	 * this needs to have multiple instances */
	if ((phy_n >= 0) && (phy_n <= 31)) {
		plat_dat->phy_addr = phy_n;
	}

	lp->phy_addr = plat_dat->phy_addr;
	lp->phy_mask = plat_dat->phy_mask;
	lp->phy_interface = plat_dat->interface;
	lp->phy_reset = plat_dat->phy_reset;

	DBG(probe, DEBUG, "stmmacphy_dvr_probe: exiting\n");
	return 1;		/* forces exit of driver_for_each_device() */
}

/**
 * stmmac_dvr_probe
 * @pdev: platform device pointer
 * Description: The driver is initialized through platform_device.  
 * 		Structures which define the configuration needed by the board 
 *		are defined in a board structure in arch/sh/boards/st/ .
 */
static int stmmac_dvr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	unsigned int *addr = NULL;
	struct net_device *ndev = NULL;
	struct eth_driver_local *lp;
	struct plat_stmmacenet_data *plat_dat;

	printk(KERN_INFO "STMMAC driver:\n\tplatform registration... ");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto out;
	}
	printk(KERN_INFO "done!\n");

	if (!request_mem_region(res->start, (res->end - res->start),
				ETH_RESOURCE_NAME)) {
		printk(KERN_ERR "%s: ERROR: memory allocation failed"
		       "cannot get the I/O addr 0x%x\n",
		       __FUNCTION__, (unsigned int)res->start);
		ret = -EBUSY;
		goto out;
	}

	addr = ioremap(res->start, (res->end - res->start));
	if (!addr) {
		printk(KERN_ERR "%s: ERROR: memory mapping failed \n",
		       __FUNCTION__);
		ret = -ENOMEM;
		goto out;
	}

	ndev = alloc_etherdev(sizeof(struct eth_driver_local));
	if (!ndev) {
		printk(KERN_ERR "%s: ERROR: allocating the device\n",
		       __FUNCTION__);
		ret = -ENOMEM;
		goto out;
	}

	/* Get the MAC information */
	if ((ndev->irq = platform_get_irq_byname(pdev, "macirq")) == 0) {
		printk(KERN_ERR "%s: ERROR: MAC IRQ configuration "
		       "information not found\n", __FUNCTION__);
		ret = -ENODEV;
		goto out;
	}

	lp = netdev_priv(ndev);
	lp->device = &(pdev->dev);
	lp->dev = ndev;
	plat_dat = (struct plat_stmmacenet_data *)((pdev->dev).platform_data);
	lp->bus_id = plat_dat->bus_id;
	lp->pbl = plat_dat->pbl;	/* TLI */
	lp->is_gmac = plat_dat->has_gmac;	/* GMAC is on board */

	platform_set_drvdata(pdev, ndev);

	/* Set the I/O base addr */
	ndev->base_addr = (unsigned long)addr;

	/* MAC HW revice detection */
	stmmac_mac_device_setup(ndev);

	/* Network Device Registration */
	ret = stmmac_probe(ndev);
	if (ret < 0) {
		goto out;
	}

	/* associate a PHY - it is provided by another platform bus */
	if (!driver_for_each_device
	    (&(stmmacphy_driver.driver), NULL, (void *)lp,
	     stmmac_associate_phy)) {
		printk(KERN_ERR "No PHY device is associated with this MAC!\n");
		ret = -ENODEV;
		goto out;
	}

	lp->fix_mac_speed = plat_dat->fix_mac_speed;
	lp->bsp_priv = plat_dat->bsp_priv;

	/* MDIO bus Registration */
	printk(KERN_DEBUG "registering MDIO bus...\n");
	ret = stmmac_mdio_register(ndev);
	printk(KERN_DEBUG "registering MDIO bus done\n");
	ndev = __dev_get_by_name("eth0");

      out:
	if (ret < 0) {
		platform_set_drvdata(pdev, NULL);
		release_mem_region(res->start, (res->end - res->start));
		if (addr != NULL)
			iounmap(addr);
	}

	return ret;
}

/**
 * stmmac_dvr_remove
 * @pdev: platform device pointer
 * Description: This function resets the TX/RX processes, disables the MAC RX/TX
 *   changes the link status, releases the DMA descriptor rings,
 *   unregisters the MDIO busm and unmaps allocated memory.
 */
static int stmmac_dvr_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct resource *res;

	printk(KERN_INFO "%s:\n\tremoving driver", __FUNCTION__);

	stmmac_dma_stop_rx(ndev->base_addr);
	stmmac_dma_stop_tx(ndev->base_addr);

	stmmac_mac_disable_rx(ndev);
	stmmac_mac_disable_tx(ndev);

	netif_carrier_off(ndev);

	stmmac_mdio_unregister(ndev);

	platform_set_drvdata(pdev, NULL);
	unregister_netdev(ndev);

	iounmap((void *)ndev->base_addr);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, (res->end - res->start));

	free_netdev(ndev);

	return 0;
}

#ifdef CONFIG_PM
static void stmmac_powerdown(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Stop TX DMA */
	stmmac_dma_stop_tx(dev->base_addr);
	/* Disable MAC transmitter and receiver */
	stmmac_mac_disable_tx(dev);
	stmmac_mac_disable_rx(dev);

	/* Sanity state for the rings */
	if (lp->is_gmac) {
		reset_gmac_descs(lp->dma_rx, lp->dma_rx_size, OWN_BIT);
		reset_gmac_descs(lp->dma_tx, lp->dma_tx_size, 0);
	} else {
		reset_mac_descs(lp->dma_rx, lp->dma_rx_size, OWN_BIT);
		reset_mac_descs(lp->dma_tx, lp->dma_tx_size, 0);
	}

	/* Enable Power down mode by programming the PMT regs */
	if (lp->wolenabled == PMT_SUPPORTED)
		lp->mac->ops->enable_wol(dev->base_addr, lp->wolopts);

	/* Enable receiver */
	stmmac_mac_enable_rx(dev);

	return;
}
 
static int stmmac_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);

	if (!dev || !netif_running(dev))
		return 0;

	if (state.event==PM_EVENT_SUSPEND && device_may_wakeup(&(pdev->dev))){
		stmmac_powerdown(dev);
		return 0;
	}

	netif_device_detach(dev);
	netif_stop_queue(dev);
	stmmac_shutdown(dev);

	spin_unlock(&lp->lock);
	return 0;
}

static int stmmac_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);

	spin_lock(&lp->lock);
	if (!netif_running(dev))
		return 0;

	netif_device_attach(dev);

	stmmac_enable(dev);

	netif_start_queue(dev);
	spin_unlock(&lp->lock);
	return 0;
}
#endif

static struct platform_driver stmmac_driver = {
	.driver = {
		   .name = ETH_RESOURCE_NAME,
		   },
	.probe = stmmac_dvr_probe,
	.remove = stmmac_dvr_remove,
#ifdef CONFIG_PM
	.suspend = stmmac_suspend,
	.resume = stmmac_resume,
#endif

};

/**
 * stmmac_init_module - Entry point for the driver
 * Description: This function is the entry point for the driver.
 */
static int __init stmmac_init_module(void)
{
	if (platform_driver_register(&stmmacphy_driver)) {
		printk(KERN_ERR "No PHY devices registered!\n");
		return -ENODEV;
	}

	return platform_driver_register(&stmmac_driver);
}

/**
 * stmmac_cleanup_module - Cleanup routine for the driver
 * Description: This function is the cleanup routine for the driver.
 */
static void __exit stmmac_cleanup_module(void)
{
	platform_driver_unregister(&stmmac_driver);
}

static int __init stmmac_cmdline_opt(char *str)
{
	char *opt;

	if (!str || !*str)
		return -EINVAL;

	while ((opt = strsep(&str, ",")) != NULL) {
		if (!strncmp(opt, "msglvl:", 7)) {
			debug = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "phyaddr:", 8)) {
			phy_n = simple_strtoul(opt + 8, NULL, 0);
		} else if (!strncmp(opt, "watchdog:", 9)) {
			watchdog = simple_strtoul(opt + 9, NULL, 0);
		} else if (!strncmp(opt, "minrx:", 6)) {
			rx_copybreak = simple_strtoul(opt + 6, NULL, 0);
		} else if (!strncmp(opt, "txsize:", 7)) {
			dma_tx_size_param = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "rxsize:", 7)) {
			dma_rx_size_param = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "flow_ctrl:", 10)) {
			flow_ctrl = simple_strtoul(opt + 10, NULL, 0);
		} else if (!strncmp(opt, "pause:", 6)) {
			pause = simple_strtoul(opt + 6, NULL, 0);
		} else if (!strncmp(opt, "txopt:", 6)) {
			tx_aggregation = simple_strtoul(opt + 6, NULL, 0);
		}
	}
	return 0;
}

__setup("stmmaceth=", stmmac_cmdline_opt);

module_init(stmmac_init_module);
module_exit(stmmac_cleanup_module);

MODULE_DESCRIPTION("STMMAC 10/100/1000 Ethernet driver");
MODULE_AUTHOR("Giuseppe Cavallaro");
MODULE_LICENSE("GPL");
