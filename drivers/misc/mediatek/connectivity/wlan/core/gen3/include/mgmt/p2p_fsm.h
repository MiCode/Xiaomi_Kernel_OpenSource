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
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/include/mgmt/p2p_fsm.h#23
*/

/*! \file   p2p_fsm.h
 *  \brief  Declaration of functions and finite state machine for P2P Module.
 *
 *  Declaration of functions and finite state machine for P2P Module.
 */

#ifndef _P2P_FSM_H
#define _P2P_FSM_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ********************************************************************************
 */
#define CID52_53_54         0

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ********************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 ********************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */
typedef enum _ENUM_P2P_STATE_T {
	P2P_STATE_IDLE = 0,
	P2P_STATE_SCAN,
	P2P_STATE_AP_CHANNEL_DETECT,
	P2P_STATE_REQING_CHANNEL,
	P2P_STATE_CHNL_ON_HAND,	/* Requesting Channel to Send Specific Frame. */
	P2P_STATE_GC_JOIN,	/* Sending Specific Frame. May extending channel by other event. */
	P2P_STATE_NUM
} ENUM_P2P_STATE_T, *P_ENUM_P2P_STATE_T;

struct _P2P_FSM_INFO_T {
	/* State related. */
	ENUM_P2P_STATE_T ePreviousState;
	ENUM_P2P_STATE_T eCurrentState;

	/* Channel related. */
	P2P_CHNL_REQ_INFO_T rChnlReqInfo;

	/* Scan related. */
	P2P_SCAN_REQ_INFO_T rScanReqInfo;

	/* Connection related. */
	P2P_CONNECTION_REQ_INFO_T rConnReqInfo;

	/* Mgmt tx related. */
	P2P_MGMT_TX_REQ_INFO_T rMgmtTxInfo;

	/* Beacon related. */
	P2P_BEACON_UPDATE_INFO_T rBcnContentInfo;

	/* Probe Response related. */
	P2P_PROBE_RSP_UPDATE_INFO_T rProbeRspContentInfo;

	/* Assoc Rsp related. */
	P2P_ASSOC_RSP_UPDATE_INFO_T rAssocRspContentInfo;

	/* GC Join related. */
	P2P_JOIN_INFO_T rJoinInfo;

	/* Auto channel selection related. */
	struct P2P_ACS_REQ_INFO rAcsReqInfo;

	/* FSM Timer */
	TIMER_T rP2pFsmTimeoutTimer;

	/* GC Target BSS. */
	P_BSS_DESC_T prTargetBss;

	/* GC Connection Request. */
	BOOLEAN fgIsConnectionRequested;

	BOOLEAN fgIsApMode;

	/* Channel grant interval. */
	UINT_32 u4GrantInterval;

	/* Packet filter for P2P module. */
	UINT_32 u4P2pPacketFilter;

	/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv Prepare for use vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
	/* Msg event queue. */
	LINK_T rMsgEventQueue;
};

/*---------------- Messages -------------------*/

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */
VOID p2pFsmStateTransition(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState);

VOID p2pFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo);

VOID p2pFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventStartAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventBeaconUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventStopAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventChannelRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventChannelAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventDissolve(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

WLAN_STATUS
p2pFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T
			      rTxDoneStatus);

VOID p2pFsmRunEventMgmtFrameRegister(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

/*3 --------------- WFA P2P DEFAULT PARAMETERS --------------- */
#define P2P_WILDCARD_SSID                           "DIRECT-"
#define P2P_WILDCARD_SSID_LEN                       7
#define P2P_GROUP_ID_LEN                            9
#define P2P_DRIVER_VERSION                          2	/* Update when needed. */
#define P2P_DEFAULT_DEV_NAME                        "Wireless Client"
#define P2P_DEFAULT_DEV_NAME_LEN                    15
#define P2P_DEFAULT_PRIMARY_CATEGORY_ID             10
#define P2P_DEFAULT_PRIMARY_SUB_CATEGORY_ID         5
#define P2P_DEFAULT_CONFIG_METHOD                   (WPS_ATTRI_CFG_METHOD_PUSH_BUTTON | WPS_ATTRI_CFG_METHOD_KEYPAD | \
						     WPS_ATTRI_CFG_METHOD_DISPLAY)
#define P2P_MAX_SUPPORTED_SEC_DEV_TYPE_COUNT        0	/* NOTE(Kevin): Shall <= 16 */
#define P2P_MAX_SUPPORTED_CHANNEL_LIST_COUNT         13
#define P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE            51	/* Contains 6 sub-band. */
#define P2P_GC_MAX_CACHED_SEC_DEV_TYPE_COUNT        8	/* NOTE(Kevin): Shall <= 16 */
#define P2P_CTWINDOW_DEFAULT                        25	/* in TU=(1024usec) */
/* P2P 3.1.2.1.3 - Find Phase */
#define P2P_MAX_DISCOVERABLE_INTERVAL    8	/* 3 */
#define P2P_MIN_DISCOVERABLE_INTERVAL    5	/* 1 */
#define P2P_LISTEN_SCAN_UNIT                     100	/* MS */
/* FSM Time Related constrain. */
#define P2P_SERACH_STATE_PERIOD_MS              1000	/* Deprecated. */
#define P2P_GO_CHANNEL_STAY_INTERVAL             1000
#define P2P_GO_NEGO_TIMEOUT_MS                          500
#define P2P_CONNECTION_TIMEOUT_SEC                   120
#define P2P_INVITAION_TIMEOUT_MS                         500	/* Timeout Wait Invitation Resonse. */
#define P2P_PROVISION_DISCOVERY_TIMEOUT_MS     500	/* Timeout Wait Provision Discovery Resonse. */
/* 3 */
/*#define P2P_ATTRI_MAX_LEN_NOTICE_OF_ABSENCE                 2 + (n* (13)) */ /* 12 */
#define P2P_ATTRI_MAX_LEN_NOTICE_OF_ABSENCE                 (2 + (P2P_MAXIMUM_NOA_COUNT * (13)))	/* 12 */
#define P2P_ATTRI_MAX_LEN_P2P_DEV_INFO                      (17 + (8 * (8)) + 36)	/* 13 */
/* #define P2P_ATTRI_MAX_LEN_P2P_GROUP_INFO                    n* (25 + (m* (8)) + 32) */ /* 14 */
#define P2P_ATTRI_MAX_LEN_P2P_GROUP_ID                      38	/* 15 */
#define P2P_ATTRI_MAX_LEN_P2P_INTERFACE                     253	/* 7 + 6* [0~41] */ /* 16 */
#if CID52_53_54
#define P2P_ATTRI_MAX_LEN_OPERATING_CHANNEL                 5	/* 17 */
#else
#define P2P_ATTRI_MAX_LEN_OPERATING_CHANNEL                 5	/* 17 */
#endif
#define P2P_ATTRI_MAX_LEN_INVITATION_FLAGS                          1	/* 18 */
/* P2P 4.1.3 - P2P Minor Reason Code definitions */
#define P2P_REASON_SUCCESS                                  0
#define P2P_REASON_DISASSOCIATED_DUE_CROSS_CONNECTION       1
#define P2P_REASON_DISASSOCIATED_DUE_UNMANAGEABLE           2
#define P2P_REASON_DISASSOCIATED_DUE_NO_P2P_COEXIST_PARAM   3
#define P2P_REASON_DISASSOCIATED_DUE_MANAGEABLE             4
/* P2P 4.1.4 - Device Capability Bitmap definitions */
#define P2P_DEV_CAPABILITY_SERVICE_DISCOVERY                BIT(0)
#define P2P_DEV_CAPABILITY_CLIENT_DISCOVERABILITY           BIT(1)
#define P2P_DEV_CAPABILITY_CONCURRENT_OPERATION             BIT(2)
#define P2P_DEV_CAPABILITY_P2P_INFRA_MANAGED                BIT(3)
#define P2P_DEV_CAPABILITY_P2P_DEVICE_LIMIT                 BIT(4)
#define P2P_DEV_CAPABILITY_P2P_INVITATION_PROCEDURE         BIT(5)
/* P2P 4.1.4 - Group Capability Bitmap definitions */
#define P2P_GROUP_CAPABILITY_P2P_GROUP_OWNER                BIT(0)
#define P2P_GROUP_CAPABILITY_PERSISTENT_P2P_GROUP           BIT(1)
#define P2P_GROUP_CAPABILITY_P2P_GROUP_LIMIT                BIT(2)
#define P2P_GROUP_CAPABILITY_INTRA_BSS_DISTRIBUTION         BIT(3)
#define P2P_GROUP_CAPABILITY_CROSS_CONNECTION               BIT(4)
#define P2P_GROUP_CAPABILITY_PERSISTENT_RECONNECT           BIT(5)
#define P2P_GROUP_CAPABILITY_GROUP_FORMATION                BIT(6)
/* P2P 4.1.6 - GO Intent field definitions */
#define P2P_GO_INTENT_TIE_BREAKER_FIELD                     BIT(0)
#define P2P_GO_INTENT_VALUE_MASK                            BITS(1, 7)
#define P2P_GO_INTENT_VALUE_OFFSET                          1
/* P2P 4.1.12 - Manageability Bitmap definitions */
#define P2P_DEVICE_MANAGEMENT                               BIT(0)
/* P2P 4.1.14 - CTWindow and OppPS Parameters definitions */
#define P2P_CTW_OPPPS_PARAM_OPPPS_FIELD                     BIT(7)
#define P2P_CTW_OPPPS_PARAM_CTWINDOW_MASK                   BITS(0, 6)
#define ELEM_MAX_LEN_P2P_FOR_PROBE_REQ                      \
	(P2P_OUI_TYPE_LEN + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_P2P_CAPABILITY) + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_P2P_DEV_ID) + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_LISTEN_CHANNEL) + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_OPERATING_CHANNEL))
#define ELEM_MAX_LEN_P2P_FOR_ASSOC_REQ                      \
	(P2P_OUI_TYPE_LEN + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_P2P_CAPABILITY) + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_EXT_LISTEN_TIMING) + \
	 (P2P_ATTRI_HDR_LEN + P2P_ATTRI_MAX_LEN_P2P_DEV_INFO))
/* P2P 4.1.16 - P2P Client Infor Descriptor */
#define P2P_CLIENT_INFO_DESC_HDR_LEN                        1	/* Length(1 octets) */
/* P2P 4.1.20 - P2P Invitation Flags Attribute*/
#define P2P_INVITATION_FLAGS_INVITATION_TYPE       BIT(0)
#define P2P_INVITATION_TYPE_INVITATION                      0
#define P2P_INVITATION_TYPE_REINVOKE                          1
/* 3 */
/* WPS 11 - Data Element Definitions */
#define WPS_ATTRI_ID_VERSION            0x104A
#define WPS_ATTRI_ID_CONFIGURATION_METHODS   0x1008
#define WPS_ATTRI_ID_DEVICE_PASSWORD    0x1012
#define WPS_ATTRI_ID_DEVICE_NAME        0x1011
#define WPS_ATTRI_ID_PRI_DEVICE_TYPE    0x1054
#define WPS_ATTRI_ID_SEC_DEVICE_TYPE    0x1055
#define WPS_ATTRI_MAX_LEN_DEVICE_NAME   32	/* 0x1011 */
#define WPS_ATTRI_CFG_METHOD_USBA           BIT(0)
#define WPS_ATTRI_CFG_METHOD_ETHERNET       BIT(1)
#define WPS_ATTRI_CFG_METHOD_LABEL          BIT(2)
#define WPS_ATTRI_CFG_METHOD_DISPLAY        BIT(3)
#define WPS_ATTRI_CFG_METHOD_EXT_NFC        BIT(4)
#define WPS_ATTRI_CFG_METHOD_INT_NFC        BIT(5)
#define WPS_ATTRI_CFG_METHOD_NFC_IF         BIT(6)
#define WPS_ATTRI_CFG_METHOD_PUSH_BUTTON    BIT(7)
#define WPS_ATTRI_CFG_METHOD_KEYPAD         BIT(8)
#define P2P_FLAGS_PROVISION_COMPLETE                            0x00000001
#define P2P_FLAGS_PROVISION_DISCOVERY_COMPLETE        0x00000002
#define P2P_FLAGS_PROVISION_DISCOVERY_WAIT_RESPONSE 0x00000004
#define P2P_FLAGS_PROVISION_DISCOVERY_RESPONSE_WAIT  0x00000008
#define P2P_FLAGS_MASK_PROVISION                                    0x00000017
#define P2P_FLAGS_MASK_PROVISION_COMPLETE                   0x00000015
#define P2P_FLAGS_PROVISION_DISCOVERY_INDICATED        0x00000010
#define P2P_FLAGS_INVITATION_TOBE_GO                            0x00000100
#define P2P_FLAGS_INVITATION_TOBE_GC                            0x00000200
#define P2P_FLAGS_INVITATION_SUCCESS                            0x00000400
#define P2P_FLAGS_INVITATION_WAITING_TARGET                            0x00000800
#define P2P_FLAGS_MASK_INVITATION                                  0x00000F00
#define P2P_FLAGS_FORMATION_ON_GOING                          0x00010000
#define P2P_FLAGS_FORMATION_LOCAL_PWID_RDY              0x00020000
#define P2P_FLAGS_FORMATION_TARGET_PWID_RDY           0x00040000
#define P2P_FLAGS_FORMATION_COMPLETE                            0x00080000
#define P2P_FLAGS_MASK_FORMATION                                  0x000F0000
#define P2P_FLAGS_DEVICE_DISCOVER_REQ                        0x00100000
#define P2P_FLAGS_DEVICE_DISCOVER_DONE                       0x00200000
#define P2P_FLAGS_DEVICE_INVITATION_WAIT                      0x00400000
#define P2P_FLAGS_DEVICE_SERVICE_DISCOVER_WAIT         0x00800000
#define P2P_FLAGS_MASK_DEVICE_DISCOVER                        0x00F00000
#define P2P_FLAGS_DEVICE_FORMATION_REQUEST                 0x01000000
/* MACRO for flag operation */
#define SET_FLAGS(_FlagsVar, _BitsToSet) \
	{(_FlagsVar) = ((_FlagsVar) | (_BitsToSet))}

#define TEST_FLAGS(_FlagsVar, _BitsToCheck) \
	(((_FlagsVar) & (_BitsToCheck)) == (_BitsToCheck))
#define CLEAR_FLAGS(_FlagsVar, _BitsToClear) \
	{(_FlagsVar) &= ~(_BitsToClear)}

#define CFG_DISABLE_WIFI_DIRECT_ENHANCEMENT_I     0
#define CFG_DISABLE_WIFI_DIRECT_ENHANCEMENT_II     0
#define CFG_DISABLE_WIFI_DIRECT_ENHANCEMENT_III     0
#define CFG_DISABLE_WIFI_DIRECT_ENHANCEMENT_IV     0
#define CFG_DISABLE_DELAY_PROVISION_DISCOVERY      0
#define CFG_CONNECTION_POLICY_2_0                            0
/* Device Password ID */
enum wps_dev_password_id {
	DEV_PW_DEFAULT = 0x0000,
	DEV_PW_USER_SPECIFIED = 0x0001,
	DEV_PW_MACHINE_SPECIFIED = 0x0002,
	DEV_PW_REKEY = 0x0003,
	DEV_PW_PUSHBUTTON = 0x0004,
	DEV_PW_REGISTRAR_SPECIFIED = 0x0005
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */
#if defined(WINDOWS_DDK) || defined(WINDOWS_CE)
#pragma pack(1)
#endif

/* 3 */

#if 0
/* P2P 4.1.1 - General P2P Attribute */
typedef struct _P2P_ATTRIBUTE_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucBody[1];	/* Body field */
} __KAL_ATTRIB_PACKED__ P2P_ATTRIBUTE_T, ATTRIBUTE_HDR_T, *P_P2P_ATTRIBUTE_T, *P_ATTRIBUTE_HDR_T;
#endif

/* P2P 4.1.3 - P2P Minor Reason Code Attribute */
typedef struct _P2P_ATTRI_REASON_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucMinorReasonCode;	/* Minor Reason Code */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_REASON_T, *P_P2P_ATTRI_REASON_T;

/* P2P 4.1.4 - P2P Capability Attribute */
typedef struct _P2P_ATTRI_CAPABILITY_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucDeviceCap;	/* Device Capability Bitmap */
	UINT_8 ucGroupCap;	/* Group Capability Bitmap */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_CAPABILITY_T, *P_P2P_ATTRI_CAPABILITY_T;

/* P2P 4.1.5 - P2P Device ID Attribute */
typedef struct _P2P_ATTRI_DEV_ID_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_DEV_ID_T, *P_P2P_ATTRI_DEV_ID_T;

/* P2P 4.1.6 - Group Owner Intent Attribute */
typedef struct _P2P_ATTRI_GO_INTENT_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucGOIntent;	/* Group Owner Intent */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_GO_INTENT_T, *P_P2P_ATTRI_GO_INTENT_T;

/* P2P 4.1.7 - Configuration Timeout Attribute */
typedef struct _P2P_ATTRI_CFG_TIMEOUT_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucGOCfgTimeout;	/* GO Configuration Timeout */
	UINT_8 ucClientCfgTimeout;	/* Client Configuration Timeout */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_CFG_TIMEOUT_T, *P_P2P_ATTRI_CFG_TIMEOUT_T;

/* P2P 4.1.8 - Listen Channel Attribute */
typedef struct _P2P_ATTRI_LISTEN_CHANNEL_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucCountryString[3];	/* Country String */
	UINT_8 ucOperatingClass;	/* Operating Class from 802.11 Annex J/P802.11 REVmb 3.0 */
	UINT_8 ucChannelNumber;	/* Channel Number */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_LISTEN_CHANNEL_T, *P_P2P_ATTRI_LISTEN_CHANNEL_T;

/* P2P 4.1.9 - P2P Group BSSID Attribute */
typedef struct _P2P_ATTRI_GROUP_BSSID_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucBssid[MAC_ADDR_LEN];	/* P2P Group BSSID */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_GROUP_BSSID_T, *P_P2P_ATTRI_GROUP_BSSID_T;

/* P2P 4.1.11 - Intended P2P Interface Address Attribute */
typedef struct _P2P_ATTRI_INTENDED_IF_ADDR_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucIfAddr[MAC_ADDR_LEN];	/* P2P Interface Address */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_INTENDED_IF_ADDR_T, *P_P2P_ATTRI_INTENDED_IF_ADDR_T;

/* P2P 4.1.12 - P2P Manageability Attribute */
typedef struct _P2P_ATTRI_MANAGEABILITY_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucManageability;	/* P2P Manageability Bitmap */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_MANAGEABILITY_T, *P_P2P_ATTRI_MANAGEABILITY_T;

/* P2P 4.1.13 - Channel List Attribute */
typedef struct _P2P_ATTRI_CHANNEL_LIST_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucCountryString[3];	/* Country String */
	UINT_8 aucChannelEntry[1];	/* Channel Entry List */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_CHANNEL_T, *P_P2P_ATTRI_CHANNEL_T;

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

typedef struct _P2P_ATTRI_DEV_INFO_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */
	UINT_16 u2ConfigMethodsBE;	/* Config Method */
	DEVICE_TYPE_T rPrimaryDevTypeBE;	/* Primary Device Type */
	UINT_8 ucNumOfSecondaryDevType;	/* Number of Secondary Device Types */
	DEVICE_TYPE_T arSecondaryDevTypeListBE[1];	/* Secondary Device Type List */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_DEV_INFO_T, *P_P2P_ATTRI_DEV_INFO_T;

/* WPS 7.1 & 11 WPS TLV Data Format - Device Name */
typedef struct _DEVICE_NAME_TLV_T {
	UINT_16 u2Id;		/* WPS Attribute Type */
	UINT_16 u2Length;	/* Data Length */
	UINT_8 aucName[32];	/* Device Name *//* TODO : Fixme */
} __KAL_ATTRIB_PACKED__ DEVICE_NAME_TLV_T, *P_DEVICE_NAME_TLV_T;

/* P2P 4.1.16 - P2P Group Info Attribute */
typedef struct _P2P_CLIENT_INFO_DESC_T {
	UINT_8 ucLength;	/* Length */
	UINT_8 aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */
	UINT_8 aucIfAddr[MAC_ADDR_LEN];	/* P2P Interface Address */
	UINT_8 ucDeviceCap;	/* Device Capability Bitmap */
	UINT_16 u2ConfigMethodsBE;	/* Config Method */
	DEVICE_TYPE_T rPrimaryDevTypeBE;	/* Primary Device Type */
	UINT_8 ucNumOfSecondaryDevType;	/* Number of Secondary Device Types */
	DEVICE_TYPE_T arSecondaryDevTypeListBE[1];	/* Secondary Device Type List */
} __KAL_ATTRIB_PACKED__ P2P_CLIENT_INFO_DESC_T, *P_P2P_CLIENT_INFO_DESC_T;

typedef struct _P2P_ATTRI_GROUP_INFO_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	P2P_CLIENT_INFO_DESC_T arClientDesc[1];	/* P2P Client Info Descriptors */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_GROUP_INFO_T, *P_P2P_ATTRI_GROUP_INFO_T;

/* P2P 4.1.17 - P2P Group ID Attribute */
typedef struct _P2P_ATTRI_GROUP_ID_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];	/* SSID */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_GROUP_ID_T, *P_P2P_ATTRI_GROUP_ID_T;

/* P2P 4.1.18 - P2P Interface Attribute */
typedef struct _P2P_ATTRI_INTERFACE_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucDevAddr[MAC_ADDR_LEN];	/* P2P Device Address */
	UINT_8 ucIfAddrCount;	/* P2P Interface Address Count */
	UINT_8 aucIfAddrList[MAC_ADDR_LEN];	/* P2P Interface Address List */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_INTERFACE_T, *P_P2P_ATTRI_INTERFACE_T;

/* P2P 4.1.19 - Operating Channel Attribute */
typedef struct _P2P_ATTRI_OPERATING_CHANNEL_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 aucCountryString[3];	/* Country String */
	UINT_8 ucOperatingClass;	/* Operating Class from 802.11 Annex J/P802.11 REVmb 3.0 */
	UINT_8 ucChannelNumber;	/* Channel Number */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_OPERATING_CHANNEL_T, *P_P2P_ATTRI_OPERATING_CHANNEL_T;

/* P2P 4.1.20 - Invitation Flags Attribute */
typedef struct _P2P_ATTRI_INVITATION_FLAG_T {
	UINT_8 ucId;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucInviteFlagsBitmap;	/* Invitation Flags Bitmap */
} __KAL_ATTRIB_PACKED__ P2P_ATTRI_INVITATION_FLAG_T, *P_P2P_ATTRI_INVITATION_FLAG_T;

/* WSC 1.0 Table 28 */
typedef struct _WSC_ATTRI_VERSION_T {
	UINT_16 u2Id;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_8 ucVersion;	/* Version 1.0 or 1.1 */
} __KAL_ATTRIB_PACKED__ WSC_ATTRI_VERSION_T, *P_WSC_ATTRI_VERSION_T;

typedef struct _WSC_ATTRI_DEVICE_PASSWORD_ID_T {
	UINT_16 u2Id;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_16 u2DevPasswordId;	/* Device Password ID */
} __KAL_ATTRIB_PACKED__ WSC_ATTRI_DEVICE_PASSWORD_ID_T, *P_WSC_ATTRI_DEVICE_PASSWORD_ID_T;

typedef struct _WSC_ATTRI_CONFIGURATION_METHOD_T {
	UINT_16 u2Id;		/* Attribute ID */
	UINT_16 u2Length;	/* Length */
	UINT_16 u2ConfigMethods;	/* Configure Methods */
} __KAL_ATTRIB_PACKED__ WSC_ATTRI_CONFIGURATION_METHOD_T, *P_WSC_ATTRI_CONFIGURATION_METHOD_T;

#if defined(WINDOWS_DDK) || defined(WINDOWS_CE)
#pragma pack()
#endif

/* 3 --------------- WFA P2P Attributes Handler prototype --------------- */
typedef UINT_32(*PFN_APPEND_ATTRI_FUNC) (P_ADAPTER_T, BOOLEAN, PUINT_16, PUINT_8, UINT_16);

typedef VOID(*PFN_HANDLE_ATTRI_FUNC) (P_SW_RFB_T, P_P2P_ATTRIBUTE_T);

typedef VOID(*PFN_VERIFY_ATTRI_FUNC) (P_SW_RFB_T, P_P2P_ATTRIBUTE_T, PUINT_16);

typedef UINT_32(*PFN_CALCULATE_VAR_ATTRI_LEN_FUNC) (P_ADAPTER_T, P_STA_RECORD_T);

typedef enum _ENUM_CONFIG_METHOD_SEL {
	ENUM_CONFIG_METHOD_SEL_AUTO,
	ENUM_CONFIG_METHOD_SEL_USER,
	ENUM_CONFIG_METHOD_SEL_NUM
} ENUM_CONFIG_METHOD_SEL, *P_ENUM_CONFIG_METHOD_SEL;

typedef enum _ENUM_P2P_FORMATION_POLICY {
	ENUM_P2P_FORMATION_POLICY_AUTO = 0,
	ENUM_P2P_FORMATION_POLICY_PASSIVE,	/* Device would wait GO NEGO REQ instead of sending it actively. */
	ENUM_P2P_FORMATION_POLICY_NUM
} ENUM_P2P_FORMATION_POLICY, P_ENUM_P2P_FORMATION_POLICY;

typedef enum _ENUM_P2P_INVITATION_POLICY {
	ENUM_P2P_INVITATION_POLICY_USER = 0,
	ENUM_P2P_INVITATION_POLICY_ACCEPT_FIRST,
	ENUM_P2P_INVITATION_POLICY_DENY_ALL,
	ENUM_P2P_INVITATION_POLICY_NUM
} ENUM_P2P_INVITATION_POLICY, P_ENUM_P2P_INVITATION_POLICY;

/* 3 --------------- Data Structure for P2P Operation --------------- */
/* 3 Session for CONNECTION SETTINGS of P2P */
struct _P2P_CONNECTION_SETTINGS_T {
	UINT_8 ucDevNameLen;
	UINT_8 aucDevName[WPS_ATTRI_MAX_LEN_DEVICE_NAME];

	DEVICE_TYPE_T rPrimaryDevTypeBE;

	ENUM_P2P_FORMATION_POLICY eFormationPolicy;	/* Formation Policy. */

	/*------------WSC Related Param---------------*/
	UINT_16 u2ConfigMethodsSupport;	/* Preferred configure method.
					 * Some device may not have keypad.
					 */
	ENUM_CONFIG_METHOD_SEL eConfigMethodSelType;
	UINT_16 u2TargetConfigMethod;	/* Configure method selected by user or auto. */
	UINT_16 u2LocalConfigMethod;	/* Configure method of target. */
	BOOLEAN fgIsPasswordIDRdy;
	/*------------WSC Related Param---------------*/

	UINT_8 ucClientConfigTimeout;
	UINT_8 ucGoConfigTimeout;

	UINT_8 ucSecondaryDevTypeCount;
#if P2P_MAX_SUPPORTED_SEC_DEV_TYPE_COUNT
	DEVICE_TYPE_T arSecondaryDevTypeBE[P2P_MAX_SUPPORTED_SEC_DEV_TYPE_COUNT];
#endif

#if 0
	UINT_8 ucRfChannelListCount;
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_COUNT
	UINT_8 aucChannelList[P2P_MAX_SUPPORTED_CHANNEL_LIST_COUNT];	/*
									 * Channel Numbering
									 * depends on 802.11mb
									 * Annex J.
									 */

#endif
#else
	UINT_8 ucRfChannelListSize;
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE
	UINT_8 aucChannelEntriesField[P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE];
#endif
#endif

	/* Go Intent */
	UINT_8 ucTieBreaker;
	UINT_8 ucGoIntent;

	/* For Device Capability */
	BOOLEAN fgSupportServiceDiscovery;
	BOOLEAN fgSupportClientDiscoverability;
	BOOLEAN fgSupportConcurrentOperation;
	BOOLEAN fgSupportInfraManaged;
	BOOLEAN fgSupportInvitationProcedure;

	/* For Group Capability */
	BOOLEAN fgSupportPersistentP2PGroup;
	BOOLEAN fgSupportIntraBSSDistribution;
	BOOLEAN fgSupportCrossConnection;
	BOOLEAN fgSupportPersistentReconnect;

	BOOLEAN fgP2pGroupLimit;

	BOOLEAN fgSupportOppPS;
	UINT_16 u2CTWindow;

	BOOLEAN fgIsScanReqIssued;
	BOOLEAN fgIsServiceDiscoverIssued;

	/*============ Target Device Connection Settings ============*/

	/* Discover Target Device Info. */
	BOOLEAN fgIsDevId;
	BOOLEAN fgIsDevType;

	/* Encryption mode of Target Device */
	ENUM_PARAM_AUTH_MODE_T eAuthMode;

	/* SSID
	 *  1. AP Mode, this is the desired SSID user specified.
	 *  2. Client Mode, this is the target SSID to be connected to.
	 */
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];
	UINT_8 ucSSIDLen;

	/* Operating channel requested. */
	UINT_8 ucOperatingChnl;
	ENUM_BAND_T eBand;

	/* Linten channel requested. */
	UINT_8 ucListenChnl;

	/* For device discover address/type. */
	UINT_8 aucTargetDevAddr[MAC_ADDR_LEN];	/*
						 * P2P Device Address, for P2P Device Discovery
						 * & P2P Connection.
						 */

#if CFG_ENABLE_WIFI_DIRECT
	P_P2P_DEVICE_DESC_T prTargetP2pDesc;
#endif

	UINT_8 ucLastStatus;	/*
				 * P2P FSM would append status attribute according to this
				 * field.
				 */

#if !CFG_DISABLE_DELAY_PROVISION_DISCOVERY
	UINT_8 ucLastDialogToken;
	UINT_8 aucIndicateDevAddr[MAC_ADDR_LEN];
#endif

#if 0
	UINT_8 ucTargetRfChannelListCount;
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_COUNT
	UINT_8 aucTargetChannelList[P2P_MAX_SUPPORTED_CHANNEL_LIST_COUNT];	/*
										 * Channel
										 * Numbering
										 * depends on
										 * 802.11mb Annex J.
										 */
#endif
#endif
};

typedef enum _ENUM_P2P_IOCTL_T {
	P2P_IOCTL_IDLE = 0,
	P2P_IOCTL_DEV_DISCOVER,
	P2P_IOCTL_INVITATION_REQ,
	P2P_IOCTL_SERV_DISCOVER,
	P2P_IOCTL_WAITING,
	P2P_IOCTL_NUM
} ENUM_P2P_IOCTL_T;

/*---------------- Service Discovery Related -------------------*/
typedef enum _ENUM_SERVICE_TX_TYPE_T {
	ENUM_SERVICE_TX_TYPE_BY_DA,
	ENUM_SERVICE_TX_TYPE_BY_CHNL,
	ENUM_SERVICE_TX_TYPE_NUM
} ENUM_SERVICE_TX_TYPE_T;

typedef struct _SERVICE_DISCOVERY_FRAME_DATA_T {
	QUE_ENTRY_T rQueueEntry;
	P_MSDU_INFO_T prSDFrame;
	ENUM_SERVICE_TX_TYPE_T eServiceType;
	UINT_8 ucSeqNum;
	union {
		UINT_8 ucChannelNum;
		UINT_8 aucPeerAddr[MAC_ADDR_LEN];
	} uTypeData;
	BOOLEAN fgIsTxDoneIndicate;
} SERVICE_DISCOVERY_FRAME_DATA_T, *P_SERVICE_DISCOVERY_FRAME_DATA_T;

struct _P2P_FSM_INFO_T_DEPRECATED {
	/* P2P FSM State */
	ENUM_P2P_STATE_T eCurrentState;

	/* Channel */
	BOOLEAN fgIsChannelRequested;

	ENUM_P2P_STATE_T ePreviousState;

	ENUM_P2P_STATE_T eReturnState;	/*
					 * Return state after current activity finished or
					 * abort.
					 */

	UINT_8 aucTargetIfAddr[PARAM_MAC_ADDR_LEN];
	P_BSS_DESC_T prTargetBss;	/*
					 * BSS of target P2P Device. For Connection/Service
					 * Discovery
					 */

	P_STA_RECORD_T prTargetStaRec;

	BOOLEAN fgIsRsponseProbe;	/*
					 * Indicate if P2P FSM can response probe
					 * request frame.
					 */

	/* Sequence number of requested message. */
	UINT_8 ucSeqNumOfReqMsg;	/* Used for SAA FSM request message. */

	/* Channel Privilege */
	UINT_8 ucSeqNumOfChReq;	/* Used for Channel Request message. */

	UINT_8 ucSeqNumOfScnMsg;	/* Used for SCAN FSM request message. */
	UINT_8 ucSeqNumOfCancelMsg;

	UINT_8 ucDialogToken;
	UINT_8 ucRxDialogToken;

	/* Timer */
	TIMER_T rDeviceDiscoverTimer;	/*
					 * For device discovery time of each discovery
					 * request from user.
					 */
	TIMER_T rOperationListenTimer;	/* For Find phase under operational state. */
	TIMER_T rFSMTimer;	/*
				 * A timer used for Action frame timeout usage.
				 */

	TIMER_T rRejoinTimer;	/*
				 * A timer used for Action frame timeout usage.
				 */

	/* Flag to indicate Provisioning */
	BOOLEAN fgIsConnectionRequested;

	/* Current IOCTL. */
	ENUM_P2P_IOCTL_T eP2pIOCTL;

	UINT_8 ucAvailableAuthTypes;	/* Used for AUTH_MODE_AUTO_SWITCH */

	/*--------SERVICE DISCOVERY--------*/
	QUE_T rQueueGASRx;	/* Input Request/Response. */
	QUE_T rQueueGASTx;	/* Output Response. */
	P_SERVICE_DISCOVERY_FRAME_DATA_T prSDRequest;
	UINT_8 ucVersionNum;	/* GAS packet sequence number for...Action Frame? */
	UINT_8 ucGlobalSeqNum;	/* Sequence Number of RX SD packet. */
	/*--------Service DISCOVERY--------*/

	/*--------DEVICE DISCOVERY---------*/
	UINT_8 aucTargetGroupID[PARAM_MAC_ADDR_LEN];
	UINT_16 u2TargetGroupSsidLen;
	UINT_8 aucTargetSsid[32];
	UINT_8 aucSearchingP2pDevice[PARAM_MAC_ADDR_LEN];
	UINT_8 ucDLToken;
	/*----------------------------------*/

	/* Indicating Peer Status. */
	UINT_32 u4Flags;

	/*Indicating current running mode. */
	BOOLEAN fgIsApMode;

	/*------------INVITATION------------*/
	ENUM_P2P_INVITATION_POLICY eInvitationRspPolicy;
	/*----------------------------------*/
};

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
	UINT_8 ucListenChannel;	/*
				 * Linten Channel only on channels 1, 6 and 11
				 * in the 2.4 GHz.
				 */

	UINT_8 ucPreferredChannel;	/*
					 * Operating Channel, should be one of channel
					 * list in p2p connection settings.
					 */
	ENUM_CHNL_EXT_T eRfSco;
	ENUM_BAND_T eRfBand;

	/* Extended Listen Timing. */
	UINT_16 u2AvailabilityPeriod;
	UINT_16 u2AvailabilityInterval;

#if 0				/* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0) */
	UINT_16 u2IELenForBCN;
	UINT_8 aucBeaconIECache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE + WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

/* UINT_16                 u2IELenForProbeRsp; */
/* UINT_8                  aucProbeRspIECache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE + WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE]; */

	UINT_16 u2IELenForAssocRsp;
	UINT_8 aucAssocRspIECache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE + WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

#else
	UINT_16 u2AttributeLen;
	UINT_8 aucAttributesCache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	UINT_16 u2WscAttributeLen;
	UINT_8 aucWscAttributesCache[WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];
#endif
	UINT_8 aucGroupID[MAC_ADDR_LEN];
	UINT_16 u2GroupSsidLen;
	UINT_8 aucGroupSsid[ELEM_MAX_LEN_SSID];

	PARAM_CUSTOM_NOA_PARAM_STRUCT_T rNoaParam;
	PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T rOppPsParam;

#if 0				/* CL2055022 */
	UINT_16 u2WpaIeLen;
	UINT_8 aucWpaIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_WPA];
#endif
};

typedef struct _MSG_P2P_DEVICE_DISCOVER_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_32 u4DevDiscoverTime;	/* 0: Infinite, 1~X: in unit of MS. */
	BOOLEAN fgIsSpecificType;
#if CFG_ENABLE_WIFI_DIRECT
	P2P_DEVICE_TYPE_T rTargetDeviceType;
#endif
	UINT_8 aucTargetDeviceID[MAC_ADDR_LEN];
} MSG_P2P_DEVICE_DISCOVER_T, *P_MSG_P2P_DEVICE_DISCOVER_T;

typedef struct _MSG_P2P_INVITATION_REQUEST_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 aucDeviceID[MAC_ADDR_LEN];	/* Target Device ID to be invited. */
} MSG_P2P_INVITATION_REQUEST_T, *P_MSG_P2P_INVITATION_REQUEST_T;

typedef struct _MSG_P2P_FUNCTION_SWITCH_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	BOOLEAN fgIsFuncOn;
} MSG_P2P_FUNCTION_SWITCH_T, *P_MSG_P2P_FUNCTION_SWITCH_T;

typedef struct _MSG_P2P_SERVICE_DISCOVERY_REQUEST_T {
	MSG_HDR_T rMsgHdr;	/* Must be the first member */
	UINT_8 aucDeviceID[MAC_ADDR_LEN];
	BOOLEAN fgNeedTxDoneIndicate;
	UINT_8 ucSeqNum;
} MSG_P2P_SERVICE_DISCOVERY_REQUEST_T, *P_MSG_P2P_SERVICE_DISCOVERY_REQUEST_T;

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
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

/*======P2P State======*/
VOID
p2pStateInit_LISTEN(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN P_P2P_SPECIFIC_BSS_INFO_T
		    prSP2pBssInfo, IN UINT_8 ucListenChannel);

VOID p2pStateAbort_LISTEN(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsChannelExtenstion);

VOID p2pStateAbort_SEARCH_SCAN(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsChannelExtenstion);

VOID p2pStateAbort_GO_OPERATION(IN P_ADAPTER_T prAdapter);

VOID p2pStateAbort_GC_OPERATION(IN P_ADAPTER_T prAdapter);

VOID
p2pStateInit_CONFIGURATION(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN P_BSS_INFO_T prP2pBssInfo, IN
			   P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecBssInfo);

VOID p2pStateAbort_CONFIGURATION(IN P_ADAPTER_T prAdapter);

VOID p2pStateInit_JOIN(IN P_ADAPTER_T prAdapter);

VOID p2pStateAbort_JOIN(IN P_ADAPTER_T prAdapter);

/*====== P2P Functions ======*/

VOID p2pFuncInitGO(IN P_ADAPTER_T prAdapter);

VOID
p2pFuncDisconnect(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN BOOLEAN fgSendDeauth, IN UINT_16
		  u2ReasonCode);

VOID p2pFuncRunEventProvisioningComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

WLAN_STATUS p2pFuncSetGroupID(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucGroupID, IN PUINT_8 pucSsid, IN UINT_8 ucSsidLen);

WLAN_STATUS
p2pFuncSendDeviceDiscoverabilityReqFrame(IN P_ADAPTER_T prAdapter, IN UINT_8 aucDestAddr[], IN UINT_8 ucDialogToken);

WLAN_STATUS
p2pFuncSendDeviceDiscoverabilityRspFrame(IN P_ADAPTER_T prAdapter, IN UINT_8 aucDestAddr[], IN UINT_8 ucDialogToken);

UINT_8 p2pFuncGetVersionNumOfSD(IN P_ADAPTER_T prAdapter);

/*====== P2P FSM ======*/
VOID p2pFsmRunEventConnectionRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventDeviceDiscoveryRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventDeviceDiscoveryAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventRxGroupNegotiationReqFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS
p2pFsmRunEventGroupNegotiationRequestTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN
					    ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS
p2pFsmRunEventGroupNegotiationResponseTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN
					     ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS
p2pFsmRunEventGroupNegotiationConfirmTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN
					    ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS
p2pFsmRunEventProvisionDiscoveryRequestTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN
					      ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS
p2pFsmRunEventProvisionDiscoveryResponseTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN
					       ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS
p2pFsmRunEventInvitationRequestTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T
				      rTxDoneStatus);

VOID p2pFsmRunEventRxDeauthentication(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

VOID p2pFsmRunEventRxDisassociation(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

VOID p2pFsmRunEventBeaconTimeout(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo);

WLAN_STATUS
p2pFsmRunEventDeauthTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T
			   rTxDoneStatus);

#if 1
#endif

/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/* //////////////////////////////////////////////////////////////////////// */
/*======Mail Box Event Message=====*/

VOID p2pFsmRunEventConnectionAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventConnectionTrigger(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventP2PFunctionSwitch(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventConnectionPause(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID
p2pIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter, IN ENUM_PARAM_MEDIA_STATE_T eConnectionState, IN UINT_8
				aucTargetAddr[]);

VOID p2pUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prAssocRspSwRfb);

/*======Mail Box Event Message=====*/

VOID p2pFsmInit(IN P_ADAPTER_T prAdapter);

VOID p2pFsmUninit(IN P_ADAPTER_T prAdapter);

VOID p2pFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_P2P_STATE_T eNextState);

VOID p2pStartGO(IN P_ADAPTER_T prAdapter);

VOID p2pAssignSsid(IN PUINT_8 pucSsid, IN PUINT_8 pucSsidLen);

VOID p2pFsmRunEventIOReqTimeout(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Param);

VOID p2pFsmRunEventSearchPeriodTimeout(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Param);

VOID p2pFsmRunEventFsmTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID p2pFsmRunEventRejoinTimeout(IN P_ADAPTER_T prAdapter, IN UINT_32 u4Parm);

/*=============== P2P Function Related ================*/

/*=============== P2P Function Related ================*/

#if CFG_TEST_WIFI_DIRECT_GO
VOID p2pTest(IN P_ADAPTER_T prAdapter);
#endif /* CFG_TEST_WIFI_DIRECT_GO */

VOID p2pGenerateP2P_IEForBeacon(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID p2pGenerateP2P_IEForAssocReq(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID p2pGenerateP2P_IEForAssocRsp(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID
p2pGenerateP2P_IEForProbeReq(IN P_ADAPTER_T prAdapter, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN UINT_16 u2BufSize);

UINT_32 p2pCalculateP2P_IELenForBeacon(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

UINT_32 p2pCalculateP2P_IELenForAssocRsp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

UINT_32 p2pCalculateP2P_IELenForProbeReq(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

VOID p2pGenerateWSC_IEForProbeResp(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID
p2pGenerateWSC_IEForProbeReq(IN P_ADAPTER_T prAdapter, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN UINT_16 u2BufSize);

UINT_16 p2pCalculateWSC_IELenForProbeReq(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

UINT_32 p2pCalculateWSC_IELenForProbeResp(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_STA_RECORD_T prStaRec);

UINT_32
p2pAppendAttriStatus(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN
		     UINT_16 u2BufSize);

UINT_32
p2pAppendAttriCapability(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			 IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriGoIntent(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN
		       UINT_16 u2BufSize);

UINT_32
p2pAppendAttriCfgTimeout(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			 IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriGroupBssid(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			 IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriDeviceIDForBeacon(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
				pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriDeviceIDForProbeReq(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
				  pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriDeviceIDForDeviceDiscoveryReq(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset,
					    IN PUINT_8 pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriListenChannel(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
			    pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriIntendP2pIfAddr(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
			      pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriChannelList(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			  IN UINT_16 u2BufSize);

UINT_32 p2pCalculateAttriLenChannelList(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

UINT_32
p2pAppendAttriNoA(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf, IN
		  UINT_16 u2BufSize);

UINT_32
p2pAppendAttriDeviceInfo(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			 IN UINT_16 u2BufSize);

UINT_32 p2pCalculateAttriLenDeviceInfo(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

UINT_32
p2pAppendAttriGroupInfo(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			IN UINT_16 u2BufSize);

UINT_32 p2pCalculateAttriLenGroupInfo(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

UINT_32
p2pAppendAttriP2pGroupID(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			 IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriOperatingChannel(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
			       pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriInvitationFlag(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
			     pucBuf, IN UINT_16 u2BufSize);

VOID
p2pGenerateWscIE(IN P_ADAPTER_T prAdapter, IN UINT_8 ucOuiType, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN
		PUINT_8 pucBuf, IN UINT_16 u2BufSize, IN APPEND_VAR_ATTRI_ENTRY_T arAppendAttriTable[],
		IN UINT_32 u4AttriTableSize);

UINT_32
p2pAppendAttriWSCConfigMethod(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8
			      pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriWSCVersion(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN PUINT_8 pucBuf,
			 IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriWSCGONegReqDevPasswordId(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN
				       PUINT_8 pucBuf, IN UINT_16 u2BufSize);

UINT_32
p2pAppendAttriWSCGONegRspDevPasswordId(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsAssocFrame, IN PUINT_16 pu2Offset, IN
				       PUINT_8 pucBuf, IN UINT_16 u2BufSize);

WLAN_STATUS
p2pGetWscAttriList(IN P_ADAPTER_T prAdapter, IN UINT_8 ucOuiType, IN PUINT_8 pucIE, IN UINT_16 u2IELength, OUT PPUINT_8
		   ppucAttriList, OUT PUINT_16 pu2AttriListLen);

WLAN_STATUS
p2pGetAttriList(IN P_ADAPTER_T prAdapter, IN UINT_8 ucOuiType, IN PUINT_8 pucIE, IN UINT_16 u2IELength, OUT PPUINT_8
		ppucAttriList, OUT PUINT_16 pu2AttriListLen);

VOID p2pRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

WLAN_STATUS p2pRunEventAAASuccess(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

WLAN_STATUS p2pRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

WLAN_STATUS p2pSendProbeResponseFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

BOOLEAN p2pFsmRunEventRxProbeRequestFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID p2pFsmRunEventRxProbeResponseFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, IN P_BSS_DESC_T prBssDesc);

WLAN_STATUS p2pRxPublicActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS p2pRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID p2pFsmRunEventRxGroupNegotiationRspFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID p2pFsmRunEventRxGroupNegotiationCfmFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

#if 0				/* frog */
BOOLEAN scanMatchFilterOfP2P(IN P_SW_RFB_T prSWRfb, IN PP_BSS_DESC_T pprBssDesc);
#endif /* frog */

VOID
p2pProcessEvent_UpdateNOAParam(IN P_ADAPTER_T prAdapter, UINT_8 ucNetTypeIndex, P_EVENT_UPDATE_NOA_PARAMS_T
			       prEventUpdateNoaParam);

VOID p2pFuncCompleteIOCTL(IN P_ADAPTER_T prAdapter, IN WLAN_STATUS rWlanStatus);

/*******************************************************************************
 *                              F U N C T I O N S
 ********************************************************************************
 */
#ifndef _lint
/*
 * Kevin: we don't have to call following function to inspect the data structure.
 * It will check automatically while at compile time.
 * We'll need this for porting driver to different RTOS.
 */
static __KAL_INLINE__ VOID p2pDataTypeCheck(VOID)
{
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(IE_P2P_T) == (2 + 4 + 1));	/* all UINT_8 */
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRIBUTE_T) == (3 + 1));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_STATUS_T) == (3 + 1));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_REASON_T) == (3 + 1));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_CAPABILITY_T) == (3 + 2));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_DEV_ID_T) == (3 + 6));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_GO_INTENT_T) == (3 + 1));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_CFG_TIMEOUT_T) == (3 + 2));
#if CID52_53_54
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_LISTEN_CHANNEL_T) == (3 + 5));
#else
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_LISTEN_CHANNEL_T) == (3 + 5));
#endif
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_GROUP_BSSID_T) == (3 + 6));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_EXT_LISTEN_TIMING_T) == (3 + 4));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_INTENDED_IF_ADDR_T) == (3 + 6));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_MANAGEABILITY_T) == (3 + 1));

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_CHANNEL_T) == (3 + 4));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(CHANNEL_ENTRY_FIELD_T) == 3);
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_NOA_T) == (3 + 3));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(NOA_DESCRIPTOR_T) == 13);
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(DEVICE_TYPE_T) == 8);
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_DEV_INFO_T) == (3 + 6 + 2 + 8 + 1 + 8));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(DEVICE_NAME_TLV_T) == (4 + 32));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_CLIENT_INFO_DESC_T) == (1 + 6 + 6 + 1 + 2 + 8 + 1 + 8));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_GROUP_INFO_T) == (3 + (1 + 6 + 6 + 1 + 2 + 8 + 1 + 8)));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_GROUP_ID_T) == (3 + 38));
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_INTERFACE_T) == (3 + 13));
#if CID52_53_54
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_OPERATING_CHANNEL_T) == (3 + 5));
#else
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(P2P_ATTRI_OPERATING_CHANNEL_T) == (3 + 5));
#endif
}
#endif /* _lint */

#endif /* _P2P_FSM_H */
