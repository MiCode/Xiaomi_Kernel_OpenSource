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
*[File]             hif_api.c
*[Version]          v1.0
*[Revision Date]    2015-09-08
*[Author]
*[Description]
*    The program provides SDIO HIF APIs
*[Copyright]
*    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#if MTK_WCN_HIF_SDIO
#include "hif_sdio.h"
#else
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>	/* sdio_readl(), etc */
#include <linux/mmc/sdio_ids.h>
#endif

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define RX_RESPONSE_TIMEOUT (3000)

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
/*
 *TX Done Counter Layout
 *
 * enum HIF_TX_COUNT_IDX_T {
 *
 *	 /==== First WMM ====/
 *
 *	 HIF_TXC_IDX_0, ==>AC0 PSE
 *	 HIF_TXC_IDX_1, ==>AC1 PSE
 *	 HIF_TXC_IDX_2, ==>AC2 PSE
 *	 HIF_TXC_IDX_3, ==>AC3 PLE
 *	 HIF_TXC_IDX_4, ==>CPU PSE
 *	 HIF_TXC_IDX_5, ==>AC0 PLE
 *	 HIF_TXC_IDX_6, ==>AC1 PLE
 *	 HIF_TXC_IDX_7, ==>AC2 PLE
 *	 HIF_TXC_IDX_8, ==>AC3 PLE
 *
 *	/==== Second WMM ====/
 *
 *	 HIF_TXC_IDX_9, ==>AC10, AC11 PSE
 *	 HIF_TXC_IDX_10, ==> AC12 PSE
 *	 HIF_TXC_IDX_11, ==>AC13 PSE
 *	 HIF_TXC_IDX_12, ==>AC10, AC11 PLE
 *	 HIF_TXC_IDX_13, ==>AC12 PLE
 *	 HIF_TXC_IDX_14, ==>Reservec
 *	 HIF_TXC_IDX_15, ==>AC13 PLE
 * };
*/
#define HIF_TXC_IDX_2_TC_IDX_PSE(hif_idx) (hif_idx)
#define HIF_TXC_IDX_2_TC_IDX_PLE(hif_idx) (hif_idx - HIF_TXC_IDX_5)
#define TC_IDX_PSE_2_HIF_TXC_IDX(ucTc) (ucTc)
#define TC_IDX_PLE_2_HIF_TXC_IDX(ucTc) (ucTc + HIF_TXC_IDX_5)

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
	uint32_t u4CIR = 0;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	if (prAdapter->fgIsReadRevID)
		return TRUE;

	HAL_MCR_RD(prAdapter, MCR_WCIR, &u4CIR);

	DBGLOG(INIT, TRACE, "Chip ID: 0x%lx\n", u4CIR & WCIR_CHIP_ID);
	DBGLOG(INIT, TRACE, "Revision ID: 0x%lx\n", ((u4CIR & WCIR_REVISION_ID) >> 16));

	prChipInfo = prAdapter->chip_info;

	if ((u4CIR & WCIR_CHIP_ID) != prChipInfo->chip_id)
		return FALSE;

	prAdapter->ucRevID = (uint8_t) (((u4CIR & WCIR_REVISION_ID) >> 16) & 0xF);
	prAdapter->fgIsReadRevID = TRUE;

	return TRUE;
}

uint32_t
halRxWaitResponse(IN struct ADAPTER *prAdapter, IN uint8_t ucPortIdx, OUT uint8_t *pucRspBuffer,
		  IN uint32_t u4MaxRespBufferLen, OUT uint32_t *pu4Length)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4Value = 0, u4PktLen = 0, i = 0, u4CpyLen;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	uint32_t u4Time, u4Current;
	struct RX_CTRL *prRxCtrl;
	struct WIFI_EVENT *prEvent;

	DEBUGFUNC("halRxWaitResponse");

	ASSERT(prAdapter);
	ASSERT(pucRspBuffer);
	prChipInfo = prAdapter->chip_info;

	prRxCtrl = &prAdapter->rRxCtrl;

	u4Time = (uint32_t) kalGetTimeTick();

	do {
		/* Read the packet length */
		HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4Value);

		if ((u4Value & 0xFFFF) != 0) {
			u4PktLen = u4Value & 0xFFFF;
			i = 0;
		} else {
			u4PktLen = (u4Value >> 16) & 0xFFFF;
			i = 1;
		}

		if (u4PktLen > u4MaxRespBufferLen) {
			DBGLOG(RX, ERROR, "Packet length over buffer! Dump Response buffer, length = 0x%x\n",
				*pu4Length);
			prEvent = (struct WIFI_EVENT *)
				(pucRspBuffer + prChipInfo->rxd_size);
			DBGLOG(RX, ERROR, "RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n",
				   prEvent->ucEID, prEvent->ucSeqNum, prEvent->u2PacketLength);
			DBGLOG_MEM8(RX, ERROR, pucRspBuffer, u4MaxRespBufferLen);
			return WLAN_STATUS_FAILURE;
		}

		if (u4PktLen == 0) {
			/* timeout exceeding check */
			u4Current = (uint32_t) kalGetTimeTick();

			if ((u4Current > u4Time) && ((u4Current - u4Time) > RX_RESPONSE_TIMEOUT)) {
				DBGLOG(RX, ERROR, "Timeout! %d - %d = %d\n", u4Current, u4Time, (u4Current-u4Time));
				return WLAN_STATUS_FAILURE;
			} else if (u4Current < u4Time && ((u4Current + (0xFFFFFFFF - u4Time)) > RX_RESPONSE_TIMEOUT)) {
				DBGLOG(RX, ERROR, "Timeout! %d - %d = %d\n",
					u4Current, u4Time, (u4Current + (0xFFFFFFFF - u4Time)));
				return WLAN_STATUS_FAILURE;
			}

			/* Response packet is not ready */
			kalUdelay(50);
		} else {

#if (CFG_ENABLE_READ_EXTRA_4_BYTES == 1)
#if CFG_SDIO_RX_AGG
			/* decide copy length */
			if (u4PktLen > u4MaxRespBufferLen)
				u4CpyLen = u4MaxRespBufferLen;
			else
				u4CpyLen = u4PktLen;

			/* read from SDIO to tmp. buffer */
			HAL_PORT_RD(prAdapter, i == 0 ? MCR_WRDR0 : MCR_WRDR1,
				ALIGN_4(u4PktLen + 4), prRxCtrl->pucRxCoalescingBufPtr,
				HIF_RX_COALESCING_BUFFER_SIZE);

			/* copy to destination buffer */
			kalMemCopy(pucRspBuffer, prRxCtrl->pucRxCoalescingBufPtr, u4CpyLen);

			/* update valid buffer count */
			u4PktLen = u4CpyLen;
#else
#error "Please turn on RX coalescing"
#endif
#else
			HAL_PORT_RD(prAdapter,
				    i == 0 ? MCR_WRDR0 : MCR_WRDR1, u4PktLen, pucRspBuffer, u4MaxRespBufferLen);
#endif
			*pu4Length = u4PktLen;
			break;
		}
	} while (TRUE);

	return u4Status;
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
	u_int8_t fgIsIntEnableCache, fgIsPendingInt;

	ASSERT(prAdapter);
	fgIsIntEnableCache = prAdapter->fgIsIntEnable;
	/* Not to enable interrupt if there is pending interrupt */
	fgIsPendingInt = prAdapter->prGlueInfo->rHifInfo.fgIsPendingInt;

	if (!fgIsPendingInt)
		prAdapter->fgIsIntEnable = TRUE;	/* NOTE(Kevin): It must be placed before MCR GINT write. */

	/* If need enable INT and also set LPOwn at the same time. */
	if (prAdapter->fgIsIntEnableWithLPOwnSet) {
		prAdapter->fgIsIntEnableWithLPOwnSet = FALSE;	/* NOTE(Kevin): It's better to place it
								 * before MCR GINT write.
								 */
		/* If INT was enabled, only set LPOwn */
		if (fgIsIntEnableCache) {
			HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);
			prAdapter->fgIsFwOwn = TRUE;
		}
		/* If INT was not enabled, enable it and also set LPOwn now */
		else if (!fgIsPendingInt) {
			HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET | WHLPCR_INT_EN_SET);
			prAdapter->fgIsFwOwn = TRUE;
		}
	}
	/* If INT was not enabled, enable it now */
	else if (!fgIsIntEnableCache && !fgIsPendingInt)
		HAL_BYTE_WR(prAdapter, MCR_WHLPCR, WHLPCR_INT_EN_SET);

	if (fgIsPendingInt)
		kalSetIntEvent(prAdapter->prGlueInfo);
}				/* end of nicEnableInterrupt() */


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

	ASSERT(prAdapter);

	HAL_BYTE_WR(prAdapter, MCR_WHLPCR, WHLPCR_INT_EN_CLR);

	prAdapter->fgIsIntEnable = FALSE;

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
	u_int8_t fgStatus = TRUE;
	uint32_t i, j, u4CurrTick = 0, u4WriteTick, u4WriteTickTemp;
	u_int8_t fgTimeout;
	u_int8_t fgResult;
	uint32_t u4DriverOwnTime = 0, u4Cr4ReadyTime = 0;
	struct GL_HIF_INFO *prHifInfo;
	u_int8_t fgWmtCoreDump = FALSE;

	ASSERT(prAdapter);

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_SET_OWN);

	GLUE_INC_REF_CNT(prAdapter->u4PwrCtrlBlockCnt);

	if (prAdapter->fgIsFwOwn == FALSE)
		goto unlock;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	DBGLOG(INIT, TRACE, "DRIVER OWN\n");

	u4WriteTick = 0;
	u4CurrTick = kalGetTimeTick();
	i = 0;
	j = 0;

	glWakeupSdio(prAdapter->prGlueInfo);

	while (1) {
		HAL_LP_OWN_RD(prAdapter, &fgResult);

		if (TIME_BEFORE(kalGetTimeTick(), u4CurrTick)) { /* To prevent timer wraparound */
			fgTimeout =
				((kalGetTimeTick() + (~u4CurrTick)) > LP_OWN_BACK_TOTAL_DELAY_MS) ? TRUE : FALSE;
		} else {
			fgTimeout =
				((kalGetTimeTick() - u4CurrTick) > LP_OWN_BACK_TOTAL_DELAY_MS) ? TRUE : FALSE;
		}

		if (fgResult) {
			prAdapter->fgIsFwOwn = FALSE;
			prAdapter->u4OwnFailedCount = 0;
			prAdapter->u4OwnFailedLogCount = 0;

			if (nicSerIsWaitingReset(prAdapter)) {
				DBGLOG(INIT, WARN,
					"[SER][L1] driver own pass\n");

				/* SER is done, start Tx/Rx */
				nicSerStartTxRx(prAdapter);
				nicTxRelease(prAdapter, 0x0);
			}
			break;
		} else if ((i > LP_OWN_BACK_FAILED_RETRY_CNT) &&
			   (kalIsCardRemoved(prAdapter->prGlueInfo) || fgIsBusAccessFailed || fgTimeout
			    || wlanIsChipNoAck(prAdapter))) {

			/* For driver own back fail debug,  get current PC value */
			halPrintMailbox(prAdapter);
			halPollDbgCr(prAdapter, LP_OWN_BACK_FAILED_DBGCR_POLL_ROUND);

			if ((prAdapter->u4OwnFailedCount == 0) ||
			    CHECK_FOR_TIMEOUT(u4CurrTick, prAdapter->rLastOwnFailedLogTime,
					      MSEC_TO_SYSTIME(LP_OWN_BACK_FAILED_LOG_SKIP_MS))) {

				DBGLOG(INIT, ERROR,
				       "LP cannot be own back, Timeout[%u](%ums), BusAccessError[%u]",
				       fgTimeout, kalGetTimeTick() - u4CurrTick, fgIsBusAccessFailed);
				DBGLOG(INIT, ERROR,
				       "Resetting[%u], CardRemoved[%u] NoAck[%u] Cnt[%u] fgCoreDump[%u]\n",
				       kalIsResetting(),
				       kalIsCardRemoved(prAdapter->prGlueInfo), wlanIsChipNoAck(prAdapter),
				       prAdapter->u4OwnFailedCount, fgWmtCoreDump);

				DBGLOG(INIT, INFO,
				       "Skip LP own back failed log for next %ums\n", LP_OWN_BACK_FAILED_LOG_SKIP_MS);

				prAdapter->u4OwnFailedLogCount++;
				if (prAdapter->u4OwnFailedLogCount > LP_OWN_BACK_FAILED_RESET_CNT) {
					/* Trigger RESET */
					glGetRstReason(RST_DRV_OWN_FAIL);
					GL_RESET_TRIGGER(prAdapter,
						RST_FLAG_DO_CORE_DUMP);
				}
				GET_CURRENT_SYSTIME(&prAdapter->rLastOwnFailedLogTime);
			}

			prAdapter->u4OwnFailedCount++;
			fgStatus = FALSE;
			break;
		}

		u4WriteTickTemp = kalGetTimeTick();
		if ((i == 0) || TIME_AFTER(u4WriteTickTemp, (u4WriteTick + LP_OWN_REQ_CLR_INTERVAL_MS))) {
			/* Driver get LP ownership per 200 ms, to avoid iteration time not accurate */
			HAL_LP_OWN_CLR(prAdapter, &fgResult);
			u4WriteTick = u4WriteTickTemp;
		}

		/* Delay for LP engine to complete its operation. */
		kalUsleep_range(LP_OWN_BACK_LOOP_DELAY_MIN_US, LP_OWN_BACK_LOOP_DELAY_MAX_US);
		i++;
	}
	u4DriverOwnTime = ((kalGetTimeTick() >= u4CurrTick) ?
			(kalGetTimeTick() - u4CurrTick) : (kalGetTimeTick() + (~u4CurrTick)));

	/* 1. Driver need to polling until CR4 ready, then could do normal Tx/Rx */
	/* 2. Send a dummy command to change data path to store-forward mode */
	if (prAdapter->fgIsCr4FwDownloaded && prAdapter->fgIsFwDownloaded) {
		const uint32_t ready_bits = prAdapter->chip_info->sw_ready_bits;
		u_int8_t fgReady = FALSE;

		DBGLOG(INIT, INFO, "halSetDriverOwn:: Check ready_bits(=0x%x)\n", ready_bits);
		u4CurrTick = kalGetTimeTick();
		while (1) {
			HAL_WIFI_FUNC_READY_CHECK(prAdapter, ready_bits/*WIFI_FUNC_READY_BITS*/, &fgReady);

			if (TIME_BEFORE(kalGetTimeTick(), u4CurrTick)) { /* To prevent timer wraparound */
				fgTimeout =
					((kalGetTimeTick() + (~u4CurrTick)) > LP_OWN_BACK_TOTAL_DELAY_MS)
						? TRUE : FALSE;
			} else {
				fgTimeout =
					((kalGetTimeTick() - u4CurrTick) > LP_OWN_BACK_TOTAL_DELAY_MS)
						? TRUE : FALSE;
			}

			if (fgReady) {
				break;
			} else if (kalIsCardRemoved(prAdapter->prGlueInfo) || fgIsBusAccessFailed || fgTimeout
			    || wlanIsChipNoAck(prAdapter)) {

				/* For driver own back fail debug,	get current PC value */
				halPrintMailbox(prAdapter);
				halPollDbgCr(prAdapter, LP_OWN_BACK_FAILED_DBGCR_POLL_ROUND);

				DBGLOG(INIT, ERROR,
				       "Resetting[%u], CardRemoved[%u] NoAck[%u] Timeout[%u](%u - %u)ms, fgCoreDump[%u]\n",
				       kalIsResetting(),
				       kalIsCardRemoved(prAdapter->prGlueInfo), wlanIsChipNoAck(prAdapter),
				       fgTimeout, kalGetTimeTick(), u4CurrTick, fgWmtCoreDump);


				DBGLOG(INIT, INFO,
					"Skip waiting CR4 ready for next %ums\n", LP_OWN_BACK_FAILED_LOG_SKIP_MS);
				fgStatus = FALSE;

				if (fgTimeout) {
					/* Trigger RESET */
					glGetRstReason(RST_DRV_OWN_FAIL);
					GL_RESET_TRIGGER(prAdapter,
						RST_FLAG_DO_CORE_DUMP);
				}

				break;
			}
			/* Delay for CR4 to complete its operation. */
			kalUsleep_range(LP_OWN_BACK_LOOP_DELAY_MIN_US, LP_OWN_BACK_LOOP_DELAY_MAX_US);
		}

		halTagIntLog(prAdapter, SDIO_INT_DRV_OWN);
		HAL_MCR_RD(prAdapter, MCR_D2HRM1R, &j);

		if (j == 0x77889901) {

			struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;

			if (halIsPendingTxDone(prAdapter)) {
				/* Workaround for missing Tx done */
				halSerHifReset(prAdapter);
			}

			/* fgIsWakeupFromDeepSleep */
			wlanSendDummyCmd(prAdapter, FALSE);

			/* Workaround for dummy command which is not count in Tx done count */
			if (prChipInfo->is_support_cr4)
				prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[TC4_INDEX]--;
			halTagIntLog(prAdapter, SDIO_INT_WAKEUP_DSLP);
		}
		u4Cr4ReadyTime = ((kalGetTimeTick() >= u4CurrTick) ?
				(kalGetTimeTick() - u4CurrTick) : (kalGetTimeTick() + (~u4CurrTick)));
	}

	DBGLOG(NIC, INFO, "DRIVER OWN %d, %d, DSLP %s, count %d\n",
		u4DriverOwnTime, u4Cr4ReadyTime, ((j == 0x77889901)?"1":"0"), i);
unlock:
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_SET_OWN);
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

	u_int8_t fgResult;

	ASSERT(prAdapter);
	ASSERT(prAdapter->u4PwrCtrlBlockCnt != 0);

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_SET_OWN);

	/* Decrease Block to Enter Low Power Semaphore count */
	GLUE_DEC_REF_CNT(prAdapter->u4PwrCtrlBlockCnt);

	if (prAdapter->u4PwrCtrlBlockCnt != 0) {
		DBGLOG(INIT, TRACE, "prAdapter->u4PwrCtrlBlockCnt = %d\n",
			prAdapter->u4PwrCtrlBlockCnt);
		goto unlock;
	}

	if (prAdapter->fgForceFwOwn == FALSE
		&& prAdapter->fgWiFiInSleepyState == FALSE)
		goto unlock;

	if (prAdapter->fgIsFwOwn == TRUE)
		goto unlock;

	if ((nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) &&
	     !nicSerIsWaitingReset(prAdapter)) {
		DBGLOG(INIT, INFO, "FW OWN Skipped due to pending INT\n");
		/* pending interrupts */
		goto unlock;
	}

	if (fgEnableGlobalInt) {
		prAdapter->fgIsIntEnableWithLPOwnSet = TRUE;
	} else {
		if (nicSerIsWaitingReset(prAdapter))
			DBGLOG(INIT, WARN, "[SER][L1] set fw own\n");

		HAL_LP_OWN_SET(prAdapter, &fgResult);

		if (fgResult) {
			/* if set firmware own not successful (possibly pending
			 * interrupts), indicate an own clear event
			 */
			HAL_LP_OWN_CLR(prAdapter, &fgResult);
		} else {
			prAdapter->fgIsFwOwn = TRUE;

			DBGLOG(INIT, INFO, "FW OWN\n");

			if (nicSerIsWaitingReset(prAdapter))
				DBGLOG(INIT, WARN, "[SER][L1] set fw pass\n");
		}
	}

unlock:
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_SET_OWN);
}

void halWakeUpWiFi(IN struct ADAPTER *prAdapter)
{

	u_int8_t fgResult;

	ASSERT(prAdapter);

	HAL_LP_OWN_RD(prAdapter, &fgResult);

	if (fgResult)
		prAdapter->fgIsFwOwn = FALSE;
	else
		HAL_LP_OWN_CLR(prAdapter, &fgResult);
}

void halDevInit(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Value = 0;

	ASSERT(prAdapter);

#if CFG_SDIO_INTR_ENHANCE
	/* 4 <1> Check STATUS Buffer is DW alignment. */
	ASSERT(IS_ALIGN_4((unsigned long)&prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl->u4WHISR));

	/* 4 <2> Setup STATUS count. */
	{
		HAL_MCR_RD(prAdapter, MCR_WHCR, &u4Value);

		/* 4 <2.1> Setup the number of maximum RX length to be report */
		u4Value &= ~(WHCR_MAX_HIF_RX_LEN_NUM);
		u4Value |= ((SDIO_MAXIMUM_RX_LEN_NUM << WHCR_OFFSET_MAX_HIF_RX_LEN_NUM));

		/* 4 <2.2> Setup RX enhancement mode */
#if CFG_SDIO_RX_ENHANCE
		u4Value |= WHCR_RX_ENHANCE_MODE_EN;
#else
		u4Value &= ~WHCR_RX_ENHANCE_MODE_EN;
#endif /* CFG_SDIO_RX_AGG */

		HAL_MCR_WR(prAdapter, MCR_WHCR, u4Value);
	}
#endif /* CFG_SDIO_INTR_ENHANCE */

	HAL_MCR_WR(prAdapter, MCR_WHIER, WHIER_DEFAULT);

	HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, HIF_RX_MAX_AGG_NUM);
}

void halTxCancelSendingCmd(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo)
{
}

u_int8_t halTxIsDataBufEnough(IN struct ADAPTER *prAdapter, IN struct MSDU_INFO *prMsduInfo)
{
	return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will calculate TC page count according to
*        the given TX_STATUS COUNTER after TX Done.
*
* \param[in] prAdapter              Pointer to the Adapter structure.
* \param[in] au2TxRlsCnt           array of TX STATUS
* \param[in] au2FreeTcResource           array of free & available resource count
*
* @return TRUE      there are available resource to release
* @return FALSE     no available resource to release
*/
/*----------------------------------------------------------------------------*/
u_int8_t halTxCalculateResource(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxRlsCnt, OUT uint16_t *au2FreeTcResource)
{
	struct TX_TCQ_STATUS *prTcqStatus;
	u_int8_t bStatus = FALSE;
	uint8_t ucTcIdx;
	uint32_t u4TotalTxDoneCnt = 0;
	uint32_t u4TotalExtraTxDone = 0;
	uint32_t au4UsedCnt[TC_NUM];
	uint32_t au4ExtraTxDone[TC_NUM];

	uint32_t *au4TxDoneCnt;
	uint32_t *au4PreUsedCnt;
	uint32_t u4AvaliableCnt;
	u_int8_t fgEnExtraTxDone;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prTcqStatus = &prAdapter->rTxCtrl.rTc;

	au4TxDoneCnt = prTcqStatus->au4TxDonePageCount;
	au4PreUsedCnt = prTcqStatus->au4PreUsedPageCount;
	u4AvaliableCnt = prTcqStatus->u4AvaliablePageCount;
	fgEnExtraTxDone = prAdapter->rWifiVar.ucExtraTxDone;

	/* Get used page count */
	if (fgEnExtraTxDone) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
		for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {
			au4UsedCnt[ucTcIdx] = prTcqStatus->au4MaxNumOfPage[ucTcIdx] -
			    prTcqStatus->au4FreePageCount[ucTcIdx];
		}
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
	}

	/* Get Tx done & available page count */
	u4AvaliableCnt += au2TxRlsCnt[HIF_TX_FFA_INDEX];
	for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {

		/* Get Tx done count from Tx interrupt status */
		au4TxDoneCnt[ucTcIdx] += au2TxRlsCnt[nicTxGetTxQByTc(prAdapter, ucTcIdx)];

		/* Get available EXTRA Tx done */
		if (fgEnExtraTxDone) {
			/* Release Tx done if there are pre-used resource */
			if (au4TxDoneCnt[ucTcIdx] >= au4PreUsedCnt[ucTcIdx]) {
				au4TxDoneCnt[ucTcIdx] -= au4PreUsedCnt[ucTcIdx];
				au4PreUsedCnt[ucTcIdx] = 0;
			} else {
				au4PreUsedCnt[ucTcIdx] -= au4TxDoneCnt[ucTcIdx];
				au4TxDoneCnt[ucTcIdx] = 0;
			}

			/* Calculate extra Tx done to share rest FFA resource */
			if (au4TxDoneCnt[ucTcIdx] >= au4UsedCnt[ucTcIdx]) {
				au4TxDoneCnt[ucTcIdx] = au4UsedCnt[ucTcIdx];
				au4ExtraTxDone[ucTcIdx] = 0;
			} else {
				au4ExtraTxDone[ucTcIdx] = au4UsedCnt[ucTcIdx] - au4TxDoneCnt[ucTcIdx];
			}
			u4TotalExtraTxDone += au4ExtraTxDone[ucTcIdx];
		}

		u4TotalTxDoneCnt += au4TxDoneCnt[ucTcIdx];

	}

	DBGLOG(TX, TRACE, "TxDone result, FFA[%u] AC[%u:%u:%u:%u] CPU[%u]\n",
		au2TxRlsCnt[HIF_TX_FFA_INDEX], au2TxRlsCnt[HIF_TX_AC0_INDEX],
		au2TxRlsCnt[HIF_TX_AC1_INDEX], au2TxRlsCnt[HIF_TX_AC2_INDEX],
		au2TxRlsCnt[HIF_TX_AC3_INDEX], au2TxRlsCnt[HIF_TX_CPU_INDEX]);

	DBGLOG(TX, TRACE, "TxDone Page count, TC[%u:%u:%u:%u:%u]\n",
		au4TxDoneCnt[TC0_INDEX], au4TxDoneCnt[TC1_INDEX], au4TxDoneCnt[TC2_INDEX],
		au4TxDoneCnt[TC3_INDEX], au4TxDoneCnt[TC4_INDEX]);

	/* Calculate free Tc page count */
	if (u4AvaliableCnt && u4TotalTxDoneCnt) {
		/* Distribute resource by Tx done counter */
		if (u4AvaliableCnt >= u4TotalTxDoneCnt) {
			/* Fulfill all TC resource */
			kalMemCopy(au2FreeTcResource, prTcqStatus->au4TxDonePageCount,
				   sizeof(prTcqStatus->au4TxDonePageCount));

			kalMemZero(prTcqStatus->au4TxDonePageCount, sizeof(prTcqStatus->au4TxDonePageCount));

			u4AvaliableCnt -= u4TotalTxDoneCnt;
		} else {
			/* Round-robin distribute resource */
			ucTcIdx = prTcqStatus->ucNextTcIdx;
			while (u4AvaliableCnt) {
				/* Enough resource, fulfill this TC */
				if (u4AvaliableCnt >= au4TxDoneCnt[ucTcIdx]) {
					au2FreeTcResource[ucTcIdx] = au4TxDoneCnt[ucTcIdx];
					u4AvaliableCnt -= au4TxDoneCnt[ucTcIdx];
					au4TxDoneCnt[ucTcIdx] = 0;

					/* Round-robin get next TC */
					ucTcIdx++;
					ucTcIdx %= TC_NUM;
				}
				/* no more resource, distribute rest of resource to this TC */
				else {
					au2FreeTcResource[ucTcIdx] = u4AvaliableCnt;
					au4TxDoneCnt[ucTcIdx] -= u4AvaliableCnt;
					u4AvaliableCnt = 0;
				}
			}
			prTcqStatus->ucNextTcIdx = ucTcIdx;
		}
		bStatus = TRUE;
	}

	if (u4AvaliableCnt && u4TotalExtraTxDone && fgEnExtraTxDone) {
		/* Distribute resource by EXTRA Tx done counter */
		if (u4AvaliableCnt >= u4TotalExtraTxDone) {
			for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {
				au2FreeTcResource[ucTcIdx] += au4ExtraTxDone[ucTcIdx];
				au4PreUsedCnt[ucTcIdx] += au4ExtraTxDone[ucTcIdx];
				au4ExtraTxDone[ucTcIdx] = 0;
			}

			u4AvaliableCnt -= u4TotalExtraTxDone;
		} else {
			/* Round-robin distribute resource */
			ucTcIdx = prTcqStatus->ucNextTcIdx;
			while (u4AvaliableCnt) {
				/* Enough resource, fulfill this TC */
				if (u4AvaliableCnt >= au4ExtraTxDone[ucTcIdx]) {
					au2FreeTcResource[ucTcIdx] += au4ExtraTxDone[ucTcIdx];
					au4PreUsedCnt[ucTcIdx] += au4ExtraTxDone[ucTcIdx];
					u4AvaliableCnt -= au4ExtraTxDone[ucTcIdx];
					au4ExtraTxDone[ucTcIdx] = 0;

					/* Round-robin get next TC */
					ucTcIdx++;
					ucTcIdx %= TC_NUM;
				}
				/* no more resource, distribute rest of resource to this TC */
				else {
					au2FreeTcResource[ucTcIdx] += u4AvaliableCnt;
					au4PreUsedCnt[ucTcIdx] += u4AvaliableCnt;
					au4ExtraTxDone[ucTcIdx] -= u4AvaliableCnt;
					u4AvaliableCnt = 0;
				}
			}
			prTcqStatus->ucNextTcIdx = ucTcIdx;
		}
		bStatus = TRUE;
	}

	prTcqStatus->u4AvaliablePageCount = u4AvaliableCnt;

	return bStatus;
}
u_int8_t halTxReleaseResource(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxRlsCnt)
{
	struct TX_TCQ_STATUS *prTcqStatus;
	u_int8_t bStatus = FALSE;
	uint32_t i;
	struct SDIO_STAT_COUNTER *prStatCnt;
	uint16_t au2TxDoneCnt[HIF_TX_NUM] = { 0 };

	ASSERT(prAdapter);
	prTcqStatus = &prAdapter->rTxCtrl.rTc;
	prStatCnt = &prAdapter->prGlueInfo->rHifInfo.rStatCounter;

	/* Update Free Tc resource counter */
	halTxGetFreeResource(prAdapter, au2TxDoneCnt, au2TxRlsCnt);

	/* Return free Tc page count */
	halTxReturnFreeResource(prAdapter, au2TxDoneCnt);
	bStatus = TRUE;

	/* Update Statistic counter */
	prStatCnt->u4TxDonePendingPktCnt += nicTxGetMsduPendingCnt(prAdapter);
	prStatCnt->u4TxDoneIntTotCnt++;

	for (i = HIF_TXC_IDX_0; i < HIF_TXC_IDX_NUM; i++) {
		if (au2TxRlsCnt[i]) {
			prStatCnt->u4TxDoneCnt[i] += au2TxRlsCnt[i];
			prStatCnt->u4TxDoneIntCnt[i]++;
		}
	}

	if (!nicTxSanityCheckResource(prAdapter))
		DBGLOG(TX, ERROR, "Tx Done INT result, FFA[%u] AC[%u:%u:%u:%u] CPU[%u]\n",
			au2TxRlsCnt[HIF_TX_FFA_INDEX], au2TxRlsCnt[HIF_TX_AC0_INDEX],
			au2TxRlsCnt[HIF_TX_AC1_INDEX], au2TxRlsCnt[HIF_TX_AC2_INDEX],
			au2TxRlsCnt[HIF_TX_AC3_INDEX], au2TxRlsCnt[HIF_TX_CPU_INDEX]);

	DBGLOG(TX, LOUD, "TCQ Status Free Page <<PSE>>:Buf[%u:%u, %u:%u, %u:%u, %u:%u, %u:%u]\n",
		prTcqStatus->au4FreePageCount[TC0_INDEX], prTcqStatus->au4FreeBufferCount[TC0_INDEX],
		prTcqStatus->au4FreePageCount[TC1_INDEX], prTcqStatus->au4FreeBufferCount[TC1_INDEX],
		prTcqStatus->au4FreePageCount[TC2_INDEX], prTcqStatus->au4FreeBufferCount[TC2_INDEX],
		prTcqStatus->au4FreePageCount[TC3_INDEX], prTcqStatus->au4FreeBufferCount[TC3_INDEX],
		prTcqStatus->au4FreePageCount[TC4_INDEX], prTcqStatus->au4FreeBufferCount[TC4_INDEX]);


	if (prAdapter->rTxCtrl.rTc.fgNeedPleCtrl)
		DBGLOG(TX, LOUD, "TCQ Status Free Page <<PLE>>:Buf[%u:%u, %u:%u, %u:%u, %u:%u, %u:%u]\n",
			prTcqStatus->au4FreePageCount_PLE[TC0_INDEX], prTcqStatus->au4FreePageCount_PLE[TC0_INDEX],
			prTcqStatus->au4FreePageCount_PLE[TC1_INDEX], prTcqStatus->au4FreePageCount_PLE[TC1_INDEX],
			prTcqStatus->au4FreePageCount_PLE[TC2_INDEX], prTcqStatus->au4FreePageCount_PLE[TC2_INDEX],
			prTcqStatus->au4FreePageCount_PLE[TC3_INDEX], prTcqStatus->au4FreePageCount_PLE[TC3_INDEX],
			prTcqStatus->au4FreePageCount_PLE[TC4_INDEX], prTcqStatus->au4FreePageCount_PLE[TC4_INDEX]);

	return bStatus;
}

uint32_t halTxPollingResource(IN struct ADAPTER *prAdapter, IN uint8_t ucTC)
{
	struct TX_CTRL *prTxCtrl;
	uint32_t u4Status = WLAN_STATUS_RESOURCES;
	uint32_t au4WTSR[8];
	struct GL_HIF_INFO *prHifInfo;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	prTxCtrl = &prAdapter->rTxCtrl;

	if (prHifInfo->fgIsPendingInt && (prHifInfo->prSDIOCtrl->u4WHISR & WHISR_TX_DONE_INT)) {
		/* Get Tx done resource from pending interrupt status */
		kalMemCopy(au4WTSR, &prHifInfo->prSDIOCtrl->rTxInfo, sizeof(uint32_t) * 8);

		/* Clear pending Tx done interrupt */
		prHifInfo->prSDIOCtrl->u4WHISR &= ~WHISR_TX_DONE_INT;
	} else
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au4WTSR);

	if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
		u4Status = WLAN_STATUS_FAILURE;
	} else if (halTxReleaseResource(prAdapter, (uint16_t *) au4WTSR)) {
		if (prTxCtrl->rTc.au4FreeBufferCount[ucTC] > 0)
			u4Status = WLAN_STATUS_SUCCESS;
	}

	return u4Status;
}

void halTxInterruptSanityCheck(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxRlsCnt)
{
	uint8_t ucIdx;
	u_int8_t fgError = FALSE;

	if (prAdapter->rWifiVar.ucTxDbg & BIT(1)) {
		for (ucIdx = HIF_TX_AC0_INDEX; ucIdx < HIF_TX_NUM; ucIdx++) {
			if (au2TxRlsCnt[ucIdx] > prAdapter->rTxCtrl.u4TotalPageNum)
				fgError = TRUE;
		}

		if (fgError)
			DBGLOG(TX, ERROR, "Tx Done INT result, FFA[%u] AC[%u:%u:%u:%u] CPU[%u]\n",
			       au2TxRlsCnt[HIF_TX_FFA_INDEX], au2TxRlsCnt[HIF_TX_AC0_INDEX],
			       au2TxRlsCnt[HIF_TX_AC1_INDEX], au2TxRlsCnt[HIF_TX_AC2_INDEX],
			       au2TxRlsCnt[HIF_TX_AC3_INDEX], au2TxRlsCnt[HIF_TX_CPU_INDEX]);
	}
}

#if CFG_SDIO_INTR_ENHANCE
void halProcessEnhanceInterruptStatus(IN struct ADAPTER *prAdapter)
{
	struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl;

	/* Set Tx done interrupt if there are Tx done count */
	if ((prSDIOCtrl->u4WHISR & WHISR_TX_DONE_INT) == 0 &&
		(prSDIOCtrl->rTxInfo.au4WTSR[0] | prSDIOCtrl->rTxInfo.au4WTSR[1] |
		prSDIOCtrl->rTxInfo.au4WTSR[2] | prSDIOCtrl->rTxInfo.au4WTSR[3] |
		prSDIOCtrl->rTxInfo.au4WTSR[4] | prSDIOCtrl->rTxInfo.au4WTSR[5] |
		prSDIOCtrl->rTxInfo.au4WTSR[6] | prSDIOCtrl->rTxInfo.au4WTSR[7])) {

		prSDIOCtrl->u4WHISR |= WHISR_TX_DONE_INT;
	}

	/* Set SW ASSERT INFO interrupt if there are pending mail box */
	if (((prSDIOCtrl->u4WHISR & WHISR_D2H_SW_ASSERT_INFO_INT) == 0) &&
		HAL_GET_MAILBOX_READ_CLEAR(prAdapter) &&
		(prSDIOCtrl->u4RcvMailbox0 || prSDIOCtrl->u4RcvMailbox1)) {

		prSDIOCtrl->u4WHISR |= WHISR_D2H_SW_ASSERT_INFO_INT;
	}
}
#endif

void halProcessTxInterrupt(IN struct ADAPTER *prAdapter)
{
	struct TX_CTRL *prTxCtrl;
#if CFG_SDIO_INTR_ENHANCE
	struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl;
#else
	uint32_t au4TxCount[2];
#endif /* CFG_SDIO_INTR_ENHANCE */
	SDIO_TIME_INTERVAL_DEC();

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	SDIO_REC_TIME_START();

	/* Get the TX STATUS */
#if CFG_SDIO_INTR_ENHANCE

	prSDIOCtrl = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl;
#if DBG
	/* DBGLOG_MEM8(RX, TRACE, (PUINT_8)prSDIOCtrl, sizeof(SDIO_CTRL_T)); */
#endif

	halTxInterruptSanityCheck(prAdapter, (uint16_t *)&prSDIOCtrl->rTxInfo);
	halTxReleaseResource(prAdapter, (uint16_t *)&prSDIOCtrl->rTxInfo);
	kalMemZero(&prSDIOCtrl->rTxInfo, sizeof(prSDIOCtrl->rTxInfo));

#else

	HAL_MCR_RD(prAdapter, MCR_WTSR0, &au4TxCount[0]);
	HAL_MCR_RD(prAdapter, MCR_WTSR1, &au4TxCount[1]);
	DBGLOG(EMU, TRACE, "MCR_WTSR0: 0x%x, MCR_WTSR1: 0x%x\n", au4TxCount[0], au4TxCount[1]);

	halTxReleaseResource(prAdapter, (uint8_t *) au4TxCount);

#endif /* CFG_SDIO_INTR_ENHANCE */

	nicTxAdjustTcq(prAdapter);
	kalSetTxEvent2Hif(prAdapter->prGlueInfo);

	SDIO_REC_TIME_END();
	SDIO_ADD_TIME_INTERVAL(prAdapter->prGlueInfo->rHifInfo.rStatCounter.u4TxDoneIntTime);

}				/* end of nicProcessTxInterrupt() */

#if !CFG_SDIO_INTR_ENHANCE
/*----------------------------------------------------------------------------*/
/*!
* @brief Read the rx data from data port and setup RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @retval WLAN_STATUS_SUCCESS: SUCCESS
* @retval WLAN_STATUS_FAILURE: FAILURE
*
*/
/*----------------------------------------------------------------------------*/
uint32_t halRxReadBuffer(IN struct ADAPTER *prAdapter, IN OUT struct SW_RFB *prSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	uint8_t *pucBuf;
	struct HW_MAC_RX_DESC *prRxStatus;
	uint32_t u4PktLen = 0, u4ReadBytes;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	u_int8_t fgResult = TRUE;
	uint32_t u4RegValue;
	uint32_t rxNum;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	uint32_t *pu4HwAppendDW;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	DEBUGFUNC("halRxReadBuffer");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	pucBuf = prSwRfb->pucRecvBuff;
	prRxStatus = prSwRfb->prRxStatus;

	ASSERT(prRxStatus);
	ASSERT(pucBuf);
	DBGLOG(RX, TRACE, "pucBuf= 0x%x, prRxStatus= 0x%x\n", pucBuf, prRxStatus);

	do {
		/* Read the RFB DW length and packet length */
		HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4RegValue);
		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read RX Packet Lentgh Error\n");
			return WLAN_STATUS_FAILURE;
		}
		/* 20091021 move the line to get the HIF RX header (for RX0/1) */
		if (u4RegValue == 0) {
			DBGLOG(RX, ERROR, "No RX packet\n");
			return WLAN_STATUS_FAILURE;
		}

		u4PktLen = u4RegValue & BITS(0, 15);
		if (u4PktLen != 0) {
			rxNum = 0;
		} else {
			rxNum = 1;
			u4PktLen = (u4RegValue & BITS(16, 31)) >> 16;
		}

		DBGLOG(RX, TRACE, "RX%d: u4PktLen = %d\n", rxNum, u4PktLen);

		/* 4 <4> Read Entire RFB and packet, include HW appended DW (Checksum Status) */
		u4ReadBytes = ALIGN_4(u4PktLen) + 4;
		HAL_READ_RX_PORT(prAdapter, rxNum, u4ReadBytes, pucBuf, CFG_RX_MAX_PKT_SIZE);

		/* 20091021 move the line to get the HIF RX header */
		/* u4PktLen = (UINT_32)prHifRxHdr->u2PacketLen; */
		if (u4PktLen != (uint32_t) HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus)) {
			DBGLOG(RX, ERROR, "Read u4PktLen = %d, prHifRxHdr->u2PacketLen: %d\n",
			       u4PktLen, HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus));
#if DBG
			DBGLOG_MEM8(RX, TRACE, (uint8_t *) prRxStatus,
				    (HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus) >
				     4096) ? 4096 : prRxStatus->u2RxByteCount);
#endif
			ASSERT(0);
		}
		/* u4PktLen is byte unit, not inlude HW appended DW */

		prSwRfb->ucPacketType = (uint8_t) HAL_RX_STATUS_GET_PKT_TYPE(prRxStatus);
		DBGLOG(RX, TRACE, "ucPacketType = %d\n", prSwRfb->ucPacketType);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		pu4HwAppendDW = (uint32_t *) prRxStatus;
		pu4HwAppendDW += (ALIGN_4(prRxStatus->u2RxByteCount) >> 2);
		prSwRfb->u4TcpUdpIpCksStatus = *pu4HwAppendDW;
		DBGLOG(RX, TRACE, "u4TcpUdpIpCksStatus[0x%02x]\n", prSwRfb->u4TcpUdpIpCksStatus);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

		prSwRfb->ucStaRecIdx =
		    secGetStaIdxByWlanIdx(prAdapter, (uint8_t) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus));

		/* fgResult will be updated in MACRO */
		if (!fgResult)
			return WLAN_STATUS_FAILURE;

		DBGLOG(RX, TRACE, "Dump RX buffer, length = 0x%x\n", u4ReadBytes);
		DBGLOG_MEM8(RX, TRACE, pucBuf, u4ReadBytes);
	} while (FALSE);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port, fill RFB
*        and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter   Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void halRxSDIOReceiveRFBs(IN struct ADAPTER *prAdapter)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	struct HW_MAC_RX_DESC *prRxStatus;
	uint32_t u4HwAppendDW;
	uint32_t *pu4Temp;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("halRxSDIOReceiveRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		if (!prSwRfb) {
			DBGLOG(RX, TRACE, "No More RFB\n");
			break;
		}
		/* need to consider */
		if (halRxReadBuffer(prAdapter, prSwRfb) == WLAN_STATUS_FAILURE) {
			DBGLOG(RX, TRACE, "halRxFillRFB failed\n");
			nicRxReturnRFB(prAdapter, prSwRfb);
			break;
		}

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
		RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		prRxStatus = prSwRfb->prRxStatus;
		ASSERT(prRxStatus);

		pu4Temp = (uint32_t *) prRxStatus;
		u4HwAppendDW = *(pu4Temp + (ALIGN_4(prRxStatus->u2RxByteCount) >> 2));
		DBGLOG(RX, TRACE, "u4HwAppendDW = 0x%x\n", u4HwAppendDW);
		DBGLOG(RX, TRACE, "u2PacketLen = 0x%x\n", HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus));
	} while (FALSE);

}				/* end of nicReceiveRFBs() */

#else
/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port, fill RFB
*        and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param u4DataPort     Specify which port to read
* @param u2RxLength     Specify to the the rx packet length in Byte.
* @param prSwRfb        the RFB to receive rx data.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

uint32_t
halRxEnhanceReadBuffer(IN struct ADAPTER *prAdapter,
		       IN uint32_t u4DataPort, IN uint16_t u2RxLength, IN OUT struct SW_RFB *prSwRfb)
{
	struct RX_CTRL *prRxCtrl;
	uint8_t *pucBuf;
	struct HW_MAC_RX_DESC *prRxStatus;
	uint32_t u4PktLen = 0;
	uint32_t u4Status = WLAN_STATUS_FAILURE;
	u_int8_t fgResult = TRUE;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	uint32_t *pu4HwAppendDW;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	DEBUGFUNC("halRxEnhanceReadBuffer");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	pucBuf = prSwRfb->pucRecvBuff;
	ASSERT(pucBuf);

	prRxStatus = prSwRfb->prRxStatus;
	ASSERT(prRxStatus);

	/* DBGLOG(RX, TRACE, ("u2RxLength = %d\n", u2RxLength)); */

	do {
		/* 4 <1> Read RFB frame from MCR_WRDR0, include HW appended DW */
		HAL_READ_RX_PORT(prAdapter,
				 u4DataPort, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN), pucBuf, CFG_RX_MAX_PKT_SIZE);

		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read RX Packet Lentgh Error\n");
			break;
		}

		u4PktLen = (uint32_t) (HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus));
		/* DBGLOG(RX, TRACE, ("u4PktLen = %d\n", u4PktLen)); */

		prSwRfb->ucPacketType = (uint8_t) HAL_RX_STATUS_GET_PKT_TYPE(prRxStatus);
		/* DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType)); */

		prSwRfb->ucStaRecIdx =
		    secGetStaIdxByWlanIdx(prAdapter, (uint8_t) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus));

		/* 4 <2> if the RFB dw size or packet size is zero */
		if (u4PktLen == 0) {
			DBGLOG(RX, ERROR, "Packet Length = %u\n",
				u4PktLen);
			ASSERT(0);
			break;
		}
		/* 4 <3> if the packet is too large or too small */
		/* ToDo[6630]: adjust CFG_RX_MAX_PKT_SIZE */
		if (u4PktLen > CFG_RX_MAX_PKT_SIZE) {
			DBGLOG(RX, TRACE, "Read RX Packet Lentgh Error (%u)\n",
				u4PktLen);
			ASSERT(0);
			break;
		}

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		pu4HwAppendDW = (uint32_t *) prRxStatus;
		pu4HwAppendDW += (ALIGN_4(prRxStatus->u2RxByteCount) >> 2);
		prSwRfb->u4TcpUdpIpCksStatus = *pu4HwAppendDW;
		DBGLOG(RX, TRACE, "u4TcpUdpIpCksStatus[0x%02x]\n", prSwRfb->u4TcpUdpIpCksStatus);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

		u4Status = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	DBGLOG_MEM8(RX, TRACE, pucBuf, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN));
	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port for SDIO
*        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void halRxSDIOEnhanceReceiveRFBs(IN struct ADAPTER *prAdapter)
{
	struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl;
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	uint32_t i, rxNum;
	uint16_t u2RxPktNum, u2RxLength = 0, u2Tmp = 0;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("halRxSDIOEnhanceReceiveRFBs");

	ASSERT(prAdapter);

	prSDIOCtrl = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl;
	ASSERT(prSDIOCtrl);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	for (rxNum = 0; rxNum < 2; rxNum++) {
		u2RxPktNum =
		    (rxNum == 0 ? prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len : prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len);

		if (u2RxPktNum == 0)
			continue;

		for (i = 0; i < u2RxPktNum; i++) {
			if (rxNum == 0) {
				/* HAL_READ_RX_LENGTH */
				HAL_READ_RX_LENGTH(prAdapter, &u2RxLength, &u2Tmp);
			} else if (rxNum == 1) {
				/* HAL_READ_RX_LENGTH */
				HAL_READ_RX_LENGTH(prAdapter, &u2Tmp, &u2RxLength);
			}

			if (!u2RxLength)
				break;

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
			QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, struct SW_RFB *);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

			if (!prSwRfb) {
				DBGLOG(RX, TRACE, "No More RFB\n");
				break;
			}
			ASSERT(prSwRfb);

			if (halRxEnhanceReadBuffer(prAdapter, rxNum, u2RxLength, prSwRfb) == WLAN_STATUS_FAILURE) {
				DBGLOG(RX, TRACE, "nicRxEnhanceRxReadBuffer failed\n");
				nicRxReturnRFB(prAdapter, prSwRfb);
				break;
			}
			/* prSDIOCtrl->au4RxLength[i] = 0; */

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
			RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		}
	}

	prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len = 0;
	prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len = 0;

}				/* end of nicRxSDIOReceiveRFBs() */

#endif /* CFG_SDIO_INTR_ENHANCE */

#if CFG_SDIO_RX_AGG
/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port for SDIO with Rx aggregation enabled
*        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void halRxSDIOAggReceiveRFBs(IN struct ADAPTER *prAdapter)
{
	struct ENHANCE_MODE_DATA_STRUCT *prEnhDataStr;
	struct RX_CTRL *prRxCtrl;
	uint32_t u4RxLength;
	uint32_t i, rxNum;
	uint32_t u4RxAggCount = 0, u4RxAggLength = 0;
	uint32_t u4RxAvailAggLen;
	uint8_t *pucSrcAddr;
	uint16_t u2RxPktNum;
	struct GL_HIF_INFO *prHifInfo;
	struct SDIO_RX_COALESCING_BUF *prRxBuf;
	u_int8_t fgNoFreeBuf = FALSE;

	SDIO_TIME_INTERVAL_DEC();

	DEBUGFUNC("halRxSDIOAggReceiveRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prEnhDataStr = prHifInfo->prSDIOCtrl;

	if (prEnhDataStr->rRxInfo.u.u2NumValidRx0Len == 0 && prEnhDataStr->rRxInfo.u.u2NumValidRx1Len == 0)
		return;

	for (rxNum = 0; rxNum < 2; rxNum++) {
		u2RxPktNum = (rxNum == 0 ? prEnhDataStr->rRxInfo.u.u2NumValidRx0Len :
			prEnhDataStr->rRxInfo.u.u2NumValidRx1Len);

		if (u2RxPktNum > HIF_RX_MAX_AGG_NUM) {
			DBGLOG(RX, ERROR,
			"[%s] u2RxPktNum (%d) > HIF_RX_MAX_AGG_NUM\n",
			__func__, u2RxPktNum);

			halProcessAbnormalInterrupt(prAdapter);
			GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP);
			return;
		}

		if (u2RxPktNum == 0)
			continue;

#if CFG_HIF_STATISTICS
		prRxCtrl->u4TotalRxAccessNum++;
		prRxCtrl->u4TotalRxPacketNum += u2RxPktNum;
#endif

		mutex_lock(&prHifInfo->rRxFreeBufQueMutex);
		fgNoFreeBuf = QUEUE_IS_EMPTY(&prHifInfo->rRxFreeBufQueue);
		mutex_unlock(&prHifInfo->rRxFreeBufQueMutex);

		if (fgNoFreeBuf) {
			DBGLOG(RX, TRACE, "[%s] No free Rx buffer\n", __func__);
			prHifInfo->rStatCounter.u4RxBufUnderFlowCnt++;

			if (prAdapter->prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
				struct QUE rTempQue;
				struct QUE *prTempQue = &rTempQue;

				/* During halt state, move all pending Rx buffer to free queue */
				mutex_lock(&prHifInfo->rRxDeAggQueMutex);
				QUEUE_MOVE_ALL(prTempQue, &prHifInfo->rRxDeAggQueue);
				mutex_unlock(&prHifInfo->rRxDeAggQueMutex);

				mutex_lock(&prHifInfo->rRxFreeBufQueMutex);
				QUEUE_CONCATENATE_QUEUES(&prHifInfo->rRxFreeBufQueue, prTempQue);
				mutex_unlock(&prHifInfo->rRxFreeBufQueMutex);
			}

			continue;
		}

		u4RxAvailAggLen = HIF_RX_COALESCING_BUFFER_SIZE;
#if CFG_SDIO_RX_ENHANCE
		u4RxAvailAggLen -= (sizeof(struct ENHANCE_MODE_DATA_STRUCT) + HIF_RX_ENHANCE_MODE_PAD_LEN);
#endif
		u4RxAggCount = 0;

		for (i = 0; i < u2RxPktNum; i++) {
			u4RxLength = (rxNum == 0 ? (uint32_t) prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
				(uint32_t) prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

			if (!u4RxLength) {
				DBGLOG(RX, ERROR, "[%s] RxLength == 0\n", __func__);
				halProcessAbnormalInterrupt(prAdapter);
				GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP);
				return;
			}

			if (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN) < u4RxAvailAggLen) {
				u4RxAvailAggLen -= ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN);
				u4RxAggCount++;
			} else {
				/* CFG_RX_COALESCING_BUFFER_SIZE is not large enough */
				DBGLOG(RX, ERROR, "[%s] Request_len(%d) >= Available_len(%d)\n",
					__func__, (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN)), u4RxAvailAggLen);

				halProcessAbnormalInterrupt(prAdapter);
				GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP);
				return;
			}
		}

		mutex_lock(&prHifInfo->rRxFreeBufQueMutex);
		QUEUE_REMOVE_HEAD(&prHifInfo->rRxFreeBufQueue, prRxBuf, struct SDIO_RX_COALESCING_BUF *);
		mutex_unlock(&prHifInfo->rRxFreeBufQueMutex);

		prRxBuf->u4PktCount = u4RxAggCount;

		u4RxAggLength = (HIF_RX_COALESCING_BUFFER_SIZE - u4RxAvailAggLen);

		prRxBuf->u4PktTotalLength = u4RxAggLength - sizeof(struct ENHANCE_MODE_DATA_STRUCT);

		prRxBuf->u4IntLogIdx = prHifInfo->u4IntLogIdx;

		SDIO_REC_TIME_START();
		HAL_READ_RX_PORT(prAdapter, rxNum, u4RxAggLength,
			prRxBuf->pvRxCoalescingBuf, HIF_RX_COALESCING_BUFFER_SIZE);
		SDIO_REC_TIME_END();
		SDIO_ADD_TIME_INTERVAL(prHifInfo->rStatCounter.u4PortReadTime);

#if CFG_SDIO_RX_ENHANCE
		pucSrcAddr = prRxBuf->pvRxCoalescingBuf + u4RxAggLength - sizeof(struct ENHANCE_MODE_DATA_STRUCT);

		/* Sanity check of zero padding before interrupt status */
		if (((uint32_t)*(pucSrcAddr - HIF_RX_ENHANCE_MODE_PAD_LEN)) == 0) {
			kalMemCopy(prHifInfo->prSDIOCtrl, pucSrcAddr, sizeof(struct ENHANCE_MODE_DATA_STRUCT));

			halProcessEnhanceInterruptStatus(prAdapter);

			if (prHifInfo->prSDIOCtrl->u4WHISR) {
				/* Interrupt status without Rx done */
				/* Mask Rx done interrupt to avoid recurrsion */
				uint32_t u4IntStatus = prHifInfo->prSDIOCtrl->u4WHISR &
					(~(WHISR_RX0_DONE_INT | WHISR_RX1_DONE_INT));

				if ((rxNum == 0) && prEnhDataStr->rRxInfo.u.u2NumValidRx1Len && u4IntStatus) {
					/* Handle interrupt here if there are pending Rx port1 */

					nicProcessIST_impl(prAdapter, u4IntStatus);
				} else {
					prAdapter->prGlueInfo->rHifInfo.fgIsPendingInt = TRUE;
				}
			}
		}
#endif
		halDeAggRxPkt(prAdapter, prRxBuf);

		/* Update statistic counter */
		prHifInfo->rStatCounter.u4PktReadCnt[rxNum] += u4RxAggCount;
		prHifInfo->rStatCounter.u4PortReadCnt[rxNum]++;
	}

}
#endif /* CFG_SDIO_RX_AGG */


void halProcessRxInterrupt(IN struct ADAPTER *prAdapter)
{
	if (prAdapter->prGlueInfo->rHifInfo.fgSkipRx)
		return;

#if CFG_SDIO_INTR_ENHANCE
#if CFG_SDIO_RX_AGG
	halRxSDIOAggReceiveRFBs(prAdapter);
#else
	halRxSDIOEnhanceReceiveRFBs(prAdapter);
#endif
#else
	halRxSDIOReceiveRFBs(prAdapter);
#endif /* CFG_SDIO_INTR_ENHANCE */
}

bool halHifSwInfoInit(IN struct ADAPTER *prAdapter)
{
	return true;
}

void halRxProcessMsduReport(IN struct ADAPTER *prAdapter, IN OUT struct SW_RFB *prSwRfb)
{

}

uint32_t halTxGetPageCountPSE(IN struct ADAPTER *prAdapter, IN uint32_t u4FrameLength)
{
	uint32_t u4PageSize = prAdapter->rTxCtrl.u4PageSize;

	return ((u4FrameLength + prAdapter->nicTxReousrce.ucPpTxAddCnt + u4PageSize - 1)/u4PageSize);
}

uint32_t halTxGetPageCount(IN struct ADAPTER *prAdapter, IN uint32_t u4FrameLength, IN u_int8_t fgIncludeDesc)
{
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;

	if (prChipInfo->is_support_cr4)
		return 1;

	return halTxGetPageCountPSE(prAdapter, u4FrameLength);
}

uint32_t halDumpHifStatus(IN struct ADAPTER *prAdapter, IN uint8_t *pucBuf, IN uint32_t u4Max)
{
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	struct SDIO_STAT_COUNTER *prStatCnt = &prHifInfo->rStatCounter;
	uint32_t u4Len = 0;
	uint32_t u4Idx;

	/* Print out counter */
	LOGBUF(pucBuf, u4Max, u4Len, "\n");
	LOGBUF(pucBuf, u4Max, u4Len, "------<Dump SDIO Status>------\n");

	LOGBUF(pucBuf, u4Max, u4Len, "Coalescing buffer size[%u] Rx Cnt[%u/%u] DeAgg[%u] UF Cnt[%u]\n",
		prAdapter->u4CoalescingBufCachedSize, prHifInfo->rRxFreeBufQueue.u4NumElem,
		HIF_RX_COALESCING_BUF_COUNT, prHifInfo->rRxDeAggQueue.u4NumElem,
		prStatCnt->u4RxBufUnderFlowCnt);

	LOGBUF(pucBuf, u4Max, u4Len, "Pkt cnt Tx[%u] RxP0[%u] RxP1[%u] Tx/Rx ratio[%u.%u]\n",
		prStatCnt->u4DataPktWriteCnt, prStatCnt->u4PktReadCnt[0], prStatCnt->u4PktReadCnt[1],
		DIV2INT(prStatCnt->u4DataPktWriteCnt, prStatCnt->u4PktReadCnt[0]),
		DIV2DEC(prStatCnt->u4DataPktWriteCnt, prStatCnt->u4PktReadCnt[0]));

	LOGBUF(pucBuf, u4Max, u4Len, "Tx pkt/wt[%u.%u] pkt/kick[%u.%u] cmd/wt[%u.%u]\n",
		DIV2INT(prStatCnt->u4DataPktWriteCnt, prStatCnt->u4DataPortWriteCnt),
		DIV2DEC(prStatCnt->u4DataPktWriteCnt, prStatCnt->u4DataPortWriteCnt),
		DIV2INT(prStatCnt->u4DataPktWriteCnt, prStatCnt->u4DataPortKickCnt),
		DIV2DEC(prStatCnt->u4DataPktWriteCnt, prStatCnt->u4DataPortKickCnt),
		DIV2INT(prStatCnt->u4CmdPktWriteCnt, prStatCnt->u4CmdPortWriteCnt),
		DIV2DEC(prStatCnt->u4CmdPktWriteCnt, prStatCnt->u4CmdPortWriteCnt));

	LOGBUF(pucBuf, u4Max, u4Len, "Rx P0 pkt/rd[%u.%u] P1 pkt/rd[%u.%u]\n",
		DIV2INT(prStatCnt->u4PktReadCnt[0], prStatCnt->u4PortReadCnt[0]),
		DIV2DEC(prStatCnt->u4PktReadCnt[0], prStatCnt->u4PortReadCnt[0]),
		DIV2INT(prStatCnt->u4PktReadCnt[1], prStatCnt->u4PortReadCnt[1]),
		DIV2DEC(prStatCnt->u4PktReadCnt[1], prStatCnt->u4PortReadCnt[1]));

	LOGBUF(pucBuf, u4Max, u4Len, "Tx done pending cnt HIF_TXC00~04[%u, %u, %u, %u, %u]\n",
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_0],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_1],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_2],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_3],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_4]);

	LOGBUF(pucBuf, u4Max, u4Len, "Tx done pending cnt HIF_TXC05~09[%u, %u, %u, %u, %u]\n",
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_5],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_6],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_7],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_8],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_9]);

	LOGBUF(pucBuf, u4Max, u4Len, "Tx done pending cnt HIF_TXC10~15[%u, %u, %u, %u, %u, %u]\n",
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_10],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_11],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_12],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_13],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_14],
		prHifInfo->au4PendingTxDoneCount[HIF_TXC_IDX_15]);


	LOGBUF(pucBuf, u4Max, u4Len, "Tx done counter/int:\n");
	LOGBUF(pucBuf, u4Max, u4Len, "AC00~03[%u.%u, %u.%u, %u.%u, %u.%u]\n",
		DIV2INT(prStatCnt->u4TxDoneCnt[0], prStatCnt->u4TxDoneIntCnt[0]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[0], prStatCnt->u4TxDoneIntCnt[0]),
		DIV2INT(prStatCnt->u4TxDoneCnt[1], prStatCnt->u4TxDoneIntCnt[1]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[1], prStatCnt->u4TxDoneIntCnt[1]),
		DIV2INT(prStatCnt->u4TxDoneCnt[2], prStatCnt->u4TxDoneIntCnt[2]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[2], prStatCnt->u4TxDoneIntCnt[2]),
		DIV2INT(prStatCnt->u4TxDoneCnt[3], prStatCnt->u4TxDoneIntCnt[3]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[3], prStatCnt->u4TxDoneIntCnt[3]));

	LOGBUF(pucBuf, u4Max, u4Len, "AC10~13[%u.%u, %u.%u, %u.%u, %u.%u]\n",
		DIV2INT(prStatCnt->u4TxDoneCnt[4], prStatCnt->u4TxDoneIntCnt[4]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[4], prStatCnt->u4TxDoneIntCnt[4]),
		DIV2INT(prStatCnt->u4TxDoneCnt[5], prStatCnt->u4TxDoneIntCnt[5]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[5], prStatCnt->u4TxDoneIntCnt[5]),
		DIV2INT(prStatCnt->u4TxDoneCnt[6], prStatCnt->u4TxDoneIntCnt[6]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[5], prStatCnt->u4TxDoneIntCnt[5]),
		DIV2INT(prStatCnt->u4TxDoneCnt[7], prStatCnt->u4TxDoneIntCnt[7]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[7], prStatCnt->u4TxDoneIntCnt[7]));

	LOGBUF(pucBuf, u4Max, u4Len, "AC20~23[%u.%u, %u.%u, %u.%u, %u.%u] FFA,CPU[%u.%u, %u.%u]\n",
		DIV2INT(prStatCnt->u4TxDoneCnt[8], prStatCnt->u4TxDoneIntCnt[8]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[8], prStatCnt->u4TxDoneIntCnt[8]),
		DIV2INT(prStatCnt->u4TxDoneCnt[9], prStatCnt->u4TxDoneIntCnt[9]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[9], prStatCnt->u4TxDoneIntCnt[9]),
		DIV2INT(prStatCnt->u4TxDoneCnt[10], prStatCnt->u4TxDoneIntCnt[10]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[10], prStatCnt->u4TxDoneIntCnt[10]),
		DIV2INT(prStatCnt->u4TxDoneCnt[11], prStatCnt->u4TxDoneIntCnt[11]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[11], prStatCnt->u4TxDoneIntCnt[11]),
		DIV2INT(prStatCnt->u4TxDoneCnt[14], prStatCnt->u4TxDoneIntCnt[14]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[14], prStatCnt->u4TxDoneIntCnt[14]),
		DIV2INT(prStatCnt->u4TxDoneCnt[15], prStatCnt->u4TxDoneIntCnt[15]),
		DIV2DEC(prStatCnt->u4TxDoneCnt[15], prStatCnt->u4TxDoneIntCnt[15]));

	LOGBUF(pucBuf, u4Max, u4Len, "Pending pkt/int[%u.%u] kick/int[%u.%u] rx_enh/sts[%u.%u]\n",
		DIV2INT(prStatCnt->u4TxDonePendingPktCnt, prStatCnt->u4TxDoneIntTotCnt),
		DIV2DEC(prStatCnt->u4TxDonePendingPktCnt, prStatCnt->u4TxDoneIntTotCnt),
		DIV2INT(prStatCnt->u4DataPortKickCnt, prStatCnt->u4TxDoneIntTotCnt),
		DIV2DEC(prStatCnt->u4DataPortKickCnt, prStatCnt->u4TxDoneIntTotCnt),
		DIV2INT((prStatCnt->u4IntCnt - prStatCnt->u4IntReadCnt), prStatCnt->u4IntCnt),
		DIV2DEC((prStatCnt->u4IntCnt - prStatCnt->u4IntReadCnt), prStatCnt->u4IntCnt));

#if CFG_SDIO_TIMING_PROFILING
	LOGBUF(pucBuf, u4Max, u4Len, "Tx cp_t/pkt[%u.%uus] free/pkt[%u.%uus]\n",
		DIV2INT(prStatCnt->u4TxDataCpTime, prStatCnt->u4DataPktWriteCnt),
		DIV2DEC(prStatCnt->u4TxDataCpTime, prStatCnt->u4DataPktWriteCnt),
		DIV2INT(prStatCnt->u4TxDataFreeTime, prStatCnt->u4DataPktWriteCnt),
		DIV2DEC(prStatCnt->u4TxDataFreeTime, prStatCnt->u4DataPktWriteCnt));

	LOGBUF(pucBuf, u4Max, u4Len, "Rx P0 cp_t/pkt[%u.%uus] avg read[%u.%uus]\n",
		DIV2INT(prStatCnt->u4RxDataCpTime, prStatCnt->u4PktReadCnt[0]),
		DIV2DEC(prStatCnt->u4RxDataCpTime, prStatCnt->u4PktReadCnt[0]),
		DIV2INT(prStatCnt->u4PortReadTime, prStatCnt->u4PortReadCnt[0]),
		DIV2DEC(prStatCnt->u4PortReadTime, prStatCnt->u4PortReadCnt[0]));

	LOGBUF(pucBuf, u4Max, u4Len, "INT rd_sts/sts[%u.%uus] tx_sts/sts[%u.%uus]\n",
		DIV2INT(prStatCnt->u4IntReadTime, prStatCnt->u4IntReadCnt),
		DIV2DEC(prStatCnt->u4IntReadTime, prStatCnt->u4IntReadCnt),
		DIV2INT(prStatCnt->u4TxDoneIntTime, prStatCnt->u4TxDoneIntTotCnt),
		DIV2DEC(prStatCnt->u4TxDoneIntTime, prStatCnt->u4TxDoneIntTotCnt));
#endif

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n");

	for (u4Idx = 0; u4Idx < CFG_SDIO_INT_LOG_CNT; u4Idx++) {
		struct SDIO_INT_LOG_T *prIntLog = &prHifInfo->arIntLog[u4Idx];
		struct ENHANCE_MODE_DATA_STRUCT *prIntSts = (struct ENHANCE_MODE_DATA_STRUCT *)&prIntLog->aucIntSts[0];
		uint8_t ucPktIdx;

		LOGBUF(pucBuf, u4Max, u4Len, "INT IDX[%u] STS[0x%08x] FG[0x%08x] Rx Pkt[%u] Sts0/1[%u:%u]\n",
			prIntLog->u4Idx, prIntSts->u4WHISR, prIntLog->u4Flag, prIntLog->ucRxPktCnt,
			prIntSts->rRxInfo.u.u2NumValidRx0Len, prIntSts->rRxInfo.u.u2NumValidRx1Len);

		if (prIntLog->ucRxPktCnt) {
			LOGBUF(pucBuf, u4Max, u4Len, "RxDAggLen[");
			for (ucPktIdx = 0; ucPktIdx < prIntLog->ucRxPktCnt; ucPktIdx++)
				LOGBUF(pucBuf, u4Max, u4Len, "%4u:", prIntLog->au2RxPktLen[ucPktIdx]);
			LOGBUF(pucBuf, u4Max, u4Len, "]\n");

			LOGBUF(pucBuf, u4Max, u4Len, "RxDAggSn [");
			for (ucPktIdx = 0; ucPktIdx < prIntLog->ucRxPktCnt; ucPktIdx++)
				LOGBUF(pucBuf, u4Max, u4Len, "0x%08x: ", prIntLog->au4RxPktInfo[ucPktIdx]);
			LOGBUF(pucBuf, u4Max, u4Len, "]\n");
		}

		if (prIntSts->rRxInfo.u.u2NumValidRx0Len) {
			LOGBUF(pucBuf, u4Max, u4Len, "Rx0StsLen[");
			for (ucPktIdx = 0; ucPktIdx < prIntSts->rRxInfo.u.u2NumValidRx0Len; ucPktIdx++)
				LOGBUF(pucBuf, u4Max, u4Len, "%4u:", prIntSts->rRxInfo.u.au2Rx0Len[ucPktIdx]);
			LOGBUF(pucBuf, u4Max, u4Len, "]\n");
		}

		if (prIntSts->rRxInfo.u.u2NumValidRx1Len) {
			LOGBUF(pucBuf, u4Max, u4Len, "Rx1StsLen[");
			for (ucPktIdx = 0; ucPktIdx < HIF_RX_MAX_AGG_NUM; ucPktIdx++)
				LOGBUF(pucBuf, u4Max, u4Len, "%4u:", prIntSts->rRxInfo.u.au2Rx1Len[ucPktIdx]);
			LOGBUF(pucBuf, u4Max, u4Len, "]\n");
		}
	}

	LOGBUF(pucBuf, u4Max, u4Len, "---------------------------------\n");

	/* Reset statistic counter */
	kalMemZero(prStatCnt, sizeof(struct SDIO_STAT_COUNTER));

	halDumpIntLog(prAdapter);

	return u4Len;
}

#if (CFG_SDIO_ACCESS_N9_REGISTER_BY_MAILBOX == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief
*       This routine is used to get the value of N9 register
*       by SDIO SW interrupt and mailbox.
*
* \param[in]
*       pvAdapter: Pointer to the Adapter structure.
*       addr: the interested address to be read
*       prresult: to stored the value of the addr
*
* \return
*       the error of the reading operation
*/
/*----------------------------------------------------------------------------*/

u_int8_t halReadN9RegisterByMailBox(IN struct ADAPTER *prAdapter, IN uint32_t addr, IN uint32_t *prresult)
{
	uint32_t ori_whlpcr, temp, counter = 0;
	u_int8_t err = TRUE, stop = FALSE;

	/* use polling mode */
	HAL_MCR_RD(prAdapter, MCR_WHLPCR, &ori_whlpcr); /* backup the original setting of W_INT_EN */
	ori_whlpcr &= WHLPCR_INT_EN_SET;
	HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_INT_EN_CLR); /* disabel interrupt */

	/* progrqm h2d mailbox0 as interested register address */
	HAL_MCR_WR(prAdapter, MCR_H2DSM0R, addr);

	/* set h2d interrupt to notify firmware (bit16) */
	HAL_MCR_WR(prAdapter, MCR_WSICR, SDIO_MAILBOX_FUNC_READ_REG_IDX);

	/* polling interrupt status for the returned result */
	while (!stop) {
		HAL_MCR_RD(prAdapter, MCR_WHISR, &temp); /* read clear mode */
		if (temp & SDIO_MAILBOX_FUNC_READ_REG_IDX) {
			/* get the result */

			/* read d2h mailbox0 for interested register address */
			HAL_MCR_RD(prAdapter, MCR_D2HRM0R, &temp);
			if (temp == addr) {
				/* read d2h mailbox1 for the value of the register */
				HAL_MCR_RD(prAdapter, MCR_D2HRM1R, prresult);
				err = FALSE;
			} else {
	DBGLOG(HAL, ERROR, "halReadN9RegisterByMailBox >> interested address is not correct.\n");
			}
			stop = TRUE;
		} else {
counter++;

if (counter > 300000) {
	DBGLOG(HAL, ERROR, "halReadN9RegisterByMailBox >> get response failure.\n");
				ASSERT(0);
				break;
			}
		}
	}

	HAL_MCR_WR(prAdapter, MCR_WHLPCR, ori_whlpcr); /* restore the W_INT_EN */

	return err;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*       This routine is used to write the value of N9 register by SDIO SW interrupt and mailbox.
*
* \param[in]
*       pvAdapter: Pointer to the Adapter structure.
*       addr: the interested address to be write
*       value: the value to write into the addr
*
* \return
*       the error of the write operation
*/
/*----------------------------------------------------------------------------*/

u_int8_t halWriteN9RegisterByMailBox(IN struct ADAPTER *prAdapter, IN uint32_t addr, IN uint32_t value)
{
	uint32_t ori_whlpcr, temp, counter = 0;
	u_int8_t err = TRUE, stop = FALSE;

	/* use polling mode */
	HAL_MCR_RD(prAdapter, MCR_WHLPCR, &ori_whlpcr); /* backup the original setting of W_INT_EN */
	ori_whlpcr &= WHLPCR_INT_EN_SET;
	HAL_MCR_WR(prAdapter, MCR_WHLPCR, WHLPCR_INT_EN_CLR); /* disabel interrupt */

	/* progrqm h2d mailbox0 as interested register address */
	HAL_MCR_WR(prAdapter, MCR_H2DSM0R, addr);

	/* progrqm h2d mailbox1 as the value to write */
	HAL_MCR_WR(prAdapter, MCR_H2DSM1R, value);

	/* set h2d interrupt to notify firmware (bit17) */
	HAL_MCR_WR(prAdapter, MCR_WSICR, SDIO_MAILBOX_FUNC_WRITE_REG_IDX);

	/* polling interrupt status for the returned result */
	while (!stop) {
		HAL_MCR_RD(prAdapter, MCR_WHISR, &temp); /* read clear mode */

		if (temp & SDIO_MAILBOX_FUNC_WRITE_REG_IDX) {
			/* get the result */

			/* read d2h mailbox0 for interested register address */
			HAL_MCR_RD(prAdapter, MCR_D2HRM0R, &temp);
			if (temp == addr)
				err = FALSE;
			else {
					DBGLOG(HAL, ERROR, "halWriteN9RegisterByMailBox >> ");
					DBGLOG(HAL, ERROR, "interested address is not correct.\n");
			}
			stop = TRUE;
		} else {
			counter++;

if (counter > 300000) {
	DBGLOG(HAL, ERROR, "halWriteN9RegisterByMailBox >> get response failure.\n");
				ASSERT(0);
				break;
			}
		}
	}

	HAL_MCR_WR(prAdapter, MCR_WHLPCR, ori_whlpcr); /* restore the W_INT_EN */

	return err;
}
#endif

u_int8_t halIsPendingRx(IN struct ADAPTER *prAdapter)
{
	return FALSE;
}

uint32_t halGetValidCoalescingBufSize(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo;
	uint32_t u4BufSize;
#if (MTK_WCN_HIF_SDIO == 0)
	struct sdio_func *prSdioFunc;
	uint32_t u4RuntimeMaxBuf;

#endif

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (HIF_TX_COALESCING_BUFFER_SIZE > HIF_RX_COALESCING_BUFFER_SIZE)
		u4BufSize = HIF_TX_COALESCING_BUFFER_SIZE;
	else
		u4BufSize = HIF_RX_COALESCING_BUFFER_SIZE;

#if (MTK_WCN_HIF_SDIO == 0)
	prSdioFunc = prHifInfo->func;

	/* Check host capability */
	/* 1. Should less than host-max_req_size */
	if (u4BufSize > prSdioFunc->card->host->max_req_size)
		u4BufSize = prSdioFunc->card->host->max_req_size;

	/* 2. Should less than runtime-blksize * host-blk_count  */
	u4RuntimeMaxBuf = prSdioFunc->cur_blksize *
					prSdioFunc->card->host->max_blk_count;
	if (u4BufSize > u4RuntimeMaxBuf)
		u4BufSize = u4RuntimeMaxBuf;

	DBGLOG(INIT, TRACE, "\n"
				"Final buf : 0x%X\n"
				"Default TX buf : 0x%X\n"
				"Default RX buf : 0x%X\n"
				"Host caps -\n"
				"max_req_size : 0x%X\n"
				"max_seg_size : 0x%X\n"
				"max_segs : 0x%X\n"
				"max_blk_size : 0x%X\n"
				"max_blk_count : 0x%X\n"
				"Runtime -\n"
				"cur_blksize : 0x%X\n",
				u4BufSize,
				HIF_TX_COALESCING_BUFFER_SIZE,
				HIF_RX_COALESCING_BUFFER_SIZE,
				prSdioFunc->card->host->max_req_size,
				prSdioFunc->card->host->max_seg_size,
				prSdioFunc->card->host->max_segs,
				prSdioFunc->card->host->max_blk_size,
				prSdioFunc->card->host->max_blk_count,
				prSdioFunc->cur_blksize);
#endif

	return u4BufSize;
}

uint32_t halAllocateIOBuffer(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo;
	uint8_t ucIdx;
	struct SDIO_RX_COALESCING_BUF *prRxBuf;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	/* 4 <5> Memory for enhanced interrupt response */
#ifdef CFG_PREALLOC_MEMORY
	prHifInfo->prSDIOCtrl = (struct ENHANCE_MODE_DATA_STRUCT *)
		preallocGetMem(MEM_ID_IO_CTRL);
#else
	prHifInfo->prSDIOCtrl = (struct ENHANCE_MODE_DATA_STRUCT *)
		kalAllocateIOBuffer(sizeof(struct ENHANCE_MODE_DATA_STRUCT));
#endif
	if (prHifInfo->prSDIOCtrl == NULL) {
		DBGLOG(HAL, ERROR,
			"Could not allocate %d bytes for interrupt response.\n",
			sizeof(struct ENHANCE_MODE_DATA_STRUCT));

		return WLAN_STATUS_RESOURCES;
	}

	/* Alloc coalescing buffer */
	for (ucIdx = 0; ucIdx < HIF_RX_COALESCING_BUF_COUNT; ucIdx++) {
		prRxBuf = &prHifInfo->rRxCoalesingBuf[ucIdx];

		prRxBuf->u4PktCount = 0;

		prRxBuf->u4BufSize = HIF_RX_COALESCING_BUFFER_SIZE;
#ifdef CFG_PREALLOC_MEMORY
		prRxBuf->pvRxCoalescingBuf = preallocGetMem(MEM_ID_RX_DATA);
#else
		prRxBuf->pvRxCoalescingBuf = kalAllocateIOBuffer(prRxBuf->u4BufSize);
#endif
		if (!prRxBuf->pvRxCoalescingBuf) {
			DBGLOG(HAL, ERROR, "Rx coalescing alloc failed!\n");
			continue;
		}

		QUEUE_INSERT_TAIL(&prHifInfo->rRxFreeBufQueue, &prRxBuf->rQueEntry);
	}

	return WLAN_STATUS_SUCCESS;
}

uint32_t halReleaseIOBuffer(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo;
	uint8_t ucIdx;
	struct SDIO_RX_COALESCING_BUF *prRxBuf;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	/* Release coalescing buffer */
	for (ucIdx = 0; ucIdx < HIF_RX_COALESCING_BUF_COUNT; ucIdx++) {
		prRxBuf = &prHifInfo->rRxCoalesingBuf[ucIdx];
#ifndef CFG_PREALLOC_MEMORY
		kalReleaseIOBuffer(prRxBuf->pvRxCoalescingBuf, prRxBuf->u4BufSize);
#endif
		prRxBuf->pvRxCoalescingBuf = NULL;
	}

	/* 4 <5> Memory for enhanced interrupt response */
	if (prHifInfo->prSDIOCtrl) {
#ifndef CFG_PREALLOC_MEMORY
		kalReleaseIOBuffer((void *) prHifInfo->prSDIOCtrl, sizeof(struct ENHANCE_MODE_DATA_STRUCT));
#endif
		prHifInfo->prSDIOCtrl = (struct ENHANCE_MODE_DATA_STRUCT *) NULL;
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief dump firmware Assert message
*
* \param[in]
*           prAdapter
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
void halPrintFirmwareAssertInfo(IN struct ADAPTER *prAdapter)
{
	uint32_t u4MailBox0, u4MailBox1;
	uint32_t line = 0;
	uint8_t aucAssertFile[7];
	/* UINT_32 u4ChipId; */

#if CFG_SDIO_INTR_ENHANCE
	u4MailBox0 = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl->u4RcvMailbox0;
	u4MailBox1 = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl->u4RcvMailbox1;
#else
	halGetMailbox(prAdapter, 0, &u4MailBox0);
	halGetMailbox(prAdapter, 1, &u4MailBox1);
#endif

	line = u4MailBox0 & 0x0000FFFF;

	u4MailBox0 = ((u4MailBox0 >> 16) & 0x0000FFFF);

	kalMemCopy(&aucAssertFile[0], &u4MailBox0, 2);
	kalMemCopy(&aucAssertFile[2], &u4MailBox1, 4);

	aucAssertFile[6] = '\0';

	LOG_FUNC("[%s][wifi][Firmware] Assert at \"%s\" #%u\n\n",
		NIC_NAME, aucAssertFile, line);

}

void halPrintMailbox(IN struct ADAPTER *prAdapter)
{
	uint32_t u4MailBoxStatus0, u4MailBoxStatus1;
	uint8_t fgResult;

	HAL_LP_OWN_RD(prAdapter, &fgResult);
	if (fgResult != TRUE)
		return;

	halGetMailbox(prAdapter, 0, &u4MailBoxStatus0);
	halGetMailbox(prAdapter, 1, &u4MailBoxStatus1);
	DBGFWLOG(INIT, ERROR, "MailBox Status = 0x%08X, 0x%08X\n", u4MailBoxStatus0, u4MailBoxStatus1);
}

void halPrintIntStatus(IN struct ADAPTER *prAdapter)
{
#if CFG_SDIO_INTR_ENHANCE
	struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl;

	prSDIOCtrl = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl;
	ASSERT(prSDIOCtrl);

	DBGLOG_MEM32(REQ, WARN, prSDIOCtrl, sizeof(struct ENHANCE_MODE_DATA_STRUCT));
#else
	uint32_t u4IntStatus;

	HAL_MCR_RD(prAdapter, MCR_WHISR, u4IntStatus);

	DBGLOG(REQ, WARN, "INT status[0x%08x]\n", u4IntStatus);
#endif /* CFG_SDIO_INTR_ENHANCE */
}

void halDumpIntLog(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	struct SDIO_INT_LOG_T *prIntLog;
	struct ENHANCE_MODE_DATA_STRUCT *prIntSts;
	uint32_t u4Idx;

	for (u4Idx = 0; u4Idx < CFG_SDIO_INT_LOG_CNT; u4Idx++) {
		prIntLog = &prHifInfo->arIntLog[u4Idx];
		prIntSts = (struct ENHANCE_MODE_DATA_STRUCT *)&prIntLog->aucIntSts[0];

		DBGLOG(INTR, ERROR, "INT IDX[%u] STS[0x%08x] FG[0x%08x] Rx Pkt[%u] Sts0/1[%u:%u]\n",
			prIntLog->u4Idx, prIntSts->u4WHISR, prIntLog->u4Flag, prIntLog->ucRxPktCnt,
			prIntSts->rRxInfo.u.u2NumValidRx0Len, prIntSts->rRxInfo.u.u2NumValidRx1Len);
		DBGLOG_MEM32(INTR, ERROR, &prIntLog->au2RxPktLen[0], sizeof(uint16_t) * HIF_RX_MAX_AGG_NUM);
		DBGLOG_MEM32(INTR, ERROR, &prIntLog->au4RxPktInfo[0], sizeof(uint32_t) * HIF_RX_MAX_AGG_NUM);
		DBGLOG_MEM32(INTR, ERROR, prIntSts, sizeof(struct ENHANCE_MODE_DATA_STRUCT));
	}

	DBGLOG(INTR, ERROR, "---------------------------------\n");
}

void halTagIntLog(IN struct ADAPTER *prAdapter, IN enum HIF_SDIO_INT_STS eTag)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	prHifInfo->arIntLog[prHifInfo->ucIntLogEntry].u4Flag |= BIT(eTag);
}

void halRecIntLog(IN struct ADAPTER *prAdapter, IN struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl)
{
	struct SDIO_INT_LOG_T *prIntLog;
	uint8_t ucLogEntry;
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	prHifInfo->u4IntLogIdx++;
	ucLogEntry = prHifInfo->u4IntLogIdx % CFG_SDIO_INT_LOG_CNT;
	prIntLog = &prHifInfo->arIntLog[ucLogEntry];
	kalMemZero(prIntLog, sizeof(struct SDIO_INT_LOG_T));

	prIntLog->u4Idx = prHifInfo->u4IntLogIdx;
	prHifInfo->ucIntLogEntry = ucLogEntry;
	kalMemCopy(&prIntLog->aucIntSts[0], prSDIOCtrl, sizeof(struct ENHANCE_MODE_DATA_STRUCT));
}

struct SDIO_INT_LOG_T *halGetIntLog(IN struct ADAPTER *prAdapter, IN uint32_t u4Idx)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	return &prHifInfo->arIntLog[u4Idx % CFG_SDIO_INT_LOG_CNT];
}

void halProcessAbnormalInterrupt(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Data = 0;
	uint8_t fgResult;

	/* dump abnormal status */

	/* Need driver own */
	HAL_LP_OWN_RD(prAdapter, &fgResult);
	if (fgResult == TRUE)
		HAL_MCR_RD(prAdapter, MCR_WASR, &u4Data);

	halPollDbgCr(prAdapter, 5);

	halPrintIntStatus(prAdapter);

	if (u4Data) {
		DBGLOG(REQ, WARN, "SDIO Abnormal: WASR = 0x%x!\n",
			u4Data);
	}

	if (u4Data & (WASR_RX0_UNDER_FLOW | WASR_RX1_UNDER_FLOW)) {
		DBGLOG(REQ, ERROR,
		"Skip all SDIO Rx due to Rx underflow error!\n");
		prAdapter->prGlueInfo->rHifInfo.fgSkipRx = TRUE;
		halDumpHifStatus(prAdapter, NULL, 0);
		GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP);
	}

	halDumpIntLog(prAdapter);
}

void halProcessSoftwareInterrupt(IN struct ADAPTER *prAdapter)
{
	uint32_t u4IntrBits;

	ASSERT(prAdapter);

	u4IntrBits = prAdapter->u4IntStatus & BITS(8, 31);

	if ((u4IntrBits & WHISR_D2H_SW_ASSERT_INFO_INT) != 0) {
		halPrintFirmwareAssertInfo(prAdapter);
#if CFG_CHIP_RESET_SUPPORT
		glSendResetRequest();
#endif
	}

	if (u4IntrBits & WHISR_D2H_WKUP_BY_RX_PACKET)
		DBGLOG(RX, INFO, "Wake up by Rx\n");

	if (u4IntrBits & WHISR_D2H_SW_RD_MAILBOX_INT)
		halPrintMailbox(prAdapter);

	if (u4IntrBits & SER_SDIO_N9_HOST_STOP_TX_OP) {
		halPrintMailbox(prAdapter);
		/* Stop HIF Tx operation */
		nicSerStopTx(prAdapter);
	}

	if (u4IntrBits & SER_SDIO_N9_HOST_STOP_TX_RX_OP) {
		DBGLOG(INIT, WARN, "[SER][L1] fw notify host L1 start\n");
		halPrintMailbox(prAdapter);
		/* Stop HIF Tx/Rx operation */
		nicSerStopTxRx(prAdapter);
	}

	if (u4IntrBits & SER_SDIO_N9_HOST_RECOVERY_DONE)
		DBGLOG(INIT, WARN, "[SER][L1] fw L1 rst done\n");

	if ((u4IntrBits & ~WHISR_D2H_WKUP_BY_RX_PACKET) != 0)
		DBGLOG(SW4, WARN, "u4IntrBits: 0x%08x\n", u4IntrBits);

} /* end of halProcessSoftwareInterrupt() */

void halPutMailbox(IN struct ADAPTER *prAdapter, IN uint32_t u4MailboxNum, IN uint32_t u4Data)
{

	switch (u4MailboxNum) {
	case 0:
		HAL_MCR_WR(prAdapter, MCR_H2DSM0R, u4Data);
		break;
	case 1:
		HAL_MCR_WR(prAdapter, MCR_H2DSM1R, u4Data);
		break;

	default:
		ASSERT(0);
	}

}

void halGetMailbox(IN struct ADAPTER *prAdapter, IN uint32_t u4MailboxNum, OUT uint32_t *pu4Data)
{
	switch (u4MailboxNum) {
	case 0:
			HAL_MCR_RD(prAdapter, MCR_D2HRM0R, pu4Data);
			break;
	case 1:
			HAL_MCR_RD(prAdapter, MCR_D2HRM1R, pu4Data);
			break;

	default:
			ASSERT(0);
	}
}

u_int8_t halDeAggErrorCheck(struct ADAPTER *prAdapter,
			    struct SDIO_RX_COALESCING_BUF *prRxBuf,
			    uint8_t *pucPktAddr)
{
	struct mt66xx_chip_info *prChipInfo;
	uint16_t u2PktLength;
	uint8_t *pucRxBufEnd;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	pucRxBufEnd = (uint8_t *)prRxBuf->pvRxCoalescingBuf + prRxBuf->u4PktTotalLength;

	u2PktLength = HAL_RX_STATUS_GET_RX_BYTE_CNT((struct HW_MAC_RX_DESC *)pucPktAddr);

	/* Rx buffer boundary check */
	if ((pucPktAddr + ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN)) >= pucRxBufEnd)
		return TRUE;

	/* Rx packet min length check */
	if (u2PktLength <= prChipInfo->rxd_size)
		return TRUE;

	/* Rx packet max length check */
	if (u2PktLength >= CFG_RX_MAX_PKT_SIZE)
		return TRUE;

	return FALSE;
}

void halDeAggRxPktProc(struct ADAPTER *prAdapter,
			struct SDIO_RX_COALESCING_BUF *prRxBuf)
{
	struct GL_HIF_INFO *prHifInfo;
	uint32_t i;
	struct QUE rTempFreeRfbList, rTempRxRfbList;
	struct QUE *prTempFreeRfbList = &rTempFreeRfbList;
	struct QUE *prTempRxRfbList = &rTempRxRfbList;
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = (struct SW_RFB *) NULL;
	uint8_t *pucSrcAddr;
	uint16_t u2PktLength;
	u_int8_t fgReschedule = FALSE;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	uint32_t *pu4HwAppendDW;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
	u_int8_t fgDeAggErr = FALSE;
	struct SDIO_INT_LOG_T *prIntLog;
	uint64_t u8Current = 0;

	KAL_SPIN_LOCK_DECLARATION();
	SDIO_TIME_INTERVAL_DEC();

	prRxCtrl = &prAdapter->rRxCtrl;
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	QUEUE_INITIALIZE(prTempFreeRfbList);
	QUEUE_INITIALIZE(prTempRxRfbList);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	if (prRxCtrl->rFreeSwRfbList.u4NumElem < prRxBuf->u4PktCount) {
		fgReschedule = TRUE;
	} else {
		/* Get enough free SW_RFB to be Rx */
		for (i = 0; i < prRxBuf->u4PktCount; i++) {
			QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList,
				prSwRfb, struct SW_RFB *);

			QUEUE_INSERT_TAIL(prTempFreeRfbList,
				&prSwRfb->rQueEntry);
		}
		fgReschedule = FALSE;
	}
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	if (fgReschedule) {
		mutex_lock(&prHifInfo->rRxDeAggQueMutex);

		QUEUE_INSERT_HEAD(&prHifInfo->rRxDeAggQueue,
						  (struct QUE_ENTRY *)prRxBuf);

		mutex_unlock(&prHifInfo->rRxDeAggQueMutex);

		/* Reschedule this work */
		if ((prAdapter->prGlueInfo->ulFlag & GLUE_FLAG_HALT) == 0)
			schedule_delayed_work(
				&prAdapter->prGlueInfo->rRxPktDeAggWork, 0);

		return;
	}

	pucSrcAddr = prRxBuf->pvRxCoalescingBuf;
	fgDeAggErr = FALSE;

	prIntLog = halGetIntLog(prAdapter, prRxBuf->u4IntLogIdx);
	u8Current = sched_clock();

	SDIO_REC_TIME_START();
	for (i = 0; i < prRxBuf->u4PktCount; i++) {
		/* Rx de-aggregation check */
		if (halDeAggErrorCheck(prAdapter, prRxBuf, pucSrcAddr)) {
			fgDeAggErr = TRUE;
			break;
		}

		u2PktLength =
			HAL_RX_STATUS_GET_RX_BYTE_CNT(
				(struct HW_MAC_RX_DESC *)pucSrcAddr);

		prIntLog->au2RxPktLen[i] = u2PktLength;

		QUEUE_REMOVE_HEAD(prTempFreeRfbList,
			prSwRfb, struct SW_RFB *);

		kalMemCopy(prSwRfb->pucRecvBuff, pucSrcAddr,
			ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));

		prSwRfb->ucPacketType =
		  (uint8_t)HAL_RX_STATUS_GET_PKT_TYPE(prSwRfb->prRxStatus);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		pu4HwAppendDW = (uint32_t *) prSwRfb->prRxStatus;
		pu4HwAppendDW +=
			(ALIGN_4(prSwRfb->prRxStatus->u2RxByteCount) >> 2);
		prSwRfb->u4TcpUdpIpCksStatus = *pu4HwAppendDW;
		DBGLOG(RX, TRACE,
			"u4TcpUdpIpCksStatus[0x%02x]\n",
			prSwRfb->u4TcpUdpIpCksStatus);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

		kalMemCopy(&prIntLog->au4RxPktInfo[i],
			pucSrcAddr + ALIGN_4(u2PktLength), sizeof(uint32_t));

		GLUE_RX_SET_PKT_INT_TIME(prSwRfb->pvPacket,
			prAdapter->prGlueInfo->u8HifIntTime);

		GLUE_RX_SET_PKT_RX_TIME(prSwRfb->pvPacket, u8Current);

		QUEUE_INSERT_TAIL(prTempRxRfbList, &prSwRfb->rQueEntry);

		pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
	}
	SDIO_REC_TIME_END();
	SDIO_ADD_TIME_INTERVAL(prHifInfo->rStatCounter.u4RxDataCpTime);

	prIntLog->ucRxPktCnt = i;

	if (fgDeAggErr) {
		/* Rx de-aggregation error */
		/* Dump current Rx buffer */
		DBGLOG(RX, ERROR,
			"Rx de-aggregation error!, INT sts: total len[%u] pkt cnt[%u]\n",
			prRxBuf->u4PktTotalLength, prRxBuf->u4PktCount);
#if 0
		/* Sometimes the larger frame is received, and dump
		 * those message will let platform stop application.
		 */
		DBGLOG_MEM32(RX, ERROR,
				prRxBuf->pvRxCoalescingBuf,
				prRxBuf->u4PktTotalLength);

		halDumpIntLog(prAdapter);
#endif

		/* Free all de-aggregated SwRfb */
		QUEUE_CONCATENATE_QUEUES(prTempFreeRfbList, prTempRxRfbList);
	} else {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		RX_ADD_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT,
				   prTempRxRfbList->u4NumElem);

		QUEUE_CONCATENATE_QUEUES(&prRxCtrl->rReceivedRfbList,
					prTempRxRfbList);

		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		/* Wake up Rx handling thread */
		set_bit(GLUE_FLAG_RX_BIT, &(prAdapter->prGlueInfo->ulFlag));
		wake_up_interruptible(&(prAdapter->prGlueInfo->waitq));
	}

	if (prTempFreeRfbList->u4NumElem) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_CONCATENATE_QUEUES(&prRxCtrl->rFreeSwRfbList,
					prTempFreeRfbList);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	}

	prRxBuf->u4PktCount = 0;
	mutex_lock(&prHifInfo->rRxFreeBufQueMutex);
	QUEUE_INSERT_TAIL(&prHifInfo->rRxFreeBufQueue,
					  (struct QUE_ENTRY *)prRxBuf);
	mutex_unlock(&prHifInfo->rRxFreeBufQueMutex);
}


void halDeAggRxPktWorker(struct work_struct *work)
{
	struct GLUE_INFO *prGlueInfo;
	struct GL_HIF_INFO *prHifInfo;
	struct ADAPTER *prAdapter;
	struct SDIO_RX_COALESCING_BUF *prRxBuf;
	struct RX_CTRL *prRxCtrl;

	if (g_u4HaltFlag)
		return;

	prGlueInfo = ENTRY_OF(work, struct GLUE_INFO, rRxPktDeAggWork);
	prHifInfo = &prGlueInfo->rHifInfo;
	prAdapter = prGlueInfo->prAdapter;

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT)
		return;

	prRxCtrl = &prAdapter->rRxCtrl;
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	mutex_lock(&prHifInfo->rRxDeAggQueMutex);

	QUEUE_REMOVE_HEAD(&prHifInfo->rRxDeAggQueue,
		prRxBuf, struct SDIO_RX_COALESCING_BUF *);

	mutex_unlock(&prHifInfo->rRxDeAggQueMutex);
	while (prRxBuf) {
		halDeAggRxPktProc(prAdapter, prRxBuf);

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT)
			return;

		mutex_lock(&prHifInfo->rRxDeAggQueMutex);
		QUEUE_REMOVE_HEAD(&prHifInfo->rRxDeAggQueue, prRxBuf, struct SDIO_RX_COALESCING_BUF *);
		mutex_unlock(&prHifInfo->rRxDeAggQueMutex);
	}
}

void halDeAggRxPkt(struct ADAPTER *prAdapter, struct SDIO_RX_COALESCING_BUF *prRxBuf)
{
	struct GL_HIF_INFO *prHifInfo;

	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	/* Avoid to schedule DeAggWorker during uninit flow */
	if (prAdapter->prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		mutex_lock(&prHifInfo->rRxFreeBufQueMutex);
		QUEUE_INSERT_TAIL(&prHifInfo->rRxFreeBufQueue, (struct QUE_ENTRY *)prRxBuf);
		mutex_unlock(&prHifInfo->rRxFreeBufQueMutex);

		return;
	}

#if CFG_SDIO_RX_AGG_WORKQUE
	mutex_lock(&prHifInfo->rRxDeAggQueMutex);
	QUEUE_INSERT_TAIL(&prHifInfo->rRxDeAggQueue, (struct QUE_ENTRY *)prRxBuf);
	mutex_unlock(&prHifInfo->rRxDeAggQueMutex);

	schedule_delayed_work(&prAdapter->prGlueInfo->rRxPktDeAggWork, 0);
#else
	halDeAggRxPktProc(prAdapter, prRxBuf);
#endif
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
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	if (prAdapter->rAcpiState == ACPI_STATE_D0 &&
		!wlanIsChipNoAck(prAdapter) && !kalIsCardRemoved(prAdapter->prGlueInfo)) {
		/* 0. Disable interrupt, this can be done without Driver own */
		nicDisableInterrupt(prAdapter);

		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		/* 1. Set CMD to FW to tell WIFI to stop (enter power off state) */
		if (prAdapter->fgIsFwOwn == FALSE && wlanSendNicPowerCtrlCmd(prAdapter, 1) == WLAN_STATUS_SUCCESS) {
			uint32_t i;
			/* 2. Clear pending interrupt */
			i = 0;
			while (i < CFG_IST_LOOP_COUNT && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
				i++;
			};

			/* 3. Wait til RDY bit has been cleaerd */
			rStatus = wlanCheckWifiFunc(prAdapter, FALSE);
		}
#if !CFG_ENABLE_FULL_PM
		/* 4. Set Onwership to F/W */
		nicpmSetFWOwn(prAdapter, FALSE);
#endif

#if CFG_FORCE_RESET_UNDER_BUS_ERROR
		if (HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR) == TRUE) {
			/* force acquire firmware own */
			kalDevRegWrite(prAdapter->prGlueInfo, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);

			/* delay for 10ms */
			kalMdelay(10);

			/* force firmware reset via software interrupt */
			kalDevRegWrite(prAdapter->prGlueInfo, MCR_WSICR, WSICR_H2D_SW_INT_SET);

			/* force release firmware own */
			kalDevRegWrite(prAdapter->prGlueInfo, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);
		}
#endif

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	}
	return rStatus;
}

void halPollDbgCr(IN struct ADAPTER *prAdapter, IN uint32_t u4LoopCount)
{
	uint32_t au4Value[] = {MCR_WCIR, MCR_WHLPCR, MCR_D2HRM2R};
	uint32_t au4Value1[] = {MCR_WHIER, MCR_D2HRM0R, MCR_D2HRM1R};
	uint32_t u4Loop = 0;
	uint32_t u4Data = 0;
	uint8_t i = 0, fgResult;
#if MTK_WCN_HIF_SDIO
	uint8_t *pucCCR = (uint8_t *)&au4Value[0];
	unsigned long cltCtx = prAdapter->prGlueInfo->rHifInfo.cltCtx;
#endif
	for (; i < sizeof(au4Value)/sizeof(uint32_t); i++)
		HAL_MCR_RD(prAdapter, au4Value[i], &au4Value[i]);

	DBGLOG(REQ, WARN, "MCR_WCIR:0x%x, MCR_WHLPCR:0x%x, MCR_D2HRM2R:0x%x\n",
		au4Value[0], au4Value[1], au4Value[2]);

	/* Need driver own */
	HAL_LP_OWN_RD(prAdapter, &fgResult);
	if (fgResult == TRUE) {
		/* dump N9 programming counter */
		for (u4Loop = 0; u4Loop < u4LoopCount; u4Loop++) {
			HAL_MCR_RD(prAdapter, MCR_SWPCDBGR, &u4Data);
			DBGLOG(INIT, WARN, "SWPCDBGR 0x%08X\n", u4Data);
		}

		/* dump others */
		for (i = 0; i < sizeof(au4Value1)/sizeof(uint32_t); i++)
			HAL_MCR_RD(prAdapter, au4Value1[i], &au4Value1[i]);

		DBGLOG(REQ, WARN, "MCR_WHIER:0x%x, MCR_D2HRM0R:0x%x",
			au4Value1[0], au4Value1[1]);
		DBGLOG(REQ, WARN, "MCR_D2HRM1R:0x%x\n",
			au4Value1[2]);
	}

#if MTK_WCN_HIF_SDIO
	for (i = 0; i < 8; i++)
		mtk_wcn_hif_sdio_f0_readb(cltCtx, 0xf8 + i, &pucCCR[i]);
	DBGLOG(REQ, WARN, "CCCR %02x %02x %02x %02x %02x %02x %02x %02x\n",
		pucCCR[0], pucCCR[1], pucCCR[2], pucCCR[3], pucCCR[4], pucCCR[5], pucCCR[6], pucCCR[7]);
#endif
}

void halSerHifReset(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	/* Restore Tx resource */
	halRestoreTxResource(prAdapter);

	/* Clear interrupt status from Rx interrupt enhance mode */
	prHifInfo->fgIsPendingInt = FALSE;
	kalMemZero(prHifInfo->prSDIOCtrl, sizeof(struct ENHANCE_MODE_DATA_STRUCT));
}

u_int8_t halIsPendingTxDone(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	uint32_t i;
	u_int8_t fgIsPendingTxDone = FALSE;

	for (i = TC0_INDEX; i <= TC4_INDEX; i++) {
		if (prHifInfo->au4PendingTxDoneCount[i]) {
			fgIsPendingTxDone = TRUE;
			break;
		}
	}

	if (fgIsPendingTxDone)
		DBGLOG(NIC, ERROR, "Missing Tx done[%u:%u:%u:%u:%u]\n",
			prHifInfo->au4PendingTxDoneCount[TC0_INDEX],
			prHifInfo->au4PendingTxDoneCount[TC1_INDEX],
			prHifInfo->au4PendingTxDoneCount[TC2_INDEX],
			prHifInfo->au4PendingTxDoneCount[TC3_INDEX],
			prHifInfo->au4PendingTxDoneCount[TC4_INDEX]);

	return fgIsPendingTxDone;
}

void halPrintHifDbgInfo(IN struct ADAPTER *prAdapter)
{
	if (prAdapter->u4HifDbgFlag & DEG_HIF_ALL ||
		prAdapter->u4HifDbgFlag & DEG_HIF_DEFAULT_DUMP) {
		halPrintMailbox(prAdapter);
		halPollDbgCr(prAdapter, LP_DBGCR_POLL_ROUND);
	}
	prAdapter->u4HifDbgFlag = 0;
}

u_int8_t halIsTxResourceControlEn(IN struct ADAPTER *prAdapter)
{
	return TRUE;
}

void halTxResourceResetHwTQCounter(IN struct ADAPTER *prAdapter)
{
	uint32_t *pu4WHISR = NULL;
	uint16_t au2TxCount[16];

	pu4WHISR = (uint32_t *)kalMemAlloc(sizeof(uint32_t), PHY_MEM_TYPE);
	if (!pu4WHISR) {
		DBGLOG(INIT, ERROR, "Allocate pu4WHISR fail\n");
		return;
	}

	HAL_READ_INTR_STATUS(prAdapter, sizeof(uint32_t), (uint8_t *)pu4WHISR);
	if (HAL_IS_TX_DONE_INTR(*pu4WHISR))
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);

	if (pu4WHISR)
		kalMemFree(pu4WHISR, PHY_MEM_TYPE, sizeof(uint32_t));
}

uint32_t halGetHifTxPageSize(IN struct ADAPTER *prAdapter)
{
	if (!prAdapter->chip_info->is_support_cr4) {
		if (prAdapter->fgIsNicTxReousrceValid)
			return prAdapter->nicTxReousrce.u4DataResourceUnit;
		else
			return HIF_TX_PAGE_SIZE_STORED_FORWARD;
	}

    /*cr4 mode*/
	return HIF_TX_PAGE_SIZE;
}
/*----------------------------------------------------------------------------*/
/*!
* @brief Generic update Tx done counter
*
* @param prAdapter      Pointer to the Adapter structure.
* @param au2TxDoneCnt   Pointer to the final reference table
* @param au2TxRlsCnt    Pointer to the Tx done counter result got fom interrupt
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

void halTxGetFreeResource(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt, IN uint16_t *au2TxRlsCnt)
{
	uint8_t i;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	if (prBusInfo->halTxGetFreeResource)
		return prBusInfo->halTxGetFreeResource(prAdapter, au2TxDoneCnt, au2TxRlsCnt);

	/* 6632, 7668 ways */
	for (i = HIF_TX_AC0_INDEX; i <= HIF_TX_AC23_INDEX; i++)
		au2TxDoneCnt[i % WMM_AC_INDEX_NUM] += au2TxRlsCnt[i];
	au2TxDoneCnt[HIF_TX_CPU_INDEX] = au2TxRlsCnt[HIF_TX_CPU_INDEX];
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Update Tx done counter for mt7663
*
* @param prAdapter      Pointer to the Adapter structure.
* @param au2TxDoneCnt   Pointer to the final reference table
* @param au2TxRlsCnt    Pointer to the Tx done counter result got fom interrupt
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

void halTxGetFreeResource_v1(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt, IN uint16_t *au2TxRlsCnt)
{
	uint8_t i;

	for (i = HIF_TXC_IDX_0; i < HIF_TXC_IDX_NUM; i++)
		au2TxDoneCnt[i] = au2TxRlsCnt[i];
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Generic return free resource to TCs
*
* @param prAdapter      Pointer to the Adapter structure.
* @param au2TxDoneCnt   Pointer to the final reference table
* @param au2TxRlsCnt    Pointer to the Tx done counter result got fom interrupt
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

void halTxReturnFreeResource(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt)
{
	uint8_t i;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;
	uint16_t u2ReturnCnt;

	KAL_SPIN_LOCK_DECLARATION();


	if (prBusInfo->halTxReturnFreeResource)
		prBusInfo->halTxReturnFreeResource(prAdapter, au2TxDoneCnt);
	else {
		/* 6632, 7668 ways */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
		for (i = TC0_INDEX; i < TC_NUM; i++) {
			u2ReturnCnt = au2TxDoneCnt[nicTxGetTxQByTc(prAdapter, i)];
			nicTxReleaseResource_PSE(prAdapter, i, u2ReturnCnt, FALSE);
			prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[i] -= u2ReturnCnt;
		}
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Return free resource to TCs for mt7663
*
* @param prAdapter      Pointer to the Adapter structure.
* @param au2TxDoneCnt   Pointer to the final reference table
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void halTxReturnFreeResource_v1(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt)
{
	uint8_t i;
	uint16_t u2ReturnCnt;

	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	/* WMM 1 */
	for (i = HIF_TXC_IDX_0; i <= HIF_TXC_IDX_8; i++) {
		uint8_t ucTc;

		u2ReturnCnt = au2TxDoneCnt[i];

		if (i < HIF_TXC_IDX_5) {
			ucTc = HIF_TXC_IDX_2_TC_IDX_PSE(i);
			nicTxReleaseResource_PSE(prAdapter, ucTc, u2ReturnCnt, FALSE);
		} else {
			ucTc = HIF_TXC_IDX_2_TC_IDX_PLE(i);
			nicTxReleaseResource_PLE(prAdapter, ucTc, u2ReturnCnt, FALSE);
		}

		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[i] -= u2ReturnCnt;
	}

	/* WMM 2,3 */
	/*TBD*/

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Generic rollback resource to TCs
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

void halRestoreTxResource(IN struct ADAPTER *prAdapter)
{
	uint8_t i;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	KAL_SPIN_LOCK_DECLARATION();

	if (prBusInfo->halRestoreTxResource)
		prBusInfo->halRestoreTxResource(prAdapter);
	else {
		/* 6632, 7668 ways */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
		for (i = TC0_INDEX; i < TC_NUM; i++) {
			nicTxReleaseResource_PSE(prAdapter, i, prHifInfo->au4PendingTxDoneCount[i], FALSE);
			prHifInfo->au4PendingTxDoneCount[i] = 0;
		}
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Rollback resource to TCs for mt7663
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

void halRestoreTxResource_v1(IN struct ADAPTER *prAdapter)
{
	uint8_t i;

	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	/* WMM 1 */
	/* PSE pages: HIF_TXC_IDX_0- */
	for (i = HIF_TXC_IDX_0; i <= HIF_TXC_IDX_4; i++) {
		nicTxReleaseResource_PSE(prAdapter, HIF_TXC_IDX_2_TC_IDX_PSE(i),
					prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[i],
					FALSE);

		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[i] = 0;
	}

	/* PLE pages */
	for (i = HIF_TXC_IDX_5; i <= HIF_TXC_IDX_8; i++) {
		nicTxReleaseResource_PLE(prAdapter, HIF_TXC_IDX_2_TC_IDX_PLE(i),
					prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[i],
					FALSE);

		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[i] = 0;
	}

	/* WMM 2,3 */
	/*TBD*/

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
}

void halUpdateTxDonePendingCount(IN struct ADAPTER *prAdapter, IN u_int8_t isIncr, IN uint8_t ucTc, IN uint32_t u4Len)
{
	uint8_t u2PageCnt;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;

	u2PageCnt = halTxGetPageCount(prAdapter, u4Len, FALSE);

	if (prBusInfo->halUpdateTxDonePendingCount)
		prBusInfo->halUpdateTxDonePendingCount(prAdapter, isIncr, ucTc, u2PageCnt);
	else {
		/* 6632, 7668 ways */
		if (isIncr)
			prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[ucTc] += u2PageCnt;
		else
			prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[ucTc] -= u2PageCnt;
	}
}

void halUpdateTxDonePendingCount_v1(IN struct ADAPTER *prAdapter, IN u_int8_t isIncr, IN uint8_t ucTc, IN uint16_t u2Cnt)
{
	uint8_t idx;

	/* Update PSE part */
	idx = TC_IDX_PSE_2_HIF_TXC_IDX(ucTc);

	if (idx >= HIF_TXC_IDX_NUM)
		ASSERT(0);

	if (isIncr)
		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[idx] += u2Cnt;
	else
		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[idx] -= u2Cnt;

	/* Update PLE part */
	if (!nicTxResourceIsPleCtrlNeeded(prAdapter, ucTc))
		return;

	idx = TC_IDX_PLE_2_HIF_TXC_IDX(ucTc);

	if (idx >= HIF_TXC_IDX_NUM)
		ASSERT(0);

	if (isIncr)
		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[idx] += NIX_TX_PLE_PAGE_CNT_PER_FRAME;
	else
		prAdapter->prGlueInfo->rHifInfo.au4PendingTxDoneCount[idx] -= NIX_TX_PLE_PAGE_CNT_PER_FRAME;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Send HIF_CTRL command to inform FW stop send packet/event to host
*	suspend = 1
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (void)
*/
/*----------------------------------------------------------------------------*/
void halPreSuspendCmd(IN struct ADAPTER *prAdapter)
{
	struct CMD_HIF_CTRL rCmdHifCtrl;
	uint32_t rStatus;

	rCmdHifCtrl.ucHifType = ENUM_HIF_TYPE_SDIO;
	rCmdHifCtrl.ucHifDirection = ENUM_HIF_TX;
	rCmdHifCtrl.ucHifStop = 1;
	rCmdHifCtrl.ucHifSuspend = 1;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				CMD_ID_HIF_CTRL,  /* ucCID */
				TRUE, /* fgSetQuery */
				FALSE,	  /* fgNeedResp */
				FALSE,	  /* fgIsOid */
				NULL, /* nicEventHifCtrl */
				NULL, /* pfCmdTimeoutHandler */
				sizeof(struct CMD_HIF_CTRL),
				(uint8_t *)&rCmdHifCtrl,  /* pucInfoBuffer */
				NULL, /* pvSetQueryBuffer */
				0 /* u4SetQueryBufferLen */
		);

	if (kalIsResetting())
		return;

	ASSERT(rStatus == WLAN_STATUS_PENDING);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Send HIF_CTRL command to inform FW allow send packet/event to host
*	suspend = 0
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (void)
*/
/*----------------------------------------------------------------------------*/
void halPreResumeCmd(IN struct ADAPTER *prAdapter)
{
	struct CMD_HIF_CTRL rCmdHifCtrl;
	uint32_t rStatus;

	rCmdHifCtrl.ucHifType = ENUM_HIF_TYPE_SDIO;
	rCmdHifCtrl.ucHifDirection = ENUM_HIF_TX;
	rCmdHifCtrl.ucHifStop = 0;
	rCmdHifCtrl.ucHifSuspend = 0;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				CMD_ID_HIF_CTRL,  /* ucCID */
				TRUE, /* fgSetQuery */
				FALSE,	  /* fgNeedResp */
				FALSE,	  /* fgIsOid */
				NULL, /* nicEventHifCtrl */
				NULL, /* pfCmdTimeoutHandler */
				sizeof(struct CMD_HIF_CTRL),
				(uint8_t *)&rCmdHifCtrl,  /* pucInfoBuffer */
				NULL, /* pvSetQueryBuffer */
				0 /* u4SetQueryBufferLen */
		);

	if (kalIsResetting())
		return;

	ASSERT(rStatus == WLAN_STATUS_PENDING);
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
	if (!prAdapter)
		return FALSE;

	if (!prAdapter->prGlueInfo)
		return FALSE;

	if (prAdapter->prGlueInfo->u4ReadyFlag == 0)
		return FALSE;

	if (pucState)
		*pucState = prAdapter->prGlueInfo->rHifInfo.state;

	if (prAdapter->prGlueInfo->rHifInfo.state != SDIO_STATE_READY)
		return FALSE;

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
	enum sdio_state state;

	if (!prAdapter)
		return FALSE;

	if (!prAdapter->prGlueInfo)
		return FALSE;

	state = prAdapter->prGlueInfo->rHifInfo.state;

	if (state == SDIO_STATE_SUSPEND)
		return TRUE;

	return FALSE;
}


