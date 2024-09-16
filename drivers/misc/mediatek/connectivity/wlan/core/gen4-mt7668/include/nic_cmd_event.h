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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic_cmd_event.h#1
*/

/*! \file   "nic_cmd_event.h"
*    \brief This file contains the declairation file of the WLAN OID processing routines
*	   of Windows driver for MediaTek Inc. 802.11 Wireless LAN Adapters.
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
#define CMD_PQ_ID           (0x8000)
#define CMD_PACKET_TYPE_ID  (0xA0)

#define PKT_FT_CMD			0x2

#define CMD_STATUS_SUCCESS      0
#define CMD_STATUS_REJECTED     1
#define CMD_STATUS_UNKNOWN      2

#define EVENT_HDR_WITHOUT_RXD_SIZE (OFFSET_OF(WIFI_EVENT_T, aucBuffer[0]) - OFFSET_OF(WIFI_EVENT_T, u2PacketLength))

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
#define S2D_INDEX_CMD_H2N		0x0
#define S2D_INDEX_CMD_C2N		0x1
#define S2D_INDEX_CMD_H2C		0x2
#define S2D_INDEX_CMD_H2N_H2C	0x3

#define S2D_INDEX_EVENT_N2H		0x0
#define S2D_INDEX_EVENT_N2C		0x1
#define S2D_INDEX_EVENT_C2H		0x2
#define S2D_INDEX_EVENT_N2H_N2C	0x3

#define RDD_EVENT_HDR_SIZE              20
#define RDD_ONEPLUSE_SIZE               8 /* size of one pulse is 8 bytes */
#define RDD_PULSE_OFFSET0               0
#define RDD_PULSE_OFFSET1               1
#define RDD_PULSE_OFFSET2               2
#define RDD_PULSE_OFFSET3               3
#define RDD_PULSE_OFFSET4               4
#define RDD_PULSE_OFFSET5               5
#define RDD_PULSE_OFFSET6               6
#define RDD_PULSE_OFFSET7               7

#if (CFG_SUPPORT_DFS_MASTER == 1)
#define RDD_IN_SEL_0                    0
#define RDD_IN_SEL_1                    1
#endif

#if CFG_SUPPORT_QA_TOOL
#define IQ_FILE_LINE_OFFSET     18
#define IQ_FILE_IQ_STR_LEN	 8
#define RTN_IQ_DATA_LEN         1024	/* return 1k per packet */

#define MCAST_WCID_TO_REMOVE	0

/* Network type */
#define NETWORK_INFRA	BIT(16)
#define NETWORK_P2P		BIT(17)
#define NETWORK_IBSS	BIT(18)
#define NETWORK_MESH	BIT(19)
#define NETWORK_BOW		BIT(20)
#define NETWORK_WDS		BIT(21)

/* Station role */
#define STA_TYPE_STA BIT(0)
#define STA_TYPE_AP BIT(1)
#define STA_TYPE_ADHOC BIT(2)
#define STA_TYPE_TDLS BIT(3)
#define STA_TYPE_WDS BIT(4)

/* Connection type */
#define CONNECTION_INFRA_STA		(STA_TYPE_STA|NETWORK_INFRA)
#define CONNECTION_INFRA_AP		(STA_TYPE_AP|NETWORK_INFRA)
#define CONNECTION_P2P_GC			(STA_TYPE_STA|NETWORK_P2P)
#define CONNECTION_P2P_GO			(STA_TYPE_AP|NETWORK_P2P)
#define CONNECTION_MESH_STA		(STA_TYPE_STA|NETWORK_MESH)
#define CONNECTION_MESH_AP		(STA_TYPE_AP|NETWORK_MESH)
#define CONNECTION_IBSS_ADHOC		(STA_TYPE_ADHOC|NETWORK_IBSS)
#define CONNECTION_TDLS			(STA_TYPE_STA|NETWORK_INFRA|STA_TYPE_TDLS)
#define CONNECTION_WDS			(STA_TYPE_WDS|NETWORK_WDS)

#define ICAP_CONTENT_ADC		0x10000006
#define ICAP_CONTENT_TOAE		0x7
#define ICAP_CONTENT_SPECTRUM	0xB
#define ICAP_CONTENT_RBIST		0x10
#define ICAP_CONTENT_DCOC		0x20
#define ICAP_CONTENT_FIIQ		0x48
#define ICAP_CONTENT_FDIQ		0x49

#if CFG_SUPPORT_BUFFER_MODE

typedef struct _CMD_EFUSE_BUFFER_MODE_T {
	UINT_8 ucSourceMode;
	UINT_8 ucCount;
	UINT_8 ucCmdType;  /* 0:6632, 1: 7668 */
	UINT_8 ucReserved;
	UINT_8 aBinContent[MAX_EEPROM_BUFFER_SIZE];
} CMD_EFUSE_BUFFER_MODE_T, *P_CMD_EFUSE_BUFFER_MODE_T;


/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
typedef struct _CMD_ACCESS_EFUSE_T {
	UINT_32 u4Address;
	UINT_32 u4Valid;
	UINT_8 aucData[16];
} CMD_ACCESS_EFUSE_T, *P_CMD_ACCESS_EFUSE_T;

typedef struct _CMD_EFUSE_FREE_BLOCK_T {
	UINT_8  ucGetFreeBlock;
	UINT_8  aucReserved[3];
} CMD_EFUSE_FREE_BLOCK_T, *P_CMD_EFUSE_FREE_BLOCK_T;


typedef struct _CMD_GET_TX_POWER_T {
	UINT_8 ucTxPwrType;
	UINT_8 ucCenterChannel;
	UINT_8 ucDbdcIdx; /* 0:Band 0, 1: Band1 */
	UINT_8 ucBand; /* 0:G-band 1: A-band*/
	UINT_8 ucReserved[4];
} CMD_GET_TX_POWER_T, *P_CMD_GET_TX_POWER_T;

/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */


typedef struct _CMD_SET_TX_TARGET_POWER_T {
	INT_8 cTxPwr2G4Cck;       /* signed, in unit of 0.5dBm */
	INT_8 cTxPwr2G4Dsss;      /* signed, in unit of 0.5dBm */
	UINT_8 ucTxTargetPwr;		/* Tx target power base for all*/
	UINT_8 ucReserved;

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
} CMD_SET_TX_TARGET_POWER_T, *P_CMD_SET_TX_TARGET_POWER_T;


/*
* Definitions for extension CMD_ID
*/
typedef enum _ENUM_EXT_CMD_ID_T {
	EXT_CMD_ID_EFUSE_ACCESS = 0x01,
	EXT_CMD_ID_RF_REG_ACCESS = 0x02,
	EXT_CMD_ID_EEPROM_ACCESS = 0x03,
	EXT_CMD_ID_RF_TEST = 0x04,
	EXT_CMD_ID_RADIO_ON_OFF_CTRL = 0x05,
	EXT_CMD_ID_WIFI_RX_DISABLE = 0x06,
	EXT_CMD_ID_PM_STATE_CTRL = 0x07,
	EXT_CMD_ID_CHANNEL_SWITCH = 0x08,
	EXT_CMD_ID_NIC_CAPABILITY = 0x09,
	EXT_CMD_ID_AP_PWR_SAVING_CLEAR = 0x0A,
	EXT_CMD_ID_SET_WTBL2_RATETABLE = 0x0B,
	EXT_CMD_ID_GET_WTBL_INFORMATION = 0x0C,
	EXT_CMD_ID_ASIC_INIT_UNINIT_CTRL = 0x0D,
	EXT_CMD_ID_MULTIPLE_REG_ACCESS = 0x0E,
	EXT_CMD_ID_AP_PWR_SAVING_CAPABILITY = 0x0F,
	EXT_CMD_ID_SECURITY_ADDREMOVE_KEY = 0x10,
	EXT_CMD_ID_SET_TX_POWER_CONTROL = 0x11,
	EXT_CMD_ID_SET_THERMO_CALIBRATION = 0x12,
	EXT_CMD_ID_FW_LOG_2_HOST = 0x13,
	EXT_CMD_ID_AP_PWR_SAVING_START = 0x14,
	EXT_CMD_ID_MCC_OFFLOAD_START = 0x15,
	EXT_CMD_ID_MCC_OFFLOAD_STOP = 0x16,
	EXT_CMD_ID_LED = 0x17,
	EXT_CMD_ID_PACKET_FILTER = 0x18,
	EXT_CMD_ID_COEXISTENCE = 0x19,
	EXT_CMD_ID_PWR_MGT_BIT_WIFI = 0x1B,
	EXT_CMD_ID_GET_TX_POWER = 0x1C,
	EXT_CMD_ID_BF_ACTION = 0x1E,

	EXT_CMD_ID_WMT_CMD_OVER_WIFI = 0x20,
	EXT_CMD_ID_EFUSE_BUFFER_MODE = 0x21,
	EXT_CMD_ID_OFFLOAD_CTRL = 0x22,
	EXT_CMD_ID_THERMAL_PROTECT = 0x23,
	EXT_CMD_ID_CLOCK_SWITCH_DISABLE = 0x24,
	EXT_CMD_ID_STAREC_UPDATE = 0x25,
	EXT_CMD_ID_BSSINFO_UPDATE = 0x26,
	EXT_CMD_ID_EDCA_SET = 0x27,
	EXT_CMD_ID_SLOT_TIME_SET = 0x28,
	EXT_CMD_ID_DEVINFO_UPDATE = 0x2A,
	EXT_CMD_ID_NOA_OFFLOAD_CTRL = 0x2B,
	EXT_CMD_ID_GET_SENSOR_RESULT = 0x2C,
	EXT_CMD_ID_TMR_CAL = 0x2D,
	EXT_CMD_ID_WAKEUP_OPTION = 0x2E,
	EXT_CMD_ID_OBTW = 0x2F,

	EXT_CMD_ID_GET_TX_STATISTICS = 0x30,
	EXT_CMD_ID_AC_QUEUE_CONTROL = 0x31,
	EXT_CMD_ID_WTBL_UPDATE = 0x32,
	EXT_CMD_ID_BCN_UPDATE = 0x33,

	EXT_CMD_ID_DRR_CTRL = 0x36,
	EXT_CMD_ID_BSSGROUP_CTRL = 0x37,
	EXT_CMD_ID_VOW_FEATURE_CTRL = 0x38,
	EXT_CMD_ID_PKT_PROCESSOR_CTRL = 0x39,
	EXT_CMD_ID_PALLADIUM = 0x3A,
#if CFG_SUPPORT_MU_MIMO
	EXT_CMD_ID_MU_CTRL = 0x40,
#endif /* CFG_SUPPORT_MU_MIMO */

	EXT_CMD_ID_EFUSE_FREE_BLOCK = 0x4F

} ENUM_EXT_CMD_ID_T, *P_ENUM_EXT_CMD_ID_T;

typedef enum _NDIS_802_11_WEP_STATUS {
	Ndis802_11WEPEnabled,
	Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
	Ndis802_11WEPDisabled,
	Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
	Ndis802_11WEPKeyAbsent,
	Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
	Ndis802_11WEPNotSupported,
	Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
	Ndis802_11TKIPEnable,
	Ndis802_11Encryption2Enabled = Ndis802_11TKIPEnable,
	Ndis802_11Encryption2KeyAbsent,
	Ndis802_11AESEnable,
	Ndis802_11Encryption3Enabled = Ndis802_11AESEnable,
	Ndis802_11CCMP256Enable,
	Ndis802_11GCMP128Enable,
	Ndis802_11GCMP256Enable,
	Ndis802_11Encryption3KeyAbsent,
	Ndis802_11TKIPAESMix,
	Ndis802_11Encryption4Enabled = Ndis802_11TKIPAESMix,	/* TKIP or AES mix */
	Ndis802_11Encryption4KeyAbsent,
	Ndis802_11GroupWEP40Enabled,
	Ndis802_11GroupWEP104Enabled,
#ifdef WAPI_SUPPORT
	Ndis802_11EncryptionSMS4Enabled,	/* WPI SMS4 support */
#endif /* WAPI_SUPPORT */
} NDIS_802_11_WEP_STATUS, *PNDIS_802_11_WEP_STATUS, NDIS_802_11_ENCRYPTION_STATUS, *PNDIS_802_11_ENCRYPTION_STATUS;

#if CFG_SUPPORT_MU_MIMO
enum {
	/* debug commands */
	MU_SET_ENABLE = 0,
	MU_GET_ENABLE,
	MU_SET_MUPROFILE_ENTRY,
	MU_GET_MUPROFILE_ENTRY,
	MU_SET_GROUP_TBL_ENTRY,
	MU_GET_GROUP_TBL_ENTRY,
	MU_SET_CLUSTER_TBL_ENTRY,
	MU_GET_CLUSTER_TBL_ENTRY,
	MU_SET_GROUP_USER_THRESHOLD,
	MU_GET_GROUP_USER_THRESHOLD,
	MU_SET_GROUP_NSS_THRESHOLD,
	MU_GET_GROUP_NSS_THRESHOLD,
	MU_SET_TXREQ_MIN_TIME,
	MU_GET_TXREQ_MIN_TIME,
	MU_SET_SU_NSS_CHECK,
	MU_GET_SU_NSS_CHECK,
	MU_SET_CALC_INIT_MCS,
	MU_GET_CALC_INIT_MCS,
	MU_SET_TXOP_DEFAULT,
	MU_GET_TXOP_DEFAULT,
	MU_SET_SU_LOSS_THRESHOLD,
	MU_GET_SU_LOSS_THRESHOLD,
	MU_SET_MU_GAIN_THRESHOLD,
	MU_GET_MU_GAIN_THRESHOLD,
	MU_SET_SECONDARY_AC_POLICY,
	MU_GET_SECONDARY_AC_POLICY,
	MU_SET_GROUP_TBL_DMCS_MASK,
	MU_GET_GROUP_TBL_DMCS_MASK,
	MU_SET_MAX_GROUP_SEARCH_CNT,
	MU_GET_MAX_GROUP_SEARCH_CNT,
	MU_GET_MU_PROFILE_TX_STATUS_CNT,
	MU_SET_TRIGGER_MU_TX,
	/* F/W flow test commands */
	MU_SET_TRIGGER_GID_MGMT_FRAME,
	/* HQA commands */
	MU_HQA_SET_STA_PARAM,
	MU_HQA_SET_ENABLE,
	MU_HQA_SET_SNR_OFFSET,
	MU_HQA_SET_ZERO_NSS,
	MU_HQA_SET_SPEED_UP_LQ,
	MU_HQA_SET_GROUP,
	MU_HQA_SET_MU_TABLE,
	MU_HQA_SET_CALC_LQ,
	MU_HQA_GET_CALC_LQ,
	MU_HQA_SET_CALC_INIT_MCS,
	MU_HQA_GET_CALC_INIT_MCS,
	MU_HQA_GET_QD,
};
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_QA_TOOL */

typedef enum _ENUM_CMD_ID_T {
	CMD_ID_DUMMY_RSV = 0x00,	/* 0x00 (Set) */
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
	CMD_ID_SET_DBDC_PARMS = 0x28,	/* 0x28 (Set) */

	CMD_ID_KEEP_FULL_PWR = 0x2A,	/* 0x2A (Set) */

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

#if CFG_WOW_SUPPORT
	CMD_ID_SET_WOWLAN,			/* 0x4a (Set) */
#endif

#if CFG_SUPPORT_CSI
	CMD_ID_CSI_CONTROL = 0x4c, /* 0x4c (Set /Query) */
#endif

	CMD_ID_SET_MDNS_RECORD = 0X4e,

#if CFG_SUPPORT_WIFI_HOST_OFFLOAD
	CMD_ID_SET_AM_FILTER = 0x55,	/* 0x55 (Set) */
	CMD_ID_SET_AM_HEARTBEAT,	/* 0x56 (Set) */
	CMD_ID_SET_AM_TCP,		/* 0x57 (Set) */
#endif
	CMD_ID_SET_SUSPEND_MODE = 0x58,	/* 0x58 (Set) */

#if CFG_WOW_SUPPORT
	CMD_ID_SET_PF_CAPABILITY = 0x59,	/* 0x59 (Set) */
#endif

#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	CMD_ID_SET_ROAMING_SKIP = 0x6D,	/* 0x6D (Set) used to setting roaming skip*/
#endif
	CMD_ID_GET_SET_CUSTOMER_CFG = 0x70, /* 0x70(Set) */
	CMD_ID_COEX_CTRL = 0x7C, /* 0x7C (Set/Query) */
	CMD_ID_GET_NIC_CAPABILITY = 0x80,	/* 0x80 (Query) */
	CMD_ID_GET_LINK_QUALITY,	/* 0x81 (Query) */
	CMD_ID_GET_STATISTICS,	/* 0x82 (Query) */
	CMD_ID_GET_CONNECTION_STATUS,	/* 0x83 (Query) */
	CMD_ID_GET_STA_STATISTICS = 0x85,	/* 0x85 (Query) */

	CMD_ID_GET_LTE_CHN = 0x87,	/* 0x87 (Query) */
	CMD_ID_GET_CHN_LOADING = 0x88,	/* 0x88 (Query) */
	CMD_ID_GET_BUG_REPORT = 0x89,	/* 0x89 (Query) */
	CMD_ID_GET_NIC_CAPABILITY_V2 = 0x8A,/* 0x8A (Query) */

#if (CFG_SUPPORT_DFS_MASTER == 1)
	CMD_ID_RDD_ON_OFF_CTRL = 0x8F, /* 0x8F(Set) */
#endif

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
	CMD_ID_CAL_BACKUP_IN_HOST_V2 = 0xAE,	/* 0xAE (Set / Query) */
#endif

	CMD_ID_ACCESS_REG = 0xc0,	/* 0xc0 (Set / Query) */
	CMD_ID_MAC_MCAST_ADDR,	/* 0xc1 (Set / Query) */
	CMD_ID_802_11_PMKID,	/* 0xc2 (Set / Query) */
	CMD_ID_ACCESS_EEPROM,	/* 0xc3 (Set / Query) */
	CMD_ID_SW_DBG_CTRL,	/* 0xc4 (Set / Query) */
	CMD_ID_FW_LOG_2_HOST,	/* 0xc5 (Set) */
	CMD_ID_DUMP_MEM,	/* 0xc6 (Query) */
	CMD_ID_RESOURCE_CONFIG,	/* 0xc7 (Set / Query) */
#if CFG_SUPPORT_QA_TOOL
	CMD_ID_ACCESS_RX_STAT,	/* 0xc8 (Query) */
#endif /* CFG_SUPPORT_QA_TOOL */
	CMD_ID_CHIP_CONFIG = 0xCA,	/* 0xca (Set / Query) */
	CMD_ID_STATS_LOG = 0xCB,	/* 0xcb (Set) */

	CMD_ID_WLAN_INFO	= 0xCD, /* 0xcd (Query) */
	CMD_ID_MIB_INFO		= 0xCE, /* 0xce (Query) */

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
	CMD_ID_TX_MCS_INFO	= 0xCF, /* 0xcf (Query) */
#endif
	CMD_ID_GET_TXPWR_TBL = 0xD0, /* 0xd0 (Query) */

	CMD_ID_SET_RDD_CH = 0xE1,

#if CFG_SUPPORT_QA_TOOL
	CMD_ID_LAYER_0_EXT_MAGIC_NUM = 0xED,	/* magic number for Extending MT6630 original CMD header */
#endif /* CFG_SUPPORT_QA_TOOL */

	CMD_ID_SET_BWCS = 0xF1,
	CMD_ID_SET_OSC = 0xF2,

	CMD_ID_HIF_CTRL = 0xF6,	/* 0xF6 (Set) */

	CMD_ID_GET_BUILD_DATE_CODE = 0xF8,	/* 0xf8 (Query) */
	CMD_ID_GET_BSS_INFO = 0xF9,	/* 0xF9 (Query) */
	CMD_ID_SET_HOTSPOT_OPTIMIZATION = 0xFA,	/* 0xFA (Set) */
	CMD_ID_SET_TDLS_CH_SW = 0xFB,
	CMD_ID_SET_MONITOR = 0xFC,	/* 0xFC (Set) */
#if CFG_SUPPORT_ADVANCE_CONTROL
	CMD_ID_ADV_CONTROL = 0xFE,	/* 0xFE (Set / Query) */
#endif
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

#if CFG_SUPPORT_CSI
	EVENT_ID_CSI_DATA = 0x3c, /* 0x3c (Query) */
#endif

	EVENT_ID_GET_GTK_REKEY_DATA = 0x3d, /* 0x3d (Query) */

	EVENT_ID_UART_ACK = 0x40,	/* 0x40 (Unsolicited) */
	EVENT_ID_UART_NAK,	/* 0x41 (Unsolicited) */
	EVENT_ID_GET_CHIPID,	/* 0x42 (Query - CMD_ID_GET_CHIPID) */
	EVENT_ID_SLT_STATUS,	/* 0x43 (Query - CMD_ID_SET_SLTINFO) */
	EVENT_ID_CHIP_CONFIG,	/* 0x44 (Query - CMD_ID_CHIP_CONFIG) */
#if CFG_SUPPORT_QA_TOOL
	EVENT_ID_ACCESS_RX_STAT,	/* 0x45 (Query - CMD_ID_ACCESS_RX_STAT) */
#endif /* CFG_SUPPORT_QA_TOOL */

	EVENT_ID_RDD_SEND_PULSE = 0x50,

#if CFG_SUPPORT_TX_BF
	EVENT_ID_PFMU_TAG_READ = 0x51,
	EVENT_ID_PFMU_DATA_READ = 0x52,
#endif

#if CFG_SUPPORT_MU_MIMO
	EVENT_ID_MU_GET_QD = 0x53,
	EVENT_ID_MU_GET_LQ = 0x54,
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
	EVENT_ID_RDD_REPORT = 0x60,
	EVENT_ID_CSA_DONE = 0x61,
#endif

#if (CFG_WOW_SUPPORT == 1)
	EVENT_ID_WOW_WAKEUP_REASON = 0x62,
#endif

	EVENT_ID_TDLS = 0x80,	/* TDLS event_id */

	EVENT_ID_UPDATE_COEX_PHYRATE = 0x90,	/* 0x90 (Unsolicited) */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
	EVENT_ID_CAL_BACKUP_IN_HOST_V2 = 0xAE,	/* 0xAE (Query - CMD_ID_CAL_BACKUP) */
	EVENT_ID_CAL_ALL_DONE = 0xAF,	/* 0xAF (FW Cal All Done Event) */
#endif

	EVENT_ID_WLAN_INFO = 0xCD,
	EVENT_ID_MIB_INFO = 0xCE,

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
	EVENT_ID_TX_MCS_INFO = 0xCF,
#endif
	EVENT_ID_GET_TXPWR_TBL = 0xD0,

	EVENT_ID_NIC_CAPABILITY_V2 = 0xEC,		/* 0xEC (Query - CMD_ID_GET_NIC_CAPABILITY_V2) */
/*#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)*/
	EVENT_ID_LAYER_0_EXT_MAGIC_NUM	= 0xED,	    /* magic number for Extending MT6630 original EVENT header  */
/*#endif*/

#if CFG_ASSERT_DUMP
	EVENT_ID_ASSERT_DUMP = 0xF0,
#endif
	EVENT_ID_HIF_CTRL = 0xF6,
	EVENT_ID_BUILD_DATE_CODE = 0xF8,
	EVENT_ID_GET_AIS_BSS_INFO = 0xF9,
	EVENT_ID_DEBUG_CODE = 0xFB,
	EVENT_ID_RFTEST_READY = 0xFC,	/* 0xFC */
#if CFG_SUPPORT_ADVANCE_CONTROL
	EVENT_ID_ADV_CONTROL = 0xFE,
#endif
	EVENT_ID_END
} ENUM_EVENT_ID_T, *P_ENUM_EVENT_ID_T;

#if CFG_WOW_SUPPORT

/* Filter Flag */
#define WOWLAN_FF_DROP_ALL                      BIT(0)
#define WOWLAN_FF_SEND_MAGIC_TO_HOST            BIT(1)
#define WOWLAN_FF_ALLOW_ARP                     BIT(2)
#define WOWLAN_FF_ALLOW_BMC                     BIT(3)
#define WOWLAN_FF_ALLOW_UC                      BIT(4)
#define WOWLAN_FF_ALLOW_1X                      BIT(5)
#define WOWLAN_FF_ALLOW_ARP_REQ2ME              BIT(6)

/* wow detect type */
#define WOWLAN_DETECT_TYPE_MAGIC                BIT(0)
#define WOWLAN_DETECT_TYPE_ALLOW_NORMAL         BIT(1)
#define WOWLAN_DETECT_TYPE_ONLY_PHONE_SUSPEND   BIT(2)
#define WOWLAN_DETECT_TYPE_DISASSOCIATION       BIT(3)
#define WOWLAN_DETECT_TYPE_BCN_LOST             BIT(4)

/* Wakeup command bit define */
#define WOWLAN_DETECT_TYPE_NONE                 0
#define PF_WAKEUP_CMD_BIT0_OUTPUT_MODE_EN   BIT(0)
#define PF_WAKEUP_CMD_BIT1_OUTPUT_DATA      BIT(1)
#define PF_WAKEUP_CMD_BIT2_WAKEUP_LEVEL     BIT(2)

#define PM_WOWLAN_REQ_START         0x1
#define PM_WOWLAN_REQ_STOP          0x2

typedef struct _CMD_WAKE_HIF_T {
	UINT_8		ucWakeupHif;	/* use in-band signal to wakeup system, ENUM_HIF_TYPE */
	UINT_8		ucGpioPin;		/* GPIO Pin */
	UINT_8		ucTriggerLvl;	/* GPIO Pin */
	UINT_8		aucResv1[1];
	UINT_32		u4GpioInterval;/* 0: low to high, 1: high to low */
	UINT_8		aucResv2[4];
} CMD_WAKE_HIF_T, *P_CMD_WAKE_HIF_T;

typedef struct _CMD_WOWLAN_PARAM_T {
	UINT_8		ucCmd;
	UINT_8		ucDetectType;
	UINT_16		u2FilterFlag; /* ARP/MC/DropExceptMagic/SendMagicToHost */
	UINT_8		ucScenarioID; /* WOW/WOBLE/Proximity */
	UINT_8		ucBlockCount;
	UINT_8		ucDbdcBand;
	UINT_8		aucReserved1[1];
	CMD_WAKE_HIF_T astWakeHif[2];
	WOW_PORT_T	stWowPort;
	UINT_8		aucReserved2[32];
} CMD_WOWLAN_PARAM_T, *P_CMD_WOWLAN_PARAM_T;

typedef struct _EVENT_WOWLAN_NOTIFY_T {
	UINT_8	ucNetTypeIndex;
	UINT_8	aucReserved[3];
} EVENT_WOWLAN_NOTIFY_T, *P_EVENT_WOWLAN_NOTIFY_T;

/* PACKETFILTER CAPABILITY TYPE */

#define PACKETF_CAP_TYPE_ARP			BIT(1)
#define PACKETF_CAP_TYPE_MAGIC			BIT(2)
#define PACKETF_CAP_TYPE_BITMAP			BIT(3)
#define PACKETF_CAP_TYPE_EAPOL			BIT(4)
#define PACKETF_CAP_TYPE_TDLS			BIT(5)
#define PACKETF_CAP_TYPE_CF				BIT(6)
#define PACKETF_CAP_TYPE_HEARTBEAT		BIT(7)
#define PACKETF_CAP_TYPE_TCP_SYN		BIT(8)
#define PACKETF_CAP_TYPE_UDP_SYN		BIT(9)
#define PACKETF_CAP_TYPE_BCAST_SYN		BIT(10)
#define PACKETF_CAP_TYPE_MCAST_SYN		BIT(11)
#define PACKETF_CAP_TYPE_V6				BIT(12)
#define PACKETF_CAP_TYPE_TDIM			BIT(13)


typedef enum _ENUM_FUNCTION_SELECT {
	FUNCTION_PF				= 1,
	FUNCTION_BITMAP			= 2,
	FUNCTION_EAPOL			= 3,
	FUNCTION_TDLS			= 4,
	FUNCTION_ARPNS			= 5,
	FUNCTION_CF				= 6,
	FUNCTION_MODE			= 7,
	FUNCTION_BSSID			= 8,
	FUNCTION_MGMT			= 9,
	FUNCTION_BMC_DROP		= 10,
	FUNCTION_UC_DROP		= 11,
	FUNCTION_ALL_TOMCU		= 12,
} _ENUM_FUNCTION_SELECT, *P_ENUM_FUNCTION_SELECT;

typedef enum _ENUM_PF_OPCODE_T {
	PF_OPCODE_ADD = 0,
	PF_OPCODE_DEL,
	PF_OPCODE_ENABLE,
	PF_OPCODE_DISABLE,
	PF_OPCODE_NUM
} ENUM_PF_OPCODE_T;

typedef struct _CMD_PACKET_FILTER_CAP_T {
	UINT_8			ucCmd;
	UINT_16			packet_cap_type;
	UINT_8			aucReserved1[1];
/* GLOBAL */
	UINT_32			PFType;
	UINT_32			FunctionSelect;
	UINT_32			Enable;
/* MAGIC */
	UINT_8			ucBssid;
	UINT_16			usEnableBits;
	UINT_8			aucReserved5[1];
/* DTIM */
	UINT_8			DtimEnable;
	UINT_8			DtimValue;
	UINT_8			aucReserved2[2];
/* BITMAP_PATTERN_T */
	UINT_32			Index;
	UINT_32			Offset;
	UINT_32			FeatureBits;
	UINT_32			Resv;
	UINT_32			PatternLength;
	UINT_32			Mask[4];
	UINT_32			Pattern[32];
/* COALESCE */
	UINT_32			FilterID;
	UINT_32			PacketType;
	UINT_32			CoalesceOP;
	UINT_8			FieldLength;
	UINT_8			CompareOP;
	UINT_8			FieldID;
	UINT_8			aucReserved3[1];
	UINT_32			Pattern3[4];
/* TCPSYN */
	UINT_32			AddressType;
	UINT_32			TCPSrcPort;
	UINT_32			TCPDstPort;
	UINT_32			SourceIP[4];
	UINT_32			DstIP[4];
	UINT_8			aucReserved4[64];
} CMD_PACKET_FILTER_CAP_T, *P_CMD_PACKET_FILTER_CAP_T;
#endif /*CFG_WOW_SUPPORT*/

#if CFG_SUPPORT_WIFI_HOST_OFFLOAD
typedef struct _CMD_TCP_GENERATOR {
	ENUM_PF_OPCODE_T eOpcode;
	UINT_32 u4ReplyId;
	UINT_32 u4Period;
	UINT_32 u4Timeout;
	UINT_32 u4IpId;
	UINT_32 u4DestPort;
	UINT_32 u4SrcPort;
	UINT_32 u4Seq;
	UINT_8 aucDestIp[4];
	UINT_8 aucSrcIp[4];
	UINT_8 aucDestMac[6];
	UINT_8 ucBssId;
	UINT_8 aucReserved1[1];
	UINT_8 aucReserved2[64];
} CMD_TCP_GENERATOR, *P_CMD_TCP_GENERATOR;

typedef struct _CMD_PATTERN_GENERATOR {
	ENUM_PF_OPCODE_T eOpcode;
	UINT_32 u4ReplyId;
	UINT_32 u4EthernetLength;
	UINT_32 u4Period;
	UINT_8 aucEthernetFrame[128];
	UINT_8 ucBssId;
	UINT_8 aucReserved1[3];
	UINT_8 aucReserved2[64];
} CMD_PATTERN_GENERATOR, *P_CMD_PATTERN_GENERATOR;

typedef struct _CMD_BITMAP_FILTER {
	ENUM_PF_OPCODE_T eOpcode;
	UINT_32 u4ReplyId;
	UINT_32 u4Offset;
	UINT_32 u4Length;
	UINT_8 aucPattern[64];
	UINT_8 aucBitMask[64];
	BOOLEAN fgIsEqual;
	BOOLEAN fgIsAccept;
	UINT_8 ucBssId;
	UINT_8 aucReserved1[1];
	UINT_8 aucReserved2[64];
} CMD_BITMAP_FILTER, *P_CMD_BITMAP_FILTER;

#endif /*CFG_SUPPORT_WIFI_HOST_OFFLOAD*/

typedef struct _CMD_RX_PACKET_FILTER {
	UINT_32 u4RxPacketFilter;
	UINT_8 aucReserved[64];
} CMD_RX_PACKET_FILTER, *P_CMD_RX_PACKET_FILTER;


#if defined(MT6632)
#define S2D_INDEX_CMD_H2N      0x0
#define S2D_INDEX_CMD_C2N      0x1
#define S2D_INDEX_CMD_H2C      0x2
#define S2D_INDEX_CMD_H2N_H2C  0x3

#define S2D_INDEX_EVENT_N2H     0x0
#define S2D_INDEX_EVENT_N2C     0x1
#define S2D_INDEX_EVENT_C2H     0x2
#define S2D_INDEX_EVENT_N2H_N2C 0x3
#endif

#define EXT_EVENT_ID_CMD_RESULT    0x00

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
#define EXT_EVENT_ID_CMD_EFUSE_ACCESS   0x1
#define EXT_EVENT_ID_EFUSE_FREE_BLOCK  0x4D
#define EXT_EVENT_ID_GET_TX_POWER       0x1C
#define EXT_EVENT_TARGET_TX_POWER  0x1

/*#endif*/
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#ifndef LINUX
typedef UINT_8 CMD_STATUS;
#endif
/* for Event Packet (via HIF-RX) */
typedef struct _PSE_CMD_HDR_T {
	/* DW0 */
	UINT_16 u2TxByteCount;
	UINT_16 u2Reserved1:10;
	UINT_16 u2Qidx:5;
	UINT_16 u2Pidx:1;

	/* DW1 */
	UINT_16 u2Reserved2:13;
	UINT_16 u2Hf:2;
	UINT_16 u2Ft:1;
	UINT_16 u2Reserved3:8;
	UINT_16 u2PktFt:2;
	UINT_16 u2Reserved4:6;

	/* DW2~7 */
	UINT_32 au4Reserved[6];
} PSE_CMD_HDR_T, *P_PSE_CMD_HDR_T;

typedef struct _WIFI_CMD_T {
	UINT_16 u2TxByteCount;	/* Max value is over 2048 */
	UINT_16 u2PQ_ID;	/* Must be 0x8000 (Port1, Queue 0) */
#if 1
	UINT_8 ucWlanIdx;
	UINT_8 ucHeaderFormat;
	UINT_8 ucHeaderPadding;
	UINT_8 ucPktFt:2;
	UINT_8 ucOwnMAC:6;
	UINT_32 au4Reserved1[6];

	UINT_16 u2Length;
	UINT_16 u2PqId;
#endif
	UINT_8 ucCID;
	UINT_8 ucPktTypeID;	/* Must be 0x20 (CMD Packet) */
	UINT_8 ucSetQuery;
	UINT_8 ucSeqNum;
#if 1
	UINT_8 ucD2B0Rev;	/* padding fields, hw may auto modify this field */
	UINT_8 ucExtenCID;	/* Extend CID */
	UINT_8 ucS2DIndex;	/* Index for Src to Dst in CMD usage */
	UINT_8 ucExtCmdOption;	/* Extend CID option */

	UINT_8 ucCmdVersion;
	UINT_8 ucReserved2[3];
	UINT_32 au4Reserved3[4];	/* padding fields */
#endif
	UINT_8 aucBuffer[0];
} WIFI_CMD_T, *P_WIFI_CMD_T;

/* for Command Packet (via HIF-TX) */
    /* following CM's documentation v0.7 */
typedef struct _WIFI_EVENT_T {
#if 1
	UINT_32 au4HwMacRxDesc[4];
#endif
	UINT_16 u2PacketLength;
	UINT_16 u2PacketType;	/* Must be filled with 0xE000 (EVENT Packet) */
	UINT_8 ucEID;
	UINT_8 ucSeqNum;
	UINT_8 ucEventVersion;
	UINT_8 aucReserved[1];

	UINT_8 ucExtenEID;
	UINT_8 aucReserved2[2];
	UINT_8 ucS2DIndex;

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
	UINT_8 ucMgmtProtection;
	UINT_8 aucKeyMaterial[32];
	UINT_8 aucKeyRsc[16];
} CMD_802_11_KEY, *P_CMD_802_11_KEY;

/* CMD_ID_DEFAULT_KEY_ID */
typedef struct _CMD_DEFAULT_KEY {
	UINT_8 ucBssIdx;
	UINT_8 ucKeyId;
	UINT_8 ucWlanIndex;
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

typedef struct _CMD_CSUM_OFFLOAD_T {
	UINT_16 u2RxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
	UINT_16 u2TxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
} CMD_CSUM_OFFLOAD_T, *P_CMD_CSUM_OFFLOAD_T;

/* CMD_BASIC_CONFIG */
typedef struct _CMD_BASIC_CONFIG_T {
	UINT_8 ucNative80211;
	UINT_8 ucCtrlFlagAssertPath;
	UINT_8 ucCtrlFlagDebugLevel;
	UINT_8 aucReserved[1];
	CMD_CSUM_OFFLOAD_T rCsumOffload;
	UINT_8	ucCrlFlagSegememt;
	UINT_8	aucReserved2[3];
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
	UINT_32  u4NoaDurationMs;
	UINT_32  u4NoaIntervalMs;
	UINT_32  u4NoaCount;
	UINT_8	 ucBssIdx;
	UINT_8   aucReserved[3];
} CMD_CUSTOM_NOA_PARAM_STRUCT_T, *P_CMD_CUSTOM_NOA_PARAM_STRUCT_T;

typedef struct _CMD_CUSTOM_OPPPS_PARAM_STRUCT_T {
	UINT_32 u4CTwindowMs;
	UINT_8	 ucBssIdx;
	UINT_8   aucReserved[3];
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
	UINT_8 ucHwNotSupportAC;

	UINT_8 ucRfVersion;
	UINT_8 ucPhyVersion;
	UINT_8 ucRfCalFail;
	UINT_8 ucBbCalFail;
	UINT_8 aucDateCode[16];
	UINT_32 u4FeatureFlag0;
	UINT_32 u4FeatureFlag1;
	UINT_32 u4CompileFlag0;
	UINT_32 u4CompileFlag1;
	UINT_8 aucBranchInfo[4];
	UINT_8 ucFwBuildNumber;
	UINT_8 ucHwSetNss1x1;
	UINT_8 ucHwNotSupportDBDC;
	UINT_8 ucHwWiFiZeroOnly;
	UINT_8 aucReserved1[56];
} EVENT_NIC_CAPABILITY_T, *P_EVENT_NIC_CAPABILITY_T;

typedef struct _EVENT_NIC_CAPABILITY_V2_T {
	UINT_16 u2TotalElementNum;
	UINT_8 aucReserved[2];
	UINT_8 aucBuffer[0];
} EVENT_NIC_CAPABILITY_V2_T, *P_EVENT_NIC_CAPABILITY_V2_T;

typedef struct _NIC_CAPABILITY_V2_ELEMENT {
	UINT_32 tag_type; /* NIC_CAPABILITY_V2_TAG_T */
	UINT_32 body_len;
	UINT_8 aucbody[0];
} NIC_CAPABILITY_V2_ELEMENT, *P_NIC_CAPABILITY_V2_ELEMENT;

typedef WLAN_STATUS(*NIC_CAP_V2_ELEMENT_HDLR)(P_ADAPTER_T prAdapter, P_UINT_8 buff);
typedef struct _NIC_CAPABILITY_V2_REF_TABLE_T {
	UINT_32 tag_type; /* NIC_CAPABILITY_V2_TAG_T */
	NIC_CAP_V2_ELEMENT_HDLR hdlr;
} NIC_CAPABILITY_V2_REF_TABLE_T, *P_NIC_CAPABILITY_V2_REF_TABLE_T;

typedef enum _NIC_CAPABILITY_V2_TAG_T {
	TAG_CAP_TX_RESOURCE = 0x0,
	TAG_CAP_TX_EFUSEADDRESS = 0x1,
	TAG_CAP_COEX_FEATURE = 0x2,
	TAG_CAP_SINGLE_SKU = 0x3,
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	TAG_CAP_CSUM_OFFLOAD = 0x4,
#endif
	TAG_CAP_R_MODE_CAP = 0xf,
	TAG_CAP_MAC_EFUSE_OFFSET = 0x14,
	TAG_CAP_TOTAL
} NIC_CAPABILITY_V2_TAG_T;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
typedef struct _NIC_CSUM_OFFLOAD_T {
	UINT_8 ucIsSupportCsumOffload;  /* 1: Support, 0: Not Support */
	UINT_8 acReseved[3];
} NIC_CSUM_OFFLOAD_T, *P_NIC_CSUM_OFFLOAD_T;
#endif

typedef struct _NIC_COEX_FEATURE_T {
	UINT_32 u4FddMode;  /* TRUE for COEX FDD mode */
} NIC_COEX_FEATURE_T, *P_NIC_COEX_FEATURE_T;

typedef struct _NIC_EFUSE_ADDRESS_T {
	UINT_32 u4EfuseStartAddress;  /* Efuse Start Address */
	UINT_32 u4EfuseEndAddress;   /* Efuse End Address */
} NIC_EFUSE_ADDRESS_T, *P_NIC_EFUSE_ADDRESS_T;

struct _NIC_EFUSE_OFFSET_T {
	UINT_32 u4TotalItem;	/* Efuse offset items */
	UINT_32 u4WlanMacAddr;  /* Efuse Offset 1 */
};


struct _CAP_R_MODE_CAP_T {
	UINT_8 ucRModeOnlyFlag;     /* 1: R mode only, 0:not R mode only */
	UINT_8 ucRModeReserve[7];   /* reserve fields for future use */
};

typedef struct _NIC_TX_RESOURCE_T {
	UINT_32 u4McuTotalResource;  /* the total usable resource for MCU port */
	UINT_32 u4McuResourceUnit;   /* the unit of a MCU resource */
	UINT_32 u4LmacTotalResource; /* the total usable resource for LMAC port */
	UINT_32 u4LmacResourceUnit;  /* the unit of a LMAC resource */
} NIC_TX_RESOURCE_T, *P_NIC_TX_RESOURCE_T;

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

#define COEX_CTRL_BUF_LEN 460
#define COEX_INFO_LEN 115

/* CMD_COEX_CTRL & EVENT_COEX_CTRL */
/************************************************/
/*  UINT_32 u4SubCmd : Coex Ctrl Sub Command    */
/*  UINT_8 aucBuffer : Reserve for Sub Command  */
/*                    Data Structure            */
/************************************************/
struct CMD_COEX_CTRL {
	UINT_32 u4SubCmd;
	UINT_8  aucBuffer[COEX_CTRL_BUF_LEN];
};

/* Sub Command Data Structure */
/************************************************/
/*  UINT_32 u4IsoPath : WF Path (WF0/WF1)       */
/*  UINT_32 u4Channel : WF Channel              */
/*  UINT_32 u4Band    : WF Band (Band0/Band1)(Not used now)   */
/*  UINT_32 u4Isolation  : Isolation value      */
/************************************************/
struct CMD_COEX_ISO_DETECT {
	UINT_32 u4IsoPath;
	UINT_32 u4Channel;
	/*UINT_32 u4Band;*/
	UINT_32 u4Isolation;
};


/************************************************/
/*  PCHAR   pucCoexInfo : CoexInfoTag           */
/************************************************/
struct CMD_COEX_GET_INFO {
	UINT_32   u4CoexInfo[COEX_INFO_LEN];
};

/* Use for Coex Ctrl Cmd */
enum ENUM_COEX_CTRL_CMD {
	ENUM_COEX_CTRL_ISO_DETECT = 1,
	ENUM_COEX_CTRL_GET_INFO = 2,
	ENUM_COEX_CTRL_NUM
};

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
/* CMD_ID_CAL_BACKUP_IN_HOST_V2 & EVENT_ID_CAL_BACKUP_IN_HOST_V2 */
typedef struct _CMD_CAL_BACKUP_STRUCT_V2_T {
	UINT_8	ucReason;
	UINT_8	ucAction;
	UINT_8	ucNeedResp;
	UINT_8	ucFragNum;
	UINT_8	ucRomRam;
	UINT_32	u4ThermalValue;
	UINT_32 u4Address;
	UINT_32	u4Length;
	UINT_32	u4RemainLength;
	UINT_32	au4Buffer[PARAM_CAL_DATA_DUMP_MAX_NUM];
} CMD_CAL_BACKUP_STRUCT_V2_T, *P_CMD_CAL_BACKUP_STRUCT_V2_T;

typedef struct _CMD_CAL_BACKUP_STRUCT_T {
	UINT_8	ucReason;
	UINT_8	ucAction;
	UINT_8	ucNeedResp;
	UINT_8	ucFragNum;
	UINT_8	ucRomRam;
	UINT_32	u4ThermalValue;
	UINT_32 u4Address;
	UINT_32	u4Length;
	UINT_32	u4RemainLength;
} CMD_CAL_BACKUP_STRUCT_T, *P_CMD_CAL_BACKUP_STRUCT_T;
#endif

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
typedef struct _CMD_ACCESS_CHN_LOAD {
	UINT_32 u4Address;
	UINT_32 u4Data;
	UINT_16 u2Channel;
	UINT_8 aucReserved[2];
} CMD_ACCESS_CHN_LOAD, *P_ACCESS_CHN_LOAD;

typedef struct _CMD_GET_LTE_SAFE_CHN_T {
	UINT_8 ucIndex;
	UINT_8 ucFlags;
	UINT_8 aucReserved0[2];
	UINT_8 aucReserved2[16];
} CMD_GET_LTE_SAFE_CHN_T, *P_CMD_GET_LTE_SAFE_CHN_T;
#endif

/* CMD_DUMP_MEMORY */
typedef struct _CMD_DUMP_MEM {
	UINT_32 u4Address;
	UINT_32 u4Length;
	UINT_32 u4RemainLength;
#if CFG_SUPPORT_QA_TOOL
	UINT_32 u4IcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
	UINT_8 ucFragNum;
} CMD_DUMP_MEM, *P_CMD_DUMP_MEM;

typedef struct _EVENT_DUMP_MEM_T {
	UINT_32 u4Address;
	UINT_32 u4Length;
	UINT_32 u4RemainLength;
#if CFG_SUPPORT_QA_TOOL
	UINT_32 eIcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
	UINT_8 ucFragNum;
	UINT_8 aucBuffer[1];
} EVENT_DUMP_MEM_T, *P_EVENT_DUMP_MEM_T;

#if CFG_SUPPORT_QA_TOOL
typedef struct _CMD_ACCESS_RX_STAT {
	UINT_32 u4SeqNum;
	UINT_32 u4TotalNum;
} CMD_ACCESS_RX_STAT, *P_CMD_ACCESS_RX_STAT;

typedef struct _EVENT_ACCESS_RX_STAT {
	UINT_32 u4SeqNum;
	UINT_32 u4TotalNum;
	UINT_32 au4Buffer[1];
} EVENT_ACCESS_RX_STAT, *P_EVENT_ACCESS_RX_STAT;

#if CFG_SUPPORT_TX_BF
typedef union _CMD_TXBF_ACTION_T {
	PROFILE_TAG_READ_T rProfileTagRead;
	PROFILE_TAG_WRITE_T rProfileTagWrite;
	PROFILE_DATA_READ_T rProfileDataRead;
	PROFILE_DATA_WRITE_T rProfileDataWrite;
	PROFILE_PN_READ_T rProfilePnRead;
	PROFILE_PN_WRITE_T rProfilePnWrite;
	TX_BF_SOUNDING_START_T rTxBfSoundingStart;
	TX_BF_SOUNDING_STOP_T rTxBfSoundingStop;
	TX_BF_TX_APPLY_T rTxBfTxApply;
	TX_BF_PFMU_MEM_ALLOC_T rTxBfPfmuMemAlloc;
	TX_BF_PFMU_MEM_RLS_T rTxBfPfmuMemRls;
} CMD_TXBF_ACTION_T, *P_CMD_TXBF_ACTION_T;

#define CMD_DEVINFO_UPDATE_HDR_SIZE 8
typedef struct _CMD_DEV_INFO_UPDATE_T {
	UINT_8 ucOwnMacIdx;
	UINT_8 ucReserve;
	UINT_16 u2TotalElementNum;
	UINT_8 ucAppendCmdTLV;
	UINT_8 aucReserve[3];
	UINT_8 aucBuffer[0];
	/* CMD_DEVINFO_ACTIVE_T rCmdDevInfoActive; */
} CMD_DEV_INFO_UPDATE_T, *P_CMD_DEV_INFO_UPDATE_T;

#define CMD_BSSINFO_UPDATE_HDR_SIZE 8
typedef struct _CMD_BSS_INFO_UPDATE_T {
	UINT_8 ucBssIndex;
	UINT_8 ucReserve;
	UINT_16 u2TotalElementNum;
	UINT_32 u4Reserve;
	/* CMD_BSSINFO_BASIC_T rCmdBssInfoBasic; */
	UINT_8 aucBuffer[0];
} CMD_BSS_INFO_UPDATE_T, *P_CMD_BSS_INFO_UPDATE_T;

/*  STA record command */
#define CMD_STAREC_UPDATE_HDR_SIZE 8
typedef struct _CMD_STAREC_UPDATE_T {
	UINT_8 ucBssIndex;
	UINT_8 ucWlanIdx;
	UINT_16 u2TotalElementNum;
	UINT_32 u4Reserve;
	UINT_8 aucBuffer[0];
} CMD_STAREC_UPDATE_T, *P_CMD_STAREC_UPDATE_T;

typedef struct _EVENT_PFMU_TAG_READ_T {
	PFMU_PROFILE_TAG1 ru4TxBfPFMUTag1;
	PFMU_PROFILE_TAG2 ru4TxBfPFMUTag2;
} EVENT_PFMU_TAG_READ_T, *P_EVENT_PFMU_TAG_READ_T;

#if CFG_SUPPORT_MU_MIMO
typedef struct _EVENT_HQA_GET_QD {
	UINT_32 u4EventId;
	UINT_32 au4RawData[14];
} EVENT_HQA_GET_QD, *P_EVENT_HQA_GET_QD;

typedef struct _EVENT_HQA_GET_MU_CALC_LQ {
	UINT_32 u4EventId;
	MU_STRUCT_LQ_REPORT rEntry;
} EVENT_HQA_GET_MU_CALC_LQ, *P_EVENT_HQA_GET_MU_CALC_LQ;

typedef struct _EVENT_SHOW_GROUP_TBL_ENTRY {
	UINT_32 u4EventId;
	UINT_8 index;
	UINT_8 numUser:2;
	UINT_8 BW:2;
	UINT_8 NS0:2;
	UINT_8 NS1:2;
	/* UINT_8       NS2:1; */
	/* UINT_8       NS3:1; */
	UINT_8 PFIDUser0;
	UINT_8 PFIDUser1;
	/* UINT_8       PFIDUser2; */
	/* UINT_8       PFIDUser3; */
	BOOLEAN fgIsShortGI;
	BOOLEAN fgIsUsed;
	BOOLEAN fgIsDisable;
	UINT_8 initMcsUser0:4;
	UINT_8 initMcsUser1:4;
	/* UINT_8       initMcsUser2:4; */
	/* UINT_8       initMcsUser3:4; */
	UINT_8 dMcsUser0:4;
	UINT_8 dMcsUser1:4;
	/* UINT_8       dMcsUser2:4; */
	/* UINT_8       dMcsUser3:4; */
} EVENT_SHOW_GROUP_TBL_ENTRY, *P_EVENT_SHOW_GROUP_TBL_ENTRY;

typedef union _CMD_MUMIMO_ACTION_T {
	UINT_8 ucMuMimoCategory;
	UINT_8 aucRsv[3];
	union {
		MU_GET_CALC_INIT_MCS_T rMuGetCalcInitMcs;
		MU_SET_INIT_MCS_T rMuSetInitMcs;
		MU_SET_CALC_LQ_T rMuSetCalcLq;
		MU_GET_LQ_T rMuGetLq;
		MU_SET_SNR_OFFSET_T rMuSetSnrOffset;
		MU_SET_ZERO_NSS_T rMuSetZeroNss;
		MU_SPEED_UP_LQ_T rMuSpeedUpLq;
		MU_SET_MU_TABLE_T rMuSetMuTable;
		MU_SET_GROUP_T rMuSetGroup;
		MU_GET_QD_T rMuGetQd;
		MU_SET_ENABLE_T rMuSetEnable;
		MU_SET_GID_UP_T rMuSetGidUp;
		MU_TRIGGER_MU_TX_T rMuTriggerMuTx;
	} unMuMimoParam;
} CMD_MUMIMO_ACTION_T, *P_CMD_MUMIMO_ACTION_T;
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

typedef struct _CMD_SW_DBG_CTRL_T {
	UINT_32 u4Id;
	UINT_32 u4Data;
	/* Debug Support */
	UINT_32 u4DebugCnt[64];
} CMD_SW_DBG_CTRL_T, *P_CMD_SW_DBG_CTRL_T;

typedef struct _CMD_FW_LOG_2_HOST_CTRL_T {
	UINT_8 ucFwLog2HostCtrl;
	UINT_8 ucMcuDest;
#if     CFG_SUPPORT_FW_DBG_LEVEL_CTRL
	UINT_8 ucFwLogLevel;
	UINT_8 ucReserve;
#else
	UINT_8 ucReserve[2];
#endif
	UINT_32 u4HostTimeSec;
	UINT_32 u4HostTimeMSec;
} CMD_FW_LOG_2_HOST_CTRL_T, *P_CMD_FW_LOG_2_HOST_CTRL_T;

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

/* CMD_ID_KEEP_FULL_PWR */
struct CMD_KEEP_FULL_PWR_T {
	UINT_8 ucEnable;
	UINT_8 aucReserved[3];
};

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

#if (CFG_SUPPORT_DFS_MASTER == 1)
typedef enum _ENUM_REG_DOMAIN_T {
	REG_DEFAULT = 0,
	REG_JP_53,
	REG_JP_56
} ENUM_REG_DOMAIN_T, *P_ENUM_REG_DOMAIN_T;

typedef struct _CMD_RDD_ON_OFF_CTRL_T {
	UINT_8 ucDfsCtrl;
	UINT_8 ucRddIdx;
	UINT_8 ucRddInSel;
	UINT_8 ucRegDomain;
	UINT_8 ucRadarDetectMode;
} CMD_RDD_ON_OFF_CTRL_T, *P_CMD_RDD_ON_OFF_CTRL_T;
#endif

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
#if (CFG_SUPPORT_SINGLE_SKU == 1)
typedef struct _CMD_SET_DOMAIN_INFO_V2_T {
	UINT_32 u4CountryCode;

	UINT_8 uc2G4Bandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	UINT_8 uc5GBandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	UINT_8 aucReserved[2];
	struct acctive_channel_list active_chs;
} CMD_SET_DOMAIN_INFO_V2_T, *P_CMD_SET_DOMAIN_INFO_V2_T;
#endif

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

#if (CFG_SUPPORT_SINGLE_SKU == 1)
typedef struct _CMD_CHANNEL_POWER_LIMIT_V2 {
	UINT_8 ucCentralCh;
	UINT_8 ucReserved[3];

	UINT_8 tx_pwr_dsss_cck;
	UINT_8 tx_pwr_dsss_bpsk;

	UINT_8 tx_pwr_ofdm_bpsk; /* 6M, 9M */
	UINT_8 tx_pwr_ofdm_qpsk; /* 12M, 18M */
	UINT_8 tx_pwr_ofdm_16qam; /* 24M, 36M */
	UINT_8 tx_pwr_ofdm_48m;
	UINT_8 tx_pwr_ofdm_54m;

	UINT_8 tx_pwr_ht20_bpsk; /* MCS0*/
	UINT_8 tx_pwr_ht20_qpsk; /* MCS1, MCS2*/
	UINT_8 tx_pwr_ht20_16qam; /* MCS3, MCS4*/
	UINT_8 tx_pwr_ht20_mcs5; /* MCS5*/
	UINT_8 tx_pwr_ht20_mcs6; /* MCS6*/
	UINT_8 tx_pwr_ht20_mcs7; /* MCS7*/

	UINT_8 tx_pwr_ht40_bpsk; /* MCS0*/
	UINT_8 tx_pwr_ht40_qpsk; /* MCS1, MCS2*/
	UINT_8 tx_pwr_ht40_16qam; /* MCS3, MCS4*/
	UINT_8 tx_pwr_ht40_mcs5; /* MCS5*/
	UINT_8 tx_pwr_ht40_mcs6; /* MCS6*/
	UINT_8 tx_pwr_ht40_mcs7; /* MCS7*/
	UINT_8 tx_pwr_ht40_mcs32; /* MCS32*/

	UINT_8 tx_pwr_vht20_bpsk; /* MCS0*/
	UINT_8 tx_pwr_vht20_qpsk; /* MCS1, MCS2*/
	UINT_8 tx_pwr_vht20_16qam; /* MCS3, MCS4*/
	UINT_8 tx_pwr_vht20_64qam; /* MCS5, MCS6*/
	UINT_8 tx_pwr_vht20_mcs7;
	UINT_8 tx_pwr_vht20_mcs8;
	UINT_8 tx_pwr_vht20_mcs9;

	UINT_8 tx_pwr_vht_40;
	UINT_8 tx_pwr_vht_80;
	UINT_8 tx_pwr_vht_160c;
	UINT_8 tx_pwr_vht_160nc;
	UINT_8 tx_pwr_lg_40;
	UINT_8 tx_pwr_lg_80;

	UINT_8 tx_pwr_1ss_delta;
	UINT_8 ucReserved_1[2];
} CMD_CHANNEL_POWER_LIMIT_V2, *P_CMD_CHANNEL_POWER_LIMIT_V2;

typedef struct _CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T {
	UINT_8 ucNum;
	UINT_8 eband; /*ENUM_BAND_T*/
	UINT_8 usReserved[2];
	UINT_32 countryCode;
	CMD_CHANNEL_POWER_LIMIT_V2 rChannelPowerLimit[0];
} CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T, *P_CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2_T;

#define TX_PWR_LIMIT_SECTION_NUM 5
#define TX_PWR_LIMIT_ELEMENT_NUM 7
#define TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN 4
#define TX_PWR_LIMIT_MAX_VAL 63



struct CHANNEL_TX_PWR_LIMIT {
	UINT_8 ucChannel;
	INT_8 rTxPwrLimitValue[TX_PWR_LIMIT_SECTION_NUM][TX_PWR_LIMIT_ELEMENT_NUM];
};

struct TX_PWR_LIMIT_DATA {
	UINT_32 countryCode;
	UINT_32 ucChNum;
	struct CHANNEL_TX_PWR_LIMIT *rChannelTxPwrLimit;
};

#endif

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

typedef struct _EVENT_TX_DONE_T {
	UINT_8 ucPacketSeq;
	UINT_8 ucStatus;
	UINT_16 u2SequenceNumber;

	UINT_8 ucWlanIndex;
	UINT_8 ucTxCount;
	UINT_16 u2TxRate;

	UINT_8 ucFlag;
	UINT_8 ucTid;
	UINT_8 ucRspRate;
	UINT_8 ucRateTableIdx;

	UINT_8 ucBandwidth;
	UINT_8 ucTxPower;
	UINT_8 aucReserved0[2];

	UINT_32 u4TxDelay;
	UINT_32 u4Timestamp;
	UINT_32 u4AppliedFlag;

	UINT_8 aucRawTxS[28];

	UINT_8 aucReserved1[32];
} EVENT_TX_DONE_T, *P_EVENT_TX_DONE_T;

typedef enum _ENUM_TXS_APPLIED_FLAG_T {
	TX_FRAME_IN_AMPDU_FORMAT = 0,
	TX_FRAME_EXP_BF,
	TX_FRAME_IMP_BF,
	TX_FRAME_PS_BIT
} ENUM_TXS_APPLIED_FLAG_T, *P_ENUM_TXS_APPLIED_FLAG_T;

typedef enum _ENUM_TXS_CONTROL_FLAG_T {
	TXS_WITH_ADVANCED_INFO = 0,
	TXS_IS_EXIST
} ENUM_TXS_CONTROL_FLAG_T, *P_ENUM_TXS_CONTROL_FLAG_T;

#if (CFG_SUPPORT_DFS_MASTER == 1)
typedef enum _ENUM_DFS_CTRL_T {
	RDD_STOP = 0,
	RDD_START,
	RDD_DET_MODE,
	RDD_RADAR_EMULATE,
	RDD_START_TXQ
} ENUM_DFS_CTRL_T, *P_ENUM_DFS_CTRL_T;
#endif

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
	UINT_8 ucNss;
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
	UINT_8 ucDBDCBand;
	UINT_8 ucWmmSet;
	UINT_8  ucDBDCAction;
	UINT_8  ucNss;
	UINT_8 aucReserved[20];
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
	UINT_8 ucVhtOpMode; /* VHT operating mode, bit 7: Rx NSS Type, bit 4-6, Rx NSS, bit 0-1: Channel Width */

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
	UINT_8 aucReserved3[2];

	TXBF_PFMU_STA_INFO rTxBfPfmuInfo;

	UINT_8 ucTxAmsduInAmpdu;
	UINT_8 ucRxAmsduInAmpdu;
	UINT_8 aucReserved5[2];

	UINT_32 u4TxMaxAmsduInAmpduLen;
	/* UINT_8 aucReserved4[30]; */
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
	UINT_8 aucReserved[2];					/*total 8*/
	PARAM_SSID_T arSSID[4];					/*(4+32)*4 = 144, total 152*/
	UINT_16 u2ProbeDelayTime;
	UINT_16 u2ChannelDwellTime;
	UINT_16 u2TimeoutValue;
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;				/*total 160*/
	CHANNEL_INFO_T arChannelList[32];		/*total 160+64=224*/
	UINT_16 u2IELen;						/*total 226*/
	UINT_8 aucIE[MAX_IE_LENGTH];			/*total 826*/
	UINT_8 ucScnCtrlFlag;
	UINT_8 aucReserved2;					/*total 828*/
	/*Extend for Scan cmds*/
	CHANNEL_INFO_T arChannelListExtend[32];	/*total 892*/
	UINT_8 arPerChannelControl[32];
	UINT_8 arPerExtendChannelControl[32];	/*total 956*/
	UINT_8 ucScanChannelListenTime;			/*total 957*/
	UINT_8 aucReserved3[3];					/*total 960, max 1024*/
} CMD_SCAN_REQ_V2, *P_CMD_SCAN_REQ_V2;

typedef struct _CMD_SCAN_CANCEL_T {
	UINT_8 ucSeqNum;
	UINT_8 ucIsExtChannel;	/* For P2P channel extension. */
	UINT_8 aucReserved[2];
} CMD_SCAN_CANCEL, *P_CMD_SCAN_CANCEL;

/* 20150107  Daniel Added complete channels number in the scan done event */
/* before*/
/*
*typedef struct _EVENT_SCAN_DONE_T {
*	UINT_8          ucSeqNum;
*	UINT_8          ucSparseChannelValid;
*	CHANNEL_INFO_T  rSparseChannel;
*} EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;
*/
/* after */

#define EVENT_SCAN_DONE_CHANNEL_NUM_MAX 64
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
	/*channel idle count # Mike */
	UINT_8 ucSparseChannelArrayValidNum;
	UINT_8 aucReserved[3];
	UINT_8 aucChannelNum[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];
	/* Idle format for au2ChannelIdleTime */
	/* 0: first bytes: idle time(ms) 2nd byte: dwell time(ms) */
	/* 1: first bytes: idle time(8ms) 2nd byte: dwell time(8ms) */
	/* 2: dwell time (16us) */
	UINT_16 au2ChannelIdleTime[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];
	/* B0: Active/Passive B3-B1: Idle format  */
	UINT_8 aucChannelFlag[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];
	UINT_8 aucChannelMDRDYCnt[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];

} EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;

#if CFG_SUPPORT_BATCH_SCAN
typedef struct _CMD_BATCH_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucCmd;		/* Start/ Stop */
	UINT_8 ucMScan;		/* an integer number of scans per batch */
	UINT_8 ucBestn;		/* an integer number of the max AP to remember per scan */
	UINT_8 ucRtt;		/* an integer number of highest-strength AP for which we'd */
				/* like approximate distance reported */
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
	UINT_8 ucDBDCBand;
	UINT_8 aucReserved;
	UINT_32 u4MaxInterval;	/* In unit of ms */
	UINT_8 aucReserved2[8];
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

typedef struct _CMD_DBDC_SETTING_T {
	UINT_8 ucDbdcEn;
	UINT_8 ucWmmBandBitmap;
	UINT_8 ucUpdateSettingNextChReq;
	UINT_8 aucReserved1;
	UINT_8 aucReserved2[32];
} CMD_DBDC_SETTING_T, *P_CMD_DBDC_SETTING_T;

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
	UINT_8 ucDBDCBand;
	UINT_8 aucReserved;
	UINT_32 u4GrantInterval;	/* In unit of ms */
	UINT_8 aucReserved2[8];
} EVENT_CH_PRIVILEGE_T, *P_EVENT_CH_PRIVILEGE_T;

#if (CFG_SUPPORT_DFS_MASTER == 1)
typedef struct _LONG_PULSE_BUFFER_T {
	UINT_32 u4LongStartTime;
	UINT_16 u2LongPulseWidth;
} LONG_PULSE_BUFFER_T, *PLONG_PULSE_BUFFER_T;

typedef struct _PERIODIC_PULSE_BUFFER_T {
	UINT_32 u4PeriodicStartTime;
	UINT_16 u2PeriodicPulseWidth;
	INT_16 i2PeriodicPulsePower;
} PERIODIC_PULSE_BUFFER_T, *PPERIODIC_PULSE_BUFFER_T;

typedef struct _EVENT_RDD_REPORT_T {
	UINT_8 ucRadarReportMode; /*0: Only report radar detected;   1:  Add parameter reports*/
	UINT_8 ucRddIdx;
	UINT_8 ucLongDetected;
	UINT_8 ucPeriodicDetected;
	UINT_8 ucLPBNum;
	UINT_8 ucPPBNum;
	UINT_8 ucLPBPeriodValid;
	UINT_8 ucLPBWidthValid;
	UINT_8 ucPRICountM1;
	UINT_8 ucPRICountM1TH;
	UINT_8 ucPRICountM2;
	UINT_8 ucPRICountM2TH;
	UINT_32 u4PRI1stUs;
	LONG_PULSE_BUFFER_T arLpbContent[32];
	PERIODIC_PULSE_BUFFER_T arPpbContent[32];
} EVENT_RDD_REPORT_T, *P_EVENT_RDD_REPORT_T;
#endif

#if (CFG_WOW_SUPPORT == 1)
/* event of wake up reason */
struct _EVENT_WAKEUP_REASON_INFO {
	UINT_8 reason;
	UINT_8 aucReserved[3];
};
#endif

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

#if CFG_SUPPORT_CSI
enum CSI_EVENT_TLV_TAG {
	CSI_EVENT_IS_CCK,
	CSI_EVENT_CBW,
	CSI_EVENT_RSSI,
	CSI_EVENT_SNR,
	CSI_EVENT_BAND,
	CSI_EVENT_CSI_NUM,
	CSI_EVENT_CSI_I_DATA,
	CSI_EVENT_CSI_Q_DATA,
	CSI_EVENT_DBW,
	CSI_EVENT_CH_IDX,
	CSI_EVENT_TA,
	CSI_EVENT_EXTRA_INFO,
	CSI_EVENT_RX_MODE,
	CSI_EVENT_RSVD1,
	CSI_EVENT_RSVD2,
	CSI_EVENT_RSVD3,
	CSI_EVENT_RSVD4,
	CSI_EVENT_TLV_TAG_NUM,
};
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
/* command type */
#define CMD_ADV_CONTROL_SET (1<<15)
#define CMD_PTA_CONFIG_TYPE (0x1)
#define CMD_AFH_CONFIG_TYPE (0x2)
#define CMD_BA_CONFIG_TYPE (0x3)
#define CMD_GET_REPORT_TYPE (0x4)
#define CMD_NOISE_HISTOGRAM_TYPE (0x5)

/* for PtaConfig field */
#define CMD_PTA_CONFIG_PTA_EN (1<<0)
#define CMD_PTA_CONFIG_RW_EN (1<<1)
#define CMD_PTA_CONFIG_PTA_STAT_EN (1<<2)

/* pta config related mask */
#define CMD_PTA_CONFIG_PTA (1<<0)
#define CMD_PTA_CONFIG_RW (1<<1)
#define CMD_PTA_CONFIG_TXDATA_TAG (1<<2)
#define CMD_PTA_CONFIG_RXDATAACK_TAG (1<<3)
#define CMD_PTA_CONFIG_RX_NSW_TAG (1<<4)
#define CMD_PTA_CONFIG_TXACK_TAG (1<<5)
#define CMD_PTA_CONFIG_TXPROTFRAME_TAG (1<<6)
#define CMD_PTA_CONFIG_RXPROTFRAMEACK_TAG (1<<7)
#define CMD_PTA_CONFIG_TX_BMC_TAG (1<<8)
#define CMD_PTA_CONFIG_TX_BCN_TAG (1<<9)
#define CMD_PTA_CONFIG_RX_SP_TAG (1<<10)
#define CMD_PTA_CONFIG_TX_MGMT_TAG (1<<11)
#define CMD_PTA_CONFIG_RXMGMTACK_TAG (1<<12)
#define CMD_PTA_CONFIG_PTA_STAT (1<<13)
#define CMD_PTA_CONFIG_PTA_STAT_RESET (1<<14)

/* for config PTA Tag */
#define EVENT_CONFIG_PTA_OFFSET (0)
#define EVENT_CONFIG_PTA_FEILD (0x1)
#define EVENT_CONFIG_PTA_WIFI_OFFSET (12)
#define EVENT_CONFIG_PTA_WIFI_FEILD (0x1)
#define EVENT_CONFIG_PTA_BT_OFFSET (15)
#define EVENT_CONFIG_PTA_BT_FEILD (0x1)
#define EVENT_CONFIG_PTA_ARB_OFFSET (16)
#define EVENT_CONFIG_PTA_ARB_FEILD (0x1)

#define EVENT_CONFIG_WIFI_GRANT_OFFSET (30)
#define EVENT_CONFIG_WIFI_GRANT_FEILD (0x1)
#define EVENT_CONFIG_WIFI_PRI_OFFSET (12)
#define EVENT_CONFIG_WIFI_PRI_FEILD (0xf)
#define EVENT_CONFIG_WIFI_TXREQ_OFFSET (8)
#define EVENT_CONFIG_WIFI_TXREQ_FEILD (0x1)
#define EVENT_CONFIG_WIFI_RXREQ_OFFSET (9)
#define EVENT_CONFIG_WIFI_RXREQ_FEILD (0x1)

#define EVENT_CONFIG_BT_GRANT_OFFSET (29)
#define EVENT_CONFIG_BT_GRANT_FEILD (0x1)
#define EVENT_CONFIG_BT_PRI_OFFSET (4)
#define EVENT_CONFIG_BT_PRI_FEILD (0xf)
#define EVENT_CONFIG_BT_TXREQ_OFFSET (0)
#define EVENT_CONFIG_BT_TXREQ_FEILD (0x1)
#define EVENT_CONFIG_BT_RXREQ_OFFSET (1)
#define EVENT_CONFIG_BT_RXREQ_FEILD (0x1)

#define EVENT_PTA_BTTRX_CNT_OFFSET (16)
#define EVENT_PTA_BTTRX_CNT_FEILD (0xFFFF)
#define EVENT_PTA_BTTRX_GRANT_CNT_OFFSET (0)
#define EVENT_PTA_BTTRX_GRANT_CNT_FEILD (0xFFFF)

#define EVENT_PTA_WFTRX_CNT_OFFSET (16)
#define EVENT_PTA_WFTRX_CNT_FEILD (0xFFFF)
#define EVENT_PTA_WFTRX_GRANT_CNT_OFFSET (0)
#define EVENT_PTA_WFTRX_GRANT_CNT_FEILD (0xFFFF)

#define EVENT_PTA_TX_ABT_CNT_OFFSET (16)
#define EVENT_PTA_TX_ABT_CNT_FEILD (0xFFFF)
#define EVENT_PTA_RX_ABT_CNT_OFFSET (0)
#define EVENT_PTA_RX_ABT_CNT_FEILD (0xFFFF)
typedef struct _CMD_PTA_CONFIG {
	UINT_16 u2Type;
	UINT_16 u2Len;
	UINT_32 u4ConfigMask;
	/* common usage in set/get */
	UINT_32 u4PtaConfig;
	UINT_32 u4TxDataTag;
	UINT_32 u4RxDataAckTag;
	UINT_32 u4RxNswTag;
	UINT_32 u4TxAckTag;
	UINT_32 u4TxProtFrameTag;
	UINT_32 u4RxProtFrameAckTag;
	UINT_32 u4TxBMCTag;
	UINT_32 u4TxBCNTag;
	UINT_32 u4RxSPTag;
	UINT_32 u4TxMgmtTag;
	UINT_32 u4RxMgmtAckTag;
	/* Only used in get */
	UINT_32 u4PtaWF0TxCnt;
	UINT_32 u4PtaWF0RxCnt;
	UINT_32 u4PtaWF0AbtCnt;
	UINT_32 u4PtaWF1TxCnt;
	UINT_32 u4PtaWF1RxCnt;
	UINT_32 u4PtaWF1AbtCnt;
	UINT_32 u4PtaBTTxCnt;
	UINT_32 u4PtaBTRxCnt;
	UINT_32 u4PtaBTAbtCnt;
	UINT_32 u4GrantStat;
	UINT_32 u4CoexMode;
} CMD_PTA_CONFIG_T, *P_CMD_PTA_CONFIG_T;

/* get report related */
enum _ENUM_GET_REPORT_ACTION_T {
	CMD_GET_REPORT_ENABLE = 1,
	CMD_GET_REPORT_DISABLE,
	CMD_GET_REPORT_RESET,
	CMD_GET_REPORT_GET,
	CMD_SET_REPORT_SAMPLE_DUR,
	CMD_SET_REPORT_SAMPLE_POINT,
	CMD_SET_REPORT_TXTHRES,
	CMD_SET_REPORT_RXTHRES,
	CMD_GET_REPORT_ACTIONS
};
#define EVENT_REPORT_OFDM_FCCA (16)
#define EVENT_REPORT_OFDM_FCCA_FEILD (0xffff)
#define EVENT_REPORT_CCK_FCCA (0)
#define EVENT_REPORT_CCK_FCCA_FEILD (0xffff)
#define EVENT_REPORT_OFDM_SIGERR (16)
#define EVENT_REPORT_OFDM_SIGERR_FEILD (0xffff)
#define EVENT_REPORT_CCK_SIGERR (0)
#define EVENT_REPORT_CCK_SIGERR_FEILD (0xffff)
struct CMD_GET_TRAFFIC_REPORT {
	UINT_16 u2Type;
	UINT_16 u2Len;
	/* parameter */
	UINT_8 ucBand;
	UINT_8 ucAction;
	UINT_8 reserved[2];
	/* report 1 */
	UINT_32 u4FalseCCA;
	UINT_32 u4HdrCRC;
	UINT_32 u4PktSent;
	UINT_32 u4PktRetried;
	UINT_32 u4PktTxfailed;
	UINT_32 u4RxMPDU;
	UINT_32 u4RxFcs;
	/* air time report */
	UINT_32 u4FetchSt; /* ms */
	UINT_32 u4FetchEd; /* ms */
	UINT_32 u4ChBusy; /* us */
	UINT_32 u4ChIdle; /* us */
	UINT_32 u4TxAirTime; /* us */
	UINT_32 u4RxAirTime; /* us */
	UINT_32 u4TimerDur; /* ms */
	UINT_32 u4FetchCost; /* us */
	INT_32 TimerDrift; /* ms */
	INT_16 u2SamplePoints; /* ms */
	INT_8 ucTxThres; /* ms */
	INT_8 ucRxThres; /* ms */
};

typedef struct _CMD_ADV_CONFIG_HEADER {
	UINT_16 u2Type;
	UINT_16 u2Len;
} CMD_ADV_CONFIG_HEADER_T, *P_CMD_ADV_CONFIG_HEADER_T;

/* noise histogram related */
enum _ENUM_NOISE_HISTOGRAM_ACTION_T {
	CMD_NOISE_HISTOGRAM_ENABLE = 1,
	CMD_NOISE_HISTOGRAM_DISABLE,
	CMD_NOISE_HISTOGRAM_RESET,
	CMD_NOISE_HISTOGRAM_GET
};
struct CMD_NOISE_HISTOGRAM_REPORT {
	UINT_16 u2Type;
	UINT_16 u2Len;
	/* parameter */
	UINT_8 ucAction;
	UINT_8 reserved[3];
	/* IPI_report */
	UINT_32 u4IPI0;  /* Power <= -92 */
	UINT_32 u4IPI1;  /* -92 < Power <= -89 */
	UINT_32 u4IPI2;  /* -89 < Power <= -86 */
	UINT_32 u4IPI3;  /* -86 < Power <= -83 */
	UINT_32 u4IPI4;  /* -83 < Power <= -80 */
	UINT_32 u4IPI5;  /* -80 < Power <= -75 */
	UINT_32 u4IPI6;  /* -75 < Power <= -70 */
	UINT_32 u4IPI7;  /* -70 < Power <= -65 */
	UINT_32 u4IPI8;  /* -65 < Power <= -60 */
	UINT_32 u4IPI9;  /* -60 < Power <= -55 */
	UINT_32 u4IPI10; /* -55 < Power  */
};
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
#if CFG_SUPPORT_QA_TOOL
	UINT_32 u4IcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
} EVENT_ICAP_STATUS_T, *P_EVENT_ICAP_STATUS_T;

#if CFG_SUPPORT_QA_TOOL
typedef struct _ADC_BUS_FMT_T {
	UINT_32 u4Dcoc0Q:14;	/* [13:0] */
	UINT_32 u4Dcoc0I:14;	/* [27:14] */
	UINT_32 u4DbgData1:4;	/* [31:28] */

	UINT_32 u4Dcoc1Q:14;	/* [45:32] */
	UINT_32 u4Dcoc1I:14;	/* [46:59] */
	UINT_32 u4DbgData2:4;	/* [63:60] */

	UINT_32 u4DbgData3;	/* [95:64] */
} ADC_BUS_FMT_T, *P_ADC_BUS_FMT_T;

typedef struct _IQC_BUS_FMT_T {
	INT_32 u4Iqc0Q:14;	/* [13:0] */
	INT_32 u4Iqc0I:14;	/* [27:14] */
	INT_32 u4Na1:4;		/* [31:28] */

	INT_32 u4Iqc1Q:14;	/* [45:32] */
	INT_32 u4Iqc1I:14;	/* [59:46] */
	INT_32 u4Na2:4;		/* [63:60] */

	INT_32 u4Na3;		/* [95:64] */
} IQC_BUS_FMT_T, *P_IQC_BUS_FMT_T;

typedef struct _IQC_160_BUS_FMT_T {
	INT_32 u4Iqc0Q1:12;	/* [11:0] */
	INT_32 u4Iqc0I1:12;	/* [23:12] */
	UINT_32 u4Iqc0Q0P1:8;	/* [31:24] */

	INT_32 u4Iqc0Q0P2:4;	/* [35:32] */
	INT_32 u4Iqc0I0:12;	/* [47:36] */
	INT_32 u4Iqc1Q1:12;	/* [59:48] */
	UINT_32 u4Iqc1I1P1:4;	/* [63:60] */

	INT_32 u4Iqc1I1P2:8;	/* [71:64] */
	INT_32 u4Iqc1Q0:12;	/* [83:72] */
	INT_32 u4Iqc1I0:12;	/* [95:84] */
} IQC_160_BUS_FMT_T, *P_IQC_160_BUS_FMT_T;

typedef struct _SPECTRUM_BUS_FMT_T {
	INT_32 u4DcocQ:12;	/* [11:0] */
	INT_32 u4DcocI:12;	/* [23:12] */
	INT_32 u4LpfGainIdx:4;	/* [27:24] */
	INT_32 u4LnaGainIdx:2;	/* [29:28] */
	INT_32 u4AssertData:2;	/* [31:30] */
} SPECTRUM_BUS_FMT_T, *P_SPECTRUM_BUS_FMT_T;

typedef struct _PACKED_ADC_BUS_FMT_T {
	UINT_32 u4AdcQ0T2:4;	/* [19:16] */
	UINT_32 u4AdcQ0T1:4;	/* [11:8] */
	UINT_32 u4AdcQ0T0:4;	/* [3:0] */

	UINT_32 u4AdcI0T2:4;	/* [23:20] */
	UINT_32 u4AdcI0T1:4;	/* [15:12] */
	UINT_32 u4AdcI0T0:4;	/* [7:4] */

	UINT_32 u4AdcQ0T5:4;	/* [43:40] */
	UINT_32 u4AdcQ0T4:4;	/* [35:32] */
	UINT_32 u4AdcQ0T3:4;	/* [27:24] */

	UINT_32 u4AdcI0T5:4;	/* [47:44] */
	UINT_32 u4AdcI0T4:4;	/* [39:36] */
	UINT_32 u4AdcI0T3:4;	/* [31:28] */

	UINT_32 u4AdcQ1T2:4;	/* [19:16] */
	UINT_32 u4AdcQ1T1:4;	/* [11:8] */
	UINT_32 u4AdcQ1T0:4;	/* [3:0] */

	UINT_32 u4AdcI1T2:4;	/* [23:20] */
	UINT_32 u4AdcI1T1:4;	/* [15:12] */
	UINT_32 u4AdcI1T0:4;	/* [7:4] */

	UINT_32 u4AdcQ1T5:4;	/* [43:40] */
	UINT_32 u4AdcQ1T4:4;	/* [35:32] */
	UINT_32 u4AdcQ1T3:4;	/* [27:24] */

	UINT_32 u4AdcI1T5:4;	/* [47:44] */
	UINT_32 u4AdcI1T4:4;	/* [39:36] */
	UINT_32 u4AdcI1T3:4;	/* [31:28] */
} PACKED_ADC_BUS_FMT_T, *P_PACKED_ADC_BUS_FMT_T;

typedef union _ICAP_BUS_FMT {
	ADC_BUS_FMT_T rAdcBusData;	/* 12 bytes */
	IQC_BUS_FMT_T rIqcBusData;	/* 12 bytes */
	IQC_160_BUS_FMT_T rIqc160BusData;	/* 12 bytes */
	SPECTRUM_BUS_FMT_T rSpectrumBusData;	/* 4  bytes */
	PACKED_ADC_BUS_FMT_T rPackedAdcBusData;	/* 12 bytes */
} ICAP_BUS_FMT, *P_ICAP_BUS_FMT;
#endif /* CFG_SUPPORT_QA_TOOL */

typedef struct _CMD_SET_TXPWR_CTRL_T {
	INT_8 c2GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c2GHotspotPwrOffset;
	INT_8 c2GP2pPwrOffset;
	INT_8 c2GBowPwrOffset;
	INT_8 c5GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c5GHotspotPwrOffset;
	INT_8 c5GP2pPwrOffset;
	INT_8 c5GBowPwrOffset;
	/* TX power policy when concurrence
	*  in the same channel
	*  0: Highest power has priority
	*  1: Lowest power has priority
	*/
	UINT_8 ucConcurrencePolicy;
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
	UINT_8 fgStopAfterIndication;
	UINT_8 ucFastScanIteration;
	UINT_16 u2FastScanPeriod;
	UINT_16 u2SlowScanPeriod;
	UINT_8 ucEntryNum;
	UINT_8 ucFlag;		/* BIT(0) Check cipher */
	UINT_16 u2IELen;
	NLO_NETWORK arNetworkList[16];
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

typedef struct _CMD_HIF_CTRL_T {
	UINT_8 ucHifType;
	UINT_8 ucHifDirection;
	UINT_8 ucHifStop;
	UINT_8 aucReserved1;
	UINT_8 aucReserved2[32];
} CMD_HIF_CTRL_T, *P_CMD_HIF_CTRL_T;

typedef enum _ENUM_HIF_TYPE {
	ENUM_HIF_TYPE_SDIO = 0x00,
	ENUM_HIF_TYPE_USB = 0x01,
	ENUM_HIF_TYPE_PCIE = 0x02,
	ENUM_HIF_TYPE_GPIO = 0x03,
} ENUM_HIF_TYPE, *P_ENUM_HIF_TYPE;

typedef enum _ENUM_HIF_DIRECTION {
	ENUM_HIF_TX = 0x01,
	ENUM_HIF_RX = 0x02,
	ENUM_HIF_TRX = 0x03,
} ENUM_HIF_DIRECTION, *P_ENUM_HIF_DIRECTION;

typedef enum _ENUM_HIF_TRAFFIC_STATUS {
	ENUM_HIF_TRAFFIC_BUSY = 0x01,
	ENUM_HIF_TRAFFIC_IDLE = 0x02,
	ENUM_HIF_TRAFFIC_INVALID = 0x3,
} ENUM_HIF_TRAFFIC_STATUS, *P_ENUM_HIF_TRAFFIC_STATUS;

typedef struct _EVENT_HIF_CTRL_T {
	UINT_8 ucHifType;
	UINT_8 ucHifTxTrafficStatus;
	UINT_8 ucHifRxTrafficStatus;
	UINT_8 ucReserved1;
	UINT_8 aucReserved2[32];
} EVENT_HIF_CTRL_T, *P_EVENT_HIF_CTRL_T;

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
	UINT_8 ucResetCounter;
	UINT_8 aucReserved1[1];
	UINT_8 aucReserved2[16];
} CMD_GET_STA_STATISTICS_T, *P_CMD_GET_STA_STATISTICS_T;

/* per access category statistics */
typedef struct _WIFI_WMM_AC_STAT_GET_FROM_FW_T {
	UINT_32 u4TxFailMsdu;
	UINT_32 u4TxRetryMsdu;
} WIFI_WMM_AC_STAT_GET_FROM_FW_T, *P_WIFI_WMM_AC_STAT_GET_FROM_FW_T;

/* CFG_SUPPORT_WFD */
typedef struct _EVENT_STA_STATISTICS_T {
	/* Event header */
	/* UINT_16     u2Length; */
	/* UINT_16     u2Reserved1; *//* Must be filled with 0x0001 (EVENT Packet) */
	/* UINT_8            ucEID; */
	/* UINT_8      ucSeqNum; */
	/* UINT_8            aucReserved2[2]; */

	/* Event Body */
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

	UINT_8 ucTemperature;
	UINT_8 ucSkipAr;
	UINT_8 ucArTableIdx;
	UINT_8 ucRateEntryIdx;
	UINT_8 ucRateEntryIdxPrev;
	UINT_8 ucTxSgiDetectPassCnt;
	UINT_8 ucAvePer;
	UINT_8 aucArRatePer[AR_RATE_TABLE_ENTRY_MAX];
	UINT_8 aucRateEntryIndex[AUTO_RATE_NUM];
	UINT_8 ucArStateCurr;
	UINT_8 ucArStatePrev;
	UINT_8 ucArActionType;
	UINT_8 ucHighestRateCnt;
	UINT_8 ucLowestRateCnt;
	UINT_16 u2TrainUp;
	UINT_16 u2TrainDown;
	UINT_32 u4Rate1TxCnt;
	UINT_32 u4Rate1FailCnt;
	TX_VECTOR_BBP_LATCH_T rTxVector[ENUM_BAND_NUM];
	MIB_INFO_STAT_T rMibInfo[ENUM_BAND_NUM];
	BOOLEAN fgIsForceTxStream;
	BOOLEAN fgIsForceSeOff;
	UINT_8 aucReserved[21];
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

typedef struct _EVENT_WIFI_RDD_TEST_T {
	UINT_32 u4FuncIndex;
	UINT_32 u4FuncLength;
	UINT_32 u4Prefix;
	UINT_32 u4Count;
	UINT_32 u4SubBandRssi0;
	UINT_32 u4SubBandRssi1;
	UINT_8 ucRddIdx;
	UINT_8 aucReserve[3];
	UINT_8 aucBuffer[0];
} EVENT_WIFI_RDD_TEST_T, *P_EVENT_WIFI_RDD_TEST_T;

#if CFG_SUPPORT_MSP
/* EVENT_ID_WLAN_INFO */
typedef struct _EVENT_WLAN_INFO {

	PARAM_TX_CONFIG_T	rWtblTxConfig;
	PARAM_SEC_CONFIG_T	rWtblSecConfig;
	PARAM_KEY_CONFIG_T	rWtblKeyConfig;
	PARAM_PEER_RATE_INFO_T	rWtblRateInfo;
	PARAM_PEER_BA_CONFIG_T	rWtblBaConfig;
	PARAM_PEER_CAP_T	rWtblPeerCap;
	PARAM_PEER_RX_COUNTER_ALL_T rWtblRxCounter;
	PARAM_PEER_TX_COUNTER_ALL_T rWtblTxCounter;
} EVENT_WLAN_INFO, *P_EVENT_WLAN_INFO;

/* EVENT_ID_MIB_INFO */
typedef struct _EVENT_MIB_INFO {
	HW_MIB_COUNTER_T	    rHwMibCnt;
	HW_MIB2_COUNTER_T	    rHwMib2Cnt;
	HW_TX_AMPDU_METRICS_T	    rHwTxAmpduMts;

} EVENT_MIB_INFO, *P_EVENT_MIB_INFO;
#endif

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
struct EVENT_TX_MCS_INFO {
	UINT_16		au2TxRateCode[MCS_INFO_SAMPLE_CNT];
	UINT_8		aucTxRatePer[MCS_INFO_SAMPLE_CNT];
	UINT_8      aucReserved[2];
};
#endif

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
typedef struct _EVENT_ACCESS_EFUSE {

	UINT_32         u4Address;
	UINT_32         u4Valid;
	UINT_8          aucData[16];

} EVENT_ACCESS_EFUSE, *P_EVENT_ACCESS_EFUSE;


typedef struct _EXT_EVENT_EFUSE_FREE_BLOCK_T {

	UINT_16  u2FreeBlockNum;
	UINT_8  aucReserved[2];
} EVENT_EFUSE_FREE_BLOCK_T, *P_EVENT_EFUSE_FREE_BLOCK_T;

typedef struct _EXT_EVENT_GET_TX_POWER_T {

	UINT_8  ucTxPwrType;
	UINT_8  ucEfuseAddr;
	UINT_8  ucTx0TargetPower;
	UINT_8  ucDbdcIdx;

} EVENT_GET_TX_POWER_T, *P_EVENT_GET_TX_POWER_T;

typedef struct _CMD_SUSPEND_MODE_SETTING_T {
	UINT_8 ucBssIndex;
	UINT_8 ucEnableSuspendMode;
	UINT_8 ucMdtim; /* LP parameter */
	UINT_8 ucReserved1[1];
	UINT_8 ucReserved2[64];
} CMD_SUSPEND_MODE_SETTING_T, *P_CMD_SUSPEND_MODE_SETTING_T;

typedef struct _EVENT_UPDATE_COEX_PHYRATE_T {
	UINT_8 ucVersion;
	UINT_8 aucReserved1[3];    /* 4 byte alignment */
	UINT_32 u4Flags;
	UINT_32 au4PhyRateLimit[HW_BSSID_NUM+1];
} EVENT_UPDATE_COEX_PHYRATE_T, *P_EVENT_UPDATE_COEX_PHYRATE_T;

#if CFG_SUPPORT_CSI

#define CSI_INFO_RSVD1 BIT(0)
#define CSI_INFO_RSVD2 BIT(1)

enum CSI_CONTROL_MODE_T {
	CSI_CONTROL_MODE_STOP,
	CSI_CONTROL_MODE_START,
	CSI_CONTROL_MODE_SET,
	CSI_CONTROL_MODE_NUM
};

enum CSI_CONFIG_ITEM_T {
	CSI_CONFIG_RSVD1,
	CSI_CONFIG_WF,
	CSI_CONFIG_RSVD2,
	CSI_CONFIG_FRAME_TYPE,
	CSI_CONFIG_TX_PATH,
	CSI_CONFIG_OUTPUT_FORMAT,
	CSI_CONFIG_INFO,
	CSI_CONFIG_ITEM_NUM
};

struct CMD_CSI_CONTROL_T {
	UINT_8 ucMode;
	UINT_8 ucCfgItem;
	UINT_8 ucValue1;
	UINT_8 ucValue2;
};

enum CSI_OUTPUT_FORMAT_T {
	CSI_OUTPUT_RAW,
	CSI_OUTPUT_TONE_MASKED,
	CSI_OUTPUT_TONE_MASKED_SHIFTED,
	CSI_OUTPUT_FORMAT_NUM
};

#endif


struct CMD_GET_TXPWR_TBL {
	UINT_8 ucDbdcIdx;
	UINT_8 aucReserved[3];
};

struct EVENT_GET_TXPWR_TBL {
	UINT_8 ucCenterCh;
	UINT_8 aucReserved[3];
	struct POWER_LIMIT tx_pwr_tbl[TXPWR_TBL_NUM];
};

struct TLV_ELEMENT {
	UINT_32 tag_type;
	UINT_32 body_len;
	UINT_8 aucbody[0];
};

/*#endif*/

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

/* Nic cmd/event for Coex related */
VOID nicCmdEventQueryCoexIso(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
VOID nicCmdEventQueryCoexGetInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

#if CFG_SUPPORT_QA_TOOL
VOID nicCmdEventQueryRxStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

UINT_32 TsfRawData2IqFmt(P_EVENT_DUMP_MEM_T prEventDumpMem);

INT_32 GetIQData(INT_32 **prIQAry, UINT_32 *prDataLen, UINT_32 u4IQ, UINT_32 u4GetWf1);

#if CFG_SUPPORT_TX_BF
VOID nicCmdEventPfmuDataRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventPfmuTagRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif /* CFG_SUPPORT_TX_BF */
#if CFG_SUPPORT_MU_MIMO
VOID nicCmdEventGetQd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
VOID nicCmdEventGetCalcLq(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
VOID nicCmdEventGetCalcInitMcs(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
VOID nicCmdEventQueryCalBackupV2(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

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

VOID nicCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

VOID nicOidCmdEnterRFTestTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

#if CFG_SUPPORT_BUILD_DATE_CODE
VOID nicCmdEventBuildDateCode(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

VOID nicCmdEventQueryStaStatistics(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
/* 4 Auto Channel Selection */
VOID nicCmdEventQueryLteSafeChn(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

#if CFG_SUPPORT_BATCH_SCAN
VOID nicCmdEventBatchScanResult(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
VOID nicCmdEventQueryAdvCtrl(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

VOID nicEventRddPulseDump(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryWlanInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID nicCmdEventQueryMibInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
VOID nicCmdEventTxMcsInfo(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
#endif

VOID nicCmdEventQueryNicCapabilityV2(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

WLAN_STATUS nicCmdEventQueryNicTxResource(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

WLAN_STATUS nicCmdEventQueryNicEfuseAddr(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

WLAN_STATUS nicCmdEventQueryEfuseOffset(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

WLAN_STATUS nicCmdEventQueryRModeCapability(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

WLAN_STATUS nicCmdEventQueryNicCoexFeature(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
WLAN_STATUS nicCmdEventQueryNicCsumOffload(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);
#endif

VOID nicEventLinkQuality(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventLayer0ExtMagic(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventMicErrorInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventScanDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventNloDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventSleepyNotify(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventBtOverWifi(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventStatistics(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventWlanInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventMibInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#if CFG_SUPPORT_LAST_SEC_MCS_INFO
VOID nicEventTxMcsInfo(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#endif
VOID nicEventBeaconTimeout(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventUpdateNoaParams(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventStaAgingTimeout(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventApObssStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventRoamingStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventSendDeauth(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventUpdateRddStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventUpdateBwcsStatus(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventUpdateBcmDebug(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventAddPkeyDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventIcapDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
VOID nicEventCalAllDone(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#endif
VOID nicEventDebugMsg(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventTdls(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventDumpMem(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventAssertDump(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventHifCtrl(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventRddSendPulse(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
VOID nicEventUpdateCoexPhyrate(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#if (CFG_WOW_SUPPORT == 1)
VOID nicEventWakeUpReason(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#endif
VOID nicEventCSIData(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);

#if CFG_SUPPORT_REPLAY_DETECTION
VOID nicCmdEventSetAddKey(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);
VOID nicOidCmdTimeoutSetAddKey(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

VOID nicEventGetGtkDataSync(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);
#endif

VOID nicCmdEventGetTxPwrTbl(IN P_ADAPTER_T prAdapter,
			    IN P_CMD_INFO_T prCmdInfo,
			    IN PUINT_8 pucEventBuf);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _NIC_CMD_EVENT_H */
