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
 *
 *  Sep 2007:
 *	- first version of this driver
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
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#include <linux/if_vlan.h>
#endif
#include <linux/dma-mapping.h>
#include "stmmac.h"

#define DMA_MAX_BUFFER_SIZE 0x7ff	/* maximum value in according to the TBS1/2 RBS1/2 bits */
#define DMA_BUFFER_SIZE DMA_MAX_BUFFER_SIZE	//0x600
#define TDES1_MAX_BUF1_SIZE ((DMA_BUFFER_SIZE << DES1_RBS1_SIZE_SHIFT) & \
			DES1_RBS1_SIZE_MASK);
#define TDES1_MAX_BUF2_SIZE ((DMA_BUFFER_SIZE << DES1_RBS2_SIZE_SHIFT) & \
			DES1_RBS2_SIZE_MASK);
#define MIN_MTU 46
#define MAX_MTU ETH_DATA_LEN

#undef STMMAC_DEBUG
/*#define STMMAC_DEBUG*/
#ifdef STMMAC_DEBUG
#define DBG(nlevel, klevel, fmt, args...) \
                (void)(netif_msg_##nlevel(lp) && \
                printk(KERN_##klevel fmt, ## args))
#else
#define DBG(nlevel, klevel, fmt, args...)  do { } while(0)
#endif

#undef STMMAC_TX_DEBUG
/*#define STMMAC_TX_DEBUG*/
#ifdef STMMAC_TX_DEBUG
#define TX_DBG(mss, klevel, fmt, args...) \
                if (mss!=0)     \
                printk(KERN_##klevel fmt, ## args)
#else
#define TX_DBG(mss, klevel, fmt, args...)  do { } while(0)
#endif

#undef STMMAC_RX_DEBUG
/*#define STMMAC_RX_DEBUG*/
#ifdef STMMAC_RX_DEBUG
#define RX_DBG(fmt,args...)  printk(fmt, ## args)
#else
#define RX_DBG(fmt, args...)  do { } while(0)
#endif

/* Module Arguments */
#define TX_TIMEO (5*HZ)
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit Timeout (in milliseconds)");

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, S_IRUGO);
MODULE_PARM_DESC(debug, "Message Level (0: no output, 16: all)");

static int rx_copybreak = ETH_FRAME_LEN + 22;
module_param(rx_copybreak, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rx_copybreak, "Copy only tiny-frames");

static int phy_n = -1;
module_param(phy_n, int, S_IRUGO);
MODULE_PARM_DESC(phy_n, "Physical device address");

static int dma_buffer_size = DMA_BUFFER_SIZE;
module_param(dma_buffer_size, int, S_IRUGO);
MODULE_PARM_DESC(dma_buffer_size, "DMA buffer size");

#define DMA_TX_SIZE 16
static int dma_tx_size_param = DMA_TX_SIZE;
module_param(dma_tx_size_param, int, S_IRUGO);
MODULE_PARM_DESC(dma_buffer_size, "Number of descriptors in the TX list");

#define DMA_RX_SIZE 32
static int dma_rx_size_param = DMA_RX_SIZE;
module_param(dma_rx_size_param, int, S_IRUGO);
MODULE_PARM_DESC(dma_buffer_size, "Number of descriptors in the RX list");

#define TX_BUFFS_AVAIL(lp) \
        (lp->dirty_tx + lp->dma_tx_size - lp->cur_tx - 1)

static const char version[] = "stmmac - (C) 2006-2007 STMicroelectronics\n";

static const u32 default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_LINK | NETIF_MSG_IFUP |
				      NETIF_MSG_IFDOWN | NETIF_MSG_TIMER);

extern int stmmac_mdio_unregister(struct net_device *ndev);
extern int stmmac_mdio_register(struct net_device *ndev);
extern struct ethtool_ops stmmac_ethtool_ops;
extern struct stmmmac_driver mac_driver;
static irqreturn_t stmmac_interrupt(int irq, void *dev_id);
static int stmmac_poll(struct net_device *dev, int *budget);
static void stmmac_check_mod_params(struct net_device *dev);

static inline void print_mac_addr(u8 addr[6])
{
	int i;
	for (i = 0; i < 5; i++)
		printk("%2.2x:", addr[i]);
	printk("%2.2x\n", addr[5]);
	return;
}

#ifdef STMMAC_DEBUG
static void print_pkt(unsigned char *buf, int len)
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

	DBG(probe, DEBUG, "stmmac_adjust_link: called.  address %d link %d\n",
	    phydev->addr, phydev->link);

	spin_lock_irqsave(&lp->lock, flags);
	if (phydev->link) {
		unsigned int ctrl = (unsigned int)readl(ioaddr + MAC_CONTROL);

		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (phydev->duplex != lp->oldduplex) {
			new_state = 1;
			if (!(phydev->duplex))
				ctrl &= ~STMMAC_FULL_DUPLEX;
			else
				ctrl |= STMMAC_FULL_DUPLEX;
			lp->oldduplex = phydev->duplex;
		}

		if (phydev->speed != lp->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case 1000:
				ctrl |= STMMAC_PORT;	/* GMII...if supported */
			case 100:
			case 10:
				ctrl &= ~STMMAC_PORT;	/* MII */
				/* In RMII mode, the 7109 MAC 10/100 needs 
				 * to change the MAC speed field in
				 * system configuration register. */
				if (lp->mac->have_hw_fix) {
					lp->fix_mac_speed(lp->bsp_priv,
							  phydev->speed);
				} else {
					/* GMAC needs no special hack...
					 * just reduces mode distinguishes
					 * between 10 and 100 */
					if (phydev->speed == SPEED_100) {
						ctrl |= STMMAC_SPEED_100;
					} else {
						ctrl &= ~(STMMAC_SPEED_100);
					}
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

		writel(ctrl, ioaddr + MAC_CONTROL);

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
 * Description: the function sets the hardware MAC address
 */
static void set_mac_addr(unsigned long ioaddr, u8 Addr[6])
{
	unsigned long data;

	data = (Addr[5] << 8) | Addr[4];
	writel(data, ioaddr + MAC_ADDR_HIGH);
	data = (Addr[3] << 24) | (Addr[2] << 16) | (Addr[1] << 8) | Addr[0];
	writel(data, ioaddr + MAC_ADDR_LOW);

	return;
}

/**
 * get_mac_addr
 * @ioaddr: device I/O address
 * @addr: mac address
 * Description: the function gets the hardware MAC address
 */
static void get_mac_address(unsigned long ioaddr, unsigned char *addr)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = (unsigned int)readl(ioaddr + MAC_ADDR_HIGH);
	lo_addr = (unsigned int)readl(ioaddr + MAC_ADDR_LOW);

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
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	/* set the RE (receive enable, bit 2) */
	value |= (MAC_CONTROL_RE);
	writel(value, ioaddr + MAC_CONTROL);
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
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	/* set: TE (transmitter enable, bit 3) */
	value |= (MAC_CONTROL_TE);
	writel(value, ioaddr + MAC_CONTROL);
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
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	value &= ~MAC_CONTROL_RE;
	writel(value, ioaddr + MAC_CONTROL);
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
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	value &= ~(MAC_CONTROL_TE);
	writel(value, ioaddr + MAC_CONTROL);
	return;
}

static void display_dma_desc_ring(dma_desc * p, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		printk("\t%d [0x%x]: "
		       "desc0=0x%x desc1=0x%x buffer1=0x%x", i,
		       (unsigned int)virt_to_phys(&p[i].des0), p[i].des0,
		       p[i].des1, (unsigned int)p[i].des2);
		if (p[i].des3 != 0)
			printk(" buffer2=0x%x", (unsigned int)p[i].des3);
		printk("\n");
	}
}

/**
 * clear_dma_descs - reset the DMA descriptors
 * @p: it starts pointing to the first element in the ring.
 * @ring_size: it is the size of the ring.
 * @own_bit: it is the owner bit (RX: OWN_BIT - TX: 0).
 * Description: this function clears both RX and TX descriptors.
 * Note that the driver uses the 'implicit' scheme for implementing
 * the TX/RX DMA linked lists. So the second buffer doesn't point
 * to the next descriptor.
 */
static void clear_dma_descs(dma_desc * p, unsigned int ring_size,
			    unsigned int own_bit)
{
	int i;
	for (i = 0; i < ring_size; i++) {
		p->des0 = own_bit;
		if (!(own_bit))
			p->des1 = 0;
		else
			p->des1 = (dma_buffer_size << DES1_RBS1_SIZE_SHIFT);
		if (i == ring_size - 1) {
			p->des1 |= DES1_CONTROL_TER;
		}
		p->des3 = 0;
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
	unsigned int txsize = lp->dma_tx_size;
	unsigned int rxsize = lp->dma_rx_size;
	lp->dma_buf_sz = dma_buffer_size;

	DBG(probe, DEBUG, "%s: allocate and init the DMA RX/TX\n",
	    ETH_RESOURCE_NAME);

	lp->rx_skbuff_dma =
	    (dma_addr_t *) kmalloc(lp->dma_rx_size * sizeof(dma_addr_t),
				   GFP_KERNEL);
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
	    "(buff size: %d)\n", lp->dma_buf_sz);
	for (i = 0; i < rxsize; i++) {
		dma_desc *p = lp->dma_rx + i;
		struct sk_buff *skb = dev_alloc_skb(lp->dma_buf_sz);
		skb->dev = dev;
		skb_reserve(skb, NET_IP_ALIGN);
		lp->rx_skbuff[i] = skb;
		if (unlikely(skb == NULL)) {
			printk(KERN_ERR "%s: Rx init fails; skb is NULL\n",
			       __FUNCTION__);
			break;
		}
		lp->rx_skbuff_dma[i] = dma_map_single(lp->device, skb->data,
						      lp->dma_buf_sz,
						      DMA_FROM_DEVICE);
		p->des2 = lp->rx_skbuff_dma[i];
		DBG(probe, DEBUG, "[0x%08x]\t[0x%08x]\n",
		    (unsigned int)lp->rx_skbuff[i]->data,
		    (unsigned int)lp->rx_skbuff_dma[i]);
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
	clear_dma_descs(lp->dma_rx, rxsize, OWN_BIT);
	clear_dma_descs(lp->dma_tx, txsize, 0);

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
						 (p->
						  des1 & DES1_RBS1_SIZE_MASK) >>
						 DES1_RBS1_SIZE_SHIFT,
						 DMA_TO_DEVICE);
			}
			if ((lp->dma_tx + i)->des3) {
				dma_unmap_single(lp->device, p->des3,
						 (p->
						  des1 & DES1_RBS2_SIZE_MASK) >>
						 DES1_RBS2_SIZE_SHIFT,
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

	if (netif_msg_hw(lp))
		lp->mac->dma_registers(ioaddr);

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
 * stmmac_clean_tx_irq1
 * @data:  address of the private member of the device structure
 * Description: it is used for freeing the TX resources.  
 */
static void stmmac_clean_tx_irq1(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int txsize = lp->dma_tx_size;
	int entry = lp->dirty_tx % txsize;

	spin_lock(&lp->tx_lock);

	while (lp->dirty_tx != lp->cur_tx) {
		dma_desc *p = lp->dma_tx + entry;
		int status = p->des0;
		if (!(status & OWN_BIT)) {
			/* When the transmission is completed the frame status
			 * is written into TDESC0 of the descriptor having the 
			 * LS bit set. */
			if (likely(p->des1 & TDES1_CONTROL_LS)) {
				if (unlikely(lp->mac->check_tx_summary(&lp->stats, status)<0)) {
					lp->stats.tx_errors++;
				} else {
					lp->stats.tx_packets++;
				}
			}
#ifdef STMMAC_TX_DEBUG
			if (netif_queue_stopped(dev))
				if (netif_msg_tx_done(lp))
					printk("tx done free entry %d\n",
					       entry);
#endif
			if (p->des2) {
				dma_unmap_single(lp->device, p->des2,
					 (p->des1 & DES1_RBS1_SIZE_MASK)>>DES1_RBS1_SIZE_SHIFT,
					 DMA_TO_DEVICE);
				p->des2 = 0;
			}
			if (unlikely(p->des3)) {
				dma_unmap_single(lp->device, p->des3,
					 (p->des1 & DES1_RBS2_SIZE_MASK)>>DES1_RBS2_SIZE_SHIFT,
						 DMA_TO_DEVICE);
				p->des3 = 0;
			}
			if (lp->tx_skbuff[entry] != NULL) {
				dev_kfree_skb_irq(lp->tx_skbuff[entry]);
				lp->tx_skbuff[entry] = NULL;
			}
		}
		entry = (++lp->dirty_tx) % txsize;
	}
	if (netif_queue_stopped(dev)) {
		/*printk ("TX queue started.\n"); */
		netif_wake_queue(dev);
	}

	spin_unlock(&lp->tx_lock);
	return;
}

/**
 * stmmac_dma_interrupt - Interrupt handler for the STMMAC DMA
 * @dev: net device structure
 * Description: It determines if we have to call either the Rx or the Tx
 * interrupt handler.
 */
static void stmmac_dma_interrupt(struct net_device *dev)
{
	unsigned int status;
	unsigned int ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);

	lp->rx_buff = readl(ioaddr + DMA_CUR_RX_BUF_ADDR);
	/* read the status register (CSR5) */
	status = (unsigned int)readl(ioaddr + DMA_STATUS);

	DBG(intr, INFO, "%s: [CSR5: 0x%08x]\n", __FUNCTION__, status);

#ifdef STMMAC_DEBUG
	/* It displays the DMA transmit process state (CSR5 register) */
	if (netif_msg_tx_done(lp))
		show_tx_process_state(status);
	if (netif_msg_rx_status(lp))
		show_rx_process_state(status);
#endif
	/* Process the NORMAL interrupts */
	if (status & DMA_STATUS_NIS) {
		DBG(intr, INFO, " CSR5[16]: DMA NORMAL IRQ: ");
		if (status & DMA_STATUS_RI) {

			RX_DBG("Receive irq [buf: 0x%08x]\n", lp->rx_buff);
			/*display_dma_desc_ring(lp->dma_rx, lp->dma_rx_size); */
			stmmac_dma_disable_irq_rx(ioaddr);
			if (likely(netif_rx_schedule_prep(dev))) {
				__netif_rx_schedule(dev);
			} else {
				RX_DBG("IRQ: bug!interrupt while in poll\n");
			}

		}
		if (status & DMA_STATUS_TI) {
			DBG(intr, INFO, " Transmit irq [buf: 0x%lx]\n",
			    readl(ioaddr + DMA_CUR_TX_BUF_ADDR));
			stmmac_clean_tx_irq1(dev);
		}
	}
	DBG(intr, INFO, "\n\n");

	/* Clear the interrupt by writing a logic 1 to the CSR5[15-0] */
	writel(status, ioaddr + DMA_STATUS);
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
int stmmac_open(struct net_device *dev)
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

	/* Request the IRQ lines */
	ret = request_irq(dev->irq, &stmmac_interrupt,
			  IRQF_SHARED, dev->name, dev);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: ERROR: allocating the IRQ %d (error: %d)\n",
		       __FUNCTION__, dev->irq, ret);
		return ret;
	}

	/* Attach the PHY */
	ret = stmmac_init_phy(dev);
	if (ret) {
		printk(KERN_ERR "%s: Cannot attach to PHY (error: %d)\n",
		       __FUNCTION__, ret);
		return -ENODEV;
	}

	/* Create and initialize the TX/RX descriptors rings */
	init_dma_desc_rings(dev);

	/* Intialize the DMA controller and send the SW reset
	 * This must be after we have successfully initialised the PHY
	 * (see comment in stmmac_dma_reset). */
	if (stmmac_dma_init(dev) < 0) {
		DBG(probe, ERR, "%s: DMA initialization failed\n",
		    __FUNCTION__);
		return -1;
	}

	/* Copy the MAC addr into the HW in case we have set it with nwhw */
	printk(KERN_DEBUG "stmmac_open (%s) ", lp->mac->name);
	print_mac_addr(dev->dev_addr);
	set_mac_addr(dev->base_addr, dev->dev_addr);

	/* Initialize the MAC110 Core */
	lp->mac->core_init(dev);

	/* Enable the MAC/DMA */
	stmmac_mac_enable_rx(dev);
	stmmac_mac_enable_tx(dev);

	if (netif_msg_hw(lp))
		lp->mac->mac_registers((unsigned int)dev->base_addr);

	phy_start(lp->phydev);

	/* Start the ball rolling... */
	DBG(probe, DEBUG, "%s: DMA RX/TX processes started...\n",
	    ETH_RESOURCE_NAME);

	stmmac_dma_start_rx(dev->base_addr);
	stmmac_dma_start_tx(dev->base_addr);

	netif_start_queue(dev);
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
int stmmac_release(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Stop the PHY */
	phy_stop(lp->phydev);
	phy_disconnect(lp->phydev);
	lp->phydev = NULL;

	/* Free the IRQ lines */
	free_irq(dev->irq, dev);

	/* Stop TX/RX DMA and clear the descriptors */
	stmmac_dma_stop_tx(dev->base_addr);
	stmmac_dma_stop_rx(dev->base_addr);

	free_dma_desc_resources(dev);

	/* Disable the MAC core */
	stmmac_mac_disable_tx(dev);
	stmmac_mac_disable_rx(dev);

	/* Change the link status */
	netif_carrier_off(dev);

	return 0;
}

/**
 *  stmmac_fill_tx_buffer
 *  @data : data buffer
 *  @size : fragment size
 *  @mss : Maximum  Segment Size
 *  @lp : driver local structure
 *  @first : first element in the ring
 *  Description: it is used for filling the DMA tx ring with the frame to be
 *  transmitted.
 *  Note that the algorithm works both for the non-paged data and for the paged
 *  fragment (SG).
 *  Return value:
 *    current entry point in the tx ring
 */
static int stmmac_fill_tx_buffer(void *data, unsigned int size,
				 unsigned int mss,
				 struct eth_driver_local *lp, int first)
{
	int new_des = 0;
	void *addr = data;
	dma_desc *p = lp->dma_tx;
	unsigned int entry;
	int bsize = lp->dma_buf_sz;
	unsigned int txsize = lp->dma_tx_size;

	TX_DBG(mss, INFO, "  %s (size=%d, addr=0x%x)\n", __FUNCTION__,
	       size, (unsigned int)addr);
	do {
		if (new_des) {
			lp->cur_tx++;
			new_des = 0;
		}
		entry = lp->cur_tx % txsize;
		/* Set the owner field */
		p[entry].des0 = OWN_BIT;
		/* Reset the descriptor number 1 */
		p[entry].des1 = (p[entry].des1 & DES1_CONTROL_TER);
		if (first)
			p[entry].des1 |= TDES1_CONTROL_FS;
		else
			lp->tx_skbuff[entry] = NULL;

		TX_DBG(mss, INFO, "\t[entry =%d] buf1 len=%d\n",
		       entry, min((int)size, bsize));
		/* If the data size is too big we need to use the buffer 2
		 * (in the same descriptor) or, if necessary, another descriptor
		 * in the ring. */
		if (likely(size < bsize)) {
			p[entry].des1 |= ((size << DES1_RBS1_SIZE_SHIFT) &
					  DES1_RBS1_SIZE_MASK);
			p[entry].des2 = dma_map_single(lp->device, addr,
						       size, DMA_TO_DEVICE);
		} else {
			int b2_size = (size - bsize);

			p[entry].des1 |= TDES1_MAX_BUF1_SIZE;
			p[entry].des2 = dma_map_single(lp->device, addr, bsize,
						       DMA_TO_DEVICE);

			/* Check if we need to use the buffer 2 */
			if (b2_size > 0) {
				void *buffer2 = addr;

				TX_DBG(mss, INFO, "\t[entry=%d] buf2 len=%d\n",
				       entry, min(b2_size, bsize));

				/* Check if we need another descriptor. */
				if (b2_size > bsize) {
					b2_size = bsize;
					size -= (2 * bsize);
					addr += ((2 * bsize) + 1);
					new_des = 1;
					TX_DBG(mss, INFO,
					       "\tnew descriptor - "
					       "%s (len = %d)\n",
					       (first) ? "skb->data" :
					       "Frag", size);
				}
				p[entry].des3 = dma_map_single(lp->device,
							       (buffer2 +
								bsize + 1),
							       b2_size,
							       DMA_TO_DEVICE);
				if (b2_size == bsize) {
					p[entry].des1 |= TDES1_MAX_BUF2_SIZE;
				} else {
					p[entry].des1 |=
					    ((b2_size << DES1_RBS2_SIZE_SHIFT)
					     & DES1_RBS2_SIZE_MASK);
				}
			}
		}
	} while (new_des);
	return entry;
}

/**
 *  stmmac_xmit - Tx entry point of the driver
 *  @skb : the socket buffer
 *  @dev : device pointer
 *  Description :
 *  This function is the Tx entry point of the driver.
 */
int stmmac_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	dma_desc *p = lp->dma_tx;
	unsigned int txsize = lp->dma_tx_size;
	unsigned int nfrags = skb_shinfo(skb)->nr_frags,
	    entry = lp->cur_tx % txsize, i, mss = 0, nopaged_len;
	unsigned long flags;

	if (skb_padto(skb, ETH_ZLEN)) {
		printk("stmmac: xmit padto returned 0\n");
		return 0;
	}

	spin_lock_irqsave(&lp->tx_lock, flags);
	/* This is a hard error log it. */
	if (unlikely(TX_BUFFS_AVAIL(lp) < nfrags + 1)) {
		if (netif_msg_drv(lp)) {
			printk(KERN_ERR
			       "%s: bug! Tx Ring full when queue awake!\n",
			       dev->name);
		}
		return 1;
	}

	/* Verify the checksum */
	lp->mac->tx_checksum(skb);

	/* Get the amount of non-paged data (skb->data). */
	nopaged_len = skb_headlen(skb);
	lp->tx_skbuff[entry] = skb;
	TX_DBG(mss, INFO, "\n%s:\n(skb->len=%d, nfrags=%d, "
	       "nopaged_len=%d, mss=%d)\n", __FUNCTION__, skb->len,
	       nfrags, nopaged_len, mss);
	/* Handle the non-paged data (skb->data) */
	stmmac_fill_tx_buffer(skb->data, nopaged_len, mss, lp, 1);

	/* Handle the paged fragments */
	for (i = 0; i < nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		void *addr =
		    ((void *)page_address(frag->page) + frag->page_offset);
		int len = frag->size;

		lp->cur_tx++;
		entry = stmmac_fill_tx_buffer(addr, len, mss, lp, 0);
	}
	p[entry].des1 |= TDES1_CONTROL_LS | TDES1_CONTROL_IC;
	lp->cur_tx++;
	lp->stats.tx_bytes += skb->len;

#ifdef STMMAC_DEBUG
	if (netif_msg_pktdata(lp)) {
		printk(">>> (current=%d, dirty=%d; entry=%d)\n",
		       (lp->cur_tx % txsize), (lp->dirty_tx % txsize), entry);
		display_dma_desc_ring(lp->dma_tx, txsize);
		printk(">>> frame to be transmitted: ");
		print_pkt(skb->data, skb->len);
	}
#endif
	/* CSR1 enables the transmit DMA to check for new descriptor */
	writel(1, dev->base_addr + DMA_XMT_POLL_DEMAND);

	if (TX_BUFFS_AVAIL(lp) <= (MAX_SKB_FRAGS + 1)) {
#if 0
		printk("XMIT (current=%d, dirty=%d; entry=%d, "
		       "nfrags=%d,skb->data=0x%p\n",
		       (lp->cur_tx % txsize), (lp->dirty_tx % txsize),
		       entry, nfrags, skb->data);
		display_dma_desc_ring(lp->dma_tx, txsize);
		printk("\tstop Queue: buffs_avail %d)\n", TX_BUFFS_AVAIL(lp));
#endif
		netif_stop_queue(dev);
	}

	dev->trans_start = jiffies;

	spin_unlock_irqrestore(&lp->tx_lock, flags);
	return 0;
}

static __inline__ void stmmac_rx_refill(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int rxsize = lp->dma_rx_size;

	for (; lp->cur_rx - lp->dirty_rx > 0; lp->dirty_rx++) {
		struct sk_buff *skb;
		int entry = lp->dirty_rx % rxsize;
		if (lp->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(lp->dma_buf_sz);
			lp->rx_skbuff[entry] = skb;
			if (unlikely(skb == NULL)) {
				printk(KERN_ERR "%s: skb is NULL\n",
				       __FUNCTION__);
				break;
			}
			skb->dev = dev;
			lp->rx_skbuff_dma[entry] = dma_map_single(lp->device,
							  skb->data, lp->dma_buf_sz,
							  DMA_FROM_DEVICE);
			(lp->dma_rx + entry)->des2 = lp->rx_skbuff_dma[entry];
			RX_DBG(rx_status, INFO, "\trefill entry #%d\n", entry);
		}
	}
	return;
}

/**
 *  stmmac_poll - stmmac poll method (NAPI)
 *  @dev : device pointer
 *  @budget : maximum number of packets that the current CPU can receive from
 *	      all interfaces.
 *  Description :
 *   This function implements the the reception process.
 *   It is based on NAPI which provides a "inherent mitigation" in order
 *   to improve network performance.
 *   Note: when the RIE bit, in the INT_EN register (CSR7), is turned off 
 *   (in order to disable the dma rx interrupt in the irq handler), 
 *   the corresponding bit in the status register (CSR5 ) continues to be 
 *   turned on with new packet arrivals. In poll method, we loop until the 
 *   descriptor 0 is not owned by the DMA. If quota is exceeded and there are
 *   pending work we do not touch the irq status and the method is not removed
 *   from the poll list. Otherwise we can call the netif_rx_complete function
 *   and normal exit. To debug that enable the STMMAC_RX_DEBUG macro.
 *   In the end, stmmac_poll also supports the zero-copy mechanism by 
 *   tuning the rx_copybreak parameter.
 */
static int stmmac_poll(struct net_device *dev, int *budget)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int rxsize = lp->dma_rx_size;
	int frame_len = 0, entry = lp->cur_rx % rxsize, nframe = 0,
	    rx_work_limit = *budget;
	unsigned int ioaddr = dev->base_addr;
	dma_desc *drx = lp->dma_rx + entry;

#ifdef STMMAC_RX_DEBUG
	printk(">>> stmmac_poll: RX descriptor ring:\n");
	display_dma_desc_ring(lp->dma_rx, rxsize);
#endif

	if (rx_work_limit > dev->quota)
		rx_work_limit = dev->quota;

	while (!(drx->des0 & OWN_BIT)) {
		unsigned int status = drx->des0;

		if (--rx_work_limit < 0) {
			RX_DBG("\twork limit!!!\n");
			goto not_done;
		}

		if (unlikely(lp->mac->check_rx_summary(&lp->stats, status) < 0)) {
			lp->stats.rx_errors++;
		} else {
			struct sk_buff *skb;

			/* frame_len is the length in bytes (omitting the FCS) */
			frame_len = (((status & RDES0_STATUS_FL_MASK) >>
				      RDES0_STATUS_FL_SHIFT) - 4);

			RX_DBG
			    ("\tquota %d, desc addr: 0x%0x [entry: %d] buff=0x%x\n",
			     rx_work_limit, (unsigned int)drx, entry,
			     drx->des2);

			/* Check if the packet is long enough to accept without
			   copying to a minimally-sized skbuff. */
			if ((frame_len < rx_copybreak) &&
			    (skb = dev_alloc_skb(frame_len + 2)) != NULL) {
				skb->dev = dev;
				skb_reserve(skb, NET_IP_ALIGN);
				dma_sync_single_for_cpu(lp->device,
						lp->rx_skbuff_dma[entry], frame_len,
						DMA_FROM_DEVICE);
				skb_copy_to_linear_data(skb, lp->rx_skbuff[entry]->data, 
							frame_len);

				skb_put(skb, frame_len);
				dma_sync_single_for_device(lp->device,
						   lp->rx_skbuff_dma[entry], frame_len,
						   DMA_FROM_DEVICE);
			} else {	/* zero-copy */
				skb = lp->rx_skbuff[entry];
				if (unlikely(!skb)) {
					printk(KERN_ERR "%s: Inconsistent Rx "
					       "descriptor chain.\n",
					       dev->name);
					lp->stats.rx_dropped++;
					goto next_frame;
				}
				lp->rx_skbuff[entry] = NULL;
				skb_put(skb, frame_len);
				dma_unmap_single(lp->device,
						 lp->rx_skbuff_dma[entry],
						 frame_len, DMA_FROM_DEVICE);
			}
#ifdef STMMAC_DEBUG
			if (netif_msg_pktdata(lp)) {
				printk(KERN_DEBUG " - frame received: ");
				print_pkt(skb->data, frame_len);
			}
#endif
			skb->protocol = eth_type_trans(skb, dev);
			lp->mac->rx_checksum(skb, status);
			netif_receive_skb(skb);
			lp->stats.rx_packets++;
			lp->stats.rx_bytes += frame_len;
			dev->last_rx = jiffies;
			nframe++;
		}
	      next_frame:
		drx->des0 = OWN_BIT;
		entry = (++lp->cur_rx) % rxsize;
		drx = lp->dma_rx + entry;
	}

	dev->quota -= nframe;
	*budget -= nframe;
	stmmac_rx_refill(dev);
	__netif_rx_complete(dev);
	RX_DBG("<<< stmmmac_poll: poll stopped and exits...\n");
	stmmac_dma_enable_irq_rx(ioaddr);

	return 0;

      not_done:
	RX_DBG("<<< stmmmac_poll: not done... \n");
	dev->quota -= nframe;
	*budget -= nframe;
	stmmac_rx_refill(dev);
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
void stmmac_tx_timeout(struct net_device *dev)
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
	stmmac_dma_stop_tx(dev->base_addr);
	clear_dma_descs(lp->dma_tx, lp->dma_tx_size, 0);
	stmmac_dma_start_tx(dev->base_addr);
	spin_unlock(&lp->tx_lock);

	lp->stats.tx_errors++;
	dev->trans_start = jiffies;
	netif_wake_queue(dev);

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
int stmmac_config(struct net_device *dev, struct ifmap *map)
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
	lp->mac->set_filter(dev);

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

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
static void stmmac_vlan_rx_register(struct net_device *dev,
				    struct vlan_group *grp)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	lp->vlgrp = grp;
	stmmac_multicast_list(dev);
	spin_unlock(&lp->lock);
}

static void stmmac_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	stmmac_multicast_list(dev);
	spin_unlock(&lp->lock);
}

static void stmmac_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	if (lp->vlgrp)
		lp->vlgrp->vlan_devices[vid] = NULL;
	stmmac_multicast_list(dev);
	spin_unlock(&lp->lock);
}
#endif

/**
 *  stmmac_probe - Initialization of the adapter .
 *  @dev : device pointer
 *  @ioaddr: device I/O address
 *  Description: The function initializes the network device structure for
 *	         the STMMAC driver. It also calls the low level routines 
 *		 in order to init the HW (i.e. the DMA engine)
 */
static int stmmac_probe(struct net_device *dev, unsigned long ioaddr)
{
	int ret = 0;
	struct eth_driver_local *lp = netdev_priv(dev);

	ether_setup(dev);

	dev->open = stmmac_open;
	dev->stop = stmmac_release;
	dev->set_config = stmmac_config;

	dev->hard_start_xmit = stmmac_xmit;
	dev->features |= (NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_HIGHDMA);

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
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	dev->features |= NETIF_F_HW_VLAN_RX | NETIF_F_HW_VLAN_FILTER;
	dev->vlan_rx_register = stmmac_vlan_rx_register;
	dev->vlan_rx_add_vid = stmmac_vlan_rx_add_vid;
	dev->vlan_rx_kill_vid = stmmac_vlan_rx_kill_vid;
#endif

	lp->msg_enable = netif_msg_init(debug, default_msg_level);
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	lp->ip_header_len = VLAN_ETH_HLEN;
#else
	lp->ip_header_len = ETH_HLEN;
#endif
	lp->rx_csum = 0;

	lp->dma_tx_size = dma_tx_size_param;
	lp->dma_rx_size = dma_rx_size_param;

	dev->poll = stmmac_poll;
	dev->weight = lp->dma_rx_size;

	/* Check the module arguments */
	stmmac_check_mod_params(dev);

	/* Set the I/O base addr */
	dev->base_addr = ioaddr;

	/* Get the MAC address */
	get_mac_address(ioaddr, dev->dev_addr);

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
 * stmmac_hw_dev_register
 * @lp : driver local structure
 * Description: it inits the hw mac device function pointers 
 *		within the local network driver structure.
 */
static int stmmac_hw_dev_register(struct eth_driver_local *lp)
{
	struct stmmmac_driver *mac = &mac_driver;

	printk(KERN_DEBUG "stmmmac: %s device\n", mac->name);

	lp->mac = kmalloc(sizeof(struct stmmmac_driver), GFP_KERNEL);
	lp->mac->name = mac->name;
	lp->mac->have_hw_fix = mac->have_hw_fix;
	lp->mac->core_init = mac->core_init;
	lp->mac->mac_registers = mac->mac_registers;
	lp->mac->dma_registers = mac->dma_registers;
	lp->mac->check_tx_summary = mac->check_tx_summary;
	lp->mac->check_rx_summary = mac->check_rx_summary;
	lp->mac->tx_checksum = mac->tx_checksum;
	lp->mac->rx_checksum = mac->rx_checksum;
	lp->mac->set_filter = mac->set_filter;

	return 0;
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
 * @lp: pointer to local context
 * Description: Scans through all the PHYs we have registered and checks if
 *              any are associated with our MAC.  If so, then just fill in
 *              the blanks in our local context structure
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

	/* override with kernel parameters if supplied XXX CRS XXX this needs to have multiple instances */
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
	plat_dat = (struct plat_stmmacenet_data *)((pdev->dev).platform_data);
	lp->bus_id = plat_dat->bus_id;
	lp->pbl = plat_dat->pbl;

	platform_set_drvdata(pdev, ndev);

	/* Network Device Registration */
	ret = stmmac_probe(ndev, (unsigned long)addr);
	if (ret < 0) {
		goto out;
	}

	stmmac_hw_dev_register(lp);

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
static int stmmac_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	unsigned long flags;
	struct eth_driver_local *lp = netdev_priv(dev);

	if (!dev || !netif_running(dev))
		return 0;

	netif_device_detach(dev);
	netif_stop_queue(dev);

	spin_lock_irqsave(&lp->lock, flags);

	/* Disable Rx and Tx */
	stmmac_dma_stop_tx(dev->base_addr);
	stmmac_dma_stop_rx(dev->base_addr);

	clear_dma_descs(lp->dma_tx, lp->dma_tx_size, 0);
	clear_dma_descs(lp->dma_rx, lp->dma_rx_size, OWN_BIT);

	/* Disable the MAC core */
	stmmac_mac_disable_tx(dev);
	stmmac_mac_disable_rx(dev);

	spin_unlock_irqrestore(&lp->lock, flags);

	return 0;
}

static int stmmac_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	unsigned long flags;
	struct eth_driver_local *lp = netdev_priv(dev);

	if (!netif_running(dev))
		return 0;

	netif_device_attach(dev);

	spin_lock_irqsave(&lp->lock, flags);

	/* Enable the MAC/DMA */
	stmmac_mac_enable_rx(dev);
	stmmac_mac_enable_tx(dev);

	stmmac_dma_start_rx(dev->base_addr);
	stmmac_dma_start_tx(dev->base_addr);

	netif_start_queue(dev);

	spin_unlock_irqrestore(&lp->lock, flags);

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

static void stmmac_check_mod_params(struct net_device *dev)
{
	/* Wrong parameters are replaced with the default values */
	if (watchdog < 0)
		watchdog = TX_TIMEO;
	if (dma_buffer_size > DMA_MAX_BUFFER_SIZE)
		dma_buffer_size = DMA_MAX_BUFFER_SIZE;
	if (rx_copybreak < 0)
		rx_copybreak = ETH_FRAME_LEN;
	if (dma_rx_size_param < 0)
		dma_rx_size_param = DMA_RX_SIZE;
	if (dma_tx_size_param < 0)
		dma_tx_size_param = DMA_TX_SIZE;
	return;
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
		} else if (!strncmp(opt, "bfsize:", 7)) {
			dma_buffer_size = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "txsize:", 7)) {
			dma_tx_size_param = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "rxsize:", 7)) {
			dma_rx_size_param = simple_strtoul(opt + 7, NULL, 0);
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
