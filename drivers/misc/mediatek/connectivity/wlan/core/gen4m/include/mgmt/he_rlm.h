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

/*! \file   "rlm.h"
 *  \brief
 */

#ifndef _HE_RLM_H
#define _HE_RLM_H

#if (CFG_SUPPORT_802_11AX == 1)

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

#define HE_PHY_CAP2_INFO_DEFAULT_VAL (HE_PHY_CAP2_FULL_BW_UL_MU_MIMO | \
					HE_PHY_CAP2_PARTIAL_BW_UL_MU_MIMO)
#define HE_PHY_CAP6_INFO_DEFAULT_VAL (HE_PHY_CAP6_PPE_THRESHOLD)

#define PPE_RU_IDX_SIZE              4
#define PPE_SUBFIELD_BITS_NUM        6

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */
#define HE_RESET_MAC_CAP(_aucHeMacCapInfo) \
	(memset(_aucHeMacCapInfo, 0, HE_MAC_CAP_BYTE_NUM))

#define HE_RESET_PHY_CAP(_aucHePhyCapInfo) \
{ \
	memset(_aucHePhyCapInfo, 0, HE_PHY_CAP_BYTE_NUM); \
	_aucHePhyCapInfo[2] = (u_int8_t)HE_PHY_CAP2_INFO_DEFAULT_VAL; \
	_aucHePhyCapInfo[6] = (u_int8_t)HE_PHY_CAP6_INFO_DEFAULT_VAL; \
}

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */
struct __HE_TWT_INFO_T {
	u_int8_t ucFlowType;
	u_int8_t ucFlowID;
	u_int8_t ucIntervalExp;
	u_int8_t ucProtection;
};

struct __HE_CFG_INFO_T {
	u_int8_t fgHeSupport;              /* HE Support */
	u_int8_t fgHeEnable;               /* HE Enable/Disable Control */
	u_int8_t fgTwtRequesterEnable;     /* HW TWT Requester  Control */
	u_int8_t fgTwtResponderEnable;     /* HW TWT Responder Control */
	u_int8_t fgHtcHe;                  /* +HTC HE Support */
	u_int8_t fgFragment;               /* Fragmentation Support */
	u_int8_t fgMultiTidAgg;            /* Multi-TID Aggregation Support */
	u_int8_t fgAmsduFragment;          /* A-MSDU Fragmentation Support */
	u_int8_t fg32bitBaBitmap;          /* 32-bit BA Bitmap Support */
};

struct HE_A_CTRL_OM_T {
	u_int8_t ucRxNss;
	u_int8_t ucTxNsts;
	u_int8_t ucBW;
	u_int8_t fgDisMuUL;
	u_int8_t fgDisMuULData;
};


/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

u_int32_t heRlmCalculateHeCapIELen(
	struct ADAPTER *prAdapter,
	u_int8_t ucBssIndex,
	struct STA_RECORD *prStaRec);
u_int32_t heRlmCalculateHeOpIELen(
	struct ADAPTER *prAdapter,
	u_int8_t ucBssIndex,
	struct STA_RECORD *prStaRec);
void heRlmReqGenerateHeCapIE(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);
void heRlmRspGenerateHeCapIE(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);
void heRlmRspGenerateHeOpIE(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);
void heRlmRecHeCapInfo(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	u_int8_t *pucIE);
void heRlmRecHeOperation(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	u_int8_t *pucIE);
u_int8_t heRlmRecHeSRParams(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct SW_RFB *prSwRfb,
	u_int8_t *pucIE,
	u_int16_t u2IELength);
void heRlmInitHeHtcACtrlOMAndUPH(
	struct ADAPTER *prAdapter);
void heRlmParseHeHtcACtrlOM(
	uint32_t u4Htc,
	struct HE_A_CTRL_OM_T *prHeActrlOM);
uint32_t heRlmSendHtcNullFrame(
	IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t ucUP,
	IN PFN_TX_DONE_HANDLER pfTxDoneHandler);
#endif /* CFG_SUPPORT_802_11AX == 1 */
#endif /* !_HE_RLM_H */
