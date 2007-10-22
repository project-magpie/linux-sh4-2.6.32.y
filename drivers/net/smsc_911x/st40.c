/***************************************************************************
 *
 * Copyright (C) 2004-2005  SMSC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***************************************************************************
 * File: st40.c
 *
 * 03/18/2005 Phong Le, rev 1
 * Make this driver to work with Lan911x driver Version 1.13
 *
 * 03/22/2005 Bryan Whitehead, rev 2
 * Added support for 16/32 bit autodetect
 *
 * 04/11/2005 Bryan Whitehead, rev 3
 *    updated platform code to support version 1.14 platform changes
 *
 * 05/02/2005 Phong Le, rev 4
 * Make this driver to work with Lan911x driver Version 1.15
 *
 ***************************************************************************
 * NOTE: When making changes to platform specific code please remember to
 *   update the revision number in the PLATFORM_NAME macro. This is a
 *   platform specific version number and independent of the
 *   common code version number. The PLATFORM_NAME macro should be found in
 *   your platforms ".h" file.
 */

#ifndef ST40_H
#define ST40_H

//for a description of these MACROs see readme.txt

#define PLATFORM_IRQ_POL	(0UL)
#define PLATFORM_IRQ_TYPE	(0UL)

#define DB641_USE_PORT0
#if defined(CONFIG_SH_STI5528_EVAL)
/* db641 STEM card plugged into mb376 */
#ifdef DB641_USE_PORT0
/* STEM CS0 = BANK3 */
#define PLATFORM_CSBASE		(0xA3000000UL)
#define PLATFORM_IRQ		(10UL)
#else
/* STEM CS1 = BANK4 */
#define PLATFORM_CSBASE		(0xA3400000UL)
#define PLATFORM_IRQ		(9UL)
#endif
#elif defined(CONFIG_SH_ST_MB411)
/* db641 STEM card plugged into mb376 */
#ifdef DB641_USE_PORT0
/* STEM CS0 = BankB, A23=0 */
#define PLATFORM_CSBASE		(0xA1000000UL)
#define PLATFORM_IRQ		(12UL)
#else
/* STEM CS1 = BankB, A23=1 */
#define PLATFORM_CSBASE		(0xA1800000UL)
#define PLATFORM_IRQ		(11UL)
#endif
#else
#error Unknown board
#endif

#define PLATFORM_CACHE_LINE_BYTES (32UL)
#define PLATFORM_RX_DMA	(TRANSFER_PIO)
#define PLATFORM_TX_DMA	(TRANSFER_PIO)
#define PLATFORM_NAME		"ST40 STMICRO r3"

//the dma threshold has not been thoroughly tuned but it is
//  slightly better than using zero
#define PLATFORM_DMA_THRESHOLD (200)

typedef struct _PLATFORM_DATA {
	DWORD dwBitWidth;
	DWORD dwIdRev;
	DWORD dwIrq;
	void * dev_id;
} PLATFORM_DATA, *PPLATFORM_DATA;

inline void Platform_SetRegDW(
		DWORD dwLanBase,
		DWORD dwOffset,
		DWORD dwVal)
{
	(*(volatile DWORD *)(dwLanBase+dwOffset))=dwVal;
}

inline DWORD Platform_GetRegDW(
	DWORD dwLanBase,
	DWORD dwOffset)
{
	return (*(volatile DWORD *)(dwLanBase+dwOffset));
}

//See readme.txt for a description of how these
//functions must be implemented
DWORD Platform_Initialize(
	PPLATFORM_DATA platformData,
	DWORD dwLanBase,
	DWORD dwBusWidth);
void Platform_CleanUp(
	PPLATFORM_DATA platformData);
BOOLEAN Platform_Is16BitMode(
	PPLATFORM_DATA platformData);
BOOLEAN Platform_RequestIRQ(
	PPLATFORM_DATA platformData,
	DWORD dwIrq,
	irqreturn_t (*pIsr)(int irq,void *dev_id),
	void *dev_id);
DWORD Platform_CurrentIRQ(
	PPLATFORM_DATA platformData);
void Platform_FreeIRQ(
	PPLATFORM_DATA platformData);
BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh);
BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh);
BOOLEAN Platform_DmaDisable(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);
void Platform_CacheInvalidate(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes);
void Platform_CachePurge(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes);
DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData);
void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel);
BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer);
DWORD Platform_DmaGetDwCnt(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);
void Platform_DmaComplete(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);
void Platform_GetFlowControlParameters(
	PPLATFORM_DATA platformData,
	PFLOW_CONTROL_PARAMETERS flowControlParameters,
	BOOLEAN useDma);
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount);
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount);


#endif

static const char date_code[] = "072605";

/*********/
/* GPDMA */
/*********/
#define DMA_BASE_ADDR		0xB9161000UL
#define DMA_CHAN_BASE_ADDR(n)	(DMA_BASE_ADDR + ((n + 1) * 0x100UL))
#define DMA_VCR_STATUS		(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x00UL))
#define DMA_VCR_VERSION		(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x08UL))
#define DMA_GLOBAL_ENABLE	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x10UL))
#define DMA_GLOBAL_DISABLE	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x18UL))
#define DMA_GLOBAL_STATUS	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x20UL))
#define DMA_GLOBAL_INTERRUPT	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x28UL))
#define DMA_GLOBAL_ERROR	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x30UL))
#define DMA_CHAN_ID(n)		(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n)))
#define DMA_CHAN_ENABLE(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x08UL))
#define DMA_CHAN_DISABLE(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x10UL))
#define DMA_CHAN_STATUS(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x18UL))
#define DMA_CHAN_ACTION(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x20UL))
#define DMA_CHAN_POINTER(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x28UL))
#define DMA_CHAN_REQUEST(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x30UL))
#define DMA_CHAN_CONTROL(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x80UL))
#define DMA_CHAN_COUNT(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x88UL))
#define DMA_CHAN_SAR(n)		(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x90UL))
#define DMA_CHAN_DAR(n)		(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x98UL))

/* DMAC bitmasks */
#define DMA_GLOBAL_ENABLE_CHAN_(n)		(0x00000001UL << n)
#define DMA_GLOBAL_DISABLE_CHAN_(n)		(0x00000001UL << n)
#define DMA_CHAN_ENABLE_CHAN_			0x00000001UL
#define DMA_CHAN_DISABLE_ALL_			0x0000003FUL
#define DMA_CHAN_CONTROL_FREE_RUNNING_		0x00000000UL
#define DMA_CHAN_CONTROL_TRIGGER_		0x00000001UL
#define DMA_CHAN_CONTROL_PACED_SOURCE_		0x00000002UL
#define DMA_CHAN_CONTROL_PACED_DESTINATION_	0x00000003UL
#define DMA_CHAN_CONTROL_NO_LINK_LIST_		0x00000000UL
#define DMA_CHAN_CONTROL_FINAL_LINK_ELEM_	0x00000000UL
#define DMA_CHAN_CONTROL_LINK_ELEM_		0x00000080UL
#define DMA_CHAN_CONTROL_SRC_TYPE_CONST_	0x00000000UL
#define DMA_CHAN_CONTROL_SRC_ADDRESSMODE_INC_	0x00010000UL
#define DMA_CHAN_CONTROL_SRC_UNIT_2BYTES_	0x00080000UL
#define DMA_CHAN_CONTROL_SRC_UNIT_4BYTES_	0x00100000UL
#define DMA_CHAN_CONTROL_SRC_UNIT_32BYTES_	0x00280000UL
#define DMA_CHAN_CONTROL_DST_TYPE_CONST_	0x00000000UL
#define DMA_CHAN_CONTROL_DST_ADDRESSMODE_INC_	0x01000000UL
#define DMA_CHAN_CONTROL_DST_UNIT_2BYTES_	0x08000000UL
#define DMA_CHAN_CONTROL_DST_UNIT_4BYTES_	0x10000000UL
#define DMA_CHAN_CONTROL_DST_UNIT_32BYTES_	0x28000000UL
#define DMA_CHAN_ACTION_COMPLETE_ACK_		0x00000002UL
#define DMA_CHAN_STATUS_COMPLETE_		0x00000002UL
#define DMA_CHAN_REQUEST0			0x0
#define DMA_CHAN_REQUEST1			0x1
#define DMA_CHAN_REQUEST2			0x2
#define DMA_CHAN_REQUEST3			0x3

/* SMSC LAN9118 Byte ordering test register offset */
#define BYTE_TEST_OFFSET	(0x64UL)
#define ID_REV_OFFSET		(0x50UL)

#define CpuToPhysicalAddr(cpuAddr) ((DWORD)cpuAddr)

DWORD Platform_Initialize(
	PPLATFORM_DATA platformData,
	DWORD dwLanBase, DWORD dwBusWidth)
{
	DWORD dwIdRev=0;
	SMSC_TRACE("--> Platform_Initialize");
	SMSC_ASSERT(platformData!=NULL);
	platformData->dwBitWidth=0;

	if(dwLanBase==0x0UL) {
		dwLanBase=PLATFORM_CSBASE;
	}

	SMSC_TRACE("Lan Base at 0x%08lX",dwLanBase);

	platformData->dwBitWidth=16;

	dwIdRev=(*(volatile unsigned long *)(dwLanBase+ID_REV_OFFSET));
	platformData->dwIdRev = dwIdRev;

	SMSC_TRACE("<-- Platform_Initialize");
	return dwLanBase;
}

void Platform_CleanUp(
	PPLATFORM_DATA platformData)
{
}

BOOLEAN Platform_Is16BitMode(
	PPLATFORM_DATA platformData)
{
	SMSC_ASSERT(platformData != NULL);
	if (platformData->dwBitWidth == 16) {
		return TRUE;
	}
	return FALSE;
}

BOOLEAN Platform_RequestIRQ(
	PPLATFORM_DATA platformData,
	DWORD dwIrq,
	irqreturn_t (*pIsr)(int,void *),
	void * dev_id)
{
	SMSC_ASSERT(platformData != NULL);
	SMSC_ASSERT(platformData->dev_id == NULL);
	if (request_irq(
		dwIrq,
		pIsr,
		0,
		"SMSC_LAN9118_ISR",
		dev_id) != 0)
	{
		SMSC_WARNING("Unable to use IRQ = %ld", dwIrq);
		return FALSE;
	}
	platformData->dwIrq = dwIrq;
	platformData->dev_id = dev_id;
	return TRUE;
}

DWORD Platform_CurrentIRQ(
	PPLATFORM_DATA platformData)
{
	SMSC_ASSERT(platformData != NULL);
	return platformData->dwIrq;
}

void Platform_FreeIRQ(
	PPLATFORM_DATA platformData)
{
	SMSC_ASSERT(platformData != NULL);
	SMSC_ASSERT(platformData->dev_id != NULL);

	free_irq(platformData->dwIrq, platformData->dev_id);

	platformData->dwIrq = 0;
	platformData->dev_id = NULL;
}

BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh)
{
	/* for sh4/st40 only use channels 1-4, do not use channel 0 */
	if((dwDmaCh >= 1) && (dwDmaCh <= 4)) {
		return TRUE;
	}
	return FALSE;
}

BOOLEAN Platform_DmaDisable(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh)
{
	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh))

	DMA_CHAN_ACTION(dwDmaCh) = DMA_CHAN_ACTION_COMPLETE_ACK_;

	// Disable DMA controller
	DMA_CHAN_DISABLE(dwDmaCh) = DMA_CHAN_DISABLE_ALL_;
	return TRUE;
}

void Platform_CacheInvalidate(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes)
{
	__flush_invalidate_region((void *)pStartAddress, (dwLengthInBytes));
}

void Platform_CachePurge(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes)
{
	__flush_purge_region((void *)pStartAddress, (dwLengthInBytes));
}

DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData)
{
	return TRANSFER_PIO;
}

void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel)
{
	//since Platform_RequestDmaChannel
	//  never returns a dma channel
	//  then this function should never be called
	SMSC_ASSERT(FALSE);
}

BOOLEAN Platform_IsDmaComplete(
	const DWORD dwDmaCh)
{
	// channel is disable
	if ((DMA_CHAN_ENABLE(dwDmaCh) &  DMA_CHAN_ENABLE_CHAN_) == 0UL)
		return TRUE;
	if ((DMA_CHAN_STATUS(dwDmaCh) & DMA_CHAN_STATUS_COMPLETE_) != 0UL) {
		Platform_DmaDisable ((PPLATFORM_DATA) 0, dwDmaCh);
		return TRUE;
	}
	else
		return FALSE;
}

void Platform_DmaComplete(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh)
{
	DWORD dwTimeOut = 1000000;
	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh))

	// channel is disable
	if ((DMA_CHAN_ENABLE(dwDmaCh) &  DMA_CHAN_ENABLE_CHAN_) == 0UL)
		return;

	while((dwTimeOut) && ((DMA_CHAN_STATUS(dwDmaCh) & DMA_CHAN_STATUS_COMPLETE_) == 0UL))
	{
		udelay(1);
		dwTimeOut--;
	}
	Platform_DmaDisable(platformData, dwDmaCh);
	if(dwTimeOut == 0)
	{
		SMSC_WARNING("Platform_DmaComplete: Timed out");
	}
}

DWORD Platform_DmaGetDwCnt(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh)
{
	DWORD dwCount;

	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh));
	if (Platform_IsDmaComplete(dwDmaCh) == TRUE)
		return 0UL;
	else {
		dwCount = DMA_CHAN_COUNT(dwDmaCh);
		if (dwCount)
			return dwCount;
		else
			return 1UL;
	}
}

BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh)
{
	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh))

	DMA_CHAN_DISABLE(dwDmaCh) = DMA_CHAN_DISABLE_ALL_;
	DMA_CHAN_COUNT(dwDmaCh) = 0;
	DMA_GLOBAL_ENABLE = DMA_GLOBAL_ENABLE_CHAN_(dwDmaCh);
	DMA_CHAN_STATUS(dwDmaCh) = 0UL;		/* no request outstanding */
	DMA_CHAN_ACTION(dwDmaCh) = DMA_CHAN_ACTION_COMPLETE_ACK_;
	DMA_CHAN_REQUEST(dwDmaCh) = (dwDmaCh & 3);	/* req# == chan# */

	SMSC_TRACE("Platform_DmaInitialize -- initialising channel %ld", dwDmaCh);
	SMSC_TRACE("Platform_DmaInitialize -- DMA_CHANn_ENABLE=0x%08lX", DMA_CHAN_ENABLE(dwDmaCh));
	SMSC_TRACE("Platform_DmaInitialize -- DMA_CHANn_COUNT=0x%08lX", DMA_CHAN_COUNT(dwDmaCh));
	SMSC_TRACE("Platform_DmaInitialize -- DMA_GLOBAL_ENABLE=0x%08lX", DMA_GLOBAL_ENABLE);
	SMSC_TRACE("Platform_DmaInitialize -- DMA_ERROR=0x%08lX", DMA_GLOBAL_ERROR);

	return TRUE;
}

BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer)
{
	DWORD dwSrcAddr, dwDestAddr;
	DWORD dwAlignMask, dwControlRegister;
	DWORD dwLanPhysAddr, dwMemPhysAddr;

	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// 2. make sure the channel's not already running
	if (DMA_CHAN_COUNT(pDmaXfer->dwDmaCh) != 0UL)
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

	// 6. Config Control reg
	// Disable the selected channel first
	DMA_CHAN_DISABLE(pDmaXfer->dwDmaCh) = DMA_CHAN_DISABLE_ALL_;

	// Select correct ch and set SRC, DST and counter
	dwControlRegister = DMA_CHAN_CONTROL_FREE_RUNNING_ |
		DMA_CHAN_CONTROL_NO_LINK_LIST_ |
		DMA_CHAN_CONTROL_SRC_UNIT_32BYTES_ |
		DMA_CHAN_CONTROL_DST_UNIT_32BYTES_;

	if (pDmaXfer->fMemWr == TRUE)
	{
		dwSrcAddr = dwLanPhysAddr;
		dwDestAddr = dwMemPhysAddr;
		dwControlRegister |= (DMA_CHAN_CONTROL_SRC_TYPE_CONST_ | DMA_CHAN_CONTROL_DST_ADDRESSMODE_INC_);
	}
	else
	{
		dwSrcAddr = dwMemPhysAddr;
		dwDestAddr = dwLanPhysAddr;
		dwControlRegister |= (DMA_CHAN_CONTROL_DST_TYPE_CONST_ | DMA_CHAN_CONTROL_SRC_ADDRESSMODE_INC_);
	}

	// Set Source and destination addresses
	DMA_CHAN_SAR(pDmaXfer->dwDmaCh) = dwSrcAddr;
	DMA_CHAN_DAR(pDmaXfer->dwDmaCh) = dwDestAddr;

	// Set the transmit size in terms of the xfer mode
	DMA_CHAN_CONTROL(pDmaXfer->dwDmaCh) = dwControlRegister;
	DMA_CHAN_COUNT(pDmaXfer->dwDmaCh) = (pDmaXfer->dwDwCnt << 2);

	// Enable DMA controller ch x
	DMA_CHAN_ENABLE(pDmaXfer->dwDmaCh) = DMA_CHAN_ENABLE_CHAN_;

	// DMA Transfering....
	return TRUE;
}

void Platform_GetFlowControlParameters(
	PPLATFORM_DATA platformData,
	PFLOW_CONTROL_PARAMETERS flowControlParameters,
	BOOLEAN useDma)
{
	memset(flowControlParameters,0,sizeof(FLOW_CONTROL_PARAMETERS));
	flowControlParameters->BurstPeriod=100;
	flowControlParameters->IntDeas=0;
	if(useDma) {
		if(Platform_Is16BitMode(platformData)) {
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,16 bit,DMA
				flowControlParameters->MaxThroughput=(0xEAF0CUL);
				flowControlParameters->MaxPacketCount=(0x282UL);
				flowControlParameters->PacketCost=(0xC2UL);
				flowControlParameters->BurstPeriod=(0x66UL);
				flowControlParameters->IntDeas=(40UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,16 bit,DMA
				flowControlParameters->MaxThroughput=0xB3A3CUL;
				flowControlParameters->MaxPacketCount=0x1E6UL;
				flowControlParameters->PacketCost=0xF4UL;
				flowControlParameters->BurstPeriod=0x26UL;
				flowControlParameters->IntDeas=40UL;
				break;
			default:break;//make lint happy
			}
		} else {
			/* st40 DMA now only support 16-bit, not 32-bit */
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,32 bit,DMA
				flowControlParameters->MaxThroughput=(0xC7F82UL);
				flowControlParameters->MaxPacketCount=(0x21DUL);
				flowControlParameters->PacketCost=(0x17UL);
				flowControlParameters->BurstPeriod=(0x1EUL);
				flowControlParameters->IntDeas=(0x17UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,32 bit,DMA
				flowControlParameters->MaxThroughput=0xABE0AUL;
				flowControlParameters->MaxPacketCount=0x1D1UL;
				flowControlParameters->PacketCost=0x00UL;
				flowControlParameters->BurstPeriod=0x30UL;
				flowControlParameters->IntDeas=0x0A;
				break;
			default:break;//make lint happy
			}
		}
	} else {
		if(Platform_Is16BitMode(platformData)) {
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,16 bit,PIO
				flowControlParameters->MaxThroughput=(0xA0C9EUL);
				flowControlParameters->MaxPacketCount=(0x1B3UL);
				flowControlParameters->PacketCost=(0x1C4UL);
				flowControlParameters->BurstPeriod=(0x5CUL);
				flowControlParameters->IntDeas=(60UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,16 bit,PIO
				flowControlParameters->MaxThroughput=(0x76A6AUL);
				flowControlParameters->MaxPacketCount=(0x141UL);
				flowControlParameters->PacketCost=(0x11AUL);
				flowControlParameters->BurstPeriod=(0x77UL);
				flowControlParameters->IntDeas=(70UL);
				break;
			default:break;//make lint happy
			}
		} else {
			/* st40 PIO now only support 16-bit, not 32-bit */
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,32 bit,PIO
				flowControlParameters->MaxThroughput=(0xAE5C8UL);
				flowControlParameters->MaxPacketCount=(0x1D8UL);
				flowControlParameters->PacketCost=(0UL);
				flowControlParameters->BurstPeriod=(0x57UL);
				flowControlParameters->IntDeas=(0x14UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,32 bit,PIO
				flowControlParameters->MaxThroughput=(0x9E338UL);
				flowControlParameters->MaxPacketCount=(0x1ACUL);
				flowControlParameters->PacketCost=(0xD2UL);
				flowControlParameters->BurstPeriod=(0x60UL);
				flowControlParameters->IntDeas=(0x0EUL);
				break;
			default:break;//make lint happy
			}
		}
	}
}

#if 0
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	volatile DWORD *pdwReg;
	pdwReg = (volatile DWORD *)(dwLanBase+TX_DATA_FIFO);
	while(dwDwordCount)
	{
		*pdwReg = *pdwBuf++;
		dwDwordCount--;
	}
}
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	const volatile DWORD * const pdwReg =
		(const volatile DWORD * const)(dwLanBase+RX_DATA_FIFO);

	while (dwDwordCount)
	{
		*pdwBuf++ = *pdwReg;
		dwDwordCount--;
	}
}
#else
#include <asm/io.h>
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	writesl(dwLanBase+TX_DATA_FIFO, pdwBuf, dwDwordCount);
}
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	readsl(dwLanBase+RX_DATA_FIFO, pdwBuf, dwDwordCount);
}
#endif

