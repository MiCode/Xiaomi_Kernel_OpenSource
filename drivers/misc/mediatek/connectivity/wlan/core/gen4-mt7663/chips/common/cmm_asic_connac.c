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
/*! \file   connac.c
 *    \brief  Internal driver stack will export the required procedures
 *            here for GLUE Layer.
 *
 *    This file contains all routines which are exported from MediaTek 802.11
 *    Wireless LAN driver stack to GLUE Layer.
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
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

#if defined(_HIF_USB)
/*
 * USB Endpoint OUT/DMA Scheduler Group Mapping (HW Define)
 * EP#4 / Group0  (DATA)
 * EP#5 / Group1  (DATA)
 * EP#6 / Group2  (DATA)
 * EP#7 / Group3  (DATA)
 * EP#9 / Group4  (DATA)
 * EP#8 / Group15 (CMD)
 */
uint8_t arAcQIdx2GroupId[MAC_TXQ_NUM] = {
	GROUP0_INDEX,    /* MAC_TXQ_AC0_INDEX */
	GROUP1_INDEX,    /* MAC_TXQ_AC1_INDEX */
	GROUP2_INDEX,    /* MAC_TXQ_AC2_INDEX */
	GROUP3_INDEX,    /* MAC_TXQ_AC3_INDEX */

	GROUP4_INDEX,    /* MAC_TXQ_AC10_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC11_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC12_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC13_INDEX */

	GROUP4_INDEX,    /* MAC_TXQ_AC20_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC21_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC22_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC23_INDEX */

	GROUP4_INDEX,    /* MAC_TXQ_AC30_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC31_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC32_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_AC33_INDEX */

	GROUP4_INDEX,    /* MAC_TXQ_ALTX_0_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_BMC_0_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_BCN_0_INDEX */
	GROUP4_INDEX,    /* MAC_TXQ_PSMP_0_INDEX */

	GROUP5_INDEX,    /* Reserved */
	GROUP5_INDEX,    /* Reserved */
	GROUP5_INDEX,    /* Reserved */
	GROUP5_INDEX,    /* Reserved */

	GROUP5_INDEX,    /* Reserved */
	GROUP5_INDEX,    /* Reserved */
};
#endif /* _HIF_USB */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
#if defined(_HIF_USB)
#define USB_DMA_SHDL_GROUP_DEF_SEQUENCE_ORDER 0xFFFEFFFF
#define USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA 0x3
#define USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA 0x1FF
#define USB_ACCESS_RETRY_LIMIT           1
#endif /* _HIF_USB */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
void asicCapInit(IN struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prChipInfo = prAdapter->chip_info;

	prChipInfo->u2HifTxdSize = 0;
	prChipInfo->u2TxInitCmdPort = 0;
	prChipInfo->u2TxFwDlPort = 0;
	prChipInfo->fillHifTxDesc = NULL;
	prChipInfo->u4ExtraTxByteCount = 0;

	switch (prGlueInfo->u4InfType) {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	case MT_DEV_INF_PCIE:
	case MT_DEV_INF_AXI:
		prChipInfo->u2TxInitCmdPort = TX_RING_FWDL_IDX_3;
		prChipInfo->u2TxFwDlPort = TX_RING_FWDL_IDX_3;
		prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD;
		prChipInfo->u4HifDmaShdlBaseAddr = PCIE_HIF_DMASHDL_BASE;

		HAL_MCR_WR(prAdapter, CONN_HIF_ON_IRQ_ENA, BIT(0));
		break;
#endif /* _HIF_PCIE */
#if defined(_HIF_USB)
	case MT_DEV_INF_USB:
		prChipInfo->u2HifTxdSize = USB_HIF_TXD_LEN;
		prChipInfo->fillHifTxDesc = fillUsbHifTxDesc;
		prChipInfo->u2TxInitCmdPort = USB_DATA_BULK_OUT_EP8;
		prChipInfo->u2TxFwDlPort = USB_DATA_BULK_OUT_EP4;
		prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD_PAYLOAD;
		prChipInfo->u4ExtraTxByteCount =
			EXTRA_TXD_SIZE_FOR_TX_BYTE_COUNT;
		prChipInfo->u4HifDmaShdlBaseAddr = USB_HIF_DMASHDL_BASE;
		asicUsbDmaShdlInit(prAdapter);
		asicUdmaTxTimeoutEnable(prAdapter);
		asicUdmaRxFlush(prAdapter, FALSE);
		asicPdmaHifReset(prAdapter, TRUE);
		break;
#endif /* _HIF_USB */
#if defined(_HIF_SDIO)
	case MT_DEV_INF_SDIO:
		prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD_PAYLOAD;
		prChipInfo->u4ExtraTxByteCount =
			EXTRA_TXD_SIZE_FOR_TX_BYTE_COUNT;
		break;
#endif /* _HIF_SDIO */
	default:
		break;
	}
}

uint32_t asicGetFwDlInfo(struct ADAPTER *prAdapter,
			 char *pcBuf, int i4TotalLen)
{
	struct TAILER_COMMON_FORMAT_T *prComTailer;
	uint32_t u4Offset = 0;
	uint8_t aucBuf[32];

	prComTailer = &prAdapter->rVerInfo.rCommonTailer;

	kalMemZero(aucBuf, 32);
	kalMemCopy(aucBuf, prComTailer->aucRamVersion, 10);
	u4Offset += snprintf(pcBuf + u4Offset, i4TotalLen - u4Offset,
			     "Tailer Ver[%u:%u] %s (%s) info %u:E%u\n",
			     prComTailer->ucFormatVer,
			     prComTailer->ucFormatFlag,
			     aucBuf,
			     prComTailer->aucRamBuiltDate,
			     prComTailer->ucChipInfo,
			     prComTailer->ucEcoCode + 1);

	if (prComTailer->ucFormatFlag) {
		u4Offset += snprintf(pcBuf + u4Offset, i4TotalLen - u4Offset,
				     "Release manifest: %s\n",
				     prAdapter->rVerInfo.aucReleaseManifest);
	}
	return u4Offset;
}

uint32_t asicGetChipID(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4ChipID = 0;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;
	ASSERT(prChipInfo);

	/* Compose chipID from chip ip version
	 *
	 * BIT(30, 31) : Coding type, 00: compact, 01: index table
	 * BIT(24, 29) : IP config (6 bits)
	 * BIT(8, 23)  : IP version
	 * BIT(0, 7)   : A die info
	 */

	u4ChipID = (0x0 << 30) |
		   ((prChipInfo->u4ChipIpConfig & 0x3F) << 24) |
		   ((prChipInfo->u4ChipIpVersion & 0xF0000000) >>  8) |
		   ((prChipInfo->u4ChipIpVersion & 0x000F0000) >>  0) |
		   ((prChipInfo->u4ChipIpVersion & 0x00000F00) <<  4) |
		   ((prChipInfo->u4ChipIpVersion & 0x0000000F) <<  8) |
		   CONNAC_CHIP_ADIE_INFO;

	log_dbg(HAL, INFO, "ChipID = [0x%08x]\n", u4ChipID);
	return u4ChipID;
}

void asicEnableFWDownload(IN struct ADAPTER *prAdapter,
			  IN u_int8_t fgEnable)
{
	struct GLUE_INFO *prGlueInfo;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;

	switch (prGlueInfo->u4InfType) {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	case MT_DEV_INF_PCIE:
	case MT_DEV_INF_AXI:
	{
		union WPDMA_GLO_CFG_STRUCT GloCfg;

		kalDevRegRead(prGlueInfo, WPDMA_GLO_CFG, &GloCfg.word);

		GloCfg.field_conn.bypass_dmashdl_txring3 = fgEnable;

		kalDevRegWrite(prGlueInfo, WPDMA_GLO_CFG, GloCfg.word);
	}
	break;
#endif /* _HIF_PCIE */

#if defined(_HIF_USB)
	case MT_DEV_INF_USB:
	{
		uint32_t u4Value = 0;

		ASSERT(prAdapter);

		HAL_MCR_RD(prAdapter, CONNAC_UDMA_TX_QSEL, &u4Value);

		if (fgEnable)
			u4Value |= FW_DL_EN;
		else
			u4Value &= ~FW_DL_EN;

		HAL_MCR_WR(prAdapter, CONNAC_UDMA_TX_QSEL, u4Value);
	}
	break;
#endif /* _HIF_USB */

	default:
		break;
	}
}

void fillNicTxDescAppend(IN struct ADAPTER *prAdapter,
			 IN struct MSDU_INFO *prMsduInfo,
			 OUT uint8_t *prTxDescBuffer)
{
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;

	/* Fill TxD append */
	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *)
			   prTxDescBuffer;
	kalMemZero(prHwTxDescAppend, prChipInfo->txd_append_size);
}

void fillNicTxDescAppendWithCR4(IN struct ADAPTER
				*prAdapter, IN struct MSDU_INFO *prMsduInfo,
				OUT uint8_t *prTxDescBuffer)
{
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;

	/* Fill TxD append */
	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *)
			   prTxDescBuffer;
	kalMemZero(prHwTxDescAppend, prChipInfo->txd_append_size);
	prHwTxDescAppend->CR4_APPEND.u2PktFlags =
		HIT_PKT_FLAGS_CT_WITH_TXD;
	prHwTxDescAppend->CR4_APPEND.ucBssIndex =
		prMsduInfo->ucBssIndex;
}

void fillTxDescAppendByHost(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
	IN phys_addr_t rDmaAddr, IN uint32_t u4Idx,
	IN u_int8_t fgIsLast,
	OUT uint8_t *pucBuffer)
{
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;
	struct TXD_PTR_LEN *prPtrLen;

	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *) (
				   pucBuffer + NIC_TX_DESC_LONG_FORMAT_LENGTH);
	prHwTxDescAppend->CONNAC_APPEND.au2MsduId[u4Idx] =
		u4MsduId | TXD_MSDU_ID_VLD;
	prPtrLen = &prHwTxDescAppend->CONNAC_APPEND.arPtrLen[u4Idx >> 1];
	if ((u4Idx & 1) == 0) {
		prPtrLen->u4Ptr0 = rDmaAddr;
		prPtrLen->u2Len0 = prMsduInfo->u2FrameLength | TXD_LEN_ML;
		if (fgIsLast)
			prPtrLen->u2Len0 |= TXD_LEN_AL;
	} else {
		prPtrLen->u4Ptr1 = rDmaAddr;
		prPtrLen->u2Len1 = prMsduInfo->u2FrameLength | TXD_LEN_ML;
		if (fgIsLast)
			prPtrLen->u2Len1 |= TXD_LEN_AL;
	}
}

void fillTxDescAppendByHostV2(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
	IN phys_addr_t rDmaAddr, IN uint32_t u4Idx,
	IN u_int8_t fgIsLast,
	OUT uint8_t *pucBuffer)
{
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;
	struct TXD_PTR_LEN *prPtrLen;
	uint64_t u8Addr = (uint64_t)rDmaAddr;

	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *)
		(pucBuffer + NIC_TX_DESC_LONG_FORMAT_LENGTH);
	prHwTxDescAppend->CONNAC_APPEND.au2MsduId[u4Idx] =
		u4MsduId | TXD_MSDU_ID_VLD;
	prPtrLen = &prHwTxDescAppend->CONNAC_APPEND.arPtrLen[u4Idx >> 1];

	if ((u4Idx & 1) == 0) {
		prPtrLen->u4Ptr0 = (uint32_t)u8Addr;
		prPtrLen->u2Len0 =
			(prMsduInfo->u2FrameLength & TXD_LEN_MASK_V2) |
			((u8Addr >> TXD_ADDR2_OFFSET) & TXD_ADDR2_MASK);
		prPtrLen->u2Len0 |= TXD_LEN_ML_V2;
	} else {
		prPtrLen->u4Ptr1 = (uint32_t)u8Addr;
		prPtrLen->u2Len1 =
			(prMsduInfo->u2FrameLength & TXD_LEN_MASK_V2) |
			((u8Addr >> TXD_ADDR2_OFFSET) & TXD_ADDR2_MASK);
		prPtrLen->u2Len1 |= TXD_LEN_ML_V2;
	}
}

void fillTxDescAppendByCR4(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
	IN phys_addr_t rDmaAddr, IN uint32_t u4Idx,
	IN u_int8_t fgIsLast,
	OUT uint8_t *pucBuffer)
{
	union HW_MAC_TX_DESC_APPEND *prHwTxDescAppend;

	prHwTxDescAppend = (union HW_MAC_TX_DESC_APPEND *)
		(pucBuffer + NIC_TX_DESC_LONG_FORMAT_LENGTH);
	prHwTxDescAppend->CR4_APPEND.u2MsduToken = u4MsduId;
	prHwTxDescAppend->CR4_APPEND.ucBufNum = u4Idx + 1;
	prHwTxDescAppend->CR4_APPEND.au4BufPtr[u4Idx] = rDmaAddr;
	prHwTxDescAppend->CR4_APPEND.au2BufLen[u4Idx] =
		prMsduInfo->u2FrameLength;
}

void fillTxDescTxByteCount(IN struct ADAPTER *prAdapter,
			   IN struct MSDU_INFO *prMsduInfo,
			   struct HW_MAC_TX_DESC *prTxDesc)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4TxByteCount = NIC_TX_DESC_LONG_FORMAT_LENGTH;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(prTxDesc);

	prChipInfo = prAdapter->chip_info;
	u4TxByteCount += prMsduInfo->u2FrameLength;

	if (prMsduInfo->ucPacketType == TX_PACKET_TYPE_DATA)
		u4TxByteCount += prChipInfo->u4ExtraTxByteCount;

	/* Calculate Tx byte count */
	HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc, u4TxByteCount);
}

void fillTxDescTxByteCountWithCR4(IN struct ADAPTER
				  *prAdapter, IN struct MSDU_INFO *prMsduInfo,
				  struct HW_MAC_TX_DESC *prTxDesc)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4TxByteCount = NIC_TX_DESC_LONG_FORMAT_LENGTH;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(prTxDesc);

	prChipInfo = prAdapter->chip_info;
	u4TxByteCount += prMsduInfo->u2FrameLength;

	if (prMsduInfo->ucPacketType == TX_PACKET_TYPE_DATA)
		u4TxByteCount += prChipInfo->txd_append_size;

	/* Calculate Tx byte count */
	HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc, u4TxByteCount);
}

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
void asicPcieDmaShdlInit(IN struct ADAPTER *prAdapter)
{
	uint32_t u4BaseAddr, u4MacVal;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;
	u4BaseAddr = prChipInfo->u4HifDmaShdlBaseAddr;

	HAL_MCR_RD(prAdapter,
		   CONN_HIF_DMASHDL_PACKET_MAX_SIZE(u4BaseAddr), &u4MacVal);
	u4MacVal &= ~(PLE_PKT_MAX_SIZE_MASK | PSE_PKT_MAX_SIZE_MASK);
	u4MacVal |= PLE_PKT_MAX_SIZE_NUM(0x1);
	u4MacVal |= PSE_PKT_MAX_SIZE_NUM(0x18); /* 0x18 * 128 = 3K */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_PACKET_MAX_SIZE(u4BaseAddr), u4MacVal);

	u4MacVal =
	(CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP1_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP2_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP3_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP4_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP5_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP6_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP7_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP8_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP9_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP10_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP11_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP12_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP13_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP14_REFILL_DISABLE_MASK |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP15_REFILL_DISABLE_MASK);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_REFILL_CONTROL(u4BaseAddr), u4MacVal);

	u4MacVal = DMASHDL_MIN_QUOTA_NUM(0x3);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(0xfff);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP0_CTRL(u4BaseAddr), u4MacVal);

	u4MacVal = 0;
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP1_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP2_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP3_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP4_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP5_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP6_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP7_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP8_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP9_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP10_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP11_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP12_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP13_CTRL(u4BaseAddr), u4MacVal);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP14_CTRL(u4BaseAddr), u4MacVal);
}

void asicPdmaLoopBackConfig(struct GLUE_INFO *prGlueInfo, u_int8_t fgEnable)
{
	union WPDMA_GLO_CFG_STRUCT GloCfg;
	uint32_t word = 1;

	kalDevRegRead(prGlueInfo, WPDMA_GLO_CFG, &GloCfg.word);

	GloCfg.field_conn.bypass_dmashdl_txring3 = 1;
	GloCfg.field_conn.pdma_addr_ext_en = 0;
	GloCfg.field_conn.omit_rx_info = 1;
	GloCfg.field_conn.omit_tx_info = 1;
	GloCfg.field_conn.multi_dma_en = 0;
	GloCfg.field_conn.pdma_addr_ext_en = 0;
	GloCfg.field_conn.tx_dma_en = 1;
	GloCfg.field_conn.rx_dma_en = 1;
	GloCfg.field_conn.multi_dma_en = 0;

	kalDevRegWrite(prGlueInfo, WPDMA_FIFO_TEST_MOD, word);
	kalDevRegWrite(prGlueInfo, WPDMA_GLO_CFG, GloCfg.word);
}
#if CFG_SUPPORT_PCIE_L2
void asicPdmaStop(
	struct GLUE_INFO *prGlueInfo,
	u_int8_t enable)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	union WPDMA_GLO_CFG_STRUCT GloCfg;

	HAL_MCR_RD(prAdapter, WPDMA_GLO_CFG, &GloCfg.word);

	if (enable == TRUE) {
		GloCfg.field_conn.tx_dma_en = 0;
		GloCfg.field_conn.rx_dma_en = 0;
	} else {
		GloCfg.field_conn.tx_dma_en = 1;
		GloCfg.field_conn.rx_dma_en = 1;
	}

	HAL_MCR_WR(prAdapter, WPDMA_GLO_CFG, GloCfg.word);

}
#endif
void asicPdmaConfig(struct GLUE_INFO *prGlueInfo, u_int8_t fgEnable)
{
	struct BUS_INFO *prBusInfo =
			prGlueInfo->prAdapter->chip_info->bus_info;
	union WPDMA_GLO_CFG_STRUCT GloCfg;
	union WPDMA_INT_MASK IntMask;

	kalDevRegRead(prGlueInfo, WPDMA_GLO_CFG, &GloCfg.word);
	kalDevRegRead(prGlueInfo, WPDMA_INT_MSK, &IntMask.word);

	if (fgEnable == TRUE) {
		GloCfg.field_conn.tx_dma_en = 1;
		GloCfg.field_conn.rx_dma_en = 1;
		GloCfg.field_conn.pdma_bt_size = 3;
		GloCfg.field_conn.pdma_addr_ext_en =
			(prBusInfo->u4DmaMask > 32) ? 1 : 0;
		GloCfg.field_conn.tx_wb_ddone = 1;
		GloCfg.field_conn.multi_dma_en = 2;
		GloCfg.field_conn.fifo_little_endian = 1;
		GloCfg.field_conn.clk_gate_dis = 1;

		IntMask.field.rx_done_0 = 1;
		IntMask.field.rx_done_1 = 1;
		IntMask.field.tx_done =
			BIT(prBusInfo->tx_ring_fwdl_idx) |
			BIT(prBusInfo->tx_ring_cmd_idx) |
			BIT(prBusInfo->tx_ring_data_idx);
		IntMask.field_conn.tx_coherent = 0;
		IntMask.field_conn.rx_coherent = 0;
		IntMask.field_conn.tx_dly_int = 0;
		IntMask.field_conn.rx_dly_int = 0;
		IntMask.field_conn.mcu2host_sw_int_ena = 1;
	} else {
		GloCfg.field_conn.tx_dma_en = 0;
		GloCfg.field_conn.rx_dma_en = 0;

		IntMask.field_conn.rx_done_0 = 0;
		IntMask.field_conn.rx_done_1 = 0;
		IntMask.field_conn.tx_done = 0;
		IntMask.field_conn.tx_coherent = 0;
		IntMask.field_conn.rx_coherent = 0;
		IntMask.field_conn.tx_dly_int = 0;
		IntMask.field_conn.rx_dly_int = 0;
		IntMask.field_conn.mcu2host_sw_int_ena = 0;
	}

	kalDevRegWrite(prGlueInfo, WPDMA_INT_MSK, IntMask.word);
	kalDevRegWrite(prGlueInfo, WPDMA_GLO_CFG, GloCfg.word);
	kalDevRegWrite(prGlueInfo, MCU2HOST_SW_INT_ENA,
		       ERROR_DETECT_MASK);
}

void asicEnableInterrupt(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(prAdapter);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	enable_irq(prHifInfo->u4IrqId);
}

void asicDisableInterrupt(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(prAdapter);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	disable_irq_nosync(prHifInfo->u4IrqId);
}

void asicLowPowerOwnRead(IN struct ADAPTER *prAdapter,
			 OUT u_int8_t *pfgResult)
{
	uint32_t u4RegValue;

	HAL_MCR_RD(prAdapter, CONN_HIF_ON_LPCTL, &u4RegValue);
	*pfgResult = (u4RegValue & PCIE_LPCR_HOST_SET_OWN) == 0 ?
		     TRUE : FALSE;
}

void asicLowPowerOwnSet(IN struct ADAPTER *prAdapter,
			OUT u_int8_t *pfgResult)
{
	uint32_t u4RegValue;

	HAL_MCR_WR(prAdapter, CONN_HIF_ON_LPCTL,
		   PCIE_LPCR_HOST_SET_OWN);
	HAL_MCR_RD(prAdapter, CONN_HIF_ON_LPCTL, &u4RegValue);
	*pfgResult = (u4RegValue & PCIE_LPCR_HOST_SET_OWN) == 1;
}

void asicLowPowerOwnClear(IN struct ADAPTER *prAdapter,
			  OUT u_int8_t *pfgResult)
{
	uint32_t u4RegValue;

	HAL_MCR_WR(prAdapter, CONN_HIF_ON_LPCTL,
		   PCIE_LPCR_HOST_CLR_OWN);
	HAL_MCR_RD(prAdapter, CONN_HIF_ON_LPCTL, &u4RegValue);
	*pfgResult = (u4RegValue & PCIE_LPCR_HOST_SET_OWN) == 0;
}

#if defined(_HIF_PCIE)
void asicLowPowerOwnClearPCIe(IN struct ADAPTER *prAdapter,
			  OUT u_int8_t *pfgResult)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_HIF_INFO *prHif = NULL;

	prGlueInfo = prAdapter->prGlueInfo;
	prHif = &prGlueInfo->rHifInfo;

	pci_write_config_byte(prHif->pdev,
				PCIE_DOORBELL_PUSH,
				CR_PCIE_CFG_CLEAR_OWN);
}
#endif

void asicWakeUpWiFi(IN struct ADAPTER *prAdapter)
{
	u_int8_t fgResult;

	ASSERT(prAdapter);

	HAL_LP_OWN_RD(prAdapter, &fgResult);

	if (fgResult)
		prAdapter->fgIsFwOwn = FALSE;
	else
		HAL_LP_OWN_CLR(prAdapter, &fgResult);
}

bool asicIsValidRegAccess(IN struct ADAPTER *prAdapter, IN uint32_t u4Register)
{
	uint32_t au4ExcludeRegs[] = { CONN_HIF_ON_LPCTL };
	uint32_t u4Idx, u4Size = sizeof(au4ExcludeRegs) / sizeof(uint32_t);

	if (wlanIsChipNoAck(prAdapter))
		return false;

	/* driver can access all consys registers on driver own */
	if (!prAdapter->fgIsFwOwn)
		return true;

	/* only own control register can be accessed on fw own */
	for (u4Idx = 0; u4Idx < u4Size; u4Idx++) {
		if (u4Register == au4ExcludeRegs[u4Idx])
			return true;
	}

	return false;
}

void asicGetMailboxStatus(IN struct ADAPTER *prAdapter,
			  OUT uint32_t *pu4Val)
{
	uint32_t u4RegValue;

	HAL_MCR_RD(prAdapter,
		   CONN_MCU_CONFG_ON_HOST_MAILBOX_WF_ADDR, &u4RegValue);
	*pu4Val = u4RegValue;
}

void asicSetDummyReg(struct GLUE_INFO *prGlueInfo)
{
	kalDevRegWrite(prGlueInfo, CONN_DUMMY_CR, PDMA_DUMMY_MAGIC_NUM);
}

void asicCheckDummyReg(struct GLUE_INFO *prGlueInfo)
{
	struct GL_HIF_INFO *prHifInfo;
	uint32_t u4Value = 0;
	uint32_t u4Idx;

	prHifInfo = &prGlueInfo->rHifInfo;
	kalDevRegRead(prGlueInfo, CONN_DUMMY_CR, &u4Value);
	DBGLOG(HAL, TRACE, "Check sleep mode DummyReg[0x%x]\n", u4Value);
	if (u4Value != PDMA_DUMMY_RESET_VALUE)
		return;

	for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++)
		prHifInfo->TxRing[u4Idx].TxSwUsedIdx = 0;
	DBGLOG(HAL, INFO, "Weakup from sleep mode\n");

	/* Write sleep mode magic num to dummy reg */
	asicSetDummyReg(prGlueInfo);
}
#endif /* _HIF_PCIE || _HIF_AXI */

#if defined(_HIF_USB)
/* DMS Scheduler Init */
void asicUsbDmaShdlGroupInit(IN struct ADAPTER *prAdapter,
			     uint32_t u4RefillGroup)
{
	uint32_t u4BaseAddr, u4MacVal = 0;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4CfgVal = 0;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;
	u4BaseAddr = prChipInfo->u4HifDmaShdlBaseAddr;

	HAL_MCR_RD(prAdapter,
		   CONN_HIF_DMASHDL_PACKET_MAX_SIZE(u4BaseAddr), &u4MacVal);
	u4MacVal &= ~(PLE_PKT_MAX_SIZE_MASK | PSE_PKT_MAX_SIZE_MASK);
	u4MacVal |= PLE_PKT_MAX_SIZE_NUM(0x1);
	/* page size = 128 bytes */
	u4MacVal |= PSE_PKT_MAX_SIZE_NUM(DIV_ROUND_UP(USB_TX_CMD_BUF_SIZE,
						      128));
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_PACKET_MAX_SIZE(u4BaseAddr), u4MacVal);

	u4RefillGroup |=
	(CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP2_REFILL_PRIORITY_MASK
	|
	CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP3_REFILL_PRIORITY_MASK);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_REFILL_CONTROL(u4BaseAddr), u4RefillGroup);

	/* Use "User program group sequence order" by default.[16]1'b0	*/
	HAL_MCR_RD(prAdapter,
		   CONN_HIF_DMASHDL_PAGE_SETTING(u4BaseAddr), &u4MacVal);
	u4MacVal &= USB_DMA_SHDL_GROUP_DEF_SEQUENCE_ORDER;
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_PAGE_SETTING(u4BaseAddr), u4MacVal);

#if CFG_SUPPORT_CFG_FILE
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup1MinQuota",
				    USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(u4CfgVal);
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup1MaxQuota",
				    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(u4CfgVal);
#else /* CFG_SUPPORT_CFG_FILE */
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(
			   USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(
			    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
#endif /* !CFG_SUPPORT_CFG_FILE */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP1_CTRL(u4BaseAddr), u4MacVal);

#if CFG_SUPPORT_CFG_FILE
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup0MinQuota",
				    USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(u4CfgVal);
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup0MaxQuota",
				    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(u4CfgVal);
#else /* CFG_SUPPORT_CFG_FILE */
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(
			   USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(
			    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
#endif /* !CFG_SUPPORT_CFG_FILE */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP0_CTRL(u4BaseAddr), u4MacVal);

#if CFG_SUPPORT_CFG_FILE
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup2MinQuota",
				    USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(u4CfgVal);
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup2MaxQuota",
				    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(u4CfgVal);
#else /* CFG_SUPPORT_CFG_FILE */
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(
			   USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(
			    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
#endif /* !CFG_SUPPORT_CFG_FILE */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP2_CTRL(u4BaseAddr), u4MacVal);

#if CFG_SUPPORT_CFG_FILE
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup3MinQuota",
				    USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(u4CfgVal);
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup3MaxQuota",
				    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(u4CfgVal);
#else /* CFG_SUPPORT_CFG_FILE */
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(
			   USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(
			    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
#endif /* !CFG_SUPPORT_CFG_FILE */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP3_CTRL(u4BaseAddr), u4MacVal);

#if CFG_SUPPORT_CFG_FILE
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup4MinQuota",
				    USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(u4CfgVal);
	u4CfgVal = wlanCfgGetUint32(prAdapter,
				    "DmaShdlGroup4MaxQuota",
				    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(u4CfgVal);
#else /* CFG_SUPPORT_CFG_FILE */
	u4MacVal = DMASHDL_MIN_QUOTA_NUM(
			   USB_DMA_SHDL_GROUP_DEF_MIN_QUOTA);
	u4MacVal |= DMASHDL_MAX_QUOTA_NUM(
			    USB_DMA_SHDL_GROUP_DEF_MAX_QUOTA);
#endif /* !CFG_SUPPORT_CFG_FILE */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_GROUP4_CTRL(u4BaseAddr), u4MacVal);

	u4MacVal = ((arAcQIdx2GroupId[MAC_TXQ_AC0_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE0_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC1_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE1_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC2_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE2_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC3_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE3_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC10_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE4_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC11_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE5_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC12_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE6_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC13_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE7_MAPPING));
	HAL_MCR_WR(prAdapter, CONN_HIF_DMASHDL_Q_MAP0(u4BaseAddr),
		   u4MacVal);

	u4MacVal = ((arAcQIdx2GroupId[MAC_TXQ_AC20_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE8_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC21_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE9_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC22_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE10_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC23_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE11_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC30_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE12_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC31_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE13_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC32_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE14_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_AC33_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE15_MAPPING));
	HAL_MCR_WR(prAdapter, CONN_HIF_DMASHDL_Q_MAP1(u4BaseAddr),
		   u4MacVal);

	u4MacVal = ((arAcQIdx2GroupId[MAC_TXQ_ALTX_0_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE16_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_BMC_0_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE17_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_BCN_0_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE18_MAPPING) |
		    (arAcQIdx2GroupId[MAC_TXQ_PSMP_0_INDEX] <<
		     CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE19_MAPPING));
	HAL_MCR_WR(prAdapter, CONN_HIF_DMASHDL_Q_MAP2(u4BaseAddr),
		   u4MacVal);
}

void asicUsbDmaShdlInit(IN struct ADAPTER *prAdapter)
{
	uint32_t u4BaseAddr, u4MacVal;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;
	u4BaseAddr = prChipInfo->u4HifDmaShdlBaseAddr;

	/*
	 * Enable refill control group 0, 1, 2, 3, 4.
	 * Keep all group low refill priority to prevent low
	 * group starvation if we have high group.
	 */
	u4MacVal =
	(CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP5_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP6_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP7_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP8_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP9_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP10_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP11_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP12_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP13_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP14_REFILL_DISABLE_MASK
	 |
	 CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP15_REFILL_DISABLE_MASK);

	asicUsbDmaShdlGroupInit(prAdapter, u4MacVal);

	/*
	 * HIF Scheduler Setting
	 * Group15(CMD) is highest priority.
	 */
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_SHDL_SET0(u4BaseAddr), 0x6501234f);
	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_SHDL_SET1(u4BaseAddr), 0xedcba987);

	HAL_MCR_WR(prAdapter,
		   CONN_HIF_DMASHDL_OPTIONAL_CONTROL(u4BaseAddr), 0x7004801c);
}

u_int8_t asicUsbSuspend(IN struct ADAPTER *prAdapter,
			IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4Value;
	int32_t ret = 0;
	struct BUS_INFO *prBusInfo;

	DBGLOG(HAL, INFO, "%s ---->\n", __func__);
	prBusInfo = prAdapter->chip_info->bus_info;


	prGlueInfo->rHifInfo.state = USB_STATE_SUSPEND;
	halDisableInterrupt(prGlueInfo->prAdapter);
	halTxCancelAllSending(prGlueInfo->prAdapter);

	ret = usb_control_msg(prGlueInfo->rHifInfo.udev,
			      usb_sndctrlpipe(prGlueInfo->rHifInfo.udev, 0),
			      VND_REQ_FEATURE_SET,
			      DEVICE_VENDOR_REQUEST_OUT,
			      FEATURE_SET_WVALUE_SUSPEND, 0,
			      NULL, 0,
			      VENDOR_TIMEOUT_MS);
	if (ret) {
		DBGLOG(HAL, ERROR,
		       "%s:: VendorRequest FeatureSetResume ERROR: %x, enable PDMA TX again.\n",
		       __func__, (unsigned int)ret);
		/* Enable PDMA TX again */
		HAL_MCR_RD(prAdapter, PDMA_IF_MISC, &u4Value);
		u4Value |= PDMA_IF_MISC_TX_ENABLE_MASK;
		HAL_MCR_WR(prAdapter, PDMA_IF_MISC, u4Value);
		DBGLOG(HAL, INFO, "%s <----\n", __func__);
		return FALSE;
	}
	DBGLOG(HAL, INFO, "%s <----\n", __func__);
	return TRUE;
}

uint8_t asicUsbEventEpDetected(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t ret = 0;
	uint8_t ucRetryCount = 0;
	u_int8_t ucEp5Disable = FALSE;

	ASSERT(FALSE == 0);
	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;

	if (prHifInfo->fgEventEpDetected == FALSE) {
		prHifInfo->fgEventEpDetected = TRUE;
		do {
			ret = mtk_usb_vendor_request(prGlueInfo, 0,
						     DEVICE_VENDOR_REQUEST_IN,
						     VND_REQ_EP5_IN_INFO,
						     0, 0, &ucEp5Disable,
						     sizeof(ucEp5Disable));
			if (ret || ucRetryCount)
				DBGLOG(HAL, ERROR,
				       "usb_control_msg() status: %x retry: %u\n",
				       (unsigned int)ret, ucRetryCount);
			ucRetryCount++;
			if (ucRetryCount > USB_ACCESS_RETRY_LIMIT)
				break;
		} while (ret);

		if (ret) {
			kalSendAeeWarning(HIF_USB_ERR_TITLE_STR,
				HIF_USB_ERR_DESC_STR
				"USB() reports error: %x retry: %u",
				ret, ucRetryCount);
			DBGLOG(HAL, ERROR,
			  "usb_readl() reports error: %x retry: %u\n", ret,
			  ucRetryCount);
		} else {
			DBGLOG(HAL, INFO,
				"%s: Get ucEp5Disable = %d\n", __func__,
			  ucEp5Disable);
			if (ucEp5Disable)
				prHifInfo->eEventEpType = EVENT_EP_TYPE_DATA_EP;
		}
	}
	if (prHifInfo->eEventEpType == EVENT_EP_TYPE_DATA_EP)
		return USB_DATA_EP_IN;
	else
		return USB_EVENT_EP_IN;
}

void asicUdmaTxTimeoutEnable(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo;
	uint32_t u4Value;

	prBusInfo = prAdapter->chip_info->bus_info;
	HAL_MCR_RD(prAdapter, prBusInfo->u4UdmaWlCfg_1_Addr,
		   &u4Value);
	u4Value &= ~UDMA_WLCFG_1_TX_TIMEOUT_LIMIT_MASK;
	u4Value |= UDMA_WLCFG_1_TX_TIMEOUT_LIMIT(
			   prBusInfo->u4UdmaTxTimeout);
	HAL_MCR_WR(prAdapter, prBusInfo->u4UdmaWlCfg_1_Addr,
		   u4Value);

	HAL_MCR_RD(prAdapter, prBusInfo->u4UdmaWlCfg_0_Addr,
		   &u4Value);
	u4Value |= UDMA_WLCFG_0_TX_TIMEOUT_EN_MASK;
	HAL_MCR_WR(prAdapter, prBusInfo->u4UdmaWlCfg_0_Addr,
		   u4Value);
}

void asicUdmaRxFlush(IN struct ADAPTER *prAdapter,
		     IN u_int8_t bEnable)
{
	struct BUS_INFO *prBusInfo;
	uint32_t u4Value;

	prBusInfo = prAdapter->chip_info->bus_info;

	HAL_MCR_RD(prAdapter, prBusInfo->u4UdmaWlCfg_0_Addr,
		   &u4Value);
	if (bEnable)
		u4Value |= UDMA_WLCFG_0_RX_FLUSH_MASK;
	else
		u4Value &= ~UDMA_WLCFG_0_RX_FLUSH_MASK;
	HAL_MCR_WR(prAdapter, prBusInfo->u4UdmaWlCfg_0_Addr,
		   u4Value);
}

void asicPdmaHifReset(IN struct ADAPTER *prAdapter,
		      IN u_int8_t bRelease)
{
	uint32_t u4Value;

	HAL_MCR_RD(prAdapter, PDMA_HIF_RESET, &u4Value);
	if (bRelease)
		u4Value |= DPMA_HIF_LOGIC_RESET_MASK;
	else
		u4Value &= ~DPMA_HIF_LOGIC_RESET_MASK;
	HAL_MCR_WR(prAdapter, PDMA_HIF_RESET, u4Value);
}

void fillUsbHifTxDesc(IN uint8_t **pDest,
		      IN uint16_t *pInfoBufLen)
{
	/*USB TX Descriptor (4 bytes)*/
	/* BIT[15:0] - TX Bytes Count
	 * (Not including USB TX Descriptor and 4-bytes zero padding.
	 */
	kalMemZero((void *)*pDest, sizeof(uint32_t));
	kalMemCopy((void *)*pDest, (void *) pInfoBufLen,
		   sizeof(uint16_t));
}
#endif /* _HIF_USB */
