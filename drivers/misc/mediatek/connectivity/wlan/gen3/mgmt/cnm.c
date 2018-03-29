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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/cnm.c#2
*/

/*! \file   "cnm.c"
    \brief  Module of Concurrent Network Management

    Module of Concurrent Network Management
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
	cnmTimerInitTimer(prAdapter, &prCnmInfo->rReqChnlUtilTimer,
				  (PFN_MGMT_TIMEOUT_FUNC)cnmRunEventReqChnlUtilTimeout, (ULONG) NULL);

}				/* end of cnmInit() */

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
	cnmTimerStopTimer(prAdapter, &prAdapter->rCnmInfo.rReqChnlUtilTimer);
}				/* end of cnmUninit() */

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

	prMsgChReq = (P_MSG_CH_REQ_T) prMsgHdr;

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

	DBGLOG(CNM, INFO, "ChReq net=%d token=%d b=%d c=%d s=%d w=%d s1=%d s2=%d\n",
			   prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID,
			   prMsgChReq->eRfBand, prMsgChReq->ucPrimaryChannel,
			   prMsgChReq->eRfSco, prMsgChReq->eRfChannelWidth,
			   prMsgChReq->ucRfCenterFreqSeg1, prMsgChReq->ucRfCenterFreqSeg2);

	prCmdBody->ucBssIndex = prMsgChReq->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChReq->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_REQ;	/* Request */
	prCmdBody->ucPrimaryChannel = prMsgChReq->ucPrimaryChannel;
	prCmdBody->ucRfSco = (UINT_8) prMsgChReq->eRfSco;
	prCmdBody->ucRfBand = (UINT_8) prMsgChReq->eRfBand;
	prCmdBody->ucRfChannelWidth = (UINT_8) prMsgChReq->eRfChannelWidth;
	prCmdBody->ucRfCenterFreqSeg1 = (UINT_8) prMsgChReq->ucRfCenterFreqSeg1;
	prCmdBody->ucRfCenterFreqSeg2 = (UINT_8) prMsgChReq->ucRfCenterFreqSeg2;
	prCmdBody->ucReqType = (UINT_8) prMsgChReq->eReqType;
	prCmdBody->aucReserved[0] = 0;
	prCmdBody->aucReserved[1] = 0;
	prCmdBody->u4MaxInterval = prMsgChReq->u4MaxInterval;

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
				      (PUINT_8) prCmdBody,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);

}				/* end of cnmChMngrRequestPrivilege() */

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

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChAbort = (P_MSG_CH_ABORT_T) prMsgHdr;

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

	DBGLOG(CNM, INFO, "ChAbort net=%d token=%d\n", prCmdBody->ucBssIndex, prCmdBody->ucTokenID);

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
				      (PUINT_8) prCmdBody,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
	    );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);

}				/* end of cnmChMngrAbortPrivilege() */

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

	prEventBody = (P_EVENT_CH_PRIVILEGE_T) (prEvent->aucBuffer);
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
			   prEventBody->ucBssIndex, prEventBody->ucTokenID,
			   prEventBody->ucPrimaryChannel, prEventBody->ucRfSco);

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
	prChResp->eRfSco = (ENUM_CHNL_EXT_T) prEventBody->ucRfSco;
	prChResp->eRfBand = (ENUM_BAND_T) prEventBody->ucRfBand;
	prChResp->eRfChannelWidth = (ENUM_CHANNEL_WIDTH_T) prEventBody->ucRfChannelWidth;
	prChResp->ucRfCenterFreqSeg1 = prEventBody->ucRfCenterFreqSeg1;
	prChResp->ucRfCenterFreqSeg2 = prEventBody->ucRfCenterFreqSeg2;
	prChResp->eReqType = (ENUM_CH_REQ_TYPE_T) prEventBody->ucReqType;
	prChResp->u4GrantInterval = prEventBody->u4GrantInterval;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prChResp, MSG_SEND_METHOD_BUF);

	/* Record current granted BSS for TXM's reference */
	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->ucBssIndex = prEventBody->ucBssIndex;
	prCnmInfo->ucTokenID = prEventBody->ucTokenID;
	prCnmInfo->fgChGranted = TRUE;
}

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

	ASSERT(prAdapter);

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

#if 0
		DBGLOG(INIT, INFO, "%s BSS[%u] active[%u] netType[%u]\n",
				    __func__, i, prBssInfo->fgIsNetActive, prBssInfo->eNetworkType;
#endif

		if (!IS_NET_ACTIVE(prAdapter, i))
			continue;

#if CFG_ENABLE_WIFI_DIRECT
		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			BOOLEAN fgFixedChannel = p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings);

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
	UINT_8 ucAPBandwidth = MAX_BW_80MHZ;
	P_BSS_DESC_T    prBssDesc = NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	/* Currently we only support 2 p2p interface. So the RoleIndex is 0. */
	prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[0];

	if (IS_BSS_AIS(prBssInfo)) {
		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
	} else if (IS_BSS_P2P(prBssInfo)) {
		/* P2P mode */
		if (!p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			if (prP2pRoleFsmInfo)
				prBssDesc = prP2pRoleFsmInfo->rJoinInfo.prTargetBssDesc;
		}
	}

	if (prBssDesc) {
		if (prBssDesc->eChannelWidth == CW_20_40MHZ) {
			if ((prBssDesc->eSco == CHNL_EXT_SCA) || (prBssDesc->eSco == CHNL_EXT_SCB))
				ucAPBandwidth = MAX_BW_40MHZ;
			else
				ucAPBandwidth = MAX_BW_20MHZ;
		}
#if (CFG_FORCE_USE_20BW == 1)
		if (prBssDesc->eBand == BAND_2G4)
			ucAPBandwidth = MAX_BW_20MHZ;
#endif
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
	UINT_8 ucAPBandwidth;

	ASSERT(prAdapter);

	/* Note: To support real-time decision instead of current activated-time,
	 *       the STA roaming case shall be considered about synchronization
	 *       problem. Another variable fgAssoc40mBwAllowed is added to
	 *       represent HT capability when association
	 */

	ucAPBandwidth = cnmGetAPBwPermitted(prAdapter, ucBssIndex);

	/* Decide max bandwidth by feature option */
	if ((cnmGetBssMaxBw(prAdapter, ucBssIndex) < MAX_BW_40MHZ) || (ucAPBandwidth < MAX_BW_40MHZ))
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

BOOLEAN cnmBss40mBwPermittedForJoin(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	UINT_8 ucAPBandwidth;
	P_BSS_DESC_T prBssDesc = NULL;
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucMaxBandwidth = MAX_BW_80MHZ;

	ASSERT(prAdapter);

	ucAPBandwidth = cnmGetAPBwPermitted(prAdapter, ucBssIndex);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo)) {
		/* STA mode */
		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		if (prBssDesc->eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		/* AP mode */
		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp5gBandwidth;
		}
		/* P2P mode */
		else {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p5gBandwidth;
		}
	}

	/* Decide max bandwidth by feature option */
	if ((ucMaxBandwidth < MAX_BW_40MHZ) || (ucAPBandwidth < MAX_BW_40MHZ))
		return FALSE;

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
	UINT_8 ucAPBandwidth;

	ASSERT(prAdapter);

	/* Note: To support real-time decision instead of current activated-time,
	 *       the STA roaming case shall be considered about synchronization
	 *       problem. Another variable fgAssoc40mBwAllowed is added to
	 *       represent HT capability when association
	 */

	ucAPBandwidth = cnmGetAPBwPermitted(prAdapter, ucBssIndex);

	/* Decide max bandwidth by feature option */
	if ((cnmGetBssMaxBw(prAdapter, ucBssIndex) < MAX_BW_80MHZ) || (ucAPBandwidth < MAX_BW_80MHZ))
		return FALSE;

	return TRUE;
}

UINT_8 cnmGetBssMaxBw(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucMaxBandwidth = MAX_BW_80MHZ;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo)) {
		/* STA mode */
		if (prBssInfo->eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		/* AP mode */
		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp5gBandwidth;
		}
		/* P2P mode */
		else {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p5gBandwidth;
		}
	}

	return ucMaxBandwidth;
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
	UINT_8 ucBssIndex, ucOwnMacIdx;

	ASSERT(prAdapter);

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
		prBssInfo->ucKeyCmdAction = SEC_TX_KEY_COMMAND;
		return prBssInfo;
	}

	ucOwnMacIdx = (eNetworkType == NETWORK_TYPE_MBSS) ? 0 : 1;

	/* Find available HW set */
	do {
		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (prBssInfo && prBssInfo->fgIsInUse && ucOwnMacIdx == prBssInfo->ucOwnMacIndex)
				break;
		}

		if (ucBssIndex >= BSS_INFO_NUM)
			break;	/* No hit */
	} while (++ucOwnMacIdx < HW_BSSID_NUM);

	/* Find available BSS_INFO */
	for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (prBssInfo && !prBssInfo->fgIsInUse) {
			prBssInfo->fgIsInUse = TRUE;
			prBssInfo->ucBssIndex = ucBssIndex;
			prBssInfo->eNetworkType = eNetworkType;
			prBssInfo->ucOwnMacIndex = ucOwnMacIdx;
			break;
		}
	}

	if (ucOwnMacIdx >= HW_BSSID_NUM || ucBssIndex >= BSS_INFO_NUM)
		prBssInfo = NULL;
#if CFG_SUPPORT_PNO
	if (prBssInfo) {
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
	}
#endif
	if (prBssInfo)
		prBssInfo->ucKeyCmdAction = SEC_TX_KEY_COMMAND;
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

VOID cnmRunEventReqChnlUtilTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_CNM_INFO_T prCnmInfo = &prAdapter->rCnmInfo;
	struct MSG_CH_UTIL_RSP *prMsgChUtil = NULL;
	P_MSG_SCN_SCAN_REQ prScanReqMsg = NULL;

	DBGLOG(CNM, INFO, "Request Channel Utilization timeout\n");
	wlanReleasePendingCmdById(prAdapter, CMD_ID_REQ_CHNL_UTILIZATION);
	prMsgChUtil = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prMsgChUtil));
	kalMemZero(prMsgChUtil, sizeof(*prMsgChUtil));
	prMsgChUtil->rMsgHdr.eMsgId = prCnmInfo->u2ReturnMID;
	prMsgChUtil->ucChnlNum = 0;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgChUtil, MSG_SEND_METHOD_BUF);
	/* tell scan_fsm to continue to process scan request, if there's any pending */
	prScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prScanReqMsg));
	kalMemZero(prScanReqMsg, sizeof(*prScanReqMsg));
	prScanReqMsg->rMsgHdr.eMsgId = MID_MNY_CNM_SCAN_CONTINUE;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prScanReqMsg, MSG_SEND_METHOD_BUF);
}

VOID cnmHandleChannelUtilization(P_ADAPTER_T prAdapter,
	struct EVENT_RSP_CHNL_UTILIZATION *prChnlUtil)
{
	P_CNM_INFO_T prCnmInfo = &prAdapter->rCnmInfo;
	struct MSG_CH_UTIL_RSP *prMsgChUtil = NULL;
	P_MSG_SCN_SCAN_REQ prScanReqMsg = NULL;

	if (!timerPendingTimer(&prCnmInfo->rReqChnlUtilTimer))
		return;
	prMsgChUtil = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prMsgChUtil));
	if (!prMsgChUtil) {
		DBGLOG(CNM, ERROR, "No memory!");
		return;
	}
	DBGLOG(CNM, INFO, "Receive Channel Utilization response\n");
	cnmTimerStopTimer(prAdapter, &prCnmInfo->rReqChnlUtilTimer);
	kalMemZero(prMsgChUtil, sizeof(*prMsgChUtil));
	prMsgChUtil->rMsgHdr.eMsgId = prCnmInfo->u2ReturnMID;
	prMsgChUtil->ucChnlNum = prChnlUtil->ucChannelNum;
	kalMemCopy(prMsgChUtil->aucChnlList, prChnlUtil->aucChannelMeasureList, prChnlUtil->ucChannelNum);
	kalMemCopy(prMsgChUtil->aucChUtil, prChnlUtil->aucChannelUtilization, prChnlUtil->ucChannelNum);
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgChUtil, MSG_SEND_METHOD_BUF);
	prScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prScanReqMsg));
	kalMemZero(prScanReqMsg, sizeof(*prScanReqMsg));
	prScanReqMsg->rMsgHdr.eMsgId = MID_MNY_CNM_SCAN_CONTINUE;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prScanReqMsg, MSG_SEND_METHOD_BUF);
}

VOID cnmRequestChannelUtilization(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_CNM_INFO_T prCnmInfo = &prAdapter->rCnmInfo;
	struct MSG_REQ_CH_UTIL *prMsgReqChUtil = (struct MSG_REQ_CH_UTIL *)prMsgHdr;
	struct CMD_REQ_CHNL_UTILIZATION rChnlUtilCmd;

	if (!prMsgReqChUtil)
		return;
	if (timerPendingTimer(&prCnmInfo->rReqChnlUtilTimer)) {
		cnmMemFree(prAdapter, prMsgReqChUtil);
		return;
	}
	DBGLOG(CNM, INFO, "Request Channel Utilization, channel count %d\n", prMsgReqChUtil->ucChnlNum);
	kalMemZero(&rChnlUtilCmd, sizeof(rChnlUtilCmd));
	prCnmInfo->u2ReturnMID = prMsgReqChUtil->u2ReturnMID;
	rChnlUtilCmd.u2MeasureDuration = prMsgReqChUtil->u2Duration;
	if (prMsgReqChUtil->ucChnlNum > 9)
		prMsgReqChUtil->ucChnlNum = 9;
	rChnlUtilCmd.ucChannelNum = prMsgReqChUtil->ucChnlNum;
	kalMemCopy(rChnlUtilCmd.aucChannelList, prMsgReqChUtil->aucChnlList, rChnlUtilCmd.ucChannelNum);
	cnmMemFree(prAdapter, prMsgReqChUtil);
	rStatus = wlanSendSetQueryCmd(
				prAdapter,                  /* prAdapter */
				CMD_ID_REQ_CHNL_UTILIZATION,/* ucCID */
				TRUE,                       /* fgSetQuery */
				FALSE,                      /* fgNeedResp */
				FALSE,                       /* fgIsOid */
				nicCmdEventSetCommon,		/* pfCmdDoneHandler*/
				nicOidCmdTimeoutCommon,		/* pfCmdTimeoutHandler */
				sizeof(rChnlUtilCmd),/* u4SetQueryInfoLen */
				(PUINT_8)&rChnlUtilCmd,      /* pucInfoBuffer */
				NULL,                       /* pvSetQueryBuffer */
				0                           /* u4SetQueryBufferLen */
				);
	cnmTimerStartTimer(prAdapter, &prCnmInfo->rReqChnlUtilTimer, 1000);
}

BOOLEAN cnmChUtilIsRunning(P_ADAPTER_T prAdapter)
{
	return timerPendingTimer(&prAdapter->rCnmInfo.rReqChnlUtilTimer);
}
