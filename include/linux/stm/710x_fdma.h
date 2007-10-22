/*
 *  STb710x FDMA Driver
 *  Copyright (c) 2005 STMicroelectronics Limited.
 *  Author: Mark Glaisher <Mark.Glaisher@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef STB7100_FDMA_H
#define STB7100_FDMA_H

#if ! defined(CONFIG_STM_DMA)
	#define CONFIG_MAX_STM_DMA_CHANNEL_NR 0
	#define CONFIG_MIN_STM_DMA_CHANNEL_NR 0
#endif

#include <linux/interrupt.h>

#include <linux/device.h>
#include <linux/firmware.h>

#include <linux/dmapool.h>
#include <linux/stm/stm-dma.h>

/* Memory section offsets from FDMA base address */
#define STB7100_FDMA_BASE 					0x19220000
#define STB7109_FDMA_BASE 					STB7100_FDMA_BASE

#define STB7100_DMEM_OFFSET            				0x8000     /* Contains the control word interface */
#define STB7100_IMEM_OFFSET            				0xC000     /* Contains config data */

#define STB7109_DMEM_OFFSET					STB7100_DMEM_OFFSET
#define STB7109_IMEM_OFFSET					STB7100_IMEM_OFFSET

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
#define STB7100_FDMA_CMD_STATn_REG 				0x8040 /*(+ n *0x04) */
#define STB7100_FDMA_PTR_REG 					0x9180 /*(+ n * 0x40)*/
#define STB7100_FDMA_COUNT_REG					0x9188/* (+ n * 0x40)*/
#define STB7100_FDMA_SADDR_REG					0x918c/* (+ n * 0x40)*/
#define STB7100_FDMA_DADDR_REG					0x9190/* (+ n * 0x40)*/
#define STB7100_FDMA_REQ_CTLn_REG				0x9780 /*(+ n *0x04) */

#define STB7109_FDMA_CMD_STATn_REG				0x9140/* (+ n *0x04) */
#define STB7109_FDMA_PTR_REG					0x9400 /*(+ n * 0x40)*/
#define STB7109_FDMA_COUNT_REG					0x9408 /*(+ n * 0x40)*/
#define STB7109_FDMA_SADDR_REG					0x940c /*(+ n * 0x40)*/
#define STB7109_FDMA_DADDR_REG					0x9410 /*(+ n * 0x40)*/
#define STB7109_FDMA_REQ_CTLn_REG				0x9180/* (+ n *0x04) */

#define FDMA2_SYNCREG                 				0xBF88
#define FDMA2_CMD_MBOX_STAT_REG					0xBFC0
#define FDMA2_CMD_MBOX_SET_REG					0xBFC4
#define FDMA2_CMD_MBOX_CLR_REG					0xBFC8
#define FDMA2_CMD_MBOX_MASK_REG 				0xBFCC

#define FDMA2_INT_STAT_REG					0xBFD0
#define FDMA2_INT_SET_REG					0xBFD4
#define FDMA2_INT_CLR_REG					0xBFD8
#define FDMA2_INT_MASK_REG					0xBFDC


#define CHANNEL_NOFLUSH 				0
#define CHANNEL_FLUSH   				1
#define CHAN_ALL_ENABLE 				3

/**cmd stat vals*/
#define SET_NODE_COMP_PAUSE		    		1 <<30
#define SET_NODE_COMP_IRQ				1 <<31
#define NODE_ADDR_STATIC 				0x01
#define NODE_ADDR_INCR	 				0x02

#define SOURCE_ADDR 					0x05
#define DEST_ADDR   					0x07

#define CMDSTAT_FDMA_START_CHANNEL  			1
#define CMDSTAT_FDMA_PAUSE_CHANNEL  			3


#define LINUX_FDMA_STB7100_IRQ_VECT			140
#define LINUX_FDMA_STB7109_IRQ_VECT			LINUX_FDMA_STB7100_IRQ_VECT
#define STB7100_FDMA_CHANS              		16
#define STB7109_FDMA_CHANS              		STB7100_FDMA_CHANS

#define FDMA_COMPLETE_OK				0
#define FDMA_COMPLETE_ERR				1

/*******************************/
/*MBOX SETUP VALUES*/

#define MBOX_CMD_PAUSE_FLUSH_CHANNEL 			3
#define MBOX_CMD_PAUSE_CHANNEL		 		2
#define MBOX_CMD_START_CHANNEL       			1
#define CLEAR_WORD					0XFFFFFFFF

#define IS_PACED_CHANNEL_SET(flags)(flags & 0x1f)
#define ASSERT_NODE_BUS_ADDR(addr)( (((PXSEG(addr) == P0SEG) && addr))?1:0)
#define IS_CHANNEL_PAUSED(ch)(stb710x_get_engine_status(ch)== FDMA_CHANNEL_PAUSED ?1:0)
#define IS_CHANNEL_RUNNING(ch)(stb710x_get_engine_status(ch)== FDMA_CHANNEL_RUNNING ?1:0)
#define IS_CHANNEL_IDLE(ch)(stb710x_get_engine_status(ch)== FDMA_CHANNEL_IDLE ?1:0)
#define IS_TRANSFER_SG(parms)((MODE_SRC_SCATTER==parms->mode)||(MODE_DST_SCATTER==parms->mode )?1:0)
#define MBOX_STR_CMD(ch) (MBOX_CMD_START_CHANNEL << (ch*2))
#define CHAN_OTB(ch_num)( ((ch_num >= chip.ch_min) && (ch_num <= chip.ch_max)) ? 1:0 )
#define IS_CHANNEL_RESERVED(ch)(chip.channel[ch].reserved==1)
#define CMD_STAT_REG(ch)(chip.io_base + chip.regs.fdma_cmd_statn + (ch * CMD_STAT_OFFSET))
#define CH_PTR_REG(ch)(chip.io_base + chip.regs.fdma_ptrn  + (ch * CMD_STAT_OFFSET))

#define IS_NODE_MALLOCED(priv)((priv.node!=0))


#define IS_NODELIST_EQUAL(priv)((priv.sublist_nents == priv.alloced_nents))

typedef void (*pf)(void * data);

#define CHANNEL_ERR_IRQ 		3
#define CHANNEL_IRQ     		1

#define FDMA_CHANNEL_IDLE 		0
#define FDMA_CHANNEL_RUNNING 		2
#define FDMA_CHANNEL_PAUSED 		3

/*FDMA Channel FLAGS*/
/*values below D28 are reserved for REQ_LINE parameter*/
#define REQ_LINE_MASK 	0x1f

#define CHAN_NUM(chan) ((chan) - chip.channel)

typedef struct fdma_fw_s {
	unsigned long * data_reg;
	unsigned long * imem_reg;
	unsigned long imem_fw_sz;
	unsigned long dmem_fw_sz;
	unsigned long imem_len;
	unsigned long dmem_len;
}fdma_fw_data_t;

typedef struct fdma_platform_device_data {
	void * req_line_tbl_adr;
	void * registers_ptr;
	int    cpu_subtype;
	int    cpu_rev;
	int    min_ch_num;
	int    max_ch_num;
	int    nr_reqlines;
	char  * fw_device_name;
	unsigned long fdma_base;
	unsigned long irq_vect;
	fdma_fw_data_t fw;
}fdma_platform_device_data;

typedef struct fdma_llu_entry {
	u32 next_item;
	u32 control;
	u32 size_bytes;
	u32 saddr;
	u32 daddr;
	u32 line_len;
	u32 sstride;
	u32 dstride;
}fdma_llu_entry;

typedef struct channel_status{
	char	ch_term;
	char	ch_pause;
	char	is_xferring;
	char 	reserved;
	char 	callback_only;

	pf 	comp_cb;
	void	*comp_cb_param;
	int	comp_cb_isr;

	pf	err_cb;
	void	*err_cb_param;
	int	err_cb_isr;

	struct  dma_channel * cur_cfg;
	struct stm_dma_params params;
	struct tasklet_struct fdma_complete;
	struct tasklet_struct fdma_error;
}channel_status;

typedef struct fdma_regs_s
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
}fdma_regs_t;

typedef struct fdma_chip{
	channel_status			channel[CONFIG_MAX_STM_DMA_CHANNEL_NR +1];
	spinlock_t 			fdma_lock;
	spinlock_t 			channel_lock;
	wait_queue_head_t   		irq_check;
	u32				io_base;
	u32				firmware_loaded;
	u8				num_req_lines;
	u8				ch_min;
	u8 				ch_max;
	u8				irq_val;
	u32                     	cpu_subtype;
    	u32				cpu_rev;
	struct dma_pool 		*llu_pool;
	wait_queue_head_t		fw_load_q;
	struct device 			dev;
	struct 	platform_device 	*platform_dev;
	fdma_regs_t  			regs;
	fdmareq_RequestConfig_t		*req_tbl;
	int				irq_enable_ok ;
	struct dma_info 		*info;
	char *				fw_name;
	fdma_fw_data_t			fw;
}fdma_chip;

/*---- Constants for use in defining the request signals -----*/

/* Access */
#define ENABLE_FLG      1
#define DISABLE_FLG     0

/* Access */
#define READ            0
#define WRITE           1

/* Opcodes */
#define OPCODE_1        0x00
#define OPCODE_2        0x01
#define OPCODE_4        0x02
#define OPCODE_8        0x03
#define OPCODE_16       0x04
#define OPCODE_32       0x05
/* Increment Size */
#define INCSIZE_0       0
#define INCSIZE_4       4
#define INCSIZE_8       8
#define INCSIZE_16      16
#define INCSIZE_32      32

/*STBUS Initiator Target*/
#define STBUS_INT1 1
#define STBUS_INT0 0

/*RQ sample holdoff time microseconds*/
#define HOLDOFF_0US 0
#define HOLDOFF_1US 1
#define HOLDOFF_2US 2


/* Utility values */
#define UNUSED       	0xff

typedef enum __stb7100_fdma_req_ids {
/*0*/	STB7100_FDMA_REQ_SPDIF_TEST =	0,
/*1*/	STB7100_FDMA_REQ_NOT_CONN_1,
/*2*/	STB7100_FDMA_REQ_NOT_CONN_2,
/*3*/	STB7100_FDMA_REQ_VIDEO_HDMI,
/*4*/	STB7100_FDMA_REQ_DISEQC_HALF_EMPTY,
/*5*/	STB7100_FDMA_REQ_DISEQC_HALF_FULL,
/*6*/	STB7100_FDMA_REQ_SH4_SCIF_RX,
/*7*/	STB7100_FDMA_REQ_SH4_SCIF_TX,
/*8*/	STB7100_FDMA_REQ_SSC_0_RX,
/*9*/	STB7100_FDMA_REQ_SSC_1_RX,
/*10*/	STB7100_FDMA_REQ_SSC_2_RX,
/*11*/	STB7100_FDMA_REQ_SSC_0_TX,
/*12*/	STB7100_FDMA_REQ_SSC_1_TX,
/*13*/	STB7100_FDMA_REQ_SSC_2_TX,
/*14*/	STB7100_FDMA_REQ_UART_0_RX,
/*15*/	STB7100_FDMA_REQ_UART_1_RX,
/*16*/	STB7100_FDMA_REQ_UART_2_RX,
/*17*/	STB7100_FDMA_REQ_UART_3_RX,
/*18*/	STB7100_FDMA_REQ_UART_0_TX,
/*19*/	STB7100_FDMA_REQ_UART_1_TX,
/*20*/	STB7100_FDMA_REQ_UART_2_TX,
/*21*/	STB7100_FDMA_REQ_UART_3_TX,
/*22*/	STB7100_FDMA_REQ_EXT_PIO_0,
/*23*/	STB7100_FDMA_REQ_EXT_PIO_1,
/*24*/	STB7100_FDMA_REQ_CPXM_DECRYPT,
/*25*/	STB7100_FDMA_REQ_CPXM_ENCRYPT,
/*26*/	STB7100_FDMA_REQ_PCM_0,
/*27*/	STB7100_FDMA_REQ_PCM_1,
/*28*/	STB7100_FDMA_REQ_PCM_READ,
/*29*/	STB7100_FDMA_REQ_SPDIF,
/*30*/	STB7100_FDMA_REQ_SWTS,
/*31*/	STB7100_FDMA_REQ_UNUSED
}stb7100_fdma_req_ids;

typedef enum __stb7109_fdma_reqids {
	STB7109_FDMA_REQ_UNUSED =0,//0
	STB7109_FDMA_DMA_REQ_HDMI_AVI,
	STB7109_FDMA_REQ_DISEQC_HALF_EMPTY,
	STB7109_FDMA_REQ_DISEQC_HALF_FULL,
	STB7109_FDMA_REQ_SH4_SCIF_RX,
	STB7109_FDMA_REQ_SH4_SCIF_TX,//5
	STB7109_FDMA_REQ_SSC_0_RX,//6-8
	STB7109_FDMA_REQ_SSC_1_RX,
	STB7109_FDMA_REQ_SSC_2_RX,
	STB7109_FDMA_REQ_SSC_0_TX,//9-11
	STB7109_FDMA_REQ_SSC_1_TX,
	STB7109_FDMA_REQ_SSC_2_TX,
	STB7109_FDMA_REQ_UART_0_RX,//12-15
	STB7109_FDMA_REQ_UART_1_RX,
	STB7109_FDMA_REQ_UART_2_RX,
	STB7109_FDMA_REQ_UART_3_RX,
	STB7109_FDMA_REQ_UART_0_TX,//16-19
	STB7109_FDMA_REQ_UART_1_TX,
	STB7109_FDMA_REQ_UART_2_TX,
	STB7109_FDMA_REQ_UART_3_TX,
	STB7109_FDMA_REQ_REQ_EXT_PIO_0,//20
	STB7109_FDMA_REQ_REQ_EXT_PIO_1,//21
	STB7109_FDMA_REQ_CPXM_DECRYPT,
	STB7109_FDMA_REQ_CPXM_ENCRYPT,
	STB7109_FDMA_REQ_PCM_0=24,//24
	STB7109_FDMA_REQ_PCM_1,
	STB7109_FDMA_REQ_PCM_READ,
	STB7109_FDMA_REQ_SPDIF,
	STB7109_FDMA_REQ_SWTS_0,
	STB7109_FDMA_REQ_SWTS_1,
	STB7109_FDMA_REQ_SWTS_2
}stb7109_fdma_req_ids;


typedef volatile unsigned long device_t;

#define DUMP_FDMA_CHANNEL(chan)\
	(fdma_log("CHANNEL%d is \n CMD_STAT %x\n PTR %x\n CNT %x\n SADDR %x\n DADDR %x\n REQ_CTL %x\n", \
		chan, \
		(int)readl((chip.io_base + CMD_STAT_OFFSET * chan) + chip.regs.fdma_cmd_statn), \
		(int)readl((chip.io_base + NODE_DATA_OFFSET * chan) + chip.regs.fdma_ptrn), \
		(int)readl((chip.io_base + NODE_DATA_OFFSET * chan) + chip.regs.fdma_cntn), \
		(int)readl((chip.io_base + NODE_DATA_OFFSET * chan) + chip.regs.fdma_saddrn), \
		(int)readl((chip.io_base + NODE_DATA_OFFSET * chan) + chip.regs.fdma_daddrn), \
		(int)readl((chip.io_base + CMD_STAT_OFFSET * chan) +chip.regs.fdma_req_ctln)))

#define DUMP_FDMA_INTERFACE()(\
	fdma_log(" FDMA_CMD_STA %x\n FDMA_CMD_SET %x\n FDMA_CMD_CLR %x\n FDMA_CMD_MASK %x\n FDMA_INT_STA %x\n FDMA_INT_SET %x\n FDMA_INT_CLR %x\n FDMA_INT_MASK %x\n", \
		(int)readl(chip.io_base + chip.regs.fdma_cmd_sta),\
		(int)readl(chip.io_base + chip.regs.fdma_cmd_set),\
		(int)readl(chip.io_base + chip.regs.fdma_cmd_clr),\
		(int)readl(chip.io_base + chip.regs.fdma_cmd_mask),\
		(int)readl(chip.io_base + chip.regs.fdma_int_sta),\
		(int)readl(chip.io_base + chip.regs.fdma_int_set),\
		(int)readl(chip.io_base + chip.regs.fdma_int_clr),\
		(int)readl(chip.io_base + chip.regs.fdma_int_mask)))


#define DUMP_NODE_FROM_EXTMEM(addr)(\
	printk(" %s\n ADDR %x\n NEXT %x\n CTL %x\n NBYTES %x\n SADDR %x\n DADDR %x\n NODELEN %x\n SSTRIDE %x\n DSTRIDE %x\n\n",\
		__FUNCTION__,\
		(int)(addr),\
		(int)readl(addr), \
		(int)readl(addr+0x04),\
		(int)readl(addr+0x08),\
		(int)readl(addr+0xc),\
		(int)readl(addr+0x10),\
		(int)readl(addr+0x14),\
		(int)readl(addr+0x18),\
		(int)readl(addr+0x1c)))

#define DUMP_FDMA_REG_OFFSETS(chip) (\
	fdma_log(" ID %x\n VER %x\n EN %x\n REV_ID %x\n CMD_STAT %x\n PTRN %x\n CNTn %x\n SADDR %x\n DADDR %x\n REQ_CTL %x\n CMD_STA %x\n CMD_SET %x\n CMD_CLR %x\n CMD_MASK %x\n INT_STA %x\n INT_SET %x\n INT_CLR %x\n INT_MASK %x\n SYNC %x\n CLK %x\n IMEM %x\n DMEM %x\n", \
		(u32)(chip.io_base +chip.regs.fdma_id), \
		(u32)(chip.io_base +chip.regs.fdma_ver), \
	        (u32)(chip.io_base +chip.regs.fdma_en), \
       		(u32)(chip.io_base +chip.regs.fdma_rev_id),\
        	(u32)(chip.io_base +chip.regs.fdma_cmd_statn), \
        	(u32)(chip.io_base +chip.regs.fdma_ptrn), \
        	(u32)(chip.io_base +chip.regs.fdma_cntn), \
        	(u32)(chip.io_base +chip.regs.fdma_saddrn), \
         	(u32)(chip.io_base +chip.regs.fdma_daddrn), \
         	(u32)(chip.io_base +chip.regs.fdma_req_ctln), \
         	(u32)(chip.io_base +chip.regs.fdma_cmd_sta), \
         	(u32)(chip.io_base +chip.regs.fdma_cmd_set), \
         	(u32)(chip.io_base +chip.regs.fdma_cmd_clr), \
         	(u32)(chip.io_base +chip.regs.fdma_cmd_mask), \
         	(u32)(chip.io_base +chip.regs.fdma_int_sta), \
         	(u32)(chip.io_base +chip.regs.fdma_int_set), \
         	(u32)(chip.io_base +chip.regs.fdma_int_clr), \
         	(u32)(chip.io_base +chip.regs.fdma_int_mask), \
         	(u32)(chip.io_base +chip.regs.fdma_sync_reg),\
         	(u32)(chip.io_base +chip.regs.fdma_clk_gate), \
         	(u32)(chip.io_base +chip.regs.fdma_imem_region), \
         	(u32)(chip.io_base +chip.regs.fdma_dmem_region)))

static inline void walk_nodelist(struct fdma_llu_entry * first_node)
{
	struct fdma_llu_entry ** np = & first_node;
	do{
		DUMP_NODE_FROM_EXTMEM(*np);
	}while((*np =(struct fdma_llu_entry*) (*np)->next_item));
}

#endif
