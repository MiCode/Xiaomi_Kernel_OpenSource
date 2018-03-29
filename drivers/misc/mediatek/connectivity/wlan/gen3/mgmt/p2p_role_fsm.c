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
#include "precomp.h"
#include "p2p_role_state.h"

#if 1
/*lint -save -e64 Type mismatch */
static PUINT_8 apucDebugP2pRoleState[P2P_ROLE_STATE_NUM] = {
	(PUINT_8) DISP_STRING("P2P_ROLE_STATE_IDLE"),
	(PUINT_8) DISP_STRING("P2P_ROLE_STATE_SCAN"),
	(PUINT_8) DISP_STRING("P2P_ROLE_STATE_REQING_CHANNEL"),
	(PUINT_8) DISP_STRING("P2P_ROLE_STATE_AP_CHNL_DETECTION"),
	(PUINT_8) DISP_STRING("P2P_ROLE_STATE_GC_JOIN")
};

/*lint -restore */
#endif /* DBG */

VOID
p2pRoleFsmStateTransition(IN P_ADAPTER_T prAdapter,
			  IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN ENUM_P2P_ROLE_STATE_T eNextState);

UINT_8 p2pRoleFsmInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prP2pChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		ASSERT_BREAK(P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) == NULL);

		prP2pRoleFsmInfo = kalMemAlloc(sizeof(P2P_ROLE_FSM_INFO_T), VIR_MEM_TYPE);

		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) = prP2pRoleFsmInfo;

		prP2pRoleFsmInfo->ucRoleIndex = ucRoleIdx;

		ASSERT_BREAK(prP2pRoleFsmInfo != NULL);

		kalMemZero(prP2pRoleFsmInfo, sizeof(P2P_ROLE_FSM_INFO_T));

		prP2pRoleFsmInfo->eCurrentState = P2P_ROLE_STATE_IDLE;

		prP2pChnlReqInfo = &prP2pRoleFsmInfo->rChnlReqInfo;
		LINK_INITIALIZE(&(prP2pChnlReqInfo->rP2pChnlReqLink));

		cnmTimerInitTimer(prAdapter,
				  &(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer),
				  (PFN_MGMT_TIMEOUT_FUNC) p2pRoleFsmRunEventTimeout, (ULONG) prP2pRoleFsmInfo);

		prP2pBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_P2P, FALSE);

		if (!prP2pBssInfo) {
			kalMemFree(prP2pRoleFsmInfo, VIR_MEM_TYPE, sizeof(P2P_ROLE_FSM_INFO_T));
			DBGLOG(P2P, ERROR, "Error allocating BSS Info Structure\n");
			break;
		}

		BSS_INFO_INIT(prAdapter, prP2pBssInfo);
		prP2pRoleFsmInfo->ucBssIndex = prP2pBssInfo->ucBssIndex;

		/* For state identify, not really used. */
		prP2pBssInfo->eIntendOPMode = OP_MODE_P2P_DEVICE;

		COPY_MAC_ADDR(prP2pBssInfo->aucOwnMacAddr, prAdapter->rMyMacAddr);
		prP2pBssInfo->aucOwnMacAddr[0] ^= 0x2;	/* change to local administrated address */

		/* For BSS_INFO back trace to P2P Role & get Role FSM. */
		prP2pBssInfo->u4PrivateData = ucRoleIdx;

		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G;
			prP2pBssInfo->u2HwDefaultFixedRateCode = RATE_CCK_1M_LONG;
		} else {
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
			prP2pBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		}

		prP2pBssInfo->ucNonHTBasicPhyType = (UINT_8)
		    rNonHTApModeAttributes[prP2pBssInfo->ucConfigAdHocAPMode].ePhyTypeIndex;
		prP2pBssInfo->u2BSSBasicRateSet =
		    rNonHTApModeAttributes[prP2pBssInfo->ucConfigAdHocAPMode].u2BSSBasicRateSet;

		prP2pBssInfo->u2OperationalRateSet =
		    rNonHTPhyAttributes[prP2pBssInfo->ucNonHTBasicPhyType].u2SupportedRateSet;

		rateGetDataRatesFromRateSet(prP2pBssInfo->u2OperationalRateSet,
					    prP2pBssInfo->u2BSSBasicRateSet,
					    prP2pBssInfo->aucAllSupportedRates, &prP2pBssInfo->ucAllSupportedRatesLen);

		prP2pBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
							OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem[0]) + MAX_IE_LENGTH);

		if (prP2pBssInfo->prBeacon) {
			prP2pBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
			prP2pBssInfo->prBeacon->ucStaRecIndex = STA_REC_INDEX_BMCAST;	/* NULL STA_REC */
			prP2pBssInfo->prBeacon->ucBssIndex = prP2pBssInfo->ucBssIndex;
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

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucQoS))
			prP2pBssInfo->fgIsQBSS = TRUE;
		else
			prP2pBssInfo->fgIsQBSS = FALSE;

		/* SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex); */

		p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);

	} while (FALSE);

	if (prP2pBssInfo)
		return prP2pBssInfo->ucBssIndex;
	else
		return P2P_DEV_BSS_INDEX;
}				/* p2pFsmInit */

VOID p2pRoleFsmUninit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pRoleFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pRoleFsmUninit()\n");

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx);

		ASSERT_BREAK(prP2pRoleFsmInfo != NULL);

		if (!prP2pRoleFsmInfo)
			return;

		prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

		p2pFuncDissolve(prAdapter, prP2pBssInfo, TRUE, REASON_CODE_DEAUTH_LEAVING_BSS);

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Function Dissolve should already enter IDLE state. */
		p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);

		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);

		/* Clear CmdQue */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo, prP2pBssInfo->ucBssIndex);
		kalClearSecurityFramesByBssIdx(prAdapter->prGlueInfo, prP2pBssInfo->ucBssIndex);
		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter, prP2pBssInfo->ucBssIndex);
		/* Clear PendingTxMsdu */
		nicFreePendingTxMsduInfoByBssIdx(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Deactivate BSS. */
		UNSET_NET_ACTIVE(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		nicDeactivateNetwork(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) = NULL;

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}

		cnmFreeBssInfo(prAdapter, prP2pBssInfo);

		if (prP2pRoleFsmInfo)
			kalMemFree(prP2pRoleFsmInfo, VIR_MEM_TYPE, sizeof(P2P_ROLE_FSM_INFO_T));

	} while (FALSE);

	return;
#if 0
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pFsmUninit()\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		p2pFuncSwitchOPMode(prAdapter, prP2pBssInfo, OP_MODE_P2P_DEVICE, TRUE);

		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		p2pStateAbort_IDLE(prAdapter, prP2pFsmInfo, P2P_STATE_NUM);

		UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		wlanAcquirePowerControl(prAdapter);

		/* Release all pending CMD queue. */
		DBGLOG(P2P, TRACE,
		       "p2pFsmUninit: wlanProcessCommandQueue, num of element:%d\n",
			prAdapter->prGlueInfo->rCmdQueue.u4NumElem);
		wlanProcessCommandQueue(prAdapter, &prAdapter->prGlueInfo->rCmdQueue);

		wlanReleasePowerControl(prAdapter);

		/* Release pending mgmt frame,
		 * mgmt frame may be pending by CMD without resource.
		 */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo, NETWORK_TYPE_P2P_INDEX);

		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter, NETWORK_TYPE_P2P_INDEX);

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}

	} while (FALSE);

	return;
#endif
}				/* p2pRoleFsmUninit */

VOID
p2pRoleFsmStateTransition(IN P_ADAPTER_T prAdapter,
			  IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN ENUM_P2P_ROLE_STATE_T eNextState)
{
	BOOLEAN fgIsTransitionOut = (BOOLEAN) FALSE;
	P_BSS_INFO_T prP2pRoleBssInfo = (P_BSS_INFO_T) NULL;

	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

	do {
		if (!IS_BSS_ACTIVE(prP2pRoleBssInfo)) {
			if (!cnmP2PIsPermitted(prAdapter))
				return;

			SET_NET_ACTIVE(prAdapter, prP2pRoleBssInfo->ucBssIndex);
			nicActivateNetwork(prAdapter, prP2pRoleBssInfo->ucBssIndex);
		}

		fgIsTransitionOut = fgIsTransitionOut ? FALSE : TRUE;

		if (!fgIsTransitionOut) {
			DBGLOG(P2P, STATE, "[P2P_ROLE][%d]TRANSITION: [%s] -> [%s]\n",
					    prP2pRoleFsmInfo->ucRoleIndex,
					    apucDebugP2pRoleState[prP2pRoleFsmInfo->eCurrentState],
					    apucDebugP2pRoleState[eNextState]);

			/* Transition into current state. */
			prP2pRoleFsmInfo->eCurrentState = eNextState;
		}

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_IDLE:
			if (!fgIsTransitionOut)
				p2pRoleStateInit_IDLE(prAdapter, prP2pRoleFsmInfo, prP2pRoleBssInfo);
			else
				p2pRoleStateAbort_IDLE(prAdapter, prP2pRoleFsmInfo, &(prP2pRoleFsmInfo->rChnlReqInfo));
			break;
		case P2P_ROLE_STATE_SCAN:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_SCAN(prAdapter, prP2pRoleFsmInfo->ucBssIndex,
						      &(prP2pRoleFsmInfo->rScanReqInfo));
			} else {
				p2pRoleStateAbort_SCAN(prAdapter, prP2pRoleFsmInfo);
			}
			break;
		case P2P_ROLE_STATE_REQING_CHANNEL:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_REQING_CHANNEL(prAdapter,
								prP2pRoleFsmInfo->ucBssIndex,
								&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_REQING_CHANNEL(prAdapter, prP2pRoleBssInfo,
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
								    &(prP2pRoleFsmInfo->rScanReqInfo), eNextState);
			}
			break;
		case P2P_ROLE_STATE_GC_JOIN:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_GC_JOIN(prAdapter,
							 prP2pRoleFsmInfo, &(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_GC_JOIN(prAdapter,
							  prP2pRoleFsmInfo, &(prP2pRoleFsmInfo->rJoinInfo), eNextState);
			}
			break;
		default:
			ASSERT(FALSE);
			break;
		}
	} while (fgIsTransitionOut);

}				/* p2pRoleFsmStateTransition */

VOID p2pRoleFsmRunEventTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) ulParamPtr;
	P_P2P_CHNL_REQ_INFO_T prP2pChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL));

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_IDLE:
			prP2pChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
			if (prP2pChnlReqInfo->fgIsChannelRequested) {
				p2pFuncReleaseCh(prAdapter, prP2pRoleFsmInfo->ucBssIndex, prP2pChnlReqInfo);
				if (IS_NET_PWR_STATE_IDLE(prAdapter, prP2pRoleFsmInfo->ucBssIndex))
					ASSERT(FALSE);
			}

			if (IS_NET_PWR_STATE_IDLE(prAdapter, prP2pRoleFsmInfo->ucBssIndex)) {
				DBGLOG(P2P, TRACE, "Role BSS IDLE, deactive network.\n");
				UNSET_NET_ACTIVE(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
				nicDeactivateNetwork(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
			}
			break;
		case P2P_ROLE_STATE_GC_JOIN:
			DBGLOG(P2P, ERROR,
			"Current P2P Role State P2P_ROLE_STATE_GC_JOIN is unexpected for FSM timeout event.\n");
			break;
		default:
			DBGLOG(P2P, ERROR,
			       "Current P2P Role State %d is unexpected for FSM timeout event.\n",
				prP2pRoleFsmInfo->eCurrentState);
			ASSERT(FALSE);
			break;
		}
	} while (FALSE);

}				/* p2pRoleFsmRunEventTimeout */

VOID p2pRoleFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo)
{

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL));

		if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
			/* Get into IDLE state. */
			p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);
		}

		/* Abort IDLE. */
		p2pRoleStateAbort_IDLE(prAdapter, prP2pRoleFsmInfo, &(prP2pRoleFsmInfo->rChnlReqInfo));

	} while (FALSE);

}				/* p2pRoleFsmRunEventAbort */

WLAN_STATUS
p2pRoleFsmRunEventDeauthTxDone(IN P_ADAPTER_T prAdapter,
			       IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_FAILURE;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(P2P, INFO, "Deauth TX Done,rTxDoneStatus = %d\n", rTxDoneStatus);

		prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

		if (prStaRec == NULL) {
			DBGLOG(P2P, TRACE, "Station Record NULL, Index:%d\n", prMsduInfo->ucStaRecIndex);
			break;
		}

		prP2pBssInfo = prAdapter->aprBssInfo[prMsduInfo->ucBssIndex];
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
			DBGLOG(P2P, TRACE, "No More Client, Media Status DISCONNECTED\n");
			p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);
		}

		nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

	} while (FALSE);

	return rWlanStatus;

}				/* p2pRoleFsmRunEventDeauthTxDone */

VOID p2pRoleFsmRunEventRxDeauthentication(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	UINT_16 u2ReasonCode = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		if (prStaRec == NULL)
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

		if (!prStaRec)
			break;

		prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

		if (prStaRec->ucStaState == STA_STATE_1)
			break;

		DBGLOG(P2P, TRACE, "RX Deauth\n");

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_INFRASTRUCTURE:
			if (authProcessRxDeauthFrame(prSwRfb,
						     prStaRec->aucMacAddr, &u2ReasonCode) == WLAN_STATUS_SUCCESS) {
				P_WLAN_DEAUTH_FRAME_T prDeauthFrame = (P_WLAN_DEAUTH_FRAME_T) prSwRfb->pvHeader;
				UINT_16 u2IELength = 0;

				if (prP2pBssInfo->prStaRecOfAP != prStaRec)
					break;

				prStaRec->u2ReasonCode = u2ReasonCode;
				u2IELength = prSwRfb->u2PacketLen - (WLAN_MAC_HEADER_LEN + REASON_CODE_FIELD_LEN);

				ASSERT(prP2pBssInfo->prStaRecOfAP == prStaRec);

				/* Indicate disconnect to Host. */
				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
								 (UINT_8) prP2pBssInfo->u4PrivateData, NULL,
								 prDeauthFrame->aucInfoElem, u2IELength, u2ReasonCode);

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec, FALSE, u2ReasonCode);

				SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

				p2pRoleFsmStateTransition(prAdapter,
							  P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
											 prP2pBssInfo->u4PrivateData),
							  P2P_ROLE_STATE_IDLE);
			}
			break;
		case OP_MODE_ACCESS_POINT:
			/* Delete client from client list. */
			if (authProcessRxDeauthFrame(prSwRfb,
						     prP2pBssInfo->aucBSSID, &u2ReasonCode) == WLAN_STATUS_SUCCESS) {

#if 0
				P_LINK_T prStaRecOfClientList = (P_LINK_T) NULL;
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;
				P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;

				prStaRecOfClientList = &prP2pBssInfo->rStaRecOfClientList;

				LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
					prCurrStaRec = LINK_ENTRY(prLinkEntry, STA_RECORD_T, rLinkEntry);

					ASSERT(prCurrStaRec);

					if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr, prStaRec->aucMacAddr)) {

						/* Remove STA from client list. */
						LINK_REMOVE_KNOWN_ENTRY(prStaRecOfClientList,
									&prCurrStaRec->rLinkEntry);

						/* Indicate to Host. */
						/* kalP2PGOStationUpdate(prAdapter->prGlueInfo, prStaRec, FALSE); */

						/* Indicate disconnect to Host. */
						p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec,
								  FALSE, u2ReasonCode);

						break;
					}
				}
#else
				if (bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec))
					/* Indicate disconnect to Host. */
					p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec, FALSE, u2ReasonCode);
#endif
			}
			break;
		case OP_MODE_P2P_DEVICE:
		default:
			/* Findout why someone sent deauthentication frame to us. */
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, TRACE, "Deauth Reason:%d\n", u2ReasonCode);

	} while (FALSE);

}				/* p2pRoleFsmRunEventRxDeauthentication */

VOID p2pRoleFsmRunEventRxDisassociation(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	UINT_16 u2ReasonCode = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		if (prStaRec == NULL) {
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
			if (prStaRec == NULL)
				break;
		}
		prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

		if (prStaRec->ucStaState == STA_STATE_1)
			break;

		DBGLOG(P2P, TRACE, "RX Disassoc\n");

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_INFRASTRUCTURE:
			if (assocProcessRxDisassocFrame(prAdapter,
							prSwRfb,
							prStaRec->aucMacAddr,
							&prStaRec->u2ReasonCode) == WLAN_STATUS_SUCCESS) {
				P_WLAN_DISASSOC_FRAME_T prDisassocFrame = (P_WLAN_DISASSOC_FRAME_T) prSwRfb->pvHeader;
				UINT_16 u2IELength = 0;

				ASSERT(prP2pBssInfo->prStaRecOfAP == prStaRec);

				if (prP2pBssInfo->prStaRecOfAP != prStaRec)
					break;

				u2IELength = prSwRfb->u2PacketLen - (WLAN_MAC_HEADER_LEN + REASON_CODE_FIELD_LEN);

				/* Indicate disconnect to Host. */
				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
								 (UINT_8) prP2pBssInfo->u4PrivateData, NULL,
								 prDisassocFrame->aucInfoElem,
								 u2IELength, prStaRec->u2ReasonCode);

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec, FALSE, prStaRec->u2ReasonCode);

				SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

				p2pRoleFsmStateTransition(prAdapter,
							  P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
											 prP2pBssInfo->u4PrivateData),
							  P2P_ROLE_STATE_IDLE);
			}
			break;
		case OP_MODE_ACCESS_POINT:
			/* Delete client from client list. */
			if (assocProcessRxDisassocFrame(prAdapter,
							prSwRfb,
							prP2pBssInfo->aucBSSID, &u2ReasonCode) == WLAN_STATUS_SUCCESS) {
#if 0
				P_LINK_T prStaRecOfClientList = (P_LINK_T) NULL;
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;
				P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;

				prStaRecOfClientList = &prP2pBssInfo->rStaRecOfClientList;

				LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
					prCurrStaRec = LINK_ENTRY(prLinkEntry, STA_RECORD_T, rLinkEntry);

					ASSERT(prCurrStaRec);

					if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr, prStaRec->aucMacAddr)) {

						/* Remove STA from client list. */
						LINK_REMOVE_KNOWN_ENTRY(prStaRecOfClientList,
									&prCurrStaRec->rLinkEntry);

						/* Indicate to Host. */
						/* kalP2PGOStationUpdate(prAdapter->prGlueInfo, prStaRec, FALSE); */

						/* Indicate disconnect to Host. */
						p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec,
								  FALSE, u2ReasonCode);

						break;
					}
				}
#else
				if (bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec))
					/* Indicate disconnect to Host. */
					p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec, FALSE, u2ReasonCode);
#endif
			}
			break;
		case OP_MODE_P2P_DEVICE:
		default:
			DBGLOG(P2P, INFO, "RX Disassoc unexpectedly. Current role: %d\n",
				prP2pBssInfo->eCurrentOPMode);
			/*
			 * Can not expect peer not send disassoc to us when we
			 * are not ready to process disassoc
			 * ASSERT(FALSE);
			 */
			break;
		}

	} while (FALSE);

}				/* p2pRoleFsmRunEventRxDisassociation */

VOID p2pRoleFsmRunEventBeaconTimeout(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pBssInfo->u4PrivateData);

		/* Only client mode would have beacon lost event. */
		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE) {
			DBGLOG(P2P, ERROR,
			       "Error case, P2P BSS %d not INFRA mode but beacon timeout\n",
				prP2pRoleFsmInfo->ucRoleIndex);
			break;
		}

		DBGLOG(P2P, TRACE,
		       "p2pFsmRunEventBeaconTimeout: BSS %d Beacon Timeout\n", prP2pRoleFsmInfo->ucRoleIndex);

		if (prP2pBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
							 prP2pRoleFsmInfo->ucRoleIndex,
							 NULL, NULL, 0, REASON_CODE_DISASSOC_INACTIVITY);

			if (prP2pBssInfo->prStaRecOfAP != NULL) {
				P_STA_RECORD_T prStaRec = prP2pBssInfo->prStaRecOfAP;

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec, FALSE,
						  REASON_CODE_DISASSOC_LEAVING_BSS);

				SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);
				/* 20120830 moved into p2pFuncDisconnect() */
				/* cnmStaRecFree(prAdapter, prP2pBssInfo->prStaRecOfAP); */
				p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);

			}
		}
	} while (FALSE);

}				/* p2pFsmRunEventBeaconTimeout */

/*================== Message Event ==================*/
VOID p2pRoleFsmRunEventStartAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_MSG_P2P_START_AP_T prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) NULL;
	P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, INFO, "p2pRoleFsmRunEventStartAP\n");

		prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) prMsgHdr;

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pStartAPMsg->ucRoleIdx);

		if (!prP2pRoleFsmInfo) {
			DBGLOG(P2P, ERROR,
			       "p2pRoleFsmRunEventStartAP: Corresponding P2P Role FSM empty: %d.\n",
				prP2pStartAPMsg->ucRoleIdx);
			break;
		}

		prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];
		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;
		prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

		if (prP2pStartAPMsg->u4BcnInterval) {
			DBGLOG(P2P, TRACE, "Beacon interval updated to :%ld\n", prP2pStartAPMsg->u4BcnInterval);
			prP2pBssInfo->u2BeaconInterval = (UINT_16) prP2pStartAPMsg->u4BcnInterval;
		} else if (prP2pBssInfo->u2BeaconInterval == 0) {
			prP2pBssInfo->u2BeaconInterval = DOT11_BEACON_PERIOD_DEFAULT;
		}

		if (prP2pStartAPMsg->u4DtimPeriod) {
			DBGLOG(P2P, TRACE, "DTIM interval updated to :%ld\n", prP2pStartAPMsg->u4DtimPeriod);
			prP2pBssInfo->ucDTIMPeriod = (UINT_8) prP2pStartAPMsg->u4DtimPeriod;
		} else if (prP2pBssInfo->ucDTIMPeriod == 0) {
			prP2pBssInfo->ucDTIMPeriod = DOT11_DTIM_PERIOD_DEFAULT;
		}

		if (prP2pStartAPMsg->u2SsidLen != 0) {
			kalMemCopy(prP2pConnReqInfo->rSsidStruct.aucSsid, prP2pStartAPMsg->aucSsid,
				   prP2pStartAPMsg->u2SsidLen);
			prP2pConnReqInfo->rSsidStruct.ucSsidLen =
			    prP2pSpecificBssInfo->u2GroupSsidLen = prP2pStartAPMsg->u2SsidLen;
			kalMemCopy(prP2pSpecificBssInfo->aucGroupSsid, prP2pStartAPMsg->aucSsid,
				   prP2pStartAPMsg->u2SsidLen);
		}

		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_PURE_AP;

			/* Overwrite AP channel */
			if (prAdapter->rWifiVar.ucApChannel) {
				prP2pConnReqInfo->rChannelInfo.ucChannelNum = prAdapter->rWifiVar.ucApChannel;

				if (prAdapter->rWifiVar.ucApChannel <= 14)
					prP2pConnReqInfo->rChannelInfo.eBand = BAND_2G4;
				else
					prP2pConnReqInfo->rChannelInfo.eBand = BAND_5G;
			}
		} else {
			prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GO;
		}

		prP2pBssInfo->eHiddenSsidType = prP2pStartAPMsg->ucHiddenSsidType;

		if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
		    (prP2pBssInfo->eIntendOPMode != OP_MODE_NUM)) {
			/* 1. No switch to AP mode.
			 * 2. Not started yet.
			 */

			if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_AP_CHNL_DETECTION &&
			    prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
				/* Make sure the state is in IDLE state. */
				p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
			} else if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_AP_CHNL_DETECTION) {
				break;
			}

			/* Leave IDLE state. */
			SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

			prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;

			if (prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum != 0) {
				p2pRoleStatePrepare_To_REQING_CHANNEL_STATE(prAdapter,
									    GET_BSS_INFO_BY_INDEX(prAdapter,
									    prP2pRoleFsmInfo->ucBssIndex),
									    &(prP2pRoleFsmInfo->rConnReqInfo),
									    &(prP2pRoleFsmInfo->rChnlReqInfo));
				p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_REQING_CHANNEL);
			} else {
				/* For AP/GO mode with specific channel or non-specific channel. */
				p2pRoleFsmStateTransition(prAdapter,
							  prP2pRoleFsmInfo, P2P_ROLE_STATE_AP_CHNL_DETECTION);
			}
		} else {
			/* 1. Current OP mode is AP mode.
			 * 2. && Intentd OP mode is OP_MODE_NUM. (finished start the AP mode)
			 */
			/* AP mode is started, do nothing. */
			break;
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventStartAP */

VOID p2pRoleFsmRunEventStopAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_SWITCH_OP_MODE_T prP2pSwitchMode = (P_MSG_P2P_SWITCH_OP_MODE_T) NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pSwitchMode = (P_MSG_P2P_SWITCH_OP_MODE_T) prMsgHdr;

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pSwitchMode->ucRoleIdx);

		if (!prP2pRoleFsmInfo) {
			DBGLOG(P2P, ERROR,
			       "p2pRoleFsmRunEventStopAP: Corresponding P2P Role FSM empty: %d.\n",
				prP2pSwitchMode->ucRoleIdx);
			break;
		}

		prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

		if (!prP2pBssInfo)
			break;

		p2pFuncStopGO(prAdapter, prP2pBssInfo);

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventStopAP */

VOID p2pRoleFsmRunEventConnectionRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_MSG_P2P_CONNECTION_REQUEST_T prP2pConnReqMsg = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;

	P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;
	P_P2P_JOIN_INFO_T prJoinInfo = (P_P2P_JOIN_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pConnReqMsg = (P_MSG_P2P_CONNECTION_REQUEST_T) prMsgHdr;

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pConnReqMsg->ucRoleIdx);

		if (!prP2pRoleFsmInfo) {
			DBGLOG(P2P, ERROR,
			       "p2pRoleFsmRunEventConnectionRequest: Corresponding P2P Role FSM empty: %d.\n",
				prP2pConnReqMsg->ucRoleIdx);
			break;
		}

		prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

		if (!prP2pBssInfo)
			break;

		prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
		prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
		prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionRequest\n");

		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
			break;

		SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Make sure the state is in IDLE state. */
		if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE)
			p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
		/* Update connection request information. */
		prConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GC;
		COPY_MAC_ADDR(prConnReqInfo->aucBssid, prP2pConnReqMsg->aucBssid);
		kalMemCopy(&(prConnReqInfo->rSsidStruct), &(prP2pConnReqMsg->rSsid), sizeof(P2P_SSID_STRUCT_T));
		kalMemCopy(prConnReqInfo->aucIEBuf, prP2pConnReqMsg->aucIEBuf, prP2pConnReqMsg->u4IELen);
		prConnReqInfo->u4BufLength = prP2pConnReqMsg->u4IELen;

		/* Find BSS Descriptor first. */
		prJoinInfo->prTargetBssDesc = scanP2pSearchDesc(prAdapter, prConnReqInfo);

		if (prJoinInfo->prTargetBssDesc == NULL) {
			/* Update scan parameter... to scan target device. */
			P_P2P_SCAN_REQ_INFO_T prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);

			if (prP2pConnReqMsg->rChannelInfo.ucChannelNum) {
				prScanReqInfo->ucNumChannelList = 1;
				prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
				prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;
				prScanReqInfo->arScanChannelList[0].ucChannelNum =
								prP2pConnReqMsg->rChannelInfo.ucChannelNum;
				prScanReqInfo->arScanChannelList[0].eBand = prP2pConnReqMsg->rChannelInfo.eBand;
				prScanReqInfo->ucSsidNum = 1;
				kalMemCopy(&(prScanReqInfo->arSsidStruct[0]), &(prP2pConnReqMsg->rSsid),
						sizeof(P2P_SSID_STRUCT_T));
				prScanReqInfo->u4BufLength = 0;	/* Prevent other P2P ID in IE. */
				prScanReqInfo->fgIsAbort = TRUE;
			} else {
				/* Update scan parameter... to scan target device. */
				/* TODO: Need refine. */
				prScanReqInfo->ucNumChannelList = 1;
				prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
				prScanReqInfo->eChannelSet = SCAN_CHANNEL_FULL;
				prScanReqInfo->u4BufLength = 0;	/* Prevent other P2P ID in IE. */
				prScanReqInfo->fgIsAbort = TRUE;
			}

			p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_SCAN);
		} else {
			prChnlReqInfo->u8Cookie = 0;
			prChnlReqInfo->ucReqChnlNum = prP2pConnReqMsg->rChannelInfo.ucChannelNum;
			prChnlReqInfo->eBand = prP2pConnReqMsg->rChannelInfo.eBand;
			prChnlReqInfo->eChnlSco = prP2pConnReqMsg->eChnlSco;
			prChnlReqInfo->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;
			prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_JOIN;

			prChnlReqInfo->eChannelWidth = prJoinInfo->prTargetBssDesc->eChannelWidth;
			prChnlReqInfo->ucCenterFreqS1 = prJoinInfo->prTargetBssDesc->ucCenterFreqS1;
			prChnlReqInfo->ucCenterFreqS2 = prJoinInfo->prTargetBssDesc->ucCenterFreqS2;

			p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_REQING_CHANNEL);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pRoleFsmRunEventConnectionRequest */

VOID p2pRoleFsmRunEventConnectionAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T) prMsgHdr;

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prDisconnMsg->ucRoleIdx);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionAbort: Connection Abort.\n");

		if (!prP2pRoleFsmInfo) {
			DBGLOG(P2P, ERROR,
			       "p2pRoleFsmRunEventConnectionAbort: Corresponding P2P Role FSM empty: %d.\n",
				prDisconnMsg->ucRoleIdx);
			break;
		}

		prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

		if (!prP2pBssInfo)
			break;

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_INFRASTRUCTURE:
			{
				UINT_8 aucBCBSSID[] = BC_BSSID;

				if (!prP2pBssInfo->prStaRecOfAP) {
					DBGLOG(P2P, TRACE, "GO's StaRec is NULL\n");
					break;
				}
				if (UNEQUAL_MAC_ADDR(prP2pBssInfo->prStaRecOfAP->aucMacAddr, prDisconnMsg->aucTargetID)
				    && UNEQUAL_MAC_ADDR(prDisconnMsg->aucTargetID, aucBCBSSID)) {
					DBGLOG(P2P, TRACE,
					       "Unequal MAC ADDR [" MACSTR ":" MACSTR "]\n",
						MAC2STR(prP2pBssInfo->prStaRecOfAP->aucMacAddr),
						MAC2STR(prDisconnMsg->aucTargetID));
					break;
				}

				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
								 prP2pRoleFsmInfo->ucRoleIndex, NULL, NULL, 0, 0);

				/* Stop rejoin timer if it is started. */
				/* TODO: If it has. */

				p2pFuncDisconnect(prAdapter, prP2pBssInfo,
						  prP2pBssInfo->prStaRecOfAP,
						  prDisconnMsg->fgSendDeauth, prDisconnMsg->u2ReasonCode);

				/* prTargetStaRec = prP2pBssInfo->prStaRecOfAP; */

				/* Fix possible KE when RX Beacon & call nicPmIndicateBssConnected().
				 * hit prStaRecOfAP == NULL. */
				p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_DISCONNECTED);

				prP2pBssInfo->prStaRecOfAP = NULL;

				SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

				p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);

			}
			break;
		case OP_MODE_ACCESS_POINT:
			{
				/* Search specific client device, and disconnect. */
				/* 1. Send deauthentication frame. */
				/* 2. Indication: Device disconnect. */
				P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;

				DBGLOG(P2P, TRACE, "Disconnecting with Target ID: " MACSTR "\n",
							MAC2STR(prDisconnMsg->aucTargetID));

				prCurrStaRec = bssRemoveClientByMac(prAdapter, prP2pBssInfo, prDisconnMsg->aucTargetID);

				if (prCurrStaRec) {
					DBGLOG(P2P, TRACE, "Disconnecting: " MACSTR "\n",
							    MAC2STR(prCurrStaRec->aucMacAddr));

					/* Glue layer indication. */
					/* kalP2PGOStationUpdate(prAdapter->prGlueInfo, prCurrStaRec, FALSE); */

					/* Send deauth & do indication. */
					p2pFuncDisconnect(prAdapter, prP2pBssInfo, prCurrStaRec,
							  prDisconnMsg->fgSendDeauth, prDisconnMsg->u2ReasonCode);
				}
#if 0
				LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
					prCurrStaRec = LINK_ENTRY(prLinkEntry, STA_RECORD_T, rLinkEntry);

					ASSERT(prCurrStaRec);

					if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr, prDisconnMsg->aucTargetID)) {

						DBGLOG(P2P, TRACE,
						       "Disconnecting: " MACSTR "\n",
							MAC2STR(prCurrStaRec->aucMacAddr));

						/* Remove STA from client list. */
						LINK_REMOVE_KNOWN_ENTRY(prStaRecOfClientList,
									&prCurrStaRec->rLinkEntry);

						/* Glue layer indication. */
						/* kalP2PGOStationUpdate(prAdapter->prGlueInfo, prCurrStaRec, FALSE); */

						/* Send deauth & do indication. */
						p2pFuncDisconnect(prAdapter, prP2pBssInfo,
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

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pRoleFsmRunEventConnectionAbort */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is called when JOIN complete message event is received from SAA.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pRoleFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_P2P_JOIN_INFO_T prJoinInfo = (P_P2P_JOIN_INFO_T) NULL;
	P_MSG_JOIN_COMP_T prJoinCompMsg = (P_MSG_JOIN_COMP_T) NULL;
	P_SW_RFB_T prAssocRspSwRfb = (P_SW_RFB_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prJoinCompMsg = (P_MSG_JOIN_COMP_T) prMsgHdr;
		prStaRec = prJoinCompMsg->prStaRec;
		prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

		DBGLOG(P2P, TRACE, "P2P BSS %d, Join Complete\n", prStaRec->ucBssIndex);

		if (prStaRec == NULL) {
			ASSERT(FALSE);
			break;
		} else if (prStaRec->ucBssIndex >= P2P_DEV_BSS_INDEX) {
			ASSERT(FALSE);
			break;
		}

		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			if (prP2pBssInfo->u4PrivateData >= BSS_P2P_NUM)
				break;

			prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pBssInfo->u4PrivateData);

			prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

			/* Check SEQ NUM */
			if (prJoinCompMsg->ucSeqNum == prJoinInfo->ucSeqNumOfReqMsg) {
				ASSERT(prStaRec == prJoinInfo->prTargetStaRec);
				prJoinInfo->fgIsJoinComplete = TRUE;

				if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {

					/* 4 <1.1> Change FW's Media State immediately. */
					p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_CONNECTED);

					/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
					if ((prP2pBssInfo->prStaRecOfAP) && (prP2pBssInfo->prStaRecOfAP != prStaRec)) {
						cnmStaRecChangeState(prAdapter,
								     prP2pBssInfo->prStaRecOfAP, STA_STATE_1);

						cnmStaRecFree(prAdapter, prP2pBssInfo->prStaRecOfAP);

						prP2pBssInfo->prStaRecOfAP = NULL;
					}
					/* 4 <1.3> Update BSS_INFO_T */
					p2pFuncUpdateBssInfoForJOIN(prAdapter,
								    prJoinInfo->prTargetBssDesc,
								    prStaRec, prP2pBssInfo, prAssocRspSwRfb);

					/* 4 <1.4> Activate current AP's STA_RECORD_T in Driver. */
					cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

#if CFG_SUPPORT_P2P_RSSI_QUERY
					/* <1.5> Update RSSI if necessary */
					nicUpdateRSSI(prAdapter, prP2pBssInfo->ucBssIndex,
						      (INT_8) (RCPI_TO_dBm(prStaRec->ucRCPI)), 0);
#endif

					/* 4 <1.6> Indicate Connected Event to Host immediately. */
					/* Require BSSID, Association ID, Beacon Interval.. from AIS_BSS_INFO_T */
					/* p2pIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED,
					 * prStaRec->aucMacAddr); */
					if (prJoinInfo->prTargetBssDesc)
						scanReportBss2Cfg80211(prAdapter,
								       OP_MODE_P2P_DEVICE, prJoinInfo->prTargetBssDesc);
					kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
									 prP2pRoleFsmInfo->ucRoleIndex,
									 &prP2pRoleFsmInfo->rConnReqInfo,
									 prJoinInfo->aucIEBuf,
									 prJoinInfo->u4BufLength,
									 prStaRec->u2StatusCode);

				} else {
					/* Join Fail */
					/* 4 <2.1> Redo JOIN process with other Auth Type if possible */
					if (p2pFuncRetryJOIN(prAdapter, prStaRec, prJoinInfo) == FALSE) {
						P_BSS_DESC_T prBssDesc;

						/* Increase Failure Count */
						prStaRec->ucJoinFailureCount++;

						prBssDesc = prJoinInfo->prTargetBssDesc;

						ASSERT(prBssDesc);
						ASSERT(prBssDesc->fgIsConnecting);

						prBssDesc->fgIsConnecting = FALSE;

						kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
										 prP2pRoleFsmInfo->ucRoleIndex,
										 &prP2pRoleFsmInfo->rConnReqInfo,
										 prJoinInfo->aucIEBuf,
										 prJoinInfo->u4BufLength,
										 prStaRec->u2StatusCode);

					}

				}
			}

			if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_GC_JOIN) {
				/* Return to IDLE state. */
				p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_IDLE);
			}
		} else {
			ASSERT(FALSE);
			break;
		}

	} while (FALSE);

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pRoleFsmRunEventJoinComplete */

VOID p2pRoleFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_SCAN_REQUEST_T prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_P2P_SCAN_REQ_INFO_T prScanReqInfo = (P_P2P_SCAN_REQ_INFO_T) NULL;
	UINT_32 u4ChnlListSize = 0;
	P_P2P_SSID_STRUCT_T prP2pSsidStruct = (P_P2P_SSID_STRUCT_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) prMsgHdr;

		prP2pBssInfo = prAdapter->aprBssInfo[prP2pScanReqMsg->ucBssIdx];

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pBssInfo->u4PrivateData);

		if (prP2pRoleFsmInfo == NULL)
			break;

		prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) prMsgHdr;
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
			DBGLOG(P2P, TRACE, "Scan Request Channel List Number: %d\n", prScanReqInfo->ucNumChannelList);
			if (prScanReqInfo->ucNumChannelList > MAXIMUM_OPERATION_CHANNEL_LIST) {
				DBGLOG(P2P, TRACE,
				       "Channel List Number Overloaded: %d, change to: %d\n",
					prScanReqInfo->ucNumChannelList, MAXIMUM_OPERATION_CHANNEL_LIST);
				prScanReqInfo->ucNumChannelList = MAXIMUM_OPERATION_CHANNEL_LIST;
			}

			u4ChnlListSize = sizeof(RF_CHANNEL_INFO_T) * prScanReqInfo->ucNumChannelList;
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
		     prScanReqInfo->ucSsidNum < prP2pScanReqMsg->i4SsidNum; prScanReqInfo->ucSsidNum++) {

			kalMemCopy(prScanReqInfo->arSsidStruct[prScanReqInfo->ucSsidNum].aucSsid,
				   prP2pSsidStruct->aucSsid, prP2pSsidStruct->ucSsidLen);

			prScanReqInfo->arSsidStruct[prScanReqInfo->ucSsidNum].ucSsidLen = prP2pSsidStruct->ucSsidLen;

			prP2pSsidStruct++;
		}

		/* IE Buffer */
		kalMemCopy(prScanReqInfo->aucIEBuf, prP2pScanReqMsg->pucIEBuf, prP2pScanReqMsg->u4IELen);

		prScanReqInfo->u4BufLength = prP2pScanReqMsg->u4IELen;

		p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, P2P_ROLE_STATE_SCAN);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pDevFsmRunEventScanRequest */

VOID
p2pRoleFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) NULL;
	P_P2P_SCAN_REQ_INFO_T prScanReqInfo = (P_P2P_SCAN_REQ_INFO_T) NULL;
	ENUM_P2P_ROLE_STATE_T eNextState = P2P_ROLE_STATE_NUM;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL) && (prP2pRoleFsmInfo != NULL));

		DBGLOG(P2P, TRACE, "P2P Role Scan Done Event\n");

		prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);
		prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;

		if (prScanDoneMsg->ucSeqNum != prScanReqInfo->ucSeqNumOfScnMsg) {
			/* Scan Done message sequence number mismatch.
			 * Ignore this event. (P2P FSM issue two scan events.)
			 */
			/* The scan request has been cancelled.
			 * Ignore this message. It is possible.
			 */
			DBGLOG(P2P, TRACE,
			       "P2P Role Scan Don SeqNum Received:%d <-> P2P Role Fsm SCAN Seq Issued:%d\n",
				prScanDoneMsg->ucSeqNum, prScanReqInfo->ucSeqNumOfScnMsg);

			break;
		}

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_SCAN:
			{
				P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
				P_P2P_JOIN_INFO_T prP2pJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

				prScanReqInfo->fgIsAbort = FALSE;

				if (prConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_GC) {

					prP2pJoinInfo->prTargetBssDesc =
					    p2pFuncKeepOnConnection(prAdapter,
								    prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex],
								    prConnReqInfo,
								    &prP2pRoleFsmInfo->rChnlReqInfo,
								    &prP2pRoleFsmInfo->rScanReqInfo);
					if ((prP2pJoinInfo->prTargetBssDesc) == NULL) {
						eNextState = P2P_ROLE_STATE_SCAN;
					} else {
						/* For GC join. */
						eNextState = P2P_ROLE_STATE_REQING_CHANNEL;
					}
				} else {
					eNextState = P2P_ROLE_STATE_IDLE;
				}

			}
			break;
		case P2P_ROLE_STATE_AP_CHNL_DETECTION:
			eNextState = P2P_ROLE_STATE_REQING_CHANNEL;
			break;
		default:
			/* Unexpected channel scan done event without being chanceled. */
			ASSERT(FALSE);
			break;
		}

		prScanReqInfo->fgIsScanRequest = FALSE;

		p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, eNextState);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventScanDone */

VOID
p2pRoleFsmRunEventChnlGrant(IN P_ADAPTER_T prAdapter,
			    IN P_MSG_HDR_T prMsgHdr, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo)
{
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;
	P_MSG_CH_GRANT_T prMsgChGrant = (P_MSG_CH_GRANT_T) NULL;
	UINT_8 ucTokenID = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL) && (prP2pRoleFsmInfo != NULL));

		DBGLOG(P2P, TRACE, "P2P Run Event Role Channel Grant\n");

		prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;
		ucTokenID = prMsgChGrant->ucTokenID;
		prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
		if (prChnlReqInfo->u4MaxInterval != prMsgChGrant->u4GrantInterval) {
			DBGLOG(P2P, WARN,
			       "P2P Role:%d Request Channel Interval:%d, Grant Interval:%d\n",
				prP2pRoleFsmInfo->ucRoleIndex, prChnlReqInfo->u4MaxInterval,
				prMsgChGrant->u4GrantInterval);
			prChnlReqInfo->u4MaxInterval = prMsgChGrant->u4GrantInterval;
		}

		if (ucTokenID == prChnlReqInfo->ucSeqNumOfChReq) {
			ENUM_P2P_ROLE_STATE_T eNextState = P2P_ROLE_STATE_NUM;

			switch (prP2pRoleFsmInfo->eCurrentState) {
			case P2P_ROLE_STATE_REQING_CHANNEL:
				switch (prChnlReqInfo->eChnlReqType) {
				case CH_REQ_TYPE_JOIN:
					eNextState = P2P_ROLE_STATE_GC_JOIN;
					break;
				case CH_REQ_TYPE_GO_START_BSS:
					eNextState = P2P_ROLE_STATE_IDLE;
					break;
				default:
					DBGLOG(P2P, WARN,
					       "p2pRoleFsmRunEventChnlGrant: Invalid Channel Request Type:%d\n",
						prChnlReqInfo->eChnlReqType);
					ASSERT(FALSE);
					break;
				}

				p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, eNextState);
				break;
			case P2P_ROLE_STATE_IDLE:
				DBGLOG(P2P, WARN, "Ignore channel grant event when in ROLE IDLE\n");
				p2pFuncReleaseCh(prAdapter, prP2pRoleFsmInfo->ucBssIndex, prChnlReqInfo);
				break;
			default:
				/* Channel is granted under unexpected state.
				 * Driver should cancel channel privileagea before leaving the states.
				 */
				if (IS_BSS_ACTIVE(prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex])) {
					DBGLOG(P2P, WARN,
					       "p2pRoleFsmRunEventChnlGrant: Invalid CurrentState:%d\n",
						prP2pRoleFsmInfo->eCurrentState);
					ASSERT(FALSE);
				}
				break;
			}
		} else {
			/* Channel requsted, but released. */
			ASSERT(!prChnlReqInfo->fgIsChannelRequested);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pRoleFsmRunEventChnlGrant */

/* ////////////////////////////////////// */
VOID p2pRoleFsmRunEventDissolve(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	/* TODO: */

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventDissolve */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Event of Successful Completion of AAA Module.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
p2pRoleFsmRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	ENUM_PARAM_MEDIA_STATE_T eOriMediaState;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prStaRec != NULL) && (prP2pBssInfo != NULL));

		eOriMediaState = prP2pBssInfo->eConnectionState;

		bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
		if (prP2pBssInfo->rStaRecOfClientList.u4NumElem >= P2P_MAXIMUM_CLIENT_COUNT ||
		    kalP2PMaxClients(prAdapter->prGlueInfo, prP2pBssInfo->rStaRecOfClientList.u4NumElem)) {
#else
		if (prP2pBssInfo->rStaRecOfClientList.u4NumElem >= P2P_MAXIMUM_CLIENT_COUNT) {
#endif
			rStatus = WLAN_STATUS_RESOURCES;
			break;
		}

		bssAddClient(prAdapter, prP2pBssInfo, prStaRec);

		prStaRec->u2AssocId = bssAssignAssocID(prStaRec);

		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

		p2pChangeMediaState(prAdapter, prP2pBssInfo, PARAM_MEDIA_STATE_CONNECTED);

		/* Update Connected state to FW. */
		if (eOriMediaState != prP2pBssInfo->eConnectionState)
			nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

	} while (FALSE);

	return rStatus;
}				/* p2pRunEventAAAComplete */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Event of Successful Completion of AAA Module.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
p2pRoleFsmRunEventAAASuccess(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prStaRec != NULL) && (prP2pBssInfo != NULL));

		if ((prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P) || (prP2pBssInfo->u4PrivateData >= BSS_P2P_NUM)) {
			ASSERT(FALSE);
			rStatus = WLAN_STATUS_INVALID_DATA;
			break;
		}

		ASSERT(prP2pBssInfo->ucBssIndex < P2P_DEV_BSS_INDEX);

		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prP2pBssInfo->u4PrivateData);

		/* Glue layer indication. */
		kalP2PGOStationUpdate(prAdapter->prGlueInfo, prP2pRoleFsmInfo->ucRoleIndex, prStaRec, TRUE);

	} while (FALSE);

	return rStatus;
}				/* p2pRoleFsmRunEventAAASuccess */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Event of Tx Fail of AAA Module.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID p2pRoleFsmRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prStaRec);

	bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

	p2pFuncDisconnect(prAdapter, prP2pBssInfo, prStaRec, FALSE, REASON_CODE_UNSPECIFIED);

	/* 20120830 moved into p2puUncDisconnect. */
	/* cnmStaRecFree(prAdapter, prStaRec); */

}				/* p2pRoleFsmRunEventAAATxFail */

VOID p2pRoleFsmRunEventSwitchOPMode(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_SWITCH_OP_MODE_T prSwitchOpMode = (P_MSG_P2P_SWITCH_OP_MODE_T) prMsgHdr;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwitchOpMode != NULL));

		DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventSwitchOPMode\n");

		if (prSwitchOpMode->ucRoleIdx < BSS_P2P_NUM)
			prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[prSwitchOpMode->ucRoleIdx];
		else
			ASSERT(FALSE);

		ASSERT(prP2pRoleFsmInfo->ucBssIndex < P2P_DEV_BSS_INDEX);

		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		if (prSwitchOpMode->eOpMode >= OP_MODE_NUM) {
			ASSERT(FALSE);
			break;
		}

		/* P2P Device / GC. */
		p2pFuncSwitchOPMode(prAdapter, prP2pBssInfo, prSwitchOpMode->eOpMode, TRUE);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventSwitchOPMode */

/* /////////////////////////////// TO BE REFINE //////////////////////////////// */

VOID p2pFsmRunEventBeaconUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_ROLE_FSM_INFO_T prRoleP2pFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_BEACON_UPDATE_T prBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	P_P2P_BEACON_UPDATE_INFO_T prBcnUpdateInfo = (P_P2P_BEACON_UPDATE_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventBeaconUpdate\n");

		prBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) prMsgHdr;

		prRoleP2pFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, prBcnUpdateMsg->ucRoleIndex);

		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prRoleP2pFsmInfo->ucBssIndex);

		prBcnUpdateInfo = &(prRoleP2pFsmInfo->rBeaconUpdateInfo);

		p2pFuncBeaconUpdate(prAdapter,
				    prP2pBssInfo,
				    prBcnUpdateInfo,
				    prBcnUpdateMsg->pucBcnHdr,
				    prBcnUpdateMsg->u4BcnHdrLen,
				    prBcnUpdateMsg->pucBcnBody, prBcnUpdateMsg->u4BcnBodyLen);

		if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
		    (prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			/* AP is created, Beacon Update. */
			/* nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_P2P_INDEX); */

			bssUpdateBeaconContent(prAdapter, prRoleP2pFsmInfo->ucBssIndex);

			/* nicPmIndicateBssCreated(prAdapter, NETWORK_TYPE_P2P_INDEX); */
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventBeaconUpdate */

VOID
p2pProcessEvent_UpdateNOAParam(IN P_ADAPTER_T prAdapter,
			       IN UINT_8 ucBssIdx, IN P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo;
	UINT_32 i;
	BOOLEAN fgNoaAttrExisted = FALSE;

	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

	prP2pSpecificBssInfo->fgEnableOppPS = prEventUpdateNoaParam->ucEnableOppPS;
	prP2pSpecificBssInfo->u2CTWindow = prEventUpdateNoaParam->u2CTWindow;
	prP2pSpecificBssInfo->ucNoAIndex = prEventUpdateNoaParam->ucNoAIndex;
	prP2pSpecificBssInfo->ucNoATimingCount = prEventUpdateNoaParam->ucNoATimingCount;

	fgNoaAttrExisted |= prP2pSpecificBssInfo->fgEnableOppPS;

	ASSERT(prP2pSpecificBssInfo->ucNoATimingCount <= P2P_MAXIMUM_NOA_COUNT);

	for (i = 0; i < prP2pSpecificBssInfo->ucNoATimingCount; i++) {
		/* in used */
		prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse = prEventUpdateNoaParam->arEventNoaTiming[i].ucIsInUse;
		/* count */
		prP2pSpecificBssInfo->arNoATiming[i].ucCount = prEventUpdateNoaParam->arEventNoaTiming[i].ucCount;
		/* duration */
		prP2pSpecificBssInfo->arNoATiming[i].u4Duration = prEventUpdateNoaParam->arEventNoaTiming[i].u4Duration;
		/* interval */
		prP2pSpecificBssInfo->arNoATiming[i].u4Interval = prEventUpdateNoaParam->arEventNoaTiming[i].u4Interval;
		/* start time */
		prP2pSpecificBssInfo->arNoATiming[i].u4StartTime =
		    prEventUpdateNoaParam->arEventNoaTiming[i].u4StartTime;

		fgNoaAttrExisted |= prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse;
	}

	prP2pSpecificBssInfo->fgIsNoaAttrExisted = fgNoaAttrExisted;

	/* update beacon content by the change */
	bssUpdateBeaconContent(prAdapter, ucBssIdx);
}				/* p2pProcessEvent_UpdateNOAParam */
