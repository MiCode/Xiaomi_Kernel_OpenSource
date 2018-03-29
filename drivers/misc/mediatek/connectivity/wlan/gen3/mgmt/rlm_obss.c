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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/rlm_obss.c#2
*/

/*! \file   "rlm_obss.c"
    \brief

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
static VOID rlmObssScanTimeout(P_ADAPTER_T prAdapter, ULONG ulParamPtr);

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
VOID rlmObssInit(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		cnmTimerInitTimer(prAdapter, &prBssInfo->rObssScanTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) rlmObssScanTimeout, (ULONG) prBssInfo);
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
BOOLEAN rlmObssUpdateChnlLists(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	return TRUE;
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
VOID rlmObssScanDone(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;
	P_BSS_INFO_T prBssInfo;
	P_MSDU_INFO_T prMsduInfo;
	P_ACTION_20_40_COEXIST_FRAME prTxFrame;
	UINT_16 i, u2PayloadLen;

	ASSERT(prMsgHdr);

	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;
	prBssInfo = prAdapter->aprBssInfo[prScanDoneMsg->ucBssIndex];
	ASSERT(prBssInfo);

	DBGLOG(RLM, INFO, "OBSS Scan Done (NetIdx=%d, Mode=%d)\n",
			   prScanDoneMsg->ucBssIndex, prBssInfo->eCurrentOPMode);

	cnmMemFree(prAdapter, prMsgHdr);

#if CFG_ENABLE_WIFI_DIRECT
	/* AP mode */
	if ((prAdapter->fgIsP2PRegistered) &&
	    (IS_NET_ACTIVE(prAdapter, prBssInfo->ucBssIndex)) && (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
		return;
	}
#endif

	/* STA mode */
	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
	    !RLM_NET_PARAM_VALID(prBssInfo) || prBssInfo->u2ObssScanInterval == 0) {
		DBGLOG(RLM, WARN, "OBSS Scan Done (NetIdx=%d) -- Aborted!!\n", prBssInfo->ucBssIndex);
		return;
	}

	/* To do: check 2.4G channel list to decide if obss mgmt should be
	 *        sent to associated AP. Note: how to handle concurrent network?
	 * To do: invoke rlmObssChnlLevel() to decide if 20/40 BSS coexistence
	 *        management frame is needed.
	 */
	if (prBssInfo->auc2G_20mReqChnlList[0] > 0 || prBssInfo->auc2G_NonHtChnlList[0] > 0) {

		DBGLOG(RLM, INFO, "Send 20/40 coexistence mgmt(20mReq=%d, NonHt=%d)\n",
				   prBssInfo->auc2G_20mReqChnlList[0], prBssInfo->auc2G_NonHtChnlList[0]);

		prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter, MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

		if (prMsduInfo) {
			prTxFrame = (P_ACTION_20_40_COEXIST_FRAME)
			    ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

			prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
			COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
			COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
			COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

			prTxFrame->ucCategory = CATEGORY_PUBLIC_ACTION;
			prTxFrame->ucAction = ACTION_PUBLIC_20_40_COEXIST;

			/* To do: find correct algorithm */
			prTxFrame->rBssCoexist.ucId = ELEM_ID_20_40_BSS_COEXISTENCE;
			prTxFrame->rBssCoexist.ucLength = 1;
			prTxFrame->rBssCoexist.ucData =
				(prBssInfo->auc2G_20mReqChnlList[0] > 0) ? BSS_COEXIST_20M_REQ : 0;

			u2PayloadLen = 2 + 3;

			if (prBssInfo->auc2G_NonHtChnlList[0] > 0) {
				ASSERT(prBssInfo->auc2G_NonHtChnlList[0] <= CHNL_LIST_SZ_2G);

				prTxFrame->rChnlReport.ucId = ELEM_ID_20_40_INTOLERANT_CHNL_REPORT;
				prTxFrame->rChnlReport.ucLength = prBssInfo->auc2G_NonHtChnlList[0] + 1;
				prTxFrame->rChnlReport.ucRegulatoryClass = 81;	/* 2.4GHz, ch1~13 */
				for (i = 0; i < prBssInfo->auc2G_NonHtChnlList[0] && i < CHNL_LIST_SZ_2G; i++)
					prTxFrame->rChnlReport.aucChannelList[i] =
										prBssInfo->auc2G_NonHtChnlList[i + 1];

				u2PayloadLen += IE_SIZE(&prTxFrame->rChnlReport);
			}
			ASSERT((WLAN_MAC_HEADER_LEN + u2PayloadLen) <= PUBLIC_ACTION_MAX_LEN);

			/* Clear up channel lists in 2.4G band */
			prBssInfo->auc2G_20mReqChnlList[0] = 0;
			prBssInfo->auc2G_NonHtChnlList[0] = 0;

			/* 4 Update information of MSDU_INFO_T */

			TX_SET_MMPDU(prAdapter,
				     prMsduInfo,
				     prBssInfo->ucBssIndex,
				     prBssInfo->prStaRecOfAP->ucIndex,
				     WLAN_MAC_MGMT_HEADER_LEN,
				     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL, MSDU_RATE_MODE_AUTO);

			/* 4 Enqueue the frame to send this action frame. */
			nicTxEnqueueMsdu(prAdapter, prMsduInfo);
		}
	}
	/* end of prMsduInfo != NULL */
	if (prBssInfo->u2ObssScanInterval > 0) {
		DBGLOG(RLM, INFO, "Set OBSS timer (NetIdx=%d, %d sec)\n",
				   prBssInfo->ucBssIndex, prBssInfo->u2ObssScanInterval);

		cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer, prBssInfo->u2ObssScanInterval * MSEC_PER_SEC);
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
static VOID rlmObssScanTimeout(P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_BSS_INFO_T prBssInfo;

	prBssInfo = (P_BSS_INFO_T) ulParamPtr;
	ASSERT(prBssInfo);

#if CFG_ENABLE_WIFI_DIRECT
	/* AP mode */
	if (prAdapter->fgIsP2PRegistered &&
	    (IS_NET_ACTIVE(prAdapter, prBssInfo->ucBssIndex)) && (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {

		prBssInfo->fgObssActionForcedTo20M = FALSE;

		/* Check if Beacon content need to be updated */
		rlmUpdateParamsForAP(prAdapter, prBssInfo, FALSE);

		return;
	}
#if CFG_SUPPORT_WFD
	/* WFD streaming */
	else {
		P_WFD_CFG_SETTINGS_T prWfdCfgSettings = &prAdapter->rWifiVar.rWfdConfigureSettings;

		/* If WFD is enabled & connected */
		if (prWfdCfgSettings->ucWfdEnable) {

			/* Skip OBSS scan */
			prBssInfo->u2ObssScanInterval = 0;
			DBGLOG(RLM, INFO, "WFD is running. Stop OBSS scan.\n");
			return;
		}		/* WFD is enabled */
	}
#endif
#endif /* end of CFG_ENABLE_WIFI_DIRECT */

	/* STA mode */
	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
	    !RLM_NET_PARAM_VALID(prBssInfo) || prBssInfo->u2ObssScanInterval == 0) {
		DBGLOG(RLM, WARN, "OBSS Scan timeout (NetIdx=%d) -- Aborted!!\n", prBssInfo->ucBssIndex);
		return;
	}

	rlmObssTriggerScan(prAdapter, prBssInfo);
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
VOID rlmObssTriggerScan(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	P_MSG_SCN_SCAN_REQ prScanReqMsg;

	ASSERT(prBssInfo);

	prScanReqMsg = (P_MSG_SCN_SCAN_REQ)
	    cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_REQ));
	ASSERT(prScanReqMsg);

	if (!prScanReqMsg) {
		DBGLOG(RLM, WARN, "No buf for OBSS scan (NetIdx=%d)!!\n", prBssInfo->ucBssIndex);

		cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer, prBssInfo->u2ObssScanInterval * MSEC_PER_SEC);
		return;
	}

	/* It is ok that ucSeqNum is set to fixed value because the same network
	 * OBSS scan interval is limited to OBSS_SCAN_MIN_INTERVAL (min 10 sec)
	 * and scan module don't care seqNum of OBSS scanning
	 */
	prScanReqMsg->rMsgHdr.eMsgId = MID_RLM_SCN_SCAN_REQ;
	prScanReqMsg->ucSeqNum = 0x33;
	prScanReqMsg->ucBssIndex = prBssInfo->ucBssIndex;
	prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
	prScanReqMsg->ucSSIDLength = 0;
	prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
	prScanReqMsg->u2IELen = 0;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanReqMsg, MSG_SEND_METHOD_BUF);

	DBGLOG(RLM, INFO, "Timeout to trigger OBSS scan (NetIdx=%d)!!\n", prBssInfo->ucBssIndex);
}
