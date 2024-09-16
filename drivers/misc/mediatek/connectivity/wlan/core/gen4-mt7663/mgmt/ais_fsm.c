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
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/ais_fsm.c#4
 */

/*! \file   "aa_fsm.c"
 *    \brief  This file defines the FSM for SAA and AAA MODULE.
 *
 *    This file defines the FSM for SAA and AAA MODULE.
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
#define AIS_ROAMING_CONNECTION_TRIAL_LIMIT  2
#define AIS_JOIN_TIMEOUT                    7

#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_0	0
#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_1	1
#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_2	2

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
#if CFG_DISCONN_DEBUG_FEATURE
struct AIS_DISCONN_INFO_T g_rDisconnInfoTemp;
uint8_t g_DisconnInfoIdx;
struct AIS_DISCONN_INFO_T *g_prDisconnInfo;
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static uint8_t *apucDebugAisState[AIS_STATE_NUM] = {
	(uint8_t *) DISP_STRING("IDLE"),
	(uint8_t *) DISP_STRING("SEARCH"),
	(uint8_t *) DISP_STRING("SCAN"),
	(uint8_t *) DISP_STRING("ONLINE_SCAN"),
	(uint8_t *) DISP_STRING("LOOKING_FOR"),
	(uint8_t *) DISP_STRING("WAIT_FOR_NEXT_SCAN"),
	(uint8_t *) DISP_STRING("REQ_CHANNEL_JOIN"),
	(uint8_t *) DISP_STRING("JOIN"),
	(uint8_t *) DISP_STRING("JOIN_FAILURE"),
	(uint8_t *) DISP_STRING("IBSS_ALONE"),
	(uint8_t *) DISP_STRING("IBSS_MERGE"),
	(uint8_t *) DISP_STRING("NORMAL_TR"),
	(uint8_t *) DISP_STRING("DISCONNECTING"),
	(uint8_t *) DISP_STRING("REQ_REMAIN_ON_CHANNEL"),
	(uint8_t *) DISP_STRING("REMAIN_ON_CHANNEL")
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static void aisFsmRunEventScanDoneTimeOut(IN struct ADAPTER *prAdapter,
					  unsigned long ulParam);
static void aisFsmSetOkcTimeout(IN struct ADAPTER *prAdapter,
				unsigned long ulParam);
/* Support AP Selection*/
static void aisRemoveDisappearedBlacklist(struct ADAPTER *prAdapter);
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static void aisResetBssTranstionMgtParam(struct AIS_SPECIFIC_BSS_INFO
					 *prSpecificBssInfo)
{
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
	    &prSpecificBssInfo->rBTMParam;

#if !CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	return;
#endif
	if (prBtmParam->u2OurNeighborBssLen > 0) {
		kalMemFree(prBtmParam->pucOurNeighborBss, VIR_MEM_TYPE,
			   prBtmParam->u2OurNeighborBssLen);
		prBtmParam->u2OurNeighborBssLen = 0;
	}
	kalMemZero(prBtmParam, sizeof(*prBtmParam));
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief the function is used to initialize the value of the connection
 *        settings for AIS network
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisInitializeConnectionSettings(IN struct ADAPTER *prAdapter,
				     IN struct REG_INFO *prRegInfo)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t aucAnyBSSID[] = BC_BSSID;
	uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	int i = 0;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* Setup default values for operation */
	COPY_MAC_ADDR(prConnSettings->aucMacAddress, aucZeroMacAddr);

	prConnSettings->ucDelayTimeOfDisconnectEvent =
	    AIS_DELAY_TIME_OF_DISCONNECT_SEC;

	COPY_MAC_ADDR(prConnSettings->aucBSSID, aucAnyBSSID);
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	prConnSettings->eReConnectLevel = RECONNECT_LEVEL_MIN;
	prConnSettings->fgIsConnReqIssued = FALSE;
	prConnSettings->fgIsDisconnectedByNonRequest = FALSE;

	prConnSettings->ucSSIDLen = 0;

	prConnSettings->eOPMode = NET_TYPE_INFRA;

	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	if (prRegInfo) {
		prConnSettings->ucAdHocChannelNum =
		    (uint8_t) nicFreq2ChannelNum(prRegInfo->u4StartFreq);
		prConnSettings->eAdHocBand =
		    prRegInfo->u4StartFreq < 5000000 ? BAND_2G4 : BAND_5G;
		prConnSettings->eAdHocMode =
		    (enum ENUM_PARAM_AD_HOC_MODE)(prRegInfo->u4AdhocMode);
	}

	prConnSettings->eAuthMode = AUTH_MODE_OPEN;

	prConnSettings->eEncStatus = ENUM_ENCRYPTION_DISABLED;

	prConnSettings->fgIsScanReqIssued = FALSE;

	/* MIB attributes */
	prConnSettings->u2BeaconPeriod = DOT11_BEACON_PERIOD_DEFAULT;

	prConnSettings->u2RTSThreshold = DOT11_RTS_THRESHOLD_DEFAULT;

	prConnSettings->u2DesiredNonHTRateSet = RATE_SET_ALL_ABG;

	/* prConnSettings->u4FreqInKHz; *//* Center frequency */

	/* Set U-APSD AC */
	prConnSettings->bmfgApsdEnAc = PM_UAPSD_NONE;

	secInit(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* Features */
	prConnSettings->fgIsEnableRoaming = FALSE;

	prConnSettings->fgSecModeChangeStartTimer = FALSE;

#if CFG_SUPPORT_ROAMING
#if 0
	if (prRegInfo)
		prConnSettings->fgIsEnableRoaming =
		    ((prRegInfo->fgDisRoaming > 0) ? (FALSE) : (TRUE));
#else
	if (prAdapter->rWifiVar.fgDisRoaming)
		prConnSettings->fgIsEnableRoaming = FALSE;
	else
		prConnSettings->fgIsEnableRoaming = TRUE;
#endif
#endif /* CFG_SUPPORT_ROAMING */

	prConnSettings->fgIsAdHocQoSEnable = FALSE;

#if CFG_SUPPORT_802_11AC
	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGNAC;
#else
	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGN;
#endif

	if (prAdapter->rWifiVar.ucHwNotSupportAC)
		prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGN;

	/* Set default bandwidth modes */
	prConnSettings->uc2G4BandwidthMode = CONFIG_BW_20M;
	prConnSettings->uc5GBandwidthMode = CONFIG_BW_20_40M;

	prConnSettings->rRsnInfo.ucElemId = 0x30;
	prConnSettings->rRsnInfo.u2Version = 0x0001;
	prConnSettings->rRsnInfo.u4GroupKeyCipherSuite = 0;
	prConnSettings->rRsnInfo.u4GroupMgmtKeyCipherSuite = 0;
	prConnSettings->rRsnInfo.u4PairwiseKeyCipherSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++)
		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[i] = 0;
	prConnSettings->rRsnInfo.u4AuthKeyMgtSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++)
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[i] = 0;
	prConnSettings->rRsnInfo.u2RsnCap = 0;
	prConnSettings->rRsnInfo.fgRsnCapPresent = FALSE;
	prConnSettings->rRsnInfo.u2PmkidCnt = 0;
	kalMemZero(prConnSettings->rRsnInfo.aucPmkidList,
		(sizeof(uint8_t) * MAX_NUM_SUPPORTED_PMKID * RSN_PMKID_LEN));
#if CFG_SUPPORT_CFG80211_AUTH
	prConnSettings->bss = NULL;
#endif
#if CFG_SUPPORT_OWE
	kalMemSet(&prConnSettings->rOweInfo, 0, sizeof(struct OWE_INFO_T));
#endif
} /* end of aisFsmInitializeConnectionSettings() */

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
void aisFsmInit(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t i;

	DEBUGFUNC("aisFsmInit()");
	DBGLOG(SW1, INFO, "->aisFsmInit()\n");

	/* avoid that the prAisBssInfo is realloc */
	if (prAdapter->prAisBssInfo != NULL)
		return;

	prAdapter->prAisBssInfo = prAisBssInfo =
	    cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_AIS, FALSE);
	ASSERT(prAisBssInfo);

	/* update MAC address */
	COPY_MAC_ADDR(prAdapter->prAisBssInfo->aucOwnMacAddr,
		      prAdapter->rMyMacAddr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	/* 4 <1> Initiate FSM */
	prAisFsmInfo->ePreviousState = AIS_STATE_IDLE;
	prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;

	prAisFsmInfo->ucAvailableAuthTypes = 0;

	prAisFsmInfo->prTargetBssDesc = (struct BSS_DESC *)NULL;

	prAisFsmInfo->ucSeqNumOfReqMsg = 0;
	prAisFsmInfo->ucSeqNumOfChReq = 0;
	prAisFsmInfo->ucSeqNumOfScanReq = 0;
	prAisFsmInfo->u2SeqNumOfScanReport = AIS_SCN_REPORT_SEQ_NOT_SET;

	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
#if CFG_SUPPORT_ROAMING
	prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif /* CFG_SUPPORT_ROAMING */
	prAisFsmInfo->fgIsChannelRequested = FALSE;
	prAisFsmInfo->fgIsChannelGranted = FALSE;
	prAisFsmInfo->u4PostponeIndStartTime = 0;
	/* Support AP Selection */
	prAisFsmInfo->ucJoinFailCntAfterScan = 0;

	prAisFsmInfo->fgIsScanOidAborted = FALSE;

	prAisFsmInfo->fgIsScanning = FALSE;

	/* 4 <1.1> Initiate FSM - Timer INIT */
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rBGScanTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventBGSleepTimeOut,
			  (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rIbssAloneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)
			  aisFsmRunEventIbssAloneTimeOut, (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rScanDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventScanDoneTimeOut,
			  (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rJoinTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventJoinTimeout,
			  (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rChannelTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventChannelTimeout,
			  (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rDeauthDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventDeauthTimeout,
			  (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rWaitOkcPMKTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmSetOkcTimeout,
			  (unsigned long)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rSecModeChangeTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)
			  aisFsmRunEventSecModeChangeTimeout,
			  (unsigned long)NULL);

	/* 4 <1.2> Initiate PWR STATE */
	SET_NET_PWR_STATE_IDLE(prAdapter, prAisBssInfo->ucBssIndex);

	/* 4 <2> Initiate BSS_INFO_T - common part */
	BSS_INFO_INIT(prAdapter, prAisBssInfo);
	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr,
		      prAdapter->rWifiVar.aucMacAddress);

	/* 4 <3> Initiate BSS_INFO_T - private part */
	/* TODO */
	prAisBssInfo->eBand = BAND_2G4;
	prAisBssInfo->ucPrimaryChannel = 1;
	prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *) NULL;
	prAisBssInfo->ucNss =
	    wlanGetSupportNss(prAdapter, prAisBssInfo->ucBssIndex);
#if (CFG_HW_WMM_BY_BSS == 0)
	prAisBssInfo->ucWmmQueSet =
	    (prAdapter->rWifiVar.eDbdcMode ==
	     ENUM_DBDC_MODE_DISABLED) ? DBDC_5G_WMM_INDEX : DBDC_2G_WMM_INDEX;
#endif
	/* 4 <4> Allocate MSDU_INFO_T for Beacon */
	prAisBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
		OFFSET_OF(struct WLAN_BEACON_FRAME,
		aucInfoElem[0]) + MAX_IE_LENGTH);

	if (prAisBssInfo->prBeacon) {
		prAisBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
		/* NULL STA_REC */
		prAisBssInfo->prBeacon->ucStaRecIndex = 0xFF;
	} else {
		ASSERT(0);
	}

	prAisBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;

	for (i = 0; i < MAX_KEY_NUM; i++) {
		prAisBssInfo->ucBMCWlanIndexS[i] = WTBL_RESERVED_ENTRY;
		prAisBssInfo->ucBMCWlanIndexSUsed[i] = FALSE;
		prAisBssInfo->wepkeyUsed[i] = FALSE;
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
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC =
	    (uint8_t) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC =
	    (uint8_t) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp =
	    (uint8_t) prAdapter->u4MaxSpLen;
#endif

	/* request list initialization */
	LINK_INITIALIZE(&prAisFsmInfo->rPendingReqList);

	/* Support AP Selection */
	LINK_MGMT_INIT(&prAdapter->rWifiVar.rConnSettings.rBlackList);
	kalMemZero(&prAisSpecificBssInfo->arCurEssChnlInfo[0],
		   sizeof(prAisSpecificBssInfo->arCurEssChnlInfo));
	LINK_INITIALIZE(&prAisSpecificBssInfo->rCurEssLink);
	/* end Support AP Selection */
	/* 11K, 11V */
	LINK_MGMT_INIT(&prAisSpecificBssInfo->rNeighborApList);
	kalMemZero(&prAisSpecificBssInfo->rBTMParam,
		   sizeof(prAisSpecificBssInfo->rBTMParam));

	/* DBGPRINTF("[2] ucBmpDeliveryAC:0x%x,
	 * ucBmpTriggerAC:0x%x, ucUapsdSp:0x%x",
	 */
	/* prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC, */
	/* prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC, */
	/* prAisBssInfo->rPmProfSetupInfo.ucUapsdSp); */

	/* Bind NetDev & BssInfo */
	/* wlanBindBssIdxToNetInterface(prAdapter->prGlueInfo,
	 * NET_DEV_WLAN_IDX, prAisBssInfo->ucBssIndex);
	 */

#if CFG_DISCONN_DEBUG_FEATURE
	g_prDisconnInfo = (struct AIS_DISCONN_INFO_T *)kalMemAlloc(
		sizeof(struct AIS_DISCONN_INFO_T) * MAX_DISCONNECT_RECORD,
		VIR_MEM_TYPE);
	if (g_prDisconnInfo != NULL) {
		kalMemZero(g_prDisconnInfo,
			sizeof(struct AIS_DISCONN_INFO_T) *
			MAX_DISCONNECT_RECORD);
	} else {
		DBGLOG(AIS, ERROR,
			"allocate memory for g_prDisconnInfo failed!\n");
	}

	g_DisconnInfoIdx = 0;

	/* default value */
	kalMemZero(&g_rDisconnInfoTemp, sizeof(struct AIS_DISCONN_INFO_T));
	g_rDisconnInfoTemp.ucBcnTimeoutReason = 0xF;
	g_rDisconnInfoTemp.u2DisassocSeqNum = 0xFFFF;
#endif

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
void aisFsmUninit(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmUninit()");
	DBGLOG(SW1, INFO, "->aisFsmUninit()\n");

	/* avoid that the prAisBssInfo is double freed */
	if (prAdapter->prAisBssInfo == NULL)
		return;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	/* 4 <1> Stop all timers */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rSecModeChangeTimer);

	/* 4 <2> flush pending request */
	aisFsmFlushRequest(prAdapter);
	aisResetBssTranstionMgtParam(prAisSpecificBssInfo);

	/* 4 <3> Reset driver-domain BSS-INFO */
	if (prAisBssInfo) {

		if (prAisBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prAisBssInfo->prBeacon);
			prAisBssInfo->prBeacon = NULL;
		}

		cnmFreeBssInfo(prAdapter, prAisBssInfo);
		prAdapter->prAisBssInfo = NULL;
	}
#if CFG_SUPPORT_802_11W
	rsnStopSaQuery(prAdapter);
#endif
	/* Support AP Selection */
	LINK_MGMT_UNINIT(&prAdapter->rWifiVar.rConnSettings.rBlackList,
			 struct AIS_BLACKLIST_ITEM, VIR_MEM_TYPE);
	/* end Support AP Selection */
	LINK_MGMT_UNINIT(&prAisSpecificBssInfo->rNeighborApList,
			 struct NEIGHBOR_AP_T, VIR_MEM_TYPE);

#if CFG_DISCONN_DEBUG_FEATURE
	if (g_prDisconnInfo != NULL) {
		kalMemFree(g_prDisconnInfo,
			VIR_MEM_TYPE,
			sizeof(struct AIS_DISCONN_INFO_T) *
			MAX_DISCONNECT_RECORD);
		g_prDisconnInfo = NULL;
	}
#endif
}				/* end of aisFsmUninit() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialization of JOIN STATE
 *
 * @param[in] prBssDesc  The pointer of BSS_DESC_T which is the BSS we will
 *                       try to join with.
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmStateInit_JOIN(IN struct ADAPTER *prAdapter,
			  struct BSS_DESC *prBssDesc)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct STA_RECORD *prStaRec;
	struct MSG_SAA_FSM_START *prJoinReqMsg;

	DEBUGFUNC("aisFsmStateInit_JOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	ASSERT(prBssDesc);

	/* 4 <1> We are going to connect to this BSS. */
	prBssDesc->fgIsConnecting = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_LEGACY_AP,
					      prAdapter->
					      prAisBssInfo->ucBssIndex,
					      prBssDesc);

	prAisFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <2.1> sync. to firmware domain */
	if (prStaRec->ucStaState == STA_STATE_1)
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* 4 <3> Update ucAvailableAuthTypes which we can choice during SAA */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

		prStaRec->fgIsReAssoc = FALSE;

		/*Fill Auth Type */
#if CFG_SUPPORT_CFG80211_AUTH
		prAisFsmInfo->ucAvailableAuthTypes =
			(uint8_t) prAdapter->prGlueInfo->rWpaInfo.u4AuthAlg;
		DBGLOG(AIS, INFO, "JOIN INIT: Auth Algorithm :%d\n",
			prAisFsmInfo->ucAvailableAuthTypes);
#else
		switch (prConnSettings->eAuthMode) {
			/* FT initial mobility doamin association always
			 ** use Open AA
			 */
		case AUTH_MODE_NON_RSN_FT:
		case AUTH_MODE_WPA2_FT:
		case AUTH_MODE_WPA2_FT_PSK:
		case AUTH_MODE_OPEN:	/* Note: Omit break here. */
		case AUTH_MODE_WPA:
		case AUTH_MODE_WPA_PSK:
		case AUTH_MODE_WPA2:
		case AUTH_MODE_WPA2_PSK:
		case AUTH_MODE_WPA_OSEN:
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_OPEN_SYSTEM;
			break;

		case AUTH_MODE_SHARED:
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_SHARED_KEY;
			break;

		case AUTH_MODE_AUTO_SWITCH:
			DBGLOG(AIS, LOUD,
			       "JOIN INIT: eAuthMode == AUTH_MODE_AUTO_SWITCH\n");
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) (AUTH_TYPE_OPEN_SYSTEM |
				       AUTH_TYPE_SHARED_KEY);
			break;

		default:
			ASSERT(!
			       (prConnSettings->eAuthMode ==
				AUTH_MODE_WPA_NONE));
			DBGLOG(AIS, ERROR,
			       "JOIN INIT: Auth Algorithm : %d was not supported by JOIN\n",
			       prConnSettings->eAuthMode);
			/* TODO(Kevin): error handling ? */
			return;
		}
#endif
		/* TODO(tyhsu): Assume that Roaming Auth Type
		 * is equal to ConnSettings eAuthMode
		 */
		prAisSpecificBssInfo->ucRoamingAuthTypes =
		    prAisFsmInfo->ucAvailableAuthTypes;

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;
		/* reset BTM Params when do first connection */
		aisResetBssTranstionMgtParam(prAisSpecificBssInfo);

		/* Update Bss info before join */
		prAisBssInfo->eBand = prBssDesc->eBand;
		prAisBssInfo->ucPrimaryChannel = prBssDesc->ucChannelNum;

	} else {
		ASSERT(prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE);

		DBGLOG(AIS, LOUD, "JOIN INIT: AUTH TYPE = %d for Roaming\n",
		       prAisSpecificBssInfo->ucRoamingAuthTypes);

		/* We do roaming while the medium is connected */
		prStaRec->fgIsReAssoc = TRUE;

		/* TODO(Kevin): We may call a sub function to
		 * acquire the Roaming Auth Type
		 */
		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_WPA2_FT:
		case AUTH_MODE_WPA2_FT_PSK:
		case AUTH_MODE_NON_RSN_FT:
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_FAST_BSS_TRANSITION;
			break;
		default:
			prAisFsmInfo->ucAvailableAuthTypes =
			    prAisSpecificBssInfo->ucRoamingAuthTypes;
			break;
		}

		prStaRec->ucTxAuthAssocRetryLimit =
		    TX_AUTH_ASSOCI_RETRY_LIMIT_FOR_ROAMING;
	}

	/* 4 <4> Use an appropriate Authentication Algorithm
	 * Number among the ucAvailableAuthTypes
	 */
	if (prAisFsmInfo->ucAvailableAuthTypes &
	(uint8_t) AUTH_TYPE_SHARED_KEY) {

		DBGLOG(AIS, LOUD,
		       "JOIN INIT: Try to do Authentication with AuthType == SHARED_KEY.\n");

		prAisFsmInfo->ucAvailableAuthTypes &=
		    ~(uint8_t) AUTH_TYPE_SHARED_KEY;

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_SHARED_KEY;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
		   AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, LOUD,
		       "JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n");
		prAisFsmInfo->ucAvailableAuthTypes &=
		    ~(uint8_t) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
		   AUTH_TYPE_FAST_BSS_TRANSITION) {

		DBGLOG(AIS, LOUD,
		       "JOIN INIT: Try to do Authentication with AuthType == FAST_BSS_TRANSITION.\n");

		prAisFsmInfo->ucAvailableAuthTypes &=
		    ~(uint8_t) AUTH_TYPE_FAST_BSS_TRANSITION;

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION;
#if CFG_SUPPORT_SAE
	} else if (prAisFsmInfo->ucAvailableAuthTypes &
		(uint8_t) AUTH_TYPE_SAE) {
		DBGLOG(AIS, LOUD,
		"JOIN INIT: Try to do Authentication with AuthType == SAE.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(uint8_t) AUTH_TYPE_SAE;

		prStaRec->ucAuthAlgNum = (uint8_t) AUTH_ALGORITHM_NUM_SAE;
#endif
	} else {
		ASSERT(0);
	}

	/* 4 <5> Overwrite Connection Setting for eConnectionPolicy
	 * == ANY (Used by Assoc Req)
	 */
	if (prBssDesc->ucSSIDLen)
		COPY_SSID(prConnSettings->aucSSID, prConnSettings->ucSSIDLen,
			  prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	/* 4 <6> Send a Msg to trigger SAA to start JOIN process. */
	prJoinReqMsg =
	    (struct MSG_SAA_FSM_START *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						    sizeof(struct
							   MSG_SAA_FSM_START));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	if (1) {
		int j;
		struct FRAG_INFO *prFragInfo;

		for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
			prFragInfo = &prStaRec->rFragInfo[j];

			if (prFragInfo->pr1stFrag) {
				/* nicRxReturnRFB(prAdapter,
				 * prFragInfo->pr1stFrag);
				 */
				prFragInfo->pr1stFrag = (struct SW_RFB *)NULL;
			}
		}
	}
#if CFG_SUPPORT_802_11K
	rlmSetMaxTxPwrLimit(prAdapter,
			    (prBssDesc->cPowerLimit != RLM_INVALID_POWER_LIMIT)
			    ? prBssDesc->cPowerLimit : RLM_MAX_TX_PWR, 1);
#endif
#if CFG_SUPPORT_CFG80211_AUTH
	prConnSettings->fgIsConnInitialized = TRUE;
#endif
	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prJoinReqMsg,
		    MSG_SEND_METHOD_BUF);
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
u_int8_t aisFsmStateInit_RetryJOIN(IN struct ADAPTER *prAdapter,
				   struct STA_RECORD *prStaRec)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_SAA_FSM_START *prJoinReqMsg;

	DEBUGFUNC("aisFsmStateInit_RetryJOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* Retry other AuthType if possible */
	if (!prAisFsmInfo->ucAvailableAuthTypes)
		return FALSE;

	if ((prStaRec->u2StatusCode !=
		STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED) &&
		(prStaRec->u2StatusCode !=
		STATUS_CODE_AUTH_TIMEOUT)) {
		prAisFsmInfo->ucAvailableAuthTypes = 0;
		return FALSE;
	}


	if (prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
	    AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, INFO,
		       "RETRY JOIN INIT: Retry Authentication with AuthType == OPEN_SYSTEM.\n");

		prAisFsmInfo->ucAvailableAuthTypes &=
		    ~(uint8_t) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else {
		DBGLOG(AIS, ERROR,
		       "RETRY JOIN INIT: Retry Authentication with Unexpected AuthType.\n");
		ASSERT(0);
	}

	/* No more available Auth Types */
	prAisFsmInfo->ucAvailableAuthTypes = 0;

	/* Trigger SAA to start JOIN process. */
	prJoinReqMsg =
	    (struct MSG_SAA_FSM_START *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						    sizeof(struct
							   MSG_SAA_FSM_START));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return FALSE;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prJoinReqMsg,
		    MSG_SEND_METHOD_BUF);

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
void aisFsmStateInit_IBSS_ALONE(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1> Check if IBSS was created before ? */
	if (prAisBssInfo->fgIsBeaconActivated) {

	/* 4 <2> Start IBSS Alone Timer for periodic SCAN and then SEARCH */
#if !CFG_SLT_SUPPORT
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer,
				   SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));
#endif
	}

	aisFsmCreateIBSS(prAdapter);
}				/* end of aisFsmStateInit_IBSS_ALONE() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief State Initialization of AIS_STATE_IBSS_MERGE
 *
 * @param[in] prBssDesc  The pointer of BSS_DESC_T which is the IBSS we will
 *                       try to merge with.
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmStateInit_IBSS_MERGE(IN struct ADAPTER *prAdapter,
				struct BSS_DESC *prBssDesc)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *)NULL;

	ASSERT(prBssDesc);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1> We will merge with to this BSS immediately. */
	prBssDesc->fgIsConnecting = FALSE;
	prBssDesc->fgIsConnected = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_ADHOC_PEER,
					      prAdapter->
					      prAisBssInfo->ucBssIndex,
					      prBssDesc);

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
void aisFsmStateAbort_JOIN(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_SAA_FSM_ABORT *prJoinAbortMsg;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* 1. Abort JOIN process */
	prJoinAbortMsg =
	    (struct MSG_SAA_FSM_ABORT *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						    sizeof(struct
							   MSG_SAA_FSM_ABORT));
	if (!prJoinAbortMsg) {

		ASSERT(0);	/* Can't abort SAA FSM */
		return;
	}

	prJoinAbortMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_ABORT;
	prJoinAbortMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinAbortMsg->prStaRec = prAisFsmInfo->prTargetStaRec;

	prAisFsmInfo->prTargetBssDesc->fgIsConnected = FALSE;
	prAisFsmInfo->prTargetBssDesc->fgIsConnecting = FALSE;

	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prJoinAbortMsg,
		    MSG_SEND_METHOD_BUF);

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
void aisFsmStateAbort_SCAN(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_SCN_SCAN_CANCEL *prScanCancelMsg;

	if (!prAdapter || prAdapter->prAisBssInfo == NULL) {
		DBGLOG(AIS, WARN, "Can't abort SCN FSM\n");
		return;
	}

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	DBGLOG(AIS, STATE, "aisFsmStateAbort_SCAN\n");

	/* Abort JOIN process. */
	prScanCancelMsg =
	    (struct MSG_SCN_SCAN_CANCEL *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct MSG_SCN_SCAN_CANCEL));
	if (!prScanCancelMsg) {

		ASSERT(0);	/* Can't abort SCN FSM */
		return;
	}
	kalMemZero(prScanCancelMsg, sizeof(struct MSG_SCN_SCAN_CANCEL));
	prScanCancelMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_CANCEL;
	prScanCancelMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfScanReq;
	prScanCancelMsg->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	prScanCancelMsg->fgIsChannelExt = FALSE;
	if (prAisFsmInfo->fgIsScanOidAborted) {
		prScanCancelMsg->fgIsOidRequest = TRUE;
		prAisFsmInfo->fgIsScanOidAborted = FALSE;
	}

	/* unbuffered message to guarantee scan is cancelled in sequence */
	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prScanCancelMsg,
		    MSG_SEND_METHOD_UNBUF);
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
void aisFsmStateAbort_NORMAL_TR(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* TODO(Kevin): Do abort other MGMT func */

	/* 1. Release channel to CNM */
	aisFsmReleaseCh(prAdapter);

	/* 2.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	/* 2.2 reset local variable */
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
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
void aisFsmStateAbort_IBSS(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_DESC *prBssDesc;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* reset BSS-DESC */
	if (prAisFsmInfo->prTargetStaRec) {
		prBssDesc =
		    scanSearchBssDescByTA(prAdapter,
					  prAisFsmInfo->
					  prTargetStaRec->aucMacAddr);

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
 * @brief The Core FSM engine of AIS(Ad-hoc, Infra STA)
 *
 * @param[in] eNextState Enum value of next AIS STATE
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmSteps(IN struct ADAPTER *prAdapter, enum ENUM_AIS_STATE eNextState)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_DESC *prBssDesc;
	struct MSG_CH_REQ *prMsgChReq;
	struct MSG_SCN_SCAN_REQ_V2 *prScanReqMsg;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;
	struct AIS_REQ_HDR *prAisReq;
	enum ENUM_BAND eBand;
	uint8_t ucChannel;
	uint16_t u2ScanIELen;
	u_int8_t fgIsTransition = (u_int8_t) FALSE;
#if CFG_SUPPORT_DBDC
	struct CNM_DBDC_CAP rDbdcCap;
#endif /*CFG_SUPPORT_DBDC */
	uint8_t ucRfBw;
	uint8_t ucReasonCode;
	struct GLUE_INFO *prGlueInfo;

	DEBUGFUNC("aisFsmSteps()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prGlueInfo = prAdapter->prGlueInfo;

	do {

		/* Do entering Next State */
		prAisFsmInfo->ePreviousState = prAisFsmInfo->eCurrentState;

		DBGLOG(AIS, STATE, "[AIS]TRANSITION: [%s] -> [%s]\n",
		       apucDebugAisState[prAisFsmInfo->eCurrentState],
		       apucDebugAisState[eNextState]);

		/* NOTE(Kevin): This is the only place to change the
		 * eCurrentState(except initial)
		 */
		prAisFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (u_int8_t) FALSE;

		aisPostponedEventOfDisconnTimeout(prAdapter, prAisFsmInfo);

		/* Do tasks of the State that we just entered */
		switch (prAisFsmInfo->eCurrentState) {
		/* NOTE(Kevin): we don't have to rearrange the
		 * sequence of following switch case. Instead
		 * I would like to use a common lookup table of array
		 * of function pointer to speed up state search.
		 */
		case AIS_STATE_IDLE:
#if CFG_SUPPORT_CFG80211_AUTH
			if (prAisFsmInfo->ePreviousState !=
				prAisFsmInfo->eCurrentState)
				prConnSettings->fgIsConnInitialized = FALSE;
#endif
			prAisReq = aisFsmGetNextRequest(prAdapter);
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rScanDoneTimer);
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rWaitOkcPMKTimer);

			if (prAisReq)
				DBGLOG(AIS, TRACE,
				       "eReqType=%d, fgIsConnReqIssued=%d, DisByNonRequest=%d\n",
				prAisReq->eReqType,
				prConnSettings->fgIsConnReqIssued,
				prConnSettings->fgIsDisconnectedByNonRequest);
			if (prAisReq == NULL
			    || prAisReq->eReqType == AIS_REQUEST_RECONNECT) {
				if (prConnSettings->fgIsConnReqIssued == TRUE
				    &&
				    prConnSettings->fgIsDisconnectedByNonRequest
				    == FALSE) {

					prAisFsmInfo->fgTryScan = TRUE;

					if (!IS_NET_ACTIVE
					    (prAdapter,
					     prAdapter->
					     prAisBssInfo->ucBssIndex)) {
						SET_NET_ACTIVE(prAdapter,
						prAdapter->prAisBssInfo->
						ucBssIndex);
						/* sync with firmware */
						nicActivateNetwork(prAdapter,
						prAdapter->prAisBssInfo->
						ucBssIndex);
					}

					SET_NET_PWR_STATE_ACTIVE(prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex);
#if CFG_SUPPORT_PNO
					prAisBssInfo->fgIsNetRequestInActive =
					    FALSE;
#endif
					/* reset trial count */
					prAisFsmInfo->ucConnTrialCount = 0;

					eNextState = AIS_STATE_SEARCH;
					fgIsTransition = TRUE;
				} else {
					SET_NET_PWR_STATE_IDLE(prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex);

					/* sync with firmware */
#if CFG_SUPPORT_PNO
					prAisBssInfo->fgIsNetRequestInActive =
					    TRUE;
					if (prAisBssInfo->fgIsPNOEnable) {
						DBGLOG(BSS, INFO,
						       "[BSSidx][Network]=%d PNOEnable&&OP_MODE_INFRASTRUCTURE,KEEP ACTIVE\n",
						prAisBssInfo->ucBssIndex);
					} else
#endif
					{
						UNSET_NET_ACTIVE(prAdapter,
						prAdapter->prAisBssInfo->
						ucBssIndex);
						nicDeactivateNetwork(prAdapter,
						prAdapter->prAisBssInfo->
						ucBssIndex);
					}

					/* check for other pending request */
					if (prAisReq && (aisFsmIsRequestPending
							 (prAdapter,
							  AIS_REQUEST_SCAN,
							  TRUE) == TRUE)) {
						wlanClearScanningResult
						    (prAdapter);
						eNextState = AIS_STATE_SCAN;
						prConnSettings->
						fgIsScanReqIssued = TRUE;

						fgIsTransition = TRUE;
					}
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

				prConnSettings->fgIsScanReqIssued = TRUE;
				eNextState = AIS_STATE_SCAN;
				fgIsTransition = TRUE;

				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType ==
				   AIS_REQUEST_ROAMING_CONNECT
				   || prAisReq->eReqType ==
				   AIS_REQUEST_ROAMING_SEARCH) {
				/* ignore */
				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType ==
				   AIS_REQUEST_REMAIN_ON_CHANNEL) {
				eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
				fgIsTransition = TRUE;

				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			}

			prAisFsmInfo->u4SleepInterval =
			    AIS_BG_SCAN_INTERVAL_MIN_SEC;


			if (prGlueInfo->u4LinkDownPendFlag == TRUE) {
				prGlueInfo->u4LinkDownPendFlag = FALSE;
				kalOidComplete(prAdapter->prGlueInfo,
					TRUE, 0, WLAN_STATUS_SUCCESS);
			}
			break;

		case AIS_STATE_SEARCH:
			/* 4 <1> Search for a matched candidate and save
			 * it to prTargetBssDesc.
			 * changing the state,
			 * ATTENTION: anyone can't leave this case without
			 * except BTM, otherwise, may cause BtmResponseTimer's
			 * handler run worngly
			 */
#if CFG_SLT_SUPPORT
			prBssDesc =
			prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
			/* Support AP Selection */
			if (prAisFsmInfo->ucJoinFailCntAfterScan >=
				SCN_BSS_JOIN_FAIL_THRESOLD) {
				prBssDesc = NULL;
				DBGLOG(AIS, STATE,
				       "Failed to connect %s more than 4 times after last scan, scan again\n",
				       prConnSettings->aucSSID);
			} else {
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM && !CFG_SUPPORT_CFG80211_AUTH
				prBssDesc = scanSearchBssDescByScoreForAis
				    (prAdapter);
#else
				prBssDesc = scanSearchBssDescByPolicy
				    (prAdapter, prAisBssInfo->ucBssIndex);
#endif
			}
#endif
			/* we are under Roaming Condition. */
			if (prAisBssInfo->eConnectionState ==
			    PARAM_MEDIA_STATE_CONNECTED) {
				if (prAisFsmInfo->ucConnTrialCount >
				    AIS_ROAMING_CONNECTION_TRIAL_LIMIT) {
#if CFG_SUPPORT_ROAMING
					DBGLOG(AIS, STATE,
					       "Roaming retry count :%d fail!\n",
					       prAisFsmInfo->ucConnTrialCount);
					roamingFsmRunEventFail(prAdapter,
					ROAMING_FAIL_REASON_CONNLIMIT);
#endif /* CFG_SUPPORT_ROAMING */
					/* reset retry count */
					prAisFsmInfo->ucConnTrialCount = 0;

					/* abort connection trial */
					if (prConnSettings->eReConnectLevel <
					    RECONNECT_LEVEL_BEACON_TIMEOUT) {
					prConnSettings->eReConnectLevel =
					RECONNECT_LEVEL_ROAMING_FAIL;
					prConnSettings->fgIsConnReqIssued =
					FALSE;
					} else {
						DBGLOG(AIS, INFO,
						       "Do not set fgIsConnReqIssued, level is %d\n",
						       prConnSettings->
						       eReConnectLevel);
					}

					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;

					break;
				}
			}
			/* 4 <2> We are not under Roaming Condition. */
			if (prAisBssInfo->eConnectionState ==
			    PARAM_MEDIA_STATE_DISCONNECTED) {

				/* 4 <2.a> If we have the matched one */
				if (prBssDesc) {
					/* 4 <A> Stored the Selected BSS
					 * security cipher.
					 */
					/* or later asoc req compose IE */
					prAisBssInfo->u4RsnSelectedGroupCipher =
					    prBssDesc->u4RsnSelectedGroupCipher;
					prAisBssInfo->
					u4RsnSelectedPairwiseCipher =
					prBssDesc->u4RsnSelectedPairwiseCipher;
					prAisBssInfo->u4RsnSelectedAKMSuite =
					    prBssDesc->u4RsnSelectedAKMSuite;
					prAisBssInfo->eBand = prBssDesc->eBand;
					if (prAisBssInfo->fgIsWmmInited
						== FALSE)
						prAisBssInfo->ucWmmQueSet =
						cnmWmmIndexDecision(prAdapter,
						prAisBssInfo);
#if CFG_SUPPORT_DBDC
					cnmDbdcEnableDecision(prAdapter,
						prAisBssInfo->ucBssIndex,
						prBssDesc->eBand,
						prBssDesc->ucChannelNum,
						prAisBssInfo->ucWmmQueSet);
					cnmGetDbdcCapability(prAdapter,
						prAisBssInfo->ucBssIndex,
						prBssDesc->eBand,
						prBssDesc->ucChannelNum,
						wlanGetSupportNss(prAdapter,
						    prAisBssInfo->ucBssIndex),
						&rDbdcCap);

					prAisBssInfo->ucNss = rDbdcCap.ucNss;
#endif /*CFG_SUPPORT_DBDC*/
					/* 4 <B> Do STATE transition and update
					 * current Operation Mode.
					 */
					if (prBssDesc->eBSSType ==
					    BSS_TYPE_INFRASTRUCTURE) {

						prAisBssInfo->eCurrentOPMode =
						    OP_MODE_INFRASTRUCTURE;

						/* Record the target BSS_DESC_T
						 * for next STATE.
						 */
						prAisFsmInfo->prTargetBssDesc =
						    prBssDesc;

						/* Transit to channel acquire */
						eNextState =
						    AIS_STATE_REQ_CHANNEL_JOIN;
						fgIsTransition = TRUE;

						/* increase connection trial
						 * count
						 */
						prAisFsmInfo->
						ucConnTrialCount++;
					}
#if CFG_SUPPORT_ADHOC
					else if (prBssDesc->eBSSType ==
						 BSS_TYPE_IBSS) {

						prAisBssInfo->eCurrentOPMode =
						    OP_MODE_IBSS;

						/* Record the target BSS_DESC_T
						 * for next STATE.
						 */
						prAisFsmInfo->prTargetBssDesc =
						    prBssDesc;

						eNextState =
						    AIS_STATE_IBSS_MERGE;
						fgIsTransition = TRUE;
					}
#endif /* CFG_SUPPORT_ADHOC */
					else {
						ASSERT(0);
						eNextState =
						AIS_STATE_WAIT_FOR_NEXT_SCAN;
						fgIsTransition = TRUE;
					}
				}
				/* 4 <2.b> If we don't have the matched one */
				else {
					if (prAisFsmInfo->rJoinReqTime != 0 &&
					    CHECK_FOR_TIMEOUT(kalGetTimeTick(),
						prAisFsmInfo->rJoinReqTime,
						SEC_TO_SYSTIME
						(AIS_JOIN_TIMEOUT))) {
						eNextState =
						    AIS_STATE_JOIN_FAILURE;
						fgIsTransition = TRUE;
						break;
					}
					/* increase connection trial count
					 * for infrastructure connection
					 */
					if (prConnSettings->eOPMode ==
					    NET_TYPE_INFRA)
						prAisFsmInfo->
						ucConnTrialCount++;

					/* 4 <A> Try to SCAN */
					if (prAisFsmInfo->fgTryScan) {
						eNextState =
						    AIS_STATE_LOOKING_FOR;

						fgIsTransition = TRUE;
					}
					/* 4 <B> We've do SCAN already, now wait
					 * in some STATE.
					 */
					else {
					eNextState =
					aisFsmStateSearchAction
					(prAdapter,
					AIS_FSM_STATE_SEARCH_ACTION_PHASE_0);
					fgIsTransition = TRUE;
					}
				}
			}
			/* 4 <3> We are under Roaming Condition. */
			/* prAdapter->eConnectionState ==
			 * MEDIA_STATE_CONNECTED.
			 */
			else {

				/* 4 <3.a> This BSS_DESC_T is our AP. */
				/* NOTE(Kevin 2008/05/16): Following cases
				 * will go back to NORMAL_TR.
				 * CASE I: During Roaming, APP(WZC/NDISTEST)
				 * change the connection
				 * settings. That make we can NOT match
				 * the original AP, so the
				 * prBssDesc is NULL.
				 * CASE II: The same reason as CASE I.
				 * Because APP change the
				 * eOPMode to other network type in
				 * connection setting
				 * (e.g. NET_TYPE_IBSS), so the BssDesc
				 * become the IBSS node.
				 * (For CASE I/II, before WZC/NDISTEST set
				 * the OID_SSID , it will change
				 * other parameters in connection setting
				 * first. So if we do roaming
				 * at the same time, it will hit these cases.)
				 *
				 * CASE III: Normal case, we can't find other
				 * candidate to roam
				 * out, so only the current AP will be matched.
				 *
				 * CASE VI: Timestamp of the current AP
				 * might be reset
				 */
				if (prAisBssInfo->ucReasonOfDisconnect !=
				DISCONNECT_REASON_CODE_REASSOCIATION &&
				((!prBssDesc) ||    /* CASE I */
				(prBssDesc->eBSSType !=
				BSS_TYPE_INFRASTRUCTURE) ||
				/* CASE II */
				(prBssDesc->fgIsConnected) ||
				/* CASE III */
				(EQUAL_MAC_ADDR(prBssDesc->aucBSSID,
				prAisBssInfo->aucBSSID))) /* CASE VI */) {
					if (prBssDesc) {
						DBGLOG(ROAMING, INFO,
						       "fgIsConnected=%d, prBssDesc->BSSID "
						       MACSTR
						       ", prAisBssInfo->BSSID "
						       MACSTR "\n",
						       prBssDesc->fgIsConnected,
						       MAC2STR
						       (prBssDesc->aucBSSID),
						       MAC2STR
						       (prAisBssInfo->
						       aucBSSID));
					}
#if DBG
					if ((prBssDesc)
					    && (prBssDesc->fgIsConnected))
						ASSERT(EQUAL_MAC_ADDR
						(prBssDesc->aucBSSID,
						prAisBssInfo->aucBSSID));
#endif /* DBG */
					if (prAisFsmInfo->
					    fgTargetChnlScanIssued) {
						/* if target channel scan has
						 * issued, and no roaming
						 * target is found, need
						 * to do full scan again
						 */
						DBGLOG(AIS, INFO,
						       "[Roaming] No target found, try to full scan again\n");
						prAisFsmInfo->
						    fgTargetChnlScanIssued =
						    FALSE;
						eNextState =
						    AIS_STATE_LOOKING_FOR;
						fgIsTransition = TRUE;
						break;
					}

					/* We already associated with it
					 * , go back to NORMAL_TR
					 */
					/* TODO(Kevin): Roaming Fail */
#if CFG_SUPPORT_ROAMING
					roamingFsmRunEventFail(prAdapter,
					ROAMING_FAIL_REASON_NOCANDIDATE);
#endif /* CFG_SUPPORT_ROAMING */
					/* Retreat to NORMAL_TR state */
					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;
					break;
				}
				/* 4 <3.b> Try to roam out for JOIN this
				 * BSS_DESC_T.
				 */
				else {
					if (!prBssDesc) {
						fgIsTransition = TRUE;
						eNextState =
						aisFsmStateSearchAction
						(prAdapter,
					AIS_FSM_STATE_SEARCH_ACTION_PHASE_1);
						break;
					}
					aisFsmStateSearchAction(prAdapter,
					AIS_FSM_STATE_SEARCH_ACTION_PHASE_2);
					/* 4 <A> Record the target BSS_DESC_T
					 ** for next STATE.
					 */
					prAisFsmInfo->prTargetBssDesc =
					    prBssDesc;

					/* tyhsu: increase connection trial
					 ** count
					 */
					prAisFsmInfo->ucConnTrialCount++;

					/* Transit to channel acquire */
					eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
					/* Find target AP to roaming
					 * and set
					 *   fgTargetChnlScanIssued
					 * to false
					 */
					prAisFsmInfo->fgTargetChnlScanIssued =
					    FALSE;
					fgIsTransition = TRUE;
				}
			}
			if (prBssDesc && prConnSettings->fgOkcEnabled) {
				uint8_t
				    aucBuf[sizeof
					   (struct PARAM_PMKID_CANDIDATE_LIST) +
					   sizeof(struct
						  PARAM_STATUS_INDICATION)];
				struct PARAM_STATUS_INDICATION *prStatusEvent =
				    (struct PARAM_STATUS_INDICATION *)aucBuf;
				struct PARAM_PMKID_CANDIDATE_LIST
				*prPmkidCandicate =
				    (struct PARAM_PMKID_CANDIDATE_LIST
				     *)(prStatusEvent + 1);
				uint32_t u4Entry = 0;

				if (rsnSearchPmkidEntry
				    (prAdapter, prBssDesc->aucBSSID, &u4Entry)
				    && prAdapter->rWifiVar.
				    rAisSpecificBssInfo.arPmkidCache[u4Entry].
				    fgPmkidExist)
					break;
				DBGLOG(AIS, INFO, "No PMK for " MACSTR
				       ", try to generate a OKC PMK\n",
				       MAC2STR(prBssDesc->aucBSSID));
				prStatusEvent->eStatusType =
				    ENUM_STATUS_TYPE_CANDIDATE_LIST;
				prPmkidCandicate->u4Version = 1;
				prPmkidCandicate->u4NumCandidates = 1;
				/* don't request preauth */
				prPmkidCandicate->
					      arCandidateList[0].u4Flags = 0;
				COPY_MAC_ADDR(prPmkidCandicate->arCandidateList
					      [0].arBSSID, prBssDesc->aucBSSID);
				kalIndicateStatusAndComplete
				    (prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (void *)aucBuf, sizeof(aucBuf));
				cnmTimerStartTimer(prAdapter,
					&prAisFsmInfo->rWaitOkcPMKTimer,
					AIS_WAIT_OKC_PMKID_SEC);
			}

			break;

		case AIS_STATE_WAIT_FOR_NEXT_SCAN:

			DBGLOG(AIS, LOUD,
			       "SCAN: Idle Begin - Current Time = %u\n",
			       kalGetTimeTick());

			cnmTimerStartTimer(prAdapter,
					   &prAisFsmInfo->rBGScanTimer,
					   SEC_TO_MSEC
					   (prAisFsmInfo->u4SleepInterval));

			SET_NET_PWR_STATE_IDLE(prAdapter,
					       prAdapter->
					       prAisBssInfo->ucBssIndex);

			if (prAisFsmInfo->u4SleepInterval <
			    AIS_BG_SCAN_INTERVAL_MAX_SEC)
				prAisFsmInfo->u4SleepInterval <<= 1;

			break;

		case AIS_STATE_SCAN:
		case AIS_STATE_ONLINE_SCAN:
		case AIS_STATE_LOOKING_FOR:

			if (!IS_NET_ACTIVE
			    (prAdapter, prAdapter->prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter,
					       prAdapter->
					       prAisBssInfo->ucBssIndex);

				/* sync with firmware */
				nicActivateNetwork(prAdapter,
						   prAdapter->
						   prAisBssInfo->ucBssIndex);
#if CFG_SUPPORT_PNO
				prAisBssInfo->fgIsNetRequestInActive = FALSE;
#endif
			}
			prScanRequest = &(prAisFsmInfo->rScanRequest);

			/* IE length decision */
			if (prScanRequest->u4IELength > 0) {
				u2ScanIELen =
				    (uint16_t) prScanRequest->u4IELength;
			} else {
#if CFG_SUPPORT_WPS2
				u2ScanIELen = prAdapter->prGlueInfo->u2WSCIELen;
#else
				u2ScanIELen = 0;
#endif
			}

			prScanReqMsg =
			    (struct MSG_SCN_SCAN_REQ_V2 *)cnmMemAlloc(prAdapter,
					RAM_TYPE_MSG,
					OFFSET_OF
					(struct
					MSG_SCN_SCAN_REQ_V2,
					aucIE) +
					u2ScanIELen);
			if (!prScanReqMsg) {
				ASSERT(0);	/* Can't trigger SCAN FSM */
				return;
			}
			kalMemZero(prScanReqMsg, OFFSET_OF
				   (struct MSG_SCN_SCAN_REQ_V2,
				    aucIE)+u2ScanIELen);
			prScanReqMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_REQ_V2;
			prScanReqMsg->ucSeqNum =
			    ++prAisFsmInfo->ucSeqNumOfScanReq;
			if (prAisFsmInfo->u2SeqNumOfScanReport ==
			    AIS_SCN_REPORT_SEQ_NOT_SET) {
				prAisFsmInfo->u2SeqNumOfScanReport =
				    (uint16_t) prScanReqMsg->ucSeqNum;
			}
			prScanReqMsg->ucBssIndex =
			    prAdapter->prAisBssInfo->ucBssIndex;
#if CFG_SUPPORT_802_11K
			if (rlmFillScanMsg(prAdapter, prScanReqMsg)) {
				mboxSendMsg(prAdapter, MBOX_ID_0,
					    (struct MSG_HDR *)prScanReqMsg,
					    MSG_SEND_METHOD_BUF);
				break;
			}
			COPY_MAC_ADDR(prScanReqMsg->aucBSSID,
				      "\xff\xff\xff\xff\xff\xff");
#endif

#if CFG_SUPPORT_RDD_TEST_MODE
			prScanReqMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;
#else
			if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN
			    || prAisFsmInfo->eCurrentState ==
			    AIS_STATE_ONLINE_SCAN) {
				uint8_t ucScanSSIDNum;
				enum ENUM_SCAN_TYPE eScanType;

				ucScanSSIDNum = prScanRequest->u4SsidNum;
				eScanType = prScanRequest->ucScanType;

				if (eScanType == SCAN_TYPE_ACTIVE_SCAN
				    && ucScanSSIDNum == 0) {
					prScanReqMsg->eScanType = eScanType;

					prScanReqMsg->ucSSIDType
					    = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
				} else if (eScanType == SCAN_TYPE_PASSIVE_SCAN
					   && ucScanSSIDNum == 0) {
					prScanReqMsg->eScanType = eScanType;

					prScanReqMsg->ucSSIDType = 0;
					prScanReqMsg->ucSSIDNum = 0;
				} else {
					prScanReqMsg->eScanType =
					    SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType =
					    SCAN_REQ_SSID_SPECIFIED;
					prScanReqMsg->ucSSIDNum = ucScanSSIDNum;
					prScanReqMsg->prSsid =
					    prScanRequest->rSsid;
				}
				kalMemCopy(prScanReqMsg->aucRandomMac,
					   prScanRequest->aucRandomMac,
					   MAC_ADDR_LEN);
				prScanReqMsg->ucScnFuncMask |=
				    prScanRequest->ucScnFuncMask;

			} else {
				prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

				COPY_SSID(prAisFsmInfo->rRoamingSSID.aucSsid,
					  prAisFsmInfo->rRoamingSSID.u4SsidLen,
					  prConnSettings->aucSSID,
					  prConnSettings->ucSSIDLen);

				/* Scan for determined SSID */
				prScanReqMsg->ucSSIDType =
				    SCAN_REQ_SSID_SPECIFIED;
				prScanReqMsg->ucSSIDNum = 1;
				prScanReqMsg->prSsid =
				    &(prAisFsmInfo->rRoamingSSID);
#if CFG_SUPPORT_SCAN_RANDOM_MAC
				prScanReqMsg->ucScnFuncMask |=
				    ENUM_SCN_RANDOM_MAC_EN;
#endif
			}
#endif

			/* using default channel dwell time/timeout value */
			prScanReqMsg->u2ProbeDelay = 0;
			prScanReqMsg->u2ChannelDwellTime = 0;
			prScanReqMsg->u2ChannelMinDwellTime = 0;
			prScanReqMsg->u2TimeoutValue = 0;
			/* check if tethering is running and need to fix on
			 * specific channel
			 */
			if (cnmAisInfraChannelFixed
			    (prAdapter, &eBand, &ucChannel) == TRUE) {
				prScanReqMsg->eScanChannel =
				    SCAN_CHANNEL_SPECIFIED;
				prScanReqMsg->ucChannelListNum = 1;
				prScanReqMsg->arChnlInfoList[0].eBand = eBand;
				prScanReqMsg->arChnlInfoList[0].ucChannelNum =
				    ucChannel;
			} else if (prAisBssInfo->eConnectionState ==
				   PARAM_MEDIA_STATE_CONNECTED
				   && (prAdapter->rWifiVar.
				       rRoamingInfo.eCurrentState ==
				       ROAMING_STATE_DISCOVERY)
				   && prAisFsmInfo->fgTargetChnlScanIssued) {
				struct RF_CHANNEL_INFO *prChnlInfo =
				    &prScanReqMsg->arChnlInfoList[0];
				uint8_t ucChannelNum = 0;
				uint8_t i = 0;
#if CFG_SUPPORT_802_11K
				struct LINK *prNeighborAPLink =
				    &prAdapter->rWifiVar.
				    rAisSpecificBssInfo.rNeighborApList.
				    rUsingLink;
#endif
				for (i = 0;
				     i <
				     prAdapter->rWifiVar.rAisSpecificBssInfo.
				     ucCurEssChnlInfoNum; i++) {
					ucChannelNum =
					    prAdapter->rWifiVar.
					    rAisSpecificBssInfo.
					    arCurEssChnlInfo[i].ucChannel;
					if ((ucChannelNum >= 1)
					    && (ucChannelNum <= 14))
						prChnlInfo[i].eBand = BAND_2G4;
					else
						prChnlInfo[i].eBand = BAND_5G;
					prChnlInfo[i].ucChannelNum
					    = ucChannelNum;
				}
				prScanReqMsg->ucChannelListNum
				    =
				    prAdapter->rWifiVar.rAisSpecificBssInfo.
				    ucCurEssChnlInfoNum;
				prScanReqMsg->eScanChannel =
				    SCAN_CHANNEL_SPECIFIED;
				DBGLOG(AIS, INFO,
				       "[Roaming] Target Scan: Total number of scan channel(s)=%d\n",
				       prScanReqMsg->ucChannelListNum);

#if CFG_SUPPORT_802_11K
				/* Add channels provided by Neighbor Report to
				 ** channel list for roaming scanning.
				 */
				if (!LINK_IS_EMPTY(prNeighborAPLink)) {
					struct NEIGHBOR_AP_T *prNeiAP = NULL;
					struct RF_CHANNEL_INFO *prChnlInfo =
					    &prScanReqMsg->arChnlInfoList[0];
					uint8_t ucChnlNum =
					    prScanReqMsg->ucChannelListNum;
					uint8_t i = 0;

					LINK_FOR_EACH_ENTRY(prNeiAP,
							    prNeighborAPLink,
							    rLinkEntry, struct
							    NEIGHBOR_AP_T) {
					ucChannel = prNeiAP->ucChannel;
					eBand = ucChannel <= 14
						? BAND_2G4 : BAND_5G;
					if (!rlmDomainIsLegalChannel
						(prAdapter, eBand,
						ucChannel))
						continue;

					/* Append channel(s) provided by
					 * neighbor report into channel
					 * list of current ESS in scan msg.
					 */
					for (i = 0; i < ucChnlNum; i++) {
					if (ucChannel ==
					prChnlInfo
					[i].ucChannelNum)
						break;
					}

					if (i != ucChnlNum)
						continue;
					prChnlInfo[ucChnlNum].eBand =
						eBand;
					prChnlInfo[ucChnlNum]
					.ucChannelNum = ucChannel;
					ucChnlNum++;
					if (ucChnlNum ==
					MAXIMUM_OPERATION_CHANNEL_LIST)
						break;
					}
					DBGLOG(AIS, INFO,
					       "[Roaming] Target Scan: Total number of scan channel(s)=%d and %d channel(s) provided by neighbor report\n",
					       ucChnlNum, ucChnlNum -
					       prScanReqMsg->ucChannelListNum);
					prScanReqMsg->ucChannelListNum =
					    ucChnlNum;
				}
#endif
#if CFG_SUPPORT_NCHO
			} else if (prAdapter->rNchoInfo.fgECHOEnabled &&
				   prAdapter->rNchoInfo.u4RoamScanControl ==
				   TRUE
				   && prAisBssInfo->eConnectionState ==
				   PARAM_MEDIA_STATE_CONNECTED
				   && prAdapter->rWifiVar.
				   rRoamingInfo.eCurrentState ==
				   ROAMING_STATE_DISCOVERY) {
				/* handle NCHO scan channel info */
				uint32_t u4size = 0;
				struct _CFG_NCHO_SCAN_CHNL_T *prRoamScnChnl =
				    NULL;

				prRoamScnChnl =
				    &prAdapter->rNchoInfo.rRoamScnChnl;
				/* set partial scan */
				prScanReqMsg->ucChannelListNum =
				    prRoamScnChnl->ucChannelListNum;
				u4size = sizeof(prRoamScnChnl->arChnlInfoList);

				DBGLOG(AIS, TRACE,
				       "NCHO SCAN channel num = %d, total size=%d\n",
				       prScanReqMsg->ucChannelListNum, u4size);

				kalMemCopy(&(prScanReqMsg->arChnlInfoList),
					   &(prRoamScnChnl->arChnlInfoList),
					   u4size);

				/* set scan channel type for NCHO scan */
				prScanReqMsg->eScanChannel =
				    SCAN_CHANNEL_SPECIFIED;
#endif
			} else
			if (prAdapter->aePreferBand
				[prAdapter->prAisBssInfo->ucBssIndex] ==
				BAND_NULL) {
				if (prAdapter->fgEnable5GBand == TRUE)
					prScanReqMsg->eScanChannel =
					    SCAN_CHANNEL_FULL;
				else
					prScanReqMsg->eScanChannel =
					    SCAN_CHANNEL_2G4;

			} else
			if (prAdapter->aePreferBand
				[prAdapter->prAisBssInfo->ucBssIndex] ==
				BAND_2G4) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
			} else
			if (prAdapter->aePreferBand
				[prAdapter->prAisBssInfo->ucBssIndex] ==
				BAND_5G) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
			} else {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
				ASSERT(0);
			}

			switch (prScanReqMsg->eScanChannel) {
			case SCAN_CHANNEL_FULL:
			case SCAN_CHANNEL_2G4:
			case SCAN_CHANNEL_5G:
				scanSetRequestChannel(prAdapter,
					prScanRequest->u4ChannelNum,
					prScanRequest->arChannel,
					(prAisFsmInfo->eCurrentState ==
					AIS_STATE_ONLINE_SCAN),
					prScanReqMsg);
				break;
			default:
				break;
			}
			if (u2ScanIELen > 0) {
				kalMemCopy(prScanReqMsg->aucIE,
					   prScanRequest->pucIE, u2ScanIELen);
			} else {
#if CFG_SUPPORT_WPS2
				if (prAdapter->prGlueInfo->u2WSCIELen > 0) {
					kalMemCopy(prScanReqMsg->aucIE,
						   &prAdapter->
						   prGlueInfo->aucWSCIE,
						   prAdapter->
						   prGlueInfo->u2WSCIELen);
				}
			}
#endif
			prScanReqMsg->u2IELen = u2ScanIELen;

			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *)prScanReqMsg,
				    MSG_SEND_METHOD_BUF);

			kalMemZero(prAisFsmInfo->aucScanIEBuf,
				   sizeof(prAisFsmInfo->aucScanIEBuf));
			prScanRequest->u4SsidNum = 0;
			prScanRequest->ucScanType = 0;
			prScanRequest->u4IELength = 0;
			prScanRequest->u4ChannelNum = 0;
			prScanRequest->ucScnFuncMask = 0;
			kalMemZero(prScanRequest->aucRandomMac, MAC_ADDR_LEN);
			/* Will enable background sleep for infrastructure */
			prAisFsmInfo->fgTryScan = FALSE;
			prAisFsmInfo->fgIsScanning = TRUE;

			/* Support AP Selection */
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;
			break;

		case AIS_STATE_REQ_CHANNEL_JOIN:
			/* stop Tx due to we need to connect a new AP. even the
			 ** new AP is operating on the same channel with current
			 ** , we still need to stop Tx, because firmware should
			 ** ensure all mgmt and dhcp packets are Tx in time,
			 ** and may cause normal data packets was queued and
			 ** eventually flushed in firmware
			 */
			if (prAisBssInfo->prStaRecOfAP &&
			    prAisBssInfo->ucReasonOfDisconnect !=
			    DISCONNECT_REASON_CODE_REASSOCIATION)
				prAdapter->prAisBssInfo->
				    prStaRecOfAP->fgIsTxAllowed = FALSE;

			/* send message to CNM for acquiring channel */
			prMsgChReq =
			    (struct MSG_CH_REQ *)cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_CH_REQ));
			if (!prMsgChReq) {
				/* Can't indicate CNM for channel acquiring */
				ASSERT(0);
				return;
			}

			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex =
			    prAdapter->prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
#ifdef CFG_SUPPORT_ADJUST_JOIN_CH_REQ_INTERVAL
			prMsgChReq->u4MaxInterval =
			    prAdapter->rWifiVar.u4AisJoinChReqIntervel;
#else
			prMsgChReq->u4MaxInterval =
			    AIS_JOIN_CH_REQUEST_INTERVAL;
#endif
			DBGLOG(AIS, INFO, "Request join interval: %u\n",
			    prMsgChReq->u4MaxInterval);

			prMsgChReq->ucPrimaryChannel =
			    prAisFsmInfo->prTargetBssDesc->ucChannelNum;
			prMsgChReq->eRfSco =
			    prAisFsmInfo->prTargetBssDesc->eSco;
			prMsgChReq->eRfBand =
			    prAisFsmInfo->prTargetBssDesc->eBand;
#if CFG_SUPPORT_DBDC
			prMsgChReq->eDBDCBand = ENUM_BAND_AUTO;
#endif /*CFG_SUPPORT_DBDC */
			/* To do: check if 80/160MHz bandwidth is needed here */
			/* Decide RF BW by own OP and Peer OP BW */
			ucRfBw =
			    cnmGetDbdcBwCapability(prAdapter,
						   prAisBssInfo->ucBssIndex);
			/* Revise to VHT OP BW */
			ucRfBw = rlmGetVhtOpBwByBssOpBw(ucRfBw);
			if (ucRfBw >
			    prAisFsmInfo->prTargetBssDesc->eChannelWidth)
				ucRfBw =
				    prAisFsmInfo->
				    prTargetBssDesc->eChannelWidth;

			prMsgChReq->eRfChannelWidth = ucRfBw;
			/* TODO: BW80+80 support */
			prMsgChReq->ucRfCenterFreqSeg1 =
			    nicGetVhtS1(prMsgChReq->ucPrimaryChannel,
					prMsgChReq->eRfChannelWidth);
			DBGLOG(RLM, INFO,
			       "AIS req CH for CH:%d, Bw:%d, s1=%d\n",
			       prAisBssInfo->ucPrimaryChannel,
			       prMsgChReq->eRfChannelWidth,
			       prMsgChReq->ucRfCenterFreqSeg1);
			prMsgChReq->ucRfCenterFreqSeg2 = 0;

			rlmReviseMaxBw(prAdapter, prAisBssInfo->ucBssIndex,
				       &prMsgChReq->eRfSco,
				       (enum ENUM_CHANNEL_WIDTH *)
				       &prMsgChReq->eRfChannelWidth,
				       &prMsgChReq->ucRfCenterFreqSeg1,
				       &prMsgChReq->ucPrimaryChannel);

			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *)prMsgChReq,
				    MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;
			break;

		case AIS_STATE_JOIN:
			aisFsmStateInit_JOIN(prAdapter,
					     prAisFsmInfo->prTargetBssDesc);
			break;

		case AIS_STATE_JOIN_FAILURE:
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel =
			    RECONNECT_LEVEL_MIN;
			prConnSettings->fgIsDisconnectedByNonRequest = TRUE;

			nicMediaJoinFailure(prAdapter,
					    prAdapter->prAisBssInfo->ucBssIndex,
					    WLAN_STATUS_JOIN_FAILURE);

			eNextState = AIS_STATE_IDLE;
			fgIsTransition = TRUE;

			break;

#if CFG_SUPPORT_ADHOC
		case AIS_STATE_IBSS_ALONE:
			aisFsmStateInit_IBSS_ALONE(prAdapter);
			break;

		case AIS_STATE_IBSS_MERGE:
			aisFsmStateInit_IBSS_MERGE(prAdapter,
				prAisFsmInfo->prTargetBssDesc);
			break;
#endif /* CFG_SUPPORT_ADHOC */

		case AIS_STATE_NORMAL_TR:
			if (prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* Don't do anything when rJoinTimeoutTimer
				 * is still ticking
				 */
				break;
			}

			/* 1. Process for pending roaming scan */
			if (aisFsmIsRequestPending(prAdapter,
				AIS_REQUEST_ROAMING_SEARCH, TRUE) == TRUE) {
				eNextState = AIS_STATE_LOOKING_FOR;
				fgIsTransition = TRUE;
			}
			/* 2. Process for pending roaming connect */
			else if (aisFsmIsRequestPending(prAdapter,
					AIS_REQUEST_ROAMING_CONNECT, TRUE)
						== TRUE) {
				eNextState = AIS_STATE_SEARCH;
				fgIsTransition = TRUE;
			}
			/* 3. Process for pending scan */
			else if (aisFsmIsRequestPending(prAdapter,
					AIS_REQUEST_SCAN, TRUE) == TRUE) {
				wlanClearScanningResult(prAdapter);
				eNextState = AIS_STATE_ONLINE_SCAN;
				fgIsTransition = TRUE;
			} else if (aisFsmIsRequestPending(prAdapter,
					AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE)
								== TRUE) {
				eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
				fgIsTransition = TRUE;
			}

			break;

		case AIS_STATE_DISCONNECTING:
			/* send for deauth frame for disconnection */
			ucReasonCode = REASON_CODE_DEAUTH_LEAVING_BSS;
#if CFG_DISCONN_DEBUG_FEATURE
			g_rDisconnInfoTemp.ucDisassocReason = ucReasonCode;
#endif
			authSendDeauthFrame(prAdapter,
					    prAisBssInfo,
					    prAisBssInfo->prStaRecOfAP,
					    (struct SW_RFB *)NULL,
					    ucReasonCode,
					    aisDeauthXmitComplete);
			/* If it is scanning or BSS absent, HW may go away from
			 * serving channel, which may cause driver be not able
			 * to TX mgmt frame. So we need to start a longer timer
			 * to wait HW return to serving channel.
			 * We set the time out value to 1 second because
			 * it is long enough to return to serving channel
			 * in most cases, and disconnection delay is seamless
			 * to end-user even time out.
			 */
			cnmTimerStartTimer(prAdapter,
					   &prAisFsmInfo->rDeauthDoneTimer,
					   (prAisFsmInfo->fgIsScanning
					    || prAisBssInfo->fgIsNetAbsent) ?
					   1000 : 100);
			break;

		case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
			/* send message to CNM for acquiring channel */
			prMsgChReq =
			    (struct MSG_CH_REQ *)cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_CH_REQ));
			if (!prMsgChReq) {
				/* Can't indicate CNM for channel acquiring */
				ASSERT(0);
				return;
			}

			/* release channel */
			aisFsmReleaseCh(prAdapter);

			/* zero-ize */
			kalMemZero(prMsgChReq, sizeof(struct MSG_CH_REQ));

			/* filling */
			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex =
			    prAdapter->prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval =
			    prAisFsmInfo->rChReqInfo.u4DurationMs;
			prMsgChReq->ucPrimaryChannel =
			    prAisFsmInfo->rChReqInfo.ucChannelNum;
			prMsgChReq->eRfSco = prAisFsmInfo->rChReqInfo.eSco;
			prMsgChReq->eRfBand = prAisFsmInfo->rChReqInfo.eBand;
#if CFG_SUPPORT_DBDC
			prMsgChReq->eDBDCBand = ENUM_BAND_AUTO;
#endif
			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *)prMsgChReq,
				    MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;

			break;

		case AIS_STATE_REMAIN_ON_CHANNEL:
			if (!IS_NET_ACTIVE(prAdapter,
					   prAdapter->
					   prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter,
					       prAdapter->
					       prAisBssInfo->ucBssIndex);

				/* sync with firmware */
				nicActivateNetwork(prAdapter,
						   prAdapter->
						   prAisBssInfo->ucBssIndex);
			}
#if CFG_SUPPORT_PNO
			prAisBssInfo->fgIsNetRequestInActive = FALSE;
#endif
			break;

		default:
			/* Make sure we have handle all STATEs */
			ASSERT(0);
			break;

		}
	} while (fgIsTransition);

	return;

}				/* end of aisFsmSteps() */

enum ENUM_AIS_STATE aisFsmStateSearchAction(IN struct ADAPTER *prAdapter,
					    uint8_t ucPhase)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_DESC *prBssDesc;
	enum ENUM_AIS_STATE eState = AIS_STATE_IDLE;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

#if CFG_SLT_SUPPORT
	prBssDesc = prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
	prBssDesc =
	    scanSearchBssDescByPolicy(prAdapter,
				      prAdapter->prAisBssInfo->ucBssIndex);
#endif

	if (ucPhase == AIS_FSM_STATE_SEARCH_ACTION_PHASE_0) {
		if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

			/* issue reconnect request, */
			/*and retreat to idle state for scheduling */
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);
			eState = AIS_STATE_IDLE;
		}
#if CFG_SUPPORT_ADHOC
		else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
			 || (prConnSettings->eOPMode == NET_TYPE_AUTO_SWITCH)
			 || (prConnSettings->eOPMode ==
			     NET_TYPE_DEDICATED_IBSS)) {
			prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
			prAisFsmInfo->prTargetBssDesc = NULL;
			eState = AIS_STATE_IBSS_ALONE;
		}
#endif /* CFG_SUPPORT_ADHOC */
		else {
			ASSERT(0);
			eState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		}
	} else if (ucPhase == AIS_FSM_STATE_SEARCH_ACTION_PHASE_1) {
		/* increase connection trial count for infrastructure
		 * connection
		 */
		if (prConnSettings->eOPMode == NET_TYPE_INFRA)
			prAisFsmInfo->ucConnTrialCount++;
		/* 4 <A> Try to SCAN */
		if (prAisFsmInfo->fgTryScan)
			eState = AIS_STATE_LOOKING_FOR;

		/* 4 <B> We've do SCAN already, now wait in some STATE. */
		else {
			if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

				/* issue reconnect request, and */
				/* retreat to idle state for scheduling */
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_RECONNECT);

				eState = AIS_STATE_IDLE;
			}
#if CFG_SUPPORT_ADHOC
			else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
				 || (prConnSettings->eOPMode ==
				     NET_TYPE_AUTO_SWITCH)
				 || (prConnSettings->eOPMode ==
				     NET_TYPE_DEDICATED_IBSS)) {

				prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
				prAisFsmInfo->prTargetBssDesc = NULL;

				eState = AIS_STATE_IBSS_ALONE;
			}
#endif /* CFG_SUPPORT_ADHOC */
			else {
				ASSERT(0);
				eState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
			}
		}
	} else {
#if DBG
		if (prAisBssInfo->ucReasonOfDisconnect !=
		    DISCONNECT_REASON_CODE_REASSOCIATION)
			ASSERT(UNEQUAL_MAC_ADDR
			       (prBssDesc->aucBSSID, prAisBssInfo->aucBSSID));
#endif /* DBG */
	}
	return eState;
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
void aisFsmRunEventScanDone(IN struct ADAPTER *prAdapter,
			    IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_SCN_SCAN_DONE *prScanDoneMsg;
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;
	uint8_t ucSeqNumOfCompMsg;
	struct CONNECTION_SETTINGS *prConnSettings;
	enum ENUM_SCAN_STATUS eStatus = SCAN_STATUS_DONE;
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq =
	    &prAdapter->rWifiVar.rRmReqParams;
	struct BCN_RM_PARAMS *prBcnRmParam = &prRmReq->rBcnRmParam;

	DEBUGFUNC("aisFsmRunEventScanDone()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	DBGLOG(AIS, LOUD, "EVENT-SCAN DONE: Current Time = %u\n",
	       kalGetTimeTick());

	if (prAdapter->prAisBssInfo == NULL) {
		/* This case occurs when the AIS isn't done, but the wlan0 */
		/* has changed to AP mode. And the prAisBssInfo is freed.  */
		DBGLOG(AIS, WARN, "prAisBssInfo is NULL, and then return\n");
		return;
	}

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prScanDoneMsg = (struct MSG_SCN_SCAN_DONE *)prMsgHdr;
	ASSERT(prScanDoneMsg->ucBssIndex ==
	       prAdapter->prAisBssInfo->ucBssIndex);

	ucSeqNumOfCompMsg = prScanDoneMsg->ucSeqNum;
	eStatus = prScanDoneMsg->eScanStatus;
	cnmMemFree(prAdapter, prMsgHdr);

	DBGLOG(AIS, INFO, "ScanDone %u, status(%d) native req(%u)\n",
	       ucSeqNumOfCompMsg, eStatus, prAisFsmInfo->u2SeqNumOfScanReport);

	eNextState = prAisFsmInfo->eCurrentState;

	if ((uint16_t) ucSeqNumOfCompMsg ==
		prAisFsmInfo->u2SeqNumOfScanReport) {
		prAisFsmInfo->u2SeqNumOfScanReport = AIS_SCN_REPORT_SEQ_NOT_SET;
		prConnSettings->fgIsScanReqIssued = FALSE;
		kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX,
			    (eStatus == SCAN_STATUS_DONE) ?
			    WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE);
	}
	if (ucSeqNumOfCompMsg != prAisFsmInfo->ucSeqNumOfScanReq) {
		DBGLOG(AIS, WARN,
		       "SEQ NO of AIS SCN DONE MSG is not matched %u %u\n",
		       ucSeqNumOfCompMsg, prAisFsmInfo->ucSeqNumOfScanReq);
	} else {
		prAisFsmInfo->fgIsScanning = FALSE;
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_SCAN:
			eNextState = AIS_STATE_IDLE;
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
			break;

		case AIS_STATE_ONLINE_SCAN:
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_NORMAL_TR;
#endif /* CFG_SUPPORT_ROAMING */
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
/* Support AP Selection */
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			scanGetCurrentEssChnlList(prAdapter);
#endif
/* end Support AP Selection */
			break;

		case AIS_STATE_LOOKING_FOR:
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_SEARCH;
#endif /* CFG_SUPPORT_ROAMING */
/* Support AP Selection */
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			scanGetCurrentEssChnlList(prAdapter);
#endif
/* ebd Support AP Selection */
			break;

		default:
			break;

		}
	}
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);
	if (prBcnRmParam->eState == RM_NO_REQUEST)
		return;
	/* normal mode scan done, and beacon measurement is pending,
	 ** schedule to do measurement
	 */
	if (prBcnRmParam->eState == RM_WAITING) {
		rlmDoBeaconMeasurement(prAdapter, 0);
		/* pending normal scan here, should schedule it on time */
	} else if (prBcnRmParam->rNormalScan.fgExist) {
		struct PARAM_SCAN_REQUEST_ADV *prScanRequest =
			&prBcnRmParam->rNormalScan.rScanRequest;

		DBGLOG(AIS, INFO,
		       "BCN REQ: Schedule normal scan after a beacon measurement done\n");
		prBcnRmParam->eState = RM_WAITING;
		prBcnRmParam->rNormalScan.fgExist = FALSE;
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer,
				   SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));

		aisFsmScanRequestAdv(prAdapter, prScanRequest);
		/* Radio Measurement is on-going, schedule to next Measurement
		 ** Element
		 */
	} else
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
void aisFsmRunEventAbort(IN struct ADAPTER *prAdapter,
			 IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_AIS_ABORT *prAisAbortMsg;
	struct AIS_FSM_INFO *prAisFsmInfo;
	uint8_t ucReasonOfDisconnect;
	u_int8_t fgDelayIndication;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("aisFsmRunEventAbort()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Extract information of Abort Message and then free memory. */
	prAisAbortMsg = (struct MSG_AIS_ABORT *)prMsgHdr;
	ucReasonOfDisconnect = prAisAbortMsg->ucReasonOfDisconnect;
	fgDelayIndication = prAisAbortMsg->fgDelayIndication;

	cnmMemFree(prAdapter, prMsgHdr);

	DBGLOG(AIS, STATE,
	       "EVENT-ABORT: Current State %s, ucReasonOfDisconnect:%d\n",
	       apucDebugAisState[prAisFsmInfo->eCurrentState],
	       ucReasonOfDisconnect);

	/* record join request time */
	GET_CURRENT_SYSTIME(&(prAisFsmInfo->rJoinReqTime));

	/* 4 <2> clear previous pending connection request and insert new one */
	/* Support AP Selection */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED ||
	    ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED) {
		struct STA_RECORD *prSta = prAisFsmInfo->prTargetStaRec;
		struct BSS_DESC *prBss = prAisFsmInfo->prTargetBssDesc;

		if (prSta && prBss && prSta->u2ReasonCode ==
		    REASON_CODE_DISASSOC_AP_OVERLOAD) {
			struct AIS_BLACKLIST_ITEM *prBlackList =
			    aisAddBlacklist(prAdapter, prBss);

			if (prBlackList)
				prBlackList->u2DeauthReason =
				    prSta->u2ReasonCode;
		}
		if (prAisFsmInfo->prTargetBssDesc)
			prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = TRUE;
		prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
		/* end Support AP Selection */
	} else {
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
	}

	/* to support user space triggered roaming */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_ROAMING &&
	    prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
		cnmTimerStopTimer(prAdapter,
				  &prAisFsmInfo->rSecModeChangeTimer);
		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			/* 1. release channel */
			aisFsmReleaseCh(prAdapter);
			/* 2.1 stop join timeout timer */
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rJoinTimeoutTimer);
			/* 2.2 reset local variable */
			prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
			aisFsmSteps(prAdapter, AIS_STATE_SEARCH);
		} else {
			aisFsmIsRequestPending(prAdapter,
					       AIS_REQUEST_ROAMING_SEARCH,
					       TRUE);
			aisFsmIsRequestPending(prAdapter,
					       AIS_REQUEST_ROAMING_CONNECT,
					       TRUE);
			aisFsmInsertRequest(prAdapter,
					    AIS_REQUEST_ROAMING_CONNECT);
		}
		return;
	}
	/* Support AP Selection */
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	scanGetCurrentEssChnlList(prAdapter);
#endif
	/* end Support AP Selection */

	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
	aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

	if (prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
		if (ucReasonOfDisconnect !=
		    DISCONNECT_REASON_CODE_REASSOCIATION) {
			/* 4 <3> invoke abort handler */
			aisFsmStateAbort(prAdapter, ucReasonOfDisconnect,
					 fgDelayIndication);
		} else {
			/* 1. release channel */
			aisFsmReleaseCh(prAdapter);
			/* 2.1 stop join timeout timer */
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rJoinTimeoutTimer);
			/* 2.2 reset local variable */
			prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

			prAdapter->prAisBssInfo->ucReasonOfDisconnect =
			    ucReasonOfDisconnect;
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);
		}
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
void aisFsmStateAbort(IN struct ADAPTER *prAdapter,
		      uint8_t ucReasonOfDisconnect, u_int8_t fgDelayIndication)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	u_int8_t fgIsCheckConnected;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* XXX: The wlan0 may has been changed to AP mode. */
	if (prAisBssInfo == NULL)
		return;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	fgIsCheckConnected = FALSE;

	DBGLOG(AIS, STATE, "aisFsmStateAbort DiscReason[%d], CurState[%d]\n",
	       ucReasonOfDisconnect, prAisFsmInfo->eCurrentState);

	/* 4 <1> Save information of Abort Message and then free memory. */
	prAisBssInfo->ucReasonOfDisconnect = ucReasonOfDisconnect;
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
	    prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING &&
	    ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION &&
	    ucReasonOfDisconnect != DISCONNECT_REASON_CODE_ROAMING)
		wmmNotifyDisconnected(prAdapter);

	/* 4 <2> Abort current job. */
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IDLE:
	case AIS_STATE_SEARCH:
	case AIS_STATE_JOIN_FAILURE:
		break;

	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		/* Do cancel timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_SCAN:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

		/* queue for later handling */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE)
		    == FALSE)
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
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);
		/* queue for later handling */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE)
		    == FALSE)
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
		fgIsCheckConnected = TRUE;

		/* release channel */
		aisFsmReleaseCh(prAdapter);
		break;

	case AIS_STATE_REMAIN_ON_CHANNEL:
		fgIsCheckConnected = TRUE;

		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter,
				  &prAisFsmInfo->rChannelTimeoutTimer);

		break;

	default:
		break;
	}

	if (fgIsCheckConnected
	    && (prAisBssInfo->eConnectionState ==
		PARAM_MEDIA_STATE_CONNECTED)) {

		/* switch into DISCONNECTING state for sending DEAUTH
		 * if necessary
		 */
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
#if !CFG_SUPPORT_CFG80211_AUTH
			prAisBssInfo->ucReasonOfDisconnect ==
			DISCONNECT_REASON_CODE_NEW_CONNECTION &&
#endif
			prAisBssInfo->prStaRecOfAP &&
			prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			aisFsmSteps(prAdapter, AIS_STATE_DISCONNECTING);

			return;
		}
		/* Do abort NORMAL_TR */
		aisFsmStateAbort_NORMAL_TR(prAdapter);
	}
	rlmFreeMeasurementResources(prAdapter);
	aisFsmDisconnect(prAdapter, fgDelayIndication);

	return;

}				/* end of aisFsmStateAbort() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will handle the Join Complete Event from SAA FSM
 *        for AIS FSM
 * @param[in] prMsgHdr   Message of Join Complete of SAA FSM.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventJoinComplete(IN struct ADAPTER *prAdapter,
				IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_SAA_FSM_COMP *prJoinCompMsg;
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;
	struct SW_RFB *prAssocRspSwRfb;

	DEBUGFUNC("aisFsmRunEventJoinComplete()");
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *)prMsgHdr;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

	eNextState = prAisFsmInfo->eCurrentState;

	/* Check State and SEQ NUM */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN) {
		/* Check SEQ NUM */
		if (prJoinCompMsg->ucSeqNum == prAisFsmInfo->ucSeqNumOfReqMsg)
			eNextState =
			    aisFsmJoinCompleteAction(prAdapter, prMsgHdr);
#if DBG
		else
			DBGLOG(AIS, WARN,
			       "SEQ NO of AIS JOIN COMP MSG is not matched.\n");
#endif /* DBG */
	}
	/* Support AP Selection */
	/* try to remove timeout blacklist item */
	aisRemoveDisappearedBlacklist(prAdapter);
	/* end Support AP Selection */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);
}				/* end of aisFsmRunEventJoinComplete() */

enum ENUM_AIS_STATE aisFsmJoinCompleteAction(IN struct ADAPTER *prAdapter,
					     IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_SAA_FSM_COMP *prJoinCompMsg;
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;
	struct STA_RECORD *prStaRec;
	struct SW_RFB *prAssocRspSwRfb;
	struct BSS_INFO *prAisBssInfo;
	OS_SYSTIME rCurrentTime;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("aisFsmJoinCompleteAction()");

	ASSERT(prMsgHdr);

	GET_CURRENT_SYSTIME(&rCurrentTime);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *)prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
	prAisBssInfo = prAdapter->prAisBssInfo;
	eNextState = prAisFsmInfo->eCurrentState;
	prConnSettings = &prAdapter->rWifiVar.rConnSettings;

	do {
		/* 4 <1> JOIN was successful */
		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {
			prAdapter->rWifiVar.
			    rConnSettings.fgSecModeChangeStartTimer = FALSE;

			/* 1. Reset retry count */
			prAisFsmInfo->ucConnTrialCount = 0;
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel =
			    RECONNECT_LEVEL_MIN;

			/* Completion of roaming */
			if (prAisBssInfo->eConnectionState ==
			    PARAM_MEDIA_STATE_CONNECTED) {

#if CFG_SUPPORT_ROAMING
				/* 2. Deactivate previous BSS */
				aisFsmRoamingDisconnectPrevAP(prAdapter,
							      prStaRec);

				/* 3. Update bss based on roaming staRec */
				aisUpdateBssInfoForRoamingAP(prAdapter,
							     prStaRec,
							     prAssocRspSwRfb);
#endif /* CFG_SUPPORT_ROAMING */
			} else {
				kalResetStats(prAdapter->
					prGlueInfo->prDevHandler);

				/* 4 <1.1> Change FW's Media State
				 * immediately.
				 */
				aisChangeMediaState(prAdapter,
					PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous AP's STA_RECORD_T
				 * in Driver if have.
				 */
				if ((prAisBssInfo->prStaRecOfAP) &&
				    (prAisBssInfo->prStaRecOfAP != prStaRec) &&
				    (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {

					cnmStaRecChangeState(prAdapter,
					prAisBssInfo->prStaRecOfAP,
					STA_STATE_1);
					cnmStaRecFree(prAdapter,
					prAisBssInfo->prStaRecOfAP);
				}

				/* For temp solution, need to refine */
				/* 4 <1.4> Update BSS_INFO_T */
				aisUpdateBssInfoForJOIN(prAdapter, prStaRec,
							prAssocRspSwRfb);

				/* 4 <1.3> Activate current AP's STA_RECORD_T
				 * in Driver.
				 */
				cnmStaRecChangeState(prAdapter, prStaRec,
						     STA_STATE_3);

				/* 4 <1.5> Update RSSI if necessary */
				nicUpdateRSSI(prAdapter,
					      prAdapter->
					      prAisBssInfo->ucBssIndex,
					      (int8_t) (RCPI_TO_dBm
							(prStaRec->ucRCPI)), 0);

				/* 4 <1.6> Indicate Connected Event to Host
				 * immediately.
				 */
				/* Require BSSID, Association ID,
				 * Beacon Interval
				 */
				/* .. from AIS_BSS_INFO_T */
				aisIndicationOfMediaStateToHost(prAdapter,
					PARAM_MEDIA_STATE_CONNECTED,
					FALSE);

				if (prAdapter->rWifiVar.ucTpTestMode ==
				    ENUM_TP_TEST_MODE_THROUGHPUT)
					nicEnterTPTestMode(prAdapter,
						TEST_MODE_THROUGHPUT);
				else if (prAdapter->rWifiVar.ucTpTestMode ==
					 ENUM_TP_TEST_MODE_SIGMA_AC_N_PMF)
					nicEnterTPTestMode(prAdapter,
					TEST_MODE_SIGMA_AC_N_PMF);
				else if (prAdapter->rWifiVar.ucTpTestMode ==
					 ENUM_TP_TEST_MODE_SIGMA_WMM_PS)
					nicEnterTPTestMode(prAdapter,
						TEST_MODE_SIGMA_WMM_PS);
			}

#if CFG_SUPPORT_ROAMING
			/* if user space roaming is enabled, we should
			 * disable driver/fw roaming
			 */
			if ((prAdapter->rWifiVar.
			     rConnSettings.eConnectionPolicy !=
			     CONNECT_BY_BSSID)
			     && prAdapter->rWifiVar.
			     rRoamingInfo.fgDrvRoamingAllow)
				roamingFsmRunEventStart(prAdapter);
#endif /* CFG_SUPPORT_ROAMING */
			if (aisFsmIsRequestPending
			    (prAdapter, AIS_REQUEST_ROAMING_CONNECT,
			     FALSE) == FALSE)
				prAisFsmInfo->rJoinReqTime = 0;

			/* Support AP Selection */
			prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = FALSE;
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;
			/* end Support AP Selection */

#if CFG_SUPPORT_802_11K
			aisResetNeighborApList(prAdapter);
			if (prAisFsmInfo->prTargetBssDesc->aucRrmCap[0] &
			    BIT(RRM_CAP_INFO_NEIGHBOR_REPORT_BIT))
				aisSendNeighborRequest(prAdapter);
#endif

			/* 4 <1.7> Set the Next State of AIS FSM */
			eNextState = AIS_STATE_NORMAL_TR;
		}
		/* 4 <2> JOIN was not successful */
		else {
			/* 4 <2.1> Redo JOIN process with other Auth Type
			 * if possible
			 */
			if (aisFsmStateInit_RetryJOIN(prAdapter, prStaRec) ==
			    FALSE) {
				struct BSS_DESC *prBssDesc;
				struct PARAM_SSID rSsid;
				struct CONNECTION_SETTINGS *prConnSettings;

				prConnSettings =
				    &(prAdapter->rWifiVar.rConnSettings);
				prBssDesc = prAisFsmInfo->prTargetBssDesc;

				/* 1. Increase Failure Count */
				prStaRec->ucJoinFailureCount++;

				/* 2. release channel */
				aisFsmReleaseCh(prAdapter);

				/* 3.1 stop join timeout timer */
				cnmTimerStopTimer(prAdapter,
					&prAisFsmInfo->rJoinTimeoutTimer);

				/* 3.2 reset local variable */
				prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
				/* Support AP Selection */
				prAisFsmInfo->ucJoinFailCntAfterScan++;

				kalMemZero(&rSsid, sizeof(struct PARAM_SSID));
				if (prBssDesc)
					COPY_SSID(rSsid.aucSsid,
						  rSsid.u4SsidLen,
						  prBssDesc->aucSSID,
						  prBssDesc->ucSSIDLen);
				else
					COPY_SSID(rSsid.aucSsid,
						  rSsid.u4SsidLen,
						  prConnSettings->aucSSID,
						  prConnSettings->ucSSIDLen);

				prBssDesc =
				    scanSearchBssDescByBssidAndSsid(prAdapter,
					prStaRec->aucMacAddr,
					TRUE,
					&rSsid);

#if CFG_SUPPORT_CFG80211_AUTH
				if (prBssDesc == NULL) {
					prBssDesc =
					scanSearchBssDescByBssidAndChanNum(
						prAdapter,
						prConnSettings->aucBSSID,
						TRUE,
						prConnSettings->ucChannelNum);
				}
#endif
				if (prBssDesc == NULL)
					break;

				DBGLOG(AIS, TRACE,
				       "ucJoinFailureCount=%d %d, Status=%d Reason=%d, eConnectionState=%d",
				       prStaRec->ucJoinFailureCount,
				       prBssDesc->ucJoinFailureCount,
				       prStaRec->u2StatusCode,
				       prStaRec->u2ReasonCode,
				       prAisBssInfo->eConnectionState);

				/* ASSERT(prBssDesc); */
				/* ASSERT(prBssDesc->fgIsConnecting); */
				prBssDesc->u2JoinStatus =
				    prStaRec->u2StatusCode;
				prBssDesc->ucJoinFailureCount++;
				if (prBssDesc->ucJoinFailureCount >=
				    SCN_BSS_JOIN_FAIL_THRESOLD) {
					/* Support AP Selection */
					aisAddBlacklist(prAdapter, prBssDesc);

					GET_CURRENT_SYSTIME
					    (&prBssDesc->rJoinFailTime);
					DBGLOG(AIS, INFO,
					       "Bss " MACSTR
					       " join fail %d times, temp disable it at time: %u\n",
					       MAC2STR(prBssDesc->aucBSSID),
					       SCN_BSS_JOIN_FAIL_THRESOLD,
					       prBssDesc->rJoinFailTime);
				}

				/* Support AP Selection */
				if (prBssDesc->prBlack)
					prBssDesc->prBlack->u2AuthStatus =
					    prStaRec->u2StatusCode;

				if (prBssDesc)
					prBssDesc->fgIsConnecting = FALSE;

				/* 3.3 Free STA-REC */
				if (prStaRec != prAisBssInfo->prStaRecOfAP)
					cnmStaRecFree(prAdapter, prStaRec);

				if (prAisBssInfo->eConnectionState ==
				    PARAM_MEDIA_STATE_CONNECTED) {
					struct PARAM_SSID rSsid;

					/* roaming fail count and time */
					prAdapter->prGlueInfo->u4RoamFailCnt++;
					prAdapter->prGlueInfo->u8RoamFailTime =
					    sched_clock();
#if CFG_SUPPORT_ROAMING
					eNextState =
					    AIS_STATE_WAIT_FOR_NEXT_SCAN;
#endif /* CFG_SUPPORT_ROAMING */

					if (prAisBssInfo->prStaRecOfAP)
						prAisBssInfo->
						    prStaRecOfAP->fgIsTxAllowed
						    = TRUE;

					if (prConnSettings->eConnectionPolicy
					    == CONNECT_BY_BSSID
					    && prBssDesc->u2JoinStatus) {
						uint32_t u4InfoBufLen = 0;
						/* For framework roaming case,
						 * if authentication is
						 * rejected, need to make
						 * driver disconnecting
						 * because wpa_supplicant will
						 * enter disconnected state in
						 * this case, otherwise,
						 * connection state between
						 * driver and supplicant will
						 * be not synchronized.
						 */
						wlanoidSetDisassociate
						    (prAdapter, NULL, 0,
						     &u4InfoBufLen);
						eNextState =
						    prAisFsmInfo->eCurrentState;
						break;
					}
					COPY_SSID(rSsid.aucSsid,
						  rSsid.u4SsidLen,
						  prAisBssInfo->aucSSID,
						  prAisBssInfo->ucSSIDLen);
					prAisFsmInfo->prTargetBssDesc =
					    scanSearchBssDescByBssidAndSsid
					    (prAdapter, prAisBssInfo->aucBSSID,
					     TRUE, &rSsid);
					prAisFsmInfo->prTargetStaRec =
					    prAisBssInfo->prStaRecOfAP;
					ASSERT(prAisFsmInfo->prTargetBssDesc);
					if (!prAisFsmInfo->prTargetBssDesc)
						DBGLOG(AIS, ERROR,
						       "Can't retrieve target bss descriptor\n");
				} else if (prAisFsmInfo->rJoinReqTime != 0
					   && CHECK_FOR_TIMEOUT(rCurrentTime,
						prAisFsmInfo->rJoinReqTime,
						SEC_TO_SYSTIME
						(AIS_JOIN_TIMEOUT))) {
					/* 4.a temrminate join operation */
					eNextState = AIS_STATE_JOIN_FAILURE;
				} else if (prAisFsmInfo->rJoinReqTime != 0
					   && prBssDesc->ucJoinFailureCount >=
					   SCN_BSS_JOIN_FAIL_THRESOLD
					   && prBssDesc->u2JoinStatus) {
					/* AP reject STA for
					 * STATUS_CODE_ASSOC_DENIED_AP_OVERLOAD
					 * , or AP block STA
					 */
					eNextState = AIS_STATE_JOIN_FAILURE;
				} else {
					/* 4.b send reconnect request */
					aisFsmInsertRequest(prAdapter,
						AIS_REQUEST_RECONNECT);

					eNextState = AIS_STATE_IDLE;
				}
			}
		}
	} while (0);
	return eNextState;
}

#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will handle the Grant Msg of IBSS Create which was
 *        sent by CNM to indicate that channel was changed for creating IBSS.
 *
 * @param[in] prAdapter  Pointer of ADAPTER_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmCreateIBSS(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;

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
 * @brief This function will handle the Grant Msg of IBSS Merge which was
 *        sent by CNM to indicate that channel was changed for merging IBSS.
 *
 * @param[in] prAdapter  Pointer of ADAPTER_T
 * @param[in] prStaRec   Pointer of STA_RECORD_T for merge
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmMergeIBSS(IN struct ADAPTER *prAdapter,
		     IN struct STA_RECORD *prStaRec)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;
	struct BSS_INFO *prAisBssInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	do {

		eNextState = prAisFsmInfo->eCurrentState;

		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_IBSS_MERGE:
			{
				struct BSS_DESC *prBssDesc;

				/* 4 <1.1> Change FW's Media State
				 * immediately.
				 */
				aisChangeMediaState(prAdapter,
					PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous Peers'
				 * STA_RECORD_T in Driver if have.
				 */
				bssInitializeClientList(prAdapter,
							prAisBssInfo);

				/* 4 <1.3> Unmark connection flag of previous
				 * BSS_DESC_T.
				 */
				prBssDesc =
				    scanSearchBssDescByBssid(prAdapter,
					prAisBssInfo->aucBSSID);
				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = FALSE;
				}
				/* 4 <1.4> Add Peers' STA_RECORD_T to
				 * Client List
				 */
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

				/* 4 <1.5> Activate current Peer's STA_RECORD_T
				 * in Driver.
				 */
				cnmStaRecChangeState(prAdapter, prStaRec,
						     STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				/* 4 <1.6> Update BSS_INFO_T */
				aisUpdateBssInfoForMergeIBSS(prAdapter,
							     prStaRec);

				/* 4 <1.7> Enable other features */

				/* 4 <1.8> Indicate Connected Event to Host
				 * immediately.
				 */
				aisIndicationOfMediaStateToHost(prAdapter,
					PARAM_MEDIA_STATE_CONNECTED,
					FALSE);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter);

#if CFG_SLT_SUPPORT
				prAdapter->rWifiVar.rSltInfo.prPseudoStaRec =
				    prStaRec;
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
void aisFsmRunEventFoundIBSSPeer(IN struct ADAPTER *prAdapter,
				 IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_AIS_IBSS_PEER_FOUND *prAisIbssPeerFoundMsg;
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prAisBssInfo;
	struct BSS_DESC *prBssDesc;
	u_int8_t fgIsMergeIn;

	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	prAisIbssPeerFoundMsg = (struct MSG_AIS_IBSS_PEER_FOUND *)prMsgHdr;

	ASSERT(prAisIbssPeerFoundMsg->ucBssIndex ==
	       prAdapter->prAisBssInfo->ucBssIndex);

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

				/* 4 <1.1> Change FW's Media State
				 * immediately.
				 */
				aisChangeMediaState(prAdapter,
					PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Add Peers' STA_RECORD_T to
				 * Client List
				 */
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				/* 4 <1.3> Mark connection flag of
				 * BSS_DESC_T.
				 */
				prBssDesc =
				    scanSearchBssDescByTA(prAdapter,
							  prStaRec->aucMacAddr);

				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					/* Should be able to find a
					 * BSS_DESC_T here.
					 */
					ASSERT(0);
				}

				/* 4 <1.4> Activate current Peer's
				 * STA_RECORD_T in Driver.
				 */
				/* TODO(Kevin): TBD */
				prStaRec->fgIsQoS = TRUE;
#else
				/* 4 <1.3> Mark connection flag
				 * of BSS_DESC_T.
				 */
				prBssDesc =
				    scanSearchBssDescByBssid(prAdapter,
					prAisBssInfo->aucBSSID);

				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					/* Should be able to find a
					 * BSS_DESC_T here.
					 */
					ASSERT(0);
				}

				/* 4 <1.4> Activate current Peer's STA_RECORD_T
				 * in Driver.
				 */
				/* TODO(Kevin): TBD */
				prStaRec->fgIsQoS = FALSE;

#endif

				cnmStaRecChangeState(prAdapter, prStaRec,
						     STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				/* 4 <1.6> sync. to firmware */
				nicUpdateBss(prAdapter,
					     prAdapter->
					     prAisBssInfo->ucBssIndex);

				/* 4 <1.7> Indicate Connected Event to Host
				 * immediately.
				 */
				aisIndicationOfMediaStateToHost(prAdapter,
					PARAM_MEDIA_STATE_CONNECTED,
					FALSE);

				/* 4 <1.8> indicate PM for connected */
				nicPmIndicateBssConnected(prAdapter,
					prAdapter->prAisBssInfo->ucBssIndex);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter);
			}
			/* 4 <2> We need 'merge out' to this IBSS */
			else {

				/* 4 <2.1> Get corresponding BSS_DESC_T */
				prBssDesc =
				    scanSearchBssDescByTA(prAdapter,
							  prStaRec->aucMacAddr);

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

				/* 4 <3.1> Add Peers' STA_RECORD_T to
				 * Client List
				 */
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				/* 4 <3.2> Activate current Peer's STA_RECORD_T
				 * in Driver.
				 */
				/* TODO(Kevin): TBD */
				prStaRec->fgIsQoS = TRUE;
#else
				/* 4 <3.2> Activate current Peer's STA_RECORD_T
				 * in Driver.
				 */
				/* TODO(Kevin): TBD */
				prStaRec->fgIsQoS = FALSE;
#endif

				cnmStaRecChangeState(prAdapter, prStaRec,
						     STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

			}
			/* 4 <4> We need 'merge out' to this IBSS */
			else {

				/* 4 <4.1> Get corresponding BSS_DESC_T */
				prBssDesc =
				    scanSearchBssDescByTA(prAdapter,
							  prStaRec->aucMacAddr);

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
 * @param[in] fgDelayIndication  Set TRUE for postponing the Disconnect
 *                               Indication.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void
aisIndicationOfMediaStateToHost(IN struct ADAPTER *prAdapter,
				enum ENUM_PARAM_MEDIA_STATE eConnectionState,
				u_int8_t fgDelayIndication)
{
	struct EVENT_CONNECTION_STATUS rEventConnStatus;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;

	DEBUGFUNC("aisIndicationOfMediaStateToHost()");

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	DBGLOG(AIS, LOUD,
	       "AIS indicate Media State to Host Current State [%d]\n",
	       prAisBssInfo->eConnectionState);

	/* NOTE(Kevin): Move following line to aisChangeMediaState()
	 * macro per CM's request.
	 */
	/* prAisBssInfo->eConnectionState = eConnectionState; */

	/* For indicating the Disconnect Event only if current media state is
	 * disconnected and we didn't do indication yet.
	 */
	DBGLOG(AIS, INFO, "Current state: %d, connection state indicated: %d\n",
	       prAisFsmInfo->eCurrentState,
	       prAisBssInfo->eConnectionStateIndicated);

	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED &&
		/* if receive DEAUTH in JOIN state, report disconnect*/
		!(prAisBssInfo->ucReasonOfDisconnect ==
		 DISCONNECT_REASON_CODE_DEAUTHENTICATED &&
		 prAisFsmInfo->eCurrentState == AIS_STATE_JOIN)) {
		if (prAisBssInfo->eConnectionStateIndicated == eConnectionState)
			return;
	}
	kalMemZero(&rEventConnStatus, sizeof(rEventConnStatus));

	if (!fgDelayIndication) {
		/* 4 <0> Cancel Delay Timer */
		prAisFsmInfo->u4PostponeIndStartTime = 0;

		/* 4 <1> Fill EVENT_CONNECTION_STATUS */
		rEventConnStatus.ucMediaStatus = (uint8_t) eConnectionState;

		if (eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			rEventConnStatus.ucReasonOfDisconnect =
			    DISCONNECT_REASON_CODE_RESERVED;

			if (prAisBssInfo->eCurrentOPMode ==
			    OP_MODE_INFRASTRUCTURE) {
				rEventConnStatus.ucInfraMode =
				    (uint8_t) NET_TYPE_INFRA;
				rEventConnStatus.u2AID =
				    prAisBssInfo->u2AssocId;
				rEventConnStatus.u2ATIMWindow = 0;
			} else if (prAisBssInfo->eCurrentOPMode ==
				OP_MODE_IBSS) {
				rEventConnStatus.ucInfraMode =
				    (uint8_t) NET_TYPE_IBSS;
				rEventConnStatus.u2AID = 0;
				rEventConnStatus.u2ATIMWindow =
				    prAisBssInfo->u2ATIMWindow;
			} else {
				ASSERT(0);
			}

			COPY_SSID(rEventConnStatus.aucSsid,
				  rEventConnStatus.ucSsidLen,
				  prConnSettings->aucSSID,
				  prConnSettings->ucSSIDLen);

			COPY_MAC_ADDR(rEventConnStatus.aucBssid,
				      prAisBssInfo->aucBSSID);

			rEventConnStatus.u2BeaconPeriod =
			    prAisBssInfo->u2BeaconInterval;
			rEventConnStatus.u4FreqInKHz =
			    nicChannelNum2Freq(prAisBssInfo->ucPrimaryChannel);

			switch (prAisBssInfo->ucNonHTBasicPhyType) {
			case PHY_TYPE_HR_DSSS_INDEX:
				rEventConnStatus.ucNetworkType =
				    (uint8_t) PARAM_NETWORK_TYPE_DS;
				break;

			case PHY_TYPE_ERP_INDEX:
				rEventConnStatus.ucNetworkType =
				    (uint8_t) PARAM_NETWORK_TYPE_OFDM24;
				break;

			case PHY_TYPE_OFDM_INDEX:
				rEventConnStatus.ucNetworkType =
				    (uint8_t) PARAM_NETWORK_TYPE_OFDM5;
				break;

			default:
				ASSERT(0);
				rEventConnStatus.ucNetworkType =
				    (uint8_t) PARAM_NETWORK_TYPE_DS;
				break;
			}
		} else {
			/* Clear the pmkid cache while media disconnect */
			secClearPmkid(prAdapter);
			rEventConnStatus.ucReasonOfDisconnect =
			    prAisBssInfo->ucReasonOfDisconnect;
		}

		/* 4 <2> Indication */
		nicMediaStateChange(prAdapter,
				    prAdapter->prAisBssInfo->ucBssIndex,
				    &rEventConnStatus);
		prAisBssInfo->eConnectionStateIndicated = eConnectionState;
		if (eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {
			prAisFsmInfo->prTargetBssDesc = NULL;
			prAisFsmInfo->prTargetStaRec = NULL;
		}
	} else {
		/* NOTE: Only delay the Indication of Disconnect Event */
		ASSERT(eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED);

		DBGLOG(AIS, INFO,
		       "Postpone the indication of Disconnect for %d seconds\n",
		       prConnSettings->ucDelayTimeOfDisconnectEvent);

		prAisFsmInfo->u4PostponeIndStartTime = kalGetTimeTick();
	}
}				/* end of aisIndicationOfMediaStateToHost() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate an Event of "Media Disconnect" to HOST
 *
 * @param[in] u4Param  Unused timer parameter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisPostponedEventOfDisconnTimeout(IN struct ADAPTER *prAdapter,
				       IN struct AIS_FSM_INFO *prAisFsmInfo)
{
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	bool fgFound = TRUE;
	bool fgIsPostponeTimeout;
	bool fgIsBeaconTimeout;

	/* firstly, check if we have started postpone indication.
	 ** otherwise, give a chance to do join before indicate to host
	 **/
	if (prAisFsmInfo->u4PostponeIndStartTime == 0)
		return;

	/* if we're in  req channel/join/search state,
	 * don't report disconnect.
	 */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN ||
	    prAisFsmInfo->eCurrentState == AIS_STATE_SEARCH ||
	    prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN)
		return;

	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	DBGLOG(AIS, EVENT, "aisPostponedEventOfDisconnTimeout\n");

	fgIsPostponeTimeout = CHECK_FOR_TIMEOUT(kalGetTimeTick(),
				prAisFsmInfo->u4PostponeIndStartTime,
				SEC_TO_MSEC
				(prConnSettings->ucDelayTimeOfDisconnectEvent));

	fgIsBeaconTimeout =
	    prAisBssInfo->ucReasonOfDisconnect ==
	    DISCONNECT_REASON_CODE_RADIO_LOST;

	/* only retry connect once when beacon timeout */
	if (!fgIsPostponeTimeout
	    && !(fgIsBeaconTimeout && prAisFsmInfo->ucConnTrialCount > 1)) {
		DBGLOG(AIS, INFO,
		       "DelayTimeOfDisconnect, don't report disconnect\n");
		return;
	}

	/* 4 <1> Deactivate previous AP's STA_RECORD_T in Driver if have. */
	if (prAisBssInfo->prStaRecOfAP) {
		/* cnmStaRecChangeState(prAdapter,
		 * prAisBssInfo->prStaRecOfAP, STA_STATE_1);
		 */

		prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *)NULL;
	}
	/* 4 <2> Remove all connection request */
	while (fgFound)
		fgFound =
		    aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT,
					   TRUE);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR)
		prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;
	prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
	prAisBssInfo->u2DeauthReason = REASON_CODE_BEACON_TIMEOUT;
	/* 4 <3> Indicate Disconnected Event to Host immediately. */
	/* Instead of indicating disconnect directly.
	 * We use aisFsmStateAbort here.
	 * Because AIS FSM might be running and need to abort from some state.
	 * For example: If driver is under LOOKING_FOR state, need abort FSM to cancel scan.
	 */
	aisFsmStateAbort(prAdapter,
			DISCONNECT_REASON_CODE_RADIO_LOST,
			FALSE);



}				/* end of aisPostponedEventOfDisconnTimeout() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will update the contain of BSS_INFO_T for AIS
 *        network once the association was completed.
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 * @param[in] prAssocRspSwRfb        Pointer to SW RFB of ASSOC RESP FRAME.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisUpdateBssInfoForJOIN(IN struct ADAPTER *prAdapter,
			     struct STA_RECORD *prStaRec,
			     struct SW_RFB *prAssocRspSwRfb)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame;
	struct BSS_DESC *prBssDesc;
	uint16_t u2IELength;
	uint8_t *pucIE;
	struct PARAM_SSID rSsid;

	DEBUGFUNC("aisUpdateBssInfoForJOIN()");

	ASSERT(prStaRec);
	ASSERT(prAssocRspSwRfb);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAssocRspFrame =
	    (struct WLAN_ASSOC_RSP_FRAME *)prAssocRspSwRfb->pvHeader;

	DBGLOG(AIS, INFO, "Update AIS_BSS_INFO_T and apply settings to MAC\n");

	/* 3 <1> Update BSS_INFO_T from AIS_FSM_INFO_T or User Settings */
	/* 4 <1.1> Setup Operation Mode */
	prAisBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;

	/* 4 <1.2> Setup SSID */
	COPY_SSID(prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen,
		  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	/* 4 <1.3> Setup Channel, Band */
	prAisBssInfo->ucPrimaryChannel =
	    prAisFsmInfo->prTargetBssDesc->ucChannelNum;
	prAisBssInfo->eBand = prAisFsmInfo->prTargetBssDesc->eBand;

	/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
	/* 4 <2.1> Save current AP's STA_RECORD_T and current AID */
	prAisBssInfo->prStaRecOfAP = prStaRec;
	prAisBssInfo->u2AssocId = prStaRec->u2AssocId;

	/* 4 <2.2> Setup Capability */
	/* Use AP's Cap Info as BSS Cap Info */
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
	else
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;

#if CFG_SUPPORT_TDLS
	prAisBssInfo->fgTdlsIsProhibited = prStaRec->fgTdlsIsProhibited;
	prAisBssInfo->fgTdlsIsChSwProhibited = prStaRec->fgTdlsIsChSwProhibited;
#endif /* CFG_SUPPORT_TDLS */

	/* 4 <2.3> Setup PHY Attributes and Basic Rate Set/Operational
	 * Rate Set
	 */
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	nicTxUpdateBssDefaultRate(prAisBssInfo);

	/* 3 <3> Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */
	/* 4 <3.1> Setup BSSID */
	COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prAssocRspFrame->aucBSSID);

	u2IELength =
	    (uint16_t) ((prAssocRspSwRfb->u2PacketLen -
			 prAssocRspSwRfb->u2HeaderLen) -
			(OFFSET_OF(struct WLAN_ASSOC_RSP_FRAME, aucInfoElem[0])
			 - WLAN_MAC_MGMT_HEADER_LEN));
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

	prBssDesc =
	    scanSearchBssDescByBssidAndSsid(prAdapter,
					    prAssocRspFrame->aucBSSID, TRUE,
					    &rSsid);

#if CFG_SUPPORT_CFG80211_AUTH
	if (prBssDesc == NULL) {
		prBssDesc = scanSearchBssDescByBssidAndChanNum(
						prAdapter,
						prConnSettings->aucBSSID,
						TRUE,
						prConnSettings->ucChannelNum);
	}
#endif

	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;
		prBssDesc->ucJoinFailureCount = 0;

		aisRemoveBlackList(prAdapter, prBssDesc);
		/* 4 <4.1> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
	} else {
		/* should never happen */
		ASSERT(0);
	}

	/* NOTE: Defer ucDTIMPeriod updating to when beacon is received
	 * after connection
	 */
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->fgTIMPresent = TRUE;
	prAisBssInfo->u2ATIMWindow = 0;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_INFRA;
#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	prAisBssInfo->ucRoamSkipTimes = ROAMING_ONE_AP_SKIP_TIMES;
	prAisBssInfo->fgGoodRcpiArea = FALSE;
	prAisBssInfo->fgPoorRcpiArea = FALSE;
#endif

	/* 4 <4.2> Update HT information and set channel */
	/* Record HT related parameters in rStaRec and rBssInfo
	 * Note: it shall be called before nicUpdateBss()
	 */
	rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	secPostUpdateAddr(prAdapter, prAdapter->prAisBssInfo);

	/* 4 <4.3> Sync with firmware for BSS-INFO */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <4.4> *DEFER OPERATION* nicPmIndicateBssConnected()
	 * will be invoked
	 */
	/* inside scanProcessBeaconAndProbeResp() after 1st beacon
	 * is received
	 */
}				/* end of aisUpdateBssInfoForJOIN() */

#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will create an Ad-Hoc network and start sending
 *        Beacon Frames.
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisUpdateBssInfoForCreateIBSS(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (prAisBssInfo->fgIsBeaconActivated)
		return;

	/* 3 <1> Update BSS_INFO_T per Network Basis */
	/* 4 <1.1> Setup Operation Mode */
	prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

	/* 4 <1.2> Setup SSID */
	COPY_SSID(prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen,
		  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
	prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *)NULL;
	prAisBssInfo->u2AssocId = 0;

	/* 4 <1.4> Setup Channel, Band and Phy Attributes */
	prAisBssInfo->ucPrimaryChannel = prConnSettings->ucAdHocChannelNum;
	prAisBssInfo->eBand = prConnSettings->eAdHocBand;

	if (prAisBssInfo->eBand == BAND_2G4) {
		/* Depend on eBand */
		prAisBssInfo->ucPhyTypeSet =
		    prAdapter->
		    rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_MIXED_11BG;
	} else {
		/* Depend on eBand */
		prAisBssInfo->ucPhyTypeSet =
		    prAdapter->
		    rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11ANAC;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_11A;
	}

	/* 4 <1.5> Setup MIB for current BSS */
	prAisBssInfo->u2BeaconInterval = prConnSettings->u2BeaconPeriod;
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = prConnSettings->u2AtimWindow;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;

	if (prConnSettings->eEncStatus == ENUM_ENCRYPTION1_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION2_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION3_ENABLED) {
		prAisBssInfo->fgIsProtection = TRUE;
	} else {
		prAisBssInfo->fgIsProtection = FALSE;
	}

	/* 3 <2> Update BSS_INFO_T common part */
	ibssInitForAdHoc(prAdapter, prAisBssInfo);
	/* 4 <2.1> Initialize client list */
	bssInitializeClientList(prAdapter, prAisBssInfo);

	/* 3 <3> Set MAC HW */
	/* 4 <3.1> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <3.2> use command packets to inform firmware */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <3.3> enable beaconing */
	bssUpdateBeaconContent(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <3.4> Update AdHoc PM parameter */
	nicPmIndicateBssCreated(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 3 <4> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

	/* 3 <5> Start IBSS Alone Timer */
	cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer,
			   SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));
}				/* end of aisCreateIBSS() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will update the contain of BSS_INFO_T for
 *        AIS network once the existing IBSS was found.
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisUpdateBssInfoForMergeIBSS(IN struct ADAPTER *prAdapter,
				  IN struct STA_RECORD *prStaRec)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_DESC *prBssDesc;
	/* UINT_16 u2IELength; */
	/* PUINT_8 pucIE; */

	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);

	if (!prAisBssInfo->fgIsBeaconActivated) {

		/* 3 <1> Update BSS_INFO_T per Network Basis */
		/* 4 <1.1> Setup Operation Mode */
		prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

		/* 4 <1.2> Setup SSID */
		COPY_SSID(prAisBssInfo->aucSSID,
			  prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen);

		/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
		prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *)NULL;
		prAisBssInfo->u2AssocId = 0;
	}
	/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
	/* 4 <2.1> Setup Capability */
	/* Use Peer's Cap Info as IBSS Cap Info */
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE) {
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
		prAisBssInfo->fgUseShortPreamble = TRUE;
	} else {
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;
		prAisBssInfo->fgUseShortPreamble = FALSE;
	}

	/* 7.3.1.4 For IBSS, the Short Slot Time subfield shall be set to 0. */
	/* Set to FALSE for AdHoc */
	prAisBssInfo->fgUseShortSlotTime = FALSE;
	prAisBssInfo->u2CapInfo &= ~CAP_INFO_SHORT_SLOT_TIME;

	if (prAisBssInfo->u2CapInfo & CAP_INFO_PRIVACY)
		prAisBssInfo->fgIsProtection = TRUE;
	else
		prAisBssInfo->fgIsProtection = FALSE;

	/* 4 <2.2> Setup PHY Attributes and Basic Rate Set/Operational
	 * Rate Set
	 */
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	rateGetDataRatesFromRateSet(prAisBssInfo->u2OperationalRateSet,
				    prAisBssInfo->u2BSSBasicRateSet,
				    prAisBssInfo->aucAllSupportedRates,
				    &prAisBssInfo->ucAllSupportedRatesLen);

	/* 3 <3> X Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */

	/* 3 <4> Update BSS_INFO_T from BSS_DESC_T */
	prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;
		/* Support AP Selection */
		aisRemoveBlackList(prAdapter, prBssDesc);

		/* 4 <4.1> Setup BSSID */
		COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prBssDesc->aucBSSID);

		/* 4 <4.2> Setup Channel, Band */
		prAisBssInfo->ucPrimaryChannel = prBssDesc->ucChannelNum;
		prAisBssInfo->eBand = prBssDesc->eBand;

		/* 4 <4.3> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
		prAisBssInfo->ucDTIMPeriod = 0;
		prAisBssInfo->u2ATIMWindow = 0;	/* TBD(Kevin) */

		prAisBssInfo->ucBeaconTimeoutCount =
		    AIS_BEACON_TIMEOUT_COUNT_ADHOC;
	} else {
		/* should never happen */
		ASSERT(0);
	}

	/* 3 <5> Set MAC HW */
	/* 4 <5.1> Find Lowest Basic Rate Index for default TX Rate of MMPDU */
	nicTxUpdateBssDefaultRate(prAisBssInfo);

	/* 4 <5.2> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <5.3> use command packets to inform firmware */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <5.4> enable beaconing */
	bssUpdateBeaconContent(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <5.5> Update AdHoc PM parameter */
	nicPmIndicateBssConnected(prAdapter,
				  prAdapter->prAisBssInfo->ucBssIndex);

	/* 3 <6> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;
}				/* end of aisUpdateBssInfoForMergeIBSS() */

#endif /* CFG_SUPPORT_ADHOC */

#if (CFG_SUPPORT_ADHOC || CFG_SUPPORT_PROBE_REQ_REPORT)

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will validate the Rx Probe Request Frame and then return
 *        result to BSS to indicate if need to send the corresponding
 *         Probe Response Frame if the specified conditions were matched.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 * @param[out] pu4ControlFlags   Control flags for replying the Probe Response
 *
 * @retval TRUE      Reply the Probe Response
 * @retval FALSE     Don't reply the Probe Response
 */
/*----------------------------------------------------------------------------*/
u_int8_t aisValidateProbeReq(IN struct ADAPTER *prAdapter,
			     IN struct SW_RFB *prSwRfb,
			     OUT uint32_t *pu4ControlFlags)
{
	struct WLAN_MAC_MGMT_HEADER *prMgtHdr;
	struct BSS_INFO *prBssInfo;
	struct IE_SSID *prIeSsid = (struct IE_SSID *)NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	uint8_t *pucIE;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;
	u_int8_t fgReplyProbeResp = FALSE;

	ASSERT(prSwRfb);
	ASSERT(pu4ControlFlags);

	prBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1> Parse Probe Req IE and Get IE ptr
	 * (SSID, Supported Rate IE, ...)
	 */
	prMgtHdr = (struct WLAN_MAC_MGMT_HEADER *)prSwRfb->pvHeader;

	u2IELength = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen;
	pucIE =
	    (uint8_t *) ((unsigned long)prSwRfb->pvHeader +
			 prSwRfb->u2HeaderLen);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (IE_ID(pucIE) == ELEM_ID_SSID) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (struct IE_SSID *)pucIE;

			break;
		}
	}			/* end of IE_FOR_EACH */

	/* 4 <2> Check network conditions */

	if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {

		if ((prIeSsid) && ((prIeSsid->ucLength ==
			BC_SSID_LEN) ||	/* WILDCARD SSID */
			EQUAL_SSID(prBssInfo->aucSSID,
			prBssInfo->ucSSIDLen,	/* CURRENT SSID */
			prIeSsid->aucSSID,
			prIeSsid->ucLength))) {
			fgReplyProbeResp = TRUE;
		}
	}

	if (prAisFsmInfo->u4AisPacketFilter & PARAM_PACKET_FILTER_PROBE_REQ) {
		DBGLOG(AIS, INFO, "[AIS] RX Probe Req Frame\n");
		kalIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb);
	}


	return fgReplyProbeResp;

}				/* end of aisValidateProbeReq() */

#endif /* CFG_SUPPORT_ADHOC || CFG_SUPPORT_PROBE_REQ_REPORT */

#if CFG_DISCONN_DEBUG_FEATURE
void aisCollectDisconnInfo(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prAisBssInfo;
	uint32_t u4BufLen = 0;
	uint8_t ucWlanIndex;
	uint8_t *pucMacAddr = NULL;
	struct AIS_DISCONN_INFO_T *prDisconn = NULL;

	if (prAdapter == NULL) {
		DBGLOG(AIS, ERROR, "Null adapter\n");
		return;
	}

	if (g_prDisconnInfo == NULL) {
		DBGLOG(AIS, ERROR, "Null g_prDisconnInfo\n");
		return;
	}

	if (g_DisconnInfoIdx >= MAX_DISCONNECT_RECORD) {
		DBGLOG(AIS, ERROR, "Invalid g_DisconnInfoIdx\n");
		return;
	}

	prDisconn = g_prDisconnInfo + g_DisconnInfoIdx;
	kalMemZero(prDisconn, sizeof(struct AIS_DISCONN_INFO_T));

	prAisBssInfo = prAdapter->prAisBssInfo;

	do_gettimeofday(&prDisconn->tv);

	prDisconn->ucTrigger = g_rDisconnInfoTemp.ucTrigger;
	prDisconn->ucDisConnReason = prAisBssInfo->ucReasonOfDisconnect;
	prDisconn->ucBcnTimeoutReason = g_rDisconnInfoTemp.ucBcnTimeoutReason;
	prDisconn->ucDisassocReason = g_rDisconnInfoTemp.ucDisassocReason;
	prDisconn->u2DisassocSeqNum = g_rDisconnInfoTemp.u2DisassocSeqNum;

#if CFG_SUPPORT_ADVANCE_CONTROL
	/* Query Average noise info */
	prDisconn->rNoise.u2Type = CMD_NOISE_HISTOGRAM_TYPE;
	prDisconn->rNoise.u2Len = sizeof(struct CMD_NOISE_HISTOGRAM_REPORT);
	prDisconn->rNoise.ucAction = CMD_NOISE_HISTOGRAM_GET;
	wlanAdvCtrl(prAdapter, &prDisconn->rNoise,
		sizeof(struct CMD_NOISE_HISTOGRAM_REPORT), &u4BufLen, FALSE);
#endif

	/* Query Beacon RSSI info */
	wlanQueryRssi(prAdapter, &prDisconn->rBcnRssi,
		sizeof(prDisconn->rBcnRssi), &u4BufLen, FALSE);

	/* Store target sta rec and get wlan index */
	if (prAisBssInfo->prStaRecOfAP) {
		kalMemCopy(&prDisconn->rStaRec,
			prAisBssInfo->prStaRecOfAP,
			sizeof(struct STA_RECORD));

		ucWlanIndex = prAisBssInfo->prStaRecOfAP->ucWlanIndex;
	} else if (!wlanGetWlanIdxByAddress(prAdapter, NULL, &ucWlanIndex)) {
		DBGLOG(AIS, LOUD, "Null wlan index\n");
		goto leave;
	}

	/* Query WTBL info */
	prDisconn->rHwInfo.u4Index = ucWlanIndex;
	prDisconn->rHwInfo.rWtblRxCounter.fgRxCCSel = FALSE;
	wlanQueryWlanInfo(prAdapter, &prDisconn->rHwInfo,
		sizeof(struct PARAM_HW_WLAN_INFO), &u4BufLen, FALSE);

	/* Query Statistics info */
	prDisconn->rStaStatistics.ucResetCounter = FALSE;
	pucMacAddr = wlanGetStaAddrByWlanIdx(prAdapter, ucWlanIndex);

	if (pucMacAddr) {
		COPY_MAC_ADDR(prDisconn->rStaStatistics.aucMacAddr,
			pucMacAddr);

		wlanQueryStaStatistics(prAdapter,
			&prDisconn->rStaStatistics,
			sizeof(struct PARAM_GET_STA_STATISTICS),
			&u4BufLen, FALSE);
	}

leave:
	g_DisconnInfoIdx = (g_DisconnInfoIdx + 1) % MAX_DISCONNECT_RECORD;

	/* Default value */
	kalMemZero(&g_rDisconnInfoTemp, sizeof(struct AIS_DISCONN_INFO_T));
	g_rDisconnInfoTemp.ucBcnTimeoutReason = 0xF;
	g_rDisconnInfoTemp.u2DisassocSeqNum = 0xFFFF;
}
#endif /* CFG_DISCONN_DEBUG_FEATURE */

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
void aisFsmDisconnect(IN struct ADAPTER *prAdapter,
		      IN u_int8_t fgDelayIndication)
{
	struct BSS_INFO *prAisBssInfo;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;
	cnmTimerStopTimer(prAdapter,
			  &prAdapter->rWifiVar.rAisFsmInfo.rSecModeChangeTimer);
	nicPmIndicateBssAbort(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

#if CFG_SUPPORT_ADHOC
	if (prAisBssInfo->fgIsBeaconActivated) {
		nicUpdateBeaconIETemplate(prAdapter,
					  IE_UPD_METHOD_DELETE_ALL,
					  prAdapter->prAisBssInfo->ucBssIndex,
					  0, NULL, 0);

		prAisBssInfo->fgIsBeaconActivated = FALSE;
	}
#endif

	rlmBssAborted(prAdapter, prAisBssInfo);

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and send Deauth
	 * if needed.
	 */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {

		{
			if (prAdapter->rWifiVar.ucTpTestMode !=
			    ENUM_TP_TEST_MODE_NORMAL)
				nicEnterTPTestMode(prAdapter, TEST_MODE_NONE);

#if 0
			if (prAdapter->rWifiVar.ucSigmaTestMode)
				nicEnterTPTestMode(prAdapter, TEST_MODE_NONE);
#endif
		}
		/* for NO TIM IE case */
		if (!prAisBssInfo->fgTIMPresent) {
			nicConfigPowerSaveProfile(prAdapter,
						  prAisBssInfo->ucBssIndex,
						  Param_PowerModeFast_PSP,
						  FALSE, PS_CALLER_NO_TIM);
		}

		if (prAisBssInfo->ucReasonOfDisconnect ==
		    DISCONNECT_REASON_CODE_RADIO_LOST) {
			scanRemoveBssDescByBssid(prAdapter,
						 prAisBssInfo->aucBSSID);

			/* remove from scanning results as well */
			wlanClearBssInScanningResult(prAdapter,
						     prAisBssInfo->aucBSSID);

			/* trials for re-association */
			if (fgDelayIndication) {
				aisFsmIsRequestPending(prAdapter,
						       AIS_REQUEST_RECONNECT,
						       TRUE);
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_RECONNECT);
			}
		} else {
			scanRemoveConnFlagOfBssDescByBssid(prAdapter,
				prAisBssInfo->aucBSSID);
			prAdapter->rWifiVar.rAisFsmInfo.
			    prTargetBssDesc->fgIsConnected = FALSE;
			prAdapter->rWifiVar.rAisFsmInfo.
			    prTargetBssDesc->fgIsConnecting = FALSE;
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

#if CFG_DISCONN_DEBUG_FEATURE
	aisCollectDisconnInfo(prAdapter);
#endif

	/* 4 <4> Change Media State immediately. */
	if (prAisBssInfo->ucReasonOfDisconnect !=
	    DISCONNECT_REASON_CODE_REASSOCIATION) {
		aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

		/* 4 <4.1> sync. with firmware */
		nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
	}

	if (!fgDelayIndication) {
		/* 4 <5> Deactivate previous AP's STA_RECORD_T or all Clients in
		 * Driver if have.
		 */
		if (prAisBssInfo->prStaRecOfAP) {
			/* cnmStaRecChangeState(prAdapter,
			 * prAisBssInfo->prStaRecOfAP, STA_STATE_1);
			 */
			prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *)NULL;
		}
	}
#if CFG_SUPPORT_ROAMING
	roamingFsmRunEventAbort(prAdapter);

	/* clear pending roaming connection request */
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
#endif /* CFG_SUPPORT_ROAMING */

	/* 4 <6> Indicate Disconnected Event to Host */
	aisIndicationOfMediaStateToHost(prAdapter,
					PARAM_MEDIA_STATE_DISCONNECTED,
					fgDelayIndication);

	/* 4 <7> Trigger AIS FSM */
	aisFsmSteps(prAdapter, AIS_STATE_IDLE);
}				/* end of aisFsmDisconnect() */

static void aisFsmRunEventScanDoneTimeOut(IN struct ADAPTER *prAdapter,
					  unsigned long ulParam)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("aisFsmRunEventScanDoneTimeOut()");

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	DBGLOG(AIS, STATE, "aisFsmRunEventScanDoneTimeOut Current[%d] Seq=%u\n",
	       prAisFsmInfo->eCurrentState, prAisFsmInfo->ucSeqNumOfScanReq);

	/* try to stop scan in CONNSYS */
	aisFsmStateAbort_SCAN(prAdapter);
}				/* end of aisFsmBGSleepTimeout() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate an Event of "Background Scan Time-Out"
 *        to AIS FSM.
 * @param[in] u4Param  Unused timer parameter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventBGSleepTimeOut(IN struct ADAPTER *prAdapter,
				  unsigned long ulParamPtr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;

	DEBUGFUNC("aisFsmRunEventBGSleepTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		DBGLOG(AIS, LOUD,
		       "EVENT - SCAN TIMER: Idle End - Current Time = %u\n",
		       kalGetTimeTick());

		eNextState = AIS_STATE_LOOKING_FOR;

		SET_NET_PWR_STATE_ACTIVE(prAdapter,
					 prAdapter->prAisBssInfo->ucBssIndex);

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
 * @brief This function will indicate an Event of "IBSS ALONE Time-Out" to
 *        AIS FSM.
 * @param[in] u4Param  Unused timer parameter
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventIbssAloneTimeOut(IN struct ADAPTER *prAdapter,
				    unsigned long ulParamPtr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;

	DEBUGFUNC("aisFsmRunEventIbssAloneTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:

		/* There is no one participate in our AdHoc during this
		 * TIMEOUT Interval so go back to search for a valid
		 * IBSS again.
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
void aisFsmRunEventJoinTimeout(IN struct ADAPTER *prAdapter,
			       unsigned long ulParamPtr)
{
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	enum ENUM_AIS_STATE eNextState;
	OS_SYSTIME rCurrentTime;

	DEBUGFUNC("aisFsmRunEventJoinTimeout()");

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_JOIN:
		DBGLOG(AIS, WARN, "EVENT- JOIN TIMEOUT\n");

		/* 1. Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter);

		/* 2. Increase Join Failure Count */
		/* Support AP Selection */
		aisAddBlacklist(prAdapter, prAisFsmInfo->prTargetBssDesc);
		prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount++;

		if (prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount <
		    JOIN_MAX_RETRY_FAILURE_COUNT) {
			/* 3.1 Retreat to AIS_STATE_SEARCH state for next try */
			eNextState = AIS_STATE_SEARCH;
		} else if (prAisBssInfo->eConnectionState ==
			   PARAM_MEDIA_STATE_CONNECTED) {
			/* roaming cases */
			/* 3.2 Retreat to AIS_STATE_WAIT_FOR_NEXT_SCAN state for
			 * next try
			 */
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else
		if (prAisFsmInfo->rJoinReqTime != 0 && !CHECK_FOR_TIMEOUT
			(rCurrentTime, prAisFsmInfo->rJoinReqTime,
			 SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
			/* 3.3 Retreat to AIS_STATE_WAIT_FOR_NEXT_SCAN state
			 * for next try
			 */
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else {
			/* 3.4 Retreat to AIS_STATE_JOIN_FAILURE to
			 * terminate join operation
			 */
			eNextState = AIS_STATE_JOIN_FAILURE;
		}

		break;

	case AIS_STATE_NORMAL_TR:
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);
		prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

		/* 2. process if there is pending scan */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) ==
		    TRUE) {
			wlanClearScanningResult(prAdapter);
			eNextState = AIS_STATE_ONLINE_SCAN;
		}
		/* 3. Process for pending roaming scan */
		else if (aisFsmIsRequestPending
			 (prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE) == TRUE)
			eNextState = AIS_STATE_LOOKING_FOR;
		/* 4. Process for pending roaming scan */
		else if (aisFsmIsRequestPending
			 (prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE) == TRUE)
			eNextState = AIS_STATE_SEARCH;
		else if (aisFsmIsRequestPending
			 (prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL,
			  TRUE) == TRUE)
			eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;

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

void aisFsmRunEventDeauthTimeout(IN struct ADAPTER *prAdapter,
				 unsigned long ulParamPtr)
{
	aisDeauthXmitComplete(prAdapter, NULL, TX_RESULT_LIFE_TIMEOUT);
}

void aisFsmRunEventSecModeChangeTimeout(IN struct ADAPTER *prAdapter,
					unsigned long ulParamPtr)
{
	DBGLOG(AIS, WARN,
	       "Beacon security mode change timeout, trigger disconnect!\n");
	aisBssSecurityChanged(prAdapter);
}

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
void aisTest(void)
{
	struct MSG_AIS_ABORT *prAisAbortMsg;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t aucSSID[] = "pci-11n";
	uint8_t ucSSIDLen = 7;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* Set Connection Request Issued Flag */
	prConnSettings->fgIsConnReqIssued = TRUE;
	prConnSettings->ucSSIDLen = ucSSIDLen;
	kalMemCopy(prConnSettings->aucSSID, aucSSID, ucSSIDLen);

	prAisAbortMsg =
	    (struct MSG_AIS_ABORT *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						sizeof(struct MSG_AIS_ABORT));
	if (!prAisAbortMsg) {

		ASSERT(0);	/* Can't trigger SCAN FSM */
		return;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_HEM_AIS_FSM_ABORT;

	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prAisAbortMsg,
		    MSG_SEND_METHOD_BUF);

	wifi_send_msg(INDX_WIFI, MSG_ID_WIFI_IST, 0);
}
#endif /* CFG_TEST_MGMT_FSM */

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is used to handle OID_802_11_BSSID_LIST_SCAN
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[in] prSsid     Pointer of SSID_T if specified
 * \param[in] pucIe      Pointer to buffer of extra information elements
 *                       to be attached
 * \param[in] u4IeLength Length of information elements
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void aisFsmScanRequest(IN struct ADAPTER *prAdapter,
		       IN struct PARAM_SSID *prSsid, IN uint8_t *pucIe,
		       IN uint32_t u4IeLength)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;

	DEBUGFUNC("aisFsmScanRequest()");

	ASSERT(prAdapter);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prScanRequest = &(prAisFsmInfo->rScanRequest);

	DBGLOG(SCN, TRACE, "eCurrentState=%d, fgIsScanReqIssued=%d\n",
	       prAisFsmInfo->eCurrentState, prConnSettings->fgIsScanReqIssued);
	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
		scanInitEssResult(prAdapter);
		kalMemZero(prScanRequest,
			   sizeof(struct PARAM_SCAN_REQUEST_ADV));
		prScanRequest->pucIE = prAisFsmInfo->aucScanIEBuf;

		if (prSsid == NULL) {
			prScanRequest->u4SsidNum = 0;
		} else {
			prScanRequest->u4SsidNum = 1;

			COPY_SSID(prScanRequest->rSsid[0].aucSsid,
				  prScanRequest->rSsid[0].u4SsidLen,
				  prSsid->aucSsid, prSsid->u4SsidLen);
		}

		if (u4IeLength > 0 && u4IeLength <= MAX_IE_LENGTH) {
			prScanRequest->u4IELength = u4IeLength;
			kalMemCopy(prScanRequest->pucIE, pucIe, u4IeLength);
		} else {
			prScanRequest->u4IELength = 0;
		}
		prScanRequest->ucScanType = SCAN_TYPE_ACTIVE_SCAN;
		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode ==
			    OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished ==
			    FALSE) {
				/* 802.1x might not finished yet, pend it for
				 * later handling ..
				 */
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(SCN, WARN,
					"Scan Request with channel granted for join operation: %d, %d",
					prAisFsmInfo->fgIsChannelGranted,
					prAisFsmInfo->fgIsChannelRequested);
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
		DBGLOG(SCN, WARN, "Scan Request dropped. (state: %d)\n",
		       prAisFsmInfo->eCurrentState);
	}

}				/* end of aisFsmScanRequest() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is used to handle OID_802_11_BSSID_LIST_SCAN
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 * \param[in] prRequestIn  scan request
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void
aisFsmScanRequestAdv(IN struct ADAPTER *prAdapter,
		     IN struct PARAM_SCAN_REQUEST_ADV *prRequestIn)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;

	DEBUGFUNC("aisFsmScanRequestAdv()");

	ASSERT(prAdapter);
	if (!prRequestIn) {
		log_dbg(SCN, WARN, "Scan request is NULL\n");
		return;
	}

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prScanRequest = &(prAisFsmInfo->rScanRequest);

	DBGLOG(SCN, TRACE, "eCurrentState=%d, fgIsScanReqIssued=%d\n",
	       prAisFsmInfo->eCurrentState, prConnSettings->fgIsScanReqIssued);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
		scanInitEssResult(prAdapter);

		kalMemCopy(prScanRequest, prRequestIn,
			   sizeof(struct PARAM_SCAN_REQUEST_ADV));
		prScanRequest->pucIE = prAisFsmInfo->aucScanIEBuf;

		if (prRequestIn->u4IELength > 0 &&
		    prRequestIn->u4IELength <= MAX_IE_LENGTH) {
			prScanRequest->u4IELength = prRequestIn->u4IELength;
			kalMemCopy(prScanRequest->pucIE, prRequestIn->pucIE,
				   prScanRequest->u4IELength);
		} else {
			prScanRequest->u4IELength = 0;
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode ==
			    OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished ==
			    FALSE) {
				/* 802.1x might not finished yet, pend it for
				 * later handling ..
				 */
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(SCN, WARN,
					"Scan Request with channel granted for join operation: %d, %d",
					prAisFsmInfo->fgIsChannelGranted,
					prAisFsmInfo->fgIsChannelRequested);
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
	} else if (prAdapter->rWifiVar.rRmReqParams.rBcnRmParam.eState ==
		   RM_ON_GOING) {
		struct NORMAL_SCAN_PARAMS *prNormalScan =
			&prAdapter->rWifiVar.rRmReqParams.rBcnRmParam
				 .rNormalScan;
		struct PARAM_SCAN_REQUEST_ADV *prScanRequest =
			&prNormalScan->rScanRequest;

		prNormalScan->fgExist = TRUE;
		kalMemCopy(prScanRequest, prRequestIn,
			sizeof(struct PARAM_SCAN_REQUEST_ADV));
		if (prRequestIn->u4IELength > 0 &&
		prRequestIn->u4IELength <= MAX_IE_LENGTH) {
			prScanRequest->u4IELength =
			    prRequestIn->u4IELength;
			kalMemCopy(prNormalScan->aucScanIEBuf,
				prRequestIn->pucIE,
				prRequestIn->u4IELength);
		} else {
			prScanRequest->u4IELength = 0;
		}

		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
		DBGLOG(AIS, INFO,
		       "BCN REQ: Buffer normal scan while Beacon request is scanning\n");
	} else {
		DBGLOG(SCN, WARN, "Scan Request dropped. (state: %d)\n",
		       prAisFsmInfo->eCurrentState);
	}

}				/* end of aisFsmScanRequestAdv() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is invoked when CNM granted channel privilege
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventChGrant(IN struct ADAPTER *prAdapter,
			   IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_CH_GRANT *prMsgChGrant;
	uint8_t ucTokenID;
	uint32_t u4GrantInterval;
	uint32_t u4Entry = 0;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prMsgChGrant = (struct MSG_CH_GRANT *)prMsgHdr;

	ucTokenID = prMsgChGrant->ucTokenID;
	u4GrantInterval = prMsgChGrant->u4GrantInterval;

#if CFG_SISO_SW_DEVELOP
	/* Driver record granted CH in BSS info */
	prAisBssInfo->fgIsGranted = TRUE;
	prAisBssInfo->eBandGranted = prMsgChGrant->eRfBand;
	prAisBssInfo->ucPrimaryChannelGranted = prMsgChGrant->ucPrimaryChannel;
#endif

	/* 1. free message */
	cnmMemFree(prAdapter, prMsgHdr);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN
	    && prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

		/* 3. state transition to join/ibss-alone/ibss-merge */
		/* 3.1 set timeout timer in cases join could not be completed */
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rJoinTimeoutTimer,
				   prAisFsmInfo->u4ChGrantedInterval -
				   AIS_JOIN_CH_GRANT_THRESHOLD);
		/* 3.2 set local variable to indicate join timer is ticking */
		prAisFsmInfo->fgIsInfraChannelFinished = FALSE;

		/* 3.3 switch to join state */
		/* Three cases can switch to join state:
		 ** 1. There's an available PMKSA in wpa_supplicant
		 ** 2. found okc pmkid entry for this BSS
		 ** 3. current state is disconnected. In this case,
		 ** supplicant may not get a valid pmksa,
		 ** so no pmkid will be passed to driver, so we no need
		 ** to wait pmkid anyway.
		 */
		if (!prAdapter->rWifiVar.rConnSettings.fgOkcPmksaReady ||
		    (rsnSearchPmkidEntry
		     (prAdapter, prAisFsmInfo->prTargetBssDesc->aucBSSID,
		      &u4Entry)
		     && prAdapter->rWifiVar.
		     rAisSpecificBssInfo.arPmkidCache[u4Entry].fgPmkidExist))
			aisFsmSteps(prAdapter, AIS_STATE_JOIN);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else if (prAisFsmInfo->eCurrentState ==
		   AIS_STATE_REQ_REMAIN_ON_CHANNEL
		   && prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

#if CFG_SUPPORT_NCHO
		if (prAdapter->rNchoInfo.fgECHOEnabled == TRUE &&
		    prAdapter->rNchoInfo.fgIsSendingAF == TRUE &&
		    prAdapter->rNchoInfo.fgChGranted == FALSE) {
			DBGLOG(INIT, TRACE,
			       "NCHO complete rAisChGrntComp trace time is %u\n",
			       kalGetTimeTick());
			prAdapter->rNchoInfo.fgChGranted = TRUE;
			complete(&prAdapter->prGlueInfo->rAisChGrntComp);
		}
#endif
		/* 3.1 set timeout timer in cases upper layer
		 * cancel_remain_on_channel never comes
		 */
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rChannelTimeoutTimer,
				   prAisFsmInfo->u4ChGrantedInterval);

		/* 3.2 switch to remain_on_channel state */
		aisFsmSteps(prAdapter, AIS_STATE_REMAIN_ON_CHANNEL);

		/* 3.3. indicate upper layer for channel ready */
		kalReadyOnChannel(prAdapter->prGlueInfo,
				  prAisFsmInfo->rChReqInfo.u8Cookie,
				  prAisFsmInfo->rChReqInfo.eBand,
				  prAisFsmInfo->rChReqInfo.eSco,
				  prAisFsmInfo->rChReqInfo.ucChannelNum,
				  prAisFsmInfo->rChReqInfo.u4DurationMs);

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
void aisFsmReleaseCh(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_CH_ABORT *prMsgChAbort;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	if (prAisFsmInfo->fgIsChannelGranted == TRUE
	    || prAisFsmInfo->fgIsChannelRequested == TRUE) {

		prAisFsmInfo->fgIsChannelRequested = FALSE;
		prAisFsmInfo->fgIsChannelGranted = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChAbort =
		    (struct MSG_CH_ABORT *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						       sizeof(struct
							      MSG_CH_ABORT));
		if (!prMsgChAbort) {
			ASSERT(0);	/* Can't release Channel to CNM */
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
		prMsgChAbort->ucTokenID = prAisFsmInfo->ucSeqNumOfChReq;
#if CFG_SUPPORT_DBDC
		prMsgChAbort->eDBDCBand = ENUM_BAND_AUTO;
#endif /*CFG_SUPPORT_DBDC */
		mboxSendMsg(prAdapter, MBOX_ID_0,
			    (struct MSG_HDR *)prMsgChAbort,
			    MSG_SEND_METHOD_BUF);
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
void aisBssBeaconTimeout(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prAisBssInfo;
	u_int8_t fgDoAbortIndication = FALSE;
	struct CONNECTION_SETTINGS *prConnSettings;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Diagnose Connection for Beacon Timeout Event */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			struct STA_RECORD *prStaRec =
			    prAisBssInfo->prStaRecOfAP;

			if (prStaRec)
				fgDoAbortIndication = TRUE;

		} else if (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
			fgDoAbortIndication = TRUE;
		}
	}
	/* 4 <2> invoke abort handler */
	if (fgDoAbortIndication) {
#if CFG_DISCONN_DEBUG_FEATURE
		g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_PASSIVE;
#endif
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
		if (prConnSettings->eReConnectLevel <
			RECONNECT_LEVEL_USER_SET) {
			prConnSettings->eReConnectLevel =
			    RECONNECT_LEVEL_BEACON_TIMEOUT;
#if CFG_SUPPORT_CFG80211_AUTH
			prConnSettings->fgIsConnReqIssued = FALSE;
#else
			prConnSettings->fgIsConnReqIssued = TRUE;
#endif
		}
		DBGLOG(AIS, EVENT, "aisBssBeaconTimeout\n");
		aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_RADIO_LOST,
				 TRUE);
	}
}				/* end of aisBssBeaconTimeout() */

void aisBssSecurityChanged(struct ADAPTER *prAdapter)
{
	prAdapter->rWifiVar.rConnSettings.fgIsDisconnectedByNonRequest = TRUE;
	aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_DEAUTHENTICATED,
			 FALSE);
}

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
void aisBssLinkDown(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prAisBssInfo;
	u_int8_t fgDoAbortIndication = FALSE;
	struct CONNECTION_SETTINGS *prConnSettings;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Diagnose Connection for Beacon Timeout Event */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			struct STA_RECORD *prStaRec =
			    prAisBssInfo->prStaRecOfAP;

			if (prStaRec)
				fgDoAbortIndication = TRUE;

		} else if (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
			fgDoAbortIndication = TRUE;
		}
	}
	/* 4 <2> invoke abort handler */
	if (fgDoAbortIndication) {
#if CFG_DISCONN_DEBUG_FEATURE
		g_rDisconnInfoTemp.ucTrigger = DISCONNECT_TRIGGER_ACTIVE;
#endif
		prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
		DBGLOG(AIS, EVENT, "aisBssLinkDown\n");
		aisFsmStateAbort(prAdapter,
				 DISCONNECT_REASON_CODE_DISASSOCIATED, FALSE);
	}

	/* kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
	 * WLAN_STATUS_SCAN_COMPLETE, NULL, 0);
	 */
}				/* end of aisBssBeaconTimeout() */

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
uint32_t
aisDeauthXmitComplete(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo,
		      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct AIS_FSM_INFO *prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_DISCONNECTING) {
		DBGLOG(AIS, EVENT, "aisDeauthXmitComplete\n");
		if (rTxDoneStatus != TX_RESULT_DROPPED_IN_DRIVER
		    && rTxDoneStatus != TX_RESULT_QUEUE_CLEARANCE)
#if CFG_SUPPORT_CFG80211_AUTH
			aisFsmStateAbort(prAdapter,
				DISCONNECT_REASON_CODE_DEAUTHENTICATED,
				FALSE);
#else
			aisFsmStateAbort(prAdapter,
				DISCONNECT_REASON_CODE_NEW_CONNECTION,
				FALSE);
#endif
	} else {
		DBGLOG(AIS, WARN,
		       "DEAUTH frame transmitted without further handling");
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of aisDeauthXmitComplete() */

#if CFG_SUPPORT_ROAMING
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate an Event of "Looking for a candidate
 *         due to weak signal" to AIS FSM.
 * @param[in] u4ReqScan  Requesting Scan or not
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventRoamingDiscovery(IN struct ADAPTER *prAdapter,
				    uint32_t u4ReqScan)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	enum ENUM_AIS_REQUEST_TYPE eAisRequest = AIS_REQUEST_NUM;

	DBGLOG(AIS, LOUD, "aisFsmRunEventRoamingDiscovery()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* search candidates by best rssi */
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	/* TODO: Stop roaming event in FW */
#if CFG_SUPPORT_WFD
#if CFG_ENABLE_WIFI_DIRECT
	{
		/* Check WFD is running */
		struct WFD_CFG_SETTINGS *prWfdCfgSettings =
		    (struct WFD_CFG_SETTINGS *)NULL;

		prWfdCfgSettings = &(prAdapter->rWifiVar.rWfdConfigureSettings);
		if ((prWfdCfgSettings->ucWfdEnable != 0)) {
			DBGLOG(ROAMING, INFO,
			       "WFD is running. Stop roaming.\n");
			roamingFsmRunEventRoam(prAdapter);
			roamingFsmRunEventFail(prAdapter,
					       ROAMING_FAIL_REASON_NOCANDIDATE);
			return;
		}
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
	}

	if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR
	    && prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
		if (eAisRequest == AIS_REQUEST_ROAMING_SEARCH) {
			prAisFsmInfo->fgTargetChnlScanIssued = TRUE;
			aisFsmSteps(prAdapter, AIS_STATE_LOOKING_FOR);
		} else
			aisFsmSteps(prAdapter, AIS_STATE_SEARCH);
	} else {
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH,
			TRUE);
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT,
			TRUE);

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
enum ENUM_AIS_STATE aisFsmRoamingScanResultsUpdate(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_AIS_STATE eNextState;

	DBGLOG(AIS, LOUD, "->aisFsmRoamingScanResultsUpdate()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prRoamingFsmInfo =
	    (struct ROAMING_INFO *)&(prAdapter->rWifiVar.rRoamingInfo);

	roamingFsmScanResultsUpdate(prAdapter);

	eNextState = prAisFsmInfo->eCurrentState;
	if (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) {
		roamingFsmRunEventRoam(prAdapter);
		eNextState = AIS_STATE_SEARCH;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
		eNextState = AIS_STATE_SEARCH;
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
void aisFsmRoamingDisconnectPrevAP(IN struct ADAPTER *prAdapter,
				   IN struct STA_RECORD *prTargetStaRec)
{
	struct BSS_INFO *prAisBssInfo;

	DBGLOG(AIS, EVENT, "aisFsmRoamingDisconnectPrevAP()");

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;
	if (prAisBssInfo->prStaRecOfAP != prTargetStaRec)
		wmmNotifyDisconnected(prAdapter);

	nicPmIndicateBssAbort(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* Not invoke rlmBssAborted() here to avoid prAisBssInfo->fg40mBwAllowed
	 * to be reset. RLM related parameters will be reset again when handling
	 * association response in rlmProcessAssocRsp(). 20110413
	 */
	/* rlmBssAborted(prAdapter, prAisBssInfo); */

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and
	 * send Deauth if needed.
	 */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		struct PARAM_SSID rSsid;
		struct BSS_DESC *prBssDesc = NULL;

		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prAisBssInfo->aucSSID,
			  prAisBssInfo->ucSSIDLen);
		prBssDesc =
		    scanSearchBssDescByBssidAndSsid(prAdapter,
						    prAisBssInfo->aucBSSID,
						    TRUE, &rSsid);
		if (prBssDesc) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;
		}
	}

	/* 4 <4> Change Media State immediately. */
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECT_PREV);

	/* 4 <4.1> sync. with firmware */
	/* Virtial BSSID */
	prTargetStaRec->ucBssIndex = (prAdapter->ucHwBssIdNum + 1);
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	secRemoveBssBcEntry(prAdapter, prAisBssInfo, TRUE);
	prTargetStaRec->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	/* before deactivate previous AP, should move its pending MSDUs
	 ** to the new AP
	 */
	if (prAisBssInfo->prStaRecOfAP)
		if (prAisBssInfo->prStaRecOfAP != prTargetStaRec &&
		    prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			qmMoveStaTxQueue(prAisBssInfo->prStaRecOfAP,
					 prTargetStaRec);
			/* Currently, firmware just drop all previous AP's
			 **  data packets, need to handle waiting tx done
			 ** status packets so driver no
			 */
#if 0
			nicTxHandleRoamingDone(prAdapter,
					       prAisBssInfo->prStaRecOfAP,
					       prTargetStaRec);
#endif
			cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP);
		} else
			DBGLOG(AIS, WARN, "prStaRecOfAP is in use %d\n",
			       prAisBssInfo->prStaRecOfAP->fgIsInUse);
	else
		DBGLOG(AIS, WARN,
		       "NULL pointer of prAisBssInfo->prStaRecOfAP\n");
}				/* end of aisFsmRoamingDisconnectPrevAP() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will update the contain of BSS_INFO_T for AIS
 *         network once the roaming was completed.
 *
 * @param IN prAdapter          Pointer to the Adapter structure.
 *           prStaRec           StaRec of roaming AP
 *           prAssocRspSwRfb
 *
 * @retval None
 */
/*----------------------------------------------------------------------------*/
void aisUpdateBssInfoForRoamingAP(IN struct ADAPTER *prAdapter,
				  IN struct STA_RECORD *prStaRec,
				  IN struct SW_RFB *prAssocRspSwRfb)
{
	struct BSS_INFO *prAisBssInfo;

	DBGLOG(AIS, LOUD, "aisUpdateBssInfoForRoamingAP()");

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1.1> Change FW's Media State immediately. */
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

	/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
	if ((prAisBssInfo->prStaRecOfAP) &&
	    (prAisBssInfo->prStaRecOfAP != prStaRec)
	    && (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {
		/* before deactivate previous AP, should move its pending MSDUs
		 ** to the new AP
		 */
		qmMoveStaTxQueue(prAisBssInfo->prStaRecOfAP, prStaRec);
		/* cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP,
		 ** STA_STATE_1);
		 */
		cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP);
	}

	/* 4 <1.4> Update BSS_INFO_T */
	aisUpdateBssInfoForJOIN(prAdapter, prStaRec, prAssocRspSwRfb);

	/* 4 <1.3> Activate current AP's STA_RECORD_T in Driver. */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

	/* 4 <1.6> Indicate Connected Event to Host immediately. */
	/* Require BSSID, Association ID, Beacon Interval..
	 * from AIS_BSS_INFO_T
	 */
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED,
					FALSE);
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
u_int8_t aisFsmIsRequestPending(IN struct ADAPTER *prAdapter,
				IN enum ENUM_AIS_REQUEST_TYPE eReqType,
				IN u_int8_t bRemove)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct AIS_REQ_HDR *prPendingReqHdr, *prPendingReqHdrNext;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* traverse through pending request list */
	LINK_FOR_EACH_ENTRY_SAFE(prPendingReqHdr,
				 prPendingReqHdrNext,
				 &(prAisFsmInfo->rPendingReqList), rLinkEntry,
				 struct AIS_REQ_HDR) {
		/* check for specified type */
		if (prPendingReqHdr->eReqType == eReqType) {
			/* check if need to remove */
			if (bRemove == TRUE) {
				LINK_REMOVE_KNOWN_ENTRY(&(prAisFsmInfo->
					rPendingReqList),
					&(prPendingReqHdr->rLinkEntry));

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
struct AIS_REQ_HDR *aisFsmGetNextRequest(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct AIS_REQ_HDR *prPendingReqHdr;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	LINK_REMOVE_HEAD(&(prAisFsmInfo->rPendingReqList), prPendingReqHdr,
			 struct AIS_REQ_HDR *);

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
u_int8_t aisFsmInsertRequest(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType)
{
	struct AIS_REQ_HDR *prAisReq;
	struct AIS_FSM_INFO *prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	prAisReq =
	    (struct AIS_REQ_HDR *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					      sizeof(struct AIS_REQ_HDR));

	if (!prAisReq) {
		ASSERT(0);	/* Can't generate new message */
		return FALSE;
	}
	DBGLOG(AIS, INFO, "aisFsmInsertRequest\n");

	prAisReq->eReqType = eReqType;

	/* attach request into pending request list */
	LINK_INSERT_TAIL(&prAisFsmInfo->rPendingReqList, &prAisReq->rLinkEntry);

	DBGLOG(AIS, TRACE, "eCurrentState=%d, eReqType=%d, u4NumElem=%d\n",
	       prAisFsmInfo->eCurrentState, eReqType,
	       prAisFsmInfo->rPendingReqList.u4NumElem);

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
void aisFsmFlushRequest(IN struct ADAPTER *prAdapter)
{
	struct AIS_REQ_HDR *prAisReq;

	ASSERT(prAdapter);

	while ((prAisReq = aisFsmGetNextRequest(prAdapter)) != NULL)
		cnmMemFree(prAdapter, prAisReq);
}

void aisFsmRunEventRemainOnChannel(IN struct ADAPTER *prAdapter,
				   IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_REMAIN_ON_CHANNEL *prRemainOnChannel;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prRemainOnChannel = (struct MSG_REMAIN_ON_CHANNEL *)prMsgHdr;

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

void aisFsmRunEventCancelRemainOnChannel(IN struct ADAPTER *prAdapter,
					 IN struct MSG_HDR *prMsgHdr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct MSG_CANCEL_REMAIN_ON_CHANNEL *prCancelRemainOnChannel;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	prCancelRemainOnChannel =
	    (struct MSG_CANCEL_REMAIN_ON_CHANNEL *)prMsgHdr;

	/* 1. Check the cookie first */
	if (prCancelRemainOnChannel->u8Cookie ==
	    prAisFsmInfo->rChReqInfo.u8Cookie) {

		/* 2. release channel privilege/request */
		if (prAisFsmInfo->eCurrentState ==
		    AIS_STATE_REQ_REMAIN_ON_CHANNEL) {
			/* 2.1 elease channel */
			aisFsmReleaseCh(prAdapter);
		} else if (prAisFsmInfo->eCurrentState ==
			   AIS_STATE_REMAIN_ON_CHANNEL) {
			/* 2.1 release channel */
			aisFsmReleaseCh(prAdapter);

			/* 2.2 stop channel timeout timer */
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rChannelTimeoutTimer);
		}

		/* 3. clear pending request of remain_on_channel */
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL,
				       TRUE);

		/* 4. decide which state to retreat */
		if (prAisFsmInfo->eCurrentState ==
		    AIS_STATE_REQ_REMAIN_ON_CHANNEL
		    || prAisFsmInfo->eCurrentState ==
		    AIS_STATE_REMAIN_ON_CHANNEL) {
			if (prAisBssInfo->eConnectionState ==
			    PARAM_MEDIA_STATE_CONNECTED)
				aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
			else
				aisFsmSteps(prAdapter, AIS_STATE_IDLE);
		}
	}

	/* 5. free message */
	cnmMemFree(prAdapter, prMsgHdr);
}

void aisFsmRunEventMgmtFrameTx(IN struct ADAPTER *prAdapter,
			       IN struct MSG_HDR *prMsgHdr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
	    (struct MSG_MGMT_TX_REQUEST *)NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMsgHdr != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (prAisFsmInfo == NULL)
			break;

		prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *)prMsgHdr;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo,
				   prMgmtTxMsg->prMgmtMsduInfo,
				   prMgmtTxMsg->u8Cookie);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* aisFsmRunEventMgmtFrameTx */

#if CFG_SUPPORT_NCHO
void aisFsmRunEventNchoActionFrameTx(IN struct ADAPTER *prAdapter,
				     IN struct MSG_HDR *prMsgHdr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo = (struct BSS_INFO *)NULL;
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
	    (struct MSG_MGMT_TX_REQUEST *)NULL;
	struct MSDU_INFO *prMgmtFrame = (struct MSDU_INFO *)NULL;
	struct _ACTION_VENDOR_SPEC_FRAME_T *prVendorSpec = NULL;
	uint8_t *pucFrameBuf = (uint8_t *) NULL;
	struct _NCHO_INFO_T *prNchoInfo = NULL;
	uint16_t u2PktLen = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));
		DBGLOG(REQ, TRACE, "NCHO in aisFsmRunEventNchoActionFrameTx\n");

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prNchoInfo = &(prAdapter->rNchoInfo);
		prAisBssInfo =
		    &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS]);

		if (prAisFsmInfo == NULL)
			break;

		prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *)prMsgHdr;
		u2PktLen =
		    (uint16_t) OFFSET_OF(struct _ACTION_VENDOR_SPEC_FRAME_T,
					 aucElemInfo[0]) +
		    prNchoInfo->rParamActionFrame.i4len + MAC_TX_RESERVED_FIELD;
		prMgmtFrame = cnmMgtPktAlloc(prAdapter, u2PktLen);
		if (prMgmtFrame == NULL) {
			ASSERT(FALSE);
			DBGLOG(REQ, ERROR,
			       "NCHO there is no memory for prMgmtFrame\n");
			break;
		}
		prMgmtTxMsg->prMgmtMsduInfo = prMgmtFrame;

		pucFrameBuf =
		    (uint8_t *) ((unsigned long)prMgmtFrame->prPacket +
				 MAC_TX_RESERVED_FIELD);
		prVendorSpec =
		    (struct _ACTION_VENDOR_SPEC_FRAME_T *)pucFrameBuf;
		prVendorSpec->u2FrameCtrl = MAC_FRAME_ACTION;
		prVendorSpec->u2Duration = 0;
		prVendorSpec->u2SeqCtrl = 0;
		COPY_MAC_ADDR(prVendorSpec->aucDestAddr,
			      prNchoInfo->rParamActionFrame.aucBssid);
		COPY_MAC_ADDR(prVendorSpec->aucSrcAddr,
			      prAisBssInfo->aucOwnMacAddr);
		COPY_MAC_ADDR(prVendorSpec->aucBSSID, prAisBssInfo->aucBSSID);

		kalMemCopy(prVendorSpec->aucElemInfo,
			   prNchoInfo->rParamActionFrame.aucData,
			   prNchoInfo->rParamActionFrame.i4len);

		prMgmtFrame->u2FrameLength = u2PktLen;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo,
				   prMgmtTxMsg->prMgmtMsduInfo,
				   prMgmtTxMsg->u8Cookie);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* aisFsmRunEventNchoActionFrameTx */
#endif

void aisFsmRunEventChannelTimeout(IN struct ADAPTER *prAdapter,
				  unsigned long ulParamPtr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter,
				  &prAisFsmInfo->rChannelTimeoutTimer);

		/* 3. expiration indication to upper layer */
		kalRemainOnChannelExpired(prAdapter->prGlueInfo,
					  prAisFsmInfo->rChReqInfo.u8Cookie,
					  prAisFsmInfo->rChReqInfo.eBand,
					  prAisFsmInfo->rChReqInfo.eSco,
					  prAisFsmInfo->
					  rChReqInfo.ucChannelNum);

		/* 4. decide which state to retreat */
		if (prAisBssInfo->eConnectionState ==
		    PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	} else {
		DBGLOG(AIS, WARN,
		       "Unexpected remain_on_channel timeout event\n");
		DBGLOG(AIS, STATE, "CURRENT State: [%s]\n",
		       apucDebugAisState[prAisFsmInfo->eCurrentState]);
	}
}

uint32_t
aisFsmRunEventMgmtFrameTxDone(IN struct ADAPTER *prAdapter,
			      IN struct MSDU_INFO *prMsduInfo,
			      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo =
	    (struct AIS_MGMT_TX_REQ_INFO *)NULL;
	u_int8_t fgIsSuccess = FALSE;

	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prMgmtTxReqInfo = &(prAisFsmInfo->rMgmtTxInfo);

		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(AIS, ERROR, "Mgmt Frame TX Fail, Status:%d.\n",
			       rTxDoneStatus);
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
		}

		if (prMgmtTxReqInfo->prMgmtTxMsdu == prMsduInfo) {
			kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
						prMgmtTxReqInfo->u8Cookie,
						fgIsSuccess,
						prMsduInfo->prPacket,
						(uint32_t)
						prMsduInfo->u2FrameLength);

			prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
		}

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* aisFsmRunEventMgmtFrameTxDone */

void aisFsmRunEventSetOkcPmk(IN struct ADAPTER *prAdapter)
{
	struct AIS_FSM_INFO *prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;

	prAdapter->rWifiVar.rConnSettings.fgOkcPmksaReady = TRUE;
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rWaitOkcPMKTimer);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN &&
	    prAisFsmInfo->fgIsChannelGranted)
		aisFsmSteps(prAdapter, AIS_STATE_JOIN);
}

static void aisFsmSetOkcTimeout(IN struct ADAPTER *prAdapter,
				unsigned long ulParam)
{
	struct AIS_FSM_INFO *prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;

	DBGLOG(AIS, WARN,
	       "Wait OKC PMKID timeout, current state[%d],fgIsChannelGranted=%d\n",
	       prAisFsmInfo->eCurrentState, prAisFsmInfo->fgIsChannelGranted);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN
	    && prAisFsmInfo->fgIsChannelGranted)
		aisFsmSteps(prAdapter, AIS_STATE_JOIN);
}

uint32_t
aisFuncTxMgmtFrame(IN struct ADAPTER *prAdapter,
		   IN struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo,
		   IN struct MSDU_INFO *prMgmtTxMsdu, IN uint64_t u8Cookie)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct MSDU_INFO *prTxMsduInfo = (struct MSDU_INFO *)NULL;
	struct WLAN_MAC_HEADER *prWlanHdr = (struct WLAN_MAC_HEADER *)NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *)NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMgmtTxReqInfo != NULL));

		if (prMgmtTxReqInfo->fgIsMgmtTxRequested) {

			/* 1. prMgmtTxReqInfo->prMgmtTxMsdu != NULL */
			/* Packet on driver, not done yet, drop it. */
			prTxMsduInfo = prMgmtTxReqInfo->prMgmtTxMsdu;
			if (prTxMsduInfo != NULL) {

				kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
					  prMgmtTxReqInfo->u8Cookie,
					  FALSE,
					  prTxMsduInfo->prPacket,
					  (uint32_t)
					  prTxMsduInfo->u2FrameLength);

				/* Leave it to TX Done handler. */
				/* cnmMgtPktFree(prAdapter, prTxMsduInfo); */
				prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
			}
			/* 2. prMgmtTxReqInfo->prMgmtTxMsdu == NULL */
			/* Packet transmitted, wait tx done. (cookie issue) */
		}

		ASSERT(prMgmtTxReqInfo->prMgmtTxMsdu == NULL);

		prWlanHdr =
		    (struct WLAN_MAC_HEADER *)((unsigned long)
					       prMgmtTxMsdu->prPacket +
					       MAC_TX_RESERVED_FIELD);
		prStaRec =
		    cnmGetStaRecByAddress(prAdapter,
					  prAdapter->prAisBssInfo->ucBssIndex,
					  prWlanHdr->aucAddr1);

		TX_SET_MMPDU(prAdapter,
			     prMgmtTxMsdu,
			     (prStaRec !=
			      NULL) ? (prStaRec->
				       ucBssIndex)
			     : (prAdapter->prAisBssInfo->ucBssIndex),
			     (prStaRec !=
			      NULL) ? (prStaRec->ucIndex)
			     : (STA_REC_INDEX_NOT_FOUND),
			     WLAN_MAC_MGMT_HEADER_LEN,
			     prMgmtTxMsdu->u2FrameLength,
			     aisFsmRunEventMgmtFrameTxDone,
			     MSDU_RATE_MODE_AUTO);
		prMgmtTxReqInfo->u8Cookie = u8Cookie;
		prMgmtTxReqInfo->prMgmtTxMsdu = prMgmtTxMsdu;
		prMgmtTxReqInfo->fgIsMgmtTxRequested = TRUE;

		nicTxConfigPktControlFlag(prMgmtTxMsdu,
					  MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

		/* send to TX queue */
		nicTxEnqueueMsdu(prAdapter, prMgmtTxMsdu);

	} while (FALSE);

	return rWlanStatus;
}				/* aisFuncTxMgmtFrame */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will validate the Rx Action Frame and indicate to uppoer
 *            layer if the specified conditions were matched.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 * @param[out] pu4ControlFlags   Control flags for replying the Probe Response
 *
 * @retval none
 */
/*----------------------------------------------------------------------------*/
void aisFuncValidateRxActionFrame(IN struct ADAPTER *prAdapter,
				  IN struct SW_RFB *prSwRfb)
{
	struct AIS_FSM_INFO *prAisFsmInfo = (struct AIS_FSM_INFO *)NULL;

	DEBUGFUNC("aisFuncValidateRxActionFrame");

	do {

		ASSERT((prAdapter != NULL) && (prSwRfb != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (prAisFsmInfo->u4AisPacketFilter & PARAM_PACKET_FILTER_ACTION_FRAME) {
			/* Leave the action frame to wpa_supplicant. */
			kalIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb);
		}

	} while (FALSE);

	return;

}				/* aisFuncValidateRxActionFrame */

/* Support AP Selection */
void aisRefreshFWKBlacklist(struct ADAPTER *prAdapter)
{
	struct CONNECTION_SETTINGS *prConnSettings =
	    &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct LINK *prBlackList = &prConnSettings->rBlackList.rUsingLink;

	DBGLOG(AIS, INFO,
		"Refresh all the BSSes' fgIsInFWKBlacklist to FALSE\n");
	LINK_FOR_EACH_ENTRY(prEntry, prBlackList, rLinkEntry,
			    struct AIS_BLACKLIST_ITEM) {
		prEntry->fgIsInFWKBlacklist = FALSE;
	}
}

struct AIS_BLACKLIST_ITEM *aisAddBlacklist(struct ADAPTER *prAdapter,
					   struct BSS_DESC *prBssDesc)
{
	struct CONNECTION_SETTINGS *prConnSettings =
	    &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct LINK_MGMT *prBlackList = &prConnSettings->rBlackList;

	if (!prBssDesc) {
		DBGLOG(AIS, ERROR, "bss descriptor is NULL\n");
		return NULL;
	}
	if (prBssDesc->prBlack) {
		GET_CURRENT_SYSTIME(&prBssDesc->prBlack->rAddTime);
		prBssDesc->prBlack->ucCount++;
		if (prBssDesc->prBlack->ucCount > 10)
			prBssDesc->prBlack->ucCount = 10;
		DBGLOG(AIS, INFO, "update blacklist for " MACSTR
		       ", count %d\n",
		       MAC2STR(prBssDesc->aucBSSID),
		       prBssDesc->prBlack->ucCount);
		return prBssDesc->prBlack;
	}

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);

	if (prEntry) {
		GET_CURRENT_SYSTIME(&prEntry->rAddTime);
		prBssDesc->prBlack = prEntry;
		prEntry->ucCount++;
		if (prEntry->ucCount > 10)
			prEntry->ucCount = 10;
		DBGLOG(AIS, INFO, "update blacklist for " MACSTR
		       ", count %d\n",
		       MAC2STR(prBssDesc->aucBSSID), prEntry->ucCount);
		return prEntry;
	}
	LINK_MGMT_GET_ENTRY(prBlackList, prEntry, struct AIS_BLACKLIST_ITEM,
			    VIR_MEM_TYPE);
	if (!prEntry) {
		DBGLOG(AIS, WARN, "No memory to allocate\n");
		return NULL;
	}
	prEntry->ucCount = 1;
	/* Support AP Selection */
	prEntry->fgIsInFWKBlacklist = FALSE;
	COPY_MAC_ADDR(prEntry->aucBSSID, prBssDesc->aucBSSID);
	COPY_SSID(prEntry->aucSSID, prEntry->ucSSIDLen, prBssDesc->aucSSID,
		  prBssDesc->ucSSIDLen);
	GET_CURRENT_SYSTIME(&prEntry->rAddTime);
	prBssDesc->prBlack = prEntry;

	DBGLOG(AIS, INFO, "Add " MACSTR " to black List\n",
	       MAC2STR(prBssDesc->aucBSSID));
	return prEntry;
}

void aisRemoveBlackList(struct ADAPTER *prAdapter, struct BSS_DESC *prBssDesc)
{
	struct CONNECTION_SETTINGS *prConnSettings =
	    &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);
	if (!prEntry)
		return;
	LINK_MGMT_RETURN_ENTRY(&prConnSettings->rBlackList, prEntry);
	prBssDesc->prBlack = NULL;
	DBGLOG(AIS, INFO, "Remove " MACSTR " from blacklist\n",
	       MAC2STR(prBssDesc->aucBSSID));
}

struct AIS_BLACKLIST_ITEM *aisQueryBlackList(struct ADAPTER *prAdapter,
					     struct BSS_DESC *prBssDesc)
{
	struct CONNECTION_SETTINGS *prConnSettings =
	    &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct LINK *prBlackList = &prConnSettings->rBlackList.rUsingLink;

	if (!prBssDesc)
		return NULL;
	else if (prBssDesc->prBlack)
		return prBssDesc->prBlack;

	LINK_FOR_EACH_ENTRY(prEntry, prBlackList, rLinkEntry,
			    struct AIS_BLACKLIST_ITEM) {
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry) &&
		    EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			       prEntry->aucSSID, prEntry->ucSSIDLen)) {
			prBssDesc->prBlack = prEntry;
			return prEntry;
		}
	}
	DBGLOG(AIS, TRACE, MACSTR " is not in blacklist\n",
	       MAC2STR(prBssDesc->aucBSSID));
	return NULL;
}

void aisRemoveTimeoutBlacklist(struct ADAPTER *prAdapter)
{
	struct CONNECTION_SETTINGS *prConnSettings =
	    &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	struct LINK *prBlackList = &prConnSettings->rBlackList.rUsingLink;
	OS_SYSTIME rCurrent;

	GET_CURRENT_SYSTIME(&rCurrent);

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry,
				 struct AIS_BLACKLIST_ITEM) {
		/* Support AP Selection */
		if (prEntry->fgIsInFWKBlacklist == TRUE)
			continue;
		/* end Support AP Selection */
		if (!CHECK_FOR_TIMEOUT(rCurrent, prEntry->rAddTime,
				       SEC_TO_MSEC(AIS_BLACKLIST_TIMEOUT)))
			continue;
		LINK_MGMT_RETURN_ENTRY(&prConnSettings->rBlackList, prEntry);
	}
}

static void aisRemoveDisappearedBlacklist(struct ADAPTER *prAdapter)
{
	struct CONNECTION_SETTINGS *prConnSettings =
	    &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	struct LINK *prBlackList = &prConnSettings->rBlackList.rUsingLink;
	struct LINK *prFreeList = &prConnSettings->rBlackList.rFreeLink;
	struct BSS_DESC *prBssDesc = NULL;
	struct LINK *prBSSDescList =
	    &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	uint32_t u4Current = (uint32_t)kalGetBootTime();
	u_int8_t fgDisappeared = TRUE;

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry,
				 struct AIS_BLACKLIST_ITEM) {
		fgDisappeared = TRUE;
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry,
				    struct BSS_DESC) {
			if (prBssDesc->prBlack == prEntry ||
			    (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry) &&
			     EQUAL_SSID(prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen,
					prEntry->aucSSID,
					prEntry->ucSSIDLen))) {
				fgDisappeared = FALSE;
				break;
			}
		}
		if (!fgDisappeared || (u4Current - prEntry->u4DisapperTime) <
		    600 * USEC_PER_SEC)
			continue;

		DBGLOG(AIS, INFO, "Remove disappeared blacklist %s " MACSTR,
		       prEntry->aucSSID, MAC2STR(prEntry->aucBSSID));
		LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
		LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	}
}

u_int8_t aisApOverload(struct AIS_BLACKLIST_ITEM *prBlack)
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

uint16_t aisCalculateBlackListScore(struct ADAPTER *prAdapter,
				    struct BSS_DESC *prBssDesc)
{
	if (!prBssDesc->prBlack)
		prBssDesc->prBlack = aisQueryBlackList(prAdapter, prBssDesc);

	if (!prBssDesc->prBlack)
		return 100;
	else if (aisApOverload(prBssDesc->prBlack) ||
		 prBssDesc->prBlack->ucCount >= 10)
		return 0;
	return 100 - prBssDesc->prBlack->ucCount * 10;
}

void aisFsmRunEventBssTransition(IN struct ADAPTER *prAdapter,
				 IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_AIS_BSS_TRANSITION_T *prMsg =
	    (struct MSG_AIS_BSS_TRANSITION_T *)prMsgHdr;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo =
	    &prAdapter->rWifiVar.rAisSpecificBssInfo;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
	    &prAisSpecificBssInfo->rBTMParam;
	enum WNM_AIS_BSS_TRANSITION eTransType = BSS_TRANSITION_MAX_NUM;
	struct BSS_DESC *prBssDesc =
	    prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
	u_int8_t fgNeedBtmResponse = FALSE;
	uint8_t ucStatus = BSS_TRANSITION_MGT_STATUS_UNSPECIFIED;
	uint8_t ucRcvToken = 0;
	static uint8_t aucChnlList[MAXIMUM_OPERATION_CHANNEL_LIST];

	if (!prMsg) {
		DBGLOG(AIS, WARN, "Msg Header is NULL\n");
		return;
	}
	eTransType = prMsg->eTransitionType;
	fgNeedBtmResponse = prMsg->fgNeedResponse;
	ucRcvToken = prMsg->ucToken;

	DBGLOG(AIS, INFO, "Transition Type: %d\n", eTransType);
	aisCollectNeighborAP(prAdapter, prMsg->pucCandList,
			     prMsg->u2CandListLen, prMsg->ucValidityInterval);
	cnmMemFree(prAdapter, prMsgHdr);
	/* Solicited BTM request: the case we're waiting btm request
	 ** after send btm query before roaming scan
	 */
	if (prBtmParam->ucDialogToken == ucRcvToken) {
		prBtmParam->fgPendingResponse = fgNeedBtmResponse;
		prBtmParam->fgUnsolicitedReq = FALSE;

		switch (prAdapter->rWifiVar.rRoamingInfo.eCurrentState) {
		case ROAMING_STATE_REQ_CAND_LIST:
			roamingFsmSteps(prAdapter, ROAMING_STATE_DISCOVERY);
			return;
		case ROAMING_STATE_DISCOVERY:
			/* this case need to fall through */
		case ROAMING_STATE_ROAM:
			ucStatus = BSS_TRANSITION_MGT_STATUS_UNSPECIFIED;
			goto send_response;
		default:
			/* not solicited btm request, but dialog token matches
			 ** occasionally.
			 */
			break;
		}
	}
	prBtmParam->fgUnsolicitedReq = TRUE;
	/* Unsolicited BTM request */
	switch (eTransType) {
	case BSS_TRANSITION_DISASSOC:
		ucStatus = BSS_TRANSITION_MGT_STATUS_ACCEPT;
		break;
	case BSS_TRANSITION_REQ_ROAMING:
	{
		struct NEIGHBOR_AP_T *prNeiAP = NULL;
		struct LINK *prUsingLink =
			&prAisSpecificBssInfo->rNeighborApList.rUsingLink;
		uint8_t i = 0;
		uint8_t ucChannel = 0;
		uint8_t ucChnlCnt = 0;
		uint16_t u2LeftTime = 0;

		if (!prBssDesc) {
			DBGLOG(AIS, ERROR, "Target Bss Desc is NULL\n");
		break;
		}
		prBtmParam->fgPendingResponse = fgNeedBtmResponse;
		kalMemZero(aucChnlList, sizeof(aucChnlList));
		LINK_FOR_EACH_ENTRY(prNeiAP, prUsingLink, rLinkEntry,
				    struct NEIGHBOR_AP_T)
		{
			ucChannel = prNeiAP->ucChannel;
			for (i = 0;
			     i < ucChnlCnt && ucChannel != aucChnlList[i]; i++)
				;
			if (i == ucChnlCnt)
				ucChnlCnt++;
		}
		/* reserve 1 second for association */
		u2LeftTime = prBtmParam->u2DisassocTimer *
				     prBssDesc->u2BeaconInterval -
			     1000;
		/* check if left time is enough to do partial scan, if not
		 ** enought, reject directly
		 */
		if (u2LeftTime < ucChnlCnt * prBssDesc->u2BeaconInterval) {
			ucStatus = BSS_TRANSITION_MGT_STATUS_UNSPECIFIED;
			goto send_response;
		}
		roamingFsmSteps(prAdapter, ROAMING_STATE_DISCOVERY);
		return;
	}
	default:
		ucStatus = BSS_TRANSITION_MGT_STATUS_ACCEPT;
		break;
	}
send_response:
	if (fgNeedBtmResponse && prAdapter->prAisBssInfo &&
	    prAdapter->prAisBssInfo->prStaRecOfAP) {
		prBtmParam->ucStatusCode = ucStatus;
		prBtmParam->ucTermDelay = 0;
		kalMemZero(prBtmParam->aucTargetBssid, MAC_ADDR_LEN);
		prBtmParam->u2OurNeighborBssLen = 0;
		prBtmParam->fgPendingResponse = FALSE;
		wnmSendBTMResponseFrame(prAdapter,
					prAdapter->prAisBssInfo->prStaRecOfAP);
	}
}

#if CFG_SUPPORT_802_11K
void aisSendNeighborRequest(struct ADAPTER *prAdapter)
{
	struct SUB_ELEMENT_LIST *prSSIDIE;
	uint8_t aucBuffer[sizeof(*prSSIDIE) + 31];
	struct BSS_INFO *prBssInfo = prAdapter->prAisBssInfo;

	kalMemZero(aucBuffer, sizeof(aucBuffer));
	prSSIDIE = (struct SUB_ELEMENT_LIST *)&aucBuffer[0];
	prSSIDIE->rSubIE.ucSubID = ELEM_ID_SSID;
	COPY_SSID(&prSSIDIE->rSubIE.aucOptInfo[0], prSSIDIE->rSubIE.ucLength,
		  prBssInfo->aucSSID, prBssInfo->ucSSIDLen);
	rlmTxNeighborReportRequest(prAdapter, prBssInfo->prStaRecOfAP,
				   prSSIDIE);
}

static u_int8_t aisCandPrefIEIsExist(uint8_t *pucSubIe, uint8_t ucLength)
{
	uint16_t u2Offset = 0;

	IE_FOR_EACH(pucSubIe, ucLength, u2Offset) {
		if (IE_ID(pucSubIe) == ELEM_ID_NR_BSS_TRANSITION_CAND_PREF)
			return TRUE;
	}
	return FALSE;
}

static uint8_t aisGetNeighborApPreference(uint8_t *pucSubIe, uint8_t ucLength)
{
	uint16_t u2Offset = 0;

	IE_FOR_EACH(pucSubIe, ucLength, u2Offset) {
		if (IE_ID(pucSubIe) == ELEM_ID_NR_BSS_TRANSITION_CAND_PREF)
			return pucSubIe[2];
	}
	/* If no preference element is presence, give default value(lowest) 0,
	 */
	/* but it will not be used as a reference. */
	return 0;
}

static uint64_t aisGetBssTermTsf(uint8_t *pucSubIe, uint8_t ucLength)
{
	uint16_t u2Offset = 0;

	IE_FOR_EACH(pucSubIe, ucLength, u2Offset) {
		if (IE_ID(pucSubIe) == ELEM_ID_NR_BSS_TERMINATION_DURATION)
			return *(uint64_t *) &pucSubIe[2];
	}
	/* If no preference element is presence, give default value(lowest) 0 */
	return 0;
}

void aisCollectNeighborAP(struct ADAPTER *prAdapter, uint8_t *pucApBuf,
			  uint16_t u2ApBufLen, uint8_t ucValidInterval)
{
	struct NEIGHBOR_AP_T *prNeighborAP = NULL;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo =
	    &prAdapter->rWifiVar.rAisSpecificBssInfo;
	struct LINK_MGMT *prAPlist = &prAisSpecBssInfo->rNeighborApList;
	struct IE_NEIGHBOR_REPORT *prIe = (struct IE_NEIGHBOR_REPORT *)pucApBuf;
	uint16_t u2BufLen;
	uint16_t u2PrefIsZeroCount = 0;

	if (!prIe || !u2ApBufLen || u2ApBufLen < prIe->ucLength)
		return;
	LINK_MERGE_TO_TAIL(&prAPlist->rFreeLink, &prAPlist->rUsingLink);
	for (u2BufLen = u2ApBufLen; u2BufLen > 0; u2BufLen -= IE_SIZE(prIe),
	     prIe = (struct IE_NEIGHBOR_REPORT *)((uint8_t *) prIe +
						  IE_SIZE(prIe))) {
		/* BIT0-1: AP reachable, BIT2: same security with current
		 ** setting,
		 ** BIT3: same authenticator with current AP
		 */
		if (prIe->ucId != ELEM_ID_NEIGHBOR_REPORT ||
		    (prIe->u4BSSIDInfo & 0x7) != 0x7)
			continue;
		LINK_MGMT_GET_ENTRY(prAPlist, prNeighborAP,
				    struct NEIGHBOR_AP_T, VIR_MEM_TYPE);
		if (!prNeighborAP)
			break;
		prNeighborAP->fgHT = !!(prIe->u4BSSIDInfo & BIT(11));
		prNeighborAP->fgFromBtm = !!ucValidInterval;
		prNeighborAP->fgRmEnabled = !!(prIe->u4BSSIDInfo & BIT(7));
		prNeighborAP->fgQoS = !!(prIe->u4BSSIDInfo & BIT(5));
		prNeighborAP->fgSameMD = !!(prIe->u4BSSIDInfo & BIT(10));
		prNeighborAP->ucChannel = prIe->ucChnlNumber;
		prNeighborAP->fgPrefPresence = aisCandPrefIEIsExist(
			prIe->aucSubElem,
			IE_SIZE(prIe) - OFFSET_OF(struct IE_NEIGHBOR_REPORT,
						   aucSubElem));
		prNeighborAP->ucPreference = aisGetNeighborApPreference(
			prIe->aucSubElem,
			IE_SIZE(prIe) - OFFSET_OF(struct IE_NEIGHBOR_REPORT,
						  aucSubElem));
		prNeighborAP->u8TermTsf = aisGetBssTermTsf(
			prIe->aucSubElem,
			IE_SIZE(prIe) - OFFSET_OF(struct IE_NEIGHBOR_REPORT,
					       aucSubElem));
		COPY_MAC_ADDR(prNeighborAP->aucBssid, prIe->aucBSSID);
		DBGLOG(AIS, INFO,
		       "Bssid" MACSTR
		       ", PrefPresence %d, Pref %d, Chnl %d, BssidInfo 0x%08x\n",
		       MAC2STR(prNeighborAP->aucBssid),
		       prNeighborAP->fgPrefPresence,
		       prNeighborAP->ucPreference, prIe->ucChnlNumber,
		       prIe->u4BSSIDInfo);
		/* No need to save neighbor ap list with decendant preference
		 ** for (prTemp = LINK_ENTRY(prAPlist->rUsingLink.prNext, struct
		 ** NEIGHBOR_AP_T, rLinkEntry);
		 **     prTemp != prNeighborAP;
		 **     prTemp = LINK_ENTRY(prTemp->rLinkEntry.prNext, struct
		 ** NEIGHBOR_AP_T, rLinkEntry)) {
		 **     if (prTemp->ucPreference < prNeighborAP->ucPreference) {
		 **             __linkDel(prNeighborAP->rLinkEntry.prPrev,
		 ** prNeighborAP->rLinkEntry.prNext);
		 **             __linkAdd(&prNeighborAP->rLinkEntry,
		 ** prTemp->rLinkEntry.prPrev, &prTemp->rLinkEntry);
		 **             break;
		 **     }
		 ** }
		 */
		if (prNeighborAP->fgPrefPresence &&
		    prNeighborAP->ucPreference == 0)
			u2PrefIsZeroCount++;
	}
	prAisSpecBssInfo->rNeiApRcvTime = kalGetTimeTick();
	prAisSpecBssInfo->u4NeiApValidInterval =
	    !ucValidInterval
	    ? 0xffffffff
	    : TU_TO_MSEC(ucValidInterval *
			 prAdapter->prAisBssInfo->u2BeaconInterval);

	if (prAPlist->rUsingLink.u4NumElem > 0 &&
	    prAPlist->rUsingLink.u4NumElem == u2PrefIsZeroCount)
		DBGLOG(AIS, INFO,
		       "The number of valid neighbors is equal to the number of perf value is 0.\n");
}

void aisResetNeighborApList(IN struct ADAPTER *prAdapter)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo =
	    &prAdapter->rWifiVar.rAisSpecificBssInfo;
	struct LINK_MGMT *prAPlist = &prAisSpecBssInfo->rNeighborApList;

	LINK_MERGE_TO_TAIL(&prAPlist->rFreeLink, &prAPlist->rUsingLink);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief Trigger when cfg80211_suspend
*		 1. cancel scan and report scan done event
*		 2. linkdown if wow is disable
*
* @param prAdapter
*        eReqType
*        bRemove
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
void aisPreSuspendFlow(IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_CANCEL_REMAIN_ON_CHANNEL *prMsgChnlAbort;

	GLUE_SPIN_LOCK_DECLARATION();

	/* report scan abort */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prGlueInfo->prScanRequest) {
		kalCfg80211ScanDone(prGlueInfo->prScanRequest, TRUE);
		prGlueInfo->prScanRequest = NULL;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	/* cancel scan */
	aisFsmStateAbort_SCAN(prGlueInfo->prAdapter);

	DBGLOG(REQ, STATE, "Wow:%d, WowEnable:%d, state:%d\n",
		prGlueInfo->prAdapter->rWifiVar.ucWow,
		prGlueInfo->prAdapter->rWowCtrl.fgWowEnable,
		kalGetMediaStateIndicated(prGlueInfo));

	prAisFsmInfo = &(prGlueInfo->prAdapter->rWifiVar.rAisFsmInfo);
	if ((prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) ||
	    (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL)) {
		prMsgChnlAbort =
			cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
				sizeof(struct MSG_CANCEL_REMAIN_ON_CHANNEL));

		if (prMsgChnlAbort == NULL)
			DBGLOG(REQ, ERROR, "ChnlAbort Msg allocate fail!\n");
		else {
			prMsgChnlAbort->rMsgHdr.eMsgId =
				MID_MNY_AIS_CANCEL_REMAIN_ON_CHANNEL;
			prMsgChnlAbort->u8Cookie =
				prAisFsmInfo->rChReqInfo.u8Cookie;

			mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
				(struct MSG_HDR *) prMsgChnlAbort,
				MSG_SEND_METHOD_BUF);
		}
	}

	/* 1) wifi cfg "Wow" must be true,
	 * 2) wow is disable
	 * 3) AdvPws is disable
	 * 4) WIfI connected => execute link down flow
	 */
	/* link down AIS */
	if (prGlueInfo->prAdapter->rWifiVar.ucWow &&
		!prGlueInfo->prAdapter->rWowCtrl.fgWowEnable &&
		!prGlueInfo->prAdapter->rWifiVar.ucAdvPws) {
		if (kalGetMediaStateIndicated(prGlueInfo) ==
			PARAM_MEDIA_STATE_CONNECTED) {
			DBGLOG(REQ, STATE, "CFG80211 suspend link down\n");
			rStatus = kalIoctl(prGlueInfo, wlanoidLinkDown, NULL, 0,
				TRUE, FALSE, FALSE, &u4BufLen);
		}
	}
}

void aisFuncUpdateMgmtFrameRegister(
	IN struct ADAPTER *prAdapter,
	IN uint32_t u4NewPacketFilter)
{
	struct AIS_FSM_INFO *prAisFsmInfo = (struct AIS_FSM_INFO *)NULL;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

#if CFG_SUPPORT_PER_BSS_FILTER
	if (prAisFsmInfo->u4AisPacketFilter^u4NewPacketFilter) {
		/* Filter setings changed. */
		struct CMD_RX_PACKET_FILTER rSetRxPacketFilter;
		uint32_t u4OsFilter = 0;

		prAisFsmInfo->u4AisPacketFilter = u4NewPacketFilter;

		kalMemZero(&rSetRxPacketFilter, sizeof(rSetRxPacketFilter));

		/* For not impact original functionality. */
		rSetRxPacketFilter.u4RxPacketFilter =
					prAdapter->u4OsPacketFilter;

		if (prAdapter->prAisBssInfo) {
			rSetRxPacketFilter.ucIsPerBssFilter = TRUE;
			rSetRxPacketFilter.ucBssIndex =
					prAdapter->prAisBssInfo->ucBssIndex;
			rSetRxPacketFilter.u4BssMgmtFilter =
					prAisFsmInfo->u4AisPacketFilter;

			wlanSendSetQueryCmd(prAdapter,
					CMD_ID_SET_RX_FILTER,
					TRUE,
					FALSE,
					FALSE,
					nicCmdEventSetCommon,
					nicOidCmdTimeoutCommon,
					sizeof(struct CMD_RX_PACKET_FILTER),
					(uint8_t *)&rSetRxPacketFilter,
					&u4OsFilter,
					sizeof(u4OsFilter));
		}
	}
#else

	prAisFsmInfo->u4AisPacketFilter = u4NewPacketFilter;

#endif
}



