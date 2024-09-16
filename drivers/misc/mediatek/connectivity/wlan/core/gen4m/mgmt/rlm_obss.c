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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/rlm_obss.c#2
 */

/*! \file   "rlm_obss.c"
 *    \brief
 *
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static void rlmObssScanTimeout(struct ADAPTER *prAdapter,
			       unsigned long ulParamPtr);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
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
void rlmObssInit(struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;

	ASSERT(prAdapter);

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		cnmTimerInitTimer(prAdapter, &prBssInfo->rObssScanTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) rlmObssScanTimeout,
				  (unsigned long) prBssInfo);
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
u_int8_t rlmObssUpdateChnlLists(struct ADAPTER *prAdapter,
				struct SW_RFB *prSwRfb)
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
void rlmObssScanDone(struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct MSG_SCN_SCAN_DONE *prScanDoneMsg;
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_20_40_COEXIST_FRAME *prTxFrame;
	uint16_t i, u2PayloadLen;

	ASSERT(prMsgHdr);

	prScanDoneMsg = (struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	prBssInfo = prAdapter->aprBssInfo[prScanDoneMsg->ucBssIndex];
	ASSERT(prBssInfo);

	DBGLOG(RLM, INFO, "OBSS Scan Done (NetIdx=%d, Mode=%d)\n",
	       prScanDoneMsg->ucBssIndex, prBssInfo->eCurrentOPMode);

	cnmMemFree(prAdapter, prMsgHdr);

#if CFG_ENABLE_WIFI_DIRECT
	/* AP mode */
	if ((prAdapter->fgIsP2PRegistered) &&
	    (IS_NET_ACTIVE(prAdapter, prBssInfo->ucBssIndex)) &&
	     (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
		return;
	}
#endif

	/* STA mode */
	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
	    !RLM_NET_PARAM_VALID(prBssInfo) ||
	    prBssInfo->u2ObssScanInterval == 0) {
		DBGLOG(RLM, WARN, "OBSS Scan Done (NetIdx=%d) -- Aborted!!\n",
		       prBssInfo->ucBssIndex);
		return;
	}

	/* To do: check 2.4G channel list to decide if obss mgmt should be
	 *        sent to associated AP. Note: how to handle concurrent network?
	 * To do: invoke rlmObssChnlLevel() to decide if 20/40 BSS coexistence
	 *        management frame is needed.
	 */
	if (prBssInfo->auc2G_20mReqChnlList[0] > 0 ||
	    prBssInfo->auc2G_NonHtChnlList[0] > 0) {

		DBGLOG(RLM, INFO,
		       "Send 20/40 coexistence mgmt(20mReq=%d, NonHt=%d)\n",
		       prBssInfo->auc2G_20mReqChnlList[0],
		       prBssInfo->auc2G_NonHtChnlList[0]);

		prMsduInfo = (struct MSDU_INFO *) cnmMgtPktAlloc(prAdapter,
				MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN);

		if (prMsduInfo) {
			prTxFrame = (struct ACTION_20_40_COEXIST_FRAME *)
			    ((unsigned long) (prMsduInfo->prPacket) +
			     MAC_TX_RESERVED_FIELD);

			prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
			COPY_MAC_ADDR(prTxFrame->aucDestAddr,
				      prBssInfo->aucBSSID);
			COPY_MAC_ADDR(prTxFrame->aucSrcAddr,
				      prBssInfo->aucOwnMacAddr);
			COPY_MAC_ADDR(prTxFrame->aucBSSID,
				      prBssInfo->aucBSSID);

			prTxFrame->ucCategory = CATEGORY_PUBLIC_ACTION;
			prTxFrame->ucAction = ACTION_PUBLIC_20_40_COEXIST;

			/* To do: find correct algorithm */
			prTxFrame->rBssCoexist.ucId =
						ELEM_ID_20_40_BSS_COEXISTENCE;
			prTxFrame->rBssCoexist.ucLength = 1;
			prTxFrame->rBssCoexist.ucData =
			    (prBssInfo->auc2G_20mReqChnlList[0] > 0) ?
			    BSS_COEXIST_20M_REQ : 0;

			u2PayloadLen = 2 + 3;

			if (prBssInfo->auc2G_NonHtChnlList[0] > 0) {
				ASSERT(prBssInfo->auc2G_NonHtChnlList[0] <=
							CHNL_LIST_SZ_2G);

				prTxFrame->rChnlReport.ucId =
					ELEM_ID_20_40_INTOLERANT_CHNL_REPORT;
				prTxFrame->rChnlReport.ucLength =
					prBssInfo->auc2G_NonHtChnlList[0] + 1;
				/* 2.4GHz, ch1~13 */
				prTxFrame->rChnlReport.ucRegulatoryClass = 81;
				for (i = 0;
				     i < prBssInfo->auc2G_NonHtChnlList[0] &&
					i < CHNL_LIST_SZ_2G; i++)
					prTxFrame->rChnlReport.aucChannelList[i]
					    = prBssInfo->
						auc2G_NonHtChnlList[i + 1];

				u2PayloadLen += IE_SIZE(&prTxFrame->
								rChnlReport);
			}
			ASSERT((WLAN_MAC_HEADER_LEN + u2PayloadLen) <=
						PUBLIC_ACTION_MAX_LEN);

			/* Clear up channel lists in 2.4G band */
			prBssInfo->auc2G_20mReqChnlList[0] = 0;
			prBssInfo->auc2G_NonHtChnlList[0] = 0;

			/* 4 Update information of MSDU_INFO_T */

			TX_SET_MMPDU(prAdapter,
				     prMsduInfo,
				     prBssInfo->ucBssIndex,
				     prBssInfo->prStaRecOfAP->ucIndex,
				     WLAN_MAC_MGMT_HEADER_LEN,
				     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen,
				     NULL, MSDU_RATE_MODE_AUTO);

			/* 4 Enqueue the frame to send this action frame. */
			nicTxEnqueueMsdu(prAdapter, prMsduInfo);
		}
	}
	/* end of prMsduInfo != NULL */
	if (prBssInfo->u2ObssScanInterval > 0) {
		DBGLOG(RLM, INFO, "Set OBSS timer (NetIdx=%d, %d sec)\n",
		       prBssInfo->ucBssIndex, prBssInfo->u2ObssScanInterval);

		cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer,
				   prBssInfo->
					u2ObssScanInterval * MSEC_PER_SEC);
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
static void rlmObssScanTimeout(struct ADAPTER *prAdapter,
			       unsigned long ulParamPtr)
{
	struct BSS_INFO *prBssInfo;

	prBssInfo = (struct BSS_INFO *) ulParamPtr;
	ASSERT(prBssInfo);

#if CFG_ENABLE_WIFI_DIRECT
	/* AP mode */
	if (prAdapter->fgIsP2PRegistered &&
	    (IS_NET_ACTIVE(prAdapter, prBssInfo->ucBssIndex)) &&
	     (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {

		prBssInfo->fgObssActionForcedTo20M = FALSE;

		/* Check if Beacon content need to be updated */
		rlmUpdateParamsForAP(prAdapter, prBssInfo, FALSE);

		return;
	}
#if CFG_SUPPORT_WFD
	/* WFD streaming */
	else {
		struct WFD_CFG_SETTINGS *prWfdCfgSettings =
				&prAdapter->rWifiVar.rWfdConfigureSettings;

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
	    !RLM_NET_PARAM_VALID(prBssInfo) ||
	    prBssInfo->u2ObssScanInterval == 0) {
		DBGLOG(RLM, WARN,
		       "OBSS Scan timeout (NetIdx=%d) -- Aborted!!\n",
		       prBssInfo->ucBssIndex);
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
void rlmObssTriggerScan(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo)
{
	struct MSG_SCN_SCAN_REQ_V2 *prScanReqMsg;

	if (prBssInfo == NULL) {
		DBGLOG(RLM, WARN, "BssInfo = NULL!");
		return;
	}

	prScanReqMsg = (struct MSG_SCN_SCAN_REQ_V2 *)
	    cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			sizeof(struct MSG_SCN_SCAN_REQ_V2));

	if (!prScanReqMsg) {
		DBGLOG(RLM, WARN, "No buf for OBSS scan (NetIdx=%d)!!\n",
		       prBssInfo->ucBssIndex);

		cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer,
				   prBssInfo->
					u2ObssScanInterval * MSEC_PER_SEC);
		return;
	}

	/* It is ok that ucSeqNum is set to fixed value because the same network
	 * OBSS scan interval is limited to OBSS_SCAN_MIN_INTERVAL (min 10 sec)
	 * and scan module don't care seqNum of OBSS scanning
	 */
	kalMemZero(prScanReqMsg, sizeof(struct MSG_SCN_SCAN_REQ_V2));
	prScanReqMsg->rMsgHdr.eMsgId = MID_RLM_SCN_SCAN_REQ_V2;
	prScanReqMsg->ucSeqNum = 0x33;
	prScanReqMsg->ucBssIndex = prBssInfo->ucBssIndex;
	prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
	prScanReqMsg->ucSSIDNum = 0;
	prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
	prScanReqMsg->u2IELen = 0;

	mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prScanReqMsg,
		    MSG_SEND_METHOD_BUF);

	DBGLOG(RLM, INFO, "Timeout to trigger OBSS scan (NetIdx=%d)!!\n",
	       prBssInfo->ucBssIndex);
}
