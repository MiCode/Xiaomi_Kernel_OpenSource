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
enum ENUM_P2P_DEV_STATE {
	P2P_DEV_STATE_IDLE = 0,
	P2P_DEV_STATE_SCAN,
	P2P_DEV_STATE_REQING_CHANNEL,
	P2P_DEV_STATE_CHNL_ON_HAND,
	P2P_DEV_STATE_OFF_CHNL_TX,
	/* Requesting Channel to Send Specific Frame. */
	P2P_DEV_STATE_NUM
};

/*-------------------- EVENT MESSAGE ---------------------*/
struct MSG_P2P_SCAN_REQUEST {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucBssIdx;
	enum ENUM_SCAN_TYPE eScanType;
	struct P2P_SSID_STRUCT *prSSID;
	int32_t i4SsidNum;
	uint32_t u4NumChannel;
	uint8_t *pucIEBuf;
	uint32_t u4IELen;
	u_int8_t fgIsAbort;
	enum ENUM_SCAN_REASON eScanReason;
	struct RF_CHANNEL_INFO arChannelListInfo[1];
};

struct MSG_P2P_CHNL_REQUEST {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint64_t u8Cookie;
	uint32_t u4Duration;
	enum ENUM_CHNL_EXT eChnlSco;
	struct RF_CHANNEL_INFO rChannelInfo;
	enum ENUM_CH_REQ_TYPE eChnlReqType;
};

#define P2P_DEV_EXTEND_CHAN_TIME	500

#if CFG_SUPPORT_WFD

#define WFD_FLAGS_DEV_INFO_VALID            BIT(0)
/* 1. WFD_DEV_INFO, 2. WFD_CTRL_PORT, 3. WFD_MAT_TP. */
#define WFD_FLAGS_SINK_INFO_VALID           BIT(1)
/* 1. WFD_SINK_STATUS, 2. WFD_SINK_MAC. */
#define WFD_FLAGS_ASSOC_MAC_VALID        BIT(2)
/* 1. WFD_ASSOC_MAC. */
#define WFD_FLAGS_EXT_CAPABILITY_VALID  BIT(3)
/* 1. WFD_EXTEND_CAPABILITY. */

struct WFD_CFG_SETTINGS {
	uint32_t u4WfdCmdType;
	uint8_t ucWfdEnable;
	uint8_t ucWfdCoupleSinkStatus;
	uint8_t ucWfdSessionAvailable;	/* 0: NA 1:Set 2:Clear */
	uint8_t ucWfdSigmaMode;
	uint16_t u2WfdDevInfo;
	uint16_t u2WfdControlPort;
	uint16_t u2WfdMaximumTp;
	uint16_t u2WfdExtendCap;
	uint8_t aucWfdCoupleSinkAddress[MAC_ADDR_LEN];
	uint8_t aucWfdAssociatedBssid[MAC_ADDR_LEN];
	uint8_t aucWfdVideoIp[4];
	uint8_t aucWfdAudioIp[4];
	uint16_t u2WfdVideoPort;
	uint16_t u2WfdAudioPort;
	uint32_t u4WfdFlag;
	uint32_t u4WfdPolicy;
	uint32_t u4WfdState;
	uint8_t aucWfdSessionInformationIE[24 * 8];
	uint16_t u2WfdSessionInformationIELen;
	uint8_t aucReserved1[2];
	uint8_t aucWfdPrimarySinkMac[MAC_ADDR_LEN];
	uint8_t aucWfdSecondarySinkMac[MAC_ADDR_LEN];
	uint32_t u4WfdAdvancedFlag;
	/* Group 1 64 bytes */
	uint8_t aucWfdLocalIp[4];
	uint16_t u2WfdLifetimeAc2;	/* Unit is 2 TU */
	uint16_t u2WfdLifetimeAc3;	/* Unit is 2 TU */
	uint16_t u2WfdCounterThreshold;	/* Unit is ms */
	uint8_t aucReverved2[54];
	/* Group 2 64 bytes */
	uint8_t aucReverved3[64];
	/* Group 3 64 bytes */
	uint8_t aucReverved4[64];
	uint32_t u4LinkScore;
};

#endif

struct MSG_P2P_ACTIVE_DEV_BSS {
	struct MSG_HDR rMsgHdr;
};

/*-------------------- P2P FSM ACTION STRUCT ---------------------*/

struct P2P_OFF_CHNL_TX_REQ_INFO {
	struct LINK_ENTRY rLinkEntry;
	struct MSDU_INFO *prMgmtTxMsdu;
	u_int8_t fgNoneCckRate;
	struct RF_CHANNEL_INFO rChannelInfo;	/* Off channel TX. */
	enum ENUM_CHNL_EXT eChnlExt;
	/* See if driver should keep at the same channel. */
	u_int8_t fgIsWaitRsp;
	uint64_t u8Cookie; /* cookie used to match with supplicant */
	uint32_t u4Duration; /* wait time for tx request */
	uint8_t ucBssIndex;
};

struct P2P_DEV_FSM_INFO {
	uint8_t ucBssIndex;
	/* State related. */
	enum ENUM_P2P_DEV_STATE eCurrentState;

	/* Channel related. */
	struct P2P_CHNL_REQ_INFO rChnlReqInfo;

	/* Scan related. */
	struct P2P_SCAN_REQ_INFO rScanReqInfo;

	/* Mgmt tx related. */
	struct P2P_MGMT_TX_REQ_INFO rMgmtTxInfo;

	/* FSM Timer */
	struct TIMER rP2pFsmTimeoutTimer;

	/* Packet filter for P2P module. */
	uint32_t u4P2pPacketFilter;

	/* Queued p2p action frame */
	struct P2P_QUEUED_ACTION_FRAME rQueuedActionFrame;
};

struct MSG_P2P_NETDEV_REGISTER {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	u_int8_t fgIsEnable;
	uint8_t ucMode;
};

#if CFG_SUPPORT_WFD
struct MSG_WFD_CONFIG_SETTINGS_CHANGED {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	struct WFD_CFG_SETTINGS *prWfdCfgSettings;
};
#endif

struct MSG_P2P_ACS_REQUEST {
	struct MSG_HDR rMsgHdr; /* Must be the first member */
	uint8_t ucRoleIdx;
	u_int8_t fgIsHtEnable;
	u_int8_t fgIsHt40Enable;
	u_int8_t fgIsVhtEnable;
	enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw;
	enum P2P_VENDOR_ACS_HW_MODE eHwMode;
	uint32_t u4NumChannel;
	struct RF_CHANNEL_INFO arChannelListInfo[1];
};

/*========================= Initial ============================*/

uint8_t p2pDevFsmInit(IN struct ADAPTER *prAdapter);

void p2pDevFsmUninit(IN struct ADAPTER *prAdapter);

/*========================= FUNCTIONs ============================*/

void
p2pDevFsmStateTransition(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo,
		IN enum ENUM_P2P_DEV_STATE eNextState);

void p2pDevFsmRunEventAbort(IN struct ADAPTER *prAdapter,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo);

void p2pDevFsmRunEventTimeout(IN struct ADAPTER *prAdapter,
		IN unsigned long ulParamPtr);

/*================ Message Event =================*/
void p2pDevFsmRunEventScanRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);
void p2pDevFsmRunEventScanAbort(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void
p2pDevFsmRunEventScanDone(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo);

void p2pDevFsmRunEventChannelRequest(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void p2pDevFsmRunEventChannelAbort(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void
p2pDevFsmRunEventChnlGrant(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr,
		IN struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo);

void p2pDevFsmRunEventMgmtTx(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

uint32_t
p2pDevFsmRunEventMgmtFrameTxDone(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo,
		IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void p2pDevFsmRunEventMgmtFrameRegister(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

/* /////////////////////////////// */

void p2pDevFsmRunEventActiveDevBss(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

void
p2pDevFsmNotifyP2pRx(IN struct ADAPTER *prAdapter, uint8_t p2pFrameType,
		u_int8_t *prFgBufferFrame);

void p2pDevFsmRunEventTxCancelWait(IN struct ADAPTER *prAdapter,
		IN struct MSG_HDR *prMsgHdr);

