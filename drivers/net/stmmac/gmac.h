/* 
 * GMAC header file
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 */

/* --- GMAC BLOCK defines --- */
#define MAC_CONTROL               0x00000000	/* Configuration */
#define MAC_FRAME_FILTER          0x00000004	/* Frame Filter */
#define MAC_HASH_HIGH             0x00000008	/* Multicast Hash Table High */
#define MAC_HASH_LOW              0x0000000c	/* Multicast Hash Table Low */
#define MAC_MII_ADDR             0x00000010	/* MII Address */
#define MAC_MII_DATA             0x00000014	/* MII Data */
#define MAC_FLOW_CONTROL          0x00000018	/* Flow Control */
#define MAC_VLAN                  0x0000001c	/* VLAN Tag */
#define MAC_VERSION               0x00000020	/* GMAC CORE Version */
#define MAC_WAKEUP_FILTER         0x00000028	/* Wake-up Frame Filter */
#define MAC_PMT 		   0x0000002c	/* PMT Control and Status */
/* To be verified */
/*#define MAC_ADDR_HIGH(reg)	  (0x00000040+(reg*8))
#define MAC_ADDR_LOW(reg)	  (0x00000044+(reg*8))*/
#define MAC_ADDR_HIGH   	   0x00000040	/* Mac Address 0 higher 16 bits */
#define MAC_ADDR_LOW		   0x00000044	/* Mac Address 0 lower 32 bits */
#define MAC_AN_CTRL		   0x000000c0	/* AN control */
#define MAC_AN_STATUS  	   0x000000c4	/* AN status */
#define MAC_ANE_ADV		   0x000000c8	/* Auto-Neg. Advertisement */
#define MAC_ANE_LINK		   0x000000cc	/* Auto-Neg. link partener ability */
#define MAC_ANE_EXP		   0x000000d0	/* ANE expansion */
#define MAC_TBI		   0x000000d4	/* TBI extend status */
#define MAC_GMII_STATUS 	   0x000000d8	/* S/R-GMII status */
/* GMAC Configuration defines */
#define MAC_CONTROL_WD            0x00800000	/* Disable Watchdog */
#define MAC_CONTROL_JD		   0x00400000	/* Jabber disable */
#define MAC_CONTROL_BE		   0x00200000	/* Frame Burst Enable */
#define MAC_CONTROL_JE		   0x00100000	/* Jumbo frame */
#define MAC_CONTROL_IFG_88	   0x00040000
#define MAC_CONTROL_IFG_80	   0x00020000
#define MAC_CONTROL_IFG_40	   0x000e0000
#define MAC_CONTROL_PS		   0x00008000	/* Port Select 0:GMI 1:MII */
#define MAC_CONTROL_FES	   0x00004000	/* Speed 0:10 1:100 */
#define MAC_CONTROL_DO		   0x00002000	/* Disable Rx Own */
#define MAC_CONTROL_LM		   0x00001000	/* Loop-back mode */
#define MAC_CONTROL_DM            0x00000800	/* Duplex Mode */
#define MAC_CONTROL_IPC	   0x00000400	/* Checksum Offload */
#define MAC_CONTROL_DR            0x00000200	/* Disable Retry */
#define MAC_CONTROL_LUD           0x00000100	/* Link up/down */
#define MAC_CONTROL_ACS          0x00000080	/* Automatic Pad Stripping */
#define MAC_CONTROL_DC            0x00000010	/* Deferral Check */
#define MAC_CONTROL_TE            0x00000008	/* Transmitter Enable */
#define MAC_CONTROL_RE            0x00000004	/* Receiver Enable */

#define MAC_CORE_INIT (MAC_CONTROL_JE | MAC_CONTROL_ACS | MAC_CONTROL_IPC)

/* GMAC Frame Filter defines */
#define MAC_FRAME_FILTER_PR	0x00000001	/* Promiscuous Mode */
#define MAC_FRAME_FILTER_HUC	0x00000002	/*Hash Unicast */
#define MAC_FRAME_FILTER_HMC	0x00000004	/*Hash Multicast */
#define MAC_FRAME_FILTER_DAIF	0x00000008	/*DA Inverse Filtering */
#define MAC_FRAME_FILTER_PM	0x00000010	/*Pass all multicast */
#define MAC_FRAME_FILTER_DBF	0x00000020	/*Disable Broadcast frames */
#define MAC_FRAME_FILTER_RA	0x80000000	/*Receive all mode */
/* GMII ADDR  defines */
#define MAC_MII_ADDR_WRITE        0x00000002	/* MII Write */
#define MAC_MII_ADDR_BUSY         0x00000001	/* MII Busy */
/* GMAC FLOW CTRL defines */
#define MAC_FLOW_CONTROL_PT_MASK  0xffff0000	/* Pause Time Mask */
#define MAC_FLOW_CONTROL_PT_SHIFT 16
#define MAC_FLOW_CONTROL_RFE      0x00000004	/* Rx Flow Control Enable */
#define MAC_FLOW_CONTROL_TFE      0x00000002	/* Tx Flow Control Enable */
#define MAC_FLOW_CONTROL_PAUSE    0x00000001	/* Flow Control Busy ... */

/*--- DMA BLOCK defines ---*/
/* DMA CRS Control and Status Register Mapping */
#define DMA_BUS_MODE              0x00001000	/* Bus Mode */
#define DMA_XMT_POLL_DEMAND       0x00001004	/* Transmit Poll Demand */
#define DMA_RCV_POLL_DEMAND       0x00001008	/* Received Poll Demand */
#define DMA_RCV_BASE_ADDR         0x0000100c	/* Receive List Base */
#define DMA_TX_BASE_ADDR          0x00001010	/* Transmit List Base */
#define DMA_STATUS                0x00001014	/* Status Register */
#define DMA_CONTROL               0x00001018	/* Operation Mode */
#define DMA_INTR_ENA              0x0000101c	/* Interrupt Enable */
#define DMA_MISSED_FRAME_CTR      0x00001020	/* Missed Frame Counter and Buffer Overflow Counter */
#define DMA_HOST_TX_DESC	  0x00001048	/* Current Host Tx descriptor */
#define DMA_HOST_RX_DESC	  0x0000104c	/* Current Host Rx descriptor */
#define DMA_CUR_TX_BUF_ADDR       0x00001050	/* Current Host Transmit Buffer */
#define DMA_CUR_RX_BUF_ADDR       0x00001054	/* Current Host Receive Buffer */
/*  DMA Bus Mode register defines */
#define DMA_BUS_MODE_SFT_RESET    0x00000001	/* Software Reset */
#define DMA_BUS_MODE_BAR_BUS      0x00000002	/* Bar-Bus Arbitration */
#define DMA_BUS_MODE_DSL_MASK     0x0000007c	/* Descriptor Skip Length */
#define DMA_BUS_MODE_DSL_SHIFT    2
#define DMA_BUS_MODE_PBL_MASK     0x00003f00	/* Programmable Burst Length */
#define DMA_BUS_MODE_PBL_SHIFT    8
#define DMA_BUS_PR_RATIO_MASK	  0x0000c000	/* Rx/Tx priority ratio */
#define DMA_BUS_PR_RATIO_SHIFT	  14
#define DMA_BUS_FB	  	  0x00010000	/* Fixed Burst */
#define DMA_BUS_MODE_DEFAULT      0x00000000
/* DMA Status register defines */
#define DMA_STATUS_GPI		0x10000000	/* PMT interrupt */
#define DMA_STATUS_GMI		0x08000000	/* MMC interrupt */
#define DMA_STATUS_GLI		0x04000000	/* GMAC Line interface interrupt */
#define DMA_STATUS_EB_MASK        0x00380000	/* Error Bits Mask */
#define DMA_STATUS_EB_TX_ABORT    0x00080000	/* Error Bits - TX Abort */
#define DMA_STATUS_EB_RX_ABORT    0x00100000	/* Error Bits - RX Abort */
#define DMA_STATUS_TS_MASK        0x00700000	/* Transmit Process State */
#define DMA_STATUS_TS_SHIFT       20
#define DMA_STATUS_RS_MASK        0x000e0000	/* Receive Process State */
#define DMA_STATUS_RS_SHIFT       17
#define DMA_STATUS_NIS            0x00010000	/* Normal Interrupt Summary */
#define DMA_STATUS_AIS            0x00008000	/* Abnormal Interrupt Summary */
#define DMA_STATUS_ERI            0x00004000	/* Early Receive Interrupt */
#define DMA_STATUS_FBI            0x00002000	/* Fatal Bus Error Interrupt */
#define DMA_STATUS_ETI            0x00000400	/* Early Transmit Interrupt */
#define DMA_STATUS_RWT            0x00000200	/* Receive Watchdog Timeout */
#define DMA_STATUS_RPS            0x00000100	/* Receive Process Stopped */
#define DMA_STATUS_RU             0x00000080	/* Receive Buffer Unavailable */
#define DMA_STATUS_RI             0x00000040	/* Receive Interrupt */
#define DMA_STATUS_UNF            0x00000020	/* Transmit Underflow */
#define DMA_STATUS_OVF            0x00000010	/* Receive Overflow */
#define DMA_STATUS_TJT            0x00000008	/* Transmit Jabber Timeout */
#define DMA_STATUS_TU             0x00000004	/* Transmit Buffer Unavailable */
#define DMA_STATUS_TPS            0x00000002	/* Transmit Process Stopped */
#define DMA_STATUS_TI             0x00000001	/* Transmit Interrupt */
/* DMA operation mode defines */
#define DMA_CONTROL_SF            0x00200000	/* Store And Forward */
#define DMA_CONTROL_FTF		  0x00100000	/* Flush transmit FIFO */
#define DMA_CONTROL_TTC_MASK      0x0001c000	/* Transmit Threshold Control */
#define DMA_CONTROL_ST            0x00002000	/* Start/Stop Transmission */
#define DMA_CONTROL_SR            0x00000002	/* Start/Stop Receive */
/* DMA Interrupt Enable register defines */
#define DMA_INTR_ENA_NIE          0x00010000	/* Normal Interrupt Summary */
#define DMA_INTR_ENA_AIE          0x00008000	/* Abnormal Interrupt Summary */
#define DMA_INTR_ENA_ERE          0x00004000	/* Early Receive */
#define DMA_INTR_ENA_FBE          0x00002000	/* Fatal Bus Error */
#define DMA_INTR_ENA_ETE          0x00000400	/* Early Transmit */
#define DMA_INTR_ENA_RWE          0x00000200	/* Receive Watchdog */
#define DMA_INTR_ENA_RSE          0x00000100	/* Receive Stopped */
#define DMA_INTR_ENA_RUE          0x00000080	/* Receive Buffer Unavailable */
#define DMA_INTR_ENA_RIE          0x00000040	/* Receive Interrupt */
#define DMA_INTR_ENA_UNE          0x00000020	/* Underflow */
#define DMA_INTR_ENA_OVE          0x00000010	/* Receive Overflow */
#define DMA_INTR_ENA_TJE          0x00000008	/* Transmit Jabber */
#define DMA_INTR_ENA_TUE          0x00000004	/* Transmit Buffer Unavailable */
#define DMA_INTR_ENA_TSE          0x00000002	/* Transmit Stopped */
#define DMA_INTR_ENA_TIE          0x00000001	/* Transmit Interrupt */
/* DMA default interrupt mask */
#define DMA_INTR_DEFAULT_MASK 	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | \
				DMA_INTR_ENA_TIE)
#define DMA_INTR_NO_RX		(DMA_INTR_ENA_NIE | DMA_INTR_ENA_TIE)

/* --- Descriptor defines --- */
/* Common fields */
#define OWN_BIT			0x80000000	/* Own Bit (owned by hardware) */
#define DES1_CONTROL_CH		0x01000000	/* Second Address Chained */
#define DES1_CONTROL_TER	0x02000000	/* End of Ring */
#define DES1_RBS2_SIZE_MASK	0x003ff800	/* Buffer 2 Size Mask */
#define DES1_RBS2_SIZE_SHIFT	11	/* Buffer 2 Size Shift */
#define DES1_RBS1_SIZE_MASK	0x000007ff	/* Buffer 1 Size Mask */
#define DES1_RBS1_SIZE_SHIFT	0	/* Buffer 1 Size Shift */

/* Receive Descriptor 1*/
#define RDES1_CONTROL_DIOC        0x80000000	/* Disable Intr On Completion */
/* Receive Descriptor 0*/
#define RDES0_STATUS_FILTER_FAIL  0x40000000	/* DA Filtering Fails */
#define RDES0_STATUS_FL_MASK      0x3fff0000	/* Frame Length Mask */
#define RDES0_STATUS_FL_SHIFT     16	/* Frame Length Shift */
#define RDES0_STATUS_ES           0x00008000	/* Error Summary */
#define RDES0_STATUS_DE           0x00004000	/* Descriptor Error */
#define RDES0_STATUS_SAF          0x00002000	/* Source Address filter Fail */
#define RDES0_STATUS_LENGTH_ERROR 0x00001000	/* Length Error */
#define RDES0_STATUS_OE     0x00000800	/* Overflow Error */
#define RDES0_STATUS_VLAN 0x00000400	/* VLAN tag */
#define RDES0_STATUS_FS           0x00000200	/* First Descriptor */
#define RDES0_STATUS_LS           0x00000100	/* Last Descriptor */
#define RDES0_STATUS_IPC           0x00000080	/* Checksum Error */
#define RDES0_STATUS_LC     0x00000040	/* Collision Seen */
#define RDES0_STATUS_FT     0x00000020	/* Frame Type */
#define RDES0_STATUS_RWT  0x00000010	/* Receive Watchdog */
#define RDES0_STATUS_RE      0x00000008	/* Receive Error  */
#define RDES0_STATUS_DRIBBLE      0x00000004	/* Dribbling Bit */
#define RDES0_STATUS_CE           0x00000002	/* CRC Error */
#define RDES0_STATUS_RX_MAC_ADDR            0x00000001	/* RX MAC ADDR. */

/* Transmit Descriptor */
#define TDES0_STATUS_ES		  0x00008000	/* Error Summary */
#define TDES0_STATUS_JT		  0x00004000	/* jabber timeout */
#define TDES0_STATUS_FF		  0x00002000	/* frame flushed */
#define TDES0_STATUS_LOSS_CARRIER 0x00000800	/* Loss of Carrier */
#define TDES0_STATUS_NO_CARRIER   0x00000400	/* No Carrier */
#define TDES0_STATUS_LATE_COL     0x00000200	/* Late Collision */
#define TDES0_STATUS_EX_COL       0x00000100	/* Excessive Collisions */
#define TDES0_STATUS_VLAN   0x00000080	/* VLAN FRAME */
#define TDES0_STATUS_COLCNT_MASK  0x00000078	/* Collision Count Mask */
#define TDES0_STATUS_COLCNT_SHIFT 3	/* Collision Count Shift */
#define TDES0_STATUS_EX_DEF       0x00000004	/* Excessive Deferrals */
#define TDES0_STATUS_UF           0x00000002	/* Underflow Error */
#define TDES0_STATUS_DF           0x00000001	/* Deferred */
#define TDES1_CONTROL_IC          0x80000000	/* Interrupt on Completion */
#define TDES1_CONTROL_LS          0x40000000	/* Last Segment */
#define TDES1_CONTROL_FS          0x20000000	/* First Segment */
#define TDES1_CONTROL_AC          0x04000000	/* Add CRC Disable */
#define TDES1_CONTROL_DPD         0x00800000	/* Disable Padding */

#define STMMAC_FULL_DUPLEX MAC_CONTROL_DM
#define STMMAC_PORT	   MAC_CONTROL_PS
#define STMMAC_SPEED_100   MAC_CONTROL_FES
