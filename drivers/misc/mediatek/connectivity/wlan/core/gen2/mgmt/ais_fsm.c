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
#include "fwcfg.h"
static VOID aisFsmSetOkcTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParam);
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define AIS_ROAMING_CONNECTION_TRIAL_LIMIT  2
#define AIS_ROAMING_SCAN_CHANNEL_DWELL_TIME 80
#if CFG_SUPPORT_ROAMING_RETRY
#define AIS_ROAMING_RETRY_BSS_THRESHOLD     1
#endif
#define CTIA_MAGIC_SSID                     "ctia_test_only_*#*#3646633#*#*"
#define CTIA_MAGIC_SSID_LEN                 30

#define AIS_JOIN_TIMEOUT                    7
#define AIS_DEFAULT_ROAMING_THRESHOLD       -75 /* dbm */

#define AIS_DEFAULT_ROAMING_THRESHOLD       -75 /* dbm */
#define AIS_DRT_ROAMING_THRESHOLD           -85 /* DRT=Dynamic Roaming Threshold, dbm */
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
static PUINT_8 apucDebugAisState[AIS_STATE_NUM] = {
	(PUINT_8) DISP_STRING("AIS_STATE_IDLE"),
	(PUINT_8) DISP_STRING("AIS_STATE_SEARCH"),
	(PUINT_8) DISP_STRING("AIS_STATE_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_ONLINE_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_LOOKING_FOR"),
	(PUINT_8) DISP_STRING("AIS_STATE_WAIT_FOR_NEXT_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_REQ_CHANNEL_JOIN"),
	(PUINT_8) DISP_STRING("AIS_STATE_JOIN"),
	(PUINT_8) DISP_STRING("AIS_STATE_IBSS_ALONE"),
	(PUINT_8) DISP_STRING("AIS_STATE_IBSS_MERGE"),
	(PUINT_8) DISP_STRING("AIS_STATE_NORMAL_TR"),
	(PUINT_8) DISP_STRING("AIS_STATE_DISCONNECTING"),
	(PUINT_8) DISP_STRING("AIS_STATE_REQ_REMAIN_ON_CHANNEL"),
	(PUINT_8) DISP_STRING("AIS_STATE_REMAIN_ON_CHANNEL")
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
static VOID aisRemoveOldestBcnTimeout(P_AIS_FSM_INFO_T prAisFsmInfo);
static VOID aisRemoveDisappearedBlacklist(P_ADAPTER_T prAdapter);

static VOID
aisSendNeighborRequest(P_ADAPTER_T prAdapter);
#if CFG_SUPPORT_DYNAMIC_ROAM
static VOID aisFsmSetRoamingThreshold(P_ADAPTER_T prAdapter, INT_8 cThreshold);
#endif
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static VOID aisResetBssTranstionMgtParam(P_AIS_SPECIFIC_BSS_INFO_T prSpecificBssInfo)
{
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam = &prSpecificBssInfo->rBTMParam;

#if !CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	return;
#endif
	if (prBtmParam->u2PeerNeighborBssLen > 0)
		kalMemFree(prBtmParam->pucPeerNeighborBss, VIR_MEM_TYPE, prBtmParam->u2PeerNeighborBssLen);
	kalMemZero(prBtmParam, sizeof(*prBtmParam));
}

/*----------------------------------------------------------------------------*/
/*!
* @brief the function is used to initialize the value of the connection settings for
*        AIS network
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisInitializeConnectionSettings(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_8 aucAnyBSSID[] = BC_BSSID;
	UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	int i = 0;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* Setup default values for operation */
	COPY_MAC_ADDR(prConnSettings->aucMacAddress, aucZeroMacAddr);

	if (prRegInfo)
		prConnSettings->ucDelayTimeOfDisconnectEvent =
		    (!prAdapter->fgIsHw5GBandDisabled && prRegInfo->ucSupport5GBand) ?
		    AIS_DELAY_TIME_OF_DISC_SEC_DUALBAND : AIS_DELAY_TIME_OF_DISC_SEC_ONLY_2G4;
	else
		prConnSettings->ucDelayTimeOfDisconnectEvent = AIS_DELAY_TIME_OF_DISC_SEC_ONLY_2G4;

	COPY_MAC_ADDR(prConnSettings->aucBSSID, aucAnyBSSID);
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	prConnSettings->eReConnectLevel = RECONNECT_LEVEL_MIN;
	prConnSettings->fgIsConnReqIssued = FALSE;
	prConnSettings->fgIsDisconnectedByNonRequest = FALSE;

	prConnSettings->ucSSIDLen = 0;

	prConnSettings->eOPMode = NET_TYPE_INFRA;

	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	if (prRegInfo) {
		prConnSettings->ucAdHocChannelNum = (UINT_8) nicFreq2ChannelNum(prRegInfo->u4StartFreq);
		prConnSettings->eAdHocBand = prRegInfo->u4StartFreq < 5000000 ? BAND_2G4 : BAND_5G;
		prConnSettings->eAdHocMode = (ENUM_PARAM_AD_HOC_MODE_T) (prRegInfo->u4AdhocMode);
	}

	prConnSettings->eAuthMode = AUTH_MODE_OPEN;

	prConnSettings->eEncStatus = ENUM_ENCRYPTION_DISABLED;

	prConnSettings->fgIsScanReqIssued = FALSE;

	/* MIB attributes */
	prConnSettings->u2BeaconPeriod = DOT11_BEACON_PERIOD_DEFAULT;

	prConnSettings->u2RTSThreshold = DOT11_RTS_THRESHOLD_DEFAULT;

	prConnSettings->u2DesiredNonHTRateSet = RATE_SET_ALL_ABG;

	/* prConnSettings->u4FreqInKHz; */ /* Center frequency */

	/* Set U-APSD AC */
	prConnSettings->bmfgApsdEnAc = PM_UAPSD_NONE;

	secInit(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* Features */
	prConnSettings->fgIsEnableRoaming = FALSE;
#if CFG_SUPPORT_ROAMING
	if (prRegInfo)
		prConnSettings->fgIsEnableRoaming = ((prRegInfo->fgDisRoaming > 0) ? (FALSE) : (TRUE));
#endif /* CFG_SUPPORT_ROAMING */

	prConnSettings->fgIsAdHocQoSEnable = FALSE;

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
		prConnSettings->fgSecModeChangeStartTimer = FALSE;
#endif

	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGN;

	/* Set default bandwidth modes */
	prConnSettings->uc2G4BandwidthMode = CONFIG_BW_20M;
	prConnSettings->uc5GBandwidthMode = CONFIG_BW_20_40M;

	prConnSettings->rRsnInfo.ucElemId = 0x30;
	prConnSettings->rRsnInfo.u2Version = 0x0001;
	prConnSettings->rRsnInfo.u4GroupKeyCipherSuite = 0;
	prConnSettings->rRsnInfo.u4PairwiseKeyCipherSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++)
		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[i] = 0;
	prConnSettings->rRsnInfo.u4AuthKeyMgtSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++)
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[i] = 0;
	prConnSettings->rRsnInfo.u2RsnCap = 0;
	prConnSettings->rRsnInfo.fgRsnCapPresent = FALSE;

}				/* end of aisFsmInitializeConnectionSettings() */

/*----------------------------------------------------------------------------*/
/*!
* @brief the function is used to initialize the value in AIS_FSM_INFO_T for
*        AIS FSM operation
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32 ucScanTimeoutTimes;
VOID aisFsmInit(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmInit()");
	DBGLOG(SW1, TRACE, "->aisFsmInit()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	/* 4 <1> Initiate FSM */
	prAisFsmInfo->ePreviousState = AIS_STATE_IDLE;
	prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;

	prAisFsmInfo->ucAvailableAuthTypes = 0;

	prAisFsmInfo->prTargetBssDesc = (P_BSS_DESC_T) NULL;

	prAisFsmInfo->ucSeqNumOfReqMsg = 0;
	prAisFsmInfo->ucSeqNumOfChReq = 0;
	prAisFsmInfo->ucSeqNumOfScanReq = 0;

	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
#if CFG_SUPPORT_ROAMING
	prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif /* CFG_SUPPORT_ROAMING */
	prAisFsmInfo->fgIsChannelRequested = FALSE;
	prAisFsmInfo->fgIsChannelGranted = FALSE;
#if CFG_SCAN_ABORT_HANDLE
	prAisFsmInfo->fgIsAbortEvnetDuringScan = FALSE;
#endif
	prAisFsmInfo->ucJoinFailCntAfterScan = 0;
#if CFG_SUPPORT_DYNAMIC_ROAM
	prAisFsmInfo->cRoamTriggerThreshold = 0;
#endif

	/* 4 <1.1> Initiate FSM - Timer INIT */
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rBGScanTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventBGSleepTimeOut, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rIbssAloneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventIbssAloneTimeOut, (ULONG) NULL);

	prAisFsmInfo->u4PostponeIndStartTime = 0;

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rJoinTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventJoinTimeout, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rScanDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventScanDoneTimeOut, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rChannelTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventChannelTimeout, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rDeauthDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventDeauthTimeout, (ULONG) NULL);
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
		cnmTimerInitTimer(prAdapter,
				  &prAisFsmInfo->rSecModeChangeTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventSecModeChangeTimeout, (ULONG) NULL);
#endif

	cnmTimerInitTimer(prAdapter,
				  &prAisFsmInfo->rWaitOkcPMKTimer,
				  (PFN_MGMT_TIMEOUT_FUNC)aisFsmSetOkcTimeout, (ULONG) NULL);
	/* 4 <1.2> Initiate PWR STATE */
	SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 4 <2> Initiate BSS_INFO_T - common part */
	BSS_INFO_INIT(prAdapter, NETWORK_TYPE_AIS_INDEX);
	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr, prAdapter->rWifiVar.aucMacAddress);

	/* 4 <3> Initiate BSS_INFO_T - private part */
	/* TODO */
	prAisBssInfo->eBand = BAND_2G4;
	prAisBssInfo->ucPrimaryChannel = 1;
	prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;

	/* 4 <4> Allocate MSDU_INFO_T for Beacon */
	prAisBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
						OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem[0]) + MAX_IE_LENGTH);

	if (prAisBssInfo->prBeacon) {
		prAisBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
		prAisBssInfo->prBeacon->ucStaRecIndex = 0xFF;	/* NULL STA_REC */
	} else {
		ASSERT(0);
	}

#if 0
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
#else
	if (prAdapter->u4UapsdAcBmp == 0) {
		prAdapter->u4UapsdAcBmp = CFG_INIT_UAPSD_AC_BMP;
		/* ASSERT(prAdapter->u4UapsdAcBmp); */
	}
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = (UINT_8) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = (UINT_8) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp = (UINT_8) prAdapter->u4MaxSpLen;
#endif

	/* request list initialization */
	LINK_INITIALIZE(&prAisFsmInfo->rPendingReqList);
	LINK_MGMT_INIT(&prAdapter->rWifiVar.rConnSettings.rBlackList);
	LINK_MGMT_INIT(&prAisFsmInfo->rBcnTimeout);
	kalMemZero(&prAisSpecificBssInfo->arCurEssChnlInfo[0],
		sizeof(prAisSpecificBssInfo->arCurEssChnlInfo));
	LINK_INITIALIZE(&prAisSpecificBssInfo->rCurEssLink);
	kalMemZero(&prAisSpecificBssInfo->rBTMParam, sizeof(prAisSpecificBssInfo->rBTMParam));

#if (CFG_REFACTORY_PMKSA == 1)
	LINK_INITIALIZE(&prAisSpecificBssInfo->rPmkidCache);
#endif

	/* DBGPRINTF("[2] ucBmpDeliveryAC:0x%x, ucBmpTriggerAC:0x%x, ucUapsdSp:0x%x", */
	/* prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC, */
	/* prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC, */
	/* prAisBssInfo->rPmProfSetupInfo.ucUapsdSp); */

	/*reset ucScanTimeoutTimes value*/
	ucScanTimeoutTimes = 0;

}				/* end of aisFsmInit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief the function is used to uninitialize the value in AIS_FSM_INFO_T for
*        AIS FSM operation
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmUninit(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmUninit()");
	DBGLOG(SW1, INFO, "->aisFsmUninit()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	/* 4 <1> Stop all timers */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);	/* Add by Enlai */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer);
	/* 4 <2> flush pending request */
	aisFsmFlushRequest(prAdapter);
	aisResetBssTranstionMgtParam(prAisSpecificBssInfo);

	/* 4 <3> Reset driver-domain BSS-INFO */
	if (prAisBssInfo->prBeacon) {
		cnmMgtPktFree(prAdapter, prAisBssInfo->prBeacon);
		prAisBssInfo->prBeacon = NULL;
	}
#if CFG_SUPPORT_802_11W
	rsnStopSaQuery(prAdapter);
#endif
	LINK_MGMT_UNINIT(&prAdapter->rWifiVar.rConnSettings.rBlackList,
		struct AIS_BLACKLIST_ITEM, VIR_MEM_TYPE);
	LINK_MGMT_UNINIT(&prAisFsmInfo->rBcnTimeout, struct AIS_BEACON_TIMEOUT_BSS, VIR_MEM_TYPE);

#if (CFG_REFACTORY_PMKSA == 1)
	/* make sure pmkid cached is empty after uninit*/
	rsnFlushPmkid(prAdapter);
#endif
}				/* end of aisFsmUninit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Initialization of JOIN STATE
*
* @param[in] prBssDesc  The pointer of BSS_DESC_T which is the BSS we will try to join with.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateInit_JOIN(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_STA_RECORD_T prStaRec;
	P_MSG_JOIN_REQ_T prJoinReqMsg;
#if (CFG_REFACTORY_PMKSA == 0)
	UINT_32 u4Entry = 0;
#endif

	DEBUGFUNC("aisFsmStateInit_JOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	ASSERT(prBssDesc);

	/* 4 <1> We are going to connect to this BSS. */
	prBssDesc->fgIsConnecting = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter, STA_TYPE_LEGACY_AP, NETWORK_TYPE_AIS_INDEX, prBssDesc);
	if (prStaRec == NULL) {
		DBGLOG(AIS, WARN, "Create station record fail\n");
		return;
	}

	prAisFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <2.1> sync. to firmware domain */
	if (prStaRec->ucStaState == STA_STATE_1)
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
	/* 4 <3> Update ucAvailableAuthTypes which we can choice during SAA */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

		prStaRec->fgIsReAssoc = FALSE;

		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_NON_RSN_FT: /* FT initial mobility doamin association always use Open AA */
		case AUTH_MODE_WPA2_FT:
		case AUTH_MODE_WPA2_FT_PSK:
		case AUTH_MODE_OPEN:	/* Note: Omit break here. */
		case AUTH_MODE_WPA:
		case AUTH_MODE_WPA_PSK:
		case AUTH_MODE_WPA2:
		case AUTH_MODE_WPA2_PSK:
		case AUTH_MODE_WPA_OSEN:
		case AUTH_MODE_WPA3_OWE:
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_OPEN_SYSTEM;
			break;

		case AUTH_MODE_SHARED:
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_SHARED_KEY;
			break;

		case AUTH_MODE_AUTO_SWITCH:
			DBGLOG(AIS, LOUD, "JOIN INIT: eAuthMode == AUTH_MODE_AUTO_SWITCH\n");
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) (AUTH_TYPE_OPEN_SYSTEM | AUTH_TYPE_SHARED_KEY);
			break;

		case AUTH_MODE_WPA3_SAE:
			DBGLOG(AIS, LOUD,
			       "JOIN INIT: eAuthMode == AUTH_MODE_SAE\n");
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_SAE;
			break;

		default:
			ASSERT(!(prConnSettings->eAuthMode == AUTH_MODE_WPA_NONE));
			DBGLOG(AIS, ERROR, "JOIN INIT: Auth Algorithm : %d was not supported by JOIN\n",
					    prConnSettings->eAuthMode);
			/* TODO(Kevin): error handling ? */
			return;
		}

		/* TODO(tyhsu): Assume that Roaming Auth Type is equal to ConnSettings eAuthMode */
		prAisSpecificBssInfo->ucRoamingAuthTypes = prAisFsmInfo->ucAvailableAuthTypes;

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;
		/* reset Bss Transition Management Params when do first connection */
		aisResetBssTranstionMgtParam(prAisSpecificBssInfo);
	} else {
		ASSERT(prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE);
		ASSERT(!prBssDesc->fgIsConnected);

		DBGLOG(AIS, LOUD, "JOIN INIT: AUTH TYPE = %d for Roaming\n",
				   prAisSpecificBssInfo->ucRoamingAuthTypes);

		prStaRec->fgIsReAssoc = TRUE;	/* We do roaming while the medium is connected */

		/* TODO(Kevin): We may call a sub function to acquire the Roaming Auth Type */
		/* FT and FT Resource Request Protocol should use FT AA(Auth Algorithm) */
		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_WPA2_FT:
		case AUTH_MODE_WPA2_FT_PSK:
		case AUTH_MODE_NON_RSN_FT:
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_FAST_BSS_TRANSITION;
			break;
		case AUTH_MODE_WPA3_SAE:
#if (CFG_REFACTORY_PMKSA == 0)
			if (rsnSearchPmkidEntry(prAdapter, prBssDesc->aucBSSID,
						&u4Entry)) {
#else
			if (rsnSearchPmkidEntry(prAdapter, prBssDesc->aucBSSID)) {
#endif
				prAisFsmInfo->ucAvailableAuthTypes =
					(uint8_t) AUTH_TYPE_OPEN_SYSTEM;
				DBGLOG(AIS, INFO,
					"SAE: change AUTH to OPEN when roaming with PMK\n");
			} else {
				prAisFsmInfo->ucAvailableAuthTypes =
					(uint8_t) AUTH_TYPE_SAE;
			}
			break;
		default:
			prAisFsmInfo->ucAvailableAuthTypes =
			    prAisSpecificBssInfo->ucRoamingAuthTypes;
			break;
		}

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT_FOR_ROAMING;
	}

	/* 4 <4> Use an appropriate Authentication Algorithm Number among the ucAvailableAuthTypes */
	if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_SHARED_KEY) {

		DBGLOG(AIS, LOUD, "JOIN INIT: Try to do Authentication with AuthType == SHARED_KEY.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SHARED_KEY;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, LOUD, "JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n");
		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_FAST_BSS_TRANSITION) {

		DBGLOG(AIS, LOUD, "JOIN INIT: Try to do Authentication with AuthType == FAST_BSS_TRANSITION.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_FAST_BSS_TRANSITION;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
		   AUTH_TYPE_SAE) {
		DBGLOG(AIS, LOUD,
		       "JOIN INIT: Try to do Authentication with AuthType == SAE.\n");
		/*the following one line used to fix WPA3-SAE Certification 5.2.4*/
		/*when finish test 5.2.3, the aisFsmState will always in JOIN state which make*/
		/*5.2.4 fail to connect AP */
		/*the following one line can let the aisFsmState will back to IDLE after 5.2.3*/
		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SAE;
		prStaRec->ucAuthAlgNum = (uint8_t) AUTH_ALGORITHM_NUM_SAE;
	} else {
		DBGLOG(AIS, ERROR,
		       "JOIN INIT: No available AuthType found, %d\n",
		       prAisFsmInfo->ucAvailableAuthTypes);
	}

	/* 4 <5> Overwrite Connection Setting for eConnectionPolicy == ANY (Used by Assoc Req) */
	if (prBssDesc->ucSSIDLen)
		COPY_SSID(prConnSettings->aucSSID, prConnSettings->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	/* 4 <6> Send a Msg to trigger SAA to start JOIN process. */
	prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	if (1) {
		int j;
		P_FRAG_INFO_T prFragInfo;

		for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
			prFragInfo = &prStaRec->rFragInfo[j];

			if (prFragInfo->pr1stFrag) {
				/* nicRxReturnRFB(prAdapter, prFragInfo->pr1stFrag); */
				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
			}
		}
	}
#if CFG_SUPPORT_802_11K
	if (prBssDesc->cPowerLimit != RLM_INVALID_POWER_LIMIT)
		rlmSetMaxTxPwrLimit(prAdapter, prBssDesc->cPowerLimit, 1);
#endif
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

}				/* end of aisFsmInit_JOIN() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Retry JOIN for AUTH_MODE_AUTO_SWITCH
*
* @param[in] prStaRec       Pointer to the STA_RECORD_T
*
* @retval TRUE      We will retry JOIN
* @retval FALSE     We will not retry JOIN
*/
/*----------------------------------------------------------------------------*/
BOOLEAN aisFsmStateInit_RetryJOIN(IN P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_JOIN_REQ_T prJoinReqMsg;
	INT_32 rssi = 0;

	DEBUGFUNC("aisFsmStateInit_RetryJOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	rssi = RCPI_TO_dBm(prStaRec->ucRCPI);
	/*check if driver exists pending scan reques and candiate bss's signal is too weak.*/
	if ((aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == TRUE)
		&& (rssi < AIS_DEFAULT_ROAMING_THRESHOLD)) {
		DBGLOG(AIS, WARN
			, "Retry BSS :[%pM] Rss:%d is too weak!Skipping retry join and doing next scan!\n"
			, prStaRec->aucMacAddr, rssi);
		return FALSE;
	}

	DBGLOG(AIS, INFO, "AvailableAuthTypes = %d\n", prAisFsmInfo->ucAvailableAuthTypes);

	/* Retry other AuthType if possible */
	if (!prAisFsmInfo->ucAvailableAuthTypes)
		return FALSE;

	if (prStaRec->u2StatusCode != STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED) {
		prAisFsmInfo->ucAvailableAuthTypes = 0;
		return FALSE;
	}

	if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, INFO, "RETRY JOIN INIT: Retry Authentication with AuthType == OPEN SYSTEM.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else {
		DBGLOG(AIS, ERROR, "RETRY JOIN INIT: Retry Authentication with Unexpected AuthType.\n");
		ASSERT(0);
	}

	prAisFsmInfo->ucAvailableAuthTypes = 0;	/* No more available Auth Types */

	/* Trigger SAA to start JOIN process. */
	prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return FALSE;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

	return TRUE;

}				/* end of aisFsmRetryJOIN() */

#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief State Initialization of AIS_STATE_IBSS_ALONE
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateInit_IBSS_ALONE(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	/* 4 <1> Check if IBSS was created before ? */
	if (prAisBssInfo->fgIsBeaconActivated) {

		/* 4 <2> Start IBSS Alone Timer for periodic SCAN and then SEARCH */
#if !CFG_SLT_SUPPORT
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer, SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));
#endif
	}

	aisFsmCreateIBSS(prAdapter);

}				/* end of aisFsmStateInit_IBSS_ALONE() */

/*----------------------------------------------------------------------------*/
/*!
* @brief State Initialization of AIS_STATE_IBSS_MERGE
*
* @param[in] prBssDesc  The pointer of BSS_DESC_T which is the IBSS we will try to merge with.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateInit_IBSS_MERGE(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	ASSERT(prBssDesc);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	/* 4 <1> We will merge with to this BSS immediately. */
	prBssDesc->fgIsConnecting = FALSE;
	prBssDesc->fgIsConnected = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter, STA_TYPE_ADHOC_PEER, NETWORK_TYPE_AIS_INDEX, prBssDesc);
	if (prStaRec == NULL) {
		DBGLOG(AIS, WARN, "Create station record fail\n");
		return;
	}

	prStaRec->fgIsMerging = TRUE;

	prAisFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <2.1> sync. to firmware domain */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* 4 <3> IBSS-Merge */
	aisFsmMergeIBSS(prAdapter, prStaRec);

}				/* end of aisFsmStateInit_IBSS_MERGE() */

#endif /* CFG_SUPPORT_ADHOC */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process of JOIN Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_JOIN(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_JOIN_ABORT_T prJoinAbortMsg;
	P_AIS_BSS_INFO_T prAisBSSInfo;

	prAisBSSInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* 1. Abort JOIN process */
	prJoinAbortMsg = (P_MSG_JOIN_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_ABORT_T));
	if (!prJoinAbortMsg) {

		ASSERT(0);	/* Can't abort SAA FSM */
		return;
	}

	prJoinAbortMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_ABORT;
	prJoinAbortMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinAbortMsg->prStaRec = prAisFsmInfo->prTargetStaRec;

	scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisFsmInfo->prTargetStaRec->aucMacAddr);

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinAbortMsg, MSG_SEND_METHOD_BUF);

	/* 2. Return channel privilege */
	aisFsmReleaseCh(prAdapter);

	/* 3.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	/* 3.2 reset local variable */
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
}				/* end of aisFsmAbortJOIN() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process of SCAN Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_SCAN(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_SCN_SCAN_CANCEL prScanCancelMsg;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* Abort JOIN process. */
	prScanCancelMsg = (P_MSG_SCN_SCAN_CANCEL) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_CANCEL));
	if (!prScanCancelMsg) {

		ASSERT(0);	/* Can't abort SCN FSM */
		return;
	}

	prScanCancelMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_CANCEL;
	prScanCancelMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfScanReq;
	prScanCancelMsg->ucNetTypeIndex = (UINT_8) NETWORK_TYPE_AIS_INDEX;
#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered)
		prScanCancelMsg->fgIsChannelExt = FALSE;
#endif

	/* unbuffered message to guarantee scan is cancelled in sequence */
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanCancelMsg, MSG_SEND_METHOD_UNBUF);

}				/* end of aisFsmAbortSCAN() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process of NORMAL_TR Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_NORMAL_TR(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_BSS_INFO_T prAisBssInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	DBGLOG(AIS, TRACE, "aisFsmStateAbort_NORMAL_TR\n");

	/* TODO(Kevin): Do abort other MGMT func */

	/* 1. Release channel to CNM */
	aisFsmReleaseCh(prAdapter);

	/* 2.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	/* 2.2 reset local variable */
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
	/* 2.3 need check ReasonOfDisconnect */

	if (prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST)
		/*Beacon timeout and driver will try to connect to this the same SSID but different BSSID*/
		DBGLOG(AIS, INFO, "Radio lost don't clear SSID len!\n");
	else
		prAdapter->rWifiVar.rConnSettings.ucSSIDLen = 0;

}				/* end of aisFsmAbortNORMAL_TR() */

#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief Process of NORMAL_TR Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_IBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_DESC_T prBssDesc;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* reset BSS-DESC */
	if (prAisFsmInfo->prTargetStaRec) {
		prBssDesc = scanSearchBssDescByTA(prAdapter, prAisFsmInfo->prTargetStaRec->aucMacAddr);

		if (prBssDesc) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;
		}
	}
	/* release channel privilege */
	aisFsmReleaseCh(prAdapter);

}
#endif /* CFG_SUPPORT_ADHOC */

/*----------------------------------------------------------------------------*/
/*!
* @brief Change array index to channel number.
*
* @param[in] ucIndex            array index.
*
* @retval ucChannelNum           ucChannelNum
*/
/*----------------------------------------------------------------------------*/
UINT_8 aisIndex2ChannelNum(IN UINT_8 ucIndex)
{
	UINT_8 ucChannel;

	/*Full2Partial*/
	if (ucIndex >= 1 && ucIndex <= 14)
		/*1---14*/
		ucChannel = ucIndex;
		/*1---14*/
	else if (ucIndex >= 15 && ucIndex <= 22)
		/*15---22*/
		ucChannel = (ucIndex - 6) << 2;
		/*36---64*/
	else if (ucIndex >= 23 && ucIndex <= 34)
		/*23---34*/
		ucChannel = (ucIndex + 2) << 2;
		/*100---144*/
	else if (ucIndex >= 35 && ucIndex <= 39) {
		/*35---39*/
		ucIndex = ucIndex + 2;
		ucChannel = (ucIndex << 2) + 1;
		/*149---164*/
	} else {
		/*error*/
		ucChannel = 0;
	}
	return ucChannel;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Full2Partial Process full scan channel info
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

VOID aisGetAndSetScanChannel(IN P_ADAPTER_T prAdapter)
{
	P_PARTIAL_SCAN_INFO PartialScanChannel = NULL;
	UINT_8	*ucChannelp;
	UINT_8	ucChannelNum;
	int i = 1;
	int t = 0;
	/*Full2Partial*/

	if (prAdapter->prGlueInfo->u4LastFullScanTime == 0) {
		/*there is full scan before this time*/
		DBGLOG(AIS, INFO, "Full2Partial u4LastFullScanTime=0\n");
		return;
	}
	/*
	*if (prAdapter->prGlueInfo->ucChannelListNum == 0) {
	*	DBGLOG(AIS, TRACE, "aisGetAndSetScanChannel ucChannelListNum=0\n");
	*	return;
	*}
	*/
	if (prAdapter->prGlueInfo->puFullScan2PartialChannel != NULL) {
		DBGLOG(AIS, TRACE, "Full2Partial puFullScan2PartialChannel not null\n");
		return;
	}

	/*at here set channel info*/
	PartialScanChannel = (P_PARTIAL_SCAN_INFO) kalMemAlloc(sizeof(PARTIAL_SCAN_INFO), VIR_MEM_TYPE);
	if (PartialScanChannel == NULL) {
		DBGLOG(AIS, INFO, "Full2Partial alloc PartialScanChannel fail\n");
		return;
	}
	kalMemSet(PartialScanChannel, 0, sizeof(PARTIAL_SCAN_INFO));

	ucChannelp = prAdapter->prGlueInfo->ucChannelNum;
	while (i < FULL_SCAN_MAX_CHANNEL_NUM) {
		if (ucChannelp[i] != 0) {
			ucChannelNum = aisIndex2ChannelNum(i);
			DBGLOG(AIS, TRACE, "Full2Partial i=%d, channel value=%d\n", i, ucChannelNum);
			if (ucChannelNum != 0) {
				if ((ucChannelNum >= 1) && (ucChannelNum <= 14))
					PartialScanChannel->arChnlInfoList[t].eBand = BAND_2G4;
				else
					PartialScanChannel->arChnlInfoList[t].eBand = BAND_5G;

				PartialScanChannel->arChnlInfoList[t].ucChannelNum = ucChannelNum;
				t++;
			}
		}
		i++;
	}
	DBGLOG(AIS, INFO, "Full2Partial channel num=%d\n", t);
	if ((t > 0) && (t <= MAXIMUM_OPERATION_CHANNEL_LIST)) {
		PartialScanChannel->ucChannelListNum = t;
		prAdapter->prGlueInfo->puFullScan2PartialChannel = (PUINT_8)PartialScanChannel;
	} else {
		DBGLOG(AIS, INFO, "Full2Partial channel num great %d max channel number\n",
			MAXIMUM_OPERATION_CHANNEL_LIST);
		kalMemFree(PartialScanChannel, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief The Core FSM engine of AIS(Ad-hoc, Infra STA)
*
* @param[in] eNextState Enum value of next AIS STATE
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmSteps(IN P_ADAPTER_T prAdapter, ENUM_AIS_STATE_T eNextState)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc;
	P_MSG_CH_REQ_T prMsgChReq;
#if CFG_MULTI_SSID_SCAN
	P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg;
#else
	P_MSG_SCN_SCAN_REQ prScanReqMsg;
#endif
	P_AIS_REQ_HDR_T prAisReq;
	P_SCAN_INFO_T prScanInfo;
	ENUM_BAND_T eBand;
	UINT_8 ucChannel;
	UINT_16 u2ScanIELen;
	ENUM_AIS_STATE_T eOriPreState;
	OS_SYSTIME rCurrentTime;

	BOOLEAN fgIsTransition = (BOOLEAN) FALSE;
	BOOLEAN fgIsRequestScanPending = (BOOLEAN) FALSE;

	DEBUGFUNC("aisFsmSteps()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	eOriPreState = prAisFsmInfo->ePreviousState;

	do {

		/* Do entering Next State */
		prAisFsmInfo->ePreviousState = prAisFsmInfo->eCurrentState;

#if DBG
		DBGLOG(AIS, STATE, "TRANSITION: [%s] -> [%s]\n",
				    apucDebugAisState[prAisFsmInfo->eCurrentState], apucDebugAisState[eNextState]);
#else
		DBGLOG(AIS, STATE, "[%d] TRANSITION: [%d] -> [%d]\n",
				    DBG_AIS_IDX, prAisFsmInfo->eCurrentState, eNextState);
#endif
		/* NOTE(Kevin): This is the only place to change the eCurrentState(except initial) */
		prAisFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (BOOLEAN) FALSE;

		aisPostponedEventOfDisconnTimeout(prAdapter, prAisFsmInfo);

		/* Do tasks of the State that we just entered */
		switch (prAisFsmInfo->eCurrentState) {
			/* NOTE(Kevin): we don't have to rearrange the sequence of following
			 * switch case. Instead I would like to use a common lookup table of array
			 * of function pointer to speed up state search.
			 */
		case AIS_STATE_IDLE:

			prAisReq = aisFsmGetNextRequest(prAdapter);

			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer);

			if (prAisReq)
				DBGLOG(AIS, TRACE, "eReqType=%d, fgIsConnReqIssued=%d, DisByNonRequest=%d\n",
				prAisReq->eReqType, prConnSettings->fgIsConnReqIssued,
				prConnSettings->fgIsDisconnectedByNonRequest);
			if (prAisReq == NULL || prAisReq->eReqType == AIS_REQUEST_RECONNECT) {
				if (IS_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX)) {
#if !CFG_SUPPORT_RLM_ACT_NETWORK
					UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);
					nicDeactivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX);
#else
					rlmDeactivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX,
						NET_ACTIVE_SRC_CONNECT | NET_ACTIVE_SRC_SCAN);
#endif
				}
				if (prConnSettings->fgIsConnReqIssued == TRUE &&
				    prConnSettings->fgIsDisconnectedByNonRequest == FALSE) {

					prAisFsmInfo->fgTryScan = TRUE;

					SET_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);

					/* sync with firmware */
#if !CFG_SUPPORT_RLM_ACT_NETWORK
					SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);
					nicActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX);
#else
					rlmActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_CONNECT);
#endif
					/* reset trial count */
					prAisFsmInfo->ucConnTrialCount = 0;

					eNextState = AIS_STATE_COLLECT_ESS_INFO;
					fgIsTransition = TRUE;
				} else {
					SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_AIS_INDEX);

					/* check for other pending request */

					fgIsRequestScanPending =
						aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE);

					if (prAisReq && fgIsRequestScanPending == TRUE) {

						wlanClearScanningResult(prAdapter);
						eNextState = AIS_STATE_SCAN;

						fgIsTransition = TRUE;
					}
					/*check for pending sched scan request*/
					if (prAisReq == NULL &&
						fgIsRequestScanPending == FALSE &&
						prScanInfo->fgIsPostponeSchedScan == TRUE)
						aisPostponedEventOfSchedScanReq(prAdapter, prAisFsmInfo);

#if CFG_SCAN_ABORT_HANDLE
					if (prAisFsmInfo->fgIsAbortEvnetDuringScan) {
						DBGLOG(AIS, WARN, "proccess the pending abort event(%d)!\n"
							, prAisBssInfo->ucReasonOfDisconnect);
						prAisFsmInfo->fgIsAbortEvnetDuringScan = FALSE;
						aisFsmStateAbort(prAdapter,
								prAisBssInfo->ucReasonOfDisconnect,
								prAisBssInfo->fgIsDelayIndication);
					}
#endif
				}
				if (prAisReq) {
					/* free the message */
					cnmMemFree(prAdapter, prAisReq);
				}
			} else if (prAisReq->eReqType == AIS_REQUEST_SCAN) {
#if CFG_SUPPORT_ROAMING
				prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif /* CFG_SUPPORT_ROAMING */
				wlanClearScanningResult(prAdapter);

				eNextState = AIS_STATE_SCAN;
				fgIsTransition = TRUE;

				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType == AIS_REQUEST_ROAMING_CONNECT
				   || prAisReq->eReqType == AIS_REQUEST_ROAMING_SEARCH) {
				/* ignore */
				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType == AIS_REQUEST_REMAIN_ON_CHANNEL) {
				eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
				fgIsTransition = TRUE;

				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			}

			prAisFsmInfo->u4SleepInterval = AIS_BG_SCAN_INTERVAL_MIN_SEC;

			break;

		case AIS_STATE_SEARCH:
			/* 4 <1> Search for a matched candidate and save it to prTargetBssDesc. */
#if CFG_SLT_SUPPORT
			prBssDesc = prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
			if (prAisFsmInfo->ucJoinFailCntAfterScan >= 4) {
				prBssDesc = NULL;
				DBGLOG(AIS, STATE,
					"Failed to connect %s more than 4 times after last scan, scan again\n",
					prConnSettings->aucSSID);
			} else {
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
				prBssDesc = scanSearchBssDescByScoreForAis(prAdapter);
#else
				prBssDesc = scanSearchBssDescByPolicy(prAdapter, NETWORK_TYPE_AIS_INDEX);
#endif
			}
#endif
			/* we are under Roaming Condition. */
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				if (prAisFsmInfo->ucConnTrialCount > AIS_ROAMING_CONNECTION_TRIAL_LIMIT) {
#if CFG_SUPPORT_ROAMING
					DBGLOG(AIS, STATE,
						"Roaming retry count :%d fail!\n", prAisFsmInfo->ucConnTrialCount);
					roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_CONNLIMIT);
#endif /* CFG_SUPPORT_ROAMING */
					/* reset retry count */
					prAisFsmInfo->ucConnTrialCount = 0;

					/* abort connection trial */
					if (prConnSettings->eReConnectLevel < RECONNECT_LEVEL_BEACON_TIMEOUT) {
						prConnSettings->eReConnectLevel = RECONNECT_LEVEL_ROAMING_FAIL;
						prConnSettings->fgIsConnReqIssued = FALSE;
					} else {
						DBGLOG(AIS, INFO,
						       "Do not set fgIsConnReqIssued, Level is %d\n",
						       prConnSettings->eReConnectLevel);
					}

					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;

					break;
				}
			}
			/* 4 <2> We are not under Roaming Condition. */
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

				/* 4 <2.a> If we have the matched one */
				if (prBssDesc) {

					/*
					 * 4 <A> Stored the Selected BSS security cipher.
					 * For later asoc req compose IE
					 */
					prAisBssInfo->u4RsnSelectedGroupCipher = prBssDesc->u4RsnSelectedGroupCipher;
					prAisBssInfo->u4RsnSelectedPairwiseCipher =
					    prBssDesc->u4RsnSelectedPairwiseCipher;
					prAisBssInfo->u4RsnSelectedAKMSuite = prBssDesc->u4RsnSelectedAKMSuite;

					/* 4 <B> Do STATE transition and update current Operation Mode. */
					if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;

						/* Record the target BSS_DESC_T for next STATE. */
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						/* Transit to channel acquire */
						eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
						fgIsTransition = TRUE;

						/* increase connection trial count */
						prAisFsmInfo->ucConnTrialCount++;
					}
#if CFG_SUPPORT_ADHOC
					else if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

						/* Record the target BSS_DESC_T for next STATE. */
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						eNextState = AIS_STATE_IBSS_MERGE;
						fgIsTransition = TRUE;
					}
#endif /* CFG_SUPPORT_ADHOC */
					else {
						ASSERT(0);
						eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
						fgIsTransition = TRUE;
					}
				}
				/* 4 <2.b> If we don't have the matched one */
				else {
#if CFG_SUPPORT_RN
					if (prAisBssInfo->fgDisConnReassoc == TRUE) {
						/* abort connection trial */
						prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
						prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
						prAisBssInfo->fgDisConnReassoc = FALSE;
						aisIndicationOfMediaStateToHost(prAdapter,
								PARAM_MEDIA_STATE_DISCONNECTED, FALSE);
						eNextState = AIS_STATE_IDLE;
						fgIsTransition = TRUE;
						break;
					}
#endif /* CFG_SUPPORT_RN */
					if (prAisFsmInfo->rJoinReqTime != 0 &&
							CHECK_FOR_TIMEOUT(kalGetTimeTick(),
							prAisFsmInfo->rJoinReqTime,
							SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
						UINT_16 u2StaTusCode = STATUS_CODE_JOIN_TIMEOUT;

						/* abort connection trial */
						prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
						prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
						kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
							WLAN_STATUS_JOIN_FAILURE,
							(PVOID) & u2StaTusCode, sizeof(u2StaTusCode));

						eNextState = AIS_STATE_IDLE;
						fgIsTransition = TRUE;
						break;
					}
					/* increase connection trial count for infrastructure connection */
					if (prConnSettings->eOPMode == NET_TYPE_INFRA)
						prAisFsmInfo->ucConnTrialCount++;

					/* if alway can't find traget bss, during new connect */
					GET_CURRENT_SYSTIME(&rCurrentTime);
					if ((prAisBssInfo->ucReasonOfDisconnect ==
						    DISCONNECT_REASON_CODE_NEW_CONNECTION) &&
						prAisFsmInfo->rJoinReqTime != 0 &&
					   CHECK_FOR_TIMEOUT(rCurrentTime,
							     prAisFsmInfo->rJoinReqTime,
							     SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
						/* abort connection trial */
						prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
						prAdapter->rWifiVar.rConnSettings.eReConnectLevel
							= RECONNECT_LEVEL_MIN;

						DBGLOG(AIS, WARN,
							"Target BSS is NULL ,timeout and report disconnect!\n");
						kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
									WLAN_STATUS_JOIN_FAILURE, NULL, 0);
						eNextState = AIS_STATE_IDLE;
						fgIsTransition = TRUE;
						prAisFsmInfo->rJoinReqTime = 0;
						break;
					}

					/* 4 <A> Try to SCAN */
					if (prAisFsmInfo->fgTryScan) {
						eNextState = AIS_STATE_LOOKING_FOR;

						fgIsTransition = TRUE;
						break;
					}
					/* 4 <B> We've do SCAN already, now wait in some STATE. */
					if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

						/*
						 * issue reconnect request,
						 * and retreat to idle state for scheduling
						 */
						aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

						eNextState = AIS_STATE_IDLE;
						fgIsTransition = TRUE;
					}
#if CFG_SUPPORT_ADHOC
					else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
						 || (prConnSettings->eOPMode == NET_TYPE_AUTO_SWITCH)
						 || (prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS)) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
						prAisFsmInfo->prTargetBssDesc = NULL;

						eNextState = AIS_STATE_IBSS_ALONE;
						fgIsTransition = TRUE;
					}
#endif /* CFG_SUPPORT_ADHOC */
					else {
						ASSERT(0);
						eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
						fgIsTransition = TRUE;
					}
				}
			}
			/* 4 <3> We are under Roaming Condition. */
			else {	/* prAdapter->eConnectionState == MEDIA_STATE_CONNECTED. */

				/* 4 <3.a> This BSS_DESC_T is our AP. */
				/* NOTE(Kevin 2008/05/16): Following cases will go back to NORMAL_TR.
				 * CASE I: During Roaming, APP(WZC/NDISTEST) change the connection
				 *         settings. That make we can NOT match the original AP, so the
				 *         prBssDesc is NULL.
				 * CASE II: The same reason as CASE I. Because APP change the
				 *          eOPMode to other network type in connection setting
				 *          (e.g. NET_TYPE_IBSS), so the BssDesc become the IBSS node.
				 * (For CASE I/II, before WZC/NDISTEST set the OID_SSID, it will change
				 * other parameters in connection setting first. So if we do roaming
				 * at the same time, it will hit these cases.)
				 *
				 * CASE III: Normal case, we can't find other candidate to roam
				 * out, so only the current AP will be matched.
				 *
				 * CASE IV: Timestamp of the current AP might be reset
				 */
				if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION &&
				    ((!prBssDesc) ||	/* CASE I */
				    (prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE) ||	/* CASE II */
				    (prBssDesc->fgIsConnected) ||	/* CASE III */
				    (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID))) /* CASE IV */) {
#if DBG
					if ((prBssDesc) && (prBssDesc->fgIsConnected))
						ASSERT(EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID));
#endif /* DBG */
					if (prAisFsmInfo->fgTargetChnlScanIssued) {
						/* if target channel scan has issued,
						** and no roaming target is found, need to do full scan again
						*/
						DBGLOG(AIS, INFO,
							"[Roaming] No target found, try to full scan again\n");
						prAisFsmInfo->fgTargetChnlScanIssued = FALSE;
						eNextState = AIS_STATE_LOOKING_FOR;
						fgIsTransition = TRUE;
						break;
					}
					/* We already associated with it, go back to NORMAL_TR */
					/* TODO(Kevin): Roaming Fail */
#if CFG_SUPPORT_ROAMING
					roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_NOCANDIDATE);
#endif /* CFG_SUPPORT_ROAMING */

					/* Retreat to NORMAL_TR state */
					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;
					break;
				}

				/* 4 <3.b> Try to roam out for JOIN this BSS_DESC_T. */
				if (prBssDesc == NULL) {
					/* increase connection trial count for infrastructure connection */
					if (prConnSettings->eOPMode == NET_TYPE_INFRA)
						prAisFsmInfo->ucConnTrialCount++;
					/* 4 <A> Try to SCAN */
					if (prAisFsmInfo->fgTryScan) {
						eNextState = AIS_STATE_LOOKING_FOR;

						fgIsTransition = TRUE;
						break;
					}

					/* 4 <B> We've do SCAN already, now wait in some STATE. */
					if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

						/*
						 * issue reconnect request, and retreat to idle state
						 * for scheduling
						 */
						aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

						eNextState = AIS_STATE_IDLE;
						fgIsTransition = TRUE;
					}
#if CFG_SUPPORT_ADHOC
					else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
						 || (prConnSettings->eOPMode == NET_TYPE_AUTO_SWITCH)
						 || (prConnSettings->eOPMode ==
						     NET_TYPE_DEDICATED_IBSS)) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
						prAisFsmInfo->prTargetBssDesc = NULL;

						eNextState = AIS_STATE_IBSS_ALONE;
						fgIsTransition = TRUE;
					}
#endif /* CFG_SUPPORT_ADHOC */
					else {
						ASSERT(0);
						eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
						fgIsTransition = TRUE;
					}
				} else {
#if DBG
					if (prAisBssInfo->ucReasonOfDisconnect !=
					    DISCONNECT_REASON_CODE_REASSOCIATION) {
						ASSERT(UNEQUAL_MAC_ADDR
						       (prBssDesc->aucBSSID, prAisBssInfo->aucBSSID));
					}
#endif /* DBG */

					/* 4 <A> Record the target BSS_DESC_T for next STATE. */
					prAisFsmInfo->prTargetBssDesc = prBssDesc;

					/* tyhsu: increase connection trial count */
					prAisFsmInfo->ucConnTrialCount++;
					/* stop Tx due to we need to connect a new AP. even the new
					* AP is operating on the same channel with current, we still
					* need to stop Tx, because firmware should ensure all mgmt and
					* dhcp packets are Tx in time, and may cause normal data
					* packets was queued and eventually flushed in firmware
					*/
					if (prAisBssInfo->prStaRecOfAP &&
						prAisBssInfo->ucReasonOfDisconnect !=
						DISCONNECT_REASON_CODE_REASSOCIATION)
						prAisBssInfo->prStaRecOfAP->fgIsTxAllowed = FALSE;
					/* Transit to channel acquire */
					eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
					/* Find target AP to roaming and set fgTargetChnlScanIssued to false */
					if (prAisFsmInfo->fgTargetChnlScanIssued)
						prAisFsmInfo->fgTargetChnlScanIssued = FALSE;
					fgIsTransition = TRUE;
				}
			}
#if (CFG_REFACTORY_PMKSA == 0)
			if (prBssDesc && prConnSettings->fgOkcEnabled) {
				UINT_8 aucBuf[sizeof(PARAM_PMKID_CANDIDATE_LIST_T) + sizeof(PARAM_STATUS_INDICATION_T)];
				P_PARAM_STATUS_INDICATION_T prStatusEvent = (P_PARAM_STATUS_INDICATION_T)aucBuf;
				P_PARAM_PMKID_CANDIDATE_LIST_T prPmkidCandicate =
					(P_PARAM_PMKID_CANDIDATE_LIST_T)(prStatusEvent+1);
				UINT_32 u4Entry = 0;

				if (rsnSearchPmkidEntry(prAdapter, prBssDesc->aucBSSID, &u4Entry) &&
					prAdapter->rWifiVar.rAisSpecificBssInfo.arPmkidCache[u4Entry].fgPmkidExist)
					break;
				DBGLOG(AIS, INFO, "No PMK for %pM, try to generate a OKC PMK\n", prBssDesc->aucBSSID);
				prStatusEvent->eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;
				prPmkidCandicate->u4Version = 1;
				prPmkidCandicate->u4NumCandidates = 1;
				prPmkidCandicate->arCandidateList[0].u4Flags = 0; /* don't request preauth */
				COPY_MAC_ADDR(prPmkidCandicate->arCandidateList[0].arBSSID, prBssDesc->aucBSSID);
				kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					 WLAN_STATUS_MEDIA_SPECIFIC_INDICATION, (PVOID)aucBuf, sizeof(aucBuf));
				cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer, AIS_WAIT_OKC_PMKID_SEC);
			}
#endif
			break;

		case AIS_STATE_WAIT_FOR_NEXT_SCAN:

			DBGLOG(AIS, LOUD, "SCAN: Idle Begin - Current Time = %u\n", kalGetTimeTick());

			cnmTimerStartTimer(prAdapter,
					   &prAisFsmInfo->rBGScanTimer, SEC_TO_MSEC(prAisFsmInfo->u4SleepInterval));

			SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_AIS_INDEX);

			if (prAisFsmInfo->u4SleepInterval < AIS_BG_SCAN_INTERVAL_MAX_SEC)
				prAisFsmInfo->u4SleepInterval <<= 1;
			break;

		case AIS_STATE_SCAN:
		case AIS_STATE_ONLINE_SCAN:
		case AIS_STATE_LOOKING_FOR:

#if !CFG_SUPPORT_RLM_ACT_NETWORK
			if (!IS_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX)) {
				SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);

				/* sync with firmware */
				nicActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX);
			}
#else
			rlmActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_SCAN);
#endif
			/* IE length decision */
			if (prAisFsmInfo->u4ScanIELength > 0) {
				u2ScanIELen = (UINT_16) prAisFsmInfo->u4ScanIELength;
			} else {
#if CFG_SUPPORT_WPS2
				u2ScanIELen = prAdapter->prGlueInfo->u2WSCIELen;
#else
				u2ScanIELen = 0;
#endif
			}

#if CFG_MULTI_SSID_SCAN
			if (rlmBcnRmRunning(prAdapter)) {
				P_MSG_SCN_SCAN_REQ prRmReqMsg =
					(P_MSG_SCN_SCAN_REQ) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					OFFSET_OF(MSG_SCN_SCAN_REQ, aucIE) + u2ScanIELen);

				if (!prRmReqMsg) {
					ASSERT(0);	/* Can't trigger SCAN FSM */
					return;
				}

				prRmReqMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_REQ;
				prRmReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfScanReq;
				prRmReqMsg->ucNetTypeIndex = (UINT_8) NETWORK_TYPE_AIS_INDEX;
				rlmFillScanMsg(prAdapter, prRmReqMsg);
				mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prRmReqMsg, MSG_SEND_METHOD_BUF);
				break;
			}

			prScanReqMsg = (P_MSG_SCN_SCAN_REQ_V2) cnmMemAlloc(prAdapter,
									RAM_TYPE_MSG,
									OFFSET_OF(MSG_SCN_SCAN_REQ_V2,
										  aucIE) + u2ScanIELen);
			if (!prScanReqMsg) {
				ASSERT(0);	/* Can't trigger SCAN FSM */
				return;
			}

			prScanReqMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_REQ_V2;
			prScanReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfScanReq;
			prScanReqMsg->ucNetTypeIndex = (UINT_8) NETWORK_TYPE_AIS_INDEX;
#else
			prScanReqMsg = (P_MSG_SCN_SCAN_REQ) cnmMemAlloc(prAdapter,
									RAM_TYPE_MSG,
									OFFSET_OF(MSG_SCN_SCAN_REQ,
										  aucIE) + u2ScanIELen);
			if (!prScanReqMsg) {
				ASSERT(0);	/* Can't trigger SCAN FSM */
				return;
			}

			prScanReqMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_REQ;
			prScanReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfScanReq;
			prScanReqMsg->ucNetTypeIndex = (UINT_8) NETWORK_TYPE_AIS_INDEX;

			if (rlmFillScanMsg(prAdapter, prScanReqMsg)) {
				mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanReqMsg, MSG_SEND_METHOD_BUF);
				break;
			}
			prScanReqMsg->u2ChannelDwellTime = 0;
			prScanReqMsg->u2MinChannelDwellTime = 0;
			COPY_MAC_ADDR(prScanReqMsg->aucBSSID, "\xff\xff\xff\xff\xff\xff");
#endif

#if CFG_SUPPORT_RDD_TEST_MODE
			prScanReqMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;
#else
#ifdef CFG_TC1_FEATURE /* for Passive Scan */
			prScanReqMsg->eScanType = (ENUM_SCAN_TYPE_T)prAdapter->ucScanType;
#else
			prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
#endif
#endif

#if CFG_SUPPORT_ROAMING_ENC
			if (prAdapter->fgIsRoamingEncEnabled == TRUE) {
				if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR &&
				    prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
					prScanReqMsg->u2ChannelDwellTime = AIS_ROAMING_SCAN_CHANNEL_DWELL_TIME;
				}
			}
#endif /* CFG_SUPPORT_ROAMING_ENC */

#if CFG_MULTI_SSID_SCAN
			if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN
				|| prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
				if (prAisFsmInfo->ucScanSSIDNum == 0) {
					/* Scan for all available SSID */
					/* prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN; */
					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
				} else if (prAisFsmInfo->ucScanSSIDNum == 1 &&
						prAisFsmInfo->arScanSSID[0].u4SsidLen == 0) {
					/* prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN; */
					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
				} else {
					/* prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN; */
					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
					prScanReqMsg->ucSSIDNum = prAisFsmInfo->ucScanSSIDNum;
					prScanReqMsg->prSsid = prAisFsmInfo->arScanSSID;
				}
				kalMemCopy(prScanReqMsg->aucRandomMac,
					prAisFsmInfo->aucRandomMac, MAC_ADDR_LEN);
			} else {
				/* prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN; */

				COPY_SSID(prAisFsmInfo->rRoamingSSID.aucSsid,
							prAisFsmInfo->rRoamingSSID.u4SsidLen,
							prConnSettings->aucSSID,
							prConnSettings->ucSSIDLen);

				/* Scan for determined SSID */
				prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
				prScanReqMsg->ucSSIDNum = 1;
				prScanReqMsg->prSsid = &(prAisFsmInfo->rRoamingSSID);
			}

			/* using default channel dwell time/timeout value */
			prScanReqMsg->u2ProbeDelay = 0;
			prScanReqMsg->u2ChannelDwellTime = 0;
			prScanReqMsg->u2TimeoutValue = 0;
#else
			if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN
			    || prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
				if (prAisFsmInfo->ucScanSSIDLen == 0) {
					/* Scan for all available SSID */
					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
				} else {
					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
					COPY_SSID(prScanReqMsg->aucSSID,
						  prScanReqMsg->ucSSIDLength,
						  prAisFsmInfo->aucScanSSID, prAisFsmInfo->ucScanSSIDLen);
				}
			} else {
				/* Scan for determined SSID */
				prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
				COPY_SSID(prScanReqMsg->aucSSID,
					  prScanReqMsg->ucSSIDLength,
					  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);
			}
#endif
			/* check if tethering is running and need to fix on specific channel */
			if (cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel) == TRUE) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
				prScanReqMsg->ucChannelListNum = 1;
				prScanReqMsg->arChnlInfoList[0].eBand = eBand;
				prScanReqMsg->arChnlInfoList[0].ucChannelNum = ucChannel;
			} else if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR &&
					prAisFsmInfo->aucNeighborAPChnl[0] != 0) {
				PUINT_8 pucChnl = &prAisFsmInfo->aucNeighborAPChnl[0];
				P_RF_CHANNEL_INFO_T prChnlInfo = &prScanReqMsg->arChnlInfoList[0];
				UINT_8 ucChnlNum = 0;

				while (pucChnl[ucChnlNum] > 0 && ucChnlNum < MAXIMUM_OPERATION_CHANNEL_LIST) {
					prChnlInfo[ucChnlNum].ucChannelNum = pucChnl[ucChnlNum];
					prChnlInfo[ucChnlNum].eBand = pucChnl[ucChnlNum] > 14 ? BAND_5G:BAND_2G4;
					ucChnlNum++;
				}
				prScanReqMsg->ucChannelListNum = ucChnlNum;
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
				DBGLOG(AIS, INFO, "Looking %s in %d channels, first 5 channels %d %d %d %d %d\n",
					prConnSettings->aucSSID, ucChnlNum, pucChnl[0], pucChnl[1], pucChnl[2],
					pucChnl[3], pucChnl[4]);
			} else if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
					prAdapter->rWifiVar.rRoamingInfo.eCurrentState == ROAMING_STATE_DISCOVERY &&
					prAisFsmInfo->fgTargetChnlScanIssued) {
				P_RF_CHANNEL_INFO_T prChnlInfo = &prScanReqMsg->arChnlInfoList[0];
				UINT_8 ucChannelNum = 0;
				UINT_8 i = 0;

				for (i = 0; i < prAdapter->rWifiVar.rAisSpecificBssInfo.ucCurEssChnlInfoNum; i++) {
					ucChannelNum =
						prAdapter->rWifiVar.rAisSpecificBssInfo.arCurEssChnlInfo[i].ucChannel;
					if ((ucChannelNum >= 1) && (ucChannelNum <= 14))
						prChnlInfo[i].eBand = BAND_2G4;
					else
						prChnlInfo[i].eBand = BAND_5G;
					prChnlInfo[i].ucChannelNum = ucChannelNum;
				}
				prScanReqMsg->ucChannelListNum =
					prAdapter->rWifiVar.rAisSpecificBssInfo.ucCurEssChnlInfoNum;
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
				DBGLOG(AIS, INFO,
					"[Roaming] Target Scan: ucChannelListNum=%d\n", prScanReqMsg->ucChannelListNum);
			} else if ((prAdapter->prGlueInfo != NULL) &&
				(prAdapter->prGlueInfo->puScanChannel != NULL)) {
				/* handle partial scan channel info */
				P_PARTIAL_SCAN_INFO channel_t;
				UINT_32	u4size;

				channel_t = (P_PARTIAL_SCAN_INFO)prAdapter->prGlueInfo->puScanChannel;

				/* set partial scan */
				prScanReqMsg->ucChannelListNum = channel_t->ucChannelListNum;
				u4size = sizeof(channel_t->arChnlInfoList);

				DBGLOG(AIS, TRACE,
					"Partial Scan: ucChannelListNum=%d, total size=%d\n",
					prScanReqMsg->ucChannelListNum, u4size);

				kalMemCopy(&(prScanReqMsg->arChnlInfoList), &(channel_t->arChnlInfoList),
					u4size);

				/* clear prGlueInfo partial scan info */
				prAdapter->prGlueInfo->puScanChannel = NULL;
				kalMemFree(channel_t, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));

				/* set scan channel type for partial scan */
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
#if CFG_SUPPORT_NCHO
			} else if (prAdapter->rNchoInfo.fgECHOEnabled &&
				prAdapter->rNchoInfo.u4RoamScanControl == TRUE &&
				prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
				prAdapter->rWifiVar.rRoamingInfo.eCurrentState == ROAMING_STATE_DISCOVERY) {
				/* handle NCHO scan channel info */
				UINT_32	u4size = 0;
				PCFG_NCHO_SCAN_CHNL_T prRoamScnChnl = NULL;

				prRoamScnChnl = &prAdapter->rNchoInfo.rRoamScnChnl;
				/* set partial scan */
				prScanReqMsg->ucChannelListNum = prRoamScnChnl->ucChannelListNum;
				u4size = sizeof(prRoamScnChnl->arChnlInfoList);

				DBGLOG(AIS, TRACE,
					"NCHO SCAN channel num = %d, total size=%d\n",
					prScanReqMsg->ucChannelListNum, u4size);

				kalMemCopy(&(prScanReqMsg->arChnlInfoList), &(prRoamScnChnl->arChnlInfoList),
					u4size);

				/* set scan channel type for NCHO scan */
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
#endif
			} else {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
				ASSERT(0);
			}

			if (prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] == BAND_2G4)
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
			else if (prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX] == BAND_5G)
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;

			if (prAdapter->rWifiVar.rRoamingInfo.eCurrentState == ROAMING_STATE_DISCOVERY) {
				if (prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX] == BAND_2G4)
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
				else if (prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX] == BAND_5G)
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
			}

			DBGLOG(AIS, TRACE, "Full2Partial eScanChannel = %d, ucChannelListNum=%d\n",
				prScanReqMsg->eScanChannel, prScanReqMsg->ucChannelListNum);
			/*Full2Partial at here, chech sould update full scan to partial scan or not*/
			if ((prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN)
				&& (prScanReqMsg->eScanChannel == SCAN_CHANNEL_FULL
				|| prScanReqMsg->ucChannelListNum == 0)) {
				/*this is a full scan*/
				OS_SYSTIME rCurrentTime;
				P_PARTIAL_SCAN_INFO channel_t;
				P_GLUE_INFO_T pGlinfo;
				UINT_32 u4size;

				pGlinfo = prAdapter->prGlueInfo;
				GET_CURRENT_SYSTIME(&rCurrentTime);
				DBGLOG(AIS, TRACE, "Full2Partial LastFullST= %d,CurrentT=%d\n",
					pGlinfo->u4LastFullScanTime, rCurrentTime);
				if ((pGlinfo->u4LastFullScanTime == 0) ||
					(CHECK_FOR_TIMEOUT(rCurrentTime, pGlinfo->u4LastFullScanTime,
						SEC_TO_SYSTIME(UPDATE_FULL_TO_PARTIAL_SCAN_TIMEOUT)))) {
					/*first full scan during connected*/
					/*or time over 60s from last full scan*/
					DBGLOG(AIS, INFO, "Full2Partial not update full scan\n");
					pGlinfo->u4LastFullScanTime = rCurrentTime;
					pGlinfo->ucTrScanType = 1;
					kalMemSet(pGlinfo->ucChannelNum, 0, FULL_SCAN_MAX_CHANNEL_NUM);
					if (pGlinfo->puFullScan2PartialChannel != NULL) {
						kalMemFree(pGlinfo->puFullScan2PartialChannel,
							VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
						pGlinfo->puFullScan2PartialChannel = NULL;
					}
				} else {
					DBGLOG(AIS, INFO, "Full2Partial update full scan to partial scan\n");

					/*at here, we should update full scan to partial scan*/
					aisGetAndSetScanChannel(prAdapter);

					if (pGlinfo->puFullScan2PartialChannel != NULL) {
						PUINT_8 pChanneltmp;
						/* update full scan to partial scan */
						pChanneltmp = pGlinfo->puFullScan2PartialChannel;
						channel_t = (P_PARTIAL_SCAN_INFO)pChanneltmp;

						/* set partial scan */
						prScanReqMsg->ucChannelListNum = channel_t->ucChannelListNum;
						u4size = sizeof(channel_t->arChnlInfoList);

						DBGLOG(AIS, TRACE, "Full2Partial ChList=%d,u4size=%d\n",
							channel_t->ucChannelListNum, u4size);

						kalMemCopy(&(prScanReqMsg->arChnlInfoList),
							&(channel_t->arChnlInfoList), u4size);
						/* set scan channel type for partial scan */
						prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
					}
				}
			}

			if (prAisFsmInfo->u4ScanIELength > 0) {
				kalMemCopy(prScanReqMsg->aucIE, prAisFsmInfo->aucScanIEBuf,
					   prAisFsmInfo->u4ScanIELength);
			} else {
#if CFG_SUPPORT_WPS2
				if (prAdapter->prGlueInfo->u2WSCIELen > 0) {
					kalMemCopy(prScanReqMsg->aucIE, &prAdapter->prGlueInfo->aucWSCIE,
						   prAdapter->prGlueInfo->u2WSCIELen);
				}
			}
#endif

			prScanReqMsg->u2IELen = u2ScanIELen;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanReqMsg, MSG_SEND_METHOD_BUF);
			DBGLOG(AIS, TRACE, "SendSR%d\n", prScanReqMsg->ucSeqNum);
			kalMemZero(prAisFsmInfo->aucRandomMac, MAC_ADDR_LEN);
			prAisFsmInfo->fgTryScan = FALSE;	/* Will enable background sleep for infrastructure */
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;

			prAdapter->ucScanTime++;
			break;

		case AIS_STATE_REQ_CHANNEL_JOIN:

			/*set timeout timer*/
			cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer
				, AIS_JOIN_CH_REQUEST_INTERVAL);

			/* send message to CNM for acquiring channel */
			prMsgChReq = (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_REQ_T));
			if (!prMsgChReq) {
				ASSERT(0);	/* Can't indicate CNM for channel acquiring */
				return;
			}

			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;

			if (prAisFsmInfo->prTargetBssDesc != NULL) {
				prMsgChReq->ucPrimaryChannel = prAisFsmInfo->prTargetBssDesc->ucChannelNum;
				prMsgChReq->eRfSco = prAisFsmInfo->prTargetBssDesc->eSco;
				prMsgChReq->eRfBand = prAisFsmInfo->prTargetBssDesc->eBand;
				COPY_MAC_ADDR(prMsgChReq->aucBSSID, prAisFsmInfo->prTargetBssDesc->aucBSSID);
			}

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;
			break;

		case AIS_STATE_JOIN:
			aisFsmStateInit_JOIN(prAdapter, prAisFsmInfo->prTargetBssDesc);
			break;

#if CFG_SUPPORT_ADHOC
		case AIS_STATE_IBSS_ALONE:
			aisFsmStateInit_IBSS_ALONE(prAdapter);
			break;

		case AIS_STATE_IBSS_MERGE:
			aisFsmStateInit_IBSS_MERGE(prAdapter, prAisFsmInfo->prTargetBssDesc);
			break;
#endif /* CFG_SUPPORT_ADHOC */

		case AIS_STATE_NORMAL_TR:
			if (prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* Don't do anything when rJoinTimeoutTimer is still ticking */
			} else {
				/* 1. Process for pending scan */
				if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE) {
					wlanClearScanningResult(prAdapter);
					eNextState = AIS_STATE_ONLINE_SCAN;
					fgIsTransition = TRUE;
				}
				/* 2. Process for pending roaming scan */
				else if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE) == TRUE) {
					eNextState = AIS_STATE_LOOKING_FOR;
					fgIsTransition = TRUE;
				}
				/* 3. Process for pending roaming scan */
				else if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE) == TRUE) {
					eNextState = AIS_STATE_COLLECT_ESS_INFO;
					fgIsTransition = TRUE;
				} else if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE) ==
					   TRUE) {
					eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
					fgIsTransition = TRUE;
				}
#if CFG_SCAN_ABORT_HANDLE
				if (prAisFsmInfo->fgIsAbortEvnetDuringScan) {
					DBGLOG(AIS, WARN, "proccess the pending abort event(%d)!\n"
						, prAisBssInfo->ucReasonOfDisconnect);
					prAisFsmInfo->fgIsAbortEvnetDuringScan = FALSE;
					aisFsmStateAbort(prAdapter,
								prAisBssInfo->ucReasonOfDisconnect,
								prAisBssInfo->fgIsDelayIndication);
				}
#endif
			}

			break;

		case AIS_STATE_DISCONNECTING:
			/* send for deauth frame for disconnection */
			authSendDeauthFrame(prAdapter,
					    prAisBssInfo->prStaRecOfAP,
					    (P_SW_RFB_T) NULL, REASON_CODE_DEAUTH_LEAVING_BSS, aisDeauthXmitComplete);
			cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer, 100);
			break;

		case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
			/* send message to CNM for acquiring channel */
			prMsgChReq = (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_REQ_T));
			if (!prMsgChReq) {
				ASSERT(0);	/* Can't indicate CNM for channel acquiring */
				return;
			}

			/* release channel */
			aisFsmReleaseCh(prAdapter);

			/* zero-ize */
			kalMemZero(prMsgChReq, sizeof(MSG_CH_REQ_T));

			/* filling */
			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval = prAisFsmInfo->rChReqInfo.u4DurationMs;
			prMsgChReq->ucPrimaryChannel = prAisFsmInfo->rChReqInfo.ucChannelNum;
			prMsgChReq->eRfSco = prAisFsmInfo->rChReqInfo.eSco;
			prMsgChReq->eRfBand = prAisFsmInfo->rChReqInfo.eBand;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;

			break;

		case AIS_STATE_REMAIN_ON_CHANNEL:
#if !CFG_SUPPORT_RLM_ACT_NETWORK
			SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);
			/* sync with firmware */
			nicActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX);
#else
			rlmActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_CONNECT);
#endif
			break;

		case AIS_STATE_COLLECT_ESS_INFO:
		{
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM && 0 /* disable channel utilization now */
			UINT_8 i = 0;
			P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
			struct MSG_REQ_CH_UTIL *prMsgReqChUtil = NULL;

			/* don't request channel utilization if user asked to connect a specific bss */
			if (prConnSettings->eConnectionPolicy == CONNECT_BY_BSSID) {
				eNextState = AIS_STATE_SEARCH;
				fgIsTransition = TRUE;
				break;
			}
			prMsgReqChUtil = (struct MSG_REQ_CH_UTIL *)
				cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_REQ_CH_UTIL));
			if (!prMsgReqChUtil) {
				DBGLOG(AIS, ERROR, "No memory!");
				return;
			}
			kalMemZero(prMsgReqChUtil, sizeof(*prMsgReqChUtil));
			prMsgReqChUtil->rMsgHdr.eMsgId = MID_MNY_CNM_REQ_CH_UTIL;
			prMsgReqChUtil->u2ReturnMID = MID_CNM_AIS_RSP_CH_UTIL;
			prMsgReqChUtil->u2Duration = 100; /* 100ms */
			prMsgReqChUtil->ucChnlNum = prAisSpecBssInfo->ucCurEssChnlInfoNum;
			for (; i < prMsgReqChUtil->ucChnlNum && i < sizeof(prMsgReqChUtil->aucChnlList); i++)
				prMsgReqChUtil->aucChnlList[i] = prAisSpecBssInfo->arCurEssChnlInfo[i].ucChannel;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgReqChUtil, MSG_SEND_METHOD_BUF);
#else
			eNextState = AIS_STATE_SEARCH;
			fgIsTransition = TRUE;
#endif
			break;
		}

		default:
			ASSERT(0);	/* Make sure we have handle all STATEs */
			break;

		}
	} while (fgIsTransition);

	return;

}				/* end of aisFsmSteps() */
#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmSetChannelInfo(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ ScanReqMsg, IN ENUM_AIS_STATE_T CurrentState)
{
	/*get scan channel infro from prAdapter->prGlueInfo->prScanRequest*/
	struct cfg80211_scan_request *scan_req_t = NULL;
	struct ieee80211_channel *channel_tmp = NULL;
	int i = 0;
	int j = 0;
	UINT_8 channel_num = 0;
	UINT_8 channel_counts = 0;

	if ((prAdapter == NULL) || (ScanReqMsg == NULL))
		return;
	if ((CurrentState == AIS_STATE_SCAN) || (CurrentState == AIS_STATE_ONLINE_SCAN)) {
		if (prAdapter->prGlueInfo->prScanRequest != NULL) {
			scan_req_t = prAdapter->prGlueInfo->prScanRequest;
			if ((scan_req_t != NULL) && (scan_req_t->n_channels != 0) &&
				(scan_req_t->channels != NULL)) {
				channel_counts = scan_req_t->n_channels;
				DBGLOG(AIS, TRACE, "channel_counts=%d\n", channel_counts);

				while (j < channel_counts) {
					channel_tmp = scan_req_t->channels[j];
					if (channel_tmp == NULL)
						break;

					DBGLOG(AIS, TRACE, "set channel band=%d\n", channel_tmp->band);
					if (channel_tmp->band >= IEEE80211_BAND_60GHZ) {
						j++;
						continue;
					}
					if (i >= MAXIMUM_OPERATION_CHANNEL_LIST)
						break;
					if (channel_tmp->band == IEEE80211_BAND_2GHZ)
						ScanReqMsg->arChnlInfoList[i].eBand = BAND_2G4;
					else if (channel_tmp->band == IEEE80211_BAND_5GHZ)
						ScanReqMsg->arChnlInfoList[i].eBand = BAND_5G;

					DBGLOG(AIS, TRACE, "set channel channel_rer =%d\n",
						channel_tmp->center_freq);

					channel_num = (UINT_8)nicFreq2ChannelNum(
						channel_tmp->center_freq * 1000);

					DBGLOG(AIS, TRACE, "set channel channel_num=%d\n",
						channel_num);
					ScanReqMsg->arChnlInfoList[i].ucChannelNum = channel_num;

					j++;
					i++;
				}
			}
		}
	}

	DBGLOG(AIS, INFO, "set channel i=%d\n", i);
	if (i > 0) {
		ScanReqMsg->ucChannelListNum = i;
		ScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;

		return;
	}

	if (prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX]
		== BAND_NULL) {
		if (prAdapter->fgEnable5GBand == TRUE)
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
		else
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
		} else if (prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX]
				== BAND_2G4) {
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
		} else if (prAdapter->aePreferBand[NETWORK_TYPE_AIS_INDEX]
				== BAND_5G) {
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
		} else {
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
			ASSERT(0);
		}


}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/

VOID aisFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	UINT_8 ucSeqNumOfCompMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq = &prAdapter->rWifiVar.rRmReqParams;
	struct BCN_RM_PARAMS *prBcnRmParam = &prRmReq->rBcnRmParam;

	DEBUGFUNC("aisFsmRunEventScanDone()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	ucScanTimeoutTimes = 0;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;
	ASSERT(prScanDoneMsg->ucNetTypeIndex == (UINT_8) NETWORK_TYPE_AIS_INDEX);

	ucSeqNumOfCompMsg = prScanDoneMsg->ucSeqNum;
	cnmMemFree(prAdapter, prMsgHdr);

	eNextState = prAisFsmInfo->eCurrentState;


	if (ucSeqNumOfCompMsg != prAisFsmInfo->ucSeqNumOfScanReq) {
		DBGLOG(AIS, WARN, "SEQ NO of AIS SCN DONE MSG is not matched %d %d.\n",
				   ucSeqNumOfCompMsg, prAisFsmInfo->ucSeqNumOfScanReq);
	} else {
		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_SCAN:
			prConnSettings->fgIsScanReqIssued = FALSE;

			/* reset scan IE buffer */
			prAisFsmInfo->u4ScanIELength = 0;

			kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
			eNextState = AIS_STATE_IDLE;
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
			break;

		case AIS_STATE_ONLINE_SCAN:
			prConnSettings->fgIsScanReqIssued = FALSE;

			/* reset scan IE buffer */
			prAisFsmInfo->u4ScanIELength = 0;

			kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_NORMAL_TR;
#endif /* CFG_SUPPORT_ROAMING */
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			scanGetCurrentEssChnlList(prAdapter);
#endif
			break;

		case AIS_STATE_LOOKING_FOR:
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
			scanReportBss2Cfg80211(prAdapter, BSS_TYPE_INFRASTRUCTURE, NULL);
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_COLLECT_ESS_INFO;
#endif /* CFG_SUPPORT_ROAMING */
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			scanGetCurrentEssChnlList(prAdapter);
#endif

			break;

		default:
			DBGLOG(AIS, WARN, "current state[%d],ScanSeqNum=%d can't report SCAN_DONE!\n",
				   prAisFsmInfo->eCurrentState, prAisFsmInfo->ucSeqNumOfScanReq);
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
			break;

		}
	}
	aisRemoveOldestBcnTimeout(prAisFsmInfo);
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

	if (prBcnRmParam->eState == RM_NO_REQUEST)
		return;
	/* normal mode scan done, and beacon measurement is pending, schedule to do measurement */
	if (prBcnRmParam->eState == RM_WAITING) {
		rlmDoBeaconMeasurement(prAdapter, 0);
	} else if (prBcnRmParam->rNormalScan.fgExist) {/* pending normal scan here, should schedule it on time */
		struct NORMAL_SCAN_PARAMS *prParam = &prBcnRmParam->rNormalScan;

		DBGLOG(AIS, INFO, "Schedule normal scan after a beacon measurement done\n");
		prBcnRmParam->eState = RM_WAITING;
		prBcnRmParam->rNormalScan.fgExist = FALSE;
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer,
					SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));
		aisFsmScanRequestAdv(prAdapter, prParam->ucSsidNum, prParam->arSSID,
					prParam->aucScanIEBuf, prParam->u4IELen, prParam->aucRandomMac);
	} else /* Radio Measurement is on-going, schedule to next Measurement Element */
		rlmStartNextMeasurement(prAdapter, FALSE);

}				/* end of aisFsmRunEventScanDone() */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	UINT_8 ucReasonOfDisconnect;
	BOOLEAN fgDelayIndication;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventAbort()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Extract information of Abort Message and then free memory. */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) prMsgHdr;
	ucReasonOfDisconnect = prAisAbortMsg->ucReasonOfDisconnect;
	fgDelayIndication = prAisAbortMsg->fgDelayIndication;

	cnmMemFree(prAdapter, prMsgHdr);

#if DBG
	DBGLOG(AIS, STATE, "EVENT-ABORT: Current State %s %d\n",
			    apucDebugAisState[prAisFsmInfo->eCurrentState], ucReasonOfDisconnect);
#else
	DBGLOG(AIS, STATE, "[%d] EVENT-ABORT: Current State [%d %d] fgIsConnReqIssue:%d\n",
			    DBG_AIS_IDX, prAisFsmInfo->eCurrentState, ucReasonOfDisconnect
			    , prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued);

#endif

	GET_CURRENT_SYSTIME(&(prAisFsmInfo->rJoinReqTime));

	/* 4 <2> clear previous pending connection request and insert new one */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED
	    || ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED) {
		P_STA_RECORD_T prSta = prAisFsmInfo->prTargetStaRec;
		P_BSS_DESC_T prBss = prAisFsmInfo->prTargetBssDesc;

		if (prSta && prBss && prSta->u2ReasonCode == REASON_CODE_DISASSOC_AP_OVERLOAD) {
			struct AIS_BLACKLIST_ITEM *prBlackList = aisAddBlacklist(prAdapter, prBss);

			if (prBlackList)
				prBlackList->u2DeauthReason = prSta->u2ReasonCode;
		}
		if (prAisFsmInfo->prTargetBssDesc)
			prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = TRUE;
		prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
	} else {
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
	}
	/* to support user space triggered roaming */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_ROAMING &&
	    prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR &&
		    prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
			aisFsmSteps(prAdapter, AIS_STATE_COLLECT_ESS_INFO);
		} else {
			aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
			aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_ROAMING_CONNECT);
		}
		return;
	}
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	scanGetCurrentEssChnlList(prAdapter);
#endif
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
	aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

	if (prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
		/* 4 <3> invoke abort handler */
		aisFsmStateAbort(prAdapter, ucReasonOfDisconnect, fgDelayIndication);
	}

}				/* end of aisFsmRunEventAbort() */

/*----------------------------------------------------------------------------*/
/*!
* \brief        This function handles AIS-FSM abort event/command
*
* \param[in] prAdapter              Pointer of ADAPTER_T
*            ucReasonOfDisconnect   Reason for disonnection
*            fgDelayIndication      Option to delay disconnection indication
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort(IN P_ADAPTER_T prAdapter, UINT_8 ucReasonOfDisconnect, BOOLEAN fgDelayIndication)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	BOOLEAN fgIsCheckConnected;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	fgIsCheckConnected = FALSE;

	/* 4 <1> Save information of Abort Message and then free memory. */
	prAisBssInfo->ucReasonOfDisconnect = ucReasonOfDisconnect;
	prAisBssInfo->fgIsDelayIndication = fgDelayIndication;

	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
		prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING &&
		ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION &&
		ucReasonOfDisconnect != DISCONNECT_REASON_CODE_ROAMING)
		wmmNotifyDisconnected(prAdapter);

	/* 4 <2> Abort current job. */
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IDLE:
	case AIS_STATE_SEARCH:
		break;

	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		/* Do cancel timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_SCAN:
#if CFG_SCAN_ABORT_HANDLE
		/*
		 * when driver received the disconnected event from AP.
		 * driver will insert a AIS_REQUEST_SCAN, it leads scan request not match when scan_done report before!
		 * AIS_FSM poended the abort event and wait scan_done to report to upper layer and process
		 * the disconnected event from AP at Idle state!
		 */
		if ((ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST) ||
			(ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED) ||
			(ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED)) {
			prAisFsmInfo->fgIsAbortEvnetDuringScan = TRUE;
			DBGLOG(AIS, INFO, "Reason code:%d! Postpone the evnet of abort for AIS scanning\n"
				, prAisBssInfo->ucReasonOfDisconnect);
			return;
		}
#endif
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

#if CFG_SCAN_ABORT_HANDLE
		/* To avoid the AIS_FSM took a lot of time to connect and leads to scan pending too long
		 * AIS_FSM abort scan and wait scan_done (scan_cancel) to report to upper layer
		 * and process AIS_REQUEST_RECONNECT at Idle state.
		 */
		if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_NEW_CONNECTION
			&& prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued == TRUE) {
			DBGLOG(AIS, WARN, "Reason code:%d! Abort the AIS scanning and wait for AIS scan done!\n"
			, prAisBssInfo->ucReasonOfDisconnect);
			return;
		}
#endif
		/* queue for later handling */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == FALSE)
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);

		break;

	case AIS_STATE_LOOKING_FOR:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_REQ_CHANNEL_JOIN:
		/* Release channel to CNM */
		aisFsmReleaseCh(prAdapter);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;

		/* stop okc timeout timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer);
		break;

	case AIS_STATE_JOIN:
		/* Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

#if CFG_SUPPORT_ADHOC
	case AIS_STATE_IBSS_ALONE:
	case AIS_STATE_IBSS_MERGE:
		aisFsmStateAbort_IBSS(prAdapter);
		break;
#endif /* CFG_SUPPORT_ADHOC */

	case AIS_STATE_ONLINE_SCAN:
#if CFG_SCAN_ABORT_HANDLE
		/*
		 * when driver received the disconnected event from AP.
		 * driver will insert a AIS_REQUEST_SCAN, it leads scan request not match when scan_done report before!
		 * AIS_FSM poended the abort event and wait scan_done to report to upper layer and process
		 * the disconnected event from AP at Idle state!
		 */
		if ((ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST) ||
			(ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED) ||
			(ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED)) {
			prAisFsmInfo->fgIsAbortEvnetDuringScan = TRUE;
			DBGLOG(AIS, INFO, "Reason code:%d! Postpone the evnet of abort for AIS online scanning\n"
				, prAisBssInfo->ucReasonOfDisconnect);
			return;
		}
#endif
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

#if CFG_SCAN_ABORT_HANDLE
		/* New connection will abort the scan
		 * To avoid the AIS_FSM took a lot of time to connect and leads to scan pending too long
		 * AIS_FSM abort scan and wait scan_done (scan_cancel) to report to upper layer
		 * and process AIS_REQUEST_RECONNECT at Idle state.
		 */
		if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_NEW_CONNECTION
			&& prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued == TRUE) {
			DBGLOG(AIS, WARN, "Reason code:%d! Abort the AIS scanning and wait for AIS scan done!\n"
			, prAisBssInfo->ucReasonOfDisconnect);
			return;
		}
#endif
		/* queue for later handling */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == FALSE)
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);

		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_NORMAL_TR:
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_DISCONNECTING:
		/* Do abort NORMAL_TR */
		aisFsmStateAbort_NORMAL_TR(prAdapter);

		break;

	case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
		/* release channel */
		aisFsmReleaseCh(prAdapter);
		break;

	case AIS_STATE_REMAIN_ON_CHANNEL:
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		break;

	default:
		break;
	}

	if (fgIsCheckConnected && (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)) {

		/* switch into DISCONNECTING state for sending DEAUTH if necessary */
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
		    prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_NEW_CONNECTION &&
		    prAisBssInfo->prStaRecOfAP && prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			aisFsmSteps(prAdapter, AIS_STATE_DISCONNECTING);

			return;
		}
		/* Do abort NORMAL_TR */
		aisFsmStateAbort_NORMAL_TR(prAdapter);

	}

	if (!fgDelayIndication)
		kalMemZero(prAisFsmInfo->aucNeighborAPChnl, CFG_NEIGHBOR_AP_CHANNEL_NUM);

	rlmCancelRadioMeasurement(prAdapter);
	/* restore tx power control */
	rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
	aisFsmDisconnect(prAdapter, fgDelayIndication);


}				/* end of aisFsmStateAbort() */

#if CFG_SUPPORT_DETECT_ATHEROS_AP
VOID configDelBaToFw(P_ADAPTER_T prAdapter, BOOLEAN fgEnable)
{
	struct _CMD_HEADER_T *pcmdV1Header = NULL;
	UINT_8 itemString[] = "CoexRemoveBA";
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_FORMAT_V1_T *pr_cmd_v1 = NULL;

	pcmdV1Header = (struct _CMD_HEADER_T *) kalMemAlloc(sizeof(struct _CMD_HEADER_T), VIR_MEM_TYPE);
	if (pcmdV1Header == NULL)
		return;

	kalMemSet(pcmdV1Header->buffer, 0, MAX_CMD_BUFFER_LENGTH);

	pr_cmd_v1 = (struct _CMD_FORMAT_V1_T *) pcmdV1Header->buffer;
	pr_cmd_v1->itemStringLength = strlen(itemString);
	kalMemCopy(pr_cmd_v1->itemString, itemString, pr_cmd_v1->itemStringLength);
	pr_cmd_v1->itemType = 1;
	pr_cmd_v1->itemValueLength = 1;
	if (fgEnable)
		kalMemCopy(pr_cmd_v1->itemValue, "1", pr_cmd_v1->itemValueLength);
	else
		kalMemCopy(pr_cmd_v1->itemValue, "0", pr_cmd_v1->itemValueLength);

	pcmdV1Header->cmdVersion = CMD_VER_1_EXT;
	pcmdV1Header->cmdType = CMD_TYPE_SET;
	pcmdV1Header->itemNum = 1;
	pcmdV1Header->cmdBufferLen = sizeof(struct _CMD_FORMAT_V1_T);

	rStatus = wlanSendSetQueryCmd(prAdapter, CMD_ID_GET_SET_CUSTOMER_CFG,
				TRUE, FALSE, FALSE,
				NULL, NULL,
				sizeof(struct _CMD_HEADER_T),
				(PUINT_8) pcmdV1Header,
				NULL, 0);
	kalMemFree(pcmdV1Header, VIR_MEM_TYPE, sizeof(struct _CMD_HEADER_T));

	if (rStatus == WLAN_STATUS_FAILURE)
		DBGLOG(INIT, ERROR, "wifiSefCFG fail 0x%x\n", rStatus);
}

VOID aisSendBaCmdByAtherosAp(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	static BOOLEAN fgSetDelBatoFw = FALSE;

	if (fgSetDelBatoFw) {
		configDelBaToFw(prAdapter, FALSE);
		fgSetDelBatoFw = FALSE;
	}

	if (prBssDesc) {
		if (prBssDesc->fgIsAtherosAP) {
			DBGLOG(AIS, INFO, "Connect to AtherosAP,Del BA\n");
			configDelBaToFw(prAdapter, TRUE);
			fgSetDelBatoFw = TRUE;
		}
	} else {
		return;
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Join Complete Event from SAA FSM for AIS FSM
*
* @param[in] prMsgHdr   Message of Join Complete of SAA FSM.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_JOIN_COMP_T prJoinCompMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_STA_RECORD_T prStaRec;
	P_SW_RFB_T prAssocRspSwRfb;
	P_BSS_INFO_T prAisBssInfo;
	UINT_8 aucP2pSsid[] = CTIA_MAGIC_SSID;
	OS_SYSTIME rCurrentTime;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_16 u2StatusCode = 0;
#if CFG_SUPPORT_ROAMING_RETRY
	P_LINK_T prEssLink = NULL;
	BOOLEAN fgIsUnderRoaming;
#endif

	DEBUGFUNC("aisFsmRunEventJoinComplete()");

	ASSERT(prMsgHdr);

	GET_CURRENT_SYSTIME(&rCurrentTime);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (P_MSG_JOIN_COMP_T) prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
#if CFG_SUPPORT_ROAMING_RETRY
	prEssLink = &prAdapter->rWifiVar.rAisSpecificBssInfo.rCurEssLink;
	fgIsUnderRoaming = FALSE;
#endif
	eNextState = prAisFsmInfo->eCurrentState;
	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	DBGLOG(AIS, TRACE, "AISOK\n");

	/* Check State and SEQ NUM */
	do {
		if (prAisFsmInfo->eCurrentState != AIS_STATE_JOIN)
			break;

		prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

#if CFG_SUPPORT_RN
		GET_CURRENT_SYSTIME(&prAisBssInfo->rConnTime);
#endif
		/* Check SEQ NUM */
		if (prJoinCompMsg->ucSeqNum != prAisFsmInfo->ucSeqNumOfReqMsg) {
#if DBG
			DBGLOG(AIS, WARN, "SEQ NO of AIS JOIN COMP MSG is not matched.\n");
#endif
			break;
		}

		/* 4 <1> JOIN was successful */
		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {
#if CFG_SUPPORT_RN
			prAisBssInfo->fgDisConnReassoc = FALSE;
#endif
			/* 1. Reset retry count */
			prAisFsmInfo->ucConnTrialCount = 0;
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
			/* Completion of roaming */
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {

#if CFG_SUPPORT_ROAMING
				/* 2. Deactivate previous BSS */
				aisFsmRoamingDisconnectPrevAP(prAdapter, prStaRec);

				/* 3. Update bss based on roaming staRec */
				aisUpdateBssInfoForRoamingAP(prAdapter, prStaRec, prAssocRspSwRfb);
#endif /* CFG_SUPPORT_ROAMING */
			} else {
				kalMemZero(&prAdapter->prGlueInfo->rNetDevStats,
					sizeof(prAdapter->prGlueInfo->rNetDevStats));

				/* 4 <1.1> Change FW's Media State immediately. */
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
				if ((prAisBssInfo->prStaRecOfAP) &&
					   (prAisBssInfo->prStaRecOfAP != prStaRec) &&
					   (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {

					cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP,
								    STA_STATE_1);
					cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP, TRUE);
				}
				prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = FALSE;
				/* 4 <1.3> Update BSS_INFO_T */
				aisUpdateBssInfoForJOIN(prAdapter, prStaRec, prAssocRspSwRfb);

				/* 4 <1.4> Activate current AP's STA_RECORD_T in Driver. */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

				/* 4 <1.5> Update RSSI if necessary */
				nicUpdateRSSI(prAdapter, NETWORK_TYPE_AIS_INDEX,
						      (INT_8) (RCPI_TO_dBm(prStaRec->ucRCPI)), 0);

				/* 4 <1.6> Indicate Connected Event to Host immediately. */
				/* Require BSSID, Association ID, Beacon Interval.. */
				/* from AIS_BSS_INFO_T */
				aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED,
								FALSE);

				/* add for ctia mode */
				if (EQUAL_SSID(aucP2pSsid, CTIA_MAGIC_SSID_LEN, prAisBssInfo->aucSSID,
					prAisBssInfo->ucSSIDLen)) {
					nicEnterCtiaMode(prAdapter, TRUE, FALSE);
				}
			}

			kalMemZero(prAisFsmInfo->aucNeighborAPChnl, CFG_NEIGHBOR_AP_CHANNEL_NUM);
			aisSendNeighborRequest(prAdapter);

#if CFG_SUPPORT_ROAMING
			/* if bssid is given, it means we no need fw roaming */
			if ((prAdapter->rWifiVar.rConnSettings.eConnectionPolicy != CONNECT_BY_BSSID)
				&& (prAdapter->rWifiVar.rRoamingInfo.DrvRoamingAllow == 1))
				roamingFsmRunEventStart(prAdapter);
#endif /* CFG_SUPPORT_ROAMING */

			/* clear rJoinReqTime if there is no more framework roaming connect request */
			if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, FALSE) == FALSE)
				prAisFsmInfo->rJoinReqTime = 0;

			prAisFsmInfo->ucJoinFailCntAfterScan = 0;
			/* 4 <1.7> Set the Next State of AIS FSM */
			eNextState = AIS_STATE_NORMAL_TR;
#if CFG_SUPPORT_DYNAMIC_ROAM
			aisFsmSetRoamingThreshold(prAdapter, AIS_DEFAULT_ROAMING_THRESHOLD);
#endif
		}
			/* 4 <2> JOIN was not successful */
		else {
			/* 4 <2.1> Redo JOIN process with other Auth Type if possible */
			if (aisFsmStateInit_RetryJOIN(prAdapter, prStaRec) == FALSE) {
				P_BSS_DESC_T prBssDesc;
				PARAM_SSID_T rSsid;
				P_CONNECTION_SETTINGS_T prConnSettings;

				prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
				prBssDesc = prAisFsmInfo->prTargetBssDesc;

				/* 1. Increase Failure Count */
				prStaRec->ucJoinFailureCount++;

				/* 2. release channel */
				aisFsmReleaseCh(prAdapter);

				/* 3.1 stop join timeout timer */
				cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

				/* 3.2 reset local variable */
				prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
				prAisFsmInfo->ucJoinFailCntAfterScan++;

				kalMemZero(&rSsid, sizeof(PARAM_SSID_T));
				if (prBssDesc)
					COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
						prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
				else
					COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
						prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

				prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter,
								prStaRec->aucMacAddr, TRUE, &rSsid);

				if (prBssDesc == NULL) {
					/* it maybe NULL when wlanRemove */
					/*
					(1) UI does wifi off during SAA does auth/assoc procedure.
					(2) We will do LINK_INITIALIZE(&prScanInfo->rBSSDescList);
					in nicUninitMGMT().
					(3) We will handle prMsduInfo->pfTxDoneHandler
					in nicTxRelease().
					(4) prMsduInfo->pfTxDoneHandler will point to
					saaFsmRunEventTxDone().
					(5) Then jump to saaFsmSteps() -> saaFsmSendEventJoinComplete()
					(6) Finally mboxSendMsg() -> aisFsmRunEventJoinComplete().
					(7) In aisFsmRunEventJoinComplete(), we will check
					"prBssDesc = scanSearchBssDescByBssid(prAdapter,
					prStaRec->aucMacAddr);"
					(8) And prBssDesc will be NULL and hangs in
					"ASSERT(prBssDesc->fgIsConnecting);" when DBG=0.
					ASSERT(prBssDesc);
					ASSERT(prBssDesc->fgIsConnecting);
					*/
					aisFsmStateAbort(prAdapter,
						DISCONNECT_REASON_CODE_DEAUTHENTICATED, FALSE);
					break;
				}
				DBGLOG(AIS, TRACE,
					"ucJoinFailureCount=%d %d, Status=%d Reason=%d, eConnectionState=%d, fgDisConnReassoc=%d\n",
					prStaRec->ucJoinFailureCount, prBssDesc->ucJoinFailureCount,
					prStaRec->u2StatusCode, prStaRec->u2ReasonCode,
					prAisBssInfo->eConnectionState, prAisBssInfo->fgDisConnReassoc);

				/* ASSERT(prBssDesc); */
				/* ASSERT(prBssDesc->fgIsConnecting); */
				u2StatusCode = prStaRec->u2StatusCode;
				prBssDesc->ucJoinFailureCount++;
				if (prBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD) {
					aisAddBlacklist(prAdapter, prBssDesc);
					GET_CURRENT_SYSTIME(&prBssDesc->rJoinFailTime);
					DBGLOG(AIS, INFO,
						"Bss %pM join fail %d > %d times,temp disable it at time:%u\n",
						prBssDesc->aucBSSID,
						prBssDesc->ucJoinFailureCount,
						SCN_BSS_JOIN_FAIL_THRESOLD,
						prBssDesc->rJoinFailTime);
				}
				if (prBssDesc->prBlack)
					prBssDesc->prBlack->u2AuthStatus = prStaRec->u2StatusCode;

				prBssDesc->fgIsConnecting = FALSE;

				/* 3.3 Free STA-REC */
				if (prStaRec != prAisBssInfo->prStaRecOfAP)
					cnmStaRecFree(prAdapter, prStaRec, FALSE);

				if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
#if CFG_SUPPORT_ROAMING
					if (prAisBssInfo->prStaRecOfAP)
						prAisBssInfo->prStaRecOfAP->fgIsTxAllowed = TRUE;
#if CFG_SUPPORT_ROAMING_RETRY
					/*Under roamming case : After candidate BSS joined fail.*/
					/*STA will re-try other BSS.*/
					DBGLOG(AIS, INFO,
						"Under roamming %pM join fail and STA will re-try other AP\n",
					prBssDesc->aucBSSID);
					prBssDesc->fgIsRoamFail = TRUE;
					fgIsUnderRoaming = TRUE;

					eNextState = AIS_STATE_COLLECT_ESS_INFO;
#else
					eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
#endif /* CFG_SUPPORT_ROAMING_RETRY */
					if (prConnSettings->eConnectionPolicy == CONNECT_BY_BSSID &&
						(u2StatusCode == STATUS_CODE_ASSOC_DENIED_AP_OVERLOAD ||
						u2StatusCode == STATUS_CODE_ASSOC_DENIED_OUTSIDE_STANDARD)) {
						kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
									     WLAN_STATUS_MEDIA_DISCONNECT,
									     (PVOID)&u2StatusCode,
									     sizeof(u2StatusCode));
						prAisBssInfo->eConnectionStateIndicated =
							PARAM_MEDIA_STATE_DISCONNECTED;
						eNextState = AIS_STATE_IDLE;
					}
#endif /* CFG_SUPPORT_ROAMING */
#if CFG_SUPPORT_RN
				} else if (prAisBssInfo->fgDisConnReassoc == TRUE) {
					/* abort connection trial */
					prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
					prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;

					prAisBssInfo->u2DeauthReason = prStaRec->u2ReasonCode;

					prAisBssInfo->fgDisConnReassoc = FALSE;
					aisIndicationOfMediaStateToHost(prAdapter,
							PARAM_MEDIA_STATE_DISCONNECTED, FALSE);
					/* restore tx power control */
					rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
					eNextState = AIS_STATE_IDLE;
#endif
				} else if (prAisFsmInfo->rJoinReqTime != 0 &&
					   CHECK_FOR_TIMEOUT(rCurrentTime,
							     prAisFsmInfo->rJoinReqTime,
							     SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
					/* abort connection trial */
					prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
					prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
					/* restore tx power control */
					rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
					kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
								     WLAN_STATUS_JOIN_FAILURE,
								     (PVOID)&u2StatusCode,
								     sizeof(u2StatusCode));

					eNextState = AIS_STATE_IDLE;
				/* Don't send join fail if this join is the driver retry join (rJoinReqTime == 0) */
				} else if (prAisFsmInfo->rJoinReqTime != 0 &&
					   prBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD) {
					/*Avoid STA to retry connect AP fenqency and printk too much.*/
					/*abort connection trial */
					DBGLOG(AIS, INFO,
					"Bss %pM join fail over %d,response upper layer to connect fail\n",
					prBssDesc->aucBSSID, SCN_BSS_JOIN_FAIL_THRESOLD);
					prAdapter->rWifiVar.rConnSettings.fgIsConnReqIssued = FALSE;
					prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
					/* restore tx power control */
					rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
					kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
								     WLAN_STATUS_JOIN_FAILURE,
								     (PVOID)&u2StatusCode,
								     sizeof(u2StatusCode));

					eNextState = AIS_STATE_IDLE;
				} else {
					/* 4.b send reconnect request */
					aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);
					eNextState = AIS_STATE_IDLE;
				}

#if CFG_SUPPORT_ROAMING_RETRY
				if (fgIsUnderRoaming == TRUE &&
					prEssLink->u4NumElem > AIS_ROAMING_RETRY_BSS_THRESHOLD) {
					/*Under roaming case : After candidate BSS joined fail.*/
					/*STA will re-try other BSS.*/
					/*if roaming failure was over then AIS_ROAMING_CONNECTION_TRIAL_LIMIT */
					/*ROAM_FSM was stopped and AIS_FSM was transferred to NORMAL_TR.*/
					DBGLOG(AIS, INFO,
						"Under roamming %pM join fail and STA will re-try other AP :%d\n"
						, prBssDesc->aucBSSID
						, prEssLink->u4NumElem);
						prBssDesc->fgIsRoamFail = TRUE;
						fgIsUnderRoaming = FALSE;
						eNextState = AIS_STATE_COLLECT_ESS_INFO;
				}
#endif /* CFG_SUPPORT_ROAMING_RETRY */
			}
		}

		/* try to remove timeout blacklist item */
		aisRemoveDisappearedBlacklist(prAdapter);

		if (eNextState != prAisFsmInfo->eCurrentState)
			aisFsmSteps(prAdapter, eNextState);
	} while (FALSE);

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);

}				/* end of aisFsmRunEventJoinComplete() */

#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Grant Msg of IBSS Create which was sent by
*        CNM to indicate that channel was changed for creating IBSS.
*
* @param[in] prAdapter  Pointer of ADAPTER_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmCreateIBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	do {
		/* Check State */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_IBSS_ALONE)
			aisUpdateBssInfoForCreateIBSS(prAdapter);
	} while (FALSE);

}				/* end of aisFsmCreateIBSS() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Grant Msg of IBSS Merge which was sent by
*        CNM to indicate that channel was changed for merging IBSS.
*
* @param[in] prAdapter  Pointer of ADAPTER_T
* @param[in] prStaRec   Pointer of STA_RECORD_T for merge
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_BSS_INFO_T prAisBssInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	do {

		eNextState = prAisFsmInfo->eCurrentState;

		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_IBSS_MERGE:
			{
				P_BSS_DESC_T prBssDesc;

				/* 4 <1.1> Change FW's Media State immediately. */
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous Peers' STA_RECORD_T in Driver if have. */
				bssClearClientList(prAdapter, prAisBssInfo);

				/* 4 <1.3> Unmark connection flag of previous BSS_DESC_T. */
				prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = FALSE;
				}
				/* 4 <1.4> Update BSS_INFO_T */
				aisUpdateBssInfoForMergeIBSS(prAdapter, prStaRec);

				/* 4 <1.5> Add Peers' STA_RECORD_T to Client List */
				bssAddStaRecToClientList(prAdapter, prAisBssInfo, prStaRec);

				/* 4 <1.6> Activate current Peer's STA_RECORD_T in Driver. */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				/* 4 <1.7> Enable other features */

				/* 4 <1.8> Indicate Connected Event to Host immediately. */
				aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter);

#if CFG_SLT_SUPPORT
				prAdapter->rWifiVar.rSltInfo.prPseudoStaRec = prStaRec;
#endif
			}
			break;

		default:
			break;
		}

		if (eNextState != prAisFsmInfo->eCurrentState)
			aisFsmSteps(prAdapter, eNextState);

	} while (FALSE);

}				/* end of aisFsmMergeIBSS() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Notification of existing IBSS was found
*        from SCN.
*
* @param[in] prMsgHdr   Message of Notification of an IBSS was present.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventFoundIBSSPeer(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_AIS_IBSS_PEER_FOUND_T prAisIbssPeerFoundMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc;
	BOOLEAN fgIsMergeIn;

	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	prAisIbssPeerFoundMsg = (P_MSG_AIS_IBSS_PEER_FOUND_T) prMsgHdr;

	ASSERT(prAisIbssPeerFoundMsg->ucNetTypeIndex == NETWORK_TYPE_AIS_INDEX);

	prStaRec = prAisIbssPeerFoundMsg->prStaRec;
	ASSERT(prStaRec);

	fgIsMergeIn = prAisIbssPeerFoundMsg->fgIsMergeIn;

	cnmMemFree(prAdapter, prMsgHdr);

	eNextState = prAisFsmInfo->eCurrentState;
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:
		{
			/* 4 <1> An IBSS Peer 'merged in'. */
			if (fgIsMergeIn) {

				/* 4 <1.1> Change FW's Media State immediately. */
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Add Peers' STA_RECORD_T to Client List */
				bssAddStaRecToClientList(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				/* 4 <1.3> Mark connection flag of BSS_DESC_T. */
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);
				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					ASSERT(0);	/* Should be able to find a BSS_DESC_T here. */
				}

				/* 4 <1.4> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = TRUE;	/* TODO(Kevin): TBD */
#else
				/* 4 <1.3> Mark connection flag of BSS_DESC_T. */
				prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					ASSERT(0);	/* Should be able to find a BSS_DESC_T here. */
				}

				/* 4 <1.4> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = FALSE;	/* TODO(Kevin): TBD */

#endif

				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				/* 4 <1.6> sync. to firmware */
				nicUpdateBss(prAdapter, NETWORK_TYPE_AIS_INDEX);

				/* 4 <1.7> Indicate Connected Event to Host immediately. */
				aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

				/* 4 <1.8> indicate PM for connected */
				nicPmIndicateBssConnected(prAdapter, NETWORK_TYPE_AIS_INDEX);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter);
			}
			/* 4 <2> We need 'merge out' to this IBSS */
			else {

				/* 4 <2.1> Get corresponding BSS_DESC_T */
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				prAisFsmInfo->prTargetBssDesc = prBssDesc;

				/* 4 <2.2> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_IBSS_MERGE;
			}
		}
		break;

	case AIS_STATE_NORMAL_TR:
		{

			/* 4 <3> An IBSS Peer 'merged in'. */
			if (fgIsMergeIn) {

				/* 4 <3.1> Add Peers' STA_RECORD_T to Client List */
				bssAddStaRecToClientList(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				/* 4 <3.2> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = TRUE;	/* TODO(Kevin): TBD */
#else
				/* 4 <3.2> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = FALSE;	/* TODO(Kevin): TBD */
#endif

				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

			}
			/* 4 <4> We need 'merge out' to this IBSS */
			else {

				/* 4 <4.1> Get corresponding BSS_DESC_T */
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				prAisFsmInfo->prTargetBssDesc = prBssDesc;

				/* 4 <4.2> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_IBSS_MERGE;

			}
		}
		break;

	default:
		break;
	}

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				/* end of aisFsmRunEventFoundIBSSPeer() */
#endif /* CFG_SUPPORT_ADHOC */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Media State to HOST
*
* @param[in] eConnectionState   Current Media State
* @param[in] fgDelayIndication  Set TRUE for postponing the Disconnect Indication.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
aisIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter,
				ENUM_PARAM_MEDIA_STATE_T eConnectionState, BOOLEAN fgDelayIndication)
{
	EVENT_CONNECTION_STATUS rEventConnStatus;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisIndicationOfMediaStateToHost()");

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* NOTE(Kevin): Move following line to aisChangeMediaState() macro per CM's request. */
	/* prAisBssInfo->eConnectionState = eConnectionState; */

	/* For indicating the Disconnect Event only if current media state is
	 * disconnected and we didn't do indication yet.
	 */
	DBGLOG(AIS, INFO, "Current state: %d, connection state indicated: %d\n",
		prAisFsmInfo->eCurrentState, prAisBssInfo->eConnectionStateIndicated);

	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED &&
		/* if receive DEAUTH in JOIN state, report disconnect*/
		!(prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED &&
		 prAisFsmInfo->eCurrentState == AIS_STATE_JOIN)) {
		if (prAisBssInfo->eConnectionStateIndicated == eConnectionState)
			return;
	}

	if (!fgDelayIndication) {
		/* 4 <0> Cancel Delay Timer */
		prAisFsmInfo->u4PostponeIndStartTime = 0;

		/* 4 <1> Fill EVENT_CONNECTION_STATUS */
		rEventConnStatus.ucMediaStatus = (UINT_8) eConnectionState;

		if (eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			rEventConnStatus.ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED;

			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_INFRA;
				rEventConnStatus.u2AID = prAisBssInfo->u2AssocId;
				rEventConnStatus.u2ATIMWindow = 0;
			} else if (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_IBSS;
				rEventConnStatus.u2AID = 0;
				rEventConnStatus.u2ATIMWindow = prAisBssInfo->u2ATIMWindow;
			} else {
				ASSERT(0);
			}

			COPY_SSID(rEventConnStatus.aucSsid,
				  rEventConnStatus.ucSsidLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

			COPY_MAC_ADDR(rEventConnStatus.aucBssid, prAisBssInfo->aucBSSID);

			rEventConnStatus.u2BeaconPeriod = prAisBssInfo->u2BeaconInterval;
			rEventConnStatus.u4FreqInKHz = nicChannelNum2Freq(prAisBssInfo->ucPrimaryChannel);

			switch (prAisBssInfo->ucNonHTBasicPhyType) {
			case PHY_TYPE_HR_DSSS_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;

			case PHY_TYPE_ERP_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM24;
				break;

			case PHY_TYPE_OFDM_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM5;
				break;

			default:
				ASSERT(0);
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;
			}
		} else {
			/* Deactivate previous Peers' STA_RECORD_T in Driver if have. */
			bssClearClientList(prAdapter, prAisBssInfo);
#if (CFG_REFACTORY_PMKSA == 0)
#if CFG_PRIVACY_MIGRATION
			/* Clear the pmkid cache while media disconnect */
			secClearPmkid(prAdapter);
#endif
#endif
			rEventConnStatus.ucReasonOfDisconnect = prAisBssInfo->ucReasonOfDisconnect;
		}

		/* 4 <2> Indication */
		nicMediaStateChange(prAdapter, NETWORK_TYPE_AIS_INDEX, &rEventConnStatus);
		prAisBssInfo->eConnectionStateIndicated = eConnectionState;
	} else {
		/* NOTE: Only delay the Indication of Disconnect Event */
		ASSERT(eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED);

#if CFG_SUPPORT_RN
		if (prAisBssInfo->fgDisConnReassoc)
			DBGLOG(AIS, INFO, "Reassoc the AP once beacause of receive deauth/deassoc\n");
		else
#endif
		{
			DBGLOG(AIS, INFO, "Postpone the indication of Disconnect for %d seconds\n",
					   prConnSettings->ucDelayTimeOfDisconnectEvent);
			prAisFsmInfo->u4PostponeIndStartTime = kalGetTimeTick();
		}

	}

}				/* end of aisIndicationOfMediaStateToHost() */
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Sched Scan Start"
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisPostponedEventOfSchedScanReq(IN P_ADAPTER_T prAdapter, IN P_AIS_FSM_INFO_T prAisFsmInfo)
{
	P_SCAN_INFO_T prScanInfo;
	P_PARAM_SCHED_SCAN_REQUEST prSchedScanRequest;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prSchedScanRequest = &prScanInfo->rSchedScanRequest;

	DBGLOG(AIS, INFO, "aisPostponedEventOfSchedScanReq:AIS CurState[%d] SchedScanReq:%d\n"
		, prAisFsmInfo->eCurrentState
		, prScanInfo->eCurrendSchedScanReq);

	if (prScanInfo->fgIsPostponeSchedScan == TRUE) {
		if (prScanInfo->eCurrendSchedScanReq == SCHED_SCAN_POSTPONE_START) {
			/*resume schedscan start*/
			if (scnFsmSchedScanRequest(prAdapter) == TRUE)
				DBGLOG(AIS, INFO, "aisPostponedEventOf SchedScanStart: Success!\n");
			else
				DBGLOG(AIS, WARN, "aisPostponedEventOf SchedScanStart: fail\n");

		} else if (prScanInfo->eCurrendSchedScanReq == SCHED_SCAN_POSTPONE_STOP) {
			/*resume schedscan stop*/
			if (scnFsmSchedScanStopRequest(prAdapter) == TRUE)
				DBGLOG(AIS, INFO, "aisPostponedEventOf SchedScanStop: Success!\n");
			else
				DBGLOG(AIS, INFO, "aisPostponedEventOf SchedScanStop: fail!\n");

		} else
			DBGLOG(AIS, INFO, "unexcept SchedScan Request!\n");
	} else {
		DBGLOG(AIS, WARN, "driver don't resume schedScan Request\n");
	}



}				/* end of aisPostponedEventOfSchedScanReq() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Media Disconnect" to HOST
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisPostponedEventOfDisconnTimeout(IN P_ADAPTER_T prAdapter, IN P_AIS_FSM_INFO_T prAisFsmInfo)
{
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_SCAN_INFO_T prScanInfo;
	BOOLEAN fgFound = TRUE;
	BOOLEAN fgIsPostponeTimeout;
	BOOLEAN fgIsBeaconTimeout;

	/*
	 * firstly, check if we have started postpone indication.
	 * otherwise, give a chance to do join before indicate to host
	 */
	if (prAisFsmInfo->u4PostponeIndStartTime == 0)
		return;

	/* if we're in	req channel/join/search state, don't report disconnect. */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_SEARCH ||
		prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_COLLECT_ESS_INFO) {
		DBGLOG(AIS, INFO, "CurrentState: %d, don't report disconnect\n",
				   prAisFsmInfo->eCurrentState);
		return;
	}

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	if (prScanInfo->eCurrentState == SCAN_STATE_SCANNING) {
		DBGLOG(AIS, INFO, "SCANNING, don't report disconnect\n");
		return;
	}

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	fgIsPostponeTimeout = CHECK_FOR_TIMEOUT(kalGetTimeTick(), prAisFsmInfo->u4PostponeIndStartTime,
					SEC_TO_MSEC(prConnSettings->ucDelayTimeOfDisconnectEvent));
	fgIsBeaconTimeout = prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST;
#if CFG_SUPPORT_RN
	fgIsBeaconTimeout &= !prAisBssInfo->fgDisConnReassoc;
#endif
	/* only retry connect once when beacon timeout */
	if (!fgIsPostponeTimeout && !(fgIsBeaconTimeout && prAisFsmInfo->ucConnTrialCount > 1)) {
		DBGLOG(AIS, INFO, "DelayTimeOfDisconnect, don't report disconnect\n");
		return;
	}

	/* 4 <1> Deactivate previous AP's STA_RECORD_T in Driver if have. */
	if (prAisBssInfo->prStaRecOfAP) {
		/* cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1); */
		prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
	}
	/* 4 <2> Remove all pending connection request */
	while (fgFound)
		fgFound = aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR)
		prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;
	prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
	prAisBssInfo->u2DeauthReason = REASON_CODE_BEACON_TIMEOUT;
	/* 4 <3> Indicate Disconnected Event to Host immediately. */
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED, FALSE);

}				/* end of aisPostponedEventOfDisconnTimeout() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the association was completed.
*
* @param[in] prStaRec               Pointer to the STA_RECORD_T
* @param[in] prAssocRspSwRfb        Pointer to SW RFB of ASSOC RESP FRAME.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, P_SW_RFB_T prAssocRspSwRfb)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame;
	P_BSS_DESC_T prBssDesc;
	UINT_16 u2IELength;
	PUINT_8 pucIE;
	PARAM_SSID_T rSsid;

	DEBUGFUNC("aisUpdateBssInfoForJOIN()");

	ASSERT(prStaRec);
	ASSERT(prAssocRspSwRfb);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prAssocRspSwRfb->pvHeader;

	DBGLOG(AIS, TRACE, "Update AIS_BSS_INFO_T and apply settings to MAC\n");

	/* 3 <1> Update BSS_INFO_T from AIS_FSM_INFO_T or User Settings */
	/* 4 <1.1> Setup Operation Mode */
	prAisBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;

	/* 4 <1.2> Setup SSID */
	COPY_SSID(prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	/* 4 <1.3> Setup Channel, Band */
	prAisBssInfo->ucPrimaryChannel = prAisFsmInfo->prTargetBssDesc->ucChannelNum;
	prAisBssInfo->eBand = prAisFsmInfo->prTargetBssDesc->eBand;

	/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
	/* 4 <2.1> Save current AP's STA_RECORD_T and current AID */
	prAisBssInfo->prStaRecOfAP = prStaRec;
	prAisBssInfo->u2AssocId = prStaRec->u2AssocId;

	/* 4 <2.2> Setup Capability */
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;	/* Use AP's Cap Info as BSS Cap Info */

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
	else
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;

#if (CFG_SUPPORT_TDLS == 1)
	/* init the TDLS flags */
	prAisBssInfo->fgTdlsIsProhibited = prStaRec->fgTdlsIsProhibited;
	prAisBssInfo->fgTdlsIsChSwProhibited = prStaRec->fgTdlsIsChSwProhibited;
#endif /* CFG_SUPPORT_TDLS */

	/* 4 <2.3> Setup PHY Attributes and Basic Rate Set/Operational Rate Set */
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	/* 3 <3> Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */
	/* 4 <3.1> Setup BSSID */
	COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prAssocRspFrame->aucBSSID);

	u2IELength = (UINT_16) ((prAssocRspSwRfb->u2PacketLen - prAssocRspSwRfb->u2HeaderLen) -
				(OFFSET_OF(WLAN_ASSOC_RSP_FRAME_T, aucInfoElem[0]) - WLAN_MAC_MGMT_HEADER_LEN));
	pucIE = prAssocRspFrame->aucInfoElem;

	/* 4 <3.2> Parse WMM and setup QBSS flag */
	/* Parse WMM related IEs and configure HW CRs accordingly */
	mqmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	prAisBssInfo->fgIsQBSS = prStaRec->fgIsQoS;

	/* 3 <4> Update BSS_INFO_T from BSS_DESC_T */
	prBssDesc = prAisFsmInfo->prTargetBssDesc;
	if (prBssDesc)
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
			prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	else
		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen,
			prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter, prAssocRspFrame->aucBSSID, TRUE, &rSsid);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;
		prBssDesc->ucJoinFailureCount = 0;
		aisRemoveBlackList(prAdapter, prBssDesc);
		/* 4 <4.1> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
	} else {
		/* should never happen */
		DBGLOG(AIS, WARN, "no prBssDesc found!\n");
		ASSERT(0);
	}
#if CFG_SUPPORT_DETECT_ATHEROS_AP
	aisSendBaCmdByAtherosAp(prAdapter, prBssDesc);
#endif
	/* NOTE: Defer ucDTIMPeriod updating to when beacon is received after connection */
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = 0;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_INFRA;
	prAisBssInfo->ucRoamSkipTimes = CFG_GOOG_RCPI_SCAN_SKIP_TIMES;
	prAisBssInfo->fgGoodRcpiArea = FALSE;
	prAisBssInfo->fgPoorRcpiArea = FALSE;

	/* 4 <4.2> Update HT information and set channel */
	/* Record HT related parameters in rStaRec and rBssInfo
	 * Note: it shall be called before nicUpdateBss()
	 */
	rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	/* 4 <4.3> Sync with firmware for BSS-INFO */
	nicUpdateBss(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 4 <4.4> *DEFER OPERATION* nicPmIndicateBssConnected() will be invoked */
	/* inside scanProcessBeaconAndProbeResp() after 1st beacon is received */

}				/* end of aisUpdateBssInfoForJOIN() */

#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will create an Ad-Hoc network and start sending Beacon Frames.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisUpdateBssInfoForCreateIBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (prAisBssInfo->fgIsBeaconActivated)
		return;
	/* 3 <1> Update BSS_INFO_T per Network Basis */
	/* 4 <1.1> Setup Operation Mode */
	prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

	/* 4 <1.2> Setup SSID */
	COPY_SSID(prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
	prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
	prAisBssInfo->u2AssocId = 0;

	/* 4 <1.4> Setup Channel, Band and Phy Attributes */
	prAisBssInfo->ucPrimaryChannel = prConnSettings->ucAdHocChannelNum;
	prAisBssInfo->eBand = prConnSettings->eAdHocBand;

	if (prAisBssInfo->eBand == BAND_2G4) {
		/* Depend on eBand */
		prAisBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_MIXED_11BG;
	} else {
		/* Depend on eBand */
		prAisBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11AN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_11A;
	}

	/* 4 <1.5> Setup MIB for current BSS */
	prAisBssInfo->u2BeaconInterval = prConnSettings->u2BeaconPeriod;
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = prConnSettings->u2AtimWindow;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;

#if CFG_PRIVACY_MIGRATION
	if (prConnSettings->eEncStatus == ENUM_ENCRYPTION1_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION2_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION3_ENABLED) {
		prAisBssInfo->fgIsProtection = TRUE;
	} else {
		prAisBssInfo->fgIsProtection = FALSE;
	}
#else
	prAisBssInfo->fgIsProtection = FALSE;
#endif

	/* 3 <2> Update BSS_INFO_T common part */
	ibssInitForAdHoc(prAdapter, prAisBssInfo);

	/* 3 <3> Set MAC HW */
	/* 4 <3.1> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <3.2> use command packets to inform firmware */
	nicUpdateBss(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 4 <3.3> enable beaconing */
	bssUpdateBeaconContent(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 4 <3.4> Update AdHoc PM parameter */
	nicPmIndicateBssCreated(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 3 <4> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

	/* 3 <5> Start IBSS Alone Timer */
	cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer, SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));

	return;

}				/* end of aisCreateIBSS() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the existing IBSS was found.
*
* @param[in] prStaRec               Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisUpdateBssInfoForMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc;
	/* UINT_16 u2IELength; */
	/* PUINT_8 pucIE; */

	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);

	if (!prAisBssInfo->fgIsBeaconActivated) {

		/* 3 <1> Update BSS_INFO_T per Network Basis */
		/* 4 <1.1> Setup Operation Mode */
		prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

		/* 4 <1.2> Setup SSID */
		COPY_SSID(prAisBssInfo->aucSSID,
			  prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

		/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
		prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		prAisBssInfo->u2AssocId = 0;
	}
	/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
	/* 4 <2.1> Setup Capability */
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;	/* Use Peer's Cap Info as IBSS Cap Info */

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE) {
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
		prAisBssInfo->fgUseShortPreamble = TRUE;
	} else {
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;
		prAisBssInfo->fgUseShortPreamble = FALSE;
	}

	/* 7.3.1.4 For IBSS, the Short Slot Time subfield shall be set to 0. */
	prAisBssInfo->fgUseShortSlotTime = FALSE;	/* Set to FALSE for AdHoc */
	prAisBssInfo->u2CapInfo &= ~CAP_INFO_SHORT_SLOT_TIME;

	if (prAisBssInfo->u2CapInfo & CAP_INFO_PRIVACY)
		prAisBssInfo->fgIsProtection = TRUE;
	else
		prAisBssInfo->fgIsProtection = FALSE;

	/* 4 <2.2> Setup PHY Attributes and Basic Rate Set/Operational Rate Set */
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	rateGetDataRatesFromRateSet(prAisBssInfo->u2OperationalRateSet,
				    prAisBssInfo->u2BSSBasicRateSet,
				    prAisBssInfo->aucAllSupportedRates, &prAisBssInfo->ucAllSupportedRatesLen);

	/* 3 <3> X Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */

	/* 3 <4> Update BSS_INFO_T from BSS_DESC_T */
	prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;

		/* 4 <4.1> Setup BSSID */
		COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prBssDesc->aucBSSID);

		/* 4 <4.2> Setup Channel, Band */
		prAisBssInfo->ucPrimaryChannel = prBssDesc->ucChannelNum;
		prAisBssInfo->eBand = prBssDesc->eBand;

		/* 4 <4.3> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
		prAisBssInfo->ucDTIMPeriod = 0;
		prAisBssInfo->u2ATIMWindow = 0;	/* TBD(Kevin) */

		prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;
	} else {
		/* should never happen */
		ASSERT(0);
	}

	/* 3 <5> Set MAC HW */
	/* 4 <5.1> Find Lowest Basic Rate Index for default TX Rate of MMPDU */
	{
		UINT_8 ucLowestBasicRateIndex;

		if (!rateGetLowestRateIndexFromRateSet(prAisBssInfo->u2BSSBasicRateSet, &ucLowestBasicRateIndex)) {

			if (prAisBssInfo->ucPhyTypeSet & PHY_TYPE_BIT_OFDM)
				ucLowestBasicRateIndex = RATE_6M_INDEX;
			else
				ucLowestBasicRateIndex = RATE_1M_INDEX;
		}

		prAisBssInfo->ucHwDefaultFixedRateCode =
		    aucRateIndex2RateCode[prAisBssInfo->fgUseShortPreamble][ucLowestBasicRateIndex];
	}

	/* 4 <5.2> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <5.3> use command packets to inform firmware */
	nicUpdateBss(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 4 <5.4> enable beaconing */
	bssUpdateBeaconContent(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 4 <5.5> Update AdHoc PM parameter */
	nicPmIndicateBssConnected(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* 3 <6> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

}				/* end of aisUpdateBssInfoForMergeIBSS() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Probe Request Frame and then return
*        result to BSS to indicate if need to send the corresponding Probe Response
*        Frame if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval TRUE      Reply the Probe Response
* @retval FALSE     Don't reply the Probe Response
*/
/*----------------------------------------------------------------------------*/
BOOLEAN aisValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags)
{
	P_WLAN_MAC_MGMT_HEADER_T prMgtHdr;
	P_BSS_INFO_T prBssInfo;
	P_IE_SSID_T prIeSsid = (P_IE_SSID_T) NULL;
	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;
	BOOLEAN fgReplyProbeResp = FALSE;

	ASSERT(prSwRfb);
	ASSERT(pu4ControlFlags);

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	/* 4 <1> Parse Probe Req IE and Get IE ptr (SSID, Supported Rate IE, ...) */
	prMgtHdr = (P_WLAN_MAC_MGMT_HEADER_T) prSwRfb->pvHeader;

	u2IELength = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen;
	pucIE = (PUINT_8) prSwRfb->pvHeader + prSwRfb->u2HeaderLen;

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (IE_ID(pucIE) == ELEM_ID_SSID) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (P_IE_SSID_T) pucIE;
			break;
		}
	}			/* end of IE_FOR_EACH */

	/* 4 <2> Check network conditions */

	if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {

		if ((prIeSsid) && ((prIeSsid->ucLength == BC_SSID_LEN) ||	/* WILDCARD SSID */
				   EQUAL_SSID(prBssInfo->aucSSID, prBssInfo->ucSSIDLen,	/* CURRENT SSID */
					      prIeSsid->aucSSID, prIeSsid->ucLength))) {
			fgReplyProbeResp = TRUE;
		}
	}

	return fgReplyProbeResp;

}				/* end of aisValidateProbeReq() */

#endif /* CFG_SUPPORT_ADHOC */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will modify and update necessary information to firmware
*        for disconnection handling
*
* @param[in] prAdapter          Pointer to the Adapter structure.
*
* @retval None
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmDisconnect(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgDelayIndication)
{
	P_BSS_INFO_T prAisBssInfo;

	ASSERT(prAdapter);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	DBGLOG(AIS, INFO, "aisFsmDisconnect: ConnectionState=%d fgDelayIndication=%d\n"
		, prAisBssInfo->eConnectionState
		, fgDelayIndication);

	nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_AIS_INDEX);

#if CFG_SUPPORT_ADHOC
	if (prAisBssInfo->fgIsBeaconActivated) {
		nicUpdateBeaconIETemplate(prAdapter, IE_UPD_METHOD_DELETE_ALL, NETWORK_TYPE_AIS_INDEX, 0, NULL, 0);

		prAisBssInfo->fgIsBeaconActivated = FALSE;
	}
#endif

	rlmBssAborted(prAdapter, prAisBssInfo);

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and send Deauth if needed. */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		/* add for ctia mode */
		{
			UINT_8 aucP2pSsid[] = CTIA_MAGIC_SSID;

			if (EQUAL_SSID(aucP2pSsid, CTIA_MAGIC_SSID_LEN, prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen))
				nicEnterCtiaMode(prAdapter, FALSE, FALSE);
		}

		if (prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST) {
#if CFG_SUPPORT_RN
			if (prAisBssInfo->fgDisConnReassoc == FALSE)
#endif
				{
					scanRemoveBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

					/* remove from scanning results as well */
					wlanClearBssInScanningResult(prAdapter, prAisBssInfo->aucBSSID);
				}

			/* trials for re-association */
			if (fgDelayIndication) {
				DBGLOG(AIS, INFO, "try to do re-association due to radio lost!\n");
				aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);
			}
		} else {
			scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
		}

		if (fgDelayIndication) {
			if (prAisBssInfo->eCurrentOPMode != OP_MODE_IBSS)
				prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		} else {
			prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		}
	} else {
		prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
	}

	/* 4 <4> Change Media State immediately. */
	if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION) {
		aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

		/* 4 <4.1> sync. with firmware */
		nicUpdateBss(prAdapter, NETWORK_TYPE_AIS_INDEX);
	}

	if (!fgDelayIndication) {
		/* 4 <5> Deactivate previous AP's STA_RECORD_T or all Clients in Driver if have. */
		if (prAisBssInfo->prStaRecOfAP) {
			/* cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1); */

			prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		}
	}
#if CFG_SUPPORT_ROAMING
	roamingFsmRunEventAbort(prAdapter);

	/* clear pending roaming connection request */
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
#endif /* CFG_SUPPORT_ROAMING */

	/* 4 <6> Indicate Disconnected Event to Host */
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED, fgDelayIndication);

	/* 4 <7> Trigger AIS FSM */
	aisFsmSteps(prAdapter, AIS_STATE_IDLE);

}				/* end of aisFsmDisconnect() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of Scan done Time-Out to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
UINT_32 IsrCnt = 0, IsrPassCnt = 0, TaskIsrCnt = 0;
VOID aisFsmRunEventScanDoneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
#define SCAN_DONE_TIMEOUT_TIMES_LIMIT		20

	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_CONNECTION_SETTINGS_T prConnSettings;
	GL_HIF_INFO_T *HifInfo;
	UINT_32 u4FwCnt;
	P_GLUE_INFO_T prGlueInfo;

	DEBUGFUNC("aisFsmRunEventScanDoneTimeOut()");

	prGlueInfo = prAdapter->prGlueInfo;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	HifInfo = &prAdapter->prGlueInfo->rHifInfo;

	DBGLOG(AIS, WARN, "aisFsmRunEventScanDoneTimeOut Current[%d], ucScanTimeoutTimes=%d, u4NumElem=%d\n"
		, prAisFsmInfo->eCurrentState, ucScanTimeoutTimes
		, prAdapter->prGlueInfo->rCmdQueue.u4NumElem);
	DBGLOG(AIS, WARN, "Isr/task %u %u %u (0x%x)\n", prGlueInfo->IsrCnt, prGlueInfo->IsrPassCnt,
			prGlueInfo->TaskIsrCnt, prAdapter->fgIsIntEnable);

	/* dump firmware program counter */
	DBGLOG(AIS, WARN, "CONNSYS FW CPUINFO:\n");
	for (u4FwCnt = 0; u4FwCnt < 16; u4FwCnt++)
		DBGLOG(AIS, WARN, "0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR));

	/*dump firmware status */
	wlanDumpCommandFwStatus();

	/* dump TX_Description for Scan Request */
	/* (1) dump Tc4[0]~[3] for Scan Request before hal write */
	/* (2) dump Tc4[0]~[3] for Scan Request after hal write done  */
	/* if TC[X]ucOwn 1 -> 0,FW available and set 1, and Hardware receiced and set 0 */
	wlanDebugScanDump(prAdapter);

	ucScanTimeoutTimes++;
	if (ucScanTimeoutTimes > SCAN_DONE_TIMEOUT_TIMES_LIMIT) {
		kalSendAeeWarning("[Scan done timeout more than 20 times!]", __func__);
		GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
	}
#if 0 /* ALPS02018734: remove trigger assert */
	if (prAdapter->fgTestMode == FALSE) {
		/* Titus - xxx */
		/* assert if and only if in normal mode */
		mtk_wcn_wmt_assert(WMTDRV_TYPE_WIFI, 0x40);
	}
#endif
	/* report all scanned frames to upper layer to avoid scanned frame is timeout */
	/* must be put before kalScanDone */
/* scanReportBss2Cfg80211(prAdapter,BSS_TYPE_INFRASTRUCTURE,NULL); */

	prConnSettings->fgIsScanReqIssued = FALSE;
	kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_SCAN:
		prAisFsmInfo->u4ScanIELength = 0;
		eNextState = AIS_STATE_IDLE;
		break;
	case AIS_STATE_ONLINE_SCAN:
		/* reset scan IE buffer */
		prAisFsmInfo->u4ScanIELength = 0;
#if CFG_SUPPORT_ROAMING
		eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
		eNextState = AIS_STATE_NORMAL_TR;
#endif /* CFG_SUPPORT_ROAMING */
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
		scanGetCurrentEssChnlList(prAdapter);
#endif
		break;
	default:
		break;
	}

	/* try to stop scan in CONNSYS */
	aisFsmStateAbort_SCAN(prAdapter);
	aisRemoveOldestBcnTimeout(prAisFsmInfo);
	/* wlanQueryDebugCode(prAdapter); */ /* display current SCAN FSM in FW, debug use */

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				/* end of aisFsmBGSleepTimeout() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Background Scan Time-Out" to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventBGSleepTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DEBUGFUNC("aisFsmRunEventBGSleepTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		DBGLOG(AIS, LOUD, "EVENT - SCAN TIMER: Idle End - Current Time = %u\n", kalGetTimeTick());

		eNextState = AIS_STATE_LOOKING_FOR;

		SET_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);

		break;

	default:
		break;
	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				/* end of aisFsmBGSleepTimeout() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "IBSS ALONE Time-Out" to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventIbssAloneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DEBUGFUNC("aisFsmRunEventIbssAloneTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:

		/* There is no one participate in our AdHoc during this TIMEOUT Interval
		 * so go back to search for a valid IBSS again.
		 */

		DBGLOG(AIS, LOUD, "EVENT-IBSS ALONE TIMER: Start pairing\n");

		prAisFsmInfo->fgTryScan = TRUE;

		/* abort timer */
		aisFsmReleaseCh(prAdapter);

		/* Pull back to SEARCH to find candidate again */
		eNextState = AIS_STATE_SEARCH;

		break;

	default:
		break;
	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				/* end of aisIbssAloneTimeOut() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Join Time-Out" to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventJoinTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	OS_SYSTIME rCurrentTime;
	P_MSG_SAA_FSM_COMP_T prSaaFsmCompMsg;

	DEBUGFUNC("aisFsmRunEventJoinTimeout()");
	prAisBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX];
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	eNextState = prAisFsmInfo->eCurrentState;
	DBGLOG(AIS, INFO, "aisFsmRunEventJoinTimeout,CurrentState[%d]\n", prAisFsmInfo->eCurrentState);

	GET_CURRENT_SYSTIME(&rCurrentTime);
	switch (prAisFsmInfo->eCurrentState) {

	case AIS_STATE_JOIN:
		/* 1. Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter);
		aisAddBlacklist(prAdapter, prAisFsmInfo->prTargetBssDesc);
#if 0
		/* 2. Increase Join Failure Count */
		prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount++;
/* For JB nl802.11 */
		if (prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount < JOIN_MAX_RETRY_FAILURE_COUNT) {
			/* 3.1 Retreat to AIS_STATE_SEARCH state for next try */
			eNextState = AIS_STATE_SEARCH;
			/* restore tx power control */
			rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
		} else if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			/* 3.2 Retreat to AIS_STATE_WAIT_FOR_NEXT_SCAN state for next try */
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else if (prAisFsmInfo->rJoinReqTime != 0 &&
			   !CHECK_FOR_TIMEOUT(rCurrentTime,
					      prAisFsmInfo->rJoinReqTime,
					      SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
			/* 3.3 Retreat to AIS_STATE_WAIT_FOR_NEXT_SCAN state for next try */
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
			/* restore tx power control */
			rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
		} else {
			/* restore tx power control */
			rlmSetMaxTxPwrLimit(prAdapter, 0, 0);
			/* 3.4 Retreat to AIS_STATE_JOIN_FAILURE to terminate join operation */
			kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_JOIN_FAILURE, NULL, 0);
			eNextState = AIS_STATE_IDLE;
		}
#else
		/*request channel timeout, driver avoid to aisFsmStateInit_RetryJOIN*/
		if (prAisFsmInfo->ucAvailableAuthTypes != 0)
			DBGLOG(AIS, INFO, "driver avoids to aisFsmStateInit_RetryJOIN retry!\n");

		prAisFsmInfo->ucAvailableAuthTypes = 0;

		/* keep eNextState the same (so will not do action here), and do action in aisFsmRunEventJoinComplete */
		prSaaFsmCompMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SAA_FSM_COMP_T));
		if (!prSaaFsmCompMsg) {
			DBGLOG(AIS, WARN, "aisFsmRunEventJoinTimeout memory alloc fail!\n");
			return;
		}
		prSaaFsmCompMsg->rMsgHdr.eMsgId = MID_SAA_AIS_JOIN_COMPLETE;
		prSaaFsmCompMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfReqMsg;
		prSaaFsmCompMsg->rJoinStatus = WLAN_STATUS_FAILURE;
		prSaaFsmCompMsg->prStaRec = prAisFsmInfo->prTargetStaRec;
		prSaaFsmCompMsg->prSwRfb = NULL;
		/* joint complete only use it when rJoinStatus is WLAN_STATUS_SUCCESS*/
		/* NOTE(Kevin): Set to UNBUF for immediately JOIN complete */
		aisFsmRunEventJoinComplete(prAdapter, (P_MSG_HDR_T) prSaaFsmCompMsg);
		eNextState = prAisFsmInfo->eCurrentState;
#endif

		break;

	case AIS_STATE_NORMAL_TR:
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);
		prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

		/* 2. process if there is pending scan */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE) {
			wlanClearScanningResult(prAdapter);
			eNextState = AIS_STATE_ONLINE_SCAN;
		} else if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE)) {
			DBGLOG(AIS, INFO, "Upper layer trigger roaming, resume request!\n");
			eNextState = AIS_STATE_COLLECT_ESS_INFO;
		}

		break;

	default:
		/* release channel */
		aisFsmReleaseCh(prAdapter);
		break;

	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				/* end of aisFsmRunEventJoinTimeout() */

VOID aisFsmRunEventDeauthTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	aisDeauthXmitComplete(prAdapter, NULL, TX_RESULT_LIFE_TIMEOUT);
}
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisFsmRunEventSecModeChangeTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	DBGLOG(AIS, INFO, "Beacon security mode change timeout, trigger disconnect!\n");
	aisBssSecurityChanged(prAdapter);
}
#endif

#if defined(CFG_TEST_MGMT_FSM) && (CFG_TEST_MGMT_FSM != 0)
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisTest(VOID)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_8 aucSSID[] = "pci-11n";
	UINT_8 ucSSIDLen = 7;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* Set Connection Request Issued Flag */
	prConnSettings->fgIsConnReqIssued = TRUE;
	prConnSettings->ucSSIDLen = ucSSIDLen;
	kalMemCopy(prConnSettings->aucSSID, aucSSID, ucSSIDLen);

	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {

		ASSERT(0);	/* Can't trigger SCAN FSM */
		return;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_HEM_AIS_FSM_ABORT;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	wifi_send_msg(INDX_WIFI, MSG_ID_WIFI_IST, 0);

}
#endif /* CFG_TEST_MGMT_FSM */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle OID_802_11_BSSID_LIST_SCAN
*
* \param[in] prAdapter  Pointer of ADAPTER_T
* \param[in] prSsid     Pointer of SSID_T if specified
* \param[in] pucIe      Pointer to buffer of extra information elements to be attached
* \param[in] u4IeLength Length of information elements
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmScanRequest(IN P_ADAPTER_T prAdapter, IN P_PARAM_SSID_T prSsid, IN PUINT_8 pucIe, IN UINT_32 u4IeLength)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisFsmScanRequest()");

	ASSERT(prAdapter);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	DBGLOG(AIS, TRACE, "eCurrentState=%d, fgIsScanReqIssued = %d\n",
		prAisFsmInfo->eCurrentState, prConnSettings->fgIsScanReqIssued);
	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
		scanInitEssResult(prAdapter);

		if (prSsid == NULL) {
			prAisFsmInfo->ucScanSSIDLen = 0;
		} else {
			COPY_SSID(prAisFsmInfo->aucScanSSID,
				  prAisFsmInfo->ucScanSSIDLen, prSsid->aucSsid, (UINT_8) prSsid->u4SsidLen);
		}

		if (u4IeLength > 0 && u4IeLength <= MAX_IE_LENGTH) {
			prAisFsmInfo->u4ScanIELength = u4IeLength;
			kalMemCopy(prAisFsmInfo->aucScanIEBuf, pucIe, u4IeLength);
		} else {
			prAisFsmInfo->u4ScanIELength = 0;
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* 802.1x might not finished yet, pend it for later handling .. */
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(AIS, WARN,
					       "Scan Request with channel granted for join operation: %d, %d",
						prAisFsmInfo->fgIsChannelGranted, prAisFsmInfo->fgIsChannelRequested);
				}

				/* start online scan */
				wlanClearScanningResult(prAdapter);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
		}
	} else {
		DBGLOG(AIS, WARN, "Scan Request dropped. (state: %d)\n", prAisFsmInfo->eCurrentState);
	}

}				/* end of aisFsmScanRequest() */

#if CFG_MULTI_SSID_SCAN
/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle OID_802_11_BSSID_LIST_SCAN
*
* \param[in] prAdapter  Pointer of ADAPTER_T
* \param[in] ucSsidNum  Number of SSID
* \param[in] prSsid     Pointer to the array of SSID_T if specified
* \param[in] pucIe      Pointer to buffer of extra information elements to be attached
* \param[in] u4IeLength Length of information elements
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmScanRequestAdv(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSsidNum, IN P_PARAM_SSID_T prSsid,
				   IN PUINT_8 pucIe, IN UINT_32 u4IeLength, IN PUINT_8 paucRandomMac)
{
	UINT_32 i;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisFsmScanRequestAdv()");

	ASSERT(prAdapter);
	ASSERT(ucSsidNum <= SCN_SSID_MAX_NUM);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	DBGLOG(AIS, TRACE, "eCurrentState=%d, fgIsScanReqIssued = %d\n",
			prAisFsmInfo->eCurrentState, prConnSettings->fgIsScanReqIssued);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
		scanInitEssResult(prAdapter);

		if (ucSsidNum == 0)
			prAisFsmInfo->ucScanSSIDNum = 0;
		else {
			prAisFsmInfo->ucScanSSIDNum = ucSsidNum;

			for (i = 0; i < ucSsidNum; i++) {
				COPY_SSID(prAisFsmInfo->arScanSSID[i].aucSsid,
						prAisFsmInfo->arScanSSID[i].u4SsidLen,
						prSsid[i].aucSsid,
						prSsid[i].u4SsidLen);
			}
		}

		kalMemCopy(prAisFsmInfo->aucRandomMac, paucRandomMac,
			MAC_ADDR_LEN);

		if (u4IeLength > 0 && u4IeLength <= MAX_IE_LENGTH) {
			prAisFsmInfo->u4ScanIELength = u4IeLength;
			kalMemCopy(prAisFsmInfo->aucScanIEBuf, pucIe, u4IeLength);
		} else
			prAisFsmInfo->u4ScanIELength = 0;

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* 802.1x might not finished yet, pend it for later handling .. */
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(AIS, WARN,
					       "Scan Request with channel granted for join operation: %d, %d",
						prAisFsmInfo->fgIsChannelGranted, prAisFsmInfo->fgIsChannelRequested);
				}

				/* start online scan */
				wlanClearScanningResult(prAdapter);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
		}
	} else if (prAdapter->rWifiVar.rRmReqParams.rBcnRmParam.eState == RM_ON_GOING) {
		struct NORMAL_SCAN_PARAMS *prNormalScan = &prAdapter->rWifiVar.rRmReqParams.rBcnRmParam.rNormalScan;

		prNormalScan->fgExist = TRUE;
		if (ucSsidNum == 0) {
			prNormalScan->ucSsidNum = 0;
		} else {
			prNormalScan->ucSsidNum = ucSsidNum;

			for (i = 0; i < ucSsidNum; i++) {
				COPY_SSID(prNormalScan->arSSID[i].aucSsid,
					  prNormalScan->arSSID[i].u4SsidLen,
					  prSsid[i].aucSsid, prSsid[i].u4SsidLen);
			}
		}

		if (u4IeLength > 0 && u4IeLength <= MAX_IE_LENGTH) {
			prNormalScan->u4IELen = u4IeLength;
			kalMemCopy(prNormalScan->aucScanIEBuf, pucIe, u4IeLength);
		} else {
			prNormalScan->u4IELen = 0;
		}
		/* prNormalScan->fgFull2Partial = ucSetChannel; */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
		DBGLOG(AIS, INFO, "Buffer normal scan while Beacon request measurement\n");
	} else {
		DBGLOG(AIS, WARN, "Scan Request dropped. (state: %d)\n", prAisFsmInfo->eCurrentState);
	}
} /* end of aisFsmScanRequestAdv() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is invoked when CNM granted channel privilege
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_CH_GRANT_T prMsgChGrant;
	UINT_8 ucTokenID;
	UINT_32 u4GrantInterval;
#if (CFG_REFACTORY_PMKSA == 0)
	UINT_32 u4Entry = 0;
#endif

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;

	ucTokenID = prMsgChGrant->ucTokenID;
	u4GrantInterval = prMsgChGrant->u4GrantInterval;

	/* 1. free message */
	cnmMemFree(prAdapter, prMsgHdr);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN && prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/*release timer*/
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

		/* 3. state transition to join/ibss-alone/ibss-merge */
		/* 3.1 set timeout timer in cases join could not be completed */
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rJoinTimeoutTimer,
				   prAisFsmInfo->u4ChGrantedInterval - AIS_JOIN_CH_GRANT_THRESHOLD);
		/* 3.2 set local variable to indicate join timer is ticking */
		prAisFsmInfo->fgIsInfraChannelFinished = FALSE;
#if (CFG_REFACTORY_PMKSA == 0)
		/* 3.3 switch to join state */
		/* Three cases can switch to join state:
		** 1. There's an available PMKSA in wpa_supplicant
		** 2. found okc pmkid entry for this BSS
		** 3. current state is disconnected. In this case, supplicant may not get a valid pmksa,
		** so no pmkid will be passed to driver, so we no need to wait pmkid anyway.
		*/
		if (!prAdapter->rWifiVar.rConnSettings.fgOkcPmksaReady ||
		    (rsnSearchPmkidEntry(prAdapter, prAisFsmInfo->prTargetBssDesc->aucBSSID, &u4Entry) &&
		     prAdapter->rWifiVar.rAisSpecificBssInfo.arPmkidCache[u4Entry].fgPmkidExist))
#endif
			aisFsmSteps(prAdapter, AIS_STATE_JOIN);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL &&
		   prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

#if CFG_SUPPORT_NCHO
		if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE &&
			prAdapter->rNchoInfo.fgIsSendingAF == TRUE &&
			prAdapter->rNchoInfo.fgChGranted == FALSE) {
			DBGLOG(INIT, TRACE, "NCHO complete rAisChGrntComp trace time is %u\n", kalGetTimeTick());
			prAdapter->rNchoInfo.fgChGranted = TRUE;
			complete(&prAdapter->prGlueInfo->rAisChGrntComp);
		}
#endif
		/* 3.1 set timeout timer in cases upper layer cancel_remain_on_channel never comes */
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer, prAisFsmInfo->u4ChGrantedInterval);

		/* 3.2 switch to remain_on_channel state */
		aisFsmSteps(prAdapter, AIS_STATE_REMAIN_ON_CHANNEL);

		/* 3.3. indicate upper layer for channel ready */
		kalReadyOnChannel(prAdapter->prGlueInfo,
				  prAisFsmInfo->rChReqInfo.u8Cookie,
				  prAisFsmInfo->rChReqInfo.eBand,
				  prAisFsmInfo->rChReqInfo.eSco,
				  prAisFsmInfo->rChReqInfo.ucChannelNum, prAisFsmInfo->rChReqInfo.u4DurationMs);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else {		/* mismatched grant */
		/* 2. return channel privilege to CNM immediately */
		aisFsmReleaseCh(prAdapter);
	}

}				/* end of aisFsmRunEventChGrant() */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform CNM that channel privilege
*           has been released
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmReleaseCh(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_CH_ABORT_T prMsgChAbort;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	if (prAisFsmInfo->fgIsChannelGranted == TRUE || prAisFsmInfo->fgIsChannelRequested == TRUE) {

		prAisFsmInfo->fgIsChannelRequested = FALSE;
		prAisFsmInfo->fgIsChannelGranted = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChAbort = (P_MSG_CH_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_ABORT_T));
		if (!prMsgChAbort) {
			ASSERT(0);	/* Can't release Channel to CNM */
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;
		prMsgChAbort->ucTokenID = prAisFsmInfo->ucSeqNumOfChReq;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChAbort, MSG_SEND_METHOD_BUF);
	}

}				/* end of aisFsmReleaseCh() */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform AIS that corresponding beacon has not
*           been received for a while and probing is not successful
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisBssBeaconTimeout(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prAisBssInfo;
	BOOLEAN fgDoAbortIndication = FALSE;
	P_CONNECTION_SETTINGS_T prConnSettings;

	ASSERT(prAdapter);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Diagnose Connection for Beacon Timeout Event */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			P_STA_RECORD_T prStaRec = prAisBssInfo->prStaRecOfAP;

			if (prStaRec)
				fgDoAbortIndication = TRUE;
		} else if (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
			fgDoAbortIndication = TRUE;
		}
	}
	/* 4 <2> invoke abort handler */
	if (fgDoAbortIndication && (prAdapter->rWifiVar.rAisFsmInfo.u4PostponeIndStartTime == 0)) {
#if 0
		P_CONNECTION_SETTINGS_T prConnSettings;

		prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
		prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
#endif

		DBGLOG(AIS, INFO, "Beacon Timeout, Remove BSS [%pM]\n", prAisBssInfo->aucBSSID);
		scanRemoveBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

		/*
		 * Note: Cannot change TRUE to FALSE; or you will suffer the problem in
		 * ALPS01270257/ ALPS01804173
		 */
		if (prConnSettings->eReConnectLevel < RECONNECT_LEVEL_USER_SET) {
			prConnSettings->eReConnectLevel = RECONNECT_LEVEL_BEACON_TIMEOUT;
			prConnSettings->fgIsConnReqIssued = TRUE;
		}
		aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_RADIO_LOST, TRUE);
	}

}				/* end of aisBssBeaconTimeout() */

VOID aisBssSecurityChanged(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	prAdapter->rWifiVar.rConnSettings.fgIsDisconnectedByNonRequest = TRUE;
	/* To avoid unable to report disconnect to the upper layer */
	prAisBssInfo->fgDisConnReassoc = FALSE;
	prAisBssInfo->u2DeauthReason = REASON_CODE_BSS_SECURITY_CHANGE;
	aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_DEAUTHENTICATED, FALSE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform AIS that DEAUTH frame has been
*           sent and thus state machine could go ahead
*
* \param[in] prAdapter  Pointer of ADAPTER_T
* \param[in] prMsduInfo Pointer of MSDU_INFO_T for DEAUTH frame
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
aisDeauthXmitComplete(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_DISCONNECTING) {
		if (rTxDoneStatus != TX_RESULT_DROPPED_IN_DRIVER)
			aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_NEW_CONNECTION, FALSE);
	} else {
		DBGLOG(AIS, WARN, "DEAUTH frame transmitted without further handling");
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of aisDeauthXmitComplete() */

#if CFG_SUPPORT_ROAMING
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Looking for a candidate due to weak signal" to AIS FSM.
*
* @param[in] u4ReqScan  Requesting Scan or not
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventRoamingDiscovery(IN P_ADAPTER_T prAdapter, UINT_32 u4ReqScan)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	ENUM_AIS_REQUEST_TYPE_T eAisRequest;
	P_BSS_INFO_T prAisBssInfo = NULL;

	DBGLOG(AIS, LOUD, "aisFsmRunEventRoamingDiscovery()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* search candidates by best rssi */
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

#if CFG_SUPPORT_WFD
#if CFG_ENABLE_WIFI_DIRECT
	{
		/* Check WFD is running */
		P_BSS_INFO_T prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;

		if (prAdapter->fgIsP2PRegistered &&
		    IS_BSS_ACTIVE(prP2pBssInfo) &&
		    (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT ||
		     prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE)) {
			DBGLOG(ROAMING, INFO, "Handle roaming when P2P is GC or GO.\n");
			if (prAdapter->rWifiVar.prP2pFsmInfo) {
				prWfdCfgSettings = &(prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings);
				if ((prWfdCfgSettings->ucWfdEnable == 1) &&
				    ((prWfdCfgSettings->u4WfdFlag & WFD_FLAGS_DEV_INFO_VALID))) {
					DBGLOG(ROAMING, INFO, "WFD is running. Stop roaming.\n");
					roamingFsmRunEventRoam(prAdapter);
					roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_NOCANDIDATE);
					return;
				}
			} else {
				ASSERT(0);
			}
		}		/* fgIsP2PRegistered */
	}
#endif
#endif

	/* results are still new */
	if (!u4ReqScan) {
		roamingFsmRunEventRoam(prAdapter);
		eAisRequest = AIS_REQUEST_ROAMING_CONNECT;
	} else {
		if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN
		    || prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
			eAisRequest = AIS_REQUEST_ROAMING_CONNECT;
		} else {
			eAisRequest = AIS_REQUEST_ROAMING_SEARCH;
		}
		/* if AP supports Bss Transition Mgmt, then send BSS Transition Query frame to obtain possible
		* Neighbor AP report, which can used to assist roaming candicate selection
		*/
		if (prAisFsmInfo->prTargetStaRec && prAisFsmInfo->prTargetStaRec->fgSupportBTM) {
			prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam.ucDialogToken =
				wnmGetBtmToken();
			prAdapter->rWifiVar.rAisSpecificBssInfo.rBTMParam.ucQueryReason =
				BSS_TRANSITION_LOW_RSSI;
			prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
			wnmSendBTMQueryFrame(prAdapter, prAisBssInfo->prStaRecOfAP);
		}
	}

	if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR && prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
		if (eAisRequest == AIS_REQUEST_ROAMING_SEARCH) {
			prAisFsmInfo->fgTargetChnlScanIssued = TRUE;
			aisFsmSteps(prAdapter, AIS_STATE_LOOKING_FOR);
		} else
			aisFsmSteps(prAdapter, AIS_STATE_COLLECT_ESS_INFO);
	} else {
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);

		aisFsmInsertRequest(prAdapter, eAisRequest);
	}

}				/* end of aisFsmRunEventRoamingDiscovery() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Update the time of ScanDone for roaming and transit to Roam state.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
ENUM_AIS_STATE_T aisFsmRoamingScanResultsUpdate(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DBGLOG(AIS, LOUD, "->aisFsmRoamingScanResultsUpdate()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	roamingFsmScanResultsUpdate(prAdapter);

	eNextState = prAisFsmInfo->eCurrentState;
	if (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) {
		roamingFsmRunEventRoam(prAdapter);
		eNextState = AIS_STATE_COLLECT_ESS_INFO;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
		eNextState = AIS_STATE_COLLECT_ESS_INFO;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
		eNextState = AIS_STATE_NORMAL_TR;
	}

	return eNextState;
}				/* end of aisFsmRoamingScanResultsUpdate() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will modify and update necessary information to firmware
*        for disconnection of last AP before switching to roaming bss.
*
* @param IN prAdapter          Pointer to the Adapter structure.
*           prTargetStaRec     Target of StaRec of roaming
*
* @retval None
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRoamingDisconnectPrevAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prTargetStaRec)
{
	P_BSS_INFO_T prAisBssInfo;

	DBGLOG(AIS, LOUD, "aisFsmRoamingDisconnectPrevAP()");

	ASSERT(prAdapter);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	if (prAisBssInfo->prStaRecOfAP != prTargetStaRec)
		wmmNotifyDisconnected(prAdapter);
	nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_AIS_INDEX);

	/* Not invoke rlmBssAborted() here to avoid prAisBssInfo->fg40mBwAllowed
	 * to be reset. RLM related parameters will be reset again when handling
	 * association response in rlmProcessAssocRsp(). 20110413
	 */
	/* rlmBssAborted(prAdapter, prAisBssInfo); */

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and send Deauth if needed. */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
		scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
	/* 4 <4> Change Media State immediately. */
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECT_PREV);

	/* 4 <4.1> sync. with firmware */
	prTargetStaRec->ucNetTypeIndex = 0xff;	/* Virtial NetType */
	nicUpdateBss(prAdapter, NETWORK_TYPE_AIS_INDEX);
	prTargetStaRec->ucNetTypeIndex = NETWORK_TYPE_AIS_INDEX;	/* Virtial NetType */
	/* before deactivate previous AP, should move its pending MSDUs to the new AP */
	if (prAisBssInfo->prStaRecOfAP)
		if (prAisBssInfo->prStaRecOfAP != prTargetStaRec && prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			qmMoveStaTxQueue(prAisBssInfo->prStaRecOfAP, prTargetStaRec);
			cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP, FALSE);
		} else
			DBGLOG(AIS, WARN, "prStaRecOfAP is in use %d\n", prAisBssInfo->prStaRecOfAP->fgIsInUse);
	else
		DBGLOG(AIS, WARN, "NULL pointer of prAisBssInfo->prStaRecOfAP\n");
	kalClearSecurityFramesByNetType(prAdapter->prGlueInfo, NETWORK_TYPE_AIS_INDEX);
#if (CFG_SUPPORT_TDLS == 1)
	TdlsexLinkHistoryRecord(prAdapter->prGlueInfo, TRUE, prAisBssInfo->aucBSSID,
				TRUE, TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_ROAMING);
#endif /* CFG_SUPPORT_TDLS */

}				/* end of aisFsmRoamingDisconnectPrevAP() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the roaming was completed.
*
* @param IN prAdapter          Pointer to the Adapter structure.
*           prStaRec           StaRec of roaming AP
*           prAssocRspSwRfb
*
* @retval None
*/
/*----------------------------------------------------------------------------*/
VOID aisUpdateBssInfoForRoamingAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prAssocRspSwRfb)
{
	P_BSS_INFO_T prAisBssInfo;

	DBGLOG(AIS, LOUD, "aisUpdateBssInfoForRoamingAP()");

	ASSERT(prAdapter);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	/* 4 <1.1> Change FW's Media State immediately. */
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

	/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
	if ((prAisBssInfo->prStaRecOfAP) &&
	    (prAisBssInfo->prStaRecOfAP != prStaRec) && (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {
		/* before deactivate previous AP, should move its pending MSDUs to the new AP */
		qmMoveStaTxQueue(prAisBssInfo->prStaRecOfAP, prStaRec);
		cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1);
	}
	/* 4 <1.3> Update BSS_INFO_T */
	aisUpdateBssInfoForJOIN(prAdapter, prStaRec, prAssocRspSwRfb);

	/* 4 <1.4> Activate current AP's STA_RECORD_T in Driver. */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

	/* 4 <1.6> Indicate Connected Event to Host immediately. */
	/* Require BSSID, Association ID, Beacon Interval.. from AIS_BSS_INFO_T */
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

}				/* end of aisFsmRoamingUpdateBss() */

#endif /* CFG_SUPPORT_ROAMING */

/*----------------------------------------------------------------------------*/
/*!
* @brief Check if there is any pending request and remove it (optional)
*
* @param prAdapter
*        eReqType
*        bRemove
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN aisFsmIsRequestPending(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType, IN BOOLEAN bRemove)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_REQ_HDR_T prPendingReqHdr, prPendingReqHdrNext;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* traverse through pending request list */
	LINK_FOR_EACH_ENTRY_SAFE(prPendingReqHdr,
				 prPendingReqHdrNext, &(prAisFsmInfo->rPendingReqList), rLinkEntry, AIS_REQ_HDR_T) {
		/* check for specified type */
		if (prPendingReqHdr->eReqType == eReqType) {
			/* check if need to remove */
			if (bRemove == TRUE) {
				LINK_REMOVE_KNOWN_ENTRY(&(prAisFsmInfo->rPendingReqList),
							&(prPendingReqHdr->rLinkEntry));
				if (eReqType == AIS_REQUEST_SCAN) {
					if (prPendingReqHdr->pu8ChannelInfo != NULL) {
						DBGLOG(AIS, INFO, "scan req pu8ChannelInfo no NULL\n");
						prAdapter->prGlueInfo->puScanChannel = prPendingReqHdr->pu8ChannelInfo;
						prPendingReqHdr->pu8ChannelInfo = NULL;
					}
				}
				cnmMemFree(prAdapter, prPendingReqHdr);
			}

			return TRUE;
		}
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Get next pending request
*
* @param prAdapter
*
* @return P_AIS_REQ_HDR_T
*/
/*----------------------------------------------------------------------------*/
P_AIS_REQ_HDR_T aisFsmGetNextRequest(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_REQ_HDR_T prPendingReqHdr;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	DBGLOG(AIS, INFO, "aisFsmGetNextRequest\n");

	LINK_REMOVE_HEAD(&(prAisFsmInfo->rPendingReqList), prPendingReqHdr, P_AIS_REQ_HDR_T);
	/* save partial scan puScanChannel info to prGlueInfo */
	if ((prPendingReqHdr != NULL) && (prAdapter->prGlueInfo != NULL)) {
		if (prAdapter->prGlueInfo->puScanChannel != NULL)
			DBGLOG(INIT, TRACE, "prGlueInfo error puScanChannel=%p", prAdapter->prGlueInfo->puScanChannel);

		if (prPendingReqHdr->pu8ChannelInfo != NULL) {
			prAdapter->prGlueInfo->puScanChannel = prPendingReqHdr->pu8ChannelInfo;
			DBGLOG(AIS, INFO, "aisFsmGetNextRequest pu8ChannelInfo NOT NULL, SAVE\n");
			prPendingReqHdr->pu8ChannelInfo = NULL;
		}
	}
	return prPendingReqHdr;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Insert a new request
*
* @param prAdapter
*        eReqType
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN aisFsmInsertRequest(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType)
{
	P_AIS_REQ_HDR_T prAisReq;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	prAisReq = (P_AIS_REQ_HDR_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(AIS_REQ_HDR_T));

	if (!prAisReq) {
		ASSERT(0);	/* Can't generate new message */
		return FALSE;
	}
	DBGLOG(AIS, TRACE, "aisFsmInsertRequest\n");

	prAisReq->eReqType = eReqType;
	prAisReq->pu8ChannelInfo = NULL;
	/* save partial scan puScanChannel info to pending scan */
	if ((prAdapter->prGlueInfo != NULL) &&
		(prAdapter->prGlueInfo->puScanChannel != NULL)) {
		DBGLOG(AIS, INFO, "aisFsmInsertRequest puScanChannel NOT NULL, SAVE\n");
		prAisReq->pu8ChannelInfo = prAdapter->prGlueInfo->puScanChannel;
		prAdapter->prGlueInfo->puScanChannel = NULL;
	}
	/* attach request into pending request list */
	LINK_INSERT_TAIL(&prAisFsmInfo->rPendingReqList, &prAisReq->rLinkEntry);

	DBGLOG(AIS, INFO, "eCurrentState=%d, eReqType = %d, u4NumElem=%d\n",
		prAisFsmInfo->eCurrentState, eReqType, prAisFsmInfo->rPendingReqList.u4NumElem);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Flush all pending requests
*
* @param prAdapter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmFlushRequest(IN P_ADAPTER_T prAdapter)
{
	P_AIS_REQ_HDR_T prAisReq;

	ASSERT(prAdapter);

	while ((prAisReq = aisFsmGetNextRequest(prAdapter)) != NULL) {
		/* for partional scan, if channel infor exist, free channel info */
		if (prAisReq->pu8ChannelInfo != NULL)
			kalMemFree(prAisReq->pu8ChannelInfo, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));

		cnmMemFree(prAdapter, prAisReq);
	}

}

VOID aisFsmRunEventRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_REMAIN_ON_CHANNEL_T prRemainOnChannel;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prRemainOnChannel = (P_MSG_REMAIN_ON_CHANNEL_T) prMsgHdr;

	/* record parameters */
	prAisFsmInfo->rChReqInfo.eBand = prRemainOnChannel->eBand;
	prAisFsmInfo->rChReqInfo.eSco = prRemainOnChannel->eSco;
	prAisFsmInfo->rChReqInfo.ucChannelNum = prRemainOnChannel->ucChannelNum;
	prAisFsmInfo->rChReqInfo.u4DurationMs = prRemainOnChannel->u4DurationMs;
	prAisFsmInfo->rChReqInfo.u8Cookie = prRemainOnChannel->u8Cookie;

	if ((prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) ||
		(prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR
		&& prAisFsmInfo->fgIsInfraChannelFinished == TRUE)) {
		/* transit to next state */
		aisFsmSteps(prAdapter, AIS_STATE_REQ_REMAIN_ON_CHANNEL);
	} else {
		aisFsmInsertRequest(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL);
	}

	/* free messages */
	cnmMemFree(prAdapter, prMsgHdr);

}

VOID aisFsmRunEventCancelRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_MSG_CANCEL_REMAIN_ON_CHANNEL_T prCancelRemainOnChannel;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	prCancelRemainOnChannel = (P_MSG_CANCEL_REMAIN_ON_CHANNEL_T) prMsgHdr;

	/* 1. Check the cookie first */
	if (prCancelRemainOnChannel->u8Cookie == prAisFsmInfo->rChReqInfo.u8Cookie) {

		/* 2. release channel privilege/request */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL) {
			/* 2.1 elease channel */
			aisFsmReleaseCh(prAdapter);
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
			/* 2.1 release channel */
			aisFsmReleaseCh(prAdapter);

			/* 2.2 stop channel timeout timer */
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
		}

		/* 3. clear pending request of remain_on_channel */
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE);

		/* 4. decide which state to retreat */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL ||
			prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
				aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
			else
				aisFsmSteps(prAdapter, AIS_STATE_IDLE);
		}
	}

	/* 5. free message */
	cnmMemFree(prAdapter, prMsgHdr);

}

VOID aisFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_MGMT_TX_REQUEST_T prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		/* prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]); */

		if (prAisFsmInfo == NULL)
			break;
		prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) prMsgHdr;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo, prMgmtTxMsg->prMgmtMsduInfo, prMgmtTxMsg->u8Cookie);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* aisFsmRunEventMgmtFrameTx */

#if CFG_SUPPORT_NCHO
VOID aisFsmRunEventNchoActionFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_MGMT_TX_REQUEST_T prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;
	P_ACTION_VENDOR_SPEC_FRAME_T prVendorSpec = NULL;
	PUINT_8 pucFrameBuf = (PUINT_8) NULL;
	P_NCHO_INFO prNchoInfo = NULL;
	UINT_16 u2PktLen = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));
		DBGLOG(REQ, TRACE, "NCHO in aisFsmRunEventNchoActionFrameTx\n");

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prNchoInfo = &(prAdapter->rNchoInfo);
		prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

		if (prAisFsmInfo == NULL)
			break;

		prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) prMsgHdr;
		u2PktLen = (UINT_16) OFFSET_OF(ACTION_VENDOR_SPEC_FRAME_T, aucElemInfo[0]) +
				  prNchoInfo->rParamActionFrame.i4len +
				  MAC_TX_RESERVED_FIELD;
		prMgmtFrame = cnmMgtPktAlloc(prAdapter, u2PktLen);
		if (prMgmtFrame == NULL) {
			ASSERT(FALSE);
			DBGLOG(REQ, ERROR, "NCHO there is no memory for prMgmtFrame\n");
			break;
		}
		prMgmtTxMsg->prMgmtMsduInfo = prMgmtFrame;

		pucFrameBuf = (PUINT_8) ((ULONG) prMgmtFrame->prPacket + MAC_TX_RESERVED_FIELD);
		prVendorSpec = (P_ACTION_VENDOR_SPEC_FRAME_T)pucFrameBuf;
		prVendorSpec->u2FrameCtrl = MAC_FRAME_ACTION;
		prVendorSpec->u2Duration = 0;
		prVendorSpec->u2SeqCtrl = 0;
		COPY_MAC_ADDR(prVendorSpec->aucDestAddr, prNchoInfo->rParamActionFrame.aucBssid);
		COPY_MAC_ADDR(prVendorSpec->aucSrcAddr, prAisBssInfo->aucOwnMacAddr);
		COPY_MAC_ADDR(prVendorSpec->aucBSSID, prAisBssInfo->aucBSSID);

		kalMemCopy(prVendorSpec->aucElemInfo,
			   prNchoInfo->rParamActionFrame.aucData,
			   prNchoInfo->rParamActionFrame.i4len);

		prMgmtFrame->u2FrameLength = u2PktLen;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo, prMgmtTxMsg->prMgmtMsduInfo, prMgmtTxMsg->u8Cookie);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* aisFsmRunEventNchoActionFrameTx */
#endif

VOID aisFsmRunEventChannelTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	DBGLOG(AIS, INFO, "aisFsmRunEventChannelTimeout, CurrentState = [%d]\n"
		, prAisFsmInfo->eCurrentState);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		/* 3. expiration indication to upper layer */
		kalRemainOnChannelExpired(prAdapter->prGlueInfo,
					  prAisFsmInfo->rChReqInfo.u8Cookie,
					  prAisFsmInfo->rChReqInfo.eBand,
					  prAisFsmInfo->rChReqInfo.eSco, prAisFsmInfo->rChReqInfo.ucChannelNum);

		/* 4. decide which state to retreat */
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN) {
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		prAisFsmInfo->fgIsInfraChannelFinished = FALSE;

		prAisFsmInfo->fgIsChannelGranted = FALSE;
		/* 3 switch to idle state */
		aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	} else {
		DBGLOG(AIS, WARN, "Unexpected remain_on_channel timeout event\n");
#if DBG
		DBGLOG(AIS, STATE, "CURRENT State: [%s]\n", apucDebugAisState[prAisFsmInfo->eCurrentState]);
#else
		DBGLOG(AIS, STATE, "[%d] CURRENT State: [%d]\n", DBG_AIS_IDX, prAisFsmInfo->eCurrentState);
#endif
	}

}

WLAN_STATUS
aisFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter,
			      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo = (P_AIS_MGMT_TX_REQ_INFO_T) NULL;
	BOOLEAN fgIsSuccess = FALSE;
	P_WLAN_MAC_HEADER_T prWlanFrame = NULL;
	P_WLAN_ACTION_FRAME prActFrame = NULL;
	UINT_16 u2TxFrameCtrl = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prMgmtTxReqInfo = &(prAisFsmInfo->rMgmtTxInfo);
		prWlanFrame = (P_WLAN_MAC_HEADER_T) prMsduInfo->prPacket;
		u2TxFrameCtrl = (prWlanFrame->u2FrameCtrl) & MASK_FRAME_TYPE;

		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(AIS, ERROR, "Mgmt Frame TX Fail, Status:%d.\n", rTxDoneStatus);
		} else {
			fgIsSuccess = TRUE;
#if CFG_SUPPORT_NCHO
			if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE &&
				prAdapter->rNchoInfo.fgIsSendingAF == TRUE &&
				prAdapter->rNchoInfo.fgChGranted == TRUE) {
				prAdapter->rNchoInfo.fgIsSendingAF = FALSE;
				DBGLOG(AIS, TRACE, "NCHO action frame tx done");
			}
#endif
			/* printk("Mgmt Frame TX Done.\n"); */
		}

		if (prMgmtTxReqInfo->prMgmtTxMsdu == prMsduInfo) {
			if (u2TxFrameCtrl == MAC_FRAME_ACTION) {
				prActFrame = (P_WLAN_ACTION_FRAME) prMsduInfo->prPacket;
				if (prActFrame->ucCategory == CATEGORY_PUBLIC_ACTION) {
					switch (prActFrame->ucAction) {
					case PUBLIC_ACTION_GAS_INITIAL_REQ:
						DBGLOG(AIS, INFO, "Send GAS Initial Request frame successfully\n");
						break;
					case PUBLIC_ACTION_GAS_INITIAL_RESP:
						DBGLOG(AIS, INFO, "Send GAS Initial Response frame successfully\n");
						break;
					default:
						DBGLOG(AIS, TRACE, "Send other public action frame(%u) successfully\n",
							prActFrame->ucAction);
						break;
					}
				}
			}
			kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
						prMgmtTxReqInfo->u8Cookie,
						fgIsSuccess, prMsduInfo->prPacket, (UINT_32) prMsduInfo->u2FrameLength);

			prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
		}

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* aisFsmRunEventMgmtFrameTxDone */

#if (CFG_REFACTORY_PMKSA == 0)
VOID aisFsmRunEventSetOkcPmk(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;

	prAdapter->rWifiVar.rConnSettings.fgOkcPmksaReady = TRUE;
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN &&
		prAisFsmInfo->fgIsChannelGranted)
		aisFsmSteps(prAdapter, AIS_STATE_JOIN);
}
#endif

static VOID aisFsmSetOkcTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;

	DBGLOG(AIS, WARN, "Wait OKC PMKID timeout, current state[%d],fgIsChannelGranted=%d\n"
		, prAisFsmInfo->eCurrentState, prAisFsmInfo->fgIsChannelGranted);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN &&
		prAisFsmInfo->fgIsChannelGranted)
		aisFsmSteps(prAdapter, AIS_STATE_JOIN);
}

WLAN_STATUS
aisFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter,
		   IN P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo, IN P_MSDU_INFO_T prMgmtTxMsdu, IN UINT_64 u8Cookie)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_MSDU_INFO_T prTxMsduInfo = (P_MSDU_INFO_T) NULL;
	P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMgmtTxReqInfo != NULL));

		if (prMgmtTxReqInfo->fgIsMgmtTxRequested) {

			/* 1. prMgmtTxReqInfo->prMgmtTxMsdu != NULL */
			/* Packet on driver, not done yet, drop it. */
			prTxMsduInfo = prMgmtTxReqInfo->prMgmtTxMsdu;
			if (prTxMsduInfo != NULL) {

				kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
							prMgmtTxReqInfo->u8Cookie,
							FALSE,
							prTxMsduInfo->prPacket, (UINT_32) prTxMsduInfo->u2FrameLength);

				/* Leave it to TX Done handler. */
				/* cnmMgtPktFree(prAdapter, prTxMsduInfo); */
				prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
			}
			/* 2. prMgmtTxReqInfo->prMgmtTxMsdu == NULL */
			/* Packet transmitted, wait tx done. (cookie issue) */
		}

		ASSERT(prMgmtTxReqInfo->prMgmtTxMsdu == NULL);

		prWlanHdr = (P_WLAN_MAC_HEADER_T) ((ULONG) prMgmtTxMsdu->prPacket + MAC_TX_RESERVED_FIELD);
		prStaRec = cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_AIS_INDEX, prWlanHdr->aucAddr1);
		prMgmtTxMsdu->ucNetworkType = (UINT_8) NETWORK_TYPE_AIS_INDEX;

		prMgmtTxReqInfo->u8Cookie = u8Cookie;
		prMgmtTxReqInfo->prMgmtTxMsdu = prMgmtTxMsdu;
		prMgmtTxReqInfo->fgIsMgmtTxRequested = TRUE;

		prMgmtTxMsdu->eSrc = TX_PACKET_MGMT;
		prMgmtTxMsdu->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;
		prMgmtTxMsdu->ucStaRecIndex = (prStaRec != NULL) ? (prStaRec->ucIndex) : (0xFF);
		if (prStaRec != NULL) {
			/* Do nothing */
			/* printk("Mgmt with station record: %pM .\n", prStaRec->aucMacAddr); */
		}

		prMgmtTxMsdu->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;	/* TODO: undcertain. */
		prMgmtTxMsdu->fgIs802_1x = FALSE;
		prMgmtTxMsdu->fgIs802_11 = TRUE;
		prMgmtTxMsdu->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
		prMgmtTxMsdu->pfTxDoneHandler = aisFsmRunEventMgmtFrameTxDone;
		prMgmtTxMsdu->fgIsBasicRate = TRUE;
		DBGLOG(AIS, TRACE, "Mgmt seq NO. %d .\n", prMgmtTxMsdu->ucTxSeqNum);

		nicTxEnqueueMsdu(prAdapter, prMgmtTxMsdu);

	} while (FALSE);

	return rWlanStatus;
}				/* aisFuncTxMgmtFrame */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Action Frame and indicate to uppoer layer
*           if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval none
*/
/*----------------------------------------------------------------------------*/
VOID aisFuncValidateRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = (P_AIS_FSM_INFO_T) NULL;

	DEBUGFUNC("aisFuncValidateRxActionFrame");

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (1 /* prAisFsmInfo->u4AisPacketFilter & PARAM_PACKET_FILTER_ACTION_FRAME */) {
			/* Leave the action frame to wpa_supplicant. */
			kalIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb);
		}

	} while (FALSE);

	return;

}				/* aisFuncValidateRxActionFrame */

VOID aisRefreshFWKBlacklist(P_ADAPTER_T prAdapter)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	DBGLOG(AIS, INFO, "Refresh all the BSSes' fgIsInFWKBlacklist to FALSE\n");
	LINK_FOR_EACH_ENTRY(prEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		prEntry->fgIsInFWKBlacklist = FALSE;
	}
}

struct AIS_BLACKLIST_ITEM *
aisAddBlacklist(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	if (!prBssDesc) {
		DBGLOG(AIS, ERROR, "bss descriptor is NULL\n");
		return NULL;
	}
	if (prBssDesc->prBlack) {
		GET_CURRENT_SYSTIME(&prBssDesc->prBlack->rAddTime);
		prBssDesc->prBlack->ucCount++;
		DBGLOG(AIS, INFO, "update blacklist for %pM, count %d\n",
			prBssDesc->aucBSSID, prBssDesc->prBlack->ucCount);
		return prBssDesc->prBlack;
	}

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);

	if (prEntry) {
		GET_CURRENT_SYSTIME(&prEntry->rAddTime);
		prBssDesc->prBlack = prEntry;
		prEntry->ucCount++;
		DBGLOG(AIS, INFO, "update blacklist for %pM, count %d\n",
			prBssDesc->aucBSSID, prEntry->ucCount);
		return prEntry;
	}

	LINK_REMOVE_HEAD(prFreeList, prEntry, struct AIS_BLACKLIST_ITEM *);
	if (!prEntry)
		prEntry = kalMemAlloc(sizeof(struct AIS_BLACKLIST_ITEM), VIR_MEM_TYPE);
	if (!prEntry) {
		DBGLOG(AIS, WARN, "No memory to allocate\n");
		return NULL;
	}
	kalMemZero(prEntry, sizeof(*prEntry));
	prEntry->ucCount = 1;
	prEntry->fgIsInFWKBlacklist = FALSE;
	COPY_MAC_ADDR(prEntry->aucBSSID, prBssDesc->aucBSSID);
	COPY_SSID(prEntry->aucSSID, prEntry->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	GET_CURRENT_SYSTIME(&prEntry->rAddTime);
	LINK_INSERT_HEAD(prBlackList, &prEntry->rLinkEntry);
	prBssDesc->prBlack = prEntry;

	DBGLOG(AIS, INFO, "Add %pM to black List\n", prBssDesc->aucBSSID);
	return prEntry;
}

VOID aisRemoveBlackList(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);
	if (!prEntry)
		return;
	LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
	LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	prBssDesc->prBlack = NULL;
	DBGLOG(AIS, INFO, "Remove %pM from blacklist\n", prBssDesc->aucBSSID);
}

struct AIS_BLACKLIST_ITEM *
aisQueryBlackList(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	if (!prBssDesc)
		return NULL;
	else if (prBssDesc->prBlack)
		return prBssDesc->prBlack;

	LINK_FOR_EACH_ENTRY(prEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry->aucBSSID) &&
			EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			prEntry->aucSSID, prEntry->ucSSIDLen)) {
			prBssDesc->prBlack = prEntry;
			return prEntry;
		}
	}
	DBGLOG(AIS, TRACE, "%pM is not in blacklist\n", prBssDesc->aucBSSID);
	return NULL;
}

VOID aisRemoveTimeoutBlacklist(P_ADAPTER_T prAdapter)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	OS_SYSTIME rCurrent;
	P_BSS_DESC_T prBssDesc = NULL;

	GET_CURRENT_SYSTIME(&rCurrent);

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		if (prEntry->fgIsInFWKBlacklist == TRUE)
			continue;
		if (!CHECK_FOR_TIMEOUT(rCurrent, prEntry->rAddTime, SEC_TO_MSEC(AIS_BLACKLIST_TIMEOUT)))
			continue;

		prBssDesc = scanSearchBssDescByBssid(prAdapter,
						     prEntry->aucBSSID);
		if (prBssDesc) {
			prBssDesc->prBlack = NULL;
			DBGLOG(AIS, INFO, "Remove Timeout %pM from blacklist\n",
			       prBssDesc->aucBSSID);
		}
		LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
		LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	}
}

static VOID aisRemoveDisappearedBlacklist(P_ADAPTER_T prAdapter)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	P_BSS_DESC_T prBssDesc = NULL;
	P_LINK_T prBSSDescList = &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	UINT_32 u4Current = (UINT_32)kalGetBootTime();
	BOOLEAN fgDisappeared = TRUE;

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		fgDisappeared = TRUE;
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
			if (prBssDesc->prBlack == prEntry || (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry->aucBSSID) &&
				EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
				prEntry->aucSSID, prEntry->ucSSIDLen))) {
				fgDisappeared = FALSE;
				break;
			}
		}
		if (!fgDisappeared || (u4Current - prEntry->u4DisapperTime) < 600 * USEC_PER_SEC)
			continue;

		prBssDesc = scanSearchBssDescByBssid(prAdapter,
						     prEntry->aucBSSID);
		if (prBssDesc) {
			prBssDesc->prBlack = NULL;
			DBGLOG(AIS, INFO,
			       "Remove disappeared blacklist %s %pM\n",
			       prEntry->aucSSID, prEntry->aucBSSID);
		}
		LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
		LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	}
}

BOOLEAN aisApOverload(struct AIS_BLACKLIST_ITEM *prBlack)
{
	switch (prBlack->u2AuthStatus) {
	case STATUS_CODE_ASSOC_DENIED_AP_OVERLOAD:
	case STATUS_CODE_ASSOC_DENIED_BANDWIDTH:
		return TRUE;
	}
	switch (prBlack->u2DeauthReason) {
	case REASON_CODE_DISASSOC_LACK_OF_BANDWIDTH:
	case REASON_CODE_DISASSOC_AP_OVERLOAD:
		return TRUE;
	}
	return FALSE;
}

UINT_16 aisCalculateBlackListScore(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	if (!prBssDesc->prBlack)
		prBssDesc->prBlack = aisQueryBlackList(prAdapter, prBssDesc);

	if (!prBssDesc->prBlack)
		return 100;
	else if (aisApOverload(prBssDesc->prBlack) || prBssDesc->prBlack->ucCount >= 10)
		return 0;
	return 100 - prBssDesc->prBlack->ucCount * 10;
}

VOID aisRecordBeaconTimeout(P_ADAPTER_T prAdapter, P_BSS_INFO_T prAisBssInfo)
{
#if 0 /* wave2-feature */
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;
	struct LINK_MGMT *prBcnTimeout = &prAisFsmInfo->rBcnTimeout;
	struct AIS_BEACON_TIMEOUT_BSS *prEntry = NULL;

	LINK_MGMT_GET_ENTRY(prBcnTimeout, prEntry, struct AIS_BEACON_TIMEOUT_BSS, VIR_MEM_TYPE);
	if (!prEntry) {
		DBGLOG(CNM, WARN, "No memory to allocate\n");
		return;
	}
	COPY_MAC_ADDR(prEntry->aucBSSID, prAisBssInfo->aucBSSID);
	COPY_SSID(prEntry->aucSSID, prEntry->ucSSIDLen,
			prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen);
	prEntry->u8Tsf = prAisFsmInfo->prTargetBssDesc->u8TimeStamp.QuadPart;
	prEntry->u8AddTime = kalGetBootTime();
	LINK_INSERT_TAIL(&prBcnTimeout->rUsingLink, &prEntry->rLinkEntry);
#endif
}

VOID aisRemoveBeaconTimeoutEntry(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
#if 0 /* wave2-feature */
	UINT_64 u8Tsf = 0;
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;
	struct LINK_MGMT *prBcnTimeout = &prAisFsmInfo->rBcnTimeout;
	struct AIS_BEACON_TIMEOUT_BSS *prEntry = NULL;

	LINK_FOR_EACH_ENTRY(prEntry, &prBcnTimeout->rUsingLink,
		rLinkEntry, struct AIS_BEACON_TIMEOUT_BSS) {
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry->aucBSSID) &&
			EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			prEntry->aucSSID, prEntry->ucSSIDLen)) {
			u8Tsf = prBssDesc->u8TimeStamp.QuadPart;
			if (u8Tsf < prEntry->u8Tsf)
				DBGLOG(AIS, INFO, "%pM %s may reboot %llu seconds ago\n",
					prBssDesc->aucBSSID, prBssDesc->aucSSID, u8Tsf/USEC_PER_SEC);
			else
				DBGLOG(AIS, INFO, "%pM %s\n", prBssDesc->aucBSSID, prBssDesc->aucSSID);
			LINK_REMOVE_KNOWN_ENTRY(&prBcnTimeout->rUsingLink, prEntry);
			LINK_INSERT_HEAD(&prBcnTimeout->rFreeLink, &prEntry->rLinkEntry);
			return;
		}
	}
#endif
}

static VOID aisRemoveOldestBcnTimeout(P_AIS_FSM_INFO_T prAisFsmInfo)
{
#if 0 /* wave2-feature */
	struct AIS_BEACON_TIMEOUT_BSS *prEntry = NULL;
	P_LINK_T prLink = &prAisFsmInfo->rBcnTimeout.rUsingLink;
	UINT_64 u8Current = kalGetBootTime();

	while (TRUE) {
		prEntry = LINK_PEEK_HEAD(prLink, struct AIS_BEACON_TIMEOUT_BSS, rLinkEntry);
		if (!prEntry || (u8Current - prEntry->u8AddTime < CFG_BSS_DISAPPEAR_THRESOLD * USEC_PER_SEC))
			break;
		DBGLOG(AIS, INFO, "%pM %s has disappeard about %llu seconds\n",
			prEntry->aucBSSID, prEntry->aucSSID, (u8Current - prEntry->u8AddTime)/USEC_PER_SEC);
		LINK_REMOVE_HEAD(prLink, prEntry, struct AIS_BEACON_TIMEOUT_BSS *);
	}
#endif
}

VOID aisFsmRunEventBssTransition(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	struct MSG_AIS_BSS_TRANSITION_T *prMsg = (struct MSG_AIS_BSS_TRANSITION_T *)prMsgHdr;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
	P_STA_RECORD_T prStaRec = NULL;
	P_BSS_INFO_T prAisBssInfo = NULL;

	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam = &prAisSpecificBssInfo->rBTMParam;
	enum WNM_AIS_BSS_TRANSITION eTransType = BSS_TRANSITION_MAX_NUM;
	BOOLEAN fgNeedBtmResponse = FALSE;

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prStaRec = prAisBssInfo->prStaRecOfAP;

	if (!prMsg) {
		DBGLOG(AIS, WARN, "Msg Header is NULL\n");
		return;
	}
	eTransType = prMsg->eTransitionType;
	fgNeedBtmResponse = prMsg->fgNeedResponse;
	cnmMemFree(prAdapter, prMsgHdr);

	DBGLOG(AIS, INFO, "Transition Type: %d\n", eTransType);

	switch (eTransType) {
	case BSS_TRANSITION_DISASSOC:
		break;
	case BSS_TRANSITION_REQ_ROAMING:
		break;
	default:
		break;
	}

	/* always reject roaming request */
	prBtmParam->ucStatusCode = BSS_TRANSITION_MGT_STATUS_CAND_NO_CANDIDATES;
	prBtmParam->ucTermDelay = 0;
	kalMemZero(prBtmParam->aucTargetBssid, MAC_ADDR_LEN);
	prBtmParam->u2OurNeighborBssLen = 0;

	if (fgNeedBtmResponse)
		wnmSendBTMResponseFrame(prAdapter, prStaRec);
}

static VOID
aisSendNeighborRequest(P_ADAPTER_T prAdapter)
{
	struct SUB_ELEMENT_LIST *prSSIDIE;
	UINT_8 aucBuffer[sizeof(*prSSIDIE) + 31];
	P_BSS_INFO_T prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	kalMemZero(aucBuffer, sizeof(aucBuffer));
	prSSIDIE = (struct SUB_ELEMENT_LIST *)&aucBuffer[0];
	prSSIDIE->rSubIE.ucSubID = ELEM_ID_SSID;
	COPY_SSID(&prSSIDIE->rSubIE.aucOptInfo[0], prSSIDIE->rSubIE.ucLength,
		prBssInfo->aucSSID, prBssInfo->ucSSIDLen);
	rlmTxNeighborReportRequest(prAdapter, prBssInfo->prStaRecOfAP, prSSIDIE);
}

VOID aisCollectNeighborAPChannel(P_ADAPTER_T prAdapter,
	struct IE_NEIGHBOR_REPORT_T *prNeiRep, UINT_16 u2Length)
{
	PUINT_8 pucChnlList = &prAdapter->rWifiVar.rAisFsmInfo.aucNeighborAPChnl[0];
	UINT_8 i = 0;
	BOOLEAN fgValidChannel = FALSE;

	kalMemZero(pucChnlList, CFG_NEIGHBOR_AP_CHANNEL_NUM);
	while (u2Length > ELEM_HDR_LEN && i < CFG_NEIGHBOR_AP_CHANNEL_NUM) {
		fgValidChannel = rlmDomainIsLegalChannel(prAdapter,
			prNeiRep->ucChnlNumber <= 14 ? BAND_2G4:BAND_5G, prNeiRep->ucChnlNumber);
		if (fgValidChannel) {
			*pucChnlList++ = prNeiRep->ucChnlNumber;
			i++;
		}
		u2Length -= IE_SIZE(prNeiRep);
		prNeiRep = (struct IE_NEIGHBOR_REPORT_T *)((PUINT_8)prNeiRep + IE_SIZE(prNeiRep));
	}
	pucChnlList = &prAdapter->rWifiVar.rAisFsmInfo.aucNeighborAPChnl[0];
	DBGLOG(AIS, INFO, "Neighbor AP channel cnt %d, list %d %d %d %d %d %d\n", i, pucChnlList[0],
		pucChnlList[1], pucChnlList[2], pucChnlList[3], pucChnlList[4], pucChnlList[5]);
}

VOID aisRunEventChnlUtilRsp(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	struct MSG_CH_UTIL_RSP *prChUtilRsp = (struct MSG_CH_UTIL_RSP *)prMsgHdr;
	struct ESS_CHNL_INFO *prEssChnlInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo.arCurEssChnlInfo[0];
	PUINT_8 pucChnlList = NULL;
	PUINT_8 pucUtilization = NULL;
	UINT_8 i = 0;
	UINT_8 j = 0;

	if (!prChUtilRsp)
		return;
	if (prAdapter->rWifiVar.rAisFsmInfo.eCurrentState != AIS_STATE_COLLECT_ESS_INFO) {
		cnmMemFree(prAdapter, prChUtilRsp);
		return;
	}
	pucChnlList = prChUtilRsp->aucChnlList;
	pucUtilization = prChUtilRsp->aucChUtil;
	for (i = 0; i < prChUtilRsp->ucChnlNum; i++) {
		DBGLOG(AIS, INFO, "channel %d, utilization %d\n", pucChnlList[i], pucUtilization[i]);
		for (j = 0; j < prAdapter->rWifiVar.rAisSpecificBssInfo.ucCurEssChnlInfoNum; j++) {
			if (prEssChnlInfo[j].ucChannel != pucChnlList[i])
				continue;
			if (prEssChnlInfo[j].ucUtilization >= pucUtilization[i])
				continue;
			prEssChnlInfo[j].ucUtilization = pucUtilization[i];
			break;
		}
	}
	cnmMemFree(prAdapter, prChUtilRsp);
	aisFsmSteps(prAdapter, AIS_STATE_SEARCH);
}
#if CFG_SUPPORT_DYNAMIC_ROAM
#define CFG_BUF_SIZE 64
static VOID aisFsmSetRoamingThreshold(P_ADAPTER_T prAdapter, INT_8 cThreshold)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;
	UINT_8 aucRoamingCfgBuf[CFG_BUF_SIZE];
	UINT_8 ucRCPI;

	DBGLOG(AIS, INFO, "cThreshold %d, cRoamTriggerThreshold %d\n",
		cThreshold, prAisFsmInfo->cRoamTriggerThreshold);

#if CFG_SUPPORT_NCHO
	if (prAdapter->rNchoInfo.fgECHOEnabled) {
		DBGLOG(AIS, WARN, "NCHO is enabled now! Don't support to set Roaming Threshold!\n");
		return;
	}
#endif
	if (prAisFsmInfo->cRoamTriggerThreshold == cThreshold)
		return;
	prAisFsmInfo->cRoamTriggerThreshold = cThreshold;
	ucRCPI = dBm_TO_RCPI(cThreshold);
	kalMemZero(aucRoamingCfgBuf, sizeof(aucRoamingCfgBuf));
	kalSnprintf(aucRoamingCfgBuf, CFG_BUF_SIZE, "RoamingRCPIGoodValue %d\nRoamingRCPIPoorValue %d"
		, ucRCPI, ucRCPI);
	if (wlanFwCfgParse(prAdapter, aucRoamingCfgBuf) != WLAN_STATUS_SUCCESS)
		DBGLOG(AIS, INFO, "set cfg parse failed\n");
}
#endif
