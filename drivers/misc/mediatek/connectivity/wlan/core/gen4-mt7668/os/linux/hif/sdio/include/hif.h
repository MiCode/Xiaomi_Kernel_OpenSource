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

#define HIF_TX_PAGE_SIZE_IN_POWER_OF_2      11
#define HIF_TX_PAGE_SIZE                    2048	/* in unit of bytes */

#define HIF_EXTRA_IO_BUFFER_SIZE \
	(sizeof(ENHANCE_MODE_DATA_STRUCT_T) + HIF_RX_COALESCING_BUF_COUNT * HIF_RX_COALESCING_BUFFER_SIZE)

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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef struct _ENHANCE_MODE_DATA_STRUCT_T SDIO_CTRL_T, *P_SDIO_CTRL_T;

typedef struct _SDIO_STAT_COUNTER_T {
	/* Tx data */
	UINT_32 u4DataPortWriteCnt;
	UINT_32 u4DataPktWriteCnt;
	UINT_32 u4DataPortKickCnt;

	/* Tx command */
	UINT_32 u4CmdPortWriteCnt;
	UINT_32 u4CmdPktWriteCnt;

	/* Tx done interrupt */
	UINT_32 u4TxDoneCnt[16];
	UINT_32 u4TxDoneIntCnt[16];
	UINT_32 u4TxDoneIntTotCnt;
	UINT_32 u4TxDonePendingPktCnt;

	UINT_32 u4IntReadCnt;
	UINT_32 u4IntCnt;

	/* Rx data/cmd*/
	UINT_32 u4PortReadCnt[2];
	UINT_32 u4PktReadCnt[2];

	UINT_32 u4RxBufUnderFlowCnt;

#if CFG_SDIO_TIMING_PROFILING
	UINT_32 u4TxDataCpTime;
	UINT_32 u4TxDataFreeTime;

	UINT_32 u4RxDataCpTime;
	UINT_32 u4PortReadTime;

	UINT_32 u4TxDoneIntTime;
	UINT_32 u4IntReadTime;
#endif
} SDIO_STAT_COUNTER_T, *P_SDIO_STAT_COUNTER_T;

typedef struct _SDIO_RX_COALESCING_BUF_T {
	QUE_ENTRY_T rQueEntry;
	PVOID pvRxCoalescingBuf;
	UINT_32 u4BufSize;
	UINT_32 u4PktCount;
} SDIO_RX_COALESCING_BUF_T, *P_SDIO_RX_COALESCING_BUF_T;

/* host interface's private data structure, which is attached to os glue
** layer info structure.
 */
typedef struct _GL_HIF_INFO_T {
#if MTK_WCN_HIF_SDIO
	MTK_WCN_HIF_SDIO_CLTCTX cltCtx;

	const MTK_WCN_HIF_SDIO_FUNCINFO *prFuncInfo;
#else
	struct sdio_func *func;
#endif

	P_SDIO_CTRL_T prSDIOCtrl;

	BOOLEAN fgIntReadClear;
	BOOLEAN fgMbxReadClear;
	QUE_T rFreeQueue;
	BOOLEAN fgIsPendingInt;

	UINT_32 au4PendingTxDoneCount[6];

	/* Statistic counter */
	SDIO_STAT_COUNTER_T rStatCounter;

	SDIO_RX_COALESCING_BUF_T rRxCoalesingBuf[HIF_RX_COALESCING_BUF_COUNT];

	QUE_T rRxDeAggQueue;
	QUE_T rRxFreeBufQueue;

	struct mutex rRxFreeBufQueMutex;
	struct mutex rRxDeAggQueMutex;
} GL_HIF_INFO_T, *P_GL_HIF_INFO_T;

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

WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove);

VOID glUnregisterBus(remove_card pfRemove);

VOID glSetHifInfo(P_GLUE_INFO_T prGlueInfo, ULONG ulCookie);

VOID glClearHifInfo(P_GLUE_INFO_T prGlueInfo);

BOOL glBusInit(PVOID pvData);

VOID glBusRelease(PVOID pData);

INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie);

VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie);

VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode);

void glGetDev(PVOID ctx, struct device **dev);

void glGetHifDev(P_GL_HIF_INFO_T prHif, struct device **dev);

BOOLEAN glWakeupSdio(P_GLUE_INFO_T prGlueInfo);

#if !CFG_SDIO_INTR_ENHANCE
VOID halRxSDIOReceiveRFBs(IN P_ADAPTER_T prAdapter);

WLAN_STATUS halRxReadBuffer(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

#else
VOID halRxSDIOEnhanceReceiveRFBs(IN P_ADAPTER_T prAdapter);

WLAN_STATUS halRxEnhanceReadBuffer(IN P_ADAPTER_T prAdapter, IN UINT_32 u4DataPort,
	IN UINT_16 u2RxLength, IN OUT P_SW_RFB_T prSwRfb);

VOID halProcessEnhanceInterruptStatus(IN P_ADAPTER_T prAdapter);

#endif /* CFG_SDIO_INTR_ENHANCE */

#if CFG_SDIO_RX_AGG
VOID halRxSDIOAggReceiveRFBs(IN P_ADAPTER_T prAdapter);
#endif

VOID halPutMailbox(IN P_ADAPTER_T prAdapter, IN UINT_32 u4MailboxNum, IN UINT_32 u4Data);
VOID halGetMailbox(IN P_ADAPTER_T prAdapter, IN UINT_32 u4MailboxNum, OUT PUINT_32 pu4Data);
VOID halDeAggRxPkt(P_ADAPTER_T prAdapter, P_SDIO_RX_COALESCING_BUF_T prRxBuf);
VOID halPrintMailbox(IN P_ADAPTER_T prAdapter);
VOID halPollDbgCr(IN P_ADAPTER_T prAdapter, IN UINT_32 u4LoopCount);
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
