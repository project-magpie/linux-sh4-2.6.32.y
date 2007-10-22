/*
 *  STb7100 FDMA Driver
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

#ifndef STM_DMA_H
#define STM_DMA_H

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/string.h>
#include <linux/module.h>


#if defined(CONFIG_STM_DMA_DEBUG)
	#define fdma_log(format, args...)  printk(format, ## args)
#else
	#define fdma_log(format, args...) ;
#endif

/*when we are running in SH-compatibility mode these mode and dim flags will#
 * be muxed into the dma_channel-flags member*/

/*DMA Modes */
#define MODE_FREERUNNING   		0x01	/* FDMA, GPDMA */
#define MODE_PACED  		 	0x02	/* FDMA */
#define MODE_SH_COMPATIBILITY		0x03
#define MODE_SRC_SCATTER		0x04
#define MODE_DST_SCATTER		0x05

/* DMA dimensions */
#define DIM_SRC_SHIFT 0
#define DIM_DST_SHIFT 2
#define DIM_SRC(x) (((x) >> DIM_SRC_SHIFT) & 3)
#define DIM_DST(x) (((x) >> DIM_DST_SHIFT) & 3)
enum stm_dma_dimensions {
	DIM_0_x_0 = (0 << DIM_SRC_SHIFT) | (0 << DIM_DST_SHIFT),
	DIM_0_x_1 = (0 << DIM_SRC_SHIFT) | (1 << DIM_DST_SHIFT),
	DIM_0_x_2 = (0 << DIM_SRC_SHIFT) | (2 << DIM_DST_SHIFT),
	DIM_1_x_0 = (1 << DIM_SRC_SHIFT) | (0 << DIM_DST_SHIFT),
	DIM_1_x_1 = (1 << DIM_SRC_SHIFT) | (1 << DIM_DST_SHIFT),
	DIM_1_x_2 = (1 << DIM_SRC_SHIFT) | (2 << DIM_DST_SHIFT),
	DIM_2_x_0 = (2 << DIM_SRC_SHIFT) | (0 << DIM_DST_SHIFT),
	DIM_2_x_1 = (2 << DIM_SRC_SHIFT) | (1 << DIM_DST_SHIFT),
	DIM_2_x_2 = (2 << DIM_SRC_SHIFT) | (2 << DIM_DST_SHIFT),
	DIM_REQ_SEL = 0x10,
};

enum stm_dma_flags {
	STM_DMA_INTER_NODE_PAUSE=0x800,
	STM_DMA_LIST_COMP_INT=0x1000,
	STM_DMA_CB_CONTEXT_ISR=0x2000,
	STM_DMA_CB_CONTEXT_TASKLET=0x4000,
	STM_DMA_SETUP_CONTEXT_TASK=0x8000,
	STM_DMA_SETUP_CONTEXT_ISR=0x10000,
	STM_DMA_CHANNEL_PAUSE_FLUSH=0x20000,
	STM_DMA_CHANNEL_PAUSE_NOFLUSH=0x40000,
	STM_DMA_NOBLOCK_MODE=0x80000,
	STM_DMA_BLOCK_MODE=0x100000,
	STM_DMA_LIST_CIRC=0x200000,
	STM_DMA_LIST_OPEN=0x400000,
};

#define DMA_CHANNEL_STATUS_IDLE 		0
#define DMA_CHANNEL_STATUS_RUNNING 		2
#define DMA_CHANNEL_STATUS_PAUSED 		3

/*we only have the notion of two types of channels thus far*/
#define STM_DMA_CAP_HIGH_BW 	"STM_DMA_HIGH_BANDWIDTH"
#define STM_DMA_CAP_LOW_BW 		"STM_DMA_LOW_BANDWIDTH"
#define STM_DMAC_ID 			"ST40 STB710x FDMAC"


/* dma_extend() operations */
#define STM_DMA_OP_PAUSE			1
#define STM_DMA_OP_UNPAUSE			2
#define STM_DMA_OP_STOP				3
#define STM_DMA_OP_COMPILE			4
#define STM_DMA_OP_STATUS			5
#define STM_DMA_OP_MEM_FREE			6
#define STM_DMA_OP_PACING			7


struct stm_dma_params;

typedef struct fmdareq_RequestConfig_s
{
    char Index;         /* Request line index number */
    char Access;        /* Access type: Read or Write */
    char OpCode;        /* Size of word access */
    char Count;         /* Number of transfers per request */
    char Increment;     /* Whether to increment. On 5517, number of bytes to increment per request */
    char HoldOff;       /* Holdoff value between req signal samples (clock cycles)*/
    char Initiator;     /* Use the default value */
}fdmareq_RequestConfig_t;

typedef struct llu_node{
	struct fdma_llu_entry * virt_addr;
	dma_addr_t dma_addr;
}llu_node;

typedef struct dma_xfer_descriptor {
	struct 	llu_node * node;
	int 	alloced_nents;
	int    	(*nodelist_setup) (struct stm_dma_params *xfer);
	void    (*extrapolate_fn)(struct stm_dma_params *xfer);
	int	extrapolate_line_len;
	int 	sublist_nents;
}dma_xfer_descriptor;

typedef struct stm_dma_params {

	struct stm_dma_params *next;
	char  dmac_name[30];
	unsigned long mode;  /*For STMicro DMA API modes see /include/linux/7100_fdma2.h*/
	/* a pointer to a callback function of type void foo(void*)
	 * which will be called on completion of the entire
	 * transaction or after each transfer suceeds if
	 * NODE_PAUSE_ISR is specifed */
	void				(*comp_cb)(void*);
	void				*comp_cb_parm;

	/* a pointer to a callback function of type void foo(void*)
	 * which will be called upon failure of a transfer or
	 * transaction*/
	void				(*err_cb)(void*);
	void				*err_cb_parm;

	/*Source location line stride for use in 0/1/2 x 2D modes*/
	unsigned long			sstride;

	/*Source location line stride for use in 2D x 0/1/2 modes*/
	unsigned long			dstride;

	/* Line length for any 2D modes */
	unsigned long			line_len;

	/*source addr - given in phys*/
	unsigned long 			sar;

	/*dest addr  - given in phys*/
	unsigned long 			dar;

	unsigned long 			node_bytes;

	struct scatterlist * srcsg;
	struct scatterlist * dstsg;

	int err_cb_isr	:1;
	int comp_cb_isr	:1;

	int node_pause		:1;
	int node_interrupt	:1;
	int blocking		:1;
	int circular_llu        :1;

	unsigned long dim;
	/* Parameters for paced transfers */
	unsigned long req_line;
	/*setup called from task or isr context ? */
	unsigned long context;
	/* Pointer to compiled parameters
	 * this includes the *template* llu node and
	 * its assoc'd memory */
	dma_xfer_descriptor priv;
}stm_dma_params;


#define REPORT_STM_DMA_PARMS(dmap)(\
fdma_log("DMA Struct is MODE %s\n CCB %x\n\
CCBParm %x\n CCBISR %s\n ECB %x\n\
ECBParm %x\n ECBISR %s\n SADDR %x\n\
DADDR %x\n SSTRIDE %x\n BYTES %x\n\
CALL_CNTX %s\n DSTRIDE %x\n LEN %x\n\
PAUSE %s\n ISR %s\n DIM %x\n\
LIST_TYPE %s\n REQ %x\n",\
	(dmap.mode== MODE_FREERUNNING ? "FREE":"PACED"),\
	(int)dmap.comp_cb,\
	(int)dmap.comp_cb_parm,\
	dmap.comp_cb_isr ? "INTERRUPT":"TASKLET",\
	(int)dmap->err_cb,\
	(int)dmap->err_cb_parm,\
	dmap->err_cb_isr ? "INTERRUPT":"TASKLET",\
	(int)dmap->sar,\
	(int) dmap->dar, \
	(int)dmap->sstride,\
	(int)dmap->node_bytes,\
	dmap->context==STM_DMA_SETUP_CONTEXT_TASK ?"TASK":"INTERRUPT",\
	(int)dmap->dstride,\
	(int)dmap->line_len,\
	dmap->node_pause? "PAUSE_ISR":"NO_NODE_PAUSE",\
	dmap->node_interrupt ?"NODE_ISE":"NO_NODE_ISR",\
	(int)dmap->dim,\
	dmap->circular_llu ?"CIRCULAR":"UNLINKED",\
	(int)dmap->req_line))


static inline void declare_dma_parms(	struct stm_dma_params * p,
					unsigned long mode,
					unsigned long list_type,
					unsigned long context,
					unsigned long blocking,
					char * name)
{
	if(p){
		memset(p,0,sizeof(struct stm_dma_params));
		p->mode = mode;
		p->circular_llu = (STM_DMA_LIST_CIRC ==list_type ?1:0);
		p->context  = ((STM_DMA_SETUP_CONTEXT_ISR == context) ?
						GFP_ATOMIC:GFP_KERNEL);
		p->blocking = (STM_DMA_NOBLOCK_MODE == blocking ? 0:1);
		p->priv.sublist_nents=1;
		if(strlen(name) >=sizeof(p->dmac_name))
			printk("%s Failed - limit 'name' to (%d) chars",
				__FUNCTION__,sizeof(p->dmac_name));
		else
			memcpy(&p->dmac_name,name,strlen(name));

	}
};

static inline int dma_manual_stbus_pacing(struct stm_dma_params *params,
									struct fmdareq_RequestConfig_s * rq)
{
	struct dma_info * info =  get_dma_info_by_name(params->dmac_name);
	return dma_extend(info->channels[0].chan,STM_DMA_OP_PACING,rq);
}

static inline int dma_get_status(unsigned int chan)
{
	return dma_extend(chan,STM_DMA_OP_STATUS,NULL);
}

static inline int dma_pause_channel(int flags, unsigned int chan)
{
	return dma_extend(chan, STM_DMA_OP_PAUSE, (void*)flags);
}

static inline void dma_unpause_channel(unsigned int chan)
{
	dma_extend(chan, STM_DMA_OP_UNPAUSE, NULL);
}

static inline int dma_stop_channel(unsigned int chan)
{
	return dma_extend(chan, STM_DMA_OP_STOP, NULL);
}

static inline int dma_free_descriptor(struct stm_dma_params *params)
{
	struct dma_info * info =  get_dma_info_by_name(params->dmac_name);
	return dma_extend(info->channels[0].chan,STM_DMA_OP_MEM_FREE,params);
}

static inline int dma_compile_list(struct stm_dma_params *params)
{
	/*we dont care about channel nrs for a compile, but we need
	 *  a valid set of hooks, so get the first valid channel for given
	 * controller.*/
	 struct dma_info * info =  get_dma_info_by_name(params->dmac_name);
	 if(info == NULL){
	 	printk("%s Cant find matching controller to %s\n",
	 			__FUNCTION__,params->dmac_name);
	 	return -EINVAL;
	 }
	 return dma_extend(info->first_channel_nr,STM_DMA_OP_COMPILE,params);
}

static inline int dma_xfer_list(unsigned int chan,stm_dma_params * p)
{
	struct dma_channel * this_ch = get_dma_channel(chan);
	if((this_ch != NULL) && (this_ch->chan == chan) ){
		/*TODO :- this is a bit 'orrible -
		 * should really extend arch/sh/drivers/dma/dma-api.c
		 * to include a 'set_dma_channel'*/
		this_ch->priv_data = (void*)p;
		dma_configure_channel(chan,0);
		return dma_xfer(chan,0,0,0,0);
	}
	return -EINVAL;

}


/* Configure parameters via an API */

static inline  void dma_parms_sg(	struct stm_dma_params *p,
					struct scatterlist * sg,
					int nents)
{
	if(MODE_SRC_SCATTER==p->mode)
		p->srcsg=sg;
	else if (MODE_DST_SCATTER==p->mode)
		p->dstsg = sg;
	else
		BUG();

	p->priv.sublist_nents=nents;
}

static inline void dma_link_nodes(	struct stm_dma_params * parent,
					struct stm_dma_params * child)
{
	if(child)
		parent->next=child;
}

static inline void dma_parms_addrs(	struct stm_dma_params *p,
					unsigned long src,
					unsigned long dst,
					unsigned long bytes)
{
	p->sar = src;
	p->dar = dst;
	p->node_bytes = bytes;
}

static inline void dma_parms_interrupts(struct stm_dma_params *p,
					unsigned long isrflag)
{
	if(isrflag & STM_DMA_INTER_NODE_PAUSE)
		p->node_pause=1;
	if(isrflag & STM_DMA_INTER_NODE_PAUSE )
		p->node_interrupt=1;

}

static inline void dma_parms_comp_cb(	struct stm_dma_params *p,
					void (*fn)(void* param),
					void* param,
					int isr_context)
{
	p->comp_cb = fn;
	p->comp_cb_parm = param;
	p->comp_cb_isr = (isr_context == STM_DMA_CB_CONTEXT_ISR ?1:0);
}

static inline void dma_parms_err_cb(	struct stm_dma_params *p,
					void (*fn)(void* param),
	      				void* param,
	      				int isr_context)
{
	p->err_cb = fn;
	p->err_cb_parm = param;
	p->err_cb_isr = (isr_context == STM_DMA_CB_CONTEXT_ISR ?1:0);
}

static inline void dma_parms_manual_dim_parms(	struct stm_dma_params *p,
						unsigned long length,
						unsigned long sstride,
						unsigned long dstride,
						unsigned long dim)
{
	p->sstride = sstride;
	p->dstride = dstride;
	p->line_len = length;
	p->dim =dim;
}

static inline void dma_parms_DIM_0_x_0(	struct stm_dma_params *p,
					unsigned long srcsize)
{
	p->sstride = 0;
	p->dstride = 0;
	p->line_len = srcsize;
	p->dim  =DIM_0_x_0;
}

static inline void dma_parms_paced(	struct stm_dma_params *p,
					unsigned long xfer_size,
					int req_line)
{
	p->sstride = 0;
	p->dstride = 0;
	p->line_len = xfer_size;
	p->req_line  =req_line;
	p->dim  =DIM_REQ_SEL;
}


static inline void dma_parms_DIM_0_x_1(	struct stm_dma_params *p,
					unsigned long srcsize)
{
	p->sstride = 0;
	p->dstride = srcsize;
	p->line_len = srcsize;
	p->dim  =DIM_0_x_1;
}
static inline void dma_parms_DIM_0_x_2(	struct stm_dma_params *p,
					unsigned long srcsize,
					unsigned long dstride)
{
	p->sstride =0;
	p->dstride = dstride;
	p->line_len =srcsize;
	p->dim  =DIM_0_x_2;
}
static inline void dma_parms_DIM_1_x_0(	struct stm_dma_params *p,
					unsigned long srcsize)
{
	p->sstride = srcsize;
	p->dstride =0;
	p->line_len = srcsize;
	p->dim  =DIM_1_x_0;
}
static inline void dma_parms_DIM_1_x_1(	struct stm_dma_params *p,
					unsigned long srcsize)
{
	p->sstride =0;
	p->dstride =0;
	p->line_len = srcsize;
	p->dim  =DIM_1_x_1;
}

static inline void dma_parms_DIM_1_x_2(	struct stm_dma_params *p,
					unsigned long dstsize,
					unsigned long dstride)
{
	p->sstride = dstsize;
	p->dstride =dstride;
	p->line_len = dstsize;
	p->dim  =DIM_1_x_2;
}
static inline void dma_parms_DIM_2_x_0(	struct stm_dma_params *p,
					unsigned long srcsize,
					unsigned long sstride)
{
	p->sstride =sstride;
	p->dstride =0;
	p->line_len = srcsize;
	p->dim  =DIM_2_x_0;
}

static inline void dma_parms_DIM_2_x_1(	struct stm_dma_params *p,
					unsigned long srcsize,
					unsigned long sstride)
{
        p->sstride = sstride;
      	p->dstride= srcsize;
	p->line_len =srcsize;
	p->dim  =DIM_2_x_1;
}
#endif
