/*
 * Copyright (C) 2005,7 STMicroelectronics Limited
 * Authors: Mark Glaisher <Mark.Glaisher@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef STB7100_FDMA_H
#define STB7100_FDMA_H

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


#define STB7100_FDMA_CHANS              		16
#define STB7109_FDMA_CHANS              		STB7100_FDMA_CHANS
#define STB7200_FDMA_CHANS				32
#define STB7100_REQ_LINES				32
#define STB7200_REQ_LINES				64

/*******************************/
/*MBOX SETUP VALUES*/

#define MBOX_CMD_PAUSE_CHANNEL		 		2
#define MBOX_CMD_START_CHANNEL       			1
#define CLEAR_WORD					0XFFFFFFFF

#define IS_PACED_CHANNEL_SET(flags)(flags & 0x1f)
#define ASSERT_NODE_BUS_ADDR(addr)( (((PXSEG(addr) == P0SEG) && addr))?1:0)
#define IS_CHANNEL_PAUSED(fd,ch)(stb710x_get_engine_status(fd,ch)== FDMA_CHANNEL_PAUSED ?1:0)
#define IS_CHANNEL_RUNNING(fd,ch)(stb710x_get_engine_status(fd,ch)== FDMA_CHANNEL_RUNNING ?1:0)
#define IS_CHANNEL_IDLE(fd,ch)(stb710x_get_engine_status(fd,ch)== FDMA_CHANNEL_IDLE ?1:0)
#define IS_TRANSFER_SG(parms)((MODE_SRC_SCATTER==parms->mode)||(MODE_DST_SCATTER==parms->mode )?1:0)
#define MBOX_STR_CMD(ch) (MBOX_CMD_START_CHANNEL << (ch*2))
#define CMD_STAT_REG(ch)(fd->io_base + fd->regs.fdma_cmd_statn + (ch * CMD_STAT_OFFSET))
#define CH_PTR_REG(ch)(fd->io_base + fd->regs.fdma_ptrn  + (ch * CMD_STAT_OFFSET))

#define IS_NODE_MALLOCED(priv)((priv.node!=0))


#define IS_NODELIST_EQUAL(priv)((priv.sublist_nents == priv.alloced_nents))
#define FDMA_CHAN(channel) ((struct channel_status*)(channel->priv_data))
#define FDMA_DEV(channel) (FDMA_CHAN(channel)->fd)
struct fdma_dev;

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


struct llu_node{
	struct fdma_llu_entry * virt_addr;
	dma_addr_t dma_addr;
};


typedef struct dma_xfer_descriptor {
	struct llu_node* (*extrapolate_fn)(struct stm_dma_params *xfer,
					   struct dma_xfer_descriptor *desc,
					   struct llu_node* nodes);
	int	extrapolate_line_len;
	struct fdma_llu_entry template_llu;

	/* only used when this is the first parameter in a list */
	struct 	llu_node *llu_nodes;
	int 	alloced_nodes;
}dma_xfer_descriptor;


enum fdma_state {
	FDMA_IDLE,
	FDMA_CONFIGURED,
	FDMA_RUNNING,
	FDMA_STOPPING,
	FDMA_PAUSING,
	FDMA_PAUSED,
};

struct channel_status {
	struct fdma_dev *fd;
	enum fdma_state sw_state;
	struct dma_channel * cur_cfg;
	struct stm_dma_params *params;
	struct tasklet_struct fdma_complete;
	struct tasklet_struct fdma_error;
};

#define FDMA_NAME_LEN 20

struct fdma_dev {
	char				name[FDMA_NAME_LEN];
	struct dma_info 		dma_info;
	struct channel_status		channel[STB7100_FDMA_CHANS];
	spinlock_t 			channel_lock;
	struct resource *		phys_mem;
	void __iomem			*io_base;
	u32				firmware_loaded;
	u8				ch_min;
	u8 				ch_max;
	u8				irq_val;
	u8				fdma_num;
	u32				ch_status_mask;
	struct dma_pool 		*llu_pool;
	wait_queue_head_t		fw_load_q;

	struct fdma_regs		regs;

	char *				fw_name;
	struct fdma_fw			fw;
	int				comp_ch;

	/* This is used with the xbar to allocate the next available req line */
	unsigned long 			req_lines_inuse;
};

typedef volatile unsigned long device_t;

#define fdma_printk(level, fd, format, arg...)	\
	dev_printk(level, &fd->dma_info.pdev->dev, format, ## arg);
#define fdma_info(fd, format, arg...)		\
	fdma_printk(KERN_INFO, fd, format, ## arg)

#if defined(CONFIG_STM_DMA_DEBUG)
#define fdma_dbg(fd, format, arg...)		\
	fdma_printk(KERN_DEBUG, fd, format, ## arg)
#else
#define fdma_dbg(fd, format, arg...)		do { } while (0)
#endif

struct stm_dma_req {
	struct channel_status *chan;
	int local_req_line;
};

#endif
