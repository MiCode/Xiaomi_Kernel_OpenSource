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
 *[File]             hif_pdma.c
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

#include "hif_pdma.h"

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"
#include "gl_kal.h"
#include "host_csr.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define RX_RESPONSE_TIMEOUT (3000)


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

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

static uint8_t halRingDataSelectByWmmIndex(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucWmmIndex)
{
	struct BUS_INFO *bus_info;
	uint16_t u2Port = TX_RING_DATA0_IDX_0;

	bus_info = prAdapter->chip_info->bus_info;
	if (bus_info->tx_ring0_data_idx != bus_info->tx_ring1_data_idx) {
		u2Port = (ucWmmIndex == 1) ?
			TX_RING_DATA1_IDX_1 : TX_RING_DATA0_IDX_0;
	}
	return u2Port;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Decide TxRingData number by MsduInfo
 *
 * @param prGlueInfo
 *
 * @param prMsduInfo
 *
 * @return TxRingData number
 */
/*----------------------------------------------------------------------------*/
uint8_t halTxRingDataSelect(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	ASSERT(prAdapter);
	return halRingDataSelectByWmmIndex(prAdapter, prMsduInfo->ucWmmQueSet);
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief check is timeout or not
 *
 * @param u4StartTime start time
 *
 * @param u4Timeout timeout value
 *
 * @return is timeout
 */
/*----------------------------------------------------------------------------*/
static inline bool halIsTimeout(uint32_t u4StartTime, uint32_t u4Timeout)
{
	uint32_t u4CurTime = kalGetTimeTick();
	uint32_t u4Time = 0;

	if (u4CurTime >= u4StartTime)
		u4Time = u4CurTime - u4StartTime;
	else
		u4Time = u4CurTime + (0xFFFFFFFF - u4StartTime);

	return u4Time > u4Timeout;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Verify the CHIP ID
 *
 * @param prAdapter      a pointer to adapter private data structure.
 *
 *
 * @retval TRUE          CHIP ID is the same as the setting compiled
 * @retval FALSE         CHIP ID is different from the setting compiled
 */
/*----------------------------------------------------------------------------*/
u_int8_t halVerifyChipID(IN struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	struct BUS_INFO *prBusInfo;
	uint32_t u4CIR = 0;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	if (prAdapter->fgIsReadRevID || !prChipInfo->should_verify_chip_id)
		return TRUE;

	HAL_MCR_RD(prAdapter, prBusInfo->top_cfg_base + TOP_HW_CONTROL, &u4CIR);

	DBGLOG(INIT, INFO, "WCIR_CHIP_ID = 0x%x, chip_id = 0x%x\n",
	       (uint32_t)(u4CIR & WCIR_CHIP_ID), prChipInfo->chip_id);

	if ((u4CIR & WCIR_CHIP_ID) != prChipInfo->chip_id)
		return FALSE;

	HAL_MCR_RD(prAdapter, prBusInfo->top_cfg_base + TOP_HW_VERSION, &u4CIR);

	prAdapter->ucRevID = (uint8_t)(u4CIR & 0xF);
	prAdapter->fgIsReadRevID = TRUE;

	return TRUE;
}

uint32_t halRxWaitResponse(IN struct ADAPTER *prAdapter, IN uint8_t ucPortIdx,
	OUT uint8_t *pucRspBuffer, IN uint32_t u4MaxRespBufferLen,
	OUT uint32_t *pu4Length)
{
	struct GLUE_INFO *prGlueInfo;
	uint32_t u4PktLen = 0, u4Value = 0, u4Time;
	u_int8_t fgStatus;
	struct mt66xx_chip_info *prChipInfo;

	DEBUGFUNC("nicRxWaitResponse");

	ASSERT(prAdapter);
	prGlueInfo = prAdapter->prGlueInfo;
	prChipInfo = prAdapter->chip_info;
	ASSERT(prGlueInfo);
	ASSERT(pucRspBuffer);

	if (prChipInfo->is_support_wfdma1)
		ucPortIdx = prChipInfo->rx_event_port;
	else {
		ASSERT(ucPortIdx < 2);
		ucPortIdx = HIF_IMG_DL_STATUS_PORT_IDX;
	}

	u4Time = kalGetTimeTick();
	u4PktLen = u4MaxRespBufferLen;

	do {
		if (wlanIsChipNoAck(prAdapter)) {
			DBGLOG(HAL, ERROR, "Chip No Ack\n");
			return WLAN_STATUS_FAILURE;
		}

		fgStatus = kalDevPortRead(
			prGlueInfo, ucPortIdx, u4PktLen,
			pucRspBuffer, HIF_RX_COALESCING_BUFFER_SIZE);
		if (fgStatus) {
			*pu4Length = u4PktLen;
			break;
		}

		if (halIsTimeout(u4Time, RX_RESPONSE_TIMEOUT)) {
			kalDevRegRead(prGlueInfo, CONN_HIF_ON_DBGCR01,
				      &u4Value);
			DBGLOG(HAL, ERROR, "CONN_HIF_ON_DBGCR01[0x%x]\n",
			       u4Value);
			return WLAN_STATUS_FAILURE;
		}

		/* Response packet is not ready */
		kalUdelay(50);
	} while (TRUE);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief enable global interrupt
 *
 * @param prAdapter pointer to the Adapter handler
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void halEnableInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = NULL;

	ASSERT(prAdapter);

	prBusInfo = prAdapter->chip_info->bus_info;

	if (prBusInfo->enableInterrupt)
		prBusInfo->enableInterrupt(prAdapter);

	prAdapter->fgIsIntEnable = TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief disable global interrupt
 *
 * @param prAdapter pointer to the Adapter handler
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void halDisableInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo;

	ASSERT(prAdapter);

	prBusInfo = prAdapter->chip_info->bus_info;

	if (prBusInfo->disableInterrupt)
		prBusInfo->disableInterrupt(prAdapter);

	prAdapter->fgIsIntEnable = FALSE;
}

static u_int8_t halDriverOwnCheckCR4(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4CurrTick;
	uint32_t ready_bits;
	u_int8_t fgStatus = TRUE;
	u_int8_t fgReady = FALSE;
	u_int8_t fgDummyReq = FALSE;
	bool fgTimeout;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;
	ready_bits = prChipInfo->sw_ready_bits;

	HAL_WIFI_FUNC_READY_CHECK(prAdapter,
				  WIFI_FUNC_DUMMY_REQ, &fgDummyReq);

	u4CurrTick = kalGetTimeTick();
	/* Wait CR4 ready */
	while (1) {
		fgTimeout = halIsTimeout(u4CurrTick,
					 LP_OWN_BACK_TOTAL_DELAY_MS);
		HAL_WIFI_FUNC_READY_CHECK(prAdapter, ready_bits, &fgReady);

		if (fgReady) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) ||
			   fgIsBusAccessFailed || fgTimeout
			   || wlanIsChipNoAck(prAdapter)) {
			DBGLOG(INIT, INFO,
			       "Skip waiting CR4 ready for next %ums\n",
			       LP_OWN_BACK_FAILED_LOG_SKIP_MS);
			fgStatus = FALSE;
#if CFG_CHIP_RESET_SUPPORT
			glSetRstReason(RST_DRV_OWN_FAIL);
			GL_RESET_TRIGGER(prAdapter,
					 RST_FLAG_CHIP_RESET);
#endif
			break;
		}
		/* Delay for CR4 to complete its operation. */
		kalUsleep_range(LP_OWN_BACK_LOOP_DELAY_MIN_US,
				LP_OWN_BACK_LOOP_DELAY_MAX_US);
	}

	/* Send dummy cmd and clear flag */
	if (fgDummyReq) {
		wlanSendDummyCmd(prAdapter, FALSE);
		HAL_CLEAR_DUMMY_REQ(prAdapter);
	}

	return fgStatus;
}

static void halDriverOwnTimeout(struct ADAPTER *prAdapter,
				uint32_t u4CurrTick, u_int8_t fgTimeout)
{
	struct mt66xx_chip_info *prChipInfo;
	struct CHIP_DBG_OPS *prChipDbgOps;

	prChipInfo = prAdapter->chip_info;
	prChipDbgOps = prChipInfo->prDebugOps;

	if ((prAdapter->u4OwnFailedCount == 0) ||
	    CHECK_FOR_TIMEOUT(u4CurrTick, prAdapter->rLastOwnFailedLogTime,
			      MSEC_TO_SYSTIME(LP_OWN_BACK_FAILED_LOG_SKIP_MS))
		) {
		DBGLOG(INIT, ERROR,
		       "LP cannot be own back, Timeout[%u](%ums), BusAccessError[%u]",
		       fgTimeout,
		       kalGetTimeTick() - u4CurrTick,
		       fgIsBusAccessFailed);
		DBGLOG(INIT, ERROR,
		       "Resetting[%u], CardRemoved[%u] NoAck[%u] Cnt[%u]\n",
		       kalIsResetting(),
		       kalIsCardRemoved(prAdapter->prGlueInfo),
		       wlanIsChipNoAck(prAdapter),
		       prAdapter->u4OwnFailedCount);
		DBGLOG(INIT, INFO,
		       "Skip LP own back failed log for next %ums\n",
		       LP_OWN_BACK_FAILED_LOG_SKIP_MS);
		if (prChipInfo->dumpwfsyscpupcr)
			prChipInfo->dumpwfsyscpupcr(prAdapter);

		prAdapter->u4OwnFailedLogCount++;
		if (prAdapter->u4OwnFailedLogCount >
		    LP_OWN_BACK_FAILED_RESET_CNT) {
			if (prChipDbgOps->showCsrInfo)
				prChipDbgOps->showCsrInfo(prAdapter);
			if (prChipInfo->dumpBusHangCr)
				prChipInfo->dumpBusHangCr(prAdapter);
#if CFG_CHIP_RESET_SUPPORT
			/* Trigger RESET */
			glSetRstReason(RST_DRV_OWN_FAIL);
#if (CFG_SUPPORT_CONNINFRA == 0)
			GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
#else
			GL_RESET_TRIGGER(prAdapter, RST_FLAG_WF_RESET);
#endif

#endif
		}
		GET_CURRENT_SYSTIME(&prAdapter->rLastOwnFailedLogTime);
	}

	prAdapter->u4OwnFailedCount++;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to process the POWER OFF procedure.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
u_int8_t halSetDriverOwn(IN struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	struct BUS_INFO *prBusInfo;
	u_int8_t fgStatus = TRUE;
	uint32_t i, u4CurrTick, u4WriteTick, u4WriteTickTemp;
	u_int8_t fgTimeout;
	u_int8_t fgResult;

	KAL_TIME_INTERVAL_DECLARATION();

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	GLUE_INC_REF_CNT(prAdapter->u4PwrCtrlBlockCnt);

	if (prAdapter->fgIsFwOwn == FALSE)
		return fgStatus;

	DBGLOG(INIT, TRACE, "DRIVER OWN Start\n");
	KAL_REC_TIME_START();

	u4WriteTick = 0;
	u4CurrTick = kalGetTimeTick();
	i = 0;

	/* PCIE/AXI need to do clear own, then could start polling status */
	HAL_LP_OWN_CLR(prAdapter, &fgResult);
	fgResult = FALSE;

	while (1) {
		if (!prBusInfo->fgCheckDriverOwnInt ||
		    test_bit(GLUE_FLAG_INT_BIT, &prAdapter->prGlueInfo->ulFlag))
			HAL_LP_OWN_RD(prAdapter, &fgResult);

		fgTimeout = ((kalGetTimeTick() - u4CurrTick) >
			     LP_OWN_BACK_TOTAL_DELAY_MS) ? TRUE : FALSE;

		if (fgResult) {
			/* Check WPDMA FW own interrupt status and clear */
			if (prBusInfo->fgCheckDriverOwnInt)
				HAL_MCR_WR(prAdapter,
					prBusInfo->fw_own_clear_addr,
					prBusInfo->fw_own_clear_bit);
			prAdapter->fgIsFwOwn = FALSE;
			prAdapter->u4OwnFailedCount = 0;
			prAdapter->u4OwnFailedLogCount = 0;
			break;
		} else if (wlanIsChipNoAck(prAdapter)) {
			DBGLOG(INIT, INFO,
			"Driver own return due to chip reset and chip no response.\n");
			fgStatus = FALSE;
			break;
		} else if ((i > LP_OWN_BACK_FAILED_RETRY_CNT) &&
			   (kalIsCardRemoved(prAdapter->prGlueInfo) ||
			    fgIsBusAccessFailed || fgTimeout)) {
			halDriverOwnTimeout(prAdapter, u4CurrTick, fgTimeout);
			fgStatus = FALSE;
			break;
		}

		u4WriteTickTemp = kalGetTimeTick();
		if ((i == 0) || TIME_AFTER(u4WriteTickTemp,
			(u4WriteTick + LP_OWN_REQ_CLR_INTERVAL_MS))) {
			/* Driver get LP ownership per 200 ms,
			 * to avoid iteration time not accurate
			 */
			HAL_LP_OWN_CLR(prAdapter, &fgResult);
			u4WriteTick = u4WriteTickTemp;
		}

		/* Delay for LP engine to complete its operation. */
		kalUsleep_range(LP_OWN_BACK_LOOP_DELAY_MIN_US,
				LP_OWN_BACK_LOOP_DELAY_MAX_US);
		i++;
	}

	/* For Low power Test */
	/* 1. Driver need to polling until CR4 ready,
	 *    then could do normal Tx/Rx
	 * 2. After CR4 ready, send a dummy command to change data path
	 *    to store-forward mode
	 */
	if (prAdapter->fgIsFwDownloaded && prChipInfo->is_support_cr4)
		fgStatus &= halDriverOwnCheckCR4(prAdapter);

	if (fgStatus) {
		if (prAdapter->fgIsFwDownloaded) {
			/*WFDMA re-init flow after chip deep sleep*/
			if (prChipInfo->asicWfdmaReInit)
				prChipInfo->asicWfdmaReInit(prAdapter);
		}
		/* Check consys enter sleep mode DummyReg(0x0F) */
		if (prBusInfo->checkDummyReg)
			prBusInfo->checkDummyReg(prAdapter->prGlueInfo);
	}

	KAL_REC_TIME_END();
	DBGLOG(INIT, INFO,
		"DRIVER OWN Done[%lu us]\n", KAL_GET_TIME_INTERVAL());

	return fgStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to process the POWER ON procedure.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void halSetFWOwn(IN struct ADAPTER *prAdapter, IN u_int8_t fgEnableGlobalInt)
{
	struct BUS_INFO *prBusInfo;
	u_int8_t fgResult;

	ASSERT(prAdapter);

	prBusInfo = prAdapter->chip_info->bus_info;

	/* Decrease Block to Enter Low Power Semaphore count */
	GLUE_DEC_REF_CNT(prAdapter->u4PwrCtrlBlockCnt);
	if (!(prAdapter->fgWiFiInSleepyState &&
		(prAdapter->u4PwrCtrlBlockCnt == 0)))
		return;

	if (prAdapter->fgIsFwOwn == TRUE)
		return;

	if (nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		DBGLOG(INIT, STATE, "Skip FW OWN due to pending INT\n");
		/* pending interrupts */
		return;
	}

	if (fgEnableGlobalInt) {
		prAdapter->fgIsIntEnableWithLPOwnSet = TRUE;
	} else {
		/* Write sleep mode magic num to dummy reg */
		if (prBusInfo->setDummyReg)
			prBusInfo->setDummyReg(prAdapter->prGlueInfo);

		HAL_LP_OWN_SET(prAdapter, &fgResult);

		prAdapter->fgIsFwOwn = TRUE;

		DBGLOG(INIT, INFO, "FW OWN:%u\n", fgResult);
	}
}

void halWakeUpWiFi(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo;

	ASSERT(prAdapter);

	prBusInfo = prAdapter->chip_info->bus_info;
	if (prBusInfo->wakeUpWiFi)
		prBusInfo->wakeUpWiFi(prAdapter);
}

void halTxCancelSendingCmd(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo)
{
}

u_int8_t halTxIsDataBufEnough(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct RTMP_TX_RING *prTxRing;
	uint16_t u2Port;

	u2Port = halTxRingDataSelect(prAdapter, prMsduInfo);
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prTxRing = &prHifInfo->TxRing[u2Port];

	if ((prHifInfo->u4TxDataQLen < halGetMsduTokenFreeCnt(prAdapter)) &&
	    (prTxRing->u4UsedCnt + prHifInfo->u4TxDataQLen + 1 < TX_RING_SIZE))
		return TRUE;

	DBGLOG(HAL, TRACE,
		"Low Tx Data Resource Tok[%u] Ring%d[%u] List[%u]\n",
		halGetMsduTokenFreeCnt(prAdapter),
		u2Port,
		(TX_RING_SIZE - prTxRing->u4UsedCnt),
		prHifInfo->u4TxDataQLen);
	kalTraceEvent("Low T[%u]Ring%d[%u]L[%u] id=0x%04x sn=%d",
		halGetMsduTokenFreeCnt(prAdapter),
		u2Port,
		(TX_RING_SIZE - prTxRing->u4UsedCnt),
		prHifInfo->u4TxDataQLen,
		GLUE_GET_PKT_IP_ID(prMsduInfo->prPacket),
		GLUE_GET_PKT_SEQ_NO(prMsduInfo->prPacket));
	return FALSE;
}

static void halDefaultProcessTxInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	union WPDMA_INT_STA_STRUCT rIntrStatus;
	bool fgIsSetHifTxEvent = false;

	rIntrStatus = (union WPDMA_INT_STA_STRUCT)prHifInfo->u4IntStatus;

	if (rIntrStatus.field.tx_done & BIT(prBusInfo->tx_ring_fwdl_idx))
		halWpdmaProcessCmdDmaDone(prAdapter->prGlueInfo,
			TX_RING_FWDL_IDX_3);

	if (rIntrStatus.field.tx_done & BIT(prBusInfo->tx_ring_cmd_idx))
		halWpdmaProcessCmdDmaDone(prAdapter->prGlueInfo,
			TX_RING_CMD_IDX_2);

	if (rIntrStatus.field.tx_done & BIT(prBusInfo->tx_ring0_data_idx)) {
		halWpdmaProcessDataDmaDone(prAdapter->prGlueInfo,
			TX_RING_DATA0_IDX_0);
		fgIsSetHifTxEvent = true;
	}

	if (prBusInfo->tx_ring0_data_idx != prBusInfo->tx_ring1_data_idx &&
		rIntrStatus.field.tx_done & BIT(prBusInfo->tx_ring1_data_idx)) {
		halWpdmaProcessDataDmaDone(prAdapter->prGlueInfo,
			TX_RING_DATA1_IDX_1);
		fgIsSetHifTxEvent = true;
	}

	if (fgIsSetHifTxEvent)
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
}


void halProcessTxInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	if (prBusInfo->processTxInterrupt)
		prBusInfo->processTxInterrupt(prAdapter);
	else
		halDefaultProcessTxInterrupt(prAdapter);
}


void halInitMsduTokenInfo(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct MSDU_TOKEN_INFO *prTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4Idx;
	uint32_t u4TxHeadRoomSize;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prTokenInfo = &prHifInfo->rTokenInfo;
	prChipInfo = prAdapter->chip_info;

	prTokenInfo->u4UsedCnt = 0;
	u4TxHeadRoomSize = NIC_TX_DESC_AND_PADDING_LENGTH +
		prChipInfo->txd_append_size;

	for (u4Idx = 0; u4Idx < HIF_TX_MSDU_TOKEN_NUM; u4Idx++) {
		prToken = &prTokenInfo->arToken[u4Idx];
		prToken->fgInUsed = FALSE;
		prToken->prMsduInfo = NULL;

#if HIF_TX_PREALLOC_DATA_BUFFER
		prToken->u4DmaLength = NIC_TX_MAX_SIZE_PER_FRAME +
			u4TxHeadRoomSize;
		if (prMemOps->allocTxDataBuf)
			prMemOps->allocTxDataBuf(prToken, u4Idx);

		if (prToken->prPacket) {
			/* DBGLOG(HAL, TRACE,
				"Msdu Entry[0x%p] Tok[%u] Buf[0x%p] len[%u]\n",
				prToken, u4Idx, prToken->prPacket,
				prToken->u4DmaLength);
			*/
		} else {
			prTokenInfo->u4UsedCnt++;
			DBGLOG(HAL, WARN,
				"Msdu Token Memory alloc failed[%u]\n",
				u4Idx);
			continue;
		}
#else
		prToken->prPacket = NULL;
		prToken->u4DmaLength = 0;
		prToken->rDmaAddr = 0;
#endif
		prToken->rPktDmaAddr = 0;
		prToken->u4PktDmaLength = 0;
		prToken->u4Token = u4Idx;
		prToken->u4CpuIdx = TX_RING_SIZE;

		prTokenInfo->aprTokenStack[u4Idx] = prToken;
	}

	spin_lock_init(&prTokenInfo->rTokenLock);

	DBGLOG(HAL, INFO, "Msdu Token Init: Tot[%u] Used[%u]\n",
		HIF_TX_MSDU_TOKEN_NUM, prTokenInfo->u4UsedCnt);
}

void halUninitMsduTokenInfo(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct MSDU_TOKEN_INFO *prTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	uint32_t u4Idx;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prTokenInfo = &prHifInfo->rTokenInfo;

	for (u4Idx = 0; u4Idx < HIF_TX_MSDU_TOKEN_NUM; u4Idx++) {
		prToken = &prTokenInfo->arToken[u4Idx];

		if (prToken->fgInUsed) {
			if (prMemOps->unmapTxBuf) {
				prMemOps->unmapTxBuf(
					prHifInfo, prToken->rPktDmaAddr,
					prToken->u4PktDmaLength);
				prMemOps->unmapTxBuf(
					prHifInfo, prToken->rDmaAddr,
					prToken->u4DmaLength);
			}

			log_dbg(HAL, TRACE, "Clear pending Tok[%u] Msdu[0x%p] Free[%u]\n",
				prToken->u4Token, prToken->prMsduInfo,
				halGetMsduTokenFreeCnt(prAdapter));

#if !HIF_TX_PREALLOC_DATA_BUFFER
			nicTxFreePacket(prAdapter, prToken->prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prToken->prMsduInfo);
#endif
		}

#if HIF_TX_PREALLOC_DATA_BUFFER
		if (prMemOps->freeBuf)
			prMemOps->freeBuf(prToken->prPacket,
					  prToken->u4DmaLength);
		prToken->prPacket = NULL;
#endif
	}

	prTokenInfo->u4UsedCnt = 0;

	DBGLOG(HAL, INFO, "Msdu Token Uninit: Tot[%u] Used[%u]\n",
		HIF_TX_MSDU_TOKEN_NUM, prTokenInfo->u4UsedCnt);
}

uint32_t halGetMsduTokenFreeCnt(IN struct ADAPTER *prAdapter)
{
	struct PERF_MONITOR *prPerMonitor;
	struct MSDU_TOKEN_INFO *prTokenInfo =
		&prAdapter->prGlueInfo->rHifInfo.rTokenInfo;
	prPerMonitor = &prAdapter->rPerMonitor;
	prPerMonitor->u4UsedCnt = prTokenInfo->u4UsedCnt;

	return HIF_TX_MSDU_TOKEN_NUM - prTokenInfo->u4UsedCnt;
}

struct MSDU_TOKEN_ENTRY *halGetMsduTokenEntry(IN struct ADAPTER *prAdapter,
	uint32_t u4TokenNum)
{
	struct MSDU_TOKEN_INFO *prTokenInfo =
		&prAdapter->prGlueInfo->rHifInfo.rTokenInfo;

	return &prTokenInfo->arToken[u4TokenNum];
}

struct MSDU_TOKEN_ENTRY *halAcquireMsduToken(IN struct ADAPTER *prAdapter)
{
	struct MSDU_TOKEN_INFO *prTokenInfo =
		&prAdapter->prGlueInfo->rHifInfo.rTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	unsigned long flags = 0;

	if (!halGetMsduTokenFreeCnt(prAdapter)) {
		DBGLOG(HAL, INFO, "No more free MSDU token, Used[%u]\n",
			prTokenInfo->u4UsedCnt);
		return NULL;
	}

	spin_lock_irqsave(&prTokenInfo->rTokenLock, flags);

	prToken = prTokenInfo->aprTokenStack[prTokenInfo->u4UsedCnt];
	do_gettimeofday(&prToken->rTs);
	prToken->fgInUsed = TRUE;
	prTokenInfo->u4UsedCnt++;

	spin_unlock_irqrestore(&prTokenInfo->rTokenLock, flags);

	DBGLOG_LIMITED(HAL, TRACE,
		       "Acquire Entry[0x%p] Tok[%u] Buf[%p] Len[%u]\n",
		       prToken, prToken->u4Token,
		       prToken->prPacket, prToken->u4DmaLength);

	return prToken;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Reset all msdu token. Return used msdu & re-init token.
 *
 * @param prAdapter      a pointer to adapter private data structure.
 *
 */
/*----------------------------------------------------------------------------*/

static void halResetMsduToken(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct MSDU_TOKEN_INFO *prTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	uint32_t u4Idx = 0;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prTokenInfo = &prHifInfo->rTokenInfo;

	for (u4Idx = 0; u4Idx < HIF_TX_MSDU_TOKEN_NUM; u4Idx++) {
		prToken = &prTokenInfo->arToken[u4Idx];
		if (prToken->fgInUsed) {
			if (prMemOps->unmapTxBuf) {
				prMemOps->unmapTxBuf(
					prHifInfo, prToken->rPktDmaAddr,
					prToken->u4PktDmaLength);
				prMemOps->unmapTxBuf(
					prHifInfo, prToken->rDmaAddr,
					prToken->u4DmaLength);
				prToken->rPktDmaAddr = 0;
				prToken->u4PktDmaLength = 0;
				prToken->rDmaAddr = 0;
			}

#if !HIF_TX_PREALLOC_DATA_BUFFER
			nicTxFreePacket(prAdapter, prToken->prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prToken->prMsduInfo);
#endif
		}

		prToken->fgInUsed = FALSE;
		prTokenInfo->aprTokenStack[u4Idx] = prToken;
	}
	prTokenInfo->u4UsedCnt = 0;
}

void halReturnMsduToken(IN struct ADAPTER *prAdapter, uint32_t u4TokenNum)
{
	struct MSDU_TOKEN_INFO *prTokenInfo =
		&prAdapter->prGlueInfo->rHifInfo.rTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	unsigned long flags = 0;

	if (!prTokenInfo->u4UsedCnt) {
		DBGLOG(HAL, WARN, "MSDU token is full, Used[%u]\n",
			prTokenInfo->u4UsedCnt);
		return;
	}

	prToken = &prTokenInfo->arToken[u4TokenNum];
	if (!prToken->fgInUsed) {
		DBGLOG(HAL, ERROR, "Return unuse token[%u]\n", u4TokenNum);
		return;
	}

	spin_lock_irqsave(&prTokenInfo->rTokenLock, flags);

	prToken->fgInUsed = FALSE;
	prTokenInfo->u4UsedCnt--;
	prTokenInfo->aprTokenStack[prTokenInfo->u4UsedCnt] = prToken;

	spin_unlock_irqrestore(&prTokenInfo->rTokenLock, flags);
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief Return all timeout msdu token.
 *
 * @param prAdapter      a pointer to adapter private data structure.
 *
 */
/*----------------------------------------------------------------------------*/
void halReturnTimeoutMsduToken(struct ADAPTER *prAdapter)
{
	struct MSDU_TOKEN_INFO *prTokenInfo;
	struct MSDU_TOKEN_ENTRY *prToken;
	struct timeval rNowTs, rTime;
	struct timeval rTimeout;
	uint32_t u4Idx = 0;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);

	prTokenInfo = &prAdapter->prGlueInfo->rHifInfo.rTokenInfo;

	rTimeout.tv_sec = HIF_MSDU_REPORT_RETURN_TIMEOUT;
	rTimeout.tv_usec = 0;
	do_gettimeofday(&rNowTs);

	for (u4Idx = 0; u4Idx < HIF_TX_MSDU_TOKEN_NUM; u4Idx++) {
		prToken = &prTokenInfo->arToken[u4Idx];
		if (!prToken->fgInUsed)
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

		/* Return token to free stack */
		if (halTimeCompare(&rTime, &rTimeout) >= 0) {
			DBGLOG(HAL, INFO,
			       "Free TokenId[%u] timeout[sec:%ld, usec:%ld]\n",
			       u4Idx, rTime.tv_sec, rTime.tv_usec);
			halReturnMsduToken(prAdapter, u4Idx);
		}
	}
}

bool halHifSwInfoInit(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct BUS_INFO *prBusInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prBusInfo = prAdapter->chip_info->bus_info;
	if (prBusInfo->DmaShdlInit)
		prBusInfo->DmaShdlInit(prAdapter);

	if (!halWpdmaAllocRing(prAdapter->prGlueInfo, true))
		return false;

	halWpdmaInitRing(prAdapter->prGlueInfo, true);
	halInitMsduTokenInfo(prAdapter);
	/* Initialize wfdma reInit handshake parameters */
	prChipInfo = prAdapter->chip_info;
	if ((prChipInfo->asicWfdmaReInit)
	    && (prChipInfo->asicWfdmaReInit_handshakeInit))
		prChipInfo->asicWfdmaReInit_handshakeInit(prAdapter);

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	prAdapter->ucSerState = SER_IDLE_DONE;
	prHifInfo->rErrRecoveryCtl.eErrRecovState = ERR_RECOV_STOP_IDLE;
	prHifInfo->rErrRecoveryCtl.u4Status = 0;

#if (KERNEL_VERSION(4, 15, 0) <= CFG80211_VERSION_CODE)
	timer_setup(&prHifInfo->rSerTimer, halHwRecoveryTimeout, 0);
	prHifInfo->rSerTimerData = (unsigned long)prAdapter->prGlueInfo;
#else
	init_timer(&prHifInfo->rSerTimer);
	prHifInfo->rSerTimer.function = halHwRecoveryTimeout;
	prHifInfo->rSerTimer.data = (unsigned long)prAdapter->prGlueInfo;
#endif
	prHifInfo->rSerTimer.expires =
		jiffies + HIF_SER_TIMEOUT * HZ / MSEC_PER_SEC;

	INIT_LIST_HEAD(&prHifInfo->rTxCmdQ);
	INIT_LIST_HEAD(&prHifInfo->rTxDataQ);
	prHifInfo->u4TxDataQLen = 0;
#endif

#if (CFG_ENABLE_HOST_BUS_TIMEOUT == 1)
	DBGLOG(HAL, INFO, "Enable Host CSR timeout mechanism.\n");
	HAL_MCR_WR(prAdapter, HOST_CSR_BUS_TIMOUT_CTRL_ADDR, 0x80EFFFFF);
#endif

	prHifInfo->fgIsPowerOff = false;

	return true;
}

void halHifSwInfoUnInit(IN struct GLUE_INFO *prGlueInfo)
{
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	struct list_head *prCur, *prNext;
	struct TX_CMD_REQ *prTxCmdReq;
	struct TX_DATA_REQ *prTxDataReq;

	del_timer_sync(&prHifInfo->rSerTimer);

	halUninitMsduTokenInfo(prGlueInfo->prAdapter);
	halWpdmaFreeRing(prGlueInfo);

	list_for_each_safe(prCur, prNext, &prHifInfo->rTxCmdQ) {
		prTxCmdReq = list_entry(prCur, struct TX_CMD_REQ, list);
		list_del(prCur);
		kfree(prTxCmdReq);
	}

	list_for_each_safe(prCur, prNext, &prHifInfo->rTxDataQ) {
		prTxDataReq = list_entry(prCur, struct TX_DATA_REQ, list);
		list_del(prCur);
		prHifInfo->u4TxDataQLen--;
	}
#endif
}

void halRxProcessMsduReport(IN struct ADAPTER *prAdapter,
			    IN OUT struct SW_RFB *prSwRfb)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_DMACB *prTxCell;
	struct RTMP_TX_RING *prTxRing;
	struct HW_MAC_MSDU_REPORT *prMsduReport;
	struct MSDU_TOKEN_ENTRY *prTokenEntry;
#if !HIF_TX_PREALLOC_DATA_BUFFER
	struct MSDU_INFO *prMsduInfo;
#endif
	struct QUE rFreeQueue;
	struct QUE *prFreeQueue;
	uint16_t u2TokenCnt, u2TotalTokenCnt;
	uint32_t u4Idx, u4Token;
	uint8_t ucVer;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;

	prFreeQueue = &rFreeQueue;
	QUEUE_INITIALIZE(prFreeQueue);

	prMsduReport = (struct HW_MAC_MSDU_REPORT *)prSwRfb->pucRecvBuff;
	ucVer = prMsduReport->DW1.field.u4Ver;
	if (ucVer == TFD_EVT_VER_3)
		u2TotalTokenCnt = prMsduReport->DW0.field_v3.u2MsduCount;
	else
		u2TotalTokenCnt = prMsduReport->DW0.field.u2MsduCount;

	u4Idx = u2TokenCnt = 0;
	while (u2TokenCnt < u2TotalTokenCnt) {
		/* Format version of this tx done event.
		 *	0: MT7615
		 *	1: MT7622, CONNAC (X18/P18/MT7663)
		 *	2: MT7915 E1/Petrus
		 *      3: MT7915 E2/Buzzard
		 */
		if (ucVer == TFD_EVT_VER_0)
			u4Token = prMsduReport->au4MsduToken[u4Idx >> 1].
				rFormatV0.u2MsduID[u4Idx & 1];
		else if (ucVer == TFD_EVT_VER_1)
			u4Token = prMsduReport->au4MsduToken[u4Idx].
				rFormatV1.u2MsduID;
		else if (ucVer == TFD_EVT_VER_2)
			u4Token = prMsduReport->au4MsduToken[u4Idx].
				rFormatV2.u2MsduID;
		else {
			if (!prMsduReport->au4MsduToken[u4Idx].
				rFormatV3.rP0.u4Pair)
				u4Token = prMsduReport->au4MsduToken[u4Idx].
						rFormatV3.rP0.u4MsduID;
			else {
				u4Idx++;
				continue;
			}
		}
		u4Idx++;
		u2TokenCnt++;

		if (u4Token >= HIF_TX_MSDU_TOKEN_NUM) {
			DBGLOG(HAL, ERROR, "Error MSDU report[%u]\n", u4Token);
			DBGLOG_MEM32(HAL, ERROR, prMsduReport, 64);
			prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
			halPrintHifDbgInfo(prAdapter);
			return;
		}

		prTokenEntry = halGetMsduTokenEntry(prAdapter, u4Token);

#if HIF_TX_PREALLOC_DATA_BUFFER
		DBGLOG_LIMITED(HAL, TRACE,
			       "MsduRpt: Cnt[%u] Tok[%u] Free[%u]\n",
			       u2TokenCnt, u4Token,
			       halGetMsduTokenFreeCnt(prAdapter));
#else
		prMsduInfo = prTokenEntry->prMsduInfo;
		prMsduInfo->prToken = NULL;
		if (!prMsduInfo->pfTxDoneHandler)
			QUEUE_INSERT_TAIL(prFreeQueue,
				(struct QUE_ENTRY *) prMsduInfo);

		DBGLOG_LIMITED(HAL, TRACE,
			       "MsduRpt: Cnt[%u] Tok[%u] Msdu[0x%p] TxDone[%u] Free[%u]\n",
			       u2TokenCnt, u4Token, prMsduInfo,
			       (prMsduInfo->pfTxDoneHandler ? TRUE : FALSE),
			       halGetMsduTokenFreeCnt(prAdapter));
#endif
		if (prMemOps->unmapTxBuf) {
			prMemOps->unmapTxBuf(prHifInfo,
					     prTokenEntry->rPktDmaAddr,
					     prTokenEntry->u4PktDmaLength);
			prMemOps->unmapTxBuf(prHifInfo,
					     prTokenEntry->rDmaAddr,
					     prTokenEntry->u4DmaLength);
		}

		if (prTokenEntry->u4CpuIdx < TX_RING_SIZE) {
			prTxRing = &prHifInfo->TxRing[prTokenEntry->u2Port];
			prTxCell = &prTxRing->Cell[prTokenEntry->u4CpuIdx];
			prTxCell->prToken = NULL;
		}
		prTokenEntry->u4CpuIdx = TX_RING_SIZE;
		halReturnMsduToken(prAdapter, u4Token);
		GLUE_INC_REF_CNT(prAdapter->rHifStats.u4DataMsduRptCount);
	}

#if !HIF_TX_PREALLOC_DATA_BUFFER
	nicTxMsduDoneCb(prAdapter->prGlueInfo, prFreeQueue);
#endif

	/* Indicate Service Thread */
	if (wlanGetTxPendingFrameCount(prAdapter) > 0)
		kalSetEvent(prAdapter->prGlueInfo);

	kalSetTxEvent2Hif(prAdapter->prGlueInfo);
}

void halTxUpdateCutThroughDesc(struct GLUE_INFO *prGlueInfo,
			       struct MSDU_INFO *prMsduInfo,
			       struct MSDU_TOKEN_ENTRY *prFillToken,
			       struct MSDU_TOKEN_ENTRY *prDataToken,
			       uint32_t u4Idx, bool fgIsLast)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct mt66xx_chip_info *prChipInfo;
	struct TX_DESC_OPS_T *prTxDescOps;
	uint8_t *pucBufferTxD;
	uint32_t u4TxHeadRoomSize;
	phys_addr_t rPhyAddr = 0;

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	prTxDescOps = prChipInfo->prTxDescOps;
	pucBufferTxD = prDataToken->prPacket;
	u4TxHeadRoomSize = NIC_TX_DESC_AND_PADDING_LENGTH +
		prChipInfo->txd_append_size;

	if (prMemOps->mapTxBuf) {
		rPhyAddr = prMemOps->mapTxBuf(
			prHifInfo, pucBufferTxD, u4TxHeadRoomSize,
			prMsduInfo->u2FrameLength);
	} else {
		if (prDataToken->rDmaAddr)
			rPhyAddr = prDataToken->rDmaAddr + u4TxHeadRoomSize;
	}

	if (!rPhyAddr) {
		DBGLOG(HAL, ERROR, "Get address error!\n");
		return;
	}

	if (prTxDescOps->fillHifAppend)
		prTxDescOps->fillHifAppend(prGlueInfo->prAdapter,
			prMsduInfo, prDataToken->u4Token,
			rPhyAddr, u4Idx, fgIsLast, prFillToken->prPacket);

	prDataToken->rPktDmaAddr = rPhyAddr;
	prDataToken->u4PktDmaLength = prMsduInfo->u2FrameLength;
}

uint32_t halTxGetPageCount(IN struct ADAPTER *prAdapter,
	IN uint32_t u4FrameLength, IN u_int8_t fgIncludeDesc)
{
	return 1;
}

uint32_t halTxPollingResource(IN struct ADAPTER *prAdapter, IN uint8_t ucTC)
{
	nicTxReleaseResource(prAdapter, ucTC, 1, TRUE, FALSE);
	return WLAN_STATUS_SUCCESS;
}

void halSerHifReset(IN struct ADAPTER *prAdapter)
{
}

void halRxReceiveRFBs(IN struct ADAPTER *prAdapter, uint32_t u4Port,
	uint8_t fgRxData)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	uint8_t *pucBuf = NULL;
	void *prRxStatus;
	u_int8_t fgStatus;
	uint32_t u4RxCnt;
	struct RX_DESC_OPS_T *prRxDescOps;
	struct RTMP_RX_RING *prRxRing;
	struct GL_HIF_INFO *prHifInfo;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxPCIeReceiveRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);
	prRxDescOps = prAdapter->chip_info->prRxDescOps;
	ASSERT(prRxDescOps->nic_rxd_get_rx_byte_count);
	ASSERT(prRxDescOps->nic_rxd_get_pkt_type);
	ASSERT(prRxDescOps->nic_rxd_get_wlan_idx);
#if DBG
	ASSERT(prRxDescOps->nic_rxd_get_sec_mode);
#endif /* DBG */

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prRxRing = &prHifInfo->RxRing[u4Port];

	u4RxCnt = halWpdmaGetRxDmaDoneCnt(prAdapter->prGlueInfo, u4Port);

	DBGLOG(RX, LOUD, "halRxReceiveRFBs: u4RxCnt:%d\n", u4RxCnt);

	while (u4RxCnt--) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList,
			prSwRfb, struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		if (!prSwRfb) {
			DBGLOG_LIMITED(RX, WARN, "No More RFB for P[%u]\n",
					u4Port);
			prAdapter->u4NoMoreRfb |= BIT(u4Port);
			break;
		}

		/* unset no more rfb port bit */
		prAdapter->u4NoMoreRfb &= ~BIT(u4Port);

		if (fgRxData) {
			fgStatus = kalDevReadData(prAdapter->prGlueInfo,
				u4Port, prSwRfb);
		} else {
			pucBuf = prSwRfb->pucRecvBuff;
			ASSERT(pucBuf);

			fgStatus = kalDevPortRead(prAdapter->prGlueInfo,
				u4Port, CFG_RX_MAX_PKT_SIZE,
				pucBuf, CFG_RX_MAX_PKT_SIZE);
		}
		if (!fgStatus) {
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
			QUEUE_INSERT_TAIL(&prRxCtrl->rFreeSwRfbList,
				&prSwRfb->rQueEntry);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

			continue;
		}

		prRxStatus = prSwRfb->prRxStatus;
		ASSERT(prRxStatus);

		prSwRfb->ucPacketType =
			prRxDescOps->nic_rxd_get_pkt_type(prRxStatus);
#if DBG
		DBGLOG_LIMITED(RX, LOUD, "ucPacketType = %u, ucSecMode = %u\n",
				  prSwRfb->ucPacketType,
				  prRxDescOps->nic_rxd_get_sec_mode(
					prRxStatus));
#endif /* DBG */

		if (prSwRfb->ucPacketType == RX_PKT_TYPE_MSDU_REPORT) {
			nicRxProcessMsduReport(prAdapter, prSwRfb);

			continue;
		}

		GLUE_RX_SET_PKT_INT_TIME(prSwRfb->pvPacket,
					 prAdapter->prGlueInfo->u8HifIntTime);
		GLUE_RX_SET_PKT_RX_TIME(prSwRfb->pvPacket, sched_clock());

		kalTraceEvent("Recv id=0x%04x sn=%d",
			GLUE_GET_PKT_IP_ID(prSwRfb->pvPacket),
			GLUE_GET_PKT_SEQ_NO(prSwRfb->pvPacket));

		prSwRfb->ucStaRecIdx =
			secGetStaIdxByWlanIdx(
				prAdapter,
				prRxDescOps->nic_rxd_get_wlan_idx(prRxStatus));

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList,
			&prSwRfb->rQueEntry);
		RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
	}
	prRxRing->u4PendingCnt = halWpdmaGetRxDmaDoneCnt(prAdapter->prGlueInfo,
			u4Port);
}

static void halDefaultProcessRxInterrupt(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	union WPDMA_INT_STA_STRUCT rIntrStatus;

	rIntrStatus = (union WPDMA_INT_STA_STRUCT)prHifInfo->u4IntStatus;
	prAdapter->prGlueInfo->u8HifIntTime = sched_clock();

	if (rIntrStatus.field.rx_done_1 ||
	    (prAdapter->u4NoMoreRfb & BIT(RX_RING_EVT_IDX_1)))
		halRxReceiveRFBs(prAdapter, RX_RING_EVT_IDX_1, FALSE);

	if (rIntrStatus.field.rx_done_0 ||
	    (prAdapter->u4NoMoreRfb & BIT(RX_RING_DATA_IDX_0)))
		halRxReceiveRFBs(prAdapter, RX_RING_DATA_IDX_0, TRUE);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Read frames from the data port for PCIE
 *        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void halProcessRxInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	if (prBusInfo->processRxInterrupt)
		prBusInfo->processRxInterrupt(prAdapter);
	else
		halDefaultProcessRxInterrupt(prAdapter);
}

static int32_t halWpdmaFreeRingDesc(struct GLUE_INFO *prGlueInfo,
				    struct RTMP_DMABUF *prDescRing)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;

	if (prMemOps->freeDesc)
		prMemOps->freeDesc(prHifInfo, prDescRing);

	return TRUE;
}

bool halWpdmaAllocTxRing(struct GLUE_INFO *prGlueInfo, uint32_t u4Num,
			 uint32_t u4Size, uint32_t u4DescSize, bool fgAllocMem)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_TX_RING *pTxRing;
	struct RTMP_DMABUF *prTxDesc;
	struct RTMP_DMACB *prTxCell;
	phys_addr_t RingBasePa;
	void *RingBaseVa;
	uint32_t u4Idx;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prTxDesc = &prHifInfo->TxDescRing[u4Num];

	/* Don't re-alloc memory when second time call alloc ring */
	prTxDesc->AllocSize = u4Size * u4DescSize;
	if (fgAllocMem && prMemOps->allocTxDesc)
		prMemOps->allocTxDesc(prHifInfo, prTxDesc, u4Num);

	if (prTxDesc->AllocVa == NULL) {
		DBGLOG(HAL, ERROR, "TxDescRing[%d] allocation failed\n", u4Num);
		return false;
	}

	DBGLOG(HAL, TRACE, "TxDescRing[%p]: total %lu bytes allocated\n",
	       prTxDesc->AllocVa, prTxDesc->AllocSize);

	/* Save PA & VA for further operation */
	RingBasePa = prTxDesc->AllocPa;
	RingBaseVa = prTxDesc->AllocVa;

	/*
	 * Initialize Tx Ring Descriptor and associated buffer memory
	 */
	pTxRing = &prHifInfo->TxRing[u4Num];
	for (u4Idx = 0; u4Idx < u4Size; u4Idx++) {
		prTxCell = &pTxRing->Cell[u4Idx];
		prTxCell->pPacket = NULL;
		prTxCell->pBuffer = NULL;

		/* Init Tx Ring Size, Va, Pa variables */
		prTxCell->AllocSize = u4DescSize;
		prTxCell->AllocVa = RingBaseVa;
		prTxCell->AllocPa = RingBasePa;
		prTxCell->prToken = NULL;

		RingBasePa += u4DescSize;
		RingBaseVa += u4DescSize;

		if (fgAllocMem && prMemOps->allocTxCmdBuf) {
			bool ret;

			ret = prMemOps->allocTxCmdBuf(&prTxCell->DmaBuf,
						u4Num, u4Idx);
			if (ret == false) {
				DBGLOG(HAL, ERROR,
					"TxRing[%u] TxCmd[%u] alloc failed\n",
					u4Num, u4Idx);
				return false;
			}
		}
	}

	DBGLOG(HAL, TRACE, "TxRing[%d]: total %d entry allocated\n",
	       u4Num, u4Idx);

	return true;
}

bool halWpdmaAllocRxRing(struct GLUE_INFO *prGlueInfo, uint32_t u4Num,
			 uint32_t u4Size, uint32_t u4DescSize,
			 uint32_t u4BufSize, bool fgAllocMem)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_RX_RING *pRxRing;
	struct RTMP_DMABUF *prRxDesc;
	struct RTMP_DMABUF *pDmaBuf;
	struct RTMP_DMACB *prRxCell;
	struct RXD_STRUCT *pRxD;
	phys_addr_t RingBasePa;
	void *RingBaseVa;
	uint32_t u4Idx;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prRxDesc = &prHifInfo->RxDescRing[u4Num];

	/* Don't re-alloc memory when second time call alloc ring */
	prRxDesc->AllocSize = u4Size * u4DescSize;
	if (fgAllocMem && prMemOps->allocRxDesc)
		prMemOps->allocRxDesc(prHifInfo, prRxDesc, u4Num);

	if (prRxDesc->AllocVa == NULL) {
		DBGLOG(HAL, ERROR, "RxDescRing allocation failed!!\n");
		return false;
	}

	DBGLOG(HAL, TRACE, "RxDescRing[%p]: total %lu bytes allocated\n",
		prRxDesc->AllocVa, prRxDesc->AllocSize);

	/* Initialize Rx Ring and associated buffer memory */
	RingBasePa = prRxDesc->AllocPa;
	RingBaseVa = prRxDesc->AllocVa;

	pRxRing = &prHifInfo->RxRing[u4Num];
	pRxRing->u4BufSize = u4BufSize;
	pRxRing->u4RingSize = u4Size;
	pRxRing->fgRxSegPkt = FALSE;

	for (u4Idx = 0; u4Idx < u4Size; u4Idx++) {
		/* Init RX Ring Size, Va, Pa variables */
		prRxCell = &pRxRing->Cell[u4Idx];
		prRxCell->AllocSize = u4DescSize;
		prRxCell->AllocVa = RingBaseVa;
		prRxCell->AllocPa = RingBasePa;
		prRxCell->prToken = NULL;

		/* Offset to next ring descriptor address */
		RingBasePa += u4DescSize;
		RingBaseVa += u4DescSize;

		/* Setup Rx associated Buffer size & allocate share memory */
		pDmaBuf = &prRxCell->DmaBuf;
		pDmaBuf->AllocSize = u4BufSize;

		if (fgAllocMem && prMemOps->allocRxBuf)
			prRxCell->pPacket = prMemOps->allocRxBuf(
				prHifInfo, pDmaBuf, u4Num, u4Idx);
		if (pDmaBuf->AllocVa == NULL) {
			log_dbg(HAL, ERROR, "\nFailed to allocate RxRing buffer idx[%u]\n",
				u4Idx);
			return false;
		}

		/* Write RxD buffer address & allocated buffer length */
		pRxD = (struct RXD_STRUCT *)prRxCell->AllocVa;
		pRxD->SDPtr0 = ((uint64_t)pDmaBuf->AllocPa) &
			DMA_LOWER_32BITS_MASK;
#ifdef CONFIG_PHYS_ADDR_T_64BIT
		pRxD->SDPtr1 = (((uint64_t)pDmaBuf->AllocPa >>
			DMA_BITS_OFFSET) & DMA_HIGHER_4BITS_MASK);
#else
		pRxD->SDPtr1 = 0;
#endif
		pRxD->SDLen0 = u4BufSize;
		pRxD->DMADONE = 0;
	}

	DBGLOG(HAL, TRACE, "Rx[%d] Ring: total %d entry allocated\n",
	       u4Num, u4Idx);

	return true;
}

static void halDefaultHifRst(struct GLUE_INFO *prGlueInfo)
{
	/* Reset Conn HIF logic */
	kalDevRegWrite(prGlueInfo, CONN_HIF_RST, 0x00000020);
	kalDevRegWrite(prGlueInfo, CONN_HIF_RST, 0x00000030);
}

void halHifRst(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct BUS_INFO *prBusInfo;

	prAdapter = prGlueInfo->prAdapter;
	prBusInfo = prAdapter->chip_info->bus_info;

	/* Reset dmashdl and wpdma */
	if (prBusInfo->hifRst)
		prBusInfo->hifRst(prGlueInfo);
	else
		halDefaultHifRst(prGlueInfo);
}

bool halWpdmaAllocRing(struct GLUE_INFO *prGlueInfo, bool fgAllocMem)
{
	struct GL_HIF_INFO *prHifInfo;
	int32_t u4Num, u4Index;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct BUS_INFO *prBusInfo = NULL;
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
#if (CFG_SUPPORT_CONNAC2X == 1)
	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
#endif /* CFG_SUPPORT_CONNAC2X == 1  */

	/*
	 *   Allocate all ring descriptors, include TxD, RxD, MgmtD.
	 *   Although each size is different, to prevent cacheline and alignment
	 *   issue, I intentional set them all to 64 bytes
	 */
	for (u4Num = 0; u4Num < NUM_OF_TX_RING; u4Num++) {
		if (!halWpdmaAllocTxRing(prGlueInfo, u4Num, TX_RING_SIZE,
					 TXD_SIZE, fgAllocMem)) {
			DBGLOG(HAL, ERROR, "AllocTxRing[%d] fail\n", u4Num);
			return false;
		}
	}

	/* Data Rx path */
	if (!halWpdmaAllocRxRing(prGlueInfo, RX_RING_DATA_IDX_0,
				 RX_RING0_SIZE, RXD_SIZE,
				 CFG_RX_MAX_PKT_SIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[0] fail\n");
		return false;
	}

	/* For connac & old ic: Event Rx path */
	/* For falcon: TxFreeDoneEvent to Host Rx path */
	/* For buzzard(costdown wfdma): Event Rx path */
	/* Event Rx path */
#if (CFG_SUPPORT_CONNAC2X_2x2 == 1)

	if (!halWpdmaAllocRxRing(prGlueInfo, RX_RING_EVT_IDX_1,
				 RX_RING0_SIZE, RXD_SIZE,
				 RX_BUFFER_AGGRESIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[1] fail\n");
		return false;
	}
#else
	if (!halWpdmaAllocRxRing(prGlueInfo, RX_RING_EVT_IDX_1,
			 RX_RING1_SIZE, RXD_SIZE,
			 RX_BUFFER_AGGRESIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[1] fail\n");
		return false;
	}
#endif

#if (CFG_SUPPORT_CONNAC2X == 1)
	if (prBusInfo->wfdmaAllocRxRing)
		if (!prBusInfo->wfdmaAllocRxRing(prGlueInfo, fgAllocMem)) {
			DBGLOG(HAL, ERROR, "wfdmaAllocRxRing fail\n");
			return false;
		}
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

	/* Initialize all transmit related software queues */

	/* Init TX rings index pointer */
	for (u4Index = 0; u4Index < NUM_OF_TX_RING; u4Index++) {
		prHifInfo->TxRing[u4Index].TxSwUsedIdx = 0;
		prHifInfo->TxRing[u4Index].TxCpuIdx = 0;
	}

	return true;
}

void halWpdmaFreeRing(struct GLUE_INFO *prGlueInfo)
{
	struct GL_HIF_INFO *prHifInfo;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_TX_RING *pTxRing;
	struct RTMP_RX_RING *pRxRing;
	struct TXD_STRUCT *pTxD;
	struct RTMP_DMACB *prDmaCb;
	void *pPacket, *pBuffer;
	uint32_t i, j;

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;

	/* Free Tx Ring Packet */
	for (i = 0; i < NUM_OF_TX_RING; i++) {
		pTxRing = &prHifInfo->TxRing[i];
		for (j = 0; j < TX_RING_SIZE; j++) {
			pTxD = (struct TXD_STRUCT *) (pTxRing->Cell[j].AllocVa);

			pPacket = pTxRing->Cell[j].pPacket;
			pBuffer = pTxRing->Cell[j].pBuffer;
			if (prMemOps->unmapTxBuf && pPacket)
				prMemOps->unmapTxBuf(
					prHifInfo, pTxRing->Cell[j].PacketPa,
					pTxD->SDLen0);
			pTxRing->Cell[j].pPacket = NULL;

			if (prMemOps->freeBuf && pBuffer)
				prMemOps->freeBuf(pBuffer, pTxD->SDLen0);
			pTxRing->Cell[j].pBuffer = NULL;
		}

		halWpdmaFreeRingDesc(prGlueInfo, &prHifInfo->TxDescRing[i]);
	}

	for (i = 0; i < NUM_OF_RX_RING; i++) {
		pRxRing = &prHifInfo->RxRing[i];
		for (j = 0; j < pRxRing->u4RingSize; j++) {
			prDmaCb = &pRxRing->Cell[j];
			if (prMemOps->unmapRxBuf && prDmaCb->DmaBuf.AllocVa)
				prMemOps->unmapRxBuf(prHifInfo,
						     prDmaCb->DmaBuf.AllocPa,
						     prDmaCb->DmaBuf.AllocSize);
			if (prMemOps->freePacket && prDmaCb->pPacket)
				prMemOps->freePacket(prDmaCb->pPacket);
		}

		halWpdmaFreeRingDesc(prGlueInfo, &prHifInfo->RxDescRing[i]);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief enable firmware download.
 *
 * @param[in] fgEnable 1 for fw download, 0 for normal data operation.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void halEnableFWDownload(IN struct ADAPTER *prAdapter, IN u_int8_t fgEnable)
{
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->asicEnableFWDownload)
		prChipInfo->asicEnableFWDownload(prAdapter, fgEnable);
}

u_int8_t halWpdmaWaitIdle(struct GLUE_INFO *prGlueInfo,
	int32_t round, int32_t wait_us)
{
	int32_t i = 0;
	union WPDMA_GLO_CFG_STRUCT GloCfg;

	do {
		kalDevRegRead(prGlueInfo, WPDMA_GLO_CFG, &GloCfg.word);
		if ((GloCfg.field.TxDMABusy == 0) &&
		(GloCfg.field.RxDMABusy == 0)) {
			DBGLOG(HAL, TRACE,
				"==>  DMAIdle, GloCfg=0x%x\n", GloCfg.word);
			return TRUE;
		}
		kalUdelay(wait_us);
	} while ((i++) < round);

	DBGLOG(HAL, INFO, "==>  DMABusy, GloCfg=0x%x\n", GloCfg.word);

	return FALSE;
}

void halWpdmaInitRing(struct GLUE_INFO *prGlueInfo, bool fgResetHif)
{
	struct GL_HIF_INFO *prHifInfo;
	struct BUS_INFO *prBusInfo;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	/* Set DMA global configuration except TX_DMA_EN and RX_DMA_EN bits */
	if (prBusInfo->pdmaSetup)
		prBusInfo->pdmaSetup(prGlueInfo, FALSE, fgResetHif);

	halWpdmaInitTxRing(prGlueInfo);

	/* Init RX Ring0 Base/Size/Index pointer CSR */
	halWpdmaInitRxRing(prGlueInfo);

	if (prBusInfo->wfdmaManualPrefetch)
		prBusInfo->wfdmaManualPrefetch(prGlueInfo);

	if (prBusInfo->pdmaSetup)
		prBusInfo->pdmaSetup(prGlueInfo, TRUE, fgResetHif);

	/* Write sleep mode magic num to dummy reg */
	if (prBusInfo->setDummyReg)
		prBusInfo->setDummyReg(prGlueInfo);
}

void halWpdmaInitTxRing(IN struct GLUE_INFO *prGlueInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct BUS_INFO *prBusInfo = NULL;
	struct RTMP_TX_RING *prTxRing = NULL;
	struct RTMP_DMACB *prTxCell;
	uint32_t i = 0, offset = 0, phy_addr = 0;
	struct mt66xx_chip_info *prChipInfo;

	prHifInfo = &prGlueInfo->rHifInfo;
	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
	prChipInfo = prGlueInfo->prAdapter->chip_info;

	/* reset all TX Ring register */
	for (i = 0; i < NUM_OF_TX_RING; i++) {
		prTxRing = &prHifInfo->TxRing[i];
		prTxCell = &prTxRing->Cell[0];

		if (i == TX_RING_CMD_IDX_2)
			offset = prBusInfo->tx_ring_cmd_idx * MT_RINGREG_DIFF;
		else if (i == TX_RING_FWDL_IDX_3)
			offset = prBusInfo->tx_ring_fwdl_idx * MT_RINGREG_DIFF;
#if (CFG_SUPPORT_CONNAC2X == 1)
		else if (prChipInfo->is_support_wacpu) {
			uint32_t idx = 0;

			if (i == TX_RING_DATA0_IDX_0)
				idx = prBusInfo->tx_ring0_data_idx;
			else if (i == TX_RING_DATA1_IDX_1)
				idx = prBusInfo->tx_ring1_data_idx;
			else if (i == TX_RING_WA_CMD_IDX_4)
				idx = prBusInfo->tx_ring_wa_cmd_idx;
			offset = idx * MT_RINGREG_DIFF;
		}
#endif /* CFG_SUPPORT_CONNAC2X == 1 */
		else
			offset = i * MT_RINGREG_DIFF;

		phy_addr = ((uint64_t)prTxCell->AllocPa) &
			DMA_LOWER_32BITS_MASK;
		prTxRing->TxSwUsedIdx = 0;
		prTxRing->u4UsedCnt = 0;
		prTxRing->TxCpuIdx = 0;

		prTxRing->hw_desc_base =
			prBusInfo->host_tx_ring_base + offset;

		prTxRing->hw_cidx_addr =
			prBusInfo->host_tx_ring_cidx_addr + offset;
		prTxRing->hw_didx_addr =
			prBusInfo->host_tx_ring_didx_addr + offset;
		prTxRing->hw_cnt_addr =
			prBusInfo->host_tx_ring_cnt_addr + offset;

		kalDevRegWrite(prGlueInfo, prTxRing->hw_desc_base, phy_addr);
		kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr,
			prTxRing->TxCpuIdx);
		kalDevRegWrite(prGlueInfo, prTxRing->hw_cnt_addr,
			TX_RING_SIZE);

		if (prBusInfo->tx_ring_ext_ctrl)
			prBusInfo->tx_ring_ext_ctrl(prGlueInfo, prTxRing, i);

		DBGLOG(HAL, TRACE, "-->TX_RING_%d[0x%x]: Base=0x%x, Cnt=%d!\n",
			i, prHifInfo->TxRing[i].hw_desc_base,
			phy_addr, TX_RING_SIZE);
	}
}

static uint8_t defaultSetRxRingHwAddr(
	struct RTMP_RX_RING *prRxRing,
	struct BUS_INFO *prBusInfo,
	struct mt66xx_chip_info *prChipInfo,
	uint32_t u4SwRingIdx)
{
	uint32_t offset = 0;

	if (u4SwRingIdx >= WFDMA1_RX_RING_IDX_0) {
#if (CFG_SUPPORT_CONNAC2X == 1)
		if (prChipInfo->is_support_wfdma1) {
			offset = (u4SwRingIdx - WFDMA1_RX_RING_IDX_0)
					* MT_RINGREG_DIFF;
			prRxRing->hw_desc_base =
				prBusInfo->host_wfdma1_rx_ring_base
					+ offset;
			prRxRing->hw_cidx_addr =
				prBusInfo->host_wfdma1_rx_ring_cidx_addr
					+ offset;
			prRxRing->hw_didx_addr =
				prBusInfo->host_wfdma1_rx_ring_didx_addr
					+ offset;
			prRxRing->hw_cnt_addr =
				prBusInfo->host_wfdma1_rx_ring_cnt_addr
					+ offset;
		} else
#endif /* CFG_SUPPORT_CONNAC2X == 1 */
			return FALSE;
	} else {
		offset = u4SwRingIdx * MT_RINGREG_DIFF;
		prRxRing->hw_desc_base =
			prBusInfo->host_rx_ring_base + offset;
		prRxRing->hw_cidx_addr =
			prBusInfo->host_rx_ring_cidx_addr + offset;
		prRxRing->hw_didx_addr =
			prBusInfo->host_rx_ring_didx_addr + offset;
		prRxRing->hw_cnt_addr =
			prBusInfo->host_rx_ring_cnt_addr + offset;
	}

	return TRUE;
}

void halWpdmaInitRxRing(IN struct GLUE_INFO *prGlueInfo)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct RTMP_RX_RING *prRxRing = NULL;
	uint32_t i = 0, phy_addr = 0;
	struct BUS_INFO *prBusInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;
	uint8_t rv;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	prBusInfo = prChipInfo->bus_info;

	/* reset all RX Ring register */
	for (i = 0; i < NUM_OF_RX_RING; i++) {
		prRxRing = &prHifInfo->RxRing[i];
		if (prBusInfo->setRxRingHwAddr)
			rv = prBusInfo->setRxRingHwAddr(prRxRing, prBusInfo, i);
		else
			rv = defaultSetRxRingHwAddr(
				prRxRing, prBusInfo, prChipInfo, i);

		if (!rv)
			break;
		phy_addr = ((uint64_t)prRxRing->Cell[0].AllocPa &
			DMA_LOWER_32BITS_MASK);
		prRxRing->RxCpuIdx = prRxRing->u4RingSize - 1;
		kalDevRegWrite(prGlueInfo, prRxRing->hw_desc_base, phy_addr);
		kalDevRegWrite(prGlueInfo, prRxRing->hw_cidx_addr,
			prRxRing->RxCpuIdx);
		kalDevRegWrite(prGlueInfo, prRxRing->hw_cnt_addr,
			prRxRing->u4RingSize);

		if (prBusInfo->rx_ring_ext_ctrl)
			prBusInfo->rx_ring_ext_ctrl(prGlueInfo, prRxRing, i);

		prRxRing->fgIsDumpLog = false;

		DBGLOG(HAL, TRACE, "-->RX_RING_%d[0x%x]: Base=0x%x, Cnt=%d\n",
			i, prRxRing->hw_desc_base,
			phy_addr, prRxRing->u4RingSize);
	}
}

void halWpdmaProcessCmdDmaDone(IN struct GLUE_INFO *prGlueInfo,
	IN uint16_t u2Port)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_TX_RING *prTxRing;
	struct TXD_STRUCT *pTxD;
	phys_addr_t PacketPa = 0;
	void *pBuffer = NULL;
	uint32_t u4SwIdx, u4DmaIdx = 0;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prTxRing = &prHifInfo->TxRing[u2Port];

	kalDevRegRead(prGlueInfo, prTxRing->hw_didx_addr, &u4DmaIdx);
	u4SwIdx = prTxRing->TxSwUsedIdx;

	do {
		pBuffer = prTxRing->Cell[u4SwIdx].pBuffer;
		PacketPa = prTxRing->Cell[u4SwIdx].PacketPa;
		pTxD = (struct TXD_STRUCT *) prTxRing->Cell[u4SwIdx].AllocVa;

		if (pTxD->DMADONE == 0)
			break;

		log_dbg(HAL, TRACE, "DMA done: port[%u] dma[%u] idx[%u] done[%u] pkt[0x%p] used[%u]\n",
			u2Port, u4DmaIdx, u4SwIdx, pTxD->DMADONE,
			prTxRing->Cell[u4SwIdx].pPacket, prTxRing->u4UsedCnt);

		if (prMemOps->unmapTxBuf && PacketPa)
			prMemOps->unmapTxBuf(prHifInfo, PacketPa, pTxD->SDLen0);

		pTxD->DMADONE = 0;
		if (prMemOps->freeBuf && pBuffer)
			prMemOps->freeBuf(pBuffer, 0);
		prTxRing->Cell[u4SwIdx].pBuffer = NULL;
		prTxRing->Cell[u4SwIdx].pPacket = NULL;
		prTxRing->u4UsedCnt--;

		GLUE_INC_REF_CNT(
			prGlueInfo->prAdapter->rHifStats.u4CmdTxdoneCount);

		INC_RING_INDEX(u4SwIdx, TX_RING_SIZE);
	} while (u4SwIdx != u4DmaIdx);

	prTxRing->TxSwUsedIdx = u4SwIdx;

}

void halWpdmaProcessDataDmaDone(IN struct GLUE_INFO *prGlueInfo,
	IN uint16_t u2Port)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint32_t u4SwIdx, u4DmaIdx = 0, u4Diff = 0;
	struct RTMP_TX_RING *prTxRing;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prTxRing = &prHifInfo->TxRing[u2Port];

	kalDevRegRead(prGlueInfo, prTxRing->hw_didx_addr, &u4DmaIdx);
	u4SwIdx = prTxRing->TxSwUsedIdx;

	if (u4DmaIdx > u4SwIdx) {
		u4Diff = u4DmaIdx - u4SwIdx;
		prTxRing->u4UsedCnt -= u4Diff;
	} else if (u4DmaIdx < u4SwIdx) {
		u4Diff = (TX_RING_SIZE + u4DmaIdx) - u4SwIdx;
		prTxRing->u4UsedCnt -= u4Diff;
	} else {
		/* DMA index == SW used index */
		if (prTxRing->u4UsedCnt == TX_RING_SIZE) {
			u4Diff = TX_RING_SIZE;
			prTxRing->u4UsedCnt = 0;
		}
	}

	DBGLOG_LIMITED(HAL, TRACE,
		"DMA done: port[%u] dma[%u] idx[%u] used[%u]\n", u2Port,
		u4DmaIdx, u4SwIdx, prTxRing->u4UsedCnt);

	GLUE_ADD_REF_CNT(u4Diff,
			prGlueInfo->prAdapter->rHifStats.u4DataTxdoneCount);

	prTxRing->TxSwUsedIdx = u4DmaIdx;
}

uint32_t halWpdmaGetRxDmaDoneCnt(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucRingNum)
{
	struct RTMP_RX_RING *prRxRing;
	struct GL_HIF_INFO *prHifInfo;
	uint32_t u4MaxCnt = 0, u4CpuIdx = 0, u4DmaIdx = 0, u4RxPktCnt;

	prHifInfo = &prGlueInfo->rHifInfo;
	prRxRing = &prHifInfo->RxRing[ucRingNum];

	kalDevRegRead(prGlueInfo, prRxRing->hw_cnt_addr, &u4MaxCnt);
	kalDevRegRead(prGlueInfo, prRxRing->hw_cidx_addr, &u4CpuIdx);
	kalDevRegRead(prGlueInfo, prRxRing->hw_didx_addr, &u4DmaIdx);

	if (u4MaxCnt == 0 || u4MaxCnt > RX_RING_SIZE)
		return 0;

	if (u4CpuIdx > u4DmaIdx)
		u4RxPktCnt = u4MaxCnt + u4DmaIdx - u4CpuIdx - 1;
	else if (u4CpuIdx < u4DmaIdx)
		u4RxPktCnt = u4DmaIdx - u4CpuIdx - 1;
	else
		u4RxPktCnt = u4MaxCnt - 1;

	return u4RxPktCnt;
}

enum ENUM_CMD_TX_RESULT halWpdmaWriteCmd(IN struct GLUE_INFO *prGlueInfo,
		      IN struct CMD_INFO *prCmdInfo, IN uint8_t ucTC)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct RTMP_TX_RING *prTxRing;
	struct RTMP_DMACB *pTxCell;
	struct TXD_STRUCT *pTxD;
	uint16_t u2Port = TX_RING_CMD_IDX_2;
	uint32_t u4TotalLen;
	void *pucSrc = NULL;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif /* CFG_SUPPORT_CONNAC2 == 1 */

	ASSERT(prGlueInfo);

#if (CFG_SUPPORT_CONNAC2X == 1)
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	if (prChipInfo->is_support_wacpu)
		u2Port = TX_RING_WA_CMD_IDX_4;
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prTxRing = &prHifInfo->TxRing[u2Port];

	u4TotalLen = prCmdInfo->u4TxdLen + prCmdInfo->u4TxpLen;
#if (CFG_SUPPORT_CONNAC2X == 1)
	if (u4TotalLen > prChipInfo->cmd_max_pkt_size) {
		DBGLOG(HAL, ERROR,
			"type: %u, cid: 0x%x, seq: %u, txd: %u, txp: %u\n",
				prCmdInfo->eCmdType,
				prCmdInfo->ucCID,
				prCmdInfo->ucCmdSeqNum,
				prCmdInfo->u4TxdLen,
				prCmdInfo->u4TxpLen);
		if (prCmdInfo->u4TxdLen)
			DBGLOG_MEM32(HAL, ERROR, prCmdInfo->pucTxd,
				prCmdInfo->u4TxdLen);
		if (prCmdInfo->u4TxpLen)
			DBGLOG_MEM32(HAL, ERROR, prCmdInfo->pucTxp,
				prCmdInfo->u4TxpLen);
		return FALSE;
	}
#endif /* CFG_SUPPORT_CONNAC2X == 1 */
	if (prMemOps->allocRuntimeMem)
		pucSrc = prMemOps->allocRuntimeMem(u4TotalLen);

	kalDevRegRead(prGlueInfo, prTxRing->hw_cidx_addr, &prTxRing->TxCpuIdx);
	if (prTxRing->TxCpuIdx >= TX_RING_SIZE) {
		DBGLOG(HAL, ERROR, "Error TxCpuIdx[%u]\n", prTxRing->TxCpuIdx);
		if (prMemOps->freeBuf)
			prMemOps->freeBuf(pucSrc, u4TotalLen);
		return CMD_TX_RESULT_FAILED;
	}

	pTxCell = &prTxRing->Cell[prTxRing->TxCpuIdx];
	pTxD = (struct TXD_STRUCT *)pTxCell->AllocVa;
	pTxCell->pPacket = (void *)prCmdInfo;
	pTxCell->pBuffer = pucSrc;

	if (prMemOps->copyCmd &&
	    !prMemOps->copyCmd(prHifInfo, pTxCell, pucSrc,
			       prCmdInfo->pucTxd, prCmdInfo->u4TxdLen,
			       prCmdInfo->pucTxp, prCmdInfo->u4TxpLen)) {
		if (prMemOps->freeBuf)
			prMemOps->freeBuf(pucSrc, u4TotalLen);
		ASSERT(0);
		return CMD_TX_RESULT_FAILED;
	}

	pTxD->SDPtr0 = (uint64_t)pTxCell->PacketPa & DMA_LOWER_32BITS_MASK;
#ifdef CONFIG_PHYS_ADDR_T_64BIT
	pTxD->SDPtr0Ext = ((uint64_t)pTxCell->PacketPa >> DMA_BITS_OFFSET) &
		DMA_HIGHER_4BITS_MASK;
#else
	pTxD->SDPtr0Ext = 0;
#endif
	pTxD->SDLen0 = u4TotalLen;
	pTxD->SDPtr1 = 0;
	pTxD->SDLen1 = 0;
	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

	/* Increase TX_CTX_IDX, but write to register later. */
	INC_RING_INDEX(prTxRing->TxCpuIdx, TX_RING_SIZE);

	prTxRing->u4UsedCnt++;
	kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);

	GLUE_INC_REF_CNT(prGlueInfo->prAdapter->rHifStats.u4CmdTxCount);

	DBGLOG(HAL, TRACE,
	       "%s: CmdInfo[0x%p], TxD[0x%p/%u] TxP[0x%p/%u] CPU idx[%u] Used[%u]\n",
	       __func__, prCmdInfo, prCmdInfo->pucTxd, prCmdInfo->u4TxdLen,
	       prCmdInfo->pucTxp, prCmdInfo->u4TxpLen,
	       prTxRing->TxCpuIdx, prTxRing->u4UsedCnt);
	DBGLOG_MEM32(HAL, TRACE, prCmdInfo->pucTxd, prCmdInfo->u4TxdLen);

	if (u2Port == TX_RING_CMD_IDX_2
#if (CFG_SUPPORT_CONNAC2X == 1)
			|| u2Port == TX_RING_WA_CMD_IDX_4
#endif /* CFG_SUPPORT_CONNAC2 == 1 */
		)
		nicTxReleaseResource_PSE(prGlueInfo->prAdapter,
			TC4_INDEX,
			nicTxGetPageCount(prGlueInfo->prAdapter,
				pTxD->SDLen0,
				TRUE),
			TRUE);

	return CMD_TX_RESULT_SUCCESS;
}

static bool halWpdmaFillTxRing(struct GLUE_INFO *prGlueInfo,
			       struct MSDU_TOKEN_ENTRY *prToken)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;
	struct RTMP_TX_RING *prTxRing;
	struct RTMP_DMACB *pTxCell;
	struct TXD_STRUCT *pTxD;
	uint16_t u2Port = TX_RING_DATA0_IDX_0;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prChipInfo = prGlueInfo->prAdapter->chip_info;

	u2Port = halTxRingDataSelect(
		prGlueInfo->prAdapter, prToken->prMsduInfo);
	prTxRing = &prHifInfo->TxRing[u2Port];

	kalDevRegRead(prGlueInfo, prTxRing->hw_cidx_addr, &prTxRing->TxCpuIdx);
	if (prTxRing->TxCpuIdx >= TX_RING_SIZE) {
		DBGLOG(HAL, ERROR, "Error TxCpuIdx[%u]\n", prTxRing->TxCpuIdx);
		halReturnMsduToken(prGlueInfo->prAdapter, prToken->u4Token);
		return FALSE;
	}

	pTxCell = &prTxRing->Cell[prTxRing->TxCpuIdx];
	prToken->u4CpuIdx = prTxRing->TxCpuIdx;
	prToken->u2Port = u2Port;
	pTxCell->prToken = prToken;

	pTxD = (struct TXD_STRUCT *)pTxCell->AllocVa;
	pTxD->SDPtr0 = (uint64_t)prToken->rDmaAddr & DMA_LOWER_32BITS_MASK;
#ifdef CONFIG_PHYS_ADDR_T_64BIT
	pTxD->SDPtr0Ext = ((uint64_t)prToken->rDmaAddr >> DMA_BITS_OFFSET) &
		DMA_HIGHER_4BITS_MASK;
#else
	pTxD->SDPtr0Ext = 0;
#endif
	pTxD->SDLen0 = NIC_TX_DESC_AND_PADDING_LENGTH +
		prChipInfo->txd_append_size;
	if (prChipInfo->is_support_cr4)
		pTxD->SDLen0 += HIF_TX_PAYLOAD_LENGTH;
	pTxD->SDPtr1 = 0;
	pTxD->SDLen1 = 0;
	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

	/* Increase TX_CTX_IDX, but write to register later. */
	INC_RING_INDEX(prTxRing->TxCpuIdx, TX_RING_SIZE);

	/* Update HW Tx DMA ring */
	prTxRing->u4UsedCnt++;
	kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);

	DBGLOG_LIMITED(HAL, TRACE,
		"Tx Data:Ring%d CPU idx[0x%x] Used[%u]\n",
		u2Port, prTxRing->TxCpuIdx, prTxRing->u4UsedCnt);

	GLUE_INC_REF_CNT(prGlueInfo->prAdapter->rHifStats.u4DataTxCount);

	return TRUE;
}

static bool halFlushToken(struct GLUE_INFO *prGlueInfo,
			  struct MSDU_TOKEN_ENTRY *prToken)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;

	if (prMemOps->mapTxBuf) {
		prToken->rDmaAddr = prMemOps->mapTxBuf(
			prHifInfo, prToken->prPacket, 0, prToken->u4DmaLength);
		if (!prToken->rDmaAddr)
			return false;
	}

	return true;
}

static bool halWpdmaWriteData(struct GLUE_INFO *prGlueInfo,
			      struct MSDU_INFO *prMsduInfo,
			      struct MSDU_TOKEN_ENTRY *prFillToken,
			      struct MSDU_TOKEN_ENTRY *prToken,
			      uint32_t u4Idx, uint32_t u4Num)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;
	bool fgIsLast = (u4Idx + 1) == u4Num;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;
	prChipInfo = prGlueInfo->prAdapter->chip_info;

	/* Update Tx descriptor */
	halTxUpdateCutThroughDesc(prGlueInfo, prMsduInfo, prFillToken,
				  prToken, u4Idx, fgIsLast);

	/* Update token exclude FillToken */
	if (prToken != prFillToken) {
		if (!halFlushToken(prGlueInfo, prToken))
			return false;
	}

	/* Update FillToken */
	if (fgIsLast) {
		if (!halFlushToken(prGlueInfo, prFillToken))
			return false;
		halWpdmaFillTxRing(prGlueInfo, prFillToken);
	}

	return true;
}

void halWpdamFreeMsdu(struct GLUE_INFO *prGlueInfo,
		      struct MSDU_INFO *prMsduInfo,
		      bool fgSetEvent)
{
	DBGLOG(HAL, LOUD, "Tx Data: Msdu[0x%p], TokFree[%u] TxDone[%u]\n",
		prMsduInfo, halGetMsduTokenFreeCnt(prGlueInfo->prAdapter),
		(prMsduInfo->pfTxDoneHandler ? TRUE : FALSE));

	nicTxReleaseResource_PSE(prGlueInfo->prAdapter, prMsduInfo->ucTC,
		nicTxGetPageCount(prGlueInfo->prAdapter,
		prMsduInfo->u2FrameLength, TRUE), TRUE);

#if HIF_TX_PREALLOC_DATA_BUFFER
	if (!prMsduInfo->pfTxDoneHandler) {
		nicTxFreePacket(prGlueInfo->prAdapter, prMsduInfo, FALSE);
		nicTxReturnMsduInfo(prGlueInfo->prAdapter, prMsduInfo);
	}
#endif

	if (fgSetEvent && wlanGetTxPendingFrameCount(prGlueInfo->prAdapter))
		kalSetEvent(prGlueInfo);
}

bool halWpdmaWriteMsdu(struct GLUE_INFO *prGlueInfo,
		       struct MSDU_INFO *prMsduInfo,
		       struct list_head *prCurList)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct MSDU_TOKEN_ENTRY *prToken = NULL;
	struct sk_buff *prSkb;
	uint8_t *pucSrc;
	uint32_t u4TotalLen;

	uint32_t u4TxDescAppendSize;

	ASSERT(prGlueInfo);
	ASSERT(prMsduInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	prSkb = (struct sk_buff *)prMsduInfo->prPacket;

	if (prSkb == NULL || prSkb->data == NULL) {
		DBGLOG(HAL, ERROR, "prSkb is 0x%p\n", prSkb);

		if (prCurList) {
			list_del(prCurList);
			prHifInfo->u4TxDataQLen--;
		}
		halWpdamFreeMsdu(prGlueInfo, prMsduInfo, true);

		return false;
	}

	pucSrc = prSkb->data;

	u4TotalLen = prSkb->len;

	if (prGlueInfo->prAdapter != NULL &&
		prGlueInfo->prAdapter->chip_info != NULL) {

		u4TxDescAppendSize =
			prGlueInfo->prAdapter->chip_info->txd_append_size;
	} else {
		DBGLOG(HAL, ERROR, "prGlueInfo->prAdapter is null\n");

		if (prCurList) {
			list_del(prCurList);
			prHifInfo->u4TxDataQLen--;
		}
		halWpdamFreeMsdu(prGlueInfo, prMsduInfo, true);

		return false;
	}

	if (u4TotalLen <= (AXI_TX_MAX_SIZE_PER_FRAME + u4TxDescAppendSize)) {

		/* Acquire MSDU token */
		prToken = halAcquireMsduToken(prGlueInfo->prAdapter);
		if (!prToken) {
			DBGLOG(HAL, ERROR, "Write MSDU acquire token fail\n");
			return false;
		}

		/* Use MsduInfo to select TxRing */
		prToken->prMsduInfo = prMsduInfo;

#if HIF_TX_PREALLOC_DATA_BUFFER
		if (prMemOps->copyTxData)
			prMemOps->copyTxData(prToken, pucSrc, u4TotalLen);
#else
		prToken->prPacket = pucSrc;
		prToken->u4DmaLength = u4TotalLen;
		prMsduInfo->prToken = prToken;
#endif

		if (!halWpdmaWriteData(prGlueInfo, prMsduInfo, prToken,
			prToken, 0, 1)) {
			halReturnMsduToken(prGlueInfo->prAdapter,
				prToken->u4Token);
			return false;
		}

	} else {
		DBGLOG(HAL, ERROR, "u4Len=%u, 0x%p, txd_append_size=%d\n",
			u4TotalLen, prSkb,
			prGlueInfo->prAdapter->chip_info->txd_append_size);

		DBGLOG(HAL, ERROR, "%u,%u,%u,%u,%u,%u,%u,%u\n",
			prMsduInfo->eSrc, prMsduInfo->ucUserPriority,
			prMsduInfo->ucTC, prMsduInfo->ucPacketType,
			prMsduInfo->ucStaRecIndex, prMsduInfo->ucBssIndex,
			prMsduInfo->ucWlanIndex, prMsduInfo->ucPacketFormat);
	}

	if (prCurList) {
		list_del(prCurList);
		prHifInfo->u4TxDataQLen--;
	}
	if (prMsduInfo->pfHifTxMsduDoneCb)
		prMsduInfo->pfHifTxMsduDoneCb(prGlueInfo->prAdapter,
				prMsduInfo);
	halWpdamFreeMsdu(prGlueInfo, prMsduInfo, true);

	return true;
}

bool halWpdmaWriteAmsdu(struct GLUE_INFO *prGlueInfo,
			struct list_head *prList,
			uint32_t u4Num, uint16_t u2Size)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct list_head *prCur, *prNext;
	struct TX_DATA_REQ *prTxReq;
	struct MSDU_TOKEN_ENTRY *prFillToken = NULL, *prToken = NULL;
	struct MSDU_INFO *prMsduInfo;
	struct AMSDU_MAC_TX_DESC *prTxD = NULL;
	struct sk_buff *prSkb;
	uint8_t *pucSrc;
	uint32_t u4TotalLen, u4Idx, u4FreeToken, u4FreeRing;
	uint16_t u2Port;
	bool fgIsLast;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;
	prMemOps = &prHifInfo->rMemOps;
	u4FreeToken = halGetMsduTokenFreeCnt(prGlueInfo->prAdapter);

	/* Peek head to select TxRing */
	prTxReq = list_entry(prList, struct TX_DATA_REQ, list);
	u2Port = halTxRingDataSelect(
		prGlueInfo->prAdapter, prTxReq->prMsduInfo);
	u4FreeRing = TX_RING_SIZE - prHifInfo->TxRing[u2Port].u4UsedCnt;

	if ((u4FreeToken < u4Num) || (u4FreeRing <= 1)) {
		DBGLOG(HAL, WARN,
		       "Amsdu low tx res acquire[%u], tok[%u], ring%d[%u]\n",
		       u4Num, u4FreeToken, u2Port, u4FreeRing);
		return false;
	}

	prCur = prList;
	for (u4Idx = 0; u4Idx < u4Num; u4Idx++) {
		prTxReq = list_entry(prCur, struct TX_DATA_REQ, list);
		prMsduInfo = prTxReq->prMsduInfo;
		prSkb = (struct sk_buff *)prMsduInfo->prPacket;
		pucSrc = prSkb->data;
		u4TotalLen = prSkb->len;
		fgIsLast = (u4Idx == u4Num - 1);

		/* Acquire MSDU token */
		prToken = halAcquireMsduToken(prGlueInfo->prAdapter);
		if (!prToken) {
			DBGLOG(HAL, ERROR, "Write AMSDU acquire token fail\n");
			return false;
		}

		/* Use MsduInfo to select TxRing */
		prToken->prMsduInfo = prMsduInfo;
#if HIF_TX_PREALLOC_DATA_BUFFER
		if (prMemOps->copyTxData)
			prMemOps->copyTxData(prToken, pucSrc, u4TotalLen);
#else
		prToken->prPacket = pucSrc;
		prToken->u4DmaLength = u4TotalLen;
		prMsduInfo->prToken = prToken;
#endif

		if (!prFillToken) {
			prFillToken = prToken;
			prTxD = (struct AMSDU_MAC_TX_DESC *)prToken->prPacket;
		}

		if (fgIsLast) {
			prTxD->u2TxByteCount = u2Size;
			prTxD->u4DW1 |= TXD_DW1_AMSDU_C;
		}

		if (!halWpdmaWriteData(prGlueInfo, prMsduInfo, prFillToken,
				       prToken, u4Idx, u4Num)) {
			halReturnMsduToken(prGlueInfo->prAdapter,
					   prToken->u4Token);
			return false;
		}
		prCur = prCur->next;
	}

	prCur = prList;
	for (u4Idx = 0; u4Idx < u4Num; u4Idx++) {
		prNext = prCur->next;
		prTxReq = list_entry(prCur, struct TX_DATA_REQ, list);
		prMsduInfo = prTxReq->prMsduInfo;

		list_del(prCur);
		prHifInfo->u4TxDataQLen--;
		if (prMsduInfo->pfHifTxMsduDoneCb)
			prMsduInfo->pfHifTxMsduDoneCb(prGlueInfo->prAdapter,
					prMsduInfo);
		halWpdamFreeMsdu(prGlueInfo, prMsduInfo, true);
		prCur = prNext;
	}

	DBGLOG(HAL, LOUD, "Amsdu num:%d tx byte: %d\n", u4Num, u2Size);
	return true;
}

u_int8_t halIsStaticMapBusAddr(IN struct ADAPTER *prAdapter,
	IN uint32_t u4Addr)
{
	if (u4Addr < prAdapter->chip_info->bus_info->max_static_map_addr)
		return TRUE;
	else
		return FALSE;
}

u_int8_t halChipToStaticMapBusAddr(IN struct GLUE_INFO *prGlueInfo,
				   IN uint32_t u4ChipAddr,
				   OUT uint32_t *pu4BusAddr)
{
	struct BUS_INFO *prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
	uint32_t u4StartAddr, u4EndAddr, u4BusAddr;
	uint32_t u4Idx = 0;

	if (halIsStaticMapBusAddr(prGlueInfo->prAdapter, u4ChipAddr)) {
		*pu4BusAddr = u4ChipAddr;
		return TRUE;
	}

	while (TRUE) {
		u4StartAddr = prBusInfo->bus2chip[u4Idx].u4ChipAddr;
		u4EndAddr = prBusInfo->bus2chip[u4Idx].u4ChipAddr +
			prBusInfo->bus2chip[u4Idx].u4Range;

		/* End of mapping table */
		if (u4EndAddr == 0x0)
			return FALSE;

		if ((u4ChipAddr >= u4StartAddr) && (u4ChipAddr <= u4EndAddr)) {
			u4BusAddr = (u4ChipAddr - u4StartAddr) +
				prBusInfo->bus2chip[u4Idx].u4BusAddr;
			break;
		}

		u4Idx++;
	}

	*pu4BusAddr = u4BusAddr;
	return TRUE;
}

u_int8_t halGetDynamicMapReg(IN struct GLUE_INFO *prGlueInfo,
			     IN uint32_t u4ChipAddr, OUT uint32_t *pu4Value)
{
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	uint32_t u4ReMapReg, u4BusAddr;

	if (!halChipToStaticMapBusAddr(prGlueInfo, MCU_CFG_PCIE_REMAP2,
				       &u4ReMapReg))
		return FALSE;


	RTMP_IO_WRITE32(prHifInfo, u4ReMapReg, u4ChipAddr & PCIE_REMAP2_MASK);
	u4BusAddr = PCIE_REMAP2_BUS_ADDR + (u4ChipAddr & ~PCIE_REMAP2_MASK);
	RTMP_IO_READ32(prHifInfo, u4BusAddr, pu4Value);

	return TRUE;
}

u_int8_t halSetDynamicMapReg(IN struct GLUE_INFO *prGlueInfo,
			     IN uint32_t u4ChipAddr, IN uint32_t u4Value)
{
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	uint32_t u4ReMapReg, u4BusAddr;

	if (!halChipToStaticMapBusAddr(prGlueInfo, MCU_CFG_PCIE_REMAP2,
				       &u4ReMapReg))
		return FALSE;

	RTMP_IO_WRITE32(prHifInfo, u4ReMapReg, u4ChipAddr & PCIE_REMAP2_MASK);
	u4BusAddr = PCIE_REMAP2_BUS_ADDR + (u4ChipAddr & ~PCIE_REMAP2_MASK);
	RTMP_IO_WRITE32(prHifInfo, u4BusAddr, u4Value);

	return TRUE;
}

u_int8_t halIsPendingRx(IN struct ADAPTER *prAdapter)
{
	/* TODO: check pending Rx
	 * if previous Rx handling is break due to lack of SwRfb
	 */
	return FALSE;
}

uint32_t halGetValidCoalescingBufSize(IN struct ADAPTER *prAdapter)
{
	uint32_t u4BufSize;

	if (HIF_TX_COALESCING_BUFFER_SIZE > HIF_RX_COALESCING_BUFFER_SIZE)
		u4BufSize = HIF_TX_COALESCING_BUFFER_SIZE;
	else
		u4BufSize = HIF_RX_COALESCING_BUFFER_SIZE;

	return u4BufSize;
}

uint32_t halAllocateIOBuffer(IN struct ADAPTER *prAdapter)
{
	return WLAN_STATUS_SUCCESS;
}

uint32_t halReleaseIOBuffer(IN struct ADAPTER *prAdapter)
{
	return WLAN_STATUS_SUCCESS;
}

void halProcessAbnormalInterrupt(IN struct ADAPTER *prAdapter)
{

}

static void halDefaultProcessSoftwareInterrupt(
	IN struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_HIF_INFO *prHifInfo;
	struct ERR_RECOVERY_CTRL_T *prErrRecoveryCtrl;
	uint32_t u4Status = 0;

	if (prAdapter->prGlueInfo == NULL) {
		DBGLOG(HAL, ERROR, "prGlueInfo is NULL\n");
		return;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;
	prErrRecoveryCtrl = &prHifInfo->rErrRecoveryCtl;

	kalDevRegRead(prGlueInfo, MCU2HOST_SW_INT_STA, &u4Status);
	prErrRecoveryCtrl->u4BackupStatus = u4Status;
	if (u4Status & ERROR_DETECT_MASK) {
		prErrRecoveryCtrl->u4Status = u4Status;
		kalDevRegWrite(prGlueInfo, MCU2HOST_SW_INT_STA,
			ERROR_DETECT_MASK);
		halHwRecoveryFromError(prAdapter);
	} else
		DBGLOG(HAL, ERROR, "undefined SER status[0x%x].\n", u4Status);
}

void halProcessSoftwareInterrupt(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = NULL;

	if (prAdapter == NULL) {
		DBGLOG(HAL, ERROR, "prAdapter is NULL\n");
		return;
	}

	prBusInfo = prAdapter->chip_info->bus_info;

	if (prBusInfo->processSoftwareInterrupt)
		prBusInfo->processSoftwareInterrupt(prAdapter);
	else
		halDefaultProcessSoftwareInterrupt(prAdapter);
}
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
void halHwRecoveryTimeout(struct timer_list *timer)
#else
void halHwRecoveryTimeout(unsigned long arg)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
	struct GL_HIF_INFO *prHif = from_timer(prHif, timer, rSerTimer);
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)prHif->rSerTimerData;
#else
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)arg;
#endif
	struct ADAPTER *prAdapter = NULL;
	struct GL_HIF_INFO *prHifInfo;
	struct ERR_RECOVERY_CTRL_T *prErrRecoveryCtrl;

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prHifInfo = &prGlueInfo->rHifInfo;
	prErrRecoveryCtrl = &prHifInfo->rErrRecoveryCtl;

	DBGLOG(HAL, ERROR,
	       "SER timer Timeout. ErrState[%d] Status[0x%x] Backup[0x%x]\n",
	       prErrRecoveryCtrl->eErrRecovState,
	       prErrRecoveryCtrl->u4Status,
	       prErrRecoveryCtrl->u4BackupStatus);

#if CFG_CHIP_RESET_SUPPORT
#if (CFG_SUPPORT_CONNINFRA == 0)
	GL_RESET_TRIGGER(prAdapter, RST_FLAG_CHIP_RESET);
#else
	kalSetSerTimeoutEvent(prGlueInfo);
#endif
#endif
}

void halSetDrvSer(struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prAdapter->prGlueInfo);

	prBusInfo = prAdapter->chip_info->bus_info;

	DBGLOG(HAL, INFO, "Set Driver Ser\n");
	if (prBusInfo->softwareInterruptMcu)
		prBusInfo->softwareInterruptMcu(prAdapter,
				MCU_INT_DRIVER_SER);
	else
		kalDevRegWrite(prAdapter->prGlueInfo, HOST2MCU_SW_INT_SET,
				MCU_INT_DRIVER_SER);
}

static void halStartSerTimer(IN struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_HIF_INFO *prHifInfo;

	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;
	mod_timer(&prHifInfo->rSerTimer,
		  jiffies + HIF_SER_TIMEOUT * HZ / MSEC_PER_SEC);
	DBGLOG(HAL, INFO, "Start SER timer\n");
}

void halHwRecoveryFromError(IN struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo;
	struct mt66xx_chip_info *prChipInfo;
	struct GL_HIF_INFO *prHifInfo;
	struct BUS_INFO *prBusInfo = NULL;
	struct ERR_RECOVERY_CTRL_T *prErrRecoveryCtrl;
	uint32_t u4Status = 0;

	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;
	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
	prErrRecoveryCtrl = &prHifInfo->rErrRecoveryCtl;

	u4Status = prErrRecoveryCtrl->u4Status;
	prErrRecoveryCtrl->u4Status = 0;

	if (prAdapter->rWifiVar.fgEnableSer == FALSE)
		return;

	switch (prErrRecoveryCtrl->eErrRecovState) {
	case ERR_RECOV_STOP_IDLE:
		if (u4Status & ERROR_DETECT_STOP_PDMA) {
			prChipInfo = prAdapter->chip_info;
			if (prChipInfo->asicDumpSerDummyCR)
				prChipInfo->asicDumpSerDummyCR(prAdapter);
			halStartSerTimer(prAdapter);
			DBGLOG(HAL, INFO,
				"SER(E) Host stop PDMA tx/rx ring operation & receive\n");
			nicSerStopTxRx(prAdapter);
#if (CFG_SUPPORT_CONNAC2X == 1)
			/*get WFDMA HW data before Layer 1 SER*/
			/*event*/
			halRxReceiveRFBs(prAdapter, WFDMA1_RX_RING_IDX_0,
						FALSE);
#endif
#if CFG_SUPPORT_MULTITHREAD
			kalSetRxProcessEvent(prAdapter->prGlueInfo);
			DBGLOG(HAL, INFO,
				"SER(F) kalSetRxProcessEvent\n");
#else
			DBGLOG(HAL, INFO,
				"SER(F) nicRxProcessRFBs\n");
			nicRxProcessRFBs(prAdapter);
#endif

			DBGLOG(HAL, INFO,
				"SER(F) Host ACK PDMA tx/rx ring stop operation\n");

			if (prBusInfo->softwareInterruptMcu) {
				prBusInfo->softwareInterruptMcu(prAdapter,
					MCU_INT_PDMA0_STOP_DONE);
			} else {
				kalDevRegWrite(prGlueInfo, HOST2MCU_SW_INT_SET,
					MCU_INT_PDMA0_STOP_DONE);
			}

			/* re-call for change status to stop dma0 */
			prErrRecoveryCtrl->eErrRecovState =
				ERR_RECOV_STOP_PDMA0;
		} else {
			DBGLOG(HAL, ERROR, "SER CurStat=%u Event=%x\n",
			       prErrRecoveryCtrl->eErrRecovState, u4Status);
		}
		break;

	case ERR_RECOV_STOP_PDMA0:
		if (u4Status & ERROR_DETECT_RESET_DONE) {
			DBGLOG(HAL, INFO, "SER(L) Host re-initialize PDMA\n");
			/* only reset TXD & RXD */
			halWpdmaAllocRing(prAdapter->prGlueInfo, false);
			nicFreePendingTxMsduInfo(prAdapter, 0xFF,
					MSDU_REMOVE_BY_ALL);
			wlanClearPendingCommandQueue(prAdapter);
			halResetMsduToken(prAdapter);
			prAdapter->u4NoMoreRfb = 0;

			DBGLOG(HAL, INFO, "SER(M) Host enable PDMA\n");
			halWpdmaInitRing(prGlueInfo, false);

			/* reset SW value after InitRing */
			prChipInfo = prAdapter->chip_info;
			if (prChipInfo->asicWfdmaReInit)
				prChipInfo->asicWfdmaReInit(prAdapter);

			DBGLOG(HAL, INFO,
				"SER(N) Host interrupt MCU PDMA ring init done\n");
			prErrRecoveryCtrl->eErrRecovState =
				ERR_RECOV_RESET_PDMA0;
			if (prBusInfo->softwareInterruptMcu) {
				prBusInfo->softwareInterruptMcu(prAdapter,
					MCU_INT_PDMA0_INIT_DONE);
			} else {
				kalDevRegWrite(prGlueInfo, HOST2MCU_SW_INT_SET,
					MCU_INT_PDMA0_INIT_DONE);
			}
		} else {
			DBGLOG(HAL, ERROR, "SER CurStat=%u Event=%x\n",
			       prErrRecoveryCtrl->eErrRecovState, u4Status);
		}
		break;

	case ERR_RECOV_RESET_PDMA0:
		if (u4Status & ERROR_DETECT_RECOVERY_DONE) {
			DBGLOG(HAL, INFO,
				"SER(Q) Host interrupt MCU SER handle done\n");
			prErrRecoveryCtrl->eErrRecovState =
				ERR_RECOV_WAIT_MCU_NORMAL;
			if (prBusInfo->softwareInterruptMcu) {
				prBusInfo->softwareInterruptMcu(prAdapter,
					MCU_INT_PDMA0_RECOVERY_DONE);
			} else {
				kalDevRegWrite(prGlueInfo, HOST2MCU_SW_INT_SET,
					MCU_INT_PDMA0_RECOVERY_DONE);
			}
		} else {
			DBGLOG(HAL, ERROR, "SER CurStat=%u Event=%x\n",
			       prErrRecoveryCtrl->eErrRecovState, u4Status);
		}
		break;

	case ERR_RECOV_WAIT_MCU_NORMAL:
		if (u4Status & ERROR_DETECT_MCU_NORMAL_STATE) {
			del_timer_sync(&prHifInfo->rSerTimer);

			/* update Beacon frame if operating in AP mode. */
			DBGLOG(HAL, INFO, "SER(T) Host re-initialize BCN\n");
			nicSerReInitBeaconFrame(prAdapter);

			kalDevKickCmd(prAdapter->prGlueInfo);
			kalDevKickData(prAdapter->prGlueInfo);
			halRxReceiveRFBs(prAdapter, RX_RING_EVT_IDX_1, FALSE);
			halRxReceiveRFBs(prAdapter, RX_RING_DATA_IDX_0, TRUE);
			nicSerStartTxRx(prAdapter);
#if CFG_SUPPORT_MULTITHREAD
			kalSetTxEvent2Hif(prAdapter->prGlueInfo);
#endif
			prErrRecoveryCtrl->eErrRecovState = ERR_RECOV_STOP_IDLE;
		} else {
			DBGLOG(HAL, ERROR, "SER CurStat=%u Event=%x\n",
			       prErrRecoveryCtrl->eErrRecovState, u4Status);
		}
		break;

	default:
		DBGLOG(HAL, ERROR, "SER CurStat=%u Event=%x!!!\n",
		       prErrRecoveryCtrl->eErrRecovState, u4Status);
		break;
	}
}

void halDeAggRxPktWorker(struct work_struct *work)
{

}

void halRxTasklet(unsigned long data)
{

}

void halTxCompleteTasklet(unsigned long data)
{

}

/* Hif power off wifi */
uint32_t halHifPowerOffWifi(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct BUS_INFO *prBusInfo = NULL;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prBusInfo = prAdapter->chip_info->bus_info;

	DBGLOG(INIT, INFO, "Power off Wi-Fi!\n");

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

#if defined(_HIF_AXI)
	if (nicProcessISTWithSpecifiedCount(prAdapter, 5) !=
		WLAN_STATUS_NOT_INDICATING)
		DBGLOG(INIT, INFO,
		       "Handle pending interrupt\n");
#endif
	/* Power off Wi-Fi */
	wlanSendNicPowerCtrlCmd(prAdapter, TRUE);

	prHifInfo->fgIsPowerOff = true;

	/* prAdapter->fgWiFiInSleepyState = TRUE; */
	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

	rStatus = wlanCheckWifiFunc(prAdapter, FALSE);

	if (prBusInfo->setPdmaIntMask)
		prBusInfo->setPdmaIntMask(prAdapter->prGlueInfo, FALSE);
	nicDisableInterrupt(prAdapter);

	return rStatus;
}

u_int8_t halIsTxResourceControlEn(IN struct ADAPTER *prAdapter)
{
	return FALSE;
}

void halTxResourceResetHwTQCounter(IN struct ADAPTER *prAdapter)
{
}

uint32_t halGetHifTxPageSize(IN struct ADAPTER *prAdapter)
{
	return HIF_TX_PAGE_SIZE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Check if HIF state is READY for upper layer cfg80211
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (TRUE: ready, FALSE: not ready)
*/
/*----------------------------------------------------------------------------*/
bool halIsHifStateReady(IN struct ADAPTER *prAdapter, uint8_t *pucState)
{
	/* PCIE owner should implement this function */

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Check if HIF state is during supend process
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (TRUE: suspend, reject the caller action. FALSE: not suspend)
*/
/*----------------------------------------------------------------------------*/
bool halIsHifStateSuspend(IN struct ADAPTER *prAdapter)
{
	/* PCIE owner should implement this function */

	return FALSE;
}

void halUpdateTxMaxQuota(IN struct ADAPTER *prAdapter)
{
	struct BUS_INFO *prBusInfo;
	uint32_t u4Ret;
	uint32_t u4Quota;
	uint16_t u2PortIdx;
	bool fgRun;
	uint8_t ucWmmIndex;

	KAL_SPIN_LOCK_DECLARATION();

	prBusInfo = prAdapter->chip_info->bus_info;

	for (ucWmmIndex = 0; ucWmmIndex < prAdapter->ucWmmSetNum;
		ucWmmIndex++) {

		u4Ret = WLAN_STATUS_SUCCESS;
		u2PortIdx = halRingDataSelectByWmmIndex(prAdapter, ucWmmIndex);

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_UPDATE_WMM_QUOTA);
		u4Quota = prAdapter->rWmmQuotaReqCS[ucWmmIndex].u4Quota;
		fgRun = prAdapter->rWmmQuotaReqCS[ucWmmIndex].fgRun;
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_UPDATE_WMM_QUOTA);

		if (fgRun) {
			if (prBusInfo->updateTxRingMaxQuota) {
				u4Ret = prBusInfo->updateTxRingMaxQuota(
						prAdapter, u2PortIdx, u4Quota);
			} else {
				DBGLOG(HAL, INFO,
				"updateTxRingMaxQuota not implemented\n");
				u4Ret = WLAN_STATUS_NOT_ACCEPTED;
			}
		}

		DBGLOG(HAL, INFO,
			"WmmQuota,Run,%u,Wmm,%u,Port,%u,Quota,0x%x,ret=0x%x\n",
			fgRun, ucWmmIndex, u2PortIdx, u4Quota, u4Ret);

		if (u4Ret != WLAN_STATUS_PENDING) {
			KAL_ACQUIRE_SPIN_LOCK(prAdapter,
				SPIN_LOCK_UPDATE_WMM_QUOTA);
			prAdapter->rWmmQuotaReqCS[ucWmmIndex].fgRun
				= false;
			KAL_RELEASE_SPIN_LOCK(prAdapter,
				SPIN_LOCK_UPDATE_WMM_QUOTA);
		}
	}
}

void halEnableSlpProt(struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4Val = 0;
	uint32_t u4WaitDelay = 20000;

	kalDevRegRead(prGlueInfo, CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_ADDR, &u4Val);
	u4Val |= CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_PDMA_AXI_SLPPROT_ENABLE_MASK;
	kalDevRegWrite(prGlueInfo, CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_ADDR, u4Val);
	while (TRUE) {
		u4WaitDelay--;
		kalDevRegRead(prGlueInfo, CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_ADDR,
			&u4Val);
		if (CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_PDMA_AXI_SLPPROT_RDY_MASK &
				u4Val)
			break;
		if (u4WaitDelay == 0) {
			DBGLOG(HAL, ERROR, "wait for sleep protect timeout.\n");
			GL_RESET_TRIGGER(prGlueInfo->prAdapter,
				RST_FLAG_CHIP_RESET);
			break;
		}
		kalUdelay(1);
	}
}

void halDisableSlpProt(struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4Val = 0;

	kalDevRegRead(prGlueInfo, CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_ADDR, &u4Val);
	u4Val &= ~CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_PDMA_AXI_SLPPROT_ENABLE_MASK;
	kalDevRegWrite(prGlueInfo, CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_ADDR, u4Val);
}


