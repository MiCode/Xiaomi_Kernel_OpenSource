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

#include "p2p_precomp.h"

BOOLEAN
p2pStateInit_IDLE(IN P_ADAPTER_T prAdapter,
		  IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN P_BSS_INFO_T prP2pBssInfo, OUT P_ENUM_P2P_STATE_T peNextState)
{
	BOOLEAN fgIsTransOut = FALSE;
/* P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T)NULL; */

	do {
		ASSERT_BREAK((prAdapter != NULL) &&
			     (prP2pFsmInfo != NULL) && (prP2pBssInfo != NULL) && (peNextState != NULL));

		if ((prP2pBssInfo->eIntendOPMode == OP_MODE_ACCESS_POINT)
		    && IS_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX)) {
			P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

			fgIsTransOut = TRUE;
			prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_GO_START_BSS;
			*peNextState = P2P_STATE_REQING_CHANNEL;

		} else {
#if 0
			else
		if (IS_NET_PWR_STATE_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX)) {

			ASSERT((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) ||
			       (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE));

			prChnlReqInfo = &prP2pFsmInfo->rChnlReqInfo;

			if (prChnlReqInfo->fgIsChannelRequested) {
				/* Start a timer for return channel. */
				DBGLOG(P2P, TRACE, "start a GO channel timer.\n");
			}

		}
#endif
			cnmTimerStartTimer(prAdapter, &(prP2pFsmInfo->rP2pFsmTimeoutTimer), 5000);
		}

	} while (FALSE);

	return fgIsTransOut;
}				/* p2pStateInit_IDLE */

VOID p2pStateAbort_IDLE(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState)
{

	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL));

		prChnlReqInfo = &prP2pFsmInfo->rChnlReqInfo;

		if (prChnlReqInfo->fgIsChannelRequested) {
			/* Release channel before timeout. */
			p2pFuncReleaseCh(prAdapter, prChnlReqInfo);
		}

		/* Stop timer for leaving this state. */
		cnmTimerStopTimer(prAdapter, &(prP2pFsmInfo->rP2pFsmTimeoutTimer));

	} while (FALSE);

}				/* p2pStateAbort_IDLE */

VOID p2pStateInit_CHNL_ON_HAND(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN P_P2P_FSM_INFO_T prP2pFsmInfo)
{
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL));

		prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

		/* Store the original channel info. */
		prChnlReqInfo->ucOriChnlNum = prP2pBssInfo->ucPrimaryChannel;
		prChnlReqInfo->eOriBand = prP2pBssInfo->eBand;
		prChnlReqInfo->eOriChnlSco = prP2pBssInfo->eBssSCO;

		/* RX Probe Request would check primary channel. */
		prP2pBssInfo->ucPrimaryChannel = prChnlReqInfo->ucReqChnlNum;
		prP2pBssInfo->eBand = prChnlReqInfo->eBand;
		prP2pBssInfo->eBssSCO = prChnlReqInfo->eChnlSco;

		DBGLOG(P2P, TRACE, "start a channel on hand timer.\n");
		cnmTimerStartTimer(prAdapter, &(prP2pFsmInfo->rP2pFsmTimeoutTimer), prChnlReqInfo->u4MaxInterval);

		kalP2PIndicateChannelReady(prAdapter->prGlueInfo,
					   prChnlReqInfo->u8Cookie,
					   prChnlReqInfo->ucReqChnlNum,
					   prChnlReqInfo->eBand, prChnlReqInfo->eChnlSco, prChnlReqInfo->u4MaxInterval);

	} while (FALSE);

}				/* p2pStateInit_CHNL_ON_HAND */

VOID
p2pStateAbort_CHNL_ON_HAND(IN P_ADAPTER_T prAdapter,
			   IN P_P2P_FSM_INFO_T prP2pFsmInfo,
			   IN P_BSS_INFO_T prP2pBssInfo, IN ENUM_P2P_STATE_T eNextState)
{
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL));

		prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

		cnmTimerStopTimer(prAdapter, &(prP2pFsmInfo->rP2pFsmTimeoutTimer));

		/* Restore the original channel info. */
		prP2pBssInfo->ucPrimaryChannel = prChnlReqInfo->ucOriChnlNum;
		prP2pBssInfo->eBand = prChnlReqInfo->eOriBand;
		prP2pBssInfo->eBssSCO = prChnlReqInfo->eOriChnlSco;
#if 0
		if (eNextState != P2P_STATE_CHNL_ON_HAND) {
			/* Indicate channel return. */
			kalP2PIndicateChannelExpired(prAdapter->prGlueInfo, &prP2pFsmInfo->rChnlReqInfo);

			/* Return Channel. */
			p2pFuncReleaseCh(prAdapter, &(prP2pFsmInfo->rChnlReqInfo));
		}
#endif
	} while (FALSE);
}				/* p2pStateAbort_CHNL_ON_HAND */

VOID
p2pStateAbort_REQING_CHANNEL(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState)
{
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;

	do {

		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL) && (eNextState < P2P_STATE_NUM));

		prP2pBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX]);
		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

		if (eNextState == P2P_STATE_IDLE) {
			if (prP2pBssInfo->eIntendOPMode == OP_MODE_ACCESS_POINT) {
				/* Intend to be AP. */
				/* Setup for AP mode. */
#if 0
				p2pFuncStartGO(prAdapter,
					       prP2pBssInfo,
					       prP2pSpecificBssInfo->aucGroupSsid,
					       prP2pSpecificBssInfo->u2GroupSsidLen,
					       prP2pSpecificBssInfo->ucPreferredChannel,
					       prP2pSpecificBssInfo->eRfBand,
					       prP2pSpecificBssInfo->eRfSco, prP2pFsmInfo->fgIsApMode);
#endif
			} else {
				/* Return Channel. */
				p2pFuncReleaseCh(prAdapter, &(prP2pFsmInfo->rChnlReqInfo));
			}

		}

	} while (FALSE);

}				/* p2pStateInit_AP_CHANNEL_DETECT */

VOID
p2pStateAbort_AP_CHANNEL_DETECT(IN P_ADAPTER_T prAdapter,
				IN P_P2P_FSM_INFO_T prP2pFsmInfo,
				IN P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo, IN ENUM_P2P_STATE_T eNextState)
{
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T) NULL;
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T) NULL;

	do {

		if (eNextState == P2P_STATE_REQING_CHANNEL) {
			UINT_8 ucPreferedChnl = 0;
			ENUM_BAND_T eBand = BAND_NULL;
			ENUM_CHNL_EXT_T eSco = CHNL_EXT_SCN;

			prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

			/* Determine the channel for AP. */
			if (cnmPreferredChannel(prAdapter, &eBand, &ucPreferedChnl, &eSco) == FALSE) {
#if 0
				prP2pConnSettings = prAdapter->rWifiVar.prP2PConnSettings;

				ucPreferedChnl = prP2pConnSettings->ucOperatingChnl;
				if ((ucPreferedChnl) == 0) {

					if (scnQuerySparseChannel(prAdapter, &eBand, &ucPreferedChnl) == FALSE) {

						/* What to do? */
						ASSERT(FALSE);
						/* TODO: Pick up a valid channel from channel list. */
					}
				}
#endif
			}

			prChnlReqInfo->eChannelReqType = CHANNEL_REQ_TYPE_GO_START_BSS;
			prChnlReqInfo->ucReqChnlNum = prP2pSpecificBssInfo->ucPreferredChannel = ucPreferedChnl;
			prChnlReqInfo->eBand = prP2pSpecificBssInfo->eRfBand = eBand;
			prChnlReqInfo->eChnlSco = prP2pSpecificBssInfo->eRfSco = eSco;
		} else {
			/* p2pFuncCancelScan(prAdapter, &(prP2pFsmInfo->rScanReqInfo)); */
		}

	} while (FALSE);

}				/* p2pStateAbort_AP_CHANNEL_DETECT */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process of JOIN Abort. Leave JOIN State & Abort JOIN.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
p2pStateAbort_GC_JOIN(IN P_ADAPTER_T prAdapter,
		      IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN P_P2P_JOIN_INFO_T prJoinInfo, IN ENUM_P2P_STATE_T eNextState)
{
	P_MSG_JOIN_ABORT_T prJoinAbortMsg = (P_MSG_JOIN_ABORT_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pFsmInfo != NULL) && (prJoinInfo != NULL));

		if (prJoinInfo->fgIsJoinComplete == FALSE) {

			prJoinAbortMsg =
			    (P_MSG_JOIN_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_ABORT_T));
			if (!prJoinAbortMsg) {
				DBGLOG(P2P, TRACE, "Fail to allocate join abort message buffer\n");
				ASSERT(FALSE);
				return;
			}

			prJoinAbortMsg->rMsgHdr.eMsgId = MID_P2P_SAA_FSM_ABORT;
			prJoinAbortMsg->ucSeqNum = prJoinInfo->ucSeqNumOfReqMsg;
			prJoinAbortMsg->prStaRec = prJoinInfo->prTargetStaRec;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinAbortMsg, MSG_SEND_METHOD_BUF);

		}

		/* Stop Join Timer. */
		cnmTimerStopTimer(prAdapter, &(prP2pFsmInfo->rP2pFsmTimeoutTimer));

		/* Release channel requested. */
		p2pFuncReleaseCh(prAdapter, &(prP2pFsmInfo->rChnlReqInfo));

	} while (FALSE);

	return;

}				/* p2pStateAbort_GC_JOIN */
