// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

struct TIMER rBeaconReqTimer;

struct REF_TSF {
	uint32_t au4Tsf[2];
	OS_SYSTIME rTime;
};
static struct REF_TSF rTsf;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

static u_int8_t rrmAllMeasurementIssued(
	struct RADIO_MEASUREMENT_REQ_PARAMS *prReq);

static void rrmCalibrateRepetions(
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq);

static void rrmHandleBeaconReqSubelem(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rrmParamInit(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReqParam =
			aisGetRmReqParam(prAdapter, ucBssIndex);
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam =
			aisGetRmReportParam(prAdapter, ucBssIndex);

	kalMemZero(prRmRepParam, sizeof(*prRmRepParam));
	kalMemZero(prRmReqParam, sizeof(*prRmReqParam));
	prRmReqParam->rBcnRmParam.eState = RM_NO_REQUEST;
	prRmReqParam->fgRmIsOngoing = FALSE;
	LINK_INITIALIZE(&prRmRepParam->rReportLink);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rrmParamUninit(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	rrmFreeMeasurementResources(prAdapter, ucBssIndex);
}

void rrmProcessNeighborReportResonse(struct ADAPTER *prAdapter,
				     struct WLAN_ACTION_FRAME *prAction,
				     struct SW_RFB *prSwRfb)
{
	struct ACTION_NEIGHBOR_REPORT_FRAME *prNeighborResponse =
		(struct ACTION_NEIGHBOR_REPORT_FRAME *)prAction;
	uint8_t ucBssIndex = secGetBssIdxByRfb(prAdapter,
		prSwRfb);

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	ASSERT(prNeighborResponse);

	DBGLOG(RRM, INFO,
		"[%d] Neighbor Resp From " MACSTR ", DialogToken %d\n",
		ucBssIndex,
		MAC2STR(prNeighborResponse->aucSrcAddr),
		prNeighborResponse->ucDialogToken);
#if CFG_SUPPORT_802_11K
	aisCollectNeighborAP(
		prAdapter, &prNeighborResponse->aucInfoElem[0],
		prSwRfb->u2PacketLen
			- OFFSET_OF(struct ACTION_NEIGHBOR_REPORT_FRAME,
			aucInfoElem),
		0,
		ucBssIndex);
#endif
}

void rrmTxNeighborReportRequest(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				struct SUB_ELEMENT_LIST *prSubIEs)
{
	static uint8_t ucDialogToken = 1;
	struct MSDU_INFO *prMsduInfo = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	uint8_t *pucPayload = NULL;
	struct ACTION_NEIGHBOR_REPORT_FRAME *prTxFrame = NULL;
	uint16_t u2TxFrameLen = 500;
	uint16_t u2FrameLen = 0;

	if (!prStaRec)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ASSERT(prBssInfo);
	/* 1 Allocate MSDU Info */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, MAC_TX_RESERVED_FIELD + u2TxFrameLen);
	if (!prMsduInfo)
		return;
	prTxFrame = (struct ACTION_NEIGHBOR_REPORT_FRAME
			     *)((unsigned long)(prMsduInfo->prPacket) +
				MAC_TX_RESERVED_FIELD);

	/* 2 Compose The Mac Header. */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);
	prTxFrame->ucCategory = CATEGORY_RM_ACTION;
	prTxFrame->ucAction = RM_ACTION_NEIGHBOR_REQUEST;
	u2FrameLen =
		OFFSET_OF(struct ACTION_NEIGHBOR_REPORT_FRAME, aucInfoElem);
	/* 3 Compose the frame body's frame. */
	prTxFrame->ucDialogToken = ucDialogToken++;
	u2TxFrameLen -= sizeof(*prTxFrame) - 1;
	pucPayload = &prTxFrame->aucInfoElem[0];
	while (prSubIEs && u2TxFrameLen >= (prSubIEs->rSubIE.ucLength + 2)) {
		kalMemCopy(pucPayload, &prSubIEs->rSubIE,
			   prSubIEs->rSubIE.ucLength + 2);
		pucPayload += prSubIEs->rSubIE.ucLength + 2;
		u2FrameLen += prSubIEs->rSubIE.ucLength + 2;
		prSubIEs = prSubIEs->prNext;
	}
	nicTxSetMngPacket(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
			  prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
			  u2FrameLen, NULL, MSDU_RATE_MODE_AUTO);

	/* 5 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
}

void rrmFreeMeasurementResources(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq =
		aisGetRmReqParam(prAdapter, ucBssIndex);
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRep =
		aisGetRmReportParam(prAdapter, ucBssIndex);
	struct RM_MEASURE_REPORT_ENTRY *prReportEntry = NULL;
	struct LINK *prReportLink = &prRmRep->rReportLink;
	u_int8_t fgHasBcnReqTimer = timerPendingTimer(&rBeaconReqTimer);

	DBGLOG(RRM, TRACE, "Free measurement, Beacon Req timer is %d\n",
		fgHasBcnReqTimer);
	if (fgHasBcnReqTimer)
		cnmTimerStopTimer(prAdapter, &rBeaconReqTimer);

	kalMemFree(prRmReq->pucReqIeBuf, VIR_MEM_TYPE, prRmReq->u2ReqIeBufLen);
	kalMemFree(prRmRep->pucReportFrameBuff, VIR_MEM_TYPE,
		   RM_REPORT_FRAME_MAX_LENGTH);
	while (!LINK_IS_EMPTY(prReportLink)) {
		LINK_REMOVE_HEAD(prReportLink, prReportEntry,
				 struct RM_MEASURE_REPORT_ENTRY *);
		kalMemFree(prReportEntry->pucMeasReport,
			VIR_MEM_TYPE, prReportEntry->u2MeasReportLen);
		kalMemFree(prReportEntry, VIR_MEM_TYPE, sizeof(*prReportEntry));
	}
	kalMemZero(prRmReq, sizeof(*prRmReq));
	kalMemZero(prRmRep, sizeof(*prRmRep));
	prRmReq->rBcnRmParam.eState = RM_NO_REQUEST;
	prRmReq->fgRmIsOngoing = FALSE;
	LINK_INITIALIZE(&prRmRep->rReportLink);
}

/* purpose: check if Radio Measurement is done */
static u_int8_t rrmAllMeasurementIssued(
	struct RADIO_MEASUREMENT_REQ_PARAMS *prReq)
{
	return prReq->u2RemainReqLen > IE_SIZE(prReq->prCurrMeasElem) ? FALSE
								      : TRUE;
}

void rrmComposeIncapableRmRep(struct RADIO_MEASUREMENT_REPORT_PARAMS *prRep,
			      uint8_t ucToken, uint8_t ucMeasType)
{
	struct IE_MEASUREMENT_REPORT *prRepIE =
		(struct IE_MEASUREMENT_REPORT *)(prRep->pucReportFrameBuff +
						 prRep->u2ReportFrameLen);

	prRepIE->ucId = ELEM_ID_MEASUREMENT_REPORT;
	prRepIE->ucToken = ucToken;
	prRepIE->ucMeasurementType = ucMeasType;
	prRepIE->ucLength = 3;
	prRepIE->ucReportMode = RM_REP_MODE_INCAPABLE;
	prRep->u2ReportFrameLen += 5;
}

int rrmBeaconRepUpdateLastFrame(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *rmReqParam =
		aisGetRmReqParam(prAdapter, ucBssIndex);
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam =
		aisGetRmReportParam(prAdapter, ucBssIndex);
	struct IE_MEASUREMENT_REQ *request = rmReqParam->prCurrMeasElem;
	uint8_t *pos = prRmRepParam->pucReportFrameBuff +
		OFFSET_OF(struct ACTION_RM_REPORT_FRAME, aucInfoElem);
	size_t len = prRmRepParam->u2ReportFrameLen -
		OFFSET_OF(struct ACTION_RM_REPORT_FRAME, aucInfoElem);
	struct IE_MEASUREMENT_REPORT *msr_rep;
	uint8_t *end = pos + len;
	uint8_t *msr_rep_end;
	struct RM_BCN_REPORT *rep = NULL;
	uint8_t *subelem;

	if (request->ucMeasurementType != ELEM_RM_TYPE_BEACON_REQ)
		return -EINVAL;

	/* Find the last beacon report element */
	while (end - pos >= (int) sizeof(*msr_rep)) {
		msr_rep = (struct IE_MEASUREMENT_REPORT *) pos;
		msr_rep_end = pos + msr_rep->ucLength + 2;

		if (msr_rep->ucId != ELEM_ID_MEASUREMENT_REPORT ||
		    msr_rep_end > end) {
#define TEMP_LOG_TEMPLATE \
	"non-measurement report element in measurement report frame\n"
			/* Should not happen. This indicates a bug. */
			DBGLOG(RRM, ERROR, TEMP_LOG_TEMPLATE);
#undef TEMP_LOG_TEMPLATE
			return -EINVAL;
		}

		if (msr_rep->ucMeasurementType == ELEM_RM_TYPE_BEACON_REPORT)
			rep = (struct RM_BCN_REPORT *)
				msr_rep->aucReportFields;

		pos += pos[1] + 2;
	}

	if (!rep)
		return 0;

	subelem = rep->aucOptElem;
	while (subelem + 2 < msr_rep_end &&
	       subelem[0] != BEACON_REPORT_SUBELEM_LAST_INDICATION)
		subelem += 2 + subelem[1];

	if (subelem + 2 < msr_rep_end &&
	    subelem[0] == BEACON_REPORT_SUBELEM_LAST_INDICATION &&
	    subelem[1] == 1 &&
	    subelem + BEACON_REPORT_LAST_INDICATION_SUBELEM_LEN <= end) {
		subelem[2] = 1;
		DBGLOG(RRM, INFO, "update last indication subelem\n");
	}

	return 0;
}

/* Purpose: Interative processing Measurement Request Element. If it is not the
 ** first element,
 ** will copy all collected report element to the report frame buffer. and
 ** may tx the radio report frame.
 ** prAdapter: pointer to the Adapter
 ** fgNewStarted: if it is the first element in measurement request frame
 */
void rrmStartNextMeasurement(struct ADAPTER *prAdapter, u_int8_t fgNewStarted,
	uint8_t ucBssIndex)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq =
		aisGetRmReqParam(prAdapter, ucBssIndex);
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRep =
		aisGetRmReportParam(prAdapter, ucBssIndex);
	struct IE_MEASUREMENT_REQ *prCurrReq = prRmReq->prCurrMeasElem;
	uint16_t u2RandomTime = 0;

schedule_next:
	if (!prRmReq->fgRmIsOngoing) {
		DBGLOG(RRM, INFO, "Rm has been stopped\n");
		return;
	}
	/* we don't support parallel measurement now */
	if (prCurrReq->ucRequestMode & RM_REQ_MODE_PARALLEL_BIT) {
		DBGLOG(RRM, WARN,
		       "Parallel request, compose incapable report\n");
		if (prRmRep->u2ReportFrameLen + 5 > RM_REPORT_FRAME_MAX_LENGTH)
			rrmTxRadioMeasurementReport(prAdapter, ucBssIndex);
		rrmComposeIncapableRmRep(prRmRep, prCurrReq->ucToken,
					 prCurrReq->ucMeasurementType);
		if (rrmAllMeasurementIssued(prRmReq)) {
			rrmTxRadioMeasurementReport(prAdapter, ucBssIndex);

			/* repeat measurement if repetitions is required and not
			 * only parallel measurements.
			 * otherwise, no need to repeat, it is not make sense to
			 * do that.
			 */
			if (prRmReq->u2Repetitions > 0) {
				prRmReq->fgInitialLoop = FALSE;
				prRmReq->u2Repetitions--;
				prCurrReq = prRmReq->prCurrMeasElem =
					(struct IE_MEASUREMENT_REQ *)
						prRmReq->pucReqIeBuf;
				prRmReq->u2RemainReqLen =
					prRmReq->u2ReqIeBufLen;
				rrmHandleBeaconReqSubelem(
					prAdapter, ucBssIndex);
			} else {
				rrmFreeMeasurementResources(prAdapter,
					ucBssIndex);
				DBGLOG(RRM, INFO,
				       "Radio Measurement done\n");
				return;
			}
		} else {
			uint16_t u2IeSize = IE_SIZE(prRmReq->prCurrMeasElem);

			prCurrReq = prRmReq->prCurrMeasElem =
				(struct IE_MEASUREMENT_REQ
					 *)((uint8_t *)prRmReq->prCurrMeasElem +
					    u2IeSize);
			prRmReq->u2RemainReqLen -= u2IeSize;
			rrmHandleBeaconReqSubelem(
				prAdapter, ucBssIndex);
		}
		fgNewStarted = FALSE;
		goto schedule_next;
	}
	/* copy collected measurement report for specific measurement type */
	if (!fgNewStarted) {
		struct RM_MEASURE_REPORT_ENTRY *prReportEntry = NULL;
		struct LINK *prReportLink = &prRmRep->rReportLink;
		uint8_t *pucReportFrame =
			prRmRep->pucReportFrameBuff + prRmRep->u2ReportFrameLen;
		uint16_t u2IeSize = 0;
		u_int8_t fgNewLoop = FALSE;

		DBGLOG(RRM, INFO,
		       "total %u report element for current request\n",
		       prReportLink->u4NumElem);
		/* copy collected report into the Measurement Report Frame
		 ** Buffer.
		 */
		while (1) {
			LINK_REMOVE_HEAD(prReportLink, prReportEntry,
					 struct RM_MEASURE_REPORT_ENTRY *);
			if (!prReportEntry)
				break;
			u2IeSize = prReportEntry->u2MeasReportLen;
			/* if reach the max length of a MMPDU size, send a Rm
			 ** report first
			 */
			if (u2IeSize + prRmRep->u2ReportFrameLen >
			    RM_REPORT_FRAME_MAX_LENGTH) {
				rrmTxRadioMeasurementReport(prAdapter,
					ucBssIndex);
				pucReportFrame = prRmRep->pucReportFrameBuff +
						 prRmRep->u2ReportFrameLen;
			}
			kalMemCopy(pucReportFrame, prReportEntry->pucMeasReport,
				   u2IeSize);
			pucReportFrame += u2IeSize;
			prRmRep->u2ReportFrameLen += u2IeSize;

			kalMemFree(prReportEntry->pucMeasReport,
				VIR_MEM_TYPE, u2IeSize);
			kalMemFree(prReportEntry,
				VIR_MEM_TYPE, sizeof(*prReportEntry));
		}
		rrmBeaconRepUpdateLastFrame(prAdapter, ucBssIndex);
		/* if Measurement is done, free report element memory */
		if (rrmAllMeasurementIssued(prRmReq)) {
			rrmTxRadioMeasurementReport(prAdapter, ucBssIndex);
			/* repeat measurement if repetitions is required */
			if (prRmReq->u2Repetitions > 0) {
				fgNewLoop = TRUE;
				prRmReq->fgInitialLoop = FALSE;
				prRmReq->u2Repetitions--;
				prRmReq->prCurrMeasElem =
					(struct IE_MEASUREMENT_REQ *)
						prRmReq->pucReqIeBuf;
				prRmReq->u2RemainReqLen =
					prRmReq->u2ReqIeBufLen;
				rrmHandleBeaconReqSubelem(
					prAdapter, ucBssIndex);
			} else {
				/* don't free radio measurement resource due to
				 ** TSM is running
				 */
				if (!wmmTsmIsOngoing(prAdapter, ucBssIndex)) {
					rrmFreeMeasurementResources(prAdapter,
						ucBssIndex);
					DBGLOG(RRM, INFO,
					       "Radio Measurement done\n");
				}
				return;
			}
		}
		if (!fgNewLoop) {
			u2IeSize = IE_SIZE(prRmReq->prCurrMeasElem);
			prCurrReq = prRmReq->prCurrMeasElem =
				(struct IE_MEASUREMENT_REQ
					 *)((uint8_t *)prRmReq->prCurrMeasElem +
					    u2IeSize);
			prRmReq->u2RemainReqLen -= u2IeSize;
			rrmHandleBeaconReqSubelem(prAdapter, ucBssIndex);
		}
	}

	/* do specific measurement */
	switch (prCurrReq->ucMeasurementType) {
	case ELEM_RM_TYPE_BEACON_REQ: {
		struct RM_BCN_REQ *prBeaconReq =
			(struct RM_BCN_REQ *)&prCurrReq->aucRequestFields[0];

		if (!prRmReq->fgInitialLoop) {
			/* If this is the repeating measurement, then wait next
			 ** scan done
			 */
			prRmReq->rBcnRmParam.eState = RM_WAITING;
			break;
		}
		if (prBeaconReq->u2RandomInterval == 0)
			rrmDoBeaconMeasurement(prAdapter, ucBssIndex);
		else {
			get_random_bytes(&u2RandomTime, 2);
			u2RandomTime =
				(u2RandomTime * prBeaconReq->u2RandomInterval) /
				65535;
			u2RandomTime = TU_TO_MSEC(u2RandomTime);
			if (u2RandomTime > 0) {
				cnmTimerStopTimer(prAdapter, &rBeaconReqTimer);
				cnmTimerInitTimer(prAdapter, &rBeaconReqTimer,
					rrmDoBeaconMeasurement, ucBssIndex);
				cnmTimerStartTimer(prAdapter, &rBeaconReqTimer,
						   u2RandomTime);
			} else
				rrmDoBeaconMeasurement(prAdapter, ucBssIndex);
		}
		break;
	}
#if 0
	case ELEM_RM_TYPE_TSM_REQ:
	{
		struct RM_TS_MEASURE_REQ *prTsmReqIE =
			(struct RM_TS_MEASURE_REQ *)
			&prCurrReq->aucRequestFields[0];
		struct RM_TSM_REQ *prTsmReq = NULL;
		uint16_t u2OffSet = 0;
		uint8_t *pucIE = prTsmReqIE->aucSubElements;
		struct ACTION_RM_REPORT_FRAME *prReportFrame = NULL;

		/* In case of repeating measurement, no need to start
		 ** triggered measurement again. According to current
		 ** specification of Radio Measurement, only TSM has the
		 ** triggered type of measurement.
		 */
		if ((prCurrReq->ucRequestMode & RM_REQ_MODE_ENABLE_BIT) &&
			!prRmReq->fgInitialLoop)
			goto schedule_next;

		/* if enable bit is 1 and report bit is 0, need to stop all
		 ** triggered TSM measurement
		 */
		if ((prCurrReq->ucRequestMode &
			(RM_REQ_MODE_ENABLE_BIT|RM_REQ_MODE_REPORT_BIT)) ==
			RM_REQ_MODE_ENABLE_BIT) {
			wmmRemoveAllTsmMeasurement(prAdapter, TRUE);
			break;
		}
		prTsmReq = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
			sizeof(struct RM_TSM_REQ));
		if (!prTsmReq) {
			DBGLOG(RLM, ERROR, "No memory\n");
			break;
		}
		prTsmReq->ucToken = prCurrReq->ucToken;
		prTsmReq->u2Duration = prTsmReqIE->u2Duration;
		prTsmReq->ucTID = (prTsmReqIE->ucTrafficID & 0xf0) >> 4;
		prTsmReq->ucB0Range = prTsmReqIE->ucBin0Range;
		prReportFrame = (struct ACTION_RM_REPORT_FRAME *)
			prRmRep->pucReportFrameBuff;
		COPY_MAC_ADDR(prTsmReq->aucPeerAddr,
			prReportFrame->aucDestAddr);
		IE_FOR_EACH(pucIE, prCurrReq->ucLength - 3, u2OffSet) {
			switch (IE_ID(pucIE)) {
			case 1: /* Triggered Reporting */
				kalMemCopy(&prTsmReq->rTriggerCond,
					pucIE+2, IE_LEN(pucIE));
				break;
			case 221: /* Vendor Specified */
				break; /* No vendor IE now */
			default:
				break;
			}
		}
		if (!prTsmReqIE->u2RandomInterval) {
			wmmStartTsmMeasurement(prAdapter,
				(unsigned long)prTsmReq);
			break;
		}
		get_random_bytes(&u2RandomTime, 2);
		u2RandomTime =
		(u2RandomTime * prTsmReqIE->u2RandomInterval) / 65535;
		u2RandomTime = TU_TO_MSEC(u2RandomTime);
		cnmTimerStopTimer(prAdapter, &rTSMReqTimer);
		cnmTimerInitTimer(prAdapter, &rTSMReqTimer,
			wmmStartTsmMeasurement, (unsigned long)prTsmReq);
		cnmTimerStartTimer(prAdapter, &rTSMReqTimer, u2RandomTime);
		break;
	}
#endif
	default: {
		if (prRmRep->u2ReportFrameLen + 5 > RM_REPORT_FRAME_MAX_LENGTH)
			rrmTxRadioMeasurementReport(prAdapter,
				ucBssIndex);
		rrmComposeIncapableRmRep(prRmRep, prCurrReq->ucToken,
					 prCurrReq->ucMeasurementType);
		fgNewStarted = FALSE;
		DBGLOG(RRM, INFO,
		       "RM type %d is not supported on this chip\n",
		       prCurrReq->ucMeasurementType);
		goto schedule_next;
	}
	}
}

u_int8_t rrmFillScanMsg(struct ADAPTER *prAdapter,
			struct MSG_SCN_SCAN_REQ_V2 *prMsg)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = NULL;
	struct IE_MEASUREMENT_REQ *prCurrReq = NULL;
	struct RM_BCN_REQ *prBeaconReq = NULL;
	uint16_t u2RemainLen = 0;
	uint8_t *pucSubIE = NULL;

	static struct PARAM_SSID rBcnReqSsid;

	if (!prMsg)
		return FALSE;

	prRmReq = aisGetRmReqParam(prAdapter,
		prMsg->ucBssIndex);

	if (prRmReq->rBcnRmParam.eState != RM_ON_GOING)
		return FALSE;

	prCurrReq = prRmReq->prCurrMeasElem;
	prBeaconReq = (struct RM_BCN_REQ *)&prCurrReq->aucRequestFields[0];
	prMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
	switch (prBeaconReq->ucMeasurementMode) {
	case RM_BCN_REQ_PASSIVE_MODE:
		prMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;
		break;
	case RM_BCN_REQ_ACTIVE_MODE:
		prMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
		break;
	default:
		DBGLOG(RRM, WARN,
		       "Unexpect measure mode %d, use active mode as default\n",
			   prBeaconReq->ucMeasurementMode);
		prMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
		break;
	}

	WLAN_GET_FIELD_16(&prBeaconReq->u2Duration, &prMsg->u2ChannelDwellTime);

	COPY_MAC_ADDR(prMsg->aucBSSID, prBeaconReq->aucBssid);

	prMsg->u2ProbeDelay = 0;
	prMsg->u2TimeoutValue = 0;
	prMsg->ucSSIDNum = 0;
	prMsg->u2IELen = 0;
	/* if mandatory bit is set, we should do */
	if (prCurrReq->ucRequestMode & RM_REQ_MODE_DURATION_MANDATORY_BIT)
		prMsg->u2ChannelMinDwellTime = prMsg->u2ChannelDwellTime;
	else
		prMsg->u2ChannelMinDwellTime =
			(prMsg->u2ChannelDwellTime * 2) / 3;
	if (prBeaconReq->ucChannel == 0)
		prMsg->eScanChannel = SCAN_CHANNEL_FULL;
	else if (prBeaconReq->ucChannel == 255) { /* latest Ap Channel Report */
		struct BSS_DESC *prBssDesc =
			aisGetTargetBssDesc(prAdapter,
			prMsg->ucBssIndex);
		uint8_t *pucChnl = NULL;
		uint8_t ucChnlNum = 0;
		uint8_t ucIndex = 0;
		struct RF_CHANNEL_INFO *prChnlInfo = prMsg->arChnlInfoList;

		prMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
		prMsg->ucChannelListNum = 0;
		if (prBssDesc) {
			uint8_t *pucIE = NULL;
			uint16_t u2IELength = 0;
			uint16_t u2Offset = 0;

			pucIE = prBssDesc->aucIEBuf;
			u2IELength = prBssDesc->u2IELength;
			IE_FOR_EACH(pucIE, u2IELength, u2Offset)
			{
			if (IE_ID(pucIE) != ELEM_ID_AP_CHANNEL_REPORT)
				continue;
			pucChnl = ((struct IE_AP_CHNL_REPORT *)pucIE)
				->aucChnlList;
			ucChnlNum = pucIE[1] - 1;
			DBGLOG(RRM, INFO,
				"Channel number in latest AP channel report %d\n",
				ucChnlNum);
			while (ucIndex < ucChnlNum &&
				prMsg->ucChannelListNum <
				MAXIMUM_OPERATION_CHANNEL_LIST) {
				if (pucChnl[ucIndex] <= 14)
					prChnlInfo
						[prMsg->ucChannelListNum]
							.eBand =
						BAND_2G4;
				else
					prChnlInfo
						[prMsg->ucChannelListNum]
							.eBand =
						BAND_5G;
				prChnlInfo[prMsg->ucChannelListNum]
					.ucChannelNum =
					pucChnl[ucIndex];
				prMsg->ucChannelListNum++;
				ucIndex++;
			}
			}
		}
	} else {
		prMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
		prMsg->ucChannelListNum = 1;
		prMsg->arChnlInfoList[0].ucChannelNum = prBeaconReq->ucChannel;
		if (prBeaconReq->ucChannel <= 14)
			prMsg->arChnlInfoList[0].eBand = BAND_2G4;
		else
			prMsg->arChnlInfoList[0].eBand = BAND_5G;
	}
	u2RemainLen = prCurrReq->ucLength - 3 -
		      OFFSET_OF(struct RM_BCN_REQ, aucSubElements);
	pucSubIE = &prBeaconReq->aucSubElements[0];
	while (u2RemainLen > 0) {
		if (IE_SIZE(pucSubIE) > u2RemainLen)
			break;
		switch (pucSubIE[0]) {
		case BEACON_REQUEST_SUBELEM_SSID: /* SSID */
			/* length of sub-element ssid is 0 or first byte is 0,
			 ** means wildcard ssid matching
			 */
			if (!IE_LEN(pucSubIE) || !pucSubIE[2])
				break;
			prMsg->ucSSIDNum = 1;
			prMsg->prSsid = &rBcnReqSsid;
			COPY_SSID(&rBcnReqSsid.aucSsid[0],
				  rBcnReqSsid.u4SsidLen, &pucSubIE[2],
				  pucSubIE[1]);
			prMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED_ONLY;
			break;
		case BEACON_REQUEST_SUBELEM_AP_CHANNEL: /* AP channel report */
		{
			struct IE_AP_CHNL_REPORT *prApChnl =
				(struct IE_AP_CHNL_REPORT *)pucSubIE;
			uint8_t ucChannelCnt = prApChnl->ucLength - 1;
			uint8_t ucIndex = 0;

			if (prBeaconReq->ucChannel == 0)
				break;
			prMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
			DBGLOG(RRM, INFO,
			       "Channel number in measurement AP channel report %d\n",
			       ucChannelCnt);
			while (ucIndex < ucChannelCnt &&
			       prMsg->ucChannelListNum <
				       MAXIMUM_OPERATION_CHANNEL_LIST) {
				if (prApChnl->aucChnlList[ucIndex] <= 14)
					prMsg->arChnlInfoList
						[prMsg->ucChannelListNum]
							.eBand = BAND_2G4;
				else
					prMsg->arChnlInfoList
						[prMsg->ucChannelListNum]
							.eBand = BAND_5G;
				prMsg->arChnlInfoList[prMsg->ucChannelListNum]
					.ucChannelNum =
					prApChnl->aucChnlList[ucIndex];
				prMsg->ucChannelListNum++;
				ucIndex++;
			}
			break;
		}
		}
		u2RemainLen -= IE_SIZE(pucSubIE);
		pucSubIE += IE_SIZE(pucSubIE);
	}

	GET_CURRENT_SYSTIME(&prRmReq->rScanStartTime);
	DBGLOG(RRM, INFO,
	       "SSIDtype %d, ScanType %d, Dwell %d, MinDwell %d, ChnlType %d, ChnlNum %d\n",
		prMsg->ucSSIDType, prMsg->eScanType, prMsg->u2ChannelDwellTime,
	       prMsg->u2ChannelMinDwellTime, prMsg->eScanChannel,
	       prMsg->ucChannelListNum);

	return TRUE;
}

void rrmDoBeaconMeasurement(struct ADAPTER *prAdapter, unsigned long ulParam)
{
	uint8_t ucBssIndex = (uint8_t) ulParam;
	struct CONNECTION_SETTINGS *prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq =
		aisGetRmReqParam(prAdapter, ucBssIndex);
	struct RM_BCN_REQ *prBcnReq =
		(struct RM_BCN_REQ *)&prRmReq->prCurrMeasElem
			->aucRequestFields[0];

	if (prBcnReq->ucMeasurementMode == RM_BCN_REQ_TABLE_MODE) {
		struct LINK *prBSSDescList =
			&prAdapter->rWifiVar.rScanInfo.rBSSDescList;
		struct BSS_DESC *prBssDesc = NULL;

		prRmReq->rBcnRmParam.eState = RM_ON_GOING;
		prBcnReq->ucChannel = 0;
		DBGLOG(RRM, INFO,
		       "Beacon Table Mode, Beacon Table Num %u\n",
		       prBSSDescList->u4NumElem);
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry,
				    struct BSS_DESC)
		{
			rrmCollectBeaconReport(prAdapter,
				prBssDesc, ucBssIndex);
		}
		rrmStartNextMeasurement(prAdapter, FALSE, ucBssIndex);
		return;
	}
	if (prConnSettings->fgIsScanReqIssued) {
		prRmReq->rBcnRmParam.eState = RM_WAITING;
	} else {
		prRmReq->rBcnRmParam.eState = RM_ON_GOING;
		GET_CURRENT_SYSTIME(&prRmReq->rStartTime);
		aisFsmScanRequest(prAdapter, NULL, NULL, 0,
			ucBssIndex);
	}
}

static u_int8_t rrmRmFrameIsValid(struct SW_RFB *prSwRfb)
{
	uint16_t u2ElemLen = 0;
	uint16_t u2Offset =
		(uint16_t)OFFSET_OF(struct ACTION_RM_REQ_FRAME, aucInfoElem);
	uint8_t *pucIE = (uint8_t *)prSwRfb->pvHeader;
	struct IE_MEASUREMENT_REQ *prCurrMeasElem = NULL;
	uint16_t u2CalcIELen = 0;
	uint16_t u2IELen = 0;

	if (prSwRfb->u2PacketLen <= u2Offset) {
		DBGLOG(RRM, ERROR, "Rm Packet length %d is too short\n",
		       prSwRfb->u2PacketLen);
		return FALSE;
	}
	pucIE += u2Offset;
	u2ElemLen = prSwRfb->u2PacketLen - u2Offset;
	IE_FOR_EACH(pucIE, u2ElemLen, u2Offset)
	{
		u2IELen = IE_LEN(pucIE);

		/* The minimum value of the Length field is 3 (based on a
		 ** minimum length for the Measurement Request field
		 ** of 0 octets
		 */
		if (u2IELen < 3) {
			DBGLOG(RRM, ERROR, "Abnormal RM IE length is %d\n",
			       u2IELen);
			return FALSE;
		}

		/* Check whether the length of each measurment request element
		 ** is reasonable
		 */
		prCurrMeasElem = (struct IE_MEASUREMENT_REQ *)pucIE;
		switch (prCurrMeasElem->ucMeasurementType) {
		case ELEM_RM_TYPE_BEACON_REQ:
			if (u2IELen < (3 + OFFSET_OF(struct RM_BCN_REQ,
						     aucSubElements))) {
				DBGLOG(RRM, ERROR,
				       "Abnormal Becaon Req IE length is %d\n",
				       u2IELen);
				return FALSE;
			}
			break;
		case ELEM_RM_TYPE_TSM_REQ:
			if (u2IELen < (3 + OFFSET_OF(struct RM_TS_MEASURE_REQ,
						     aucSubElements))) {
				DBGLOG(RRM, ERROR,
				       "Abnormal TSM Req IE length is %d\n",
				       u2IELen);
				return FALSE;
			}
			break;
		default:
			DBGLOG(RRM, ERROR,
			       "Not support: MeasurementType is %d, IE length is %d\n",
				prCurrMeasElem->ucMeasurementType, u2IELen);
			return FALSE;
		}

		u2CalcIELen += IE_SIZE(pucIE);
	}
	if (u2CalcIELen != u2ElemLen) {
		DBGLOG(RRM, ERROR,
		       "Calculated Total IE len is not equal to received length\n");
		return FALSE;
	}
	return TRUE;
}

void rrmProcessRadioMeasurementRequest(struct ADAPTER *prAdapter,
				       struct SW_RFB *prSwRfb)
{
	struct ACTION_RM_REQ_FRAME *prRmReqFrame = NULL;
	struct ACTION_RM_REPORT_FRAME *prReportFrame = NULL;
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReqParam = NULL;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam = NULL;
	enum RM_REQ_PRIORITY eNewPriority;
	struct BSS_INFO *prAisBssInfo = NULL;
	struct STA_RECORD *prStaRec = NULL;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prAisBssInfo = aisGetAisBssInfo(prAdapter,
		secGetBssIdxByRfb(prAdapter, prSwRfb));
	if (prAisBssInfo == NULL) {
		DBGLOG(RRM, INFO, "Ignored due to AIS isn't created\n");
		return;
	}

	prRmReqFrame = (struct ACTION_RM_REQ_FRAME *)prSwRfb->pvHeader;
	prRmReqParam = aisGetRmReqParam(prAdapter,
		prAisBssInfo->ucBssIndex);
	prRmRepParam = aisGetRmReportParam(prAdapter,
		prAisBssInfo->ucBssIndex);

	if (!rrmRmFrameIsValid(prSwRfb))
		return;
	prStaRec = prAisBssInfo->prStaRecOfAP;
	if (!prStaRec) {
		DBGLOG(RRM, INFO, "StaRec is NULL, ignore request\n");
		return;
	}
	DBGLOG(RRM, INFO, "RM Request From "MACSTR", DialogToken %d\n",
			MAC2STR(prRmReqFrame->aucSrcAddr),
			prRmReqFrame->ucDialogToken);
	eNewPriority = rrmGetRmRequestPriority(prRmReqFrame->aucDestAddr);
	if (prRmReqParam->ePriority > eNewPriority) {
		DBGLOG(RRM, INFO, "ignore lower precedence rm request\n");
		return;
	}
	prRmReqParam->ePriority = eNewPriority;
	/* */
	if (prRmReqParam->fgRmIsOngoing) {
		DBGLOG(RRM, INFO, "Old RM is on-going, cancel it first\n");
		rrmTxRadioMeasurementReport(prAdapter,
			prAisBssInfo->ucBssIndex);
		wmmRemoveAllTsmMeasurement(prAdapter, FALSE,
			prAisBssInfo->ucBssIndex);
		rrmFreeMeasurementResources(prAdapter,
			prAisBssInfo->ucBssIndex);
	}
	prRmReqParam->fgRmIsOngoing = TRUE;
	/* Step1: Save Measurement Request Params */
	prRmReqParam->u2ReqIeBufLen = prRmReqParam->u2RemainReqLen =
		prSwRfb->u2PacketLen -
		OFFSET_OF(struct ACTION_RM_REQ_FRAME, aucInfoElem);
	if (prRmReqParam->u2RemainReqLen < sizeof(struct IE_MEASUREMENT_REQ)) {
		DBGLOG(RRM, ERROR,
		       "empty Radio Measurement Request Frame, Elem Len %d\n",
			prRmReqParam->u2RemainReqLen);
		return;
	}
	WLAN_GET_FIELD_BE16(&prRmReqFrame->u2Repetitions,
			    &prRmReqParam->u2Repetitions);
	prRmReqParam->pucReqIeBuf =
		kalMemAlloc(prRmReqParam->u2RemainReqLen, VIR_MEM_TYPE);
	if (!prRmReqParam->pucReqIeBuf) {
		DBGLOG(RRM, ERROR,
		       "Alloc %d bytes Req IE Buffer failed, No Memory\n",
		       prRmReqParam->u2RemainReqLen);
		return;
	}
	kalMemCopy(prRmReqParam->pucReqIeBuf, &prRmReqFrame->aucInfoElem[0],
		   prRmReqParam->u2RemainReqLen);
	prRmReqParam->prCurrMeasElem =
		(struct IE_MEASUREMENT_REQ *)prRmReqParam->pucReqIeBuf;
	prRmReqParam->fgInitialLoop = TRUE;
	rrmHandleBeaconReqSubelem(prAdapter, prAisBssInfo->ucBssIndex);

	/* Step2: Prepare Report Frame and fill in Frame Header */
	prRmRepParam->pucReportFrameBuff =
		kalMemAlloc(RM_REPORT_FRAME_MAX_LENGTH, VIR_MEM_TYPE);
	if (!prRmRepParam->pucReportFrameBuff) {
		DBGLOG(RRM, ERROR,
		       "Alloc Memory for Measurement Report Frame buffer failed\n");
		return;
	}
	kalMemZero(prRmRepParam->pucReportFrameBuff,
		   RM_REPORT_FRAME_MAX_LENGTH);
	prReportFrame = (struct ACTION_RM_REPORT_FRAME *)
				prRmRepParam->pucReportFrameBuff;
	prReportFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	COPY_MAC_ADDR(prReportFrame->aucDestAddr, prRmReqFrame->aucSrcAddr);
	COPY_MAC_ADDR(prReportFrame->aucSrcAddr,
		      prAisBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prReportFrame->aucBSSID, prRmReqFrame->aucBSSID);
	prReportFrame->ucCategory = CATEGORY_RM_ACTION;
	prReportFrame->ucAction = RM_ACTION_RM_REPORT;
	prReportFrame->ucDialogToken = prRmReqFrame->ucDialogToken;
	prRmRepParam->u2ReportFrameLen =
		OFFSET_OF(struct ACTION_RM_REPORT_FRAME, aucInfoElem);
	rrmCalibrateRepetions(prRmReqParam);
	/* Step3: Start to process Measurement Request Element */
	rrmStartNextMeasurement(prAdapter, TRUE, prAisBssInfo->ucBssIndex);
}

void rrmTxRadioMeasurementReport(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct MSDU_INFO *prMsduInfo = NULL;
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRepParam =
		aisGetRmReportParam(prAdapter, ucBssIndex);
	struct STA_RECORD *prStaRec = NULL;
	struct BSS_INFO *prAisBssInfo = NULL;

	if (prRmRepParam->u2ReportFrameLen <=
	    OFFSET_OF(struct ACTION_RM_REPORT_FRAME, aucInfoElem)) {
		DBGLOG(RRM, INFO, "report frame length is too short, %d\n",
		       prRmRepParam->u2ReportFrameLen);
		goto out;
	}
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	if (!prAisBssInfo) {
		DBGLOG(RRM, INFO, "ais bss info is NULL\n");
		goto out;
	}
	prStaRec = prAisBssInfo->prStaRecOfAP;
	if (!prStaRec) {
		DBGLOG(RRM, INFO, "StaRec of Ais is NULL\n");
		goto out;
	}
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(
		prAdapter, prRmRepParam->u2ReportFrameLen);
	if (!prMsduInfo) {
		DBGLOG(RRM, INFO,
		       "Alloc MSDU Info failed, frame length %d\n",
		       prRmRepParam->u2ReportFrameLen);
		goto out;
	}
	DBGLOG(RRM, INFO, "frame length %d\n",
	       prRmRepParam->u2ReportFrameLen);
	kalMemCopy(prMsduInfo->prPacket, prRmRepParam->pucReportFrameBuff,
		   prRmRepParam->u2ReportFrameLen);

	/* 2 Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter, prMsduInfo, prStaRec->ucBssIndex,
		     prStaRec->ucIndex, WLAN_MAC_MGMT_HEADER_LEN,
		     prRmRepParam->u2ReportFrameLen, NULL, MSDU_RATE_MODE_AUTO);
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

out:
	/* reset u2ReportFrameLen after tx frame */
	prRmRepParam->u2ReportFrameLen =
		OFFSET_OF(struct ACTION_RM_REPORT_FRAME, aucInfoElem);
}

void rrmGenerateRRMEnabledCapIE(IN struct ADAPTER *prAdapter,
				IN struct MSDU_INFO *prMsduInfo)
{
	struct IE_RRM_ENABLED_CAP *prRrmEnabledCap = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prRrmEnabledCap = (struct IE_RRM_ENABLED_CAP *)
	    (((uint8_t *) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);
	prRrmEnabledCap->ucId = ELEM_ID_RRM_ENABLED_CAP;
	prRrmEnabledCap->ucLength = ELEM_MAX_LEN_RRM_CAP;
	kalMemZero(&prRrmEnabledCap->aucCap[0], ELEM_MAX_LEN_RRM_CAP);
	rrmFillRrmCapa(&prRrmEnabledCap->aucCap[0]);
	prMsduInfo->u2FrameLength += IE_SIZE(prRrmEnabledCap);
}

void rrmFillRrmCapa(uint8_t *pucCapa)
{
	uint8_t ucIndex = 0;
	uint8_t aucEnabledBits[] = {RRM_CAP_INFO_LINK_MEASURE_BIT,
				    RRM_CAP_INFO_NEIGHBOR_REPORT_BIT,
				    RRM_CAP_INFO_REPEATED_MEASUREMENT,
				    RRM_CAP_INFO_BEACON_PASSIVE_MEASURE_BIT,
				    RRM_CAP_INFO_BEACON_ACTIVE_MEASURE_BIT,
				    RRM_CAP_INFO_BEACON_TABLE_BIT,
				    RRM_CAP_INFO_RRM_BIT};

	for (; ucIndex < sizeof(aucEnabledBits); ucIndex++)
		SET_EXT_CAP(pucCapa, ELEM_MAX_LEN_RRM_CAP,
			    aucEnabledBits[ucIndex]);
}

enum RM_REQ_PRIORITY rrmGetRmRequestPriority(uint8_t *pucDestAddr)
{
	if (IS_UCAST_MAC_ADDR(pucDestAddr))
		return RM_PRI_UNICAST;
	else if (EQUAL_MAC_ADDR(pucDestAddr, "\xff\xff\xff\xff\xff\xff"))
		return RM_PRI_BROADCAST;
	return RM_PRI_MULTICAST;
}

static void rrmCalibrateRepetions(struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq)
{
	uint16_t u2IeSize = 0;
	uint16_t u2RemainReqLen = prRmReq->u2ReqIeBufLen;
	struct IE_MEASUREMENT_REQ *prCurrReq =
		(struct IE_MEASUREMENT_REQ *)prRmReq->prCurrMeasElem;

	if (prRmReq->u2Repetitions == 0)
		return;

	u2IeSize = IE_SIZE(prCurrReq);
	while (u2RemainReqLen >= u2IeSize) {
		/* 1. If all measurement request has enable bit, no need to
		 ** repeat
		 ** see 11.10.6 Measurement request elements with the enable bit
		 ** set to 1 shall be processed once
		 ** regardless of the value in the number of repetitions in the
		 ** measurement request.
		 ** 2. Due to we don't support parallel measurement, if all
		 ** request has parallel bit, no need to repeat
		 ** measurement, to avoid frequent composing incapable response
		 ** IE and exhauste CPU resource
		 ** and then cause watch dog timeout.
		 ** 3. if all measurements are not supported, no need to repeat.
		 ** currently we only support Beacon request
		 ** on this chip.
		 */
		if (!(prCurrReq->ucRequestMode &
		      (RM_REQ_MODE_ENABLE_BIT | RM_REQ_MODE_PARALLEL_BIT))) {
			if (prCurrReq->ucMeasurementType ==
			    ELEM_RM_TYPE_BEACON_REQ)
				return;
		}
		u2RemainReqLen -= u2IeSize;
		prCurrReq = (struct IE_MEASUREMENT_REQ *)((uint8_t *)prCurrReq +
							  u2IeSize);
		u2IeSize = IE_SIZE(prCurrReq);
	}
	DBGLOG(RRM, INFO,
		"All Measurement has set enable bit, or all are parallel or not supported, don't repeat\n");
	prRmReq->u2Repetitions = 0;
}

void rrmRunEventProcessNextRm(struct ADAPTER *prAdapter,
			      struct MSG_HDR *prMsgHdr)
{
	struct MSG_SCN_SCAN_DONE *prMsg;
	uint8_t ucBssIndex = 0;

	ASSERT(prMsgHdr);

	prMsg = (struct MSG_SCN_SCAN_DONE *)prMsgHdr;
	ucBssIndex = prMsg->ucBssIndex;
	cnmMemFree(prAdapter, prMsgHdr);
	rrmStartNextMeasurement(prAdapter, FALSE, ucBssIndex);
}

void rrmScheduleNextRm(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct MSG_SCN_SCAN_DONE *prMsg = NULL;

	prMsg = (struct MSG_SCN_SCAN_DONE *) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, sizeof(struct MSG_SCN_SCAN_DONE));
	if (!prMsg) {
		DBGLOG(RRM, ERROR, "[RRM] No memory\n");
		return;
	}
	prMsg->rMsgHdr.eMsgId = MID_RRM_REQ_SCHEDULE;
	prMsg->ucBssIndex = ucBssIndex;
	mboxSendMsg(prAdapter, MBOX_ID_0,
		(struct MSG_HDR *)prMsg, MSG_SEND_METHOD_BUF);
}

static void rrmHandleBeaconReqSubelem(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *rmReqParam = NULL;
	struct IE_MEASUREMENT_REQ *request = NULL;
	struct RM_BCN_REQ *prBcnReq = NULL;
	struct BCN_RM_PARAMS *data = NULL;
	uint16_t elemsLen = 0;
	uint8_t *subelems = NULL;
	uint8_t slen = 0;
	uint8_t reportInfo = 0;

	rmReqParam = aisGetRmReqParam(prAdapter, ucBssIndex);
	request = rmReqParam->prCurrMeasElem;
	prBcnReq = (struct RM_BCN_REQ *)&request->aucRequestFields[0];
	data = &rmReqParam->rBcnRmParam;

	if (request->ucMeasurementType != ELEM_RM_TYPE_BEACON_REQ)
		return;

	/* reset data */
	data->token = request->ucToken;
	data->lastIndication = 0;
	data->ssidLen = 0;
	data->reportDetail = BEACON_REPORT_DETAIL_ALL_FIELDS_AND_ELEMENTS;
	data->reportIeIdsLen = 0;
	data->apChannelsLen = 0;

	elemsLen = request->ucLength - 3 -
		OFFSET_OF(struct RM_BCN_REQ, aucSubElements);
	subelems = &prBcnReq->aucSubElements[0];
	while (elemsLen >= 2) {
		slen = subelems[1];
		if (slen > elemsLen - 2) {
			DBGLOG(RRM, WARN,
				"Beacon Request: Truncated subelement");
			return;
		}

		switch (subelems[0]) {
		case BEACON_REQUEST_SUBELEM_SSID:
		{
			if (!slen) {
				DBGLOG(RRM, WARN, "Wildcard SSID");
				break;
			}

			if (slen > ELEM_MAX_LEN_SSID) {
				DBGLOG(RRM, WARN,
					"Invalid SSID length: %u", slen);
				break;
			}

			COPY_SSID(data->ssid, data->ssidLen,
				&subelems[2], slen);
			break;
		}
		case BEACON_REQUEST_SUBELEM_INFO:
		{
			if (slen != 2) {
				DBGLOG(RRM, WARN,
					"%u Invalid reporting len: %u",
					subelems[0], slen);
				break;
			}
			/* Beacon reporting data field format
			 * Reporting condition 1, Threshold/Offset Reference 1
			 */
			reportInfo = subelems[2];
			if (reportInfo != 0) {
				DBGLOG(RRM, WARN,
					"reporting info=%u not supported",
					reportInfo);
				break;
			}
			break;
		}
		case BEACON_REQUEST_SUBELEM_DETAIL:
		{
			if (slen != 1) {
				DBGLOG(RRM, WARN, "%u Wrong len %u",
					subelems[0], slen);
				break;
			}

			if (subelems[2] >
			    BEACON_REPORT_DETAIL_ALL_FIELDS_AND_ELEMENTS) {
				DBGLOG(RRM, WARN,
					"Invalid detail: %u", subelems[2]);
				break;
			}

			data->reportDetail = subelems[2];

			break;
		}
		case BEACON_REQUEST_SUBELEM_REQUEST:
		{
			if (data->reportDetail !=
			    BEACON_REPORT_DETAIL_REQUESTED_ONLY) {
				DBGLOG(RRM, WARN,
					"unexpected report detail is %u",
					   data->reportDetail);
				break;
			}

			if (!slen) {
				DBGLOG(RRM, WARN, "subelem %u Wrong len %u",
					subelems[0], slen);
				break;
			}

			data->reportIeIds = &subelems[2];
			data->reportIeIdsLen = slen;
			break;
		}
		case BEACON_REQUEST_SUBELEM_AP_CHANNEL:
		{
			uint8_t *strbuf, *pos, *end;
			uint8_t i;

			if (slen < 2) {
				DBGLOG(RRM, WARN, "subelem %u Wrong len %u",
					subelems[0], slen);
				break;
			}

			/* AP Channel Report element
			 * EleID 1, Length 1, Op class 1, channel List N
			 */
			data->apChannels = &subelems[3];
			data->apChannelsLen = slen - 1;

			strbuf = kalMemAlloc(slen * 4, VIR_MEM_TYPE);
			if (strbuf) {
				pos = strbuf;
				end = pos + slen * 4;
				for (i = 0; i < data->apChannelsLen; i++) {
					pos += kalSnprintf(pos, end - pos,
						" %d", data->apChannels[i]);
				}
				*pos = '\0';
				DBGLOG(RRM, INFO, "AP chnls %s", strbuf);
				kalMemFree(strbuf, VIR_MEM_TYPE, slen * 4);
			}
			break;
		}
		case BEACON_REQUEST_SUBELEM_LAST_INDICATION:
		{
			if (slen != 1) {
				DBGLOG(RRM, WARN, "Wrong len %u", slen);
				break;
			}

			data->lastIndication = subelems[2];
			break;
		}
		default:
			DBGLOG(RRM, WARN, "Unknown subelem id %u", subelems[0]);
			break;
		}
		elemsLen -= 2 + slen;
		subelems += 2 + slen;
	}
}

uint8_t rrmCheckReportId(uint8_t id, uint8_t *ie, uint8_t len)
{
	uint8_t i;

	if (ie && len > 0) {
		for (i = 0; i < len; i++) {
			if (id == ie[i])
				return TRUE;
		}
	}
	return FALSE;
}

int rrmBeaconRepAddFrameBody(struct BCN_RM_PARAMS *data,
				  struct BSS_DESC *bss, uint8_t *buf,
				  uint32_t buf_len, uint8_t **ies_buf,
				  uint32_t *ie_len, int add_fixed)
{
	uint8_t *ies = *ies_buf;
	uint32_t ies_len = *ie_len;
	uint32_t old_ies_len = ies_len;
	uint8_t *pos = buf;
	int rem_len;
	enum BEACON_REPORT_DETAIL detail = data->reportDetail;

	rem_len = 255 - sizeof(struct RM_BCN_REPORT) -
		sizeof(struct IE_MEASUREMENT_REPORT) - 2 -
		REPORTED_FRAME_BODY_SUBELEM_LEN;

	if (detail > BEACON_REPORT_DETAIL_ALL_FIELDS_AND_ELEMENTS) {
		DBGLOG(RRM, WARN, "Invalid reporting detail: %d", detail);
		return -EINVAL;
	}

	if (detail == BEACON_REPORT_DETAIL_NONE)
		return 0;

	/*
	 * Minimal frame body subelement size: EID(1) + length(1) + TSF(8) +
	 * beacon interval(2) + capabilities(2) = 14 bytes
	 */
	if (add_fixed && buf_len < 14)
		return -EINVAL;

	*pos++ = BEACON_REPORT_SUBELEM_FRAME_BODY;
	/* The length will be filled later */
	pos++;

	if (add_fixed) {
		WLAN_SET_FIELD_64(pos, bss->u8TimeStamp.QuadPart);
		pos += 8;
		WLAN_SET_FIELD_16(pos, bss->u2BeaconInterval);
		pos += 2;
		WLAN_SET_FIELD_16(pos, bss->u2CapInfo);
		pos += 2;
	}

	rem_len -= pos - buf;

	/*
	 * According to IEEE Std 802.11-2016, 9.4.2.22.7, if the reported frame
	 * body subelement causes the element to exceed the maximum element
	 * size, the subelement is truncated so that the last IE is a complete
	 * IE. So even when required to report all IEs, add elements one after
	 * the other and stop once there is no more room in the measurement
	 * element.
	 */
	while (ies_len > 2 && 2U + ies[1] <= ies_len && rem_len > 0) {
		if (detail == BEACON_REPORT_DETAIL_ALL_FIELDS_AND_ELEMENTS ||
		    rrmCheckReportId(ies[0], data->reportIeIds,
				data->reportIeIdsLen)) {
			uint8_t elen = ies[1];

			if (2 + elen > buf + buf_len - pos ||
			    2 + elen > rem_len)
				break;

			*pos++ = ies[0];
			*pos++ = elen;
			kalMemCopy(pos, ies + 2, elen);
			pos += elen;
			rem_len -= 2 + elen;
		}

		ies_len -= 2 + ies[1];
		ies += 2 + ies[1];
	}

	*ie_len = ies_len;
	*ies_buf = ies;

	/* Now the length is known */
	buf[1] = pos - buf - 2;
	return old_ies_len != ies_len ? pos - buf : -EINVAL;
}

int rrmReportElem(struct RM_MEASURE_REPORT_ENTRY *reportEntry,
	uint8_t token, uint8_t mode, uint8_t type,
	const uint8_t *data, uint32_t data_len)
{
	uint8_t *report = reportEntry->pucMeasReport;
	uint8_t len = reportEntry->u2MeasReportLen;
	uint32_t size = len + 5 + data_len;
	uint8_t *buf;

	buf = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!buf) {
		DBGLOG(RRM, ERROR, "alloc report elem fail\n");
		return -ENOMEM;
	}
	reportEntry->u2MeasReportLen = size;
	reportEntry->pucMeasReport = buf;

	if (len) {
		kalMemCopy(buf, report, len);
		kalMemFree(report, VIR_MEM_TYPE, len);
		buf += len;
	}

	*buf++ = ELEM_ID_MEASUREMENT_REPORT;
	*buf++ = 3 + data_len;
	*buf++ = token;
	*buf++ = mode;
	*buf++ = type;

	if (data_len) {
		kalMemCopy(buf, data, data_len);
		buf += data_len;
	}

	return 0;
}

int rrmAddBeaconRepElem(IN struct ADAPTER *prAdapter,
			struct BCN_RM_PARAMS *data,
			struct BSS_DESC *bss,
			struct RM_MEASURE_REPORT_ENTRY *reportEntry,
			struct RM_BCN_REPORT *rep,
			uint8_t **ie, uint32_t *ie_len, uint8_t idx)
{
	int ret;
	uint8_t *buf, *pos;
	uint8_t ve = prAdapter->rWifiVar.u4SwTestMode ==
		ENUM_SW_TEST_MODE_SIGMA_VOICE_ENT;
	uint32_t subelems_len =
		(!ve ? REPORTED_FRAME_BODY_SUBELEM_LEN : 0) +
		(data->lastIndication ?
		 BEACON_REPORT_LAST_INDICATION_SUBELEM_LEN : 0);
	uint32_t size = sizeof(*rep) + 14 + *ie_len + subelems_len;

	/* Maximum element length: Beacon Report element + Reported Frame Body
	 * subelement + all IEs of the reported Beacon frame + Reported Frame
	 * Body Fragment ID subelement
	 */
	buf = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!buf) {
		DBGLOG(RRM, ERROR, "alloc beacon report elem fail\n");
		return -ENOMEM;
	}

	kalMemCopy(buf, rep, sizeof(*rep));

	ret = rrmBeaconRepAddFrameBody(data,
				       bss, buf + sizeof(*rep),
				       14 + *ie_len, ie, ie_len,
				       idx == 0);
	if (ret < 0)
		goto out;

	pos = buf + ret + sizeof(*rep);


	if (!ve) {
		pos[0] = BEACON_REPORT_SUBELEM_FRAME_BODY_FRAGMENT_ID;
		pos[1] = 2;

		/*
		 * Only one Beacon Report Measurement is supported at a time,
		 * so the Beacon Report ID can always be set to 1.
		 */
		pos[2] = 1;

		/* Fragment ID Number (bits 0..6) and
		 * More Frame Body Fragments (bit 7)
		 */
		pos[3] = idx;
		if (data->reportDetail != BEACON_REPORT_DETAIL_NONE && *ie_len)
			pos[3] |= REPORTED_FRAME_BODY_MORE_FRAGMENTS;
		else
			pos[3] &= ~REPORTED_FRAME_BODY_MORE_FRAGMENTS;

		pos += REPORTED_FRAME_BODY_SUBELEM_LEN;
	}

	if (data->lastIndication) {
		pos[0] = BEACON_REPORT_SUBELEM_LAST_INDICATION;
		pos[1] = 1;

		/* This field will be updated later if this is the last frame */
		pos[2] = 0;
	}

	ret = rrmReportElem(reportEntry, data->token,
			   MEASUREMENT_REPORT_MODE_ACCEPT,
			   ELEM_RM_TYPE_BEACON_REPORT, buf,
			   ret + sizeof(*rep) + subelems_len);
out:
	kalMemFree(buf, VIR_MEM_TYPE, size);
	return ret;
}

void rrmCollectBeaconReport(IN struct ADAPTER *prAdapter,
	IN struct BSS_DESC *prBssDesc, IN uint8_t ucBssIndex)
{
	struct RADIO_MEASUREMENT_REQ_PARAMS *rmReq =
		aisGetRmReqParam(prAdapter, ucBssIndex);
	struct RADIO_MEASUREMENT_REPORT_PARAMS *rmRep =
		aisGetRmReportParam(prAdapter, ucBssIndex);
	struct RM_BCN_REQ *bcnReq =
	     (struct RM_BCN_REQ *)&rmReq->prCurrMeasElem->aucRequestFields[0];
	struct BCN_RM_PARAMS *data = &rmReq->rBcnRmParam;
	uint8_t *bssid = prBssDesc->aucBSSID;
	uint8_t *pos = prBssDesc->aucIEBuf;
	uint32_t ies_len = prBssDesc->u2IELength;
	struct RM_BCN_REPORT rep;
	struct RM_MEASURE_REPORT_ENTRY *reportEntry = NULL;
	struct RM_MEASURE_REPORT_ENTRY *tmp = NULL;
	u_int8_t idx = 0;
	u_int8_t validChannel = FALSE;
	OS_SYSTIME rCurrent;
	uint64_t u8Tsf = 0;

	/* sanity check 1: bssid */
	if (!EQUAL_MAC_ADDR(bcnReq->aucBssid, "\xff\xff\xff\xff\xff\xff") &&
		!EQUAL_MAC_ADDR(bcnReq->aucBssid, bssid)) {
		DBGLOG(RRM, INFO,
		       "bssid mismatch, req "MACSTR", actual "MACSTR"\n",
		       MAC2STR(bcnReq->aucBssid), MAC2STR(bssid));
		return;
	}

	/* sanity check 2: channel */
	if (prBssDesc->ucChannelNum == bcnReq->ucChannel)
		validChannel = TRUE;
	if (!validChannel) {
		uint8_t i = 0;

		for (i = 0; i < data->apChannelsLen; i++) {
			if (prBssDesc->ucChannelNum == data->apChannels[i]) {
				validChannel = TRUE;
				break;
			}
		}
	}
	if (!validChannel &&
	    bcnReq->ucChannel > 0 && bcnReq->ucChannel < 255) {
		DBGLOG(RRM, INFO, ""MACSTR" chnl %d invalid, req %d\n",
			MAC2STR(bssid), prBssDesc->ucChannelNum,
			bcnReq->ucChannel);
		return;
	}

	/* sanity check 3: ssid */
	if (data->ssidLen &&
		UNEQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			     data->ssid, data->ssidLen)) {
		uint8_t reqSsid[ELEM_MAX_LEN_SSID + 1] = {0};
		uint8_t bcnSsid[ELEM_MAX_LEN_SSID + 1] = {0};

		kalMemCopy(reqSsid, data->ssid,
			min_t(uint8_t, data->ssidLen, ELEM_MAX_LEN_SSID));
		kalMemCopy(bcnSsid, prBssDesc->aucSSID,
		       min_t(uint8_t, prBssDesc->ucSSIDLen, ELEM_MAX_LEN_SSID));
		DBGLOG(RRM, TRACE,
		       ""MACSTR" SSID mismatch, req(%d, %s), bcn(%d, %s)\n",
		       MAC2STR(bssid), data->ssidLen, HIDE(reqSsid),
		       prBssDesc->ucSSIDLen, HIDE(bcnSsid));
		return;
	}

	/* Compose Beacon Report in a temp buffer, search in saved
	 * reported link, check if we have saved a report for this AP
	 */
	LINK_FOR_EACH_ENTRY(tmp, &rmRep->rReportLink, rLinkEntry,
			    struct RM_MEASURE_REPORT_ENTRY)
	{
		if (EQUAL_MAC_ADDR(tmp->aucBSSID, bssid)) {
			reportEntry = tmp;
			break;
		}
	}
	/* not found a entry in collected report link */
	if (!reportEntry) {
		reportEntry = kalMemAlloc(sizeof(*reportEntry),
					    VIR_MEM_TYPE);
		if (!reportEntry)/* no memory to allocate in OS */ {
			DBGLOG(RRM, ERROR,
			       "Alloc entry failed, No Memory\n");
			return;
		}
		COPY_MAC_ADDR(reportEntry->aucBSSID, bssid);
		reportEntry->u2MeasReportLen = 0;
		reportEntry->pucMeasReport = NULL;
		DBGLOG(RRM, TRACE,
		       "allocate entry for Bss "MACSTR", total entry %u\n",
			MAC2STR(bssid), rmRep->rReportLink.u4NumElem);
		LINK_INSERT_TAIL(&rmRep->rReportLink,
				 &reportEntry->rLinkEntry);
	} else {
		if (reportEntry->u2MeasReportLen) {
			kalMemFree(reportEntry->pucMeasReport,
				VIR_MEM_TYPE, reportEntry->u2MeasReportLen);
			reportEntry->u2MeasReportLen = 0;
			reportEntry->pucMeasReport = NULL;
		}
	}

	/* Fixed length field */
	rep.ucRegulatoryClass = bcnReq->ucRegulatoryClass;
	rep.ucChannel = prBssDesc->ucChannelNum;
	rep.u2Duration = bcnReq->u2Duration;
	/* ucReportInfo: Bit 0 is the type of frame, 0 means beacon/probe
	 ** response, bit 1~7 means phy type
	 */
	rep.ucReportInfo = 0;
	rep.ucRCPI = prBssDesc->ucRCPI;
	rep.ucRSNI = 255; /* 255 = RSNI not available. see 7.3.2.41 */
	COPY_MAC_ADDR(rep.aucBSSID, bssid);
	rep.ucAntennaID = 1;
	u8Tsf = *(uint64_t *)&rTsf.au4Tsf[0];
	GET_CURRENT_SYSTIME(&rCurrent);
	if (rmReq->rStartTime >= rTsf.rTime)
		u8Tsf += rmReq->rStartTime - rTsf.rTime;
	else
		u8Tsf += rTsf.rTime - rmReq->rStartTime;
	kalMemCopy(rep.aucStartTime, &u8Tsf, 8);
	u8Tsf = *(uint64_t *)&rTsf.au4Tsf[0] + rCurrent - rTsf.rTime;
	kalMemCopy(rep.aucParentTSF, &u8Tsf, 4); /* low part of TSF */

	do {
		int ret;

		ret = rrmAddBeaconRepElem(prAdapter, data, prBssDesc,
			reportEntry, &rep, &pos, &ies_len, idx++);
		if (ret)
			break;
	} while (data->reportDetail != BEACON_REPORT_DETAIL_NONE &&
		 ies_len >= 2);

	DBGLOG(RRM, TRACE,
	       "Bss "MACSTR", ReportDeail %d, IncludeIE Num %d, chnl %d\n",
	       MAC2STR(bssid), data->reportDetail, data->reportIeIdsLen,
	       prBssDesc->ucChannelNum);
}

void rrmUpdateBssTimeTsf(struct ADAPTER *prAdapter, struct BSS_DESC *prBssDesc)
{
	ASSERT(prAdapter);
	ASSERT(prBssDesc);

	rTsf.rTime = prBssDesc->rUpdateTime;
	kalMemCopy(&rTsf.au4Tsf[0], &prBssDesc->u8TimeStamp, 8);
}
