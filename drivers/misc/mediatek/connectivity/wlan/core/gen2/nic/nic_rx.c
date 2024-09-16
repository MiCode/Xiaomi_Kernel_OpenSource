/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#ifndef LINUX
#include <limits.h>
#else
#include <linux/limits.h>
#endif

#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"

#include <linux/ktime.h>
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>
#include "gl_cfg80211.h"
#include "gl_vendor.h"
#include "wnm.h"
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define RX_RESPONSE_TIMEOUT (1000)

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

#if CFG_MGMT_FRAME_HANDLING
static PROCESS_RX_MGT_FUNCTION apfnProcessRxMgtFrame[MAX_NUM_OF_FC_SUBTYPES] = {
#if CFG_SUPPORT_AAA
	aaaFsmRunEventRxAssoc,	/* subtype 0000: Association request */
#else
	NULL,			/* subtype 0000: Association request */
#endif /* CFG_SUPPORT_AAA */
	saaFsmRunEventRxAssoc,	/* subtype 0001: Association response */
#if CFG_SUPPORT_AAA
	aaaFsmRunEventRxAssoc,	/* subtype 0010: Reassociation request */
#else
	NULL,			/* subtype 0010: Reassociation request */
#endif /* CFG_SUPPORT_AAA */
	saaFsmRunEventRxAssoc,	/* subtype 0011: Reassociation response */
#if (CFG_SUPPORT_ADHOC) || (CFG_SUPPORT_AAA)
	bssProcessProbeRequest,	/* subtype 0100: Probe request */
#else
	NULL,			/* subtype 0100: Probe request */
#endif /* CFG_SUPPORT_ADHOC */
	scanProcessBeaconAndProbeResp,	/* subtype 0101: Probe response */
	NULL,			/* subtype 0110: reserved */
	NULL,			/* subtype 0111: reserved */
	scanProcessBeaconAndProbeResp,	/* subtype 1000: Beacon */
	NULL,			/* subtype 1001: ATIM */
	saaFsmRunEventRxDisassoc,	/* subtype 1010: Disassociation */
	authCheckRxAuthFrameTransSeq,	/* subtype 1011: Authentication */
	saaFsmRunEventRxDeauth,	/* subtype 1100: Deauthentication */
	nicRxProcessActionFrame,	/* subtype 1101: Action */
	NULL,			/* subtype 1110: reserved */
	NULL			/* subtype 1111: reserved */
};
#endif

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
* @brief Initialize the RFBs
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxInitialize(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucMemHandle;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	UINT_32 i;
	UINT_16 failCounter = 0;

	DEBUGFUNC("nicRxInitialize");

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;

	/* 4 <0> Clear allocated memory. */
	kalMemZero((PVOID) prRxCtrl->pucRxCached, prRxCtrl->u4RxCachedSize);

	/* 4 <1> Initialize the RFB lists */
	QUEUE_INITIALIZE(&prRxCtrl->rFreeSwRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rReceivedRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rIndicatedRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rUnInitializedRfbList);

#if CFG_SUPPORT_MULTITHREAD
	QUEUE_INITIALIZE(&prRxCtrl->rRxDataRfbList);
#endif

	pucMemHandle = prRxCtrl->pucRxCached;
	for (i = CFG_RX_MAX_PKT_NUM; i != 0; i--) {
		prSwRfb = (P_SW_RFB_T) pucMemHandle;

		/* TODO: have an error handling mechanism when failing packet allocate */
		if (nicRxSetupRFB(prAdapter, prSwRfb) != WLAN_STATUS_SUCCESS)
			failCounter++;
		nicRxReturnRFB(prAdapter, prSwRfb);

		pucMemHandle += ALIGN_4(sizeof(SW_RFB_T));
	}
	if (failCounter > 0)
		DBGLOG(RX, ERROR, "nicRxSetupRFB allocate (%d) packets failed\n", failCounter);

	ASSERT(prRxCtrl->rFreeSwRfbList.u4NumElem == CFG_RX_MAX_PKT_NUM);
	/* Check if the memory allocation consist with this initialization function */
	ASSERT((ULONG) (pucMemHandle - prRxCtrl->pucRxCached) == prRxCtrl->u4RxCachedSize);

	/* 4 <2> Clear all RX counters */
	RX_RESET_ALL_CNTS(prRxCtrl);

#if CFG_SDIO_RX_AGG
	prRxCtrl->pucRxCoalescingBufPtr = prAdapter->pucCoalescingBufCached;
	HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, CFG_SDIO_MAX_RX_AGG_NUM);
#else
	HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, 1);
#endif

#if CFG_HIF_STATISTICS
	prRxCtrl->u4TotalRxAccessNum = 0;
	prRxCtrl->u4TotalRxPacketNum = 0;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
	prRxCtrl->u4QueuedCnt = 0;
	prRxCtrl->u4DequeuedCnt = 0;
#endif

}				/* end of nicRxInitialize() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Uninitialize the RFBs
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxUninitialize(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	nicRxFlush(prAdapter);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rReceivedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
			prSwRfb->pucRecvBuff = NULL;
		} else {
			break;
		}
	} while (TRUE);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
			prSwRfb->pucRecvBuff = NULL;
		} else {
			break;
		}
	} while (TRUE);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rUnInitializedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
			prSwRfb->pucRecvBuff = NULL;
		} else {
			break;
		}
	} while (TRUE);

#if CFG_SUPPORT_MULTITHREAD
	do {

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rRxDataRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
			prSwRfb->pucRecvBuff = NULL;
		} else {
			break;
		}
	} while (TRUE);
#endif

}				/* end of nicRxUninitialize() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Fill RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb   specify the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxFillRFB(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_HIF_RX_HEADER_T prHifRxHdr;

	UINT_32 u4PktLen = 0;
	UINT_32 u4MacHeaderLen;
	UINT_32 u4HeaderOffset;

	DEBUGFUNC("nicRxFillRFB");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(prHifRxHdr);

	u4PktLen = prHifRxHdr->u2PacketLen;

	u4HeaderOffset = (UINT_32) (prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_OFFSET_MASK);
	u4MacHeaderLen = (UINT_32) (prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_LEN)
	    >> HIF_RX_HDR_HEADER_LEN_OFFSET;

	/* DBGLOG(RX, TRACE, ("u4HeaderOffset = %d, u4MacHeaderLen = %d\n", */
	/* u4HeaderOffset, u4MacHeaderLen)); */

	prSwRfb->u2HeaderLen = (UINT_16) u4MacHeaderLen;
	prSwRfb->pvHeader = (PUINT_8) prHifRxHdr + HIF_RX_HDR_SIZE + u4HeaderOffset;
	prSwRfb->u2PacketLen = (UINT_16) (u4PktLen - (HIF_RX_HDR_SIZE + u4HeaderOffset));

	/* DBGLOG(RX, TRACE, ("Dump Rx packet, u2PacketLen = %d\n", prSwRfb->u2PacketLen)); */
	/* DBGLOG_MEM8(RX, TRACE, prSwRfb->pvHeader, prSwRfb->u2PacketLen); */

#if 0
	if (prHifRxHdr->ucReorder & HIF_RX_HDR_80211_HEADER_FORMAT) {
		prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_802_11_FORMAT;
		DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_802_11_FORMAT\n");
	}

	if (prHifRxHdr->ucReorder & HIF_RX_HDR_DO_REORDER) {
		prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_DO_REORDERING;
		DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_DO_REORDERING\n");

		/* Get Seq. No and TID, Wlan Index info */
		if (prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_BAR_FRAME) {
			prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_BAR_FRAME;
			DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_BAR_FRAME\n");
		}

		prSwRfb->u2SSN = prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_SEQ_NO_MASK;
		prSwRfb->ucTid = (UINT_8) ((prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_TID_MASK)
					   >> HIF_RX_HDR_TID_OFFSET);
		DBGLOG(RX, TRACE, "u2SSN = %d, ucTid = %d\n", prSwRfb->u2SSN, prSwRfb->ucTid);
	}

	if (prHifRxHdr->ucReorder & HIF_RX_HDR_WDS) {
		prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_AMP_WDS;
		DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_AMP_WDS\n");
	}
#endif
}

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
/*----------------------------------------------------------------------------*/
/*!
* @brief Fill checksum status in RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
* @param u4TcpUdpIpCksStatus specify the Checksum status
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxFillChksumStatus(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb, IN UINT_32 u4TcpUdpIpCksStatus)
{

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (prAdapter->u4CSUMFlags != CSUM_NOT_SUPPORTED) {
		if (u4TcpUdpIpCksStatus & RX_CS_TYPE_IPv4) {	/* IPv4 packet */
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
			if (u4TcpUdpIpCksStatus & RX_CS_STATUS_IP) {	/* IP packet csum failed */
				prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_FAILED;
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_SUCCESS;
			}

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {	/* TCP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_TCP) {	/* TCP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_SUCCESS;
				}
			} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_UDP) {	/* UDP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_UDP) {	/* UDP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_SUCCESS;
				}
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
			}
		} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_IPv6) {	/* IPv6 packet */
			prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_SUCCESS;

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {	/* TCP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_TCP) {	/* TCP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_SUCCESS;
				}
			} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_UDP) {	/* UDP packet */
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_UDP) {	/* UDP packet csum failed */
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_SUCCESS;
				}
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
			}
		} else {
			prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
		}
	}

}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process packet doesn't need to do buffer reordering
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessPktWithoutReorder(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_TX_CTRL_T prTxCtrl;
	BOOLEAN fgIsRetained = FALSE;
	UINT_32 u4CurrentRxBufferCount;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	BOOLEAN fgIsUninitRfb = FALSE;

	DEBUGFUNC("nicRxProcessPktWithoutReorder");
	/* DBGLOG(RX, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	u4CurrentRxBufferCount = prRxCtrl->rFreeSwRfbList.u4NumElem;
	/* QM USED = $A, AVAILABLE COUNT = $B, INDICATED TO OS = $C
	 * TOTAL = $A + $B + $C
	 *
	 * Case #1 (Retain)
	 * -------------------------------------------------------
	 * $A + $B < THRESHOLD := $A + $B + $C < THRESHOLD + $C := $TOTAL - THRESHOLD < $C
	 * => $C used too much, retain
	 *
	 * Case #2 (Non-Retain)
	 * -------------------------------------------------------
	 * $A + $B > THRESHOLD := $A + $B + $C > THRESHOLD + $C := $TOTAL - THRESHOLD > $C
	 * => still available for $C to use
	 *
	 */
	fgIsRetained = (((u4CurrentRxBufferCount +
			  qmGetRxReorderQueuedBufferCount(prAdapter) +
			  prTxCtrl->i4PendingFwdFrameCount) < CFG_RX_RETAINED_PKT_THRESHOLD) ? TRUE : FALSE);

	/* DBGLOG(RX, INFO, ("fgIsRetained = %d\n", fgIsRetained)); */

	if (kalProcessRxPacket(prAdapter->prGlueInfo,
			       prSwRfb->pvPacket,
			       prSwRfb->pvHeader,
			       (UINT_32) prSwRfb->u2PacketLen, fgIsRetained, prSwRfb->aeCSUM) != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, ERROR, "kalProcessRxPacket return value != WLAN_STATUS_SUCCESS\n");
		ASSERT(0);

		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

		if (prStaRec) {
#if CFG_ENABLE_WIFI_DIRECT
			if (prStaRec->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX && prAdapter->fgIsP2PRegistered == TRUE)
				GLUE_SET_PKT_FLAG_P2P(prSwRfb->pvPacket);
#endif
#if CFG_ENABLE_BT_OVER_WIFI
			if (prStaRec->ucNetTypeIndex == NETWORK_TYPE_BOW_INDEX)
				GLUE_SET_PKT_FLAG_PAL(prSwRfb->pvPacket);
#endif

			/* record the count to pass to os */
			STATS_RX_PASS2OS_INC(prStaRec, prSwRfb);
		}
		prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] = prSwRfb->pvPacket;
		prRxCtrl->ucNumIndPacket++;
		wlanPktStatusDebugTraceInfoSeq(prAdapter, prSwRfb->prHifRxHdr->u2SeqNoTid);

	if (fgIsRetained) {
		prRxCtrl->apvRetainedPacket[prRxCtrl->ucNumRetainedPacket] = prSwRfb->pvPacket;
		prRxCtrl->ucNumRetainedPacket++;
		/* TODO : error handling of nicRxSetupRFB */
		if (nicRxSetupRFB(prAdapter, prSwRfb))
			fgIsUninitRfb = TRUE;
		nicRxReturnRFBwithUninit(prAdapter, prSwRfb, fgIsUninitRfb);
	} else {
		prSwRfb->pvPacket = NULL;
		prSwRfb->pucRecvBuff = NULL;
		nicRxReturnRFBwithUninit(prAdapter, prSwRfb, fgIsUninitRfb);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Process forwarding data packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessForwardPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_MSDU_INFO_T prMsduInfo, prRetMsduInfoList;
	P_TX_CTRL_T prTxCtrl;
	P_RX_CTRL_T prRxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessForwardPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	QUEUE_REMOVE_HEAD(&prTxCtrl->rFreeMsduInfoList, prMsduInfo, P_MSDU_INFO_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

	if (prMsduInfo && kalProcessRxPacket(prAdapter->prGlueInfo,
					     prSwRfb->pvPacket,
					     prSwRfb->pvHeader,
					     (UINT_32) prSwRfb->u2PacketLen,
					     prRxCtrl->rFreeSwRfbList.u4NumElem <
					     CFG_RX_RETAINED_PKT_THRESHOLD ? TRUE : FALSE,
					     prSwRfb->aeCSUM) == WLAN_STATUS_SUCCESS) {

		prMsduInfo->eSrc = TX_PACKET_FORWARDING;
		/* pack into MSDU_INFO_T */
		nicTxFillMsduInfo(prAdapter, prMsduInfo, (P_NATIVE_PACKET) (prSwRfb->pvPacket));
		/* Overwrite the ucNetworkType */
		prMsduInfo->ucNetworkType = HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr);

		/* release RX buffer (to rIndicatedRfbList) */
		prSwRfb->pvPacket = NULL;
		prSwRfb->pucRecvBuff = NULL;
		nicRxReturnRFB(prAdapter, prSwRfb);

		/* increase forward frame counter */
		GLUE_INC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);

		/* send into TX queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
		prRetMsduInfoList = qmEnqueueTxPackets(prAdapter, prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

		if (prRetMsduInfoList != NULL) {	/* TX queue refuses queuing the packet */
#if (CFG_SUPPORT_TDLS == 1)
			TdlsexForwardFrameTag((struct sk_buff *)prMsduInfo->prPacket, TRUE);
#endif
			nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfoList);
			nicTxReturnMsduInfo(prAdapter, prRetMsduInfoList);
		}
#if (CFG_SUPPORT_TDLS == 1)
		else
			TdlsexForwardFrameTag((struct sk_buff *)prMsduInfo->prPacket, FALSE);
#endif
		/* indicate service thread for sending */
		if (prTxCtrl->i4PendingFwdFrameCount > 0)
			kalSetEvent(prAdapter->prGlueInfo);
	} else {		/* no TX resource */
#if (CFG_SUPPORT_TDLS == 1)
		struct sk_buff *skb = (struct sk_buff *)prSwRfb->pvPacket;

		skb->data = prSwRfb->pvHeader;
		TdlsexForwardFrameTag(skb, TRUE);
#endif
		nicRxReturnRFB(prAdapter, prSwRfb);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Process broadcast data packet for both host and forwarding
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessGOBroadcastPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_SW_RFB_T prSwRfbDuplicated;
	P_TX_CTRL_T prTxCtrl;
	P_RX_CTRL_T prRxCtrl;
	P_HIF_RX_HEADER_T prHifRxHdr;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessGOBroadcastPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(prHifRxHdr);

	ASSERT(CFG_NUM_OF_QM_RX_PKT_NUM >= 16);

	if (prRxCtrl->rFreeSwRfbList.u4NumElem
	    >= (CFG_RX_MAX_PKT_NUM - (CFG_NUM_OF_QM_RX_PKT_NUM - 16 /* Reserved for others */))) {

		/* 1. Duplicate SW_RFB_T */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfbDuplicated, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (prSwRfbDuplicated) {
			kalMemCopy(prSwRfbDuplicated->pucRecvBuff,
				   prSwRfb->pucRecvBuff, ALIGN_4(prHifRxHdr->u2PacketLen + HIF_RX_HW_APPENDED_LEN));

			prSwRfbDuplicated->ucPacketType = HIF_RX_PKT_TYPE_DATA;
			prSwRfbDuplicated->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);
			nicRxFillRFB(prAdapter, prSwRfbDuplicated);

			/* 2. Modify eDst */
			prSwRfbDuplicated->eDst = RX_PKT_DESTINATION_FORWARD;

			/* 4. Forward */
			nicRxProcessForwardPkt(prAdapter, prSwRfbDuplicated);
		}
	} else {
		DBGLOG(RX, WARN, "Stop to forward BMC packet due to less free Sw Rfb %u\n",
				  prRxCtrl->rFreeSwRfbList.u4NumElem);
	}

	/* 3. Indicate to host */
	prSwRfb->eDst = RX_PKT_DESTINATION_HOST;
	nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Process HIF data packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessDataPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prRetSwRfb, prNextSwRfb;
	P_HIF_RX_HEADER_T prHifRxHdr;
	P_STA_RECORD_T prStaRec;
	UINT_16 u2Etype = 0;
	BOOLEAN fIsDummy = FALSE;
	BOOLEAN fgIsSkipClass3Chk = FALSE;

	DEBUGFUNC("nicRxProcessDataPacket");
	/* DBGLOG(RX, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prHifRxHdr = prSwRfb->prHifRxHdr;
	prRxCtrl = &prAdapter->rRxCtrl;

	fIsDummy = (prHifRxHdr->u2PacketLen >= 12) ? FALSE : TRUE;

	nicRxFillRFB(prAdapter, prSwRfb);

	if (prSwRfb->u2PacketLen > 14) {
		PUINT_8 pc = (PUINT_8)prSwRfb->pvHeader;

		u2Etype = (pc[ETH_TYPE_LEN_OFFSET] << 8) | (pc[ETH_TYPE_LEN_OFFSET + 1]);
	}

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	{
		UINT_32 u4TcpUdpIpCksStatus;

		u4TcpUdpIpCksStatus = *((PUINT_32) ((ULONG) prHifRxHdr + (UINT_32) (ALIGN_4(prHifRxHdr->u2PacketLen))));
		nicRxFillChksumStatus(prAdapter, prSwRfb, u4TcpUdpIpCksStatus);

	}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	prStaRec = cnmGetStaRecByIndex(prAdapter, prHifRxHdr->ucStaRecIdx);

	if ((u2Etype == ETH_P_1X) || (u2Etype == ETH_P_PRE_1X)) {
		if ((prStaRec != NULL) && (prStaRec->eAuthAssocState == SAA_STATE_WAIT_ASSOC2)) {
			DBGLOG(RX, INFO, "skip class 3 error:Type=%d,Len=%d\n", u2Etype, prSwRfb->u2PacketLen);
			fgIsSkipClass3Chk = TRUE;
		}
	}

	if ((fgIsSkipClass3Chk == TRUE) ||
		(secCheckClassError(prAdapter, prSwRfb, prStaRec) == TRUE && prAdapter->fgTestMode == FALSE)) {
#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4QueuedCnt++;
#endif
		prRetSwRfb = qmHandleRxPackets(prAdapter, prSwRfb);
		if (prRetSwRfb != NULL) {
			do {
				/* save next first */
				prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prRetSwRfb);
				if (fIsDummy == TRUE) {
					nicRxReturnRFB(prAdapter, prRetSwRfb);
					RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
					DBGLOG(RX, WARN, "Drop Dummy Packets");

				} else {
					switch (prRetSwRfb->eDst) {
					case RX_PKT_DESTINATION_HOST:
#if ARP_MONITER_ENABLE
					if (IS_STA_IN_AIS(prStaRec)) {
						qmHandleRxArpPackets(prAdapter, prRetSwRfb);
						qmHandleRxDhcpPackets(prAdapter, prRetSwRfb);
					}
#endif
						secHandleRxEapolPacket(prAdapter, prRetSwRfb, prStaRec);
						nicRxProcessPktWithoutReorder(prAdapter, prRetSwRfb);
						break;

					case RX_PKT_DESTINATION_FORWARD:
						nicRxProcessForwardPkt(prAdapter, prRetSwRfb);
						break;

					case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
						nicRxProcessGOBroadcastPkt(prAdapter, prRetSwRfb);
						break;

					case RX_PKT_DESTINATION_NULL:
						nicRxReturnRFB(prAdapter, prRetSwRfb);
						RX_INC_CNT(prRxCtrl, RX_DST_NULL_DROP_COUNT);
						RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
						break;

					default:
						break;
					}
				}
#if CFG_HIF_RX_STARVATION_WARNING
				prRxCtrl->u4DequeuedCnt++;
#endif
				prRetSwRfb = prNextSwRfb;
			} while (prRetSwRfb);
		}
	} else {
		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(prRxCtrl, RX_CLASS_ERR_DROP_COUNT);
		RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
	}
}

#if CFG_SUPPORT_GSCN
/*----------------------------------------------------------------------------*/
/*!
* @brief Process GSCAN event packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
UINT_8 nicRxProcessGSCNEvent(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_SCAN_INFO_T prScanInfo;
	P_WIFI_EVENT_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	struct wiphy *wiphy;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
	prGlueInfo = prAdapter->prGlueInfo;
	/* Push the data to the skb */
	wiphy = priv_to_wiphy(prGlueInfo);

	/* Event Handling */
	switch (prEvent->ucEID) {
	case EVENT_ID_GSCAN_SCAN_AVAILABLE:
		{
			P_EVENT_GSCAN_SCAN_AVAILABLE_T prEventGscnAvailable;

			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_SCAN_AVAILABLE\n");
			prEventGscnAvailable = (P_EVENT_GSCAN_SCAN_AVAILABLE_T) (prEvent->aucBuffer);
			mtk_cfg80211_vendor_event_scan_results_available(wiphy,
				prGlueInfo->prDevHandler->ieee80211_ptr, prEventGscnAvailable->u2Num);
		}
		break;

	case EVENT_ID_GSCAN_RESULT:
		{
			P_EVENT_GSCAN_RESULT_T prEventBuffer;

			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_RESULT 2\n");
			prEventBuffer = (P_EVENT_GSCAN_RESULT_T) (prEvent->aucBuffer);
			/* scnFsmGSCNResults(prAdapter, prEventBuffer); */
		}
		break;

	case EVENT_ID_GSCAN_CAPABILITY:
		{
			P_EVENT_GSCAN_CAPABILITY_T prEventGscnCapbiblity;

			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_CAPABILITY\n");
			prEventGscnCapbiblity = (P_EVENT_GSCAN_CAPABILITY_T) (prEvent->aucBuffer);
			mtk_cfg80211_vendor_get_gscan_capabilities(wiphy, prGlueInfo->prDevHandler->ieee80211_ptr,
				prEventGscnCapbiblity, sizeof(EVENT_GSCAN_CAPABILITY_T));
		}
		break;

	case EVENT_ID_GSCAN_SCAN_COMPLETE:
		{
			P_EVENT_GSCAN_SCAN_COMPLETE_T prEventGscnScnDone;

			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_SCAN_COMPLETE\n");
			prEventGscnScnDone = (P_EVENT_GSCAN_SCAN_COMPLETE_T) (prEvent->aucBuffer);
			mtk_cfg80211_vendor_event_complete_scan(wiphy,
				prGlueInfo->prDevHandler->ieee80211_ptr, prEventGscnScnDone->ucScanState);
		}
		break;

	case EVENT_ID_GSCAN_FULL_RESULT:
		{
			UINT_32 ie_len = 0;
			P_EVENT_GSCAN_FULL_RESULT_T prEventGscnFullResult;
			/* P_PARAM_WIFI_GSCAN_FULL_RESULT prParamGscnFullResult; */

			DBGLOG(SCN, TRACE, "EVENT_ID_GSCAN_FULL_RESULT\n");

			prEventGscnFullResult = (P_EVENT_GSCAN_FULL_RESULT_T)(prEvent->aucBuffer);
			ie_len = min(prEventGscnFullResult->u4IeLength, (UINT_32)CFG_IE_BUFFER_SIZE);

			DBGLOG(SCN, LOUD, "arSsid=%s, bssid="MACSTR", u4Channel=%d u4IeLength=%d\n",
				prEventGscnFullResult->rResult.arSsid,
				MAC2STR(prEventGscnFullResult->rResult.arMacAddr),
				prEventGscnFullResult->rResult.u4Channel, prEventGscnFullResult->u4IeLength);

			kalMemZero(prScanInfo->prGscnFullResult,
				offsetof(PARAM_WIFI_GSCAN_FULL_RESULT, ie_data) + CFG_IE_BUFFER_SIZE);
			/* WIFI_GSCAN_RESULT_T similar to PARAM_WIFI_GSCAN_RESULT*/
			kalMemCopy(&prScanInfo->prGscnFullResult->fixed, &prEventGscnFullResult->rResult,
				sizeof(WIFI_GSCAN_RESULT_T));
			prScanInfo->prGscnFullResult->u4BucketMask = prEventGscnFullResult->u4BucketMask;
			prScanInfo->prGscnFullResult->ie_length = prEventGscnFullResult->u4IeLength;
			kalMemCopy(prScanInfo->prGscnFullResult->ie_data, prEventGscnFullResult->ucIeData, ie_len);

			mtk_cfg80211_vendor_event_full_scan_results(wiphy,
					prGlueInfo->prDevHandler->ieee80211_ptr,
					prScanInfo->prGscnFullResult,
					offsetof(PARAM_WIFI_GSCAN_FULL_RESULT, ie_data) + ie_len);
		}
		break;

	case EVENT_ID_GSCAN_SIGNIFICANT_CHANGE:
		{
			P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T prEventGscnSignificantChange;

			prEventGscnSignificantChange = (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer);
			memcpy(prEventGscnSignificantChange, (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_SIGNIFICANT_CHANGE_T));
		}
		break;

	case EVENT_ID_GSCAN_GEOFENCE_FOUND:
		{
			P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T prEventGscnGeofenceFound;

			prEventGscnGeofenceFound = (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer);
			memcpy(prEventGscnGeofenceFound, (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_SIGNIFICANT_CHANGE_T));
		}
		break;

	default:
		DBGLOG(SCN, ERROR, "not a GSCN event\n");
		break;
	}

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief Process HIF event packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessEventPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_CMD_INFO_T prCmdInfo;
	P_MSDU_INFO_T prMsduInfo;
	P_WIFI_EVENT_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	struct wiphy *wiphy;

	DEBUGFUNC("nicRxProcessEventPacket");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
	prGlueInfo = prAdapter->prGlueInfo;
	wiphy = priv_to_wiphy(prGlueInfo);

	DBGLOG(RX, EVENT, "prEvent->ucEID = 0x%02x\n", prEvent->ucEID);
	/* Event Handling */
	switch (prEvent->ucEID) {
	case EVENT_ID_CMD_RESULT:
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			P_EVENT_CMD_RESULT prCmdResult;

			prCmdResult = (P_EVENT_CMD_RESULT) ((PUINT_8) prEvent + EVENT_HDR_SIZE);

			prCmdInfo->u4FwResponseTime = kalGetTimeTick();
			wlanDebugCommandRecodTime(prCmdInfo);

			/* CMD_RESULT should be only in response to Set commands */
			ASSERT(prCmdInfo->fgSetQuery == FALSE || prCmdInfo->fgNeedResp == TRUE);

			if (prCmdResult->ucStatus == 0) {	/* success */
				if (prCmdInfo->pfCmdDoneHandler) {
					prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
				} else if (prCmdInfo->fgIsOid == TRUE) {
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
						       WLAN_STATUS_SUCCESS);
				}
			} else if (prCmdResult->ucStatus == 1) {	/* reject */
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
						       WLAN_STATUS_FAILURE);
			} else if (prCmdResult->ucStatus == 2) {	/* unknown CMD */
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
						       WLAN_STATUS_NOT_SUPPORTED);
			}
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else
			DBGLOG(RX, WARN, "prCmdInfo is null ,ucEID = 0x%02x ucSeqNum = 0x%02x\n"
			, prEvent->ucEID, prEvent->ucSeqNum);

		break;

#if 0
	case EVENT_ID_CONNECTION_STATUS:
		/* OBSELETE */
		{
			P_EVENT_CONNECTION_STATUS prConnectionStatus;

			prConnectionStatus = (P_EVENT_CONNECTION_STATUS) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_CONNECTION_STATUS = %d\n", prConnectionStatus->ucMediaStatus);
			if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_DISCONNECTED) {
				/* disconnected */
				if (kalGetMediaStateIndicated(prGlueInfo) != PARAM_MEDIA_STATE_DISCONNECTED) {

					kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);

					prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();
				}
			} else if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_CONNECTED) {
				/* connected */
				prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();

				/* fill information for association result */
				prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen = prConnectionStatus->ucSsidLen;
				kalMemCopy(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
					   prConnectionStatus->aucSsid, prConnectionStatus->ucSsidLen);

				kalMemCopy(prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
					prConnectionStatus->aucBssid, MAC_ADDR_LEN);

				/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.u4Privacy = prConnectionStatus->ucEncryptStatus;
				prAdapter->rWlanInfo.rCurrBssId.rRssi = 0;	/* @FIXME */
				/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.eNetworkTypeInUse = PARAM_NETWORK_TYPE_AUTOMODE;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4BeaconPeriod
					= prConnectionStatus->u2BeaconPeriod;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4ATIMWindow
					= prConnectionStatus->u2ATIMWindow;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4DSConfig
					= prConnectionStatus->u4FreqInKHz;
				prAdapter->rWlanInfo.ucNetworkType = prConnectionStatus->ucNetworkType;

				switch (prConnectionStatus->ucInfraMode) {
				case 0:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_IBSS;
					break;
				case 1:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_INFRA;
					break;
				case 2:
				default:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_AUTO_SWITCH;
					break;
				}
				/* always indicate to OS according to MSDN (re-association/roaming) */
				kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_CONNECT, NULL, 0);
			}
		}
		break;

	case EVENT_ID_SCAN_RESULT:
		/* OBSELETE */
		break;
#endif

	case EVENT_ID_RX_ADDBA:
		/* The FW indicates that an RX BA agreement will be established */
		qmHandleEventRxAddBa(prAdapter, prEvent);
		break;

	case EVENT_ID_RX_DELBA:
		/* The FW indicates that an RX BA agreement has been deleted */
		qmHandleEventRxDelBa(prAdapter, prEvent);
		break;

#if CFG_RX_BA_REORDERING_ENHANCEMENT
	case EVENT_ID_BA_FW_DROP_SN:
		qmHandleEventDropByFW(prAdapter, prEvent);
		break;
#endif

	case EVENT_ID_LINK_QUALITY:
#if CFG_ENABLE_WIFI_DIRECT && CFG_SUPPORT_P2P_RSSI_QUERY
		if (prEvent->u2PacketLen == EVENT_HDR_SIZE + sizeof(EVENT_LINK_QUALITY_EX)) {
			P_EVENT_LINK_QUALITY_EX prLqEx = (P_EVENT_LINK_QUALITY_EX) (prEvent->aucBuffer);

			if (prLqEx->ucIsLQ0Rdy)
				nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_AIS_INDEX, (P_EVENT_LINK_QUALITY) prLqEx);
			if (prLqEx->ucIsLQ1Rdy)
				nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_P2P_INDEX, (P_EVENT_LINK_QUALITY) prLqEx);
		} else {
			/* For old FW, P2P may invoke link quality query, and make driver flag becone TRUE. */
			DBGLOG(P2P, WARN, "Old FW version, not support P2P RSSI query.\n");

			/* Must not use NETWORK_TYPE_P2P_INDEX, cause the structure is mismatch. */
			nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_AIS_INDEX,
					     (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
		}
#else
		nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_AIS_INDEX, (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
#endif

		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			prCmdInfo->u4FwResponseTime = kalGetTimeTick();
			wlanDebugCommandRecodTime(prCmdInfo);
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}
#ifndef LINUX
		if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_GREATER &&
		    prAdapter->rWlanInfo.rRssiTriggerValue >= (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
			prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

			kalIndicateStatusAndComplete(prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID)&(prAdapter->rWlanInfo.rRssiTriggerValue),
						     sizeof(PARAM_RSSI));
		} else if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_LESS
			   && prAdapter->rWlanInfo.rRssiTriggerValue <= (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
			prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

			kalIndicateStatusAndComplete(prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID)&(prAdapter->rWlanInfo.rRssiTriggerValue),
						     sizeof(PARAM_RSSI));
		}
#endif

		break;

	case EVENT_ID_MIC_ERR_INFO:
		{
			P_EVENT_MIC_ERR_INFO prMicError;
			/* P_PARAM_AUTH_EVENT_T prAuthEvent; */
			P_STA_RECORD_T prStaRec;

			DBGLOG(RSN, EVENT, "EVENT_ID_MIC_ERR_INFO\n");

			prMicError = (P_EVENT_MIC_ERR_INFO) (prEvent->aucBuffer);
			prStaRec = cnmGetStaRecByAddress(prAdapter,
							 (UINT_8) NETWORK_TYPE_AIS_INDEX,
							 prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
			ASSERT(prStaRec);

			if (prStaRec)
				rsnTkipHandleMICFailure(prAdapter, prStaRec, (BOOLEAN) prMicError->u4Flags);
			else
				DBGLOG(RSN, WARN, "No STA rec!!\n");
#if 0
			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

			/* Status type: Authentication Event */
			prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

			/* Authentication request */
			prAuthEvent->arRequest[0].u4Length = sizeof(PARAM_AUTH_REQUEST_T);
			kalMemCopy((PVOID) prAuthEvent->arRequest[0].arBssid,
				(PVOID) prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
				/* whsu:Todo? */PARAM_MAC_ADDR_LEN);

			if (prMicError->u4Flags != 0)
				prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
			else
				prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) prAuthEvent,
						     sizeof(PARAM_STATUS_INDICATION_T) + sizeof(PARAM_AUTH_REQUEST_T));
#endif
		}
		break;

	case EVENT_ID_ASSOC_INFO:
		{
			P_EVENT_ASSOC_INFO prAssocInfo;

			prAssocInfo = (P_EVENT_ASSOC_INFO) (prEvent->aucBuffer);

			kalHandleAssocInfo(prAdapter->prGlueInfo, prAssocInfo);
		}
		break;
#if (CFG_REFACTORY_PMKSA == 0)
	case EVENT_ID_802_11_PMKID:
		{
			P_PARAM_AUTH_EVENT_T prAuthEvent;
			PUINT_8 cp;
			UINT_32 u4LenOfUsedBuffer;

			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

			prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;

			u4LenOfUsedBuffer = (UINT_32) (prEvent->u2PacketLen - 8);

			prAuthEvent->arRequest[0].u4Length = u4LenOfUsedBuffer;

			cp = (PUINT_8) &prAuthEvent->arRequest[0];

			/* Status type: PMKID Candidatelist Event */
			kalMemCopy(cp, (P_EVENT_PMKID_CANDIDATE_LIST_T) (prEvent->aucBuffer), prEvent->u2PacketLen - 8);

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) prAuthEvent,
						     sizeof(PARAM_STATUS_INDICATION_T) + u4LenOfUsedBuffer);
		}
		break;
#endif

#if 0
	case EVENT_ID_ACTIVATE_STA_REC_T:
		{
			P_EVENT_ACTIVATE_STA_REC_T prActivateStaRec;

			prActivateStaRec = (P_EVENT_ACTIVATE_STA_REC_T) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_ACTIVATE_STA_REC_T Index:%d, MAC:[%pM]\n",
				 prActivateStaRec->ucStaRecIdx, prActivateStaRec->aucMacAddr);

			qmActivateStaRec(prAdapter,
					 (UINT_32) prActivateStaRec->ucStaRecIdx,
					 ((prActivateStaRec->fgIsQoS) ? TRUE : FALSE),
					 prActivateStaRec->ucNetworkTypeIndex,
					 ((prActivateStaRec->fgIsAP) ? TRUE : FALSE), prActivateStaRec->aucMacAddr);

		}
		break;

	case EVENT_ID_DEACTIVATE_STA_REC_T:
		{
			P_EVENT_DEACTIVATE_STA_REC_T prDeactivateStaRec;

			prDeactivateStaRec = (P_EVENT_DEACTIVATE_STA_REC_T) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_DEACTIVATE_STA_REC_T Index:%d, MAC:[%pM]\n",
				 prDeactivateStaRec->ucStaRecIdx, prActivateStaRec->aucMacAddr);

			qmDeactivateStaRec(prAdapter, prDeactivateStaRec->ucStaRecIdx);
		}
		break;
#endif

	case EVENT_ID_SCAN_DONE:
		scnEventScanDone(prAdapter, (P_EVENT_SCAN_DONE) (prEvent->aucBuffer));
		break;

	case EVENT_ID_TX_DONE_STATUS:
	{
		secHandleTxStatus(prAdapter, prEvent->aucBuffer);
	}
		break;

	case EVENT_ID_TX_DONE:
	{
		P_EVENT_TX_DONE_T prTxDone;

		prTxDone = (P_EVENT_TX_DONE_T) (prEvent->aucBuffer);

		if (prTxDone->ucStatus) {
			DBGLOG(RX, INFO, "EVENT_ID_TX_DONE PacketSeq:%u ucStatus: %u SN: %u\n",
				prTxDone->ucPacketSeq, prTxDone->ucStatus, prTxDone->u2SequenceNumber);
			if (prTxDone->ucStatus == TX_RESULT_FW_FLUSH)
				prAdapter->ucFlushCount++;
		} else
			prAdapter->ucFlushCount = 0;

		/*when Fw flushed continusous packages, driver do whole chip reset !*/
		if (prAdapter->ucFlushCount >= RX_FW_FLUSH_PKT_THRESHOLD) {
			DBGLOG(RX, ERROR, "FW flushed continusous packages :%d\n", prAdapter->ucFlushCount);
			prAdapter->ucFlushCount = 0;
#if 0
			kalSendAeeWarning("[Fatal error! FW Flushed PKT too much!]", __func__);
			GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
#endif
		}

		/* call related TX Done Handler */
		prMsduInfo = nicGetPendingTxMsduInfo(prAdapter, prTxDone->ucPacketSeq);

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
		DBGLOG(RX, TRACE, "EVENT_ID_TX_DONE u4TimeStamp = %x u2AirDelay = %x\n",
					    prTxDone->au4Reserved1, prTxDone->au4Reserved2);

		wnmReportTimingMeas(prAdapter, prMsduInfo->ucStaRecIndex,
					   prTxDone->au4Reserved1, prTxDone->au4Reserved1 + prTxDone->au4Reserved2);
#endif

		if (prMsduInfo) {
			prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
							   (ENUM_TX_RESULT_CODE_T) (prTxDone->ucStatus));

			cnmMgtPktFree(prAdapter, prMsduInfo);
		}
	}
		break;
	case EVENT_ID_SLEEPY_NOTIFY:
		{
			P_EVENT_SLEEPY_NOTIFY prEventSleepyNotify;

			prEventSleepyNotify = (P_EVENT_SLEEPY_NOTIFY) (prEvent->aucBuffer);

			/* DBGLOG(RX, INFO, ("ucSleepyState = %d\n", prEventSleepyNotify->ucSleepyState)); */

			prAdapter->fgWiFiInSleepyState = (BOOLEAN) (prEventSleepyNotify->ucSleepyState);
		}
		break;
	case EVENT_ID_BT_OVER_WIFI:
#if CFG_ENABLE_BT_OVER_WIFI
		{
			UINT_8 aucTmp[sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)];
			P_EVENT_BT_OVER_WIFI prEventBtOverWifi;
			P_AMPC_EVENT prBowEvent;
			P_BOW_LINK_CONNECTED prBowLinkConnected;
			P_BOW_LINK_DISCONNECTED prBowLinkDisconnected;

			prEventBtOverWifi = (P_EVENT_BT_OVER_WIFI) (prEvent->aucBuffer);

			/* construct event header */
			prBowEvent = (P_AMPC_EVENT) aucTmp;

			if (prEventBtOverWifi->ucLinkStatus == 0) {
				/* Connection */
				prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_CONNECTED;
				prBowEvent->rHeader.ucSeqNumber = 0;
				prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_CONNECTED);

				/* fill event body */
				prBowLinkConnected = (P_BOW_LINK_CONNECTED) (prBowEvent->aucPayload);
				prBowLinkConnected->rChannel.ucChannelNum = prEventBtOverWifi->ucSelectedChannel;
				kalMemZero(prBowLinkConnected->aucPeerAddress, MAC_ADDR_LEN);	/* @FIXME */

				kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
			} else {
				/* Disconnection */
				prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_DISCONNECTED;
				prBowEvent->rHeader.ucSeqNumber = 0;
				prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_DISCONNECTED);

				/* fill event body */
				prBowLinkDisconnected = (P_BOW_LINK_DISCONNECTED) (prBowEvent->aucPayload);
				prBowLinkDisconnected->ucReason = 0;	/* @FIXME */
				kalMemZero(prBowLinkDisconnected->aucPeerAddress, MAC_ADDR_LEN);	/* @FIXME */

				kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
			}
		}
		break;
#endif
	case EVENT_ID_STATISTICS:
		/* buffer statistics for further query */
		prAdapter->fgIsStatValid = TRUE;
		prAdapter->rStatUpdateTime = kalGetTimeTick();
		kalMemCopy(&prAdapter->rStatStruct, prEvent->aucBuffer, sizeof(EVENT_STATISTICS));

		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			prCmdInfo->u4FwResponseTime = kalGetTimeTick();
			wlanDebugCommandRecodTime(prCmdInfo);
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

	case EVENT_ID_CH_PRIVILEGE:
		cnmChMngrHandleChEvent(prAdapter, prEvent);
		break;

	case EVENT_ID_BSS_ABSENCE_PRESENCE:
		qmHandleEventBssAbsencePresence(prAdapter, prEvent);
		break;

	case EVENT_ID_STA_CHANGE_PS_MODE:
		qmHandleEventStaChangePsMode(prAdapter, prEvent);
		break;
#if CFG_ENABLE_WIFI_DIRECT
	case EVENT_ID_STA_UPDATE_FREE_QUOTA:
		qmHandleEventStaUpdateFreeQuota(prAdapter, prEvent);
		break;
#endif
	case EVENT_ID_BSS_BEACON_TIMEOUT:
		if (prAdapter->fgDisBcnLostDetection == FALSE) {
			P_EVENT_BSS_BEACON_TIMEOUT_T prEventBssBeaconTimeout;

			prEventBssBeaconTimeout = (P_EVENT_BSS_BEACON_TIMEOUT_T) (prEvent->aucBuffer);

			DBGLOG(RX, INFO, "Beacon Timeout Reason = %u, update bad RSSI=-127 to upper layer\n",
				prEventBssBeaconTimeout->ucReason);

			if (prEventBssBeaconTimeout->ucNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
				/* Request stats report before beacon timeout */
				P_BSS_INFO_T prBssInfo;
				P_STA_RECORD_T prStaRec;

				prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
				if (prBssInfo) {
					prStaRec = cnmGetStaRecByAddress(prAdapter,
									NETWORK_TYPE_AIS_INDEX,
									prBssInfo->aucBSSID);
					if (prStaRec)
						STATS_ENV_REPORT_DETECT(prAdapter, prStaRec->ucIndex);
				}

				mtk_cfg80211_vendor_event_rssi_beyond_range(wiphy,
					prAdapter->prGlueInfo->prDevHandler->ieee80211_ptr,
					(INT_32)-127);

				aisBssBeaconTimeout(prAdapter);
				aisRecordBeaconTimeout(prAdapter, prBssInfo);
			}
#if CFG_ENABLE_WIFI_DIRECT
			else if ((prAdapter->fgIsP2PRegistered) &&
				 (prEventBssBeaconTimeout->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX))

				p2pFsmRunEventBeaconTimeout(prAdapter);
#endif
			else {
				DBGLOG(RX, ERROR, "EVENT_ID_BSS_BEACON_TIMEOUT: (ucNetTypeIdx = %d)\n",
						   prEventBssBeaconTimeout->ucNetTypeIndex);
			}
		}

		break;
	case EVENT_ID_UPDATE_NOA_PARAMS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam;

			prEventUpdateNoaParam = (P_EVENT_UPDATE_NOA_PARAMS_T) (prEvent->aucBuffer);

			if (prEventUpdateNoaParam->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
				p2pProcessEvent_UpdateNOAParam(prAdapter,
							       prEventUpdateNoaParam->ucNetTypeIndex,
							       prEventUpdateNoaParam);
			} else {
				ASSERT(0);
			}
		}
#else
		ASSERT(0);
#endif
		break;

	case EVENT_ID_STA_AGING_TIMEOUT:
#if CFG_ENABLE_WIFI_DIRECT
		{
			if (prAdapter->fgDisStaAgingTimeoutDetection == FALSE) {
				P_EVENT_STA_AGING_TIMEOUT_T prEventStaAgingTimeout;
				P_STA_RECORD_T prStaRec;
				P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

				prEventStaAgingTimeout = (P_EVENT_STA_AGING_TIMEOUT_T) (prEvent->aucBuffer);
				prStaRec = cnmGetStaRecByIndex(prAdapter, prEventStaAgingTimeout->ucStaRecIdx);
				if (prStaRec == NULL)
					break;

				DBGLOG(RX, INFO, "EVENT_ID_STA_AGING_TIMEOUT %u %pM\n",
						    prEventStaAgingTimeout->ucStaRecIdx,
						    prStaRec->aucMacAddr);

				prBssInfo = &(prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex]);

				bssRemoveStaRecFromClientList(prAdapter, prBssInfo, prStaRec);

				/* Call False Auth */
				if (prAdapter->fgIsP2PRegistered)
					p2pFuncDisconnect(prAdapter, prStaRec, FALSE, REASON_CODE_DISASSOC_INACTIVITY);

			}
			/* gDisStaAgingTimeoutDetection */
		}
#endif
		break;

	case EVENT_ID_AP_OBSS_STATUS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			rlmHandleObssStatusEventPkt(prAdapter, (P_EVENT_AP_OBSS_STATUS_T) prEvent->aucBuffer);
#endif
		break;

	case EVENT_ID_ROAMING_STATUS:
#if CFG_SUPPORT_ROAMING
		{
			P_ROAMING_PARAM_T prParam;

			prParam = (P_ROAMING_PARAM_T) (prEvent->aucBuffer);
			roamingFsmProcessEvent(prAdapter, prParam);
		}
#endif /* CFG_SUPPORT_ROAMING */
		break;
	case EVENT_ID_SEND_DEAUTH:
		{
			P_WLAN_MAC_HEADER_T prWlanMacHeader;
			P_STA_RECORD_T prStaRec;

			prWlanMacHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
			DBGLOG(RSN, INFO, "nicRx: aucAddr1: %pM, nicRx: aucAddr2: %pM\n",
					prWlanMacHeader->aucAddr1, prWlanMacHeader->aucAddr2);
			prStaRec = cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_AIS_INDEX, prWlanMacHeader->aucAddr2);
			if (prStaRec != NULL && prStaRec->ucStaState == STA_STATE_3) {
				DBGLOG(RSN, WARN, "Ignore Deauth for Rx Class 3 error!\n");
			} else {
				/* receive packets without StaRec */
				prSwRfb->pvHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
				if (authSendDeauthFrame(prAdapter,
							NULL,
							prSwRfb,
							REASON_CODE_CLASS_3_ERR,
							(PFN_TX_DONE_HANDLER) NULL) == WLAN_STATUS_SUCCESS)
					DBGLOG(RSN, INFO, "Send Deauth for Rx Class3 Error\n");
				else
					DBGLOG(RSN, WARN, "failed to send deauth for Rx class3 error\n");
			}
		}
		break;

#if CFG_SUPPORT_RDD_TEST_MODE
	case EVENT_ID_UPDATE_RDD_STATUS:
		{
			P_EVENT_RDD_STATUS_T prEventRddStatus;

			prEventRddStatus = (P_EVENT_RDD_STATUS_T) (prEvent->aucBuffer);

			prAdapter->ucRddStatus = prEventRddStatus->ucRddStatus;
		}

		break;
#endif

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	case EVENT_ID_UPDATE_BWCS_STATUS:
		{
			P_PTA_IPC_T prEventBwcsStatus;

			prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			DBGLOG(RSN, INFO,
			"BCM BWCS Event: %02x%02x%02x%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);

			DBGLOG(RSN, INFO,
			"BCM BWCS Event: BTPParams[0]:%02x, BTPParams[1]:%02x, BTPParams[2]:%02x, BTPParams[3]:%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);
#endif

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_BWCS_UPDATE,
						     (PVOID) prEventBwcsStatus, sizeof(PTA_IPC_T));
		}

		break;

	case EVENT_ID_UPDATE_BCM_DEBUG:
		{
			P_PTA_IPC_T prEventBwcsStatus;

			prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			DBGLOG(RSN, INFO,
			"BCM FW status: %02x%02x%02x%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);

			DBGLOG(RSN, INFO,
			"BCM FW status: BTPParams[0]:%02x, BTPParams[1]:%02x, BTPParams[2]:%02x, BTPParams[3]:%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3];
#endif
		}

		break;
#endif

	case EVENT_ID_DEBUG_CODE:	/* only for debug */
		{
			UINT_32 u4CodeId;

			DBGLOG(RSN, INFO, "[wlan-fw] function sequence: ");
			for (u4CodeId = 0; u4CodeId < 1000; u4CodeId++)
				DBGLOG(RSN, INFO, "%d ", prEvent->aucBuffer[u4CodeId]);
			DBGLOG(RSN, INFO, "\n\n");
		}
		break;

	case EVENT_ID_RFTEST_READY:

		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			prCmdInfo->u4FwResponseTime = kalGetTimeTick();
			wlanDebugCommandRecodTime(prCmdInfo);
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

#if CFG_SUPPORT_GSCN
	case EVENT_ID_GSCAN_SCAN_AVAILABLE:
	case EVENT_ID_GSCAN_CAPABILITY:
	case EVENT_ID_GSCAN_SCAN_COMPLETE:
	case EVENT_ID_GSCAN_FULL_RESULT:
	case EVENT_ID_GSCAN_SIGNIFICANT_CHANGE:
	case EVENT_ID_GSCAN_GEOFENCE_FOUND:
	case EVENT_ID_GSCAN_RESULT:
		nicRxProcessGSCNEvent(prAdapter, prSwRfb);
		break;
#endif

	case EVENT_ID_NLO_DONE:
#if CFG_SUPPORT_SCN_PSCN
		prAdapter->rWifiVar.rScanInfo.fgPscnOngoing = FALSE;
#endif
		DBGLOG(INIT, INFO, "EVENT_ID_NLO_DONE\n");
		scnEventNloDone(prAdapter, (P_EVENT_NLO_DONE_T) (prEvent->aucBuffer));

		break;
	case EVENT_ID_RSP_CHNL_UTILIZATION:
		cnmHandleChannelUtilization(prAdapter, (struct EVENT_RSP_CHNL_UTILIZATION *)prEvent->aucBuffer);
		break;

#if CFG_SUPPORT_BATCH_SCAN
	case EVENT_ID_BATCH_RESULT:
		DBGLOG(SCN, TRACE, "Got EVENT_ID_BATCH_RESULT");

		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			prCmdInfo->u4FwResponseTime = kalGetTimeTick();
			wlanDebugCommandRecodTime(prCmdInfo);
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
#endif /* CFG_SUPPORT_BATCH_SCAN */

	case EVENT_ID_RSSI_MONITOR:
		{
			INT_32 rssi = 0;

			kalMemCopy(&rssi, prEvent->aucBuffer, sizeof(INT_32));
			DBGLOG(RX, TRACE, "EVENT_ID_RSSI_MONITOR value=%d\n", rssi);

			mtk_cfg80211_vendor_event_rssi_beyond_range(wiphy,
				prGlueInfo->prDevHandler->ieee80211_ptr, rssi);
		}
		break;

#if (CFG_SUPPORT_TDLS == 1)
	case EVENT_ID_TDLS:
		TdlsexEventHandle(prAdapter->prGlueInfo,
				  (UINT8 *) prEvent->aucBuffer, (UINT32) (prEvent->u2PacketLen - 8));
		break;
#endif /* CFG_SUPPORT_TDLS */

#if (CFG_SUPPORT_STATISTICS == 1)
	case EVENT_ID_STATS_ENV:
		statsEventHandle(prAdapter->prGlueInfo,
				 (UINT8 *) prEvent->aucBuffer, (UINT32) (prEvent->u2PacketLen - 8));
		break;
#endif /* CFG_SUPPORT_STATISTICS */
	case EVENT_ID_CHECK_REORDER_BUBBLE:
		qmHandleEventCheckReorderBubble(prAdapter, prEvent);
		break;
#if (CFG_SUPPORT_EMI_DEBUG == 1)
	case EVENT_ID_DRIVER_DUMP_LOG:
		{
			P_EVENT_DRIVER_DUMP_EMI_LOG_T prEventDriverDumpEmiLog;

			DBGLOG(RX, TRACE, "EVENT_ID_DRIVER_DUMP_LOG\n");
			prEventDriverDumpEmiLog = (P_EVENT_DRIVER_DUMP_EMI_LOG_T) (prEvent->aucBuffer);
			wlanReadFwInfoFromEmi(&(prEventDriverDumpEmiLog->u4RequestDriverDumpAddr));
			break;
		}
#endif
	case EVENT_ID_FW_LOG_ENV:
		{
			P_EVENT_FW_LOG_T prEventLog;

			prEventLog = (P_EVENT_FW_LOG_T) (prEvent->aucBuffer);
			prEventLog->log[MAX_FW_LOG_LENGTH - 1] = '\0';
			DBGLOG(RX, INFO, "[F-L]%s\n", prEventLog->log);
		}
		break;
	case EVENT_ID_ADD_PKEY_DONE:
		{
			struct EVENT_ADD_KEY_DONE_INFO *prKeyDone =
				(struct EVENT_ADD_KEY_DONE_INFO *)prEvent->aucBuffer;
			P_STA_RECORD_T prStaRec = NULL;
			UINT_8 ucKeyId;

			prStaRec = cnmGetStaRecByAddress(prAdapter, prKeyDone->ucNetworkType, prKeyDone->aucStaAddr);

			if (!prStaRec) {
				ucKeyId = prAdapter->rWifiVar.rAisSpecificBssInfo.ucKeyAlgorithmId;
				if ((ucKeyId == CIPHER_SUITE_WEP40) || (ucKeyId == CIPHER_SUITE_WEP104)) {
					DBGLOG(RX, INFO, "WEP, ucKeyAlgorithmId= %d\n", ucKeyId);
					prStaRec = cnmGetStaRecByAddress(prAdapter, prKeyDone->ucNetworkType,
						prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].aucBSSID);
					if (!prStaRec) {
						DBGLOG(RX, INFO, "WEP, AddPKeyDone, Net %d, Addr %pM, StaRec is NULL\n",
							prKeyDone->ucNetworkType,
							prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].aucBSSID);
						break;
					}
				} else {
					DBGLOG(RX, INFO, "AddPKeyDone, Net %d, Addr %pM, StaRec is NULL\n",
						prKeyDone->ucNetworkType, prKeyDone->aucStaAddr);
					break;
				}
			}
			prStaRec->fgIsTxKeyReady = TRUE;
			if (prStaRec->fgIsValid)
				prStaRec->fgIsTxAllowed = TRUE;
			DBGLOG(RX, INFO, "AddPKeyDone, Net %d, Addr %pM, Tx Allowed %d\n",
				prKeyDone->ucNetworkType, prKeyDone->aucStaAddr, prStaRec->fgIsTxAllowed);
			break;
		}
#if CFG_SUPPORT_P2P_ECSA
	case EVENT_ID_ECSA_RESULT:
		{
			P_EVENT_ECSA_RESULT prEcsa = (P_EVENT_ECSA_RESULT) (prEvent->aucBuffer);

			DBGLOG(RX, INFO, "BssIndex:status:PrimaryChannel:Sco: %d:%d:%d:%d\n",
				prEcsa->ucNetTypeIndex,
				prEcsa->ucStatus,
				prEcsa->ucPrimaryChannel,
				prEcsa->ucRfSco);
			kalP2pUpdateECSA(prAdapter, prEcsa);
		}
		break;
#endif
	default:
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			prCmdInfo->u4FwResponseTime = kalGetTimeTick();
			wlanDebugCommandRecodTime(prCmdInfo);
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else if (prEvent->ucEID == EVENT_ID_GET_TSM_STATISTICS)/* in case of unsolicited event */
			wmmComposeTsmRpt(prAdapter, NULL, prEvent->aucBuffer);

		break;
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief nicRxProcessMgmtPacket is used to dispatch management frames
*        to corresponding modules
*
* @param prAdapter Pointer to the Adapter structure.
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessMgmtPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	UINT_8 ucSubtype;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	nicRxFillRFB(prAdapter, prSwRfb);

	ucSubtype = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FC_SUBTYPE) >> OFFSET_OF_FC_SUBTYPE;

#if 0				/* CFG_RX_PKTS_DUMP */
	{
		P_HIF_RX_HEADER_T prHifRxHdr;
		UINT_16 u2TxFrameCtrl;

		prHifRxHdr = prSwRfb->prHifRxHdr;
		u2TxFrameCtrl = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FRAME_TYPE);
		/* if (prAdapter->rRxCtrl.u4RxPktsDumpTypeMask & BIT(HIF_RX_PKT_TYPE_MANAGEMENT)) { */
		/* if (u2TxFrameCtrl == MAC_FRAME_BEACON || */
		/* u2TxFrameCtrl == MAC_FRAME_PROBE_RSP) { */

		DBGLOG(RX, INFO, "QM RX MGT: net %u sta idx %u wlan idx %u ssn %u ptype %u subtype %u 11 %u\n",
		(UINT_32) HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr), prHifRxHdr->ucStaRecIdx,
		prSwRfb->ucWlanIdx, (UINT_32) HIF_RX_HDR_GET_SN(prHifRxHdr),/* The new SN of the frame */
		prSwRfb->ucPacketType, ucSubtype, HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr));

		/* DBGLOG_MEM8(SW4, TRACE, (PUINT_8)prSwRfb->pvHeader, prSwRfb->u2PacketLen); */
		/* } */
		/* } */
	}
#endif

	if ((prAdapter->fgTestMode == FALSE) && (prAdapter->prGlueInfo->fgIsRegistered == TRUE)) {
#if CFG_MGMT_FRAME_HANDLING
#if CFG_SUPPORT_802_11W
		P_RX_CTRL_T prRxCtrl;
		BOOLEAN fgMfgDrop = FALSE;

		fgMfgDrop = rsnCheckRxMgmt(prAdapter, prSwRfb, ucSubtype);
		if (fgMfgDrop) {
			prRxCtrl = &prAdapter->rRxCtrl;
			ASSERT(prRxCtrl);
#if DBG
			LOG_FUNC("QM RX MGT: Drop Unprotected Mgmt frame!!!\n");
#endif
			nicRxReturnRFB(prAdapter, prSwRfb);
			RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
			return;
		}
#endif
		if (apfnProcessRxMgtFrame[ucSubtype]) {
			switch (apfnProcessRxMgtFrame[ucSubtype] (prAdapter, prSwRfb)) {
			case WLAN_STATUS_PENDING:
				return;
			case WLAN_STATUS_SUCCESS:
			case WLAN_STATUS_FAILURE:
				break;

			default:
				DBGLOG(RX, WARN,
				       "Unexpected MMPDU(0x%02X) returned with abnormal status\n", ucSubtype);
				break;
			}
		}
#endif
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
static VOID nicRxCheckWakeupReason(P_SW_RFB_T prSwRfb)
{
	PUINT_8 pvHeader = NULL;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_16 u2PktLen = 0;
	UINT_32 u4HeaderOffset;

	if (!prSwRfb)
		return;
	prHifRxHdr = prSwRfb->prHifRxHdr;
	if (!prHifRxHdr)
		return;

	switch (prSwRfb->ucPacketType) {
	case HIF_RX_PKT_TYPE_DATA:
	{
		UINT_16 u2Temp = 0;

		if (HIF_RX_HDR_GET_BAR_FLAG(prHifRxHdr)) {
			DBGLOG(RX, INFO, "BAR frame[SSN:%d, TID:%d] wakeup host\n",
				(UINT_16)HIF_RX_HDR_GET_SN(prHifRxHdr), (UINT_8)HIF_RX_HDR_GET_TID(prHifRxHdr));
			break;
		}
		u4HeaderOffset = (UINT_32)(prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_OFFSET_MASK);
		pvHeader = (PUINT_8)prHifRxHdr + HIF_RX_HDR_SIZE + u4HeaderOffset;
		u2PktLen = (UINT_16)(prHifRxHdr->u2PacketLen - (HIF_RX_HDR_SIZE + u4HeaderOffset));
		if (!pvHeader) {
			DBGLOG(RX, ERROR, "data packet but pvHeader is NULL!\n");
			break;
		}
		u2Temp = (pvHeader[ETH_TYPE_LEN_OFFSET] << 8) | (pvHeader[ETH_TYPE_LEN_OFFSET + 1]);

		switch (u2Temp) {
		case ETH_P_IPV4:
			u2Temp = *(UINT_16 *) &pvHeader[ETH_HLEN + 4];
			DBGLOG(RX, INFO, "IP Packet:%d.%d.%d.%d, to:%d.%d.%d.%d,ID 0x%04x wakeup host\n",
				pvHeader[ETH_HLEN + 12], pvHeader[ETH_HLEN + 13],
				pvHeader[ETH_HLEN + 14], pvHeader[ETH_HLEN + 15],
				pvHeader[ETH_HLEN + 16], pvHeader[ETH_HLEN + 17],
				pvHeader[ETH_HLEN + 18], pvHeader[ETH_HLEN + 19],
				u2Temp);
			break;
		case ETH_P_ARP:
		{
			PUINT_8 pucEthBody = &pvHeader[ETH_HLEN];
			UINT_16 u2OpCode = (pucEthBody[6] << 8) | pucEthBody[7];

			if (u2OpCode == ARP_PRO_REQ)
				DBGLOG(RX, INFO, "Arp Req From IP: %d.%d.%d.%d wakeup host\n",
					pucEthBody[14], pucEthBody[15], pucEthBody[16], pucEthBody[17]);
			else if (u2OpCode == ARP_PRO_RSP)
				DBGLOG(RX, INFO, "Arp Rsp from IP: %d.%d.%d.%d wakeup host\n",
					pucEthBody[14], pucEthBody[15], pucEthBody[16], pucEthBody[17]);
			break;
		}
		case ETH_P_1X:
			/* Fall through */
		case ETH_P_PRE_1X:
			/* Fall through */
#if CFG_SUPPORT_WAPI
		case ETH_WPI_1X:
			/* Fall through */
#endif
		case ETH_P_AARP:
			/* Fall through */
		case ETH_P_IPV6:
			/* Fall through */
		case ETH_P_IPX:
			/* Fall through */
		case 0x8100: /* VLAN */
			/* Fall through */
		case 0x890d: /* TDLS */
			DBGLOG(RX, INFO, "Data Packet, EthType 0x%04x wakeup host\n", u2Temp);
			break;
		default:
			DBGLOG(RX, WARN, "maybe abnormal data packet, EthType 0x%04x wakeup host, dump it\n",
				u2Temp);
			DBGLOG_MEM8(RX, INFO, pvHeader, u2PktLen > 50 ? 50 : u2PktLen);
			break;
		}
		break;
	}
	case HIF_RX_PKT_TYPE_EVENT:
	{
		P_WIFI_EVENT_T prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;

		DBGLOG(RX, INFO, "Event 0x%02x wakeup host\n", prEvent->ucEID);
		break;
	}
	case HIF_RX_PKT_TYPE_MANAGEMENT:
	{
		UINT_8 ucSubtype;
		P_WLAN_MAC_MGMT_HEADER_T prWlanMgmtHeader;

		u4HeaderOffset = (UINT_32)(prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_OFFSET_MASK);
		pvHeader = (PUINT_8)prHifRxHdr + HIF_RX_HDR_SIZE + u4HeaderOffset;
		if (!pvHeader) {
			DBGLOG(RX, ERROR, "Mgmt Frame but pvHeader is NULL!\n");
			break;
		}
		prWlanMgmtHeader = (P_WLAN_MAC_MGMT_HEADER_T)pvHeader;
		ucSubtype = (prWlanMgmtHeader->u2FrameCtrl & MASK_FC_SUBTYPE) >>
				OFFSET_OF_FC_SUBTYPE;
		DBGLOG(RX, INFO, "MGMT frame subtype: %d SeqCtrl %d wakeup host\n",
				ucSubtype, prWlanMgmtHeader->u2SeqCtrl);
		break;
	}
	default:
		DBGLOG(RX, WARN, "Unknown Packet %d wakeup host\n", prSwRfb->ucPacketType);
		break;
	}
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief nicProcessRFBs is used to process RFBs in the rReceivedRFBList queue.
*
* @param prAdapter Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessRFBs(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

#if CFG_SUPPORT_MULTITHREAD
	QUE_T rTempRxDataQue;
	P_QUE_T prTempRxDataQue = &rTempRxDataQue;

	QUEUE_INITIALIZE(prTempRxDataQue);
#endif

	DEBUGFUNC("nicRxProcessRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

#if !(CFG_SUPPORT_MULTITHREAD)
	prRxCtrl->ucNumIndPacket = 0;
	prRxCtrl->ucNumRetainedPacket = 0;
#endif
	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rReceivedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (prSwRfb) {
#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
			if (kalIsWakeupByWlan(prAdapter))
				nicRxCheckWakeupReason(prSwRfb);
#endif
			switch (prSwRfb->ucPacketType) {
			case HIF_RX_PKT_TYPE_DATA:
#if CFG_SUPPORT_MULTITHREAD
				QUEUE_INSERT_TAIL(prTempRxDataQue, &prSwRfb->rQueEntry);
#else
				nicRxProcessDataPacket(prAdapter, prSwRfb);
#endif
				break;

			case HIF_RX_PKT_TYPE_EVENT:
				nicRxProcessEventPacket(prAdapter, prSwRfb);
				break;

			case HIF_RX_PKT_TYPE_TX_LOOPBACK:
#if (CONF_HIF_LOOPBACK_AUTO == 1)
				{
					kalDevLoopbkRxHandle(prAdapter, prSwRfb);
					nicRxReturnRFB(prAdapter, prSwRfb);
				}
#else
				DBGLOG(RX, ERROR, "ucPacketType = %d\n", prSwRfb->ucPacketType);
#endif /* CONF_HIF_LOOPBACK_AUTO */
				break;

			case HIF_RX_PKT_TYPE_MANAGEMENT:
				nicRxProcessMgmtPacket(prAdapter, prSwRfb);
				break;

			default:
				RX_INC_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT);
				RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
				DBGLOG(RX, ERROR, "ucPacketType = %d\n", prSwRfb->ucPacketType);
				nicRxReturnRFB(prAdapter, prSwRfb);	/* need to free it */
				break;
			}
		} else {
			break;
		}
	} while (TRUE);
#if CFG_SUPPORT_MULTITHREAD
	if (QUEUE_IS_NOT_EMPTY(prTempRxDataQue)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_DATA_QUE);
		QUEUE_CONCATENATE_QUEUES(&prRxCtrl->rRxDataRfbList, prTempRxDataQue);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_DATA_QUE);
		kalWakeupRxThread(prAdapter->prGlueInfo);
	}
#else
	if (prRxCtrl->ucNumIndPacket > 0) {
		RX_ADD_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT, prRxCtrl->ucNumIndPacket);
		RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT, prRxCtrl->ucNumRetainedPacket);

		/* DBGLOG(RX, INFO, ("%d packets indicated, Retained cnt = %d\n", */
		/* prRxCtrl->ucNumIndPacket, prRxCtrl->ucNumRetainedPacket)); */
	#if CFG_NATIVE_802_11
		kalRxIndicatePkts(prAdapter->prGlueInfo, (UINT_32) prRxCtrl->ucNumIndPacket,
				  (UINT_32) prRxCtrl->ucNumRetainedPacket);
	#else
		kalRxIndicatePkts(prAdapter->prGlueInfo, prRxCtrl->apvIndPacket, (UINT_32) prRxCtrl->ucNumIndPacket);
	#endif
	}
#endif
}				/* end of nicRxProcessRFBs() */

#if !CFG_SDIO_INTR_ENHANCE
/*----------------------------------------------------------------------------*/
/*!
* @brief Read the rx data from data port and setup RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @retval WLAN_STATUS_SUCCESS: SUCCESS
* @retval WLAN_STATUS_FAILURE: FAILURE
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxReadBuffer(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucBuf;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_32 u4PktLen = 0, u4ReadBytes;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	BOOLEAN fgResult = TRUE;
	UINT_32 u4RegValue;
	UINT_32 rxNum;

	DEBUGFUNC("nicRxReadBuffer");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	pucBuf = prSwRfb->pucRecvBuff;
	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(pucBuf);
	DBGLOG(RX, TRACE, "pucBuf= 0x%x, prHifRxHdr= 0x%x\n", pucBuf, prHifRxHdr);

	do {
		/* Read the RFB DW length and packet length */
		HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4RegValue);
		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read RX Packet Lentgh Error\n");
			return WLAN_STATUS_FAILURE;
		}
		/* 20091021 move the line to get the HIF RX header (for RX0/1) */
		if (u4RegValue == 0) {
			DBGLOG(RX, ERROR, "No RX packet\n");
			return WLAN_STATUS_FAILURE;
		}

		u4PktLen = u4RegValue & BITS(0, 15);
		if (u4PktLen != 0) {
			rxNum = 0;
		} else {
			rxNum = 1;
			u4PktLen = (u4RegValue & BITS(16, 31)) >> 16;
		}

		DBGLOG(RX, TRACE, "RX%d: u4PktLen = %d\n", rxNum, u4PktLen);

		/* 4 <4> Read Entire RFB and packet, include HW appended DW (Checksum Status) */
		u4ReadBytes = ALIGN_4(u4PktLen) + 4;
		HAL_READ_RX_PORT(prAdapter, rxNum, u4ReadBytes, pucBuf, CFG_RX_MAX_PKT_SIZE);

		/* 20091021 move the line to get the HIF RX header */
		/* u4PktLen = (UINT_32)prHifRxHdr->u2PacketLen; */
		if (u4PktLen != (UINT_32) prHifRxHdr->u2PacketLen) {
			DBGLOG(RX, ERROR, "Read u4PktLen = %d, prHifRxHdr->u2PacketLen: %d\n",
					   u4PktLen, prHifRxHdr->u2PacketLen);
#if DBG
			dumpMemory8((PUINT_8) prHifRxHdr,
				    (prHifRxHdr->u2PacketLen > 4096) ? 4096 : prHifRxHdr->u2PacketLen);
#endif
			ASSERT(0);
		}
		/* u4PktLen is byte unit, not inlude HW appended DW */

		prSwRfb->ucPacketType = (UINT_8) (prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK);
		DBGLOG(RX, TRACE, "ucPacketType = %d\n", prSwRfb->ucPacketType);

		prSwRfb->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);

		/* fgResult will be updated in MACRO */
		if (!fgResult)
			return WLAN_STATUS_FAILURE;

		DBGLOG(RX, TRACE, "Dump RX buffer, length = 0x%x\n", u4ReadBytes);
		DBGLOG_MEM8(RX, TRACE, pucBuf, u4ReadBytes);
	} while (FALSE);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port, fill RFB
*        and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter   Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	P_HIF_RX_HEADER_T prHifRxHdr;

	UINT_32 u4HwAppendDW;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxReceiveRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (!prSwRfb) {
			DBGLOG(RX, TRACE, "No More RFB\n");
			break;
		}
		/* need to consider */
		if (nicRxReadBuffer(prAdapter, prSwRfb) == WLAN_STATUS_FAILURE) {
			DBGLOG(RX, TRACE, "halRxFillRFB failed\n");
			nicRxReturnRFB(prAdapter, prSwRfb);
			break;
		}

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
		RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		prHifRxHdr = prSwRfb->prHifRxHdr;
		u4HwAppendDW = *((PUINT_32) ((ULONG) prHifRxHdr + (UINT_32) (ALIGN_4(prHifRxHdr->u2PacketLen))));
		DBGLOG(RX, TRACE, "u4HwAppendDW = 0x%x\n", u4HwAppendDW);
		DBGLOG(RX, TRACE, "u2PacketLen = 0x%x\n", prHifRxHdr->u2PacketLen);
	} while (FALSE);	/* while (RX_STATUS_TEST_MORE_FLAG(u4HwAppendDW)); */

	return;

}				/* end of nicReceiveRFBs() */

#else
/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port, fill RFB
*        and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param u4DataPort     Specify which port to read
* @param u2RxLength     Specify to the the rx packet length in Byte.
* @param prSwRfb        the RFB to receive rx data.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS
nicRxEnhanceReadBuffer(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DataPort, IN UINT_16 u2RxLength, IN OUT P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucBuf;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_32 u4PktLen = 0;
	WLAN_STATUS u4Status = WLAN_STATUS_FAILURE;
	BOOLEAN fgResult = TRUE;

	DEBUGFUNC("nicRxEnhanceReadBuffer");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	pucBuf = prSwRfb->pucRecvBuff;
	ASSERT(pucBuf);

	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(prHifRxHdr);

	/* DBGLOG(RX, TRACE, ("u2RxLength = %d\n", u2RxLength)); */

	do {
		/* 4 <1> Read RFB frame from MCR_WRDR0, include HW appended DW */
		HAL_READ_RX_PORT(prAdapter,
				 u4DataPort, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN), pucBuf, CFG_RX_MAX_PKT_SIZE);

		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read RX Packet Lentgh Error\n");
			break;
		}

		u4PktLen = (UINT_32) (prHifRxHdr->u2PacketLen);
		/* DBGLOG(RX, TRACE, ("u4PktLen = %d\n", u4PktLen)); */

		prSwRfb->ucPacketType = (UINT_8) (prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK);
		/* DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType)); */

		prSwRfb->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);

		/* 4 <2> if the RFB dw size or packet size is zero */
		if (u4PktLen == 0) {
			DBGLOG(RX, ERROR, "Packet Length = %u\n", u4PktLen);
			ASSERT(0);
			break;
		}
		/* 4 <3> if the packet is too large or too small */
		if (u4PktLen > CFG_RX_MAX_PKT_SIZE) {
			DBGLOG(RX, TRACE, "Read RX Packet Lentgh Error (%u)\n", u4PktLen);
			ASSERT(0);
			break;
		}

		u4Status = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	DBGLOG_MEM8(RX, TRACE, pucBuf, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN));
	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port for SDIO
*        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxSDIOReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
	P_SDIO_CTRL_T prSDIOCtrl;
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	UINT_32 i, rxNum;
	UINT_16 u2RxPktNum, u2RxLength = 0, u2Tmp = 0;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxSDIOReceiveRFBs");

	ASSERT(prAdapter);

	prSDIOCtrl = prAdapter->prSDIOCtrl;
	ASSERT(prSDIOCtrl);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	for (rxNum = 0; rxNum < 2; rxNum++) {
		u2RxPktNum =
		    (rxNum == 0 ? prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len : prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len);

		if (u2RxPktNum == 0)
			continue;

		for (i = 0; i < u2RxPktNum; i++) {
			if (rxNum == 0) {
				/* HAL_READ_RX_LENGTH */
				HAL_READ_RX_LENGTH(prAdapter, &u2RxLength, &u2Tmp);
			} else if (rxNum == 1) {
				/* HAL_READ_RX_LENGTH */
				HAL_READ_RX_LENGTH(prAdapter, &u2Tmp, &u2RxLength);
			}

			if (!u2RxLength)
				break;

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

			if (!prSwRfb) {
				DBGLOG(RX, TRACE, "No More RFB\n");
				break;
			}
			ASSERT(prSwRfb);

			if (nicRxEnhanceReadBuffer(prAdapter, rxNum, u2RxLength, prSwRfb) == WLAN_STATUS_FAILURE) {
				DBGLOG(RX, TRACE, "nicRxEnhanceRxReadBuffer failed\n");
				nicRxReturnRFB(prAdapter, prSwRfb);
				break;
			}
			/* prSDIOCtrl->au4RxLength[i] = 0; */

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
			RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		}
	}

	prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len = 0;
	prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len = 0;

}				/* end of nicRxSDIOReceiveRFBs() */

#endif /* CFG_SDIO_INTR_ENHANCE */

#if CFG_SDIO_RX_AGG
/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port for SDIO with Rx aggregation enabled
*        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxSDIOAggReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
	P_ENHANCE_MODE_DATA_STRUCT_T prEnhDataStr;
	P_RX_CTRL_T prRxCtrl;
	P_SDIO_CTRL_T prSDIOCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	UINT_32 u4RxLength;
	UINT_32 i, rxNum;
	UINT_32 u4RxAggCount = 0, u4RxAggLength = 0;
	UINT_32 u4RxAvailAggLen, u4CurrAvailFreeRfbCnt;
	PUINT_8 pucSrcAddr;
	P_HIF_RX_HEADER_T prHifRxHdr;
	BOOLEAN fgIsRxEnhanceMode;
	UINT_16 u2RxPktNum;
#if CFG_SDIO_RX_ENHANCE
	UINT_32 u4MaxLoopCount = CFG_MAX_RX_ENHANCE_LOOP_COUNT;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxSDIOAggReceiveRFBs");

	ASSERT(prAdapter);
	prEnhDataStr = prAdapter->prSDIOCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;
	prSDIOCtrl = prAdapter->prSDIOCtrl;

#if CFG_SDIO_RX_ENHANCE
	fgIsRxEnhanceMode = TRUE;
#else
	fgIsRxEnhanceMode = FALSE;
#endif


	do {
#if CFG_SDIO_RX_ENHANCE
		/* to limit maximum loop for RX */
		u4MaxLoopCount--;
		if (u4MaxLoopCount == 0)
			break;
#endif

		if (prEnhDataStr->rRxInfo.u.u2NumValidRx0Len == 0 && prEnhDataStr->rRxInfo.u.u2NumValidRx1Len == 0)
			break;

		for (rxNum = 0; rxNum < 2; rxNum++) {
			u2RxPktNum =
			    (rxNum ==
			     0 ? prEnhDataStr->rRxInfo.u.u2NumValidRx0Len : prEnhDataStr->rRxInfo.u.u2NumValidRx1Len);

			/* if this assertion happened, it is most likely a F/W bug */
			ASSERT(u2RxPktNum <= 16);

			if (u2RxPktNum > 16)
				continue;

			if (u2RxPktNum == 0)
				continue;

#if CFG_HIF_STATISTICS
			prRxCtrl->u4TotalRxAccessNum++;
			prRxCtrl->u4TotalRxPacketNum += u2RxPktNum;
#endif

			u4CurrAvailFreeRfbCnt = prRxCtrl->rFreeSwRfbList.u4NumElem;

			/* if SwRfb is not enough, abort reading this time */
			if (u4CurrAvailFreeRfbCnt < u2RxPktNum) {
#if CFG_HIF_RX_STARVATION_WARNING
				DbgPrint("FreeRfb is not enough: %d available, need %d\n", u4CurrAvailFreeRfbCnt,
					 u2RxPktNum);
				DbgPrint("Queued Count: %d / Dequeud Count: %d\n", prRxCtrl->u4QueuedCnt,
					 prRxCtrl->u4DequeuedCnt);
#endif
				continue;
			}
#if CFG_SDIO_RX_ENHANCE
			u4RxAvailAggLen =
			    CFG_RX_COALESCING_BUFFER_SIZE - (sizeof(ENHANCE_MODE_DATA_STRUCT_T) +
							     4 /* extra HW padding */);
#else
			u4RxAvailAggLen = CFG_RX_COALESCING_BUFFER_SIZE;
#endif
			for (i = 0, u4RxAggCount = 0; i < u2RxPktNum; i++) {
				PUINT_8 pucZeroArray = NULL;

restart:
				u4RxLength = (rxNum == 0 ?
					      (UINT_32) prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
					      (UINT_32) prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

				if (!u4RxLength) {
					ASSERT(0);
					break;
				}

				if (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN) < u4RxAvailAggLen) {
					u4RxAvailAggLen -= ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN);
					u4RxAggCount++;
					continue;
				}

				/* CFG_RX_COALESCING_BUFFER_SIZE is not large enough */
				DBGLOG(RX, ERROR,
				       "[%s] Request_len(%d) is greater than Available_len(%d)\n",
				       __func__,
				       (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN)), u4RxAvailAggLen);
				u4RxLength += (CFG_RX_COALESCING_BUFFER_SIZE - u4RxAvailAggLen);
				pucZeroArray = kalMemAlloc(1000, VIR_MEM_TYPE);
				if (!pucZeroArray)
					break;
				kalMemZero(pucZeroArray, 1000);
				HAL_READ_RX_PORT(prAdapter, rxNum, CFG_RX_COALESCING_BUFFER_SIZE,
						prRxCtrl->pucRxCoalescingBufPtr, CFG_RX_COALESCING_BUFFER_SIZE);
				/* dump RXD if total u4RxLength is greater than u4RxAvailAggLen */
				DBGLOG(RX, ERROR,
					"RXD for the wrong packet is\n");
				DBGLOG_MEM32(RX, ERROR, prRxCtrl->pucRxCoalescingBufPtr+
					(CFG_RX_COALESCING_BUFFER_SIZE - u4RxAvailAggLen),
					sizeof(HW_MAC_RX_DESC_T));
				u4RxLength -= CFG_RX_COALESCING_BUFFER_SIZE;
				/*
				 * we should read out all pending data, otherwise,
				 * DE said the port will be in abnormal case
				 */
				while (u4RxLength > CFG_RX_COALESCING_BUFFER_SIZE) {
					HAL_READ_RX_PORT(prAdapter, rxNum, CFG_RX_COALESCING_BUFFER_SIZE,
						prRxCtrl->pucRxCoalescingBufPtr, CFG_RX_COALESCING_BUFFER_SIZE);
					/* if continuous 1000 bytes were zeros, means there's no data in this port */
					if (!kalMemCmp(pucZeroArray, prRxCtrl->pucRxCoalescingBufPtr, 1000)) {
						kalMemFree(pucZeroArray, VIR_MEM_TYPE, 1000);
						goto restart;
					}
				}
				if (u4RxLength > 0) {
					HAL_READ_RX_PORT(prAdapter, rxNum, ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN),
							prRxCtrl->pucRxCoalescingBufPtr, CFG_RX_COALESCING_BUFFER_SIZE);
				}
				kalMemFree(pucZeroArray, VIR_MEM_TYPE, 1000);
				goto restart;
			}

			u4RxAggLength = (CFG_RX_COALESCING_BUFFER_SIZE - u4RxAvailAggLen);
			/* DBGLOG(RX, INFO, ("u4RxAggCount = %d, u4RxAggLength = %d\n", */
			/* u4RxAggCount, u4RxAggLength)); */

			HAL_READ_RX_PORT(prAdapter,
					 rxNum,
					 u4RxAggLength, prRxCtrl->pucRxCoalescingBufPtr, CFG_RX_COALESCING_BUFFER_SIZE);
			if (fgIsBusAccessFailed) {
				DBGLOG(RX, ERROR, "Read RX Agg Packet Error\n");
				continue;
			}

			pucSrcAddr = prRxCtrl->pucRxCoalescingBufPtr;
			for (i = 0; i < u4RxAggCount; i++) {
				UINT_16 u2PktLength;
				UINT_16 u4HeaderOffset = (((P_HIF_RX_HEADER_T)pucSrcAddr)->ucHerderLenOffset
								& HIF_RX_HDR_HEADER_OFFSET_MASK);
				UINT_16 u4HeaderLen = HIF_RX_HDR_SIZE + u4HeaderOffset;

				u2PktLength = (rxNum == 0 ?
					       prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
					       prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

				if (((P_HIF_RX_HEADER_T)pucSrcAddr)->u2PacketLen < u4HeaderLen
					&& ((((P_HIF_RX_HEADER_T)pucSrcAddr)->u2PacketType
					& HIF_RX_HDR_PACKET_TYPE_MASK) == HIF_RX_PKT_TYPE_DATA)) {
					DBGLOG(RX, ERROR, "rxNum(%d), u2PacketLen(%d), headerLen(%d)\n",
						rxNum, ((P_HIF_RX_HEADER_T)pucSrcAddr)->u2PacketLen,
						u4HeaderLen);
					DBGLOG(RX, ERROR, "Drop the unexpected packet...\n");
					DBGLOG_MEM8(RX, ERROR, pucSrcAddr,
						ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));

					pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
					RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
					GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP | RST_FLAG_PREVENT_POWER_OFF);
					continue;
				}

				if (ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN) > CFG_RX_MAX_PKT_SIZE
					|| ((P_HIF_RX_HEADER_T)pucSrcAddr)->u2PacketLen > CFG_RX_MAX_PKT_SIZE) {
					DBGLOG(RX, ERROR,
						"[%s] rxNum(%d), Request_len(%d), pkt_field_len(%d), MAX_PKT_SIZE(%d)...",
						__func__, rxNum, (ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN)),
						((P_HIF_RX_HEADER_T)pucSrcAddr)->u2PacketLen,
						CFG_RX_MAX_PKT_SIZE);
					DBGLOG(RX, ERROR, "Drop the unexpected packet...\n");
					DBGLOG_MEM8(RX, ERROR, pucSrcAddr,
						ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));

					pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
					RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
					GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP | RST_FLAG_PREVENT_POWER_OFF);
					continue;
				}

				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
				QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

				ASSERT(prSwRfb);
				kalMemCopy(prSwRfb->pucRecvBuff, pucSrcAddr,
					   ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));

				/* record the rx time */
				STATS_RX_ARRIVE_TIME_RECORD(prSwRfb);	/* ms */

				prHifRxHdr = prSwRfb->prHifRxHdr;
				ASSERT(prHifRxHdr);

				prSwRfb->ucPacketType =
				    (UINT_8) (prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK);
				/* DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType)); */

				prSwRfb->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);

				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
				QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
				RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

				pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
				/* prEnhDataStr->au4RxLength[i] = 0; */
			}

#if CFG_SDIO_RX_ENHANCE
			kalMemCopy(prAdapter->prSDIOCtrl, (pucSrcAddr + 4), sizeof(ENHANCE_MODE_DATA_STRUCT_T));

			/* do the same thing what nicSDIOReadIntStatus() does */
			if ((prSDIOCtrl->u4WHISR & WHISR_TX_DONE_INT) == 0 &&
			    (prSDIOCtrl->rTxInfo.au4WTSR[0] | prSDIOCtrl->rTxInfo.au4WTSR[1])) {
				prSDIOCtrl->u4WHISR |= WHISR_TX_DONE_INT;
			}

			if ((prSDIOCtrl->u4WHISR & BIT(31)) == 0 &&
			    HAL_GET_MAILBOX_READ_CLEAR(prAdapter) == TRUE &&
			    (prSDIOCtrl->u4RcvMailbox0 != 0 || prSDIOCtrl->u4RcvMailbox1 != 0)) {
				prSDIOCtrl->u4WHISR |= BIT(31);
			}

			/* dispatch to interrupt handler with RX bits masked */
			nicProcessIST_impl(prAdapter,
					   prSDIOCtrl->u4WHISR & (~(WHISR_RX0_DONE_INT | WHISR_RX1_DONE_INT)));
#endif
		}

#if !CFG_SDIO_RX_ENHANCE
		prEnhDataStr->rRxInfo.u.u2NumValidRx0Len = 0;
		prEnhDataStr->rRxInfo.u.u2NumValidRx1Len = 0;
#endif
	} while ((prEnhDataStr->rRxInfo.u.u2NumValidRx0Len || prEnhDataStr->rRxInfo.u.u2NumValidRx1Len)
		&& fgIsRxEnhanceMode);

}
#endif /* CFG_SDIO_RX_AGG */

/*----------------------------------------------------------------------------*/
/*!
* @brief Setup a RFB and allocate the os packet to the RFB
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prSwRfb        Pointer to the RFB
*
* @retval WLAN_STATUS_SUCCESS
* @retval WLAN_STATUS_RESOURCES
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxSetupRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	PVOID pvPacket;
	PUINT_8 pucRecvBuff;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (!prSwRfb->pvPacket) {
		kalMemZero(prSwRfb, sizeof(SW_RFB_T));
		pvPacket = kalPacketAlloc(prAdapter->prGlueInfo, CFG_RX_MAX_PKT_SIZE, &pucRecvBuff);
		if (pvPacket == NULL)
			return WLAN_STATUS_RESOURCES;

		prSwRfb->pvPacket = pvPacket;
		prSwRfb->pucRecvBuff = (PVOID) pucRecvBuff;
	} else {
		kalMemZero(((PUINT_8) prSwRfb + OFFSET_OF(SW_RFB_T, prHifRxHdr)),
			   (sizeof(SW_RFB_T) - OFFSET_OF(SW_RFB_T, prHifRxHdr)));
	}

	prSwRfb->prHifRxHdr = (P_HIF_RX_HEADER_T) (prSwRfb->pucRecvBuff);

	return WLAN_STATUS_SUCCESS;

}				/* end of nicRxSetupRFB() */
VOID nicRxReturnRFBwithUninit(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb
	, IN BOOLEAN fgIsUninitRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_QUE_ENTRY_T prQueEntry;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRxCtrl = &prAdapter->rRxCtrl;
	prQueEntry = &prSwRfb->rQueEntry;

	ASSERT(prQueEntry);

	if (fgIsUninitRfb) {
		/*
		 * The processing on this RFB is uninitiated, so put it back on the tail of
		 * our list
		 */
		DBGLOGLIMITED(RX, WARN,
			   "wlanReturnPacket nicRxSetupRFB fail!\n");
		/* insert initialized SwRfb block */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prRxCtrl->rUnInitializedRfbList, prQueEntry);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
	} else {
		nicRxReturnRFB(prAdapter, prSwRfb);
	}



}

/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is called to put a RFB back onto the "RFB with Buffer" list
*        or "RFB without buffer" list according to pvPacket.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prSwRfb          Pointer to the RFB
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxReturnRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_QUE_ENTRY_T prQueEntry;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRxCtrl = &prAdapter->rRxCtrl;
	prQueEntry = &prSwRfb->rQueEntry;

	ASSERT(prQueEntry);

	/*
	 * The processing on this RFB is done, so put it back on the tail of
	 * our list
	 */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

	if (prSwRfb->pvPacket) {
		/* QUEUE_INSERT_TAIL */
		QUEUE_INSERT_TAIL(&prRxCtrl->rFreeSwRfbList, prQueEntry);
	} else {
		/* QUEUE_INSERT_TAIL */
		prSwRfb->pucRecvBuff = NULL;
		QUEUE_INSERT_TAIL(&prRxCtrl->rIndicatedRfbList, prQueEntry);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
}				/* end of nicRxReturnRFB() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process rx interrupt. When the rx
*        Interrupt is asserted, it means there are frames in queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicProcessRxInterrupt(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;

	prGlueInfo->IsrRxCnt++;
#if CFG_SDIO_INTR_ENHANCE
#if CFG_SDIO_RX_AGG
	nicRxSDIOAggReceiveRFBs(prAdapter);
#else
	nicRxSDIOReceiveRFBs(prAdapter);
#endif
#else
	nicRxReceiveRFBs(prAdapter);
#endif /* CFG_SDIO_INTR_ENHANCE */

	nicRxProcessRFBs(prAdapter);
}				/* end of nicProcessRxInterrupt() */

#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
* @brief Used to update IP/TCP/UDP checksum statistics of RX Module.
*
* @param prAdapter  Pointer to the Adapter structure.
* @param aeCSUM     The array of checksum result.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxUpdateCSUMStatistics(IN P_ADAPTER_T prAdapter, IN const ENUM_CSUM_RESULT_T aeCSUM[])
{
	P_RX_CTRL_T prRxCtrl;

	ASSERT(prAdapter);
	ASSERT(aeCSUM);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS) ||
		(aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_IP_SUCCESS_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_FAILED) || (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_FAILED)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_IP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_NONE) && (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_NONE)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	} else {
		ASSERT(0);
	}

	if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_SUCCESS) {
		/* count success num */
		RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_FAILED) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_FAILED_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_SUCCESS) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_FAILED) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_NONE) && (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_NONE)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	} else {
		ASSERT(0);
	}

}				/* end of nicRxUpdateCSUMStatistics() */
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to query current status of RX Module.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param pucBuffer      Pointer to the message buffer.
* @param pu4Count      Pointer to the buffer of message length count.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxQueryStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucCurrBuf = pucBuffer;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	/* if (pucBuffer) {} */ /* For Windows, we'll print directly instead of sprintf() */
	ASSERT(pu4Count);

	SPRINTF(pucCurrBuf, ("\n\nRX CTRL STATUS:"));
	SPRINTF(pucCurrBuf, ("\n==============="));
	SPRINTF(pucCurrBuf, ("\nFREE RFB w/i BUF LIST :%9u", prRxCtrl->rFreeSwRfbList.u4NumElem));
	SPRINTF(pucCurrBuf, ("\nFREE RFB w/o BUF LIST :%9u", prRxCtrl->rIndicatedRfbList.u4NumElem));
	SPRINTF(pucCurrBuf, ("\nRECEIVED RFB LIST     :%9u", prRxCtrl->rReceivedRfbList.u4NumElem));
#if CFG_SUPPORT_MULTITHREAD
	SPRINTF(pucCurrBuf, ("\nRECEIVED DATA RFB LIST:%9u", prRxCtrl->rRxDataRfbList.u4NumElem));
#endif
	SPRINTF(pucCurrBuf, ("\n\n"));

	/* *pu4Count = (UINT_32)((UINT_32)pucCurrBuf - (UINT_32)pucBuffer); */

}				/* end of nicRxQueryStatus() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Clear RX related counters
*
* @param prAdapter Pointer of Adapter Data Structure
*
* @return - (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxClearStatistics(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	RX_RESET_ALL_CNTS(prRxCtrl);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to query current statistics of RX Module.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param pucBuffer      Pointer to the message buffer.
* @param pu4Count      Pointer to the buffer of message length count.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxQueryStatistics(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucCurrBuf = pucBuffer;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	/* if (pucBuffer) {} */ /* For Windows, we'll print directly instead of sprintf() */
	ASSERT(pu4Count);

#define SPRINTF_RX_COUNTER(eCounter) \
	SPRINTF(pucCurrBuf, ("%-30s : %u\n", #eCounter, (UINT_32)prRxCtrl->au8Statistics[eCounter]))

	SPRINTF_RX_COUNTER(RX_MPDU_TOTAL_COUNT);
	SPRINTF_RX_COUNTER(RX_SIZE_ERR_DROP_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_INDICATION_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_RETURNED_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_RETAINED_COUNT);

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	SPRINTF_RX_COUNTER(RX_CSUM_TCP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UDP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_IP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_TCP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UDP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_IP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	SPRINTF_RX_COUNTER(RX_IP_V6_PKT_CCOUNT);
#endif

	/* *pu4Count = (UINT_32)(pucCurrBuf - pucBuffer); */

	nicRxClearStatistics(prAdapter);

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Read the Response data from data port
*
* @param prAdapter pointer to the Adapter handler
* @param pucRspBuffer pointer to the Response buffer
*
* @retval WLAN_STATUS_SUCCESS: Response packet has been read
* @retval WLAN_STATUS_FAILURE: Read Response packet timeout or error occurred
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
nicRxWaitResponse(IN P_ADAPTER_T prAdapter,
		  IN UINT_8 ucPortIdx, OUT PUINT_8 pucRspBuffer, IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length)
{
	UINT_32 u4Value = 0, u4PktLen = 0;
	UINT_32 i = 0;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	BOOLEAN fgResult = TRUE;
	ktime_t rStartTime, rCurTime;
	P_RX_CTRL_T prRxCtrl;

	DEBUGFUNC("nicRxWaitResponse");

	ASSERT(prAdapter);
	ASSERT(pucRspBuffer);
	ASSERT(ucPortIdx < 2);

	rStartTime = ktime_get();
	prRxCtrl = &prAdapter->rRxCtrl;

	do {
		/* Read the packet length */
		HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4Value);

		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read Response Packet Error\n");
			return WLAN_STATUS_FAILURE;
		}

		if (ucPortIdx == 0)
			u4PktLen = u4Value & 0xFFFF;
		else
			u4PktLen = (u4Value >> 16) & 0xFFFF;

/* DBGLOG(RX, TRACE, ("i = %d, u4PktLen = %d\n", i, u4PktLen)); */

		if (u4PktLen == 0) {
			/* timeout exceeding check */
			rCurTime = ktime_get();
			if (ktime_to_ms(ktime_sub(rCurTime, rStartTime)) >
			    RX_RESPONSE_TIMEOUT) {
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
				DBGLOG(RX, ERROR,
				       "RX_RESPONSE_TIMEOUT %u %u %lld %lld\n",
				       u4PktLen, i, rStartTime,
				       rCurTime);
#else
				DBGLOG(RX, ERROR,
				       "RX_RESPONSE_TIMEOUT %u %u %lld %lld\n",
				       u4PktLen, i, rStartTime.tv64,
				       rCurTime.tv64);
#endif
				return WLAN_STATUS_FAILURE;
			}

			/* Response packet is not ready */
			kalUdelay(50);

			i++;
			continue;
		}
		if (u4PktLen > u4MaxRespBufferLen) {
			/*
			 * TO: buffer is not enough but we still need to read all data from HIF to avoid
			 *  HIF crazy.
			 */
			DBGLOG(RX, ERROR,
			       "Not enough Event Buffer: required length = 0x%x, available buffer length = %d\n",
				u4PktLen, u4MaxRespBufferLen);
			DBGLOG(RX, ERROR, "i = %d, u4PktLen = %u\n", i, u4PktLen);
			return WLAN_STATUS_FAILURE;
		}

		wlanFWDLDebugAddRxStartTime(kalGetTimeTick());

		HAL_PORT_RD(prAdapter,
				ucPortIdx == 0 ? MCR_WRDR0 : MCR_WRDR1, u4PktLen,
				prRxCtrl->pucRxCoalescingBufPtr, u4MaxRespBufferLen);

		wlanFWDLDebugAddRxDoneTime(kalGetTimeTick());

		/* fgResult will be updated in MACRO */
		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read Response Packet Error\n");
			return WLAN_STATUS_FAILURE;
		}

		kalMemCopy(pucRspBuffer, prRxCtrl->pucRxCoalescingBufPtr, u4PktLen);
		DBGLOG(RX, TRACE, "Dump Response buffer, length = 0x%x\n", u4PktLen);
		DBGLOG_MEM8(RX, TRACE, pucRspBuffer, u4PktLen);

		*pu4Length = u4PktLen;
		break;
	} while (TRUE);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Set filter to enable Promiscuous Mode
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxEnablePromiscuousMode(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

}				/* end of nicRxEnablePromiscuousMode() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Set filter to disable Promiscuous Mode
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxDisablePromiscuousMode(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

}				/* end of nicRxDisablePromiscuousMode() */

/*----------------------------------------------------------------------------*/
/*!
* @brief this function flushes all packets queued in reordering module
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Flushed successfully
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxFlush(IN P_ADAPTER_T prAdapter)
{
	P_SW_RFB_T prSwRfb;

	ASSERT(prAdapter);

	prSwRfb = qmFlushRxQueues(prAdapter);
	if (prSwRfb != NULL) {
		do {
			P_SW_RFB_T prNextSwRfb;

			/* save next first */
			prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prSwRfb);

			/* free */
			nicRxReturnRFB(prAdapter, prSwRfb);

			prSwRfb = prNextSwRfb;
		} while (prSwRfb);
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param
*
* @retval
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxProcessActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_ACTION_FRAME prActFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (prSwRfb->u2PacketLen < sizeof(WLAN_ACTION_FRAME) - 1)
		return WLAN_STATUS_INVALID_PACKET;
	prActFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;
	DBGLOG(RX, INFO, "Category %u, Action %u\n", prActFrame->ucCategory, prActFrame->ucAction);

	switch (prActFrame->ucCategory) {
	case CATEGORY_QOS_ACTION:
	case CATEGORY_WME_MGT_NOTIFICATION:
		if (prActFrame->ucCategory == CATEGORY_QOS_ACTION) {
			DBGLOG(RX, INFO, "received dscp action frame: %d\n", __LINE__);
			handleQosMapConf(prAdapter, prSwRfb);
		}
		wmmParseQosAction(prAdapter, prSwRfb);

		break;
	case CATEGORY_PUBLIC_ACTION:
		if (prActFrame->ucAction == PUBLIC_ACTION_GAS_INITIAL_REQ) /* GAS Initial Request */
			DBGLOG(RX, INFO, "received GAS Initial Request frame\n");
		else if (prActFrame->ucAction == PUBLIC_ACTION_GAS_INITIAL_RESP) /* GAS Initial Response */
			DBGLOG(RX, INFO, "received GAS Initial Response frame\n");
		if (HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr) == NETWORK_TYPE_AIS_INDEX)
			aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
#if CFG_ENABLE_WIFI_DIRECT
		else if (prAdapter->fgIsP2PRegistered) {
			rlmProcessPublicAction(prAdapter, prSwRfb);

			p2pFuncValidateRxActionFrame(prAdapter, prSwRfb);

		}
#endif
		break;

	case CATEGORY_HT_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			rlmProcessHtAction(prAdapter, prSwRfb);
#endif
		break;
	case CATEGORY_VENDOR_SPECIFIC_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			p2pFuncValidateRxActionFrame(prAdapter, prSwRfb);
#endif
#if CFG_SUPPORT_NCHO
		if (HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr) == NETWORK_TYPE_AIS_INDEX) {
			if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE && prAdapter->rNchoInfo.u4WesMode == TRUE) {
				aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
				DBGLOG(INIT, INFO, "NCHO CATEGORY_VENDOR_SPECIFIC_ACTION\n");
			}
		}
#endif
		break;
#if CFG_SUPPORT_802_11W
	case CATEGORY_SA_QUERT_ACTION:
		{
			P_HIF_RX_HEADER_T prHifRxHdr;

			prHifRxHdr = prSwRfb->prHifRxHdr;

			if ((HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr) == NETWORK_TYPE_AIS_INDEX)
					&& prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection	/* Use MFP */) {
				if (!(prHifRxHdr->ucReserved & CONTROL_FLAG_UC_MGMT_NO_ENC)) {
					DBGLOG(RSN, INFO, "Rx SA Query\n");
					/* MFP test plan 5.3.3.4 */
					rsnSaQueryAction(prAdapter, prSwRfb);
				} else {
					DBGLOG(RSN, WARN, "Un-Protected SA Query, do nothing\n");
				}
			} else if (HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr) ==
				NETWORK_TYPE_P2P_INDEX) {
				P_BSS_INFO_T prBssInfo =
					&(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
				P_STA_RECORD_T prStaRec =
					cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
				if (prBssInfo &&
					prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT &&
					prStaRec) {
					/* AP PMF */
					DBGLOG(RSN, INFO, "[Rx] nicRx AP PMF SAQ action\n");
					if (rsnCheckBipKeyInstalled(prAdapter,
						prStaRec)) {
						/* MFP test plan 4.3.3.4 */
						rsnApSaQueryAction(prAdapter, prSwRfb);
					}
				}
			}
		}
		break;
#endif
#if (CFG_SUPPORT_802_11V || CFG_SUPPORT_PPR2)
	case CATEGORY_WNM_ACTION:
		{
			if (HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr) == NETWORK_TYPE_AIS_INDEX) {
				DBGLOG(RX, INFO, "WNM action frame: %d\n", __LINE__);
				wnmWNMAction(prAdapter, prSwRfb);
			} else
				DBGLOG(RX, INFO, "WNM action frame: %d\n", __LINE__);
		}
		break;
#endif

#if CFG_SUPPORT_DFS		/* Add by Enlai */
	case CATEGORY_SPEC_MGT:
		{
			if (prAdapter->fgEnable5GBand == TRUE)
				rlmProcessSpecMgtAction(prAdapter, prSwRfb);
		}
		break;
#endif

#if CFG_SUPPORT_802_11K
	case CATEGORY_RM_ACTION:
		switch (prActFrame->ucAction) {
		case RM_ACTION_RM_REQUEST:
			rlmProcessRadioMeasurementRequest(prAdapter, prSwRfb);
			break;
		/*case RM_ACTION_LM_REQUEST:
		*	rlmProcessLinkMeasurementRequest(prAdapter, prActFrame);
		*	break;*/ /* Link Measurement is handled in Firmware
		*	rlmProcessLinkMeasurementRequest(prAdapter, prActFrame);
		*	break;
		*/
			/* Link Measurement is handled in Firmware */

		case RM_ACTION_REIGHBOR_RESPONSE:
			rlmProcessNeighborReportResonse(prAdapter, prActFrame, prSwRfb->u2PacketLen);
			break;
	}
	break;
#endif

#if (CFG_SUPPORT_TDLS == 1)
	case 12:		/* shall not be here */
		/*
		 * A received TDLS Action frame with the Type field set to Management shall
		 * be discarded. Note that the TDLS Discovery Response frame is not a TDLS
		 * frame but a Public Action frame.
		 */
		break;
#endif /* CFG_SUPPORT_TDLS */

	default:
		break;
	}			/* end of switch case */

	return WLAN_STATUS_SUCCESS;
}
