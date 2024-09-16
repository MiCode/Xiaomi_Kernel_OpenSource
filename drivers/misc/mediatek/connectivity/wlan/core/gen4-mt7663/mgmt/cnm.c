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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/cnm.c#2
 */

/*! \file   "cnm.c"
 *    \brief  Module of Concurrent Network Management
 *
 *    Module of Concurrent Network Management
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#if CFG_SUPPORT_DBDC
#define DBDC_SWITCH_GUARD_TIME		(4*1000)	/* ms */
#define DBDC_DISABLE_COUNTDOWN_TIME	(2*1000)	/* ms */
#endif /* CFG_SUPPORT_DBDC */

#if CFG_SUPPORT_IDC_CH_SWITCH
#define IDC_CSA_GUARD_TIME			(60)	/* 60 Sec */
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
#if CFG_SUPPORT_DBDC
enum ENUM_DBDC_GUARD_TIMER_T {
	ENUM_DBDC_GUARD_TIMER_NONE,

	/* Prevent switch too quick */
	ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME,

	/* Prevent continuously trigger by reconnection */
	ENUM_DBDC_GUARD_TIMER_DISABLE_COUNT_DOWN,

	ENUM_DBDC_GUARD_TIMER_NUM
};

enum ENUM_DBDC_FSM_STATE_T {
	ENUM_DBDC_FSM_STATE_DISABLE_IDLE,
	ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_ENABLE,
	ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE,
	ENUM_DBDC_FSM_STATE_ENABLE_GUARD,
	ENUM_DBDC_FSM_STATE_ENABLE_IDLE,
	ENUM_DBDC_FSM_STATE_WAIT_HW_DISABLE,
	ENUM_DBDC_FSM_STATE_DISABLE_GUARD,
	ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_DISABLE,
	ENUM_DBDC_FSM_STATE_NUM
};

enum ENUM_OPMODE_STATE_T {
	ENUM_OPMODE_STATE_DONE,
	ENUM_OPMODE_STATE_FAIL,
	ENUM_OPMODE_STATE_WAIT,
	ENUM_OPMODE_STATE_NUM
};

struct DBDC_INFO_T {
	enum ENUM_DBDC_FSM_STATE_T eDbdcFsmCurrState;
	enum ENUM_DBDC_FSM_STATE_T eDbdcFsmPrevState;
	enum ENUM_DBDC_FSM_STATE_T eDbdcFsmNextState;

	struct TIMER rDbdcGuardTimer;
	enum ENUM_DBDC_GUARD_TIMER_T eDdbcGuardTimerType;

	uint8_t fgReqPrivelegeLock;
	struct LINK rPendingMsgList;

	uint8_t fgDbdcDisableOpmodeChangeDone;
	enum ENUM_OPMODE_STATE_T eBssOpModeState[BSSID_NUM];

	/* Set DBDC setting for incoming network */
	uint8_t ucPrimaryChannel;
	uint8_t ucWmmQueIdx;

	/* Used for iwpriv to force enable DBDC*/
	bool fgHasSentCmd;
	bool fgCmdEn;
};

enum ENUM_DBDC_FSM_EVENT_T {
	DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG,
	DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG,
	DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO,
	DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO,
	DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS,
	DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL,
	DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE,
	DBDC_FSM_EVENT_ERR,
	DBDC_FSM_EVENT_NUM
};

enum ENUM_DBDC_PROTOCOL_STATUS_T {
	ENUM_DBDC_PROTOCOL_STATUS_DONE_SUCCESS = 0,
	ENUM_DBDC_PROTOCOL_STATUS_DONE_FAIL,
	ENUM_DBDC_PROTOCOL_STATUS_WAIT,
	ENUM_DBDC_PROTOCOL_STATUS_NUM
};

typedef void (*DBDC_ENTRY_FUNC)(struct ADAPTER *);
typedef void (*DBDC_EVENT_HNDL_FUNC)(struct ADAPTER *,
				     enum ENUM_DBDC_FSM_EVENT_T);
typedef void (*DBDC_EXIT_FUNC)(struct ADAPTER *);

struct DBDC_FSM_T {
	DBDC_ENTRY_FUNC pfEntryFunc;
	DBDC_EVENT_HNDL_FUNC pfEventHandlerFunc;
	DBDC_EXIT_FUNC pfExitFunc;
};
#endif /*CFG_SUPPORT_DBDC*/

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
#if CFG_SUPPORT_IDC_CH_SWITCH
struct EVENT_LTE_SAFE_CHN g_rLteSafeChInfo;
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */
#if CFG_SUPPORT_DBDC
static struct DBDC_INFO_T g_rDbdcInfo;
#endif

#if CFG_SUPPORT_IDC_CH_SWITCH
OS_SYSTIME g_rLastCsaSysTime;
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

#if CFG_SUPPORT_DBDC
#define DBDC_IS_BSS_ALIVE(_prBssInfo) \
	(_prBssInfo->fgIsInUse && \
	_prBssInfo->fgIsNetActive && \
	(_prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED || \
	_prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT))

#define DBDC_IS_BSS_NOT_ALIVE(_prBssInfo) \
	(!_prBssInfo->fgIsInUse || \
	!_prBssInfo->fgIsNetActive || \
	(_prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED && \
	_prBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT))

#define DBDC_SET_GUARD_TIME(_prAdapter) { \
	cnmTimerStartTimer(_prAdapter, \
		&g_rDbdcInfo.rDbdcGuardTimer, \
		DBDC_SWITCH_GUARD_TIME); \
	g_rDbdcInfo.eDdbcGuardTimerType = \
		ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME; \
}

#define DBDC_SET_DISABLE_COUNTDOWN(_prAdapter) { \
	cnmTimerStartTimer(_prAdapter, \
		&g_rDbdcInfo.rDbdcGuardTimer, \
		DBDC_DISABLE_COUNTDOWN_TIME); \
	g_rDbdcInfo.eDdbcGuardTimerType = \
		ENUM_DBDC_GUARD_TIMER_DISABLE_COUNT_DOWN; \
	}

#define DBDC_FSM_MSG_WRONG_EVT(_eEvent) \
	log_dbg(CNM, WARN, \
		"[DBDC] Should not reveice evt %u during state %u\n", \
		_eEvent, \
		g_rDbdcInfo.eDbdcFsmCurrState)

#define DBDC_FSM_MSG_ERROR_EVT(_eEvent) \
	log_dbg(CNM, ERROR, "[DBDC] Reveice evt %u during state %u\n", \
		_eEvent, \
		g_rDbdcInfo.eDbdcFsmCurrState)

#define USE_DBDC_CAPABILITY() \
	((g_rDbdcInfo.eDbdcFsmCurrState \
		== ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_ENABLE || \
	g_rDbdcInfo.eDbdcFsmCurrState \
		== ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE || \
	g_rDbdcInfo.eDbdcFsmCurrState \
		== ENUM_DBDC_FSM_STATE_ENABLE_GUARD || \
	g_rDbdcInfo.eDbdcFsmCurrState \
		== ENUM_DBDC_FSM_STATE_ENABLE_IDLE)?TRUE:FALSE)

#define DBDC_SET_WMMBAND_FW_AUTO_BY_CHNL(_ucPrimaryChannel, _ucWmmQueIdx) \
	{ \
		g_rDbdcInfo.ucPrimaryChannel = (_ucPrimaryChannel);\
		g_rDbdcInfo.ucWmmQueIdx = (_ucWmmQueIdx);\
	}

#define DBDC_SET_WMMBAND_FW_AUTO_DEFAULT() \
	{ \
		g_rDbdcInfo.ucPrimaryChannel = 0; \
		g_rDbdcInfo.ucWmmQueIdx = 0;\
	}

#define DBDC_UPDATE_CMD_WMMBAND_FW_AUTO(_prCmdBody) \
	{ \
		(_prCmdBody)->ucPrimaryChannel = g_rDbdcInfo.ucPrimaryChannel; \
		(_prCmdBody)->ucWmmQueIdx = g_rDbdcInfo.ucWmmQueIdx; \
		DBDC_SET_WMMBAND_FW_AUTO_DEFAULT(); \
	}

#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static u_int8_t
cnmDBDCIsReqPeivilegeLock(
	void
);

static void
cnmDbdcFsmEntryFunc_DISABLE_IDLE(
	IN struct ADAPTER *prAdapter
);

static void
cnmDbdcFsmEntryFunc_WAIT_PROTOCOL_ENABLE(
	IN struct ADAPTER *prAdapter
);

static void
cnmDbdcFsmEntryFunc_WAIT_HW_ENABLE(
	IN struct ADAPTER *prAdapter
);

static void
cnmDbdcFsmEntryFunc_ENABLE_GUARD(
	IN struct ADAPTER *prAdapter
);

static void
cnmDbdcFsmEntryFunc_WAIT_HW_DISABLE(
	IN struct ADAPTER *prAdapter
);

static void
cnmDbdcFsmEntryFunc_DISABLE_GUARD(
	IN struct ADAPTER *prAdapter
);

static void
cnmDbdcFsmEventHandler_DISABLE_IDLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_WAIT_PROTOCOL_ENABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_WAIT_HW_ENABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_ENABLE_GUARD(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_ENABLE_IDLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_WAIT_HW_DISABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_DISABLE_GUARD(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmEventHandler_WAIT_PROTOCOL_DISABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T eEvent
);

static void
cnmDbdcFsmExitFunc_WAIT_HW_ENABLE(
	IN struct ADAPTER *prAdapter
);

/*******************************************************************************
 *                           P R I V A T E   D A T A 2
 *******************************************************************************
 */
static struct DBDC_FSM_T arDdbcFsmActionTable[] = {
	/* ENUM_DBDC_FSM_STATE_DISABLE_IDLE */
	{
		cnmDbdcFsmEntryFunc_DISABLE_IDLE,
		cnmDbdcFsmEventHandler_DISABLE_IDLE,
		NULL
	},

	/* ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_ENABLE */
	{
		cnmDbdcFsmEntryFunc_WAIT_PROTOCOL_ENABLE,
		cnmDbdcFsmEventHandler_WAIT_PROTOCOL_ENABLE,
		NULL
	},

	/* ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE */
	{
		cnmDbdcFsmEntryFunc_WAIT_HW_ENABLE,
		cnmDbdcFsmEventHandler_WAIT_HW_ENABLE,
		cnmDbdcFsmExitFunc_WAIT_HW_ENABLE
	},

	/* ENUM_DBDC_FSM_STATE_ENABLE_GUARD */
	{
		cnmDbdcFsmEntryFunc_ENABLE_GUARD,
		cnmDbdcFsmEventHandler_ENABLE_GUARD,
		NULL
	},

	/* ENUM_DBDC_FSM_STATE_ENABLE_IDLE */
	{
		NULL,
		cnmDbdcFsmEventHandler_ENABLE_IDLE,
		NULL
	},

	/* ENUM_DBDC_FSM_STATE_WAIT_HW_DISABLE */
	{
		cnmDbdcFsmEntryFunc_WAIT_HW_DISABLE,
		cnmDbdcFsmEventHandler_WAIT_HW_DISABLE,
		NULL
	},

	/* ENUM_DBDC_FSM_STATE_DISABLE_GUARD */
	{
		cnmDbdcFsmEntryFunc_DISABLE_GUARD,
		cnmDbdcFsmEventHandler_DISABLE_GUARD,
		NULL
	},

	/* ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_DISABLE */
	{
		NULL,
		cnmDbdcFsmEventHandler_WAIT_PROTOCOL_DISABLE,
		NULL
	},
};

/*******************************************************************************
 *                                 M A C R O S 2
 *******************************************************************************
 */
#define DBDC_FSM_EVENT_HANDLER(_prAdapter, _event) { \
		arDdbcFsmActionTable[g_rDbdcInfo.eDbdcFsmCurrState] \
		.pfEventHandlerFunc(_prAdapter, _event); \
	}

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to initialize variables in CNM_INFO_T.
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmInit(struct ADAPTER *prAdapter)
{
	struct CNM_INFO *prCnmInfo;

	ASSERT(prAdapter);

	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->fgChGranted = FALSE;
#if CFG_SUPPORT_IDC_CH_SWITCH
	g_rLastCsaSysTime = 0;
#endif

#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
	cnmTimerInitTimer(prAdapter,
			&prAdapter->rRxCntMonitorTimer,
			(PFN_MGMT_TIMEOUT_FUNC) cnmRxCntMonitor,
			(unsigned long)NULL);

	prAdapter->fgMccFirstIn = true;

	cnmTimerStartTimer(prAdapter,
			&prAdapter->rRxCntMonitorTimer,
			SEC_TO_MSEC(MCC_CHECK_TIME));
#endif /*CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY*/
}	/* end of cnmInit()*/

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is used to initialize variables in CNM_INFO_T.
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmUninit(struct ADAPTER *prAdapter)
{
	cnmTimerStopTimer(prAdapter,
		&g_rDbdcInfo.rDbdcGuardTimer);
}	/* end of cnmUninit()*/

/*----------------------------------------------------------------------------*/
/*!
 * @brief Before handle the message from other module, it need to obtain
 *        the Channel privilege from Channel Manager
 *
 * @param[in] prMsgHdr   The message need to be handled.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmChMngrRequestPrivilege(struct ADAPTER
			       *prAdapter,
			       struct MSG_HDR *prMsgHdr)
{
	struct MSG_CH_REQ *prMsgChReq;
	struct CMD_CH_PRIVILEGE *prCmdBody;
	uint32_t rStatus;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChReq = (struct MSG_CH_REQ *)prMsgHdr;

#if CFG_SUPPORT_DBDC
	if (cnmDBDCIsReqPeivilegeLock()) {
		LINK_INSERT_TAIL(&g_rDbdcInfo.rPendingMsgList,
				 &prMsgHdr->rLinkEntry);
		log_dbg(CNM, INFO,
		       "[DBDC] ChReq: queued BSS %u Token %u REQ\n",
		       prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID);
		return;
	}
#endif

	prCmdBody = (struct CMD_CH_PRIVILEGE *)
		    cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				sizeof(struct CMD_CH_PRIVILEGE));
	ASSERT(prCmdBody);

	/* To do: exception handle */
	if (!prCmdBody) {
		log_dbg(CNM, ERROR,
		       "ChReq: fail to get buf (net=%d, token=%d)\n",
		       prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID);

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	log_dbg(CNM, INFO,
	       "ChReq net=%d token=%d b=%d c=%d s=%d w=%d s1=%d s2=%d\n",
	       prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID,
	       prMsgChReq->eRfBand, prMsgChReq->ucPrimaryChannel,
	       prMsgChReq->eRfSco, prMsgChReq->eRfChannelWidth,
	       prMsgChReq->ucRfCenterFreqSeg1,
	       prMsgChReq->ucRfCenterFreqSeg2);

	prCmdBody->ucBssIndex = prMsgChReq->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChReq->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_REQ;	/* Request */
	prCmdBody->ucPrimaryChannel =
		prMsgChReq->ucPrimaryChannel;
	prCmdBody->ucRfSco = (uint8_t)prMsgChReq->eRfSco;
	prCmdBody->ucRfBand = (uint8_t)prMsgChReq->eRfBand;
	prCmdBody->ucRfChannelWidth = (uint8_t)
				      prMsgChReq->eRfChannelWidth;
	prCmdBody->ucRfCenterFreqSeg1 = (uint8_t)
					prMsgChReq->ucRfCenterFreqSeg1;
	prCmdBody->ucRfCenterFreqSeg2 = (uint8_t)
					prMsgChReq->ucRfCenterFreqSeg2;
	prCmdBody->ucReqType = (uint8_t)prMsgChReq->eReqType;
	prCmdBody->ucDBDCBand = (uint8_t)prMsgChReq->eDBDCBand;
	prCmdBody->aucReserved = 0;
	prCmdBody->u4MaxInterval = prMsgChReq->u4MaxInterval;
	prCmdBody->aucReserved2[0] = 0;
	prCmdBody->aucReserved2[1] = 0;
	prCmdBody->aucReserved2[2] = 0;
	prCmdBody->aucReserved2[3] = 0;
	prCmdBody->aucReserved2[4] = 0;
	prCmdBody->aucReserved2[5] = 0;
	prCmdBody->aucReserved2[6] = 0;
	prCmdBody->aucReserved2[7] = 0;

	ASSERT(prCmdBody->ucBssIndex <=
	       prAdapter->ucHwBssIdNum);

	/* For monkey testing 20110901 */
	if (prCmdBody->ucBssIndex > prAdapter->ucHwBssIdNum)
		log_dbg(CNM, ERROR,
		       "CNM: ChReq with wrong netIdx=%d\n\n",
		       prCmdBody->ucBssIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_CH_PRIVILEGE,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */

				      /* u4SetQueryInfoLen */
				      sizeof(struct CMD_CH_PRIVILEGE),

				      /* pucInfoBuffer */
				      (uint8_t *)prCmdBody,

				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
				     );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);
}	/* end of cnmChMngrRequestPrivilege()*/

/*----------------------------------------------------------------------------*/
/*!
 * @brief Before deliver the message to other module, it need to release
 *        the Channel privilege to Channel Manager.
 *
 * @param[in] prMsgHdr   The message need to be delivered
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmChMngrAbortPrivilege(struct ADAPTER *prAdapter,
			     struct MSG_HDR *prMsgHdr)
{
	struct MSG_CH_ABORT *prMsgChAbort;
	struct CMD_CH_PRIVILEGE *prCmdBody;
	struct CNM_INFO *prCnmInfo;
	uint32_t rStatus;
#if CFG_SISO_SW_DEVELOP
	struct BSS_INFO *prBssInfo;
#endif
#if CFG_SUPPORT_DBDC
	struct LINK_ENTRY *prLinkEntry_pendingMsg;
	struct MSG_CH_REQ *prPendingMsg;
#endif

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChAbort = (struct MSG_CH_ABORT *)prMsgHdr;

#if CFG_SUPPORT_DBDC
	if (cnmDBDCIsReqPeivilegeLock()) {
		LINK_FOR_EACH(prLinkEntry_pendingMsg,
			      &g_rDbdcInfo.rPendingMsgList) {
			prPendingMsg = (struct MSG_CH_REQ *)
				       LINK_ENTRY(prLinkEntry_pendingMsg,
						struct MSG_HDR, rLinkEntry);

			/* Find matched request and check
			 * if it is being served.
			 */
			if (prPendingMsg->ucBssIndex == prMsgChAbort->ucBssIndex
			    && prPendingMsg->ucTokenID
				== prMsgChAbort->ucTokenID) {

				LINK_REMOVE_KNOWN_ENTRY(
					&g_rDbdcInfo.rPendingMsgList,
					&prPendingMsg->rMsgHdr.rLinkEntry);

				log_dbg(CNM, INFO, "[DBDC] ChAbort: remove BSS %u Token %u REQ)\n",
					prPendingMsg->ucBssIndex,
					prPendingMsg->ucTokenID);

				cnmMemFree(prAdapter, prPendingMsg);
				cnmMemFree(prAdapter, prMsgHdr);

				return;
			}
		}
	}
#endif

	/* Check if being granted channel privilege is aborted */
	prCnmInfo = &prAdapter->rCnmInfo;
	if (prCnmInfo->fgChGranted &&
	    prCnmInfo->ucBssIndex == prMsgChAbort->ucBssIndex
	    && prCnmInfo->ucTokenID == prMsgChAbort->ucTokenID) {

		prCnmInfo->fgChGranted = FALSE;
	}

	prCmdBody = (struct CMD_CH_PRIVILEGE *)
		    cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				sizeof(struct CMD_CH_PRIVILEGE));
	ASSERT(prCmdBody);

	/* To do: exception handle */
	if (!prCmdBody) {
		log_dbg(CNM, ERROR,
		       "ChAbort: fail to get buf (net=%d, token=%d)\n",
		       prMsgChAbort->ucBssIndex, prMsgChAbort->ucTokenID);

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prCmdBody->ucBssIndex = prMsgChAbort->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChAbort->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_ABORT;	/* Abort */
	prCmdBody->ucDBDCBand = (uint8_t)
				prMsgChAbort->eDBDCBand;

	log_dbg(CNM, INFO, "ChAbort net=%d token=%d dbdc=%u\n",
	       prCmdBody->ucBssIndex, prCmdBody->ucTokenID,
	       prCmdBody->ucDBDCBand);

	ASSERT(prCmdBody->ucBssIndex <=
	       prAdapter->ucHwBssIdNum);

	/* For monkey testing 20110901 */
	if (prCmdBody->ucBssIndex > prAdapter->ucHwBssIdNum)
		log_dbg(CNM, ERROR,
		       "CNM: ChAbort with wrong netIdx=%d\n\n",
		       prCmdBody->ucBssIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_CH_PRIVILEGE,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */

				      /* u4SetQueryInfoLen */
				      sizeof(struct CMD_CH_PRIVILEGE),

				      /* pucInfoBuffer */
				      (uint8_t *)prCmdBody,

				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
				     );

	/* ASSERT(rStatus == WLAN_STATUS_PENDING); */

#if CFG_SISO_SW_DEVELOP
	prBssInfo =
		prAdapter->aprBssInfo[prMsgChAbort->ucBssIndex];
	/* Driver clear granted CH in BSS info */
	prBssInfo->fgIsGranted = FALSE;
	prBssInfo->eBandGranted = BAND_NULL;
	prBssInfo->ucPrimaryChannelGranted = 0;
#endif

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);
}				/* end of cnmChMngrAbortPrivilege()*/

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmChMngrHandleChEvent(struct ADAPTER *prAdapter,
			    struct WIFI_EVENT *prEvent)
{
	struct EVENT_CH_PRIVILEGE *prEventBody;
	struct MSG_CH_GRANT *prChResp;
	struct BSS_INFO *prBssInfo;
	struct CNM_INFO *prCnmInfo;

	ASSERT(prAdapter);
	ASSERT(prEvent);

	prEventBody = (struct EVENT_CH_PRIVILEGE *)(
			      prEvent->aucBuffer);
	prChResp = (struct MSG_CH_GRANT *)
		   cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			       sizeof(struct MSG_CH_GRANT));
	ASSERT(prChResp);

	/* To do: exception handle */
	if (!prChResp) {
		log_dbg(CNM, ERROR,
		       "ChGrant: fail to get buf (net=%d, token=%d)\n",
		       prEventBody->ucBssIndex, prEventBody->ucTokenID);

		return;
	}

	log_dbg(CNM, INFO,
	       "ChGrant net=%d token=%d ch=%d sco=%d u4GrantInterval=%d\n",
	       prEventBody->ucBssIndex, prEventBody->ucTokenID,
	       prEventBody->ucPrimaryChannel,
	       prEventBody->ucRfSco, prEventBody->u4GrantInterval);

	ASSERT(prEventBody->ucBssIndex <=
	       prAdapter->ucHwBssIdNum);
	ASSERT(prEventBody->ucStatus == EVENT_CH_STATUS_GRANT);

	prBssInfo =
		prAdapter->aprBssInfo[prEventBody->ucBssIndex];

	/* Decide message ID based on network and response status */
	if (IS_BSS_AIS(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_AIS_CH_GRANT;
#if CFG_ENABLE_WIFI_DIRECT
	else if (prAdapter->fgIsP2PRegistered
		 && IS_BSS_P2P(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_P2P_CH_GRANT;
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	else if (IS_BSS_BOW(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_BOW_CH_GRANT;
#endif
	else {
		cnmMemFree(prAdapter, prChResp);
		return;
	}

	prChResp->ucBssIndex = prEventBody->ucBssIndex;
	prChResp->ucTokenID = prEventBody->ucTokenID;
	prChResp->ucPrimaryChannel =
		prEventBody->ucPrimaryChannel;
	prChResp->eRfSco = (enum ENUM_CHNL_EXT)
			   prEventBody->ucRfSco;
	prChResp->eRfBand = (enum ENUM_BAND)
			    prEventBody->ucRfBand;
	prChResp->eRfChannelWidth = (enum ENUM_CHANNEL_WIDTH)
				    prEventBody->ucRfChannelWidth;
	prChResp->ucRfCenterFreqSeg1 =
		prEventBody->ucRfCenterFreqSeg1;
	prChResp->ucRfCenterFreqSeg2 =
		prEventBody->ucRfCenterFreqSeg2;
	prChResp->eReqType = (enum ENUM_CH_REQ_TYPE)
			     prEventBody->ucReqType;
	prChResp->eDBDCBand = (enum ENUM_DBDC_BN)
			      prEventBody->ucDBDCBand;
	prChResp->u4GrantInterval =
		prEventBody->u4GrantInterval;

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *)prChResp, MSG_SEND_METHOD_BUF);

	/* Record current granted BSS for TXM's reference */
	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->ucBssIndex = prEventBody->ucBssIndex;
	prCnmInfo->ucTokenID = prEventBody->ucTokenID;
	prCnmInfo->fgChGranted = TRUE;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
void cnmRadarDetectEvent(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent)
{
	struct EVENT_RDD_REPORT *prEventBody;
	struct BSS_INFO *prBssInfo;
	struct MSG_P2P_RADAR_DETECT *prP2pRddDetMsg;
	uint8_t ucBssIndex;

	log_dbg(CNM, INFO, "cnmRadarDetectEvent.\n");

	prEventBody = (struct EVENT_RDD_REPORT *)(
			      prEvent->aucBuffer);

	prP2pRddDetMsg = (struct MSG_P2P_RADAR_DETECT *)
			 cnmMemAlloc(prAdapter,
				     RAM_TYPE_MSG, sizeof(*prP2pRddDetMsg));

	if (!prP2pRddDetMsg) {
		log_dbg(CNM, ERROR,
		       "cnmMemAlloc for prP2pRddDetMsg failed!\n");
		return;
	}

	prP2pRddDetMsg->rMsgHdr.eMsgId =
		MID_CNM_P2P_RADAR_DETECT;

	for (ucBssIndex = 0; ucBssIndex < BSS_DEFAULT_NUM;
	     ucBssIndex++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  ucBssIndex);

		if (prBssInfo && prBssInfo->fgIsDfsActive) {
			prP2pRddDetMsg->ucBssIndex = ucBssIndex;
			break;
		}
	}

	p2pFuncSetDfsState(DFS_STATE_DETECTED);

	p2pFuncRadarInfoInit();

	g_rP2pRadarInfo.ucRadarReportMode =
		prEventBody->ucRadarReportMode;
	g_rP2pRadarInfo.ucRddIdx = prEventBody->ucRddIdx;
	g_rP2pRadarInfo.ucLongDetected =
		prEventBody->ucLongDetected;
	g_rP2pRadarInfo.ucPeriodicDetected =
		prEventBody->ucPeriodicDetected;
	g_rP2pRadarInfo.ucLPBNum = prEventBody->ucLPBNum;
	g_rP2pRadarInfo.ucPPBNum = prEventBody->ucPPBNum;
	g_rP2pRadarInfo.ucLPBPeriodValid =
		prEventBody->ucLPBPeriodValid;
	g_rP2pRadarInfo.ucLPBWidthValid =
		prEventBody->ucLPBWidthValid;
	g_rP2pRadarInfo.ucPRICountM1 =
		prEventBody->ucPRICountM1;
	g_rP2pRadarInfo.ucPRICountM1TH =
		prEventBody->ucPRICountM1TH;
	g_rP2pRadarInfo.ucPRICountM2 =
		prEventBody->ucPRICountM2;
	g_rP2pRadarInfo.ucPRICountM2TH =
		prEventBody->ucPRICountM2TH;
	g_rP2pRadarInfo.u4PRI1stUs = prEventBody->u4PRI1stUs;
	if (prEventBody->ucLPBNum <= LPB_SIZE) {
		kalMemCopy(&g_rP2pRadarInfo.arLpbContent[0],
			   &prEventBody->arLpbContent[0],
			   prEventBody->ucLPBNum * sizeof(struct
				   LONG_PULSE_BUFFER));
	}
	if (prEventBody->ucPPBNum <= PPB_SIZE) {
		kalMemCopy(&g_rP2pRadarInfo.arPpbContent[0],
			   &prEventBody->arPpbContent[0],
			   prEventBody->ucPPBNum * sizeof(struct
				   PERIODIC_PULSE_BUFFER));
	}

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *)prP2pRddDetMsg, MSG_SEND_METHOD_BUF);
}

void cnmCsaDoneEvent(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent)
{
	struct BSS_INFO *prBssInfo;
	struct MSG_P2P_CSA_DONE *prP2pCsaDoneMsg;
	uint8_t ucBssIndex;

	DBGLOG(CNM, INFO, "cnmCsaDoneEvent.\n");

	prP2pCsaDoneMsg = (struct MSG_P2P_CSA_DONE *)
			  cnmMemAlloc(
				  prAdapter,
				  RAM_TYPE_MSG, sizeof(*prP2pCsaDoneMsg));

	if (!prP2pCsaDoneMsg) {
		log_dbg(CNM, ERROR,
		       "cnmMemAlloc for prP2pCsaDoneMsg failed!\n");
		return;
	}

	prAdapter->rWifiVar.fgCsaInProgress = FALSE;

	prP2pCsaDoneMsg->rMsgHdr.eMsgId = MID_CNM_P2P_CSA_DONE;

	for (ucBssIndex = 0; ucBssIndex < BSS_DEFAULT_NUM;
	     ucBssIndex++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  ucBssIndex);

		if (prBssInfo &&
			(prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
			prP2pCsaDoneMsg->ucBssIndex = ucBssIndex;
			break;
		}
	}
	DBGLOG(CNM, INFO, "cnmCsaDoneEvent.ucBssIndex=%d\n",
		prP2pCsaDoneMsg->ucBssIndex);

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *)prP2pCsaDoneMsg, MSG_SEND_METHOD_BUF);
}
#endif

#define CFG_SUPPORT_IDC_CROSS_BAND_SWITCH   1

#if CFG_SUPPORT_IDC_CH_SWITCH
uint8_t cnmDecideSapNewChannel(
	IN struct GLUE_INFO *prGlueInfo, uint8_t ucCurrentChannel)
{

	u_int8_t fgIsReady = FALSE;
	struct RF_CHANNEL_INFO aucChannelList2G[MAX_2G_BAND_CHN_NUM];
	struct RF_CHANNEL_INFO aucChannelList5G[MAX_5G_BAND_CHN_NUM];
	uint8_t ucNumOfChannel, i, ucIdx, ucSwitchMode;
	uint16_t u2APNumScore = 0, u2UpThreshold = 0,
		u2LowThreshold = 0, ucInnerIdx = 0;
	uint32_t u4LteSafeChnBitMask_2G  = 0, u4LteSafeChnBitMask_5G_1 = 0,
		u4LteSafeChnBitMask_5G_2 = 0;

	struct PARAM_GET_CHN_INFO *prGetChnLoad;
	struct PARAM_PREFER_CHN_INFO rPreferChannel = { 0, 0xFFFF, 0 };
	struct PARAM_PREFER_CHN_INFO
		arChannelDirtyScore_2G[MAX_2G_BAND_CHN_NUM];
	kalMemZero(arChannelDirtyScore_2G,
		sizeof(struct PARAM_PREFER_CHN_INFO)*MAX_2G_BAND_CHN_NUM);

	if (!prGlueInfo) {
		DBGLOG(P2P, ERROR, "prGlueInfo is NULL\n");
		return -EFAULT;
	}

	ASSERT(ucCurrentChannel);

	if (ucCurrentChannel <= 14)
		ucSwitchMode = CH_SWITCH_2G;
	else {
		ucSwitchMode = CH_SWITCH_5G;
		DBGLOG(P2P, WARN,
			"Switch to 5G channel instead\n");
	}
	/*
	*  Get LTE safe channels
	*/
	if (g_rLteSafeChInfo.u4Flags & BIT(0)) {
		u4LteSafeChnBitMask_2G = g_rLteSafeChInfo
			.rLteSafeChn.au4SafeChannelBitmask[0];
		u4LteSafeChnBitMask_5G_1 = g_rLteSafeChInfo
			.rLteSafeChn.au4SafeChannelBitmask[1];
		u4LteSafeChnBitMask_5G_2 = g_rLteSafeChInfo
			.rLteSafeChn.au4SafeChannelBitmask[2];
	}

	if ((ucSwitchMode == CH_SWITCH_2G)
			&& (!(u4LteSafeChnBitMask_2G & BITS(1, 14)))) {
		DBGLOG(P2P, WARN,
			"FW report 2.4G all channels unsafe!?\n");
		u4LteSafeChnBitMask_2G = BITS(1, 14);
#if CFG_SUPPORT_IDC_CROSS_BAND_SWITCH
		/* Choose 5G non-RDD Channel */
		if (u4LteSafeChnBitMask_5G_1 || u4LteSafeChnBitMask_5G_2) {
			ucSwitchMode = CH_SWITCH_5G;
			DBGLOG(P2P, WARN,
				"Switch to 5G channel instead\n");
		} else {
			/* not to switch channel*/
			return 0;
		}
#endif
	}

	if (ucSwitchMode == CH_SWITCH_2G) {
		/*
		* 1. Get 2.4G Band channel list in current regulatory domain
		*/
		rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			MAX_2G_BAND_CHN_NUM, &ucNumOfChannel, aucChannelList2G);

		fgIsReady = prGlueInfo->prAdapter->rWifiVar
				.rChnLoadInfo.fgDataReadyBit;

		if (fgIsReady == TRUE) {
			/*
			* 2. Calculate each channel's dirty score
			*/
			prGetChnLoad = &(prGlueInfo->prAdapter->rWifiVar
				.rChnLoadInfo);

			for (i = 0; i < ucNumOfChannel; i++) {
				ucIdx = aucChannelList2G[i]
					.ucChannelNum - 1;

				/* Current channel's dirty score */
				u2APNumScore =
					prGetChnLoad->rEachChnLoad[ucIdx]
					.u2APNum * CHN_DIRTY_WEIGHT_UPPERBOUND;
				u2LowThreshold = u2UpThreshold = 3;

				if (ucIdx < 3) {
					u2LowThreshold = ucIdx;
					u2UpThreshold = 3;
				} else if (ucIdx >= (ucNumOfChannel - 3)) {
					u2LowThreshold = 3;
					u2UpThreshold =
						ucNumOfChannel - (ucIdx + 1);
				}

				/* Lower channel's dirty score */
				for (ucInnerIdx = 0;
					ucInnerIdx < u2LowThreshold;
					ucInnerIdx++) {
					u2APNumScore +=
					(prGetChnLoad->rEachChnLoad
					[ucIdx - ucInnerIdx - 1].u2APNum *
					(CHN_DIRTY_WEIGHT_UPPERBOUND - 1
					- ucInnerIdx));
				}

				/* Upper channel's dirty score */
				for (ucInnerIdx = 0;
					ucInnerIdx < u2UpThreshold;
					ucInnerIdx++) {
					u2APNumScore +=
					(prGetChnLoad->rEachChnLoad
					[ucIdx + ucInnerIdx + 1].u2APNum *
					(CHN_DIRTY_WEIGHT_UPPERBOUND - 1
					- ucInnerIdx));
				}

				arChannelDirtyScore_2G[i].ucChannel =
					aucChannelList2G[i].ucChannelNum;
				arChannelDirtyScore_2G[i].u2APNumScore
					= u2APNumScore;
			}
		}

		/* 4. Find best channel, skip unsafe*/
		for (i = 0; i < ucNumOfChannel; i++) {
			if (!(u4LteSafeChnBitMask_2G
				& BIT(arChannelDirtyScore_2G[i].ucChannel)))
				continue;

			if (rPreferChannel.u2APNumScore
				>= arChannelDirtyScore_2G[i].u2APNumScore) {
				rPreferChannel.ucChannel =
					arChannelDirtyScore_2G[i].ucChannel;
				rPreferChannel.u2APNumScore =
					arChannelDirtyScore_2G[i].u2APNumScore;
			}
		}
	} else if (ucSwitchMode == CH_SWITCH_5G) {

		rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_5G, TRUE,
			MAX_5G_BAND_CHN_NUM, &ucNumOfChannel, aucChannelList5G);

		/* 4. Find best channel, skip unsafe*/
		for (i = 0; i < ucNumOfChannel; i++) {
			if ((aucChannelList5G[i].ucChannelNum >= 36)
				&& (aucChannelList5G[i].ucChannelNum <= 144)) {
				ucIdx = (aucChannelList5G[i]
					.ucChannelNum - 36) / 4;
				if (u4LteSafeChnBitMask_5G_1 & BIT(ucIdx)) {
					rPreferChannel.ucChannel =
						aucChannelList5G[i]
						.ucChannelNum;
					break;
				}
			} else if ((aucChannelList5G[i].ucChannelNum >= 149)
				&& (aucChannelList5G[i].ucChannelNum <= 181)) {
				ucIdx = (aucChannelList5G[i]
					.ucChannelNum - 149) / 4;
				if (u4LteSafeChnBitMask_5G_2 & BIT(ucIdx)) {
					rPreferChannel.ucChannel =
						aucChannelList5G[i]
						.ucChannelNum;
					break;
				}
			}
		}
	} else {
		/* Should not be here */
		DBGLOG(P2P, ERROR,
			"ERROR!! ucSwitchMode = %d\n", ucSwitchMode);
		ASSERT(0);
	}

	DBGLOG(P2P, INFO, "rPreferChannel = %d, u2APNumScore = %d\n",
		rPreferChannel.ucChannel, rPreferChannel.u2APNumScore);

	return rPreferChannel.ucChannel;

}

uint8_t cnmIdcCsaReq(IN struct ADAPTER *prAdapter,
	IN uint8_t ch_num, IN uint8_t ucRoleIdx)
{
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct BSS_INFO *prBssInfo = NULL;
	uint8_t ucBssIdx = 0;
	struct RF_CHANNEL_INFO rRfChnlInfo;
	struct MSG_P2P_SET_NEW_CHANNEL *prP2pSetNewChannelMsg =
		(struct MSG_P2P_SET_NEW_CHANNEL *) NULL;
	struct MSG_P2P_BEACON_UPDATE *prP2pBcnUpdateMsg =
		(struct MSG_P2P_BEACON_UPDATE *) NULL;

	ASSERT(ch_num);
	ASSERT(prGlueInfo);

	if (p2pFuncRoleToBssIdx(
		prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	DBGLOG(REQ, INFO,
		"[CSA]RoleIdx = %d ,CH = %d BssIdx = %d\n",
		ucRoleIdx, ch_num, ucBssIdx);

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];


	if (prBssInfo->ucPrimaryChannel != ch_num) {

		/* allocate chandef buffer to inform Kernel */
		if (prGlueInfo->prP2PInfo[ucRoleIdx]->chandef == NULL) {
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef =
				(struct cfg80211_chan_def *)
				cnmMemAlloc(prAdapter,
				RAM_TYPE_BUF, sizeof(struct cfg80211_chan_def));

			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan =
				(struct ieee80211_channel *)
				cnmMemAlloc(prAdapter,
				RAM_TYPE_BUF, sizeof(struct ieee80211_channel));
		}

		/* Build New CH Info */
		rRfChnlInfo.ucChannelNum = ch_num;
		rRfChnlInfo.eBand =
			(rRfChnlInfo.ucChannelNum <= 14)
			? BAND_2G4 : BAND_5G;

		/* Heritage BW from Old connection*/
		rRfChnlInfo.ucChnlBw = (uint8_t) CW_20_40MHZ;
		rRfChnlInfo.u2PriChnlFreq =
			nicChannelNum2Freq((uint32_t)ch_num) / 1000;
		rRfChnlInfo.u4CenterFreq1 =
			nicGetVhtS1(rRfChnlInfo.ucChannelNum,
				prBssInfo->ucVhtChannelWidth);
		rRfChnlInfo.u4CenterFreq2 = 0;

		DBGLOG(REQ, INFO,
		"[CSA]CH=%d,Band=%d,BW=%d,PriFreq=%d,S1=%d\n",
			rRfChnlInfo.ucChannelNum,
			rRfChnlInfo.eBand,
			rRfChnlInfo.ucChnlBw,
			rRfChnlInfo.u2PriChnlFreq,
			rRfChnlInfo.u4CenterFreq1);

		/* fill in chinfo to chandef */

		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan->center_freq
			= rRfChnlInfo.u2PriChnlFreq;
		prGlueInfo->prP2PInfo[ucRoleIdx]
			->chandef->center_freq1 = rRfChnlInfo.u4CenterFreq1;
		prGlueInfo->prP2PInfo[ucRoleIdx]
			->chandef->center_freq2 = rRfChnlInfo.u4CenterFreq2;

		if (rRfChnlInfo.ucChnlBw == ((uint8_t)MAX_BW_20MHZ))
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width
				= NL80211_CHAN_WIDTH_20;
		else if (rRfChnlInfo.ucChnlBw == ((uint8_t)MAX_BW_40MHZ))
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width
				= NL80211_CHAN_WIDTH_40;
		else if (rRfChnlInfo.ucChnlBw == ((uint8_t)MAX_BW_80MHZ))
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width
				= NL80211_CHAN_WIDTH_80;
		else
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width
				= NL80211_CHAN_WIDTH_20;

		if (rRfChnlInfo.eBand == BAND_5G)
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->
				chan->band = KAL_BAND_5GHZ;
		else
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->
				chan->band = KAL_BAND_2GHZ;

		/* Copy NEW CHINFO to Adapter */
		p2pFuncSetChannel(prAdapter,
			ucRoleIdx, &rRfChnlInfo);

		p2pFuncSetDfsState(DFS_STATE_INACTIVE);

		/* Set CSA IE parameters */
		prAdapter->rWifiVar.fgCsaInProgress = TRUE;
		prAdapter->rWifiVar.ucChannelSwitchMode = 1;
		prAdapter->rWifiVar.ucNewChannelNumber =
			rRfChnlInfo.ucChannelNum;
		prAdapter->rWifiVar.ucChannelSwitchCount = 5;

		/* Set new channel parameters */
		prP2pSetNewChannelMsg = (struct MSG_P2P_SET_NEW_CHANNEL *)
			cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG, sizeof(*prP2pSetNewChannelMsg));

		if (prP2pSetNewChannelMsg == NULL) {
			ASSERT(FALSE);
			return -1;
		}

		prP2pSetNewChannelMsg->rMsgHdr.eMsgId =
			MID_MNY_P2P_SET_NEW_CHANNEL;

		prP2pSetNewChannelMsg->eChannelWidth =
			(enum ENUM_CHANNEL_WIDTH) rRfChnlInfo.ucChnlBw;

		prP2pSetNewChannelMsg->ucRoleIdx = ucRoleIdx;

		prP2pSetNewChannelMsg->ucBssIndex = ucBssIdx;

		mboxSendMsg(prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prP2pSetNewChannelMsg,
			MSG_SEND_METHOD_BUF);

		/* Update beacon */
		prP2pBcnUpdateMsg = (struct MSG_P2P_BEACON_UPDATE *)
			cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG,
			sizeof(struct MSG_P2P_BEACON_UPDATE));

		if (prP2pBcnUpdateMsg == NULL) {
			ASSERT(FALSE);
			return -1;
		}

		prP2pBcnUpdateMsg->ucRoleIndex = ucRoleIdx;
		prP2pBcnUpdateMsg->rMsgHdr.eMsgId =
			MID_MNY_P2P_BEACON_UPDATE;

		prP2pBcnUpdateMsg->u4BcnHdrLen = 0;
		prP2pBcnUpdateMsg->pucBcnHdr = NULL;

		prP2pBcnUpdateMsg->u4BcnBodyLen = 0;
		prP2pBcnUpdateMsg->pucBcnBody = NULL;


		kalP2PSetRole(prGlueInfo, 2, ucRoleIdx);

		mboxSendMsg(prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prP2pBcnUpdateMsg,
			MSG_SEND_METHOD_BUF);

		/* Record Last Channel Switch Time */
		GET_CURRENT_SYSTIME(&g_rLastCsaSysTime);

		return 0; /* Return Success */

	} else {
		DBGLOG(CNM, INFO,
			"[CSA]Req CH = cur CH:%d, Stop Req\n",
			prBssInfo->ucPrimaryChannel);
		return -1;
	}
}

void cnmIdcDetectHandler(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent)
{

	struct EVENT_LTE_SAFE_CHN *prEventBody;
	uint8_t ucBssIndex, ucIdx;
	struct BSS_INFO *prBssInfo;
	uint8_t ucRoleIdx = 0;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	uint8_t ucNewChannel = 0;
	uint32_t u4Ret = 0;
	OS_SYSTIME rCurrentTime = 0;
	bool fgCsaCoolDown = FALSE;

	prEventBody = (struct EVENT_LTE_SAFE_CHN *)(
		prEvent->aucBuffer);

	g_rLteSafeChInfo.ucVersion = prEventBody->ucVersion;
	g_rLteSafeChInfo.u4Flags = prEventBody->u4Flags;

	/* Statistics from FW is valid */
	if (prEventBody->u4Flags & BIT(0)) {
		for (ucIdx = 0;
			ucIdx < NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_MAX;
				ucIdx++) {
			g_rLteSafeChInfo.rLteSafeChn.
				au4SafeChannelBitmask[ucIdx]
				= prEventBody->rLteSafeChn.
				au4SafeChannelBitmask[ucIdx];

			DBGLOG(P2P, INFO,
				"[CSA]LTE safe channels[%d]=0x%08x\n",
				ucIdx,
				prEventBody->rLteSafeChn.
				au4SafeChannelBitmask[ucIdx]);
		}
	}

	/* Only allow to switch channel once each minute*/
	GET_CURRENT_SYSTIME(&rCurrentTime);
	if ((CHECK_FOR_TIMEOUT(rCurrentTime,
			g_rLastCsaSysTime,
			SEC_TO_SYSTIME(IDC_CSA_GUARD_TIME)))
			|| (g_rLastCsaSysTime == 0)) {
		fgCsaCoolDown = TRUE;
	}

	if (!fgCsaCoolDown) {
		DBGLOG(CNM, INFO,
			"[CSA]CsaCoolDown not Finish yet,rCurrentTime=%d,g_rLastCsaSysTime=%d,IDC_CSA_GUARD_TIME=%d\n",
			rCurrentTime,
			g_rLastCsaSysTime,
			SEC_TO_SYSTIME(IDC_CSA_GUARD_TIME));
		return;
	}

	/* Choose New Ch & Start CH Swtich*/
	for (ucBssIndex = 0; ucBssIndex < BSS_DEFAULT_NUM;
		ucBssIndex++) {

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (prBssInfo &&
			(prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
			DBGLOG(CNM, INFO, "[CSA]BssIdx=%d,CurCH=%d\n",
				prBssInfo->ucBssIndex,
				prBssInfo->ucPrimaryChannel);
			ucNewChannel = cnmDecideSapNewChannel(prGlueInfo,
				prBssInfo->ucPrimaryChannel);
			if (ucNewChannel) {
				u4Ret = cnmIdcCsaReq(prAdapter, ucNewChannel,
							ucRoleIdx);
				DBGLOG(CNM, INFO, "[CSA]BssIdx=%d,NewCH=%d\n",
					ucBssIndex, ucNewChannel);
			} else {
				DBGLOG(CNM, INFO,
					"[CSA]No Safe channel,not switch CH\n");
			}
			break;
		}
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is invoked for P2P or BOW networks
 *
 * @param (none)
 *
 * @return TRUE: suggest to adopt the returned preferred channel
 *         FALSE: No suggestion. Caller should adopt its preference
 */
/*----------------------------------------------------------------------------*/
u_int8_t
cnmPreferredChannel(struct ADAPTER *prAdapter,
		    enum ENUM_BAND *prBand, uint8_t *pucPrimaryChannel,
		    enum ENUM_CHNL_EXT *prBssSCO)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;

	ASSERT(prAdapter);
	ASSERT(prBand);
	ASSERT(pucPrimaryChannel);
	ASSERT(prBssSCO);

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

		if (prBssInfo) {
			if (IS_BSS_AIS(prBssInfo)
			    && RLM_NET_PARAM_VALID(prBssInfo)) {
				*prBand = prBssInfo->eBand;
				*pucPrimaryChannel
					= prBssInfo->ucPrimaryChannel;
				*prBssSCO = prBssInfo->eBssSCO;

				return TRUE;
			}
		}
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return TRUE: available channel is limited to return value
 *         FALSE: no limited
 */
/*----------------------------------------------------------------------------*/
u_int8_t cnmAisInfraChannelFixed(struct ADAPTER
				 *prAdapter,
				 enum ENUM_BAND *prBand,
				 uint8_t *pucPrimaryChannel)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;
	struct WIFI_VAR *prWifiVar;

	ASSERT(prAdapter);

	prWifiVar = &prAdapter->rWifiVar;

	if (prWifiVar->u4ScanCtrl &
	    SCN_CTRL_DEFAULT_SCAN_CTRL) {
		/* log_dbg(CNM, INFO, "ByPass AIS channel Fix check\n");*/
		return FALSE;
	}

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

#if 0
		log_dbg(INIT, INFO,
		       "%s BSS[%u] active[%u] netType[%u]\n",
		       __func__, i, prBssInfo->fgIsNetActive,
		       prBssInfo->eNetworkType);
#endif

		if (!IS_NET_ACTIVE(prAdapter, i))
			continue;

#if CFG_ENABLE_WIFI_DIRECT
		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			u_int8_t fgFixedChannel =
				p2pFuncIsAPMode(
					prAdapter->rWifiVar.prP2PConnSettings[
						prBssInfo->u4PrivateData]);

			if (fgFixedChannel) {

				*prBand = prBssInfo->eBand;
				*pucPrimaryChannel
					= prBssInfo->ucPrimaryChannel;

				return TRUE;

			}
		}
#endif

#if CFG_ENABLE_BT_OVER_WIFI && CFG_BOW_LIMIT_AIS_CHNL
		if (prBssInfo->eNetworkType == NETWORK_TYPE_BOW) {
			*prBand = prBssInfo->eBand;
			*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;

			return TRUE;
		}
#endif

	}

	return FALSE;
}

#if CFG_SUPPORT_CHNL_CONFLICT_REVISE
u_int8_t cnmAisDetectP2PChannel(struct ADAPTER
				*prAdapter,
				enum ENUM_BAND *prBand,
				uint8_t *pucPrimaryChannel)
{
	uint8_t i = 0;
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);

#if CFG_ENABLE_WIFI_DIRECT
	for (; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
			continue;
		if (prBssInfo->eConnectionState ==
		    PARAM_MEDIA_STATE_CONNECTED ||
		    (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT
		     && prBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			*prBand = prBssInfo->eBand;
			*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;
			return TRUE;
		}
	}
#endif
	return FALSE;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmAisInfraConnectNotify(struct ADAPTER *prAdapter)
{
#if CFG_ENABLE_BT_OVER_WIFI
	struct BSS_INFO *prBssInfo, *prAisBssInfo, *prBowBssInfo;
	uint8_t i;

	ASSERT(prAdapter);

	prAisBssInfo = NULL;
	prBowBssInfo = NULL;

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)) {
			if (IS_BSS_AIS(prBssInfo))
				prAisBssInfo = prBssInfo;
			else if (IS_BSS_BOW(prBssInfo))
				prBowBssInfo = prBssInfo;
		}
	}

	if (prAisBssInfo && prBowBssInfo
	    && RLM_NET_PARAM_VALID(prAisBssInfo)
	    && RLM_NET_PARAM_VALID(prBowBssInfo)) {
		if (prAisBssInfo->eBand != prBowBssInfo->eBand ||
		    prAisBssInfo->ucPrimaryChannel !=
		    prBowBssInfo->ucPrimaryChannel) {

			/* Notify BOW to do deactivation */
			bowNotifyAllLinkDisconnected(prAdapter);
		}
	}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return TRUE: permitted
 *         FALSE: Not permitted
 */
/*----------------------------------------------------------------------------*/
u_int8_t cnmAisIbssIsPermitted(struct ADAPTER
			       *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;

	ASSERT(prAdapter);

	/* P2P device network shall be included */
	for (i = 0; i <= prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)
		    && !IS_BSS_AIS(prBssInfo))
			return FALSE;
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return TRUE: permitted
 *         FALSE: Not permitted
 */
/*----------------------------------------------------------------------------*/
u_int8_t cnmP2PIsPermitted(struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;
	u_int8_t fgBowIsActive;

	ASSERT(prAdapter);

	fgBowIsActive = FALSE;

	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS)
				return FALSE;
			else if (IS_BSS_BOW(prBssInfo))
				fgBowIsActive = TRUE;
		}
	}

#if CFG_ENABLE_BT_OVER_WIFI
	if (fgBowIsActive) {
		/* Notify BOW to do deactivation */
		bowNotifyAllLinkDisconnected(prAdapter);
	}
#endif

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return TRUE: permitted
 *         FALSE: Not permitted
 */
/*----------------------------------------------------------------------------*/
u_int8_t cnmBowIsPermitted(struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i;

	ASSERT(prAdapter);

	/* P2P device network shall be included */
	for (i = 0; i <= prAdapter->ucHwBssIdNum; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) &&
		    (IS_BSS_P2P(prBssInfo)
		     || prBssInfo->eCurrentOPMode == OP_MODE_IBSS)) {
			return FALSE;
		}
	}

	return TRUE;
}



static uint8_t cnmGetAPBwPermitted(struct ADAPTER
				   *prAdapter, uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	uint8_t ucAPBandwidth = MAX_BW_160MHZ;
	struct BSS_DESC *prBssDesc = NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *)NULL;
	uint8_t i = 0;
	uint8_t ucOffset = (MAX_BW_80MHZ - CW_80MHZ);


	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  ucBssIndex);


	if (IS_BSS_AIS(prBssInfo)) {
		/*AIS station mode*/
		prBssDesc =
			prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
	} else if (IS_BSS_P2P(prBssInfo)) {
		/* P2P mode */

		for (i = 0 ; i < BSS_P2P_NUM; i++) {

			if (!prAdapter->rWifiVar.aprP2pRoleFsmInfo[i])
				continue;

			if (prAdapter->rWifiVar.aprP2pRoleFsmInfo[i]->ucBssIndex
			    ==
			    ucBssIndex)
				break;

		}

		if (i >= BSS_P2P_NUM) {
			prP2pRoleFsmInfo = NULL;
		} else {
			prP2pRoleFsmInfo =
				prAdapter->rWifiVar.aprP2pRoleFsmInfo[i];

			/*only GC need to consider GO's BW*/
			if (!p2pFuncIsAPMode(
					prAdapter->rWifiVar.prP2PConnSettings[
						prBssInfo->u4PrivateData])) {
				prBssDesc = prP2pRoleFsmInfo->rJoinInfo
					.prTargetBssDesc;
			}

		}


	}

	if (prBssDesc) {
		if (prBssDesc->eChannelWidth == CW_20_40MHZ) {
			if ((prBssDesc->eSco == CHNL_EXT_SCA)
			    || (prBssDesc->eSco == CHNL_EXT_SCB))
				ucAPBandwidth = MAX_BW_40MHZ;
			else
				ucAPBandwidth = MAX_BW_20MHZ;
		} else {
			ucAPBandwidth = prBssDesc->eChannelWidth + ucOffset;
		}

	}

	return ucAPBandwidth;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return TRUE: permitted
 *         FALSE: Not permitted
 */
/*----------------------------------------------------------------------------*/
u_int8_t cnmBss40mBwPermitted(struct ADAPTER *prAdapter,
			      uint8_t ucBssIndex)
{
	ASSERT(prAdapter);

	/* Note: To support real-time decision instead of current
	 *       activated-time, the STA roaming case shall be considered
	 *       about synchronization problem. Another variable
	 *       fgAssoc40mBwAllowed is added to represent HT capability
	 *       when association
	 */

	/* Decide max bandwidth by feature option */
	if (cnmGetBssMaxBw(prAdapter,
			   ucBssIndex) < MAX_BW_40MHZ)
		return FALSE;

	/*check AP or GO capbility for Station or GC */
	if (cnmGetAPBwPermitted(prAdapter,
				ucBssIndex) < MAX_BW_40MHZ)
		return FALSE;
#if 0
	/* Decide max by other BSS */
	for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
		if (i != ucBssIndex) {
			prBssInfo = prAdapter->aprBssInfo[i];

			if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) &&
			    (prBssInfo->fg40mBwAllowed
			     || prBssInfo->fgAssoc40mBwAllowed))
				return FALSE;
		}
	}
#endif

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param (none)
 *
 * @return TRUE: permitted
 *         FALSE: Not permitted
 */
/*----------------------------------------------------------------------------*/
u_int8_t cnmBss80mBwPermitted(struct ADAPTER *prAdapter,
			      uint8_t ucBssIndex)
{
	ASSERT(prAdapter);

	/* Note: To support real-time decision instead of current
	 *       activated-time, the STA roaming case shall be considered
	 *       about synchronization problem. Another variable
	 *       fgAssoc40mBwAllowed is added to represent HT capability
	 *       when association
	 */

	/* Check 40Mhz first */
	if (!cnmBss40mBwPermitted(prAdapter, ucBssIndex))
		return FALSE;

	/* Decide max bandwidth by feature option */
	if (cnmGetBssMaxBw(prAdapter,
			   ucBssIndex) < MAX_BW_80MHZ)
		return FALSE;

	/*check AP or GO capbility for Station or GC */
	if (cnmGetAPBwPermitted(prAdapter,
				ucBssIndex) < MAX_BW_80MHZ)
		return FALSE;

	return TRUE;
}

uint8_t cnmGetBssMaxBw(struct ADAPTER *prAdapter,
		       uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	uint8_t ucMaxBandwidth =
		MAX_BW_80_80_MHZ; /*chip capability*/
	struct BSS_DESC *prBssDesc = NULL;
	enum ENUM_BAND eBand = BAND_NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	uint8_t ucChannelBw = MAX_BW_80_80_MHZ;
#endif

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					  ucBssIndex);

	if (IS_BSS_AIS(prBssInfo)) {
		/* STA mode */


		/* should check Bss_info could be used or not
		 *the info might not be trustable before state3
		 */

		prBssDesc =
			prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		if (prBssDesc)
			eBand = prBssDesc->eBand;
		else
			eBand = prBssInfo->eBand;


		ASSERT(eBand != BAND_NULL);

		if (eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		prP2pRoleFsmInfo = p2pFuncGetRoleByBssIdx(prAdapter,
				   ucBssIndex);
		if (!prAdapter->rWifiVar.ucApChnlDefFromCfg
		    && prP2pRoleFsmInfo
		    && prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
			ucMaxBandwidth = prP2pConnReqInfo->eChnlBw;
		} else {
			/* AP mode */
			if (p2pFuncIsAPMode(
					prAdapter->rWifiVar.prP2PConnSettings[
						prBssInfo->u4PrivateData])) {
				if (prBssInfo->eBand == BAND_2G4)
					ucMaxBandwidth = prAdapter->rWifiVar
						.ucAp2gBandwidth;
				else
					ucMaxBandwidth = prAdapter->rWifiVar
						.ucAp5gBandwidth;

				if (ucMaxBandwidth
					> prAdapter->rWifiVar.ucApBandwidth)
					ucMaxBandwidth = prAdapter->rWifiVar
						.ucApBandwidth;
			}
			/* P2P mode */
			else {
				if (prBssInfo->eBand == BAND_2G4)
					ucMaxBandwidth = prAdapter->rWifiVar
						.ucP2p2gBandwidth;
				else
					ucMaxBandwidth = prAdapter->rWifiVar
						.ucP2p5gBandwidth;
			}

		}

	}

#if (CFG_SUPPORT_SINGLE_SKU == 1)
	if (IS_BSS_AIS(prBssInfo) && prBssDesc)
		ucChannelBw = rlmDomainGetChannelBw(prBssDesc->ucChannelNum);
	else
		ucChannelBw =
			rlmDomainGetChannelBw(prBssInfo->ucPrimaryChannel);
	if (ucMaxBandwidth > ucChannelBw)
		ucMaxBandwidth = ucChannelBw;
#endif
	DBGLOG(CNM, INFO, "pCH=%d, BW=%d\n",
		prBssInfo->ucPrimaryChannel, ucMaxBandwidth);

	return ucMaxBandwidth;
}


uint8_t cnmGetBssMaxBwToChnlBW(struct ADAPTER
			       *prAdapter,
			       uint8_t ucBssIndex)
{
	uint8_t ucMaxBandwidth = cnmGetBssMaxBw(prAdapter,
						ucBssIndex);
	return ucMaxBandwidth == MAX_BW_20MHZ ? ucMaxBandwidth :
	       (ucMaxBandwidth - 1);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Search available HW ID and BSS_INFO structure and initialize
 *           these parameters, i.e., fgIsNetActive, ucBssIndex, eNetworkType
 *           and ucOwnMacIndex
 *
 * @param (none)
 *
 * @return
 */
/*----------------------------------------------------------------------------*/
struct BSS_INFO *cnmGetBssInfoAndInit(struct ADAPTER *prAdapter,
				      enum ENUM_NETWORK_TYPE eNetworkType,
				      u_int8_t fgIsP2pDevice)
{
	struct BSS_INFO *prBssInfo;
	uint8_t i, ucBssIndex, ucOwnMacIdx;

	ASSERT(prAdapter);

	/*specific case for p2p device scan*/
	if (eNetworkType == NETWORK_TYPE_P2P && fgIsP2pDevice) {
		prBssInfo =
			prAdapter->aprBssInfo[prAdapter->ucP2PDevBssIdx];

		prBssInfo->fgIsInUse = TRUE;
		prBssInfo->ucBssIndex = prAdapter->ucP2PDevBssIdx;
		prBssInfo->eNetworkType = eNetworkType;
		prBssInfo->ucOwnMacIndex = prAdapter->ucHwBssIdNum;
#if CFG_SUPPORT_PNO
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
#endif
		/* init add key action */
		prBssInfo->eKeyAction = SEC_TX_KEY_COMMAND;

		/* initialize wlan id and status for keys */
		prBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;
		prBssInfo->wepkeyWlanIdx = WTBL_RESERVED_ENTRY;
		for (i = 0; i < MAX_KEY_NUM; i++) {
			prBssInfo->ucBMCWlanIndexSUsed[i] = FALSE;
			prBssInfo->ucBMCWlanIndexS[i] = WTBL_RESERVED_ENTRY;
			prBssInfo->wepkeyUsed[i] = FALSE;
		}
		return prBssInfo;
	}

	/*reserve ownMAC0 for MBSS*/
	ucOwnMacIdx = (eNetworkType == NETWORK_TYPE_MBSS) ? 0 :
		      1;

	/* Find available HW set  with the order 1,2,..*/
	do {
		for (ucBssIndex = 0;
		     ucBssIndex < prAdapter->ucHwBssIdNum;
		     ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (prBssInfo && prBssInfo->fgIsInUse
			    && ucOwnMacIdx == prBssInfo->ucOwnMacIndex)
				break;
		}

		if (ucBssIndex >= prAdapter->ucHwBssIdNum) {
			/* No hit the ucOwnMacIndex could be
			 * assigned to this new bss
			 */
			break;
		}
	} while (++ucOwnMacIdx < prAdapter->ucHwBssIdNum);


	/* should not dispatch P2P_DEV_BSS_INDEX (prAdapter->ucHwBssIdNum)
	 * to general bss. It means total BSS_INFO_NUM BSS are created,
	 * no more reseve for MBSS
	 */
	if (ucOwnMacIdx == prAdapter->ucHwBssIdNum) {

		for (ucBssIndex = 0;
		     ucBssIndex < prAdapter->ucHwBssIdNum;
		     ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			/*If the Bss was alredy assigned, and in use*/
			if (prBssInfo && prBssInfo->fgIsInUse
			    && prBssInfo->ucOwnMacIndex == 0)
				break;
		}

		if (ucBssIndex >= prAdapter->ucHwBssIdNum) {
			/* there is no NETWORK_TYPE_MBSS used before */

			log_dbg(INIT, WARN, "[Warning] too much Bss in use, take reserve OwnMac(%d)for usage!\n",
				ucOwnMacIdx);
			ucOwnMacIdx = 0;
		}

	}

	/* Find available BSS_INFO */
	for (ucBssIndex = 0;
	     ucBssIndex < prAdapter->ucHwBssIdNum;
	     ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (prBssInfo && !prBssInfo->fgIsInUse) {
			prBssInfo->fgIsInUse = TRUE;
			prBssInfo->ucBssIndex = ucBssIndex;
			prBssInfo->eNetworkType = eNetworkType;
			prBssInfo->ucOwnMacIndex = ucOwnMacIdx;
#if (CFG_HW_WMM_BY_BSS == 1)
			prBssInfo->ucWmmQueSet = DEFAULT_HW_WMM_INDEX;
			prBssInfo->fgIsWmmInited = FALSE;
#endif
			break;
		}
	}

	if (ucOwnMacIdx >= prAdapter->ucHwBssIdNum
	    || ucBssIndex >= prAdapter->ucHwBssIdNum)
		prBssInfo = NULL;
	if (prBssInfo) {
#if CFG_SUPPORT_PNO
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
#endif
		/* init add key action */
		prBssInfo->eKeyAction = SEC_TX_KEY_COMMAND;

		/* initialize wlan id and status for keys */
		prBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;
		prBssInfo->wepkeyWlanIdx = WTBL_RESERVED_ENTRY;
		for (i = 0; i < MAX_KEY_NUM; i++) {
			prBssInfo->ucBMCWlanIndexSUsed[i] = FALSE;
			prBssInfo->ucBMCWlanIndexS[i] = WTBL_RESERVED_ENTRY;
			prBssInfo->wepkeyUsed[i] = FALSE;
		}
	}
	return prBssInfo;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Search available HW ID and BSS_INFO structure and initialize
 *           these parameters, i.e., ucBssIndex, eNetworkType and ucOwnMacIndex
 *
 * @param (none)
 *
 * @return
 */
/*----------------------------------------------------------------------------*/
void cnmFreeBssInfo(struct ADAPTER *prAdapter,
		    struct BSS_INFO *prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	prBssInfo->fgIsInUse = FALSE;
}

#if CFG_SUPPORT_DBDC
/*----------------------------------------------------------------------------*/
/*!
 * @brief    Init DBDC
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmInitDbdcSetting(IN struct ADAPTER *prAdapter)
{
	uint8_t ucBssLoopIndex;

	DBDC_SET_WMMBAND_FW_AUTO_DEFAULT();
	g_rDbdcInfo.fgHasSentCmd = FALSE;

	/* Parameter decision */
	switch (prAdapter->rWifiVar.eDbdcMode) {
	case ENUM_DBDC_MODE_DISABLED:
		cnmUpdateDbdcSetting(prAdapter, FALSE);
		break;

	case ENUM_DBDC_MODE_DYNAMIC:
		g_rDbdcInfo.eDbdcFsmCurrState =
			ENUM_DBDC_FSM_STATE_DISABLE_IDLE;
		g_rDbdcInfo.eDbdcFsmPrevState =
			ENUM_DBDC_FSM_STATE_DISABLE_IDLE;
		g_rDbdcInfo.eDbdcFsmNextState =
			ENUM_DBDC_FSM_STATE_DISABLE_IDLE;

		cnmTimerInitTimer(prAdapter,
			&g_rDbdcInfo.rDbdcGuardTimer,
			(PFN_MGMT_TIMEOUT_FUNC)cnmDbdcGuardTimerCallback,
			(unsigned long) NULL);

		g_rDbdcInfo.eDdbcGuardTimerType =
			ENUM_DBDC_GUARD_TIMER_NONE;
		g_rDbdcInfo.fgReqPrivelegeLock = FALSE;
		LINK_INITIALIZE(&g_rDbdcInfo.rPendingMsgList);
		g_rDbdcInfo.fgDbdcDisableOpmodeChangeDone = TRUE;

		for (ucBssLoopIndex = 0;
		     ucBssLoopIndex < prAdapter->ucHwBssIdNum;
		     ucBssLoopIndex++)
			g_rDbdcInfo.eBssOpModeState[ucBssLoopIndex] =
				ENUM_OPMODE_STATE_DONE;

		cnmUpdateDbdcSetting(prAdapter, FALSE);
		break;

	case ENUM_DBDC_MODE_STATIC:
		cnmUpdateDbdcSetting(prAdapter, TRUE);
		break;

	default:
		log_dbg(CNM, ERROR, "[DBDC]Incorrect DBDC mode %u\n",
		       prAdapter->rWifiVar.eDbdcMode);
		break;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Check A+G Condition
 *
 * @param (none)
 *
 * @return TRUE: A+G, FALSE: NOT A+G
 */
/*----------------------------------------------------------------------------*/
static u_int8_t cnmDbdcIsAGConcurrent(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_BAND eRfBand_Connecting)
{
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;
	enum ENUM_BAND eBandCompare = eRfBand_Connecting;
	u_int8_t fgAGConcurrent = FALSE;
	enum ENUM_BAND eBssBand[BSSID_NUM] = {BAND_NULL};

	for (ucBssIndex = 0;
		ucBssIndex < prAdapter->ucHwBssIdNum; ucBssIndex++) {

		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (DBDC_IS_BSS_NOT_ALIVE(prBssInfo))
			continue;

		if (prBssInfo->eBand != BAND_2G4
		    && prBssInfo->eBand != BAND_5G)
			continue;

		eBssBand[ucBssIndex] = prBssInfo->eBand;

		if (eBandCompare != BAND_2G4 && eBandCompare != BAND_5G)
			eBandCompare = prBssInfo->eBand;

		if (eBandCompare != prBssInfo->eBand)
			fgAGConcurrent = TRUE;	/*A+G*/
	}

	log_dbg(CNM, INFO, "[DBDC] BSS AG[%u.%u.%u.%u][%u]\n",
	       eBssBand[BSSID_0],
	       eBssBand[BSSID_1],
	       eBssBand[BSSID_2],
	       eBssBand[BSSID_3],
	       eRfBand_Connecting);

	return fgAGConcurrent;	/*NOT A+G*/
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    MT6632 HW capability will change between BW160+NSS2 and BW80+NSS1
 *
 * @param (none)
 *
 * @return TRUE: WAIT/WAIT FAIL/Done Success/Done Fail
 */
/*----------------------------------------------------------------------------*/
static enum ENUM_DBDC_PROTOCOL_STATUS_T cnmDbdcOpmodeChangeAndWait(
	IN struct ADAPTER *prAdapter,
	IN u_int8_t fgDbdcEn)
{
	uint8_t ucBssIndex;
	uint8_t ucWmmSetBitmap = 0;
	uint8_t ucOpBw;
	uint8_t ucNss;
	struct BSS_INFO *prBssInfo;
	enum ENUM_OP_CHANGE_STATUS_T eBssOpmodeChange;
	enum ENUM_DBDC_PROTOCOL_STATUS_T eRetVar =
		ENUM_DBDC_PROTOCOL_STATUS_DONE_SUCCESS;

#define IS_BSS_CLIENT(_prBssInfo) \
(_prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE)

	if (fgDbdcEn)
		ucWmmSetBitmap |= BIT(DBDC_2G_WMM_INDEX);

	for (ucBssIndex = 0;
		ucBssIndex <= prAdapter->ucHwBssIdNum; ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
		if (DBDC_IS_BSS_ALIVE(prBssInfo)) {

			ucOpBw = rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo);
			if (fgDbdcEn && ucOpBw > MAX_BW_80MHZ)
				ucOpBw = MAX_BW_80MHZ;

			ucNss = fgDbdcEn ? 1 : wlanGetSupportNss(prAdapter,
					ucBssIndex);

			eBssOpmodeChange = rlmChangeOperationMode(prAdapter,
					ucBssIndex,
					ucOpBw,
					ucNss,
					IS_BSS_CLIENT(prBssInfo) ?
					cnmDbdcOpModeChangeDoneCallback :
					NULL);

			log_dbg(CNM, INFO, "[DBDC] BSS index[%u] to BW %u NSS %u Mode:%s, status %u\n",
				ucBssIndex,
				ucOpBw,
				ucNss,
				IS_BSS_CLIENT(prBssInfo) ? "Client" : "Master",
				eBssOpmodeChange);

			switch (eBssOpmodeChange) {
			case OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_WAIT:
				g_rDbdcInfo.fgDbdcDisableOpmodeChangeDone
					= FALSE;
				g_rDbdcInfo.eBssOpModeState[ucBssIndex]
					= ENUM_OPMODE_STATE_WAIT;
				eRetVar = ENUM_DBDC_PROTOCOL_STATUS_WAIT;

				break;

			case OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_DONE:
			case OP_CHANGE_STATUS_VALID_NO_CHANGE:
				g_rDbdcInfo.eBssOpModeState[ucBssIndex]
					= ENUM_OPMODE_STATE_DONE;
				break;

			case OP_CHANGE_STATUS_INVALID:
				g_rDbdcInfo.eBssOpModeState[ucBssIndex]
					= ENUM_OPMODE_STATE_FAIL;

#define __SUCCESS__	ENUM_DBDC_PROTOCOL_STATUS_DONE_SUCCESS
#define __FAIL__	ENUM_DBDC_PROTOCOL_STATUS_DONE_FAIL
				if (eRetVar == __SUCCESS__)
					eRetVar = __FAIL__;
#undef __SUCCESS__
#undef __FAIL__

				break;

			default:
				ASSERT(0);
				break;
			}
		} else {
			g_rDbdcInfo.eBssOpModeState[ucBssIndex]
				= ENUM_OPMODE_STATE_DONE;
		}
	}

	return eRetVar;
}


void cnmDbdcOpModeChangeDoneCallback(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex,
	IN u_int8_t fgSuccess)
{
	uint8_t ucBssLoopIndex;
	u_int8_t fgIsAllActionFrameSuccess = TRUE;

	if (fgSuccess)
		g_rDbdcInfo.eBssOpModeState[ucBssIndex] =
			ENUM_OPMODE_STATE_DONE;
	else
		g_rDbdcInfo.eBssOpModeState[ucBssIndex] =
			ENUM_OPMODE_STATE_FAIL;

	log_dbg(CNM, INFO, "[DBDC] OPMODE STATE [%u/%u/%u/%u]\n",
	       g_rDbdcInfo.eBssOpModeState[BSSID_0],
	       g_rDbdcInfo.eBssOpModeState[BSSID_1],
	       g_rDbdcInfo.eBssOpModeState[BSSID_2],
	       g_rDbdcInfo.eBssOpModeState[BSSID_3]);

	for (ucBssLoopIndex = 0;
	     ucBssLoopIndex <= prAdapter->ucHwBssIdNum;
	     ucBssLoopIndex++) {

		if (g_rDbdcInfo.eBssOpModeState[ucBssIndex] ==
		    ENUM_OPMODE_STATE_WAIT)
			return;

		if (g_rDbdcInfo.eBssOpModeState[ucBssIndex] ==
		    ENUM_OPMODE_STATE_FAIL &&
		    fgIsAllActionFrameSuccess == TRUE) {
			/* Some OP mode change FAIL */
			fgIsAllActionFrameSuccess = FALSE;
		}
	}

	if (!g_rDbdcInfo.fgDbdcDisableOpmodeChangeDone) {
		if (fgIsAllActionFrameSuccess) {
			DBDC_FSM_EVENT_HANDLER(prAdapter,
				DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS);
		} else {
			DBDC_FSM_EVENT_HANDLER(prAdapter,
				DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL);
		}

		g_rDbdcInfo.fgDbdcDisableOpmodeChangeDone = TRUE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Send DBDC Enable/Disable command to FW
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmUpdateDbdcSetting(IN struct ADAPTER *prAdapter,
			  IN u_int8_t fgDbdcEn)
{
	struct CMD_DBDC_SETTING		rDbdcSetting;
	struct CMD_DBDC_SETTING *prCmdBody;
	uint32_t				rStatus = WLAN_STATUS_SUCCESS;

	log_dbg(CNM, INFO, "[DBDC] %s\n",
	       fgDbdcEn ? "Enable" : "Disable");

	/* Send event to FW */
	prCmdBody = (struct CMD_DBDC_SETTING *)&rDbdcSetting;

	kalMemZero(prCmdBody, sizeof(struct CMD_DBDC_SETTING));

	prCmdBody->ucDbdcEn = fgDbdcEn;

		/* Parameter decision */
#if (CFG_HW_WMM_BY_BSS == 1)
	if (fgDbdcEn) {
		u_int8_t ucWmmSetBitmapPerBSS;
		struct BSS_INFO *prBssInfo;
		u_int8_t ucBssIndex;
		/*
		 * As DBDC enabled, for BSS use 2.4g Band, assign related
		 * WmmGroupSet bitmask to 1.
		 * This is used to indicate the WmmGroupSet is associated
		 * to Band#1 (otherwise, use for band#0)
		 */
		for (ucBssIndex = 0; ucBssIndex < prAdapter->ucHwBssIdNum;
			ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (!prBssInfo || prBssInfo->fgIsInUse == FALSE)
				continue;

			if (prBssInfo->eBand == BAND_2G4) {
				ucWmmSetBitmapPerBSS = prBssInfo->ucWmmQueSet;
				prCmdBody->ucWmmBandBitmap |=
					BIT(ucWmmSetBitmapPerBSS);
			}
		}
		/* For P2P Device, we force it to use WMM3 */
		prBssInfo = prAdapter->aprBssInfo[P2P_DEV_BSS_INDEX];
		if (prBssInfo->eBand == BAND_2G4)
			prCmdBody->ucWmmBandBitmap |= BIT(MAX_HW_WMM_INDEX);
	}
#else
	if (fgDbdcEn)
		prCmdBody->ucWmmBandBitmap |= BIT(DBDC_2G_WMM_INDEX);
#endif

	/* FW uses ucWmmBandBitmap from driver if it does not support ver 1*/
	prCmdBody->ucCmdVer = 0x1;
	prCmdBody->u2CmdLen = sizeof(struct CMD_DBDC_SETTING);
	DBDC_UPDATE_CMD_WMMBAND_FW_AUTO(prCmdBody);

	if (g_rDbdcInfo.fgHasSentCmd == TRUE)
		log_dbg(CNM, WARN, "Not event came back for DBDC\n");

	g_rDbdcInfo.fgHasSentCmd = TRUE;
	g_rDbdcInfo.fgCmdEn = fgDbdcEn;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_SET_DBDC_PARMS,	/* ucCID */
				      TRUE, /* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL, /* pfCmdDoneHandler */
				      NULL, /* pfCmdTimeoutHandler */

				      /* u4SetQueryInfoLen */
				      sizeof(struct CMD_DBDC_SETTING),

				      /* pucInfoBuffer */
				      (uint8_t *)prCmdBody,

				      NULL, /* pvSetQueryBuffer */
				      0 /* u4SetQueryBufferLen */);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief DBDC FSM Entry
 *
 * \param[in]
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
static void
cnmDbdcFsmSteps(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_STATE_T   eNextState,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	/* Do entering Next State and do its initial function. */
	g_rDbdcInfo.eDbdcFsmPrevState = g_rDbdcInfo.eDbdcFsmCurrState;
	g_rDbdcInfo.eDbdcFsmCurrState = eNextState;
	g_rDbdcInfo.eDbdcFsmNextState = eNextState;

	log_dbg(CNM, INFO, "[DBDC] event %d state %d->%d\n",
	       eEvent,
	       g_rDbdcInfo.eDbdcFsmPrevState,
	       g_rDbdcInfo.eDbdcFsmCurrState);

	if (g_rDbdcInfo.eDbdcFsmPrevState != g_rDbdcInfo.eDbdcFsmCurrState) {
		/* state change, call exit function of previous state */
		if (arDdbcFsmActionTable[g_rDbdcInfo.eDbdcFsmPrevState]
			.pfExitFunc) {
			arDdbcFsmActionTable[g_rDbdcInfo.eDbdcFsmPrevState]
				.pfExitFunc(prAdapter);
		}

		/* state change, call entry function of current state */
		if (arDdbcFsmActionTable[g_rDbdcInfo.eDbdcFsmCurrState]
			.pfEntryFunc) {
			arDdbcFsmActionTable[g_rDbdcInfo.eDbdcFsmCurrState]
				.pfEntryFunc(prAdapter);
		}
	}
}

static u_int8_t
cnmDBDCIsReqPeivilegeLock(void)
{
	return g_rDbdcInfo.fgReqPrivelegeLock;
}

static void
cnmDBDCFsmActionReqPeivilegeLock(void)
{
	g_rDbdcInfo.fgReqPrivelegeLock = TRUE;
	log_dbg(CNM, INFO, "[DBDC] ReqPrivelege Lock!!\n");
}

static void
cnmDBDCFsmActionReqPeivilegeUnLock(IN struct ADAPTER *prAdapter)
{
	struct MSG_CH_REQ *prPendingMsg;
	struct MSG_HDR *prMsgHdr;

	g_rDbdcInfo.fgReqPrivelegeLock = FALSE;
	log_dbg(CNM, INFO, "[DBDC] ReqPrivelege Unlock!!\n");

	while (!LINK_IS_EMPTY(&g_rDbdcInfo.rPendingMsgList)) {

		LINK_REMOVE_HEAD(&g_rDbdcInfo.rPendingMsgList, prMsgHdr,
				 struct MSG_HDR *);

		if (prMsgHdr) {
			prPendingMsg = (struct MSG_CH_REQ *)prMsgHdr;

			log_dbg(CNM, INFO, "[DBDC] ChReq: send queued REQ of BSS %u Token %u\n",
				prPendingMsg->ucBssIndex,
				prPendingMsg->ucTokenID);

			cnmChMngrRequestPrivilege(prAdapter,
						  &prPendingMsg->rMsgHdr);
		} else {
			ASSERT(0);
		}
	}
}

static void
cnmDbdcFsmEntryFunc_DISABLE_IDLE(IN struct ADAPTER *prAdapter)
{
	cnmDBDCFsmActionReqPeivilegeUnLock(prAdapter);
}

static void
cnmDbdcFsmEntryFunc_WAIT_PROTOCOL_ENABLE(IN struct ADAPTER *prAdapter)
{
	cnmDBDCFsmActionReqPeivilegeLock();
}

static void
cnmDbdcFsmEntryFunc_WAIT_HW_ENABLE(IN struct ADAPTER *prAdapter)
{
	if (!cnmDBDCIsReqPeivilegeLock())
		cnmDBDCFsmActionReqPeivilegeLock();

	cnmUpdateDbdcSetting(prAdapter, TRUE);
}

static void
cnmDbdcFsmEntryFunc_ENABLE_GUARD(IN struct ADAPTER *prAdapter)
{
	if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer)) {
		log_dbg(CNM, WARN,
		       "[DBDC] Guard Timer type %u should not exist, stop it\n",
		       g_rDbdcInfo.eDdbcGuardTimerType);
		cnmTimerStopTimer(prAdapter,
				  &g_rDbdcInfo.rDbdcGuardTimer);
		g_rDbdcInfo.eDdbcGuardTimerType =
			ENUM_DBDC_GUARD_TIMER_NONE;
	}
	DBDC_SET_GUARD_TIME(prAdapter);
}

static void
cnmDbdcFsmEntryFunc_WAIT_HW_DISABLE(IN struct ADAPTER *prAdapter)
{
	cnmUpdateDbdcSetting(prAdapter, FALSE);
}

static void
cnmDbdcFsmEntryFunc_DISABLE_GUARD(IN struct ADAPTER *prAdapter)
{
	if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer)) {
		log_dbg(CNM, WARN,
		       "[DBDC] Guard Timer type %u should not exist, stop it\n",
		       g_rDbdcInfo.eDdbcGuardTimerType);
		cnmTimerStopTimer(prAdapter,
				  &g_rDbdcInfo.rDbdcGuardTimer);
		g_rDbdcInfo.eDdbcGuardTimerType =
			ENUM_DBDC_GUARD_TIMER_NONE;
	}
	DBDC_SET_GUARD_TIME(prAdapter);

	cnmDbdcOpmodeChangeAndWait(prAdapter, FALSE);
}

static void
cnmDbdcFsmEventHandler_DISABLE_IDLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T	eEvent)
{
	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
		/* Do Nothing */
		break;

	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* Enable DBDC */
		switch (cnmDbdcOpmodeChangeAndWait(prAdapter, TRUE)) {
		case ENUM_DBDC_PROTOCOL_STATUS_WAIT:
			g_rDbdcInfo.eDbdcFsmNextState =
			ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_ENABLE;
			break;

		case ENUM_DBDC_PROTOCOL_STATUS_DONE_SUCCESS:
			g_rDbdcInfo.eDbdcFsmNextState =
			ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE;
			break;

		case ENUM_DBDC_PROTOCOL_STATUS_DONE_FAIL:
			/* Should NOT FAIL, not recover anything now.
			 * Stop enable DBDC
			 */
			break;
		default:
			break;
		}
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;
	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmEventHandler_WAIT_PROTOCOL_ENABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* IGNORE */
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
		g_rDbdcInfo.eDbdcFsmNextState =
		ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE;
		break;

	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
		/* Not recover anything. Stop Enable DBDC */
		g_rDbdcInfo.eDbdcFsmNextState =
		ENUM_DBDC_FSM_STATE_DISABLE_IDLE;
		break;

	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmEventHandler_WAIT_HW_ENABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	/* Prepare to Enable DBDC */

	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* IGNORE */
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		g_rDbdcInfo.eDbdcFsmNextState =
		ENUM_DBDC_FSM_STATE_ENABLE_GUARD;
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}


static void
cnmDbdcFsmEventHandler_ENABLE_GUARD(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* IGNORE */
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
		if (cnmDbdcIsAGConcurrent(prAdapter, BAND_NULL)) {
			g_rDbdcInfo.eDbdcFsmNextState =
				ENUM_DBDC_FSM_STATE_ENABLE_IDLE;
		} else {
			g_rDbdcInfo.eDbdcFsmNextState =
				ENUM_DBDC_FSM_STATE_WAIT_HW_DISABLE;
		}
		break;

	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmEventHandler_ENABLE_IDLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
		/* start DBDC disable countdown timer */
		if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer)) {
			log_dbg(CNM, WARN, "[DBDC] Guard Timer type %u should not exist, stop it\n",
				g_rDbdcInfo.eDdbcGuardTimerType);
			cnmTimerStopTimer(prAdapter,
					  &g_rDbdcInfo.rDbdcGuardTimer);
			g_rDbdcInfo.eDdbcGuardTimerType =
			ENUM_DBDC_GUARD_TIMER_NONE;
		}
		DBDC_SET_DISABLE_COUNTDOWN(prAdapter);
		break;

	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* cancel DBDC disable countdown if exist */
		if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer) &&
		    g_rDbdcInfo.eDdbcGuardTimerType ==
		    ENUM_DBDC_GUARD_TIMER_DISABLE_COUNT_DOWN) {
			cnmTimerStopTimer(prAdapter,
					  &g_rDbdcInfo.rDbdcGuardTimer);
		}
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
		if (!cnmDbdcIsAGConcurrent(prAdapter, BAND_NULL))
			g_rDbdcInfo.eDbdcFsmNextState =
				ENUM_DBDC_FSM_STATE_WAIT_HW_DISABLE;
		break;

	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmEventHandler_WAIT_HW_DISABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* IGNORE */
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		g_rDbdcInfo.eDbdcFsmNextState =
		ENUM_DBDC_FSM_STATE_DISABLE_GUARD;
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmEventHandler_DISABLE_GUARD(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* IGNORE */
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:

#define __PRO_ENABLE__	ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_ENABLE
#define __PRO_DISABLE__	ENUM_DBDC_FSM_STATE_WAIT_PROTOCOL_DISABLE
#define __HW_ENABLE__	ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE
#define __DISABLE__	ENUM_DBDC_FSM_STATE_DISABLE_IDLE
#define __STAT_WAIT__	ENUM_DBDC_PROTOCOL_STATUS_WAIT

		if (g_rDbdcInfo.fgDbdcDisableOpmodeChangeDone) {
			if (cnmDbdcIsAGConcurrent(prAdapter, BAND_NULL)) {
				switch (cnmDbdcOpmodeChangeAndWait(
					prAdapter, TRUE)) {
				case ENUM_DBDC_PROTOCOL_STATUS_WAIT:
					g_rDbdcInfo.eDbdcFsmNextState =
						__PRO_ENABLE__;
					break;
				case ENUM_DBDC_PROTOCOL_STATUS_DONE_SUCCESS:
					g_rDbdcInfo.eDbdcFsmNextState =
						__HW_ENABLE__;
					break;
				case ENUM_DBDC_PROTOCOL_STATUS_DONE_FAIL:
					if (cnmDbdcOpmodeChangeAndWait(
						prAdapter, FALSE)
						== __STAT_WAIT__)
						g_rDbdcInfo.eDbdcFsmNextState =
							__PRO_DISABLE__;
					else
						g_rDbdcInfo.eDbdcFsmNextState =
							__DISABLE__;
					break;
				default:
					break;
				}
			} else {
				g_rDbdcInfo.eDbdcFsmNextState =
					__DISABLE__;
			}
		} else {
			g_rDbdcInfo.eDbdcFsmNextState =
				__PRO_DISABLE__;
		}

#undef __PRO_ENABLE__
#undef __PRO_DISABLE__
#undef __HW_ENABLE__
#undef __DISABLE__
#undef __STAT_WAIT__

		break;

	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
		/* ABNORMAL CASE */
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
		/* Do nothing */
		break;

	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		/* ABNORMAL CASE */
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmEventHandler_WAIT_PROTOCOL_DISABLE(
	IN struct ADAPTER *prAdapter,
	IN enum ENUM_DBDC_FSM_EVENT_T   eEvent)
{
	/* Prepare to Enable DBDC */

	switch (eEvent) {
	case DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG:
	case DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG:
		/* IGNORE */
		break;

	case DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO:
	case DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	case DBDC_FSM_EVENT_ACTION_FRAME_ALL_SUCCESS:
	case DBDC_FSM_EVENT_ACTION_FRAME_SOME_FAIL:
		g_rDbdcInfo.eDbdcFsmNextState =
		ENUM_DBDC_FSM_STATE_DISABLE_IDLE;
		break;

	case DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE:
		/* ABNORMAL CASE*/
		DBDC_FSM_MSG_WRONG_EVT(eEvent);
		break;

	default:
		/* WRONG EVENT */
		DBDC_FSM_MSG_ERROR_EVT(eEvent);
		break;
	}

	cnmDbdcFsmSteps(prAdapter, g_rDbdcInfo.eDbdcFsmNextState, eEvent);
}

static void
cnmDbdcFsmExitFunc_WAIT_HW_ENABLE(
	IN struct ADAPTER *prAdapter)
{
	cnmDBDCFsmActionReqPeivilegeUnLock(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Get the connection capability.
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmGetDbdcCapability(
	IN struct ADAPTER *prAdapter,
	IN uint8_t				ucBssIndex,
	IN enum ENUM_BAND			eRfBand,
	IN uint8_t				ucPrimaryChannel,
	IN uint8_t				ucNss,
	OUT struct CNM_DBDC_CAP *prDbdcCap)
{
	if (!prDbdcCap)
		return;

	/* BSS index */
	prDbdcCap->ucBssIndex = ucBssIndex;

#if (CFG_HW_WMM_BY_BSS == 0)
	/* WMM set */
	if (eRfBand == BAND_5G)
		prDbdcCap->ucWmmSetIndex = DBDC_5G_WMM_INDEX;
	else
		prDbdcCap->ucWmmSetIndex =
			(prAdapter->rWifiVar.eDbdcMode ==
			 ENUM_DBDC_MODE_DISABLED) ?
			DBDC_5G_WMM_INDEX : DBDC_2G_WMM_INDEX;
#endif
	/* Nss & band 0/1 */
	switch (prAdapter->rWifiVar.eDbdcMode) {
	case ENUM_DBDC_MODE_DISABLED:
		/* DBDC is disabled, all BSS run on band 0 */
		if (wlanGetSupportNss(prAdapter, ucBssIndex) < ucNss)
			prDbdcCap->ucNss = wlanGetSupportNss(prAdapter,
							     ucBssIndex);
		else
			prDbdcCap->ucNss = ucNss;
		break;

	case ENUM_DBDC_MODE_STATIC:
		/* Static DBDC mode, 1SS only */
		prDbdcCap->ucNss = 1;
		break;

	case ENUM_DBDC_MODE_DYNAMIC:
		if (USE_DBDC_CAPABILITY()) {
			prDbdcCap->ucNss = 1;
		} else {
			prDbdcCap->ucNss = wlanGetSupportNss(prAdapter,
							     ucBssIndex);
		}
		break;

	default:
		break;
	}

	log_dbg(CNM, INFO,
			"[DBDC] BSS%u RF%u CH%u Nss%u get Wmm%u Nss%u\n",
			ucBssIndex,
			eRfBand,
			ucPrimaryChannel,
			ucNss,
#if (CFG_HW_WMM_BY_BSS == 0)
			prDbdcCap->ucWmmSetIndex,
#else
			/*Wmm Selection varies from Bss instead of RfBand. */
			0xFF,
#endif
			prDbdcCap->ucNss
		);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Get maximum bandwidth capability with considering DBDC mode
 *
 * @param (none)
 *
 * @return
 */
/*----------------------------------------------------------------------------*/
uint8_t cnmGetDbdcBwCapability(IN struct ADAPTER
			       *prAdapter,
			       IN uint8_t ucBssIndex)
{
	uint8_t ucMaxBw = MAX_BW_20MHZ;

	ucMaxBw = cnmGetBssMaxBw(prAdapter, ucBssIndex);

	/* Can't use BW160 when DBDC enabled */
	if (USE_DBDC_CAPABILITY() && (ucMaxBw >= MAX_BW_160MHZ))
		ucMaxBw = MAX_BW_80MHZ;

	/* TODO: BW80+80 support */
	if (ucMaxBw == MAX_BW_80_80_MHZ)
		ucMaxBw = MAX_BW_80MHZ; /* VHT should default support BW80 */

	return ucMaxBw;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Run-time check if DBDC Need enable or update guard time.
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmDbdcEnableDecision(
	IN struct ADAPTER *prAdapter,
	IN uint8_t		ucChangedBssIndex,
	IN enum ENUM_BAND	eRfBand,
	IN uint8_t ucPrimaryChannel,
	IN uint8_t ucWmmQueIdx)
{
	log_dbg(CNM, INFO, "[DBDC] BSS %u Rf %u", ucChangedBssIndex, eRfBand);

	if (prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_DYNAMIC &&
		(prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_STATIC)) {
		log_dbg(CNM, INFO, "[DBDC Debug] DBDC Mode %u Return",
		       prAdapter->rWifiVar.eDbdcMode);
		return;
	}

	if (prAdapter->rWifiVar.fgDbDcModeEn) {
		if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer) &&
		    g_rDbdcInfo.eDdbcGuardTimerType ==
		    ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME) {
			/* update timer for connection retry */
			log_dbg(CNM, INFO, "[DBDC] DBDC guard time extend\n");
			cnmTimerStopTimer(prAdapter,
					  &g_rDbdcInfo.rDbdcGuardTimer);
			cnmTimerStartTimer(prAdapter,
					   &g_rDbdcInfo.rDbdcGuardTimer,
					   DBDC_SWITCH_GUARD_TIME);
		}
		/* The DBDC is already ON, so renew WMM band information only */
		DBDC_SET_WMMBAND_FW_AUTO_BY_CHNL(ucPrimaryChannel, ucWmmQueIdx);
		cnmUpdateDbdcSetting(prAdapter, TRUE);
		return;
	}

	if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer) &&
		g_rDbdcInfo.eDdbcGuardTimerType
		== ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME) {
		log_dbg(CNM, INFO, "[DBDC Debug] Guard Time Return");
		return;
	}

	if (eRfBand != BAND_2G4 && eRfBand != BAND_5G) {
		log_dbg(CNM, INFO, "[DBDC Debug] Wrong RF band Return");
		return;
	}

	if (cnmDbdcIsAGConcurrent(prAdapter, eRfBand)) {
		DBDC_SET_WMMBAND_FW_AUTO_BY_CHNL(ucPrimaryChannel, ucWmmQueIdx);
		DBDC_FSM_EVENT_HANDLER(prAdapter,
			DBDC_FSM_EVENT_BSS_CONNECTING_ENTER_AG);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    Run-time check if DBDC Need disable or update guard time.
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmDbdcDisableDecision(IN struct ADAPTER
			    *prAdapter,
			    IN uint8_t ucChangedBssIndex)
{
	log_dbg(CNM, INFO, "[DBDC Debug] BSS %u",
	       ucChangedBssIndex);

	if (prAdapter->rWifiVar.eDbdcMode !=
	    ENUM_DBDC_MODE_DYNAMIC) {
		log_dbg(CNM, INFO, "[DBDC Debug] DBDC Mode %u Return",
		       prAdapter->rWifiVar.eDbdcMode);
		return;
	}

	if (!prAdapter->rWifiVar.fgDbDcModeEn) {
		if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer) &&
		    g_rDbdcInfo.eDdbcGuardTimerType ==
		    ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME) {
			/* update timer for connection retry */
			log_dbg(CNM, INFO, "[DBDC] DBDC guard time extend\n");
			cnmTimerStopTimer(prAdapter,
					  &g_rDbdcInfo.rDbdcGuardTimer);
			cnmTimerStartTimer(prAdapter,
					   &g_rDbdcInfo.rDbdcGuardTimer,
					   DBDC_SWITCH_GUARD_TIME);
		}
		return;
	}

	if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer) &&
	    g_rDbdcInfo.eDdbcGuardTimerType ==
	    ENUM_DBDC_GUARD_TIMER_DISABLE_COUNT_DOWN) {
		log_dbg(CNM, INFO,
		       "[DBDC Debug] Disable Countdown Return");
		return;
	}

	if (timerPendingTimer(&g_rDbdcInfo.rDbdcGuardTimer) &&
	    g_rDbdcInfo.eDdbcGuardTimerType ==
	    ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME) {
		log_dbg(CNM, INFO, "[DBDC Debug] Guard Time Return");
		return;
	}

	if (!cnmDbdcIsAGConcurrent(prAdapter, BAND_NULL))
		DBDC_FSM_EVENT_HANDLER(prAdapter,
				       DBDC_FSM_EVENT_BSS_DISCONNECT_LEAVE_AG);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    DBDC Guard Time/Countdown Callback
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmDbdcGuardTimerCallback(IN struct ADAPTER
			       *prAdapter,
			       IN unsigned long plParamPtr)
{
	log_dbg(CNM, INFO, "[DBDC Debug] Timer %u",
	       g_rDbdcInfo.eDdbcGuardTimerType);

	if (prAdapter->rWifiVar.eDbdcMode !=
	    ENUM_DBDC_MODE_DYNAMIC) {
		log_dbg(CNM, INFO, "[DBDC Debug] DBDC Mode %u Return",
		       prAdapter->rWifiVar.eDbdcMode);
		return;
	}

	if (g_rDbdcInfo.eDdbcGuardTimerType ==
	    ENUM_DBDC_GUARD_TIMER_SWITCH_GUARD_TIME) {

		g_rDbdcInfo.eDdbcGuardTimerType =
			ENUM_DBDC_GUARD_TIMER_NONE;
		DBDC_FSM_EVENT_HANDLER(prAdapter,
				       DBDC_FSM_EVENT_SWITCH_GUARD_TIME_TO);

	} else if (g_rDbdcInfo.eDdbcGuardTimerType ==
		   ENUM_DBDC_GUARD_TIMER_DISABLE_COUNT_DOWN) {

		g_rDbdcInfo.eDdbcGuardTimerType =
			ENUM_DBDC_GUARD_TIMER_NONE;
		DBDC_FSM_EVENT_HANDLER(prAdapter,
				       DBDC_FSM_EVENT_DISABLE_COUNT_DOWN_TO);

	} else
		log_dbg(CNM, ERROR, "[DBDC] WRONG DBDC TO TYPE %u\n",
		       g_rDbdcInfo.eDdbcGuardTimerType);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief    DBDC HW Switch done event
 *
 * @param (none)
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void cnmDbdcEventHwSwitchDone(IN struct ADAPTER
			      *prAdapter,
			      IN struct WIFI_EVENT *prEvent)
{
	struct CMD_INFO *prCmdInfo;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
	u_int8_t fgDbdcEn;

	/* command response handling */
	prCmdInfo = nicGetPendingCmdInfo(prAdapter,
					 prEvent->ucSeqNum);

	if (prCmdInfo != NULL) {
		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
						    prEvent->aucBuffer);
		else if (prCmdInfo->fgIsOid)
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo->fgSetQuery,
				       0, WLAN_STATUS_SUCCESS);
		/* return prCmdInfo */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
	}

	/* Check DBDC state by FSM */
	if (g_rDbdcInfo.eDbdcFsmCurrState ==
	    ENUM_DBDC_FSM_STATE_WAIT_HW_ENABLE) {
		fgDbdcEn = TRUE;
	} else if (g_rDbdcInfo.eDbdcFsmCurrState ==
		   ENUM_DBDC_FSM_STATE_WAIT_HW_DISABLE) {
		fgDbdcEn = FALSE;
	} else if (g_rDbdcInfo.fgHasSentCmd == TRUE) {
		log_dbg(CNM, INFO,
				"[DBDC] switch event from cmd happen in state %u\n",
				g_rDbdcInfo.eDbdcFsmCurrState);
		g_rDbdcInfo.fgHasSentCmd = FALSE;
		prAdapter->rWifiVar.fgDbDcModeEn = g_rDbdcInfo.fgCmdEn;
		return;
	} else {
		log_dbg(CNM, ERROR,
		       "[DBDC] switch event happen in state %u\n",
		       g_rDbdcInfo.eDbdcFsmCurrState);
		return;
	}

	/* Change DBDC state */
	prAdapter->rWifiVar.fgDbDcModeEn = fgDbdcEn;
	for (ucBssIndex = 0;
	     ucBssIndex <= prAdapter->ucHwBssIdNum;
	     ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	}

	DBDC_FSM_EVENT_HANDLER(prAdapter,
			       DBDC_FSM_EVENT_DBDC_HW_SWITCH_DONE);
}

#endif /*CFG_SUPPORT_DBDC*/


enum ENUM_CNM_NETWORK_TYPE_T cnmGetBssNetworkType(
	struct BSS_INFO *prBssInfo)
{
	if (prBssInfo->eNetworkType == NETWORK_TYPE_AIS)
		return ENUM_CNM_NETWORK_TYPE_AIS;
	else if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE)
			return ENUM_CNM_NETWORK_TYPE_P2P_GC;
		else if (prBssInfo->eCurrentOPMode ==
			 OP_MODE_ACCESS_POINT)
			return ENUM_CNM_NETWORK_TYPE_P2P_GO;
	}
	return ENUM_CNM_NETWORK_TYPE_OTHER;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief    Search available HW WMM index.
*
* @param (none)
*
* @return
*/
/*----------------------------------------------------------------------------*/
u_int8_t cnmWmmIndexDecision(
	IN struct ADAPTER *prAdapter,
	IN struct BSS_INFO *prBssInfo)
{
#if (CFG_HW_WMM_BY_BSS == 1)

	u_int8_t ucWmmIndex;
	for (ucWmmIndex = 0; ucWmmIndex < HW_WMM_NUM; ucWmmIndex++) {
		if (prBssInfo && prBssInfo->fgIsInUse &&
			prBssInfo->fgIsWmmInited == FALSE) {
			if (!(prAdapter->ucHwWmmEnBit & BIT(ucWmmIndex))) {
				prAdapter->ucHwWmmEnBit |= BIT(ucWmmIndex);
				prBssInfo->fgIsWmmInited = TRUE;
				break;
			}
		}
	}
	return (ucWmmIndex < HW_WMM_NUM) ? ucWmmIndex : MAX_HW_WMM_INDEX;

#else
	/* Follow the same rule with cnmUpdateDbdcSetting */
	if (prBssInfo->eBand == BAND_5G)
		return DBDC_5G_WMM_INDEX;
	else
		return (prAdapter->rWifiVar.eDbdcMode ==
			 ENUM_DBDC_MODE_DISABLED) ?
			DBDC_5G_WMM_INDEX : DBDC_2G_WMM_INDEX;
#endif
}
/*----------------------------------------------------------------------------*/
/*!
* @brief    Free BSS HW WMM index.
*
* @param (none)
*
* @return None
*/
/*----------------------------------------------------------------------------*/
void cnmFreeWmmIndex(
	IN struct ADAPTER *prAdapter,
	IN struct BSS_INFO *prBssInfo)
{
#if (CFG_HW_WMM_BY_BSS == 1)
	prAdapter->ucHwWmmEnBit &= (~BIT(prBssInfo->ucWmmQueSet));
#endif
	prBssInfo->ucWmmQueSet = DEFAULT_HW_WMM_INDEX;
	prBssInfo->fgIsWmmInited = FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief    Release pending timer.
*
* @param (none)
*
* @return None
*/
/*----------------------------------------------------------------------------*/
void cnmCheckPendingTimer(IN struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prAisBssInfo;
	struct AIS_FSM_INFO *prAisFsmInfo;

	if (prAdapter == NULL)
		return;

	/* Timer 1: rJoinTimeoutTimer
	 * Driver couldn't get any CH_GRANT event of CH_REQ after resume
	 * Because pending AIS join timer should do CH_ABORT to FW.
	 * Without CH_ABORT cmd, FW CNM's FSM would keep in GRANT stage.
	 * FW's CNM couldn't service any other CH_REQ in GRANT stage.
	 * As a result, checking the timer in suspend flow.
	 */
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (timerPendingTimer(&prAisFsmInfo->rJoinTimeoutTimer)) {
		DBGLOG(CNM, STATE, "[AIS] pending rJoinTimeoutTimer\n");
		cnmTimerStopTimer(prAdapter,
			&prAisFsmInfo->rJoinTimeoutTimer);
		/* Release Channel */
		aisFsmReleaseCh(prAdapter);
		prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
	}
}

#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
/*----------------------------------------------------------------------------*/
/*!
 * @brief   Monitor the RX STA/P2P count and adjust the MCC priority.
 *
 * @param (none)
 *
 * @return None
 */
/*----------------------------------------------------------------------------*/
void cnmRxCntMonitor(
	IN struct ADAPTER *prAdapter,
	IN unsigned long plParamPtr)
{
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct BSS_INFO *prBssInfo;
	uint64_t u8StaRxDiff = 0, u8P2pRxDiff = 0;
	uint8_t ucBssIdx = 0;
	uint16_t u2AIS_ch = 0, u2P2P_ch = 0;
	uint16_t u2P2pRole = CFG_MCC_P2PGO_QUOTA_TYPE;

	for (ucBssIdx = 0; ucBssIdx < MAX_BSS_INDEX ; ucBssIdx++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);

		if (!prBssInfo)
			continue;
		if ((IS_BSS_AIS(prBssInfo)) &&
		prBssInfo->fgIsNetActive &&
		(prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)) {
			u2AIS_ch = wlanGetChannelNumberByNetwork(prAdapter,
								ucBssIdx);
		}
		if ((IS_BSS_P2P(prBssInfo)) &&
		prBssInfo->fgIsNetActive &&
		(prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)) {
			u2P2P_ch = wlanGetChannelNumberByNetwork(prAdapter,
								ucBssIdx);

			/*GC check*/
			if (!(prBssInfo->eCurrentOPMode ==
							OP_MODE_ACCESS_POINT))
				u2P2pRole = CFG_MCC_P2PGC_QUOTA_TYPE;
		}
	}
	DBGLOG(CNM, TRACE,
		"Original MCC mode: %d STA:%lu P2P:%lu STAch:%d P2Pch:%d\n",
		prAdapter->ucModeMCC, prAdapter->ucModeMCC_STA_time,
		prAdapter->ucModeMCC_P2P_time, u2AIS_ch, u2P2P_ch);

	u8StaRxDiff = RX_GET_CNT(&prAdapter->rRxCtrl,
		RX_DATA_REORDER_WITHIN_COUNT_STA) - prAdapter->u8StaRxCount;
	u8P2pRxDiff = RX_GET_CNT(&prAdapter->rRxCtrl,
		RX_DATA_REORDER_WITHIN_COUNT_P2P) - prAdapter->u8P2pRxCount;
	DBGLOG(CNM, TRACE, "STA: %llu -> %llu, diff:%llu\n",
		prAdapter->u8StaRxCount,
		RX_GET_CNT(&prAdapter->rRxCtrl,
		RX_DATA_REORDER_WITHIN_COUNT_STA),
		u8StaRxDiff);
	DBGLOG(CNM, TRACE, "P2P: %llu -> %llu, diff:%llu\n",
		prAdapter->u8P2pRxCount,
		RX_GET_CNT(&prAdapter->rRxCtrl,
		RX_DATA_REORDER_WITHIN_COUNT_P2P),
		u8P2pRxDiff);


	if (u2AIS_ch != 0 && u2P2P_ch != 0 && u2AIS_ch != u2P2P_ch &&
#if CFG_SUPPORT_DBDC
	(!prAdapter->rWifiVar.fgDbDcModeEn) &&
#endif
	(u8StaRxDiff >= 0 && u8P2pRxDiff >= 0) &&
	(prAdapter->ucModeMCC == MCC_FAIR)) {
		if (u8StaRxDiff < MCC_RX_CNT_LOW_BOUND &&
				u8P2pRxDiff > MCC_RX_CNT_HIGH_BOUND) {
			DBGLOG(CNM, TRACE,
			"Current dynamic MCC priority : P2P Favor\n");
			prAdapter->u4SetStaMccTime = MCC_TIME_3;
			prAdapter->u4SetP2pMccTime = MCC_TIME_7;
		} else if (u8P2pRxDiff < MCC_RX_CNT_LOW_BOUND &&
				u8StaRxDiff > MCC_RX_CNT_HIGH_BOUND) {
			DBGLOG(CNM, TRACE,
			"Current dynamic MCC priority : STA favor\n");
			prAdapter->u4SetStaMccTime = MCC_TIME_7;
			prAdapter->u4SetP2pMccTime = MCC_TIME_3;
		} else if (u8P2pRxDiff > MCC_RX_CNT_LOW_BOUND &&
				u8StaRxDiff > MCC_RX_CNT_HIGH_BOUND) {
			DBGLOG(CNM, TRACE,
			"Current dynamic MCC priority : Fair\n");
			prAdapter->u4SetStaMccTime = MCC_TIME_5;
			prAdapter->u4SetP2pMccTime = MCC_TIME_5;
		} else {
			if (prAdapter->fgMccFirstIn == TRUE) {
				DBGLOG(CNM, TRACE,
				"First time sets dynamic MCC priority.\n");
				DBGLOG(CNM, TRACE,
				"Current dynamic MCC priority : Fair\n");
				prAdapter->u4SetStaMccTime = MCC_TIME_5;
				prAdapter->u4SetP2pMccTime = MCC_TIME_5;
				prAdapter->fgMccFirstIn = FALSE;
			} else {
				if (prAdapter->ucModeMCC_STA_time ==
								   MCC_TIME_7)
					DBGLOG(CNM, TRACE,
				"Stay dynamic MCC priority : STA favor\n");
				else if (prAdapter->ucModeMCC_STA_time ==
								   MCC_TIME_3)
					DBGLOG(CNM, TRACE,
				"Stay dynamic MCC priority : P2P favor\n");
				else
					DBGLOG(CNM, TRACE,
				"Stay dynamic MCC priority : Fair\n");
			}
		}
	}

	if ((u2AIS_ch == 0 || u2P2P_ch == 0) &&
			prAdapter->ucModeMCC == MCC_FAIR) {
		if (prAdapter->ucModeMCC_STA_time !=
				prAdapter->ucModeMCC_P2P_time) {
			DBGLOG(CNM, TRACE,
			"Leave the dynamic MCC priority mode\n");
			prAdapter->u4SetStaMccTime = MCC_TIME_5;
			prAdapter->u4SetP2pMccTime = MCC_TIME_5;
		}
	}

	if (prAdapter->ucModeMCC == MCC_FAIR) {
		/*Set MCC Time*/
		cnmSetMccTime(prAdapter,
			CFG_MCC_AIS_QUOTA_TYPE,
			prAdapter->u4SetStaMccTime);
		cnmSetMccTime(prAdapter,
			u2P2pRole,
			prAdapter->u4SetP2pMccTime);
	}
	DBGLOG(CNM, TRACE,
	"Current MCC mode: %d STA:%lu P2P:%lu STAch:%d P2Pch:%d\n",
		prAdapter->ucModeMCC,
		prAdapter->ucModeMCC_STA_time,
		prAdapter->ucModeMCC_P2P_time,
		u2AIS_ch, u2P2P_ch);

	prAdapter->u8StaRxCount = RX_GET_CNT(&prAdapter->rRxCtrl,
				RX_DATA_REORDER_WITHIN_COUNT_STA);
	prAdapter->u8P2pRxCount = RX_GET_CNT(&prAdapter->rRxCtrl,
				RX_DATA_REORDER_WITHIN_COUNT_P2P);

	cnmTimerStartTimer(prGlueInfo->prAdapter,
			&prGlueInfo->prAdapter->rRxCntMonitorTimer,
			SEC_TO_MSEC(MCC_CHECK_TIME));
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief   Set MCC time of STA and P2P.
 *
 * @param (none)
 *
 * @return None
 */
/*----------------------------------------------------------------------------*/
void cnmSetMccTime(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucRole,
	IN uint32_t u4StayTime)
{
	struct CMD_CHIP_CONFIG rCmdChipConfig;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	char pcCommand[MCC_CMD_MAX_LEN];

	kalMemZero(&rCmdChipConfig, sizeof(rCmdChipConfig));

	if (ucRole == CFG_MCC_AIS_QUOTA_TYPE) {
		if (prAdapter->ucModeMCC_STA_time == u4StayTime)
			return;
		prAdapter->ucModeMCC_STA_time = u4StayTime;
	} else if (ucRole == CFG_MCC_P2PGC_QUOTA_TYPE) {
		if (prAdapter->ucModeMCC_P2P_time == u4StayTime)
			return;
		prAdapter->ucModeMCC_P2P_time = u4StayTime;
	} else if (ucRole == CFG_MCC_P2PGO_QUOTA_TYPE) {
		if (prAdapter->ucModeMCC_P2P_time == u4StayTime)
			return;
		prAdapter->ucModeMCC_P2P_time = u4StayTime;
	}

	snprintf(pcCommand,
		MCC_CMD_MAX_LEN,
		"mccTime %d %d", ucRole, u4StayTime);
	rCmdChipConfig.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
	rCmdChipConfig.u2MsgSize = kalStrnLen(pcCommand,
						MCC_CMD_MAX_LEN);
	kalMemCopy(rCmdChipConfig.aucCmd,
		pcCommand,
		rCmdChipConfig.u2MsgSize);

	rStatus = wlanSendSetQueryCmd(prAdapter,
						CMD_ID_CHIP_CONFIG,
						TRUE,
						FALSE,
						FALSE,
						NULL,
						NULL,
						sizeof(struct CMD_CHIP_CONFIG),
						(uint8_t *) &rCmdChipConfig,
						NULL,
						0);
}
#endif /*CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY*/
