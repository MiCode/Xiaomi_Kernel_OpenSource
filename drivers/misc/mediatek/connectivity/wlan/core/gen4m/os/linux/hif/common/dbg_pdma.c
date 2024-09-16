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
/******************************************************************************
 *[File]             dbg_pdma.c
 *[Version]          v1.0
 *[Revision Date]    2015-09-08
 *[Author]
 *[Description]
 *    The program provides PDMA HIF APIs
 *[Copyright]
 *    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#include "pse.h"
#include "wf_ple.h"
#include "host_csr.h"
#include "dma_sch.h"
#include "mt_dmac.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

struct wfdma_ring_info {
	char name[20];
	uint32_t ring_idx;
	bool dump_ring_content;

	/* query from register */
	uint32_t base;
	uint32_t base_ext;
	uint32_t cnt;
	uint32_t cidx;
	uint32_t didx;
};

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
static void halCheckHifState(struct ADAPTER *prAdapter);
static void halDumpHifDebugLog(struct ADAPTER *prAdapter);
static bool halIsTxHang(struct ADAPTER *prAdapter);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

void halPrintHifDbgInfo(IN struct ADAPTER *prAdapter)
{
	if (!prAdapter->fgIsFwOwn) {
		halCheckHifState(prAdapter);
		halDumpHifDebugLog(prAdapter);
	}
	if (prAdapter->chip_info->dumpwfsyscpupcr)
		prAdapter->chip_info->dumpwfsyscpupcr(prAdapter);
}

static void halCheckHifState(struct ADAPTER *prAdapter)
{
	uint32_t u4DebugLevel = 0;
	if (prAdapter->u4HifChkFlag & HIF_CHK_TX_HANG) {
		if (halIsTxHang(prAdapter)) {
			DBGLOG(HAL, ERROR,
			       "Tx timeout, set hif debug info flag\n");
			wlanGetDriverDbgLevel(DBG_TX_IDX, &u4DebugLevel);
			if (u4DebugLevel & DBG_CLASS_TRACE) {
				DBGLOG(HAL, ERROR, "Set debug flag bit\n");
				prAdapter->u4HifDbgFlag |= DEG_HIF_ALL;
			}
			else {
				struct CHIP_DBG_OPS *prDbgOps;

				prDbgOps = prAdapter->chip_info->prDebugOps;
				DBGLOG(HAL, ERROR, "Dump debug info\n");
				if (prDbgOps && prDbgOps->showPleInfo)
					prDbgOps->showPleInfo(prAdapter, FALSE);

				if (prDbgOps && prDbgOps->showPseInfo)
					prDbgOps->showPseInfo(prAdapter);

				if (prDbgOps && prDbgOps->showPdmaInfo)
					prDbgOps->showPdmaInfo(prAdapter);

				if (prDbgOps && prDbgOps->showDmaschInfo)
					prDbgOps->showDmaschInfo(prAdapter);

				if (prDbgOps && prDbgOps->dumpMacInfo)
					prDbgOps->dumpMacInfo(prAdapter);

			}
		}
	}

	if (prAdapter->u4HifChkFlag & HIF_DRV_SER)
		halSetDrvSer(prAdapter);

	prAdapter->u4HifChkFlag = 0;
}

static void halDumpHifDebugLog(struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct CHIP_DBG_OPS *prDbgOps;
	uint32_t ret = 0;

	ASSERT(prAdapter);
	prGlueInfo = prAdapter->prGlueInfo;
	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	/* Only dump all hif log once */
	if (prAdapter->u4HifDbgFlag & DEG_HIF_ALL) {
		if (!prAdapter->fgEnHifDbgInfo) {
			DBGLOG(HAL, ERROR, "return due to HifDbg is NULL\n");
			prAdapter->u4HifDbgFlag = 0;
			return;
		}
		prAdapter->fgEnHifDbgInfo = false;
	}

	/* Avoid register checking */
	prHifInfo->fgIsDumpLog = true;

	prDbgOps = prAdapter->chip_info->prDebugOps;

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_HOST_CSR)) {
		if (prDbgOps->showCsrInfo) {
			bool fgIsClkEn = prDbgOps->showCsrInfo(prAdapter);

			if (!fgIsClkEn)
				return;
		}
	}

	/* need to check Bus readable */
	if (prAdapter->chip_info->checkbushang) {
		ret = prAdapter->chip_info->checkbushang((void *) prAdapter,
				TRUE);
		if (ret != 0) {
			DBGLOG(HAL, ERROR,
				"return due to checkbushang fail %d\n", ret);
			return;
		}
	}

	/* Check Driver own HW CR */
	{
		struct BUS_INFO *prBusInfo = NULL;
		u_int8_t driver_owen_result = 0;

		prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

		if (prBusInfo->lowPowerOwnRead)
			prBusInfo->lowPowerOwnRead(prGlueInfo->prAdapter,
				&driver_owen_result);
		else {
			DBGLOG(HAL, ERROR, "retrun due to null API\n");
			return;
		}

		if (driver_owen_result == 0) {
			DBGLOG(HAL, ERROR, "return, not driver-own[%d]\n",
				driver_owen_result);
			return;
		}
	}

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_PLE)) {
		if (prDbgOps && prDbgOps->showPleInfo)
			prDbgOps->showPleInfo(prAdapter, FALSE);
	}

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_PSE)) {
		if (prDbgOps && prDbgOps->showPseInfo)
			prDbgOps->showPseInfo(prAdapter);
	}

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_PDMA)) {
		if (prDbgOps && prDbgOps->showPdmaInfo)
			prDbgOps->showPdmaInfo(prAdapter);
	}

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_DMASCH)) {
		if (prDbgOps && prDbgOps->showDmaschInfo)
			prDbgOps->showDmaschInfo(prAdapter);
	}

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_MAC)) {
		if (prDbgOps && prDbgOps->dumpMacInfo)
			prDbgOps->dumpMacInfo(prAdapter);
	}

	if (prAdapter->u4HifDbgFlag & (DEG_HIF_ALL | DEG_HIF_PHY))
		haldumpPhyInfo(prAdapter);

	prHifInfo->fgIsDumpLog = false;
	prAdapter->u4HifDbgFlag = 0;
}

static void halDumpTxRing(IN struct GLUE_INFO *prGlueInfo,
			  IN uint16_t u2Port, IN uint32_t u4Idx)
{
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	struct RTMP_TX_RING *prTxRing;
	struct TXD_STRUCT *pTxD;

	if (u2Port >= NUM_OF_TX_RING || u4Idx >= TX_RING_SIZE) {
		DBGLOG(HAL, INFO, "Dump fail u2Port[%u] u4Idx[%u]\n",
		       u2Port, u4Idx);
		return;
	}

	prTxRing = &prHifInfo->TxRing[u2Port];

	pTxD = (struct TXD_STRUCT *) prTxRing->Cell[u4Idx].AllocVa;

	log_dbg(SW4, INFO, "TX Ring[%u] Idx[%04u] SDP0[0x%08x] SDL0[%u] LS[%u] B[%u] DDONE[%u] SDP0_EXT[%u]\n",
		u2Port, u4Idx, pTxD->SDPtr0, pTxD->SDLen0, pTxD->LastSec0,
		pTxD->Burst, pTxD->DMADONE, pTxD->SDPtr0Ext);
}

uint32_t halDumpHifStatus(IN struct ADAPTER *prAdapter,
	IN uint8_t *pucBuf, IN uint32_t u4Max)
{
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	uint32_t u4Idx, u4DmaIdx = 0;
	uint32_t u4CpuIdx = 0, u4MaxCnt = 0;
	uint32_t u4Len = 0;
	struct RTMP_TX_RING *prTxRing;
	struct RTMP_RX_RING *prRxRing;

	LOGBUF(pucBuf, u4Max, u4Len, "\n------<Dump HIF Status>------\n");

	for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
		prTxRing = &prHifInfo->TxRing[u4Idx];
		kalDevRegRead(prGlueInfo, prTxRing->hw_cnt_addr, &u4MaxCnt);
		kalDevRegRead(prGlueInfo, prTxRing->hw_cidx_addr, &u4CpuIdx);
		kalDevRegRead(prGlueInfo, prTxRing->hw_didx_addr, &u4DmaIdx);

		LOGBUF(pucBuf, u4Max, u4Len,
			"TX[%u] SZ[%04u] CPU[%04u/%04u] DMA[%04u/%04u] SW_UD[%04u] Used[%u]\n",
			u4Idx, u4MaxCnt, prTxRing->TxCpuIdx,
			u4CpuIdx, prTxRing->TxDmaIdx,
			u4DmaIdx, prTxRing->TxSwUsedIdx, prTxRing->u4UsedCnt);

		if (u4Idx == TX_RING_DATA0_IDX_0) {
			halDumpTxRing(prGlueInfo, u4Idx, prTxRing->TxCpuIdx);
			halDumpTxRing(prGlueInfo, u4Idx, u4CpuIdx);
			halDumpTxRing(prGlueInfo, u4Idx, u4DmaIdx);
			halDumpTxRing(prGlueInfo, u4Idx, prTxRing->TxSwUsedIdx);
		}

		if (u4Idx == TX_RING_DATA1_IDX_1) {
			halDumpTxRing(prGlueInfo, u4Idx, prTxRing->TxCpuIdx);
			halDumpTxRing(prGlueInfo, u4Idx, u4CpuIdx);
			halDumpTxRing(prGlueInfo, u4Idx, u4DmaIdx);
			halDumpTxRing(prGlueInfo, u4Idx, prTxRing->TxSwUsedIdx);
		}
	}

	for (u4Idx = 0; u4Idx < NUM_OF_RX_RING; u4Idx++) {
		prRxRing = &prHifInfo->RxRing[u4Idx];

		kalDevRegRead(prGlueInfo, prRxRing->hw_cnt_addr, &u4MaxCnt);
		kalDevRegRead(prGlueInfo, prRxRing->hw_cidx_addr, &u4CpuIdx);
		kalDevRegRead(prGlueInfo, prRxRing->hw_didx_addr, &u4DmaIdx);

		LOGBUF(pucBuf, u4Max, u4Len,
		       "RX[%u] SZ[%04u] CPU[%04u/%04u] DMA[%04u/%04u]\n",
		       u4Idx, u4MaxCnt, prRxRing->RxCpuIdx, u4CpuIdx,
		       prRxRing->RxDmaIdx, u4DmaIdx);
	}

	LOGBUF(pucBuf, u4Max, u4Len, "MSDU Tok: Free[%u] Used[%u]\n",
		halGetMsduTokenFreeCnt(prGlueInfo->prAdapter),
		prGlueInfo->rHifInfo.rTokenInfo.u4UsedCnt);
	LOGBUF(pucBuf, u4Max, u4Len, "Pending QLen Normal[%u] Sec[%u]\n",
		prGlueInfo->i4TxPendingFrameNum,
		prGlueInfo->i4TxPendingSecurityFrameNum);

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n\n");

	return u4Len;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Compare two struct timeval
 *
 * @param prTs1          a pointer to timeval
 * @param prTs2          a pointer to timeval
 *
 *
 * @retval 0             two time value is equal
 * @retval 1             prTs1 value > prTs2 value
 * @retval -1            prTs1 value < prTs2 value
 */
/*----------------------------------------------------------------------------*/
int halTimeCompare(struct timeval *prTs1, struct timeval *prTs2)
{
	if (prTs1->tv_sec > prTs2->tv_sec)
		return 1;
	else if (prTs1->tv_sec < prTs2->tv_sec)
		return -1;
	/* sec part is equal */
	else if (prTs1->tv_usec > prTs2->tv_usec)
		return 1;
	else if (prTs1->tv_usec < prTs2->tv_usec)
		return -1;
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Checking tx hang
 *
 * @param prAdapter      a pointer to adapter private data structure.
 *
 * @retval true          tx is hang because msdu report too long
 */
/*----------------------------------------------------------------------------*/
static bool halIsTxHang(struct ADAPTER *prAdapter)
{
	struct MSDU_TOKEN_INFO *prTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	struct MSDU_INFO *prMsduInfo;
	struct timeval rNowTs, rTime, rLongest, rTimeout;
	uint32_t u4Idx = 0, u4TokenId = 0;
	bool fgIsTimeout = false;
	struct WIFI_VAR *prWifiVar;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);

	prTokenInfo = &prAdapter->prGlueInfo->rHifInfo.rTokenInfo;
	prWifiVar = &prAdapter->rWifiVar;

	rTimeout.tv_sec = prWifiVar->ucMsduReportTimeout;
	rTimeout.tv_usec = 0;
	rLongest.tv_sec = 0;
	rLongest.tv_usec = 0;
	do_gettimeofday(&rNowTs);

	for (u4Idx = 0; u4Idx < HIF_TX_MSDU_TOKEN_NUM; u4Idx++) {
		prToken = &prTokenInfo->arToken[u4Idx];
		prMsduInfo = prToken->prMsduInfo;
		if (!prToken->fgInUsed || !prMsduInfo)
			continue;

		/* check tx hang is enabled */
		if ((prAdapter->u4TxHangFlag &
		      BIT(prMsduInfo->ucBssIndex)) == 0)
			continue;

		/* Ignore now time < token time */
		if (halTimeCompare(&rNowTs, &prToken->rTs) < 0)
			continue;

		rTime.tv_sec = rNowTs.tv_sec - prToken->rTs.tv_sec;
		rTime.tv_usec = rNowTs.tv_usec;
		if (prToken->rTs.tv_usec > rNowTs.tv_usec) {
			rTime.tv_sec -= 1;
			rTime.tv_usec += SEC_TO_USEC(1);
		}
		rTime.tv_usec -= prToken->rTs.tv_usec;

		if (halTimeCompare(&rTime, &rTimeout) >= 0)
			fgIsTimeout = true;

		/* rTime > rLongest */
		if (halTimeCompare(&rTime, &rLongest) > 0) {
			rLongest.tv_sec = rTime.tv_sec;
			rLongest.tv_usec = rTime.tv_usec;
			u4TokenId = u4Idx;
		}
	}

	if (fgIsTimeout) {
		DBGLOG(HAL, INFO, "TokenId[%u] timeout[sec:%ld, usec:%ld]\n",
		       u4TokenId, rLongest.tv_sec, rLongest.tv_usec);
		prToken = &prTokenInfo->arToken[u4TokenId];
		if (prToken->prPacket)
			DBGLOG_MEM32(HAL, INFO, prToken->prPacket, 64);
	}

	return fgIsTimeout;
}

void kalDumpTxRing(struct GLUE_INFO *prGlueInfo,
		   struct RTMP_TX_RING *prTxRing,
		   uint32_t u4Num, bool fgDumpContent)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_DMACB *pTxCell;
	struct TXD_STRUCT *pTxD;
	uint32_t u4DumpLen = 64;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;

	if (u4Num >= TX_RING_SIZE)
		return;

	pTxCell = &prTxRing->Cell[u4Num];
	pTxD = (struct TXD_STRUCT *) pTxCell->AllocVa;

	if (!pTxD)
		return;

	DBGLOG(HAL, INFO, "Tx Dese Num[%u]\n", u4Num);
	DBGLOG_MEM32(HAL, INFO, pTxD, sizeof(struct TXD_STRUCT));

	if (!fgDumpContent)
		return;

	DBGLOG(HAL, INFO, "Tx Contents\n");
	if (prMemOps->dumpTx)
		prMemOps->dumpTx(prHifInfo, prTxRing, u4Num, u4DumpLen);
	DBGLOG(HAL, INFO, "\n\n");
}

void kalDumpRxRing(struct GLUE_INFO *prGlueInfo,
		   struct RTMP_RX_RING *prRxRing,
		   uint32_t u4Num, bool fgDumpContent)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_DMACB *pRxCell;
	struct RXD_STRUCT *pRxD;
	uint32_t u4DumpLen = 64;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;

	if (u4Num >= prRxRing->u4RingSize)
		return;

	pRxCell = &prRxRing->Cell[u4Num];
	pRxD = (struct RXD_STRUCT *) pRxCell->AllocVa;

	if (!pRxD)
		return;

	DBGLOG(HAL, INFO, "Rx Dese Num[%u]\n", u4Num);
	DBGLOG_MEM32(HAL, INFO, pRxD, sizeof(struct RXD_STRUCT));

	if (!fgDumpContent)
		return;

	if (u4DumpLen > pRxD->SDLen0)
		u4DumpLen = pRxD->SDLen0;

	DBGLOG(HAL, INFO, "Rx Contents\n");
	if (prMemOps->dumpRx)
		prMemOps->dumpRx(prHifInfo, prRxRing, u4Num, u4DumpLen);
	DBGLOG(HAL, INFO, "\n\n");
}

void halShowPdmaInfo(IN struct ADAPTER *prAdapter)
{
#define BUF_SIZE 1024

	uint32_t i = 0, u4Value = 0, pos = 0;
	uint32_t offset, offset_ext, SwIdx;
	char *buf;
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct BUS_INFO *prBus_info = prAdapter->chip_info->bus_info;
	struct RTMP_TX_RING *prTxRing;
	struct RTMP_RX_RING *prRxRing;
	struct wfdma_ring_info wfmda_tx_group[] = {
		{"AP DATA0", prBus_info->tx_ring0_data_idx, true},
		{"AP DATA1", prBus_info->tx_ring1_data_idx, true},
		{"AP CMD", prBus_info->tx_ring_cmd_idx, true},
		{"FWDL", prBus_info->tx_ring_fwdl_idx, true},
#if CFG_MTK_MCIF_WIFI_SUPPORT
		{"MD DATA0", 8, false},
		{"MD DATA1", 9, false},
		{"MD CMD", 14, false},
#endif
	};
	struct wfdma_ring_info wfmda_rx_group[] = {
		{"AP DATA", 0, true},
		{"AP EVENT", 1, true},
#if CFG_MTK_MCIF_WIFI_SUPPORT
		{"MD DATA", 2, false},
		{"MD EVENT", 3, false},
#endif
	};

	buf = (char *) kalMemAlloc(BUF_SIZE, VIR_MEM_TYPE);

	/* PDMA HOST_INT */
	HAL_MCR_RD(prAdapter, WPDMA_INT_STA, &u4Value);
	DBGLOG(HAL, INFO, "WPDMA HOST_INT:0x%08x = 0x%08x\n",
		WPDMA_INT_STA, u4Value);

	/* PDMA GLOBAL_CFG  */
	HAL_MCR_RD(prAdapter, WPDMA_GLO_CFG, &u4Value);
	DBGLOG(HAL, INFO, "WPDMA GLOBAL_CFG:0x%08x = 0x%08x\n",
		WPDMA_GLO_CFG, u4Value);

	HAL_MCR_RD(prAdapter, CONN_HIF_RST, &u4Value);
	DBGLOG(HAL, INFO, "WPDMA CONN_HIF_RST:0x%08x = 0x%08x\n",
		CONN_HIF_RST, u4Value);

	HAL_MCR_RD(prAdapter, MCU2HOST_SW_INT_STA, &u4Value);
	DBGLOG(HAL, INFO, "WPDMA MCU2HOST_SW_INT_STA:0x%08x = 0x%08x\n",
		MCU2HOST_SW_INT_STA, u4Value);

#if CFG_MTK_MCIF_WIFI_SUPPORT
	HAL_MCR_RD(prAdapter, MD_INT_STA, &u4Value);
	DBGLOG(HAL, INFO, "MD_INT_STA:0x%08x = 0x%08x\n",
		MD_INT_STA, u4Value);
	HAL_MCR_RD(prAdapter, MD_WPDMA_GLO_CFG, &u4Value);
	DBGLOG(HAL, INFO, "MD_WPDMA_GLO_CFG:0x%08x = 0x%08x\n",
		MD_WPDMA_GLO_CFG, u4Value);
	HAL_MCR_RD(prAdapter, MD_INT_ENA, &u4Value);
	DBGLOG(HAL, INFO, "MD_INT_ENA:0x%08x = 0x%08x\n",
		MD_INT_ENA, u4Value);
	HAL_MCR_RD(prAdapter, MD_WPDMA_DLY_INIT_CFG, &u4Value);
	DBGLOG(HAL, INFO, "MD_WPDMA_DLY_INIT_CFG:0x%08x = 0x%08x\n",
		MD_WPDMA_DLY_INIT_CFG, u4Value);
	HAL_MCR_RD(prAdapter, MD_WPDMA_MISC, &u4Value);
	DBGLOG(HAL, INFO, "MD_WPDMA_MISC:0x%08x = 0x%08x\n",
		MD_WPDMA_MISC, u4Value);
#endif

	/* PDMA Tx/Rx Ring  Info */
	DBGLOG(HAL, INFO, "Tx Ring configuration\n");
	DBGLOG(HAL, INFO, "%10s%10s%12s%20s%10s%10s%10s\n",
		"Tx Ring", "Idx", "Reg", "Base", "Cnt", "CIDX", "DIDX");

	if (buf) {
		kalMemZero(buf, BUF_SIZE);
		for (i = 0; i < sizeof(wfmda_tx_group) /
				sizeof(struct wfdma_ring_info); i++) {
			int ret;

			offset = wfmda_tx_group[i].ring_idx *
				MT_RINGREG_DIFF;
			offset_ext = wfmda_tx_group[i].ring_idx *
				MT_RINGREG_EXT_DIFF;

			HAL_MCR_RD(prAdapter, WPDMA_TX_RING0_CTRL0 + offset,
					&wfmda_tx_group[i].base);
			HAL_MCR_RD(prAdapter, WPDMA_TX_RING0_BASE_PTR_EXT +
					offset_ext,
					&wfmda_tx_group[i].base_ext);
			HAL_MCR_RD(prAdapter, WPDMA_TX_RING0_CTRL1 + offset,
					&wfmda_tx_group[i].cnt);
			HAL_MCR_RD(prAdapter, WPDMA_TX_RING0_CTRL2 + offset,
					&wfmda_tx_group[i].cidx);
			HAL_MCR_RD(prAdapter, WPDMA_TX_RING0_CTRL3 + offset,
					&wfmda_tx_group[i].didx);

			ret = kalSnprintf(buf, BUF_SIZE,
				"%10s%10d  0x%08x  0x%016llx%10d%10d%10d",
				wfmda_tx_group[i].name,
				wfmda_tx_group[i].ring_idx,
				WPDMA_TX_RING0_CTRL0 + offset,
				(wfmda_tx_group[i].base + ((uint64_t)
					wfmda_tx_group[i].base_ext << 32)),
				wfmda_tx_group[i].cnt,
				wfmda_tx_group[i].cidx,
				wfmda_tx_group[i].didx);
			if (ret >= 0 || ret < BUF_SIZE)
				DBGLOG(HAL, INFO, "%s\n", buf);
			else
				DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
						__LINE__, ret);
		}

		DBGLOG(HAL, INFO, "Rx Ring configuration\n");
		DBGLOG(HAL, INFO, "%10s%10s%12s%20s%10s%10s%10s\n",
			"Rx Ring", "Idx", "Reg", "Base", "Cnt", "CIDX", "DIDX");

		kalMemZero(buf, BUF_SIZE);
		for (i = 0; i < sizeof(wfmda_rx_group) /
				sizeof(struct wfdma_ring_info); i++) {
			int ret;

			offset = wfmda_rx_group[i].ring_idx * MT_RINGREG_DIFF;
			offset_ext = wfmda_rx_group[i].ring_idx *
				MT_RINGREG_EXT_DIFF;

			HAL_MCR_RD(prAdapter, WPDMA_RX_RING0_CTRL0 + offset,
					&wfmda_rx_group[i].base);
			HAL_MCR_RD(prAdapter, WPDMA_RX_RING0_BASE_PTR_EXT +
				offset_ext,
					&wfmda_rx_group[i].base_ext);
			HAL_MCR_RD(prAdapter, WPDMA_RX_RING0_CTRL1 + offset,
					&wfmda_rx_group[i].cnt);
			HAL_MCR_RD(prAdapter, WPDMA_RX_RING0_CTRL2 + offset,
					&wfmda_rx_group[i].cidx);
			HAL_MCR_RD(prAdapter, WPDMA_RX_RING0_CTRL3 + offset,
					&wfmda_rx_group[i].didx);

			ret = kalSnprintf(buf, BUF_SIZE,
				"%10s%10d  0x%08x  0x%016llx%10d%10d%10d",
				wfmda_rx_group[i].name,
				wfmda_rx_group[i].ring_idx,
				WPDMA_RX_RING0_CTRL0 + offset,
				(wfmda_rx_group[i].base + ((uint64_t)
					wfmda_rx_group[i].base_ext << 32)),
				wfmda_rx_group[i].cnt,
				wfmda_rx_group[i].cidx,
				wfmda_rx_group[i].didx);
			if (ret >= 0 || ret < BUF_SIZE)
				DBGLOG(HAL, INFO, "%s\n", buf);
			else
				DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
						__LINE__, ret);
		}
	}

	/* PDMA Tx/Rx descriptor & packet content */
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	for (i = 0; i < sizeof(wfmda_tx_group) /
			sizeof(struct wfdma_ring_info); i++) {
		if (!wfmda_tx_group[i].dump_ring_content)
			continue;
		DBGLOG(HAL, INFO, "Dump PDMA Tx Ring[%u]\n",
				wfmda_tx_group[i].ring_idx);
		prTxRing = &prHifInfo->TxRing[i];
		SwIdx = wfmda_tx_group[i].didx;
		kalDumpTxRing(prAdapter->prGlueInfo, prTxRing,
			      SwIdx, true);
		SwIdx = wfmda_tx_group[i].didx == 0 ?
				wfmda_tx_group[i].cnt - 1 :
				wfmda_tx_group[i].didx - 1;
		kalDumpTxRing(prAdapter->prGlueInfo, prTxRing,
			      SwIdx, true);
	}

	for (i = 0; i < sizeof(wfmda_rx_group) /
			sizeof(struct wfdma_ring_info); i++) {
		if (!wfmda_rx_group[i].dump_ring_content)
			continue;
		DBGLOG(HAL, INFO, "Dump PDMA Rx Ring[%u]\n",
				wfmda_rx_group[i].ring_idx);
		prRxRing = &prHifInfo->RxRing[i];
		SwIdx = wfmda_rx_group[i].didx;
		kalDumpRxRing(prAdapter->prGlueInfo, prRxRing,
			      SwIdx, true);
		SwIdx = wfmda_rx_group[i].didx == 0 ?
				wfmda_rx_group[i].cnt - 1 :
				wfmda_rx_group[i].didx - 1;
		kalDumpRxRing(prAdapter->prGlueInfo, prRxRing,
			      SwIdx, true);
	}

	/* PDMA Busy Status */
	HAL_MCR_RD(prAdapter, PDMA_DEBUG_BUSY_STATUS, &u4Value);
	DBGLOG(HAL, INFO, "PDMA busy status:0x%08x = 0x%08x\n",
		PDMA_DEBUG_STATUS, u4Value);
	HAL_MCR_RD(prAdapter, PDMA_DEBUG_HIF_BUSY_STATUS, &u4Value);
	DBGLOG(HAL, INFO, "CONN_HIF busy status:0x%08x = 0x%08x\n\n",
		PDMA_DEBUG_HIF_BUSY_STATUS, u4Value);

	/* PDMA Debug Flag Info */
	DBGLOG(HAL, INFO, "PDMA core dbg");
	if (buf) {
		kalMemZero(buf, BUF_SIZE);
		pos = 0;
		for (i = 0; i < 24; i++) {
			u4Value = 256 + i;
			HAL_MCR_WR(prAdapter, PDMA_DEBUG_EN, u4Value);
			HAL_MCR_RD(prAdapter, PDMA_DEBUG_STATUS, &u4Value);
			pos += kalSnprintf(buf + pos, 40,
				"Set:0x%02x, result=0x%08x%s",
				i, u4Value, i == 23 ? "\n" : "; ");
			mdelay(1);
		}
		DBGLOG(HAL, INFO, "%s", buf);
	}

	/* AXI Debug Flag */
	HAL_MCR_WR(prAdapter, AXI_DEBUG_DEBUG_EN, PDMA_AXI_DEBUG_FLAG);
	HAL_MCR_RD(prAdapter, CONN_HIF_DEBUG_STATUS, &u4Value);
	DBGLOG(HAL, INFO, "Set:0x%04x, pdma axi dbg:0x%08x",
	       PDMA_AXI_DEBUG_FLAG, u4Value);

	HAL_MCR_WR(prAdapter, AXI_DEBUG_DEBUG_EN, GALS_AXI_DEBUG_FLAG);
	HAL_MCR_RD(prAdapter, CONN_HIF_DEBUG_STATUS, &u4Value);
	DBGLOG(HAL, INFO, "Set:0x%04x, gals axi dbg:0x%08x",
	       GALS_AXI_DEBUG_FLAG, u4Value);

	HAL_MCR_WR(prAdapter, AXI_DEBUG_DEBUG_EN, MCU_AXI_DEBUG_FLAG);
	HAL_MCR_RD(prAdapter, CONN_HIF_DEBUG_STATUS, &u4Value);
	DBGLOG(HAL, INFO, "Set:0x%04x, mcu axi dbg:0x%08x",
	       MCU_AXI_DEBUG_FLAG, u4Value);

	/* Rbus Bridge Debug Flag */
	DBGLOG(HAL, INFO, "rbus dbg");
	HAL_MCR_WR(prAdapter, PDMA_DEBUG_EN, RBUS_DEBUG_FLAG);
	if (buf) {
		kalMemZero(buf, BUF_SIZE);
		pos = 0;
		for (i = 0; i < 9; i++) {
			u4Value = i << 16;
			HAL_MCR_WR(prAdapter, AXI_DEBUG_DEBUG_EN, u4Value);
			HAL_MCR_RD(prAdapter, PDMA_DEBUG_STATUS, &u4Value);
			pos += kalSnprintf(buf + pos, 40,
				"Set[19:16]:0x%02x, result = 0x%08x%s",
				i, u4Value, i == 8 ? "\n" : "; ");
		}
		DBGLOG(HAL, INFO, "%s", buf);
	}
	if (prAdapter->chip_info->prDebugOps->showHifInfo)
		prAdapter->chip_info->prDebugOps->showHifInfo(prAdapter);
	if (buf)
		kalMemFree(buf, VIR_MEM_TYPE, BUF_SIZE);

#undef BUF_SIZE
}

bool halShowHostCsrInfo(IN struct ADAPTER *prAdapter)
{
	uint32_t i = 0, u4Value = 0;
	bool fgIsDriverOwn = false;
	bool fgEnClock = false;

	DBGLOG(HAL, INFO, "Host CSR Configuration Info:\n\n");

	HAL_MCR_RD(prAdapter, HOST_CSR_BASE, &u4Value);
	DBGLOG(HAL, INFO, "Get 0x87654321: 0x%08x = 0x%08x\n",
		HOST_CSR_BASE, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_DRIVER_OWN_INFO, &u4Value);
	DBGLOG(HAL, INFO, "Driver own info: 0x%08x = 0x%08x\n",
		HOST_CSR_BASE, u4Value);
	fgIsDriverOwn = (u4Value & PCIE_LPCR_HOST_SET_OWN) == 0;

	for (i = 0; i < 5; i++) {
		HAL_MCR_RD(prAdapter, HOST_CSR_MCU_PORG_COUNT, &u4Value);
		DBGLOG(HAL, INFO,
			"MCU programming Counter info (no sync): 0x%08x = 0x%08x\n",
			HOST_CSR_MCU_PORG_COUNT, u4Value);
	}

	HAL_MCR_RD(prAdapter, HOST_CSR_RGU, &u4Value);
	DBGLOG(HAL, INFO, "RGU Info: 0x%08x = 0x%08x\n", HOST_CSR_RGU, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_HIF_BUSY_CORQ_WFSYS_ON, &u4Value);
	DBGLOG(HAL, INFO, "HIF_BUSY / CIRQ / WFSYS_ON info: 0x%08x = 0x%08x\n",
		HOST_CSR_HIF_BUSY_CORQ_WFSYS_ON, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_PINMUX_MON_FLAG, &u4Value);
	DBGLOG(HAL, INFO, "Pinmux/mon_flag info: 0x%08x = 0x%08x\n",
		HOST_CSR_PINMUX_MON_FLAG, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_MCU_PWR_STAT, &u4Value);
	DBGLOG(HAL, INFO, "Bit[5] mcu_pwr_stat: 0x%08x = 0x%08x\n",
		HOST_CSR_MCU_PWR_STAT, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_FW_OWN_SET, &u4Value);
	DBGLOG(HAL, INFO, "Bit[15] fw_own_stat: 0x%08x = 0x%08x\n",
		HOST_CSR_FW_OWN_SET, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_MCU_SW_MAILBOX_0, &u4Value);
	DBGLOG(HAL, INFO, "WF Mailbox[0]: 0x%08x = 0x%08x\n",
		HOST_CSR_MCU_SW_MAILBOX_0, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_MCU_SW_MAILBOX_1, &u4Value);
	DBGLOG(HAL, INFO, "MCU Mailbox[1]: 0x%08x = 0x%08x\n",
		HOST_CSR_MCU_SW_MAILBOX_1, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_MCU_SW_MAILBOX_2, &u4Value);
	DBGLOG(HAL, INFO, "BT Mailbox[2]: 0x%08x = 0x%08x\n",
		HOST_CSR_MCU_SW_MAILBOX_2, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_MCU_SW_MAILBOX_3, &u4Value);
	DBGLOG(HAL, INFO, "GPS Mailbox[3]: 0x%08x = 0x%08x\n",
		HOST_CSR_MCU_SW_MAILBOX_3, u4Value);

	HAL_MCR_RD(prAdapter, HOST_CSR_CONN_CFG_ON, &u4Value);
	DBGLOG(HAL, INFO, "Conn_cfg_on info: 0x%08x = 0x%08x\n",
		HOST_CSR_CONN_CFG_ON, u4Value);

#if (CFG_ENABLE_HOST_BUS_TIMEOUT == 1)
	HAL_MCR_RD(prAdapter, HOST_CSR_AP2CONN_AHB_HADDR, &u4Value);
	DBGLOG(HAL, INFO, "HOST_CSR_AP2CONN_AHB_HADDR: 0x%08x = 0x%08x\n",
		HOST_CSR_AP2CONN_AHB_HADDR, u4Value);
#endif

#if CFG_MTK_MCIF_WIFI_SUPPORT
	HAL_MCR_RD(prAdapter, HOST_CSR_CONN_HIF_ON_MD_LPCTL_ADDR, &u4Value);
	DBGLOG(HAL, INFO, "CONN_HIF_ON_MD_LPCTL_ADDR: 0x%08x = 0x%08x\n",
		HOST_CSR_CONN_HIF_ON_MD_LPCTL_ADDR, u4Value);
	HAL_MCR_RD(prAdapter, HOST_CSR_CONN_HIF_ON_MD_IRQ_STAT_ADDR, &u4Value);
	DBGLOG(HAL, INFO, "CONN_HIF_ON_MD_IRQ_STAT_ADDR: 0x%08x = 0x%08x\n",
		HOST_CSR_CONN_HIF_ON_MD_IRQ_STAT_ADDR, u4Value);
	HAL_MCR_RD(prAdapter, HOST_CSR_CONN_HIF_ON_MD_IRQ_ENA_ADDR, &u4Value);
	DBGLOG(HAL, INFO, "CONN_HIF_ON_MD_IRQ_ENA_ADDR: 0x%08x = 0x%08x\n",
		HOST_CSR_CONN_HIF_ON_MD_IRQ_ENA_ADDR, u4Value);
#endif

	HAL_MCR_WR(prAdapter, HOST_CSR_DRIVER_OWN_INFO, 0x00030000);
	kalUdelay(1);
	HAL_MCR_RD(prAdapter, HOST_CSR_DRIVER_OWN_INFO, &u4Value);
	DBGLOG(HAL, INFO, "Bit[17]/[16], Get HCLK info: 0x%08x = 0x%08x\n",
		HOST_CSR_DRIVER_OWN_INFO, u4Value);

	/* check clock is enabled */
	fgEnClock = ((u4Value & BIT(17)) != 0) && ((u4Value & BIT(16)) != 0);

	return fgIsDriverOwn && fgEnClock;
}

void haldumpPhyInfo(struct ADAPTER *prAdapter)
{
	uint32_t i = 0, value = 0;

	for (i = 0; i < 20; i++) {
		HAL_MCR_RD(prAdapter, 0x82072644, &value);
		DBGLOG(HAL, INFO, "0x82072644: 0x%08x\n", value);
		HAL_MCR_RD(prAdapter, 0x82072654, &value);
		DBGLOG(HAL, INFO, "0x82072654: 0x%08x\n", value);
		kalMdelay(1);
	}
}

