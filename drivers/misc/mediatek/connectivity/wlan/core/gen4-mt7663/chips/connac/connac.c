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
 *  \brief  Internal driver stack will export the required procedures here
 *          for GLUE Layer.
 *
 *  This file contains all routines which are exported from MediaTek 802.11
 *  Wireless LAN driver stack to GLUE Layer.
 */

#ifdef CONNAC

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#include "connac.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
uint8_t *apucConnacFwName[] = {
	(uint8_t *) CFG_FW_FILENAME "_soc1_0",
	NULL
};

struct ECO_INFO connac_eco_table[] = {
	/* HW version,  ROM version,    Factory version */
	{0x00, 0x00, 0xA, 0x1}, /* E1 */
	{0x00, 0x00, 0x0, 0x0}	/* End of table */
};

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
struct PCIE_CHIP_CR_MAPPING connac_bus2chip_cr_mapping[] = {
	/* chip addr, bus addr, range */
	{0x80000000, 0x00002000, 0x00001000}, /* MCU_CFG */

	{0x50000000, 0x00004000, 0x00004000}, /* CONN_HIF (PDMA) */
	{0x50002000, 0x00005000, 0x00001000}, /* CONN_HIF (Reserved) */
	{0x5000A000, 0x00006000, 0x00001000}, /* CONN_HIF (DMASHDL) */
	{0x000E0000, 0x00007000, 0x00001000}, /* CONN_HIF_ON (HOST_CSR) */

	{0x82060000, 0x00008000, 0x00004000}, /* WF_UMAC_TOP (PLE) */
	{0x82068000, 0x0000C000, 0x00002000}, /* WF_UMAC_TOP (PSE) */
	{0x8206C000, 0x0000E000, 0x00002000}, /* WF_UMAC_TOP (PP) */
	{0x82070000, 0x00010000, 0x00010000}, /* WF_PHY */
	{0x820F0000, 0x00020000, 0x00010000}, /* WF_LMAC_TOP */
	{0x820E0000, 0x00030000, 0x00010000}, /* WF_LMAC_TOP (WF_WTBL) */

	{0x81000000, 0x00050000, 0x00010000}, /* BTSYS_OFF */
	{0x80070000, 0x00060000, 0x00010000}, /* GPSSYS */
	{0x40000000, 0x00070000, 0x00010000}, /* WF_SYSRAM */
	{0x00300000, 0x00080000, 0x00010000}, /* MCU_SYSRAM */

	{0x80010000, 0x000A1000, 0x00001000}, /* CONN_MCU_DMA */
	{0x80030000, 0x000A2000, 0x00001000}, /* CONN_MCU_BTIF0 */
	{0x81030000, 0x000A3000, 0x00001000}, /* CONN_MCU_CFG_ON */
	{0x80050000, 0x000A4000, 0x00001000}, /* CONN_UART_PTA */
	{0x81040000, 0x000A5000, 0x00001000}, /* CONN_MCU_CIRQ */
	{0x81050000, 0x000A6000, 0x00001000}, /* CONN_MCU_GPT */
	{0x81060000, 0x000A7000, 0x00001000}, /* CONN_PTA */
	{0x81080000, 0x000A8000, 0x00001000}, /* CONN_MCU_WDT */
	{0x81090000, 0x000A9000, 0x00001000}, /* CONN_MCU_PDA */
	{0x810A0000, 0x000AA000, 0x00001000}, /* CONN_RDD_AHB_WRAP0 */
	{0x810B0000, 0x000AB000, 0x00001000}, /* BTSYS_ON */
	{0x810C0000, 0x000AC000, 0x00001000}, /* CONN_RBIST_TOP */
	{0x810D0000, 0x000AD000, 0x00001000}, /* CONN_RDD_AHB_WRAP0 */
	{0x820D0000, 0x000AE000, 0x00001000}, /* WFSYS_ON */
	{0x60000000, 0x000AF000, 0x00001000}, /* CONN_MCU_PDA */

	{0x80020000, 0x000B0000, 0x00010000}, /* CONN_TOP_MISC_OFF */
	{0x81020000, 0x000C0000, 0x00010000}, /* CONN_TOP_MISC_ON */

	{0x0, 0x0, 0x0}
};
#endif /* _HIF_PCIE || _HIF_AXI */

int connacGetIpSetVersion(struct GLUE_INFO *prGlueInfo)
{
	return 1;
}

void connacConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx)
{
	uint8_t ucIdx = 0;

	for (ucIdx = 0; apucConnacFwName[ucIdx]; ucIdx++) {
		if ((*pucNameIdx + 3) < ucMaxNameIdx) {
			/* Type 1. WIFI_RAM_CODE_soc1_0_1_1 */
			snprintf(*(apucName + (*pucNameIdx)),
					CFG_FW_NAME_MAX_LEN, "%s_%u_%u",
					apucConnacFwName[ucIdx],
					connacGetIpSetVersion(prGlueInfo),
					wlanGetEcoVersion(
						prGlueInfo->prAdapter));
			(*pucNameIdx) += 1;

			/* Type 2. WIFI_RAM_CODE_soc1_0_1_1.bin */
			snprintf(*(apucName + (*pucNameIdx)),
					CFG_FW_NAME_MAX_LEN, "%s_%u_%u.bin",
					apucConnacFwName[ucIdx],
					connacGetIpSetVersion(prGlueInfo),
					wlanGetEcoVersion(
						prGlueInfo->prAdapter));
			(*pucNameIdx) += 1;

			/* Type 3. WIFI_RAM_CODE_soc1_0 */
			snprintf(*(apucName + (*pucNameIdx)),
					CFG_FW_NAME_MAX_LEN, "%s",
					apucConnacFwName[ucIdx]);
			(*pucNameIdx) += 1;

			/* Type 4. WIFI_RAM_CODE_soc1_0.bin */
			snprintf(*(apucName + (*pucNameIdx)),
					CFG_FW_NAME_MAX_LEN, "%s.bin",
					apucConnacFwName[ucIdx]);
			(*pucNameIdx) += 1;
		} else {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
				"kalFirmwareImageMapping >> file name array is not enough.\n");
			ASSERT(0);
		}
	}
}

void connacConstructPatchName(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucName, uint8_t *pucNameIdx)
{
	snprintf(apucName[(*pucNameIdx)],
		CFG_FW_NAME_MAX_LEN, "mtsoc1_0_patch_e%x_hdr.bin",
		wlanGetEcoVersion(prGlueInfo->prAdapter));
}

struct BUS_INFO connac_bus_info = {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	.top_cfg_base = CONNAC_TOP_CFG_BASE,
	.bus2chip = connac_bus2chip_cr_mapping,
	.tx_ring_fwdl_idx = 3,
	.tx_ring_cmd_idx = 15,
	.tx_ring_data_idx = 0,
	.fgCheckDriverOwnInt = FALSE,
	.fgInitPCIeInt = FALSE,
	.u4DmaMask = 36,

	.pdmaSetup = asicPdmaConfig,
	.enableInterrupt = asicEnableInterrupt,
	.disableInterrupt = asicDisableInterrupt,
	.lowPowerOwnRead = asicLowPowerOwnRead,
	.lowPowerOwnSet = asicLowPowerOwnSet,
	.lowPowerOwnClear = asicLowPowerOwnClear,
	.wakeUpWiFi = asicWakeUpWiFi,
	.isValidRegAccess = asicIsValidRegAccess,
	.getMailboxStatus = asicGetMailboxStatus,
	.setDummyReg = asicSetDummyReg,
	.checkDummyReg = asicCheckDummyReg,
#endif /* _HIF_PCIE || _HIF_AXI */
#if defined(_HIF_USB)
	.u4UdmaWlCfg_0_Addr = CONNAC_UDMA_WLCFG_0,
	.u4UdmaWlCfg_1_Addr = CONNAC_UDMA_WLCFG_1,
	.u4UdmaWlCfg_0 =
		(UDMA_WLCFG_0_TX_EN(1) |
		UDMA_WLCFG_0_RX_EN(1) |
		UDMA_WLCFG_0_RX_MPSZ_PAD0(1) |
		UDMA_WLCFG_0_1US_TIMER_EN(1)),
#endif /* _HIF_USB */
};

struct FWDL_OPS_T connac_fw_dl_ops = {
	.constructFirmwarePrio = connacConstructFirmwarePrio,
	.constructPatchName = connacConstructPatchName,
#if !CFG_MTK_ANDROID_WMT
	.downloadPatch = wlanDownloadPatch,
#endif
	.downloadFirmware = wlanConnacFormatDownload,
	.getFwInfo = wlanGetConnacFwInfo,
	.getFwDlInfo = asicGetFwDlInfo,
};

struct TX_DESC_OPS_T connacTxDescOps = {
	.fillNicAppend = fillNicTxDescAppend,
	.fillHifAppend = fillTxDescAppendByHostV2,
	.fillTxByteCount = fillTxDescTxByteCount,
};

#if CFG_SUPPORT_QA_TOOL
struct ATE_OPS_T connacAteOps = {
	.setICapStart = connacSetICapStart,
	.getICapStatus = connacGetICapStatus,
	.getICapIQData = connacGetICapIQData,
	.getRbistDataDumpEvent = nicExtEventICapIQData,
};
#endif

struct CHIP_DBG_OPS connac_debug_ops = {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	.showPdmaInfo = halShowPdmaInfo,
	.showPseInfo = halShowPseInfo,
	.showPleInfo = halShowPleInfo,
	.showCsrInfo = halShowHostCsrInfo,
	.showDmaschInfo = halShowDmaschInfo,
#else
	.showPdmaInfo = NULL,
	.showPseInfo = NULL,
	.showPleInfo = NULL,
	.showCsrInfo = NULL,
	.showDmaschInfo = NULL,
#endif
};

struct mt66xx_chip_info mt66xx_chip_info_connac = {
	.bus_info = &connac_bus_info,
	.fw_dl_ops = &connac_fw_dl_ops,
	.prTxDescOps = &connacTxDescOps,
#if CFG_SUPPORT_QA_TOOL
	.prAteOps = &connacAteOps,
#endif
	.prDebugOps = &connac_debug_ops,

	.chip_id = CONNAC_CHIP_ID,
	.should_verify_chip_id = FALSE,
	.sw_sync0 = CONNAC_SW_SYNC0,
	.sw_ready_bits = WIFI_FUNC_NO_CR4_READY_BITS,
	.sw_ready_bit_offset = CONNAC_SW_SYNC0_RDY_OFFSET,
	.patch_addr = CONNAC_PATCH_START_ADDR,
	.is_support_cr4 = FALSE,
	.txd_append_size = CONNAC_TX_DESC_APPEND_LENGTH,
	.rxd_size = CONNAC_RX_DESC_LENGTH,
	.init_event_size = CONNAC_RX_INIT_EVENT_LENGTH,
	.event_hdr_size = CONNAC_RX_EVENT_HDR_LENGTH,
	.eco_info = connac_eco_table,
	.isNicCapV1 = FALSE,
	.is_support_efuse = FALSE,

	/* IP info, should be overwrite by getNicCapabalityV2 */
	.u4ChipIpVersion = CONNAC_CHIP_IP_VERSION,
	.u4ChipIpConfig = CONNAC_CHIP_IP_CONFIG,
	.asicCapInit = asicCapInit,
	.asicEnableFWDownload = asicEnableFWDownload,
	.asicGetChipID = asicGetChipID,
	.downloadBufferBin = NULL,
#if CFG_MTK_ANDROID_WMT
	.showTaskStack = connectivity_export_show_stack,
#else
	.showTaskStack = NULL,
#endif
	.is_support_hw_amsdu = FALSE,
	.ucMaxSwAmsduNum = 4,
	.workAround = 0,
};

struct mt66xx_hif_driver_data mt66xx_driver_data_connac = {
	.chip_info = &mt66xx_chip_info_connac,
};

#endif /* CONNAC */
