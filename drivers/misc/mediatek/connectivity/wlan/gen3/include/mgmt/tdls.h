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
** Id: include/tdls.h#1
*/

/*! \file   "tdls.h"
    \brief This file contains the internal used in TDLS modules
	 for MediaTek Inc. 802.11 Wireless LAN Adapters.
*/

#ifndef _TDLS_H
#define _TDLS_H

#if CFG_SUPPORT_TDLS

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
#define TDLS_CFG_CMD_TEST

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/* assign station record idx for the packet */
#define TDLSEX_STA_REC_IDX_GET(__prAdapter__, __MsduInfo__)					\
{												\
	STA_RECORD_T *__StaRec__;								\
	__MsduInfo__->ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;					\
	__StaRec__ = cnmGetStaRecByAddress(__prAdapter__,					\
								(UINT_8) NETWORK_TYPE_AIS_INDEX,\
								__MsduInfo__->aucEthDestAddr);	\
	if ((__StaRec__ != NULL) && (IS_DLS_STA(__StaRec__)))					\
		__MsduInfo__->ucStaRecIndex = __StaRec__->ucIndex;				\
}

/* fill wiphy flag */
#define TDLSEX_WIPHY_FLAGS_INIT(__fgFlag__)							\
{												\
	__fgFlag__ |= (WIPHY_FLAG_SUPPORTS_TDLS | WIPHY_FLAG_TDLS_EXTERNAL_SETUP);\
}

#define LR_TDLS_FME_FIELD_FILL(__Len)	\
{					\
	pPkt += __Len;			\
	u4PktLen += __Len;		\
}

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
extern int wlanHardStartXmit(struct sk_buff *prSkb, struct net_device *prDev);

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/* Status code */
#define TDLS_STATUS							WLAN_STATUS

#define TDLS_STATUS_SUCCESS					WLAN_STATUS_SUCCESS
#define TDLS_STATUS_FAIL					WLAN_STATUS_FAILURE
#define TDLS_STATUS_INVALID_LENGTH			WLAN_STATUS_INVALID_LENGTH
#define TDLS_STATUS_RESOURCES				WLAN_STATUS_RESOURCES
#define TDLS_FME_MAC_ADDR_LEN				6
#define TDLS_EX_CAP_PEER_UAPSD				BIT(0)
#define TDLS_EX_CAP_CHAN_SWITCH				BIT(1)
#define TDLS_EX_CAP_TDLS					BIT(2)
#define TDLS_CMD_PEER_UPDATE_EXT_CAP_MAXLEN			5
#define TDLS_CMD_PEER_UPDATE_SUP_RATE_MAX			50
#define TDLS_CMD_PEER_UPDATE_SUP_CHAN_MAX			50

#define MAXNUM_TDLS_PEER            4

/* command */
typedef enum _TDLS_CMD_ID {
	TDLS_CMD_TEST_TX_FRAME = 0x00,
	TDLS_CMD_TEST_RCV_FRAME,
	TDLS_CMD_TEST_PEER_ADD,
	TDLS_CMD_TEST_PEER_UPDATE,
	TDLS_CMD_TEST_DATA_FRAME,
	TDLS_CMD_TEST_RCV_NULL
} TDLS_CMD_ID;

/* protocol */
#define TDLS_FRM_PROT_TYPE							0x890d

/* payload specific type in the LLC/SNAP header */
#define TDLS_FRM_PAYLOAD_TYPE						2

#define TDLS_FRM_CATEGORY							12

typedef enum _TDLS_FRM_ACTION_ID {
	TDLS_FRM_ACTION_SETUP_REQ = 0x00,
	TDLS_FRM_ACTION_SETUP_RSP,
	TDLS_FRM_ACTION_CONFIRM,
	TDLS_FRM_ACTION_TEARDOWN,
	TDLS_FRM_ACTION_PTI,
	TDLS_FRM_ACTION_CHAN_SWITCH_REQ,
	TDLS_FRM_ACTION_CHAN_SWITCH_RSP,
	TDLS_FRM_ACTION_PEER_PSM_REQ,
	TDLS_FRM_ACTION_PEER_PSM_RSP,
	TDLS_FRM_ACTION_PTI_RSP,
	TDLS_FRM_ACTION_DISCOVERY_REQ,
	TDLS_FRM_ACTION_DISCOVERY_RSP = 0x0e,
	TDLS_FRM_ACTION_EVENT_TEAR_DOWN_TO_SUPPLICANT = 0x30
} TDLS_FRM_ACTION_ID;

/* 7.3.2.62 Link Identifier element */
#define ELEM_ID_LINK_IDENTIFIER						101

typedef struct _IE_LINK_IDENTIFIER_T {
	UINT_8 ucId;
	UINT_8 ucLength;
	UINT_8 aBSSID[6];
	UINT_8 aInitiator[6];
	UINT_8 aResponder[6];
} __KAL_ATTRIB_PACKED__ IE_LINK_IDENTIFIER_T;

#define TDLS_LINK_IDENTIFIER_IE(__ie__)	((IE_LINK_IDENTIFIER_T *)(__ie__))

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef struct _STATION_PRARAMETERS {
	const u8 *supported_rates;
	struct net_device *vlan;
	u32 sta_flags_mask, sta_flags_set;
	u32 sta_modify_mask;
	int listen_interval;
	u16 aid;
	u8 supported_rates_len;
	u8 plink_action;
	u8 plink_state;
	const struct ieee80211_ht_cap *ht_capa;
	const struct ieee80211_vht_cap *vht_capa;
	u8 uapsd_queues;
	u8 max_sp;
	/* enum nl80211_mesh_power_mode local_pm; */
	u16 capability;
	const u8 *ext_capab;
	u8 ext_capab_len;
} STATION_PRARAMETERS, P_STATION_PRARAMETERS;

/* test command use */
typedef struct _PARAM_CUSTOM_TDLS_CMD_STRUCT_T {

	UINT_8 ucFmeType;	/* TDLS_FRM_ACTION_ID */

	UINT_8 ucToken;
	UINT_8 ucCap;

	/* bit0: TDLS, bit1: Peer U-APSD Buffer, bit2: Channel Switching */

	UINT_8 ucExCap;

	UINT_8 arSupRate[4];
	UINT_8 arSupChan[4];

	UINT_32 u4Timeout;

	UINT_8 arRspAddr[TDLS_FME_MAC_ADDR_LEN];
	UINT_8 arBssid[TDLS_FME_MAC_ADDR_LEN];

	/* Linux Kernel-3.10 */

	struct ieee80211_ht_cap rHtCapa;
	struct ieee80211_vht_cap rVhtCapa;
	/* struct */
	STATION_PRARAMETERS rPeerInfo;

} PARAM_CUSTOM_TDLS_CMD_STRUCT_T;

typedef enum _ENUM_TDLS_LINK_OPER {
	TDLS_DISCOVERY_REQ,
	TDLS_SETUP,
	TDLS_TEARDOWN,
	TDLS_ENABLE_LINK,
	TDLS_DISABLE_LINK
} ENUM_TDLS_LINK_OPER;

typedef struct _TDLS_CMD_LINK_OPER_T {

	UINT_8 aucPeerMac[6];
	ENUM_TDLS_LINK_OPER oper;
} TDLS_CMD_LINK_OPER_T;

typedef struct _TDLS_CMD_LINK_MGT_T {

	UINT_8 aucPeer[6];
	UINT_8 ucActionCode;
	UINT_8 ucDialogToken;
	UINT_16 u2StatusCode;
	UINT_32 u4SecBufLen;
	UINT_8 aucSecBuf[1000];

} TDLS_CMD_LINK_MGT_T;

typedef struct _TDLS_CMD_PEER_ADD_T {

	UINT_8 aucPeerMac[6];
	ENUM_STA_TYPE_T eStaType;
} TDLS_CMD_PEER_ADD_T;

typedef struct _TDLS_CMD_PEER_UPDATE_HT_CAP_MCS_INFO_T {
	UINT_8 arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
	UINT_16 u2RxHighest;
	UINT_8 ucTxParams;
	UINT_8 Reserved[3];
} TDLS_CMD_PEER_UPDATE_HT_CAP_MCS_INFO_T;

typedef struct _TDLS_CMD_PEER_UPDATE_VHT_CAP_MCS_INFO_T {
	UINT_8 arRxMask[SUP_MCS_RX_BITMASK_OCTET_NUM];
} TDLS_CMD_PEER_UPDATE_VHT_CAP_MCS_INFO_T;

typedef struct _TDLS_CMD_PEER_UPDATE_HT_CAP_T {
	UINT_16 u2CapInfo;
	UINT_8 ucAmpduParamsInfo;

	/* 16 bytes MCS information */
	TDLS_CMD_PEER_UPDATE_HT_CAP_MCS_INFO_T rMCS;

	UINT_16 u2ExtHtCapInfo;
	UINT_32 u4TxBfCapInfo;
	UINT_8 ucAntennaSelInfo;
} TDLS_CMD_PEER_UPDATE_HT_CAP_T;

typedef struct _TDLS_CMD_PEER_UPDATE_VHT_CAP_T {
	UINT_16 u2CapInfo;
	/* 16 bytes MCS information */
	TDLS_CMD_PEER_UPDATE_VHT_CAP_MCS_INFO_T rVMCS;

} TDLS_CMD_PEER_UPDATE_VHT_CAP_T;

typedef struct _TDLS_CMD_PEER_UPDATE_T {

	UINT_8 aucPeerMac[6];

	UINT_8 aucSupChan[TDLS_CMD_PEER_UPDATE_SUP_CHAN_MAX];

	UINT_16 u2StatusCode;

	UINT_8 aucSupRate[TDLS_CMD_PEER_UPDATE_SUP_RATE_MAX];
	UINT_16 u2SupRateLen;

	UINT_8 UapsdBitmap;
	UINT_8 UapsdMaxSp;	/* MAX_SP */

	UINT_16 u2Capability;

	UINT_8 aucExtCap[TDLS_CMD_PEER_UPDATE_EXT_CAP_MAXLEN];
	UINT_16 u2ExtCapLen;

	TDLS_CMD_PEER_UPDATE_HT_CAP_T rHtCap;
	TDLS_CMD_PEER_UPDATE_VHT_CAP_T rVHtCap;

	BOOLEAN fgIsSupHt;
	ENUM_STA_TYPE_T eStaType;

} TDLS_CMD_PEER_UPDATE_T;

/* Command to TDLS core module */
typedef enum _TDLS_CMD_CORE_ID {
	TDLS_CORE_CMD_TEST_NULL_RCV = 0x00
} TDLS_CMD_CORE_ID;

typedef struct _TDLS_CMD_CORE_TEST_NULL_RCV_T {

	UINT_32 u4PM;
} TDLS_CMD_CORE_TEST_NULL_RCV_T;

typedef struct _TDLS_CMD_CORE_T {

	UINT_32 u4Command;

	UINT_8 aucPeerMac[6];

#define TDLS_CMD_CORE_RESERVED_SIZE					50
	union {
		TDLS_CMD_CORE_TEST_NULL_RCV_T rCmdNullRcv;
		UINT_8 Reserved[TDLS_CMD_CORE_RESERVED_SIZE];
	} Content;
} TDLS_CMD_CORE_T;

typedef enum _TDLS_EVENT_HOST_ID {
	TDLS_HOST_EVENT_TEAR_DOWN = 0x00,
	TDLS_HOST_EVENT_TX_DONE
} TDLS_EVENT_HOST_ID;

typedef enum _TDLS_EVENT_HOST_SUBID_TEAR_DOWN {
	TDLS_HOST_EVENT_TD_PTI_TIMEOUT = 0x00,
	TDLS_HOST_EVENT_TD_AGE_TIMEOUT,
	TDLS_HOST_EVENT_TD_PTI_SEND_FAIL,
	TDLS_HOST_EVENT_TD_PTI_SEND_MAX_FAIL,
	TDLS_HOST_EVENT_TD_WRONG_NETWORK_IDX,
	TDLS_HOST_EVENT_TD_NON_STATE3,
	TDLS_HOST_EVENT_TD_LOST_TEAR_DOWN
} TDLS_EVENT_HOST_SUBID_TEAR_DOWN;

typedef enum _TDLS_REASON_CODE {
	TDLS_REASON_CODE_UNREACHABLE = 25,
	TDLS_REASON_CODE_UNSPECIFIED = 26,

	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_UNKNOWN = 0x80,	/* 128 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_WIFI_OFF = 0x81,	/* 129 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_ROAMING = 0x82,	/* 130 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_PTI_TIMEOUT = 0x83,	/* 131 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_AGE_TIMEOUT = 0x84,	/* 132 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_REKEY = 0x85,	/* 133 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_PTI_SEND_FAIL = 0x86,	/* 134 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_PTI_SEND_MAX_FAIL = 0x87,	/* 135 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_WRONG_NETWORK_IDX = 0x88,	/* 136 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_NON_STATE3 = 0x89,	/* 137 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_TX_QUOTA_EMPTY = 0x8a,	/* 138 */
	TDLS_REASON_CODE_MTK_DIS_BY_US_DUE_TO_LOST_TEAR_DOWN = 0x8b	/* 139 */
} TDLS_REASON_CODE;

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
UINT_32 TdlsFrameGeneralIeAppend(ADAPTER_T *prAdapter, STA_RECORD_T *prStaRec, UINT_8 *pPkt);

WLAN_STATUS			/* TDLS_STATUS */

TdlsDataFrameSend_TearDown(ADAPTER_T *prAdapter,
			   STA_RECORD_T *prStaRec,
			   UINT_8 *pPeerMac,
			   UINT_8 ucActionCode,
			   UINT_8 ucDialogToken, UINT_16 u2StatusCode, UINT_8 *pAppendIe, UINT_32 AppendIeLen);

WLAN_STATUS			/* TDLS_STATUS */

TdlsDataFrameSend_CONFIRM(ADAPTER_T *prAdapter,
			  STA_RECORD_T *prStaRec,
			  UINT_8 *pPeerMac,
			  UINT_8 ucActionCode,
			  UINT_8 ucDialogToken, UINT_16 u2StatusCode, UINT_8 *pAppendIe, UINT_32 AppendIeLen);

WLAN_STATUS			/* TDLS_STATUS */

TdlsDataFrameSend_SETUP_REQ(ADAPTER_T *prAdapter,
			    STA_RECORD_T *prStaRec,
			    UINT_8 *pPeerMac,
			    UINT_8 ucActionCode,
			    UINT_8 ucDialogToken, UINT_16 u2StatusCode, UINT_8 *pAppendIe, UINT_32 AppendIeLen);

WLAN_STATUS			/* TDLS_STATUS */

TdlsDataFrameSend_DISCOVERY_REQ(ADAPTER_T *prAdapter,
				STA_RECORD_T *prStaRec,
				UINT_8 *pPeerMac,
				UINT_8 ucActionCode,
				UINT_8 ucDialogToken, UINT_16 u2StatusCode, UINT_8 *pAppendIe, UINT_32 AppendIeLen);

WLAN_STATUS			/* TDLS_STATUS */

TdlsDataFrameSend_SETUP_RSP(ADAPTER_T *prAdapter,
			    STA_RECORD_T *prStaRec,
			    UINT_8 *pPeerMac,
			    UINT_8 ucActionCode,
			    UINT_8 ucDialogToken, UINT_16 u2StatusCode, UINT_8 *pAppendIe, UINT_32 AppendIeLen);

WLAN_STATUS			/* TDLS_STATUS */

TdlsDataFrameSend_DISCOVERY_RSP(ADAPTER_T *prAdapter,
				STA_RECORD_T *prStaRec,
				UINT_8 *pPeerMac,
				UINT_8 ucActionCode,
				UINT_8 ucDialogToken, UINT_16 u2StatusCode, UINT_8 *pAppendIe, UINT_32 AppendIeLen);

UINT_32 TdlsexLinkOper(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen);

UINT_32 TdlsexLinkMgt(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen);

VOID TdlsexEventHandle(P_GLUE_INFO_T prGlueInfo, UINT_8 *prInBuf, UINT_32 u4InBufLen);

VOID TdlsEventTearDown(GLUE_INFO_T *prGlueInfo, UINT_8 *prInBuf, UINT_32 u4InBufLen);

VOID TdlsBssExtCapParse(P_STA_RECORD_T prStaRec, P_UINT_8 pucIE);

WLAN_STATUS
TdlsSendChSwControlCmd(P_ADAPTER_T prAdapter, PVOID pvSetBuffer, UINT_32 u4SetBufferLen, PUINT_32 pu4SetInfoLen);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* CFG_SUPPORT_TDLS */

#endif /* _TDLS_H */
