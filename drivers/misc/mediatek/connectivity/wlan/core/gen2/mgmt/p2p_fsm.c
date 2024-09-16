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

#if CFG_ENABLE_WIFI_DIRECT

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
static PUINT_8 apucDebugP2pState[P2P_STATE_NUM] = {
	(PUINT_8) DISP_STRING("P2P_STATE_IDLE"),
	(PUINT_8) DISP_STRING("P2P_STATE_SCAN"),
	(PUINT_8) DISP_STRING("P2P_STATE_AP_CHANNEL_DETECT"),
	(PUINT_8) DISP_STRING("P2P_STATE_REQING_CHANNEL"),
	(PUINT_8) DISP_STRING("P2P_STATE_CHNL_ON_HAND"),
	(PUINT_8) DISP_STRING("P2P_STATE_GC_JOIN")
};

/*lint -restore */
#else
static UINT_8 apucDebugP2pState[P2P_STATE_NUM] = {
	P2P_STATE_IDLE,
	P2P_STATE_SCAN,
	P2P_STATE_AP_CHANNEL_DETECT,
	P2P_STATE_REQING_CHANNEL,
	P2P_STATE_CHNL_ON_HAND,
	P2P_STATE_GC_JOIN
};

#endif /* DBG */

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID p2pFsmCheckDeauthComplete(IN P_ADAPTER_T prAdapter);
static u_int8_t p2pFsmIsAcsProcessing(IN P_ADAPTER_T prAdapter);
static void p2pFsmAbortCurrentAcsReq(IN P_ADAPTER_T prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*   p2pStateXXX : Processing P2P FSM related action.
  *   p2pFSMXXX : Control P2P FSM flow.
  *   p2pFuncXXX : Function for doing one thing.
  */
VOID p2pFsmInit(IN P_ADAPTER_T prAdapter)
{

	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		ASSERT_BREAK(prP2pFsmInfo != NULL);

		LINK_INITIALIZE(&(prP2pFsmInfo->rMsgEventQueue));
		LINK_INITIALIZE(&(prP2pBssInfo->rStaRecOfClientList));

		prP2pFsmInfo->eCurrentState = prP2pFsmInfo->ePreviousState = P2P_STATE_IDLE;
		prP2pFsmInfo->prTargetBss = NULL;
		prP2pFsmInfo->fgIsWPSMode = 0;

		cnmTimerInitTimer(prAdapter,
				  &(prAdapter->rP2pFsmTimeoutTimer),
				  (PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventFsmTimeout, (ULONG) prP2pFsmInfo);
		cnmTimerInitTimer(prAdapter,
				  &(prAdapter->rTdlsStateTimer),
				  (PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventTdlsTimeout, (ULONG) NULL);

		/* 4 <2> Initiate BSS_INFO_T - common part */
		BSS_INFO_INIT(prAdapter, NETWORK_TYPE_P2P_INDEX);

		/* init add key action */
		prP2pBssInfo->eKeyAction = SEC_TX_KEY_COMMAND;

		/* 4 <2.1> Initiate BSS_INFO_T - Setup HW ID */
		prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
		prP2pBssInfo->ucHwDefaultFixedRateCode = RATE_OFDM_6M;

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
			prP2pBssInfo->prBeacon->ucStaRecIndex = 0xFF;	/* NULL STA_REC */
			prP2pBssInfo->prBeacon->ucNetworkType = NETWORK_TYPE_P2P_INDEX;
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
#if CFG_SUPPORT_P2P_EAP_FAIL_WORKAROUND
		prP2pBssInfo->fgP2PPendingDeauth = FALSE;
		prP2pBssInfo->u4P2PEapTxDoneTime = 0;
#endif
		if (prAdapter->rWifiVar.fgSupportQoS)
			prP2pBssInfo->fgIsQBSS = TRUE;
		else
			prP2pBssInfo->fgIsQBSS = FALSE;

		SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		if (prP2pFsmInfo->fgIsApMode)
			p2pFuncSwitchOPMode(prAdapter, prP2pBssInfo,
				OP_MODE_P2P_DEVICE, TRUE);

		p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
	} while (FALSE);

}				/* p2pFsmInit */

/*----------------------------------------------------------------------------*/
/*!
* @brief The function is used to uninitialize the value in P2P_FSM_INFO_T for
*        P2P FSM operation
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmUninit(IN P_ADAPTER_T prAdapter)
{
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

		p2pFunCleanQueuedMgmtFrame(prAdapter,
				&prP2pFsmInfo->rQueuedActionFrame);

		/* Release all pending CMD queue. */
		DBGLOG(P2P, TRACE, "p2pFsmUninit: wlanProcessCommandQueue, num of element:%d\n",
				    (UINT_32) prAdapter->prGlueInfo->rCmdQueue.u4NumElem);
		wlanProcessCommandQueue(prAdapter, &prAdapter->prGlueInfo->rCmdQueue);

		wlanReleasePowerControl(prAdapter);

		/* Release pending mgmt frame,
		 * mgmt frame may be pending by CMD without resource.
		 */
		kalClearMgmtFramesByNetType(prAdapter->prGlueInfo, NETWORK_TYPE_P2P_INDEX);

		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX);

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}

	} while (FALSE);

	return;

}				/* end of p2pFsmUninit() */

VOID p2pFsmStateTransition(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState)
{
	BOOLEAN fgIsTransOut = (BOOLEAN) FALSE;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;

	ASSERT((prAdapter != NULL) && (prP2pFsmInfo != NULL));

	prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

	do {
		if (!IS_BSS_ACTIVE(prP2pBssInfo)) {
			if (!cnmP2PIsPermitted(prAdapter))
				return;

#if !CFG_SUPPORT_RLM_ACT_NETWORK
			SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);
			nicActivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX);
			nicUpdateBss(prAdapter, NETWORK_TYPE_P2P_INDEX);
#else
			rlmActivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX, NET_ACTIVE_SRC_NONE);
#endif
		}

		fgIsTransOut = fgIsTransOut ? FALSE : TRUE;

		if (!fgIsTransOut) {
#if DBG
			DBGLOG(P2P, STATE, "TRANSITION: [%s] -> [%s]\n",
					    apucDebugP2pState[prP2pFsmInfo->eCurrentState],
					    apucDebugP2pState[eNextState]);
#else
			DBGLOG(P2P, STATE, "[%d] TRANSITION: [%d] -> [%d]\n",
					    DBG_P2P_IDX, apucDebugP2pState[prP2pFsmInfo->eCurrentState],
					    apucDebugP2pState[eNextState]);
#endif

			/* Transition into current state. */
			prP2pFsmInfo->ePreviousState = prP2pFsmInfo->eCurrentState;
			prP2pFsmInfo->eCurrentState = eNextState;
		}

		switch (prP2pFsmInfo->eCurrentState) {
		case P2P_STATE_IDLE:
			if (fgIsTransOut)
				p2pStateAbort_IDLE(prAdapter, prP2pFsmInfo, eNextState);
			else
				fgIsTransOut = p2pStateInit_IDLE(prAdapter, prP2pFsmInfo, prP2pBssInfo, &eNextState);
			break;
		case P2P_STATE_SCAN:
			if (fgIsTransOut) {
				/* Scan done / scan canceled. */
				p2pStateAbort_SCAN(prAdapter, prP2pFsmInfo, eNextState);
			} else {
				/* Initial scan request. */
				p2pStateInit_SCAN(prAdapter, prP2pFsmInfo);
			}

			break;
		case P2P_STATE_AP_CHANNEL_DETECT:
			if (fgIsTransOut) {
				/* Scan done */
				/* Get sparse channel result. */
				p2pStateAbort_AP_CHANNEL_DETECT(prAdapter,
								prP2pFsmInfo, prP2pSpecificBssInfo, eNextState);
			}

			else {
				/* Initial passive scan request. */
				p2pStateInit_AP_CHANNEL_DETECT(prAdapter, prP2pFsmInfo);
			}

			break;
		case P2P_STATE_REQING_CHANNEL:
			if (fgIsTransOut) {
				/* Channel on hand / Channel canceled. */
				p2pStateAbort_REQING_CHANNEL(prAdapter, prP2pFsmInfo, eNextState);
			} else {
				/* Initial channel request. */
				p2pFuncAcquireCh(prAdapter, &(prP2pFsmInfo->rChnlReqInfo));
			}

			break;
		case P2P_STATE_CHNL_ON_HAND:
			if (fgIsTransOut) {
				p2pStateAbort_CHNL_ON_HAND(prAdapter, prP2pFsmInfo, prP2pBssInfo, eNextState);
			} else {
				/* Initial channel ready. */
				/* Send channel ready event. */
				/* Start a FSM timer. */
				p2pStateInit_CHNL_ON_HAND(prAdapter, prP2pBssInfo, prP2pFsmInfo);
			}

			break;
		case P2P_STATE_GC_JOIN:
			if (fgIsTransOut) {
				/* Join complete / join canceled. */
				p2pStateAbort_GC_JOIN(prAdapter, prP2pFsmInfo, &(prP2pFsmInfo->rJoinInfo), eNextState);
			} else {
				if (prP2pFsmInfo->prTargetBss == NULL) {
					ASSERT(FALSE);
				} else {
					/* Send request to SAA module. */
					p2pStateInit_GC_JOIN(prAdapter,
						     prP2pFsmInfo,
						     prP2pBssInfo,
						     &(prP2pFsmInfo->rJoinInfo), prP2pFsmInfo->prTargetBss);
				}
			}

			break;
		default:
			break;
		}

	} while (fgIsTransOut);

}				/* p2pFsmStateTransition */

VOID p2pFsmRunEventSwitchOPMode(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_SWITCH_OP_MODE_T prSwitchOpMode = (P_MSG_P2P_SWITCH_OP_MODE_T) prMsgHdr;
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T) NULL;
	P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwitchOpMode != NULL));


		DBGLOG(P2P, TRACE, "p2pFsmRunEventSwitchOPMode\n");

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pConnSettings = prAdapter->rWifiVar.prP2PConnSettings;
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prConnReqInfo = prP2pFsmInfo != NULL ?
				&(prP2pFsmInfo->rConnReqInfo) : NULL;

		if (prSwitchOpMode == NULL)
			break;

		if (prSwitchOpMode->eOpMode >= OP_MODE_NUM) {
			ASSERT(FALSE);
			break;
		}

		/* P2P Device / GC. */
		p2pFuncSwitchOPMode(prAdapter, prP2pBssInfo, prSwitchOpMode->eOpMode, TRUE);

		if (prConnReqInfo &&
				prP2pBssInfo->eIftype == IFTYPE_P2P_CLIENT &&
				prSwitchOpMode->eIftype == IFTYPE_STATION)
			kalP2pUnlinkBss(prAdapter->prGlueInfo,
					prConnReqInfo->aucBssid);

		prP2pBssInfo->eIftype = prSwitchOpMode->eIftype;

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventSwitchOPMode */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle scan done event during Device Discovery.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_SCAN_REQ_INFO_T prScanReqInfo = (P_P2P_SCAN_REQ_INFO_T) NULL;
	P_MSG_SCN_SCAN_DONE prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) NULL;
	ENUM_P2P_STATE_T eNextState = P2P_STATE_NUM;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		/* This scan done event is either for "SCAN" phase or "SEARCH" state or "LISTEN" state.
		 * The scan done for SCAN phase & SEARCH state doesn't imply Device
		 * Discovery over.
		 */
		DBGLOG(P2P, TRACE, "P2P Scan Done Event\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);
		prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;

		if (prScanDoneMsg->ucSeqNum != prScanReqInfo->ucSeqNumOfScnMsg) {
			/* Scan Done message sequence number mismatch.
			 * Ignore this event. (P2P FSM issue two scan events.)
			 */
			/* The scan request has been cancelled.
			 * Ignore this message. It is possible.
			 */
			DBGLOG(P2P, TRACE, "P2P Scan Don SeqNum:%d <-> P2P Fsm SCAN Msg:%d\n",
					    prScanDoneMsg->ucSeqNum, prScanReqInfo->ucSeqNumOfScnMsg);

			break;
		}

		switch (prP2pFsmInfo->eCurrentState) {
		case P2P_STATE_SCAN:
			{
				P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo = &(prP2pFsmInfo->rConnReqInfo);

				prScanReqInfo->fgIsAbort = FALSE;

				if (prConnReqInfo->fgIsConnRequest) {
					prP2pFsmInfo->prTargetBss = p2pFuncKeepOnConnection(prAdapter,
										&prP2pFsmInfo->rConnReqInfo,
										&prP2pFsmInfo->rChnlReqInfo,
										&prP2pFsmInfo->rScanReqInfo);
					if (prP2pFsmInfo->prTargetBss == NULL)
						eNextState = P2P_STATE_SCAN;
					else
						eNextState = P2P_STATE_REQING_CHANNEL;
				} else if (prScanReqInfo->fgIsAcsReq == TRUE) {
					struct P2P_ACS_REQ_INFO *prAcsReqInfo;

					prAcsReqInfo = &prP2pFsmInfo->rAcsReqInfo;
					prScanReqInfo->fgIsAcsReq = FALSE;
					p2pFunCalAcsChnScores(prAdapter,
							BAND_2G4);
					if (wlanQueryLteSafeChannel(prAdapter) ==
							WLAN_STATUS_SUCCESS) {
						/* do nothing & wait for FW event */
					} else {
						DBGLOG(P2P, WARN, "query safe chn fail.\n");
						p2pFunProcessAcsReport(prAdapter,
								NULL,
								prAcsReqInfo);
					}
					eNextState = P2P_STATE_IDLE;
				} else {
					eNextState = P2P_STATE_IDLE;
				}

			}
			break;
		case P2P_STATE_AP_CHANNEL_DETECT:
			eNextState = P2P_STATE_REQING_CHANNEL;
			break;
		default:
			/* Unexpected channel scan done event without being chanceled. */
			ASSERT(FALSE);
			break;
		}

		prScanReqInfo->fgIsScanRequest = FALSE;

		p2pFsmStateTransition(prAdapter, prP2pFsmInfo, eNextState);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventScanDone */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is call when channel is granted by CNM module from FW.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;
	P_MSG_CH_GRANT_T prMsgChGrant = (P_MSG_CH_GRANT_T) NULL;
	UINT_8 ucTokenID = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "P2P Run Event Channel Grant\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;
		ucTokenID = prMsgChGrant->ucTokenID;
		prP2pFsmInfo->u4GrantInterval = prMsgChGrant->u4GrantInterval;

		/* Reset p2p conn state & channel extended flag */
		prP2pFsmInfo->fgIsChannelExtended = FALSE;
		prP2pFsmInfo->eCNNState = P2P_CNN_NORMAL;

		prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

		if (ucTokenID == prChnlReqInfo->ucSeqNumOfChReq) {
			ENUM_P2P_STATE_T eNextState = P2P_STATE_NUM;

			switch (prP2pFsmInfo->eCurrentState) {
			case P2P_STATE_REQING_CHANNEL:
				switch (prChnlReqInfo->eChannelReqType) {
				case CHANNEL_REQ_TYPE_REMAIN_ON_CHANNEL:
					eNextState = P2P_STATE_CHNL_ON_HAND;
					break;
				case CHANNEL_REQ_TYPE_GC_JOIN_REQ:
					eNextState = P2P_STATE_GC_JOIN;
					break;
				case CHANNEL_REQ_TYPE_GO_START_BSS:
					eNextState = P2P_STATE_IDLE;
					break;
				default:
					break;
				}

				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, eNextState);
				break;
			default:
				/* Channel is granted under unexpected state.
				 * Driver should cancel channel privileagea before leaving the states.
				 */
				ASSERT(FALSE);
				break;
			}

		} else {
			/* Channel requsted, but released. */
			/* ASSERT(!prChnlReqInfo->fgIsChannelRequested); */
			DBGLOG(P2P, TRACE, "Channel requsted, but released\n");
		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pFsmRunEventChGrant */

VOID p2pFsmRunEventChannelRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;
	P_MSG_P2P_CHNL_REQUEST_T prP2pChnlReqMsg = (P_MSG_P2P_CHNL_REQUEST_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	ENUM_P2P_STATE_T eNextState = P2P_STATE_NUM;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pChnlReqMsg = (P_MSG_P2P_CHNL_REQUEST_T) prMsgHdr;
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventChannelRequest\n");

		/* Make sure the state is in IDLE state. */
		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		/* Cookie can only be assign after abort.(for indication) */
		prChnlReqInfo->u8Cookie = prP2pChnlReqMsg->u8Cookie;
		prChnlReqInfo->ucReqChnlNum = prP2pChnlReqMsg->rChannelInfo.ucChannelNum;
		prChnlReqInfo->eBand = prP2pChnlReqMsg->rChannelInfo.eBand;
		prChnlReqInfo->eChnlSco = prP2pChnlReqMsg->eChnlSco;
		prChnlReqInfo->u4MaxInterval = prP2pChnlReqMsg->u4Duration;
		prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_REMAIN_ON_CHANNEL;

		eNextState = P2P_STATE_REQING_CHANNEL;

		p2pFsmStateTransition(prAdapter, prP2pFsmInfo, eNextState);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventChannelRequest */

VOID p2pFsmRunEventChannelAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_MSG_P2P_CHNL_ABORT_T prChnlAbortMsg = (P_MSG_P2P_CHNL_ABORT_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prChnlAbortMsg = (P_MSG_P2P_CHNL_ABORT_T) prMsgHdr;
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prChnlReqInfo = &prP2pFsmInfo->rChnlReqInfo;

		DBGLOG(P2P, TRACE, "p2pFsmRunEventChannelAbort\n");

		p2pFunCleanQueuedMgmtFrame(prAdapter,
				&prP2pFsmInfo->rQueuedActionFrame);

		if ((prChnlAbortMsg->u8Cookie == prChnlReqInfo->u8Cookie) && (prChnlReqInfo->fgIsChannelRequested)) {

			ASSERT((prP2pFsmInfo->eCurrentState == P2P_STATE_REQING_CHANNEL ||
				(prP2pFsmInfo->eCurrentState == P2P_STATE_CHNL_ON_HAND)));
			/*
			 * If cancel-roc cmd is called from Supplicant while driver is waiting
			 * for FW's channel grant event, roc event must be returned to Supplicant
			 * first to reset Supplicant's variables and then transition to idle state.
			 */
			if (prP2pFsmInfo->eCurrentState == P2P_STATE_REQING_CHANNEL) {
				DBGLOG(P2P, INFO, "Transition to P2P_STATE_CHNL_ON_HAND first\n");
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_CHNL_ON_HAND);
			}
			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
		} else {
			/* just avoid supplicant waiting too long */
			complete(&prAdapter->prGlueInfo->rP2pReq);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventChannelAbort */

VOID p2pFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{

	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_MSG_P2P_SCAN_REQUEST_T prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) NULL;
	P_P2P_SCAN_REQ_INFO_T prScanReqInfo = (P_P2P_SCAN_REQ_INFO_T) NULL;
	UINT_32 u4ChnlListSize = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prP2pScanReqMsg = (P_MSG_P2P_SCAN_REQUEST_T) prMsgHdr;
		prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventScanRequest\n");

		/* Make sure the state is in IDLE state. */
		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		ASSERT(prScanReqInfo->fgIsScanRequest == FALSE);

		prScanReqInfo->fgIsAbort = TRUE;
		prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
		prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;

		/* Channel List */
		prScanReqInfo->ucNumChannelList = prP2pScanReqMsg->u4NumChannel;
		DBGLOG(P2P, TRACE, "Scan Request Channel List Number: %d\n", prScanReqInfo->ucNumChannelList);
		if (prScanReqInfo->ucNumChannelList > MAXIMUM_OPERATION_CHANNEL_LIST) {
			DBGLOG(P2P, TRACE, "Channel List Number Overloaded: %d, change to: %d\n",
					    prScanReqInfo->ucNumChannelList, MAXIMUM_OPERATION_CHANNEL_LIST);
			prScanReqInfo->ucNumChannelList = MAXIMUM_OPERATION_CHANNEL_LIST;
		}

		u4ChnlListSize = sizeof(RF_CHANNEL_INFO_T) * prScanReqInfo->ucNumChannelList;
		kalMemCopy(prScanReqInfo->arScanChannelList, prP2pScanReqMsg->arChannelListInfo, u4ChnlListSize);

		/* TODO: I only take the first SSID. Multiple SSID may be needed in the future. */
		/* SSID */
		if (prP2pScanReqMsg->i4SsidNum >= 1)
			kalMemCopy(&(prScanReqInfo->rSsidStruct), prP2pScanReqMsg->prSSID, sizeof(P2P_SSID_STRUCT_T));
		else
			prScanReqInfo->rSsidStruct.ucSsidLen = 0;

		/* IE Buffer */
		kalMemCopy(prScanReqInfo->aucIEBuf, prP2pScanReqMsg->pucIEBuf, prP2pScanReqMsg->u4IELen);

		prScanReqInfo->u4BufLength = prP2pScanReqMsg->u4IELen;
		prScanReqInfo->fgIsAcsReq = prP2pScanReqMsg->fgIsAcsReq;
		prP2pFsmInfo->eCNNState = P2P_CNN_NORMAL;

		p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_SCAN);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventScanRequest */

VOID p2pFsmRunEventScanAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{

	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventScanAbort\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo->eCurrentState == P2P_STATE_SCAN) {
			P_P2P_SCAN_REQ_INFO_T prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

			prScanReqInfo->fgIsAbort = TRUE;

			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventScanAbort */

VOID p2pFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventAbort\n");

		if (prP2pFsmInfo->eCurrentState != P2P_STATE_IDLE) {

			if (prP2pFsmInfo->eCurrentState == P2P_STATE_SCAN) {

				P_P2P_SCAN_REQ_INFO_T prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

				prScanReqInfo->fgIsAbort = TRUE;
			} else if (prP2pFsmInfo->eCurrentState == P2P_STATE_REQING_CHANNEL) {
				/* 2012/08/06: frog
				 * Prevent Start GO.
				 */
				prP2pBssInfo->eIntendOPMode = OP_MODE_NUM;
			}
			/* For other state, is there any special action that should be take before leaving? */

			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
		} else {
			/* P2P State IDLE. */
			P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

			if (prChnlReqInfo->fgIsChannelRequested)
				p2pFuncReleaseCh(prAdapter, prChnlReqInfo);

			cnmTimerStopTimer(prAdapter, &(prAdapter->rP2pFsmTimeoutTimer));
		}

	} while (FALSE);

}				/* p2pFsmRunEventAbort */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle FSM Timeout.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventFsmTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParam)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) ulParam;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL));

		DBGLOG(P2P, TRACE, "P2P FSM Timeout Event\n");

		switch (prP2pFsmInfo->eCurrentState) {
		case P2P_STATE_IDLE:
			{
				P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = &prP2pFsmInfo->rChnlReqInfo;

				if (prChnlReqInfo->fgIsChannelRequested) {
					p2pFuncReleaseCh(prAdapter, prChnlReqInfo);
				} else if (IS_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX)) {
					DBGLOG(P2P, INFO, "Force DeactivateNetwork");
					UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);
					nicDeactivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX);
				}
			}
			break;

/* case P2P_STATE_SCAN: */
/* break; */
/* case P2P_STATE_AP_CHANNEL_DETECT: */
/* break; */
/* case P2P_STATE_REQING_CHANNEL: */
/* break; */
		case P2P_STATE_CHNL_ON_HAND:
#if 0
			switch (prP2pFsmInfo->eListenExted) {
			case P2P_DEV_NOT_EXT_LISTEN:
			case P2P_DEV_EXT_LISTEN_WAITFOR_TIMEOUT:
				DBGLOG(P2P, INFO, "p2p timeout, state==P2P_STATE_CHNL_ON_HAND, eListenExted: %d\n",
					prP2pFsmInfo->eListenExted);
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
				prP2pFsmInfo->eListenExted = P2P_DEV_NOT_EXT_LISTEN;
				break;
			case P2P_DEV_EXT_LISTEN_ING:
				DBGLOG(P2P, INFO, "p2p timeout, state==P2P_STATE_CHNL_ON_HAND, eListenExted: %d\n",
					prP2pFsmInfo->eListenExted);
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_CHNL_ON_HAND);
				prP2pFsmInfo->eListenExted = P2P_DEV_EXT_LISTEN_WAITFOR_TIMEOUT;
				break;
			default:
				ASSERT(FALSE);
				DBGLOG(P2P, ERROR,
					"Current P2P State %d is unexpected for FSM timeout event.\n",
					prP2pFsmInfo->eCurrentState);
#else
			if (prP2pFsmInfo->fgIsChannelExtended == TRUE) {
				prP2pFsmInfo->fgIsChannelExtended = FALSE;
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
				break;
			}
			switch (prP2pFsmInfo->eCNNState) {
			case P2P_CNN_GO_NEG_REQ:
			case P2P_CNN_GO_NEG_RESP:
			case P2P_CNN_INVITATION_REQ:
			case P2P_CNN_DEV_DISC_REQ:
			case P2P_CNN_PROV_DISC_REQ:
				DBGLOG(P2P, INFO,
					"Re-enter channel on hand state, eCNNState: %d\n",
					prP2pFsmInfo->eCNNState);
				prP2pFsmInfo->fgIsChannelExtended = TRUE;
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_CHNL_ON_HAND);
				break;

			case P2P_CNN_NORMAL:
			case P2P_CNN_GO_NEG_CONF:
			case P2P_CNN_INVITATION_RESP:
			case P2P_CNN_DEV_DISC_RESP:
			case P2P_CNN_PROV_DISC_RES:
			default:
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
				break;
#endif
			}
/* case P2P_STATE_GC_JOIN: */
/* break; */
		default:
			break;
		}

	} while (FALSE);

}				/* p2pFsmRunEventFsmTimeout */


/*
 * TDLS link monitor function
 * teardown link if setup failed or
 * no data traffic for 4s
 */
VOID p2pFsmRunEventTdlsTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParam)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	struct ksta_info *prTargetSta = prGlueInfo->prStaHash[STA_HASH_SIZE];

	if (!prTargetSta) {
		DBGLOG(TDLS, INFO, "TDLS: No target station, return\n");
		return;
	}

	if (prTargetSta->eTdlsRole == MTK_TDLS_ROLE_RESPONDER) {
		prTargetSta->u4Throughput = (prTargetSta->ulRxBytes * HZ) / TDLS_MONITOR_UT;

		if (prTargetSta->u4Throughput < TDLS_TEARDOWN_RX_THD) {
			switch (prTargetSta->eTdlsStatus) {
			case MTK_TDLS_LINK_ENABLE:
				MTKTdlsTearDown(prGlueInfo, prTargetSta, "Low RX Throughput");
			default:
				return;
			}
		}
		DBGLOG(TDLS, INFO, "TDLS: Rx data stream OK\n");
		prTargetSta->ulRxBytes = 0;
		goto start_timer;
	}

	switch (prTargetSta->eTdlsStatus) {
	case MTK_TDLS_NOT_SETUP:
		DBGLOG(TDLS, INFO, "Last TDLS monitor timer\n");
		return;
	case MTK_TDLS_SETUP_INPROCESS:
		DBGLOG(TDLS, INFO, "TDLS: setup timeout\n");
		if (prTargetSta->u4SetupFailCount++ > TDLS_SETUP_COUNT)
			MTKTdlsTearDown(prAdapter->prGlueInfo, prTargetSta, "Setup Failed");
		break;
	case MTK_TDLS_LINK_ENABLE:
		/* go through as we need restart timer to monitor tdls link */
	default:
		if (time_after(jiffies, prGlueInfo->ulLastUpdate + 2 * SAMPLING_UT)) {
			MTKTdlsTearDown(prAdapter->prGlueInfo, prTargetSta, "No Traffic");
			return;
		}
		DBGLOG(TDLS, INFO, "TDLS: TX data stream OK\n");
		break;
	}

start_timer:
	DBGLOG(TDLS, TRACE, "Restart tdls monitor timer\n");
	cnmTimerStartTimer(prAdapter, &(prAdapter->rTdlsStateTimer),
			   SEC_TO_MSEC(TDLS_MONITOR_UT));
}


VOID p2pFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_MSG_P2P_MGMT_TX_REQUEST_T prMgmtTxMsg = (P_MSG_P2P_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventMgmtFrameTx\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prMgmtTxMsg = (P_MSG_P2P_MGMT_TX_REQUEST_T) prMsgHdr;
		prMgmtFrame = prMgmtTxMsg->prMgmtMsduInfo;

		p2pFuncTxMgmtFrame(prAdapter,
				   &prP2pFsmInfo->rMgmtTxInfo, prMgmtFrame, prMgmtFrame->u8Cookie);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pFsmRunEventMgmtTx */

VOID p2pFsmRunEventStartAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_START_AP_T prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo;
	P_P2P_SCAN_REQ_INFO_T prScanReqInfo;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventStartAP\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		if (prP2pFsmInfo == NULL)
			break;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) prMsgHdr;
		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;
		prConnReqInfo = &(prP2pFsmInfo->rConnReqInfo);

		if (prP2pStartAPMsg->u4BcnInterval) {
			DBGLOG(P2P, TRACE, "Beacon interval updated to: %u\n", prP2pStartAPMsg->u4BcnInterval);
			prP2pBssInfo->u2BeaconInterval = (UINT_16) prP2pStartAPMsg->u4BcnInterval;
		} else if (prP2pBssInfo->u2BeaconInterval == 0) {
			prP2pBssInfo->u2BeaconInterval = DOT11_BEACON_PERIOD_DEFAULT;
		}

		if (prP2pStartAPMsg->u4DtimPeriod) {
			DBGLOG(P2P, TRACE, "DTIM interval updated to: %u\n", prP2pStartAPMsg->u4DtimPeriod);
			prP2pBssInfo->ucDTIMPeriod = (UINT_8) prP2pStartAPMsg->u4DtimPeriod;
		} else if (prP2pBssInfo->ucDTIMPeriod == 0) {
			prP2pBssInfo->ucDTIMPeriod = DOT11_DTIM_PERIOD_DEFAULT;
		}

		if (prP2pStartAPMsg->u2SsidLen != 0) {
			kalMemCopy(prP2pBssInfo->aucSSID, prP2pStartAPMsg->aucSsid, prP2pStartAPMsg->u2SsidLen);
			kalMemCopy(prP2pSpecificBssInfo->aucGroupSsid, prP2pStartAPMsg->aucSsid,
				   prP2pStartAPMsg->u2SsidLen);
			prP2pBssInfo->ucSSIDLen = prP2pSpecificBssInfo->u2GroupSsidLen = prP2pStartAPMsg->u2SsidLen;
		}

		prP2pBssInfo->eHiddenSsidType = prP2pStartAPMsg->ucHiddenSsidType;

		/* TODO: JB */
		/* Privacy & inactive timeout. */

		if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
		    (prP2pBssInfo->eIntendOPMode != OP_MODE_NUM)) {
			UINT_8 ucPreferedChnl = 0;
			ENUM_BAND_T eBand = BAND_NULL;
			ENUM_CHNL_EXT_T eSco = CHNL_EXT_SCN;
			ENUM_P2P_STATE_T eNextState = P2P_STATE_NUM;
			P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = prAdapter->rWifiVar.prP2PConnSettings;

			if (prP2pFsmInfo->eCurrentState != P2P_STATE_SCAN &&
			    prP2pFsmInfo->eCurrentState != P2P_STATE_IDLE) {
				/* Make sure the state is in IDLE state. */
				p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);
			}
			prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsGOInitialDone = 0;
			DBGLOG(P2P, INFO, "NFC:fgIsGOInitialDone[%d]\n",
				prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsGOInitialDone);

			/* Leave IDLE state. */
			SET_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);

			/* Trigger P2P FSM to REQING_CHANNEL state for AP mode. */
			prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;

			/* Clear GC's connection request. */
			prConnReqInfo->fgIsConnRequest = FALSE;

#if CFG_SUPPORT_CFG_FILE
#ifdef ENABLED_IN_ENGUSERDEBUG
			/* Overwrite AP channel */
			if (prAdapter->rWifiVar.ucApChannel) {
				DBGLOG(P2P, INFO, "p2pFsmRunEventStartAP: ucApChannel = %u\n",
					prAdapter->rWifiVar.ucApChannel);
				prP2pConnSettings->ucOperatingChnl = prAdapter->rWifiVar.ucApChannel;
				if (prAdapter->rWifiVar.ucApChannel <= 14)
					prP2pConnSettings->eBand = BAND_2G4;
				else
					prP2pConnSettings->eBand = BAND_5G;
			}
#endif
#endif

			p2pFuncClearGcDeauthRetry(prAdapter);

			if ((cnmPreferredChannel(prAdapter,
						 &eBand,
						 &ucPreferedChnl,
						 &eSco) == FALSE) && (prP2pConnSettings->ucOperatingChnl == 0)) {
				/* Sparse channel detection using passive mode. */
				eNextState = P2P_STATE_AP_CHANNEL_DETECT;
			} else {
				prChnlReqInfo = &prP2pFsmInfo->rChnlReqInfo;
				prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

#if 1
				/* 2012-01-27: frog - Channel set from upper layer is the first priority. */
				/* Because the channel & beacon is decided by p2p_supplicant. */
				/* Channel set from upper layer is the first priority */
				if (prP2pConnSettings->ucOperatingChnl != 0) {
					prP2pSpecificBssInfo->ucPreferredChannel = prP2pConnSettings->ucOperatingChnl;
					prP2pSpecificBssInfo->eRfBand = prP2pConnSettings->eBand;
					prP2pSpecificBssInfo->eRfSco = rlmDecideScoForAP(prAdapter, prP2pBssInfo);
				} else {
					ASSERT(ucPreferedChnl != 0);
					prP2pSpecificBssInfo->ucPreferredChannel = ucPreferedChnl;
					prP2pSpecificBssInfo->eRfBand = eBand;
					prP2pSpecificBssInfo->eRfSco = rlmDecideScoForAP(prAdapter, prP2pBssInfo);
				}
#else
				if (ucPreferedChnl) {
					prP2pSpecificBssInfo->ucPreferredChannel = ucPreferedChnl;
					prP2pSpecificBssInfo->eRfBand = eBand;
					prP2pSpecificBssInfo->eRfSco = rlmDecideScoForAP(prAdapter, prP2pBssInfo);
				} else {
					ASSERT(prP2pConnSettings->ucOperatingChnl != 0);
					prP2pSpecificBssInfo->ucPreferredChannel = prP2pConnSettings->ucOperatingChnl;
					prP2pSpecificBssInfo->eRfBand = prP2pConnSettings->eBand;
					prP2pSpecificBssInfo->eRfSco = rlmDecideScoForAP(prAdapter, prP2pBssInfo);
				}
#endif
				prChnlReqInfo->ucReqChnlNum = prP2pSpecificBssInfo->ucPreferredChannel;
				prChnlReqInfo->eBand = prP2pSpecificBssInfo->eRfBand;
				prChnlReqInfo->u4MaxInterval = P2P_AP_CHNL_HOLD_TIME_MS;
				prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_GO_START_BSS;

				eNextState = P2P_STATE_REQING_CHANNEL;
			}

			prP2pFsmInfo->eCNNState = P2P_CNN_NORMAL;
			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, eNextState);

		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventStartAP */

VOID p2pFsmRunEventNetDeviceRegister(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_NETDEV_REGISTER_T prNetDevRegisterMsg = (P_MSG_P2P_NETDEV_REGISTER_T) NULL;

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventNetDeviceRegister\n");

		prNetDevRegisterMsg = (P_MSG_P2P_NETDEV_REGISTER_T) prMsgHdr;

		if (prNetDevRegisterMsg->fgIsEnable) {
			p2pSetMode((prNetDevRegisterMsg->ucMode == 1) ? TRUE : FALSE);

			if (p2pLaunch(prAdapter->prGlueInfo))
				ASSERT(prAdapter->fgIsP2PRegistered);

		} else {
			if (prAdapter->fgIsP2PRegistered)
				p2pRemove(prAdapter->prGlueInfo);

		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventNetDeviceRegister */

VOID p2pFsmRunEventUpdateMgmtFrame(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_MGMT_FRAME_UPDATE_T prP2pMgmtFrameUpdateMsg = (P_MSG_P2P_MGMT_FRAME_UPDATE_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventUpdateMgmtFrame\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prP2pMgmtFrameUpdateMsg = (P_MSG_P2P_MGMT_FRAME_UPDATE_T) prMsgHdr;

		switch (prP2pMgmtFrameUpdateMsg->eBufferType) {
		case ENUM_FRAME_TYPE_EXTRA_IE_BEACON:
			break;
		case ENUM_FRAME_TYPE_EXTRA_IE_ASSOC_RSP:
			break;
		case ENUM_FRAME_TYPE_EXTRA_IE_PROBE_RSP:
			break;
		case ENUM_FRAME_TYPE_PROBE_RSP_TEMPLATE:
			break;
		case ENUM_FRAME_TYPE_BEACON_TEMPLATE:
			break;
		default:
			break;
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventUpdateMgmtFrame */

VOID p2pFsmRunEventBeaconUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_BEACON_UPDATE_T prBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventBeaconUpdate\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		if (prP2pFsmInfo == NULL)
			break;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) prMsgHdr;

		DBGLOG_MEM8_IE_ONE_LINE(P2P, TRACE, "BcnHdr"
			, prBcnUpdateMsg->pucBcnHdr, prBcnUpdateMsg->u4BcnHdrLen)
		DBGLOG_MEM8_IE_ONE_LINE(P2P, TRACE, "BcnBody"
			, prBcnUpdateMsg->pucBcnBody, prBcnUpdateMsg->u4BcnBodyLen);

		p2pFuncProcessBeacon(prAdapter,
				     prP2pBssInfo,
				     &prP2pFsmInfo->rBcnContentInfo,
				     prBcnUpdateMsg->pucBcnHdr,
				     prBcnUpdateMsg->u4BcnHdrLen,
				     prBcnUpdateMsg->pucBcnBody,
				     prBcnUpdateMsg->u4BcnBodyLen);

		if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
		    (prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			/* AP is created, Beacon Update. */
			bssUpdateBeaconContent(prAdapter, NETWORK_TYPE_P2P_INDEX);

#if CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP
			p2pFuncUpdateProbeRspIEs(prAdapter, prBcnUpdateMsg,
						NETWORK_TYPE_P2P_INDEX);
#endif
			/* nicPmIndicateBssCreated(prAdapter, NETWORK_TYPE_P2P_INDEX); */
		} else
			DBGLOG(P2P, WARN, "driver skipped the beacon update! CurrentOPMode :%d eIntendOPMode:%d\n"
				, prP2pBssInfo->eCurrentOPMode, prP2pBssInfo->eIntendOPMode);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventBeaconUpdate */

VOID p2pFsmRunEventStopAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventStopAP\n");

		if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
		    && (prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			p2pFuncDissolve(prAdapter, prP2pBssInfo, TRUE, REASON_CODE_DEAUTH_LEAVING_BSS);
		}
		/* 20120118: Moved to p2pFuncSwitchOPMode(). */
		/* UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX); */

		/* Enter IDLE state. */
		SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

#if CFG_SUPPORT_WFD
		p2pFsmRunEventWfdSettingUpdate(prAdapter, NULL);
#endif

		/* p2pFsmRunEventAbort(prAdapter, prAdapter->rWifiVar.prP2pFsmInfo); */
		p2pFsmStateTransition(prAdapter, prAdapter->rWifiVar.prP2pFsmInfo, P2P_STATE_IDLE);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventStopAP */

VOID p2pFsmRunEventConnectionRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;
	P_MSG_P2P_CONNECTION_REQUEST_T prConnReqMsg = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;
	P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prConnReqMsg = (P_MSG_P2P_CONNECTION_REQUEST_T) prMsgHdr;

		prConnReqInfo = &(prP2pFsmInfo->rConnReqInfo);
		prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionRequest\n");

		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
			break;

		SET_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		/* In P2P GC case, the interval of two ASSOC flow could be very short, */
		/* we must start to connect directly before Deauth done */
		p2pFsmCheckDeauthComplete(prAdapter);

		/* Make sure the state is in IDLE state. */
		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		/* Update connection request information. */
		prConnReqInfo->fgIsConnRequest = TRUE;
		COPY_MAC_ADDR(prConnReqInfo->aucBssid, prConnReqMsg->aucBssid);
		kalMemCopy(&(prConnReqInfo->rSsidStruct), &(prConnReqMsg->rSsid), sizeof(P2P_SSID_STRUCT_T));
		kalMemCopy(prConnReqInfo->aucIEBuf, prConnReqMsg->aucIEBuf, prConnReqMsg->u4IELen);
		prConnReqInfo->u4BufLength = prConnReqMsg->u4IELen;

		/* Find BSS Descriptor first. */
		prP2pFsmInfo->prTargetBss = scanP2pSearchDesc(prAdapter, prP2pBssInfo, prConnReqInfo);
		prP2pFsmInfo->eCNNState = P2P_CNN_NORMAL;

		p2pFuncClearGcDeauthRetry(prAdapter);

		if (prP2pFsmInfo->prTargetBss == NULL) {
			/* Update scan parameter... to scan target device. */
			P_P2P_SCAN_REQ_INFO_T prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

			DBGLOG(P2P, INFO, "p2pFsmRunEventConnectionRequest,Trigger New Scan\n");

			prScanReqInfo->ucNumChannelList = 1;
			prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
			prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;
			prScanReqInfo->arScanChannelList[0].ucChannelNum = prConnReqMsg->rChannelInfo.ucChannelNum;
			kalMemCopy(&(prScanReqInfo->rSsidStruct), &(prConnReqMsg->rSsid), sizeof(P2P_SSID_STRUCT_T));
			prScanReqInfo->u4BufLength = 0;	/* Prevent other P2P ID in IE. */
			prScanReqInfo->fgIsAbort = TRUE;

			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_SCAN);
		} else {
			prChnlReqInfo->u8Cookie = 0;
			prChnlReqInfo->ucReqChnlNum = prConnReqMsg->rChannelInfo.ucChannelNum;
			prChnlReqInfo->eBand = prConnReqMsg->rChannelInfo.eBand;
			prChnlReqInfo->eChnlSco = prConnReqMsg->eChnlSco;
			prChnlReqInfo->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;
			prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_GC_JOIN_REQ;
			DBGLOG(P2P, INFO, "p2pFsmRunEventConnectionRequest, Report the Connecting BSS Again.\n");
			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_REQING_CHANNEL);
		}

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pFsmRunEventConnectionRequest */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle Connection Request from Supplicant.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventConnectionAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	STA_RECORD_T *prStaRec = (P_STA_RECORD_T) NULL;
	P_STA_RECORD_T prTargetStaRec = (P_STA_RECORD_T) NULL;
	P_P2P_GC_DISCONNECTION_REQ_INFO_T prGcDisConnReqInfo;

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionAbort: Connection Abort.\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T) prMsgHdr;

		prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) NETWORK_TYPE_P2P_INDEX, prDisconnMsg->aucTargetID);

		/*
		 * Do nothing as TDLS disable operation will free this STA REC
		 * when this is TDLS peer.
		 * This operation will go through when as GO/HP.
		 */
		if (prStaRec && prStaRec->eStaType == STA_TYPE_TDLS_PEER) {
			DBGLOG(P2P, INFO, "TDLS peer, do nothing\n");
			return;
		}

		switch (prP2pBssInfo->eCurrentOPMode) {
		case OP_MODE_INFRASTRUCTURE:
			{
				UINT_8 aucBCBSSID[] = BC_BSSID;

				prTargetStaRec = prP2pBssInfo->prStaRecOfAP;

				if (!prTargetStaRec) {
					DBGLOG(P2P, TRACE, "GO's StaRec is NULL\n");
					break;
				}
				if (UNEQUAL_MAC_ADDR(prTargetStaRec->aucMacAddr, prDisconnMsg->aucTargetID)
				    && UNEQUAL_MAC_ADDR(prDisconnMsg->aucTargetID, aucBCBSSID)) {
					DBGLOG(P2P, TRACE,
					       "Unequal MAC ADDR [ %pM : %pM ]\n",
						prTargetStaRec->aucMacAddr,
						prDisconnMsg->aucTargetID);
					break;
				}

				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					NULL, NULL, 0, 0,
					WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY);

				/* Stop rejoin timer if it is started. */
				/* TODO: If it has. */

				prGcDisConnReqInfo = &(prP2pFsmInfo->rGcDisConnReqInfo);
				prGcDisConnReqInfo->prTargetStaRec = prTargetStaRec;
				prGcDisConnReqInfo->u4RetryCount = 0;
				prGcDisConnReqInfo->u2ReasonCode = prDisconnMsg->u2ReasonCode;
				prGcDisConnReqInfo->fgSendDeauth = prDisconnMsg->fgSendDeauth;

				p2pFuncDisconnect(prAdapter, prTargetStaRec, prDisconnMsg->fgSendDeauth,
						  prDisconnMsg->u2ReasonCode);

				DBGLOG(P2P, INFO, "start GC deauth timer for %pM\n",
					prTargetStaRec->aucMacAddr);
				cnmTimerStopTimer(prAdapter, &(prTargetStaRec->rDeauthTxDoneTimer));
				cnmTimerInitTimer(prAdapter, &(prTargetStaRec->rDeauthTxDoneTimer),
					(PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventDeauthTimeout, (ULONG) prTargetStaRec);
				cnmTimerStartTimer(prAdapter, &(prTargetStaRec->rDeauthTxDoneTimer),
					P2P_DEAUTH_TIMEOUT_TIME_MS);

				prP2pBssInfo->prStaRecOfAP = NULL;

				SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);

			}
			break;
		case OP_MODE_ACCESS_POINT:
			{
				P_LINK_T prStaRecOfClientList = &prP2pBssInfo->rStaRecOfClientList;
				/* Search specific client device, and disconnect. */
				/* 1. Send deauthentication frame. */
				/* 2. Indication: Device disconnect. */
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;
				P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;

				DBGLOG(P2P, TRACE,
				       "Disconnecting with Target ID: %pM\n",
					prDisconnMsg->aucTargetID);

				LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
					prCurrStaRec = LINK_ENTRY(prLinkEntry, STA_RECORD_T, rLinkEntry);

					if (prCurrStaRec
					&& EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr, prDisconnMsg->aucTargetID)) {

						DBGLOG(P2P, TRACE,
						       "Disconnecting: %pM\n",
							prCurrStaRec->aucMacAddr);

						/* Remove STA from client list. */
						LINK_REMOVE_KNOWN_ENTRY(prStaRecOfClientList,
									&prCurrStaRec->rLinkEntry);

						/* Glue layer indication. */
						/* kalP2PGOStationUpdate(prAdapter->prGlueInfo, prCurrStaRec, FALSE); */

						/* Send deauth & do indication. */
						p2pFuncDisconnect(prAdapter, prCurrStaRec, prDisconnMsg->fgSendDeauth,
								  prDisconnMsg->u2ReasonCode);

						DBGLOG(P2P, INFO, "start GO deauth timer for %pM\n",
							prCurrStaRec->aucMacAddr);
						cnmTimerStopTimer(prAdapter, &(prCurrStaRec->rDeauthTxDoneTimer));
						cnmTimerInitTimer(prAdapter, &(prCurrStaRec->rDeauthTxDoneTimer),
							(PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventDeauthTimeout,
							(ULONG) prCurrStaRec);
						cnmTimerStartTimer(prAdapter, &(prCurrStaRec->rDeauthTxDoneTimer),
							P2P_DEAUTH_TIMEOUT_TIME_MS);

						/* prTargetStaRec = prCurrStaRec; */

						break;
					}
				}

			}
			break;
		case OP_MODE_P2P_DEVICE:
		default:
			ASSERT(FALSE);
			break;
		}

	} while (FALSE);

	/* 20120830 moved into p2pFuncDisconnect() */
	/* if ((!prDisconnMsg->fgSendDeauth) && (prTargetStaRec)) { */
	/* cnmStaRecFree(prAdapter, prTargetStaRec, TRUE); */
	/* } */

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventConnectionAbort */

VOID p2pFsmRunEventDissolve(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{

	/* TODO: */

	DBGLOG(P2P, TRACE, "p2pFsmRunEventDissolve\n");

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}

static VOID p2pFsmDeauthComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec,
	IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;

	do {
		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		/* Skip deauth tx done if AAA in progress */
		if ((prStaRec->eAuthAssocState == AAA_STATE_SEND_AUTH2 ||
				prStaRec->eAuthAssocState == AAA_STATE_SEND_ASSOC2) &&
			(prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
			(p2pFuncIsAPMode(prP2pFsmInfo) == FALSE)) {
			DBGLOG(P2P, WARN, "Skip deauth tx done since AAA fsm is in progress.\n");
			break;
		} else if (prStaRec->eAuthAssocState == SAA_STATE_SEND_AUTH1 ||
			prStaRec->eAuthAssocState == SAA_STATE_SEND_ASSOC1) {
			DBGLOG(P2P, WARN,
				"Skip deauth tx done since SAA fsm is in progress.\n");
			return;
		}

		if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) &&
			(p2pFuncRetryGcDeauth(prAdapter, prP2pFsmInfo, prStaRec,
				rTxDoneStatus) == TRUE))
			break;

		/* Change station state. */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

		/* Reset Station Record Status. */
		p2pFuncResetStaRecStatus(prAdapter, prStaRec);

		bssRemoveStaRecFromClientList(prAdapter, prP2pBssInfo, prStaRec);

		cnmStaRecFree(prAdapter, prStaRec, TRUE);

		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) {
			p2pFuncClearGcDeauthRetry(prAdapter);
			p2pFuncDeauthComplete(prAdapter, prP2pBssInfo);
		} else if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
			(prP2pBssInfo->rStaRecOfClientList.u4NumElem == 0) &&
			IS_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX)) {
			p2pFuncDeauthComplete(prAdapter, prP2pBssInfo);
		}
	} while (FALSE);
}

static VOID p2pFsmCheckDeauthComplete(IN P_ADAPTER_T prAdapter)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_GC_DISCONNECTION_REQ_INFO_T prGcDisConnReqInfo;

	do {
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prGcDisConnReqInfo = &(prP2pFsmInfo->rGcDisConnReqInfo);

		DBGLOG(P2P, INFO, "p2pFsmCheckDeauthComplete\n");

		if (prGcDisConnReqInfo && prGcDisConnReqInfo->fgSendDeauth) {
			DBGLOG(P2P, INFO, "Force stop previous deauth process since new connection came.\n");
			p2pFsmDeauthComplete(prAdapter,
				prGcDisConnReqInfo->prTargetStaRec, TX_RESULT_LIFE_TIMEOUT);
		}
	} while (FALSE);
}

VOID p2pFsmRunEventDeauthTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParam)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) ulParam;

	if (prStaRec) {
		DBGLOG(P2P, INFO, "Deauth frame timeout for %pM\n", prStaRec->aucMacAddr);
		p2pFsmDeauthComplete(prAdapter, prStaRec, TX_RESULT_LIFE_TIMEOUT);
	}
}

WLAN_STATUS
p2pFsmRunEventDeauthTxDone(IN P_ADAPTER_T prAdapter,
	IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(P2P, INFO, "Deauth TX Done Status: %d, seqNo %d, staRecIdx: %d\n",
			rTxDoneStatus, prMsduInfo->ucTxSeqNum, prMsduInfo->ucStaRecIndex);

		prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

		if (prStaRec == NULL) {
			DBGLOG(P2P, TRACE, "Station Record NULL, Index:%d\n", prMsduInfo->ucStaRecIndex);
			break;
		}

		p2pFsmDeauthComplete(prAdapter, prStaRec, rTxDoneStatus);
		DBGLOG(P2P, INFO, "stop deauth timer for %pM\n", prStaRec->aucMacAddr);
		/* Avoid re-entry */
		cnmTimerStopTimer(prAdapter, &(prStaRec->rDeauthTxDoneTimer));

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;
}				/* p2pFsmRunEventDeauthTxDone */

WLAN_STATUS
p2pFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter,
			      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo = (P_P2P_MGMT_TX_REQ_INFO_T) NULL;
	BOOLEAN fgIsSuccess = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prMgmtTxReqInfo = &(prP2pFsmInfo->rMgmtTxInfo);

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			fgIsSuccess = TRUE;

		DBGLOG(P2P, INFO, "Mgmt Frame : Status: %d, seq NO. %d, Cookie: 0x%llx\n",
				rTxDoneStatus, prMsduInfo->ucTxSeqNum, prMsduInfo->u8Cookie);


		if (prMgmtTxReqInfo->prMgmtTxMsdu == prMsduInfo) {
			kalP2PIndicateMgmtTxStatus(prAdapter->prGlueInfo,
						   prMsduInfo->u8Cookie,
						   fgIsSuccess,
						   prMsduInfo->prPacket, (UINT_32) prMsduInfo->u2FrameLength);

			prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
		}
		/*
		 * wake up supplicant if it is waiting for tx done
		 */
		complete(&prAdapter->prGlueInfo->rP2pReq);
	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* p2pFsmRunEventMgmtFrameTxDone */

#if CFG_SUPPORT_P2P_ECSA
WLAN_STATUS
p2pFsmRunEventMgmtEcsaTxDone(IN P_ADAPTER_T prAdapter,
			      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(P2P, INFO, "Mgmt ECSA Frame TX Fail, Status: %d, seq NO. %d\n",
				rTxDoneStatus, prMsduInfo->ucTxSeqNum);
		} else {
			DBGLOG(P2P, INFO, "Mgmt ECSA Frame TX Done.\n");
		}

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is called when JOIN complete message event is received from SAA.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_JOIN_INFO_T prJoinInfo = (P_P2P_JOIN_INFO_T) NULL;
	P_MSG_JOIN_COMP_T prJoinCompMsg = (P_MSG_JOIN_COMP_T) NULL;
	P_SW_RFB_T prAssocRspSwRfb = (P_SW_RFB_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));
	DBGLOG(P2P, INFO, "P2P Join Complete\n");

	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	if (prP2pFsmInfo == NULL) {
		if (prMsgHdr)
			cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prJoinInfo = &(prP2pFsmInfo->rJoinInfo);
	if (prMsgHdr == NULL)
		return;
	prJoinCompMsg = (P_MSG_JOIN_COMP_T) prMsgHdr;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
	prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

	if (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
		P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

		prStaRec = prJoinCompMsg->prStaRec;

		/* Check SEQ NUM */
		if (prJoinCompMsg->ucSeqNum == ((prJoinInfo->ucSeqNumOfReqMsg)%256)) {
			ASSERT(prStaRec == prJoinInfo->prTargetStaRec);
			prJoinInfo->fgIsJoinComplete = TRUE;

			if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {

				/* 4 <1.1> Change FW's Media State immediately. */
				p2pChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
				if ((prP2pBssInfo->prStaRecOfAP) && (prP2pBssInfo->prStaRecOfAP != prStaRec)) {
					cnmStaRecChangeState(prAdapter, prP2pBssInfo->prStaRecOfAP,
							     STA_STATE_1);

					cnmStaRecFree(prAdapter, prP2pBssInfo->prStaRecOfAP, TRUE);

					prP2pBssInfo->prStaRecOfAP = NULL;
				}
				/* 4 <1.3> Update BSS_INFO_T */
				p2pFuncUpdateBssInfoForJOIN(prAdapter, prP2pFsmInfo->prTargetBss, prStaRec,
							    prAssocRspSwRfb);

				/* 4 <1.4> Activate current AP's STA_RECORD_T in Driver. */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				/* fire the update jiffies */
				prAdapter->prGlueInfo->ulLastUpdate = jiffies;
				DBGLOG(P2P, INFO, "P2P GC Join Success\n");

				/* reset add key action */
				prP2pBssInfo->eKeyAction = SEC_TX_KEY_COMMAND;

#if CFG_SUPPORT_P2P_RSSI_QUERY
				/* <1.5> Update RSSI if necessary */
				nicUpdateRSSI(prAdapter, NETWORK_TYPE_P2P_INDEX,
					      (INT_8) (RCPI_TO_dBm(prStaRec->ucRCPI)), 0);
#endif

				/* 4 <1.6> Indicate Connected Event to Host immediately. */
				/* Require BSSID, Association ID, Beacon Interval.. from AIS_BSS_INFO_T */
				/* p2pIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, */
				/* prStaRec->aucMacAddr); */
				if (prP2pFsmInfo->prTargetBss)
					scanReportBss2Cfg80211(prAdapter, BSS_TYPE_P2P_DEVICE,
							       prP2pFsmInfo->prTargetBss);
				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
								 &prP2pFsmInfo->rConnReqInfo,
								 prJoinInfo->aucIEBuf, prJoinInfo->u4BufLength,
								 prStaRec->u2StatusCode,
								 WLAN_STATUS_MEDIA_CONNECT);

			} else {
				/* Join Fail */
				/* 4 <2.1> Redo JOIN process with other Auth Type if possible */
				if (p2pFuncRetryJOIN(prAdapter, prStaRec, prJoinInfo) == FALSE) {
					P_BSS_DESC_T prBssDesc;

					/* Increase Failure Count */
					prStaRec->ucJoinFailureCount++;

					prBssDesc = prP2pFsmInfo->prTargetBss;

					ASSERT(prBssDesc);
					ASSERT(prBssDesc->fgIsConnecting);

					prBssDesc->fgIsConnecting = FALSE;

					if (prStaRec->ucJoinFailureCount >= 3) {

						kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
										 &prP2pFsmInfo->rConnReqInfo,
										 prJoinInfo->aucIEBuf,
										 prJoinInfo->u4BufLength,
										 prStaRec->u2StatusCode,
										 WLAN_STATUS_MEDIA_CONNECT);
					} else {
						/* Sometime the GO is not ready to response auth. */
						/* Connect it again */
						prP2pFsmInfo->prTargetBss = NULL;
					}
					DBGLOG(P2P, INFO, "P2P GC Join Failed\n");

				}

			}
		}
	}

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	if (prP2pFsmInfo->eCurrentState == P2P_STATE_GC_JOIN) {

		if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].eConnectionState ==
		    PARAM_MEDIA_STATE_CONNECTED) {
			/* do nothing & wait for timeout or EAPOL 4/4 TX done */
		} else {
			/* p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE); */
			/* one more scan */
			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_SCAN);
		}
	}

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventJoinComplete */

VOID p2pFsmRunEventMgmtFrameRegister(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_P2P_MGMT_FRAME_REGISTER_T prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) prMsgHdr;

		p2pFuncMgmtFrameRegister(prAdapter,
					 prMgmtFrameRegister->u2FrameType,
					 prMgmtFrameRegister->fgIsRegister, &prP2pFsmInfo->u4P2pPacketFilter);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;

}				/* p2pFsmRunEventMgmtFrameRegister */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is call when RX deauthentication frame from the AIR.
*             If we are under STA mode, we would go back to P2P Device.
*             If we are under AP mode, we would stay in AP mode until disconnect event from HOST.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventRxDeauthentication(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb)
{

	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	UINT_16 u2ReasonCode = 0;
	BOOLEAN fgSendDeauth = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		if (prStaRec == NULL)
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		if (!prStaRec)
			break;

		prP2pBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX];

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
								 NULL,
								 prDeauthFrame->aucInfoElem, u2IELength,
								 u2ReasonCode,
								 WLAN_STATUS_MEDIA_DISCONNECT);

				prP2pBssInfo->prStaRecOfAP = NULL;
				DBGLOG(P2P, INFO, "GC RX Deauth Reason: %d\n", u2ReasonCode);

				p2pFuncDisconnect(prAdapter, prStaRec, FALSE, u2ReasonCode);
				p2pFuncDeauthComplete(prAdapter, prP2pBssInfo);

				SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
			}
			break;
		case OP_MODE_ACCESS_POINT:
			/* Delete client from client list. */
			if (authProcessRxDeauthFrame(prSwRfb,
						     prP2pBssInfo->aucBSSID, &u2ReasonCode) == WLAN_STATUS_SUCCESS) {
				P_LINK_T prStaRecOfClientList = (P_LINK_T) NULL;
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;
				P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;

#if CFG_SUPPORT_802_11W
				/* AP PMF */
				if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
					P_HIF_RX_HEADER_T prHifRxHdr = prSwRfb->prHifRxHdr;

					if (prHifRxHdr->ucReserved & CONTROL_FLAG_UC_MGMT_NO_ENC) {
						/* if cipher mismatch, or incorrect encrypt, just drop */
						DBGLOG(P2P, ERROR, "Rx deauth CM/CLM=1\n");
						return;
					}

					/* 4.3.3.1 send unprotected deauth reason 6/7 */
					DBGLOG(P2P, INFO, "deauth reason=6\n");
					fgSendDeauth = TRUE;
					u2ReasonCode = REASON_CODE_CLASS_2_ERR;
					prStaRec->rPmfCfg.fgRxDeauthResp = TRUE;
				}
#endif

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
						DBGLOG(P2P, INFO, "GO RX Deauth Reason: %d\n", u2ReasonCode);
						p2pFuncDisconnect(prAdapter, prStaRec, fgSendDeauth, u2ReasonCode);

						break;
					}
				}
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
}				/* p2pFsmRunEventRxDeauthentication */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is call when RX deauthentication frame from the AIR.
*             If we are under STA mode, we would go back to P2P Device.
*             If we are under AP mode, we would stay in AP mode until disconnect event from HOST.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventRxDisassociation(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	UINT_16 u2ReasonCode = 0;
	BOOLEAN fgSendDeauth = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));
		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		if (prStaRec == NULL) {
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
			if (prStaRec == NULL)
				break;
		}

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

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
								 NULL,
								 prDisassocFrame->aucInfoElem,
								 u2IELength, prStaRec->u2ReasonCode,
								 WLAN_STATUS_MEDIA_DISCONNECT);

				prP2pBssInfo->prStaRecOfAP = NULL;

				DBGLOG(P2P, INFO, "GC RX Disassoc Reason %d\n", prStaRec->u2ReasonCode);
				p2pFuncDisconnect(prAdapter, prStaRec, FALSE, prStaRec->u2ReasonCode);
				p2pFuncDeauthComplete(prAdapter, prP2pBssInfo);
				SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);

			}
			break;
		case OP_MODE_ACCESS_POINT:
			/* Delete client from client list. */
			if (assocProcessRxDisassocFrame(prAdapter,
							prSwRfb,
							prP2pBssInfo->aucBSSID, &u2ReasonCode) == WLAN_STATUS_SUCCESS) {
				P_LINK_T prStaRecOfClientList = (P_LINK_T) NULL;
				P_LINK_ENTRY_T prLinkEntry = (P_LINK_ENTRY_T) NULL;
				P_STA_RECORD_T prCurrStaRec = (P_STA_RECORD_T) NULL;

#if CFG_SUPPORT_802_11W
				/* AP PMF */
				if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
					P_HIF_RX_HEADER_T prHifRxHdr = prSwRfb->prHifRxHdr;

					if (prHifRxHdr->ucReserved & CONTROL_FLAG_UC_MGMT_NO_ENC) {
						/* if cipher mismatch, or incorrect encrypt, just drop */
						DBGLOG(P2P, ERROR, "Rx disassoc CM/CLM=1\n");
						return;
					}

					/* 4.3.3.1 send unprotected deauth reason 6/7 */
					DBGLOG(P2P, INFO, "deauth reason=6\n");
					fgSendDeauth = TRUE;
					u2ReasonCode = REASON_CODE_CLASS_2_ERR;
					prStaRec->rPmfCfg.fgRxDeauthResp = TRUE;
				}
#endif

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
						DBGLOG(P2P, INFO, "GO RX Disassoc Reason %d\n", u2ReasonCode);
						p2pFuncDisconnect(prAdapter, prStaRec, fgSendDeauth, u2ReasonCode);

						break;
					}
				}
			}
			break;
		case OP_MODE_P2P_DEVICE:
		default:
			ASSERT(FALSE);
			break;
		}

	} while (FALSE);
}				/* p2pFsmRunEventRxDisassociation */

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is called when a probe request frame is received.
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return boolean value if probe response frame is accepted & need cancel scan request.
*/
/*----------------------------------------------------------------------------*/
VOID p2pFsmRunEventRxProbeResponseFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_BSS_DESC_T prBssDesc)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T) NULL;
	P_WLAN_MAC_MGMT_HEADER_T prMgtHdr = (P_WLAN_MAC_MGMT_HEADER_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL) && (prBssDesc != NULL));

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pConnSettings = prAdapter->rWifiVar.prP2PConnSettings;
		prP2pBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX];

		/* There is a connection request. */
		prMgtHdr = (P_WLAN_MAC_MGMT_HEADER_T) prSwRfb->pvHeader;

	} while (FALSE);

}				/* p2pFsmRunEventRxProbeResponseFrame */

VOID p2pFsmRunEventBeaconTimeout(IN P_ADAPTER_T prAdapter)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventBeaconTimeout: Beacon Timeout\n");

		/* Only client mode would have beacon lost event. */
		ASSERT(prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE);

		if (prP2pBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
							 NULL, NULL, 0, REASON_CODE_DEAUTH_LEAVING_BSS,
							 WLAN_STATUS_MEDIA_DISCONNECT);
			if (prP2pBssInfo->prStaRecOfAP != NULL) {
				P_STA_RECORD_T prStaRec = prP2pBssInfo->prStaRecOfAP;

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter, prStaRec, FALSE, REASON_CODE_DISASSOC_LEAVING_BSS);

				/* 20120830 moved into p2pFuncDisconnect() */
				/* cnmStaRecFree(prAdapter, prP2pBssInfo->prStaRecOfAP, TRUE); */
				p2pChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);
				SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);

			}
		}
	} while (FALSE);

}				/* p2pFsmRunEventBeaconTimeout */

VOID p2pFsmRunEventExtendListen(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = NULL;
	struct _MSG_P2P_EXTEND_LISTEN_INTERVAL_T *prExtListenMsg = NULL;

	ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

	if (prMsgHdr == NULL)  /* for coverity issue */
		return;

	prExtListenMsg = (struct _MSG_P2P_EXTEND_LISTEN_INTERVAL_T *) prMsgHdr;

	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	ASSERT_BREAK(prP2pFsmInfo);

	if (!prExtListenMsg->wait) {
		DBGLOG(P2P, TRACE, "reset listen interval\n");
		prP2pFsmInfo->eListenExted = P2P_DEV_NOT_EXT_LISTEN;
		if (prMsgHdr)
			cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	if (prP2pFsmInfo && (prP2pFsmInfo->eListenExted == P2P_DEV_NOT_EXT_LISTEN)) {
		DBGLOG(P2P, TRACE, "try to ext listen, p2p state: %d\n", prP2pFsmInfo->eCurrentState);
		if (prP2pFsmInfo->eCurrentState == P2P_STATE_CHNL_ON_HAND) {
			DBGLOG(P2P, TRACE, "here to ext listen interval\n");
			prP2pFsmInfo->eListenExted = P2P_DEV_EXT_LISTEN_ING;
		}
	}
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pFsmRunEventUpdateMgmtFrame */
#if CFG_SUPPORT_P2P_ECSA
VOID p2pFsmRunEventSendCSA(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	MSDU_INFO_T *prMsduInfoMgmtCSA;

	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_ECSA_T prMsgCSA = NULL;

	UINT_8 aucBcMac[] = BC_MAC_ADDR;

	if (prMsgHdr == NULL)
		return;
	prMsgCSA = (P_MSG_P2P_ECSA_T)prMsgHdr;

	prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

	prMsduInfoMgmtCSA = (MSDU_INFO_T *)
		cnmMgtPktAlloc(prAdapter, PUBLIC_ACTION_MAX_LEN);

	if (prMsduInfoMgmtCSA == NULL) {
		DBGLOG(P2P, ERROR, "<ECSA> %s: allocate mgmt pkt fail\n", __func__);
		return;
	}

	rlmGenActionCSHdr((u8 *)prMsduInfoMgmtCSA->prPacket,
			aucBcMac,
			prP2pBssInfo->aucBSSID,
			prP2pBssInfo->aucBSSID, CATEGORY_SPEC_MGT, 4);


	rlmGenActionCSA((u8 *)prMsduInfoMgmtCSA->prPacket,
			prMsgCSA->rP2pECSA.mode,
			prMsgCSA->rP2pECSA.channel,
			prMsgCSA->rP2pECSA.count,
			prMsgCSA->rP2pECSA.sco);

	prMsduInfoMgmtCSA->eSrc = TX_PACKET_MGMT;
	prMsduInfoMgmtCSA->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;
	prMsduInfoMgmtCSA->ucStaRecIndex = STA_REC_INDEX_BMCAST;
	prMsduInfoMgmtCSA->ucNetworkType = NETWORK_TYPE_P2P_INDEX;
	prMsduInfoMgmtCSA->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfoMgmtCSA->fgIs802_1x = FALSE;
	prMsduInfoMgmtCSA->fgIs802_11 = TRUE;
	prMsduInfoMgmtCSA->u2FrameLength = 34; /* header len + payload */
	prMsduInfoMgmtCSA->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfoMgmtCSA->pfTxDoneHandler = p2pFsmRunEventMgmtEcsaTxDone;
	prMsduInfoMgmtCSA->fgIsBasicRate = TRUE;	/* use basic rate */

	/* Send them to HW queue */
	nicTxEnqueueMsdu(prAdapter, prMsduInfoMgmtCSA);
	cnmMemFree(prAdapter, prMsgHdr);
}


VOID p2pFsmRunEventSendECSA(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	MSDU_INFO_T *prMsduInfoMgmtECSA;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_ECSA_T prMsgECSA = NULL;

	UINT_8 aucBcMac[] = BC_MAC_ADDR;

	if (prMsgHdr == NULL)
		return;
	prMsgECSA = (P_MSG_P2P_ECSA_T)prMsgHdr;

	prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

	prMsduInfoMgmtECSA = (MSDU_INFO_T *)
		cnmMgtPktAlloc(prAdapter, PUBLIC_ACTION_MAX_LEN);

	if (prMsduInfoMgmtECSA == NULL) {
		DBGLOG(P2P, ERROR, "<ECSA> %s: allocate mgmt pkt fail\n", __func__);
		return;
	}

	rlmGenActionCSHdr((u8 *)prMsduInfoMgmtECSA->prPacket,
			aucBcMac,
			prP2pBssInfo->aucBSSID,
			prP2pBssInfo->aucBSSID, CATEGORY_PUBLIC_ACTION, 4);

	rlmGenActionECSA((u8 *)prMsduInfoMgmtECSA->prPacket,
			prMsgECSA->rP2pECSA.mode,
			prMsgECSA->rP2pECSA.channel,
			prMsgECSA->rP2pECSA.count,
			prMsgECSA->rP2pECSA.op_class);

	prP2pBssInfo->ucOpClass = prMsgECSA->rP2pECSA.op_class;
	prP2pBssInfo->ucSwitchCount = prMsgECSA->rP2pECSA.count;
	prP2pBssInfo->ucSwitchMode = prMsgECSA->rP2pECSA.mode;
	prP2pBssInfo->ucEcsaChannel = prMsgECSA->rP2pECSA.channel;
	prP2pBssInfo->fgChanSwitching = TRUE;

	prMsduInfoMgmtECSA->eSrc = TX_PACKET_MGMT;
	prMsduInfoMgmtECSA->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;
	prMsduInfoMgmtECSA->ucStaRecIndex = STA_REC_INDEX_BMCAST;
	prMsduInfoMgmtECSA->ucNetworkType = NETWORK_TYPE_P2P_INDEX;
	prMsduInfoMgmtECSA->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfoMgmtECSA->fgIs802_1x = FALSE;
	prMsduInfoMgmtECSA->fgIs802_11 = TRUE;
	prMsduInfoMgmtECSA->u2FrameLength = 30; /* header len + payload */
	prMsduInfoMgmtECSA->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
	prMsduInfoMgmtECSA->pfTxDoneHandler = p2pFsmRunEventMgmtEcsaTxDone;
	prMsduInfoMgmtECSA->fgIsBasicRate = TRUE;	/* use basic rate */

	/* Send them to HW queue */
	nicTxEnqueueMsdu(prAdapter, prMsduInfoMgmtECSA);
	cnmMemFree(prAdapter, prMsgHdr);
}
#endif

#if CFG_SUPPORT_WFD
VOID p2pFsmRunEventWfdSettingUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
	P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T prMsgWfdCfgSettings = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) NULL;
	WLAN_STATUS rStatus;

	DBGLOG(P2P, INFO, "p2pFsmRunEventWfdSettingUpdate\n");

	do {
		ASSERT_BREAK((prAdapter != NULL));

		if (prMsgHdr != NULL) {
			prMsgWfdCfgSettings = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) prMsgHdr;
			prWfdCfgSettings = prMsgWfdCfgSettings->prWfdCfgSettings;
		} else {
			prWfdCfgSettings = &prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings;
		}

		DBGLOG(P2P, INFO, "WFD Enalbe %x info %x state %x flag %x adv %x\n",
				   prWfdCfgSettings->ucWfdEnable,
				   prWfdCfgSettings->u2WfdDevInfo,
				   (UINT_32) prWfdCfgSettings->u4WfdState,
				   (UINT_32) prWfdCfgSettings->u4WfdFlag,
				   (UINT_32) prWfdCfgSettings->u4WfdAdvancedFlag);

		if (prWfdCfgSettings->ucWfdEnable == 0)
			prAdapter->prGlueInfo->prP2PInfo->u2WFDIELen = 0;

		rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
					      CMD_ID_SET_WFD_CTRL,	/* ucCID */
					      TRUE,	/* fgSetQuery */
					      FALSE,	/* fgNeedResp */
					      FALSE,	/* fgIsOid */
					      NULL, NULL,	/* pfCmdTimeoutHandler */
					      sizeof(WFD_CFG_SETTINGS_T),	/* u4SetQueryInfoLen */
					      (PUINT_8) prWfdCfgSettings,	/* pucInfoBuffer */
					      NULL,	/* pvSetQueryBuffer */
					      0	/* u4SetQueryBufferLen */
		    );

	} while (FALSE);

	return;

}

/* p2pFsmRunEventWfdSettingUpdate */

#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to generate P2P IE for Beacon frame.
*
* @param[in] prMsduInfo             Pointer to the composed MSDU_INFO_T.
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID p2pGenerateP2P_IEForAssocReq(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

		if (prStaRec != NULL) {
			if (IS_STA_P2P_TYPE(prStaRec)) {
				/* Do nothing */
				/* TODO: */
			}
		}

	} while (FALSE);

	return;

}				/* end of p2pGenerateP2P_IEForAssocReq() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to generate P2P IE for Probe Request frame.
*
* @param[in] prMsduInfo             Pointer to the composed MSDU_INFO_T.
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID
p2pGenerateP2P_IEForProbeReq(IN P_ADAPTER_T prAdapter, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN UINT_16 u2BufSize)
{
	ASSERT(prAdapter);
	ASSERT(pucBuf);

	/* TODO: */

	return;

}				/* end of p2pGenerateP2P_IEForProbReq() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to calculate P2P IE length for Beacon frame.
*
* @param[in] eNetTypeIndex      Specify which network
* @param[in] prStaRec           Pointer to the STA_RECORD_T
*
* @return The length of P2P IE added
*/
/*----------------------------------------------------------------------------*/
UINT_32
p2pCalculateP2P_IELenForProbeReq(IN P_ADAPTER_T prAdapter,
				 IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex, IN P_STA_RECORD_T prStaRec)
{

	if (eNetTypeIndex != NETWORK_TYPE_P2P_INDEX)
		return 0;
	/* TODO: */

	return 0;

}				/* end of p2pCalculateP2P_IELenForProbeReq() */

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
VOID p2pRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_BSS_INFO_T prBssInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex]);

	bssRemoveStaRecFromClientList(prAdapter, prBssInfo, prStaRec);

	p2pFuncDisconnect(prAdapter, prStaRec, FALSE, REASON_CODE_UNSPECIFIED);

	/* 20120830 moved into p2puUncDisconnect. */
	/* cnmStaRecFree(prAdapter, prStaRec, TRUE); */

}				/* p2pRunEventAAATxFail */

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
WLAN_STATUS p2pRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	ENUM_PARAM_MEDIA_STATE_T eOriMediaState;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prStaRec != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		eOriMediaState = prP2pBssInfo->eConnectionState;

		bssAddStaRecToClientList(prAdapter, prP2pBssInfo, prStaRec);

		prStaRec->u2AssocId = bssAssignAssocID(prStaRec);

		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

		p2pChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

		/* Update Connected state to FW. */
		if (eOriMediaState != prP2pBssInfo->eConnectionState)
			nicUpdateBss(prAdapter, NETWORK_TYPE_P2P_INDEX);

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
WLAN_STATUS p2pRunEventAAASuccess(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
#if CFG_SUPPORT_P2P_EAP_FAIL_WORKAROUND
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	ASSERT((prAdapter != NULL));
	prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
	prP2pBssInfo->fgP2PPendingDeauth = FALSE;
	prP2pBssInfo->u4P2PEapTxDoneTime = 0;
#endif
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prStaRec != NULL));

		/* Glue layer indication. */
		kalP2PGOStationUpdate(prAdapter->prGlueInfo, prStaRec, TRUE);

	} while (FALSE);

	return rStatus;
}				/* p2pRunEventAAASuccess */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS p2pRxPublicActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_P2P_PUBLIC_ACTION_FRAME_T prPublicActionFrame = (P_P2P_PUBLIC_ACTION_FRAME_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;

	ASSERT(prSwRfb);
	ASSERT(prAdapter);

	prPublicActionFrame = (P_P2P_PUBLIC_ACTION_FRAME_T) prSwRfb->pvHeader;
	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

	DBGLOG(P2P, TRACE, "RX Public Action Frame Token:%d.\n", prPublicActionFrame->ucDialogToken);

	if (prPublicActionFrame->ucCategory != CATEGORY_PUBLIC_ACTION)
		return rWlanStatus;

	switch (prPublicActionFrame->ucAction) {
	case ACTION_PUBLIC_WIFI_DIRECT:
		break;
	case ACTION_GAS_INITIAL_REQUEST:
	case ACTION_GAS_INITIAL_RESPONSE:
	case ACTION_GAS_COMEBACK_REQUEST:
	case ACTION_GAS_COMEBACK_RESPONSE:
		break;
	default:
		break;
	}

	return rWlanStatus;
}				/* p2pRxPublicActionFrame */

WLAN_STATUS p2pRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_P2P_ACTION_FRAME_T prP2pActionFrame = (P_P2P_ACTION_FRAME_T) NULL;
	UINT_8 aucOui[3] = VENDOR_OUI_WFA_SPECIFIC;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		prP2pActionFrame = (P_P2P_ACTION_FRAME_T) prSwRfb->pvHeader;

		if (prP2pActionFrame->ucCategory != CATEGORY_VENDOR_SPECIFIC_ACTION) {
			DBGLOG(P2P, TRACE, "RX Action Frame but not vendor specific.\n");
			break;
		}

		if ((prP2pActionFrame->ucOuiType != VENDOR_OUI_TYPE_P2P) ||
		    (prP2pActionFrame->aucOui[0] != aucOui[0]) ||
		    (prP2pActionFrame->aucOui[1] != aucOui[1]) || (prP2pActionFrame->aucOui[2] != aucOui[2])) {
			DBGLOG(P2P, TRACE, "RX Vendor Specific Action Frame but not P2P Type or not WFA OUI.\n");
			break;
		}

	} while (FALSE);

	return rWlanStatus;
}				/* p2pRxActionFrame */
#if CFG_SUPPORT_P2P_ECSA
WLAN_STATUS p2pUpdateBeaconEcsaIE(IN P_ADAPTER_T prAdapter, IN UINT_8 ucNetTypeIndex)
{
	if (!prAdapter)
		return WLAN_STATUS_FAILURE;

	return bssUpdateBeaconContent(prAdapter, ucNetTypeIndex);
}
#endif

VOID
p2pProcessEvent_UpdateNOAParam(IN P_ADAPTER_T prAdapter,
			       UINT_8 ucNetTypeIndex, P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam)
{
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo;
	UINT_32 i;
	BOOLEAN fgNoaAttrExisted = FALSE;

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[ucNetTypeIndex]);
	prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

	prP2pSpecificBssInfo->fgEnableOppPS = prEventUpdateNoaParam->fgEnableOppPS;
	prP2pSpecificBssInfo->u2CTWindow = prEventUpdateNoaParam->u2CTWindow;
	prP2pSpecificBssInfo->ucNoAIndex = prEventUpdateNoaParam->ucNoAIndex;
	prP2pSpecificBssInfo->ucNoATimingCount = prEventUpdateNoaParam->ucNoATimingCount;

	fgNoaAttrExisted |= prP2pSpecificBssInfo->fgEnableOppPS;

	DBGLOG(P2P, INFO, "Update NoA Count=%d.\n", prEventUpdateNoaParam->ucNoATimingCount);

	ASSERT(prP2pSpecificBssInfo->ucNoATimingCount <= P2P_MAXIMUM_NOA_COUNT);

	for (i = 0; i < prP2pSpecificBssInfo->ucNoATimingCount; i++) {
		/* in used */
		prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse = prEventUpdateNoaParam->arEventNoaTiming[i].fgIsInUse;
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
	bssUpdateBeaconContent(prAdapter, ucNetTypeIndex);
}

VOID p2pFsmNotifyTxStatus(IN P_ADAPTER_T prAdapter, UINT_8 *pucEvtBuf)
{
	EVENT_TX_DONE_STATUS_T *prTxDone;
	P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	PUINT_8 pucPkt;

	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	prTxDone = (EVENT_TX_DONE_STATUS_T *) pucEvtBuf;
	pucPkt = &prTxDone->aucPktBuf[64];

	if (prBssInfo == NULL || prP2pFsmInfo == NULL || prTxDone == NULL ||
			pucPkt == NULL)
		return;

	if (prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED ||
			prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;

	if (prP2pFsmInfo->eCurrentState != P2P_STATE_GC_JOIN)
		return;

	if (secGetEapolKeyType(pucPkt) == EAPOL_KEY_4_OF_4 &&
			prTxDone->ucStatus == 0) {
		/* Finish GC connection process. */
		p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
	}
}

VOID p2pFsmNotifyRxP2pActionFrame(IN P_ADAPTER_T prAdapter,
		IN enum P2P_ACTION_FRAME_TYPE eP2pFrameType,
		OUT u_int8_t *prFgBufferFrame)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

	if (prP2pFsmInfo->eCurrentState != P2P_STATE_CHNL_ON_HAND &&
			prP2pBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED) {
		switch (eP2pFrameType) {
		case P2P_INVITATION_REQ:
			*prFgBufferFrame = TRUE;
			break;
		default:
			break;
		}
		return;
	}

	if (prP2pFsmInfo->eCNNState != P2P_CNN_NORMAL)
		return;

	switch (eP2pFrameType) {
	case P2P_GO_NEG_REQ:
	case P2P_GO_NEG_RESP:
	case P2P_INVITATION_REQ:
	case P2P_DEV_DISC_REQ:
	case P2P_PROV_DISC_REQ:
		DBGLOG(P2P, INFO, "Extend channel duration, p2pFrameType: %d.\n", eP2pFrameType);
		prP2pFsmInfo->eCNNState = eP2pFrameType + 1;
		break;
	default:
		break;
	}
}

static void initAcsParams(IN P_ADAPTER_T prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo) {
	P_RF_CHANNEL_INFO_T prRfChannelInfo;
	uint8_t i;

	if (!prAdapter || !prMsgAcsRequest || !prAcsReqInfo)
		return;

	prAcsReqInfo->fgIsProcessing = TRUE;
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

	DBGLOG(P2P, INFO, "ht=%d, ht40=%d, vht=%d, bw=%d, mode=%d",
			prMsgAcsRequest->fgIsHtEnable,
			prMsgAcsRequest->fgIsHt40Enable,
			prMsgAcsRequest->fgIsVhtEnable,
			prMsgAcsRequest->eChnlBw,
			prMsgAcsRequest->eHwMode);
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

static void trimAcsScanList(IN P_ADAPTER_T prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo,
		IN ENUM_BAND_T eBand)
{
	uint32_t u4NumChannel = 0;
	uint8_t i;
	P_RF_CHANNEL_INFO_T prRfChannelInfo1;
	P_RF_CHANNEL_INFO_T prRfChannelInfo2;

	if (!prAdapter || !prAcsReqInfo)
		return;

	for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
		prRfChannelInfo1 =
				&(prMsgAcsRequest->arChannelListInfo[i]);
		if (eBand == prRfChannelInfo1->eBand) {
			prRfChannelInfo2 = &(prMsgAcsRequest->arChannelListInfo[
					u4NumChannel]);
			prRfChannelInfo2->eBand = prRfChannelInfo1->eBand;
			prRfChannelInfo2->ucChannelNum =
					prRfChannelInfo1->ucChannelNum;
			prRfChannelInfo2->eDFS = prRfChannelInfo1->eDFS;
			u4NumChannel++;
			DBGLOG(P2P, INFO, "acs trim scan list, [%d]=%d %d\n",
					u4NumChannel,
					prRfChannelInfo1->eBand,
					prRfChannelInfo2->ucChannelNum);
		}
		prRfChannelInfo1++;
	}
	prMsgAcsRequest->u4NumChannel = u4NumChannel;
}

static void initAcsChnlMask(IN P_ADAPTER_T prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		IN struct P2P_ACS_REQ_INFO *prAcsReqInfo)
{
	uint8_t i;
	P_RF_CHANNEL_INFO_T prRfChannelInfo;

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

void p2pFsmRunEventAcs(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest;
	P_P2P_FSM_INFO_T prP2pFsmInfo;
	P_MSG_P2P_SCAN_REQUEST_T prP2pScanReqMsg;
	struct P2P_ACS_REQ_INFO *prAcsReqInfo;
	uint32_t u4MsgSize = 0;

	if (!prAdapter || !prMsgHdr)
		return;

	prMsgAcsRequest = (struct MSG_P2P_ACS_REQUEST *) prMsgHdr;
	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	prAcsReqInfo = &prP2pFsmInfo->rAcsReqInfo;

	p2pFsmAbortCurrentAcsReq(prAdapter, prMsgAcsRequest);

	initAcsParams(prAdapter, prMsgAcsRequest, prAcsReqInfo);

	if (prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11ANY) {
		if (prAdapter->fgEnable5GBand) {
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

	if (prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11A) {
		p2pFunCalAcsChnScores(prAdapter,
				BAND_5G);
		p2pFunProcessAcsReport(prAdapter,
				NULL,
				prAcsReqInfo);
		goto exit;
	}

	u4MsgSize = sizeof(MSG_P2P_SCAN_REQUEST_T) + (
			prMsgAcsRequest->u4NumChannel *
				sizeof(RF_CHANNEL_INFO_T));

	prP2pScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, u4MsgSize);
	if (prP2pScanReqMsg == NULL) {
		DBGLOG(P2P, ERROR, "alloc scan req. fail\n");
		return;
	}
	kalMemSet(prP2pScanReqMsg, 0, u4MsgSize);
	prP2pScanReqMsg->i4SsidNum = 0;
	prP2pScanReqMsg->u4NumChannel = prMsgAcsRequest->u4NumChannel;
	prP2pScanReqMsg->u4IELen = 0;
	prP2pScanReqMsg->fgIsAcsReq = TRUE;
	kalMemCopy(&(prP2pScanReqMsg->arChannelListInfo),
			&(prMsgAcsRequest->arChannelListInfo),
			(prMsgAcsRequest->u4NumChannel *
				sizeof(RF_CHANNEL_INFO_T)));
	p2pFsmRunEventScanRequest(prAdapter, (P_MSG_HDR_T) prP2pScanReqMsg);

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

static u_int8_t p2pFsmIsAcsProcessing(IN P_ADAPTER_T prAdapter)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo;
	struct P2P_ACS_REQ_INFO *prAcsReqInfo;

	if (!prAdapter)
		return FALSE;

	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	if (!prP2pFsmInfo)
		return FALSE;

	prAcsReqInfo = &prP2pFsmInfo->rAcsReqInfo;
	if (!prAcsReqInfo)
		return FALSE;

	return prAcsReqInfo->fgIsProcessing;
}

static void
p2pFsmAbortCurrentAcsReq(IN P_ADAPTER_T prAdapter,
		IN struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo;
	P_P2P_SCAN_REQ_INFO_T prScanReqInfo = NULL;

	if (!prAdapter || !prMsgAcsRequest)
		return;

	prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
	prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

	if (!p2pFsmIsAcsProcessing(prAdapter))
		return;

	if (prP2pFsmInfo->eCurrentState == P2P_STATE_SCAN &&
			prScanReqInfo->fgIsAcsReq) {
		DBGLOG(P2P, INFO, "Cancel current ACS scan.\n");
		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);
	}
}

#endif /* CFG_ENABLE_WIFI_DIRECT */
