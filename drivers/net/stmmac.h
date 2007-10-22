
/*----------------------------------------------------------------------------
 *	 			MAC BLOCK defines
 *---------------------------------------------------------------------------*/
/* MAC CSR offset */
#define MAC_CONTROL               0x00000000  /* MAC Control */
#define MAC_ADDR_HIGH             0x00000004  /* MAC Address High */
#define MAC_ADDR_LOW              0x00000008  /* MAC Address Low */
#define MAC_HASH_HIGH             0x0000000c  /* Multicast Hash Table High */
#define MAC_HASH_LOW              0x00000010  /* Multicast Hash Table Low */
#define MAC_MII_ADDR              0x00000014  /* MII Address */
#define MAC_MII_DATA              0x00000018  /* MII Data */
#define MAC_FLOW_CONTROL          0x0000001c  /* Flow Control */
#define MAC_VLAN1                 0x00000020  /* VLAN1 Tag */
#define MAC_VLAN2                 0x00000024  /* VLAN2 Tag */
#define MAC_WAKEUP_FILTER         0x00000028  /* Wake-up Frame Filter */
#define MAC_WAKEUP_CONTROL_STATUS 0x0000002c  /* Wake-up Control And Status */

/* MAC CTRL defines */
#define MAC_CONTROL_RA            0x80000000  /* Receive All Mode */
#define MAC_CONTROL_BLE           0x40000000  /* Endian Mode */
#define MAC_CONTROL_HBD           0x10000000  /* Heartbeat Disable */
#define MAC_CONTROL_PS            0x08000000  /* Port Select */
#define MAC_CONTROL_DRO           0x00800000  /* Disable Receive Own */
#define MAC_CONTROL_EXT_LOOPBACK  0x00400000  /* Reserved (ext loopback?) */
#define MAC_CONTROL_OM            0x00200000  /* Loopback Operating Mode */
#define MAC_CONTROL_F             0x00100000  /* Full Duplex Mode */
#define MAC_CONTROL_PM            0x00080000  /* Pass All Multicast */
#define MAC_CONTROL_PR            0x00040000  /* Promiscuous Mode */
#define MAC_CONTROL_IF            0x00020000  /* Inverse Filtering */
#define MAC_CONTROL_PB            0x00010000  /* Pass Bad Frames */
#define MAC_CONTROL_HO            0x00008000  /* Hash Only Filtering Mode */
#define MAC_CONTROL_HP            0x00002000  /* Hash/Perfect Filtering Mode */
#define MAC_CONTROL_LCC           0x00001000  /* Late Collision Control */
#define MAC_CONTROL_DBF           0x00000800  /* Disable Broadcast Frames */
#define MAC_CONTROL_DRTY          0x00000400  /* Disable Retry */
#define MAC_CONTROL_ASTP          0x00000100  /* Automatic Pad Stripping */
#define MAC_CONTROL_BOLMT_10      0x00000000  /* Back Off Limit 10 */
#define MAC_CONTROL_BOLMT_8       0x00000040  /* Back Off Limit 8 */
#define MAC_CONTROL_BOLMT_4       0x00000080  /* Back Off Limit 4 */
#define MAC_CONTROL_BOLMT_1       0x000000c0  /* Back Off Limit 1 */
#define MAC_CONTROL_DC            0x00000020  /* Deferral Check */
#define MAC_CONTROL_TE            0x00000008  /* Transmitter Enable */
#define MAC_CONTROL_RE            0x00000004  /* Receiver Enable */

/* MAC FLOW CTRL defines */
#define MAC_FLOW_CONTROL_PT_MASK  0xffff0000  /* Pause Time Mask */
#define MAC_FLOW_CONTROL_PT_SHIFT 16
#define MAC_FLOW_CONTROL_PCF      0x00000004  /* Pass Control Frames */
#define MAC_FLOW_CONTROL_FCE      0x00000002  /* Flow Control Enable */
#define MAC_FLOW_CONTROL_PAUSE    0x00000001  /* Flow Control Busy ... */

/* MII ADDR  defines */
#define MAC_MII_ADDR_WRITE        0x00000002  /* MII Write */
#define MAC_MII_ADDR_BUSY         0x00000001  /* MII Busy */

/* MAC Management Counters register */
#define MMC_CONTROL               0x00000100  /* MMC Control */
#define MMC_HIGH_INTR             0x00000104  /* MMC High Interrupt */
#define MMC_LOW_INTR              0x00000108  /* MMC Low Interrupt */
#define MMC_HIGH_INTR_MASK        0x0000010c  /* MMC High Interrupt Mask */
#define MMC_LOW_INTR_MASK         0x00000110  /* MMC Low Interrupt Mask */

#define MMC_CONTROL_MAX_FRM_MASK  0x0003ff8   /* Maximum Frame Size */
#define MMC_CONTROL_MAX_FRM_SHIFT 3
#define MMC_CONTROL_MAX_FRAME 	  0x7FF

/*----------------------------------------------------------------------------
 * 				DMA BLOCK defines
 *---------------------------------------------------------------------------*/
/* DMA CRS Control and Status Register Mapping */
#define DMA_BUS_MODE              0x00001000  /* Bus Mode */
#define DMA_XMT_POLL_DEMAND       0x00001004  /* Transmit Poll Demand */
#define DMA_RCV_POLL_DEMAND       0x00001008  /* Received Poll Demand */
#define DMA_RCV_BASE_ADDR         0x0000100c  /* Receive List Base */
#define DMA_TX_BASE_ADDR          0x00001010  /* Transmit List Base */
#define DMA_STATUS                0x00001014  /* Status Register */
#define DMA_CONTROL               0x00001018  /* Control (Operational Mode) */
#define DMA_INTR_ENA              0x0000101c  /* Interrupt Enable */
#define DMA_MISSED_FRAME_CTR      0x00001020  /* Missed Frame Counter */
#define DMA_CUR_TX_BUF_ADDR       0x00001050  /* Current Host Transmit Buffer */
#define DMA_CUR_RX_BUF_ADDR       0x00001054  /* Current Host Receive Buffer */

/*  DMA Bus Mode register defines */
#define DMA_BUS_MODE_DBO          0x00100000  /* Descriptor Byte Ordering */
#define DMA_BUS_MODE_PBL_MASK     0x00003f00  /* Programmable Burst Length */
#define DMA_BUS_MODE_PBL_SHIFT    8
#define DMA_BUS_MODE_BLE          0x00000080  /* Big Endian/Little Endian */
#define DMA_BUS_MODE_DSL_MASK     0x0000007c  /* Descriptor Skip Length */
#define DMA_BUS_MODE_DSL_SHIFT    2           /*       (in DWORDS)      */
#define DMA_BUS_MODE_BAR_BUS      0x00000002  /* Bar-Bus Arbitration */
#define DMA_BUS_MODE_SFT_RESET    0x00000001  /* Software Reset */
#define DMA_BUS_MODE_DEFAULT      0x00000000

/* DMA Status register defines */
#define DMA_STATUS_EB_MASK        0x00380000  /* Error Bits Mask */
#define DMA_STATUS_EB_TX_ABORT    0x00080000  /* Error Bits - TX Abort */
#define DMA_STATUS_EB_RX_ABORT    0x00100000  /* Error Bits - RX Abort */
#define DMA_STATUS_TS_MASK        0x00700000  /* Transmit Process State */
#define DMA_STATUS_TS_SHIFT       20
#define DMA_STATUS_RS_MASK        0x000e0000  /* Receive Process State */
#define DMA_STATUS_RS_SHIFT       17
#define DMA_STATUS_NIS            0x00010000  /* Normal Interrupt Summary */
#define DMA_STATUS_AIS            0x00008000  /* Abnormal Interrupt Summary */
#define DMA_STATUS_ERI            0x00004000  /* Early Receive Interrupt */
#define DMA_STATUS_FBI            0x00002000  /* Fatal Bus Error Interrupt */
#define DMA_STATUS_ETI            0x00000400  /* Early Transmit Interrupt */
#define DMA_STATUS_RWT            0x00000200  /* Receive Watchdog Timeout */
#define DMA_STATUS_RPS            0x00000100  /* Receive Process Stopped */
#define DMA_STATUS_RU             0x00000080  /* Receive Buffer Unavailable */
#define DMA_STATUS_RI             0x00000040  /* Receive Interrupt */
#define DMA_STATUS_UNF            0x00000020  /* Transmit Underflow */
#define DMA_STATUS_OVF            0x00000010  /* Receive Overflow */
#define DMA_STATUS_TJT            0x00000008  /* Transmit Jabber Timeout */
#define DMA_STATUS_TU             0x00000004  /* Transmit Buffer Unavailable */
#define DMA_STATUS_TPS            0x00000002  /* Transmit Process Stopped */
#define DMA_STATUS_TI             0x00000001  /* Transmit Interrupt */

/* DMA Control register defines */
#define DMA_CONTROL_SF            0x00200000  /* Store And Forward */
#define DMA_CONTROL_TTC_MASK      0x0001c000  /* Transmit Threshold Control */
#define DMA_CONTROL_TTC_32        0x00000000  /* Threshold is 32 DWORDS */
#define DMA_CONTROL_TTC_64        0x00004000  /* Threshold is 64 DWORDS */
#define DMA_CONTROL_TTC_128       0x00008000  /* Threshold is 128 DWORDS */
#define DMA_CONTROL_TTC_256       0x0000c000  /* Threshold is 256 DWORDS */
#define DMA_CONTROL_TTC_18        0x00010000  /* Threshold is 18 DWORDS */
#define DMA_CONTROL_TTC_24        0x00014000  /* Threshold is 24 DWORDS */
#define DMA_CONTROL_TTC__32_      0x00018000  /* Threshold is 32 DWORDS */
#define DMA_CONTROL_TTC_40        0x0001c000  /* Threshold is 40 DWORDS */
#define DMA_CONTROL_ST            0x00002000  /* Start/Stop Transmission */
#define DMA_CONTROL_SE            0x00000008  /* Stop On Empty */
#define DMA_CONTROL_OSF           0x00000004  /* Operate On 2nd Frame */
#define DMA_CONTROL_SR            0x00000002  /* Start/Stop Receive */

/* DMA Interrupt Enable register defines */
#define DMA_INTR_ENA_NIE          0x00010000  /* Normal Interrupt Summary */
#define DMA_INTR_ENA_AIE          0x00008000  /* Abnormal Interrupt Summary */
#define DMA_INTR_ENA_ERE          0x00004000  /* Early Receive */
#define DMA_INTR_ENA_FBE          0x00002000  /* Fatal Bus Error */
#define DMA_INTR_ENA_ETE          0x00000400  /* Early Transmit */
#define DMA_INTR_ENA_RWE          0x00000200  /* Receive Watchdog */
#define DMA_INTR_ENA_RSE          0x00000100  /* Receive Stopped */
#define DMA_INTR_ENA_RUE          0x00000080  /* Receive Buffer Unavailable */
#define DMA_INTR_ENA_RIE          0x00000040  /* Receive Interrupt */
#define DMA_INTR_ENA_UNE          0x00000020  /* Underflow */
#define DMA_INTR_ENA_OVE          0x00000010  /* Receive Overflow */
#define DMA_INTR_ENA_TJE          0x00000008  /* Transmit Jabber */
#define DMA_INTR_ENA_TUE          0x00000004  /* Transmit Buffer Unavailable */
#define DMA_INTR_ENA_TSE          0x00000002  /* Transmit Stopped */
#define DMA_INTR_ENA_TIE          0x00000001  /* Transmit Interrupt */
/* DMA default interrupt mask */
#define DMA_INTR_DEFAULT_MASK 	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | \
				DMA_INTR_ENA_TIE)
/* STMAC110 DMA Missed Frame Counter register defines */
#define DMA_MISSED_FRAME_OVE      0x10000000  /* FIFO Overflow Overflow */
#define DMA_MISSED_FRAME_OVE_CNTR 0x0ffe0000  /* Overflow Frame Counter */
#define DMA_MISSED_FRAME_OVE_M    0x00010000  /* Missed Frame Overflow */
#define DMA_MISSED_FRAME_M_CNTR   0x0000ffff  /* Missed Frame Couinter */

/*----------------------------------------------------------------------------
 * 		    	    Descriptor defines
 *---------------------------------------------------------------------------*/
/* Common fields */
#define OWN_BIT			0x80000000  /* Own Bit (owned by hardware) */
#define DES1_CONTROL_CH		0x01000000  /* Second Address Chained */
#define DES1_CONTROL_TER	0x02000000  /* End of Ring */
#define DES1_RBS2_SIZE_MASK	0x003ff800  /* Buffer 2 Size Mask */
#define DES1_RBS2_SIZE_SHIFT	11          /* Buffer 2 Size Shift */
#define DES1_RBS1_SIZE_MASK	0x000007ff  /* Buffer 1 Size Mask */
#define DES1_RBS1_SIZE_SHIFT	0           /* Buffer 1 Size Shift */

/* Receive Descriptor */
#define RDES0_STATUS_FILTER_FAIL  0x40000000  /* Filtering Fail */
#define RDES0_STATUS_FL_MASK      0x3fff0000  /* Frame Length Mask */
#define RDES0_STATUS_FL_SHIFT     16          /* Frame Length Shift */
#define RDES0_STATUS_ES           0x00008000  /* Error Summary */
#define RDES0_STATUS_DE           0x00004000  /* Descriptor Error */
#define RDES0_STATUS_PFE          0x00002000  /* Partial Frame Error */
#define RDES0_STATUS_LENGTH_ERROR 0x00001000  /* Length Error */
#define RDES0_STATUS_RUNT_FRM     0x00000800  /* Runt Frame */
#define RDES0_STATUS_MULTICST_FRM 0x00000400  /* Multicast Frame */
#define RDES0_STATUS_FS           0x00000200  /* First Descriptor */
#define RDES0_STATUS_LS           0x00000100  /* Last Descriptor */
#define RDES0_STATUS_TL           0x00000080  /* Frame Too Long */
#define RDES0_STATUS_COL_SEEN     0x00000040  /* Collision Seen */
#define RDES0_STATUS_FRM_TYPE     0x00000020  /* Frame Type */
#define RDES0_STATUS_RX_WATCHDOG  0x00000010  /* Receive Watchdog */
#define RDES0_STATUS_MII_ERR      0x00000008  /* Report on MII Error */
#define RDES0_STATUS_DRIBBLE      0x00000004  /* Dribbling Bit */
#define RDES0_STATUS_CE           0x00000002  /* CRC Error */
#define RDES0_STATUS_0            0x00000000  /* Always tied to zero */

#define RDES1_CONTROL_DIOC        0x80000000  /* Disable Intr On Completion */
#define RDES0_STATUS_ERROR  (RDES0_STATUS_ES|RDES0_STATUS_LENGTH_ERROR|RDES0_STATUS_MII_ERR)

/* Transmit Descriptor */
#define TDES0_STATUS_ES		  0x00008000  /* Error Summary */
#define TDES0_STATUS_LOSS_CARRIER 0x00000800  /* Loss of Carrier */
#define TDES0_STATUS_NO_CARRIER   0x00000400  /* No Carrier */
#define TDES0_STATUS_LATE_COL     0x00000200  /* Late Collision */
#define TDES0_STATUS_EX_COL       0x00000100  /* Excessive Collisions */
#define TDES0_STATUS_HRTBT_FAIL   0x00000080  /* Heartbeat Fail */
#define TDES0_STATUS_COLCNT_MASK  0x00000078  /* Collision Count Mask */
#define TDES0_STATUS_COLCNT_SHIFT 3           /* Collision Count Shift */
#define TDES0_STATUS_EX_DEF       0x00000004  /* Excessive Deferrals */
#define TDES0_STATUS_UF           0x00000002  /* Underflow Error */
#define TDES0_STATUS_DF           0x00000001  /* Deferred */

#define TDES1_CONTROL_IC          0x80000000  /* Interrupt on Completion */
#define TDES1_CONTROL_LS          0x40000000  /* Last Segment */
#define TDES1_CONTROL_FS          0x20000000  /* First Segment */
#define TDES1_CONTROL_AC          0x04000000  /* Add CRC Disable */
#define TDES1_CONTROL_DPD         0x00800000  /* Disable Padding */
