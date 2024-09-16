/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _NIC_CMD_EVENT_H
#define _NIC_CMD_EVENT_H

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
#define CMD_STATUS_SUCCESS      0
#define CMD_STATUS_REJECTED     1
#define CMD_STATUS_UNKNOWN      2

#define EVENT_HDR_SIZE          OFFSET_OF(WIFI_EVENT_T, aucBuffer[0])

#define MAX_IE_LENGTH       (600)
#define MAX_WSC_IE_LENGTH   (400)
#define MAX_FW_LOG_LENGTH   (896)

/* Action field in structure CMD_CH_PRIVILEGE_T */
#define CMD_CH_ACTION_REQ           0
#define CMD_CH_ACTION_ABORT         1

/* Status field in structure EVENT_CH_PRIVILEGE_T */
#define EVENT_CH_STATUS_GRANT       0

#define SCN_PSCAN_SWC_RSSI_WIN_MAX  75
#define SCN_PSCAN_SWC_MAX_NUM       8
#define SCN_PSCAN_HOTLIST_REPORT_MAX_NUM 8

typedef enum _ENUM_CMD_ID_T {
	CMD_ID_TEST_MODE = 1,	/* 0x01 (Set) */
	CMD_ID_RESET_REQUEST,	/* 0x02 (Set) */
	CMD_ID_BUILD_CONNECTION,	/* 0x03 (Set) */
	CMD_ID_SCAN_REQ_V2,	/* 0x04 (Set) */
	CMD_ID_NIC_POWER_CTRL,	/* 0x05 (Set) */
	CMD_ID_POWER_SAVE_MODE,	/* 0x06 (Set) */
	CMD_ID_LINK_ATTRIB,	/* 0x07 (Set) */
	CMD_ID_ADD_REMOVE_KEY,	/* 0x08 (Set) */
	CMD_ID_DEFAULT_KEY_ID,	/* 0x09 (Set) */
	CMD_ID_INFRASTRUCTURE,	/* 0x0a (Set) */
	CMD_ID_SET_RX_FILTER,	/* 0x0b (Set) */
	CMD_ID_DOWNLOAD_BUF,	/* 0x0c (Set) */
	CMD_ID_WIFI_START,	/* 0x0d (Set) */
	CMD_ID_CMD_BT_OVER_WIFI,	/* 0x0e (Set) */
	CMD_ID_SET_MEDIA_CHANGE_DELAY_TIME,	/* 0x0f (Set) */
	CMD_ID_SEND_ADDBA_RSP,	/* 0x10 (Set) */
	CMD_ID_WAPI_MODE,	/* 0x11 (Set)  (obsolete) */
	CMD_ID_WAPI_ASSOC_INFO,	/* 0x12 (Set)  (obsolete) */
	CMD_ID_SET_DOMAIN_INFO,	/* 0x13 (Set) */
	CMD_ID_SET_IP_ADDRESS,	/* 0x14 (Set) */
	CMD_ID_BSS_ACTIVATE_CTRL,	/* 0x15 (Set) */
	CMD_ID_SET_BSS_INFO,	/* 0x16 (Set) */
	CMD_ID_UPDATE_STA_RECORD,	/* 0x17 (Set) */
	CMD_ID_REMOVE_STA_RECORD,	/* 0x18 (Set) */
	CMD_ID_INDICATE_PM_BSS_CREATED,	/* 0x19 (Set) */
	CMD_ID_INDICATE_PM_BSS_CONNECTED,	/* 0x1a (Set) */
	CMD_ID_INDICATE_PM_BSS_ABORT,	/* 0x1b (Set) */
	CMD_ID_UPDATE_BEACON_CONTENT,	/* 0x1c (Set) */
	CMD_ID_SET_BSS_RLM_PARAM,	/* 0x1d (Set) */
	CMD_ID_SCAN_REQ,	/* 0x1e (Set) */
	CMD_ID_SCAN_CANCEL,	/* 0x1f (Set) */
	CMD_ID_CH_PRIVILEGE,	/* 0x20 (Set) */
	CMD_ID_UPDATE_WMM_PARMS,	/* 0x21 (Set) */
	CMD_ID_SET_WMM_PS_TEST_PARMS,	/* 0x22 (Set) */
	CMD_ID_TX_AMPDU,	/* 0x23 (Set) */
	CMD_ID_ADDBA_REJECT,	/* 0x24 (Set) */
	CMD_ID_SET_PS_PROFILE_ADV,	/* 0x25 (Set) */
	CMD_ID_SET_RAW_PATTERN,	/* 0x26 (Set) */
	CMD_ID_CONFIG_PATTERN_FUNC,	/* 0x27 (Set) */
	CMD_ID_SET_TX_PWR,	/* 0x28 (Set) */
	CMD_ID_SET_5G_PWR_OFFSET,	/* 0x29 (Set) */
	CMD_ID_SET_PWR_PARAM,	/* 0x2A (Set) */
	CMD_ID_P2P_ABORT,	/* 0x2B (Set) */
#if CFG_STRESS_TEST_SUPPORT
	CMD_ID_RANDOM_RX_RESET_EN = 0x2C,	/* 0x2C (Set ) */
	CMD_ID_RANDOM_RX_RESET_DE = 0x2D,	/* 0x2D (Set ) */
	CMD_ID_SAPP_EN = 0x2E,	/* 0x2E (Set ) */
	CMD_ID_SAPP_DE = 0x2F,	/* 0x2F (Set ) */
#endif
	CMD_ID_ROAMING_TRANSIT = 0x30,	/* 0x30 (Set) */
	CMD_ID_SET_PHY_PARAM,	/* 0x31 (Set) */
	CMD_ID_SET_NOA_PARAM,	/* 0x32 (Set) */
	CMD_ID_SET_OPPPS_PARAM,	/* 0x33 (Set) */
	CMD_ID_SET_UAPSD_PARAM,	/* 0x34 (Set) */
	CMD_ID_SET_SIGMA_STA_SLEEP,	/* 0x35 (Set) */
	CMD_ID_SET_EDGE_TXPWR_LIMIT,	/* 0x36 (Set) */
	CMD_ID_SET_DEVICE_MODE,	/* 0x37 (Set) */
	CMD_ID_SET_TXPWR_CTRL,	/* 0x38 (Set) */
	CMD_ID_SET_AUTOPWR_CTRL,	/* 0x39 (Set) */
	CMD_ID_SET_WFD_CTRL,	/* 0x3A (Set) */
	CMD_ID_SET_5G_EDGE_TXPWR_LIMIT,	/* 0x3B (Set) */
	CMD_ID_SET_RSSI_COMPENSATE,	/* 0x3C (Set) */
	CMD_ID_SET_BAND_SUPPORT = 0x3D,	/* 0x3D (Set) */
	CMD_ID_SET_NLO_REQ,	/* 0x3E (Set) */
	CMD_ID_SET_NLO_CANCEL,	/* 0x3F (Set) */
	CMD_ID_SET_BATCH_REQ,	/* 0x40 (Set) */
	CMD_ID_SET_WOWLAN,	/* 0x41 (Set) */ /*CFG_SUPPORT_WOWLAN */
	CMD_ID_GET_PSCAN_CAPABILITY = 0x42,	/* 0x42 (Set) */
	CMD_ID_SET_PSCN_ENABLE = 0x43,	/* 0x43 (Set) */
	CMD_ID_SET_PSCAN_PARAM = 0x44,	/* 0x44 (Set) */
	CMD_ID_SET_PSCN_ADD_HOTLIST_BSSID = 0x45,	/* 0x45 (Set) */
	CMD_ID_SET_PSCN_ADD_SW_BSSID = 0x46,	/* 0x46 (Set) */
	CMD_ID_SET_PSCN_MAC_ADDR = 0x47,	/* 0x47 (Set) */
	CMD_ID_GET_GSCN_SCN_RESULT = 0x48,	/* 0x48 (Get) */
	CMD_ID_SET_COUNTRY_POWER_LIMIT = 0x4A,	/* 0x4A (Set) */
	CMD_ID_SET_RRM_CAPABILITY = 0x59, /* 0x59 (Set) */
	CMD_ID_SET_MAX_TXPWR_LIMIT = 0x5A, /* 0x5A (Set) */
	CMD_ID_REQ_CHNL_UTILIZATION = 0x5C, /* 0x5C (Get) */
#if CFG_SUPPORT_P2P_ECSA
	CMD_ID_SET_ECSA_PARAM = 0x5D,		/* 0x5D (Set) */
#endif
	CMD_ID_SET_TSM_STATISTICS_REQUEST = 0x5E,
	CMD_ID_GET_TSM_STATISTICS = 0x5F,
	CMD_ID_SET_SYSTEM_SUSPEND = 0x60,	/* 0x60 (Set) */
	CMD_ID_UPDATE_AC_PARMS = 0x6A,		/* 0x6A (Set) */
	CMD_ID_SET_CTIA_MODE_STATUS = 0x6B,		/* 0x6B (Set) */
	CMD_ID_SET_ROAMING_SKIP = 0x6D, /* 0x6D (Set) */
	CMD_ID_SET_DROP_PACKET_CFG = 0x6E,   /* 0x6E (Set) */
#if (CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST || CFG_SUPPORT_FCC_POWER_BACK_OFF)
	CMD_ID_SET_FCC_TX_PWR_CERT = 0x6F,	/* 0x6F (Set) */
#endif
#ifdef FW_CFG_SUPPORT
		CMD_ID_GET_SET_CUSTOMER_CFG = 0x70,
#endif
	CMD_ID_SET_ALWAYS_SCAN_PARAM = 0x73,/*0x73(set)*/
	CMD_ID_SET_RX_BA_WIN_SIZE = 0x74,	/* 0x74 (Set) */
	CMD_ID_TDLS_PS = 0x75,	/* 0x75 (Set) */
#if CFG_SUPPORT_EMI_DEBUG
	CMD_ID_DRIVER_DUMP_EMI_LOG = 0x76,      /* 0x76 (Set) */
#endif
	CMD_ID_GET_NIC_CAPABILITY = 0x80,	/* 0x80 (Query) */
	CMD_ID_GET_LINK_QUALITY,	/* 0x81 (Query) */
	CMD_ID_GET_STATISTICS,	/* 0x82 (Query) */
	CMD_ID_GET_CONNECTION_STATUS,	/* 0x83 (Query) */
	CMD_ID_GET_ASSOC_INFO,	/* 0x84 (Query) (obsolete) */
	CMD_ID_GET_STA_STATISTICS = 0x85,	/* 0x85 (Query) */
	CMD_ID_GET_DEBUG_CODE = 0x86,	/* 0x86 (Query) */
	CMD_ID_GET_LTE_CHN = 0x87,	/* 0x87 (Query) */
	CMD_ID_GET_CHN_LOADING = 0x88,	/* 0x88 (Query) */
	CMD_ID_GET_STATISTICS_PL = 0x89,	/* 0x87 (Query) */
#if CFG_SUPPORT_GAMING_MODE
	CMD_ID_SET_GAMING_MODE = 0x8B,	/* 0x8B (Set) */
#endif /* CFG_SUPPORT_GAMING_MODE */
#if CFG_SUPPORT_OSHARE
		CMD_ID_SET_OSHARE_MODE = 0x8C,
#endif
	CMD_ID_WIFI_LOG_LEVEL  = 0x8D,	/* 0x8D (Set / Query) */
	CMD_ID_WFC_KEEP_ALIVE = 0xa0,	/* 0xa0(Set) */
	CMD_ID_RSSI_MONITOR = 0xa1,	/* 0xa1(Set) */
	CMD_ID_BASIC_CONFIG = 0xc1,	/* 0xc1 (Set / Query) */
	CMD_ID_ACCESS_REG,	/* 0xc2 (Set / Query) */
	CMD_ID_MAC_MCAST_ADDR,	/* 0xc3 (Set / Query) */
	CMD_ID_802_11_PMKID,	/* 0xc4 (Set / Query) */
	CMD_ID_ACCESS_EEPROM,	/* 0xc5 (Set / Query) */
	CMD_ID_SW_DBG_CTRL,	/* 0xc6 (Set / Query) */
#if 1				/* CFG_SUPPORT_ANTI_PIRACY */
	CMD_ID_SEC_CHECK,	/* 0xc7 (Set / Query) */
#endif
	CMD_ID_DUMP_MEM,	/* 0xc8 (Query) */
#if CFG_SUPPORT_TX_POWER_BACK_OFF
	CMD_ID_SET_TX_PWR_OFFSET = 0xC9,	/* 0xc9 (Set) */
#endif
	CMD_ID_CHIP_CONFIG = 0xCA,	/* 0xca (Set / Query) */
#if CFG_SUPPORT_TX_POWER_BACK_OFF
	CMD_ID_SET_TX_PWR_BACKOFF = 0xCC,	/* 0xcc (Set) */
#endif
#if CFG_SUPPORT_RDD_TEST_MODE
	CMD_ID_SET_RDD_CH = 0xE1,
#endif

	CMD_ID_SET_NVRAM_SETTINGS = 0xEF,
	CMD_ID_SET_BWCS = 0xF1,
	CMD_ID_SET_ROAMING_INFO = 0xF3,

#if CFG_SUPPORT_BUILD_DATE_CODE
	CMD_ID_GET_BUILD_DATE_CODE = 0xF8,
#endif
	CMD_ID_GET_BSS_INFO = 0xF9,
#if 1				/* CFG_SUPPORT_HOTSPOT_OPTIMIZATION */
	CMD_ID_SET_HOTSPOT_OPTIMIZATION = 0xFA,	/* 0xFA (Set) */
#endif

	CMD_ID_TDLS_CORE = 0xFC,
	CMD_ID_STATS = 0xFD,
	CMD_ID_TX_AR_ERR_CONFIG = 0xFF
} ENUM_CMD_ID_T, *P_ENUM_CMD_ID_T;

typedef enum _ENUM_EVENT_ID_T {
	EVENT_ID_CMD_RESULT = 1,	/* 0x01 (Query) */
	EVENT_ID_NIC_CAPABILITY,	/* 0x02 (Query) */
	EVENT_ID_CONNECTION_STATUS,	/* 0x03 (Query / Unsolicited) (obsolete) */
	EVENT_ID_SCAN_RESULT,	/* 0x04 (Query / Unsolicited) (obselete) */
	EVENT_ID_LINK_QUALITY,	/* 0x05 (Query / Unsolicited) */
	EVENT_ID_STATISTICS,	/* 0x06 (Query) */
	EVENT_ID_MIC_ERR_INFO,	/* 0x07 (Unsolicited) */
	EVENT_ID_ASSOC_INFO,	/* 0x08 (Query - CMD_ID_GET_ASSOC_INFO) */
	EVENT_ID_BASIC_CONFIG,	/* 0x09 (Query - CMD_ID_BASIC_CONFIG) */
	EVENT_ID_ACCESS_REG,	/* 0x0a (Query - CMD_ID_ACCESS_REG) */
	EVENT_ID_MAC_MCAST_ADDR,	/* 0x0b (Query - CMD_ID_MAC_MCAST_ADDR) */
	EVENT_ID_802_11_PMKID,	/* 0x0c (Query - CMD_ID_802_11_PMKID) */
	EVENT_ID_ACCESS_EEPROM,	/* 0x0d (Query - CMD_ID_ACCESS_EEPROM) */
	EVENT_ID_SLEEPY_NOTIFY,	/* 0x0e (Query) */
	EVENT_ID_BT_OVER_WIFI,	/* 0x0f (Unsolicited) */
	EVENT_ID_TEST_STATUS,	/* 0x10 (Query - CMD_ID_TEST_MODE) */
	EVENT_ID_RX_ADDBA,	/* 0x11 (Unsolicited) (obsolete) */
	EVENT_ID_RX_DELBA,	/* 0x12 (Unsolicited) (obsolete) */
	EVENT_ID_ACTIVATE_STA_REC_T,	/* 0x13 (Unsolicited) */
	EVENT_ID_DEACTIVATE_STA_REC_T,	/* 0x14 (Unsolicited) */
	EVENT_ID_SCAN_DONE,	/* 0x15 (Unsoiicited) */
	EVENT_ID_RX_FLUSH,	/* 0x16 (Unsolicited) */
	EVENT_ID_TX_DONE,	/* 0x17 (Unsolicited) */
	EVENT_ID_CH_PRIVILEGE,	/* 0x18 (Unsolicited) */
	EVENT_ID_BSS_ABSENCE_PRESENCE = 0x19,	/* 0x19 (Unsolicited) */
	EVENT_ID_STA_CHANGE_PS_MODE,	/* 0x1A (Unsolicited) */
	EVENT_ID_BSS_BEACON_TIMEOUT,	/* 0x1B (Unsolicited) */
	EVENT_ID_UPDATE_NOA_PARAMS,	/* 0x1C (Unsolicited) */
	EVENT_ID_AP_OBSS_STATUS,	/* 0x1D (Unsolicited) */
	EVENT_ID_STA_UPDATE_FREE_QUOTA,	/* 0x1E (Unsolicited) */
	EVENT_ID_SW_DBG_CTRL,	/* 0x1F (Query - CMD_ID_SW_DBG_CTRL) */
	EVENT_ID_ROAMING_STATUS,	/* 0x20 (Unsolicited) */
	EVENT_ID_STA_AGING_TIMEOUT,	/* 0x21 (Unsolicited) */
#if 1				/* CFG_SUPPORT_ANTI_PIRACY */
	EVENT_ID_SEC_CHECK_RSP,	/* 0x22 (Unsolicited) */
#endif
	EVENT_ID_SEND_DEAUTH,	/* 0x23 (Unsolicited) */

#if CFG_SUPPORT_RDD_TEST_MODE
	EVENT_ID_UPDATE_RDD_STATUS,	/* 0x24 (Unsolicited) */
#endif

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	EVENT_ID_UPDATE_BWCS_STATUS = 0x25,	/* 0x25 (Unsolicited) */
	EVENT_ID_UPDATE_BCM_DEBUG,	/* 0x26 (Unsolicited) */
#endif
	EVENT_ID_RX_ERR,
	EVENT_ID_DUMP_MEM,
	EVENT_ID_STA_STATISTICS = 0x29,	/* 0x29 (Query ) */
	EVENT_ID_STA_STATISTICS_UPDATE,	/* 0x2A (Unsolicited) */
	EVENT_ID_NLO_DONE = 0x2b,

	EVENT_ID_GSCAN_CAPABILITY = 0x30,
	EVENT_ID_GSCAN_SCAN_COMPLETE = 0x31,
	EVENT_ID_GSCAN_FULL_RESULT = 0x32,
	EVENT_ID_GSCAN_SIGNIFICANT_CHANGE = 0x33,
	EVENT_ID_GSCAN_GEOFENCE_FOUND = 0x34,
	EVENT_ID_GSCAN_SCAN_AVAILABLE = 0x35,
	EVENT_ID_GSCAN_RESULT = 0x36,
	EVENT_ID_BATCH_RESULT = 0x37,

	EVENT_ID_CHECK_REORDER_BUBBLE = 0x39,
#if CFG_SUPPORT_P2P_ECSA
		EVENT_ID_ECSA_RESULT = 0x3D,
#endif
	EVENT_ID_ADD_PKEY_DONE = 0x44, /* 0x44 (Unsolicited) */
	EVENT_ID_GET_TSM_STATISTICS = 0x47,



#if CFG_RX_BA_REORDERING_ENHANCEMENT
	EVENT_ID_BA_FW_DROP_SN = 0x51,
#endif
	EVENT_ID_RSP_CHNL_UTILIZATION = 0x59, /* 0x59 (Query - CMD_ID_REQ_CHNL_UTILIZATION) */
#if CFG_SUPPORT_EMI_DEBUG
	EVENT_ID_DRIVER_DUMP_LOG = 0x76, /*request driver to dump EMI message*/
#endif
	EVENT_ID_TDLS = 0x80,
	EVENT_ID_STATS_ENV = 0x81,
	EVENT_ID_WIFI_LOG_LEVEL  = 0x8D,
	EVENT_ID_RSSI_MONITOR = 0xa1,

#if CFG_SUPPORT_BUILD_DATE_CODE
	EVENT_ID_BUILD_DATE_CODE = 0xF8,
#endif
	EVENT_ID_GET_AIS_BSS_INFO = 0xF9,
	EVENT_ID_DEBUG_CODE = 0xFB,
	EVENT_ID_RFTEST_READY = 0xFC,	/* 0xFC */
	EVENT_ID_TX_DONE_STATUS = 0xFD,
	EVENT_ID_FW_LOG_ENV = 0xFE,	/* 0xFE, FW real time debug log */
} ENUM_EVENT_ID_T, *P_ENUM_EVENT_ID_T;

#if CFG_SUPPORT_P2P_ECSA
typedef enum _ENUM_ECSA_STATE_T {
	ECSA_EVENT_STATUS_SUCCESS = 0,
	ECSA_EVENT_STATUS_UPDATE_BEACON = 1, /*Notify Driver to update GO’s ECSA/CSA IE*/
	ECSA_EVENT_STATUS_INVALID_PARAM = 2,
	ECSA_EVENT_STATUS_CHNL_SWITCH_FAILED = 3,
	ECSA_EVENT_STATUS_UNACCEPTABLE = 4,
	ECSA_EVENT_STATUS_NUM,
} ENUM_ECSA_STATE_T;
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#ifndef LINUX
typedef UINT_8 CMD_STATUS;
#endif

#if (CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST || CFG_SUPPORT_FCC_POWER_BACK_OFF)
/* TX Power Adjust For FCC/CE Certification */
typedef struct _CMD_FCC_TX_PWR_ADJUST_T {
	UINT_8 fgFccTxPwrAdjust;
	UINT_8 Offset_CCK;      /* Offset for CH 11~14 */
	UINT_8 Offset_HT20;     /* Offset for CH 11~14 */
	UINT_8 Offset_HT40;     /* Offset for CH 11~14 */
	UINT_8 Channel_CCK[2];  /* [0] for start channel, [1] for ending channel */
	UINT_8 Channel_HT20[2]; /* [0] for start channel, [1] for ending channel */
	UINT_8 Channel_HT40[2]; /* [0] for start channel, [1] for ending channel */
	UINT_8 Channel_Bandedge[2]; /* Set specical bandedge for flight mode
								  *[0] for start channel, [1] for ending channel
								  */
} CMD_FCC_TX_PWR_ADJUST, *P_CMD_FCC_TX_PWR_ADJUST;
#endif

typedef struct _EVENT_TX_DONE_STATUS_T {
	UINT_8 ucPacketSeq;
	UINT_8 ucStatus;
	UINT_16 u2SequenceNumber;
	UINT_32 au4Reserved1;
	UINT_32 au4Reserved2;
	UINT_32 au4Reserved3;
	UINT_32 u4PktBufInfo;
	UINT_8 aucPktBuf[200];
} EVENT_TX_DONE_STATUS_T, *P_EVENT_TX_DONE_STATUS_T;

/* for Event Packet (via HIF-RX) */
    /* following CM's documentation v0.7 */
typedef struct _WIFI_CMD_T {
	UINT_16 u2TxByteCount_UserPriority;
	UINT_8 ucEtherTypeOffset;
	UINT_8 ucResource_PktType_CSflags;
	UINT_8 ucCID;
	UINT_8 ucSetQuery;
	UINT_8 ucSeqNum;
	UINT_8 aucReserved2;

	UINT_8 aucBuffer[0];
} WIFI_CMD_T, *P_WIFI_CMD_T;

/* for Command Packet (via HIF-TX) */
    /* following CM's documentation v0.7 */
typedef struct _WIFI_EVENT_T {
	UINT_16 u2PacketLen;
	UINT_16 u2PacketType;
	UINT_8 ucEID;
	UINT_8 ucSeqNum;
	UINT_8 aucReserved2[2];

	UINT_8 aucBuffer[0];
} WIFI_EVENT_T, *P_WIFI_EVENT_T;

/* CMD_ID_TEST_MODE */
typedef struct _CMD_TEST_CTRL_T {
	UINT_8 ucAction;
	UINT_8 aucReserved[3];
	union {
		UINT_32 u4OpMode;
		UINT_32 u4ChannelFreq;
		PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	} u;
} CMD_TEST_CTRL_T, *P_CMD_TEST_CTRL_T;

/* EVENT_TEST_STATUS */
typedef struct _PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T {
	UINT_32 u4PktSentStatus;
	UINT_32 u4PktSentCount;
	UINT_16 u2AvgAlc;
	UINT_8 ucCckGainControl;
	UINT_8 ucOfdmGainControl;
} PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T, *P_PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T;

typedef struct _PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T {
	UINT_32 u4IntRxOk;	/*!< number of packets that Rx ok from interrupt */
	UINT_32 u4IntCrcErr;	/*!< number of packets that CRC error from interrupt */
	UINT_32 u4IntShort;	/*!< number of packets that is short preamble from interrupt */
	UINT_32 u4IntLong;	/*!< number of packets that is long preamble from interrupt */
	UINT_32 u4PauRxPktCount;	/*!< number of packets that Rx ok from PAU */
	UINT_32 u4PauCrcErrCount;	/*!< number of packets that CRC error from PAU */
	UINT_32 u4PauRxFifoFullCount;	/*!< number of packets that is short preamble from PAU */
	UINT_32 u4PauCCACount;	/*!< CCA rising edge count */
} PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T, *P_PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T;

typedef union _EVENT_TEST_STATUS {
	PARAM_MTK_WIFI_TEST_STRUCT_T rATInfo;
/* PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T   rTxStatus; */
/* PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T   rRxStatus; */
} EVENT_TEST_STATUS, *P_EVENT_TEST_STATUS;

/* CMD_BUILD_CONNECTION */
typedef struct _CMD_BUILD_CONNECTION {
	UINT_8 ucInfraMode;
	UINT_8 ucAuthMode;
	UINT_8 ucEncryptStatus;
	UINT_8 ucSsidLen;
	UINT_8 aucSsid[PARAM_MAX_LEN_SSID];
	UINT_8 aucBssid[PARAM_MAC_ADDR_LEN];

	/* Ad-hoc mode */
	UINT_16 u2BeaconPeriod;
	UINT_16 u2ATIMWindow;
	UINT_8 ucJoinOnly;
	UINT_8 ucReserved;
	UINT_32 u4FreqInKHz;

	/* for faster connection */
	UINT_8 aucScanResult[0];
} CMD_BUILD_CONNECTION, *P_CMD_BUILD_CONNECTION;

/* CMD_ADD_REMOVE_KEY */
typedef struct _CMD_802_11_KEY {
	UINT_8 ucAddRemove;
	UINT_8 ucTxKey;
	UINT_8 ucKeyType;
	UINT_8 ucIsAuthenticator;
	UINT_8 aucPeerAddr[6];
	UINT_8 ucNetType;
	UINT_8 ucAlgorithmId;
	UINT_8 ucKeyId;
	UINT_8 ucKeyLen;
	UINT_8 aucReverved[2];
	UINT_8 aucKeyMaterial[32];
	UINT_8 aucKeyRsc[16];
} CMD_802_11_KEY, *P_CMD_802_11_KEY;

/* WPA2 PMKID cache structure */
typedef struct _PMKID_ENTRY_T {
#if (CFG_REFACTORY_PMKSA == 0)
	PARAM_BSSID_INFO_T rBssidInfo;
	BOOLEAN fgPmkidExist;
#else
	LINK_ENTRY_T rLinkEntry;
	PARAM_PMKID_T rBssidInfo;
#endif
} PMKID_ENTRY_T, *P_PMKID_ENTRY_T;

typedef struct _CMD_802_11_PMKID {
	ULONG u4BSSIDInfoCount;
	P_PMKID_ENTRY_T arPMKIDInfo[1];
} CMD_802_11_PMKID, *P_CMD_802_11_PMKID;

/* CMD_BASIC_CONFIG */
typedef struct _CMD_CSUM_OFFLOAD {
	UINT_16 u2RxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
	UINT_16 u2TxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
} CMD_CSUM_OFFLOAD, *P_CMD_CSUM_OFFLOAD;

typedef struct _CMD_BASIC_CONFIG {
	PARAM_MAC_ADDRESS rMyMacAddr;
	UINT_8 ucNative80211;
	UINT_8 aucReserved[1];

	CMD_CSUM_OFFLOAD rCsumOffload;
} CMD_BASIC_CONFIG, *P_CMD_BASIC_CONFIG, EVENT_BASIC_CONFIG, *P_EVENT_BASIC_CONFIG;

/* CMD_MAC_MCAST_ADDR */
typedef struct _CMD_MAC_MCAST_ADDR {
	UINT_32 u4NumOfGroupAddr;
	UINT_8 ucNetTypeIndex;
	UINT_8 aucReserved[3];
	PARAM_MAC_ADDRESS arAddress[MAX_NUM_GROUP_ADDR];
} CMD_MAC_MCAST_ADDR, *P_CMD_MAC_MCAST_ADDR, EVENT_MAC_MCAST_ADDR, *P_EVENT_MAC_MCAST_ADDR;

/* CMD_ACCESS_EEPROM */
typedef struct _CMD_ACCESS_EEPROM {
	UINT_16 u2Offset;
	UINT_16 u2Data;
} CMD_ACCESS_EEPROM, *P_CMD_ACCESS_EEPROM, EVENT_ACCESS_EEPROM, *P_EVENT_ACCESS_EEPROM;

typedef struct _CMD_CUSTOM_NOA_PARAM_STRUCT_T {
	UINT_32 u4NoaDurationMs;
	UINT_32 u4NoaIntervalMs;
	UINT_32 u4NoaCount;
} CMD_CUSTOM_NOA_PARAM_STRUCT_T, *P_CMD_CUSTOM_NOA_PARAM_STRUCT_T;

typedef struct _CMD_CUSTOM_OPPPS_PARAM_STRUCT_T {
	UINT_32 u4CTwindowMs;
} CMD_CUSTOM_OPPPS_PARAM_STRUCT_T, *P_CMD_CUSTOM_OPPPS_PARAM_STRUCT_T;

typedef struct _CMD_CUSTOM_UAPSD_PARAM_STRUCT_T {
	UINT_8 fgEnAPSD;
	UINT_8 fgEnAPSD_AcBe;
	UINT_8 fgEnAPSD_AcBk;
	UINT_8 fgEnAPSD_AcVo;
	UINT_8 fgEnAPSD_AcVi;
	UINT_8 ucMaxSpLen;
	UINT_8 aucResv[2];
} CMD_CUSTOM_UAPSD_PARAM_STRUCT_T, *P_CMD_CUSTOM_UAPSD_PARAM_STRUCT_T;

struct CMD_SET_MAX_TXPWR_LIMIT {
	UINT_8 ucMaxTxPwrLimitEnable;
	INT_8 cMaxTxPwr; /* in unit of 0.5 dBm */
	INT_8 cMinTxPwr; /* in unit of 0.5 dBm */
	UINT_8 ucReserved;
};

struct CMD_SET_RRM_CAPABILITY {
	UINT_8 ucDot11RadioMeasurementEnabled;
	UINT_8 aucCapabilities[5];
	UINT_8 aucReserved[2];
};

/* EVENT_CONNECTION_STATUS */
typedef struct _EVENT_CONNECTION_STATUS {
	UINT_8 ucMediaStatus;
	UINT_8 ucReasonOfDisconnect;

	UINT_8 ucInfraMode;
	UINT_8 ucSsidLen;
	UINT_8 aucSsid[PARAM_MAX_LEN_SSID];
	UINT_8 aucBssid[PARAM_MAC_ADDR_LEN];
	UINT_8 ucAuthenMode;
	UINT_8 ucEncryptStatus;
	UINT_16 u2BeaconPeriod;
	UINT_16 u2AID;
	UINT_16 u2ATIMWindow;
	UINT_8 ucNetworkType;
	UINT_8 aucReserved[1];
	UINT_32 u4FreqInKHz;

#if CFG_ENABLE_WIFI_DIRECT
	UINT_8 aucInterfaceAddr[PARAM_MAC_ADDR_LEN];
#endif

} EVENT_CONNECTION_STATUS, *P_EVENT_CONNECTION_STATUS;

/* EVENT_NIC_CAPABILITY */
typedef struct _EVENT_NIC_CAPABILITY {
	UINT_16 u2ProductID;
	UINT_16 u2FwVersion;
	UINT_16 u2DriverVersion;
	UINT_8 ucHw5GBandDisabled;
	UINT_8 ucEepromUsed;
	UINT_8 ucEfuseValid;
	UINT_8 ucMacAddrValid;
#if CFG_REPORT_RFBB_VERSION
	UINT_8 ucRfVersion;
	UINT_8 ucPhyVersion;
#endif
#if CFG_ENABLE_CAL_LOG
	UINT_8 ucRfCalFail;
	UINT_8 ucBbCalFail;
#endif

#define FEATURE_SET_OFFSET_TDLS					0
#define FEATURE_SET_OFFSET_5G_SUPPORT			1
	UINT_8 ucFeatureSet;	/* bit0: TDLS */

	UINT_8 aucReserved[1];
#if CFG_EMBED_FIRMWARE_BUILD_DATE_CODE
	UINT_8 aucDateCode[16];
#endif
} EVENT_NIC_CAPABILITY, *P_EVENT_NIC_CAPABILITY;

/* modified version of WLAN_BEACON_FRAME_BODY_T for simplier buffering */
typedef struct _WLAN_BEACON_FRAME_BODY_T_LOCAL {
	/* Beacon frame body */
	UINT_32 au4Timestamp[2];	/* Timestamp */
	UINT_16 u2BeaconInterval;	/* Beacon Interval */
	UINT_16 u2CapInfo;	/* Capability */
	UINT_8 aucInfoElem[MAX_IE_LENGTH];	/* Various IEs, start from SSID */
	UINT_16 u2IELength;	/* This field is *NOT* carried by F/W but caculated by nic_rx */
} WLAN_BEACON_FRAME_BODY_T_LOCAL, *P_WLAN_BEACON_FRAME_BODY_T_LOCAL;

/* EVENT_SCAN_RESULT */
typedef struct _EVENT_SCAN_RESULT_T {
	INT_32 i4RSSI;
	UINT_32 u4LinkQuality;
	UINT_32 u4DSConfig;	/* Center frequency */
	UINT_32 u4DomainInfo;	/* Require CM opinion */
	UINT_32 u4Reserved;
	UINT_8 ucNetworkType;
	UINT_8 ucOpMode;
	UINT_8 aucBssid[MAC_ADDR_LEN];
	UINT_8 aucRatesEx[PARAM_MAX_LEN_RATES_EX];
	WLAN_BEACON_FRAME_BODY_T_LOCAL rBeaconFrameBody;
} EVENT_SCAN_RESULT_T, *P_EVENT_SCAN_RESULT_T;

/* event of tkip mic error */
typedef struct _EVENT_MIC_ERR_INFO {
	UINT_32 u4Flags;
} EVENT_MIC_ERR_INFO, *P_EVENT_MIC_ERR_INFO;

typedef struct _EVENT_PMKID_CANDIDATE_LIST_T {
	UINT_32 u4Version;	/*!< Version */
	UINT_32 u4NumCandidates;	/*!< How many candidates follow */
	PARAM_PMKID_CANDIDATE_T arCandidateList[1];
} EVENT_PMKID_CANDIDATE_LIST_T, *P_EVENT_PMKID_CANDIDATE_LIST_T;

typedef struct _EVENT_CMD_RESULT {
	UINT_8 ucCmdID;
	UINT_8 ucStatus;
	UINT_8 aucReserved[2];
} EVENT_CMD_RESULT, *P_EVENT_CMD_RESULT;

/* CMD_ID_ACCESS_REG & EVENT_ID_ACCESS_REG */
typedef struct _CMD_ACCESS_REG {
	UINT_32 u4Address;
	UINT_32 u4Data;
} CMD_ACCESS_REG, *P_CMD_ACCESS_REG;

/* CMD_DUMP_MEMORY */
typedef struct _CMD_DUMP_MEM {
	UINT_32 u4Address;
	UINT_32 u4Length;
	UINT_32 u4RemainLength;
	UINT_8 ucFragNum;
} CMD_DUMP_MEM, *P_CMD_DUMP_MEM;

typedef struct _EVENT_DUMP_MEM_T {
	UINT_32 u4Address;
	UINT_32 u4Length;
	UINT_32 u4RemainLength;
	UINT_8 ucFragNum;
	UINT_8 aucBuffer[1];
} EVENT_DUMP_MEM_T, *P_EVENT_DUMP_MEM_T;

typedef struct _CMD_SW_DBG_CTRL_T {
	UINT_32 u4Id;
	UINT_32 u4Data;
	/* Debug Support */
	UINT_32 u4DebugCnt[64];
} CMD_SW_DBG_CTRL_T, *P_CMD_SW_DBG_CTRL_T;

/* CMD_ID_LINK_ATTRIB */
typedef struct _CMD_LINK_ATTRIB {
	INT_8 cRssiTrigger;
	UINT_8 ucDesiredRateLen;
	UINT_16 u2DesiredRate[32];
	UINT_8 ucMediaStreamMode;
	UINT_8 aucReserved[1];
} CMD_LINK_ATTRIB, *P_CMD_LINK_ATTRIB;

/* CMD_ID_NIC_POWER_CTRL */
typedef struct _CMD_NIC_POWER_CTRL {
	UINT_8 ucPowerMode;
	UINT_8 aucReserved[3];
} CMD_NIC_POWER_CTRL, *P_CMD_NIC_POWER_CTRL;

/* CMD_ID_POWER_SAVE_MODE */
typedef struct _CMD_PS_PROFILE_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucPsProfile;
	UINT_8 aucReserved[2];
} CMD_PS_PROFILE_T, *P_CMD_PS_PROFILE_T;

/* EVENT_LINK_QUALITY */
typedef struct _EVENT_LINK_QUALITY {
	INT_8 cRssi;
	INT_8 cLinkQuality;
	UINT_16 u2LinkSpeed;
	UINT_8 ucMediumBusyPercentage;
} EVENT_LINK_QUALITY, *P_EVENT_LINK_QUALITY;

#if CFG_SUPPORT_P2P_RSSI_QUERY
/* EVENT_LINK_QUALITY */
typedef struct _EVENT_LINK_QUALITY_EX {
	INT_8 cRssi;
	INT_8 cLinkQuality;
	UINT_16 u2LinkSpeed;
	UINT_8 ucMediumBusyPercentage;
	UINT_8 ucIsLQ0Rdy;
	INT_8 cRssiP2P;		/* For P2P Network. */
	INT_8 cLinkQualityP2P;
	UINT_16 u2LinkSpeedP2P;
	UINT_8 ucMediumBusyPercentageP2P;
	UINT_8 ucIsLQ1Rdy;
} EVENT_LINK_QUALITY_EX, *P_EVENT_LINK_QUALITY_EX;
#endif

/* EVENT_ID_STATISTICS */
typedef struct _EVENT_STATISTICS {
	LARGE_INTEGER rTransmittedFragmentCount;
	LARGE_INTEGER rMulticastTransmittedFrameCount;
	LARGE_INTEGER rFailedCount;
	LARGE_INTEGER rRetryCount;
	LARGE_INTEGER rMultipleRetryCount;
	LARGE_INTEGER rRTSSuccessCount;
	LARGE_INTEGER rRTSFailureCount;
	LARGE_INTEGER rACKFailureCount;
	LARGE_INTEGER rFrameDuplicateCount;
	LARGE_INTEGER rReceivedFragmentCount;
	LARGE_INTEGER rMulticastReceivedFrameCount;
	LARGE_INTEGER rFCSErrorCount;
} EVENT_STATISTICS, *P_EVENT_STATISTICS;

/* EVENT_ID_FW_SLEEPY_NOTIFY */
typedef struct _EVENT_SLEEPY_NOTIFY {
	UINT_8 ucSleepyState;
	UINT_8 aucReserved[3];
} EVENT_SLEEPY_NOTIFY, *P_EVENT_SLEEPY_NOTIFY;

typedef struct _EVENT_ACTIVATE_STA_REC_T {
	UINT_8 aucMacAddr[6];
	UINT_8 ucStaRecIdx;
	UINT_8 ucNetworkTypeIndex;
	BOOLEAN fgIsQoS;
	BOOLEAN fgIsAP;
	UINT_8 aucReserved[2];
} EVENT_ACTIVATE_STA_REC_T, *P_EVENT_ACTIVATE_STA_REC_T;

typedef struct _EVENT_DEACTIVATE_STA_REC_T {
	UINT_8 ucStaRecIdx;
	UINT_8 aucReserved[3];
} EVENT_DEACTIVATE_STA_REC_T, *P_EVENT_DEACTIVATE_STA_REC_T;

/* CMD_BT_OVER_WIFI */
typedef struct _CMD_BT_OVER_WIFI {
	UINT_8 ucAction;	/* 0: query, 1: setup, 2: destroy */
	UINT_8 ucChannelNum;
	PARAM_MAC_ADDRESS rPeerAddr;
	UINT_16 u2BeaconInterval;
	UINT_8 ucTimeoutDiscovery;
	UINT_8 ucTimeoutInactivity;
	UINT_8 ucRole;
	UINT_8 PAL_Capabilities;
	UINT_8 cMaxTxPower;
	UINT_8 ucChannelBand;
	UINT_8 ucReserved[1];
} CMD_BT_OVER_WIFI, *P_CMD_BT_OVER_WIFI;

/* EVENT_BT_OVER_WIFI */
typedef struct _EVENT_BT_OVER_WIFI {
	UINT_8 ucLinkStatus;
	UINT_8 ucSelectedChannel;
	INT_8 cRSSI;
	UINT_8 ucReserved[1];
} EVENT_BT_OVER_WIFI, *P_EVENT_BT_OVER_WIFI;

/* Same with DOMAIN_SUBBAND_INFO */
typedef struct _CMD_SUBBAND_INFO {
	UINT_8 ucRegClass;
	UINT_8 ucBand;
	UINT_8 ucChannelSpan;
	UINT_8 ucFirstChannelNum;
	UINT_8 ucNumChannels;
	UINT_8 aucReserved[3];
} CMD_SUBBAND_INFO, *P_CMD_SUBBAND_INFO;

/* CMD_SET_DOMAIN_INFO */
typedef struct _CMD_SET_DOMAIN_INFO_T {
	UINT_16 u2CountryCode;
	UINT_16 u2IsSetPassiveScan;	/* 0: set channel domain; 1: set passive scan channel domain */
	CMD_SUBBAND_INFO rSubBand[6];

	UINT_8 uc2G4Bandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	UINT_8 uc5GBandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	UINT_8 aucReserved[2];
} CMD_SET_DOMAIN_INFO_T, *P_CMD_SET_DOMAIN_INFO_T;

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

/* CMD_SET_PWR_LIMIT_TABLE */
typedef struct _CMD_CHANNEL_POWER_LIMIT {
	UINT_8 ucCentralCh;
	INT_8 cPwrLimitCCK;
	INT_8 cPwrLimit20;
	INT_8 cPwrLimit40;
	INT_8 cPwrLimit80;
	INT_8 cPwrLimit160;
	UINT_8 ucFlag;
	UINT_8 aucReserved[1];
} CMD_CHANNEL_POWER_LIMIT, *P_CMD_CHANNEL_POWER_LIMIT;

typedef struct _CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T {
	UINT_16 u2CountryCode;
	UINT_8 ucCountryFlag;
	UINT_8 ucNum;
	UINT_8 aucReserved[4];
	CMD_CHANNEL_POWER_LIMIT rChannelPowerLimit[1];
} CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T, *P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_T;

#endif

/* CMD_SET_IP_ADDRESS */
typedef struct _IPV4_NETWORK_ADDRESS {
	UINT_8 aucIpAddr[4];
} IPV4_NETWORK_ADDRESS, *P_IPV4_NETWORK_ADDRESS;

typedef struct _CMD_SET_NETWORK_ADDRESS_LIST {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucAddressCount;
	UINT_8 ucReserved[2];
	IPV4_NETWORK_ADDRESS arNetAddress[1];
} CMD_SET_NETWORK_ADDRESS_LIST, *P_CMD_SET_NETWORK_ADDRESS_LIST;

typedef struct _PATTERN_DESCRIPTION {
	UINT_8 fgCheckBcA1;
	UINT_8 fgCheckMcA1;
	UINT_8 ePatternHeader;
	UINT_8 fgAndOp;
	UINT_8 fgNotOp;
	UINT_8 ucPatternMask;
	UINT_16 ucPatternOffset;
	UINT_8 aucPattern[8];
} PATTERN_DESCRIPTION, *P_PATTERN_DESCRIPTION;

typedef struct _CMD_RAW_PATTERN_CONFIGURATION_T {
	PATTERN_DESCRIPTION arPatternDesc[4];
} CMD_RAW_PATTERN_CONFIGURATION_T, *P_CMD_RAW_PATTERN_CONFIGURATION_T;

typedef struct _CMD_PATTERN_FUNC_CONFIG {
	BOOLEAN fgBcA1En;
	BOOLEAN fgMcA1En;
	BOOLEAN fgBcA1MatchDrop;
	BOOLEAN fgMcA1MatchDrop;
} CMD_PATTERN_FUNC_CONFIG, *P_CMD_PATTERN_FUNC_CONFIG;

typedef struct _EVENT_TX_DONE_T {
	UINT_8 ucPacketSeq;
	UINT_8 ucStatus;
	UINT_16 u2SequenceNumber;
	UINT_32 au4Reserved1;
	UINT_32 au4Reserved2;
	UINT_32 au4Reserved3;
} EVENT_TX_DONE_T, *P_EVENT_TX_DONE_T;

typedef struct _CMD_BSS_ACTIVATE_CTRL {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucActive;
	/* version=1 means Has new format */
	UINT_8 ucVersion;
	UINT_8 ucReserved;
	UINT_8 aucBssMacAddr[MAC_ADDR_LEN];
	UINT_32 au4Reserved[8];
} CMD_BSS_ACTIVATE_CTRL, *P_CMD_BSS_ACTIVATE_CTRL;

typedef struct _CMD_SET_BSS_RLM_PARAM_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucRfBand;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucErpProtectMode;
	UINT_8 ucHtProtectMode;
	UINT_8 ucGfOperationMode;
	UINT_8 ucTxRifsMode;
	UINT_16 u2HtOpInfo3;
	UINT_16 u2HtOpInfo2;
	UINT_8 ucHtOpInfo1;
	UINT_8 ucUseShortPreamble;
	UINT_8 ucUseShortSlotTime;
	UINT_8 ucCheckId;	/* Fixed value: 0x72 */
} CMD_SET_BSS_RLM_PARAM_T, *P_CMD_SET_BSS_RLM_PARAM_T;

typedef struct _CMD_SET_BSS_INFO {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucConnectionState;
	UINT_8 ucCurrentOPMode;
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[32];
	UINT_8 aucBSSID[6];
	UINT_8 ucIsQBSS;
	UINT_8 ucReserved1;
	UINT_16 u2OperationalRateSet;
	UINT_16 u2BSSBasicRateSet;
	UINT_8 ucStaRecIdxOfAP;
	UINT_8 ucReserved2;
	UINT_8 ucReserved3;
	UINT_8 ucNonHTBasicPhyType;	/* For Slot Time and CWmin */
	UINT_8 ucAuthMode;
	UINT_8 ucEncStatus;
	UINT_8 ucPhyTypeSet;
	UINT_8 aucOwnMac[6];
	UINT_8 fgWapiMode;
	UINT_8 fgIsApMode;
	UINT_8 fgHiddenSsidMode;
	CMD_SET_BSS_RLM_PARAM_T rBssRlmParam;
} CMD_SET_BSS_INFO, *P_CMD_SET_BSS_INFO;

typedef struct _CMD_UPDATE_STA_RECORD_T {
	UINT_8 ucIndex;
	UINT_8 ucStaType;
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	UINT_16 u2AssocId;
	UINT_16 u2ListenInterval;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucDesiredPhyTypeSet;
	UINT_16 u2DesiredNonHTRateSet;
	UINT_16 u2BSSBasicRateSet;
	UINT_8 ucIsQoS;
	UINT_8 ucIsUapsdSupported;
	UINT_8 ucStaState;
	UINT_8 ucMcsSet;
	UINT_8 ucSupMcs32;
	UINT_8 ucAmpduParam;
	UINT_16 u2HtCapInfo;
	UINT_16 u2HtExtendedCap;
	UINT_32 u4TxBeamformingCap;
	UINT_8 ucAselCap;
	UINT_8 ucRCPI;
	UINT_8 ucNeedResp;
	UINT_8 ucUapsdAc;	/* b0~3: Trigger enabled, b4~7: Delivery enabled */
	UINT_8 ucUapsdSp;	/* 0: all, 1: max 2, 2: max 4, 3: max 6 */
	UINT_8 ucKeepAliveDuration; /* unit is 1s */
	UINT_8 ucKeepAliveOption; /* only bit0 is used now */
	UINT_8 ucApplyPmf;
	/* TBD */
} CMD_UPDATE_STA_RECORD_T, *P_CMD_UPDATE_STA_RECORD_T;

typedef struct _CMD_REMOVE_STA_RECORD_T {
	UINT_8 ucIndex;
	UINT_8 ucReserved;
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
} CMD_REMOVE_STA_RECORD_T, *P_CMD_REMOVE_STA_RECORD_T;

typedef struct _CMD_INDICATE_PM_BSS_CREATED_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucDtimPeriod;
	UINT_16 u2BeaconInterval;
	UINT_16 u2AtimWindow;
	UINT_8 aucReserved[2];
} CMD_INDICATE_PM_BSS_CREATED, *P_CMD_INDICATE_PM_BSS_CREATED;

typedef struct _CMD_INDICATE_PM_BSS_CONNECTED_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucDtimPeriod;
	UINT_16 u2AssocId;
	UINT_16 u2BeaconInterval;
	UINT_16 u2AtimWindow;
	UINT_8 fgIsUapsdConnection;
	UINT_8 ucBmpDeliveryAC;
	UINT_8 ucBmpTriggerAC;
	UINT_8 aucReserved[1];
} CMD_INDICATE_PM_BSS_CONNECTED, *P_CMD_INDICATE_PM_BSS_CONNECTED;

typedef struct _CMD_INDICATE_PM_BSS_ABORT {
	UINT_8 ucNetTypeIndex;
	UINT_8 aucReserved[3];
} CMD_INDICATE_PM_BSS_ABORT, *P_CMD_INDICATE_PM_BSS_ABORT;

typedef struct _CMD_BEACON_TEMPLATE_UPDATE {
	UINT_8 ucUpdateMethod;	/* 0: update randomly,
				 * 1: update all,
				 * 2: delete all (1 and 2 will update directly without search)
				 */
	UINT_8 ucNetTypeIndex;
	UINT_8 aucReserved[2];
	UINT_16 u2Capability;
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_BEACON_TEMPLATE_UPDATE, *P_CMD_BEACON_TEMPLATE_UPDATE;

typedef struct _CMD_SET_WMM_PS_TEST_STRUCT_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 bmfgApsdEnAc;	/* b0~3: trigger-en AC0~3. b4~7: delivery-en AC0~3 */
	UINT_8 ucIsEnterPsAtOnce;	/* enter PS immediately without 5 second guard after connected */
	UINT_8 ucIsDisableUcTrigger;	/* not to trigger UC on beacon TIM is matched (under U-APSD) */
} CMD_SET_WMM_PS_TEST_STRUCT_T, *P_CMD_SET_WMM_PS_TEST_STRUCT_T;

/* Definition for CHANNEL_INFO.ucBand:
 * 0:       Reserved
 * 1:       BAND_2G4
 * 2:       BAND_5G
 * Others:  Reserved
 */
typedef struct _CHANNEL_INFO_T {
	UINT_8 ucBand;
	UINT_8 ucChannelNum;
} CHANNEL_INFO_T, *P_CHANNEL_INFO_T;

typedef struct _CMD_SCAN_REQ_EXT_CH_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetworkType;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDLength;
	UINT_8 aucReserved[1];
	UINT_16 u2ChannelMinDwellTime;
	UINT_8 aucSSID[32];
	UINT_16 u2ChannelDwellTime;	/* For P2P */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	CHANNEL_INFO_T arChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_EXT_CH, *P_CMD_SCAN_REQ_EXT_CH;

typedef struct _CMD_SCAN_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetworkType;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDLength;
	UINT_8 ucStructVersion;
	UINT_16 u2ChannelMinDwellTime;
	UINT_8 aucSSID[32];
	UINT_16 u2ChannelDwellTime;	/* For P2P */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	CHANNEL_INFO_T arChannelList[32];
	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ, *P_CMD_SCAN_REQ;

typedef struct _CMD_SCAN_REQ_V2_EXT_CH_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetworkType;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;
	PARAM_SSID_T arSSID[4];
	UINT_16 u2ProbeDelayTime;
	UINT_16 u2ChannelDwellTime;	/* For P2P */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	CHANNEL_INFO_T arChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_V2_EXT_CH, *P_CMD_SCAN_REQ_V2_EXT_CH;

typedef struct _CMD_SCAN_REQ_V2_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetworkType;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;
	PARAM_SSID_T arSSID[4];
	UINT_16 u2ProbeDelayTime;
	UINT_16 u2ChannelDwellTime;	/* For P2P */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	CHANNEL_INFO_T arChannelList[32];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_V2, *P_CMD_SCAN_REQ_V2;

/* MULTI SSID */
typedef struct _CMD_SCAN_REQ_V3_EXT_CH_T {
	UINT_8			ucSeqNum;
	UINT_8			ucNetworkType;
	UINT_8			ucScanType;
	UINT_8			ucSSIDType;
	PARAM_SSID_T	arSSID[11];
	UINT_16			u2ProbeDelayTime;
	UINT_16			u2ChannelDwellTime;	/* For P2P */
	UINT_8			ucChannelType;
	UINT_8			ucChannelListNum;
	CHANNEL_INFO_T	arChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16			u2IELen;
	UINT_8			aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_V3_EXT_CH, *P_CMD_SCAN_REQ_V3_EXT_CH;

typedef struct _CMD_SCAN_REQ_V3_T {
	UINT_8          ucSeqNum;
	UINT_8          ucNetworkType;
	UINT_8          ucScanType;
	UINT_8          ucSSIDType;
	PARAM_SSID_T    arSSID[11];
	UINT_16         u2ProbeDelayTime;
	UINT_16         u2ChannelDwellTime; /* For P2P */
	UINT_8          ucChannelType;
	UINT_8          ucChannelListNum;
	CHANNEL_INFO_T  arChannelList[32];
	UINT_16         u2IELen;
	UINT_8          aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_V3, *P_CMD_SCAN_REQ_V3;

/* MULTI SSID */
struct CMD_SCAN_REQ_V4_EXT_CH {
	UINT_8			ucSeqNum;
	UINT_8			ucNetworkType;
	UINT_8			ucScanType;
	UINT_8			ucSSIDType;
	PARAM_SSID_T	arSSID[11];
	UINT_16			u2ProbeDelayTime;
	UINT_16			u2ChannelDwellTime;	/* For P2P */
	UINT_8			ucChannelType;
	UINT_8			ucChannelListNum;
	CHANNEL_INFO_T	arChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16			u2IELen;
	UINT_8			aucIE[MAX_IE_LENGTH];
	UINT_8		ucScnFuncMask;
	UINT_8		aucRandomMac[MAC_ADDR_LEN];
};

struct CMD_SCAN_REQ_V4 {
	UINT_8          ucSeqNum;
	UINT_8          ucNetworkType;
	UINT_8          ucScanType;
	UINT_8          ucSSIDType;
	PARAM_SSID_T    arSSID[11];
	UINT_16         u2ProbeDelayTime;
	UINT_16         u2ChannelDwellTime; /* For P2P */
	UINT_8          ucChannelType;
	UINT_8          ucChannelListNum;
	CHANNEL_INFO_T  arChannelList[32];
	UINT_16         u2IELen;
	UINT_8          aucIE[MAX_IE_LENGTH];
	UINT_8		ucScnFuncMask;
	UINT_8		aucRandomMac[MAC_ADDR_LEN];
};

typedef struct _CMD_SCAN_CANCEL_T {
	UINT_8 ucSeqNum;
	UINT_8 ucIsExtChannel;	/* For P2P channel extension. */
	UINT_8 aucReserved[2];
} CMD_SCAN_CANCEL, *P_CMD_SCAN_CANCEL;

typedef struct _EVENT_SCAN_DONE_T {
	UINT_8 ucSeqNum;
	UINT_8 ucSparseChannelValid;
	CHANNEL_INFO_T rSparseChannel;
} EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;

#if CFG_SUPPORT_GET_CH_ENV
typedef struct _CH_ENV_T {
	UINT_8 ucChNum;
	UINT_8 ucApNum;
} CH_ENV_T, *P_CH_ENV_T;
#endif

#if 0				/* CFG_SUPPORT_BATCH_SCAN */
typedef struct _CMD_BATCH_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucCmd;		/* Start/ Stop */
	UINT_8 ucMScan;		/* an integer number of scans per batch */
	UINT_8 ucBestn;		/* an integer number of the max AP to remember per scan */
	UINT_8 ucRtt;		/*
				 * an integer number of highest-strength AP for which we'd
				 * like approximate distance reported
				 */
	UINT_8 ucChannel;	/* channels */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	UINT_8 aucReserved[3];
	UINT_32 u4Scanfreq;	/* an integer number of seconds between scans */
	CHANNEL_INFO_T arChannelList[32];	/* channels */
} CMD_BATCH_REQ_T, *P_CMD_BATCH_REQ_T;

typedef struct _EVENT_BATCH_RESULT_ENTRY_T {
	UINT_8 aucBssid[MAC_ADDR_LEN];
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];
	UINT_8 ucSSIDLen;
	INT_8 cRssi;
	UINT_32 ucFreq;
	UINT_32 u4Age;
	UINT_32 u4Dist;
	UINT_32 u4Distsd;
} EVENT_BATCH_RESULT_ENTRY_T, *P_EVENT_BATCH_RESULT_ENTRY_T;

typedef struct _EVENT_BATCH_RESULT_T {
	UINT_8 ucScanCount;
	UINT_8 aucReserved[3];
	EVENT_BATCH_RESULT_ENTRY_T arBatchResult[12];	/* Must be the same with SCN_BATCH_STORE_MAX_NUM */
} EVENT_BATCH_RESULT_T, *P_EVENT_BATCH_RESULT_T;
#endif

typedef struct _CMD_CH_PRIVILEGE_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucTokenID;
	UINT_8 ucAction;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucRfBand;
	UINT_8 ucReqType;
	UINT_8 ucReserved;
	UINT_32 u4MaxInterval;	/* In unit of ms */
	UINT_8 aucBSSID[6];
	UINT_8 aucReserved[2];
} CMD_CH_PRIVILEGE_T, *P_CMD_CH_PRIVILEGE_T;

typedef struct _CMD_TX_PWR_T {
	INT_8 cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
#if defined(MT6620)
	INT_8 acReserved[3];
#elif defined(MT6628)
	INT_8 cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	INT_8 acReserved[2];
#else
#error "No valid definition!"
#endif

	INT_8 cTxPwr2G4OFDM_BPSK;
	INT_8 cTxPwr2G4OFDM_QPSK;
	INT_8 cTxPwr2G4OFDM_16QAM;
	INT_8 cTxPwr2G4OFDM_Reserved;
	INT_8 cTxPwr2G4OFDM_48Mbps;
	INT_8 cTxPwr2G4OFDM_54Mbps;

	INT_8 cTxPwr2G4HT20_BPSK;
	INT_8 cTxPwr2G4HT20_QPSK;
	INT_8 cTxPwr2G4HT20_16QAM;
	INT_8 cTxPwr2G4HT20_MCS5;
	INT_8 cTxPwr2G4HT20_MCS6;
	INT_8 cTxPwr2G4HT20_MCS7;

	INT_8 cTxPwr2G4HT40_BPSK;
	INT_8 cTxPwr2G4HT40_QPSK;
	INT_8 cTxPwr2G4HT40_16QAM;
	INT_8 cTxPwr2G4HT40_MCS5;
	INT_8 cTxPwr2G4HT40_MCS6;
	INT_8 cTxPwr2G4HT40_MCS7;

	INT_8 cTxPwr5GOFDM_BPSK;
	INT_8 cTxPwr5GOFDM_QPSK;
	INT_8 cTxPwr5GOFDM_16QAM;
	INT_8 cTxPwr5GOFDM_Reserved;
	INT_8 cTxPwr5GOFDM_48Mbps;
	INT_8 cTxPwr5GOFDM_54Mbps;

	INT_8 cTxPwr5GHT20_BPSK;
	INT_8 cTxPwr5GHT20_QPSK;
	INT_8 cTxPwr5GHT20_16QAM;
	INT_8 cTxPwr5GHT20_MCS5;
	INT_8 cTxPwr5GHT20_MCS6;
	INT_8 cTxPwr5GHT20_MCS7;

	INT_8 cTxPwr5GHT40_BPSK;
	INT_8 cTxPwr5GHT40_QPSK;
	INT_8 cTxPwr5GHT40_16QAM;
	INT_8 cTxPwr5GHT40_MCS5;
	INT_8 cTxPwr5GHT40_MCS6;
	INT_8 cTxPwr5GHT40_MCS7;
} CMD_TX_PWR_T, *P_CMD_TX_PWR_T;

typedef struct _CMD_5G_PWR_OFFSET_T {
	INT_8 cOffsetBand0;	/* 4.915-4.980G */
	INT_8 cOffsetBand1;	/* 5.000-5.080G */
	INT_8 cOffsetBand2;	/* 5.160-5.180G */
	INT_8 cOffsetBand3;	/* 5.200-5.280G */
	INT_8 cOffsetBand4;	/* 5.300-5.340G */
	INT_8 cOffsetBand5;	/* 5.500-5.580G */
	INT_8 cOffsetBand6;	/* 5.600-5.680G */
	INT_8 cOffsetBand7;	/* 5.700-5.825G */
} CMD_5G_PWR_OFFSET_T, *P_CMD_5G_PWR_OFFSET_T;

#if CFG_SUPPORT_TX_POWER_BACK_OFF
typedef struct _CMD_MITIGATED_PWR_OFFSET_T {
	MITIGATED_PWR_BY_CH_BY_MODE arRlmMitigatedPwrByChByMode[40];
} CMD_MITIGATED_PWR_OFFSET_T, *P_CMD_MITIGATED_PWR_OFFSET_T;
#endif

typedef struct _CMD_PWR_PARAM_T {
	UINT_32 au4Data[28];
	UINT_32 u4RefValue1;
	UINT_32 u4RefValue2;
} CMD_PWR_PARAM_T, *P_CMD_PWR_PARAM_T;

typedef struct _CMD_PHY_PARAM_T {
	UINT_8 aucData[144];	/* eFuse content */
} CMD_PHY_PARAM_T, *P_CMD_PHY_PARAM_T;

typedef struct _CMD_AUTO_POWER_PARAM_T {
	UINT_8 ucType;		/* 0: Disable 1: Enalbe 0x10: Change parameters */
	UINT_8 ucNetTypeIndex;
	UINT_8 aucReserved[2];
	UINT_8 aucLevelRcpiTh[3];
	UINT_8 aucReserved2[1];
	INT_8 aicLevelPowerOffset[3];	/* signed, in unit of 0.5dBm */
	UINT_8 aucReserved3[1];
	UINT_8 aucReserved4[8];
} CMD_AUTO_POWER_PARAM_T, *P_CMD_AUTO_POWER_PARAM_T;

/*for WMMAC, CMD_ID_UPDATE_AC_PARAMS*/
typedef struct _CMD_UPDATE_AC_PARAMS_T {
	UINT_8  ucAcIndex; /*0 ~3, from AC0 to AC3*/
	UINT_8  ucNetTypeIndex;  /*no use*/
	UINT_16 u2MediumTime; /*if 0, disable ACM for ACx specified by ucAcIndex,
							* otherwise in unit of 32us
							*/
	UINT_32 u4PhyRate; /* rate to be used to tx packet with priority ucAcIndex , unit: bps */
	UINT_16 u2EDCALifeTime; /* msdu life time for this TC, unit: 2TU */
	UINT_8 ucRetryCount; /* if we use fix rate to tx packets, should tell firmware the limited retries */
	UINT_8 aucReserved[5];
} CMD_UPDATE_AC_PARAMS_T, *P_CMD_UPDATE_AC_PARAMS_T;
/* S56 Traffic Stream Metrics */
typedef struct _CMD_SET_TSM_STATISTICS_REQUEST_T {
	UINT_8 ucEnabled; /* 0, disable; 1, enable; */
	UINT_8 ucNetTypeIndex; /* always NETWORK_TYPE_AIS_INDEX now */
	UINT_8 ucAcIndex; /* wmm ac index, the statistics should be on this TC */
	UINT_8 ucTid;
	UINT_8 aucPeerAddr[MAC_ADDR_LEN]; /* packet to the target address to be mesured */
	UINT_8 ucBin0Range;
	UINT_8 aucReserved[3];

	 /* if this variable is 0, followed variables are meaningless
	 *   only report once for a same trigger condition in this time frame
	 */
	UINT_8 ucTriggerCondition; /* for triggered mode: bit(0):average, bit(1):consecutive, bit(2):delay */
	UINT_8 ucAvgErrThreshold;
	UINT_8 ucConsecutiveErrThreshold;
	UINT_8 ucDelayThreshold;
	UINT_8 ucMeasureCount;
	UINT_8 ucTriggerTimeout; /* unit: 100 TU*/
} CMD_SET_TSM_STATISTICS_REQUEST_T, *P_CMD_SET_TSM_STATISTICS_REQUEST_T;

typedef struct _CMD_GET_TSM_STATISTICS_T {
	UINT_8 ucNetTypeIndex; /* always NETWORK_TYPE_AIS_INDEX now */
	UINT_8 ucAcIndex;	/* wmm ac index, the statistics should be on this TC or TS */
	UINT_8 ucTid; /* */
	UINT_8 aucPeerAddr[MAC_ADDR_LEN];  /* indicating the RA for the measured frames */
	UINT_8 ucReportReason; /* for triggered mode: bit(0):average, bit(1):consecutive, bit(2):delay */
	UINT_16 u2Reserved;

	UINT_32 u4PktTxDoneOK;
	UINT_32 u4PktDiscard; /* u2PktTotal - u2PktTxDoneOK */
	UINT_32 u4PktFail; /* failed count for exceeding retry limit */
	UINT_32 u4PktRetryTxDoneOK;
	UINT_32 u4PktQosCfPollLost;

	/* 802.11k - Average Packet Transmission delay for all packets per this TC or TS */
	UINT_32 u4AvgPktTxDelay;
	/* 802.11k - Average Packet Queue Delay */
	UINT_32 u4AvgPktQueueDelay;
	UINT_64 u8StartTime; /* represented by TSF */
	/* sum of packets whose packet tx delay is less than Bi (i=0~6) range value(unit: TU) */
	UINT_32 au4PktCntBin[6];
} CMD_GET_TSM_STATISTICS_T, *P_CMD_GET_TSM_STATISTICS_T;

typedef struct _CMD_MAX_TXPWR_LIMIT_T {
	UINT_8 ucMaxTxPwrLimitEnable;
	UINT_8 ucMaxTxPwr;
	UINT_8 ucReserved[2];
} CMD_MAX_TXPWR_LIMIT_T, *P_CMD_MAX_TXPWR_LIMIT_T;

typedef struct _EVENT_CH_PRIVILEGE_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucTokenID;
	UINT_8 ucStatus;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucRfBand;
	UINT_8 ucReqType;
	UINT_8 ucReserved;
	UINT_32 u4GrantInterval;	/* In unit of ms */
} EVENT_CH_PRIVILEGE_T, *P_EVENT_CH_PRIVILEGE_T;

typedef enum _ENUM_BEACON_TIMEOUT_TYPE_T {
	BEACON_TIMEOUT_LOST_BEACON = 0,
	BEACON_TIMEOUT_AGE,
	BEACON_TIMEOUT_CONNECT,
	BEACON_TIMEOUT_BEACON_INTERVAL,
	BEACON_TIMEOUT_ABORT,
	BEACON_TIMEOUT_TX_ERROR,
	BEACON_TIMEOUT_TYPE_NUM
} ENUM_BEACON_TIMEOUT_TYPE_T, *P_ENUM_BEACON_TIMEOUT_TYPE_T;

typedef struct _EVENT_BSS_BEACON_TIMEOUT_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucReason;	/* ENUM_BEACON_TIMEOUT_TYPE_T */
	UINT_8 aucReserved[2];
} EVENT_BSS_BEACON_TIMEOUT_T, *P_EVENT_BSS_BEACON_TIMEOUT_T;

typedef struct _EVENT_STA_AGING_TIMEOUT_T {
	UINT_8 ucStaRecIdx;
	UINT_8 aucReserved[3];
} EVENT_STA_AGING_TIMEOUT_T, *P_EVENT_STA_AGING_TIMEOUT_T;

typedef struct _EVENT_NOA_TIMING_T {
	UINT_8 fgIsInUse;	/* Indicate if this entry is in use or not */
	UINT_8 ucCount;		/* Count */
	UINT_8 aucReserved[2];

	UINT_32 u4Duration;	/* Duration */
	UINT_32 u4Interval;	/* Interval */
	UINT_32 u4StartTime;	/* Start Time */
} EVENT_NOA_TIMING_T, *P_EVENT_NOA_TIMING_T;

typedef struct _EVENT_UPDATE_NOA_PARAMS_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 aucReserved[2];
	UINT_8 fgEnableOppPS;
	UINT_16 u2CTWindow;

	UINT_8 ucNoAIndex;
	UINT_8 ucNoATimingCount;	/* Number of NoA Timing */
	EVENT_NOA_TIMING_T arEventNoaTiming[8 /*P2P_MAXIMUM_NOA_COUNT */];
} EVENT_UPDATE_NOA_PARAMS_T, *P_EVENT_UPDATE_NOA_PARAMS_T;

typedef struct _EVENT_AP_OBSS_STATUS_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucObssErpProtectMode;
	UINT_8 ucObssHtProtectMode;
	UINT_8 ucObssGfOperationMode;
	UINT_8 ucObssRifsOperationMode;
	UINT_8 ucObssBeaconForcedTo20M;
	UINT_8 aucReserved[2];
} EVENT_AP_OBSS_STATUS_T, *P_EVENT_AP_OBSS_STATUS_T;

struct EVENT_ADD_KEY_DONE_INFO {
	UINT_8 ucNetworkType;
	UINT_8 ucReserved;
	UINT_8 aucStaAddr[MAC_ADDR_LEN];
};

typedef struct _CMD_EDGE_TXPWR_LIMIT_T {
	INT_8 cBandEdgeMaxPwrCCK;
	INT_8 cBandEdgeMaxPwrOFDM20;
	INT_8 cBandEdgeMaxPwrOFDM40;
	INT_8 cBandEdgeCert;
} CMD_EDGE_TXPWR_LIMIT_T, *P_CMD_EDGE_TXPWR_LIMIT_T;

typedef struct _CMD_RSSI_COMPENSATE_T {
	UINT_8 uc2GRssiCompensation;
	UINT_8 uc5GRssiCompensation;
	UINT_8 ucRssiCompensationValidbit;
	UINT_8 cReserved;
} CMD_RSSI_COMPENSATE_T, *P_CMD_RSSI_COMPENSATE_T;

typedef struct _CMD_BAND_SUPPORT_T {
	UINT_8 uc5GBandSupport;
	UINT_8 cReserved[3];
} CMD_BAND_SUPPORT_T, *P_CMD_BAND_SUPPORT_T;

typedef struct _CMD_TX_PWR_CE_T {
	INT_8 cTxPwrCckLmt;	/* signed, in unit of 0.5dBm */
	INT_8 cTxPwrOfdmLmt;	/* signed, in unit of 0.5dBm */
	INT_8 cTxPwrHt20Lmt;
	INT_8 cTxPwrHt40Lmt;
} CMD_TX_PWR_CE_T, *P_CMD_TX_PWR_CE_T;

typedef struct _CMD_SET_DEVICE_MODE_T {
	UINT_16 u2ChipID;
	UINT_16 u2Mode;
} CMD_SET_DEVICE_MODE_T, *P_CMD_SET_DEVICE_MODE_T;

#if CFG_SUPPORT_RDD_TEST_MODE
typedef struct _CMD_RDD_CH_T {
	UINT_8 ucRddTestMode;
	UINT_8 ucRddShutCh;
	UINT_8 ucRddStartCh;
	UINT_8 ucRddStopCh;
	UINT_8 ucRddDfs;
	UINT_8 ucReserved;
	UINT_8 ucReserved1;
	UINT_8 ucReserved2;
} CMD_RDD_CH_T, *P_CMD_RDD_CH_T;

typedef struct _EVENT_RDD_STATUS_T {
	UINT_8 ucRddStatus;
	UINT_8 aucReserved[3];
} EVENT_RDD_STATUS_T, *P_EVENT_RDD_STATUS_T;
#endif

typedef struct _EVENT_AIS_BSS_INFO_T {
	ENUM_PARAM_MEDIA_STATE_T eConnectionState;	/* Connected Flag used in AIS_NORMAL_TR */
	ENUM_OP_MODE_T eCurrentOPMode;	/* Current Operation Mode - Infra/IBSS */
	BOOLEAN fgIsNetActive;	/* TRUE if this network has been actived */
	UINT_8 ucReserved[3];
} EVENT_AIS_BSS_INFO_T, *P_EVENT_AIS_BSS_INFO_T;

typedef struct _CMD_SET_TXPWR_CTRL_T {
	INT_8 c2GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c2GHotspotPwrOffset;
	INT_8 c2GP2pPwrOffset;
	INT_8 c2GBowPwrOffset;
	INT_8 c5GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c5GHotspotPwrOffset;
	INT_8 c5GP2pPwrOffset;
	INT_8 c5GBowPwrOffset;
	UINT_8 ucConcurrencePolicy;	/*
					 * TX power policy when concurrence
					 * in the same channel
					 * 0: Highest power has priority
					 * 1: Lowest power has priority
					 */
	INT_8 acReserved1[3];	/* Must be zero */

	/* Power limit by channel for all data rates */
	INT_8 acTxPwrLimit2G[14];	/* Channel 1~14, Unit: 0.5dBm */
	INT_8 acTxPwrLimit5G[4];	/* UNII 1~4 */
	INT_8 acReserved2[2];	/* Must be zero */
} CMD_SET_TXPWR_CTRL_T, *P_CMD_SET_TXPWR_CTRL_T;

#if CFG_SUPPORT_BUILD_DATE_CODE
typedef struct _CMD_GET_BUILD_DATE_CODE {
	UINT_8 aucReserved[4];
} CMD_GET_BUILD_DATE_CODE, *P_CMD_GET_BUILD_DATE_CODE;

typedef struct _EVENT_BUILD_DATE_CODE {
	UINT_8 aucDateCode[16];
} EVENT_BUILD_DATE_CODE, *P_EVENT_BUILD_DATE_CODE;
#endif

typedef struct _CMD_GET_STA_STATISTICS_T {
	UINT_8 ucIndex;
	UINT_8 ucFlags;
	UINT_8 ucReadClear;
	UINT_8 aucReserved0[1];
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	UINT_8 aucReserved1[2];
	UINT_8 aucReserved2[16];
} CMD_GET_STA_STATISTICS_T, *P_CMD_GET_STA_STATISTICS_T;

/* CFG_SUPPORT_WFD */
typedef struct _EVENT_STA_STATISTICS_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved1[3];
	UINT_32 u4Flags;	/* Bit0: valid */

	UINT_8 ucStaRecIdx;
	UINT_8 ucNetworkTypeIndex;
	UINT_8 ucWTEntry;
	UINT_8 aucReserved4[1];

	UINT_8 ucMacAddr[MAC_ADDR_LEN];
	UINT_8 ucPer;		/* base: 128 */
	UINT_8 ucRcpi;

	UINT_32 u4PhyMode;	/* SGI BW */
	UINT_16 u2LinkSpeed;	/* unit is 0.5 Mbits */
	UINT_8 ucLinkQuality;
	UINT_8 ucLinkReserved;

	UINT_32 u4TxCount;
	UINT_32 u4TxFailCount;
	UINT_32 u4TxLifeTimeoutCount;
	UINT_32 u4TxDoneAirTime;

	UINT_8 aucReserved[64];
} EVENT_STA_STATISTICS_T, *P_EVENT_STA_STATISTICS_T;

#if CFG_SUPPORT_HOTSPOT_OPTIMIZATION
typedef struct _CMD_HOTSPOT_OPTIMIZATION_CONFIG {
	UINT_32 fgHotspotOptimizationEn;
	UINT_32 u4Level;
} CMD_HOTSPOT_OPTIMIZATION_CONFIG, *P_HOTSPOT_OPTIMIZATION_CONFIG;
#endif

typedef struct _EVENT_LTE_SAFE_CHN_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved[3];
	UINT_32 u4Flags;	/* Bit0: valid */
	LTE_SAFE_CHN_INFO_T rLteSafeChn;
} EVENT_LTE_SAFE_CHN_T, *P_EVENT_LTE_SAFE_CHN_T;

typedef struct _CMD_ROAMING_INFO_T {
	UINT_32 fgIsFastRoamingApplied;
	UINT_32 Reserved[9];
} CMD_ROAMING_INFO_T;

typedef struct _CMD_WFD_DEBUG_MODE_INFO_T {
	UINT_8 ucDebugMode;
	UINT_16 u2PeriodInteval;
	UINT_8 Reserved;
} CMD_WFD_DEBUG_MODE_INFO_T, *P_CMD_WFD_DEBUG_MODE_INFO_T;

typedef struct _EVENT_FW_LOG_T {
	UINT_8 fileName[64];
	UINT_32 lineNo;
	UINT_32 WifiUpTime;
	UINT_8 log[MAX_FW_LOG_LENGTH];	/* total size is aucBuffer in WIFI_EVENT_T */
} EVENT_FW_LOG_T, *P_EVENT_FW_LOG_T;

typedef enum _ENUM_NLO_CIPHER_ALGORITHM {
	NLO_CIPHER_ALGO_NONE = 0x00,
	NLO_CIPHER_ALGO_WEP40 = 0x01,
	NLO_CIPHER_ALGO_TKIP = 0x02,
	NLO_CIPHER_ALGO_CCMP = 0x04,
	NLO_CIPHER_ALGO_WEP104 = 0x05,
	NLO_CIPHER_ALGO_WPA_USE_GROUP = 0x100,
	NLO_CIPHER_ALGO_RSN_USE_GROUP = 0x100,
	NLO_CIPHER_ALGO_WEP = 0x101,
} ENUM_NLO_CIPHER_ALGORITHM, *P_ENUM_NLO_CIPHER_ALGORITHM;

typedef enum _ENUM_NLO_AUTH_ALGORITHM {
	NLO_AUTH_ALGO_80211_OPEN = 1,
	NLO_AUTH_ALGO_80211_SHARED_KEY = 2,
	NLO_AUTH_ALGO_WPA = 3,
	NLO_AUTH_ALGO_WPA_PSK = 4,
	NLO_AUTH_ALGO_WPA_NONE = 5,
	NLO_AUTH_ALGO_RSNA = 6,
	NLO_AUTH_ALGO_RSNA_PSK = 7,
} ENUM_NLO_AUTH_ALGORITHM, *P_ENUM_NLO_AUTH_ALGORITHM;

typedef struct _NLO_NETWORK {
	UINT_8 ucNumChannelHint[4];
	UINT_8 ucSSIDLength;
	UINT_8 ucCipherAlgo;
	UINT_16 u2AuthAlgo;
	UINT_8 aucSSID[32];
} NLO_NETWORK, *P_NLO_NETWORK;

typedef struct _CMD_NLO_REQ {
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_8 ucNetworkType;
	UINT_8 fgStopAfterIndication;
	UINT_8 ucFastScanIteration;
	UINT_16 u2FastScanPeriod;
	UINT_16 u2SlowScanPeriod;
	UINT_8 ucEntryNum;
	UINT_8 ucReserved;
	UINT_16 u2IELen;
	NLO_NETWORK arNetworkList[16];
	UINT_8 aucIE[0];
	UINT_8 ucScanType;
#if CFG_NLO_MSP
	BOOLEAN fgNLOMspEnable; /*Flag for NLO/PNO MSP enable indicator*/
	UINT_8 ucNLOMspEntryNum; /*indicates the entry num of MSP List */
	UINT_16 au2NLOMspList[10];
#endif
	UINT_8 ucScnFuncMask;
} CMD_NLO_REQ, *P_CMD_NLO_REQ;

typedef struct _CMD_NLO_CANCEL_T {
	UINT_8 ucSeqNum;
	UINT_8 aucReserved[3];
} CMD_NLO_CANCEL, *P_CMD_NLO_CANCEL;


struct CMD_SET_CTIA_MODE {
	UINT_8  ucCmdVersion;
	UINT_8  ucCtiaModeEnable;
	UINT_8  ucReserved[2];
};

typedef struct _EVENT_NLO_DONE_T {
	UINT_8      ucSeqNum;
	UINT_8      ucStatus;
	UINT_8      aucReserved[2];
} EVENT_NLO_DONE_T, *P_EVENT_NLO_DONE_T;

typedef struct _EVENT_GSCAN_CAPABILITY_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved1[3];
	UINT_32 u4MaxScanCacheSize;
	UINT_32 u4MaxScanBuckets;
	UINT_32 u4MaxApCachePerScan;
	UINT_32 u4MaxRssiSampleSize;
	UINT_32 u4MaxScanReportingThreshold;
	UINT_32 u4MaxHotlistAps;
	UINT_32 u4MaxSignificantWifiChangeAps;
	UINT_32 au4Reserved[4];
} EVENT_GSCAN_CAPABILITY_T, *P_EVENT_GSCAN_CAPABILITY_T;

typedef struct _EVENT_GSCAN_SCAN_AVAILABLE_T {
	UINT_16 u2Num;
	UINT_8 aucReserved[2];
} EVENT_GSCAN_SCAN_AVAILABLE_T, *P_EVENT_GSCAN_SCAN_AVAILABLE_T;

typedef struct _EVENT_GSCAN_SCAN_COMPLETE_T {
	UINT_8 ucScanState;
	UINT_8 aucReserved[3];
} EVENT_GSCAN_SCAN_COMPLETE_T, *P_EVENT_GSCAN_SCAN_COMPLETE_T;

typedef struct WIFI_GSCAN_RESULT {
	UINT_64 u8Ts;		/* Time of discovery           */
	UINT_8 arSsid[ELEM_MAX_LEN_SSID + 1];	/* null terminated             */
	UINT_8 arMacAddr[6];	/* BSSID                       */
	UINT_32 u4Channel;	/* channel frequency in MHz    */
	INT_32 i4Rssi;		/* in db                       */
	UINT_64 u8Rtt;		/* in nanoseconds              */
	UINT_64 u8RttSd;	/* standard deviation in rtt   */
	UINT_16 u2BeaconPeriod;	/* units are Kusec             */
	UINT_16 u2Capability;	/* Capability information      */
	UINT_32 u4IeLength;	/* byte length of Information Elements */
	UINT_8 ucIeData[1];	/* IE data to follow           */
} WIFI_GSCAN_RESULT_T, *P_WIFI_GSCAN_RESULT_T;

typedef struct _EVENT_GSCAN_RESULT_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved[3];
	UINT_16 u2ScanId;
	UINT_16 u2ScanFlags;
	UINT_16 u2NumOfResults;
	WIFI_GSCAN_RESULT_T rResult[1];
} EVENT_GSCAN_RESULT_T, *P_EVENT_GSCAN_RESULT_T;

typedef struct _EVENT_GSCAN_FULL_RESULT_T {
	WIFI_GSCAN_RESULT_T rResult;
	UINT_32 u4BucketMask;		/* scan chbucket bitmask */
	UINT_32 u4IeLength;		/* byte length of Information Elements */
	UINT_8  ucIeData[1];		/* IE data to follow */
} EVENT_GSCAN_FULL_RESULT_T, *P_EVENT_GSCAN_FULL_RESULT_T;

typedef struct GSCAN_SWC_NET {
	UINT_16 u2Flags;
	UINT_16 u2Channel;
	UINT_8 arBssid[6];
	INT_8 aicRssi[SCN_PSCAN_SWC_RSSI_WIN_MAX];
} GSCAN_SWC_NET_T, P_GSCAN_SWC_NET_T;

typedef struct _EVENT_GSCAN_SIGNIFICANT_CHANGE_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved[3];
	GSCAN_SWC_NET_T arNet[SCN_PSCAN_SWC_MAX_NUM];
} EVENT_GSCAN_SIGNIFICANT_CHANGE_T, *P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T;

typedef struct _EVENT_GSCAN_GEOFENCE_FOUND_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved[3];
	WIFI_GSCAN_RESULT_T rResult[SCN_PSCAN_HOTLIST_REPORT_MAX_NUM];
} EVENT_GSCAN_GEOFENCE_FOUND_T, *P_EVENT_GSCAN_GEOFENCE_FOUND_T;

#if CFG_SUPPORT_BATCH_SCAN

#if 0				/* !CFG_SUPPORT_GSCN */
typedef struct _CMD_BATCH_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucCmd;		/* Start/ Stop */
	UINT_8 ucMScan;		/* an integer number of scans per batch */
	UINT_8 ucBestn;		/* an integer number of the max AP to remember per scan */
	UINT_8 ucRtt;		/*
				 * an integer number of highest-strength AP for which
				 * we'd like approximate distance reported
				 */
	UINT_8 ucChannel;	/* channels */
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	UINT_8 aucReserved[3];
	UINT_32 u4Scanfreq;	/* an integer number of seconds between scans */
	CHANNEL_INFO_T arChannelList[32];	/* channels */
} CMD_BATCH_REQ_T, *P_CMD_BATCH_REQ_T;

#endif

typedef struct _EVENT_BATCH_RESULT_ENTRY_T {
	UINT_8 aucBssid[MAC_ADDR_LEN];
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];
	UINT_8 ucSSIDLen;
	INT_8 cRssi;
	UINT_32 ucFreq;
	UINT_32 u4Age;
	UINT_32 u4Dist;
	UINT_32 u4Distsd;
} EVENT_BATCH_RESULT_ENTRY_T, *P_EVENT_BATCH_RESULT_ENTRY_T;

typedef struct _EVENT_BATCH_RESULT_T {
	UINT_8 ucScanCount;
	UINT_8 aucReserved[3];
	EVENT_BATCH_RESULT_ENTRY_T arBatchResult[12];	/* Must be the same with SCN_BATCH_STORE_MAX_NUM */
} EVENT_BATCH_RESULT_T, *P_EVENT_BATCH_RESULT_T;
#endif

typedef struct _CMD_RLM_INFO_T {
	UINT_32 u4Version;
	UINT_32 fgIsErrRatioEnhanceApplied;
	UINT_8 ucErrRatio2LimitMinRate;
	/*
	 * 0:1M, 1:2M, 2:5.5M, 3:11M, 6:6M, 7:9M, 8:12M, 9:18M, 10:24M, 11:36M, 12:48M, 13:54M
	 */
	UINT_8 ucMinLegacyRateIdx;
	INT_8 cMinRssiThreshold;
	BOOLEAN fgIsRtsApplied;
	UINT_8 ucRecoverTime;

	UINT_32 u4Reserved[0];
} CMD_RLM_INFO_T;

typedef struct _WIFI_SYSTEM_SUSPEND_CMD_T {
	BOOLEAN fgIsSystemSuspend;
	UINT_8 reserved[3];
} WIFI_SYSTEM_SUSPEND_CMD_T, *P_WIFI_SYSTEM_SUSPEND_CMD_T;

typedef struct _CMD_ID_SET_ROAMING_SKIP_T {
	BOOLEAN IsRoamingSkipOneAp;
} CMD_ID_SET_ROAMING_SKIP_T, *P_CMD_ID_SET_ROAMING_SKIP_T;

struct CMD_TDLS_PS_T {
	UINT_8	ucIsEnablePs; /* 0: disable tdls power save; 1: enable tdls power save */
	UINT_8	aucReserved[3];
};


struct CMD_REQ_CHNL_UTILIZATION {
	UINT_16 u2MeasureDuration;
	UINT_8 ucChannelNum;
	UINT_8 aucChannelList[48];
	UINT_8 aucReserved[13];
};

#if CFG_SUPPORT_EMI_DEBUG
typedef struct _CMD_DRIVER_DUMP_EMI_LOG_T {
	BOOLEAN fgIsDriverDumpEmiLogEnable; /* TRUE: notify to FW Driver supoort*/
} CMD_DRIVER_DUMP_EMI_LOG_T, *P_CMD_DRIVER_DUMP_EMI_LOG_T;

typedef struct _EVENT_DRIVER_DUMP_EMI_LOG_T {
	UINT_32 u4RequestDriverDumpAddr; /*EMI dump end page num */
} EVENT_DRIVER_DUMP_EMI_LOG_T, *P_EVENT_DRIVER_DUMP_EMI_LOG_T;
#endif

struct EVENT_RSP_CHNL_UTILIZATION {
	UINT_8 ucChannelNum;
	UINT_8 aucChannelMeasureList[48];
	UINT_8 aucReserved0[15];
	UINT_8 aucChannelUtilization[48];
	UINT_8 aucReserved1[16];
	UINT_8 aucChannelBusyTime[48];
	UINT_8 aucReserved2[16];
};

#if CFG_SUPPORT_P2P_ECSA
typedef struct _CMD_SET_ECSA_PARAM_T {
	UINT_8  ucNetTypeIndex;
	UINT_8  ucSwitchMode;
	UINT_8  ucOperatingClass;
	UINT_8  ucSwitchTotalCount; /* unit:tbtt, min value: 1 sec */
	UINT_8  ucPrimaryChannel;
	UINT_8  ucRfSco;
	UINT_8  ucReserved[2];
} CMD_SET_ECSA_PARAM, *P_CMD_SET_ECSA_PARAM;
typedef struct _EVENT_ECSA_RESULT_T {
	UINT_8 ucNetTypeIndex;
	UINT_8 ucStatus;	/*
				 * 0: ECSA success
				 * 1: update beacon success
				 * 2: Fail due to wrong parameter
				 * 3: Set channel fail
				 */
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucReserved[4];
} EVENT_ECSA_RESULT, *P_EVENT_ECSA_RESULT;
#endif

#if CFG_SUPPORT_GAMING_MODE
struct GAMING_MODE_SETTING {
	UINT_8 fgEnable;
	UINT_8 aucReserved[7];
};

struct CMD_GAMING_MODE_HEADER {
	UINT_8 ucVersion;
	UINT_8 ucType;
	UINT_8 ucMagicCode;
	UINT_8 ucBufferLen;
	struct GAMING_MODE_SETTING rSetting;
};
#endif /* CFG_SUPPORT_GAMING_MODE */

struct CMD_EVENT_LOG_LEVEL {
	UINT_32 u4Version;
	UINT_32 u4LogLevel;
	UINT_8 aucReserved[3];
};

enum ENUM_SCN_FUNC_MASK {
	ENUM_SCN_RANDOM_MAC_EN = (1 << 0),
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
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID nicCmdEventQueryMcrRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQuerySwCtrlRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRfTestATInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetDisassociate(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetIpAddress(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryLinkQuality(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryLinkSpeed(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventEnterRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventLeaveRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryAddress(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryMcastAddr(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryEepromRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetMediaStreamMode(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetStopSchedScan(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

/* Statistics responder */
VOID nicCmdEventQueryXmitOk(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRecvOk(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryXmitError(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRecvError(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRecvNoBuffer(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRecvCrcError(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRecvErrorAlignment(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryXmitOneCollision(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryXmitMoreCollisions(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryXmitMaxCollisions(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

/* for timeout check */
VOID nicOidCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

VOID nicCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

VOID nicOidCmdEnterRFTestTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

#if CFG_SUPPORT_BUILD_DATE_CODE
VOID nicCmdEventBuildDateCode(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

VOID nicCmdEventQueryStaStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryLteSafeChn(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

#if CFG_SUPPORT_BATCH_SCAN
VOID nicCmdEventBatchScanResult(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

VOID nicCmdEventGetBSSInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

#ifdef FW_CFG_SUPPORT
VOID nicCmdEventQueryCfgRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _NIC_CMD_EVENT_H */
