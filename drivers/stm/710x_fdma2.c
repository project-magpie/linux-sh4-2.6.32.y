/*
 * Copyright (C) 2005,7 STMicroelectronics Limited
 * Authors: Mark Glaisher <Mark.Glaisher@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/dmapool.h>
#include <linux/stm/fdma-plat.h>
#include <linux/stm/stm-dma.h>

#include "fdma.h"



static int setup_freerunning_node(struct stm_dma_params *params,
				  struct fdma_llu_entry* llu)
{
	memset(llu, 0, sizeof(*llu));

	if (params->node_pause)
		llu->control |=  SET_NODE_COMP_PAUSE | SET_NODE_COMP_IRQ;

	if (params->node_interrupt)
		llu->control |= SET_NODE_COMP_IRQ;

	if (DIM_SRC(params->dim) == 0) {
		llu->control |= NODE_ADDR_STATIC <<SOURCE_ADDR;
	} else {
		llu->control |= NODE_ADDR_INCR <<SOURCE_ADDR;
	}

	if (DIM_DST(params->dim) == 0) {
		llu->control |= NODE_ADDR_STATIC <<DEST_ADDR;
	} else {
		llu->control |= NODE_ADDR_INCR <<DEST_ADDR;
	}

	llu->line_len		= params->line_len;
	llu->sstride 		= params->sstride;
	llu->dstride 		= params->dstride;
	return 0;
}

static int setup_paced_node(struct stm_dma_params *params,
			    fdma_llu_entry* llu)

{
	memset(llu, 0, sizeof(*llu));

	/* Moved this into the extrapolate functions so that we can
	 * change channel in the same way as address. Yech */
	/* llu->control= params->req_line; */
	llu->size_bytes= params->node_bytes;
	llu->line_len = params->node_bytes;

	if (params->node_pause)
		/* In order to recieve the pause interrupt
		 * we must also enable end of node interrupts. */
		llu->control |=  SET_NODE_COMP_PAUSE | SET_NODE_COMP_IRQ;

	if (params->node_interrupt)
		llu->control |= SET_NODE_COMP_IRQ;

	if (DIM_SRC(params->dim) == 0) {
		llu->control |= NODE_ADDR_STATIC <<SOURCE_ADDR;
	} else {
		llu->control |= NODE_ADDR_INCR <<SOURCE_ADDR;
	}

	if (DIM_DST(params->dim) == 0) {
		llu->control |= NODE_ADDR_STATIC <<DEST_ADDR;
	} else {
		llu->control |= NODE_ADDR_INCR <<DEST_ADDR;
	}

	return 0;
}

static struct llu_node* extrapolate_simple(
	struct stm_dma_params *params,
	struct dma_xfer_descriptor *desc,
	struct llu_node* llu_node)
{
	struct fdma_llu_entry* dest_llu = llu_node->virt_addr;

	dest_llu->control	= desc->template_llu.control |
		(params->req ? params->req->local_req_line : 0);
	dest_llu->size_bytes	= params->node_bytes;
	dest_llu->saddr		= params->sar;
	dest_llu->daddr		= params->dar;
	if (desc->extrapolate_line_len)
		dest_llu->line_len = params->node_bytes;
	else
		dest_llu->line_len = desc->template_llu.line_len;
	dest_llu->sstride	= desc->template_llu.sstride;
	dest_llu->dstride	= desc->template_llu.dstride;

	return llu_node;
}

static struct llu_node* extrapolate_sg_src(
	struct stm_dma_params *params,
	struct dma_xfer_descriptor *desc,
	struct llu_node* llu_node)
{
	int i;
	struct scatterlist * sg = params->srcsg;
	struct llu_node* last_llu_node = llu_node;

	for (i=0; i<params->sglen; i++) {
		struct fdma_llu_entry* dest_llu = llu_node->virt_addr;

		dest_llu->control	= desc->template_llu.control;
		dest_llu->size_bytes	= sg_dma_len(sg);
		dest_llu->saddr		= sg_dma_address(sg);
		dest_llu->daddr		= params->dar;
		if (desc->extrapolate_line_len)
			dest_llu->line_len = sg_dma_len(sg);
		else
			dest_llu->line_len = desc->template_llu.line_len;
		dest_llu->sstride	= desc->template_llu.sstride;
		dest_llu->dstride	= 0;

		last_llu_node = llu_node++;
		dest_llu->next_item	= llu_node->dma_addr;
		sg++;
	}

	return last_llu_node;
}

static struct llu_node* extrapolate_sg_dst(
	struct stm_dma_params *params,
	struct dma_xfer_descriptor *desc,
	struct llu_node* llu_node)
{
	int i;
	struct scatterlist * sg = params->dstsg;
	struct llu_node* last_llu_node = llu_node;

	for (i=0; i<params->sglen; i++) {
		struct fdma_llu_entry* dest_llu = llu_node->virt_addr;

		dest_llu->control	= desc->template_llu.control;
		dest_llu->size_bytes	= sg_dma_len(sg);
		dest_llu->saddr		= params->sar;
		dest_llu->daddr		= sg_dma_address(sg);
		if (desc->extrapolate_line_len)
			dest_llu->line_len = sg_dma_len(sg);
		else
			dest_llu->line_len = desc->template_llu.line_len;
		dest_llu->sstride	= 0;
		dest_llu->dstride	= desc->template_llu.dstride;

		last_llu_node = llu_node++;
		dest_llu->next_item	= llu_node->dma_addr;
		sg++;
	}

	return last_llu_node;
}

static int resize_nodelist_mem(struct fdma_dev * fd,
			       struct dma_xfer_descriptor *desc,
			       unsigned int new_nnodes, gfp_t context)
{
	int old_list_size, new_list_size;
	unsigned int cur_nnodes;
	struct llu_node* new_nodes;

	/* This holds the number of allocated nodes, which may differ
	 * from the old or new size. It must be maintained so that
	 * free_list works. */
	cur_nnodes = desc->alloced_nodes;

	/* The only resize down we need to support is freeing everything. */
	if (new_nnodes == 0) {
		goto free_list;
	}

	old_list_size = sizeof(struct llu_node)*desc->alloced_nodes;
	new_list_size = sizeof(struct llu_node)*new_nnodes;
	new_nodes = kmalloc(new_list_size, context);
	if (new_nodes == NULL)
		goto free_list;

	if (old_list_size > 0) {
		memcpy(new_nodes, desc->llu_nodes, old_list_size);
		kfree(desc->llu_nodes);
	}

	desc->llu_nodes = new_nodes;

	for (new_nodes += desc->alloced_nodes;
	     cur_nnodes < new_nnodes;
	     cur_nnodes++, new_nodes++) {
		new_nodes->virt_addr = dma_pool_alloc(
					fd->llu_pool,
					context,
					&new_nodes->dma_addr);
		if (new_nodes->virt_addr == NULL)
			goto free_list;
	}

	desc->alloced_nodes = new_nnodes;
	return 0;

free_list:
	new_nodes = desc->llu_nodes;
	for( ; cur_nnodes; cur_nnodes--, new_nodes++) {
		dma_pool_free(fd->llu_pool,
			      new_nodes->virt_addr,
			      new_nodes->dma_addr);
	}
	if (desc->llu_nodes)
		kfree(desc->llu_nodes);

	desc->llu_nodes = NULL;
	desc->alloced_nodes = 0;

	return -ENOMEM;
}

static void fdma_start_channel(struct fdma_dev * fd,
			      int ch_num,
			      unsigned long start_addr)
{
	u32 cmd_sta_value = (start_addr | CMDSTAT_FDMA_START_CHANNEL);

	writel(cmd_sta_value,CMD_STAT_REG(ch_num));
	writel(MBOX_STR_CMD(ch_num),fd->io_base +fd->regs.fdma_cmd_set);
}

static int stb710x_get_engine_status(struct fdma_dev * fd,int channel)
{
	return readl(CMD_STAT_REG(channel))&3;
}

static inline void __handle_fdma_err_irq(struct fdma_dev *fd, int chan_num)
{
	struct channel_status *chan = &fd->channel[chan_num];
	void (*err_cb)(unsigned long) = chan->params->err_cb;
	unsigned long err_cb_parm = chan->params->err_cb_parm;

	spin_lock(&fd->channel_lock);

	/*err is bits 2-4*/
	fdma_dbg(fd, "%s: FDMA error %d on channel %d\n", __FUNCTION__,
			(readl(CMD_STAT_REG(chan_num)) >> 2) & 0x7, chan_num);

	/* According to the spec, in case of error transfer "may be
	 * aborted" (or may not be, sigh) so let's make the situation
	 * clear and stop it explicitly now. */
	writel(MBOX_CMD_PAUSE_CHANNEL << (chan_num * 2),
			fd->io_base + fd->regs.fdma_cmd_set);
	chan->sw_state = FDMA_STOPPING;

	spin_unlock(&fd->channel_lock);

	wake_up(&chan->cur_cfg->wait_queue);

	if (err_cb) {
		if (chan->params->err_cb_isr)
			err_cb(err_cb_parm);
		else
			tasklet_schedule(&chan->fdma_error);
	}
}

static inline void __handle_fdma_completion_irq(struct fdma_dev *fd,
		int chan_num)
{
	struct channel_status *chan = &fd->channel[chan_num];
	void (*comp_cb)(unsigned long) = chan->params->comp_cb;
	unsigned long comp_cb_parm = chan->params->comp_cb_parm;

	spin_lock(&fd->channel_lock);

	switch (stb710x_get_engine_status(fd, chan_num)) {
	case FDMA_CHANNEL_PAUSED:
		switch (chan->sw_state) {
		case FDMA_RUNNING:	/* Hit a pause node */
		case FDMA_PAUSING:
			chan->sw_state = FDMA_PAUSED;
			break;
		case FDMA_STOPPING:
			writel(0, CMD_STAT_REG(chan_num));
			chan->sw_state = FDMA_IDLE;
			break;
		default:
			BUG();
		}
		break;
	case FDMA_CHANNEL_IDLE:
		switch (chan->sw_state) {
		case FDMA_RUNNING:
		case FDMA_PAUSING:
		case FDMA_STOPPING:
			chan->sw_state = FDMA_IDLE;
			break;
		default:
			BUG();
		}
		break;
	case FDMA_CHANNEL_RUNNING:
		break;
	default:
		fdma_dbg(fd, "ERR::FDMA2 unknown interrupt status \n");
	}

	spin_unlock(&fd->channel_lock);

	wake_up(&chan->cur_cfg->wait_queue);

	if (comp_cb) {
		if (chan->params->comp_cb_isr)
			comp_cb(comp_cb_parm);
		else
			tasklet_schedule(&chan->fdma_complete);
	}
}

static irqreturn_t fdma_irq(int irq, void *dev_id)
{
	struct fdma_dev * fd = (struct fdma_dev *)dev_id;
	int chan_num;
	u32 int_stat_val = readl(fd->io_base + fd->regs.fdma_int_sta);
	u32 cur_val = int_stat_val & fd->ch_status_mask;

	writel(cur_val, fd->io_base +fd->regs.fdma_int_clr);
	for (cur_val >>= fd->ch_min * 2, chan_num=fd->ch_min;
	     cur_val != 0;
	     cur_val >>= 2, chan_num++) {
		/*error interrupts will raise boths bits, so check
		 * the err bit first*/
		if(unlikely(cur_val & 2))
			__handle_fdma_err_irq(fd,chan_num);
		else if (cur_val & 1)
			__handle_fdma_completion_irq(fd, chan_num);
	}

	/*here we check to see if there is still pending ints for the other dmac, if so
	 * rely on it to signal IRQ_HANDLED once all vectors are cleared, we return IRQ_NONE.
	 * otherwise we have handled everything so we can now safely returnd IRQ_HANDLED
	 * to lower the IRQ.*/
	return IRQ_RETVAL( !(cur_val & (~fd->ch_status_mask)) );
}

/* Paced channel handling */

#ifdef CONFIG_CPU_SUBTYPE_STB7100

static struct stm_dma_req fdma_reqs[STB7100_REQ_LINES];

/* This is the dummy xbar for 710x devices */
static int xbar_local_req(int req_line,
			  struct channel_status *chan)
{
	return req_line;
}

static void xbar_local_free(struct channel_status *chan, int local_req_line)
{
}

static int __init xbar_init(void)
{
	return 0;
}
module_init(xbar_init)

#else

/* Real xbar device */

static struct stm_dma_req fdma_reqs[STB7200_REQ_LINES];

struct xbar_dev {
	struct resource *phys_mem;
	void* io_base;
};

/* Gross hack, we use a global static so the FDMA code can find the
 * xbar. */
static struct xbar_dev* xbar_dev;

/* Needs to be called with both the channel and xbar locks taken. */
static int xbar_local_req(int req_line,
			  struct channel_status *chan)
{
	struct fdma_dev *fd = chan->fd;
	int local_req_line;
	void* xbar_addr;

	if (fd->req_lines_inuse == ~0UL)
		return -1;

	local_req_line = ffz(fd->req_lines_inuse);
	fd->req_lines_inuse |= 1<<local_req_line;

	xbar_addr = xbar_dev->io_base +
		(fd->fdma_num * 0x80) +
		(local_req_line * 4);
	writel(req_line, xbar_addr);

	return local_req_line;
}

static void xbar_local_free(struct channel_status *chan, int local_req_line)
{
	struct fdma_dev *fd = chan->fd;
	fd->req_lines_inuse &= ~(1<<local_req_line);
}

static int __init xbar_driver_probe(struct platform_device *pdev)
{
	struct xbar_dev *xd;
	struct resource *mem_res;
	unsigned long phys_base, phys_size;

	xd = kzalloc(sizeof(struct xbar_dev), GFP_KERNEL);
	if (xd == NULL) {
		return -ENOMEM;
	}

	mem_res = platform_get_resource(pdev,IORESOURCE_MEM,0);
        phys_base = mem_res->start;
        phys_size = mem_res->end - mem_res->start + 1;

        xd->phys_mem = request_mem_region(phys_base, phys_size, "xbar");
	if (xd->phys_mem == NULL) {
		kfree(xd);
                return -EBUSY;
	}

	xd->io_base = ioremap_nocache(phys_base, phys_size);
	if (xd->io_base == NULL) {
		release_resource(xd->phys_mem);
		kfree(xd);
	}

	platform_set_drvdata(pdev, xd);
	xbar_dev = xd;

       	return 0;
}

static int xbar_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver xbar_driver = {
	.driver = {
		.name = "fdma-xbar",
	},
	.probe = xbar_driver_probe,
	.remove = xbar_driver_remove,
};

static int __init xbar_init(void)
{
	return platform_driver_register(&xbar_driver);
}

static void __exit xbar_exit(void)
{
	platform_driver_unregister(&xbar_driver);
}

module_init(xbar_init)
module_exit(xbar_exit)

#endif

static DEFINE_SPINLOCK(fdma_req_lock);

static struct stm_dma_req *fdma_req_allocate(unsigned int req_line, struct channel_status *chan)
{
	struct stm_dma_req* req = NULL;
	int local_req_line;

	if ((req_line < 0) || (req_line >= ARRAY_SIZE(fdma_reqs)))
		return NULL;

	spin_lock(&fdma_req_lock);
	if (fdma_reqs[req_line].chan != NULL) {
		goto out;
	}

	req = &fdma_reqs[req_line];

	local_req_line = xbar_local_req(req_line, chan);
	if (local_req_line == -1) {
		goto out;
	}

	req->chan = chan;
	req->local_req_line = local_req_line;
out:
	spin_unlock(&fdma_req_lock);
	return req;
}

static void fdma_req_free(struct stm_dma_req *req)
{
	spin_lock(&fdma_req_lock);

	if (req->chan)
		xbar_local_free(req->chan, req->local_req_line);

	req->chan = NULL;

	spin_unlock(&fdma_req_lock);
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * FIRMWARE DOWNLOAD & ENGINE INIT
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void fdma_initialise(struct fdma_dev * fd)
{
/*These pokes come from the current STAPI tree.
 * The three magic vals are pokes to undocumented regs so
 * we don't know what they mean.
 *
 * The effect is to turn on and initialise the clocks
 * and set all channels off*/

	/*clear the status regs MBOX & IRQ*/
	writel(CLEAR_WORD, fd->io_base+fd->regs.fdma_int_clr);
	writel(CLEAR_WORD, fd->io_base+fd->regs.fdma_cmd_clr);

	/* Enable the FDMA block */
	writel(1,fd->io_base+fd->regs.fdma_sync_reg);
	writel(5,fd->io_base+fd->regs.fdma_clk_gate);
	writel(0,fd->io_base+fd->regs.fdma_clk_gate);

}
/*this function enables messaging and intr generation for all channels &
 * starts the fdma running*/
static int fdma_enable_all_channels(struct fdma_dev * fd)
{
	writel(CLEAR_WORD,fd->io_base + fd->regs.fdma_int_mask);
	writel(CLEAR_WORD,fd->io_base + fd->regs.fdma_cmd_mask);
	writel(1,fd->io_base +fd->regs.fdma_en);
	return (readl(fd->io_base + fd->regs.fdma_en) &1);
}
static int fdma_disable_all_channels(struct fdma_dev * fd)
{
	writel(0x00,fd->io_base + fd->regs.fdma_int_mask);
	writel(0x00,fd->io_base + fd->regs.fdma_cmd_mask);
	writel(0,fd->io_base + fd->regs.fdma_en);
	return (readl(fd->io_base + fd->regs.fdma_en) &~1);
}

static void fdma_reset_channels(struct fdma_dev * fd)
{
	int channel=0;
	for(;channel <(fd->ch_max-1);channel++)
		writel(0,CMD_STAT_REG(channel));
}

static struct stm_dma_req *stb710x_configure_pace_channel(struct fdma_dev *fd,
	struct dma_channel *channel,
	struct stm_dma_req_config *req_config)
{
	struct channel_status *chan = FDMA_CHAN(channel);
	void __iomem *req_base_reg = fd->io_base+fd->regs.fdma_req_ctln;
	struct stm_dma_req *fdma_req;
	u32 req_ctl;

	fdma_req = fdma_req_allocate(req_config->req_line, chan);
	if (fdma_req == NULL) {
		return NULL;
	}

	req_ctl = 0;
	req_ctl |= (req_config->hold_off	& 0x0f) <<  0;/*Bits 3.0*/
	req_ctl |= (req_config->opcode		& 0x0f) <<  4;/*7..4*/
	req_ctl |= (req_config->rw		& 0x01) << 14;/*14*/
	req_ctl |= (req_config->initiator	& 0x03) << 22;/*23..22*/
	req_ctl |= ((req_config->count-1)	& 0x1F) << 24;/*28..24*/
	req_ctl |= (req_config->increment	& 0x01) << 29;/*29*/

	writel(req_ctl, req_base_reg + (fdma_req->local_req_line * CMD_STAT_OFFSET));

	return fdma_req;
}

static int fdma_register_caps(struct fdma_dev * fd)
{
	int channel = fd->ch_min;
	int res=0;
	int num_caps = fd->ch_max - fd->ch_min + 1;
	struct dma_chan_caps  dmac_caps[num_caps];
	static const char* hb_caps[] = {STM_DMA_CAP_HIGH_BW,NULL};
	static const char* lb_caps[] = {STM_DMA_CAP_LOW_BW,NULL};
	static const char* eth_caps[] = {STM_DMA_CAP_ETH_BUF,NULL};

	for (;channel <= fd->ch_max;channel++) {
		dmac_caps[channel-fd->ch_min].ch_num = channel;
		switch (channel) {
		case 0 ... 3:
			dmac_caps[channel-fd->ch_min].caplist = hb_caps;
			break;
		case 11:
			dmac_caps[channel-fd->ch_min].caplist = eth_caps;
			break;
		default:
			dmac_caps[channel-fd->ch_min].caplist = lb_caps;
			break;
		}
	}
	res= register_chan_caps(fd->name, &dmac_caps[0]);

	if(res!=0){
		fdma_dbg(fd, "%s %s failed to register capabilities err-%d\n",
			__FUNCTION__, fd->name, res);
		return -ENODEV;
	}
	else return 0;
}

static int fdma_run_initialise_sequence(struct fdma_dev *fd)
{
	fd->llu_pool = dma_pool_create(fd->name, NULL,
					sizeof(struct fdma_llu_entry),32,0);
	if (fd->llu_pool == NULL) {
		fdma_dbg(fd, "%s Can't allocate dma_pool memory",__FUNCTION__);
		return -ENOMEM;
	}
	fdma_initialise(fd);
	fdma_reset_channels(fd);

	if(!fdma_enable_all_channels(fd))
		return -ENODEV;
	else return  0;
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * FIRMWARE DOWNLOAD & ENGINE INIT
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void fdma_get_fw_revision(struct fdma_dev * fd, int *major, int *minor)
{
	int reg = readl(fd->io_base + fd->regs.fdma_dmem_region);
	*major  = (reg & 0xff0000) >> 16;
	*minor  = (reg & 0xff00) >> 8;
}

static void fdma_get_hw_revision(struct fdma_dev * fd, int *major, int *minor)
{
	*major = readl(fd->io_base + fd->regs.fdma_id);
	*minor = readl(fd->io_base + fd->regs.fdma_ver);
}

#if  defined(CONFIG_STM_DMA_FW_KERNEL)

static int fdma_do_bootload(struct fdma_dev * fd)
{
	device_t* ptr=0;
	struct fdma_fw fw=fd->fw;
	unsigned long unused_ibytes;
	unsigned long unused_dbytes;
	unsigned long irqflags;
	void * addr =(char*)fd->io_base;

	fdma_dbg(fd, "FDMA: Loading Firmware...");
	unused_ibytes= fw.imem_len - fw.imem_fw_sz;
	unused_dbytes= fw.dmem_len - fw.dmem_fw_sz;

	spin_lock_irqsave(&fd->channel_lock,irqflags);
	ptr = (device_t*) ((char*) addr +fd->regs.fdma_dmem_region);
	memcpy((void*)ptr,&fw.data_reg[0],fw.dmem_fw_sz * sizeof(u32));
	if(unused_dbytes){
		ptr =(device_t*) ((char*)addr +fd->regs.fdma_dmem_region
				  +(fw.dmem_fw_sz*sizeof(u32)));
		memset((void*)ptr ,0, unused_dbytes);
	}

	ptr = (device_t*) ((char*) addr +fd->regs.fdma_imem_region);
	memcpy((void*)ptr,&fw.imem_reg[0],fw.imem_fw_sz* sizeof(u32));
	if(unused_ibytes){
		ptr =(device_t*) ((char*)addr +fd->regs.fdma_imem_region
				  +(fw.imem_fw_sz*sizeof(u32)));
		memset((void*)ptr,0, unused_ibytes);
	}
	spin_unlock_irqrestore(&fd->channel_lock,irqflags);

	fd->firmware_loaded=1;
	wake_up(&fd->fw_load_q);

	return 0;
}
#elif defined(CONFIG_STM_DMA_FW_USERSPACE)

/* Awkwardly the current FDMA elf instruction section is stored with
 * 3 byte words. The Slim requires the following - fmt(0x00nnnnnn).
 * where - 	0 - appended 0 byte
 * 	 	n value read from elf
 *
 * So we must manually insert these word by word from the elf,
 * This also means the size parameter is incorrect
 * - Grrrr.
*/
static void build_elf_imem(	struct fdma_dev * fd,
				struct elf32_shdr * sect_hd,
				struct firmware * slimcore_elf)
{
	int pos=0;
	char * file_off=0;
	int imem_sz = sect_hd->sh_size+ (sect_hd->sh_size /3);
	u8 * imem_st=  kmalloc(imem_sz,GFP_KERNEL);
	u8 * imem_ptr =imem_st;
	char * imem_sect = (char*)fd->io_base + fd->regs.fdma_imem_region;

	file_off =(u8*) &slimcore_elf->data[sect_hd->sh_offset];

	do{
		memcpy(imem_ptr,file_off,sizeof(char)*3);
		imem_ptr+=3;
		file_off+=3;
		*imem_ptr=0x00;
		imem_ptr++;
	}while((pos+=3)< sect_hd->sh_size);

	memcpy(imem_sect,imem_st ,imem_sz);
	kfree(imem_ptr);
}

static void build_elf_dmem(	struct fdma_dev * fd,
				struct elf32_shdr * sect_hd,
				struct firmware * slimcore_elf)
{
	char * dmem_sect = (char*)fd->io_base + fd->regs.fdma_dmem_region;
	char * file_off=0;

	file_off = (char*)&slimcore_elf->data[sect_hd->sh_offset];
	memcpy(dmem_sect,(char*)file_off ,sect_hd->sh_size);
}


static int fdma_do_bootload(struct fdma_dev * fd)
{
	int err=0;
	int i=0,imem_loaded=0,dmem_loaded=0;
	char fw_revision[20];
	char hw_revision[20];
	int major=0,minor=0;
	struct firmware *slimcore_elf={0};
	struct elf32_hdr hdr;

	fdma_dbg(fd, "FDMA: Loading Firmware ELF...");

	err = request_firmware((const struct firmware **)&slimcore_elf,
				fd->fw_name,&fdma_device_list[fd->hwid]);
	if(err != 0 ){
		fdma_dbg(fd, "%s Can't Locate/Load Firmware %s\n",
		       __FUNCTION__, fd->fw_name);
		return -ENOENT;
	}

	memcpy(&hdr,slimcore_elf->data,sizeof(struct elf32_hdr));


	/* build the section header tbl */
	for(i=0;i < hdr.e_shnum;i++){
		struct elf32_shdr sect_hdr;
		char* sh_addr = (char*)&slimcore_elf->data[hdr.e_shoff + (i * sizeof(struct elf32_shdr))];
		memcpy(&sect_hdr,(char*)sh_addr ,sizeof(struct elf32_shdr));

		if(SHT_PROGBITS== sect_hdr.sh_type){
			if(sect_hdr.sh_flags & SHF_ALLOC){

				if((sect_hdr.sh_flags & SHF_EXECINSTR) == SHF_EXECINSTR){
					build_elf_imem(fd,&sect_hdr,slimcore_elf);
					imem_loaded=1;
				}
				else if((sect_hdr.sh_flags & SHF_WRITE) == SHF_WRITE){
					build_elf_dmem(fd,&sect_hdr,slimcore_elf);
					dmem_loaded=1;
				}
			}
			if(dmem_loaded && imem_loaded){
				/*we can discard the remainder of the elf now*/
				break;
			}
		}
	}
	release_firmware(slimcore_elf);
	if(dmem_loaded && imem_loaded){
		fd->firmware_loaded=1;
		wake_up(&fd->fw_load_q);
	}
	else return -ENODEV;

	fdma_get_fw_revision(fd,&fw_revision[0],major,minor);
	fdma_get_hw_revision(fd,&hw_revision[0],major,minor);
	fdma_dbg(fd, "STB_%dC%d %s %s OK\n",
		 fd->cpu_subtype,fd->cpu_rev,hw_revision,fw_revision);
	return 0;
}
#endif

static int fdma_load_firmware(struct fdma_dev * fd)
{
	unsigned long irqflags=0;
	int hw_major, hw_minor;
	int fw_major, fw_minor;
	spin_lock_irqsave(&fd->channel_lock,irqflags);
	switch ( fd->firmware_loaded ) {
		case 0:
			fd->firmware_loaded = -1;
			spin_unlock_irqrestore(&fd->channel_lock,irqflags);
			if (fdma_do_bootload(fd)!=0){
				fd->firmware_loaded=0;
				return  -ENOMEM;
			}
			fdma_get_hw_revision(fd, &hw_major, &hw_minor);
			fdma_get_fw_revision(fd, &fw_major, &fw_minor);
			fdma_info(fd, "SLIM hw %d.%d, FDMA fw %d.%d\n",
				  hw_major, hw_minor, fw_major, fw_minor);

			if(fdma_run_initialise_sequence(fd)!=0)
				return -ENODEV;

			return (fd->firmware_loaded==1) ? 0:-ENODEV;
		case 1:
			spin_unlock_irqrestore(&fd->channel_lock,irqflags);
			return 0;
		default:
		case -1:
			spin_unlock_irqrestore(&fd->channel_lock,irqflags);
			wait_event_interruptible(fd->fw_load_q,(fd->firmware_loaded==1));
			if(!fd->firmware_loaded)
				return -ENODEV;
			else return 0;
	}
	return 0;
}

static int fdma_check_firmware_state(struct fdma_dev * fd)
{
	return (fd->firmware_loaded) ? 0:fdma_load_firmware(fd);
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * Linux -SH DMA API hooks
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

/*returns the number of bytes left to transfer for the current node*/
static int stb710x_fdma_get_residue(struct dma_channel *chan)
{
	struct fdma_dev *fd = FDMA_DEV(chan);
	unsigned long irqflags;
	int count = 0;

	spin_lock_irqsave(&fd->channel_lock, irqflags);

	if (likely(FDMA_CHAN(chan)->sw_state != FDMA_IDLE)) {
		void __iomem *chan_base = fd->io_base +
				(chan->chan * NODE_DATA_OFFSET);
		fdma_llu_entry *current_node, *next_node;

		/* Get info about current node */
		current_node = phys_to_virt(readl(CMD_STAT_REG(chan->chan)) &
				~0x1f);
		count = readl(chan_base + fd->regs.fdma_cntn);

		/* Accumulate the bytes remaining in the list */
		next_node = phys_to_virt(readl(chan_base + fd->regs.fdma_ptrn));
		while (next_node && next_node > current_node) {
			count += next_node->size_bytes;
			next_node = phys_to_virt(next_node->next_item);
		}
	}

	spin_unlock_irqrestore(&fd->channel_lock, irqflags);

	return count;
}

/*must only be called when channel is in paused state*/
static int stb710x_fdma_unpause(struct fdma_dev * fd,struct dma_channel * channel)
{
	struct channel_status *chan = FDMA_CHAN(channel);
	unsigned long irqflags=0;
	u32 cmd_sta_value;

	spin_lock_irqsave(&fd->channel_lock,irqflags);
	if (chan->sw_state != FDMA_PAUSED) {
		spin_unlock_irqrestore(&fd->channel_lock,irqflags);
		return -EBUSY;
	}

	cmd_sta_value = readl(CMD_STAT_REG(channel->chan));
	cmd_sta_value &= ~CMDSTAT_FDMA_CMD_MASK;
	cmd_sta_value |= CMDSTAT_FDMA_RESTART_CHANNEL;
	writel(cmd_sta_value, CMD_STAT_REG(channel->chan));

	writel(MBOX_CMD_START_CHANNEL << (channel->chan*2),
	       fd->io_base + fd->regs.fdma_cmd_set);
	chan->sw_state = FDMA_RUNNING;

	spin_unlock_irqrestore(&fd->channel_lock,irqflags);
	return 0;
}

static int stb710x_fdma_pause(struct fdma_dev * fd,
		struct dma_channel * channel,
		int flush)
{
	struct channel_status *chan = FDMA_CHAN(channel);
	unsigned long irqflags=0;

	spin_lock_irqsave(&fd->channel_lock,irqflags);
	switch (chan->sw_state) {
	case FDMA_IDLE:
	case FDMA_CONFIGURED:
		/* Hardware isn't set up yet, so treat this as an error */
		spin_unlock_irqrestore(&fd->channel_lock,irqflags);
		return -EBUSY;
	case FDMA_PAUSED:
		/* Hardware is already paused */
		spin_unlock_irqrestore(&fd->channel_lock,irqflags);
		return 0;
	case FDMA_RUNNING:
		/* Hardware is running, send the command */
		writel((flush ? MBOX_CMD_FLUSH_CHANNEL : MBOX_CMD_PAUSE_CHANNEL)
				<< (channel->chan * 2),
				fd->io_base + fd->regs.fdma_cmd_set);
		/* Fall through */
	case FDMA_PAUSING:
	case FDMA_STOPPING:
		/* Hardware is pausing already, wait for interrupt */
		chan->sw_state = FDMA_PAUSING;
		spin_unlock_irqrestore(&fd->channel_lock,irqflags);
#if 0
		/* In some cases this is called from a context which cannot
		 * block, so disable the wait at the moment. */
		wait_event(chan->cur_cfg->wait_queue,
			   chan->sw_state == FDMA_PAUSED);
#endif
		break;
	}

	return 0;
}

static int stb710x_fdma_request(struct dma_channel *channel)
{
	struct fdma_dev *fd = FDMA_DEV(channel);

	if(fdma_check_firmware_state(fd)==0){
		return 0;
	}

	return ENOSYS;
}

static int stb710x_fdma_stop(struct fdma_dev * fd,struct dma_channel *channel)
{
	struct channel_status *chan = FDMA_CHAN(channel);
	unsigned long cmd_val = (MBOX_CMD_PAUSE_CHANNEL << (channel->chan*2));
	unsigned long irqflags=0;

	spin_lock_irqsave(&fd->channel_lock,irqflags);
	switch (chan->sw_state) {
	case FDMA_IDLE:
	case FDMA_CONFIGURED:
	case FDMA_PAUSED:
		/* Hardware is already idle, simply change state */
		chan->sw_state = FDMA_IDLE;
		writel(0,CMD_STAT_REG(channel->chan));
		spin_unlock_irqrestore(&fd->channel_lock,irqflags);
		break;
	case FDMA_RUNNING:
		/* Hardware is running, send the command */
		writel(cmd_val,(fd->io_base +fd->regs.fdma_cmd_set));
		/* Fall through */
	case FDMA_PAUSING:
	case FDMA_STOPPING:
		/* Hardware is pausing already, wait for interrupt */
		chan->sw_state = FDMA_STOPPING;
		spin_unlock_irqrestore(&fd->channel_lock,irqflags);
#if 0
		/* In some cases this is called from a context which cannot
		 * block, so disable the wait at the moment. */
		wait_event(chan->cur_cfg->wait_queue,
			   chan->sw_state == FDMA_IDLE);
#endif
		break;
	}

	return 0;
}

static int stb710x_fdma_free_params(struct stm_dma_params *params)
{
	struct fdma_dev * fd = params->params_ops_priv;
	struct stm_dma_params *this;

	for (this = params; this; this = this->next) {
		struct dma_xfer_descriptor *desc = (struct dma_xfer_descriptor*)this->priv;
		if (desc) {
			resize_nodelist_mem(fd, desc, 0, 0);
			kfree(desc);
		}
	}

	return 0;
}

static struct params_ops fdma_params_ops = {
	.free_params	= stb710x_fdma_free_params
};

/* Compile params part 1: generate template nodes */
static int _compile1(struct fdma_dev * fd,struct stm_dma_params *params)
{
	struct stm_dma_params *this;

	for (this = params; this; this = this->next) {
		struct dma_xfer_descriptor *desc;

		desc = (struct dma_xfer_descriptor*)this->priv;
		if (desc != NULL)
			continue;

		desc = kzalloc(sizeof(struct dma_xfer_descriptor), params->context);
		if (desc == NULL)
			return -ENOMEM;
		this->priv = desc;

		if (IS_TRANSFER_SG(this)){
			if(MODE_SRC_SCATTER==this->mode)
				desc->extrapolate_fn = extrapolate_sg_src;
			else if(MODE_DST_SCATTER==this->mode)
				desc->extrapolate_fn = extrapolate_sg_dst;
			else return -EINVAL;
		} else {
			desc->extrapolate_fn = extrapolate_simple;
		}

		if(this->mode == MODE_PACED){
			setup_paced_node(this, &desc->template_llu);
		} else {
			setup_freerunning_node(this, &desc->template_llu);
		}

		/* For any 1D transfers, line_len = nbytes */
		desc->extrapolate_line_len =
			!((DIM_SRC(this->dim) == 2) || (DIM_DST(this->dim) == 2));
	}

	return 0;
}

/* Compile params part 2: allocate node list */
static int _compile2(struct fdma_dev * fd,struct stm_dma_params *params)
{
	struct stm_dma_params *this;
	int numnodes = 0;
	struct dma_xfer_descriptor *desc;

	for (this = params; this; this = this->next) {
		if (IS_TRANSFER_SG(this))
			numnodes += this->sglen;
		else
			numnodes++;
	}

	desc = (struct dma_xfer_descriptor*)params->priv;
	if (desc->alloced_nodes < numnodes) {
		int res;
		res = resize_nodelist_mem(fd, desc, numnodes, params->context);
		if (res)
			return res;
	}

	return 0;
}

/* Compile params part 3: extrapolate */
static int _compile3(struct fdma_dev * fd,struct stm_dma_params *params)
{
	struct stm_dma_params *this;
	struct dma_xfer_descriptor *this_desc;
	struct llu_node *first_node, *last_node, *node;

	this = params;
	this_desc = (struct dma_xfer_descriptor*)this->priv;
	first_node = this_desc->llu_nodes;

	node = first_node;
	while (1) {

		last_node = this_desc->extrapolate_fn(this, this_desc, node);

		this = this->next;
		if (this == NULL)
			break;

		this_desc = (struct dma_xfer_descriptor*)this->priv;
		node = last_node + 1;
		last_node->virt_addr->next_item = node->dma_addr;
	}

	if(params->circular_llu)
		last_node->virt_addr->next_item = first_node->dma_addr;
	else
		last_node->virt_addr->next_item = 0;

	return 0;
}

static int stb710x_fdma_compile_params(struct fdma_dev * fd,struct stm_dma_params *params)
{
	int res;

	res = _compile1(fd, params);
	if (res)
		return res;

	res = _compile2(fd, params);
	if (res)
		return res;

	res = _compile3(fd, params);
	if (res == 0) {
		params->params_ops = &fdma_params_ops;
		params->params_ops_priv = fd;
	}

	return res;
}

static void stb710x_fdma_free(struct dma_channel *channel)
{
	struct fdma_dev *fd = FDMA_DEV(channel);
	struct channel_status *chan = FDMA_CHAN(channel);
	unsigned long irq_flags=0;

	spin_lock_irqsave(&fd->channel_lock,irq_flags);

	if (chan->sw_state != FDMA_IDLE) {
		spin_unlock_irqrestore(&fd->channel_lock,irq_flags);
		fdma_dbg(fd, "%s channel not idle\n",__FUNCTION__);
		return;
	}

	BUG_ON(!(IS_CHANNEL_IDLE(fd,channel->chan)));

	spin_unlock_irqrestore(&fd->channel_lock,irq_flags);
}

/* Note although this returns an int, the dma-api code throws this away. */
static int stb710x_fdma_configure(	struct dma_channel *channel,
				  	unsigned long flags)
{
	struct fdma_dev *fd = FDMA_DEV(channel);
	struct channel_status *chan = FDMA_CHAN(channel);
	struct stm_dma_params * params;
	unsigned long irq_flags=0;

	spin_lock_irqsave(&fd->channel_lock,irq_flags);
	if (chan->sw_state != FDMA_IDLE) {
		spin_unlock_irqrestore(&fd->channel_lock,irq_flags);
		fdma_dbg(fd, "%s channel not idle\n",__FUNCTION__);
		return -EBUSY;
	}

	params = (struct stm_dma_params *)flags;
	if(!((struct dma_xfer_descriptor*)(params->priv))->llu_nodes){
		fdma_dbg(fd, "%s no nodelist alloced\n",__FUNCTION__);
		spin_unlock_irqrestore(&fd->channel_lock,irq_flags);
		return -ENOMEM;
	}

	/* Now we are associating the compiled transfer llu & params to the channel*/
	chan->params = params;
	tasklet_init(&chan->fdma_complete,
		     params->comp_cb, (unsigned long)params->comp_cb_parm);
	tasklet_init(&chan->fdma_error,
		     params->err_cb, (unsigned long)params->err_cb_parm);
	chan->sw_state = FDMA_CONFIGURED;

	spin_unlock_irqrestore(&fd->channel_lock,irq_flags);

	return 0;
}

static int stb710x_fdma_xfer(
				struct dma_channel *channel,
				unsigned long sar,
			     	unsigned long dar,
			     	size_t count,
			     	unsigned int mode)
{
	struct fdma_dev *fd = FDMA_DEV(channel);
	struct channel_status *chan = FDMA_CHAN(channel);
	struct dma_xfer_descriptor *desc;
	unsigned long irqflags=0;

	/*we need to check that the compile has been completed*/
	spin_lock_irqsave(&fd->channel_lock, irqflags);

	if (chan->sw_state != FDMA_CONFIGURED) {
		spin_unlock_irqrestore(&fd->channel_lock, irqflags);
		return -EINVAL;
	}

	desc = (struct dma_xfer_descriptor*)chan->params->priv;

	BUG_ON(!(IS_CHANNEL_IDLE(fd,channel->chan)));

	fdma_start_channel(fd,channel->chan, desc->llu_nodes->dma_addr);
	chan->sw_state = FDMA_RUNNING;

	spin_unlock_irqrestore(&fd->channel_lock, irqflags);

	return 0;
}

static int stb710x_fdma_extended_op(struct dma_channel *  ch,
				    unsigned long opcode,
				    void * parm)
{
	struct fdma_dev *fd = FDMA_DEV(ch);
	switch(opcode){
		case STM_DMA_OP_FLUSH:
			return stb710x_fdma_pause(fd,ch,1);
		case STM_DMA_OP_PAUSE:
			return stb710x_fdma_pause(fd,ch,0);
		case STM_DMA_OP_UNPAUSE:
			return  stb710x_fdma_unpause(fd,ch);
		case STM_DMA_OP_STOP:
			return stb710x_fdma_stop(fd,ch);
		case STM_DMA_OP_COMPILE:
			return stb710x_fdma_compile_params(fd, (struct stm_dma_params *)parm);
		case STM_DMA_OP_STATUS:
			return stb710x_get_engine_status(fd,ch->chan);
		case STM_DMA_OP_REQ_CONFIG:
			return (int)stb710x_configure_pace_channel(fd, ch, (struct stm_dma_req_config *)parm);
		case STM_DMA_OP_REQ_FREE:
			fdma_req_free((struct stm_dma_req *)parm);
			return 0;
		default:
			return -ENOSYS;
	}
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * MODULE INIT & REGISTRATION
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static struct dma_ops stb710x_fdma_ops = {
	.request		= stb710x_fdma_request,
	.free			= stb710x_fdma_free,
	.get_residue		= stb710x_fdma_get_residue,
	.xfer			= stb710x_fdma_xfer,
	.configure		= stb710x_fdma_configure,
	.extend			= stb710x_fdma_extended_op,
};

static int __init fdma_driver_probe(struct platform_device *pdev)
{
	struct fdma_platform_device_data * plat_data;
	struct fdma_dev *fd=NULL;
	struct resource *res;
	int i=0;
	int err=0;

	plat_data = pdev->dev.platform_data;

	fd = kzalloc(sizeof(struct fdma_dev), GFP_KERNEL);
	if (fd == NULL) {
		return -ENOMEM;
	};

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		return -ENODEV;
	}

	fd->phys_mem = request_mem_region(res->start, res->end - res->start + 1,
					  pdev->name);
	if (fd->phys_mem == NULL) {
		return -EBUSY;
	};

	fd->io_base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (fd->io_base == NULL) {
		return -EINVAL;
	}

	fd->ch_min = plat_data->min_ch_num;
	fd->ch_max = plat_data->max_ch_num;
	fd->fdma_num = pdev->id;
	fd->ch_status_mask =
		((1ULL << (fd->ch_max*2)) - 1ULL) ^
		((1    << (fd->ch_min*2)) - 1);

	memcpy(&fd->regs,(u32*)plat_data->registers_ptr,sizeof(struct fdma_regs));
	fd->fw_name = plat_data->fw_device_name;
	fd->fw = plat_data->fw;

	/* 7200: Req lines 0 and 31 are connected internally, not to the xbar */
	fd->req_lines_inuse = (1<<31) | (1<<0);

	spin_lock_init(&(fd)->channel_lock);
	init_waitqueue_head(&(fd)->fw_load_q);

	fd->dma_info.nr_channels = (fd->ch_max+1) - fd->ch_min;
	fd->dma_info.ops	= &stb710x_fdma_ops;
	fd->dma_info.flags	= DMAC_CHANNELS_TEI_CAPABLE;
	strlcpy(fd->name, STM_DMAC_ID, FDMA_NAME_LEN);
	if (pdev->id != -1) {
		int len=strlen(fd->name);
		snprintf(fd->name+len, FDMA_NAME_LEN-len, ".%d", pdev->id);
	}
	fd->dma_info.name = fd->name;

	if(register_dmac(&fd->dma_info)!=0)
		printk("%s Error Registering DMAC\n",__FUNCTION__);
	/*must take account of CH 0*/

	for (i=fd->ch_min; i<=fd->ch_max; i++) {
		struct dma_channel *channel;
		channel = get_dma_channel(i);
		channel->priv_data = &fd->channel[i];
		fd->channel[i].cur_cfg = channel;
		fd->channel[i].fd = fd;
	}

	err =request_irq(platform_get_irq(pdev, 0),
			 fdma_irq,
			 IRQF_DISABLED | IRQF_SHARED,
			 fd->name,
			 fd);
	if(err <0)
		panic(" Cant Register irq %d for FDMA engine err %d\n",
					fd->irq_val,err);


	fdma_register_caps(fd);

	fdma_check_firmware_state(fd);

	platform_set_drvdata(pdev, fd);

	return 0;
}

static int fdma_driver_remove(struct platform_device *pdev)
{
	struct fdma_dev *fd = platform_get_drvdata(pdev);

	fdma_disable_all_channels(fd);
	iounmap(fd->io_base);
	dma_pool_destroy(fd->llu_pool);
	free_irq(fd->irq_val, fd);
	unregister_dmac(&fd->dma_info);
	release_resource(fd->phys_mem);
	kfree(fd);

	return 0;
}

static struct platform_driver fdma_driver = {
	.driver = {
		.name = "stmfdma",
	},
	.probe = fdma_driver_probe,
	.remove = fdma_driver_remove,
};

static int __init fdma_init(void)
{
	return platform_driver_register(&fdma_driver);
}

static void __exit fdma_exit(void)
{
	platform_driver_unregister(&fdma_driver);
}

module_init(fdma_init)
module_exit(fdma_exit)
