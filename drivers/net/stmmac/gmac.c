/* 
 * drivers/net/stmmac/gmac.c
 *
 * Giga Ethernet driver
 *
 * Copyright (C) 2007 by STMicroelectronics
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/ethtool.h>
#include <asm/io.h>

#include "common.h"
#include "gmac.h"

static void gmac_mac_registers(unsigned long ioaddr)
{
	int i;
	printk("\t----------------------------------------------\n"
	       "\t  GMAC registers (base addr = 0x%8x)\n"
	       "\t----------------------------------------------\n",
	       (unsigned int)ioaddr);

	for (i = 0; i < 55; i++) {
		if ((i < 12) || (i > 15)) {
			int offset = i * 4;
			printk("\tReg No. %d (offset 0x%x): 0x%08x\n", i,
			       offset, readl(ioaddr + offset));
		}
	}
	return;
}

static void gmac_dma_registers(unsigned long ioaddr)
{
	int i;
	printk("\t--------------------\n"
	       "\t   DMA registers\n" "\t--------------------\n");
	for (i = 0; i < 9; i++) {
		if ((i < 9) || (i > 17)) {
			int offset = i * 4;
			printk("\t Reg No. %d (offset 0x%x): 0x%08x\n", i,
			       (DMA_BUS_MODE + offset),
			       readl(ioaddr + DMA_BUS_MODE + offset));
		}
	}
	return;
}

static int gmac_tx_summary(void *p, unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if (unlikely(status & TDES0_STATUS_DF)) {
		MAC_DBG(WARNING, "gmac: DMA tx: deferred error\n");
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_VLAN)) {
		MAC_DBG(WARNING, "gmac: DMA tx: VLAN frame Fails\n");
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_ES)) {
		MAC_DBG(ERR, "gmac: DMA tx ERROR: ");
		if (unlikely(status & TDES0_STATUS_JT))
			MAC_DBG(WARNING, "jabber timeout\n");
		if (unlikely(status & TDES0_STATUS_FF))
			MAC_DBG(WARNING, "frame flushed\n");
		if (unlikely(status & TDES0_STATUS_LOSS_CARRIER))
			MAC_DBG(WARNING, "Loss of Carrier\n");
		if (status & TDES0_STATUS_NO_CARRIER)
			MAC_DBG(ERR, "No Carrier\n");
		if (status & TDES0_STATUS_LATE_COL) {
			MAC_DBG(ERR, "Late Collision\n");
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		if (status & TDES0_STATUS_EX_COL) {
			MAC_DBG(ERR, "Ex Collisions\n");
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		if (status & TDES0_STATUS_EX_DEF)
			MAC_DBG(ERR, "Ex Deferrals\n");
		if (status & TDES0_STATUS_UF)
			MAC_DBG(ERR, "Underflow\n");
		ret = -1;
	}

	return (ret);
}

static int gmac_rx_summary(void *p, unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if (unlikely((status & RDES0_STATUS_ES))) {
		MAC_DBG(ERR, "gmac: DMA rx ERROR: ");
		if (unlikely(status & RDES0_STATUS_DE))
			MAC_DBG(ERR, "descriptor\n");
		if (unlikely(status & RDES0_STATUS_OE))
			MAC_DBG(ERR, "Overflow\n");
		if (unlikely(status & RDES0_STATUS_LC)) {
			MAC_DBG(ERR, "late collision\n");
			stats->collisions++;
		}
		if (unlikely(status & RDES0_STATUS_RWT))
			MAC_DBG(ERR, "watchdog timeout\n");
		if (unlikely(status & RDES0_STATUS_RE))
			MAC_DBG(ERR, "Receive Error (MII)\n");
		if (unlikely(status & RDES0_STATUS_CE)) {
			MAC_DBG(ERR, "CRC Error\n");
			stats->rx_crc_errors++;
		}
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_FILTER_FAIL)) {
		MAC_DBG(ERR, "DMA rx: DA Filtering Fails\n");
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_LENGTH_ERROR)) {
		MAC_DBG(ERR, "DMA rx: Lenght error\n");
		ret = -1;
	}
	return (ret);
}

static void gmac_tx_checksum(struct sk_buff *skb)
{
	return;
}

static void gmac_rx_checksum(struct sk_buff *skb, int status)
{
	/* IPC verification */
	if (unlikely(status & RDES0_STATUS_IPC)) {
		/* Packet with erroneous checksum, so let the
		 * upper layers deal with it.  */
		skb->ip_summed = CHECKSUM_NONE;
	} else {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	}
	return;
}

static void gmac_core_init(unsigned long ioaddr)
{
	unsigned int value = 0;

	/* Set the MAC control register with our default value */
	value = (unsigned int)readl(ioaddr + MAC_CONTROL);
	value |= MAC_CORE_INIT;
	writel(value, ioaddr + MAC_CONTROL);

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	writel(ETH_P_8021Q, ioaddr + MAC_VLAN);
#endif
	return;
}

static void gmac_set_filter(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_FRAME_FILTER);

	if (dev->flags & IFF_PROMISC) {
		value |= MAC_FRAME_FILTER_PR;
		value &= ~(MAC_FRAME_FILTER_PM);
	} else if ((dev->mc_count > HASH_TABLE_SIZE)
		   || (dev->flags & IFF_ALLMULTI)) {
		value |= MAC_FRAME_FILTER_PM;
		value &= ~(MAC_FRAME_FILTER_PR | MAC_FRAME_FILTER_DAIF);
		writel(0xffffffff, ioaddr + MAC_HASH_HIGH);
		writel(0xffffffff, ioaddr + MAC_HASH_LOW);
	} else if (dev->mc_count == 0) {
		value |= MAC_FRAME_FILTER_HUC;
		value &= ~(MAC_FRAME_FILTER_PM | MAC_FRAME_FILTER_PR |
			   MAC_FRAME_FILTER_DAIF | MAC_FRAME_FILTER_HMC);
	} else {		/* Store the addresses in the multicast HW filter */
		int i;
		u32 mc_filter[2];
		struct dev_mc_list *mclist;

		/* Perfect filter mode for physical address and Hash
		   filter for multicast */
		value |= MAC_FRAME_FILTER_HMC;
		value &= ~(MAC_FRAME_FILTER_PR | MAC_FRAME_FILTER_DAIF
			   | MAC_FRAME_FILTER_PM | MAC_FRAME_FILTER_HUC);

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
		writel(mc_filter[0], ioaddr + MAC_HASH_HIGH);
		writel(mc_filter[1], ioaddr + MAC_HASH_LOW);
	}

	writel(value, ioaddr + MAC_CONTROL);

	MAC_DBG(DEBUG,
		"%s: CTRL reg: 0x%08x - Hash regs: HI 0x%08x, LO 0x%08x\n",
		__FUNCTION__, readl(ioaddr + MAC_CONTROL),
		readl(ioaddr + MAC_HASH_HIGH), readl(ioaddr + MAC_HASH_LOW));
	return;
}

static void gmac_flow_ctrl(unsigned long ioaddr, unsigned int duplex,
			   unsigned int fc, unsigned int pause_time)
{
	unsigned int flow = 0;

	if (fc & FLOW_RX)
		flow |= MAC_FLOW_CTRL_RFE;
	if (fc & FLOW_TX)
		flow |= MAC_FLOW_CTRL_TFE;

	if (duplex) {
		MAC_DBG(INFO, "mac100: flow control (pause 0x%x)\n.",
			pause_time);
		flow |= (pause_time << MAC_FLOW_CTRL_PT_SHIFT);
	}
	writel(flow, ioaddr + MAC_FLOW_CTRL);
	return;
}

struct device_ops gmac_driver = {
	.core_init = gmac_core_init,
	.mac_registers = gmac_mac_registers,
	.dma_registers = gmac_dma_registers,
	.check_tx_summary = gmac_tx_summary,
	.check_rx_summary = gmac_rx_summary,
	.tx_checksum = gmac_tx_checksum,
	.rx_checksum = gmac_rx_checksum,
	.set_filter = gmac_set_filter,
	.flow_ctrl = gmac_flow_ctrl,
};

struct device_info_t *gmac_setup(unsigned long ioaddr)
{
	struct device_info_t *mac;
	unsigned int id;
	id = (unsigned int)readl(ioaddr + MAC_VERSION);
	id &= 0x000000ff;

	if (id != GMAC_CORE_VERSION)
		return NULL;

	mac = kmalloc(sizeof(const struct device_info_t), GFP_KERNEL);
	memset(mac, 0, sizeof(struct device_info_t));

	mac->ops = &gmac_driver;
	mac->name = "gmac";
	mac->hw.control = MAC_CONTROL;
	mac->hw.addr_high = MAC_ADDR_HIGH;
	mac->hw.addr_low = MAC_ADDR_LOW;
	mac->hw.enable_rx = MAC_CONTROL_RE;
	mac->hw.enable_tx = MAC_CONTROL_TE;
	mac->hw.link.port = MAC_CONTROL_PS;
	mac->hw.link.duplex = MAC_CONTROL_DM;
	mac->hw.link.speed = MAC_CONTROL_FES;
	mac->hw.mii.addr = MAC_MII_ADDR;
	mac->hw.mii.data = MAC_MII_DATA;
	mac->hw.mii.addr_write = MAC_MII_ADDR_WRITE;
	mac->hw.mii.addr_busy = MAC_MII_ADDR_BUSY;

	return mac;
}
