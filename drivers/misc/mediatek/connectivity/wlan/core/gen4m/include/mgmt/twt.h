/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
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
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
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
#ifndef _TWT_H
#define _TWT_H

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
#define TWT_MAX_FLOW_NUM        8
#define TWT_MAX_WAKE_INTVAL_EXP (TWT_REQ_TYPE_TWT_WAKE_INTVAL_EXP >> \
	TWT_REQ_TYPE_TWT_WAKE_INTVAL_EXP_OFFSET)

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

/* Macros for setting request type bit fields in TWT IE */
#define SET_TWT_RT_REQUEST(fgReq) \
	(((fgReq) << TWT_REQ_TYPE_TWT_REQUEST_OFFSET) & \
		TWT_REQ_TYPE_TWT_REQUEST)

#define SET_TWT_RT_SETUP_CMD(ucSetupCmd) \
	(((ucSetupCmd) << TWT_REQ_TYPE_TWT_SETUP_COMMAND_OFFSET) & \
		TWT_REQ_TYPE_TWT_SETUP_COMMAND)

#define SET_TWT_RT_TRIGGER(fgTrigger) \
	(((fgTrigger) << TWT_REQ_TYPE_TRIGGER_OFFSET) & TWT_REQ_TYPE_TRIGGER)

#define SET_TWT_RT_FLOW_TYPE(fgUnannounced) \
	(((fgUnannounced) << TWT_REQ_TYPE_FLOWTYPE_OFFSET) & \
		TWT_REQ_TYPE_FLOWTYPE)

#define SET_TWT_RT_FLOW_ID(ucTWTFlowId) \
	(((ucTWTFlowId) << TWT_REQ_TYPE_TWT_FLOW_IDENTIFIER_OFFSET) & \
		TWT_REQ_TYPE_TWT_FLOW_IDENTIFIER)

#define SET_TWT_RT_WAKE_INTVAL_EXP(ucWakeIntvlExponent) \
	(((ucWakeIntvlExponent) << TWT_REQ_TYPE_TWT_WAKE_INTVAL_EXP_OFFSET) & \
		TWT_REQ_TYPE_TWT_WAKE_INTVAL_EXP)

#define SET_TWT_RT_PROTECTION(fgProtect) \
	(((fgProtect) << TWT_REQ_TYPE_TWT_PROTECTION_OFFSET) & \
		TWT_REQ_TYPE_TWT_PROTECTION)

/* Macros for getting request type bit fields in TWT IE */
#define GET_TWT_RT_REQUEST(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_TWT_REQUEST) >> \
		TWT_REQ_TYPE_TWT_REQUEST_OFFSET)

#define GET_TWT_RT_SETUP_CMD(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_TWT_SETUP_COMMAND) >> \
		TWT_REQ_TYPE_TWT_SETUP_COMMAND_OFFSET)

#define GET_TWT_RT_TRIGGER(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_TRIGGER) >> TWT_REQ_TYPE_TRIGGER_OFFSET)

#define GET_TWT_RT_FLOW_TYPE(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_FLOWTYPE) >> TWT_REQ_TYPE_FLOWTYPE_OFFSET)

#define GET_TWT_RT_FLOW_ID(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_TWT_FLOW_IDENTIFIER) >> \
		TWT_REQ_TYPE_TWT_FLOW_IDENTIFIER_OFFSET)

#define GET_TWT_RT_WAKE_INTVAL_EXP(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_TWT_WAKE_INTVAL_EXP) >> \
		TWT_REQ_TYPE_TWT_WAKE_INTVAL_EXP_OFFSET)

#define GET_TWT_RT_PROTECTION(u2ReqType) \
	(((u2ReqType) & TWT_REQ_TYPE_TWT_PROTECTION) >> \
		TWT_REQ_TYPE_TWT_PROTECTION_OFFSET)

/* Macros to set TWT info field */
#define SET_TWT_INFO_FLOW_ID(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) << TWT_INFO_FLOW_ID_OFFSET) & TWT_INFO_FLOW_ID)

#define SET_TWT_INFO_RESP_REQUESTED(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) << TWT_INFO_RESP_REQUESTED_OFFSET) & \
	TWT_INFO_RESP_REQUESTED)

#define SET_TWT_INFO_NEXT_TWT_REQ(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) << TWT_INFO_NEXT_TWT_REQ_OFFSET) & \
	TWT_INFO_NEXT_TWT_REQ)

#define SET_TWT_INFO_NEXT_TWT_SIZE(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) << TWT_INFO_NEXT_TWT_SIZE_OFFSET) & \
	TWT_INFO_NEXT_TWT_SIZE)

#define SET_TWT_INFO_BCAST_RESCHED(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) << TWT_INFO_BCAST_RESCHED_OFFSET) & \
	TWT_INFO_BCAST_RESCHED)

/* Macros to get TWT info field */
#define GET_TWT_INFO_FLOW_ID(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) & TWT_INFO_FLOW_ID) >> TWT_INFO_FLOW_ID_OFFSET)

#define GET_TWT_INFO_RESP_REQUESTED(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) & TWT_INFO_RESP_REQUESTED) >> \
	TWT_INFO_RESP_REQUESTED_OFFSET)

#define GET_TWT_INFO_NEXT_TWT_REQ(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) & TWT_INFO_NEXT_TWT_REQ) >> \
	TWT_INFO_NEXT_TWT_REQ_OFFSET)

#define GET_TWT_INFO_NEXT_TWT_SIZE(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) & TWT_INFO_NEXT_TWT_SIZE) >> \
	TWT_INFO_NEXT_TWT_SIZE_OFFSET)

#define GET_TWT_INFO_BCAST_RESCHED(ucNextTWTCtrl) \
	(((ucNextTWTCtrl) & TWT_INFO_BCAST_RESCHED) >> \
	TWT_INFO_BCAST_RESCHED_OFFSET)

/* Next TWT from the packet should be little endian */
#define GET_48_BITS_NEXT_TWT_FROM_PKT(pMem) \
	((u_int64_t)(*((u_int8_t *)(pMem))) | \
	((u_int64_t)(*(((u_int8_t *)(pMem)) + 1)) >> 8) | \
	((u_int64_t)(*(((u_int8_t *)(pMem)) + 2)) >> 16) | \
	((u_int64_t)(*(((u_int8_t *)(pMem)) + 3)) >> 24) | \
	((u_int64_t)(*(((u_int8_t *)(pMem)) + 4)) >> 32) | \
	((u_int64_t)(*(((u_int8_t *)(pMem)) + 5)) >> 40))

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

void twtProcessS1GAction(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);

uint32_t twtSendSetupFrame(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	u_int8_t ucTWTFlowId,
	struct _TWT_PARAMS_T *prTWTParams,
	PFN_TX_DONE_HANDLER pfTxDoneHandler);

uint32_t twtSendTeardownFrame(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	u_int8_t ucTWTFlowId,
	PFN_TX_DONE_HANDLER pfTxDoneHandler);

uint32_t twtSendInfoFrame(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	u_int8_t ucTWTFlowId,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo,
	PFN_TX_DONE_HANDLER pfTxDoneHandler);

u_int8_t twtGetTxSetupFlowId(
	struct MSDU_INFO *prMsduInfo);

u_int8_t twtGetTxTeardownFlowId(
	struct MSDU_INFO *prMsduInfo);

uint8_t twtGetTxInfoFlowId(
	struct MSDU_INFO *prMsduInfo);

static inline u_int8_t twtGetNextTWTByteCnt(u_int8_t ucNextTWTSize)
{
	return (ucNextTWTSize == NEXT_TWT_SUBFIELD_64_BITS) ? 8 :
		((ucNextTWTSize == NEXT_TWT_SUBFIELD_32_BITS) ? 4 :
		((ucNextTWTSize == NEXT_TWT_SUBFIELD_48_BITS) ? 6 : 0));
}
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _TWT_H */
