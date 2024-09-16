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

typedef enum _ENUM_BUFFER_TYPE_T {
	ENUM_FRAME_TYPE_EXTRA_IE_BEACON,
	ENUM_FRAME_TYPE_EXTRA_IE_ASSOC_RSP,
	ENUM_FRAME_TYPE_EXTRA_IE_PROBE_RSP,
	ENUM_FRAME_TYPE_PROBE_RSP_TEMPLATE,
	ENUM_FRAME_TYPE_BEACON_TEMPLATE,
	ENUM_FRAME_IE_NUM
} ENUM_BUFFER_TYPE_T, *P_ENUM_BUFFER_TYPE_T;

typedef enum _ENUM_HIDDEN_SSID_TYPE_T {
	ENUM_HIDDEN_SSID_NONE,
	ENUM_HIDDEN_SSID_LEN,
	ENUM_HIDDEN_SSID_ZERO_CONTENT,
	ENUM_HIDDEN_SSID_NUM
} ENUM_HIDDEN_SSID_TYPE_T, *P_ENUM_HIDDEN_SSID_TYPE_T;

typedef struct _P2P_BEACON_UPDATE_INFO_T {
	PUINT_8 pucBcnHdr;
	UINT_32 u4BcnHdrLen;
	PUINT_8 pucBcnBody;
	UINT_32 u4BcnBodyLen;
} P2P_BEACON_UPDATE_INFO_T, *P_P2P_BEACON_UPDATE_INFO_T;

typedef struct _P2P_PROBE_RSP_UPDATE_INFO_T {
	P_MSDU_INFO_T prProbeRspMsduTemplate;
} P2P_PROBE_RSP_UPDATE_INFO_T, *P_P2P_PROBE_RSP_UPDATE_INFO_T;

typedef struct _P2P_ASSOC_RSP_UPDATE_INFO_T {
	PUINT_8 pucAssocRspExtIE;
	UINT_16 u2AssocIELen;
} P2P_ASSOC_RSP_UPDATE_INFO_T, *P_P2P_ASSOC_RSP_UPDATE_INFO_T;

typedef struct _AP_CRYPTO_SETTINGS_T {
	UINT_32 u4WpaVersion;
	UINT_32 u4CipherGroup;
	INT_32 i4NumOfCiphers;
	UINT_32 aucCiphersPairwise[5];
	INT_32 i4NumOfAkmSuites;
	UINT_32 aucAkmSuites[2];
	BOOLEAN fgIsControlPort;
	UINT_16 u2ControlPortBE;
	BOOLEAN fgIsControlPortEncrypt;
} AP_CRYPTO_SETTINGS_T, *P_AP_CRYPTO_SETTINGS_T;

/* ////////////////////////////////// Message //////////////////////////////////////////////////// */

typedef struct _MSG_P2P_BEACON_UPDATE_T {
	MSG_HDR_T rMsgHdr;
	UINT_8 ucRoleIndex;
	UINT_32 u4BcnHdrLen;
	UINT_32 u4BcnBodyLen;
	UINT_32 u4AssocRespLen;
	PUINT_8 pucBcnHdr;
	PUINT_8 pucBcnBody;
	PUINT_8 pucAssocRespIE;
	BOOLEAN fgIsWepCipher;
	UINT_8 aucBuffer[1];	/* Header & Body & Extra IEs are put here. */
} MSG_P2P_BEACON_UPDATE_T, *P_MSG_P2P_BEACON_UPDATE_T;

typedef struct _MSG_P2P_MGMT_FRAME_UPDATE_T {
	MSG_HDR_T rMsgHdr;
	ENUM_BUFFER_TYPE_T eBufferType;
	UINT_32 u4BufferLen;
	UINT_8 aucBuffer[1];
} MSG_P2P_MGMT_FRAME_UPDATE_T, *P_MSG_P2P_MGMT_FRAME_UPDATE_T;

typedef struct _MSG_P2P_SWITCH_OP_MODE_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	ENUM_OP_MODE_T eOpMode;
	UINT_8 ucRoleIdx;
} MSG_P2P_SWITCH_OP_MODE_T, *P_MSG_P2P_SWITCH_OP_MODE_T;

typedef struct _MSG_P2P_MGMT_FRAME_REGISTER_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_16 u2FrameType;
	BOOLEAN fgIsRegister;
} MSG_P2P_MGMT_FRAME_REGISTER_T, *P_MSG_P2P_MGMT_FRAME_REGISTER_T;

typedef struct _MSG_P2P_CHNL_ABORT_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_64 u8Cookie;
} MSG_P2P_CHNL_ABORT_T, *P_MSG_P2P_CHNL_ABORT_T;

typedef struct _MSG_P2P_CONNECTION_REQUEST_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucRoleIdx;
	P2P_SSID_STRUCT_T rSsid;
	UINT_8 aucBssid[MAC_ADDR_LEN];
	UINT_8 aucSrcMacAddr[MAC_ADDR_LEN];
	ENUM_CHNL_EXT_T eChnlSco;
	RF_CHANNEL_INFO_T rChannelInfo;
	UINT_32 u4IELen;
	UINT_8 aucIEBuf[1];
	/* TODO: Auth Type, OPEN, SHARED, FT, EAP... */
} MSG_P2P_CONNECTION_REQUEST_T, *P_MSG_P2P_CONNECTION_REQUEST_T;

typedef struct _MSG_P2P_CONNECTION_ABORT_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member. */
	UINT_8 ucRoleIdx;
	UINT_8 aucTargetID[MAC_ADDR_LEN];
	UINT_16 u2ReasonCode;
	BOOLEAN fgSendDeauth;
} MSG_P2P_CONNECTION_ABORT_T, *P_MSG_P2P_CONNECTION_ABORT_T;

typedef struct _MSG_P2P_START_AP_T {
	MSG_HDR_T rMsgHdr;
	UINT_32 u4DtimPeriod;
	UINT_32 u4BcnInterval;
	UINT_8 aucSsid[32];
	UINT_16 u2SsidLen;
	UINT_8 ucHiddenSsidType;
	BOOLEAN fgIsPrivacy;
	UINT_8 ucRoleIdx;
	AP_CRYPTO_SETTINGS_T rEncryptionSettings;
	INT_32 i4InactiveTimeout;
} MSG_P2P_START_AP_T, *P_MSG_P2P_START_AP_T;

#if (CFG_SUPPORT_DFS_MASTER == 1)
typedef struct _MSG_P2P_DFS_CAC_T {
	MSG_HDR_T rMsgHdr;
	ENUM_CHANNEL_WIDTH_T eChannelWidth;
	UINT_8 ucRoleIdx;
} MSG_P2P_DFS_CAC_T, *P_MSG_P2P_DFS_CAC_T;

typedef struct _MSG_P2P_RADAR_DETECT_T {
	MSG_HDR_T rMsgHdr;
	UINT_8 ucBssIndex;
} MSG_P2P_RADAR_DETECT_T, *P_MSG_P2P_RADAR_DETECT_T;

struct P2P_RADAR_INFO {
	UINT_8 ucRadarReportMode; /*0: Only report radar detected;   1:  Add parameter reports*/
	UINT_8 ucRddIdx;
	UINT_8 ucLongDetected;
	UINT_8 ucPeriodicDetected;
	UINT_8 ucLPBNum;
	UINT_8 ucPPBNum;
	UINT_8 ucLPBPeriodValid;
	UINT_8 ucLPBWidthValid;
	UINT_8 ucPRICountM1;
	UINT_8 ucPRICountM1TH;
	UINT_8 ucPRICountM2;
	UINT_8 ucPRICountM2TH;
	UINT_32 u4PRI1stUs;
	LONG_PULSE_BUFFER_T arLpbContent[32];
	PERIODIC_PULSE_BUFFER_T arPpbContent[32];
};

typedef struct _MSG_P2P_SET_NEW_CHANNEL_T {
	MSG_HDR_T rMsgHdr;
	ENUM_CHANNEL_WIDTH_T eChannelWidth;
	UINT_8 ucRoleIdx;
	UINT_8 ucBssIndex;
} MSG_P2P_SET_NEW_CHANNEL_T, *P_MSG_P2P_SET_NEW_CHANNEL_T;

typedef struct _MSG_P2P_CSA_DONE_T {
	MSG_HDR_T rMsgHdr;
	UINT_8 ucBssIndex;
} MSG_P2P_CSA_DONE_T, *P_MSG_P2P_CSA_DONE_T;
#endif

typedef struct _MSG_P2P_DEL_IFACE_T {
	MSG_HDR_T rMsgHdr;
	UINT_8 ucRoleIdx;
} MSG_P2P_DEL_IFACE_T, *P_MSG_P2P_DEL_IFACE_T;

typedef struct _P2P_STATION_INFO_T {
	UINT_32 u4InactiveTime;
	UINT_32 u4RxBytes;	/* TODO: */
	UINT_32 u4TxBytes;	/* TODO: */
	UINT_32 u4RxPackets;	/* TODO: */
	UINT_32 u4TxPackets;	/* TODO: */
	/* TODO: Add more for requirement. */
} P2P_STATION_INFO_T, *P_P2P_STATION_INFO_T;

/* 3  --------------- WFA P2P Attributes Handler prototype --------------- */
typedef UINT_32(*PFN_APPEND_ATTRI_FUNC) (P_ADAPTER_T, UINT_8, BOOLEAN, PUINT_16, PUINT_8, UINT_16);

typedef UINT_32(*PFN_CALCULATE_VAR_ATTRI_LEN_FUNC) (P_ADAPTER_T, P_STA_RECORD_T);

typedef struct _APPEND_VAR_ATTRI_ENTRY_T {
	UINT_16 u2EstimatedFixedAttriLen;	/* For fixed length */
	PFN_CALCULATE_VAR_ATTRI_LEN_FUNC pfnCalculateVariableAttriLen;
	PFN_APPEND_ATTRI_FUNC pfnAppendAttri;
} APPEND_VAR_ATTRI_ENTRY_T, *P_APPEND_VAR_ATTRI_ENTRY_T;

/* //////////////////////////////////////////////////////////////// */

typedef enum _ENUM_P2P_ROLE_STATE_T {
	P2P_ROLE_STATE_IDLE = 0,
	P2P_ROLE_STATE_SCAN,
	P2P_ROLE_STATE_REQING_CHANNEL,
	P2P_ROLE_STATE_AP_CHNL_DETECTION,	/* Requesting Channel to Send Specific Frame. */
	P2P_ROLE_STATE_GC_JOIN,
#if (CFG_SUPPORT_DFS_MASTER == 1)
	P2P_ROLE_STATE_DFS_CAC,
	P2P_ROLE_STATE_SWITCH_CHANNEL,
#endif
	P2P_ROLE_STATE_NUM
} ENUM_P2P_ROLE_STATE_T, *P_ENUM_P2P_ROLE_STATE_T;

typedef enum _ENUM_P2P_CONNECTION_TYPE_T {
	P2P_CONNECTION_TYPE_IDLE = 0,
	P2P_CONNECTION_TYPE_GO,
	P2P_CONNECTION_TYPE_GC,
	P2P_CONNECTION_TYPE_PURE_AP,
	P2P_CONNECTION_TYPE_NUM
} ENUM_P2P_CONNECTION_TYPE_T, *P_ENUM_P2P_CONNECTION_TYPE_T;

typedef struct _P2P_JOIN_INFO_T {
	UINT_8 ucSeqNumOfReqMsg;
	UINT_8 ucAvailableAuthTypes;
	P_STA_RECORD_T prTargetStaRec;
	P_BSS_DESC_T prTargetBssDesc;
	BOOLEAN fgIsJoinComplete;
	/* For ASSOC Rsp. */
	UINT_32 u4BufLength;
	UINT_8 aucIEBuf[MAX_IE_LENGTH];
} P2P_JOIN_INFO_T, *P_P2P_JOIN_INFO_T;

/* For STA & AP mode. */
typedef struct _P2P_CONNECTION_REQ_INFO_T {
	ENUM_P2P_CONNECTION_TYPE_T eConnRequest;
	P2P_SSID_STRUCT_T rSsidStruct;
	UINT_8 aucBssid[MAC_ADDR_LEN];

	/* AP preferred channel. */
	RF_CHANNEL_INFO_T rChannelInfo;
	ENUM_CHNL_EXT_T eChnlExt;

	/* To record channel bandwidth from CFG80211 */
	ENUM_MAX_BANDWIDTH_SETTING eChnlBw;

	/* To record primary channel frequency (MHz) from CFG80211 */
	UINT_16 u2PriChnlFreq;

	/* To record Channel Center Frequency Segment 0 (MHz) from CFG80211 */
	UINT_32 u4CenterFreq1;

	/* To record Channel Center Frequency Segment 1 (MHz) from CFG80211 */
	UINT_32 u4CenterFreq2;

	/* For ASSOC Req. */
	UINT_32 u4BufLength;
	UINT_8 aucIEBuf[MAX_IE_LENGTH];
} P2P_CONNECTION_REQ_INFO_T, *P_P2P_CONNECTION_REQ_INFO_T;

#define P2P_ROLE_INDEX_2_ROLE_FSM_INFO(_prAdapter, _RoleIndex) \
	((_prAdapter)->rWifiVar.aprP2pRoleFsmInfo[_RoleIndex])

struct _P2P_ROLE_FSM_INFO_T {
	UINT_8 ucRoleIndex;

	UINT_8 ucBssIndex;

	/* State related. */
	ENUM_P2P_ROLE_STATE_T eCurrentState;

	/* Channel related. */
	P2P_CHNL_REQ_INFO_T rChnlReqInfo;

	/* Scan related. */
	P2P_SCAN_REQ_INFO_T rScanReqInfo;

	/* FSM Timer */
	TIMER_T rP2pRoleFsmTimeoutTimer;

#if (CFG_SUPPORT_DFS_MASTER == 1)
	TIMER_T rDfsShutDownTimer;
#endif

	/* Packet filter for P2P module. */
	UINT_32 u4P2pPacketFilter;

	/* GC Join related. */
	P2P_JOIN_INFO_T rJoinInfo;

	/* Connection related. */
	P2P_CONNECTION_REQ_INFO_T rConnReqInfo;

	/* Beacon Information. */
	P2P_BEACON_UPDATE_INFO_T rBeaconUpdateInfo;
};

/*========================= Initial ============================*/

UINT_8 p2pRoleFsmInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx);

VOID p2pRoleFsmUninit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx);

/*================== Message Event ==================*/

VOID p2pRoleFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo);

VOID p2pRoleFsmRunEventStartAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventDelIface(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventStopAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

#if (CFG_SUPPORT_DFS_MASTER == 1)
VOID p2pRoleFsmRunEventDfsCac(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventRadarDet(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventSetNewChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventCsaDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventDfsShutDownTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);
#endif

VOID p2pRoleFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
p2pRoleFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_ROLE_FSM_INFO_T
			   prP2pRoleFsmInfo);

VOID p2pRoleFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID p2pRoleFsmDeauthTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID p2pRoleFsmRunEventBeaconTimeout(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo);

VOID p2pRoleUpdateACLEntry(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx);

BOOL p2pRoleProcessACLInspection(IN P_ADAPTER_T prAdapter, IN PUCHAR pMacAddr, IN UINT_8 ucBssIdx);

WLAN_STATUS
p2pRoleFsmRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo);

WLAN_STATUS
p2pRoleFsmRunEventAAASuccess(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo);

VOID p2pRoleFsmRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo);

VOID p2pRoleFsmRunEventConnectionRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventConnectionAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
p2pRoleFsmRunEventChnlGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_ROLE_FSM_INFO_T
			    prP2pRoleFsmInfo);

WLAN_STATUS
p2pRoleFsmRunEventDeauthTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T
			       rTxDoneStatus);

VOID p2pRoleFsmRunEventRxDeauthentication(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

VOID p2pRoleFsmRunEventRxDisassociation(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

/* //////////////////////// TO BE REFINE ///////////////////// */
VOID p2pRoleFsmRunEventSwitchOPMode(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventBeaconUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventDissolve(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
p2pProcessEvent_UpdateNOAParam(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_EVENT_UPDATE_NOA_PARAMS_T
			       prEventUpdateNoaParam);

VOID
p2pProcessPreSuspendFlow(IN P_ADAPTER_T prAdapter);

#endif
