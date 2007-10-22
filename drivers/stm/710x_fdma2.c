/*
 *  STb710x FDMA Driver
 *  Copyright (c) 2005 STMicroelectronics Limited.
 *  Authors: 	Mark Glaisher <Mark.Glaisher@st.com>
 * 		Stuart Menefy <Stuart.Menefy@st.com>
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
#include <linux/stm/710x_fdma.h>
#include <linux/platform_device.h>
static fdma_chip chip;

static int setup_freerunning_node(struct stm_dma_params *params)
{
	fdma_llu_entry* llu = params->priv.node->virt_addr;
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

	/*correct parms for the dimension ar set by API layer*/
	llu->line_len		= params->line_len;
	llu->sstride 		= params->sstride;
	llu->dstride 		= params->dstride;
	return 0;
}

static int setup_paced_node(struct stm_dma_params *params)
{
	fdma_llu_entry* llu = params->priv.node->virt_addr;

	memset(llu, 0, sizeof(*llu));

	llu->control= params->req_line;
	llu->size_bytes= params->node_bytes;
	llu->line_len = params->node_bytes;

	if (params->node_pause)
		/*in order to recieve the pause interrupt
		 *  we must also enable end of node interrupts*/
		llu->control |=  SET_NODE_COMP_PAUSE | SET_NODE_COMP_IRQ;

	if (params->node_interrupt)
		llu->control |= SET_NODE_COMP_IRQ;

	if (params->dim & DIM_1_x_0 ){
		llu->control |= NODE_ADDR_INCR <<SOURCE_ADDR;
		llu->control |= NODE_ADDR_STATIC <<DEST_ADDR;
	}
	else if (params->dim & DIM_0_x_1){
		llu->control |= NODE_ADDR_STATIC <<SOURCE_ADDR;
		llu->control |= NODE_ADDR_INCR <<DEST_ADDR;
	}
	else return -EINVAL;

	return 0;
}

static int stb710x_get_engine_status(int channel)
{
	return readl(CMD_STAT_REG(channel))&3;
}

static void extrapolate_simple(struct stm_dma_params *xfer)
{
	struct fdma_llu_entry* dest_llu =xfer->priv.node->virt_addr;
	dest_llu->size_bytes = xfer->node_bytes;
	dest_llu->saddr = xfer->sar;
	dest_llu->daddr = xfer->dar;
	if (xfer->priv.extrapolate_line_len)
		dest_llu->line_len = xfer->node_bytes;
}

static void extrapolate_sg_src(struct stm_dma_params *xfer)
{
	int i;
	struct scatterlist * sg = xfer->srcsg;
	struct fdma_llu_entry* dest_llu=0;
	struct llu_node * cur_node = xfer->priv.node;
	unsigned long control =xfer->priv.node->virt_addr->control;

	for (i=0; i<xfer->priv.sublist_nents; i++) {
		dest_llu = cur_node->virt_addr;
		dest_llu->control = control;
		dest_llu->size_bytes = sg_dma_len(sg);
		dest_llu->saddr = sg_dma_address(sg);
		dest_llu->daddr = xfer->dar;
		dest_llu->sstride = xfer->sstride;
		if (xfer->priv.extrapolate_line_len)
			dest_llu->line_len = sg_dma_len(sg);
		else
			dest_llu->line_len = xfer->line_len;
		dest_llu->dstride=0;
		cur_node++;
		dest_llu->next_item = cur_node->dma_addr;
		sg++;
	}
	dest_llu->next_item=0;
}
static void extrapolate_sg_dst(struct stm_dma_params *xfer)
{
	int i;
	struct scatterlist * sg = xfer->dstsg;
	struct fdma_llu_entry* dest_llu=0;
	struct llu_node * cur_node = xfer->priv.node;
	unsigned long control =xfer->priv.node->virt_addr->control;

	for (i=0; i<xfer->priv.sublist_nents; i++) {
		dest_llu= cur_node->virt_addr;
		dest_llu->control = control;
		dest_llu->size_bytes = sg_dma_len(sg);
		dest_llu->saddr = xfer->sar;
		dest_llu->daddr = sg_dma_address(sg);
		dest_llu->sstride = 0;
		if (xfer->priv.extrapolate_line_len)
			dest_llu->line_len = sg_dma_len(sg);
		else
			dest_llu->line_len = xfer->line_len;
		dest_llu->dstride=xfer->dstride;
		cur_node++;
		dest_llu->next_item = cur_node->dma_addr;
		sg++;
	}
	dest_llu->next_item=0;
}

static inline void set_to_sublist_end(struct stm_dma_params *transfer,unsigned addr)
{
	struct llu_node * cur_node = transfer->priv.node;
	struct fdma_llu_entry* dest_llu=0;
	int i;

	for (i=0; i<transfer->priv.sublist_nents; i++)
		dest_llu = (cur_node++)->virt_addr;

	dest_llu->next_item = addr;
}

static void create_llu_list(struct stm_dma_params *transfer)
{
	struct stm_dma_params * this = transfer;
	struct stm_dma_params * next = transfer->next;

	while(this->next){
		if(this->mode == MODE_SRC_SCATTER || this->mode == MODE_DST_SCATTER)
			set_to_sublist_end(this,next->priv.node->dma_addr);
		else
			this->priv.node->virt_addr->next_item = next->priv.node->dma_addr;
		this = next;
		next = next->next;
	};

	if(transfer->circular_llu)
		set_to_sublist_end(this,transfer->priv.node->dma_addr);
	else
		set_to_sublist_end(this,0);
}

static int alloc_nodelist_mem(struct stm_dma_params *transfer,int nents)
{
	int res=0;
	int i=0;
	int list_size = sizeof(struct llu_node)*nents;
	struct llu_node* first=0;
	struct llu_node* new_node = kmalloc(list_size,transfer->context);

	if (new_node == NULL)
		return -ENOMEM;

	first = new_node;

	for(;i<nents;i++){
		new_node->virt_addr = dma_pool_alloc(
					chip.llu_pool,
					transfer->context,
					&new_node->dma_addr);

		if (new_node->virt_addr == NULL){
			/* SIM need to free nodes as well */
			kfree(first);
			return -ENOMEM;
		}
		new_node++;
	}
	transfer->priv.node = first;
	transfer->priv.alloced_nents = nents;
	return res;
}

static inline int fdma_sh_compatibility_setup(struct dma_channel *channel,
					struct channel_status * chan,
					unsigned long sar, unsigned long dar,
					size_t count, unsigned int mode)
{
	struct stm_dma_params dmap= {0};
	dmap.mode =mode;
	dmap.dim = (channel->flags & 0xf0);
	dmap.context =(channel->flags & STM_DMA_SETUP_CONTEXT_ISR) ?
						GFP_ATOMIC:GFP_KERNEL;
	dmap.sar =sar;
	dmap.dar =dar;
	dmap.node_bytes = count;
	dmap.line_len = count;
	dmap.priv.extrapolate_fn = extrapolate_simple;
	dmap.priv.nodelist_setup =setup_freerunning_node;
	if (alloc_nodelist_mem(&dmap,1) != 0){
		fdma_log("%s Cant allocate memory for xfer\n",__FUNCTION__);
		return -ENOMEM;
	}
	dmap.priv.nodelist_setup(&dmap);
	dmap.priv.extrapolate_fn(&dmap);
	create_llu_list(&dmap);
	memcpy(&chan->params,&dmap,sizeof(struct stm_dma_params));
	return 0;
}

static inline void fdma_check_xfer_params(stm_dma_params * params)
{
	BUG_ON((params->priv.node->dma_addr & 0x1F)!=0);
	BUG_ON(ASSERT_NODE_BUS_ADDR(params->priv.node->virt_addr->saddr)==0);
	BUG_ON(ASSERT_NODE_BUS_ADDR(params->priv.node->virt_addr->daddr)==0);
	BUG_ON(params->priv.node->virt_addr->size_bytes==0);
}

static void completion_ok(int channel)
{
	unsigned long irqflags=0;
	struct channel_status *chan= &chip.channel[channel];
	void (*comp_cb)(void*) = chan->params.comp_cb;
	void *comp_cb_parm = chan->params.comp_cb_parm;

	if((!chan->callback_only) || chan->ch_term){
		spin_lock_irqsave(&chip.channel_lock,irqflags);
		memset(&chip.channel[channel].params,0,sizeof(struct stm_dma_params));
		chan->is_xferring = 0;
		chan->ch_term=0;
		spin_unlock_irqrestore(&chip.channel_lock,irqflags);
	}
	chan->callback_only=0;

	wake_up(&chan->cur_cfg->wait_queue);
	if (comp_cb)
		comp_cb(comp_cb_parm);
}

static void completion_err(int channel)
{
	unsigned long irqflags=0;
	struct channel_status *chan= &chip.channel[channel];
	void (*err_cb)(void*) = chan->params.err_cb;
	void *err_cb_parm = chan->params.err_cb_parm;

	if((!chan->callback_only)  || chan->ch_term){
		spin_lock_irqsave(&chip.channel_lock,irqflags);
		memset(&chan->params,0,sizeof(struct stm_dma_params));
		chan->is_xferring = 0;
		chan->ch_term=0;
		spin_unlock_irqrestore(&chip.channel_lock,irqflags);
	}
	chan->callback_only=0;
	wake_up(&chan->cur_cfg->wait_queue);
	if(err_cb)
		err_cb(err_cb_parm);
}

static void handle_completion(int channel,int comp_code)
{
	struct channel_status *chan = &chip.channel[channel];

	if(FDMA_COMPLETE_OK == comp_code) {
		if(chan->params.comp_cb_isr)
			completion_ok(channel);
		else{
			chan->fdma_complete.data = channel;
			tasklet_schedule(&chan->fdma_complete);
		}
	} else {
		if(chan->params.err_cb_isr)
			completion_err(channel);
		else{
			chan->fdma_error.data = channel;
			tasklet_schedule(&chan->fdma_error);
		}
	}
}

static int fdma_start_channel(int ch_num,
			      unsigned long start_addr)
{
	unsigned long irqflags=0;
	u32 cmd_sta_value = (start_addr  | CMDSTAT_FDMA_START_CHANNEL);

	spin_lock_irqsave(&chip.fdma_lock,irqflags);
	writel(cmd_sta_value,CMD_STAT_REG(ch_num));
	writel(MBOX_STR_CMD(ch_num),chip.io_base +chip.regs.fdma_cmd_set);
	spin_unlock_irqrestore(&chip.fdma_lock,irqflags);
	return 0;
}

static void fdma_cb_continue(int channel)
{
	unsigned long irqflags=0;
	unsigned long new_reg_val=0;
	struct channel_status * ch = &chip.channel[channel];
	if(ch->comp_cb && ch->comp_cb_isr)
		ch->comp_cb(ch->comp_cb_param);
	else{
		ch->fdma_complete.data = channel;
		if(!ch->ch_term)
			ch->callback_only = 1;
		tasklet_schedule(&ch->fdma_complete);
	}
	if(ch->ch_pause){ /*usr signals pause*/
		ch->ch_pause=0;
		/*we want to continue the current transfer*/
		writel(MBOX_STR_CMD(channel),chip.io_base + chip.regs.fdma_cmd_set);
	} else {
		/*we need to load the next node*/
		spin_lock_irqsave(&chip.fdma_lock,irqflags);
		new_reg_val = (readl(CH_PTR_REG(channel))|CMDSTAT_FDMA_START_CHANNEL);
		writel( new_reg_val,CMD_STAT_REG(channel) );
		writel(MBOX_STR_CMD(channel) ,chip.io_base + chip.regs.fdma_cmd_set);
		spin_unlock_irqrestore(&chip.fdma_lock,irqflags);
	}
}

static inline void __handle_fdma_err_irq(int channel)
{
	printk("%s ERROR CH_%d err %d\n",
		__FUNCTION__,
		channel,
		(int)( readl(CMD_STAT_REG(channel))& 0x1c) >>2);
			/*err is bits 2-4*/
	/*clearing the channel interface here will stop further
	 * transactions after the err and reset the channel*/
	writel(0,CMD_STAT_REG(channel));
	writel(readl(chip.io_base + chip.regs.fdma_cmd_sta),chip.io_base + chip.regs.fdma_cmd_clr);
	handle_completion(channel,FDMA_COMPLETE_ERR);
}

static inline void __handle_fdma_completion_irq(int channel)
{
	/*now we look for reason of int may be*/
	switch(stb710x_get_engine_status(channel)){
		case FDMA_CHANNEL_PAUSED:
			if(chip.channel[channel].ch_term==1){
				writel(0,CMD_STAT_REG(channel));
				handle_completion(channel,FDMA_COMPLETE_OK);
			}
			else if(chip.channel[channel].ch_pause)
				chip.channel[channel].ch_pause=0;
			else
				fdma_cb_continue(channel);
			break;
		case FDMA_CHANNEL_IDLE:
			handle_completion(channel,FDMA_COMPLETE_OK);
			break;
		case FDMA_CHANNEL_RUNNING:
			break;
		default:
			fdma_log("ERR::FDMA2 unknown interrupt status \n");
			handle_completion(channel,FDMA_COMPLETE_ERR);
	}
}

static irqreturn_t fdma_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int channel=chip.ch_min;
	unsigned long clear_mask = ~((0x1 << (chip.ch_min*2))-1);
	/*this gives us a bitmask of the channels in available range to clear*/
	u32 int_stat_val = readl(chip.io_base + chip.regs.fdma_int_sta);
	u32 cur_val = int_stat_val >> (channel *2);

	writel(int_stat_val & clear_mask, chip.io_base +chip.regs.fdma_int_clr);
	do{
		/*error interrupts will raise boths bits, so check
		 * the err bit first*/
		if(unlikely(cur_val & 2))
				__handle_fdma_err_irq(channel);

		else if (cur_val & 1)
				__handle_fdma_completion_irq(channel);

		cur_val = cur_val>>2;
	}while(channel++ < chip.ch_max);

	/*here we check to see if there is still pending ints for the other dmac, if so
	 * rely on it to signal IRQ_HANDLED once all vectors are cleared, we return IRQ_NONE.
	 * otherwise we have handled everything so we can now safely returnd IRQ_HANDLED
	 * to lower the IRQ.*/
	return (cur_val == 0) && ((int_stat_val & (0x1 << ((chip.ch_min*2)-1)))==0)  ?
			IRQ_HANDLED:
			IRQ_NONE;
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * FIRMWARE DOWNLOAD & ENGINE INIT
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static int fdma_get_fw_revision(char* revision, int major, int minor)
{
	int reg = readl(chip.io_base + chip.regs.fdma_dmem_region);
	major  = (reg &  0xff00) >>8;
	minor  = reg &  0xff;

	if(! (major || minor))
		return -ENODEV;

	sprintf(revision,"FDMA_FW V%d.%d",major,minor);
	return 0;
}

static int fdma_get_hw_revision(char * revision,int major, int minor)
{
	major = readl(chip.io_base + chip.regs.fdma_id);
	minor = readl(chip.io_base + chip.regs.fdma_ver);
	sprintf(revision,"SLIMCORE_HW V%d.%d",major,minor);
	return 0;
}

static int fdma_do_bootload(void)
{
	int major=0,minor=0;
	char fw_revision[20];
	char hw_revision[20];
	device_t* ptr=0;
	fdma_fw_data_t fw=chip.fw;
	unsigned long unused_ibytes;
	unsigned long unused_dbytes;
	unsigned long irqflags;
	void * addr =(char*)chip.io_base;

	fdma_log("FDMA: Loading Firmware...");
	unused_ibytes= fw.imem_len - fw.imem_fw_sz;
	unused_dbytes= fw.dmem_len - fw.dmem_fw_sz;

	spin_lock_irqsave(&chip.fdma_lock,irqflags);
	ptr = (device_t*) ((char*) addr +chip.regs.fdma_dmem_region);
	memcpy((void*)ptr,&fw.data_reg[0],fw.dmem_fw_sz * sizeof(u32));
	if(unused_dbytes){
		ptr =(device_t*) ((char*)addr +chip.regs.fdma_dmem_region
				  +(fw.dmem_fw_sz*sizeof(u32)));
		memset((void*)ptr ,0, unused_dbytes);
	}

	ptr = (device_t*) ((char*) addr +chip.regs.fdma_imem_region);
	memcpy((void*)ptr,&fw.imem_reg[0],fw.imem_fw_sz* sizeof(u32));
	if(unused_ibytes){
		ptr =(device_t*) ((char*)addr +chip.regs.fdma_imem_region
				  +(fw.imem_fw_sz*sizeof(u32)));
		memset((void*)ptr,0, unused_ibytes);
	}
	spin_unlock_irqrestore(&chip.fdma_lock,irqflags);

	chip.firmware_loaded=1;
	fdma_get_fw_revision(&fw_revision[0],major,minor);
	fdma_get_hw_revision(&hw_revision[0],major,minor);
	wake_up(&chip.fw_load_q);

	printk(KERN_INFO "   STB_%dC%d %s %s \n",
		 chip.cpu_subtype,chip.cpu_rev,hw_revision,fw_revision);
	return 0;
}

static void fdma_initialise(void)
{
/*These pokes come from the current STAPI tree.
 * The three magic vals are pokes to undocumented regs so
 * we don't know what they mean.
 *
 * The effect is to turn on and initialise the clocks
 * and set all channels off*/

	/*clear the status regs MBOX & IRQ*/
	writel(CLEAR_WORD, chip.io_base+chip.regs.fdma_int_clr);
	writel(CLEAR_WORD, chip.io_base+chip.regs.fdma_cmd_clr);

	/* Enable the FDMA block */
	writel(1,chip.io_base+chip.regs.fdma_sync_reg);
	writel(5,chip.io_base+chip.regs.fdma_clk_gate);
	writel(0,chip.io_base+chip.regs.fdma_clk_gate);

}
/*this function enables messaging and intr generation for all channels &
 * starts the fdma running*/
static int fdma_enable_all_channels(void)
{
	writel(CLEAR_WORD,chip.io_base + chip.regs.fdma_int_mask);
	writel(CLEAR_WORD,chip.io_base + chip.regs.fdma_cmd_mask);
	writel(ENABLE_FLG ,chip.io_base +chip.regs.fdma_en);
	return (readl(chip.io_base + chip.regs.fdma_en) &1);
}

static void fdma_reset_channels(void)
{
	int channel=0;
	for(;channel <(chip.ch_max-1);channel++)
		writel(0,CMD_STAT_REG(0));
}

static int stb710x_configure_pace_channel(struct fmdareq_RequestConfig_s * prq)
{
	unsigned long ReqC=0;
	unsigned long req_base_reg = chip.io_base+chip.regs.fdma_req_ctln;

	if(prq->Index <0 || prq->Index > chip.num_req_lines)
		return -EINVAL;

	ReqC = (u32)(prq->HoldOff    & 0x0f) <<  0;/*Bits 3.0*/
	ReqC |= (u32)(prq->OpCode    & 0x0f) <<  4;/*7..4*/
	ReqC |= (u32)(prq->Access    & 0x01) << 14;/*14*/
	ReqC |= (u32)(prq->Initiator & 0x03) << 22;/*23..22*/
	ReqC |= (u32)((prq->Count-1) & 0x1F) << 24;/*28..24*/
	ReqC |= (u32)(prq->Increment & 0x01) << 29;/*29*/

	writel(ReqC,req_base_reg+(prq->Index *CMD_STAT_OFFSET));
	return (readl(req_base_reg+(prq->Index *CMD_STAT_OFFSET)) == ReqC) ?
		 0:
		 -ENODEV;
}

static void fdma_initialise_req_ctl(void)
{
	int i=0;
	for(;i < (chip.num_req_lines -1 );i++){
		if(stb710x_configure_pace_channel(&chip.req_tbl[i])<0)
			fdma_log("%s Error programming FDMA_REQ %d\n",
					__FUNCTION__, chip.req_tbl[i].Index);
	}
}

static int fdma_register_caps(void)
{
	int channel = chip.ch_min;
	int res=0;
	int num_caps = chip.ch_max - chip.ch_min + 1;
	struct dma_chan_caps  dmac_caps[num_caps];
	const char  * dmac_id = (const char *)STM_DMAC_ID;
	static const char* hb_caps[] = {STM_DMA_CAP_HIGH_BW,NULL};
	static const char* lb_caps[] = {STM_DMA_CAP_LOW_BW,NULL};

	for (;channel <= chip.ch_max;channel++) {
		dmac_caps[channel-chip.ch_min].ch_num = channel;
		dmac_caps[channel-chip.ch_min].caplist =
			(channel < 4) ? hb_caps : lb_caps;
	}
	res= register_chan_caps(dmac_id,&dmac_caps[0]);

	if(res!=0){
		fdma_log("%s %s failed to register capabilities\n",
			__FUNCTION__,dmac_id);
		return -ENODEV;
	}
	else return 0;
}

static int fdma_run_initialise_sequence(void)
{
	int i=0;
	chip.llu_pool = dma_pool_create("STB710X FDMA", NULL,
					sizeof(struct fdma_llu_entry),32,0);
	if (chip.llu_pool == NULL) {
		fdma_log("%s Can't allocate dma_pool memory",__FUNCTION__);
		return -ENOMEM;
	}
	fdma_initialise();
	fdma_reset_channels();
	fdma_initialise_req_ctl();

	for(i=0;i < chip.ch_max+1;i++){
		tasklet_init(&chip.channel[i].fdma_error,(void*)completion_err,i);
		tasklet_init(&chip.channel[i].fdma_complete,(void*)completion_ok,i);
	}
	if(!fdma_enable_all_channels())
		return -ENODEV;
	else return  0;
}

static int fdma_load_firmware(void)
{
	unsigned long irqflags=0;
	spin_lock_irqsave(&chip.channel_lock,irqflags);
	switch ( chip.firmware_loaded ) {
		case 0:
			chip.firmware_loaded = -1;
			spin_unlock_irqrestore(&chip.channel_lock,irqflags);
			if (fdma_do_bootload()!=0 ){
				chip.firmware_loaded=0;
				return  -ENOMEM;
			}
			if(fdma_run_initialise_sequence()!=0)
				return -ENODEV;

			return (chip.firmware_loaded==1) ? 0:-ENODEV;
		case 1:
			spin_unlock_irqrestore(&chip.channel_lock,irqflags);
			return 0;
		default:
		case -1:
			spin_unlock_irqrestore(&chip.channel_lock,irqflags);
			wait_event_interruptible(chip.fw_load_q,(chip.firmware_loaded==1));
			if(!chip.firmware_loaded)
				return -ENODEV;
			else return 0;
	}
	return 0;
}

static int fdma_check_firmware_state(void)
{
	return (chip.firmware_loaded) ? 0:fdma_load_firmware();
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*
 * Linux -SH DMA API hooks
 *---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

/*returns the number of bytes left to transfer for the current node*/
static  int stb710x_fdma_get_residue(struct dma_channel *chan)
{
	unsigned long irqflags;
	u32 chan_base = chip.io_base + (chan->chan * NODE_DATA_OFFSET);
	unsigned long total = 0,count=0;
	void *first_ptr=0;
	fdma_llu_entry *cur_ptr;

	spin_lock_irqsave(&chip.fdma_lock, irqflags);
	count = readl(chan_base +chip.regs.fdma_cntn);
	/*first read the current node data*/
	first_ptr = (void *) readl(chan_base + chip.regs.fdma_ptrn);
	if(! first_ptr)
		goto list_complete;

	first_ptr = P2SEGADDR(first_ptr);
	/* Accumulate the bytes remaining in the list */
	cur_ptr = first_ptr;
	do {
		if(first_ptr >=(void*)P2SEGADDR(cur_ptr->next_item)
		   || cur_ptr->next_item ==0)
			goto list_complete;

		total += cur_ptr->size_bytes;
	} while ((cur_ptr = P2SEGADDR((fdma_llu_entry *) cur_ptr->next_item))!=0);
list_complete:
	spin_unlock_irqrestore(&chip.fdma_lock, irqflags);
	total+= count;
	return total;
}



/*must only be called when channel is in pasued state*/
static int stb710x_fdma_unpause(struct dma_channel * chan)
{
	if(IS_CHANNEL_PAUSED(chan->chan)){
		writel(MBOX_CMD_START_CHANNEL << (chan->chan*2),
				chip.io_base +chip.regs.fdma_cmd_set);
		return 0;
	}
	return -EBUSY;
}

static int stb710x_fdma_pause(struct dma_channel * chan,int flush)
{
	unsigned long irqflags=0;
	spin_lock_irqsave(&chip.fdma_lock,irqflags);
	if(IS_CHANNEL_RUNNING(chan->chan)){
		chip.channel[chan->chan].ch_pause =1;
		if(flush)
			writel( MBOX_CMD_PAUSE_FLUSH_CHANNEL << (chan->chan*2),
					chip.io_base + chip.regs.fdma_cmd_set);
		else
			writel( MBOX_CMD_PAUSE_CHANNEL << (chan->chan*2),
					chip.io_base + chip.regs.fdma_cmd_set);

		spin_unlock_irqrestore(&chip.fdma_lock,irqflags);
		return IS_CHANNEL_PAUSED (chan->chan) ? 0:
			-ENODEV;
	}
	spin_unlock_irqrestore(&chip.fdma_lock,irqflags);
	fdma_log("%s Cant Pause - CH_%d not running\n",__FUNCTION__,chan->chan);
	return -EBUSY;
}


static int stb710x_fdma_request(struct dma_channel *ch)
{
	if(fdma_check_firmware_state()==0){
		if(!(IS_CHANNEL_RESERVED(ch->chan))){
			chip.channel[ch->chan].reserved=1;
			chip.channel[ch->chan].cur_cfg = ch;
			return 0;
		}
	/*the upper level API code requires a positive err code*/
		else return EBUSY;
	}
	return ENOSYS;
}


static int stb710x_fdma_stop(struct dma_channel *chan)
{
	unsigned long cmd_val = (MBOX_CMD_PAUSE_CHANNEL << (chan->chan*2));
	unsigned long irqflags=0;

	/*Issuing a pause on an inactive channel results in the FDMA
	* attempting to load the next ptr*/
	spin_lock_irqsave(&chip.fdma_lock,irqflags);
	if(!(IS_CHANNEL_IDLE(chan->chan))){
		chip.channel[chan->chan].ch_term=1;
		writel(cmd_val,(chip.io_base +chip.regs.fdma_cmd_set));
		spin_unlock_irqrestore(&chip.fdma_lock,irqflags);
		return 0;
	}
	else{/*throw an error if trying to stop an inactive channel*/
		fdma_log("%s Cant stop Idle Channel %d \n",__FUNCTION__,chan->chan);
		spin_unlock_irqrestore(&chip.fdma_lock,irqflags);
		return -ENODEV;
	}
}

static int stb710x_list_mem_free(stm_dma_params * xfer_ptr)
{
	if(xfer_ptr->priv.node){
		int i=0;
		for(;i<xfer_ptr->priv.alloced_nents;i++){
			dma_pool_free(
				chip.llu_pool,
				xfer_ptr->priv.node[i].virt_addr,
				xfer_ptr->priv.node[i].dma_addr);
		}
		kfree(xfer_ptr->priv.node);
		xfer_ptr->priv.alloced_nents =0;
		return 0;
	}
	else return -ENOMEM;
}

/*not be be called with locks held !*/
static int handle_ch_busy(int channel)
{
	struct channel_status *chan = &chip.channel[channel];

	if(chan->is_xferring){
		fdma_log("%s Channel_%d Busy - xfer %s \n",__FUNCTION__,
			channel,(chan->params.blocking ?"BLOCK":"ABORT"));
		if(chan->params.blocking){
			BUG_ON(in_interrupt());
			wait_event(chan->cur_cfg->wait_queue,(chan->is_xferring==0));
		}
		else return -EBUSY;
	}
	return 0;
}

static int stb710x_fdma_compile_params(struct stm_dma_params *params)
{
	struct stm_dma_params * this =  params;

	if(unlikely((params->context == GFP_ATOMIC) && params->blocking)){
		fdma_log("%s Cant specify blocking transfers from isr ctx\n",__FUNCTION__);
		return -EINVAL;
	}
	do{
		/*here we are looking for a re/un-used node - if so then we must alloc enough mem
		for the dma node or nodes(sg only).*/
		if( (!IS_NODE_MALLOCED(this->priv)) ||(!IS_NODELIST_EQUAL(this->priv))) {

			if( IS_NODE_MALLOCED(this->priv) &&(!IS_NODELIST_EQUAL(this->priv)))
				stb710x_list_mem_free(this);

			if (alloc_nodelist_mem(this,this->priv.sublist_nents) != 0){
				fdma_log("%s Cant allocate memory for xfer\n",__FUNCTION__);
				return -ENOMEM;
			}
			if(IS_TRANSFER_SG(this)){

				if(MODE_SRC_SCATTER==this->mode)
					this->priv.extrapolate_fn = extrapolate_sg_src;
				else if(MODE_DST_SCATTER==this->mode)
					this->priv.extrapolate_fn = extrapolate_sg_dst;
				else return -EINVAL;
			}
			else this->priv.extrapolate_fn = extrapolate_simple;

			this->priv.nodelist_setup =(this->mode == MODE_PACED) ?
					setup_paced_node:
					setup_freerunning_node;

			/* For any 1D transfers, line_len = nbytes */
			this->priv.extrapolate_line_len =
				!((DIM_SRC(this->dim) == 2) || (DIM_DST(this->dim) == 2));
		}
		this->priv.nodelist_setup(this);
		this->priv.extrapolate_fn(this);
		this = this->next;
	}while(this);

	create_llu_list(params);
	return 0;
}

static void stb710x_fdma_free(struct dma_channel *channel)
{
	struct channel_status *this_ch = &chip.channel[channel->chan];
	if(CHAN_OTB(channel->chan)!=0)
		return;

	if(!IS_CHANNEL_RESERVED(channel->chan))
		return;
	else this_ch->reserved=0;

	if(!(IS_CHANNEL_IDLE(channel->chan))){
		stb710x_fdma_stop(channel);
		/*TODO :-should have some confirmation the cmd has been processed here before
		 * continuing - either a wait, not always possible, or a spin ? */
		if(handle_ch_busy(channel->chan)==-EBUSY){
			fdma_log("%s Cant free memory on active channel %d sts %d\n",
				__FUNCTION__,channel->chan,stb710x_get_engine_status(channel->chan));
			return;
		}
	}
        if(IS_NODE_MALLOCED(this_ch->params.priv))
	       stb710x_list_mem_free(&this_ch->params);

        memset(this_ch,0,sizeof(struct channel_status));
}

static int stb710x_fdma_configure(struct dma_channel *channel,
				  unsigned long flags)
{
	struct channel_status *chan;
	struct stm_dma_params * params;
	unsigned long irq_flags=0;
	BUG_ON( CHAN_OTB(channel->chan)==0);

	if(handle_ch_busy(channel->chan)==-EBUSY)
		return -EBUSY;

	if(unlikely(flags & MODE_SH_COMPATIBILITY)){
	/*nothing to do here - we setup our llu when we
	 * have the data on the call to xfer*/
		channel->flags = flags;
		return 0;
	}
	spin_lock_irqsave(&chip.channel_lock,irq_flags);
	chan = &chip.channel[channel->chan];

	if(channel->priv_data != NULL)
		params  = (stm_dma_params *) channel->priv_data;
	else{
		 fdma_log("%s Channel %d not compiled - xfer abort ! \n",
			  __FUNCTION__,channel->chan);
		 spin_unlock_irqrestore(&chip.channel_lock,irq_flags);
		 return -EINVAL;
	}
	if(!params->priv.node->dma_addr){
		fdma_log("%s no nodelist allocation !\n",__FUNCTION__);
		spin_unlock_irqrestore(&chip.channel_lock,irq_flags);
		return -ENOMEM;
	}
	/*Nodelist Not Configured to channel or memory badness*/
	/*now we are associating the compiled transfer llu & parms to the channel*/
	memcpy(&chan->params,params,sizeof(struct stm_dma_params));
	chan->is_xferring=1;
	spin_unlock_irqrestore(&chip.channel_lock,irq_flags);
	return 0;
}


static int stb710x_fdma_xfer(struct dma_channel *channel, unsigned long sar,
			     unsigned long dar, size_t count, unsigned int mode)
{
	struct channel_status *chan = &chip.channel[channel->chan];
	unsigned long irqflags=0;

	BUG_ON(CHAN_OTB(channel->chan)==0);
	if(unlikely(MODE_SH_COMPATIBILITY == mode)){
		if(fdma_sh_compatibility_setup(channel,chan,sar,dar,count,mode)!=0)
			return -ENOMEM;
	}
	/*we need to check that the compile has been completed*/
	spin_lock_irqsave(&chip.channel_lock, irqflags);

	if(!IS_NODE_MALLOCED(chan->params.priv)){
		spin_unlock_irqrestore(&chip.channel_lock, irqflags);
		fdma_log("%s CH_%d invalid descriptor\n",__FUNCTION__,channel->chan);
		return -EINVAL;
	}
	if(!(IS_CHANNEL_IDLE(channel->chan))){
		spin_unlock_irqrestore(&chip.channel_lock, irqflags);
		fdma_log("%s FDMA engine not ready - status %d\n",
			__FUNCTION__,
			stb710x_get_engine_status(channel->chan));
		return -ENODEV;
	}
	fdma_check_xfer_params(&chan->params);

	spin_unlock_irqrestore(&chip.channel_lock, irqflags);
	return fdma_start_channel(channel->chan, chan->params.priv.node->dma_addr);
}

static int stb710x_fdma_extended_op(struct dma_channel *  ch,
				    unsigned long opcode,
				    void * parm)
{
	switch(opcode){
		case STM_DMA_OP_PAUSE:
			return stb710x_fdma_pause(ch,(int) parm);
		case STM_DMA_OP_UNPAUSE:
			return  stb710x_fdma_unpause(ch);
		case STM_DMA_OP_STOP:
			return stb710x_fdma_stop(ch);
		case STM_DMA_OP_COMPILE:
			return stb710x_fdma_compile_params((struct stm_dma_params *)parm);
		case STM_DMA_OP_STATUS:
			return stb710x_get_engine_status(ch->chan);
		case STM_DMA_OP_MEM_FREE:
			return stb710x_list_mem_free((struct stm_dma_params *)parm);
		case STM_DMA_OP_PACING:
			return stb710x_configure_pace_channel((struct fmdareq_RequestConfig_s *)parm);
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

static struct dma_info stb710x_fdma_info = {
	.name			= (char*)STM_DMAC_ID,
/*	.nr_channels		= defined at probe time*/
	.ops			= &stb710x_fdma_ops,
	.flags			= DMAC_CHANNELS_TEI_CAPABLE,
};

static int __init stb710x_platform_fdma_probe(struct device *dev)
{
	fdma_platform_device_data * plat_data;
	chip.platform_dev = to_platform_device(dev);

	if (!chip.platform_dev->name){
		fdma_log("FDMA probe failed. Check your kernel SoC config\n");
		return -EINVAL;
	}
	plat_data = chip.platform_dev->dev.platform_data;
	if((plat_data->cpu_subtype ==7109)&& (plat_data->cpu_rev==1)){
		fdma_log("%s Unsupportable CPU revision STB_%d C%d\n",
		__FUNCTION__,(int)chip.cpu_subtype,(int)chip.cpu_rev);
		return -EINVAL;
	}
	else return 0;
}

static struct device_driver fdma_driver = {
	.name  = "710x_FDMA",
	.owner = THIS_MODULE,
	.bus   = &platform_bus_type,
	.probe = stb710x_platform_fdma_probe,
};

static int fdma_do_platform_device_setup(void)
{
	fdma_platform_device_data * plat_data={0};
	unsigned long req_tbl_sz=0;

	sprintf(chip.dev.bus_id,"fdma_710x");
	chip.dev.parent   = &platform_bus ;
	chip.dev.bus      = &platform_bus_type;
	chip.dev.driver   = &fdma_driver;

	if(device_register(&chip.dev)){
		fdma_log("%s Error on FDMA device registration\n",__FUNCTION__);
		return -EINVAL;
	}
	if(!chip.platform_dev) {
		fdma_log("%s No FDMA device available\n",__FUNCTION__);
		return -ENODEV;
	}

	plat_data  = chip.platform_dev->dev.platform_data;
	chip.io_base = (u32)ioremap_nocache(plat_data->fdma_base,
					    plat_data->fdma_base +0x100);
	chip.irq_val = plat_data->irq_vect;
	chip.num_req_lines = plat_data->nr_reqlines;
	chip.ch_min = plat_data->min_ch_num;
	chip.ch_max = plat_data->max_ch_num;

	req_tbl_sz = sizeof( fdmareq_RequestConfig_t)* chip.num_req_lines;
	chip.req_tbl =(fdmareq_RequestConfig_t*)kmalloc(req_tbl_sz,GFP_KERNEL);
	memcpy(chip.req_tbl,(u32*)plat_data->req_line_tbl_adr,req_tbl_sz);

	memcpy(&chip.regs,(u32*)plat_data->registers_ptr,sizeof(fdma_regs_t));
	chip.cpu_subtype = plat_data->cpu_subtype;
	chip.cpu_rev= plat_data->cpu_rev;
	chip.fw_name = plat_data->fw_device_name;
	chip.fw = plat_data->fw;
	return 0;
}

static void __exit deinitialise_710x_fdma2(void)
{
	writel(0,chip.io_base + chip.regs.fdma_en);
	kfree(chip.req_tbl);
	iounmap((u32*)chip.io_base);
	device_unregister(&chip.dev);
	unregister_dmac(chip.info);
	dma_pool_destroy(chip.llu_pool);
	free_irq(chip.irq_val,(void *)&chip);
}

static int __init initialise_710x_fdma2(void)
{
	int err=0;

	memset(&chip,0,sizeof(struct fdma_chip));
	chip.info = &stb710x_fdma_info;
	driver_register(&fdma_driver);
	spin_lock_init(&chip.channel_lock);
	spin_lock_init(&chip.fdma_lock);
	init_waitqueue_head(&chip.fw_load_q);

	if(fdma_do_platform_device_setup()!=0)
		return -ENOSYS;

	/*must take accoutn of CH 0*/
	chip.info->nr_channels = (chip.ch_max+1) -chip.ch_min;
	chip.info->first_channel_nr = chip.ch_min;

	err =request_irq(chip.irq_val,fdma_irq,
			IRQF_DISABLED | IRQF_SHARED,
			"STB710x FDMA",
			(void*)&chip );
	if(err <0)
		panic(" Cant Register irq %d for FDMA engine err %d\n",
					chip.irq_val,err);

	register_dmac(chip.info);
	fdma_register_caps();
	fdma_check_firmware_state();
	return 0;
}
module_init(initialise_710x_fdma2)
module_exit(deinitialise_710x_fdma2)
