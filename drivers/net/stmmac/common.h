#include "descs.h"

/* *********************************************
   DMA CRS Control and Status Register Mapping 
 * *********************************************/
#define DMA_BUS_MODE		0x00001000	/* Bus Mode */
#define DMA_XMT_POLL_DEMAND	0x00001004	/* Transmit Poll Demand */
#define DMA_RCV_POLL_DEMAND	0x00001008	/* Received Poll Demand */
#define DMA_RCV_BASE_ADDR	0x0000100c	/* Receive List Base */
#define DMA_TX_BASE_ADDR	0x00001010	/* Transmit List Base */
#define DMA_STATUS		0x00001014	/* Status Register */
#define DMA_CONTROL		0x00001018	/* Ctrl (Operational Mode) */
#define DMA_INTR_ENA		0x0000101c	/* Interrupt Enable */
#define DMA_MISSED_FRAME_CTR	0x00001020	/* Missed Frame Counter */
#define DMA_CUR_TX_BUF_ADDR	0x00001050	/* Current Host Tx Buffer */
#define DMA_CUR_RX_BUF_ADDR	0x00001054	/* Current Host Rx Buffer */

/* ********************************
   DMA Control register defines
 * ********************************/
#define DMA_CONTROL_ST		0x00002000	/* Start/Stop Transmission */
#define DMA_CONTROL_SR		0x00000002	/* Start/Stop Receive */

/* **************************************
   DMA Interrupt Enable register defines
 * **************************************/
/**** NORMAL INTERRUPT ****/
#define DMA_INTR_ENA_NIE 0x00010000	/* Normal Summary */
#define DMA_INTR_ENA_TIE 0x00000001	/* Transmit Interrupt */
#define DMA_INTR_ENA_TUE 0x00000004	/* Transmit Buffer Unavailable */
#define DMA_INTR_ENA_RIE 0x00000040	/* Receive Interrupt */
#define DMA_INTR_ENA_ERE 0x00004000	/* Early Receive */

#define DMA_INTR_NORMAL	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | DMA_INTR_ENA_TIE \
			/*| DMA_INTR_ENA_ERE | DMA_INTR_ENA_TUE*/)

/**** ABNORMAL INTERRUPT ****/
#define DMA_INTR_ENA_AIE 0x00008000	/* Abnormal Summary */
#define DMA_INTR_ENA_FBE 0x00002000	/* Fatal Bus Error */
#define DMA_INTR_ENA_ETE 0x00000400	/* Early Transmit */
#define DMA_INTR_ENA_RWE 0x00000200	/* Receive Watchdog */
#define DMA_INTR_ENA_RSE 0x00000100	/* Receive Stopped */
#define DMA_INTR_ENA_RUE 0x00000080	/* Receive Buffer Unavailable */
#define DMA_INTR_ENA_UNE 0x00000020	/* Tx Underflow */
#define DMA_INTR_ENA_OVE 0x00000010	/* Receive Overflow */
#define DMA_INTR_ENA_TJE 0x00000008	/* Transmit Jabber */
#define DMA_INTR_ENA_TSE 0x00000002	/* Transmit Stopped */

#define DMA_INTR_ABNORMAL	DMA_INTR_ENA_AIE | DMA_INTR_ENA_FBE /*| DMA_INTR_ENA_UNE*/

/* DMA default interrupt mask */
#define DMA_INTR_DEFAULT_MASK	(DMA_INTR_NORMAL | DMA_INTR_ABNORMAL)
/* Disable DMA Rx IRQ (NAPI) */
#define DMA_INTR_NO_RX	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_TIE | \
			DMA_INTR_ENA_TUE | DMA_INTR_ABNORMAL)

/* ****************************
 *  DMA Status register defines
 * ****************************/
#define DMA_STATUS_EB_MASK	0x00380000	/* Error Bits Mask */
#define DMA_STATUS_EB_TX_ABORT	0x00080000	/* Error Bits - TX Abort */
#define DMA_STATUS_EB_RX_ABORT	0x00100000	/* Error Bits - RX Abort */
#define DMA_STATUS_TS_MASK	0x00700000	/* Transmit Process State */
#define DMA_STATUS_TS_SHIFT	20
#define DMA_STATUS_RS_MASK	0x000e0000	/* Receive Process State */
#define DMA_STATUS_RS_SHIFT	17
#define DMA_STATUS_NIS	0x00010000	/* Normal Interrupt Summary */
#define DMA_STATUS_AIS	0x00008000	/* Abnormal Interrupt Summary */
#define DMA_STATUS_ERI	0x00004000	/* Early Receive Interrupt */
#define DMA_STATUS_FBI	0x00002000	/* Fatal Bus Error Interrupt */
#define DMA_STATUS_ETI	0x00000400	/* Early Transmit Interrupt */
#define DMA_STATUS_RWT	0x00000200	/* Receive Watchdog Timeout */
#define DMA_STATUS_RPS	0x00000100	/* Receive Process Stopped */
#define DMA_STATUS_RU	0x00000080	/* Receive Buffer Unavailable */
#define DMA_STATUS_RI	0x00000040	/* Receive Interrupt */
#define DMA_STATUS_UNF	0x00000020	/* Transmit Underflow */
#define DMA_STATUS_OVF	0x00000010	/* Receive Overflow */
#define DMA_STATUS_TJT	0x00000008	/* Transmit Jabber Timeout */
#define DMA_STATUS_TU	0x00000004	/* Transmit Buffer Unavailable */
#define DMA_STATUS_TPS	0x00000002	/* Transmit Process Stopped */
#define DMA_STATUS_TI	0x00000001	/* Transmit Interrupt */

#define DMA_BUFFER_SIZE	2048

/* Other defines */
#define HASH_TABLE_SIZE 64
#define PAUSE_TIME 0x200

/* Flow Control defines */
#define FLOW_OFF	0
#define FLOW_RX		1
#define FLOW_TX		2
#define FLOW_AUTO	(FLOW_TX | FLOW_RX)

/* HW csum */
#define NO_HW_CSUM 0
#define HAS_HW_CSUM 1

/* Power Down and WOL */
#define PMT_NOT_SUPPORTED 0
#define PMT_SUPPORTED 1

/* Common MAC defines */
#define MAC_CTRL_REG		0x00000000	/* MAC Control */
#define MAC_ENABLE_TX		0x00000008	/* Transmitter Enable */
#define MAC_RNABLE_RX		0x00000004	/* Receiver Enable */

struct stmmac_extra_stats {
	unsigned long tx_underflow;
	unsigned long tx_carrier;
	unsigned long tx_losscarrier;
	unsigned long tx_heartbeat;
	unsigned long tx_deferred;
	unsigned long tx_vlan;
	unsigned long tx_jabber;
	unsigned long tx_frame_flushed;
	unsigned long rx_desc;
	unsigned long rx_partial;
	unsigned long rx_runt;
	unsigned long rx_toolong;
	unsigned long rx_collision;
	unsigned long rx_crc;
	unsigned long rx_lenght;
	unsigned long rx_mii;
	unsigned long rx_multicast;
	unsigned long rx_gmac_overflow;
	unsigned long rx_watchdog;
	unsigned long rx_filter;
	unsigned long rx_dropped;
	unsigned long rx_bytes;
	unsigned long tx_bytes;
	unsigned long tx_irq_n;
	unsigned long rx_irq_n;
	unsigned long tx_undeflow_irq;
	unsigned long threshold;
	unsigned long tx_process_stopped_irq;
	unsigned long tx_jabber_irq;
	unsigned long rx_overflow_irq;
	unsigned long rx_buf_unav_irq;
	unsigned long rx_process_stopped_irq;
	unsigned long rx_watchdog_irq;
	unsigned long tx_early_irq;
	unsigned long fatal_bus_error_irq;
	unsigned long rx_poll_n;
	unsigned long tx_payload_error;
	unsigned long tx_ip_header_error;
	unsigned long rx_missed_cntr;
	unsigned long rx_overflow_cntr;
};
#define EXTRA_STATS 40

/* Specific device structure VFP in order to mark the
 * difference between mac and gmac in terms of registers, descriptors etc.
 */
struct device_ops {
	/* MAC core */
	void (*core_init) (unsigned long ioaddr);
	void (*dump_mac_regs) (unsigned long ioaddr);

	/* DMA core */
	int (*dma_init) (unsigned long ioaddr, int pbl, u32 dma_tx, u32 dma_rx);
	void (*dump_dma_regs) (unsigned long ioaddr);
	void (*dma_operation_mode) (unsigned long ioaddr, int threshold);
	void (*dma_diagnostic_fr) (void *data, struct stmmac_extra_stats *x,
					unsigned long ioaddr);


	/* Descriptors */
	void (*init_rx_desc) (dma_desc * p, unsigned int ring_size,
			      int rx_irq_threshol);
	void (*init_tx_desc) (dma_desc * p, unsigned int ring_size);
	int (*set_buf_size) (unsigned int len);
	int (*read_tx_ls) (void);
	int (*read_tx_owner) (dma_desc * p);
	int (*read_rx_owner) (dma_desc * p);
	void (*release_tx_desc) (dma_desc * p);
	void (*prepare_tx_desc) (dma_desc * p, int is_fs, int len, 
				unsigned int csum_flags);
	void (*set_tx_ic) (dma_desc * p, int value);
	void (*set_tx_ls) (dma_desc * p);
	int (*get_tx_ls) (dma_desc * p);
	void (*set_tx_owner) (dma_desc * p);
	void (*set_rx_owner) (dma_desc * p);
	int (*get_rx_frame_len) (dma_desc * p);

	/* driver functions */
	int (*tx_status) (void *data, struct stmmac_extra_stats * x,
			  dma_desc * p, unsigned long ioaddr);
	int (*rx_status) (void *data, struct stmmac_extra_stats * x,
			  dma_desc * p);
	void (*tx_checksum) (struct sk_buff * skb, dma_desc * p);
	int (*rx_checksum) (dma_desc * p);
	void (*set_filter) (struct net_device * dev);
	void (*flow_ctrl) (unsigned long ioaddr, unsigned int duplex,
			   unsigned int fc, unsigned int pause_time);
	void (*pmt) (unsigned long ioaddr, unsigned long mode);
};

struct mac_link_t {
	int port;
	int duplex;
	int speed;
};

struct mii_regs_t {
	unsigned int addr;	/* MII Address */
	unsigned int data;	/* MII Data */
};

struct mac_regs_t {
	unsigned int addr_high;	/* Multicast Hash Table High */
	unsigned int addr_low;	/* Multicast Hash Table Low */
	unsigned int version;	/* Core Version register (GMAC) */
	unsigned int pmt;	/* Power-Down mode (GMAC) */
	unsigned int csum;	/* Checksum Offload */
	struct mac_link_t link;
	struct mii_regs_t mii;
};

struct device_info_t {
	struct mac_regs_t hw;
	struct device_ops *ops;
};
