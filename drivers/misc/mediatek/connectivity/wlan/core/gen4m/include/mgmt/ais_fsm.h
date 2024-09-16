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
#define AIS_JOIN_CH_REQUEST_INTERVAL        4000
#define AIS_SCN_DONE_TIMEOUT_SEC            15 /* 15 for 2.4G + 5G */	/* 5 */
#define AIS_SCN_REPORT_SEQ_NOT_SET          (0xFFFF)

/* Support AP Selection*/
#define AIS_BLACKLIST_TIMEOUT               15 /* seconds */

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
	AIS_STATE_OFF_CHNL_TX,
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
	uint8_t ucBssIndex;
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
	enum ENUM_CH_REQ_TYPE eReqType;
};

struct AIS_MGMT_TX_REQ_INFO {
	struct LINK rTxReqLink;
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
	u_int8_t fgDeauthLastTime;
	u_int8_t fgIsInFWKBlacklist;
};
/* end Support AP Selection */

struct AIS_FSM_INFO {
	enum ENUM_AIS_STATE ePreviousState;
	enum ENUM_AIS_STATE eCurrentState;

	u_int8_t fgIsScanning;

	u_int8_t fgIsChannelRequested;
	u_int8_t fgIsChannelGranted;

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

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	struct TIMER rSecModeChangeTimer;
#endif

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
	uint8_t ucConnTrialCountLimit;

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

struct AIS_OFF_CHNL_TX_REQ_INFO {
	struct LINK_ENTRY rLinkEntry;
	struct MSDU_INFO *prMgmtTxMsdu;
	u_int8_t fgNoneCckRate;
	struct RF_CHANNEL_INFO rChannelInfo;	/* Off channel TX. */
	enum ENUM_CHNL_EXT eChnlExt;
	/* See if driver should keep at the same channel. */
	u_int8_t fgIsWaitRsp;
	uint64_t u8Cookie; /* cookie used to match with supplicant */
	uint32_t u4Duration; /* wait time for tx request */
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
	uint8_t ucBssIndex;
};
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
#define aisChangeMediaState(_prAisBssInfo, _eNewMediaState) \
	(_prAisBssInfo->eConnectionState = (_eNewMediaState))

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void aisInitializeConnectionSettings(IN struct ADAPTER
		*prAdapter, IN struct REG_INFO *prRegInfo,
		IN uint8_t ucBssIndex);

void aisFsmInit(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void aisFsmUninit(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);

bool aisFsmIsInProcessPostpone(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

bool aisFsmIsInBeaconTimeout(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void aisFsmStateInit_JOIN(IN struct ADAPTER *prAdapter,
		struct BSS_DESC *prBssDesc, uint8_t ucBssIndex);

u_int8_t aisFsmStateInit_RetryJOIN(IN struct ADAPTER
				   *prAdapter, IN struct STA_RECORD *prStaRec,
				   uint8_t ucBssIndex);

void aisFsmStateInit_IBSS_ALONE(IN struct ADAPTER
				*prAdapter, uint8_t ucBssIndex);

void aisFsmStateInit_IBSS_MERGE(IN struct ADAPTER
	*prAdapter, struct BSS_DESC *prBssDesc, uint8_t ucBssIndex);

void aisFsmStateAbort(IN struct ADAPTER *prAdapter,
		      uint8_t ucReasonOfDisconnect, u_int8_t fgDelayIndication,
		      uint8_t ucBssIndex);

void aisFsmStateAbort_JOIN(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void aisFsmStateAbort_SCAN(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void aisFsmStateAbort_NORMAL_TR(IN struct ADAPTER
				*prAdapter, uint8_t ucBssIndex);

void aisFsmStateAbort_IBSS(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);

void aisFsmSteps(IN struct ADAPTER *prAdapter,
		 enum ENUM_AIS_STATE eNextState, uint8_t ucBssIndex);

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
void aisFsmCreateIBSS(IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);

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
void aisFsmReleaseCh(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex);

/*----------------------------------------------------------------------------*/
/* Event Indication                                                           */
/*----------------------------------------------------------------------------*/
void
aisIndicationOfMediaStateToHost(IN struct ADAPTER
				*prAdapter,
				enum ENUM_PARAM_MEDIA_STATE eConnectionState,
				u_int8_t fgDelayIndication,
				uint8_t ucBssIndex);

void aisPostponedEventOfDisconnTimeout(IN struct ADAPTER *prAdapter,
				IN uint8_t ucBssIndex);

void aisUpdateBssInfoForJOIN(IN struct ADAPTER *prAdapter,
			     struct STA_RECORD *prStaRec,
			     struct SW_RFB *prAssocRspSwRfb);

void aisUpdateBssInfoForCreateIBSS(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void aisUpdateBssInfoForMergeIBSS(IN struct ADAPTER *prAdapter,
				IN struct STA_RECORD *prStaRec);

u_int8_t aisValidateProbeReq(IN struct ADAPTER *prAdapter,
				IN struct SW_RFB *prSwRfb,
				IN uint8_t ucBssIndex,
				OUT uint32_t *pu4ControlFlags);

uint32_t
aisFsmRunEventMgmtFrameTxDone(IN struct ADAPTER *prAdapter,
			      IN struct MSDU_INFO *prMsduInfo,
			      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

/*----------------------------------------------------------------------------*/
/* Disconnection Handling                                                     */
/*----------------------------------------------------------------------------*/
void aisFsmDisconnect(IN struct ADAPTER *prAdapter,
		      IN u_int8_t fgDelayIndication,
		      IN uint8_t ucBssIndex);

/*----------------------------------------------------------------------------*/
/* Event Handling                                                             */
/*----------------------------------------------------------------------------*/
void aisBssBeaconTimeout(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

void aisBssBeaconTimeout_impl(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBcnTimeoutReason, IN uint8_t ucDisconnectReason,
	IN uint8_t ucBssIndex);

void aisBssLinkDown(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
void aisBssSecurityChanged(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);
#endif

uint32_t
aisDeauthXmitComplete(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo,
		      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);
uint32_t
aisDeauthXmitCompleteBss(IN struct ADAPTER *prAdapter,
		      IN uint8_t ucBssIndex,
		      IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

#if CFG_SUPPORT_ROAMING
void aisFsmRunEventRoamingDiscovery(
	IN struct ADAPTER *prAdapter,
	uint32_t u4ReqScan,
	uint8_t ucBssIndex);

enum ENUM_AIS_STATE aisFsmRoamingScanResultsUpdate(
				   IN struct ADAPTER *prAdapter,
				   IN uint8_t ucBssIndex);

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

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
void aisFsmRunEventSecModeChangeTimeout(IN struct ADAPTER
					*prAdapter, unsigned long ulParamPtr);
#endif

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
void aisFsmScanRequest(IN struct ADAPTER *prAdapter,
		       IN struct PARAM_SSID *prSsid, IN uint8_t *pucIe,
		       IN uint32_t u4IeLength,
		       IN uint8_t ucBssIndex);

void
aisFsmScanRequestAdv(IN struct ADAPTER *prAdapter,
		IN struct PARAM_SCAN_REQUEST_ADV *prRequestIn);

/*----------------------------------------------------------------------------*/
/* Internal State Checking                                                    */
/*----------------------------------------------------------------------------*/
u_int8_t aisFsmIsRequestPending(IN struct ADAPTER *prAdapter,
				IN enum ENUM_AIS_REQUEST_TYPE eReqType,
				IN u_int8_t bRemove,
				IN uint8_t ucBssIndex);

void aisFsmRemoveRoamingRequest(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex);

struct AIS_REQ_HDR *aisFsmGetNextRequest(IN struct ADAPTER *prAdapter,
				IN uint8_t ucBssIndex);

u_int8_t aisFsmInsertRequest(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType,
			     IN uint8_t ucBssIndex);

u_int8_t aisFsmInsertRequestToHead(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType,
			     IN uint8_t ucBssIndex);

u_int8_t aisFsmClearRequest(IN struct ADAPTER *prAdapter,
			     IN enum ENUM_AIS_REQUEST_TYPE eReqType,
			     IN uint8_t ucBssIndex);

void aisFsmFlushRequest(IN struct ADAPTER *prAdapter,
				IN uint8_t ucBssIndex);

uint32_t
aisFuncTxMgmtFrame(IN struct ADAPTER *prAdapter,
		   IN struct AIS_MGMT_TX_REQ_INFO *prMgmtTxReqInfo,
		   IN struct MSDU_INFO *prMgmtTxMsdu, IN uint64_t u8Cookie,
		   IN uint8_t ucBssIndex);

void aisFsmRunEventMgmtFrameTx(IN struct ADAPTER *prAdapter,
				IN struct MSG_HDR *prMsgHdr);

#if CFG_SUPPORT_NCHO
void aisFsmRunEventNchoActionFrameTx(IN struct ADAPTER *prAdapter,
				IN struct MSG_HDR *prMsgHdr);
#endif

void aisFuncValidateRxActionFrame(IN struct ADAPTER *prAdapter,
				IN struct SW_RFB *prSwRfb);

void aisFsmRunEventBssTransition(IN struct ADAPTER *prAdapter,
				IN struct MSG_HDR *prMsgHdr);

void aisFsmRunEventCancelTxWait(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

enum ENUM_AIS_STATE aisFsmStateSearchAction(
	IN struct ADAPTER *prAdapter, uint8_t ucBssIndex);
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
/* end Support AP Selection */

/* Support 11K */
void aisResetNeighborApList(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
#if CFG_SUPPORT_802_11K
void aisCollectNeighborAP(struct ADAPTER *prAdapter, uint8_t *pucApBuf,
			  uint16_t u2ApBufLen, uint8_t ucValidInterval,
			  uint8_t ucBssIndex);
#endif
void aisSendNeighborRequest(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
/* end Support 11K */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#define AIS_DEFAULT_INDEX (0)

struct AIS_FSM_INFO *aisGetAisFsmInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct AIS_SPECIFIC_BSS_INFO *aisGetAisSpecBssInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct BSS_TRANSITION_MGT_PARAM_T *
	aisGetBTMParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct BSS_INFO *aisGetConnectedBssInfo(
	IN struct ADAPTER *prAdapter);

struct BSS_INFO *aisGetAisBssInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct STA_RECORD *aisGetStaRecOfAP(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct BSS_DESC *aisGetTargetBssDesc(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct STA_RECORD *aisGetTargetStaRec(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

uint8_t aisGetTargetBssDescChannel(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
struct TIMER *aisGetSecModeChangeTimer(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);
#endif

struct TIMER *aisGetScanDoneTimer(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

enum ENUM_AIS_STATE aisGetCurrState(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct CONNECTION_SETTINGS *
	aisGetConnSettings(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct GL_WPA_INFO *aisGetWpaInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

u_int8_t aisGetWapiMode(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

enum ENUM_PARAM_AUTH_MODE aisGetAuthMode(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

enum ENUM_PARAM_OP_MODE aisGetOPMode(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

enum ENUM_WEP_STATUS aisGetEncStatus(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct IEEE_802_11_MIB *aisGetMib(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct ROAMING_INFO *aisGetRoamingInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct PARAM_BSSID_EX *aisGetCurrBssId(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

#if CFG_SUPPORT_PASSPOINT
struct HS20_INFO *aisGetHS20Info(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);
#endif

struct RADIO_MEASUREMENT_REQ_PARAMS *aisGetRmReqParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct RADIO_MEASUREMENT_REPORT_PARAMS *
	aisGetRmReportParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct WMM_INFO *
	aisGetWMMInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

#ifdef CFG_SUPPORT_REPLAY_DETECTION
struct GL_DETECT_REPLAY_INFO *
	aisGetDetRplyInfo(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);
#endif

uint8_t *
	aisGetFsmState(
	IN enum ENUM_AIS_STATE);

struct FT_IES *
	aisGetFtIe(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

struct cfg80211_ft_event_params *
	aisGetFtEventParam(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex);

#endif /* _AIS_FSM_H */
