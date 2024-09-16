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
** Id: @(#)
*/

/*! \file   "scan.h"
*    \brief
*
*/


#ifndef _SCAN_H
#define _SCAN_H

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
/*! Maximum buffer size of SCAN list */
#define SCN_MAX_BUFFER_SIZE                 (CFG_MAX_NUM_BSS_LIST * ALIGN_4(sizeof(BSS_DESC_T)))

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
#define SCN_ROAM_MAX_BUFFER_SIZE		(CFG_MAX_NUM_ROAM_BSS_LIST * ALIGN_4(sizeof(ROAM_BSS_DESC_T)))
#endif

#define SCN_RM_POLICY_EXCLUDE_SPECIFIC_SSID     BIT(4)	/* Remove SCAN result except the connected one. */

#define SCN_RM_POLICY_EXCLUDE_CONNECTED     BIT(0)	/* Remove SCAN result except the connected one. */
#define SCN_RM_POLICY_TIMEOUT               BIT(1)	/* Remove the timeout one */
#define SCN_RM_POLICY_OLDEST_HIDDEN         BIT(2)	/* Remove the oldest one with hidden ssid */
#define SCN_RM_POLICY_SMART_WEAKEST         BIT(3)	/* If there are more than half BSS which has the
							 * same ssid as connection setting, remove the
							 * weakest one from them
							 * Else remove the weakest one.
							 */
/* Remove entire SCAN result */
#define SCN_RM_POLICY_ENTIRE                BIT(5)

#define SCN_BSS_DESC_SAME_SSID_THRESHOLD    3	/* This is used by POLICY SMART WEAKEST,
						 * If exceed this value, remove weakest BSS_DESC_T
						 * with same SSID first in large network.
						 */
#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
#define REMOVE_TIMEOUT_TWO_DAY     (60*60*24*2)
#endif

#if 1
#define SCN_BSS_DESC_REMOVE_TIMEOUT_SEC     30
#define SCN_BSS_DESC_STALE_SEC				20	/* Scan Request Timeout */
#else
#define SCN_BSS_DESC_REMOVE_TIMEOUT_SEC     5	/* Second. */
					      /* This is used by POLICY TIMEOUT,
					       * If exceed this value, remove timeout BSS_DESC_T.
					       */

#endif

#define SCN_PROBE_DELAY_MSEC                0

#define SCN_ADHOC_BSS_DESC_TIMEOUT_SEC      5	/* Second. */

#define SCN_NLO_NETWORK_CHANNEL_NUM         (4)

#define SCAN_DONE_DIFFERENCE                3

/*----------------------------------------------------------------------------*/
/* MSG_SCN_SCAN_REQ                                                           */
/*----------------------------------------------------------------------------*/
#define SCAN_REQ_SSID_WILDCARD              BIT(0)
#define SCAN_REQ_SSID_P2P_WILDCARD          BIT(1)
#define SCAN_REQ_SSID_SPECIFIED             BIT(2)

/*----------------------------------------------------------------------------*/
/* Support Multiple SSID SCAN                                                 */
/*----------------------------------------------------------------------------*/
#define SCN_SSID_MAX_NUM                    CFG_SCAN_SSID_MAX_NUM
#define SCN_SSID_MATCH_MAX_NUM              CFG_SCAN_SSID_MATCH_MAX_NUM

#if CFG_SUPPORT_AGPS_ASSIST
#define SCN_AGPS_AP_LIST_MAX_NUM					32
#endif

#define SCN_BSS_JOIN_FAIL_THRESOLD				5
#define SCN_BSS_JOIN_FAIL_CNT_RESET_SEC			15
#define SCN_BSS_JOIN_FAIL_RESET_STEP				2

#if CFG_SUPPORT_BATCH_SCAN
/*----------------------------------------------------------------------------*/
/* SCAN_BATCH_REQ                                                             */
/*----------------------------------------------------------------------------*/
#define SCAN_BATCH_REQ_START                BIT(0)
#define SCAN_BATCH_REQ_STOP                 BIT(1)
#define SCAN_BATCH_REQ_RESULT               BIT(2)
#endif

#define SCAN_NLO_CHECK_SSID_ONLY    0x00000001
#define SCAN_NLO_DEFAULT_INTERVAL           30000

#define	SCN_CTRL_SCAN_CHANNEL_LISTEN_TIME_ENABLE		BIT(1)
#define	SCN_CTRL_IGNORE_AIS_FIX_CHANNEL		BIT(1)
#define	SCN_CTRL_ENABLE		BIT(0)

#define SCN_CTRL_DEFAULT_SCAN_CTRL		SCN_CTRL_IGNORE_AIS_FIX_CHANNEL

#define SCN_SCAN_DONE_PRINT_BUFFER_LENGTH		200
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_SCAN_TYPE_T {
	SCAN_TYPE_PASSIVE_SCAN = 0,
	SCAN_TYPE_ACTIVE_SCAN,
	SCAN_TYPE_NUM
} ENUM_SCAN_TYPE_T, *P_ENUM_SCAN_TYPE_T;

typedef enum _ENUM_SCAN_STATE_T {
	SCAN_STATE_IDLE = 0,
	SCAN_STATE_SCANNING,
	SCAN_STATE_NUM
} ENUM_SCAN_STATE_T;

typedef enum _ENUM_FW_SCAN_STATE_T {
	FW_SCAN_STATE_IDLE = 0,	/* 0 */
	FW_SCAN_STATE_SCAN_START,	/* 1 */
	FW_SCAN_STATE_REQ_CHANNEL,	/* 2 */
	FW_SCAN_STATE_SET_CHANNEL,	/* 3 */
	FW_SCAN_STATE_DELAYED_ACTIVE_PROB_REQ,	/* 4 */
	FW_SCAN_STATE_ACTIVE_PROB_REQ,	/* 5 */
	FW_SCAN_STATE_LISTEN,	/* 6 */
	FW_SCAN_STATE_SCAN_DONE,	/* 7 */
	FW_SCAN_STATE_NLO_START,	/* 8 */
	FW_SCAN_STATE_NLO_HIT_CHECK,	/* 9 */
	FW_SCAN_STATE_NLO_STOP,	/* 10 */
	FW_SCAN_STATE_BATCH_START,	/* 11 */
	FW_SCAN_STATE_BATCH_CHECK,	/* 12 */
	FW_SCAN_STATE_BATCH_STOP,	/* 13 */
	FW_SCAN_STATE_NUM	/* 14 */
} ENUM_FW_SCAN_STATE_T;

typedef enum _ENUM_SCAN_CHANNEL_T {
	SCAN_CHANNEL_FULL = 0,
	SCAN_CHANNEL_2G4,
	SCAN_CHANNEL_5G,
	SCAN_CHANNEL_P2P_SOCIAL,
	SCAN_CHANNEL_SPECIFIED,
	SCAN_CHANNEL_NUM
} ENUM_SCAN_CHANNEL, *P_ENUM_SCAN_CHANNEL;

typedef struct _MSG_SCN_FSM_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_32 u4Dummy;
} MSG_SCN_FSM_T, *P_MSG_SCN_FSM_T;

/*----------------------------------------------------------------------------*/
/* BSS Descriptors                                                            */
/*----------------------------------------------------------------------------*/
struct _BSS_DESC_T {
	LINK_ENTRY_T rLinkEntry;

	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* For IBSS, the SrcAddr is different from BSSID */

	BOOLEAN fgIsConnecting;	/* If we are going to connect to this BSS
				 * (JOIN or ROAMING to another BSS), don't
				 * remove this record from BSS List.
				 */
	BOOLEAN fgIsConnected;	/* If we have connected to this BSS (NORMAL_TR),
				 * don't removed this record from BSS list.
				 */

	BOOLEAN fgIsHiddenSSID;	/* When this flag is TRUE, means the SSID
				 * of this BSS is not known yet.
				 */
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];

	OS_SYSTIME rUpdateTime;

	ENUM_BSS_TYPE_T eBSSType;

	UINT_16 u2CapInfo;

	UINT_16 u2BeaconInterval;
	UINT_16 u2ATIMWindow;

	UINT_16 u2OperationalRateSet;
	UINT_16 u2BSSBasicRateSet;
	BOOLEAN fgIsUnknownBssBasicRate;

	BOOLEAN fgIsERPPresent;
	BOOLEAN fgIsHTPresent;
	BOOLEAN fgIsVHTPresent;

	UINT_8 ucPhyTypeSet;	/* Available PHY Type Set of this BSS */
	UINT_8 ucVhtCapNumSoundingDimensions; /* record from bcn or probe response*/

	UINT_8 ucChannelNum;

	ENUM_CHNL_EXT_T eSco;	/* Record bandwidth for association process */
				/*Some AP will send association resp by 40MHz BW */
	ENUM_CHANNEL_WIDTH_T eChannelWidth;	/*VHT operation ie */
	UINT_8 ucCenterFreqS1;
	UINT_8 ucCenterFreqS2;
	ENUM_BAND_T eBand;

	UINT_8 ucDTIMPeriod;

	BOOLEAN fgIsLargerTSF;	/* This BSS's TimeStamp is larger than us(TCL == 1 in RX_STATUS_T) */

	UINT_8 ucRCPI; /* WF0 */
	UINT_8 ucRCPI1; /* WF1 */

	UINT_8 ucWmmFlag;	/* A flag to indicate this BSS's WMM capability */

	/*! \brief The srbiter Search State will matched the scan result,
	*   and saved the selected cipher and akm, and report the score,
	*   for arbiter join state, join module will carry this target BSS
	*   to rsn generate ie function, for gen wpa/rsn ie
	*/
	UINT_32 u4RsnSelectedGroupCipher;
	UINT_32 u4RsnSelectedPairwiseCipher;
	UINT_32 u4RsnSelectedAKMSuite;

	UINT_16 u2RsnCap;

	RSN_INFO_T rRSNInfo;
	RSN_INFO_T rWPAInfo;
#if 1				/* CFG_SUPPORT_WAPI */
	WAPI_INFO_T rIEWAPI;
	BOOL fgIEWAPI;
#endif
	BOOL fgIERSN;
	BOOL fgIEWPA;

	/*! \brief RSN parameters selected for connection */
	/*! \brief The Select score for final AP selection,
	 *  0, no sec, 1,2,3 group cipher is WEP, TKIP, CCMP
	 */
	UINT_8 ucEncLevel;

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2PPresent;
	BOOLEAN fgIsP2PReport;	/* TRUE: report to upper layer */
	P_P2P_DEVICE_DESC_T prP2pDesc;

	UINT_8 aucIntendIfAddr[MAC_ADDR_LEN];	/* For IBSS, the SrcAddr is different from BSSID */
	/* UINT_8 ucDevCapabilityBitmap; *//* Device Capability Attribute. (P2P_DEV_CAPABILITY_XXXX) */
	/* UINT_8 ucGroupCapabilityBitmap; *//* Group Capability Attribute. (P2P_GROUP_CAPABILITY_XXXX) */

	LINK_T rP2pDeviceList;

/* P_LINK_T prP2pDeviceList; */

	/* For
	 *    1. P2P Capability.
	 *    2. P2P Device ID. ( in aucSrcAddr[] )
	 *    3. NOA   (TODO:)
	 *    4. Extend Listen Timing. (Probe Rsp)  (TODO:)
	 *    5. P2P Device Info. (Probe Rsp)
	 *    6. P2P Group Info. (Probe Rsp)
	 */
#endif

	BOOLEAN fgIsIEOverflow;	/* The received IE length exceed the maximum IE buffer size */
	UINT_16 u2RawLength;	/* The byte count of aucRawBuf[] */
	UINT_16 u2IELength;	/* The byte count of aucIEBuf[] */

	ULARGE_INTEGER u8TimeStamp;	/* Place u8TimeStamp before aucIEBuf[1] to force DW align */
	UINT_8 aucRawBuf[CFG_RAW_BUFFER_SIZE];
	UINT_8 aucIEBuf[CFG_IE_BUFFER_SIZE];
	UINT_8 ucJoinFailureCount;
	OS_SYSTIME rJoinFailTime;
	struct AIS_BLACKLIST_ITEM *prBlack;
};

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
struct _ROAM_BSS_DESC_T {
	LINK_ENTRY_T rLinkEntry;
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];
	OS_SYSTIME rUpdateTime;
};
#endif

typedef struct _SCAN_PARAM_T {	/* Used by SCAN FSM */
	/* Active or Passive */
	ENUM_SCAN_TYPE_T eScanType;

	/* Network Type */
	UINT_8 ucBssIndex;

	/* Specified SSID Type */
	UINT_8 ucSSIDType;
	UINT_8 ucSSIDNum;

	/* Length of Specified SSID */
	UINT_8 ucSpecifiedSSIDLen[SCN_SSID_MAX_NUM];

	/* Specified SSID */
	UINT_8 aucSpecifiedSSID[SCN_SSID_MAX_NUM][ELEM_MAX_LEN_SSID];

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgFindSpecificDev;	/* P2P: Discovery Protocol */
	UINT_8 aucDiscoverDevAddr[MAC_ADDR_LEN];
	BOOLEAN fgIsDevType;
	P2P_DEVICE_TYPE_T rDiscoverDevType;

	/* TODO: Find Specific Device Type. */
#endif				/* CFG_SUPPORT_P2P */

	UINT_16 u2ChannelDwellTime;
	UINT_16 u2TimeoutValue;

	BOOLEAN fgIsObssScan;
	BOOLEAN fgIsScanV2;

	/* Run time flags */
	UINT_16 u2ProbeDelayTime;

	/* channel information */
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];

	/* Feedback information */
	UINT_8 ucSeqNum;

	/* Information Element */
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];

} SCAN_PARAM_T, *P_SCAN_PARAM_T;

typedef struct _NLO_PARAM_T {	/* Used by SCAN FSM */
	SCAN_PARAM_T rScanParam;

	/* NLO */
	BOOLEAN fgStopAfterIndication;
	UINT_8 ucFastScanIteration;
	UINT_16 u2FastScanPeriod;
	UINT_16 u2SlowScanPeriod;

	/* Match SSID */
	UINT_8 ucMatchSSIDNum;
	UINT_8 ucMatchSSIDLen[SCN_SSID_MATCH_MAX_NUM];
	UINT_8 aucMatchSSID[SCN_SSID_MATCH_MAX_NUM][ELEM_MAX_LEN_SSID];

	UINT_8 aucCipherAlgo[SCN_SSID_MATCH_MAX_NUM];
	UINT_16 au2AuthAlgo[SCN_SSID_MATCH_MAX_NUM];
	UINT_8 aucChannelHint[SCN_SSID_MATCH_MAX_NUM][SCN_NLO_NETWORK_CHANNEL_NUM];

} NLO_PARAM_T, *P_NLO_PARAM_T;

typedef struct _SCAN_INFO_T {
	ENUM_SCAN_STATE_T eCurrentState;	/* Store the STATE variable of SCAN FSM */

	OS_SYSTIME rLastScanCompletedTime;

	SCAN_PARAM_T rScanParam;
	NLO_PARAM_T rNloParam;

	UINT_32 u4NumOfBssDesc;

	UINT_8 aucScanBuffer[SCN_MAX_BUFFER_SIZE];

	LINK_T rBSSDescList;

	LINK_T rFreeBSSDescList;

	LINK_T rPendingMsgList;
#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	UINT_8 aucScanRoamBuffer[SCN_ROAM_MAX_BUFFER_SIZE];
	LINK_T rRoamFreeBSSDescList;
	LINK_T rRoamBSSDescList;
#endif
	/* Sparse Channel Detection */
	BOOLEAN fgIsSparseChannelValid;
	RF_CHANNEL_INFO_T rSparseChannel;

	/* NLO scanning state tracking */
	BOOLEAN fgNloScanning;

	/*channel idle count # Mike */
	UINT_8			ucSparseChannelArrayValidNum;
	UINT_8			aucReserved[3];
	UINT_8			aucChannelNum[64];
	UINT_16			au2ChannelIdleTime[64];
	UINT_8			aucChannelFlag[64];
	UINT_8			aucChannelMDRDYCnt[64];

} SCAN_INFO_T, *P_SCAN_INFO_T;

/* Incoming Mailbox Messages */
typedef struct _MSG_SCN_SCAN_REQ_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	ENUM_SCAN_TYPE_T eScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDLength;
	UINT_8 aucSSID[PARAM_MAX_LEN_SSID];
	UINT_16 u2ChannelDwellTime;	/* ms unit */
	UINT_16 u2TimeoutValue;	/* ms unit */
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} MSG_SCN_SCAN_REQ, *P_MSG_SCN_SCAN_REQ;

typedef struct _MSG_SCN_SCAN_REQ_V2_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	ENUM_SCAN_TYPE_T eScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDNum;
	P_PARAM_SSID_T prSsid;
	UINT_16 u2ProbeDelay;
	UINT_16 u2ChannelDwellTime;	/* In TU. 1024us. */
	UINT_16 u2TimeoutValue;	/* ms unit */
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} MSG_SCN_SCAN_REQ_V2, *P_MSG_SCN_SCAN_REQ_V2;

typedef struct _MSG_SCN_SCAN_CANCEL_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	BOOLEAN fgIsChannelExt;
} MSG_SCN_SCAN_CANCEL, *P_MSG_SCN_SCAN_CANCEL;

typedef struct _tagOFFLOAD_NETWORK {
	UINT_8 aucSsid[ELEM_MAX_LEN_SSID];
	UINT_8 ucSsidLen;
	UINT_8 ucUnicastCipher;	/* ENUM_NLO_CIPHER_ALGORITHM */
	UINT_16 u2AuthAlgo;	/* ENUM_NLO_AUTH_ALGORITHM */
	UINT_8 aucChannelList[SCN_NLO_NETWORK_CHANNEL_NUM];
} OFFLOAD_NETWORK, *P_OFFLOAD_NETWORK;

typedef struct _MSG_SCN_NLO_REQ_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	BOOLEAN fgStopAfterIndication;
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_32 u4FastScanPeriod;
	UINT_32 u4FastScanIterations;
	UINT_32 u4SlowScanPeriod;
	UINT_32 u4NumOfEntries;
	OFFLOAD_NETWORK arNetwork[CFG_SCAN_SSID_MAX_NUM];
} MSG_SCN_NLO_REQ, *P_MSG_SCN_NLO_REQ;

typedef struct _MSG_SCN_NLO_CANCEL_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
} MSG_SCN_NLO_CANCEL, *P_MSG_SCN_NLO_CANCEL;

/* Outgoing Mailbox Messages */
typedef enum _ENUM_SCAN_STATUS_T {
	SCAN_STATUS_DONE = 0,
	SCAN_STATUS_CANCELLED,
	SCAN_STATUS_FAIL,
	SCAN_STATUS_BUSY,
	SCAN_STATUS_NUM
} ENUM_SCAN_STATUS, *P_ENUM_SCAN_STATUS;

typedef struct _MSG_SCN_SCAN_DONE_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	ENUM_SCAN_STATUS eScanStatus;
} MSG_SCN_SCAN_DONE, *P_MSG_SCN_SCAN_DONE;

#if CFG_SUPPORT_AGPS_ASSIST
typedef enum {
	AGPS_PHY_A,
	AGPS_PHY_B,
	AGPS_PHY_G,
} AP_PHY_TYPE;

typedef struct _AGPS_AP_INFO_T {
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	INT_16 i2ApRssi;	/* -127..128 */
	UINT_16 u2Channel;	/* 0..256 */
	AP_PHY_TYPE ePhyType;
} AGPS_AP_INFO_T, *P_AGPS_AP_INFO_T;

typedef struct _AGPS_AP_LIST_T {
	UINT_8 ucNum;
	AGPS_AP_INFO_T arApInfo[SCN_AGPS_AP_LIST_MAX_NUM];
} AGPS_AP_LIST_T, *P_AGPS_AP_LIST_T;
#endif

typedef enum _ENUM_NLO_STATUS_T {
	NLO_STATUS_FOUND = 0,
	NLO_STATUS_CANCELLED,
	NLO_STATUS_FAIL,
	NLO_STATUS_BUSY,
	NLO_STATUS_NUM
} ENUM_NLO_STATUS, *P_ENUM_NLO_STATUS;

typedef struct _MSG_SCN_NLO_DONE_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	ENUM_NLO_STATUS eNloStatus;
} MSG_SCN_NLO_DONE, *P_MSG_SCN_NLO_DONE;

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
/*----------------------------------------------------------------------------*/
/* Routines in scan.c                                                         */
/*----------------------------------------------------------------------------*/
VOID scnInit(IN P_ADAPTER_T prAdapter);

VOID scnUninit(IN P_ADAPTER_T prAdapter);

/* BSS-DESC Search */
P_BSS_DESC_T scanSearchBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

P_BSS_DESC_T
scanSearchBssDescByBssidAndSsid(IN P_ADAPTER_T prAdapter,
				IN UINT_8 aucBSSID[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

#if CFG_SUPPORT_CFG80211_AUTH
P_BSS_DESC_T
scanSearchBssDescByBssidAndChanNum(IN P_ADAPTER_T prAdapter,
	IN UINT_8 aucBSSID[], IN BOOLEAN fgCheckChanNum,
	IN UINT_8 ucChannelNum);
#endif
P_BSS_DESC_T scanSearchBssDescByTA(IN P_ADAPTER_T prAdapter, IN UINT_8 aucSrcAddr[]);

P_BSS_DESC_T
scanSearchBssDescByTAAndSsid(IN P_ADAPTER_T prAdapter,
			     IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

/* BSS-DESC Search - Alternative */
P_BSS_DESC_T
scanSearchExistingBssDesc(IN P_ADAPTER_T prAdapter,
			  IN ENUM_BSS_TYPE_T eBSSType, IN UINT_8 aucBSSID[], IN UINT_8 aucSrcAddr[]);

P_BSS_DESC_T
scanSearchExistingBssDescWithSsid(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BSS_TYPE_T eBSSType,
				  IN UINT_8 aucBSSID[],
				  IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

/* BSS-DESC Allocation */
P_BSS_DESC_T scanAllocateBssDesc(IN P_ADAPTER_T prAdapter);

/* BSS-DESC Removal */
VOID scanRemoveBssDescsByPolicy(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemovePolicy);

VOID scanRemoveBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

VOID scanRemoveBssDescByBandAndNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_BAND_T eBand, IN UINT_8 ucBssIndex);

/* BSS-DESC State Change */
VOID scanRemoveConnFlagOfBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

/* BSS-DESC Insertion - ALTERNATIVE */
P_BSS_DESC_T scanAddToBssDesc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS scanProcessBeaconAndProbeResp(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb);

VOID
scanBuildProbeReqFrameCommonIEs(IN P_MSDU_INFO_T prMsduInfo,
				IN PUINT_8 pucDesiredSsid, IN UINT_32 u4DesiredSsidLen, IN UINT_16 u2SupportedRateSet);

WLAN_STATUS scanSendProbeReqFrames(IN P_ADAPTER_T prAdapter, IN P_SCAN_PARAM_T prScanParam);

VOID scanUpdateBssDescForSearch(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);

P_BSS_DESC_T scanSearchBssDescByPolicy(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

WLAN_STATUS scanAddScanResult(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc, IN P_SW_RFB_T prSwRfb);

VOID scanReportBss2Cfg80211(IN P_ADAPTER_T prAdapter, IN ENUM_BSS_TYPE_T eBSSType, IN P_BSS_DESC_T SpecificprBssDesc);

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
P_ROAM_BSS_DESC_T scanSearchRoamBssDescBySsid(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);
P_ROAM_BSS_DESC_T scanAllocateRoamBssDesc(IN P_ADAPTER_T prAdapter);
VOID scanAddToRoamBssDesc(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);
VOID scanSearchBssDescOfRoamSsid(IN P_ADAPTER_T prAdapter);
VOID scanRemoveRoamBssDescsByTime(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemoveTime);
#endif
/*----------------------------------------------------------------------------*/
/* Routines in scan_fsm.c                                                     */
/*----------------------------------------------------------------------------*/
VOID scnFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_SCAN_STATE_T eNextState);

/*----------------------------------------------------------------------------*/
/* Command Routines                                                           */
/*----------------------------------------------------------------------------*/
VOID scnSendScanReq(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReqV2(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* RX Event Handling                                                          */
/*----------------------------------------------------------------------------*/
VOID scnEventScanDone(IN P_ADAPTER_T prAdapter, IN P_EVENT_SCAN_DONE prScanDone, BOOLEAN fgIsNewVersion);

VOID scnEventNloDone(IN P_ADAPTER_T prAdapter, IN P_EVENT_NLO_DONE_T prNloDone);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Handling                                                   */
/*----------------------------------------------------------------------------*/
VOID scnFsmMsgStart(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmMsgAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmHandleScanMsg(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ prScanReqMsg);

VOID scnFsmHandleScanMsgV2(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg);

VOID scnFsmRemovePendingMsg(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSeqNum, IN UINT_8 ucBssIndex);

VOID scnFsmNloMsgStart(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmNloMsgAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmHandleNloMsg(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_NLO_REQ prNloReqMsg);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Generation                                                 */
/*----------------------------------------------------------------------------*/
VOID
scnFsmGenerateScanDoneMsg(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucSeqNum, IN UINT_8 ucBssIndex, IN ENUM_SCAN_STATUS eScanStatus);

/*----------------------------------------------------------------------------*/
/* Query for sparse channel                                                   */
/*----------------------------------------------------------------------------*/
BOOLEAN scnQuerySparseChannel(IN P_ADAPTER_T prAdapter, P_ENUM_BAND_T prSparseBand, PUINT_8 pucSparseChannel);

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
BOOLEAN
scnFsmSchedScanRequest(IN P_ADAPTER_T prAdapter,
		       IN UINT_8 ucSsidNum,
		       IN P_PARAM_SSID_T prSsid, IN UINT_32 u4IeLength, IN PUINT_8 pucIe, IN UINT_16 u2Interval);

BOOLEAN scnFsmSchedScanStopRequest(IN P_ADAPTER_T prAdapter);

#if CFG_SUPPORT_PASSPOINT
P_BSS_DESC_T scanSearchBssDescByBssidAndLatestUpdateTime(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_AGPS_ASSIST
VOID scanReportScanResultToAgps(P_ADAPTER_T prAdapter);
#endif

#if CFG_SCAN_CHANNEL_SPECIFIED
static inline bool is_valid_scan_chnl_cnt(UINT_8 num)
{
	return (num && num < MAXIMUM_OPERATION_CHANNEL_LIST);
}
#endif
#endif /* _SCAN_H */
