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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/auth.c#1
*/

/*! \file   "auth.c"
    \brief  This file includes the authentication-related functions.

    This file includes the authentication-related functions.
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
APPEND_IE_ENTRY_T txAuthIETable[] = {
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_CHALLENGE_TEXT), authAddIEChallengeText}
};

HANDLE_IE_ENTRY_T rxAuthIETable[] = {
	{ELEM_ID_CHALLENGE_TEXT, authHandleIEChallengeText}
};

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
* @brief This function will compose the Authentication frame header and fixed fields.
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
__KAL_INLINE__ VOID
authComposeAuthFrameHeaderAndFF(IN PUINT_8 pucBuffer,
				IN UINT_8 aucPeerMACAddress[],
				IN UINT_8 aucMACAddress[],
				IN UINT_16 u2AuthAlgNum, IN UINT_16 u2TransactionSeqNum, IN UINT_16 u2StatusCode)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	UINT_16 u2FrameCtrl;

	ASSERT(pucBuffer);
	ASSERT(aucPeerMACAddress);
	ASSERT(aucMACAddress);

	prAuthFrame = (P_WLAN_AUTH_FRAME_T) pucBuffer;

	/* 4 <1> Compose the frame header of the Authentication frame. */
	/* Fill the Frame Control field. */
	u2FrameCtrl = MAC_FRAME_AUTH;

	/* If this frame is the third frame in the shared key authentication
	 * sequence, it shall be encrypted.
	 */
	if ((u2AuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY) && (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3))
		u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;	/* HW will also detect this bit for applying encryption */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2FrameCtrl, u2FrameCtrl); */
	prAuthFrame->u2FrameCtrl = u2FrameCtrl;	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prAuthFrame->aucDestAddr, aucPeerMACAddress);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prAuthFrame->aucSrcAddr, aucMACAddress);

	switch (u2TransactionSeqNum) {
	case AUTH_TRANSACTION_SEQ_1:
	case AUTH_TRANSACTION_SEQ_3:

		/* Fill the BSSID field with Target BSSID. */
		COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucPeerMACAddress);
		break;

	case AUTH_TRANSACTION_SEQ_2:
	case AUTH_TRANSACTION_SEQ_4:

		/* Fill the BSSID field with Current BSSID. */
		COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucMACAddress);
		break;

	default:
		ASSERT(0);
	}

	/* Clear the SEQ/FRAG_NO field. */
	prAuthFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's fixed field part of the Authentication frame. */
	/* Fill the Authentication Algorithm Number field. */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2AuthAlgNum, u2AuthAlgNum); */
	prAuthFrame->u2AuthAlgNum = u2AuthAlgNum;	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the Authentication Transaction Sequence Number field. */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, u2TransactionSeqNum); */
	prAuthFrame->u2AuthTransSeqNo = u2TransactionSeqNum;	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the Status Code field. */
	/* WLAN_SET_FIELD_16(&prAuthFrame->u2StatusCode, u2StatusCode); */
	prAuthFrame->u2StatusCode = u2StatusCode;	/* NOTE(Kevin): Optimized for ARM */

}				/* end of authComposeAuthFrameHeaderAndFF() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will append Challenge Text IE to the Authentication frame
*
* @param[in] prMsduInfo     Pointer to the composed MSDU_INFO_T.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID authAddIEChallengeText(IN P_ADAPTER_T prAdapter, IN OUT P_MSDU_INFO_T prMsduInfo)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	P_STA_RECORD_T prStaRec;
	UINT_16 u2TransactionSeqNum;

	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	ASSERT(prStaRec);

	/* For Management, frame header and payload are in a continuous buffer */
	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prMsduInfo->prPacket;

	WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2TransactionSeqNum)

	    /* Only consider SEQ_3 for Challenge Text */
	    if ((u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3) &&
		(prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY) && (prStaRec->prChallengeText != NULL)) {

		COPY_IE(((ULONG) (prMsduInfo->prPacket) + prMsduInfo->u2FrameLength), (prStaRec->prChallengeText));

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
WLAN_STATUS authSendAuthFrame(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN UINT_16 u2TransactionSeqNum)
{
	P_MSDU_INFO_T prMsduInfo;
	P_BSS_INFO_T prBssInfo;
	UINT_16 u2EstimatedFrameLen;
	UINT_16 u2EstimatedExtraIELen;
	UINT_16 u2PayloadLen;
	UINT_32 i;

	ASSERT(prStaRec);

	/* 4 <1> Allocate a PKT_INFO_T for Authentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields */
	u2EstimatedFrameLen = (MAC_TX_RESERVED_FIELD +
			       WLAN_MAC_MGMT_HEADER_LEN +
			       AUTH_ALGORITHM_NUM_FIELD_LEN +
			       AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	/* + Extra IE Length */
	u2EstimatedExtraIELen = 0;

	for (i = 0; i < sizeof(txAuthIETable) / sizeof(APPEND_IE_ENTRY_T); i++)
		u2EstimatedExtraIELen += txAuthIETable[i].u2EstimatedIELen;

	u2EstimatedFrameLen += u2EstimatedExtraIELen;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN, "No MSDU_INFO_T for sending Auth\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose Authentication Request frame header and fixed fields in MSDU_INfO_T. */
	ASSERT(prStaRec->ucBssIndex <= MAX_BSS_INDEX);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex)

	/* Compose Header and some Fixed Fields */
	authComposeAuthFrameHeaderAndFF((PUINT_8)
					((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD),
					prStaRec->aucMacAddr,
					prBssInfo->aucOwnMacAddr, prStaRec->ucAuthAlgNum,
					u2TransactionSeqNum, STATUS_CODE_RESERVED);

	u2PayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN + AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, saaFsmRunEventTxDone, MSDU_RATE_MODE_AUTO);

	/* 4 <4> Compose IEs in MSDU_INFO_T */
	for (i = 0; i < sizeof(txAuthIETable) / sizeof(APPEND_IE_ENTRY_T); i++) {
		if (txAuthIETable[i].pfnAppendIE)
			txAuthIETable[i].pfnAppendIE(prAdapter, prMsduInfo);

	}

	/* TODO(Kevin): Also release the unused tail room of the composed MMPDU */

	nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	/* 4 <6> Inform TXM  to send this Authentication frame. */
	DBGLOG(SAA, TRACE, "Send Auth, SeqNo: %d\n", prMsduInfo->ucTxSeqNum);
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
WLAN_STATUS
authSendAuthFrame(IN P_ADAPTER_T prAdapter,
		  IN P_STA_RECORD_T prStaRec,
		  IN UINT_8 ucBssIndex,
		  IN P_SW_RFB_T prFalseAuthSwRfb, IN UINT_16 u2TransactionSeqNum, IN UINT_16 u2StatusCode)
{
	PUINT_8 pucReceiveAddr;
	PUINT_8 pucTransmitAddr;
	P_MSDU_INFO_T prMsduInfo;
	P_BSS_INFO_T prBssInfo;
	/*get from input parameter */
	/* ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex = NETWORK_TYPE_AIS_INDEX; */
	PFN_TX_DONE_HANDLER pfTxDoneHandler = (PFN_TX_DONE_HANDLER) NULL;
	UINT_16 u2EstimatedFrameLen;
	UINT_16 u2EstimatedExtraIELen;
	UINT_16 u2PayloadLen;
	UINT_16 ucAuthAlgNum;
	UINT_32 i;

	/* 4 <1> Allocate a PKT_INFO_T for Authentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
			      WLAN_MAC_MGMT_HEADER_LEN +
			      AUTH_ALGORITHM_NUM_FIELD_LEN +
			      AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN;

	/* + Extra IE Length */
	u2EstimatedExtraIELen = 0;

	for (i = 0; i < sizeof(txAuthIETable) / sizeof(APPEND_IE_ENTRY_T); i++)
		u2EstimatedExtraIELen += txAuthIETable[i].u2EstimatedIELen;

	u2EstimatedFrameLen += u2EstimatedExtraIELen;

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN, "No MSDU_INFO_T for sending Auth\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <2> Compose Authentication Request frame header and fixed fields in MSDU_INfO_T. */
	if (prStaRec) {
		ASSERT(prStaRec->ucBssIndex <= MAX_BSS_INDEX);
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

		pucTransmitAddr = prBssInfo->aucOwnMacAddr;

		pucReceiveAddr = prStaRec->aucMacAddr;

		ucAuthAlgNum = prStaRec->ucAuthAlgNum;

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

	} else {		/* For Error Status Code */
		P_WLAN_AUTH_FRAME_T prFalseAuthFrame;

		ASSERT(prFalseAuthSwRfb);
		prFalseAuthFrame = (P_WLAN_AUTH_FRAME_T) prFalseAuthSwRfb->pvHeader;

		ASSERT(u2StatusCode != STATUS_CODE_SUCCESSFUL);

		pucTransmitAddr = prFalseAuthFrame->aucDestAddr;

		pucReceiveAddr = prFalseAuthFrame->aucSrcAddr;

		ucAuthAlgNum = prFalseAuthFrame->u2AuthAlgNum;

		u2TransactionSeqNum = (prFalseAuthFrame->u2AuthTransSeqNo + 1);
	}

	/* Compose Header and some Fixed Fields */
	authComposeAuthFrameHeaderAndFF((PUINT_8)
					((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD),
					pucReceiveAddr, pucTransmitAddr, ucAuthAlgNum,
					u2TransactionSeqNum, u2StatusCode);

	u2PayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN + AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	/* 4 <3> Update information of MSDU_INFO_T */

	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     ucBssIndex,
		     (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	if ((ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY) && (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3))
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
	/* 4 <4> Compose IEs in MSDU_INFO_T */
	for (i = 0; i < sizeof(txAuthIETable) / sizeof(APPEND_IE_ENTRY_T); i++) {
		if (txAuthIETable[i].pfnAppendIE)
			txAuthIETable[i].pfnAppendIE(prAdapter, prMsduInfo);

	}

	/* TODO(Kevin): Also release the unused tail room of the composed MMPDU */

	nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	/* 4 <6> Inform TXM  to send this Authentication frame. */
	DBGLOG(SAA, TRACE, "Send Auth, StatusCode: %d, SeqNo: %d\n", u2StatusCode, prMsduInfo->ucTxSeqNum);
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of authSendAuthFrame() */

#endif /* CFG_SUPPORT_AAA */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will strictly check the TX Authentication frame for SAA/AAA event
*        handling.
*
* @param[in] prMsduInfo             Pointer of MSDU_INFO_T
* @param[in] u2TransactionSeqNum    Transaction Sequence Number
*
* @retval WLAN_STATUS_FAILURE   This is not the frame we should handle at current state.
* @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS authCheckTxAuthFrame(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN UINT_16 u2TransactionSeqNum)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	P_STA_RECORD_T prStaRec;
	UINT_16 u2TxFrameCtrl;
	UINT_16 u2TxAuthAlgNum;
	UINT_16 u2TxTransactionSeqNum;

	ASSERT(prMsduInfo);

	prAuthFrame = (P_WLAN_AUTH_FRAME_T) (prMsduInfo->prPacket);
	ASSERT(prAuthFrame);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2FrameCtrl, &u2TxFrameCtrl) */
	u2TxFrameCtrl = prAuthFrame->u2FrameCtrl;	/* NOTE(Kevin): Optimized for ARM */
	u2TxFrameCtrl &= MASK_FRAME_TYPE;
	if (u2TxFrameCtrl != MAC_FRAME_AUTH)
		return WLAN_STATUS_FAILURE;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthAlgNum, &u2TxAuthAlgNum) */
	u2TxAuthAlgNum = prAuthFrame->u2AuthAlgNum;	/* NOTE(Kevin): Optimized for ARM */
	if (u2TxAuthAlgNum != (UINT_16) (prStaRec->ucAuthAlgNum))
		return WLAN_STATUS_FAILURE;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2TxTransactionSeqNum) */
	u2TxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;	/* NOTE(Kevin): Optimized for ARM */
	if (u2TxTransactionSeqNum != u2TransactionSeqNum)
		return WLAN_STATUS_FAILURE;

	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckTxAuthFrame() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will check the incoming Auth Frame's Transaction Sequence
*        Number before delivering it to the corresponding SAA or AAA Module.
*
* @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
*
* @retval WLAN_STATUS_SUCCESS   Always not retain authentication frames
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS authCheckRxAuthFrameTransSeq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	UINT_16 u2RxTransactionSeqNum;
	UINT_16 u2MinPayloadLen;

	ASSERT(prSwRfb);

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Authentication Frame. */
	u2MinPayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN +
			   AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) < u2MinPayloadLen) {
		DBGLOG(SAA, WARN, "Rx Auth payload: len[%u] < min expected len[%u]\n",
				   (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen), u2MinPayloadLen);
		DBGLOG(SAA, WARN, "=== Dump Rx Auth ===\n");
		DBGLOG_MEM8(SAA, WARN, prAuthFrame, prSwRfb->u2PacketLen);
		return WLAN_STATUS_SUCCESS;
	}
	/* 4 <3> Parse the Fixed Fields of Authentication Frame Body. */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2RxTransactionSeqNum); */
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;	/* NOTE(Kevin): Optimized for ARM */

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
		       "Strange Authentication Packet: Auth Trans Seq No = %d, Error Status Code = %d\n",
			u2RxTransactionSeqNum, prAuthFrame->u2StatusCode);
		break;
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckRxAuthFrameTransSeq() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the incoming Authentication Frame and take
*        the status code out.
*
* @param[in] prSwRfb                Pointer to SW RFB data structure.
* @param[in] u2TransactionSeqNum    Transaction Sequence Number
* @param[out] pu2StatusCode         Pointer to store the Status Code from Authentication.
*
* @retval WLAN_STATUS_FAILURE   This is not the frame we should handle at current state.
* @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
authCheckRxAuthFrameStatus(IN P_ADAPTER_T prAdapter,
			   IN P_SW_RFB_T prSwRfb, IN UINT_16 u2TransactionSeqNum, OUT PUINT_16 pu2StatusCode)
{
	P_STA_RECORD_T prStaRec;
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	UINT_16 u2RxAuthAlgNum;
	UINT_16 u2RxTransactionSeqNum;
	/* UINT_16 u2RxStatusCode; // NOTE(Kevin): Optimized for ARM */

	ASSERT(prSwRfb);
	ASSERT(pu2StatusCode);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec) {
		DBGLOG(SAA, ERROR, "Cannot find corresponding StaRec, invalid packet\n");
		return WLAN_STATUS_INVALID_PACKET;
	}

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	/* 4 <2> Parse the Fixed Fields of Authentication Frame Body. */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthAlgNum, &u2RxAuthAlgNum); */
	u2RxAuthAlgNum = prAuthFrame->u2AuthAlgNum;	/* NOTE(Kevin): Optimized for ARM */
	if (u2RxAuthAlgNum != (UINT_16) prStaRec->ucAuthAlgNum) {
		DBGLOG(SAA, WARN, "Auth frame with AlgNum: %d, expected: %d\n",
		       u2RxAuthAlgNum, prStaRec->ucAuthAlgNum);
		*pu2StatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;
		return WLAN_STATUS_SUCCESS;
	}
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2RxTransactionSeqNum); */
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;	/* NOTE(Kevin): Optimized for ARM */
	if (u2RxTransactionSeqNum != u2TransactionSeqNum) {
		DBGLOG(SAA, WARN, "Auth frame with TransactionSeqNum: %d, expected: %d\n",
		       u2RxTransactionSeqNum, u2TransactionSeqNum);
		*pu2StatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;
		return WLAN_STATUS_FAILURE;
	}
	/* 4 <3> Get the Status code */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2StatusCode, &u2RxStatusCode); */
	/* *pu2StatusCode = u2RxStatusCode; */
	*pu2StatusCode = prAuthFrame->u2StatusCode;	/* NOTE(Kevin): Optimized for ARM */

	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckRxAuthFrameStatus() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Challenge Text IE from the Authentication frame
*
* @param[in] prSwRfb                Pointer to SW RFB data structure.
* @param[in] prIEHdr                Pointer to start address of IE
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID authHandleIEChallengeText(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb, P_IE_HDR_T prIEHdr)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	P_STA_RECORD_T prStaRec;
	UINT_16 u2TransactionSeqNum;

	ASSERT(prSwRfb);
	ASSERT(prIEHdr);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (!prStaRec)
		return;

	/* For Management, frame header and payload are in a continuous buffer */
	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2TransactionSeqNum) */
	u2TransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;	/* NOTE(Kevin): Optimized for ARM */

	/* Only consider SEQ_2 for Challenge Text */
	if ((u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_2) &&
	    (prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)) {

		/* Free previous allocated TCM memory */
		if (prStaRec->prChallengeText) {
			/* ASSERT(0); */
			cnmMemFree(prAdapter, prStaRec->prChallengeText);
			prStaRec->prChallengeText = (P_IE_CHALLENGE_TEXT_T) NULL;
		}
		prStaRec->prChallengeText = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, IE_SIZE(prIEHdr));
		if (prStaRec->prChallengeText == NULL)
			return;

		/* Save the Challenge Text from Auth Seq 2 Frame, before sending Auth Seq 3 Frame */
		COPY_IE(prStaRec->prChallengeText, prIEHdr);
	}

	return;

}				/* end of authAddIEChallengeText() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will parse and process the incoming Authentication frame.
*
* @param[in] prSwRfb            Pointer to SW RFB data structure.
*
* @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS authProcessRxAuth2_Auth4Frame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	PUINT_8 pucIEsBuffer;
	UINT_16 u2IEsLen;
	UINT_16 u2Offset;
	UINT_8 ucIEID;
	UINT_32 i;

	ASSERT(prSwRfb);

	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	pucIEsBuffer = &prAuthFrame->aucInfoElem[0];
	u2IEsLen = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (AUTH_ALGORITHM_NUM_FIELD_LEN + AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	IE_FOR_EACH(pucIEsBuffer, u2IEsLen, u2Offset) {
		ucIEID = IE_ID(pucIEsBuffer);

		for (i = 0; i < (sizeof(rxAuthIETable) / sizeof(HANDLE_IE_ENTRY_T)); i++) {
			if ((ucIEID == rxAuthIETable[i].ucElemID) && (rxAuthIETable[i].pfnHandleIE != NULL))
				rxAuthIETable[i].pfnHandleIE(prAdapter, prSwRfb, (P_IE_HDR_T) pucIEsBuffer);
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
__KAL_INLINE__ VOID
authComposeDeauthFrameHeaderAndFF(IN PUINT_8 pucBuffer,
				  IN UINT_8 aucPeerMACAddress[],
				  IN UINT_8 aucMACAddress[], IN UINT_8 aucBssid[], IN UINT_16 u2ReasonCode)
{
	P_WLAN_DEAUTH_FRAME_T prDeauthFrame;
	UINT_16 u2FrameCtrl;

	ASSERT(pucBuffer);
	ASSERT(aucPeerMACAddress);
	ASSERT(aucMACAddress);
	ASSERT(aucBssid);

	prDeauthFrame = (P_WLAN_DEAUTH_FRAME_T) pucBuffer;

	/* 4 <1> Compose the frame header of the Deauthentication frame. */
	/* Fill the Frame Control field. */
	u2FrameCtrl = MAC_FRAME_DEAUTH;

	/* WLAN_SET_FIELD_16(&prDeauthFrame->u2FrameCtrl, u2FrameCtrl); */
	prDeauthFrame->u2FrameCtrl = u2FrameCtrl;	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prDeauthFrame->aucDestAddr, aucPeerMACAddress);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prDeauthFrame->aucSrcAddr, aucMACAddress);

	/* Fill the BSSID field with Target BSSID. */
	COPY_MAC_ADDR(prDeauthFrame->aucBSSID, aucBssid);

	/* Clear the SEQ/FRAG_NO field(HW won't overide the FRAG_NO, so we need to clear it). */
	prDeauthFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's fixed field part of the Authentication frame. */
	/* Fill the Status Code field. */
	/* WLAN_SET_FIELD_16(&prDeauthFrame->u2ReasonCode, u2ReasonCode); */
	prDeauthFrame->u2ReasonCode = u2ReasonCode;	/* NOTE(Kevin): Optimized for ARM */

}				/* end of authComposeDeauthFrameHeaderAndFF() */

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
WLAN_STATUS
authSendDeauthFrame(IN P_ADAPTER_T prAdapter,
		    IN P_BSS_INFO_T prBssInfo,
		    IN P_STA_RECORD_T prStaRec,
		    IN P_SW_RFB_T prClassErrSwRfb, IN UINT_16 u2ReasonCode, IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	PUINT_8 pucReceiveAddr;
	PUINT_8 pucTransmitAddr;
	PUINT_8 pucBssid = NULL;
	P_MSDU_INFO_T prMsduInfo;
	UINT_16 u2EstimatedFrameLen;

	P_DEAUTH_INFO_T prDeauthInfo;
	OS_SYSTIME rCurrentTime;
	INT_32 i4NewEntryIndex, i;
	UINT_8 ucStaRecIdx = STA_REC_INDEX_NOT_FOUND;
	UINT_8 ucBssIndex = BSS_INFO_NUM;
	UINT_8 aucBMC[] = BC_MAC_ADDR;

	/* NOTE(Kevin): The best way to reply the Deauth is according to the incoming data
	 * frame
	 */
	/* 4 <1.1> Find the Receiver Address */
	if (prClassErrSwRfb) {
		BOOLEAN fgIsAbleToSendDeauth = FALSE;
		UINT_16 u2RxFrameCtrl;
		P_WLAN_MAC_HEADER_A4_T prWlanMacHeader = NULL;

		prWlanMacHeader = (P_WLAN_MAC_HEADER_A4_T) prClassErrSwRfb->pvHeader;

		/* WLAN_GET_FIELD_16(&prWlanMacHeader->u2FrameCtrl, &u2RxFrameCtrl); */
		u2RxFrameCtrl = prWlanMacHeader->u2FrameCtrl;	/* NOTE(Kevin): Optimized for ARM */

		/* TODO(Kevin): Currently we won't send Deauth for IBSS node. How about DLS ? */
		if ((prWlanMacHeader->u2FrameCtrl & MASK_TO_DS_FROM_DS) == 0)
			return WLAN_STATUS_FAILURE;

		/* Check if corresponding BSS is able to send Deauth */
		for (i = 0; i < BSS_INFO_NUM; i++) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

			if (IS_NET_ACTIVE(prAdapter, i) &&
			    (EQUAL_MAC_ADDR(prWlanMacHeader->aucAddr1, prBssInfo->aucOwnMacAddr))) {

				fgIsAbleToSendDeauth = TRUE;
				ucBssIndex = (UINT_8) i;
				break;
			}
		}

		if (!fgIsAbleToSendDeauth)
			return WLAN_STATUS_FAILURE;

		pucReceiveAddr = prWlanMacHeader->aucAddr2;
	} else if (prStaRec) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
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
		/* 4 <2> Check if already send a Deauth frame in MIN_DEAUTH_INTERVAL_MSEC */
		GET_CURRENT_SYSTIME(&rCurrentTime);

		i4NewEntryIndex = -1;
		for (i = 0; i < MAX_DEAUTH_INFO_COUNT; i++) {
			prDeauthInfo = &(prAdapter->rWifiVar.arDeauthInfo[i]);

			/* For continuously sending Deauth frame, the minimum interval is
			 * MIN_DEAUTH_INTERVAL_MSEC.
			 */
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
					      prDeauthInfo->rLastSendTime, MSEC_TO_SYSTIME(MIN_DEAUTH_INTERVAL_MSEC))) {

				i4NewEntryIndex = i;
			} else if (EQUAL_MAC_ADDR(pucReceiveAddr, prDeauthInfo->aucRxAddr) && (!pfTxDoneHandler)) {

				return WLAN_STATUS_FAILURE;
			}
		}

		/* 4 <3> Update information. */
		if (i4NewEntryIndex > 0) {

			prDeauthInfo = &(prAdapter->rWifiVar.arDeauthInfo[i4NewEntryIndex]);

			COPY_MAC_ADDR(prDeauthInfo->aucRxAddr, pucReceiveAddr);
			prDeauthInfo->rLastSendTime = rCurrentTime;
		} else {
			/* NOTE(Kevin): for the case of AP mode, we may encounter this case
			 * if deauth all the associated clients.
			 */
			DBGLOG(SAA, WARN, "No unused DEAUTH_INFO_T !\n");
		}
	}
	/* 4 <5> Allocate a PKT_INFO_T for Deauthentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields + IE Length */
	u2EstimatedFrameLen = (MAC_TX_RESERVED_FIELD + WLAN_MAC_MGMT_HEADER_LEN + REASON_CODE_FIELD_LEN);

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN, "No MSDU_INFO_T for sending Deauth\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <6> compose Deauthentication frame header and some fixed fields */
	authComposeDeauthFrameHeaderAndFF((PUINT_8)
					  ((ULONG) (prMsduInfo->prPacket) +
					   MAC_TX_RESERVED_FIELD), pucReceiveAddr, pucTransmitAddr,
					  pucBssid, u2ReasonCode);

#if CFG_SUPPORT_802_11W
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		P_WLAN_DEAUTH_FRAME_T prDeauthFrame;

		prDeauthFrame =
		    (P_WLAN_DEAUTH_FRAME_T) (PUINT_8) ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

		prDeauthFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
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
		     WLAN_MAC_MGMT_HEADER_LEN + REASON_CODE_FIELD_LEN, pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec))
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);

#endif

	/* 4 <8> Inform TXM to send this Deauthentication frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of authSendDeauthFrame() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will parse and process the incoming Deauthentication frame
*        if the given BSSID is matched.
*
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[in] aucBSSID           Given BSSID
* @param[out] pu2ReasonCode     Pointer to store the Reason Code from Deauthentication.
*
* @retval WLAN_STATUS_FAILURE   This is not the frame we should handle at current state.
* @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS authProcessRxDeauthFrame(IN P_SW_RFB_T prSwRfb, IN UINT_8 aucBSSID[], OUT PUINT_16 pu2ReasonCode)
{
	P_WLAN_DEAUTH_FRAME_T prDeauthFrame;
	UINT_16 u2RxReasonCode;

	ASSERT(prSwRfb);
	ASSERT(aucBSSID);
	ASSERT(pu2ReasonCode);

	/* 4 <1> locate the Deauthentication Frame. */
	prDeauthFrame = (P_WLAN_DEAUTH_FRAME_T) prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Deauthentication Frame. */
#if 0				/* Kevin: Seems redundant */
	WLAN_GET_FIELD_16(&prDeauthFrame->u2FrameCtrl, &u2RxFrameCtrl)
	    u2RxFrameCtrl &= MASK_FRAME_TYPE;
	if (u2RxFrameCtrl != MAC_FRAME_DEAUTH)
		return WLAN_STATUS_FAILURE;

#endif

	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) < REASON_CODE_FIELD_LEN) {
		ASSERT(0);
		return WLAN_STATUS_FAILURE;
	}

	/* Check if this Deauth Frame is coming from Target BSSID */
	if (UNEQUAL_MAC_ADDR(prDeauthFrame->aucBSSID, aucBSSID)) {
		DBGLOG(SAA, LOUD, "Ignore Deauth Frame from other BSS [" MACSTR "]\n",
				   MAC2STR(prDeauthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}
	/* 4 <3> Parse the Fixed Fields of Deauthentication Frame Body. */
	WLAN_GET_FIELD_16(&prDeauthFrame->u2ReasonCode, &u2RxReasonCode);
	*pu2ReasonCode = u2RxReasonCode;
	DBGLOG(SAA, INFO, "RX deauth, reason code: %d\n", u2RxReasonCode);

	return WLAN_STATUS_SUCCESS;

}				/* end of authProcessRxDeauthFrame() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will parse and process the incoming Authentication frame.
*
* @param[in] prSwRfb                Pointer to SW RFB data structure.
* @param[in] aucExpectedBSSID       Given Expected BSSID.
* @param[in] u2ExpectedAuthAlgNum   Given Expected Authentication Algorithm Number
* @param[in] u2ExpectedTransSeqNum  Given Expected Transaction Sequence Number.
* @param[out] pu2StatusCode   Pointer to store the Status Code carried in returned Authentication.
*
* @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
* @retval WLAN_STATUS_FAILURE   The frame we will ignore.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
authProcessRxAuth1Frame(IN P_ADAPTER_T prAdapter,
			IN P_SW_RFB_T prSwRfb,
			IN UINT_8 aucExpectedBSSID[],
			IN UINT_16 u2ExpectedAuthAlgNum,
			IN UINT_16 u2ExpectedTransSeqNum, OUT PUINT_16 pu2StatusCode)
{
	P_WLAN_AUTH_FRAME_T prAuthFrame;
	UINT_16 u2StatusCode = STATUS_CODE_SUCCESSFUL;

	ASSERT(prSwRfb);
	ASSERT(aucExpectedBSSID);
	ASSERT(pu2StatusCode);

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (P_WLAN_AUTH_FRAME_T) prSwRfb->pvHeader;

	/* 4 <2> Check the BSSID */
	if (UNEQUAL_MAC_ADDR(prAuthFrame->aucBSSID, aucExpectedBSSID)) {
		DBGLOG(AAA, ERROR, "Auth frame is not from current BSSID\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <3> Check the SA, which should not be MC/BC */
	if (prAuthFrame->aucSrcAddr[0] & BIT(0)) {
		DBGLOG(P2P, WARN, "Invalid STA MAC with MC/BC bit set: " MACSTR "\n",
		       MAC2STR(prAuthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <4> Parse the Fixed Fields of Authentication Frame Body. */
	if (prAuthFrame->u2AuthAlgNum != u2ExpectedAuthAlgNum)
		u2StatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;

	if (prAuthFrame->u2AuthTransSeqNo != u2ExpectedTransSeqNum)
		u2StatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;

	*pu2StatusCode = u2StatusCode;

	return WLAN_STATUS_SUCCESS;

}				/* end of authProcessRxAuth1Frame() */
