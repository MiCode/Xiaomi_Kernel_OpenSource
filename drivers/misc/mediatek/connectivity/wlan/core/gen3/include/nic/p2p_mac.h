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
 * ! \file   "p2p_mac.h"
 *  \brief  Brief description.
 *
 *  Detail description.
 */

#ifndef _P2P_MAC_H
#define _P2P_MAC_H

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

#define ACTION_PUBLIC_WIFI_DIRECT                   9
#define ACTION_GAS_INITIAL_REQUEST                 10
#define ACTION_GAS_INITIAL_RESPONSE               11
#define ACTION_GAS_COMEBACK_REQUEST           12
#define ACTION_GAS_COMEBACK_RESPONSE         13

/* P2P 4.2.8.1 - P2P Public Action Frame Type. */
#define P2P_PUBLIC_ACTION_GO_NEGO_REQ               0
#define P2P_PUBLIC_ACTION_GO_NEGO_RSP               1
#define P2P_PUBLIC_ACTION_GO_NEGO_CFM               2
#define P2P_PUBLIC_ACTION_INVITATION_REQ            3
#define P2P_PUBLIC_ACTION_INVITATION_RSP            4
#define P2P_PUBLIC_ACTION_DEV_DISCOVER_REQ          5
#define P2P_PUBLIC_ACTION_DEV_DISCOVER_RSP          6
#define P2P_PUBLIC_ACTION_PROV_DISCOVERY_REQ        7
#define P2P_PUBLIC_ACTION_PROV_DISCOVERY_RSP        8

/* P2P 4.2.9.1 - P2P Action Frame Type */
#define P2P_ACTION_NOTICE_OF_ABSENCE                0
#define P2P_ACTION_P2P_PRESENCE_REQ                 1
#define P2P_ACTION_P2P_PRESENCE_RSP                 2
#define P2P_ACTION_GO_DISCOVER_REQ                  3

#define P2P_PUBLIC_ACTION_FRAME_LEN                (WLAN_MAC_MGMT_HEADER_LEN + 8)
#define P2P_ACTION_FRAME_LEN                       (WLAN_MAC_MGMT_HEADER_LEN + 7)

/*******************************************************************************
 *                                 M A C R O S
 ********************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */

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

/* --------------- WPS Data Element Definitions --------------- */
/* P2P 4.2.2 - General WSC Attribute */
#define WSC_ATTRI_HDR_LEN                                   4	/* ID(2 octet) + Length(2 octets) */
#define WSC_ATTRI_MAX_LEN_VERSION                           1
#define WSC_ATTRI_MAX_LEN_DEVICE_PASSWORD_ID                2
#define WSC_ATTRI_LEN_CONFIG_METHOD                         2

/* --------------- WFA P2P IE --------------- */
/* P2P 4.1.1 - P2P IE format */
#define P2P_OUI_TYPE_LEN                            4
#define P2P_IE_OUI_HDR                              (ELEM_HDR_LEN + P2P_OUI_TYPE_LEN)	/*
											 * == OFFSET_OF(IE_P2P_T,
											 * aucP2PAttributes[0])
											 */

/* P2P 4.1.1 - General P2P Attribute */
#define P2P_ATTRI_HDR_LEN                           3	/* ID(1 octet) + Length(2 octets) */

/* P2P 4.1.1 - P2P Attribute ID definitions */
#define P2P_ATTRI_ID_STATUS                                 0
#define P2P_ATTRI_ID_REASON_CODE                            1
#define P2P_ATTRI_ID_P2P_CAPABILITY                         2
#define P2P_ATTRI_ID_P2P_DEV_ID                             3
#define P2P_ATTRI_ID_GO_INTENT                              4
#define P2P_ATTRI_ID_CFG_TIMEOUT                            5
#define P2P_ATTRI_ID_LISTEN_CHANNEL                         6
#define P2P_ATTRI_ID_P2P_GROUP_BSSID                        7
#define P2P_ATTRI_ID_EXT_LISTEN_TIMING                      8
#define P2P_ATTRI_ID_INTENDED_P2P_IF_ADDR                   9
#define P2P_ATTRI_ID_P2P_MANAGEABILITY                      10
#define P2P_ATTRI_ID_CHANNEL_LIST                           11
#define P2P_ATTRI_ID_NOTICE_OF_ABSENCE                      12
#define P2P_ATTRI_ID_P2P_DEV_INFO                           13
#define P2P_ATTRI_ID_P2P_GROUP_INFO                         14
#define P2P_ATTRI_ID_P2P_GROUP_ID                           15
#define P2P_ATTRI_ID_P2P_INTERFACE                          16
#define P2P_ATTRI_ID_OPERATING_CHANNEL                      17
#define P2P_ATTRI_ID_INVITATION_FLAG                        18
#define P2P_ATTRI_ID_VENDOR_SPECIFIC                        221

/* Maximum Length of P2P Attributes */
#define P2P_ATTRI_MAX_LEN_STATUS                            1	/* 0 */
#define P2P_ATTRI_MAX_LEN_REASON_CODE                       1	/* 1 */
#define P2P_ATTRI_MAX_LEN_P2P_CAPABILITY                    2	/* 2 */
#define P2P_ATTRI_MAX_LEN_P2P_DEV_ID                        6	/* 3 */
#define P2P_ATTRI_MAX_LEN_GO_INTENT                         1	/* 4 */
#define P2P_ATTRI_MAX_LEN_CFG_TIMEOUT                       2	/* 5 */
#define P2P_ATTRI_MAX_LEN_LISTEN_CHANNEL                    5	/* 6 */
#define P2P_ATTRI_MAX_LEN_P2P_GROUP_BSSID                   6	/* 7 */
#define P2P_ATTRI_MAX_LEN_EXT_LISTEN_TIMING                 4	/* 8 */
#define P2P_ATTRI_MAX_LEN_INTENDED_P2P_IF_ADDR              6	/* 9 */
#define P2P_ATTRI_MAX_LEN_P2P_MANAGEABILITY                 1	/* 10 */
/* #define P2P_ATTRI_MAX_LEN_CHANNEL_LIST                      3 + (n* (2 + num_of_ch)) *//* 11 */
#define P2P_ATTRI_LEN_CHANNEL_LIST                                  3	/* 11 */
#define P2P_ATTRI_LEN_CHANNEL_ENTRY                                  2	/* 11 */

#define P2P_MAXIMUM_ATTRIBUTE_LEN                   251

/* P2P 4.1.2 - P2P Status definitions */
#define P2P_STATUS_SUCCESS                                  0
#define P2P_STATUS_FAIL_INFO_IS_CURRENTLY_UNAVAILABLE   1
#define P2P_STATUS_FAIL_INCOMPATIBLE_PARAM                  2
#define P2P_STATUS_FAIL_LIMIT_REACHED                       3
#define P2P_STATUS_FAIL_INVALID_PARAM                       4
#define P2P_STATUS_FAIL_UNABLE_ACCOMMODATE_REQ              5
#define P2P_STATUS_FAIL_PREVIOUS_PROTOCOL_ERR               6
#define P2P_STATUS_FAIL_NO_COMMON_CHANNELS                  7
#define P2P_STATUS_FAIL_UNKNOWN_P2P_GROUP                   8
#define P2P_STATUS_FAIL_SAME_INTENT_VALUE_15                9
#define P2P_STATUS_FAIL_INCOMPATIBLE_PROVISION_METHOD       10
#define P2P_STATUS_FAIL_REJECTED_BY_USER                    11

/* P2P 4.1.14 - CTWindow and OppPS Parameters definitions */
#define P2P_CTW_OPPPS_PARAM_OPPPS_FIELD                     BIT(7)
#define P2P_CTW_OPPPS_PARAM_CTWINDOW_MASK                   BITS(0, 6)
/* Action frame categories (IEEE 802.11-2007, 7.3.1.11, Table 7-24) */
#define WLAN_ACTION_SPECTRUM_MGMT 0
#define WLAN_ACTION_QOS 1
#define WLAN_ACTION_DLS 2
#define WLAN_ACTION_BLOCK_ACK 3
#define WLAN_ACTION_PUBLIC 4
#define WLAN_ACTION_RADIO_MEASUREMENT 5
#define WLAN_ACTION_FT 6
#define WLAN_ACTION_HT 7
#define WLAN_ACTION_SA_QUERY 8
#define WLAN_ACTION_PROTECTED_DUAL 9
#define WLAN_ACTION_WNM 10
#define WLAN_ACTION_UNPROTECTED_WNM 11
#define WLAN_ACTION_TDLS 12
#define WLAN_ACTION_SELF_PROTECTED 15
#define WLAN_ACTION_WMM 17 /* WMM Specification 1.1 */
#define WLAN_ACTION_VENDOR_SPECIFIC 127
#define P2P_IE_VENDOR_TYPE 0x506f9a09
#define WFD_IE_VENDOR_TYPE 0x506f9a0a

/* Public action codes */
#define WLAN_PA_20_40_BSS_COEX 0
#define WLAN_PA_VENDOR_SPECIFIC 9
#define WLAN_PA_GAS_INITIAL_REQ 10
#define WLAN_PA_GAS_INITIAL_RESP 11
#define WLAN_PA_GAS_COMEBACK_REQ 12
#define WLAN_PA_GAS_COMEBACK_RESP 13
#define WLAN_TDLS_DISCOVERY_RESPONSE 14

/* P2P public action frames */
typedef enum p2p_action_frame_type {
	P2P_GO_NEG_REQ = 0,
	P2P_GO_NEG_RESP = 1,
	P2P_GO_NEG_CONF = 2,
	P2P_INVITATION_REQ = 3,
	P2P_INVITATION_RESP = 4,
	P2P_DEV_DISC_REQ = 5,
	P2P_DEV_DISC_RESP = 6,
	P2P_PROV_DISC_REQ = 7,
	P2P_PROV_DISC_RESP = 8
} ENUM_P2P_ACTION_TYPE;
/* --------------- WFA P2P IE and Attributes --------------- */

/* P2P 4.1.1 - P2P Information Element */
typedef struct _IE_P2P_T {
	UINT_8 ucId;		/* Element ID */
	UINT_8 ucLength;	/* Length */
	UINT_8 aucOui[3];	/* OUI */
	UINT_8 ucOuiType;	/* OUI Type */
	UINT_8 aucP2PAttributes[1];	/* P2P Attributes */
} __KAL_ATTRIB_PACKED__ IE_P2P_T, *P_IE_P2P_T;

/* P2P 4.1.1 - General WSC Attribute */
typedef struct _WSC_ATTRIBUTE_T {
	UINT_16 u2Id;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucBody[1];	/* Body field */
} __KAL_ATTRIB_PACKED__ WSC_ATTRIBUTE_T, *P_WSC_ATTRIBUTE_T;

/* P2P 4.1.2 - P2P Status Attribute */
typedef struct _P2P_ATTRI_STATUS_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucStatusCode;	/* Status Code */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_STATUS_T, *P_P2P_ATTRI_STATUS_T;

/* P2P 4.1.10 - Extended Listen Timing Attribute */
typedef struct _P2P_ATTRI_EXT_LISTEN_TIMING_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_16 u2AvailPeriod;	/* Availability Period */
	UINT_16 u2AvailInterval;	/* Availability Interval */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_EXT_LISTEN_TIMING_T, *P_P2P_ATTRI_EXT_LISTEN_TIMING_T;

/* P2P 4.2.8.2 P2P Public Action Frame Format */
typedef struct _P2P_PUBLIC_ACTION_FRAME_T {
	/* MAC header */
	UINT_16 u2FrameCtrl;	/* Frame Control */
	UINT_16 u2Duration;	/* Duration */
	UINT_8 aucDestAddr[MAC_ADDR_LEN];	/* DA */
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* BSSID */
	UINT_16 u2SeqCtrl;	/* Sequence Control */
	/* P2P Public Action Frame Body */
	UINT_8 ucCategory;	/* Category, 0x04 */
	UINT_8 ucAction;	/* Action Value, 0x09 */
	UINT_8 aucOui[3];	/* 0x50, 0x6F, 0x9A */
	UINT_8 ucOuiType;	/* 0x09 */
	UINT_8 ucOuiSubtype;	/*
				 * GO Nego Req/Rsp/Cfm, P2P Invittion Req/Rsp, Device Discoverability
				 * Req/Rsp
				 */
	UINT_8 ucDialogToken;	/* Dialog Token. */
	UINT_8 aucInfoElem[1];	/* P2P IE, WSC IE. */
} __KAL_ATTRIB_PACKED__ P2P_PUBLIC_ACTION_FRAME_T, *P_P2P_PUBLIC_ACTION_FRAME_T;

/* P2P 4.2.9.1 -  General Action Frame Format. */
typedef struct _P2P_ACTION_FRAME_T {
	/* MAC header */
	UINT_16 u2FrameCtrl;	/* Frame Control */
	UINT_16 u2Duration;	/* Duration */
	UINT_8 aucDestAddr[MAC_ADDR_LEN];	/* DA */
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* BSSID */
	UINT_16 u2SeqCtrl;	/* Sequence Control */
	/* P2P Action Frame Body */
	UINT_8 ucCategory;	/* 0x7F */
	UINT_8 aucOui[3];	/* 0x50, 0x6F, 0x9A */
	UINT_8 ucOuiType;	/* 0x09 */
	UINT_8 ucOuiSubtype;	/*  */
	UINT_8 ucDialogToken;
	UINT_8 aucInfoElem[1];
} __KAL_ATTRIB_PACKED__ P2P_ACTION_FRAME_T, *P_P2P_ACTION_FRAME_T;

/* P2P C.1 GAS Public Action Initial Request Frame Format */
typedef struct _GAS_PUBLIC_ACTION_INITIAL_REQUEST_FRAME_T {
	/* MAC header */
	UINT_16 u2FrameCtrl;	/* Frame Control */
	UINT_16 u2Duration;	/* Duration */
	UINT_8 aucDestAddr[MAC_ADDR_LEN];	/* DA */
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* BSSID */
	UINT_16 u2SeqCtrl;	/* Sequence Control */
	/* P2P Public Action Frame Body */
	UINT_8 ucCategory;	/* Category, 0x04 */
	UINT_8 ucAction;	/* Action Value, 0x09 */
	UINT_8 ucDialogToken;	/* Dialog Token. */
	UINT_8 aucInfoElem[1];	/* Advertisement IE. */
} __KAL_ATTRIB_PACKED__ GAS_PUBLIC_ACTION_INITIAL_REQUEST_FRAME_T, *P_GAS_PUBLIC_ACTION_INITIAL_REQUEST_FRAME_T;

/* P2P C.2 GAS Public Action Initial Response Frame Format */
typedef struct _GAS_PUBLIC_ACTION_INITIAL_RESPONSE_FRAME_T {
	/* MAC header */
	UINT_16 u2FrameCtrl;	/* Frame Control */
	UINT_16 u2Duration;	/* Duration */
	UINT_8 aucDestAddr[MAC_ADDR_LEN];	/* DA */
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* BSSID */
	UINT_16 u2SeqCtrl;	/* Sequence Control */
	/* P2P Public Action Frame Body */
	UINT_8 ucCategory;	/* Category, 0x04 */
	UINT_8 ucAction;	/* Action Value, 0x09 */
	UINT_8 ucDialogToken;	/* Dialog Token. */
	UINT_16 u2StatusCode;	/* Initial Response. */
	UINT_16 u2ComebackDelay;	/* Initial Response. *//* In unit of TU. */
	UINT_8 aucInfoElem[1];	/* Advertisement IE. */
} __KAL_ATTRIB_PACKED__ GAS_PUBLIC_ACTION_INITIAL_RESPONSE_FRAME_T, *P_GAS_PUBLIC_ACTION_INITIAL_RESPONSE_FRAME_T;

/* P2P C.3-1 GAS Public Action Comeback Request Frame Format */
typedef struct _GAS_PUBLIC_ACTION_COMEBACK_REQUEST_FRAME_T {
	/* MAC header */
	UINT_16 u2FrameCtrl;	/* Frame Control */
	UINT_16 u2Duration;	/* Duration */
	UINT_8 aucDestAddr[MAC_ADDR_LEN];	/* DA */
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* BSSID */
	UINT_16 u2SeqCtrl;	/* Sequence Control */
	/* P2P Public Action Frame Body */
	UINT_8 ucCategory;	/* Category, 0x04 */
	UINT_8 ucAction;	/* Action Value, 0x09 */
	UINT_8 ucDialogToken;	/* Dialog Token. */
} __KAL_ATTRIB_PACKED__ GAS_PUBLIC_ACTION_COMEBACK_REQUEST_FRAME_T, *P_GAS_PUBLIC_ACTION_COMEBACK_REQUEST_FRAME_T;

/* P2P C.3-2 GAS Public Action Comeback Response Frame Format */
typedef struct _GAS_PUBLIC_ACTION_COMEBACK_RESPONSE_FRAME_T {
	/* MAC header */
	UINT_16 u2FrameCtrl;	/* Frame Control */
	UINT_16 u2Duration;	/* Duration */
	UINT_8 aucDestAddr[MAC_ADDR_LEN];	/* DA */
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	UINT_8 aucBSSID[MAC_ADDR_LEN];	/* BSSID */
	UINT_16 u2SeqCtrl;	/* Sequence Control */
	/* P2P Public Action Frame Body */
	UINT_8 ucCategory;	/* Category, 0x04 */
	UINT_8 ucAction;	/* Action Value, 0x09 */
	UINT_8 ucDialogToken;	/* Dialog Token. */
	UINT_16 u2StatusCode;	/* Comeback Response. */
	UINT_8 ucFragmentID;	/*Comeback Response. */
	UINT_16 u2ComebackDelay;	/* Comeback Response. */
	UINT_8 aucInfoElem[1];	/* Advertisement IE. */
} __KAL_ATTRIB_PACKED__ GAS_PUBLIC_ACTION_COMEBACK_RESPONSE_FRAME_T, *P_GAS_PUBLIC_ACTION_COMEBACK_RESPONSE_FRAME_T;

typedef struct _P2P_SD_VENDER_SPECIFIC_CONTENT_T {
	/* Service Discovery Vendor-specific Content. */
	UINT_8 ucOuiSubtype;	/* 0x09 */
	UINT_16 u2ServiceUpdateIndicator;
	UINT_8 aucServiceTLV[1];
} __KAL_ATTRIB_PACKED__ P2P_SD_VENDER_SPECIFIC_CONTENT_T, *P_P2P_SD_VENDER_SPECIFIC_CONTENT_T;

typedef struct _P2P_SERVICE_REQUEST_TLV_T {
	UINT_16 u2Length;
	UINT_8 ucServiceProtocolType;
	UINT_8 ucServiceTransID;
	UINT_8 aucQueryData[1];
} __KAL_ATTRIB_PACKED__ P2P_SERVICE_REQUEST_TLV_T, *P_P2P_SERVICE_REQUEST_TLV_T;

typedef struct _P2P_SERVICE_RESPONSE_TLV_T {
	UINT_16 u2Length;
	UINT_8 ucServiceProtocolType;
	UINT_8 ucServiceTransID;
	UINT_8 ucStatusCode;
	UINT_8 aucResponseData[1];
} __KAL_ATTRIB_PACKED__ P2P_SERVICE_RESPONSE_TLV_T, *P_P2P_SERVICE_RESPONSE_TLV_T;

/* P2P 4.1.1 - General P2P Attribute */
typedef struct _P2P_ATTRIBUTE_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucBody[1];	/* Body field */
} __KAL_ATTRIB_PACKED__ P2P_ATTRIBUTE_T, ATTRIBUTE_HDR_T, *P_P2P_ATTRIBUTE_T, *P_ATTRIBUTE_HDR_T;

/* P2P 4.1.14 - Notice of Absence Attribute */
typedef struct _P2P_ATTRI_NOA_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucIndex;		/* Index */
	UINT_8 ucCTWOppPSParam;	/* CTWindow and OppPS Parameters */
	UINT_8 aucNoADesc[1];	/* NoA Descriptor */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_NOA_T, *P_P2P_ATTRI_NOA_T;

typedef struct _NOA_DESCRIPTOR_T {
	UINT_8 ucCountType;	/* Count/Type */
	UINT_32 u4Duration;	/* Duration */
	UINT_32 u4Interval;	/* Interval */
	UINT_32 u4StartTime;	/* Start Time */
} __KAL_ATTRIB_PACKED__ NOA_DESCRIPTOR_T, *P_NOA_DESCRIPTOR_T;
typedef struct _CHANNEL_ENTRY_FIELD_T {
	UINT_8 ucRegulatoryClass;	/* Regulatory Class */
	UINT_8 ucNumberOfChannels;	/* Number Of Channels */
	UINT_8 aucChannelList[1];	/* Channel List */
} __KAL_ATTRIB_PACKED__ CHANNEL_ENTRY_FIELD_T, *P_CHANNEL_ENTRY_FIELD_T;

enum p2p_attr_id {
	P2P_ATTR_STATE = 0,
};

#endif
