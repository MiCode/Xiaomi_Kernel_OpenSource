/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/mgmt/p2p_fsm.c#61
*/

/*! \file   "p2p_fsm.c"
 *  \brief  This file defines the FSM for P2P Module.
 *
 *  This file defines the FSM for P2P Module.
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

/* /////////////////////////////////MT6630 CODE ///////////////////////////////// */

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

		prP2pFsmInfo->eCurrentState = prP2pFsmInfo->ePreviousState = P2P_STATE_IDLE;
		prP2pFsmInfo->prTargetBss = NULL;

		cnmTimerInitTimer(prAdapter,
				  &(prP2pFsmInfo->rP2pFsmTimeoutTimer),
				  (PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventFsmTimeout, (ULONG) prP2pFsmInfo);

		/* 4 <2> Initiate BSS_INFO_T - common part */
		BSS_INFO_INIT(prAdapter, NETWORK_TYPE_P2P_INDEX);

		/* 4 <2.1> Initiate BSS_INFO_T - Setup HW ID */
		prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;

		prP2pBssInfo->ucNonHTBasicPhyType = (UINT_8)
		    rNonHTApModeAttributes[prP2pBssInfo->ucConfigAdHocAPMode].ePhyTypeIndex;
		prP2pBssInfo->u2BSSBasicRateSet =
		    rNonHTApModeAttributes[prP2pBssInfo->ucConfigAdHocAPMode].u2BSSBasicRateSet;

		prP2pBssInfo->u2OperationalRateSet =
		    rNonHTPhyAttributes[prP2pBssInfo->ucNonHTBasicPhyType].u2SupportedRateSet;

		rateGetDataRatesFromRateSet(prP2pBssInfo->u2OperationalRateSet,
					    prP2pBssInfo->u2BSSBasicRateSet,
					    prP2pBssInfo->aucAllSupportedRates, &prP2pBssInfo->ucAllSupportedRatesLen);

		nicTxUpdateBssDefaultRate(prP2pBssInfo);

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

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucQoS))
			prP2pBssInfo->fgIsQBSS = TRUE;
		else
			prP2pBssInfo->fgIsQBSS = FALSE;

		SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		/* wlanBindBssIdxToNetInterface(prAdapter->prGlueInfo, NET_DEV_P2P_IDX, prP2pBssInfo->ucBssIndex); */

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

}				/* end of p2pFsmUninit() */

VOID p2pFsmStateTransition(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState)
{
	BOOLEAN fgIsTransOut = (BOOLEAN) FALSE;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

		if (!IS_BSS_ACTIVE(prP2pBssInfo)) {
			if (!cnmP2PIsPermitted(prAdapter))
				return;

			SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);
			nicActivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX);
		}

		fgIsTransOut = fgIsTransOut ? FALSE : TRUE;

		if (!fgIsTransOut) {
			DBGLOG(P2P, STATE, "TRANSITION: [%s] -> [%s]\n",
					    apucDebugP2pState[prP2pFsmInfo->eCurrentState],
					    apucDebugP2pState[eNextState];

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
				/* p2pStateAbort_SCAN(prAdapter, prP2pFsmInfo, eNextState); */
			} else {
				/* Initial scan request. */
				/* p2pStateInit_SCAN(prAdapter, prP2pFsmInfo); */
			}

			break;
		case P2P_STATE_AP_CHANNEL_DETECT:
			if (fgIsTransOut) {
				/* Scan done */
				/* Get sparse channel result. */
				p2pStateAbort_AP_CHANNEL_DETECT(prAdapter,
								prP2pFsmInfo, prP2pSpecificBssInfo, eNextState);
			} else {
				/* Initial passive scan request. */
				/* p2pStateInit_AP_CHANNEL_DETECT(prAdapter, prP2pFsmInfo); */
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
				ASSERT(prP2pFsmInfo->prTargetBss != NULL);

				/* Send request to SAA module. */

				p2pStateInit_GC_JOIN(prAdapter,
						     prP2pFsmInfo,
						     prP2pBssInfo,
						     &(prP2pFsmInfo->rJoinInfo), prP2pFsmInfo->prTargetBss);
			}

			break;
		default:
			break;
		}
	} while (fgIsTransOut);
}				/* p2pFsmStateTransition */

#if 0

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

		/* Special case of time renewing for same frequency. */
		if ((prP2pFsmInfo->eCurrentState == P2P_STATE_CHNL_ON_HAND) &&
		    (prChnlReqInfo->ucReqChnlNum == prP2pChnlReqMsg->rChannelInfo.ucChannelNum) &&
		    (prChnlReqInfo->eBand == prP2pChnlReqMsg->rChannelInfo.eBand) &&
		    (prChnlReqInfo->eChnlSco == prP2pChnlReqMsg->eChnlSco)) {
			ASSERT(prChnlReqInfo->fgIsChannelRequested == TRUE);
			ASSERT(prChnlReqInfo->eChannelReqType == CHANNEL_REQ_TYPE_REMAIN_ON_CHANNEL);

			prChnlReqInfo->u8Cookie = prP2pChnlReqMsg->u8Cookie;
			prChnlReqInfo->u4MaxInterval = prP2pChnlReqMsg->u4Duration;

			/* Re-enter the state. */
			eNextState = P2P_STATE_CHNL_ON_HAND;
		} else {
			/* Make sure the state is in IDLE state. */
			p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

			prChnlReqInfo->u8Cookie = prP2pChnlReqMsg->u8Cookie;	/* Cookie can only be assign after
										 * abort.(for indication) */
			prChnlReqInfo->ucReqChnlNum = prP2pChnlReqMsg->rChannelInfo.ucChannelNum;
			prChnlReqInfo->eBand = prP2pChnlReqMsg->rChannelInfo.eBand;
			prChnlReqInfo->eChnlSco = prP2pChnlReqMsg->eChnlSco;
			prChnlReqInfo->u4MaxInterval = prP2pChnlReqMsg->u4Duration;
			prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_REMAIN_ON_CHANNEL;

			eNextState = P2P_STATE_REQING_CHANNEL;
		}

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

		if ((prChnlAbortMsg->u8Cookie == prChnlReqInfo->u8Cookie) && (prChnlReqInfo->fgIsChannelRequested)) {
			ASSERT((prP2pFsmInfo->eCurrentState == P2P_STATE_REQING_CHANNEL ||
				(prP2pFsmInfo->eCurrentState == P2P_STATE_CHNL_ON_HAND)));

			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventChannelAbort */

#endif

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
					    prScanReqInfo->ucNumChannelList, MAXIMUM_OPERATION_CHANNEL_LIST;
			prScanReqInfo->ucNumChannelList = MAXIMUM_OPERATION_CHANNEL_LIST;
		}

		u4ChnlListSize = sizeof(RF_CHANNEL_INFO_T) * prScanReqInfo->ucNumChannelList;
		kalMemCopy(prScanReqInfo->arScanChannelList, prP2pScanReqMsg->arChannelListInfo, u4ChnlListSize);

		/* TODO: I only take the first SSID. Multiple SSID may be needed in the future. */
		/* SSID */
#if 0
		if (prP2pScanReqMsg->i4SsidNum >= 1)
			kalMemCopy(&(prScanReqInfo->rSsidStruct), prP2pScanReqMsg->prSSID, sizeof(P2P_SSID_STRUCT_T));
		else
			prScanReqInfo->rSsidStruct.ucSsidLen = 0;
#endif

		/* IE Buffer */
		kalMemCopy(prScanReqInfo->aucIEBuf, prP2pScanReqMsg->pucIEBuf, prP2pScanReqMsg->u4IELen);

		prScanReqInfo->u4BufLength = prP2pScanReqMsg->u4IELen;

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

			cnmTimerStopTimer(prAdapter, &(prP2pFsmInfo->rP2pFsmTimeoutTimer));
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
VOID p2pFsmRunEventFsmTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) ulParamPtr;

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
			{
				p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_IDLE);
			}
			break;
/* case P2P_STATE_GC_JOIN: */
/* break; */
		default:
			break;
		}
	} while (FALSE);

}				/* p2pFsmRunEventFsmTimeout */

VOID p2pFsmRunEventStartAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_MSG_P2P_START_AP_T prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventStartAP\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) prMsgHdr;
		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

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
			ENUM_P2P_STATE_T eNextState = P2P_STATE_SCAN;
			P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = prAdapter->rWifiVar.prP2PConnSettings;

			if (prP2pFsmInfo->eCurrentState != P2P_STATE_SCAN &&
			    prP2pFsmInfo->eCurrentState != P2P_STATE_IDLE) {
				/* Make sure the state is in IDLE state. */
				p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);
			}
			/* 20120118: Moved to p2pFuncSwitchOPMode(). */
			/* SET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX); */

			/* Leave IDLE state. */
			SET_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);

			/* sync with firmware */
			/* DBGLOG(P2P, INFO, ("Activate P2P Network.\n")); */
			/* nicActivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX); */

			/* Key to trigger P2P FSM to allocate channel for AP mode. */
			prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;

			/* Sparse Channel to decide which channel to use. */
			if ((cnmPreferredChannel(prAdapter, &eBand, &ucPreferedChnl, &eSco) == FALSE)
			    && (prP2pConnSettings->ucOperatingChnl == 0)) {
				/* Sparse Channel Detection using passive mode. */
				eNextState = P2P_STATE_AP_CHANNEL_DETECT;
			} else {
				P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo =
				    prAdapter->rWifiVar.prP2pSpecificBssInfo;
				P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = &prP2pFsmInfo->rChnlReqInfo;

#if 1
				/* 2012-01-27: frog - Channel set from upper layer is the first priority. */
				/* Because the channel & beacon is decided by p2p_supplicant. */
				if (prP2pConnSettings->ucOperatingChnl != 0) {
					prP2pSpecificBssInfo->ucPreferredChannel = prP2pConnSettings->ucOperatingChnl;
					prP2pSpecificBssInfo->eRfBand = prP2pConnSettings->eBand;
				} else {
					ASSERT(ucPreferedChnl != 0);
					prP2pSpecificBssInfo->ucPreferredChannel = ucPreferedChnl;
					prP2pSpecificBssInfo->eRfBand = eBand;
				}
#else
				if (ucPreferedChnl) {
					prP2pSpecificBssInfo->ucPreferredChannel = ucPreferedChnl;
					prP2pSpecificBssInfo->eRfBand = eBand;
				} else {
					ASSERT(prP2pConnSettings->ucOperatingChnl != 0);
					prP2pSpecificBssInfo->ucPreferredChannel = prP2pConnSettings->ucOperatingChnl;
					prP2pSpecificBssInfo->eRfBand = prP2pConnSettings->eBand;
				}

#endif
				prChnlReqInfo->ucReqChnlNum = prP2pSpecificBssInfo->ucPreferredChannel;
				prChnlReqInfo->eBand = prP2pSpecificBssInfo->eRfBand;
				prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_GO_START_BSS;
			}

			/* If channel is specified, use active scan to shorten the scan time. */
			p2pFsmStateTransition(prAdapter, prAdapter->rWifiVar.prP2pFsmInfo, eNextState);
		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventStartAP */

VOID p2pFsmRunEventStopAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		DBGLOG(P2P, TRACE, "p2pFsmRunEventStopAP\n");

		if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
		    && (prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			/* AP is created, Beacon Update. */

			p2pFuncDissolve(prAdapter, prP2pBssInfo, TRUE, REASON_CODE_DEAUTH_LEAVING_BSS);

			DBGLOG(P2P, TRACE, "Stop Beaconing\n");
			nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_P2P_INDEX);

			/* Reset RLM related field of BSSINFO. */
			rlmBssAborted(prAdapter, prP2pBssInfo);
		}

		/* 20120118: Moved to p2pFuncSwitchOPMode(). */
		/* UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX); */

		/* Enter IDLE state. */
		SET_NET_PWR_STATE_IDLE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		DBGLOG(P2P, INFO, "Re activate P2P Network.\n");
		nicDeactivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX);

		nicActivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX);

		p2pFsmRunEventAbort(prAdapter, prAdapter->rWifiVar.prP2pFsmInfo);
/* p2pFsmStateTransition(prAdapter, prAdapter->rWifiVar.prP2pFsmInfo, P2P_STATE_IDLE); */
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

		/* Make sure the state is in IDLE state. */
		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		/* Update connection request information. */
		prConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GC;
		COPY_MAC_ADDR(prConnReqInfo->aucBssid, prConnReqMsg->aucBssid);
		kalMemCopy(&(prConnReqInfo->rSsidStruct), &(prConnReqMsg->rSsid), sizeof(P2P_SSID_STRUCT_T));
		kalMemCopy(prConnReqInfo->aucIEBuf, prConnReqMsg->aucIEBuf, prConnReqMsg->u4IELen);
		prConnReqInfo->u4BufLength = prConnReqMsg->u4IELen;

		/* Find BSS Descriptor first. */
		prP2pFsmInfo->prTargetBss = scanP2pSearchDesc(prAdapter, prConnReqInfo);

		if (prP2pFsmInfo->prTargetBss == NULL) {
			/* Update scan parameter... to scan target device. */
			P_P2P_SCAN_REQ_INFO_T prScanReqInfo = &(prP2pFsmInfo->rScanReqInfo);

			prScanReqInfo->ucNumChannelList = 1;
			prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
			prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;
			prScanReqInfo->arScanChannelList[0].ucChannelNum = prConnReqMsg->rChannelInfo.ucChannelNum;
			prScanReqInfo->ucSsidNum = 1;
			kalMemCopy(&(prScanReqInfo->arSsidStruct[0]), &(prConnReqMsg->rSsid),
				   sizeof(P2P_SSID_STRUCT_T));
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

			p2pFsmStateTransition(prAdapter, prP2pFsmInfo, P2P_STATE_REQING_CHANNEL);
		}
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

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

	/* P_STA_RECORD_T prTargetStaRec = (P_STA_RECORD_T)NULL; */

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));

		DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionAbort: Connection Abort.\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;

		if (prP2pFsmInfo == NULL)
			break;

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T) prMsgHdr;

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

				kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo, NULL, NULL, 0, 0);

				/* Stop rejoin timer if it is started. */
				/* TODO: If it has. */

				p2pFuncDisconnect(prAdapter, prP2pBssInfo->prStaRecOfAP,
						  prDisconnMsg->fgSendDeauth, prDisconnMsg->u2ReasonCode);

				/* prTargetStaRec = prP2pBssInfo->prStaRecOfAP; */

				/* Fix possible KE when RX Beacon & call nicPmIndicateBssConnected().
				 * hit prStaRecOfAP == NULL.
				 **/
				p2pChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

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
				       "Disconnecting with Target ID: " MACSTR "\n",
					MAC2STR(prDisconnMsg->aucTargetID));

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
						p2pFuncDisconnect(prAdapter, prCurrStaRec,
								  prDisconnMsg->fgSendDeauth,
								  prDisconnMsg->u2ReasonCode);

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
	/* cnmStaRecFree(prAdapter, prTargetStaRec); */
	/* } */

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pFsmRunEventConnectionAbort */

WLAN_STATUS
p2pFsmRunEventDeauthTxDone(IN P_ADAPTER_T prAdapter,
			   IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	ENUM_PARAM_MEDIA_STATE_T eOriMediaStatus;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(P2P, TRACE, "Deauth TX Done\n");

		prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

		if (prStaRec == NULL) {
			DBGLOG(P2P, TRACE, "Station Record NULL, Index:%d\n", prMsduInfo->ucStaRecIndex);
			break;
		}

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		eOriMediaStatus = prP2pBssInfo->eConnectionState;

		/* Change station state. */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

		/* Reset Station Record Status. */
		p2pFuncResetStaRecStatus(prAdapter, prStaRec);

		 /**/ cnmStaRecFree(prAdapter, prStaRec);

		if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
		    (prP2pBssInfo->rStaRecOfClientList.u4NumElem == 0)) {
			DBGLOG(P2P, TRACE, "No More Client, Media Status DISCONNECTED\n");
			p2pChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);
		}

		if (eOriMediaStatus != prP2pBssInfo->eConnectionState) {
			/* Update Disconnected state to FW. */
			nicUpdateBss(prAdapter, NETWORK_TYPE_P2P_INDEX);
		}
	} while (FALSE);

	return WLAN_STATUS_SUCCESS;
}				/* p2pFsmRunEventDeauthTxDone */

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
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	UINT_16 u2ReasonCode = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		if (prStaRec == NULL)
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

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
								 prDeauthFrame->aucInfoElem, u2IELength, u2ReasonCode);

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter, prStaRec, FALSE, u2ReasonCode);
			}
			break;
		case OP_MODE_ACCESS_POINT:
			/* Delete client from client list. */
			if (authProcessRxDeauthFrame(prSwRfb,
						     prP2pBssInfo->aucBSSID, &u2ReasonCode) == WLAN_STATUS_SUCCESS) {
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
						p2pFuncDisconnect(prAdapter, prStaRec, FALSE, u2ReasonCode);

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
	UINT_16 u2ReasonCode = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prSwRfb != NULL));

		if (prStaRec == NULL)
			prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

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
								 u2IELength, prStaRec->u2ReasonCode);

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter, prStaRec, FALSE, prStaRec->u2ReasonCode);
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
						p2pFuncDisconnect(prAdapter, prStaRec, FALSE, u2ReasonCode);

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

		if (IS_STA_IN_P2P(prStaRec)) {
			/* Do nothing */
			/* TODO: */
		}
	} while (FALSE);

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

#endif /* CFG_ENABLE_WIFI_DIRECT */
