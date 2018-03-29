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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic_cmd_event.h#1
*/

/*! \file   "nic_cmd_event.h"
    \brief This file contains the declairation file of the WLAN OID processing routines
	   of Windows driver for MediaTek Inc. 802.11 Wireless LAN Adapters.
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
#include "gl_vendor.h"
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define CMD_PQ_ID           (0x8000)
#define CMD_PACKET_TYPE_ID  (0xA0)

#define CMD_STATUS_SUCCESS      0
#define CMD_STATUS_REJECTED     1
#define CMD_STATUS_UNKNOWN      2

#define EVENT_HDR_SIZE          OFFSET_OF(WIFI_EVENT_T, aucBuffer[0])

#define MAX_IE_LENGTH       (600)
#define MAX_WSC_IE_LENGTH   (400)

/* Action field in structure CMD_CH_PRIVILEGE_T */
#define CMD_CH_ACTION_REQ           0
#define CMD_CH_ACTION_ABORT         1

/* Status field in structure EVENT_CH_PRIVILEGE_T */
#define EVENT_CH_STATUS_GRANT       0

/*CMD_POWER_OFFSET_T , follow 5G sub-band*/
/* #define MAX_SUBBAND_NUM             8 */
/*  */
/*  */
/*  */
/*  */

#define SCN_PSCAN_SWC_RSSI_WIN_MAX  75
#define SCN_PSCAN_SWC_MAX_NUM       8
#define SCN_PSCAN_HOTLIST_REPORT_MAX_NUM 8

#define BEACON_TIMEOUT_DUE_2_NO_TX_DONE_EVENT 0x8

typedef enum _ENUM_CMD_ID_T {
	CMD_ID_TEST_CTRL = 0x01,	/* 0x01 (Set) */
	CMD_ID_BASIC_CONFIG,	/* 0x02 (Set) */
	CMD_ID_SCAN_REQ_V2,	/* 0x03 (Set) */
	CMD_ID_NIC_POWER_CTRL,	/* 0x04 (Set) */
	CMD_ID_POWER_SAVE_MODE,	/* 0x05 (Set) */
	CMD_ID_LINK_ATTRIB,	/* 0x06 (Set) */
	CMD_ID_ADD_REMOVE_KEY,	/* 0x07 (Set) */
	CMD_ID_DEFAULT_KEY_ID,	/* 0x08 (Set) */
	CMD_ID_INFRASTRUCTURE,	/* 0x09 (Set) */
	CMD_ID_SET_RX_FILTER,	/* 0x0a (Set) */
	CMD_ID_DOWNLOAD_BUF,	/* 0x0b (Set) */
	CMD_ID_WIFI_START,	/* 0x0c (Set) */
	CMD_ID_CMD_BT_OVER_WIFI,	/* 0x0d (Set) */
	CMD_ID_SET_MEDIA_CHANGE_DELAY_TIME,	/* 0x0e (Set) */
	CMD_ID_SET_DOMAIN_INFO,	/* 0x0f (Set) */
	CMD_ID_SET_IP_ADDRESS,	/* 0x10 (Set) */
	CMD_ID_BSS_ACTIVATE_CTRL,	/* 0x11 (Set) */
	CMD_ID_SET_BSS_INFO,	/* 0x12 (Set) */
	CMD_ID_UPDATE_STA_RECORD,	/* 0x13 (Set) */
	CMD_ID_REMOVE_STA_RECORD,	/* 0x14 (Set) */
	CMD_ID_INDICATE_PM_BSS_CREATED,	/* 0x15 (Set) */
	CMD_ID_INDICATE_PM_BSS_CONNECTED,	/* 0x16 (Set) */
	CMD_ID_INDICATE_PM_BSS_ABORT,	/* 0x17 (Set) */
	CMD_ID_UPDATE_BEACON_CONTENT,	/* 0x18 (Set) */
	CMD_ID_SET_BSS_RLM_PARAM,	/* 0x19 (Set) */
	CMD_ID_SCAN_REQ,	/* 0x1a (Set) */
	CMD_ID_SCAN_CANCEL,	/* 0x1b (Set) */
	CMD_ID_CH_PRIVILEGE,	/* 0x1c (Set) */
	CMD_ID_UPDATE_WMM_PARMS,	/* 0x1d (Set) */
	CMD_ID_SET_WMM_PS_TEST_PARMS,	/* 0x1e (Set) */
	CMD_ID_TX_AMPDU,	/* 0x1f (Set) */
	CMD_ID_ADDBA_REJECT,	/* 0x20 (Set) */
	CMD_ID_SET_PS_PROFILE_ADV,	/* 0x21 (Set) */
	CMD_ID_SET_RAW_PATTERN,	/* 0x22 (Set) */
	CMD_ID_CONFIG_PATTERN_FUNC,	/* 0x23 (Set) */
	CMD_ID_SET_TX_PWR,	/* 0x24 (Set) */
	CMD_ID_SET_PWR_PARAM,	/* 0x25 (Set) */
	CMD_ID_P2P_ABORT,	/* 0x26 (Set) */

	/* SLT commands */
	CMD_ID_RANDOM_RX_RESET_EN = 0x2C,	/* 0x2C (Set ) */
	CMD_ID_RANDOM_RX_RESET_DE = 0x2D,	/* 0x2D (Set ) */
	CMD_ID_SAPP_EN = 0x2E,	/* 0x2E (Set ) */
	CMD_ID_SAPP_DE = 0x2F,	/* 0x2F (Set ) */

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
	CMD_ID_SET_WFD_CTRL,	/* 0x3a (Set) */
	CMD_ID_SET_NLO_REQ,	/* 0x3b (Set) */
	CMD_ID_SET_NLO_CANCEL,	/* 0x3c (Set) */
	CMD_ID_SET_GTK_REKEY_DATA,	/* 0x3d (Set) */
	CMD_ID_ROAMING_CONTROL,	/* 0x3e (Set) */
/*	CFG_M0VE_BA_TO_DRIVER */
	CMD_ID_RESET_BA_SCOREBOARD = 0x3f,	/* 0x3f (Set) */
	CMD_ID_SET_EDGE_TXPWR_LIMIT_5G = 0x40,	/* 0x40 (Set) */
	CMD_ID_SET_CHANNEL_PWR_OFFSET,	/* 0x41 (Set) */
	CMD_ID_SET_80211AC_TX_PWR,	/* 0x42 (Set) */
	CMD_ID_SET_PATH_COMPASATION,	/* 0x43 (Set) */

	CMD_ID_SET_BATCH_REQ = 0x47,	/* 0x47 (Set) */
	CMD_ID_SET_NVRAM_SETTINGS,	/* 0x48 (Set) */
	CMD_ID_SET_COUNTRY_POWER_LIMIT,	/* 0x49 (Set) */

	CMD_ID_SET_SUSPEND_MODE     = 0x58, /* 0x58 (Set) */
	CMD_ID_REQ_CHNL_UTILIZATION = 0x5C, /* 0x5C (Get) */
	/* CFG_SUPPORT_GSCN  */
	CMD_ID_GET_PSCAN_CAPABILITY = 0x60,	/* 0x60 (Set) */
	CMD_ID_SET_PSCAN_ENABLE,	/* 0x61 (Set) */
	CMD_ID_SET_PSCAN_PARAM,	/* 0x62 (Set) */
	CMD_ID_SET_GSCAN_ADD_HOTLIST_BSSID,	/* 0x63 (Set) */
	CMD_ID_SET_GSCAN_ADD_SWC_BSSID,	/* 0x64 (Set) */
	CMD_ID_SET_GSCAN_MAC_ADDR,	/* 0x65 (Set) */
	CMD_ID_GET_GSCAN_RESULT,	/* 0x66 (Get) */
#if CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST
	CMD_ID_SET_FCC_TX_PWR_CERT = 0x6F,	/* 0x6F (Set) */
#endif
#if FW_CFG_SUPPORT
	CMD_ID_GET_SET_CUSTOMER_CFG = 0x70, /* 0x70(Set) */
#endif

	CMD_ID_TDLS_PS = 0x75,		/* 0x75 (Set) */
	CMD_ID_GET_NIC_CAPABILITY = 0x80,	/* 0x80 (Query) */
	CMD_ID_GET_LINK_QUALITY,	/* 0x81 (Query) */
	CMD_ID_GET_STATISTICS,	/* 0x82 (Query) */
	CMD_ID_GET_CONNECTION_STATUS,	/* 0x83 (Query) */
	CMD_ID_GET_STA_STATISTICS = 0x85,	/* 0x85 (Query) */

	CMD_ID_GET_LTE_CHN = 0x87,	/* 0x87 (Query) */
	CMD_ID_GET_CHN_LOADING = 0x88,	/* 0x88 (Query) */
	CMD_ID_GET_BUG_REPORT = 0x89,	/* 0x89 (Query) */

	CMD_ID_WFC_KEEP_ALIVE = 0xA0,	/* 0xa0(Set) */
	CMD_ID_RSSI_MONITOR = 0xA1,	/* 0xa1(Set) */
	CMD_ID_ACCESS_REG = 0xC0,	/* 0xc0 (Set / Query) */
	CMD_ID_MAC_MCAST_ADDR,	/* 0xc1 (Set / Query) */
	CMD_ID_802_11_PMKID,	/* 0xc2 (Set / Query) */
	CMD_ID_ACCESS_EEPROM,	/* 0xc3 (Set / Query) */
	CMD_ID_SW_DBG_CTRL,	/* 0xc4 (Set / Query) */
	CMD_ID_SEC_CHECK,	/* 0xc5 (Set / Query) */
	CMD_ID_DUMP_MEM,	/* 0xc6 (Query) */
	CMD_ID_RESOURCE_CONFIG,	/* 0xc7 (Set / Query) */
	CMD_ID_CHIP_CONFIG = 0xCA,	/* 0xca (Set / Query) */
	CMD_ID_STATS_LOG = 0xCB,	/* 0xcb (Set) */
	CMD_ID_SET_RDD_CH = 0xE1,
	CMD_ID_SET_BWCS = 0xF1,
	CMD_ID_SET_OSC = 0xF2,

	CMD_ID_GET_BUILD_DATE_CODE = 0xF8,	/* 0xf8 (Query) */
	CMD_ID_GET_BSS_INFO = 0xF9,	/* 0xF9 (Query) */
	CMD_ID_SET_HOTSPOT_OPTIMIZATION = 0xFA,	/* 0xFA (Set) */
	CMD_ID_SET_TDLS_CH_SW = 0xFB,
	CMD_ID_SET_MONITOR = 0xFC,	/* 0xFC (Set) */
	CMD_ID_END
} ENUM_CMD_ID_T, *P_ENUM_CMD_ID_T;

typedef enum _ENUM_EVENT_ID_T {
	EVENT_ID_NIC_CAPABILITY = 0x01,	/* 0x01 (Query) */
	EVENT_ID_LINK_QUALITY,	/* 0x02 (Query / Unsolicited) */
	EVENT_ID_STATISTICS,	/* 0x03 (Query) */
	EVENT_ID_MIC_ERR_INFO,	/* 0x04 (Unsolicited) */
	EVENT_ID_ACCESS_REG,	/* 0x05 (Query - CMD_ID_ACCESS_REG) */
	EVENT_ID_ACCESS_EEPROM,	/* 0x06 (Query - CMD_ID_ACCESS_EEPROM) */
	EVENT_ID_SLEEPY_INFO,	/* 0x07 (Unsolicited) */
	EVENT_ID_BT_OVER_WIFI,	/* 0x08 (Unsolicited) */
	EVENT_ID_TEST_STATUS,	/* 0x09 (Query - CMD_ID_TEST_CTRL) */
	EVENT_ID_RX_ADDBA,	/* 0x0a (Unsolicited) */
	EVENT_ID_RX_DELBA,	/* 0x0b (Unsolicited) */
	EVENT_ID_ACTIVATE_STA_REC,	/* 0x0c (Response) */
	EVENT_ID_SCAN_DONE,	/* 0x0d (Unsoiicited) */
	EVENT_ID_RX_FLUSH,	/* 0x0e (Unsolicited) */
	EVENT_ID_TX_DONE,	/* 0x0f (Unsolicited) */
	EVENT_ID_CH_PRIVILEGE,	/* 0x10 (Unsolicited) */
	EVENT_ID_BSS_ABSENCE_PRESENCE,	/* 0x11 (Unsolicited) */
	EVENT_ID_STA_CHANGE_PS_MODE,	/* 0x12 (Unsolicited) */
	EVENT_ID_BSS_BEACON_TIMEOUT,	/* 0x13 (Unsolicited) */
	EVENT_ID_UPDATE_NOA_PARAMS,	/* 0x14 (Unsolicited) */
	EVENT_ID_AP_OBSS_STATUS,	/* 0x15 (Unsolicited) */
	EVENT_ID_STA_UPDATE_FREE_QUOTA,	/* 0x16 (Unsolicited) */
	EVENT_ID_SW_DBG_CTRL,	/* 0x17 (Query - CMD_ID_SW_DBG_CTRL) */
	EVENT_ID_ROAMING_STATUS,	/* 0x18 (Unsolicited) */
	EVENT_ID_STA_AGING_TIMEOUT,	/* 0x19 (Unsolicited) */
	EVENT_ID_SEC_CHECK_RSP,	/* 0x1a (Query - CMD_ID_SEC_CHECK) */
	EVENT_ID_SEND_DEAUTH,	/* 0x1b (Unsolicited) */
	EVENT_ID_UPDATE_RDD_STATUS,	/* 0x1c (Unsolicited) */
	EVENT_ID_UPDATE_BWCS_STATUS,	/* 0x1d (Unsolicited) */
	EVENT_ID_UPDATE_BCM_DEBUG,	/* 0x1e (Unsolicited) */
	EVENT_ID_RX_ERR,	/* 0x1f (Unsolicited) */
	EVENT_ID_DUMP_MEM = 0x20,	/* 0x20 (Query - CMD_ID_DUMP_MEM) */
	EVENT_ID_STA_STATISTICS,	/* 0x21 (Query ) */
	EVENT_ID_STA_STATISTICS_UPDATE,	/* 0x22 (Unsolicited) */
	EVENT_ID_NLO_DONE,	/* 0x23 (Unsoiicited) */
	EVENT_ID_ADD_PKEY_DONE,	/* 0x24 (Unsoiicited) */
	EVENT_ID_ICAP_DONE,	/* 0x25 (Unsoiicited) */
	EVENT_ID_RESOURCE_CONFIG = 0x26,	/* 0x26 (Query - CMD_ID_RESOURCE_CONFIG) */
	EVENT_ID_DEBUG_MSG = 0x27,	/* 0x27 (Unsoiicited) */
	EVENT_ID_RTT_CALIBR_DONE = 0x28,	/* 0x28 (Unsoiicited) */
	EVENT_ID_RTT_UPDATE_RANGE = 0x29,	/* 0x29 (Unsoiicited) */
	EVENT_ID_CHECK_REORDER_BUBBLE = 0x2a,	/* 0x2a (Unsoiicited) */
	EVENT_ID_BATCH_RESULT = 0x2b,	/* 0x2b (Query) */
	/* CFG_SUPPORT_GSCN  */
	EVENT_ID_GSCAN_CAPABILITY = 0x30,
	EVENT_ID_GSCAN_SCAN_COMPLETE = 0x31,
	EVENT_ID_GSCAN_FULL_RESULT = 0x32,
	EVENT_ID_GSCAN_SIGNIFICANT_CHANGE = 0x33,
	EVENT_ID_GSCAN_GEOFENCE_FOUND = 0x34,
	EVENT_ID_GSCAN_SCAN_AVAILABLE = 0x35,
	EVENT_ID_GSCAN_RESULT = 0x36,

	EVENT_ID_UART_ACK = 0x40,	/* 0x40 (Unsolicited) */
	EVENT_ID_UART_NAK,	/* 0x41 (Unsolicited) */
	EVENT_ID_GET_CHIPID,	/* 0x42 (Query - CMD_ID_GET_CHIPID) */
	EVENT_ID_SLT_STATUS,	/* 0x43 (Query - CMD_ID_SET_SLTINFO) */
	EVENT_ID_CHIP_CONFIG,	/* 0x44 (Query - CMD_ID_CHIP_CONFIG) */

	EVENT_ID_RSP_CHNL_UTILIZATION = 0x59, /* 0x59 (Query - CMD_ID_REQ_CHNL_UTILIZATION) */

	EVENT_ID_TDLS = 0x80,	/* TDLS event_id */
	EVENT_ID_RSSI_MONITOR = 0xA1,

	EVENT_ID_BUILD_DATE_CODE = 0xF8,
	EVENT_ID_GET_AIS_BSS_INFO = 0xF9,
	EVENT_ID_DEBUG_CODE = 0xFB,
	EVENT_ID_RFTEST_READY = 0xFC,	/* 0xFC */
	EVENT_ID_END
} ENUM_EVENT_ID_T, *P_ENUM_EVENT_ID_T;

#define CMD_ID_SET_PSCN_ADD_HOTLIST_BSSID CMD_ID_SET_GSCAN_ADD_HOTLIST_BSSID	/* 0x45 (Set) */
#define CMD_ID_SET_PSCN_ADD_SW_BSSID CMD_ID_SET_GSCAN_ADD_SWC_BSSID	/* 0x46 (Set) */
#define CMD_ID_SET_PSCN_MAC_ADDR     CMD_ID_SET_GSCAN_MAC_ADDR          /* 0x65 (Set) */	/* 0x47 (Set) */
#define CMD_ID_SET_PSCN_ENABLE      CMD_ID_SET_PSCAN_ENABLE

#define NLO_CHANNEL_TYPE_SPECIFIED	0
#define NLO_CHANNEL_TYPE_DUAL_BAND	1
#define NLO_CHANNEL_TYPE_2G4_ONLY	2
#define NLO_CHANNEL_TYPE_5G_ONLY	3

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#ifndef LINUX
typedef UINT_8 CMD_STATUS;
#endif
/* for Event Packet (via HIF-RX) */
    /* following CM's documentation v0.7 */
typedef struct _WIFI_CMD_T {
	UINT_16 u2TxByteCount;	/* Max value is over 2048 */
	UINT_16 u2PQ_ID;	/* Must be 0x8000 (Port1, Queue 0) */
	UINT_8 ucCID;
	UINT_8 ucPktTypeID;	/* Must be 0x20 (CMD Packet) */
	UINT_8 ucSetQuery;
	UINT_8 ucSeqNum;

	UINT_8 aucBuffer[0];
} WIFI_CMD_T, *P_WIFI_CMD_T;

/* for Command Packet (via HIF-TX) */
    /* following CM's documentation v0.7 */
typedef struct _WIFI_EVENT_T {
	UINT_16 u2PacketLength;
	UINT_16 u2PacketType;	/* Must be filled with 0xE000 (EVENT Packet) */
	UINT_8 ucEID;
	UINT_8 ucSeqNum;
	UINT_8 aucReserved[2];

	UINT_8 aucBuffer[0];
} WIFI_EVENT_T, *P_WIFI_EVENT_T;

/* CMD_ID_TEST_CTRL */
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
	UINT_8 ucBssIdx;
	UINT_8 ucAlgorithmId;
	UINT_8 ucKeyId;
	UINT_8 ucKeyLen;
	UINT_8 ucWlanIndex;
	UINT_8 ucReverved;
	UINT_8 aucKeyMaterial[32];
	UINT_8 aucKeyRsc[16];
} CMD_802_11_KEY, *P_CMD_802_11_KEY;

/* CMD_ID_DEFAULT_KEY_ID */
typedef struct _CMD_DEFAULT_KEY {
	UINT_8 ucBssIdx;
	UINT_8 ucKeyId;
	UINT_8 ucUnicast;
	UINT_8 ucMulticast;
} CMD_DEFAULT_KEY, *P_CMD_DEFAULT_KEY;

/* WPA2 PMKID cache structure */
typedef struct _PMKID_ENTRY_T {
	PARAM_BSSID_INFO_T rBssidInfo;
	BOOLEAN fgPmkidExist;
} PMKID_ENTRY_T, *P_PMKID_ENTRY_T;

typedef struct _CMD_802_11_PMKID {
	UINT_32 u4BSSIDInfoCount;
	P_PMKID_ENTRY_T arPMKIDInfo[1];
} CMD_802_11_PMKID, *P_CMD_802_11_PMKID;

typedef struct _CMD_GTK_REKEY_DATA_T {
	UINT_8 aucKek[16];
	UINT_8 aucKck[16];
	UINT_8 aucReplayCtr[8];
} CMD_GTK_REKEY_DATA_T, *P_CMD_GTK_REKEY_DATA_T;

/* CMD_BASIC_CONFIG */
typedef struct _CMD_CSUM_OFFLOAD_T {
	UINT_16 u2RxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
	UINT_16 u2TxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
} CMD_CSUM_OFFLOAD_T, *P_CMD_CSUM_OFFLOAD_T;

typedef struct _CMD_BASIC_CONFIG_T {
	UINT_8 ucNative80211;
	UINT_8 aucReserved[3];

	CMD_CSUM_OFFLOAD_T rCsumOffload;
} CMD_BASIC_CONFIG_T, *P_CMD_BASIC_CONFIG_T;

/* CMD_MAC_MCAST_ADDR */
typedef struct _CMD_MAC_MCAST_ADDR {
	UINT_32 u4NumOfGroupAddr;
	UINT_8 ucBssIndex;
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

#if CFG_M0VE_BA_TO_DRIVER
typedef struct _CMD_RESET_BA_SCOREBOARD_T {
	UINT_8 ucflag;
	UINT_8 ucTID;
	UINT_8 aucMacAddr[PARAM_MAC_ADDR_LEN];
} CMD_RESET_BA_SCOREBOARD_T, *P_CMD_RESET_BA_SCOREBOARD_T;
#endif

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
typedef struct _EVENT_NIC_CAPABILITY_T {
	UINT_16 u2ProductID;
	UINT_16 u2FwVersion;
	UINT_16 u2DriverVersion;
	UINT_8 ucHw5GBandDisabled;
	UINT_8 ucEepromUsed;
	UINT_8 aucMacAddr[6];
	UINT_8 ucEndianOfMacAddrNumber;
	UINT_8 ucReserved;

	UINT_8 ucRfVersion;
	UINT_8 ucPhyVersion;
	UINT_8 ucRfCalFail;
	UINT_8 ucBbCalFail;
	UINT_8 aucDateCode[16];
	UINT_32 u4FeatureFlag0;
	UINT_32 u4FeatureFlag1;
	UINT_32 u4CompileFlag0;
	UINT_32 u4CompileFlag1;
	UINT_8 aucReserved0[64];
} EVENT_NIC_CAPABILITY_T, *P_EVENT_NIC_CAPABILITY_T;

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

/* event of add key done for port control */
typedef struct _EVENT_ADD_KEY_DONE_INFO {
	UINT_8 ucBSSIndex;
	UINT_8 ucReserved;
	UINT_8 aucStaAddr[6];
} EVENT_ADD_KEY_DONE_INFO, *P_EVENT_ADD_KEY_DONE_INFO;

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

typedef struct _CMD_CHIP_CONFIG_T {
	UINT_16 u2Id;
	UINT_8 ucType;
	UINT_8 ucRespType;
	UINT_16 u2MsgSize;
	UINT_8 aucReserved0[2];
	UINT_8 aucCmd[CHIP_CONFIG_RESP_SIZE];
} CMD_CHIP_CONFIG_T, *P_CMD_CHIP_CONFIG_T;

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
	UINT_8 ucBssIndex;
	UINT_8 ucPsProfile;
	UINT_8 aucReserved[2];
} CMD_PS_PROFILE_T, *P_CMD_PS_PROFILE_T;

/* EVENT_LINK_QUALITY */
#if 1
typedef struct _LINK_QUALITY_ {
	INT_8 cRssi;		/* AIS Network. */
	INT_8 cLinkQuality;
	UINT_16 u2LinkSpeed;	/* TX rate1 */
	UINT_8 ucMediumBusyPercentage;	/* Read clear */
	UINT_8 ucIsLQ0Rdy;	/* Link Quality BSS0 Ready. */
} LINK_QUALITY, *P_LINK_QUALITY;

typedef struct _EVENT_LINK_QUALITY_V2 {
	LINK_QUALITY rLq[BSSID_NUM];
} EVENT_LINK_QUALITY_V2, *P_EVENT_LINK_QUALITY_V2;

typedef struct _EVENT_LINK_QUALITY {
	INT_8 cRssi;
	INT_8 cLinkQuality;
	UINT_16 u2LinkSpeed;
	UINT_8 ucMediumBusyPercentage;
} EVENT_LINK_QUALITY, *P_EVENT_LINK_QUALITY;
#endif

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

typedef struct _EVENT_BUG_REPORT_T {
	/* BugReportVersion 1 */
	UINT_32 u4BugReportVersion;

	/* FW Module State 2 */
	UINT_32 u4FWState;

	/* Scan Counter 3-6 */
	UINT_32 u4ReceivedBeaconCount;
	UINT_32 u4ReceivedProbeResponseCount;
	UINT_32 u4SentProbeRequestCount;
	UINT_32 u4SentProbeRequestFailCount;

	/* Roaming Counter 7-9 */
	UINT_32 u4RoamingDebugFlag;
	UINT_32 u4RoamingThreshold;
	UINT_32 u4RoamingCurrentRcpi;

	/* RF Counter 10-14 */
	UINT_32 u4RFPriChannel;
	UINT_32 u4RFChannelS1;
	UINT_32 u4RFChannelS2;
	UINT_32 u4RFChannelWidth;
	UINT_32 u4RFSco;

	/* Coex Counter 15-17 */
	UINT_32 u4BTProfile;
	UINT_32 u4BTOn;
	UINT_32 u4LTEOn;

	/* Low Power Counter 18-20 */
	UINT_32 u4LPTxUcPktNum;
	UINT_32 u4LPRxUcPktNum;
	UINT_32 u4LPPSProfile;

	/* Base Band Counter 21- 32 */
	UINT_32 u4OfdmPdCnt;
	UINT_32 u4CckPdCnt;
	UINT_32 u4CckSigErrorCnt;
	UINT_32 u4CckSfdErrorCnt;
	UINT_32 u4OfdmSigErrorCnt;
	UINT_32 u4OfdmTaqErrorCnt;
	UINT_32 u4OfdmFcsErrorCnt;
	UINT_32 u4CckFcsErrorCnt;
	UINT_32 u4OfdmMdrdyCnt;
	UINT_32 u4CckMdrdyCnt;
	UINT_32 u4PhyCcaStatus;
	UINT_32 u4WifiFastSpiStatus;

	/* Mac RX Counter 33-45 */
	UINT_32 u4RxMdrdyCount;
	UINT_32 u4RxFcsErrorCount;
	UINT_32 u4RxbFifoFullCount;
	UINT_32 u4RxMpduCount;
	UINT_32 u4RxLengthMismatchCount;
	UINT_32 u4RxCcaPrimCount;
	UINT_32 u4RxEdCount;
	UINT_32 u4LmacFreeRunTimer;
	UINT_32 u4WtblReadPointer;
	UINT_32 u4RmacWritePointer;
	UINT_32 u4SecWritePointer;
	UINT_32 u4SecReadPointer;
	UINT_32 u4DmaReadPointer;

	/* Mac TX Counter 46-47 */
	UINT_32 u4TxChannelIdleCount;
	UINT_32 u4TxCcaNavTxTime;
	/* If you want to add item, please modify BUG_REPORT_NUM in mtk_driver_nl80211.h */
} EVENT_BUG_REPORT_T, *P_EVENT_BUG_REPORT_T;

/* EVENT_ID_FW_SLEEPY_NOTIFY */
typedef struct _EVENT_SLEEPY_INFO_T {
	UINT_8 ucSleepyState;
	UINT_8 aucReserved[3];
} EVENT_SLEEPY_INFO_T, *P_EVENT_SLEEPY_INFO_T;

typedef struct _EVENT_ACTIVATE_STA_REC_T {
	UINT_8 aucMacAddr[6];
	UINT_8 ucStaRecIdx;
	UINT_8 ucBssIndex;
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
	UINT_16 u2IsSetPassiveScan;
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
	UINT_8 ucBssIndex;
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
	UINT_16 u2PatternOffset;
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

#if CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST
/* TX Power Adjust For FCC/CE Certification */
typedef struct _CMD_FCC_TX_PWR_ADJUST_T {
	UINT_8 fgFccTxPwrAdjust;
	UINT_8 Offset_CCK;      /* Offset for CH 11~14 */
	UINT_8 Offset_HT20;     /* Offset for CH 11~14 */
	UINT_8 Offset_HT40;     /* Offset for CH 11~14 */
	UINT_8 Channel_CCK[2];  /* [0] for start channel, [1] for ending channel */
	UINT_8 Channel_HT20[2]; /* [0] for start channel, [1] for ending channel */
	UINT_8 Channel_HT40[2]; /* [0] for start channel, [1] for ending channel */
	UINT_8 cReserved[2];
} CMD_FCC_TX_PWR_ADJUST, *P_CMD_FCC_TX_PWR_ADJUST;
#endif

typedef struct _EVENT_TX_DONE_T {
	UINT_8 ucPacketSeq;
	UINT_8 ucStatus;
	UINT_16 u2SequenceNumber;
	UINT_8 ucWlanIndex;
	UINT_8 ucTxCount;
	UINT_16 u2TxRate;
	UINT_8 ucFlag;
	UINT_8 au4Reserved2[3];
	UINT_32 au4Reserved3;
} EVENT_TX_DONE_T, *P_EVENT_TX_DONE_T;

typedef struct _CMD_BSS_ACTIVATE_CTRL {
	UINT_8 ucBssIndex;
	UINT_8 ucActive;
	UINT_8 ucNetworkType;
	UINT_8 ucOwnMacAddrIndex;
	UINT_8 aucBssMacAddr[6];
	UINT_8 ucBMCWlanIndex;
	UINT_8 ucReserved;
} CMD_BSS_ACTIVATE_CTRL, *P_CMD_BSS_ACTIVATE_CTRL;

typedef struct _CMD_SET_BSS_RLM_PARAM_T {
	UINT_8 ucBssIndex;
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
	UINT_8 ucVhtChannelWidth;
	UINT_8 ucVhtChannelFrequencyS1;
	UINT_8 ucVhtChannelFrequencyS2;
	UINT_16 u2VhtBasicMcsSet;
} CMD_SET_BSS_RLM_PARAM_T, *P_CMD_SET_BSS_RLM_PARAM_T;

typedef struct _CMD_SET_BSS_INFO {
	UINT_8 ucBssIndex;
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
	UINT_16 u2HwDefaultFixedRateCode;
	UINT_8 ucNonHTBasicPhyType;	/* For Slot Time and CWmin */
	UINT_8 ucAuthMode;
	UINT_8 ucEncStatus;
	UINT_8 ucPhyTypeSet;
	UINT_8 ucWapiMode;
	UINT_8 ucIsApMode;
	UINT_8 ucBMCWlanIndex;
	UINT_8 ucHiddenSsidMode;
	UINT_8 ucDisconnectDetectTh;
	UINT_32 u4PrivateData;
	CMD_SET_BSS_RLM_PARAM_T rBssRlmParam;
} CMD_SET_BSS_INFO, *P_CMD_SET_BSS_INFO;

typedef enum _ENUM_RTS_POLICY_T {
	RTS_POLICY_AUTO,
	RTS_POLICY_STATIC_BW,
	RTS_POLICY_DYNAMIC_BW,
	RTS_POLICY_LEGACY,
	RTS_POLICY_NO_RTS
} ENUM_RTS_POLICY;

typedef struct _CMD_UPDATE_STA_RECORD_T {
	UINT_8 ucStaIndex;
	UINT_8 ucStaType;
	/* This field should assign at create and keep consistency for update usage */
	UINT_8 aucMacAddr[MAC_ADDR_LEN];

	UINT_16 u2AssocId;
	UINT_16 u2ListenInterval;
	UINT_8 ucBssIndex;	/* This field should assign at create and keep consistency for update usage */
	UINT_8 ucDesiredPhyTypeSet;
	UINT_16 u2DesiredNonHTRateSet;

	UINT_16 u2BSSBasicRateSet;
	UINT_8 ucIsQoS;
	UINT_8 ucIsUapsdSupported;
	UINT_8 ucStaState;
	UINT_8 ucMcsSet;
	UINT_8 ucSupMcs32;
	UINT_8 aucReserved1[1];

	UINT_8 aucRxMcsBitmask[10];
	UINT_16 u2RxHighestSupportedRate;
	UINT_32 u4TxRateInfo;

	UINT_16 u2HtCapInfo;
	UINT_16 u2HtExtendedCap;
	UINT_32 u4TxBeamformingCap;

	UINT_8 ucAmpduParam;
	UINT_8 ucAselCap;
	UINT_8 ucRCPI;
	UINT_8 ucNeedResp;
	UINT_8 ucUapsdAc;	/* b0~3: Trigger enabled, b4~7: Delivery enabled */
	UINT_8 ucUapsdSp;	/* 0: all, 1: max 2, 2: max 4, 3: max 6 */
	UINT_8 ucWlanIndex;	/* This field should assign at create and keep consistency for update usage */
	UINT_8 ucBMCWlanIndex;	/* This field should assign at create and keep consistency for update usage */

	UINT_32 u4VhtCapInfo;
	UINT_16 u2VhtRxMcsMap;
	UINT_16 u2VhtRxHighestSupportedDataRate;
	UINT_16 u2VhtTxMcsMap;
	UINT_16 u2VhtTxHighestSupportedDataRate;
	UINT_8 ucRtsPolicy;	/* 0: auto 1: Static BW 2: Dynamic BW 3: Legacy 7: WoRts */
	UINT_8 aucReserved2[1];

	UINT_8 ucTrafficDataType;	/* 0: auto 1: data 2: video 3: voice */
	UINT_8 ucTxGfMode;
	UINT_8 ucTxSgiMode;
	UINT_8 ucTxStbcMode;
	UINT_16 u2HwDefaultFixedRateCode;
	UINT_8 ucTxAmpdu;
	UINT_8 ucRxAmpdu;
	UINT_32 u4FixedPhyRate;	/* */
	UINT_16 u2MaxLinkSpeed;	/* unit is 0.5 Mbps */
	UINT_16 u2MinLinkSpeed;

	UINT_32 u4Flags;

	UINT_8 ucTxBaSize;
	UINT_8 ucRxBaSize;
	UINT_8 ucKeepAliveDuration; /* unit is 1s */
	UINT_8 ucKeepAliveOption; /* only bit0 is used now */
	UINT_8 aucReserved4[28];
} CMD_UPDATE_STA_RECORD_T, *P_CMD_UPDATE_STA_RECORD_T;

typedef struct _CMD_REMOVE_STA_RECORD_T {
	UINT_8 ucActionType;
	UINT_8 ucStaIndex;
	UINT_8 ucBssIndex;
	UINT_8 ucReserved;
} CMD_REMOVE_STA_RECORD_T, *P_CMD_REMOVE_STA_RECORD_T;

typedef struct _CMD_INDICATE_PM_BSS_CREATED_T {
	UINT_8 ucBssIndex;
	UINT_8 ucDtimPeriod;
	UINT_16 u2BeaconInterval;
	UINT_16 u2AtimWindow;
	UINT_8 aucReserved[2];
} CMD_INDICATE_PM_BSS_CREATED, *P_CMD_INDICATE_PM_BSS_CREATED;

typedef struct _CMD_INDICATE_PM_BSS_CONNECTED_T {
	UINT_8 ucBssIndex;
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
	UINT_8 ucBssIndex;
	UINT_8 aucReserved[3];
} CMD_INDICATE_PM_BSS_ABORT, *P_CMD_INDICATE_PM_BSS_ABORT;

typedef struct _CMD_BEACON_TEMPLATE_UPDATE {
/* 0: update randomly, 1: update all, 2: delete all (1 and 2 will update directly without search) */
	UINT_8 ucUpdateMethod;
	UINT_8 ucBssIndex;
	UINT_8 aucReserved[2];
	UINT_16 u2Capability;
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_BEACON_TEMPLATE_UPDATE, *P_CMD_BEACON_TEMPLATE_UPDATE;

typedef struct _CMD_SET_WMM_PS_TEST_STRUCT_T {
	UINT_8 ucBssIndex;
	UINT_8 bmfgApsdEnAc;	/* b0~3: trigger-en AC0~3. b4~7: delivery-en AC0~3 */
	UINT_8 ucIsEnterPsAtOnce;	/* enter PS immediately without 5 second guard after connected */
	UINT_8 ucIsDisableUcTrigger;	/* not to trigger UC on beacon TIM is matched (under U-APSD) */
} CMD_SET_WMM_PS_TEST_STRUCT_T, *P_CMD_SET_WMM_PS_TEST_STRUCT_T;

typedef struct _GSCN_CHANNEL_INFO_T {
	UINT_8 ucBand;
	UINT_8 ucChannelNumber;	/* Channel Number */
	UINT_8 ucPassive;	/* 0 => active, 1 => passive scan; ignored for DFS */
	UINT_8 aucReserved[1];

	UINT_32 u4DwellTimeMs;	/* dwell time hint */
	/* Add channel class */
} GSCN_CHANNEL_INFO_T, *P_GSCN_CHANNEL_INFO_T;

typedef struct _GSCAN_BUCKET_T {
	UINT_16 u2BucketIndex;	/* bucket index, 0 based */
	UINT_8 ucBucketFreqMultiple;	/* desired period, in millisecond;
					 * if this is too low, the firmware should choose to generate
					 * results as fast as it can instead of failing the command */
	 /* report_events semantics -
	  *  This is a bit field; which defines following bits -
	  *  REPORT_EVENTS_EACH_SCAN	=> report a scan completion event after scan. If this is not set
	  *				    then scan completion events should be reported if
	  *				    report_threshold_percent or report_threshold_num_scans is
	  *				    reached.
	  *  REPORT_EVENTS_FULL_RESULTS => forward scan results (beacons/probe responses + IEs)
	  *				    in real time to HAL, in addition to completion events
	  *				    Note: To keep backward compatibility, fire completion
	  *				    events regardless of REPORT_EVENTS_EACH_SCAN.
	  *  REPORT_EVENTS_NO_BATCH	=> controls if scans for this bucket should be placed in the
	  *				    history buffer
	  */
	UINT_8 ucReportFlag;
	UINT_8 ucMaxBucketFreqMultiple; /* max_period / base_period */
	UINT_8 ucStepCount;
	UINT_8 ucNumChannels;
	UINT_8 aucReserved[1];
	WIFI_BAND eBand;	/* when UNSPECIFIED, use channel list */
	GSCN_CHANNEL_INFO_T arChannelList[GSCAN_MAX_CHANNELS];	/* channels to scan; these may include DFS channels */
} GSCAN_BUCKET_T, *P_GSCAN_BUCKET_T;

typedef struct _CMD_GSCN_REQ_T {
	UINT_8 ucFlags;
	UINT_8 ucNumScnToCache;
	UINT_8 aucReserved[2];
	UINT_32 u4BufferThreshold;
	UINT_32 u4BasePeriod;	/* base timer period in ms */
	UINT_32 u4NumBuckets;
	UINT_32 u4MaxApPerScan;	/* number of APs to store in each scan in the */
	/* BSSID/RSSI history buffer (keep the highest RSSI APs) */
	GSCAN_BUCKET_T arBucket[GSCAN_MAX_BUCKETS];
} CMD_GSCN_REQ_T, *P_CMD_GSCN_REQ_T;

typedef struct _CMD_GSCN_SCN_COFIG_T {
	UINT_8 ucNumApPerScn;		/* GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN */
	UINT_32 u4BufferThreshold;	/* GSCAN_ATTRIBUTE_REPORT_THRESHOLD */
	UINT_32 u4NumScnToCache;	/* GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE */
} CMD_GSCN_SCN_COFIG_T, *P_CMD_GSCN_SCN_COFIG_T;

typedef struct _CMD_GET_GSCAN_RESULT {
	UINT_8 ucVersion;
	UINT_8 aucReserved[2];
	UINT_8 ucFlush;
	UINT_32 u4Num;
} CMD_GET_GSCAN_RESULT_T, *P_CMD_GET_GSCAN_RESULT_T;

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

typedef struct _CMD_SCAN_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	UINT_8 ucSSIDLength;
	UINT_8 ucNumProbeReq;
	UINT_16 u2ChannelMinDwellTime;
	UINT_16 u2ChannelDwellTime;
	UINT_16 u2TimeoutValue;
	UINT_8 aucSSID[32];
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	UINT_8 aucReserved[2];
	CHANNEL_INFO_T arChannelList[32];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ, *P_CMD_SCAN_REQ;

typedef struct _CMD_SCAN_REQ_V2_T {
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_8 ucScanType;
	UINT_8 ucSSIDType;
	UINT_8 ucSSIDNum;
	UINT_8 ucNumProbeReq;
	UINT_8 aucReserved[2];
	PARAM_SSID_T arSSID[4];
	UINT_16 u2ProbeDelayTime;
	UINT_16 u2ChannelDwellTime;
	UINT_16 u2TimeoutValue;
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	CHANNEL_INFO_T arChannelList[32];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} CMD_SCAN_REQ_V2, *P_CMD_SCAN_REQ_V2;

typedef struct _CMD_SCAN_CANCEL_T {
	UINT_8 ucSeqNum;
	UINT_8 ucIsExtChannel;	/* For P2P channel extension. */
	UINT_8 aucReserved[2];
} CMD_SCAN_CANCEL, *P_CMD_SCAN_CANCEL;

/* 20150107  Daniel Added complete channels number in the scan done event */
/* before*/
/*
typedef struct _EVENT_SCAN_DONE_T {
    UINT_8          ucSeqNum;
    UINT_8          ucSparseChannelValid;
    CHANNEL_INFO_T  rSparseChannel;
} EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;
*/
/* after */
typedef struct _EVENT_SCAN_DONE_T {
	UINT_8 ucSeqNum;
	UINT_8 ucSparseChannelValid;
	CHANNEL_INFO_T rSparseChannel;
	/*scan done version #2 */
	UINT_8 ucCompleteChanCount;
	UINT_8 ucCurrentState;
	UINT_8 ucScanDoneVersion;
	/*scan done version #3 */
	UINT_8 ucReserved;
	UINT_32 u4ScanDurBcnCnt;
	UINT_8 fgIsPNOenabled;
	UINT_8 aucReserving[3];
} EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;

#if CFG_SUPPORT_BATCH_SCAN
typedef struct _CMD_BATCH_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucCmd;		/* Start/ Stop */
	UINT_8 ucMScan;		/* an integer number of scans per batch */
	UINT_8 ucBestn;		/* an integer number of the max AP to remember per scan */
	UINT_8 ucRtt;		/* an integer number of highest-strength AP for which we'd
				   like approximate distance reported */
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
	UINT_8 ucBssIndex;
	UINT_8 ucTokenID;
	UINT_8 ucAction;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucRfBand;
	UINT_8 ucRfChannelWidth;	/* To support 80/160MHz bandwidth */
	UINT_8 ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	UINT_8 ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	UINT_8 ucReqType;
	UINT_8 aucReserved[2];
	UINT_32 u4MaxInterval;	/* In unit of ms */
} CMD_CH_PRIVILEGE_T, *P_CMD_CH_PRIVILEGE_T;

typedef struct _CMD_TX_PWR_T {
	INT_8 cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
	INT_8 cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	INT_8 acReserved[2];

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

typedef struct _CMD_TX_AC_PWR_T {
	INT_8 ucBand;
#if 0
	INT_8 c11AcTxPwr_BPSK;
	INT_8 c11AcTxPwr_QPSK;
	INT_8 c11AcTxPwr_16QAM;
	INT_8 c11AcTxPwr_MCS5_MCS6;
	INT_8 c11AcTxPwr_MCS7;
	INT_8 c11AcTxPwr_MCS8;
	INT_8 c11AcTxPwr_MCS9;
	INT_8 c11AcTxPwrVht40_OFFSET;
	INT_8 c11AcTxPwrVht80_OFFSET;
	INT_8 c11AcTxPwrVht160_OFFSET;
#else
	AC_PWR_SETTING_STRUCT rAcPwr;
#endif
} CMD_TX_AC_PWR_T, *P_CMD_TX_AC_PWR_T;

typedef struct _CMD_RSSI_PATH_COMPASATION_T {
	INT_8 c2GRssiCompensation;
	INT_8 c5GRssiCompensation;
} CMD_RSSI_PATH_COMPASATION_T, *P_CMD_RSSI_PATH_COMPASATION_T;
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
	UINT_8 ucBssIndex;
	UINT_8 aucReserved[2];
	UINT_8 aucLevelRcpiTh[3];
	UINT_8 aucReserved2[1];
	INT_8 aicLevelPowerOffset[3];	/* signed, in unit of 0.5dBm */
	UINT_8 aucReserved3[1];
	UINT_8 aucReserved4[8];
} CMD_AUTO_POWER_PARAM_T, *P_CMD_AUTO_POWER_PARAM_T;

typedef struct _EVENT_CH_PRIVILEGE_T {
	UINT_8 ucBssIndex;
	UINT_8 ucTokenID;
	UINT_8 ucStatus;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucRfSco;
	UINT_8 ucRfBand;
	UINT_8 ucRfChannelWidth;	/* To support 80/160MHz bandwidth */
	UINT_8 ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	UINT_8 ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	UINT_8 ucReqType;
	UINT_8 aucReserved[2];
	UINT_32 u4GrantInterval;	/* In unit of ms */
} EVENT_CH_PRIVILEGE_T, *P_EVENT_CH_PRIVILEGE_T;

typedef struct _EVENT_BSS_BEACON_TIMEOUT_T {
	UINT_8 ucBssIndex;
	UINT_8 ucReasonCode;
	UINT_8 aucReserved[2];
} EVENT_BSS_BEACON_TIMEOUT_T, *P_EVENT_BSS_BEACON_TIMEOUT_T;

typedef struct _EVENT_STA_AGING_TIMEOUT_T {
	UINT_8 ucStaRecIdx;
	UINT_8 aucReserved[3];
} EVENT_STA_AGING_TIMEOUT_T, *P_EVENT_STA_AGING_TIMEOUT_T;

typedef struct _EVENT_NOA_TIMING_T {
	UINT_8 ucIsInUse;	/* Indicate if this entry is in use or not */
	UINT_8 ucCount;		/* Count */
	UINT_8 aucReserved[2];

	UINT_32 u4Duration;	/* Duration */
	UINT_32 u4Interval;	/* Interval */
	UINT_32 u4StartTime;	/* Start Time */
} EVENT_NOA_TIMING_T, *P_EVENT_NOA_TIMING_T;

typedef struct _EVENT_UPDATE_NOA_PARAMS_T {
	UINT_8 ucBssIndex;
	UINT_8 aucReserved[2];
	UINT_8 ucEnableOppPS;
	UINT_16 u2CTWindow;

	UINT_8 ucNoAIndex;
	UINT_8 ucNoATimingCount;	/* Number of NoA Timing */
	EVENT_NOA_TIMING_T arEventNoaTiming[8 /*P2P_MAXIMUM_NOA_COUNT */];
} EVENT_UPDATE_NOA_PARAMS_T, *P_EVENT_UPDATE_NOA_PARAMS_T;

typedef struct _EVENT_AP_OBSS_STATUS_T {
	UINT_8 ucBssIndex;
	UINT_8 ucObssErpProtectMode;
	UINT_8 ucObssHtProtectMode;
	UINT_8 ucObssGfOperationMode;
	UINT_8 ucObssRifsOperationMode;
	UINT_8 ucObssBeaconForcedTo20M;
	UINT_8 aucReserved[2];
} EVENT_AP_OBSS_STATUS_T, *P_EVENT_AP_OBSS_STATUS_T;

typedef struct _EVENT_DEBUG_MSG_T {
	UINT_16 u2DebugMsgId;
	UINT_8 ucMsgType;
	UINT_8 ucFlags;		/* unused */
	UINT_32 u4Value;	/* memory addre or ... */
	UINT_16 u2MsgSize;
	UINT_8 aucReserved0[2];
	UINT_8 aucMsg[1];
} EVENT_DEBUG_MSG_T, *P_EVENT_DEBUG_MSG_T;

typedef struct _CMD_EDGE_TXPWR_LIMIT_T {
	INT_8 cBandEdgeMaxPwrCCK;
	INT_8 cBandEdgeMaxPwrOFDM20;
	INT_8 cBandEdgeMaxPwrOFDM40;
	INT_8 cBandEdgeMaxPwrOFDM80;
} CMD_EDGE_TXPWR_LIMIT_T, *P_CMD_EDGE_TXPWR_LIMIT_T;

typedef struct _CMD_POWER_OFFSET_T {
	UINT_8 ucBand;		/*1:2.4G ;  2:5G */
	UINT_8 ucSubBandOffset[MAX_SUBBAND_NUM_5G];	/*the max num subband is 5G, devide with 8 subband */
	UINT_8 aucReverse[3];

} CMD_POWER_OFFSET_T, *P_CMD_POWER_OFFSET_T;

typedef struct _CMD_NVRAM_SETTING_T {

	WIFI_CFG_PARAM_STRUCT rNvramSettings;

} CMD_NVRAM_SETTING_T, *P_CMD_NVRAM_SETTING_T;

#if CFG_SUPPORT_TDLS
typedef struct _CMD_TDLS_CH_SW_T {
	BOOLEAN fgIsTDLSChSwProhibit;
} CMD_TDLS_CH_SW_T, *P_CMD_TDLS_CH_SW_T;
#endif

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

typedef struct _EVENT_ICAP_STATUS_T {
	UINT_8 ucRddStatus;
	UINT_8 aucReserved[3];
	UINT_32 u4StartAddress;
	UINT_32 u4IcapSieze;
} EVENT_ICAP_STATUS_T, *P_EVENT_ICAP_STATUS_T;

typedef struct _CMD_SET_TXPWR_CTRL_T {
	INT_8 c2GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c2GHotspotPwrOffset;
	INT_8 c2GP2pPwrOffset;
	INT_8 c2GBowPwrOffset;
	INT_8 c5GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c5GHotspotPwrOffset;
	INT_8 c5GP2pPwrOffset;
	INT_8 c5GBowPwrOffset;
	UINT_8 ucConcurrencePolicy;	/* TX power policy when concurrence
					   in the same channel
					   0: Highest power has priority
					   1: Lowest power has priority */
	INT_8 acReserved1[3];	/* Must be zero */

	/* Power limit by channel for all data rates */
	INT_8 acTxPwrLimit2G[14];	/* Channel 1~14, Unit: 0.5dBm */
	INT_8 acTxPwrLimit5G[4];	/* UNII 1~4 */
	INT_8 acReserved2[2];	/* Must be zero */
} CMD_SET_TXPWR_CTRL_T, *P_CMD_SET_TXPWR_CTRL_T;

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

struct NLO_SSID_MATCH_SETS {
	INT_8 cRssiThresold;
	UINT_8 ucSSIDLength;
	UINT_8 aucSSID[32];
};

struct NLO_NETWORK {
	UINT_8 ucChannelType; /* 0: specific channel; 1: dual band; 2: 2.4G; 3: 5G; 3*/
	UINT_8 ucChnlNum;
	UINT_8 aucChannel[94];
	struct NLO_SSID_MATCH_SETS arMatchSets[16];
};

typedef struct _CMD_NLO_REQ {
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_8 fgStopAfterIndication;
	UINT_8 ucFastScanIteration;
	UINT_16 u2FastScanPeriod;
	UINT_16 u2SlowScanPeriod;
	UINT_8 ucEntryNum;
	UINT_8 ucFlag;		/* BIT(0) Check cipher */
	UINT_16 u2IELen;
	struct NLO_NETWORK rNLONetwork;
	UINT_8 aucIE[0];
} CMD_NLO_REQ, *P_CMD_NLO_REQ;

typedef struct _CMD_NLO_CANCEL_T {
	UINT_8 ucSeqNum;
	UINT_8 ucBssIndex;
	UINT_8 aucReserved[2];
} CMD_NLO_CANCEL, *P_CMD_NLO_CANCEL;

typedef struct _EVENT_NLO_DONE_T {
	UINT_8 ucSeqNum;
	UINT_8 ucStatus;
	UINT_8 aucReserved[2];
} EVENT_NLO_DONE_T, *P_EVENT_NLO_DONE_T;

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
	UINT_8 ucLlsReadClear;
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	UINT_8 aucReserved1[2];
	UINT_8 aucReserved2[16];
} CMD_GET_STA_STATISTICS_T, *P_CMD_GET_STA_STATISTICS_T;

/* per access category statistics */
typedef struct _WIFI_WMM_AC_STAT_GET_FROM_FW_T {
	UINT_32 u4TxFailMsdu;
	UINT_32 u4TxRetryMsdu;
} WIFI_WMM_AC_STAT_GET_FROM_FW_T, *P_WIFI_WMM_AC_STAT_GET_FROM_FW_T;

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
	UINT_32 u4TransmitCount;	/* Transmit in the air (wtbl) */
	UINT_32 u4TransmitFailCount;	/* Transmit without ack/ba in the air (wtbl) */

	WIFI_WMM_AC_STAT_GET_FROM_FW_T arLinkStatistics[AC_NUM];	/*link layer statistics */

	UINT_8 aucReserved[24];
} EVENT_STA_STATISTICS_T, *P_EVENT_STA_STATISTICS_T;

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
typedef struct _EVENT_LTE_SAFE_CHN_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved[3];
	UINT_32 u4Flags;	/* Bit0: valid */
	LTE_SAFE_CHN_INFO_T rLteSafeChn;
} EVENT_LTE_SAFE_CHN_T, *P_EVENT_LTE_SAFE_CHN_T;
#endif

#if CFG_SUPPORT_SNIFFER
typedef struct _CMD_MONITOR_SET_INFO_T {
	UINT_8 ucEnable;
	UINT_8 ucBand;
	UINT_8 ucPriChannel;
	UINT_8 ucSco;
	UINT_8 ucChannelWidth;
	UINT_8 ucChannelS1;
	UINT_8 ucChannelS2;
	UINT_8 aucResv[9];
} CMD_MONITOR_SET_INFO_T, *P_CMD_MONITOR_SET_INFO_T;
#endif

typedef struct _CMD_STATS_LOG_T {
	UINT_32 u4DurationInMs;
	UINT_8 aucReserved[32];
} CMD_STATS_LOG_T, *P_CMD_STATS_LOG_T;

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

typedef struct _CMD_SET_PSCAN_PARAM {
	UINT_8 ucVersion;
	CMD_NLO_REQ rCmdNloReq;
	CMD_BATCH_REQ_T rCmdBatchReq;
	CMD_GSCN_REQ_T rCmdGscnReq;
	BOOLEAN fgNLOScnEnable;
	BOOLEAN fgBatchScnEnable;
	BOOLEAN fgGScnEnable;
	UINT_32 u4BasePeriod;
} CMD_SET_PSCAN_PARAM, *P_CMD_SET_PSCAN_PARAM;

typedef struct _CMD_SET_PSCAN_ADD_HOTLIST_BSSID {
	UINT_8 aucMacAddr[6];
	UINT_8 ucFlags;
	UINT_8 aucReserved[5];
} CMD_SET_PSCAN_ADD_HOTLIST_BSSID, *P_CMD_SET_PSCAN_ADD_HOTLIST_BSSID;

typedef struct _CMD_SET_PSCAN_ADD_SWC_BSSID {
	INT_32 i4RssiLowThreshold;
	INT_32 i4RssiHighThreshold;
	UINT_8 aucMacAddr[6];
	UINT_8 aucReserved[6];
} CMD_SET_PSCAN_ADD_SWC_BSSID, *P_CMD_SET_PSCAN_ADD_SWC_BSSID;

typedef struct _CMD_SET_PSCAN_MAC_ADDR {
	UINT_8 ucVersion;
	UINT_8 ucFlags;
	UINT_8 aucMacAddr[6];
	UINT_8 aucReserved[8];
} CMD_SET_PSCAN_MAC_ADDR, *P_CMD_SET_PSCAN_MAC_ADDR;

typedef struct _CMD_SUSPEND_MODE_SETTING_T {
	UINT_8		ucBssIndex;
	UINT_8		fIsEnableSuspendMode;
	UINT_8		ucReserved[2];
} CMD_SUSPEND_MODE_SETTING_T, *P_CMD_SUSPEND_MODE_SETTING_T;

struct CMD_REQ_CHNL_UTILIZATION {
	UINT_16 u2MeasureDuration;
	UINT_8 ucChannelNum;
	UINT_8 aucChannelList[48];
	UINT_8 aucReserved[13];
};

struct EVENT_RSP_CHNL_UTILIZATION {
	UINT_8 ucChannelNum;
	UINT_8 aucChannelMeasureList[48];
	UINT_8 aucReserved0[15];
	UINT_8 aucChannelUtilization[48];
	UINT_8 aucReserved1[16];
	UINT_8 aucChannelBusyTime[48];
	UINT_8 aucReserved2[16];
};

struct CMD_TDLS_PS_T {
	UINT_8	ucIsEnablePs; /* 0: disable tdls power save; 1: enable tdls power save */
	UINT_8	aucReserved[3];
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

VOID nicEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQuerySwCtrlRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryChipConfig(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryRfTestATInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetDisassociate(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventSetIpAddress(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryLinkQuality(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryLinkSpeed(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryBugReport(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventEnterRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventLeaveRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

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

VOID nicOidCmdEnterRFTestTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

#if CFG_SUPPORT_BUILD_DATE_CODE
VOID nicCmdEventBuildDateCode(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

VOID nicCmdEventQueryStaStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
VOID nicCmdEventQueryLteSafeChn(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

#if CFG_SUPPORT_BATCH_SCAN
VOID nicCmdEventBatchScanResult(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

#ifdef FW_CFG_SUPPORT
VOID nicCmdEventQueryCfgRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _NIC_CMD_EVENT_H */
