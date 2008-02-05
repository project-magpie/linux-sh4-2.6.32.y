/* 
 * GMAC header file
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 */

/* --- GMAC BLOCK defines --- */
#define GMAC_CONTROL		0x00000000	/* Configuration */
#define GMAC_FRAME_FILTER	0x00000004	/* Frame Filter */
#define GMAC_HASH_HIGH		0x00000008	/* Multicast Hash Table High */
#define GMAC_HASH_LOW		0x0000000c	/* Multicast Hash Table Low */
#define GMAC_MII_ADDR		0x00000010	/* MII Address */
#define GMAC_MII_DATA		0x00000014	/* MII Data */
#define GMAC_FLOW_CTRL		0x00000018	/* Flow Control */
#define GMAC_VLAN		0x0000001c	/* VLAN Tag */

/* GMAC ID */
#define GMAC_VERSION	0x00000020	/* GMAC CORE Version */

#define GMAC_INT_STATUS	0x00000038 /* interrupt status register */
#define GMAC_INT_MASK	0x0000003c /* interrupt status register */

/* GMAC HW ADDR regs */
#define GMAC_ADDR_HIGH	0x00000040	/* Mac Address 0 HI */
#define GMAC_ADDR_LOW	0x00000044	/* Mac Address 0 LO */
#define GMAC_ADDRs_HIGH(reg)		(0x00000048+((reg-1)*8))
#define GMAC_ADDRs_LOW(reg)		(0x0000004c+((reg-1)*8))

#define GMAC_AN_CTRL	0x000000c0	/* AN control */
#define GMAC_AN_STATUS	0x000000c4	/* AN status */
#define GMAC_ANE_ADV	0x000000c8	/* Auto-Neg. Advertisement */
#define GMAC_ANE_LINK	0x000000cc	/* Auto-Neg. link partener ability */
#define GMAC_ANE_EXP	0x000000d0	/* ANE expansion */
#define GMAC_TBI	0x000000d4	/* TBI extend status */
#define GMAC_GMII_STATUS 0x000000d8	/* S/R-GMII status */
/* GMAC Configuration defines */
#define GMAC_CONTROL_WD	0x00800000	/* Disable Watchdog */
#define GMAC_CONTROL_JD	0x00400000	/* Jabber disable */
#define GMAC_CONTROL_BE	0x00200000	/* Frame Burst Enable */
#define GMAC_CONTROL_JE	0x00100000	/* Jumbo frame */
#define GMAC_CONTROL_IFG_88	0x00040000
#define GMAC_CONTROL_IFG_80	0x00020000
#define GMAC_CONTROL_IFG_40	0x000e0000
#define GMAC_CONTROL_PS		0x00008000	/* Port Select 0:GMI 1:MII */
#define GMAC_CONTROL_FES	0x00004000	/* Speed 0:10 1:100 */
#define GMAC_CONTROL_DO		0x00002000	/* Disable Rx Own */
#define GMAC_CONTROL_LM		0x00001000	/* Loop-back mode */
#define GMAC_CONTROL_DM		0x00000800	/* Duplex Mode */
#define GMAC_CONTROL_IPC	0x00000400	/* Checksum Offload */
#define GMAC_CONTROL_DR		0x00000200	/* Disable Retry */
#define GMAC_CONTROL_LUD	0x00000100	/* Link up/down */
#define GMAC_CONTROL_ACS	0x00000080	/* Automatic Pad Stripping */
#define GMAC_CONTROL_DC		0x00000010	/* Deferral Check */
#define GMAC_CONTROL_TE		0x00000008	/* Transmitter Enable */
#define GMAC_CONTROL_RE		0x00000004	/* Receiver Enable */

#define GMAC_CORE_INIT (GMAC_CONTROL_PS | GMAC_CONTROL_ACS | GMAC_CONTROL_IPC)

/* GMAC Frame Filter defines */
#define GMAC_FRAME_FILTER_PR	0x00000001	/* Promiscuous Mode */
#define GMAC_FRAME_FILTER_HUC	0x00000002	/*Hash Unicast */
#define GMAC_FRAME_FILTER_HMC	0x00000004	/*Hash Multicast */
#define GMAC_FRAME_FILTER_DAIF	0x00000008	/*DA Inverse Filtering */
#define GMAC_FRAME_FILTER_PM	0x00000010	/*Pass all multicast */
#define GMAC_FRAME_FILTER_DBF	0x00000020	/*Disable Broadcast frames */
#define GMAC_FRAME_FILTER_RA	0x80000000	/*Receive all mode */
/* GMII ADDR  defines */
#define GMAC_MII_ADDR_WRITE	0x00000002	/* MII Write */
#define GMAC_MII_ADDR_BUSY	0x00000001	/* MII Busy */
/* GMAC FLOW CTRL defines */
#define GMAC_FLOW_CTRL_PT_MASK	0xffff0000	/* Pause Time Mask */
#define GMAC_FLOW_CTRL_PT_SHIFT	16
#define GMAC_FLOW_CTRL_RFE	0x00000004	/* Rx Flow Control Enable */
#define GMAC_FLOW_CTRL_TFE	0x00000002	/* Tx Flow Control Enable */
#define GMAC_FLOW_CTRL_FCB_BPA	0x00000001	/* Flow Control Busy ... */

/*--- DMA BLOCK defines ---*/
/* DMA CRS Control and Status Register Mapping */
#define DMA_HOST_TX_DESC	  0x00001048	/* Current Host Tx descriptor */
#define DMA_HOST_RX_DESC	  0x0000104c	/* Current Host Rx descriptor */
/*  DMA Bus Mode register defines */
#define DMA_BUS_PR_RATIO_MASK	  0x0000c000	/* Rx/Tx priority ratio */
#define DMA_BUS_PR_RATIO_SHIFT	  14
#define DMA_BUS_FB	  	  0x00010000	/* Fixed Burst */

/* DMA Status register defines */
#define DMA_STATUS_GPI		0x10000000	/* PMT interrupt */
#define DMA_STATUS_GMI		0x08000000	/* MMC interrupt */
#define DMA_STATUS_GLI		0x04000000	/* GMAC Line interface interrupt */

/* DMA operation mode defines */
#define DMA_CONTROL_SF		0x00200000	/* Store And Forward */
#define DMA_CONTROL_FTF		0x00100000	/* Flush transmit FIFO */
#define DMA_CONTROL_TTC_MASK	0x0001c000	/* Transmit Threshold Control */
#define DMA_CONTROL_TTC_64	0x00000000
#define DMA_CONTROL_TTC_128	0x00040000
#define DMA_CONTROL_TTC_192	0x00080000
#define DMA_CONTROL_TTC_256	0x000c0000
#define DMA_CONTROL_TTC_40	0x00100000
#define DMA_CONTROL_TTC_32	0x00140000
#define DMA_CONTROL_TTC_24	0x00180000
#define DMA_CONTROL_TTC_16	0x001c0000

/* --- Descriptor defines --- */
/* Receive Descriptor 0*/
#define RDES0_STATUS_FILTER_FAIL  0x40000000	/* DA Filtering Fails */
#define RDES0_STATUS_FL_MASK      0x3fff0000	/* Frame Length Mask */
#define RDES0_STATUS_FL_SHIFT     16	/* Frame Length Shift */
#define RDES0_STATUS_DE		0x00004000	/* Descriptor Error */
#define RDES0_STATUS_SAF	0x00002000	/* Source Address filter Fail */
#define RDES0_STATUS_LENGTH_ERROR 0x00001000	/* Length Error */
#define RDES0_STATUS_OE		0x00000800	/* Overflow Error */
#define RDES0_STATUS_VLAN	0x00000400	/* VLAN tag */
#define RDES0_STATUS_IPC	0x00000080	/* Checksum Error */
#define RDES0_STATUS_LC		0x00000040	/* Collision Seen */
#define RDES0_STATUS_FT		0x00000020	/* Frame Type */
#define RDES0_STATUS_RWT	0x00000010	/* Receive Watchdog */
#define RDES0_STATUS_RE		0x00000008	/* Receive Error  */
#define RDES0_STATUS_DRIBBLE	0x00000004	/* Dribbling Bit */
#define RDES0_STATUS_CE		0x00000002	/* CRC Error */
#define RDES0_STATUS_RX_GMAC_ADDR 0x00000001	/* RX GMAC ADDR. */

/* Transmit Descriptor */
#define TDES0_STATUS_JT		  0x00004000	/* jabber timeout */
#define TDES0_STATUS_FF		  0x00002000	/* frame flushed */
#define TDES0_STATUS_LOSS_CARRIER 0x00000800	/* Loss of Carrier */
#define TDES0_STATUS_NO_CARRIER   0x00000400	/* No Carrier */
#define TDES0_STATUS_LATE_COL     0x00000200	/* Late Collision */
#define TDES0_STATUS_EX_COL	0x00000100	/* Excessive Collisions */
#define TDES0_STATUS_VLAN	0x00000080	/* VLAN FRAME */
#define TDES0_STATUS_COLCNT_MASK  0x00000078	/* Collision Count Mask */
#define TDES0_STATUS_COLCNT_SHIFT 3	/* Collision Count Shift */
#define TDES0_STATUS_EX_DEF	0x00000004	/* Excessive Deferrals */
#define TDES0_STATUS_UF		0x00000002	/* Underflow Error */
#define TDES0_STATUS_DF		0x00000001	/* Deferred */
