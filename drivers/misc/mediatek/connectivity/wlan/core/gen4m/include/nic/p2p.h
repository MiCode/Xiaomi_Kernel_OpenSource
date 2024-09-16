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
 ** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/include/nic/p2p.h#3
 */


#ifndef _P2P_H
#define _P2P_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/* refer to 'Config Methods' in WPS */
#define WPS_CONFIG_USBA                 0x0001
#define WPS_CONFIG_ETHERNET             0x0002
#define WPS_CONFIG_LABEL                0x0004
#define WPS_CONFIG_DISPLAY              0x0008
#define WPS_CONFIG_EXT_NFC              0x0010
#define WPS_CONFIG_INT_NFC              0x0020
#define WPS_CONFIG_NFC                  0x0040
#define WPS_CONFIG_PBC                  0x0080
#define WPS_CONFIG_KEYPAD               0x0100

/* refer to 'Device Password ID' in WPS */
#define WPS_DEV_PASSWORD_ID_PIN         0x0000
#define WPS_DEV_PASSWORD_ID_USER        0x0001
#define WPS_DEV_PASSWORD_ID_MACHINE     0x0002
#define WPS_DEV_PASSWORD_ID_REKEY       0x0003
#define WPS_DEV_PASSWORD_ID_PUSHBUTTON  0x0004
#define WPS_DEV_PASSWORD_ID_REGISTRAR   0x0005

#define P2P_DEVICE_TYPE_NUM         2
#define P2P_DEVICE_NAME_LENGTH      32
#define P2P_NETWORK_NUM             8
#define P2P_MEMBER_NUM              8

/* Device Capability Definition. */
#define P2P_MAXIMUM_NOA_COUNT                       8

#define P2P_MAX_AKM_SUITES 2

#define P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE 51	/* Contains 6 sub-band. */

/* Memory Size Definition. */
#define P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE           768
#define WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE           300

#define P2P_WILDCARD_SSID           "DIRECT-"

/* Device Charactoristic. */
/* 1000 is too short , the deauth would block in the queue */
#define P2P_AP_CHNL_HOLD_TIME_MS 5000
#define P2P_DEFAULT_LISTEN_CHANNEL                   1

#if (CFG_SUPPORT_DFS_MASTER == 1)
#define P2P_AP_CAC_WEATHER_CHNL_HOLD_TIME_MS (600*1000)
#endif

#define P2P_DEAUTH_TIMEOUT_TIME_MS 1000

#define P2P_SAA_RETRY_COUNT     5

#define AP_DEFAULT_CHANNEL_2G     6
#define AP_DEFAULT_CHANNEL_5G     36

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

#if DBG
#define ASSERT_BREAK(_exp) \
	{ \
		if (!(_exp)) { \
			ASSERT(FALSE); \
			break; \
		} \
	}

#else
#define ASSERT_BREAK(_exp)
#endif

#define p2pChangeMediaState(_prAdapter, _prP2pBssInfo, _eNewMediaState) \
	(_prP2pBssInfo->eConnectionState = (_eNewMediaState))

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */
/* if driver need wait for a longger time when do p2p connection */
enum ENUM_P2P_CONNECT_STATE {
	P2P_CNN_NORMAL = 0,
	P2P_CNN_GO_NEG_REQ,
	P2P_CNN_GO_NEG_RESP,
	P2P_CNN_GO_NEG_CONF,
	P2P_CNN_INVITATION_REQ,
	P2P_CNN_INVITATION_RESP,
	P2P_CNN_DEV_DISC_REQ,
	P2P_CNN_DEV_DISC_RESP,
	P2P_CNN_PROV_DISC_REQ,
	P2P_CNN_PROV_DISC_RESP
};

struct P2P_INFO {
	uint32_t u4DeviceNum;
	enum ENUM_P2P_CONNECT_STATE eConnState;
	struct EVENT_P2P_DEV_DISCOVER_RESULT
		arP2pDiscoverResult[CFG_MAX_NUM_BSS_LIST];
	uint8_t *pucCurrIePtr;
	/* A common pool for IE of all scan results. */
	uint8_t aucCommIePool[CFG_MAX_COMMON_IE_BUF_LEN];
	uint8_t ucExtendChanFlag;
};

enum ENUM_P2P_PEER_TYPE {
	ENUM_P2P_PEER_GROUP,
	ENUM_P2P_PEER_DEVICE,
	ENUM_P2P_PEER_NUM
};

struct P2P_DEVICE_INFO {
	uint8_t aucDevAddr[PARAM_MAC_ADDR_LEN];
	uint8_t aucIfAddr[PARAM_MAC_ADDR_LEN];
	uint8_t ucDevCapabilityBitmap;
	int32_t i4ConfigMethod;
	uint8_t aucPrimaryDeviceType[8];
	uint8_t aucSecondaryDeviceType[8];
	uint8_t aucDeviceName[P2P_DEVICE_NAME_LENGTH];
};

struct P2P_GROUP_INFO {
	struct PARAM_SSID rGroupID;
	struct P2P_DEVICE_INFO rGroupOwnerInfo;
	uint8_t ucMemberNum;
	struct P2P_DEVICE_INFO arMemberInfo[P2P_MEMBER_NUM];
};

struct P2P_NETWORK_INFO {
	enum ENUM_P2P_PEER_TYPE eNodeType;

	union {
		struct P2P_GROUP_INFO rGroupInfo;
		struct P2P_DEVICE_INFO rDeviceInfo;
	} node;
};

struct P2P_NETWORK_LIST {
	uint8_t ucNetworkNum;
	struct P2P_NETWORK_INFO rP2PNetworkInfo[P2P_NETWORK_NUM];
};

struct P2P_DISCONNECT_INFO {
	uint8_t ucRole;
	uint8_t ucRsv[3];
};

struct P2P_SSID_STRUCT {
	uint8_t aucSsid[32];
	uint8_t ucSsidLen;
};

enum ENUM_SCAN_REASON {
	SCAN_REASON_UNKNOWN = 0,
	SCAN_REASON_CONNECT,
	SCAN_REASON_STARTAP,
	SCAN_REASON_ACS,
	SCAN_REASON_NUM,
};

struct P2P_SCAN_REQ_INFO {
	enum ENUM_SCAN_TYPE eScanType;
	enum ENUM_SCAN_CHANNEL eChannelSet;
	uint16_t u2PassiveDewellTime;
	uint8_t ucSeqNumOfScnMsg;
	u_int8_t fgIsAbort;
	u_int8_t fgIsScanRequest;
	uint8_t ucNumChannelList;
	struct RF_CHANNEL_INFO
		arScanChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	uint32_t u4BufLength;
	uint8_t aucIEBuf[MAX_IE_LENGTH];
	uint8_t ucSsidNum;
	enum ENUM_SCAN_REASON eScanReason;
	/* Currently we can only take one SSID scan request */
	struct P2P_SSID_STRUCT arSsidStruct[SCN_SSID_MAX_NUM];
};

enum P2P_VENDOR_ACS_HW_MODE {
	P2P_VENDOR_ACS_HW_MODE_11B,
	P2P_VENDOR_ACS_HW_MODE_11G,
	P2P_VENDOR_ACS_HW_MODE_11A,
	P2P_VENDOR_ACS_HW_MODE_11AD,
	P2P_VENDOR_ACS_HW_MODE_11ANY
};

struct P2P_ACS_REQ_INFO {
	uint8_t ucRoleIdx;
	u_int8_t fgIsProcessing;
	u_int8_t fgIsHtEnable;
	u_int8_t fgIsHt40Enable;
	u_int8_t fgIsVhtEnable;
	enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw;
	enum P2P_VENDOR_ACS_HW_MODE eHwMode;
	uint32_t u4LteSafeChnMask_2G;
	uint32_t u4LteSafeChnMask_5G_1;
	uint32_t u4LteSafeChnMask_5G_2;

	/* output only */
	uint8_t ucPrimaryCh;
	uint8_t ucSecondCh;
	uint8_t ucCenterFreqS1;
	uint8_t ucCenterFreqS2;
};

struct P2P_CHNL_REQ_INFO {
	struct LINK rP2pChnlReqLink;
	u_int8_t fgIsChannelRequested;
	uint8_t ucSeqNumOfChReq;
	uint64_t u8Cookie;
	uint8_t ucReqChnlNum;
	enum ENUM_BAND eBand;
	enum ENUM_CHNL_EXT eChnlSco;
	uint8_t ucOriChnlNum;
	enum ENUM_CHANNEL_WIDTH eChannelWidth;	/*VHT operation ie */
	uint8_t ucCenterFreqS1;
	uint8_t ucCenterFreqS2;
	enum ENUM_BAND eOriBand;
	enum ENUM_CHNL_EXT eOriChnlSco;
	uint32_t u4MaxInterval;
	enum ENUM_CH_REQ_TYPE eChnlReqType;
#if CFG_SUPPORT_NFC_BEAM_PLUS
	uint32_t NFC_BEAM;	/*NFC Beam + Indication */
#endif
};

/* Glubal Connection Settings. */
struct P2P_CONNECTION_SETTINGS {
	/*UINT_8 ucRfChannelListSize;*/
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE
	/*UINT_8 aucChannelEntriesField[P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE];*/
#endif

	u_int8_t fgIsApMode;
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	u_int8_t fgIsWPSMode;
#endif
};

struct NOA_TIMING {
	u_int8_t fgIsInUse;	/* Indicate if this entry is in use or not */
	uint8_t ucCount;		/* Count */

	uint8_t aucReserved[2];

	uint32_t u4Duration;	/* Duration */
	uint32_t u4Interval;	/* Interval */
	uint32_t u4StartTime;	/* Start Time */
};

struct P2P_SPECIFIC_BSS_INFO {
	/* For GO(AP) Mode - Compose TIM IE */
	/*UINT_16 u2SmallestAID;*//* TH3 multiple P2P */
	/*UINT_16 u2LargestAID;*//* TH3 multiple P2P */
	/*UINT_8 ucBitmapCtrl;*//* TH3 multiple P2P */
	/* UINT_8 aucPartialVirtualBitmap[MAX_LEN_TIM_PARTIAL_BMP]; */

	/* For GC/GO OppPS */
	u_int8_t fgEnableOppPS;
	uint16_t u2CTWindow;

	/* For GC/GO NOA */
	uint8_t ucNoAIndex;
	uint8_t ucNoATimingCount;	/* Number of NoA Timing */
	struct NOA_TIMING arNoATiming[P2P_MAXIMUM_NOA_COUNT];

	u_int8_t fgIsNoaAttrExisted;

	/* For P2P Device */
	/* TH3 multiple P2P */	/* Regulatory Class for channel. */
	/*UINT_8 ucRegClass;*/
	/* Linten Channel only on channels 1, 6 and 11 in the 2.4 GHz. */
	/*UINT_8 ucListenChannel;*//* TH3 multiple P2P */

	/* Operating Channel, should be one of channel */
	/* list in p2p connection settings. */
	uint8_t ucPreferredChannel;
	enum ENUM_CHNL_EXT eRfSco;
	enum ENUM_BAND eRfBand;

	/* Extended Listen Timing. */
	uint16_t u2AvailabilityPeriod;
	uint16_t u2AvailabilityInterval;

	uint16_t u2AttributeLen;
	uint8_t aucAttributesCache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	/*UINT_16 u2WscAttributeLen;*//* TH3 multiple P2P */
	/* TH3 multiple P2P */
	/*UINT_8 aucWscAttributesCache[WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];*/

	/*UINT_8 aucGroupID[MAC_ADDR_LEN];*//* TH3 multiple P2P */
	uint16_t u2GroupSsidLen;
	uint8_t aucGroupSsid[ELEM_MAX_LEN_SSID];

	struct PARAM_CUSTOM_NOA_PARAM_STRUCT rNoaParam;
	struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT rOppPsParam;

	uint32_t u4KeyMgtSuiteCount;
	uint32_t au4KeyMgtSuite[P2P_MAX_AKM_SUITES];

	uint16_t u2WpaIeLen;
	uint8_t aucWpaIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_WPA];

	uint16_t u2RsnIeLen;
	uint8_t aucRsnIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_RSN];
};

struct P2P_QUEUED_ACTION_FRAME {
	uint8_t ucRoleIdx;
	int32_t u4Freq;
	uint8_t *prHeader;
	uint16_t u2Length;
};

struct P2P_MGMT_TX_REQ_INFO {
	struct LINK rTxReqLink;
	struct MSDU_INFO *prMgmtTxMsdu;
	u_int8_t fgIsWaitRsp;
};

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

#endif	/*_P2P_H */
