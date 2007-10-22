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

#include "mac_hw.h"

#undef GMAC_DEBUG
#ifdef GMAC_DEBUG
#define DBG(klevel, fmt, args...) \
                printk(KERN_##klevel fmt, ## args)
#else
#define DBG(klevel, fmt, args...)  do { } while(0)
#endif

#define HASH_TABLE_SIZE 64

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
		DBG(WARNING, "gmac: DMA tx: deferred error\n");
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_VLAN)) {
		DBG(WARNING, "gmac: DMA tx: VLAN frame Fails\n");
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_ES)) {
		DBG(ERR, "gmac: DMA tx ERROR: ");
		if (unlikely(status & TDES0_STATUS_JT))
			DBG(WARNING, "jabber timeout\n");
		if (unlikely(status & TDES0_STATUS_FF))
			DBG(WARNING, "frame flushed\n");
		if (unlikely(status & TDES0_STATUS_LOSS_CARRIER))
			DBG(WARNING, "Loss of Carrier\n");
		if (status & TDES0_STATUS_NO_CARRIER)
			DBG(ERR, "No Carrier\n");
		if (status & TDES0_STATUS_LATE_COL) {
			DBG(ERR, "Late Collision\n");
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		if (status & TDES0_STATUS_EX_COL) {
			DBG(ERR, "Ex Collisions\n");
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		if (status & TDES0_STATUS_EX_DEF)
			DBG(ERR, "Ex Deferrals\n");
		if (status & TDES0_STATUS_UF)
			DBG(ERR, "Underflow\n");
		ret = -1;
	}

	return (ret);
}

static int gmac_rx_summary(void *p, unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if (unlikely((status & RDES0_STATUS_ES))) {
		DBG(ERR, "gmac: DMA rx ERROR: ");
		if (unlikely(status & RDES0_STATUS_DE))
			DBG(ERR, "descriptor\n");
		if (unlikely(status & RDES0_STATUS_OE))
			DBG(ERR, "Overflow\n");
		if (unlikely(status & RDES0_STATUS_LC)) {
			DBG(ERR, "late collision\n");
			stats->collisions++;
		}
		if (unlikely(status & RDES0_STATUS_RWT))
			DBG(ERR, "watchdog timeout\n");
		if (unlikely(status & RDES0_STATUS_RE))
			DBG(ERR, "Receive Error (MII)\n");
		if (unlikely(status & RDES0_STATUS_CE)) {
			DBG(ERR, "CRC Error\n");
			stats->rx_crc_errors++;
		}
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_FILTER_FAIL)) {
		DBG(ERR, "DMA rx: DA Filtering Fails\n");
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_LENGTH_ERROR)) {
		DBG(ERR, "DMA rx: Lenght error\n");
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

static void gmac_core_init(struct net_device *dev)
{
	unsigned int value = 0;
	unsigned long ioaddr = dev->base_addr;

	/* Set the MAC control register with our default value */
	value = (unsigned int)readl(ioaddr + MAC_CONTROL);
	value |= MAC_CORE_INIT;
	writel(value, ioaddr + MAC_CONTROL);

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	writel(ETH_P_8021Q, dev->base_addr + MAC_VLAN);
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

	DBG(DEBUG, "%s: CTRL reg: 0x%08x - Hash regs: HI 0x%08x, LO 0x%08x\n",
	    __FUNCTION__, readl(ioaddr + MAC_CONTROL),
	    readl(ioaddr + MAC_HASH_HIGH), readl(ioaddr + MAC_HASH_LOW));
	return;
}

struct stmmmac_driver mac_driver = {
	.name = "gmac",
	.have_hw_fix = 0,
	.core_init = gmac_core_init,
	.mac_registers = gmac_mac_registers,
	.dma_registers = gmac_dma_registers,
	.check_tx_summary = gmac_tx_summary,
	.check_rx_summary = gmac_rx_summary,
	.tx_checksum = gmac_tx_checksum,
	.rx_checksum = gmac_rx_checksum,
	.set_filter = gmac_set_filter,
};
