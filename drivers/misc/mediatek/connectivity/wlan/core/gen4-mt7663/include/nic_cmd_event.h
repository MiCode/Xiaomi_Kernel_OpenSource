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
 ** Id: //Department/DaVinci/BRANCHES/
 *      MT6620_WIFI_DRIVER_V2_3/include/nic_cmd_event.h#1
 */

/*! \file   "nic_cmd_event.h"
 *  \brief This file contains the declairation file of the WLAN OID processing
 *	 routines of Windows driver for MediaTek Inc.
 *   802.11 Wireless LAN Adapters.
 */

#ifndef _NIC_CMD_EVENT_H
#define _NIC_CMD_EVENT_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_vendor.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define CMD_PQ_ID           (0x8000)
#define CMD_PACKET_TYPE_ID  (0xA0)

#define PKT_FT_CMD			0x2

#define CMD_STATUS_SUCCESS      0
#define CMD_STATUS_REJECTED     1
#define CMD_STATUS_UNKNOWN      2

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
#define PPB_SIZE                        32
#define LPB_SIZE                        32
#endif

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
#define TXPOWER_EVENT_SHOW_ALL_RATE_TXPOWER_INFO    0x5
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
#define CONNECTION_TDLS	\
		(STA_TYPE_STA|NETWORK_INFRA|STA_TYPE_TDLS)
#define CONNECTION_WDS			(STA_TYPE_WDS|NETWORK_WDS)

#define ICAP_CONTENT_ADC		0x10000006
#define ICAP_CONTENT_TOAE		0x7
#define ICAP_CONTENT_SPECTRUM	0xB
#define ICAP_CONTENT_RBIST		0x10
#define ICAP_CONTENT_DCOC		0x20
#define ICAP_CONTENT_FIIQ		0x48
#define ICAP_CONTENT_FDIQ		0x49

#if CFG_SUPPORT_BUFFER_MODE

struct CMD_EFUSE_BUFFER_MODE {
	uint8_t ucSourceMode;
	uint8_t ucCount;
	uint8_t ucCmdType;  /* 0:6632, 1: 7668 */
	uint8_t ucReserved;
	uint8_t aBinContent[MAX_EEPROM_BUFFER_SIZE];
};

#define SOURCE_MODE_EFUSE                   (0)
#define SOURCE_MODE_BUFFER_MODE             (1)

#define CMD_TYPE_CONTENT_FORMAT_MASK        BITS(0, 3)
#define CONTENT_FORMAT_BIN_CONTENT          (0)
#define CONTENT_FORMAT_WHOLE_CONTENT        (1)
#define CONTENT_FORMAT_MULTIPLE_SECTIONS    (2)

#define CMD_TYPE_CAL_TIME_REDUCTION_MASK    BIT(4)
#define CMD_TYPE_CAL_TIME_REDUCTION_SHFT    (4)
#define CAL_TIME_REDUCTION_ENABLE           (1)

struct CMD_EFUSE_BUFFER_MODE_CONNAC_T {
	uint8_t ucSourceMode;
	uint8_t ucContentFormat;
	uint16_t u2Count;
	uint8_t aBinContent[MAX_EEPROM_BUFFER_SIZE];
};

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
struct CMD_ACCESS_EFUSE {
	uint32_t u4Address;
	uint32_t u4Valid;
	uint8_t aucData[16];
};

struct CMD_EFUSE_FREE_BLOCK {
	uint8_t  ucGetFreeBlock;
	uint8_t  ucVersion;
	uint8_t  ucDieIndex;
	uint8_t  aucReserved;
};


struct CMD_GET_TX_POWER {
	uint8_t ucTxPwrType;
	uint8_t ucCenterChannel;
	uint8_t ucDbdcIdx; /* 0:Band 0, 1: Band1 */
	uint8_t ucBand; /* 0:G-band 1: A-band*/
	uint8_t ucReserved[4];
};

/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */


struct CMD_SET_TX_TARGET_POWER {
	int8_t cTxPwr2G4Cck;       /* signed, in unit of 0.5dBm */
	int8_t cTxPwr2G4Dsss;      /* signed, in unit of 0.5dBm */
	uint8_t ucTxTargetPwr;		/* Tx target power base for all*/
	uint8_t ucReserved;

	int8_t cTxPwr2G4OFDM_BPSK;
	int8_t cTxPwr2G4OFDM_QPSK;
	int8_t cTxPwr2G4OFDM_16QAM;
	int8_t cTxPwr2G4OFDM_Reserved;
	int8_t cTxPwr2G4OFDM_48Mbps;
	int8_t cTxPwr2G4OFDM_54Mbps;

	int8_t cTxPwr2G4HT20_BPSK;
	int8_t cTxPwr2G4HT20_QPSK;
	int8_t cTxPwr2G4HT20_16QAM;
	int8_t cTxPwr2G4HT20_MCS5;
	int8_t cTxPwr2G4HT20_MCS6;
	int8_t cTxPwr2G4HT20_MCS7;

	int8_t cTxPwr2G4HT40_BPSK;
	int8_t cTxPwr2G4HT40_QPSK;
	int8_t cTxPwr2G4HT40_16QAM;
	int8_t cTxPwr2G4HT40_MCS5;
	int8_t cTxPwr2G4HT40_MCS6;
	int8_t cTxPwr2G4HT40_MCS7;

	int8_t cTxPwr5GOFDM_BPSK;
	int8_t cTxPwr5GOFDM_QPSK;
	int8_t cTxPwr5GOFDM_16QAM;
	int8_t cTxPwr5GOFDM_Reserved;
	int8_t cTxPwr5GOFDM_48Mbps;
	int8_t cTxPwr5GOFDM_54Mbps;

	int8_t cTxPwr5GHT20_BPSK;
	int8_t cTxPwr5GHT20_QPSK;
	int8_t cTxPwr5GHT20_16QAM;
	int8_t cTxPwr5GHT20_MCS5;
	int8_t cTxPwr5GHT20_MCS6;
	int8_t cTxPwr5GHT20_MCS7;

	int8_t cTxPwr5GHT40_BPSK;
	int8_t cTxPwr5GHT40_QPSK;
	int8_t cTxPwr5GHT40_16QAM;
	int8_t cTxPwr5GHT40_MCS5;
	int8_t cTxPwr5GHT40_MCS6;
	int8_t cTxPwr5GHT40_MCS7;
};


/*
 * Definitions for extension CMD_ID
 */
enum ENUM_EXT_CMD_ID {
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

	EXT_CMD_ID_EFUSE_FREE_BLOCK = 0x4F,
	EXT_CMD_ID_TX_POWER_FEATURE_CTRL = 0x58,
	EXT_CMD_ID_SER = 0x81,
};

enum NDIS_802_11_WEP_STATUS {
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
	/* TKIP or AES mix */
	Ndis802_11Encryption4Enabled = Ndis802_11TKIPAESMix,
	Ndis802_11Encryption4KeyAbsent,
	Ndis802_11GroupWEP40Enabled,
	Ndis802_11GroupWEP104Enabled,
#ifdef WAPI_SUPPORT
	Ndis802_11EncryptionSMS4Enabled,	/* WPI SMS4 support */
#endif /* WAPI_SUPPORT */
};

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

enum ENUM_CMD_ID {
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
#if CFG_SUPPORT_ADVANCE_CONTROL
	CMD_ID_KEEP_FULL_PWR = 0x2A,	/* 0x2A (Set) */
#endif
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
	CMD_ID_SET_MDNS_RECORD = 0x4e,

#if CFG_SUPPORT_WIFI_HOST_OFFLOAD
	CMD_ID_SET_AM_FILTER = 0x55,	/* 0x55 (Set) */
	CMD_ID_SET_AM_HEARTBEAT,	/* 0x56 (Set) */
	CMD_ID_SET_AM_TCP,		/* 0x57 (Set) */
#endif
	CMD_ID_SET_SUSPEND_MODE = 0x58,	/* 0x58 (Set) */

	CMD_ID_SET_COUNTRY_POWER_LIMIT_PER_RATE = 0x5d, /* 0x5d (Set) */

#if CFG_WOW_SUPPORT
	CMD_ID_SET_PF_CAPABILITY = 0x59,	/* 0x59 (Set) */
#endif
	CMD_ID_SET_RRM_CAPABILITY = 0x5A, /* 0x5A (Set) */
	CMD_ID_SET_AP_CONSTRAINT_PWR_LIMIT = 0x5B, /* 0x5B (Set) */
	CMD_ID_SET_TSM_STATISTICS_REQUEST = 0x5E,
	CMD_ID_GET_TSM_STATISTICS = 0x5F,
	CMD_ID_GET_PSCAN_CAPABILITY = 0x60,     /* 0x60 (Get) deprecated */
	CMD_ID_SET_SCHED_SCAN_ENABLE,           /* 0x61 (Set) */
	CMD_ID_SET_SCHED_SCAN_REQ,              /* 0x62 (Set) */
	CMD_ID_SET_GSCAN_ADD_HOTLIST_BSSID,     /* 0x63 (Set) deprecated */
	CMD_ID_SET_GSCAN_ADD_SWC_BSSID,         /* 0x64 (Set) deprecated */
	CMD_ID_SET_GSCAN_MAC_ADDR,              /* 0x65 (Set) deprecated */
	CMD_ID_GET_GSCAN_RESULT,                /* 0x66 (Get) deprecated */
	CMD_ID_SET_PSCAN_MAC_ADDR,              /* 0x67 (Set) deprecated */
	CMD_ID_UPDATE_AC_PARMS = 0x6A,		/* 0x6A (Set) */
#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
	/* 0x6D (Set) used to setting roaming skip*/
	CMD_ID_SET_ROAMING_SKIP = 0x6D,
#endif
	CMD_ID_GET_SET_CUSTOMER_CFG = 0x70, /* 0x70(Set) */
	CMD_ID_TDLS_PS = 0x75,		/* 0x75 (Set) */
	CMD_ID_GET_CNM = 0x79,
	CMD_ID_COEX_CTRL = 0x7C, /* 0x7C (Set/Query) */
	CMD_ID_PERF_IND = 0x7E, /* 0x7C (Set) */
	CMD_ID_GET_NIC_CAPABILITY = 0x80,	/* 0x80 (Query) */
	CMD_ID_GET_LINK_QUALITY,	/* 0x81 (Query) */
	CMD_ID_GET_STATISTICS,	/* 0x82 (Query) */
	CMD_ID_GET_CONNECTION_STATUS,	/* 0x83 (Query) */
	CMD_ID_GET_STA_STATISTICS = 0x85,	/* 0x85 (Query) */

	CMD_ID_GET_LTE_CHN = 0x87,	/* 0x87 (Query) */
	CMD_ID_GET_CHN_LOADING = 0x88,	/* 0x88 (Query) */
	CMD_ID_GET_BUG_REPORT = 0x89,	/* 0x89 (Query) */
	CMD_ID_GET_NIC_CAPABILITY_V2 = 0x8A,/* 0x8A (Query) */
	CMD_ID_LOG_UI_INFO = 0x8D,	/* 0x8D (Set / Query) */

#if CFG_SUPPORT_OSHARE
	CMD_ID_SET_OSHARE_MODE = 0x8C,
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
	CMD_ID_RDD_ON_OFF_CTRL = 0x8F, /* 0x8F(Set) */
#endif

#if CFG_SUPPORT_ANT_DIV
	CMD_ID_ANT_DIV_CTRL = 0x91,
#endif

	CMD_ID_TMR_ACTION = 0x92,

	CMD_ID_WFC_KEEP_ALIVE = 0xA0,	/* 0xa0(Set) */
	CMD_ID_RSSI_MONITOR = 0xA1,	/* 0xa1(Set) */

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
	CMD_ID_IPI_INFO		= 0xCC, /* 0xcc (Query) */
	CMD_ID_WLAN_INFO	= 0xCD, /* 0xcd (Query) */
	CMD_ID_MIB_INFO		= 0xCE, /* 0xce (Query) */
#if CFG_SUPPORT_GET_MCS_INFO
	CMD_ID_TX_MCS_INFO	= 0xCF, /* 0xcf (Query) */
#endif
	CMD_ID_GET_TXPWR_TBL = 0xD0, /* 0xd0 (Query) */
	CMD_ID_SET_TXBF_BACKOFF = 0xD1,

	CMD_ID_SET_RDD_CH = 0xE1,

#if CFG_SUPPORT_QA_TOOL
	/* magic number for Extending MT6630 original CMD header */
	CMD_ID_LAYER_0_EXT_MAGIC_NUM = 0xED,
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
};

enum ENUM_EVENT_ID {
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
	EVENT_ID_SCHED_SCAN_DONE,	/* 0x23 (Unsoiicited) */
	EVENT_ID_ADD_PKEY_DONE,	/* 0x24 (Unsoiicited) */
	EVENT_ID_ICAP_DONE,	/* 0x25 (Unsoiicited) */
	/* 0x26 (Query - CMD_ID_RESOURCE_CONFIG) */
	EVENT_ID_RESOURCE_CONFIG = 0x26,
	EVENT_ID_DEBUG_MSG = 0x27,	/* 0x27 (Unsoiicited) */
	EVENT_ID_RTT_CALIBR_DONE = 0x28,	/* 0x28 (Unsoiicited) */
	EVENT_ID_RTT_UPDATE_RANGE = 0x29,	/* 0x29 (Unsoiicited) */
	EVENT_ID_CHECK_REORDER_BUBBLE = 0x2a,	/* 0x2a (Unsoiicited) */
	EVENT_ID_BATCH_RESULT = 0x2b,	/* 0x2b (Query) */
	EVENT_ID_TX_ADDBA = 0x2e,	/* 0x2e (Unsolicited) */
	EVENT_ID_GET_GTK_REKEY_DATA = 0x3d, /* 0x3d (Query) */

	EVENT_ID_UART_ACK = 0x40,	/* 0x40 (Unsolicited) */
	EVENT_ID_UART_NAK,	/* 0x41 (Unsolicited) */
	EVENT_ID_GET_CHIPID,	/* 0x42 (Query - CMD_ID_GET_CHIPID) */
	EVENT_ID_SLT_STATUS,	/* 0x43 (Query - CMD_ID_SET_SLTINFO) */
	EVENT_ID_CHIP_CONFIG,	/* 0x44 (Query - CMD_ID_CHIP_CONFIG) */

#if CFG_SUPPORT_QA_TOOL
	/* 0x45 (Query - CMD_ID_ACCESS_RX_STAT) */
	EVENT_ID_ACCESS_RX_STAT,
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
#if (CFG_WOW_SUPPORT == 1)
	EVENT_ID_WOW_WAKEUP_REASON = 0x62,
#endif
#if CFG_SUPPORT_IDC_CH_SWITCH
	EVENT_ID_LTE_IDC_REPORT = 0x64,
#endif
#endif
	EVENT_ID_DBDC_SWITCH_DONE = 0x78,
	EVENT_ID_GET_CNM = 0x79,
	EVENT_ID_TDLS = 0x80,	/* TDLS event_id */
	EVENT_ID_LOG_UI_INFO = 0x8D,

	EVENT_ID_UPDATE_COEX_PHYRATE = 0x90,	/* 0x90 (Unsolicited) */

	EVENT_ID_TM_REPORT = 0x92,

	EVENT_ID_RSSI_MONITOR = 0xA1,

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
	/* 0xAE (Query - CMD_ID_CAL_BACKUP) */
	EVENT_ID_CAL_BACKUP_IN_HOST_V2 = 0xAE,
	/* 0xAF (FW Cal All Done Event) */
	EVENT_ID_CAL_ALL_DONE = 0xAF,
#endif
	EVENT_ID_IPI_INFO = 0xCC,
	EVENT_ID_WLAN_INFO = 0xCD,
	EVENT_ID_MIB_INFO = 0xCE,
#if CFG_SUPPORT_GET_MCS_INFO
	EVENT_ID_TX_MCS_INFO = 0xCF,
#endif
	EVENT_ID_GET_TXPWR_TBL = 0xD0,

	/* 0xEC (Query - CMD_ID_GET_NIC_CAPABILITY_V2) */
	EVENT_ID_NIC_CAPABILITY_V2 = 0xEC,
	/*#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)*/
	/* magic number for Extending MT6630 original EVENT header  */
	EVENT_ID_LAYER_0_EXT_MAGIC_NUM	= 0xED,
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
};

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
#define WOWLAN_DETECT_TYPE_NONE                 0
#define WOWLAN_DETECT_TYPE_MAGIC                BIT(0)
#define WOWLAN_DETECT_TYPE_ANY                  BIT(1)
#define WOWLAN_DETECT_TYPE_DISCONNECT          BIT(2)
#define WOWLAN_DETECT_TYPE_GTK_REKEY_FAILURE  BIT(3)
#define WOWLAN_DETECT_TYPE_BCN_LOST            BIT(4)

/* Wakeup command bit define */
#define PF_WAKEUP_CMD_BIT0_OUTPUT_MODE_EN   BIT(0)
#define PF_WAKEUP_CMD_BIT1_OUTPUT_DATA      BIT(1)
#define PF_WAKEUP_CMD_BIT2_WAKEUP_LEVEL     BIT(2)

#define PM_WOWLAN_REQ_START         0x1
#define PM_WOWLAN_REQ_STOP          0x2

struct CMD_WAKE_HIF {
	/* use in-band signal to wakeup system, ENUM_HIF_TYPE */
	uint8_t		ucWakeupHif;
	uint8_t		ucGpioPin;		/* GPIO Pin */
	uint8_t		ucTriggerLvl;	/* GPIO Pin */
	uint8_t		aucResv1[1];
	/* 0: low to high, 1: high to low */
	uint32_t		u4GpioInterval;
	uint8_t		aucResv2[4];
};

struct CMD_WOWLAN_PARAM {
	uint8_t		ucCmd;
	uint8_t		ucDetectType;
	/* ARP/MC/DropExceptMagic/SendMagicToHost */
	uint16_t		u2FilterFlag;
	uint8_t		ucScenarioID; /* WOW/WOBLE/Proximity */
	uint8_t		ucBlockCount;
	uint8_t		ucBssid;
	uint8_t		aucReserved1[1];
	struct CMD_WAKE_HIF astWakeHif[2];
	struct WOW_PORT	stWowPort;
	uint8_t		aucReserved2[32];
};

struct EVENT_WOWLAN_NOTIFY {
	uint8_t	ucNetTypeIndex;
	uint8_t	aucReserved[3];
};

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


enum _ENUM_FUNCTION_SELECT {
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
};

enum ENUM_PF_OPCODE {
	PF_OPCODE_ADD = 0,
	PF_OPCODE_DEL,
	PF_OPCODE_ENABLE,
	PF_OPCODE_DISABLE,
	PF_OPCODE_NUM
};

enum ENUM_SCN_FUNC_MASK {
	ENUM_SCN_RANDOM_MAC_EN = (1 << 0),
};

enum ENUM_WOW_WAKEUP_REASON {
	ENUM_PF_CMD_TYPE_MAGIC                         = 0,
	ENUM_PF_CMD_TYPE_BITMAP                        = 1,
	ENUM_PF_CMD_TYPE_ARPNS                         = 2,
	ENUM_PF_CMD_TYPE_GTK_REKEY                     = 3,
	ENUM_PF_CMD_TYPE_COALESCING_FILTER             = 4,
	ENUM_PF_CMD_TYPE_HW_GLOBAL_ENABLE              = 5,
	ENUM_PF_CMD_TYPE_TCP_SYN                       = 6,
	ENUM_PF_CMD_TYPE_TDLS                          = 7,
	ENUM_PF_CMD_TYPE_DISCONNECT                    = 8,
	ENUM_PF_CMD_TYPE_IPV4_UDP                      = 9,
	ENUM_PF_CMD_TYPE_IPV4_TCP                      = 10,
	ENUM_PF_CMD_TYPE_IPV6_UDP                      = 11,
	ENUM_PF_CMD_TYPE_IPV6_TCP                      = 12,
	ENUM_PF_CMD_TYPE_BEACON_LOST                   = 13,
	ENUM_PF_CMD_TYPE_UNDEFINED                     = 255,
};

struct CMD_PACKET_FILTER_CAP {
	uint8_t			ucCmd;
	uint16_t			packet_cap_type;
	uint8_t			aucReserved1[1];
	/* GLOBAL */
	uint32_t			PFType;
	uint32_t			FunctionSelect;
	uint32_t			Enable;
	/* MAGIC */
	uint8_t			ucBssid;
	uint16_t			usEnableBits;
	uint8_t			aucReserved5[1];
	/* DTIM */
	uint8_t			DtimEnable;
	uint8_t			DtimValue;
	uint8_t			aucReserved2[2];
	/* BITMAP_PATTERN_T */
	uint32_t			Index;
	uint32_t			Offset;
	uint32_t			FeatureBits;
	uint32_t			Resv;
	uint32_t			PatternLength;
	uint32_t			Mask[4];
	uint32_t			Pattern[32];
	/* COALESCE */
	uint32_t			FilterID;
	uint32_t			PacketType;
	uint32_t			CoalesceOP;
	uint8_t			FieldLength;
	uint8_t			CompareOP;
	uint8_t			FieldID;
	uint8_t			aucReserved3[1];
	uint32_t			Pattern3[4];
	/* TCPSYN */
	uint32_t			AddressType;
	uint32_t			TCPSrcPort;
	uint32_t			TCPDstPort;
	uint32_t			SourceIP[4];
	uint32_t			DstIP[4];
	uint8_t			aucReserved4[64];
};

#endif /*CFG_WOW_SUPPORT*/

#if CFG_SUPPORT_WIFI_HOST_OFFLOAD
struct CMD_TCP_GENERATOR {
	enum ENUM_PF_OPCODE eOpcode;
	uint32_t u4ReplyId;
	uint32_t u4Period;
	uint32_t u4Timeout;
	uint32_t u4IpId;
	uint32_t u4DestPort;
	uint32_t u4SrcPort;
	uint32_t u4Seq;
	uint8_t aucDestIp[4];
	uint8_t aucSrcIp[4];
	uint8_t aucDestMac[6];
	uint8_t ucBssId;
	uint8_t aucReserved1[1];
	uint8_t aucReserved2[64];
};

struct CMD_PATTERN_GENERATOR {
	enum ENUM_PF_OPCODE eOpcode;
	uint32_t u4ReplyId;
	uint32_t u4EthernetLength;
	uint32_t u4Period;
	uint8_t aucEthernetFrame[128];
	uint8_t ucBssId;
	uint8_t aucReserved1[3];
	uint8_t aucReserved2[64];
};

struct CMD_BITMAP_FILTER {
	enum ENUM_PF_OPCODE eOpcode;
	uint32_t u4ReplyId;
	uint32_t u4Offset;
	uint32_t u4Length;
	uint8_t aucPattern[64];
	uint8_t aucBitMask[64];
	u_int8_t fgIsEqual;
	u_int8_t fgIsAccept;
	uint8_t ucBssId;
	uint8_t aucReserved1[1];
	uint8_t aucReserved2[64];
};

#endif /*CFG_SUPPORT_WIFI_HOST_OFFLOAD*/

#if CFG_SUPPORT_PER_BSS_FILTER
struct CMD_RX_PACKET_FILTER {
	uint32_t u4RxPacketFilter;
	uint8_t  ucIsPerBssFilter;
	uint8_t  ucBssIndex;
	uint8_t  aucReserved[2];
	uint32_t u4BssMgmtFilter;
	uint8_t	aucReserved2[56];
};
#else
struct CMD_RX_PACKET_FILTER {
	uint32_t u4RxPacketFilter;
	uint8_t aucReserved[64];
};
#endif

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

#define EXT_EVENT_TARGET_TX_POWER  0x1

#define EXT_EVENT_ID_CMD_RESULT 0x00
#define EXT_EVENT_ID_EFUSE_ACCESS 0x01
#define EXT_EVENT_ID_RF_REG_ACCESS 0x02
#define EXT_EVENT_ID_EEPROM_ACCESS 0x03
#define EXT_EVENT_ID_RF_TEST 0x04
#define EXT_EVENT_ID_PS_SYNC 0x05
#define EXT_EVENT_ID_SLEEPY_NOTIFY 0x06
#define EXT_EVENT_ID_WLAN_ERROR 0x07
#define EXT_EVENT_ID_NIC_CAPABILITY 0x09
#define EXT_EVENT_ID_AP_PWR_SAVING_CLEAR 0x0A
#define EXT_EVENT_ID_SET_WTBL2_RATETABLE 0x0B
#define EXT_EVENT_ID_GET_WTBL2_INFORMATION 0x0C
#define EXT_EVENT_ID_MULTIPLE_REG_ACCESS 0x0E
#define EXT_EVENT_ID_AP_PWR_SAVING_CAPABILITY 0x0F
#define EXT_EVENT_ID_SECURITY_ADDREMOVE_KEY 0x10
#define EXT_EVENT_ID_FW_LOG_2_HOST 0x13
#define EXT_EVENT_ID_AP_PWR_SAVING_START 0x14
#define EXT_EVENT_ID_PACKET_FILTER 0x18
#define EXT_EVENT_ID_COEXISTENCE 0x19
#define EXT_EVENT_ID_BEACON_LOSS 0x1A
#define EXT_EVENT_ID_PWR_MGT_BIT_WIFI 0x1B
#define EXT_EVENT_ID_GET_TX_POWER 0x1C

#define EXT_EVENT_ID_WMT_EVENT_OVER_WIFI 0x20
#define EXT_EVENT_ID_MCC_TRIGGER 0x21
#define EXT_EVENT_ID_THERMAL_PROTECT 0x22
#define EXT_EVENT_ID_ASSERT_DUMP 0x23
#define EXT_EVENT_ID_GET_SENSOR_RESULT 0x2C
#define EXT_EVENT_ID_ROAMING_DETECT_NOTIFICATION 0x2D
#define EXT_EVENT_ID_TMR_CAL 0x2E
#define EXT_EVENT_ID_RA_THROUGHPUT_BURST 0x2F

#define EXT_EVENT_ID_GET_TX_STATISTIC 0x30
#define EXT_EVENT_ID_PRETBTT_INT 0x31
#define EXT_EVENT_ID_WTBL_UPDATE 0x32

#define EXT_EVENT_ID_BF_STATUS_READ 0x35
#define EXT_EVENT_ID_DRR_CTRL 0x36
#define EXT_EVENT_ID_BSSGROUP_CTRL 0x37
#define EXT_EVENT_ID_VOW_FEATURE_CTRL 0x38
#define EXT_EVENT_ID_PKT_PROCESSOR_CTRL 0x39
#define EXT_EVENT_ID_RDD_REPORT 0x3A
#define EXT_EVENT_ID_DEVICE_CAPABILITY 0x3B
#define EXT_EVENT_ID_MAC_INFO 0x3C
#define EXT_EVENT_ID_ATE_TEST_MODE 0x3D
#define EXT_EVENT_ID_CAC_END 0x3E
#define EXT_EVENT_ID_MU_CTRL 0x40

#define EXT_EVENT_ID_DBDC_CTRL 0x45
#define EXT_EVENT_ID_CONFIG_MUAR 0x48

#define EXT_EVENT_ID_RX_AIRTIME_CTRL 0x4a
#define EXT_EVENT_ID_AT_PROC_MODULE 0x4b
#define EXT_EVENT_ID_MAX_AMSDU_LENGTH_UPDATE 0x4c
#define EXT_EVENT_ID_EFUSE_FREE_BLOCK 0x4d
#define EXT_EVENT_ID_MURA_CTRL 0x4d
#define EXT_EVENT_ID_CSA_NOTIFY 0x4F
#define EXT_EVENT_ID_WIFI_SPECTRUM 0x50
#define EXT_EVENT_ID_TMR_CALCU_INFO 0x51
#define EXT_EVENT_ID_DUMP_MEM 0x57
#define EXT_EVENT_ID_TX_POWER_FEATURE_CTRL 0x58

#define EXT_EVENT_ID_G_BAND_256QAM_PROBE_RESULT 0x6B
#define EXT_EVENT_ID_MPDU_TIME_UPDATE 0x6F

/*#endif*/

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
#define EXT_EVENT_ID_TX_POWER_FEATURE_CTRL  0x58
#endif

#define SCHED_SCAN_CHANNEL_TYPE_SPECIFIED      (0)
#define SCHED_SCAN_CHANNEL_TYPE_DUAL_BAND      (1)
#define SCHED_SCAN_CHANNEL_TYPE_2G4_ONLY       (2)
#define SCHED_SCAN_CHANNEL_TYPE_5G_ONLY        (3)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
#ifndef LINUX
#endif
/* for Event Packet (via HIF-RX) */
struct PSE_CMD_HDR {
	/* DW0 */
	uint16_t u2TxByteCount;
	uint16_t u2Reserved1: 10;
	uint16_t u2Qidx: 5;
	uint16_t u2Pidx: 1;

	/* DW1 */
	uint16_t u2Reserved2: 13;
	uint16_t u2Hf: 2;
	uint16_t u2Ft: 1;
	uint16_t u2Reserved3: 8;
	uint16_t u2PktFt: 2;
	uint16_t u2Reserved4: 6;

	/* DW2~7 */
	uint32_t au4Reserved[6];
};

struct WIFI_CMD {
	uint16_t u2TxByteCount;	/* Max value is over 2048 */
	uint16_t u2PQ_ID;	/* Must be 0x8000 (Port1, Queue 0) */
#if 1
	uint8_t ucWlanIdx;
	uint8_t ucHeaderFormat;
	uint8_t ucHeaderPadding;
	uint8_t ucPktFt: 2;
	uint8_t ucOwnMAC: 6;
	uint32_t au4Reserved1[6];

	uint16_t u2Length;
	uint16_t u2PqId;
#endif
	uint8_t ucCID;
	uint8_t ucPktTypeID;	/* Must be 0x20 (CMD Packet) */
	uint8_t ucSetQuery;
	uint8_t ucSeqNum;
#if 1
	/* padding fields, hw may auto modify this field */
	uint8_t ucD2B0Rev;
	uint8_t ucExtenCID;	/* Extend CID */
	uint8_t ucS2DIndex;	/* Index for Src to Dst in CMD usage */
	uint8_t ucExtCmdOption;	/* Extend CID option */

	uint8_t ucCmdVersion;
	uint8_t ucReserved2[3];
	uint32_t au4Reserved3[4];	/* padding fields */
#endif
	uint8_t aucBuffer[0];
};

/* for Command Packet (via HIF-TX) */
/* following CM's documentation v0.7 */
struct WIFI_EVENT {
	uint16_t u2PacketLength;
	uint16_t u2PacketType;	/* Must be filled with 0xE000 (EVENT Packet) */
	uint8_t ucEID;
	uint8_t ucSeqNum;
	uint8_t ucEventVersion;
	uint8_t aucReserved[1];

	uint8_t ucExtenEID;
	uint8_t aucReserved2[2];
	uint8_t ucS2DIndex;

	uint8_t aucBuffer[0];
};

/* CMD_ID_TEST_CTRL */
struct CMD_TEST_CTRL {
	uint8_t ucAction;
	uint8_t aucReserved[3];
	union {
		uint32_t u4OpMode;
		uint32_t u4ChannelFreq;
		struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;
	} u;
};

struct CMD_TEST_CTRL_EXT_T {
	uint8_t ucAction;
	uint8_t ucIcapLen;
	uint8_t aucReserved[2];
	union {
		uint32_t u4OpMode;
		uint32_t u4ChannelFreq;
		struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T rRfATInfo;
	} u;
};

/* EVENT_TEST_STATUS */
struct PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT {
	uint32_t u4PktSentStatus;
	uint32_t u4PktSentCount;
	uint16_t u2AvgAlc;
	uint8_t ucCckGainControl;
	uint8_t ucOfdmGainControl;
};

struct PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT {
	/*!< number of packets that Rx ok from interrupt */
	uint32_t u4IntRxOk;
	/*!< number of packets that CRC error from interrupt */
	uint32_t u4IntCrcErr;
	/*!< number of packets that is short preamble from interrupt */
	uint32_t u4IntShort;
	/*!< number of packets that is long preamble from interrupt */
	uint32_t u4IntLong;
	/*!< number of packets that Rx ok from PAU */
	uint32_t u4PauRxPktCount;
	/*!< number of packets that CRC error from PAU */
	uint32_t u4PauCrcErrCount;
	/*!< number of packets that is short preamble from PAU */
	uint32_t u4PauRxFifoFullCount;
	uint32_t u4PauCCACount;	/*!< CCA rising edge count */
};

union EVENT_TEST_STATUS {
	struct PARAM_MTK_WIFI_TEST_STRUCT rATInfo;
	/* PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T   rTxStatus; */
	/* PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T   rRxStatus; */
};

/* CMD_BUILD_CONNECTION */
struct CMD_BUILD_CONNECTION {
	uint8_t ucInfraMode;
	uint8_t ucAuthMode;
	uint8_t ucEncryptStatus;
	uint8_t ucSsidLen;
	uint8_t aucSsid[PARAM_MAX_LEN_SSID];
	uint8_t aucBssid[PARAM_MAC_ADDR_LEN];

	/* Ad-hoc mode */
	uint16_t u2BeaconPeriod;
	uint16_t u2ATIMWindow;
	uint8_t ucJoinOnly;
	uint8_t ucReserved;
	uint32_t u4FreqInKHz;

	/* for faster connection */
	uint8_t aucScanResult[0];
};

/* CMD_ADD_REMOVE_KEY */
struct CMD_802_11_KEY {
	uint8_t ucAddRemove;
	uint8_t ucTxKey;
	uint8_t ucKeyType;
	uint8_t ucIsAuthenticator;
	uint8_t aucPeerAddr[6];
	uint8_t ucBssIdx;
	uint8_t ucAlgorithmId;
	uint8_t ucKeyId;
	uint8_t ucKeyLen;
	uint8_t ucWlanIndex;
	uint8_t ucMgmtProtection;
	uint8_t aucKeyMaterial[32];
	uint8_t aucKeyRsc[16];
};

/* CMD_ID_DEFAULT_KEY_ID */
struct CMD_DEFAULT_KEY {
	uint8_t ucBssIdx;
	uint8_t ucKeyId;
	uint8_t ucWlanIndex;
	uint8_t ucMulticast;
};

/* WPA2 PMKID cache structure */
struct PMKID_ENTRY {
	struct PARAM_BSSID_INFO rBssidInfo;
	u_int8_t fgPmkidExist;
};

struct CMD_802_11_PMKID {
	uint32_t u4BSSIDInfoCount;
	struct PMKID_ENTRY *arPMKIDInfo[1];
};

struct CMD_GTK_REKEY_DATA {
	uint8_t aucKek[16];
	uint8_t aucKck[16];
	uint8_t aucReplayCtr[8];
};

struct CMD_CSUM_OFFLOAD {
	uint16_t u2RxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
	uint16_t u2TxChecksum;	/* bit0: IP, bit1: UDP, bit2: TCP */
};

/* CMD_BASIC_CONFIG */
struct CMD_BASIC_CONFIG {
	uint8_t ucNative80211;
	uint8_t ucCtrlFlagAssertPath;
	uint8_t ucCtrlFlagDebugLevel;
	uint8_t aucReserved[1];
	struct CMD_CSUM_OFFLOAD rCsumOffload;
	uint8_t	ucCrlFlagSegememt;
	uint8_t	aucReserved2[3];
};

/* CMD_MAC_MCAST_ADDR */
struct CMD_MAC_MCAST_ADDR {
	uint32_t u4NumOfGroupAddr;
	uint8_t ucBssIndex;
	uint8_t aucReserved[3];
	uint8_t arAddress[MAX_NUM_GROUP_ADDR][PARAM_MAC_ADDR_LEN];
};

/* CMD_ACCESS_EEPROM */
struct CMD_ACCESS_EEPROM {
	uint16_t u2Offset;
	uint16_t u2Data;
};

struct CMD_CUSTOM_NOA_PARAM_STRUCT {
	uint32_t u4NoaDurationMs;
	uint32_t u4NoaIntervalMs;
	uint32_t u4NoaCount;
	uint8_t	ucBssIdx;
	uint8_t aucReserved[3];
};

struct CMD_CUSTOM_OPPPS_PARAM_STRUCT {
	uint32_t u4CTwindowMs;
	uint8_t	ucBssIdx;
	uint8_t aucReserved[3];
};

struct CMD_CUSTOM_UAPSD_PARAM_STRUCT {
	uint8_t fgEnAPSD;
	uint8_t fgEnAPSD_AcBe;
	uint8_t fgEnAPSD_AcBk;
	uint8_t fgEnAPSD_AcVo;
	uint8_t fgEnAPSD_AcVi;
	uint8_t ucMaxSpLen;
	uint8_t aucResv[2];
};

struct CMD_SET_AP_CONSTRAINT_PWR_LIMIT {
	/* DWORD_0 - Common Part */
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen; /* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body */
	uint8_t  ucBssIndex;
	uint8_t  ucPwrSetEnable;
	int8_t   cMaxTxPwr;              /* In unit of 0.5 dBm (signed) */
	int8_t   cMinTxPwr;              /* In unit of 0.5 dBm (signed) */

	uint8_t  aucPadding1[32];        /* for new param in the future */
};

struct CMD_SET_RRM_CAPABILITY {
	/* DWORD_0 - Common Part */
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen; /* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body */
	uint8_t  ucBssIndex;
	uint8_t  ucRrmEnable;          /* 802.11k rrm flag */
	/* Table 7-43e, RRM Enabled Capabilities Field */
	uint8_t ucCapabilities[5];
	uint8_t  aucPadding1[1];

	uint8_t  aucPadding2[32];      /* for new param in the future */
};

#if CFG_M0VE_BA_TO_DRIVER
struct CMD_RESET_BA_SCOREBOARD {
	uint8_t ucflag;
	uint8_t ucTID;
	uint8_t aucMacAddr[PARAM_MAC_ADDR_LEN];
};
#endif

/* EVENT_CONNECTION_STATUS */
struct EVENT_CONNECTION_STATUS {
	uint8_t ucMediaStatus;
	uint8_t ucReasonOfDisconnect;

	uint8_t ucInfraMode;
	uint8_t ucSsidLen;
	uint8_t aucSsid[PARAM_MAX_LEN_SSID];
	uint8_t aucBssid[PARAM_MAC_ADDR_LEN];
	uint8_t ucAuthenMode;
	uint8_t ucEncryptStatus;
	uint16_t u2BeaconPeriod;
	uint16_t u2AID;
	uint16_t u2ATIMWindow;
	uint8_t ucNetworkType;
	uint8_t aucReserved[1];
	uint32_t u4FreqInKHz;

#if CFG_ENABLE_WIFI_DIRECT
	uint8_t aucInterfaceAddr[PARAM_MAC_ADDR_LEN];
#endif

};

/* EVENT_NIC_CAPABILITY */
#define FEATURE_FLAG0_NIC_CAPABILITY_V2 BIT(0)

struct EVENT_NIC_CAPABILITY {
	uint16_t u2ProductID;
	uint16_t u2FwVersion;
	uint16_t u2DriverVersion;
	uint8_t ucHw5GBandDisabled;
	uint8_t ucEepromUsed;
	uint8_t aucMacAddr[6];
	uint8_t ucEndianOfMacAddrNumber;
	uint8_t ucHwNotSupportAC;

	uint8_t ucRfVersion;
	uint8_t ucPhyVersion;
	uint8_t ucRfCalFail;
	uint8_t ucBbCalFail;
	uint8_t aucDateCode[16];
	uint32_t u4FeatureFlag0;
	uint32_t u4FeatureFlag1;
	uint32_t u4CompileFlag0;
	uint32_t u4CompileFlag1;
	uint8_t aucBranchInfo[4];
	uint8_t ucFwBuildNumber;
	uint8_t ucHwSetNss1x1;
	uint8_t ucHwNotSupportDBDC;
	uint8_t ucHwBssIdNum;
	uint8_t aucReserved1[56];
};

struct EVENT_NIC_CAPABILITY_V2 {
	uint16_t u2TotalElementNum;
	uint8_t aucReserved[2];
	uint8_t aucBuffer[0];
};

struct NIC_CAPABILITY_V2_ELEMENT {
	uint32_t tag_type; /* NIC_CAPABILITY_V2_TAG_T */
	uint32_t body_len;
	uint8_t aucbody[0];
};

typedef uint32_t(*NIC_CAP_V2_ELEMENT_HDLR)(
	struct ADAPTER *prAdapter, uint8_t *buff);
struct NIC_CAPABILITY_V2_REF_TABLE {
	uint32_t tag_type; /* NIC_CAPABILITY_V2_TAG_T */
	NIC_CAP_V2_ELEMENT_HDLR hdlr;
};

enum NIC_CAPABILITY_V2_TAG {
	TAG_CAP_TX_RESOURCE = 0x0,
	TAG_CAP_TX_EFUSEADDRESS = 0x1,
	TAG_CAP_COEX_FEATURE = 0x2,
	TAG_CAP_SINGLE_SKU = 0x3,
	TAG_CAP_CSUM_OFFLOAD = 0x4,
	TAG_CAP_HW_VERSION = 0x5,
	TAG_CAP_SW_VERSION = 0x6,
	TAG_CAP_MAC_ADDR = 0x7,
	TAG_CAP_PHY_CAP = 0x8,
	TAG_CAP_MAC_CAP = 0x9,
	TAG_CAP_FRAME_BUF_CAP = 0xa,
	TAG_CAP_BEAMFORM_CAP = 0xb,
	TAG_CAP_LOCATION_CAP = 0xc,
	TAG_CAP_MUMIMO_CAP = 0xd,
	TAG_CAP_BUFFER_MODE_INFO = 0xe,
	TAG_CAP_TOTAL
};

#if CFG_TCP_IP_CHKSUM_OFFLOAD
struct NIC_CSUM_OFFLOAD {
	uint8_t ucIsSupportCsumOffload;  /* 1: Support, 0: Not Support */
	uint8_t aucReseved[3];
};
#endif

struct NIC_COEX_FEATURE {
	uint32_t u4FddMode;  /* TRUE for COEX FDD mode */
};

struct NIC_EFUSE_ADDRESS {
	uint32_t u4EfuseStartAddress;  /* Efuse Start Address */
	uint32_t u4EfuseEndAddress;   /* Efuse End Address */
};

struct CAP_HW_VERSION {
	uint16_t u2ProductID; /* CHIP ID */
	uint8_t ucEcoVersion; /* ECO version */
	uint8_t ucReserved;
	uint32_t u4MacIpID; /* MAC IP version */
	uint32_t u4BBIpID; /* Baseband IP version */
	uint32_t u4TopIpID; /* TOP IP version */
	uint32_t u4ConfigId;  /* Configuration ID */
};

struct CAP_SW_VERSION {
	uint16_t u2FwVersion; /* FW version <major.minor> */
	uint16_t u2FwBuildNumber; /* FW build number */
	uint8_t aucBranchInfo[4]; /* Branch name in ASCII */
	uint8_t aucDateCode[16]; /* FW build data code */
};

struct CAP_MAC_ADDR {
	uint8_t aucMacAddr[6];
	uint8_t aucReserved[2];
};

struct CAP_PHY_CAP {
	uint8_t ucHt; /* 1:support, 0:not*/
	uint8_t ucVht; /* 1:support, 0:not*/
	uint8_t uc5gBand; /* 1:support, 0:not*/
	/* 0: BW20, 1:BW40, 2:BW80, 3:BW160, 4:BW80+80 */
	uint8_t ucMaxBandwidth;
	uint8_t ucNss; /* 1:1x1, 2:2x2, ... */
	uint8_t ucDbdc; /* 1:support, 0:not*/
	uint8_t ucTxLdpc; /* 1:support, 0:not*/
	uint8_t ucRxLdpc; /* 1:support, 0:not*/
	uint8_t ucTxStbc; /* 1:support, 0:not*/
	uint8_t ucRxStbc; /* 1:support, 0:not*/
	/* BIT(0): 2G4_WF0, BIT(1): 5G_WF0, BIT(2): 2G4_WF1, BIT(3): 5G_WF1 */
	uint8_t ucHwWifiPath;
	uint8_t aucReserved[1];
};

struct CAP_MAC_CAP {
	uint8_t ucHwBssIdNum; /* HW BSSID number */
	uint8_t ucWmmSet; /* 1: AC0~3, 2: AC0~3 and AC10~13, ... */
	uint8_t ucWtblEntryNum; /* WTBL entry number */
	uint8_t ucReserved;
};

struct CAP_FRAME_BUF_CAP {
	/* 1: support in-chip Tx AMSDU (HW or CR4) */
	uint8_t ucChipTxAmsdu;
	/* 2: support 2 MSDU in AMSDU, 3:support 3 MSDU in AMSDU,...*/
	uint8_t ucTxAmsduNum;
	/* Rx AMSDU size, 0:4K, 1:8K,2:12K */
	uint8_t ucRxAmsduSize;
	uint8_t ucReserved;
	/* Txd entry number */
	uint32_t u4TxdCount;
	/* Txd and packet buffer in KB (cut through sysram size) */
	uint32_t u4PacketBufSize;
};

struct CAP_BEAMFORM_CAP {
	uint8_t ucBFer; /* Tx beamformer, 1:support, 0:not*/
	uint8_t ucIBFer; /* Tx implicit beamformer 1:support, 0:not*/
	uint8_t ucBFee; /* Rx beamformee, 1:support, 0:not */
	uint8_t ucReserved;
	uint32_t u4BFerCap; /* Tx beamformere cap */
	uint32_t u4BFeeCap; /* Rx beamformee cap */
};

struct CAP_LOCATION_CAP {
	uint8_t ucTOAE; /* 1:support, 0:not */
	uint8_t aucReserved[3];
};

struct CAP_MUMIMO_CAP {
	uint8_t ucMuMimoRx; /* 1:support, 0:not */
	uint8_t ucMuMimoTx; /* 1:support, 0:not */
	uint8_t aucReserved[2];
};

#define EFUSE_SECTION_TABLE_SIZE        (10)   /* It should not be changed. */

struct EFUSE_SECTION_T {
	uint16_t         u2StartOffset;
	uint16_t         u2Length;
};

struct CAP_BUFFER_MODE_INFO_T {
	uint8_t ucVersion; /* Version */
	uint8_t ucFormatSupportBitmap; /* Format Support Bitmap*/
	uint16_t u2EfuseTotalSize; /* Total eFUSE Size */
	struct EFUSE_SECTION_T arSections[EFUSE_SECTION_TABLE_SIZE];
	/* NOTE: EFUSE_SECTION_TABLE_SIZE should be fixed to
	 * 10 so that driver don't change.
	 */
};

/*
 * NIC_TX_RESOURCE_REPORT_EVENT related definition
 */

#define NIC_TX_RESOURCE_REPORT_VERSION_PREFIX (0x80000000)
#define NIC_TX_RESOURCE_REPORT_VERSION_1 \
	(NIC_TX_RESOURCE_REPORT_VERSION_PREFIX | 0x1)

struct nicTxRsrcEvtHdlr {
	uint32_t u4Version;

	uint32_t(*nicEventTxResource)
		(IN struct ADAPTER *prAdapter,
		 IN uint8_t *pucEventBuf);
	void (*nicTxResourceInit)(IN struct ADAPTER *prAdapter);
};

struct NIC_TX_RESOURCE {
	/* the total usable resource for MCU port */
	uint32_t u4CmdTotalResource;
	/* the unit of a MCU resource */
	uint32_t u4CmdResourceUnit;
	/* the total usable resource for LMAC port */
	uint32_t u4DataTotalResource;
	/* the unit of a LMAC resource */
	uint32_t u4DataResourceUnit;
};

struct tx_resource_report_v1 {
	/*
	 * u4Version: NIC_TX_RESOURCE_REPORT_VERSION_1
	 */
	uint32_t u4Version;

	/*
	 * the followings are content for u4Verion = 0x80000001
	 */
	uint32_t u4HifDataPsePageQuota;
	uint32_t u4HifCmdPsePageQuota;
	uint32_t u4HifDataPlePageQuota;
	uint32_t u4HifCmdPlePageQuota;

	/*
	 * u4PlePsePageSize: the unit of a page in PLE and PSE
	 * [31:16] PLE page size
	 * [15:0] PLE page size
	 */
	uint32_t u4PlePsePageSize;

	/*
	 * ucPpTxAddCnt: the extra pse resource needed by HW
	 */
	uint8_t ucPpTxAddCnt;

	uint8_t ucReserved[3];
};


/* modified version of WLAN_BEACON_FRAME_BODY_T for simplier buffering */
struct WLAN_BEACON_FRAME_BODY_T_LOCAL {
	/* Beacon frame body */
	uint32_t au4Timestamp[2];	/* Timestamp */
	uint16_t u2BeaconInterval;	/* Beacon Interval */
	uint16_t u2CapInfo;	/* Capability */
	/* Various IEs, start from SSID */
	uint8_t aucInfoElem[MAX_IE_LENGTH];
	/* This field is *NOT* carried by F/W but caculated by nic_rx */
	uint16_t u2IELength;
};

/* EVENT_SCAN_RESULT */
struct EVENT_SCAN_RESULT {
	int32_t i4RSSI;
	uint32_t u4LinkQuality;
	uint32_t u4DSConfig;	/* Center frequency */
	uint32_t u4DomainInfo;	/* Require CM opinion */
	uint32_t u4Reserved;
	uint8_t ucNetworkType;
	uint8_t ucOpMode;
	uint8_t aucBssid[MAC_ADDR_LEN];
	uint8_t aucRatesEx[PARAM_MAX_LEN_RATES_EX];
	struct WLAN_BEACON_FRAME_BODY_T_LOCAL rBeaconFrameBody;
};

/* event of tkip mic error */
struct EVENT_MIC_ERR_INFO {
	uint32_t u4Flags;
};

/* event of add key done for port control */
struct EVENT_ADD_KEY_DONE_INFO {
	uint8_t ucBSSIndex;
	uint8_t ucReserved;
	uint8_t aucStaAddr[6];
};

struct EVENT_PMKID_CANDIDATE_LIST {
	uint32_t u4Version;	/*!< Version */
	uint32_t u4NumCandidates;	/*!< How many candidates follow */
	struct PARAM_PMKID_CANDIDATE arCandidateList[1];
};

struct EVENT_CMD_RESULT {
	uint8_t ucCmdID;
	uint8_t ucStatus;
	uint8_t aucReserved[2];
};

/* CMD_ID_ACCESS_REG & EVENT_ID_ACCESS_REG */
struct CMD_ACCESS_REG {
	uint32_t u4Address;
	uint32_t u4Data;
};

#define COEX_CTRL_BUF_LEN 460
/* CMD_COEX_HANDLER & EVENT_COEX_HANDLER */
/************************************************/
/*  UINT_32 u4SubCmd : Coex Ctrl Sub Command    */
/*  UINT_8 aucBuffer : Reserve for Sub Command  */
/*                    Data Structure            */
/************************************************/
struct CMD_COEX_HANDLER {
	uint32_t u4SubCmd;
	uint8_t aucBuffer[COEX_CTRL_BUF_LEN];
};


/* Sub Command Data Structure */
/************************************************/
/*  UINT_32 u4IsoPath : WF Path (WF0/WF1)       */
/*  UINT_32 u4Channel : WF Channel              */
/*  UINT_32 u4Band    : WF Band (Band0/Band1)(Not used now)   */
/*  UINT_32 u4Isolation  : Isolation value      */
/************************************************/
struct CMD_COEX_ISO_DETECT {
	uint32_t u4IsoPath;
	uint32_t u4Channel;
	uint32_t u4Isolation;
};

/************************************************/
/*  PCHAR   pucCoexInfo : CoexInfoTag           */
/************************************************/
struct CMD_COEX_GET_INFO {
	uint8_t ucCoexInfo[COEX_CTRL_BUF_LEN];
};

/* Coex Command Used  */
enum ENUM_COEX_CMD_CTRL {
	/* Set */
	COEX_CMD_SET_DISABLE_DATA_RSSI_UPDATE = 0x00,
	COEX_CMD_SET_DATA_RSSI = 0x01,
	/* Get */
	COEX_CMD_GET_ISO_DETECT = 0x80,
	COEX_CMD_GET_INFO = 0x81,
	COEX_CMD_NUM
};

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
/* CMD_ID_CAL_BACKUP_IN_HOST_V2 & EVENT_ID_CAL_BACKUP_IN_HOST_V2 */
struct CMD_CAL_BACKUP_STRUCT_V2 {
	uint8_t	ucReason;
	uint8_t	ucAction;
	uint8_t	ucNeedResp;
	uint8_t	ucFragNum;
	uint8_t	ucRomRam;
	uint32_t	u4ThermalValue;
	uint32_t u4Address;
	uint32_t	u4Length;
	uint32_t	u4RemainLength;
	uint32_t	au4Buffer[PARAM_CAL_DATA_DUMP_MAX_NUM];
};

struct CMD_CAL_BACKUP_STRUCT {
	uint8_t	ucReason;
	uint8_t	ucAction;
	uint8_t	ucNeedResp;
	uint8_t	ucFragNum;
	uint8_t	ucRomRam;
	uint32_t	u4ThermalValue;
	uint32_t u4Address;
	uint32_t	u4Length;
	uint32_t	u4RemainLength;
};
#endif

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
struct CMD_ACCESS_CHN_LOAD {
	uint32_t u4Address;
	uint32_t u4Data;
	uint16_t u2Channel;
	uint8_t aucReserved[2];
};

struct CMD_GET_LTE_SAFE_CHN {
	uint8_t ucIndex;
	uint8_t ucFlags;
	uint8_t aucReserved0[2];
	uint8_t aucReserved2[16];
};
#endif

/* CMD_DUMP_MEMORY */
struct CMD_DUMP_MEM {
	uint32_t u4Address;
	uint32_t u4Length;
	uint32_t u4RemainLength;
#if CFG_SUPPORT_QA_TOOL
	uint32_t u4IcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
	uint8_t ucFragNum;
};

struct EVENT_DUMP_MEM {
	uint32_t u4Address;
	uint32_t u4Length;
	uint32_t u4RemainLength;
#if CFG_SUPPORT_QA_TOOL
	uint32_t eIcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
	uint8_t ucFragNum;
	uint8_t aucBuffer[1];
};

#if CFG_SUPPORT_QA_TOOL
struct CMD_ACCESS_RX_STAT {
	uint32_t u4SeqNum;
	uint32_t u4TotalNum;
};

struct EVENT_ACCESS_RX_STAT {
	uint32_t u4SeqNum;
	uint32_t u4TotalNum;
	uint32_t au4Buffer[1];
};

#if CFG_SUPPORT_TX_BF
union CMD_TXBF_ACTION {
	struct PROFILE_TAG_READ rProfileTagRead;
	struct PROFILE_TAG_WRITE rProfileTagWrite;
	struct PROFILE_DATA_READ rProfileDataRead;
	struct PROFILE_DATA_WRITE rProfileDataWrite;
	struct PROFILE_PN_READ rProfilePnRead;
	struct PROFILE_PN_WRITE rProfilePnWrite;
	struct TX_BF_SOUNDING_START rTxBfSoundingStart;
	struct TX_BF_SOUNDING_STOP rTxBfSoundingStop;
	struct TX_BF_TX_APPLY rTxBfTxApply;
	struct TX_BF_PFMU_MEM_ALLOC rTxBfPfmuMemAlloc;
	struct TX_BF_PFMU_MEM_RLS rTxBfPfmuMemRls;
#if CFG_SUPPORT_TX_BF_FPGA
	struct TX_BF_PROFILE_SW_TAG_WRITE rTxBfProfileSwTagWrite;
#endif
};

#define CMD_DEVINFO_UPDATE_HDR_SIZE 8
struct CMD_DEV_INFO_UPDATE {
	uint8_t ucOwnMacIdx;
	uint8_t ucReserve;
	uint16_t u2TotalElementNum;
	uint8_t ucAppendCmdTLV;
	uint8_t aucReserve[3];
	uint8_t aucBuffer[0];
	/* CMD_DEVINFO_ACTIVE_T rCmdDevInfoActive; */
};

#define CMD_BSSINFO_UPDATE_HDR_SIZE 8
struct CMD_BSS_INFO_UPDATE {
	uint8_t ucBssIndex;
	uint8_t ucReserve;
	uint16_t u2TotalElementNum;
	uint32_t u4Reserve;
	/* CMD_BSSINFO_BASIC_T rCmdBssInfoBasic; */
	uint8_t aucBuffer[0];
};

/*  STA record command */
#define CMD_STAREC_UPDATE_HDR_SIZE 8
struct CMD_STAREC_UPDATE {
	uint8_t ucBssIndex;
	uint8_t ucWlanIdx;
	uint16_t u2TotalElementNum;
	uint32_t u4Reserve;
	uint8_t aucBuffer[0];
};

struct EXT_EVENT_BF_STATUS_T {
	uint8_t ucEventCategoryID;
	uint8_t ucBw;
	uint16_t u2SubCarrIdx;
	u_int8_t fgBFer;
	uint8_t aucReserved[3];
	uint8_t aucBuf[1000]; /* temp size */
};

struct EVENT_PFMU_TAG_READ {
	union PFMU_PROFILE_TAG1 ru4TxBfPFMUTag1;
	union PFMU_PROFILE_TAG2 ru4TxBfPFMUTag2;
};

#if CFG_SUPPORT_MU_MIMO
struct EVENT_HQA_GET_QD {
	uint32_t u4EventId;
	uint32_t au4RawData[14];
};

struct EVENT_HQA_GET_MU_CALC_LQ {
	uint32_t u4EventId;
	struct MU_STRUCT_LQ_REPORT rEntry;
};

struct EVENT_SHOW_GROUP_TBL_ENTRY {
	uint32_t u4EventId;
	uint8_t index;
	uint8_t numUser: 2;
	uint8_t BW: 2;
	uint8_t NS0: 2;
	uint8_t NS1: 2;
	/* UINT_8       NS2:1; */
	/* UINT_8       NS3:1; */
	uint8_t PFIDUser0;
	uint8_t PFIDUser1;
	/* UINT_8       PFIDUser2; */
	/* UINT_8       PFIDUser3; */
	u_int8_t fgIsShortGI;
	u_int8_t fgIsUsed;
	u_int8_t fgIsDisable;
	uint8_t initMcsUser0: 4;
	uint8_t initMcsUser1: 4;
	/* UINT_8       initMcsUser2:4; */
	/* UINT_8       initMcsUser3:4; */
	uint8_t dMcsUser0: 4;
	uint8_t dMcsUser1: 4;
	/* UINT_8       dMcsUser2:4; */
	/* UINT_8       dMcsUser3:4; */
};

union CMD_MUMIMO_ACTION {
	uint8_t ucMuMimoCategory;
	uint8_t aucRsv[3];
	union {
		struct MU_GET_CALC_INIT_MCS rMuGetCalcInitMcs;
		struct MU_SET_INIT_MCS rMuSetInitMcs;
		struct MU_SET_CALC_LQ rMuSetCalcLq;
		struct MU_GET_LQ rMuGetLq;
		struct MU_SET_SNR_OFFSET rMuSetSnrOffset;
		struct MU_SET_ZERO_NSS rMuSetZeroNss;
		struct MU_SPEED_UP_LQ rMuSpeedUpLq;
		struct MU_SET_MU_TABLE rMuSetMuTable;
		struct MU_SET_GROUP rMuSetGroup;
		struct MU_GET_QD rMuGetQd;
		struct MU_SET_ENABLE rMuSetEnable;
		struct MU_SET_GID_UP rMuSetGidUp;
		struct MU_TRIGGER_MU_TX rMuTriggerMuTx;
	} unMuMimoParam;
};
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

struct CMD_SW_DBG_CTRL {
	uint32_t u4Id;
	uint32_t u4Data;
	/* Debug Support */
	uint32_t u4DebugCnt[64];
};


#if CFG_SUPPORT_ANT_DIV
enum {
	ANT_DIV_DISABLE = 0,
	ANT_DIV_ANT_1,
	ANT_DIV_ANT_2,
	ANT_DIV_SUCCESS,
	ANT_DIV_WF_SCNING,
	ANT_DIV_BT_SCNING,
	ANT_DIV_DISCONNECT,
	ANT_DIV_INVALID_RSSI,
	ANT_DIV_MSG_TIMEOUT,
	ANT_DIV_SWH_FAIL,
	ANT_DIV_PARA_ERR,
	ANT_DIV_OTHER_FAIL,
	ANT_DIV_STATE_NUM
};
enum {
	ANT_DIV_CMD_SET_ANT = 0,
	ANT_DIV_CMD_GET_ANT,
	ANT_DIV_CMD_DETC,
	ANT_DIV_CMD_SWH,
};
struct CMD_ANT_DIV_CTRL {
	uint8_t ucAction;
	uint8_t ucAntId;
	uint8_t ucState;
	uint8_t ucRcpi;
	uint8_t ucReserve[8];
};
#endif

struct CMD_FW_LOG_2_HOST_CTRL {
	uint8_t ucFwLog2HostCtrl;
	uint8_t ucMcuDest;
	uint8_t ucReserve[2];
};

struct CMD_CHIP_CONFIG {
	uint16_t u2Id;
	uint8_t ucType;
	uint8_t ucRespType;
	uint16_t u2MsgSize;
	uint8_t aucReserved0[2];
	uint8_t aucCmd[CHIP_CONFIG_RESP_SIZE];
};

/* CMD_ID_LINK_ATTRIB */
struct CMD_LINK_ATTRIB {
	int8_t cRssiTrigger;
	uint8_t ucDesiredRateLen;
	uint16_t u2DesiredRate[32];
	uint8_t ucMediaStreamMode;
	uint8_t aucReserved[1];
};

/* CMD_ID_NIC_POWER_CTRL */
struct CMD_NIC_POWER_CTRL {
	uint8_t ucPowerMode;
	uint8_t aucReserved[3];
};

/* CMD_ID_POWER_SAVE_MODE */
struct CMD_PS_PROFILE {
	uint8_t ucBssIndex;
	uint8_t ucPsProfile;
	uint8_t aucReserved[2];
};

/* EVENT_LINK_QUALITY */
#if 1
struct LINK_QUALITY_ {
	int8_t cRssi;		/* AIS Network. */
	int8_t cLinkQuality;
	uint16_t u2LinkSpeed;	/* TX rate1 */
	uint8_t ucMediumBusyPercentage;	/* Read clear */
	uint8_t ucIsLQ0Rdy;	/* Link Quality BSS0 Ready. */
};

struct EVENT_LINK_QUALITY_V2 {
	struct LINK_QUALITY_ rLq[BSSID_NUM];
};

struct EVENT_LINK_QUALITY {
	int8_t cRssi;
	int8_t cLinkQuality;
	uint16_t u2LinkSpeed;
	uint8_t ucMediumBusyPercentage;
};
#endif

#if CFG_SUPPORT_P2P_RSSI_QUERY
/* EVENT_LINK_QUALITY */
struct EVENT_LINK_QUALITY_EX {
	int8_t cRssi;
	int8_t cLinkQuality;
	uint16_t u2LinkSpeed;
	uint8_t ucMediumBusyPercentage;
	uint8_t ucIsLQ0Rdy;
	int8_t cRssiP2P;		/* For P2P Network. */
	int8_t cLinkQualityP2P;
	uint16_t u2LinkSpeedP2P;
	uint8_t ucMediumBusyPercentageP2P;
	uint8_t ucIsLQ1Rdy;
};
#endif

/* EVENT_ID_STATISTICS */
struct EVENT_STATISTICS {
	union LARGE_INTEGER rTransmittedFragmentCount;
	union LARGE_INTEGER rMulticastTransmittedFrameCount;
	union LARGE_INTEGER rFailedCount;
	union LARGE_INTEGER rRetryCount;
	union LARGE_INTEGER rMultipleRetryCount;
	union LARGE_INTEGER rRTSSuccessCount;
	union LARGE_INTEGER rRTSFailureCount;
	union LARGE_INTEGER rACKFailureCount;
	union LARGE_INTEGER rFrameDuplicateCount;
	union LARGE_INTEGER rReceivedFragmentCount;
	union LARGE_INTEGER rMulticastReceivedFrameCount;
	union LARGE_INTEGER rFCSErrorCount;
};

struct _EVENT_BUG_REPORT_T {
	/* BugReportVersion 1 */
	uint32_t u4BugReportVersion;

	/* FW Module State 2 */
	uint32_t u4FWState;

	/* Scan Counter 3-6 */
	uint32_t u4ReceivedBeaconCount;
	uint32_t u4ReceivedProbeResponseCount;
	uint32_t u4SentProbeRequestCount;
	uint32_t u4SentProbeRequestFailCount;

	/* Roaming Counter 7-9 */
	uint32_t u4RoamingDebugFlag;
	uint32_t u4RoamingThreshold;
	uint32_t u4RoamingCurrentRcpi;

	/* RF Counter 10-14 */
	uint32_t u4RFPriChannel;
	uint32_t u4RFChannelS1;
	uint32_t u4RFChannelS2;
	uint32_t u4RFChannelWidth;
	uint32_t u4RFSco;

	/* Coex Counter 15-17 */
	uint32_t u4BTProfile;
	uint32_t u4BTOn;
	uint32_t u4LTEOn;

	/* Low Power Counter 18-20 */
	uint32_t u4LPTxUcPktNum;
	uint32_t u4LPRxUcPktNum;
	uint32_t u4LPPSProfile;

	/* Base Band Counter 21- 32 */
	uint32_t u4OfdmPdCnt;
	uint32_t u4CckPdCnt;
	uint32_t u4CckSigErrorCnt;
	uint32_t u4CckSfdErrorCnt;
	uint32_t u4OfdmSigErrorCnt;
	uint32_t u4OfdmTaqErrorCnt;
	uint32_t u4OfdmFcsErrorCnt;
	uint32_t u4CckFcsErrorCnt;
	uint32_t u4OfdmMdrdyCnt;
	uint32_t u4CckMdrdyCnt;
	uint32_t u4PhyCcaStatus;
	uint32_t u4WifiFastSpiStatus;

	/* Mac RX Counter 33-45 */
	uint32_t u4RxMdrdyCount;
	uint32_t u4RxFcsErrorCount;
	uint32_t u4RxbFifoFullCount;
	uint32_t u4RxMpduCount;
	uint32_t u4RxLengthMismatchCount;
	uint32_t u4RxCcaPrimCount;
	uint32_t u4RxEdCount;
	uint32_t u4LmacFreeRunTimer;
	uint32_t u4WtblReadPointer;
	uint32_t u4RmacWritePointer;
	uint32_t u4SecWritePointer;
	uint32_t u4SecReadPointer;
	uint32_t u4DmaReadPointer;

	/* Mac TX Counter 46-47 */
	uint32_t u4TxChannelIdleCount;
	uint32_t u4TxCcaNavTxTime;
	/* If you want to add item,
	 * please modify BUG_REPORT_NUM in mtk_driver_nl80211.h
	 */
};

/* EVENT_ID_FW_SLEEPY_NOTIFY */
struct EVENT_SLEEPY_INFO {
	uint8_t ucSleepyState;
	uint8_t aucReserved[3];
};

struct EVENT_ACTIVATE_STA_REC {
	uint8_t aucMacAddr[6];
	uint8_t ucStaRecIdx;
	uint8_t ucBssIndex;
};

struct EVENT_DEACTIVATE_STA_REC {
	uint8_t ucStaRecIdx;
	uint8_t aucReserved[3];
};

/* CMD_BT_OVER_WIFI */
struct CMD_BT_OVER_WIFI {
	uint8_t ucAction;	/* 0: query, 1: setup, 2: destroy */
	uint8_t ucChannelNum;
	uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2BeaconInterval;
	uint8_t ucTimeoutDiscovery;
	uint8_t ucTimeoutInactivity;
	uint8_t ucRole;
	uint8_t PAL_Capabilities;
	uint8_t cMaxTxPower;
	uint8_t ucChannelBand;
	uint8_t ucReserved[1];
};

#if (CFG_SUPPORT_DFS_MASTER == 1)
enum ENUM_REG_DOMAIN {
	REG_DEFAULT = 0,
	REG_JP_53,
	REG_JP_56
};

struct CMD_RDD_ON_OFF_CTRL {
	uint8_t ucDfsCtrl;
	uint8_t ucRddIdx;
	uint8_t ucRddRxSel;
	uint8_t ucSetVal;
	uint8_t aucReserve[4];
};
#endif

/* EVENT_BT_OVER_WIFI */
struct EVENT_BT_OVER_WIFI {
	uint8_t ucLinkStatus;
	uint8_t ucSelectedChannel;
	int8_t cRSSI;
	uint8_t ucReserved[1];
};

/* Same with DOMAIN_SUBBAND_INFO */
struct CMD_SUBBAND_INFO {
	uint8_t ucRegClass;
	uint8_t ucBand;
	uint8_t ucChannelSpan;
	uint8_t ucFirstChannelNum;
	uint8_t ucNumChannels;
	uint8_t aucReserved[3];
};

/* CMD_SET_DOMAIN_INFO */
#if (CFG_SUPPORT_SINGLE_SKU == 1)
struct CMD_SET_DOMAIN_INFO_V2 {
	uint32_t u4CountryCode;

	uint8_t uc2G4Bandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	uint8_t uc5GBandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	uint8_t aucReserved[2];
	struct acctive_channel_list active_chs;
};
#endif

struct CMD_SET_DOMAIN_INFO {
	uint16_t u2CountryCode;
	uint16_t u2IsSetPassiveScan;
	struct CMD_SUBBAND_INFO rSubBand[6];

	uint8_t uc2G4Bandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	uint8_t uc5GBandwidth;	/* CONFIG_BW_20_40M or CONFIG_BW_20M */
	uint8_t aucReserved[2];
};

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

/* CMD_SET_PWR_LIMIT_TABLE */
struct CMD_CHANNEL_POWER_LIMIT {
	uint8_t ucCentralCh;

	int8_t cPwrLimitCCK;
	int8_t cPwrLimit20L; /* MCS0~4 */
	int8_t cPwrLimit20H; /* MCS5~8 */
	int8_t cPwrLimit40L; /* MCS0~4 */
	int8_t cPwrLimit40H; /* MCS5~9 */
	int8_t cPwrLimit80L; /* MCS0~4 */
	int8_t cPwrLimit80H; /* MCS5~9 */
	int8_t cPwrLimit160L; /* MCS0~4 */
	int8_t cPwrLimit160H; /* MCS5~9 */

	uint8_t ucFlag; /*Not used in driver*/
	uint8_t aucReserved[1];
};

struct CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT {
	uint16_t u2CountryCode;
	uint8_t ucCountryFlag; /*Not used in driver*/
	uint8_t ucNum; /*Numbers of channel to set power limit*/
	uint8_t ucTempVersion; /*Temp use for 160nc power limit implementation*/
	uint8_t aucReserved[3];
	struct CMD_CHANNEL_POWER_LIMIT
		rChannelPowerLimit[1]; /*Channel power limit entries to be set*/
};

#if (CFG_SUPPORT_SINGLE_SKU == 1)
struct CMD_CHANNEL_POWER_LIMIT_V2 {
	uint8_t ucCentralCh;
	uint8_t ucReserved[3];

	uint8_t tx_pwr_dsss_cck;
	uint8_t tx_pwr_dsss_bpsk;

	uint8_t tx_pwr_ofdm_bpsk; /* 6M, 9M */
	uint8_t tx_pwr_ofdm_qpsk; /* 12M, 18M */
	uint8_t tx_pwr_ofdm_16qam; /* 24M, 36M */
	uint8_t tx_pwr_ofdm_48m;
	uint8_t tx_pwr_ofdm_54m;

	uint8_t tx_pwr_ht20_bpsk; /* MCS0*/
	uint8_t tx_pwr_ht20_qpsk; /* MCS1, MCS2*/
	uint8_t tx_pwr_ht20_16qam; /* MCS3, MCS4*/
	uint8_t tx_pwr_ht20_mcs5; /* MCS5*/
	uint8_t tx_pwr_ht20_mcs6; /* MCS6*/
	uint8_t tx_pwr_ht20_mcs7; /* MCS7*/

	uint8_t tx_pwr_ht40_bpsk; /* MCS0*/
	uint8_t tx_pwr_ht40_qpsk; /* MCS1, MCS2*/
	uint8_t tx_pwr_ht40_16qam; /* MCS3, MCS4*/
	uint8_t tx_pwr_ht40_mcs5; /* MCS5*/
	uint8_t tx_pwr_ht40_mcs6; /* MCS6*/
	uint8_t tx_pwr_ht40_mcs7; /* MCS7*/
	uint8_t tx_pwr_ht40_mcs32; /* MCS32*/

	uint8_t tx_pwr_vht20_bpsk; /* MCS0*/
	uint8_t tx_pwr_vht20_qpsk; /* MCS1, MCS2*/
	uint8_t tx_pwr_vht20_16qam; /* MCS3, MCS4*/
	uint8_t tx_pwr_vht20_64qam; /* MCS5, MCS6*/
	uint8_t tx_pwr_vht20_mcs7;
	uint8_t tx_pwr_vht20_mcs8;
	uint8_t tx_pwr_vht20_mcs9;

	uint8_t tx_pwr_vht_40;
	uint8_t tx_pwr_vht_80;
	uint8_t tx_pwr_vht_160c;
	uint8_t tx_pwr_vht_160nc;
	uint8_t tx_pwr_lg_40;
	uint8_t tx_pwr_lg_80;

	uint8_t tx_pwr_1ss_delta;
	uint8_t ucReserved_1[2];
};

struct CMD_SET_COUNTRY_CHANNEL_POWER_LIMIT_V2 {
	uint8_t ucNum;
	uint8_t eband; /*ENUM_BAND_T*/
	uint8_t usReserved[2];
	uint32_t countryCode;
	struct CMD_CHANNEL_POWER_LIMIT_V2 rChannelPowerLimit[0];
};

#define BF_TX_PWR_LIMIT_SECTION_NUM 17
#define BF_TX_PWR_LIMIT_ELEMENT_NUM 10
#define TX_PWR_LIMIT_SECTION_NUM 9
#define TX_PWR_LIMIT_ELEMENT_NUM 10
#define TX_PWR_LIMIT_COUNTRY_STR_MAX_LEN 4
#define TX_PWR_LIMIT_MAX_VAL 63

#define POWER_LIMIT_SKU_CCK_NUM 4
#define POWER_LIMIT_SKU_OFDM_NUM 8
#define POWER_LIMIT_SKU_HT20_NUM 8
#define POWER_LIMIT_SKU_HT40_NUM 9
#define POWER_LIMIT_SKU_VHT20_NUM 10
#define POWER_LIMIT_SKU_VHT40_NUM 10
#define POWER_LIMIT_SKU_VHT80_NUM 10
#define POWER_LIMIT_SKU_VHT160_NUM 10
#define SINGLE_SKU_PARAM_NUM \
	(POWER_LIMIT_SKU_CCK_NUM + \
	 POWER_LIMIT_SKU_OFDM_NUM + \
	 POWER_LIMIT_SKU_HT20_NUM + \
	 POWER_LIMIT_SKU_HT40_NUM + \
	 POWER_LIMIT_SKU_VHT20_NUM + \
	 POWER_LIMIT_SKU_VHT40_NUM + \
	 POWER_LIMIT_SKU_VHT80_NUM + \
	 POWER_LIMIT_SKU_VHT160_NUM)

#define POWER_LIMIT_TXBF_BACKOFF_PARAM_NUM 6

struct CHANNEL_TX_PWR_LIMIT {
	uint8_t ucChannel;
	int8_t rTxPwrLimitValue[TX_PWR_LIMIT_SECTION_NUM]
		[TX_PWR_LIMIT_ELEMENT_NUM];
	int8_t rTxBfBackoff[POWER_LIMIT_TXBF_BACKOFF_PARAM_NUM];
};

struct TX_PWR_LIMIT_DATA {
	uint32_t countryCode;
	uint32_t ucChNum;
	struct CHANNEL_TX_PWR_LIMIT *rChannelTxPwrLimit;
};

struct SKU_TABLE_TYPE {
	int8_t i1PwrLimit[SINGLE_SKU_PARAM_NUM];
};

struct CHANNEL_POWER_LIMIT_PER_RATE {
	uint8_t ucCentralCh;
	struct SKU_TABLE_TYPE aucTxPwrLimit;
};

struct CMD_SET_COUNTRY_TX_POWER_LIMIT_PER_RATE {
	uint8_t ucCmdVer;
	uint8_t aucPadding0[1];
	uint16_t u2CmdLen;
	uint8_t ucNum;
	uint8_t eBand;
	uint8_t bCmdFinished;
	uint8_t aucPadding1[1];
	uint32_t countryCode;
	uint8_t aucPadding2[32];
	struct CHANNEL_POWER_LIMIT_PER_RATE rChannelPowerLimit[0];
};

struct CMD_TXPWR_TXBF_CHANNEL_BACKOFF {
	uint8_t ucCentralCh;
	uint8_t aucPadding0[1];
	int8_t aucTxBfBackoff[POWER_LIMIT_TXBF_BACKOFF_PARAM_NUM];
};

#define CMD_POWER_LIMIT_TABLE_SUPPORT_CHANNEL_NUM 64
struct CMD_TXPWR_TXBF_SET_BACKOFF {
	uint8_t ucCmdVer;
	uint8_t aucPadding0[1];
	uint16_t u2CmdLen;
	uint8_t ucNum;
	uint8_t ucBssIdx;
	uint8_t aucPadding1[2];
	uint8_t aucPadding2[32];
	struct CMD_TXPWR_TXBF_CHANNEL_BACKOFF
		rChannelTxBfBackoff[CMD_POWER_LIMIT_TABLE_SUPPORT_CHANNEL_NUM];
};
#endif

#endif

/* CMD_SET_IP_ADDRESS */
struct IPV4_NETWORK_ADDRESS {
	uint8_t aucIpAddr[4];
};

struct CMD_SET_NETWORK_ADDRESS_LIST {
	uint8_t ucBssIndex;
	uint8_t ucAddressCount;
	uint8_t ucReserved[2];
	struct IPV4_NETWORK_ADDRESS arNetAddress[1];
};

struct PATTERN_DESCRIPTION {
	uint8_t fgCheckBcA1;
	uint8_t fgCheckMcA1;
	uint8_t ePatternHeader;
	uint8_t fgAndOp;
	uint8_t fgNotOp;
	uint8_t ucPatternMask;
	uint16_t u2PatternOffset;
	uint8_t aucPattern[8];
};

struct CMD_RAW_PATTERN_CONFIGURATION {
	struct PATTERN_DESCRIPTION arPatternDesc[4];
};

struct CMD_PATTERN_FUNC_CONFIG {
	u_int8_t fgBcA1En;
	u_int8_t fgMcA1En;
	u_int8_t fgBcA1MatchDrop;
	u_int8_t fgMcA1MatchDrop;
};

struct EVENT_TX_DONE {
	uint8_t ucPacketSeq;
	uint8_t ucStatus;
	uint16_t u2SequenceNumber;

	uint8_t ucWlanIndex;
	uint8_t ucTxCount;
	uint16_t u2TxRate;

	uint8_t ucFlag;
	uint8_t ucTid;
	uint8_t ucRspRate;
	uint8_t ucRateTableIdx;

	uint8_t ucBandwidth;
	uint8_t ucTxPower;
	uint8_t ucFlushReason;
	uint8_t aucReserved0[1];

	uint32_t u4TxDelay;
	uint32_t u4Timestamp;
	uint32_t u4AppliedFlag;

	uint8_t aucRawTxS[28];

	uint8_t aucReserved1[32];
};

enum ENUM_TXS_APPLIED_FLAG {
	TX_FRAME_IN_AMPDU_FORMAT = 0,
	TX_FRAME_EXP_BF,
	TX_FRAME_IMP_BF,
	TX_FRAME_PS_BIT
};

enum ENUM_TXS_CONTROL_FLAG {
	TXS_WITH_ADVANCED_INFO = 0,
	TXS_IS_EXIST
};

#if (CFG_SUPPORT_DFS_MASTER == 1)
enum ENUM_DFS_CTRL {
	RDD_STOP = 0,
	RDD_START,
	RDD_DET_MODE,
	RDD_RADAR_EMULATE,
	RDD_START_TXQ = 20
};
#endif

struct CMD_BSS_ACTIVATE_CTRL {
	uint8_t ucBssIndex;
	uint8_t ucActive;
	uint8_t ucNetworkType;
	uint8_t ucOwnMacAddrIndex;
	uint8_t aucBssMacAddr[6];
	uint8_t ucBMCWlanIndex;
	uint8_t ucReserved;
};

struct CMD_SET_BSS_RLM_PARAM {
	uint8_t ucBssIndex;
	uint8_t ucRfBand;
	uint8_t ucPrimaryChannel;
	uint8_t ucRfSco;
	uint8_t ucErpProtectMode;
	uint8_t ucHtProtectMode;
	uint8_t ucGfOperationMode;
	uint8_t ucTxRifsMode;
	uint16_t u2HtOpInfo3;
	uint16_t u2HtOpInfo2;
	uint8_t ucHtOpInfo1;
	uint8_t ucUseShortPreamble;
	uint8_t ucUseShortSlotTime;
	uint8_t ucVhtChannelWidth;
	uint8_t ucVhtChannelFrequencyS1;
	uint8_t ucVhtChannelFrequencyS2;
	uint16_t u2VhtBasicMcsSet;
	uint8_t ucNss;
};

struct CMD_SET_BSS_INFO {
	uint8_t ucBssIndex;
	uint8_t ucConnectionState;
	uint8_t ucCurrentOPMode;
	uint8_t ucSSIDLen;
	uint8_t aucSSID[32];
	uint8_t aucBSSID[6];
	uint8_t ucIsQBSS;
	uint8_t ucReserved1;
	uint16_t u2OperationalRateSet;
	uint16_t u2BSSBasicRateSet;
	uint8_t ucStaRecIdxOfAP;
	uint16_t u2HwDefaultFixedRateCode;
	uint8_t ucNonHTBasicPhyType;	/* For Slot Time and CWmin */
	uint8_t ucAuthMode;
	uint8_t ucEncStatus;
	uint8_t ucPhyTypeSet;
	uint8_t ucWapiMode;
	uint8_t ucIsApMode;
	uint8_t ucBMCWlanIndex;
	uint8_t ucHiddenSsidMode;
	uint8_t ucDisconnectDetectTh;
	uint32_t u4PrivateData;
	struct CMD_SET_BSS_RLM_PARAM rBssRlmParam;
	uint8_t ucDBDCBand;
	uint8_t ucWmmSet;
	uint8_t  ucDBDCAction;
	uint8_t  ucNss;
	uint8_t aucReserved[20];
};

enum ENUM_RTS_POLICY {
	RTS_POLICY_AUTO,
	RTS_POLICY_STATIC_BW,
	RTS_POLICY_DYNAMIC_BW,
	RTS_POLICY_LEGACY,
	RTS_POLICY_NO_RTS
};

struct CMD_UPDATE_STA_RECORD {
	uint8_t ucStaIndex;
	uint8_t ucStaType;
	/* This field should assign at create
	 * and keep consistency for update usage
	 */
	uint8_t aucMacAddr[MAC_ADDR_LEN];

	uint16_t u2AssocId;
	uint16_t u2ListenInterval;
	/* This field should assign at create
	 * and keep consistency for update usage
	 */
	uint8_t ucBssIndex;
	uint8_t ucDesiredPhyTypeSet;
	uint16_t u2DesiredNonHTRateSet;

	uint16_t u2BSSBasicRateSet;
	uint8_t ucIsQoS;
	uint8_t ucIsUapsdSupported;
	uint8_t ucStaState;
	uint8_t ucMcsSet;
	uint8_t ucSupMcs32;
	uint8_t aucReserved1[1];

	uint8_t aucRxMcsBitmask[10];
	uint16_t u2RxHighestSupportedRate;
	uint32_t u4TxRateInfo;

	uint16_t u2HtCapInfo;
	uint16_t u2HtExtendedCap;
	uint32_t u4TxBeamformingCap;

	uint8_t ucAmpduParam;
	uint8_t ucAselCap;
	uint8_t ucRCPI;
	uint8_t ucNeedResp;
	/* b0~3: Trigger enabled, b4~7: Delivery enabled */
	uint8_t ucUapsdAc;
	/* 0: all, 1: max 2, 2: max 4, 3: max 6 */
	uint8_t ucUapsdSp;
	/* This field should assign at create
	 * and keep consistency for update usage
	 */
	uint8_t ucWlanIndex;
	/* This field should assign at create
	 * and keep consistency for update usage
	 */
	uint8_t ucBMCWlanIndex;

	uint32_t u4VhtCapInfo;
	uint16_t u2VhtRxMcsMap;
	uint16_t u2VhtRxHighestSupportedDataRate;
	uint16_t u2VhtTxMcsMap;
	uint16_t u2VhtTxHighestSupportedDataRate;
	/* 0: auto 1: Static BW 2: Dynamic BW 3: Legacy 7: WoRts */
	uint8_t ucRtsPolicy;
	/* VHT operating mode, bit 7: Rx NSS Type,
	 * bit 4-6, Rx NSS, bit 0-1: Channel Width
	 */
	uint8_t ucVhtOpMode;

	uint8_t ucTrafficDataType;	/* 0: auto 1: data 2: video 3: voice */
	uint8_t ucTxGfMode;
	uint8_t ucTxSgiMode;
	uint8_t ucTxStbcMode;
	uint16_t u2HwDefaultFixedRateCode;
	uint8_t ucTxAmpdu;
	uint8_t ucRxAmpdu;
	uint32_t u4FixedPhyRate;	/* */
	uint16_t u2MaxLinkSpeed;	/* unit is 0.5 Mbps */
	uint16_t u2MinLinkSpeed;

	uint32_t u4Flags;

	uint8_t ucTxBaSize;
	uint8_t ucRxBaSize;
	uint8_t aucReserved3[2];

	struct TXBF_PFMU_STA_INFO rTxBfPfmuInfo;

	uint8_t ucTxAmsduInAmpdu;
	uint8_t ucRxAmsduInAmpdu;
	uint8_t aucReserved5[2];

	uint32_t u4TxMaxAmsduInAmpduLen;
	/* UINT_8 aucReserved4[30]; */
};

struct CMD_REMOVE_STA_RECORD {
	uint8_t ucActionType;
	uint8_t ucStaIndex;
	uint8_t ucBssIndex;
	uint8_t ucReserved;
};

struct CMD_INDICATE_PM_BSS_CREATED {
	uint8_t ucBssIndex;
	uint8_t ucDtimPeriod;
	uint16_t u2BeaconInterval;
	uint16_t u2AtimWindow;
	uint8_t aucReserved[2];
};

struct CMD_INDICATE_PM_BSS_CONNECTED {
	uint8_t ucBssIndex;
	uint8_t ucDtimPeriod;
	uint16_t u2AssocId;
	uint16_t u2BeaconInterval;
	uint16_t u2AtimWindow;
	uint8_t fgIsUapsdConnection;
	uint8_t ucBmpDeliveryAC;
	uint8_t ucBmpTriggerAC;
	uint8_t aucReserved[1];
};

struct CMD_INDICATE_PM_BSS_ABORT {
	uint8_t ucBssIndex;
	uint8_t aucReserved[3];
};

struct CMD_BEACON_TEMPLATE_UPDATE {
	/* 0: update randomly,
	 * 1: update all,
	 * 2: delete all (1 and 2 will update directly without search)
	 */
	uint8_t ucUpdateMethod;
	uint8_t ucBssIndex;
	uint8_t aucReserved[2];
	uint16_t u2Capability;
	uint16_t u2IELen;
	uint8_t aucIE[MAX_IE_LENGTH];
};

struct CMD_SET_WMM_PS_TEST_STRUCT {
	uint8_t ucBssIndex;
	/* b0~3: trigger-en AC0~3. b4~7: delivery-en AC0~3 */
	uint8_t bmfgApsdEnAc;
	/* enter PS immediately without 5 second guard after connected */
	uint8_t ucIsEnterPsAtOnce;
	/* not to trigger UC on beacon TIM is matched (under U-APSD) */
	uint8_t ucIsDisableUcTrigger;
};

struct GSCN_CHANNEL_INFO {
	uint8_t ucBand;
	uint8_t ucChannelNumber; /* Channel Number */
	uint8_t ucPassive;       /* 0:active, 1:passive scan; ignored for DFS */
	uint8_t aucReserved[1];
	uint32_t u4DwellTimeMs;  /* dwell time hint */
};

enum ENUM_WIFI_BAND {
	WIFI_BAND_UNSPECIFIED,
	WIFI_BAND_BG = 1,              /* 2.4 GHz */
	WIFI_BAND_A = 2,               /* 5 GHz without DFS */
	WIFI_BAND_A_DFS = 4,           /* 5 GHz DFS only */
	WIFI_BAND_A_WITH_DFS = 6,      /* 5 GHz with DFS */
	WIFI_BAND_ABG = 3,             /* 2.4 GHz + 5 GHz; no DFS */
	WIFI_BAND_ABG_WITH_DFS = 7,    /* 2.4 GHz + 5 GHz with DFS */
};

struct GSCAN_BUCKET {
	uint16_t u2BucketIndex;  /* bucket index, 0 based */
	/* desired period, in millisecond;
	 * if this is too low, the firmware should choose to generate
	 * results as fast as it can instead of failing the command
	 */
	uint8_t ucBucketFreqMultiple;
	/* report_events semantics -
	 *  This is a bit field; which defines following bits -
	 *  REPORT_EVENTS_EACH_SCAN
	 *      report a scan completion event after scan. If this is not set
	 *      then scan completion events should be reported if
	 *      report_threshold_percent or report_threshold_num_scans is
	 *				    reached.
	 *  REPORT_EVENTS_FULL_RESULTS
	 *      forward scan results (beacons/probe responses + IEs)
	 *      in real time to HAL, in addition to completion events
	 *      Note: To keep backward compatibility, fire completion
	 *	events regardless of REPORT_EVENTS_EACH_SCAN.
	 *  REPORT_EVENTS_NO_BATCH
	 *      controls if scans for this bucket should be placed in the
	 *      history buffer
	 */
	uint8_t ucReportFlag;
	uint8_t ucMaxBucketFreqMultiple; /* max_period / base_period */
	uint8_t ucStepCount;
	uint8_t ucNumChannels;
	uint8_t aucReserved[1];
	enum ENUM_WIFI_BAND
	eBand; /* when UNSPECIFIED, use channel list */
	/* channels to scan; these may include DFS channels */
	struct GSCN_CHANNEL_INFO arChannelList[8];
};

struct CMD_GSCN_REQ {
	uint8_t ucFlags;
	uint8_t ucNumScnToCache;
	uint8_t aucReserved[2];
	uint32_t u4BufferThreshold;
	uint32_t u4BasePeriod; /* base timer period in ms */
	uint32_t u4NumBuckets;
	/* number of APs to store in each scan in the */
	uint32_t u4MaxApPerScan;
	/* BSSID/RSSI history buffer (keep the highest RSSI APs) */
	struct GSCAN_BUCKET arBucket[8];
};

struct CMD_GSCN_SCN_COFIG {
	uint8_t ucNumApPerScn;       /* GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN */
	uint32_t u4BufferThreshold;  /* GSCAN_ATTRIBUTE_REPORT_THRESHOLD */
	uint32_t u4NumScnToCache;    /* GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE */
};

/* Definition for CHANNEL_INFO.ucBand:
 * 0:       Reserved
 * 1:       BAND_2G4
 * 2:       BAND_5G
 * Others:  Reserved
 */
struct CHANNEL_INFO {
	uint8_t ucBand;
	uint8_t ucChannelNum;
};

struct CMD_SCAN_REQ {
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;
	uint8_t ucScanType;
	/* BIT(0) wildcard / BIT(1) P2P-wildcard / BIT(2) specific */
	uint8_t ucSSIDType;
	uint8_t ucSSIDLength;
	uint8_t ucNumProbeReq;
	uint16_t u2ChannelMinDwellTime;
	uint16_t u2ChannelDwellTime;
	uint16_t u2TimeoutValue;
	uint8_t aucSSID[32];
	uint8_t ucChannelType;
	uint8_t ucChannelListNum;
	uint8_t aucReserved[2];
	struct CHANNEL_INFO arChannelList[32];
	uint16_t u2IELen;
	uint8_t aucIE[MAX_IE_LENGTH];
};

struct CMD_SCAN_REQ_V2 {
	uint8_t ucSeqNum;
	uint8_t ucBssIndex;
	uint8_t ucScanType;
	uint8_t ucSSIDType;
	uint8_t ucSSIDNum;
	uint8_t ucNumProbeReq;
	uint8_t ucScnFuncMask;
	uint8_t auVersion[1];
	struct PARAM_SSID arSSID[4];
	uint16_t u2ProbeDelayTime;
	uint16_t u2ChannelDwellTime;
	uint16_t u2TimeoutValue;
	uint8_t ucChannelType;
	uint8_t ucChannelListNum;			/*total 160*/
	struct CHANNEL_INFO arChannelList[32];		/*total 160+64=224*/
	uint16_t u2IELen;				/*total 226*/
	uint8_t aucIE[MAX_IE_LENGTH];			/*total 826*/
	uint8_t ucChannelListExtNum;
	uint8_t ucSSIDExtNum;
	uint16_t u2ChannelMinDwellTime;
	struct CHANNEL_INFO arChannelListExtend[32];
	struct PARAM_SSID arSSIDExtend[6];
	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint8_t aucRandomMac[MAC_ADDR_LEN];
	uint8_t aucPadding_2[64];
};

struct CMD_SCAN_CANCEL {
	uint8_t ucSeqNum;
	uint8_t ucIsExtChannel;	/* For P2P channel extension. */
	uint8_t aucReserved[2];
};

/* 20150107  Daniel Added complete channels number in the scan done event */
/* before*/
/*
 * typedef struct EVENT_SCAN_DONE {
 *	UINT_8          ucSeqNum;
 *	UINT_8          ucSparseChannelValid;
 *	CHANNEL_INFO_T  rSparseChannel;
 * } EVENT_SCAN_DONE, *P_EVENT_SCAN_DONE;
 */
/* after */

#define EVENT_SCAN_DONE_CHANNEL_NUM_MAX 64
struct EVENT_SCAN_DONE {
	uint8_t ucSeqNum;
	uint8_t ucSparseChannelValid;
	struct CHANNEL_INFO rSparseChannel;
	/*scan done version #2 */
	uint8_t ucCompleteChanCount;
	uint8_t ucCurrentState;
	uint8_t ucScanDoneVersion;
	/*scan done version #3 */
	uint8_t ucReserved;
	uint32_t u4ScanDurBcnCnt;
	uint8_t fgIsPNOenabled;
	uint8_t aucReserving[3];
	/*channel idle count # Mike */
	uint8_t ucSparseChannelArrayValidNum;
	uint8_t aucReserved[3];
	uint8_t aucChannelNum[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];
	/* Idle format for au2ChannelIdleTime */
	/* 0: first bytes: idle time(ms) 2nd byte: dwell time(ms) */
	/* 1: first bytes: idle time(8ms) 2nd byte: dwell time(8ms) */
	/* 2: dwell time (16us) */
	uint16_t au2ChannelIdleTime[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];
	/* Beacon and Probe Response Count in each Channel  */
	uint8_t aucChannelBAndPCnt[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];
	/* Mdrdy Count in each Channel  */
	uint8_t aucChannelMDRDYCnt[EVENT_SCAN_DONE_CHANNEL_NUM_MAX];

};

struct CMD_BATCH_REQ {
	uint8_t ucSeqNum;
	uint8_t ucNetTypeIndex;
	uint8_t ucCmd;		/* Start/ Stop */
	/* an integer number of scans per batch */
	uint8_t ucMScan;
	/* an integer number of the max AP to remember per scan */
	uint8_t ucBestn;
	/* an integer number of highest-strength AP for which we'd */
	uint8_t ucRtt;
	/* like approximate distance reported */
	uint8_t ucChannel;	/* channels */
	uint8_t ucChannelType;
	uint8_t ucChannelListNum;
	uint8_t aucReserved[3];
	uint32_t u4Scanfreq;	/* an integer number of seconds between scans */
	struct CHANNEL_INFO arChannelList[32];	/* channels */
};

struct EVENT_BATCH_RESULT_ENTRY {
	uint8_t aucBssid[MAC_ADDR_LEN];
	uint8_t aucSSID[ELEM_MAX_LEN_SSID];
	uint8_t ucSSIDLen;
	int8_t cRssi;
	uint32_t ucFreq;
	uint32_t u4Age;
	uint32_t u4Dist;
	uint32_t u4Distsd;
};

struct EVENT_BATCH_RESULT {
	uint8_t ucScanCount;
	uint8_t aucReserved[3];
	/* Must be the same with SCN_BATCH_STORE_MAX_NUM */
	struct EVENT_BATCH_RESULT_ENTRY arBatchResult[12];
};

struct CMD_CH_PRIVILEGE {
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
	uint8_t ucAction;
	uint8_t ucPrimaryChannel;
	uint8_t ucRfSco;
	uint8_t ucRfBand;
	uint8_t ucRfChannelWidth;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	uint8_t ucReqType;
	uint8_t ucDBDCBand;
	uint8_t aucReserved;
	uint32_t u4MaxInterval;	/* In unit of ms */
	uint8_t aucReserved2[8];
};

struct CMD_TX_PWR {
	int8_t cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
	int8_t cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	int8_t acReserved[2];

	int8_t cTxPwr2G4OFDM_BPSK;
	int8_t cTxPwr2G4OFDM_QPSK;
	int8_t cTxPwr2G4OFDM_16QAM;
	int8_t cTxPwr2G4OFDM_Reserved;
	int8_t cTxPwr2G4OFDM_48Mbps;
	int8_t cTxPwr2G4OFDM_54Mbps;

	int8_t cTxPwr2G4HT20_BPSK;
	int8_t cTxPwr2G4HT20_QPSK;
	int8_t cTxPwr2G4HT20_16QAM;
	int8_t cTxPwr2G4HT20_MCS5;
	int8_t cTxPwr2G4HT20_MCS6;
	int8_t cTxPwr2G4HT20_MCS7;

	int8_t cTxPwr2G4HT40_BPSK;
	int8_t cTxPwr2G4HT40_QPSK;
	int8_t cTxPwr2G4HT40_16QAM;
	int8_t cTxPwr2G4HT40_MCS5;
	int8_t cTxPwr2G4HT40_MCS6;
	int8_t cTxPwr2G4HT40_MCS7;

	int8_t cTxPwr5GOFDM_BPSK;
	int8_t cTxPwr5GOFDM_QPSK;
	int8_t cTxPwr5GOFDM_16QAM;
	int8_t cTxPwr5GOFDM_Reserved;
	int8_t cTxPwr5GOFDM_48Mbps;
	int8_t cTxPwr5GOFDM_54Mbps;

	int8_t cTxPwr5GHT20_BPSK;
	int8_t cTxPwr5GHT20_QPSK;
	int8_t cTxPwr5GHT20_16QAM;
	int8_t cTxPwr5GHT20_MCS5;
	int8_t cTxPwr5GHT20_MCS6;
	int8_t cTxPwr5GHT20_MCS7;

	int8_t cTxPwr5GHT40_BPSK;
	int8_t cTxPwr5GHT40_QPSK;
	int8_t cTxPwr5GHT40_16QAM;
	int8_t cTxPwr5GHT40_MCS5;
	int8_t cTxPwr5GHT40_MCS6;
	int8_t cTxPwr5GHT40_MCS7;
};

struct CMD_TX_AC_PWR {
	int8_t ucBand;
#if 0
	int8_t c11AcTxPwr_BPSK;
	int8_t c11AcTxPwr_QPSK;
	int8_t c11AcTxPwr_16QAM;
	int8_t c11AcTxPwr_MCS5_MCS6;
	int8_t c11AcTxPwr_MCS7;
	int8_t c11AcTxPwr_MCS8;
	int8_t c11AcTxPwr_MCS9;
	int8_t c11AcTxPwrVht40_OFFSET;
	int8_t c11AcTxPwrVht80_OFFSET;
	int8_t c11AcTxPwrVht160_OFFSET;
#else
	struct AC_PWR_SETTING_STRUCT rAcPwr;
#endif
};

struct CMD_RSSI_PATH_COMPASATION {
	int8_t c2GRssiCompensation;
	int8_t c5GRssiCompensation;
};
struct CMD_5G_PWR_OFFSET {
	int8_t cOffsetBand0;	/* 4.915-4.980G */
	int8_t cOffsetBand1;	/* 5.000-5.080G */
	int8_t cOffsetBand2;	/* 5.160-5.180G */
	int8_t cOffsetBand3;	/* 5.200-5.280G */
	int8_t cOffsetBand4;	/* 5.300-5.340G */
	int8_t cOffsetBand5;	/* 5.500-5.580G */
	int8_t cOffsetBand6;	/* 5.600-5.680G */
	int8_t cOffsetBand7;	/* 5.700-5.825G */
};

struct CMD_PWR_PARAM {
	uint32_t au4Data[28];
	uint32_t u4RefValue1;
	uint32_t u4RefValue2;
};

struct CMD_PHY_PARAM {
	uint8_t aucData[144];	/* eFuse content */
};

struct CMD_AUTO_POWER_PARAM {
	/* 0: Disable 1: Enalbe 0x10: Change parameters */
	uint8_t ucType;
	uint8_t ucBssIndex;
	uint8_t aucReserved[2];
	uint8_t aucLevelRcpiTh[3];
	uint8_t aucReserved2[1];
	int8_t aicLevelPowerOffset[3];	/* signed, in unit of 0.5dBm */
	uint8_t aucReserved3[1];
	uint8_t aucReserved4[8];
};

/*for WMMAC, CMD_ID_UPDATE_AC_PARAMS*/
struct CMD_UPDATE_AC_PARAMS {
	uint8_t  ucAcIndex; /*0 ~3, from AC0 to AC3*/
	uint8_t  ucBssIdx;  /*no use*/
	/* if 0, disable ACM for ACx specified by
	** ucAcIndex, otherwise in unit of 32us
	*/
	uint16_t u2MediumTime;
	/* rate to be used to tx packet with priority
	** ucAcIndex , unit: bps
	*/
	uint32_t u4PhyRate;
	uint16_t u2EDCALifeTime; /* msdu life time for this TC, unit: 2TU */
	/* if we use fix rate to tx packets, should tell
	** firmware the limited retries
	*/
	uint8_t ucRetryCount;
	uint8_t aucReserved[5];
};

/* S56 Traffic Stream Metrics */
struct CMD_SET_TSM_STATISTICS_REQUEST {
	uint8_t ucEnabled; /* 0, disable; 1, enable; */
	uint8_t ucBssIdx; /* always AIS Bss index now */
	uint8_t ucAcIndex; /* wmm ac index, the statistics should be on this TC
			      */
	uint8_t ucTid;

	uint8_t aucPeerAddr
		[MAC_ADDR_LEN]; /* packet to the target address to be mesured */
	uint8_t ucBin0Range;
	uint8_t aucReserved[3];

	/* if this variable is 0, followed variables are meaningless
	** only report once for a same trigger condition in this time frame
	** for triggered mode: bit(0):average, bit(1):consecutive,
	** bit(2):delay
	*/
	uint8_t ucTriggerCondition;
	uint8_t ucAvgErrThreshold;
	uint8_t ucConsecutiveErrThreshold;
	uint8_t ucDelayThreshold;
	uint8_t ucMeasureCount;
	uint8_t ucTriggerTimeout; /* unit: 100 TU*/
};

struct CMD_GET_TSM_STATISTICS {
	uint8_t ucBssIdx; /* always AIS Bss now */
	/* wmm ac index, the statistics should be on this TC or TS */
	uint8_t ucAcIndex;
	uint8_t ucTid; /* */

	uint8_t aucPeerAddr
		[MAC_ADDR_LEN]; /* indicating the RA for the measured frames */
	/* for triggered mode: bit(0):average, bit(1):consecutive,
	** bit(2):delay
	*/
	uint8_t ucReportReason;
	uint16_t u2Reserved;

	uint32_t u4PktTxDoneOK;
	uint32_t u4PktDiscard; /* u2PktTotal - u2PktTxDoneOK */
	uint32_t u4PktFail; /* failed count for exceeding retry limit */
	uint32_t u4PktRetryTxDoneOK;
	uint32_t u4PktQosCfPollLost;

	/* 802.11k - Average Packet Transmission delay for all packets per this
	** TC or TS
	*/
	uint32_t u4AvgPktTxDelay;
	/* 802.11k - Average Packet Queue Delay */
	uint32_t u4AvgPktQueueDelay;
	uint64_t u8StartTime; /* represented by TSF */
	/* sum of packets whose packet tx delay is less than Bi (i=0~6) range
	** value(unit: TU)
	*/
	uint32_t au4PktCntBin[6];
};

struct CMD_DBDC_SETTING {
	uint8_t ucDbdcEn;
	uint8_t ucWmmBandBitmap;
	uint8_t ucUpdateSettingNextChReq;
	uint8_t aucPadding0[1];
	uint8_t ucCmdVer;
	uint8_t aucPadding1[1];
	uint16_t u2CmdLen;
	uint8_t ucPrimaryChannel;
	uint8_t ucWmmQueIdx;
	uint8_t aucPadding2[2];
	uint8_t aucPadding3[24];
};

struct EVENT_CH_PRIVILEGE {
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
	uint8_t ucStatus;
	uint8_t ucPrimaryChannel;
	uint8_t ucRfSco;
	uint8_t ucRfBand;
	uint8_t ucRfChannelWidth;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	uint8_t ucReqType;
	uint8_t ucDBDCBand;
	uint8_t aucReserved;
	uint32_t u4GrantInterval;	/* In unit of ms */
	uint8_t aucReserved2[8];
};

#if (CFG_SUPPORT_DFS_MASTER == 1)
struct LONG_PULSE_BUFFER {
	uint32_t u4LongStartTime;
	uint16_t u2LongPulseWidth;
};

struct PERIODIC_PULSE_BUFFER {
	uint32_t u4PeriodicStartTime;
	uint16_t u2PeriodicPulseWidth;
	int16_t i2PeriodicPulsePower;
};

struct EVENT_RDD_REPORT {
	/*0: Only report radar detected;   1:  Add parameter reports*/
	uint8_t ucRadarReportMode;
	uint8_t ucRddIdx;
	uint8_t ucLongDetected;
	uint8_t ucPeriodicDetected;
	uint8_t ucLPBNum;
	uint8_t ucPPBNum;
	uint8_t ucLPBPeriodValid;
	uint8_t ucLPBWidthValid;
	uint8_t ucPRICountM1;
	uint8_t ucPRICountM1TH;
	uint8_t ucPRICountM2;
	uint8_t ucPRICountM2TH;
	uint32_t u4PRI1stUs;
	struct LONG_PULSE_BUFFER arLpbContent[LPB_SIZE];
	struct PERIODIC_PULSE_BUFFER arPpbContent[PPB_SIZE];
};
#endif

#if (CFG_WOW_SUPPORT == 1)
/* event of wake up reason */
struct _EVENT_WAKEUP_REASON_INFO {
	uint8_t reason;
	uint8_t aucReserved[3];
};
#endif

struct EVENT_BSS_BEACON_TIMEOUT {
	uint8_t ucBssIndex;
	uint8_t ucReasonCode;
	uint8_t aucReserved[2];
};

struct EVENT_STA_AGING_TIMEOUT {
	uint8_t ucStaRecIdx;
	uint8_t aucReserved[3];
};

struct EVENT_NOA_TIMING {
	uint8_t ucIsInUse;	/* Indicate if this entry is in use or not */
	uint8_t ucCount;		/* Count */
	uint8_t aucReserved[2];

	uint32_t u4Duration;	/* Duration */
	uint32_t u4Interval;	/* Interval */
	uint32_t u4StartTime;	/* Start Time */
};

struct EVENT_UPDATE_NOA_PARAMS {
	uint8_t ucBssIndex;
	uint8_t aucReserved[2];
	uint8_t ucEnableOppPS;
	uint16_t u2CTWindow;

	uint8_t ucNoAIndex;
	uint8_t ucNoATimingCount;	/* Number of NoA Timing */
	struct EVENT_NOA_TIMING
		arEventNoaTiming[8 /*P2P_MAXIMUM_NOA_COUNT */];
};

struct EVENT_AP_OBSS_STATUS {
	uint8_t ucBssIndex;
	uint8_t ucObssErpProtectMode;
	uint8_t ucObssHtProtectMode;
	uint8_t ucObssGfOperationMode;
	uint8_t ucObssRifsOperationMode;
	uint8_t ucObssBeaconForcedTo20M;
	uint8_t aucReserved[2];
};

struct EVENT_DEBUG_MSG {
	uint16_t u2DebugMsgId;
	uint8_t ucMsgType;
	uint8_t ucFlags;		/* unused */
	uint32_t u4Value;	/* memory addre or ... */
	uint16_t u2MsgSize;
	uint8_t aucReserved0[2];
	uint8_t aucMsg[1];
};

struct CMD_EDGE_TXPWR_LIMIT {
	int8_t cBandEdgeMaxPwrCCK;
	int8_t cBandEdgeMaxPwrOFDM20;
	int8_t cBandEdgeMaxPwrOFDM40;
	int8_t cBandEdgeMaxPwrOFDM80;
};

struct CMD_POWER_OFFSET {
	uint8_t ucBand;		/*1:2.4G ;  2:5G */
	/*the max num subband is 5G, devide with 8 subband */
	uint8_t ucSubBandOffset[MAX_SUBBAND_NUM_5G];
	uint8_t aucReverse[3];

};

struct CMD_NVRAM_SETTING {

	struct WIFI_CFG_PARAM_STRUCT rNvramSettings;

};

#if CFG_SUPPORT_TDLS
struct CMD_TDLS_CH_SW {
	u_int8_t fgIsTDLSChSwProhibit;
};
#endif

struct CMD_SET_DEVICE_MODE {
	uint16_t u2ChipID;
	uint16_t u2Mode;
};
#if CFG_SUPPORT_ADVANCE_CONTROL
/* command type */
#define CMD_ADV_CONTROL_SET (1<<15)
#define CMD_PTA_CONFIG_TYPE (0x1)
#define CMD_AFH_CONFIG_TYPE (0x2)
#define CMD_BA_CONFIG_TYPE (0x3)
#define CMD_GET_REPORT_TYPE (0x4)
#define CMD_NOISE_HISTOGRAM_TYPE (0x5)
#if CFG_IPI_2CHAIN_SUPPORT
#define CMD_NOISE_HISTOGRAM_TYPE2 (0x51)
#endif
#define CMD_ADMINCTRL_CONFIG_TYPE (0x6)
#ifdef CFG_SUPPORT_EXT_PTA_DEBUG_COMMAND
#define CMD_EXT_PTA_CONFIG_TYPE (0x7)
#endif

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
#define CMD_PTA_CONFIG_COMM_ACT_BT_WF0_INBAND (1<<15)
#define CMD_PTA_CONFIG_COMM_ACT_BT_WF0_OUTBAND (1<<16)
#define CMD_PTA_CONFIG_COMM_ACT_BT_WF1_INBAND (1<<17)
#define CMD_PTA_CONFIG_COMM_ACT_BT_WF1_OUTBAND (1<<18)

#ifdef CFG_SUPPORT_EXT_PTA_DEBUG_COMMAND
/* ext pta config related mask */
#define CMD_EXT_PTA_CONFIG_EXT_PTA (1<<0)
#define CMD_EXT_PTA_CONFIG_HI_RX_TAG (1<<1)
#define CMD_EXT_PTA_CONFIG_LO_RX_TAG (1<<2)
#define CMD_EXT_PTA_CONFIG_HI_TX_TAG (1<<3)
#define CMD_EXT_PTA_CONFIG_LO_TX_TAG (1<<4)
#define CMD_EXT_PTA_CONFIG_COMM_ACT_ZB_BT_UNSAFE (1<<5)
#define CMD_EXT_PTA_CONFIG_COMM_ACT_ZB_WF0_UNSAFE (1<<6)
#define CMD_EXT_PTA_CONFIG_COMM_ACT_ZB_WF1_UNSAFE (1<<7)
#define CMD_EXT_PTA_CONFIG_COMM_ACT_ZB_BT_HSF (1<<8)
#define CMD_EXT_PTA_CONFIG_COMM_ACT_ZB_WF0_HSF (1<<9)
#define CMD_EXT_PTA_CONFIG_COMM_ACT_ZB_WF1_HSF (1<<10)
#endif

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
struct CMD_PTA_CONFIG {
	uint16_t u2Type;
	uint16_t u2Len;
	uint32_t u4ConfigMask;
	/* common usage in set/get */
	uint32_t u4PtaConfig;
	uint32_t u4TxDataTag;
	uint32_t u4RxDataAckTag;
	uint32_t u4RxNswTag;
	uint32_t u4TxAckTag;
	uint32_t u4TxProtFrameTag;
	uint32_t u4RxProtFrameAckTag;
	uint32_t u4TxBMCTag;
	uint32_t u4TxBCNTag;
	uint32_t u4RxSPTag;
	uint32_t u4TxMgmtTag;
	uint32_t u4RxMgmtAckTag;
	uint32_t u4CommActBtWf0Inband;
	uint32_t u4CommActBtWf0Outband;
	uint32_t u4CommActBtWf1Inband;
	uint32_t u4CommActBtWf1Outband;
	/* Only used in get */
	uint32_t u4PtaWF0TxCnt;
	uint32_t u4PtaWF0RxCnt;
	uint32_t u4PtaWF0AbtCnt;
	uint32_t u4PtaWF1TxCnt;
	uint32_t u4PtaWF1RxCnt;
	uint32_t u4PtaWF1AbtCnt;
	uint32_t u4PtaBTTxCnt;
	uint32_t u4PtaBTRxCnt;
	uint32_t u4PtaBTAbtCnt;
	uint32_t u4GrantStat;
	uint32_t u4CoexMode;
};

#ifdef CFG_SUPPORT_EXT_PTA_DEBUG_COMMAND
struct CMD_EXT_PTA_CONFIG {
	uint16_t u2Type;
	uint16_t u2Len;
	uint32_t u4ConfigMask;
	/* common usage in set/get */
	uint32_t u4ExtPtaConfig;
	uint32_t u4ZbHiRxTag;
	uint32_t u4ZbLoRxTag;
	uint32_t u4ZbHiTxTag;
	uint32_t u4ZbLoTxTag;
	uint32_t u4CommActZbBtUnsafe;
	uint32_t u4CommActZbWf0Unsafe;
	uint32_t u4CommActZbWf1Unsafe;
	uint32_t u4CommActZbBtHsf;
	uint32_t u4CommActZbWf0Hsf;
	uint32_t u4CommActZbWf1Hsf;
	/* used in get */
	uint32_t u4ZbGntCnt;
	uint32_t u4ZbAbtCnt;
	uint32_t u4ZbLoTxReqCnt;
	uint32_t u4ZbHiTxReqCnt;
	uint32_t u4ZbLoRxReqCnt;
	uint32_t u4ZbHiRxReqCnt;
};
#endif

/* get report related */
enum ENUM_GET_REPORT_ACTION_T {
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
	uint16_t u2Type;
	uint16_t u2Len;
	/* parameter */
	uint8_t ucBand;
	uint8_t ucAction;
	uint8_t reserved[2];
	/* report 1 */
	uint32_t u4FalseCCA;
	uint32_t u4HdrCRC;
	uint32_t u4PktSent;
	uint32_t u4PktRetried;
	uint32_t u4PktTxfailed;
	uint32_t u4RxMPDU;
	uint32_t u4RxFcs;
	/* air time report */
	uint32_t u4FetchSt; /* ms */
	uint32_t u4FetchEd; /* ms */
	uint32_t u4ChBusy; /* us */
	uint32_t u4ChIdle; /* us */
	uint32_t u4TxAirTime; /* us */
	uint32_t u4RxAirTime; /* us */
	uint32_t u4TimerDur; /* ms */
	uint32_t u4FetchCost; /* us */
	int32_t TimerDrift; /* ms */
	int16_t u2SamplePoints; /* ms */
	int8_t ucTxThres; /* ms */
	int8_t ucRxThres; /* ms */
};

/* admission control related define */
/* support set operations */
#define ADMIN_CTRL_SET_MODE (0)
#define ADMIN_CTRL_SET_BASE (1)
#define ADMIN_CTRL_SET_TBL1 (2)
#define ADMIN_CTRL_SET_TBL2 (3)
#define ADMIN_CTRL_SET_TBL3 (4)
#define ADMIN_CTRL_SET_METHOD (5)

/* admission ctrl mode */
#define ADMIN_CTRL_MODE_DIS (0)
#define ADMIN_CTRL_MODE_AUTO (1)
#define ADMIN_CTRL_MODE_MAN (2)
#define ADMIN_CTRL_MODE_RESET (3)
/* default value */
#define ADMIN_CTRL_RATE_CODE_NUM (8) /* AUTO_RATE_NUM */
#define ADMIN_CTRL_TBL_ENTRY_NUM (6)
#define ADMIN_CTRL_MAX_PERCENTAGE (100)
/* status define */
#define BT_PROF_A2DP_SRC 0x02
#define BT_PROF_LINK_CONNECTED 0x04
#define BT_PROF_A2DP_SINK 0x400
#define ADMIN_LINK_2G BIT(0)
#define ADMIN_LINK_OTHER BIT(1)
#define ADMIN_ENABLED BIT(2)
#define ADMIN_PER_PKT_ENABLED BIT(3)
#define ADMIN_METHOD1_ENABLED BIT(4)
#define ADMIN_METHOD2_ENABLED BIT(5)

struct ADMIN_CTRL_PARAM {
	/* bt info updated to admin ctrl */
	uint32_t u4CoexMode;
	/* Ctrl mode */
	uint16_t u2eMode;
	/* Admin ctrl related */
	uint16_t u2AdminCtrlBase;
	uint16_t u2CurAdminTime;
	uint16_t u2ForceAdminTime;
	/* Rate related */
	uint16_t au2RateCode[ADMIN_CTRL_RATE_CODE_NUM];
	/* admin ctrl % tbl */
	uint8_t aucAdminTbl1[ADMIN_CTRL_TBL_ENTRY_NUM];
	uint8_t aucAdminTbl2[ADMIN_CTRL_TBL_ENTRY_NUM];
	uint8_t aucAdminTbl3[ADMIN_CTRL_TBL_ENTRY_NUM];
	uint8_t ucAdminThermalLimit;
	uint8_t ucLastChosenTbl;
	uint8_t ucAdminStatus;
	uint8_t reserved[3];
};

struct CMD_ADMIN_CTRL_CONFIG {
	uint16_t u2Type;
	uint16_t u2Len;
	/* parameter */
	uint16_t u2Action;
	uint8_t reserved[2];
	/* content */
	struct ADMIN_CTRL_PARAM content;
};

struct CMD_ADV_CONFIG_HEADER {
	uint16_t u2Type;
	uint16_t u2Len;
};

/* noise histogram related */
enum ENUM_NOISE_HISTOGRAM_ACTION_T {
	CMD_NOISE_HISTOGRAM_ENABLE = 1,
	CMD_NOISE_HISTOGRAM_DISABLE,
	CMD_NOISE_HISTOGRAM_RESET,
	CMD_NOISE_HISTOGRAM_GET,
#if CFG_IPI_2CHAIN_SUPPORT
	CMD_NOISE_HISTOGRAM_GET2
#endif
};
struct CMD_NOISE_HISTOGRAM_REPORT {
	uint16_t u2Type;
	uint16_t u2Len;
	/* parameter */
	uint8_t ucAction;
	uint8_t reserved[3];
	/* IPI_report */
	uint32_t u4IPI0;  /* Power <= -92 */
	uint32_t u4IPI1;  /* -92 < Power <= -89 */
	uint32_t u4IPI2;  /* -89 < Power <= -86 */
	uint32_t u4IPI3;  /* -86 < Power <= -83 */
	uint32_t u4IPI4;  /* -83 < Power <= -80 */
	uint32_t u4IPI5;  /* -80 < Power <= -75 */
	uint32_t u4IPI6;  /* -75 < Power <= -70 */
	uint32_t u4IPI7;  /* -70 < Power <= -65 */
	uint32_t u4IPI8;  /* -65 < Power <= -60 */
	uint32_t u4IPI9;  /* -60 < Power <= -55 */
	uint32_t u4IPI10; /* -55 < Power  */
};
#endif



#if CFG_SUPPORT_RDD_TEST_MODE
struct CMD_RDD_CH {
	uint8_t ucRddTestMode;
	uint8_t ucRddShutCh;
	uint8_t ucRddStartCh;
	uint8_t ucRddStopCh;
	uint8_t ucRddDfs;
	uint8_t ucReserved;
	uint8_t ucReserved1;
	uint8_t ucReserved2;
};

struct EVENT_RDD_STATUS {
	uint8_t ucRddStatus;
	uint8_t aucReserved[3];
};
#endif

struct EVENT_ICAP_STATUS {
	uint8_t ucRddStatus;
	uint8_t aucReserved[3];
	uint32_t u4StartAddress;
	uint32_t u4IcapSieze;
#if CFG_SUPPORT_QA_TOOL
	uint32_t u4IcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
};

#if CFG_SUPPORT_QA_TOOL
struct ADC_BUS_FMT {
	uint32_t u4Dcoc0Q: 14;	/* [13:0] */
	uint32_t u4Dcoc0I: 14;	/* [27:14] */
	uint32_t u4DbgData1: 4;	/* [31:28] */

	uint32_t u4Dcoc1Q: 14;	/* [45:32] */
	uint32_t u4Dcoc1I: 14;	/* [46:59] */
	uint32_t u4DbgData2: 4;	/* [63:60] */

	uint32_t u4DbgData3;	/* [95:64] */
};

struct IQC_BUS_FMT {
	int32_t u4Iqc0Q: 12;	/* [11:0] */
	int32_t u4Iqc0I: 12;	/* [23:12] */
	int32_t u4Na1: 8;		/* [31:24] */

	int32_t u4Iqc1Q: 12;	/* [43:32] */
	int32_t u4Iqc1I: 12;	/* [55:44] */
	int32_t u4Na2: 8;		/* [63:56] */

	int32_t u4Na3;		/* [95:64] */
};

struct IQC_160_BUS_FMT {
	int32_t u4Iqc0Q1: 12;	/* [11:0] */
	int32_t u4Iqc0I1: 12;	/* [23:12] */
	uint32_t u4Iqc0Q0P1: 8;	/* [31:24] */

	int32_t u4Iqc0Q0P2: 4;	/* [35:32] */
	int32_t u4Iqc0I0: 12;	/* [47:36] */
	int32_t u4Iqc1Q1: 12;	/* [59:48] */
	uint32_t u4Iqc1I1P1: 4;	/* [63:60] */

	int32_t u4Iqc1I1P2: 8;	/* [71:64] */
	int32_t u4Iqc1Q0: 12;	/* [83:72] */
	int32_t u4Iqc1I0: 12;	/* [95:84] */
};

struct SPECTRUM_BUS_FMT {
	int32_t u4DcocQ: 12;	/* [11:0] */
	int32_t u4DcocI: 12;	/* [23:12] */
	int32_t u4LpfGainIdx: 4;	/* [27:24] */
	int32_t u4LnaGainIdx: 2;	/* [29:28] */
	int32_t u4AssertData: 2;	/* [31:30] */
};

struct PACKED_ADC_BUS_FMT {
	uint32_t u4AdcQ0T2: 4;	/* [19:16] */
	uint32_t u4AdcQ0T1: 4;	/* [11:8] */
	uint32_t u4AdcQ0T0: 4;	/* [3:0] */

	uint32_t u4AdcI0T2: 4;	/* [23:20] */
	uint32_t u4AdcI0T1: 4;	/* [15:12] */
	uint32_t u4AdcI0T0: 4;	/* [7:4] */

	uint32_t u4AdcQ0T5: 4;	/* [43:40] */
	uint32_t u4AdcQ0T4: 4;	/* [35:32] */
	uint32_t u4AdcQ0T3: 4;	/* [27:24] */

	uint32_t u4AdcI0T5: 4;	/* [47:44] */
	uint32_t u4AdcI0T4: 4;	/* [39:36] */
	uint32_t u4AdcI0T3: 4;	/* [31:28] */

	uint32_t u4AdcQ1T2: 4;	/* [19:16] */
	uint32_t u4AdcQ1T1: 4;	/* [11:8] */
	uint32_t u4AdcQ1T0: 4;	/* [3:0] */

	uint32_t u4AdcI1T2: 4;	/* [23:20] */
	uint32_t u4AdcI1T1: 4;	/* [15:12] */
	uint32_t u4AdcI1T0: 4;	/* [7:4] */

	uint32_t u4AdcQ1T5: 4;	/* [43:40] */
	uint32_t u4AdcQ1T4: 4;	/* [35:32] */
	uint32_t u4AdcQ1T3: 4;	/* [27:24] */

	uint32_t u4AdcI1T5: 4;	/* [47:44] */
	uint32_t u4AdcI1T4: 4;	/* [39:36] */
	uint32_t u4AdcI1T3: 4;	/* [31:28] */
};

union ICAP_BUS_FMT {
	struct ADC_BUS_FMT rAdcBusData;	/* 12 bytes */
	struct IQC_BUS_FMT rIqcBusData;	/* 12 bytes */
	struct IQC_160_BUS_FMT rIqc160BusData;	/* 12 bytes */
	struct SPECTRUM_BUS_FMT rSpectrumBusData;	/* 4  bytes */
	struct PACKED_ADC_BUS_FMT rPackedAdcBusData;	/* 12 bytes */
};
#endif /* CFG_SUPPORT_QA_TOOL */

struct CMD_SET_TXPWR_CTRL {
	int8_t c2GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	int8_t c2GHotspotPwrOffset;
	int8_t c2GP2pPwrOffset;
	int8_t c2GBowPwrOffset;
	int8_t c5GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	int8_t c5GHotspotPwrOffset;
	int8_t c5GP2pPwrOffset;
	int8_t c5GBowPwrOffset;
	/* TX power policy when concurrence
	 * in the same channel
	 * 0: Highest power has priority
	 * 1: Lowest power has priority
	 */
	uint8_t ucConcurrencePolicy;
	int8_t acReserved1[3];	/* Must be zero */

	/* Power limit by channel for all data rates */
	int8_t acTxPwrLimit2G[14];	/* Channel 1~14, Unit: 0.5dBm */
	int8_t acTxPwrLimit5G[4];	/* UNII 1~4 */
	int8_t acReserved2[2];	/* Must be zero */
};

struct SSID_MATCH_SETS {
	int32_t i4RssiThresold;
	uint8_t aucSsid[32];
	uint8_t ucSsidLen;
	uint8_t aucPadding_1[3];
};

struct CMD_SCHED_SCAN_REQ {
	uint8_t ucVersion;
	uint8_t ucSeqNum;
	uint8_t fgStopAfterIndication;
	uint8_t ucSsidNum;
	uint8_t ucMatchSsidNum;
	uint8_t aucPadding_0;
	uint16_t u2IELen;
	struct PARAM_SSID auSsid[10];
	struct SSID_MATCH_SETS auMatchSsid[16];
	uint8_t ucChannelType;
	uint8_t ucChnlNum;
	uint8_t ucMspEntryNum;
	uint8_t ucScnFuncMask;
	struct CHANNEL_INFO aucChannel[64];
	uint16_t au2MspList[10];
	uint8_t aucPadding_3[64];

	/* keep last */
	uint8_t aucIE[0];             /* MUST be the last for IE content */
};

struct EVENT_SCHED_SCAN_DONE {
	uint8_t ucSeqNum;
	uint8_t ucStatus;
	uint8_t aucReserved[2];
};

struct CMD_HIF_CTRL {
	uint8_t ucHifType;
	uint8_t ucHifDirection;
	uint8_t ucHifStop;
	uint8_t ucHifSuspend;
	uint8_t aucReserved2[32];
};

enum ENUM_HIF_TYPE {
	ENUM_HIF_TYPE_SDIO = 0x00,
	ENUM_HIF_TYPE_USB = 0x01,
	ENUM_HIF_TYPE_PCIE = 0x02,
	ENUM_HIF_TYPE_GPIO = 0x03,
};

enum ENUM_HIF_DIRECTION {
	ENUM_HIF_TX = 0x01,
	ENUM_HIF_RX = 0x02,
	ENUM_HIF_TRX = 0x03,
};

enum ENUM_HIF_TRAFFIC_STATUS {
	ENUM_HIF_TRAFFIC_BUSY = 0x01,
	ENUM_HIF_TRAFFIC_IDLE = 0x02,
	ENUM_HIF_TRAFFIC_INVALID = 0x3,
};

struct EVENT_HIF_CTRL {
	uint8_t ucHifType;
	uint8_t ucHifTxTrafficStatus;
	uint8_t ucHifRxTrafficStatus;
	uint8_t ucHifSuspend;
	uint8_t aucReserved2[32];
};

#if CFG_SUPPORT_BUILD_DATE_CODE
struct CMD_GET_BUILD_DATE_CODE {
	uint8_t aucReserved[4];
};

struct EVENT_BUILD_DATE_CODE {
	uint8_t aucDateCode[16];
};
#endif

struct CMD_GET_STA_STATISTICS {
	uint8_t ucIndex;
	uint8_t ucFlags;
	uint8_t ucReadClear;
	uint8_t ucLlsReadClear;
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucResetCounter;
	uint8_t aucReserved1[1];
	uint8_t aucReserved2[16];
};

/* per access category statistics */
struct WIFI_WMM_AC_STAT_GET_FROM_FW {
	uint32_t u4TxFailMsdu;
	uint32_t u4TxRetryMsdu;
};

/* CFG_SUPPORT_WFD */
struct EVENT_STA_STATISTICS {
	/* Event header */
	/* UINT_16     u2Length; */
	/* Must be filled with 0x0001 (EVENT Packet) */
	/* UINT_16     u2Reserved1; */
	/* UINT_8            ucEID; */
	/* UINT_8      ucSeqNum; */
	/* UINT_8            aucReserved2[2]; */

	/* Event Body */
	uint8_t ucVersion;
	uint8_t aucReserved1[3];
	uint32_t u4Flags;	/* Bit0: valid */

	uint8_t ucStaRecIdx;
	uint8_t ucNetworkTypeIndex;
	uint8_t ucWTEntry;
	uint8_t aucReserved4[1];

	uint8_t ucMacAddr[MAC_ADDR_LEN];
	uint8_t ucPer;		/* base: 128 */
	uint8_t ucRcpi;

	uint32_t u4PhyMode;	/* SGI BW */
	uint16_t u2LinkSpeed;	/* unit is 0.5 Mbits */
	uint8_t ucLinkQuality;
	uint8_t ucLinkReserved;

	uint32_t u4TxCount;
	uint32_t u4TxFailCount;
	uint32_t u4TxLifeTimeoutCount;
	uint32_t u4TxDoneAirTime;
	/* Transmit in the air (wtbl) */
	uint32_t u4TransmitCount;
	/* Transmit without ack/ba in the air (wtbl) */
	uint32_t u4TransmitFailCount;

	struct WIFI_WMM_AC_STAT_GET_FROM_FW
		arLinkStatistics[AC_NUM];	/*link layer statistics */

	uint8_t ucTemperature;
	uint8_t ucSkipAr;
	uint8_t ucArTableIdx;
	uint8_t ucRateEntryIdx;
	uint8_t ucRateEntryIdxPrev;
	uint8_t ucTxSgiDetectPassCnt;
	uint8_t ucAvePer;
#if (CFG_SUPPORT_RA_GEN == 0)
	uint8_t aucArRatePer[AR_RATE_TABLE_ENTRY_MAX];
	uint8_t aucRateEntryIndex[AUTO_RATE_NUM];
#else
	uint32_t u4AggRangeCtrl_0;
	uint32_t u4AggRangeCtrl_1;
	uint8_t ucRangeType;
	uint8_t aucReserved5[24];
#endif
	uint8_t ucArStateCurr;
	uint8_t ucArStatePrev;
	uint8_t ucArActionType;
	uint8_t ucHighestRateCnt;
	uint8_t ucLowestRateCnt;
	uint16_t u2TrainUp;
	uint16_t u2TrainDown;
	uint32_t u4Rate1TxCnt;
	uint32_t u4Rate1FailCnt;
	struct TX_VECTOR_BBP_LATCH rTxVector[ENUM_BAND_NUM];
	struct MIB_INFO_STAT rMibInfo[ENUM_BAND_NUM];
	u_int8_t fgIsForceTxStream;
	u_int8_t fgIsForceSeOff;
#if (CFG_SUPPORT_RA_GEN == 0)
	uint8_t aucReserved6[17];
#else
	uint16_t u2RaRunningCnt;
	uint8_t ucRaStatus;
	uint8_t ucFlag;
	uint8_t aucTxQuality[MAX_TX_QUALITY_INDEX];
	uint8_t ucTxRateUpPenalty;
	uint8_t ucLowTrafficMode;
	uint8_t ucLowTrafficCount;
	uint8_t ucLowTrafficDashBoard;
	uint8_t ucDynamicSGIState;
	uint8_t ucDynamicSGIScore;
	uint8_t ucDynamicBWState;
	uint8_t ucDynamicGband256QAMState;
	uint8_t ucVhtNonSpRateState;
#endif
	uint8_t aucReserved[4];
};

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
struct EVENT_LTE_SAFE_CHN {
	uint8_t ucVersion;
	uint8_t aucReserved[3];
	uint32_t u4Flags;	/* Bit0: valid */
	struct LTE_SAFE_CHN_INFO rLteSafeChn;
};
#endif

#if CFG_SUPPORT_SNIFFER
struct CMD_MONITOR_SET_INFO {
	uint8_t ucEnable;
	uint8_t ucBand;
	uint8_t ucPriChannel;
	uint8_t ucSco;
	uint8_t ucChannelWidth;
	uint8_t ucChannelS1;
	uint8_t ucChannelS2;
	uint8_t aucResv[9];
};
#endif

struct CMD_STATS_LOG {
	uint32_t u4DurationInMs;
	uint8_t aucReserved[32];
};

struct EVENT_WIFI_RDD_TEST {
	uint32_t u4FuncIndex;
	uint32_t u4FuncLength;
	uint32_t u4Prefix;
	uint32_t u4Count;
	uint8_t ucRddIdx;
	uint8_t aucReserve[3];
	uint8_t aucBuffer[0];
};

#if CFG_SUPPORT_MSP
/* EVENT_ID_WLAN_INFO */
struct EVENT_WLAN_INFO {

	struct PARAM_TX_CONFIG	rWtblTxConfig;
	struct PARAM_SEC_CONFIG	rWtblSecConfig;
	struct PARAM_KEY_CONFIG	rWtblKeyConfig;
	struct PARAM_PEER_RATE_INFO	rWtblRateInfo;
	struct PARAM_PEER_BA_CONFIG	rWtblBaConfig;
	struct PARAM_PEER_CAP	rWtblPeerCap;
	struct PARAM_PEER_RX_COUNTER_ALL rWtblRxCounter;
	struct PARAM_PEER_TX_COUNTER_ALL rWtblTxCounter;
};

/* EVENT_ID_MIB_INFO */
struct EVENT_MIB_INFO {
	struct HW_MIB_COUNTER	    rHwMibCnt;
	struct HW_MIB2_COUNTER	    rHwMib2Cnt;
	struct HW_TX_AMPDU_METRICS	    rHwTxAmpduMts;

};
#endif
#if CFG_SUPPORT_GET_MCS_INFO
struct EVENT_TX_MCS_INFO {
	uint16_t     au2TxRateCode[MCS_INFO_SAMPLE_CNT];
	uint8_t      aucTxRatePer[MCS_INFO_SAMPLE_CNT];
	uint8_t      aucReserved[2];
};
#endif

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
struct EVENT_ACCESS_EFUSE {

	uint32_t         u4Address;
	uint32_t         u4Valid;
	uint8_t          aucData[16];

};

struct EXT_EVENT_EFUSE_FREE_BLOCK {
	uint8_t  u2FreeBlockNum;
	uint8_t  ucVersion; /* 0: original format ; 1: modified format */
	uint8_t  ucTotalBlockNum; /* Total Block */
	uint8_t  ucReserved;
};

struct EXT_EVENT_GET_TX_POWER {

	uint8_t  ucTxPwrType;
	uint8_t  ucEfuseAddr;
	uint8_t  ucTx0TargetPower;
	uint8_t  ucDbdcIdx;

};

struct EXT_EVENT_RF_TEST_RESULT_T {
	uint32_t u4FuncIndex;
	uint32_t u4PayloadLength;
	uint8_t  aucEvent[0];
};

struct EXT_EVENT_RBIST_DUMP_DATA_T {
	uint32_t u4FuncIndex;
	uint32_t u4PktNum;
	uint32_t u4Bank;
	uint32_t u4DataLength;
	uint32_t u4WFCnt;
	uint32_t u4SmplCnt;
	uint32_t u4Reserved[6];
	uint32_t u4Data[256];
};

struct EXT_EVENT_RBIST_CAP_STATUS_T {
	uint32_t u4FuncIndex;
	uint32_t u4CapDone;
	uint32_t u4Reserved[15];
};

struct EXT_EVENT_RECAL_DATA_T {
	uint32_t u4FuncIndex;
	uint32_t u4Type;	/* 0 for string, 1 for int data */
	union {
		uint8_t ucData[32];
		uint32_t u4Data[3];
	} u;
};


struct CMD_SUSPEND_MODE_SETTING {
	uint8_t ucBssIndex;
	uint8_t ucEnableSuspendMode;
	uint8_t ucMdtim; /* LP parameter */
	uint8_t ucDeviceSuspend; /* kernel 1:suspend 0:resume (reserved) */
	uint32_t u4CtrlFlag; /* b0:Dis ARPNS, b1:Dis REKEY, b2:DTIM policy */
	uint8_t ucWowSuspend; /* 0:updabye by bora origin policy,1:wow mdtim */
	uint8_t ucReserved1[3];
	uint8_t ucReserved2[56];
};

struct EVENT_UPDATE_COEX_PHYRATE {
	uint8_t ucVersion;
	uint8_t aucReserved1[3];    /* 4 byte alignment */
	uint32_t u4Flags;
	uint32_t au4PhyRateLimit[MAX_BSSID_NUM + 1];
	uint8_t ucSupportSisoOnly;
	uint8_t ucWfPathSupport;
	uint8_t aucReserved2[2];    /* 4 byte alignment */
};

struct CMD_GET_TXPWR_TBL {
	/* DWORD_0 - Common Part */
	uint8_t  ucCmdVer;
	uint8_t  ucAction;
	uint16_t u2CmdLen;

	/* DWORD_1 ~ x - Command Body */
	uint8_t ucDbdcIdx;
	uint8_t aucReserved[3];
};

struct EVENT_GET_TXPWR_TBL {
	/* DWORD_0 - Common Part */
	uint8_t  ucEvtVer;
	uint8_t  ucAction;
	uint16_t u2EvtLen;

	/* DWORD_1 ~ x - Command Body */
	uint8_t ucCenterCh;
	uint8_t aucReserved[3];
	struct POWER_LIMIT tx_pwr_tbl[TXPWR_TBL_NUM];
};

#ifdef CFG_GET_TEMPURATURE

struct EVENT_GET_THERMAL_SENSOR {
	uint8_t  u1ThermalCategory;
	uint8_t  u1Reserved[3];
	uint32_t u4SensorResult;
};

struct CMD_THERMAL_SENSOR_INFO {
	uint8_t  u1ThermalCtrlFormatId;
	/* 0: get temperature, 1: get thermal sensor ADC */
	uint8_t  u1ActionIdx;
	uint8_t  u1Reserved[2];
};


/** enum for EVENT */
enum THERMAL_EVENT_CATEGORY {
	THERMAL_EVENT_TEMPERATURE_INFO = 0x0,
	TXPOWER_EVENT_THERMAL_SENSOR_SHOW_INFO = 0x1,
	THERMAL_EVENT_NUM
};

/** enum for Get Thermal Sensor Info Action */
enum THERMAL_SENSOR_INFO_ACTION {
	THERMAL_SENSOR_INFO_TEMPERATURE = 0,
	THERMAL_SENSOR_INFO_ADC,
	THERMAL_SENSOR_INFO_NUM
};


/** enum for CMD */
enum THERMAL_ACTION_CATEGORY {
	THERMAL_SENSOR_INFO_GET = 0x0,
	THERMAL_MANUAL_CTRL = 0x1,
	THERMAL_SENSOR_BASIC_INFO = 0x2,
	THERMAL_ACTION_NUM
};

#endif

/*#endif*/
struct CMD_TDLS_PS_T {
	/* 0: disable tdls power save; 1: enable tdls power save */
	uint8_t	ucIsEnablePs;
	uint8_t	aucReserved[3];
};

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
struct CMD_TX_POWER_SHOW_INFO_T {
	uint8_t ucPowerCtrlFormatId;
	uint8_t ucTxPowerInfoCatg;
	uint8_t ucBandIdx;
	uint8_t ucReserved;
};

struct EXT_EVENT_TXPOWER_ALL_RATE_POWER_INFO_T {
	uint8_t ucTxPowerCategory;
	uint8_t ucBandIdx;
	uint8_t ucChBand;
	uint8_t ucReserved;

	/* Rate power info */
	struct FRAME_POWER_CONFIG_INFO_T rRatePowerInfo;

	/* tx Power Max/Min Limit info */
	int8_t icPwrMaxBnd;
	int8_t icPwrMinBnd;
	uint8_t ucReserved2;
};
#endif

struct CMD_SET_SCHED_SCAN_ENABLE {
	uint8_t ucSchedScanAct;
	uint8_t aucReserved[3];
};

struct CMD_EVENT_LOG_UI_INFO {
	uint32_t u4Version;
	uint32_t u4LogLevel;
	uint8_t aucReserved[4];
};

struct EXT_EVENT_MAX_AMSDU_LENGTH_UPDATE {
	uint8_t ucWlanIdx;
	uint8_t ucAmsduLen;
};

struct EVENT_GET_IPI_INFO_T {
	uint32_t au4IpiValue[11];
};

#if (CFG_SUPPORT_PERF_IND == 1)
struct CMD_PERF_IND {
	/* DWORD_0 - Common Part */
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* cmd size including common part and body. */
	/* DWORD_1 ~ x - Command Body */
	uint32_t u4VaildPeriod;   /* in ms */
	/* Current State */
	uint32_t ulCurTxBytes[4];   /* in Bps */
	uint32_t ulCurRxBytes[4];   /* in Bps */
	uint16_t u2CurRxRate[4];     /* Unit 500 Kbps */
	uint8_t ucCurRxRCPI0[4];
	uint8_t ucCurRxRCPI1[4];
	uint8_t ucCurRxNss[4];
	uint32_t au4Reserve[63];
};
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
struct CMD_KEEP_FULL_PWR {
	uint8_t ucEnable;
	uint8_t aucReserved[3];
};
#endif

#ifdef CFG_SUPPORT_TIME_MEASURE
/* TM CMD/EVENT */
enum ENUM_TM_EVENT_INDEX_T {
	TM_EVENT_NULLBOT = 0,
	TM_EVENT_TM_REPORT,
	TM_EVENT_QUERY_TMSYNC,
	TM_EVENT_QUERY_DISTANCE,
	TM_EVENT_NUM
};

enum ENUM_TM_ACTION_CATEGORY {
	TM_ACTION_NULLBOT = 0,
	TM_ACTION_TMR_ENABLE,
	TM_ACTION_TOAE_CAL,
	TM_ACTION_START_FTM,
	TM_ACTION_TMSYNC_QUERY,
	TM_ACTION_DISTANCE_QUERY,
	TM_ACTION_NUM
};

struct EVENT_TM_REPORT_T {
	/* DWORD_0 - Common Part */
	uint8_t ucCmdVer;
	uint8_t ucTmrEventIdx;
	uint16_t u2CmdLen; /* Cmd size including common part and body */

	/* DWORD 1/2 - common */
	uint8_t aucRttPeerAddr[6]; /* TM peer address */
	uint16_t u2NumOfValidResult;

	/* DWORD 3 - Distance */
	uint32_t u4DistanceCm;
	/* DWORD 4~5 - Distance STDEV in burst */
	uint64_t u8DistStdevSq;

	/* DWORD 6~13 - Audio Sync */
	uint64_t u8Tsf;
	int64_t i8ClockOffset;
	int64_t i8ClkRateDiffRatioIn10ms;
	uint64_t u8LastToA;

	uint8_t aucPadding2[64];

};

struct CMD_TM_ACTION_T {
	/* DWORD_0 - Common Part */
	uint8_t ucCmdVer;
	uint8_t ucTmCategory;
	uint16_t u2CmdLen; /* Cmd size including common part and body */

	/* DWORD 1/2 */
	uint8_t aucRttPeerAddr[6]; /* TM peer address */
	uint8_t ucFTMNum;
	uint8_t ucMinDeltaIn100US;

	/* DWORD 3 - FTM parameter */
	uint8_t ucFTMBandwidth;
	uint8_t fgFtmEnable;
	uint8_t ucPadding1[2];

	uint8_t aucPadding2[64];

};

#define CMD_TM_ACTION_START_FTM_LEN \
				OFFSET_OF(struct CMD_TM_ACTION_T, fgFtmEnable)
#define CMD_TM_ACTION_QUERY_LEN \
			  OFFSET_OF(struct CMD_TM_ACTION_T, aucRttPeerAddr[0])
#define TM_CMD_EVENT_VER 0
#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void nicCmdEventQueryMcrRead(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryCoexGetInfo(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryCoexIso(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

#if CFG_SUPPORT_QA_TOOL
void nicCmdEventQueryRxStatistics(IN struct ADAPTER
				  *prAdapter, IN struct CMD_INFO *prCmdInfo,
				  IN uint8_t *pucEventBuf);

uint32_t nicTsfRawData2IqFmt(struct EVENT_DUMP_MEM *prEventDumpMem,
	struct ICAP_INFO_T *prIcap);
uint32_t nicExtTsfRawData2IqFmt(
	struct EXT_EVENT_RBIST_DUMP_DATA_T *prEventDumpMem,
	struct ICAP_INFO_T *prIcap);

int32_t GetIQData(struct ADAPTER *prAdapter,
		  int32_t **prIQAry, uint32_t *prDataLen, uint32_t u4IQ,
		  uint32_t u4GetWf1);

#if CFG_SUPPORT_TX_BF
void nicCmdEventPfmuDataRead(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventPfmuTagRead(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
#endif /* CFG_SUPPORT_TX_BF */
#if CFG_SUPPORT_MU_MIMO
void nicCmdEventGetQd(IN struct ADAPTER *prAdapter,
		      IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void nicCmdEventGetCalcLq(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void nicCmdEventGetCalcInitMcs(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
void nicCmdEventQueryCalBackupV2(IN struct ADAPTER
				 *prAdapter, IN struct CMD_INFO *prCmdInfo,
				 IN uint8_t *pucEventBuf);
#endif
#if 0
void nicEventQueryMemDump(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucEventBuf);
#endif

void nicCmdEventQueryMemDump(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQuerySwCtrlRead(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);

void nicCmdEventQueryChipConfig(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);

void nicCmdEventQueryRfTestATInfo(IN struct ADAPTER
				  *prAdapter, IN struct CMD_INFO *prCmdInfo,
				  IN uint8_t *pucEventBuf);

void nicCmdEventSetCommon(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventSetDisassociate(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);

void nicCmdEventSetIpAddress(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryLinkQuality(IN struct ADAPTER
				 *prAdapter, IN struct CMD_INFO *prCmdInfo,
				 IN uint8_t *pucEventBuf);

void nicCmdEventQueryLinkSpeed(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryStatistics(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);

void nicCmdEventEnterRfTest(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventLeaveRfTest(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryMcastAddr(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryEepromRead(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);

void nicCmdEventSetMediaStreamMode(IN struct ADAPTER
				   *prAdapter, IN struct CMD_INFO *prCmdInfo,
				   IN uint8_t *pucEventBuf);

void nicCmdEventSetStopSchedScan(IN struct ADAPTER
				 *prAdapter, IN struct CMD_INFO *prCmdInfo,
				 IN uint8_t *pucEventBuf);

/* Statistics responder */
void nicCmdEventQueryXmitOk(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryRecvOk(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryXmitError(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryRecvError(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryRecvNoBuffer(IN struct ADAPTER
				  *prAdapter, IN struct CMD_INFO *prCmdInfo,
				  IN uint8_t *pucEventBuf);

void nicCmdEventQueryRecvCrcError(IN struct ADAPTER
				  *prAdapter, IN struct CMD_INFO *prCmdInfo,
				  IN uint8_t *pucEventBuf);

void nicCmdEventQueryRecvErrorAlignment(IN struct ADAPTER
	*prAdapter, IN struct CMD_INFO *prCmdInfo,
	IN uint8_t *pucEventBuf);

void nicCmdEventQueryXmitOneCollision(IN struct ADAPTER
				      *prAdapter, IN struct CMD_INFO *prCmdInfo,
				      IN uint8_t *pucEventBuf);

void nicCmdEventQueryXmitMoreCollisions(IN struct ADAPTER
	*prAdapter, IN struct CMD_INFO *prCmdInfo,
	IN uint8_t *pucEventBuf);

void nicCmdEventQueryXmitMaxCollisions(IN struct ADAPTER
	*prAdapter, IN struct CMD_INFO *prCmdInfo,
	IN uint8_t *pucEventBuf);

/* for timeout check */
void nicOidCmdTimeoutCommon(IN struct ADAPTER *prAdapter,
			    IN struct CMD_INFO *prCmdInfo);

void nicCmdTimeoutCommon(IN struct ADAPTER *prAdapter,
			 IN struct CMD_INFO *prCmdInfo);

void nicOidCmdEnterRFTestTimeout(IN struct ADAPTER
				 *prAdapter, IN struct CMD_INFO *prCmdInfo);

#if CFG_SUPPORT_BUILD_DATE_CODE
void nicCmdEventBuildDateCode(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
#endif

void nicCmdEventQueryStaStatistics(IN struct ADAPTER
				   *prAdapter, IN struct CMD_INFO *prCmdInfo,
				   IN uint8_t *pucEventBuf);

void nicCmdEventQueryBugReport(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
/* 4 Auto Channel Selection */
void nicCmdEventQueryLteSafeChn(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);
#endif

#if CFG_SUPPORT_BATCH_SCAN
void nicCmdEventBatchScanResult(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
void nicCmdEventQueryAdvCtrl(IN struct ADAPTER
				*prAdapter, IN struct CMD_INFO *prCmdInfo,
				IN uint8_t *pucEventBuf);
#endif

void nicEventRddPulseDump(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucEventBuf);

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
void nicCmdEventQueryTxPowerInfo(IN struct ADAPTER
				 *prAdapter, IN struct CMD_INFO *prCmdInfo,
				 IN uint8_t *pucEventBuf);
#endif

void nicCmdEventQueryWlanInfo(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryMibInfo(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicCmdEventQueryNicCapabilityV2(IN struct ADAPTER
				     *prAdapter, IN uint8_t *pucEventBuf);

uint32_t nicCmdEventQueryNicTxResource(IN struct ADAPTER
				       *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCmdEventQueryNicEfuseAddr(IN struct ADAPTER
				      *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCmdEventQueryNicCoexFeature(IN struct ADAPTER
					*prAdapter, IN uint8_t *pucEventBuf);
#if CFG_TCP_IP_CHKSUM_OFFLOAD
uint32_t nicCmdEventQueryNicCsumOffload(IN struct ADAPTER
					*prAdapter, IN uint8_t *pucEventBuf);
#endif
uint32_t nicCfgChipCapHwVersion(IN struct ADAPTER
				*prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapSwVersion(IN struct ADAPTER
				*prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapMacAddr(IN struct ADAPTER *prAdapter,
			      IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapPhyCap(IN struct ADAPTER *prAdapter,
			     IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapMacCap(IN struct ADAPTER *prAdapter,
			     IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapFrameBufCap(IN struct ADAPTER
				  *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapBeamformCap(IN struct ADAPTER
				  *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapLocationCap(IN struct ADAPTER
				  *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapMuMimoCap(IN struct ADAPTER
				*prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicCfgChipCapBufferModeInfo(IN struct ADAPTER
					*prAdapter, IN uint8_t *pucEventBuf);

void nicExtEventICapIQData(IN struct ADAPTER *prAdapter,
			   IN uint8_t *pucEventBuf);
void nicExtEventQueryMemDump(IN struct ADAPTER *prAdapter,
			     IN uint8_t *pucEventBuf);
void nicEventLinkQuality(IN struct ADAPTER *prAdapter,
			 IN struct WIFI_EVENT *prEvent);
void nicEventLayer0ExtMagic(IN struct ADAPTER *prAdapter,
			    IN struct WIFI_EVENT *prEvent);
void nicEventMicErrorInfo(IN struct ADAPTER *prAdapter,
			  IN struct WIFI_EVENT *prEvent);
void nicEventScanDone(IN struct ADAPTER *prAdapter,
		      IN struct WIFI_EVENT *prEvent);
void nicEventSchedScanDone(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent);
void nicEventSleepyNotify(IN struct ADAPTER *prAdapter,
			  IN struct WIFI_EVENT *prEvent);
void nicEventBtOverWifi(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent);
void nicEventStatistics(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent);
void nicEventWlanInfo(IN struct ADAPTER *prAdapter,
		      IN struct WIFI_EVENT *prEvent);
void nicEventMibInfo(IN struct ADAPTER *prAdapter,
		     IN struct WIFI_EVENT *prEvent);
void nicEventIpiInfo(IN struct ADAPTER *prAdapter,
		     IN struct WIFI_EVENT *prEvent);
void nicEventBeaconTimeout(IN struct ADAPTER *prAdapter,
			   IN struct WIFI_EVENT *prEvent);
void nicEventUpdateNoaParams(IN struct ADAPTER *prAdapter,
			     IN struct WIFI_EVENT *prEvent);
void nicEventStaAgingTimeout(IN struct ADAPTER *prAdapter,
			     IN struct WIFI_EVENT *prEvent);
void nicEventApObssStatus(IN struct ADAPTER *prAdapter,
			  IN struct WIFI_EVENT *prEvent);
void nicEventRoamingStatus(IN struct ADAPTER *prAdapter,
			   IN struct WIFI_EVENT *prEvent);
void nicEventSendDeauth(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent);
void nicEventUpdateRddStatus(IN struct ADAPTER *prAdapter,
			     IN struct WIFI_EVENT *prEvent);
void nicEventUpdateBwcsStatus(IN struct ADAPTER *prAdapter,
			      IN struct WIFI_EVENT *prEvent);
void nicEventUpdateBcmDebug(IN struct ADAPTER *prAdapter,
			    IN struct WIFI_EVENT *prEvent);
void nicEventAddPkeyDone(IN struct ADAPTER *prAdapter,
			 IN struct WIFI_EVENT *prEvent);
void nicEventIcapDone(IN struct ADAPTER *prAdapter,
		      IN struct WIFI_EVENT *prEvent);
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
void nicEventCalAllDone(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent);
#endif

void nicEventDebugMsg(IN struct ADAPTER *prAdapter,
		      IN struct WIFI_EVENT *prEvent);
void nicEventTdls(IN struct ADAPTER *prAdapter,
		  IN struct WIFI_EVENT *prEvent);
void nicEventRssiMonitor(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent);
void nicEventDumpMem(IN struct ADAPTER *prAdapter,
		     IN struct WIFI_EVENT *prEvent);
void nicEventAssertDump(IN struct ADAPTER *prAdapter,
			IN struct WIFI_EVENT *prEvent);
void nicEventHifCtrl(IN struct ADAPTER *prAdapter,
		     IN struct WIFI_EVENT *prEvent);
void nicEventRddSendPulse(IN struct ADAPTER *prAdapter,
			  IN struct WIFI_EVENT *prEvent);
void nicEventUpdateCoexPhyrate(IN struct ADAPTER *prAdapter,
			       IN struct WIFI_EVENT *prEvent);
uint32_t nicEventQueryTxResource_v1(IN struct ADAPTER
				    *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicEventQueryTxResourceEntry(IN struct ADAPTER
				      *prAdapter, IN uint8_t *pucEventBuf);
uint32_t nicEventQueryTxResource(IN struct ADAPTER
				 *prAdapter, IN uint8_t *pucEventBuf);

#if CFG_SUPPORT_ANT_DIV
void nicCmdEventAntDiv(IN struct ADAPTER
	*prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
#endif

#if (CFG_WOW_SUPPORT == 1)
void nicEventWakeUpReason(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent);
#endif
void nicCmdEventQueryCnmInfo(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void nicEventCnmInfo(IN struct ADAPTER *prAdapter,
		     IN struct WIFI_EVENT *prEvent);
#if CFG_SUPPORT_REPLAY_DETECTION
void nicCmdEventSetAddKey(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void nicOidCmdTimeoutSetAddKey(IN struct ADAPTER *prAdapter,
			       IN struct CMD_INFO *prCmdInfo);
void nicEventGetGtkDataSync(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent);
#endif
void nicCmdEventGetTxPwrTbl(IN struct ADAPTER *prAdapter,
			    IN struct CMD_INFO *prCmdInfo,
			    IN uint8_t *pucEventBuf);

#ifdef CFG_GET_TEMPURATURE
void nicCmdEventGetTemperature(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo,
	IN uint8_t *pucEventBuf);
#endif

#if (CFG_SUPPORT_GET_MCS_INFO == 1)
void nicCmdEventQueryTxMcsInfo(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);

void nicEventTxMcsInfo(IN struct ADAPTER *prAdapter,
		     IN struct WIFI_EVENT *prEvent);
#endif

#ifdef CFG_SUPPORT_TIME_MEASURE
void nicCmdEventGetTmReport(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo,
	IN uint8_t *pucEventBuf);
#endif

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _NIC_CMD_EVENT_H */
