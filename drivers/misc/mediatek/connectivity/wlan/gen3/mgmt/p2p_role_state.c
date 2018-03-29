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

#include "precomp.h"

VOID
p2pRoleStateInit_IDLE(IN P_ADAPTER_T prAdapter, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN P_BSS_INFO_T prP2pBssInfo)
{

	cnmTimerStartTimer(prAdapter, &(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer), P2P_AP_CHNL_HOLD_TIME_MS);

}				/* p2pRoleStateInit_IDLE */

VOID
p2pRoleStateAbort_IDLE(IN P_ADAPTER_T prAdapter,
		       IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN P_P2P_CHNL_REQ_INFO_T prP2pChnlReqInfo)
{

	/* AP mode channel hold time. */
	if (prP2pChnlReqInfo->fgIsChannelRequested)
		p2pFuncReleaseCh(prAdapter, prP2pRoleFsmInfo->ucBssIndex, prP2pChnlReqInfo);

	cnmTimerStopTimer(prAdapter, &(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer));

}				/* p2pRoleStateAbort_IDLE */

VOID p2pRoleStateInit_SCAN(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prScanReqInfo != NULL));

		prScanReqInfo->fgIsScanRequest = TRUE;

		p2pFuncRequestScan(prAdapter, ucBssIndex, prScanReqInfo);

	} while (FALSE);

}				/* p2pRoleStateInit_SCAN */

VOID p2pRoleStateAbort_SCAN(IN P_ADAPTER_T prAdapter, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo)
{
	P_P2P_SCAN_REQ_INFO_T prScanInfo = (P_P2P_SCAN_REQ_INFO_T) NULL;

	do {
		prScanInfo = &prP2pRoleFsmInfo->rScanReqInfo;

		p2pFuncCancelScan(prAdapter, prP2pRoleFsmInfo->ucBssIndex, prScanInfo);

		/* TODO: May need indicate port index to upper layer. */
		kalP2PIndicateScanDone(prAdapter->prGlueInfo, prP2pRoleFsmInfo->ucRoleIndex, prScanInfo->fgIsAbort);
	} while (FALSE);

}				/* p2pRoleStateAbort_SCAN */

VOID
p2pRoleStateInit_REQING_CHANNEL(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prChnlReqInfo != NULL));

		p2pFuncAcquireCh(prAdapter, ucBssIdx, prChnlReqInfo);

	} while (FALSE);

}				/* p2pRoleStateInit_REQING_CHANNEL */

VOID
p2pRoleStateAbort_REQING_CHANNEL(IN P_ADAPTER_T prAdapter,
				 IN P_BSS_INFO_T prP2pRoleBssInfo,
				 IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN ENUM_P2P_ROLE_STATE_T eNextState)
{

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleBssInfo != NULL) && (prP2pRoleFsmInfo != NULL));

		if (eNextState == P2P_ROLE_STATE_IDLE) {
			if (prP2pRoleBssInfo->eIntendOPMode == OP_MODE_ACCESS_POINT) {
				p2pFuncStartGO(prAdapter,
					       prP2pRoleBssInfo,
					       &(prP2pRoleFsmInfo->rConnReqInfo), &(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pFuncReleaseCh(prAdapter, prP2pRoleFsmInfo->ucBssIndex,
						 &(prP2pRoleFsmInfo->rChnlReqInfo));
			}
		}
	} while (FALSE);

}				/* p2pRoleStateAbort_REQING_CHANNEL */

VOID
p2pRoleStateInit_AP_CHNL_DETECTION(IN P_ADAPTER_T prAdapter,
				   IN UINT_8 ucBssIndex,
				   IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo, IN P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	UINT_8 ucPreferedChnl = 0;
	ENUM_BAND_T eBand = BAND_NULL;
	ENUM_CHNL_EXT_T eSco = CHNL_EXT_SCN;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prScanReqInfo != NULL)
			     && (prConnReqInfo != NULL));

		prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

		if ((cnmPreferredChannel(prAdapter,
					 &eBand,
					 &ucPreferedChnl,
					 &eSco) == FALSE) && (prConnReqInfo->rChannelInfo.ucChannelNum == 0)) {
			/* Sparse channel detection. */
			prP2pSpecificBssInfo->ucPreferredChannel = 0;

			prScanReqInfo->eScanType = SCAN_TYPE_PASSIVE_SCAN;
			prScanReqInfo->u2PassiveDewellTime = 50;	/* 50ms for passive channel load detection */
		} else {
			/* Active scan to shorten scan time. */
			prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
			prScanReqInfo->u2PassiveDewellTime = 0;

			if (prConnReqInfo->rChannelInfo.ucChannelNum != 0) {
				prP2pSpecificBssInfo->ucPreferredChannel = prConnReqInfo->rChannelInfo.ucChannelNum;
				prP2pSpecificBssInfo->eRfBand = prConnReqInfo->rChannelInfo.eBand;
				prP2pSpecificBssInfo->eRfSco = CHNL_EXT_SCN;
			} else {
				prP2pSpecificBssInfo->ucPreferredChannel = ucPreferedChnl;
				prP2pSpecificBssInfo->eRfBand = eBand;
				prP2pSpecificBssInfo->eRfSco = eSco;
			}

		}

		/* TODO: See if channel set to include 5G or only 2.4G */
		prScanReqInfo->eChannelSet = SCAN_CHANNEL_2G4;

		prScanReqInfo->fgIsAbort = TRUE;
		prScanReqInfo->fgIsScanRequest = TRUE;
		prScanReqInfo->ucNumChannelList = 0;
		prScanReqInfo->u4BufLength = 0;
		prScanReqInfo->ucSsidNum = 1;
		prScanReqInfo->arSsidStruct[0].ucSsidLen = 0;

		p2pFuncRequestScan(prAdapter, ucBssIndex, prScanReqInfo);

	} while (FALSE);

}				/* p2pRoleStateInit_AP_CHNL_DETECTION */

VOID
p2pRoleStateAbort_AP_CHNL_DETECTION(IN P_ADAPTER_T prAdapter,
				    IN UINT_8 ucBssIndex,
				    IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo,
				    IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo,
				    IN P_P2P_SCAN_REQ_INFO_T prP2pScanReqInfo, IN ENUM_P2P_ROLE_STATE_T eNextState)
{
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;

	do {
		if (eNextState == P2P_ROLE_STATE_REQING_CHANNEL) {
			prP2pSpecificBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo;

			if (prP2pSpecificBssInfo->ucPreferredChannel == 0) {
				if (scnQuerySparseChannel(prAdapter,
							  &prP2pSpecificBssInfo->eRfBand,
							  &prP2pSpecificBssInfo->ucPreferredChannel)) {
					prP2pSpecificBssInfo->eRfSco = CHNL_EXT_SCN;
				} else {
					DBGLOG(P2P, ERROR, "Sparse Channel Error, use default settings\n");
					/* Sparse channel false. */
					prP2pSpecificBssInfo->ucPreferredChannel = P2P_DEFAULT_LISTEN_CHANNEL;
					prP2pSpecificBssInfo->eRfBand = BAND_2G4;
					prP2pSpecificBssInfo->eRfSco = CHNL_EXT_SCN;
				}
			}

			prChnlReqInfo->u8Cookie = 0;
			prChnlReqInfo->ucReqChnlNum = prP2pSpecificBssInfo->ucPreferredChannel;
			prChnlReqInfo->eBand = prP2pSpecificBssInfo->eRfBand;
			prChnlReqInfo->eChnlSco = prP2pSpecificBssInfo->eRfSco;
			prChnlReqInfo->u4MaxInterval = P2P_AP_CHNL_HOLD_TIME_MS;
			prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_GO_START_BSS;

			prChnlReqInfo->eChannelWidth = CW_20_40MHZ;
			prChnlReqInfo->ucCenterFreqS1 = 0;
			prChnlReqInfo->ucCenterFreqS2 = 0;

		} else {
			p2pFuncCancelScan(prAdapter, ucBssIndex, prP2pScanReqInfo);
		}
	} while (FALSE);

}

VOID
p2pRoleStateInit_GC_JOIN(IN P_ADAPTER_T prAdapter,
			 IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{
	/* P_MSG_JOIN_REQ_T prJoinReqMsg = (P_MSG_JOIN_REQ_T)NULL; */
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL) && (prChnlReqInfo != NULL));

		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

		/* Setup a join timer. */
		DBGLOG(P2P, TRACE, "Start a join init timer\n");
		cnmTimerStartTimer(prAdapter,
				   &(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer),
				   (prChnlReqInfo->u4MaxInterval - AIS_JOIN_CH_GRANT_THRESHOLD));

		p2pFuncGCJoin(prAdapter, prP2pBssInfo, &(prP2pRoleFsmInfo->rJoinInfo));

	} while (FALSE);

}				/* p2pRoleStateInit_GC_JOIN */

VOID
p2pRoleStateAbort_GC_JOIN(IN P_ADAPTER_T prAdapter,
			  IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo,
			  IN P_P2P_JOIN_INFO_T prJoinInfo, IN ENUM_P2P_ROLE_STATE_T eNextState)
{
	do {

		if (prJoinInfo->fgIsJoinComplete == FALSE) {
			P_MSG_JOIN_ABORT_T prJoinAbortMsg = (P_MSG_JOIN_ABORT_T) NULL;

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
		cnmTimerStopTimer(prAdapter, &(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer));

		/* Release channel requested. */
		p2pFuncReleaseCh(prAdapter, prP2pRoleFsmInfo->ucBssIndex, &(prP2pRoleFsmInfo->rChnlReqInfo));
	} while (FALSE);
}

VOID
p2pRoleStatePrepare_To_REQING_CHANNEL_STATE(IN P_ADAPTER_T prAdapter,
					    IN P_BSS_INFO_T prBssInfo,
					    IN P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo,
					    OUT P_P2P_CHNL_REQ_INFO_T prChnlReqInfo)
{
	ENUM_BAND_T eBand;
	UINT_8 ucChannel;
	ENUM_CHNL_EXT_T eSCO;
	ENUM_BAND_T eBandBackup;
	UINT_8 ucChannelBackup;
	ENUM_CHNL_EXT_T eSCOBackup;

	do {
		/* P2P BSS info is for temporarily use
		 * Request a 80MHz channel before starting AP/GO
		 * to prevent from STA/GC connected too early (before CH abort)
		 * Therefore, STA/GC Rate will drop during DHCP exchange packets
		 */

		/* Previous issue:
		 * Always request 20MHz channel, but carry 40MHz HT cap/80MHz VHT cap,
		 * then if GC/STA connected before CH abort,
		 * GO/AP cannot listen to GC/STA's 40MHz/80MHz packets.
		 */

		eBandBackup = prBssInfo->eBand;
		ucChannelBackup = prBssInfo->ucPrimaryChannel;
		eSCOBackup = prBssInfo->eBssSCO;

		prBssInfo->ucPrimaryChannel = prConnReqInfo->rChannelInfo.ucChannelNum;
		prBssInfo->eBand = prConnReqInfo->rChannelInfo.eBand;

		if (cnmPreferredChannel(prAdapter, &eBand, &ucChannel, &eSCO) &&
		    eSCO != CHNL_EXT_SCN && ucChannel == prBssInfo->ucPrimaryChannel && eBand == prBssInfo->eBand) {
			prBssInfo->eBssSCO = eSCO;
		} else {
			prBssInfo->eBssSCO = rlmDecideScoForAP(prAdapter, prBssInfo);
		}

		ASSERT_BREAK((prAdapter != NULL) && (prConnReqInfo != NULL) && (prChnlReqInfo != NULL));
		prChnlReqInfo->u8Cookie = 0;
		prChnlReqInfo->ucReqChnlNum = prConnReqInfo->rChannelInfo.ucChannelNum;
		prChnlReqInfo->eBand = prConnReqInfo->rChannelInfo.eBand;
		prChnlReqInfo->eChnlSco = prBssInfo->eBssSCO;
		prChnlReqInfo->u4MaxInterval = P2P_AP_CHNL_HOLD_TIME_MS;
		prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_GO_START_BSS;

		if (prBssInfo->eBand == BAND_5G)
			prChnlReqInfo->eChannelWidth = CW_80MHZ;
		else
			prChnlReqInfo->eChannelWidth = CW_20_40MHZ;

		prChnlReqInfo->ucCenterFreqS1 = nicGetVhtS1(prBssInfo->ucPrimaryChannel);
		prChnlReqInfo->ucCenterFreqS2 = 0;
		DBGLOG(P2P, TRACE, "p2pRoleStatePrepare_To_REQING_CHANNEL_STATE\n");

		if (prChnlReqInfo->ucReqChnlNum > 161) {
			DBGLOG(P2P, INFO, "Channels above 161 only use 20M bandwidth\n");
			prChnlReqInfo->ucCenterFreqS1 = 0;
			prChnlReqInfo->eChannelWidth = CW_20_40MHZ;
		}

		/* Reset */
		prBssInfo->ucPrimaryChannel = ucChannelBackup;
		prBssInfo->eBand = eBandBackup;
		prBssInfo->eBssSCO = eSCOBackup;
	} while (FALSE);
}
