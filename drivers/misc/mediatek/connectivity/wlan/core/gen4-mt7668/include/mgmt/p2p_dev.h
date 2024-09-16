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
typedef enum _ENUM_P2P_DEV_STATE_T {
	P2P_DEV_STATE_IDLE = 0,
	P2P_DEV_STATE_SCAN,
	P2P_DEV_STATE_REQING_CHANNEL,
	P2P_DEV_STATE_CHNL_ON_HAND,
	P2P_DEV_STATE_OFF_CHNL_TX,	/* Requesting Channel to Send Specific Frame. */
	P2P_DEV_STATE_NUM
} ENUM_P2P_DEV_STATE_T, *P_ENUM_P2P_DEV_STATE_T;

/*-------------------- EVENT MESSAGE ---------------------*/
typedef struct _MSG_P2P_SCAN_REQUEST_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucBssIdx;
	ENUM_SCAN_TYPE_T eScanType;
	P_P2P_SSID_STRUCT_T prSSID;
	INT_32 i4SsidNum;
	UINT_32 u4NumChannel;
	PUINT_8 pucIEBuf;
	UINT_32 u4IELen;
	BOOLEAN fgIsAbort;
	RF_CHANNEL_INFO_T arChannelListInfo[1];
} MSG_P2P_SCAN_REQUEST_T, *P_MSG_P2P_SCAN_REQUEST_T;

typedef struct _MSG_P2P_CHNL_REQUEST_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_64 u8Cookie;
	UINT_32 u4Duration;
	ENUM_CHNL_EXT_T eChnlSco;
	RF_CHANNEL_INFO_T rChannelInfo;
	ENUM_CH_REQ_TYPE_T eChnlReqType;
} MSG_P2P_CHNL_REQUEST_T, *P_MSG_P2P_CHNL_REQUEST_T;

typedef struct _MSG_P2P_MGMT_TX_REQUEST_T {
	MSG_HDR_T rMsgHdr;
	UINT_8 ucBssIdx;
	P_MSDU_INFO_T prMgmtMsduInfo;
	UINT_64 u8Cookie;	/* For indication. */
	BOOLEAN fgNoneCckRate;
	BOOLEAN fgIsOffChannel;
	RF_CHANNEL_INFO_T rChannelInfo;	/* Off channel TX. */
	ENUM_CHNL_EXT_T eChnlExt;
	BOOLEAN fgIsWaitRsp;
} MSG_P2P_MGMT_TX_REQUEST_T, *P_MSG_P2P_MGMT_TX_REQUEST_T;

#if CFG_SUPPORT_WFD

#define WFD_FLAGS_DEV_INFO_VALID            BIT(0)	/* 1. WFD_DEV_INFO, 2. WFD_CTRL_PORT, 3. WFD_MAT_TP. */
#define WFD_FLAGS_SINK_INFO_VALID           BIT(1)	/* 1. WFD_SINK_STATUS, 2. WFD_SINK_MAC. */
#define WFD_FLAGS_ASSOC_MAC_VALID        BIT(2)	/* 1. WFD_ASSOC_MAC. */
#define WFD_FLAGS_EXT_CAPABILITY_VALID  BIT(3)	/* 1. WFD_EXTEND_CAPABILITY. */

struct _WFD_CFG_SETTINGS_T {
	UINT_32 u4WfdCmdType;
	UINT_8 ucWfdEnable;
	UINT_8 ucWfdCoupleSinkStatus;
	UINT_8 ucWfdSessionAvailable;	/* 0: NA 1:Set 2:Clear */
	UINT_8 ucWfdSigmaMode;
	UINT_16 u2WfdDevInfo;
	UINT_16 u2WfdControlPort;
	UINT_16 u2WfdMaximumTp;
	UINT_16 u2WfdExtendCap;
	UINT_8 aucWfdCoupleSinkAddress[MAC_ADDR_LEN];
	UINT_8 aucWfdAssociatedBssid[MAC_ADDR_LEN];
	UINT_8 aucWfdVideoIp[4];
	UINT_8 aucWfdAudioIp[4];
	UINT_16 u2WfdVideoPort;
	UINT_16 u2WfdAudioPort;
	UINT_32 u4WfdFlag;
	UINT_32 u4WfdPolicy;
	UINT_32 u4WfdState;
	UINT_8 aucWfdSessionInformationIE[24 * 8];
	UINT_16 u2WfdSessionInformationIELen;
	UINT_8 aucReserved1[2];
	UINT_8 aucWfdPrimarySinkMac[MAC_ADDR_LEN];
	UINT_8 aucWfdSecondarySinkMac[MAC_ADDR_LEN];
	UINT_32 u4WfdAdvancedFlag;
	/* Group 1 64 bytes */
	UINT_8 aucWfdLocalIp[4];
	UINT_16 u2WfdLifetimeAc2;	/* Unit is 2 TU */
	UINT_16 u2WfdLifetimeAc3;	/* Unit is 2 TU */
	UINT_16 u2WfdCounterThreshold;	/* Unit is ms */
	UINT_8 aucReverved2[54];
	/* Group 2 64 bytes */
	UINT_8 aucReverved3[64];
	/* Group 3 64 bytes */
	UINT_8 aucReverved4[64];
};

#endif

typedef struct _MSG_P2P_ACTIVE_DEV_BSS_T {
	MSG_HDR_T rMsgHdr;
} MSG_P2P_ACTIVE_DEV_BSS_T, *P_MSG_P2P_ACTIVE_DEV_BSS_T;

/*-------------------- P2P FSM ACTION STRUCT ---------------------*/

typedef struct _P2P_OFF_CHNL_TX_REQ_INFO_T {
	LINK_ENTRY_T rLinkEntry;
	P_MSDU_INFO_T prMgmtTxMsdu;
	BOOLEAN fgNoneCckRate;
	RF_CHANNEL_INFO_T rChannelInfo;	/* Off channel TX. */
	ENUM_CHNL_EXT_T eChnlExt;
	BOOLEAN fgIsWaitRsp;	/* See if driver should keep at the same channel. */
} P2P_OFF_CHNL_TX_REQ_INFO_T, *P_P2P_OFF_CHNL_TX_REQ_INFO_T;

typedef struct _P2P_MGMT_TX_REQ_INFO_T {
	LINK_T rP2pTxReqLink;
	P_MSDU_INFO_T prMgmtTxMsdu;
	BOOLEAN fgIsWaitRsp;
} P2P_MGMT_TX_REQ_INFO_T, *P_P2P_MGMT_TX_REQ_INFO_T;

struct _P2P_DEV_FSM_INFO_T {
	UINT_8 ucBssIndex;
	/* State related. */
	ENUM_P2P_DEV_STATE_T eCurrentState;

	/* Channel related. */
	P2P_CHNL_REQ_INFO_T rChnlReqInfo;

	/* Scan related. */
	P2P_SCAN_REQ_INFO_T rScanReqInfo;

	/* Mgmt tx related. */
	P2P_MGMT_TX_REQ_INFO_T rMgmtTxInfo;

	/* FSM Timer */
	TIMER_T rP2pFsmTimeoutTimer;

	/* Packet filter for P2P module. */
	UINT_32 u4P2pPacketFilter;
};

typedef struct _MSG_P2P_NETDEV_REGISTER_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	BOOLEAN fgIsEnable;
	UINT_8 ucMode;
} MSG_P2P_NETDEV_REGISTER_T, *P_MSG_P2P_NETDEV_REGISTER_T;

#if CFG_SUPPORT_WFD
typedef struct _MSG_WFD_CONFIG_SETTINGS_CHANGED_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings;
} MSG_WFD_CONFIG_SETTINGS_CHANGED_T, *P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T;
#endif

/*========================= Initial ============================*/

UINT_8 p2pDevFsmInit(IN P_ADAPTER_T prAdapter);

VOID p2pDevFsmUninit(IN P_ADAPTER_T prAdapter);

/*========================= FUNCTIONs ============================*/

VOID
p2pDevFsmStateTransition(IN P_ADAPTER_T prAdapter, IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo, IN ENUM_P2P_DEV_STATE_T
			 eNextState);

VOID p2pDevFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo);

VOID p2pDevFsmRunEventTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

/*================ Message Event =================*/
VOID p2pDevFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
p2pDevFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo);

VOID p2pDevFsmRunEventChannelRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pDevFsmRunEventChannelAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
p2pDevFsmRunEventChnlGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo);

VOID p2pDevFsmRunEventMgmtTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

WLAN_STATUS
p2pDevFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T
				 rTxDoneStatus);

VOID p2pDevFsmRunEventMgmtFrameRegister(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

/* /////////////////////////////// */

VOID p2pDevFsmRunEventActiveDevBss(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

