/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/common/gl_kal_ndis51.c#1 $
*/

/*! \file   gl_kal_ndis51.c
    \brief  KAL routines of Windows NDIS5.1 driver

*/



/*
** $Log: gl_kal_ndis51.c $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 01 04 2011 cp.wu
 * [WCXRP00000338] [MT6620 Wi-Fi][Driver] Separate kalMemAlloc into kmalloc and vmalloc implementations to ease physically continous memory demands
 * separate kalMemAlloc() into virtually-continous and physically-continous type to ease slab system pressure
 *
 * 10 06 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * code reorganization to improve isolation between GLUE and CORE layers.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 04 20 2010 cp.wu
 * [WPD00003829][MT6620 Wi-Fi] WHQL 2c_simulation fail with crash
 * sort packets array before invoking NdisMIndicateReceivePacket
 * 'cause once NDIS_STATUS_RESOURCES is indicated, all subsequent packets are treat in the same manner.
 * i.e., NDIS_STATUS_RESOURCES and NDIS_STATUS_SUCCESS should never interleave in the indicated packet array
 *
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  *  *  *  *  *  *  *  *  * are done in adapter layer.
 *
 * 03 29 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * simplify media stream indication when associating/disassociating
 *
 * 03 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add indication for media stream mode change
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add high resolution kalGetNanoTick (100ns) for profiling purpose
 *
 * 02 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * move EVENT_ID_ASSOC_INFO from nic_rx.c to gl_kal_ndis_51.c
 *  *  * 'cause it involves OS dependent data structure handling
 *
 * 02 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add checksum offloading support.
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-12-10 16:45:16 GMT mtk02752
**  remove unused API
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-10-02 14:04:11 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-05-12 09:44:10 GMT mtk01461
**  User InterlockDecrese to count the i4TxPendingFrameNum
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-04-29 15:43:23 GMT mtk01461
**  Update function of kalQueryTxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-04-14 20:50:08 GMT mtk01426
**  Fixed RX OOB error
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-04-14 15:51:58 GMT mtk01426
**  Update kalSetRxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-23 21:58:01 GMT mtk01461
**  Add kalQueryTxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-18 21:05:25 GMT mtk01426
**  Add kalSetRxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-17 09:54:14 GMT mtk01426
**  Fixed typo error
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:36:56 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_os.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MIN_ASSOC_REQ_BODY_LEN          9
#define MIN_REASSOC_REQ_BODY_LEN        15
#define MIN_REASSOC_RESP_BODY_LEN       6

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/



/*----------------------------------------------------------------------------*/
/*!
* @brief Cache-able memory allocation
*
* @param u4Size Size of memory to allocate, in bytes
* @param eMemType Memory allocation type
*
* @return Return the virtual address os allocated memory
*/
/*----------------------------------------------------------------------------*/
PVOID kalMemAlloc(IN UINT_32 u4Size, IN ENUM_KAL_MEM_ALLOCATION_TYPE eMemType)
{
	PVOID pvAddr;

	if (NdisAllocateMemoryWithTag(&pvAddr, u4Size, NIC_MEM_TAG) == NDIS_STATUS_SUCCESS) {
		return pvAddr;
	}

	return NULL;
}				/* kalMemAlloc */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function returns current timestamp in unit of OS system, that have
*        elapsed since the system was booted.
*
* @param  none
*
* @return The LSB 32-bits of the system uptime
*/
/*----------------------------------------------------------------------------*/
OS_SYSTIME kalGetTimeTick(VOID)
{
	ULONG u4SystemUpTime;

	NdisGetSystemUpTime(&u4SystemUpTime);

	return (OS_SYSTIME) u4SystemUpTime;
}				/* kalGetTimeTick */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function returns current timestamp in unit of 100ns, that have
*        elapsed since January 1, 1601.
*
* @param  none
*
* @return The 64-bits of the system ticks in unit of 100ns
*/
/*----------------------------------------------------------------------------*/
UINT_64 kalGetNanoTick(VOID)
{
	LARGE_INTEGER rSystemTime;

	NdisGetCurrentSystemTime(&rSystemTime);

	return rSystemTime.QuadPart;
}				/* kalGetNanoTick */


/*----------------------------------------------------------------------------*/
/*!
* @brief This is to copy the content of the packet into one contiguous buffer,
*        which may be distributed into different buffers from the OS originally.
*
* @param prGlueInfo     Pointer of GLUE Data Structure
* @param pvPacket       Pointer of the packet descriptor
* @param pucDestBuffer  Pointer of packet buffer to be copied into
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalCopyFrame(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, OUT PUINT_8 pucDestBuffer)
{
	PNDIS_PACKET prNdisPacket;
	PNDIS_BUFFER prNdisBuffer, prNextNdisBuffer;
	UINT u4PacketLen;
	UINT u4BytesToCopy;
	PVOID pvMbuf;
	UINT u4MbufLength;
	UINT u4BytesCopied;

	ASSERT(pvPacket);

	prNdisPacket = (PNDIS_PACKET) pvPacket;
	NdisQueryPacket(prNdisPacket, NULL, NULL, &prNdisBuffer, &u4PacketLen);

	u4BytesToCopy = u4PacketLen;
	u4BytesCopied = 0;

	while (u4BytesToCopy != 0) {

#ifdef NDIS51_MINIPORT
		NdisQueryBufferSafe(prNdisBuffer, &pvMbuf, &u4MbufLength, HighPagePriority);
#else
		NdisQueryBuffer(prNdisBuffer, &pvMbuf, &u4MbufLength);
#endif				/* NDIS51_MINIPORT */

		if (pvMbuf == (PVOID) NULL) {
			ASSERT(pvMbuf);
			break;
		}

		NdisMoveMemory((PVOID) pucDestBuffer, pvMbuf, u4MbufLength);

		u4BytesToCopy -= u4MbufLength;
		u4BytesCopied += u4MbufLength;
		pucDestBuffer += u4MbufLength;

		NdisGetNextBuffer(prNdisBuffer, &prNextNdisBuffer);
		prNdisBuffer = prNextNdisBuffer;
	}

	ASSERT(u4BytesCopied == u4PacketLen);

	return;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to allocate memory buffer for receiving frames.
*
* @param prGlueInfo Pointer of GLUE Data Structure
* @param u4Size     Size of the packet content in unit of byte
* @param ppucData   Pointer of buffer to be copied into
*
* @return Pointer to packet descriptor
*/
/*----------------------------------------------------------------------------*/
PVOID kalPacketAlloc(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Size, OUT PPUINT_8 ppucData)
{
	PNDIS_PACKET packet_p;

	packet_p = getPoolPacket(prGlueInfo);

	/* The mbuf descriptor will be created and chained to packet
	 * in function kalPacketPut().
	 */
	if (packet_p != NULL) {
		*ppucData =
		    *(PUCHAR *) &packet_p->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pvBuf)];
		NDIS_SET_PACKET_HEADER_SIZE(packet_p, 14);	/* Ether header size */
	}

	return packet_p;
}				/* kalPacketAlloc */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to free packet allocated from kalPacketAlloc.
*
* @param prGlueInfo     Pointer of GLUE Data Structure
* @param pvPacket       Pointer of the packet descriptor
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalPacketFree(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket)
{
	PNDIS_PACKET prNdisPacket;
	PNDIS_BUFFER prNdisBuf;


	ASSERT(prGlueInfo);
	ASSERT(pvPacket);

	prNdisPacket = (PNDIS_PACKET) pvPacket;

	do {
		NdisUnchainBufferAtBack(prNdisPacket, &prNdisBuf);

		if (prNdisBuf) {
			NdisFreeBuffer(prNdisBuf);
		} else {
			break;
		}

	} while (TRUE);

	/* Reinitialize the packet descriptor for reuse. */
	NdisReinitializePacket(prNdisPacket);

#if CETK_NDIS_PERFORMANCE_WORKAROUND
	{
		PUINT_32 ptr;
		ptr = (PUINT_32) prNdisPacket->ProtocolReserved;
		*ptr = 0;
	}
#endif

	putPoolPacket(prGlueInfo, prNdisPacket, NULL);

}				/* kalPacketFree */


/*----------------------------------------------------------------------------*/
/*!
* @brief Process the received packet for indicating to OS.
*
* @param prGlueInfo     Pointer to GLUE Data Structure
* @param pvPacket       Pointer of the packet descriptor
* @param pucPacketStart The starting address of the buffer of Rx packet.
* @param u4PacketLen    The packet length.
* @param pfgIsRetain    Is the packet to be retained.
* @param aerCSUM        The result of TCP/ IP checksum offload.
*
* @retval WLAN_STATUS_SUCCESS.
* @retval WLAN_STATUS_FAILURE.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
kalProcessRxPacket(IN P_GLUE_INFO_T prGlueInfo,
		   IN PVOID pvPacket,
		   IN PUINT_8 pucPacketStart,
		   IN UINT_32 u4PacketLen, IN BOOL fgIsRetain, IN ENUM_CSUM_RESULT_T aerCSUM[]
    )
{
	PNDIS_PACKET prNdisPacket;
	PNDIS_BUFFER prNdisBuf;
	NDIS_STATUS rStatus;


	ASSERT(prGlueInfo);
	ASSERT(pvPacket);
	ASSERT(pucPacketStart);

	prNdisPacket = (PNDIS_PACKET) pvPacket;

	NdisAllocateBuffer(&rStatus,
			   &prNdisBuf,
			   prGlueInfo->hBufPool, (PVOID) pucPacketStart, (UINT_32) u4PacketLen);

	if (rStatus != NDIS_STATUS_SUCCESS) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}

	NdisChainBufferAtBack(prNdisPacket, prNdisBuf);

	if (fgIsRetain) {
		/* We don't have enough receive buffers, so set the status
		   on the packet to NDIS_STATUS_RESOURCES to force the
		   protocol driver(s) to copy this packet and return this
		   buffer immediately after returning from the
		   NdisMIndicateReceivePacket function. */
		NDIS_SET_PACKET_STATUS(prNdisPacket, NDIS_STATUS_RESOURCES);
	} else {

		/* We have enough receive buffers, so set the status on the
		   packet to NDIS_STATUS_SUCCESS. */
		NDIS_SET_PACKET_STATUS(prNdisPacket, NDIS_STATUS_SUCCESS);
	}

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	kalUpdateRxCSUMOffloadParam(pvPacket, aerCSUM);
#endif

	return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief To indicate an array of received packets is available for higher
*        level protocol uses.
*
* @param prGlueInfo Pointer to GLUE Data Structure
* @param apvPkts    The packet array to be indicated
* @param ucPktNum   The number of packets to be indicated
*
* @retval WLAN_STATUS_SUCCESS: SUCCESS
* @retval WLAN_STATUS_FAILURE: FAILURE
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalRxIndicatePkts(IN P_GLUE_INFO_T prGlueInfo, IN PVOID apvPkts[], IN UINT_32 ucPktNum)
{
	NDIS_STATUS arStatus[CFG_RX_MAX_PKT_NUM];
	UINT_32 u4Idx;

	for (u4Idx = 0; u4Idx < ucPktNum; u4Idx++) {
		UINT_32 i, pivot;
		PVOID pvTmp;

		if (NDIS_GET_PACKET_STATUS((PNDIS_PACKET) apvPkts[u4Idx]) == NDIS_STATUS_RESOURCES) {
			pivot = u4Idx;
			for (i = u4Idx + 1; i < ucPktNum; i++) {
				if (NDIS_GET_PACKET_STATUS((PNDIS_PACKET) apvPkts[i]) !=
				    NDIS_STATUS_RESOURCES) {
					pvTmp = apvPkts[pivot];
					apvPkts[pivot] = apvPkts[i];
					apvPkts[i] = pvTmp;
					pivot++;
				}
			}
			break;
		}
	}

	for (u4Idx = 0; u4Idx < ucPktNum; u4Idx++) {
		arStatus[u4Idx] = NDIS_GET_PACKET_STATUS((PNDIS_PACKET) apvPkts[u4Idx]);
		if (arStatus[u4Idx] == NDIS_STATUS_SUCCESS) {
			/* 4 Increase the Pending Count before calling NdisMIndicateReceivePacket(). */
			InterlockedIncrement(&prGlueInfo->i4RxPendingFrameNum);
		}
	}

	NdisMIndicateReceivePacket(prGlueInfo->rMiniportAdapterHandle,
				   (PPNDIS_PACKET) apvPkts, (UINT) ucPktNum);

	for (u4Idx = 0; u4Idx < ucPktNum; u4Idx++) {

		/* 4 <1> Packets be retained. */
		if (arStatus[u4Idx] != NDIS_STATUS_SUCCESS) {
			PNDIS_PACKET prNdisPacket = (PNDIS_PACKET) apvPkts[u4Idx];
			PNDIS_BUFFER prNdisBuf = (PNDIS_BUFFER) NULL;

			ASSERT(prNdisPacket);

			NdisUnchainBufferAtBack(prNdisPacket, &prNdisBuf);

			if (prNdisBuf) {
				NdisFreeBuffer(prNdisBuf);
			}
#if DBG
			else {
				ASSERT(0);
			}
#endif				/* DBG */

			/* Reinitialize the packet descriptor for reuse. */
			NdisReinitializePacket(prNdisPacket);

#if CETK_NDIS_PERFORMANCE_WORKAROUND
			{
				PUINT_32 pu4Dummy;
				pu4Dummy = (PUINT_32) prNdisPacket->ProtocolReserved;
				*pu4Dummy = 0;
			}
#endif				/* CETK_NDIS_PERFORMANCE_WORKAROUND */

		}
	}

	return WLAN_STATUS_SUCCESS;
}				/* kalIndicatePackets */


/*----------------------------------------------------------------------------*/
/*!
* @brief Indicates changes in the status of a NIC to higher-level NDIS drivers.
*
* @param prGlueInfo Pointer to GLUE Data Structure
* @param eStatus    Specifies the WLAN_STATUS_XXX value that indicates the
*                       general change in status for the NIC.
* @param pvBuf      Pointer to a caller-allocated buffer containing data that
*                       is medium-specific and dependent on the value of eStatus.
* @param u4BufLen   Specifies the size in bytes of the buffer at StatusBuffer.
*
* @retval none
*
*/
/*----------------------------------------------------------------------------*/
VOID
kalIndicateStatusAndComplete(IN P_GLUE_INFO_T prGlueInfo,
			     IN WLAN_STATUS eStatus, IN PVOID pvBuf, IN UINT_32 u4BufLen)
{
	ASSERT(prGlueInfo);

	/* Indicate the protocol that the media state was changed. */
	NdisMIndicateStatus(prGlueInfo->rMiniportAdapterHandle,
			    (NDIS_STATUS) eStatus, (PVOID) pvBuf, u4BufLen);

	/* NOTE: have to indicate status complete every time you indicate status */
	NdisMIndicateStatusComplete(prGlueInfo->rMiniportAdapterHandle);

	if (eStatus == WLAN_STATUS_MEDIA_CONNECT || eStatus == WLAN_STATUS_MEDIA_DISCONNECT) {

		if (eStatus == WLAN_STATUS_MEDIA_CONNECT) {
			prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_CONNECTED;
		} else if (eStatus == WLAN_STATUS_MEDIA_DISCONNECT) {
			prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED;
		}

		if (wlanResetMediaStreamMode(prGlueInfo->prAdapter) == TRUE) {
			MEDIA_STREAMING_INDICATIONS_T rMediaStreamIndication;

			/* following MSDN for Media Streaming Indication */
			rMediaStreamIndication.StatusType = Ndis802_11StatusType_MediaStreamMode;
			rMediaStreamIndication.MediaStreamMode = Ndis802_11MediaStreamOff;

			NdisMIndicateStatus(prGlueInfo->rMiniportAdapterHandle,
					    (NDIS_STATUS) WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
					    (PVOID) &rMediaStreamIndication,
					    sizeof(MEDIA_STREAMING_INDICATIONS_T));
		}
	}
}				/* kalIndicateStatusAndComplete */


/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is called to update the (re)association
*        request information to the structure used to query and
*        set OID_802_11_ASSOCIATION_INFORMATION.
*
* @param prGlueInfo       Pointer to GLUE Data Structure
* @param pucFrameBody     Pointer to the frame body of the last (Re)Association
*                         Request frame from the AP.
* @param u4FrameBodyLen   The length of the frame body of the last
*                          (Re)Association Request frame.
* @param fgReassocRequest TRUE, if it is a Reassociation Request frame.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
kalUpdateReAssocReqInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody,
			IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest)
{
	PUINT_8 cp;
	PNDIS_802_11_ASSOCIATION_INFORMATION prNdisAssocInfo;

	if (fgReassocRequest) {
		ASSERT(u4FrameBodyLen >= MIN_REASSOC_REQ_BODY_LEN);

		if (u4FrameBodyLen < MIN_REASSOC_REQ_BODY_LEN) {
			return;
		}
	} else {
		ASSERT(u4FrameBodyLen >= MIN_ASSOC_REQ_BODY_LEN);

		if (u4FrameBodyLen < MIN_ASSOC_REQ_BODY_LEN) {
			return;
		}
	}

	prNdisAssocInfo = &prGlueInfo->rNdisAssocInfo;

	cp = pucFrameBody;

	/* Update the fixed information elements. */
	if (fgReassocRequest) {
		prNdisAssocInfo->AvailableRequestFixedIEs =
		    NDIS_802_11_AI_REQFI_CAPABILITIES |
		    NDIS_802_11_AI_REQFI_LISTENINTERVAL | NDIS_802_11_AI_REQFI_CURRENTAPADDRESS;
	} else {
		prNdisAssocInfo->AvailableRequestFixedIEs =
		    NDIS_802_11_AI_REQFI_CAPABILITIES | NDIS_802_11_AI_REQFI_LISTENINTERVAL;
	}

	kalMemCopy(&prNdisAssocInfo->RequestFixedIEs.Capabilities, cp, 2);
	cp += 2;
	u4FrameBodyLen -= 2;

	kalMemCopy(&prNdisAssocInfo->RequestFixedIEs.ListenInterval, cp, 2);
	cp += 2;
	u4FrameBodyLen -= 2;

	if (fgReassocRequest) {
		kalMemCopy(&prNdisAssocInfo->RequestFixedIEs.CurrentAPAddress, cp, 6);
		cp += 6;
		u4FrameBodyLen -= 6;
	} else {
		kalMemZero(&prNdisAssocInfo->RequestFixedIEs.CurrentAPAddress, 6);
	}

	/* Update the variable length information elements. */
	prNdisAssocInfo->RequestIELength = u4FrameBodyLen;
	prNdisAssocInfo->OffsetRequestIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);

	kalMemCopy(prGlueInfo->aucNdisAssocInfoIEs, cp, u4FrameBodyLen);

	/* Clear the information for the last association/reassociation response
	   from the AP. */
	prNdisAssocInfo->AvailableResponseFixedIEs = 0;
	prNdisAssocInfo->ResponseFixedIEs.Capabilities = 0;
	prNdisAssocInfo->ResponseFixedIEs.StatusCode = 0;
	prNdisAssocInfo->ResponseFixedIEs.AssociationId = 0;
	prNdisAssocInfo->ResponseIELength = 0;
	prNdisAssocInfo->OffsetResponseIEs =
	    sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + u4FrameBodyLen;

}				/* kalUpdateReAssocReqInfo */


/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is called to update the (re)association
*        response information to the structure used to query and
*        set OID_802_11_ASSOCIATION_INFORMATION.
*
* @param prGlueInfo      Pointer to adapter descriptor
* @param pucFrameBody    Pointer to the frame body of the last (Re)Association
*                         Response frame from the AP
* @param u4FrameBodyLen  The length of the frame body of the last
*                          (Re)Association Response frame
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
kalUpdateReAssocRspInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen)
{
	PUINT_8 cp;
	PNDIS_802_11_ASSOCIATION_INFORMATION prNdisAssocInfo;
	UINT_32 u4AvailableAssocRespIEBufLen;

	ASSERT(u4FrameBodyLen >= MIN_REASSOC_RESP_BODY_LEN);

	if (u4FrameBodyLen < MIN_REASSOC_RESP_BODY_LEN) {
		return;
	}

	prNdisAssocInfo = &prGlueInfo->rNdisAssocInfo;

	cp = pucFrameBody;

	/* Update the fixed information elements. */
	prNdisAssocInfo->AvailableResponseFixedIEs =
	    NDIS_802_11_AI_RESFI_CAPABILITIES |
	    NDIS_802_11_AI_RESFI_STATUSCODE | NDIS_802_11_AI_RESFI_ASSOCIATIONID;

	kalMemCopy(&prNdisAssocInfo->ResponseFixedIEs.Capabilities, cp, 2);
	cp += 2;

	kalMemCopy(&prNdisAssocInfo->ResponseFixedIEs.StatusCode, cp, 2);
	cp += 2;

	kalMemCopy(&prNdisAssocInfo->ResponseFixedIEs.AssociationId, cp, 2);
	cp += 2;

	u4FrameBodyLen -= 6;

	/* Update the variable length information elements. */
	u4AvailableAssocRespIEBufLen = (sizeof(prGlueInfo->aucNdisAssocInfoIEs) >
					prNdisAssocInfo->RequestIELength) ?
	    sizeof(prGlueInfo->aucNdisAssocInfoIEs) - prNdisAssocInfo->RequestIELength : 0;

	if (u4FrameBodyLen > u4AvailableAssocRespIEBufLen) {
		ASSERT(u4FrameBodyLen <= u4AvailableAssocRespIEBufLen);
		u4FrameBodyLen = u4AvailableAssocRespIEBufLen;
	}

	prNdisAssocInfo->ResponseIELength = u4FrameBodyLen;
	prNdisAssocInfo->OffsetResponseIEs =
	    sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + prNdisAssocInfo->RequestIELength;

	if (u4FrameBodyLen) {
#if BUILD_WMM
		UINT_32 len = 0;
		UINT_8 elemLen = 0, elemId = 0, wifiElemId = 221;
		UINT_8 wifiOuiWMM[] = { 0x00, 0x50, 0xF2, 0x02 };
#endif

		kalMemCopy(&prGlueInfo->aucNdisAssocInfoIEs[prNdisAssocInfo->RequestIELength],
			   cp, u4FrameBodyLen);

#if BUILD_WMM
		prGlueInfo->supportWMM = FALSE;
		while (len < u4FrameBodyLen) {
			elemId = *cp;
			elemLen = *(cp + 1);

			if (elemId == wifiElemId && (elemLen > sizeof(wifiOuiWMM))) {
				if (kalMemCmp(cp + 2, wifiOuiWMM, sizeof(wifiOuiWMM)) == 0) {
					prGlueInfo->supportWMM = TRUE;
#if DBG
					/* DbgPrint("WMM AP\n"); */
#endif
					break;
				}
			}

			len += elemLen + 2;
			cp += elemLen + 2;
		}
#endif
	}
}				/* kalUpdateReAssocRspInfo */


#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
* @brief To query the packet information for offload related parameters.
*
* @param  pvPacket Pointer to the packet descriptor.
* @param  pucFlag  Pointer to the offload related parameter.
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID kalQueryTxChksumOffloadParam(IN PVOID pvPacket, OUT PUINT_8 pucFlag)
{
	PNDIS_PACKET prPacket = (PNDIS_PACKET) pvPacket;
	NDIS_TCP_IP_CHECKSUM_PACKET_INFO rChecksumPktInfo;
	UINT_8 ucFlag = 0;

	if (NDIS_GET_PACKET_PROTOCOL_TYPE(prPacket) == NDIS_PROTOCOL_ID_TCP_IP) {

		rChecksumPktInfo.Value = (UINT_32)
		    NDIS_PER_PACKET_INFO_FROM_PACKET(prPacket, TcpIpChecksumPacketInfo);

		/* TODO: need to check NIC_CHECKSUM_OFFLOAD from glue_info ?? */
		if (rChecksumPktInfo.Transmit.NdisPacketChecksumV4 ||
		    rChecksumPktInfo.Transmit.NdisPacketChecksumV6) {

			/* only apply checksum offload for IPv4 packets */
			if (rChecksumPktInfo.Transmit.NdisPacketIpChecksum) {
				ucFlag |= TX_CS_IP_GEN;
			}

			if (rChecksumPktInfo.Transmit.NdisPacketTcpChecksum ||
			    rChecksumPktInfo.Transmit.NdisPacketUdpChecksum) {
				ucFlag |= TX_CS_TCP_UDP_GEN;
			}
		}
	}
	*pucFlag = ucFlag;

}				/* kalQueryChksumOffloadParam */


/*----------------------------------------------------------------------------*/
/*!
* @brief To update the checksum offload status to the packet to be indicated to OS.
*
* @param pvPacket Pointer to the packet descriptor.
* @param pucFlag  Points to the offload related parameter.
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID kalUpdateRxCSUMOffloadParam(IN PVOID pvPacket, IN ENUM_CSUM_RESULT_T aeCSUM[]
    )
{
	PNDIS_PACKET prPacket = (PNDIS_PACKET) pvPacket;
	NDIS_TCP_IP_CHECKSUM_PACKET_INFO rChecksumPktInfo;
	PNDIS_PACKET_EXTENSION prPktExt;

	prPktExt = NDIS_PACKET_EXTENSION_FROM_PACKET(prPacket);

	/* Initialize receive checksum value */
	rChecksumPktInfo.Value = 0;

	if (aeCSUM[CSUM_TYPE_IPV4] != CSUM_RES_NONE) {
		if (aeCSUM[CSUM_TYPE_IPV4] != CSUM_RES_SUCCESS) {
			rChecksumPktInfo.Receive.NdisPacketIpChecksumFailed = TRUE;
		} else {
			rChecksumPktInfo.Receive.NdisPacketIpChecksumSucceeded = TRUE;
		}
	} else if (aeCSUM[CSUM_TYPE_IPV6] != CSUM_RES_NONE) {
		if (aeCSUM[CSUM_TYPE_IPV6] != CSUM_RES_SUCCESS) {
			rChecksumPktInfo.Receive.NdisPacketIpChecksumFailed = TRUE;
		} else {
			rChecksumPktInfo.Receive.NdisPacketIpChecksumSucceeded = TRUE;
		}
	}

	if (aeCSUM[CSUM_TYPE_TCP] != CSUM_RES_NONE) {
		if (aeCSUM[CSUM_TYPE_TCP] != CSUM_RES_SUCCESS) {
			rChecksumPktInfo.Receive.NdisPacketTcpChecksumFailed = TRUE;
		} else {
			rChecksumPktInfo.Receive.NdisPacketTcpChecksumSucceeded = TRUE;
		}
	} else if (aeCSUM[CSUM_TYPE_UDP] != CSUM_RES_NONE) {
		if (aeCSUM[CSUM_TYPE_UDP] != CSUM_RES_SUCCESS) {
			rChecksumPktInfo.Receive.NdisPacketUdpChecksumFailed = TRUE;
		} else {
			rChecksumPktInfo.Receive.NdisPacketUdpChecksumSucceeded = TRUE;
		}
	}

	prPktExt->NdisPacketInfo[TcpIpChecksumPacketInfo] = (PVOID) rChecksumPktInfo.Value;

}
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */


/*----------------------------------------------------------------------------*/
/*!
* @brief Notify OS with SendComplete event of the specific packet. Linux should
*        free packets here.
*
* @param pvGlueInfo     Pointer of GLUE Data Structure
* @param pvPacket       Pointer of Packet Handle
* @param status         Status Code for OS upper layer
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalSendComplete(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN WLAN_STATUS rStatus)
{
	ASSERT(pvPacket);

	/* For WHQL test, indicate send packet successfully even
	 * Tx status is not OK
	 */
	NdisMSendComplete(prGlueInfo->rMiniportAdapterHandle,
			  (PNDIS_PACKET) pvPacket, NDIS_STATUS_SUCCESS);
	/* (NDIS_STATUS) rStatus); */

	GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Notify OS with SendComplete event of the specific packet. Linux should
*        free packets here.
*
* @param pvGlueInfo     Pointer of GLUE Data Structure
* @param pvPacket       Pointer of Packet Handle
* @param status         Status Code for OS upper layer
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID
kalSecurityFrameSendComplete(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN WLAN_STATUS rStatus)
{
	ASSERT(pvPacket);

	/* For WHQL test, indicate send packet successfully even
	 * Tx status is not OK
	 */
	NdisMSendComplete(prGlueInfo->rMiniportAdapterHandle,
			  (PNDIS_PACKET) pvPacket, NDIS_STATUS_SUCCESS);
	/* (NDIS_STATUS) rStatus); */

	GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Handle EVENT_ID_ASSOC_INFO event packet by indicating to OS with
*        proper information
*
* @param pvGlueInfo     Pointer of GLUE Data Structure
* @param prAssocInfo    Pointer of EVENT_ID_ASSOC_INFO Packet
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalHandleAssocInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_EVENT_ASSOC_INFO prAssocInfo)
{
	UINT_16 u2FrameBodyLen;

	ASSERT(prGlueInfo);

	u2FrameBodyLen = prAssocInfo->u2Length;

	if (prAssocInfo->ucAssocReq) {
		PUINT_8 cp;
		PNDIS_802_11_ASSOCIATION_INFORMATION prNdisAssocInfo;

#if 0
		if (prAssocInfo->ucReassoc) {
			ASSERT(u2FrameBodyLen >= MIN_REASSOC_REQ_BODY_LEN);

			if (u2FrameBodyLen < MIN_REASSOC_REQ_BODY_LEN) {
				return;
			}
		} else {
			ASSERT(u2FrameBodyLen >= MIN_ASSOC_REQ_BODY_LEN);

			if (u2FrameBodyLen < MIN_ASSOC_REQ_BODY_LEN) {
				return;
			}
		}
#endif
		prNdisAssocInfo = &prGlueInfo->rNdisAssocInfo;

		cp = (PUINT_8) &prAssocInfo->pucIe;

		/* Update the fixed information elements. */
		if (prAssocInfo->ucReassoc) {
			prNdisAssocInfo->AvailableRequestFixedIEs =
			    NDIS_802_11_AI_REQFI_CAPABILITIES |
			    NDIS_802_11_AI_REQFI_LISTENINTERVAL |
			    NDIS_802_11_AI_REQFI_CURRENTAPADDRESS;
		} else {
			prNdisAssocInfo->AvailableRequestFixedIEs =
			    NDIS_802_11_AI_REQFI_CAPABILITIES | NDIS_802_11_AI_REQFI_LISTENINTERVAL;
		}
		kalMemCopy(&prNdisAssocInfo->RequestFixedIEs.Capabilities, cp, 2);
		cp += 2;
		u2FrameBodyLen -= 2;

		kalMemCopy(&prNdisAssocInfo->RequestFixedIEs.ListenInterval, cp, 2);
		cp += 2;
		u2FrameBodyLen -= 2;
		if (prAssocInfo->ucReassoc) {
			kalMemCopy(&prNdisAssocInfo->RequestFixedIEs.CurrentAPAddress, cp, 6);
			cp += 6;
			u2FrameBodyLen -= 6;
		} else {
			kalMemZero(&prNdisAssocInfo->RequestFixedIEs.CurrentAPAddress, 6);
		}

		/* Update the variable length information elements. */
		prNdisAssocInfo->RequestIELength = u2FrameBodyLen;
		prNdisAssocInfo->OffsetRequestIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);

		kalMemCopy(prGlueInfo->aucNdisAssocInfoIEs, cp, u2FrameBodyLen);

		/* Clear the information for the last association/reassociation response
		 * from the AP. */
		prNdisAssocInfo->AvailableResponseFixedIEs = 0;
		prNdisAssocInfo->ResponseFixedIEs.Capabilities = 0;
		prNdisAssocInfo->ResponseFixedIEs.StatusCode = 0;
		prNdisAssocInfo->ResponseFixedIEs.AssociationId = 0;
		prNdisAssocInfo->ResponseIELength = 0;
		prNdisAssocInfo->OffsetResponseIEs =
		    sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + u2FrameBodyLen;

	} else {

		PUINT_8 cp;
		PNDIS_802_11_ASSOCIATION_INFORMATION prNdisAssocInfo;
		UINT_16 u2AvailableAssocRespIEBufLen;

#if 0
		ASSERT(u2FrameBodyLen >= MIN_REASSOC_RESP_BODY_LEN);

		if (u2FrameBodyLen < MIN_REASSOC_RESP_BODY_LEN) {
			return;
		}
#endif
		prNdisAssocInfo = &prGlueInfo->rNdisAssocInfo;

		cp = (PUINT_8) &prAssocInfo->pucIe;

		/* Update the fixed information elements. */
		prNdisAssocInfo->AvailableResponseFixedIEs =
		    NDIS_802_11_AI_RESFI_CAPABILITIES |
		    NDIS_802_11_AI_RESFI_STATUSCODE | NDIS_802_11_AI_RESFI_ASSOCIATIONID;

		kalMemCopy(&prNdisAssocInfo->ResponseFixedIEs.Capabilities, cp, 2);
		cp += 2;

		kalMemCopy(&prNdisAssocInfo->ResponseFixedIEs.StatusCode, cp, 2);
		cp += 2;

		kalMemCopy(&prNdisAssocInfo->ResponseFixedIEs.AssociationId, cp, 2);
		cp += 2;

		u2FrameBodyLen -= 6;

		/* Update the variable length information elements. */
		u2AvailableAssocRespIEBufLen = (sizeof(prGlueInfo->aucNdisAssocInfoIEs) >
						prNdisAssocInfo->RequestIELength) ?
		    (UINT_16) (sizeof(prGlueInfo->aucNdisAssocInfoIEs) -
			       prNdisAssocInfo->RequestIELength) : 0;

		if (u2FrameBodyLen > u2AvailableAssocRespIEBufLen) {
			ASSERT(u2FrameBodyLen <= u2AvailableAssocRespIEBufLen);
			u2FrameBodyLen = u2AvailableAssocRespIEBufLen;
		}

		prNdisAssocInfo->ResponseIELength = u2FrameBodyLen;
		prNdisAssocInfo->OffsetResponseIEs =
		    sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + prNdisAssocInfo->RequestIELength;

		if (u2FrameBodyLen) {
			kalMemCopy(&prGlueInfo->
				   aucNdisAssocInfoIEs[prNdisAssocInfo->RequestIELength], cp,
				   u2FrameBodyLen);
		}
	}
}
