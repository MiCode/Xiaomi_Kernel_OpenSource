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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/auth.c#1
 */

/*! \file   "auth.c"
 *    \brief  This file includes the authentication-related functions.
 *
 *   This file includes the authentication-related functions.
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
struct APPEND_VAR_IE_ENTRY txAuthIETable[] = {
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_CHALLENGE_TEXT), NULL,
	 authAddIEChallengeText},
	{0, authCalculateRSNIELen, authAddRSNIE}, /* Element ID: 48 */
	{(ELEM_HDR_LEN + 1), NULL, authAddMDIE}, /* Element ID: 54 */
	{0, rsnCalculateFTIELen, rsnGenerateFTIE}, /* Element ID: 55 */
};

struct HANDLE_IE_ENTRY rxAuthIETable[] = {
	{ELEM_ID_CHALLENGE_TEXT, authHandleIEChallengeText}
};

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

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the Authentication frame header and
 *        fixed fields.
 *
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucPeerMACAddress      Given Peer MAC Address.
 * @param[in] aucMACAddress          Given Our MAC Address.
 * @param[in] u2AuthAlgNum           Authentication Algorithm Number
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 * @param[in] u2StatusCode           Status Code
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
authComposeAuthFrameHeaderAndFF(IN struct ADAPTER *prAdapter,
				IN struct STA_RECORD *prStaRec,
				IN uint8_t *pucBuffer,
				IN uint8_t aucPeerMACAddress[],
				IN uint8_t aucMACAddress[],
				IN uint16_t u2AuthAlgNum,
				IN uint16_t u2TransactionSeqNum,
				IN uint16_t u2StatusCode)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2FrameCtrl;
#if CFG_SUPPORT_CFG80211_AUTH
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
#endif

	ASSERT(pucBuffer);
	ASSERT(aucPeerMACAddress);
	ASSERT(aucMACAddress);

	prAuthFrame = (struct WLAN_AUTH_FRAME *)pucBuffer;

	/* 4 <1> Compose the frame header of the Authentication frame. */
	/* Fill the Frame Control field. */
	u2FrameCtrl = MAC_FRAME_AUTH;

	/* If this frame is the third frame in the shared key authentication
	 * sequence, it shall be encrypted.
	 */
	if ((u2AuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)
	    && (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3))
		u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		/* HW will also detect this bit for applying encryption */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2FrameCtrl, u2FrameCtrl); */
	prAuthFrame->u2FrameCtrl = u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prAuthFrame->aucDestAddr, aucPeerMACAddress);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prAuthFrame->aucSrcAddr, aucMACAddress);

	if (prStaRec != NULL && IS_AP_STA(prStaRec)) {
		/* Fill the BSSID field with Target BSSID. */
		COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucPeerMACAddress);

	} else if (prStaRec != NULL && IS_CLIENT_STA(prStaRec)) {
		/* Fill the BSSID field with Current BSSID. */
		COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucMACAddress);
	} else {
		COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucMACAddress);
		DBGLOG(SAA, INFO,
			"Error status code flow!\n");
	}

	/* Clear the SEQ/FRAG_NO field. */
	prAuthFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's fixed field part of
	 *       the Authentication frame.
	 */
	/* Fill the Authentication Algorithm Number field. */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2AuthAlgNum, u2AuthAlgNum); */
	prAuthFrame->u2AuthAlgNum = u2AuthAlgNum;
	/* NOTE(Kevin): Optimized for ARM */
#if CFG_SUPPORT_CFG80211_AUTH
	if ((prConnSettings->ucAuthDataLen != 0) &&
		!IS_STA_IN_P2P(prStaRec)) {
		kalMemCopy(prAuthFrame->aucAuthData,
			prConnSettings->aucAuthData,
			prConnSettings->ucAuthDataLen);
	} else {
		/* Fill the Authentication Transaction Sequence Number field. */
		/* NOTE(Kevin): Optimized for ARM */
		prAuthFrame->aucAuthData[0] = (uint8_t)
						(u2TransactionSeqNum & 0xff);
		prAuthFrame->aucAuthData[1] = (uint8_t)
					((u2TransactionSeqNum >> 8) & 0xff);
		/* Fill the Status Code field. */
		/* NOTE(Kevin): Optimized for ARM */
		prAuthFrame->aucAuthData[2] = (uint8_t)(u2StatusCode & 0xff);
		prAuthFrame->aucAuthData[3] = (uint8_t)
						((u2StatusCode >> 8) & 0xff);
	}
	DBGLOG(SAA, INFO, "Compose auth with TransSN = %d,Status = %d\n",
		prAuthFrame->aucAuthData[0], prAuthFrame->aucAuthData[2]);
#else
	/* Fill the Authentication Transaction Sequence Number field. */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	u2TransactionSeqNum);
	 */
	prAuthFrame->u2AuthTransSeqNo = u2TransactionSeqNum;
	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the Status Code field. */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2StatusCode, u2StatusCode); */
	prAuthFrame->u2StatusCode = u2StatusCode;
	/* NOTE(Kevin): Optimized for ARM */
#endif
}				/* end of authComposeAuthFrameHeaderAndFF() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will append Challenge Text IE to the Authentication
 *        frame
 *
 * @param[in] prMsduInfo     Pointer to the composed MSDU_INFO_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void authAddIEChallengeText(IN struct ADAPTER *prAdapter,
			    IN OUT struct MSDU_INFO *prMsduInfo)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TransactionSeqNum;

	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	ASSERT(prStaRec);

	/* For Management, frame header and payload are in a continuous
	 * buffer
	 */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prMsduInfo->prPacket;
#if CFG_SUPPORT_CFG80211_AUTH
	WLAN_GET_FIELD_16(&prAuthFrame->aucAuthData[0], &u2TransactionSeqNum)
#else
	WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2TransactionSeqNum)
#endif
	    /* Only consider SEQ_3 for Challenge Text */
	if ((u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3) &&
		(prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)
		&& (prStaRec->prChallengeText != NULL)) {

		COPY_IE(((unsigned long)(prMsduInfo->prPacket) +
			 prMsduInfo->u2FrameLength),
			(prStaRec->prChallengeText));

		prMsduInfo->u2FrameLength += IE_SIZE(prStaRec->prChallengeText);
	}

	return;

}				/* end of authAddIEChallengeText() */

#if !CFG_SUPPORT_AAA
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the Authenticiation frame
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 */
/*----------------------------------------------------------------------------*/
uint32_t authSendAuthFrame(IN struct ADAPTER *prAdapter,
			   IN struct STA_RECORD *prStaRec,
			   IN uint16_t u2TransactionSeqNum)
{
	struct MSDU_INFO *prMsduInfo;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	uint16_t u2EstimatedExtraIELen;
	uint16_t u2PayloadLen;
	uint32_t i;

	DBGLOG(SAA, LOUD, "Send Auth Frame\n");

	ASSERT(prStaRec);

	/* 4 <1> Allocate a PKT_INFO_T for Authentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields */
	u2EstimatedFrameLen = (MAC_TX_RESERVED_FIELD +
			       WLAN_MAC_MGMT_HEADER_LEN +
			       AUTH_ALGORITHM_NUM_FIELD_LEN +
			       AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
			       STATUS_CODE_FIELD_LEN);

	/* + Extra IE Length */
	u2EstimatedExtraIELen = 0;

	for (i = 0;
	     i < sizeof(txAuthIETable) / sizeof(struct APPEND_VAR_IE_ENTRY);
	     i++) {
		if (txAssocRespIETable[i].u2EstimatedFixedIELen != 0)
			u2EstimatedExtraIELen +=
				txAssocRespIETable[i].u2EstimatedFixedIELen;
		else if (txAssocRespIETable[i].pfnCalculateVariableIELen !=
			 NULL)
			u2EstimatedExtraIELen +=
				(uint16_t)txAssocRespIETable[i]
					.pfnCalculateVariableIELen(
						prAdapter, prStaRec->ucBssIndex,
						prStaRec);
	}

	u2EstimatedFrameLen += u2EstimatedExtraIELen;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN, "No PKT_INFO_T for sending Auth Frame.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose Authentication Request frame header and fixed fields
	 * in MSDU_INfO_T.
	 */
	ASSERT(prStaRec->ucBssIndex <= prAdapter->ucHwBssIdNum);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex)

	    /* Compose Header and some Fixed Fields */
	    authComposeAuthFrameHeaderAndFF(prAdapter, prStaRec, (uint8_t *)
					    ((uint32_t) (prMsduInfo->prPacket) +
					     MAC_TX_RESERVED_FIELD),
					    prStaRec->aucMacAddr,
					    prBssInfo->aucOwnMacAddr,
					    prStaRec->ucAuthAlgNum,
					    u2TransactionSeqNum,
					    STATUS_CODE_RESERVED);

	u2PayloadLen =
	    (AUTH_ALGORITHM_NUM_FIELD_LEN +
	     AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen,
		     saaFsmRunEventTxDone, MSDU_RATE_MODE_AUTO);

	/* 4 <4> Compose IEs in MSDU_INFO_T */
	for (i = 0; i < sizeof(txAuthIETable) / sizeof(struct APPEND_IE_ENTRY);
	     i++) {
		if (txAuthIETable[i].pfnAppendIE)
			txAuthIETable[i].pfnAppendIE(prAdapter, prMsduInfo);

	}

	/* TODO(Kevin):
	 * Also release the unused tail room of the composed MMPDU
	 */

	nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	/* 4 <6> Inform TXM  to send this Authentication frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of authSendAuthFrame() */

#else

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the Authenticiation frame
 *
 * @param[in] prStaRec               Pointer to the STA_RECORD_T
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 */
/*----------------------------------------------------------------------------*/
uint32_t
authSendAuthFrame(IN struct ADAPTER *prAdapter,
		  IN struct STA_RECORD *prStaRec,
		  IN uint8_t ucBssIndex,
		  IN struct SW_RFB *prFalseAuthSwRfb,
		  IN uint16_t u2TransactionSeqNum, IN uint16_t u2StatusCode)
{
	uint8_t *pucReceiveAddr;
	uint8_t *pucTransmitAddr;
	struct MSDU_INFO *prMsduInfo;
	struct BSS_INFO *prBssInfo;
	/*get from input parameter */
	/* ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex = NETWORK_TYPE_AIS_INDEX; */
	PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER) NULL;
	uint16_t u2EstimatedFrameLen;
	uint16_t u2EstimatedExtraIELen;
	uint16_t u2PayloadLen;
	uint16_t ucAuthAlgNum;
	uint32_t i;
#if CFG_SUPPORT_CFG80211_AUTH
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
#endif

	DBGLOG(SAA, LOUD, "Send Auth Frame %d, Status Code = %d\n",
			u2TransactionSeqNum, u2StatusCode);
#if CFG_SUPPORT_CFG80211_AUTH
	if ((prConnSettings->ucAuthDataLen != 0) &&
		!IS_STA_IN_P2P(prStaRec)) {
		DBGLOG(SAA, INFO, "prConnSettings->ucAuthDataLen = %d\n",
			prConnSettings->ucAuthDataLen);
		u2EstimatedFrameLen = (MAC_TX_RESERVED_FIELD +
			WLAN_MAC_MGMT_HEADER_LEN +
			AUTH_ALGORITHM_NUM_FIELD_LEN +
			prConnSettings->ucAuthDataLen);
	} else
		u2EstimatedFrameLen = (MAC_TX_RESERVED_FIELD +
			WLAN_MAC_MGMT_HEADER_LEN +
			AUTH_ALGORITHM_NUM_FIELD_LEN +
			AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
			STATUS_CODE_FIELD_LEN);
#else
	/* 4 <1> Allocate a PKT_INFO_T for Authentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields */
	u2EstimatedFrameLen = (MAC_TX_RESERVED_FIELD +
			       WLAN_MAC_MGMT_HEADER_LEN +
			       AUTH_ALGORITHM_NUM_FIELD_LEN +
			       AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
			       STATUS_CODE_FIELD_LEN);
#endif
	/* + Extra IE Length */
	u2EstimatedExtraIELen = 0;

	for (i = 0;
	     i < sizeof(txAuthIETable) / sizeof(struct APPEND_VAR_IE_ENTRY);
	     i++) {
		if (txAuthIETable[i].u2EstimatedFixedIELen != 0)
			u2EstimatedExtraIELen +=
				txAuthIETable[i].u2EstimatedFixedIELen;
		else
			u2EstimatedExtraIELen +=
				txAuthIETable[i].pfnCalculateVariableIELen(
					prAdapter, ucBssIndex, prStaRec);
	}

	u2EstimatedFrameLen += u2EstimatedExtraIELen;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN, "No PKT_INFO_T for sending Auth Frame.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose Authentication Request frame header and
	 * fixed fields in MSDU_INfO_T.
	 */
	if (prStaRec) {
		ASSERT(prStaRec->ucBssIndex <= prAdapter->ucHwBssIdNum);
		prBssInfo =
		    GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		pucTransmitAddr = prBssInfo->aucOwnMacAddr;

		pucReceiveAddr = prStaRec->aucMacAddr;

		ucAuthAlgNum = prStaRec->ucAuthAlgNum;

#if CFG_SUPPORT_CFG80211_AUTH
		if (IS_AP_STA(prStaRec))	/* STA mode */
			pfTxDoneHandler = saaFsmRunEventTxDone;
		else if (IS_CLIENT_STA(prStaRec))
			pfTxDoneHandler = aaaFsmRunEventTxDone;
		else {
			DBGLOG(SAA, WARN,
			"Can't send auth with unsupport peer's StaType:%d\n",
			prStaRec->eStaType);
			return WLAN_STATUS_FAILURE;
		}
#else
		switch (u2TransactionSeqNum) {
		case AUTH_TRANSACTION_SEQ_1:
		case AUTH_TRANSACTION_SEQ_3:
			pfTxDoneHandler = saaFsmRunEventTxDone;
			break;

		case AUTH_TRANSACTION_SEQ_2:
		case AUTH_TRANSACTION_SEQ_4:
			pfTxDoneHandler = aaaFsmRunEventTxDone;
			break;
		}
#endif
	} else {		/* For Error Status Code */
		struct WLAN_AUTH_FRAME *prFalseAuthFrame;

		ASSERT(prFalseAuthSwRfb);
		prFalseAuthFrame =
		    (struct WLAN_AUTH_FRAME *)prFalseAuthSwRfb->pvHeader;

		ASSERT(u2StatusCode != STATUS_CODE_SUCCESSFUL);

		pucTransmitAddr = prFalseAuthFrame->aucDestAddr;

		pucReceiveAddr = prFalseAuthFrame->aucSrcAddr;

		ucAuthAlgNum = prFalseAuthFrame->u2AuthAlgNum;
#if CFG_SUPPORT_CFG80211_AUTH
		u2TransactionSeqNum = (prFalseAuthFrame->aucAuthData[1] << 8) +
					(prFalseAuthFrame->aucAuthData[0] + 1);
#else
		u2TransactionSeqNum = (prFalseAuthFrame->u2AuthTransSeqNo + 1);
#endif
	}

	/* Compose Header and some Fixed Fields */
	authComposeAuthFrameHeaderAndFF(prAdapter, prStaRec, (uint8_t *)
					((unsigned long)(prMsduInfo->prPacket) +
					 MAC_TX_RESERVED_FIELD), pucReceiveAddr,
					pucTransmitAddr, ucAuthAlgNum,
					u2TransactionSeqNum, u2StatusCode);

	/* fill the length of auth frame body */
#if CFG_SUPPORT_CFG80211_AUTH
	if ((prConnSettings->ucAuthDataLen != 0) &&
		!IS_STA_IN_P2P(prStaRec))
		u2PayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN +
			prConnSettings->ucAuthDataLen);
	else
		u2PayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN +
			AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
			STATUS_CODE_FIELD_LEN);
#else
	u2PayloadLen =
	    (AUTH_ALGORITHM_NUM_FIELD_LEN +
	     AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);
#endif
	/* 4 <3> Update information of MSDU_INFO_T */

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     ucBssIndex,
		     (prStaRec !=
		      NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler,
		     MSDU_RATE_MODE_AUTO);

	if ((ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)
	    && (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3))
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				     TRUE);
	/* 4 <4> Compose IEs in MSDU_INFO_T */
	for (i = 0;
	     i < sizeof(txAuthIETable) / sizeof(struct APPEND_VAR_IE_ENTRY);
	     i++) {
		if (txAuthIETable[i].pfnAppendIE)
			txAuthIETable[i].pfnAppendIE(prAdapter, prMsduInfo);
	}

	/* TODO(Kevin):
	 * Also release the unused tail room of the composed MMPDU
	 */

	nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	/* 4 <6> Inform TXM  to send this Authentication frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(SAA, INFO,
	       "Send Auth Frame, TranSeq: %d, Status: %d, Seq: %d\n",
	       u2TransactionSeqNum, u2StatusCode, prMsduInfo->ucTxSeqNum);

	return WLAN_STATUS_SUCCESS;
}				/* end of authSendAuthFrame() */

#endif /* CFG_SUPPORT_AAA */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will strictly check the TX Authentication frame
 *        for SAA/AAA event handling.
 *
 * @param[in] prMsduInfo             Pointer of MSDU_INFO_T
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 *
 * @retval WLAN_STATUS_FAILURE       This is not the frame we should handle
 *                                   at current state.
 * @retval WLAN_STATUS_SUCCESS       This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t authCheckTxAuthFrame(IN struct ADAPTER *prAdapter,
			      IN struct MSDU_INFO *prMsduInfo,
			      IN uint16_t u2TransactionSeqNum)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TxFrameCtrl;
	uint16_t u2TxAuthAlgNum;
	uint16_t u2TxTransactionSeqNum;

	ASSERT(prMsduInfo);

	prAuthFrame = (struct WLAN_AUTH_FRAME *)(prMsduInfo->prPacket);
	ASSERT(prAuthFrame);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2FrameCtrl, &u2TxFrameCtrl) */
	u2TxFrameCtrl = prAuthFrame->u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */
	u2TxFrameCtrl &= MASK_FRAME_TYPE;
	if (u2TxFrameCtrl != MAC_FRAME_AUTH)
		return WLAN_STATUS_FAILURE;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthAlgNum, &u2TxAuthAlgNum) */
	u2TxAuthAlgNum = prAuthFrame->u2AuthAlgNum;
	/* NOTE(Kevin): Optimized for ARM */
	if (u2TxAuthAlgNum != (uint16_t) (prStaRec->ucAuthAlgNum))
		return WLAN_STATUS_FAILURE;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2TxTransactionSeqNum)
	 */
#if CFG_SUPPORT_CFG80211_AUTH
	u2TxTransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
#else
	u2TxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
#endif
	/* NOTE(Kevin): Optimized for ARM */
	if (u2TxTransactionSeqNum != u2TransactionSeqNum)
		return WLAN_STATUS_FAILURE;

	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckTxAuthFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will check the incoming Auth Frame's Transaction
 *        Sequence Number before delivering it to the corresponding
 *        SAA or AAA Module.
 *
 * @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
 *
 * @retval WLAN_STATUS_SUCCESS   Always not retain authentication frames
 */
/*----------------------------------------------------------------------------*/
uint32_t authCheckRxAuthFrameTransSeq(IN struct ADAPTER *prAdapter,
				      IN struct SW_RFB *prSwRfb)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2RxTransactionSeqNum;
	uint16_t u2MinPayloadLen;
#if CFG_SUPPORT_SAE
#if CFG_IGNORE_INVALID_AUTH_TSN
	struct STA_RECORD *prStaRec;
#endif
	struct BSS_INFO *prBssInfo = NULL;
#endif


	ASSERT(prSwRfb);

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Authentication Frame. */
	u2MinPayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN +
			   AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
			   STATUS_CODE_FIELD_LEN);
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) < u2MinPayloadLen) {
		DBGLOG(SAA, WARN,
		       "Rx Auth payload: len[%u] < min expected len[%u]\n",
		       (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen),
		       u2MinPayloadLen);
		DBGLOG(SAA, WARN, "=== Dump Rx Auth ===\n");
		DBGLOG_MEM8(SAA, WARN, prAuthFrame, prSwRfb->u2PacketLen);
		return WLAN_STATUS_SUCCESS;
	}
	/* 4 <3> Parse the Fixed Fields of Authentication Frame Body. */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2RxTransactionSeqNum);
	 */
#if CFG_SUPPORT_CFG80211_AUTH
	u2RxTransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
#else
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
#endif

	if ((u2RxTransactionSeqNum < 0) || (u2RxTransactionSeqNum > 4)) {
		DBGLOG(SAA, WARN,
			"RX auth with unexpected TransactionSeqNum:%d\n",
			u2RxTransactionSeqNum);
		return WLAN_STATUS_SUCCESS;
	}
#if CFG_SUPPORT_SAE
	if (prAuthFrame->u2AuthAlgNum == AUTH_ALGORITHM_NUM_SAE) {
		if ((u2RxTransactionSeqNum ==
			AUTH_TRANSACTION_SEQ_1) ||
			(u2RxTransactionSeqNum ==
			AUTH_TRANSACTION_SEQ_2)) {
			prStaRec = prSwRfb->prStaRec;
			if (prStaRec)
				prBssInfo =
					GET_BSS_INFO_BY_INDEX(
					prAdapter,
					prStaRec->ucBssIndex);
			else
				prBssInfo =
					p2pFuncBSSIDFindBssInfo(
					prAdapter,
					prAuthFrame->aucBSSID);

			if (prBssInfo == NULL)
				return WLAN_STATUS_SUCCESS;

			if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE)
				saaFsmRunEventRxAuth(prAdapter, prSwRfb);
#if CFG_SUPPORT_AAA
			else if (prBssInfo->eCurrentOPMode ==
				OP_MODE_ACCESS_POINT)
				aaaFsmRunEventRxAuth(prAdapter, prSwRfb);
#endif
			else
				DBGLOG(SAA, WARN,
					"Don't support SAE for non-AIS/P2P network\n");
		} else {
			DBGLOG(SAA, WARN,
				"RX SAE auth with unexpected TransSeqNum:%d\n",
				u2RxTransactionSeqNum);
		}

	} else {
#endif
		switch (u2RxTransactionSeqNum) {
		case AUTH_TRANSACTION_SEQ_2:
		case AUTH_TRANSACTION_SEQ_4:
			saaFsmRunEventRxAuth(prAdapter, prSwRfb);
			break;
		case AUTH_TRANSACTION_SEQ_1:
		case AUTH_TRANSACTION_SEQ_3:
#if CFG_SUPPORT_AAA
			aaaFsmRunEventRxAuth(prAdapter, prSwRfb);
#endif /* CFG_SUPPORT_AAA */
			break;
		default:
			DBGLOG(SAA, WARN,
			       "Strange Authentication Packet: Auth Trans Seq No = %d\n",
			       u2RxTransactionSeqNum);
		}
#if CFG_SUPPORT_SAE
	}
#endif
	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckRxAuthFrameTransSeq() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will validate the incoming Authentication Frame and
 *        take the status code out.
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 * @param[out] pu2StatusCode         Pointer to store the Status Code from
 *                                   Authentication.
 *
 * @retval WLAN_STATUS_FAILURE       This is not the frame we should handle
 *                                   at current state.
 * @retval WLAN_STATUS_SUCCESS       This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t
authCheckRxAuthFrameStatus(IN struct ADAPTER *prAdapter,
			   IN struct SW_RFB *prSwRfb,
			   IN uint16_t u2TransactionSeqNum,
			   OUT uint16_t *pu2StatusCode)
{
	struct STA_RECORD *prStaRec;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2RxAuthAlgNum;
	uint16_t u2RxTransactionSeqNum;
	/* UINT_16 u2RxStatusCode; // NOTE(Kevin): Optimized for ARM */

	ASSERT(prSwRfb);
	ASSERT(pu2StatusCode);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Fixed Fields of Authentication Frame Body. */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthAlgNum, &u2RxAuthAlgNum); */
	u2RxAuthAlgNum = prAuthFrame->u2AuthAlgNum;
	/* NOTE(Kevin): Optimized for ARM */
	if (u2RxAuthAlgNum != (uint16_t) prStaRec->ucAuthAlgNum) {
		DBGLOG(SAA, WARN,
		       "Discard Auth frame with auth type = %d, current = %d\n",
		       u2RxAuthAlgNum, prStaRec->ucAuthAlgNum);
		*pu2StatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;
		return WLAN_STATUS_SUCCESS;
	}
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2RxTransactionSeqNum);
	 */
#if CFG_SUPPORT_CFG80211_AUTH
	u2RxTransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
	/* Still report to upper layer to let it do the error handling */
	if (u2RxTransactionSeqNum < u2TransactionSeqNum) {
		DBGLOG(SAA, WARN,
		"Rx Auth frame with unexpected Transaction Seq No = %d\n",
		u2RxTransactionSeqNum);
		*pu2StatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;
		return WLAN_STATUS_FAILURE;
	}
#else
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
	if (u2RxTransactionSeqNum != u2TransactionSeqNum) {
		DBGLOG(SAA, WARN,
		       "Discard Auth frame with Transaction Seq No = %d\n",
		       u2RxTransactionSeqNum);
		*pu2StatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;
		return WLAN_STATUS_FAILURE;
	}
#endif
	/* 4 <3> Get the Status code */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2StatusCode, &u2RxStatusCode); */
	/* *pu2StatusCode = u2RxStatusCode; */
#if CFG_SUPPORT_CFG80211_AUTH
	*pu2StatusCode = (prAuthFrame->aucAuthData[3] << 8) +
					prAuthFrame->aucAuthData[2];
#else
	*pu2StatusCode = prAuthFrame->u2StatusCode;
	/* NOTE(Kevin): Optimized for ARM */
#endif
	DBGLOG(SAA, INFO,
	"Rx Auth frame with auth type = %d, SN = %d, Status Code = %d\n",
	u2RxAuthAlgNum, u2RxTransactionSeqNum, *pu2StatusCode);

	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckRxAuthFrameStatus() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will handle the Challenge Text IE from
 *        the Authentication frame
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[in] prIEHdr                Pointer to start address of IE
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void authHandleIEChallengeText(struct ADAPTER *prAdapter,
			       struct SW_RFB *prSwRfb, struct IE_HDR *prIEHdr)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TransactionSeqNum;

	ASSERT(prSwRfb);
	ASSERT(prIEHdr);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (!prStaRec)
		return;

	/* For Management, frame header and payload are in
	 * a continuous buffer
	 */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2TransactionSeqNum)
	 */
#if CFG_SUPPORT_CFG80211_AUTH
	u2TransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
#else
	u2TransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
#endif
	/* Only consider SEQ_2 for Challenge Text */
	if ((u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_2) &&
	    (prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)) {

		/* Free previous allocated TCM memory */
		if (prStaRec->prChallengeText) {
			/* ASSERT(0); */
			cnmMemFree(prAdapter, prStaRec->prChallengeText);
			prStaRec->prChallengeText =
			    (struct IE_CHALLENGE_TEXT *)NULL;
		}
		prStaRec->prChallengeText =
		    cnmMemAlloc(prAdapter, RAM_TYPE_MSG, IE_SIZE(prIEHdr));
		if (prStaRec->prChallengeText == NULL)
			return;

		/* Save the Challenge Text from Auth Seq 2 Frame,
		 * before sending Auth Seq 3 Frame
		 */
		COPY_IE(prStaRec->prChallengeText, prIEHdr);
	}

	return;

}				/* end of authAddIEChallengeText() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Authentication
 *        frame.
 *
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 *
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t authProcessRxAuth2_Auth4Frame(IN struct ADAPTER *prAdapter,
				       IN struct SW_RFB *prSwRfb)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint8_t *pucIEsBuffer;
	uint16_t u2IEsLen;
	uint16_t u2Offset;
	uint8_t ucIEID;
	uint32_t i;
	uint16_t u2TransactionSeqNum;

	ASSERT(prSwRfb);

	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

#if CFG_SUPPORT_CFG80211_AUTH
	pucIEsBuffer = (uint8_t *)&prAuthFrame->aucAuthData[0] + 4;
#else
	pucIEsBuffer = &prAuthFrame->aucInfoElem[0];
#endif
	u2IEsLen = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (AUTH_ALGORITHM_NUM_FIELD_LEN +
	     AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	IE_FOR_EACH(pucIEsBuffer, u2IEsLen, u2Offset) {
		ucIEID = IE_ID(pucIEsBuffer);

		for (i = 0;
		     i <
		     (sizeof(rxAuthIETable) / sizeof(struct HANDLE_IE_ENTRY));
		     i++) {
			if ((ucIEID == rxAuthIETable[i].ucElemID)
			    && (rxAuthIETable[i].pfnHandleIE != NULL))
				rxAuthIETable[i].pfnHandleIE(prAdapter,
					prSwRfb,
					(struct IE_HDR *)pucIEsBuffer);
		}
	}
#if CFG_SUPPORT_CFG80211_AUTH
		u2TransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
#else
		u2TransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
		/* NOTE(Kevin): Optimized for ARM */
#endif
	if (prAuthFrame->u2AuthAlgNum ==
	    AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION) {
		if (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_4) {
			/* todo: check MIC, if mic error, return
			 * WLAN_STATUS_FAILURE
			 */
		} else if (u2TransactionSeqNum ==
			   AUTH_TRANSACTION_SEQ_2) {
			prAdapter->prGlueInfo->rFtEventParam.ies =
				&prAuthFrame->aucInfoElem[0];
			prAdapter->prGlueInfo->rFtEventParam.ies_len = u2IEsLen;
		}
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of authProcessRxAuth2_Auth4Frame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the Deauthentication frame
 *
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucPeerMACAddress      Given Peer MAC Address.
 * @param[in] aucMACAddress          Given Our MAC Address.
 * @param[in] u2StatusCode           Status Code
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
authComposeDeauthFrameHeaderAndFF(IN uint8_t *pucBuffer,
				  IN uint8_t aucPeerMACAddress[],
				  IN uint8_t aucMACAddress[],
				  IN uint8_t aucBssid[],
				  IN uint16_t u2ReasonCode)
{
	struct WLAN_DEAUTH_FRAME *prDeauthFrame;
	uint16_t u2FrameCtrl;

	ASSERT(pucBuffer);
	ASSERT(aucPeerMACAddress);
	ASSERT(aucMACAddress);
	ASSERT(aucBssid);

	prDeauthFrame = (struct WLAN_DEAUTH_FRAME *)pucBuffer;

	/* 4 <1> Compose the frame header of the Deauthentication frame. */
	/* Fill the Frame Control field. */
	u2FrameCtrl = MAC_FRAME_DEAUTH;

	/* WLAN_SET_FIELD_16(&prDeauthFrame->u2FrameCtrl, u2FrameCtrl); */
	prDeauthFrame->u2FrameCtrl = u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prDeauthFrame->aucDestAddr, aucPeerMACAddress);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prDeauthFrame->aucSrcAddr, aucMACAddress);

	/* Fill the BSSID field with Target BSSID. */
	COPY_MAC_ADDR(prDeauthFrame->aucBSSID, aucBssid);

	/* Clear the SEQ/FRAG_NO field(HW won't overide the FRAG_NO,
	 * so we need to clear it).
	 */
	prDeauthFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's fixed field part of
	 *       the Authentication frame.
	 */
	/* Fill the Status Code field. */
	/* WLAN_SET_FIELD_16(&prDeauthFrame->u2ReasonCode, u2ReasonCode); */
	prDeauthFrame->u2ReasonCode = u2ReasonCode;
	/* NOTE(Kevin): Optimized for ARM */
}			/* end of authComposeDeauthFrameHeaderAndFF() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the Deauthenticiation frame
 *
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 * @param[in] prClassErrSwRfb    Pointer to the SW_RFB_T which is Class Error.
 * @param[in] u2ReasonCode       A reason code to indicate why to leave BSS.
 * @param[in] pfTxDoneHandler    TX Done call back function
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 * @retval WLAN_STATUS_FAILURE   Didn't send Deauth frame for various reasons.
 */
/*----------------------------------------------------------------------------*/
uint32_t
authSendDeauthFrame(IN struct ADAPTER *prAdapter,
		    IN struct BSS_INFO *prBssInfo,
		    IN struct STA_RECORD *prStaRec,
		    IN struct SW_RFB *prClassErrSwRfb, IN uint16_t u2ReasonCode,
		    IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	uint8_t *pucReceiveAddr;
	uint8_t *pucTransmitAddr;
	uint8_t *pucBssid = NULL;
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2EstimatedFrameLen;

	struct DEAUTH_INFO *prDeauthInfo;
	OS_SYSTIME rCurrentTime;
	int32_t i4NewEntryIndex, i;
	uint8_t ucStaRecIdx = STA_REC_INDEX_NOT_FOUND;
	uint8_t ucBssIndex = prAdapter->ucHwBssIdNum;
	uint8_t aucBMC[] = BC_MAC_ADDR;
#if CFG_SUPPORT_CFG80211_AUTH
	struct WLAN_DEAUTH_FRAME *prDeauthFrame;
#endif

	DBGLOG(RSN, INFO, "authSendDeauthFrame\n");

	/* NOTE(Kevin): The best way to reply the Deauth is according to
	 * the incoming data frame
	 */
	/* 4 <1.1> Find the Receiver Address */
	if (prClassErrSwRfb) {
		u_int8_t fgIsAbleToSendDeauth = FALSE;
		uint16_t u2RxFrameCtrl;
		struct WLAN_MAC_HEADER_A4 *prWlanMacHeader = NULL;

		prWlanMacHeader =
		    (struct WLAN_MAC_HEADER_A4 *)prClassErrSwRfb->pvHeader;

		/* WLAN_GET_FIELD_16(&prWlanMacHeader->u2FrameCtrl,
		 *   &u2RxFrameCtrl);
		 */
		u2RxFrameCtrl = prWlanMacHeader->u2FrameCtrl;
		/* NOTE(Kevin): Optimized for ARM */

		/* TODO(Kevin): Currently we won't send Deauth for IBSS node.
		 * How about DLS ?
		 */
		if ((prWlanMacHeader->u2FrameCtrl & MASK_TO_DS_FROM_DS) == 0)
			return WLAN_STATUS_FAILURE;

		DBGLOG(SAA, INFO,
		       "u2FrameCtrl=0x%x, DestAddr=" MACSTR
		       " srcAddr=" MACSTR " BSSID=" MACSTR
		       ", u2SeqCtrl=0x%x\n",
		       prWlanMacHeader->u2FrameCtrl,
		       MAC2STR(prWlanMacHeader->aucAddr1),
		       MAC2STR(prWlanMacHeader->aucAddr2),
		       MAC2STR(prWlanMacHeader->aucAddr3),
		       prWlanMacHeader->u2SeqCtrl);
		/* Check if corresponding BSS is able to send Deauth */
		for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

			if (IS_NET_ACTIVE(prAdapter, i) &&
			    (EQUAL_MAC_ADDR
			     (prWlanMacHeader->aucAddr1,
			      prBssInfo->aucOwnMacAddr))) {

				fgIsAbleToSendDeauth = TRUE;
				ucBssIndex = (uint8_t) i;
				break;
			}
		}

		if (!fgIsAbleToSendDeauth)
			return WLAN_STATUS_FAILURE;

		pucReceiveAddr = prWlanMacHeader->aucAddr2;
	} else if (prStaRec) {
		prBssInfo =
		    GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
		ucStaRecIdx = prStaRec->ucIndex;
		ucBssIndex = prBssInfo->ucBssIndex;

		pucReceiveAddr = prStaRec->aucMacAddr;
	} else if (prBssInfo) {
		ucBssIndex = prBssInfo->ucBssIndex;
		ucStaRecIdx = STA_REC_INDEX_BMCAST;

		pucReceiveAddr = aucBMC;
	} else {
		DBGLOG(SAA, WARN, "Not to send Deauth, invalid data!\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 4 <1.2> Find Transmitter Address and BSSID. */
	pucTransmitAddr = prBssInfo->aucOwnMacAddr;
	pucBssid = prBssInfo->aucBSSID;

	if (ucStaRecIdx != STA_REC_INDEX_BMCAST) {
		/* 4 <2> Check if already send a Deauth frame in
		 * MIN_DEAUTH_INTERVAL_MSEC
		 */
		GET_CURRENT_SYSTIME(&rCurrentTime);

		i4NewEntryIndex = -1;
		for (i = 0; i < MAX_DEAUTH_INFO_COUNT; i++) {
			prDeauthInfo = &(prAdapter->rWifiVar.arDeauthInfo[i]);

			/* For continuously sending Deauth frame, the minimum
			 * interval is MIN_DEAUTH_INTERVAL_MSEC.
			 */
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
					      prDeauthInfo->rLastSendTime,
					      MSEC_TO_SYSTIME
					      (MIN_DEAUTH_INTERVAL_MSEC))) {

				i4NewEntryIndex = i;
			} else
			if (EQUAL_MAC_ADDR
				(pucReceiveAddr, prDeauthInfo->aucRxAddr)
				&& (!pfTxDoneHandler)) {

				return WLAN_STATUS_FAILURE;
			}
		}

		/* 4 <3> Update information. */
		if (i4NewEntryIndex > 0) {

			prDeauthInfo =
			    &(prAdapter->
			      rWifiVar.arDeauthInfo[i4NewEntryIndex]);

			COPY_MAC_ADDR(prDeauthInfo->aucRxAddr, pucReceiveAddr);
			prDeauthInfo->rLastSendTime = rCurrentTime;
		} else {
			/* NOTE(Kevin): for the case of AP mode, we may
			 * encounter this case
			 * if deauth all the associated clients.
			 */
			DBGLOG(SAA, WARN, "No unused DEAUTH_INFO_T !\n");
		}
	}
	/* 4 <5> Allocate a PKT_INFO_T for Deauthentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields + IE Length */
	u2EstimatedFrameLen =
	    (MAC_TX_RESERVED_FIELD + WLAN_MAC_MGMT_HEADER_LEN +
	     REASON_CODE_FIELD_LEN);

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN,
		       "No PKT_INFO_T for sending Deauth Request.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <6> compose Deauthentication frame header and some fixed fields */
	authComposeDeauthFrameHeaderAndFF((uint8_t *)
					  ((unsigned long)(prMsduInfo->prPacket)
					   + MAC_TX_RESERVED_FIELD),
					  pucReceiveAddr, pucTransmitAddr,
					  pucBssid, u2ReasonCode);

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		/* PMF certification 4.3.3.1, 4.3.3.2 send unprotected
		 * deauth reason 6/7
		 * if (AP mode & not for PMF reply case) OR (STA PMF)
		 */
		if (((GET_BSS_INFO_BY_INDEX
		      (prAdapter,
		       prStaRec->ucBssIndex)->eCurrentOPMode ==
		      OP_MODE_ACCESS_POINT)
		     && (prStaRec->rPmfCfg.fgRxDeauthResp != TRUE))
		    ||
		    (GET_BSS_INFO_BY_INDEX
		     (prAdapter,
		      prStaRec->ucBssIndex)->eNetworkType ==
		     (uint8_t) NETWORK_TYPE_AIS)) {

			struct WLAN_DEAUTH_FRAME *prDeauthFrame;

			prDeauthFrame = (struct WLAN_DEAUTH_FRAME *)(uint8_t *)
			    ((unsigned long)(prMsduInfo->prPacket)
			     + MAC_TX_RESERVED_FIELD);

			prDeauthFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
			DBGLOG(SAA, INFO,
			       "Reason=%d, DestAddr=" MACSTR
			       " srcAddr=" MACSTR " BSSID=" MACSTR "\n",
			       prDeauthFrame->u2ReasonCode,
			       MAC2STR(prDeauthFrame->aucDestAddr),
			       MAC2STR(prDeauthFrame->aucSrcAddr),
			       MAC2STR(prDeauthFrame->aucBSSID));
		}
	}
#endif
	nicTxSetPktLifeTime(prMsduInfo, 100);

	nicTxSetPktRetryLimit(prMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);

	/* 4 <7> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     ucBssIndex,
		     ucStaRecIdx,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + REASON_CODE_FIELD_LEN,
		     pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	/* caution: access prStaRec only if true */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		/* 4.3.3.1 send unprotected deauth reason 6/7 */
		if (prStaRec->rPmfCfg.fgRxDeauthResp != TRUE) {
			DBGLOG(RSN, INFO,
			       "Deauth Set MSDU_OPT_PROTECTED_FRAME\n");
			nicTxConfigPktOption(prMsduInfo,
					     MSDU_OPT_PROTECTED_FRAME, TRUE);
		}

		prStaRec->rPmfCfg.fgRxDeauthResp = FALSE;
	}
#endif
#if CFG_SUPPORT_CFG80211_AUTH
	prDeauthFrame = (struct WLAN_DEAUTH_FRAME *) (uint8_t *)
		((unsigned long) (prMsduInfo->prPacket) +
		MAC_TX_RESERVED_FIELD);
	DBGLOG(SAA, INFO, "notification of TX deauthentication, %d\n",
		prMsduInfo->u2FrameLength);

	cfg80211_tx_mlme_mgmt(prAdapter->prGlueInfo->prDevHandler,
		(uint8_t *)prDeauthFrame,
		(size_t)prMsduInfo->u2FrameLength);
	DBGLOG(SAA, INFO,
		"notification of TX deauthentication, Done\n");
#endif

	DBGLOG(SAA, INFO, "ucTxSeqNum=%d ucStaRecIndex=%d u2ReasonCode=%d\n",
	       prMsduInfo->ucTxSeqNum, prMsduInfo->ucStaRecIndex, u2ReasonCode);

	/* 4 <8> Inform TXM to send this Deauthentication frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of authSendDeauthFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Deauthentication
 *        frame if the given BSSID is matched.
 *
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 * @param[in] aucBSSID           Given BSSID
 * @param[out] pu2ReasonCode     Pointer to store the Reason Code from
 *                               Deauthentication.
 *
 * @retval WLAN_STATUS_FAILURE   This is not the frame we should handle at
 *                               current state.
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t authProcessRxDeauthFrame(IN struct SW_RFB *prSwRfb,
				  IN uint8_t aucBSSID[],
				  OUT uint16_t *pu2ReasonCode)
{
	struct WLAN_DEAUTH_FRAME *prDeauthFrame;
	uint16_t u2RxReasonCode;

	if (!prSwRfb || !aucBSSID || !pu2ReasonCode) {
		DBGLOG(SAA, WARN, "Invalid parameters, ignore pkt!\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <1> locate the Deauthentication Frame. */
	prDeauthFrame = (struct WLAN_DEAUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Deauthentication Frame. */
#if 0				/* Kevin: Seems redundant */
	WLAN_GET_FIELD_16(&prDeauthFrame->u2FrameCtrl, &u2RxFrameCtrl)
	    u2RxFrameCtrl &= MASK_FRAME_TYPE;
	if (u2RxFrameCtrl != MAC_FRAME_DEAUTH)
		return WLAN_STATUS_FAILURE;

#endif

	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
	    REASON_CODE_FIELD_LEN) {
		DBGLOG(SAA, WARN, "Invalid Deauth packet length");
		return WLAN_STATUS_FAILURE;
	}

	/* Check if this Deauth Frame is coming from Target BSSID */
	if (UNEQUAL_MAC_ADDR(prDeauthFrame->aucBSSID, aucBSSID)) {
		DBGLOG(SAA, LOUD,
		       "Ignore Deauth Frame from other BSS [" MACSTR "]\n",
		       MAC2STR(prDeauthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}
	/* 4 <3> Parse the Fixed Fields of Deauthentication Frame Body. */
	WLAN_GET_FIELD_16(&prDeauthFrame->u2ReasonCode, &u2RxReasonCode);
	*pu2ReasonCode = u2RxReasonCode;

	return WLAN_STATUS_SUCCESS;

}		/* end of authProcessRxDeauthFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Authentication
 *        frame.
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[in] aucExpectedBSSID       Given Expected BSSID.
 * @param[in] u2ExpectedAuthAlgNum   Given Expected Authentication Algorithm
 *                                   Number
 * @param[in] u2ExpectedTransSeqNum  Given Expected Transaction Sequence Number.
 * @param[out] pu2ReturnStatusCode   Return Status Code.
 *
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 * @retval WLAN_STATUS_FAILURE   The frame we will ignore.
 */
/*----------------------------------------------------------------------------*/
uint32_t
authProcessRxAuth1Frame(IN struct ADAPTER *prAdapter,
			IN struct SW_RFB *prSwRfb,
			IN uint8_t aucExpectedBSSID[],
			IN uint16_t u2ExpectedAuthAlgNum,
			IN uint16_t u2ExpectedTransSeqNum,
			OUT uint16_t *pu2ReturnStatusCode)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2ReturnStatusCode = STATUS_CODE_SUCCESSFUL;

	ASSERT(prSwRfb);
	ASSERT(aucExpectedBSSID);
	ASSERT(pu2ReturnStatusCode);

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Check the BSSID */
	if (UNEQUAL_MAC_ADDR(prAuthFrame->aucBSSID, aucExpectedBSSID))
		return WLAN_STATUS_FAILURE;	/* Just Ignore this MMPDU */

	/* 4 <3> Check the SA, which should not be MC/BC */
	if (prAuthFrame->aucSrcAddr[0] & BIT(0)) {
		DBGLOG(P2P, WARN,
		       "Invalid STA MAC with MC/BC bit set: " MACSTR "\n",
		       MAC2STR(prAuthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <4> Parse the Fixed Fields of Authentication Frame Body. */
	if (prAuthFrame->u2AuthAlgNum != u2ExpectedAuthAlgNum)
		u2ReturnStatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;

#if CFG_SUPPORT_CFG80211_AUTH
	if (prAuthFrame->aucAuthData[0] != u2ExpectedTransSeqNum)
#else
	if (prAuthFrame->u2AuthTransSeqNo != u2ExpectedTransSeqNum)
#endif
		u2ReturnStatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;

	*pu2ReturnStatusCode = u2ReturnStatusCode;

	return WLAN_STATUS_SUCCESS;

}				/* end of authProcessRxAuth1Frame() */

/* ToDo: authAddRicIE, authHandleFtIEs, authAddTimeoutIE */

void authAddMDIE(IN struct ADAPTER *prAdapter,
		 IN OUT struct MSDU_INFO *prMsduInfo)
{
	struct FT_IES *prFtIEs = &prAdapter->prGlueInfo->rFtIeForTx;
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prMDIE)
		return;
	prMsduInfo->u2FrameLength +=
		5; /* IE size for MD IE is fixed, it is 5 */
	kalMemCopy(pucBuffer, prFtIEs->prMDIE, 5);
}

uint32_t authCalculateRSNIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			       struct STA_RECORD *prStaRec)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
		prAdapter->rWifiVar.rConnSettings.eAuthMode;
	struct FT_IES *prFtIEs = &prAdapter->prGlueInfo->rFtIeForTx;

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prRsnIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				  eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return 0;
	return IE_SIZE(prFtIEs->prRsnIE);
}

void authAddRSNIE(IN struct ADAPTER *prAdapter,
		  IN OUT struct MSDU_INFO *prMsduInfo)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
		prAdapter->rWifiVar.rConnSettings.eAuthMode;
	struct FT_IES *prFtIEs = &prAdapter->prGlueInfo->rFtIeForTx;
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint32_t ucRSNIeSize = 0;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prRsnIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				  eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return;
	ucRSNIeSize = IE_SIZE(prFtIEs->prRsnIE);
	prMsduInfo->u2FrameLength += ucRSNIeSize;
	kalMemCopy(pucBuffer, prFtIEs->prRsnIE, ucRSNIeSize);
}

