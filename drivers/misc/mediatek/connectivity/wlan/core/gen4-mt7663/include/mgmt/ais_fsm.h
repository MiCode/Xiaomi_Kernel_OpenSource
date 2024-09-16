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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/
 *						include/mgmt/ais_fsm.h#2
 */

/*! \file   ais_fsm.h
 *  \brief  Declaration of functions and finite state machine for AIS Module.
 *
 *  Declaration of functions and finite state machine for AIS Module.
 */


#ifndef _AIS_FSM_H
#define _AIS_FSM_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#if CFG_DISCONN_DEBUG_FEATURE
extern struct AIS_DISCONN_INFO_T g_rDisconnInfoTemp;
extern uint8_t g_DisconnInfoIdx;
extern struct AIS_DISCONN_INFO_T *g_prDisconnInfo;
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define AIS_BG_SCAN_INTERVAL_MIN_SEC        2	/* 30 // exponential to 960 */
#define AIS_BG_SCAN_INTERVAL_MAX_SEC        2	/* 960 // 16min */

#define AIS_DELAY_TIME_OF_DISCONNECT_SEC    5	/* 10 */

#define AIS_IBSS_ALONE_TIMEOUT_SEC          20	/* seconds */

#define AIS_BEACON_TIMEOUT_COUNT_ADHOC      30
#define AIS_BEACON_TIMEOUT_COUNT_INFRA      10
#define AIS_BEACON_TIMEOUT_GUARD_TIME_SEC   1	/* Second */

#define AIS_BEACON_MAX_TIMEOUT_TU           100
#define AIS_BEACON_MIN_TIMEOUT_TU           5
#define AIS_BEACON_MAX_TIMEOUT_VALID        TRUE
#define AIS_BEACON_MIN_TIMEOUT_VALID        TRUE

#define AIS_BMC_MAX_TIMEOUT_TU              100
#define AIS_BMC_MIN_TIMEOUT_TU              5
#define AIS_BMC_MAX_TIMEOUT_VALID           TRUE
#define AIS_BMC_MIN_TIMEOUT_VALID           TRUE

#define AIS_JOIN_CH_GRANT_THRESHOLD         10
#if CFG_SUPPORT_CFG80211_AUTH
/* expand 4000 to 6000 to improve SAE connection success probability */
#define AIS_JOIN_CH_REQUEST_INTERVAL        6000
#else
#define AIS_JOIN_CH_REQUEST_INTERVAL        4000
#endif
#define AIS_SCN_DONE_TIMEOUT_SEC            15 /* 15 for 2.4G + 5G */	/* 5 */
#define AIS_SCN_REPORT_SEQ_NOT_SET          (0xFFFF)

#define AIS_WAIT_OKC_PMKID_SEC              1000 /* unit: ms */
/* Support AP Selection*/
#define AIS_BLACKLIST_TIMEOUT               15 /* seconds */

#ifdef CFG_SUPPORT_ADJUST_JOIN_CH_REQ_INTERVAL
#define AIS_JOIN_CH_REQUEST_MAX_INTERVAL    4000
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_AIS_STATE {
	AIS_STATE_IDLE = 0,
	AIS_STATE_SEARCH,
	AIS_STATE_SCAN,
	AIS_STATE_ONLINE_SCAN,
	AIS_STATE_LOOKING_FOR,
	AIS_STATE_WAIT_FOR_NEXT_SCAN,
	AIS_STATE_REQ_CHANNEL_JOIN,
	AIS_STATE_JOIN,
	AIS_STATE_JOIN_FAILURE,
	AIS_STATE_IBSS_ALONE,
	AIS_STATE_IBSS_MERGE,
	AIS_STATE_NORMAL_TR,
	AIS_STATE_DISCONNECTING,
	AIS_STATE_REQ_REMAIN_ON_CHANNEL,
	AIS_STATE_REMAIN_ON_CHANNEL,
	AIS_STATE_NUM
};

/* reconnect level for determining if we should reconnect */
enum ENUM_RECONNECT_LEVEL_T {
	RECONNECT_LEVEL_MIN = 0,
	RECONNECT_LEVEL_ROAMING_FAIL,	/* roaming failed */
	RECONNECT_LEVEL_BEACON_TIMEOUT,	/* driver beacon timeout */
	RECONNECT_LEVEL_USER_SET,	/* user set connect	 */
					/*	 or disassociate */
	RECONNECT_LEVEL_MAX
};

struct MSG_AIS_ABORT {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucReasonOfDisconnect;
	u_int8_t fgDelayIndication;
};

struct MSG_AIS_IBSS_PEER_FOUND {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucBssIndex;
	u_int8_t fgIsMergeIn;	/* TRUE: Merge In, FALSE: Merge Out */
	struct STA_RECORD *prStaRec;
};

enum ENUM_AIS_REQUEST_TYPE {
	AIS_REQUEST_SCAN,
	AIS_REQUEST_RECONNECT,
	AIS_REQUEST_ROAMING_SEARCH,
	AIS_REQUEST_ROAMING_CONNECT,
	AIS_REQUEST_REMAIN_ON_CHANNEL,
	AIS_REQUEST_NUM
};

struct AIS_REQ_HDR {
	struct LINK_ENTRY rLinkEntry;
	enum ENUM_AIS_REQUEST_TYPE eReqType;
};

struct AIS_REQ_CHNL_INFO {
	enum ENUM_BAND eBand;
	enum ENUM_CHNL_EXT eSco;
	uint8_t ucChannelNum;
	uint32_t u4DurationMs;
	uint64_t u8Cookie;
};

struct AIS_MGMT_TX_REQ_INFO {
	u_int8_t fgIsMgmtTxRequested;
	struct MSDU_INFO *prMgmtTxMsdu;
	uint64_t u8Cookie;
};

/* Support AP Selection */
struct AIS_BLACKLIST_ITEM {
	struct LINK_ENTRY rLinkEntry;

	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint16_t u2DeauthReason;
	uint16_t u2AuthStatus;
	uint8_t ucCount;
	uint8_t ucSSIDLen;
	uint8_t aucSSID[32];
	OS_SYSTIME rAddTime;
	uint32_t u4DisapperTime;
	u_int8_t fgIsInFWKBlacklist;
};
/* end Support AP Selection */

struct AIS_FSM_INFO {
	enum ENUM_AIS_STATE ePreviousState;
	enum ENUM_AIS_STATE eCurrentState;

	u_int8_t fgTryScan;

	u_int8_t fgIsScanning;

	u_int8_t fgIsInfraChannelFinished;
	u_int8_t fgIsChannelRequested;
	u_int8_t fgIsChannelGranted;

#if CFG_SUPPORT_ROAMING
	u_int8_t fgIsRoamingScanPending;
#endif				/* CFG_SUPPORT_ROAMING */

	uint8_t ucAvailableAuthTypes;	/* Used for AUTH_MODE_AUTO_SWITCH */

	struct BSS_DESC *prTargetBssDesc;	/* For destination */

	struct STA_RECORD *prTargetStaRec;	/* For JOIN Abort */

	uint32_t u4SleepInterval;

	struct TIMER rBGScanTimer;

	struct TIMER rIbssAloneTimer;

	uint32_t u4PostponeIndStartTime;

	struct TIMER rJoinTimeoutTimer;

	struct TIMER rChannelTimeoutTimer;

	struct TIMER rScanDoneTimer;

	struct TIMER rDeauthDoneTimer;

	struct TIMER rWaitOkcPMKTimer;

	struct TIMER rSecModeChangeTimer;

	uint8_t ucSeqNumOfReqMsg;
	uint8_t ucSeqNumOfChReq;
	uint8_t ucSeqNumOfScanReq;

	/* Save SeqNum for reporting scan done.
	 * In order to distinguish seq num and default value, make sure that
	 * sizeof(u2SeqNumOfScanReport) > sizeof(ucSeqNumOfScanReq).
	 * Set AIS_SCN_REPORT_SEQ_NOT_SET as default value
	 */
	uint16_t u2SeqNumOfScanReport;

	uint32_t u4ChGrantedInterval;

	uint8_t ucConnTrialCount;

	struct PARAM_SCAN_REQUEST_ADV rScanRequest;
	uint8_t aucScanIEBuf[MAX_IE_LENGTH];

	u_int8_t fgIsScanOidAborted;

	/* Pending Request List */
	struct LINK rPendingReqList;

	/* Join Request Timestamp */
	OS_SYSTIME rJoinReqTime;

	/* for cfg80211 REMAIN_ON_CHANNEL support */
	struct AIS_REQ_CHNL_INFO rChReqInfo;

	/* Mgmt tx related. */
	struct AIS_MGMT_TX_REQ_INFO rMgmtTxInfo;

	/* Packet filter for AIS module. */
	uint32_t u4AisPacketFilter;

	/* for roaming target */
	struct PARAM_SSID rRoamingSSID;

	/* Support AP Selection */
	uint8_t ucJoinFailCntAfterScan;
	/* end Support AP Selection */

	/* Scan target channel when device roaming */
	uint8_t fgTargetChnlScanIssued;
};

enum WNM_AIS_BSS_TRANSITION {
	BSS_TRANSITION_NO_MORE_ACTION,
	BSS_TRANSITION_REQ_ROAMING,
	BSS_TRANSITION_DISASSOC,
	BSS_TRANSITION_MAX_NUM
};
struct MSG_AIS_BSS_TRANSITION_T {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucToken;
	u_int8_t fgNeedResponse;
	uint8_t ucValidityInterval;
	enum WNM_AIS_BSS_TRANSITION eTransitionType;
	uint16_t u2CandListLen;
	uint8_t *pucCandList;
};

#if CFG_DISCONN_DEBUG_FEATURE
struct AIS_DISCONN_INFO_T {
	struct timeval tv;
	uint8_t ucTrigger;
	uint8_t ucDisConnReason;
	uint8_t ucBcnTimeoutReason;
	uint8_t ucDisassocReason;
	uint16_t u2DisassocSeqNum;
	struct STA_RECORD rStaRec;
	int32_t rBcnRssi;
	struct CMD_NOISE_HISTOGRAM_REPORT rNoise;
	struct PARAM_HW_WLAN_INFO rHwInfo;
	struct PARAM_GET_STA_STATISTICS rStaStatistics;
};
#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define aisChangeMediaState(_prAdapter, _eNewMediaState) \
	(_prAdapter->prAisBssInfo->eConnectionState = (_eNewMediaState))

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void aisInitializeConnectionSettings(IN struct ADAPTER
				     *prAdapter, IN struct REG_INFO *prRegInfo);

void aisFsmInit(IN struct ADAPTER *prAdapter);

void aisFsmUninit(IN struct ADAPTER *prAdapter);

void aisFsmStateInit_JOIN(IN struct ADAPTER *prAdapter,
			  struct BSS_DESC *prBssDesc);

u_int8_t aisFsmStateInit_RetryJOIN(IN struct ADAPTER
				   *prAdapter, IN struct STA_RECORD *prStaRec);

void aisFsmStateInit_IBSS_ALONE(IN struct ADAPTER
				*prAdapter);

void aisFsmStateInit_IBSS_MERGE(IN struct ADAPTER
				*prAdapter, struct BSS_DESC *prBssDesc);

void aisFsmStateAbort(IN struct ADAPTER *prAdapter,
		      uint8_t ucReasonOfDisconnect, u_int8_t fgDelayIndication);

void aisFsmStateAbort_JOIN(IN struct ADAPTER *prAdapter);

void aisFsmStateAbort_SCAN(IN struct ADAPTER *prAdapter);

void aisFsmStateAbort_NORMAL_TR(IN struct ADAPTER
				*prAdapter);

void aisFsmStateAbort_IBSS(IN struct ADAPTER *prAdapter);

void aisFsmSteps(IN struct ADAPTER *prAdapter,
		 enum ENUM_AIS_STATE eNextState);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Handling                                                   */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventScanDone(IN struct ADAPTER *prAdapter,
			    IN struct MSG_HDR *prMsgHdr);

void aisFsmRunEventAbort(IN struct ADAPTER *prAdapter,
			 IN struct MSG_HDR *prMsgHdr);

void aisFsmRunEventJoinComplete(IN struct ADAPTER
				*prAdapter, IN struct MSG_HDR *prMsgHdr);

enum ENUM_AIS_STATE aisFsmJoinCompleteAction(
	IN struct ADAPTER *prAdapter, IN struct MSG_HDR *prMsgHdr);

void aisFsmRunEventFoundIBSSPeer(IN struct ADAPTER
				 *prAdapter, IN struct MSG_HDR *prMsgHdr);

void aisFsmRunEventRemainOnChannel(IN struct ADAPTER
				   *prAdapter, IN struct MSG_HDR *prMsgHdr);

void aisFsmRunEventCancelRemainOnChannel(IN struct ADAPTER
		*prAdapter, IN struct MSG_HDR *prMsgHdr);

/*----------------------------------------------------------------------------*/
/* Handling for Ad-Hoc Network                                                */
/*----------------------------------------------------------------------------*/
void aisFsmCreateIBSS(IN struct ADAPTER *prAdapter);

void aisFsmMergeIBSS(IN struct ADAPTER *prAdapter,
		     IN struct STA_RECORD *prStaRec);

/*----------------------------------------------------------------------------*/
/* Handling of Incoming Mailbox Message from CNM                              */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventChGrant(IN struct ADAPTER *prAdapter,
			   IN struct MSG_HDR *prMsgHdr);

/*----------------------------------------------------------------------------*/
/* Generating Outgoing Mailbox Message to CNM                                 */
/*----------------------------------------------------------------------------*/
void aisFsmReleaseCh(IN struct ADAPTER *prAdapter);

/*----------------------------------------------------------------------------*/
/* Event Indication                                                           */
/*----------------------------------------------------------------------------*/
void
aisIndicationOfMediaStateToHost(IN struct ADAPTER
				*prAdapter,
				enum ENUM_PARAM_MEDIA_STATE eConnectionState,
				u_int8_t fgDelayIndication);

void aisPostponedEventOfDisconnTimeout(IN struct ADAPTER *prAdapter,
				IN struct AIS_FSM_INFO *prAisFsmInfo);

void aisUpdateBssInfoForJOIN(IN struct ADAPTER *prAdapter,
			     struct STA_RECORD *prStaRec,
			     struct SW_RFB *prAssocRspSwRfb);

void aisUpdateBssInfoForCreateIBSS(IN struct ADAPTER *prAdapter);

void aisUpdateBssInfoForMergeIBSS(IN struct ADAPTER *prAdapter,
				IN struct STA_RECORD *prStaRec);

u_int8_t aisValidateProbeReq(IN struct ADAPTER *prAdapter,
				IN struct SW_RFB *prSwRfb,
				OUT uint32_t *pu4ControlFlags);

uint32_t
aisFsmRunEventMgmtFrameTxDone(IN struct ADAPTER *prAdapter,
			      IN struct MSDU_INFO *prMsduInfo,
			      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

/*----------------------------------------------------------------------------*/
/* Disconnection Handling                                                     */
/*----------------------------------------------------------------------------*/
void aisFsmDisconnect(IN struct ADAPTER *prAdapter,
		      IN u_int8_t fgDelayIndication);

#if CFG_DISCONN_DEBUG_FEATURE
void aisCollectDisconnInfo(IN struct ADAPTER *prAdapter);
#endif
/*----------------------------------------------------------------------------*/
/* Event Handling                                                             */
/*----------------------------------------------------------------------------*/
void aisBssBeaconTimeout(IN struct ADAPTER *prAdapter);

void aisBssLinkDown(IN struct ADAPTER *prAdapter);

void aisBssSecurityChanged(IN struct ADAPTER *prAdapter);

uint32_t
aisDeauthXmitComplete(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo,
		      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

#if CFG_SUPPORT_ROAMING
void aisFsmRunEventRoamingDiscovery(IN struct ADAPTER
				    *prAdapter, uint32_t u4ReqScan);

enum ENUM_AIS_STATE aisFsmRoamingScanResultsUpdate(
				   IN struct ADAPTER *prAdapter);

void aisFsmRoamingDisconnectPrevAP(IN struct ADAPTER
				   *prAdapter,
				   IN struct STA_RECORD *prTargetStaRec);

void aisUpdateBssInfoForRoamingAP(IN struct ADAPTER
				  *prAdapter, IN struct STA_RECORD *prStaRec,
				  IN struct SW_RFB *prAssocRspSwRfb);
#endif /*CFG_SUPPORT_ROAMING */

/*----------------------------------------------------------------------------*/
/* Timeout Handling                                                           */
/*----------------------------------------------------------------------------*/
void aisFsmRunEventBGSleepTimeOut(IN struct ADAPTER
				  *prAdapter, unsigned long ulParamPtr);

void aisFsmRunEventIbssAloneTimeOut(IN struct ADAPTER
				    *prAdapter, unsigned long ulParamPtr);

void aisFsmRunEventJoinTimeout(IN struct ADAPTER *prAdapter,
			       unsigned long ulParamPtr);

void aisFsmRunEventChannelTimeout(IN struct ADAPTER
				  *prAdapter, unsigned long ulParamPtr);

void aisFsmRunEventDeauthTimeout(IN struct ADAPTER
				 *prAdapter, unsigned long ulParamPtr);

void aisFsmRunEventSecModeChangeTimeout(IN struct ADAPTER
					*prAdapter, unsigned long ulParamPtr);

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
void aisFsmScanRequest(IN struct ADAPTER *prAdapter,
		       IN struct PARAM_SSID *prSsid, IN uint8_t *pucIe,
		       IN uint32_t u4IeLength);

void
aisFsmScanRequestAdv(IN struct ADAPTER *prAdapter,
		IN struct PARAM_SCAN_REQUEST_ADV *prRequestIn);

/*----------------------------------------------------------------------------*/
/* Internal State Checking                                                    */
/*----------------------------------------------------------------------------*/
u_int8_t aisFsmIsRequestPending(IN struct ADAPTER *prAdapter,
				IN enum ENUM_AIS_REQUEST_TYPE eReqType,
				IN u_int8_t bRemove);

struct AIS_REQ_HDR *aisFsmGetNextRequest(IN struct ADAPTER *prAdapter);

u_int8_t aisFsmInsertRequest(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType);

void aisFsmFlushRequest(IN struct ADAPTER *prAdapter);

uint32_t
aisFuncTxMgmtFrame(IN struct ADAPTER *prAdapter,
		   IN struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo,
		   IN struct MSDU_INFO *prMgmtTxMsdu, IN uint64_t u8Cookie);

void aisFsmRunEventMgmtFrameTx(IN struct ADAPTER *prAdapter,
				IN struct MSG_HDR *prMsgHdr);

void aisFuncValidateRxActionFrame(IN struct ADAPTER *prAdapter,
				IN struct SW_RFB *prSwRfb);

void aisFsmRunEventSetOkcPmk(IN struct ADAPTER *prAdapter);

void aisFsmRunEventBssTransition(IN struct ADAPTER *prAdapter,
				IN struct MSG_HDR *prMsgHdr);

enum ENUM_AIS_STATE aisFsmStateSearchAction(
	IN struct ADAPTER *prAdapter, uint8_t ucPhase);
#if defined(CFG_TEST_MGMT_FSM) && (CFG_TEST_MGMT_FSM != 0)
void aisTest(void);
#endif /* CFG_TEST_MGMT_FSM */

/* Support AP Selection */
void aisRefreshFWKBlacklist(struct ADAPTER *prAdapter);
struct AIS_BLACKLIST_ITEM *aisAddBlacklist(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc);
void aisRemoveBlackList(struct ADAPTER *prAdapter, struct BSS_DESC *prBssDesc);
void aisRemoveTimeoutBlacklist(struct ADAPTER *prAdapter);
struct AIS_BLACKLIST_ITEM *aisQueryBlackList(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc);
uint16_t aisCalculateBlackListScore(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc);
/* end Support AP Selection */

/* Support 11K */
void aisResetNeighborApList(struct ADAPTER *prAdapter);
void aisCollectNeighborAP(struct ADAPTER *prAdapter, uint8_t *pucApBuf,
			  uint16_t u2ApBufLen, uint8_t ucValidInterval);
void aisSendNeighborRequest(struct ADAPTER *prAdapter);
/* end Support 11K */

void aisPreSuspendFlow(IN struct GLUE_INFO *prGlueInfo);

void aisFuncUpdateMgmtFrameRegister(
	IN struct ADAPTER *prAdapter,
	IN uint32_t u4NewPacketFilter);


/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _AIS_FSM_H */
