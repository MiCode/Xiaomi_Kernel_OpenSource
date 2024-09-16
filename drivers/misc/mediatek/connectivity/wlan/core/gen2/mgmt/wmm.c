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

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
UINT_8 const aucUp2ACIMap[8] = {ACI_BE, ACI_BK, ACI_BK, ACI_BE, ACI_VI, ACI_VI, ACI_VO, ACI_VO};

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

static VOID
wmmTxTspecFrame(P_ADAPTER_T prAdapter, UINT_8 ucTid, enum TSPEC_OP_CODE eOpCode,
						  P_PARAM_QOS_TSPEC prTsParam);
static VOID wmmSyncAcParamWithFw(
	P_ADAPTER_T prAdapter, UINT_8 ucAc, UINT_16 u2MediumTime, UINT_32 u4PhyRate);

static void wmmGetTsmRptTimeout(P_ADAPTER_T prAdapter, ULONG ulParam);
static VOID wmmQueryTsmResult(P_ADAPTER_T prAdapter, ULONG ulParam);
static VOID
wmmRemoveTSM(P_ADAPTER_T prAdapter, struct ACTIVE_RM_TSM_REQ *prActiveTsm, BOOLEAN fgNeedStop);
static struct ACTIVE_RM_TSM_REQ *
	wmmGetActiveTsmReq(P_ADAPTER_T prAdapter, UINT_8 ucTid,
								   BOOLEAN fgTriggered, BOOLEAN fgAllocIfNotExist);
static WLAN_STATUS
wmmRunEventActionTxDone(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo,
								      ENUM_TX_RESULT_CODE_T rTxDoneStatus);

static VOID wmmMayDoTsReplacement(P_ADAPTER_T prAdapter, UINT_8 ucNewTid);
#if 0
static void DumpData(PUINT8 prAddr, UINT8 uLen, char *tag);
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

VOID wmmInit(IN P_ADAPTER_T prAdapter)
{
	struct WMM_INFO *prWmmInfo = &prAdapter->rWifiVar.rWmmInfo;
	struct TSPEC_INFO *prTspecInfo = &prWmmInfo->arTsInfo[0];
	UINT_8 ucTid = 0;

	for (ucTid = 0; ucTid < WMM_TSPEC_ID_NUM; ucTid++, prTspecInfo++)
		cnmTimerInitTimer(prAdapter, &prTspecInfo->rAddTsTimer,
						  (PFN_MGMT_TIMEOUT_FUNC)wmmSetupTspecTimeOut, (ULONG)ucTid);

	LINK_INITIALIZE(&prWmmInfo->rActiveTsmReq);
	prWmmInfo->rTriggeredTsmRptTime = 0;
	DBGLOG(WMM, TRACE, "wmm init done\n");
}

VOID wmmUnInit(IN P_ADAPTER_T prAdapter)
{
	struct WMM_INFO *prWmmInfo = &prAdapter->rWifiVar.rWmmInfo;
	struct TSPEC_INFO *prTspecInfo = &prWmmInfo->arTsInfo[0];
	UINT_8 ucTid = 0;

	for (ucTid = 0; ucTid < WMM_TSPEC_ID_NUM; ucTid++, prTspecInfo++)
		cnmTimerStopTimer(prAdapter, &prTspecInfo->rAddTsTimer);
	wmmRemoveAllTsmMeasurement(prAdapter, FALSE);
	DBGLOG(WMM, TRACE, "wmm uninit done\n");
}

VOID
wmmFillTsinfo(P_PARAM_QOS_TSINFO prTsInfo, PUINT_8 pucTsInfo)
{
	UINT_32 u4TsInfoValue = 0;
	/*	|    0         |1-4  | 5-6 |	7-8          | 9           | 10  | 11-13 | 14-23  |
	*	Traffic Type|TSID| Dir  |Access Policy|Reserved | PSB|	UP   |reserved|
	*/

	u4TsInfoValue = prTsInfo->ucTrafficType & 0x1;
	u4TsInfoValue |= (prTsInfo->ucTid & 0xf) << 1;
	u4TsInfoValue |= (prTsInfo->ucDirection & 0x3) << 5;
	u4TsInfoValue |= (prTsInfo->ucAccessPolicy & 0x3) << 7;
	u4TsInfoValue |= (prTsInfo->ucApsd & 0x1) << 10;
	u4TsInfoValue |= (prTsInfo->ucuserPriority) << 11;
	u4TsInfoValue |= BIT(7); /* Fixed bit in spec */

	pucTsInfo[0] = u4TsInfoValue & 0xFF;
	pucTsInfo[1] = (u4TsInfoValue >> 8) & 0xff;
	pucTsInfo[2] = (u4TsInfoValue >> 16) & 0xff;
}

VOID
wmmComposeTspecIE(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo,
							   P_PARAM_QOS_TSPEC prParamQosTspec)
{
	P_IE_WMM_TSPEC_T prIeWmmTspec = NULL;
	UINT_8 *pucTemp = NULL;
	UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

	prIeWmmTspec = (P_IE_WMM_TSPEC_T)((PUINT8)prMsduInfo->prPacket + prMsduInfo->u2FrameLength);
	pucTemp = prIeWmmTspec->aucTspecBodyPart;

	/*fill WMM head*/
	prIeWmmTspec->ucId = ELEM_ID_VENDOR;
	prIeWmmTspec->ucLength = ELEM_MAX_LEN_WMM_TSPEC;
	kalMemCopy(prIeWmmTspec->aucOui, aucWfaOui, sizeof(aucWfaOui));
	prIeWmmTspec->ucOuiType = VENDOR_OUI_TYPE_WMM;
	prIeWmmTspec->ucOuiSubtype = VENDOR_OUI_SUBTYPE_WMM_TSPEC;
	prIeWmmTspec->ucVersion = VERSION_WMM;

	/*fill tsinfo*/
	wmmFillTsinfo(&prParamQosTspec->rTsInfo, prIeWmmTspec->aucTsInfo);
	/*1.2 BODY*/
	/*nominal size*/
	/*DumpData(prParamQosTspec, sizeof(struct _PARAM_QOS_TSPEC), "QosTspc");*/
	WLAN_SET_FIELD_16(pucTemp, prParamQosTspec->u2NominalMSDUSize);
	pucTemp += 2;
	WLAN_SET_FIELD_16(pucTemp, prParamQosTspec->u2MaxMSDUsize);
	pucTemp += 2;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4MinSvcIntv);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4MaxSvcIntv);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4InactIntv);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4SpsIntv);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4SvcStartTime);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4MinDataRate);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4MeanDataRate);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4PeakDataRate);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4MaxBurstSize);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4DelayBound);
	pucTemp += 4;
	WLAN_SET_FIELD_32(pucTemp, prParamQosTspec->u4MinPHYRate);
	pucTemp += 4;
	WLAN_SET_FIELD_16(pucTemp, prParamQosTspec->u2Sba);
	pucTemp += 2;
	WLAN_SET_FIELD_16(pucTemp, prParamQosTspec->u2MediumTime);
	/*DumpData(prIeWmmTspec->aucTsInfo, 55, "tspec ie");*/

	prMsduInfo->u2FrameLength += IE_SIZE(prIeWmmTspec);
}

static UINT_8 wmmNewDlgToken(VOID)
{
	static UINT_8 sWmmDlgToken;

	return sWmmDlgToken++;
}

/* follow WMM spec, send add/del tspec request frame */
static VOID
wmmTxTspecFrame(P_ADAPTER_T prAdapter, UINT_8 ucTid, enum TSPEC_OP_CODE eOpCode,
						  P_PARAM_QOS_TSPEC prTsParam)
{
	P_BSS_INFO_T prBssInfo = NULL;
	UINT_16 u2PayLoadLen = WLAN_MAC_HEADER_LEN + 4; /*exclude TSPEC IE*/
	P_STA_RECORD_T prStaRec = prAdapter->rWifiVar.rAisFsmInfo.prTargetStaRec;
	P_MSDU_INFO_T prMsduInfo = NULL;
	struct WMM_ACTION_TSPEC_FRAME *prActionFrame = NULL;
	UINT_16 u2FrameCtrl = MAC_FRAME_ACTION;

	if (!prStaRec || !prTsParam) {
		DBGLOG(WMM, ERROR, "prStaRec NULL %d, prTsParam NULL %d\n", !prStaRec, !prTsParam);
		return;
	}

	 /*build ADDTS for TID*/
	 /*1 compose Action frame Fix field*/
	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	DBGLOG(WMM, INFO, "Tspec Action to AP="MACSTR"\n", MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = cnmMgtPktAlloc(prAdapter, ACTION_ADDTS_REQ_FRAME_LEN);

	if (prMsduInfo == NULL) {
		DBGLOG(WMM, ERROR, "prMsduInfo is null!");
		return;
	}
	prMsduInfo->eSrc = TX_PACKET_MGMT;
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;
	prMsduInfo->ucStaRecIndex = prStaRec->ucIndex;
	prMsduInfo->ucNetworkType = prStaRec->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = u2PayLoadLen;
	prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfo->pfTxDoneHandler = wmmRunEventActionTxDone;
	prMsduInfo->fgIsBasicRate = TRUE;
	kalMemZero(prMsduInfo->prPacket, ACTION_ADDTS_REQ_FRAME_LEN);

	prActionFrame = (struct WMM_ACTION_TSPEC_FRAME *)prMsduInfo->prPacket;

	/*********frame header**********************/
	WLAN_SET_FIELD_16(&prActionFrame->u2FrameCtrl, u2FrameCtrl);
	COPY_MAC_ADDR(prActionFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prActionFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prActionFrame->aucBSSID, prStaRec->aucMacAddr);
	prActionFrame->u2SeqCtrl = 0;

	/********Frame body*************/
	prActionFrame->ucCategory = CATEGORY_WME_MGT_NOTIFICATION; /*CATEGORY_QOS_ACTION;*/
	if (eOpCode == TX_ADDTS_REQ) {
		prActionFrame->ucAction = ACTION_ADDTS_REQ;
		prActionFrame->ucDlgToken =
			(prTsParam->ucDialogToken == 0) ? wmmNewDlgToken():prTsParam->ucDialogToken;
	} else if (eOpCode == TX_DELTS_REQ) {
		prActionFrame->ucAction = ACTION_DELTS;
		prActionFrame->ucDlgToken = 0; /* dialog token should be always 0 in delts frame */
	}

	prActionFrame->ucStatusCode = 0; /* this field only meanful in ADD TS response, otherwise set to 0 */

	/*DumpData((P_UINT_8)prMsduInfo->prPacket,u2PayLoadLen, "ADDTS-FF");*/

	/********Information Element *************/
	wmmComposeTspecIE(prAdapter, prMsduInfo, prTsParam);

	/******** Insert into Msdu Queue *************/
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	/*DumpData(((PUINT8)prMsduInfo->prPacket) + u2PayLoadLen,
	*	prMsduInfo->u2FrameLength - u2PayLoadLen, "TSPEC-IE");
	*/
}

VOID wmmSetupTspecTimeOut(P_ADAPTER_T prAdapter, ULONG ulParam)
{
	struct TSPEC_INFO *prTsInfo = NULL;
	UINT_8 ucTimeoutTid = (UINT_8)ulParam;

	if (ulParam >= WMM_TSPEC_ID_NUM) {
		DBGLOG(WMM, INFO, "Wrong TS ID %d\n", ucTimeoutTid);
		return;
	}
	prTsInfo = &prAdapter->rWifiVar.rWmmInfo.arTsInfo[ucTimeoutTid];
	switch (prTsInfo->eState) {
	case QOS_TS_ACTIVE:
		DBGLOG(WMM, INFO, "Update TS TIMEOUT for TID %d\n", ucTimeoutTid);
		break;
	case QOS_TS_SETUPING:
		DBGLOG(WMM, INFO, "ADD TS TIMEOUT for TID %d\n", ucTimeoutTid);
		prTsInfo->eState = QOS_TS_INACTIVE;
		break;
	default:
		DBGLOG(WMM, INFO, "Shouldn't start this timer when Ts %d in state %d\n",
			   ucTimeoutTid, prTsInfo->eState);
		break;
	}
}
#if 0
VOID
wmmSyncTXPwrLimitWithFw(
	IN P_ADAPTER_T prAdapter,
	IN BOOLEAN enable,
	IN INT_8 cMaxPwr
	)
{
	CMD_MAX_TXPWR_LIMIT_T rCmdMaxTxPwrLimit;
	UINT_8 ucLevel = 100; /*default 100 percent */
	/*  <5mw	5mw     10mw	20mw	30mw	40mw	50mw
	*	<7dbm	7dbm	10dbm	13dbm	15dbm	16dbm	17dbm
	*	5%		10%		20%		40%		60%		80%		100%
	*/
	if (cMaxPwr <= 2) {
		ucLevel = 0;
		cMaxPwr = 2; /* FW can accept the lowest power value: 2dbm */
	} else if (cMaxPwr < 7)
		ucLevel = 5;
	else if (cMaxPwr < 10)
		ucLevel = 10;
	else if (cMaxPwr < 13)
		ucLevel = 20;
	else if (cMaxPwr < 15)
		ucLevel = 40;
	else if (cMaxPwr == 15)
		ucLevel = 60;
	else if (cMaxPwr == 16)
		ucLevel = 80;
	else if (cMaxPwr >= 17) {
		cMaxPwr = 17;
		ucLevel = 100;
	}
	/* base on test plan, only the tx power limit is less than the one user set, we need use tpc limit */
	if (prAdapter->rWifiVar.rCCXInfo.ucTxPwrLevel < ucLevel)
		return;
	kalIndicateCcxNotify(prAdapter, CCX_EVENT_TX_PWR, &ucLevel, 1);
	prAdapter->rWifiVar.rCCXInfo.ucTxPwrLevel = ucLevel;
	rCmdMaxTxPwrLimit.ucMaxTxPwrLimitEnable = enable;
	rCmdMaxTxPwrLimit.ucMaxTxPwr = (UINT_8)cMaxPwr * 2; /* in fw, he will devide this value */
	/* CMD_ID_SET_MAX_TXPWR_LIMIT: if tx power of some modulation which from nvram
	*	is less than this limit, he will use the value from nvram. otherwise, he will use this limit value
	*/
	wlanSendSetQueryCmd(prAdapter,
		CMD_ID_SET_MAX_TXPWR_LIMIT,
		TRUE,
		FALSE,
		FALSE,
		NULL,
		NULL,
		sizeof(CMD_MAX_TXPWR_LIMIT_T),
		(PUINT8)&rCmdMaxTxPwrLimit,
		NULL,
		0);
}
#endif

UINT_8 wmmCalculateUapsdSetting(P_ADAPTER_T prAdapter)
{
	P_PM_PROFILE_SETUP_INFO_T prPmProf =
			&prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].rPmProfSetupInfo;
	struct TSPEC_INFO *prCurTs = &prAdapter->rWifiVar.rWmmInfo.arTsInfo[0];
	UINT_8 ucTid = 0;
	UINT_8 ucFinalSetting = 0;

	ucFinalSetting = (prPmProf->ucBmpDeliveryAC << 4) | prPmProf->ucBmpTriggerAC;
	for (ucTid = 0; ucTid < WMM_TSPEC_ID_NUM; ucTid++, prCurTs++) {
		UINT_8 ucPsd = 0;

		if (prCurTs->eState != QOS_TS_ACTIVE)
			continue;
		switch (prCurTs->eDir) {
		case UPLINK_TS:
			ucPsd = BIT(prCurTs->eAC);
			break;
		case DOWNLINK_TS:
			ucPsd = BIT(prCurTs->eAC + 4);
			break;
		case BI_DIR_TS:
			ucPsd =  BIT(prCurTs->eAC) | BIT(prCurTs->eAC + 4);
			break;
		}
		if (prCurTs->fgUapsd)
			ucFinalSetting |= ucPsd;
		else
			ucFinalSetting &= ~ucPsd;
	}
	return ucFinalSetting;
}

VOID wmmSyncAcParamWithFw(
	P_ADAPTER_T prAdapter, UINT_8 ucAc, UINT_16 u2MediumTime, UINT_32 u4PhyRate)
{
	CMD_UPDATE_AC_PARAMS_T rCmdUpdateAcParam;
	CMD_SET_WMM_PS_TEST_STRUCT_T rSetWmmPsTestParam;

	kalMemZero(&rCmdUpdateAcParam, sizeof(rCmdUpdateAcParam));
	rCmdUpdateAcParam.ucAcIndex = ucAc;
	rCmdUpdateAcParam.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
	rCmdUpdateAcParam.u2MediumTime = u2MediumTime;
	rCmdUpdateAcParam.u4PhyRate = u4PhyRate;
	wlanSendSetQueryCmd(prAdapter,
		CMD_ID_UPDATE_AC_PARMS,
		TRUE,
		FALSE,
		FALSE,
		NULL,
		NULL,
		sizeof(CMD_UPDATE_AC_PARAMS_T),
		(PUINT_8)&rCmdUpdateAcParam,
		NULL,
		0);

	kalMemZero(&rSetWmmPsTestParam, sizeof(rSetWmmPsTestParam));
	rSetWmmPsTestParam.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
	rSetWmmPsTestParam.bmfgApsdEnAc = wmmCalculateUapsdSetting(prAdapter);
	wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_WMM_PS_TEST_PARMS,
			TRUE, FALSE, FALSE, NULL,
			NULL, sizeof(CMD_SET_WMM_PS_TEST_STRUCT_T),
			(PUINT_8)&rSetWmmPsTestParam, NULL, 0);

	DBGLOG(WMM, INFO, "Ac=%d, MediumTime=%d PhyRate=%u Uapsd 0x%02x\n",
		   ucAc, u2MediumTime, u4PhyRate, rSetWmmPsTestParam.bmfgApsdEnAc);
}

/* Return: AC List in bit map if this ac has active tspec */
UINT_8 wmmHasActiveTspec(struct WMM_INFO *prWmmInfo)
{
	UINT_8 ucTid = 0;
	UINT_8 ucACList = 0;

	/* if any tspec is active, it means */
	for (; ucTid < WMM_TSPEC_ID_NUM; ucTid++)
		if (prWmmInfo->arTsInfo[ucTid].eState == QOS_TS_ACTIVE)
			ucACList |= 1<<prWmmInfo->arTsInfo[ucTid].eAC;
	return ucACList;
}

VOID
wmmRunEventTSOperate(
	IN P_ADAPTER_T prAdapter,
	IN P_MSG_HDR_T prMsgHdr
	)
{
	struct MSG_TS_OPERATE *prMsgTsOperate = (struct MSG_TS_OPERATE *)prMsgHdr;

	if (prMsgTsOperate == NULL) {
		DBGLOG(WMM, INFO, "prMsgTsOperate = NULL, do nothing.\n");
		return;
	}

	wmmTspecSteps(prAdapter, prMsgTsOperate->ucTid,
				  prMsgTsOperate->eOpCode, (VOID *)&prMsgTsOperate->rTspecParam);

	cnmMemFree(prAdapter, prMsgHdr);
}

VOID
wmmTspecSteps(P_ADAPTER_T prAdapter, UINT_8 ucTid, enum TSPEC_OP_CODE eOpCode,
					  VOID *prStepParams)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;
	struct WMM_INFO *prWmmInfo = &prAdapter->rWifiVar.rWmmInfo;
	struct TSPEC_INFO *prCurTs = NULL;

	if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].eConnectionState !=
		PARAM_MEDIA_STATE_CONNECTED ||
		prAisFsmInfo->eCurrentState == AIS_STATE_DISCONNECTING) {
		DBGLOG(WMM, INFO, "ignore OP code %d when medium disconnected\n", eOpCode);
		return;
	}

	if (ucTid >= WMM_TSPEC_ID_NUM) {
		DBGLOG(WMM, INFO, "Invalid TID %d\n", ucTid);
		return;
	}

	prCurTs = &prWmmInfo->arTsInfo[ucTid];
	DBGLOG(WMM, TRACE, "TID %d, State %d, Oper %d\n", ucTid, prCurTs->eState, eOpCode);

	switch (prCurTs->eState) {
	case QOS_TS_INACTIVE:
	{
		P_PARAM_QOS_TSPEC prQosTspec = (P_PARAM_QOS_TSPEC)prStepParams;

		if (eOpCode != TX_ADDTS_REQ)
			break;

		if (!prQosTspec) {
			DBGLOG(WMM, INFO, "Lack of Tspec Param\n");
			break;
		}
		/*Send ADDTS req Frame*/
		wmmTxTspecFrame(prAdapter, ucTid, TX_ADDTS_REQ, prQosTspec);

		/*start ADDTS timer*/
		cnmTimerStartTimer(prAdapter, &prCurTs->rAddTsTimer, 1000);
		prCurTs->eState = QOS_TS_SETUPING;
		prCurTs->eAC = aucUp2ACIMap[prQosTspec->rTsInfo.ucuserPriority];
		prCurTs->ucToken = prQosTspec->ucDialogToken;
		break;
	}
	case QOS_TS_SETUPING:
	{
		struct WMM_ADDTS_RSP_STEP_PARAM *prParam =
			(struct WMM_ADDTS_RSP_STEP_PARAM *)prStepParams;
		if (eOpCode == TX_DELTS_REQ || eOpCode == RX_DELTS_REQ || eOpCode == DISC_DELTS_REQ) {
			cnmTimerStopTimer(prAdapter, &prCurTs->rAddTsTimer);
			prCurTs->eState = QOS_TS_INACTIVE;
			DBGLOG(WMM, INFO, "Del Ts %d in setuping state\n", ucTid);
			break;
		} else if (eOpCode != RX_ADDTS_RSP || prParam->ucDlgToken != prWmmInfo->arTsInfo[ucTid].ucToken)
			break;

		cnmTimerStopTimer(prAdapter, &prCurTs->rAddTsTimer);
		if (prParam->ucStatusCode == WMM_TS_STATUS_ADMISSION_ACCEPTED) {
			struct ACTIVE_RM_TSM_REQ *prActiveTsmReq = NULL;

			prCurTs->eState = QOS_TS_ACTIVE;
			prCurTs->eDir = prParam->eDir;
			prCurTs->fgUapsd = !!prParam->ucApsd;
			prCurTs->u2MediumTime = prParam->u2MediumTime;
			prCurTs->u4PhyRate = prParam->u4PhyRate;
			wmmSyncAcParamWithFw(prAdapter, prCurTs->eAC, prParam->u2MediumTime,
								 prParam->u4PhyRate);
			wmmMayDoTsReplacement(prAdapter, ucTid);
			/* start pending TSM if it was requested before admitted */
			prActiveTsmReq = wmmGetActiveTsmReq(prAdapter, ucTid, TRUE, FALSE);
			if (prActiveTsmReq)
				wmmStartTsmMeasurement(prAdapter, (ULONG)prActiveTsmReq->prTsmReq);
			prActiveTsmReq = wmmGetActiveTsmReq(prAdapter, ucTid, FALSE, FALSE);
			if (prActiveTsmReq)
				wmmStartTsmMeasurement(prAdapter, (ULONG)prActiveTsmReq->prTsmReq);
		} else {
			prCurTs->eState = QOS_TS_INACTIVE;
			DBGLOG(WMM, ERROR, "ADD TS is rejected, status=%d\n", prParam->ucStatusCode);
		}
		break;
	}
	case QOS_TS_ACTIVE:
	{
		struct ACTIVE_RM_TSM_REQ *prActiveTsm = NULL;

		switch (eOpCode) {
		case TX_DELTS_REQ:
		case RX_DELTS_REQ:
		case DISC_DELTS_REQ:
			prActiveTsm = wmmGetActiveTsmReq(prAdapter, ucTid, TRUE, FALSE);
			if (prActiveTsm)
				wmmRemoveTSM(prAdapter, prActiveTsm, TRUE);
			prActiveTsm = wmmGetActiveTsmReq(prAdapter, ucTid, FALSE, FALSE);
			if (prActiveTsm)
				wmmRemoveTSM(prAdapter, prActiveTsm, TRUE);
			prCurTs->eState = QOS_TS_INACTIVE;
#if 0 /* No need to change station tx queue, because test plan requires stop this UP 1s after delts was transmitted */
			qmHandleDelTspec(prAdapter, NETWORK_TYPE_AIS_INDEX, prAisFsmInfo->prTargetStaRec, prCurTs->eAC);
#endif
			wmmSyncAcParamWithFw(prAdapter, prCurTs->eAC, 0, 0);
			wmmDumpActiveTspecs(prAdapter, NULL, 0);
			if (eOpCode == TX_DELTS_REQ)
				wmmTxTspecFrame(prAdapter, ucTid, TX_DELTS_REQ, (P_PARAM_QOS_TSPEC)prStepParams);
			break;
		case TX_ADDTS_REQ:
			/*Send ADDTS req Frame*/
			wmmTxTspecFrame(prAdapter, ucTid, TX_ADDTS_REQ, (P_PARAM_QOS_TSPEC)prStepParams);
			prCurTs->eAC = aucUp2ACIMap[((P_PARAM_QOS_TSPEC)prStepParams)->rTsInfo.ucuserPriority];
			prCurTs->ucToken = ((P_PARAM_QOS_TSPEC)prStepParams)->ucDialogToken;
			/*start ADDTS timer*/
			cnmTimerStartTimer(prAdapter, &prCurTs->rAddTsTimer, 1000);
			break;
		/* for case: TS of tid N has existed, then setup TS with this tid again. */
		case RX_ADDTS_RSP:
		{
			struct WMM_ADDTS_RSP_STEP_PARAM *prParam = (struct WMM_ADDTS_RSP_STEP_PARAM *)prStepParams;

			if (prParam->ucStatusCode != WMM_TS_STATUS_ADMISSION_ACCEPTED) {
				DBGLOG(WMM, INFO, "Update TS %d request was rejected by BSS\n", ucTid);
				break;
			}
			prCurTs->eDir = prParam->eDir;
			prCurTs->fgUapsd = !!prParam->ucApsd;
			prCurTs->u2MediumTime = prParam->u2MediumTime;
			prCurTs->u4PhyRate = prParam->u4PhyRate;
			wmmSyncAcParamWithFw(prAdapter, prCurTs->eAC, prParam->u2MediumTime, prParam->u4PhyRate);
			wmmMayDoTsReplacement(prAdapter, ucTid);
			break;
		}
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
}

static WLAN_STATUS
wmmRunEventActionTxDone(P_ADAPTER_T prAdapter, P_MSDU_INFO_T prMsduInfo,
									   ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	DBGLOG(WMM, INFO, "Status %d\n", rTxDoneStatus);
	return WLAN_STATUS_SUCCESS;
}

static char const charmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
					'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
void DumpData(PUINT8 prAddr, UINT8 uLen, char *tag)
{
	UINT16 k = 0;
	char buf[16*3+1];
	UINT16 loop = 0;
	PUINT8 p = prAddr;

	uLen = (uLen > 128) ? 128 : uLen;
	loop = uLen / 16;
	if (tag)
		DBGLOG(WMM, INFO, "++++++++ dump data \"%s\" p=%p len=%d\n", tag, prAddr, uLen);
	else
		DBGLOG(WMM, INFO, "++++++ dump data p=%p, len=%d\n", prAddr, uLen);

	while (loop) {
		for (k = 0; k < 16; k++) {
			buf[k*3] = charmap[((*(p + k) & 0xF0) >> 4)];
			buf[k*3 + 1] = charmap[(*(p + k) & 0x0F)];
			buf[k*3 + 2] = ' ';
		}
		buf[16*3] = 0;
		DBGLOG(WMM, INFO, "%s\n", buf);
		loop--;
		p += 16;
	}
	uLen = uLen%16;
	k = 0;
	while (uLen) {
		buf[k*3] =  charmap[((*(p + k) & 0xF0) >> 4)];
		buf[k*3 + 1] = charmap[(*(p + k) & 0x0F)];
		buf[k*3 + 2] = ' ';
		k++;
		uLen--;
	}
	buf[k*3] = 0;
	DBGLOG(WMM, INFO, "%s\n", buf);
	DBGLOG(WMM, INFO, "====== end dump data\n");
}
/* TSM related */

static VOID wmmQueryTsmResult(P_ADAPTER_T prAdapter, ULONG ulParam)
{
	struct RM_TSM_REQ *prTsmReq = ((struct ACTIVE_RM_TSM_REQ *)ulParam)->prTsmReq;
	struct WMM_INFO *prWmmInfo = &prAdapter->rWifiVar.rWmmInfo;
	CMD_GET_TSM_STATISTICS_T rGetTsmStatistics;

	DBGLOG(WMM, INFO, "Query TSM statistics, tid = %d\n", prTsmReq->ucTID);
	DBGLOG(WMM, INFO, "%p , aci %d, duration %d\n", prTsmReq, prTsmReq->ucACI, prTsmReq->u2Duration);
	rGetTsmStatistics.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
	rGetTsmStatistics.ucAcIndex = prTsmReq->ucACI;
	rGetTsmStatistics.ucTid = prTsmReq->ucTID;
	COPY_MAC_ADDR(rGetTsmStatistics.aucPeerAddr, prTsmReq->aucPeerAddr);

	wlanSendSetQueryCmd(prAdapter,
		CMD_ID_GET_TSM_STATISTICS,
		FALSE,
		TRUE,
		FALSE,
		wmmComposeTsmRpt,
		NULL,
		sizeof(CMD_GET_TSM_STATISTICS_T),
		(PUINT_8)&rGetTsmStatistics,
		NULL,
		0);
	cnmTimerInitTimer(prAdapter, &prWmmInfo->rTsmTimer, wmmGetTsmRptTimeout, ulParam);
	cnmTimerStartTimer(prAdapter, &prWmmInfo->rTsmTimer, 2000);
}

static struct ACTIVE_RM_TSM_REQ *
	wmmGetActiveTsmReq(P_ADAPTER_T prAdapter, UINT_8 ucTid,
	BOOLEAN fgTriggered, BOOLEAN fgAllocIfNotExist)
{
	struct WMM_INFO *prWMMInfo = &prAdapter->rWifiVar.rWmmInfo;
	struct ACTIVE_RM_TSM_REQ *prActiveReq = NULL;
	BOOLEAN fgFound = FALSE;

	LINK_FOR_EACH_ENTRY(prActiveReq, &prWMMInfo->rActiveTsmReq, rLinkEntry, struct ACTIVE_RM_TSM_REQ) {
		if ((!!prActiveReq->prTsmReq->u2Duration) == fgTriggered &&
			ucTid == prActiveReq->prTsmReq->ucTID) {
			fgFound = TRUE;
			break;
		}
	}
	if (!fgFound && fgAllocIfNotExist) {
		fgFound = TRUE;
		prActiveReq = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(struct ACTIVE_RM_TSM_REQ));
		LINK_INSERT_TAIL(&prWMMInfo->rActiveTsmReq, &prActiveReq->rLinkEntry);
	}
	return fgFound ? prActiveReq:NULL;
}

static VOID
wmmRemoveTSM(P_ADAPTER_T prAdapter, struct ACTIVE_RM_TSM_REQ *prActiveTsm, BOOLEAN fgNeedStop)
{
	struct WMM_INFO *prWMMInfo = &prAdapter->rWifiVar.rWmmInfo;
	P_LINK_T prActiveTsmLink = &prWMMInfo->rActiveTsmReq;

	LINK_REMOVE_KNOWN_ENTRY(prActiveTsmLink, prActiveTsm);
	if (fgNeedStop) {
		CMD_SET_TSM_STATISTICS_REQUEST_T rTsmStatistics;

		rTsmStatistics.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
		rTsmStatistics.ucEnabled = FALSE;
		rTsmStatistics.ucAcIndex = prActiveTsm->prTsmReq->ucACI;
		rTsmStatistics.ucTid = prActiveTsm->prTsmReq->ucTID;
		COPY_MAC_ADDR(rTsmStatistics.aucPeerAddr, prActiveTsm->prTsmReq->aucPeerAddr);
		wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_TSM_STATISTICS_REQUEST,
			TRUE,
			FALSE,
			FALSE,
			NULL,
			NULL,
			sizeof(CMD_SET_TSM_STATISTICS_REQUEST_T),
			(PUINT_8)&rTsmStatistics,
			NULL,
			0);
	}
	cnmMemFree(prAdapter, prActiveTsm->prTsmReq);
	cnmMemFree(prAdapter, prActiveTsm);
}

void
wmmStartTsmMeasurement(P_ADAPTER_T prAdapter, ULONG ulParam)
{
	struct WMM_INFO *prWMMInfo = &prAdapter->rWifiVar.rWmmInfo;
	CMD_SET_TSM_STATISTICS_REQUEST_T rTsmStatistics;
	struct RM_TSM_REQ *prTsmReq = (struct RM_TSM_REQ *)ulParam;
	UINT_8 ucTid = prTsmReq->ucTID;
	struct ACTIVE_RM_TSM_REQ *prActiveTsmReq = NULL;
	P_STA_RECORD_T prStaRec = prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].prStaRecOfAP;
	struct TSPEC_INFO *prCurTs = NULL;

	if (!prTsmReq->u2Duration && !(prTsmReq->rTriggerCond.ucCondition & TSM_TRIGGER_CONDITION_ALL)) {
		DBGLOG(WMM, WARN, "Duration is %d, Trigger Condition %d\n",
			   prTsmReq->u2Duration, prTsmReq->rTriggerCond.ucCondition);
		cnmMemFree(prAdapter, prTsmReq);
		rlmScheduleNextRm(prAdapter);
		return;
	}
	/* if current TID is not admitted, don't start measurement, only save this requirement */
	if (!prStaRec) {
		DBGLOG(WMM, INFO, "No station record found for %pM\n", prTsmReq->aucPeerAddr);
		cnmMemFree(prAdapter, prTsmReq);
		rlmScheduleNextRm(prAdapter);
		return;
	}
	/* if there's a active tspec, then TID means TS ID */
	prCurTs = &prWMMInfo->arTsInfo[ucTid];
	if (prCurTs->eState == QOS_TS_ACTIVE)
		prTsmReq->ucACI = prCurTs->eAC;
	else { /* otherwise TID means TC ID */
		UINT_8 ucTsAcs = wmmHasActiveTspec(prWMMInfo);

		prTsmReq->ucACI = aucUp2ACIMap[ucTid];
		/* if current TID is not admitted, don't start measurement, only save this requirement */
		if (prStaRec->afgAcmRequired[prTsmReq->ucACI] && !(ucTsAcs & BIT(prTsmReq->ucACI))) {
			DBGLOG(WMM, INFO, "ACM is set for UP %d, but No tspec is setup\n", ucTid);
			rlmScheduleNextRm(prAdapter);
			return;
		}
	}

	kalMemZero(&rTsmStatistics, sizeof(rTsmStatistics));
	if (prTsmReq->u2Duration) {
		/* If a non-AP QoS STa receives a Transmit Stream/Category Measurement Request for a TC, or
		** TS that is already being measured using a triggered transmit stream/category measurement,
		** the triggered traffic stream measurement shall be suspended for the duration of the requested
		** traffic stream measurement. When triggered measurement resumes, the traffic stream metrics
		** shall be reset.  See end part of 802.11k 11.10.8.8
		**/
		LINK_FOR_EACH_ENTRY(prActiveTsmReq, &prWMMInfo->rActiveTsmReq, rLinkEntry, struct ACTIVE_RM_TSM_REQ) {
			if (prActiveTsmReq->prTsmReq->u2Duration || prActiveTsmReq->prTsmReq->ucACI != prTsmReq->ucACI)
				continue;
			rTsmStatistics.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
			rTsmStatistics.ucEnabled = FALSE;
			rTsmStatistics.ucAcIndex = prTsmReq->ucACI;
			rTsmStatistics.ucTid = prActiveTsmReq->prTsmReq->ucTID;
			COPY_MAC_ADDR(rTsmStatistics.aucPeerAddr, prActiveTsmReq->prTsmReq->aucPeerAddr);
			wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_TSM_STATISTICS_REQUEST,
			TRUE,
			FALSE,
			FALSE,
			NULL,
			NULL,
			sizeof(CMD_SET_TSM_STATISTICS_REQUEST_T),
			(PUINT_8)&rTsmStatistics,
			NULL,
			0);
		}
		prActiveTsmReq = wmmGetActiveTsmReq(prAdapter, ucTid, !!prTsmReq->u2Duration, TRUE);
		/* if exist normal tsm on the same ts, replace it */
		if (prActiveTsmReq->prTsmReq)
			cnmMemFree(prAdapter, prActiveTsmReq->prTsmReq);
		DBGLOG(WMM, INFO, "%p tid %d, aci %d, duration %d\n",
			   prTsmReq, prTsmReq->ucTID, prTsmReq->ucACI, prTsmReq->u2Duration);
		cnmTimerInitTimer(prAdapter, &prWMMInfo->rTsmTimer, wmmQueryTsmResult, (ULONG)prActiveTsmReq);
		cnmTimerStartTimer(prAdapter, &prWMMInfo->rTsmTimer, TU_TO_MSEC(prTsmReq->u2Duration));
	} else {
		prActiveTsmReq = wmmGetActiveTsmReq(prAdapter, ucTid, !prTsmReq->u2Duration, TRUE);
		/* if exist triggered tsm on the same ts, replace it */
		if (prActiveTsmReq->prTsmReq)  {
			cnmTimerStopTimer(prAdapter,  &prActiveTsmReq->rTsmTimer);
			cnmMemFree(prAdapter, prActiveTsmReq->prTsmReq);
		}
		rTsmStatistics.ucTriggerCondition = prTsmReq->rTriggerCond.ucCondition;
		rTsmStatistics.ucMeasureCount = prTsmReq->rTriggerCond.ucMeasureCount;
		rTsmStatistics.ucTriggerTimeout = prTsmReq->rTriggerCond.ucTriggerTimeout;
		rTsmStatistics.ucAvgErrThreshold = prTsmReq->rTriggerCond.ucAvgErrThreshold;
		rTsmStatistics.ucConsecutiveErrThreshold = prTsmReq->rTriggerCond.ucConsecutiveErr;
		rTsmStatistics.ucDelayThreshold = prTsmReq->rTriggerCond.ucDelayThreshold;
		rTsmStatistics.ucBin0Range = prTsmReq->ucB0Range;
	}
	prActiveTsmReq->prTsmReq = prTsmReq;
	rTsmStatistics.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
	rTsmStatistics.ucAcIndex = prTsmReq->ucACI;
	rTsmStatistics.ucTid = prTsmReq->ucTID;
	rTsmStatistics.ucEnabled = TRUE;
	COPY_MAC_ADDR(rTsmStatistics.aucPeerAddr, prTsmReq->aucPeerAddr);
	DBGLOG(WMM, INFO, "enabled=%d, tid=%d\n", rTsmStatistics.ucEnabled, ucTid);
	wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_TSM_STATISTICS_REQUEST,
			TRUE,
			FALSE,
			FALSE,
			NULL,
			NULL,
			sizeof(CMD_SET_TSM_STATISTICS_REQUEST_T),
			(PUINT_8)&rTsmStatistics,
			NULL,
			0);
}

VOID wmmRemoveAllTsmMeasurement(P_ADAPTER_T prAdapter, BOOLEAN fgOnlyTriggered)
{
	P_LINK_T prActiveTsmLink = &prAdapter->rWifiVar.rWmmInfo.rActiveTsmReq;
	struct ACTIVE_RM_TSM_REQ *prActiveTsm = NULL;
	struct ACTIVE_RM_TSM_REQ *prHead =
		LINK_PEEK_HEAD(prActiveTsmLink, struct ACTIVE_RM_TSM_REQ, rLinkEntry);
	BOOLEAN fgFinished = FALSE;

	if (!fgOnlyTriggered)
		cnmTimerStopTimer(prAdapter, &prAdapter->rWifiVar.rWmmInfo.rTsmTimer);
	do {
		prActiveTsm = LINK_PEEK_TAIL(prActiveTsmLink, struct ACTIVE_RM_TSM_REQ, rLinkEntry);
		if (!prActiveTsm)
			break;
		if (prActiveTsm == prHead)
			fgFinished = TRUE;
		if (fgOnlyTriggered && prActiveTsm->prTsmReq->u2Duration)
			continue;
		wmmRemoveTSM(prAdapter, prActiveTsm, TRUE);
	} while (!fgFinished);
	prAdapter->rWifiVar.rWmmInfo.rTriggeredTsmRptTime = 0;
}

BOOLEAN
wmmParseQosAction(
	IN P_ADAPTER_T prAdapter,
	IN P_SW_RFB_T prSwRfb
	)
{
	P_WLAN_ACTION_FRAME prWlanActionFrame = NULL;
	P_UINT_8 pucIE = NULL;
	PARAM_QOS_TSPEC rTspec;
	UINT_16  u2Offset = 0;
	UINT_16  u2IEsBufLen = 0;
	UINT_8 ucTid = WMM_TSPEC_ID_NUM;
	struct WMM_ADDTS_RSP_STEP_PARAM rStepParam;
	BOOLEAN ret = FALSE;

	prWlanActionFrame = (P_WLAN_ACTION_FRAME)prSwRfb->pvHeader;
	DBGLOG(WMM, INFO,  "Action=%d\n",  prWlanActionFrame->ucAction);
	switch (prWlanActionFrame->ucAction) {
	case ACTION_ADDTS_RSP:
	{
		kalMemZero(&rStepParam, sizeof(rStepParam));
		if (prWlanActionFrame->ucCategory == CATEGORY_WME_MGT_NOTIFICATION) {
			struct WMM_ACTION_TSPEC_FRAME *prAddTsRsp = (struct WMM_ACTION_TSPEC_FRAME *)prWlanActionFrame;

			rStepParam.ucDlgToken = prAddTsRsp->ucDlgToken;
			rStepParam.ucStatusCode = prAddTsRsp->ucStatusCode;
			pucIE = (PUINT8)prAddTsRsp->aucInfoElem;
		} else if (prWlanActionFrame->ucCategory == CATEGORY_QOS_ACTION) {
			P_ACTION_ADDTS_RSP_FRAME prAddTsRsp = (P_ACTION_ADDTS_RSP_FRAME)prWlanActionFrame;

			rStepParam.ucDlgToken = prAddTsRsp->ucDialogToken;
			rStepParam.ucStatusCode = prAddTsRsp->ucStatusCode;
			pucIE = (PUINT8)prAddTsRsp->aucInfoElem;
		}
		/*for each IE*/
		u2IEsBufLen = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen -
			(UINT_16)(OFFSET_OF(ACTION_ADDTS_RSP_FRAME, aucInfoElem) - WLAN_MAC_HEADER_LEN);

		if (pucIE == NULL) {
			DBGLOG(WMM, INFO, "pueIE = NULL when Category=%d\n",  prWlanActionFrame->ucCategory);
			break;
		}

		IE_FOR_EACH(pucIE, u2IEsBufLen, u2Offset) {
			switch (IE_ID(pucIE)) {
			case ELEM_ID_TSPEC:
			case ELEM_ID_VENDOR:
				if (wmmParseTspecIE(prAdapter, pucIE, &rTspec)) {
					rStepParam.u2MediumTime = rTspec.u2MediumTime;
					ucTid = rTspec.rTsInfo.ucTid;
					rStepParam.eDir = rTspec.rTsInfo.ucDirection;
					rStepParam.u4PhyRate = rTspec.u4MinPHYRate;
					rStepParam.ucApsd = rTspec.rTsInfo.ucApsd;
				} else {
					DBGLOG(WMM, INFO, "can't parse Tspec IE?!\n");
					ASSERT(FALSE);
				}
				break;
			default:
				break;
			}
		}
		wmmTspecSteps(prAdapter, ucTid, RX_ADDTS_RSP, &rStepParam);
		ret = TRUE;
		break;
	}
	case ACTION_DELTS:
	{
		if (prWlanActionFrame->ucCategory == CATEGORY_WME_MGT_NOTIFICATION) {
			/* wmm Tspec */
			struct WMM_ACTION_TSPEC_FRAME *prDelTs = (struct WMM_ACTION_TSPEC_FRAME *)prWlanActionFrame;

			u2IEsBufLen = prSwRfb->u2PacketLen -
				(UINT_16)OFFSET_OF(struct WMM_ACTION_TSPEC_FRAME, aucInfoElem);
			u2Offset = 0;
			pucIE = prDelTs->aucInfoElem;
			IE_FOR_EACH(pucIE, u2IEsBufLen, u2Offset) {
				if (!wmmParseTspecIE(prAdapter, pucIE, &rTspec))
					continue;
				ucTid = rTspec.rTsInfo.ucTid;
				break;
			}
		} else if (prWlanActionFrame->ucCategory == CATEGORY_QOS_ACTION) {
			/* IEEE 802.11 Tspec */
			P_ACTION_DELTS_FRAME prDelTs = (P_ACTION_DELTS_FRAME)prWlanActionFrame;

			ucTid = WMM_TSINFO_TSID(prDelTs->aucTsInfo[0]);
		}

		wmmTspecSteps(prAdapter, ucTid, RX_DELTS_REQ, NULL);
		ret = TRUE;
		break;
	}
	default:
		break;
	}
	return ret;
}

BOOLEAN
wmmParseTspecIE(P_ADAPTER_T prAdapter, PUINT_8 pucIE, P_PARAM_QOS_TSPEC prTspec)
{
	UINT_32 u4TsInfoValue = 0;
	UINT_8 *pucTemp = NULL;

	if (IE_ID(pucIE) == ELEM_ID_TSPEC) {
		DBGLOG(WMM, INFO, "found 802.11 Tspec Information Element\n");
		/* todo: implement 802.11 Tspec here, assign value to u4TsInfoValue and pucTemp */
		u4TsInfoValue = 0;
		pucTemp = NULL;
		return FALSE; /* we didn't support IEEE 802.11 Tspec now */
	}
	{
		P_IE_WMM_TSPEC_T prIeWmmTspec = (P_IE_WMM_TSPEC_T)pucIE;
		UINT_8 aucWfaOui[] = VENDOR_OUI_WFA;

		if (prIeWmmTspec->ucId != ELEM_ID_VENDOR ||
			 kalMemCmp(prIeWmmTspec->aucOui, aucWfaOui, sizeof(aucWfaOui)) ||
			 prIeWmmTspec->ucOuiType != VENDOR_OUI_TYPE_WMM ||
			 prIeWmmTspec->ucOuiSubtype != VENDOR_OUI_SUBTYPE_WMM_TSPEC) {
			return FALSE;
		}
		u4TsInfoValue |= prIeWmmTspec->aucTsInfo[0];
		u4TsInfoValue |= (prIeWmmTspec->aucTsInfo[1] << 8);
		u4TsInfoValue |= (prIeWmmTspec->aucTsInfo[2] << 16);
		pucTemp = prIeWmmTspec->aucTspecBodyPart;
	}

	prTspec->rTsInfo.ucTrafficType = WMM_TSINFO_TRAFFIC_TYPE(u4TsInfoValue);
	prTspec->rTsInfo.ucTid = WMM_TSINFO_TSID(u4TsInfoValue);
	prTspec->rTsInfo.ucDirection = WMM_TSINFO_DIR(u4TsInfoValue);
	prTspec->rTsInfo.ucAccessPolicy = WMM_TSINFO_AC(u4TsInfoValue);
	prTspec->rTsInfo.ucApsd = WMM_TSINFO_PSB(u4TsInfoValue);
	prTspec->rTsInfo.ucuserPriority = WMM_TSINFO_UP(u4TsInfoValue);

	/* nominal size*/
	WLAN_GET_FIELD_16(pucTemp, &prTspec->u2NominalMSDUSize);
	pucTemp += 2;
	WLAN_GET_FIELD_16(pucTemp, &prTspec->u2MaxMSDUsize);
	pucTemp += 2;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4MinSvcIntv);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4MaxSvcIntv);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4InactIntv);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4SpsIntv);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4SvcStartTime);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4MinDataRate);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4MeanDataRate);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4PeakDataRate);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4MaxBurstSize);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4DelayBound);
	pucTemp += 4;
	WLAN_GET_FIELD_32(pucTemp, &prTspec->u4MinPHYRate);
	pucTemp += 4;
	WLAN_GET_FIELD_16(pucTemp, &prTspec->u2Sba);
	pucTemp += 2;
	WLAN_GET_FIELD_16(pucTemp, &prTspec->u2MediumTime);
	pucTemp += 2;
	ASSERT((pucTemp == (IE_SIZE(pucIE) + pucIE)));
	DBGLOG(WMM, INFO, "TsId=%d, TrafficType=%d, PSB=%d, MediumTime=%d\n",
						prTspec->rTsInfo.ucTid, prTspec->rTsInfo.ucTrafficType,
						prTspec->rTsInfo.ucApsd, prTspec->u2MediumTime);
	return TRUE;
}

static void wmmGetTsmRptTimeout(P_ADAPTER_T prAdapter, ULONG ulParam)
{
	DBGLOG(WMM, ERROR, "timeout to get Tsm Rpt from firmware\n");
	wlanReleasePendingCmdById(prAdapter, CMD_ID_GET_TSM_STATISTICS);
	wmmRemoveTSM(prAdapter, (struct ACTIVE_RM_TSM_REQ *)ulParam, TRUE);
	/* schedule next measurement after a duration based TSM done */
	rlmStartNextMeasurement(prAdapter, FALSE);
}

void wmmComposeTsmRpt(P_ADAPTER_T prAdapter, P_CMD_INFO_T prCmdInfo, PUINT_8 pucEventBuf)
{
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRmRep = &prAdapter->rWifiVar.rRmRepParams;
	P_IE_MEASUREMENT_REPORT_T prTsmRpt = NULL;
	struct RM_TSM_REPORT *prTsmRptField = NULL;
	P_CMD_GET_TSM_STATISTICS_T prTsmStatistic = (P_CMD_GET_TSM_STATISTICS_T)pucEventBuf;
	UINT_16 u2IeSize = OFFSET_OF(IE_MEASUREMENT_REPORT_T, aucReportFields) + sizeof(*prTsmRptField);
	struct ACTIVE_RM_TSM_REQ *prCurrentTsmReq = NULL;
	struct WMM_INFO *prWMMInfo = &prAdapter->rWifiVar.rWmmInfo;

	prCurrentTsmReq =
		wmmGetActiveTsmReq(prAdapter, prTsmStatistic->ucTid, !prTsmStatistic->ucReportReason, FALSE);
	/* prCmdInfo is not NULL or report reason is 0 means it is a command reply, so we need to stop the timer */
	if (prCmdInfo || !prTsmStatistic->ucReportReason)
		cnmTimerStopTimer(prAdapter, &prWMMInfo->rTsmTimer);
	if (!prCurrentTsmReq) {
		DBGLOG(WMM, ERROR, "unexpected Tsm statistic event, tid %d\n", prTsmStatistic->ucTid);
		/* schedule next measurement after a duration based TSM done */
		rlmScheduleNextRm(prAdapter);
		return;
	}

	/* Put the report IE into report frame */
	if (u2IeSize + prRmRep->u2ReportFrameLen > RM_REPORT_FRAME_MAX_LENGTH)
		rlmTxRadioMeasurementReport(prAdapter);

	DBGLOG(WMM, INFO, "tid %d, aci %d\n", prCurrentTsmReq->prTsmReq->ucTID, prCurrentTsmReq->prTsmReq->ucACI);
	prTsmRpt = (P_IE_MEASUREMENT_REPORT_T)(prRmRep->pucReportFrameBuff + prRmRep->u2ReportFrameLen);
	prTsmRpt->ucId = ELEM_ID_MEASUREMENT_REPORT;
	prTsmRpt->ucToken = prCurrentTsmReq->prTsmReq->ucToken;
	prTsmRpt->ucMeasurementType = ELEM_RM_TYPE_TSM_REPORT;
	prTsmRpt->ucReportMode = 0;
	prTsmRpt->ucLength = u2IeSize - 2;
	prTsmRptField = (struct RM_TSM_REPORT *)&prTsmRpt->aucReportFields[0];
	prTsmRptField->u8ActualStartTime = prTsmStatistic->u8StartTime;
	prTsmRptField->u2Duration = prCurrentTsmReq->prTsmReq->u2Duration;
	COPY_MAC_ADDR(prTsmRptField->aucPeerAddress, prTsmStatistic->aucPeerAddr);
	/* TID filed: bit0~bit3 reserved, bit4~bit7: real tid */
	prTsmRptField->ucTID = (prCurrentTsmReq->prTsmReq->ucTID & 0xf) << 4;
	prTsmRptField->ucReason = prTsmStatistic->ucReportReason;
	prTsmRptField->u4TransmittedMsduCnt = prTsmStatistic->u4PktTxDoneOK;
	prTsmRptField->u4DiscardedMsduCnt = prTsmStatistic->u4PktDiscard;
	prTsmRptField->u4FailedMsduCnt = prTsmStatistic->u4PktFail;
	prTsmRptField->u4MultiRetryCnt = prTsmStatistic->u4PktRetryTxDoneOK;
	prTsmRptField->u4CfPollLostCnt = prTsmStatistic->u4PktQosCfPollLost;
	prTsmRptField->u4AvgQueDelay = prTsmStatistic->u4AvgPktQueueDelay;
	prTsmRptField->u4AvgDelay = prTsmStatistic->u4AvgPktTxDelay;
	prTsmRptField->ucBin0Range = prCurrentTsmReq->prTsmReq->ucB0Range;
	kalMemCopy(&prTsmRptField->u4Bin[0], &prTsmStatistic->au4PktCntBin[0], sizeof(prTsmStatistic->au4PktCntBin));
	prRmRep->u2ReportFrameLen += u2IeSize;
	/* For normal TSM, only once measurement */
	if (prCurrentTsmReq->prTsmReq->u2Duration) {
		struct RM_TSM_REQ *prTsmReq = NULL;
		CMD_SET_TSM_STATISTICS_REQUEST_T rTsmStatistics;

		wmmRemoveTSM(prAdapter, prCurrentTsmReq, FALSE);
		/* Resume all triggered tsm whose TC is same with this normal tsm */
		LINK_FOR_EACH_ENTRY(prCurrentTsmReq, &prWMMInfo->rActiveTsmReq, rLinkEntry, struct ACTIVE_RM_TSM_REQ) {
			prTsmReq = prCurrentTsmReq->prTsmReq;
			if (prTsmReq->u2Duration || prTsmReq->ucACI != prTsmStatistic->ucAcIndex)
				continue;
			kalMemZero(&rTsmStatistics, sizeof(rTsmStatistics));
			rTsmStatistics.ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
			rTsmStatistics.ucEnabled = TRUE;
			rTsmStatistics.ucAcIndex = prTsmReq->ucACI;
			rTsmStatistics.ucTid = prTsmReq->ucTID;
			COPY_MAC_ADDR(rTsmStatistics.aucPeerAddr, prTsmReq->aucPeerAddr);
			rTsmStatistics.ucTriggerCondition = prTsmReq->rTriggerCond.ucCondition;
			rTsmStatistics.ucMeasureCount = prTsmReq->rTriggerCond.ucMeasureCount;
			rTsmStatistics.ucTriggerTimeout = prTsmReq->rTriggerCond.ucTriggerTimeout;
			rTsmStatistics.ucAvgErrThreshold = prTsmReq->rTriggerCond.ucAvgErrThreshold;
			rTsmStatistics.ucConsecutiveErrThreshold = prTsmReq->rTriggerCond.ucConsecutiveErr;
			rTsmStatistics.ucDelayThreshold = prTsmReq->rTriggerCond.ucDelayThreshold;
			rTsmStatistics.ucBin0Range = prTsmReq->ucB0Range;
			wlanSendSetQueryCmd(prAdapter, CMD_ID_SET_TSM_STATISTICS_REQUEST, TRUE, FALSE, FALSE, NULL,
								NULL, sizeof(rTsmStatistics),
								(PUINT_8)&rTsmStatistics, NULL, 0);
		}
		/* schedule next measurement after a duration based TSM done */
		rlmScheduleNextRm(prAdapter);
	} else {
	/* Triggered TSM, we should send TSM report to peer if the first report time to now more than 10 second */
		OS_SYSTIME rCurrent = kalGetTimeTick();

		if (prWMMInfo->rTriggeredTsmRptTime == 0)
			prWMMInfo->rTriggeredTsmRptTime = rCurrent;
		else if (CHECK_FOR_TIMEOUT(rCurrent, prWMMInfo->rTriggeredTsmRptTime, 10000)) {
			rlmTxRadioMeasurementReport(prAdapter);
			prWMMInfo->rTriggeredTsmRptTime = 0;
		}
	}
}

VOID wmmNotifyDisconnected(P_ADAPTER_T prAdapter)
{
	UINT_8 ucTid = 0;

	for (; ucTid < WMM_TSPEC_ID_NUM; ucTid++)
		wmmTspecSteps(prAdapter, ucTid, DISC_DELTS_REQ, NULL);
	wmmRemoveAllTsmMeasurement(prAdapter, FALSE);
}

BOOLEAN wmmTsmIsOngoing(P_ADAPTER_T prAdapter)
{
	return !LINK_IS_EMPTY(&prAdapter->rWifiVar.rWmmInfo.rActiveTsmReq);
}

/* This function implements TS replacement rule
** Replace case base on same AC:
** 1. old: Uni-dir; New: Bi-dir or same dir with old
** 2. old: Bi-dir; New: Bi-dir or Uni-dir
** 3. old: two diff Uni-dir; New: Bi-dir
** for detail, see WMM spec V1.2.0, section 3.5
*/
static VOID wmmMayDoTsReplacement(P_ADAPTER_T prAdapter, UINT_8 ucNewTid)
{
	struct TSPEC_INFO *prTspec = &prAdapter->rWifiVar.rWmmInfo.arTsInfo[0];
	UINT_8 ucTid = 0;

	for (; ucTid < WMM_TSPEC_ID_NUM; ucTid++) {
		if (ucTid == ucNewTid)
			continue;
		if (prTspec[ucTid].eState != QOS_TS_ACTIVE ||
			prTspec[ucTid].eAC != prTspec[ucNewTid].eAC)
			continue;
		if (prTspec[ucNewTid].eDir != prTspec[ucTid].eDir &&
			prTspec[ucNewTid].eDir < BI_DIR_TS &&
			prTspec[ucTid].eDir < BI_DIR_TS)
			continue;
		prTspec[ucTid].eAC = ACI_NUM;
		prTspec[ucTid].eState = QOS_TS_INACTIVE;
	}
	wmmDumpActiveTspecs(prAdapter, NULL, 0);
}

UINT_32 wmmDumpActiveTspecs(P_ADAPTER_T prAdapter, PUINT_8 pucBuffer, UINT_16 u2BufferLen)
{
	UINT_8 ucTid = 0;
	INT_32 i4BytesWritten = 0;

	struct TSPEC_INFO *prTspec = &prAdapter->rWifiVar.rWmmInfo.arTsInfo[0];

	for (; ucTid < WMM_TSPEC_ID_NUM; ucTid++, prTspec++) {
		if (prTspec->eState != QOS_TS_ACTIVE)
			continue;
		if (u2BufferLen > 0 && pucBuffer) {
			i4BytesWritten += kalSnprintf(pucBuffer+i4BytesWritten, u2BufferLen,
						"Tid %d, AC %d, Dir %d, Uapsd %d, MediumTime %d, PhyRate %u\n",
						ucTid, prTspec->eAC, prTspec->eDir, prTspec->fgUapsd,
						prTspec->u2MediumTime, prTspec->u4PhyRate);
			if (i4BytesWritten <= 0)
				break;
			u2BufferLen -= (UINT_16)i4BytesWritten;
		} else
			DBGLOG(WMM, INFO, "Tid %d, AC %d, Dir %d, Uapsd %d, MediumTime %d, PhyRate %u\n",
				   ucTid, prTspec->eAC, prTspec->eDir, prTspec->fgUapsd,
				   prTspec->u2MediumTime, prTspec->u4PhyRate);
	}
	return (UINT_32)i4BytesWritten;
}
