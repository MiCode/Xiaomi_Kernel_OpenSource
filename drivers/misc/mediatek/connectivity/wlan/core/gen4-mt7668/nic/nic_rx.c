/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_rx.c#5
*/

/*! \file   nic_rx.c
*    \brief  Functions that provide many rx-related functions
*
*    This file includes the functions used to process RFB and dispatch RFBs to
*    the appropriate related rx functions for protocols.
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
#include "que_mgt.h"

#ifndef LINUX
#include <limits.h>
#else
#include <linux/limits.h>
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
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
#if CFG_SUPPORT_ADHOC || CFG_ENABLE_WIFI_DIRECT
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

static RX_EVENT_HANDLER_T arEventTable[] = {
	{EVENT_ID_RX_ADDBA,					qmHandleEventRxAddBa},
	{EVENT_ID_RX_DELBA,					qmHandleEventRxDelBa},
	{EVENT_ID_CHECK_REORDER_BUBBLE,		qmHandleEventCheckReorderBubble},
	{EVENT_ID_LINK_QUALITY,				nicEventLinkQuality},
	{EVENT_ID_LAYER_0_EXT_MAGIC_NUM,	nicEventLayer0ExtMagic},
	{EVENT_ID_MIC_ERR_INFO,				nicEventMicErrorInfo},
	{EVENT_ID_SCAN_DONE,				nicEventScanDone},
	{EVENT_ID_NLO_DONE,					nicEventNloDone},
	{EVENT_ID_TX_DONE,					nicTxProcessTxDoneEvent},
	{EVENT_ID_SLEEPY_INFO,				nicEventSleepyNotify},
#if CFG_ENABLE_BT_OVER_WIFI
	{EVENT_ID_BT_OVER_WIFI,				nicEventBtOverWifi},
#endif
	{EVENT_ID_STATISTICS,				nicEventStatistics},
	{EVENT_ID_WLAN_INFO,				nicEventWlanInfo},
	{EVENT_ID_MIB_INFO,					nicEventMibInfo},
#if CFG_SUPPORT_LAST_SEC_MCS_INFO
	{EVENT_ID_TX_MCS_INFO,				nicEventTxMcsInfo},
#endif
	{EVENT_ID_CH_PRIVILEGE,				cnmChMngrHandleChEvent},
	{EVENT_ID_BSS_ABSENCE_PRESENCE,		qmHandleEventBssAbsencePresence},
	{EVENT_ID_STA_CHANGE_PS_MODE,		qmHandleEventStaChangePsMode},
	{EVENT_ID_STA_UPDATE_FREE_QUOTA,	qmHandleEventStaUpdateFreeQuota},
	{EVENT_ID_BSS_BEACON_TIMEOUT,		nicEventBeaconTimeout},
	{EVENT_ID_UPDATE_NOA_PARAMS,		nicEventUpdateNoaParams},
	{EVENT_ID_STA_AGING_TIMEOUT,		nicEventStaAgingTimeout},
	{EVENT_ID_AP_OBSS_STATUS,			nicEventApObssStatus},
	{EVENT_ID_ROAMING_STATUS,			nicEventRoamingStatus},
	{EVENT_ID_SEND_DEAUTH,				nicEventSendDeauth},
	{EVENT_ID_UPDATE_RDD_STATUS,		nicEventUpdateRddStatus},
	{EVENT_ID_UPDATE_BWCS_STATUS,		nicEventUpdateBwcsStatus},
	{EVENT_ID_UPDATE_BCM_DEBUG,			nicEventUpdateBcmDebug},
	{EVENT_ID_ADD_PKEY_DONE,			nicEventAddPkeyDone},
	{EVENT_ID_ICAP_DONE,				nicEventIcapDone},
	{EVENT_ID_DEBUG_MSG,				nicEventDebugMsg},
	{EVENT_ID_TDLS,						nicEventTdls},
	{EVENT_ID_DUMP_MEM,					nicEventDumpMem},
#if CFG_ASSERT_DUMP
	{EVENT_ID_ASSERT_DUMP,				nicEventAssertDump},
#endif
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
	{EVENT_ID_CAL_ALL_DONE,				nicEventCalAllDone},
#endif
	{EVENT_ID_HIF_CTRL,					nicEventHifCtrl},
	{EVENT_ID_RDD_SEND_PULSE,			nicEventRddSendPulse},
#if (CFG_SUPPORT_DFS_MASTER == 1)
	{EVENT_ID_UPDATE_COEX_PHYRATE,		nicEventUpdateCoexPhyrate},
	{EVENT_ID_RDD_REPORT,			cnmRadarDetectEvent},
	{EVENT_ID_CSA_DONE,			cnmCsaDoneEvent},
#else
	{EVENT_ID_UPDATE_COEX_PHYRATE,		nicEventUpdateCoexPhyrate},
#endif
#if (CFG_WOW_SUPPORT == 1)
	{EVENT_ID_WOW_WAKEUP_REASON,		nicEventWakeUpReason},
#endif
#if CFG_SUPPORT_CSI
	{EVENT_ID_CSI_DATA, nicEventCSIData},
#endif
#if CFG_SUPPORT_REPLAY_DETECTION
	{EVENT_ID_GET_GTK_REKEY_DATA,       nicEventGetGtkDataSync},
#endif
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define MAX_EAPOL_REQUE 50
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

	DEBUGFUNC("nicRxInitialize");

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;

	/* 4 <0> Clear allocated memory. */
	kalMemZero((PVOID) prRxCtrl->pucRxCached, prRxCtrl->u4RxCachedSize);

	/* 4 <1> Initialize the RFB lists */
	QUEUE_INITIALIZE(&prRxCtrl->rFreeSwRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rReceivedRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rIndicatedRfbList);

	pucMemHandle = prRxCtrl->pucRxCached;
	for (i = CFG_RX_MAX_PKT_NUM; i != 0; i--) {
		prSwRfb = (P_SW_RFB_T) pucMemHandle;

		if (nicRxSetupRFB(prAdapter, prSwRfb)) {
			DBGLOG(RX, ERROR, "nicRxInitialize failed: Cannot allocate packet buffer for SwRfb!\n");
			return;
		}
		nicRxReturnRFB(prAdapter, prSwRfb);

		pucMemHandle += ALIGN_4(sizeof(SW_RFB_T));
	}

	ASSERT(prRxCtrl->rFreeSwRfbList.u4NumElem == CFG_RX_MAX_PKT_NUM);
	/* Check if the memory allocation consist with this initialization function */
	ASSERT((UINT_32) (pucMemHandle - prRxCtrl->pucRxCached) == prRxCtrl->u4RxCachedSize);

	/* 4 <2> Clear all RX counters */
	RX_RESET_ALL_CNTS(prRxCtrl);

	prRxCtrl->pucRxCoalescingBufPtr = prAdapter->pucCoalescingBufCached;

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
		} else {
			break;
		}
	} while (TRUE);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
		} else {
			break;
		}
	} while (TRUE);

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
	P_HW_MAC_RX_DESC_T prRxStatus;

	UINT_32 u4PktLen = 0;
	/* UINT_32 u4MacHeaderLen; */
	UINT_32 u4HeaderOffset;
	UINT_16 u2RxStatusOffset;

	DEBUGFUNC("nicRxFillRFB");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxStatus = prSwRfb->prRxStatus;
	ASSERT(prRxStatus);

	u4PktLen = (UINT_32) HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus);
	u4HeaderOffset = (UINT_32) (HAL_RX_STATUS_GET_HEADER_OFFSET(prRxStatus));
	/* u4MacHeaderLen = (UINT_32)(HAL_RX_STATUS_GET_HEADER_LEN(prRxStatus)); */

	/* DBGLOG(RX, TRACE, ("u4HeaderOffset = %d, u4MacHeaderLen = %d\n", */
	/* u4HeaderOffset, u4MacHeaderLen)); */
	u2RxStatusOffset = sizeof(HW_MAC_RX_DESC_T);
	prSwRfb->ucGroupVLD = (UINT_8) HAL_RX_STATUS_GET_GROUP_VLD(prRxStatus);
	if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_4)) {
		prSwRfb->prRxStatusGroup4 = (P_HW_MAC_RX_STS_GROUP_4_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
		u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_4_T);
		/* Fill out the TID and SSN */
		prSwRfb->ucTid = (UINT_8) (HAL_RX_STATUS_GET_TID(prRxStatus));
		prSwRfb->u2SSN = HAL_RX_STATUS_GET_SEQFrag_NUM(
			prSwRfb->prRxStatusGroup4) >> RX_STATUS_SEQ_NUM_OFFSET;
	}
	if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_1)) {
		prSwRfb->prRxStatusGroup1 = (P_HW_MAC_RX_STS_GROUP_1_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
		u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_1_T);

	}
	if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_2)) {
		prSwRfb->prRxStatusGroup2 = (P_HW_MAC_RX_STS_GROUP_2_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
		u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_2_T);

	}
	if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) {
		prSwRfb->prRxStatusGroup3 = (P_HW_MAC_RX_STS_GROUP_3_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
		u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_3_T);
	}

	prSwRfb->u2RxStatusOffst = u2RxStatusOffset;
	prSwRfb->pvHeader = (PUINT_8) prRxStatus + u2RxStatusOffset + u4HeaderOffset;
	prSwRfb->u2PacketLen = (UINT_16) (u4PktLen - (u2RxStatusOffset + u4HeaderOffset));
	prSwRfb->u2HeaderLen = (UINT_16) HAL_RX_STATUS_GET_HEADER_LEN(prRxStatus);
	prSwRfb->ucWlanIdx = (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus);
	prSwRfb->ucStaRecIdx = secGetStaIdxByWlanIdx(prAdapter, (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus));
	prSwRfb->prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

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
* \brief rxDefragMPDU() is used to defragment the incoming packets.
*
* \param[in] prSWRfb        The RFB which is being processed.
* \param[in] UINT_16     u2FrameCtrl
*
* \retval NOT NULL  Receive the last fragment data
* \retval NULL      Receive the fragment packet which is not the last
*/
/*----------------------------------------------------------------------------*/
P_SW_RFB_T incRxDefragMPDU(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb, OUT P_QUE_T prReturnedQue)
{

	P_SW_RFB_T prOutputSwRfb = (P_SW_RFB_T) NULL;
#if 1
	P_RX_CTRL_T prRxCtrl;
	P_FRAG_INFO_T prFragInfo;
	UINT_32 i = 0, j;
	UINT_16 u2SeqCtrl, u2FrameCtrl;
	UINT_8 ucFragNum;
	BOOLEAN fgFirst = FALSE;
	BOOLEAN fgLast = FALSE;
	OS_SYSTIME rCurrentTime;
	P_WLAN_MAC_HEADER_T prWlanHeader = NULL;
	P_HW_MAC_RX_DESC_T prRxStatus = NULL;
	P_HW_MAC_RX_STS_GROUP_4_T prRxStatusGroup4 = NULL;

	DEBUGFUNC("nicRx: rxmDefragMPDU\n");

	ASSERT(prSWRfb);

	prRxCtrl = &prAdapter->rRxCtrl;

	prRxStatus = prSWRfb->prRxStatus;
	ASSERT(prRxStatus);

	if (HAL_RX_STATUS_IS_HEADER_TRAN(prRxStatus) == FALSE) {
		prWlanHeader = (P_WLAN_MAC_HEADER_T) prSWRfb->pvHeader;
		prSWRfb->u2SequenceControl = prWlanHeader->u2SeqCtrl;
		u2FrameCtrl = prWlanHeader->u2FrameCtrl;
	} else {
		prRxStatusGroup4 = prSWRfb->prRxStatusGroup4;
		prSWRfb->u2SequenceControl = HAL_RX_STATUS_GET_SEQFrag_NUM(prRxStatusGroup4);
		u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(prRxStatusGroup4);
	}
	u2SeqCtrl = prSWRfb->u2SequenceControl;
	ucFragNum = (UINT_8) (u2SeqCtrl & MASK_SC_FRAG_NUM);
	prSWRfb->u2FrameCtrl = u2FrameCtrl;

	if (!(u2FrameCtrl & MASK_FC_MORE_FRAG)) {
		/* The last fragment frame */
		if (ucFragNum) {
			DBGLOG(RX, LOUD,
			       "FC %04x M %04x SQ %04x\n",
				   u2FrameCtrl,
				   (UINT_16)(u2FrameCtrl & MASK_FC_MORE_FRAG),
				   u2SeqCtrl);
			fgLast = TRUE;
		}
		/* Non-fragment frame */
		else
			return prSWRfb;
	}
	/* The fragment frame except the last one */
	else {
		if (ucFragNum == 0) {
			DBGLOG(RX, LOUD,
			       "FC %04x M %04x SQ %04x\n",
				   u2FrameCtrl,
				   (UINT_16)(u2FrameCtrl & MASK_FC_MORE_FRAG),
				   u2SeqCtrl);
			fgFirst = TRUE;
		} else {
			DBGLOG(RX, LOUD,
			       "FC %04x M %04x SQ %04x\n",
				   u2FrameCtrl,
				   (UINT_16)(u2FrameCtrl & MASK_FC_MORE_FRAG),
				   u2SeqCtrl);
		}
	}

	GET_CURRENT_SYSTIME(&rCurrentTime);

	for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
		prFragInfo = &prSWRfb->prStaRec->rFragInfo[j];
		if (prFragInfo->pr1stFrag) {
			/* I. If the receive timer for the MSDU or MMPDU that is stored in the
			 * fragments queue exceeds dot11MaxReceiveLifetime, we discard the
			 * uncompleted fragments.
			 * II. If we didn't receive the last MPDU for a period, we use
			 * this function for remove frames.
			 */
			if (CHECK_FOR_EXPIRATION(rCurrentTime, prFragInfo->rReceiveLifetimeLimit)) {

				/* cnmPktFree((P_PKT_INFO_T)prFragInfo->pr1stFrag, TRUE); */
				prFragInfo->pr1stFrag->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prFragInfo->pr1stFrag);

				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
			}
		}
	}

	for (i = 0; i < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; i++) {

		prFragInfo = &prSWRfb->prStaRec->rFragInfo[i];

		if (fgFirst) {	/* looking for timed-out frag buffer */

			if (prFragInfo->pr1stFrag == (P_SW_RFB_T) NULL)	/* find a free frag buffer */
				break;
		} else {	/* looking for a buffer with desired next seqctrl */

			if (prFragInfo->pr1stFrag == (P_SW_RFB_T) NULL)
				continue;

			if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl)) {
				if (RXM_IS_QOS_DATA_FRAME(prFragInfo->pr1stFrag->u2FrameCtrl)) {
					if (u2SeqCtrl == prFragInfo->u2NextFragSeqCtrl)
						break;
				}
			} else {
				if (!RXM_IS_QOS_DATA_FRAME(prFragInfo->pr1stFrag->u2FrameCtrl)) {
					if (u2SeqCtrl == prFragInfo->u2NextFragSeqCtrl)
						break;
				}
			}
		}
	}

	if (i >= MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS) {

		/* Can't find a proper FRAG_INFO_T.
		 * I. 1st Fragment MPDU, all of the FragInfo are exhausted
		 * II. 2nd ~ (n-1)th Fragment MPDU, can't find the right FragInfo for defragment.
		 * Because we won't process fragment frame outside this function, so
		 * we should free it right away.
		 */
		nicRxReturnRFB(prAdapter, prSWRfb);

		return (P_SW_RFB_T) NULL;
	}

	ASSERT(prFragInfo);

	/* retrieve Rx payload */
	prSWRfb->u2HeaderLen = HAL_RX_STATUS_GET_HEADER_LEN(prRxStatus);
	prSWRfb->pucPayload = (PUINT_8) (((ULONG) prSWRfb->pvHeader) + prSWRfb->u2HeaderLen);
	prSWRfb->u2PayloadLength =
	    (UINT_16) (HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus) - ((ULONG) prSWRfb->pucPayload - (ULONG) prRxStatus));

	if (fgFirst) {
		DBGLOG(RX, LOUD, "rxDefragMPDU first\n");

		SET_EXPIRATION_TIME(prFragInfo->rReceiveLifetimeLimit,
				    TU_TO_SYSTIME(DOT11_RECEIVE_LIFETIME_TU_DEFAULT));

		prFragInfo->pr1stFrag = prSWRfb;

		prFragInfo->pucNextFragStart =
		    (PUINT_8) prSWRfb->pucRecvBuff + HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus);

		prFragInfo->u2NextFragSeqCtrl = u2SeqCtrl + 1;
		DBGLOG(RX, LOUD, "First: nextFragmentSeqCtrl = %04x, u2SeqCtrl = %04x\n",
		       prFragInfo->u2NextFragSeqCtrl, u2SeqCtrl);

		/* prSWRfb->fgFragmented = TRUE; */
		/* whsu: todo for checksum */
	} else {
		prFragInfo->pr1stFrag->prRxStatus->u2RxByteCount += prSWRfb->u2PayloadLength;

		if (prFragInfo->pr1stFrag->prRxStatus->u2RxByteCount > CFG_RX_MAX_PKT_SIZE) {

			prFragInfo->pr1stFrag->eDst = RX_PKT_DESTINATION_NULL;
			QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prFragInfo->pr1stFrag);

			prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;

			nicRxReturnRFB(prAdapter, prSWRfb);
		} else {
			kalMemCopy(prFragInfo->pucNextFragStart, prSWRfb->pucPayload, prSWRfb->u2PayloadLength);
			/* [6630] update rx byte count and packet length */
			prFragInfo->pr1stFrag->u2PacketLen += prSWRfb->u2PayloadLength;
			prFragInfo->pr1stFrag->u2PayloadLength += prSWRfb->u2PayloadLength;

			if (fgLast) {	/* The last one, free the buffer */
				DBGLOG(RX, LOUD, "Defrag: finished\n");

				prOutputSwRfb = prFragInfo->pr1stFrag;

				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
			} else {
				DBGLOG(RX, LOUD, "Defrag: mid fraged\n");

				prFragInfo->pucNextFragStart += prSWRfb->u2PayloadLength;

				prFragInfo->u2NextFragSeqCtrl++;
			}

			nicRxReturnRFB(prAdapter, prSWRfb);
		}
	}

	/* DBGLOG_MEM8(RXM, INFO, */
	/* prFragInfo->pr1stFrag->pucPayload, */
	/* prFragInfo->pr1stFrag->u2PayloadLength); */
#endif
	return prOutputSwRfb;
}				/* end of rxmDefragMPDU() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Do duplicate detection
*
* @param prSwRfb Pointer to the RX packet
*
* @return TRUE: a duplicate, FALSE: not a duplicate
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicRxIsDuplicateFrame(IN OUT P_SW_RFB_T prSwRfb)
{

	/* Non-QoS Unicast Data or Unicast MMPDU: SC Cache #4;
	 *   QoS Unicast Data: SC Cache #0~3;
	 *   Broadcast/Multicast: RetryBit == 0
	 */
	UINT_32 u4SeqCtrlCacheIdx;
	UINT_16 u2SequenceControl, u2FrameCtrl;
	BOOLEAN fgIsDuplicate = FALSE, fgIsAmsduSubframe = FALSE;
	P_WLAN_MAC_HEADER_T prWlanHeader = NULL;
	P_HW_MAC_RX_DESC_T prRxStatus = NULL;
	P_HW_MAC_RX_STS_GROUP_4_T prRxStatusGroup4 = NULL;

	DEBUGFUNC("nicRx: Enter rxmIsDuplicateFrame()\n");

	ASSERT(prSwRfb);

	/* Situations in which the STC_REC is missing include:
	 *   (1) Probe Request (2) (Re)Association Request (3) IBSS data frames (4) Probe Response
	 */
	if (!prSwRfb->prStaRec)
		return FALSE;

	prRxStatus = prSwRfb->prRxStatus;
	ASSERT(prRxStatus);

	fgIsAmsduSubframe = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);
	if (HAL_RX_STATUS_IS_HEADER_TRAN(prRxStatus) == FALSE) {
		prWlanHeader = (P_WLAN_MAC_HEADER_T) prSwRfb->pvHeader;
		u2SequenceControl = prWlanHeader->u2SeqCtrl;
		u2FrameCtrl = prWlanHeader->u2FrameCtrl;
	} else {
		prRxStatusGroup4 = prSwRfb->prRxStatusGroup4;
		u2SequenceControl = HAL_RX_STATUS_GET_SEQFrag_NUM(prRxStatusGroup4);
		u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(prRxStatusGroup4);
	}
	prSwRfb->u2SequenceControl = u2SequenceControl;

	/* Case 1: Unicast QoS data */
	if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl)) {	/* WLAN header shall exist when doing duplicate detection */
		if (prSwRfb->prStaRec->aprRxReorderParamRefTbl[prSwRfb->ucTid]) {

			/* QoS data with an RX BA agreement
			 *  Case 1: The packet is not an AMPDU subframe, so the RetryBit may be set to 1 (TBC).
			 *  Case 2: The RX BA agreement was just established. Some enqueued packets may not be
			 *          sent with aggregation.
			 */

			DBGLOG(RX, LOUD, "RX: SC=0x%X (BA Entry present)\n", u2SequenceControl);

			/* Update the SN cache in order to ensure the correctness of duplicate
			 * removal in case the BA agreement is deleted
			 */
			prSwRfb->prStaRec->au2CachedSeqCtrl[prSwRfb->ucTid] = u2SequenceControl;

			/* debug */
#if 0
			DBGLOG(RXM, LOUD, "RXM: SC= 0x%X (Cache[%d] updated) with BA\n",
			       u2SequenceControl, prSwRfb->ucTID);

			if (g_prMqm->arRxBaTable[prSwRfb->prStaRec->aucRxBaTable[prSwRfb->ucTID]].ucStatus ==
			    BA_ENTRY_STATUS_DELETING) {
				DBGLOG(RXM, LOUD,
				       "RXM: SC= 0x%X (Cache[%d] updated) with DELETING BA ****************\n",
				       u2SequenceControl, prSwRfb->ucTID);
			}
#endif

			/* HW scoreboard shall take care Case 1. Let the layer layer handle Case 2. */
			return FALSE;	/* Not a duplicate */
		}

			if (prSwRfb->prStaRec->ucDesiredPhyTypeSet & (PHY_TYPE_BIT_HT | PHY_TYPE_BIT_VHT)) {
				u4SeqCtrlCacheIdx = prSwRfb->ucTid;
			} else {
				if (prSwRfb->ucTid < 8) {	/* UP = 0~7 */
					u4SeqCtrlCacheIdx = aucTid2ACI[prSwRfb->ucTid];
				} else {
					DBGLOG(RX, WARN,
					       "RXM: (Warning) Unknown QoS Data with TID=%d\n", prSwRfb->ucTid);

					return TRUE;	/* Will be dropped */
				}
			}
	}
	/* Case 2: Unicast non-QoS data or MMPDUs */
	else
		u4SeqCtrlCacheIdx = TID_NUM;

	/* If this is a retransmission */
	if (u2FrameCtrl & MASK_FC_RETRY) {
		if (u2SequenceControl != prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx]) {
			prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx] = u2SequenceControl;
			if (fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU)
				prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx] = TRUE;
			DBGLOG(RX, LOUD,
				"RXM: SC= 0x%X (Cache[%u] updated)\n",
				u2SequenceControl, u4SeqCtrlCacheIdx);
		} else {
			/* A duplicate. */
			if (prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx]) {
				if (fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU)
					prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx] = FALSE;
			} else {
				fgIsDuplicate = TRUE;
				DBGLOG(RX, LOUD,
					"RXM: SC= 0x%X (Cache[%u] duplicate)\n",
				       u2SequenceControl, u4SeqCtrlCacheIdx);
			}
		}
	}

	/* Not a retransmission */
	else {

		prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx] = u2SequenceControl;
		prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx] = FALSE;

		DBGLOG(RX, LOUD,
				"RXM: SC= 0x%X (Cache[%u] updated)\n",
				u2SequenceControl, u4SeqCtrlCacheIdx);
	}

	return fgIsDuplicate;
}

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
	BOOL fgIsRetained = FALSE;
	UINT_32 u4CurrentRxBufferCount;
	KAL_SPIN_LOCK_DECLARATION();
	/* P_STA_RECORD_T prStaRec = (P_STA_RECORD_T)NULL; */

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

#if defined(LINUX)
	fgIsRetained = FALSE;
#else
	fgIsRetained = (((u4CurrentRxBufferCount +
			  qmGetRxReorderQueuedBufferCount(prAdapter) +
			  prTxCtrl->i4PendingFwdFrameCount) < CFG_RX_RETAINED_PKT_THRESHOLD) ? TRUE : FALSE);
#endif

	/* DBGLOG(RX, INFO, ("fgIsRetained = %d\n", fgIsRetained)); */
#if CFG_ENABLE_PER_STA_STATISTICS
	if (prSwRfb->prStaRec && (prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable > 0))
		prSwRfb->prStaRec->u4TotalRxPktsNumber++;
#endif

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	if (kalProcessRxPacket(prAdapter->prGlueInfo,
			       prSwRfb->pvPacket,
			       prSwRfb->pvHeader,
			       (UINT_32) prSwRfb->u2PacketLen, fgIsRetained, prSwRfb->aeCSUM) != WLAN_STATUS_SUCCESS) {
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
		DBGLOG(RX, ERROR, "kalProcessRxPacket return value != WLAN_STATUS_SUCCESS\n");
		ASSERT(0);

		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

#if CFG_SUPPORT_MULTITHREAD
	if (HAL_IS_RX_DIRECT(prAdapter)) {
		kalRxIndicateOnePkt(prAdapter->prGlueInfo,
				    (PVOID) GLUE_GET_PKT_DESCRIPTOR(GLUE_GET_PKT_QUEUE_ENTRY(prSwRfb->pvPacket)));
		RX_ADD_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT, 1);
		if (fgIsRetained)
			RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT, 1);
	} else {
		KAL_SPIN_LOCK_DECLARATION();

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
		QUEUE_INSERT_TAIL(&(prAdapter->rRxQueue), (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prSwRfb->pvPacket));
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

		prRxCtrl->ucNumIndPacket++;
		kalSetTxEvent2Rx(prAdapter->prGlueInfo);
	}
#else
	prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] = prSwRfb->pvPacket;
	prRxCtrl->ucNumIndPacket++;
#endif

#ifndef LINUX
	if (fgIsRetained) {
		prRxCtrl->apvRetainedPacket[prRxCtrl->ucNumRetainedPacket] = prSwRfb->pvPacket;
		prRxCtrl->ucNumRetainedPacket++;
	} else
#endif
		prSwRfb->pvPacket = NULL;


	/* Return RFB */
	if ((!timerPendingTimer(&prAdapter->rPacketDelaySetupTimer))
	   && nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN,
			"Allocate buf failed, Start IndicatedRfb Timer (%u)\n",
			RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
		cnmTimerStartTimer(prAdapter,
			&prAdapter->rPacketDelaySetupTimer,
			SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
	}
	nicRxReturnRFB(prAdapter, prSwRfb);
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

	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (prMsduInfo &&
	    kalProcessRxPacket(prAdapter->prGlueInfo,
			       prSwRfb->pvPacket,
			       prSwRfb->pvHeader,
			       (UINT_32) prSwRfb->u2PacketLen,
			       prRxCtrl->rFreeSwRfbList.u4NumElem <
			       CFG_RX_RETAINED_PKT_THRESHOLD ? TRUE : FALSE, prSwRfb->aeCSUM) == WLAN_STATUS_SUCCESS) {

		/* parsing forward frame */
		wlanProcessTxFrame(prAdapter, (P_NATIVE_PACKET) (prSwRfb->pvPacket));
		/* pack into MSDU_INFO_T */
		nicTxFillMsduInfo(prAdapter, prMsduInfo, (P_NATIVE_PACKET) (prSwRfb->pvPacket));

		prMsduInfo->eSrc = TX_PACKET_FORWARDING;
		prMsduInfo->ucBssIndex = secGetBssIdxByWlanIdx(prAdapter, prSwRfb->ucWlanIdx);

		/* release RX buffer (to rIndicatedRfbList) */
		prSwRfb->pvPacket = NULL;
		nicRxReturnRFB(prAdapter, prSwRfb);

		/* increase forward frame counter */
		GLUE_INC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);

		/* send into TX queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
		prRetMsduInfoList = qmEnqueueTxPackets(prAdapter, prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

		if (prRetMsduInfoList != NULL) {	/* TX queue refuses queuing the packet */
			nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfoList);
			nicTxReturnMsduInfo(prAdapter, prRetMsduInfoList);
		}
		/* indicate service thread for sending */
		if (prTxCtrl->i4PendingFwdFrameCount > 0)
			kalSetEvent(prAdapter->prGlueInfo);
	} else {		/* no TX resource */
		DBGLOG(QM, INFO, "No Tx MSDU_INFO for forwarding frames\n");
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
	P_HW_MAC_RX_DESC_T prRxStatus;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessGOBroadcastPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	prRxStatus = prSwRfb->prRxStatus;
	ASSERT(prRxStatus);

	ASSERT(CFG_NUM_OF_QM_RX_PKT_NUM >= 16);

	if (prRxCtrl->rFreeSwRfbList.u4NumElem
	    >= (CFG_RX_MAX_PKT_NUM - (CFG_NUM_OF_QM_RX_PKT_NUM - 16 /* Reserved for others */))) {

		/* 1. Duplicate SW_RFB_T */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfbDuplicated, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		if (prSwRfbDuplicated) {
			kalMemCopy(prSwRfbDuplicated->pucRecvBuff,
				   prSwRfb->pucRecvBuff, ALIGN_4(prRxStatus->u2RxByteCount + HIF_RX_HW_APPENDED_LEN));

			prSwRfbDuplicated->ucPacketType = RX_PKT_TYPE_RX_DATA;
			prSwRfbDuplicated->ucStaRecIdx = prSwRfb->ucStaRecIdx;
			nicRxFillRFB(prAdapter, prSwRfbDuplicated);

			/* 2. Modify eDst */
			prSwRfbDuplicated->eDst = RX_PKT_DESTINATION_FORWARD;

			/* 4. Forward */
			nicRxProcessForwardPkt(prAdapter, prSwRfbDuplicated);
		}
	} else {
		DBGLOG(RX, WARN,
		       "Stop to forward BMC packet due to less free Sw Rfb %u\n",
			   prRxCtrl->rFreeSwRfbList.u4NumElem);
	}

	/* 3. Indicate to host */
	prSwRfb->eDst = RX_PKT_DESTINATION_HOST;
	nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);

}

#if CFG_SUPPORT_SNIFFER
VOID nicRxFillRadiotapMCS(IN OUT P_MONITOR_RADIOTAP_T prMonitorRadiotap, IN P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3)
{
	UINT_8 ucFrMode;
	UINT_8 ucShortGI;
	UINT_8 ucRxMode;
	UINT_8 ucLDPC;
	UINT_8 ucSTBC;
	UINT_8 ucNess;

	ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET);
	/* VHTA1 B0-B1 */
	ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI) ? 1 : 0;	/* HT_shortgi */
	ucRxMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET);
	ucLDPC = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_LDPC) ? 1 : 0;	/* HT_adcode */
	ucSTBC = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET);	/* HT_stbc */
	ucNess = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_NESS_MASK) >> RX_VT_NESS_OFFSET);	/* HT_extltf */

	prMonitorRadiotap->ucMcsKnown = (BITS(0, 6) | (((ucNess & BIT(1)) >> 1) << 7));

	prMonitorRadiotap->ucMcsFlags = ((ucFrMode) |
					 (ucShortGI << 2) |
					 ((ucRxMode & BIT(0)) << 3) |
					 (ucLDPC << 4) | (ucSTBC << 5) | ((ucNess & BIT(0)) << 7));
	/* Bit[6:0] for 802.11n, mcs0 ~ mcs7 */
	prMonitorRadiotap->ucMcsMcs = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_MASK);
}

VOID nicRxFillRadiotapVHT(IN OUT P_MONITOR_RADIOTAP_T prMonitorRadiotap, IN P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3)
{
	UINT_8 ucSTBC;
	UINT_8 ucTxopPsNotAllow;
	UINT_8 ucShortGI;
	UINT_8 ucNsym;
	UINT_8 ucLdpcExtraOfdmSym;
	UINT_8 ucBeamFormed;
	UINT_8 ucFrMode;
	UINT_8 ucNsts;
	UINT_8 ucMcs;

	prMonitorRadiotap->u2VhtKnown = RADIOTAP_VHT_ALL_KNOWN;
	prMonitorRadiotap->u2VhtKnown &= ~RADIOTAP_VHT_SHORT_GI_NSYM_KNOWN;

	ucSTBC = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET);	/* BIT[7]: VHTA1 B3 */
	ucTxopPsNotAllow = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_TXOP_PS_NOT_ALLOWED) ? 1 : 0;	/* VHTA1 B22 */
	/*
	*ucNsym = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI_NSYM) ? 1 : 0;	//VHTA2 B1
	*/
	ucNsym = 0; /* Invalid in MT6632*/
	ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI) ? 1 : 0;	/* VHTA2 B0 */
	ucLdpcExtraOfdmSym = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_LDPC_EXTRA_OFDM_SYM) ? 1 : 0; /* VHTA2 B3 */
	ucBeamFormed = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_BEAMFORMED) ? 1 : 0;	/* VHTA2 B8 */
	prMonitorRadiotap->ucVhtFlags = ((ucSTBC) |
		(ucTxopPsNotAllow << 1) |
		(ucShortGI << 2) | (ucNsym << 3) | (ucLdpcExtraOfdmSym << 4) | (ucBeamFormed << 5));

	ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET);
	/* VHTA1 B0-B1 */
	switch (ucFrMode) {
	case RX_VT_FR_MODE_20:
		prMonitorRadiotap->ucVhtBandwidth = 0;
		break;
	case RX_VT_FR_MODE_40:
		prMonitorRadiotap->ucVhtBandwidth = 1;
		break;
	case RX_VT_FR_MODE_80:
		prMonitorRadiotap->ucVhtBandwidth = 4;
		break;
	case RX_VT_FR_MODE_160:
		prMonitorRadiotap->ucVhtBandwidth = 11;
		break;
	default:
		prMonitorRadiotap->ucVhtBandwidth = 0;
	}

	/* Set to 0~7 for 1~8 space time streams */
	ucNsts = (((prRxStatusGroup3)->u4RxVector[1] & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET) + 1;
	/* VHTA1 B10-B12 */

	/* Bit[3:0] for 802.11ac, mcs0 ~ mcs9 */
	ucMcs = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_AC_MASK);

	prMonitorRadiotap->aucVhtMcsNss[0] = ((ucMcs << 4) | (ucNsts - ucSTBC));	/* STBC = Nsts - Nss */

	/*
	*prMonitorRadiotap->ucVhtCoding =
	*		(((prRxStatusGroup3)->u4RxVector[0] & RX_VT_CODING_MASK) >> RX_VT_CODING_OFFSET);
	*/
	prMonitorRadiotap->ucVhtCoding = 0; /* Invalid in MT6632*/

	/* VHTA2 B2-B3 */

	prMonitorRadiotap->ucVhtGroupId =
		(((((prRxStatusGroup3)->u4RxVector[1] & RX_VT_GROUPID_1_MASK) >> RX_VT_GROUPID_1_OFFSET) << 2) |
			(((prRxStatusGroup3)->u4RxVector[0] & RX_VT_GROUPID_0_MASK) >> RX_VT_GROUPID_0_OFFSET));
								/* VHTA1 B4-B9 */
	/* VHTA1 B13-B21 */
	prMonitorRadiotap->u2VhtPartialAid = ((((prRxStatusGroup3)->u4RxVector[2] & RX_VT_AID_1_MASK) << 4) |
					      (((prRxStatusGroup3)->u4RxVector[1] & RX_VT_AID_0_MASK) >>
					       RX_VT_AID_0_OFFSET));

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Process HIF monitor packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessMonitorPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	struct sk_buff *prSkb = NULL;
	P_RX_CTRL_T prRxCtrl;
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_HW_MAC_RX_STS_GROUP_2_T prRxStatusGroup2;
	P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3;
	MONITOR_RADIOTAP_T rMonitorRadiotap;
	RADIOTAP_FIELD_VENDOR_T rRadiotapFieldVendor;
	PUINT_8 prVendorNsOffset;
	UINT_32 u4VendorNsLen;
	UINT_32 u4RadiotapLen;
	UINT_32 u4ItPresent;
	UINT_8 aucMtkOui[] = VENDOR_OUI_MTK;
	UINT_8 ucRxRate;
	UINT_8 ucRxMode;
	UINT_8 ucChanNum;
	UINT_8 ucMcs;
	UINT_8 ucFrMode;
	UINT_8 ucShortGI;
	UINT_32 u4PhyRate;

#if CFG_SUPPORT_MULTITHREAD
	KAL_SPIN_LOCK_DECLARATION();
#endif

	DEBUGFUNC("nicRxProcessMonitorPacket");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;

	nicRxFillRFB(prAdapter, prSwRfb);

	/* can't parse radiotap info if no rx vector */
	if (((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_2)) == 0) || ((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) == 0)) {
		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}

	prRxStatus = prSwRfb->prRxStatus;
	prRxStatusGroup2 = prSwRfb->prRxStatusGroup2;
	prRxStatusGroup3 = prSwRfb->prRxStatusGroup3;

	/* Bit Number 30 Vendor Namespace */
	u4VendorNsLen = sizeof(RADIOTAP_FIELD_VENDOR_T);
	rRadiotapFieldVendor.aucOUI[0] = aucMtkOui[0];
	rRadiotapFieldVendor.aucOUI[1] = aucMtkOui[1];
	rRadiotapFieldVendor.aucOUI[2] = aucMtkOui[2];
	rRadiotapFieldVendor.ucSubNamespace = 0;
	rRadiotapFieldVendor.u2DataLen = u4VendorNsLen - 6;
	/* VHTA1 B0-B1 */
	rRadiotapFieldVendor.ucData = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >>
				       RX_VT_FR_MODE_OFFSET);

	ucRxMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET);

	if (ucRxMode == RX_VT_VHT_MODE) {
		u4RadiotapLen = RADIOTAP_LEN_VHT;
		u4ItPresent = RADIOTAP_FIELDS_VHT;
	} else if ((ucRxMode == RX_VT_MIXED_MODE) || (ucRxMode == RX_VT_GREEN_MODE)) {
		u4RadiotapLen = RADIOTAP_LEN_HT;
		u4ItPresent = RADIOTAP_FIELDS_HT;
	} else {
		u4RadiotapLen = RADIOTAP_LEN_LEGACY;
		u4ItPresent = RADIOTAP_FIELDS_LEGACY;
	}

	/* Radiotap Header & Bit Number 30 Vendor Namespace */
	prVendorNsOffset = (PUINT_8) &rMonitorRadiotap + u4RadiotapLen;
	u4RadiotapLen += u4VendorNsLen;
	kalMemSet(&rMonitorRadiotap, 0, sizeof(MONITOR_RADIOTAP_T));
	kalMemCopy(prVendorNsOffset, (PUINT_8) &rRadiotapFieldVendor, u4VendorNsLen);
	rMonitorRadiotap.u2ItLen = cpu_to_le16(u4RadiotapLen);
	rMonitorRadiotap.u4ItPresent = u4ItPresent;

	/* Bit Number 0 TSFT */
	rMonitorRadiotap.u8MacTime = (prRxStatusGroup2->u4Timestamp);

	/* Bit Number 1 FLAGS */
	if (HAL_RX_STATUS_IS_FRAG(prRxStatus) == TRUE)
		rMonitorRadiotap.ucFlags |= BIT(3);

	if (HAL_RX_STATUS_IS_FCS_ERROR(prRxStatus) == TRUE)
		rMonitorRadiotap.ucFlags |= BIT(6);

	/* Bit Number 2 RATE */
	if ((ucRxMode == RX_VT_LEGACY_CCK) || (ucRxMode == RX_VT_LEGACY_OFDM)) {
		/* Bit[2:0] for Legacy CCK, Bit[3:0] for Legacy OFDM */
		ucRxRate = ((prRxStatusGroup3)->u4RxVector[0] & BITS(0, 3));
		rMonitorRadiotap.ucRate = nicGetHwRateByPhyRate(ucRxRate);
	} else {
		ucMcs = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_AC_MASK);
		/* VHTA1 B0-B1 */
		ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET);
		ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI) ? 1 : 0;	/* VHTA2 B0 */

		/* ucRate(500kbs) = u4PhyRate(100kbps) / 5, max ucRate = 0xFF */
		u4PhyRate = nicGetPhyRateByMcsRate(ucMcs, ucFrMode, ucShortGI);
		if (u4PhyRate > 1275)
			rMonitorRadiotap.ucRate = 0xFF;
		else
			rMonitorRadiotap.ucRate = u4PhyRate / 5;
	}

	/* Bit Number 3 CHANNEL */
	if (ucRxMode == RX_VT_LEGACY_CCK)
		rMonitorRadiotap.u2ChFlags |= BIT(5);
	else			/* OFDM */
		rMonitorRadiotap.u2ChFlags |= BIT(6);

	ucChanNum = HAL_RX_STATUS_GET_CHNL_NUM(prRxStatus);
	if (HAL_RX_STATUS_GET_RF_BAND(prRxStatus) == BAND_2G4) {
		rMonitorRadiotap.u2ChFlags |= BIT(7);
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 2407);
	} else {		/* BAND_5G */
		rMonitorRadiotap.u2ChFlags |= BIT(8);
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 5000);
	}

	/* Bit Number 5 ANT SIGNAL */
	rMonitorRadiotap.ucAntennaSignal = RCPI_TO_dBm(HAL_RX_STATUS_GET_RCPI0(prSwRfb->prRxStatusGroup3));

	/* Bit Number 6 ANT NOISE */
	rMonitorRadiotap.ucAntennaNoise = ((((prRxStatusGroup3)->u4RxVector[5] & RX_VT_NF0_MASK) >> 1) + 128);

	/* Bit Number 11 ANT, Invalid for MT6632 and MT7615 */
	rMonitorRadiotap.ucAntenna = ((prRxStatusGroup3)->u4RxVector[2] & RX_VT_SEL_ANT) ? 1 : 0;

	/* Bit Number 19 MCS */
	if ((u4ItPresent & RADIOTAP_FIELD_MCS))
		nicRxFillRadiotapMCS(&rMonitorRadiotap, prRxStatusGroup3);

	/* Bit Number 20 AMPDU */
	if (HAL_RX_STATUS_IS_AMPDU_SUB_FRAME(prRxStatus)) {
		if (HAL_RX_STATUS_GET_RXV_SEQ_NO(prRxStatus))
			++prRxCtrl->u4AmpduRefNum;
		rMonitorRadiotap.u4AmpduRefNum = prRxCtrl->u4AmpduRefNum;
	}

	/* Bit Number 21 VHT */
	if ((u4ItPresent & RADIOTAP_FIELD_VHT))
		nicRxFillRadiotapVHT(&rMonitorRadiotap, prRxStatusGroup3);

	prSwRfb->pvHeader -= u4RadiotapLen;
	kalMemCopy(prSwRfb->pvHeader, &rMonitorRadiotap, u4RadiotapLen);

	prSkb = (struct sk_buff *)(prSwRfb->pvPacket);
	prSkb->data = (unsigned char *)(prSwRfb->pvHeader);

	skb_reset_tail_pointer(prSkb);
	skb_trim(prSkb, 0);
	skb_put(prSkb, (u4RadiotapLen + prSwRfb->u2PacketLen));

#if CFG_SUPPORT_MULTITHREAD
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	QUEUE_INSERT_TAIL(&(prAdapter->rRxQueue), (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prSwRfb->pvPacket));
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

	prRxCtrl->ucNumIndPacket++;
	kalSetTxEvent2Rx(prAdapter->prGlueInfo);
#else
	prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] = prSwRfb->pvPacket;
	prRxCtrl->ucNumIndPacket++;
#endif

	prSwRfb->pvPacket = NULL;
	/* Return RFB */
	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN, "Cannot allocate packet buffer for SwRfb!\n");
		if (!timerPendingTimer(&prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN, "Start ReturnIndicatedRfb Timer (%u)\n", RX_RETURN_INDICATED_RFB_TIMEOUT_SEC);
			cnmTimerStartTimer(prAdapter, &prAdapter->rPacketDelaySetupTimer,
					   SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
		}
	}
	nicRxReturnRFB(prAdapter, prSwRfb);
}
#endif

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
	P_HW_MAC_RX_DESC_T prRxStatus;
	BOOLEAN fgDrop;
	struct mt66xx_chip_info *prChipInfo;

	DEBUGFUNC("nicRxProcessDataPacket");
	/* DBGLOG(INIT, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	fgDrop = FALSE;

	prRxStatus = prSwRfb->prRxStatus;
	prRxCtrl = &prAdapter->rRxCtrl;
	prChipInfo = prAdapter->chip_info;

	/* Check AMPDU_nERR_Bitmap */
	prSwRfb->fgDataFrame = TRUE;
	prSwRfb->fgFragFrame = FALSE;
	prSwRfb->fgReorderBuffer = FALSE;

	/* BA session */
	if ((prRxStatus->u2StatusFlag & RXS_DW2_AMPDU_nERR_BITMAP) == RXS_DW2_AMPDU_nERR_VALUE)
		prSwRfb->fgReorderBuffer = TRUE;
	/* non BA session */
	else if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_nERR_BITMAP) == RXS_DW2_RX_nERR_VALUE) {
		if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_nDATA_BITMAP) == RXS_DW2_RX_nDATA_VALUE)
			prSwRfb->fgDataFrame = FALSE;

		if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_FRAG_BITMAP) == RXS_DW2_RX_FRAG_VALUE)
			prSwRfb->fgFragFrame = TRUE;

	} else {
		fgDrop = TRUE;
		if (!HAL_RX_STATUS_IS_ICV_ERROR(prRxStatus)
		    && HAL_RX_STATUS_IS_TKIP_MIC_ERROR(prRxStatus)) {
			P_STA_RECORD_T prStaRec;

			prStaRec = cnmGetStaRecByAddress(prAdapter,
							 prAdapter->prAisBssInfo->ucBssIndex,
							 prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
			if (prStaRec) {
				DBGLOG(RSN, EVENT, "MIC_ERR_PKT\n");
				rsnTkipHandleMICFailure(prAdapter, prStaRec, 0);
			}
		}
#if UNIFIED_MAC_RX_FORMAT
		else if (HAL_RX_STATUS_IS_LLC_MIS(prRxStatus)
				&& !HAL_RX_STATUS_IS_ERROR(prRxStatus)
				&& !FEAT_SUP_LLC_VLAN_RX(prChipInfo)) {
			PUINT_16 pu2EtherType;

			nicRxFillRFB(prAdapter, prSwRfb);

			pu2EtherType = (PUINT_16)((PUINT_8)prSwRfb->pvHeader + 2*MAC_ADDR_LEN);

			/* If ethernet type is VLAN, do not drop it. Pass up to driver process */
			if (prSwRfb->u2HeaderLen >= ETH_HLEN
				&& *pu2EtherType == NTOHS(ETH_P_VLAN))
				fgDrop = FALSE;
		}
#else
		else if (HAL_RX_STATUS_IS_LLC_MIS(prRxStatus)) {
			DBGLOG(RSN, EVENT, ("LLC_MIS_ERR\n"));
			fgDrop = FALSE;	/* Drop after send de-auth  */
		}
#endif
	}

#if 0				/* Check 1x Pkt */
	if (prSwRfb->u2PacketLen > 14) {
		PUINT_8 pc = (PUINT_8) prSwRfb->pvHeader;
		UINT_16 u2Etype = 0;

		u2Etype = (pc[ETHER_TYPE_LEN_OFFSET] << 8) | (pc[ETHER_TYPE_LEN_OFFSET + 1]);

#if CFG_SUPPORT_WAPI
		if (u2Etype == ETH_P_1X || u2Etype == ETH_WPI_1X)
			DBGLOG(RSN, INFO, "R1X len=%d\n", prSwRfb->u2PacketLen);
#else
		if (u2Etype == ETH_P_1X)
			DBGLOG(RSN, INFO, "R1X len=%d\n", prSwRfb->u2PacketLen);
#endif
		else if (u2Etype == ETH_P_PRE_1X)
			DBGLOG(RSN, INFO, "Pre R1X len=%d\n", prSwRfb->u2PacketLen);
	}
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	if (prAdapter->fgIsSupportCsumOffload && fgDrop == FALSE) {
		UINT_32 u4TcpUdpIpCksStatus;
		PUINT_32 pu4Temp;

		pu4Temp = (PUINT_32) prRxStatus;
		u4TcpUdpIpCksStatus = *(pu4Temp + (ALIGN_4(prRxStatus->u2RxByteCount) >> 2));
		nicRxFillChksumStatus(prAdapter, prSwRfb, u4TcpUdpIpCksStatus);
	}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	/* if(secCheckClassError(prAdapter, prSwRfb, prStaRec) == TRUE && */
	if (prAdapter->fgTestMode == FALSE && fgDrop == FALSE) {
		PUINT_16 pu2EtherType;
		P_STA_RECORD_T prStaRec;
#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4QueuedCnt++;
#endif
		nicRxFillRFB(prAdapter, prSwRfb);
		GLUE_SET_PKT_BSS_IDX(prSwRfb->pvPacket, secGetBssIdxByWlanIdx(prAdapter, prSwRfb->ucWlanIdx));
		pu2EtherType =
			(PUINT_16)((PUINT_8)prSwRfb->pvHeader + 2*MAC_ADDR_LEN);
		prStaRec = prSwRfb->prStaRec;

		/* If receive EAPOL frames of sta record != STA_STATE_3
		 * Re-enqueu it of MAX_EAPOL_RE_ENQUEUE_CNT times
		 * Otherwise, the EAPOL frames might drop in the wpa_supp
		 */
		if (prSwRfb->u2HeaderLen >= ETH_HLEN
			&& *pu2EtherType == NTOHS(ETH_P_1X)
			&& prStaRec != NULL
			&& prStaRec->ucStaState != STA_STATE_3
			&& prStaRec->eapol_re_enqueue_cnt++ < MAX_EAPOL_REQUE) {
			KAL_SPIN_LOCK_DECLARATION();
			DBGLOG(RSN, WARN,
				"Re-enqueue EAPOL frame: State[%u], retry[%d]\n",
				prStaRec->ucStaState,
				prStaRec->eapol_re_enqueue_cnt);
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_INSERT_TAIL(&prAdapter->rRxCtrl.rReceivedRfbList,
				&prSwRfb->rQueEntry);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			/* Wake up Rx handling thread */
			set_bit(GLUE_FLAG_RX_BIT,
				&(prAdapter->prGlueInfo->ulFlag));
			wake_up_interruptible(&(prAdapter->prGlueInfo->waitq));
			return;
		}

		if ((prSwRfb->prStaRec != NULL)
			&& (prSwRfb->ucStaRecIdx < CFG_STA_REC_NUM)
			&& (prSwRfb->prStaRec->ucStaState < STA_STATE_3)) {
			nicRxReturnRFB(prAdapter, prSwRfb);
			RX_INC_CNT(prRxCtrl, RX_CLASS_ERR_DROP_COUNT);
			RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
			DBGLOG(RX, ERROR, "staRec < state3 [%d] drop\n",
				prSwRfb->prStaRec->ucStaState);
			return;
		}

		prRetSwRfb = qmHandleRxPackets(prAdapter, prSwRfb);
		if (prRetSwRfb != NULL) {
			do {
#if CFG_SUPPORT_MSP
				if (prRetSwRfb->ucGroupVLD &
					BIT(RX_GROUP_VLD_3) &&
					(prRetSwRfb->ucStaRecIdx <
					CFG_STA_REC_NUM)) {
					prAdapter->arStaRec
					[prRetSwRfb->ucStaRecIdx].
					u4RxVector0 =
					HAL_RX_VECTOR_GET_RX_VECTOR
					(prRetSwRfb->
					prRxStatusGroup3, 0);

					prAdapter->arStaRec
					[prRetSwRfb->ucStaRecIdx].
					u4RxVector1 =
					HAL_RX_VECTOR_GET_RX_VECTOR
					(prRetSwRfb->
					prRxStatusGroup3, 1);

					prAdapter->arStaRec
					[prRetSwRfb->ucStaRecIdx].
					u4RxVector2 =
					HAL_RX_VECTOR_GET_RX_VECTOR
					(prRetSwRfb->
					prRxStatusGroup3, 2);

					prAdapter->arStaRec
					[prRetSwRfb->ucStaRecIdx].
					u4RxVector3 =
					HAL_RX_VECTOR_GET_RX_VECTOR
					(prRetSwRfb->
					prRxStatusGroup3, 3);

					prAdapter->arStaRec
					[prRetSwRfb->ucStaRecIdx].
					u4RxVector4 =
					HAL_RX_VECTOR_GET_RX_VECTOR
					(prRetSwRfb->
					prRxStatusGroup3, 4);
				}
#endif
				/* save next first */
				prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prRetSwRfb);

				switch (prRetSwRfb->eDst) {
				case RX_PKT_DESTINATION_HOST:
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

#if 1
VOID nicRxProcessEventPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_CMD_INFO_T prCmdInfo;
	P_WIFI_EVENT_T prEvent;
	UINT_32 u4Idx, u4Size;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;

	if (prEvent->ucEID != EVENT_ID_DEBUG_MSG && prEvent->ucEID != EVENT_ID_ASSERT_DUMP) {
		DBGLOG(NIC, INFO, "RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
			prEvent->ucEID, prEvent->ucSeqNum, prEvent->u2PacketLength);
	}

	/* Event handler table */
	u4Size = sizeof(arEventTable) / sizeof(RX_EVENT_HANDLER_T);

	for (u4Idx = 0; u4Idx < u4Size; u4Idx++) {
		if (prEvent->ucEID == arEventTable[u4Idx].eEID) {
			arEventTable[u4Idx].pfnHandler(prAdapter, prEvent);

			break;
		}
	}

	/* Event cannot be found in event handler table, use default action */
	if (u4Idx >= u4Size) {
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);

			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else {
			DBGLOG(RX, WARN, "UNHANDLED RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
				prEvent->ucEID, prEvent->ucSeqNum, prEvent->u2PacketLength);
		}
	}

	/* Reset Chip NoAck flag */
	if (prAdapter->fgIsChipNoAck) {
		DBGLOG(RX, WARN, "Got response from chip, clear NoAck flag!\n");
		WARN_ON(TRUE);
	}
	prAdapter->ucOidTimeoutCount = 0;
	prAdapter->fgIsChipNoAck = FALSE;

	nicRxReturnRFB(prAdapter, prSwRfb);
}
#else
BOOLEAN fgKeepPrintCoreDump = FALSE;
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
	/* P_MSDU_INFO_T prMsduInfo; */
	P_WIFI_EVENT_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	BOOLEAN fgIsNewVersion;
/*#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)*/
	UINT_32 u4QueryInfoLen;
/*#endif*/
/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
	P_EVENT_ACCESS_EFUSE prEventEfuseAccess;
	P_EVENT_EFUSE_FREE_BLOCK_T prEventGetFreeBlock;
/*#endif*/
	DEBUGFUNC("nicRxProcessEventPacket");
	/* DBGLOG(INIT, TRACE, ("\n")); */

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
	prGlueInfo = prAdapter->prGlueInfo;

	if (prEvent->ucEID != EVENT_ID_DEBUG_MSG
#if CFG_ASSERT_DUMP
	    && prEvent->ucEID != EVENT_ID_ASSERT_DUMP
#endif
	    ) {
		DBGLOG(INIT, INFO, "RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
		       prEvent->ucEID, prEvent->ucSeqNum, prEvent->u2PacketLength);
	}

	/* Event Handling */
	switch (prEvent->ucEID) {
#if 0				/* It is removed now */
	case EVENT_ID_CMD_RESULT:
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			P_EVENT_CMD_RESULT prCmdResult;

			prCmdResult = (P_EVENT_CMD_RESULT) ((PUINT_8) prEvent + EVENT_HDR_SIZE);

			/* CMD_RESULT should be only in response to Set commands */
			ASSERT(prCmdInfo->fgSetQuery == FALSE || prCmdInfo->fgNeedResp == TRUE);

			if (prCmdResult->ucStatus == 0) {	/* success */
				if (prCmdInfo->pfCmdDoneHandler) {
					prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
				} else if (prCmdInfo->fgIsOid == TRUE) {
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_SUCCESS);
				}
			} else if (prCmdResult->ucStatus == 1) {	/* reject */
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_FAILURE);
			} else if (prCmdResult->ucStatus == 2) {	/* unknown CMD */
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_NOT_SUPPORTED);
			}
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
#endif

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
			} else if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_CONNECTED) {	/* connected */
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

	case EVENT_ID_CHECK_REORDER_BUBBLE:
		qmHandleEventCheckReorderBubble(prAdapter, prEvent);
		break;

	case EVENT_ID_LINK_QUALITY:
#if CFG_ENABLE_WIFI_DIRECT && CFG_SUPPORT_P2P_RSSI_QUERY
		if (prEvent->u2PacketLen == EVENT_HDR_SIZE + sizeof(EVENT_LINK_QUALITY_EX)) {
			P_EVENT_LINK_QUALITY_EX prLqEx = (P_EVENT_LINK_QUALITY_EX) (prEvent->aucBuffer);

			if (prLqEx->ucIsLQ0Rdy)
				nicUpdateLinkQuality(prAdapter, 0, (P_EVENT_LINK_QUALITY) prLqEx);
			if (prLqEx->ucIsLQ1Rdy)
				nicUpdateLinkQuality(prAdapter, 1, (P_EVENT_LINK_QUALITY) prLqEx);
		} else {
			/* For old FW, P2P may invoke link quality query, and make driver flag becone TRUE. */
			DBGLOG(P2P, WARN, "Old FW version, not support P2P RSSI query.\n");

			/* Must not use NETWORK_TYPE_P2P_INDEX, cause the structure is mismatch. */
			nicUpdateLinkQuality(prAdapter, 0, (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
		}
#else
		/*only support ais query */
		{
			UINT_8 ucBssIndex;
			P_BSS_INFO_T prBssInfo;

			for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
				prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

				if ((prBssInfo->eNetworkType == NETWORK_TYPE_AIS)
				    && (prBssInfo->fgIsInUse))
					break;
			}

			if (ucBssIndex >= BSS_INFO_NUM)
				ucBssIndex = 1;	/* No hit(bss1 for default ais network) */
			/* printk("=======> rssi with bss%d ,%d\n",ucBssIndex,
			 * ((P_EVENT_LINK_QUALITY_V2)(prEvent->aucBuffer))->rLq[ucBssIndex].cRssi);
			 */
			nicUpdateLinkQuality(prAdapter, ucBssIndex, (P_EVENT_LINK_QUALITY_V2) (prEvent->aucBuffer));
		}

#endif

		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
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

/*#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)*/
	case EVENT_ID_LAYER_0_EXT_MAGIC_NUM:
		if ((prEvent->ucExtenEID) == EXT_EVENT_ID_CMD_RESULT) {

			u4QueryInfoLen = sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T);

			prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

			if (prCmdInfo != NULL) {
				if ((prCmdInfo->fgIsOid) != 0) {
					kalOidComplete(prAdapter->prGlueInfo,
									prCmdInfo->fgSetQuery,
									u4QueryInfoLen,
									WLAN_STATUS_SUCCESS);
					/* return prCmdInfo */
					cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
				}
			}
		}
/*#if  (CFG_EEPROM_PAGE_ACCESS == 1)*/

		else if ((prEvent->ucExtenEID) == EXT_EVENT_ID_CMD_EFUSE_ACCESS) {
			u4QueryInfoLen = sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T);
			prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);
			prEventEfuseAccess = (P_EVENT_ACCESS_EFUSE) (prEvent->aucBuffer);

			/* Efuse block size 16 */
			kalMemCopy(prAdapter->aucEepromVaule, prEventEfuseAccess->aucData, 16);

			if (prCmdInfo != NULL) {
				if ((prCmdInfo->fgIsOid) != 0) {
					kalOidComplete(prAdapter->prGlueInfo,
									prCmdInfo->fgSetQuery,
									u4QueryInfoLen,
									WLAN_STATUS_SUCCESS);
					/* return prCmdInfo */
					cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

				}
			}
		}

		else if ((prEvent->ucExtenEID) == EXT_EVENT_ID_EFUSE_FREE_BLOCK) {
			u4QueryInfoLen = sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T);
			prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);
			prEventGetFreeBlock = (P_EVENT_EFUSE_FREE_BLOCK_T) (prEvent->aucBuffer);
			prAdapter->u4FreeBlockNum = prEventGetFreeBlock->u2FreeBlockNum;

			if (prCmdInfo != NULL) {
				if ((prCmdInfo->fgIsOid) != 0) {
					kalOidComplete(prAdapter->prGlueInfo,
									prCmdInfo->fgSetQuery,
									u4QueryInfoLen,
									WLAN_STATUS_SUCCESS);
					/* return prCmdInfo */
					cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
				}
			}
		}
/*#endif*/
		break;
/*#endif*/

	case EVENT_ID_MIC_ERR_INFO:
		{
			P_EVENT_MIC_ERR_INFO prMicError;
			/* P_PARAM_AUTH_EVENT_T prAuthEvent; */
			P_STA_RECORD_T prStaRec;

			DBGLOG(RSN, EVENT, "EVENT_ID_MIC_ERR_INFO\n");

			prMicError = (P_EVENT_MIC_ERR_INFO) (prEvent->aucBuffer);
			prStaRec = cnmGetStaRecByAddress(prAdapter,
							 prAdapter->prAisBssInfo->ucBssIndex,
							 prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
			ASSERT(prStaRec);

			if (prStaRec)
				rsnTkipHandleMICFailure(prAdapter, prStaRec, (BOOLEAN) prMicError->u4Flags);
			else
				DBGLOG(RSN, INFO, "No STA rec!!\n");
#if 0
			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

			/* Status type: Authentication Event */
			prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

			/* Authentication request */
			prAuthEvent->arRequest[0].u4Length = sizeof(PARAM_AUTH_REQUEST_T);
			kalMemCopy((PVOID) prAuthEvent->arRequest[0].arBssid,
					(PVOID) prAdapter->rWlanInfo.rCurrBssId.arMacAddress,	/* whsu:Todo? */
				   PARAM_MAC_ADDR_LEN);

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

#if 0				/* Marked for MT6630 */
	case EVENT_ID_ASSOC_INFO:
		{
			P_EVENT_ASSOC_INFO prAssocInfo;

			prAssocInfo = (P_EVENT_ASSOC_INFO) (prEvent->aucBuffer);

			kalHandleAssocInfo(prAdapter->prGlueInfo, prAssocInfo);
		}
		break;

	case EVENT_ID_802_11_PMKID:
		{
			P_PARAM_AUTH_EVENT_T prAuthEvent;
			PUINT_8 cp;
			UINT_32 u4LenOfUsedBuffer;

			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

			prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;

			u4LenOfUsedBuffer = (UINT_32) (prEvent->u2PacketLength - 8);

			prAuthEvent->arRequest[0].u4Length = u4LenOfUsedBuffer;

			cp = (PUINT_8) &prAuthEvent->arRequest[0];

			/* Status type: PMKID Candidatelist Event */
			kalMemCopy(cp, (P_EVENT_PMKID_CANDIDATE_LIST_T) (prEvent->aucBuffer),
				   prEvent->u2PacketLength - 8);

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

			DbgPrint("RX EVENT: EVENT_ID_ACTIVATE_STA_REC_T Index:%d, MAC:[" MACSTR
				 "]\n", prActivateStaRec->ucStaRecIdx, MAC2STR(prActivateStaRec->aucMacAddr));

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

			DbgPrint("RX EVENT: EVENT_ID_DEACTIVATE_STA_REC_T Index:%d, MAC:[" MACSTR
				 "]\n", prDeactivateStaRec->ucStaRecIdx);

			qmDeactivateStaRec(prAdapter, prDeactivateStaRec->ucStaRecIdx);
		}
		break;
#endif

	case EVENT_ID_SCAN_DONE:
		fgIsNewVersion = TRUE;
		scnEventScanDone(prAdapter, (P_EVENT_SCAN_DONE) (prEvent->aucBuffer), fgIsNewVersion);
		break;

	case EVENT_ID_NLO_DONE:
		DBGLOG(INIT, INFO, "EVENT_ID_NLO_DONE\n");
		scnEventNloDone(prAdapter, (P_EVENT_NLO_DONE_T) (prEvent->aucBuffer));
#if CFG_SUPPORT_PNO
		prAdapter->prAisBssInfo->fgIsPNOEnable = FALSE;
		if (prAdapter->prAisBssInfo->fgIsNetRequestInActive && prAdapter->prAisBssInfo->fgIsPNOEnable) {
			UNSET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
			DBGLOG(INIT, INFO, "INACTIVE  AIS from  ACTIVEto disable PNO\n");
			/* sync with firmware */
			nicDeactivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
		}
#endif
		break;

	case EVENT_ID_TX_DONE:
#if 1
		nicTxProcessTxDoneEvent(prAdapter, prEvent);
#else
		{
			P_EVENT_TX_DONE_T prTxDone;

			prTxDone = (P_EVENT_TX_DONE_T) (prEvent->aucBuffer);

			DBGLOG(INIT, INFO, "EVENT_ID_TX_DONE WIDX:PID[%u:%u] Status[%u] SN[%u]\n",
			       prTxDone->ucWlanIndex, prTxDone->ucPacketSeq, prTxDone->ucStatus,
			       prTxDone->u2SequenceNumber);

			/* call related TX Done Handler */
			prMsduInfo = nicGetPendingTxMsduInfo(prAdapter, prTxDone->ucWlanIndex, prTxDone->ucPacketSeq);

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
			DBGLOG(INIT, TRACE, "EVENT_ID_TX_DONE u4TimeStamp = %x u2AirDelay = %x\n",
			       prTxDone->au4Reserved1, prTxDone->au4Reserved2);

			wnmReportTimingMeas(prAdapter, prMsduInfo->ucStaRecIndex,
					    prTxDone->au4Reserved1, prTxDone->au4Reserved1 + prTxDone->au4Reserved2);
#endif

			if (prMsduInfo) {
				prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
							    (ENUM_TX_RESULT_CODE_T) (prTxDone->ucStatus));

				if (prMsduInfo->eSrc == TX_PACKET_MGMT)
					cnmMgtPktFree(prAdapter, prMsduInfo);
				else
					nicTxReturnMsduInfo(prAdapter, prMsduInfo);
			}
		}
#endif
		break;

	case EVENT_ID_SLEEPY_INFO:
#if defined(_HIF_USB)
#else
		{
			P_EVENT_SLEEPY_INFO_T prEventSleepyNotify;

			prEventSleepyNotify = (P_EVENT_SLEEPY_INFO_T) (prEvent->aucBuffer);

			/* DBGLOG(RX, INFO, ("ucSleepyState = %d\n", prEventSleepyNotify->ucSleepyState)); */

			prAdapter->fgWiFiInSleepyState = (BOOLEAN) (prEventSleepyNotify->ucSleepyState);

#if CFG_SUPPORT_MULTITHREAD
			if (prEventSleepyNotify->ucSleepyState)
				kalSetFwOwnEvent2Hif(prGlueInfo);
#endif
		}
#endif
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
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

#if CFG_SUPPORT_MSP
	case EVENT_ID_WLAN_INFO:
		/* buffer statistics for further query */
		prAdapter->fgIsStatValid = TRUE;
		prAdapter->rStatUpdateTime = kalGetTimeTick();
		kalMemCopy(&prAdapter->rEventWlanInfo, prEvent->aucBuffer, sizeof(EVENT_WLAN_INFO));

		DBGLOG(RSN, INFO, "EVENT_ID_WLAN_INFO");
		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

	case EVENT_ID_MIB_INFO:
		/* buffer statistics for further query */
		prAdapter->fgIsStatValid = TRUE;
		prAdapter->rStatUpdateTime = kalGetTimeTick();

		DBGLOG(RSN, INFO, "EVENT_ID_MIB_INFO");
		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
#endif
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
		DBGLOG(INIT, INFO, "EVENT_ID_BSS_BEACON_TIMEOUT\n");

		if (prAdapter->fgDisBcnLostDetection == FALSE) {
			P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
			P_EVENT_BSS_BEACON_TIMEOUT_T prEventBssBeaconTimeout;

			prEventBssBeaconTimeout = (P_EVENT_BSS_BEACON_TIMEOUT_T) (prEvent->aucBuffer);

			if (prEventBssBeaconTimeout->ucBssIndex >= BSS_INFO_NUM)
				break;

			DBGLOG(INIT, INFO, "Reason code: %d\n", prEventBssBeaconTimeout->ucReasonCode);

			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prEventBssBeaconTimeout->ucBssIndex);

			if (prEventBssBeaconTimeout->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex)
				aisBssBeaconTimeout(prAdapter);
#if CFG_ENABLE_WIFI_DIRECT
			else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P)
				p2pRoleFsmRunEventBeaconTimeout(prAdapter, prBssInfo);
#endif
#if CFG_ENABLE_BT_OVER_WIFI
			else if (GET_BSS_INFO_BY_INDEX(prAdapter, prEventBssBeaconTimeout->ucBssIndex)->eNetworkType ==
				 NETWORK_TYPE_BOW) {
				/* ToDo:: Nothing */
			}
#endif
			else {
				DBGLOG(RX, ERROR,
				       "EVENT_ID_BSS_BEACON_TIMEOUT: (ucBssIndex = %d)\n",
				       prEventBssBeaconTimeout->ucBssIndex);
			}
		}

		break;
	case EVENT_ID_UPDATE_NOA_PARAMS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam;

			prEventUpdateNoaParam = (P_EVENT_UPDATE_NOA_PARAMS_T) (prEvent->aucBuffer);

			if (GET_BSS_INFO_BY_INDEX(prAdapter, prEventUpdateNoaParam->ucBssIndex)->eNetworkType ==
			    NETWORK_TYPE_P2P) {
				p2pProcessEvent_UpdateNOAParam(prAdapter, prEventUpdateNoaParam->ucBssIndex,
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

				DBGLOG(INIT, INFO, "EVENT_ID_STA_AGING_TIMEOUT %u " MACSTR "\n",
				       prEventStaAgingTimeout->ucStaRecIdx, MAC2STR(prStaRec->aucMacAddr));

				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

				bssRemoveClient(prAdapter, prBssInfo, prStaRec);

				/* Call False Auth */
				if (prAdapter->fgIsP2PRegistered) {
					p2pFuncDisconnect(prAdapter, prBssInfo, prStaRec, TRUE,
							  REASON_CODE_DISASSOC_INACTIVITY);
				}

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
			P_CMD_ROAMING_TRANSIT_T prTransit;

			prTransit = (P_CMD_ROAMING_TRANSIT_T) (prEvent->aucBuffer);
			roamingFsmProcessEvent(prAdapter, prTransit);
		}
#endif /* CFG_SUPPORT_ROAMING */
		break;
	case EVENT_ID_SEND_DEAUTH:
#if DBG
		{
			P_WLAN_MAC_HEADER_T prWlanMacHeader;

			prWlanMacHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
			DBGLOG(RX, INFO, "nicRx: aucAddr1: " MACSTR "\n", MAC2STR(prWlanMacHeader->aucAddr1));
			DBGLOG(RX, INFO, "nicRx: aucAddr2: " MACSTR "\n", MAC2STR(prWlanMacHeader->aucAddr2));
		}
#endif
		/* receive packets without StaRec */
		prSwRfb->pvHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
		if (authSendDeauthFrame(prAdapter,
						NULL,
						NULL,
						prSwRfb,
						REASON_CODE_CLASS_3_ERR,
						(PFN_TX_DONE_HANDLER) NULL) == WLAN_STATUS_SUCCESS) {
			DBGLOG(RX, INFO, "Send Deauth Error\n");
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
			DBGLOG(RSN, EVENT, "BCM BWCS Event: %02x%02x%02x%02x\n",
			       prEventBwcsStatus->u.aucBTPParams[0],
			       prEventBwcsStatus->u.aucBTPParams[1],
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
			DBGLOG(RSN, EVENT, "BCM FW status: %02x%02x%02x%02x\n",
			       prEventBwcsStatus->u.aucBTPParams[0],
			       prEventBwcsStatus->u.aucBTPParams[1],
			       prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);
#endif
		}

		break;
#endif
	case EVENT_ID_ADD_PKEY_DONE:
		{
			P_EVENT_ADD_KEY_DONE_INFO prAddKeyDone;
			P_STA_RECORD_T prStaRec;

			prAddKeyDone = (P_EVENT_ADD_KEY_DONE_INFO) (prEvent->aucBuffer);

			DBGLOG(RSN, EVENT,
			       "EVENT_ID_ADD_PKEY_DONE BSSIDX=%d " MACSTR "\n",
			       prAddKeyDone->ucBSSIndex, MAC2STR(prAddKeyDone->aucStaAddr));

			prStaRec = cnmGetStaRecByAddress(prAdapter, prAddKeyDone->ucBSSIndex, prAddKeyDone->aucStaAddr);

			if (prStaRec) {
				DBGLOG(RSN, EVENT, "STA " MACSTR " Add Key Done!!\n", MAC2STR(prStaRec->aucMacAddr));
				prStaRec->fgIsTxKeyReady = TRUE;
				qmUpdateStaRec(prAdapter, prStaRec);
			}
		}
		break;
	case EVENT_ID_ICAP_DONE:
		{
			P_EVENT_ICAP_STATUS_T prEventIcapStatus;
			PARAM_CUSTOM_MEM_DUMP_STRUCT_T rMemDumpInfo;
			UINT_32 u4QueryInfo;

			prEventIcapStatus = (P_EVENT_ICAP_STATUS_T) (prEvent->aucBuffer);

			rMemDumpInfo.u4Address = prEventIcapStatus->u4StartAddress;
			rMemDumpInfo.u4Length = prEventIcapStatus->u4IcapSieze;
#if CFG_SUPPORT_QA_TOOL
			rMemDumpInfo.u4IcapContent = prEventIcapStatus->u4IcapContent;
#endif

			wlanoidQueryMemDump(prAdapter, &rMemDumpInfo, sizeof(rMemDumpInfo), &u4QueryInfo);

		}

		break;
	case EVENT_ID_DEBUG_MSG:
		{
			P_EVENT_DEBUG_MSG_T prEventDebugMsg;
			UINT_16 u2DebugMsgId;
			UINT_8 ucMsgType;
			UINT_8 ucFlags;
			UINT_32 u4Value;
			UINT_16 u2MsgSize;
			P_UINT_8 pucMsg;

			prEventDebugMsg = (P_EVENT_DEBUG_MSG_T) (prEvent->aucBuffer);

			u2DebugMsgId = prEventDebugMsg->u2DebugMsgId;
			ucMsgType = prEventDebugMsg->ucMsgType;
			ucFlags = prEventDebugMsg->ucFlags;
			u4Value = prEventDebugMsg->u4Value;
			u2MsgSize = prEventDebugMsg->u2MsgSize;
			pucMsg = prEventDebugMsg->aucMsg;

			DBGLOG(SW4, TRACE, "DEBUG_MSG Id %u Type %u Fg 0x%x Val 0x%x Size %u\n",
			       u2DebugMsgId, ucMsgType, ucFlags, u4Value, u2MsgSize);

			if (u2MsgSize <= DEBUG_MSG_SIZE_MAX) {
				if (ucMsgType >= DEBUG_MSG_TYPE_END)
					ucMsgType = DEBUG_MSG_TYPE_MEM32;

				if (ucMsgType == DEBUG_MSG_TYPE_ASCII) {
					PUINT_8 pucChr;

					pucMsg[u2MsgSize] = '\0';

					/* skip newline */
					pucChr = kalStrChr(pucMsg, '\0');
					if (*(pucChr - 1) == '\n')
						*(pucChr - 1) = '\0';

					DBGLOG(SW4, INFO, "<FW>%s\n", pucMsg);
				} else if (ucMsgType == DEBUG_MSG_TYPE_MEM8) {
					DBGLOG(SW4, INFO, "<FW>Dump MEM8\n");
					DBGLOG_MEM8(SW4, INFO, pucMsg, u2MsgSize);
				} else {
					DBGLOG(SW4, INFO, "<FW>Dump MEM32\n");
					DBGLOG_MEM32(SW4, INFO, pucMsg, u2MsgSize);
				}
			} /* DEBUG_MSG_SIZE_MAX */
			else
				DBGLOG(SW4, INFO, "Debug msg size %u is too large.\n", u2MsgSize);
		}
		break;

#if CFG_SUPPORT_BATCH_SCAN
	case EVENT_ID_BATCH_RESULT:
		DBGLOG(SCN, TRACE, "Got EVENT_ID_BATCH_RESULT");

		/* command response handling */
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
#endif /* CFG_SUPPORT_BATCH_SCAN */

#if CFG_SUPPORT_TDLS
	case EVENT_ID_TDLS:

		TdlsexEventHandle(prAdapter->prGlueInfo,
				  (UINT_8 *) prEvent->aucBuffer, (UINT_32) (prEvent->u2PacketLength - 8));
		break;
#endif /* CFG_SUPPORT_TDLS */

	case EVENT_ID_DUMP_MEM:
		DBGLOG(INIT, INFO, "%s: EVENT_ID_DUMP_MEM\n", __func__);

		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			DBGLOG(INIT, INFO, ": ==> 1\n");
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else {
			/* Burst mode */
			DBGLOG(INIT, INFO, ": ==> 2\n");
			nicEventQueryMemDump(prAdapter, prEvent->aucBuffer);
		}
		break;
#if CFG_ASSERT_DUMP
	case EVENT_ID_ASSERT_DUMP:

		if (prEvent->ucS2DIndex == S2D_INDEX_EVENT_N2H) {
			if (!prAdapter->fgN9AssertDumpOngoing) {
				DBGLOG(INIT, INFO, "%s: EVENT_ID_ASSERT_DUMP\n", __func__);
				DBGLOG(INIT, INFO, "\n[DUMP_N9]====N9 ASSERT_DUMPSTART====\n");
				fgKeepPrintCoreDump = TRUE;
				if (kalOpenCorDumpFile(TRUE) != WLAN_STATUS_SUCCESS)
					DBGLOG(INIT, INFO, "kalOpenCorDumpFile fail\n");
				else
					prAdapter->fgN9CorDumpFileOpend = TRUE;

				prAdapter->fgN9AssertDumpOngoing = TRUE;
			} else if (prAdapter->fgN9AssertDumpOngoing) {

				if (fgKeepPrintCoreDump)
					DBGLOG(INIT, INFO, "[DUMP_N9]%s:\n", prEvent->aucBuffer);
				if (!kalStrnCmp(prEvent->aucBuffer, ";more log added here", 5)
				    || !kalStrnCmp(prEvent->aucBuffer, ";[core dump start]", 5))
					fgKeepPrintCoreDump = FALSE;

				if (prAdapter->fgN9CorDumpFileOpend) {
					if (kalWriteCorDumpFile(prEvent->aucBuffer, prEvent->u2PacketLength, TRUE) !=
					    WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, INFO, "kalWriteN9CorDumpFile fail\n");
					}
				}
				wlanCorDumpTimerReset(prAdapter, TRUE);
			}
		} else {
			/* prEvent->ucS2DIndex == S2D_INDEX_EVENT_C2H */
			if (!prAdapter->fgCr4AssertDumpOngoing) {
				DBGLOG(INIT, INFO, "%s: EVENT_ID_ASSERT_DUMP\n", __func__);
				DBGLOG(INIT, INFO, "\n[DUMP_Cr4]====CR4 ASSERT_DUMPSTART====\n");
				fgKeepPrintCoreDump = TRUE;
				if (kalOpenCorDumpFile(FALSE) != WLAN_STATUS_SUCCESS)
					DBGLOG(INIT, INFO, "kalOpenCorDumpFile fail\n");
				else
					prAdapter->fgCr4CorDumpFileOpend = TRUE;

				prAdapter->fgCr4AssertDumpOngoing = TRUE;
			} else if (prAdapter->fgCr4AssertDumpOngoing) {
				if (fgKeepPrintCoreDump)
					DBGLOG(INIT, INFO, "[DUMP_CR4]%s:\n", prEvent->aucBuffer);
				if (!kalStrnCmp(prEvent->aucBuffer, ";more log added here", 5))
					fgKeepPrintCoreDump = FALSE;

				if (prAdapter->fgCr4CorDumpFileOpend) {
					if (kalWriteCorDumpFile(prEvent->aucBuffer, prEvent->u2PacketLength, FALSE) !=
					    WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, INFO, "kalWriteN9CorDumpFile fail\n");
					}
				}
				wlanCorDumpTimerReset(prAdapter, FALSE);
			}
		}
		break;

#endif

	case EVENT_ID_RDD_SEND_PULSE:
		DBGLOG(INIT, INFO, "%s: EVENT_ID_RDD_SEND_PULSE\n", __func__);

		nicEventRddPulseDump(prAdapter, prEvent->aucBuffer);
		break;

	case EVENT_ID_ACCESS_RX_STAT:
	case EVENT_ID_ACCESS_REG:
	case EVENT_ID_NIC_CAPABILITY:
		/* case EVENT_ID_MAC_MCAST_ADDR: */
	case EVENT_ID_ACCESS_EEPROM:
	case EVENT_ID_TEST_STATUS:
	default:
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			/* return prCmdInfo */
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
	}

	/* Reset Chip NoAck flag */
	if (prGlueInfo->prAdapter->fgIsChipNoAck) {
		DBGLOG(INIT, WARN, "Got response from chip, clear NoAck flag!\n");
		WARN_ON(TRUE);
	}
	prGlueInfo->prAdapter->ucOidTimeoutCount = 0;
	prGlueInfo->prAdapter->fgIsChipNoAck = FALSE;

	nicRxReturnRFB(prAdapter, prSwRfb);
}
#endif

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
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_8 ucSubtype;
#if CFG_SUPPORT_802_11W
	/* BOOL   fgMfgDrop = FALSE; */
#endif
	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	nicRxFillRFB(prAdapter, prSwRfb);

	ucSubtype = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FC_SUBTYPE) >> OFFSET_OF_FC_SUBTYPE;

#if CFG_RX_PKTS_DUMP
	{
		P_WLAN_MAC_MGMT_HEADER_T prWlanMgmtHeader;
		UINT_16 u2TxFrameCtrl;

		u2TxFrameCtrl = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FRAME_TYPE);
		if (prAdapter->rRxCtrl.u4RxPktsDumpTypeMask & BIT(HIF_RX_PKT_TYPE_MANAGEMENT)) {
			if (u2TxFrameCtrl == MAC_FRAME_BEACON || u2TxFrameCtrl == MAC_FRAME_PROBE_RSP) {

				prWlanMgmtHeader = (P_WLAN_MAC_MGMT_HEADER_T) (prSwRfb->pvHeader);

				DBGLOG(SW4, INFO,
						"QM RX MGT: net %u sta idx %u wlan idx %u ssn %u ptype %u subtype %u 11 %u\n",
						prSwRfb->prStaRec->ucBssIndex, prSwRfb->ucStaRecIdx,
						prSwRfb->ucWlanIdx, prWlanMgmtHeader->u2SeqCtrl,
						/* The new SN of the frame */
						prSwRfb->ucPacketType, ucSubtype);
				/* HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr))); */

				DBGLOG_MEM8(SW4, TRACE, (PUINT_8) prSwRfb->pvHeader, prSwRfb->u2PacketLen);
			}
		}
	}
#endif
#if CFG_SUPPORT_802_11W
	if (HAL_RX_STATUS_IS_ICV_ERROR(prSwRfb->prRxStatus)) {
		if (HAL_RX_STATUS_GET_SEC_MODE(prSwRfb->prRxStatus) == CIPHER_SUITE_BIP)
			DBGLOG(RSN, INFO, "[MFP] RX with BIP ICV ERROR\n");
		else
			DBGLOG(RSN, INFO, "[MFP] RX with ICV ERROR\n");

		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(&prAdapter->rRxCtrl, RX_DROP_TOTAL_COUNT);
		return;
	}
#endif

	if (prAdapter->fgTestMode == FALSE) {
#if CFG_MGMT_FRAME_HANDLING
		prGlueInfo = prAdapter->prGlueInfo;
		if ((prGlueInfo == NULL) || (prGlueInfo->u4ReadyFlag == 0)) {
			DBGLOG(RX, WARN,
			   "Bypass this mgmt frame without wlanProbe done\n");
		} else if (apfnProcessRxMgtFrame[ucSubtype]) {
			switch (apfnProcessRxMgtFrame[ucSubtype] (prAdapter, prSwRfb)) {
			case WLAN_STATUS_PENDING:
				return;
			case WLAN_STATUS_SUCCESS:
			case WLAN_STATUS_FAILURE:
				break;

			default:
				DBGLOG(RX, WARN, "Unexpected MMPDU(0x%02X) returned with abnormal status\n", ucSubtype);
				break;
			}
		}
#endif
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

VOID nicRxProcessMsduReport(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	halRxProcessMsduReport(prAdapter, prSwRfb);

	nicRxReturnRFB(prAdapter, prSwRfb);
}

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
	QUE_T rTempRfbList;
	P_QUE_T prTempRfbList = &rTempRfbList;
	UINT_32 u4RxLoopCount;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prRxCtrl->ucNumIndPacket = 0;
	prRxCtrl->ucNumRetainedPacket = 0;
	u4RxLoopCount = prAdapter->rWifiVar.u4TxRxLoopCount;

	QUEUE_INITIALIZE(prTempRfbList);

	while (u4RxLoopCount--) {
		while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rReceivedRfbList)) {

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_MOVE_ALL(prTempRfbList, &prRxCtrl->rReceivedRfbList);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

			while (QUEUE_IS_NOT_EMPTY(prTempRfbList)) {
				QUEUE_REMOVE_HEAD(prTempRfbList, prSwRfb, P_SW_RFB_T);

				switch (prSwRfb->ucPacketType) {
				case RX_PKT_TYPE_RX_DATA:
#if CFG_SUPPORT_SNIFFER
					if (prAdapter->prGlueInfo->fgIsEnableMon && HAL_IS_RX_DIRECT(prAdapter)) {
						nicRxProcessMonitorPacket(prAdapter, prSwRfb);
						break;
					} else if (prAdapter->prGlueInfo->fgIsEnableMon) {
						nicRxProcessMonitorPacket(prAdapter, prSwRfb);
						break;
					}
#endif
					nicRxProcessDataPacket(
							prAdapter, prSwRfb);
					break;

				case RX_PKT_TYPE_SW_DEFINED:
					/* HIF_RX_PKT_TYPE_EVENT */
					if ((prSwRfb->prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) ==
					    RXM_RXD_PKT_TYPE_SW_EVENT) {
						nicRxProcessEventPacket(prAdapter, prSwRfb);
					}
					/* case HIF_RX_PKT_TYPE_MANAGEMENT: */
					else if ((prSwRfb->prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) ==
						 RXM_RXD_PKT_TYPE_SW_FRAME) {
						nicRxProcessMgmtPacket(prAdapter, prSwRfb);
					} else if (prSwRfb->pvHeader == NULL) {
						DBGLOG(RX, ERROR,
							"Rcv Pkt format wrong.!!!\n");
						nicRxReturnRFB(prAdapter,
							prSwRfb);
						RX_INC_CNT(prRxCtrl,
							RX_TYPE_ERR_DROP_COUNT);
						RX_INC_CNT(prRxCtrl,
							RX_DROP_TOTAL_COUNT);
					} else {
						DBGLOG(RX, ERROR, "u2PktTYpe(0x%04X) is OUT OF DEF.!!!\n",
								prSwRfb->prRxStatus->u2PktTYpe);
						DBGLOG_MEM8(RX, ERROR, (PUINT_8) prSwRfb->pvHeader,
								prSwRfb->u2PacketLen);

						/*ASSERT(0);*/
						nicRxReturnRFB(prAdapter, prSwRfb);
						RX_INC_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT);
						RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);

					}
					break;

				case RX_PKT_TYPE_MSDU_REPORT:
					nicRxProcessMsduReport(prAdapter, prSwRfb);
					break;

					/* case HIF_RX_PKT_TYPE_TX_LOOPBACK: */
					/* case HIF_RX_PKT_TYPE_MANAGEMENT: */
				case RX_PKT_TYPE_TX_STATUS:
				case RX_PKT_TYPE_RX_VECTOR:
				case RX_PKT_TYPE_TM_REPORT:
				default:
					nicRxReturnRFB(prAdapter, prSwRfb);
					RX_INC_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT);
					RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
					DBGLOG(RX, ERROR, "ucPacketType = %d\n", prSwRfb->ucPacketType);
					break;
				}

			}

			if (prRxCtrl->ucNumIndPacket > 0) {
				RX_ADD_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT, prRxCtrl->ucNumIndPacket);
				RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT, prRxCtrl->ucNumRetainedPacket);
#if !CFG_SUPPORT_MULTITHREAD
				/* DBGLOG(RX, INFO, ("%d packets indicated, Retained cnt = %d\n", */
				/* prRxCtrl->ucNumIndPacket, prRxCtrl->ucNumRetainedPacket)); */
#if CFG_NATIVE_802_11
				kalRxIndicatePkts(prAdapter->prGlueInfo, (UINT_32) prRxCtrl->ucNumIndPacket,
						  (UINT_32) prRxCtrl->ucNumRetainedPacket);
#else
				kalRxIndicatePkts(prAdapter->prGlueInfo, prRxCtrl->apvIndPacket,
						  (UINT_32) prRxCtrl->ucNumIndPacket);
#endif
#endif
			}
		}
	}
}				/* end of nicRxProcessRFBs() */

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
		kalMemZero(((PUINT_8) prSwRfb + OFFSET_OF(SW_RFB_T, prRxStatus)),
			   (sizeof(SW_RFB_T) - OFFSET_OF(SW_RFB_T, prRxStatus)));
	}

	/* ToDo: remove prHifRxHdr */
	/* prSwRfb->prHifRxHdr = (P_HIF_RX_HEADER_T)(prSwRfb->pucRecvBuff); */
	prSwRfb->prRxStatus = (P_HW_MAC_RX_DESC_T) (prSwRfb->pucRecvBuff);

	return WLAN_STATUS_SUCCESS;

}				/* end of nicRxSetupRFB() */

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

	/* The processing on this RFB is done, so put it back on the tail of our list */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	if (prSwRfb->pvPacket) {
		/* QUEUE_INSERT_TAIL */
		QUEUE_INSERT_TAIL(&prRxCtrl->rFreeSwRfbList, prQueEntry);
	} else {
		/* QUEUE_INSERT_TAIL */
		QUEUE_INSERT_TAIL(&prRxCtrl->rIndicatedRfbList, prQueEntry);
	}
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	/* Trigger Rx if there are free SwRfb */
	if (halIsPendingRx(prAdapter) && (prRxCtrl->rFreeSwRfbList.u4NumElem > 0))
		kalSetIntEvent(prAdapter->prGlueInfo);
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
	ASSERT(prAdapter);

	/* SER break point */
	if (nicSerIsRxStop(prAdapter)) {
		/* Skip following Rx handling */
		return;
	}

	halProcessRxInterrupt(prAdapter);

#if CFG_SUPPORT_MULTITHREAD
	set_bit(GLUE_FLAG_RX_BIT, &(prAdapter->prGlueInfo->ulFlag));
	wake_up_interruptible(&(prAdapter->prGlueInfo->waitq));
#else
	nicRxProcessRFBs(prAdapter);
#endif

	return;

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

	if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS) || (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)) {
		/* count success num */
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
	UINT_32 u4CurrCount;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	/* if (pucBuffer) {} *//* For Windows, we'll print directly instead of sprintf() */
	ASSERT(pu4Count);

#define SPRINTF_RX_QSTATUS(arg) \
	{ \
		u4CurrCount = scnprintf(pucCurrBuf, *pu4Count, PRINTF_ARG arg); \
		pucCurrBuf += (UINT_8)u4CurrCount; \
		*pu4Count -= u4CurrCount; \
	}


	SPRINTF_RX_QSTATUS(("\n\nRX CTRL STATUS:"));
	SPRINTF_RX_QSTATUS(("\n==============="));
	SPRINTF_RX_QSTATUS(("\nFREE RFB w/i BUF LIST :%9u",
					prRxCtrl->rFreeSwRfbList.u4NumElem));
	SPRINTF_RX_QSTATUS(("\nFREE RFB w/o BUF LIST :%9u",
					prRxCtrl->rIndicatedRfbList.u4NumElem));
	SPRINTF_RX_QSTATUS(("\nRECEIVED RFB LIST     :%9u",
					prRxCtrl->rReceivedRfbList.u4NumElem));

	SPRINTF_RX_QSTATUS(("\n\n"));

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
	UINT_32 u4CurrCount;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	/* if (pucBuffer) {} *//* For Windows, we'll print directly instead of sprintf() */
	ASSERT(pu4Count);

#define SPRINTF_RX_COUNTER(eCounter) \
	{ \
		u4CurrCount = scnprintf(pucCurrBuf, *pu4Count, "%-30s : %u\n", \
					#eCounter, (UINT_32)prRxCtrl->au8Statistics[eCounter]); \
		pucCurrBuf += (UINT_8)u4CurrCount; \
		*pu4Count -= u4CurrCount; \
	}

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
	P_WIFI_EVENT_T prEvent;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	u4Status = halRxWaitResponse(prAdapter, ucPortIdx, pucRspBuffer, u4MaxRespBufferLen, pu4Length);
	if (u4Status == WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, TRACE,
				"Dump Response buffer, length = %u\n",
				*pu4Length);
		DBGLOG_MEM8(RX, TRACE, pucRspBuffer, *pu4Length);

		prEvent = (P_WIFI_EVENT_T) pucRspBuffer;
		DBGLOG(INIT, TRACE, "RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
		       prEvent->ucEID, prEvent->ucSeqNum, prEvent->u2PacketLength);
	} else {
		DBGLOG(RX, ERROR, "halRxWaitResponse fail!\n");
	}

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
	P_BSS_INFO_T prBssInfo = NULL;
#if CFG_SUPPORT_802_11W
	BOOL fgRobustAction = FALSE;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
#endif

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	DBGLOG(RSN, TRACE, "[Rx] nicRxProcessActionFrame\n");

	if (prSwRfb->u2PacketLen < sizeof(WLAN_ACTION_FRAME) - 1)
		return WLAN_STATUS_INVALID_PACKET;
	prActFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;

	DBGLOG(RSN, INFO, "Action frame category=%d\n", prActFrame->ucCategory);

#if CFG_SUPPORT_802_11W
	if ((prActFrame->ucCategory <= CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION &&
	     prActFrame->ucCategory != CATEGORY_PUBLIC_ACTION &&
	     prActFrame->ucCategory != CATEGORY_HT_ACTION) /* At 11W spec Code 7 is reserved */ ||
	    (prActFrame->ucCategory == CATEGORY_VENDOR_SPECIFIC_ACTION_PROTECTED)) {
		fgRobustAction = TRUE;
	}
	/* DBGLOG(RSN, TRACE, ("[Rx] fgRobustAction=%d\n", fgRobustAction)); */

	if (fgRobustAction && prSwRfb->prStaRec &&
	    GET_BSS_INFO_BY_INDEX(prAdapter, prSwRfb->prStaRec->ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS) {
		prAisSpecBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

		DBGLOG(RSN, INFO,
		       "[Rx]RobustAction %x %x %x\n", prSwRfb->prRxStatus->u2StatusFlag,
		       prSwRfb->prRxStatus->ucWlanIdx, prSwRfb->prRxStatus->ucTidSecMode);

		if (prAisSpecBssInfo->fgMgmtProtection && (!(prActFrame->u2FrameCtrl & MASK_FC_PROTECTED_FRAME)
							   && (HAL_RX_STATUS_GET_SEC_MODE(prSwRfb->prRxStatus) ==
							       CIPHER_SUITE_CCMP))) {
			DBGLOG(RSN, INFO, "[MFP] Not handle and drop un-protected robust action frame!!\n");
			return WLAN_STATUS_INVALID_PACKET;
		}
	}
	/* DBGLOG(RSN, INFO, "[Rx] pre check done, handle cateory %d\n", prActFrame->ucCategory); */
#endif

	if (prSwRfb->prStaRec)
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prSwRfb->prStaRec->ucBssIndex);

	switch (prActFrame->ucCategory) {
#if CFG_M0VE_BA_TO_DRIVER
	case CATEGORY_BLOCK_ACK_ACTION:
		DBGLOG(RX, WARN, "[Puff][%s] Rx CATEGORY_BLOCK_ACK_ACTION\n", __func__);

		if (prSwRfb->prStaRec)
			mqmHandleBaActionFrame(prAdapter, prSwRfb);

		break;
#endif
	case CATEGORY_PUBLIC_ACTION:
#if 0				/* CFG_SUPPORT_802_11W */
/* Sigma */
#else
		if (prAdapter->prGlueInfo &&
			prAdapter->prGlueInfo->fgIsRegistered &&
			prAdapter->prAisBssInfo &&
			prSwRfb->prStaRec &&
			prSwRfb->prStaRec->ucBssIndex ==
			prAdapter->prAisBssInfo->ucBssIndex) {
			aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
		}
#endif
		if (prAdapter->prGlueInfo &&
			prAdapter->prGlueInfo->fgIsRegistered &&
			prAdapter->prAisBssInfo &&
			prAdapter->prAisBssInfo->ucBssIndex ==
			KAL_NETWORK_TYPE_AIS_INDEX)
			aisFuncValidateRxActionFrame(prAdapter, prSwRfb);

#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		&& (prAdapter->rP2PNetRegState
			== ENUM_NET_REG_STATE_REGISTERED)) {
			rlmProcessPublicAction(prAdapter, prSwRfb);
			if (prBssInfo)
				p2pFuncValidateRxActionFrame(prAdapter, prSwRfb,
				(prBssInfo->ucBssIndex == P2P_DEV_BSS_INDEX), (UINT_8) prBssInfo->u4PrivateData);
			else
				p2pFuncValidateRxActionFrame(prAdapter, prSwRfb, TRUE, 0);
		}
#endif
		break;

	case CATEGORY_HT_ACTION:
		rlmProcessHtAction(prAdapter, prSwRfb);
		break;
	case CATEGORY_VENDOR_SPECIFIC_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		&& (prAdapter->rP2PNetRegState
			== ENUM_NET_REG_STATE_REGISTERED)) {
			if (prBssInfo)
				p2pFuncValidateRxActionFrame(prAdapter, prSwRfb,
				(prBssInfo->ucBssIndex == P2P_DEV_BSS_INDEX), (UINT_8) prBssInfo->u4PrivateData);
			else
				p2pFuncValidateRxActionFrame(prAdapter, prSwRfb, TRUE, 0);
		}
#endif
		break;
#if CFG_SUPPORT_802_11W
	case CATEGORY_SA_QUERY_ACTION:
		{
			P_BSS_INFO_T prBssInfo;

			if (prSwRfb->prStaRec) {
				prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prSwRfb->prStaRec->ucBssIndex);
				ASSERT(prBssInfo);
				if ((prBssInfo->eNetworkType == NETWORK_TYPE_AIS) &&
				    prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection /* Use MFP */) {
					/* MFP test plan 5.3.3.4 */
					rsnSaQueryAction(prAdapter, prSwRfb);
				} else if ((prBssInfo->eNetworkType == NETWORK_TYPE_P2P) &&
					(prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
					/* AP PMF */
					DBGLOG(RSN, INFO, "[Rx] nicRx AP PMF SAQ action\n");
					if (rsnCheckBipKeyInstalled(prAdapter, prSwRfb->prStaRec)) {
						/* MFP test plan 4.3.3.4 */
						rsnApSaQueryAction(prAdapter, prSwRfb);
					}
				}
			}
		}
		break;
#endif
#if CFG_SUPPORT_802_11V
	case CATEGORY_WNM_ACTION:
		{
			wnmWNMAction(prAdapter, prSwRfb);
		}
		break;
#endif

#if CFG_SUPPORT_DFS
	case CATEGORY_SPEC_MGT:
		{
			if (prAdapter->fgEnable5GBand) {
				DBGLOG(RLM, INFO, "[Channel Switch]nicRxProcessActionFrame\n");
				rlmProcessSpecMgtAction(prAdapter, prSwRfb);
			}
		}
		break;
#endif

#if CFG_SUPPORT_802_11AC
	case CATEGORY_VHT_ACTION:
		rlmProcessVhtAction(prAdapter, prSwRfb);
		break;
#endif

	default:
		break;
	}			/* end of switch case */

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
UINT_8
nicRxGetRcpiValueFromRxv(IN UINT_8 ucRcpiMode, IN P_SW_RFB_T prSwRfb)
{
	UINT_8 ucRcpi0, ucRcpi1;
	UINT_8 ucRcpiValue = 0;
	UINT_8 ucRxNum;

	ASSERT(prSwRfb);

	if (ucRcpiMode >= RCPI_MODE_NUM) {
		DBGLOG(RX, WARN, "Rcpi Mode = %d is invalid for getting RCPI value from RXV\n", ucRcpiMode);
		return 0;
	}

	ucRcpi0 = HAL_RX_STATUS_GET_RCPI0(prSwRfb->prRxStatusGroup3);
	ucRcpi1 = HAL_RX_STATUS_GET_RCPI1(prSwRfb->prRxStatusGroup3);
	ucRxNum = HAL_RX_STATUS_GET_RX_NUM(prSwRfb->prRxStatusGroup3);

	if (ucRxNum == 0)
		ucRcpiValue = ucRcpi0; /*0:1R, BBP always report RCPI0 at 1R mode*/

	else if (ucRxNum == 1) {
		switch (ucRcpiMode) {
		case RCPI_MODE_WF0:
			ucRcpiValue = ucRcpi0;
			break;

		case RCPI_MODE_WF1:
			ucRcpiValue = ucRcpi1;
			break;

		case RCPI_MODE_WF2:
		case RCPI_MODE_WF3:
			DBGLOG(RX, WARN, "Rcpi Mode = %d is invalid for device with only 2 antenna\n", ucRcpiMode);
			break;

		case RCPI_MODE_AVG: /*Not recommended for CBW80+80*/
			ucRcpiValue = (ucRcpi0 + ucRcpi1) / 2;
			break;

		case RCPI_MODE_MAX:
			ucRcpiValue = (ucRcpi0 > ucRcpi1) ? (ucRcpi0) : (ucRcpi1);
			break;

		case RCPI_MODE_MIN:
			ucRcpiValue = (ucRcpi0 < ucRcpi1) ? (ucRcpi0) : (ucRcpi1);
			break;

		default:
			break;
		}
	} else {
		DBGLOG(RX, WARN, "RX_NUM = %d is invalid for getting RCPI value from RXV\n", ucRxNum);
		return 0;
	}

	return ucRcpiValue;
}
