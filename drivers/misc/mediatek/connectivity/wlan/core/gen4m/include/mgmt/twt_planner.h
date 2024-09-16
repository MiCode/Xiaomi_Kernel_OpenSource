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
#ifndef _TWT_PLANNER_H
#define _TWT_PLANNER_H

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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct _TWT_FLOW_T {
	struct _TWT_PARAMS_T rTWTParams;
	struct _TWT_PARAMS_T rTWTPeerParams;
	u_int64_t u8NextTWT;
};

struct _TWT_AGRT_T {
	u_int8_t fgValid;
	u_int8_t ucAgrtTblIdx;
	u_int8_t ucBssIdx;
	u_int8_t ucFlowId;
	struct _TWT_PARAMS_T rTWTAgrt;
};

struct _TWT_PLANNER_T {
	struct _TWT_AGRT_T arTWTAgrtTbl[TWT_AGRT_MAX_NUM];
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

#define TSF_OFFSET_FOR_EMU	   (1 * 1000 * 1000)	/* after 1 sec */
#define TSF_OFFSET_FOR_AGRT_ADD	   (5 * 1000 * 1000)	/* after 5 sec */
#define TSF_OFFSET_FOR_AGRT_RESUME (5 * 1000 * 1000)	/* after 5 sec */

/* Definitions for action control of TWT params */
enum {
	TWT_PARAM_ACTION_NONE = 0,
	TWT_PARAM_ACTION_ADD_BYPASS = 1, /* bypass nego & add an agrt */
	TWT_PARAM_ACTION_DEL_BYPASS = 2, /* bypass proto & del an agrt */
	TWT_PARAM_ACTION_MOD_BYPASS = 3, /* bypass proto & modify an agrt */
	TWT_PARAM_ACTION_ADD = 4,
	TWT_PARAM_ACTION_DEL = 5,
	TWT_PARAM_ACTION_SUSPEND = 6,
	TWT_PARAM_ACTION_RESUME = 7,
	TWT_PARAM_ACTION_MAX
};

#define IS_TWT_PARAM_ACTION_ADD_BYPASS(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_ADD_BYPASS)
#define IS_TWT_PARAM_ACTION_DEL_BYPASS(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_DEL_BYPASS)
#define IS_TWT_PARAM_ACTION_MOD_BYPASS(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_MOD_BYPASS)
#define IS_TWT_PARAM_ACTION_ADD(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_ADD)
#define IS_TWT_PARAM_ACTION_DEL(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_DEL)
#define IS_TWT_PARAM_ACTION_SUSPEND(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_SUSPEND)
#define IS_TWT_PARAM_ACTION_RESUME(ucCtrlAction) \
	((ucCtrlAction) == TWT_PARAM_ACTION_RESUME)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
void twtPlannerSetParams(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

uint32_t twtPlannerReset(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo);

void twtPlannerRxNegoResult(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

void twtPlannerSuspendDone(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

void twtPlannerTeardownDone(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

void twtPlannerResumeDone(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

void twtPlannerRxInfoFrm(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _TWT_PLANNER_H */
