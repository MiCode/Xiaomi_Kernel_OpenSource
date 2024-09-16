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
 ** Id: //Department/DaVinci/TRUNK/MT6620_5931_WiFi_Driver/mgmt/wnm.c#1
 */

/*! \file   "wnm.c"
 *    \brief  This file includes the 802.11v default vale and functions.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

#define WNM_MAX_TOD_ERROR 0
#define WNM_MAX_TOA_ERROR 0
#define MICRO_TO_10NANO(x) ((x)*100)
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
static uint8_t ucTimingMeasToken;
#endif
static uint8_t ucBtmMgtToken = 1;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
static uint32_t
wnmRunEventTimgingMeasTxDone(IN struct ADAPTER *prAdapter,
			     IN struct MSDU_INFO *prMsduInfo,
			     IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

static void wnmComposeTimingMeasFrame(IN struct ADAPTER *prAdapter,
				      IN struct STA_RECORD *prStaRec,
				      IN PFN_TX_DONE_HANDLER pfTxDoneHandler);

static void wnmTimingMeasRequest(IN struct ADAPTER *prAdapter,
				 IN struct SW_RFB *prSwRfb);
#endif
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11v wnm category action
 * frame.
 *
 *
 * \note
 *      Called by: Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void wnmWNMAction(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct WLAN_ACTION_FRAME *prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (struct WLAN_ACTION_FRAME *)prSwRfb->pvHeader;

	DBGLOG(WNM, TRACE, "WNM action frame: %d from " MACSTR "\n",
	       prRxFrame->ucAction, MAC2STR(prRxFrame->aucSrcAddr));

	switch (prRxFrame->ucAction) {
#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
	case ACTION_WNM_TIMING_MEASUREMENT_REQUEST:
		wnmTimingMeasRequest(prAdapter, prSwRfb);
		break;
#endif
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	case ACTION_WNM_BSS_TRANSITION_MANAGEMENT_REQ:
#endif
	case ACTION_WNM_NOTIFICATION_REQUEST:
	default:
		DBGLOG(RX, INFO,
		       "WNM: action frame %d, try to send to supplicant\n",
		       prRxFrame->ucAction);
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
void wnmReportTimingMeas(IN struct ADAPTER *prAdapter, IN uint8_t ucStaRecIndex,
			 IN uint32_t u4ToD, IN uint32_t u4ToA)
{
	struct STA_RECORD *prStaRec;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIndex);

	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return;

	DBGLOG(WNM, TRACE, "WNM: wnmReportTimingMeas: u4ToD %x u4ToA %x", u4ToD,
	       u4ToA);

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
static uint32_t
wnmRunEventTimgingMeasTxDone(IN struct ADAPTER *prAdapter,
			     IN struct MSDU_INFO *prMsduInfo,
			     IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	DBGLOG(WNM, LOUD, "WNM: EVENT-TX DONE: Current Time = %ld\n",
	       kalGetTimeTick());

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return WLAN_STATUS_SUCCESS; /* For the case of replying ERROR
					     * STATUS CODE
					     */

	DBGLOG(WNM, TRACE,
	       "WNM: wnmRunEventTimgingMeasTxDone: ucDialog %d ucFollowUp %d u4ToD %x u4ToA %x",
	       prStaRec->rWNMTimingMsmt.ucDialogToken,
	       prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken,
	       prStaRec->rWNMTimingMsmt.u4ToD, prStaRec->rWNMTimingMsmt.u4ToA);

	prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken =
		prStaRec->rWNMTimingMsmt.ucDialogToken;
	prStaRec->rWNMTimingMsmt.ucDialogToken = ++ucTimingMeasToken;

	wnmComposeTimingMeasFrame(prAdapter, prStaRec, NULL);

	return WLAN_STATUS_SUCCESS;

} /* end of wnmRunEventTimgingMeasTxDone() */

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
static void wnmComposeTimingMeasFrame(IN struct ADAPTER *prAdapter,
				      IN struct STA_RECORD *prStaRec,
				      IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct BSS_INFO *prBssInfo;
	struct ACTION_UNPROTECTED_WNM_TIMING_MEAS_FRAME *prTxFrame;
	uint16_t u2PayloadLen;

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex];
	ASSERT(prBssInfo);

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_UNPROTECTED_WNM_TIMING_MEAS_FRAME
			     *)((uint32_t)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_UNPROTECTED_WNM_ACTION;
	prTxFrame->ucAction = ACTION_UNPROTECTED_WNM_TIMING_MEASUREMENT;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prStaRec->rWNMTimingMsmt.ucDialogToken;
	prTxFrame->ucFollowUpDialogToken =
		prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken;
	prTxFrame->u4ToD = prStaRec->rWNMTimingMsmt.u4ToD;
	prTxFrame->u4ToA = prStaRec->rWNMTimingMsmt.u4ToA;
	prTxFrame->ucMaxToDErr = WNM_MAX_TOD_ERROR;
	prTxFrame->ucMaxToAErr = WNM_MAX_TOA_ERROR;

	u2PayloadLen = 2 + ACTION_UNPROTECTED_WNM_TIMING_MEAS_LEN;

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	DBGLOG(WNM, TRACE,
	       "WNM: wnmComposeTimingMeasFrame: ucDialogToken %d ucFollowUpDialogToken %d u4ToD %x u4ToA %x\n",
	       prTxFrame->ucDialogToken, prTxFrame->ucFollowUpDialogToken,
	       prTxFrame->u4ToD, prTxFrame->u4ToA);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return;

} /* end of wnmComposeTimingMeasFrame() */

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11v timing measurement
 * request.
 *
 *
 * \note
 *      Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
static void wnmTimingMeasRequest(IN struct ADAPTER *prAdapter,
				 IN struct SW_RFB *prSwRfb)
{
	struct ACTION_WNM_TIMING_MEAS_REQ_FRAME *prRxFrame = NULL;
	struct STA_RECORD *prStaRec;

	prRxFrame =
		(struct ACTION_WNM_TIMING_MEAS_REQ_FRAME *)prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return;

	DBGLOG(WNM, TRACE,
	       "WNM: Received Timing Measuremen Request from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	/* reset timing msmt */
	prStaRec->rWNMTimingMsmt.fgInitiator = TRUE;
	prStaRec->rWNMTimingMsmt.ucTrigger = prRxFrame->ucTrigger;
	if (!prRxFrame->ucTrigger)
		return;
	prStaRec->rWNMTimingMsmt.ucDialogToken = ++ucTimingMeasToken;
	prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken = 0;
	wnmComposeTimingMeasFrame(prAdapter, prStaRec,
				  wnmRunEventTimgingMeasTxDone);
}

#if WNM_UNIT_TEST
void wnmTimingMeasUnitTest1(struct ADAPTER *prAdapter, uint8_t ucStaRecIndex)
{
	struct STA_RECORD *prStaRec;

	prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIndex);
	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return;
	DBGLOG(WNM, INFO,
	       "WNM: Test Timing Measuremen Request from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));
	prStaRec->rWNMTimingMsmt.fgInitiator = TRUE;
	prStaRec->rWNMTimingMsmt.ucTrigger = 1;
	prStaRec->rWNMTimingMsmt.ucDialogToken = ++ucTimingMeasToken;
	prStaRec->rWNMTimingMsmt.ucFollowUpDialogToken = 0;
	wnmComposeTimingMeasFrame(prAdapter, prStaRec,
				  wnmRunEventTimgingMeasTxDone);
}
#endif

#endif /* CFG_SUPPORT_802_11V_TIMING_MEASUREMENT */

uint8_t wnmGetBtmToken(void)
{
	return ucBtmMgtToken++;
}

static uint32_t wnmBTMQueryTxDone(IN struct ADAPTER *prAdapter,
				  IN struct MSDU_INFO *prMsduInfo,
				  IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	DBGLOG(WNM, INFO, "BTM: Query Frame Tx Done, Status %d\n",
	       rTxDoneStatus);
	return WLAN_STATUS_SUCCESS;
}

static uint32_t wnmBTMResponseTxDone(IN struct ADAPTER *prAdapter,
				     IN struct MSDU_INFO *prMsduInfo,
				     IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct BSS_TRANSITION_MGT_PARAM_T *prBtm =
		&prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	struct AIS_FSM_INFO *prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;

	DBGLOG(WNM, INFO, "BTM: Response Frame Tx Done Status %d\n",
	       rTxDoneStatus);
	if (prBtm->fgPendingResponse &&
	    prAisFsmInfo->eCurrentState == AIS_STATE_SEARCH) {
		prBtm->fgPendingResponse = FALSE;
		aisFsmSteps(prAdapter, AIS_STATE_REQ_CHANNEL_JOIN);
	}
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the BTM Response frame.
 *
 * @param[in] prAdapter              Pointer to the Adapter structure.
 * @param[in] prStaRec               Pointer to the STA_RECORD_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void wnmSendBTMResponseFrame(IN struct ADAPTER *prAdapter,
			     IN struct STA_RECORD *prStaRec)
{
	struct MSDU_INFO *prMsduInfo = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct ACTION_BTM_RSP_FRAME *prTxFrame = NULL;
	uint16_t u2PayloadLen = 0;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
	    &prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	uint8_t *pucOptInfo = NULL;

	if (!prStaRec) {
		DBGLOG(WNM, INFO, "BTM: No station record found\n");
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ASSERT(prBssInfo);

	/* 1 Allocate MSDU Info */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);
	if (!prMsduInfo)
		return;
	prTxFrame = (struct ACTION_BTM_RSP_FRAME
			     *)((unsigned long)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);

	/* 2 Compose The Mac Header. */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_WNM_ACTION;
	prTxFrame->ucAction = ACTION_WNM_BSS_TRANSITION_MANAGEMENT_RSP;

	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = prBtmParam->ucDialogToken;
	prBtmParam->ucDialogToken = 0;	/* reset dialog token */
	prTxFrame->ucStatusCode = prBtmParam->ucStatusCode;
	prTxFrame->ucBssTermDelay = prBtmParam->ucTermDelay;
	pucOptInfo = &prTxFrame->aucOptInfo[0];
	if (prBtmParam->ucStatusCode == BSS_TRANSITION_MGT_STATUS_ACCEPT) {
		COPY_MAC_ADDR(pucOptInfo, prBtmParam->aucTargetBssid);
		pucOptInfo += MAC_ADDR_LEN;
		u2PayloadLen += MAC_ADDR_LEN;
	}
	if (prBtmParam->u2OurNeighborBssLen > 0) {
		kalMemCopy(pucOptInfo, prBtmParam->pucOurNeighborBss,
			   prBtmParam->u2OurNeighborBssLen);
		kalMemFree(prBtmParam->pucOurNeighborBss, VIR_MEM_TYPE,
			   prBtmParam->u2OurNeighborBssLen);
		prBtmParam->u2OurNeighborBssLen = 0;
		u2PayloadLen += prBtmParam->u2OurNeighborBssLen;
	}

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     OFFSET_OF(struct ACTION_BTM_RSP_FRAME, aucOptInfo) +
			     u2PayloadLen,
		     wnmBTMResponseTxDone, MSDU_RATE_MODE_AUTO);

	/* 5 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}				/* end of wnmComposeBTMResponseFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the BTM Query frame.
 *
 * @param[in] prAdapter              Pointer to the Adapter structure.
 * @param[in] prStaRec               Pointer to the STA_RECORD_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void wnmSendBTMQueryFrame(IN struct ADAPTER *prAdapter,
			  IN struct STA_RECORD *prStaRec)
{
	struct MSDU_INFO *prMsduInfo = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct ACTION_BTM_QUERY_FRAME *prTxFrame = NULL;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
	    &prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ASSERT(prBssInfo);

	/* 1 Allocate MSDU Info */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);
	if (!prMsduInfo)
		return;
	prTxFrame = (struct ACTION_BTM_QUERY_FRAME
			     *)((unsigned long)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);

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
	if (prBtmParam->u2OurNeighborBssLen > 0) {
		kalMemCopy(prTxFrame->pucNeighborBss,
			   prBtmParam->pucOurNeighborBss,
			   prBtmParam->u2OurNeighborBssLen);
		kalMemFree(prBtmParam->pucOurNeighborBss, VIR_MEM_TYPE,
			   prBtmParam->u2OurNeighborBssLen);
		prBtmParam->u2OurNeighborBssLen = 0;
	}

	/* 4 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + 4 +
			     prBtmParam->u2OurNeighborBssLen,
		     wnmBTMQueryTxDone, MSDU_RATE_MODE_AUTO);

	/* 5 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}				/* end of wnmComposeBTMQueryFrame() */

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11v BTM request.
 *
 *
 * \note
 *      Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void wnmRecvBTMRequest(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct ACTION_BTM_REQ_FRAME *prRxFrame = NULL;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
	    &prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam;
	uint8_t *pucOptInfo = NULL;
	uint8_t ucRequestMode = 0;
	uint16_t u2TmpLen = 0;
	struct MSG_AIS_BSS_TRANSITION_T *prMsg = NULL;
	enum WNM_AIS_BSS_TRANSITION eTransType = BSS_TRANSITION_NO_MORE_ACTION;

	prRxFrame = (struct ACTION_BTM_REQ_FRAME *) prSwRfb->pvHeader;
	if (!prRxFrame)
		return;
	if (prSwRfb->u2PacketLen <
		OFFSET_OF(struct ACTION_BTM_REQ_FRAME, aucOptInfo)) {
		DBGLOG(WNM, WARN,
		       "BTM: Request frame length is less than a standard BTM frame\n");
		return;
	}
	prMsg = (struct MSG_AIS_BSS_TRANSITION_T *)cnmMemAlloc(
		prAdapter, RAM_TYPE_MSG,
		sizeof(struct MSG_AIS_BSS_TRANSITION_T));
	if (!prMsg) {
		DBGLOG(WNM, WARN, "BTM: Msg Hdr is NULL\n");
		return;
	}
	kalMemZero(prMsg, sizeof(*prMsg));
	prBtmParam->ucRequestMode = prRxFrame->ucRequestMode;
	prMsg->ucToken = prRxFrame->ucDialogToken;
	prBtmParam->u2DisassocTimer = prRxFrame->u2DisassocTimer;
	prBtmParam->ucDialogToken = prRxFrame->ucDialogToken;
	pucOptInfo = &prRxFrame->aucOptInfo[0];
	ucRequestMode = prBtmParam->ucRequestMode;
	u2TmpLen = OFFSET_OF(struct ACTION_BTM_REQ_FRAME, aucOptInfo);
	if (ucRequestMode & BTM_REQ_MODE_DISC_IMM)
		eTransType = BSS_TRANSITION_REQ_ROAMING;
	if (ucRequestMode & BTM_REQ_MODE_BSS_TERM_INCLUDE) {
		struct SUB_IE_BSS_TERM_DURATION *prBssTermDuration =
			(struct SUB_IE_BSS_TERM_DURATION *)pucOptInfo;

		prBtmParam->u2TermDuration = prBssTermDuration->u2Duration;
		kalMemCopy(prBtmParam->aucTermTsf,
			   prBssTermDuration->aucTermTsf, 8);
		pucOptInfo += sizeof(*prBssTermDuration);
		u2TmpLen += sizeof(*prBssTermDuration);
		eTransType = BSS_TRANSITION_REQ_ROAMING;
	}
	if (ucRequestMode & BTM_REQ_MODE_ESS_DISC_IMM) {
		kalMemCopy(prBtmParam->aucSessionURL, &pucOptInfo[1],
			   pucOptInfo[0]);
		prBtmParam->ucSessionURLLen = pucOptInfo[0];
		u2TmpLen += pucOptInfo[0];
		pucOptInfo += pucOptInfo[0] + 1;
		eTransType = BSS_TRANSITION_DISASSOC;
	}
	if (ucRequestMode & BTM_REQ_MODE_CAND_INCLUDED_BIT) {
		if (!(ucRequestMode & BTM_REQ_MODE_ESS_DISC_IMM))
			eTransType = BSS_TRANSITION_REQ_ROAMING;
		if (prSwRfb->u2PacketLen > u2TmpLen) {
			prMsg->u2CandListLen = prSwRfb->u2PacketLen - u2TmpLen;
			prMsg->pucCandList = pucOptInfo;
			prMsg->ucValidityInterval =
				prRxFrame->ucValidityInterval;
		} else
			DBGLOG(WNM, WARN,
			       "BTM: Candidate Include bit is set, but no candidate list\n");
	}

	DBGLOG(WNM, INFO,
	       "BTM: Req %d, VInt %d, DiscTimer %d, Token %d, TransType %d\n",
	       prBtmParam->ucRequestMode, prRxFrame->ucValidityInterval,
	       prBtmParam->u2DisassocTimer, prMsg->ucToken, eTransType);

	prMsg->eTransitionType = eTransType;
	prMsg->rMsgHdr.eMsgId = MID_WNM_AIS_BSS_TRANSITION;
	/* if BTM Request is dest for broadcast, don't send BTM Response */
	if (kalMemCmp(prRxFrame->aucDestAddr, "\xff\xff\xff\xff\xff\xff",
		      MAC_ADDR_LEN))
		prMsg->fgNeedResponse = TRUE;
	else
		prMsg->fgNeedResponse = FALSE;
	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prMsg,
		    MSG_SEND_METHOD_BUF);
}
