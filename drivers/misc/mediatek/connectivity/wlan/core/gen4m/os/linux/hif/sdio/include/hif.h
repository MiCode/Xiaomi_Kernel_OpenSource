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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/hif/sdio/include/hif.h#1
*/

/*! \file   "hif.h"
*    \brief  Functions for the driver to register bus and setup the IRQ
*
*    Functions for the driver to register bus and setup the IRQ
*/


#ifndef _HIF_H
#define _HIF_H

#if MTK_WCN_HIF_SDIO
#include "hif_sdio.h"
#endif

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#if MTK_WCN_HIF_SDIO
#define HIF_SDIO_SUPPORT_GPIO_SLEEP_MODE			1
#else
#define HIF_SDIO_SUPPORT_GPIO_SLEEP_MODE			0
#endif

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#if defined(_HIF_SDIO)
#define HIF_NAME "SDIO"
#else
#error "No HIF defined!"
#endif


/* Enable driver timing profiling */
#define CFG_SDIO_TIMING_PROFILING       0

#define CFG_SDIO_INT_LOG_CNT            8

#define SDIO_X86_WORKAROUND_WRITE_MCR   0x00C4
#define HIF_NUM_OF_QM_RX_PKT_NUM        512

#define HIF_TX_INIT_CMD_PORT            TX_RING_FWDL_IDX_3

#define HIF_IST_LOOP_COUNT              128
#define HIF_IST_TX_THRESHOLD            32 /* Min msdu count to trigger Tx during INT polling state */

#define HIF_TX_MAX_AGG_LENGTH           (511 * 512) /* 511 blocks x 512 */

#define HIF_RX_MAX_AGG_NUM              16
/*!< Setting the maximum RX aggregation number 0: no limited (16) */

#define HIF_TX_BUFF_COUNT_TC0           8
#define HIF_TX_BUFF_COUNT_TC1           167
#define HIF_TX_BUFF_COUNT_TC2           8
#define HIF_TX_BUFF_COUNT_TC3           8
#define HIF_TX_BUFF_COUNT_TC4           7
#define HIF_TX_BUFF_COUNT_TC5           0

#define HIF_TX_RESOURCE_CTRL            1 /* enable/disable TX resource control */
#define HIF_TX_RESOURCE_CTRL_PLE        1 /* enable/disable TX resource control PLE */


#define HIF_TX_PAGE_SIZE_IN_POWER_OF_2      11
#define HIF_TX_PAGE_SIZE                    2048	/* in unit of bytes */
#define HIF_TX_PAGE_SIZE_STORED_FORWARD     128	/* in unit of bytes */

#define HIF_EXTRA_IO_BUFFER_SIZE \
	(sizeof(struct ENHANCE_MODE_DATA_STRUCT) + HIF_RX_COALESCING_BUF_COUNT * HIF_RX_COALESCING_BUFFER_SIZE)

#define HIF_CR4_FWDL_SECTION_NUM            2
#define HIF_IMG_DL_STATUS_PORT_IDX          0

#define HIF_RX_ENHANCE_MODE_PAD_LEN         4

#define HIF_TX_TERMINATOR_LEN               4

#if CFG_SDIO_TX_AGG
#define HIF_TX_COALESCING_BUFFER_SIZE       (HIF_TX_MAX_AGG_LENGTH)
#else
#define HIF_TX_COALESCING_BUFFER_SIZE       (CFG_TX_MAX_PKT_SIZE)
#endif

#if CFG_SDIO_RX_AGG
#define HIF_RX_COALESCING_BUFFER_SIZE       ((HIF_RX_MAX_AGG_NUM  + 1) * CFG_RX_MAX_PKT_SIZE)
#else
#define HIF_RX_COALESCING_BUFFER_SIZE       (CFG_RX_MAX_PKT_SIZE)
#endif

#define HIF_RX_COALESCING_BUF_COUNT         16

/* WHISR device to host (D2H) */
/* N9 Interrupt Host to stop tx/rx operation (at the moment, HIF tx/rx are stopted) */
#define SER_SDIO_N9_HOST_STOP_TX_RX_OP             BIT(8)
/* N9 Interrupt Host to stop tx operation (at the moment, HIF tx are stopted) */
#define SER_SDIO_N9_HOST_STOP_TX_OP                BIT(9)
/* N9 Interrupt Host all modules were reset done (to let host reinit HIF) */
#define SER_SDIO_N9_HOST_RESET_DONE                BIT(10)
/* N9 Interrupt Host System Error Recovery Done */
#define SER_SDIO_N9_HOST_RECOVERY_DONE             BIT(11)

/* WSICR host to device (H2D) */
/* Host ACK HIF tx/rx ring stop operatio */
#define SER_SDIO_HOST_N9_STOP_TX_RX_OP_ACK         BIT(19)
/* Host interrupt N9 HIF init done */
#define SER_SDIO_HOST_N9_RESET_DONE_ACK            BIT(20)
/* Host interrupt N9 System Error Recovery done */
#define SER_SDIO_HOST_N9_RECOVERY_DONE_ACK         BIT(21)
enum HIF_TX_COUNT_IDX_T {
	HIF_TXC_IDX_0,
	HIF_TXC_IDX_1,
	HIF_TXC_IDX_2,
	HIF_TXC_IDX_3,
	HIF_TXC_IDX_4,
	HIF_TXC_IDX_5,
	HIF_TXC_IDX_6,
	HIF_TXC_IDX_7,
	HIF_TXC_IDX_8,
	HIF_TXC_IDX_9,
	HIF_TXC_IDX_10,
	HIF_TXC_IDX_11,
	HIF_TXC_IDX_12,
	HIF_TXC_IDX_13,
	HIF_TXC_IDX_14,
	HIF_TXC_IDX_15,
	HIF_TXC_IDX_NUM
};


/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct ENHANCE_MODE_DATA_STRUCT;	/* declare SDIO_CTRL_T */

struct SDIO_STAT_COUNTER {
	/* Tx data */
	uint32_t u4DataPortWriteCnt;
	uint32_t u4DataPktWriteCnt;
	uint32_t u4DataPortKickCnt;

	/* Tx command */
	uint32_t u4CmdPortWriteCnt;
	uint32_t u4CmdPktWriteCnt;

	/* Tx done interrupt */
	uint32_t u4TxDoneCnt[HIF_TXC_IDX_NUM];
	uint32_t u4TxDoneIntCnt[HIF_TXC_IDX_NUM];
	uint32_t u4TxDoneIntTotCnt;
	uint32_t u4TxDonePendingPktCnt;

	uint32_t u4IntReadCnt;
	uint32_t u4IntCnt;

	/* Rx data/cmd*/
	uint32_t u4PortReadCnt[2];
	uint32_t u4PktReadCnt[2];

	uint32_t u4RxBufUnderFlowCnt;

#if CFG_SDIO_TIMING_PROFILING
	uint32_t u4TxDataCpTime;
	uint32_t u4TxDataFreeTime;

	uint32_t u4RxDataCpTime;
	uint32_t u4PortReadTime;

	uint32_t u4TxDoneIntTime;
	uint32_t u4IntReadTime;
#endif
};

struct SDIO_RX_COALESCING_BUF {
	struct QUE_ENTRY rQueEntry;
	void *pvRxCoalescingBuf;
	uint32_t u4BufSize;
	uint32_t u4PktCount;
	uint32_t u4PktTotalLength;

	uint32_t u4IntLogIdx;
};

struct SDIO_INT_LOG_T {
	uint32_t u4Idx;
	uint8_t aucIntSts[128];
	uint32_t u4Flag;
	uint16_t au2RxPktLen[HIF_RX_MAX_AGG_NUM];
	uint32_t au4RxPktInfo[HIF_RX_MAX_AGG_NUM];
	uint8_t ucRxPktCnt;
};

/* host interface's private data structure, which is attached to os glue
** layer info structure.
 */
struct GL_HIF_INFO {
#if MTK_WCN_HIF_SDIO
	unsigned long cltCtx;

	const struct MTK_WCN_HIF_SDIO_FUNCINFO *prFuncInfo;
#else
	struct sdio_func *func;
#endif

	struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl;

	u_int8_t fgIntReadClear;
	u_int8_t fgMbxReadClear;
	struct QUE rFreeQueue;
	u_int8_t fgIsPendingInt;

	uint32_t au4PendingTxDoneCount[HIF_TXC_IDX_NUM];

	/* Statistic counter */
	struct SDIO_STAT_COUNTER rStatCounter;

	struct SDIO_RX_COALESCING_BUF rRxCoalesingBuf[HIF_RX_COALESCING_BUF_COUNT];

	struct QUE rRxDeAggQueue;
	struct QUE rRxFreeBufQueue;

	struct mutex rRxFreeBufQueMutex;
	struct mutex rRxDeAggQueMutex;

	/* Error handling */
	u_int8_t fgSkipRx;

	struct SDIO_INT_LOG_T arIntLog[CFG_SDIO_INT_LOG_CNT];
	uint32_t u4IntLogIdx;
	uint8_t ucIntLogEntry;
	uint8_t fgForceFwOwn;
};

struct BUS_INFO {
	void (*halTxGetFreeResource)(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt, IN uint16_t *au2TxRlsCnt);
	void (*halTxReturnFreeResource)(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt);
	void (*halRestoreTxResource)(IN struct ADAPTER *prAdapter);
	void (*halUpdateTxDonePendingCount)(IN struct ADAPTER *prAdapter,
					    IN u_int8_t isIncr, IN uint8_t ucTc, IN uint16_t u2Cnt);
};

enum HIF_SDIO_INT_STS {
	SDIO_INT_RX_ENHANCE = 0,
	SDIO_INT_DRV_OWN,
	SDIO_INT_WAKEUP_DSLP
};

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

#if CFG_SDIO_TIMING_PROFILING
#define SDIO_TIME_INTERVAL_DEC()			KAL_TIME_INTERVAL_DECLARATION()
#define SDIO_REC_TIME_START()				KAL_REC_TIME_START()
#define SDIO_REC_TIME_END()					KAL_REC_TIME_END()
#define SDIO_GET_TIME_INTERVAL()			KAL_GET_TIME_INTERVAL()
#define SDIO_ADD_TIME_INTERVAL(_Interval)	KAL_ADD_TIME_INTERVAL(_Interval)
#else
#define SDIO_TIME_INTERVAL_DEC()
#define SDIO_REC_TIME_START()
#define SDIO_REC_TIME_END()
#define SDIO_GET_TIME_INTERVAL()
#define SDIO_ADD_TIME_INTERVAL(_Interval)
#endif

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove);

void glUnregisterBus(remove_card pfRemove);

void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie);

void glClearHifInfo(struct GLUE_INFO *prGlueInfo);

u_int8_t glBusInit(void *pvData);

void glBusRelease(void *pData);

int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie);

void glBusFreeIrq(void *pvData, void *pvCookie);

void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode);

void glGetDev(void *ctx, struct device **dev);

void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev);

u_int8_t glWakeupSdio(struct GLUE_INFO *prGlueInfo);

#if !CFG_SDIO_INTR_ENHANCE
void halRxSDIOReceiveRFBs(IN struct ADAPTER *prAdapter);

uint32_t halRxReadBuffer(IN struct ADAPTER *prAdapter, IN OUT struct SW_RFB *prSwRfb);

#else
void halRxSDIOEnhanceReceiveRFBs(IN struct ADAPTER *prAdapter);

uint32_t halRxEnhanceReadBuffer(IN struct ADAPTER *prAdapter, IN uint32_t u4DataPort,
	IN uint16_t u2RxLength, IN OUT struct SW_RFB *prSwRfb);

void halProcessEnhanceInterruptStatus(IN struct ADAPTER *prAdapter);

#endif /* CFG_SDIO_INTR_ENHANCE */

#if CFG_SDIO_RX_AGG
void halRxSDIOAggReceiveRFBs(IN struct ADAPTER *prAdapter);
#endif

void halPutMailbox(IN struct ADAPTER *prAdapter, IN uint32_t u4MailboxNum, IN uint32_t u4Data);
void halGetMailbox(IN struct ADAPTER *prAdapter, IN uint32_t u4MailboxNum, OUT uint32_t *pu4Data);
void halDeAggRxPkt(struct ADAPTER *prAdapter, struct SDIO_RX_COALESCING_BUF *prRxBuf);
void halPrintMailbox(IN struct ADAPTER *prAdapter);
void halPollDbgCr(IN struct ADAPTER *prAdapter, IN uint32_t u4LoopCount);
void halTxGetFreeResource_v1(IN struct ADAPTER *prAdapter, IN uint16_t *au2TxDoneCnt, IN uint16_t *au2TxRlsCnt);

u_int8_t halIsPendingTxDone(IN struct ADAPTER *prAdapter);
void halDumpIntLog(IN struct ADAPTER *prAdapter);
void halTagIntLog(IN struct ADAPTER *prAdapter, IN enum HIF_SDIO_INT_STS eTag);
void halRecIntLog(IN struct ADAPTER *prAdapter, IN struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl);
struct SDIO_INT_LOG_T *halGetIntLog(IN struct ADAPTER *prAdapter, IN uint32_t u4Idx);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
