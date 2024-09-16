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
/*! \file   "twt_planner.c"
*   \brief  TWT Planner to determine TWT negotiation policy
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

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

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static uint32_t
_twtPlannerDrvAgrtAdd(struct _TWT_PLANNER_T *prTWTPlanner,
	uint8_t ucBssIdx, uint8_t ucFlowId,
	struct _TWT_PARAMS_T *prTWTParams, uint8_t ucIdx)
{
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[ucIdx]);

	prTWTAgrt->fgValid = TRUE;
	prTWTAgrt->ucBssIdx = ucBssIdx;
	prTWTAgrt->ucFlowId = ucFlowId;
	prTWTAgrt->ucAgrtTblIdx = ucIdx;
	kalMemCopy(&(prTWTAgrt->rTWTAgrt), prTWTParams,
		sizeof(struct _TWT_PARAMS_T));

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
_twtPlannerDrvAgrtDel(
	struct _TWT_PLANNER_T *prTWTPlanner, uint8_t ucIdx)
{
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[ucIdx]);

	kalMemSet(prTWTAgrt, 0, sizeof(struct _TWT_AGRT_T));

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
_twtPlannerDrvAgrtModify(
	struct _TWT_PLANNER_T *prTWTPlanner,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo,
	uint64_t u8CurTsf, uint8_t ucIdx,
	struct _TWT_PARAMS_T *prTWTParams)
{
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[ucIdx]);

	if (!prNextTWTInfo || !prTWTParams)
		return WLAN_STATUS_FAILURE;

	if (prNextTWTInfo->ucNextTWTSize == NEXT_TWT_SUBFIELD_64_BITS) {
		prTWTAgrt->rTWTAgrt.u8TWT = prNextTWTInfo->u8NextTWT;
	} else if (prNextTWTInfo->ucNextTWTSize == NEXT_TWT_SUBFIELD_32_BITS) {
		prTWTAgrt->rTWTAgrt.u8TWT =
			((u8CurTsf & ((uint64_t)0xFFFFFFFF << 32)) |
			prNextTWTInfo->u8NextTWT);
	} else if (prNextTWTInfo->ucNextTWTSize == NEXT_TWT_SUBFIELD_48_BITS) {
		prTWTAgrt->rTWTAgrt.u8TWT =
			((u8CurTsf & ((uint64_t)0xFFFF << 48)) |
			prNextTWTInfo->u8NextTWT);
	} else {
		/* Zero bit Next TWT is not acceptable */
		return WLAN_STATUS_FAILURE;
	}

	kalMemCopy(prTWTParams, &(prTWTAgrt->rTWTAgrt),
		sizeof(struct _TWT_PARAMS_T));

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
_twtPlannerDrvAgrtGet(
	struct _TWT_PLANNER_T *prTWTPlanner,
	uint8_t ucIdx, struct _TWT_PARAMS_T *prTWTParams)
{
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[ucIdx]);

	if (!prTWTParams)
		return WLAN_STATUS_FAILURE;

	kalMemCopy(prTWTParams, &(prTWTAgrt->rTWTAgrt),
		sizeof(struct _TWT_PARAMS_T));

	return WLAN_STATUS_SUCCESS;
}

static uint8_t
twtPlannerDrvAgrtFind(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
	uint8_t ucFlowId)
{
	uint8_t i;
	struct _TWT_PLANNER_T *prTWTPlanner = &(prAdapter->rTWTPlanner);
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[0]);

	for (i = 0; i < TWT_AGRT_MAX_NUM; i++, prTWTAgrt++) {
		if (prTWTAgrt->fgValid == TRUE &&
			prTWTAgrt->ucFlowId == ucFlowId &&
			prTWTAgrt->ucBssIdx == ucBssIdx)
			break;
	}

	return i;
}

uint32_t
twtPlannerDrvAgrtAdd(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, uint8_t ucFlowId,
	struct _TWT_PARAMS_T *prTWTParams, uint8_t *pucIdx)
{
	uint8_t ucIdx;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct _TWT_PLANNER_T *prTWTPlanner = &(prAdapter->rTWTPlanner);
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[0]);

	for (ucIdx = 0; ucIdx < TWT_AGRT_MAX_NUM; ucIdx++, prTWTAgrt++) {
		if (prTWTAgrt->fgValid == FALSE)
			break;
	}

	if (ucIdx < TWT_AGRT_MAX_NUM) {
		_twtPlannerDrvAgrtAdd(prTWTPlanner, ucBssIdx,
			ucFlowId, prTWTParams, ucIdx);
		*pucIdx = ucIdx;
		rStatus = WLAN_STATUS_SUCCESS;
	}

	return rStatus;
}

uint32_t
twtPlannerDrvAgrtModify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, uint8_t ucFlowId,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo,
	uint8_t *pucIdx, struct _TWT_PARAMS_T *prTWTParams)
{
	uint8_t ucIdx;
	uint64_t u8CurTsf;
	struct _TWT_PLANNER_T *prTWTPlanner = &(prAdapter->rTWTPlanner);
	uint32_t rStatus;

	ucIdx = twtPlannerDrvAgrtFind(prAdapter, ucBssIdx, ucFlowId);
	if (ucIdx >= TWT_AGRT_MAX_NUM) {
		DBGLOG(TWT_PLANNER, ERROR, "Can't find agrt bss %u flow %u\n",
			ucBssIdx, ucFlowId);
		return WLAN_STATUS_FAILURE;
	}

	/* TODO: get current TSF from FW */
	u8CurTsf = 0;

	rStatus = _twtPlannerDrvAgrtModify(prTWTPlanner, prNextTWTInfo,
		u8CurTsf, ucIdx, prTWTParams);
	if (rStatus == WLAN_STATUS_SUCCESS)
		*pucIdx = ucIdx;

	return rStatus;
}

uint32_t
twtPlannerDrvAgrtGet(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, uint8_t ucFlowId,
	uint8_t *pucIdx, struct _TWT_PARAMS_T *prTWTParams)
{
	uint8_t ucIdx;
	struct _TWT_PLANNER_T *prTWTPlanner = &(prAdapter->rTWTPlanner);
	uint32_t rStatus;

	ucIdx = twtPlannerDrvAgrtFind(prAdapter, ucBssIdx, ucFlowId);
	if (ucIdx >= TWT_AGRT_MAX_NUM) {
		DBGLOG(TWT_PLANNER, ERROR, "Can't find agrt bss %u flow %u\n",
			ucBssIdx, ucFlowId);
		return WLAN_STATUS_FAILURE;
	}

	rStatus = _twtPlannerDrvAgrtGet(prTWTPlanner, ucIdx, prTWTParams);
	if (rStatus == WLAN_STATUS_SUCCESS)
		*pucIdx = ucIdx;

	return rStatus;
}

bool
twtPlannerIsDrvAgrtExisting(struct ADAPTER *prAdapter)
{
	bool ret = FALSE;
	uint8_t i;
	struct _TWT_PLANNER_T *prTWTPlanner = &(prAdapter->rTWTPlanner);
	struct _TWT_AGRT_T *prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[0]);

	for (i = 0; i < TWT_AGRT_MAX_NUM; i++, prTWTAgrt++) {
		if (prTWTAgrt->fgValid == TRUE) {
			ret = TRUE;
			break;
		}
	}

	return ret;
}

void twtPlannerInit(IN struct _TWT_PLANNER_T *pTWTPlanner)
{
	ASSERT(pTWTPlanner);

	kalMemSet(&(pTWTPlanner->arTWTAgrtTbl[0]), 0,
		TWT_AGRT_MAX_NUM * sizeof(struct _TWT_AGRT_T));
}

static struct _TWT_FLOW_T *twtPlannerFlowFindById(
	struct STA_RECORD *prStaRec, uint8_t ucFlowId)
{
	struct _TWT_FLOW_T *prTWTFlow = NULL;

	ASSERT(prStaRec);

	if (ucFlowId >= TWT_MAX_FLOW_NUM) {
		DBGLOG(TWT_PLANNER, ERROR, "Invalid TWT flow id %u\n",
			ucFlowId);
		return NULL;
	}

	prTWTFlow = &(prStaRec->arTWTFlow[ucFlowId]);

	return prTWTFlow;
}

static uint32_t
twtPlannerSendReqStart(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId)
{
	struct _MSG_TWT_REQFSM_START_T *prTWTReqFsmStartMsg;

	prTWTReqFsmStartMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_START_T));
	if (prTWTReqFsmStartMsg) {
		prTWTReqFsmStartMsg->rMsgHdr.eMsgId = MID_TWT_REQ_FSM_START;
		prTWTReqFsmStartMsg->prStaRec = prStaRec;
		prTWTReqFsmStartMsg->ucTWTFlowId = ucTWTFlowId;

		mboxSendMsg(prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prTWTReqFsmStartMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return WLAN_STATUS_RESOURCES;

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
twtPlannerSendReqTeardown(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				uint8_t ucTWTFlowId)
{
	struct _MSG_TWT_REQFSM_TEARDOWN_T *prTWTReqFsmTeardownMsg;

	prTWTReqFsmTeardownMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_TEARDOWN_T));
	if (prTWTReqFsmTeardownMsg) {
		prTWTReqFsmTeardownMsg->rMsgHdr.eMsgId =
			MID_TWT_REQ_FSM_TEARDOWN;
		prTWTReqFsmTeardownMsg->prStaRec = prStaRec;
		prTWTReqFsmTeardownMsg->ucTWTFlowId = ucTWTFlowId;

		mboxSendMsg(prAdapter, MBOX_ID_0,
			(struct MSG_HDR *) prTWTReqFsmTeardownMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return WLAN_STATUS_RESOURCES;

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
twtPlannerSendReqSuspend(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				uint8_t ucTWTFlowId)
{
	struct _MSG_TWT_REQFSM_SUSPEND_T *prTWTReqFsmSuspendMsg;

	prTWTReqFsmSuspendMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_SUSPEND_T));
	if (prTWTReqFsmSuspendMsg) {
		prTWTReqFsmSuspendMsg->rMsgHdr.eMsgId =
			MID_TWT_REQ_FSM_SUSPEND;
		prTWTReqFsmSuspendMsg->prStaRec = prStaRec;
		prTWTReqFsmSuspendMsg->ucTWTFlowId = ucTWTFlowId;

		mboxSendMsg(prAdapter, MBOX_ID_0,
			(struct MSG_HDR *) prTWTReqFsmSuspendMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return WLAN_STATUS_RESOURCES;

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
twtPlannerSendReqResume(struct ADAPTER *prAdapter,
				struct STA_RECORD *prStaRec,
				uint8_t ucTWTFlowId,
				uint64_t u8NextTWT,
				uint8_t ucNextTWTSize)
{
	struct _MSG_TWT_REQFSM_RESUME_T *prTWTReqFsmResumeMsg;

	prTWTReqFsmResumeMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_RESUME_T));
	if (prTWTReqFsmResumeMsg) {
		prTWTReqFsmResumeMsg->rMsgHdr.eMsgId =
			MID_TWT_REQ_FSM_RESUME;
		prTWTReqFsmResumeMsg->prStaRec = prStaRec;
		prTWTReqFsmResumeMsg->ucTWTFlowId = ucTWTFlowId;
		prTWTReqFsmResumeMsg->u8NextTWT = u8NextTWT;
		prTWTReqFsmResumeMsg->ucNextTWTSize = ucNextTWTSize;

		mboxSendMsg(prAdapter, MBOX_ID_0,
			(struct MSG_HDR *) prTWTReqFsmResumeMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return WLAN_STATUS_RESOURCES;

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
twtPlannerAddAgrtTbl(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct STA_RECORD *prStaRec,
	struct _TWT_PARAMS_T *prTWTParams,
	uint8_t ucFlowId,
	uint8_t fgIsOid,
	PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
	PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler)
{
	uint8_t ucAgrtTblIdx;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _EXT_CMD_TWT_ARGT_UPDATE_T *prTWTAgrtUpdate;

	if (prBssInfo == NULL) {
		DBGLOG(TWT_PLANNER, ERROR, "No bssinfo to add agrt\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	rWlanStatus = twtPlannerDrvAgrtAdd(prAdapter, prBssInfo->ucBssIndex,
		ucFlowId, prTWTParams, &ucAgrtTblIdx);
	if (rWlanStatus) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Agreement table is full\n");
		return WLAN_STATUS_FAILURE;
	}

	prTWTAgrtUpdate = cnmMemAlloc(
		prAdapter,
		RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T));
	if (!prTWTAgrtUpdate) {
		DBGLOG(
		TWT_PLANNER,
		ERROR,
		"Allocate _EXT_CMD_TWT_ARGT_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prTWTAgrtUpdate->ucAgrtTblIdx = ucAgrtTblIdx;
	prTWTAgrtUpdate->ucAgrtCtrlFlag = TWT_AGRT_CTRL_ADD;
	prTWTAgrtUpdate->ucOwnMacId =
		(prBssInfo) ? prBssInfo->ucOwnMacIndex : 0;
	prTWTAgrtUpdate->ucFlowId = ucFlowId;
	prTWTAgrtUpdate->u2PeerIdGrpId =
		(prStaRec) ? CPU_TO_LE16(prStaRec->ucWlanIndex) : 1;
	prTWTAgrtUpdate->ucAgrtSpDuration = prTWTParams->ucMinWakeDur;
	prTWTAgrtUpdate->ucBssIndex = prBssInfo ? prBssInfo->ucBssIndex : 0;
	prTWTAgrtUpdate->u4AgrtSpStartTsfLow =
		CPU_TO_LE32(prTWTParams->u8TWT & 0xFFFFFFFF);
	prTWTAgrtUpdate->u4AgrtSpStartTsfHigh =
		CPU_TO_LE32((uint32_t)(prTWTParams->u8TWT >> 32));
	prTWTAgrtUpdate->u2AgrtSpWakeIntvlMantissa =
		CPU_TO_LE16(prTWTParams->u2WakeIntvalMantiss);
	prTWTAgrtUpdate->ucAgrtSpWakeIntvlExponent =
		prTWTParams->ucWakeIntvalExponent;
	prTWTAgrtUpdate->ucIsRoleAp = 0;  /* STA role */

	prTWTAgrtUpdate->ucAgrtParaBitmap =
	((prTWTParams->fgProtect << TWT_AGRT_PARA_BITMAP_PROTECT_OFFSET) |
	((!prTWTParams->fgUnannounced) << TWT_AGRT_PARA_BITMAP_ANNCE_OFFSET) |
	(prTWTParams->fgTrigger << TWT_AGRT_PARA_BITMAP_TRIGGER_OFFSET));

	prTWTAgrtUpdate->ucGrpMemberCnt = 0;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
				CMD_ID_LAYER_0_EXT_MAGIC_NUM,
				EXT_CMD_ID_TWT_AGRT_UPDATE,
				TRUE,
				TRUE,
				fgIsOid,
				pfCmdDoneHandler,
				pfCmdTimeoutHandler,
				sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T),
				(uint8_t *) (prTWTAgrtUpdate),
				NULL, 0);

	return rWlanStatus;
}

static uint32_t
twtPlannerResumeAgrtTbl(struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
			uint8_t ucFlowId, uint8_t fgIsOid,
			PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler)
{
	uint8_t ucAgrtTblIdx;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _EXT_CMD_TWT_ARGT_UPDATE_T *prTWTAgrtUpdate;
	struct _TWT_PARAMS_T rTWTParams;

	if (prBssInfo == NULL) {
		DBGLOG(TWT_PLANNER, ERROR, "No bssinfo to resume agrt\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	rWlanStatus = twtPlannerDrvAgrtGet(prAdapter, prBssInfo->ucBssIndex,
		ucFlowId, &ucAgrtTblIdx, &rTWTParams);
	if (rWlanStatus) {
		DBGLOG(TWT_PLANNER, ERROR, "No agrt to resume Bss %u flow %u\n",
			prBssInfo->ucBssIndex, ucFlowId);
		return WLAN_STATUS_FAILURE;
	}

	prTWTAgrtUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T));
	if (!prTWTAgrtUpdate) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Allocate _EXT_CMD_TWT_ARGT_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prTWTAgrtUpdate->ucAgrtTblIdx = ucAgrtTblIdx;
	prTWTAgrtUpdate->ucAgrtCtrlFlag = TWT_AGRT_CTRL_ADD;
	prTWTAgrtUpdate->ucOwnMacId = (prBssInfo) ?
		prBssInfo->ucOwnMacIndex : 0;
	prTWTAgrtUpdate->ucFlowId = ucFlowId;
	prTWTAgrtUpdate->u2PeerIdGrpId = (prStaRec) ?
		CPU_TO_LE16(prStaRec->ucWlanIndex) : 1;
	prTWTAgrtUpdate->ucAgrtSpDuration = rTWTParams.ucMinWakeDur;
	prTWTAgrtUpdate->ucBssIndex = prBssInfo->ucBssIndex;
	prTWTAgrtUpdate->u4AgrtSpStartTsfLow =
		CPU_TO_LE32(rTWTParams.u8TWT & 0xFFFFFFFF);
	prTWTAgrtUpdate->u4AgrtSpStartTsfHigh =
		CPU_TO_LE32((uint32_t)(rTWTParams.u8TWT >> 32));
	prTWTAgrtUpdate->u2AgrtSpWakeIntvlMantissa =
		CPU_TO_LE16(rTWTParams.u2WakeIntvalMantiss);
	prTWTAgrtUpdate->ucAgrtSpWakeIntvlExponent =
		rTWTParams.ucWakeIntvalExponent;
	prTWTAgrtUpdate->ucIsRoleAp = 0;  /* STA role */
	prTWTAgrtUpdate->ucAgrtParaBitmap =
	    ((rTWTParams.fgProtect << TWT_AGRT_PARA_BITMAP_PROTECT_OFFSET) |
	    ((!rTWTParams.fgUnannounced) << TWT_AGRT_PARA_BITMAP_ANNCE_OFFSET) |
	    (rTWTParams.fgTrigger << TWT_AGRT_PARA_BITMAP_TRIGGER_OFFSET));
	prTWTAgrtUpdate->ucGrpMemberCnt = 0;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_TWT_AGRT_UPDATE,
			TRUE,
			TRUE,
			fgIsOid,
			pfCmdDoneHandler,
			pfCmdTimeoutHandler,
			sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T),
			(uint8_t *) (prTWTAgrtUpdate),
			NULL, 0);

	return rWlanStatus;
}

static uint32_t
twtPlannerModifyAgrtTbl(struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
			struct _NEXT_TWT_INFO_T *prNextTWTInfo,
			uint8_t ucFlowId, uint8_t fgIsOid,
			PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler)
{
	uint8_t ucAgrtTblIdx;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _EXT_CMD_TWT_ARGT_UPDATE_T *prTWTAgrtUpdate;
	struct _TWT_PARAMS_T rTWTParams;

	if (prBssInfo == NULL) {
		DBGLOG(TWT_PLANNER, ERROR, "No bssinfo to modify agrt\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Handle driver agreement table */
	rWlanStatus = twtPlannerDrvAgrtModify(prAdapter, prBssInfo->ucBssIndex,
		ucFlowId, prNextTWTInfo, &ucAgrtTblIdx, &rTWTParams);

	if (rWlanStatus) {
		DBGLOG(TWT_PLANNER, ERROR, "No agrt to modify Bss %u flow %u\n",
			prBssInfo->ucBssIndex, ucFlowId);
		return WLAN_STATUS_FAILURE;
	}

	/* Handle FW agreement table */
	prTWTAgrtUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T));
	if (!prTWTAgrtUpdate) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Allocate _EXT_CMD_TWT_ARGT_UPDATE_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prTWTAgrtUpdate->ucAgrtTblIdx = ucAgrtTblIdx;
	prTWTAgrtUpdate->ucAgrtCtrlFlag = TWT_AGRT_CTRL_MODIFY;
	prTWTAgrtUpdate->ucOwnMacId = (prBssInfo) ?
		prBssInfo->ucOwnMacIndex : 0;
	prTWTAgrtUpdate->ucFlowId = ucFlowId;
	prTWTAgrtUpdate->u2PeerIdGrpId = (prStaRec) ?
		CPU_TO_LE16(prStaRec->ucWlanIndex) : 1;
	prTWTAgrtUpdate->ucAgrtSpDuration = rTWTParams.ucMinWakeDur;
	prTWTAgrtUpdate->u4AgrtSpStartTsfLow =
		CPU_TO_LE32(rTWTParams.u8TWT & 0xFFFFFFFF);
	prTWTAgrtUpdate->u4AgrtSpStartTsfHigh =
		CPU_TO_LE32((uint32_t)(rTWTParams.u8TWT >> 32));
	prTWTAgrtUpdate->ucGrpMemberCnt = 0;
	prTWTAgrtUpdate->ucBssIndex = prBssInfo->ucBssIndex;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_TWT_AGRT_UPDATE,
			TRUE,
			TRUE,
			fgIsOid,
			pfCmdDoneHandler,
			pfCmdTimeoutHandler,
			sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T),
			(uint8_t *) (prTWTAgrtUpdate),
			NULL, 0);

	return rWlanStatus;
}

static uint32_t
twtPlannerDelAgrtTbl(struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo, struct STA_RECORD *prStaRec,
			uint8_t ucFlowId, uint8_t fgIsOid,
			PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
			uint8_t fgDelDrvEntry)
{
	uint8_t ucAgrtTblIdx;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _TWT_PLANNER_T *prTWTPlanner = &(prAdapter->rTWTPlanner);
	struct _EXT_CMD_TWT_ARGT_UPDATE_T *prTWTAgrtUpdate;

	if (prBssInfo == NULL) {
		DBGLOG(TWT_PLANNER, ERROR, "No bssinfo to delete agrt\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Find and delete the agreement entry in the driver */
	ucAgrtTblIdx = twtPlannerDrvAgrtFind(prAdapter,
		prBssInfo->ucBssIndex, ucFlowId);

	if (ucAgrtTblIdx >= TWT_AGRT_MAX_NUM) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Cannot find the flow %u to be deleted\n", ucFlowId);
		return WLAN_STATUS_FAILURE;

	}

	if (fgDelDrvEntry)
		_twtPlannerDrvAgrtDel(prTWTPlanner, ucAgrtTblIdx);

	/* Send cmd to delete agreement entry in FW */
	prTWTAgrtUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T));
	if (!prTWTAgrtUpdate) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Alloc _EXT_CMD_TWT_ARGT_UPDATE_T for del FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prTWTAgrtUpdate->ucAgrtTblIdx = ucAgrtTblIdx;
	prTWTAgrtUpdate->ucAgrtCtrlFlag = TWT_AGRT_CTRL_DELETE;
	prTWTAgrtUpdate->ucOwnMacId = (prBssInfo) ?
		prBssInfo->ucOwnMacIndex : 0;
	prTWTAgrtUpdate->ucFlowId = ucFlowId;
	prTWTAgrtUpdate->u2PeerIdGrpId = (prStaRec) ?
		CPU_TO_LE16(prStaRec->ucWlanIndex) : 1;
	prTWTAgrtUpdate->ucIsRoleAp = 0;  /* STA role */
	prTWTAgrtUpdate->ucBssIndex = prBssInfo ? prBssInfo->ucBssIndex : 0;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_TWT_AGRT_UPDATE,
			TRUE,
			TRUE,
			fgIsOid,
			pfCmdDoneHandler,
			pfCmdTimeoutHandler,
			sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T),
			(uint8_t *) (prTWTAgrtUpdate),
			NULL, 0);

	return rWlanStatus;
}

static uint32_t
twtPlannerTeardownAgrtTbl(struct ADAPTER *prAdapter,
			struct STA_RECORD *prStaRec,
			uint8_t fgIsOid,
			PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
			PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _EXT_CMD_TWT_ARGT_UPDATE_T *prTWTAgrtUpdate;

	/* Send cmd to teardown this STA in FW */
	prTWTAgrtUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T));
	if (!prTWTAgrtUpdate) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Alloc _EXT_CMD_TWT_ARGT_UPDATE_T for del FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* Don't care about other fields of the cmd */
	prTWTAgrtUpdate->ucAgrtCtrlFlag = TWT_AGRT_CTRL_TEARDOWN;
	prTWTAgrtUpdate->u2PeerIdGrpId = (prStaRec) ?
		CPU_TO_LE16(prStaRec->ucWlanIndex) : 1;
	prTWTAgrtUpdate->ucIsRoleAp = 0;  /* STA role */
	prTWTAgrtUpdate->ucBssIndex = prStaRec ? prStaRec->ucBssIndex : 0;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_TWT_AGRT_UPDATE,
			TRUE,
			TRUE,
			fgIsOid,
			pfCmdDoneHandler,
			pfCmdTimeoutHandler,
			sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T),
			(uint8_t *) (prTWTAgrtUpdate),
			NULL, 0);

	return rWlanStatus;
}

uint32_t twtPlannerReset(
	struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct STA_RECORD *prStaRec;
	struct _EXT_CMD_TWT_ARGT_UPDATE_T *prTWTAgrtUpdate;

	/* If no agrt exits, don't bother resetting */
	if (twtPlannerIsDrvAgrtExisting(prAdapter) == FALSE)
		return rWlanStatus;

	prTWTAgrtUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T));
	if (!prTWTAgrtUpdate) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Alloc _EXT_CMD_TWT_ARGT_UPDATE_T for reset FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* send cmd to reset FW agreement table */
	ASSERT(prBssInfo);
	prStaRec = prBssInfo->prStaRecOfAP;

	prTWTAgrtUpdate->ucAgrtCtrlFlag = TWT_AGRT_CTRL_RESET;
	prTWTAgrtUpdate->u2PeerIdGrpId = (prStaRec) ?
		CPU_TO_LE16(prStaRec->ucWlanIndex) : 1;
	prTWTAgrtUpdate->ucIsRoleAp = 0;  /* STA role */
	prTWTAgrtUpdate->ucBssIndex = prBssInfo ? prBssInfo->ucBssIndex : 0;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
			CMD_ID_LAYER_0_EXT_MAGIC_NUM,
			EXT_CMD_ID_TWT_AGRT_UPDATE,
			TRUE,
			TRUE,
			FALSE,
			NULL,
			NULL,
			sizeof(struct _EXT_CMD_TWT_ARGT_UPDATE_T),
			(uint8_t *) (prTWTAgrtUpdate),
			NULL, 0);


	/* reset driver agreement table */
	memset(&(prAdapter->rTWTPlanner), 0, sizeof(prAdapter->rTWTPlanner));

	/* Enable scan after TWT agrt reset */
	prAdapter->fgEnOnlineScan = TRUE;

	return rWlanStatus;
}

uint64_t twtPlannerAdjustNextTWT(struct ADAPTER *prAdapter,
	uint8_t ucBssIdx, uint8_t ucFlowId,
	uint64_t u8NextTWTOrig)
{
	uint8_t ucAgrtTblIdx;
	struct _TWT_PARAMS_T rTWTParams = {0x0};
	uint64_t u8Diff;
	uint32_t u4WakeIntvl;

	twtPlannerDrvAgrtGet(prAdapter, ucBssIdx, ucFlowId,
		&ucAgrtTblIdx, &rTWTParams);

	u4WakeIntvl = rTWTParams.u2WakeIntvalMantiss <<
		rTWTParams.ucWakeIntvalExponent;
	u8Diff = u8NextTWTOrig - rTWTParams.u8TWT;
	/* TODO: move div_u64 to os-dependent file */
	return (rTWTParams.u8TWT +
		(div_u64(u8Diff, u4WakeIntvl) + 1) * u4WakeIntvl);
}

void twtPlannerGetTsfDone(
	struct ADAPTER *prAdapter,
	struct CMD_INFO *prCmdInfo,
	uint8_t *pucEventBuf)
{
	struct EXT_EVENT_MAC_INFO_T *prEventMacInfo;
	struct _TWT_GET_TSF_CONTEXT_T *prGetTsfCtxt;
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	struct TSF_RESULT_T *prTsfResult;
	uint64_t u8CurTsf;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	if (!pucEventBuf) {
		DBGLOG(TWT_PLANNER, ERROR, "pucEventBuf is NULL.\n");
		return;
	}
	if (!prCmdInfo->pvInformationBuffer) {
		DBGLOG(TWT_PLANNER, ERROR,
			"prCmdInfo->pvInformationBuffer is NULL.\n");
		return;
	}

	prEventMacInfo = (struct EXT_EVENT_MAC_INFO_T *) (pucEventBuf);
	prGetTsfCtxt = (struct _TWT_GET_TSF_CONTEXT_T *)
		prCmdInfo->pvInformationBuffer;

	ASSERT(prGetTsfCtxt);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prGetTsfCtxt->ucBssIdx);
	ASSERT(prBssInfo);
	prStaRec = prBssInfo->prStaRecOfAP;
	ASSERT(prStaRec);

	prTsfResult = &(prEventMacInfo->rMacInfoResult.rTsfResult);
	u8CurTsf = LE32_TO_CPU(prTsfResult->u4TsfBitsLow) |
		(((uint64_t)(LE32_TO_CPU(prTsfResult->u4TsfBitsHigh))) << 32);

	switch (prGetTsfCtxt->ucReason) {
	case TWT_GET_TSF_FOR_ADD_AGRT_BYPASS:
		prGetTsfCtxt->rTWTParams.u8TWT = u8CurTsf + TSF_OFFSET_FOR_EMU;
		twtPlannerAddAgrtTbl(prAdapter, prBssInfo,
				prStaRec, &(prGetTsfCtxt->rTWTParams),
				prGetTsfCtxt->ucTWTFlowId,
				prGetTsfCtxt->fgIsOid,
				NULL, NULL);
		break;

	case TWT_GET_TSF_FOR_ADD_AGRT:
	{
		struct _TWT_PARAMS_T *prTWTParams;
		struct _TWT_FLOW_T *prTWTFlow = twtPlannerFlowFindById(prStaRec,
					prGetTsfCtxt->ucTWTFlowId);

		if (prTWTFlow == NULL) {
			DBGLOG(TWT_PLANNER, ERROR, "prTWTFlow is NULL.\n");

			kalMemFree(prGetTsfCtxt,
				VIR_MEM_TYPE, sizeof(*prGetTsfCtxt));

			return;
		}

		prGetTsfCtxt->rTWTParams.u8TWT =
			u8CurTsf + TSF_OFFSET_FOR_AGRT_ADD;

		prTWTParams = &(prTWTFlow->rTWTParams);

		kalMemCopy(prTWTParams, &(prGetTsfCtxt->rTWTParams),
			sizeof(struct _TWT_PARAMS_T));

		/* Start the process to nego for a new agreement */
		twtPlannerSendReqStart(prAdapter,
			prStaRec, prGetTsfCtxt->ucTWTFlowId);

		break;
	}
	case TWT_GET_TSF_FOR_RESUME_AGRT:
	{
		uint8_t ucNextTWTSize = NEXT_TWT_SUBFIELD_64_BITS;
		uint64_t u8NextTWT = u8CurTsf + TSF_OFFSET_FOR_AGRT_RESUME;

		/* Adjust next TWT if 'Flexible TWT Sched' is not supported */
		if (!HE_IS_MAC_CAP_FLEXIBLE_TWT_SHDL(
			prStaRec->ucHeMacCapInfo)) {
			u8NextTWT = twtPlannerAdjustNextTWT(prAdapter,
					prBssInfo->ucBssIndex,
					prGetTsfCtxt->ucTWTFlowId,
					u8NextTWT);
		}

		/* Start the process to resume this TWT agreement */
		twtPlannerSendReqResume(prAdapter,
			prStaRec, prGetTsfCtxt->ucTWTFlowId,
			u8NextTWT, ucNextTWTSize);

		break;
	}

	default:
		DBGLOG(TWT_PLANNER, ERROR,
			"Unknown reason to get TSF %u\n",
			prGetTsfCtxt->ucReason);
		break;
	}

	/* free memory */
	kalMemFree(prGetTsfCtxt, VIR_MEM_TYPE, sizeof(*prGetTsfCtxt));
}

static uint32_t
twtPlannerGetCurrentTSF(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen)
{
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	struct _EXT_CMD_GET_MAC_INFO_T *prMacInfoCmd;
	struct _EXTRA_ARG_TSF_T *prTsfArg;

	prMacInfoCmd = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _EXT_CMD_GET_MAC_INFO_T));
	if (!prMacInfoCmd) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Alloc _EXT_CMD_GET_MAC_INFO_T FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prTsfArg = &(prMacInfoCmd->rExtraArgument.rTsfArg);
	prMacInfoCmd->u2MacInfoId = CPU_TO_LE16(MAC_INFO_TYPE_TSF);
	prTsfArg->ucHwBssidIndex = prBssInfo->ucOwnMacIndex;

	rWlanStatus = wlanSendSetQueryExtCmd(prAdapter,
					CMD_ID_LAYER_0_EXT_MAGIC_NUM,
					EXT_CMD_ID_GET_MAC_INFO,
					FALSE,
					TRUE,
					FALSE,
					twtPlannerGetTsfDone,
					NULL,
					sizeof(struct _EXT_CMD_GET_MAC_INFO_T),
					(uint8_t *) (prMacInfoCmd),
					pvSetBuffer, u4SetBufferLen);

	return rWlanStatus;
}

void twtPlannerSetParams(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_PARAMS_SET_T *prTWTParamSetMsg;
	struct _TWT_CTRL_T rTWTCtrl, *prTWTCtrl = &rTWTCtrl;
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint8_t ucBssIdx, ucFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTParamSetMsg = (struct _MSG_TWT_PARAMS_SET_T *) prMsgHdr;
	kalMemCopy(prTWTCtrl, &prTWTParamSetMsg->rTWTCtrl, sizeof(*prTWTCtrl));

	cnmMemFree(prAdapter, prMsgHdr);

	/* Find the BSS info */
	ucBssIdx = prTWTCtrl->ucBssIdx;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
		prBssInfo->eConnectionState != MEDIA_STATE_CONNECTED) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Current op mode %d connection state %d\n",
			prBssInfo->eCurrentOPMode, prBssInfo->eConnectionState);
		return;
	}

	/* Get the STA Record */
	prStaRec = prBssInfo->prStaRecOfAP;
	if (!prStaRec) {
		DBGLOG(TWT_PLANNER, ERROR, "No AP STA Record\n");
		return;
	}

	/* If bypassing TWT nego, this ctrl param is treated as a TWT agrt*/
	if (IS_TWT_PARAM_ACTION_ADD_BYPASS(prTWTCtrl->ucCtrlAction)) {
		struct _TWT_GET_TSF_CONTEXT_T *prGetTsfCtxt =
			kalMemAlloc(sizeof(struct _TWT_GET_TSF_CONTEXT_T),
				VIR_MEM_TYPE);
		if (prGetTsfCtxt == NULL) {
			DBGLOG(TWT_PLANNER, ERROR, "mem alloc failed\n");
			return;
		}

		prGetTsfCtxt->ucReason = TWT_GET_TSF_FOR_ADD_AGRT_BYPASS;
		prGetTsfCtxt->ucBssIdx = ucBssIdx;
		prGetTsfCtxt->ucTWTFlowId = prTWTCtrl->ucTWTFlowId;
		prGetTsfCtxt->fgIsOid = FALSE;
		kalMemCopy(&(prGetTsfCtxt->rTWTParams),
				&(prTWTCtrl->rTWTParams),
				sizeof(struct _TWT_PARAMS_T));
		twtPlannerGetCurrentTSF(prAdapter,
			prBssInfo, prGetTsfCtxt, sizeof(*prGetTsfCtxt));

		return;
	}

	/* Check if peer has TWT responder capability and local config */
	if (!HE_IS_MAC_CAP_TWT_RSP(prStaRec->ucHeMacCapInfo) ||
		!IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTWTRequester)) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Peer cap 0x%x user config of TWT req %u\n",
			prStaRec->ucHeMacCapInfo[0],
			prAdapter->rWifiVar.ucTWTRequester);
		return;
	}

	/* For COEX concern, suppose only 5G is allowed */
	if ((prAdapter->rWifiVar.ucTWTStaBandBitmap & prBssInfo->eBand)
		!= prBssInfo->eBand) {
		DBGLOG(TWT_PLANNER, ERROR,
			"TWT BAND support bitmaps(%u)!=%u\n",
			prAdapter->rWifiVar.ucTWTStaBandBitmap,
			prBssInfo->eBand);
		return;
	}

	ucFlowId = prTWTCtrl->ucTWTFlowId;

	switch (prTWTCtrl->ucCtrlAction) {
	case TWT_PARAM_ACTION_ADD:
		if (twtPlannerDrvAgrtFind(
			prAdapter, ucBssIdx, ucFlowId) >= TWT_AGRT_MAX_NUM) {

			struct _TWT_GET_TSF_CONTEXT_T *prGetTsfCtxt =
				kalMemAlloc(
					sizeof(struct _TWT_GET_TSF_CONTEXT_T),
					VIR_MEM_TYPE);
			if (prGetTsfCtxt == NULL) {
				DBGLOG(TWT_PLANNER, ERROR,
					"mem alloc failed\n");
				return;
			}

			prGetTsfCtxt->ucReason = TWT_GET_TSF_FOR_ADD_AGRT;
			prGetTsfCtxt->ucBssIdx = ucBssIdx;
			prGetTsfCtxt->ucTWTFlowId = prTWTCtrl->ucTWTFlowId;
			prGetTsfCtxt->fgIsOid = FALSE;
			kalMemCopy(&(prGetTsfCtxt->rTWTParams),
					&(prTWTCtrl->rTWTParams),
					sizeof(struct _TWT_PARAMS_T));
			twtPlannerGetCurrentTSF(prAdapter, prBssInfo,
				prGetTsfCtxt, sizeof(*prGetTsfCtxt));

			return;
		}

		DBGLOG(TWT_PLANNER, ERROR,
			"BSS %u TWT flow %u already exists\n",
			ucBssIdx, ucFlowId);
		break;

	case TWT_PARAM_ACTION_DEL:
		if (twtPlannerDrvAgrtFind(
			prAdapter, ucBssIdx, ucFlowId) < TWT_AGRT_MAX_NUM) {
			/* Start the process to tear down this TWT agreement */
			twtPlannerSendReqTeardown(prAdapter,
				prStaRec, ucFlowId);
		} else {
			DBGLOG(TWT_PLANNER, ERROR,
				"BSS %u TWT flow %u doesn't exist\n",
				ucBssIdx, ucFlowId);
		}
		break;

	case TWT_PARAM_ACTION_SUSPEND:
		if (twtPlannerDrvAgrtFind(
			prAdapter, ucBssIdx, ucFlowId) < TWT_AGRT_MAX_NUM) {
			/* Start the process to suspend this TWT agreement */
			twtPlannerSendReqSuspend(prAdapter,
				prStaRec, ucFlowId);
		} else {
			DBGLOG(TWT_PLANNER, ERROR,
				"BSS %u TWT flow %u doesn't exist\n",
				ucBssIdx, ucFlowId);
		}
		break;

	case TWT_PARAM_ACTION_RESUME:
		if (twtPlannerDrvAgrtFind(
			prAdapter, ucBssIdx, ucFlowId) < TWT_AGRT_MAX_NUM) {
			struct _TWT_GET_TSF_CONTEXT_T *prGetTsfCtxt =
				kalMemAlloc(
					sizeof(struct _TWT_GET_TSF_CONTEXT_T),
					VIR_MEM_TYPE);
			if (prGetTsfCtxt == NULL) {
				DBGLOG(TWT_PLANNER, ERROR,
					"mem alloc failed\n");
				return;
			}

			prGetTsfCtxt->ucReason = TWT_GET_TSF_FOR_RESUME_AGRT;
			prGetTsfCtxt->ucBssIdx = ucBssIdx;
			prGetTsfCtxt->ucTWTFlowId = prTWTCtrl->ucTWTFlowId;
			prGetTsfCtxt->fgIsOid = FALSE;
			twtPlannerGetCurrentTSF(prAdapter, prBssInfo,
				prGetTsfCtxt, sizeof(*prGetTsfCtxt));
		} else {
			DBGLOG(TWT_PLANNER, ERROR,
				"BSS %u TWT flow %u doesn't exist\n",
				ucBssIdx, ucFlowId);
		}

		break;

	default:
		DBGLOG(TWT_PLANNER, ERROR,
			"Action %u not supported\n", prTWTCtrl->ucCtrlAction);
		break;
	}
}

void twtPlannerRxNegoResult(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_IND_RESULT_T *prTWTFsmResultMsg;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucTWTFlowId;
	struct _TWT_PARAMS_T *prTWTResult, *prTWTParams;
	struct _TWT_FLOW_T *prTWTFlow;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTFsmResultMsg = (struct _MSG_TWT_REQFSM_IND_RESULT_T *) prMsgHdr;
	prStaRec = prTWTFsmResultMsg->prStaRec;
	ucTWTFlowId = prTWTFsmResultMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	cnmMemFree(prAdapter, prMsgHdr);

	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Rx nego result: invalid STA Type %d\n",
			prStaRec->eStaType);
		return;
	}

	prTWTFlow = &(prStaRec->arTWTFlow[ucTWTFlowId]);
	prTWTResult = &(prTWTFlow->rTWTPeerParams);

	switch (prTWTResult->ucSetupCmd) {
	case TWT_SETUP_CMD_ACCEPT:
		/* Update agreement table */
		twtPlannerAddAgrtTbl(prAdapter, prBssInfo, prStaRec,
			prTWTResult, ucTWTFlowId, FALSE,
			NULL, NULL /* handle TWT cmd timeout? */);

		/* Disable SCAN during TWT activity */
		prAdapter->fgEnOnlineScan = FALSE;

		break;

	case TWT_SETUP_CMD_ALTERNATE:
	case TWT_SETUP_CMD_DICTATE:
		/* Use AP's suggestions */
		prTWTParams = &(prTWTFlow->rTWTParams);
		kalMemCopy(prTWTParams,
			prTWTResult, sizeof(struct _TWT_PARAMS_T));
		prTWTParams->ucSetupCmd = TWT_SETUP_CMD_SUGGEST;
		prTWTParams->fgReq = 1;
		twtPlannerSendReqStart(prAdapter, prStaRec, ucTWTFlowId);
		break;

	case TWT_SETUP_CMD_REJECT:
		/* Clear TWT flow in StaRec */
		break;

	default:
		DBGLOG(TWT_PLANNER, ERROR,
			"Unknown setup command %u\n", prTWTResult->ucSetupCmd);
		ASSERT(0);
		break;

	}
}

void twtPlannerSuspendDone(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_IND_RESULT_T *prTWTFsmResultMsg;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucTWTFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTFsmResultMsg = (struct _MSG_TWT_REQFSM_IND_RESULT_T *) prMsgHdr;
	prStaRec = prTWTFsmResultMsg->prStaRec;
	ucTWTFlowId = prTWTFsmResultMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	cnmMemFree(prAdapter, prMsgHdr);

	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Rx suspend result: invalid STA Type %d\n",
			prStaRec->eStaType);
		return;
	}

	/* Delete only FW TWT agreement entry */
	twtPlannerDelAgrtTbl(prAdapter, prBssInfo, prStaRec,
		ucTWTFlowId, FALSE,
		NULL, NULL /* handle TWT cmd timeout? */, FALSE);

	/* Teardown FW TWT agreement entry */
	twtPlannerTeardownAgrtTbl(prAdapter, prStaRec,
		FALSE, NULL, NULL /* handle TWT cmd timeout? */);

}

void twtPlannerResumeDone(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_IND_RESULT_T *prTWTFsmResultMsg;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucTWTFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTFsmResultMsg = (struct _MSG_TWT_REQFSM_IND_RESULT_T *) prMsgHdr;
	prStaRec = prTWTFsmResultMsg->prStaRec;
	ucTWTFlowId = prTWTFsmResultMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	cnmMemFree(prAdapter, prMsgHdr);

	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Rx suspend result: invalid STA Type %d\n",
			prStaRec->eStaType);
		return;
	}

	/* Add back the FW TWT agreement entry */
	twtPlannerResumeAgrtTbl(prAdapter, prBssInfo, prStaRec,
		ucTWTFlowId, FALSE,
		NULL, NULL /* handle TWT cmd timeout? */);

}

void twtPlannerTeardownDone(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_IND_RESULT_T *prTWTFsmResultMsg;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucTWTFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTFsmResultMsg = (struct _MSG_TWT_REQFSM_IND_RESULT_T *) prMsgHdr;
	prStaRec = prTWTFsmResultMsg->prStaRec;
	ucTWTFlowId = prTWTFsmResultMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	cnmMemFree(prAdapter, prMsgHdr);

	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Rx teardown result: invalid STA Type %d\n",
			prStaRec->eStaType);
		return;
	}

	/* Delete driver & FW TWT agreement entry */
	twtPlannerDelAgrtTbl(prAdapter, prBssInfo, prStaRec,
		ucTWTFlowId, FALSE,
		NULL, NULL /* handle TWT cmd timeout? */, TRUE);

	/* Teardown FW TWT agreement entry */
	twtPlannerTeardownAgrtTbl(prAdapter, prStaRec,
		FALSE, NULL, NULL /* handle TWT cmd timeout? */);

	/* Enable SCAN after TWT agrt has been tear down */
	prAdapter->fgEnOnlineScan = TRUE;
}

void twtPlannerRxInfoFrm(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_IND_INFOFRM_T *prTWTFsmInfoFrmMsg;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	uint8_t ucTWTFlowId;
	struct _NEXT_TWT_INFO_T rNextTWTInfo;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTFsmInfoFrmMsg = (struct _MSG_TWT_REQFSM_IND_INFOFRM_T *) prMsgHdr;
	prStaRec = prTWTFsmInfoFrmMsg->prStaRec;
	ucTWTFlowId = prTWTFsmInfoFrmMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Rx info frame: invalid STA Type %d\n",
			prStaRec->eStaType);
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	kalMemCopy(&(rNextTWTInfo), &(prTWTFsmInfoFrmMsg->rNextTWTInfo),
		sizeof(struct _NEXT_TWT_INFO_T));

	cnmMemFree(prAdapter, prMsgHdr);

	/* Modify the TWT agreement entry */
	twtPlannerModifyAgrtTbl(prAdapter, prBssInfo, prStaRec,
		&rNextTWTInfo, ucTWTFlowId, FALSE,
		NULL, NULL /* handle TWT cmd timeout? */);

}
