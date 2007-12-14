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
   DMA Bus Mode register defines 
 * ********************************/
#define DMA_BUS_MODE_PBL_MASK	0x00003f00	/* Programmable Burst Len */
#define DMA_BUS_MODE_PBL_SHIFT	8
#define DMA_BUS_MODE_DSL_MASK	0x0000007c	/* Descriptor Skip Length */
#define DMA_BUS_MODE_DSL_SHIFT	2	/*   (in DWORDS)      */
#define DMA_BUS_MODE_BAR_BUS	0x00000002	/* Bar-Bus Arbitration */
#define DMA_BUS_MODE_SFT_RESET	0x00000001	/* Software Reset */
#define DMA_BUS_MODE_DEFAULT	0x00000000

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
#define DMA_INTR_ENA_ERE 0x00004000	/* Early Receive */
#define DMA_INTR_ENA_RIE 0x00000040	/* Receive Interrupt */
#define DMA_INTR_ENA_TIE 0x00000001	/* Transmit Interrupt */
#define DMA_INTR_ENA_TUE 0x00000004	/* Transmit Buffer Unavailable */

#define DMA_INTR_NORMAL	(DMA_INTR_ENA_RIE | DMA_INTR_ENA_TIE)

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

#define DMA_INTR_ABNORMAL	(DMA_INTR_ENA_UNE)

/* DMA default interrupt mask */
#define DMA_INTR_DEFAULT_MASK	(DMA_INTR_ENA_NIE | DMA_INTR_NORMAL | \
				DMA_INTR_ENA_AIE |DMA_INTR_ABNORMAL)
/* Disable DMA Rx IRQ (NAPI) */
#define DMA_INTR_NO_RX		(DMA_INTR_ENA_NIE |  DMA_INTR_ENA_TIE | \
				DMA_INTR_ENA_AIE | DMA_INTR_ABNORMAL)

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

/* ****************************
 *     Descriptor defines
 * ****************************/
#define OWN_BIT			0x80000000	/* Own Bit (owned by hardware) */
#define DES1_CONTROL_CH		0x01000000	/* Second Address Chained */
#define DES1_CONTROL_TER	0x02000000	/* End of Ring */
#define DES1_RBS2_SIZE_MASK	0x003ff800	/* Buffer 2 Size Mask */
#define DES1_RBS2_SIZE_SHIFT	11		/* Buffer 2 Size Shift */
#define DES1_RBS1_SIZE_MASK	0x000007ff	/* Buffer 1 Size Mask */
#define DES1_RBS1_SIZE_SHIFT	0		/* Buffer 1 Size Shift */


/* Transmit descriptor 0*/
#define TDES0_STATUS_ES		  0x00008000	/* Error Summary */

/* Transmit descriptor 1*/
#define TDES1_CONTROL_IC	0x80000000	/* Interrupt on Completion */
#define TDES1_CONTROL_LS	0x40000000	/* Last Segment */
#define TDES1_CONTROL_FS	0x20000000	/* First Segment */
#define TDES1_CONTROL_AC	0x04000000	/* Add CRC Disable */
#define TDES1_CONTROL_DPD	0x00800000	/* Disable Padding */

/* Rx descriptor 0 */
#define RDES0_STATUS_FL_MASK 0x3fff0000	/* Frame Length Mask */
#define RDES0_STATUS_FL_SHIFT 16	/* Frame Length Shift */
#define RDES0_STATUS_FS 0x00000200   /* First Descriptor */
#define RDES0_STATUS_LS 0x00000100   /* Last Descriptor */
#define RDES0_STATUS_ES	0x00008000	/* Error Summary */

/* Other defines */
#define HASH_TABLE_SIZE 64
#define PAUSE_TIME 0x200

/* Flow Control defines */
#define FLOW_OFF	0x0
#define FLOW_RX		0x1
#define FLOW_TX		0x2
#define FLOW_AUTO	(FLOW_TX | FLOW_RX)

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
	unsigned long rx_overflow;
	unsigned long rx_watchdog;
	unsigned long rx_filter;
	unsigned long rx_dropped;
	unsigned long rx_bytes;
	unsigned long tx_bytes;
	unsigned long tx_irq_n;
	unsigned long rx_irq_n;
	unsigned long tx_undeflow_irq;
	unsigned long tx_threshold;
	unsigned long tx_process_stopped_irq;
	unsigned long tx_jabber_irq;
	unsigned long rx_overflow_irq;
	unsigned long rx_buf_unav_irq;
	unsigned long rx_process_stopped_irq;
	unsigned long rx_watchdog_irq;
	unsigned long tx_early_irq;
	unsigned long fatal_bus_error_irq;
};
#define EXTRA_STATS 35

struct device_ops {
	/* MAC controller initialization */
	void (*core_init) (unsigned long ioaddr);
	/* MAC registers */
	void (*mac_registers) (unsigned long ioaddr);
	/* DMA registers */
	void (*dma_registers) (unsigned long ioaddr);
	/* DMA tx threshold */
	void (*dma_ttc) (unsigned long ioaddr, int value);
	/* Return zero if no error is happened during the transmission */
	int (*tx_err) (void *p, struct stmmac_extra_stats *x,
			unsigned int status);
	/* Check if the frame was not successfully received */
	int (*rx_err) (void *p, struct stmmac_extra_stats *x,
			unsigned int status);
	/* Verify the TX checksum */
	void (*tx_checksum) (struct sk_buff * skb);
	/* Verifies the RX checksum */
	void (*rx_checksum) (struct sk_buff * skb, int status);
	/* Enable/Disable Multicast filtering */
	void (*set_filter) (struct net_device * dev);
	/* Flow Control */
	void (*flow_ctrl) (unsigned long ioaddr, unsigned int duplex,
			   unsigned int fc, unsigned int pause_time);

};

struct mac_link_t {
	int port;
	int duplex;
	int speed;
};

struct mii_regs_t {
	unsigned int addr;	/* MII Address */
	unsigned int data;	/* MII Data */
	unsigned int addr_write;	/* MII Write */
	unsigned int addr_busy;	/* MII Busy */
};

struct mac_regs_t {
	unsigned int control;	/* MAC CTRL register */
	unsigned int addr_high;	/* Multicast Hash Table High */
	unsigned int addr_low;	/* Multicast Hash Table Low */
	unsigned int enable_rx;	/* Receiver Enable */
	unsigned int enable_tx;	/* Transmitter Enable */
	unsigned int version;	/* Core Version register */
	struct mac_link_t link;
	struct mii_regs_t mii;
};

struct device_info_t {
	char *name;		/* device name */
	struct mac_regs_t hw;
	struct device_ops *ops;
};
