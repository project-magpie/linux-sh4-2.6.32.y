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

static void gmac_dma_ttc(unsigned long ioaddr, int value)
{
	unsigned int csr6;
	/* Store and Forward capability is not used.
	 * The transmit threshold can be programmed by
	 * setting the TTC bits in the DMA control register.*/
	csr6 = (unsigned int)readl(ioaddr + DMA_CONTROL);

	if (value <= 32)
		csr6 |= DMA_CONTROL_TTC_32;
	else if (value <= 64)
		csr6 |= DMA_CONTROL_TTC_64;
	else if (value <= 128)
		csr6 |= DMA_CONTROL_TTC_128;
	else if (value <= 192)
		csr6 |= DMA_CONTROL_TTC_192;
	else
		csr6 |= DMA_CONTROL_TTC_256;

	writel(csr6, ioaddr + DMA_CONTROL);

	return;


	return;
}

static void gmac_dma_registers(unsigned long ioaddr)
{
	int i;
	printk(KERN_INFO " DMA registers\n");
	for (i = 0; i < 9; i++) {
		if ((i < 9) || (i > 17)) {
			int offset = i * 4;
			printk(KERN_INFO "\t Reg No. %d (offset 0x%x): 0x%08x\n", 
				i, (DMA_BUS_MODE + offset),
			       readl(ioaddr + DMA_BUS_MODE + offset));
		}
	}
	return;
}

static int gmac_tx_hw_error(void *p, struct stmmac_extra_stats *x,
			unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if (unlikely(status & TDES0_STATUS_DF)) {
		x->tx_deferred++;
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_VLAN)) {
		x->tx_vlan++;
		ret = -1;
	}
	if (unlikely(status & TDES0_STATUS_ES)) {
		if (unlikely(status & TDES0_STATUS_JT))
			x->tx_jabber++;
		if (unlikely(status & TDES0_STATUS_FF))
			x->tx_frame_flushed++;
		if (unlikely(status & TDES0_STATUS_LOSS_CARRIER))
			x->tx_losscarrier++;
		if (status & TDES0_STATUS_NO_CARRIER)
			x->tx_carrier++;
		if (status & TDES0_STATUS_LATE_COL) {
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		if (status & TDES0_STATUS_EX_COL) {
			stats->collisions +=
			    ((status & TDES0_STATUS_COLCNT_MASK) >>
			     TDES0_STATUS_COLCNT_SHIFT);
		}
		if (status & TDES0_STATUS_EX_DEF)
			x->tx_deferred++;
		if (status & TDES0_STATUS_UF)
			x->tx_underflow++;
		ret = -1;
	}

	return (ret);
}

static int gmac_rx_hw_error(void *p, struct stmmac_extra_stats *x,
			    unsigned int status)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)p;

	if (unlikely(status & RDES0_STATUS_DE)){
		x->rx_desc++;
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_OE)){
		x->rx_overflow++;
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_LC)) {
		stats->collisions++;
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_RWT)) {
		x->rx_watchdog++;
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_RE)) {
		x->rx_mii++;
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_CE)) {
		x->rx_crc++;
		stats->rx_crc_errors++;
	}

	if (unlikely(status & RDES0_STATUS_FILTER_FAIL)) {
		x->rx_filter++;
		ret = -1;
	}
	if (unlikely(status & RDES0_STATUS_LENGTH_ERROR)) {
		x->rx_lenght++;
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
	/* IPC verification (To be reviewed)*/
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

	/* Set the GMAC control register with our default value */
	value = (unsigned int)readl(ioaddr + GMAC_CONTROL);
	value |= GMAC_CORE_INIT;
	writel(value, ioaddr + GMAC_CONTROL);

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	writel(ETH_P_8021Q, ioaddr + GMAC_VLAN);
#endif
	return;
}

static void gmac_set_filter(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	unsigned int value = 0;

	if (dev->flags & IFF_PROMISC) {
		value = GMAC_FRAME_FILTER_PR;
	} else if ((dev->mc_count > HASH_TABLE_SIZE)
		   || (dev->flags & IFF_ALLMULTI)) {
		value = GMAC_FRAME_FILTER_PM; /// pass all multi
		writel(0xffffffff, ioaddr + GMAC_HASH_HIGH);
		writel(0xffffffff, ioaddr + GMAC_HASH_LOW);
	} else if (dev->mc_count == 0) {
		value = GMAC_FRAME_FILTER_HUC;
	} else {		/* Store the addresses in the multicast HW filter */
		int i;
		u32 mc_filter[2];
		struct dev_mc_list *mclist;

		/* Perfect filter mode for physical address and Hash
		   filter for multicast */
		value = GMAC_FRAME_FILTER_HMC;

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
		writel(mc_filter[0], ioaddr + GMAC_HASH_LOW);
		writel(mc_filter[1], ioaddr + GMAC_HASH_HIGH);
	}

	writel(value, ioaddr + GMAC_FRAME_FILTER);

	printk(KERN_DEBUG "%s: GMAC frame filter reg: 0x%08x - Hash regs: "
		"HI 0x%08x, LO 0x%08x\n",
		__FUNCTION__, readl(ioaddr + GMAC_FRAME_FILTER),
		readl(ioaddr + GMAC_HASH_HIGH), readl(ioaddr + GMAC_HASH_LOW));
	return;
}

static void gmac_flow_ctrl(unsigned long ioaddr, unsigned int duplex,
			   unsigned int fc, unsigned int pause_time)
{
	unsigned int flow = 0;

	if (fc & FLOW_RX)
		flow |= GMAC_FLOW_CTRL_RFE;
	if (fc & FLOW_TX)
		flow |= GMAC_FLOW_CTRL_TFE;

	if (duplex)
		flow |= (pause_time << GMAC_FLOW_CTRL_PT_SHIFT);
	writel(flow, ioaddr + GMAC_FLOW_CTRL);
	return;
}

static void gmac_enable_wol(unsigned long ioaddr, unsigned long mode)
{
	/* To be reviewed! */
	unsigned int pmt = 0x1; /* PWR_DOWN bit */

	if (mode == WAKE_MAGIC)
		pmt |= 0x2;

	writel(pmt, ioaddr + MAC_PMT);
	return;
}

struct device_ops gmac_driver = {
	.core_init = gmac_core_init,
	.mac_registers = gmac_mac_registers,
	.dma_registers = gmac_dma_registers,
	.dma_ttc = gmac_dma_ttc,
	.tx_err = gmac_tx_hw_error,
	.rx_err = gmac_rx_hw_error,
	.tx_checksum = gmac_tx_checksum,
	.rx_checksum = gmac_rx_checksum,
	.set_filter = gmac_set_filter,
	.flow_ctrl = gmac_flow_ctrl,
	.enable_wol = gmac_enable_wol,
};

struct device_info_t *gmac_setup(unsigned long ioaddr)
{
	struct device_info_t *mac;
	unsigned int id;
	id = (unsigned int)readl(ioaddr + GMAC_VERSION);

	printk(KERN_INFO "\tGMAC - user ID: 0x%x, Synopsys ID: 0x%x\n",
		((id & 0x0000ff00)>>8), (id & 0x000000ff));

	mac = kmalloc(sizeof(const struct device_info_t), GFP_KERNEL);
	memset(mac, 0, sizeof(struct device_info_t));

	mac->ops = &gmac_driver;
	mac->hw.pmt = PMT_SUPPORTED;
	mac->hw.addr_high = GMAC_ADDR_HIGH;
	mac->hw.addr_low = GMAC_ADDR_LOW;
	mac->hw.link.port = GMAC_CONTROL_PS;
	mac->hw.link.duplex = GMAC_CONTROL_DM;
	mac->hw.link.speed = GMAC_CONTROL_FES;
	mac->hw.mii.addr = GMAC_MII_ADDR;
	mac->hw.mii.data = GMAC_MII_DATA;

	return mac;
}
