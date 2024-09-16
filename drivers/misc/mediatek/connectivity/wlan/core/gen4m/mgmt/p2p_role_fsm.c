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
#include "p2p_role_state.h"
#include "gl_p2p_os.h"

#if 1
/*lint -save -e64 Type mismatch */
static uint8_t *apucDebugP2pRoleState[P2P_ROLE_STATE_NUM] = {
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_IDLE"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_SCAN"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_REQING_CHANNEL"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_AP_CHNL_DETECTION"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_GC_JOIN"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_OFF_CHNL_TX"),
#if (CFG_SUPPORT_DFS_MASTER == 1)
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_DFS_CAC"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_SWITCH_CHANNEL"),
#endif
};

uint8_t *
	p2pRoleFsmGetFsmState(
	IN enum ENUM_P2P_ROLE_STATE eCurrentState) {
	if (eCurrentState >= 0 && eCurrentState < P2P_ROLE_STATE_NUM)
		return apucDebugP2pRoleState[eCurrentState];

	return (uint8_t *) DISP_STRING("UNKNOWN");
}

/*lint -restore */
#endif /* DBG */

void
p2pRoleFsmStateTransition(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState);
static u_int8_t
p2pRoleFsmIsAcsProcessing(IN struct ADAPTER *prAdapter,
		uint8_t ucRoleIdx);
static void
p2pRoleFsmAbortCurrentAcsReq(IN struct ADAPTER *prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest);

uint8_t p2pRoleFsmInit(IN struct ADAPTER *prAdapter,
		IN uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo =
		(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct GL_P2P_INFO *prP2PInfo = NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		ASSERT_BREAK(
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx)
				== NULL);

		prP2pRoleFsmInfo = kalMemAlloc(
			sizeof(struct P2P_ROLE_FSM_INFO),
			VIR_MEM_TYPE);

		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) =
			prP2pRoleFsmInfo;

		if (!prP2pRoleFsmInfo) {
			DBGLOG(P2P, ERROR,
				"Error allocating fsm Info Structure\n");
			break;
		}

		kalMemZero(prP2pRoleFsmInfo, sizeof(struct P2P_ROLE_FSM_INFO));

		prP2pRoleFsmInfo->ucRoleIndex = ucRoleIdx;

		prP2pRoleFsmInfo->eCurrentState = P2P_ROLE_STATE_IDLE;

		prP2pRoleFsmInfo->u4P2pPacketFilter =
			PARAM_PACKET_FILTER_SUPPORTED;

		prP2pChnlReqInfo = &prP2pRoleFsmInfo->rChnlReqInfo;
		LINK_INITIALIZE(&(prP2pChnlReqInfo->rP2pChnlReqLink));

		prP2pMgmtTxReqInfo = &prP2pRoleFsmInfo->rMgmtTxInfo;
		LINK_INITIALIZE(&prP2pMgmtTxReqInfo->rTxReqLink);

		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pRoleFsmRunEventTimeout,
			(unsigned long) prP2pRoleFsmInfo);

#if CFG_ENABLE_PER_STA_STATISTICS_LOG
		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmGetStatisticsTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pRoleFsmGetStaStatistics,
			(unsigned long) prP2pRoleFsmInfo);
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer),
			(PFN_MGMT_TIMEOUT_FUNC)
			p2pRoleFsmRunEventDfsShutDownTimeout,
			(unsigned long) prP2pRoleFsmInfo);
#endif

		prP2pBssInfo = cnmGetBssInfoAndInit(prAdapter,
			NETWORK_TYPE_P2P,
			FALSE);

		if (!prP2pBssInfo) {
			DBGLOG(P2P, ERROR,
				"Error allocating BSS Info Structure\n");
			break;
		}

		BSS_INFO_INIT(prAdapter, prP2pBssInfo);
		prP2pRoleFsmInfo->ucBssIndex = prP2pBssInfo->ucBssIndex;

		/* For state identify, not really used. */
		prP2pBssInfo->eIntendOPMode = OP_MODE_P2P_DEVICE;

		/* glRegisterP2P has setup the mac address */
		/* For wlan0 as AP mode case, this function will be called when
		 * changing interface type. And the MAC Addr overwrite by Role
		 * isn't expected.
		 * Maybe only using ucRoleIdx to calc MAC addr is better than
		 * using Role type.
		 */
		prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[ucRoleIdx];
		COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr,
			      prP2PInfo->prDevHandler->dev_addr);

		/* For BSS_INFO back trace to P2P Role & get Role FSM. */
		prP2pBssInfo->u4PrivateData = ucRoleIdx;

		if (p2pFuncIsAPMode(
			prAdapter->rWifiVar.prP2PConnSettings[ucRoleIdx])) {
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G;
			prP2pBssInfo->u2HwDefaultFixedRateCode =
				RATE_CCK_1M_LONG;
		} else {
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
			prP2pBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		}

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

		rateGetDataRatesFromRateSet(prP2pBssInfo->u2OperationalRateSet,
			prP2pBssInfo->u2BSSBasicRateSet,
			prP2pBssInfo->aucAllSupportedRates,
			&prP2pBssInfo->ucAllSupportedRatesLen);

		prP2pBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
			OFFSET_OF(struct WLAN_BEACON_FRAME, aucInfoElem[0])
				+ MAX_IE_LENGTH);

		if (prP2pBssInfo->prBeacon) {
			prP2pBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
			/* NULL STA_REC */
			prP2pBssInfo->prBeacon->ucStaRecIndex =
				STA_REC_INDEX_BMCAST;
			prP2pBssInfo->prBeacon->ucBssIndex =
				prP2pBssInfo->ucBssIndex;
		} else {
			/* Out of memory. */
			ASSERT(FALSE);
		}

		prP2pBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
		prP2pBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
		prP2pBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
		prP2pBssInfo->ucPrimaryChannel = P2P_DEFAULT_LISTEN_CHANNEL;
		prP2pBssInfo->eBand = BAND_2G4;
		prP2pBssInfo->eBssSCO = CHNL_EXT_SCN;
		prP2pBssInfo->ucOpRxNss = prP2pBssInfo->ucOpTxNss =
			wlanGetSupportNss(prAdapter, prP2pBssInfo->ucBssIndex);
#if (CFG_HW_WMM_BY_BSS == 0)
		prP2pBssInfo->ucWmmQueSet = (prAdapter->rWifiVar.eDbdcMode ==
			ENUM_DBDC_MODE_DISABLED)
			? DBDC_5G_WMM_INDEX
			: DBDC_2G_WMM_INDEX;
#endif
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucQoS))
			prP2pBssInfo->fgIsQBSS = TRUE;
		else
			prP2pBssInfo->fgIsQBSS = FALSE;

#if (CFG_SUPPORT_DFS_MASTER == 1)
		p2pFuncRadarInfoInit();
#endif

		/* SET_NET_PWR_STATE_IDLE(prAdapter,
		 * prP2pBssInfo->ucBssIndex);
		 */

		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);

		LINK_INITIALIZE(&prP2pBssInfo->rPmkidCache);
	} while (FALSE);

	if (prP2pBssInfo)
		return prP2pBssInfo->ucBssIndex;
	else
		return prAdapter->ucP2PDevBssIdx;
}				/* p2pFsmInit */

void p2pRoleFsmUninit(IN struct ADAPTER *prAdapter, IN uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pRoleFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pRoleFsmUninit()\n");

		prP2pRoleFsmInfo =
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx);

		ASSERT_BREAK(prP2pRoleFsmInfo != NULL);

		if (!prP2pRoleFsmInfo)
			return;

		prP2pBssInfo =
			prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

		p2pFuncDissolve(prAdapter,
			prP2pBssInfo, TRUE,
			REASON_CODE_DEAUTH_LEAVING_BSS);

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Function Dissolve should already enter IDLE state. */
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);

		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);

		p2pFunClearAllTxReq(prAdapter,
				&(prP2pRoleFsmInfo->rMgmtTxInfo));

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
		UNSET_NET_ACTIVE(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		nicDeactivateNetwork(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) = NULL;

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}

		cnmFreeBssInfo(prAdapter, prP2pBssInfo);

		/* ensure the timer be stopped */
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer));

#if CFG_ENABLE_PER_STA_STATISTICS_LOG
		cnmTimerStopTimer(prAdapter,
			&prP2pRoleFsmInfo->rP2pRoleFsmGetStatisticsTimer);
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer));
#endif

		if (prP2pRoleFsmInfo)
			kalMemFree(prP2pRoleFsmInfo, VIR_MEM_TYPE,
				sizeof(struct P2P_ROLE_FSM_INFO));

		rsnFlushPmkid(prAdapter, prP2pBssInfo->ucBssIndex);
	} while (FALSE);

	return;
#if 0
	struct P2P_FSM_INFO *prP2pFsmInfo = (struct P2P_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pFsmUninit()\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo =
			&(prAdapter->rWifiVar
			.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		p2pFuncSwitchOPMode(prAdapter,
			prP2pBssInfo,
			OP_MODE_P2P_DEVICE,
			TRUE);

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
}				/* p2pRoleFsmUninit */

void
p2pRoleFsmStateTransition(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN enum ENUM_P2P_ROLE_STATE eNextState)
{
	u_int8_t fgIsTransitionOut = (u_int8_t) FALSE;
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;

	prP2pRoleBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

	do {
		if (!IS_BSS_ACTIVE(prP2pRoleBssInfo)) {
			if (!cnmP2PIsPermitted(prAdapter))
				return;

#if CFG_SUPPORT_DBDC
			if (cnmDBDCIsReqPeivilegeLock() &&
				((eNextState == P2P_ROLE_STATE_REQING_CHANNEL &&
					(prChnlReqInfo->eChnlReqType ==
						CH_REQ_TYPE_GO_START_BSS ||
					prChnlReqInfo->eChnlReqType ==
						CH_REQ_TYPE_JOIN))
				|| (eNextState == P2P_ROLE_STATE_IDLE &&
					prP2pRoleFsmInfo->eCurrentState ==
						P2P_ROLE_STATE_IDLE)
#if (CFG_SUPPORT_DFS_MASTER == 1)
				|| eNextState == P2P_ROLE_STATE_SWITCH_CHANNEL
#endif
			)) {
				/* Do not activate network ruring DBDC HW
				 * switch. Otherwise, BSS may use incorrect
				 * CR and result in TRx problems.
				 */
				DBGLOG(P2P, STATE,
					"[P2P_ROLE][%d](Bss%d): Skip activate network [%s]\n",
					prP2pRoleFsmInfo->ucRoleIndex,
					prP2pRoleFsmInfo->ucBssIndex,
					p2pRoleFsmGetFsmState(eNextState));
			} else
#endif
			{
				SET_NET_ACTIVE(prAdapter,
					prP2pRoleBssInfo->ucBssIndex);
				nicActivateNetwork(prAdapter,
					prP2pRoleBssInfo->ucBssIndex);
			}
		}

		fgIsTransitionOut = fgIsTransitionOut ? FALSE : TRUE;

		if (!fgIsTransitionOut) {
			/* Print log with state changed */
			if (prP2pRoleFsmInfo->eCurrentState != eNextState)
				DBGLOG(P2P, STATE,
					"[P2P_ROLE][%d]TRANSITION(Bss%d): [%s] -> [%s]\n",
					prP2pRoleFsmInfo->ucRoleIndex,
					prP2pRoleFsmInfo->ucBssIndex,
					p2pRoleFsmGetFsmState
					(prP2pRoleFsmInfo->eCurrentState),
					p2pRoleFsmGetFsmState(eNextState));

			/* Transition into current state. */
			prP2pRoleFsmInfo->eCurrentState = eNextState;
		}

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_IDLE:
			if (!fgIsTransitionOut)
				p2pRoleStateInit_IDLE(prAdapter,
					prP2pRoleFsmInfo,
					prP2pRoleBssInfo);
			else
				p2pRoleStateAbort_IDLE(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			break;
		case P2P_ROLE_STATE_SCAN:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_SCAN(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rScanReqInfo));
			} else {
				p2pRoleStateAbort_SCAN(prAdapter,
					prP2pRoleFsmInfo);
			}
			break;
		case P2P_ROLE_STATE_REQING_CHANNEL:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_REQING_CHANNEL(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_REQING_CHANNEL(prAdapter,
					prP2pRoleBssInfo,
					prP2pRoleFsmInfo, eNextState);
			}
			break;
		case P2P_ROLE_STATE_AP_CHNL_DETECTION:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_AP_CHNL_DETECTION(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rScanReqInfo),
					&(prP2pRoleFsmInfo->rConnReqInfo));
			} else {
				p2pRoleStateAbort_AP_CHNL_DETECTION(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rConnReqInfo),
					&(prP2pRoleFsmInfo->rChnlReqInfo),
					&(prP2pRoleFsmInfo->rScanReqInfo),
					eNextState);
			}
			break;
		case P2P_ROLE_STATE_GC_JOIN:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_GC_JOIN(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_GC_JOIN(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rJoinInfo),
					eNextState);
			}
			break;
		case P2P_ROLE_STATE_OFF_CHNL_TX:
			if (!fgIsTransitionOut) {
				fgIsTransitionOut =
					p2pRoleStateInit_OFF_CHNL_TX(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo),
					&(prP2pRoleFsmInfo->rMgmtTxInfo),
					&eNextState);
			} else {
				p2pRoleStateAbort_OFF_CHNL_TX(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rMgmtTxInfo),
					&(prP2pRoleFsmInfo->rChnlReqInfo),
					eNextState);
			}
			break;

#if (CFG_SUPPORT_DFS_MASTER == 1)
		case P2P_ROLE_STATE_DFS_CAC:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_DFS_CAC(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_DFS_CAC(prAdapter,
					prP2pRoleBssInfo,
					prP2pRoleFsmInfo,
					eNextState);
			}
			break;
		case P2P_ROLE_STATE_SWITCH_CHANNEL:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_SWITCH_CHANNEL(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_SWITCH_CHANNEL(prAdapter,
					prP2pRoleBssInfo,
					prP2pRoleFsmInfo,
					eNextState);
			}
			break;
#endif
		default:
			ASSERT(FALSE);
			break;
		}
	} while (fgIsTransitionOut);

}				/* p2pRoleFsmStateTransition */

void p2pRoleFsmRunEventTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL));

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_IDLE:
			prP2pChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
			if (prP2pChnlReqInfo->fgIsChannelRequested) {
				p2pFuncReleaseCh(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					prP2pChnlReqInfo);
				if (IS_NET_PWR_STATE_IDLE(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex))
					ASSERT(FALSE);
			}

			if (IS_NET_PWR_STATE_IDLE(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex) &&
				IS_NET_ACTIVE(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex)) {
				DBGLOG(P2P, TRACE,
					"Role BSS IDLE, deactive network.\n");
				UNSET_NET_ACTIVE(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex);
				nicDeactivateNetwork(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex);
				nicUpdateBss(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex);
			}
			break;
		case P2P_ROLE_STATE_GC_JOIN:
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			break;
		case P2P_ROLE_STATE_OFF_CHNL_TX:
			p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo,
				P2P_ROLE_STATE_OFF_CHNL_TX);
			break;
#if (CFG_SUPPORT_DFS_MASTER == 1)
		case P2P_ROLE_STATE_DFS_CAC:
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			kalP2PCacFinishedUpdate(prAdapter->prGlueInfo,
				prP2pRoleFsmInfo->ucRoleIndex);
			p2pFuncSetDfsState(DFS_STATE_ACTIVE);
			cnmTimerStartTimer(prAdapter,
				&(prP2pRoleFsmInfo->rDfsShutDownTimer),
				5000);
			break;
#endif
		default:
			DBGLOG(P2P, ERROR,
			       "Current P2P Role State %d is unexpected for FSM timeout event.\n",
			       prP2pRoleFsmInfo->eCurrentState);
			ASSERT(FALSE);
			break;
		}
	} while (FALSE);
}				/* p2pRoleFsmRunEventTimeout */

static void
p2pRoleFsmDeauthComplete(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	enum ENUM_PARAM_MEDIA_STATE eOriMediaStatus;
	struct GL_P2P_INFO *prP2PInfo;

	if (!prAdapter) {
		DBGLOG(P2P, ERROR, "prAdapter shouldn't be NULL!\n");
		return;
	}

	if (!prStaRec) {
		DBGLOG(P2P, ERROR, "prStaRec shouldn't be NULL!\n");
		return;
	}

	DBGLOG(P2P, INFO, "Deauth TX Complete!\n");

	prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
	ASSERT_BREAK(prP2pBssInfo != NULL);
	eOriMediaStatus = prP2pBssInfo->eConnectionState;
	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);
	prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[
			prP2pRoleFsmInfo->ucRoleIndex];

	/*
	 * After EAP exchange, GO/GC will disconnect
	 * and re-connect in short time.
	 * GC's new station record will be removed unexpectedly at GO's side
	 * if new GC's connection happens
	 * when previous GO's disconnection flow is
	 * processing. 4-way handshake will NOT be triggered.
	 */
	if ((prStaRec->eAuthAssocState == AAA_STATE_SEND_AUTH2 ||
			prStaRec->eAuthAssocState == AAA_STATE_SEND_ASSOC2) &&
		(prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
		(p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pBssInfo->u4PrivateData]) == FALSE)) {
		DBGLOG(P2P, WARN,
			"Skip deauth tx done since AAA fsm is in progress.\n");
		return;
	} else if (prStaRec->eAuthAssocState == SAA_STATE_SEND_AUTH1 ||
		prStaRec->eAuthAssocState == SAA_STATE_SEND_ASSOC1) {
		DBGLOG(P2P, WARN,
			"Skip deauth tx done since SAA fsm is in progress.\n");
		return;
	}

	ASSERT_BREAK(prP2pRoleFsmInfo != NULL);

	/* Change station state. */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* Reset Station Record Status. */
	p2pFuncResetStaRecStatus(prAdapter, prStaRec);

	/* Try to remove StaRec in BSS client list before free it */
	bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

	/* STA_RECORD free */
	cnmStaRecFree(prAdapter, prStaRec);

	if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
		(bssGetClientCount(prAdapter, prP2pBssInfo) == 0)) {
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
			DBGLOG(P2P, TRACE,
				"No More Client, Media Status DISCONNECTED\n");
		else
			DBGLOG(P2P, TRACE,
				"Deauth done, Media Status DISCONNECTED\n");
		p2pChangeMediaState(prAdapter,
			prP2pBssInfo,
			MEDIA_STATE_DISCONNECTED);
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					prP2pRoleFsmInfo->ucRoleIndex,
					NULL, NULL, 0, 0,
					WLAN_STATUS_MEDIA_DISCONNECT);
#else
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					prP2pRoleFsmInfo->ucRoleIndex,
					NULL, NULL, 0, 0);
#endif
		}
		if (prP2PInfo && prP2PInfo->eChnlSwitchPolicy ==
				CHNL_SWITCH_POLICY_DEAUTH) {
			prP2PInfo->eChnlSwitchPolicy = CHNL_SWITCH_POLICY_NONE;
			p2pFunChnlSwitchNotifyDone(prAdapter);
		}
	}

	/* STOP BSS if power is IDLE */
	if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
		if (IS_NET_PWR_STATE_IDLE(prAdapter,
			prP2pRoleFsmInfo->ucBssIndex)
			&& (bssGetClientCount(prAdapter, prP2pBssInfo) == 0)) {
			/* All Peer disconnected !! Stop BSS now!! */
			p2pFuncStopComplete(prAdapter, prP2pBssInfo);
		} else if (eOriMediaStatus != prP2pBssInfo->eConnectionState)
			/* Update the Media State if necessary */
			nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);
	} else /* GC : Stop BSS when Deauth done */
		p2pFuncStopComplete(prAdapter, prP2pBssInfo);

}

void p2pRoleFsmDeauthTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr)
{
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) ulParamPtr;

	p2pRoleFsmDeauthComplete(prAdapter, prStaRec);
}				/* p2pRoleFsmRunEventTimeout */

void p2pRoleFsmRunEventAbort(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL));

		if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
			/* Get into IDLE state. */
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
		}

		/* Abort IDLE. */
		p2pRoleStateAbort_IDLE(prAdapter,
			prP2pRoleFsmInfo,
			&(prP2pRoleFsmInfo->rChnlReqInfo));

	} while (FALSE);
}				/* p2pRoleFsmRunEventAbort */

uint32_t
p2pRoleFsmRunEventDeauthTxDone(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo,
		IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(P2P, INFO,
			"Deauth TX Done, seq = %d, sta = %d, status = %d\n",
			prMsduInfo->ucTxSeqNum,
			prMsduInfo->ucStaRecIndex,
			rTxDoneStatus);

		prStaRec = cnmGetStaRecByIndex(prAdapter,
			prMsduInfo->ucStaRecIndex);

		if (prStaRec == NULL) {
			DBGLOG(P2P, TRACE,
				"Station Record NULL, Index:%d\n",
				prMsduInfo->ucStaRecIndex);
			break;
		}

		p2pRoleFsmDeauthComplete(prAdapter, prStaRec);
		/* Avoid re-entry */
		cnmTimerStopTimer(prAdapter, &(prStaRec->rDeauthTxDoneTimer));

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* p2pRoleFsmRunEventDeauthTxDone */

void p2pRoleFsmRunEventRxDeauthentication(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	uint16_t u2ReasonCode = 0;
	/* flag to send deauth when rx sta disassc/deauth */
	u_int8_t fgSendDeauth = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		if (prStaRec == NULL)
			prStaRec = cnmGetStaRecByIndex(prAdapter,
				prSwRfb->ucStaRecIdx);

		if (!prStaRec)
			break;

		prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

		if (prStaRec->ucStaState == STA_STATE_1)
			break;

		DBGLOG(P2P, TRACE, "RX Deauth\n");

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_INFRASTRUCTURE:
			if (authProcessRxDeauthFrame(prSwRfb,
				prStaRec->aucMacAddr,
				&u2ReasonCode) == WLAN_STATUS_SUCCESS) {
				struct WLAN_DEAUTH_FRAME *prDeauthFrame =
					(struct WLAN_DEAUTH_FRAME *)
						prSwRfb->pvHeader;
				uint16_t u2IELength = 0;

				if (prP2pBssInfo->prStaRecOfAP != prStaRec)
					break;

				prStaRec->u2ReasonCode = u2ReasonCode;
				u2IELength = prSwRfb->u2PacketLen
					- (WLAN_MAC_HEADER_LEN
					+ REASON_CODE_FIELD_LEN);

#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
/* Indicate disconnect to Host. */
				kalP2PGCIndicateConnectionStatus(
					prAdapter->prGlueInfo,
					(uint8_t) prP2pBssInfo->u4PrivateData,
					NULL,
					prDeauthFrame->aucInfoElem,
					u2IELength,
					u2ReasonCode,
					WLAN_STATUS_MEDIA_DISCONNECT);

#else
/* Indicate disconnect to Host. */
				kalP2PGCIndicateConnectionStatus(
					prAdapter->prGlueInfo,
					(uint8_t) prP2pBssInfo->u4PrivateData,
					NULL,
					prDeauthFrame->aucInfoElem,
					u2IELength,
					u2ReasonCode);
#endif

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter,
					prP2pBssInfo,
					prStaRec,
					FALSE,
					u2ReasonCode);

				SET_NET_PWR_STATE_IDLE(prAdapter,
					prP2pBssInfo->ucBssIndex);

				p2pRoleFsmStateTransition(prAdapter,
					P2P_ROLE_INDEX_2_ROLE_FSM_INFO(
						prAdapter,
						prP2pBssInfo->u4PrivateData),
						P2P_ROLE_STATE_IDLE);

				p2pFuncStopComplete(prAdapter, prP2pBssInfo);
			}
			break;
		case OP_MODE_ACCESS_POINT:
			/* Delete client from client list. */
			if (authProcessRxDeauthFrame(prSwRfb,
				prP2pBssInfo->aucBSSID,
				&u2ReasonCode) == WLAN_STATUS_SUCCESS) {
#if CFG_SUPPORT_802_11W
				/* AP PMF */
				if (rsnCheckBipKeyInstalled(prAdapter,
					prStaRec)) {
					if (prSwRfb->fgIsCipherMS ||
						prSwRfb->fgIsCipherLenMS) {
						/* if cipher mismatch,
						 * or incorrect encrypt,
						 * just drop
						 */
						DBGLOG(P2P, ERROR,
							"Rx deauth CM/CLM=1\n");
						return;
					}

					/* 4.3.3.1 send unprotected deauth
					 * reason 6/7
					 */
					DBGLOG(P2P, INFO, "deauth reason=6\n");
					fgSendDeauth = TRUE;
					u2ReasonCode = REASON_CODE_CLASS_2_ERR;
					prStaRec->rPmfCfg.fgRxDeauthResp = TRUE;
				}
#endif

				if (bssRemoveClient(prAdapter,
					prP2pBssInfo, prStaRec)) {
					/* Indicate disconnect to Host. */
					p2pFuncDisconnect(prAdapter,
						prP2pBssInfo,
						prStaRec,
						fgSendDeauth,
						u2ReasonCode);
					/* Deactive BSS
					 * if PWR is IDLE and no peer
					 */
					if (IS_NET_PWR_STATE_IDLE(prAdapter,
						prP2pBssInfo->ucBssIndex) &&
						(bssGetClientCount(prAdapter,
						prP2pBssInfo) == 0)) {
						/* All Peer disconnected !!
						 * Stop BSS now!!
						 */
						p2pFuncStopComplete(prAdapter,
							prP2pBssInfo);
					}
				}
			}
			break;
		case OP_MODE_P2P_DEVICE:
		default:
			/* Findout why someone
			 * sent deauthentication frame to us.
			 */
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, TRACE, "Deauth Reason:%d\n", u2ReasonCode);

	} while (FALSE);
}				/* p2pRoleFsmRunEventRxDeauthentication */

void p2pRoleFsmRunEventRxDisassociation(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	uint16_t u2ReasonCode = 0;
	/* flag to send deauth when rx sta disassc/deauth */
	u_int8_t fgSendDeauth = FALSE;


	if (prStaRec == NULL)
		prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (!prStaRec) {
		DBGLOG(P2P, ERROR,
			"prStaRec of prSwRfb->ucStaRecIdx %d is NULL!\n",
			prSwRfb->ucStaRecIdx);
		return;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	if (prStaRec->ucStaState == STA_STATE_1)
		return;

	DBGLOG(P2P, TRACE, "RX Disassoc\n");

	switch (prP2pBssInfo->eCurrentOPMode) {
	case OP_MODE_INFRASTRUCTURE:
		if (assocProcessRxDisassocFrame(prAdapter,
			prSwRfb,
			prStaRec->aucMacAddr,
			&prStaRec->u2ReasonCode) == WLAN_STATUS_SUCCESS) {

			struct WLAN_DISASSOC_FRAME *prDisassocFrame =
				(struct WLAN_DISASSOC_FRAME *)
					prSwRfb->pvHeader;
			uint16_t u2IELength = 0;

			if (prP2pBssInfo->prStaRecOfAP != prStaRec)
				break;

			u2IELength = prSwRfb->u2PacketLen
				- (WLAN_MAC_HEADER_LEN + REASON_CODE_FIELD_LEN);

#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				(uint8_t) prP2pBssInfo->u4PrivateData, NULL,
				prDisassocFrame->aucInfoElem,
				u2IELength, prStaRec->u2ReasonCode,
				WLAN_STATUS_MEDIA_DISCONNECT);

#else
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				(uint8_t) prP2pBssInfo->u4PrivateData, NULL,
				prDisassocFrame->aucInfoElem,
				u2IELength, prStaRec->u2ReasonCode);
#endif

			prP2pBssInfo->prStaRecOfAP = NULL;

			p2pFuncDisconnect(prAdapter,
				prP2pBssInfo,
				prStaRec,
				FALSE,
				prStaRec->u2ReasonCode);

			SET_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex);

			p2pRoleFsmStateTransition(prAdapter,
				P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
					prP2pBssInfo->u4PrivateData),
					P2P_ROLE_STATE_IDLE);

			p2pFuncStopComplete(prAdapter, prP2pBssInfo);
		}
		break;
	case OP_MODE_ACCESS_POINT:
		/* Delete client from client list. */
		if (assocProcessRxDisassocFrame(prAdapter,
			prSwRfb,
			prP2pBssInfo->aucBSSID,
			&u2ReasonCode) == WLAN_STATUS_SUCCESS) {

#if CFG_SUPPORT_802_11W
			/* AP PMF */
			if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
				if (prSwRfb->fgIsCipherMS ||
					prSwRfb->fgIsCipherLenMS) {
					/* if cipher mismatch,
					 * or incorrect encrypt, just drop
					 */
					DBGLOG(P2P, ERROR,
						"Rx disassoc CM/CLM=1\n");
					return;
				}

				/* 4.3.3.1 send unprotected deauth
				 * reason 6/7
				 */
				DBGLOG(P2P, INFO, "deauth reason=6\n");
				fgSendDeauth = TRUE;
				u2ReasonCode = REASON_CODE_CLASS_2_ERR;
				prStaRec->rPmfCfg.fgRxDeauthResp = TRUE;
			}
#endif

			if (bssRemoveClient(prAdapter,
				prP2pBssInfo, prStaRec)) {
				/* Indicate disconnect to Host. */
				p2pFuncDisconnect(prAdapter,
					prP2pBssInfo,
					prStaRec,
					fgSendDeauth,
					u2ReasonCode);
				/* Deactive BSS if PWR is IDLE and no peer */
				if (IS_NET_PWR_STATE_IDLE(prAdapter,
					prP2pBssInfo->ucBssIndex) &&
					(bssGetClientCount(prAdapter,
					prP2pBssInfo) == 0)) {
					/* All Peer disconnected !!
					 * Stop BSS now!!
					 */
					p2pFuncStopComplete(prAdapter,
						prP2pBssInfo);
				}

			}
		}
		break;
	case OP_MODE_P2P_DEVICE:
	default:
		ASSERT(FALSE);
		break;
	}

}				/* p2pRoleFsmRunEventRxDisassociation */

void p2pRoleFsmRunEventBeaconTimeout(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pBssInfo)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		prP2pRoleFsmInfo =
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				prP2pBssInfo->u4PrivateData);

		/* Only client mode would have beacon lost event. */
		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE) {
			DBGLOG(P2P, ERROR,
			       "Error case, P2P BSS %d not INFRA mode but beacon timeout\n",
			       prP2pRoleFsmInfo->ucRoleIndex);
			break;
		}

		DBGLOG(P2P, TRACE,
			"p2pFsmRunEventBeaconTimeout: BSS %d Beacon Timeout\n",
			prP2pRoleFsmInfo->ucRoleIndex);

		if (prP2pBssInfo->eConnectionState
			== MEDIA_STATE_CONNECTED) {

#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					prP2pRoleFsmInfo->ucRoleIndex,
					NULL, NULL, 0,
					REASON_CODE_DEAUTH_LEAVING_BSS,
					WLAN_STATUS_MEDIA_DISCONNECT);


#else
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					prP2pRoleFsmInfo->ucRoleIndex,
					NULL, NULL, 0,
					REASON_CODE_DEAUTH_LEAVING_BSS);
#endif

			if (prP2pBssInfo->prStaRecOfAP != NULL) {
				struct STA_RECORD *prStaRec =
					prP2pBssInfo->prStaRecOfAP;

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter,
					prP2pBssInfo,
					prStaRec, FALSE,
					REASON_CODE_DISASSOC_LEAVING_BSS);

				SET_NET_PWR_STATE_IDLE(prAdapter,
					prP2pBssInfo->ucBssIndex);
				/* 20120830 moved into p2pFuncDisconnect() */
				/* cnmStaRecFree(prAdapter,
				 * prP2pBssInfo->prStaRecOfAP);
				 */
				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_IDLE);

				p2pFuncStopComplete(prAdapter, prP2pBssInfo);
			}
		}
	} while (FALSE);
}				/* p2pFsmRunEventBeaconTimeout */

/*================== Message Event ==================*/
void p2pRoleFsmRunEventStartAP(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_START_AP *prP2pStartAPMsg =
		(struct MSG_P2P_START_AP *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo =
		(struct P2P_SPECIFIC_BSS_INFO *) NULL;

	DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventStartAP\n");

	prP2pStartAPMsg = (struct MSG_P2P_START_AP *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pStartAPMsg->ucRoleIdx);

	prAdapter->prP2pInfo->eConnState = P2P_CNN_NORMAL;

	DBGLOG(P2P, TRACE,
		"p2pRoleFsmRunEventStartAP with Role(%d)\n",
		prP2pStartAPMsg->ucRoleIdx);


	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventStartAP: Corresponding P2P Role FSM empty: %d.\n",
		       prP2pStartAPMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar
			.prP2pSpecificBssInfo[prP2pBssInfo->u4PrivateData];
	prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

	if (prP2pStartAPMsg->u4BcnInterval) {
		DBGLOG(P2P, TRACE,
			"Beacon interval updated to :%u\n",
			prP2pStartAPMsg->u4BcnInterval);
		prP2pBssInfo->u2BeaconInterval =
			(uint16_t) prP2pStartAPMsg->u4BcnInterval;
	} else if (prP2pBssInfo->u2BeaconInterval == 0) {
		prP2pBssInfo->u2BeaconInterval = DOT11_BEACON_PERIOD_DEFAULT;
	}

	if (prP2pStartAPMsg->u4DtimPeriod) {
		DBGLOG(P2P, TRACE,
			"DTIM interval updated to :%u\n",
			prP2pStartAPMsg->u4DtimPeriod);
		prP2pBssInfo->ucDTIMPeriod =
			(uint8_t) prP2pStartAPMsg->u4DtimPeriod;
	} else if (prP2pBssInfo->ucDTIMPeriod == 0) {
		prP2pBssInfo->ucDTIMPeriod = DOT11_DTIM_PERIOD_DEFAULT;
	}

	if (prP2pStartAPMsg->u2SsidLen != 0) {
		kalMemCopy(prP2pConnReqInfo->rSsidStruct.aucSsid,
			prP2pStartAPMsg->aucSsid,
			prP2pStartAPMsg->u2SsidLen);
		prP2pConnReqInfo->rSsidStruct.ucSsidLen =
		    prP2pSpecificBssInfo->u2GroupSsidLen =
		    prP2pStartAPMsg->u2SsidLen;
		kalMemCopy(prP2pSpecificBssInfo->aucGroupSsid,
			prP2pStartAPMsg->aucSsid,
			prP2pStartAPMsg->u2SsidLen);
	}

	if (p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pStartAPMsg->ucRoleIdx])) {
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_PURE_AP;

		/* Overwrite AP channel */
		if (prAdapter->rWifiVar.ucApChannel &&
			prAdapter->rWifiVar.ucApChnlDefFromCfg) {
			prP2pConnReqInfo->rChannelInfo.ucChannelNum =
				prAdapter->rWifiVar.ucApChannel;

			if (prAdapter->rWifiVar.ucApChannel <= 14)
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_2G4;
			else
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_5G;
		}
	} else {
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GO;
#if CFG_P2P_DEFAULT_CLIENT_COUNT
		kalP2PSetMaxClients(prAdapter->prGlueInfo,
			P2P_DEFAULT_CLIENT_COUNT,
			prP2pStartAPMsg->ucRoleIdx);
#endif
	}

	/* Clear list to ensure no client staRec */
	if (bssGetClientCount(prAdapter, prP2pBssInfo) != 0) {
		DBGLOG(P2P, WARN,
			"Clear list to ensure no empty/client staRec\n");
		bssInitializeClientList(prAdapter, prP2pBssInfo);
	}

	/* The supplicant may start AP
	 * before rP2pRoleFsmTimeoutTimer is time out
	 */
	/* We need to make sure the BSS was deactivated
	 * and all StaRec can be free
	 */
	if (timerPendingTimer(&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer))) {
		/* call p2pRoleFsmRunEventTimeout()
		 * to deactive BSS and free channel
		 */
		p2pRoleFsmRunEventTimeout(prAdapter,
			(unsigned long)prP2pRoleFsmInfo);
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer));
	}

#if (CFG_SUPPORT_DFS_MASTER == 1)
	if (timerPendingTimer(&(prP2pRoleFsmInfo->rDfsShutDownTimer))) {
		DBGLOG(P2P, INFO,
			"p2pRoleFsmRunEventStartAP: Stop DFS shut down timer.\n");
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer));
	}
#endif

	prP2pBssInfo->eBand = prP2pConnReqInfo->rChannelInfo.eBand;
	if (prP2pBssInfo->fgIsWmmInited == FALSE)
		prP2pBssInfo->ucWmmQueSet = cnmWmmIndexDecision(prAdapter,
			prP2pBssInfo);
#if CFG_SUPPORT_DBDC
	/* DBDC decsion.may change OpNss */
	cnmDbdcPreConnectionEnableDecision(prAdapter,
		prP2pBssInfo->ucBssIndex,
		prP2pConnReqInfo->rChannelInfo.eBand,
		prP2pConnReqInfo->rChannelInfo.ucChannelNum,
		prP2pBssInfo->ucWmmQueSet);
#endif /*CFG_SUPPORT_DBDC*/

	cnmOpModeGetTRxNss(
		prAdapter, prP2pBssInfo->ucBssIndex,
		&prP2pBssInfo->ucOpRxNss, &prP2pBssInfo->ucOpTxNss);
	prP2pBssInfo->eHiddenSsidType = prP2pStartAPMsg->ucHiddenSsidType;

	DBGLOG(P2P, TRACE,
		"p2pRoleFsmRunEventStartAP: start AP CH[%u]",
		prP2pConnReqInfo->rChannelInfo.ucChannelNum);
	DBGLOG(P2P, TRACE, "RxNSS[%u]TxNss[%u].\n",
		prP2pBssInfo->ucOpRxNss, prP2pBssInfo->ucOpTxNss);
	/*
	 * beacon content is related with Nss number ,
	 * need to update because of modification
	 */
	bssUpdateBeaconContent(prAdapter, prP2pBssInfo->ucBssIndex);

	if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
	    (prP2pBssInfo->eIntendOPMode != OP_MODE_NUM)) {
		/* 1. No switch to AP mode.
		 * 2. Not started yet.
		 */

		if (prP2pRoleFsmInfo->eCurrentState
			!= P2P_ROLE_STATE_AP_CHNL_DETECTION
			&&
		    prP2pRoleFsmInfo->eCurrentState
		    != P2P_ROLE_STATE_IDLE) {
			/* Make sure the state is in IDLE state. */
			p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
		} else if (prP2pRoleFsmInfo->eCurrentState
					== P2P_ROLE_STATE_AP_CHNL_DETECTION) {
			goto error;
		}

		/* Leave IDLE state. */
		SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

		prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;

#if 0
		prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum = 8;
		prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.eBand = BAND_2G4;
		/* prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucBandwidth =
		 * 0;
		 */
		/* prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.eSCO =
		 * CHNL_EXT_SCN;
		 */
#endif

		if (prP2pRoleFsmInfo->rConnReqInfo
			.rChannelInfo.ucChannelNum != 0) {
			DBGLOG(P2P, INFO,
				"Role(%d) StartAP at CH[%u]RxNSS[%u]TxNss[%u]\n",
				prP2pStartAPMsg->ucRoleIdx,
				prP2pRoleFsmInfo->rConnReqInfo
					.rChannelInfo.ucChannelNum,
				prP2pBssInfo->ucOpRxNss,
				prP2pBssInfo->ucOpTxNss);

			p2pRoleStatePrepare_To_REQING_CHANNEL_STATE(
				prAdapter,
				GET_BSS_INFO_BY_INDEX(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex),
				&(prP2pRoleFsmInfo->rConnReqInfo),
				&(prP2pRoleFsmInfo->rChnlReqInfo));
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_REQING_CHANNEL);
		} else {
			DBGLOG(P2P, INFO,
				"Role(%d) StartAP Scan for working channel\n",
				prP2pStartAPMsg->ucRoleIdx);

			/* For AP/GO mode with specific channel
			 * or non-specific channel.
			 */
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_AP_CHNL_DETECTION);
		}
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventStartAP */


void p2pRoleFsmRunEventDelIface(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_DEL_IFACE *prP2pDelIfaceMsg =
		(struct MSG_P2P_DEL_IFACE *) NULL;
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;
	uint8_t ucRoleIdx;
	struct GL_P2P_INFO *prP2pInfo = (struct GL_P2P_INFO *) NULL;


	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventDelIface\n");

	prGlueInfo = prAdapter->prGlueInfo;
	if (prGlueInfo == NULL) {
		DBGLOG(P2P, ERROR, "prGlueInfo shouldn't be NULL!\n");
		goto error;
	}

	prP2pDelIfaceMsg = (struct MSG_P2P_DEL_IFACE *) prMsgHdr;
	ucRoleIdx = prP2pDelIfaceMsg->ucRoleIdx;
	prAdapter = prGlueInfo->prAdapter;
	prP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx);
	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
			   "p2pRoleFsmRunEventDelIface: Corresponding P2P Role FSM empty: %d.\n",
			   prP2pDelIfaceMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	/* The state is in disconnecting and can not change any BSS status */
	if (IS_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex) &&
		IS_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex)) {
		DBGLOG(P2P, TRACE, "under deauth procedure, Quit.\n");
	} else {
		/*p2pFuncDissolve(prAdapter,
		 * prP2pBssInfo, TRUE,
		 * REASON_CODE_DEAUTH_LEAVING_BSS);
		 */

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Function Dissolve should already enter IDLE state. */
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);

		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);

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
		UNSET_NET_ACTIVE(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		nicDeactivateNetwork(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
		nicUpdateBss(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventStartAP */


void p2pRoleFsmRunEventStopAP(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_STOP_AP *prP2pStopApMsg;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct STA_RECORD *prCurrStaRec;
	struct LINK *prClientList;

	prP2pStopApMsg = (struct MSG_P2P_STOP_AP *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
		prP2pStopApMsg->ucRoleIdx);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventStopAP: Corresponding P2P Role FSM empty: %d.\n",
		       prP2pStopApMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo of prP2pRoleFsmInfo->ucBssIndex %d is NULL!\n",
			prP2pRoleFsmInfo->ucBssIndex);
		goto error;
	}

#if (CFG_SUPPORT_DFS_MASTER == 1)
	p2pFuncSetDfsState(DFS_STATE_INACTIVE);
	p2pFuncStopRdd(prAdapter, prP2pBssInfo->ucBssIndex);
#endif

	rsnFlushPmkid(prAdapter, prP2pBssInfo->ucBssIndex);

	kalP2PResetBlackList(prAdapter->prGlueInfo,
		prP2pStopApMsg->ucRoleIdx);

	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_REQING_CHANNEL) {
		p2pFuncStopGO(prAdapter, prP2pBssInfo);

		/* Start all Deauth done timer for all client */
		prClientList = &prP2pBssInfo->rStaRecOfClientList;

		LINK_FOR_EACH_ENTRY(prCurrStaRec,
			prClientList, rLinkEntry, struct STA_RECORD) {
			if (!prCurrStaRec)
				break;
			/* Do not restart timer if the timer is pending, */
			/* (start in p2pRoleFsmRunEventConnectionAbort()) */
			if (!timerPendingTimer(
				&(prCurrStaRec->rDeauthTxDoneTimer))) {
				cnmTimerInitTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					(PFN_MGMT_TIMEOUT_FUNC)
					p2pRoleFsmDeauthTimeout,
					(unsigned long) prCurrStaRec);

				cnmTimerStartTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					P2P_DEAUTH_TIMEOUT_TIME_MS);
			}
		}
	}

	SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

	p2pRoleFsmStateTransition(prAdapter,
		prP2pRoleFsmInfo,
		P2P_ROLE_STATE_IDLE);

error:
	cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventStopAP */

#if (CFG_SUPPORT_DFS_MASTER == 1)
void p2pRoleFsmRunEventDfsCac(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_DFS_CAC *prP2pDfsCacMsg =
		(struct MSG_P2P_DFS_CAC *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	enum ENUM_CHANNEL_WIDTH rChannelWidth;

	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventDfsCac\n");

	prP2pDfsCacMsg = (struct MSG_P2P_DFS_CAC *) prMsgHdr;

	rChannelWidth = prP2pDfsCacMsg->eChannelWidth;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pDfsCacMsg->ucRoleIdx);

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventDfsCac with Role(%d)\n",
		prP2pDfsCacMsg->ucRoleIdx);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventDfsCac: Corresponding P2P Role FSM empty: %d.\n",
		       prP2pDfsCacMsg->ucRoleIdx);
		goto error;
	}

	if (timerPendingTimer(&(prP2pRoleFsmInfo->rDfsShutDownTimer))) {
		DBGLOG(P2P, INFO,
			"p2pRoleFsmRunEventDfsCac: Stop DFS shut down timer.\n");
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer));
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

	if (p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pDfsCacMsg->ucRoleIdx]))
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_PURE_AP;
	else
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GO;

	prP2pBssInfo->eBand = prP2pConnReqInfo->rChannelInfo.eBand;
	if (prP2pBssInfo->fgIsWmmInited == FALSE)
		prP2pBssInfo->ucWmmQueSet = cnmWmmIndexDecision(prAdapter,
			prP2pBssInfo);

#if CFG_SUPPORT_DBDC
	/* DBDC decsion.may change OpNss */
	cnmDbdcPreConnectionEnableDecision(prAdapter,
		prP2pBssInfo->ucBssIndex,
		prP2pConnReqInfo->rChannelInfo.eBand,
		prP2pConnReqInfo->rChannelInfo.ucChannelNum,
		prP2pBssInfo->ucWmmQueSet
	);
#endif /*CFG_SUPPORT_DBDC*/

	cnmOpModeGetTRxNss(
		prAdapter, prP2pBssInfo->ucBssIndex,
		&prP2pBssInfo->ucOpRxNss, &prP2pBssInfo->ucOpTxNss);

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventDfsCac: CH[%u]RxNSS[%u]TxNss[%u].\n",
		prP2pConnReqInfo->rChannelInfo.ucChannelNum,
		prP2pBssInfo->ucOpRxNss,
		prP2pBssInfo->ucOpTxNss);

	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
		/* Make sure the state is in IDLE state. */
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	}

	/* Leave IDLE state. */
	SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

	prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;
	prP2pBssInfo->fgIsDfsActive = TRUE;

	if (prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum != 0) {
		DBGLOG(P2P, INFO, "Role(%d) Set channel at CH(%d)\n",
			prP2pDfsCacMsg->ucRoleIdx,
			prP2pRoleFsmInfo->rConnReqInfo
				.rChannelInfo.ucChannelNum);

		p2pRoleStatePrepare_To_DFS_CAC_STATE(prAdapter,
				GET_BSS_INFO_BY_INDEX(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex),
				rChannelWidth,
				&(prP2pRoleFsmInfo->rConnReqInfo),
				&(prP2pRoleFsmInfo->rChnlReqInfo));
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_DFS_CAC);
	} else {
		DBGLOG(P2P, ERROR,
			"prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum shouldn't be 0\n");
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventDfsCac*/

void p2pRoleFsmRunEventRadarDet(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_RADAR_DETECT *prMsgP2pRddDetMsg;


	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventRadarDet\n");

	prMsgP2pRddDetMsg = (struct MSG_P2P_RADAR_DETECT *) prMsgHdr;

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prMsgP2pRddDetMsg->ucBssIndex);

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventRadarDet with Role(%d)\n",
		prP2pRoleFsmInfo->ucRoleIndex);

	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_DFS_CAC &&
		prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
		DBGLOG(P2P, ERROR,
			"Wrong prP2pRoleFsmInfo->eCurrentState \"%s\"!",
			(prP2pRoleFsmInfo->eCurrentState < P2P_ROLE_STATE_NUM
			? (const char *)
			p2pRoleFsmGetFsmState(prP2pRoleFsmInfo->eCurrentState)
			: ""));
		goto error;
	}

	if (p2pFuncGetRadarDetectMode()) {
		DBGLOG(P2P, INFO,
			"p2pRoleFsmRunEventRadarDet: Ignore radar event\n");
		if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_DFS_CAC)
			p2pFuncSetDfsState(DFS_STATE_CHECKING);
		else
			p2pFuncSetDfsState(DFS_STATE_ACTIVE);
	} else {
		if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_DFS_CAC)
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);

		kalP2PRddDetectUpdate(prAdapter->prGlueInfo,
			prP2pRoleFsmInfo->ucRoleIndex);
		cnmTimerStartTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer),
			5000);
	}

	p2pFuncShowRadarInfo(prAdapter, prMsgP2pRddDetMsg->ucBssIndex);

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventRadarDet*/

void p2pRoleFsmRunEventSetNewChannel(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_SET_NEW_CHANNEL *prMsgP2pSetNewChannelMsg;


	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventSetNewChannel\n");

	prMsgP2pSetNewChannelMsg = (struct MSG_P2P_SET_NEW_CHANNEL *) prMsgHdr;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsgP2pSetNewChannelMsg->ucBssIndex);

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prMsgP2pSetNewChannelMsg->ucRoleIdx);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
			"Corresponding P2P Role FSM empty: %d.\n",
			prMsgP2pSetNewChannelMsg->ucRoleIdx);
		goto error;
	}

	prP2pRoleFsmInfo->rChnlReqInfo.ucReqChnlNum =
		prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum;
	prP2pRoleFsmInfo->rChnlReqInfo.eBand =
		prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.eBand;
	prP2pRoleFsmInfo->rChnlReqInfo.eChannelWidth =
		prMsgP2pSetNewChannelMsg->eChannelWidth;

	prP2pRoleFsmInfo->rChnlReqInfo.ucCenterFreqS1 =
		nicGetVhtS1(prP2pRoleFsmInfo->rChnlReqInfo.ucReqChnlNum,
		prP2pRoleFsmInfo->rChnlReqInfo.eChannelWidth);

	prP2pRoleFsmInfo->rChnlReqInfo.ucCenterFreqS2 = 0;

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventSetNewChannel*/

void p2pRoleFsmRunEventCsaDone(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct GLUE_INFO *prGlueInfo;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_CSA_DONE *prMsgP2pCsaDoneMsg;
	uint8_t role_idx = 0;
	struct BSS_INFO *prAisBssInfo;
	struct GL_P2P_INFO *prP2PInfo = (struct GL_P2P_INFO *) NULL;

	DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventCsaDone\n");

	prMsgP2pCsaDoneMsg = (struct MSG_P2P_CSA_DONE *) prMsgHdr;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsgP2pCsaDoneMsg->ucBssIndex);
	prAisBssInfo = aisGetConnectedBssInfo(prAdapter);

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);
	prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[
			prP2pRoleFsmInfo->ucRoleIndex];

	if (prP2PInfo)
		prP2PInfo->eChnlSwitchPolicy = CHNL_SWITCH_POLICY_NONE;

	if (prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_DISABLED &&
		prP2pBssInfo->eBand != prP2pRoleFsmInfo->rChnlReqInfo.eBand) {
		nicDeactivateNetwork(prAdapter, prP2pBssInfo->ucBssIndex);
		nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);
		nicActivateNetwork(prAdapter, prP2pBssInfo->ucBssIndex);

#if CFG_SUPPORT_DBDC
		cnmDbdcPreConnectionEnableDecision(prAdapter,
			prP2pBssInfo->ucBssIndex,
			prP2pRoleFsmInfo->rChnlReqInfo.eBand,
			prP2pRoleFsmInfo->rChnlReqInfo.ucReqChnlNum,
			prP2pBssInfo->ucWmmQueSet);
#endif /*CFG_SUPPORT_DBDC*/

		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_SWITCH_CHANNEL);
	} else {
		prGlueInfo = prAdapter->prGlueInfo;
		role_idx = prP2pRoleFsmInfo->ucRoleIndex;
		/* Skip channel request/abort for STA+SAP/MCC concurrent case */
		if (prAisBssInfo &&
			(prAisBssInfo->ucPrimaryChannel !=
			prP2pBssInfo->ucPrimaryChannel) &&
			(prAisBssInfo->eConnectionState ==
			MEDIA_STATE_CONNECTED)) {
			p2pFuncDfsSwitchCh(prAdapter,
				prP2pBssInfo,
				prP2pRoleFsmInfo->rChnlReqInfo);
		} else {
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_SWITCH_CHANNEL);
		}
	}

	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventCsaDone*/

void p2pRoleFsmRunEventDfsShutDownTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventDfsShutDownTimeout: DFS shut down.\n");

	p2pFuncSetDfsState(DFS_STATE_INACTIVE);
	p2pFuncStopRdd(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

}				/* p2pRoleFsmRunEventDfsShutDownTimeout */

#endif

void
p2pRoleFsmScanTargetBss(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		IN uint8_t ucChannelNum,
		IN struct P2P_SSID_STRUCT *prSsid)
{
	/* Update scan parameter... to scan target device. */
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
			&(prP2pRoleFsmInfo->rScanReqInfo);

	prScanReqInfo->ucNumChannelList = 1;
	prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;
	prScanReqInfo->arScanChannelList[0].ucChannelNum = ucChannelNum;
	prScanReqInfo->ucSsidNum = 1;
	kalMemCopy(&(prScanReqInfo->arSsidStruct[0]), prSsid,
			sizeof(struct P2P_SSID_STRUCT));
	/* Prevent other P2P ID in IE. */
	prScanReqInfo->u4BufLength = 0;
	prScanReqInfo->fgIsAbort = TRUE;

	p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_SCAN);
}

void p2pRoleFsmRunEventConnectionRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_CONNECTION_REQUEST *prP2pConnReqMsg =
		(struct MSG_P2P_CONNECTION_REQUEST *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;

	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct P2P_JOIN_INFO *prJoinInfo = (struct P2P_JOIN_INFO *) NULL;
	uint8_t ucRfBw;

	prP2pConnReqMsg = (struct MSG_P2P_CONNECTION_REQUEST *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pConnReqMsg->ucRoleIdx);

	prAdapter->prP2pInfo->eConnState = P2P_CNN_NORMAL;


	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "Corresponding P2P Role FSM empty: %d.\n",
		       prP2pConnReqMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR,
			"prP2pRoleFsmInfo->ucBssIndex %d of prAdapter->aprBssInfo is NULL!\n",
			prP2pRoleFsmInfo->ucBssIndex);
		goto error;
	}

	prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
	prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

	DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionRequest\n");

	if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		goto error;

	SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

	/* In P2P GC case, the interval of
	 * two ASSOC flow could be very short,
	 * we must start to connect directly before Deauth done
	 */
	prStaRec = prP2pBssInfo->prStaRecOfAP;
	if (prStaRec) {
		if (timerPendingTimer(&prStaRec->rDeauthTxDoneTimer)) {
			cnmTimerStopTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer));
			/* Force to stop */
			p2pRoleFsmDeauthComplete(prAdapter, prStaRec);
		}
	}
	/* Make sure the state is in IDLE state. */
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE)
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	/* Update connection request information. */
	prConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GC;
	COPY_MAC_ADDR(prConnReqInfo->aucBssid,
		prP2pConnReqMsg->aucBssid);
	COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr,
		prP2pConnReqMsg->aucSrcMacAddr);
	kalMemCopy(&(prConnReqInfo->rSsidStruct),
		&(prP2pConnReqMsg->rSsid),
		sizeof(struct P2P_SSID_STRUCT));
	kalMemCopy(prConnReqInfo->aucIEBuf,
		prP2pConnReqMsg->aucIEBuf,
		prP2pConnReqMsg->u4IELen);
	prConnReqInfo->u4BufLength = prP2pConnReqMsg->u4IELen;

	/* Find BSS Descriptor first. */
	prJoinInfo->prTargetBssDesc =
		scanP2pSearchDesc(prAdapter, prConnReqInfo);

	if (prJoinInfo->prTargetBssDesc == NULL) {
		p2pRoleFsmScanTargetBss(prAdapter,
				prP2pRoleFsmInfo,
				prP2pConnReqMsg->rChannelInfo.ucChannelNum,
				&(prP2pConnReqMsg->rSsid));
	} else {
		prChnlReqInfo->u8Cookie = 0;
		prChnlReqInfo->ucReqChnlNum =
			prP2pConnReqMsg->rChannelInfo.ucChannelNum;
		prChnlReqInfo->eBand = prP2pConnReqMsg->rChannelInfo.eBand;
		prChnlReqInfo->eChnlSco = prP2pConnReqMsg->eChnlSco;
		prChnlReqInfo->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;
		prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_JOIN;

		rlmReviseMaxBw(prAdapter,
			prP2pBssInfo->ucBssIndex,
			&prChnlReqInfo->eChnlSco,
			(enum ENUM_CHANNEL_WIDTH *)
			&prChnlReqInfo->eChannelWidth,
			&prChnlReqInfo->ucCenterFreqS1,
			&prChnlReqInfo->ucReqChnlNum);

		prP2pBssInfo->eBand = prChnlReqInfo->eBand;
		if (prP2pBssInfo->fgIsWmmInited == FALSE)
			prP2pBssInfo->ucWmmQueSet =
				cnmWmmIndexDecision(prAdapter, prP2pBssInfo);

#if CFG_SUPPORT_DBDC
		/* DBDC decsion.may change OpNss */
		cnmDbdcPreConnectionEnableDecision(prAdapter,
			prP2pBssInfo->ucBssIndex,
			prChnlReqInfo->eBand,
			prChnlReqInfo->ucReqChnlNum,
			prP2pBssInfo->ucWmmQueSet);
#endif /* CFG_SUPPORT_DBDC */

		cnmOpModeGetTRxNss(
			prAdapter, prP2pBssInfo->ucBssIndex,
			&prP2pBssInfo->ucOpRxNss, &prP2pBssInfo->ucOpTxNss);

		DBGLOG(P2P, INFO,
		   "p2pRoleFsmRunEventConnectionRequest: start GC CH[%u]RxNSS[%u]TxNss[%u]\n",
		   prChnlReqInfo->ucReqChnlNum,
		   prP2pBssInfo->ucOpRxNss,
		   prP2pBssInfo->ucOpTxNss);


		/* Decide RF BW by own OP and Peer OP BW */
		ucRfBw = cnmGetDbdcBwCapability(prAdapter,
			prP2pBssInfo->ucBssIndex);
		/* Revise to VHT OP BW */
		ucRfBw = rlmGetVhtOpBwByBssOpBw(ucRfBw);
		if (ucRfBw > prJoinInfo->prTargetBssDesc->eChannelWidth)
			ucRfBw = prJoinInfo->prTargetBssDesc->eChannelWidth;

		prChnlReqInfo->eChannelWidth = ucRfBw;
		/* TODO: BW80+80 support */
		prChnlReqInfo->ucCenterFreqS1 =
			nicGetVhtS1(prChnlReqInfo->ucReqChnlNum,
				prChnlReqInfo->eChannelWidth);
		prChnlReqInfo->ucCenterFreqS2 = 0;

		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_REQING_CHANNEL);
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventConnectionRequest */

void p2pRoleFsmRunEventConnectionAbort(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_CONNECTION_ABORT *prDisconnMsg =
		(struct MSG_P2P_CONNECTION_ABORT *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;


	prDisconnMsg = (struct MSG_P2P_CONNECTION_ABORT *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prDisconnMsg->ucRoleIdx);

	DBGLOG(P2P, TRACE,
		"p2pFsmRunEventConnectionAbort: Connection Abort.\n");

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventConnectionAbort: Corresponding P2P Role FSM empty: %d.\n",
		       prDisconnMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR,
		       "prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex(%d)] is NULL!",
		       prP2pRoleFsmInfo->ucBssIndex);
		goto error;
	}

	switch (prP2pBssInfo->eCurrentOPMode) {
	case OP_MODE_INFRASTRUCTURE:
		{
			uint8_t aucBCBSSID[] = BC_BSSID;

			if (!prP2pBssInfo->prStaRecOfAP) {
				struct P2P_JOIN_INFO *prJoinInfo;

				DBGLOG(P2P, TRACE, "GO's StaRec is NULL\n");
				/* Receive disconnection request during GC join.
				  * Abort GC join to prevent STA record leak.
				  */
				prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);
				if (prJoinInfo->prTargetStaRec) {
					p2pFuncDisconnect(prAdapter,
						prP2pBssInfo,
						prJoinInfo->prTargetStaRec,
						FALSE,
						REASON_CODE_DEAUTH_LEAVING_BSS);

					p2pRoleFsmStateTransition(prAdapter,
						prP2pRoleFsmInfo,
						P2P_ROLE_STATE_IDLE);
				}
				break;
			}
			if (UNEQUAL_MAC_ADDR(
					prP2pBssInfo->prStaRecOfAP->aucMacAddr,
					prDisconnMsg->aucTargetID) &&
			    UNEQUAL_MAC_ADDR(prDisconnMsg->aucTargetID,
				    aucBCBSSID)) {
				DBGLOG(P2P, TRACE,
				"Unequal MAC ADDR [" MACSTR ":" MACSTR "]\n",
				MAC2STR(
					prP2pBssInfo->prStaRecOfAP->aucMacAddr),
				MAC2STR(prDisconnMsg->aucTargetID));
				break;
			}

			prStaRec = prP2pBssInfo->prStaRecOfAP;

			/* Stop rejoin timer if it is started. */
			/* TODO: If it has. */

			p2pFuncDisconnect(prAdapter, prP2pBssInfo,
				prStaRec,
				prDisconnMsg->fgSendDeauth,
				prDisconnMsg->u2ReasonCode);

			cnmTimerStopTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer));

			cnmTimerInitTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer),
				(PFN_MGMT_TIMEOUT_FUNC) p2pRoleFsmDeauthTimeout,
				(unsigned long) prStaRec);

			cnmTimerStartTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer),
				P2P_DEAUTH_TIMEOUT_TIME_MS);

			SET_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex);

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
		}
		break;
	case OP_MODE_ACCESS_POINT:
		{
			/* Search specific client device, and disconnect. */
			/* 1. Send deauthentication frame. */
			/* 2. Indication: Device disconnect. */
			struct STA_RECORD *prCurrStaRec =
				(struct STA_RECORD *) NULL;

			DBGLOG(P2P, TRACE,
				"Disconnecting with Target ID: " MACSTR "\n",
				MAC2STR(prDisconnMsg->aucTargetID));

			prCurrStaRec = bssGetClientByMac(prAdapter,
				prP2pBssInfo,
				prDisconnMsg->aucTargetID);

			if (prCurrStaRec) {
				DBGLOG(P2P, TRACE,
					"Disconnecting: " MACSTR "\n",
					MAC2STR(prCurrStaRec->aucMacAddr));

				/* Glue layer indication. */
				/* kalP2PGOStationUpdate(prAdapter->prGlueInfo,
				 * prCurrStaRec, FALSE);
				 */

				/* Send deauth & do indication. */
				p2pFuncDisconnect(prAdapter,
					prP2pBssInfo,
					prCurrStaRec,
					prDisconnMsg->fgSendDeauth,
					prDisconnMsg->u2ReasonCode);

				cnmTimerStopTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer));

				cnmTimerInitTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					(PFN_MGMT_TIMEOUT_FUNC)
					p2pRoleFsmDeauthTimeout,
					(unsigned long) prCurrStaRec);

				cnmTimerStartTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					P2P_DEAUTH_TIMEOUT_TIME_MS);
			}
#if 0
			LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
				prCurrStaRec = LINK_ENTRY(prLinkEntry,
					struct STA_RECORD, rLinkEntry);

				ASSERT(prCurrStaRec);

				if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr,
					prDisconnMsg->aucTargetID)) {

					DBGLOG(P2P, TRACE,
					"Disconnecting: " MACSTR "\n",
					MAC2STR(
					prCurrStaRec->aucMacAddr));

					/* Remove STA from client list. */
					LINK_REMOVE_KNOWN_ENTRY(
						prStaRecOfClientList,
						&prCurrStaRec->rLinkEntry);

					/* Glue layer indication. */
					/* kalP2PGOStationUpdate(
					 * prAdapter->prGlueInfo,
					 * prCurrStaRec, FALSE);
					 */

					/* Send deauth & do indication. */
					p2pFuncDisconnect(prAdapter,
						prP2pBssInfo,
						prCurrStaRec,
						prDisconnMsg->fgSendDeauth,
						prDisconnMsg->u2ReasonCode);

					/* prTargetStaRec = prCurrStaRec; */

					break;
				}
			}
#endif

		}
		break;
	case OP_MODE_P2P_DEVICE:
	default:
		ASSERT(FALSE);
		break;
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventConnectionAbort */

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is called when JOIN complete message event
 *             is received from SAA.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void p2pRoleFsmRunEventJoinComplete(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_JOIN_INFO *prJoinInfo =
		(struct P2P_JOIN_INFO *) NULL;
	struct MSG_SAA_FSM_COMP *prJoinCompMsg =
		(struct MSG_SAA_FSM_COMP *) NULL;
	struct SW_RFB *prAssocRspSwRfb = (struct SW_RFB *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *) prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

	ASSERT(prStaRec);
	if (!prStaRec) {
		DBGLOG(P2P, ERROR, "prJoinCompMsg->prStaRec is NULL!\n");
		goto error;
	}

	DBGLOG(P2P, INFO,
		"P2P BSS %d [" MACSTR "], Join Complete, status: %d\n",
		prStaRec->ucBssIndex,
		MAC2STR(prStaRec->aucMacAddr),
		prJoinCompMsg->rJoinStatus);

	ASSERT(prStaRec->ucBssIndex < prAdapter->ucP2PDevBssIdx);
	if (!(prStaRec->ucBssIndex < prAdapter->ucP2PDevBssIdx)) {
		DBGLOG(P2P, ERROR,
			"prStaRec->ucBssIndex %d should < prAdapter->ucP2PDevBssIdx(%d)!\n",
			prStaRec->ucBssIndex, prAdapter->ucP2PDevBssIdx);
		goto error;
	}

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prStaRec->ucBssIndex);

	if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo->eCurrentOPMode %d != OP_MODE_INFRASTRUCTURE(%d)!\n",
			prP2pBssInfo->eCurrentOPMode,
			OP_MODE_INFRASTRUCTURE);
		goto error;
	}

	ASSERT(prP2pBssInfo->u4PrivateData < BSS_P2P_NUM);
	if (!(prP2pBssInfo->u4PrivateData < BSS_P2P_NUM)) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo->u4PrivateData %d should < BSS_P2P_NUM(%d)!\n",
			prP2pBssInfo->u4PrivateData,
			BSS_P2P_NUM);
		goto error;
	}

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

	/* Check SEQ NUM */
	if (prJoinCompMsg->ucSeqNum == prJoinInfo->ucSeqNumOfReqMsg) {
		ASSERT(prStaRec == prJoinInfo->prTargetStaRec);
		prJoinInfo->fgIsJoinComplete = TRUE;

		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {

			/* 4 <1.1> Change FW's Media State immediately. */
			p2pChangeMediaState(prAdapter,
				prP2pBssInfo,
				MEDIA_STATE_CONNECTED);

			/* 4 <1.2> Deactivate previous AP's STA_RECORD_T
			 * in Driver if have.
			 */
			if ((prP2pBssInfo->prStaRecOfAP)
				&& (prP2pBssInfo->prStaRecOfAP != prStaRec)) {
				cnmStaRecChangeState(prAdapter,
					prP2pBssInfo->prStaRecOfAP,
					STA_STATE_1);

				cnmStaRecFree(prAdapter,
					prP2pBssInfo->prStaRecOfAP);

				prP2pBssInfo->prStaRecOfAP = NULL;
			}
			/* 4 <1.3> Update BSS_INFO_T */
			if (prAssocRspSwRfb) {
				p2pFuncUpdateBssInfoForJOIN(prAdapter,
				    prJoinInfo->prTargetBssDesc,
				    prStaRec, prP2pBssInfo, prAssocRspSwRfb);
			} else {
				DBGLOG(P2P, INFO,
					"prAssocRspSwRfb is NULL! Skip p2pFuncUpdateBssInfoForJOIN\n");
			}

			/* 4 <1.4> Activate current AP's STA_RECORD_T
			 * in Driver.
			 */
			cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

#if CFG_SUPPORT_P2P_RSSI_QUERY
			/* <1.5> Update RSSI if necessary */
			nicUpdateRSSI(prAdapter, prP2pBssInfo->ucBssIndex,
				(int8_t) (RCPI_TO_dBm(prStaRec->ucRCPI)), 0);
#endif

			/* 4 <1.6> Indicate Connected Event to
			 * Host immediately.
			 * Require BSSID, Association ID, Beacon Interval..
			 * from AIS_BSS_INFO_T
			 * p2pIndicationOfMediaStateToHost(prAdapter,
			 * MEDIA_STATE_CONNECTED,
			 * prStaRec->aucMacAddr);
			 */
			if (prJoinInfo->prTargetBssDesc)
				scanReportBss2Cfg80211(prAdapter,
					BSS_TYPE_P2P_DEVICE,
					prJoinInfo->prTargetBssDesc);
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				prP2pRoleFsmInfo->ucRoleIndex,
				&prP2pRoleFsmInfo->rConnReqInfo,
				prJoinInfo->aucIEBuf,
				prJoinInfo->u4BufLength,
				prStaRec->u2StatusCode,
				WLAN_STATUS_MEDIA_DISCONNECT);
#else
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				prP2pRoleFsmInfo->ucRoleIndex,
				&prP2pRoleFsmInfo->rConnReqInfo,
				prJoinInfo->aucIEBuf,
				prJoinInfo->u4BufLength,
				prStaRec->u2StatusCode);

#endif



		} else {
			/* Join Fail */
			/* 4 <2.1> Redo JOIN process
			 * with other Auth Type if possible
			 */
			if (p2pFuncRetryJOIN(prAdapter,
				prStaRec, prJoinInfo) == FALSE) {

				struct BSS_DESC *prBssDesc;

				prBssDesc = prJoinInfo->prTargetBssDesc;

				if (!prBssDesc) {
					DBGLOG(P2P, WARN,
						"prTargetBssDesc is NULL! Skip retry join\n");
					goto error;
				}

				ASSERT(prBssDesc->fgIsConnecting);
				prBssDesc->fgIsConnecting = FALSE;

				/* Increase Failure Count */
				prStaRec->ucJoinFailureCount++;

				if (prStaRec->ucJoinFailureCount >=
						P2P_SAA_RETRY_COUNT) {
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)
					kalP2PGCIndicateConnectionStatus(
						prAdapter->prGlueInfo,
						prP2pRoleFsmInfo->ucRoleIndex,
						&prP2pRoleFsmInfo->rConnReqInfo,
						prJoinInfo->aucIEBuf,
						prJoinInfo->u4BufLength,
						prStaRec->u2StatusCode,
						WLAN_STATUS_MEDIA_DISCONNECT);
#else
					kalP2PGCIndicateConnectionStatus(
						prAdapter->prGlueInfo,
						prP2pRoleFsmInfo->ucRoleIndex,
						&prP2pRoleFsmInfo->rConnReqInfo,
						prJoinInfo->aucIEBuf,
						prJoinInfo->u4BufLength,
						prStaRec->u2StatusCode);
#endif
				}

			}

		}
	}

	if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_GC_JOIN) {
		if (prP2pBssInfo->eConnectionState ==
				MEDIA_STATE_CONNECTED) {
			/* do nothing & wait for timeout or EAPOL 4/4 TX done */
		} else {
			struct BSS_DESC *prBssDesc;
			struct P2P_SSID_STRUCT rSsid;

			prBssDesc = prJoinInfo->prTargetBssDesc;

			COPY_SSID(rSsid.aucSsid,
				rSsid.ucSsidLen,
				prBssDesc->aucSSID,
				prBssDesc->ucSSIDLen);
			p2pRoleFsmScanTargetBss(prAdapter,
				prP2pRoleFsmInfo,
				prBssDesc->ucChannelNum,
				&rSsid);
		}
	}

error:
	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventJoinComplete */

void p2pRoleFsmRunEventScanRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_P2P_SCAN_REQUEST *prP2pScanReqMsg =
		(struct MSG_P2P_SCAN_REQUEST *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;
	uint32_t u4ChnlListSize = 0;
	struct P2P_SSID_STRUCT *prP2pSsidStruct =
		(struct P2P_SSID_STRUCT *) NULL;
	struct BSS_INFO *prP2pBssInfo = NULL;


	prP2pScanReqMsg = (struct MSG_P2P_SCAN_REQUEST *) prMsgHdr;

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pScanReqMsg->ucBssIdx];

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR, "prP2pRoleFsmInfo is NULL!");
		goto error;
	}

	prP2pScanReqMsg = (struct MSG_P2P_SCAN_REQUEST *) prMsgHdr;
	prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);

	DBGLOG(P2P, TRACE, "p2pDevFsmRunEventScanRequest\n");

	/* Do we need to be in IDLE state? */
	/* p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo); */

	ASSERT(prScanReqInfo->fgIsScanRequest == FALSE);

	prScanReqInfo->fgIsAbort = TRUE;
	prScanReqInfo->eScanType = prP2pScanReqMsg->eScanType;

	if (prP2pScanReqMsg->u4NumChannel) {
		prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;

		/* Channel List */
		prScanReqInfo->ucNumChannelList = prP2pScanReqMsg->u4NumChannel;
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

		u4ChnlListSize =
			sizeof(struct RF_CHANNEL_INFO)
			* prScanReqInfo->ucNumChannelList;
		kalMemCopy(prScanReqInfo->arScanChannelList,
			   prP2pScanReqMsg->arChannelListInfo, u4ChnlListSize);
	} else {
		/* If channel number is ZERO.
		 * It means do a FULL channel scan.
		 */
		prScanReqInfo->eChannelSet = SCAN_CHANNEL_FULL;
	}

	/* SSID */
	prP2pSsidStruct = prP2pScanReqMsg->prSSID;
	for (prScanReqInfo->ucSsidNum = 0;
	     prScanReqInfo->ucSsidNum < prP2pScanReqMsg->i4SsidNum;
		 prScanReqInfo->ucSsidNum++) {

		kalMemCopy(
			prScanReqInfo->arSsidStruct[prScanReqInfo->ucSsidNum]
				.aucSsid,
			prP2pSsidStruct->aucSsid, prP2pSsidStruct->ucSsidLen);

		prScanReqInfo->arSsidStruct[prScanReqInfo->ucSsidNum]
				.ucSsidLen =
			prP2pSsidStruct->ucSsidLen;

		prP2pSsidStruct++;
	}

	/* IE Buffer */
	kalMemCopy(prScanReqInfo->aucIEBuf,
		prP2pScanReqMsg->pucIEBuf,
		prP2pScanReqMsg->u4IELen);

	prScanReqInfo->u4BufLength = prP2pScanReqMsg->u4IELen;
	prScanReqInfo->eScanReason = prP2pScanReqMsg->eScanReason;

	p2pRoleFsmStateTransition(prAdapter,
		prP2pRoleFsmInfo,
		P2P_ROLE_STATE_SCAN);

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventScanRequest */

void
p2pRoleFsmRunEventScanDone(IN struct ADAPTER *prAdapter,
	IN struct MSG_HDR *prMsgHdr,
	IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{
	struct MSG_SCN_SCAN_DONE *prScanDoneMsg =
		(struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;
	enum ENUM_P2P_ROLE_STATE eNextState = P2P_ROLE_STATE_NUM;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
		&(prP2pRoleFsmInfo->rConnReqInfo);
	struct P2P_JOIN_INFO *prP2pJoinInfo =
		&(prP2pRoleFsmInfo->rJoinInfo);
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct P2P_SCAN_REQ_INFO *prScanInfo;

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, TRACE, "prP2pRoleFsmInfo is NULL\n");
		goto error;
	}

	DBGLOG(P2P, TRACE, "P2P Role Scan Done Event\n");

	prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);
	prScanDoneMsg = (struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	prScanInfo = &prP2pRoleFsmInfo->rScanReqInfo;

	if (prScanDoneMsg->ucSeqNum != prScanReqInfo->ucSeqNumOfScnMsg) {
		/* Scan Done message sequence number mismatch.
		 * Ignore this event. (P2P FSM issue two scan events.)
		 */
		/* The scan request has been cancelled.
		 * Ignore this message. It is possible.
		 */
		DBGLOG(P2P, TRACE,
		       "P2P Role Scan Don SeqNum Received:%d <-> P2P Role Fsm SCAN Seq Issued:%d\n",
		       prScanDoneMsg->ucSeqNum,
		       prScanReqInfo->ucSeqNumOfScnMsg);

		goto error;
	}

	switch (prP2pRoleFsmInfo->eCurrentState) {
	case P2P_ROLE_STATE_SCAN:
		prScanReqInfo->fgIsAbort = FALSE;

		if (prConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_GC) {

			prP2pJoinInfo->prTargetBssDesc =
				p2pFuncKeepOnConnection(prAdapter,
					prAdapter->aprBssInfo
						[prP2pRoleFsmInfo->ucBssIndex],
					prConnReqInfo,
					&prP2pRoleFsmInfo->rChnlReqInfo,
					&prP2pRoleFsmInfo->rScanReqInfo);
			if ((prP2pJoinInfo->prTargetBssDesc) == NULL) {
				eNextState = P2P_ROLE_STATE_SCAN;
			} else {
				prP2pBssInfo =
					prAdapter->aprBssInfo
						[prP2pRoleFsmInfo->ucBssIndex];
				if (!prP2pBssInfo)
					break;
				prChnlReqInfo =
					&(prP2pRoleFsmInfo->rChnlReqInfo);
				if (!prChnlReqInfo)
					break;

				prP2pBssInfo->eBand = prChnlReqInfo->eBand;
				if (prP2pBssInfo->fgIsWmmInited == FALSE)
					prP2pBssInfo->ucWmmQueSet =
						cnmWmmIndexDecision(prAdapter,
							prP2pBssInfo);
#if CFG_SUPPORT_DBDC
				/* DBDC decsion.may change OpNss */
				cnmDbdcPreConnectionEnableDecision(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					prChnlReqInfo->eBand,
					prChnlReqInfo->ucReqChnlNum,
					prP2pBssInfo->ucWmmQueSet);
#endif /* CFG_SUPPORT_DBDC */

				cnmOpModeGetTRxNss(
					prAdapter, prP2pRoleFsmInfo->ucBssIndex,
					&prP2pBssInfo->ucOpRxNss,
					&prP2pBssInfo->ucOpTxNss);

				DBGLOG(P2P, INFO,
					"p2pRoleFsmRunEventScanDone: start GC CH[%u]RxNSS[%u]TxNss[%u]\n",
					prChnlReqInfo->ucReqChnlNum,
					prP2pBssInfo->ucOpRxNss,
					prP2pBssInfo->ucOpTxNss);

				/* For GC join. */
				eNextState = P2P_ROLE_STATE_REQING_CHANNEL;
			}
		} else if (prScanInfo->eScanReason == SCAN_REASON_ACS) {
			struct P2P_ACS_REQ_INFO *prAcsReqInfo;

			prAcsReqInfo = &prP2pRoleFsmInfo->rAcsReqInfo;
			prScanInfo->eScanReason = SCAN_REASON_UNKNOWN;
			p2pFunCalAcsChnScores(prAdapter);
			if (wlanQueryLteSafeChannel(prAdapter,
					prP2pRoleFsmInfo->ucRoleIndex) ==
					WLAN_STATUS_SUCCESS) {
				/* do nothing & wait for FW event */
			} else {
				DBGLOG(P2P, WARN, "query safe chn fail.\n");
				p2pFunProcessAcsReport(prAdapter,
						prP2pRoleFsmInfo->ucRoleIndex,
						NULL,
						prAcsReqInfo);
			}
			eNextState = P2P_ROLE_STATE_IDLE;
		} else {
			eNextState = P2P_ROLE_STATE_IDLE;
		}
		break;
	case P2P_ROLE_STATE_AP_CHNL_DETECTION:
		eNextState = P2P_ROLE_STATE_REQING_CHANNEL;
		break;
	default:
		/* Unexpected channel scan done event
		 * without being chanceled.
		 */
		ASSERT(FALSE);
		break;
	}

	prScanReqInfo->fgIsScanRequest = FALSE;

	p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, eNextState);

error:
	cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventScanDone */

void
p2pRoleFsmRunEventChnlGrant(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct MSG_CH_GRANT *prMsgChGrant = (struct MSG_CH_GRANT *) NULL;
#if (CFG_SUPPORT_DFS_MASTER == 1)
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	uint32_t u4CacTimeMs;
#endif
	uint8_t ucTokenID = 0;


	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR, "prP2pRoleFsmInfo is NULL!\n");
		goto error;
	}

	DBGLOG(P2P, TRACE, "P2P Run Event Role Channel Grant\n");

	prMsgChGrant = (struct MSG_CH_GRANT *) prMsgHdr;
	ucTokenID = prMsgChGrant->ucTokenID;
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

#if (CFG_SUPPORT_DFS_MASTER == 1)
	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prMsgChGrant->ucBssIndex);
#endif
	if (prChnlReqInfo->u4MaxInterval != prMsgChGrant->u4GrantInterval) {
		DBGLOG(P2P, WARN,
			"P2P Role:%d Request Channel Interval:%d, Grant Interval:%d\n",
			prP2pRoleFsmInfo->ucRoleIndex,
			prChnlReqInfo->u4MaxInterval,
			prMsgChGrant->u4GrantInterval);
		prChnlReqInfo->u4MaxInterval = prMsgChGrant->u4GrantInterval;
	}

	if (ucTokenID == prChnlReqInfo->ucSeqNumOfChReq) {
		enum ENUM_P2P_ROLE_STATE eNextState = P2P_ROLE_STATE_NUM;

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_REQING_CHANNEL:
			switch (prChnlReqInfo->eChnlReqType) {
			case CH_REQ_TYPE_JOIN:
				eNextState = P2P_ROLE_STATE_GC_JOIN;
				break;
			case CH_REQ_TYPE_GO_START_BSS:
				eNextState = P2P_ROLE_STATE_IDLE;
				break;
			case CH_REQ_TYPE_OFFCHNL_TX:
				eNextState = P2P_ROLE_STATE_OFF_CHNL_TX;
				break;
			default:
				DBGLOG(P2P, WARN,
				       "p2pRoleFsmRunEventChnlGrant: Invalid Channel Request Type:%d\n",
				       prChnlReqInfo->eChnlReqType);
				GL_RESET_TRIGGER(prAdapter,
					RST_FLAG_CHIP_RESET);
				break;
			}

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo, eNextState);
			break;

#if (CFG_SUPPORT_DFS_MASTER == 1)
		case P2P_ROLE_STATE_DFS_CAC:
			p2pFuncStartRdd(prAdapter, prMsgChGrant->ucBssIndex);

			if (p2pFuncCheckWeatherRadarBand(prChnlReqInfo))
				u4CacTimeMs =
					P2P_AP_CAC_WEATHER_CHNL_HOLD_TIME_MS;
			else
				u4CacTimeMs =
					prP2pRoleFsmInfo->rChnlReqInfo
						.u4MaxInterval;

			if (p2pFuncIsManualCac())
				u4CacTimeMs = p2pFuncGetDriverCacTime() * 1000;
			else
				p2pFuncSetDriverCacTime(u4CacTimeMs/1000);

			cnmTimerStartTimer(prAdapter,
				&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer),
				u4CacTimeMs);

			p2pFuncRecordCacStartBootTime();

			p2pFuncSetDfsState(DFS_STATE_CHECKING);

			DBGLOG(P2P, INFO,
				"p2pRoleFsmRunEventChnlGrant: CAC time = %ds\n",
				u4CacTimeMs/1000);
			break;
		case P2P_ROLE_STATE_SWITCH_CHANNEL:
			p2pFuncDfsSwitchCh(prAdapter,
				prP2pBssInfo,
				prP2pRoleFsmInfo->rChnlReqInfo);
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			break;
#endif
		case P2P_ROLE_STATE_OFF_CHNL_TX:
			if (prMsgChGrant->eReqType == CH_REQ_TYPE_OFFCHNL_TX) {
				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_OFF_CHNL_TX);
			} else {
				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_IDLE);
			}
			break;
		default:
			/* Channel is granted under unexpected state.
			 * Driver should cancel channel privileagea
			 * before leaving the states.
			 */
			if (IS_BSS_ACTIVE(
				prAdapter->aprBssInfo
					[prP2pRoleFsmInfo->ucBssIndex])) {
				DBGLOG(P2P, WARN,
				       "p2pRoleFsmRunEventChnlGrant: Invalid CurrentState:%d\n",
				       prP2pRoleFsmInfo->eCurrentState);
				GL_RESET_TRIGGER(prAdapter,
					RST_FLAG_CHIP_RESET);
			}
			break;
		}
	} else {
		DBGLOG(P2P, ERROR,
			"p2pRoleFsmRunEventChnlGrant: Token mismatch, Chreq: %d, ChGrant: %d\n",
			prChnlReqInfo->ucSeqNumOfChReq, ucTokenID);
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventChnlGrant */

/* ////////////////////////////////////// */
void p2pRoleFsmRunEventDissolve(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	/* TODO: */

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventDissolve */

/*----------------------------------------------------------------------------*/
/*!
 * @	This routine update the current MAC table based on the current ACL.
 *	If ACL change causing an associated STA become un-authorized. This STA
 *	will be kicked out immediately.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] ucBssIdx            Bss index.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void p2pRoleUpdateACLEntry(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIdx)
{
	u_int8_t bMatchACL = FALSE;
	int32_t i = 0;
	struct LINK *prClientList;
	struct STA_RECORD *prCurrStaRec, *prNextStaRec;
	struct BSS_INFO *prP2pBssInfo;

	ASSERT(prAdapter);

	if ((!prAdapter) || (ucBssIdx > prAdapter->ucHwBssIdNum))
		return;

	DBGLOG(P2P, TRACE, "Update ACL Entry ucBssIdx = %d\n", ucBssIdx);
	prP2pBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	/* ACL is disabled. Do nothing about the MAC table. */
	if (prP2pBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_DISABLE)
		return;

	prClientList = &prP2pBssInfo->rStaRecOfClientList;

	LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec,
		prNextStaRec, prClientList, rLinkEntry, struct STA_RECORD) {
		if (!prCurrStaRec)
			break;
		bMatchACL = FALSE;
		for (i = 0; i < prP2pBssInfo->rACL.u4Num; i++) {
			if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr,
				prP2pBssInfo->rACL.rEntry[i].aucAddr)) {
				bMatchACL = TRUE;
				break;
			}
		}

		if (((!bMatchACL) &&
			(prP2pBssInfo->rACL.ePolicy
				== PARAM_CUSTOM_ACL_POLICY_ACCEPT))
			|| ((bMatchACL) &&
			(prP2pBssInfo->rACL.ePolicy
				== PARAM_CUSTOM_ACL_POLICY_DENY))) {
			struct MSG_P2P_CONNECTION_ABORT *prDisconnectMsg =
				(struct MSG_P2P_CONNECTION_ABORT *) NULL;

			DBGLOG(P2P, TRACE,
				"ucBssIdx=%d, ACL Policy=%d\n",
				ucBssIdx, prP2pBssInfo->rACL.ePolicy);

			prDisconnectMsg =
				(struct MSG_P2P_CONNECTION_ABORT *)
				cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				sizeof(struct MSG_P2P_CONNECTION_ABORT));
			if (prDisconnectMsg == NULL)
				return;
			prDisconnectMsg->rMsgHdr.eMsgId
				= MID_MNY_P2P_CONNECTION_ABORT;
			prDisconnectMsg->ucRoleIdx
				=  (uint8_t) prP2pBssInfo->u4PrivateData;
			COPY_MAC_ADDR(prDisconnectMsg->aucTargetID,
				prCurrStaRec->aucMacAddr);
			prDisconnectMsg->u2ReasonCode
				= STATUS_CODE_REQ_DECLINED;
			prDisconnectMsg->fgSendDeauth = TRUE;
			mboxSendMsg(prAdapter,
				MBOX_ID_0,
				(struct MSG_HDR *) prDisconnectMsg,
				MSG_SEND_METHOD_BUF);
		}
	}
} /* p2pRoleUpdateACLEntry */

/*----------------------------------------------------------------------------*/
/*!
 * @ Check if the specified STA pass the Access Control List inspection.
 *	If fails to pass the checking,
 *  then no authentication or association is allowed.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] pMacAddr           Pointer to the mac address.
 * @param[in] ucBssIdx            Bss index.
 *
 * @return TRUE - pass ACL inspection, FALSE - ACL inspection fail
 */
/*----------------------------------------------------------------------------*/
u_int8_t p2pRoleProcessACLInspection(IN struct ADAPTER *prAdapter,
		IN uint8_t *pMacAddr,
		IN uint8_t ucBssIdx)
{
	u_int8_t bPassACL = TRUE;
	int32_t i = 0;
	struct BSS_INFO *prP2pBssInfo;

	ASSERT(prAdapter);

	if ((!prAdapter) || (!pMacAddr) || (ucBssIdx > prAdapter->ucHwBssIdNum))
		return FALSE;

	prP2pBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	if (prP2pBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_DISABLE)
		return TRUE;

	if (prP2pBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_ACCEPT)
		bPassACL = FALSE;
	else
		bPassACL = TRUE;

	for (i = 0; i < prP2pBssInfo->rACL.u4Num; i++) {
		if (EQUAL_MAC_ADDR(pMacAddr,
			prP2pBssInfo->rACL.rEntry[i].aucAddr)) {
			bPassACL = !bPassACL;
			break;
		}
	}

	if (bPassACL == FALSE)
		DBGLOG(P2P, WARN,
		"this mac [" MACSTR "] is fail to pass ACL inspection.\n",
		MAC2STR(pMacAddr));

	return bPassACL;
} /* p2pRoleProcessACLInspection */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate the Event
 *           of Successful Completion of AAA Module.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t
p2pRoleFsmRunEventAAAComplete(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct BSS_INFO *prP2pBssInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	enum ENUM_PARAM_MEDIA_STATE eOriMediaState;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prStaRec != NULL) && (prP2pBssInfo != NULL));

		eOriMediaState = prP2pBssInfo->eConnectionState;

		bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

		if (prP2pBssInfo->rStaRecOfClientList.u4NumElem
			>= P2P_MAXIMUM_CLIENT_COUNT
			|| !p2pRoleProcessACLInspection(prAdapter,
					prStaRec->aucMacAddr,
					prP2pBssInfo->ucBssIndex)
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
			|| kalP2PMaxClients(prAdapter->prGlueInfo,
				prP2pBssInfo->rStaRecOfClientList.u4NumElem,
				(uint8_t) prP2pBssInfo->u4PrivateData)
#endif
		) {
			rStatus = WLAN_STATUS_RESOURCES;
			break;
		}

		bssAddClient(prAdapter, prP2pBssInfo, prStaRec);

		prStaRec->u2AssocId = bssAssignAssocID(prStaRec);

		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

		p2pChangeMediaState(prAdapter,
			prP2pBssInfo,
			MEDIA_STATE_CONNECTED);

		/* Update Connected state to FW. */
		if (eOriMediaState != prP2pBssInfo->eConnectionState)
			nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

	} while (FALSE);

	return rStatus;
}				/* p2pRunEventAAAComplete */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate the Event
 *           of Successful Completion of AAA Module.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t
p2pRoleFsmRunEventAAASuccess(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct BSS_INFO *prP2pBssInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prStaRec != NULL) && (prP2pBssInfo != NULL));

		if ((prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
			|| (prP2pBssInfo->u4PrivateData >= BSS_P2P_NUM)) {
			ASSERT(FALSE);
			rStatus = WLAN_STATUS_INVALID_DATA;
			break;
		}

		ASSERT(prP2pBssInfo->ucBssIndex < prAdapter->ucP2PDevBssIdx);

		prP2pRoleFsmInfo =
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				prP2pBssInfo->u4PrivateData);

		/* Glue layer indication. */
		kalP2PGOStationUpdate(prAdapter->prGlueInfo,
			prP2pRoleFsmInfo->ucRoleIndex, prStaRec, TRUE);

	} while (FALSE);

	return rStatus;
}				/* p2pRoleFsmRunEventAAASuccess */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate the Event
 *           of Tx Fail of AAA Module.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void p2pRoleFsmRunEventAAATxFail(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct BSS_INFO *prP2pBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prStaRec);

	bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

	p2pFuncDisconnect(prAdapter,
		prP2pBssInfo, prStaRec, FALSE,
		prStaRec->eAuthAssocState == AAA_STATE_SEND_AUTH2
		? STATUS_CODE_AUTH_TIMEOUT
		: STATUS_CODE_ASSOC_TIMEOUT);

	/* 20120830 moved into p2puUncDisconnect. */
	/* cnmStaRecFree(prAdapter, prStaRec); */
}				/* p2pRoleFsmRunEventAAATxFail */

void p2pRoleFsmRunEventSwitchOPMode(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_SWITCH_OP_MODE *prSwitchOpMode =
		(struct MSG_P2P_SWITCH_OP_MODE *) prMsgHdr;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;

	ASSERT(prSwitchOpMode->ucRoleIdx < BSS_P2P_NUM);
	if (!(prSwitchOpMode->ucRoleIdx < BSS_P2P_NUM)) {
		DBGLOG(P2P, ERROR,
			"prSwitchOpMode->ucRoleIdx %d should < BSS_P2P_NUM(%d)\n",
			prSwitchOpMode->ucRoleIdx, BSS_P2P_NUM);
		goto error;
	}

	prP2pRoleFsmInfo =
		prAdapter->rWifiVar
			.aprP2pRoleFsmInfo[prSwitchOpMode->ucRoleIdx];
	prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

	ASSERT(prP2pRoleFsmInfo->ucBssIndex < prAdapter->ucP2PDevBssIdx);
	if (!(prP2pRoleFsmInfo->ucBssIndex < prAdapter->ucP2PDevBssIdx)) {
		DBGLOG(P2P, ERROR,
			"prP2pRoleFsmInfo->ucBssIndex %d should < prAdapter->ucP2PDevBssIdx(%d)\n",
			prP2pRoleFsmInfo->ucBssIndex,
			prAdapter->ucP2PDevBssIdx);
		goto error;
	}

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prP2pRoleFsmInfo->ucBssIndex);

	if (!(prSwitchOpMode->eOpMode < OP_MODE_NUM)) {
		DBGLOG(P2P, ERROR,
			"prSwitchOpMode->eOpMode %d should < OP_MODE_NUM(%d)\n",
			prSwitchOpMode->eOpMode, OP_MODE_NUM);
		goto error;
	}

	/* P2P Device / GC. */
	p2pFuncSwitchOPMode(prAdapter,
		prP2pBssInfo,
		prSwitchOpMode->eOpMode,
		TRUE);

	if (prP2pBssInfo->eIftype == IFTYPE_P2P_CLIENT &&
			prSwitchOpMode->eIftype == IFTYPE_STATION) {
		kalP2pUnlinkBss(prAdapter->prGlueInfo, prConnReqInfo->aucBssid);
	}
	prP2pBssInfo->eIftype = prSwitchOpMode->eIftype;

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventSwitchOPMode */

/* /////////////////////////////// TO BE REFINE //////////////////////////// */

void p2pRoleFsmRunEventBeaconUpdate(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prRoleP2pFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_BEACON_UPDATE *prBcnUpdateMsg =
		(struct MSG_P2P_BEACON_UPDATE *) NULL;
	struct P2P_BEACON_UPDATE_INFO *prBcnUpdateInfo =
		(struct P2P_BEACON_UPDATE_INFO *) NULL;


	DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventBeaconUpdate\n");

	prBcnUpdateMsg = (struct MSG_P2P_BEACON_UPDATE *) prMsgHdr;
	if (prBcnUpdateMsg->ucRoleIndex >= BSS_P2P_NUM)
		goto error;

	prRoleP2pFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prBcnUpdateMsg->ucRoleIndex);
	if (!prRoleP2pFsmInfo)
		goto error;

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prRoleP2pFsmInfo->ucBssIndex);

	prP2pBssInfo->fgIsWepCipherGroup = prBcnUpdateMsg->fgIsWepCipher;

	prBcnUpdateInfo = &(prRoleP2pFsmInfo->rBeaconUpdateInfo);

	DBGLOG(P2P, TRACE, "Dump beacon content from supplicant.\n");
	if (aucDebugModule[DBG_P2P_IDX] & DBG_CLASS_TRACE) {
		dumpMemory8((uint8_t *) prBcnUpdateMsg->pucBcnBody,
			(uint32_t) prBcnUpdateMsg->u4BcnBodyLen);
	}

	p2pFuncBeaconUpdate(prAdapter,
		prP2pBssInfo,
		prBcnUpdateInfo,
		prBcnUpdateMsg->pucBcnHdr,
		prBcnUpdateMsg->u4BcnHdrLen,
		prBcnUpdateMsg->pucBcnBody,
		prBcnUpdateMsg->u4BcnBodyLen);

	if (prBcnUpdateMsg->pucAssocRespIE != NULL
		&& prBcnUpdateMsg->u4AssocRespLen > 0) {
		DBGLOG(P2P, TRACE,
			"Copy extra IEs for assoc resp (Length= %d)\n",
			prBcnUpdateMsg->u4AssocRespLen);
		DBGLOG_MEM8(P2P, TRACE,
			prBcnUpdateMsg->pucAssocRespIE,
			prBcnUpdateMsg->u4AssocRespLen);

		if (p2pFuncAssocRespUpdate(prAdapter,
			prP2pBssInfo,
			prBcnUpdateMsg->pucAssocRespIE,
			prBcnUpdateMsg->u4AssocRespLen) == WLAN_STATUS_FAILURE)
			DBGLOG(P2P, ERROR,
				"Update extra IEs for asso resp fail!\n");
	}


	if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
	    (prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {
		/* AP is created, Beacon Update. */
		/* nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_P2P_INDEX); */


		DBGLOG(P2P, TRACE,
			"p2pRoleFsmRunEventBeaconUpdate with Bssidex(%d)\n",
			prRoleP2pFsmInfo->ucBssIndex);

		bssUpdateBeaconContent(prAdapter, prRoleP2pFsmInfo->ucBssIndex);

#if CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP
		if (p2pFuncProbeRespUpdate(prAdapter,
			prP2pBssInfo,
			prBcnUpdateMsg->pucProbeRespIE,
			prBcnUpdateMsg->u4ProbeRespLen) == WLAN_STATUS_FAILURE)
			DBGLOG(P2P, ERROR,
				"Update extra IEs for probe resp fail!\n");
#endif
		/* nicPmIndicateBssCreated(prAdapter,
		 * NETWORK_TYPE_P2P_INDEX);
		 */
	}
error:
	cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventBeaconUpdate */

void
p2pProcessEvent_UpdateNOAParam(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIdx,
		IN struct EVENT_UPDATE_NOA_PARAMS *prEventUpdateNoaParam)
{
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;
	uint32_t i;
	u_int8_t fgNoaAttrExisted = FALSE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar
			.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	prP2pSpecificBssInfo->fgEnableOppPS =
		prEventUpdateNoaParam->ucEnableOppPS;
	prP2pSpecificBssInfo->u2CTWindow = prEventUpdateNoaParam->u2CTWindow;
	prP2pSpecificBssInfo->ucNoAIndex = prEventUpdateNoaParam->ucNoAIndex;
	prP2pSpecificBssInfo->ucNoATimingCount =
		prEventUpdateNoaParam->ucNoATimingCount;

	fgNoaAttrExisted |= prP2pSpecificBssInfo->fgEnableOppPS;

	ASSERT(prP2pSpecificBssInfo->ucNoATimingCount <= P2P_MAXIMUM_NOA_COUNT);

	for (i = 0; i < prP2pSpecificBssInfo->ucNoATimingCount; i++) {
		/* in used */
		prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse =
			prEventUpdateNoaParam->arEventNoaTiming[i].ucIsInUse;
		/* count */
		prP2pSpecificBssInfo->arNoATiming[i].ucCount =
			prEventUpdateNoaParam->arEventNoaTiming[i].ucCount;
		/* duration */
		prP2pSpecificBssInfo->arNoATiming[i].u4Duration =
			prEventUpdateNoaParam->arEventNoaTiming[i].u4Duration;
		/* interval */
		prP2pSpecificBssInfo->arNoATiming[i].u4Interval =
			prEventUpdateNoaParam->arEventNoaTiming[i].u4Interval;
		/* start time */
		prP2pSpecificBssInfo->arNoATiming[i].u4StartTime =
		    prEventUpdateNoaParam->arEventNoaTiming[i].u4StartTime;

		fgNoaAttrExisted |=
			prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse;
	}

	prP2pSpecificBssInfo->fgIsNoaAttrExisted = fgNoaAttrExisted;

	DBGLOG(P2P, INFO, "Update NoA param, count=%d, ucBssIdx=%d\n",
		prEventUpdateNoaParam->ucNoATimingCount,
		ucBssIdx);

	/* update beacon content by the change */
	bssUpdateBeaconContent(prAdapter, ucBssIdx);
}				/* p2pProcessEvent_UpdateNOAParam */

#if CFG_ENABLE_PER_STA_STATISTICS_LOG
void
p2pRoleFsmGetStaStatistics(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr)
{
	uint32_t u4BufLen;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	if ((!prAdapter) || (!prP2pRoleFsmInfo)) {
		DBGLOG(P2P, ERROR, "prAdapter=NULL || prP2pRoleFsmInfo=NULL\n");
		return;
	}

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prP2pRoleFsmInfo->ucBssIndex);
	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR, "prP2pBssInfo=NULL\n");
		return;
	}

	prQueryStaStatistics =
		prAdapter->rWifiVar.prP2pQueryStaStatistics
		[prP2pRoleFsmInfo->ucRoleIndex];
	if (!prQueryStaStatistics) {
		DBGLOG(P2P, ERROR, "prQueryStaStatistics=NULL\n");
		return;
	}

	if (prP2pBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
		if ((prP2pBssInfo->eCurrentOPMode
			!= OP_MODE_INFRASTRUCTURE) &&
			(prP2pBssInfo->eCurrentOPMode
			!= OP_MODE_ACCESS_POINT)) {
			DBGLOG(P2P, ERROR, "Invalid OPMode=%d\n",
				prP2pBssInfo->eCurrentOPMode);
			return;
		}
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			&& prP2pBssInfo->prStaRecOfAP) {
			COPY_MAC_ADDR(
				prQueryStaStatistics->aucMacAddr,
				prP2pBssInfo->prStaRecOfAP->aucMacAddr);
		} else if (prP2pBssInfo->eCurrentOPMode
			== OP_MODE_ACCESS_POINT) {
			struct STA_RECORD *prCurrStaRec;
			struct LINK *prClientList =
				&prP2pBssInfo->rStaRecOfClientList;
			if (!prClientList) {
				DBGLOG(P2P, ERROR, "prClientList=NULL\n");
				return;
			}
			LINK_FOR_EACH_ENTRY(prCurrStaRec,
				prClientList, rLinkEntry, struct STA_RECORD) {
				if (!prCurrStaRec)
					break;
				COPY_MAC_ADDR(
					prQueryStaStatistics->aucMacAddr,
					prCurrStaRec->aucMacAddr);
					/* break for LINK_FOR_EACH_ENTRY */
					break;
			}
		}

		prQueryStaStatistics->ucReadClear = TRUE;
		wlanQueryStaStatistics(prAdapter,
			prQueryStaStatistics,
			sizeof(struct PARAM_GET_STA_STATISTICS),
			&u4BufLen,
			FALSE);

	}

	cnmTimerStartTimer(prAdapter,
		&(prP2pRoleFsmInfo->rP2pRoleFsmGetStatisticsTimer),
		P2P_ROLE_GET_STATISTICS_TIME);
}
#endif

void p2pRoleFsmNotifyEapolTxStatus(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex,
		IN enum ENUM_EAPOL_KEY_TYPE_T rEapolKeyType,
		IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;

	if (prAdapter == NULL)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (prBssInfo == NULL || prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prBssInfo->u4PrivateData);

	if (prP2pRoleFsmInfo == NULL)
		return;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_GC_JOIN)
		return;

	if (rEapolKeyType == EAPOL_KEY_4_OF_4 &&
			rTxDoneStatus == TX_RESULT_SUCCESS) {
		/* Finish GC connection process. */
		p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
	}
}

static u_int8_t
p2pRoleNeedOffchnlTx(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prBssInfo,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	u_int8_t fgNeedOffchnlTx = FALSE;
	struct WLAN_MAC_HEADER *prWlanHdr = (struct WLAN_MAC_HEADER *) NULL;

	if (prAdapter == NULL || prBssInfo == NULL || prMgmtTxMsg == NULL)
		return FALSE;

	fgNeedOffchnlTx = prMgmtTxMsg->fgIsOffChannel;

	if (fgNeedOffchnlTx) {
		prWlanHdr = (struct WLAN_MAC_HEADER *)
			((unsigned long) prMgmtTxMsg->prMgmtMsduInfo->prPacket +
					MAC_TX_RESERVED_FIELD);
		if (prMgmtTxMsg->rChannelInfo.ucChannelNum ==
				prBssInfo->ucPrimaryChannel)
			fgNeedOffchnlTx = FALSE;
		else if ((prWlanHdr->u2FrameCtrl & MASK_FRAME_TYPE) ==
				MAC_FRAME_PROBE_RSP)
			fgNeedOffchnlTx = FALSE;
	}

	return fgNeedOffchnlTx;
}				/* p2pRoleNeedOffchnlTx */


static void
p2pRoleAdjustChnlTime(IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
	IN struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq)
{
	if (prMgmtTxMsg == NULL || prOffChnlTxReq == NULL)
		return;

	if (!prMgmtTxMsg->fgIsOffChannel)
		return;

	if (prMgmtTxMsg->u4Duration < MIN_TX_DURATION_TIME_MS) {
		/*
		 * The wait time requested from Supplicant is too short
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
}				/* p2pRoleAdjustChnlTime */

static void
p2pRoleChnlReqByOffChnl(IN struct ADAPTER *prAdapter,
	IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq)
{
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
			(struct P2P_CHNL_REQ_INFO *) NULL;

	if (prAdapter == NULL || prP2pRoleFsmInfo == NULL ||
			prOffChnlTxReq == NULL)
		return;

	prP2pChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

	prP2pChnlReqInfo->u8Cookie = prOffChnlTxReq->u8Cookie;
	prP2pChnlReqInfo->eChnlReqType = CH_REQ_TYPE_OFFCHNL_TX;
	prP2pChnlReqInfo->eBand = prOffChnlTxReq->rChannelInfo.eBand;
	prP2pChnlReqInfo->ucReqChnlNum =
			prOffChnlTxReq->rChannelInfo.ucChannelNum;
	prP2pChnlReqInfo->eChnlSco = prOffChnlTxReq->eChnlExt;
	prP2pChnlReqInfo->u4MaxInterval = prOffChnlTxReq->u4Duration;
	prP2pChnlReqInfo->eChannelWidth = prOffChnlTxReq->rChannelInfo.ucChnlBw;
	prP2pChnlReqInfo->ucCenterFreqS1 =
			prOffChnlTxReq->rChannelInfo.u4CenterFreq1;
	prP2pChnlReqInfo->ucCenterFreqS2 =
			prOffChnlTxReq->rChannelInfo.u4CenterFreq2;

	p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo,
			P2P_ROLE_STATE_REQING_CHANNEL);
}				/* p2pRoleChnlReqByOffChnl */

static u_int8_t
p2pRoleAddTxReq2Queue(IN struct ADAPTER *prAdapter,
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

	p2pRoleAdjustChnlTime(prMgmtTxMsg, prTmpOffChnlTxReq);

	LINK_INSERT_TAIL(&prP2pMgmtTxReqInfo->rTxReqLink,
			&prTmpOffChnlTxReq->rLinkEntry);

	*pprOffChnlTxReq = prTmpOffChnlTxReq;

	return TRUE;
}

static void
p2pRoleAbortChlReqIfNeed(IN struct ADAPTER *prAdapter,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		struct P2P_CHNL_REQ_INFO *prChnlReqInfo)
{
	if (prAdapter == NULL || prP2pRoleFsmInfo == NULL ||
			prChnlReqInfo == NULL)
		return;

	/* Cancel ongoing channel request whose type is not offchannel-tx*/
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_REQING_CHANNEL ||
			prChnlReqInfo->eChnlReqType == CH_REQ_TYPE_OFFCHNL_TX)
		return;

	p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
}

static void
p2pRoleHandleOffchnlTxReq(IN struct ADAPTER *prAdapter,
		IN struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo =
			(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
			(struct P2P_CHNL_REQ_INFO *) NULL;

	if (prAdapter == NULL || prMgmtTxMsg == NULL)
		return;

	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prMgmtTxMsg->ucBssIdx);
	prP2pRoleFsmInfo = prP2pRoleBssInfo != NULL ?
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pRoleBssInfo->u4PrivateData) : NULL;

	if (prP2pRoleFsmInfo == NULL)
		return;

	prP2pMgmtTxReqInfo = &(prP2pRoleFsmInfo->rMgmtTxInfo);
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

	if (prP2pMgmtTxReqInfo == NULL || prChnlReqInfo == NULL)

	p2pRoleAbortChlReqIfNeed(prAdapter, prP2pRoleFsmInfo, prChnlReqInfo);

	if (p2pRoleAddTxReq2Queue(prAdapter, prP2pMgmtTxReqInfo,
			prMgmtTxMsg, &prOffChnlTxReq) == FALSE)
		goto error;

	switch (prP2pRoleFsmInfo->eCurrentState) {
	case P2P_ROLE_STATE_IDLE:
		p2pRoleChnlReqByOffChnl(prAdapter, prP2pRoleFsmInfo,
				prOffChnlTxReq);
		break;
	case P2P_ROLE_STATE_REQING_CHANNEL:
		if (prChnlReqInfo->eChnlReqType != CH_REQ_TYPE_OFFCHNL_TX) {
			DBGLOG(P2P, WARN,
				"channel already requested by others\n");
			goto error;
		}
		break;
	case P2P_ROLE_STATE_OFF_CHNL_TX:
		if (p2pFuncCheckOnRocChnl(&(prMgmtTxMsg->rChannelInfo),
				prChnlReqInfo) &&
				prP2pMgmtTxReqInfo->rTxReqLink.u4NumElem == 1) {
			p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_OFF_CHNL_TX);
		} else {
			log_dbg(P2P, INFO, "tx ch: %d, current ch: %d, requested: %d, tx link num: %d",
				prMgmtTxMsg->rChannelInfo.ucChannelNum,
				prChnlReqInfo->ucReqChnlNum,
				prChnlReqInfo->fgIsChannelRequested,
				prP2pMgmtTxReqInfo->rTxReqLink.u4NumElem);
		}
		break;
	default:
		DBGLOG(P2P, WARN, "Unknown state (%s) for offchannel-tx.\n",
				p2pRoleFsmGetFsmState(
				prP2pRoleFsmInfo->eCurrentState));
		goto error;
	}

	return;

error:
	LINK_REMOVE_KNOWN_ENTRY(
			&(prP2pMgmtTxReqInfo->rTxReqLink),
			&prOffChnlTxReq->rLinkEntry);
	cnmMgtPktFree(prAdapter, prOffChnlTxReq->prMgmtTxMsdu);
	cnmMemFree(prAdapter, prOffChnlTxReq);
}				/* p2pRoleHandleOffchnlTxReq */

void p2pRoleFsmRunEventMgmtTx(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
		(struct MSG_MGMT_TX_REQUEST *) NULL;
	u_int8_t fgNeedOffchnlTx;

	if (prAdapter == NULL || prMsgHdr == NULL)
		goto error;

	prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *) prMsgHdr;
	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prMgmtTxMsg->ucBssIdx);

	fgNeedOffchnlTx = p2pRoleNeedOffchnlTx(prAdapter, prP2pRoleBssInfo,
			prMgmtTxMsg);
	DBGLOG(P2P, INFO, "fgNeedOffchnlTx: %d\n", fgNeedOffchnlTx);

	if (!fgNeedOffchnlTx)
		p2pFuncTxMgmtFrame(prAdapter,
				prMgmtTxMsg->ucBssIdx,
				prMgmtTxMsg->prMgmtMsduInfo,
				prMgmtTxMsg->fgNoneCckRate);
	else
		p2pRoleHandleOffchnlTxReq(prAdapter, prMgmtTxMsg);

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventMgmtTx */

void p2pRoleFsmRunEventTxCancelWait(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo =
			(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct MSG_CANCEL_TX_WAIT_REQUEST *prCancelTxWaitMsg =
			(struct MSG_CANCEL_TX_WAIT_REQUEST *) NULL;
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxPkt =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	u_int8_t fgIsCookieFound = FALSE;

	if (prAdapter == NULL || prMsgHdr == NULL)
		goto exit;

	prCancelTxWaitMsg = (struct MSG_CANCEL_TX_WAIT_REQUEST *) prMsgHdr;
	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prCancelTxWaitMsg->ucBssIdx);
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pRoleBssInfo->u4PrivateData);
	prP2pMgmtTxInfo = prP2pRoleFsmInfo != NULL ?
			&(prP2pRoleFsmInfo->rMgmtTxInfo) : NULL;

	if (prCancelTxWaitMsg == NULL || prP2pRoleFsmInfo == NULL ||
			prP2pMgmtTxInfo == NULL || prP2pMgmtTxInfo == NULL)
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

	if (fgIsCookieFound || prP2pRoleFsmInfo->eCurrentState ==
			P2P_ROLE_STATE_OFF_CHNL_TX) {
		p2pFunClearAllTxReq(prAdapter,
				&(prP2pRoleFsmInfo->rMgmtTxInfo));
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	}

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventTxCancelWait */

static void initAcsParams(IN struct ADAPTER *prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo) {
	struct RF_CHANNEL_INFO *prRfChannelInfo;
	uint8_t i;

	if (!prAdapter || !prMsgAcsRequest || !prAcsReqInfo)
		return;

	kalMemSet(prAcsReqInfo, 0, sizeof(struct P2P_ACS_REQ_INFO));
	prAcsReqInfo->fgIsProcessing = TRUE;
	prAcsReqInfo->ucRoleIdx = prMsgAcsRequest->ucRoleIdx;
	prAcsReqInfo->fgIsHtEnable = prMsgAcsRequest->fgIsHtEnable;
	prAcsReqInfo->fgIsHt40Enable = prMsgAcsRequest->fgIsHt40Enable;
	prAcsReqInfo->fgIsVhtEnable = prMsgAcsRequest->fgIsVhtEnable;
	prAcsReqInfo->eChnlBw = prMsgAcsRequest->eChnlBw;
	prAcsReqInfo->eHwMode = prMsgAcsRequest->eHwMode;

	if (prAcsReqInfo->eChnlBw == MAX_BW_UNKNOWN) {
		if (prAcsReqInfo->fgIsHtEnable &&
				prAcsReqInfo->fgIsHt40Enable) {
			prAcsReqInfo->eChnlBw = MAX_BW_40MHZ;
		} else {
			prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		}
	}
	if (!prAcsReqInfo->fgIsVhtEnable &&
			(prAcsReqInfo->eChnlBw == MAX_BW_80MHZ ||
				prAcsReqInfo->eChnlBw == MAX_BW_160MHZ)) {
		if (prAcsReqInfo->fgIsHtEnable &&
				prAcsReqInfo->fgIsHt40Enable) {
			prAcsReqInfo->eChnlBw = MAX_BW_40MHZ;
		} else {
			prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		}
	}

	DBGLOG(P2P, INFO, "idx=%d, ht=%d, ht40=%d, vht=%d, bw=%d, m=%d, c=%d",
			prMsgAcsRequest->ucRoleIdx,
			prMsgAcsRequest->fgIsHtEnable,
			prMsgAcsRequest->fgIsHt40Enable,
			prMsgAcsRequest->fgIsVhtEnable,
			prMsgAcsRequest->eChnlBw,
			prMsgAcsRequest->eHwMode,
			prMsgAcsRequest->u4NumChannel);
	if (prMsgAcsRequest->u4NumChannel) {
		for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
			prRfChannelInfo =
				&(prMsgAcsRequest->arChannelListInfo[i]);
			DBGLOG(REQ, INFO, "[%d] band=%d, ch=%d\n", i,
				prRfChannelInfo->eBand,
				prRfChannelInfo->ucChannelNum);
			prRfChannelInfo++;
		}
	}
}

static void indicateAcsResultByAisCh(IN struct ADAPTER *prAdapter,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo,
		IN struct BSS_INFO *prAisBssInfo)
{
	if (!prAdapter || !prAcsReqInfo)
		return;

	prAcsReqInfo->ucPrimaryCh = prAisBssInfo->ucPrimaryChannel;
	if (prAisBssInfo->ucVhtChannelWidth == VHT_OP_CHANNEL_WIDTH_20_40) {
		if (prAisBssInfo->eBssSCO == CHNL_EXT_SCN)
			prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		else
			prAcsReqInfo->eChnlBw = MAX_BW_40MHZ;
	} else if (prAisBssInfo->ucVhtChannelWidth == VHT_OP_CHANNEL_WIDTH_80) {
		prAcsReqInfo->eChnlBw = MAX_BW_80MHZ;
	} else if (prAisBssInfo->ucVhtChannelWidth ==
			VHT_OP_CHANNEL_WIDTH_160) {
		prAcsReqInfo->eChnlBw = MAX_BW_160MHZ;
	} else if (prAisBssInfo->ucVhtChannelWidth ==
			VHT_OP_CHANNEL_WIDTH_80P80) {
		prAcsReqInfo->eChnlBw = MAX_BW_80_80_MHZ;
	} else {
		prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
	}
	p2pFunIndicateAcsResult(prAdapter->prGlueInfo,
			prAcsReqInfo);
}

static void trimAcsScanList(IN struct ADAPTER *prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo,
		IN enum ENUM_BAND eDesiredBand)
{
	uint32_t u4NumChannel = 0;
	uint8_t i;
	struct RF_CHANNEL_INFO *prRfChannelInfo1;
	struct RF_CHANNEL_INFO *prRfChannelInfo2;

	if (!prAdapter || !prAcsReqInfo)
		return;

	for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
		prRfChannelInfo1 =
				&(prMsgAcsRequest->arChannelListInfo[i]);
		if (eDesiredBand != prRfChannelInfo1->eBand)
			continue;
		DBGLOG(P2P, INFO, "acs trim scan list, [%d]=%d %d\n",
				u4NumChannel,
				prRfChannelInfo1->eBand,
				prRfChannelInfo1->ucChannelNum);
		prRfChannelInfo2 = &(prMsgAcsRequest->arChannelListInfo[
				u4NumChannel]);
		prRfChannelInfo2->eBand = prRfChannelInfo1->eBand;
		prRfChannelInfo2->u4CenterFreq1 =
				prRfChannelInfo1->u4CenterFreq1;
		prRfChannelInfo2->u4CenterFreq2 =
				prRfChannelInfo1->u4CenterFreq2;
		prRfChannelInfo2->u2PriChnlFreq =
				prRfChannelInfo1->u2PriChnlFreq;
		prRfChannelInfo2->ucChnlBw = prRfChannelInfo1->ucChnlBw;
		prRfChannelInfo2->ucChannelNum =
				prRfChannelInfo1->ucChannelNum;
		u4NumChannel++;
		prRfChannelInfo1++;
	}
	prMsgAcsRequest->u4NumChannel = u4NumChannel;
}

static void initAcsChnlMask(IN struct ADAPTER *prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo)
{
	uint8_t i;
	struct RF_CHANNEL_INFO *prRfChannelInfo;

	prAcsReqInfo->u4LteSafeChnMask_2G = 0;
	prAcsReqInfo->u4LteSafeChnMask_5G_1 = 0;
	prAcsReqInfo->u4LteSafeChnMask_5G_2 = 0;

	for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
		prRfChannelInfo = &(prMsgAcsRequest->arChannelListInfo[i]);
		if (prRfChannelInfo->ucChannelNum <= 14) {
			prAcsReqInfo->u4LteSafeChnMask_2G |= BIT(
				prRfChannelInfo->ucChannelNum);
		} else if (prRfChannelInfo->ucChannelNum >= 36 &&
				prRfChannelInfo->ucChannelNum <= 144) {
			prAcsReqInfo->u4LteSafeChnMask_5G_1 |= BIT(
				(prRfChannelInfo->ucChannelNum - 36) / 4);
		} else if (prRfChannelInfo->ucChannelNum >= 149 &&
				prRfChannelInfo->ucChannelNum <= 181) {
			prAcsReqInfo->u4LteSafeChnMask_5G_2 |= BIT(
				(prRfChannelInfo->ucChannelNum - 149) / 4);
		}
	}

	DBGLOG(P2P, INFO, "acs chnl mask=[0x%08x][0x%08x][0x%08x]\n",
			prAcsReqInfo->u4LteSafeChnMask_2G,
			prAcsReqInfo->u4LteSafeChnMask_5G_1,
			prAcsReqInfo->u4LteSafeChnMask_5G_2);
}

void p2pRoleFsmRunEventAcs(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr)
{
	struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo;
	struct MSG_P2P_SCAN_REQUEST *prP2pScanReqMsg;
	struct P2P_ACS_REQ_INFO *prAcsReqInfo;
	uint32_t u4MsgSize = 0;

	if (!prAdapter || !prMsgHdr)
		return;

	prMsgAcsRequest = (struct MSG_P2P_ACS_REQUEST *) prMsgHdr;
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prMsgAcsRequest->ucRoleIdx);
	prAcsReqInfo = &prP2pRoleFsmInfo->rAcsReqInfo;

	p2pRoleFsmAbortCurrentAcsReq(prAdapter, prMsgAcsRequest);

	initAcsParams(prAdapter, prMsgAcsRequest, prAcsReqInfo);

#if CFG_HOTSPOT_SUPPORT_FORCE_ACS_SCC
	if (prAdapter->rWifiVar.eDbdcMode == ENUM_DBDC_MODE_DISABLED) {
		struct BSS_INFO *prAisBssInfo;

		DBGLOG(P2P, INFO, "Report SCC channel\n");

		prAisBssInfo = aisGetConnectedBssInfo(prAdapter);
		if (prAisBssInfo) {
			/* Force SCC, indicate channel directly */
			indicateAcsResultByAisCh(prAdapter, prAcsReqInfo,
			prAisBssInfo);
			goto exit;
		}
	}
#endif

	if (prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11ANY) {
		struct BSS_INFO *prAisBssInfo;

		prAisBssInfo = aisGetAisBssInfo(prAdapter,
			AIS_DEFAULT_INDEX);
		if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
			/* Force SCC, indicate channel directly */
			indicateAcsResultByAisCh(prAdapter, prAcsReqInfo,
					prAisBssInfo);
			goto exit;
		} else if (prAdapter->fgEnable5GBand) {
			/* Prefer 5G band first */
			trimAcsScanList(prAdapter, prMsgAcsRequest,
					prAcsReqInfo, BAND_5G);
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11A;
		} else {
			trimAcsScanList(prAdapter, prMsgAcsRequest,
					prAcsReqInfo, BAND_2G4);
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11G;
		}
	}

	initAcsChnlMask(prAdapter, prMsgAcsRequest, prAcsReqInfo);

	u4MsgSize = sizeof(struct MSG_P2P_SCAN_REQUEST) + (
			prMsgAcsRequest->u4NumChannel *
				sizeof(struct RF_CHANNEL_INFO));

	prP2pScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, u4MsgSize);
	if (prP2pScanReqMsg == NULL) {
		DBGLOG(P2P, ERROR, "alloc scan req. fail\n");
		return;
	}
	kalMemSet(prP2pScanReqMsg, 0, u4MsgSize);
	prP2pScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prP2pScanReqMsg->ucBssIdx = prP2pRoleFsmInfo->ucBssIndex;
	prP2pScanReqMsg->i4SsidNum = 0;
	prP2pScanReqMsg->u4NumChannel = prMsgAcsRequest->u4NumChannel;
	prP2pScanReqMsg->u4IELen = 0;
	prP2pScanReqMsg->eScanReason = SCAN_REASON_ACS;
	kalMemCopy(&(prP2pScanReqMsg->arChannelListInfo),
			&(prMsgAcsRequest->arChannelListInfo),
			(prMsgAcsRequest->u4NumChannel *
				sizeof(struct RF_CHANNEL_INFO)));
	p2pRoleFsmRunEventScanRequest(prAdapter,
			(struct MSG_HDR *) prP2pScanReqMsg);

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

static u_int8_t
p2pRoleFsmIsAcsProcessing(IN struct ADAPTER *prAdapter,
		uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo;
	struct P2P_ACS_REQ_INFO *prAcsReqInfo;

	if (!prAdapter)
		return FALSE;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			ucRoleIdx);
	if (!prP2pRoleFsmInfo)
		return FALSE;

	prAcsReqInfo = &prP2pRoleFsmInfo->rAcsReqInfo;
	if (!prAcsReqInfo)
		return FALSE;

	return prAcsReqInfo->fgIsProcessing;
}

static void
p2pRoleFsmAbortCurrentAcsReq(IN struct ADAPTER *prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo = NULL;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo = NULL;

	if (!prAdapter || !prMsgAcsRequest)
		return;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prMsgAcsRequest->ucRoleIdx);
	prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);

	if (!p2pRoleFsmIsAcsProcessing(prAdapter, prMsgAcsRequest->ucRoleIdx))
		return;

	if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_SCAN &&
			prScanReqInfo->eScanReason == SCAN_REASON_ACS) {
		DBGLOG(P2P, INFO, "Cancel current ACS scan.\n");
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	}
}

