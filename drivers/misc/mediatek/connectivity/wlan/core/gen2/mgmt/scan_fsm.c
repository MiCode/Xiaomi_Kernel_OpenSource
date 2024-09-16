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
static PUINT_8 apucDebugScanState[SCAN_STATE_NUM] = {
	(PUINT_8) DISP_STRING("SCAN_STATE_IDLE"),
	(PUINT_8) DISP_STRING("SCAN_STATE_SCANNING"),
};

/*lint -restore */
#endif /* DBG */

#define CURRENT_PSCN_VERSION 1
#define RSSI_MARGIN_DEFAULT  5
#define MAX_PERIOD 200000

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
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_SCAN_STATE_T eNextState)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	P_MSG_HDR_T prMsgHdr;

	BOOLEAN fgIsTransition = (BOOLEAN) FALSE;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	do {

#if DBG
		DBGLOG(SCN, STATE, "TRANSITION: [%s] -> [%s]\n",
				    apucDebugScanState[prScanInfo->eCurrentState], apucDebugScanState[eNextState]);
#else
		DBGLOG(SCN, STATE, "[%d] TRANSITION: [%d] -> [%d]\n",
				    DBG_SCN_IDX, prScanInfo->eCurrentState, eNextState);
#endif

		/* NOTE(Kevin): This is the only place to change the eCurrentState(except initial) */
		prScanInfo->eCurrentState = eNextState;

		fgIsTransition = (BOOLEAN) FALSE;

		switch (prScanInfo->eCurrentState) {
		case SCAN_STATE_IDLE:
			/* check for pending scanning requests */
			if (!cnmChUtilIsRunning(prAdapter) && !LINK_IS_EMPTY(&(prScanInfo->rPendingMsgList))) {
				/* load next message from pending list as scan parameters */
				LINK_REMOVE_HEAD(&(prScanInfo->rPendingMsgList), prMsgHdr, P_MSG_HDR_T);

				if (prMsgHdr->eMsgId == MID_AIS_SCN_SCAN_REQ
				    || prMsgHdr->eMsgId == MID_BOW_SCN_SCAN_REQ
				    || prMsgHdr->eMsgId == MID_P2P_SCN_SCAN_REQ
				    || prMsgHdr->eMsgId == MID_RLM_SCN_SCAN_REQ) {
					scnFsmHandleScanMsg(prAdapter, (P_MSG_SCN_SCAN_REQ) prMsgHdr);
				} else {
					scnFsmHandleScanMsgV2(prAdapter, (P_MSG_SCN_SCAN_REQ_V2) prMsgHdr);
				}

				/* switch to next state */
				eNextState = SCAN_STATE_SCANNING;
				fgIsTransition = TRUE;

				cnmMemFree(prAdapter, prMsgHdr);
			}
			break;

		case SCAN_STATE_SCANNING:
			prScanInfo->u4ScanUpdateIdx++;
			if (prScanParam->fgIsScanV2 == FALSE)
				scnSendScanReq(prAdapter);
			else
				scnSendScanReqV2(prAdapter);
			break;

		default:
			ASSERT(0);
			break;

		}
	} while (fgIsTransition);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        Generate CMD_ID_SCAN_REQ command
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnSendScanReqExtCh(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	/*CMD_SCAN_REQ_EXT_CH rCmdScanReq;*/
	P_CMD_SCAN_REQ_EXT_CH prCmdScanReq;
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	prCmdScanReq = kalMemAlloc(sizeof(CMD_SCAN_REQ_EXT_CH), VIR_MEM_TYPE);
	if (prCmdScanReq == NULL) {
		DBGLOG(SCN, ERROR, "alloc CmdScanReq V1EXT fail");
		return;
	}

	/* send command packet for scan */
	kalMemZero(prCmdScanReq, sizeof(CMD_SCAN_REQ_EXT_CH));

	prCmdScanReq->ucSeqNum = prScanParam->ucSeqNum;
	prCmdScanReq->ucNetworkType = (UINT_8) prScanParam->eNetTypeIndex;
	prCmdScanReq->ucScanType = (UINT_8) prScanParam->eScanType;
	prCmdScanReq->ucSSIDType = prScanParam->ucSSIDType;

	if (prScanParam->ucSSIDNum == 1) {
		COPY_SSID(prCmdScanReq->aucSSID,
			  prCmdScanReq->ucSSIDLength,
			  prScanParam->aucSpecifiedSSID[0], prScanParam->ucSpecifiedSSIDLen[0]);
	}

	prCmdScanReq->ucChannelType = (UINT_8) prScanParam->eScanChannel;

	if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
		/* P2P would use:
		 * 1. Specified Listen Channel of passive scan for LISTEN state.
		 * 2. Specified Listen Channel of Target Device of active scan for SEARCH state. (Target != NULL)
		 */
		prCmdScanReq->ucChannelListNum = prScanParam->ucChannelListNum;

		for (i = 0; i < prCmdScanReq->ucChannelListNum; i++) {
			prCmdScanReq->arChannelList[i].ucBand = (UINT_8) prScanParam->arChnlInfoList[i].eBand;

			prCmdScanReq->arChannelList[i].ucChannelNum =
			    (UINT_8) prScanParam->arChnlInfoList[i].ucChannelNum;
		}
	}
#if CFG_ENABLE_WIFI_DIRECT
	if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
		P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		if (prP2pBssInfo == NULL) {
			DBGLOG(P2P, ERROR, "scnSendScanReqExtCh prP2pBssInfo is NULL\n");
			return;
		}
#if CFG_TC10_FEATURE
		/*
		 * mtk supplicant will scan 4 channels for prograssive scan
		 * customer supplicant should have 3 channels when do social scan
		 */
		if (prScanParam->ucChannelListNum <= 4) {
			DBGLOG(P2P, INFO, "scnSendScanReqExtCh Channel number %d for 70ms, OP_MODE[%d]\n",
				prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
			prCmdScanReq->u2ChannelDwellTime = 70;
		} else if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			DBGLOG(P2P, INFO, "scnSendScanReqExtCh Channel number %d for 100ms, OP_MODE[%d]\n",
				prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
			prCmdScanReq->u2ChannelDwellTime = 100;
		} else
			prCmdScanReq->u2ChannelDwellTime = 30;
#else
		prCmdScanReq->u2ChannelDwellTime = prScanParam->u2PassiveListenInterval;
#endif
	}
#endif

	if (prScanParam->u2IELen <= MAX_IE_LENGTH)
		prCmdScanReq->u2IELen = prScanParam->u2IELen;
	else
		prCmdScanReq->u2IELen = MAX_IE_LENGTH;

	if (prScanParam->u2IELen)
		kalMemCopy(prCmdScanReq->aucIE, prScanParam->aucIE, sizeof(UINT_8) * prCmdScanReq->u2IELen);

	DBGLOG(SCN, INFO, "ScanReqV1EXT: ScanType=%d, SSIDType=%d, Num=%d, ChannelType=%d, Num=%d",
		prCmdScanReq->ucScanType, prCmdScanReq->ucSSIDType, prScanParam->ucSSIDNum,
		prCmdScanReq->ucChannelType, prCmdScanReq->ucChannelListNum);

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SCAN_REQ,
			    TRUE,
			    FALSE,
			    FALSE,
			    NULL,
			    NULL,
			    OFFSET_OF(CMD_SCAN_REQ_EXT_CH, aucIE) + prCmdScanReq->u2IELen,
			    (PUINT_8) prCmdScanReq, NULL, 0);
	/* sanity check for some scan parameters */
	if (prCmdScanReq->ucScanType >= SCAN_TYPE_NUM)
		kalSendAeeWarning("wlan", "wrong scan type %d", prCmdScanReq->ucScanType);
	else if (prCmdScanReq->ucChannelType >= SCAN_CHANNEL_NUM)
		kalSendAeeWarning("wlan", "wrong channel type %d", prCmdScanReq->ucChannelType);
	else if (prCmdScanReq->ucChannelType != SCAN_CHANNEL_SPECIFIED &&
		prCmdScanReq->ucChannelListNum != 0)
		kalSendAeeWarning("wlan",
			"channel list is not NULL but channel type is not specified");
	else if (prCmdScanReq->ucNetworkType >= NETWORK_TYPE_INDEX_NUM)
		kalSendAeeWarning("wlan", "wrong network type %d", prCmdScanReq->ucNetworkType);
	else if (prCmdScanReq->ucSSIDType >= BIT(4)) /* ssid type is wrong */
		kalSendAeeWarning("wlan", "wrong ssid type %d", prCmdScanReq->ucSSIDType);
	else if (prCmdScanReq->ucSSIDLength > 32)
		kalSendAeeWarning("wlan", "wrong ssid length %d", prCmdScanReq->ucSSIDLength);

	kalMemFree(prCmdScanReq, VIR_MEM_TYPE, sizeof(CMD_SCAN_REQ_EXT_CH));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief        Generate CMD_ID_SCAN_REQ command
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnSendScanReq(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	/*CMD_SCAN_REQ rCmdScanReq;*/
	P_CMD_SCAN_REQ prCmdScanReq;
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	if (prScanParam->ucChannelListNum > 32) {
		scnSendScanReqExtCh(prAdapter);
	} else {
		prCmdScanReq = kalMemAlloc(sizeof(CMD_SCAN_REQ), VIR_MEM_TYPE);
		if (prCmdScanReq == NULL) {
			DBGLOG(SCN, ERROR, "alloc CmdScanReq V1 fail");
			return;
		}
		/* send command packet for scan */
		kalMemZero(prCmdScanReq, sizeof(CMD_SCAN_REQ));
		prCmdScanReq->ucStructVersion = 1;
		COPY_MAC_ADDR(prCmdScanReq->aucBSSID, prScanParam->aucBSSID);
		if (!EQUAL_MAC_ADDR(prCmdScanReq->aucBSSID, "\xff\xff\xff\xff\xff\xff"))
			DBGLOG(SCN, INFO, "Include BSSID %pM in probe request, NetIdx %d\n",
				   prCmdScanReq->aucBSSID, prScanParam->eNetTypeIndex);
		prCmdScanReq->ucSeqNum = prScanParam->ucSeqNum;
		prCmdScanReq->ucNetworkType = (UINT_8) prScanParam->eNetTypeIndex;
		prCmdScanReq->ucScanType = (UINT_8) prScanParam->eScanType;
		prCmdScanReq->ucSSIDType = prScanParam->ucSSIDType;

		if (prScanParam->ucSSIDNum == 1) {
			COPY_SSID(prCmdScanReq->aucSSID,
				  prCmdScanReq->ucSSIDLength,
				  prScanParam->aucSpecifiedSSID[0], prScanParam->ucSpecifiedSSIDLen[0]);
		}

		prCmdScanReq->ucChannelType = (UINT_8) prScanParam->eScanChannel;

		if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
			/* P2P would use:
			 * 1. Specified Listen Channel of passive scan for LISTEN state.
			 * 2. Specified Listen Channel of Target Device of active scan for SEARCH state.
			 * (Target != NULL)
			 */
			prCmdScanReq->ucChannelListNum = prScanParam->ucChannelListNum;

			for (i = 0; i < prCmdScanReq->ucChannelListNum; i++) {
				prCmdScanReq->arChannelList[i].ucBand = (UINT_8) prScanParam->arChnlInfoList[i].eBand;

				prCmdScanReq->arChannelList[i].ucChannelNum =
				    (UINT_8) prScanParam->arChnlInfoList[i].ucChannelNum;
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
			P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

			prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
			if (prP2pBssInfo == NULL) {
				DBGLOG(P2P, ERROR, "scnSendScanReq prP2pBssInfo is NULL\n");
				return;
			}
#if CFG_TC10_FEATURE
			/*
			* mtk supplicant will scan 4 channels for prograssive scan
			* customer supplicant should have 3 channels when do social scan
			*/
			if (prScanParam->ucChannelListNum <= 4) {
				DBGLOG(P2P, INFO, "scnSendScanReq Channel number %d for 70ms, OP_MODE[%d]\n",
					prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
				prCmdScanReq->u2ChannelDwellTime = 70;
			} else if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
				DBGLOG(P2P, INFO, "scnSendScanReq Channel number %d for 100ms, OP_MODE[%d]\n",
					prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
				prCmdScanReq->u2ChannelDwellTime = 100;
			} else
				prCmdScanReq->u2ChannelDwellTime = 30;
#else
			prCmdScanReq->u2ChannelDwellTime = prScanParam->u2PassiveListenInterval;
#endif
		}
#endif
#if CFG_ENABLE_FAST_SCAN
		if (prScanParam->eNetTypeIndex == NETWORK_TYPE_AIS_INDEX)
			prCmdScanReq->u2ChannelDwellTime = CFG_FAST_SCAN_DWELL_TIME;
#endif
		if (prScanParam->eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
			prCmdScanReq->u2ChannelDwellTime = prScanParam->u2ChannelDwellTime;
			prCmdScanReq->u2ChannelMinDwellTime = prScanParam->u2MinChannelDwellTime;
		}

		if (prScanParam->u2IELen <= MAX_IE_LENGTH)
			prCmdScanReq->u2IELen = prScanParam->u2IELen;
		else
			prCmdScanReq->u2IELen = MAX_IE_LENGTH;

		if (prScanParam->u2IELen)
			kalMemCopy(prCmdScanReq->aucIE, prScanParam->aucIE, sizeof(UINT_8) * prCmdScanReq->u2IELen);

		DBGLOG(SCN, INFO, "ScanReqV1: ScanType=%d, SSIDType=%d, Num=%d, ChannelType=%d, Num=%d",
			prCmdScanReq->ucScanType, prCmdScanReq->ucSSIDType, prScanParam->ucSSIDNum,
			prCmdScanReq->ucChannelType, prCmdScanReq->ucChannelListNum);

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SCAN_REQ,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL,
				    OFFSET_OF(CMD_SCAN_REQ, aucIE) + prCmdScanReq->u2IELen,
				    (PUINT_8) prCmdScanReq, NULL, 0);
		/* sanity check for some scan parameters */
		if (prCmdScanReq->ucScanType >= SCAN_TYPE_NUM)
			kalSendAeeWarning("wlan", "wrong scan type %d", prCmdScanReq->ucScanType);
		else if (prCmdScanReq->ucChannelType >= SCAN_CHANNEL_NUM)
			kalSendAeeWarning("wlan", "wrong channel type %d", prCmdScanReq->ucChannelType);
		else if (prCmdScanReq->ucChannelType != SCAN_CHANNEL_SPECIFIED &&
			prCmdScanReq->ucChannelListNum != 0)
			kalSendAeeWarning("wlan",
				"channel list is not NULL but channel type is not specified");
		else if (prCmdScanReq->ucNetworkType >= NETWORK_TYPE_INDEX_NUM)
			kalSendAeeWarning("wlan", "wrong network type %d", prCmdScanReq->ucNetworkType);
		else if (prCmdScanReq->ucSSIDType >= BIT(4)) /* ssid type is wrong */
			kalSendAeeWarning("wlan", "wrong ssid type %d", prCmdScanReq->ucSSIDType);
		else if (prCmdScanReq->ucSSIDLength > 32)
			kalSendAeeWarning("wlan", "wrong ssid length %d", prCmdScanReq->ucSSIDLength);

		kalMemFree(prCmdScanReq, VIR_MEM_TYPE, sizeof(CMD_SCAN_REQ));
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief        Generate CMD_ID_SCAN_REQ_V2 command
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnSendScanReqV2ExtCh(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	/*CMD_SCAN_REQ_V2_EXT_CH rCmdScanReq;*/
	P_CMD_SCAN_REQ_V2_EXT_CH prCmdScanReq;
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	prCmdScanReq = kalMemAlloc(sizeof(CMD_SCAN_REQ_V2_EXT_CH), VIR_MEM_TYPE);
	if (prCmdScanReq == NULL) {
		DBGLOG(SCN, ERROR, "alloc CmdScanReq V2EXT fail");
		return;
	}

	/* send command packet for scan */
	kalMemZero(prCmdScanReq, sizeof(CMD_SCAN_REQ_V2_EXT_CH));

	prCmdScanReq->ucSeqNum = prScanParam->ucSeqNum;
	prCmdScanReq->ucNetworkType = (UINT_8) prScanParam->eNetTypeIndex;
	prCmdScanReq->ucScanType = (UINT_8) prScanParam->eScanType;
	prCmdScanReq->ucSSIDType = prScanParam->ucSSIDType;

	for (i = 0; i < prScanParam->ucSSIDNum; i++) {
		COPY_SSID(prCmdScanReq->arSSID[i].aucSsid,
			  prCmdScanReq->arSSID[i].u4SsidLen,
			  prScanParam->aucSpecifiedSSID[i], prScanParam->ucSpecifiedSSIDLen[i]);
	}

	prCmdScanReq->u2ProbeDelayTime = (UINT_8) prScanParam->u2ProbeDelayTime;
	prCmdScanReq->ucChannelType = (UINT_8) prScanParam->eScanChannel;

	if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
		/* P2P would use:
		 * 1. Specified Listen Channel of passive scan for LISTEN state.
		 * 2. Specified Listen Channel of Target Device of active scan for SEARCH state. (Target != NULL)
		 */
		prCmdScanReq->ucChannelListNum = prScanParam->ucChannelListNum;

		for (i = 0; i < prCmdScanReq->ucChannelListNum; i++) {
			prCmdScanReq->arChannelList[i].ucBand = (UINT_8) prScanParam->arChnlInfoList[i].eBand;

			prCmdScanReq->arChannelList[i].ucChannelNum =
			    (UINT_8) prScanParam->arChnlInfoList[i].ucChannelNum;
		}
	}
#if CFG_ENABLE_WIFI_DIRECT
	if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
		P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		if (prP2pBssInfo == NULL) {
			DBGLOG(P2P, ERROR, "scnSendScanReqV2ExtCh prP2pBssInfo is NULL\n");
			return;
		}
#if CFG_TC10_FEATURE
		/*
		 * mtk supplicant will scan 4 channels for prograssive scan
		 * customer supplicant should have 3 channels when do social scan
		 */
		if (prScanParam->ucChannelListNum <= 4) {
			DBGLOG(P2P, INFO, "scnSendScanReqV2ExtCh Channel number %d for 70ms, OP_MODE[%d]\n",
				prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
			prCmdScanReq->u2ChannelDwellTime = 70;
		} else if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			DBGLOG(P2P, INFO, "scnSendScanReqV2ExtCh Channel number %d for 100ms, OP_MODE[%d]\n",
				prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
			prCmdScanReq->u2ChannelDwellTime = 100;
		} else
			prCmdScanReq->u2ChannelDwellTime = 30;
#else
		prCmdScanReq->u2ChannelDwellTime = prScanParam->u2PassiveListenInterval;
#endif
	}
#endif

	if (prScanParam->u2IELen <= MAX_IE_LENGTH)
		prCmdScanReq->u2IELen = prScanParam->u2IELen;
	else
		prCmdScanReq->u2IELen = MAX_IE_LENGTH;

	if (prScanParam->u2IELen)
		kalMemCopy(prCmdScanReq->aucIE, prScanParam->aucIE, sizeof(UINT_8) * prCmdScanReq->u2IELen);

	DBGLOG(SCN, INFO, "ScanReqV2EXT: ScanType=%d, SSIDType=%d, Num=%d, ChannelType=%d, Num=%d",
		prCmdScanReq->ucScanType, prCmdScanReq->ucSSIDType, prScanParam->ucSSIDNum,
		prCmdScanReq->ucChannelType, prCmdScanReq->ucChannelListNum);

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SCAN_REQ_V2,
			    TRUE,
			    FALSE,
			    FALSE,
			    NULL,
			    NULL,
			    OFFSET_OF(CMD_SCAN_REQ_V2_EXT_CH, aucIE) + prCmdScanReq->u2IELen,
			    (PUINT_8) prCmdScanReq, NULL, 0);
	/* sanity check for some scan parameters */
	if (prCmdScanReq->ucScanType >= SCAN_TYPE_NUM)
		kalSendAeeWarning("wlan", "wrong scan type %d", prCmdScanReq->ucScanType);
	else if (prCmdScanReq->ucChannelType >= SCAN_CHANNEL_NUM)
		kalSendAeeWarning("wlan", "wrong channel type %d", prCmdScanReq->ucChannelType);
	else if (prCmdScanReq->ucChannelType != SCAN_CHANNEL_SPECIFIED &&
		prCmdScanReq->ucChannelListNum != 0)
		kalSendAeeWarning("wlan",
			"channel list is not NULL but channel type is not specified");
	else if (prCmdScanReq->ucNetworkType >= NETWORK_TYPE_INDEX_NUM)
		kalSendAeeWarning("wlan", "wrong network type %d", prCmdScanReq->ucNetworkType);
	else if (prCmdScanReq->ucSSIDType >= BIT(4)) /* ssid type is wrong */
		kalSendAeeWarning("wlan", "wrong ssid type %d", prCmdScanReq->ucSSIDType);

	kalMemFree(prCmdScanReq, VIR_MEM_TYPE, sizeof(CMD_SCAN_REQ_V2_EXT_CH));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief        Generate CMD_ID_SCAN_REQ_V3 command
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnSendScanReqV3ExtCh(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	/* P_CMD_SCAN_REQ_V3_EXT_CH prCmdScanReqV3; ScanReqV4 replace of ScanReqV3 for random MAC */
	struct CMD_SCAN_REQ_V4_EXT_CH *prCmdScanReqV4;
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	prCmdScanReqV4 = kalMemAlloc(sizeof(struct CMD_SCAN_REQ_V4_EXT_CH), VIR_MEM_TYPE);
	if (prCmdScanReqV4 == NULL) {
		DBGLOG(SCN, ERROR, "alloc CmdScanReq V4EXT fail");
		return;
	}
	/* send command packet for scan */
	kalMemZero(prCmdScanReqV4, sizeof(struct CMD_SCAN_REQ_V4_EXT_CH));

	prCmdScanReqV4->ucSeqNum = prScanParam->ucSeqNum;
	prCmdScanReqV4->ucNetworkType = (UINT_8) prScanParam->eNetTypeIndex;
	prCmdScanReqV4->ucScanType = (UINT_8) prScanParam->eScanType;
	prCmdScanReqV4->ucSSIDType = prScanParam->ucSSIDType;

	if (kalIsValidMacAddr(prScanParam->aucRandomMac)) {
		prCmdScanReqV4->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
		kalMemCopy(prCmdScanReqV4->aucRandomMac,
			prScanParam->aucRandomMac, MAC_ADDR_LEN);
	}
	kalMemCopy(prCmdScanReqV4->arSSID[0].aucSsid, "CMD_SCAN_REQ_V4_T", strlen("CMD_SCAN_REQ_V4_T"));
	prCmdScanReqV4->arSSID[0].u4SsidLen = 0;
	for (i = 1; i <= prScanParam->ucSSIDNum; i++) {
		COPY_SSID(prCmdScanReqV4->arSSID[i].aucSsid,
			  prCmdScanReqV4->arSSID[i].u4SsidLen,
			  prScanParam->aucSpecifiedSSID[i - 1], prScanParam->ucSpecifiedSSIDLen[i - 1]);
	}

	prCmdScanReqV4->u2ProbeDelayTime = (UINT_8) prScanParam->u2ProbeDelayTime;
	prCmdScanReqV4->ucChannelType = (UINT_8) prScanParam->eScanChannel;

	if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
		/* P2P would use:
		 * 1. Specified Listen Channel of passive scan for LISTEN state.
		 * 2. Specified Listen Channel of Target Device of active scan for SEARCH state. (Target != NULL)
		 */
		prCmdScanReqV4->ucChannelListNum = prScanParam->ucChannelListNum;

		for (i = 0; i < prCmdScanReqV4->ucChannelListNum; i++) {
			prCmdScanReqV4->arChannelList[i].ucBand = (UINT_8) prScanParam->arChnlInfoList[i].eBand;

			prCmdScanReqV4->arChannelList[i].ucChannelNum =
			    (UINT_8) prScanParam->arChnlInfoList[i].ucChannelNum;
		}
	}
#if CFG_ENABLE_WIFI_DIRECT
	if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
		P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		if (prP2pBssInfo == NULL) {
			DBGLOG(P2P, ERROR, "scnSendScanReqV4ExtCh prP2pBssInfo is NULL\n");
			return;
		}
#if CFG_TC10_FEATURE
		/*
		 * mtk supplicant will scan 4 channels for prograssive scan
		 * customer supplicant should have 3 channels when do social scan
		 */
		if (prScanParam->ucChannelListNum <= 4) {
			DBGLOG(P2P, INFO, "scnSendScanReqV4ExtCh Channel number %d for 70ms, OP_MODE[%d]\n",
				prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
			prCmdScanReqV4->u2ChannelDwellTime = 70;
		} else if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			DBGLOG(P2P, INFO, "scnSendScanReqV4ExtCh Channel number %d for 100ms, OP_MODE[%d]\n",
				prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
			prCmdScanReqV4->u2ChannelDwellTime = 100;
		} else
			prCmdScanReqV4->u2ChannelDwellTime = 30;
#else
		prCmdScanReqV4->u2ChannelDwellTime = prScanParam->u2PassiveListenInterval;
#endif
	}
#endif

	if (prScanParam->u2IELen <= MAX_IE_LENGTH)
		prCmdScanReqV4->u2IELen = prScanParam->u2IELen;
	else
		prCmdScanReqV4->u2IELen = MAX_IE_LENGTH;

	if (prScanParam->u2IELen)
		kalMemCopy(prCmdScanReqV4->aucIE, prScanParam->aucIE, sizeof(UINT_8) * prCmdScanReqV4->u2IELen);

	DBGLOG(SCN, INFO, "ScanReqV4EXT: ScanType=%d, SSIDType=%d, Num=%d, ChannelType=%d, Num=%d, Mask=%d, Mac="
		MACSTR"\n",
		prCmdScanReqV4->ucScanType, prCmdScanReqV4->ucSSIDType, prScanParam->ucSSIDNum,
		prCmdScanReqV4->ucChannelType, prCmdScanReqV4->ucChannelListNum,
		prCmdScanReqV4->ucScnFuncMask, MAC2STR(prCmdScanReqV4->aucRandomMac));

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SCAN_REQ_V2,
			    TRUE,
			    FALSE,
			    FALSE,
			    NULL,
			    NULL,
			    sizeof(struct CMD_SCAN_REQ_V4_EXT_CH),
			    (PUINT_8)prCmdScanReqV4, NULL, 0);

	kalMemFree(prCmdScanReqV4, VIR_MEM_TYPE, sizeof(struct CMD_SCAN_REQ_V4_EXT_CH));

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        Generate CMD_ID_SCAN_REQ_V2 command
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnSendScanReqV2(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	/* P_CMD_SCAN_REQ_V2 prCmdScanReq; */
	/* P_CMD_SCAN_REQ_V3 prCmdScanReqV3; ScanReqV3 replace of ScanReqV2 for multi SSID feature */
	struct CMD_SCAN_REQ_V4 *prCmdScanReqV4; /* ScanReqV4 replace of ScanReqV3 for random MAC */
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	if (prScanParam->ucChannelListNum > 32) {
		scnSendScanReqV3ExtCh(prAdapter);
	} else {
		prCmdScanReqV4 = kalMemAlloc(sizeof(struct CMD_SCAN_REQ_V4), VIR_MEM_TYPE);
		if (prCmdScanReqV4 == NULL) {
			DBGLOG(SCN, ERROR, "alloc CmdScanReq V4 fail");
			return;
		}
		/* send command packet for scan */
		kalMemZero(prCmdScanReqV4, sizeof(struct CMD_SCAN_REQ_V4));

		prCmdScanReqV4->ucSeqNum = prScanParam->ucSeqNum;
		prCmdScanReqV4->ucNetworkType = (UINT_8) prScanParam->eNetTypeIndex;
		prCmdScanReqV4->ucScanType = (UINT_8) prScanParam->eScanType;
		prCmdScanReqV4->ucSSIDType = prScanParam->ucSSIDType;
		if (kalIsValidMacAddr(prScanParam->aucRandomMac)) {
			prCmdScanReqV4->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
			kalMemCopy(prCmdScanReqV4->aucRandomMac,
				prScanParam->aucRandomMac, MAC_ADDR_LEN);
		}

		/* After android R, gen2 must support scan random MAC
		 * modify scan req from V3 to V4 to add random MAC related
		 * variables. FW will recognize "CMD_SCAN_REQ_V4_T" to decide
		 * use random MAC or not.
		 */
		kalMemCopy(prCmdScanReqV4->arSSID[0].aucSsid, "CMD_SCAN_REQ_V4_T", strlen("CMD_SCAN_REQ_V4_T"));
		prCmdScanReqV4->arSSID[0].u4SsidLen = 0;
		for (i = 1; i <= prScanParam->ucSSIDNum; i++) {
			COPY_SSID(prCmdScanReqV4->arSSID[i].aucSsid,
				  prCmdScanReqV4->arSSID[i].u4SsidLen,
				  prScanParam->aucSpecifiedSSID[i - 1],
				  prScanParam->ucSpecifiedSSIDLen[i - 1]);
		}

		prCmdScanReqV4->u2ProbeDelayTime = (UINT_8) prScanParam->u2ProbeDelayTime;
		prCmdScanReqV4->ucChannelType = (UINT_8) prScanParam->eScanChannel;

		if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
			/* P2P would use:
			 * 1. Specified Listen Channel of passive scan for LISTEN state.
			 * 2. Specified Listen Channel of Target Device of active scan for SEARCH state.
			 * (Target != NULL)
			 */
			prCmdScanReqV4->ucChannelListNum = prScanParam->ucChannelListNum;

			for (i = 0; i < prCmdScanReqV4->ucChannelListNum; i++) {
				prCmdScanReqV4->arChannelList[i].ucBand =
					(UINT_8) prScanParam->arChnlInfoList[i].eBand;

				prCmdScanReqV4->arChannelList[i].ucChannelNum =
				    (UINT_8) prScanParam->arChnlInfoList[i].ucChannelNum;
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
			P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

			prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
			if (prP2pBssInfo == NULL) {
				DBGLOG(P2P, ERROR, "scnSendScanReqV4 prP2pBssInfo is NULL\n");
				return;
			}
#if CFG_TC10_FEATURE
			/*
			* mtk supplicant will scan 4 channels for prograssive scan
			* customer supplicant should have 3 channels when do social scan
			*/
			if (prScanParam->ucChannelListNum <= 4) {
				DBGLOG(P2P, INFO, "scnSendScanReqV4 Channel number %d for 70ms, OP_MODE[%d]\n",
					prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
				prCmdScanReqV4->u2ChannelDwellTime = 70;
			} else if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
				DBGLOG(P2P, INFO, "scnSendScanReqV4 Channel number %d for 100ms, OP_MODE[%d]\n",
					prScanParam->ucChannelListNum, prP2pBssInfo->eCurrentOPMode);
				prCmdScanReqV4->u2ChannelDwellTime = 100;
			} else
				prCmdScanReqV4->u2ChannelDwellTime = 30;
#else
			prCmdScanReqV4->u2ChannelDwellTime = prScanParam->u2PassiveListenInterval;
#endif
		}
#endif
		if (prScanParam->u2IELen <= MAX_IE_LENGTH)
			prCmdScanReqV4->u2IELen = prScanParam->u2IELen;
		else
			prCmdScanReqV4->u2IELen = MAX_IE_LENGTH;

		if (prScanParam->u2IELen)
			kalMemCopy(prCmdScanReqV4->aucIE, prScanParam->aucIE,
				sizeof(UINT_8) * prCmdScanReqV4->u2IELen);

		DBGLOG(SCN, INFO, "ScanReqV4: ScanType=%d, SSIDType=%d, Num=%d, ChannelType=%d, Num=%d, Mask=%d, Mac="
			MACSTR"\n",
			prCmdScanReqV4->ucScanType, prCmdScanReqV4->ucSSIDType, prScanParam->ucSSIDNum,
			prCmdScanReqV4->ucChannelType, prCmdScanReqV4->ucChannelListNum,
			prCmdScanReqV4->ucScnFuncMask, MAC2STR(prCmdScanReqV4->aucRandomMac));

		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SCAN_REQ_V2,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    NULL,
				    sizeof(struct CMD_SCAN_REQ_V4),
				    (PUINT_8) prCmdScanReqV4, NULL, 0);

		/* sanity check for some scan parameters */
		if (prCmdScanReqV4->ucScanType >= SCAN_TYPE_NUM)
			kalSendAeeWarning("wlan", "wrong scan type %d", prCmdScanReqV4->ucScanType);
		else if (prCmdScanReqV4->ucChannelType >= SCAN_CHANNEL_NUM)
			kalSendAeeWarning("wlan", "wrong channel type %d", prCmdScanReqV4->ucChannelType);
		else if (prCmdScanReqV4->ucChannelType != SCAN_CHANNEL_SPECIFIED &&
			prCmdScanReqV4->ucChannelListNum != 0)
			kalSendAeeWarning("wlan",
				"channel list is not NULL but channel type is not specified");
		else if (prCmdScanReqV4->ucNetworkType >= NETWORK_TYPE_INDEX_NUM)
			kalSendAeeWarning("wlan", "wrong network type %d", prCmdScanReqV4->ucNetworkType);
		else if (prCmdScanReqV4->ucSSIDType >= BIT(4)) /* ssid type is wrong */
			kalSendAeeWarning("wlan", "wrong ssid type %d", prCmdScanReqV4->ucSSIDType);

		kalMemFree(prCmdScanReqV4, VIR_MEM_TYPE, sizeof(struct CMD_SCAN_REQ_V4));
	}
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
VOID scnFsmMsgStart(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;

	ASSERT(prMsgHdr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	if (prMsgHdr->eMsgId == MID_MNY_CNM_SCAN_CONTINUE) {
		cnmMemFree(prAdapter, prMsgHdr);
		scnFsmSteps(prAdapter, SCAN_STATE_IDLE);
	} else if (prScanInfo->eCurrentState == SCAN_STATE_IDLE && !cnmChUtilIsRunning(prAdapter)) {
		if (prMsgHdr->eMsgId == MID_AIS_SCN_SCAN_REQ
		    || prMsgHdr->eMsgId == MID_BOW_SCN_SCAN_REQ
		    || prMsgHdr->eMsgId == MID_P2P_SCN_SCAN_REQ || prMsgHdr->eMsgId == MID_RLM_SCN_SCAN_REQ) {
			scnFsmHandleScanMsg(prAdapter, (P_MSG_SCN_SCAN_REQ) prMsgHdr);
		} else if (prMsgHdr->eMsgId == MID_AIS_SCN_SCAN_REQ_V2
			   || prMsgHdr->eMsgId == MID_BOW_SCN_SCAN_REQ_V2
			   || prMsgHdr->eMsgId == MID_P2P_SCN_SCAN_REQ_V2
			   || prMsgHdr->eMsgId == MID_RLM_SCN_SCAN_REQ_V2) {
			scnFsmHandleScanMsgV2(prAdapter, (P_MSG_SCN_SCAN_REQ_V2) prMsgHdr);
		} else {
			/* should not deliver to this function */
			ASSERT(0);
		}

		cnmMemFree(prAdapter, prMsgHdr);
		scnFsmSteps(prAdapter, SCAN_STATE_SCANNING);
	} else {
		LINK_INSERT_TAIL(&prScanInfo->rPendingMsgList, &prMsgHdr->rLinkEntry);
	}

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
VOID scnFsmMsgAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_CANCEL prScanCancel;
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	CMD_SCAN_CANCEL rCmdScanCancel;

	ASSERT(prMsgHdr);

	prScanCancel = (P_MSG_SCN_SCAN_CANCEL) prMsgHdr;
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	if (prScanInfo->eCurrentState != SCAN_STATE_IDLE) {
		if (prScanCancel->ucSeqNum == prScanParam->ucSeqNum &&
		    prScanCancel->ucNetTypeIndex == (UINT_8) prScanParam->eNetTypeIndex) {
			/* send cancel message to firmware domain */
			rCmdScanCancel.ucSeqNum = prScanParam->ucSeqNum;

#if CFG_ENABLE_WIFI_DIRECT
			if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX)
				rCmdScanCancel.ucIsExtChannel = (UINT_8) prScanCancel->fgIsChannelExt;
			else
				rCmdScanCancel.ucIsExtChannel = (UINT_8) FALSE;
#endif

			wlanSendSetQueryCmd(prAdapter,
					    CMD_ID_SCAN_CANCEL,
					    TRUE,
					    FALSE,
					    FALSE,
					    NULL, NULL, sizeof(CMD_SCAN_CANCEL), (PUINT_8) &rCmdScanCancel, NULL, 0);

			/* generate scan-done event for caller */
			scnFsmGenerateScanDoneMsg(prAdapter,
						  prScanParam->ucSeqNum,
						  (UINT_8) prScanParam->eNetTypeIndex, SCAN_STATUS_CANCELLED);

			/*Full2Partial at here, should stop save channel num*/
			if (prAdapter->prGlueInfo->ucTrScanType == 1) {
				prAdapter->prGlueInfo->ucTrScanType = 0;
				prAdapter->prGlueInfo->u4LastFullScanTime = 0;
				DBGLOG(SCN, INFO, "Full2Partial scan cancel update ucTrScanType=%d\n",
					prAdapter->prGlueInfo->ucTrScanType);
			}

			/* switch to next pending scan */
			scnFsmSteps(prAdapter, SCAN_STATE_IDLE);
		} else {
			scnFsmRemovePendingMsg(prAdapter, prScanCancel->ucSeqNum, prScanCancel->ucNetTypeIndex);
		}
	}

	cnmMemFree(prAdapter, prMsgHdr);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief            Scan Message Parsing (Legacy)
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnFsmHandleScanMsg(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ prScanReqMsg)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	UINT_32 i;

	ASSERT(prAdapter);
	ASSERT(prScanReqMsg);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	prScanParam->eScanType = prScanReqMsg->eScanType;
	prScanParam->eNetTypeIndex = (ENUM_NETWORK_TYPE_INDEX_T) prScanReqMsg->ucNetTypeIndex;
	prScanParam->ucSSIDType = prScanReqMsg->ucSSIDType;
	kalMemCopy(prScanParam->aucBSSID, prScanReqMsg->aucBSSID, MAC_ADDR_LEN);
	if (prScanParam->ucSSIDType & (SCAN_REQ_SSID_SPECIFIED | SCAN_REQ_SSID_P2P_WILDCARD |
		SCAN_REQ_SSID_SPECIFIED_ONLY)) {
		prScanParam->ucSSIDNum = 1;

		COPY_SSID(prScanParam->aucSpecifiedSSID[0],
			  prScanParam->ucSpecifiedSSIDLen[0], prScanReqMsg->aucSSID, prScanReqMsg->ucSSIDLength);

		/* reset SSID length to zero for rest array entries */
		for (i = 1; i < SCN_SSID_MAX_NUM; i++)
			prScanParam->ucSpecifiedSSIDLen[i] = 0;
	} else {
		prScanParam->ucSSIDNum = 0;

		for (i = 0; i < SCN_SSID_MAX_NUM; i++)
			prScanParam->ucSpecifiedSSIDLen[i] = 0;
	}

	prScanParam->u2ProbeDelayTime = 0;
	prScanParam->eScanChannel = prScanReqMsg->eScanChannel;
	if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
		if (prScanReqMsg->ucChannelListNum <= MAXIMUM_OPERATION_CHANNEL_LIST)
			prScanParam->ucChannelListNum = prScanReqMsg->ucChannelListNum;
		else
			prScanParam->ucChannelListNum = MAXIMUM_OPERATION_CHANNEL_LIST;

		kalMemCopy(prScanParam->arChnlInfoList,
			   prScanReqMsg->arChnlInfoList, sizeof(RF_CHANNEL_INFO_T) * prScanParam->ucChannelListNum);
	}

	if (prScanReqMsg->u2IELen <= MAX_IE_LENGTH)
		prScanParam->u2IELen = prScanReqMsg->u2IELen;
	else
		prScanParam->u2IELen = MAX_IE_LENGTH;

	if (prScanParam->u2IELen)
		kalMemCopy(prScanParam->aucIE, prScanReqMsg->aucIE, prScanParam->u2IELen);
#if CFG_ENABLE_WIFI_DIRECT
	if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX)
		prScanParam->u2PassiveListenInterval = prScanReqMsg->u2ChannelDwellTime;
#endif
	if (prScanParam->eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
		prScanParam->u2ChannelDwellTime = prScanReqMsg->u2ChannelDwellTime;
		prScanParam->u2MinChannelDwellTime = prScanReqMsg->u2MinChannelDwellTime;
	}
	prScanParam->ucSeqNum = prScanReqMsg->ucSeqNum;

	if (prScanReqMsg->rMsgHdr.eMsgId == MID_RLM_SCN_SCAN_REQ)
		prScanParam->fgIsObssScan = TRUE;
	else
		prScanParam->fgIsObssScan = FALSE;

	prScanParam->fgIsScanV2 = FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief            Scan Message Parsing - V2 with multiple SSID support
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnFsmHandleScanMsgV2(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	UINT_32 i;

	ASSERT(prAdapter);
	ASSERT(prScanReqMsg);
	ASSERT(prScanReqMsg->ucSSIDNum <= SCN_SSID_MAX_NUM);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	prScanParam->eScanType = prScanReqMsg->eScanType;
	prScanParam->eNetTypeIndex = (ENUM_NETWORK_TYPE_INDEX_T) prScanReqMsg->ucNetTypeIndex;
	prScanParam->ucSSIDType = prScanReqMsg->ucSSIDType;
	prScanParam->ucSSIDNum = prScanReqMsg->ucSSIDNum;
	kalMemCopy(prScanParam->aucRandomMac, prScanReqMsg->aucRandomMac,
		MAC_ADDR_LEN);

	for (i = 0; i < prScanReqMsg->ucSSIDNum; i++) {
		COPY_SSID(prScanParam->aucSpecifiedSSID[i],
			  prScanParam->ucSpecifiedSSIDLen[i],
			  prScanReqMsg->prSsid[i].aucSsid, (UINT_8) prScanReqMsg->prSsid[i].u4SsidLen);
	}

	prScanParam->u2ProbeDelayTime = prScanReqMsg->u2ProbeDelay;
	prScanParam->eScanChannel = prScanReqMsg->eScanChannel;
	if (prScanParam->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
		if (prScanReqMsg->ucChannelListNum <= MAXIMUM_OPERATION_CHANNEL_LIST)
			prScanParam->ucChannelListNum = prScanReqMsg->ucChannelListNum;
		else
			prScanParam->ucChannelListNum = MAXIMUM_OPERATION_CHANNEL_LIST;

		kalMemCopy(prScanParam->arChnlInfoList,
			   prScanReqMsg->arChnlInfoList, sizeof(RF_CHANNEL_INFO_T) * prScanParam->ucChannelListNum);
	}

	if (prScanReqMsg->u2IELen <= MAX_IE_LENGTH)
		prScanParam->u2IELen = prScanReqMsg->u2IELen;
	else
		prScanParam->u2IELen = MAX_IE_LENGTH;

	if (prScanParam->u2IELen)
		kalMemCopy(prScanParam->aucIE, prScanReqMsg->aucIE, prScanParam->u2IELen);
#if CFG_ENABLE_WIFI_DIRECT
	if (prScanParam->eNetTypeIndex == NETWORK_TYPE_P2P_INDEX)
		prScanParam->u2PassiveListenInterval = prScanReqMsg->u2ChannelDwellTime;
#endif
	prScanParam->ucSeqNum = prScanReqMsg->ucSeqNum;

	if (prScanReqMsg->rMsgHdr.eMsgId == MID_RLM_SCN_SCAN_REQ)
		prScanParam->fgIsObssScan = TRUE;
	else
		prScanParam->fgIsObssScan = FALSE;

	prScanParam->fgIsScanV2 = TRUE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief            Remove pending scan request
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnFsmRemovePendingMsg(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSeqNum, IN UINT_8 ucNetTypeIndex)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	P_MSG_HDR_T prPendingMsgHdr, prPendingMsgHdrNext, prRemoveMsgHdr = NULL;
	P_LINK_ENTRY_T prRemoveLinkEntry = NULL;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	/* traverse through rPendingMsgList for removal */
	LINK_FOR_EACH_ENTRY_SAFE(prPendingMsgHdr,
				 prPendingMsgHdrNext, &(prScanInfo->rPendingMsgList), rLinkEntry, MSG_HDR_T) {
		if (prPendingMsgHdr->eMsgId == MID_AIS_SCN_SCAN_REQ
		    || prPendingMsgHdr->eMsgId == MID_BOW_SCN_SCAN_REQ
		    || prPendingMsgHdr->eMsgId == MID_P2P_SCN_SCAN_REQ
		    || prPendingMsgHdr->eMsgId == MID_RLM_SCN_SCAN_REQ) {
			P_MSG_SCN_SCAN_REQ prScanReqMsg = (P_MSG_SCN_SCAN_REQ) prPendingMsgHdr;

			if (ucSeqNum == prScanReqMsg->ucSeqNum && ucNetTypeIndex == prScanReqMsg->ucNetTypeIndex) {
				prRemoveLinkEntry = &(prScanReqMsg->rMsgHdr.rLinkEntry);
				prRemoveMsgHdr = prPendingMsgHdr;
			}
		} else if (prPendingMsgHdr->eMsgId == MID_AIS_SCN_SCAN_REQ_V2
			   || prPendingMsgHdr->eMsgId == MID_BOW_SCN_SCAN_REQ_V2
			   || prPendingMsgHdr->eMsgId == MID_P2P_SCN_SCAN_REQ_V2
			   || prPendingMsgHdr->eMsgId == MID_RLM_SCN_SCAN_REQ_V2) {
			P_MSG_SCN_SCAN_REQ_V2 prScanReqMsgV2 = (P_MSG_SCN_SCAN_REQ_V2) prPendingMsgHdr;

			if (ucSeqNum == prScanReqMsgV2->ucSeqNum && ucNetTypeIndex == prScanReqMsgV2->ucNetTypeIndex) {
				prRemoveLinkEntry = &(prScanReqMsgV2->rMsgHdr.rLinkEntry);
				prRemoveMsgHdr = prPendingMsgHdr;
			}
		}

		if (prRemoveLinkEntry) {
			/* generate scan-done event for caller */
			scnFsmGenerateScanDoneMsg(prAdapter, ucSeqNum, ucNetTypeIndex, SCAN_STATUS_CANCELLED);

			/* remove from pending list */
			LINK_REMOVE_KNOWN_ENTRY(&(prScanInfo->rPendingMsgList), prRemoveLinkEntry);
			cnmMemFree(prAdapter, prRemoveMsgHdr);

			break;
		}
	}

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
VOID scnEventScanDone(IN P_ADAPTER_T prAdapter, IN P_EVENT_SCAN_DONE prScanDone)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	/* buffer empty channel information */
	if (prScanParam->eScanChannel == SCAN_CHANNEL_FULL || prScanParam->eScanChannel == SCAN_CHANNEL_2G4) {
		if (prScanDone->ucSparseChannelValid) {
			prScanInfo->fgIsSparseChannelValid = TRUE;
			prScanInfo->rSparseChannel.eBand = (ENUM_BAND_T) prScanDone->rSparseChannel.ucBand;
			prScanInfo->rSparseChannel.ucChannelNum = prScanDone->rSparseChannel.ucChannelNum;
		} else {
			prScanInfo->fgIsSparseChannelValid = FALSE;
		}
	}

	/*Full2Partial at here, should stop save channel num*/
	DBGLOG(SCN, TRACE, "Full2Partial scan done ucTrScanType=%d, eScanChannel=%d\n",
		prAdapter->prGlueInfo->ucTrScanType, prScanParam->eScanChannel);
	if ((prScanParam->eScanChannel == SCAN_CHANNEL_FULL) &&
		(prAdapter->prGlueInfo->ucTrScanType == 1)) {
		prAdapter->prGlueInfo->ucTrScanType = 0;
		DBGLOG(SCN, INFO, "Full2Partial scan done update ucTrScanType=%d\n",
			prAdapter->prGlueInfo->ucTrScanType);
	}

	if (prScanInfo->eCurrentState == SCAN_STATE_SCANNING && prScanDone->ucSeqNum == prScanParam->ucSeqNum) {
		/* generate scan-done event for caller */
		scnFsmGenerateScanDoneMsg(prAdapter,
					  prScanParam->ucSeqNum, (UINT_8) prScanParam->eNetTypeIndex, SCAN_STATUS_DONE);

		/* switch to next pending scan */
		scnFsmSteps(prAdapter, SCAN_STATE_IDLE);
	} else {
		DBGLOG(SCN, WARN, "Unexpected SCAN-DONE event: SeqNum = %d, Current State = %d\n",
				   prScanDone->ucSeqNum, prScanInfo->eCurrentState);
	}

}				/* end of scnEventScanDone */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
scnFsmGenerateScanDoneMsg(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucSeqNum, IN UINT_8 ucNetTypeIndex, IN ENUM_SCAN_STATUS eScanStatus)
{
	P_SCAN_INFO_T prScanInfo;
	P_SCAN_PARAM_T prScanParam;
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanParam = &prScanInfo->rScanParam;

	DBGLOG(SCN, INFO,
		"Rcv Scan Done, NetIdx %d, Obss %d, Status %d, Seq %d ,STA MAC:[%pM] FwVer: 0x%x.%x DriVer:%s\n",
				  ucNetTypeIndex, prScanParam->fgIsObssScan, eScanStatus, ucSeqNum,
				  prAdapter->rWifiVar.aucMacAddress,
				  prAdapter->rVerInfo.u2FwOwnVersion,
				  prAdapter->rVerInfo.u2FwOwnVersionExtend,
				  WIFI_DRIVER_VERSION);
	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_DONE));
	if (!prScanDoneMsg) {
		ASSERT(0);	/* Can't indicate SCAN FSM Complete */
		return;
	}

	if (prScanParam->fgIsObssScan == TRUE) {
		prScanDoneMsg->rMsgHdr.eMsgId = MID_SCN_RLM_SCAN_DONE;
	} else {
		switch ((ENUM_NETWORK_TYPE_INDEX_T) ucNetTypeIndex) {
		case NETWORK_TYPE_AIS_INDEX:
			prScanDoneMsg->rMsgHdr.eMsgId = MID_SCN_AIS_SCAN_DONE;
			break;

#if CFG_ENABLE_WIFI_DIRECT
		case NETWORK_TYPE_P2P_INDEX:
			prScanDoneMsg->rMsgHdr.eMsgId = MID_SCN_P2P_SCAN_DONE;
			break;
#endif

#if CFG_ENABLE_BT_OVER_WIFI
		case NETWORK_TYPE_BOW_INDEX:
			prScanDoneMsg->rMsgHdr.eMsgId = MID_SCN_BOW_SCAN_DONE;
			break;
#endif

		default:
			ASSERT(0);
			break;
		}
	}

	prScanDoneMsg->ucSeqNum = ucSeqNum;
	prScanDoneMsg->ucNetTypeIndex = ucNetTypeIndex;
	prScanDoneMsg->eScanStatus = eScanStatus;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanDoneMsg, MSG_SEND_METHOD_BUF);

}				/* end of scnFsmGenerateScanDoneMsg() */

/*----------------------------------------------------------------------------*/
/*!
* \brief        Query for most sparse channel
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnQuerySparseChannel(IN P_ADAPTER_T prAdapter, P_ENUM_BAND_T prSparseBand, PUINT_8 pucSparseChannel)
{
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	if (prScanInfo->fgIsSparseChannelValid == TRUE) {
		if (prSparseBand)
			*prSparseBand = prScanInfo->rSparseChannel.eBand;

		if (pucSparseChannel)
			*pucSparseChannel = prScanInfo->rSparseChannel.ucChannelNum;

		return TRUE;
	} else {
		return FALSE;
	}
}

VOID scnFsmRunEventNloConReqTimeOut(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prScanInfo->fgNloScanning = TRUE;
	DBGLOG(SCN, INFO, "scnFsmNloConReqTimeOut\n");
	scnPSCNFsm(prAdapter, PSCN_RESET);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief        Event handler for NLO done event
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID scnEventNloDone(IN P_ADAPTER_T prAdapter, IN P_EVENT_NLO_DONE_T prNloDone)
{
	P_SCAN_INFO_T prScanInfo;
	P_NLO_PARAM_T prNloParam;
	P_SCAN_PARAM_T prScanParam;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prNloParam = &prScanInfo->rNloParam;
	prScanParam = &prNloParam->rScanParam;

	if (prScanInfo->fgNloScanning == TRUE) {
		DBGLOG(SCN, INFO, "scnEventNloDone Current State = %d\n", prScanInfo->eCurrentState);

		kalSchedScanResults(prAdapter->prGlueInfo);

		if (prNloParam->fgStopAfterIndication == TRUE)
			prScanInfo->fgNloScanning = FALSE;

		kalMemZero(&prNloParam->aprPendingBssDescToInd[0],
					CFG_SCAN_SSID_MATCH_MAX_NUM * sizeof(P_BSS_DESC_T));

		cnmTimerStopTimer(prAdapter, &prAdapter->rScanNloTimeoutTimer);

		cnmTimerInitTimer(prAdapter,
				  &prAdapter->rScanNloTimeoutTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) scnFsmRunEventNloConReqTimeOut,
				  (ULONG) NULL);

		cnmTimerStartTimer(prAdapter,
				   &prAdapter->rScanNloTimeoutTimer,
				   5000);

	} else {
		DBGLOG(SCN, INFO, "Unexpected NLO-DONE event\n");
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for starting scheduled scan
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
scnFsmSchedScanRequest(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_NLO_PARAM_T prNloParam;
	P_SCAN_PARAM_T prScanParam;
	P_CMD_NLO_REQ prCmdNloReq;
	P_PARAM_SCHED_SCAN_REQUEST prSchedScanRequest;
	UINT_32 i, j;
	UINT_8 ucNetworkIndex;
	BOOLEAN fgIsHiddenSSID;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prNloParam = &prScanInfo->rNloParam;
	prScanParam = &prNloParam->rScanParam;
	prSchedScanRequest = &prScanInfo->rSchedScanRequest;
	ucNetworkIndex = 0;
	fgIsHiddenSSID = FALSE;

	if (prScanInfo->fgNloScanning) {
		DBGLOG(SCN, WARN, "prScanInfo->fgNloScanning == TRUE  already scanning\n");
		return TRUE;
	}

	/*check if normal scanning is true, driver start to postpone sched scan request*/
	prScanInfo->eCurrendSchedScanReq = SCHED_SCAN_POSTPONE_START;

	if (prScanInfo->eCurrentState != SCAN_STATE_IDLE) {
		prScanInfo->fgIsPostponeSchedScan = TRUE;
		DBGLOG(SCN, WARN, "already normal scanning ,driver postpones sched scan request!\n");
		return TRUE;
	}

	prScanInfo->fgIsPostponeSchedScan = FALSE;
	prScanInfo->fgNloScanning = TRUE;
#if CFG_NLO_MSP
	scnSetMspParameterIntoPSCN(prAdapter, prScanInfo->prPscnParam);
#endif

	/* 1. load parameters */
	prScanParam->ucSeqNum++;
	/* prScanParam->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex; */

	prNloParam->fgStopAfterIndication = FALSE;
	prNloParam->ucFastScanIteration = 1;

	if (prSchedScanRequest->u2ScanInterval < SCAN_NLO_DEFAULT_INTERVAL) {
		prSchedScanRequest->u2ScanInterval = SCAN_NLO_DEFAULT_INTERVAL; /* millisecond */
		DBGLOG(SCN, TRACE, "force interval to SCAN_NLO_DEFAULT_INTERVAL\n");
	}
#if !CFG_SUPPORT_SCN_PSCN
#if !CFG_SUPPORT_RLM_ACT_NETWORK
	if (!IS_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX)) {
		SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);

		DBGLOG(SCN, INFO, "ACTIVATE AIS from INACTIVE to enable PNO\n");
		/* sync with firmware */
		nicActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX);
	}
#else
	rlmActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_SCHED_SCAN);
#endif
#endif
	prNloParam->u2FastScanPeriod = SCAN_NLO_MIN_INTERVAL; /* use second instead of millisecond for UINT_16*/
	prNloParam->u2SlowScanPeriod = SCAN_NLO_MAX_INTERVAL;

	if (prScanParam->ucSSIDNum > CFG_SCAN_SSID_MAX_NUM)
		prScanParam->ucSSIDNum = CFG_SCAN_SSID_MAX_NUM;
	else
		prScanParam->ucSSIDNum = prSchedScanRequest->u4SsidNum;

	if (prNloParam->ucMatchSSIDNum > CFG_SCAN_SSID_MATCH_MAX_NUM)
		prNloParam->ucMatchSSIDNum = CFG_SCAN_SSID_MATCH_MAX_NUM;
	else
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
		prNloParam->ucMatchSSIDNum = prSchedScanRequest->u4MatchSsidNum;
#else
		prNloParam->ucMatchSSIDNum = prSchedScanRequest->u4SsidNum;
#endif

	kalMemZero(prNloParam->aucSSID, sizeof(prNloParam->aucSSID));
	kalMemZero(prNloParam->aucMatchSSID, sizeof(prNloParam->aucMatchSSID));

#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	if (prNloParam->ucSSIDNum > CFG_SCAN_HIDDEN_SSID_MAX_NUM)
		prNloParam->ucSSIDNum = CFG_SCAN_HIDDEN_SSID_MAX_NUM;
	else
		prNloParam->ucSSIDNum = prSchedScanRequest->u4SsidNum;

	for (i = 0; i < prNloParam->ucSSIDNum; i++) {
		COPY_SSID(prNloParam->aucSSID[i],
			  prNloParam->ucSSIDLen[i], prSchedScanRequest->arSsid[i].aucSsid,
			  (UINT_8) prSchedScanRequest->arSsid[i].u4SsidLen);
	}
#endif
	for (i = 0; i < prNloParam->ucMatchSSIDNum; i++) {
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS

		if (i < CFG_SCAN_SSID_MAX_NUM) {
			COPY_SSID(prScanParam->aucSpecifiedSSID[i],
				  prScanParam->ucSpecifiedSSIDLen[i], prSchedScanRequest->arMatchSsid[i].aucSsid,
				  (UINT_8) prSchedScanRequest->arMatchSsid[i].u4SsidLen);
		}

		COPY_SSID(prNloParam->aucMatchSSID[i],
			  prNloParam->ucMatchSSIDLen[i], prSchedScanRequest->arMatchSsid[i].aucSsid,
			  (UINT_8) prSchedScanRequest->arMatchSsid[i].u4SsidLen);
#else
		if (i < CFG_SCAN_SSID_MAX_NUM) {
			COPY_SSID(prScanParam->aucSpecifiedSSID[i],
				  prScanParam->ucSpecifiedSSIDLen[i], prSchedScanRequest->arSsid[i].aucSsid,
				  (UINT_8) prSchedScanRequest->arSsid[i].u4SsidLen);
		}

		COPY_SSID(prNloParam->aucMatchSSID[i],
			  prNloParam->ucMatchSSIDLen[i], prSchedScanRequest->arSsid[i].aucSsid,
			  (UINT_8) prSchedScanRequest->arSsid[i].u4SsidLen);
#endif
		prNloParam->aucCipherAlgo[i] = 0;
		prNloParam->au2AuthAlgo[i] = 0;

		for (j = 0; j < SCN_NLO_NETWORK_CHANNEL_NUM; j++)
			prNloParam->aucChannelHint[i][j] = 0;
	}


	/* 2. prepare command for sending */
	prCmdNloReq = (P_CMD_NLO_REQ) cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_NLO_REQ) + prScanParam->u2IELen);

	if (!prCmdNloReq) {
		ASSERT(0);	/* Can't initiate NLO operation */
		return FALSE;
	}

	/* 3. send command packet for NLO operation */
	kalMemZero(prCmdNloReq, sizeof(CMD_NLO_REQ));

	prCmdNloReq->ucSeqNum = prScanParam->ucSeqNum;
	/* prCmdNloReq->ucBssIndex = prScanParam->ucBssIndex; */

	prCmdNloReq->ucNetworkType = prScanParam->eNetTypeIndex;
	/* prCmdNloReq->ucScanType = (UINT_8) prScanParam->eScanType; */ /* sync to firmware */

	prCmdNloReq->fgStopAfterIndication = prNloParam->fgStopAfterIndication;
	prCmdNloReq->ucFastScanIteration = prNloParam->ucFastScanIteration;
	prCmdNloReq->u2FastScanPeriod = prNloParam->u2FastScanPeriod;
	prCmdNloReq->u2SlowScanPeriod = prNloParam->u2SlowScanPeriod;

#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	for (i = 0 ; i < prNloParam->ucSSIDNum; i++) {
		COPY_SSID(prCmdNloReq->arNetworkList[ucNetworkIndex].aucSSID,
			  prCmdNloReq->arNetworkList[ucNetworkIndex].ucSSIDLength,
			  prNloParam->aucSSID[i], prNloParam->ucSSIDLen[i]);

		DBGLOG(SCN, TRACE, "ssid set(%d) %s\n",
			ucNetworkIndex,
			HIDE(prCmdNloReq->arNetworkList[ucNetworkIndex].aucSSID));
		prCmdNloReq->arNetworkList[ucNetworkIndex].ucCipherAlgo
			= prNloParam->aucCipherAlgo[ucNetworkIndex];
		prCmdNloReq->arNetworkList[ucNetworkIndex].u2AuthAlgo
			= prNloParam->au2AuthAlgo[ucNetworkIndex];

		for (j = 0; j < SCN_NLO_NETWORK_CHANNEL_NUM; j++)
			prCmdNloReq->arNetworkList[ucNetworkIndex].ucNumChannelHint[j]
				= prNloParam->aucChannelHint[ucNetworkIndex][j];

		ucNetworkIndex++;
	}


	/*prSchedScanRequest->u4SsidNum +1 ~ prNloParam->ucMatchSSIDNum*/
	for (i = 0 ; i < prNloParam->ucMatchSSIDNum; i++) {
		fgIsHiddenSSID = FALSE;
		for (j = 0 ; j < prNloParam->ucSSIDNum; j++) {
			if (EQUAL_SSID(prCmdNloReq->arNetworkList[j].aucSSID,
			prCmdNloReq->arNetworkList[j].ucSSIDLength,
			prNloParam->aucMatchSSID[i], prNloParam->ucMatchSSIDLen[i])) {
				fgIsHiddenSSID = TRUE;
				break;
			}
		}
		if (ucNetworkIndex >= CFG_SCAN_SSID_MATCH_MAX_NUM) {
			DBGLOG(SCN, TRACE, "ucNetworkIndex %d out of MAX num!\n", ucNetworkIndex);
			break;
		}
		if (!fgIsHiddenSSID && prNloParam->ucMatchSSIDLen[i] != 0) {
			COPY_SSID(prCmdNloReq->arNetworkList[ucNetworkIndex].aucSSID,
				  prCmdNloReq->arNetworkList[ucNetworkIndex].ucSSIDLength,
				  prNloParam->aucMatchSSID[i], prNloParam->ucMatchSSIDLen[i]);

			DBGLOG(SCN, TRACE, "Match set(%d) %s\n"
				, i, prCmdNloReq->arNetworkList[ucNetworkIndex].aucSSID);

			prCmdNloReq->arNetworkList[ucNetworkIndex].ucCipherAlgo
				= prNloParam->aucCipherAlgo[ucNetworkIndex];
			prCmdNloReq->arNetworkList[ucNetworkIndex].u2AuthAlgo
				= prNloParam->au2AuthAlgo[ucNetworkIndex];

			for (j = 0; j < SCN_NLO_NETWORK_CHANNEL_NUM; j++)
				prCmdNloReq->arNetworkList[ucNetworkIndex].ucNumChannelHint[j]
					= prNloParam->aucChannelHint[ucNetworkIndex][j];

			ucNetworkIndex++;
		} else
			DBGLOG(SCN, TRACE, "ignore Match set(%d)%s,beacue it existed in NetworkList.\n"
			, i, prNloParam->aucMatchSSID[i]);

	}

	/*Set uc Entry Num*/
	prCmdNloReq->ucEntryNum = ucNetworkIndex;
	/*ucEntryNum[7] enable FW's support*/
	prCmdNloReq->ucReserved |= 0x80;
	/*ucEntryNum[4:6]: set SSID sets */
	prCmdNloReq->ucReserved |= (prNloParam->ucSSIDNum & 0x07) << 4;

	DBGLOG(SCN, INFO, "ucEntryNum=%d,ucMatchSSIDNum=%d,ucSSIDNum=%d,ucReserved=0x%x,Iteration=%d,Period=%d\n"
		, prCmdNloReq->ucEntryNum
		, prNloParam->ucMatchSSIDNum
		, prNloParam->ucSSIDNum
		, prCmdNloReq->ucReserved
		, prNloParam->ucFastScanIteration
		, prNloParam->u2FastScanPeriod);
#else

	DBGLOG(SCN, INFO, "ucMatchSSIDNum %d, %s, Iteration=%d, FastScanPeriod=%d\n",
		prNloParam->ucMatchSSIDNum, prNloParam->aucMatchSSID[0],
		prNloParam->ucFastScanIteration, prNloParam->u2FastScanPeriod);

	prCmdNloReq->ucEntryNum = prNloParam->ucMatchSSIDNum;
	for (i = 0; i < prNloParam->ucMatchSSIDNum; i++) {
		COPY_SSID(prCmdNloReq->arNetworkList[i].aucSSID,
			  prCmdNloReq->arNetworkList[i].ucSSIDLength,
			  prNloParam->aucMatchSSID[i], prNloParam->ucMatchSSIDLen[i]);

		prCmdNloReq->arNetworkList[i].ucCipherAlgo = prNloParam->aucCipherAlgo[i];
		prCmdNloReq->arNetworkList[i].u2AuthAlgo = prNloParam->au2AuthAlgo[i];

		for (j = 0; j < SCN_NLO_NETWORK_CHANNEL_NUM; j++)
			prCmdNloReq->arNetworkList[i].ucNumChannelHint[j] = prNloParam->aucChannelHint[i][j];
	}
#endif

	if (prScanParam->u2IELen <= MAX_IE_LENGTH)
		prCmdNloReq->u2IELen = prScanParam->u2IELen;
	else
		prCmdNloReq->u2IELen = MAX_IE_LENGTH;

	if (prScanParam->u2IELen)
		kalMemCopy(prCmdNloReq->aucIE, prScanParam->aucIE, sizeof(UINT_8) * prCmdNloReq->u2IELen);

	prCmdNloReq->ucScnFuncMask |= prSchedScanRequest->ucScnFuncMask;

#if !CFG_SUPPORT_SCN_PSCN
	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_NLO_REQ,
			    TRUE,
			    FALSE,
			    FALSE,
			    nicCmdEventSetCommon,
			    nicOidCmdTimeoutCommon,
			    sizeof(CMD_NLO_REQ) + prCmdNloReq->u2IELen, (PUINT_8) prCmdNloReq, NULL, 0);

#else
	scnCombineParamsIntoPSCN(prAdapter, prCmdNloReq, NULL, NULL, NULL, FALSE, FALSE, FALSE);
	scnPSCNFsm(prAdapter, PSCN_RESET);
#endif
	cnmMemFree(prAdapter, (PVOID) prCmdNloReq);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for stopping scheduled scan
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmSchedScanStopRequest(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_NLO_PARAM_T prNloParam;
	P_SCAN_PARAM_T prScanParam;
	CMD_NLO_CANCEL rCmdNloCancel;

	ASSERT(prAdapter);

	/* stop Nlo timeout timer */
	cnmTimerStopTimer(prAdapter, &prAdapter->rScanNloTimeoutTimer);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prNloParam = &prScanInfo->rNloParam;
	prScanParam = &prNloParam->rScanParam;

#if !CFG_SUPPORT_SCN_PSCN
#if !CFG_SUPPORT_RLM_ACT_NETWORK
		if (IS_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX)) {
			UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);

			DBGLOG(SCN, TRACE, "DEACTIVATE AIS to disable PNO\n");
		}
#else
		rlmDeactivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_SCHED_SCAN);
#endif
#endif

	/*check if normal scanning is true, driver start to postpone sched scan stop request*/
	prScanInfo->eCurrendSchedScanReq = SCHED_SCAN_POSTPONE_STOP;

	if (prScanInfo->eCurrentState != SCAN_STATE_IDLE) {
		prScanInfo->fgIsPostponeSchedScan = TRUE;
		DBGLOG(SCN, WARN, "already normal scanning ,driver postpones sched scan stop request!\n");
		return TRUE;
	}
	prScanInfo->fgIsPostponeSchedScan = FALSE;

	/* send cancel message to firmware domain */
	rCmdNloCancel.ucSeqNum = prScanParam->ucSeqNum;

#if !CFG_SUPPORT_SCN_PSCN
	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_NLO_CANCEL,
			    TRUE,
			    FALSE,
			    FALSE,
			    nicCmdEventSetStopSchedScan,
			    nicOidCmdTimeoutCommon, sizeof(CMD_NLO_CANCEL), (PUINT_8)(&rCmdNloCancel), NULL, 0);
#else
	scnCombineParamsIntoPSCN(prAdapter, NULL, NULL, NULL, NULL, TRUE, FALSE, FALSE);
	if (prScanInfo->prPscnParam->fgGScnEnable
		|| prScanInfo->prPscnParam->fgBatchScnEnable)
		scnPSCNFsm(prAdapter, PSCN_RESET); /* in case there is any PSCN */
	else
		scnPSCNFsm(prAdapter, PSCN_IDLE);
#endif

	prScanInfo->fgNloScanning = FALSE;

	return TRUE;
}

#if CFG_SUPPORT_SCN_PSCN
/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for Set PSCN action
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmPSCNAction(IN P_ADAPTER_T prAdapter, IN ENUM_PSCAN_ACT_T ucPscanAct)
{
	P_SCAN_INFO_T prScanInfo;
	CMD_SET_PSCAN_ENABLE rCmdPscnAction;

	DBGLOG(SCN, INFO, "scnFsmPSCNAction Act = %d\n", ucPscanAct);

	kalMemZero(&rCmdPscnAction, sizeof(CMD_SET_PSCAN_ENABLE));

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	if (ucPscanAct == PSCAN_ACT_ENABLE) {
		prScanInfo->fgPscnOngoing = TRUE;
		rCmdPscnAction.ucPscanAct = 0;
	} else {
		prScanInfo->fgPscnOngoing = FALSE;
		rCmdPscnAction.ucPscanAct = 1; /* sync to firmware, 1 means disable, 0 means enable */
	}

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_SET_PSCN_ENABLE,
			    TRUE,
			    FALSE,
			    FALSE,
			    nicCmdEventSetCommon,
			    nicOidCmdTimeoutCommon,
			    sizeof(CMD_SET_PSCAN_ENABLE), (PUINT_8)&rCmdPscnAction, NULL, 0);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Set PSCN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmPSCNSetParam(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam)
{
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	DBGLOG(SCN, INFO, "fgNLOScnEnable=%d %d %d, basePeriod=%d\n",
		prCmdPscnParam->fgNLOScnEnable, prCmdPscnParam->fgBatchScnEnable,
		prCmdPscnParam->fgGScnEnable, prCmdPscnParam->u4BasePeriod);

	if (1 /*prScanInfo->fgPscnOngoing == FALSE */) {
		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_PSCAN_PARAM,
				    TRUE,
				    FALSE,
				    FALSE,
				    nicCmdEventSetCommon,
				    nicOidCmdTimeoutCommon,
				    sizeof(CMD_SET_PSCAN_PARAM), (PUINT_8) prCmdPscnParam, NULL, 0);

		return TRUE;
	}
	return FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Set hotlist
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmPSCNSetHotlist(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_ADD_HOTLIST_BSSID prCmdPscnAddHotlist)
{
	CMD_SET_PSCAN_ADD_HOTLIST_BSSID rCmdPscnAddHotlist;
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	memcpy(&rCmdPscnAddHotlist.aucMacAddr, &(prCmdPscnAddHotlist->aucMacAddr), sizeof(MAC_ADDR_LEN));

	/* rCmdPscnAddHotlist.aucMacAddr = prCmdPscnAddHotlist->aucMacAddr; */
	rCmdPscnAddHotlist.ucFlags = prCmdPscnAddHotlist->ucFlags;

	if (prScanInfo->fgPscnOngoing && prScanInfo->prPscnParam->fgGScnEnable) {
		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_PSCN_ADD_HOTLIST_BSSID,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    nicOidCmdTimeoutCommon,
				    sizeof(CMD_SET_PSCAN_ADD_HOTLIST_BSSID), (PUINT_8)&rCmdPscnAddHotlist, NULL, 0);
		return TRUE;
	}
	/* debug msg, No PSCN, Sched SCAN no need to add the hotlist ??? */
	return FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for Set CMD_ID_SET_PSCN_ADD_SW_BSSID
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmPSCNAddSWCBssId(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_ADD_SWC_BSSID prCmdPscnAddSWCBssId)
{
	CMD_SET_PSCAN_ADD_SWC_BSSID rCmdPscnAddSWCBssId;
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	memcpy(&rCmdPscnAddSWCBssId.aucMacAddr, &(prCmdPscnAddSWCBssId->aucMacAddr), sizeof(MAC_ADDR_LEN));

	/* rCmdPscnAddSWCBssId.aucMacAddr = prCmdPscnAddSWCBssId->aucMacAddr; */
	rCmdPscnAddSWCBssId.i4RssiHighThreshold = prCmdPscnAddSWCBssId->i4RssiHighThreshold;
	rCmdPscnAddSWCBssId.i4RssiLowThreshold = prCmdPscnAddSWCBssId->i4RssiLowThreshold;

	if (prScanInfo->fgPscnOngoing && prScanInfo->prPscnParam->fgGScnEnable) {
		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_PSCN_ADD_SW_BSSID,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    nicOidCmdTimeoutCommon,
				    sizeof(CMD_SET_PSCAN_ADD_SWC_BSSID), (PUINT_8)&rCmdPscnAddSWCBssId, NULL, 0);
		return TRUE;
	}
	/* debug msg, No PSCN, Sched SCAN no need to add the hotlist ??? */
	return FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for Set CMD_ID_SET_PSCN_MAC_ADDR
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmPSCNSetMacAddr(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_MAC_ADDR prCmdPscnSetMacAddr)
{
	CMD_SET_PSCAN_MAC_ADDR rCmdPscnSetMacAddr;
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	/* rCmdPscnSetMacAddr.aucMacAddr = prCmdPscnSetMacAddr->aucMacAddr; */
	memcpy(&rCmdPscnSetMacAddr.aucMacAddr, &(prCmdPscnSetMacAddr->aucMacAddr), sizeof(MAC_ADDR_LEN));

	rCmdPscnSetMacAddr.ucFlags = prCmdPscnSetMacAddr->ucFlags;
	rCmdPscnSetMacAddr.ucVersion = prCmdPscnSetMacAddr->ucVersion;

	if (1 /* (prScanInfo->fgPscnOngoing == TRUE */) {
		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_PSCN_MAC_ADDR,
				    TRUE,
				    FALSE,
				    FALSE,
				    NULL,
				    nicOidCmdTimeoutCommon,
				    sizeof(CMD_SET_PSCAN_MAC_ADDR), (PUINT_8)&rCmdPscnSetMacAddr, NULL, 0);
		return TRUE;
	}
	/* debug msg, No PSCN, Sched SCAN no need to add the hotlist ??? */
	return FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Combine PNO Scan params into PSCAN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
scnSubCombineNLOtoPSCN(IN P_ADAPTER_T prAdapter, IN P_CMD_NLO_REQ prNewCmdNloReq)
{
	P_SCAN_INFO_T prScanInfo;
	P_CMD_SET_PSCAN_PARAM prCmdPscnParam;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCmdPscnParam = (P_CMD_SET_PSCAN_PARAM) prScanInfo->prPscnParam;

	if (prNewCmdNloReq) {
		prCmdPscnParam->fgNLOScnEnable = TRUE;
		kalMemCopy(&(prCmdPscnParam->rCmdNloReq), prNewCmdNloReq, sizeof(CMD_NLO_REQ));
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Combine Batch Scan params into PSCAN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
scnSubCombineBatchSCNtoPSCN(IN P_ADAPTER_T prAdapter, IN P_CMD_BATCH_REQ_T prNewCmdBatchReq)
{
	P_SCAN_INFO_T prScanInfo;
	P_CMD_SET_PSCAN_PARAM prCmdPscnParam;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCmdPscnParam = (P_CMD_SET_PSCAN_PARAM) prScanInfo->prPscnParam;

	if (prNewCmdBatchReq) {
		prCmdPscnParam->fgBatchScnEnable = TRUE;
		kalMemCopy(&(prCmdPscnParam->rCmdBatchReq), prNewCmdBatchReq, sizeof(CMD_BATCH_REQ_T));
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Combine GSCN Scan params into PSCAN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
scnSubCombineGSCNtoPSCN(IN P_ADAPTER_T prAdapter,
			IN P_CMD_GSCN_REQ_T prNewCmdGscnReq, IN P_CMD_GSCN_SCN_COFIG_T prNewCmdGscnConfig)
{
	P_SCAN_INFO_T prScanInfo;
	P_CMD_SET_PSCAN_PARAM prCmdPscnParam;
	UINT_32 ucPeriodMin = MAX_PERIOD;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCmdPscnParam = (P_CMD_SET_PSCAN_PARAM) prScanInfo->prPscnParam;
	prCmdPscnParam->fgGScnEnable = FALSE;

	DBGLOG(SCN, TRACE, "scnSubCombineGSCNtoPSCN fgGScnParamSet %d fgGScnConfigSet %d\n",
		prScanInfo->fgGScnParamSet, prScanInfo->fgGScnConfigSet);

	if (prNewCmdGscnReq) {
		DBGLOG(SCN, INFO, "setup prNewCmdGscnReq\n");
		prScanInfo->fgGScnParamSet = TRUE;
		kalMemCopy(&(prCmdPscnParam->rCmdGscnReq), prNewCmdGscnReq, sizeof(CMD_GSCN_REQ_T));
		if (prNewCmdGscnReq->u4BasePeriod < ucPeriodMin)
			prCmdPscnParam->u4BasePeriod = prNewCmdGscnReq->u4BasePeriod;
	} else if (prScanInfo->fgGScnParamSet) {
		DBGLOG(SCN, INFO, "no new prNewCmdGscnReq but there is an old one\n");
	}

	if (prNewCmdGscnConfig) {
		DBGLOG(SCN, INFO, "setup prNewCmdGscnConfig\n");
		prScanInfo->fgGScnConfigSet = TRUE;
		prCmdPscnParam->fgGScnEnable = TRUE;
		prCmdPscnParam->rCmdGscnReq.u4MaxApPerScan = prNewCmdGscnConfig->ucNumApPerScn;
		prCmdPscnParam->rCmdGscnReq.u4BufferThreshold = prNewCmdGscnConfig->u4BufferThreshold;
		prCmdPscnParam->rCmdGscnReq.ucNumScnToCache = (UINT_8) prNewCmdGscnConfig->u4NumScnToCache;
	} else if (prScanInfo->fgGScnConfigSet) {
		DBGLOG(SCN, INFO, "no new prNewCmdGscnConfig but there is an old one\n");
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Combine   GSCN Scan params into PSCAN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
scnRemoveFromPSCN(IN P_ADAPTER_T prAdapter,
		  IN BOOLEAN fgRemoveNLOfromPSCN,
		  IN BOOLEAN fgRemoveBatchSCNfromPSCN,
		  IN BOOLEAN fgRemoveGSCNfromPSCN)
{
	P_SCAN_INFO_T prScanInfo;
	P_CMD_SET_PSCAN_PARAM prCmdPscnParam;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCmdPscnParam = (P_CMD_SET_PSCAN_PARAM) prScanInfo->prPscnParam;

	DBGLOG(SCN, INFO, "remove NLO or Batch or GSCN from PSCN--->NLO=%d, BSN=%d, GSN=%d\n",
		fgRemoveNLOfromPSCN, fgRemoveBatchSCNfromPSCN, fgRemoveGSCNfromPSCN);

	if (fgRemoveNLOfromPSCN) {
		prCmdPscnParam->fgNLOScnEnable = FALSE;
		kalMemZero(&prCmdPscnParam->rCmdNloReq, sizeof(CMD_NLO_REQ));
	}
	if (fgRemoveBatchSCNfromPSCN) {
		prCmdPscnParam->fgBatchScnEnable = FALSE;
		kalMemZero(&prCmdPscnParam->rCmdBatchReq, sizeof(CMD_BATCH_REQ_T));
	}
	if (fgRemoveGSCNfromPSCN) {
		prCmdPscnParam->fgGScnEnable = FALSE;
		prScanInfo->fgGScnParamSet = FALSE;
		prScanInfo->fgGScnConfigSet = FALSE;
		prScanInfo->fgGScnAction = FALSE;
		kalMemZero(&prCmdPscnParam->rCmdGscnReq, sizeof(CMD_GSCN_REQ_T));
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Combine GSCN , Batch, PNO Scan params into PSCAN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
scnCombineParamsIntoPSCN(IN P_ADAPTER_T prAdapter,
			 IN P_CMD_NLO_REQ prNewCmdNloReq,
			 IN P_CMD_BATCH_REQ_T prNewCmdBatchReq,
			 IN P_CMD_GSCN_REQ_T prNewCmdGscnReq,
			 IN P_CMD_GSCN_SCN_COFIG_T prNewCmdGscnConfig,
			 IN BOOLEAN fgRemoveNLOfromPSCN,
			 IN BOOLEAN fgRemoveBatchSCNfromPSCN, IN BOOLEAN fgRemoveGSCNfromPSCN)
{
	P_SCAN_INFO_T prScanInfo;
	P_CMD_SET_PSCAN_PARAM prCmdPscnParam;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCmdPscnParam = (P_CMD_SET_PSCAN_PARAM) prScanInfo->prPscnParam;

	prCmdPscnParam->ucVersion = CURRENT_PSCN_VERSION;

	if (fgRemoveNLOfromPSCN || fgRemoveBatchSCNfromPSCN || fgRemoveGSCNfromPSCN) {
		scnRemoveFromPSCN(prAdapter,
				  fgRemoveNLOfromPSCN, fgRemoveBatchSCNfromPSCN, fgRemoveGSCNfromPSCN);
	} else {
		DBGLOG(SCN, TRACE, "combine GSCN or Batch or NLO to PSCN --->\n");

		scnSubCombineNLOtoPSCN(prAdapter, prNewCmdNloReq);
		scnSubCombineBatchSCNtoPSCN(prAdapter, prNewCmdBatchReq);
		scnSubCombineGSCNtoPSCN(prAdapter, prNewCmdGscnReq, prNewCmdGscnConfig);
	}

	return TRUE;
}

#if CFG_NLO_MSP

/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for setting MSP parameter to PSCAN
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/

VOID
scnSetMspParameterIntoPSCN(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam)
{
	DBGLOG(SCN, TRACE, "--> %s()\n", __func__);

	ASSERT(prAdapter);

#if 0
	prCmdPscnParam->rCmdNloReq.fgNLOMspEnable = 1;
	prCmdPscnParam->rCmdNloReq.ucNLOMspEntryNum = 10;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[0] = 120;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[1] = 120;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[2] = 240;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[3] = 240;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[4] = 480;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[5] = 480;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[6] = 960;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[7] = 960;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[8] = 960;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[9] = 960;
#else
	/* quick test configuration */
	prCmdPscnParam->rCmdNloReq.fgNLOMspEnable = 1;
	prCmdPscnParam->rCmdNloReq.ucNLOMspEntryNum = 10;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[0] = 10;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[1] = 10;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[2] = 10;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[3] = 15;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[4] = 15;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[5] = 15;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[6] = 20;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[7] = 20;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[8] = 20;
	prCmdPscnParam->rCmdNloReq.au2NLOMspList[9] = 25;
#endif

}

#endif



VOID scnPSCNFsm(IN P_ADAPTER_T prAdapter, IN ENUM_PSCAN_STATE_T eNextPSCNState)
{
	P_SCAN_INFO_T prScanInfo;
	BOOLEAN fgTransitionState = FALSE;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	do {
		fgTransitionState = FALSE;

		DBGLOG(SCN, STATE, "eCurrentPSCNState=%d, eNextPSCNState=%d\n",
			prScanInfo->eCurrentPSCNState, eNextPSCNState);

		prScanInfo->eCurrentPSCNState = eNextPSCNState;

		switch (prScanInfo->eCurrentPSCNState) {
		case PSCN_IDLE:
			DBGLOG(SCN, TRACE, "PSCN_IDLE.... PSCAN_ACT_DISABLE\n");
#if CFG_SUPPORT_RLM_ACT_NETWORK
			rlmDeactivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_SCHED_SCAN);
#endif
			scnFsmPSCNAction(prAdapter, PSCAN_ACT_DISABLE);
			eNextPSCNState = PSCN_IDLE;
			break;

		case PSCN_RESET:
			DBGLOG(SCN, TRACE, "PSCN_RESET.... PSCAN_ACT_DISABLE\n");
#if CFG_SUPPORT_RLM_ACT_NETWORK
			rlmDeactivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_SCHED_SCAN);
#endif
			scnFsmPSCNAction(prAdapter, PSCAN_ACT_DISABLE);
			scnFsmPSCNSetParam(prAdapter, prScanInfo->prPscnParam);

			if (prScanInfo->prPscnParam->fgNLOScnEnable
				|| prScanInfo->prPscnParam->fgBatchScnEnable
				|| (prScanInfo->prPscnParam->fgGScnEnable && prScanInfo->fgGScnAction)) {
				eNextPSCNState = PSCN_SCANNING; /* keep original operation if there is any PSCN */
				DBGLOG(SCN, TRACE,
				       "PSCN_RESET->PSCN_SCANNING....fgNLOScnEnable/fgBatchScnEnable/fgGScnEnable ENABLE\n");
			} else {
				/* eNextPSCNState = PSCN_RESET; */
				DBGLOG(SCN, TRACE,
				       "PSCN_RESET->PSCN_RESET....fgNLOScnEnable/fgBatchScnEnable/fgGScnEnable DISABLE\n");
			}
			break;

		case PSCN_SCANNING:
			DBGLOG(SCN, TRACE, "PSCN_SCANNING.... PSCAN_ACT_ENABLE\n");
			if (prScanInfo->fgPscnOngoing)
				break;
#if !CFG_SUPPORT_RLM_ACT_NETWORK
			if (!IS_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX)) {
				SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_AIS_INDEX);
				DBGLOG(SCN, TRACE, "ACTIVATE AIS to enable PSCN\n");
				/* sync with firmware */
				nicActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX);
			}
#else
			rlmActivateNetwork(prAdapter, NETWORK_TYPE_AIS_INDEX, NET_ACTIVE_SRC_SCHED_SCAN);
#endif

			scnFsmPSCNAction(prAdapter, PSCAN_ACT_ENABLE);
			prScanInfo->fgPscnOngoing = TRUE;
			eNextPSCNState = PSCN_SCANNING;
			break;

		default:
			DBGLOG(SCN, WARN, "Unexpected state\n");
			ASSERT(0);
			break;
		}

		if (prScanInfo->eCurrentPSCNState != eNextPSCNState)
			fgTransitionState = TRUE;

	} while (fgTransitionState);

}
#endif

#if CFG_SUPPORT_GSCN
/*----------------------------------------------------------------------------*/
/*!
* \brief        handler for Set GSCN param
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnSetGSCNParam(IN P_ADAPTER_T prAdapter, IN P_PARAM_WIFI_GSCAN_CMD_PARAMS prCmdGscnParam)
{
	P_CMD_GSCN_REQ_T prCmdGscnReq;
	UINT_8 i = 0, j = 0; /*ucBucketIndex, ucChIndex*/

	ASSERT(prAdapter);
	prCmdGscnReq = kalMemAlloc(sizeof(CMD_GSCN_REQ_T), VIR_MEM_TYPE);
	if (!prCmdGscnReq) {
		DBGLOG(SCN, ERROR, "alloc prCmdGscnReq fail\n");
		return FALSE;
	}
	kalMemZero(prCmdGscnReq, sizeof(CMD_GSCN_REQ_T));
	prCmdGscnReq->u4NumBuckets = prCmdGscnParam->num_buckets;
	prCmdGscnReq->u4BasePeriod = prCmdGscnParam->base_period;
	DBGLOG(SCN, TRACE, "u4BasePeriod[%d], u4NumBuckets[%d]\n",
		prCmdGscnReq->u4BasePeriod, prCmdGscnReq->u4NumBuckets);

	for (i = 0; i < prCmdGscnReq->u4NumBuckets; i++) {
		prCmdGscnReq->arBucket[i].u2BucketIndex =
		    (UINT_16) prCmdGscnParam->buckets[i].bucket;
		prCmdGscnReq->arBucket[i].eBand = prCmdGscnParam->buckets[i].band;
		prCmdGscnReq->arBucket[i].ucBucketFreqMultiple =
		    (prCmdGscnParam->buckets[i].period / prCmdGscnParam->base_period);
		prCmdGscnReq->arBucket[i].ucReportFlag = prCmdGscnParam->buckets[i].report_events;
		prCmdGscnReq->arBucket[i].ucMaxBucketFreqMultiple =
		    (prCmdGscnParam->buckets[i].max_period / prCmdGscnParam->base_period);
		prCmdGscnReq->arBucket[i].ucStepCount = (UINT_8)prCmdGscnParam->buckets[i].step_count;

		prCmdGscnReq->arBucket[i].ucNumChannels =
		    (UINT_8)prCmdGscnParam->buckets[i].num_channels;
		DBGLOG(SCN, TRACE, "assign %d channels to bucket[%d]\n",
			prCmdGscnReq->arBucket[i].ucNumChannels,	i);
		for (j = 0; j < prCmdGscnParam->buckets[i].num_channels; i++) {
			prCmdGscnReq->arBucket[i].arChannelList[j].ucChannelNumber =
			    (UINT_8) nicFreq2ChannelNum(prCmdGscnParam->buckets[i].channels[j].channel * 1000);
			prCmdGscnReq->arBucket[i].arChannelList[j].ucPassive =
			    (UINT_8) prCmdGscnParam->buckets[i].channels[j].passive;
			prCmdGscnReq->arBucket[i].arChannelList[j].u4DwellTimeMs =
			    prCmdGscnParam->buckets[i].channels[j].dwellTimeMs;

			DBGLOG(SCN, TRACE, "[ucChannel %d, ucPassive %d, u4DwellTimeMs %d\n",
			       prCmdGscnReq->arBucket[i].arChannelList[j].ucChannelNumber,
			       prCmdGscnReq->arBucket[i].arChannelList[j].ucPassive,
			       prCmdGscnReq->arBucket[i].arChannelList[j].u4DwellTimeMs);

		}

	}

	scnCombineParamsIntoPSCN(prAdapter, NULL, NULL, prCmdGscnReq, NULL, FALSE, FALSE, FALSE);
	scnPSCNFsm(prAdapter, PSCN_RESET);

	kalMemFree(prCmdGscnReq, VIR_MEM_TYPE, sizeof(CMD_GSCN_REQ_T));
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for scnSetGSCNConfig
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnSetGSCNConfig(IN P_ADAPTER_T prAdapter, IN P_CMD_GSCN_SCN_COFIG_T prCmdGscnScnConfig)
{
	ASSERT(prAdapter);
	ASSERT(prCmdGscnScnConfig);

	scnCombineParamsIntoPSCN(prAdapter, NULL, NULL, NULL, prCmdGscnScnConfig, FALSE, FALSE, FALSE);
	scnPSCNFsm(prAdapter, PSCN_RESET);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief         handler for scnFsmGetGSCNResult
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN scnFsmGetGSCNResult(IN P_ADAPTER_T prAdapter, IN P_CMD_GET_GSCAN_RESULT_T prGetGscnResultCmd,
			    OUT PUINT_32 pu4SetInfoLen)
{
	CMD_GET_GSCAN_RESULT_T rGetGscnResultCmd;
	P_SCAN_INFO_T prScanInfo;
	P_PARAM_WIFI_GSCAN_RESULT_REPORT prGscnResult;
	struct wiphy *wiphy;
	UINT_32 u4SizeofGScanResults = 0;
	UINT_8 ucBkt;
	static UINT_8 scanId, numAp;

	ASSERT(prAdapter);
	wiphy = priv_to_wiphy(prAdapter->prGlueInfo);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	kalMemCopy(&rGetGscnResultCmd, prGetGscnResultCmd, sizeof(CMD_GET_GSCAN_RESULT_T));
	DBGLOG(SCN, INFO, "rGetGscnResultCmd: ucGetNum[%d], fgFlush[%d]\n",
		rGetGscnResultCmd.u4Num, rGetGscnResultCmd.ucFlush);
	if ((rGetGscnResultCmd.u4Num == 0) || (rGetGscnResultCmd.u4Num > PSCAN_MAX_AP_CACHE_PER_SCAN))
		rGetGscnResultCmd.u4Num = PSCAN_MAX_AP_CACHE_PER_SCAN;

#if 0 /* get GScan results from firmware */
	if (prScanInfo->fgPscnOngoing && prScanInfo->prPscnParam->fgGScnEnable) {
		wlanSendSetQueryCmd(prAdapter,
				CMD_ID_GET_GSCN_SCN_RESULT,
				FALSE,
				TRUE,
				FALSE,
				NULL,
				nicOidCmdTimeoutCommon,
				sizeof(CMD_GET_GSCAN_RESULT_T), (PUINT_8)&rGetGscnResultCmd, NULL, *pu4SetInfoLen);
		return TRUE;
	}
#else /* get GScan results from driver */
	scanId++;
	u4SizeofGScanResults = sizeof(PARAM_WIFI_GSCAN_RESULT_REPORT)
			+ sizeof(PARAM_WIFI_GSCAN_RESULT) * (rGetGscnResultCmd.u4Num - 1);
	prGscnResult = kalMemAlloc(u4SizeofGScanResults, VIR_MEM_TYPE);
	if (!prGscnResult) {
		DBGLOG(SCN, ERROR, "Can not alloc memory for PARAM_WIFI_GSCAN_RESULT_REPORT\n");
		return FALSE;
	}
	kalMemZero(prGscnResult, u4SizeofGScanResults);

	prGscnResult->u4ScanId = scanId;
	prGscnResult->ucScanFlag = 3;
	for (ucBkt = 0; ucBkt < GSCAN_MAX_BUCKETS; ucBkt++)
		if (prScanInfo->prPscnParam->rCmdGscnReq.arBucket[ucBkt].ucReportFlag == REPORT_EVENTS_EACH_SCAN
		|| prScanInfo->prPscnParam->rCmdGscnReq.arBucket[ucBkt].ucReportFlag == REPORT_EVENTS_FULL_RESULTS)
			prGscnResult->u4BucketMask |= (1 << ucBkt);

	/* copy scan results */
	{
		P_PARAM_BSSID_EX_T prScanResults;
		UINT_8 i = 0, remainAp = 0;

		if (numAp < prAdapter->rWlanInfo.u4ScanResultNum)
			remainAp = prAdapter->rWlanInfo.u4ScanResultNum - numAp;
		else {
			kalMemFree(prGscnResult, VIR_MEM_TYPE, u4SizeofGScanResults);
			return FALSE;
		}
		rGetGscnResultCmd.u4Num =
			(rGetGscnResultCmd.u4Num <= remainAp) ? rGetGscnResultCmd.u4Num : remainAp;

		DBGLOG(SCN, TRACE, "u4Num=%d, numAp=%d\n", rGetGscnResultCmd.u4Num, numAp);

		for (i = 0; i < rGetGscnResultCmd.u4Num; i++) {
			prScanResults = &(prAdapter->rWlanInfo.arScanResult[numAp]);
			prGscnResult->rResult[i].ts = kalGetBootTime();
			if (prScanResults->rSsid.u4SsidLen <= ELEM_MAX_LEN_SSID)
				kalMemCopy(prGscnResult->rResult[i].ssid,
					prScanResults->rSsid.aucSsid,
					prScanResults->rSsid.u4SsidLen);
			kalMemCopy(prGscnResult->rResult[i].bssid,
				prScanResults->arMacAddress, MAC_ADDR_LEN);
			prGscnResult->rResult[i].channel = prScanResults->rConfiguration.u4DSConfig / 1000;
			prGscnResult->rResult[i].rssi = prScanResults->rRssi;
			prGscnResult->rResult[i].rtt = 0;
			prGscnResult->rResult[i].rtt_sd = 0;
			prGscnResult->rResult[i].beacon_period = prScanResults->rConfiguration.u4BeaconPeriod;
			prGscnResult->rResult[i].capability = prScanResults->u2CapInfo;
			prGscnResult->rResult[i].ie_length = prScanResults->u4IELength;
			/* if (prScanResults->u4IELength <= CFG_RAW_BUFFER_SIZE)
			*	prGscnResult->rResult[i].ie_data = prAdapter->rWlanInfo.apucScanResultIEs[numAp];
			*/

			numAp++;
			DBGLOG(SCN, TRACE, "Report GScan SSID[%s][%d][" MACSTR "] u4IELength=%d u2CapInfo=0x%x\n",
					HIDE(prGscnResult->rResult[i].ssid), prGscnResult->rResult[i].channel,
					MAC2STR(prGscnResult->rResult[i].bssid), prScanResults->u4IELength,
					prScanResults->u2CapInfo);
		}
		if (numAp >= prAdapter->rWlanInfo.u4ScanResultNum)
			numAp = 0;
	}

	prGscnResult->u4NumOfResults = rGetGscnResultCmd.u4Num;
	u4SizeofGScanResults += sizeof(struct nlattr) * 2;
	DBGLOG(SCN, INFO, "scan_id=%d, scan_flag=0x%x, 0x%x, num=%d %d %d, u4SizeofGScanResults=%d\r\n",
		prGscnResult->u4ScanId,
		prGscnResult->ucScanFlag,
		prGscnResult->u4BucketMask, numAp,
		prGscnResult->u4NumOfResults, prAdapter->rWlanInfo.u4ScanResultNum,
		u4SizeofGScanResults);

	if (numAp == rGetGscnResultCmd.u4Num) /* start transfer*/
		mtk_cfg80211_vendor_gscan_results(wiphy, prAdapter->prGlueInfo->prDevHandler->ieee80211_ptr,
			prGscnResult, u4SizeofGScanResults, TRUE, FALSE);
	mtk_cfg80211_vendor_gscan_results(wiphy, prAdapter->prGlueInfo->prDevHandler->ieee80211_ptr,
		prGscnResult, u4SizeofGScanResults, FALSE, FALSE);
	if (numAp == 0) /* end transfer */
		mtk_cfg80211_vendor_gscan_results(wiphy, prAdapter->prGlueInfo->prDevHandler->ieee80211_ptr,
			prGscnResult, u4SizeofGScanResults, TRUE, TRUE);

	kalMemFree(prGscnResult, VIR_MEM_TYPE, u4SizeofGScanResults - sizeof(struct nlattr) * 2);

#endif /* get GScan results from driver */

	return TRUE;
}

BOOLEAN scnFsmGSCNResults(IN P_ADAPTER_T prAdapter, IN P_EVENT_GSCAN_RESULT_T prEventBuffer)
{
	P_PARAM_WIFI_GSCAN_RESULT_REPORT prGscnResult;
	struct wiphy *wiphy;
	UINT_32 u4SizeofGScanResults = 0;

	prGscnResult = kalMemAlloc(sizeof(PARAM_WIFI_GSCAN_RESULT_REPORT), VIR_MEM_TYPE);
	if (!prGscnResult) {
		DBGLOG(SCN, ERROR, "Can not alloc memory for PARAM_WIFI_GSCAN_RESULT_REPORT\n");
		return FALSE;
	}

	prGscnResult->u4ScanId = (UINT_32)prEventBuffer->u2ScanId;
	prGscnResult->ucScanFlag = (UINT_8)prEventBuffer->u2ScanFlags;
	prGscnResult->u4BucketMask = 1;
	prGscnResult->u4NumOfResults = (UINT_32)prEventBuffer->u2NumOfResults;

	/* PARAM_WIFI_GSCAN_RESULT similar to  WIFI_GSCAN_RESULT_T*/
	kalMemCopy(prGscnResult->rResult, prEventBuffer->rResult,
		sizeof(PARAM_WIFI_GSCAN_RESULT) * (prGscnResult->u4NumOfResults));

	u4SizeofGScanResults = sizeof(PARAM_WIFI_GSCAN_RESULT_REPORT) + sizeof(struct nlattr) * 2
		+ sizeof(PARAM_WIFI_GSCAN_RESULT) * (prGscnResult->u4NumOfResults - 1);
	DBGLOG(SCN, INFO, "scan_id=%d, scan_flag=0x%x, 0x%x, num=%d, u4SizeofGScanResults=%d\r\n",
		prGscnResult->u4ScanId,
		(UINT_32)prGscnResult->ucScanFlag,
		prGscnResult->u4BucketMask,
		prGscnResult->u4NumOfResults,
		u4SizeofGScanResults);

	wiphy = priv_to_wiphy(prAdapter->prGlueInfo);
	mtk_cfg80211_vendor_gscan_results(wiphy, prAdapter->prGlueInfo->prDevHandler->ieee80211_ptr,
		prGscnResult, u4SizeofGScanResults, FALSE, FALSE);

	kalMemFree(prGscnResult, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_RESULT_REPORT));

	return TRUE;
}
#endif

