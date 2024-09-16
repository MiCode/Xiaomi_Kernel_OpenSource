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
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_tx.c#2
 */

/*! \file   nic_tx.c
 *    \brief  Functions that provide TX operation in NIC Layer.
 *
 *    This file provides TX functions which are responsible for both Hardware
 *    and Software Resource Management and keep their Synchronization.
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
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
uint8_t nic_txd_v1_long_format_op(
	void *prTxDesc,
	uint8_t fgSet)
{
	if (fgSet)
		HAL_MAC_TX_DESC_SET_LONG_FORMAT(
			(struct HW_MAC_TX_DESC *)prTxDesc);
	return HAL_MAC_TX_DESC_IS_LONG_FORMAT(
			(struct HW_MAC_TX_DESC *)prTxDesc);
}

uint8_t nic_txd_v1_tid_op(
	void *prTxDesc,
	uint8_t ucTid,
	uint8_t fgSet)
{
	if (fgSet)
		HAL_MAC_TX_DESC_SET_TID(
			(struct HW_MAC_TX_DESC *)prTxDesc, ucTid);
	return HAL_MAC_TX_DESC_GET_TID(
			(struct HW_MAC_TX_DESC *)prTxDesc);
}

uint8_t nic_txd_v1_queue_idx_op(
	void *prTxDesc,
	uint8_t ucQueIdx,
	uint8_t fgSet)
{
	if (fgSet)
		HAL_MAC_TX_DESC_SET_QUEUE_INDEX(
			(struct HW_MAC_TX_DESC *)prTxDesc, ucQueIdx);
	return HAL_MAC_TX_DESC_GET_QUEUE_INDEX(
			(struct HW_MAC_TX_DESC *)prTxDesc);
}

#if (CFG_TCP_IP_CHKSUM_OFFLOAD == 1)
void nic_txd_v1_chksum_op(
	void *prTxDesc,
	uint8_t ucChksumFlag)
{
	if ((ucChksumFlag & TX_CS_IP_GEN))
		HAL_MAC_TX_DESC_SET_IP_CHKSUM(
			(struct HW_MAC_TX_DESC *)prTxDesc);
	if ((ucChksumFlag & TX_CS_TCP_UDP_GEN))
		HAL_MAC_TX_DESC_SET_TCP_UDP_CHKSUM(
			(struct HW_MAC_TX_DESC *)prTxDesc);
}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD == 1 */

void nic_txd_v1_header_format_op(
	void *prTxDesc,
	struct MSDU_INFO *prMsduInfo)
{
	if (!prMsduInfo->fgIs802_11) {
		if (prMsduInfo->fgIs802_3)
			HAL_MAC_TX_DESC_UNSET_ETHERNET_II(
				(struct HW_MAC_TX_DESC *)prTxDesc);
		if (prMsduInfo->fgIsVlanExists)
			HAL_MAC_TX_DESC_SET_VLAN(
				(struct HW_MAC_TX_DESC *)prTxDesc);
	}
}

void nic_txd_v1_fill_by_pkt_option(
	struct MSDU_INFO *prMsduInfo,
	void *prTxD)
{
	struct HW_MAC_TX_DESC *prTxDesc = (struct HW_MAC_TX_DESC *)prTxD;
	uint32_t u4PktOption = prMsduInfo->u4Option;
	u_int8_t fgIsLongFormat;
	u_int8_t fgProtected = FALSE;

	/* Skip this function if no options is set */
	if (!u4PktOption)
		return;

	fgIsLongFormat = HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDesc);

	/* Fields in DW0 and DW1 (Short Format) */
	if (u4PktOption & MSDU_OPT_NO_ACK)
		HAL_MAC_TX_DESC_SET_NO_ACK(prTxDesc);

	if (u4PktOption & MSDU_OPT_PROTECTED_FRAME) {
		/* DBGLOG(RSN, INFO, "MSDU_OPT_PROTECTED_FRAME\n"); */
		HAL_MAC_TX_DESC_SET_PROTECTION(prTxDesc);
		fgProtected = TRUE;
	}

	switch (HAL_MAC_TX_DESC_GET_HEADER_FORMAT(prTxDesc)) {
	case HEADER_FORMAT_802_11_ENHANCE_MODE:
		if (u4PktOption & MSDU_OPT_EOSP)
			HAL_MAC_TX_DESC_SET_EOSP(prTxDesc);

		if (u4PktOption & MSDU_OPT_AMSDU)
			HAL_MAC_TX_DESC_SET_AMSDU(prTxDesc);
		break;

	case HEADER_FORMAT_NON_802_11:
		if (u4PktOption & MSDU_OPT_EOSP)
			HAL_MAC_TX_DESC_SET_EOSP(prTxDesc);

		if (u4PktOption & MSDU_OPT_MORE_DATA)
			HAL_MAC_TX_DESC_SET_MORE_DATA(prTxDesc);

		if (u4PktOption & MSDU_OPT_REMOVE_VLAN)
			HAL_MAC_TX_DESC_SET_REMOVE_VLAN(prTxDesc);
		break;

	case HEADER_FORMAT_802_11_NORMAL_MODE:
		if (fgProtected && prMsduInfo->prPacket) {
			struct WLAN_MAC_HEADER *prWlanHeader =
				(struct WLAN_MAC_HEADER *) ((unsigned long) (
				prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

			prWlanHeader->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		}
		break;

	default:
		break;
	}

	if (!fgIsLongFormat)
		return;

	/* Fields in DW2~6 (Long Format) */
	if (u4PktOption & MSDU_OPT_NO_AGGREGATE)
		HAL_MAC_TX_DESC_SET_BA_DISABLE(prTxDesc);

	if (u4PktOption & MSDU_OPT_TIMING_MEASURE)
		HAL_MAC_TX_DESC_SET_TIMING_MEASUREMENT(prTxDesc);

	if (u4PktOption & MSDU_OPT_NDP)
		HAL_MAC_TX_DESC_SET_NDP(prTxDesc);

	if (u4PktOption & MSDU_OPT_NDPA)
		HAL_MAC_TX_DESC_SET_NDPA(prTxDesc);

	if (u4PktOption & MSDU_OPT_SOUNDING)
		HAL_MAC_TX_DESC_SET_SOUNDING_FRAME(prTxDesc);

	if (u4PktOption & MSDU_OPT_FORCE_RTS)
		HAL_MAC_TX_DESC_SET_FORCE_RTS_CTS(prTxDesc);

	if (u4PktOption & MSDU_OPT_BIP)
		HAL_MAC_TX_DESC_SET_BIP(prTxDesc);

	/* SW field */
	if (u4PktOption & MSDU_OPT_SW_DURATION)
		HAL_MAC_TX_DESC_SET_DURATION_CONTROL_BY_SW(prTxDesc);

	if (u4PktOption & MSDU_OPT_SW_PS_BIT)
		HAL_MAC_TX_DESC_SET_SW_PM_CONTROL(prTxDesc);

	if (u4PktOption & MSDU_OPT_SW_HTC)
		HAL_MAC_TX_DESC_SET_HTC_EXIST(prTxDesc);
#if 0
	if (u4PktOption & MSDU_OPT_SW_BAR_SN)
		HAL_MAC_TX_DESC_SET_SW_BAR_SSN(prTxDesc);
#endif
	if (u4PktOption & MSDU_OPT_MANUAL_SN) {
		HAL_MAC_TX_DESC_SET_TXD_SN_VALID(prTxDesc);
		HAL_MAC_TX_DESC_SET_SEQUENCE_NUMBER(prTxDesc,
						    prMsduInfo->u2SwSN);
	}

}

void nic_txd_v1_fill_by_pkt_ctrl(
	struct MSDU_INFO *prMsduInfo,
	void *prTxD)
{
	struct HW_MAC_TX_DESC *prTxDesc = (struct HW_MAC_TX_DESC *)prTxD;
	uint8_t ucPktControl = prMsduInfo->ucControlFlag;
	uint8_t ucSwReserved;

	/* Skip this function if no options is set */
	if (!ucPktControl)
		return;

	if (HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDesc)) {
		ucSwReserved = HAL_MAC_TX_DESC_GET_SW_RESERVED(prTxDesc);

		if (ucPktControl & MSDU_CONTROL_FLAG_FORCE_TX)
			ucSwReserved |= MSDU_CONTROL_FLAG_FORCE_TX;

		HAL_MAC_TX_DESC_SET_SW_RESERVED(prTxDesc, ucSwReserved);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief In this function, we'll compose the Tx descriptor of the MSDU.
 *
 * @param prAdapter              Pointer to the Adapter structure.
 * @param prMsduInfo             Pointer to the Msdu info
 * @param prTxDesc               Pointer to the Tx descriptor buffer
 *
 * @retval VOID
 */
/*----------------------------------------------------------------------------*/
void nic_txd_v1_compose(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	uint32_t u4TxDescLength,
	u_int8_t fgIsTemplate,
	uint8_t *prTxDescBuffer)
{
	struct HW_MAC_TX_DESC *prTxDesc;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucEtherTypeOffsetInWord;
	uint32_t u4TxDescAndPaddingLength;
	uint8_t ucTarPort, ucTarQueue;
#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_SPE_IDX_CONTROL == 1))
	enum ENUM_WF_PATH_FAVOR_T eWfPathFavor;
#endif
	prTxDesc = (struct HW_MAC_TX_DESC *) prTxDescBuffer;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prMsduInfo->ucBssIndex);
	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prMsduInfo->ucStaRecIndex);

	u4TxDescAndPaddingLength = u4TxDescLength +
				   NIC_TX_DESC_PADDING_LENGTH;

	kalMemZero(prTxDesc, u4TxDescAndPaddingLength);

	/* Move to nicTxFillDesc */
	/* Tx byte count */
	/* HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc,
	 * ucTxDescAndPaddingLength + prMsduInfo->u2FrameLength);
	 */

	/* Ether-type offset */
	if (prMsduInfo->fgIs802_11) {
		ucEtherTypeOffsetInWord =
			(NIC_TX_PSE_HEADER_LENGTH +
			prMsduInfo->ucMacHeaderLength +
			 prMsduInfo->ucLlcLength) >> 1;
	} else {
		ucEtherTypeOffsetInWord = ((ETHER_HEADER_LEN -
			ETHER_TYPE_LEN) + NIC_TX_PSE_HEADER_LENGTH) >> 1;
	}
	HAL_MAC_TX_DESC_SET_ETHER_TYPE_OFFSET(prTxDesc,
					      ucEtherTypeOffsetInWord);

	/* Port index / queue index */
	ucTarPort = nicTxGetTxDestPortIdxByTc(prMsduInfo->ucTC);
	HAL_MAC_TX_DESC_SET_PORT_INDEX(prTxDesc, ucTarPort);

	ucTarQueue = nicTxGetTxDestQIdxByTc(prMsduInfo->ucTC);
	if (ucTarPort == PORT_INDEX_LMAC)
		ucTarQueue += (prBssInfo->ucWmmQueSet * WMM_AC_INDEX_NUM);

	HAL_MAC_TX_DESC_SET_QUEUE_INDEX(prTxDesc, ucTarQueue);

	/* BMC packet */
	if (prMsduInfo->ucStaRecIndex == STA_REC_INDEX_BMCAST) {
		HAL_MAC_TX_DESC_SET_BMC(prTxDesc);

		/* Must set No ACK to mask retry bit in FC */
		HAL_MAC_TX_DESC_SET_NO_ACK(prTxDesc);
	}
	/* WLAN index */
	prMsduInfo->ucWlanIndex = nicTxGetWlanIdx(prAdapter,
		prMsduInfo->ucBssIndex, prMsduInfo->ucStaRecIndex);

#if 0				/* DBG */
	DBGLOG(RSN, INFO,
	       "Tx WlanIndex = %d eAuthMode = %d\n",
	       prMsduInfo->ucWlanIndex,
	       prAdapter->rWifiVar.rConnSettings.eAuthMode);
#endif
	HAL_MAC_TX_DESC_SET_WLAN_INDEX(prTxDesc,
				       prMsduInfo->ucWlanIndex);

	/* Header format */
	if (prMsduInfo->fgIs802_11) {
		HAL_MAC_TX_DESC_SET_HEADER_FORMAT(prTxDesc,
			HEADER_FORMAT_802_11_NORMAL_MODE);
		HAL_MAC_TX_DESC_SET_802_11_HEADER_LENGTH(prTxDesc,
				(prMsduInfo->ucMacHeaderLength >> 1));
	} else {
		HAL_MAC_TX_DESC_SET_HEADER_FORMAT(prTxDesc,
						  HEADER_FORMAT_NON_802_11);
		HAL_MAC_TX_DESC_SET_ETHERNET_II(prTxDesc);
	}

	/* Header Padding */
	HAL_MAC_TX_DESC_SET_HEADER_PADDING(prTxDesc,
					   NIC_TX_DESC_HEADER_PADDING_LENGTH);

	/* TID */
	HAL_MAC_TX_DESC_SET_TID(prTxDesc,
				prMsduInfo->ucUserPriority);

	/* Protection */
	if (secIsProtectedFrame(prAdapter, prMsduInfo, prStaRec)) {
		/* Update Packet option,
		 * PF bit will be set in nicTxFillDescByPktOption()
		 */
		if ((prStaRec && prStaRec->fgTransmitKeyExist)
		    || fgIsTemplate) {
			nicTxConfigPktOption(
				prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
				TRUE);

			if (prMsduInfo->fgIs802_1x &&
			    prMsduInfo->fgIs802_1x_NonProtected) {
				nicTxConfigPktOption(
					prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
					FALSE);
				DBGLOG(RSN, LOUD,
					"Pairwise EAPoL not protect!\n");
			}
		} else if (prMsduInfo->ucStaRecIndex ==
			   STA_REC_INDEX_BMCAST) {/* BMC packet */
			nicTxConfigPktOption(prMsduInfo,
				MSDU_OPT_PROTECTED_FRAME,
				TRUE);
			DBGLOG(RSN, LOUD, "Protect BMC frame!\n");
		}
	}
#if (UNIFIED_MAC_TX_FORMAT == 1)
	/* Packet Format */
	HAL_MAC_TX_DESC_SET_PKT_FORMAT(prTxDesc,
				       prMsduInfo->ucPacketFormat);
#endif

	/* Own MAC */
	HAL_MAC_TX_DESC_SET_OWN_MAC_INDEX(prTxDesc,
					  prBssInfo->ucOwnMacIndex);

	if (u4TxDescLength == NIC_TX_DESC_SHORT_FORMAT_LENGTH) {
		HAL_MAC_TX_DESC_SET_SHORT_FORMAT(prTxDesc);

		/* Update Packet option */
		nic_txd_v1_fill_by_pkt_option(prMsduInfo, prTxDesc);

		/* Short format, Skip DW 2~6 */
		return;
	}
	HAL_MAC_TX_DESC_SET_LONG_FORMAT(prTxDesc);

	/* Update Packet option */
	nic_txd_v1_fill_by_pkt_option(prMsduInfo, prTxDesc);

	nic_txd_v1_fill_by_pkt_ctrl(prMsduInfo, prTxDesc);

	/* Type */
	if (prMsduInfo->fgIs802_11) {
		struct WLAN_MAC_HEADER *prWlanHeader =
			(struct WLAN_MAC_HEADER *) ((unsigned long) (
			prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

		HAL_MAC_TX_DESC_SET_TYPE(prTxDesc,
			(prWlanHeader->u2FrameCtrl & MASK_FC_TYPE) >> 2);
		HAL_MAC_TX_DESC_SET_SUB_TYPE(prTxDesc,
			(prWlanHeader->u2FrameCtrl & MASK_FC_SUBTYPE) >>
			OFFSET_OF_FC_SUBTYPE);
	}
	/* PID */
	if (prMsduInfo->pfTxDoneHandler) {
		prMsduInfo->ucPID = nicTxAssignPID(prAdapter,
						   prMsduInfo->ucWlanIndex);
		HAL_MAC_TX_DESC_SET_PID(prTxDesc, prMsduInfo->ucPID);
		HAL_MAC_TX_DESC_SET_TXS_TO_MCU(prTxDesc);
	} else if (prAdapter->rWifiVar.ucDataTxDone == 2) {
		/* Log mode: only TxS to FW, no event to driver */
		HAL_MAC_TX_DESC_SET_PID(prTxDesc, NIC_TX_DESC_PID_RESERVED);
		HAL_MAC_TX_DESC_SET_TXS_TO_MCU(prTxDesc);
	}

	/* Remaining TX time */
	if (!(prMsduInfo->u4Option & MSDU_OPT_MANUAL_LIFE_TIME))
		prMsduInfo->u4RemainingLifetime =
			nicTxGetRemainingTxTimeByTc(prMsduInfo->ucTC);
	HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME_IN_MS(prTxDesc,
			prMsduInfo->u4RemainingLifetime);

	/* Tx count limit */
	if (!(prMsduInfo->u4Option & MSDU_OPT_MANUAL_RETRY_LIMIT)) {
		/* Note: BMC packet retry limit is set to unlimited */
		prMsduInfo->ucRetryLimit =
			nicTxGetTxCountLimitByTc(prMsduInfo->ucTC);
	}
	HAL_MAC_TX_DESC_SET_REMAINING_TX_COUNT(prTxDesc,
					       prMsduInfo->ucRetryLimit);

	/* Power Offset */
	HAL_MAC_TX_DESC_SET_POWER_OFFSET(prTxDesc,
					 prMsduInfo->cPowerOffset);

	/* Fix rate */
	switch (prMsduInfo->ucRateMode) {
	case MSDU_RATE_MODE_MANUAL_DESC:
		HAL_MAC_TX_DESC_SET_DW(prTxDesc, 6, 1,
				       &prMsduInfo->u4FixedRateOption);
#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_SPE_IDX_CONTROL == 1))
		/* Update spatial extension index setting */
		eWfPathFavor = wlanGetAntPathType(prAdapter, ENUM_WF_NON_FAVOR);
		HAL_MAC_TX_DESC_SET_SPE_IDX(prTxDesc,
			wlanGetSpeIdx(prAdapter, prBssInfo->ucBssIndex,
				eWfPathFavor));
#endif
		/* Set SPE_IDX_SEL to:
		* 0: reference SPE_IDX configuration in TXD
		* 1: reference SPE_IDX configuration in WTBL
		*/
		HAL_MAC_TX_DESC_SET_SPE_IDX_SEL(prTxDesc, 0);
		HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_DESC(prTxDesc);
		HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(prTxDesc);
		break;

	case MSDU_RATE_MODE_MANUAL_CR:
		HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_CR(prTxDesc);
		HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(prTxDesc);
		break;

	case MSDU_RATE_MODE_AUTO:
	default:
		break;
	}

}


void nic_txd_v1_compose_security_frame(
	struct ADAPTER *prAdapter,
	struct CMD_INFO *prCmdInfo,
	uint8_t *prTxDescBuffer,
	uint8_t *pucTxDescLength)
{
	struct HW_MAC_TX_DESC *prTxDesc = (struct HW_MAC_TX_DESC *)
					  prTxDescBuffer;
	uint8_t ucTxDescAndPaddingLength =
		NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH;

	struct BSS_INFO *prBssInfo;
	uint8_t ucTid = 0;
	uint8_t ucTempTC = TC4_INDEX;
	void *prNativePacket;
	uint8_t ucEtherTypeOffsetInWord;
	struct MSDU_INFO *prMsduInfo;

	prMsduInfo = prCmdInfo->prMsduInfo;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  prMsduInfo->ucBssIndex);
	prNativePacket = prMsduInfo->prPacket;

	ASSERT(prNativePacket);

	kalMemZero(prTxDesc, ucTxDescAndPaddingLength);

	/* WLAN index */
	prMsduInfo->ucWlanIndex = nicTxGetWlanIdx(prAdapter,
		prMsduInfo->ucBssIndex, prMsduInfo->ucStaRecIndex);

	/* UC to a connected peer */
	HAL_MAC_TX_DESC_SET_WLAN_INDEX(prTxDesc,
				       prMsduInfo->ucWlanIndex);
	/* Redirect Security frame to TID0 */
	/* ucTempTC = arNetwork2TcResource[prStaRec->ucBssIndex]
	 * [aucTid2ACI[ucTid]];
	 */

	/* Tx byte count */
	HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc,
		ucTxDescAndPaddingLength + prCmdInfo->u2InfoBufLen);

	/* Ether-type offset */
	ucEtherTypeOffsetInWord = ((ETHER_HEADER_LEN -
		ETHER_TYPE_LEN) + NIC_TX_PSE_HEADER_LENGTH) >> 1;
	HAL_MAC_TX_DESC_SET_ETHER_TYPE_OFFSET(prTxDesc,
					      ucEtherTypeOffsetInWord);

	/* Port index / queue index */
	HAL_MAC_TX_DESC_SET_PORT_INDEX(prTxDesc,
		nicTxGetTxDestPortIdxByTc(ucTempTC));
	HAL_MAC_TX_DESC_SET_QUEUE_INDEX(prTxDesc,
		nicTxGetTxDestQIdxByTc(ucTempTC));

	/* Header format */
	HAL_MAC_TX_DESC_SET_HEADER_FORMAT(prTxDesc,
					  HEADER_FORMAT_NON_802_11);

	/* Long Format */
	HAL_MAC_TX_DESC_SET_LONG_FORMAT(prTxDesc);

	/* Update Packet option */
	nicTxFillDescByPktOption(prAdapter, prMsduInfo, prTxDesc);

	if (!GLUE_TEST_PKT_FLAG(prNativePacket, ENUM_PKT_802_3)) {
		/* Set EthernetII */
		HAL_MAC_TX_DESC_SET_ETHERNET_II(prTxDesc);
	}
	/* Header Padding */
	HAL_MAC_TX_DESC_SET_HEADER_PADDING(prTxDesc,
					   NIC_TX_DESC_HEADER_PADDING_LENGTH);

	/* TID */
	HAL_MAC_TX_DESC_SET_TID(prTxDesc, ucTid);

	/* Remaining TX time */
	HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME_IN_MS(prTxDesc,
			nicTxGetRemainingTxTimeByTc(ucTempTC));

	/* Tx count limit */
	HAL_MAC_TX_DESC_SET_REMAINING_TX_COUNT(prTxDesc,
			nicTxGetTxCountLimitByTc(ucTempTC));

	/* Set lowest BSS basic rate */
	HAL_MAC_TX_DESC_SET_FR_RATE(prTxDesc,
				    prBssInfo->u2HwDefaultFixedRateCode);
	HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_DESC(prTxDesc);
	HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(prTxDesc);

	/* Packet Format */
	HAL_MAC_TX_DESC_SET_PKT_FORMAT(prTxDesc,
				       TXD_PKT_FORMAT_COMMAND);

	/* Own MAC */
	HAL_MAC_TX_DESC_SET_OWN_MAC_INDEX(prTxDesc,
					  prBssInfo->ucOwnMacIndex);

	/* PID */
	if (prMsduInfo->pfTxDoneHandler) {
		prMsduInfo->ucPID = nicTxAssignPID(prAdapter,
						   prMsduInfo->ucWlanIndex);
		HAL_MAC_TX_DESC_SET_PID(prTxDesc, prMsduInfo->ucPID);
		HAL_MAC_TX_DESC_SET_TXS_TO_MCU(prTxDesc);
	}

	if (pucTxDescLength)
		*pucTxDescLength = ucTxDescAndPaddingLength;
}

void nic_txd_v1_set_pkt_fixed_rate_option_full(
	struct MSDU_INFO *prMsduInfo,
	uint16_t u2RateCode,
	uint8_t ucBandwidth,
	u_int8_t fgShortGI,
	u_int8_t fgLDPC,
	u_int8_t fgDynamicBwRts,
	u_int8_t fgBeamforming,
	uint8_t ucAntennaIndex)
{
	struct HW_MAC_TX_DESC rTxDesc;
	struct HW_MAC_TX_DESC *prTxDesc = &rTxDesc;

	kalMemZero(prTxDesc, NIC_TX_DESC_LONG_FORMAT_LENGTH);

	/* Follow the format of Tx descriptor DW 6 */
	HAL_MAC_TX_DESC_SET_FR_RATE(prTxDesc, u2RateCode);

	if (ucBandwidth)
		HAL_MAC_TX_DESC_SET_FR_BW(prTxDesc, ucBandwidth);

	if (fgBeamforming)
		HAL_MAC_TX_DESC_SET_FR_BF(prTxDesc);

	if (fgShortGI)
		HAL_MAC_TX_DESC_SET_FR_SHORT_GI(prTxDesc);

	if (fgLDPC)
		HAL_MAC_TX_DESC_SET_FR_LDPC(prTxDesc);

	if (fgDynamicBwRts)
		HAL_MAC_TX_DESC_SET_FR_DYNAMIC_BW_RTS(prTxDesc);

	HAL_MAC_TX_DESC_SET_FR_ANTENNA_ID(prTxDesc, ucAntennaIndex);

	/* Write back to RateOption of MSDU_INFO */
	HAL_MAC_TX_DESC_GET_DW(prTxDesc, 6, 1,
			       &prMsduInfo->u4FixedRateOption);

	prMsduInfo->ucRateMode = MSDU_RATE_MODE_MANUAL_DESC;

}

void nic_txd_v1_set_pkt_fixed_rate_option(
	struct MSDU_INFO *prMsduInfo,
	uint16_t u2RateCode,
	uint8_t ucBandwidth,
	u_int8_t fgShortGI,
	u_int8_t fgDynamicBwRts)
{
	struct HW_MAC_TX_DESC rTxDesc;
	struct HW_MAC_TX_DESC *prTxDesc = &rTxDesc;

	kalMemZero(prTxDesc, NIC_TX_DESC_LONG_FORMAT_LENGTH);

	/* Follow the format of Tx descriptor DW 6 */
	HAL_MAC_TX_DESC_SET_FR_RATE(prTxDesc, u2RateCode);

	if (ucBandwidth)
		HAL_MAC_TX_DESC_SET_FR_BW(prTxDesc, ucBandwidth);

	if (fgShortGI)
		HAL_MAC_TX_DESC_SET_FR_SHORT_GI(prTxDesc);

	if (fgDynamicBwRts)
		HAL_MAC_TX_DESC_SET_FR_DYNAMIC_BW_RTS(prTxDesc);

	/* Write back to RateOption of MSDU_INFO */
	HAL_MAC_TX_DESC_GET_DW(prTxDesc, 6, 1,
			       &prMsduInfo->u4FixedRateOption);

	prMsduInfo->ucRateMode = MSDU_RATE_MODE_MANUAL_DESC;

}

void nic_txd_v1_set_hw_amsdu_template(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTid,
	u_int8_t fgSet)
{
	struct HW_MAC_TX_DESC *prTxDesc;

	DBGLOG(QM, INFO,
	       "Update HW Amsdu field of TXD template for STA[%u] Tid[%u]\n",
	       prStaRec->ucIndex, ucTid);

	if (prStaRec->aprTxDescTemplate[ucTid]) {
		prTxDesc = (struct HW_MAC_TX_DESC *)
			   prStaRec->aprTxDescTemplate[ucTid];
		if (fgSet)
			HAL_MAC_TX_DESC_SET_HW_AMSDU(prTxDesc);
		else
			HAL_MAC_TX_DESC_UNSET_HW_AMSDU(prTxDesc);
	}
}

void nic_txd_v1_change_data_port_by_ac(
	struct STA_RECORD *prStaRec,
	uint8_t ucAci,
	u_int8_t fgToMcu)
{
	uint8_t ucTid;
	void **pprTxDTemplate = NULL;

	if (!prStaRec)
		return;
	DBGLOG(TX, INFO, "Data Packets in Aci %d will route to %s\n", ucAci,
	       fgToMcu ? "MCU" : "LMAC");
	pprTxDTemplate = &prStaRec->aprTxDescTemplate[0];
	for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {
		if (aucTid2ACI[ucTid] != ucAci)
			continue;
		HAL_MAC_TX_DESC_SET_PORT_INDEX(
			(struct HW_MAC_TX_DESC *)pprTxDTemplate[ucTid],
			fgToMcu ? PORT_INDEX_MCU:PORT_INDEX_LMAC);
	}
}
