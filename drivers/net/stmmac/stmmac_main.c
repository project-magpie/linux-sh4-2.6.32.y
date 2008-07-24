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
 * July 2008:
 *	- Removed timer optimization through kernel timers.
 *	  RTC and TMU2 timers are also used for mitigating the transmission IRQs.
 * May 2008:
 *	- Suspend/resume functions reviewed and tested the Wake-Up-on LAN
 *	  on the GMAC (mb618).
 *	- Fixed the GMAC 6-bit CRC hash filtering.
 *	- Removed stats from the private structure.
 * April 2008:
 *	- Added kernel timer for handling interrupts.
 *	- Reviewed the GMAC HW configuration.
 *	- Frozen GMAC MMC counters in order to handle related interrupts.
 *	- Fixed a bug within the stmmac_rx function (wrong owner checking).
 * March 2008:
 *	- Added Rx timer optimization (also mitigated by using the normal
 *	  interrupt on completion). See stmmac_timer.c file.
 *	- Added hardware csum for the Gigabit Ethernet device.
 *	- Reviewed the descriptor structures and added a new header file
 *	  (descs.h). 
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
/*#define STMMAC_DEBUG*/
#ifdef STMMAC_DEBUG
#define DBG(nlevel, klevel, fmt, args...) \
		(void)(netif_msg_##nlevel(lp) && \
		printk(KERN_##klevel fmt, ## args))
#else
#define DBG(nlevel, klevel, fmt, args...)  do { } while(0)
#endif

#undef STMMAC_RX_DEBUG
/*#define STMMAC_RX_DEBUG*/
#ifdef STMMAC_RX_DEBUG
#define RX_DBG(fmt,args...)  printk(fmt, ## args)
#else
#define RX_DBG(fmt, args...)  do { } while(0)
#endif

#undef STMMAC_XMIT_DEBUG
/*#define STMMAC_XMIT_DEBUG*/

#define MIN_MTU 46
#define MAX_MTU ETH_DATA_LEN

#define STMMAC_ALIGN(x)	ALIGN((x), dma_get_cache_alignment())
#define STMMAC_IP_ALIGN NET_IP_ALIGN

#define TX_BUFFS_AVAIL(lp) \
	(lp->dirty_tx + lp->dma_tx_size - lp->cur_tx - 1)

/* Module Arguments */
#define TX_TIMEO (5*HZ)
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit Timeout (in milliseconds)");

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, S_IRUGO | S_IWUSR);
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

#define DMA_RX_SIZE 64
static int dma_rx_size_param = DMA_RX_SIZE;
module_param(dma_rx_size_param, int, S_IRUGO);
MODULE_PARM_DESC(dma_rx_size_param, "Number of descriptors in the RX list");

static int flow_ctrl = FLOW_OFF;
module_param(flow_ctrl, int, S_IRUGO);
MODULE_PARM_DESC(flow_ctrl, "Flow control ability [on/off]");

static int pause = PAUSE_TIME;
module_param(pause, int, S_IRUGO);
MODULE_PARM_DESC(pause, "Flow Control Pause Time");

#define TTC_DEFAULT 0x40
static int threshold_ctrl = TTC_DEFAULT;
module_param(threshold_ctrl, int, S_IRUGO);
MODULE_PARM_DESC(threshold_ctrl, "tranfer threshold control");

#if defined (CONFIG_STMMAC_TIMER)
#define RX_IRQ_THRESHOLD	16	/* mitigate rx irq */
#define TX_AGGREGATION		16	/* mitigate tx irq too */
#else
#define RX_IRQ_THRESHOLD 1	/* always Interrupt on completion */
#define TX_AGGREGATION	-1	/* no mitigation by default */
#endif

/* Using timer optimizations, it's worth having some interrupts on frame 
 * reception. This makes safe the network activity especially for the TCP 
 * traffic. */
static int rx_irq_mitigation = RX_IRQ_THRESHOLD;
module_param(rx_irq_mitigation, int, S_IRUGO);
MODULE_PARM_DESC(rx_irq_mitigation, "Rx irq mitigation threshold");

static int tx_aggregation = TX_AGGREGATION;
module_param(tx_aggregation, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_aggregation, "Tx aggregation value");

/* Pay attention to tune timer parameters; take care of both
 * hardware capability and network stabitily/performance impact. 
 * Many tests showed that ~4ms latency seems to be good enough. */
#ifdef CONFIG_STMMAC_TIMER
#define DEFAULT_PERIODIC_RATE	256
static int periodic_rate = DEFAULT_PERIODIC_RATE;
module_param(periodic_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(periodic_rate, "Timer periodic rate (default: 256Hz)");
#endif

static const u32 default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_LINK | NETIF_MSG_IFUP |
				      NETIF_MSG_IFDOWN | NETIF_MSG_TIMER);

static irqreturn_t stmmac_interrupt(int irq, void *dev_id);
static int stmmac_rx(struct net_device *dev, int limit);

extern struct ethtool_ops stmmac_ethtool_ops;
extern struct device_info_t *gmac_setup(unsigned long addr);
extern struct device_info_t *mac100_setup(unsigned long addr);
extern int stmmac_mdio_unregister(struct net_device *ndev);
extern int stmmac_mdio_register(struct net_device *ndev);

#ifdef CONFIG_STMMAC_TIMER
extern int stmmac_timer_close(void);
extern int stmmac_timer_stop(void);
extern int stmmac_timer_start(unsigned int freq);
extern int stmmac_timer_open(struct net_device *dev, unsigned int freq);
#endif

static __inline__ void stmmac_verify_args(void)
{
	/* Wrong parameters are replaced with the default values */
	if (watchdog < 0)
		watchdog = TX_TIMEO;
	if (rx_copybreak < 0)
		rx_copybreak = ETH_FRAME_LEN;
	if (dma_rx_size_param < 0)
		dma_rx_size_param = DMA_RX_SIZE;
	if (dma_tx_size_param < 0)
		dma_tx_size_param = DMA_TX_SIZE;

	if (flow_ctrl > 1)
		flow_ctrl = FLOW_AUTO;
	else if (flow_ctrl < 0)
		flow_ctrl = FLOW_OFF;

	if ((pause < 0) || (pause > 0xffff))
		pause = PAUSE_TIME;

	if (tx_aggregation >= (dma_tx_size_param))
		tx_aggregation = TX_AGGREGATION;

	if (rx_irq_mitigation > (dma_rx_size_param))
		rx_irq_mitigation = RX_IRQ_THRESHOLD;

	return;
}

#if defined (STMMAC_XMIT_DEBUG) || defined (STMMAC_RX_DEBUG)
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
		unsigned int ctrl = (unsigned int)readl(ioaddr + MAC_CTRL_REG);

		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (phydev->duplex != lp->oldduplex) {
			new_state = 1;
			if (!(phydev->duplex)) {
				ctrl &= ~lp->mac_type->hw.link.duplex;
			} else {
				ctrl |= lp->mac_type->hw.link.duplex;
			}
			lp->oldduplex = phydev->duplex;
		}
		/* Flow Control operation */
		if (phydev->pause)
			lp->mac_type->ops->flow_ctrl(ioaddr, phydev->duplex,
						     fc, pause_time);

		if (phydev->speed != lp->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case 1000:
				if (likely(lp->is_gmac))
					ctrl &= ~lp->mac_type->hw.link.port;
				break;
			case 100:
			case 10:
				if (lp->is_gmac) {
					ctrl |= lp->mac_type->hw.link.port;
					if (phydev->speed == SPEED_100) {
						ctrl |=
						    lp->mac_type->hw.link.speed;
					} else {
						ctrl &=
						    ~(lp->mac_type->hw.link.
						      speed);
					}
				} else {
					ctrl &= ~lp->mac_type->hw.link.port;
				}
				lp->fix_mac_speed(lp->bsp_priv, phydev->speed);
				break;
			default:
				if (netif_msg_link(lp))
					printk(KERN_WARNING
					       "%s: Ack!  Speed (%d) is not 10"
					       " or 100!\n", dev->name,
					       phydev->speed);
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

static void display_ring(dma_desc * p, int size)
{
	struct tmp_s {
		u64 a;
		unsigned int b;
		unsigned int c;
	};
	int i;
	for (i = 0; i < size; i++) {
		struct tmp_s *x = (struct tmp_s *)(p + i);
		printk("\t%d [0x%x]: des0=0x%x des1=0x%x buff=0x%x", i,
		       (unsigned int)virt_to_phys(&p[i]), (unsigned int)(x->a),
		       (unsigned int)((x->a) >> 32), x->b);
		printk("\n");
	}
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
	}
	lp->dirty_tx = lp->cur_tx = 0;

	/* Clear the Rx/Tx descriptors */
	lp->mac_type->ops->init_rx_desc(lp->dma_rx, rxsize, rx_irq_mitigation);
	lp->mac_type->ops->init_tx_desc(lp->dma_tx, txsize);

	if (netif_msg_hw(lp)) {
		printk("RX descriptor ring:\n");
		display_ring(lp->dma_rx, rxsize);
		printk("TX descriptor ring:\n");
		display_ring(lp->dma_tx, txsize);
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
						 lp->mac_type->
						 ops->get_tx_len(p),
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
static void stmmac_tx(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int txsize = lp->dma_tx_size;
	unsigned long ioaddr = dev->base_addr;
	int entry = lp->dirty_tx % txsize;

	spin_lock(&lp->tx_lock);
	while (lp->dirty_tx != lp->cur_tx) {
		int last;
		dma_desc *p = lp->dma_tx + entry;

		if (lp->mac_type->ops->read_tx_owner(p))
			break;
		/* verify tx error by looking at the last segment */
		last = lp->mac_type->ops->get_tx_ls(p);
		if (likely(last)) {
			int tx_error = lp->mac_type->ops->tx_status(&dev->stats,
								    &lp->xstats,
								    p, ioaddr);
			if (likely(tx_error == 0)) {
				dev->stats.tx_packets++;
			} else {
				dev->stats.tx_errors++;
			}
		}
		DBG(intr, DEBUG, "stmmac_tx: curr %d, dirty %d\n", lp->cur_tx,
		    lp->dirty_tx);

		if (likely(p->des2)) {
			dma_unmap_single(lp->device, p->des2,
					 lp->mac_type->ops->get_tx_len(p),
					 DMA_TO_DEVICE);
		}
		if (likely(lp->tx_skbuff[entry] != NULL)) {
			dev_kfree_skb_irq(lp->tx_skbuff[entry]);
			lp->tx_skbuff[entry] = NULL;
		}

		lp->mac_type->ops->release_tx_desc(p);

		entry = (++lp->dirty_tx) % txsize;
	}
	if (unlikely(netif_queue_stopped(dev) &&
		     TX_BUFFS_AVAIL(lp) > (MAX_SKB_FRAGS + 1)))
		netif_wake_queue(dev);

	spin_unlock(&lp->tx_lock);
	return;
}

/**
 * stmmac_schedule_rx:
 * @dev: net device structure
 * Description: it schedules the reception process.
 */
void stmmac_schedule_rx(struct net_device *dev)
{
	stmmac_dma_disable_irq_rx(dev->base_addr);

	if (likely(netif_rx_schedule_prep(dev))) {
		__netif_rx_schedule(dev);
	}

	return;
}

static void stmmac_tx_tasklet(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;

	stmmac_tx(dev);

	return;
}

#ifdef CONFIG_STMMAC_TIMER
void stmmac_timer_work(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	stmmac_schedule_rx(dev);

	tasklet_schedule(&lp->tx_task);

	return;
}
#endif

/**
 * stmmac_tx_err:
 * @dev: net device structure
 * Description: clean descriptors and restart the transmission.
 */
static __inline__ void stmmac_tx_err(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->tx_lock);

	netif_stop_queue(dev);

	stmmac_dma_stop_tx(dev->base_addr);
	dma_free_tx_skbufs(dev);
	lp->mac_type->ops->init_tx_desc(lp->dma_tx, lp->dma_tx_size);
	lp->dirty_tx = lp->cur_tx = 0;
	stmmac_dma_start_tx(dev->base_addr);

	dev->stats.tx_errors++;
	netif_wake_queue(dev);

	spin_unlock(&lp->tx_lock);
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
	unsigned long ioaddr = dev->base_addr;
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
			DBG(intr, INFO, "transmit underflow\n");
			stmmac_tx_err(dev);
			lp->xstats.tx_undeflow_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_TJT)) {
			DBG(intr, INFO, "transmit jabber\n");
			lp->xstats.tx_jabber_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_OVF)) {
			DBG(intr, INFO, "recv overflow\n");
			lp->xstats.rx_overflow_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_RU)) {
			DBG(intr, INFO, "receive buffer unavailable\n");
			lp->xstats.rx_buf_unav_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_RPS)) {
			DBG(intr, INFO, "receive process stopped\n");
			lp->xstats.rx_process_stopped_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_RWT)) {
			DBG(intr, INFO, "receive watchdog\n");
			lp->xstats.rx_watchdog_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_ETI)) {
			DBG(intr, INFO, "transmit early interrupt\n");
			lp->xstats.tx_early_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_TPS)) {
			DBG(intr, INFO, "transmit process stopped\n");
			lp->xstats.tx_process_stopped_irq++;
			stmmac_tx_err(dev);
		}
		if (unlikely(intr_status & DMA_STATUS_FBI)) {
			DBG(intr, INFO, "fatal bus error\n");
			lp->xstats.fatal_bus_error_irq++;
			stmmac_tx_err(dev);
		}
	}

	/* NORMAL interrupts */
	if (intr_status & DMA_STATUS_NIS) {
		DBG(intr, INFO, " CSR5[16]: DMA NORMAL IRQ: ");
		if (intr_status & DMA_STATUS_RI) {

			RX_DBG("Receive irq [buf: 0x%08x]\n",
			       readl(ioaddr + DMA_CUR_RX_BUF_ADDR));
			lp->xstats.rx_irq_n++;
			stmmac_schedule_rx(dev);
		}
		if (unlikely(intr_status & (DMA_STATUS_TI))) {
			DBG(intr, INFO, " Transmit irq [buf: 0x%08x]\n",
			    readl(ioaddr + DMA_CUR_TX_BUF_ADDR));
			lp->xstats.tx_irq_n++;
			tasklet_schedule(&lp->tx_task);
		}
	}

	/* Optional hardware blocks, interrupts should be disabled */
	if (unlikely(intr_status &
		     (DMA_STATUS_GPI | DMA_STATUS_GMI | DMA_STATUS_GLI))) {
		printk("%s: unexpected status %08x\n", __FUNCTION__,
		       intr_status);
	}

	DBG(intr, INFO, "\n\n");

	return;
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
	unsigned long ioaddr = dev->base_addr;
	int ret;

	/* Check that the MAC address is valid.  If its not, refuse
	 * to bring the device up. The user must specify an
	 * address using the following linux command:
	 *      ifconfig eth0 hw ether xx:xx:xx:xx:xx:xx  */
	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk(KERN_ERR "%s: no valid eth hw addr\n", __FUNCTION__);
		return -EINVAL;
	}

	/* Attach the PHY */
	ret = stmmac_init_phy(dev);
	if (ret) {
		printk(KERN_ERR "%s: Cannot attach to PHY (error: %d)\n",
		       __FUNCTION__, ret);
		return -ENODEV;
	}

	/* Request the IRQ lines */
	ret = request_irq(dev->irq, &stmmac_interrupt,
			  IRQF_SHARED, dev->name, dev);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: ERROR: allocating the IRQ %d (error: %d)\n",
		       __FUNCTION__, dev->irq, ret);
		return ret;
	}
#ifdef CONFIG_STMMAC_TIMER
	lp->has_timer = stmmac_timer_open(dev, periodic_rate);
	if (unlikely(lp->has_timer < 0)) {
		printk(KERN_WARNING "stmmac: timer opt disabled\n");
		rx_irq_mitigation = 1;
	}
#endif

	/* Create and initialize the TX/RX descriptors rings */
	init_dma_desc_rings(dev);

	/* DMA initialization and SW reset */
	if (lp->mac_type->ops->dma_init(ioaddr, lp->pbl, lp->dma_tx_phy,
					lp->dma_rx_phy) < 0) {
		printk(KERN_ERR "%s: DMA initialization failed\n",
		       __FUNCTION__);
		return -1;
	}

	/* Copy the MAC addr into the HW (in case we have set it with nwhw) */
	set_mac_addr(ioaddr, dev->dev_addr, lp->mac_type->hw.addr_high,
		     lp->mac_type->hw.addr_low);

	/* Initialize the MAC Core */
	lp->mac_type->ops->core_init(ioaddr);
	lp->tx_aggregation = 0;
	lp->shutdown = 0;

	/* Initialise the MMC (if present) to disable all interrupts */
	writel(0xffffffff, ioaddr + MMC_HIGH_INTR_MASK);
	writel(0xffffffff, ioaddr + MMC_LOW_INTR_MASK);

	/* Enable the MAC Rx/Tx */
	stmmac_mac_enable_rx(dev);
	stmmac_mac_enable_tx(dev);

	/* Extra statistics */
	memset(&lp->xstats, 0, sizeof(struct stmmac_extra_stats));
	lp->xstats.threshold = threshold_ctrl;

	/* Estabish the tx/rx operating modes and commands */
	lp->mac_type->ops->dma_operation_mode(ioaddr, threshold_ctrl);

	/* Start the ball rolling... */
	DBG(probe, DEBUG, "%s: DMA RX/TX processes started...\n",
	    ETH_RESOURCE_NAME);
	stmmac_dma_start_tx(ioaddr);
	stmmac_dma_start_rx(ioaddr);

#ifdef CONFIG_STMMAC_TIMER
	if (likely(lp->has_timer == 0))
		stmmac_timer_start(periodic_rate);
#endif
	tasklet_init(&lp->tx_task, stmmac_tx_tasklet, (unsigned long)dev);

	/* Dump DMA/MAC registers */
	if (netif_msg_hw(lp)) {
		lp->mac_type->ops->dump_mac_regs(ioaddr);
		lp->mac_type->ops->dump_dma_regs(ioaddr);
	}

	phy_start(lp->phydev);

	netif_start_queue(dev);
	return 0;
}

static void stmmac_tx_checksum(struct sk_buff *skb)
{
	const int offset = skb_transport_offset(skb);
	unsigned int csum = skb_checksum(skb, offset, skb->len - offset, 0);
	*(u16 *) (skb->data + offset + skb->csum_offset) = csum_fold(csum);
	return;
}

/**
 *  stmmac_release - close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver.
 */
static int stmmac_release(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Stop and disconnect the PHY */
	phy_stop(lp->phydev);
	phy_disconnect(lp->phydev);
	lp->phydev = NULL;

	netif_stop_queue(dev);
	tasklet_kill(&lp->tx_task);

#ifdef CONFIG_STMMAC_TIMER
	if (likely(lp->has_timer == 0)) {
		stmmac_timer_stop();
		stmmac_timer_close();
	}
#endif

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
	unsigned int txsize = lp->dma_tx_size, hwcsum,
	    nfrags = skb_shinfo(skb)->nr_frags,
	    entry = lp->cur_tx % txsize, i, nopaged_len, first = entry;
	dma_desc *p = lp->dma_tx;

	/* This is a hard error log it. */
	if (unlikely(TX_BUFFS_AVAIL(lp) < nfrags + 1)) {
		netif_stop_queue(dev);
		printk(KERN_ERR "%s: BUG! Tx Ring full when queue awake\n",
		       dev->name);
		return NETDEV_TX_BUSY;
	}

	spin_lock_irqsave(&lp->tx_lock, flags);

	if (unlikely((lp->tx_skbuff[entry] != NULL))) {
		printk(KERN_ERR "%s: BUG! Inconsistent Tx skb utilization\n",
		       dev->name);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped += 1;
		return -1;
	}

	hwcsum = 0;
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		if (lp->mac_type->hw.csum == NO_HW_CSUM)
			stmmac_tx_checksum(skb);
		else
			hwcsum = 1;
	}

	/* Get the amount of non-paged data (skb->data). */
	nopaged_len = skb_headlen(skb);

#ifdef STMMAC_XMIT_DEBUG
	if (nfrags > 0) {
		printk("stmmac xmit: len: %d, nopaged_len: %d n_frags: %d\n",
		       skb->len, nopaged_len, nfrags);
	}
#endif

	/* Handle non-paged data (skb->data) */
	p[entry].des2 = dma_map_single(lp->device, skb->data,
				       nopaged_len, DMA_TO_DEVICE);
	lp->tx_skbuff[entry] = skb;
	lp->mac_type->ops->prepare_tx_desc((p + entry), 1, nopaged_len, hwcsum);

	/* Handle paged fragments */
	for (i = 0; i < nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		int len = frag->size;

		lp->cur_tx++;
		entry = lp->cur_tx % txsize;

#ifdef STMMAC_XMIT_DEBUG
		printk("\t[entry %d] segment len: %d\n", entry, len);
#endif
		p[entry].des2 = dma_map_page(lp->device, frag->page,
					     frag->page_offset,
					     len, DMA_TO_DEVICE);
		lp->tx_skbuff[entry] = NULL;
		lp->mac_type->ops->prepare_tx_desc((p + entry), 0, len, hwcsum);
		lp->mac_type->ops->set_tx_owner(p + entry);
	}

	/* If there are more than one fragment, we set the interrupt
	 * on completition bit in the latest segment. */
	lp->mac_type->ops->set_tx_owner(p + first);	/* to avoid raise condition */
	lp->mac_type->ops->set_tx_ls(p + entry);

	lp->mac_type->ops->set_tx_ic(p + entry, 1);

	lp->cur_tx++;

#ifdef STMMAC_XMIT_DEBUG
	if (netif_msg_pktdata(lp)) {
		printk("stmmac xmit: current=%d, dirty=%d, entry=%d, "
		       "first=%d, nfrags=%d\n",
		       (lp->cur_tx % txsize), (lp->dirty_tx % txsize), entry,
		       first, nfrags);
		display_ring(lp->dma_tx, txsize);
		printk(">>> frame to be transmitted: ");
		print_pkt(skb->data, skb->len);
	}
#endif
	if (TX_BUFFS_AVAIL(lp) <= (MAX_SKB_FRAGS + 1) ||
	    (!(lp->mac_type->hw.link.duplex) && hwcsum)) {
		netif_stop_queue(dev);
	} else {
		/* Aggregation of Tx interrupts */
		if (lp->tx_aggregation <= tx_aggregation) {
			lp->tx_aggregation++;
			lp->mac_type->ops->set_tx_ic(p + entry, 0);
		} else {
			lp->tx_aggregation = 0;
		}
	}

	dev->stats.tx_bytes += skb->len;
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
	dma_desc *p = lp->dma_rx;

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
								  skb->data,
								  bfsize,
								  DMA_FROM_DEVICE);
			(p + entry)->des2 = lp->rx_skbuff_dma[entry];
			RX_DBG("\trefill entry #%d\n", entry);
		}
		lp->mac_type->ops->set_rx_owner(p + entry);
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
	display_ring(lp->dma_rx, rxsize);
#endif
	lp->xstats.rx_poll_n++;

	for (count = 0; count < limit; ++count) {
		dma_desc *p = lp->dma_rx + entry;

		if (lp->mac_type->ops->read_rx_owner(p))
			break;
		/* read the status of the incoming frame */
		if (unlikely((lp->mac_type->ops->rx_status(&dev->stats,
							   &lp->xstats,
							   p) < 0))) {
			dev->stats.rx_errors++;
		} else {
			struct sk_buff *skb;
			/* Length should omit the CRC */
			int frame_len =
			    lp->mac_type->ops->get_rx_frame_len(p) - 4;

			RX_DBG
			    ("\tdesc: 0x%0x [entry %d] buff=0x%x\n",
			     (unsigned int)p, entry, p->des2);

			/* Check if the packet is long enough to accept without
			   copying to a minimally-sized skbuff. */
			if (unlikely(frame_len < rx_copybreak)) {
				skb =
				    dev_alloc_skb(STMMAC_ALIGN(frame_len + 2));
				if (unlikely(!skb)) {
					if (printk_ratelimit())
						printk(KERN_NOTICE
						       "%s: low memory, "
						       "packet dropped.\n",
						       dev->name);
					dev->stats.rx_dropped++;
					break;
				}

				skb_reserve(skb, STMMAC_IP_ALIGN);
				dma_sync_single_for_cpu(lp->device,
							lp->rx_skbuff_dma
							[entry], frame_len,
							DMA_FROM_DEVICE);
				skb_copy_to_linear_data(skb,
							lp->rx_skbuff[entry]->
							data, frame_len);

				skb_put(skb, frame_len);
				dma_sync_single_for_device(lp->device,
							   lp->rx_skbuff_dma
							   [entry], frame_len,
							   DMA_FROM_DEVICE);
			} else {	/* zero-copy */
				skb = lp->rx_skbuff[entry];
				if (unlikely(!skb)) {
					printk(KERN_ERR "%s: Inconsistent Rx "
					       "descriptor chain.\n",
					       dev->name);
					dev->stats.rx_dropped++;
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
			if (lp->mac_type->ops->rx_checksum(p) < 0)
				skb->ip_summed = CHECKSUM_NONE;
			else
				skb->ip_summed = CHECKSUM_UNNECESSARY;

			netif_receive_skb(skb);

			dev->stats.rx_packets++;
			dev->stats.rx_bytes += frame_len;
			lp->xstats.rx_bytes += frame_len;
			dev->last_rx = jiffies;
		}
		entry = (++lp->cur_rx) % rxsize;
		p = lp->dma_rx + entry;
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
	int work_done, limit = min(dev->quota, *budget);;

	work_done = stmmac_rx(dev, limit);
	dev->quota -= work_done;
	*budget -= work_done;

	if (work_done < limit) {
		netif_rx_complete(dev);
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
	printk("(current=%d, dirty=%d)\n",
	       (lp->cur_tx % lp->dma_tx_size),
	       (lp->dirty_tx % lp->dma_tx_size));
	printk("DMA tx ring status: \n");
	display_ring(lp->dma_tx, lp->dma_tx_size);
#endif
	/* Remove tx optmizarion */
	tx_aggregation = TX_AGGREGATION;
	lp->tx_aggregation = 0;

	/* Clear Tx resources and restart transmitting again */
	stmmac_tx_err(dev);

	dev->trans_start = jiffies;

	return;
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
	lp->mac_type->ops->set_filter(dev);

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
		printk(KERN_ERR
		       "%s: must be stopped to change its MTU\n", dev->name);
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
	struct eth_driver_local *lp = netdev_priv(dev);

	if (unlikely(!dev)) {
		printk(KERN_ERR "%s: invalid dev pointer\n", __FUNCTION__);
		return IRQ_NONE;
	}

	if (lp->is_gmac) {
		unsigned long ioaddr = dev->base_addr;
		/* To handle GMAC own interrupts */
		lp->mac_type->ops->host_irq_status(ioaddr);
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
	dev->features |= (NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_HIGHDMA);

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

	if (lp->is_gmac) {
		lp->rx_csum = 1;
	}

	/* Just to keep aligned values. */
	lp->dma_tx_size = STMMAC_ALIGN(dma_tx_size_param);
	lp->dma_rx_size = STMMAC_ALIGN(dma_rx_size_param);
	lp->dma_buf_sz = lp->mac_type->hw.buf_size;

	if (flow_ctrl)
		lp->flow_ctrl = FLOW_AUTO;	/* RX/TX pause on */

	lp->pause = pause;

	dev->poll = stmmac_poll;
	dev->weight = 64;

	/* Get the MAC address */
	get_mac_address(dev->base_addr, dev->dev_addr,
			lp->mac_type->hw.addr_high, lp->mac_type->hw.addr_low);

	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk(KERN_WARNING "\tno valid MAC address; "
		       "please, set using ifconfig or nwhwconfig!\n");
	}

	if ((ret = register_netdev(dev))) {
		printk(KERN_ERR "%s: ERROR %i registering the device\n",
		       __FUNCTION__, ret);
		return -ENODEV;
	}

	DBG(probe, DEBUG, "%s: Scatter/Gather: %s - HW checksums: %s\n",
	    dev->name, (dev->features & NETIF_F_SG) ? "on" : "off",
	    (dev->features & NETIF_F_HW_CSUM) ? "on" : "off");

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
	lp->mac_type = device;
	lp->wolenabled = lp->mac_type->hw.pmt;	// PMT supported

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
	printk(KERN_DEBUG "MDIO bus registered!\n");
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

static int stmmac_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct eth_driver_local *lp = netdev_priv(dev);

	if (!dev || !netif_running(dev))
		return 0;

	spin_lock(&lp->lock);

	if (state.event == PM_EVENT_SUSPEND) {
		netif_device_detach(dev);
		netif_stop_queue(dev);
		phy_stop(lp->phydev);
		netif_stop_queue(dev);
		tasklet_disable(&lp->tx_task);

#ifdef CONFIG_STMMAC_TIMER
		if (likely(lp->has_timer == 0)) {
			stmmac_timer_stop();
		}
#endif
		/* Stop TX/RX DMA */
		stmmac_dma_stop_tx(dev->base_addr);
		stmmac_dma_stop_rx(dev->base_addr);
		/* Clear the Rx/Tx descriptors */
		lp->mac_type->ops->init_rx_desc(lp->dma_rx, lp->dma_rx_size,
						rx_irq_mitigation);
		lp->mac_type->ops->init_tx_desc(lp->dma_tx, lp->dma_tx_size);

		stmmac_mac_disable_tx(dev);

		if (device_may_wakeup(&(pdev->dev))) {
			/* Enable Power down mode by programming the PMT regs */
			if (lp->wolenabled == PMT_SUPPORTED)
				lp->mac_type->ops->pmt(dev->base_addr,
						       lp->wolopts);
		} else {
			stmmac_mac_disable_rx(dev);
		}
	} else {
		lp->shutdown = 1;
		/* Although this can appear slightly redundant it actually 
		 * makes fast the standby operation and guarantees the driver 
		 * working if hibernation is on media. */
		stmmac_release(dev);
	}

	spin_unlock(&lp->lock);
	return 0;
}

static int stmmac_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;

	if (!netif_running(dev))
		return 0;

	spin_lock(&lp->lock);

	if (lp->shutdown) {
		/* Re-open the interface and re-init the MAC/DMA
		   and the rings. */
		stmmac_open(dev);
		goto out_resume;
	}

	netif_device_attach(dev);

	/* Enable the MAC and DMA */
	stmmac_mac_enable_rx(dev);
	stmmac_mac_enable_tx(dev);
	stmmac_dma_start_tx(ioaddr);
	stmmac_dma_start_rx(ioaddr);

#ifdef CONFIG_STMMAC_TIMER
	if (likely(lp->has_timer == 0)) {
		stmmac_timer_start(periodic_rate);
	}
#endif
	tasklet_enable(&lp->tx_task);

	phy_start(lp->phydev);

	netif_start_queue(dev);

      out_resume:
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
		} else if (!strncmp(opt, "tc:", 3)) {
			threshold_ctrl = simple_strtoul(opt + 3, NULL, 0);
		} else if (!strncmp(opt, "txmit:", 6)) {
			tx_aggregation = simple_strtoul(opt + 6, NULL, 0);
		} else if (!strncmp(opt, "rxmit:", 6)) {
			rx_irq_mitigation = simple_strtoul(opt + 6, NULL, 0);
#ifdef CONFIG_STMMAC_TIMER
		} else if (!strncmp(opt, "period:", 7)) {
			periodic_rate = simple_strtoul(opt + 7, NULL, 0);
#endif
		}
	}
	return 0;
}

__setup("stmmaceth=", stmmac_cmdline_opt);

module_init(stmmac_init_module);
module_exit(stmmac_cleanup_module);

MODULE_DESCRIPTION("STMMAC 10/100/1000 Ethernet driver");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL");
