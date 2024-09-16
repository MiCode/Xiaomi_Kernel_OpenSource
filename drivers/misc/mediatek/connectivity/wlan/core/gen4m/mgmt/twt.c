/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
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
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
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
/*! \file   "twt.c"
*   \brief  Functions for processing TWT related elements and frames.
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

static void twtFillTWTElement(
	struct _IE_TWT_T *prTWTBuf,
	uint8_t ucTWTFlowId,
	struct _TWT_PARAMS_T *prTWTParams);

static void twtParseTWTElement(
	struct _IE_TWT_T *prTWTIE,
	struct _TWT_PARAMS_T *prTWTParams);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief Send TWT Setup frame (S1G action frame)
*
* \param[in] prAdapter ADAPTER structure
*            prStaRec station record structure
*
* \return none
*/
/*----------------------------------------------------------------------------*/
uint32_t twtSendSetupFrame(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	struct _TWT_PARAMS_T *prTWTParams,
	PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct _ACTION_TWT_SETUP_FRAME *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	struct _IE_TWT_T *prTWTBuf;

	ASSERT(prAdapter);
	ASSERT(prStaRec);
	ASSERT(prTWTParams);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD
		+ sizeof(struct _ACTION_TWT_SETUP_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)
			cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo) {
		DBGLOG(TWT_REQUESTER, WARN,
			"No MSDU_INFO_T for sending TWT Setup Frame.\n");
		return WLAN_STATUS_RESOURCES;
	}

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_S1G_ACTION;
	prTxFrame->ucAction = ACTION_S1G_TWT_SETUP;

	prTWTBuf = &(prTxFrame->rTWT);
	twtFillTWTElement(prTWTBuf, ucTWTFlowId, prTWTParams);

	/* Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
			prMsduInfo,
			prBssInfo->ucBssIndex,
			prStaRec->ucIndex,
			WLAN_MAC_MGMT_HEADER_LEN,
			sizeof(struct _ACTION_TWT_SETUP_FRAME),
			pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* Enqueue the frame to send this action frame */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}

uint32_t twtSendTeardownFrame(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct _ACTION_TWT_TEARDOWN_FRAME *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
		sizeof(struct _ACTION_TWT_TEARDOWN_FRAME);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *) cnmMgtPktAlloc(
		prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo) {
		DBGLOG(TWT_REQUESTER, WARN,
			"No MSDU_INFO_T for sending TWT Teardown Frame.\n");
		return WLAN_STATUS_RESOURCES;
	}

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_S1G_ACTION;
	prTxFrame->ucAction = ACTION_S1G_TWT_TEARDOWN;
	prTxFrame->ucTWTFlow = ucTWTFlowId;

	/* Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
			prMsduInfo,
			prBssInfo->ucBssIndex,
			prStaRec->ucIndex,
			WLAN_MAC_MGMT_HEADER_LEN,
			sizeof(struct _ACTION_TWT_TEARDOWN_FRAME),
			pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* Enqueue the frame to send this action frame */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}

uint32_t twtSendInfoFrame(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo,
	PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct _ACTION_TWT_INFO_FRAME *prTxFrame;
	uint32_t u4Pos =
		OFFSET_OF(struct _ACTION_TWT_INFO_FRAME, aucNextTWT[0]);
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
		sizeof(struct _ACTION_TWT_INFO_FRAME) +
		twtGetNextTWTByteCnt(prNextTWTInfo->ucNextTWTSize);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *) cnmMgtPktAlloc(
		prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo) {
		DBGLOG(TWT_REQUESTER, WARN,
			"No MSDU_INFO_T for sending TWT Info Frame.\n");
		return WLAN_STATUS_RESOURCES;
	}

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	/* Compose the frame body's frame */
	prTxFrame->ucCategory = CATEGORY_S1G_ACTION;
	prTxFrame->ucAction = ACTION_S1G_TWT_INFORMATION;
	prTxFrame->ucNextTWTCtrl = (ucTWTFlowId & TWT_INFO_FLOW_ID) |
		((prNextTWTInfo->ucNextTWTSize & TWT_INFO_NEXT_TWT_SIZE) <<
		TWT_INFO_NEXT_TWT_SIZE_OFFSET);

	switch (prNextTWTInfo->ucNextTWTSize) {
	case NEXT_TWT_SUBFIELD_64_BITS:
	{
		uint64_t *pu8NextTWT =
			(uint64_t *)(((uint8_t *)prTxFrame) + u4Pos);
		*pu8NextTWT = CPU_TO_LE64(prNextTWTInfo->u8NextTWT);
		break;
	}

	case NEXT_TWT_SUBFIELD_32_BITS:
	{
		uint32_t *pu4NextTWT =
			(uint32_t *)(((uint8_t *)prTxFrame) + u4Pos);
		*pu4NextTWT = CPU_TO_LE32(
			(uint32_t)(prNextTWTInfo->u8NextTWT & 0xFFFFFFFF));
		break;
	}

	case NEXT_TWT_SUBFIELD_48_BITS:
	{
		uint8_t *pucMem = ((uint8_t *)prTxFrame) + u4Pos;
		/* little endian placement */
		*pucMem = prNextTWTInfo->u8NextTWT & 0xFF;
		*(pucMem + 1) = (prNextTWTInfo->u8NextTWT >> 8) & 0xFF;
		*(pucMem + 2) = (prNextTWTInfo->u8NextTWT >> 16) & 0xFF;
		*(pucMem + 3) = (prNextTWTInfo->u8NextTWT >> 24) & 0xFF;
		*(pucMem + 4) = (prNextTWTInfo->u8NextTWT >> 32) & 0xFF;
		*(pucMem + 5) = (prNextTWTInfo->u8NextTWT >> 40) & 0xFF;
		break;
	}

	default:
		break;
	}

	/* Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
			prMsduInfo,
			prBssInfo->ucBssIndex,
			prStaRec->ucIndex,
			WLAN_MAC_MGMT_HEADER_LEN,
			(sizeof(struct _ACTION_TWT_INFO_FRAME) +
				prNextTWTInfo->ucNextTWTSize),
			pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* Enqueue the frame to send this action frame */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}

static void twtFillTWTElement(
	struct _IE_TWT_T *prTWTBuf,
	uint8_t ucTWTFlowId,
	struct _TWT_PARAMS_T *prTWTParams)
{
	ASSERT(prTWTBuf);
	ASSERT(prTWTParams);

	/* Add TWT element */
	prTWTBuf->ucId = ELEM_ID_TWT;
	prTWTBuf->ucLength = sizeof(struct _IE_TWT_T) - ELEM_HDR_LEN;

	/* Request Type */
	prTWTBuf->u2ReqType |= SET_TWT_RT_REQUEST(prTWTParams->fgReq) |
		SET_TWT_RT_SETUP_CMD(prTWTParams->ucSetupCmd) |
		SET_TWT_RT_TRIGGER(prTWTParams->fgTrigger) |
		TWT_REQ_TYPE_IMPLICIT_LAST_BCAST_PARAM |
		SET_TWT_RT_FLOW_TYPE(prTWTParams->fgUnannounced) |
		SET_TWT_RT_FLOW_ID(ucTWTFlowId) |
		SET_TWT_RT_WAKE_INTVAL_EXP(prTWTParams->ucWakeIntvalExponent) |
		SET_TWT_RT_PROTECTION(prTWTParams->fgProtect);

	prTWTBuf->u8TWT = CPU_TO_LE64(prTWTParams->u8TWT);
	prTWTBuf->ucMinWakeDur = prTWTParams->ucMinWakeDur;
	prTWTBuf->u2WakeIntvalMantiss =
		CPU_TO_LE16(prTWTParams->u2WakeIntvalMantiss);
}

static void twtParseTWTElement(
	struct _IE_TWT_T *prTWTIE,
	struct _TWT_PARAMS_T *prTWTParams)
{
	uint16_t u2ReqType;

	u2ReqType = LE16_TO_CPU(prTWTIE->u2ReqType);

	prTWTParams->fgReq = GET_TWT_RT_REQUEST(u2ReqType);
	prTWTParams->ucSetupCmd = GET_TWT_RT_SETUP_CMD(u2ReqType);
	prTWTParams->fgTrigger = GET_TWT_RT_TRIGGER(u2ReqType);
	prTWTParams->fgUnannounced = GET_TWT_RT_FLOW_TYPE(u2ReqType);

	prTWTParams->ucWakeIntvalExponent =
		GET_TWT_RT_WAKE_INTVAL_EXP(u2ReqType);

	prTWTParams->fgProtect = GET_TWT_RT_PROTECTION(u2ReqType);
	prTWTParams->u8TWT = LE64_TO_CPU(prTWTIE->u8TWT);
	prTWTParams->ucMinWakeDur = prTWTIE->ucMinWakeDur;

	prTWTParams->u2WakeIntvalMantiss =
		LE16_TO_CPU(prTWTIE->u2WakeIntvalMantiss);
}

uint8_t twtGetTxSetupFlowId(
	struct MSDU_INFO *prMsduInfo)
{
	uint8_t ucFlowId;
	struct _ACTION_TWT_SETUP_FRAME *prTxFrame;

	ASSERT(prMsduInfo);

	prTxFrame = (struct _ACTION_TWT_SETUP_FRAME *)(prMsduInfo->prPacket);
	ucFlowId = GET_TWT_RT_FLOW_ID(prTxFrame->rTWT.u2ReqType);

	return ucFlowId;
}

uint8_t twtGetTxTeardownFlowId(
	struct MSDU_INFO *prMsduInfo)
{
	uint8_t ucFlowId;
	struct _ACTION_TWT_TEARDOWN_FRAME *prTxFrame;

	ASSERT(prMsduInfo);

	prTxFrame = (struct _ACTION_TWT_TEARDOWN_FRAME *)(prMsduInfo->prPacket);
	ucFlowId = (prTxFrame->ucTWTFlow & TWT_TEARDOWN_FLOW_ID);

	return ucFlowId;
}

uint8_t twtGetTxInfoFlowId(
	struct MSDU_INFO *prMsduInfo)
{
	uint8_t ucFlowId;
	struct _ACTION_TWT_INFO_FRAME *prTxFrame;

	ASSERT(prMsduInfo);

	prTxFrame = (struct _ACTION_TWT_INFO_FRAME *)(prMsduInfo->prPacket);
	ucFlowId = GET_TWT_INFO_FLOW_ID(prTxFrame->ucNextTWTCtrl);

	return ucFlowId;
}

uint8_t twtGetRxSetupFlowId(
	struct _IE_TWT_T *prTWTIE)
{
	uint16_t u2ReqType;

	ASSERT(prTWTIE);

	u2ReqType = LE16_TO_CPU(prTWTIE->u2ReqType);

	return GET_TWT_RT_FLOW_ID(u2ReqType);
}

void twtProcessS1GAction(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	struct WLAN_ACTION_FRAME *prRxFrame;
	struct _ACTION_TWT_SETUP_FRAME *prRxSetupFrame = NULL;
	struct _ACTION_TWT_TEARDOWN_FRAME *prRxTeardownFrame = NULL;
	struct _ACTION_TWT_INFO_FRAME *prRxInfoFrame = NULL;
	struct STA_RECORD *prStaRec;
	struct RX_DESC_OPS_T *prRxDescOps;

	uint8_t ucTWTFlowId;
	uint32_t u4Offset;
	uint16_t u2ReqType;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxDescOps = prAdapter->chip_info->prRxDescOps;
	ASSERT(prRxDescOps->nic_rxd_get_rx_byte_count);
	ASSERT(prRxDescOps->nic_rxd_get_pkt_type);
	ASSERT(prRxDescOps->nic_rxd_get_wlan_idx);

	prRxFrame = (struct WLAN_ACTION_FRAME *) prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec) {
		DBGLOG(TWT_REQUESTER, WARN,
		"Received an S1G Action: wlanIdx[%d] w/o corresponding staRec\n"
		, prRxDescOps->nic_rxd_get_wlan_idx(prSwRfb->prRxStatus));
		return;
	}

	switch (prRxFrame->ucAction) {
	case ACTION_S1G_TWT_SETUP:
		prRxSetupFrame =
			(struct _ACTION_TWT_SETUP_FRAME *) prSwRfb->pvHeader;
		if (prStaRec->ucStaState != STA_STATE_3 ||
			prSwRfb->u2PacketLen <
				sizeof(struct _ACTION_TWT_SETUP_FRAME)) {
			DBGLOG(TWT_REQUESTER, WARN,
				"Received improper TWT Setup frame\n");
			return;
		}

		/* Parse TWT element */
		ucTWTFlowId = twtGetRxSetupFlowId(&(prRxSetupFrame->rTWT));
		twtParseTWTElement(&(prRxSetupFrame->rTWT),
			&(prStaRec->arTWTFlow[ucTWTFlowId].rTWTPeerParams));

		/* Notify TWT Requester FSM upon reception of a TWT response */
		u2ReqType = prRxSetupFrame->rTWT.u2ReqType;
		if (!(u2ReqType & TWT_REQ_TYPE_TWT_REQUEST)) {
			twtReqFsmRunEventRxSetup(prAdapter,
				prSwRfb, prStaRec, ucTWTFlowId);
		}

		break;

	case ACTION_S1G_TWT_TEARDOWN:
		prRxTeardownFrame = (struct _ACTION_TWT_TEARDOWN_FRAME *)
			prSwRfb->pvHeader;
		if (prStaRec->ucStaState != STA_STATE_3 ||
			prSwRfb->u2PacketLen <
				sizeof(struct _ACTION_TWT_TEARDOWN_FRAME)) {
			DBGLOG(TWT_REQUESTER, WARN,
				"Received improper TWT Teardown frame\n");
			return;
		}

		ucTWTFlowId = prRxTeardownFrame->ucTWTFlow;

		/* Notify TWT Requester FSM */
		twtReqFsmRunEventRxTeardown(
			prAdapter, prSwRfb, prStaRec, ucTWTFlowId);

		break;

	case ACTION_S1G_TWT_INFORMATION:
	{
		uint8_t ucNextTWTSize = 0;
		uint8_t *pucMem;
		struct _NEXT_TWT_INFO_T rNextTWTInfo;

		prRxInfoFrame = (struct _ACTION_TWT_INFO_FRAME *)
			prSwRfb->pvHeader;
		if (prStaRec->ucStaState != STA_STATE_3 ||
			prSwRfb->u2PacketLen <
				sizeof(struct _ACTION_TWT_INFO_FRAME)) {
			DBGLOG(TWT_REQUESTER, WARN,
				"Received improper TWT Info frame\n");
			return;
		}

		ucTWTFlowId = GET_TWT_INFO_FLOW_ID(
			prRxInfoFrame->ucNextTWTCtrl);
		ucNextTWTSize = GET_TWT_INFO_NEXT_TWT_SIZE(
			prRxInfoFrame->ucNextTWTCtrl);

		u4Offset = OFFSET_OF(struct _ACTION_TWT_INFO_FRAME,
			aucNextTWT[0]);
		pucMem = ((uint8_t *)prRxInfoFrame) + u4Offset;

		if (ucNextTWTSize == NEXT_TWT_SUBFIELD_64_BITS &&
			prSwRfb->u2PacketLen >=
			(sizeof(struct _ACTION_TWT_INFO_FRAME) + 8)) {
			rNextTWTInfo.u8NextTWT =
				LE64_TO_CPU(*((uint64_t *)pucMem));
		} else if (ucNextTWTSize == NEXT_TWT_SUBFIELD_32_BITS &&
			prSwRfb->u2PacketLen >=
			(sizeof(struct _ACTION_TWT_INFO_FRAME) + 4)) {
			rNextTWTInfo.u8NextTWT =
				LE32_TO_CPU(*((uint32_t *)pucMem));
		} else if (ucNextTWTSize == NEXT_TWT_SUBFIELD_48_BITS &&
			prSwRfb->u2PacketLen >=
			(sizeof(struct _ACTION_TWT_INFO_FRAME) + 6)) {
			rNextTWTInfo.u8NextTWT =
				GET_48_BITS_NEXT_TWT_FROM_PKT(pucMem);
		} else {
			DBGLOG(TWT_REQUESTER, WARN,
				"TWT Info frame with imcorrect size\n");
			return;
		}

		rNextTWTInfo.ucNextTWTSize = ucNextTWTSize;

		/* Notify TWT Requester FSM */
		twtReqFsmRunEventRxInfoFrm(
			prAdapter, prSwRfb, prStaRec, ucTWTFlowId,
			&rNextTWTInfo);

		break;
	}
	default:
		break;
	}
}
