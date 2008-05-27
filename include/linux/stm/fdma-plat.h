/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Authors: Mark Glaisher <Mark.Glaisher@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

/* Memory section offsets from FDMA base address */
#define STB7100_FDMA_BASE 					0x19220000
#define STB7109_FDMA_BASE 					STB7100_FDMA_BASE

#define STB7100_DMEM_OFFSET            				0x8000     /* Contains the control word interface */
#define STB7100_IMEM_OFFSET            				0xC000     /* Contains config data */

#define STB7109_DMEM_OFFSET					STB7100_DMEM_OFFSET
#define STB7109_IMEM_OFFSET					STB7100_IMEM_OFFSET

#define STX7105_FDMA0_BASE					0xfe220000
#define STX7105_FDMA1_BASE					0xfe410000
#define STX7105_XBAR_BASE					0xfe420000

#define STX7105_IMEM_OFFSET					STB7100_IMEM_OFFSET
#define STX7105_DMEM_OFFSET					STB7100_DMEM_OFFSET

#define STX7111_FDMA0_BASE					0xfe220000
#define STX7111_FDMA1_BASE					0xfe410000
#define STX7111_XBAR_BASE					0xfe420000

#define STX7111_IMEM_OFFSET					STB7100_IMEM_OFFSET
#define STX7111_DMEM_OFFSET					STB7100_DMEM_OFFSET

#define STB7200_FDMA0_BASE					0xFD810000
#define STB7200_FDMA1_BASE					0xFD820000
#define STB7200_XBAR_BASE					0xFD830000

#define STB7200_IMEM_OFFSET					STB7100_IMEM_OFFSET
#define STB7200_DMEM_OFFSET					STB7100_DMEM_OFFSET

#define IMEM_REGION_LENGTH					0xa00
#define DMEM_REGION_LENGTH					0x600

#define NODE_DATA_OFFSET					0x40
#define CMD_STAT_OFFSET       					0x04

#define FDMA2_ID						0x0000   /* Block Id */
#define FDAM2_VER						0x0004
#define FDMA2_ENABLE_REG					0x0008
#define FDMA2_CLOCKGATE						0x000C       /* Clock enable control */
#define FDMA2_REV_ID						0x8000

/*here our our current node params region */
#define STB7100_FDMA_CMD_STATn_REG 				0x8040 /* (+ n * 0x04) */
#define STB7100_FDMA_PTR_REG 					0x9180 /* (+ n * 0x40) */
#define STB7100_FDMA_COUNT_REG					0x9188 /* (+ n * 0x40) */
#define STB7100_FDMA_SADDR_REG					0x918c /* (+ n * 0x40) */
#define STB7100_FDMA_DADDR_REG					0x9190 /* (+ n * 0x40) */
#define STB7100_FDMA_REQ_CTLn_REG				0x9780 /* (+ n * 0x04) */

#define STB7109_FDMA_CMD_STATn_REG				0x9140 /* (+ n * 0x04) */
#define STB7109_FDMA_REQ_CTLn_REG				0x9180 /* (+ n * 0x04) */
#define STB7109_FDMA_PTR_REG					0x9400 /* (+ n * 0x40) */
#define STB7109_FDMA_COUNT_REG					0x9408 /* (+ n * 0x40) */
#define STB7109_FDMA_SADDR_REG					0x940c /* (+ n * 0x40) */
#define STB7109_FDMA_DADDR_REG					0x9410 /* (+ n * 0x40) */

#define STB7200_FDMA_CMD_STATn_REG				0x9140 /* (+ n * 0x04) */
#define STB7200_FDMA_REQ_CTLn_REG				0x9180 /* (+ n * 0x04) */
#define STB7200_FDMA_PTR_REG					0x9580 /* (+ n * 0x40) */
#define STB7200_FDMA_COUNT_REG					0x9588 /* (+ n * 0x40) */
#define STB7200_FDMA_SADDR_REG					0x958c /* (+ n * 0x40) */
#define STB7200_FDMA_DADDR_REG					0x9590 /* (+ n * 0x40) */

#define FDMA2_SYNCREG                 				0xBF88
#define FDMA2_CMD_MBOX_STAT_REG					0xBFC0
#define FDMA2_CMD_MBOX_SET_REG					0xBFC4
#define FDMA2_CMD_MBOX_CLR_REG					0xBFC8
#define FDMA2_CMD_MBOX_MASK_REG 				0xBFCC

#define FDMA2_INT_STAT_REG					0xBFD0
#define FDMA2_INT_SET_REG					0xBFD4
#define FDMA2_INT_CLR_REG					0xBFD8
#define FDMA2_INT_MASK_REG					0xBFDC

#define LINUX_FDMA_STB7100_IRQ_VECT			140
#define LINUX_FDMA_STB7109_IRQ_VECT			LINUX_FDMA_STB7100_IRQ_VECT
#define LINUX_FDMA0_STX7105_IRQ_VECT			evt2irq(0x1380)
#define LINUX_FDMA1_STX7105_IRQ_VECT			evt2irq(0x13a0)
#define LINUX_FDMA0_STX7111_IRQ_VECT			evt2irq(0x1380)
#define LINUX_FDMA1_STX7111_IRQ_VECT			evt2irq(0x13a0)
#define LINUX_FDMA0_STB7200_IRQ_VECT			ILC_IRQ(13)
#define LINUX_FDMA1_STB7200_IRQ_VECT			ILC_IRQ(15)

struct fdma_regs
{
	unsigned long fdma_id;
	unsigned long fdma_ver;
	unsigned long fdma_en;
	unsigned long fdma_rev_id;
	unsigned long fdma_cmd_statn;
	unsigned long fdma_ptrn;
	unsigned long fdma_cntn;
	unsigned long fdma_saddrn;
	unsigned long fdma_daddrn;
	unsigned long fdma_req_ctln;
	unsigned long fdma_cmd_sta;
	unsigned long fdma_cmd_set;
	unsigned long fdma_cmd_clr;
	unsigned long fdma_cmd_mask;
	unsigned long fdma_int_sta;
	unsigned long fdma_int_set;
	unsigned long fdma_int_clr;
	unsigned long fdma_int_mask;
	unsigned long fdma_sync_reg;
	unsigned long fdma_clk_gate;
	unsigned long fdma_imem_region;
	unsigned long fdma_dmem_region;
};

struct fdma_fw {
	unsigned long * data_reg;
	unsigned long * imem_reg;
	unsigned long imem_fw_sz;
	unsigned long dmem_fw_sz;
	unsigned long imem_len;
	unsigned long dmem_len;
};

struct fdma_platform_device_data {
	struct fdma_regs *registers_ptr;
	int    min_ch_num;
	int    max_ch_num;
	char  * fw_device_name;
	struct fdma_fw fw;
	//struct fdma_dev *fd;
};
