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

#if CFG_SUPPORT_ROAMING
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

#if DBG
/*lint -save -e64 Type mismatch */
static PUINT_8 apucDebugRoamingState[ROAMING_STATE_NUM] = {
	(PUINT_8) DISP_STRING("ROAMING_STATE_IDLE"),
	(PUINT_8) DISP_STRING("ROAMING_STATE_DECISION"),
	(PUINT_8) DISP_STRING("ROAMING_STATE_DISCOVERY"),
	(PUINT_8) DISP_STRING("ROAMING_STATE_ROAM")
};

/*lint -restore */
#endif /* DBG */

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
* @brief Initialize the value in ROAMING_FSM_INFO_T for ROAMING FSM operation
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmInit(IN P_ADAPTER_T prAdapter)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DBGLOG(ROAMING, LOUD, "->roamingFsmInit(): Current Time = %u\n", kalGetTimeTick());

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Initiate FSM */
	prRoamingFsmInfo->fgIsEnableRoaming = prConnSettings->fgIsEnableRoaming;
	prRoamingFsmInfo->eCurrentState = ROAMING_STATE_IDLE;
	prRoamingFsmInfo->rRoamingDiscoveryUpdateTime = 0;
	prRoamingFsmInfo->DrvRoamingAllow = 1;

}				/* end of roamingFsmInit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Uninitialize the value in AIS_FSM_INFO_T for AIS FSM operation
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmUninit(IN P_ADAPTER_T prAdapter)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;

	DBGLOG(ROAMING, LOUD, "->roamingFsmUninit(): Current Time = %u\n", kalGetTimeTick());

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	prRoamingFsmInfo->eCurrentState = ROAMING_STATE_IDLE;

}				/* end of roamingFsmUninit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Send commands to firmware
*
* @param [IN P_ADAPTER_T]       prAdapter
*        [IN P_ROAMING_PARAM_T] prParam
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmSendCmd(IN P_ADAPTER_T prAdapter, IN P_ROAMING_PARAM_T prParam)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	WLAN_STATUS rStatus;

	DBGLOG(ROAMING, LOUD, "->roamingFsmSendCmd(): Current Time = %u\n", kalGetTimeTick());

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_ROAMING_TRANSIT,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(ROAMING_PARAM_T),	/* u4SetQueryInfoLen */
				      (PUINT_8) prParam,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	ASSERT(rStatus == WLAN_STATUS_PENDING);

}				/* end of roamingFsmSendCmd() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Update the recent time when ScanDone occurred
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmScanResultsUpdate(IN P_ADAPTER_T prAdapter)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, LOUD, "->roamingFsmScanResultsUpdate(): Current Time = %u", kalGetTimeTick());

	GET_CURRENT_SYSTIME(&prRoamingFsmInfo->rRoamingDiscoveryUpdateTime);

}				/* end of roamingFsmScanResultsUpdate() */
#if !(CFG_SUPPORT_NCHO)
static BOOLEAN roamingFsmIsNeedScan(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prRoamBSSDescList;
	P_ROAM_BSS_DESC_T prRoamBssDesc;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc;
	CMD_ID_SET_ROAMING_SKIP_T rCmdSwCtrl;
	BOOLEAN fgIsNeedScan = FALSE;
	BOOLEAN fgIsRoamingSSID = FALSE;

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prRoamBSSDescList = &prScanInfo->rRoamBSSDescList;
	/* Count same BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prRoamBssDesc, prRoamBSSDescList, rLinkEntry, ROAM_BSS_DESC_T) {
		if (EQUAL_SSID(prRoamBssDesc->aucSSID,
				       prRoamBssDesc->ucSSIDLen,
				       prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen)) {
			fgIsRoamingSSID = TRUE;
			fgIsNeedScan = TRUE;
			DBGLOG(INIT, INFO, "roamingFsmSteps: IsRoamingSSID:%d\n", fgIsRoamingSSID);
			break;
		}
	}

	if (!fgIsRoamingSSID) {
		prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
		if (prBssDesc) {

			rCmdSwCtrl.IsRoamingSkipOneAp = TRUE;
			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SET_ROAMING_SKIP,
					    TRUE,
					    FALSE,
					    FALSE, NULL, NULL, sizeof(CMD_ID_SET_ROAMING_SKIP_T),
					    (PUINT_8)&rCmdSwCtrl, NULL, 0);

			DBGLOG(INIT, INFO, "roamingFsmSteps: RCPI:%d RoamSkipTimes:%d\n",
								prBssDesc->ucRCPI, prAisBssInfo->ucRoamSkipTimes);
			if (prBssDesc->ucRCPI > CFG_GOOG_RCPI_THRESHOLD) {
				prAisBssInfo->ucRoamSkipTimes = CFG_GOOG_RCPI_SCAN_SKIP_TIMES;
				prAisBssInfo->fgGoodRcpiArea = TRUE;
				prAisBssInfo->fgPoorRcpiArea = FALSE;
			} else {
				if (prAisBssInfo->fgGoodRcpiArea) {
					prAisBssInfo->ucRoamSkipTimes--;
				} else if (prBssDesc->ucRCPI > CFG_POOR_RCPI_THRESHOLD) {
					if (!prAisBssInfo->fgPoorRcpiArea) {
						prAisBssInfo->ucRoamSkipTimes = CFG_POOR_RCPI_SCAN_SKIP_TIMES;
						prAisBssInfo->fgPoorRcpiArea = TRUE;
						prAisBssInfo->fgGoodRcpiArea = FALSE;
					} else {
						prAisBssInfo->ucRoamSkipTimes--;
					}
				} else {
					prAisBssInfo->fgPoorRcpiArea = FALSE;
					prAisBssInfo->fgGoodRcpiArea = FALSE;
					prAisBssInfo->ucRoamSkipTimes--;
				}
			}

			if (prAisBssInfo->ucRoamSkipTimes == 0) {
				prAisBssInfo->ucRoamSkipTimes = CFG_GOOG_RCPI_SCAN_SKIP_TIMES;
				prAisBssInfo->fgPoorRcpiArea = FALSE;
				prAisBssInfo->fgGoodRcpiArea = FALSE;
				DBGLOG(INIT, INFO, "roamingFsmSteps: Need Scan\n");
				fgIsNeedScan = TRUE;
			}
		}
	}

	return fgIsNeedScan;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief The Core FSM engine of ROAMING for AIS Infra.
*
* @param [IN P_ADAPTER_T]          prAdapter
*        [IN ENUM_ROAMING_STATE_T] eNextState Enum value of next AIS STATE
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_ROAMING_STATE_T eNextState)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_ROAMING_STATE_T ePreviousState;
	BOOLEAN fgIsTransition = (BOOLEAN) FALSE;
#if CFG_SUPPORT_NCHO
	UINT32 u4ScnResultsTimeout = ROAMING_DISCOVERY_TIMEOUT_SEC;
	UINT_32 u4ReqScan = FALSE;
#endif

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	do {

		/* Do entering Next State */
#if DBG
		DBGLOG(ROAMING, STATE, "TRANSITION: [%s] -> [%s]\n",
					apucDebugRoamingState[prRoamingFsmInfo->eCurrentState],
					apucDebugRoamingState[eNextState]);
#else
		DBGLOG(ROAMING, STATE, "[%d] TRANSITION: [%d] -> [%d]\n",
					DBG_ROAMING_IDX, prRoamingFsmInfo->eCurrentState, eNextState);
#endif
		/* NOTE(Kevin): This is the only place to change the eCurrentState(except initial) */
		ePreviousState = prRoamingFsmInfo->eCurrentState;
		prRoamingFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (BOOLEAN) FALSE;

		/* Do tasks of the State that we just entered */
		switch (prRoamingFsmInfo->eCurrentState) {
			/* NOTE(Kevin): we don't have to rearrange the sequence of following
			 * switch case. Instead I would like to use a common lookup table of array
			 * of function pointer to speed up state search.
			 */
		case ROAMING_STATE_IDLE:
		case ROAMING_STATE_DECISION:
			break;

		case ROAMING_STATE_DISCOVERY:
			{
#if CFG_SUPPORT_NCHO
				if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE) {
					u4ScnResultsTimeout = prAdapter->rNchoInfo.u4RoamScanPeriod;
					DBGLOG(ROAMING, TRACE, "NCHO u4ScnResultsTimeout is %d\n", u4ScnResultsTimeout);
				}

				if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), prRoamingFsmInfo->rRoamingDiscoveryUpdateTime,
							  SEC_TO_SYSTIME(u4ScnResultsTimeout))) {
					DBGLOG(ROAMING, LOUD, "DiscoveryUpdateTime Timeout");
					u4ReqScan =  TRUE;
				} else {
					DBGLOG(ROAMING, LOUD, "DiscoveryUpdateTime Updated");
#if CFG_SUPPORT_ROAMING_ENC
					if (prAdapter->fgIsRoamingEncEnabled == TRUE)
						u4ReqScan =  TRUE;
					else
#endif /* CFG_SUPPORT_ROAMING_ENC */
						u4ReqScan = FALSE;
				}
				aisFsmRunEventRoamingDiscovery(prAdapter, u4ReqScan);
#else
				OS_SYSTIME rCurrentTime;
				BOOLEAN fgIsNeedScan = FALSE;

				fgIsNeedScan = roamingFsmIsNeedScan(prAdapter);

				GET_CURRENT_SYSTIME(&rCurrentTime);
				if (CHECK_FOR_TIMEOUT(rCurrentTime, prRoamingFsmInfo->rRoamingDiscoveryUpdateTime,
						  SEC_TO_SYSTIME(ROAMING_DISCOVERY_TIMEOUT_SEC)) && fgIsNeedScan) {
					DBGLOG(ROAMING, LOUD, "roamingFsmSteps: DiscoveryUpdateTime Timeout");
					aisFsmRunEventRoamingDiscovery(prAdapter, TRUE);
				} else {
					DBGLOG(ROAMING, LOUD, "roamingFsmSteps: DiscoveryUpdateTime Updated");
#if CFG_SUPPORT_ROAMING_ENC
					if (prAdapter->fgIsRoamingEncEnabled == TRUE)
						aisFsmRunEventRoamingDiscovery(prAdapter, TRUE);
					else
#endif /* CFG_SUPPORT_ROAMING_ENC */
						aisFsmRunEventRoamingDiscovery(prAdapter, FALSE);
				}
#endif
			}

			break;

		case ROAMING_STATE_ROAM:
			break;

		default:
			ASSERT(0);	/* Make sure we have handle all STATEs */
		}
	} while (fgIsTransition);

	return;

}				/* end of roamingFsmSteps() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Transit to Decision state after join completion
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmRunEventStart(IN P_ADAPTER_T prAdapter)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_ROAMING_STATE_T eNextState;
	P_BSS_INFO_T prAisBssInfo;
	ROAMING_PARAM_T rParam;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	if (prAisBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;

	DBGLOG(ROAMING, EVENT, "EVENT-ROAMING START: Current Time = %u\n", kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as DECISION, DISCOVERY -> DECISION */
	if (!(prRoamingFsmInfo->eCurrentState == ROAMING_STATE_IDLE ||
		prRoamingFsmInfo->eCurrentState == ROAMING_STATE_ROAM))
		return;

	eNextState = ROAMING_STATE_DECISION;
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rParam.u2Event = ROAMING_EVENT_START;
		roamingFsmSendCmd(prAdapter, (P_ROAMING_PARAM_T) & rParam);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}

}				/* end of roamingFsmRunEventStart() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Transit to Discovery state when deciding to find a candidate
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmRunEventDiscovery(IN P_ADAPTER_T prAdapter, IN P_ROAMING_PARAM_T prParam)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_ROAMING_STATE_T eNextState;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT, "EVENT-ROAMING DISCOVERY: Current Time = %u Reason = %u\n",
		kalGetTimeTick(), prParam->u2Reason);

	/* DECISION -> DISCOVERY */
	/* Errors as IDLE, DISCOVERY, ROAM -> DISCOVERY */
	if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_DECISION)
		return;
#if CFG_SUPPORT_ROAMING_ENC
	prRoamingFsmInfo->RoamingEntryTimeoutSkipCount = 0;
#endif

	eNextState = ROAMING_STATE_DISCOVERY;
	/* DECISION -> DISCOVERY */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		P_BSS_INFO_T prAisBssInfo;
		P_BSS_DESC_T prBssDesc;
		PARAM_MAC_ADDRESS arBssid;
		PARAM_SSID_T rSsid;
		P_AIS_FSM_INFO_T prAisFsmInfo;
		P_CONNECTION_SETTINGS_T prConnSettings;

		kalMemZero(&rSsid, sizeof(PARAM_SSID_T));
		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

		/* sync. rcpi with firmware */
		prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
		prBssDesc = prAisFsmInfo->prTargetBssDesc;
		if (prBssDesc) {
			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
			COPY_MAC_ADDR(arBssid, prBssDesc->aucBSSID);
		} else  {
			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);
			COPY_MAC_ADDR(arBssid, prConnSettings->aucBSSID);
		}
		prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter, arBssid, TRUE, &rSsid);
		if (prBssDesc) {
			prBssDesc->ucRCPI = (UINT_8) (prParam->u2Data & 0xff);
			DBGLOG(ROAMING, INFO, "RCPI %u\n", prBssDesc->ucRCPI);
		}

		roamingFsmSteps(prAdapter, eNextState);
	}

}				/* end of roamingFsmRunEventDiscovery() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Transit to Roam state after Scan Done
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmRunEventRoam(IN P_ADAPTER_T prAdapter)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_ROAMING_STATE_T eNextState;
	ROAMING_PARAM_T rParam;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT, "EVENT-ROAMING ROAM: Current Time = %u\n", kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as IDLE, DECISION, ROAM -> ROAM */
	if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_DISCOVERY)
		return;

	eNextState = ROAMING_STATE_ROAM;
	/* DISCOVERY -> ROAM */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rParam.u2Event = ROAMING_EVENT_ROAM;
		roamingFsmSendCmd(prAdapter, (P_ROAMING_PARAM_T) & rParam);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}

}				/* end of roamingFsmRunEventRoam() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Transit to Decision state as being failed to find out any candidate
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmRunEventFail(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Param)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_ROAMING_STATE_T eNextState;
	ROAMING_PARAM_T rParam;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	/* Check Roaming Conditions */
	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT, "EVENT-ROAMING FAIL: reason %x Current Time = %u\n", u4Param, kalGetTimeTick());

	/* IDLE, ROAM -> DECISION */
	/* Errors as IDLE, DECISION, DISCOVERY -> DECISION */
	if (prRoamingFsmInfo->eCurrentState != ROAMING_STATE_ROAM)
		return;

	eNextState = ROAMING_STATE_DECISION;
	/* ROAM -> DECISION */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rParam.u2Event = ROAMING_EVENT_FAIL;
		rParam.u2Data = (UINT_16) (u4Param & 0xffff);
		roamingFsmSendCmd(prAdapter, (P_ROAMING_PARAM_T) & rParam);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}

}				/* end of roamingFsmRunEventFail() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Transit to Idle state as beging aborted by other moduels, AIS
*
* @param [IN P_ADAPTER_T] prAdapter
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID roamingFsmRunEventAbort(IN P_ADAPTER_T prAdapter)
{
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_ROAMING_STATE_T eNextState;
	ROAMING_PARAM_T rParam;

	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	if (!(prRoamingFsmInfo->fgIsEnableRoaming))
		return;

	DBGLOG(ROAMING, EVENT, "EVENT-ROAMING ABORT: Current Time = %u\n", kalGetTimeTick());

	eNextState = ROAMING_STATE_IDLE;
	/* IDLE, DECISION, DISCOVERY, ROAM -> IDLE */
	if (eNextState != prRoamingFsmInfo->eCurrentState) {
		rParam.u2Event = ROAMING_EVENT_ABORT;
		roamingFsmSendCmd(prAdapter, (P_ROAMING_PARAM_T) & rParam);

		/* Step to next state */
		roamingFsmSteps(prAdapter, eNextState);
	}

}				/* end of roamingFsmRunEventAbort() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process events from firmware
*
* @param [IN P_ADAPTER_T]       prAdapter
*        [IN P_ROAMING_PARAM_T] prParam
*
* @return none
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS roamingFsmProcessEvent(IN P_ADAPTER_T prAdapter, IN P_ROAMING_PARAM_T prParam)
{
	DBGLOG(ROAMING, LOUD, "ROAMING Process Events: Current Time = %u\n", kalGetTimeTick());

	if (prParam->u2Event == ROAMING_EVENT_DISCOVERY)
		roamingFsmRunEventDiscovery(prAdapter, prParam);

	return WLAN_STATUS_SUCCESS;
}

#endif
