/* ============================================================================
 *  #####  ####### #     #
 * #     #    #    ##   ##  This is a driver for the STM on-chip
 * #          #    # # # #  Ethernet controller currently present on STb7109.
 *  #####     #    #  #  #
 *       #    #    #     #  Copyright (C) 2006 by STMicroelectronics
 * #     #    #    #     #  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *  #####     #    #     #
 * ----------------------------------------------------------------------------
 * Driver internals:
 *	The driver is initialized through the platform_device mechanism.
 *	Structures which define the configuration needed by the board
 *	are defined in a internal structure.
 *
 *	The STMMAC Ethernet Controller uses two ring of buffer
 * 	descriptors for handling the transmission and the reception processes.
 *
 *	Receive process:
 *  		When one or more packets are received, an interrupt happens.
 *  		The interrupts are not queued so the driver must scan all the
 *		descriptors in the ring before exiting to the interrupt handler.
 *  		In NAPI, the interrupt handler will signal there is work to be
 *		done, and exit. The poll method will be called at some future
 *		point. Without NAPI, the packet(s) will be handled at once
 *		(at interrupt time).
 *  		The incoming packets are stored, by the DMA, in a list of
 *  		pre-allocated socket buffers in order to avoid, for big packet,
 *		the memcpy operation. The min_rx_pkt_size module parameter can
 *		be used for tuning the size of the frame copied via memcpy.
 *
 *	Transmit process:
 *		The xmit function is invoked when the kernel needs to transmit
 *	        a packet.
 *  		The transmit skb is copied in the socket buffer list.
 *		Then the relative descriptor fields in the DMA tx ring is set.
 *		Finally, the driver informs the DMA engine that there is
 *		a packet ready to be transmitted.
 *		Once the controller's finished transmitting the packet,
 *		an interrupt is triggered. So the driver will be
 *  		able to releases the socket buffers previously allocated.
 *
 *		Zero-Copy support:
 *			When the driver sets the NETIF_F_SG bit in the features
 *			field of the net_device structure it enables
 *			the scatter/gather feature.
 *			The kernel doesn't perform any scatter/gather I/O to
 *			the driver if it doesn't provide some form of
 *			checksumming as well. Unfortunately, our hardware
 *			is not able to verify the csum.
 *			The driver is able to verify the CS and handle the
 *			scatter/gather by itself (zero-copy implementation).
 *			NOTE: The scatter/gather can be enabled/disabled using:
 *				ethtool -K <eth> sg on/off
 *
 * ----------------------------------------------------------------------------
 * Kernel Command line arguments:
 *	stmmaceth=msglvl:<debug_msg_level>,phyaddr:<phy_address>,
 *		  watchdog:<watchdog_value>,rxsize:<min_rx_pkt_size>,
 *		  bfsize:<dma_buffer_size>,txqueue:<tx_queue_size>
 *	where:
 *	  - <debug_msg_level>: message level (0: no output, 16:  all).
 *	  - <phy_address>: physical Address number.
 *	  - <watchdog>:  transmit timeout (in milliseconds).
 *	  - <pause_time>: flow-control pause time (0-65535).
 *	  - <min_rx_pkt_size>: copy only tiny-frames.
 *	  - <dma_buffer_size>: DMA buffer size
 *	  - <tx_queue_size>: transmit queue size.
 * ----------------------------------------------------------------------------
 * Changelog:
 *   July 2007:
 *   	-  Moved the DMA initialization from the probe to the open method.
 *   	-  Reviewed the ioctl method.
 *   May 2007:
 *   	-  Fixed Tx timeout function and csum calculation in the xmit method
 *	-  Added fixes for NAPI, RX tasklet and multicast
 *		Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *	-  Updated phy id mask to use kernel 2.6.17 method
 *		Carl Shaw <carl.shaw@st.com>
 *   February 2007:
 *	-  Reviewed the tasklet initialization and fixed a bug the close method.
 *	-  Added a new module parameter in order to tune the DMA buffer size.
 *   January 2007:
 *	-  Reviewed the receive process:
 *	   if the Rx buffer count is above a max threshold then we need to
 *	   reallocate the buffers from the interrupt handler itself,
 *	   else schedule a tasklet to reallocate the buffers.
 *   November 2006:
 *	- Reviewed the Multicast support.
 *	- Fixed the rx csum.
 *	- Reviewed the driver function comments.
 *   September 2006:
 *	- Reviewed the transmit function.
 *      - Added the TCP Segmentation Offload (TSO) support.
 *      - Rewritten the command line parser function.
 *   August 2006:
 *   	- Converted to new platform_driver device
 *   		Carl Shaw <carl.shaw@st.com>
 *   July 2006:
 *	- Reviewed the receive process (zero-copy).
 *	- First implementation of the scatter/gather for the transmit function.
 *	- Fixed the PBL field in the DMA CSR0 register in according to the
 *	  SYSCFG7 register configuration.
 *	- Downstream/Upstream checksum offloading.
 *	- Added the Ethtool Rx/Tx csum, SG get/set support.
 *   June 2006:
 *	- Reviewed and improved the transmit algorithm.
 *	- Added the NAPI support (as experimental code).
 * 	- Added a new debug option. Now during the kernel configuration phase
 * 	  you can enable the complete debug level for the driver
 *	  (including the debug messages for the critical functions i.e. the
 *	  interrupt handler).
 *   May 2006:
 * 	- Separated out the PHY code.
 *   April 2006:
 *	- Partially removed the STe101p MII interface code.
 *	- The DMA Rx/Tx functions has been re-written.
 *	- Removed the PnSEGADDR and PHYSADDR SH4 macros and added the
 *	  Dynamic DMA mapping support.
 *	- Removed the DMA buffer size parameter. It has been fixed to the
 *	  maximum value. Moreover, the driver will only use a single buffer
 *	  in the DMA because an ethernet frame can be stored in it.
 *	- Fixed some part in the ethtool support.
 *	- Added the 802.1q VLAN support.
 *   March 2006:
 * 	- First release of the driver.
 * ----------------------------------------------------------------------------
 * Known bugs and limits:
 *	- The two-level VLAN tag is not supported yet.
 *	- The NETPOLL support is not fully tested.
 * ---
 *	https://bugzilla.stlinux.com
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

/* Generic defines */
#define RESOURCE_NAME	"stmmaceth"
#ifdef CONFIG_STMMAC_DEBUG
#define ETHPRINTK(nlevel, klevel, fmt, args...) \
		(void)(netif_msg_##nlevel(lp) && \
		printk(KERN_##klevel fmt, ## args))
#else
#define ETHPRINTK(nlevel, klevel, fmt, args...)  do { } while(0)
#endif				/*CONFIG_STMMAC_DEBUG */

/* It enables the more debug information in the transmit function */
#undef STMMAC_XMIT_DEBUG
#ifdef STMMAC_XMIT_DEBUG
#define XMITPRINTK(mss, klevel, fmt, args...) \
		if (mss!=0)	\
		printk(KERN_##klevel fmt, ## args)
#else
#define XMITPRINTK(mss, klevel, fmt, args...)  do { } while(0)
#endif

#define DMA_BUFFER_SIZE 0x7ff
#define TDES1_MAX_BUF1_SIZE ((DMA_BUFFER_SIZE << DES1_RBS1_SIZE_SHIFT) & \
			DES1_RBS1_SIZE_MASK);
#define TDES1_MAX_BUF2_SIZE ((DMA_BUFFER_SIZE << DES1_RBS2_SIZE_SHIFT) & \
			DES1_RBS2_SIZE_MASK);
#define MIN_MTU 46
#define MAX_MTU ETH_DATA_LEN
#define HASH_TABLE_SIZE 64

#undef STMMAC_TASKLET
#define RX_BUFF_THRESHOLD (CONFIG_DMA_RX_SIZE-4)

/* This structure is common for both receive and transmit DMA descriptors.
 * A descriptor should not be used for storing more than one frame. */
struct dma_desc_t {
	unsigned int des0;	/* Status */
	unsigned int des1;	/* Ctrl bits, Buffer 2 length, Buffer 1 length */
	unsigned int des2;	/* Buffer 1 */
	unsigned int des3;	/* Buffer 2 */
};

typedef struct dma_desc_t dma_desc;

struct eth_driver_local {
	int bus_id;
	int phy_addr;
	int phy_irq;
	int phy_mask;
	phy_interface_t phy_interface;
	int (*phy_reset)(void *priv);
	void (*fix_mac_speed)(void *priv, unsigned int speed);
	void (*hw_setup)(void);
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

	dma_desc *dma_tx;	/* virtual DMA TX addr */
	dma_addr_t dma_tx_phy;	/* bus DMA TX addr */
	unsigned int cur_tx, dirty_tx;	/* Producer/consumer ring indices */
	struct sk_buff *tx_skbuff[CONFIG_DMA_TX_SIZE];

	dma_desc *dma_rx;	/* virtual DMA RX addr */
	dma_addr_t dma_rx_phy;	/* bus DMA RX addr */
	int dma_buf_sz;
	unsigned int rx_buff;	/* it contains the last rx buf owned by
				   the DMA */
	int rx_csum;
	unsigned int cur_rx, dirty_rx;	/* Producer/consumer ring indices */
	/* The addresses of receive-in-place skbuffs. */
	struct sk_buff *rx_skbuff[CONFIG_DMA_RX_SIZE];
	dma_addr_t rx_skbuff_dma[CONFIG_DMA_RX_SIZE];

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	struct vlan_group *vlgrp;
#endif
	struct device *device;
	struct tasklet_struct tx_task;
	struct tasklet_struct rx_task;
	unsigned int rx_count;
};

/* Module Arguments */
#define TX_TIMEO (5*HZ)
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit Timeout (in milliseconds)");

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, S_IRUGO);
MODULE_PARM_DESC(debug, "Message Level (0: no output, 16: all)");

#define MAX_PAUSE_TIME (MAC_FLOW_CONTROL_PT_MASK>>MAC_FLOW_CONTROL_PT_SHIFT)
static int pause_time = MAX_PAUSE_TIME;
module_param(pause_time, int, S_IRUGO);
MODULE_PARM_DESC(pause_time, "Pause Time (0-65535)");

static int min_rx_pkt_size = ETH_FRAME_LEN;     /* Use memcpy by default */;
module_param(min_rx_pkt_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(min_rx_pkt_size, "Copy only tiny-frames");

static int phy_n = -1;
module_param(phy_n, int, S_IRUGO);
MODULE_PARM_DESC(phy_n, "Physical device address");

static int dma_buffer_size = DMA_BUFFER_SIZE;
module_param(dma_buffer_size, int, S_IRUGO);
MODULE_PARM_DESC(dma_buffer_size, "DMA buffer size");

static int tx_queue_size = 1;
module_param(tx_queue_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_queue_size, "transmit queue size");

static const char version[] = "stmmaceth - (C) 2006-2007 STMicroelectronics\n";

static const u32 default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_LINK | NETIF_MSG_IFUP |
				      NETIF_MSG_IFDOWN | NETIF_MSG_TIMER);

static irqreturn_t stmmaceth_interrupt(int irq, void *dev_id);
#ifndef CONFIG_STMMAC_NAPI
static __inline__ int stmmaceth_rx(struct net_device *dev);
#else
static int stmmaceth_poll(struct net_device *dev, int *budget);
#endif

static void stmmaceth_check_mod_params(struct net_device *dev)
{
	if (watchdog < 0) {
		watchdog = TX_TIMEO;
		printk(KERN_WARNING "\tWARNING: invalid tx timeout "
		       "(default is %d)\n", watchdog);
	}
	if (pause_time < 0) {
		pause_time = MAX_PAUSE_TIME;
		printk(KERN_WARNING "\tWARNING: invalid pause value"
		       "(default is %d)\n", pause_time);
	}
	if (dma_buffer_size > DMA_BUFFER_SIZE) {
		dma_buffer_size = DMA_BUFFER_SIZE;
		printk(KERN_WARNING "\tWARNING: invalid DMA buffer size "
		       "(default is %d)\n", dma_buffer_size);
	}
	if (min_rx_pkt_size < 0) {
		min_rx_pkt_size = 0;
		printk(KERN_WARNING "\tWARNING: invalid RX size (set to 0)\n");
	}
	return;
}

static inline void print_mac_addr(u8 addr[6])
{
	int i;
	for (i = 0; i < 5; i++)
		printk("%2.2x:", addr[i]);
	printk("%2.2x\n", addr[5]);
	return;
}

#ifdef CONFIG_STMMAC_DEBUG
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

/* ----------------------------------------------------------------------------
				PHY Support
   ---------------------------------------------------------------------------*/
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

	ETHPRINTK(probe, DEBUG,
		  "stmmac_adjust_link: called.  address %d link %d\n",
		  phydev->addr, phydev->link);

	spin_lock_irqsave(&lp->lock, flags);
	if (phydev->link) {
		unsigned int flow =
		    (unsigned int)readl(ioaddr + MAC_FLOW_CONTROL);
		unsigned int ctrl = (unsigned int)readl(ioaddr + MAC_CONTROL);

		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (phydev->duplex != lp->oldduplex) {
			new_state = 1;
			if (!(phydev->duplex)) {
				flow &=
				    ~(MAC_FLOW_CONTROL_FCE |
				      MAC_FLOW_CONTROL_PT_MASK |
				      MAC_FLOW_CONTROL_PCF);
				ctrl &= ~MAC_CONTROL_F;
				ctrl |= MAC_CONTROL_DRO;
			} else {
				flow |=
				    MAC_FLOW_CONTROL_FCE | MAC_FLOW_CONTROL_PCF
				    | (pause_time << MAC_FLOW_CONTROL_PT_SHIFT);
				ctrl |= MAC_CONTROL_F;
				ctrl &= ~MAC_CONTROL_DRO;
			}

			lp->oldduplex = phydev->duplex;
		}

		if (phydev->speed != lp->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case 100:
			case 10:
				lp->fix_mac_speed(lp->bsp_priv, phydev->speed);
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

		writel(flow, ioaddr + MAC_FLOW_CONTROL);
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

	ETHPRINTK(probe, DEBUG, "stmmac_adjust_link: exiting\n");
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
	ETHPRINTK(probe, DEBUG, "stmmac_init_phy:  trying to attach to %s\n",
		  phy_id);

	phydev = phy_connect(dev, phy_id, &stmmac_adjust_link, 0, lp->phy_interface);

	if (IS_ERR(phydev)) {
		printk(KERN_ERR "%s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	ETHPRINTK(probe, DEBUG,
		  "stmmac_init_phy:  %s: attached to PHY. Link = %d\n",
		  dev->name, phydev->link);

	lp->hw_setup();

	lp->phydev = phydev;

	return 0;
}

/* ----------------------------------------------------------------------------
				MDIO Bus Support
   ---------------------------------------------------------------------------*/

/**
 * stmmac_mdio_read
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * Description: it reads data from the MII register from within the phy device.
 */
int stmmac_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	unsigned long ioaddr = ndev->base_addr;
	int data;
	u16 regValue = (((phyaddr << 11) & (0x0000F800)) |
			((phyreg << 6) & (0x000007C0)));

	while (((readl(ioaddr + MAC_MII_ADDR)) & MAC_MII_ADDR_BUSY) == 1) {
	}

	writel(regValue, ioaddr + MAC_MII_ADDR);

	while (((readl(ioaddr + MAC_MII_ADDR)) & MAC_MII_ADDR_BUSY) == 1) {
	}

	/* Read the data from the MII data register */
	data = (int)readl(ioaddr + MAC_MII_DATA);
	return data;
}

/**
 * stmmac_mdio_write
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * @phydata: phy data
 * Description: it writes the data intto the MII register from within the device.
 */
int stmmac_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg, u16 phydata)
{
	struct net_device *ndev = bus->priv;
	unsigned long ioaddr = ndev->base_addr;

	u16 value =
	    (((phyaddr << 11) & (0x0000F800)) | ((phyreg << 6) & (0x000007C0)))
	    | MAC_MII_ADDR_WRITE;

	/* Wait until any existing MII operation is complete */
	while (((readl(ioaddr + MAC_MII_ADDR)) & MAC_MII_ADDR_BUSY) == 1) {
	}

	/* Set the MII address register to write */
	writel(phydata, ioaddr + MAC_MII_DATA);
	writel(value, ioaddr + MAC_MII_ADDR);

	/* Wait until any existing MII operation is complete */
	while (((readl(ioaddr + MAC_MII_ADDR)) & MAC_MII_ADDR_BUSY) == 1) {
	}

	/* NOTE: we need to perform this "extra" read in order to fix an error
	 * during the write operation */
	stmmac_mdio_read(bus, phyaddr, phyreg);
	return 0;
}

/* Resets the MII bus */
int stmmac_mdio_reset(struct mii_bus *bus)
{
	struct net_device *ndev = bus->priv;
	struct eth_driver_local *lp = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;

	if (lp->phy_reset)
		return lp->phy_reset(lp->bsp_priv);

	/* This is a workaround for problems with the STE101P PHY.
	 * It doesn't complete its reset until at least one clock cycle
	 * on MDC, so perform a dummy mdio read.
	 */
	writel(0, ioaddr + MAC_MII_ADDR);

	return 0;
}

/**
 * stmmac_mdio_register
 * @lp: local driver structure
 * @ndev : device pointer
 * @ioaddr: device I/O address
 * Description: it registers the MII bus
 */
int stmmac_mdio_register(struct eth_driver_local *lp, struct net_device *ndev)
{
	int err = 0;
	struct mii_bus *new_bus = kzalloc(sizeof(struct mii_bus), GFP_KERNEL);
	int *irqlist = kzalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);

	if (new_bus == NULL)
		return -ENOMEM;

	/* Assign IRQ to phy at address phy_addr */
	irqlist[lp->phy_addr] = lp->phy_irq;

	new_bus->name = "STMMAC MII Bus",
	    new_bus->read = &stmmac_mdio_read,
	    new_bus->write = &stmmac_mdio_write,
	    new_bus->reset = &stmmac_mdio_reset,
	    new_bus->id = (int)lp->bus_id;
	new_bus->priv = ndev;
	new_bus->irq = irqlist;
	new_bus->phy_mask = lp->phy_mask;
	new_bus->dev = 0; /* FIXME */

	err = mdiobus_register(new_bus);

	if (err != 0) {
		printk(KERN_ERR "%s: Cannot register as MDIO bus\n",
		       new_bus->name);
		goto bus_register_fail;
	}

	lp->mii = new_bus;
	return 0;

bus_register_fail:
	kfree(new_bus);
	return err;
}

/**
 * stmmac_mdio_unregister
 * @lp: local driver structure
 * Description: it unregisters the MII bus
 */
int stmmac_mdio_unregister(struct eth_driver_local *lp)
{
	mdiobus_unregister(lp->mii);
	lp->mii->priv = NULL;
	kfree(lp->mii);

	return 0;
}

/* ----------------------------------------------------------------------------
				 MAC CORE Interface
   ---------------------------------------------------------------------------*/
/**
 * dump_stm_mac_csr
 * @ioaddr: device I/O address
 * Description: the function prints the MAC CSR registers
 */
static inline void dump_stm_mac_csr(unsigned long ioaddr)
{
	printk("\t----------------------------------------------\n"
	       "\t  MAC CSR (base addr = 0x%8x)\n"
	       "\t----------------------------------------------\n",
	       (unsigned int)ioaddr);
	printk("\tcontrol reg (offset 0x%x): 0x%lx\n", MAC_CONTROL,
	       readl(ioaddr + MAC_CONTROL));
	printk("\taddr HI (offset 0x%x): 0x%lx\n ", MAC_ADDR_HIGH,
	       readl(ioaddr + MAC_ADDR_HIGH));
	printk("\taddr LO (offset 0x%x): 0x%lx\n", MAC_ADDR_LOW,
	       readl(ioaddr + MAC_ADDR_LOW));
	printk("\tmulticast hash HI (offset 0x%x): 0x%lx\n", MAC_HASH_HIGH,
	       readl(ioaddr + MAC_HASH_HIGH));
	printk("\tmulticast hash LO (offset 0x%x): 0x%lx\n", MAC_HASH_LOW,
	       readl(ioaddr + MAC_HASH_LOW));
	printk("\tflow control (offset 0x%x): 0x%lx\n", MAC_FLOW_CONTROL,
	       readl(ioaddr + MAC_FLOW_CONTROL));
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	printk("\tVLAN1 tag (offset 0x%x): 0x%lx\n", MAC_VLAN1,
	       readl(ioaddr + MAC_VLAN1));
	printk("\tVLAN2 tag (offset 0x%x): 0x%lx\n", MAC_VLAN2,
	       readl(ioaddr + MAC_VLAN2));
#endif
	printk("\tmac wakeup frame (offset 0x%x): 0x%lx\n", MAC_WAKEUP_FILTER,
	       readl(ioaddr + MAC_WAKEUP_FILTER));
	printk("\tmac wakeup crtl (offset 0x%x): 0x%lx\n",
	       MAC_WAKEUP_CONTROL_STATUS,
	       readl(ioaddr + MAC_WAKEUP_CONTROL_STATUS));

	printk("\n\tMAC management counter registers\n");
	printk("\t MMC crtl (offset 0x%x): 0x%lx\n",
	       MMC_CONTROL, readl(ioaddr + MMC_CONTROL));
	printk("\t MMC High Interrupt (offset 0x%x): 0x%lx\n",
	       MMC_HIGH_INTR, readl(ioaddr + MMC_HIGH_INTR));
	printk("\t MMC Low Interrupt (offset 0x%x): 0x%lx\n",
	       MMC_LOW_INTR, readl(ioaddr + MMC_LOW_INTR));
	printk("\t MMC High Interrupt Mask (offset 0x%x): 0x%lx\n",
	       MMC_HIGH_INTR_MASK, readl(ioaddr + MMC_HIGH_INTR_MASK));
	printk("\t MMC Low Interrupt Mask (offset 0x%x): 0x%lx\n",
	       MMC_LOW_INTR_MASK, readl(ioaddr + MMC_LOW_INTR_MASK));
	return;
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
 * stmmaceth_mac_enable_rx
 * @dev: net device structure
 * Description: the function enables the RX MAC process
 */
static void stmmaceth_mac_enable_rx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	/* set the RE (receive enable, bit 2) */
	value |= (MAC_CONTROL_RE);
	writel(value, ioaddr + MAC_CONTROL);
	return;
}

/**
 * stmmaceth_mac_enable_rx
 * @dev: net device structure
 * Description: the function enables the TX MAC process
 */
static void stmmaceth_mac_enable_tx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	/* set: TE (transmitter enable, bit 3) */
	value |= (MAC_CONTROL_TE);
	writel(value, ioaddr + MAC_CONTROL);
	return;
}

/**
 * stmmaceth_mac_disable_rx
 * @dev: net device structure
 * Description: the function disables the RX MAC process
 */
static void stmmaceth_mac_disable_rx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	value &= ~MAC_CONTROL_RE;
	writel(value, ioaddr + MAC_CONTROL);
	return;
}

/**
 * stmmaceth_mac_disable_tx
 * @dev: net device structure
 * Description: the function disables the TX MAC process
 */
static void stmmaceth_mac_disable_tx(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	value &= ~(MAC_CONTROL_TE);
	writel(value, ioaddr + MAC_CONTROL);
	return;
}

/**
 * stmmaceth_mac_core_init
 * @dev: net device structure
 * Description:  This function provides the initial setup of the MAC controller
 */
static void stmmaceth_mac_core_init(struct net_device *dev)
{
	unsigned int value = 0;
	unsigned long ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Set the MAC control register with our default value */
	value = (unsigned int)readl(ioaddr + MAC_CONTROL);
	value |= MAC_CONTROL_HBD | MAC_CONTROL_ASTP;
	writel(value, ioaddr + MAC_CONTROL);

	/* Change the MAX_FRAME bits in the MMC control register. */
	value = dev->mtu + lp->ip_header_len + 4 /*fsc */ ;
	writel(((value << MMC_CONTROL_MAX_FRM_SHIFT) &
		MMC_CONTROL_MAX_FRM_MASK), dev->base_addr + MMC_CONTROL);

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	writel(ETH_P_8021Q, dev->base_addr + MAC_VLAN1);
#endif
	return;
}

/* ----------------------------------------------------------------------------
 *  			DESCRIPTORS functions
 * ---------------------------------------------------------------------------*/
static void display_dma_desc_ring(dma_desc * p, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		printk("\t%d [0x%x]: "
		       "desc0=0x%x desc1=0x%x buffer1=0x%x", i,
		       (unsigned int)virt_to_bus(&p[i].des0), p[i].des0,
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
 * init_dma_desc_rings - init RX/TX descriptor rings
 * @dev: net device structure
 * Description:  this function initializes the DMA RX/TX descriptors
 */
static void init_dma_desc_rings(struct net_device *dev)
{
	int i;
	struct eth_driver_local *lp = netdev_priv(dev);
	lp->dma_buf_sz = dma_buffer_size;

	ETHPRINTK(probe, DEBUG, "%s: allocate and init the DMA RX/TX lists\n",
		  RESOURCE_NAME);

	lp->dma_rx = (dma_desc *) dma_alloc_coherent(lp->device,
						     CONFIG_DMA_RX_SIZE *
						     sizeof(struct dma_desc_t),
						     &lp->dma_rx_phy,
						     GFP_KERNEL);
	lp->dma_tx =
	    (dma_desc *) dma_alloc_coherent(lp->device,
					    CONFIG_DMA_TX_SIZE *
					    sizeof(struct dma_desc_t),
					    &lp->dma_tx_phy, GFP_KERNEL);

	if ((lp->dma_rx == NULL) || (lp->dma_tx == NULL)) {
		printk(KERN_ERR "%s:ERROR allocating the DMA Tx/Rx desc\n",
		       __FUNCTION__);
		return;
	}
	ETHPRINTK(probe, DEBUG, "%s: DMA desc rings: virt addr (Rx 0x%08x, "
		  "Tx 0x%08x) DMA phy addr (Rx 0x%08x,Tx 0x%08x)\n",
		  dev->name, (unsigned int)lp->dma_rx, (unsigned int)lp->dma_tx,
		  (unsigned int)lp->dma_rx_phy, (unsigned int)lp->dma_tx_phy);

	/* ---- RX INITIALIZATION */
	ETHPRINTK(probe, DEBUG, "[RX skb data]   [DMA RX skb data] "
		  "(buff size: %d)\n", lp->dma_buf_sz);
	for (i = 0; i < CONFIG_DMA_RX_SIZE; i++) {
		dma_desc *p = lp->dma_rx + i;
		struct sk_buff *skb = dev_alloc_skb(lp->dma_buf_sz);
		skb->dev = dev;
		skb_reserve(skb, NET_IP_ALIGN);
		lp->rx_skbuff[i] = skb;
		if (unlikely(skb == NULL))
			break;
		lp->rx_skbuff_dma[i] = dma_map_single(lp->device, skb->data,
						      lp->dma_buf_sz,
						      DMA_FROM_DEVICE);
		p->des2 = lp->rx_skbuff_dma[i];
		ETHPRINTK(probe, DEBUG, "[0x%08x]\t[0x%08x]\n",
			  (unsigned int)lp->rx_skbuff[i]->data,
			  (unsigned int)lp->rx_skbuff_dma[i]);
	}
	lp->cur_rx = 0;
	lp->rx_count = 0;
	lp->dirty_rx = (unsigned int)(i - CONFIG_DMA_RX_SIZE);

	/* ---- TX INITIALIZATION */
	for (i = 0; i < CONFIG_DMA_TX_SIZE; i++) {
		lp->tx_skbuff[i] = NULL;
		lp->dma_tx[i].des2 = 0;
		lp->dma_tx[i].des3 = 0;
	}
	lp->dirty_tx = lp->cur_tx = 0;

	/* Clear the R/T descriptors 0/1 */
	clear_dma_descs(lp->dma_rx, CONFIG_DMA_RX_SIZE, OWN_BIT);
	clear_dma_descs(lp->dma_tx, CONFIG_DMA_TX_SIZE, 0);

	if (netif_msg_hw(lp)) {
		printk("RX descriptor ring:\n");
		display_dma_desc_ring(lp->dma_rx, CONFIG_DMA_RX_SIZE);
		printk("TX descriptor ring:\n");
		display_dma_desc_ring(lp->dma_tx, CONFIG_DMA_TX_SIZE);
	}
	return;
}

/**
 * dma_free_rx_bufs
 * @dev: net device structure
 * Description:  this function frees all the skbuffs in the Rx queue
 */
static void dma_free_rx_bufs(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < CONFIG_DMA_RX_SIZE; i++) {
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
 * dma_free_tx_bufs
 * @dev: net device structure
 * Description:  this function frees all the skbuffs in the Tx queue
 */
static void dma_free_tx_bufs(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < CONFIG_DMA_TX_SIZE; i++) {
		if (lp->tx_skbuff[i] != NULL) {
			if ((lp->dma_tx + i)->des2) {
				dma_unmap_single(lp->device, p->des2,
						 (p->des1 & DES1_RBS1_SIZE_MASK) >>
						 DES1_RBS1_SIZE_SHIFT,
						 DMA_TO_DEVICE);
			}
			if ((lp->dma_tx + i)->des3) {
				dma_unmap_single(lp->device, p->des3,
						 (p->des1 & DES1_RBS2_SIZE_MASK) >>
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
 * Description:  this function releases and free the DMA descriptor resources
 */
static void free_dma_desc_resources(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Release the DMA TX/RX socket buffers */
	dma_free_rx_bufs(dev);
	dma_free_tx_bufs(dev);

	/* Release the TX/RX rings */
	dma_free_coherent(lp->device,
			  CONFIG_DMA_TX_SIZE * sizeof(struct dma_desc_t),
			  lp->dma_tx, lp->dma_tx_phy);
	dma_free_coherent(lp->device,
			  CONFIG_DMA_RX_SIZE * sizeof(struct dma_desc_t),
			  lp->dma_rx, lp->dma_rx_phy);
	return;
}

/* ----------------------------------------------------------------------------
				DMA FUNCTIONS
 * ---------------------------------------------------------------------------*/
/**
 * dump_dma_csr
 * @ioaddr: device I/O address
 * Description:  this function dumps the STMAC DMA registers
 */
static inline void dump_dma_csr(unsigned long ioaddr)
{
	int i;
	printk("\t--------------------\n"
	       "\t   STMMAC DMA CSR \n" "\t--------------------\n");
	for (i = 0; i < 9; i++) {
		printk("\t CSR%d (offset 0x%x): 0x%lx\n", i,
		       (DMA_BUS_MODE + i * 4),
		       readl(ioaddr + DMA_BUS_MODE + i * 4));
	}
	printk("\t CSR20 (offset 0x%x): 0x%lx\n",
	       DMA_CUR_TX_BUF_ADDR, readl(ioaddr + DMA_CUR_TX_BUF_ADDR));
	printk("\t CSR21 (offset 0x%x): 0x%lx\n",
	       DMA_CUR_RX_BUF_ADDR, readl(ioaddr + DMA_CUR_RX_BUF_ADDR));
	return;
}

/**
 * stmmaceth_dma_reset - STMAC DMA SW reset
 * @ioaddr: device I/O address
 * Description:  this function performs the DMA SW reset.
 *  NOTE1: the MII_TxClk and the MII_RxClk must be active before this
 *	   SW reset otherwise the MAC core won't exit the reset state.
 *  NOTE2: after a SW reset all interrupts are disabled
 */
static void stmmaceth_dma_reset(unsigned long ioaddr)
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
 * stmmaceth_dma_start_tx
 * @ioaddr: device I/O address
 * Description:  this function starts the DMA tx process
 */
static void stmmaceth_dma_start_tx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CONTROL);
	return;
}

static void stmmaceth_dma_stop_tx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CONTROL);
	return;
}

/**
 * stmmaceth_dma_start_rx
 * @ioaddr: device I/O address
 * Description:  this function starts the DMA rx process
 * If the NAPI support is on this function also enables the RX IRQ.
 */
static void stmmaceth_dma_start_rx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value |= DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CONTROL);

	return;
}

static void stmmaceth_dma_stop_rx(unsigned long ioaddr)
{
	unsigned int value;
	value = (unsigned int)readl(ioaddr + DMA_CONTROL);
	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CONTROL);

	return;
}

#ifdef CONFIG_STMMAC_NAPI
static __inline__ void stmmaceth_dma_enable_irq_rx(unsigned long ioaddr)
{
	unsigned int value;

	value = (unsigned int)readl(ioaddr + DMA_INTR_ENA);
	writel((value | DMA_INTR_ENA_RIE), ioaddr + DMA_INTR_ENA);
	return;
}

static __inline__ void stmmaceth_dma_disable_irq_rx(unsigned long ioaddr)
{
	unsigned int value;

	value = (unsigned int)readl(ioaddr + DMA_INTR_ENA);
	writel((value & ~DMA_INTR_ENA_RIE), ioaddr + DMA_INTR_ENA);
	return;
}
#endif

/**
 * stmmaceth_dma_init - DMA init function
 * @dev: net device structure
 * Description: the DMA init function performs:
 * - the DMA RX/TX SW descriptors initialization
 * - the DMA HW controller initialization
 * NOTE: the DMA TX/RX processes will be started in the 'open' method.
 */
static int stmmaceth_dma_init(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);

	ETHPRINTK(probe, DEBUG, "STMMAC: DMA Core setup\n");

	/* DMA SW reset */
	stmmaceth_dma_reset(ioaddr);

	/* Enable Application Access by writing to DMA CSR0 */
	ETHPRINTK(probe, DEBUG, "\t(PBL: %d)\n", lp->pbl);
	writel(DMA_BUS_MODE_DEFAULT | ((lp->pbl) << DMA_BUS_MODE_PBL_SHIFT),
	       ioaddr + DMA_BUS_MODE);

	/* Mask interrupts by writing to CSR7 */
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_INTR_ENA);

	/* The base address of the RX/TX descriptor lists must be written into
	 * DMA CSR3 and CSR4, respectively. */
	writel((unsigned long)lp->dma_tx_phy, ioaddr + DMA_TX_BASE_ADDR);
	writel((unsigned long)lp->dma_rx_phy, ioaddr + DMA_RCV_BASE_ADDR);

	if (netif_msg_hw(lp))
		dump_dma_csr(ioaddr);

	return (0);
}

#ifdef CONFIG_STMMAC_DEBUG
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
 * check_tx_error_summary
 * @lp: local network structure
 * @entry: current ring entry point
 * Description: when the transmission is completed the frame status is written
 * into TDESC0 of the descriptor having the LS bit set.
 * This function returns zero if no error is happened during the transmission.
 */
static int check_tx_error_summary(struct eth_driver_local *lp, int entry)
{
	dma_desc *p = lp->dma_tx + entry;
	int status = p->des0;

	ETHPRINTK(intr, INFO, "%s: [0x%x] - status %s\n", __FUNCTION__,
		  (unsigned int)p, (!status) ? "done" : "with error");

	if (unlikely(status & TDES0_STATUS_ES)) {
		ETHPRINTK(tx_err, ERR, "%s: DMA tx ERROR: ", RESOURCE_NAME);

		if (status & TDES0_STATUS_UF) {
			ETHPRINTK(tx_err, ERR, "Underflow Error\n");
			lp->stats.tx_fifo_errors++;
			goto out_error;
		}
		if (status & TDES0_STATUS_EX_DEF) {
			ETHPRINTK(tx_err, ERR, "Ex Deferrals\n");
			goto set_collision;
		}
		if (status & TDES0_STATUS_EX_COL) {
			ETHPRINTK(tx_err, ERR, "Ex Collisions\n");
			goto set_collision;
		}
		if (status & TDES0_STATUS_LATE_COL) {
			ETHPRINTK(tx_err, ERR, "Late Collision\n");
			goto set_collision;
		}
		if (status & TDES0_STATUS_NO_CARRIER) {
			ETHPRINTK(tx_err, ERR, "No Carrier detected\n");
			lp->stats.tx_carrier_errors++;
			goto out_error;
		}
		if (status & TDES0_STATUS_LOSS_CARRIER) {
			ETHPRINTK(tx_err, ERR, "Loss of Carrier\n");
			goto out_error;
		}
	}

	if (unlikely(status & TDES0_STATUS_HRTBT_FAIL)) {
		ETHPRINTK(tx_err, ERR, "%s: DMA tx: Heartbeat Fail\n",
			RESOURCE_NAME);
		lp->stats.tx_heartbeat_errors++;
		goto out_error;
	}
	if (unlikely(status & TDES0_STATUS_DF)) {
		ETHPRINTK(tx_err, WARNING, "%s: transmission deferred\n",
			RESOURCE_NAME);
	}
	return (0);

      set_collision:
	lp->stats.collisions += ((status & TDES0_STATUS_COLCNT_MASK) >>
				 TDES0_STATUS_COLCNT_SHIFT);
      out_error:
	lp->stats.tx_errors++;

	return (-1);
}

/**
 * stmmaceth_clean_tx_irq
 * @data:  address of the private member of the device structure
 * Description: this is the tasklet or the bottom half of the IRQ handler.
 * The tasklet is used for freeing the TX resources.
 */
static void stmmaceth_clean_tx_irq(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct eth_driver_local *lp = netdev_priv(dev);
	int entry = lp->dirty_tx % CONFIG_DMA_TX_SIZE;

	while (lp->dirty_tx != lp->cur_tx) {
		dma_desc *p = lp->dma_tx + entry;

		if (!(p->des0 & OWN_BIT)) {
			if (p->des1 & TDES1_CONTROL_LS)
				if (!check_tx_error_summary(lp, entry))
					lp->stats.tx_packets++;

			ETHPRINTK(tx_done, INFO, "%s: (entry %d)\n",
				  __FUNCTION__, entry);
			if (p->des2) {
				dma_unmap_single(lp->device, p->des2,
						 (p->
						  des1 & DES1_RBS1_SIZE_MASK) >>
						 DES1_RBS1_SIZE_SHIFT,
						 DMA_TO_DEVICE);
				p->des2 = 0;
			}

			if (p->des3) {
				dma_unmap_single(lp->device, p->des3,
						 (p->
						  des1 & DES1_RBS2_SIZE_MASK) >>
						 DES1_RBS2_SIZE_SHIFT,
						 DMA_TO_DEVICE);
				p->des3 = 0;
			}

			if (lp->tx_skbuff[entry] != NULL) {
				dev_kfree_skb_irq(lp->tx_skbuff[entry]);
				lp->tx_skbuff[entry] = NULL;
			}
		}
		entry = (++lp->dirty_tx) % CONFIG_DMA_TX_SIZE;
	}
	if (netif_queue_stopped(dev))
		netif_wake_queue(dev);

	return;
}

/**
 * check_rx_error_summary
 * @lp: local network structure
 * @status: descriptor status field
 * Description: it checks if the frame was not successfully received
 * This function returns zero if no error is happened during the transmission.
 */
static int check_rx_error_summary(struct eth_driver_local *lp,
	                          unsigned int status)
{
	int ret = 0;
	if ((status & RDES0_STATUS_ERROR)) {
	        /* ES-Error Summary */
	        ETHPRINTK(rx_err, ERR, "stmmaceth RX:\n");
	        if (status & RDES0_STATUS_DE)
	                ETHPRINTK(rx_err, ERR, "\tdescriptor error\n");
	        if (status & RDES0_STATUS_PFE)
	                ETHPRINTK(rx_err, ERR, "\tpartial frame error\n");
	        if (status & RDES0_STATUS_RUNT_FRM)
	                ETHPRINTK(rx_err, ERR, "\trunt Frame\n");
	        if (status & RDES0_STATUS_TL)
	                ETHPRINTK(rx_err, ERR, "\tframe too long\n");
	        if (status & RDES0_STATUS_COL_SEEN) {
	                ETHPRINTK(rx_err, ERR, "\tcollision seen\n");
	                lp->stats.collisions++;
	       }
	        if (status & RDES0_STATUS_CE) {
	                ETHPRINTK(rx_err, ERR, "\tCRC Error\n");
	                lp->stats.rx_crc_errors++;
	        }

	        if (status & RDES0_STATUS_LENGTH_ERROR)
	                ETHPRINTK(rx_err, ERR, "\tLenght error\n");
	        if (status & RDES0_STATUS_MII_ERR)
	                ETHPRINTK(rx_err, ERR, "\tMII error\n");

	        lp->stats.rx_errors++;
	        ret = -1;
	}
	return (ret);
}

/**
 * stmmaceth_refill_rx_buf - refill the Rx ring buffers (zero-copy)
 * @dev: net device structure
 * Description: the function allocates Rx side skbs and puts the physical
 *  address of these buffers into the DMA buffer pointers.
 */
static void stmmaceth_refill_rx_buf(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int bsize = lp->dma_buf_sz;

	for (; lp->cur_rx - lp->dirty_rx > 0; lp->dirty_rx++) {
		struct sk_buff *skb;
		int entry = lp->dirty_rx % CONFIG_DMA_RX_SIZE;
		if (lp->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(bsize);
			lp->rx_skbuff[entry] = skb;
			if (unlikely(skb == NULL))
				break;
			skb->dev = dev;
			lp->rx_skbuff_dma[entry] =
			    dma_map_single(lp->device, skb->data,
					   bsize, DMA_FROM_DEVICE);
			(lp->dma_rx + entry)->des2 = lp->rx_skbuff_dma[entry];
			ETHPRINTK(rx_status, INFO, ">>> refill entry #%d\n",
				  entry);
		}
	}
	lp->rx_count = 0;
}

#ifdef STMMAC_TASKLET
/**
 * stmmaceth_clean_rx_irq
 * @data:  address of the private member of the device structure
 * Description: it calls the stmmaceth_refill_rx_buf in order to
 * refill the receive socket buffers.
 */
static void stmmaceth_clean_rx_irq(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	stmmaceth_refill_rx_buf(dev);
}
#endif

/**
 * stmmaceth_dma_interrupt - Interrupt handler for the STMMAC DMA
 * @dev: net device structure
 * Description: It determines if we have to call either the Rx or the Tx
 * interrupt handler.
 * Numerous events can cause an interrupt: a new packet has arrived
 * or transmission is completed or an error occurred).  */
static void stmmaceth_dma_interrupt(struct net_device *dev)
{
	unsigned int status;
	unsigned int ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);
	lp->rx_buff = readl(ioaddr + DMA_CUR_RX_BUF_ADDR);

	/* read the status register (CSR5) */
	status = (unsigned int)readl(ioaddr + DMA_STATUS);

	ETHPRINTK(intr, INFO, "%s: (%s) [CSR5: 0x%08x]\n", RESOURCE_NAME,
		  "DMA IRQ", status);
#ifdef CONFIG_STMMAC_DEBUG
	/* It displays the DMA transmit process state (CSR5 register) */
	if (netif_msg_tx_done(lp))
		show_tx_process_state(status);
	if (netif_msg_rx_status(lp))
		show_rx_process_state(status);
#endif
	/* Process the NORMAL interrupts */
	if (status & DMA_STATUS_NIS) {
		ETHPRINTK(intr, INFO, " CSR5[16]: DMA NORMAL IRQ: ");
		if (likely(status & DMA_STATUS_RI)) {
			ETHPRINTK(intr, INFO, "Receive irq [buf: 0x%08x]\n",
				  lp->rx_buff);
#ifdef CONFIG_STMMAC_NAPI
			stmmaceth_dma_disable_irq_rx(ioaddr);
			if (netif_rx_schedule_prep(dev)) {
				__netif_rx_schedule(dev);
			} else {
				ETHPRINTK(intr, ERR, "%s: bug!!! "
					  "interrupt while in poll.\n",
					  __FUNCTION__);
			}
#else
			stmmaceth_rx(dev);
#endif
		}
		if (unlikely(status & DMA_STATUS_ERI)) {
			ETHPRINTK(intr, INFO, "Early Receive Interrupt\n");
		}
		if (likely(status & DMA_STATUS_TI)) {
			ETHPRINTK(intr, INFO, " Transmit irq [buf: 0x%lx]\n",
				  readl(ioaddr + DMA_CUR_TX_BUF_ADDR));
			tasklet_schedule(&lp->tx_task);
		}
		if (unlikely(status & DMA_STATUS_TU)) {
			ETHPRINTK(intr, INFO, "Transmit Buffer Unavailable\n");
		}
	}

	/* ABNORMAL interrupts */
	if (unlikely(status & DMA_STATUS_AIS)) {
		ETHPRINTK(intr, INFO, "CSR5[15] DMA ABNORMAL IRQ: ");
		if (status & DMA_STATUS_TPS) {
			ETHPRINTK(intr, INFO, "Transmit Process Stopped \n");
		}
		if (status & DMA_STATUS_TJT) {
			ETHPRINTK(intr, INFO, "Transmit Jabber Timeout\n");
		}
		if (status & DMA_STATUS_OVF) {
			ETHPRINTK(intr, INFO, "Receive Overflow\n");
		}
		if (status & DMA_STATUS_UNF) {
			ETHPRINTK(intr, INFO, "Transmit Underflow\n");
		}
		if (status & DMA_STATUS_RU) {
			ETHPRINTK(intr, INFO, "Rx Buffer Unavailable\n");
		}
		if (status & DMA_STATUS_RPS) {
			ETHPRINTK(intr, INFO, "Receive Process Stopped\n");
		}
		if (status & DMA_STATUS_RWT) {
			ETHPRINTK(intr, INFO, "Rx Watchdog Timeout\n");
		}
		if (status & DMA_STATUS_ETI) {
			ETHPRINTK(intr, INFO, "Early Tx Interrupt\n");
		}
		if (status & DMA_STATUS_FBI) {
			ETHPRINTK(intr, INFO, "Fatal Bus Error Interrupt\n");
		}
	}
	ETHPRINTK(intr, INFO, "\n\n");

	/* Clear the interrupt by writing a logic 1 to the relative bits */
	writel(status, ioaddr + DMA_STATUS);
	return;
}

/* ----------------------------------------------------------------------------
			      DEVICE METHODS
   ---------------------------------------------------------------------------*/
/**
 *  stmmaceth_open - open entry point of the driver
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function is the open entry point of the driver.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */
int stmmaceth_open(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int ret;

	/* Request the IRQ lines */

	ret = request_irq(dev->irq, &stmmaceth_interrupt,
			  SA_SHIRQ, dev->name, dev);
	if (ret < 0) {
		printk(KERN_ERR "%s: ERROR: allocating the IRQ %d (error: %d)\n",
		       __FUNCTION__, dev->irq, ret);
		return (ret);
	}

	/* Attach the PHY */
	ret = stmmac_init_phy(dev);
	if (ret) {
		printk(KERN_ERR "%s: Cannot attach to PHY (error: %d)\n",
		       __FUNCTION__, ret);
		return (-ENODEV);
	}

	/* Create and initialize the TX/RX descriptors rings */
	init_dma_desc_rings(dev);

	/* Intialize the DMA controller and send the SW reset */
	/* This must be after we have successfully initialised the PHY
	 * (see comment in stmmaceth_dma_reset). */
	if (stmmaceth_dma_init(dev) < 0) {
		ETHPRINTK(probe, ERR, "%s: DMA initialization failed\n",
			  __FUNCTION__);
		return (-1);
	}

	/* Check that the MAC address is valid.  If its not, refuse
	 * to bring the device up. The user must specify an
	 * address using the following linux command:
	 *      ifconfig eth0 hw ether xx:xx:xx:xx:xx:xx  */

	if (!is_valid_ether_addr(dev->dev_addr)) {
		ETHPRINTK(probe, ERR, "%s: no valid eth hw addr\n",
			  __FUNCTION__);
		return (-EINVAL);
	}

	printk(KERN_INFO "stmmaceth_open: MAC address ");
	print_mac_addr(dev->dev_addr);

	/* Copy the MAC addr into the HW in case we have set it with nwhw */
	set_mac_addr(dev->base_addr, dev->dev_addr);

	/* Initialize the MAC110 Core */
	stmmaceth_mac_core_init(dev);

	/* Tasklet initialisation */
	tasklet_init(&lp->tx_task, stmmaceth_clean_tx_irq, (unsigned long)dev);
#ifdef STMMAC_TASKLET
	tasklet_init(&lp->rx_task, stmmaceth_clean_rx_irq, (unsigned long)dev);
#endif

	/* Enable the MAC/DMA */
	stmmaceth_mac_enable_rx(dev);
	stmmaceth_mac_enable_tx(dev);

	if (netif_msg_hw(lp))
		dump_stm_mac_csr((unsigned int)dev->base_addr);

	phy_start(lp->phydev);

	/* Start the ball rolling... */
	ETHPRINTK(probe, DEBUG, "%s: DMA RX/TX processes started...\n",
		  RESOURCE_NAME);

	stmmaceth_dma_start_rx(dev->base_addr);
	stmmaceth_dma_start_tx(dev->base_addr);

	netif_start_queue(dev);
	return (0);
}

/**
 *  stmmaceth_release - close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */
int stmmaceth_release(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	/* Stop the PHY */
	phy_stop(lp->phydev);
	phy_disconnect(lp->phydev);
	lp->phydev = NULL;

	/* Free the IRQ lines */
	free_irq(dev->irq, dev);

	/* Stop TX/RX DMA and clear the descriptors */
	stmmaceth_dma_stop_tx(dev->base_addr);
	stmmaceth_dma_stop_rx(dev->base_addr);

	free_dma_desc_resources(dev);

	/* Disable the MAC core */
	stmmaceth_mac_disable_tx(dev);
	stmmaceth_mac_disable_rx(dev);

	/* The tasklets won't be scheduled to run again */
	tasklet_kill(&lp->tx_task);
#ifdef STMMAC_TASKLET
	tasklet_kill(&lp->rx_task);
#endif

	/* Change the link status */
	netif_carrier_off(dev);

	return (0);
}

/**
 *  stmmaceth_fill_tx_buffer
 *  @data : data buffer
 *  @size : fragment size
 *  @mss : Maximum  Segment Size
 *  @lp : driver local structure
 *  @first : first element in the ring
 *  Description: it is used for filling the DMA tx ring with the frame to be
 *  transmitted.
 *  Note that the algorithm works both for the non-paged data and for the paged
 *  fragment (SG).
 *  It also supports the segmentation offloading for super-sized skb's
 *  (skb_shinfo(skb)->tso_size != 0).
 *  Return value:
 *    current entry point in the tx ring
 */
static int stmmaceth_fill_tx_buffer(void *data, unsigned int size,
				    unsigned int mss,
				    struct eth_driver_local *lp, int first)
{
	int new_des = 0;
	void *addr = data;
	dma_desc *p = lp->dma_tx;
	unsigned int entry;
	int bsize = lp->dma_buf_sz;

	XMITPRINTK(mss, INFO, "  %s (size=%d, addr=0x%x)\n", __FUNCTION__,
		 size, (unsigned int)addr);
	do {
		if (new_des) {
			lp->cur_tx++;
			new_des = 0;
		}
		entry = lp->cur_tx % CONFIG_DMA_TX_SIZE;
		/* Set the owner field */
		p[entry].des0 = OWN_BIT;
		/* Reset the descriptor number 1 */
		p[entry].des1 = (p[entry].des1 & DES1_CONTROL_TER);
		if (first)
			p[entry].des1 |= TDES1_CONTROL_FS;
		else
			lp->tx_skbuff[entry] = NULL;

		XMITPRINTK(mss, INFO, "\t[entry =%d] buf1 len=%d\n",
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

				XMITPRINTK(mss, INFO,
					 "\t[entry=%d] buf2 len=%d\n", entry,
					 min(b2_size, bsize));

				/* Check if we need another descriptor. */
				if (b2_size > bsize) {
					b2_size = bsize;
					size -= (2 * bsize);
					addr += ((2 * bsize) + 1);
					new_des = 1;
					XMITPRINTK(mss, INFO,
						 "\tnew descriptor - "
						 "%s (len = %d)\n",
						 (first) ? "skb->data" :
						 "Frag", size);
				}
				p[entry].des3 = dma_map_single(lp->device,
							       (buffer2 + bsize
								+ 1), b2_size,
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
	return (entry);
}

/**
 *  stmmaceth_xmit - Tx entry point of the driver
 *  @skb : the socket buffer
 *  @dev : device pointer
 *  Description :
 *  This function is the Tx entry point of the driver.
 */
int stmmaceth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	dma_desc *p = lp->dma_tx;
	unsigned int nfrags = skb_shinfo(skb)->nr_frags,
	    entry = lp->cur_tx % CONFIG_DMA_TX_SIZE, i, mss = 0, nopaged_len;

	if (skb->len < ETH_ZLEN) {
		skb = skb_padto(skb, ETH_ZLEN);
		skb->len = ETH_ZLEN;
	}

	/* Reporting an error if either the frame, to be transmitted, is too
	 * long or we haven't enough space in the DMA ring. If the following
	 * error is reported, probably, you ought to increase the ring size.*/
	if (unlikely(nfrags >= CONFIG_DMA_TX_SIZE)) {
		printk(KERN_ERR "%s: ERROR too many fragments (%d)...\n",
		       __FUNCTION__, nfrags);
		goto xmit_error;
	}
#ifdef NETIF_F_TSO
	if (dev->features & NETIF_F_TSO) {
		mss = skb_shinfo(skb)->gso_size;

		if (unlikely
		    ((skb->len > ((2 * (lp->dma_buf_sz)) * CONFIG_DMA_TX_SIZE))
		     && (mss != 0))) {
			printk(KERN_ERR "%s: (TSO) frame too long (%d)...\n",
			       __FUNCTION__, skb->len);
			goto xmit_error;
		}
	}
#endif
	/* Verify the csum via software... it's necessary because the
	 * hardware doesn't support a complete csum calculation. */
#warning or should this be CHECKSUM_PARTIAL
	if (likely(skb->ip_summed == CHECKSUM_COMPLETE)) {
		unsigned int csum;
		const int offset = skb_transport_offset(skb);

		csum = skb_checksum(skb, offset, skb->len - offset, 0);
		*(u16 *) (skb->csum_start + skb->csum_offset) = csum_fold(csum);
	}
	/* Get the amount of non-paged data (skb->data). */
	nopaged_len = skb_headlen(skb);
	lp->tx_skbuff[entry] = skb;
	XMITPRINTK(mss, INFO, "\n%s:\n(skb->len=%d, nfrags=%d, "
		 "nopaged_len=%d, mss=%d)\n", __FUNCTION__, skb->len,
		 nfrags, nopaged_len, mss);
	/* Handle the non-paged data (skb->data) */
	stmmaceth_fill_tx_buffer(skb->data, nopaged_len, mss, lp, 1);

	/* Handle the paged fragments */
	for (i = 0; i < nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		void *addr =
		    ((void *)page_address(frag->page) + frag->page_offset);
		int len = frag->size;

		lp->cur_tx++;
		entry = stmmaceth_fill_tx_buffer(addr, len, mss, lp, 0);
	}
	p[entry].des1 |= TDES1_CONTROL_LS | TDES1_CONTROL_IC;
	lp->cur_tx++;

	if ((lp->cur_tx-lp->dirty_tx) >= tx_queue_size) {
		netif_stop_queue(dev);
	}

	lp->stats.tx_bytes += skb->len;

#ifdef CONFIG_STMMAC_DEBUG
	if (netif_msg_pktdata(lp)) {
		printk(">>> (current=%d, dirty=%d; entry=%d)\n",
		       (lp->cur_tx % CONFIG_DMA_TX_SIZE),
		       (lp->dirty_tx % CONFIG_DMA_TX_SIZE), entry);
		display_dma_desc_ring(lp->dma_tx, CONFIG_DMA_TX_SIZE);
		printk(">>> frame to be transmitted: ");
		print_pkt(skb->data, skb->len);
	}
#endif
	/* CSR1 enables the transmit DMA to check for new descriptor */
	writel(1, dev->base_addr + DMA_XMT_POLL_DEMAND);

	dev->trans_start = jiffies;

	return (0);

      xmit_error:
	dev_kfree_skb(skb);
	lp->stats.tx_dropped++;
	return (0);
}

/*   If the NAPI support is enabled the stmmaceth_poll method will be
 *   scheduled at interrupt time.
 *   Otherwise, the stmmaceth_rx(...) is the receive function processed
 *   by the regular interrupt handler.
 */
#ifdef CONFIG_STMMAC_NAPI
static int stmmaceth_poll(struct net_device *dev, int *budget)
#else
static __inline__ int stmmaceth_rx(struct net_device *dev)
#endif
{
	struct eth_driver_local *lp = netdev_priv(dev);
	int frame_len = 0, entry = lp->cur_rx % CONFIG_DMA_RX_SIZE;
	dma_desc *drx = lp->dma_rx + entry;
#ifdef CONFIG_STMMAC_NAPI
	int npackets = 0, quota = min(dev->quota, *budget);
#endif
	int nframe = 0;

#ifdef CONFIG_STMMAC_DEBUG
	if (netif_msg_rx_status(lp)) {
		printk("%s: RX descriptor ring:\n", __FUNCTION__);
		display_dma_desc_ring(lp->dma_rx, CONFIG_DMA_RX_SIZE);
	}
#endif
	while (!(drx->des0 & OWN_BIT) && (nframe < CONFIG_DMA_RX_SIZE)) {
		struct sk_buff *skb;
		unsigned int status = drx->des0;
		nframe++;
#ifdef CONFIG_STMMAC_NAPI
		if (unlikely(npackets > quota)) {
			printk("%s: ERROR: out of quota (%d); npackets %d\n",
			       dev->name, quota, npackets);
			drx->des0 = OWN_BIT;
			break;
		}
#endif
		if (check_rx_error_summary(lp, status) < 0)
			goto next_frame;

		/* update multicast stats */
		if (status & RDES0_STATUS_MULTICST_FRM)
			lp->stats.multicast++;
		/* frame_len is the length in bytes (omitting the FCS) */
		frame_len = (((status & RDES0_STATUS_FL_MASK) >>
				RDES0_STATUS_FL_SHIFT) - 4);
		ETHPRINTK(rx_status, INFO,
			  ">>> desc addr: 0x%0x [entry: %d]\n"
			  "\tdesc0=0x%x desc1=0x%x buffer1=0x%x\n",
			  (unsigned int)drx, entry, drx->des0, drx->des1,
			  drx->des2);
		/* Check if the packet is long enough to accept without
		   copying to a minimally-sized skbuff. */
		if (frame_len < min_rx_pkt_size) {
			skb = dev_alloc_skb(frame_len + 2);
			if (unlikely(!skb)) {
				if (printk_ratelimit())
					printk(KERN_NOTICE "%s: low memory, "
					       "packet dropped.\n", dev->name);
				lp->stats.rx_dropped++;
				goto next_frame;
			}
			skb->dev = dev;
			skb_reserve(skb, NET_IP_ALIGN);
			dma_sync_single_for_cpu(lp->device,
						lp->rx_skbuff_dma[entry],
						lp->dma_buf_sz,
						DMA_FROM_DEVICE);
			skb_copy_to_linear_data(skb, lp->rx_skbuff[entry]->data,
					 frame_len);
			skb_put(skb, frame_len);
			dma_sync_single_for_device(lp->device,
						   lp->rx_skbuff_dma[entry],
						   lp->dma_buf_sz,
						   DMA_FROM_DEVICE);
		} else {	/* zero-copy */
			skb = lp->rx_skbuff[entry];
			if (unlikely(!skb)) {
				printk(KERN_ERR "%s: Inconsistent Rx "
					"descriptor chain.\n", dev->name);
				lp->stats.rx_dropped++;
				goto next_frame;
			}
			dma_unmap_single(lp->device, lp->rx_skbuff_dma[entry],
					 lp->dma_buf_sz, DMA_FROM_DEVICE);
			lp->rx_skbuff[entry] = NULL;
			skb_put(skb, frame_len);
			lp->rx_count++;
		}
#ifdef CONFIG_STMMAC_DEBUG
		if (netif_msg_pktdata(lp)) {
			printk(">>> frame received: ");
			print_pkt(skb->data, frame_len);
		}
#endif
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_NONE;

#ifdef CONFIG_STMMAC_NAPI
		npackets++;
		netif_receive_skb(skb);
#else
		netif_rx(skb);
#endif
		lp->stats.rx_packets++;
		lp->stats.rx_bytes += frame_len;
		dev->last_rx = jiffies;

	      next_frame:
		drx->des0 = OWN_BIT;
		entry = (++lp->cur_rx) % CONFIG_DMA_RX_SIZE;
		drx = lp->dma_rx + entry;
	}

#ifndef CONFIG_STMMAC_NAPI
#ifdef STMMAC_TASKLET
	if (lp->rx_count < RX_BUFF_THRESHOLD)
		tasklet_schedule(&lp->rx_task);
	else
#endif
#endif
		stmmaceth_refill_rx_buf(dev);

#ifdef CONFIG_STMMAC_NAPI
	/* All the packets in the DMA have been processed so we can
	 * reenable the RX interrupt. */
	*budget -= npackets;
	dev->quota -= npackets;
	netif_rx_complete(dev);
	stmmaceth_dma_enable_irq_rx(dev->base_addr);
#else
	writel(1, dev->base_addr + DMA_RCV_POLL_DEMAND);
#endif
	return 0;
}

/**
 *  stmmaceth_tx_timeout
 *  @dev : Pointer to net device structure
 *  Description: this function is called when a packet transmission fails to
 *   complete within a reasonable period. The driver will mark the error in the
 *   netdev structure and arrange for the device to be reset to a sane state
 *   in order to transmit a new packet.
 */
void stmmaceth_tx_timeout(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	printk(KERN_WARNING "%s: Tx timeout at %ld, latency %ld\n",
	       dev->name, jiffies, (jiffies - dev->trans_start));

#ifdef CONFIG_STMMAC_DEBUG
	printk("(current=%d, dirty=%d)\n", (lp->cur_tx % CONFIG_DMA_TX_SIZE),
	       (lp->dirty_tx % CONFIG_DMA_TX_SIZE));
	printk("DMA tx ring status: \n");
	display_dma_desc_ring(lp->dma_tx, CONFIG_DMA_TX_SIZE);
#endif
	netif_stop_queue(dev);
	stmmaceth_dma_stop_tx(dev->base_addr);
	clear_dma_descs(lp->dma_tx, CONFIG_DMA_TX_SIZE, 0);
	tasklet_disable(&lp->tx_task);

	tasklet_enable(&lp->tx_task);
	stmmaceth_dma_start_tx(dev->base_addr);

	lp->stats.tx_errors++;
	dev->trans_start = jiffies;
	netif_wake_queue(dev);

	return;
}

/**
 *  stmmaceth_stats
 *  @dev : Pointer to net device structure
 *  Description: this function returns statistics to the caller application
 */
struct net_device_stats *stmmaceth_stats(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	return &lp->stats;
}

/* Configuration changes (passed on by ifconfig) */
int stmmaceth_config(struct net_device *dev, struct ifmap *map)
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
	return (0);
}

/* ---------------------------------------------------------------------------
			Address Filtering Method
   ---------------------------------------------------------------------------*/
/**
 *  stmmaceth_set_rx_mode - entry point for multicast addressing
 *  @dev : pointer to the device structure
 *  Description:
 *  This function is a driver entry point which gets called by the kernel
 *  whenever multicast addresses must be enabled/disabled.
 *  Return value:
 *  void.
 */
static void stmmaceth_set_rx_mode(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	struct eth_driver_local *lp = netdev_priv(dev);
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	if (dev->flags & IFF_PROMISC) {
		value |= MAC_CONTROL_PR;
		value &=
		    ~(MAC_CONTROL_PM | MAC_CONTROL_IF | MAC_CONTROL_HO |
		      MAC_CONTROL_HP);
	} else if ((dev->mc_count > HASH_TABLE_SIZE)
		   || (dev->flags & IFF_ALLMULTI)) {
		value |= MAC_CONTROL_PM;
		value &= ~(MAC_CONTROL_PR | MAC_CONTROL_IF | MAC_CONTROL_HO);
		writel(0xffffffff, ioaddr + MAC_HASH_HIGH);
		writel(0xffffffff, ioaddr + MAC_HASH_LOW);
	} else if (dev->mc_count == 0) {	/* Just get our own stuff .. no multicast?? */
		value &=
		    ~(MAC_CONTROL_PM | MAC_CONTROL_PR | MAC_CONTROL_IF |
		      MAC_CONTROL_HO | MAC_CONTROL_HP);
	} else {		/* Store the addresses in the multicast HW filter */
		int i;
		u32 mc_filter[2];
		struct dev_mc_list *mclist;

		/* Perfect filter mode for physical address and Hash
		   filter for multicast */
		value |= MAC_CONTROL_HP;
		value &= ~(MAC_CONTROL_PM | MAC_CONTROL_PR | MAC_CONTROL_IF
				| MAC_CONTROL_HO);

		memset(mc_filter, 0, sizeof(mc_filter));
		for (i = 0, mclist = dev->mc_list;
		     mclist && i < dev->mc_count; i++, mclist = mclist->next) {
			/* The upper 6 bits of the calculated CRC are used to index
			   the contens of the hash table */
			int bit_nr =
			    ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;
			/* The most significant bit determines the register to use
			   (H/L) while the other 5 bits determine the bit within
			   the register. */
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		}
		writel(mc_filter[0], ioaddr + MAC_HASH_LOW);
		writel(mc_filter[1], ioaddr + MAC_HASH_HIGH);
	}

	writel(value, ioaddr + MAC_CONTROL);

	if (netif_msg_hw(lp)) {
		printk("%s: CTRL reg: 0x%lx - Hash regs: HI 0x%lx, LO 0x%lx\n",
		       __FUNCTION__, readl(ioaddr + MAC_CONTROL),
		       readl(ioaddr + MAC_HASH_HIGH),
		       readl(ioaddr + MAC_HASH_LOW));
	}
	return;
}

/**
 *  stmmaceth_change_mtu - entry point to change MTU size for the device.
 *   @dev : device pointer.
 *   @new_mtu : the new MTU size for the device.
 *   Description: the Maximum Transfer Unit (MTU) is used by the network layer to
 *     drive packet transmission. Ethernet has an MTU of 1500 octets (ETH_DATA_LEN).
 *     This value can be changed with ifconfig.
 *  Return value:
 *   0 on success and an appropriate (-)ve integer as defined in errno.h
 *   file on failure.
 */
static int stmmaceth_change_mtu(struct net_device *dev, int new_mtu)
{
	if (netif_running(dev)) {
		printk(KERN_ERR "%s: must be stopped to change its MTU\n",
		       dev->name);
		return -EBUSY;
	}

	if ((new_mtu < MIN_MTU) || (new_mtu > MAX_MTU))
		return -EINVAL;

	dev->mtu = new_mtu;

	return (0);
}

/* ---------------------------------------------------------------------------
			REGULAR INTERRUPT FUNCTION
   ---------------------------------------------------------------------------*/
static irqreturn_t stmmaceth_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;

	if (unlikely(!dev)) {
		printk(KERN_ERR "%s: invalid dev pointer\n", __FUNCTION__);
		return IRQ_NONE;
	}

	stmmaceth_dma_interrupt(dev);

	return IRQ_HANDLED;
}

/* ---------------------------------------------------------------------------
		  		NETPOLL SUPPORT
   ---------------------------------------------------------------------------*/
#ifdef CONFIG_NET_POLL_CONTROLLER
/* Polling receive - used by NETCONSOLE and other diagnostic tools
 * to allow network I/O with interrupts disabled. */
static void stmmaceth_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	stmmaceth_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

/* ----------------------------------------------------------------------------
		  		ETHTOOL SUPPORT
   ---------------------------------------------------------------------------*/
static void stmmaceth_ethtool_getdrvinfo(struct net_device *dev,
					 struct ethtool_drvinfo *info)
{
	strcpy(info->driver, RESOURCE_NAME);
	strncpy(info->version, version, sizeof(version));
	strcpy(info->bus_info, "STBUS");
	info->fw_version[0] = '\0';
	return;
}

static int stmmaceth_ethtool_getsettings(struct net_device *dev,
					 struct ethtool_cmd *cmd)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	struct phy_device *phy = lp->phydev;

	if (phy == NULL) {
		printk(KERN_ERR
		       "%s: ethtool_getsettings PHY is not registered\n",
		       dev->name);
		return -ENODEV;
	}

	if (!netif_running(dev)) {
		printk(KERN_ERR "%s: interface is disabled: we cannot track "
		       "link speed / duplex setting\n", dev->name);
		return -EBUSY;
	}

	cmd->transceiver = XCVR_INTERNAL;
	return phy_ethtool_gset(phy, cmd);
}

static int stmmaceth_ethtool_setsettings(struct net_device *dev,
					 struct ethtool_cmd *cmd)
{
	struct eth_driver_local *lp = dev->priv;
	struct phy_device *phy = lp->phydev;

	return phy_ethtool_sset(phy, cmd);
}

static u32 stmmaceth_ethtool_getmsglevel(struct net_device *dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	return lp->msg_enable;
}

static void stmmaceth_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct eth_driver_local *lp = netdev_priv(dev);
	lp->msg_enable = level;

}

static int stmmaceth_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;
	return (0);
}

#define REGDUMP_LEN         (32 * 1024)
int stmmaceth_ethtool_get_regs_len(struct net_device *dev)
{
	return (REGDUMP_LEN);
}

static void stmmaceth_ethtool_gregs(struct net_device *dev,
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

int stmmaceth_ethtool_set_tx_csum(struct net_device *dev, u32 data)
{
	if (data)
		dev->features |= NETIF_F_HW_CSUM;
	else
		dev->features &= ~NETIF_F_HW_CSUM;

	return 0;
}

u32 stmmaceth_ethtool_get_rx_csum(struct net_device * dev)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	return (lp->rx_csum);
}

int stmmaceth_ethtool_set_rx_csum(struct net_device *dev, u32 data)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	if (data)
		lp->rx_csum = 1;
	else
		lp->rx_csum = 0;

	return 0;
}

static struct ethtool_ops stmmaceth_ethtool_ops = {
	.begin = stmmaceth_check_if_running,
	.get_drvinfo = stmmaceth_ethtool_getdrvinfo,
	.get_settings = stmmaceth_ethtool_getsettings,
	.set_settings = stmmaceth_ethtool_setsettings,
	.get_msglevel = stmmaceth_ethtool_getmsglevel,
	.set_msglevel = stmmaceth_ethtool_setmsglevel,
	.get_regs = stmmaceth_ethtool_gregs,
	.get_regs_len = stmmaceth_ethtool_get_regs_len,
	.get_link = ethtool_op_get_link,
	.get_rx_csum = stmmaceth_ethtool_get_rx_csum,
	.set_rx_csum = stmmaceth_ethtool_set_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = stmmaceth_ethtool_set_tx_csum,
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso = ethtool_op_get_tso,
	.set_tso = ethtool_op_set_tso,
#endif
	.get_ufo = ethtool_op_get_ufo,
	.set_ufo = ethtool_op_set_ufo,

};

/* ----------------------------------------------------------------------------
		    		IOCTL SUPPORT
   ---------------------------------------------------------------------------*/
/**
 *  stmmaceth_ioctl - Entry point for the Ioctl
 *  @dev :  Device pointer.
 *  @rq :  An IOCTL specefic structure, that can contain a pointer to
 *  a proprietary structure used to pass information to the driver.
 *  @cmd :  IOCTL command
 *  Description:
 *  Currently there are no special functionality supported in IOCTL, just the
 *  phy_mii_ioctl (it changes the PHY reg. without regard to current state).
 */
static int stmmaceth_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	if (!netif_running(dev))
		return -EINVAL;

	switch(cmd) {
        case SIOCGMIIPHY:
        case SIOCGMIIREG:
        case SIOCSMIIREG:
		if (!lp->phydev)
			return -EINVAL;

		return phy_mii_ioctl(lp->phydev, if_mii(rq), cmd);
	default:
		/* do nothing */
		break;
        }
        return -EOPNOTSUPP;
}

/* ----------------------------------------------------------------------------
		    		VLAN SUPPORT
   ---------------------------------------------------------------------------*/
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
static void stmmaceth_vlan_rx_register(struct net_device *dev,
				       struct vlan_group *grp)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	lp->vlgrp = grp;
	stmmaceth_set_rx_mode(dev);
	spin_unlock(&lp->lock);
}

static void stmmaceth_vlan_rx_add_vid(struct net_device *dev,
				      unsigned short vid)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	stmmaceth_set_rx_mode(dev);
	spin_unlock(&lp->lock);
}

static void stmmaceth_vlan_rx_kill_vid(struct net_device *dev,
				       unsigned short vid)
{
	struct eth_driver_local *lp = netdev_priv(dev);

	spin_lock(&lp->lock);
	if (lp->vlgrp)
		lp->vlgrp->vlan_devices[vid] = NULL;
	stmmaceth_set_rx_mode(dev);
	spin_unlock(&lp->lock);
}
#endif

/* ----------------------------------------------------------------------------
		   DEVICE REGISTRATION, INITIALIZATION AND UNLOADING
   ---------------------------------------------------------------------------*/
/**
 *  stmmaceth_probe - Initialization of the adapter .
 *  @dev : device pointer
 *  @ioaddr: device I/O address
 *  Description: The function initializes the network device structure for
 *	         the STMMAC driver. It also calls the low level routines
 *		 in order to init the HW (i.e. the DMA engine)
 */
static int stmmaceth_probe(struct net_device *dev, unsigned long ioaddr)
{
	int ret = 0;
	struct eth_driver_local *lp = netdev_priv(dev);

	ether_setup(dev);

	dev->open = stmmaceth_open;
	dev->stop = stmmaceth_release;
	dev->set_config = stmmaceth_config;

	dev->hard_start_xmit = stmmaceth_xmit;
	dev->features |= (NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_HIGHDMA);

	dev->get_stats = stmmaceth_stats;
	dev->tx_timeout = stmmaceth_tx_timeout;
	dev->watchdog_timeo = msecs_to_jiffies(watchdog);
	dev->set_multicast_list = stmmaceth_set_rx_mode;
	dev->change_mtu = stmmaceth_change_mtu;
	dev->ethtool_ops = &stmmaceth_ethtool_ops;
	dev->do_ioctl = &stmmaceth_ioctl;
#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = stmmaceth_poll_controller;
#endif
#ifdef CONFIG_STMMAC_NAPI
	dev->poll = stmmaceth_poll;
	dev->weight = CONFIG_DMA_RX_SIZE;
#endif

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	dev->features |= NETIF_F_HW_VLAN_RX | NETIF_F_HW_VLAN_FILTER;
	dev->vlan_rx_register = stmmaceth_vlan_rx_register;
	dev->vlan_rx_add_vid = stmmaceth_vlan_rx_add_vid;
	dev->vlan_rx_kill_vid = stmmaceth_vlan_rx_kill_vid;
#endif

	lp->msg_enable = netif_msg_init(debug, default_msg_level);
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	lp->ip_header_len = VLAN_ETH_HLEN;
#else
	lp->ip_header_len = ETH_HLEN;
#endif
	lp->rx_csum = 0;

	/* Check the module arguments */
	stmmaceth_check_mod_params(dev);

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
		return (-ENODEV);
	}

	spin_lock_init(&lp->lock);

	return (ret);
}

/**
 * stmmaceth_dvr_probe
 * @pdev: platform device pointer
 * Description: The driver is initialized through platform_device.
 * 		Structures which define the configuration needed by the board
 *		are defined in a board structure in arch/sh/boards/st/ .
 */
static int stmmaceth_dvr_probe(struct platform_device *pdev)
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
				RESOURCE_NAME)) {
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
	ret = stmmaceth_probe(ndev, (unsigned long)addr);
	if (ret < 0) {
		goto out;
	}

	/* Get the PHY information */
	lp->phy_irq = platform_get_irq_byname(pdev, "phyirq");
	if ((phy_n >= 0) && (phy_n <= 31)) {
		plat_dat->phy_addr = phy_n;
	}
	lp->phy_addr = plat_dat->phy_addr;
	lp->phy_mask = plat_dat->phy_mask;
	lp->phy_interface = plat_dat->interface;
	lp->phy_reset = plat_dat->phy_reset;
	lp->fix_mac_speed = plat_dat->fix_mac_speed;
	lp->hw_setup = plat_dat->hw_setup;
	lp->bsp_priv = plat_dat->bsp_priv;

	/* MDIO bus Registration */
	ret = stmmac_mdio_register(lp, ndev);

      out:
	if (ret < 0) {
		platform_set_drvdata(pdev, NULL);
		release_mem_region(res->start, (res->end - res->start));
		if (addr != NULL)
			iounmap(addr);
	}

	return (ret);
}

/**
 * stmmaceth_dvr_remove
 * @pdev: platform device pointer
 * Description: This function performs the following:
 *   		- Reset the TX/RX processes
 *   		- Disable the MAC RX/TX
 *   		- Change the link status
 *   		- Free the DMA descriptor rings
 *   		- Unregister the MDIO bus
 *   		- unmap the memory resources
 */
static int stmmaceth_dvr_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct eth_driver_local *lp = netdev_priv(ndev);
	struct resource *res;

	printk(KERN_INFO "%s:\n\tremoving driver", __FUNCTION__);

	stmmaceth_dma_stop_rx(ndev->base_addr);
	stmmaceth_dma_stop_tx(ndev->base_addr);

	stmmaceth_mac_disable_rx(ndev);
	stmmaceth_mac_disable_tx(ndev);

	netif_carrier_off(ndev);

	stmmac_mdio_unregister(lp);

	platform_set_drvdata(pdev, NULL);
	unregister_netdev(ndev);

	iounmap((void *)ndev->base_addr);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, (res->end - res->start));

	free_netdev(ndev);

	return (0);
}

static struct platform_driver stmmaceth_driver = {
	.driver = {
		   .name = RESOURCE_NAME,
		   },
	.probe = stmmaceth_dvr_probe,
	.remove = stmmaceth_dvr_remove,
};

/**
 * stmmaceth_init_module - Entry point for the driver
 * Description: This function is the entry point for the driver.
 */
static int __init stmmaceth_init_module(void)
{
	return platform_driver_register(&stmmaceth_driver);
}

/**
 * stmmaceth_cleanup_module - Cleanup routine for the driver
 * Description: This function is the cleanup routine for the driver.
 */
static void __exit stmmaceth_cleanup_module(void)
{
	platform_driver_unregister(&stmmaceth_driver);
}

/* --------------------------------------------------------------------------
 * 		Parse the optional command line arguments
 * --------------------------------------------------------------------------*/
static int __init stmmaceth_cmdline_opt(char *str)
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
		} else if (!strncmp(opt, "pause:", 6)) {
			pause_time = simple_strtoul(opt + 6, NULL, 0);
		} else if (!strncmp(opt, "rxsize:", 7)) {
			min_rx_pkt_size = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "bfsize:", 7)) {
			dma_buffer_size = simple_strtoul(opt + 7, NULL, 0);
		} else if (!strncmp(opt, "txqueue:", 8)) {
			tx_queue_size = simple_strtoul(opt + 8, NULL, 0);
		}
	}
	return (0);
}

__setup("stmmaceth=", stmmaceth_cmdline_opt);

module_init(stmmaceth_init_module);
module_exit(stmmaceth_cleanup_module);

MODULE_DESCRIPTION("STM MAC Ethernet driver");
MODULE_AUTHOR("Giuseppe Cavallaro");
MODULE_LICENSE("GPL");
