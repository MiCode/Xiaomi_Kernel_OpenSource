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
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/include/nic/p2p.h#3
*/

#ifndef _P2P_H
#define _P2P_H

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
#define P2P_MAXIMUM_CLIENT_COUNT                    10
#define P2P_MAXIMUM_NOA_COUNT                       8

#define P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE            51	/* Contains 6 sub-band. */

/* Memory Size Definition. */
#define P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE           768
#define WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE           300

#define P2P_WILDCARD_SSID           "DIRECT-"

/* Device Charactoristic. */
#define P2P_AP_CHNL_HOLD_TIME_MS 5000	/* 1000 is too short , the deauth would block in the queue */
#define P2P_DEFAULT_LISTEN_CHANNEL                   1

/*******************************************************************************
 *                                 M A C R O S
 ********************************************************************************
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

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */

struct _P2P_INFO_T {
	UINT_32 u4DeviceNum;
	EVENT_P2P_DEV_DISCOVER_RESULT_T arP2pDiscoverResult[CFG_MAX_NUM_BSS_LIST];
	PUINT_8 pucCurrIePtr;
	UINT_8 aucCommIePool[CFG_MAX_COMMON_IE_BUF_LEN];	/* A common pool for IE of all
								 *scan results. */
};

typedef enum {
	ENUM_P2P_PEER_GROUP,
	ENUM_P2P_PEER_DEVICE,
	ENUM_P2P_PEER_NUM
} ENUM_P2P_PEER_TYPE, *P_ENUM_P2P_PEER_TYPE;

typedef struct _P2P_DEVICE_INFO {
	UINT_8 aucDevAddr[PARAM_MAC_ADDR_LEN];
	UINT_8 aucIfAddr[PARAM_MAC_ADDR_LEN];
	UINT_8 ucDevCapabilityBitmap;
	INT_32 i4ConfigMethod;
	UINT_8 aucPrimaryDeviceType[8];
	UINT_8 aucSecondaryDeviceType[8];
	UINT_8 aucDeviceName[P2P_DEVICE_NAME_LENGTH];
} P2P_DEVICE_INFO, *P_P2P_DEVICE_INFO;

typedef struct _P2P_GROUP_INFO {
	PARAM_SSID_T rGroupID;
	P2P_DEVICE_INFO rGroupOwnerInfo;
	UINT_8 ucMemberNum;
	P2P_DEVICE_INFO arMemberInfo[P2P_MEMBER_NUM];
} P2P_GROUP_INFO, *P_P2P_GROUP_INFO;

typedef struct _P2P_NETWORK_INFO {
	ENUM_P2P_PEER_TYPE eNodeType;

	union {
		P2P_GROUP_INFO rGroupInfo;
		P2P_DEVICE_INFO rDeviceInfo;
	} node;
} P2P_NETWORK_INFO, *P_P2P_NETWORK_INFO;

typedef struct _P2P_NETWORK_LIST {
	UINT_8 ucNetworkNum;
	P2P_NETWORK_INFO rP2PNetworkInfo[P2P_NETWORK_NUM];
} P2P_NETWORK_LIST, *P_P2P_NETWORK_LIST;

typedef struct _P2P_DISCONNECT_INFO {
	UINT_8 ucRole;
	UINT_8 ucRsv[3];
} P2P_DISCONNECT_INFO, *P_P2P_DISCONNECT_INFO;

typedef struct _P2P_SSID_STRUCT_T {
	UINT_8 aucSsid[32];
	UINT_8 ucSsidLen;
} P2P_SSID_STRUCT_T, *P_P2P_SSID_STRUCT_T;

typedef struct _P2P_SCAN_REQ_INFO_T {
	ENUM_SCAN_TYPE_T eScanType;
	ENUM_SCAN_CHANNEL eChannelSet;
	UINT_16 u2PassiveDewellTime;
	UINT_8 ucSeqNumOfScnMsg;
	BOOLEAN fgIsAbort;
	BOOLEAN fgIsScanRequest;
	UINT_8 ucNumChannelList;
	RF_CHANNEL_INFO_T arScanChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_32 u4BufLength;
	UINT_8 aucIEBuf[MAX_IE_LENGTH];
	UINT_8 ucSsidNum;
	P2P_SSID_STRUCT_T arSsidStruct[SCN_SSID_MAX_NUM];	/* Currently we can only take one SSID scan request */
} P2P_SCAN_REQ_INFO_T, *P_P2P_SCAN_REQ_INFO_T;

typedef struct _P2P_CHNL_REQ_INFO_T {
	LINK_T rP2pChnlReqLink;
	BOOLEAN fgIsChannelRequested;
	UINT_8 ucSeqNumOfChReq;
	UINT_64 u8Cookie;
	UINT_8 ucReqChnlNum;
	ENUM_BAND_T eBand;
	ENUM_CHNL_EXT_T eChnlSco;
	UINT_8 ucOriChnlNum;
	ENUM_CHANNEL_WIDTH_T eChannelWidth;	/*VHT operation ie */
	UINT_8 ucCenterFreqS1;
	UINT_8 ucCenterFreqS2;
	ENUM_BAND_T eOriBand;
	ENUM_CHNL_EXT_T eOriChnlSco;
	UINT_32 u4MaxInterval;
	ENUM_CH_REQ_TYPE_T eChnlReqType;
#if CFG_SUPPORT_NFC_BEAM_PLUS
	UINT_32 NFC_BEAM;	/*NFC Beam + Indication */
#endif
} P2P_CHNL_REQ_INFO_T, *P_P2P_CHNL_REQ_INFO_T;

/* Glubal Connection Settings. */
struct _P2P_CONNECTION_SETTINGS_T {
	UINT_8 ucRfChannelListSize;
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE
	UINT_8 aucChannelEntriesField[P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE];
#endif

	BOOLEAN fgIsApMode;
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	BOOLEAN fgIsWPSMode;
#endif
};

typedef struct _NOA_TIMING_T {
	BOOLEAN fgIsInUse;	/* Indicate if this entry is in use or not */
	UINT_8 ucCount;		/* Count */

	UINT_8 aucReserved[2];

	UINT_32 u4Duration;	/* Duration */
	UINT_32 u4Interval;	/* Interval */
	UINT_32 u4StartTime;	/* Start Time */
} NOA_TIMING_T, *P_NOA_TIMING_T;

struct _P2P_SPECIFIC_BSS_INFO_T {
	/* For GO(AP) Mode - Compose TIM IE */
	UINT_16 u2SmallestAID;
	UINT_16 u2LargestAID;
	UINT_8 ucBitmapCtrl;
	/* UINT_8                  aucPartialVirtualBitmap[MAX_LEN_TIM_PARTIAL_BMP]; */

	/* For GC/GO OppPS */
	BOOLEAN fgEnableOppPS;
	UINT_16 u2CTWindow;

	/* For GC/GO NOA */
	UINT_8 ucNoAIndex;
	UINT_8 ucNoATimingCount;	/* Number of NoA Timing */
	NOA_TIMING_T arNoATiming[P2P_MAXIMUM_NOA_COUNT];

	BOOLEAN fgIsNoaAttrExisted;

	/* For P2P Device */
	UINT_8 ucRegClass;	/* Regulatory Class for channel. */
	UINT_8 ucListenChannel;	/* Linten Channel only on channels 1, 6 and 11
				 *in the 2.4 GHz. */

	UINT_8 ucPreferredChannel;	/* Operating Channel, should be one of channel
					 *list in p2p connection settings. */
	ENUM_CHNL_EXT_T eRfSco;
	ENUM_BAND_T eRfBand;

	/* Extended Listen Timing. */
	UINT_16 u2AvailabilityPeriod;
	UINT_16 u2AvailabilityInterval;

	UINT_16 u2AttributeLen;
	UINT_8 aucAttributesCache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	UINT_16 u2WscAttributeLen;
	UINT_8 aucWscAttributesCache[WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	UINT_8 aucGroupID[MAC_ADDR_LEN];
	UINT_16 u2GroupSsidLen;
	UINT_8 aucGroupSsid[ELEM_MAX_LEN_SSID];

	PARAM_CUSTOM_NOA_PARAM_STRUCT_T rNoaParam;
	PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T rOppPsParam;

	UINT_16 u2WpaIeLen;
	UINT_8 aucWpaIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_WPA];
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
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

#endif	/*_P2P_H */
