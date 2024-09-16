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
 ** Id: //Department/DaVinci/BRANCHES/
 *			MT6620_WIFI_DRIVER_V2_3/include/chips/connac.h#1
 */

/*! \file  connac.h
 *    \brief This file contains the info of CONNAC
 */

#ifndef _CMM_ASIC_CONNAC_H
#define _CMM_ASIC_CONNAC_H

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define CONNAC_CHIP_IP_VERSION			(0x10020300)
#define CONNAC_CHIP_IP_CONFIG			(0x1)
#define CONNAC_CHIP_ADIE_INFO			(0x31)
#define USB_HIF_TXD_LEN    4

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void asicCapInit(IN struct ADAPTER *prAdapter);
uint32_t asicGetFwDlInfo(struct ADAPTER *prAdapter,
	char *pcBuf, int i4TotalLen);
void asicEnableFWDownload(IN struct ADAPTER *prAdapter,
	IN u_int8_t fgEnable);
uint32_t asicGetChipID(struct ADAPTER *prAdapter);
void fillNicTxDescAppend(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	OUT uint8_t *prTxDescBuffer);
void fillNicTxDescAppendWithCR4(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	OUT uint8_t *prTxDescBuffer);
void fillTxDescAppendByHost(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
	IN phys_addr_t rDmaAddr, IN uint32_t u4Idx, IN u_int8_t fgIsLast,
	OUT uint8_t *pucBuffer);
void fillTxDescAppendByHostV2(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
	IN phys_addr_t rDmaAddr, IN uint32_t u4Idx, IN u_int8_t fgIsLast,
	OUT uint8_t *pucBuffer);
void fillTxDescAppendByCR4(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
	IN phys_addr_t rDmaAddr, IN uint32_t u4Idx, IN u_int8_t fgIsLast,
	OUT uint8_t *pucBuffer);
void fillTxDescTxByteCount(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	struct HW_MAC_TX_DESC *prTxDesc);
void fillTxDescTxByteCountWithCR4(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	struct HW_MAC_TX_DESC *prTxDesc);

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
/* DMS Scheduler Init */
void asicPcieDmaShdlInit(IN struct ADAPTER *prAdapter);
void asicPdmaLoopBackConfig(struct GLUE_INFO *prGlueInfo, u_int8_t fgEnable);
void asicPdmaConfig(struct GLUE_INFO *prGlueInfo, u_int8_t fgEnable);
void asicEnableInterrupt(IN struct ADAPTER *prAdapter);
void asicDisableInterrupt(IN struct ADAPTER *prAdapter);
void asicLowPowerOwnRead(IN struct ADAPTER *prAdapter, OUT u_int8_t *pfgResult);
void asicLowPowerOwnSet(IN struct ADAPTER *prAdapter, OUT u_int8_t *pfgResult);
void asicLowPowerOwnClear(IN struct ADAPTER *prAdapter,
	OUT u_int8_t *pfgResult);
void asicLowPowerOwnClearPCIe(IN struct ADAPTER *prAdapter,
	OUT u_int8_t *pfgResult);
void asicWakeUpWiFi(IN struct ADAPTER *prAdapter);
bool asicIsValidRegAccess(IN struct ADAPTER *prAdapter, IN uint32_t u4Register);
void asicGetMailboxStatus(IN struct ADAPTER *prAdapter, OUT uint32_t *pu4Val);
void asicSetDummyReg(struct GLUE_INFO *prGlueInfo);
void asicCheckDummyReg(struct GLUE_INFO *prGlueInfo);
#if CFG_SUPPORT_PCIE_L2
void asicPdmaStop(struct GLUE_INFO *prGlueInfo, u_int8_t enable);
#endif
#endif /* _HIF_PCIE */

#if defined(_HIF_USB)
/* DMS Scheduler Init */
void asicUsbDmaShdlInit(IN struct ADAPTER *prAdapter);
void asicUdmaTxTimeoutEnable(IN struct ADAPTER *prAdapter);
u_int8_t asicUsbSuspend(IN struct ADAPTER *prAdapter,
	IN struct GLUE_INFO *prGlueInfo);
uint8_t asicUsbEventEpDetected(IN struct ADAPTER *prAdapter);
void asicUdmaRxFlush(IN struct ADAPTER *prAdapter, IN u_int8_t bEnable);
void asicPdmaHifReset(IN struct ADAPTER *prAdapter, IN u_int8_t bRelease);
void fillUsbHifTxDesc(IN uint8_t **pDest, IN uint16_t *pInfoBufLen);
#endif /* _HIF_USB */
#endif /* _CMM_ASIC_CONNAC_H */

