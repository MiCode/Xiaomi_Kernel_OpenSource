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
#if CFG_MTK_MCIF_WIFI_SUPPORT
#include "mddp.h"
#endif
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define AIS_ROAMING_CONNECTION_TRIAL_LIMIT  2
#define AIS_JOIN_TIMEOUT                    7

#if (CFG_SUPPORT_HE_ER == 1)
#define AP_TX_POWER		  20
#endif

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
	(uint8_t *) DISP_STRING("REMAIN_ON_CHANNEL"),
	(uint8_t *) DISP_STRING("OFF_CHNL_TX")
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
static void aisRemoveDeauthBlacklist(struct ADAPTER *prAdapter);

static void aisFunClearAllTxReq(IN struct ADAPTER *prAdapter,
		IN struct AIS_MGMT_TX_REQ_INFO *prAisMgmtTxInfo);
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static void aisResetBssTranstionMgtParam(struct AIS_SPECIFIC_BSS_INFO
					 *prSpecificBssInfo)
{
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam =
	    &prSpecificBssInfo->rBTMParam;

	if (prBtmParam->u2OurNeighborBssLen > 0) {
		kalMemFree(prBtmParam->pucOurNeighborBss, VIR_MEM_TYPE,
			   prBtmParam->u2OurNeighborBssLen);
		prBtmParam->u2OurNeighborBssLen = 0;
	}
	kalMemZero(prBtmParam, sizeof(*prBtmParam));
#endif
}

#if (CFG_SUPPORT_HE_ER == 1)
uint8_t aisCheckPowerMatchERCondition(IN struct ADAPTER *prAdapter,
	IN struct BSS_DESC *prBssDesc)
{
	int8_t txpwr = 0;
	int8_t icBeaconRSSI;

	icBeaconRSSI = RCPI_TO_dBm(prBssDesc->ucRCPI);
	wlanGetMiniTxPower(prAdapter, prBssDesc->eBand, PHY_MODE_OFDM, &txpwr);

	DBGLOG(AIS, INFO, "ER: STA Tx power:%x, AP Tx power:%x, Bcon RSSI:%x\n",
		txpwr, AP_TX_POWER, icBeaconRSSI);

	return ((txpwr - (AP_TX_POWER - icBeaconRSSI)) < -95);
}

bool aisCheckUsingERRate(IN struct ADAPTER *prAdapter,
	IN struct BSS_DESC *prBssDesc)
{
	bool fgIsStaUseERRate = false;

	if ((prBssDesc->fgIsERSUDisable == 0) &&
		(prBssDesc->ucDCMMaxConRx > 0) &&
		(prBssDesc->eBand == BAND_5G) &&
		(aisCheckPowerMatchERCondition(prAdapter, prBssDesc))) {
		fgIsStaUseERRate = TRUE;
	}

	DBGLOG(AIS, INFO, "ER: ER disable:%x, max rx:%x, band:%x, use ER:%x\n",
		prBssDesc->fgIsERSUDisable, prBssDesc->ucDCMMaxConRx,
		prBssDesc->eBand, fgIsStaUseERRate);

	return fgIsStaUseERRate;
}
#endif
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
		IN struct REG_INFO *prRegInfo, IN uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t aucAnyBSSID[] = BC_BSSID;
	uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	int i = 0;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* Setup default values for operation */
	COPY_MAC_ADDR(prConnSettings->aucMacAddress, aucZeroMacAddr);

	prConnSettings->ucDelayTimeOfDisconnectEvent =
	    AIS_DELAY_TIME_OF_DISCONNECT_SEC;

	COPY_MAC_ADDR(prConnSettings->aucBSSID, aucAnyBSSID);
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	prConnSettings->ucSSIDLen = 0;

	prConnSettings->eOPMode = NET_TYPE_INFRA;

	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	if (prRegInfo) {
		prConnSettings->ucAdHocChannelNum = 0;
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

	secInit(prAdapter, ucBssIndex);

	/* Features */
	prConnSettings->fgIsEnableRoaming = FALSE;

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	prConnSettings->fgSecModeChangeStartTimer = FALSE;
#endif

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
	prAdapter->rWifiVar.eDesiredPhyConfig
		= PHY_CONFIG_802_11ABGNAC;
#else
	prAdapter->rWifiVar.eDesiredPhyConfig
		= PHY_CONFIG_802_11ABGN;
#endif

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		prAdapter->rWifiVar.eDesiredPhyConfig
			= PHY_CONFIG_802_11ABGNACAX;
	}
#endif

	/* Set default bandwidth modes */
	prAdapter->rWifiVar.uc2G4BandwidthMode =
		(prAdapter->rWifiVar.ucSta2gBandwidth == MAX_BW_40MHZ)
		? CONFIG_BW_20_40M
		: CONFIG_BW_20M;
	prAdapter->rWifiVar.uc5GBandwidthMode = CONFIG_BW_20_40M;

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
void aisFsmInit(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo =
			(struct AIS_MGMT_TX_REQ_INFO *) NULL;
	uint8_t i;

	DEBUGFUNC("aisFsmInit()");
	DBGLOG(SW1, INFO, "->aisFsmInit(%d)\n", ucBssIndex);

	/* avoid that the prAisBssInfo is realloc */
	if (prAdapter->prAisBssInfo[ucBssIndex] != NULL) {
		DBGLOG(SW1, INFO, "-> realloc(%d)\n", ucBssIndex);
		return;
	}

	prAdapter->prAisBssInfo[ucBssIndex] = prAisBssInfo =
	    cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_AIS, FALSE);
	if (!prAisBssInfo) {
		DBGLOG(AIS, ERROR,
			"aisFsmInit failed because prAisBssInfo is NULL, return.\n");
		return;
	}

	/* update MAC address */
	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr,
		      prAdapter->rMyMacAddr);

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* 4 <1> Initiate FSM */
	prAisFsmInfo->ePreviousState = AIS_STATE_IDLE;
	prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;

	prAisFsmInfo->ucAvailableAuthTypes = 0;

	prAisFsmInfo->prTargetBssDesc = (struct BSS_DESC *)NULL;

	prAisFsmInfo->ucSeqNumOfReqMsg = 0;
	prAisFsmInfo->ucSeqNumOfChReq = 0;
	prAisFsmInfo->ucSeqNumOfScanReq = 0;
	prAisFsmInfo->u2SeqNumOfScanReport = AIS_SCN_REPORT_SEQ_NOT_SET;
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
			  (unsigned long)ucBssIndex);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rIbssAloneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)
			  aisFsmRunEventIbssAloneTimeOut,
			  (unsigned long)ucBssIndex);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rScanDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventScanDoneTimeOut,
			  (unsigned long)ucBssIndex);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rJoinTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventJoinTimeout,
			  (unsigned long)ucBssIndex);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rChannelTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventChannelTimeout,
			  (unsigned long)ucBssIndex);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rDeauthDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventDeauthTimeout,
			  (unsigned long)ucBssIndex);

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rSecModeChangeTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)
			  aisFsmRunEventSecModeChangeTimeout,
			  (unsigned long)ucBssIndex);
#endif

	prMgmtTxReqInfo = &prAisFsmInfo->rMgmtTxInfo;
	LINK_INITIALIZE(&prMgmtTxReqInfo->rTxReqLink);

	/* 4 <1.2> Initiate PWR STATE */
	SET_NET_PWR_STATE_IDLE(prAdapter, prAisBssInfo->ucBssIndex);

	/* 4 <2> Initiate BSS_INFO_T - common part */
	BSS_INFO_INIT(prAdapter, prAisBssInfo);
	if (ucBssIndex == AIS_DEFAULT_INDEX)
		COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr,
			prAdapter->rWifiVar.aucMacAddress);
	else
		COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr,
			prAdapter->rWifiVar.aucMacAddress1);

	/* 4 <3> Initiate BSS_INFO_T - private part */
	/* TODO */
	prAisBssInfo->eBand = BAND_2G4;
	prAisBssInfo->ucPrimaryChannel = 1;
	prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *) NULL;
	prAisBssInfo->ucOpRxNss = prAisBssInfo->ucOpTxNss =
		wlanGetSupportNss(prAdapter, prAisBssInfo->ucBssIndex);
#if (CFG_HW_WMM_BY_BSS == 0)
	prAisBssInfo->ucWmmQueSet =
		(prAdapter->rWifiVar.eDbdcMode == ENUM_DBDC_MODE_DISABLED) ?
			DBDC_5G_WMM_INDEX : DBDC_2G_WMM_INDEX;
#endif
	/* 4 <4> Allocate MSDU_INFO_T for Beacon */
	prAisBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
		OFFSET_OF(struct WLAN_BEACON_FRAME,
		aucInfoElem[0]) + MAX_IE_LENGTH);

	if (prAisBssInfo->prBeacon) {
		prAisBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
		/* NULL STA_REC */
		prAisBssInfo->prBeacon->ucStaRecIndex = 0xFF;
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
	LINK_MGMT_INIT(&prAdapter->rWifiVar.rBlackList);
	kalMemZero(&prAisSpecificBssInfo->arCurEssChnlInfo[0],
		   sizeof(prAisSpecificBssInfo->arCurEssChnlInfo));
	LINK_INITIALIZE(&prAisSpecificBssInfo->rCurEssLink);
	/* end Support AP Selection */
	LINK_INITIALIZE(&prAisBssInfo->rPmkidCache);
	/* 11K, 11V */
	LINK_MGMT_INIT(&prAisSpecificBssInfo->rNeighborApList);
	kalMemZero(&prAisSpecificBssInfo->rBTMParam,
		   sizeof(prAisSpecificBssInfo->rBTMParam));

	rrmParamInit(prAdapter, ucBssIndex);
#if CFG_SUPPORT_802_11W
	init_completion(&prAisBssInfo->rDeauthComp);
	prAisBssInfo->encryptedDeauthIsInProcess = FALSE;
#endif
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
void aisFsmUninit(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	u_int8_t fgHalted = kalIsHalted();
	GLUE_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("aisFsmUninit()");
	DBGLOG(SW1, INFO, "->aisFsmUninit(%d)\n", ucBssIndex);

	/* avoid that the prAisBssInfo is double freed */
	if (aisGetAisBssInfo(prAdapter, ucBssIndex) == NULL)
		return;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* 4 <1> Stop all timers */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
	if (timerPendingTimer(&(prAisFsmInfo->rScanDoneTimer))) {
		/* call aisFsmRunEventScanDoneTimeOut()
		 * to reset scan fsm
		 */
		if (!fgHalted) {
			aisFsmRunEventScanDoneTimeOut(prAdapter,
				(unsigned long)ucBssIndex);
			if (prAdapter->prGlueInfo->prScanRequest != NULL) {
				GLUE_ACQUIRE_SPIN_LOCK(prAdapter->prGlueInfo,
							SPIN_LOCK_NET_DEV);
				kalCfg80211ScanDone(prAdapter->prGlueInfo
					->prScanRequest, TRUE);
				prAdapter->prGlueInfo->prScanRequest = NULL;
				prAisFsmInfo->u2SeqNumOfScanReport =
						AIS_SCN_REPORT_SEQ_NOT_SET;
				GLUE_RELEASE_SPIN_LOCK(prAdapter->prGlueInfo,
							SPIN_LOCK_NET_DEV);
			}
		}
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
	}
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rSecModeChangeTimer);
#endif
	/* 4 <2> flush pending request */
	aisFsmFlushRequest(prAdapter, ucBssIndex);
	aisResetBssTranstionMgtParam(prAisSpecificBssInfo);

	aisFunClearAllTxReq(prAdapter, &(prAisFsmInfo->rMgmtTxInfo));

	/* 4 <3> Reset driver-domain BSS-INFO */
	if (prAisBssInfo) {
		/* Deactivate BSS. */
		UNSET_NET_ACTIVE(prAdapter,
			prAisBssInfo->ucBssIndex);
		if (!fgHalted)
			nicDeactivateNetwork(prAdapter,
				prAisBssInfo->ucBssIndex);

		if (prAisBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prAisBssInfo->prBeacon);
			prAisBssInfo->prBeacon = NULL;
		}

		cnmFreeBssInfo(prAdapter, prAisBssInfo);
		prAdapter->prAisBssInfo[ucBssIndex] = NULL;
	}
#if CFG_SUPPORT_802_11W
	rsnStopSaQuery(prAdapter, ucBssIndex);
#endif
	/* Support AP Selection */
	LINK_MGMT_UNINIT(&prAdapter->rWifiVar.rBlackList,
			 struct AIS_BLACKLIST_ITEM, VIR_MEM_TYPE);
	/* end Support AP Selection */
	LINK_MGMT_UNINIT(&prAisSpecificBssInfo->rNeighborApList,
			 struct NEIGHBOR_AP_T, VIR_MEM_TYPE);

	/* make sure pmkid cached is empty after uninit*/
	rsnFlushPmkid(prAdapter, ucBssIndex);

	rrmParamInit(prAdapter, ucBssIndex);
} /* end of aisFsmUninit() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief Check if ais is processing beacon timeout
 *
 * @return true if processing
 */
/*----------------------------------------------------------------------------*/
bool aisFsmIsInProcessPostpone(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *fsm = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	struct BSS_INFO *bss = aisGetAisBssInfo(prAdapter, ucBssIndex);
	struct CONNECTION_SETTINGS *set =
		aisGetConnSettings(prAdapter, ucBssIndex);

	return bss->eConnectionState == MEDIA_STATE_DISCONNECTED &&
	    fsm->u4PostponeIndStartTime > 0 &&
	    !CHECK_FOR_TIMEOUT(kalGetTimeTick(), fsm->u4PostponeIndStartTime,
	    SEC_TO_MSEC(set->ucDelayTimeOfDisconnectEvent));
}

bool aisFsmIsBeaconTimeout(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	return aisFsmIsInProcessPostpone(prAdapter, ucBssIndex) &&
		(prAisBssInfo->ucReasonOfDisconnect ==
			 DISCONNECT_REASON_CODE_RADIO_LOST ||
		 prAisBssInfo->ucReasonOfDisconnect ==
			 DISCONNECT_REASON_CODE_RADIO_LOST_TX_ERR);
}

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
		struct BSS_DESC *prBssDesc, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct STA_RECORD *prStaRec;
	struct MSG_SAA_FSM_START *prJoinReqMsg;
	struct GL_WPA_INFO *prWpaInfo;
#if (CFG_SUPPORT_HE_ER == 1)
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#endif
	struct AIS_BLACKLIST_ITEM *prBlackList;

	DEBUGFUNC("aisFsmStateInit_JOIN()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prWpaInfo = aisGetWpaInfo(prAdapter,
		ucBssIndex);

	/* 4 <1> We are going to connect to this BSS. */
	prBssDesc->fgIsConnecting = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_LEGACY_AP,
					      prAisBssInfo->ucBssIndex,
					      prBssDesc);

	if (!prStaRec) {
		DBGLOG(AIS, ERROR,
			"aisFsmStateInit_JOIN failed because prStaRec is NULL, return.\n");
		return;
	}

	prAisFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <2.1> sync. to firmware domain */
	if (prStaRec->ucStaState == STA_STATE_1)
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* 4 <3> Update ucAvailableAuthTypes which we can choice during SAA */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_DISCONNECTED
		/* Not in case of beacon timeout*/
		&& !aisFsmIsBeaconTimeout(prAdapter, ucBssIndex)) {

		prStaRec->fgIsReAssoc = FALSE;

		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_OPEN:
			if (prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0]
					== WLAN_AKM_SUITE_SAE) {
				prAisFsmInfo->ucAvailableAuthTypes =
				(uint8_t) (AUTH_TYPE_OPEN_SYSTEM |
					AUTH_TYPE_SAE);
				DBGLOG(AIS, INFO,
					"JOIN INIT: eAuthMode == OPEN | SAE\n");
			} else {
				prAisFsmInfo->ucAvailableAuthTypes =
				(uint8_t) AUTH_TYPE_OPEN_SYSTEM;
				DBGLOG(AIS, INFO,
					"JOIN INIT: eAuthMode == OPEN\n");
			}
			break;
		case AUTH_MODE_WPA2_FT:
		case AUTH_MODE_WPA2_FT_PSK:
		case AUTH_MODE_WPA:
		case AUTH_MODE_WPA_PSK:
		case AUTH_MODE_WPA2:
		case AUTH_MODE_WPA2_PSK:
		case AUTH_MODE_WPA_OSEN:
		case AUTH_MODE_WPA3_OWE:
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

		case AUTH_MODE_WPA3_SAE:
			DBGLOG(AIS, LOUD,
			       "JOIN INIT: eAuthMode == AUTH_MODE_SAE\n");
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_SAE;
			break;

		default:
			DBGLOG(AIS, ERROR,
			       "JOIN INIT: Auth Algorithm : %d was not supported by JOIN\n",
			       prConnSettings->eAuthMode);
			/* TODO(Kevin): error handling ? */
			return;
		}

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

#if (CFG_SUPPORT_HE_ER == 1)
		prStaRec->fgIsExtendedRange = FALSE;

		if (IS_FEATURE_ENABLED(prWifiVar->u4ExtendedRange)) {
			/* check using the ER rate or not */
			prStaRec->fgIsExtendedRange =
			aisCheckUsingERRate(prAdapter, prBssDesc);
		}
#endif
	} else {
		DBGLOG(AIS, LOUD, "JOIN INIT: AUTH TYPE = %d for Roaming\n",
		       prAisSpecificBssInfo->ucRoamingAuthTypes);

		/* We do roaming while the medium is connected */
		prStaRec->fgIsReAssoc = TRUE;

		/* TODO(Kevin): We may call a sub function to
		 * acquire the Roaming Auth Type
		 */
		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_OPEN:
			if (prWpaInfo->u4WpaVersion ==
			IW_AUTH_WPA_VERSION_DISABLED
			&& prWpaInfo->u4AuthAlg ==
			IW_AUTH_ALG_FT) {
				prAisFsmInfo->ucAvailableAuthTypes =
					(uint8_t) AUTH_TYPE_FAST_BSS_TRANSITION;
				DBGLOG(AIS, INFO, "FT: Non-RSN FT roaming\n");
			} else {
				prAisFsmInfo->ucAvailableAuthTypes =
					prAisSpecificBssInfo->
					ucRoamingAuthTypes;
			}
			break;
		case AUTH_MODE_WPA2_FT:
		case AUTH_MODE_WPA2_FT_PSK:
			prAisFsmInfo->ucAvailableAuthTypes =
			    (uint8_t) AUTH_TYPE_FAST_BSS_TRANSITION;
			DBGLOG(AIS, TRACE, "FT: RSN FT roaming\n");
			break;
		case AUTH_MODE_WPA3_SAE:
				prBlackList =
					aisQueryBlackList(prAdapter, prBssDesc);
			if (rsnSearchPmkidEntry(prAdapter, prBssDesc->aucBSSID,
						ucBssIndex)
				&&
				(!prBlackList || prBlackList->u2AuthStatus
					!= STATUS_INVALID_PMKID)) {
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
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
		   AUTH_TYPE_SAE) {
		DBGLOG(AIS, LOUD,
		       "JOIN INIT: Try to do Authentication with AuthType == SAE.\n");

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_SAE;
	} else {
		DBGLOG(AIS, ERROR,
		       "JOIN INIT: Unsupported auth type %d\n",
		       prAisFsmInfo->ucAvailableAuthTypes);
		return;
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
		DBGLOG(AIS, ERROR, "Can't trigger SAA FSM\n");
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
	struct STA_RECORD *prStaRec, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_SAA_FSM_START *prJoinReqMsg;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("aisFsmStateInit_RetryJOIN()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* Retry other AuthType if possible */
	if (!prAisFsmInfo->ucAvailableAuthTypes)
		return FALSE;

	if ((prStaRec->u2StatusCode !=
		STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED) &&
		(prStaRec->u2StatusCode !=
		STATUS_CODE_AUTH_TIMEOUT) &&
		(prStaRec->u2StatusCode !=
		STATUS_CODE_INVALID_INFO_ELEMENT) &&
		(prStaRec->u2StatusCode !=
		STATUS_INVALID_PMKID)) {
		prAisFsmInfo->ucAvailableAuthTypes = 0;
		return FALSE;
	}

	if (prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0]
		== WLAN_AKM_SUITE_SAE &&
		prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
		AUTH_TYPE_SAE) {
		DBGLOG(AIS, INFO,
		       "RETRY JOIN INIT: Retry Authentication with AuthType == SAE.\n");

		prAisFsmInfo->ucAvailableAuthTypes &=
		    ~(uint8_t) AUTH_TYPE_SAE;

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_SAE;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (uint8_t)
	    AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, INFO,
		       "RETRY JOIN INIT: Retry Authentication with AuthType == OPEN_SYSTEM.\n");

		prAisFsmInfo->ucAvailableAuthTypes &=
		    ~(uint8_t) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum =
		    (uint8_t) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else {
		DBGLOG(AIS, ERROR,
		       "RETRY JOIN INIT: Retry Authentication with Unexpected AuthType: %d.\n",
		       prAisFsmInfo->ucAvailableAuthTypes);
		return FALSE;
	}

	/* No more available Auth Types */
	prAisFsmInfo->ucAvailableAuthTypes = 0;

	/* Trigger SAA to start JOIN process. */
	prJoinReqMsg =
	    (struct MSG_SAA_FSM_START *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						    sizeof(struct
							   MSG_SAA_FSM_START));
	if (!prJoinReqMsg) {
		DBGLOG(AIS, ERROR, "Can't trigger SAA FSM\n");
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
void aisFsmStateInit_IBSS_ALONE(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	/* 4 <1> Check if IBSS was created before ? */
	if (prAisBssInfo->fgIsBeaconActivated) {

	/* 4 <2> Start IBSS Alone Timer for periodic SCAN and then SEARCH */
#if !CFG_SLT_SUPPORT
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer,
				   SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));
#endif
	}

	aisFsmCreateIBSS(prAdapter, ucBssIndex);
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
	struct BSS_DESC *prBssDesc, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *)NULL;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	/* 4 <1> We will merge with to this BSS immediately. */
	prBssDesc->fgIsConnecting = FALSE;
	prBssDesc->fgIsConnected = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_ADHOC_PEER,
					      prAisBssInfo->ucBssIndex,
					      prBssDesc);

	if (!prStaRec) {
		DBGLOG(AIS, ERROR,
			"aisFsmStateInit_IBSS_MERGE failed because prStaRec is NULL, return.\n");
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
void aisFsmStateAbort_JOIN(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_SAA_FSM_ABORT *prJoinAbortMsg;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	/* 1. Abort JOIN process */
	prJoinAbortMsg =
	    (struct MSG_SAA_FSM_ABORT *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						    sizeof(struct
							   MSG_SAA_FSM_ABORT));
	if (!prJoinAbortMsg) {
		DBGLOG(AIS, ERROR, "Can't abort SAA FSM\n");
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
	aisFsmReleaseCh(prAdapter, ucBssIndex);

	/* 3.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
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
void aisFsmStateAbort_SCAN(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_SCN_SCAN_CANCEL *prScanCancelMsg;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	DBGLOG(AIS, STATE, "[%d] aisFsmStateAbort_SCAN\n",
		ucBssIndex);

	/* Abort JOIN process. */
	prScanCancelMsg =
	    (struct MSG_SCN_SCAN_CANCEL *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct MSG_SCN_SCAN_CANCEL));
	if (!prScanCancelMsg) {
		DBGLOG(AIS, ERROR, "Can't abort SCN FSM\n");
		return;
	}
	kalMemZero(prScanCancelMsg, sizeof(struct MSG_SCN_SCAN_CANCEL));
	prScanCancelMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_CANCEL;
	prScanCancelMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfScanReq;
	prScanCancelMsg->ucBssIndex = ucBssIndex;
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
void aisFsmStateAbort_NORMAL_TR(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	/* TODO(Kevin): Do abort other MGMT func */

	/* 1. Release channel to CNM */
	aisFsmReleaseCh(prAdapter, ucBssIndex);

	/* 2.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
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
void aisFsmStateAbort_IBSS(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_DESC *prBssDesc;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

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
	aisFsmReleaseCh(prAdapter, ucBssIndex);
}
#endif /* CFG_SUPPORT_ADHOC */

static u_int8_t
aisState_OFF_CHNL_TX(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct AIS_MGMT_TX_REQ_INFO *prMgmtTxInfo =
			(struct AIS_MGMT_TX_REQ_INFO *) NULL;
	struct AIS_OFF_CHNL_TX_REQ_INFO *prOffChnlTxPkt =
			(struct AIS_OFF_CHNL_TX_REQ_INFO *) NULL;
	struct AIS_FSM_INFO *prAisFsmInfo =
		aisGetAisFsmInfo(prAdapter, ucBssIndex);

	prMgmtTxInfo = &prAisFsmInfo->rMgmtTxInfo;

	if (LINK_IS_EMPTY(&(prMgmtTxInfo->rTxReqLink))) {
		cnmTimerStopTimer(prAdapter,
				&prAisFsmInfo->rChannelTimeoutTimer);
		aisFsmReleaseCh(prAdapter, ucBssIndex);
		/* Link is empty, return back to IDLE. */
		return FALSE;
	}

	prOffChnlTxPkt =
		LINK_PEEK_HEAD(&(prMgmtTxInfo->rTxReqLink),
				struct AIS_OFF_CHNL_TX_REQ_INFO,
				rLinkEntry);

	if (prOffChnlTxPkt == NULL) {
		DBGLOG(AIS, ERROR,
			"Fatal Error, Link not empty but get NULL pointer.\n");
		aisFsmReleaseCh(prAdapter, ucBssIndex);
		return FALSE;
	}

	if (timerPendingTimer(&prAisFsmInfo->rChannelTimeoutTimer)) {
		cnmTimerStopTimer(prAdapter,
				&prAisFsmInfo->rChannelTimeoutTimer);
	}

	cnmTimerStartTimer(prAdapter,
			&prAisFsmInfo->rChannelTimeoutTimer,
			prOffChnlTxPkt->u4Duration);
	aisFuncTxMgmtFrame(prAdapter,
			prMgmtTxInfo,
			prOffChnlTxPkt->prMgmtTxMsdu,
			prOffChnlTxPkt->u8Cookie,
			ucBssIndex);
	LINK_REMOVE_HEAD(&(prAisFsmInfo->rMgmtTxInfo.rTxReqLink),
			prOffChnlTxPkt,
			struct AIS_OFF_CHNL_TX_REQ_INFO *);
	cnmMemFree(prAdapter, prOffChnlTxPkt);

	return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief Remove roaming requests including search and connect
 *
 * @param[in]
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void aisFsmRemoveRoamingRequest(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	/* clear pending roaming connection request */
	aisFsmClearRequest(prAdapter, AIS_REQUEST_ROAMING_SEARCH, ucBssIndex);
	aisFsmClearRequest(prAdapter, AIS_REQUEST_ROAMING_CONNECT, ucBssIndex);
}

struct BSS_DESC *aisSearchBssDescByScore(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct ROAMING_INFO *roam = aisGetRoamingInfo(prAdapter, ucBssIndex);

	return scanSearchBssDescByScoreForAis(prAdapter,
			roam->eReason, ucBssIndex);
}

uint8_t aisNeedTargetScan(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *ais = NULL;
	struct BSS_INFO *bss = NULL;
	struct ROAMING_INFO *roam = NULL;
	uint8_t discovering = FALSE;
	uint8_t postponing = FALSE;
	uint8_t issued = FALSE;
	uint8_t trial = 0;

	ais = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	bss = aisGetAisBssInfo(prAdapter, ucBssIndex);
	roam = aisGetRoamingInfo(prAdapter, ucBssIndex);
	discovering = bss->eConnectionState == MEDIA_STATE_CONNECTED &&
		     (roam->eCurrentState == ROAMING_STATE_DISCOVERY ||
		      roam->eCurrentState == ROAMING_STATE_ROAM);
	postponing = aisFsmIsInProcessPostpone(prAdapter, ucBssIndex);
	trial = ais->ucConnTrialCount;

#if CFG_SUPPORT_NCHO
	issued = ais->fgTargetChnlScanIssued ||
		 prAdapter->rNchoInfo.u4RoamScanControl;
#else
	issued = ais->fgTargetChnlScanIssued;
#endif

	return (discovering && issued) ||
	       (postponing && trial < AIS_ROAMING_CONNECTION_TRIAL_LIMIT);
}

enum ENUM_AIS_STATE aisSearchHandleBssDesc(IN struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, IN uint8_t ucBssIndex)
{

	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* 4 <2> We are not under Roaming Condition. */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_DISCONNECTED) {
		/* 4 <2.a> If we have the matched one */
		if (prBssDesc) {
			/* 4 <A> Stored the Selected BSS security cipher. or
			 * later asoc req compose IE
			 */
			prAisBssInfo->u4RsnSelectedGroupCipher =
				prBssDesc->u4RsnSelectedGroupCipher;
			prAisBssInfo->u4RsnSelectedPairwiseCipher =
				prBssDesc->u4RsnSelectedPairwiseCipher;
			prAisBssInfo->u4RsnSelectedAKMSuite =
				prBssDesc->u4RsnSelectedAKMSuite;
			prAisBssInfo->eBand = prBssDesc->eBand;
			if (prAisBssInfo->fgIsWmmInited == FALSE)
				prAisBssInfo->ucWmmQueSet =
				   cnmWmmIndexDecision(prAdapter, prAisBssInfo);
#if CFG_SUPPORT_DBDC
			/* DBDC decsion.may change OpNss */
			cnmDbdcPreConnectionEnableDecision(
				prAdapter,
				prAisBssInfo->ucBssIndex,
				prBssDesc->eBand,
				prBssDesc->ucChannelNum,
				prAisBssInfo->ucWmmQueSet);
#endif /*CFG_SUPPORT_DBDC*/

			/* 4 <B> Do STATE transition and update current
			 * Operation Mode.
			 */
			if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {
				prAisBssInfo->eCurrentOPMode =
					OP_MODE_INFRASTRUCTURE;
				prAisFsmInfo->prTargetBssDesc = prBssDesc;
				prAisFsmInfo->ucConnTrialCount++;
				return AIS_STATE_REQ_CHANNEL_JOIN;
#if CFG_SUPPORT_ADHOC
			} else if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {
				prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
				prAisFsmInfo->prTargetBssDesc = prBssDesc;
				return AIS_STATE_IBSS_MERGE;
#endif /* CFG_SUPPORT_ADHOC */
			} else {
				return AIS_STATE_WAIT_FOR_NEXT_SCAN;
			}
		} else {
			/* 4 <2.b> If we don't have the matched one */
			if (prAisFsmInfo->rJoinReqTime != 0 &&
			    CHECK_FOR_TIMEOUT(kalGetTimeTick(),
				prAisFsmInfo->rJoinReqTime,
				SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
				return AIS_STATE_JOIN_FAILURE;
			}

			return aisFsmStateSearchAction(prAdapter, ucBssIndex);
		}
	} else {
		/* 4 <3> We are under Roaming Condition. */
		if (prAisFsmInfo->ucConnTrialCount >
		    AIS_ROAMING_CONNECTION_TRIAL_LIMIT) {
#if CFG_SUPPORT_ROAMING
			DBGLOG(AIS, STATE, "Roaming retry :%d fail!\n",
			       prAisFsmInfo->ucConnTrialCount);
			roamingFsmRunEventFail(prAdapter,
				ROAMING_FAIL_REASON_CONNLIMIT, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

			/* reset retry count */
			prAisFsmInfo->ucConnTrialCount = 0;

			return AIS_STATE_NORMAL_TR;
		}

#define BSS_DESC_BAD_CASE \
	(!prBssDesc || (prBssDesc->fgIsConnected && \
	EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID) && \
	prConnSettings->eConnectionPolicy != CONNECT_BY_BSSID) || \
	prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE)
		/* 4 <3.a> Following cases will go back to NORMAL_TR.
		 * Precondition: not user space triggered roaming
		 *
		 * CASE I: During Roaming, APP(WZC/NDISTEST) change the
		 * connection settings. That make we can NOT match
		 * the original AP, so the prBssDesc is NULL.
		 *
		 * CASE II: The same reason as CASE I.
		 * Because APP change the eOPMode to other network type in
		 * connection setting (e.g. NET_TYPE_IBSS), so the BssDesc
		 * become the IBSS node.
		 * (For CASE I/II, before WZC/NDISTEST set the OID_SSID,
		 * it will change other parameters in connection setting first.
		 * if we do roaming at the same time, it will hit these cases.)
		 *
		 * CASE III: Normal case, we can't find other candidate to roam
		 * out, so only the current AP will be matched.
		 * however, if the connection policy is BSSID, means upper layer
		 * order driver connect to specific AP, we need still do connect
		 */
		if (prAisBssInfo->ucReasonOfDisconnect !=
			DISCONNECT_REASON_CODE_REASSOCIATION &&
		    prAisBssInfo->ucReasonOfDisconnect !=
			DISCONNECT_REASON_CODE_ROAMING && BSS_DESC_BAD_CASE) {
			if (prBssDesc) {
				DBGLOG(ROAMING, INFO,
					"fgIsConnected=%d, prBssDesc " MACSTR
					", prAisBssInfo " MACSTR "\n",
					prBssDesc->fgIsConnected,
					MAC2STR(prBssDesc->aucBSSID),
					MAC2STR(prAisBssInfo->aucBSSID));
			}
			if (prAisFsmInfo->fgTargetChnlScanIssued) {
				/* if target channel scan has issued, and no
				 * roaming target is found, need to do full scan
				 */
				DBGLOG(AIS, INFO,
				       "[Roaming] No target found, try to full scan again\n");

				prAisFsmInfo->fgTargetChnlScanIssued = FALSE;
				return AIS_STATE_LOOKING_FOR;
			}

#if CFG_SUPPORT_ROAMING
			roamingFsmRunEventFail(prAdapter,
				ROAMING_FAIL_REASON_NOCANDIDATE, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

			/* We already associated with it go back to NORMAL_TR */
			return AIS_STATE_NORMAL_TR;
		}

		/* 4 <3.b> Try to roam out for JOIN this BSS_DESC_T. */
		if (BSS_DESC_BAD_CASE)
			return aisFsmStateSearchAction(prAdapter, ucBssIndex);

		prAisFsmInfo->prTargetBssDesc = prBssDesc;
		prAisFsmInfo->ucConnTrialCount++;
		prAisFsmInfo->fgTargetChnlScanIssued = FALSE;

		return AIS_STATE_REQ_CHANNEL_JOIN;
	}
}

u_int8_t aisScanChannelFixed(struct ADAPTER *prAdapter, enum ENUM_BAND *prBand,
	uint8_t *pucPrimaryChannel, IN uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *setting;
	struct AIS_FSM_INFO *ais;

	ais = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	setting = aisGetConnSettings(prAdapter, ucBssIndex);
	if (ais->eCurrentState == AIS_STATE_LOOKING_FOR &&
	    setting->eConnectionPolicy == CONNECT_BY_BSSID &&
	    setting->u4FreqInKHz != 0) {
		*pucPrimaryChannel =
			nicFreq2ChannelNum(setting->u4FreqInKHz * 1000);
		if (*pucPrimaryChannel > 0) {
			if (*pucPrimaryChannel <= 14)
				*prBand = BAND_2G4;
			else
				*prBand = BAND_5G;
			DBGLOG(AIS, INFO, "fixed channel %d, band %d\n",
				*pucPrimaryChannel, *prBand);
			return TRUE;
		}
	} else {
		return cnmAisInfraChannelFixed(prAdapter,
				prBand, pucPrimaryChannel);
	}
	return FALSE;
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
void aisFsmSteps(IN struct ADAPTER *prAdapter,
	enum ENUM_AIS_STATE eNextState, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_DESC *prBssDesc;
	struct MSG_CH_REQ *prMsgChReq;
	struct MSG_SCN_SCAN_REQ_V2 *prScanReqMsg;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;
	struct AIS_REQ_HDR *prAisReq;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	enum ENUM_BAND eBand = BAND_2G4;
	uint8_t ucChannel = 1;
	uint16_t u2ScanIELen;
	u_int8_t fgIsTransition = (u_int8_t) FALSE;
	uint8_t ucRfBw;
	enum ENUM_AIS_STATE eNewState;

	DEBUGFUNC("aisFsmSteps()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prRoamingFsmInfo =
	    aisGetRoamingInfo(prAdapter, ucBssIndex);

	do {

		/* Do entering Next State */
		prAisFsmInfo->ePreviousState = prAisFsmInfo->eCurrentState;

		DBGLOG(AIS, STATE, "[AIS%d] TRANSITION: [%s] -> [%s]\n",
			ucBssIndex,
			aisGetFsmState(prAisFsmInfo->eCurrentState),
			aisGetFsmState(eNextState));

		/* NOTE(Kevin): This is the only place to change the
		 * eCurrentState(except initial)
		 */
		prAisFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (u_int8_t) FALSE;

		aisPostponedEventOfDisconnTimeout(prAdapter, ucBssIndex);

		/* Do tasks of the State that we just entered */
		switch (prAisFsmInfo->eCurrentState) {
		/* NOTE(Kevin): we don't have to rearrange the
		 * sequence of following switch case. Instead
		 * I would like to use a common lookup table of array
		 * of function pointer to speed up state search.
		 */
		case AIS_STATE_IDLE:

			prAisReq = aisFsmGetNextRequest(prAdapter, ucBssIndex);
			if (prAisFsmInfo->ePreviousState ==
					AIS_STATE_OFF_CHNL_TX)
				aisFunClearAllTxReq(prAdapter,
						&(prAisFsmInfo->rMgmtTxInfo));

			if (prAisReq)
				DBGLOG(AIS, INFO,
					"eReqType=%d", prAisReq->eReqType);
			else
				DBGLOG(AIS, INFO, "No req anymore");

			if (prAisReq == NULL ||
			    prAisReq->eReqType == AIS_REQUEST_RECONNECT) {
				if (IS_NET_ACTIVE(prAdapter,
					  prAisBssInfo->ucBssIndex)) {
					UNSET_NET_ACTIVE(prAdapter,
						prAisBssInfo->ucBssIndex);
					nicDeactivateNetwork(prAdapter,
						prAisBssInfo->ucBssIndex);
				}
				if (prAisReq != NULL) {
					SET_NET_ACTIVE(prAdapter,
						prAisBssInfo->
						ucBssIndex);
					/* sync with firmware */
					nicActivateNetwork(prAdapter,
						prAisBssInfo->ucBssIndex);

					SET_NET_PWR_STATE_ACTIVE(prAdapter,
					prAisBssInfo->ucBssIndex);

					eNextState = AIS_STATE_SEARCH;
					fgIsTransition = TRUE;
				} else {
					SET_NET_PWR_STATE_IDLE(prAdapter,
					prAisBssInfo->ucBssIndex);

					if (prAdapter->rWifiVar.
						rScanInfo.fgSchedScanning) {
						SET_NET_ACTIVE(prAdapter,
						prAisBssInfo->ucBssIndex);
						nicActivateNetwork(prAdapter,
						prAisBssInfo->ucBssIndex);
					}
				}

				if (prAisReq) {
					/* free the message */
					cnmMemFree(prAdapter, prAisReq);
				}
			} else if (prAisReq->eReqType == AIS_REQUEST_SCAN) {
				wlanClearScanningResult(prAdapter, ucBssIndex);

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
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
				prBssDesc = aisSearchBssDescByScore(
					prAdapter, ucBssIndex);
#else
				prBssDesc = scanSearchBssDescByPolicy
				    (prAdapter, prAisBssInfo->ucBssIndex);
#endif
			}
#endif

			eNewState = aisSearchHandleBssDesc(
				prAdapter, prBssDesc, ucBssIndex);

			if (eNewState != eNextState) {
				eNextState = eNewState;
				fgIsTransition = TRUE;
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
					       prAisBssInfo->ucBssIndex);

			if (prAisFsmInfo->u4SleepInterval <
			    AIS_BG_SCAN_INTERVAL_MAX_SEC)
				prAisFsmInfo->u4SleepInterval <<= 1;

			break;

		case AIS_STATE_SCAN:
		case AIS_STATE_ONLINE_SCAN:
		case AIS_STATE_LOOKING_FOR:

			if (!IS_NET_ACTIVE
			    (prAdapter, prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter,
					       prAisBssInfo->ucBssIndex);

				/* sync with firmware */
				nicActivateNetwork(prAdapter,
						   prAisBssInfo->ucBssIndex);
			}
			prScanRequest = &(prAisFsmInfo->rScanRequest);

			/* IE length decision */
			if (prScanRequest->u4IELength > 0) {
				u2ScanIELen =
				    (uint16_t) prScanRequest->u4IELength;
			} else {
#if CFG_SUPPORT_WPS2
				u2ScanIELen = prConnSettings->u2WSCIELen;
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
				DBGLOG(AIS, ERROR, "Can't trigger SCAN FSM\n");
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
			    prAisBssInfo->ucBssIndex;
#if CFG_SUPPORT_802_11K
			if (rrmFillScanMsg(prAdapter, prScanReqMsg)) {
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
				    SCAN_REQ_SSID_SPECIFIED_ONLY;
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
			/* Reduce APP scan's dwell time, prevent it affecting
			 * TX/RX performance
			 */
			if (prScanRequest->u4Flags &
				NL80211_SCAN_FLAG_LOW_SPAN) {
				prScanReqMsg->u2ChannelDwellTime =
					SCAN_CHANNEL_DWELL_TIME_MSEC_APP;
				prScanReqMsg->u2ChannelMinDwellTime =
					SCAN_CHANNEL_MIN_DWELL_TIME_MSEC_APP;
			}
			/* check if tethering is running and need to fix on
			 * specific channel
			 */
			if (aisScanChannelFixed(prAdapter, &eBand,
					&ucChannel, ucBssIndex)) {
				prScanReqMsg->eScanChannel =
				    SCAN_CHANNEL_SPECIFIED;
				prScanReqMsg->ucChannelListNum = 1;
				prScanReqMsg->arChnlInfoList[0].eBand = eBand;
				prScanReqMsg->arChnlInfoList[0].ucChannelNum =
				    ucChannel;
			} else if (aisNeedTargetScan(prAdapter,
					prAisBssInfo->ucBssIndex)) {
				struct RF_CHANNEL_INFO *prChnlInfo =
				    &prScanReqMsg->arChnlInfoList[0];
				uint8_t ucChannelNum = 0;
				uint8_t i = 0;
				uint8_t essChnlNum =
				    prAisSpecificBssInfo->ucCurEssChnlInfoNum;

				for (i = 0; i < essChnlNum; i++) {
					ucChannelNum =
					    prAisSpecificBssInfo->
					    arCurEssChnlInfo[i].ucChannel;
					if ((ucChannelNum >= 1)
					    && (ucChannelNum <= 14))
						prChnlInfo[i].eBand = BAND_2G4;
					else
						prChnlInfo[i].eBand = BAND_5G;
					prChnlInfo[i].ucChannelNum
					    = ucChannelNum;
				}
				prScanReqMsg->ucChannelListNum = essChnlNum;
				prScanReqMsg->eScanChannel =
				    SCAN_CHANNEL_SPECIFIED;
				DBGLOG(AIS, INFO,
				       "[Roaming] Target Scan: Total number of scan channel(s)=%d\n",
				       prScanReqMsg->ucChannelListNum);
			} else
			if (prAdapter->aePreferBand
				[KAL_NETWORK_TYPE_AIS_INDEX] ==
				BAND_NULL) {
				if (prAdapter->fgEnable5GBand == TRUE)
					prScanReqMsg->eScanChannel =
					    SCAN_CHANNEL_FULL;
				else
					prScanReqMsg->eScanChannel =
					    SCAN_CHANNEL_2G4;

			} else
			if (prAdapter->aePreferBand
				[KAL_NETWORK_TYPE_AIS_INDEX] ==
				BAND_2G4) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
			} else
			if (prAdapter->aePreferBand
				[KAL_NETWORK_TYPE_AIS_INDEX] ==
				BAND_5G) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
			} else {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
			}

			switch (prScanReqMsg->eScanChannel) {
			case SCAN_CHANNEL_FULL:
			case SCAN_CHANNEL_2G4:
			case SCAN_CHANNEL_5G:
				scanSetRequestChannel(prAdapter,
					prScanRequest->u4ChannelNum,
					prScanRequest->arChannel,
					prScanRequest->u4Flags,
					prAisFsmInfo->eCurrentState ==
					AIS_STATE_ONLINE_SCAN ||
					wlanWfdEnabled(prAdapter),
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
				if (prConnSettings->u2WSCIELen > 0) {
					kalMemCopy(prScanReqMsg->aucIE,
						   &prConnSettings->aucWSCIE,
						   prConnSettings->u2WSCIELen);
				}
			}
#endif
			prScanReqMsg->u2IELen = u2ScanIELen;

			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *)prScanReqMsg,
				    MSG_SEND_METHOD_BUF);

			/* reset prAisFsmInfo->rScanRequest */
			kalMemZero(prAisFsmInfo->aucScanIEBuf,
				   sizeof(prAisFsmInfo->aucScanIEBuf));
			prScanRequest->u4SsidNum = 0;
			prScanRequest->ucScanType = SCAN_TYPE_ACTIVE_SCAN;
			prScanRequest->u4IELength = 0;
			prScanRequest->u4ChannelNum = 0;
			prScanRequest->ucScnFuncMask = 0;
			kalMemZero(prScanRequest->aucRandomMac, MAC_ADDR_LEN);
			prAisFsmInfo->fgIsScanning = TRUE;

			/* Support AP Selection */
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;
			/* Scan flags will be set in next scan triggered by
			 * upper layer, reset to 0 to avoid next scan
			 * is triggered by Driver
			*/
			prScanRequest->u4Flags = 0;
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
				prAisBssInfo->
				    prStaRecOfAP->fgIsTxAllowed = FALSE;

			/* send message to CNM for acquiring channel */
			prMsgChReq =
			    (struct MSG_CH_REQ *)cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_CH_REQ));
			if (!prMsgChReq) {
				DBGLOG(AIS, ERROR, "Can't indicate CNM\n");
				return;
			}

			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex =
			    prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval =
					AIS_JOIN_CH_REQUEST_INTERVAL;
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
					     prAisFsmInfo->prTargetBssDesc,
					     ucBssIndex);
			break;

		case AIS_STATE_JOIN_FAILURE:
			nicMediaJoinFailure(prAdapter,
					    prAisBssInfo->ucBssIndex,
					    WLAN_STATUS_JOIN_FAILURE);

			prAisFsmInfo->prTargetBssDesc = NULL;
			prAisFsmInfo->ucConnTrialCountLimit = 0;
			eNextState = AIS_STATE_IDLE;
			fgIsTransition = TRUE;

			break;

#if CFG_SUPPORT_ADHOC
		case AIS_STATE_IBSS_ALONE:
			aisFsmStateInit_IBSS_ALONE(prAdapter,
				ucBssIndex);
			break;

		case AIS_STATE_IBSS_MERGE:
			aisFsmStateInit_IBSS_MERGE(prAdapter,
				prAisFsmInfo->prTargetBssDesc,
				ucBssIndex);
			break;
#endif /* CFG_SUPPORT_ADHOC */

		case AIS_STATE_NORMAL_TR:
			/* Renew op trx nss */
			cnmOpModeGetTRxNss(prAdapter,
					   prAisBssInfo->ucBssIndex,
					   &prAisBssInfo->ucOpRxNss,
					   &prAisBssInfo->ucOpTxNss);

			/* Don't do anything when rJoinTimeoutTimer
			 * is still ticking
			 */
			if (timerPendingTimer(&prAisFsmInfo->rJoinTimeoutTimer))
				break;

			if (prAisFsmInfo->ePreviousState ==
					AIS_STATE_OFF_CHNL_TX)
				aisFunClearAllTxReq(prAdapter,
						&(prAisFsmInfo->rMgmtTxInfo));

			/* 1. Process for pending roaming scan */
			if (aisFsmIsRequestPending(prAdapter,
				AIS_REQUEST_ROAMING_SEARCH, TRUE,
				ucBssIndex) == TRUE) {
				eNextState = AIS_STATE_LOOKING_FOR;
				fgIsTransition = TRUE;
			}
			/* 2. Process for pending roaming connect */
			else if (aisFsmIsRequestPending(prAdapter,
					AIS_REQUEST_ROAMING_CONNECT, TRUE,
					ucBssIndex)
						== TRUE) {
				eNextState = AIS_STATE_SEARCH;
				fgIsTransition = TRUE;
			}
			/* 3. Process for pending scan */
			else if (aisFsmIsRequestPending(prAdapter,
					AIS_REQUEST_SCAN, TRUE,
					ucBssIndex) == TRUE) {
				wlanClearScanningResult(prAdapter, ucBssIndex);
				eNextState = AIS_STATE_ONLINE_SCAN;
				fgIsTransition = TRUE;
			} else if (aisFsmIsRequestPending(prAdapter,
					AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE,
					ucBssIndex)
								== TRUE) {
				eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
				fgIsTransition = TRUE;
			}

			break;

		case AIS_STATE_DISCONNECTING:
			/* send for deauth frame for disconnection */
			authSendDeauthFrame(prAdapter,
					    prAisBssInfo,
					    prAisBssInfo->prStaRecOfAP,
					    (struct SW_RFB *)NULL,
					    REASON_CODE_DEAUTH_LEAVING_BSS,
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
				DBGLOG(AIS, ERROR, "Can't indicate CNM\n");
				return;
			}

			/* release channel */
			aisFsmReleaseCh(prAdapter, ucBssIndex);

			/* zero-ize */
			kalMemZero(prMsgChReq, sizeof(struct MSG_CH_REQ));

			/* filling */
			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex =
			    prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType =
					prAisFsmInfo->rChReqInfo.eReqType;
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
					   prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter,
					       prAisBssInfo->ucBssIndex);

				/* sync with firmware */
				nicActivateNetwork(prAdapter,
						   prAisBssInfo->ucBssIndex);
			}

			break;

		case AIS_STATE_OFF_CHNL_TX:
			if (!IS_NET_ACTIVE(prAdapter,
				prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter,
					prAisBssInfo->ucBssIndex);

				/* sync with firmware */
				nicActivateNetwork(prAdapter,
					prAisBssInfo->ucBssIndex);
			}
			if (!aisState_OFF_CHNL_TX(prAdapter, ucBssIndex)) {
				if (prAisBssInfo->eConnectionState ==
						MEDIA_STATE_CONNECTED)
					eNextState = AIS_STATE_NORMAL_TR;
				else
					eNextState = AIS_STATE_IDLE;
				fgIsTransition = TRUE;
			}
			break;

		default:
			/* Make sure we have handle all STATEs */
			ASSERT(0);
			break;

		}
	} while (fgIsTransition);

	return;

}				/* end of aisFsmSteps() */

enum ENUM_AIS_STATE aisFsmStateSearchAction(
	IN struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	if (prConnSettings->eOPMode == NET_TYPE_INFRA)
		prAisFsmInfo->ucConnTrialCount++;

#if CFG_SUPPORT_ADHOC
	if (prConnSettings->eOPMode == NET_TYPE_IBSS ||
		   prConnSettings->eOPMode == NET_TYPE_AUTO_SWITCH ||
		   prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS) {
		prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
		prAisFsmInfo->prTargetBssDesc = NULL;
		return AIS_STATE_IBSS_ALONE;
	}
#endif /* CFG_SUPPORT_ADHOC */

	return AIS_STATE_LOOKING_FOR;
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
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq;
	struct BCN_RM_PARAMS *prBcnRmParam;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFsmRunEventScanDone()");

	prScanDoneMsg = (struct MSG_SCN_SCAN_DONE *)prMsgHdr;
	ucBssIndex = prScanDoneMsg->ucBssIndex;

	DBGLOG(AIS, LOUD, "[%d] EVENT-SCAN DONE: Current Time = %u\n",
		ucBssIndex,
		kalGetTimeTick());

	if (aisGetAisBssInfo(prAdapter, ucBssIndex) == NULL) {
		/* This case occurs when the AIS isn't done, but the wlan0 */
		/* has changed to AP mode. And the prAisBssInfo is freed.  */
		DBGLOG(AIS, WARN, "prAisBssInfo is NULL, and then return\n");
		return;
	}

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prRoamingFsmInfo =
		aisGetRoamingInfo(prAdapter, ucBssIndex);
	prRmReq = aisGetRmReqParam(prAdapter, ucBssIndex);
	prBcnRmParam = &prRmReq->rBcnRmParam;

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
		kalScanDone(prAdapter->prGlueInfo, ucBssIndex,
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
			scanGetCurrentEssChnlList(prAdapter, ucBssIndex);

#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter,
				ucBssIndex);
#else
			eNextState = AIS_STATE_NORMAL_TR;
#endif /* CFG_SUPPORT_ROAMING */
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
/* Support AP Selection */

/* end Support AP Selection */
			break;

		case AIS_STATE_LOOKING_FOR:
			scanGetCurrentEssChnlList(prAdapter, ucBssIndex);

#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter,
				ucBssIndex);
#else
			eNextState = AIS_STATE_SEARCH;
#endif /* CFG_SUPPORT_ROAMING */
			break;

		default:
			break;

		}
	}
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState, ucBssIndex);
	if (prBcnRmParam->eState == RM_NO_REQUEST)
		return;
	/* normal mode scan done, and beacon measurement is pending,
	 ** schedule to do measurement
	 */
	if (prBcnRmParam->eState == RM_WAITING) {
		rrmDoBeaconMeasurement(prAdapter, ucBssIndex);
		/* pending normal scan here, should schedule it on time */
	} else if (prBcnRmParam->rNormalScan.fgExist) {
		struct NORMAL_SCAN_PARAMS *prParam = &prBcnRmParam->rNormalScan;

		DBGLOG(AIS, INFO,
		       "BCN REQ: Schedule normal scan after a beacon measurement done\n");
		prBcnRmParam->eState = RM_WAITING;
		prBcnRmParam->rNormalScan.fgExist = FALSE;
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer,
				   SEC_TO_MSEC(AIS_SCN_DONE_TIMEOUT_SEC));

		aisFsmScanRequestAdv(prAdapter, &prParam->rScanRequest);
		/* Radio Measurement is on-going, schedule to next Measurement
		 ** Element
		 */
	} else {
#if CFG_SUPPORT_802_11K
		struct LINK *prBSSDescList =
			&prAdapter->rWifiVar.rScanInfo.rBSSDescList;
		struct BSS_DESC *prBssDesc = NULL;
		uint32_t count = 0;

		/* collect updated bss for beacon request measurement */
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry,
				    struct BSS_DESC)
		{
			if (TIME_BEFORE(prRmReq->rScanStartTime,
				prBssDesc->rUpdateTime)) {
				rrmCollectBeaconReport(
					prAdapter, prBssDesc, ucBssIndex);
				count++;
			}
		}
		DBGLOG(RRM, INFO, "BCN report Active Mode, total: %d\n", count);
#endif
		rrmStartNextMeasurement(prAdapter, FALSE, ucBssIndex);
	}
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
	struct BSS_INFO *prAisBssInfo;
	uint8_t ucReasonOfDisconnect;
	u_int8_t fgDelayIndication;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFsmRunEventAbort()");

	/* 4 <1> Extract information of Abort Message and then free memory. */
	prAisAbortMsg = (struct MSG_AIS_ABORT *)prMsgHdr;
	ucReasonOfDisconnect = prAisAbortMsg->ucReasonOfDisconnect;
	fgDelayIndication = prAisAbortMsg->fgDelayIndication;
	ucBssIndex = prAisAbortMsg->ucBssIndex;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	cnmMemFree(prAdapter, prMsgHdr);

	DBGLOG(AIS, STATE,
	       "[%d] EVENT-ABORT: Current State %s, ucReasonOfDisconnect:%d\n",
	       ucBssIndex,
	       aisGetFsmState(prAisFsmInfo->eCurrentState),
	       ucReasonOfDisconnect);

	/* record join request time */
	GET_CURRENT_SYSTIME(&(prAisFsmInfo->rJoinReqTime));

	/* 4 <2> clear previous pending connection request and insert new one */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED ||
	    ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED) {
		struct STA_RECORD *prSta = prAisFsmInfo->prTargetStaRec;
		struct BSS_DESC *prBss = prAisFsmInfo->prTargetBssDesc;

		if (prSta && prBss) {
			struct AIS_BLACKLIST_ITEM *blk =
			    aisAddBlacklist(prAdapter, prBss);

			if (blk) {
				blk->u2DeauthReason = prSta->u2ReasonCode;
				blk->fgDeauthLastTime = TRUE;
			}
		}
	}

	/* to support user space triggered roaming */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_ROAMING &&
	    prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
		cnmTimerStopTimer(prAdapter,
				  &prAisFsmInfo->rSecModeChangeTimer);
#endif
		prAisBssInfo->ucReasonOfDisconnect = ucReasonOfDisconnect;
		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			/* 1. release channel */
			aisFsmReleaseCh(prAdapter, ucBssIndex);
			/* 2.1 stop join timeout timer */
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rJoinTimeoutTimer);
			aisFsmSteps(prAdapter, AIS_STATE_SEARCH, ucBssIndex);
		} else {
			aisFsmRemoveRoamingRequest(prAdapter, ucBssIndex);
			aisFsmInsertRequest(prAdapter,
					    AIS_REQUEST_ROAMING_CONNECT,
					    ucBssIndex);
		}
		return;
	}
	/* Support AP Selection */
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	scanGetCurrentEssChnlList(prAdapter, ucBssIndex);
#endif
	/* end Support AP Selection */

	aisFsmClearRequest(prAdapter, AIS_REQUEST_RECONNECT, ucBssIndex);
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_NEW_CONNECTION ||
	    ucReasonOfDisconnect == DISCONNECT_REASON_CODE_REASSOCIATION ||
	    ucReasonOfDisconnect == DISCONNECT_REASON_CODE_ROAMING)
		aisFsmInsertRequestToHead(prAdapter,
			AIS_REQUEST_RECONNECT, ucBssIndex);

	if (prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
		if (ucReasonOfDisconnect !=
		    DISCONNECT_REASON_CODE_REASSOCIATION) {
			/* 4 <3> invoke abort handler */
			aisFsmStateAbort(prAdapter, ucReasonOfDisconnect,
				fgDelayIndication, ucBssIndex);
		} else {
			/* 1. release channel */
			aisFsmReleaseCh(prAdapter, ucBssIndex);
			/* 2.1 stop join timeout timer */
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rJoinTimeoutTimer);

			prAisBssInfo->ucReasonOfDisconnect =
				ucReasonOfDisconnect;
			aisFsmSteps(prAdapter, AIS_STATE_IDLE, ucBssIndex);
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
		uint8_t ucReasonOfDisconnect, u_int8_t fgDelayIndication,
		uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	u_int8_t fgIsCheckConnected;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	/* XXX: The wlan0 may has been changed to AP mode. */
	if (prAisBssInfo == NULL)
		return;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	fgIsCheckConnected = FALSE;

	DBGLOG(AIS, STATE,
		"[%d] aisFsmStateAbort DiscReason[%d], CurState[%d], delayIndi[%d]\n",
		ucBssIndex, ucReasonOfDisconnect,
		prAisFsmInfo->eCurrentState, fgDelayIndication);

	/* 4 <1> Save information of Abort Message and then free memory. */
	prAisBssInfo->ucReasonOfDisconnect = ucReasonOfDisconnect;
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED &&
	    prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING &&
	    ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION &&
	    ucReasonOfDisconnect != DISCONNECT_REASON_CODE_ROAMING)
		wmmNotifyDisconnected(prAdapter, ucBssIndex);


	if (fgDelayIndication) {
		uint8_t p2p = cnmP2pIsActive(prAdapter);
		uint8_t join = timerPendingTimer(
				&prAisFsmInfo->rJoinTimeoutTimer);

		if (p2p || join) {
			fgDelayIndication = FALSE;
			DBGLOG(AIS, INFO,
				"delay indication not allowed due to p2p=%d, join=%d",
				p2p, join);
		}
	}

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

	case AIS_STATE_ONLINE_SCAN:
		fgIsCheckConnected = TRUE;
	case AIS_STATE_SCAN:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter, ucBssIndex);
		break;
	case AIS_STATE_LOOKING_FOR:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter, ucBssIndex);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_REQ_CHANNEL_JOIN:
		/* Release channel to CNM */
		aisFsmReleaseCh(prAdapter, ucBssIndex);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_JOIN:
		/* Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter, ucBssIndex);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

#if CFG_SUPPORT_ADHOC
	case AIS_STATE_IBSS_ALONE:
	case AIS_STATE_IBSS_MERGE:
		aisFsmStateAbort_IBSS(prAdapter, ucBssIndex);
		break;
#endif /* CFG_SUPPORT_ADHOC */
	case AIS_STATE_NORMAL_TR:
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_DISCONNECTING:
		/* Do abort NORMAL_TR */
		aisFsmStateAbort_NORMAL_TR(prAdapter, ucBssIndex);

		break;

	case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
		/* release channel */
		aisFsmReleaseCh(prAdapter, ucBssIndex);
		break;

	case AIS_STATE_REMAIN_ON_CHANNEL:
	case AIS_STATE_OFF_CHNL_TX:
		fgIsCheckConnected = TRUE;
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter, ucBssIndex);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter,
				  &prAisFsmInfo->rChannelTimeoutTimer);

		break;

	default:
		break;
	}

	if (fgIsCheckConnected
	    && (prAisBssInfo->eConnectionState ==
		MEDIA_STATE_CONNECTED)) {

		/* switch into DISCONNECTING state for sending DEAUTH
		 * if necessary
		 */
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
		    (prAisBssInfo->ucReasonOfDisconnect ==
		     DISCONNECT_REASON_CODE_NEW_CONNECTION ||
		     prAisBssInfo->ucReasonOfDisconnect ==
		     DISCONNECT_REASON_CODE_LOCALLY)
		    && prAisBssInfo->prStaRecOfAP
		    && prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			aisFsmSteps(prAdapter, AIS_STATE_DISCONNECTING,
				ucBssIndex);

			return;
		}
		/* Do abort NORMAL_TR */
		aisFsmStateAbort_NORMAL_TR(prAdapter, ucBssIndex);
	}
	rrmFreeMeasurementResources(prAdapter, ucBssIndex);
	aisFsmDisconnect(prAdapter, fgDelayIndication, ucBssIndex);

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
	struct STA_RECORD *prStaRec;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFsmRunEventJoinComplete()");

	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *)prMsgHdr;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
	prStaRec = prJoinCompMsg->prStaRec;
	if (prStaRec)
		ucBssIndex = prStaRec->ucBssIndex;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	eNextState = prAisFsmInfo->eCurrentState;

	/* Check State and SEQ NUM */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN) {
		/* Check SEQ NUM */
		if (prJoinCompMsg->ucSeqNum == prAisFsmInfo->ucSeqNumOfReqMsg)
			eNextState =
			    aisFsmJoinCompleteAction(prAdapter, prMsgHdr);
		else {
			eNextState = AIS_STATE_JOIN_FAILURE;
			DBGLOG(AIS, WARN,
			       "SEQ NO of AIS JOIN COMP MSG is not matched.\n");
		}
	}

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState, ucBssIndex);

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);
}				/* end of aisFsmRunEventJoinComplete() */

enum ENUM_AIS_STATE aisFsmJoinCompleteAction(IN struct ADAPTER *prAdapter,
					     IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_SAA_FSM_COMP *prJoinCompMsg;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct ROAMING_INFO *roam;
	enum ENUM_AIS_STATE eNextState;
	struct STA_RECORD *prStaRec;
	struct SW_RFB *prAssocRspSwRfb;
	struct BSS_INFO *prAisBssInfo;
	OS_SYSTIME rCurrentTime;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFsmJoinCompleteAction()");

	GET_CURRENT_SYSTIME(&rCurrentTime);

	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *)prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

	ucBssIndex = prStaRec->ucBssIndex;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	eNextState = prAisFsmInfo->eCurrentState;
	roam = aisGetRoamingInfo(prAdapter, ucBssIndex);

	do {
		/* 4 <1> JOIN was successful */
		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
			prConnSettings->fgSecModeChangeStartTimer = FALSE;
#endif

			/* 1. Reset retry count */
			prAisFsmInfo->ucConnTrialCount = 0;

			/* Completion of roaming */
			if (prAisBssInfo->eConnectionState ==
			    MEDIA_STATE_CONNECTED) {

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
				kalResetStats(
					wlanGetNetDev(
					prAdapter->prGlueInfo,
					ucBssIndex));

				/* 4 <1.1> Change FW's Media State
				 * immediately.
				 */
				aisChangeMediaState(prAisBssInfo,
					MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous AP's STA_RECORD_T
				 * in Driver if have.
				 */
				if ((prAisBssInfo->prStaRecOfAP) &&
				    (prAisBssInfo->prStaRecOfAP != prStaRec) &&
				    (prAisBssInfo->prStaRecOfAP->fgIsInUse) &&
				    (prAisBssInfo->prStaRecOfAP->ucBssIndex ==
				     prAisBssInfo->ucBssIndex)) {

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
					MEDIA_STATE_CONNECTED,
					FALSE,
					ucBssIndex);

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
			/* if roaming fsm is monitoring old AP, abort it*/
			if (roam->eCurrentState >= ROAMING_STATE_DECISION)
				roamingFsmRunEventAbort(prAdapter, ucBssIndex);

			/* if user space roaming is enabled, we should
			 * disable driver/fw roaming
			 */
			if (prConnSettings->eConnectionPolicy !=
			     CONNECT_BY_BSSID && roam->fgDrvRoamingAllow) {
				prConnSettings->eConnectionPolicy =
					CONNECT_BY_SSID_BEST_RSSI;
				roamingFsmRunEventStart(prAdapter, ucBssIndex);
			}
#endif /* CFG_SUPPORT_ROAMING */
			if (aisFsmIsRequestPending
			    (prAdapter, AIS_REQUEST_ROAMING_CONNECT,
			     FALSE, ucBssIndex) == FALSE)
				prAisFsmInfo->rJoinReqTime = 0;

			/* remove all deauthing AP from blacklist */
			aisRemoveDeauthBlacklist(prAdapter);
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;

#if CFG_SUPPORT_802_11K
			aisResetNeighborApList(prAdapter, ucBssIndex);
			if (prAisFsmInfo->prTargetBssDesc->aucRrmCap[0] &
			    BIT(RRM_CAP_INFO_NEIGHBOR_REPORT_BIT))
				aisSendNeighborRequest(prAdapter,
					ucBssIndex);
#endif

			/* 4 <1.7> Set the Next State of AIS FSM */
			eNextState = AIS_STATE_NORMAL_TR;
		}
		/* 4 <2> JOIN was not successful */
		else {
			/* 4 <2.1> Redo JOIN process with other Auth Type
			 * if possible
			 */
			if (aisFsmStateInit_RetryJOIN(prAdapter, prStaRec,
				ucBssIndex) == FALSE) {
				struct BSS_DESC *prBssDesc;
				struct PARAM_SSID rSsid;
				struct CONNECTION_SETTINGS *prConnSettings;

				prConnSettings =
				    aisGetConnSettings(prAdapter,
				    ucBssIndex);
				prBssDesc = prAisFsmInfo->prTargetBssDesc;

				/* 1. Increase Failure Count */
				prStaRec->ucJoinFailureCount++;

				/* 2. release channel */
				aisFsmReleaseCh(prAdapter, ucBssIndex);

				/* 3.1 stop join timeout timer */
				cnmTimerStopTimer(prAdapter,
					&prAisFsmInfo->rJoinTimeoutTimer);

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

				if (prBssDesc == NULL) {
					eNextState = AIS_STATE_JOIN_FAILURE;
					DBGLOG(AIS, ERROR,
						"Can't get bss descriptor\n");
					break;
				}

				DBGLOG(AIS, INFO,
				       "ucJoinFailureCount=%d %d, Status=%d Reason=%d, eConnectionState=%d",
				       prStaRec->ucJoinFailureCount,
				       prBssDesc->ucJoinFailureCount,
				       prStaRec->u2StatusCode,
				       prStaRec->u2ReasonCode,
				       prAisBssInfo->eConnectionState);

				prBssDesc->u2JoinStatus =
				    prStaRec->u2StatusCode;
				prBssDesc->ucJoinFailureCount++;
				if (prBssDesc->ucJoinFailureCount >=
				    SCN_BSS_JOIN_FAIL_THRESOLD ||
				    scanApOverload(prStaRec->u2StatusCode,
						prStaRec->u2ReasonCode)) {
					/* Support AP Selection */
					aisAddBlacklist(prAdapter, prBssDesc);

					GET_CURRENT_SYSTIME
					    (&prBssDesc->rJoinFailTime);
					DBGLOG(AIS, INFO,
					       "Bss " MACSTR
					       " join fail %d times, temp disable it at time: %u\n",
					       MAC2STR(prBssDesc->aucBSSID),
					       prBssDesc->ucJoinFailureCount,
					       prBssDesc->rJoinFailTime);
				}

				if (prStaRec->u2StatusCode
					== STATUS_INVALID_PMKID) {
					struct AIS_BLACKLIST_ITEM *prBlackList =
						aisAddBlacklist(prAdapter,
							prBssDesc);
					if (prBlackList) {
						prBlackList->u2AuthStatus =
							prStaRec->u2StatusCode;
						DBGLOG(AIS, INFO,
						    "Add blacklist due to STATUS_INVALID_PMKID\n");
					}
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
				    MEDIA_STATE_CONNECTED ||
				    prStaRec->u2StatusCode ==
				    STATUS_CODE_ASSOC_REJECTED_TEMPORARILY) {
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
						AIS_REQUEST_RECONNECT,
						ucBssIndex);

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
void aisFsmCreateIBSS(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	do {
		/* Check State */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_IBSS_ALONE)
			aisUpdateBssInfoForCreateIBSS(prAdapter, ucBssIndex);

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
	uint8_t ucBssIndex = 0;

	ucBssIndex = prStaRec->ucBssIndex;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	do {

		eNextState = prAisFsmInfo->eCurrentState;

		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_IBSS_MERGE:
			{
				struct BSS_DESC *prBssDesc;

				/* 4 <1.1> Change FW's Media State
				 * immediately.
				 */
				aisChangeMediaState(prAisBssInfo,
					MEDIA_STATE_CONNECTED);

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
					MEDIA_STATE_CONNECTED,
					FALSE,
					ucBssIndex);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter, ucBssIndex);

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
			aisFsmSteps(prAdapter, eNextState, ucBssIndex);

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
	uint8_t ucBssIndex = 0;

	prAisIbssPeerFoundMsg = (struct MSG_AIS_IBSS_PEER_FOUND *)prMsgHdr;
	ucBssIndex = prAisIbssPeerFoundMsg->ucBssIndex;
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	prStaRec = prAisIbssPeerFoundMsg->prStaRec;

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
				aisChangeMediaState(prAisBssInfo,
					MEDIA_STATE_CONNECTED);

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
					     prAisBssInfo->ucBssIndex);

				/* 4 <1.7> Indicate Connected Event to Host
				 * immediately.
				 */
				aisIndicationOfMediaStateToHost(prAdapter,
					MEDIA_STATE_CONNECTED,
					FALSE,
					ucBssIndex);

				/* 4 <1.8> indicate PM for connected */
				nicPmIndicateBssConnected(prAdapter,
					prAisBssInfo->ucBssIndex);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter, ucBssIndex);
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
		aisFsmSteps(prAdapter, eNextState, ucBssIndex);
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
				u_int8_t fgDelayIndication,
				uint8_t ucBssIndex)
{
	struct EVENT_CONNECTION_STATUS rEventConnStatus;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct ROAMING_INFO *prRoamingFsmInfo;

	DEBUGFUNC("aisIndicationOfMediaStateToHost()");

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);

	DBGLOG(AIS, LOUD,
	       "AIS%d indicate Media State to Host Current State [%d]\n",
	       ucBssIndex,
	       prAisBssInfo->eConnectionState);

	/* NOTE(Kevin): Move following line to aisChangeMediaState()
	 * macro per CM's request.
	 */
	/* prAisBssInfo->eConnectionState = eConnectionState; */

	/* For indicating the Disconnect Event only if current media state is
	 * disconnected and we didn't do indication yet.
	 */
	DBGLOG(AIS, INFO,
		"[%d] Current state: %d, connection state indicated: %d\n",
		ucBssIndex,
		prAisFsmInfo->eCurrentState,
		prAisBssInfo->eConnectionStateIndicated);

	if (prAisBssInfo->eConnectionState == MEDIA_STATE_DISCONNECTED &&
		/* if receive DEAUTH in JOIN state, report disconnect*/
		!(prAisBssInfo->ucReasonOfDisconnect ==
		 DISCONNECT_REASON_CODE_DEAUTHENTICATED &&
		 prAisFsmInfo->eCurrentState == AIS_STATE_JOIN)) {
		if (prAisBssInfo->eConnectionStateIndicated == eConnectionState)
			return;
	}

	if (!fgDelayIndication) {
		/* 4 <0> Cancel Delay Timer */
		prAisFsmInfo->u4PostponeIndStartTime = 0;

		/* 4 <1> Fill EVENT_CONNECTION_STATUS */
		rEventConnStatus.ucMediaStatus = (uint8_t) eConnectionState;

		if (eConnectionState == MEDIA_STATE_CONNECTED) {
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
				rEventConnStatus.ucNetworkType =
				    (uint8_t) PARAM_NETWORK_TYPE_DS;
				break;
			}
		} else {
			rEventConnStatus.ucReasonOfDisconnect =
			    prAisBssInfo->ucReasonOfDisconnect;
		}

		/* 4 <2> Indication */
		nicMediaStateChange(prAdapter,
				    prAisBssInfo->ucBssIndex,
				    &rEventConnStatus);
		prAisBssInfo->eConnectionStateIndicated = eConnectionState;
		if (eConnectionState == MEDIA_STATE_DISCONNECTED) {
			aisRemoveDeauthBlacklist(prAdapter);
			prAisFsmInfo->prTargetBssDesc = NULL;
			prAisFsmInfo->prTargetStaRec = NULL;
			prAisFsmInfo->ucConnTrialCount = 0;
			prAdapter->rAddRoamScnChnl.ucChannelListNum = 0;
			prRoamingFsmInfo->eReason = ROAMING_REASON_POOR_RCPI;
#if CFG_SUPPORT_NCHO
			wlanNchoInit(prAdapter, TRUE);
#endif
		}
	} else {
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
		IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct AIS_FSM_INFO *prAisFsmInfo;
	bool fgFound = TRUE;
	bool fgIsPostponeTimeout;
	enum ENUM_PARAM_CONNECTION_POLICY policy;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
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

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	fgIsPostponeTimeout = !aisFsmIsInProcessPostpone(prAdapter, ucBssIndex);
	policy = prConnSettings->eConnectionPolicy;

	DBGLOG(AIS, EVENT, "policy %d, timeout %d, trial %d, limit %d\n",
		policy,	fgIsPostponeTimeout, prAisFsmInfo->ucConnTrialCount,
		prAisFsmInfo->ucConnTrialCountLimit);

	/* only retry connect once when beacon timeout */
	if (policy != CONNECT_BY_BSSID && !fgIsPostponeTimeout &&
	    !(prAisFsmInfo->ucConnTrialCount >
			prAisFsmInfo->ucConnTrialCountLimit)) {
		DBGLOG(AIS, INFO,
		       "DelayTimeOfDisconnect, don't report disconnect\n");
		return;
	}

	/* 4 <2> Remove all connection request */
	while (fgFound)
		fgFound = aisFsmClearRequest(prAdapter,
				AIS_REQUEST_RECONNECT, ucBssIndex);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR)
		prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;

	/* 4 <3> Indicate Disconnected Event to Host immediately. */
	aisIndicationOfMediaStateToHost(prAdapter,
					MEDIA_STATE_DISCONNECTED, FALSE,
					ucBssIndex);
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
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame;
	struct BSS_DESC *prBssDesc;
	uint16_t u2IELength;
	uint8_t *pucIE;
	struct PARAM_SSID rSsid;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisUpdateBssInfoForJOIN()");

	ucBssIndex = prStaRec->ucBssIndex;
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prAssocRspFrame =
	    (struct WLAN_ASSOC_RSP_FRAME *)prAssocRspSwRfb->pvHeader;

	DBGLOG(AIS, INFO,
		"[%d] Update AIS_BSS_INFO_T and apply settings to MAC\n",
		ucBssIndex);

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
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;
		prBssDesc->ucJoinFailureCount = 0;

		aisRemoveBlackList(prAdapter, prBssDesc);
		/* 4 <4.1> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
#if (CFG_SUPPORT_802_11V_MBSSID == 1)
		prAisBssInfo->ucMaxBSSIDIndicator =
					prBssDesc->ucMaxBSSIDIndicator;
		prAisBssInfo->ucMBSSIDIndex = prBssDesc->ucMBSSIDIndex;
#endif
	}

	/* NOTE: Defer ucDTIMPeriod updating to when beacon is received
	 * after connection
	 */
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->fgTIMPresent = TRUE;
	prAisBssInfo->u2ATIMWindow = 0;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_INFRA;

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	prAisSpecBssInfo->ucRoamSkipTimes = ROAMING_ONE_AP_SKIP_TIMES;
	prAisSpecBssInfo->fgGoodRcpiArea = FALSE;
	prAisSpecBssInfo->fgPoorRcpiArea = FALSE;
#endif

	/* 4 <4.2> Update HT information and set channel */
	/* Record HT related parameters in rStaRec and rBssInfo
	 * Note: it shall be called before nicUpdateBss()
	 */
	rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	secPostUpdateAddr(prAdapter,
		aisGetAisBssInfo(prAdapter, ucBssIndex));

	/* 4 <4.3> Sync with firmware for BSS-INFO */
	nicUpdateBss(prAdapter, ucBssIndex);

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
void aisUpdateBssInfoForCreateIBSS(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

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
	nicUpdateBss(prAdapter, prAisBssInfo->ucBssIndex);

	/* 4 <3.3> enable beaconing */
	bssUpdateBeaconContent(prAdapter, prAisBssInfo->ucBssIndex);

	/* 4 <3.4> Update AdHoc PM parameter */
	nicPmIndicateBssCreated(prAdapter, prAisBssInfo->ucBssIndex);

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
	uint8_t ucBssIndex = 0;

	ucBssIndex = prStaRec->ucBssIndex;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

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
	}

	/* 3 <5> Set MAC HW */
	/* 4 <5.1> Find Lowest Basic Rate Index for default TX Rate of MMPDU */
	nicTxUpdateBssDefaultRate(prAisBssInfo);

	/* 4 <5.2> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <5.3> use command packets to inform firmware */
	nicUpdateBss(prAdapter, prAisBssInfo->ucBssIndex);

	/* 4 <5.4> enable beaconing */
	bssUpdateBeaconContent(prAdapter, prAisBssInfo->ucBssIndex);

	/* 4 <5.5> Update AdHoc PM parameter */
	nicPmIndicateBssConnected(prAdapter,
				  prAisBssInfo->ucBssIndex);

	/* 3 <6> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;
}				/* end of aisUpdateBssInfoForMergeIBSS() */

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
			     IN uint8_t ucBssIndex,
			     OUT uint32_t *pu4ControlFlags)
{
	struct WLAN_MAC_MGMT_HEADER *prMgtHdr;
	struct BSS_INFO *prBssInfo;
	struct IE_SSID *prIeSsid = (struct IE_SSID *)NULL;
	uint8_t *pucIE;
	uint16_t u2IELength;
	uint16_t u2Offset = 0;
	u_int8_t fgReplyProbeResp = FALSE;

	prBssInfo = aisGetAisBssInfo(prAdapter,
		ucBssIndex);

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
void aisFsmDisconnect(IN struct ADAPTER *prAdapter,
		IN u_int8_t fgDelayIndication, IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo;
	uint16_t u2ReasonCode = REASON_CODE_UNSPECIFIED;
	struct BSS_DESC *prBssDesc = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

#if (CFG_SUPPORT_TWT == 1)
	twtPlannerReset(prAdapter, prAisBssInfo);
#endif

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerStopTimer(prAdapter,
		aisGetSecModeChangeTimer(prAdapter, ucBssIndex));
#endif
	nicPmIndicateBssAbort(prAdapter, prAisBssInfo->ucBssIndex);

#if CFG_SUPPORT_ADHOC
	if (prAisBssInfo->fgIsBeaconActivated) {
		nicUpdateBeaconIETemplate(prAdapter,
					  IE_UPD_METHOD_DELETE_ALL,
					  prAisBssInfo->ucBssIndex,
					  0, NULL, 0);

		prAisBssInfo->fgIsBeaconActivated = FALSE;
	}
#endif

	rlmBssAborted(prAdapter, prAisBssInfo);

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and send Deauth
	 * if needed.
	 */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {

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

		if (prAisBssInfo->prStaRecOfAP) {
			u2ReasonCode =
				prAisBssInfo->prStaRecOfAP->u2ReasonCode;
		}
		if (prAisBssInfo->ucReasonOfDisconnect ==
			DISCONNECT_REASON_CODE_RADIO_LOST ||
		    prAisBssInfo->ucReasonOfDisconnect ==
			DISCONNECT_REASON_CODE_RADIO_LOST_TX_ERR) {
			scanRemoveBssDescByBssid(prAdapter,
						 prAisBssInfo->aucBSSID);

			/* remove from scanning results as well */
			wlanClearBssInScanningResult(prAdapter,
						     prAisBssInfo->aucBSSID);
		} else {
			scanRemoveConnFlagOfBssDescByBssid(prAdapter,
				prAisBssInfo->aucBSSID);
			prBssDesc = aisGetTargetBssDesc(prAdapter, ucBssIndex);
			if (prBssDesc) {
				prBssDesc->fgIsConnected = FALSE;
				prBssDesc->fgIsConnecting = FALSE;
			}
		}

		if (fgDelayIndication) {
			struct ROAMING_INFO *roam =
				aisGetRoamingInfo(prAdapter, ucBssIndex);

			/*
			 * There is a chance that roaming failed before
			 * beacon timeout, so reset trial count here to
			 * ensure the new reconnection runs correctly.
			 */
			prAisFsmInfo->ucConnTrialCount = 0;

			switch (prAisBssInfo->ucReasonOfDisconnect) {
			case DISCONNECT_REASON_CODE_RADIO_LOST:
				roam->eReason = ROAMING_REASON_BEACON_TIMEOUT;
				prAisFsmInfo->ucConnTrialCountLimit = 2;
				break;
			case DISCONNECT_REASON_CODE_RADIO_LOST_TX_ERR:
				roam->eReason = ROAMING_REASON_TX_ERR;
				prAisFsmInfo->ucConnTrialCountLimit = 2;
				break;
			case DISCONNECT_REASON_CODE_DEAUTHENTICATED:
			case DISCONNECT_REASON_CODE_DISASSOCIATED:
				roam->eReason = ROAMING_REASON_SAA_FAIL;
				prAisFsmInfo->ucConnTrialCountLimit = 1;
				break;
			default:
				DBGLOG(AIS, ERROR, "wrong reason %d",
					prAisBssInfo->ucReasonOfDisconnect);
			}
			aisFsmClearRequest(prAdapter,
				AIS_REQUEST_RECONNECT, ucBssIndex);
			aisFsmInsertRequest(prAdapter,
				AIS_REQUEST_RECONNECT, ucBssIndex);

			if (prAisBssInfo->eCurrentOPMode != OP_MODE_IBSS)
				prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		} else {
			prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		}
	} else {
		prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
	}

	/* 4 <4> Change Media State immediately. */
	if (prAisBssInfo->ucReasonOfDisconnect !=
	    DISCONNECT_REASON_CODE_REASSOCIATION) {
		aisChangeMediaState(prAisBssInfo,
			MEDIA_STATE_DISCONNECTED);

		/* 4 <4.1> sync. with firmware */
		nicUpdateBss(prAdapter, prAisBssInfo->ucBssIndex);
		prAisBssInfo->prStaRecOfAP = (struct STA_RECORD *)NULL;
	}

#if CFG_SUPPORT_ROAMING
	roamingFsmRunEventAbort(prAdapter, ucBssIndex);
	aisFsmRemoveRoamingRequest(prAdapter, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

	/* 4 <6> Indicate Disconnected Event to Host */
	aisIndicationOfMediaStateToHost(prAdapter,
					MEDIA_STATE_DISCONNECTED,
					fgDelayIndication,
					ucBssIndex);

	/* 4 <7> Trigger AIS FSM */
	aisFsmSteps(prAdapter, AIS_STATE_IDLE, ucBssIndex);
}				/* end of aisFsmDisconnect() */

static void aisFsmRunEventScanDoneTimeOut(IN struct ADAPTER *prAdapter,
					  unsigned long ulParam)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex = (uint8_t) ulParam;

	DEBUGFUNC("aisFsmRunEventScanDoneTimeOut()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	DBGLOG(AIS, STATE,
		"[%d] aisFsmRunEventScanDoneTimeOut Current[%d] Seq=%u\n",
		ucBssIndex,
		prAisFsmInfo->eCurrentState, prAisFsmInfo->ucSeqNumOfScanReq);

	prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	/* try to stop scan in CONNSYS */
	aisFsmStateAbort_SCAN(prAdapter, ucBssIndex);
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
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	DEBUGFUNC("aisFsmRunEventBGSleepTimeOut()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		DBGLOG(AIS, LOUD,
			"[%d] EVENT - SCAN TIMER: Idle End - Current Time = %u\n",
			ucBssIndex,
			kalGetTimeTick());

		eNextState = AIS_STATE_LOOKING_FOR;

		SET_NET_PWR_STATE_ACTIVE(prAdapter,
					 ucBssIndex);

		break;

	default:
		break;
	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState, ucBssIndex);
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
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	DEBUGFUNC("aisFsmRunEventIbssAloneTimeOut()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:

		/* There is no one participate in our AdHoc during this
		 * TIMEOUT Interval so go back to search for a valid
		 * IBSS again.
		 */

		DBGLOG(AIS, LOUD,
			"[%d] EVENT-IBSS ALONE TIMER: Start pairing\n",
			ucBssIndex);

		/* abort timer */
		aisFsmReleaseCh(prAdapter, ucBssIndex);

		/* Pull back to SEARCH to find candidate again */
		eNextState = AIS_STATE_SEARCH;

		break;

	default:
		break;
	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState, ucBssIndex);
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
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	DEBUGFUNC("aisFsmRunEventJoinTimeout()");

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	eNextState = prAisFsmInfo->eCurrentState;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_JOIN:
		DBGLOG(AIS, WARN, "EVENT- JOIN TIMEOUT\n");

		/* 1. Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter, ucBssIndex);

		/* 2. Increase Join Failure Count */
		/* Support AP Selection */
		aisAddBlacklist(prAdapter, prAisFsmInfo->prTargetBssDesc);
		prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount++;

		if (prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount <
		    JOIN_MAX_RETRY_FAILURE_COUNT) {
			/* 3.1 Retreat to AIS_STATE_SEARCH state for next try */
			eNextState = AIS_STATE_SEARCH;
		} else if (prAisBssInfo->eConnectionState ==
			   MEDIA_STATE_CONNECTED) {
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
		aisFsmReleaseCh(prAdapter, ucBssIndex);

		/* 2. process if there is pending scan */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN,
			TRUE, ucBssIndex) == TRUE) {
			wlanClearScanningResult(prAdapter, ucBssIndex);
			eNextState = AIS_STATE_ONLINE_SCAN;
		}
		/* 3. Process for pending roaming scan */
		else if (aisFsmIsRequestPending
			 (prAdapter, AIS_REQUEST_ROAMING_SEARCH,
			 TRUE, ucBssIndex) == TRUE)
			eNextState = AIS_STATE_LOOKING_FOR;
		/* 4. Process for pending roaming scan */
		else if (aisFsmIsRequestPending
			 (prAdapter, AIS_REQUEST_ROAMING_CONNECT,
			 TRUE, ucBssIndex) == TRUE)
			eNextState = AIS_STATE_SEARCH;
		else if (aisFsmIsRequestPending
			 (prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL,
			  TRUE, ucBssIndex) == TRUE)
			eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;

#if CFG_SUPPORT_LOWLATENCY_MODE
		/* 5. Check if need to set low latency after connected. */
		wlanConnectedForLowLatency(prAdapter);
#endif

		break;

	default:
		/* release channel */
		aisFsmReleaseCh(prAdapter, ucBssIndex);
		break;

	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState, ucBssIndex);
}				/* end of aisFsmRunEventJoinTimeout() */

void aisFsmRunEventDeauthTimeout(IN struct ADAPTER *prAdapter,
				 unsigned long ulParamPtr)
{
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	aisDeauthXmitCompleteBss(prAdapter, ucBssIndex, TX_RESULT_LIFE_TIMEOUT);
}

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
void aisFsmRunEventSecModeChangeTimeout(IN struct ADAPTER *prAdapter,
					unsigned long ulParamPtr)
{
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	DBGLOG(AIS, INFO,
		"[%d] Beacon security mode change timeout, trigger disconnect!\n",
		ucBssIndex);

	aisBssSecurityChanged(prAdapter, ucBssIndex);
}
#endif

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
		       IN uint32_t u4IeLength,
		       IN uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;

	DEBUGFUNC("aisFsmScanRequest()");

	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prScanRequest = &(prAisFsmInfo->rScanRequest);

	DBGLOG(SCN, TRACE,
		"[AIS%d] eCurrentState=%d, fgIsScanReqIssued=%d\n",
		ucBssIndex,
		prAisFsmInfo->eCurrentState, prConnSettings->fgIsScanReqIssued);
	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
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
			    OP_MODE_INFRASTRUCTURE &&
			    timerPendingTimer(&prAisFsmInfo->
						rJoinTimeoutTimer)) {
				/* 802.1x might not finished yet, pend it for
				 * later handling ..
				 */
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_SCAN,
						    ucBssIndex);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(SCN, WARN,
					"Scan Request with channel granted for join operation: %d, %d",
					prAisFsmInfo->fgIsChannelGranted,
					prAisFsmInfo->fgIsChannelRequested);
				}

				/* start online scan */
				wlanClearScanningResult(prAdapter, ucBssIndex);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN,
					ucBssIndex);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter, ucBssIndex);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN,
				ucBssIndex);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN,
				ucBssIndex);
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
	struct RADIO_MEASUREMENT_REQ_PARAMS *prRmReq;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFsmScanRequestAdv()");

	if (!prRequestIn) {
		log_dbg(SCN, WARN, "Scan request is NULL\n");
		return;
	}
	ucBssIndex = prRequestIn->ucBssIndex;
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prRmReq = aisGetRmReqParam(prAdapter, ucBssIndex);
	prScanRequest = &(prAisFsmInfo->rScanRequest);

	DBGLOG(SCN, TRACE, "[AIS%d] eCurrentState=%d, fgIsScanReqIssued=%d\n",
		ucBssIndex,
		prAisFsmInfo->eCurrentState, prConnSettings->fgIsScanReqIssued);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;

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
			    OP_MODE_INFRASTRUCTURE &&
			    timerPendingTimer(&prAisFsmInfo->
						rJoinTimeoutTimer)) {
				/* 802.1x might not finished yet, pend it for
				 * later handling ..
				 */
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_SCAN,
						    ucBssIndex);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(SCN, WARN,
					"Scan Request with channel granted for join operation: %d, %d",
					prAisFsmInfo->fgIsChannelGranted,
					prAisFsmInfo->fgIsChannelRequested);
				}

				/* start online scan */
				wlanClearScanningResult(prAdapter, ucBssIndex);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN,
					ucBssIndex);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter, ucBssIndex);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN,
				ucBssIndex);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN,
				ucBssIndex);
		}
	} else if (prRmReq->rBcnRmParam.eState ==
		   RM_ON_GOING) {
		struct NORMAL_SCAN_PARAMS *prNormalScan =
		    &prRmReq->rBcnRmParam.rNormalScan;

		prNormalScan->fgExist = TRUE;
		kalMemCopy(&(prNormalScan->rScanRequest), prRequestIn,
			sizeof(struct PARAM_SCAN_REQUEST_ADV));
		prNormalScan->rScanRequest.pucIE = prNormalScan->aucScanIEBuf;
		if (prRequestIn->u4IELength > 0 &&
		prRequestIn->u4IELength <= MAX_IE_LENGTH) {
			prNormalScan->rScanRequest.u4IELength =
			    prRequestIn->u4IELength;
			kalMemCopy(prNormalScan->rScanRequest.pucIE,
				   prRequestIn->pucIE, prRequestIn->u4IELength);
		} else {
			prNormalScan->rScanRequest.u4IELength = 0;
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
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	struct MSG_CH_GRANT *prMsgChGrant;
	uint8_t ucTokenID;
	uint32_t u4GrantInterval;
	uint8_t ucBssIndex = 0;

	prMsgChGrant = (struct MSG_CH_GRANT *)prMsgHdr;

	ucTokenID = prMsgChGrant->ucTokenID;
	u4GrantInterval = prMsgChGrant->u4GrantInterval;
	ucBssIndex = prMsgChGrant->ucBssIndex;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

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
		DBGLOG(AIS, INFO, "Start JOIN Timer!");
		aisFsmSteps(prAdapter, AIS_STATE_JOIN, ucBssIndex);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else if (prAisFsmInfo->eCurrentState ==
		   AIS_STATE_REQ_REMAIN_ON_CHANNEL
		   && prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

#if CFG_SUPPORT_NCHO
		if (prAdapter->rNchoInfo.fgNCHOEnabled == TRUE &&
		    prAdapter->rNchoInfo.fgIsSendingAF == TRUE &&
		    prAdapter->rNchoInfo.fgChGranted == FALSE) {
			DBGLOG(INIT, TRACE,
			       "NCHO complete rAisChGrntComp trace time is %u\n",
			       kalGetTimeTick());
			prAdapter->rNchoInfo.fgChGranted = TRUE;
			complete(&prAdapter->prGlueInfo->rAisChGrntComp);
		}
#endif
		if (prAisFsmInfo->rChReqInfo.eReqType ==
				CH_REQ_TYPE_OFFCHNL_TX) {
			aisFsmSteps(prAdapter, AIS_STATE_OFF_CHNL_TX,
				ucBssIndex);
		} else {
			/*
			 * 3.1 set timeout timer in cases upper layer
			 * cancel_remain_on_channel never comes
			 */
			cnmTimerStartTimer(prAdapter,
					&prAisFsmInfo->rChannelTimeoutTimer,
					prAisFsmInfo->u4ChGrantedInterval);

			/* 3.2 switch to remain_on_channel state */
			aisFsmSteps(prAdapter, AIS_STATE_REMAIN_ON_CHANNEL,
				ucBssIndex);

			/* 3.3. indicate upper layer for channel ready */
			kalReadyOnChannel(prAdapter->prGlueInfo,
					prAisFsmInfo->rChReqInfo.u8Cookie,
					prAisFsmInfo->rChReqInfo.eBand,
					prAisFsmInfo->rChReqInfo.eSco,
					prAisFsmInfo->rChReqInfo.ucChannelNum,
					prAisFsmInfo->rChReqInfo.u4DurationMs,
					ucBssIndex);
		}

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else {		/* mismatched grant */
		/* 2. return channel privilege to CNM immediately */
		/* aisFsmReleaseCh(prAdapter, ucBssIndex); */
		DBGLOG(AIS, WARN, "channel grant token mismatch\n");
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
void aisFsmReleaseCh(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_CH_ABORT *prMsgChAbort;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

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
			DBGLOG(AIS, ERROR, "Can't release Channel to CNM\n");
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucBssIndex = ucBssIndex;
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
void aisBssBeaconTimeout(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)

{
	/* trigger by driver, use dummy reason code */
	aisBssBeaconTimeout_impl(prAdapter, BEACON_TIMEOUT_REASON_NUM,
		DISCONNECT_REASON_CODE_RADIO_LOST, ucBssIndex);
}

void aisBssBeaconTimeout_impl(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBcnTimeoutReason, IN uint8_t ucDisconnectReason,
	IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo;
	u_int8_t fgDoAbortIndication = FALSE;
	u_int8_t fgIsReasonPER =
		(ucBcnTimeoutReason == BEACON_TIMEOUT_REASON_HIGH_PER);
	struct CONNECTION_SETTINGS *prConnSettings;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* 4 <1> Diagnose Connection for Beacon Timeout Event */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
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
		prAisBssInfo->u2DeauthReason =
			REASON_CODE_BEACON_TIMEOUT*100 +
			ucBcnTimeoutReason;

		DBGLOG(AIS, EVENT, "aisBssBeaconTimeout\n");
		aisFsmStateAbort(prAdapter,
			ucDisconnectReason,
			!fgIsReasonPER,
			ucBssIndex);
	}
}				/* end of aisBssBeaconTimeout() */

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
void aisBssSecurityChanged(struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{

	aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_DEAUTHENTICATED,
			 FALSE, ucBssIndex);
}
#endif

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
void aisBssLinkDown(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo;
	u_int8_t fgDoAbortIndication = FALSE;
	struct CONNECTION_SETTINGS *prConnSettings;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* 4 <1> Diagnose Connection for Beacon Timeout Event */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
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
		DBGLOG(AIS, EVENT, "aisBssLinkDown\n");
		aisFsmStateAbort(prAdapter,
				 DISCONNECT_REASON_CODE_DISASSOCIATED, FALSE,
				 ucBssIndex);
	}

	/* kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
	 * WLAN_STATUS_SCAN_COMPLETE, NULL, 0);
	 */
}				/* end of aisBssLinkDown() */

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
aisDeauthXmitCompleteBss(IN struct ADAPTER *prAdapter,
		      IN uint8_t ucBssIndex,
		      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
#if CFG_SUPPORT_802_11W
	/* Notify completion after encrypted deauth frame tx done */
	if (prAisBssInfo->encryptedDeauthIsInProcess == TRUE) {
		if (!completion_done(&prAisBssInfo->rDeauthComp)) {
			DBGLOG(AIS, EVENT, "Complete rDeauthComp\n");
			complete(&prAisBssInfo->rDeauthComp);
		}
	}
	prAisBssInfo->encryptedDeauthIsInProcess = FALSE;
#endif
	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_DISCONNECTING) {
		DBGLOG(AIS, EVENT, "aisDeauthXmitComplete\n");
		if (rTxDoneStatus != TX_RESULT_DROPPED_IN_DRIVER
		    && rTxDoneStatus != TX_RESULT_QUEUE_CLEARANCE)
			aisFsmStateAbort(prAdapter,
					 prAisBssInfo->ucReasonOfDisconnect,
					 FALSE, ucBssIndex);
	} else {
		DBGLOG(AIS, WARN,
		       "DEAUTH frame transmitted without further handling");
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of aisDeauthXmitComplete() */

uint32_t
aisDeauthXmitComplete(IN struct ADAPTER *prAdapter,
			IN struct MSDU_INFO *prMsduInfo,
			IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	return aisDeauthXmitCompleteBss(prAdapter,
		prMsduInfo->ucBssIndex, rTxDoneStatus);
}

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
	uint32_t u4ReqScan, uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	enum ENUM_AIS_REQUEST_TYPE eAisRequest = AIS_REQUEST_NUM;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

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
			roamingFsmRunEventRoam(prAdapter, ucBssIndex);
			roamingFsmRunEventFail(prAdapter,
					       ROAMING_FAIL_REASON_NOCANDIDATE,
					       ucBssIndex);
			return;
		}
	}
#endif
#endif

	/* results are still new */
	if (!u4ReqScan) {
		roamingFsmRunEventRoam(prAdapter, ucBssIndex);
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
	    && !timerPendingTimer(&prAisFsmInfo->rJoinTimeoutTimer)) {
		if (eAisRequest == AIS_REQUEST_ROAMING_SEARCH) {
			prAisFsmInfo->fgTargetChnlScanIssued = TRUE;
			aisFsmSteps(prAdapter, AIS_STATE_LOOKING_FOR,
				ucBssIndex);
		} else
			aisFsmSteps(prAdapter, AIS_STATE_SEARCH,
				ucBssIndex);
	} else {
		aisFsmRemoveRoamingRequest(prAdapter, ucBssIndex);
		aisFsmInsertRequest(prAdapter, eAisRequest, ucBssIndex);
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
enum ENUM_AIS_STATE aisFsmRoamingScanResultsUpdate(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct ROAMING_INFO *prRoamingFsmInfo;
	enum ENUM_AIS_STATE eNextState;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prRoamingFsmInfo =
	    aisGetRoamingInfo(prAdapter, ucBssIndex);

	roamingFsmScanResultsUpdate(prAdapter, ucBssIndex);

	eNextState = prAisFsmInfo->eCurrentState;
	if (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) {
		aisFsmRemoveRoamingRequest(prAdapter, ucBssIndex);
		roamingFsmRunEventRoam(prAdapter, ucBssIndex);
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
	uint8_t ucBssIndex = prTargetStaRec->ucBssIndex;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	if (prAisBssInfo->prStaRecOfAP != prTargetStaRec)
		wmmNotifyDisconnected(prAdapter, ucBssIndex);

	nicPmIndicateBssAbort(prAdapter, prAisBssInfo->ucBssIndex);

	/* Not invoke rlmBssAborted() here to avoid prAisBssInfo->fg40mBwAllowed
	 * to be reset. RLM related parameters will be reset again when handling
	 * association response in rlmProcessAssocRsp(). 20110413
	 */
	/* rlmBssAborted(prAdapter, prAisBssInfo); */

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and
	 * send Deauth if needed.
	 */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
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
	aisChangeMediaState(prAisBssInfo, MEDIA_STATE_ROAMING_DISC_PREV);

	/* 4 <4.1> sync. with firmware */
	/* Virtial BSSID */
	prTargetStaRec->ucBssIndex = (prAdapter->ucHwBssIdNum + 1);
	nicUpdateBss(prAdapter, prAisBssInfo->ucBssIndex);

	secRemoveBssBcEntry(prAdapter, prAisBssInfo, TRUE);
	prTargetStaRec->ucBssIndex = prAisBssInfo->ucBssIndex;
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
	uint8_t ucBssIndex = prStaRec->ucBssIndex;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	/* 4 <1.1> Change FW's Media State immediately. */
	aisChangeMediaState(prAisBssInfo, MEDIA_STATE_CONNECTED);

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
	aisIndicationOfMediaStateToHost(prAdapter, MEDIA_STATE_CONNECTED,
					FALSE, ucBssIndex);
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
				IN u_int8_t bRemove,
				IN uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct AIS_REQ_HDR *prPendingReqHdr, *prPendingReqHdrNext;
	u_int8_t found = FALSE;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	/* traverse through pending request list */
	LINK_FOR_EACH_ENTRY_SAFE(prPendingReqHdr,
				 prPendingReqHdrNext,
				 &(prAisFsmInfo->rPendingReqList), rLinkEntry,
				 struct AIS_REQ_HDR) {
		/* check for specified type */
		if (prPendingReqHdr->eReqType == eReqType) {
			found = TRUE;

			/* check if need to remove */
			if (bRemove == TRUE) {
				LINK_REMOVE_KNOWN_ENTRY(&(prAisFsmInfo->
					rPendingReqList),
					&(prPendingReqHdr->rLinkEntry));

				cnmMemFree(prAdapter, prPendingReqHdr);
				DBGLOG(AIS, INFO, "Remove req=%d\n", eReqType);
			} else
				break;
		}
	}

	return found;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Clear any pending request
 *
 * @param prAdapter
 *        eReqType
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t aisFsmClearRequest(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType,
			     IN uint8_t ucBssIndex)
{
	return aisFsmIsRequestPending(prAdapter, eReqType, TRUE, ucBssIndex);
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
struct AIS_REQ_HDR *aisFsmGetNextRequest(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct AIS_REQ_HDR *prPendingReqHdr;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

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
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType,
			     IN uint8_t ucBssIndex)
{
	struct AIS_REQ_HDR *prAisReq;
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	prAisReq =
	    (struct AIS_REQ_HDR *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					      sizeof(struct AIS_REQ_HDR));

	if (!prAisReq) {
		DBGLOG(AIS, ERROR, "Can't generate new message\n");
		return FALSE;
	}

	prAisReq->eReqType = eReqType;

	/* attach request into pending request list */
	LINK_INSERT_TAIL(&prAisFsmInfo->rPendingReqList, &prAisReq->rLinkEntry);

	DBGLOG(AIS, INFO, "eCurrentState=%d, eReqType=%d, u4NumElem=%d\n",
	       prAisFsmInfo->eCurrentState, eReqType,
	       prAisFsmInfo->rPendingReqList.u4NumElem);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Insert a new request to head
 *
 * @param prAdapter
 *        eReqType
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t aisFsmInsertRequestToHead(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType,
			     IN uint8_t ucBssIndex)
{
	struct AIS_REQ_HDR *prAisReq;
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	prAisReq =
	    (struct AIS_REQ_HDR *)cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					      sizeof(struct AIS_REQ_HDR));

	if (!prAisReq) {
		DBGLOG(AIS, ERROR, "Can't generate new message\n");
		return FALSE;
	}

	prAisReq->eReqType = eReqType;

	/* attach request into pending request list */
	LINK_INSERT_HEAD(&prAisFsmInfo->rPendingReqList, &prAisReq->rLinkEntry);

	DBGLOG(AIS, INFO, "eCurrentState=%d, eReqType=%d, u4NumElem=%d\n",
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
void aisFsmFlushRequest(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct AIS_REQ_HDR *prAisReq;
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	DBGLOG(AIS, INFO, "aisFsmFlushRequest %d\n",
		prAisFsmInfo->rPendingReqList.u4NumElem);

	while ((prAisReq = aisFsmGetNextRequest(prAdapter, ucBssIndex)) != NULL)
		cnmMemFree(prAdapter, prAisReq);
}

void aisFsmRunEventRemainOnChannel(IN struct ADAPTER *prAdapter,
				   IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_REMAIN_ON_CHANNEL *prRemainOnChannel;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	prRemainOnChannel = (struct MSG_REMAIN_ON_CHANNEL *)prMsgHdr;

	ucBssIndex = prRemainOnChannel->ucBssIdx;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);

	/* record parameters */
	prAisFsmInfo->rChReqInfo.eBand = prRemainOnChannel->eBand;
	prAisFsmInfo->rChReqInfo.eSco = prRemainOnChannel->eSco;
	prAisFsmInfo->rChReqInfo.ucChannelNum = prRemainOnChannel->ucChannelNum;
	prAisFsmInfo->rChReqInfo.u4DurationMs = prRemainOnChannel->u4DurationMs;
	prAisFsmInfo->rChReqInfo.u8Cookie = prRemainOnChannel->u8Cookie;
	prAisFsmInfo->rChReqInfo.eReqType = prRemainOnChannel->eReqType;

	if ((prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) ||
	    (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR
	     && !timerPendingTimer(&prAisFsmInfo->rJoinTimeoutTimer))) {
		/* transit to next state */
		aisFsmSteps(prAdapter, AIS_STATE_REQ_REMAIN_ON_CHANNEL,
			ucBssIndex);
	} else {
		aisFsmInsertRequest(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL,
			ucBssIndex);
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
	u_int8_t rReturn = TRUE;
	uint8_t ucBssIndex = 0;

	prCancelRemainOnChannel =
	    (struct MSG_CANCEL_REMAIN_ON_CHANNEL *)prMsgHdr;

	ucBssIndex = prCancelRemainOnChannel->ucBssIdx;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	/* 1. Check the cookie first */
	if (prCancelRemainOnChannel->u8Cookie ==
	    prAisFsmInfo->rChReqInfo.u8Cookie) {

		/* 2. release channel privilege/request */
		if (prAisFsmInfo->eCurrentState ==
		    AIS_STATE_REQ_REMAIN_ON_CHANNEL) {
			/* 2.1 elease channel */
			aisFsmReleaseCh(prAdapter, ucBssIndex);
		} else if (prAisFsmInfo->eCurrentState ==
			   AIS_STATE_REMAIN_ON_CHANNEL) {
			/* 2.1 release channel */
			aisFsmReleaseCh(prAdapter, ucBssIndex);

			/* 2.2 stop channel timeout timer */
			cnmTimerStopTimer(prAdapter,
					  &prAisFsmInfo->rChannelTimeoutTimer);
		}

		/* 3. clear pending request of remain_on_channel */
		rReturn = aisFsmClearRequest(prAdapter,
			AIS_REQUEST_REMAIN_ON_CHANNEL, ucBssIndex);

		DBGLOG(AIS, TRACE,
			"rReturn of aisFsmIsRequestPending is %d", rReturn);

		/* 4. decide which state to retreat */
		if (prAisFsmInfo->eCurrentState ==
		    AIS_STATE_REQ_REMAIN_ON_CHANNEL
		    || prAisFsmInfo->eCurrentState ==
		    AIS_STATE_REMAIN_ON_CHANNEL) {
			if (prAisBssInfo->eConnectionState ==
			    MEDIA_STATE_CONNECTED)
				aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR,
					ucBssIndex);
			else
				aisFsmSteps(prAdapter, AIS_STATE_IDLE,
					ucBssIndex);
		}
	}

	/* 5. free message */
	cnmMemFree(prAdapter, prMsgHdr);
}

static u_int8_t
aisFunChnlReqByOffChnl(IN struct ADAPTER *prAdapter,
		IN struct AIS_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq,
		IN uint8_t ucBssIndex)
{
	struct MSG_REMAIN_ON_CHANNEL *prMsgChnlReq =
			(struct MSG_REMAIN_ON_CHANNEL *) NULL;

	prMsgChnlReq = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			sizeof(struct MSG_REMAIN_ON_CHANNEL));
	if (prMsgChnlReq == NULL) {
		DBGLOG(AIS, ERROR, "channel request buffer allocate fails.\n");
		return FALSE;
	}

	prMsgChnlReq->u8Cookie = prOffChnlTxReq->u8Cookie;
	prMsgChnlReq->u4DurationMs = prOffChnlTxReq->u4Duration;
	prMsgChnlReq->ucChannelNum = prOffChnlTxReq->rChannelInfo.ucChannelNum;
	prMsgChnlReq->eBand = prOffChnlTxReq->rChannelInfo.eBand;
	prMsgChnlReq->eSco = prOffChnlTxReq->eChnlExt;
	prMsgChnlReq->eReqType = CH_REQ_TYPE_OFFCHNL_TX;

	prMsgChnlReq->ucBssIdx = ucBssIndex;

	aisFsmRunEventRemainOnChannel(prAdapter,
			(struct MSG_HDR *) prMsgChnlReq);
	return TRUE;
}

static u_int8_t
aisFunAddTxReq2Queue(IN struct ADAPTER *prAdapter,
		IN struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
		OUT struct AIS_OFF_CHNL_TX_REQ_INFO **pprOffChnlTxReq)
{
	struct AIS_OFF_CHNL_TX_REQ_INFO *prTmpOffChnlTxReq =
			(struct AIS_OFF_CHNL_TX_REQ_INFO *) NULL;

	prTmpOffChnlTxReq = cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG,
			sizeof(struct AIS_OFF_CHNL_TX_REQ_INFO));

	if (prTmpOffChnlTxReq == NULL) {
		DBGLOG(AIS, ERROR, "Allocate TX request buffer fails.\n");
		return FALSE;
	}

	prTmpOffChnlTxReq->u8Cookie = prMgmtTxMsg->u8Cookie;
	prTmpOffChnlTxReq->prMgmtTxMsdu = prMgmtTxMsg->prMgmtMsduInfo;
	prTmpOffChnlTxReq->fgNoneCckRate = prMgmtTxMsg->fgNoneCckRate;
	kalMemCopy(&prTmpOffChnlTxReq->rChannelInfo,
			&prMgmtTxMsg->rChannelInfo,
			sizeof(struct RF_CHANNEL_INFO));
	prTmpOffChnlTxReq->eChnlExt = prMgmtTxMsg->eChnlExt;
	prTmpOffChnlTxReq->fgIsWaitRsp = prMgmtTxMsg->fgIsWaitRsp;
	prTmpOffChnlTxReq->u4Duration = prMgmtTxMsg->u4Duration;

	LINK_INSERT_TAIL(&prMgmtTxReqInfo->rTxReqLink,
			&prTmpOffChnlTxReq->rLinkEntry);

	*pprOffChnlTxReq = prTmpOffChnlTxReq;

	return TRUE;
}

static void
aisFunHandleOffchnlTxReq(IN struct ADAPTER *prAdapter,
		IN struct AIS_FSM_INFO *prAisFsmInfo,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
		IN uint8_t ucBssIndex)
{
	struct AIS_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq =
			(struct AIS_OFF_CHNL_TX_REQ_INFO *) NULL;
	struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo =
			(struct AIS_MGMT_TX_REQ_INFO *) NULL;

	prMgmtTxReqInfo = &(prAisFsmInfo->rMgmtTxInfo);

	if (prMgmtTxMsg->u4Duration < MIN_TX_DURATION_TIME_MS)
		prMgmtTxMsg->u4Duration = MIN_TX_DURATION_TIME_MS;

	if (aisFunAddTxReq2Queue(prAdapter, prMgmtTxReqInfo,
			prMgmtTxMsg, &prOffChnlTxReq) == FALSE)
		goto error;

	if (prOffChnlTxReq == NULL)
		return;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_OFF_CHNL_TX:
		if (prAisFsmInfo->fgIsChannelGranted &&
				prAisFsmInfo->rChReqInfo.ucChannelNum ==
				prMgmtTxMsg->rChannelInfo.ucChannelNum &&
				prMgmtTxReqInfo->rTxReqLink.u4NumElem == 1) {
			aisFsmSteps(prAdapter, AIS_STATE_OFF_CHNL_TX,
				ucBssIndex);
		} else {
			log_dbg(P2P, INFO, "tx ch: %d, current ch: %d, granted: %d, tx link num: %d",
				prMgmtTxMsg->rChannelInfo.ucChannelNum,
				prAisFsmInfo->rChReqInfo.ucChannelNum,
				prAisFsmInfo->fgIsChannelGranted,
				prMgmtTxReqInfo->rTxReqLink.u4NumElem);
		}
		break;
	default:
		if (!aisFunChnlReqByOffChnl(prAdapter, prOffChnlTxReq,
			ucBssIndex))
			goto error;
		break;
	}

	return;

error:
	LINK_REMOVE_KNOWN_ENTRY(
			&(prMgmtTxReqInfo->rTxReqLink),
			&prOffChnlTxReq->rLinkEntry);
	cnmPktFree(prAdapter, prOffChnlTxReq->prMgmtTxMsdu);
	cnmMemFree(prAdapter, prOffChnlTxReq);
}

static u_int8_t
aisFunNeedOffchnlTx(IN struct ADAPTER *prAdapter,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	struct BSS_INFO *prAisBssInfo = (struct BSS_INFO *) NULL;
	struct AIS_FSM_INFO *prAisFsmInfo;
	uint8_t ucBssIndex = 0;

	ucBssIndex = prMgmtTxMsg->ucBssIdx;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	if (!prMgmtTxMsg->fgIsOffChannel)
		return FALSE;

	/* tx channel == op channel */
	if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED &&
			prAisBssInfo->ucPrimaryChannel ==
				prMgmtTxMsg->rChannelInfo.ucChannelNum)
		return FALSE;

	/* tx channel == roc channel */
	if (prAisFsmInfo->fgIsChannelGranted &&
			prAisFsmInfo->rChReqInfo.ucChannelNum ==
			prMgmtTxMsg->rChannelInfo.ucChannelNum)
		return FALSE;

	DBGLOG(REQ, INFO, "Use offchannel to TX.\n");

	return TRUE;
}

void aisFsmRunEventMgmtFrameTx(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
			(struct MSG_MGMT_TX_REQUEST *) NULL;
	uint8_t ucBssIndex = 0;

	if (!prAdapter || !prMsgHdr)
		return;

	prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *) prMsgHdr;
	ucBssIndex = prMgmtTxMsg->ucBssIdx;
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	if (prAisFsmInfo == NULL)
		goto exit;

	if (!aisFunNeedOffchnlTx(prAdapter, prMgmtTxMsg))
		aisFuncTxMgmtFrame(prAdapter,
				&prAisFsmInfo->rMgmtTxInfo,
				prMgmtTxMsg->prMgmtMsduInfo,
				prMgmtTxMsg->u8Cookie,
				ucBssIndex);
	else
		aisFunHandleOffchnlTxReq(prAdapter,
				prAisFsmInfo,
				prMgmtTxMsg,
				ucBssIndex);

exit:
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
	struct NCHO_INFO *prNchoInfo = NULL;
	uint16_t u2PktLen = 0;
	uint8_t ucBssIndex = 0;

	do {
		prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *)prMsgHdr;

		ucBssIndex = prMgmtTxMsg->ucBssIdx;

		prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
		prNchoInfo = &(prAdapter->rNchoInfo);
		prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

		if (prAisFsmInfo == NULL)
			break;

		u2PktLen =
		    (uint16_t) OFFSET_OF(struct _ACTION_VENDOR_SPEC_FRAME_T,
					 aucElemInfo[0]) +
		    prNchoInfo->rParamActionFrame.i4len + MAC_TX_RESERVED_FIELD;
		prMgmtFrame = cnmMgtPktAlloc(prAdapter, u2PktLen);
		if (prMgmtFrame == NULL) {
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
				   prMgmtTxMsg->u8Cookie,
				   ucBssIndex);

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
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter, ucBssIndex);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter,
				  &prAisFsmInfo->rChannelTimeoutTimer);

		/* 3. expiration indication to upper layer */
		kalRemainOnChannelExpired(prAdapter->prGlueInfo,
					  prAisFsmInfo->rChReqInfo.u8Cookie,
					  prAisFsmInfo->rChReqInfo.eBand,
					  prAisFsmInfo->rChReqInfo.eSco,
					  prAisFsmInfo->rChReqInfo.ucChannelNum,
					  ucBssIndex);

		/* 4. decide which state to retreat */
		if (prAisBssInfo->eConnectionState ==
		    MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR,
				ucBssIndex);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE,
				ucBssIndex);

	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_OFF_CHNL_TX) {
		aisFsmSteps(prAdapter, AIS_STATE_OFF_CHNL_TX,
			ucBssIndex);
	} else {
		DBGLOG(AIS, WARN,
		       "Unexpected remain_on_channel timeout event\n");
		DBGLOG(AIS, STATE, "CURRENT State: [%s]\n",
			aisGetFsmState(prAisFsmInfo->eCurrentState));
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
	uint64_t *pu8GlCookie = (uint64_t *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		ucBssIndex = prMsduInfo->ucBssIndex;

		prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
		prMgmtTxReqInfo = &(prAisFsmInfo->rMgmtTxInfo);
		pu8GlCookie =
			(uint64_t *) ((unsigned long) prMsduInfo->prPacket +
				(unsigned long) prMsduInfo->u2FrameLength +
				MAC_TX_RESERVED_FIELD);

		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(AIS, ERROR, "Mgmt Frame TX Fail, Status:%d.\n",
			       rTxDoneStatus);
		} else {
			fgIsSuccess = TRUE;
			DBGLOG(AIS, INFO,
				"Mgmt Frame TX Success, cookie: 0x%llx.\n",
				*pu8GlCookie);
#if CFG_SUPPORT_NCHO
			if (prAdapter->rNchoInfo.fgNCHOEnabled == TRUE &&
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
						prMsduInfo->u2FrameLength,
						ucBssIndex);

			prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
		}

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* aisFsmRunEventMgmtFrameTxDone */

uint32_t
aisFuncTxMgmtFrame(IN struct ADAPTER *prAdapter,
		   IN struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo,
		   IN struct MSDU_INFO *prMgmtTxMsdu, IN uint64_t u8Cookie,
		   IN uint8_t ucBssIndex)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct MSDU_INFO *prTxMsduInfo = (struct MSDU_INFO *)NULL;
	struct WLAN_MAC_HEADER *prWlanHdr = (struct WLAN_MAC_HEADER *)NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *)NULL;
	uint32_t ucStaRecIdx = STA_REC_INDEX_NOT_FOUND;

	do {
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
					  prTxMsduInfo->u2FrameLength,
					  ucBssIndex);

				/* Leave it to TX Done handler. */
				/* cnmMgtPktFree(prAdapter, prTxMsduInfo); */
				prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
			}
			/* 2. prMgmtTxReqInfo->prMgmtTxMsdu == NULL */
			/* Packet transmitted, wait tx done. (cookie issue) */
		}

		prWlanHdr =
		    (struct WLAN_MAC_HEADER *)((unsigned long)
					       prMgmtTxMsdu->prPacket +
					       MAC_TX_RESERVED_FIELD);
		prStaRec =
		    cnmGetStaRecByAddress(prAdapter,
					  ucBssIndex,
					  prWlanHdr->aucAddr1);

		if (IS_BMCAST_MAC_ADDR(prWlanHdr->aucAddr1))
			ucStaRecIdx = STA_REC_INDEX_BMCAST;

		if (prStaRec)
			ucStaRecIdx = prStaRec->ucIndex;

		TX_SET_MMPDU(prAdapter,
			     prMgmtTxMsdu,
			     (prStaRec !=
			      NULL) ? (prStaRec->
				       ucBssIndex)
			     : (ucBssIndex),
			     ucStaRecIdx,
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
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("aisFuncValidateRxActionFrame");

	do {
		if (prSwRfb->prStaRec)
			ucBssIndex = prSwRfb->prStaRec->ucBssIndex;

		prAisFsmInfo
			= aisGetAisFsmInfo(prAdapter, ucBssIndex);

		if (1
		/* prAisFsmInfo->u4AisPacketFilter &
		 * PARAM_PACKET_FILTER_ACTION_FRAME
		 */
		) {
			/* Leave the action frame to wpa_supplicant. */
			kalIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb,
				ucBssIndex);
		}

	} while (FALSE);

	return;

}				/* aisFuncValidateRxActionFrame */

/* Support AP Selection */
void aisRefreshFWKBlacklist(struct ADAPTER *prAdapter)
{
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct LINK *prBlackList = &prAdapter->rWifiVar.rBlackList.rUsingLink;

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
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct LINK_MGMT *prBlackList = &prAdapter->rWifiVar.rBlackList;

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
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);
	if (!prEntry)
		return;
	LINK_MGMT_RETURN_ENTRY(&prAdapter->rWifiVar.rBlackList, prEntry);
	prBssDesc->prBlack = NULL;
	DBGLOG(AIS, INFO, "Remove " MACSTR " from blacklist\n",
	       MAC2STR(prBssDesc->aucBSSID));
}

struct AIS_BLACKLIST_ITEM *aisQueryBlackList(struct ADAPTER *prAdapter,
					     struct BSS_DESC *prBssDesc)
{
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct LINK *prBlackList = &prAdapter->rWifiVar.rBlackList.rUsingLink;

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
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	struct LINK *prBlackList = &prAdapter->rWifiVar.rBlackList.rUsingLink;
	OS_SYSTIME rCurrent;
	struct BSS_DESC *prBssDesc = NULL;

	GET_CURRENT_SYSTIME(&rCurrent);

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry,
				 struct AIS_BLACKLIST_ITEM) {
		if (prEntry->fgIsInFWKBlacklist ||
		    !CHECK_FOR_TIMEOUT(rCurrent, prEntry->rAddTime,
				       SEC_TO_MSEC(AIS_BLACKLIST_TIMEOUT)))
			continue;
		prBssDesc = scanSearchBssDescByBssid(prAdapter,
						     prEntry->aucBSSID);
		if (prBssDesc) {
			prBssDesc->prBlack = NULL;
			prBssDesc->ucJoinFailureCount = 0;
			DBGLOG(AIS, INFO,
				"Remove Timeout "MACSTR" from blacklist\n",
			       MAC2STR(prBssDesc->aucBSSID));
		}
		LINK_MGMT_RETURN_ENTRY(&prAdapter->rWifiVar.rBlackList,
			prEntry);
	}
}

static void aisRemoveDeauthBlacklist(struct ADAPTER *prAdapter)
{
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	struct LINK *prBlackList = &prAdapter->rWifiVar.rBlackList.rUsingLink;
	struct BSS_DESC *prBssDesc = NULL;

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry,
				 struct AIS_BLACKLIST_ITEM) {
		if (prEntry->fgIsInFWKBlacklist ||
		    !prEntry->fgDeauthLastTime)
			continue;

		prBssDesc = scanSearchBssDescByBssid(prAdapter,
						     prEntry->aucBSSID);
		if (prBssDesc) {
			prBssDesc->prBlack = NULL;
			prBssDesc->ucJoinFailureCount = 0;
			DBGLOG(AIS, INFO,
			       "Remove deauth "MACSTR" from blacklist\n",
			       MAC2STR(prBssDesc->aucBSSID));
		}
		LINK_MGMT_RETURN_ENTRY(&prAdapter->rWifiVar.rBlackList,
			prEntry);
	}
}

void aisFsmRunEventBssTransition(IN struct ADAPTER *prAdapter,
				 IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_AIS_BSS_TRANSITION_T *prMsg =
	    (struct MSG_AIS_BSS_TRANSITION_T *)prMsgHdr;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct BSS_TRANSITION_MGT_PARAM_T *prBtmParam;
	enum WNM_AIS_BSS_TRANSITION eTransType = BSS_TRANSITION_MAX_NUM;
	struct BSS_DESC *prBssDesc;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	u_int8_t fgNeedBtmResponse = FALSE;
	uint8_t ucStatus = BSS_TRANSITION_MGT_STATUS_UNSPECIFIED;
	uint8_t ucRcvToken = 0;
	static uint8_t aucChnlList[MAXIMUM_OPERATION_CHANNEL_LIST];
	uint8_t ucBssIndex = 0;

	if (!prMsg) {
		DBGLOG(AIS, WARN, "Msg Header is NULL\n");
		return;
	}

	ucBssIndex = prMsg->ucBssIndex;

	prAisSpecificBssInfo =
	    aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prBtmParam =
	    &prAisSpecificBssInfo->rBTMParam;
	prBssDesc =
	    aisGetTargetBssDesc(prAdapter, ucBssIndex);
	prRoamingFsmInfo =
	    aisGetRoamingInfo(prAdapter, ucBssIndex);
	eTransType = prMsg->eTransitionType;
	fgNeedBtmResponse = prMsg->fgNeedResponse;
	ucRcvToken = prMsg->ucToken;

	DBGLOG(AIS, INFO, "Transition Type: %d\n", eTransType);
#if CFG_SUPPORT_802_11K
	aisCollectNeighborAP(prAdapter, prMsg->pucCandList,
			     prMsg->u2CandListLen, prMsg->ucValidityInterval,
			     ucBssIndex);
#endif
	cnmMemFree(prAdapter, prMsgHdr);
	/* Solicited BTM request: the case we're waiting btm request
	 ** after send btm query before roaming scan
	 */
	if (prBtmParam->ucDialogToken == ucRcvToken) {
		prBtmParam->fgPendingResponse = fgNeedBtmResponse;
		prBtmParam->fgUnsolicitedReq = FALSE;

		switch (prRoamingFsmInfo->eCurrentState) {
		case ROAMING_STATE_REQ_CAND_LIST:
			roamingFsmSteps(prAdapter, ROAMING_STATE_DISCOVERY,
				ucBssIndex);
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
		roamingFsmSteps(prAdapter, ROAMING_STATE_DISCOVERY,
			ucBssIndex);
		return;
	}
	default:
		ucStatus = BSS_TRANSITION_MGT_STATUS_ACCEPT;
		break;
	}
send_response:
	if (fgNeedBtmResponse &&
		aisGetAisBssInfo(prAdapter, ucBssIndex) &&
		aisGetStaRecOfAP(prAdapter, ucBssIndex)) {
		prBtmParam->ucStatusCode = ucStatus;
		prBtmParam->ucTermDelay = 0;
		kalMemZero(prBtmParam->aucTargetBssid, MAC_ADDR_LEN);
		prBtmParam->u2OurNeighborBssLen = 0;
		prBtmParam->fgPendingResponse = FALSE;
		wnmSendBTMResponseFrame(prAdapter,
			aisGetStaRecOfAP(prAdapter, ucBssIndex));
	}
}

#if CFG_SUPPORT_802_11K
void aisSendNeighborRequest(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct SUB_ELEMENT_LIST *prSSIDIE;
	uint8_t aucBuffer[sizeof(*prSSIDIE) + 31];
	struct BSS_INFO *prBssInfo
		= aisGetAisBssInfo(prAdapter, ucBssIndex);

	kalMemZero(aucBuffer, sizeof(aucBuffer));
	prSSIDIE = (struct SUB_ELEMENT_LIST *)&aucBuffer[0];
	prSSIDIE->rSubIE.ucSubID = ELEM_ID_SSID;
	COPY_SSID(&prSSIDIE->rSubIE.aucOptInfo[0], prSSIDIE->rSubIE.ucLength,
		  prBssInfo->aucSSID, prBssInfo->ucSSIDLen);
	rrmTxNeighborReportRequest(prAdapter, prBssInfo->prStaRecOfAP,
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
			  uint16_t u2ApBufLen, uint8_t ucValidInterval,
			  uint8_t ucBssIndex)
{
	struct NEIGHBOR_AP_T *prNeighborAP = NULL;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo =
	    aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	struct LINK_MGMT *prAPlist = &prAisSpecBssInfo->rNeighborApList;
	struct IE_NEIGHBOR_REPORT *prIe = (struct IE_NEIGHBOR_REPORT *)pucApBuf;
	int16_t c2BufLen;
	uint16_t u2PrefIsZeroCount = 0;

	if (!prIe || !u2ApBufLen || u2ApBufLen < prIe->ucLength)
		return;
	LINK_MERGE_TO_TAIL(&prAPlist->rFreeLink, &prAPlist->rUsingLink);
	for (c2BufLen = u2ApBufLen; c2BufLen > 0; c2BufLen -= IE_SIZE(prIe),
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

		if (c2BufLen < IE_SIZE(prIe)) {
			DBGLOG(AIS, WARN, "Truncated neighbor report\n");
			break;
		}
	}
	prAisSpecBssInfo->rNeiApRcvTime = kalGetTimeTick();
	prAisSpecBssInfo->u4NeiApValidInterval =
	    !ucValidInterval
	    ? 0xffffffff
	    : TU_TO_MSEC(ucValidInterval *
			 aisGetAisBssInfo(prAdapter, ucBssIndex)
			 ->u2BeaconInterval);

	if (prAPlist->rUsingLink.u4NumElem > 0 &&
	    prAPlist->rUsingLink.u4NumElem == u2PrefIsZeroCount)
		DBGLOG(AIS, INFO,
		       "The number of valid neighbors is equal to the number of perf value is 0.\n");
}

void aisResetNeighborApList(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo =
	    aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	struct LINK_MGMT *prAPlist = &prAisSpecBssInfo->rNeighborApList;

	LINK_MERGE_TO_TAIL(&prAPlist->rFreeLink, &prAPlist->rUsingLink);
}
#endif

void aisFsmRunEventCancelTxWait(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct AIS_FSM_INFO *prAisFsmInfo =
			(struct AIS_FSM_INFO *) NULL;
	struct MSG_CANCEL_TX_WAIT_REQUEST *prCancelTxWaitMsg =
			(struct MSG_CANCEL_TX_WAIT_REQUEST *) NULL;
	struct BSS_INFO *prAisBssInfo = (struct BSS_INFO *) NULL;
	struct AIS_MGMT_TX_REQ_INFO *prMgmtTxInfo =
			(struct AIS_MGMT_TX_REQ_INFO *) NULL;
	struct AIS_OFF_CHNL_TX_REQ_INFO *prOffChnlTxPkt =
			(struct AIS_OFF_CHNL_TX_REQ_INFO *) NULL;
	u_int8_t fgIsCookieFound = FALSE;
	uint8_t ucBssIndex = 0;

	if (prAdapter == NULL || prMsgHdr == NULL)
		goto exit;

	prCancelTxWaitMsg = (struct MSG_CANCEL_TX_WAIT_REQUEST *) prMsgHdr;

	ucBssIndex = prCancelTxWaitMsg->ucBssIdx;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prMgmtTxInfo = &prAisFsmInfo->rMgmtTxInfo;

	if (prAisFsmInfo == NULL ||
		prAisBssInfo == NULL || prMgmtTxInfo == NULL)
		goto exit;

	LINK_FOR_EACH_ENTRY(prOffChnlTxPkt,
			&(prMgmtTxInfo->rTxReqLink),
			rLinkEntry,
			struct AIS_OFF_CHNL_TX_REQ_INFO) {
		if (!prOffChnlTxPkt)
			break;
		if (prOffChnlTxPkt->u8Cookie == prCancelTxWaitMsg->u8Cookie) {
			fgIsCookieFound = TRUE;
			break;
		}
	}

	if (fgIsCookieFound == FALSE && prAisFsmInfo->eCurrentState !=
			AIS_STATE_OFF_CHNL_TX)
		goto exit;

	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
	aisFunClearAllTxReq(prAdapter, &(prAisFsmInfo->rMgmtTxInfo));
	aisFsmReleaseCh(prAdapter, ucBssIndex);

	if (prAisBssInfo->eConnectionState ==
			MEDIA_STATE_CONNECTED)
		aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR, ucBssIndex);
	else
		aisFsmSteps(prAdapter, AIS_STATE_IDLE, ucBssIndex);

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

static void
aisFunClearAllTxReq(IN struct ADAPTER *prAdapter,
		IN struct AIS_MGMT_TX_REQ_INFO *prAisMgmtTxInfo)
{
	struct AIS_OFF_CHNL_TX_REQ_INFO *prOffChnlTxPkt =
			(struct AIS_OFF_CHNL_TX_REQ_INFO *) NULL;

	while (!LINK_IS_EMPTY(&(prAisMgmtTxInfo->rTxReqLink))) {
		LINK_REMOVE_HEAD(&(prAisMgmtTxInfo->rTxReqLink),
				prOffChnlTxPkt,
				struct AIS_OFF_CHNL_TX_REQ_INFO *);
		if (!prOffChnlTxPkt)
			continue;
		kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
			prOffChnlTxPkt->u8Cookie,
			FALSE,
			prOffChnlTxPkt->prMgmtTxMsdu->prPacket,
			(uint32_t) prOffChnlTxPkt->prMgmtTxMsdu->u2FrameLength,
			prOffChnlTxPkt->prMgmtTxMsdu->ucBssIndex);
		cnmPktFree(prAdapter, prOffChnlTxPkt->prMgmtTxMsdu);
		cnmMemFree(prAdapter, prOffChnlTxPkt);
	}
}

struct AIS_FSM_INFO *aisGetAisFsmInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rAisFsmInfo[ucBssIndex]);
}

struct AIS_SPECIFIC_BSS_INFO *aisGetAisSpecBssInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rAisSpecificBssInfo[ucBssIndex]);
}

struct BSS_TRANSITION_MGT_PARAM_T *
	aisGetBTMParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return	&prAdapter->rWifiVar.
		rAisSpecificBssInfo[ucBssIndex].rBTMParam;
}

struct BSS_INFO *aisGetConnectedBssInfo(
	IN struct ADAPTER *prAdapter) {

	struct BSS_INFO *prBssInfo;
	uint8_t i;

	if (!prAdapter)
		return NULL;

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo &&
			IS_BSS_AIS(prBssInfo) &&
			kalGetMediaStateIndicated(
			prAdapter->prGlueInfo,
			prBssInfo->ucBssIndex) ==
			MEDIA_STATE_CONNECTED) {
			return prBssInfo;
		}
	}

	return NULL;
}

struct BSS_INFO *aisGetAisBssInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->aprBssInfo[ucBssIndex];
}

struct STA_RECORD *aisGetStaRecOfAP(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	if (prAdapter->aprBssInfo[ucBssIndex])
		return prAdapter->aprBssInfo[ucBssIndex]->prStaRecOfAP;

	DBGLOG(AIS, WARN, "prStaRecOfAP is Null\n");
	return NULL;
}

struct BSS_DESC *aisGetTargetBssDesc(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar
		.rAisFsmInfo[ucBssIndex].prTargetBssDesc;
}

struct STA_RECORD *aisGetTargetStaRec(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar.rAisFsmInfo[ucBssIndex].prTargetStaRec;
}

uint8_t aisGetTargetBssDescChannel(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar
		.rAisFsmInfo[ucBssIndex].prTargetBssDesc->ucChannelNum;
}

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
struct TIMER *aisGetSecModeChangeTimer(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return
		&prAdapter->rWifiVar
		.rAisFsmInfo[ucBssIndex].rSecModeChangeTimer;
}
#endif

struct TIMER *aisGetScanDoneTimer(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return
		&prAdapter->rWifiVar
		.rAisFsmInfo[ucBssIndex].rScanDoneTimer;
}

enum ENUM_AIS_STATE aisGetCurrState(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return
		prAdapter->rWifiVar
		.rAisFsmInfo[ucBssIndex].eCurrentState;
}

struct CONNECTION_SETTINGS *
	aisGetConnSettings(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rConnSettings[ucBssIndex]);
}

struct GL_WPA_INFO *aisGetWpaInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &prAdapter->prGlueInfo->rWpaInfo[ucBssIndex];
}

u_int8_t aisGetWapiMode(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar.rConnSettings[ucBssIndex].fgWapiMode;
}

enum ENUM_PARAM_AUTH_MODE aisGetAuthMode(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar.rConnSettings[ucBssIndex].eAuthMode;
}

enum ENUM_PARAM_OP_MODE aisGetOPMode(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar.rConnSettings[ucBssIndex].eOPMode;
}

enum ENUM_WEP_STATUS aisGetEncStatus(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return prAdapter->rWifiVar.rConnSettings[ucBssIndex].eEncStatus;
}

struct IEEE_802_11_MIB *aisGetMib(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &prAdapter->rMib[ucBssIndex];
}

struct ROAMING_INFO *aisGetRoamingInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rRoamingInfo[ucBssIndex]);
}

struct PARAM_BSSID_EX *aisGetCurrBssId(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWlanInfo.rCurrBssId[ucBssIndex]);
}

#if CFG_SUPPORT_PASSPOINT
struct HS20_INFO *aisGetHS20Info(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rHS20Info[ucBssIndex]);
}
#endif

struct RADIO_MEASUREMENT_REQ_PARAMS *aisGetRmReqParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rRmReqParams[ucBssIndex]);
}

struct RADIO_MEASUREMENT_REPORT_PARAMS *
	aisGetRmReportParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rRmRepParams[ucBssIndex]);
}

struct WMM_INFO *
	aisGetWMMInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rWmmInfo[ucBssIndex]);
}

#ifdef CFG_SUPPORT_REPLAY_DETECTION
struct GL_DETECT_REPLAY_INFO *
	aisGetDetRplyInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->prGlueInfo->prDetRplyInfo[ucBssIndex]);
}
#endif

struct FT_IES *
	aisGetFtIe(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rConnSettings[ucBssIndex].rFtIeForTx);
}

struct cfg80211_ft_event_params *
	aisGetFtEventParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex) {

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex)) {
		DBGLOG(AIS, LOUD,
			"Use default, invalid index = %d\n", ucBssIndex);
		ucBssIndex = AIS_DEFAULT_INDEX;
	}

	return &(prAdapter->rWifiVar.rConnSettings[ucBssIndex].rFtEventParam);
}

uint8_t *
	aisGetFsmState(
	IN enum ENUM_AIS_STATE eCurrentState) {
	if (eCurrentState >= 0 && eCurrentState < AIS_STATE_NUM)
		return apucDebugAisState[eCurrentState];

	ASSERT(0);
	return (uint8_t *) NULL;
}
