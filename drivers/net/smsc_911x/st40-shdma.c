#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <linux/stm/stm-dma.h>
#include <linux/stm/710x_fdma.h>

static struct stm_dma_params rx_transfer;
static struct stm_dma_params tx_transfer;

static void err_cb(void* x);
static DWORD smsc911x_request_dma(const char* chan);

#if defined (CONFIG_SMSC911x_DMA_PACED)
/* Ideally whould be: pDmaXfer->dwDwCnt*2
 * Next best would be: MAX_RX_SKBS*2
 * but for now use: */
#define MAX_NODELIST_LEN 20
static struct stm_dma_params rx_transfer_paced[MAX_NODELIST_LEN];

#define SYSCONF_DEVID 0xb9001000

#define SMSC_SHORT_PTK_CHAN 1
#define SMSC_LONG_PTK_CHAN 0


/*we wont know until runtime which req #  to use (platform - dep)*/
struct fmdareq_RequestConfig_s new_rqs[2] = {
/*10*/{0, READ,  OPCODE_32,4,DISABLE_FLG,0,1 },  /* SSC 1 txbuff empty */
/*11*/{0, READ,  OPCODE_32,1,DISABLE_FLG,0,1 }  /* SSC 2 txbuff empty */
};

DWORD Platform_RequestDmaChannelSg(
	PPLATFORM_DATA platformData)
{
	return smsc911x_request_dma(STM_DMA_CAP_ETH_BUF);
}

static void Platform_ReleaseDmaChannel_sg(void)
{
	int i;

	for(i=0;i<MAX_NODELIST_LEN;i++)
		dma_free_descriptor(&rx_transfer_paced[i]);
}

static void Platform_DmaInitialize_sg(void)
{
	int i;
	int devid = ctrl_inl(SYSCONF_DEVID);
	int chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);

	SMSC_TRACE("DMA Rx using paced transfers to main register bank");

	for(i=0;i<MAX_NODELIST_LEN;i++){
		declare_dma_parms(&rx_transfer_paced[i],
				  MODE_PACED,
				  STM_DMA_LIST_OPEN,
				  STM_DMA_SETUP_CONTEXT_ISR,
				  STM_DMA_NOBLOCK_MODE,
				  (char*)STM_DMAC_ID);
		dma_parms_err_cb(&rx_transfer_paced[i], err_cb, NULL, 0);
	}

	if(chip_7109){
		new_rqs[SMSC_LONG_PTK_CHAN].Index = STB7109_FDMA_REQ_SSC_1_TX;
		new_rqs[SMSC_SHORT_PTK_CHAN].Index = STB7109_FDMA_REQ_SSC_2_TX;
	}
	else {
		new_rqs[SMSC_LONG_PTK_CHAN].Index = STB7100_FDMA_REQ_SSC_1_TX;
		new_rqs[SMSC_SHORT_PTK_CHAN].Index = STB7100_FDMA_REQ_SSC_2_TX;
	}

	dma_manual_stbus_pacing(&rx_transfer_paced[0],&new_rqs[0]);
	dma_manual_stbus_pacing(&rx_transfer_paced[0],&new_rqs[1]);

}
#else
static struct stm_dma_params rx_transfer_sg;

DWORD Platform_RequestDmaChannelSg(
	PPLATFORM_DATA platformData)
{
	return smsc911x_request_dma(STM_DMA_CAP_LOW_BW);
}

static void Platform_ReleaseDmaChannel_sg(void)
{
	dma_free_descriptor(&rx_transfer_sg);
}

static void Platform_DmaInitialize_sg(void)
{
	SMSC_TRACE("DMA Rx using freefrunning transfers and FIFOSEL");

	declare_dma_parms(&rx_transfer_sg,
			  MODE_DST_SCATTER,
			  STM_DMA_LIST_OPEN,
			  STM_DMA_SETUP_CONTEXT_ISR,
			  STM_DMA_NOBLOCK_MODE,
			  (char*)STM_DMAC_ID);
	dma_parms_err_cb(&rx_transfer_sg, err_cb, NULL, 0);
	dma_parms_DIM_1_x_1(&rx_transfer_sg,0);
}
#endif

BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh)
{
	if ((dwDmaCh >= 0) && (dwDmaCh < TRANSFER_PIO))
		return TRUE;
	return FALSE;
}

DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData)
{
	return smsc911x_request_dma(STM_DMA_CAP_LOW_BW);
}

static DWORD smsc911x_request_dma(const char* cap)
{
	int chan;
	const char * dmac_id[] = { STM_DMAC_ID, NULL };
	const char * cap_channel[] = { cap, NULL };

	chan = request_dma_bycap(dmac_id, cap_channel, "smsc911x");
	if (chan < 0)
		return TRANSFER_PIO;
	return chan;
}

void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel)
{
	free_dma(dwDmaChannel);
	dma_free_descriptor(&rx_transfer);
	dma_free_descriptor(&tx_transfer);
	Platform_ReleaseDmaChannel_sg();
}

static void err_cb(void* x)
{
	printk("DMA err callback");
}


BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh)
{
	/* From LAN to memory */
	declare_dma_parms(  	&rx_transfer,
				MODE_FREERUNNING,
			       	STM_DMA_LIST_OPEN,
			       	STM_DMA_SETUP_CONTEXT_ISR,
			       	STM_DMA_NOBLOCK_MODE,
			       	(char*)STM_DMAC_ID);
	dma_parms_err_cb(&rx_transfer, err_cb, NULL, 0);
	dma_parms_DIM_2_x_1(&rx_transfer,0x20,0);

	/* From memory to LAN */
	declare_dma_parms(  	&tx_transfer,
				MODE_FREERUNNING,
			       	STM_DMA_LIST_OPEN,
			       	STM_DMA_SETUP_CONTEXT_ISR,
			       	STM_DMA_NOBLOCK_MODE,
			       	(char*)STM_DMAC_ID);
	dma_parms_err_cb(&tx_transfer, err_cb, NULL, 0);
	dma_parms_DIM_1_x_2(&tx_transfer,0x20,0);

	Platform_DmaInitialize_sg();

	return TRUE;
}


BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	DWORD dwAlignMask;
	DWORD dwLanPhysAddr, dwMemPhysAddr;
	stm_dma_params *dmap;
	unsigned long src, dst;
	unsigned long res=0;
	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// 2. make sure the channel's not already running
	if (dma_get_status(pDmaXfer->dwDmaCh) != DMA_CHANNEL_STATUS_IDLE)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	// 3. calculate the physical transfer addresses
	dwLanPhysAddr = CpuToPhysicalAddr((void *)pDmaXfer->dwLanReg);
	dwMemPhysAddr = 0x1fffffffUL & CpuToPhysicalAddr((void *)pDmaXfer->pdwBuf);

	// 4. validate the address alignments
	// need CL alignment for CL bursts
	dwAlignMask = (PLATFORM_CACHE_LINE_BYTES - 1UL);

	if ((dwLanPhysAddr & dwAlignMask) != 0UL)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- bad dwLanPhysAddr (0x%08lX) alignment", dwLanPhysAddr);
		return FALSE;
	}

	if ((dwMemPhysAddr & dwAlignMask) != 0UL)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- bad dwMemPhysAddr (0x%08lX) alignment", dwMemPhysAddr);
		return FALSE;
	}

	// 5. Prepare the DMA channel structure
	if (pDmaXfer->fMemWr) {
		src = PHYSADDR(dwLanPhysAddr);
		dst = PHYSADDR(dwMemPhysAddr);
		dmap = &rx_transfer;
	} else {

		src = PHYSADDR(dwMemPhysAddr);
		dst = PHYSADDR(dwLanPhysAddr);
		dmap = &tx_transfer;
	}

	dma_parms_comp_cb(dmap, pCallback, pCallbackData, 0);
	dma_parms_addrs(dmap,src,dst, pDmaXfer->dwDwCnt << 2);
	res=dma_compile_list(dmap);
	if(res != 0)
		goto err_exit;
	// 6. Start the transfer
	res=dma_xfer_list(pDmaXfer->dwDmaCh,dmap);
	if(res != 0)
		goto err_exit;

	// DMA Transfering....
	return TRUE;
err_exit:
	SMSC_WARNING("%s cant initialise DMA engine err_code %ld\n",__FUNCTION__, res);
	return FALSE;
}

#if defined (CONFIG_SMSC911x_DMA_PACED)

BOOLEAN Platform_DmaStartSgXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	DWORD dwLanPhysAddr;
	int res=0;
	int sg_count;
	struct scatterlist *sg;

	struct stm_dma_params *param;

	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// Validate this is a LAN to memory transfer
	SMSC_ASSERT(pDmaXfer->fMemWr)

	// 2. make sure the channel's not already running
	if (dma_get_status(pDmaXfer->dwDmaCh) != DMA_CHANNEL_STATUS_IDLE)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	// 3. calculate the physical transfer addresses
	dwLanPhysAddr = 0x1fffffffUL & (CpuToPhysicalAddr((void *)pDmaXfer->dwLanReg));

	// 4. Map (flush) the buffer
	sg = (struct scatterlist*)pDmaXfer->pdwBuf;
	sg_count = dma_map_sg(NULL, sg,
			      pDmaXfer->dwDwCnt,
			      pDmaXfer->fMemWr ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	// 5. Prepare the DMA channel structure
	param = rx_transfer_paced;
	for ( ; sg_count; sg_count--) {
		int long_len = sg_dma_len(sg) & (~127);
		int short_len = sg_dma_len(sg) & 127;

		if (long_len) {
			dma_parms_addrs(param,
					dwLanPhysAddr,
					sg_dma_address(sg),
					long_len);
			dma_parms_paced(param,
					long_len,
					new_rqs[SMSC_LONG_PTK_CHAN].Index);
			dma_link_nodes(param, param+1);
			param++;
		}

		if (short_len) {
			dma_parms_addrs(param,
					dwLanPhysAddr,
					sg_dma_address(sg) + long_len,
					short_len);
			dma_parms_paced(param,
					short_len,
					new_rqs[SMSC_SHORT_PTK_CHAN].Index);
			dma_link_nodes(param, param+1);
			param++;
		}

		sg++;
	}

	param--;
	dma_link_nodes(param, NULL);

	dma_parms_comp_cb(param, pCallback, pCallbackData, 0);
	res=dma_compile_list(rx_transfer_paced);
	if(res != 0)
		goto err_exit;
	// 6. Start the transfer
	dma_xfer_list(pDmaXfer->dwDmaCh,rx_transfer_paced);

	// DMA Transfering....
	return TRUE;
err_exit:
	SMSC_WARNING("%s cant initialise DMA engine err_code %d\n",__FUNCTION__,(int)res);
	return FALSE;
}

#else

BOOLEAN Platform_DmaStartSgXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	DWORD dwLanPhysAddr;
	int res;
	int sg_count;

	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// Validate this is a LAN to memory transfer
	SMSC_ASSERT(pDmaXfer->fMemWr)

	// 2. make sure the channel's not already running
	if (dma_get_status(pDmaXfer->dwDmaCh) != DMA_CHANNEL_STATUS_IDLE)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	// 3. calculate the physical transfer addresses
	dwLanPhysAddr = 0x1fffffffUL & (CpuToPhysicalAddr((void *)pDmaXfer->dwLanReg) + (1<<16));

	// 4. Map (flush) the buffer
	sg_count = dma_map_sg(NULL, (struct scatterlist*)pDmaXfer->pdwBuf,
			      pDmaXfer->dwDwCnt,
			      pDmaXfer->fMemWr ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	// 5. Prepare the DMA channel structure
	dma_parms_comp_cb(&rx_transfer_sg, pCallback, pCallbackData, 0);
	dma_parms_addrs(&rx_transfer_sg, dwLanPhysAddr, 0, 0);
	dma_parms_sg(&rx_transfer_sg, (struct scatterlist*)pDmaXfer->pdwBuf, sg_count);
	res=dma_compile_list(&rx_transfer_sg);
	if(res != 0)
		goto err_exit;

	// 6. Start the transfer
	dma_xfer_list(pDmaXfer->dwDmaCh, &rx_transfer_sg);

	// DMA Transfering....
	return TRUE;
err_exit:
	SMSC_WARNING("%s cant initialise DMA engine err_code %d\n",__FUNCTION__,(int)res);
	return FALSE;
}

#endif
