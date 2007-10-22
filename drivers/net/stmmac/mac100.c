/* 
 * drivers/net/stmmac/mac100.c
 *
 * This is a driver for the MAC 10/100 on-chip
 * Ethernet controller currently present on STb7109.
 *
 * Copyright (C) 2007 by STMicroelectronics
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
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
#include <asm/io.h>

#include "mac_hw.h"

#define HASH_TABLE_SIZE 64

#undef MAC100_DEBUG
#ifdef MAC100_DEBUG
#define DBG(klevel, fmt, args...) \
                printk(KERN_##klevel fmt, ## args)
#else
#define DBG(klevel, fmt, args...)  do { } while(0)
#endif

static void mac100_mac_registers(unsigned long ioaddr)
{
	printk("\t----------------------------------------------\n"
	       "\t  MAC100 CSR (base addr = 0x%8x)\n"
	       "\t----------------------------------------------\n",
	       (unsigned int)ioaddr);
	printk("\tcontrol reg (offset 0x%x): 0x%08x\n", MAC_CONTROL,
	       readl(ioaddr + MAC_CONTROL));
	printk("\taddr HI (offset 0x%x): 0x%08x\n ", MAC_ADDR_HIGH,
	       readl(ioaddr + MAC_ADDR_HIGH));
	printk("\taddr LO (offset 0x%x): 0x%08x\n", MAC_ADDR_LOW,
	       readl(ioaddr + MAC_ADDR_LOW));
	printk("\tmulticast hash HI (offset 0x%x): 0x%08x\n", MAC_HASH_HIGH,
	       readl(ioaddr + MAC_HASH_HIGH));
	printk("\tmulticast hash LO (offset 0x%x): 0x%08x\n", MAC_HASH_LOW,
	       readl(ioaddr + MAC_HASH_LOW));
	printk("\tflow control (offset 0x%x): 0x%08x\n", MAC_FLOW_CONTROL,
	       readl(ioaddr + MAC_FLOW_CONTROL));
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	printk("\tVLAN1 tag (offset 0x%x): 0x%08x\n", MAC_VLAN1,
	       readl(ioaddr + MAC_VLAN1));
	printk("\tVLAN2 tag (offset 0x%x): 0x%08x\n", MAC_VLAN2,
	       readl(ioaddr + MAC_VLAN2));
#endif
	printk("\tmac wakeup frame (offset 0x%x): 0x%08x\n", MAC_WAKEUP_FILTER,
	       readl(ioaddr + MAC_WAKEUP_FILTER));
	printk("\tmac wakeup crtl (offset 0x%x): 0x%08x\n",
	       MAC_WAKEUP_CONTROL_STATUS,
	       readl(ioaddr + MAC_WAKEUP_CONTROL_STATUS));

	printk("\n\tMAC management counter registers\n");
	printk("\t MMC crtl (offset 0x%x): 0x%08x\n",
	       MMC_CONTROL, readl(ioaddr + MMC_CONTROL));
	printk("\t MMC High Interrupt (offset 0x%x): 0x%08x\n",
	       MMC_HIGH_INTR, readl(ioaddr + MMC_HIGH_INTR));
	printk("\t MMC Low Interrupt (offset 0x%x): 0x%08x\n",
	       MMC_LOW_INTR, readl(ioaddr + MMC_LOW_INTR));
	printk("\t MMC High Interrupt Mask (offset 0x%x): 0x%08x\n",
	       MMC_HIGH_INTR_MASK, readl(ioaddr + MMC_HIGH_INTR_MASK));
	printk("\t MMC Low Interrupt Mask (offset 0x%x): 0x%08x\n",
	       MMC_LOW_INTR_MASK, readl(ioaddr + MMC_LOW_INTR_MASK));
	return;
}

static void mac100_dma_registers(unsigned long ioaddr)
{
	int i;
	printk("\t--------------------\n"
	       "\t   MAC100 DMA CSR \n" "\t--------------------\n");
	for (i = 0; i < 9; i++) {
		printk("\t CSR%d (offset 0x%x): 0x%08x\n", i,
		       (DMA_BUS_MODE + i * 4),
		       readl(ioaddr + DMA_BUS_MODE + i * 4));
	}
	printk("\t CSR20 (offset 0x%x): 0x%08x\n",
	       DMA_CUR_TX_BUF_ADDR, readl(ioaddr + DMA_CUR_TX_BUF_ADDR));
	printk("\t CSR21 (offset 0x%x): 0x%08x\n",
	       DMA_CUR_RX_BUF_ADDR, readl(ioaddr + DMA_CUR_RX_BUF_ADDR));
	return;
}

static int mac100_tx_summary(void *p, unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if (unlikely(status & TDES0_STATUS_ES)) {
		DBG(ERR, "mac100: DMA tx ERROR: ");
		if (status & TDES0_STATUS_UF) {
			DBG(ERR, "Underflow Error\n");
			stats->tx_fifo_errors++;
		}
		if (status & TDES0_STATUS_NO_CARRIER) {
			DBG(ERR, "No Carrier detected\n");
			stats->tx_carrier_errors++;
		}
		if (status & TDES0_STATUS_LOSS_CARRIER) {
			DBG(ERR, "Loss of Carrier\n");
		}
		if ((status & TDES0_STATUS_EX_DEF) ||
		    (status & TDES0_STATUS_EX_COL) ||
		    (status & TDES0_STATUS_LATE_COL)) {
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		ret = -1;
	}

	if (unlikely(status & TDES0_STATUS_HRTBT_FAIL)) {
		DBG(ERR, "mac100: Heartbeat Fail\n");
		stats->tx_heartbeat_errors++;
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_DF)) {
		DBG(WARNING, "mac100: tx deferred\n");
		/*ret = -1; */
	}

	return (ret);
}

/* This function verifies if the incoming frame has some errors 
 * and, if required, updates the multicast statistics. */
static int mac100_rx_summary(void *p, unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if ((status & RDES0_STATUS_ERROR)) {
		DBG(ERR, "stmmaceth RX:\n");
		if (status & RDES0_STATUS_DE)
			DBG(ERR, "\tdescriptor error\n");
		if (status & RDES0_STATUS_PFE)
			DBG(ERR, "\tpartial frame error\n");
		if (status & RDES0_STATUS_RUNT_FRM)
			DBG(ERR, "\trunt Frame\n");
		if (status & RDES0_STATUS_TL)
			DBG(ERR, "\tframe too long\n");
		if (status & RDES0_STATUS_COL_SEEN) {
			DBG(ERR, "\tcollision seen\n");
			stats->collisions++;
		}
		if (status & RDES0_STATUS_CE) {
			DBG(ERR, "\tCRC Error\n");
			stats->rx_crc_errors++;
		}

		if (status & RDES0_STATUS_LENGTH_ERROR)
			DBG(ERR, "\tLenght error\n");
		if (status & RDES0_STATUS_MII_ERR)
			DBG(ERR, "\tMII error\n");

		ret = -1;
	}

	/* update multicast stats */
	if (status & RDES0_STATUS_MULTICST_FRM)
		stats->multicast++;

	return (ret);
}

static void mac100_tx_checksum(struct sk_buff *skb)
{
	/* Verify the csum via software... it' necessary, because the
	 * hardware doesn't support a complete csum calculation. */
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		const int offset = skb_transport_offset(skb);
		unsigned int csum =
		    skb_checksum(skb, offset, skb->len - offset, 0);
		*(u16 *) (skb->data + offset + skb->csum_offset) =
		    csum_fold(csum);
	}
	return;
}

static void mac100_rx_checksum(struct sk_buff *skb, int status)
{
	skb->ip_summed = CHECKSUM_NONE;
	return;
}

static void mac100_core_init(struct net_device *dev)
{
	unsigned int value = 0;
	unsigned long ioaddr = dev->base_addr;

	DBG(DEBUG, "mac100_core_init");

	/* Set the MAC control register with our default value */
	value = (unsigned int)readl(ioaddr + MAC_CONTROL);
	writel((value | MAC_CORE_INIT), ioaddr + MAC_CONTROL);

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	writel(ETH_P_8021Q, dev->base_addr + MAC_VLAN1);
#endif
	return;
}

static void mac100_set_filter(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = (unsigned int)readl(ioaddr + MAC_CONTROL);

	if (dev->flags & IFF_PROMISC) {
		value |= MAC_CONTROL_PR;
		value &= ~(MAC_CONTROL_PM | MAC_CONTROL_IF | MAC_CONTROL_HO |
			   MAC_CONTROL_HP);
	} else if ((dev->mc_count > HASH_TABLE_SIZE)
		   || (dev->flags & IFF_ALLMULTI)) {
		value |= MAC_CONTROL_PM;
		value &= ~(MAC_CONTROL_PR | MAC_CONTROL_IF | MAC_CONTROL_HO);
		writel(0xffffffff, ioaddr + MAC_HASH_HIGH);
		writel(0xffffffff, ioaddr + MAC_HASH_LOW);
	} else if (dev->mc_count == 0) {	/* Just get our own stuff .. no multicast?? */
		value &= ~(MAC_CONTROL_PM | MAC_CONTROL_PR | MAC_CONTROL_IF |
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
			/* The upper 6 bits of the calculated CRC are used to 
			 * index the contens of the hash table */
			int bit_nr =
			    ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;
			/* The most significant bit determines the register to 
			 * use (H/L) while the other 5 bits determine the bit 
			 * within the register. */
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		}
		writel(mc_filter[0], ioaddr + MAC_HASH_LOW);
		writel(mc_filter[1], ioaddr + MAC_HASH_HIGH);
	}

	writel(value, ioaddr + MAC_CONTROL);

	DBG(DEBUG, "%s: CTRL reg: 0x%08x - Hash regs: HI 0x%08x, LO 0x%08x\n",
	    __FUNCTION__, readl(ioaddr + MAC_CONTROL),
	    readl(ioaddr + MAC_HASH_HIGH), readl(ioaddr + MAC_HASH_LOW));
	return;
}

struct stmmmac_driver mac_driver = {
	.name = "mac100",
	.have_hw_fix = 1,
	.core_init = mac100_core_init,
	.mac_registers = mac100_mac_registers,
	.dma_registers = mac100_dma_registers,
	.check_tx_summary = mac100_tx_summary,
	.check_rx_summary = mac100_rx_summary,
	.tx_checksum = mac100_tx_checksum,
	.rx_checksum = mac100_rx_checksum,
	.set_filter = mac100_set_filter,
};
