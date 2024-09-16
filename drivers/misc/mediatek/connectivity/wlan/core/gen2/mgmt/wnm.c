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

#if (CFG_SUPPORT_802_11V || CFG_SUPPORT_PPR2)

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define WNM_MAX_TOD_ERROR  0
#define WNM_MAX_TOA_ERROR  0
#define MICRO_TO_10NANO(x) ((x)*100)
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

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
static UINT_8 ucTimingMeasToken;
#endif
static UINT_8 ucBtmMgtToken = 1;

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
*
* \brief This routine is called to process the 802.11v wnm category action frame.
*
*
* \note
*      Called by: Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
VOID wnmWNMAction(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_ACTION_FRAME prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;

	DBGLOG(WNM, TRACE, "WNM action frame: %d from %pM\n", prRxFrame->ucAction, prRxFrame->aucSrcAddr);

	switch (prRxFrame->ucAction) {
#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
	case ACTION_WNM_TIMING_MEASUREMENT_REQUEST:
		wnmTimingMeasRequest(prAdapter, prSwRfb);
		return;
#endif
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	case ACTION_WNM_BSS_TRANSITION_MANAGEMENT_REQ:
		wnmRecvBTMRequest(prAdapter, prSwRfb);
		return;
#endif
	case ACTION_WNM_NOTIFICATION_REQUEST:
	default:
		DBGLOG(INIT, INFO, "WNM action frame: %d, try to send to supplicant\n", prRxFrame->ucAction);
		if (HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr) == NETWORK_TYPE_AIS_INDEX)
			aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
		break;
	}
}

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to report timing measurement data.
*
*/
/*----------------------------------------------------------------------------*/
VOID wnmReportTimingMeas(IN P_ADAPTER_T prAdapter, IN UINT_8 ucStaRecIndex, IN UINT_32 u4ToD, IN UINT_32 u4ToA)
{
	P_STA_RECORD_T prStaRec;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIndex);

	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return;

	DBGLOG(WNM, TRACE, "wnmReportTimingMeas: u4ToD %x u4ToA %x", u4ToD, u4ToA);

	if (!prStaRec->rWNMTimingMsmt.ucTrigger)
		return;

	prStaRec->rWNMTimingMsmt.u4ToD = MICRO_TO_10NANO(u4ToD);
	prStaRec->rWNMTimingMsmt.u4ToA = MICRO_TO_10NANO(u4ToA);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle TxDone(TimingMeasurement) Event.
*
* @param[in] prAdapter      Pointer to the Adapter structure.
* @param[in] prMsduInfo     Pointer to the MSDU_INFO_T.
* @param[in] rTxDoneStatus  Return TX status of the Timing Measurement frame.
*
* @retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wnmRunEventTimgingMeasTxDone(IN P_ADAPTER_T prAdapter,
			     IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_STA_RECORD_T prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	DBGLOG(WNM, LOUD, "EVENT-TX DONE: Current Time = %u\n", kalGetTimeTick());

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return WLAN_STATUS_SUCCESS;	/* For the case of replying ERROR STATUS CODE */

	DBGLOG(WNM, TRACE, "wnmRunEventTimgingMeasTxDone: ucDialog %d ucFollowUp %d u4ToD %x u4ToA %x",
			    prStaRec->rWNMTimingMsmt.ucDialogToken,
			    prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken,
			    prStaRec->rWNMTimingMsmt.u4ToD, prStaRec->rWNMTimingMsmt.u4ToA);

	prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken = prStaRec->rWNMTimingMsmt.ucDialogToken;
	prStaRec->rWNMTimingMsmt.ucDialogToken = ++ucTimingMeasToken;

	wnmComposeTimingMeasFrame(prAdapter, prStaRec, NULL);

	return WLAN_STATUS_SUCCESS;

}				/* end of wnmRunEventTimgingMeasTxDone() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will compose the Timing Measurement frame.
*
* @param[in] prAdapter              Pointer to the Adapter structure.
* @param[in] prStaRec               Pointer to the STA_RECORD_T.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
wnmComposeTimingMeasFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	P_MSDU_INFO_T prMsduInfo;
	P_BSS_INFO_T prBssInfo;
	P_ACTION_UNPROTECTED_WNM_TIMING_MEAS_FRAME prTxFrame;
	UINT_16 u2PayloadLen;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];
	ASSERT(prBssInfo);

	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (P_ACTION_UNPROTECTED_WNM_TIMING_MEAS_FRAME)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_UNPROTECTED_WNM_ACTION;
	prTxFrame->ucAction = ACTION_UNPROTECTED_WNM_TIMING_MEASUREMENT;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prStaRec->rWNMTimingMsmt.ucDialogToken;
	prTxFrame->ucFollowUpDialogToken = prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken;
	prTxFrame->u4ToD = prStaRec->rWNMTimingMsmt.u4ToD;
	prTxFrame->u4ToA = prStaRec->rWNMTimingMsmt.u4ToA;
	prTxFrame->ucMaxToDErr = WNM_MAX_TOD_ERROR;
	prTxFrame->ucMaxToAErr = WNM_MAX_TOA_ERROR;

	u2PayloadLen = 2 + ACTION_UNPROTECTED_WNM_TIMING_MEAS_LEN;

	/* 4 Update information of MSDU_INFO_T */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;	/* Management frame */
	prMsduInfo->ucStaRecIndex = prStaRec->ucIndex;
	prMsduInfo->ucNetworkType = prStaRec->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen;
	prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfo->pfTxDoneHandler = pfTxDoneHandler;
	prMsduInfo->fgIsBasicRate = FALSE;

	DBGLOG(WNM, TRACE, "wnmComposeTimingMeasFrame: ucDialogToken %d ucFollowUpDialogToken %d u4ToD %x u4ToA %x\n",
			    prTxFrame->ucDialogToken, prTxFrame->ucFollowUpDialogToken,
			    prTxFrame->u4ToD, prTxFrame->u4ToA);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return;

}				/* end of wnmComposeTimingMeasFrame() */

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to process the 802.11v timing measurement request.
*
*
* \note
*      Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
VOID wnmTimingMeasRequest(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_ACTION_WNM_TIMING_MEAS_REQ_FRAME prRxFrame = NULL;
	P_STA_RECORD_T prStaRec;

	prRxFrame = (P_ACTION_WNM_TIMING_MEAS_REQ_FRAME) prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return;

	DBGLOG(WNM, TRACE, "IEEE 802.11: Received Timing Measuremen Request from %pM\n"
			    prStaRec->aucMacAdd);

	/* reset timing msmt */
	prStaRec->rWNMTimingMsmt.fgInitiator = TRUE;
	prStaRec->rWNMTimingMsmt.ucTrigger = prRxFrame->ucTrigger;
	if (!prRxFrame->ucTrigger)
		return;

	prStaRec->rWNMTimingMsmt.ucDialogToken = ++ucTimingMeasToken;
	prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken = 0;

	wnmComposeTimingMeasFrame(prAdapter, prStaRec, wnmRunEventTimgingMeasTxDone);
}

#if WNM_UNIT_TEST
VOID wnmTimingMeasUnitTest1(P_ADAPTER_T prAdapter, UINT_8 ucStaRecIndex)
{
	P_STA_RECORD_T prStaRec;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIndex);
	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return;

	DBGLOG(WNM, INFO, "IEEE 802.11v: Test Timing Measuremen Request from %pM\n",
			prStaRec->aucMacAddr);

	prStaRec->rWNMTimingMsmt.fgInitiator = TRUE;
	prStaRec->rWNMTimingMsmt.ucTrigger = 1;

	prStaRec->rWNMTimingMsmt.ucDialogToken = ++ucTimingMeasToken;
	prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken = 0;

	wnmComposeTimingMeasFrame(prAdapter, prStaRec, wnmRunEventTimgingMeasTxDone);
}
#endif

#endif /* CFG_SUPPORT_802_11V_TIMING_MEASUREMENT */

UINT_8 wnmGetBtmToken(VOID)
{
	return ucBtmMgtToken++;
}

static WLAN_STATUS
wnmBTMQueryTxDone(IN P_ADAPTER_T prAdapter,
			     IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	DBGLOG(WNM, INFO, "Bss Transition Management Query Frame Tx Done\n");
	return WLAN_STATUS_SUCCESS;
}

static WLAN_STATUS
wnmBTMResponseTxDone(IN P_ADAPTER_T prAdapter,
			     IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	DBGLOG(WNM, INFO, "Bss Transition Management Response Frame Tx Done\n");
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will compose the Bss Transition Management Response frame.
*
* @param[in] prAdapter              Pointer to the Adapter structure.
* @param[in] prStaRec               Pointer to the STA_RECORD_T.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
wnmSendBTMResponseFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_MSDU_INFO_T prMsduInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	struct ACTION_BTM_RSP_FRAME_T *prTxFrame = NULL;
	UINT_16 u2PayloadLen = 0;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
					&prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	PUINT_8 pucOptInfo = NULL;

	if (!prStaRec) {
		DBGLOG(WNM, INFO, "No station record found\n");
		return;
	}

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];
	ASSERT(prBssInfo);

	/* 1 Allocate MSDU Info */
	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);
	if (!prMsduInfo)
		return;
	prTxFrame = (struct ACTION_BTM_RSP_FRAME_T *)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	/* 2 Compose The Mac Header. */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_WNM_ACTION;
	prTxFrame->ucAction = ACTION_WNM_BSS_TRANSITION_MANAGEMENT_RSP;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prBtmParam->ucDialogToken;
	prBtmParam->ucDialogToken = 0; /* reset dialog token */
	prTxFrame->ucStatusCode = prBtmParam->ucStatusCode;
	prTxFrame->ucBssTermDelay = prBtmParam->ucTermDelay;
	pucOptInfo = &prTxFrame->aucOptInfo[0];
	if (prBtmParam->ucStatusCode == BSS_TRANSITION_MGT_STATUS_ACCEPT) {
		COPY_MAC_ADDR(pucOptInfo, prBtmParam->aucTargetBssid);
		pucOptInfo += MAC_ADDR_LEN;
		u2PayloadLen += MAC_ADDR_LEN;
	}
	if (prBtmParam->u2OurNeighborBssLen > 0) {
		kalMemCopy(pucOptInfo, prBtmParam->aucOurNeighborBss, prBtmParam->u2OurNeighborBssLen);
		u2PayloadLen += prBtmParam->u2OurNeighborBssLen;
	}

	/* 4 Update information of MSDU_INFO_T */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;	/* Management frame */
	prMsduInfo->ucStaRecIndex = prStaRec->ucIndex;
	prMsduInfo->ucNetworkType = prStaRec->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = OFFSET_OF(struct ACTION_BTM_RSP_FRAME_T, aucOptInfo) + u2PayloadLen;
	prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfo->pfTxDoneHandler = wnmBTMResponseTxDone;
	prMsduInfo->fgIsBasicRate = FALSE;

	/* 5 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}				/* end of wnmComposeBTMResponseFrame() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will compose the Bss Transition Management Query frame.
*
* @param[in] prAdapter              Pointer to the Adapter structure.
* @param[in] prStaRec               Pointer to the STA_RECORD_T.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
wnmSendBTMQueryFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_MSDU_INFO_T prMsduInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	struct ACTION_BTM_QUERY_FRAME_T *prTxFrame = NULL;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
				&prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];
	ASSERT(prBssInfo);

	/* 1 Allocate MSDU Info */
	prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);
	if (!prMsduInfo)
		return;
	prTxFrame = (struct ACTION_BTM_QUERY_FRAME_T *)
	    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	/* 2 Compose The Mac Header. */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);
	prTxFrame->ucCategory = CATEGORY_WNM_ACTION;
	prTxFrame->ucAction = ACTION_WNM_BSS_TRANSITION_MANAGEMENT_QUERY;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prBtmParam->ucDialogToken;
	prTxFrame->ucQueryReason = prBtmParam->ucQueryReason;
	if (prBtmParam->u2OurNeighborBssLen > 0)
		kalMemCopy(prTxFrame->pucNeighborBss, prBtmParam->aucOurNeighborBss, prBtmParam->u2OurNeighborBssLen);

	/* 4 Update information of MSDU_INFO_T */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;	/* Management frame */
	prMsduInfo->ucStaRecIndex = prStaRec->ucIndex;
	prMsduInfo->ucNetworkType = prStaRec->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = WLAN_MAC_MGMT_HEADER_LEN + 4 + prBtmParam->u2OurNeighborBssLen;
	prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfo->pfTxDoneHandler = wnmBTMQueryTxDone;
	prMsduInfo->fgIsBasicRate = FALSE;

	/* 5 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}				/* end of wnmComposeBTMQueryFrame() */

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to process the 802.11v Bss Transition Management request.
*
*
* \note
*      Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
VOID wnmRecvBTMRequest(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	struct ACTION_BTM_REQ_FRAME_T *prRxFrame = NULL;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
			&prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	PUINT_8 pucOptInfo = NULL;
	UINT_8 ucRequestMode = 0;
	UINT_16 u2TmpLen = 0;
	struct MSG_AIS_BSS_TRANSITION_T *prMsg = NULL;
	enum WNM_AIS_BSS_TRANSITION eTransType = BSS_TRANSITION_NO_MORE_ACTION;

	prRxFrame = (struct ACTION_BTM_REQ_FRAME_T *) prSwRfb->pvHeader;
	if (!prRxFrame)
		return;
	if (prSwRfb->u2PacketLen < OFFSET_OF(struct ACTION_BTM_REQ_FRAME_T, aucOptInfo)) {
		DBGLOG(WNM, WARN, "BTM request frame length is less than a standard BTM frame\n");
		return;
	}
	prMsg = (struct MSG_AIS_BSS_TRANSITION_T *)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_AIS_BSS_TRANSITION_T));
	if (!prMsg) {
		DBGLOG(WNM, WARN, "Msg Hdr is NULL\n");
		return;
	}

	prBtmParam->ucRequestMode = prRxFrame->ucRequestMode;
	prBtmParam->ucValidityInterval = prRxFrame->ucValidityInterval;
	prBtmParam->u2DisassocTimer = prRxFrame->u2DisassocTimer;
	prBtmParam->ucDialogToken = prRxFrame->ucDialogToken;
	pucOptInfo = &prRxFrame->aucOptInfo[0];
	if (!pucOptInfo) {
		DBGLOG(WNM, WARN, "pucOptInfo == NULL\n");
		return;
	}
	ucRequestMode = prBtmParam->ucRequestMode;
	u2TmpLen = OFFSET_OF(struct ACTION_BTM_REQ_FRAME_T, aucOptInfo);
	if (ucRequestMode & BTM_REQ_MODE_BSS_TERM_INCLUDE) {
		struct SUB_IE_BSS_TERM_DURATION_T *prBssTermDuration =
				(struct SUB_IE_BSS_TERM_DURATION_T *)pucOptInfo;

		prBtmParam->u2TermDuration = prBssTermDuration->u2Duration;
		kalMemCopy(prBtmParam->aucTermTsf, prBssTermDuration->aucTermTsf, 8);
		pucOptInfo += sizeof(*prBssTermDuration);
		u2TmpLen += sizeof(*prBssTermDuration);
	}
	if (ucRequestMode & BTM_REQ_MODE_ESS_DISC_IMM) {
		kalMemCopy(prBtmParam->aucSessionURL, &pucOptInfo[1], pucOptInfo[0]);
		prBtmParam->ucSessionURLLen = pucOptInfo[0];
		u2TmpLen += pucOptInfo[0];
	}
	if (ucRequestMode & BTM_REQ_MODE_DISC_IMM)
		eTransType = BSS_TRANSITION_DISASSOC;

	if (ucRequestMode & BTM_REQ_MODE_CAND_INCLUDED_BIT) {
		if (prSwRfb->u2PacketLen > u2TmpLen) {
			prBtmParam->u2PeerNeighborBssLen = prSwRfb->u2PacketLen - u2TmpLen;
			prBtmParam->pucPeerNeighborBss =
				kalMemAlloc(prBtmParam->u2PeerNeighborBssLen, VIR_MEM_TYPE);
		} else
			DBGLOG(WNM, WARN, "Candidate Include bit is set, but no candidate list\n");
	}

	DBGLOG(WNM, INFO, "BTM param: Req %d, VInt %d, DiscTimer %d, Token %d, TransType %d\n",
		   prBtmParam->ucRequestMode, prBtmParam->ucValidityInterval, prBtmParam->u2DisassocTimer,
		   prBtmParam->ucDialogToken, eTransType);

	prMsg->eTransitionType = eTransType;
	prMsg->rMsgHdr.eMsgId = MID_WNM_AIS_BSS_TRANSITION;
	/* if Bss Transition Mgmt Request is dest for broadcast, don't send Btm Response */
	if (kalMemCmp(prRxFrame->aucDestAddr, "\xff\xff\xff\xff\xff\xff", MAC_ADDR_LEN))
		prMsg->fgNeedResponse = TRUE;
	else
		prMsg->fgNeedResponse = FALSE;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsg, MSG_SEND_METHOD_BUF);
}
#endif /* CFG_SUPPORT_802_11V */
