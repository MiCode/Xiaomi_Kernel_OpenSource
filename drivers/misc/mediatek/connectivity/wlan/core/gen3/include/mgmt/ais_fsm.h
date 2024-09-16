/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/ais_fsm.h#2
 */

/*
 * ! \file   ais_fsm.h
 *  \brief  Declaration of functions and finite state machine for AIS Module.
 *
 * Declaration of functions and finite state machine for AIS Module.
 */

#ifndef _AIS_FSM_H
#define _AIS_FSM_H

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
#define AIS_JOIN_CH_REQUEST_INTERVAL        2000
#define AIS_SCN_DONE_TIMEOUT_SEC            30 /* 30 for 2.4G + 5G */	/* 5 */

#define AIS_AUTORN_MIN_INTERVAL				20
#define AIS_BLACKLIST_TIMEOUT               15 /* seconds */
#define AIS_WAIT_OKC_PMKID_SEC              1000 /* unit: ms */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_AIS_STATE_T {
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
	AIS_STATE_COLLECT_ESS_INFO,
	AIS_STATE_NUM
} ENUM_AIS_STATE_T;

enum _BLACK_LIST_SOURCE {
	AIS_BLACK_LIST_FROM_DRIVER = 1,
	AIS_BLACK_LIST_FROM_FWK = 2,

	AIS_BLACK_LIST_MAX = 1 << 7
};

/* reconnect level for determining if we should reconnect */
typedef enum _ENUM_RECONNECT_LEVEL_T {
	RECONNECT_LEVEL_MIN = 0,
	RECONNECT_LEVEL_ROAMING_FAIL,		/* roaming failed */
	RECONNECT_LEVEL_BEACON_TIMEOUT,		/* driver beacon timeout */
	RECONNECT_LEVEL_USER_SET,		/* user set connect or disassociate */
	RECONNECT_LEVEL_MAX
} ENUM_RECONNECT_LEVEL_T;

typedef struct _MSG_AIS_ABORT_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucReasonOfDisconnect;
	BOOLEAN fgDelayIndication;
} MSG_AIS_ABORT_T, *P_MSG_AIS_ABORT_T;

typedef struct _MSG_AIS_IBSS_PEER_FOUND_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucBssIndex;
	BOOLEAN fgIsMergeIn;	/* TRUE: Merge In, FALSE: Merge Out */
	P_STA_RECORD_T prStaRec;
} MSG_AIS_IBSS_PEER_FOUND_T, *P_MSG_AIS_IBSS_PEER_FOUND_T;

typedef enum _ENUM_AIS_REQUEST_TYPE_T {
	AIS_REQUEST_SCAN,
	AIS_REQUEST_RECONNECT,
	AIS_REQUEST_ROAMING_SEARCH,
	AIS_REQUEST_ROAMING_CONNECT,
	AIS_REQUEST_REMAIN_ON_CHANNEL,
	AIS_REQUEST_NUM
} ENUM_AIS_REQUEST_TYPE_T;

typedef struct _AIS_REQ_HDR_T {
	LINK_ENTRY_T rLinkEntry;
	ENUM_AIS_REQUEST_TYPE_T eReqType;
	/* temp save partial scan channel info */
	PUINT_8	pucChannelInfo;
} AIS_REQ_HDR_T, *P_AIS_REQ_HDR_T;

typedef struct _AIS_REQ_CHNL_INFO {
	ENUM_BAND_T eBand;
	ENUM_CHNL_EXT_T eSco;
	UINT_8 ucChannelNum;
	UINT_32 u4DurationMs;
	UINT_64 u8Cookie;
} AIS_REQ_CHNL_INFO, *P_AIS_REQ_CHNL_INFO;

typedef struct _AIS_MGMT_TX_REQ_INFO_T {
	BOOLEAN fgIsMgmtTxRequested;
	P_MSDU_INFO_T prMgmtTxMsdu;
	UINT_64 u8Cookie;
} AIS_MGMT_TX_REQ_INFO_T, *P_AIS_MGMT_TX_REQ_INFO_T;

struct AIS_BLACKLIST_ITEM {
	LINK_ENTRY_T rLinkEntry;

	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_16 u2DeauthReason;
	UINT_16 u2AuthStatus;
	UINT_8 ucCount;
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[32];
	OS_SYSTIME rAddTime;
	UINT_64 u8DisapperTime;
	UINT_8 blackListSource;
};

struct AIS_BEACON_TIMEOUT_BSS {
	LINK_ENTRY_T rLinkEntry;

	UINT_64 u8Tsf;
	UINT_64 u8AddTime;
	UINT_8 ucReserved;
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[32];
};

typedef struct _AIS_FSM_INFO_T {
	ENUM_AIS_STATE_T ePreviousState;
	ENUM_AIS_STATE_T eCurrentState;

	BOOLEAN fgTryScan;
#if CFG_SUPPORT_ABORT_SCAN
	BOOLEAN fgIsScanning;
#endif

	BOOLEAN fgIsInfraChannelFinished;
	BOOLEAN fgIsChannelRequested;
	BOOLEAN fgIsChannelGranted;

#if CFG_SUPPORT_ROAMING
	BOOLEAN fgIsRoamingScanPending;
#endif				/* CFG_SUPPORT_ROAMING */

	UINT_8 ucAvailableAuthTypes;	/* Used for AUTH_MODE_AUTO_SWITCH */

	P_BSS_DESC_T prTargetBssDesc;	/* For destination */

	P_STA_RECORD_T prTargetStaRec;	/* For JOIN Abort */

	UINT_32 u4SleepInterval;

	TIMER_T rBGScanTimer;

	TIMER_T rIbssAloneTimer;

	UINT_32 u4PostponeIndStartTime;

	TIMER_T rJoinTimeoutTimer;

	TIMER_T rChannelTimeoutTimer;

	TIMER_T rScanDoneTimer;

	TIMER_T rDeauthDoneTimer;

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	TIMER_T rSecModeChangeTimer;
#endif
	TIMER_T rWaitOkcPMKTimer;

	UINT_8 ucSeqNumOfReqMsg;
	UINT_8 ucSeqNumOfChReq;
	UINT_8 ucSeqNumOfScanReq;

	UINT_32 u4ChGrantedInterval;

	UINT_8 ucConnTrialCount;

	UINT_8 ucScanSSIDNum;
	PARAM_SSID_T arScanSSID[SCN_SSID_MAX_NUM];
	struct _PARAM_SCAN_RANDOM_MAC_ADDR_T rScanRandMacAddr;

	UINT_32 u4ScanIELength;
	UINT_8 aucScanIEBuf[MAX_IE_LENGTH];

	/* Pending Request List */
	LINK_T rPendingReqList;

	/* Join Request Timestamp */
	OS_SYSTIME rJoinReqTime;

	/* for cfg80211 REMAIN_ON_CHANNEL support */
	AIS_REQ_CHNL_INFO rChReqInfo;

	/* Mgmt tx related. */
	AIS_MGMT_TX_REQ_INFO_T rMgmtTxInfo;

	/* Packet filter for AIS module. */
	UINT_32 u4AisPacketFilter;

	/* for roaming target */
	PARAM_SSID_T rRoamingSSID;

	struct LINK_MGMT rBcnTimeout;
	UINT_8 ucJoinFailCntAfterScan;
	UINT_32 u4ScanUpdateIdx;
	BOOLEAN fgAdjChnlScanIssued;
} AIS_FSM_INFO_T, *P_AIS_FSM_INFO_T;

enum WNM_AIS_BSS_TRANSITION {
	BSS_TRANSITION_NO_MORE_ACTION,
	BSS_TRANSITION_REQ_ROAMING,
	BSS_TRANSITION_DISASSOC,
	BSS_TRANSITION_MAX_NUM
};
struct MSG_AIS_BSS_TRANSITION_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucToken;
	BOOLEAN fgNeedResponse;
	UINT_8 ucValidityInterval;
	enum WNM_AIS_BSS_TRANSITION eTransitionType;
	UINT_16 u2CandListLen;
	PUINT_8 pucCandList;
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
#define aisChangeMediaState(_prAdapter, _eNewMediaState) \
	    (_prAdapter->prAisBssInfo->eConnectionState = (_eNewMediaState))

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID aisInitializeConnectionSettings(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo);

VOID aisFsmInit(IN P_ADAPTER_T prAdapter);

VOID aisFsmUninit(IN P_ADAPTER_T prAdapter);

VOID aisFsmStateInit_JOIN(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);

BOOLEAN aisFsmStateInit_RetryJOIN(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

VOID aisFsmStateInit_IBSS_ALONE(IN P_ADAPTER_T prAdapter);

VOID aisFsmStateInit_IBSS_MERGE(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);

VOID aisFsmStateAbort(IN P_ADAPTER_T prAdapter, UINT_8 ucReasonOfDisconnect, BOOLEAN fgDelayIndication);

VOID aisFsmStateAbort_JOIN(IN P_ADAPTER_T prAdapter);

VOID aisFsmStateAbort_SCAN(IN P_ADAPTER_T prAdapter);

VOID aisFsmStateAbort_NORMAL_TR(IN P_ADAPTER_T prAdapter);

VOID aisFsmStateAbort_IBSS(IN P_ADAPTER_T prAdapter);
#if 0
VOID aisFsmSetChannelInfo(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ_V2 ScanReqMsg,
		IN ENUM_AIS_STATE_T CurrentState);
#endif

VOID aisFsmSteps(IN P_ADAPTER_T prAdapter, ENUM_AIS_STATE_T eNextState);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Handling                                                   */
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

#if CFG_SUPPORT_ABORT_SCAN
VOID AisFsmHandlePendingScan(IN P_ADAPTER_T prAdapter,
			     IN P_AIS_REQ_HDR_T prAisReq,
			     OUT ENUM_AIS_STATE_T *peNextState,
			     OUT PBOOLEAN pfgIsTransition);

VOID AisFsmGenerateScanDoneMsg(IN P_ADAPTER_T prAdapter, IN ENUM_SCAN_STATUS eScanStatus);

BOOLEAN AisFsmGetScanState(IN P_ADAPTER_T prAdapter);

VOID AisFsmSetScanState(IN P_ADAPTER_T prAdapter, IN BOOLEAN bFlag);
#endif

VOID aisFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID aisFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

enum _ENUM_AIS_STATE_T aisFsmJoinCompleteAction(IN struct _ADAPTER_T *prAdapter, IN struct _MSG_HDR_T *prMsgHdr);

VOID aisFsmRunEventFoundIBSSPeer(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID aisFsmRunEventRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID aisFsmRunEventCancelRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

/*----------------------------------------------------------------------------*/
/* Handling for Ad-Hoc Network                                                */
/*----------------------------------------------------------------------------*/
VOID aisFsmCreateIBSS(IN P_ADAPTER_T prAdapter);

VOID aisFsmMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

/*----------------------------------------------------------------------------*/
/* Handling of Incoming Mailbox Message from CNM                              */
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

/*----------------------------------------------------------------------------*/
/* Generating Outgoing Mailbox Message to CNM                                 */
/*----------------------------------------------------------------------------*/
VOID aisFsmReleaseCh(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* Event Indication                                                           */
/*----------------------------------------------------------------------------*/
VOID
aisIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter,
				ENUM_PARAM_MEDIA_STATE_T eConnectionState, BOOLEAN fgDelayIndication);

VOID aisCheckPostponedDisconnTimeout(IN P_ADAPTER_T prAdapter, P_AIS_FSM_INFO_T prAisFsmInfo);

VOID aisUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, P_SW_RFB_T prAssocRspSwRfb);

VOID aisUpdateBssInfoForCreateIBSS(IN P_ADAPTER_T prAdapter);

VOID aisUpdateBssInfoForMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

BOOLEAN aisValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags);

WLAN_STATUS
aisFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter,
			      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

/*----------------------------------------------------------------------------*/
/* Disconnection Handling                                                     */
/*----------------------------------------------------------------------------*/
VOID aisFsmDisconnect(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgDelayIndication);

/*----------------------------------------------------------------------------*/
/* Event Handling                                                             */
/*----------------------------------------------------------------------------*/
VOID aisBssBeaconTimeout(IN P_ADAPTER_T prAdapter);

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisBssSecurityChanged(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS
aisDeauthXmitComplete(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

#if CFG_SUPPORT_ROAMING
VOID aisFsmRunEventRoamingDiscovery(IN P_ADAPTER_T prAdapter, UINT_32 u4ReqScan);

ENUM_AIS_STATE_T aisFsmRoamingScanResultsUpdate(IN P_ADAPTER_T prAdapter);

VOID aisFsmRoamingDisconnectPrevAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prTargetStaRec);

VOID aisUpdateBssInfoForRoamingAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prAssocRspSwRfb);
#endif /*CFG_SUPPORT_ROAMING */

/*----------------------------------------------------------------------------*/
/* Timeout Handling                                                           */
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventBGSleepTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr);

VOID aisFsmRunEventIbssAloneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr);

VOID aisFsmRunEventJoinTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr);

VOID aisFsmRunEventChannelTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr);

VOID aisFsmRunEventDeauthTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr);

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisFsmRunEventSecModeChangeTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr);
#endif

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
VOID aisFsmScanRequest(IN P_ADAPTER_T prAdapter, IN P_PARAM_SSID_T prSsid, IN PUINT_8 pucIe, IN UINT_32 u4IeLength);

VOID
aisFsmScanRequestAdv(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSsidNum, IN P_PARAM_SSID_T prSsid,
		IN PUINT_8 pucIe, IN UINT_32 u4IeLength, IN UINT_8 ucSetChannel,
		IN struct _PARAM_SCAN_RANDOM_MAC_ADDR_T *prScanRandMacAddr);

/*----------------------------------------------------------------------------*/
/* Internal State Checking                                                    */
/*----------------------------------------------------------------------------*/
struct _AIS_FSM_INFO_T *aisGetAisFsmInfo(IN struct _ADAPTER_T *prAdapter, IN uint8_t ucBssIndex);

BOOLEAN aisFsmIsRequestPending(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType, IN BOOLEAN bRemove);

P_AIS_REQ_HDR_T aisFsmGetNextRequest(IN P_ADAPTER_T prAdapter);

BOOLEAN aisFsmInsertRequest(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType);

VOID aisFsmFlushRequest(IN P_ADAPTER_T prAdapter);

WLAN_STATUS
aisFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter,
		   IN P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo, IN P_MSDU_INFO_T prMgmtTxMsdu, IN UINT_64 u8Cookie);

VOID aisFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID aisFuncValidateRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

enum _ENUM_AIS_STATE_T aisFsmStateSearchAction(IN struct _ADAPTER_T *prAdapter, UINT_8 ucPhase);
#if defined(CFG_TEST_MGMT_FSM) && (CFG_TEST_MGMT_FSM != 0)
VOID aisTest(VOID);
#endif /* CFG_TEST_MGMT_FSM */

VOID aisRemoveBlacklistBySource(P_ADAPTER_T prAdapter, enum _BLACK_LIST_SOURCE source);
struct AIS_BLACKLIST_ITEM *aisAddBlacklist(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc,
						enum _BLACK_LIST_SOURCE source);
struct AIS_BLACKLIST_ITEM *aisAddBlacklistByBssid(P_ADAPTER_T prAdapter, UINT_8 aucBSSID[],
						enum _BLACK_LIST_SOURCE source);
VOID aisRemoveBlackList(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc, enum _BLACK_LIST_SOURCE source);
VOID aisRemoveTimeoutBlacklist(P_ADAPTER_T prAdapter);
struct AIS_BLACKLIST_ITEM *aisQueryBlackList(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);
struct AIS_BLACKLIST_ITEM *aisQueryBlackListByBssid(P_ADAPTER_T prAdapter, UINT_8 aucBSSID[]);
VOID aisRecordBeaconTimeout(P_ADAPTER_T prAdapter, P_BSS_INFO_T prAisBssInfo);
VOID aisRemoveBeaconTimeoutEntry(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);
UINT_16 aisCalculateBlackListScore(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc);
VOID aisCollectNeighborAP(P_ADAPTER_T prAdapter, PUINT_8 pucApBuf,
			  UINT_16 u2ApBufLen, UINT_8 ucValidInterval);
VOID aisRunEventChnlUtilRsp(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr);

VOID aisFsmRunEventBssTransition(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);
VOID aisFsmRunEventSetOkcPmk(IN P_ADAPTER_T prAdapter);

VOID aisSendNeighborRequest(P_ADAPTER_T prAdapter);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _AIS_FSM_H */
