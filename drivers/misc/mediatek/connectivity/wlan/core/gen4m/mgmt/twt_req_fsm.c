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
/*! \file   "twt_req_fsm.c"
*   \brief  FSM for TWT Requesting STA negotiation
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

static uint8_t *apucDebugTWTReqState[TWT_REQ_STATE_NUM] = {
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_IDLE"),
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_REQTX"),
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_WAIT_RSP"),
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_SUSPENDING"),
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_SUSPENDED"),
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_RESUMING"),
	(uint8_t *) DISP_STRING("TWT_REQ_STATE_TEARING_DOWN"),
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static uint32_t
twtReqFsmSendEvent(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	enum ENUM_MSG_ID eMsgId);

static uint32_t
twtReqFsmSendEventRxInfoFrm(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* @brief The Core FSM engine of TWT Requester Module.
*
* @param[in] prStaRec           Pointer to the STA_RECORD_T
* @param[in] eNextState         The value of Next State
* @param[in] prRetainedSwRfb     SW_RFB_T for JOIN Success
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void
twtReqFsmSteps(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	enum _ENUM_TWT_REQUESTER_STATE_T eNextState,
	uint8_t ucTWTFlowId,
	void *pParam)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	enum _ENUM_TWT_REQUESTER_STATE_T ePreState;
	uint8_t fgIsTransition;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	do {

		DBGLOG(TWT_REQUESTER, STATE,
		"[TWT_REQ] Flow %d TRANSITION: [%s] -> [%s]\n",
		ucTWTFlowId,
		apucDebugTWTReqState[prStaRec->aeTWTReqState],
		apucDebugTWTReqState[eNextState]);

		ePreState = prStaRec->aeTWTReqState;

		prStaRec->aeTWTReqState = eNextState;
		fgIsTransition = (uint8_t) FALSE;

		switch (prStaRec->aeTWTReqState) {
		case TWT_REQ_STATE_IDLE:
			/* Notify TWT Planner of the negotiation result */
			if (ePreState == TWT_REQ_STATE_WAIT_RSP) {
				twtReqFsmSendEvent(prAdapter, prStaRec,
					ucTWTFlowId, MID_TWT_REQ_IND_RESULT);
				/* TODO: how to handle failures */
			} else if (ePreState == TWT_REQ_STATE_TEARING_DOWN) {
				twtReqFsmSendEvent(prAdapter, prStaRec,
					ucTWTFlowId,
					MID_TWT_REQ_IND_TEARDOWN_DONE);
			} else if (ePreState == TWT_REQ_STATE_RESUMING) {
				twtReqFsmSendEvent(prAdapter, prStaRec,
					ucTWTFlowId,
					MID_TWT_REQ_IND_RESUME_DONE);
			}
			break;

		case TWT_REQ_STATE_REQTX:
		{
			struct _TWT_PARAMS_T *prTWTParams =
				(struct _TWT_PARAMS_T *)pParam;
			ASSERT(prTWTParams);
			rStatus = twtSendSetupFrame(
				prAdapter, prStaRec, ucTWTFlowId,
				prTWTParams, twtReqFsmRunEventTxDone);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				eNextState = TWT_REQ_STATE_IDLE;
				fgIsTransition = TRUE;
			}
			break;
		}

		case TWT_REQ_STATE_WAIT_RSP:
			break;

		case TWT_REQ_STATE_TEARING_DOWN:
			rStatus = twtSendTeardownFrame(
				prAdapter, prStaRec, ucTWTFlowId,
				twtReqFsmRunEventTxDone);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				eNextState = TWT_REQ_STATE_IDLE;
				fgIsTransition = TRUE;
			}
			break;

		case TWT_REQ_STATE_SUSPENDING:
		{
			struct _NEXT_TWT_INFO_T rNextTWTInfo = {0};

			rStatus = twtSendInfoFrame(
				prAdapter, prStaRec, ucTWTFlowId, &rNextTWTInfo,
				twtReqFsmRunEventTxDone);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				eNextState = TWT_REQ_STATE_IDLE;
				fgIsTransition = TRUE;
			}
			break;
		}

		case TWT_REQ_STATE_RESUMING:
		{
			struct _NEXT_TWT_INFO_T *prNextTWTInfo =
				(struct _NEXT_TWT_INFO_T *)pParam;
			rStatus = twtSendInfoFrame(
				prAdapter, prStaRec, ucTWTFlowId, prNextTWTInfo,
				twtReqFsmRunEventTxDone);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				eNextState = TWT_REQ_STATE_IDLE;
				fgIsTransition = TRUE;
			}

			break;
		}

		case TWT_REQ_STATE_SUSPENDED:
			twtReqFsmSendEvent(prAdapter, prStaRec,
				ucTWTFlowId, MID_TWT_REQ_IND_SUSPEND_DONE);
			break;

		case TWT_REQ_STATE_RX_TEARDOWN:
			twtReqFsmSendEvent(prAdapter, prStaRec,
				ucTWTFlowId, MID_TWT_REQ_IND_TEARDOWN_DONE);
			break;

		case TWT_REQ_STATE_RX_INFOFRM:
		{
			struct _NEXT_TWT_INFO_T *prNextTWTInfo =
				(struct _NEXT_TWT_INFO_T *)pParam;
			twtReqFsmSendEventRxInfoFrm(prAdapter, prStaRec,
				ucTWTFlowId, prNextTWTInfo);
			break;
		}

		default:
			DBGLOG(TWT_REQUESTER, ERROR,
				"Unknown TWT_REQUESTER STATE\n");
			ASSERT(0);
			break;
		}

	} while (fgIsTransition);
}

static uint32_t
twtReqFsmSendEvent(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	enum ENUM_MSG_ID eMsgId)
{
	struct _MSG_TWT_REQFSM_IND_RESULT_T *prTWTFsmResultMsg;

	prTWTFsmResultMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_IND_RESULT_T));
	if (prTWTFsmResultMsg) {
		prTWTFsmResultMsg->rMsgHdr.eMsgId = eMsgId;
		prTWTFsmResultMsg->prStaRec = prStaRec;
		prTWTFsmResultMsg->ucTWTFlowId = ucTWTFlowId;

		mboxSendMsg(prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prTWTFsmResultMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return WLAN_STATUS_RESOURCES;

	return WLAN_STATUS_SUCCESS;
}

static uint32_t
twtReqFsmSendEventRxInfoFrm(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo)
{
	struct _MSG_TWT_REQFSM_IND_INFOFRM_T *prTWTFsmInfoFrmMsg;

	prTWTFsmInfoFrmMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_IND_INFOFRM_T));
	if (prTWTFsmInfoFrmMsg) {
		prTWTFsmInfoFrmMsg->rMsgHdr.eMsgId = MID_TWT_REQ_IND_INFOFRM;
		prTWTFsmInfoFrmMsg->prStaRec = prStaRec;
		prTWTFsmInfoFrmMsg->ucTWTFlowId = ucTWTFlowId;
		kalMemCopy(&(prTWTFsmInfoFrmMsg->rNextTWTInfo), prNextTWTInfo,
			sizeof(struct _NEXT_TWT_INFO_T));

		mboxSendMsg(prAdapter, MBOX_ID_0,
			(struct MSG_HDR *) prTWTFsmInfoFrmMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return WLAN_STATUS_RESOURCES;

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Start Event to TWT FSM.
*
* @param[in] prMsgHdr   Message of Request for a particular STA.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void twtReqFsmRunEventStart(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_START_T *prTWTReqFsmStartMsg;
	struct STA_RECORD *prStaRec;
	struct _TWT_PARAMS_T *prTWTParams;
	uint8_t ucTWTFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTReqFsmStartMsg = (struct _MSG_TWT_REQFSM_START_T *) prMsgHdr;
	prStaRec = prTWTReqFsmStartMsg->prStaRec;
	ucTWTFlowId = prTWTReqFsmStartMsg->ucTWTFlowId;
	prTWTParams = &(prStaRec->arTWTFlow[ucTWTFlowId].rTWTParams);

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	ASSERT(prStaRec);
	ASSERT(prTWTParams);

	DBGLOG(TWT_REQUESTER, LOUD,
		"EVENT-START: TWT Requester FSM %d\n", ucTWTFlowId);

	cnmMemFree(prAdapter, prMsgHdr);

	/* Validation of TWT Requester Start Event */
	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_REQUESTER, ERROR,
			"EVENT-START: Invalid Type %d\n",
			prStaRec->eStaType);

		/* TODO: Notify TWT Planner */

		return;
	}

	twtReqFsmSteps(prAdapter, prStaRec,
		TWT_REQ_STATE_REQTX, ucTWTFlowId, prTWTParams);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Teardown Event to TWT FSM.
*
* @param[in] prMsgHdr   Message of Request for a particular STA.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void twtReqFsmRunEventTeardown(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_TEARDOWN_T *prTWTReqFsmTeardownMsg;
	struct STA_RECORD *prStaRec;
	uint8_t ucTWTFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTReqFsmTeardownMsg = (struct _MSG_TWT_REQFSM_TEARDOWN_T *) prMsgHdr;
	prStaRec = prTWTReqFsmTeardownMsg->prStaRec;
	ucTWTFlowId = prTWTReqFsmTeardownMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	ASSERT(prStaRec);

	DBGLOG(TWT_REQUESTER, LOUD, "EVENT-TEARDOWN: TWT Requester FSM %d\n",
		ucTWTFlowId);

	cnmMemFree(prAdapter, prMsgHdr);

	/* Validation of TWT Requester Teardown Event */
	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_REQUESTER, ERROR, "Invalid STA Type %d\n",
			prStaRec->eStaType);

		/* TODO: Notify TWT Planner */

		return;
	}

	twtReqFsmSteps(prAdapter, prStaRec, TWT_REQ_STATE_TEARING_DOWN,
		ucTWTFlowId, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Suspend Event to TWT FSM.
*
* @param[in] prMsgHdr   Message of Request for a particular STA.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void twtReqFsmRunEventSuspend(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_SUSPEND_T *prTWTReqFsmSuspendMsg;
	struct STA_RECORD *prStaRec;
	uint8_t ucTWTFlowId;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTReqFsmSuspendMsg = (struct _MSG_TWT_REQFSM_SUSPEND_T *) prMsgHdr;
	prStaRec = prTWTReqFsmSuspendMsg->prStaRec;
	ucTWTFlowId = prTWTReqFsmSuspendMsg->ucTWTFlowId;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	ASSERT(prStaRec);

	DBGLOG(TWT_REQUESTER, LOUD, "EVENT-SUSPEND: TWT Requester FSM %d\n",
		ucTWTFlowId);

	cnmMemFree(prAdapter, prMsgHdr);

	/* Validation of TWT Requester Suspend Event */
	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_REQUESTER, ERROR, "Invalid STA Type %d\n",
			prStaRec->eStaType);

		/* TODO: Notify TWT Planner */

		return;
	}

	twtReqFsmSteps(prAdapter, prStaRec, TWT_REQ_STATE_SUSPENDING,
		ucTWTFlowId, NULL);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Suspend Event to TWT FSM.
*
* @param[in] prMsgHdr   Message of Request for a particular STA.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void twtReqFsmRunEventResume(
	struct ADAPTER *prAdapter, struct MSG_HDR *prMsgHdr)
{
	struct _MSG_TWT_REQFSM_RESUME_T *prTWTReqFsmResumeMsg;
	struct STA_RECORD *prStaRec;
	uint8_t ucTWTFlowId;
	struct _NEXT_TWT_INFO_T rNextTWTInfo;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prTWTReqFsmResumeMsg = (struct _MSG_TWT_REQFSM_RESUME_T *) prMsgHdr;
	prStaRec = prTWTReqFsmResumeMsg->prStaRec;
	ucTWTFlowId = prTWTReqFsmResumeMsg->ucTWTFlowId;
	rNextTWTInfo.u8NextTWT = prTWTReqFsmResumeMsg->u8NextTWT;
	rNextTWTInfo.ucNextTWTSize = prTWTReqFsmResumeMsg->ucNextTWTSize;

	if ((!prStaRec) || (prStaRec->fgIsInUse == FALSE)) {
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	ASSERT(prStaRec);

	DBGLOG(TWT_REQUESTER, LOUD, "EVENT-RESUME: TWT Requester FSM %d\n",
		ucTWTFlowId);

	cnmMemFree(prAdapter, prMsgHdr);

	/* Validation of TWT Requester Teardown Event */
	if (!IS_AP_STA(prStaRec)) {
		DBGLOG(TWT_REQUESTER, ERROR, "Invalid STA Type %d\n",
			prStaRec->eStaType);

		/* TODO: Notify TWT Planner */

		return;
	}

	twtReqFsmSteps(prAdapter, prStaRec, TWT_REQ_STATE_RESUMING,
		ucTWTFlowId, (void *)&rNextTWTInfo);
}

uint32_t
twtReqFsmRunEventTxDone(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec;
	enum _ENUM_TWT_REQUESTER_STATE_T eNextState;
	uint8_t ucTWTFlowId;

	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec) {
		DBGLOG(TWT_REQUESTER, ERROR,
			"EVENT-TXDONE: No valid STA Record\n");
		return WLAN_STATUS_INVALID_PACKET;
	}

	if (rTxDoneStatus)
		DBGLOG(TWT_REQUESTER, INFO,
			"EVENT-TX DONE [status: %d][seq: %d]: Current Time = %d\n",
		   rTxDoneStatus, prMsduInfo->ucTxSeqNum, kalGetTimeTick());

	/* Next state is set to current state
	 *by default and check Tx done status to transition if possible
	 */
	eNextState = prStaRec->aeTWTReqState;

	switch (prStaRec->aeTWTReqState) {
	case TWT_REQ_STATE_REQTX:

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			eNextState = TWT_REQ_STATE_WAIT_RSP;
		else
			eNextState = TWT_REQ_STATE_IDLE;

		ucTWTFlowId = twtGetTxSetupFlowId(prMsduInfo);
		twtReqFsmSteps(prAdapter,
			prStaRec, eNextState, ucTWTFlowId, NULL);

		break;

	case TWT_REQ_STATE_TEARING_DOWN:

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			eNextState = TWT_REQ_STATE_IDLE;

		ucTWTFlowId = twtGetTxTeardownFlowId(prMsduInfo);
		twtReqFsmSteps(prAdapter, prStaRec, eNextState,
			ucTWTFlowId, NULL);

		break;

	case TWT_REQ_STATE_SUSPENDING:
		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			eNextState = TWT_REQ_STATE_SUSPENDED;

		ucTWTFlowId = twtGetTxInfoFlowId(prMsduInfo);
		twtReqFsmSteps(prAdapter, prStaRec, eNextState,
			ucTWTFlowId, NULL);

		break;

	case TWT_REQ_STATE_RESUMING:
		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			eNextState = TWT_REQ_STATE_IDLE;

		ucTWTFlowId = twtGetTxInfoFlowId(prMsduInfo);
		twtReqFsmSteps(prAdapter, prStaRec, eNextState,
			ucTWTFlowId, NULL);

		break;

	default:
		break;		/* Ignore other cases */
	}

	return WLAN_STATUS_SUCCESS;
}

void twtReqFsmRunEventRxSetup(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId)
{
	if (!IS_AP_STA(prStaRec))
		return;

	switch (prStaRec->aeTWTReqState) {
	case TWT_REQ_STATE_WAIT_RSP:
		/* transition to the IDLE state */
		twtReqFsmSteps(prAdapter,
			prStaRec, TWT_REQ_STATE_IDLE, ucTWTFlowId, NULL);
		break;

	default:
		break;		/* Ignore other cases */
	}
}

void twtReqFsmRunEventRxTeardown(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId)
{
	if (!IS_AP_STA(prStaRec))
		return;

	switch (prStaRec->aeTWTReqState) {
	case TWT_REQ_STATE_IDLE:
		/* transition to the RX TEARDOWN state */
		twtReqFsmSteps(prAdapter, prStaRec, TWT_REQ_STATE_RX_TEARDOWN,
			ucTWTFlowId, NULL);
		break;

	default:
		break;		/* Ignore other cases */
	}
}

void twtReqFsmRunEventRxInfoFrm(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	struct STA_RECORD *prStaRec,
	uint8_t ucTWTFlowId,
	struct _NEXT_TWT_INFO_T *prNextTWTInfo)
{
	if (!IS_AP_STA(prStaRec))
		return;

	switch (prStaRec->aeTWTReqState) {
	case TWT_REQ_STATE_IDLE:
		/* transition to the RX Info frame state */
		twtReqFsmSteps(prAdapter, prStaRec, TWT_REQ_STATE_RX_INFOFRM,
			ucTWTFlowId, prNextTWTInfo);
		break;

	default:
		break;		/* Ignore other cases */
	}
}
