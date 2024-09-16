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
#include "precomp.h"
#include "p2p_dev_state.h"
#if CFG_ENABLE_WIFI_DIRECT

#if 1
/*lint -save -e64 Type mismatch */
static uint8_t *apucDebugP2pDevState[P2P_DEV_STATE_NUM] = {
	(uint8_t *) DISP_STRING("P2P_DEV_STATE_IDLE"),
	(uint8_t *) DISP_STRING("P2P_DEV_STATE_SCAN"),
	(uint8_t *) DISP_STRING("P2P_DEV_STATE_REQING_CHANNEL"),
	(uint8_t *) DISP_STRING("P2P_DEV_STATE_CHNL_ON_HAND"),
	(uint8_t *) DISP_STRING("P2P_DEV_STATE_OFF_CHNL_TX")
};

/*lint -restore */
#endif /* DBG */

uint8_t p2pDevFsmInit(IN struct ADAPTER *prAdapter)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo =
		(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		ASSERT_BREAK(prP2pDevFsmInfo != NULL);

		kalMemZero(prP2pDevFsmInfo, sizeof(struct P2P_DEV_FSM_INFO));

		prP2pDevFsmInfo->eCurrentState = P2P_DEV_STATE_IDLE;

		cnmTimerInitTimer(prAdapter,
			&(prP2pDevFsmInfo->rP2pFsmTimeoutTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pDevFsmRunEventTimeout,
			(unsigned long) prP2pDevFsmInfo);

		prP2pBssInfo =
			cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_P2P, TRUE);

		if (prP2pBssInfo != NULL) {
			COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr,
					prAdapter->rWifiVar.aucDeviceAddress);
			DBGLOG(INIT, INFO, "Set p2p dev mac to " MACSTR "\n",
					MAC2STR(prP2pBssInfo->aucOwnMacAddr));

			prP2pDevFsmInfo->ucBssIndex = prP2pBssInfo->ucBssIndex;

			prP2pBssInfo->eCurrentOPMode = OP_MODE_P2P_DEVICE;
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
			prP2pBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;

			prP2pBssInfo->eBand = BAND_2G4;
#if (CFG_HW_WMM_BY_BSS == 1)
			prP2pBssInfo->ucWmmQueSet = MAX_HW_WMM_INDEX;
#else
			if (prAdapter->rWifiVar.eDbdcMode
				== ENUM_DBDC_MODE_DISABLED)
				prP2pBssInfo->ucWmmQueSet = DBDC_5G_WMM_INDEX;
			else
				prP2pBssInfo->ucWmmQueSet = DBDC_2G_WMM_INDEX;
#endif

#if CFG_SUPPORT_VHT_IE_IN_2G
			if (prAdapter->rWifiVar.ucVhtIeIn2g)
				prP2pBssInfo->ucPhyTypeSet =
					prAdapter->rWifiVar.
					ucAvailablePhyTypeSet
					& (PHY_TYPE_SET_802_11GN |
					PHY_TYPE_SET_802_11AC);
			else
#endif
				prP2pBssInfo->ucPhyTypeSet =
					prAdapter->rWifiVar.
					ucAvailablePhyTypeSet
					& PHY_TYPE_SET_802_11GN;

			prP2pBssInfo->ucNonHTBasicPhyType = (uint8_t)
			    rNonHTApModeAttributes
			    [prP2pBssInfo->ucConfigAdHocAPMode]
				.ePhyTypeIndex;

			prP2pBssInfo->u2BSSBasicRateSet =
			    rNonHTApModeAttributes
			    [prP2pBssInfo->ucConfigAdHocAPMode]
				.u2BSSBasicRateSet;

			prP2pBssInfo->u2OperationalRateSet =
			    rNonHTPhyAttributes
			    [prP2pBssInfo->ucNonHTBasicPhyType]
				.u2SupportedRateSet;

			prP2pBssInfo->u4PrivateData = 0;/* TH3 Huang */

			rateGetDataRatesFromRateSet(
				prP2pBssInfo->u2OperationalRateSet,
				prP2pBssInfo->u2BSSBasicRateSet,
				prP2pBssInfo->aucAllSupportedRates,
				&prP2pBssInfo->ucAllSupportedRatesLen);
		}
		prP2pChnlReqInfo = &prP2pDevFsmInfo->rChnlReqInfo;
		LINK_INITIALIZE(&prP2pChnlReqInfo->rP2pChnlReqLink);

		prP2pMgmtTxReqInfo = &prP2pDevFsmInfo->rMgmtTxInfo;
		LINK_INITIALIZE(&prP2pMgmtTxReqInfo->rTxReqLink);

		p2pDevFsmStateTransition(prAdapter,
			prP2pDevFsmInfo,
			P2P_DEV_STATE_IDLE);
	} while (FALSE);

	if (prP2pBssInfo)
		return prP2pBssInfo->ucBssIndex;
	else
		return prAdapter->ucP2PDevBssIdx + 1;

#if 0
	do {
		ASSERT_BREAK(prAdapter != NULL);

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo =
		&(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		ASSERT_BREAK(prP2pFsmInfo != NULL);

		LINK_INITIALIZE(&(prP2pFsmInfo->rMsgEventQueue));

		prP2pFsmInfo->eCurrentState =
			prP2pFsmInfo->ePreviousState = P2P_STATE_IDLE;

		prP2pFsmInfo->prTargetBss = NULL;

		cnmTimerInitTimer(prAdapter,
			&(prP2pFsmInfo->rP2pFsmTimeoutTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventFsmTimeout,
			(unsigned long) prP2pFsmInfo);

		/* 4 <2> Initiate BSS_INFO_T - common part */
		BSS_INFO_INIT(prAdapter, NETWORK_TYPE_P2P_INDEX);

		/* 4 <2.1> Initiate BSS_INFO_T - Setup HW ID */
		prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
		prP2pBssInfo->ucHwDefaultFixedRateCode = RATE_OFDM_6M;

		prP2pBssInfo->ucNonHTBasicPhyType = (uint8_t)
		    rNonHTApModeAttributes[prP2pBssInfo->ucConfigAdHocAPMode]
			.ePhyTypeIndex;

		prP2pBssInfo->u2BSSBasicRateSet =
		    rNonHTApModeAttributes[prP2pBssInfo->ucConfigAdHocAPMode]
			.u2BSSBasicRateSet;

		prP2pBssInfo->u2OperationalRateSet =
		    rNonHTPhyAttributes[prP2pBssInfo->ucNonHTBasicPhyType]
			.u2SupportedRateSet;

		rateGetDataRatesFromRateSet(prP2pBssInfo->u2OperationalRateSet,
			prP2pBssInfo->u2BSSBasicRateSet,
			prP2pBssInfo->aucAllSupportedRates,
			&prP2pBssInfo->ucAllSupportedRatesLen);

		prP2pBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
			OFFSET_OF(struct WLAN_BEACON_FRAME, aucInfoElem[0]) +
			MAX_IE_LENGTH);

		if (prP2pBssInfo->prBeacon) {
			prP2pBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
			/* NULL STA_REC */
			prP2pBssInfo->prBeacon->ucStaRecIndex = 0xFF;
			prP2pBssInfo->prBeacon->ucNetworkType =
				NETWORK_TYPE_P2P_INDEX;
		} else {
			/* Out of memory. */
			ASSERT(FALSE);
		}

		prP2pBssInfo->eCurrentOPMode = OP_MODE_NUM;

		prP2pBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
		prP2pBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
		prP2pBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
		prP2pBssInfo->ucPrimaryChannel = P2P_DEFAULT_LISTEN_CHANNEL;
		prP2pBssInfo->eBand = BAND_2G4;
		prP2pBssInfo->eBssSCO = CHNL_EXT_SCN;

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucQoS))
			prP2pBssInfo->fgIsQBSS = TRUE;
		else
			prP2pBssInfo->fgIsQBSS = FALSE;

		SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
	} while (FALSE);

	return;
#endif
}				/* p2pDevFsmInit */

void p2pDevFsmUninit(IN struct ADAPTER *prAdapter)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		ASSERT_BREAK(prP2pDevFsmInfo != NULL);

		prP2pBssInfo =
			prAdapter->aprBssInfo[prP2pDevFsmInfo->ucBssIndex];

		cnmTimerStopTimer(prAdapter,
			&(prP2pDevFsmInfo->rP2pFsmTimeoutTimer));

		p2pFunCleanQueuedMgmtFrame(prAdapter,
				&prP2pDevFsmInfo->rQueuedActionFrame);

		p2pFunClearAllTxReq(prAdapter, &(prP2pDevFsmInfo->rMgmtTxInfo));

		/* Abort device FSM */
		p2pDevFsmStateTransition(prAdapter,
			prP2pDevFsmInfo,
			P2P_DEV_STATE_IDLE);
		p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo);

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Clear CmdQue */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
			prP2pBssInfo->ucBssIndex);
		kalClearSecurityFramesByBssIdx(prAdapter->prGlueInfo,
			prP2pBssInfo->ucBssIndex);
		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
			prP2pBssInfo->ucBssIndex);
		/* Clear PendingTxMsdu */
		nicFreePendingTxMsduInfo(prAdapter,
			prP2pBssInfo->ucBssIndex, MSDU_REMOVE_BY_BSS_INDEX);

		/* Deactivate BSS. */
		UNSET_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

		nicDeactivateNetwork(prAdapter, prP2pBssInfo->ucBssIndex);

		cnmFreeBssInfo(prAdapter, prP2pBssInfo);
	} while (FALSE);

#if 0
	struct P2P_FSM_INFO *prP2pFsmInfo = (struct P2P_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pFsmUninit()\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo =
		&(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		p2pFuncSwitchOPMode(prAdapter, prP2pBssInfo,
			OP_MODE_P2P_DEVICE, TRUE);

		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		p2pStateAbort_IDLE(prAdapter, prP2pFsmInfo, P2P_STATE_NUM);

		UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		wlanAcquirePowerControl(prAdapter);

		/* Release all pending CMD queue. */
		DBGLOG(P2P, TRACE,
		       "p2pFsmUninit: wlanProcessCommandQueue, num of element:%d\n",
		       prAdapter->prGlueInfo->rCmdQueue.u4NumElem);
		wlanProcessCommandQueue(prAdapter,
			&prAdapter->prGlueInfo->rCmdQueue);

		wlanReleasePowerControl(prAdapter);

		/* Release pending mgmt frame,
		 * mgmt frame may be pending by CMD without resource.
		 */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
			NETWORK_TYPE_P2P_INDEX);

		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
			NETWORK_TYPE_P2P_INDEX);

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}
	} while (FALSE);

	return;
#endif
}				/* p2pDevFsmUninit */

void
p2pDevFsmStateTransition(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
		IN enum ENUM_P2P_DEV_STATE eNextState)
{
	u_int8_t fgIsLeaveState = (u_int8_t) FALSE;

	ASSERT(prP2pDevFsmInfo);
	if (!prP2pDevFsmInfo) {
		DBGLOG(P2P, ERROR, "prP2pDevFsmInfo is NULL!\n");
		return;
	}

	ASSERT(prP2pDevFsmInfo->ucBssIndex == prAdapter->ucP2PDevBssIdx);
	if (prP2pDevFsmInfo->ucBssIndex != prAdapter->ucP2PDevBssIdx) {
		log_dbg(P2P, ERROR,
			"prP2pDevFsmInfo->ucBssIndex %d should be prAdapter->ucP2PDevBssIdx(%d)!\n",
			prP2pDevFsmInfo->ucBssIndex, prAdapter->ucP2PDevBssIdx);
		return;
	}

	do {
		if (!IS_BSS_ACTIVE(
			prAdapter->aprBssInfo[prP2pDevFsmInfo->ucBssIndex])) {
			if (!cnmP2PIsPermitted(prAdapter))
				return;

			SET_NET_ACTIVE(prAdapter, prP2pDevFsmInfo->ucBssIndex);
			nicActivateNetwork(prAdapter,
				prP2pDevFsmInfo->ucBssIndex);
		}

		fgIsLeaveState = fgIsLeaveState ? FALSE : TRUE;

		if (!fgIsLeaveState) {
			/* Print log with state changed */
			if (prP2pDevFsmInfo->eCurrentState != eNextState)
				DBGLOG(P2P, STATE,
					"[P2P_DEV]TRANSITION: [%s] -> [%s]\n",
					apucDebugP2pDevState
					[prP2pDevFsmInfo->eCurrentState],
					apucDebugP2pDevState[eNextState]);

			/* Transition into current state. */
			prP2pDevFsmInfo->eCurrentState = eNextState;
		}

		switch (prP2pDevFsmInfo->eCurrentState) {
		case P2P_DEV_STATE_IDLE:
			if (!fgIsLeaveState) {
				fgIsLeaveState = p2pDevStateInit_IDLE(prAdapter,
					&prP2pDevFsmInfo->rChnlReqInfo,
					&eNextState);
			} else {
				p2pDevStateAbort_IDLE(prAdapter);
			}
			break;
		case P2P_DEV_STATE_SCAN:
			if (!fgIsLeaveState) {
				p2pDevStateInit_SCAN(prAdapter,
					prP2pDevFsmInfo->ucBssIndex,
					&prP2pDevFsmInfo->rScanReqInfo);
			} else {
				p2pDevStateAbort_SCAN(prAdapter,
					prP2pDevFsmInfo);
			}
			break;
		case P2P_DEV_STATE_REQING_CHANNEL:
			if (!fgIsLeaveState) {
				fgIsLeaveState = p2pDevStateInit_REQING_CHANNEL(
					prAdapter,
					prP2pDevFsmInfo->ucBssIndex,
					&(prP2pDevFsmInfo->rChnlReqInfo),
					&eNextState);
			} else {
				p2pDevStateAbort_REQING_CHANNEL(prAdapter,
					&(prP2pDevFsmInfo->rChnlReqInfo),
					eNextState);
			}
			break;
		case P2P_DEV_STATE_CHNL_ON_HAND:
			if (!fgIsLeaveState) {
				p2pDevStateInit_CHNL_ON_HAND(prAdapter,
					prAdapter->aprBssInfo
					[prP2pDevFsmInfo->ucBssIndex],
					prP2pDevFsmInfo,
					&(prP2pDevFsmInfo->rChnlReqInfo));
			} else {
				p2pDevStateAbort_CHNL_ON_HAND(prAdapter,
					prAdapter->aprBssInfo
					[prP2pDevFsmInfo->ucBssIndex],
					prP2pDevFsmInfo,
					&(prP2pDevFsmInfo->rChnlReqInfo),
					eNextState);
			}
			break;
		case P2P_DEV_STATE_OFF_CHNL_TX:
			if (!fgIsLeaveState) {
				fgIsLeaveState = p2pDevStateInit_OFF_CHNL_TX(
					prAdapter,
					prP2pDevFsmInfo,
					&(prP2pDevFsmInfo->rChnlReqInfo),
					&(prP2pDevFsmInfo->rMgmtTxInfo),
					&eNextState);
			} else {
				p2pDevStateAbort_OFF_CHNL_TX(
					prAdapter,
					prP2pDevFsmInfo,
					&(prP2pDevFsmInfo->rMgmtTxInfo),
					&(prP2pDevFsmInfo->rChnlReqInfo),
					eNextState);
			}
			break;
		default:
			/* Unexpected state. */
			ASSERT(FALSE);
			break;
		}
	} while (fgIsLeaveState);
}				/* p2pDevFsmStateTransition */

void p2pDevFsmRunEventAbort(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo)
{
	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prP2pDevFsmInfo != NULL));

		if (prP2pDevFsmInfo->eCurrentState != P2P_DEV_STATE_IDLE) {
			/* Get into IDLE state. */
			p2pDevFsmStateTransition(prAdapter,
				prP2pDevFsmInfo,
				P2P_DEV_STATE_IDLE);
		}

		/* Abort IDLE. */
		p2pDevStateAbort_IDLE(prAdapter);
	} while (FALSE);
}				/* p2pDevFsmRunEventAbort */

void p2pDevFsmRunEventTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) ulParamPtr;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pDevFsmInfo != NULL));

		switch (prP2pDevFsmInfo->eCurrentState) {
		case P2P_DEV_STATE_IDLE:
			/* TODO: IDLE timeout for low power mode. */
			break;
		case P2P_DEV_STATE_CHNL_ON_HAND:
			if (prAdapter->prP2pInfo->ucExtendChanFlag) {
				prAdapter->prP2pInfo->ucExtendChanFlag = 0;
				p2pDevFsmStateTransition(prAdapter,
					prP2pDevFsmInfo, P2P_DEV_STATE_IDLE);
				break;
			}
			if (p2pFuncNeedWaitRsp(prAdapter,
					prAdapter->prP2pInfo->eConnState)) {
				DBGLOG(P2P, INFO,
					"P2P: re-enter CHNL_ON_HAND with state: %d\n",
					prAdapter->prP2pInfo->eConnState);
				prAdapter->prP2pInfo->ucExtendChanFlag = 1;
				p2pDevFsmStateTransition(prAdapter,
					prP2pDevFsmInfo,
					P2P_DEV_STATE_CHNL_ON_HAND);
			} else {
				p2pDevFsmStateTransition(prAdapter,
					prP2pDevFsmInfo,
					P2P_DEV_STATE_IDLE);
			}
			break;
		case P2P_DEV_STATE_OFF_CHNL_TX:
			p2pDevFsmStateTransition(prAdapter, prP2pDevFsmInfo,
					P2P_DEV_STATE_OFF_CHNL_TX);
			break;
		default:
			ASSERT(FALSE);
			log_dbg(P2P, ERROR,
			       "Current P2P Dev State %d is unexpected for FSM timeout event.\n",
			       prP2pDevFsmInfo->eCurrentState);
			break;
		}
	} while (FALSE);
}				/* p2pDevFsmRunEventTimeout */

/*================ Message Event =================*/
void p2pDevFsmRunEventScanRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_P2P_SCAN_REQUEST *prP2pScanReqMsg =
		(struct MSG_P2P_SCAN_REQUEST *) NULL;
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;
	uint32_t u4ChnlListSize = 0;
	struct P2P_SSID_STRUCT *prP2pSsidStruct =
		(struct P2P_SSID_STRUCT *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		if (prP2pDevFsmInfo == NULL)
			break;

		if (prP2pDevFsmInfo->eCurrentState != P2P_DEV_STATE_IDLE)
			p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo);

		prP2pScanReqMsg = (struct MSG_P2P_SCAN_REQUEST *) prMsgHdr;
		prScanReqInfo = &(prP2pDevFsmInfo->rScanReqInfo);

		DBGLOG(P2P, TRACE, "p2pDevFsmRunEventScanRequest\n");

		/* Do we need to be in IDLE state? */
		/* p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo); */

		ASSERT(prScanReqInfo->fgIsScanRequest == FALSE);

		prScanReqInfo->fgIsAbort = TRUE;
		prScanReqInfo->eScanType = prP2pScanReqMsg->eScanType;
		prScanReqInfo->u2PassiveDewellTime = 0;

		if (prP2pScanReqMsg->u4NumChannel) {
			prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;

			/* Channel List */
			prScanReqInfo->ucNumChannelList =
				prP2pScanReqMsg->u4NumChannel;
			DBGLOG(P2P, TRACE,
				"Scan Request Channel List Number: %d\n",
				prScanReqInfo->ucNumChannelList);
			if (prScanReqInfo->ucNumChannelList
				> MAXIMUM_OPERATION_CHANNEL_LIST) {
				DBGLOG(P2P, TRACE,
				       "Channel List Number Overloaded: %d, change to: %d\n",
				       prScanReqInfo->ucNumChannelList,
				       MAXIMUM_OPERATION_CHANNEL_LIST);
				prScanReqInfo->ucNumChannelList =
					MAXIMUM_OPERATION_CHANNEL_LIST;
			}

			u4ChnlListSize = sizeof(struct RF_CHANNEL_INFO)
					* prScanReqInfo->ucNumChannelList;
			kalMemCopy(prScanReqInfo->arScanChannelList,
					prP2pScanReqMsg->arChannelListInfo,
					u4ChnlListSize);
		} else {
			/* If channel number is ZERO.
			 * It means do a FULL channel scan.
			 */
			prScanReqInfo->eChannelSet = SCAN_CHANNEL_FULL;
		}

		/* SSID */
		prP2pSsidStruct = prP2pScanReqMsg->prSSID;
		for (prScanReqInfo->ucSsidNum = 0;
				prScanReqInfo->ucSsidNum
					< prP2pScanReqMsg->i4SsidNum;
				prScanReqInfo->ucSsidNum++) {
			kalMemCopy(
				prScanReqInfo->arSsidStruct
					[prScanReqInfo->ucSsidNum].aucSsid,
				prP2pSsidStruct->aucSsid,
				prP2pSsidStruct->ucSsidLen);

			prScanReqInfo->arSsidStruct
					[prScanReqInfo->ucSsidNum].ucSsidLen =
				prP2pSsidStruct->ucSsidLen;

			prP2pSsidStruct++;
		}

		/* IE Buffer */
		kalMemCopy(prScanReqInfo->aucIEBuf,
			prP2pScanReqMsg->pucIEBuf,
			prP2pScanReqMsg->u4IELen);

		prScanReqInfo->u4BufLength = prP2pScanReqMsg->u4IELen;

		p2pDevFsmStateTransition(prAdapter,
			prP2pDevFsmInfo,
			P2P_DEV_STATE_SCAN);
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventScanRequest */

void p2pDevFsmRunEventScanAbort(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DBGLOG(P2P, TRACE, "p2pDevFsmRunEventScanAbort\n");

		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		if (prP2pDevFsmInfo->eCurrentState == P2P_DEV_STATE_SCAN) {
			struct P2P_SCAN_REQ_INFO *prScanReqInfo =
				&(prP2pDevFsmInfo->rScanReqInfo);

			prScanReqInfo->fgIsAbort = TRUE;

			p2pDevFsmStateTransition(prAdapter,
				prP2pDevFsmInfo,
				P2P_DEV_STATE_IDLE);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pDevFsmRunEventScanAbort */

void
p2pDevFsmRunEventScanDone(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo)
{
	struct MSG_SCN_SCAN_DONE *prScanDoneMsg =
		(struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	struct P2P_SCAN_REQ_INFO *prP2pScanReqInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prMsgHdr != NULL)
			&& (prP2pDevFsmInfo != NULL));

		if (!prP2pDevFsmInfo) {
			DBGLOG(P2P, ERROR,
				"prP2pDevFsmInfo is null, maybe remove p2p already\n");
			break;
		}

		prP2pScanReqInfo = &(prP2pDevFsmInfo->rScanReqInfo);

		if (prScanDoneMsg->ucSeqNum
			!= prP2pScanReqInfo->ucSeqNumOfScnMsg) {
			DBGLOG(P2P, TRACE,
				"P2P Scan Done SeqNum:%d  <->   P2P Dev FSM Scan SeqNum:%d",
				prScanDoneMsg->ucSeqNum,
				prP2pScanReqInfo->ucSeqNumOfScnMsg);
			break;
		}

		ASSERT_BREAK(prScanDoneMsg->ucBssIndex
			== prP2pDevFsmInfo->ucBssIndex);

		prP2pScanReqInfo->fgIsAbort = FALSE;
		prP2pScanReqInfo->fgIsScanRequest = FALSE;

		p2pDevFsmStateTransition(prAdapter,
			prP2pDevFsmInfo,
			P2P_DEV_STATE_IDLE);
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventScanDone */

void p2pDevFsmRunEventChannelRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	u_int8_t fgIsChnlFound = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		if (prP2pDevFsmInfo == NULL) {
			DBGLOG(P2P, WARN, "uninitialized p2p Dev fsm\n");
			break;
		}

		prChnlReqInfo = &(prP2pDevFsmInfo->rChnlReqInfo);

		DBGLOG(P2P, TRACE, "p2pDevFsmRunEventChannelRequest\n");

		/* printk(
		 * "p2pDevFsmRunEventChannelRequest check cookie =%lld\n",
		 * prChnlReqInfo->u8Cookie);
		 */

		if (!LINK_IS_EMPTY(&prChnlReqInfo->rP2pChnlReqLink)) {
			struct LINK_ENTRY *prLinkEntry =
				(struct LINK_ENTRY *) NULL;
			struct MSG_P2P_CHNL_REQUEST *prP2pMsgChnlReq =
				(struct MSG_P2P_CHNL_REQUEST *) NULL;

			LINK_FOR_EACH(prLinkEntry,
				&prChnlReqInfo->rP2pChnlReqLink) {

				prP2pMsgChnlReq =
				    (struct MSG_P2P_CHNL_REQUEST *)
					LINK_ENTRY(prLinkEntry,
						struct MSG_HDR, rLinkEntry);

				if (prP2pMsgChnlReq->eChnlReqType
					== CH_REQ_TYPE_ROC) {
					LINK_REMOVE_KNOWN_ENTRY(
						&prChnlReqInfo->rP2pChnlReqLink,
						prLinkEntry);
					cnmMemFree(prAdapter, prP2pMsgChnlReq);
					/* DBGLOG(P2P, TRACE, */
					/* ("p2pDevFsmRunEventChannelAbort:
					 * Channel Abort, cookie found:%d\n",
					 */
					/* prChnlAbortMsg->u8Cookie)); */
					fgIsChnlFound = TRUE;
					break;
				}
			}
		}

		/* Queue the channel request. */
		LINK_INSERT_TAIL(&(prChnlReqInfo->rP2pChnlReqLink),
			&(prMsgHdr->rLinkEntry));
		prMsgHdr = NULL;

		/* If channel is not requested,
		 * it may due to channel is released.
		 */
		if ((!fgIsChnlFound)
			&& (prChnlReqInfo->eChnlReqType
				== CH_REQ_TYPE_ROC)
		    && (prChnlReqInfo->fgIsChannelRequested)) {

			ASSERT(
				(prP2pDevFsmInfo->eCurrentState
					== P2P_DEV_STATE_REQING_CHANNEL) ||
				(prP2pDevFsmInfo->eCurrentState
					== P2P_DEV_STATE_CHNL_ON_HAND));

			p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo);

			break;
		}

		if (prP2pDevFsmInfo->eCurrentState == P2P_DEV_STATE_IDLE) {
			/* Re-enter IDLE state would trigger channel request. */
			p2pDevFsmStateTransition(prAdapter,
				prP2pDevFsmInfo, P2P_DEV_STATE_IDLE);
		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventChannelRequest */

void p2pDevFsmRunEventChannelAbort(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	struct MSG_P2P_CHNL_ABORT *prChnlAbortMsg =
		(struct MSG_P2P_CHNL_ABORT *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prChnlAbortMsg = (struct MSG_P2P_CHNL_ABORT *) prMsgHdr;

		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		if (prP2pDevFsmInfo == NULL)
			break;

		prChnlReqInfo = &(prP2pDevFsmInfo->rChnlReqInfo);

		DBGLOG(P2P, TRACE, "p2pDevFsmRunEventChannelAbort\n");

		p2pFunCleanQueuedMgmtFrame(prAdapter,
				&prP2pDevFsmInfo->rQueuedActionFrame);

		/* If channel is not requested,
		 * it may due to channel is released.
		 */
		if ((prChnlAbortMsg->u8Cookie == prChnlReqInfo->u8Cookie)
			&& (prChnlReqInfo->fgIsChannelRequested) &&
			prChnlReqInfo->eChnlReqType == CH_REQ_TYPE_ROC) {
			ASSERT(
				(prP2pDevFsmInfo->eCurrentState
					== P2P_DEV_STATE_REQING_CHANNEL) ||
				(prP2pDevFsmInfo->eCurrentState
					== P2P_DEV_STATE_CHNL_ON_HAND));

			/*
			 * If cancel-roc cmd is called from Supplicant
			 * while driver is waiting for FW's channel grant event,
			 * roc event must be returned to Supplicant
			 * first to reset Supplicant's variables
			 * and then transition to idle state.
			 */
			if (prP2pDevFsmInfo->eCurrentState
				== P2P_DEV_STATE_REQING_CHANNEL) {
				kalP2PIndicateChannelReady(
					prAdapter->prGlueInfo,
					prChnlReqInfo->u8Cookie,
					prChnlReqInfo->ucReqChnlNum,
					prChnlReqInfo->eBand,
					prChnlReqInfo->eChnlSco,
					prChnlReqInfo->u4MaxInterval);
				kalP2PIndicateChannelExpired(
					prAdapter->prGlueInfo,
					prChnlReqInfo->u8Cookie,
					prChnlReqInfo->ucReqChnlNum,
					prChnlReqInfo->eBand,
					prChnlReqInfo->eChnlSco);
			}
			p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo);

			break;
		} else if (!LINK_IS_EMPTY(&prChnlReqInfo->rP2pChnlReqLink)) {
			struct LINK_ENTRY *prLinkEntry =
				(struct LINK_ENTRY *) NULL;
			struct MSG_P2P_CHNL_REQUEST *prP2pMsgChnlReq =
				(struct MSG_P2P_CHNL_REQUEST *) NULL;

			LINK_FOR_EACH(prLinkEntry,
				&prChnlReqInfo->rP2pChnlReqLink) {
				prP2pMsgChnlReq =
				    (struct MSG_P2P_CHNL_REQUEST *)
					LINK_ENTRY(prLinkEntry,
					struct MSG_HDR, rLinkEntry);

				if (prP2pMsgChnlReq->u8Cookie
					== prChnlAbortMsg->u8Cookie) {
					LINK_REMOVE_KNOWN_ENTRY(
						&prChnlReqInfo->rP2pChnlReqLink,
						prLinkEntry);
					log_dbg(P2P, TRACE,
						"p2pDevFsmRunEventChannelAbort: Channel Abort, cookie found:0x%llx\n",
						prChnlAbortMsg->u8Cookie);
					kalP2PIndicateChannelReady(
						prAdapter->prGlueInfo,
						prP2pMsgChnlReq->u8Cookie,
						prP2pMsgChnlReq->rChannelInfo
							.ucChannelNum,
						prP2pMsgChnlReq->rChannelInfo
							.eBand,
						prP2pMsgChnlReq->eChnlSco,
						prP2pMsgChnlReq->u4Duration);
					kalP2PIndicateChannelExpired(
						prAdapter->prGlueInfo,
						prP2pMsgChnlReq->u8Cookie,
						prP2pMsgChnlReq->rChannelInfo
							.ucChannelNum,
						prP2pMsgChnlReq->rChannelInfo
							.eBand,
						prP2pMsgChnlReq->eChnlSco);
					cnmMemFree(prAdapter, prP2pMsgChnlReq);
					break;
				}
			}
		} else {
			log_dbg(P2P, WARN,
			       "p2pDevFsmRunEventChannelAbort: Channel Abort Fail, cookie not found:0x%llx\n",
			       prChnlAbortMsg->u8Cookie);
		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventChannelAbort */

void
p2pDevFsmRunEventChnlGrant(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo)
{
	struct MSG_CH_GRANT *prMsgChGrant = (struct MSG_CH_GRANT *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;

	do {
		ASSERT((prAdapter != NULL)
			&& (prMsgHdr != NULL)
			&& (prP2pDevFsmInfo != NULL));

		if ((prAdapter == NULL)
			|| (prMsgHdr == NULL)
			|| (prP2pDevFsmInfo == NULL))
			break;

		prMsgChGrant = (struct MSG_CH_GRANT *) prMsgHdr;
		prChnlReqInfo = &(prP2pDevFsmInfo->rChnlReqInfo);

		if ((prMsgChGrant->ucTokenID
			!= prChnlReqInfo->ucSeqNumOfChReq)
			|| (!prChnlReqInfo->fgIsChannelRequested)) {
			break;
		}

		ASSERT(prMsgChGrant->ucPrimaryChannel
			== prChnlReqInfo->ucReqChnlNum);
		ASSERT(prMsgChGrant->eReqType
			== prChnlReqInfo->eChnlReqType);
		ASSERT(prMsgChGrant->u4GrantInterval
			== prChnlReqInfo->u4MaxInterval);

		prChnlReqInfo->u4MaxInterval = prMsgChGrant->u4GrantInterval;

		if (prMsgChGrant->eReqType == CH_REQ_TYPE_ROC) {
			p2pDevFsmStateTransition(prAdapter,
				prP2pDevFsmInfo,
				P2P_DEV_STATE_CHNL_ON_HAND);
		} else {
			ASSERT(prMsgChGrant->eReqType
				== CH_REQ_TYPE_OFFCHNL_TX);

			p2pDevFsmStateTransition(prAdapter,
				prP2pDevFsmInfo,
				P2P_DEV_STATE_OFF_CHNL_TX);
		}
	} while (FALSE);

	if (prAdapter && prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventChnlGrant */

static u_int8_t
p2pDevChnlReqByOffChnl(IN struct ADAPTER *prAdapter,
	IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
	IN struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq)
{
	struct MSG_P2P_CHNL_REQUEST *prMsgChnlReq =
			(struct MSG_P2P_CHNL_REQUEST *) NULL;

	if (prAdapter == NULL || prP2pDevFsmInfo == NULL ||
			prOffChnlTxReq == NULL)
		return FALSE;

	prMsgChnlReq = cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG,
			sizeof(struct MSG_P2P_CHNL_REQUEST));

	if (prMsgChnlReq == NULL) {
		DBGLOG(P2P, ERROR,
				"Not enough MSG buffer for chnl request.\n");
		return FALSE;
	}

	prMsgChnlReq->eChnlReqType = CH_REQ_TYPE_OFFCHNL_TX;
	prMsgChnlReq->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_REQ;
	prMsgChnlReq->u8Cookie = prOffChnlTxReq->u8Cookie;
	prMsgChnlReq->u4Duration = prOffChnlTxReq->u4Duration;
	kalMemCopy(&prMsgChnlReq->rChannelInfo,
			&prOffChnlTxReq->rChannelInfo,
			sizeof(struct RF_CHANNEL_INFO));
	prMsgChnlReq->eChnlSco = prOffChnlTxReq->eChnlExt;
	p2pDevFsmRunEventChannelRequest(prAdapter,
			(struct MSG_HDR *) prMsgChnlReq);

	return TRUE;
}				/* p2pDevChnlReqByOffChnl */

static void
p2pDevAbortChlReqIfNeed(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo)
{
	struct MSG_P2P_CHNL_ABORT *prMsgChnlAbort =
			(struct MSG_P2P_CHNL_ABORT *) NULL;

	if (prAdapter == NULL || prChnlReqInfo == NULL ||
			prP2pDevFsmInfo == NULL)
		return;

	/* Cancel ongoing channel request whose type is not offchannel-tx*/
	if (prP2pDevFsmInfo->eCurrentState != P2P_DEV_STATE_REQING_CHANNEL ||
			prChnlReqInfo->eChnlReqType == CH_REQ_TYPE_OFFCHNL_TX)
		return;

	prMsgChnlAbort = cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG, sizeof(struct MSG_P2P_CHNL_ABORT));

	if (prMsgChnlAbort == NULL) {
		DBGLOG(P2P, ERROR, "Memory allocate failed.\n");
		return;
	}

	prMsgChnlAbort->u8Cookie = prChnlReqInfo->u8Cookie;
	p2pDevFsmRunEventChannelAbort(prAdapter,
			(struct MSG_HDR *) prMsgChnlAbort);
}

static void
p2pDevAdjustChnlTime(IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
	IN struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq)
{
	enum ENUM_P2P_CONNECT_STATE eP2pActionFrameType = P2P_CNN_NORMAL;
	uint32_t u4ExtendedTime = 0;

	if (prMgmtTxMsg == NULL || prOffChnlTxReq == NULL)
		return;

	if (!prMgmtTxMsg->fgIsOffChannel)
		return;

	if (prMgmtTxMsg->u4Duration < MIN_TX_DURATION_TIME_MS) {
		/*
		 * The wait time requested from Supplicant may be too short
		 * to TX a frame, eg. nego.conf.. Overwrite the wait time
		 * as driver's min TX time.
		 */
		DBGLOG(P2P, INFO, "Overwrite channel duration from %d to %d\n",
				prMgmtTxMsg->u4Duration,
				MIN_TX_DURATION_TIME_MS);
		prOffChnlTxReq->u4Duration = MIN_TX_DURATION_TIME_MS;
	} else {
		prOffChnlTxReq->u4Duration = prMgmtTxMsg->u4Duration;
	}

	eP2pActionFrameType = p2pFuncGetP2pActionFrameType(
			prMgmtTxMsg->prMgmtMsduInfo);
	switch (eP2pActionFrameType) {
	case P2P_CNN_GO_NEG_RESP:
		u4ExtendedTime = P2P_DEV_EXTEND_CHAN_TIME;
		break;
	default:
		break;
	}

	if (u4ExtendedTime) {
		DBGLOG(P2P, INFO, "Extended channel duration from %d to %d\n",
				prOffChnlTxReq->u4Duration,
				prOffChnlTxReq->u4Duration + u4ExtendedTime);
		prOffChnlTxReq->u4Duration += u4ExtendedTime;
	}
}				/* p2pDevAdjustChnlTime */

static u_int8_t
p2pDevAddTxReq2Queue(IN struct ADAPTER *prAdapter,
		IN struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
		OUT struct P2P_OFF_CHNL_TX_REQ_INFO **pprOffChnlTxReq)
{
	struct P2P_OFF_CHNL_TX_REQ_INFO *prTmpOffChnlTxReq =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;

	prTmpOffChnlTxReq = cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG,
			sizeof(struct P2P_OFF_CHNL_TX_REQ_INFO));

	if (prTmpOffChnlTxReq == NULL) {
		DBGLOG(P2P, ERROR,
				"Allocate TX request buffer fails.\n");
		return FALSE;
	}

	prTmpOffChnlTxReq->ucBssIndex = prMgmtTxMsg->ucBssIdx;
	prTmpOffChnlTxReq->u8Cookie = prMgmtTxMsg->u8Cookie;
	prTmpOffChnlTxReq->prMgmtTxMsdu = prMgmtTxMsg->prMgmtMsduInfo;
	prTmpOffChnlTxReq->fgNoneCckRate = prMgmtTxMsg->fgNoneCckRate;
	kalMemCopy(&prTmpOffChnlTxReq->rChannelInfo,
			&prMgmtTxMsg->rChannelInfo,
			sizeof(struct RF_CHANNEL_INFO));
	prTmpOffChnlTxReq->eChnlExt = prMgmtTxMsg->eChnlExt;
	prTmpOffChnlTxReq->fgIsWaitRsp = prMgmtTxMsg->fgIsWaitRsp;

	p2pDevAdjustChnlTime(prMgmtTxMsg, prTmpOffChnlTxReq);

	LINK_INSERT_TAIL(&prP2pMgmtTxReqInfo->rTxReqLink,
			&prTmpOffChnlTxReq->rLinkEntry);

	*pprOffChnlTxReq = prTmpOffChnlTxReq;

	return TRUE;
}

static void
p2pDevHandleOffchnlTxReq(IN struct ADAPTER *prAdapter,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
			(struct P2P_DEV_FSM_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo =
			(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
			(struct P2P_CHNL_REQ_INFO *) NULL;

	if (prAdapter == NULL || prMgmtTxMsg == NULL)
		return;

	prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

	if (prP2pDevFsmInfo == NULL)
		return;

	prP2pMgmtTxReqInfo = &(prP2pDevFsmInfo->rMgmtTxInfo);
	prChnlReqInfo = &(prP2pDevFsmInfo->rChnlReqInfo);

	if (prP2pMgmtTxReqInfo == NULL || prChnlReqInfo == NULL)
		return;

	p2pDevAbortChlReqIfNeed(prAdapter, prP2pDevFsmInfo, prChnlReqInfo);

	if (p2pDevAddTxReq2Queue(prAdapter, prP2pMgmtTxReqInfo,
			prMgmtTxMsg, &prOffChnlTxReq) == FALSE)
		goto error;

	if (prOffChnlTxReq == NULL)
		return;

	switch (prP2pDevFsmInfo->eCurrentState) {
	case P2P_DEV_STATE_IDLE:
		if (!p2pDevChnlReqByOffChnl(prAdapter, prP2pDevFsmInfo,
				prOffChnlTxReq))
			goto error;
		break;
	case P2P_DEV_STATE_REQING_CHANNEL:
		if (prChnlReqInfo->eChnlReqType != CH_REQ_TYPE_OFFCHNL_TX) {
			DBGLOG(P2P, WARN,
				"channel already requested by others\n");
			goto error;
		}
		break;
	case P2P_DEV_STATE_OFF_CHNL_TX:
		if (p2pFuncCheckOnRocChnl(&(prMgmtTxMsg->rChannelInfo),
					prChnlReqInfo) &&
				prP2pMgmtTxReqInfo->rTxReqLink.u4NumElem <= 1) {
			p2pDevFsmStateTransition(prAdapter,
					prP2pDevFsmInfo,
					P2P_DEV_STATE_OFF_CHNL_TX);
		} else {
			log_dbg(P2P, INFO, "tx ch: %d, current ch: %d, isRequested: %d, tx link num: %d",
				prMgmtTxMsg->rChannelInfo.ucChannelNum,
				prChnlReqInfo->ucReqChnlNum,
				prChnlReqInfo->fgIsChannelRequested,
				prP2pMgmtTxReqInfo->rTxReqLink.u4NumElem);
		}
		break;
	default:
		/* do nothing & wait for IDLE state to handle TX request */
		break;
	}

	return;

error:
	LINK_REMOVE_KNOWN_ENTRY(
			&(prP2pMgmtTxReqInfo->rTxReqLink),
			&prOffChnlTxReq->rLinkEntry);
	cnmMgtPktFree(prAdapter, prOffChnlTxReq->prMgmtTxMsdu);
	cnmMemFree(prAdapter, prOffChnlTxReq);
}				/* p2pDevHandleOffchnlTxReq */

static u_int8_t
p2pDevNeedOffchnlTx(IN struct ADAPTER *prAdapter,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
			(struct P2P_DEV_FSM_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
			(struct P2P_CHNL_REQ_INFO *) NULL;
	struct WLAN_MAC_HEADER *prWlanHdr = (struct WLAN_MAC_HEADER *) NULL;

	if (prAdapter == NULL || prMgmtTxMsg == NULL)
		return FALSE;

	if (!prMgmtTxMsg->fgIsOffChannel)
		return FALSE;

	prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;
	prChnlReqInfo = prP2pDevFsmInfo != NULL ?
			&(prP2pDevFsmInfo->rChnlReqInfo) : NULL;

	if (prChnlReqInfo == NULL)
		return FALSE;

	prWlanHdr = (struct WLAN_MAC_HEADER *)
			((unsigned long) prMgmtTxMsg->prMgmtMsduInfo->prPacket +
					MAC_TX_RESERVED_FIELD);
	/* Probe response can only be sent during roc channel or op channel */
	if ((prWlanHdr->u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_PROBE_RSP)
		return FALSE;

	if (prP2pDevFsmInfo->eCurrentState == P2P_DEV_STATE_CHNL_ON_HAND &&
			p2pFuncCheckOnRocChnl(&(prMgmtTxMsg->rChannelInfo),
					prChnlReqInfo))
		return FALSE;

	return TRUE;
}				/* p2pDevNeedOffchnlTx */

void p2pDevFsmRunEventMgmtTx(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
			(struct MSG_MGMT_TX_REQUEST *) NULL;
	u_int8_t fgNeedOffchnlTx;

	prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *) prMsgHdr;

	fgNeedOffchnlTx = p2pDevNeedOffchnlTx(prAdapter, prMgmtTxMsg);
	DBGLOG(P2P, INFO, "fgNeedOffchnlTx: %d\n", fgNeedOffchnlTx);

	if (!fgNeedOffchnlTx)
		p2pFuncTxMgmtFrame(prAdapter,
				prAdapter->ucP2PDevBssIdx,
				prMgmtTxMsg->prMgmtMsduInfo,
				prMgmtTxMsg->fgNoneCckRate);
	else
		p2pDevHandleOffchnlTxReq(prAdapter, prMgmtTxMsg);

	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventMgmtTx */

uint32_t
p2pDevFsmRunEventMgmtFrameTxDone(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo,
		IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	u_int8_t fgIsSuccess = FALSE;
	uint64_t *pu8GlCookie = (uint64_t *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		if (!prMsduInfo->prPacket) {
			DBGLOG(P2P, WARN,
				"Freed Msdu, do not indicate to host\n");
			break;
		}

		pu8GlCookie =
			(uint64_t *) ((unsigned long) prMsduInfo->prPacket +
				(unsigned long) prMsduInfo->u2FrameLength +
				MAC_TX_RESERVED_FIELD);

		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(P2P, INFO,
				"Mgmt Frame TX Fail, Status: %d. cookie: 0x%llx\n",
				rTxDoneStatus, *pu8GlCookie);
		} else {
			fgIsSuccess = TRUE;
			DBGLOG(P2P, INFO,
				"Mgmt Frame TX Done. cookie: 0x%llx\n",
				*pu8GlCookie);
		}

		kalP2PIndicateMgmtTxStatus(prAdapter->prGlueInfo,
			prMsduInfo,
			fgIsSuccess);
	} while (FALSE);

	return WLAN_STATUS_SUCCESS;
}				/* p2pDevFsmRunEventMgmtFrameTxDone */

void p2pDevFsmRunEventMgmtFrameRegister(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	/* TODO: RX Filter Management. */

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventMgmtFrameRegister */

void p2pDevFsmRunEventActiveDevBss(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));
		prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

		if (prP2pDevFsmInfo->eCurrentState
			== P2P_DEV_STATE_IDLE) {
			/* Get into IDLE state to let BSS be active
			 * and do not Deactive.
			 */
			p2pDevFsmStateTransition(prAdapter,
				prP2pDevFsmInfo,
				P2P_DEV_STATE_IDLE);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventActiveDevBss */

void
p2pDevFsmNotifyP2pRx(IN struct ADAPTER *prAdapter, uint8_t p2pFrameType,
		u_int8_t *prFgBufferFrame)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	u_int8_t fgNeedWaitRspFrame = FALSE;

	prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;
	fgNeedWaitRspFrame = p2pFuncNeedWaitRsp(prAdapter, p2pFrameType + 1);

	if (prP2pDevFsmInfo->eCurrentState == P2P_DEV_STATE_SCAN) {
		if (fgNeedWaitRspFrame)
			*prFgBufferFrame = TRUE;
		return;
	}

	if (prAdapter->prP2pInfo->eConnState != P2P_CNN_NORMAL)
		return;

	if (fgNeedWaitRspFrame) {
		DBGLOG(P2P, INFO,
			"Extend channel duration, p2pFrameType: %d.\n",
			p2pFrameType);
		prAdapter->prP2pInfo->eConnState = p2pFrameType + 1;
	}
}

void p2pDevFsmRunEventTxCancelWait(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
			(struct P2P_DEV_FSM_INFO *) NULL;
	struct MSG_CANCEL_TX_WAIT_REQUEST *prCancelTxWaitMsg =
			(struct MSG_CANCEL_TX_WAIT_REQUEST *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo =
			(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxPkt =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	u_int8_t fgIsCookieFound = FALSE;

	if (prAdapter == NULL || prMsgHdr == NULL)
		goto exit;

	prCancelTxWaitMsg = (struct MSG_CANCEL_TX_WAIT_REQUEST *) prMsgHdr;
	prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;
	prP2pMgmtTxInfo = prP2pDevFsmInfo != NULL ?
			&(prP2pDevFsmInfo->rMgmtTxInfo) : NULL;

	if (prCancelTxWaitMsg == NULL || prP2pDevFsmInfo == NULL ||
			prP2pMgmtTxInfo == NULL)
		goto exit;

	LINK_FOR_EACH_ENTRY(prOffChnlTxPkt,
			&(prP2pMgmtTxInfo->rTxReqLink),
			rLinkEntry,
			struct P2P_OFF_CHNL_TX_REQ_INFO) {
		if (!prOffChnlTxPkt)
			break;
		if (prOffChnlTxPkt->u8Cookie == prCancelTxWaitMsg->u8Cookie) {
			fgIsCookieFound = TRUE;
			break;
		}
	}

	if (fgIsCookieFound == TRUE || prP2pDevFsmInfo->eCurrentState ==
			P2P_DEV_STATE_OFF_CHNL_TX) {
		p2pFunClearAllTxReq(prAdapter,
				&(prP2pDevFsmInfo->rMgmtTxInfo));
		p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo);
	}

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
} /* p2pDevFsmRunEventTxCancelWait */

#endif /* CFG_ENABLE_WIFI_DIRECT */
