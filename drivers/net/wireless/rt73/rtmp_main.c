/***************************************************************************
 * RT2x00 SourceForge Project - http://rt2x00.serialmonkey.com             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   Licensed under the GNU GPL                                            *
 *   Original code supplied under license from RaLink Inc, 2004.           *
 ***************************************************************************/

/***************************************************************************
 *	Module Name:	rtmp_main.c
 *
 *	Abstract: Main initialization routines
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Jan Lee		01-10-2005	modified
 *	idamlaj		04-10-2006	Apply patch by Ace17 (from forum)
 *
 ***************************************************************************/

#include "rt_config.h"


ULONG	RTDebugLevel = RT_DEBUG_OFF;
static ULONG	debug = RT_DEBUG_OFF;
static char *firmName = RT2573_IMAGE_FILE_NAME;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(debug, "i");
#else
module_param(debug, int, 0);
module_param(firmName, charp, S_IRUGO );
#endif

MODULE_PARM_DESC(debug, "Debug mask: n selects filter, 0 for none");
MODULE_PARM_DESC(firmName, "Permit to load a different firmware: (default: rt73.bin) ");

// Following information will be show when you run 'modinfo'
MODULE_AUTHOR("http://rt2x00.serialmonkey.com");
MODULE_DESCRIPTION("Ralink RT73 802.11abg WLAN Driver " DRIVER_VERSION " " DRIVER_RELDATE);

// *** open source release
MODULE_LICENSE("GPL");

/* Kernel thread and vars, which handles packets that are completed. Only
 * packets that have a "complete" function are sent here. This way, the
 * completion is run out of kernel context, and doesn't block the rest of
 * the stack. */
static int mlme_kill;
static int RTUSBCmd_kill;

extern	const struct iw_handler_def rt73_iw_handler_def;


/* module table */
struct usb_device_id    rtusb_usb_id[] = RT73_USB_DEVICES;
INT const               rtusb_usb_id_len = sizeof(rtusb_usb_id) / sizeof(struct usb_device_id);
MODULE_DEVICE_TABLE(usb, rtusb_usb_id);


#ifndef PF_NOFREEZE
#define PF_NOFREEZE  0
#endif


/**************************************************************************/
/**************************************************************************/
//tested for kernel 2.4 series
/**************************************************************************/
/**************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)


static void usb_rtusb_disconnect(struct usb_device *dev, void *ptr);
static void *usb_rtusb_probe(struct usb_device *dev, UINT interface,
				const struct usb_device_id *id_table);

struct usb_driver rtusb_driver = {
		name:"rt73",
		probe:usb_rtusb_probe,
		disconnect:usb_rtusb_disconnect,
		id_table:rtusb_usb_id,
	};
#else
/**************************************************************************/
/**************************************************************************/
//tested for kernel 2.6series
/**************************************************************************/
/**************************************************************************/
static int usb_rtusb_probe (struct usb_interface *intf,
					  const struct usb_device_id *id);

static void usb_rtusb_disconnect(struct usb_interface *intf);

struct usb_driver rtusb_driver = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
	.owner = THIS_MODULE,
#endif
	.name="rt73",
	.probe=usb_rtusb_probe,
	.disconnect=usb_rtusb_disconnect,
	.id_table=rtusb_usb_id,
	};


#endif


struct net_device_stats *rt73_get_ether_stats(
    IN  struct net_device *net_dev)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) net_dev->priv;

	DBGPRINT(RT_DEBUG_INFO, "rt73_get_ether_stats --->\n");

	pAd->stats.rx_packets = pAd->WlanCounters.ReceivedFragmentCount.vv.LowPart;        // total packets received
	pAd->stats.tx_packets = pAd->WlanCounters.TransmittedFragmentCount.vv.LowPart;     // total packets transmitted

	pAd->stats.rx_bytes= pAd->RalinkCounters.ReceivedByteCount;             // total bytes received
	pAd->stats.tx_bytes = pAd->RalinkCounters.TransmittedByteCount;         // total bytes transmitted

	pAd->stats.rx_errors = pAd->Counters8023.RxErrors;                      // bad packets received
	pAd->stats.tx_errors = pAd->Counters8023.TxErrors;                      // packet transmit problems

	pAd->stats.rx_dropped = pAd->Counters8023.RxNoBuffer;                   // no space in linux buffers
	pAd->stats.tx_dropped = pAd->WlanCounters.FailedCount.vv.LowPart;                  // no space available in linux

	pAd->stats.multicast = pAd->WlanCounters.MulticastReceivedFrameCount.vv.LowPart;   // multicast packets received
	pAd->stats.collisions = pAd->Counters8023.OneCollision + pAd->Counters8023.MoreCollisions;  // Collision packets

	pAd->stats.rx_length_errors = 0;
	pAd->stats.rx_over_errors = pAd->Counters8023.RxNoBuffer;               // receiver ring buff overflow
	pAd->stats.rx_crc_errors = 0;//pAd->WlanCounters.FCSErrorCount;         // recved pkt with crc error
	pAd->stats.rx_frame_errors = pAd->Counters8023.RcvAlignmentErrors;      // recv'd frame alignment error
	pAd->stats.rx_fifo_errors = pAd->Counters8023.RxNoBuffer;               // recv'r fifo overrun
	pAd->stats.rx_missed_errors = 0;                                        // receiver missed packet

	// detailed tx_errors
	pAd->stats.tx_aborted_errors = 0;
	pAd->stats.tx_carrier_errors = 0;
	pAd->stats.tx_fifo_errors = 0;
	pAd->stats.tx_heartbeat_errors = 0;
	pAd->stats.tx_window_errors = 0;

	// for cslip etc
	pAd->stats.rx_compressed = 0;
	pAd->stats.tx_compressed = 0;

	return &pAd->stats;
}

#if WIRELESS_EXT >= 12
/*
	========================================================================

	Routine Description:
		get wireless statistics

	Arguments:
		net_dev                     Pointer to net_device

	Return Value:
		struct iw_statistics

	Note:
		This function will be called when query /proc

	========================================================================
*/
long rt_abs(long arg)	{	return (arg<0)? -arg : arg;}
struct iw_statistics *rt73_get_wireless_stats(
	IN  struct net_device *net_dev)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) net_dev->priv;

	DBGPRINT(RT_DEBUG_TRACE, "rt73_get_wireless_stats --->\n");

	// TODO: All elements are zero before be implemented

	pAd->iw_stats.status = 0;   // Status - device dependent for now

    pAd->iw_stats.qual.qual = pAd->Mlme.ChannelQuality; // link quality (%retries, SNR, %missed beacons or better...)
#ifdef RTMP_EMBEDDED
    pAd->iw_stats.qual.level = rt_abs(pAd->PortCfg.LastRssi);   // signal level (dBm)
#else
    pAd->iw_stats.qual.level = abs(pAd->PortCfg.LastRssi);      // signal level (dBm)
#endif
	pAd->iw_stats.qual.level += 256 - pAd->BbpRssiToDbmDelta;

    pAd->iw_stats.qual.noise = (pAd->BbpWriteLatch[17] > pAd->BbpTuning.R17UpperBoundG) ? pAd->BbpTuning.R17UpperBoundG : ((ULONG) pAd->BbpWriteLatch[17]); // noise level (dBm)
    pAd->iw_stats.qual.noise += 256 - 143;
	pAd->iw_stats.qual.updated = 1;     // Flags to know if updated

	pAd->iw_stats.discard.nwid = 0;     // Rx : Wrong nwid/essid
	pAd->iw_stats.miss.beacon = 0;      // Missed beacons/superframe

	// pAd->iw_stats.discard.code, discard.fragment, discard.retries, discard.misc has counted in other place

	return &pAd->iw_stats;
}
#endif

VOID RTUSBHalt(
	IN	PRTMP_ADAPTER	pAd,
	IN  BOOLEAN         IsFree)
{
	MLME_DISASSOC_REQ_STRUCT DisReq;
	MLME_QUEUE_ELEM          MsgElem;
	INT                      i;

	DBGPRINT(RT_DEBUG_TRACE, "====> RTUSBHalt\n");

	//
	// before set flag fRTMP_ADAPTER_HALT_IN_PROGRESS,
	// we should send a disassoc frame to our AP.
	//
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		if (INFRA_ON(pAd))
		{
			COPY_MAC_ADDR(DisReq.Addr, pAd->PortCfg.Bssid);
			DisReq.Reason =  REASON_DISASSOC_STA_LEAVING;

			MsgElem.Machine = ASSOC_STATE_MACHINE;
			MsgElem.MsgType = MT2_MLME_DISASSOC_REQ;
			MsgElem.MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
			memcpy(MsgElem.Msg, &DisReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			MlmeDisassocReqAction(pAd, &MsgElem);
			RTMPusecDelay(1000);
		}

        //
		// Patch to fully turn off BBP, need to send a fake NULL frame.
		//
		RTUSBWriteMACRegister(pAd, MAC_CSR10, 0x0018);
		for (i=0; i<10; i++)
		{
			RTMPSendNullFrame(pAd, RATE_6);
			RTMPusecDelay(1000);
		}

		// disable BEACON generation and other BEACON related hardware timers
		AsicDisableSync(pAd);
		RTMPSetLED(pAd, LED_HALT);

	}

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);

	RTUSBCleanUpMLMEWaitQueue(pAd);
	RTUSBCleanUpMLMEBulkOutQueue(pAd);

    RTMPCancelTimer(&pAd->PortCfg.QuickResponeForRateUpTimer);
	RTMPCancelTimer(&pAd->RxAnt.RxAntDiversityTimer);

	// Free MLME stuff
	MlmeHalt(pAd);

    // Sleep 50 milliseconds so pending io might finish normally
	RTMPusecDelay(50000);

	// We want to wait until all pending receives and sends to the
	// device object. We cancel any
	// irps. Wait until sends and receives have stopped.
	//
	RTUSBCancelPendingIRPs(pAd);

    // Free the entire adapter object
	ReleaseAdapter(pAd, IsFree, FALSE);

	// reset mlme & command thread
    pAd->MLMEThr_pid = -1;
	pAd->RTUSBCmdThr_pid = -1;

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
}

VOID CMDHandler(
    IN PRTMP_ADAPTER pAd)
{
	PCmdQElmt	cmdqelmt;
	PUCHAR	    pData;
	NDIS_STATUS	NdisStatus = NDIS_STATUS_SUCCESS;
  unsigned long IrqFlags;
    ULONG       Now;

	while (pAd->CmdQ.size > 0)
	{

		NdisStatus = NDIS_STATUS_SUCCESS;
		NdisAcquireSpinLock(&pAd->CmdQLock, IrqFlags);
		RTUSBDequeueCmd(&pAd->CmdQ, &cmdqelmt);
		NdisReleaseSpinLock(&pAd->CmdQLock, IrqFlags);
		if (cmdqelmt == NULL)
			break;
		pData = cmdqelmt->buffer;

        //DBGPRINT_RAW(RT_DEBUG_INFO, "Cmd = %x\n", cmdqelmt->command);
		switch (cmdqelmt->command)
		{
			case RT_OID_CHECK_GPIO:
			{
				ULONG data;
				// Read GPIO pin7 as Hardware controlled radio state
				RTUSBReadMACRegister(pAd, MAC_CSR13, &data);
				if (data & 0x80)
				{
					pAd->PortCfg.bHwRadio = TRUE;
				}
				else
				{
					pAd->PortCfg.bHwRadio = FALSE;
				}
				if (pAd->PortCfg.bRadio != (pAd->PortCfg.bHwRadio && pAd->PortCfg.bSwRadio))
				{
					pAd->PortCfg.bRadio = (pAd->PortCfg.bHwRadio && pAd->PortCfg.bSwRadio);
					if (pAd->PortCfg.bRadio == TRUE)
					{
						MlmeRadioOn(pAd);
						// Update extra information
						pAd->ExtraInfo = EXTRA_INFO_CLEAR;
					}
					else
					{
						MlmeRadioOff(pAd);
						// Update extra information
						pAd->ExtraInfo = HW_RADIO_OFF;
					}
				}
			}
			break;

			case RT_OID_PERIODIC_EXECUT:
			    STAMlmePeriodicExec(pAd);
			break;

			case OID_802_11_BSSID_LIST_SCAN:
			{
				if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
				{
					MlmeEnqueue(pAd,
					            MLME_CNTL_STATE_MACHINE,
					            RT_CMD_RESET_MLME,
					            0,
					            NULL);

				}

				Now = jiffies;
				pAd->MlmeAux.CurrReqIsFromNdis = FALSE;
				// Reset Missed scan number
				pAd->PortCfg.ScanCnt = 0;
				pAd->PortCfg.LastScanTime = Now;
				MlmeEnqueue(pAd,
							MLME_CNTL_STATE_MACHINE,
							OID_802_11_BSSID_LIST_SCAN,
							0,
							NULL);
				RTUSBMlmeUp(pAd);
			}
			break;

			case RT_OID_802_11_BSSID:
			{

				if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
				{
					MlmeEnqueue(pAd,
					            MLME_CNTL_STATE_MACHINE,
					            RT_CMD_RESET_MLME,
					            0,
					            NULL);

				}

				pAd->MlmeAux.CurrReqIsFromNdis = FALSE;

				// Reset allowed scan retries
				pAd->PortCfg.ScanCnt = 0;

				MlmeEnqueue(pAd,
							MLME_CNTL_STATE_MACHINE,
							OID_802_11_BSSID,
							cmdqelmt->bufferlength,
							cmdqelmt->buffer);
				RTUSBMlmeUp(pAd);
			}
			break;

			case OID_802_11_SSID:
			{
				if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
				{
					MlmeEnqueue(pAd,
					            MLME_CNTL_STATE_MACHINE,
					            RT_CMD_RESET_MLME,
					            0,
					            NULL);

				}

				pAd->MlmeAux.CurrReqIsFromNdis = FALSE;

				// Reset allowed scan retries
				pAd->PortCfg.ScanCnt = 0;
				pAd->bConfigChanged = TRUE;

				MlmeEnqueue(pAd,
							MLME_CNTL_STATE_MACHINE,
							OID_802_11_SSID,
							cmdqelmt->bufferlength,
							pData);
				RTUSBMlmeUp(pAd);
			}
			break;

			case OID_802_11_DISASSOCIATE:
			{
				if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
				{
					MlmeEnqueue(pAd,
					            MLME_CNTL_STATE_MACHINE,
					            RT_CMD_RESET_MLME,
					            0,
					            NULL);

				}

				// Set to immediately send the media disconnect event
				pAd->MlmeAux.CurrReqIsFromNdis = TRUE;

				MlmeEnqueue(pAd,
							MLME_CNTL_STATE_MACHINE,
							OID_802_11_DISASSOCIATE,
							0,
							NULL);
				RTUSBMlmeUp(pAd);
			}
			break;

			case OID_802_11_RX_ANTENNA_SELECTED:
			{

		        NDIS_802_11_ANTENNA	Antenna = *(NDIS_802_11_ANTENNA *)pData;

				    if (Antenna == 0)
					    pAd->Antenna.field.RxDefaultAntenna = 1;    // ant-A
				    else if(Antenna == 1)
					    pAd->Antenna.field.RxDefaultAntenna = 2;    // ant-B
				    else
					    pAd->Antenna.field.RxDefaultAntenna = 0;    // diversity

			    pAd->PortCfg.BandState = UNKNOWN_BAND;
			    AsicAntennaSelect(pAd, pAd->LatchRfRegs.Channel);
			    DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_RX_ANTENNA_SELECTED (=%d)\n", Antenna);
            }
		    break;

			case OID_802_11_TX_ANTENNA_SELECTED:
		    {
			    NDIS_802_11_ANTENNA	Antenna = *(NDIS_802_11_ANTENNA *)pData;

			    if (Antenna == 0)
				    pAd->Antenna.field.TxDefaultAntenna = 1;    // ant-A
			    else if(Antenna == 1)
				    pAd->Antenna.field.TxDefaultAntenna = 2;    // ant-B
			    else
				    pAd->Antenna.field.TxDefaultAntenna = 0;    // diversity

			    pAd->PortCfg.BandState = UNKNOWN_BAND;
			    AsicAntennaSelect(pAd, pAd->LatchRfRegs.Channel);
			    DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_TX_ANTENNA_SELECTED (=%d)\n", Antenna);
            }
		    break;
#if 0
	        case RT_OID_802_11_QUERY_HARDWARE_REGISTER:
		        NdisStatus = RTUSBQueryHardWareRegister(pAd, pData);
		    break;

		    case RT_OID_802_11_SET_HARDWARE_REGISTER:
		        NdisStatus = RTUSBSetHardWareRegister(pAd, pData);
			break;
#endif
			case RT_OID_MULTI_READ_MAC:
	        {
			    USHORT	Offset = *((PUSHORT)pData);
			    USHORT	Length = *((PUSHORT)(pData + 2));
		        RTUSBMultiRead(pAd, Offset, pData + 4, Length);
		    }
		    break;

			case RT_OID_MULTI_WRITE_MAC:
	        {
		        USHORT	Offset = *((PUSHORT)pData);
			    USHORT	Length = *((PUSHORT)(pData + 2));
			    RTUSBMultiWrite(pAd, Offset, pData + 4, Length);
		    }
		    break;

			case RT_OID_USB_VENDOR_EEPROM_READ:
			{
				USHORT	Offset = *((PUSHORT)pData);
				USHORT	Length = *((PUSHORT)(pData + 2));
				RTUSBReadEEPROM(pAd, Offset, pData + 4, Length);
			}
			break;

			case RT_OID_USB_VENDOR_EEPROM_WRITE:
			{
				USHORT	Offset = *((PUSHORT)pData);
#if 0
				USHORT	Length = *((PUSHORT)(pData + 2));
				RTUSBWriteEEPROM(pAd, Offset, pData + 4, Length);
#else//F/W restricts the max EEPROM write size to 62 bytes.
				USHORT	Residual = *((PUSHORT)(pData + 2));
				pData += 4;
				while (Residual > 62)
				{
				RTUSBWriteEEPROM(pAd, Offset, pData, 62);
				Offset += 62;
				Residual -= 62;
				pData += 62;
				}
				RTUSBWriteEEPROM(pAd, Offset, pData, Residual);
#endif
			}
			break;

			case RT_OID_USB_VENDOR_ENTER_TESTMODE:
			    RTUSB_VendorRequest(pAd,
					0,
					DEVICE_VENDOR_REQUEST_OUT,
					0x1,
					0x4,
					0x1,
					NULL,
					0);
					break;

			case RT_OID_USB_VENDOR_EXIT_TESTMODE:
				RTUSB_VendorRequest(pAd,
					0,
					DEVICE_VENDOR_REQUEST_OUT,
					0x1,
					0x4,
					0x0,
					NULL,
					0);
			break;
			case RT_OID_USB_RESET_BULK_OUT:
			{
				INT 	Index;

		        DBGPRINT_RAW(RT_DEBUG_ERROR, "RT_OID_USB_RESET_BULK_OUT\n");

				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS);

				RTUSBRejectPendingPackets(pAd); //reject all NDIS packets waiting in TX queue
				RTUSBCancelPendingBulkOutIRP(pAd);
				RTUSBCleanUpDataBulkOutQueue(pAd);

				NICInitializeAsic(pAd);
				ReleaseAdapter(pAd, FALSE, TRUE);   // unlink urb releated tx context
				NICInitTransmit(pAd);

				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS);

				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET))
				{
					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				}

				if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
				{
					for (Index = 0; Index < 4; Index++)
					{
						if(!skb_queue_empty(&pAd->SendTxWaitQueue[Index]))
						{
							RTMPDeQueuePacket(pAd, Index);
						}
					}

					RTUSBKickBulkOut(pAd);
				}
			}

    	    break;

			case RT_OID_USB_RESET_BULK_IN:
		    {
			    int	i;
				DBGPRINT_RAW(RT_DEBUG_ERROR, "!!!!!RT_OID_USB_RESET_BULK_IN\n");
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS);
				NICInitializeAsic(pAd);
				//RTUSBWriteMACRegister(pAd, TXRX_CSR0, 0x025eb032); // ??
				for (i = 0; i < RX_RING_SIZE; i++)
				{
					PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

					if (pRxContext->pUrb != NULL)
					{
						RTUSB_UNLINK_URB(pRxContext->pUrb);
						usb_free_urb(pRxContext->pUrb);
						pRxContext->pUrb = NULL;
					}
					if (pRxContext->TransferBuffer != NULL)
					{
						kfree(pRxContext->TransferBuffer);
						pRxContext->TransferBuffer = NULL;
					}
				}
				NICInitRecv(pAd);
				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS);
				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET))
				{
					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET);
				}

				if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
				{
					RTUSBBulkReceive(pAd);
					RTUSBWriteMACRegister(pAd, TXRX_CSR0, 0x0276b032);  // enable RX of MAC block
				}
		    }
			break;

			case RT_OID_802_11_STA_CONFIG:
			{
				RT_802_11_STA_CONFIG *pStaConfig = (RT_802_11_STA_CONFIG *)pData;
				if (pStaConfig->EnableTxBurst != pAd->PortCfg.bEnableTxBurst)
				{
					pAd->PortCfg.bEnableTxBurst = (pStaConfig->EnableTxBurst == 1);
					//Currently Tx burst mode is only implemented in infrastructure mode.
					if (INFRA_ON(pAd))
					{
						if (pAd->PortCfg.bEnableTxBurst)
						{
							//Extend slot time if any encryption method is used to give ASIC more time to do encryption/decryption during Tx burst mode.
							if (pAd->PortCfg.WepStatus != Ndis802_11EncryptionDisabled)
							{
							// Nemo  RT2573USBWriteMACRegister_old(pAd, MAC_CSR10, 0x20);
							}
							//Set CWmin/CWmax to 0.
							// Nemo 2004    RT2573USBWriteMACRegister_old(pAd, MAC_CSR22, 0x100);
						}
						else
						{
							if (pAd->PortCfg.WepStatus != Ndis802_11EncryptionDisabled)
								AsicSetSlotTime(pAd, (BOOLEAN)pAd->PortCfg.UseShortSlotTime);
						// Nemo 2004    RT2573USBWriteMACRegister_old(pAd, MAC_CSR22, 0x53);
						}
					}
				}
				//pAd->PortCfg.EnableTurboRate = pStaConfig->EnableTurboRate;
				pAd->PortCfg.UseBGProtection = pStaConfig->UseBGProtection;
				//pAd->PortCfg.UseShortSlotTime = pStaConfig->UseShortSlotTime;
				pAd->PortCfg.UseShortSlotTime = 1; // 2003-10-30 always SHORT SLOT capable
				if (pAd->PortCfg.AdhocMode != pStaConfig->AdhocMode)
				{
					// allow dynamic change of "USE OFDM rate or not" in ADHOC mode
					// if setting changed, need to reset current TX rate as well as BEACON frame format
					pAd->PortCfg.AdhocMode = pStaConfig->AdhocMode;
					if (pAd->PortCfg.BssType == BSS_ADHOC)
					{
						MlmeUpdateTxRates(pAd, FALSE);
						MakeIbssBeacon(pAd);
						AsicEnableIbssSync(pAd);
					}
				}
				DBGPRINT(RT_DEBUG_TRACE, "CmdThread::RT_OID_802_11_SET_STA_CONFIG (Burst=%d,BGprot=%d,ShortSlot=%d,Adhoc=%d,Protection=%d\n",
					pStaConfig->EnableTxBurst,
					pStaConfig->UseBGProtection,
					pStaConfig->UseShortSlotTime,
					pStaConfig->AdhocMode,
					pAd->PortCfg.UseBGProtection);
			}
		    break;

			case RT_OID_SET_PSM_BIT_SAVE:
				MlmeSetPsmBit(pAd, PWR_SAVE);
				RTMPSendNullFrame(pAd, pAd->PortCfg.TxRate);
		    break;

		    case RT_OID_SET_RADIO:
			    if (pAd->PortCfg.bRadio == TRUE)
                {
				    MlmeRadioOn(pAd);
				    // Update extra information
				    pAd->ExtraInfo = EXTRA_INFO_CLEAR;
			    }
			    else
                {
			        MlmeRadioOff(pAd);
				    // Update extra information
    			    pAd->ExtraInfo = SW_RADIO_OFF;
    		    }
		    break;

			case RT_OID_RESET_FROM_ERROR:
			case RT_OID_RESET_FROM_NDIS:
			{
				UINT	i = 0;

				RTUSBRejectPendingPackets(pAd);//reject all NDIS packets waiting in TX queue
				RTUSBCleanUpDataBulkOutQueue(pAd);
				MlmeSuspend(pAd, FALSE);

				//Add code to access necessary registers here.
				//disable Rx
				RTUSBWriteMACRegister(pAd, TXRX_CSR2, 1);
				//Ask our device to complete any pending bulk in IRP.
				while ((atomic_read(&pAd->PendingRx) > 0) ||
                       (pAd->BulkOutPending[0] == TRUE) ||
					   (pAd->BulkOutPending[1] == TRUE) ||
					   (pAd->BulkOutPending[2] == TRUE) ||
					   (pAd->BulkOutPending[3] == TRUE))

				{
				    if (atomic_read(&pAd->PendingRx) > 0)
					{
						DBGPRINT_RAW(RT_DEBUG_TRACE, "BulkIn IRP Pending!!!\n");
						RTUSB_VendorRequest(pAd,
											0,
											DEVICE_VENDOR_REQUEST_OUT,
											0x0C,
											0x0,
											0x0,
											NULL,
											0);
					}

					if ((pAd->BulkOutPending[0] == TRUE) ||
						(pAd->BulkOutPending[1] == TRUE) ||
						(pAd->BulkOutPending[2] == TRUE) ||
						(pAd->BulkOutPending[3] == TRUE))
					{
						DBGPRINT_RAW(RT_DEBUG_TRACE, "BulkOut IRP Pending!!!\n");
						if (i == 0)
						{
							RTUSBCancelPendingBulkOutIRP(pAd);
							i++;
						}
					}

					RTMPusecDelay(500000);
				}

				NICResetFromError(pAd);
				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HARDWARE_ERROR))
				{
					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_HARDWARE_ERROR);
				}
				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET))
				{
					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET);
				}
				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET))
				{
					RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				}

				RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);

				if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF)) &&
					(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
					(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
				{
					MlmeResume(pAd);
					RTUSBBulkReceive(pAd);
					RTUSBWriteMACRegister(pAd, TXRX_CSR2, 0x7e);
				}
			}
			break;

			case RT_OID_LINK_DOWN:
				DBGPRINT_RAW(RT_DEBUG_TRACE, "LinkDown(RT_OID_LINK_DOWN)\n");
				LinkDown(pAd, TRUE);
			break;

			case RT_OID_VENDOR_WRITE_BBP:
			{
				UCHAR	Offset, Value;
				Offset = *((PUCHAR)pData);
				Value = *((PUCHAR)(pData + 1));
				DBGPRINT_RAW(RT_DEBUG_INFO, "offset = 0x%02x	value = 0x%02x\n", Offset, Value);
				RTUSBWriteBBPRegister(pAd, Offset, Value);
			}
			break;

			case RT_OID_VENDOR_READ_BBP:
			{
				UCHAR	Offset = *((PUCHAR)pData);
				PUCHAR	pValue = (PUCHAR)(pData + 1);

				DBGPRINT_RAW(RT_DEBUG_INFO, "offset = 0x%02x\n", Offset);
				RTUSBReadBBPRegister(pAd, Offset, pValue);
				DBGPRINT_RAW(RT_DEBUG_INFO, "value = 0x%02x\n", *pValue);
			}
			break;

			case RT_OID_VENDOR_WRITE_RF:
			{
				ULONG	Value = *((PULONG)pData);

				DBGPRINT_RAW(RT_DEBUG_INFO, "value = 0x%08x\n", Value);
				RTUSBWriteRFRegister(pAd, Value);
			}
			break;

			case RT_OID_802_11_RESET_COUNTERS:
			{
				UCHAR	Value[22];

				RTUSBMultiRead(pAd, STA_CSR0, Value, 24);
			}
			break;

			case RT_OID_USB_VENDOR_RESET:
				RTUSB_VendorRequest(pAd,
									0,
									DEVICE_VENDOR_REQUEST_OUT,
									1,
									1,
									0,
									NULL,
									0);
			break;

			case RT_OID_USB_VENDOR_UNPLUG:
				RTUSB_VendorRequest(pAd,
									0,
									DEVICE_VENDOR_REQUEST_OUT,
									1,
									2,
									0,
									NULL,
									0);
			break;
#if 0
			case RT_OID_USB_VENDOR_SWITCH_FUNCTION:
				RTUSBWriteMACRegister(pAd, MAC_CSR13, 0x2121);
				RTUSBWriteMACRegister(pAd, MAC_CSR14, 0x1e1e);
				RTUSBWriteMACRegister(pAd, MAC_CSR1, 3);
				RTUSBWriteMACRegister(pAd, PHY_CSR4, 0xf);

				RTUSB_VendorRequest(pAd,
									0,
									DEVICE_VENDOR_REQUEST_OUT,
									1,
									3,
									0,
									NULL,
									0);
			break;
#endif
			case RT_OID_VENDOR_FLIP_IQ:
			{
				ULONG	Value1, Value2;
				RTUSBReadMACRegister(pAd, PHY_CSR5, &Value1);
				RTUSBReadMACRegister(pAd, PHY_CSR6, &Value2);
				if (*pData == 1)
				{
					DBGPRINT_RAW(RT_DEBUG_INFO, "I/Q Flip\n");
					Value1 = Value1 | 0x0004;
					Value2 = Value2 | 0x0004;
				}
				else
				{
					DBGPRINT_RAW(RT_DEBUG_INFO, "I/Q Not Flip\n");
					Value1 = Value1 & 0xFFFB;
					Value2 = Value2 & 0xFFFB;
				}
				RTUSBWriteMACRegister(pAd, PHY_CSR5, Value1);
				RTUSBWriteMACRegister(pAd, PHY_CSR6, Value2);
			}
			break;

			case RT_OID_UPDATE_TX_RATE:
				MlmeUpdateTxRates(pAd, FALSE);
				if (ADHOC_ON(pAd))
					MakeIbssBeacon(pAd);
			break;

			case RT_OID_802_11_PREAMBLE:
			{
				ULONG	Preamble = *((PULONG)(cmdqelmt->buffer));
				if (Preamble == Rt802_11PreambleShort)
				{
					pAd->PortCfg.TxPreamble = Preamble;
					MlmeSetTxPreamble(pAd, Rt802_11PreambleShort);
				}
				else if ((Preamble == Rt802_11PreambleLong) || (Preamble == Rt802_11PreambleAuto))
				{
					// if user wants AUTO, initialize to LONG here, then change according to AP's
					// capability upon association.
					pAd->PortCfg.TxPreamble = Preamble;
					MlmeSetTxPreamble(pAd, Rt802_11PreambleLong);
				}
				else
					NdisStatus = NDIS_STATUS_FAILURE;
				DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::RT_OID_802_11_SET_PREAMBLE (=%d)\n", Preamble);
			}
			break;
			case OID_802_11_NETWORK_TYPE_IN_USE:
			{
				NDIS_802_11_NETWORK_TYPE	NetType = *(PNDIS_802_11_NETWORK_TYPE)(cmdqelmt->buffer);
				if (NetType == Ndis802_11DS)
					RTMPSetPhyMode(pAd, PHY_11B);
				else if (NetType == Ndis802_11OFDM24)
					RTMPSetPhyMode(pAd, PHY_11BG_MIXED);
				else if (NetType == Ndis802_11OFDM5)
					RTMPSetPhyMode(pAd, PHY_11A);
				else
					NdisStatus = NDIS_STATUS_FAILURE;
				DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_NETWORK_TYPE_IN_USE (=%d)\n",NetType);

            }
            break;
			case RT_OID_802_11_PHY_MODE:
				{
					ULONG	phymode = *(ULONG *)(cmdqelmt->buffer);
					RTMPSetPhyMode(pAd, phymode);
					DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::RT_OID_802_11_SET_PHY_MODE (=%d)\n", phymode);
				}
			break;

#if 0
			case OID_802_11_WEP_STATUS:
				{
					USHORT	Value;
					NDIS_802_11_WEP_STATUS	WepStatus = *(PNDIS_802_11_WEP_STATUS)pData;

					DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::66- OID_802_11_WEP_STATUS  \n");
			break;

					if (pAd->PortCfg.WepStatus != WepStatus)
					{
						DBGPRINT(RT_DEBUG_ERROR, "Config Changed !!status= %x  \n", WepStatus);

						// Config has changed
						pAd->bConfigChanged = TRUE;
					}
					pAd->PortCfg.WepStatus   = WepStatus;
					pAd->PortCfg.PairCipher  = WepStatus;
				    pAd->PortCfg.GroupCipher = WepStatus;

#if 1
					if ((WepStatus == Ndis802_11Encryption1Enabled) &&
						(pAd->SharedKey[pAd->PortCfg.DefaultKeyId].KeyLen != 0))
					{
						if (pAd->SharedKey[pAd->PortCfg.DefaultKeyId].KeyLen <= 5)
						{
							DBGPRINT(RT_DEBUG_ERROR, "WEP64!  \n");

							pAd->SharedKey[pAd->PortCfg.DefaultKeyId].CipherAlg = CIPHER_WEP64;
						}
						else
						{
							DBGPRINT(RT_DEBUG_ERROR, "WEP128!  \n");

							pAd->SharedKey[pAd->PortCfg.DefaultKeyId].CipherAlg = CIPHER_WEP128;
						}
#if 0
						RTUSBReadMACRegister_old(pAd, TXRX_CSR0, &Value);
						Value &= 0xfe00;
						Value |= ((LENGTH_802_11 << 3) | (pAd->PortCfg.CipherAlg));
						RTUSBWriteMACRegister_old(pAd, TXRX_CSR0, Value);
#endif
					}
					else if (WepStatus == Ndis802_11Encryption2Enabled)
					{
						DBGPRINT(RT_DEBUG_ERROR, " TKIP !!!  \n");

						pAd->SharedKey[pAd->PortCfg.DefaultKeyId].CipherAlg = CIPHER_TKIP;
#if 0
						RTUSBReadMACRegister_old(pAd, TXRX_CSR0, &Value);
						Value &= 0xfe00;
						Value |= ((LENGTH_802_11 << 3) | (pAd->PortCfg.CipherAlg));
						RTUSBWriteMACRegister_old(pAd, TXRX_CSR0, Value);
#endif
					}
					else if (WepStatus == Ndis802_11Encryption3Enabled)
					{
						DBGPRINT(RT_DEBUG_ERROR, " AES  !!!  \n");
						pAd->SharedKey[pAd->PortCfg.DefaultKeyId].CipherAlg = CIPHER_AES;
#if 0
						RTUSBReadMACRegister_old(pAd, TXRX_CSR0, &Value);
						Value &= 0xfe00;
						Value |= ((LENGTH_802_11 << 3) | (pAd->PortCfg.CipherAlg));
						RTUSBWriteMACRegister_old(pAd, TXRX_CSR0, Value);
#endif
					}
					else if (WepStatus == Ndis802_11EncryptionDisabled)
					{
						DBGPRINT(RT_DEBUG_ERROR, " CIPHER_NONE  !!!  \n");

						pAd->SharedKey[pAd->PortCfg.DefaultKeyId].CipherAlg = CIPHER_NONE;
#if 0
						RTUSBReadMACRegister_old(pAd, TXRX_CSR0, &Value);
						Value &= 0xfe00;
						RTUSBWriteMACRegister_old(pAd, TXRX_CSR0, Value);
#endif
					}else
					{
						DBGPRINT(RT_DEBUG_ERROR, " ERROR Cipher   !!!  \n");
					}
#endif
				}
			break;
#endif
			case OID_802_11_ADD_WEP:
			{
				ULONG	KeyIdx;
				PNDIS_802_11_WEP	pWepKey;

				DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_ADD_WEP  \n");

				pWepKey = (PNDIS_802_11_WEP)pData;
				KeyIdx = pWepKey->KeyIndex & 0x0fffffff;

				// it is a shared key
				if ((KeyIdx >= 4) || ((pWepKey->KeyLength != 5) && (pWepKey->KeyLength != 13)))
				{
					NdisStatus = NDIS_STATUS_FAILURE;
					DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_ADD_WEP, INVALID_DATA!!\n");
				}
				else
				{
					UCHAR CipherAlg;
					pAd->SharedKey[KeyIdx].KeyLen = (UCHAR) pWepKey->KeyLength;
					memcpy(pAd->SharedKey[KeyIdx].Key, &pWepKey->KeyMaterial, pWepKey->KeyLength);
					CipherAlg = (pAd->SharedKey[KeyIdx].KeyLen == 5)? CIPHER_WEP64 : CIPHER_WEP128;
					pAd->SharedKey[KeyIdx].CipherAlg = CipherAlg;
					if (pWepKey->KeyIndex & 0x80000000)
					{
						// Default key for tx (shared key)
						pAd->PortCfg.DefaultKeyId = (UCHAR) KeyIdx;
					}
					AsicAddSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx, CipherAlg, pWepKey->KeyMaterial, NULL, NULL);
					DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_ADD_WEP (KeyIdx=%d, Len=%d-byte)\n", KeyIdx, pWepKey->KeyLength);
				}
			}
			break;

			case OID_802_11_REMOVE_WEP:
			{
				ULONG		KeyIdx;


				KeyIdx = *(NDIS_802_11_KEY_INDEX *) pData;
				if (KeyIdx & 0x80000000)
				{
					NdisStatus = NDIS_STATUS_FAILURE;
					DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_WEP, INVALID_DATA!!\n");
				}
				else
				{
					KeyIdx = KeyIdx & 0x0fffffff;
					if (KeyIdx >= 4)
					{
						NdisStatus = NDIS_STATUS_FAILURE;
						DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_WEP, Invalid KeyIdx[=%d]!!\n", KeyIdx);
					}
					else
					{
						pAd->SharedKey[KeyIdx].KeyLen = 0;
						pAd->SharedKey[KeyIdx].CipherAlg = CIPHER_NONE;
						AsicRemoveSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx);
						DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_REMOVE_WEP (KeyIdx=%d)\n", KeyIdx);
					}
				}
			}
			break;

			case OID_802_11_ADD_KEY_WEP:
			{
				PNDIS_802_11_KEY		pKey;
				ULONG					i, KeyIdx;

				pKey = (PNDIS_802_11_KEY) pData;
				KeyIdx = pKey->KeyIndex & 0x0fffffff;

				// it is a shared key
			    if (KeyIdx >= 4)
				{
			        NdisStatus = NDIS_STATUS_FAILURE;
			        DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_ADD_KEY_WEP, Invalid KeyIdx[=%d]!!\n", KeyIdx);
			    }
			    else
			    {
			        UCHAR CipherAlg;

			        pAd->SharedKey[KeyIdx].KeyLen = (UCHAR) pKey->KeyLength;
			        memcpy(pAd->SharedKey[KeyIdx].Key, &pKey->KeyMaterial, pKey->KeyLength);

			        if (pKey->KeyLength == 5)
				        CipherAlg = CIPHER_WEP64;
				    else
				        CipherAlg = CIPHER_WEP128;

				    // always expand the KEY to 16-byte here for efficiency sake. so that in case CKIP is used
				    // sometime later we don't have to do key expansion for each TX in RTUSBHardTransmit().
				    // However, we shouldn't change pAd->SharedKey[BSS0][KeyIdx].KeyLen
				    if (pKey->KeyLength < 16)
				    {
				        for(i = 1; i < (16 / pKey->KeyLength); i++)
				        {
				            memcpy(&pAd->SharedKey[KeyIdx].Key[i * pKey->KeyLength],
										   &pKey->KeyMaterial[0],
										   pKey->KeyLength);
				        }
					    memcpy(&pAd->SharedKey[KeyIdx].Key[i * pKey->KeyLength],
									   &pKey->KeyMaterial[0],
									   16 - (i * pKey->KeyLength));
				    }

				    pAd->SharedKey[KeyIdx].CipherAlg = CipherAlg;
				    if (pKey->KeyIndex & 0x80000000)
					{
				        // Default key for tx (shared key)
					    pAd->PortCfg.DefaultKeyId = (UCHAR) KeyIdx;
				    }

				    AsicAddSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx, CipherAlg, pAd->SharedKey[KeyIdx].Key, NULL, NULL);
				    DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_ADD_KEY_WEP (KeyIdx=%d, KeyLen=%d, CipherAlg=%d)\n",
				        pAd->PortCfg.DefaultKeyId, pAd->SharedKey[KeyIdx].KeyLen, pAd->SharedKey[KeyIdx].CipherAlg);
				}
			}
			break;

			case OID_802_11_ADD_KEY:
			{
                PNDIS_802_11_KEY	pkey = (PNDIS_802_11_KEY)pData;

				NdisStatus = RTMPWPAAddKeyProc(pAd, pkey);
				RTUSBBulkReceive(pAd);
				DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_ADD_KEY\n");
			}
			break;

#if 0
			case RT_OID_802_11_REMOVE_WEP:
			case OID_802_11_REMOVE_WEP:
			{
				ULONG  KeyIdx;


				KeyIdx = *(NDIS_802_11_KEY_INDEX *) pData;
				if (KeyIdx & 0x80000000)
				{
					NdisStatus = NDIS_STATUS_FAILURE;
					DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_WEP, INVALID_DATA!!\n");
				}
				else
				{
					KeyIdx = KeyIdx & 0x0fffffff;
					if (KeyIdx >= 4)
					{
						NdisStatus = NDIS_STATUS_FAILURE;
						DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_WEP, INVALID_DATA!!\n");
					}
						else
					{

					}
				}
			}
			break;
#if 0
			{
				//PNDIS_802_11_REMOVE_KEY  pRemoveKey;
				ULONG  KeyIdx;
				//pRemoveKey = (PNDIS_802_11_REMOVE_KEY) pData;
				//KeyIdx = pRemoveKey->KeyIndex;


				DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_WEP\n");
				//if (InformationBufferLength != sizeof(NDIS_802_11_KEY_INDEX))
				//	Status = NDIS_STATUS_INVALID_LENGTH;
				//else
				{
					KeyIdx = *(NDIS_802_11_KEY_INDEX *) pData;

					if (KeyIdx & 0x80000000)
					{
						// Should never set default bit when remove key
						//Status = NDIS_STATUS_INVALID_DATA;
					}
					else
					{
						KeyIdx = KeyIdx & 0x0fffffff;
						if (KeyIdx >= 4)
						{
							//Status = NDIS_STATUS_INVALID_DATA;
						}
						else
						{
							pAd->SharedKey[KeyIdx].KeyLen = 0;
							//Status = RT2573USBEnqueueCmdFromNdis(pAd, OID_802_11_REMOVE_WEP, TRUE, pInformationBuffer, InformationBufferLength);

							AsicRemoveSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx);
						}
					}
				}
			}
			break;
#endif
#endif
			case OID_802_11_REMOVE_KEY:
			{
				PNDIS_802_11_REMOVE_KEY  pRemoveKey;
				ULONG  KeyIdx;

				pRemoveKey = (PNDIS_802_11_REMOVE_KEY) pData;
				if (pAd->PortCfg.AuthMode >= Ndis802_11AuthModeWPA)
				{
					NdisStatus = RTMPWPARemoveKeyProc(pAd, pData);
					DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::RTMPWPARemoveKeyProc\n");
				}
				else
				{
					KeyIdx = pRemoveKey->KeyIndex;

					if (KeyIdx & 0x80000000)
					{
						// Should never set default bit when remove key
						NdisStatus = NDIS_STATUS_FAILURE;
						DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_KEY, Invalid KeyIdx[=%d]!!\n", KeyIdx);
					}
					else
					{
						KeyIdx = KeyIdx & 0x0fffffff;
						if (KeyIdx >= 4)
						{
							NdisStatus = NDIS_STATUS_FAILURE;
							DBGPRINT(RT_DEBUG_ERROR, "CMDHandler::OID_802_11_REMOVE_KEY, Invalid KeyIdx[=%d]!!\n", KeyIdx);
						}
						else
						{
							pAd->SharedKey[KeyIdx].KeyLen = 0;
							pAd->SharedKey[KeyIdx].CipherAlg = CIPHER_NONE;
							AsicRemoveSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx);
							DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::AsicRemoveSharedKeyEntry(KeyIdx=%d)\n", KeyIdx);
						}
					}
				}

			}
			break;

			case OID_802_11_POWER_MODE:
			{
				NDIS_802_11_POWER_MODE PowerMode = *(PNDIS_802_11_POWER_MODE) pData;
				DBGPRINT(RT_DEBUG_TRACE, "CMDHandler::OID_802_11_POWER_MODE (=%d)\n",PowerMode);

				// save user's policy here, but not change PortCfg.Psm immediately
				if (PowerMode == Ndis802_11PowerModeCAM)
				{
					// clear PSM bit immediately
					MlmeSetPsmBit(pAd, PWR_ACTIVE);

					OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
					if (pAd->PortCfg.bWindowsACCAMEnable == FALSE)
						pAd->PortCfg.WindowsPowerMode = PowerMode;
					pAd->PortCfg.WindowsBatteryPowerMode = PowerMode;
				}
				else if (PowerMode == Ndis802_11PowerModeMAX_PSP)
				{
					// do NOT turn on PSM bit here, wait until MlmeCheckPsmChange()
					// to exclude certain situations.
					//     MlmeSetPsmBit(pAd, PWR_SAVE);
					if (pAd->PortCfg.bWindowsACCAMEnable == FALSE)
						pAd->PortCfg.WindowsPowerMode = PowerMode;
					pAd->PortCfg.WindowsBatteryPowerMode = PowerMode;
					OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
					pAd->PortCfg.DefaultListenCount = 5;
				}
				else if (PowerMode == Ndis802_11PowerModeFast_PSP)
				{
					// do NOT turn on PSM bit here, wait until MlmeCheckPsmChange()
					// to exclude certain situations.
					//     MlmeSetPsmBit(pAd, PWR_SAVE);
					OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
					if (pAd->PortCfg.bWindowsACCAMEnable == FALSE)
						pAd->PortCfg.WindowsPowerMode = PowerMode;
					pAd->PortCfg.WindowsBatteryPowerMode = PowerMode;
					pAd->PortCfg.DefaultListenCount = 3;
				}
			}
			break;

			case RT_PERFORM_SOFT_DIVERSITY:
				AsicRxAntEvalAction(pAd);
			break;

		    case RT_OID_FORCE_WAKE_UP:
			    AsicForceWakeup(pAd);
			break;

		    case RT_OID_SET_PSM_BIT_ACTIVE:
			    MlmeSetPsmBit(pAd, PWR_ACTIVE);
		    break;

			default:
			break;
		}


		if (cmdqelmt->CmdFromNdis == TRUE)
		{

			if ((cmdqelmt->command != OID_802_11_BSSID_LIST_SCAN) &&
				(cmdqelmt->command != RT_OID_802_11_BSSID) &&
				(cmdqelmt->command != OID_802_11_SSID) &&
				(cmdqelmt->command != OID_802_11_DISASSOCIATE))
			{
			}

			if ((cmdqelmt->command != RT_OID_MULTI_READ_MAC) &&
				(cmdqelmt->command != RT_OID_VENDOR_READ_BBP) &&
#ifdef DBG
				(cmdqelmt->command != RT_OID_802_11_QUERY_HARDWARE_REGISTER) &&
#endif
				(cmdqelmt->command != RT_OID_USB_VENDOR_EEPROM_READ))
			{
#if 1

				if (cmdqelmt->buffer != NULL){
					kfree(cmdqelmt->buffer);
				}
#endif
			}
#if 1
			if(cmdqelmt != NULL){
				kfree((PCmdQElmt)cmdqelmt);
			}
#endif
		}
		else{
			cmdqelmt->InUse = FALSE;
		}


	}


}

#ifdef RT2X00DEBUGFS
/*
 * Ethtool handlers.
 */
#define CSR_REG_BASE			0x3000
#define CSR_REG_SIZE			0x04b0
#define EEPROM_BASE			0x0000
#define EEPROM_SIZE			0x0100
#define BBP_SIZE			0x0080


static void rt73usb_read_csr(void *dev, const unsigned long word,
		void *data)
{
	RTMP_ADAPTER *pAd = dev;

	RTUSBReadMACRegister(pAd, CSR_REG_BASE + (word * sizeof(u32)), (u32*)data);
}

static void rt73usb_write_csr(void *dev, const unsigned long word,
	void *data)
{
	RTMP_ADAPTER *pAd = dev;

	RTUSBWriteMACRegister(pAd, word, *((u32*)data));
}

static void rt73usb_read_eeprom(void *dev, const unsigned long word,
		void *data)
{
	RTMP_ADAPTER *pAd = dev;

	RTUSBReadEEPROM(pAd, word * sizeof(u16), ((u8*)data), 2);
}

static void rt73usb_write_eeprom(void *dev, const unsigned long word,
	void *data)
{
	/* DANGEROUS, DON'T DO THIS! */
}

static void rt73usb_read_bbp(void *dev, const unsigned long word,
		void *data)
{
	RTMP_ADAPTER *pAd = dev;

	RTUSBReadBBPRegister(pAd, (u8)word, ((u8*)data));
}

static void rt73usb_write_bbp(void *dev, const unsigned long word,
	void *data)
{
	RTMP_ADAPTER *pAd = dev;

	RTUSBWriteBBPRegister(pAd, word, *((u8*)data));
}

static void rt73usb_open_debugfs(RTMP_ADAPTER *pAd)
{
	struct rt2x00debug *debug = &pAd->debug;

	debug->owner 			= THIS_MODULE;
	debug->mod_name			= DRIVER_NAME;
	debug->mod_version		= DRIVER_VERSION;
	debug->reg_csr.read		= rt73usb_read_csr;
	debug->reg_csr.write		= rt73usb_write_csr;
	debug->reg_csr.word_size	= sizeof(u32);
	debug->reg_csr.length		= CSR_REG_SIZE;
	debug->reg_eeprom.read		= rt73usb_read_eeprom;
	debug->reg_eeprom.write		= rt73usb_write_eeprom;
	debug->reg_eeprom.word_size	= sizeof(u16);
	debug->reg_eeprom.length	= EEPROM_SIZE;
	debug->reg_bbp.read		= rt73usb_read_bbp;
	debug->reg_bbp.write		= rt73usb_write_bbp;
	debug->reg_bbp.word_size	= sizeof(u8);
	debug->reg_bbp.length		= BBP_SIZE;
	debug->dev 			= pAd;

	snprintf(debug->intf_name, sizeof(debug->intf_name),
		"%s", pAd->net_dev->name);

	if (rt2x00debug_register(debug))
		printk(KERN_ERR "Failed to register debug handler.\n");
}

static void rt73usb_close_debugfs(RTMP_ADAPTER *pAd)
{
	rt2x00debug_deregister(&pAd->debug);
}
#else /* RT2X00DEBUGFS */
static inline void rt73usb_open_debugfs(RTMP_ADAPTER *pAd){}
static inline void rt73usb_close_debugfs(RTMP_ADAPTER *pAd){}
#endif /* RT2X00DEBUGFS */

static int usb_rtusb_open(struct net_device *net_dev)
{
	PRTMP_ADAPTER   pAd = (PRTMP_ADAPTER) net_dev->priv;
	NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
	UCHAR           TmpPhy;
	int ret = 0;

	printk("rt73 driver version - %s\n", DRIVER_VERSION);
	if ( !OPSTATUS_TEST_FLAG (pAd,fOP_STATUS_FIRMWARE_LOAD) ) {
		DBGPRINT(RT_DEBUG_ERROR, "Firmware not load");
		ret = -EIO;
		goto out_firmware_error;
	}

	init_MUTEX(&(pAd->usbdev_semaphore));

	// init mediastate to disconnected
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

	pAd->rx_bh.func = RTUSBRxPacket;
	pAd->rx_bk.func = rtusb_bulkrx;

	// Initialize pAd->PortCfg to manufacture default
	PortCfgInit(pAd);


	// Init  RTMP_ADAPTER CmdQElements
	Status = RTMPInitAdapterBlock(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return Status;
	}

    //
	// Init send data structures and related parameters
    //
	Status = NICInitTransmit(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		return Status;
	}

	//
	// Init receive data structures and related parameters
	//
	Status = NICInitRecv(pAd);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		goto out;
	}

	// Initialize Asics
	NICInitializeAsic(pAd);

	//
	// Read additional info from NIC such as MAC address
	// This function must called after register CSR base address
	//
#ifdef	INIT_FROM_EEPROM
	NICReadEEPROMParameters(pAd);
	NICInitAsicFromEEPROM(pAd);
#endif
	RTUSBWriteHWMACAddress(pAd);

	// external LNA has different R17 base
	if (pAd->NicConfig2.field.ExternalLNA)
	{
		pAd->BbpTuning.R17LowerBoundA += 0x10;
		pAd->BbpTuning.R17UpperBoundA += 0x10;
		pAd->BbpTuning.R17LowerBoundG += 0x10;
		pAd->BbpTuning.R17UpperBoundG += 0x10;
	}

	// hardware initialization after all parameters are acquired from
	// Registry or E2PROM
	TmpPhy = pAd->PortCfg.PhyMode;
	pAd->PortCfg.PhyMode = 0xff;
	RTMPSetPhyMode(pAd, TmpPhy);


	//
	// initialize MLME
    //
	Status = MlmeInit(pAd);
	if(Status != NDIS_STATUS_SUCCESS)
	{
		goto out;
	}

	// mlmethread & RTUSBCmd flag restart
	mlme_kill = 0;
	RTUSBCmd_kill = 0;

	CreateThreads(net_dev);

	// at every open handler, copy mac address.
	if (!memcmp(net_dev->dev_addr, "\x00\x00\x00\x00\x00\x00", 6))
		memcpy(pAd->net_dev->dev_addr, pAd->CurrentAddress, pAd->net_dev->addr_len);
	else
		memcpy(pAd->CurrentAddress, pAd->net_dev->dev_addr, pAd->net_dev->addr_len);

	// Clear Reset Flag before starting receiving/transmitting
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);


	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
	{
		RTUSBBulkReceive(pAd);
        RTUSBWriteMACRegister(pAd, TXRX_CSR0, 0x025eb032);    // enable RX of MAC block, Staion not drop control frame
        // Initialize RF register to default value
	    AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
	    AsicLockChannel(pAd, pAd->PortCfg.Channel);
	}


    // USB_ID info for UI
    pAd->VendorDesc = 0x148F2573;

	// Start net_dev interface tx /rx
	netif_start_queue(net_dev);

	netif_carrier_on(net_dev);
	netif_wake_queue(net_dev);
	return 0;


out:
	ReleaseAdapter(pAd, TRUE, FALSE);
out_firmware_error:
	return ret;

}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static int usb_rtusb_close(struct net_device *net_dev)
{
	PRTMP_ADAPTER   pAd = (PRTMP_ADAPTER) net_dev->priv;
	int             ret;
	int		i = 0;

	DECLARE_WAIT_QUEUE_HEAD (unlink_wakeup);
	DECLARE_WAITQUEUE (wait, current);

	DBGPRINT(RT_DEBUG_TRACE,"-->rt73_close\n");

	netif_carrier_off(pAd->net_dev);
	netif_stop_queue(pAd->net_dev);

	DBGPRINT(RT_DEBUG_INFO,"Ensure there are no more active urbs \n");
	// ensure there are no more active urbs.
	add_wait_queue (&unlink_wakeup, &wait);
	pAd->wait = &unlink_wakeup;
	// maybe wait for deletions to finish.
	while ((i < 10) && atomic_read(&pAd->PendingRx) > 0) {
		//msleep(UNLINK_TIMEOUT_MS);
		i++;
		DBGPRINT (RT_DEBUG_INFO,"waited for %d urb to complete\n", atomic_read(&pAd->PendingRx));
	}
	pAd->wait = NULL;
	remove_wait_queue (&unlink_wakeup, &wait);

	if (pAd->MLMEThr_pid >= 0)
	{
		mlme_kill = 1;
		RTUSBMlmeUp(pAd);
		wmb(); // need to check
		ret = kill_proc (pAd->MLMEThr_pid, SIGTERM, 1);
		if (ret)
		{
			printk (KERN_ERR "%s: unable to signal thread\n", pAd->net_dev->name);
			//return ret;		Fix process killing
		}
		wait_for_completion (&pAd->notify);
	}
	if (pAd->RTUSBCmdThr_pid>= 0)
	{
		RTUSBCmd_kill = 1;
		RTUSBCMDUp(pAd);
		wmb(); // need to check
		ret = kill_proc (pAd->RTUSBCmdThr_pid, SIGTERM, 1);
		if (ret)
		{
			printk (KERN_ERR "%s: unable to signal thread\n", pAd->net_dev->name);
			//return ret;		Fix process killing
		}
	    wait_for_completion (&pAd->notify);
	}

	RTUSBHalt(pAd, TRUE);
	DBGPRINT(RT_DEBUG_TRACE,"<--rt73_close\n");

	return 0;
}

INT MlmeThread(
    IN void * Context)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)Context;

	daemonize();
	current->flags |= PF_NOFREEZE;
	/* signal that we've started the thread */
	complete(&(pAd->notify));
#if 1
	while (1)
	{
		//if(down_interruptible(&pAd->mlme_semaphore))
			//break;

		/* lock the device pointers */
		down(&(pAd->mlme_semaphore));

		if (mlme_kill)
			break;

		/* lock the device pointers , need to check if required*/
		down(&(pAd->usbdev_semaphore));
		MlmeHandler(pAd);

		/* unlock the device pointers */
		up(&(pAd->usbdev_semaphore));
	}
#else
	// I tried this way for thread handling
	while(1)
	{
		timeout = next_tick;
		do {
			timeout = interruptible_sleep_on_timeout (&pAd->MLMEThr_wait, timeout);
			/* make swsusp happy with our thread */
			if (current->flags & PF_FREEZE)
				refrigerator(PF_FREEZE);
			DBGPRINT(RT_DEBUG_TRACE, "current->flags  = 0x%x\n",current->flags );
		} while (!signal_pending (current) && (timeout > 0));

		if (signal_pending (current)) {
			flush_signals(current);
		}

		if (mlme_kill)
			break;
	}
#endif

	/* notify the exit routine that we're actually exiting now
	 *
	 * complete()/wait_for_completion() is similar to up()/down(),
	 * except that complete() is safe in the case where the structure
	 * is getting deleted in a parallel mode of execution (i.e. just
	 * after the down() -- that's necessary for the thread-shutdown
	 * case.
	 *
	 * complete_and_exit() goes even further than this -- it is safe in
	 * the case that the thread of the caller is going away (not just
	 * the structure) -- this is necessary for the module-remove case.
	 * This is important in preemption kernels, which transfer the flow
	 * of execution immediately upon a complete().
	 */
	complete_and_exit (&pAd->notify, 0);
	DBGPRINT(RT_DEBUG_TRACE, "<---MlmeThread\n");

}
INT RTUSBCmdThread(
    IN void * Context)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)Context;

	daemonize();
	current->flags |= PF_NOFREEZE;
	/* signal that we've started the thread */
	complete(&(pAd->notify));

	while (1)
	{
		//if(down_interruptible(&pAd->mlme_semaphore))
			//break;

		/* lock the device pointers */
		down(&(pAd->RTUSBCmd_semaphore));

		if (RTUSBCmd_kill)
			break;

		/* lock the device pointers , need to check if required*/
		down(&(pAd->usbdev_semaphore));
		CMDHandler(pAd);

		/* unlock the device pointers */
		up(&(pAd->usbdev_semaphore));
	}

	/* notify the exit routine that we're actually exiting now
	 *
	 * complete()/wait_for_completion() is similar to up()/down(),
	 * except that complete() is safe in the case where the structure
	 * is getting deleted in a parallel mode of execution (i.e. just
	 * after the down() -- that's necessary for the thread-shutdown
	 * case.
	 *
	 * complete_and_exit() goes even further than this -- it is safe in
	 * the case that the thread of the caller is going away (not just
	 * the structure) -- this is necessary for the module-remove case.
	 * This is important in preemption kernels, which transfer the flow
	 * of execution immediately upon a complete().
	 */
	complete_and_exit (&pAd->notify, 0);
	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBCmdThread\n");

}

static void *usb_rtusb_probe(struct usb_device *dev, UINT interface,
				const struct usb_device_id *id_table)
{
		PRTMP_ADAPTER       pAd;
	int                 i;
	struct net_device   *netdev;
	int                 res = -ENOMEM;

	for (i = 0; i < rtusb_usb_id_len; i++)
	{
		if (le16_to_cpu(dev->descriptor.idVendor) == rtusb_usb_id[i].idVendor &&
			le16_to_cpu(dev->descriptor.idProduct) == rtusb_usb_id[i].idProduct)
		{
			printk("idVendor = 0x%x, idProduct = 0x%x \n",
					le16_to_cpu(dev->descriptor.idVendor),
					le16_to_cpu(dev->descriptor.idProduct));
			break;
		}
	}
	if (i == rtusb_usb_id_len) {
		printk("Device Descriptor not matching\n");
		goto out_noalloc;
	}

	/* RTMP_ADAPTER is too big with 64-bit pointers, and using the
	   builtin net_device private area causes the allocation to
	   exceed 128KB and fail.  So we allocate it separately. */
	pAd = kzalloc(sizeof (*pAd), GFP_KERNEL);
	if (!pAd) {
		printk("couldn't allocate RTMP_ADAPTER\n");
		goto out_noalloc;
	}

	netdev = alloc_etherdev(0);
	if (!netdev) {
		goto out_nonetdev;
	}

	netdev->priv = pAd;
	pAd->net_dev = netdev;
	netif_stop_queue(netdev);
	pAd->config = dev->config;
	pAd->pUsb_Dev= dev;
	SET_MODULE_OWNER(netdev);
	ether_setup(netdev);

	netdev->open = usb_rtusb_open;
	netdev->hard_start_xmit = RTMPSendPackets;
	netdev->stop = usb_rtusb_close;
	netdev->priv = pAd;
	netdev->get_stats = rt73_get_ether_stats;
#if WIRELESS_EXT >= 15
#if WIRELESS_EXT < 17
	netdev->get_wireless_stats = rt73_get_wireless_stats;
#endif
	netdev->wireless_handlers = (struct iw_handler_def *) &rt73_iw_handler_def;
#endif
	netdev->do_ioctl = rt73_ioctl;
	netdev->hard_header_len = 14;
	netdev->mtu = 1500;
	netdev->addr_len = 6;
#if WIRELESS_EXT >= 15
	netdev->weight = 64;
#endif

	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

	{// find available
		int 	i=0;
		char	slot_name[IFNAMSIZ];
		struct  net_device	*device;
		struct  usb_interface *ifp= &dev->actconfig->interface[interface];  // get interface from system
        struct  usb_interface_descriptor *as;
        struct  usb_endpoint_descriptor *ep;

		for (i = 0; i < 8; i++)
		{
			sprintf(slot_name, "wlan%d", i);

			read_lock_bh(&dev_base_lock); // avoid multiple init
			for (device = first_net_device(); device != NULL;
					device = next_net_device(device))
			{
				if (strncmp(device->name, slot_name, 4) == 0)
				{
					break;
				}
			}
			read_unlock_bh(&dev_base_lock);

			if(device == NULL)	break;
		}
		if(i == 8)
		{
			DBGPRINT(RT_DEBUG_ERROR, "No available slot name\n");
			goto out;
		}

		sprintf(netdev->name, "wlan%d", i);
		DBGPRINT(RT_DEBUG_ERROR, "usb device name %s\n",netdev->name);

        /* get Max Packet Size from usb_dev endpoint */
//        ifp = dev->actconfig->interface + i;
        as = ifp->altsetting + ifp->act_altsetting;
        ep = as->endpoint;

        pAd->BulkOutMaxPacketSize = (USHORT)ep[i].wMaxPacketSize;
        DBGPRINT(RT_DEBUG_TRACE, "BulkOutMaxPacketSize  %d\n", pAd->BulkOutMaxPacketSize);


	}

	//pAd->rx_bh.data = (unsigned long)pAd;
	pAd->rx_bh.func = RTUSBRxPacket;
	pAd->rx_bk.func = rtusb_bulkrx;

	res = register_netdev(netdev);
	if (res) {
		printk("register_netdev failed err=%d\n",res);
		goto out;
	}

	return pAd;

out:
	free_netdev(netdev);
out_nonetdev:
	kfree(pAd);
out_noalloc:
	MOD_DEC_USE_COUNT;
	usb_dec_dev_use(dev);
	return NULL;
}

//Disconnect function is called within exit routine
static void usb_rtusb_disconnect(struct usb_device *dev, void *ptr)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) ptr;


	if (!pAd)
		return;

	tasklet_kill(&pAd->rx_bh);
	tasklet_kill(&pAd->rx_bk);
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST);
	// for debug, wait to show some messages to /proc system
	udelay(1);
	//After Add Thread implementation, Upon exec there, pAd->net_dev seems becomes NULL,
	//need to check why???
	//assert(pAd->net_dev != NULL)
	if(pAd->net_dev != NULL)
	{
		printk("unregister_netdev()\n");
		unregister_netdev (pAd->net_dev);
	}
	udelay(1);
	udelay(1);

	while (MOD_IN_USE > 0) {
		MOD_DEC_USE_COUNT;
	}
	udelay(1);
	DBGPRINT(RT_DEBUG_ERROR,"<=== RTUSB disconnect successfully\n");

}

#else
static int usb_rtusb_close(struct net_device *net_dev)
{
	PRTMP_ADAPTER   pAd = (PRTMP_ADAPTER) net_dev->priv;
	int             ret;
	int	            i = 0;

	DECLARE_WAIT_QUEUE_HEAD (unlink_wakeup);
	DECLARE_WAITQUEUE (wait, current);

	DBGPRINT(RT_DEBUG_TRACE,"-->rt73_close \n");

	netif_carrier_off(pAd->net_dev);
	netif_stop_queue(pAd->net_dev);

	// ensure there are no more active urbs.
	add_wait_queue (&unlink_wakeup, &wait);
	pAd->wait = &unlink_wakeup;

	// maybe wait for deletions to finish.
	while ((i < 25) && atomic_read(&pAd->PendingRx) > 0) {
#if LINUX_VERSION_CODE >KERNEL_VERSION(2,6,9)

		msleep(UNLINK_TIMEOUT_MS);
#endif
		i++;
	}
	pAd->wait = NULL;
	remove_wait_queue (&unlink_wakeup, &wait);

	if (pAd->MLMEThr_pid >= 0)
	{
		mlme_kill = 1;
		RTUSBMlmeUp(pAd);
		wmb(); // need to check
		ret = kill_proc (pAd->MLMEThr_pid, SIGTERM, 1);
		if (ret)
		{
			printk (KERN_ERR "%s: unable to signal thread\n", pAd->net_dev->name);
			//return ret;
		}
		wait_for_completion (&pAd->notify);
	}
	if (pAd->RTUSBCmdThr_pid>= 0)
	{
		RTUSBCmd_kill = 1;
		RTUSBCMDUp(pAd);
		wmb(); // need to check
		ret = kill_proc (pAd->RTUSBCmdThr_pid, SIGTERM, 1);
		if (ret)
		{
			printk (KERN_ERR "%s: unable to signal thread\n", pAd->net_dev->name);
			//return ret;
		}
		wait_for_completion (&pAd->notify);
	}
	RTUSBHalt(pAd, TRUE);

	DBGPRINT(RT_DEBUG_TRACE,"<--rt73_close \n");

	return 0;
}

INT MlmeThread(
    IN void * Context)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)Context;

	daemonize("rt73");
	allow_signal(SIGTERM);
	current->flags |= PF_NOFREEZE;
	/* signal that we've started the thread */
	complete(&(pAd->notify));

	while (1)
	{
		//if(down_interruptible(&pAd->mlme_semaphore))
			//break;

		/* lock the device pointers */
		down(&(pAd->mlme_semaphore));

		if (mlme_kill)
			break;

		/* lock the device pointers , need to check if required*/
		down(&(pAd->usbdev_semaphore));

		MlmeHandler(pAd);

		/* unlock the device pointers */
		up(&(pAd->usbdev_semaphore));
	}

	/* notify the exit routine that we're actually exiting now
	 *
	 * complete()/wait_for_completion() is similar to up()/down(),
	 * except that complete() is safe in the case where the structure
	 * is getting deleted in a parallel mode of execution (i.e. just
	 * after the down() -- that's necessary for the thread-shutdown
	 * case.
	 *
	 * complete_and_exit() goes even further than this -- it is safe in
	 * the case that the thread of the caller is going away (not just
	 * the structure) -- this is necessary for the module-remove case.
	 * This is important in preemption kernels, which transfer the flow
	 * of execution immediately upon a complete().
	 */
	complete_and_exit (&pAd->notify, 0);
	DBGPRINT(RT_DEBUG_TRACE, "<---MlmeThread\n");
}

INT RTUSBCmdThread(
    IN void * Context)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)Context;

	daemonize("rt73");
	allow_signal(SIGTERM);
	current->flags |= PF_NOFREEZE;
	/* signal that we've started the thread */
	complete(&(pAd->notify));

	while (1)
	{
		//if(down_interruptible(&pAd->mlme_semaphore))
			//break;

		/* lock the device pointers */
		down(&(pAd->RTUSBCmd_semaphore));

		if (RTUSBCmd_kill)
			break;

		/* lock the device pointers , need to check if required*/
		down(&(pAd->usbdev_semaphore));

		CMDHandler(pAd);

		/* unlock the device pointers */
		up(&(pAd->usbdev_semaphore));
	}

	/* notify the exit routine that we're actually exiting now
	 *
	 * complete()/wait_for_completion() is similar to up()/down(),
	 * except that complete() is safe in the case where the structure
	 * is getting deleted in a parallel mode of execution (i.e. just
	 * after the down() -- that's necessary for the thread-shutdown
	 * case.
	 *
	 * complete_and_exit() goes even further than this -- it is safe in
	 * the case that the thread of the caller is going away (not just
	 * the structure) -- this is necessary for the module-remove case.
	 * This is important in preemption kernels, which transfer the flow
	 * of execution immediately upon a complete().
	 */
	complete_and_exit (&pAd->notify, 0);
}

static int usb_rtusb_probe (struct usb_interface *intf,
					  const struct usb_device_id *id)
{
	struct usb_device   *dev = interface_to_usbdev(intf);
	PRTMP_ADAPTER       pAd;
	int                 i;
	struct net_device   *netdev;
	int                 res = -ENOMEM;

	usb_get_dev(dev);
	for (i = 0; i < rtusb_usb_id_len; i++)
	{
		if (le16_to_cpu(dev->descriptor.idVendor) == rtusb_usb_id[i].idVendor &&
			le16_to_cpu(dev->descriptor.idProduct) == rtusb_usb_id[i].idProduct)
		{
			printk("idVendor = 0x%x, idProduct = 0x%x \n",
					le16_to_cpu(dev->descriptor.idVendor),
					le16_to_cpu(dev->descriptor.idProduct));
			break;
		}
	}
	if (i == rtusb_usb_id_len) {
		printk("Device Descriptor not matching\n");
		goto out_noalloc;
	}

	/* RTMP_ADAPTER is too big with 64-bit pointers, and using the
	   builtin net_device private area causes the allocation to
	   exceed 128KB and fail.  So we allocate it separately. */
	pAd = kzalloc(sizeof (*pAd), GFP_KERNEL);
	if (!pAd) {
		printk("couldn't allocate RTMP_ADAPTER\n");
		goto out_noalloc;
	}

	netdev = alloc_etherdev(0);
	if (!netdev) {
		printk("alloc_etherdev failed\n");
		goto out_nonetdev;
	}

	netdev->priv = pAd;
	pAd->net_dev = netdev;
	netif_stop_queue(netdev);
	pAd->config = &dev->config->desc;
	pAd->pUsb_Dev = dev;
	SET_MODULE_OWNER(netdev);
	ether_setup(netdev);

	netdev->open = usb_rtusb_open;
	netdev->stop = usb_rtusb_close;
	netdev->priv = pAd;
	netdev->hard_start_xmit = RTMPSendPackets;
	netdev->get_stats = rt73_get_ether_stats;

#if WIRELESS_EXT >= 12
#if WIRELESS_EXT < 17
	netdev->get_wireless_stats = rt73_get_wireless_stats;
#endif
	netdev->wireless_handlers = (struct iw_handler_def *) &rt73_iw_handler_def;
#endif
	netdev->do_ioctl = rt73_ioctl;

	netdev->hard_header_len = 14;
	netdev->mtu = 1500;
	netdev->addr_len = 6;
	netdev->weight = 64;

	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

	pAd->MLMEThr_pid= -1;
	pAd->RTUSBCmdThr_pid= -1;

	SET_NETDEV_DEV(netdev, &intf->dev);

	{// find available
		int 	i=0;
		char	slot_name[IFNAMSIZ];
		//struct  net_device	*device;
        struct  usb_host_interface *iface_desc;
        struct  usb_endpoint_descriptor *endpoint;


		for (i = 0; i < 8; i++)
		{
			sprintf(slot_name, "wlan%d", i);

#if 1
			if(dev_get_by_name(slot_name)==NULL)
				break;
#else
			read_lock_bh(&dev_base_lock); // avoid multiple init
			for (device = first_net_device(); device != NULL;
					device = next_net_device(device))
			{
				if (strncmp(device->name, slot_name, 4) == 0)
				{
					break;
				}
			}
			read_unlock_bh(&dev_base_lock);

			if(device == NULL)	break;
#endif
		}
		if(i == 8)
		{
			DBGPRINT(RT_DEBUG_ERROR, "No available slot name\n");
			return res;
		}

		sprintf(pAd->net_dev->name, "wlan%d", i);
		DBGPRINT(RT_DEBUG_ERROR, "usb device name %s\n", pAd->net_dev->name);


        /* get the active interface descriptor */
        iface_desc = intf->cur_altsetting;

        /* check out the endpoint: it has to be Interrupt & IN */
        endpoint = &iface_desc->endpoint[i].desc;

        /* get Max Packet Size from endpoint */
        pAd->BulkOutMaxPacketSize = (USHORT)endpoint->wMaxPacketSize;
        DBGPRINT(RT_DEBUG_TRACE, "BulkOutMaxPacketSize  %d\n", pAd->BulkOutMaxPacketSize);

	}

    //bottom half data is assign at  each task_scheduler
	//pAd->rx_bh.data = (unsigned long)pAd;
	pAd->rx_bh.func = RTUSBRxPacket;


	res = register_netdev(netdev);
	if (res) {
		printk("register_netdev failed err=%d\n",res);
		goto out;
	}

	usb_set_intfdata(intf, pAd);


	rt73usb_open_debugfs(pAd);
	res = LoadFirmware(pAd, firmName);
	if (res) {
		DBGPRINT(RT_DEBUG_ERROR, "Failed to request Firmware.\n");
		goto out;
	}
	return 0;

out:
	free_netdev(netdev);
out_nonetdev:
	kfree(pAd);
out_noalloc:
	module_put(THIS_MODULE);
	return res;
}

static void usb_rtusb_disconnect(struct usb_interface *intf)
{
	struct usb_device   *dev = interface_to_usbdev(intf);
	PRTMP_ADAPTER       pAd = (PRTMP_ADAPTER)NULL;

	pAd = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST);
	DBGPRINT(RT_DEBUG_ERROR,"unregister usbnet usb-%s-%s\n",
		dev->bus->bus_name, dev->devpath);
	if (!pAd)
		return;

	rt73usb_close_debugfs(pAd);

	tasklet_kill(&pAd->rx_bh);
	tasklet_kill(&pAd->rx_bk);
	// for debug, wait to show some messages to /proc system
	udelay(1);
	//After Add Thread implementation, Upon exec there, pAd->net_dev seems becomes NULL,
	//need to check why???
	//assert(pAd->net_dev != NULL)
	if(pAd->net_dev!= NULL)
	{
		printk("unregister_netdev()\n");
		unregister_netdev (pAd->net_dev);
	}
	udelay(1);
	flush_scheduled_work ();
	udelay(1);

	free_netdev(pAd->net_dev);
	kfree(pAd);
	usb_put_dev(dev);
	udelay(1);
	DBGPRINT(RT_DEBUG_ERROR,"<=== RTUSB disconnect successfully\n");

}
#endif


//
// Driver module load function
//
INT __init usb_rtusb_init(void)
{
    printk("rtusb init ====>\n");
	#ifdef DBG
    RTDebugLevel = debug;
	#else
	if (debug) {}	// keep compiler from complaining
	#endif
	if ( strlen(firmName)  > FIRMWARE_NAME_MAX ) {
		DBGPRINT(RT_DEBUG_ERROR,"Firmware name too long\n");
		return -E2BIG;
	}
	return usb_register(&rtusb_driver);
}

//
// Driver module unload function
//
VOID __exit usb_rtusb_exit(void)
{
	udelay(1);
	udelay(1);
	usb_deregister(&rtusb_driver);

	printk("<=== rtusb exit\n");
}
/**************************************/
module_init(usb_rtusb_init);
module_exit(usb_rtusb_exit);
/**************************************/

