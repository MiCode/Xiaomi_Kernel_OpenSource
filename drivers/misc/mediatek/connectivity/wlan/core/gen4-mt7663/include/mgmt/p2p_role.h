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
#ifndef _P2P_ROLE_H
#define _P2P_ROLE_H

#define P2P_ROLE_GET_STATISTICS_TIME	    5000

enum ENUM_BUFFER_TYPE {
	ENUM_FRAME_TYPE_EXTRA_IE_BEACON,
	ENUM_FRAME_TYPE_EXTRA_IE_ASSOC_RSP,
	ENUM_FRAME_TYPE_EXTRA_IE_PROBE_RSP,
	ENUM_FRAME_TYPE_PROBE_RSP_TEMPLATE,
	ENUM_FRAME_TYPE_BEACON_TEMPLATE,
	ENUM_FRAME_IE_NUM
};

enum ENUM_HIDDEN_SSID_TYPE {
	ENUM_HIDDEN_SSID_NONE,
	ENUM_HIDDEN_SSID_ZERO_LEN,
	ENUM_HIDDEN_SSID_ZERO_CONTENT,
	ENUM_HIDDEN_SSID_NUM
};

struct P2P_BEACON_UPDATE_INFO {
	uint8_t *pucBcnHdr;
	uint32_t u4BcnHdrLen;
	uint8_t *pucBcnBody;
	uint32_t u4BcnBodyLen;
};

struct P2P_PROBE_RSP_UPDATE_INFO {
	struct MSDU_INFO *prProbeRspMsduTemplate;
};

struct P2P_ASSOC_RSP_UPDATE_INFO {
	uint8_t *pucAssocRspExtIE;
	uint16_t u2AssocIELen;
};

struct AP_CRYPTO_SETTINGS {
	uint32_t u4WpaVersion;
	uint32_t u4CipherGroup;
	int32_t i4NumOfCiphers;
	uint32_t aucCiphersPairwise[5];
	int32_t i4NumOfAkmSuites;
	uint32_t aucAkmSuites[2];
	u_int8_t fgIsControlPort;
	uint16_t u2ControlPortBE;
	u_int8_t fgIsControlPortEncrypt;
};

/* ////////////////////////// Message ////////////////////////////////// */

struct MSG_P2P_BEACON_UPDATE {
	struct MSG_HDR rMsgHdr;
	uint8_t ucRoleIndex;
	uint32_t u4BcnHdrLen;
	uint32_t u4BcnBodyLen;
	uint32_t u4AssocRespLen;
#if CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP
	uint32_t u4ProbeRespLen;
#endif
	uint8_t *pucBcnHdr;
	uint8_t *pucBcnBody;
	uint8_t *pucAssocRespIE;
#if CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP
	uint8_t *pucProbeRespIE;
#endif
	u_int8_t fgIsWepCipher;
	uint8_t aucBuffer[1];	/* Header & Body & Extra IEs are put here. */
};

struct MSG_P2P_MGMT_FRAME_UPDATE {
	struct MSG_HDR rMsgHdr;
	enum ENUM_BUFFER_TYPE eBufferType;
	uint32_t u4BufferLen;
	uint8_t aucBuffer[1];
};

struct MSG_P2P_SWITCH_OP_MODE {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	enum ENUM_OP_MODE eOpMode;
	uint8_t ucRoleIdx;
	enum ENUM_IFTYPE eIftype;
};

struct MSG_P2P_MGMT_FRAME_REGISTER {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint16_t u2FrameType;
	u_int8_t fgIsRegister;
};

struct MSG_P2P_CHNL_ABORT {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint64_t u8Cookie;
};

struct MSG_P2P_CONNECTION_REQUEST {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucRoleIdx;
	struct P2P_SSID_STRUCT rSsid;
	uint8_t aucBssid[MAC_ADDR_LEN];
	uint8_t aucSrcMacAddr[MAC_ADDR_LEN];
	enum ENUM_CHNL_EXT eChnlSco;
	struct RF_CHANNEL_INFO rChannelInfo;
	uint32_t u4IELen;
	uint8_t aucIEBuf[1];
	/* TODO: Auth Type, OPEN, SHARED, FT, EAP... */
};

struct MSG_P2P_CONNECTION_ABORT {
	struct MSG_HDR rMsgHdr;	/* Must be the first member. */
	uint8_t ucRoleIdx;
	uint8_t aucTargetID[MAC_ADDR_LEN];
	uint16_t u2ReasonCode;
	u_int8_t fgSendDeauth;
};

struct MSG_P2P_START_AP {
	struct MSG_HDR rMsgHdr;
	uint32_t u4DtimPeriod;
	uint32_t u4BcnInterval;
	uint8_t aucSsid[32];
	uint16_t u2SsidLen;
	uint8_t ucHiddenSsidType;
	u_int8_t fgIsPrivacy;
	uint8_t ucRoleIdx;
	struct AP_CRYPTO_SETTINGS rEncryptionSettings;
	int32_t i4InactiveTimeout;
};

#if (CFG_SUPPORT_DFS_MASTER == 1)
struct MSG_P2P_DFS_CAC {
	struct MSG_HDR rMsgHdr;
	enum ENUM_CHANNEL_WIDTH eChannelWidth;
	uint8_t ucRoleIdx;
};

struct MSG_P2P_RADAR_DETECT {
	struct MSG_HDR rMsgHdr;
	uint8_t ucBssIndex;
};

struct P2P_RADAR_INFO {
	uint8_t ucRadarReportMode;
	/*0: Only report radar detected;   1:  Add parameter reports*/
	uint8_t ucRddIdx;
	uint8_t ucLongDetected;
	uint8_t ucPeriodicDetected;
	uint8_t ucLPBNum;
	uint8_t ucPPBNum;
	uint8_t ucLPBPeriodValid;
	uint8_t ucLPBWidthValid;
	uint8_t ucPRICountM1;
	uint8_t ucPRICountM1TH;
	uint8_t ucPRICountM2;
	uint8_t ucPRICountM2TH;
	uint32_t u4PRI1stUs;
	struct LONG_PULSE_BUFFER arLpbContent[LPB_SIZE];
	struct PERIODIC_PULSE_BUFFER arPpbContent[PPB_SIZE];
};

struct MSG_P2P_SET_NEW_CHANNEL {
	struct MSG_HDR rMsgHdr;
	enum ENUM_CHANNEL_WIDTH eChannelWidth;
	uint8_t ucRoleIdx;
	uint8_t ucBssIndex;
};

struct MSG_P2P_CSA_DONE {
	struct MSG_HDR rMsgHdr;
	uint8_t ucBssIndex;
};
#endif

struct MSG_P2P_DEL_IFACE {
	struct MSG_HDR rMsgHdr;
	uint8_t ucRoleIdx;
};

struct P2P_STATION_INFO {
	uint32_t u4InactiveTime;
	uint32_t u4RxBytes;	/* TODO: */
	uint32_t u4TxBytes;	/* TODO: */
	uint32_t u4RxPackets;	/* TODO: */
	uint32_t u4TxPackets;	/* TODO: */
	/* TODO: Add more for requirement. */
};

/* 3  --------------- WFA P2P Attributes Handler prototype --------------- */
typedef uint32_t(*PFN_APPEND_ATTRI_FUNC) (struct ADAPTER *,
		uint8_t, u_int8_t, uint16_t *, uint8_t *, uint16_t);

typedef uint32_t(*PFN_CALCULATE_VAR_ATTRI_LEN_FUNC) (struct ADAPTER *,
		struct STA_RECORD *);

struct APPEND_VAR_ATTRI_ENTRY {
	uint16_t u2EstimatedFixedAttriLen;	/* For fixed length */
	PFN_CALCULATE_VAR_ATTRI_LEN_FUNC pfnCalculateVariableAttriLen;
	PFN_APPEND_ATTRI_FUNC pfnAppendAttri;
};

/* //////////////////////////////////////////////////////////////// */

enum ENUM_P2P_ROLE_STATE {
	P2P_ROLE_STATE_IDLE = 0,
	P2P_ROLE_STATE_SCAN,
	P2P_ROLE_STATE_REQING_CHANNEL,
	P2P_ROLE_STATE_AP_CHNL_DETECTION,
	/* Requesting Channel to Send Specific Frame. */
	P2P_ROLE_STATE_GC_JOIN,
#if (CFG_SUPPORT_DFS_MASTER == 1)
	P2P_ROLE_STATE_DFS_CAC,
	P2P_ROLE_STATE_SWITCH_CHANNEL,
#endif
	P2P_ROLE_STATE_NUM
};

enum ENUM_P2P_CONNECTION_TYPE {
	P2P_CONNECTION_TYPE_IDLE = 0,
	P2P_CONNECTION_TYPE_GO,
	P2P_CONNECTION_TYPE_GC,
	P2P_CONNECTION_TYPE_PURE_AP,
	P2P_CONNECTION_TYPE_NUM
};

struct P2P_JOIN_INFO {
	uint8_t ucSeqNumOfReqMsg;
	uint8_t ucAvailableAuthTypes;
	struct STA_RECORD *prTargetStaRec;
	struct BSS_DESC *prTargetBssDesc;
	u_int8_t fgIsJoinComplete;
	/* For ASSOC Rsp. */
	uint32_t u4BufLength;
	uint8_t aucIEBuf[MAX_IE_LENGTH];
};

/* For STA & AP mode. */
struct P2P_CONNECTION_REQ_INFO {
	enum ENUM_P2P_CONNECTION_TYPE eConnRequest;
	struct P2P_SSID_STRUCT rSsidStruct;
	uint8_t aucBssid[MAC_ADDR_LEN];

	/* AP preferred channel. */
	struct RF_CHANNEL_INFO rChannelInfo;
	enum ENUM_CHNL_EXT eChnlExt;

	/* To record channel bandwidth from CFG80211 */
	enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw;

	/* To record primary channel frequency (MHz) from CFG80211 */
	uint16_t u2PriChnlFreq;

	/* To record Channel Center Frequency Segment 0 (MHz) from CFG80211 */
	uint32_t u4CenterFreq1;

	/* To record Channel Center Frequency Segment 1 (MHz) from CFG80211 */
	uint32_t u4CenterFreq2;

	/* For ASSOC Req. */
	uint32_t u4BufLength;
	uint8_t aucIEBuf[MAX_IE_LENGTH];
};

#define P2P_ROLE_INDEX_2_ROLE_FSM_INFO(_prAdapter, _RoleIndex) \
	((_prAdapter)->rWifiVar.aprP2pRoleFsmInfo[_RoleIndex])

struct P2P_ROLE_FSM_INFO {
	uint8_t ucRoleIndex;

	uint8_t ucBssIndex;

	/* State related. */
	enum ENUM_P2P_ROLE_STATE eCurrentState;

	/* Channel related. */
	struct P2P_CHNL_REQ_INFO rChnlReqInfo;

	/* Scan related. */
	struct P2P_SCAN_REQ_INFO rScanReqInfo;

	/* FSM Timer */
	struct TIMER rP2pRoleFsmTimeoutTimer;

#if	CFG_ENABLE_PER_STA_STATISTICS_LOG
	/* Get statistics Timer */
	struct TIMER rP2pRoleFsmGetStatisticsTimer;
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
	struct TIMER rDfsShutDownTimer;
#endif

	/* Packet filter for P2P module. */
	uint32_t u4P2pPacketFilter;

	/* GC Join related. */
	struct P2P_JOIN_INFO rJoinInfo;

	/* Connection related. */
	struct P2P_CONNECTION_REQ_INFO rConnReqInfo;

	/* Beacon Information. */
	struct P2P_BEACON_UPDATE_INFO rBeaconUpdateInfo;
};

/*========================= Initial ============================*/

uint8_t p2pRoleFsmInit(IN struct ADAPTER *prAdapter, IN uint8_t ucRoleIdx);

void p2pRoleFsmUninit(IN struct ADAPTER *prAdapter, IN uint8_t ucRoleIdx);

/*================== Message Event ==================*/

void p2pRoleFsmRunEventAbort(IN struct ADAPTER *prAdapter,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo);

void p2pRoleFsmRunEventStartAP(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventDelIface(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventStopAP(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

#if (CFG_SUPPORT_DFS_MASTER == 1)
void p2pRoleFsmRunEventDfsCac(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventRadarDet(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventSetNewChannel(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventCsaDone(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventDfsShutDownTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr);
#endif

void p2pRoleFsmRunEventScanRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void
p2pRoleFsmRunEventScanDone(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo);

void p2pRoleFsmRunEventJoinComplete(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr);

void p2pRoleFsmDeauthTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr);

void p2pRoleFsmRunEventBeaconTimeout(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pBssInfo);

void p2pRoleUpdateACLEntry(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIdx);

u_int8_t p2pRoleProcessACLInspection(IN struct ADAPTER *prAdapter,
		IN uint8_t *pMacAddr, IN uint8_t ucBssIdx);

uint32_t
p2pRoleFsmRunEventAAAComplete(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct BSS_INFO *prP2pBssInfo);

uint32_t
p2pRoleFsmRunEventAAASuccess(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct BSS_INFO *prP2pBssInfo);

void p2pRoleFsmRunEventAAATxFail(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec,
		IN struct BSS_INFO *prP2pBssInfo);

void p2pRoleFsmRunEventConnectionRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventConnectionAbort(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void
p2pRoleFsmRunEventChnlGrant(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo);

uint32_t
p2pRoleFsmRunEventDeauthTxDone(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo,
		IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void p2pRoleFsmRunEventRxDeauthentication(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec, IN struct SW_RFB *prSwRfb);

void p2pRoleFsmRunEventRxDisassociation(IN struct ADAPTER *prAdapter,
		IN struct STA_RECORD *prStaRec, IN struct SW_RFB *prSwRfb);

/* //////////////////////// TO BE REFINE ///////////////////// */
void p2pRoleFsmRunEventSwitchOPMode(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventBeaconUpdate(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pRoleFsmRunEventDissolve(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void
p2pProcessEvent_UpdateNOAParam(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIdx,
		IN struct EVENT_UPDATE_NOA_PARAMS *prEventUpdateNoaParam);

#if	CFG_ENABLE_PER_STA_STATISTICS_LOG
void p2pRoleFsmGetStaStatistics(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr);
#endif

void p2pRoleFsmNotifyEapolTxStatus(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex,
		IN enum ENUM_EAPOL_KEY_TYPE_T rEapolKeyType,
		IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

#endif

void p2pProcessPreSuspendFlow(IN struct ADAPTER *prAdapter);

