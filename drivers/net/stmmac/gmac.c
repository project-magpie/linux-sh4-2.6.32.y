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

#undef GMAC_DEBUG
/*#define GMAC_DEBUG*/
#ifdef GMAC_DEBUG
#define DBG(fmt,args...)  printk(fmt, ## args)
#else
#define DBG(fmt, args...)  do { } while(0)
#endif

static void gmac_dump_regs(unsigned long ioaddr)
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

static int gmac_dma_init(unsigned long ioaddr, int pbl, u32 dma_tx, u32 dma_rx)
{
	unsigned int value;

	/* DMA SW reset */
	value = (unsigned int)readl(ioaddr + DMA_BUS_MODE);
	value |= DMA_BUS_MODE_SFT_RESET;
	writel(value, ioaddr + DMA_BUS_MODE);
	while ((readl(ioaddr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET)) {
	}

	/* Enable Application Access by writing to DMA CSR0 */
	value = DMA_BUS_MODE_4PBL | ((pbl << DMA_BUS_MODE_PBL_SHIFT) |
				     (pbl << DMA_BUS_MODE_RPBL_SHIFT));

#ifdef CONFIG_STMMAC_DA
	value |= DMA_BUS_MODE_DA;	/* Rx has priority over tx */
#endif
	writel(value, ioaddr + DMA_BUS_MODE);

	/* Mask interrupts by writing to CSR7 */
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_INTR_ENA);

	/* The base address of the RX/TX descriptor lists must be written into
	 * DMA CSR3 and CSR4, respectively. */
	writel(dma_tx, ioaddr + DMA_TX_BASE_ADDR);
	writel(dma_rx, ioaddr + DMA_RCV_BASE_ADDR);

	return 0;
}

/* Transmit FIFO flush operation */
static void gmac_flush_tx_fifo(unsigned long ioaddr)
{
	unsigned int csr6;

	csr6 = (unsigned int)readl(ioaddr + DMA_CONTROL);
	writel((csr6 | DMA_CONTROL_FTF), ioaddr + DMA_CONTROL);

	while ((readl(ioaddr + DMA_CONTROL) & DMA_CONTROL_FTF)) {
	}
}

static void gmac_dma_operation_mode(unsigned long ioaddr, int threshold)
{
	unsigned int csr6;

	csr6 = (unsigned int)readl(ioaddr + DMA_CONTROL);

#ifdef GMAC_TX_STORE_AND_FORWARD
	csr6 |= DMA_CONTROL_TSF;
#else
	if (threshold <= 32)
		csr6 |= DMA_CONTROL_TTC_32;
	else if (threshold <= 64)
		csr6 |= DMA_CONTROL_TTC_64;
	else if (threshold <= 128)
		csr6 |= DMA_CONTROL_TTC_128;
	else if (threshold <= 192)
		csr6 |= DMA_CONTROL_TTC_192;
	else
		csr6 |= DMA_CONTROL_TTC_256;
#endif

#ifdef GMAC_RX_STORE_AND_FORWARD
	csr6 |= DMA_CONTROL_RSF;
#else
	if (threshold <= 32)
		csr6 |= DMA_CONTROL_RTC_32;
	else if (threshold <= 64)
		csr6 |= DMA_CONTROL_RTC_64;
	else if (threshold <= 96)
		csr6 |= DMA_CONTROL_RTC_96;
	else
		csr6 |= DMA_CONTROL_RTC_128;
#endif
	/* Operating on second frame increase the performance 
	 * especially when transmit store-and-forward is used.*/
	csr6 |= DMA_CONTROL_OSF;

	writel(csr6, ioaddr + DMA_CONTROL);
	return;
}

/* Not yet implemented --- RMON */
static void gmac_dma_diagnostic_fr(void *data, struct stmmac_extra_stats *x,
				   unsigned long ioaddr)
{
	return;
}

static void gmac_dump_dma_regs(unsigned long ioaddr)
{
	int i;
	printk(KERN_INFO " DMA registers\n");
	for (i = 0; i < 9; i++) {
		if ((i < 9) || (i > 17)) {
			int offset = i * 4;
			printk(KERN_INFO
			       "\t Reg No. %d (offset 0x%x): 0x%08x\n", i,
			       (DMA_BUS_MODE + offset),
			       readl(ioaddr + DMA_BUS_MODE + offset));
		}
	}
	return;
}

static int gmac_get_tx_frame_status(void *data, struct stmmac_extra_stats *x,
				    dma_desc * p, unsigned long ioaddr)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)data;

	if (unlikely(p->des01.etx.error_summary)) {

		if (unlikely(p->des01.etx.jabber_timeout)) {
			DBG(KERN_ERR "GMAC TX: jabber_timeout error\n");
			x->tx_jabber++;
		}

		if (unlikely(p->des01.etx.frame_flushed)) {
			DBG(KERN_ERR "GMAC TX: frame_flushed error\n");
			x->tx_frame_flushed++;
			gmac_flush_tx_fifo(ioaddr);
		}

		if (unlikely(p->des01.etx.loss_carrier)) {
			DBG(KERN_ERR "GMAC TX: loss_carrier error\n");
			x->tx_losscarrier++;
			stats->tx_carrier_errors++;
		}
		if (unlikely(p->des01.etx.no_carrier)) {
			DBG(KERN_ERR "GMAC TX: no_carrier error\n");
			x->tx_carrier++;
			stats->tx_carrier_errors++;
		}
		if (unlikely(p->des01.etx.late_collision)) {
			DBG(KERN_ERR "GMAC TX: late_collision error\n");
			stats->collisions += p->des01.etx.collision_count;
		}
		if (unlikely(p->des01.etx.excessive_collisions)) {
			DBG(KERN_ERR "GMAC TX: excessive_collisions\n");
			stats->collisions += p->des01.etx.collision_count;
		}
		if (unlikely(p->des01.etx.excessive_deferral))
			x->tx_deferred++;

		if (unlikely(p->des01.etx.underflow_error)) {
			DBG(KERN_ERR "GMAC TX: underflow error\n");
			gmac_flush_tx_fifo(ioaddr);
			x->tx_underflow++;
		}

		if (unlikely(p->des01.etx.payload_error)) {
			DBG(KERN_ERR "%s: TX Addr/Payload csum error\n",
			    __FUNCTION__);
			x->tx_payload_error++;
			gmac_flush_tx_fifo(ioaddr);
		}

		if (unlikely(p->des01.etx.ip_header_error)) {
			DBG(KERN_ERR "%s: TX IP header csum error\n",
			    __FUNCTION__);
			x->tx_ip_header_error++;
		}

		ret = -1;
	}

	if (unlikely(p->des01.etx.deferred)) {
		x->tx_deferred++;
		ret = -1;
	}
	if (p->des01.etx.vlan_frame) {
		DBG(KERN_INFO "GMAC TX: VLAN frame\n");
		x->tx_vlan++;
	}

	return (ret);
}

static int gmac_get_rx_frame_status(void *data, struct stmmac_extra_stats *x,
				    dma_desc * p)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)data;

	if (unlikely(p->des01.erx.error_summary)) {
		if (unlikely(p->des01.erx.descriptor_error)) {
			/* frame doesn't fit within the current descriptor. */
			DBG(KERN_ERR "GMAC RX: descriptor error\n");
			x->rx_desc++;
			stats->rx_length_errors++;
		}
		if (unlikely(p->des01.erx.overflow_error)) {
			DBG(KERN_ERR "GMAC RX: Overflow error\n");
			x->rx_gmac_overflow++;
		}
		if (unlikely(p->des01.erx.late_collision)) {
			DBG(KERN_ERR "GMAC RX: late_collision\n");
			stats->collisions++;
			stats->collisions++;
		}
		if (unlikely(p->des01.erx.receive_watchdog)) {
			DBG(KERN_ERR "GMAC RX: receive_watchdog error\n");
			x->rx_watchdog++;
		}
		if (unlikely(p->des01.erx.error_gmii)) {
			DBG(KERN_ERR "GMAC RX: GMII error\n");
			x->rx_mii++;
		}
		if (unlikely(p->des01.erx.crc_error)) {
			DBG(KERN_ERR "GMAC RX: CRC error\n");
			x->rx_crc++;
			stats->rx_crc_errors++;
		}
		ret = -1;
	}

	if (unlikely(p->des01.erx.dribbling)) {
		DBG(KERN_ERR "GMAC RX:  dribbling error\n");
		ret = -1;
	}
	if (unlikely(p->des01.erx.filtering_fail)) {
		DBG(KERN_ERR "GMAC RX: filtering_fail error\n");
		x->rx_filter++;
		ret = -1;
	}
	if (unlikely(p->des01.erx.length_error)) {
		DBG(KERN_ERR "GMAC RX: length_error error\n");
		x->rx_lenght++;
		ret = -1;
	}
	return (ret);
}

static int gmac_rx_checksum(dma_desc * p)
{
	int ret = 0;
	/* Full COE type 2 is supported */
	if (unlikely((p->des01.erx.ipc_csum_error == 1)) ||
	    (p->des01.erx.payload_csum_error == 1)) {

		if (p->des01.erx.payload_csum_error)
			DBG(KERN_WARNING "%s: IPC csum error\n", __FUNCTION__);
		if (p->des01.erx.payload_csum_error)
			DBG(KERN_WARNING "(Address/Payload csum error.)\n");

		ret = -1;
	}
	return ret;
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
		value = GMAC_FRAME_FILTER_PM;	/// pass all multi
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

	DBG(KERN_INFO "%s: GMAC frame filter reg: 0x%08x - Hash regs: "
	    "HI 0x%08x, LO 0x%08x\n",
	    __FUNCTION__, readl(ioaddr + GMAC_FRAME_FILTER),
	    readl(ioaddr + GMAC_HASH_HIGH), readl(ioaddr + GMAC_HASH_LOW));
	return;
}

static void gmac_flow_ctrl(unsigned long ioaddr, unsigned int duplex,
			   unsigned int fc, unsigned int pause_time)
{
	unsigned int flow = 0;

	DBG(KERN_DEBUG "GMAC Flow-Control:\n");
	if (fc & FLOW_RX) {
		DBG(KERN_DEBUG "\tReceive Flow-Control ON\n");
		flow |= GMAC_FLOW_CTRL_RFE;
	}
	if (fc & FLOW_TX) {
		DBG(KERN_DEBUG "\tTransmit Flow-Control ON\n");
		flow |= GMAC_FLOW_CTRL_TFE;
	}

	if (duplex) {
		DBG(KERN_DEBUG "\tduplex mode: pause time: %d\n", pause_time);
		flow |= (pause_time << GMAC_FLOW_CTRL_PT_SHIFT);
	}

	writel(flow, ioaddr + GMAC_FLOW_CTRL);
	return;
}

static void gmac_pmt(unsigned long ioaddr, unsigned long mode)
{
	unsigned int pmt = power_down;

	if (mode == WAKE_MAGIC) {
		pmt |= magic_pkt_en;
	} else if (mode == WAKE_UCAST) {
		pmt |= global_unicast;
	}

	writel(pmt, ioaddr + GMAC_PMT);
	return;
}

static void gmac_init_rx_desc(dma_desc * p, unsigned int ring_size,
			      int rx_irq_threshold)
{
	int i;
	for (i = 0; i < ring_size; i++) {
		p->des01.erx.own = 1;
		p->des01.erx.buffer1_size = DMA_BUFFER_SIZE - 1;
		if (i % rx_irq_threshold)
			p->des01.erx.disable_ic = 1;
		if (i == ring_size - 1)
			p->des01.erx.end_ring = 1;
		p++;
	}
	return;
}

static void gmac_init_tx_desc(dma_desc * p, unsigned int ring_size)
{
	int i;

	for (i = 0; i < ring_size; i++) {
		p->des01.etx.own = 0;
		if (i == ring_size - 1)
			p->des01.etx.end_ring = 1;
		p++;
	}

	return;
}

static int gmac_read_tx_owner(dma_desc * p)
{
	return p->des01.etx.own;
}

static int gmac_read_rx_owner(dma_desc * p)
{
	return p->des01.erx.own;
}

static void gmac_set_tx_owner(dma_desc * p)
{
	p->des01.etx.own = 1;
}

static void gmac_set_rx_owner(dma_desc * p)
{
	p->des01.erx.own = 1;
}

static int gmac_get_tx_ls(dma_desc * p)
{
	return p->des01.etx.last_segment;
}

static void gmac_release_tx_desc(dma_desc * p)
{
	int ter = p->des01.etx.end_ring;

	memset(p, 0, sizeof(dma_desc));
	p->des01.etx.end_ring = ter;

	return;
}

static void gmac_prepare_tx_desc(dma_desc * p, int is_fs, int len,
				 unsigned int csum_flags)
{
	p->des01.etx.first_segment = is_fs;
	p->des01.etx.buffer1_size = len;
	if (csum_flags)
		p->des01.etx.checksum_insertion = cic_full;
	/*p->des01.etx.checksum_insertion = cic_no_pseudoheader; */
}

static void gmac_set_tx_ic(dma_desc * p, int value)
{
	p->des01.etx.interrupt = value;
}

static void gmac_set_tx_ls(dma_desc * p)
{
	p->des01.etx.last_segment = 1;
}

static int gmac_get_rx_frame_len(dma_desc * p)
{
	return p->des01.erx.frame_length;
}

struct device_ops gmac_driver = {
	.core_init = gmac_core_init,
	.dump_mac_regs = gmac_dump_regs,
	.dma_init = gmac_dma_init,
	.dump_dma_regs = gmac_dump_dma_regs,
	.dma_operation_mode = gmac_dma_operation_mode,
	.dma_diagnostic_fr = gmac_dma_diagnostic_fr,
	.tx_status = gmac_get_tx_frame_status,
	.rx_status = gmac_get_rx_frame_status,
	.rx_checksum = gmac_rx_checksum,
	.set_filter = gmac_set_filter,
	.flow_ctrl = gmac_flow_ctrl,
	.pmt = gmac_pmt,
	.init_rx_desc = gmac_init_rx_desc,
	.init_tx_desc = gmac_init_tx_desc,
	.read_tx_owner = gmac_read_tx_owner,
	.read_rx_owner = gmac_read_rx_owner,
	.release_tx_desc = gmac_release_tx_desc,
	.prepare_tx_desc = gmac_prepare_tx_desc,
	.set_tx_ic = gmac_set_tx_ic,
	.set_tx_ls = gmac_set_tx_ls,
	.get_tx_ls = gmac_get_tx_ls,
	.set_tx_owner = gmac_set_tx_owner,
	.set_rx_owner = gmac_set_rx_owner,
	.get_rx_frame_len = gmac_get_rx_frame_len,
};

struct device_info_t *gmac_setup(unsigned long ioaddr)
{
	struct device_info_t *mac;
	unsigned int id;
	id = (unsigned int)readl(ioaddr + GMAC_VERSION);

	printk(KERN_INFO "\tGMAC - user ID: 0x%x, Synopsys ID: 0x%x\n",
	       ((id & 0x0000ff00) >> 8), (id & 0x000000ff));

	mac = kmalloc(sizeof(const struct device_info_t), GFP_KERNEL);
	memset(mac, 0, sizeof(struct device_info_t));

	mac->ops = &gmac_driver;
	mac->hw.pmt = PMT_SUPPORTED;
#ifdef GMAC_TX_STORE_AND_FORWARD
	mac->hw.csum = HAS_HW_CSUM;
#else
	mac->hw.csum = NO_HW_CSUM;
#endif
	mac->hw.addr_high = GMAC_ADDR_HIGH;
	mac->hw.addr_low = GMAC_ADDR_LOW;
	mac->hw.link.port = GMAC_CONTROL_PS;
	mac->hw.link.duplex = GMAC_CONTROL_DM;
	mac->hw.link.speed = GMAC_CONTROL_FES;
	mac->hw.mii.addr = GMAC_MII_ADDR;
	mac->hw.mii.data = GMAC_MII_DATA;

	return mac;
}
