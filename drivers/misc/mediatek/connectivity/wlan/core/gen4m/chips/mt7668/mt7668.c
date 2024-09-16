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
/*! \file   mt7668.c
 *  \brief  Internal driver stack will export the required procedures here
 *          for GLUE Layer.
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

#include "mt7668.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

struct ECO_INFO mt7668_eco_table[] = {
	/* HW version,  ROM version,    Factory version, Eco version */
	{0x00, 0x00, 0xA, 0x1},	/* E1 */
	{0x10, 0x01, 0xB, 0x2},	/* E2 */
	{0x11, 0x01, 0xB, 0x2},	/* E2 */
	{0x00, 0x00, 0x0, 0x0}	/* End of table */
};

#if defined(_HIF_PCIE)
struct PCIE_CHIP_CR_MAPPING mt7668_bus2chip_cr_mapping[] = {
	/* chip addr, bus addr, range */
	{0x82060000, 0x00008000, 0x00000450}, /* WF_PLE */
	{0x82068000, 0x0000c000, 0x00000450}, /* WF_PSE */
	{0x8206c000, 0x0000e000, 0x00000300}, /* PP */
	{0x820d0000, 0x00020000, 0x00000200}, /* WF_AON */
	{0x820f0000, 0x00020200, 0x00000400}, /* WF_CFG */
	{0x820f0800, 0x00020600, 0x00000200}, /* WF_CFGOFF */
	{0x820f1000, 0x00020800, 0x00000200}, /* WF_TRB */
	{0x820f2000, 0x00020a00, 0x00000200}, /* WF_AGG */
	{0x820f3000, 0x00020c00, 0x00000400}, /* WF_ARB */
	{0x820f4000, 0x00021000, 0x00000200}, /* WF_TMAC */
	{0x820f5000, 0x00021200, 0x00000400}, /* WF_RMAC */
	{0x820f6000, 0x00021600, 0x00000200}, /* WF_SEC */
	{0x820f7000, 0x00021800, 0x00000200}, /* WF_DMA */

	{0x820f8000, 0x00022000, 0x00001000}, /* WF_PF */
	{0x820f9000, 0x00023000, 0x00000400}, /* WF_WTBLON */
	{0x820f9800, 0x00023400, 0x00000200}, /* WF_WTBLOFF */

	{0x820fa000, 0x00024000, 0x00000200}, /* WF_ETBF */
	{0x820fb000, 0x00024200, 0x00000400}, /* WF_LPON */
	{0x820fc000, 0x00024600, 0x00000200}, /* WF_INT */
	{0x820fd000, 0x00024800, 0x00000400}, /* WF_MIB */

	{0x820fe000, 0x00025000, 0x00002000}, /* WF_MU */

	{0x820e0000, 0x00030000, 0x00010000}, /* WF_WTBL */

	{0x80020000, 0x00000000, 0x00002000}, /* TOP_CFG */
	{0x80000000, 0x00002000, 0x00002000}, /* MCU_CFG */
	{0x50000000, 0x00004000, 0x00004000}, /* PDMA_CFG */
	{0xA0000000, 0x00008000, 0x00008000}, /* PSE_CFG */
	{0x82070000, 0x00010000, 0x00010000}, /* WF_PHY */

	{0x0, 0x0, 0x0}
};
#endif /* _HIF_PCIE */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

void
mt7668ConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx)
{
	struct mt66xx_chip_info *prChipInfo = prGlueInfo->prAdapter->chip_info;
	uint32_t chip_id = prChipInfo->chip_id;
	uint8_t sub_idx = 0;

	for (sub_idx = 0; apucNameTable[sub_idx]; sub_idx++) {
		if (((*pucNameIdx) + 3) < ucMaxNameIdx) {
			/* Type 1. WIFI_RAM_CODE_MTxxxx.bin */
			snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s%x.bin",
				apucNameTable[sub_idx], chip_id);
			(*pucNameIdx) += 1;

			/* Type 2. WIFI_RAM_CODE_MTxxxx */
			snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s%x",
				apucNameTable[sub_idx], chip_id);
			(*pucNameIdx) += 1;

			/* Type 3. WIFI_RAM_CODE_MTxxxx_Ex.bin */
			snprintf(*(apucName + (*pucNameIdx)),
					CFG_FW_NAME_MAX_LEN, "%s%x_E%u.bin",
					apucNameTable[sub_idx], chip_id,
					wlanGetEcoVersion(
					prGlueInfo->prAdapter));
			(*pucNameIdx) += 1;

			/* Type 4. WIFI_RAM_CODE_MTxxxx_Ex */
			snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s%x_E%u",
					apucNameTable[sub_idx], chip_id,
					wlanGetEcoVersion(
					prGlueInfo->prAdapter));
			(*pucNameIdx) += 1;
		} else {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
				"kalFirmwareImageMapping >> file name array is not enough.\n");
			ASSERT(0);
		}
	}
}

void mt7668CapInit(IN struct ADAPTER *prAdapter)
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
	prChipInfo->ucPacketFormat = TXD_PKT_FORMAT_TXD;
	prChipInfo->u4ExtraTxByteCount = 0;
	prChipInfo->asicFillInitCmdTxd = asicFillInitCmdTxd;
	prChipInfo->asicFillCmdTxd = asicFillCmdTxd;
	prChipInfo->u2CmdTxHdrSize = sizeof(struct WIFI_CMD);
	prChipInfo->u2RxSwPktBitMap = RXM_RXD_PKT_TYPE_SW_BITMAP;
	prChipInfo->u2RxSwPktEvent = RXM_RXD_PKT_TYPE_SW_EVENT;
	prChipInfo->u2RxSwPktFrame = RXM_RXD_PKT_TYPE_SW_FRAME;
	asicInitTxdHook(prChipInfo->prTxDescOps);
	asicInitRxdHook(prChipInfo->prRxDescOps);
#if CFG_SUPPORT_WIFI_SYSDVT
	prAdapter->u2TxTest = TX_TEST_UNLIMITIED;
	prAdapter->u2TxTestCount = 0;
	prAdapter->ucTxTestUP = TX_TEST_UP_UNDEF;
#endif /* CFG_SUPPORT_WIFI_SYSDVT */

	switch (prGlueInfo->u4InfType) {
#if defined(_HIF_PCIE)
	case MT_DEV_INF_PCIE:
		prChipInfo->u2TxInitCmdPort = TX_RING_FWDL_IDX_3;
		prChipInfo->u2TxFwDlPort = TX_RING_FWDL_IDX_3;
		break;
#endif /* _HIF_PCIE */
#if defined(_HIF_USB)
	case MT_DEV_INF_USB:
		prChipInfo->u2TxInitCmdPort = USB_DATA_BULK_OUT_EP8;
		prChipInfo->u2TxFwDlPort = USB_DATA_BULK_OUT_EP8;
		break;
#endif /* _HIF_USB */
	default:
		break;
	}
}

uint32_t mt7668GetFwDlInfo(struct ADAPTER *prAdapter,
	char *pcBuf, int i4TotalLen)
{
	struct WIFI_VER_INFO *prVerInfo = &prAdapter->rVerInfo;
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	struct TAILER_FORMAT_T_2 *prTailer;
#else
	struct TAILER_FORMAT_T *prTailer;
#endif
	uint32_t u4Offset = 0;
	uint8_t aucBuf[32];

#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	prTailer = &prVerInfo->rN9Compressedtailer;
#else
	prTailer = &prVerInfo->rN9tailer[0];
#endif
	kalMemZero(aucBuf, 32);
	kalMemCopy(aucBuf, prTailer->ram_version, 10);
	u4Offset += snprintf(pcBuf + u4Offset, i4TotalLen - u4Offset,
		"N9 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);

#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	prTailer = &prVerInfo->rCR4Compressedtailer;
#else
	prTailer = &prVerInfo->rCR4tailer[0];
#endif
	kalMemZero(aucBuf, 32);
	kalMemCopy(aucBuf, prTailer->ram_version, 10);
	u4Offset += snprintf(pcBuf + u4Offset, i4TotalLen - u4Offset,
		"CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);

	return u4Offset;
}

#if defined(_HIF_PCIE)

void mt7668PdmaConfig(struct GLUE_INFO *prGlueInfo, u_int8_t enable,
		bool fgResetHif)
{
	struct BUS_INFO *prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
	union WPDMA_GLO_CFG_STRUCT GloCfg;
	union WPDMA_INT_MASK IntMask;

	kalDevRegRead(prGlueInfo, WPDMA_GLO_CFG, &GloCfg.word);

	kalDevRegRead(prGlueInfo, WPDMA_INT_MSK, &IntMask.word);

	if (enable == TRUE) {
		/*0x4208 = 5440_1E70*/
		GloCfg.field_1.EnableTxDMA = 1;
		GloCfg.field_1.EnableRxDMA = 1;

		GloCfg.field_1.WPDMABurstSIZE = 3;
		GloCfg.field_1.EnTXWriteBackDDONE = 1;

		GloCfg.field_1.tx_bt_size = 1;
		GloCfg.field_1.multi_dma_en = 3;
		GloCfg.field_1.fifo_little_endian = 1;

		GloCfg.field_1.tx_bt_size_bit21 = 1;
		GloCfg.field_1.first_token = 1;
		GloCfg.field_1.omit_tx_info = 1;
		GloCfg.field_1.reserve_30 = 1;

		IntMask.field.rx_done_0 = 1;
		IntMask.field.rx_done_1 = 1;
		IntMask.field.tx_done = BIT(prBusInfo->tx_ring_fwdl_idx) |
			BIT(prBusInfo->tx_ring_cmd_idx) |
			BIT(prBusInfo->tx_ring0_data_idx)|
			BIT(prBusInfo->tx_ring1_data_idx);
		IntMask.field.tx_dly_int = 0;
	} else {
		GloCfg.field_1.EnableRxDMA = 0;
		GloCfg.field_1.EnableTxDMA = 0;

		IntMask.field.rx_done_0 = 0;
		IntMask.field.rx_done_1 = 0;
		IntMask.field.tx_done = 0;
		IntMask.field.tx_dly_int = 0;
	}

	kalDevRegWrite(prGlueInfo, WPDMA_INT_MSK, IntMask.word);
	kalDevRegWrite(prGlueInfo, WPDMA_GLO_CFG, GloCfg.word);


	/* new PDMA */
	/*  0x4260 = 0000_0005 */
	kalDevRegWrite(prGlueInfo, WPDMA_PAUSE_RX_Q_TH10, 0x5);

	/*  0x4500 = 0000_0001*/
	kalDevRegWrite(prGlueInfo, MT_WPDMA_GLO_CFG_1, 0x1);

	/*  0x4510 = 000F_0000*/
	kalDevRegWrite(prGlueInfo, MT_WPDMA_TX_PRE_CFG, 0xF0000);

	/* 0x4520 = 0F7F_0000 */
	kalDevRegWrite(prGlueInfo, MT_WPDMA_RX_PRE_CFG, 0xF7F0000);

	/* 0x4530 = 0EA6_0026 */
	kalDevRegWrite(prGlueInfo, MT_WPDMA_ABT_CFG, 0x0EA60026);

	/* 0x4534 = E4E4_E4E4*/
	kalDevRegWrite(prGlueInfo, MT_WPDMA_ABT_CFG1, 0xE4E4E4E4);

}

void mt7668LowPowerOwnRead(IN struct ADAPTER *prAdapter,
	OUT u_int8_t *pfgResult)
{
	uint32_t u4RegValue;

	HAL_MCR_RD(prAdapter, WPDMA_INT_STA, &u4RegValue);
	*pfgResult = ((u4RegValue & WPDMA_FW_CLR_OWN_INT) ? TRUE : FALSE);
}

void mt7668LowPowerOwnSet(IN struct ADAPTER *prAdapter, OUT u_int8_t *pfgResult)
{
	uint32_t u4RegValue;

	HAL_MCR_WR(prAdapter, CFG_PCIE_LPCR_HOST, PCIE_LPCR_HOST_SET_OWN);
	HAL_MCR_RD(prAdapter, CFG_PCIE_LPCR_HOST, &u4RegValue);
	*pfgResult = (u4RegValue == 0);
}

void mt7668LowPowerOwnClear(IN struct ADAPTER *prAdapter,
	OUT u_int8_t *pfgResult)
{
	uint32_t u4RegValue;

	HAL_MCR_WR(prAdapter, CFG_PCIE_LPCR_HOST, PCIE_LPCR_HOST_CLR_OWN);
	HAL_MCR_RD(prAdapter, CFG_PCIE_LPCR_HOST, &u4RegValue);
	*pfgResult = (u4RegValue == 0);
}
void mt7668EnableInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;
	union WPDMA_INT_MASK IntMask;

	HAL_MCR_RD(prAdapter, WPDMA_INT_MSK, &IntMask.word);

	IntMask.field.rx_done_0 = 1;
	IntMask.field.rx_done_1 = 1;
	IntMask.field.tx_done = BIT(prBusInfo->tx_ring_fwdl_idx) |
		BIT(prBusInfo->tx_ring_cmd_idx) |
		BIT(prBusInfo->tx_ring0_data_idx)|
		BIT(prBusInfo->tx_ring1_data_idx);
	IntMask.field.tx_coherent = 0;
	IntMask.field.rx_coherent = 0;
	IntMask.field.tx_dly_int = 0;
	IntMask.field.rx_dly_int = 0;
	IntMask.field.fw_clr_own = 1;

	HAL_MCR_WR(prAdapter, WPDMA_INT_MSK, IntMask.word);

	DBGLOG(HAL, TRACE, "%s [0x%08x]\n", __func__, IntMask.word);
}

void mt7668DisableInterrupt(IN struct ADAPTER *prAdapter)
{
	union WPDMA_INT_MASK IntMask;

	ASSERT(prAdapter);

	IntMask.word = 0;

	HAL_MCR_WR(prAdapter, WPDMA_INT_MSK, IntMask.word);
	HAL_MCR_RD(prAdapter, WPDMA_INT_MSK, &IntMask.word);

	DBGLOG(HAL, TRACE, "%s\n", __func__);
}

void mt7668WakeUpWiFi(IN struct ADAPTER *prAdapter)
{
	u_int8_t fgResult;

	ASSERT(prAdapter);

	HAL_LP_OWN_RD(prAdapter, &fgResult);

	if (fgResult)
		prAdapter->fgIsFwOwn = FALSE;
	else
		HAL_LP_OWN_CLR(prAdapter, &fgResult);
}
#endif /* _HIF_PCIE */

struct BUS_INFO mt7668_bus_info = {
#if defined(_HIF_PCIE)
	.top_cfg_base = MT7668_TOP_CFG_BASE,
	.host_tx_ring_base = MT_TX_RING_BASE,
	.host_tx_ring_ext_ctrl_base = MT_TX_RING_BASE_EXT,
	.host_tx_ring_cidx_addr = MT_TX_RING_CIDX,
	.host_tx_ring_didx_addr = MT_TX_RING_DIDX,
	.host_tx_ring_cnt_addr = MT_TX_RING_CNT,

	.host_rx_ring_base = MT_RX_RING_BASE,
	.host_rx_ring_ext_ctrl_base = MT_RX_RING_BASE_EXT,
	.host_rx_ring_cidx_addr = MT_RX_RING_CIDX,
	.host_rx_ring_didx_addr = MT_RX_RING_DIDX,
	.host_rx_ring_cnt_addr = MT_RX_RING_CNT,
	.bus2chip = mt7668_bus2chip_cr_mapping,
	.tx_ring_fwdl_idx = 3,
	.tx_ring_cmd_idx = 2,
	.tx_ring0_data_idx = 0,
	.tx_ring1_data_idx = 0,
	.fw_own_clear_addr = WPDMA_INT_STA,
	.fw_own_clear_bit = WPDMA_FW_CLR_OWN_INT,
	.max_static_map_addr = 0x00040000,
	.fgCheckDriverOwnInt = FALSE,
	.u4DmaMask = 32,

	.pdmaSetup = mt7668PdmaConfig,
	.updateTxRingMaxQuota = NULL,
	.enableInterrupt = mt7668EnableInterrupt,
	.disableInterrupt = mt7668DisableInterrupt,
	.lowPowerOwnRead = mt7668LowPowerOwnRead,
	.lowPowerOwnSet = mt7668LowPowerOwnSet,
	.lowPowerOwnClear = mt7668LowPowerOwnClear,
	.wakeUpWiFi = mt7668WakeUpWiFi,
	.isValidRegAccess = NULL,
	.getMailboxStatus = NULL,
	.setDummyReg = NULL,
	.checkDummyReg = NULL,
	.tx_ring_ext_ctrl = asicPdmaTxRingExtCtrl,
	.rx_ring_ext_ctrl = asicPdmaRxRingExtCtrl,
	.hifRst = NULL,
	.initPcieInt = NULL,
	.DmaShdlInit = NULL,
#endif /* _HIF_PCIE */
#if defined(_HIF_USB)
	.u4UdmaWlCfg_0_Addr = UDMA_WLCFG_0,
	.u4UdmaWlCfg_1_Addr = UDMA_WLCFG_1,
	.u4UdmaWlCfg_0 =
	    (UDMA_WLCFG_0_TX_EN(1) | UDMA_WLCFG_0_RX_EN(1) |
	    UDMA_WLCFG_0_RX_MPSZ_PAD0(1)),
	.u4device_vender_request_in = DEVICE_VENDOR_REQUEST_IN,
	.u4device_vender_request_out = DEVICE_VENDOR_REQUEST_OUT,
	.asicUsbSuspend = NULL,
	.asicUsbResume = NULL,
	.asicUsbEventEpDetected = NULL,
	.asicUsbRxByteCount = NULL,
	.DmaShdlInit = NULL,
#endif /* _HIF_USB */
#if defined(_HIF_SDIO)
	.halTxGetFreeResource = NULL,
	.halTxReturnFreeResource = NULL,
	.halRestoreTxResource = NULL,
	.halUpdateTxDonePendingCount = NULL,
#endif /* _HIF_SDIO */
};

struct FWDL_OPS_T mt7668_fw_dl_ops = {
	.constructFirmwarePrio = mt7668ConstructFirmwarePrio,
	.downloadPatch = wlanDownloadPatch,
	.downloadFirmware = wlanHarvardFormatDownload,
	.downloadByDynMemMap = NULL,
	.getFwInfo = wlanGetHarvardFwInfo,
	.getFwDlInfo = mt7668GetFwDlInfo,
	.phyAction = NULL,
};

struct TX_DESC_OPS_T mt7668TxDescOps = {
	.fillNicAppend = fillNicTxDescAppendWithCR4,
	.fillHifAppend = fillTxDescAppendByCR4,
	.fillTxByteCount = fillTxDescTxByteCountWithCR4,
};

struct RX_DESC_OPS_T mt7668RxDescOps = {
};

#if CFG_SUPPORT_QA_TOOL
struct ATE_OPS_T mt7668AteOps = {
	.setICapStart = mt6632SetICapStart,
	.getICapStatus = mt6632GetICapStatus,
	.getICapIQData = commonGetICapIQData,
	.getRbistDataDumpEvent = nicExtEventQueryMemDump,
};
#endif

struct CHIP_DBG_OPS mt7668_debug_ops = {
	.showPdmaInfo = NULL,
	.showPseInfo = NULL,
	.showPleInfo = NULL,
	.showTxdInfo = NULL,
	.showCsrInfo = NULL,
	.showDmaschInfo = NULL,
	.dumpMacInfo = NULL,
	.showWtblInfo = NULL,
	.showHifInfo = NULL,
	.printHifDbgInfo = NULL,
};

/* Litien code refine to support multi chip */
struct mt66xx_chip_info mt66xx_chip_info_mt7668 = {
	.bus_info = &mt7668_bus_info,
	.fw_dl_ops = &mt7668_fw_dl_ops,
	.prTxDescOps = &mt7668TxDescOps,
	.prRxDescOps = &mt7668RxDescOps,
#if CFG_SUPPORT_QA_TOOL
	.prAteOps = &mt7668AteOps,
#endif
	.prDebugOps = &mt7668_debug_ops,

	.chip_id = MT7668_CHIP_ID,
	.should_verify_chip_id = TRUE,
	.sw_sync0 = MT7668_SW_SYNC0,
	.sw_ready_bits = WIFI_FUNC_READY_BITS,
	.sw_ready_bit_offset = MT7668_SW_SYNC0_RDY_OFFSET,
	.patch_addr = MT7668_PATCH_START_ADDR,
	.is_support_cr4 = TRUE,
	.txd_append_size = MT7668_TX_DESC_APPEND_LENGTH,
	.rxd_size = MT7668_RX_DESC_LENGTH,
	.init_evt_rxd_size = MT7668_RX_DESC_LENGTH,
	.pse_header_length = NIC_TX_PSE_HEADER_LENGTH,
	.init_event_size = MT7668_RX_INIT_EVENT_LENGTH,
	.event_hdr_size = MT7668_RX_EVENT_HDR_LENGTH,
	.eco_info = mt7668_eco_table,
	.isNicCapV1 = TRUE,
	.is_support_efuse = TRUE,

	.asicCapInit = mt7668CapInit,
	.asicEnableFWDownload = NULL,
	.asicGetChipID = NULL,
	.downloadBufferBin = wlanDownloadBufferBin,
	.features = 0,
	.is_support_hw_amsdu = FALSE,
	.ucMaxSwAmsduNum = 0,
	.ucMaxSwapAntenna = 0,
	.workAround = 0,
	.prTxPwrLimitFile = "TxPwrLimit_MT76x8.dat",
	.ucTxPwrLimitBatchSize = 32,

	.top_hcr = TOP_HCR,
	.top_hvr = TOP_HVR,
	.top_fvr = TOP_FVR,
};

struct mt66xx_hif_driver_data mt66xx_driver_data_mt7668 = {
	.chip_info = &mt66xx_chip_info_mt7668,
};

