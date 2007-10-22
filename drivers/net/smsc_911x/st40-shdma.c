#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <linux/stm/stm-dma.h>

static struct stm_dma_params rx_transfer;
static struct stm_dma_params rx_transfer_sg;
static struct stm_dma_params tx_transfer;

BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh)
{
	if ((dwDmaCh >= 0) && (dwDmaCh < TRANSFER_PIO))
		return TRUE;
	return FALSE;
}

DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData)
{
	int chan;
	const char * dmac_id =STM_DMAC_ID;
	const char * lb_cap_channel = STM_DMA_CAP_LOW_BW;
	chan = request_dma_bycap(&dmac_id,&lb_cap_channel, "smsc911x");
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
	dma_free_descriptor(&rx_transfer_sg);
	dma_free_descriptor(&tx_transfer);
}

static void err_cb(void* x)
{
	printk("DMA err callback");
}


BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh)
{

	declare_dma_parms(  	&rx_transfer,
				MODE_FREERUNNING,
			       	STM_DMA_LIST_OPEN,
			       	STM_DMA_SETUP_CONTEXT_ISR,
			       	STM_DMA_NOBLOCK_MODE,
			       	(char*)STM_DMAC_ID);

	declare_dma_parms(  	&rx_transfer_sg,
				MODE_DST_SCATTER,
			       	STM_DMA_LIST_OPEN,
			       	STM_DMA_SETUP_CONTEXT_ISR,
			       	STM_DMA_NOBLOCK_MODE,
			       	(char*)STM_DMAC_ID);

	declare_dma_parms(  	&tx_transfer,
				MODE_FREERUNNING,
			       	STM_DMA_LIST_OPEN,
			       	STM_DMA_SETUP_CONTEXT_ISR,
			       	STM_DMA_NOBLOCK_MODE,
			       	(char*)STM_DMAC_ID);

	/* From LAN to memory */
	dma_parms_err_cb(&rx_transfer, err_cb, NULL, 0);
	dma_parms_DIM_2_x_1(&rx_transfer,0x20,0);

	dma_parms_err_cb(&rx_transfer_sg, err_cb, NULL, 0);
	dma_parms_DIM_1_x_1(&rx_transfer_sg,0);

	/* From memory to LAN */
	dma_parms_err_cb(&tx_transfer, err_cb, NULL, 0);
	dma_parms_DIM_1_x_2(&tx_transfer,0x20,0);


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
