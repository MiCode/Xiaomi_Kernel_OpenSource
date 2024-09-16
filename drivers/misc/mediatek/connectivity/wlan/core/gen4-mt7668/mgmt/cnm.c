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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/cnm.c#2
*/

/*! \file   "cnm.c"
*    \brief  Module of Concurrent Network Management
*
*    Module of Concurrent Network Management
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
#if CFG_SUPPORT_DBDC
#define DBDC_SWITCH_GUARD_TIME		(4*1000)	/*ms*/
#define DBDC_DISABLE_COUNTDOWN_TIME	(2*1000)	/*ms*/
#endif /*CFG_SUPPORT_DBDC*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#if CFG_SUPPORT_DBDC
typedef enum _ENUM_CNM_DBDC_DECISION_TIMER_T {
	DBDC_DECISION_TIMER_SWITCH_GUARD_TIME,
	DBDC_DECISION_TIMER_DISABLE_COUNT_DOWN,
	DBDC_DECISION_STATE_NUM
} ENUM_CNM_DBDC_DECISION_TIMER_T, *P_ENUM_CNM_DBDC_DECISION_TIMER_T;
#endif /*CFG_SUPPORT_DBDC*/

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

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to initialize variables in CNM_INFO_T.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmInit(P_ADAPTER_T prAdapter)
{
	P_CNM_INFO_T prCnmInfo;

	ASSERT(prAdapter);

	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->fgChGranted = FALSE;
#if CFG_SUPPORT_DBDC
	cnmTimerInitTimer(prAdapter,
					&prAdapter->rWifiVar.rDBDCSwitchGuardTimer,
					(PFN_MGMT_TIMEOUT_FUNC)cnmDbdcDecision,
					(ULONG)DBDC_DECISION_TIMER_SWITCH_GUARD_TIME);

	cnmTimerInitTimer(prAdapter,
					&prAdapter->rWifiVar.rDBDCDisableCountdownTimer,
					(PFN_MGMT_TIMEOUT_FUNC)cnmDbdcDecision,
					(ULONG)DBDC_DECISION_TIMER_DISABLE_COUNT_DOWN);
#endif /*CFG_SUPPORT_DBDC*/
}				/* end of cnmInit()*/

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to initialize variables in CNM_INFO_T.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmUninit(P_ADAPTER_T prAdapter)
{
}				/* end of cnmUninit()*/

/*----------------------------------------------------------------------------*/
/*!
* @brief Before handle the message from other module, it need to obtain
*        the Channel privilege from Channel Manager
*
* @param[in] prMsgHdr   The message need to be handled.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmChMngrRequestPrivilege(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_MSG_CH_REQ_T prMsgChReq;
	P_CMD_CH_PRIVILEGE_T prCmdBody;
	WLAN_STATUS rStatus;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChReq = (P_MSG_CH_REQ_T)prMsgHdr;

	prCmdBody = (P_CMD_CH_PRIVILEGE_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_CH_PRIVILEGE_T));
	ASSERT(prCmdBody);

	/* To do: exception handle */
	if (!prCmdBody) {
		DBGLOG(CNM, ERROR, "ChReq: fail to get buf (net=%d, token=%d)\n",
		       prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID);

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	DBGLOG(CNM, INFO, "ChReq net=%d token=%d b=%d c=%d s=%d w=%d\n",
	       prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID,
	       prMsgChReq->eRfBand, prMsgChReq->ucPrimaryChannel, prMsgChReq->eRfSco, prMsgChReq->eRfChannelWidth);

	prCmdBody->ucBssIndex = prMsgChReq->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChReq->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_REQ;	/* Request */
	prCmdBody->ucPrimaryChannel = prMsgChReq->ucPrimaryChannel;
	prCmdBody->ucRfSco = (UINT_8)prMsgChReq->eRfSco;
	prCmdBody->ucRfBand = (UINT_8)prMsgChReq->eRfBand;
	prCmdBody->ucRfChannelWidth = (UINT_8)prMsgChReq->eRfChannelWidth;
	prCmdBody->ucRfCenterFreqSeg1 = (UINT_8)prMsgChReq->ucRfCenterFreqSeg1;
	prCmdBody->ucRfCenterFreqSeg2 = (UINT_8)prMsgChReq->ucRfCenterFreqSeg2;
	prCmdBody->ucReqType = (UINT_8)prMsgChReq->eReqType;
	prCmdBody->ucDBDCBand = (UINT_8)prMsgChReq->eDBDCBand;
	prCmdBody->aucReserved = 0;
	prCmdBody->u4MaxInterval = prMsgChReq->u4MaxInterval;
	prCmdBody->aucReserved2[0] = 0;
	prCmdBody->aucReserved2[1] = 0;
	prCmdBody->aucReserved2[2] = 0;
	prCmdBody->aucReserved2[3] = 0;
	prCmdBody->aucReserved2[4] = 0;
	prCmdBody->aucReserved2[5] = 0;
	prCmdBody->aucReserved2[6] = 0;
	prCmdBody->aucReserved2[7] = 0;

	ASSERT(prCmdBody->ucBssIndex <= MAX_BSS_INDEX);

	/* For monkey testing 20110901 */
	if (prCmdBody->ucBssIndex > MAX_BSS_INDEX)
		DBGLOG(CNM, ERROR, "CNM: ChReq with wrong netIdx=%d\n\n", prCmdBody->ucBssIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_CH_PRIVILEGE,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(CMD_CH_PRIVILEGE_T),	/* u4SetQueryInfoLen */
				      (PUINT_8)prCmdBody,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	   );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);
}				/* end of cnmChMngrRequestPrivilege()*/

/*----------------------------------------------------------------------------*/
/*!
* @brief Before deliver the message to other module, it need to release
*        the Channel privilege to Channel Manager.
*
* @param[in] prMsgHdr   The message need to be delivered
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmChMngrAbortPrivilege(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_MSG_CH_ABORT_T prMsgChAbort;
	P_CMD_CH_PRIVILEGE_T prCmdBody;
	P_CNM_INFO_T prCnmInfo;
	WLAN_STATUS rStatus;
#if CFG_SISO_SW_DEVELOP
	P_BSS_INFO_T prBssInfo;
#endif

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChAbort = (P_MSG_CH_ABORT_T)prMsgHdr;

	/* Check if being granted channel privilege is aborted */
	prCnmInfo = &prAdapter->rCnmInfo;
	if (prCnmInfo->fgChGranted &&
	    prCnmInfo->ucBssIndex == prMsgChAbort->ucBssIndex && prCnmInfo->ucTokenID == prMsgChAbort->ucTokenID) {

		prCnmInfo->fgChGranted = FALSE;
	}

	prCmdBody = (P_CMD_CH_PRIVILEGE_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_CH_PRIVILEGE_T));
	ASSERT(prCmdBody);

	/* To do: exception handle */
	if (!prCmdBody) {
		DBGLOG(CNM, ERROR, "ChAbort: fail to get buf (net=%d, token=%d)\n",
		       prMsgChAbort->ucBssIndex, prMsgChAbort->ucTokenID);

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prCmdBody->ucBssIndex = prMsgChAbort->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChAbort->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_ABORT;	/* Abort */
	prCmdBody->ucDBDCBand = (UINT_8)prMsgChAbort->eDBDCBand;

	DBGLOG(CNM, INFO, "ChAbort net=%d token=%d dbdc=%u\n",
		prCmdBody->ucBssIndex, prCmdBody->ucTokenID, prCmdBody->ucDBDCBand);

	ASSERT(prCmdBody->ucBssIndex <= MAX_BSS_INDEX);

	/* For monkey testing 20110901 */
	if (prCmdBody->ucBssIndex > MAX_BSS_INDEX)
		DBGLOG(CNM, ERROR, "CNM: ChAbort with wrong netIdx=%d\n\n", prCmdBody->ucBssIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_CH_PRIVILEGE,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(CMD_CH_PRIVILEGE_T),	/* u4SetQueryInfoLen */
				      (PUINT_8)prCmdBody,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	   );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

#if CFG_SISO_SW_DEVELOP
	prBssInfo = prAdapter->aprBssInfo[prMsgChAbort->ucBssIndex];
	/* Driver clear granted CH in BSS info */
	prBssInfo->fgIsGranted = FALSE;
	prBssInfo->eBandGranted = BAND_NULL;
	prBssInfo->ucPrimaryChannelGranted = 0;
#endif

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);
}				/* end of cnmChMngrAbortPrivilege()*/

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmChMngrHandleChEvent(P_ADAPTER_T prAdapter, P_WIFI_EVENT_T prEvent)
{
	P_EVENT_CH_PRIVILEGE_T prEventBody;
	P_MSG_CH_GRANT_T prChResp;
	P_BSS_INFO_T prBssInfo;
	P_CNM_INFO_T prCnmInfo;

	ASSERT(prAdapter);
	ASSERT(prEvent);

	prEventBody = (P_EVENT_CH_PRIVILEGE_T)(prEvent->aucBuffer);
	prChResp = (P_MSG_CH_GRANT_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_GRANT_T));
	ASSERT(prChResp);

	/* To do: exception handle */
	if (!prChResp) {
		DBGLOG(CNM, ERROR, "ChGrant: fail to get buf (net=%d, token=%d)\n",
		       prEventBody->ucBssIndex, prEventBody->ucTokenID);

		return;
	}

	DBGLOG(CNM, INFO, "ChGrant net=%d token=%d ch=%d sco=%d\n",
	       prEventBody->ucBssIndex, prEventBody->ucTokenID, prEventBody->ucPrimaryChannel, prEventBody->ucRfSco);

	ASSERT(prEventBody->ucBssIndex <= MAX_BSS_INDEX);
	ASSERT(prEventBody->ucStatus == EVENT_CH_STATUS_GRANT);

	prBssInfo = prAdapter->aprBssInfo[prEventBody->ucBssIndex];

	/* Decide message ID based on network and response status */
	if (IS_BSS_AIS(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_AIS_CH_GRANT;
#if CFG_ENABLE_WIFI_DIRECT
	else if (prAdapter->fgIsP2PRegistered && IS_BSS_P2P(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_P2P_CH_GRANT;
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	else if (IS_BSS_BOW(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_BOW_CH_GRANT;
#endif
	else {
		cnmMemFree(prAdapter, prChResp);
		return;
	}

	prChResp->ucBssIndex = prEventBody->ucBssIndex;
	prChResp->ucTokenID = prEventBody->ucTokenID;
	prChResp->ucPrimaryChannel = prEventBody->ucPrimaryChannel;
	prChResp->eRfSco = (ENUM_CHNL_EXT_T)prEventBody->ucRfSco;
	prChResp->eRfBand = (ENUM_BAND_T)prEventBody->ucRfBand;
	prChResp->eRfChannelWidth = (ENUM_CHANNEL_WIDTH_T)prEventBody->ucRfChannelWidth;
	prChResp->ucRfCenterFreqSeg1 = prEventBody->ucRfCenterFreqSeg1;
	prChResp->ucRfCenterFreqSeg2 = prEventBody->ucRfCenterFreqSeg2;
	prChResp->eReqType = (ENUM_CH_REQ_TYPE_T)prEventBody->ucReqType;
	prChResp->eDBDCBand = (ENUM_DBDC_BN_T)prEventBody->ucDBDCBand;
	prChResp->u4GrantInterval = prEventBody->u4GrantInterval;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prChResp, MSG_SEND_METHOD_BUF);

	/* Record current granted BSS for TXM's reference */
	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->ucBssIndex = prEventBody->ucBssIndex;
	prCnmInfo->ucTokenID = prEventBody->ucTokenID;
	prCnmInfo->fgChGranted = TRUE;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
VOID cnmRadarDetectEvent(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_EVENT_RDD_REPORT_T prEventBody;
	P_BSS_INFO_T prBssInfo;
	P_MSG_P2P_RADAR_DETECT_T prP2pRddDetMsg;
	UINT_8 ucBssIndex;

	DBGLOG(CNM, INFO, "cnmRadarDetectEvent.\n");

	prEventBody = (P_EVENT_RDD_REPORT_T)(prEvent->aucBuffer);

	prP2pRddDetMsg = (P_MSG_P2P_RADAR_DETECT_T) cnmMemAlloc(prAdapter,
					RAM_TYPE_MSG, sizeof(*prP2pRddDetMsg));

	if (!prP2pRddDetMsg) {
		DBGLOG(CNM, ERROR, "cnmMemAlloc for prP2pRddDetMsg failed!\n");
		return;
	}

	prP2pRddDetMsg->rMsgHdr.eMsgId = MID_CNM_P2P_RADAR_DETECT;

	for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

		if (prBssInfo && prBssInfo->fgIsDfsActive) {
			prP2pRddDetMsg->ucBssIndex = ucBssIndex;
			break;
		}
	}

	p2pFuncSetDfsState(DFS_STATE_DETECTED);

	p2pFuncRadarInfoInit();

	g_rP2pRadarInfo.ucRadarReportMode = prEventBody->ucRadarReportMode;
	g_rP2pRadarInfo.ucRddIdx = prEventBody->ucRddIdx;
	g_rP2pRadarInfo.ucLongDetected = prEventBody->ucLongDetected;
	g_rP2pRadarInfo.ucPeriodicDetected = prEventBody->ucPeriodicDetected;
	g_rP2pRadarInfo.ucLPBNum = prEventBody->ucLPBNum;
	g_rP2pRadarInfo.ucPPBNum = prEventBody->ucPPBNum;
	g_rP2pRadarInfo.ucLPBPeriodValid = prEventBody->ucLPBPeriodValid;
	g_rP2pRadarInfo.ucLPBWidthValid = prEventBody->ucLPBWidthValid;
	g_rP2pRadarInfo.ucPRICountM1 = prEventBody->ucPRICountM1;
	g_rP2pRadarInfo.ucPRICountM1TH = prEventBody->ucPRICountM1TH;
	g_rP2pRadarInfo.ucPRICountM2 = prEventBody->ucPRICountM2;
	g_rP2pRadarInfo.ucPRICountM2TH = prEventBody->ucPRICountM2TH;
	g_rP2pRadarInfo.u4PRI1stUs = prEventBody->u4PRI1stUs;
	kalMemCopy(&g_rP2pRadarInfo.arLpbContent[0], &prEventBody->arLpbContent[0],
				prEventBody->ucLPBNum*sizeof(LONG_PULSE_BUFFER_T));
	kalMemCopy(&g_rP2pRadarInfo.arPpbContent[0], &prEventBody->arPpbContent[0],
				prEventBody->ucPPBNum*sizeof(PERIODIC_PULSE_BUFFER_T));

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prP2pRddDetMsg, MSG_SEND_METHOD_BUF);
}

VOID cnmCsaDoneEvent(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent)
{
	P_BSS_INFO_T prBssInfo;
	P_MSG_P2P_CSA_DONE_T prP2pCsaDoneMsg;
	UINT_8 ucBssIndex;

	DBGLOG(CNM, INFO, "cnmCsaDoneEvent.\n");

	prP2pCsaDoneMsg = (P_MSG_P2P_CSA_DONE_T) cnmMemAlloc(prAdapter,
					RAM_TYPE_MSG, sizeof(*prP2pCsaDoneMsg));

	if (!prP2pCsaDoneMsg) {
		DBGLOG(CNM, ERROR, "cnmMemAlloc for prP2pCsaDoneMsg failed!\n");
		return;
	}

	prAdapter->rWifiVar.fgCsaInProgress = FALSE;

	prP2pCsaDoneMsg->rMsgHdr.eMsgId = MID_CNM_P2P_CSA_DONE;

	for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

		if (prBssInfo && prBssInfo->fgIsDfsActive) {
			prP2pCsaDoneMsg->ucBssIndex = ucBssIndex;
			break;
		}
	}

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prP2pCsaDoneMsg, MSG_SEND_METHOD_BUF);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is invoked for P2P or BOW networks
*
* @param (none)
*
* @return TRUE: suggest to adopt the returned preferred channel
*         FALSE: No suggestion. Caller should adopt its preference
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
cnmPreferredChannel(P_ADAPTER_T prAdapter, P_ENUM_BAND_T prBand, PUINT_8 pucPrimaryChannel, P_ENUM_CHNL_EXT_T prBssSCO)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);
	ASSERT(prBand);
	ASSERT(pucPrimaryChannel);
	ASSERT(prBssSCO);

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

		if (prBssInfo) {
			if (IS_BSS_AIS(prBssInfo) && RLM_NET_PARAM_VALID(prBssInfo)) {
				*prBand = prBssInfo->eBand;
				*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;
				*prBssSCO = prBssInfo->eBssSCO;

				return TRUE;
			}
		}
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return TRUE: available channel is limited to return value
*         FALSE: no limited
*/
/*----------------------------------------------------------------------------*/
BOOLEAN cnmAisInfraChannelFixed(P_ADAPTER_T prAdapter, P_ENUM_BAND_T prBand, PUINT_8 pucPrimaryChannel)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;
	P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;

	ASSERT(prAdapter);

	if (prWifiVar->u4ScanCtrl & SCN_CTRL_DEFAULT_SCAN_CTRL) {
		/*DBGLOG(CNM, INFO, "cnmAisInfraChannelFixed: ByPass AIS channel Fix check\n");*/
		return FALSE;
	}

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

#if 0
		DBGLOG(INIT, INFO, "%s BSS[%u] active[%u] netType[%u]\n",
		       __func__, i, prBssInfo->fgIsNetActive, prBssInfo->eNetworkType);
#endif

		if (!IS_NET_ACTIVE(prAdapter, i))
			continue;

#if CFG_ENABLE_WIFI_DIRECT
		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			BOOLEAN fgFixedChannel =
			p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData]);

			if (fgFixedChannel) {

				*prBand = prBssInfo->eBand;
				*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;

				return TRUE;

			}
		}
#endif

#if CFG_ENABLE_BT_OVER_WIFI && CFG_BOW_LIMIT_AIS_CHNL
		if (prBssInfo->eNetworkType == NETWORK_TYPE_BOW) {
			*prBand = prBssInfo->eBand;
			*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;

			return TRUE;
		}
#endif

	}

	return FALSE;
}

#if CFG_SUPPORT_CHNL_CONFLICT_REVISE
BOOLEAN cnmAisDetectP2PChannel(P_ADAPTER_T prAdapter, P_ENUM_BAND_T prBand, PUINT_8 pucPrimaryChannel)
{
	UINT_8 i = 0;
	P_BSS_INFO_T prBssInfo;
#if CFG_ENABLE_WIFI_DIRECT
	for (; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
			continue;
		if (prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED ||
		    (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT && prBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			*prBand = prBssInfo->eBand;
			*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;
			return TRUE;
		}
	}
#endif
	return FALSE;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID cnmAisInfraConnectNotify(P_ADAPTER_T prAdapter)
{
#if CFG_ENABLE_BT_OVER_WIFI
	P_BSS_INFO_T prBssInfo, prAisBssInfo, prBowBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	prAisBssInfo = NULL;
	prBowBssInfo = NULL;

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)) {
			if (IS_BSS_AIS(prBssInfo))
				prAisBssInfo = prBssInfo;
			else if (IS_BSS_BOW(prBssInfo))
				prBowBssInfo = prBssInfo;
		}
	}

	if (prAisBssInfo && prBowBssInfo && RLM_NET_PARAM_VALID(prAisBssInfo) && RLM_NET_PARAM_VALID(prBowBssInfo)) {
		if (prAisBssInfo->eBand != prBowBssInfo->eBand ||
		    prAisBssInfo->ucPrimaryChannel != prBowBssInfo->ucPrimaryChannel) {

			/* Notify BOW to do deactivation */
			bowNotifyAllLinkDisconnected(prAdapter);
		}
	}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return TRUE: permitted
*         FALSE: Not permitted
*/
/*----------------------------------------------------------------------------*/
BOOLEAN cnmAisIbssIsPermitted(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	/* P2P device network shall be included */
	for (i = 0; i <= BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) && !IS_BSS_AIS(prBssInfo))
			return FALSE;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return TRUE: permitted
*         FALSE: Not permitted
*/
/*----------------------------------------------------------------------------*/
BOOLEAN cnmP2PIsPermitted(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;
	BOOLEAN fgBowIsActive;

	ASSERT(prAdapter);

	fgBowIsActive = FALSE;

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS)
				return FALSE;
			else if (IS_BSS_BOW(prBssInfo))
				fgBowIsActive = TRUE;
		}
	}

#if CFG_ENABLE_BT_OVER_WIFI
	if (fgBowIsActive) {
		/* Notify BOW to do deactivation */
		bowNotifyAllLinkDisconnected(prAdapter);
	}
#endif

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return TRUE: permitted
*         FALSE: Not permitted
*/
/*----------------------------------------------------------------------------*/
BOOLEAN cnmBowIsPermitted(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	/* P2P device network shall be included */
	for (i = 0; i <= BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) &&
		    (IS_BSS_P2P(prBssInfo) || prBssInfo->eCurrentOPMode == OP_MODE_IBSS)) {
			return FALSE;
		}
	}

	return TRUE;
}



static UINT_8 cnmGetAPBwPermitted(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucAPBandwidth = MAX_BW_160MHZ;
	P_BSS_DESC_T    prBssDesc = NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T)NULL;
	UINT_8 i = 0;
	UINT_8 ucOffset = (MAX_BW_80MHZ - CW_80MHZ);


	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);


	if (IS_BSS_AIS(prBssInfo)) {
		/*AIS station mode*/
		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
	} else if (IS_BSS_P2P(prBssInfo)) {
		/* P2P mode */

		for (i = 0 ; i < BSS_P2P_NUM; i++) {

			if (!prAdapter->rWifiVar.aprP2pRoleFsmInfo[i])
				continue;

			if (prAdapter->rWifiVar.aprP2pRoleFsmInfo[i]->ucBssIndex == ucBssIndex)
				break;

		}

		if (i >= BSS_P2P_NUM) {
			prP2pRoleFsmInfo = NULL;
		} else {
			prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[i];

			/*only GC need to consider GO's BW*/
			if (!p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData]))
				prBssDesc = prP2pRoleFsmInfo->rJoinInfo.prTargetBssDesc;

		}


	}

	if (prBssDesc) {
		if (prBssDesc->eChannelWidth == CW_20_40MHZ) {
			if ((prBssDesc->eSco == CHNL_EXT_SCA) || (prBssDesc->eSco == CHNL_EXT_SCB))
				ucAPBandwidth = MAX_BW_40MHZ;
			else
				ucAPBandwidth = MAX_BW_20MHZ;
		} else {
			ucAPBandwidth = prBssDesc->eChannelWidth + ucOffset;
		}

	}

	return ucAPBandwidth;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return TRUE: permitted
*         FALSE: Not permitted
*/
/*----------------------------------------------------------------------------*/
BOOLEAN cnmBss40mBwPermitted(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	ASSERT(prAdapter);

	/* Note: To support real-time decision instead of current activated-time,
	 *       the STA roaming case shall be considered about synchronization
	 *       problem. Another variable fgAssoc40mBwAllowed is added to
	 *       represent HT capability when association
	 */

	/* Decide max bandwidth by feature option */
	if (cnmGetBssMaxBw(prAdapter, ucBssIndex) < MAX_BW_40MHZ)
		return FALSE;

	/*check AP or GO capbility for Station or GC */
	if (cnmGetAPBwPermitted(prAdapter, ucBssIndex) < MAX_BW_40MHZ)
		return FALSE;
#if 0
	/* Decide max by other BSS */
	for (i = 0; i < BSS_INFO_NUM; i++) {
		if (i != ucBssIndex) {
			prBssInfo = prAdapter->aprBssInfo[i];

			if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) &&
			    (prBssInfo->fg40mBwAllowed || prBssInfo->fgAssoc40mBwAllowed))
				return FALSE;
		}
	}
#endif

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param (none)
*
* @return TRUE: permitted
*         FALSE: Not permitted
*/
/*----------------------------------------------------------------------------*/
BOOLEAN cnmBss80mBwPermitted(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	ASSERT(prAdapter);

	/* Note: To support real-time decision instead of current activated-time,
	 *       the STA roaming case shall be considered about synchronization
	 *       problem. Another variable fgAssoc40mBwAllowed is added to
	 *       represent HT capability when association
	 */

	/* Check 40Mhz first */
	if (!cnmBss40mBwPermitted(prAdapter, ucBssIndex))
		return FALSE;

	/* Decide max bandwidth by feature option */
	if (cnmGetBssMaxBw(prAdapter, ucBssIndex) < MAX_BW_80MHZ)
		return FALSE;

	/*check AP or GO capbility for Station or GC */
	if (cnmGetAPBwPermitted(prAdapter, ucBssIndex) < MAX_BW_80MHZ)
		return FALSE;

	return TRUE;
}

UINT_8 cnmGetBssMaxBw(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucMaxBandwidth = MAX_BW_80_80_MHZ; /*chip capability*/
	P_BSS_DESC_T    prBssDesc = NULL;
	ENUM_BAND_T eBand = BAND_NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo = (P_P2P_CONNECTION_REQ_INFO_T) NULL;
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	UINT_8 ucChannelBw = MAX_BW_80_80_MHZ;
#endif

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo)) {
		/* STA mode */


		/*should check Bss_info could be used or not
		*the info might not be trustable before state3
		*/

		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		if (prBssDesc)
			eBand = prBssDesc->eBand;
		else
			eBand = prBssInfo->eBand;


		ASSERT(eBand != BAND_NULL);

		if (eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		prP2pRoleFsmInfo = p2pFuncGetRoleByBssIdx(prAdapter, ucBssIndex);
		if (!prAdapter->rWifiVar.ucApChnlDefFromCfg && prP2pRoleFsmInfo
			&& prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
			ucMaxBandwidth = prP2pConnReqInfo->eChnlBw;
		} else {
			/* AP mode */
			if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings[prBssInfo->u4PrivateData])) {
				if (prBssInfo->eBand == BAND_2G4)
					ucMaxBandwidth = prAdapter->rWifiVar.ucAp2gBandwidth;
				else
					ucMaxBandwidth = prAdapter->rWifiVar.ucAp5gBandwidth;

				if (ucMaxBandwidth > prAdapter->rWifiVar.ucApBandwidth)
					ucMaxBandwidth = prAdapter->rWifiVar.ucApBandwidth;
			}
			/* P2P mode */
			else {
				if (prBssInfo->eBand == BAND_2G4)
					ucMaxBandwidth = prAdapter->rWifiVar.ucP2p2gBandwidth;
				else
					ucMaxBandwidth = prAdapter->rWifiVar.ucP2p5gBandwidth;
			}

		}

	}

#if (CFG_SUPPORT_SINGLE_SKU == 1)
	if (IS_BSS_AIS(prBssInfo) && prBssDesc)
		ucChannelBw = rlmDomainGetChannelBw(prBssDesc->ucChannelNum);
	else
		ucChannelBw =
			rlmDomainGetChannelBw(prBssInfo->ucPrimaryChannel);
	if (ucMaxBandwidth > ucChannelBw)
		ucMaxBandwidth = ucChannelBw;
#endif
	DBGLOG(CNM, INFO, "pCH=%d, BW=%d\n",
		prBssInfo->ucPrimaryChannel, ucMaxBandwidth);

	return ucMaxBandwidth;
}


UINT_8 cnmGetBssMaxBwToChnlBW(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	UINT_8 ucMaxBandwidth = cnmGetBssMaxBw(prAdapter, ucBssIndex);
	return ucMaxBandwidth == MAX_BW_20MHZ ? ucMaxBandwidth : (ucMaxBandwidth - 1);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief    Search available HW ID and BSS_INFO structure and initialize
*           these parameters, i.e., fgIsNetActive, ucBssIndex, eNetworkType
*           and ucOwnMacIndex
*
* @param (none)
*
* @return
*/
/*----------------------------------------------------------------------------*/
P_BSS_INFO_T cnmGetBssInfoAndInit(P_ADAPTER_T prAdapter, ENUM_NETWORK_TYPE_T eNetworkType, BOOLEAN fgIsP2pDevice)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i, ucBssIndex, ucOwnMacIdx;

	ASSERT(prAdapter);

	/*specific case for p2p device scan*/
	if (eNetworkType == NETWORK_TYPE_P2P && fgIsP2pDevice) {
		prBssInfo = prAdapter->aprBssInfo[P2P_DEV_BSS_INDEX];

		prBssInfo->fgIsInUse = TRUE;
		prBssInfo->ucBssIndex = P2P_DEV_BSS_INDEX;
		prBssInfo->eNetworkType = eNetworkType;
		prBssInfo->ucOwnMacIndex = HW_BSSID_NUM;
#if CFG_SUPPORT_PNO
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
#endif

		/* initialize wlan id and status for keys */
		prBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;
		prBssInfo->wepkeyWlanIdx = WTBL_RESERVED_ENTRY;
		for (i = 0; i < MAX_KEY_NUM; i++) {
			prBssInfo->ucBMCWlanIndexSUsed[i] = FALSE;
			prBssInfo->ucBMCWlanIndexS[i] = WTBL_RESERVED_ENTRY;
			prBssInfo->wepkeyUsed[i] = FALSE;
		}
		return prBssInfo;
	}

	/*reserve ownMAC0 for MBSS*/
	ucOwnMacIdx = (eNetworkType == NETWORK_TYPE_MBSS) ? 0 : 1;

	/* Find available HW set  with the order 1,2,..*/
	do {
		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (prBssInfo && prBssInfo->fgIsInUse && ucOwnMacIdx == prBssInfo->ucOwnMacIndex)
				break;
		}

		if (ucBssIndex >= BSS_INFO_NUM) {
			/* No hit  the ucOwnMacIndex could be assigned to this new bss */
			break;
		}
	} while (++ucOwnMacIdx < HW_BSSID_NUM);


	/*should not dispatch P2P_DEV_BSS_INDEX (HW_BSSID_NUM)to general bss,
	*It means total BSS_INFO_NUM BSS are created,
	*no more reseve for MBSS
	*/
	if (ucOwnMacIdx == HW_BSSID_NUM) {

		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			/*If the Bss was alredy assigned, and in use*/
			if (prBssInfo && prBssInfo->fgIsInUse && prBssInfo->ucOwnMacIndex == 0)
				break;
		}

		if (ucBssIndex >= BSS_INFO_NUM) {
			/*there is no NETWORK_TYPE_MBSS used before */
			DBGLOG(INIT, WARN,
				"[Warning] too much Bss in use, take reserve OwnMac(%d)for usage!\n",
				ucOwnMacIdx);
			ucOwnMacIdx = 0;
		}

	}

	/* Find available BSS_INFO */
	for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (prBssInfo && !prBssInfo->fgIsInUse) {
			prBssInfo->fgIsInUse = TRUE;
			prBssInfo->ucBssIndex = ucBssIndex;
			prBssInfo->eNetworkType = eNetworkType;
			prBssInfo->ucOwnMacIndex = ucOwnMacIdx;
#if (CFG_HW_WMM_BY_BSS == 1)
			prBssInfo->ucWmmQueSet = DEFAULT_HW_WMM_INDEX;
			prBssInfo->fgIsWmmInited = FALSE;
#endif
			break;
		}
	}

	if (ucOwnMacIdx >= HW_BSSID_NUM || ucBssIndex >= BSS_INFO_NUM)
		prBssInfo = NULL;
	if (prBssInfo) {
#if CFG_SUPPORT_PNO
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
#endif

		/* initialize wlan id and status for keys */
		prBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;
		prBssInfo->wepkeyWlanIdx = WTBL_RESERVED_ENTRY;
		for (i = 0; i < MAX_KEY_NUM; i++) {
			prBssInfo->ucBMCWlanIndexSUsed[i] = FALSE;
			prBssInfo->ucBMCWlanIndexS[i] = WTBL_RESERVED_ENTRY;
			prBssInfo->wepkeyUsed[i] = FALSE;
		}
	}
	return prBssInfo;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief    Search available HW ID and BSS_INFO structure and initialize
*           these parameters, i.e., ucBssIndex, eNetworkType and ucOwnMacIndex
*
* @param (none)
*
* @return
*/
/*----------------------------------------------------------------------------*/
VOID cnmFreeBssInfo(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	prBssInfo->fgIsInUse = FALSE;
}

#if CFG_SUPPORT_DBDC
VOID cnmInitDbdcSetting(IN P_ADAPTER_T prAdapter)
{
	/* Parameter decision */
	switch (prAdapter->rWifiVar.ucDbdcMode) {
	case DBDC_MODE_DISABLED:
	case DBDC_MODE_DYNAMIC:
		cnmUpdateDbdcSetting(prAdapter, FALSE);
		break;

	case DBDC_MODE_STATIC:
		cnmUpdateDbdcSetting(prAdapter, TRUE);
		break;

	default:
		break;
	}
}

VOID cnmUpdateDbdcSetting(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgDbdcEn)
{
	UINT_8					ucWmmSetBitmap = 0;
	CMD_DBDC_SETTING_T		rDbdcSetting;
	P_CMD_DBDC_SETTING_T	prCmdBody;
	WLAN_STATUS				rStatus = WLAN_STATUS_SUCCESS;
	UINT_8					ucBssIndex;
	P_BSS_INFO_T			prBssInfo;
	UINT_8					ucMaxBw;

	DBGLOG(CNM, INFO, "DBDC %s\n", fgDbdcEn ? "Enable" : "Disable");

	/* Parameter decision */
#if (CFG_HW_WMM_BY_BSS == 1)
	if (fgDbdcEn) {
		UINT_8 ucWmmSetBitmapPerBSS;
		/*
		 * As DBDC enabled, for BSS use 2.4g Band, assign related WmmGroupSet bitmask to 1.
		 * This is used to indicate the WmmGroupSet is associated to Band#1 (otherwise, use for band#0)
		 */
		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (!prBssInfo || prBssInfo->fgIsInUse == FALSE)
				continue;

			if (prBssInfo->eBand == BAND_2G4) {
				ucWmmSetBitmapPerBSS = prBssInfo->ucWmmQueSet;
				ucWmmSetBitmap |= BIT(ucWmmSetBitmapPerBSS);
			}
		}
		ucWmmSetBitmap |= BIT(MAX_HW_WMM_INDEX); /* For P2P Device*/
	}
#else
	if (fgDbdcEn)
		ucWmmSetBitmap |= BIT(DBDC_2G_WMM_INDEX);
#endif

	/* Send event to FW */
	prCmdBody = (P_CMD_DBDC_SETTING_T)&rDbdcSetting;

	kalMemZero(prCmdBody, sizeof(CMD_DBDC_SETTING_T));

	prCmdBody->ucDbdcEn = fgDbdcEn;
	prCmdBody->ucWmmBandBitmap = ucWmmSetBitmap;

	if (!fgDbdcEn) {
		rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
						  CMD_ID_SET_DBDC_PARMS,	/* ucCID */
						  TRUE, /* fgSetQuery */
						  FALSE,	/* fgNeedResp */
						  FALSE,	/* fgIsOid */
						  NULL, /* pfCmdDoneHandler */
						  NULL, /* pfCmdTimeoutHandler */
						  sizeof(CMD_DBDC_SETTING_T),	/* u4SetQueryInfoLen */
						  (PUINT_8)prCmdBody,	/* pucInfoBuffer */
						  NULL, /* pvSetQueryBuffer */
						  0 /* u4SetQueryBufferLen */);
	}

	for (ucBssIndex = 0; ucBssIndex <= HW_BSSID_NUM; ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
		if (prBssInfo->fgIsInUse &&
			prBssInfo->fgIsNetActive &&
			(prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED ||
			prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {

			switch (prBssInfo->ucVhtChannelWidth) {
			case VHT_OP_CHANNEL_WIDTH_80P80:
				ucMaxBw = MAX_BW_160MHZ;
				break;

			case VHT_OP_CHANNEL_WIDTH_160:
				ucMaxBw = MAX_BW_160MHZ;
				break;

			case VHT_OP_CHANNEL_WIDTH_80:
				ucMaxBw = MAX_BW_80MHZ;
				break;

			case VHT_OP_CHANNEL_WIDTH_20_40:
			default:
				{
					ucMaxBw = MAX_BW_20MHZ;

					if (prBssInfo->eBssSCO != CHNL_EXT_SCN)
						ucMaxBw = MAX_BW_40MHZ;
				}
				break;
			}

			if (fgDbdcEn) {
				DBGLOG(CNM, INFO, "BSS index[%u] to 1SS\n", ucBssIndex);
				rlmChangeOperationMode(prAdapter,
							ucBssIndex,
							ucMaxBw,
							1);
			} else {
				DBGLOG(CNM, INFO, "BSS index[%u] to %uSS\n",
					ucBssIndex, wlanGetSupportNss(prAdapter, ucBssIndex));
				rlmChangeOperationMode(prAdapter,
							ucBssIndex,
							ucMaxBw,
							wlanGetSupportNss(prAdapter, ucBssIndex));
			}
		}
	}

	if (fgDbdcEn) {
		rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
					      CMD_ID_SET_DBDC_PARMS,	/* ucCID */
					      TRUE,	/* fgSetQuery */
					      FALSE,	/* fgNeedResp */
					      FALSE,	/* fgIsOid */
					      NULL,	/* pfCmdDoneHandler */
					      NULL,	/* pfCmdTimeoutHandler */
					      sizeof(CMD_DBDC_SETTING_T),	/* u4SetQueryInfoLen */
					      (PUINT_8)prCmdBody,	/* pucInfoBuffer */
					      NULL,	/* pvSetQueryBuffer */
					      0	/* u4SetQueryBufferLen */);
	}

	/*DBGLOG(CNM, INFO, "DBDC CMD status %u\n", rStatus);*/

	if (rStatus == WLAN_STATUS_SUCCESS || rStatus == WLAN_STATUS_PENDING) {
		prAdapter->rWifiVar.fgDbDcModeEn = fgDbdcEn;

		for (ucBssIndex = 0; ucBssIndex <= HW_BSSID_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (prBssInfo->eBand == BAND_2G4) {
				if (fgDbdcEn)
					prBssInfo->eDBDCBand = ENUM_BAND_1;
				else
					prBssInfo->eDBDCBand = ENUM_BAND_0;
			}
		}
	}
}

VOID cnmGetDbdcCapability(
	IN P_ADAPTER_T			prAdapter,
	IN UINT_8				ucBssIndex,
	IN ENUM_BAND_T			eRfBand,
	IN UINT_8				ucPrimaryChannel,
	IN UINT_8				ucNss,
	OUT P_CNM_DBDC_CAP_T	prDbdcCap
)
{
	if (!prDbdcCap)
		return;

	/* BSS index */
	prDbdcCap->ucBssIndex = ucBssIndex;

#if (CFG_HW_WMM_BY_BSS == 0)
	/* WMM set */
	if (eRfBand == BAND_5G)
		prDbdcCap->ucWmmSetIndex = DBDC_5G_WMM_INDEX;
	else
		prDbdcCap->ucWmmSetIndex =
			(prAdapter->rWifiVar.ucDbdcMode == DBDC_MODE_DISABLED) ? DBDC_5G_WMM_INDEX : DBDC_2G_WMM_INDEX;
#endif

	/* Nss & band 0/1 */
	switch (prAdapter->rWifiVar.ucDbdcMode) {
	case DBDC_MODE_DISABLED:
		/* DBDC is disabled, all BSS run on band 0 */
		if (wlanGetSupportNss(prAdapter, ucBssIndex) < ucNss)
			prDbdcCap->ucNss = wlanGetSupportNss(prAdapter, ucBssIndex);
		else
			prDbdcCap->ucNss = ucNss;
		break;

	case DBDC_MODE_STATIC:
		/* Static DBDC mode, 1SS only */
		prDbdcCap->ucNss = 1;
		break;

	case DBDC_MODE_DYNAMIC:
		if (prAdapter->rWifiVar.fgDbDcModeEn)
			prDbdcCap->ucNss = 1;
		else
			prDbdcCap->ucNss = wlanGetSupportNss(prAdapter, ucBssIndex);
		break;

	default:
		break;
	}
}

VOID cnmDbdcEnableDecision(
	IN P_ADAPTER_T	prAdapter,
	IN UINT_8		ucChangedBssIndex,
	IN ENUM_BAND_T	eRfBand
)
{
	P_BSS_INFO_T	prBssInfo;
	UINT_8			ucBssIndex;

	if ((prAdapter->rWifiVar.ucDbdcMode != DBDC_MODE_DYNAMIC) &&
		(prAdapter->rWifiVar.ucDbdcMode != DBDC_MODE_STATIC))
		return;

	if (prAdapter->rWifiVar.fgDbDcModeEn) {
		if (timerPendingTimer(&prAdapter->rWifiVar.rDBDCSwitchGuardTimer)) {
			/* update timer for connection retry */
			DBGLOG(CNM, INFO, "DBDC guard time extend\n");
			cnmTimerStopTimer(prAdapter,
								&prAdapter->rWifiVar.rDBDCSwitchGuardTimer);
			cnmTimerStartTimer(prAdapter,
								&prAdapter->rWifiVar.rDBDCSwitchGuardTimer,
								DBDC_SWITCH_GUARD_TIME);
		}
		/* The DBDC is already ON, so renew WMM band information only */
		cnmUpdateDbdcSetting(prAdapter, TRUE);
		return;
	}

	if (timerPendingTimer(&prAdapter->rWifiVar.rDBDCSwitchGuardTimer))
		return;

	if (eRfBand != BAND_2G4 && eRfBand != BAND_5G)
		return;

	for (ucBssIndex = 0; ucBssIndex < HW_BSSID_NUM; ucBssIndex++) {

		if (ucBssIndex == ucChangedBssIndex)
			continue;

		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (!prBssInfo->fgIsInUse ||
			!prBssInfo->fgIsNetActive ||
			(prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED &&
			prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT))
			continue;

		if (prBssInfo->eBand != BAND_2G4 && prBssInfo->eBand != BAND_5G)
			continue;

		if (prBssInfo->eBand != eRfBand) {
			/* Enable DBDC */

			/* if disable timer exist, close it*/
			if (timerPendingTimer(&prAdapter->rWifiVar.rDBDCDisableCountdownTimer)) {
				cnmTimerStopTimer(prAdapter,
						&prAdapter->rWifiVar.rDBDCDisableCountdownTimer);
			}

			cnmUpdateDbdcSetting(prAdapter, TRUE);

			/* Start Switch Guard Time */
			cnmTimerStartTimer(prAdapter,
							&prAdapter->rWifiVar.rDBDCSwitchGuardTimer,
							DBDC_SWITCH_GUARD_TIME);
			return;
		}
	}
}

VOID cnmDbdcDisableDecision(IN P_ADAPTER_T prAdapter,	IN UINT_8 ucChangedBssIndex)
{
	P_BSS_INFO_T	prBssInfo;
	UINT_8			ucBssIndex;
	ENUM_BAND_T		eBandCompare;

	if (prAdapter->rWifiVar.ucDbdcMode != DBDC_MODE_DYNAMIC)
		return;

	if (!prAdapter->rWifiVar.fgDbDcModeEn) {
		if (timerPendingTimer(&prAdapter->rWifiVar.rDBDCSwitchGuardTimer)) {
			/* update timer for connection retry */
			DBGLOG(CNM, INFO, "DBDC guard time extend\n");
			cnmTimerStopTimer(prAdapter,
								&prAdapter->rWifiVar.rDBDCSwitchGuardTimer);
			cnmTimerStartTimer(prAdapter,
								&prAdapter->rWifiVar.rDBDCSwitchGuardTimer,
								DBDC_SWITCH_GUARD_TIME);
		}
		return;
	}

	if (timerPendingTimer(&prAdapter->rWifiVar.rDBDCDisableCountdownTimer))
		return;

	if (timerPendingTimer(&prAdapter->rWifiVar.rDBDCSwitchGuardTimer))
		return;

	eBandCompare = BAND_NULL;
	for (ucBssIndex = 0; ucBssIndex < HW_BSSID_NUM; ucBssIndex++) {

		if (ucBssIndex == ucChangedBssIndex)
			continue;

		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (!prBssInfo->fgIsInUse ||
			!prBssInfo->fgIsNetActive ||
			(prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED &&
			prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT))
			continue;

		if (prBssInfo->eBand != BAND_2G4 && prBssInfo->eBand != BAND_5G)
			continue;

		if (eBandCompare == BAND_NULL) {
			eBandCompare = prBssInfo->eBand;
			continue;
		}

		if (prBssInfo->eBand != eBandCompare) {
			/*no need to disable DBDC*/
			return;
		}
	}

	/* start DBDC disable countdown timer */
	cnmTimerStartTimer(prAdapter,
						&prAdapter->rWifiVar.rDBDCDisableCountdownTimer,
						DBDC_DISABLE_COUNTDOWN_TIME);
}


VOID cnmDbdcDecision(IN P_ADAPTER_T prAdapter, IN ULONG plParamPtr)
{
	P_BSS_INFO_T	prBssInfo;
	UINT_8			ucBssIndex;
	ENUM_BAND_T		eBandCompare;

	if (prAdapter->rWifiVar.ucDbdcMode != DBDC_MODE_DYNAMIC)
		return;

	/*	MTK coding style limitation of "(A=B)?:"
	*	1. one line is too long
	*	2. cut in ?/: will encounter
	*		a. ?/: need to add one space after symbol
	*		b. line end could not have space
	*	so duplicated the code...
	*/
	if (plParamPtr == DBDC_DECISION_TIMER_SWITCH_GUARD_TIME)
		DBGLOG(CNM, INFO, "DBDC timer timeout : switch guard time end\n");
	else
		DBGLOG(CNM, INFO, "DBDC timer timeout : disable countdown finish\n");

	eBandCompare = BAND_NULL;
	for (ucBssIndex = 0; ucBssIndex < HW_BSSID_NUM; ucBssIndex++) {

		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (!prBssInfo->fgIsInUse ||
			!prBssInfo->fgIsNetActive ||
			(prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED &&
			prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT))
			continue;

		if (prBssInfo->eBand != BAND_2G4 && prBssInfo->eBand != BAND_5G)
			continue;

		if (eBandCompare == BAND_NULL) {
			eBandCompare = prBssInfo->eBand;
			continue;
		}

		if (prBssInfo->eBand != eBandCompare) {
			if (!prAdapter->rWifiVar.fgDbDcModeEn) {
				/* DBDC Enable */
				cnmUpdateDbdcSetting(prAdapter, TRUE);
				cnmTimerStartTimer(prAdapter,
								&prAdapter->rWifiVar.rDBDCSwitchGuardTimer,
								DBDC_SWITCH_GUARD_TIME);
			}
			return;
		}
	}

	if (prAdapter->rWifiVar.fgDbDcModeEn) {
		/* DBDC Disable */
		cnmUpdateDbdcSetting(prAdapter, FALSE);
		cnmTimerStartTimer(prAdapter, &prAdapter->rWifiVar.rDBDCSwitchGuardTimer, DBDC_SWITCH_GUARD_TIME);
		return;
	}
}

#endif /*CFG_SUPPORT_DBDC*/

#if (CFG_HW_WMM_BY_BSS == 1)
/*----------------------------------------------------------------------------*/
/*!
* @brief    Search available HW WMM index.
*
* @param (none)
*
* @return
*/
/*----------------------------------------------------------------------------*/
UINT_8 cnmWmmIndexDecision(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo)
{
	UINT_8 ucWmmIndex;

	for (ucWmmIndex = 0; ucWmmIndex < HW_WMM_NUM; ucWmmIndex++) {
		if (prBssInfo && prBssInfo->fgIsInUse && prBssInfo->fgIsWmmInited == FALSE) {
			if (!(prAdapter->ucHwWmmEnBit & BIT(ucWmmIndex))) {
				prAdapter->ucHwWmmEnBit |= BIT(ucWmmIndex);
				prBssInfo->fgIsWmmInited = TRUE;
				break;
			}
		}
	}
	return (ucWmmIndex < HW_WMM_NUM) ? ucWmmIndex : MAX_HW_WMM_INDEX;
}
/*----------------------------------------------------------------------------*/
/*!
* @brief    Free BSS HW WMM index.
*
* @param (none)
*
* @return None
*/
/*----------------------------------------------------------------------------*/
VOID cnmFreeWmmIndex(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prBssInfo)
{
	prAdapter->ucHwWmmEnBit &= (~BIT(prBssInfo->ucWmmQueSet));
	prBssInfo->ucWmmQueSet = DEFAULT_HW_WMM_INDEX;
	prBssInfo->fgIsWmmInited = FALSE;
}
#endif /* #if (CFG_HW_WMM_BY_BSS == 1) */
/*----------------------------------------------------------------------------*/
/*!
* @brief    Release pending timer.
*
* @param (none)
*
* @return None
*/
/*----------------------------------------------------------------------------*/
VOID cnmCheckPendingTimer(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	if (prAdapter == NULL)
		return;

	/* Timer 1: rJoinTimeoutTimer
	 * Driver couldn't get any CH_GRANT event of CH_REQ after resume
	 * Because pending AIS join timer should do CH_ABORT to FW.
	 * Without CH_ABORT cmd, FW CNM's FSM would keep in GRANT stage.
	 * FW's CNM couldn't service any other CH_REQ in GRANT stage.
	 * As a result, checking the timer in suspend flow.
	 */
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (timerPendingTimer(&prAisFsmInfo->rJoinTimeoutTimer)) {
		DBGLOG(CNM, STATE, "[AIS] pending rJoinTimeoutTimer\n");
		cnmTimerStopTimer(prAdapter,
			&prAisFsmInfo->rJoinTimeoutTimer);
		/* Release Channel */
		aisFsmReleaseCh(prAdapter);
		prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
	}
}
