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

u_int8_t
p2pDevStateInit_IDLE(IN struct ADAPTER *prAdapter,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		OUT enum ENUM_P2P_DEV_STATE *peNextState)
{
	u_int8_t fgIsTransition = FALSE, fgIsShareInterface = TRUE;
	uint32_t u4Idx = 0;
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prChnlReqInfo) && (peNextState != NULL));

		if (!LINK_IS_EMPTY(&(prChnlReqInfo->rP2pChnlReqLink))) {
			fgIsTransition = TRUE;
			*peNextState = P2P_DEV_STATE_REQING_CHANNEL;
			break;
		}

		/* Check the interface shared by P2P_DEV and P2P_ROLE or not? */
		/* If not shared, we shall let BSSID4 alive
		 * to receive PROVISION REQUEST from GC
		 */
		prGlueInfo = prAdapter->prGlueInfo;
		if (prGlueInfo) {
			for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
				if ((prGlueInfo->prP2PInfo[u4Idx] != NULL) &&
				(prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler
				!= NULL) &&
				(prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler
				!=
				prGlueInfo->prP2PInfo[u4Idx]->prDevHandler)) {
					fgIsShareInterface = FALSE;
					break;
				}
			}
		}
		/************************* End *************************/

		if (fgIsShareInterface) {
			/* Stay in IDLE state. */
			UNSET_NET_ACTIVE(prAdapter, prAdapter->ucP2PDevBssIdx);
			nicDeactivateNetwork(prAdapter,
				prAdapter->ucP2PDevBssIdx);
		}
	} while (FALSE);

	return fgIsTransition;
}				/* p2pDevStateInit_IDLE */

void p2pDevStateAbort_IDLE(IN struct ADAPTER *prAdapter)
{
	/* Currently Aobrt IDLE do nothing. */
}				/* p2pDevStateAbort_IDLE */

u_int8_t
p2pDevStateInit_REQING_CHANNEL(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIdx,
	IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
	OUT enum ENUM_P2P_DEV_STATE *peNextState)
{
	u_int8_t fgIsTransition = FALSE;
	struct MSG_P2P_CHNL_REQUEST *prP2pMsgChnlReq =
		(struct MSG_P2P_CHNL_REQUEST *) NULL;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
#if CFG_SUPPORT_DBDC
	struct CNM_DBDC_CAP rDbdcCap;
#endif /*CFG_SUPPORT_DBDC*/

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prChnlReqInfo != NULL) && (peNextState != NULL));

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);

		if (LINK_IS_EMPTY(&(prChnlReqInfo->rP2pChnlReqLink))) {
			/* NO Channel Request Pending. */
			DBGLOG(P2P, ERROR,
				"NO Pending Channel Request, but enter Req Channel State\n");
			fgIsTransition = TRUE;
			*peNextState = P2P_DEV_STATE_IDLE;
			break;
		}

		LINK_REMOVE_HEAD(&(prChnlReqInfo->rP2pChnlReqLink),
			prP2pMsgChnlReq, struct MSG_P2P_CHNL_REQUEST *);

		if (!prP2pMsgChnlReq)
			break;

		if (prBssInfo->fgIsWmmInited == FALSE)
			prBssInfo->ucWmmQueSet = MAX_HW_WMM_INDEX;
		prBssInfo->eBand = prP2pMsgChnlReq->rChannelInfo.eBand;

#if CFG_SUPPORT_DBDC
		kalMemZero(&rDbdcCap, sizeof(struct CNM_DBDC_CAP));

		cnmGetDbdcCapability(prAdapter,
			prBssInfo->ucBssIndex,
			prP2pMsgChnlReq->rChannelInfo.eBand,
			prP2pMsgChnlReq->rChannelInfo.ucChannelNum,
			wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex),
			&rDbdcCap);

		prBssInfo->ucNss = rDbdcCap.ucNss;
#endif /*CFG_SUPPORT_DBDC*/
		prChnlReqInfo->u4MaxInterval = prP2pMsgChnlReq->u4Duration;
		prChnlReqInfo->ucReqChnlNum =
			prP2pMsgChnlReq->rChannelInfo.ucChannelNum;
		prChnlReqInfo->eChnlSco = prP2pMsgChnlReq->eChnlSco;
		prChnlReqInfo->eBand = prP2pMsgChnlReq->rChannelInfo.eBand;
		prChnlReqInfo->u8Cookie = prP2pMsgChnlReq->u8Cookie;
		prChnlReqInfo->eChnlReqType = prP2pMsgChnlReq->eChnlReqType;
		prChnlReqInfo->eChannelWidth = prBssInfo->ucVhtChannelWidth;
		prChnlReqInfo->ucCenterFreqS1 =
			prBssInfo->ucVhtChannelFrequencyS1;
		prChnlReqInfo->ucCenterFreqS2 =
			prBssInfo->ucVhtChannelFrequencyS2;

		p2pFuncAcquireCh(prAdapter, ucBssIdx, prChnlReqInfo);
	} while (FALSE);

	if (prP2pMsgChnlReq)
		cnmMemFree(prAdapter, prP2pMsgChnlReq);

	return fgIsTransition;
}				/* p2pDevStateInit_REQING_CHANNEL */

void
p2pDevStateAbort_REQING_CHANNEL(IN struct ADAPTER *prAdapter,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		IN enum ENUM_P2P_DEV_STATE eNextState)
{
	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prChnlReqInfo != NULL)
			&& (eNextState < P2P_DEV_STATE_NUM));

		switch (eNextState) {
		case P2P_DEV_STATE_IDLE:
			/* Channel abort case. */
			p2pFuncReleaseCh(prAdapter,
				prAdapter->ucP2PDevBssIdx, prChnlReqInfo);
			break;
		case P2P_DEV_STATE_CHNL_ON_HAND:
			/* Channel on hand case. */
			break;
		default:
			/* Un-expected state transition. */
			DBGLOG(P2P, ERROR,
				"Unexpected State Transition(eNextState=%d)\n",
				eNextState);
			ASSERT(FALSE);
			break;
		}
	} while (FALSE);
}				/* p2pDevStateAbort_REQING_CHANNEL */

void
p2pDevStateInit_CHNL_ON_HAND(IN struct ADAPTER *prAdapter,
		 IN struct BSS_INFO *prP2pBssInfo,
		 IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
		 IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo)
{
	do {
		uint32_t u4TimeoutMs = 0;

		ASSERT_BREAK((prAdapter != NULL)
			&& (prP2pDevFsmInfo != NULL)
			&& (prChnlReqInfo != NULL));

		ASSERT(prChnlReqInfo->eChnlReqType == CH_REQ_TYPE_P2P_LISTEN);

		prChnlReqInfo->ucOriChnlNum = prP2pBssInfo->ucPrimaryChannel;
		prChnlReqInfo->eOriBand = prP2pBssInfo->eBand;
		prChnlReqInfo->eOriChnlSco = prP2pBssInfo->eBssSCO;

		prP2pBssInfo->ucPrimaryChannel = prChnlReqInfo->ucReqChnlNum;
		prP2pBssInfo->eBand = prChnlReqInfo->eBand;
		prP2pBssInfo->eBssSCO = prChnlReqInfo->eChnlSco;

		if (prAdapter->prP2pInfo->ucExtendChanFlag)
			u4TimeoutMs = P2P_DEV_EXTEND_CHAN_TIME;
		else
			u4TimeoutMs = prChnlReqInfo->u4MaxInterval;

		log_dbg(P2P, INFO,
			"Start channel on hand timer, Cookie: 0x%llx, Interval: %d\n",
			prChnlReqInfo->u8Cookie, u4TimeoutMs);

		cnmTimerStartTimer(prAdapter,
			&(prP2pDevFsmInfo->rP2pFsmTimeoutTimer),
			u4TimeoutMs);

		/* Do NOT report channel ready event again for extension case */
		if (!prAdapter->prP2pInfo->ucExtendChanFlag) {
			kalP2PIndicateChannelReady(prAdapter->prGlueInfo,
					prChnlReqInfo->u8Cookie,
					prChnlReqInfo->ucReqChnlNum,
					prChnlReqInfo->eBand,
					prChnlReqInfo->eChnlSco,
					prChnlReqInfo->u4MaxInterval);
			if (prP2pDevFsmInfo->rQueuedActionFrame.u2Length > 0) {
				kalP2pIndicateQueuedMgmtFrame(
					prAdapter->prGlueInfo,
					&prP2pDevFsmInfo->rQueuedActionFrame);
				p2pFunCleanQueuedMgmtFrame(prAdapter,
					&prP2pDevFsmInfo->rQueuedActionFrame);
			}
		}
	} while (FALSE);
}				/* p2pDevStateInit_CHNL_ON_HAND */

void
p2pDevStateAbort_CHNL_ON_HAND(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pBssInfo,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		IN enum ENUM_P2P_DEV_STATE eNextState)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) || (prChnlReqInfo != NULL));

		cnmTimerStopTimer(prAdapter,
			&(prP2pDevFsmInfo->rP2pFsmTimeoutTimer));

		prP2pBssInfo->ucPrimaryChannel = prChnlReqInfo->ucOriChnlNum;
		prP2pBssInfo->eBand = prChnlReqInfo->eOriBand;
		prP2pBssInfo->eBssSCO = prChnlReqInfo->eOriChnlSco;

		if (eNextState != P2P_DEV_STATE_CHNL_ON_HAND) {
			kalP2PIndicateChannelExpired(prAdapter->prGlueInfo,
					     prChnlReqInfo->u8Cookie,
					     prChnlReqInfo->ucReqChnlNum,
					     prChnlReqInfo->eBand,
					     prChnlReqInfo->eChnlSco);

			p2pFuncReleaseCh(prAdapter,
				prP2pDevFsmInfo->ucBssIndex, prChnlReqInfo);
			p2pFunCleanQueuedMgmtFrame(prAdapter,
					&prP2pDevFsmInfo->rQueuedActionFrame);
		}
	} while (FALSE);
}				/* p2pDevStateAbort_CHNL_ON_HAND */

void p2pDevStateInit_SCAN(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex,
		IN struct P2P_SCAN_REQ_INFO *prScanReqInfo)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prScanReqInfo != NULL));

		prScanReqInfo->fgIsScanRequest = TRUE;

		p2pFuncRequestScan(prAdapter, ucBssIndex, prScanReqInfo);
	} while (FALSE);
}				/* p2pDevStateInit_CHNL_ON_HAND */

void p2pDevStateAbort_SCAN(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo)
{
	struct P2P_SCAN_REQ_INFO *prScanInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prP2pDevFsmInfo != NULL));

		prScanInfo = &(prP2pDevFsmInfo->rScanReqInfo);

		p2pFuncCancelScan(prAdapter,
			prP2pDevFsmInfo->ucBssIndex,
			prScanInfo);

		kalP2PIndicateScanDone(prAdapter->prGlueInfo,
			0xFF,
			prScanInfo->fgIsAbort);
	} while (FALSE);
}				/* p2pDevStateAbort_CHNL_ON_HAND */

u_int8_t
p2pDevStateInit_OFF_CHNL_TX(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
		IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		IN struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo,
		OUT enum ENUM_P2P_DEV_STATE *peNextState)
{
	struct P2P_OFF_CHNL_TX_REQ_INFO *prP2pOffChnlTxPkt =
		(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	u_int8_t fgIsTransition = FALSE;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pMgmtTxInfo != NULL)
			     && (peNextState != NULL));

		if (!LINK_IS_EMPTY(&(prP2pMgmtTxInfo->rP2pTxReqLink))) {
			prP2pOffChnlTxPkt =
			    LINK_PEEK_HEAD(&(prP2pMgmtTxInfo->rP2pTxReqLink),
				struct P2P_OFF_CHNL_TX_REQ_INFO, rLinkEntry);

			if (prP2pOffChnlTxPkt == NULL) {
				DBGLOG(P2P, ERROR,
					"Fetal Error, Link not empty but get NULL pointer.\n");
				ASSERT(FALSE);
				break;
			}

			if (prChnlReqInfo->ucReqChnlNum
				!=
				prP2pOffChnlTxPkt->rChannelInfo.ucChannelNum) {

				prChnlReqInfo->ucReqChnlNum =
					prP2pOffChnlTxPkt->rChannelInfo
					.ucChannelNum;
				prChnlReqInfo->eChnlSco =
					prP2pOffChnlTxPkt->eChnlExt;
				prChnlReqInfo->eBand =
					prP2pOffChnlTxPkt->rChannelInfo.eBand;
				prChnlReqInfo->u8Cookie = 0;
				prChnlReqInfo->eChannelWidth = CW_20_40MHZ;
				prChnlReqInfo->ucCenterFreqS1 = 0;
				prChnlReqInfo->ucCenterFreqS2 = 0;

				ASSERT(prChnlReqInfo->eChnlReqType
					== CH_REQ_TYPE_OFFCHNL_TX);

				p2pFuncAcquireCh(prAdapter,
					prP2pDevFsmInfo->ucBssIndex,
					prChnlReqInfo);
			} else {
				LINK_REMOVE_HEAD(
					&(prP2pMgmtTxInfo->rP2pTxReqLink),
					prP2pOffChnlTxPkt,
					struct P2P_OFF_CHNL_TX_REQ_INFO *);

				p2pFuncTxMgmtFrame(prAdapter,
					prP2pDevFsmInfo->ucBssIndex,
					prP2pOffChnlTxPkt->prMgmtTxMsdu,
					prP2pOffChnlTxPkt->fgNoneCckRate);

				prP2pMgmtTxInfo->prMgmtTxMsdu =
					prP2pOffChnlTxPkt->prMgmtTxMsdu;
				prP2pMgmtTxInfo->fgIsWaitRsp =
					prP2pOffChnlTxPkt->fgIsWaitRsp;
			}
		} else {
			/* Link is empty, return back to IDLE. */
			*peNextState = P2P_DEV_STATE_IDLE;
			fgIsTransition = TRUE;
		}
	} while (FALSE);

	return fgIsTransition;
}				/* p2pDevSateInit_OFF_CHNL_TX */

void
p2pDevStateAbort_OFF_CHNL_TX(IN struct ADAPTER *prAdapter,
		 IN struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo,
		 IN struct P2P_CHNL_REQ_INFO *prChnlReqInfo,
		 IN enum ENUM_P2P_DEV_STATE eNextState)
{
	struct P2P_OFF_CHNL_TX_REQ_INFO *prP2pOffChnlTxPkt =
		(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;

	if (eNextState != P2P_DEV_STATE_OFF_CHNL_TX) {
		while (!LINK_IS_EMPTY(&(prP2pMgmtTxInfo->rP2pTxReqLink))) {
			LINK_REMOVE_HEAD(&(prP2pMgmtTxInfo->rP2pTxReqLink),
					 prP2pOffChnlTxPkt,
					 struct P2P_OFF_CHNL_TX_REQ_INFO *);

			if (prP2pOffChnlTxPkt)
				kalP2PIndicateMgmtTxStatus(
					prAdapter->prGlueInfo,
					prP2pOffChnlTxPkt->prMgmtTxMsdu,
					FALSE);
			else
				DBGLOG(P2P, INFO,
					"No packet for indicating Tx status!\n");

			p2pFuncReleaseCh(prAdapter,
				prAdapter->ucP2PDevBssIdx,
				prChnlReqInfo);
		}
	}
}				/* p2pDevSateAbort_OFF_CHNL_TX */
