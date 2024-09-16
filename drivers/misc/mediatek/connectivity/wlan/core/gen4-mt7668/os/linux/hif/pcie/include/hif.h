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
/*! \file   "hif.h"
*    \brief  Functions for the driver to register bus and setup the IRQ
*
*    Functions for the driver to register bus and setup the IRQ
*/

#ifndef _HIF_H
#define _HIF_H

#include "hif_pci.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#if defined(_HIF_PCIE)
#define HIF_NAME "PCIE"
#else
#error "No HIF defined!"
#endif

#define HIF_TX_PREALLOC_DATA_BUFFER			1

#define HIF_NUM_OF_QM_RX_PKT_NUM			4096
#define HIF_IST_LOOP_COUNT					32
#define HIF_IST_TX_THRESHOLD				1 /* Min msdu count to trigger Tx during INT polling state */

#define HIF_TX_BUFF_COUNT_TC0				4096
#define HIF_TX_BUFF_COUNT_TC1				4096
#define HIF_TX_BUFF_COUNT_TC2				4096
#define HIF_TX_BUFF_COUNT_TC3				4096
#define HIF_TX_BUFF_COUNT_TC4				(TX_RING_SIZE - 1)
#define HIF_TX_BUFF_COUNT_TC5				4096

#define HIF_TX_RESOURCE_CTRL                1 /* enable/disable TX resource control */

#define HIF_TX_PAGE_SIZE_IN_POWER_OF_2		11
#define HIF_TX_PAGE_SIZE					2048	/* in unit of bytes */

#define HIF_EXTRA_IO_BUFFER_SIZE			0

#define HIF_TX_COALESCING_BUFFER_SIZE		(TX_BUFFER_NORMSIZE)
#define HIF_RX_COALESCING_BUFFER_SIZE		(RX_BUFFER_AGGRESIZE)

#define HIF_CR4_FWDL_SECTION_NUM			1
#define HIF_IMG_DL_STATUS_PORT_IDX			1

#define HIF_TX_INIT_CMD_PORT				TX_RING_FWDL_IDX_3

#define HIF_TX_MSDU_TOKEN_NUM				8192

#define HIF_TX_PAYLOAD_LENGTH				72
#define HIF_TX_DESC_PAYLOAD_LENGTH			(NIC_TX_HEAD_ROOM + HIF_TX_PAYLOAD_LENGTH)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef struct _MSDU_TOKEN_ENTRY_T {
	UINT_32 u4Token;
	BOOLEAN fgInUsed;
	P_MSDU_INFO_T prMsduInfo;
	P_NATIVE_PACKET prPacket;
	dma_addr_t rDmaAddr;
	UINT_32 u4DmaLength;
	dma_addr_t rPktDmaAddr;
	UINT_32 u4PktDmaLength;
}	MSDU_TOKEN_ENTRY_T, *P_MSDU_TOKEN_ENTRY_T;

typedef struct _MSDU_TOKEN_INFO_T {
	INT_32 i4UsedCnt;
	P_MSDU_TOKEN_ENTRY_T aprTokenStack[HIF_TX_MSDU_TOKEN_NUM];
	spinlock_t rTokenLock;

	MSDU_TOKEN_ENTRY_T arToken[HIF_TX_MSDU_TOKEN_NUM];
} MSDU_TOKEN_INFO_T, *P_MSDU_TOKEN_INFO_T;

/* host interface's private data structure, which is attached to os glue
** layer info structure.
 */
typedef struct _GL_HIF_INFO_T {
	struct pci_dev *pdev;

	PUCHAR CSRBaseAddress;	/* PCI MMIO Base Address, all access will use */

	/* Shared memory of all 1st pre-allocated
	 * TxBuf associated with each TXD
	 */
	RTMP_DMABUF TxBufSpace[NUM_OF_TX_RING];
	RTMP_DMABUF TxDescRing[NUM_OF_TX_RING];	/* Shared memory for Tx descriptors */
	RTMP_TX_RING TxRing[NUM_OF_TX_RING];	/* AC0~3 + HCCA */
	spinlock_t TxRingLock[NUM_OF_TX_RING];	/* Rx Ring spinlock */

	RTMP_DMABUF RxDescRing[2];	/* Shared memory for RX descriptors */
	RTMP_RX_RING RxRing[NUM_OF_RX_RING];
	spinlock_t RxRingLock[NUM_OF_RX_RING];	/* Rx Ring spinlock */

	BOOLEAN fgIntReadClear;
	BOOLEAN fgMbxReadClear;

	UINT_32 u4IntStatus;

	MSDU_TOKEN_INFO_T rTokenInfo;

	spinlock_t rDynMapRegLock;
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

VOID halWpdmaAllocRing(P_GLUE_INFO_T prGlueInfo);
VOID halWpdmaFreeRing(P_GLUE_INFO_T prGlueInfo);
VOID halWpdmaInitRing(P_GLUE_INFO_T prGlueInfo);
VOID halWpdmaProcessCmdDmaDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_16 u2Port);
VOID halWpdmaProcessDataDmaDone(IN P_GLUE_INFO_T prGlueInfo, IN UINT_16 u2Port);
UINT_32 halWpdmaGetRxDmaDoneCnt(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRingNum);
VOID kalPciUnmapToDev(IN P_GLUE_INFO_T prGlueInfo, IN dma_addr_t rDmaAddr, IN UINT_32 u4Length);

VOID halInitMsduTokenInfo(IN P_ADAPTER_T prAdapter);
VOID halUninitMsduTokenInfo(IN P_ADAPTER_T prAdapter);
UINT_32 halGetMsduTokenFreeCnt(IN P_ADAPTER_T prAdapter);
P_MSDU_TOKEN_ENTRY_T halGetMsduTokenEntry(IN P_ADAPTER_T prAdapter, UINT_32 u4TokenNum);
P_MSDU_TOKEN_ENTRY_T halAcquireMsduToken(IN P_ADAPTER_T prAdapter);
VOID halReturnMsduToken(IN P_ADAPTER_T prAdapter, UINT_32 u4TokenNum);

VOID halTxUpdateCutThroughDesc(P_GLUE_INFO_T prGlueInfo, P_MSDU_INFO_T prMsduInfo,
	P_MSDU_TOKEN_ENTRY_T prToken);
BOOLEAN halIsStaticMapBusAddr(IN UINT_32 u4Addr);
BOOLEAN halChipToStaticMapBusAddr(IN UINT_32 u4ChipAddr, OUT PUINT_32 pu4BusAddr);
BOOLEAN halGetDynamicMapReg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 u4ChipAddr, OUT PUINT_32 pu4Value);
BOOLEAN halSetDynamicMapReg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 u4ChipAddr, IN UINT_32 u4Value);
VOID halEnhancedWpdmaConfig(P_GLUE_INFO_T prGlueInfo, BOOLEAN enable);
VOID halWpdmaConfig(P_GLUE_INFO_T prGlueInfo, BOOLEAN enable);

BOOL kalDevReadData(IN P_GLUE_INFO_T prGlueInfo, IN UINT_16 u2Port, IN OUT P_SW_RFB_T prSwRfb);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
