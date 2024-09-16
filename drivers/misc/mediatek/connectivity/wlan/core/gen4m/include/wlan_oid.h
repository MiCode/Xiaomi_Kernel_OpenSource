/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include
 *      /wlan_oid.h#4
 */

/*! \file   "wlan_oid.h"
 *    \brief This file contains the declairation file of the WLAN OID processing
 *         routines of Windows driver for MediaTek Inc.
 *         802.11 Wireless LAN Adapters.
 */

#ifndef _WLAN_OID_H
#define _WLAN_OID_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

#define PARAM_MAX_LEN_SSID                      32

#define PARAM_MAC_ADDR_LEN                      6

#define ETHERNET_HEADER_SZ                      14
#define ETHERNET_MIN_PKT_SZ                     60
#define ETHERNET_MAX_PKT_SZ                     1514

#define PARAM_MAX_LEN_RATES                     8
#define PARAM_MAX_LEN_RATES_EX                  16

#define PARAM_AUTH_REQUEST_REAUTH               0x01
#define PARAM_AUTH_REQUEST_KEYUPDATE            0x02
#define PARAM_AUTH_REQUEST_PAIRWISE_ERROR       0x06
#define PARAM_AUTH_REQUEST_GROUP_ERROR          0x0E


#define PARAM_EEPROM_READ_METHOD_GETSIZE        0
#define PARAM_EEPROM_READ_METHOD_READ           1
#define PARAM_EEPROM_READ_NVRAM					2
#define PARAM_EEPROM_WRITE_NVRAM				3


#define PARAM_WHQL_RSSI_MAX_DBM                 (-10)
#define PARAM_WHQL_RSSI_INITIAL_DBM             (-50)
#define PARAM_WHQL_RSSI_MIN_DBM                 (-200)

#define PARAM_DEVICE_WAKE_UP_ENABLE                     0x00000001
#define PARAM_DEVICE_WAKE_ON_PATTERN_MATCH_ENABLE       0x00000002
#define PARAM_DEVICE_WAKE_ON_MAGIC_PACKET_ENABLE        0x00000004

#define PARAM_WAKE_UP_MAGIC_PACKET              0x00000001
#define PARAM_WAKE_UP_PATTERN_MATCH             0x00000002
#define PARAM_WAKE_UP_LINK_CHANGE               0x00000004

/* Packet filter bit definitioin (UINT_32 bit-wise definition) */
#define PARAM_PACKET_FILTER_DIRECTED            0x00000001
#define PARAM_PACKET_FILTER_MULTICAST           0x00000002
#define PARAM_PACKET_FILTER_ALL_MULTICAST       0x00000004
#define PARAM_PACKET_FILTER_BROADCAST           0x00000008
#define PARAM_PACKET_FILTER_PROMISCUOUS         0x00000020
#define PARAM_PACKET_FILTER_ALL_LOCAL           0x00000080
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#define PARAM_PACKET_FILTER_P2P_MASK		0xF0000000
#define PARAM_PACKET_FILTER_PROBE_REQ		0x80000000
#define PARAM_PACKET_FILTER_ACTION_FRAME	0x40000000
#define PARAM_PACKET_FILTER_AUTH		0x20000000
#define PARAM_PACKET_FILTER_ASSOC_REQ		0x10000000
#endif

#if CFG_SLT_SUPPORT
#define PARAM_PACKET_FILTER_SUPPORTED   (PARAM_PACKET_FILTER_DIRECTED | \
					 PARAM_PACKET_FILTER_MULTICAST | \
					 PARAM_PACKET_FILTER_BROADCAST | \
					 PARAM_PACKET_FILTER_ALL_MULTICAST)
#else
#define PARAM_PACKET_FILTER_SUPPORTED   (PARAM_PACKET_FILTER_DIRECTED | \
					 PARAM_PACKET_FILTER_MULTICAST | \
					 PARAM_PACKET_FILTER_BROADCAST | \
					 PARAM_PACKET_FILTER_ALL_MULTICAST)
#endif

#define PARAM_MEM_DUMP_MAX_SIZE         1536

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
#define PARAM_CAL_DATA_DUMP_MAX_SIZE	1200
#define PARAM_CAL_DATA_DUMP_MAX_NUM	300
#endif

#define BT_PROFILE_PARAM_LEN		8

/* Based on EEPROM layout 20160120 */
#define EFUSE_ADDR_MAX			0x3BF
#if CFG_SUPPORT_BUFFER_MODE

/* For MT7668 */
#define EFUSE_CONTENT_BUFFER_START	0x03A
#define EFUSE_CONTENT_BUFFER_END	0x1D9
#define EFUSE_CONTENT_BUFFER_SIZE	(EFUSE_CONTENT_BUFFER_END - \
					 EFUSE_CONTENT_BUFFER_START + 1)

/* For MT6632 */
#define EFUSE_CONTENT_SIZE		16

#define EFUSE_BLOCK_SIZE		16
#define EEPROM_SIZE			1184
#define MAX_EEPROM_BUFFER_SIZE		1450
#endif /* CFG_SUPPORT_BUFFER_MODE */

#if CFG_SUPPORT_TX_BF
#define TXBF_CMD_NEED_TO_RESPONSE(u4TxBfCmdId)	\
					(u4TxBfCmdId == BF_PFMU_TAG_READ || \
					 u4TxBfCmdId == BF_PROFILE_READ)
#endif /* CFG_SUPPORT_TX_BF */
#define MU_CMD_NEED_TO_RESPONSE(u4MuCmdId)	\
					(u4MuCmdId == MU_GET_CALC_INIT_MCS || \
					 u4MuCmdId == MU_HQA_GET_QD || \
					 u4MuCmdId == MU_HQA_GET_CALC_LQ)
#if CFG_SUPPORT_MU_MIMO
/* @NITESH: MACROS For Init MCS calculation (MU Metric Table) */
#define NUM_MUT_FEC             2
#define NUM_MUT_MCS             10
#define NUM_MUT_NR_NUM          3
#define NUM_MUT_INDEX           8

#define NUM_OF_USER             2
#define NUM_OF_MODUL            5
#endif /* CFG_SUPPORT_MU_MIMO */

#define SER_ACTION_QUERY                    0
#define SER_ACTION_SET                      1
#define SER_ACTION_SET_ENABLE_MASK          2
#define SER_ACTION_RECOVER                  3

/* SER_ACTION_SET sub action */
#define SER_SET_DISABLE         0
#define SER_SET_ENABLE          1

/* SER_ACTION_SET_ENABLE_MASK mask define */
#define SER_ENABLE_TRACKING         BIT(0)
#define SER_ENABLE_L1_RECOVER       BIT(1)
#define SER_ENABLE_L2_RECOVER       BIT(2)
#define SER_ENABLE_L3_RX_ABORT      BIT(3)
#define SER_ENABLE_L3_TX_ABORT      BIT(4)
#define SER_ENABLE_L3_TX_DISABLE    BIT(5)
#define SER_ENABLE_L3_BF_RECOVER    BIT(6)

/* SER_ACTION_RECOVER recover method */
#define SER_SET_L0_RECOVER         0
#define SER_SET_L1_RECOVER         1
#define SER_SET_L2_RECOVER         2
#define SER_SET_L3_RX_ABORT        3
#define SER_SET_L3_TX_ABORT        4
#define SER_SET_L3_TX_DISABLE      5
#define SER_SET_L3_BF_RECOVER      6


/* SER user command */
#define SER_USER_CMD_DISABLE         0
#define SER_USER_CMD_ENABLE          1

#define SER_USER_CMD_ENABLE_MASK_TRACKING_ONLY      (200)
#define SER_USER_CMD_ENABLE_MASK_L1_RECOVER_ONLY    (201)
#define SER_USER_CMD_ENABLE_MASK_L2_RECOVER_ONLY    (202)
#define SER_USER_CMD_ENABLE_MASK_L3_RX_ABORT_ONLY   (203)
#define SER_USER_CMD_ENABLE_MASK_L3_TX_ABORT_ONLY   (204)
#define SER_USER_CMD_ENABLE_MASK_L3_TX_DISABLE_ONLY (205)
#define SER_USER_CMD_ENABLE_MASK_L3_BFRECOVER_ONLY  (206)
#define SER_USER_CMD_ENABLE_MASK_RECOVER_ALL        (207)


/* Use a magic number to prevent human mistake */
#define SER_USER_CMD_L0_RECOVER          956
#define SER_USER_CMD_L1_RECOVER          995

#define SER_USER_CMD_L2_BN0_RECOVER      (300)
#define SER_USER_CMD_L2_BN1_RECOVER      (301)
#define SER_USER_CMD_L3_RX0_ABORT        (302)
#define SER_USER_CMD_L3_RX1_ABORT        (303)
#define SER_USER_CMD_L3_TX0_ABORT        (304)
#define SER_USER_CMD_L3_TX1_ABORT        (305)
#define SER_USER_CMD_L3_TX0_DISABLE      (306)
#define SER_USER_CMD_L3_TX1_DISABLE      (307)
#define SER_USER_CMD_L3_BF_RECOVER       (308)


#define TXPOWER_MAN_SET_INPUT_ARG_NUM 4

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
#define TXPOWER_INFO_INPUT_ARG_NUM 2
#define TXPOWER_FORMAT_LEGACY 0
#define TXPOWER_FORMAT_HE 1

/* 1M, 2M, 5.5M, 11M */
#define MODULATION_SYSTEM_CCK_NUM       4

/* 6M, 9M, 12M, 18M, 24M, 36M, 48M, 54M */
#define MODULATION_SYSTEM_OFDM_NUM      8

#define MODULATION_SYSTEM_HT20_NUM      8       /* MCS0~7 */
#define MODULATION_SYSTEM_HT40_NUM      9       /* MCS0~7, MCS32 */
#define MODULATION_SYSTEM_VHT20_NUM     10      /* MCS0~9 */
#define MODULATION_SYSTEM_VHT40_NUM     MODULATION_SYSTEM_VHT20_NUM
#define MODULATION_SYSTEM_VHT80_NUM     MODULATION_SYSTEM_VHT20_NUM
#define MODULATION_SYSTEM_VHT160_NUM    MODULATION_SYSTEM_VHT20_NUM

#define MODULATION_SYSTEM_HE_26_MCS_NUM      12
#define MODULATION_SYSTEM_HE_52_MCS_NUM      12
#define MODULATION_SYSTEM_HE_106_MCS_NUM     12
#define MODULATION_SYSTEM_HE_242_MCS_NUM     12
#define MODULATION_SYSTEM_HE_484_MCS_NUM     12
#define MODULATION_SYSTEM_HE_996_MCS_NUM     12
#define MODULATION_SYSTEM_HE_996X2_MCS_NUM   12

#define TXPOWER_RATE_CCK_OFFSET         (0)
#define TXPOWER_RATE_OFDM_OFFSET        (TXPOWER_RATE_CCK_OFFSET  + \
					 MODULATION_SYSTEM_CCK_NUM)
#define TXPOWER_RATE_HT20_OFFSET        (TXPOWER_RATE_OFDM_OFFSET + \
					 MODULATION_SYSTEM_OFDM_NUM)
#define TXPOWER_RATE_HT40_OFFSET        (TXPOWER_RATE_HT20_OFFSET + \
					 MODULATION_SYSTEM_HT20_NUM)
#define TXPOWER_RATE_VHT20_OFFSET       (TXPOWER_RATE_HT40_OFFSET + \
					 MODULATION_SYSTEM_HT40_NUM)
#define TXPOWER_RATE_VHT40_OFFSET       (TXPOWER_RATE_VHT20_OFFSET + \
					 MODULATION_SYSTEM_VHT20_NUM)
#define TXPOWER_RATE_VHT80_OFFSET       (TXPOWER_RATE_VHT40_OFFSET + \
					 MODULATION_SYSTEM_VHT40_NUM)
#define TXPOWER_RATE_VHT160_OFFSET      (TXPOWER_RATE_VHT80_OFFSET + \
					 MODULATION_SYSTEM_VHT80_NUM)

#define TXPOWER_RATE_HE26_OFFSET    (TXPOWER_RATE_VHT160_OFFSET)
#define TXPOWER_RATE_HE52_OFFSET    (TXPOWER_RATE_HE26_OFFSET + \
					MODULATION_SYSTEM_HE_26_MCS_NUM)
#define TXPOWER_RATE_HE106_OFFSET   (TXPOWER_RATE_HE52_OFFSET + \
					MODULATION_SYSTEM_HE_52_MCS_NUM)
#define TXPOWER_RATE_HE242_OFFSET   (TXPOWER_RATE_HE106_OFFSET + \
					MODULATION_SYSTEM_HE_106_MCS_NUM)
#define TXPOWER_RATE_HE484_OFFSET   (TXPOWER_RATE_HE242_OFFSET + \
					MODULATION_SYSTEM_HE_242_MCS_NUM)
#define TXPOWER_RATE_HE996_OFFSET   (TXPOWER_RATE_HE484_OFFSET + \
					MODULATION_SYSTEM_HE_484_MCS_NUM)
#define TXPOWER_RATE_HE996X2_OFFSET (TXPOWER_RATE_HE996_OFFSET + \
					MODULATION_SYSTEM_HE_996_MCS_NUM)
#define TXPOWER_RATE_NUM            (TXPOWER_RATE_HE996X2_OFFSET + \
					MODULATION_SYSTEM_HE_996X2_MCS_NUM)
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE
#define GED_EVENT_GAS               (1 << 4)
#define GED_EVENT_NETWORK           (1 << 11)
#define GED_EVENT_DOPT_WIFI_SCAN    (1 << 12)
#define GED_EVENT_TX_DUP_DETECT     (1 << 13)

#define LOW_LATENCY_MODE_MAGIC_CODE      0x86
#define LOW_LATENCY_MODE_CMD_V2          0x2
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Parameters of User Configuration which match to NDIS5.1                    */
/*----------------------------------------------------------------------------*/

enum ENUM_PARAM_PHY_TYPE {
	PHY_TYPE_802_11ABG = 0,	/*!< Can associated with 802.11abg AP,
				 * Scan dual band.
				 */
	PHY_TYPE_802_11BG,	/*!< Can associated with 802_11bg AP,
				 * Scan single band and not report 802_11a
				 * BSSs.
				 */
	PHY_TYPE_802_11G,	/*!< Can associated with 802_11g only AP,
				 * Scan single band and not report 802_11ab
				 * BSSs.
				 */
	PHY_TYPE_802_11A,	/*!< Can associated with 802_11a only AP,
				 * Scan single band and not report 802_11bg
				 * BSSs.
				 */
	PHY_TYPE_802_11B,	/*!< Can associated with 802_11b only AP
				 * Scan single band and not report 802_11ag
				 * BSSs.
				 */
	PHY_TYPE_NUM		/* 5 */
};

enum ENUM_PARAM_OP_MODE {
	NET_TYPE_IBSS = 0,	/*!< Try to merge/establish an AdHoc,
				 * do periodic SCAN for merging.
				 */
	NET_TYPE_INFRA,		/*!< Try to join an Infrastructure,
				 * do periodic SCAN for joining.
				 */
	NET_TYPE_AUTO_SWITCH,	/*!< Try to join an Infrastructure,
				 * if fail then try to merge or
				 */
	/*  establish an AdHoc, do periodic SCAN for joining or merging. */
	NET_TYPE_DEDICATED_IBSS,/*!< Try to merge an AdHoc first, */
	/* if fail then establish AdHoc permanently, no more SCAN. */
	NET_TYPE_NUM		/* 4 */
};

struct PARAM_CONNECT {
	uint32_t u4SsidLen;	/*!< SSID length in bytes.
				 * Zero length is broadcast(any) SSID
				 */
	uint8_t *pucSsid;
	uint8_t *pucBssid;
	uint8_t *pucBssidHint;
	uint32_t u4CenterFreq;
	uint8_t ucBssIdx;
};

struct PARAM_EXTERNAL_AUTH {
	uint8_t bssid[PARAM_MAC_ADDR_LEN];
	uint16_t status;
	uint8_t ucBssIdx;
};

struct PARAM_OP_MODE {
	enum ENUM_PARAM_OP_MODE eOpMode;
	uint8_t ucBssIdx;
};

/* This is enum defined for user to select an AdHoc Mode */
enum ENUM_PARAM_AD_HOC_MODE {
	AD_HOC_MODE_11B = 0,	/*!< Create 11b IBSS if we support
				 * 802.11abg/802.11bg.
				 */
	AD_HOC_MODE_MIXED_11BG,	/*!< Create 11bg mixed IBSS if we support
				 * 802.11abg/802.11bg/802.11g.
				 */
	AD_HOC_MODE_11G,	/*!< Create 11g only IBSS if we support
				 * 802.11abg/802.11bg/802.11g.
				 */
	AD_HOC_MODE_11A,	/*!< Create 11a only IBSS if we support
				 * 802.11abg.
				 */
	AD_HOC_MODE_NUM		/* 4 */
};

enum ENUM_PARAM_NETWORK_TYPE {
	PARAM_NETWORK_TYPE_FH,
	PARAM_NETWORK_TYPE_DS,
	PARAM_NETWORK_TYPE_OFDM5,
	PARAM_NETWORK_TYPE_OFDM24,
	PARAM_NETWORK_TYPE_AUTOMODE,
	PARAM_NETWORK_TYPE_NUM	/*!< Upper bound, not real case */
};

struct PARAM_NETWORK_TYPE_LIST {
	uint32_t NumberOfItems;	/*!< At least 1 */
	enum ENUM_PARAM_NETWORK_TYPE eNetworkType[1];
};

enum ENUM_PARAM_PRIVACY_FILTER {
	PRIVACY_FILTER_ACCEPT_ALL,
	PRIVACY_FILTER_8021xWEP,
	PRIVACY_FILTER_NUM
};

enum ENUM_RELOAD_DEFAULTS {
	ENUM_RELOAD_WEP_KEYS
};

struct PARAM_PM_PACKET_PATTERN {
	uint32_t Priority;	/* Importance of the given pattern. */
	uint32_t Reserved;	/* Context information for transports. */
	uint32_t MaskSize;	/* Size in bytes of the pattern mask. */
	uint32_t PatternOffset;	/* Offset from beginning of this */
	/* structure to the pattern bytes. */
	uint32_t PatternSize;	/* Size in bytes of the pattern. */
	uint32_t PatternFlags;	/* Flags (TBD). */
};


/* Combine ucTpTestMode and ucSigmaTestMode in one flag */
/* ucTpTestMode == 0, for normal driver */
/* ucTpTestMode == 1, for pure throughput test mode (ex: RvR) */
/* ucTpTestMode == 2, for sigma TGn/TGac/PMF */
/* ucTpTestMode == 3, for sigma WMM PS */
enum ENUM_TP_TEST_MODE {
	ENUM_TP_TEST_MODE_NORMAL = 0,
	ENUM_TP_TEST_MODE_THROUGHPUT,
	ENUM_TP_TEST_MODE_SIGMA_AC_N_PMF,
	ENUM_TP_TEST_MODE_SIGMA_WMM_PS,
	ENUM_TP_TEST_MODE_NUM
};

/*--------------------------------------------------------------*/
/*! \brief Struct definition to indicate specific event.        */
/*--------------------------------------------------------------*/
enum ENUM_STATUS_TYPE {
	ENUM_STATUS_TYPE_AUTHENTICATION,
	ENUM_STATUS_TYPE_MEDIA_STREAM_MODE,
	ENUM_STATUS_TYPE_CANDIDATE_LIST,
	ENUM_STATUS_TYPE_FT_AUTH_STATUS,
	ENUM_STATUS_TYPE_NUM	/*!< Upper bound, not real case */
};

struct PARAM_802_11_CONFIG_FH {
	uint32_t u4Length;	/*!< Length of structure */
	uint32_t u4HopPattern;	/*!< Defined as 802.11 */
	uint32_t u4HopSet;	/*!< to one if non-802.11 */
	uint32_t u4DwellTime;	/*!< In unit of Kusec */
};

struct PARAM_802_11_CONFIG {
	uint32_t u4Length;	/*!< Length of structure */
	uint32_t u4BeaconPeriod;	/*!< In unit of Kusec */
	uint32_t u4ATIMWindow;	/*!< In unit of Kusec */
	uint32_t u4DSConfig;	/*!< Channel frequency in unit of kHz */
	struct PARAM_802_11_CONFIG_FH rFHConfig;
};

struct PARAM_STATUS_INDICATION {
	enum ENUM_STATUS_TYPE eStatusType;
};

struct PARAM_AUTH_REQUEST {
	uint32_t u4Length;	/*!< Length of this struct */
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4Flags;	/*!< Definitions are as follows */
};

struct PARAM_PMKID {
	uint8_t arBSSID[PARAM_MAC_ADDR_LEN];
	uint8_t arPMKID[IW_PMKID_LEN];
	uint8_t ucBssIdx;
};

struct PARAM_PMKID_CANDIDATE {
	uint8_t arBSSID[PARAM_MAC_ADDR_LEN];
	uint32_t u4Flags;
};

struct PARAM_INDICATION_EVENT {
	struct PARAM_STATUS_INDICATION rStatus;
	union {
		struct PARAM_AUTH_REQUEST rAuthReq;
		struct PARAM_PMKID_CANDIDATE rCandi;
	};
};

/*! \brief Capabilities, privacy, rssi and IEs of each BSSID */
struct PARAM_BSSID_EX {
	uint32_t u4Length;	/*!< Length of structure */
	uint8_t arMacAddress[PARAM_MAC_ADDR_LEN];	/*!< BSSID */
	uint8_t Reserved[2];
	struct PARAM_SSID rSsid;	/*!< SSID */
	uint32_t u4Privacy;	/*!< Need WEP encryption */
	int32_t rRssi;	/*!< in dBm */
	enum ENUM_PARAM_NETWORK_TYPE eNetworkTypeInUse;
	struct PARAM_802_11_CONFIG rConfiguration;
	enum ENUM_PARAM_OP_MODE eOpMode;
	uint8_t rSupportedRates[PARAM_MAX_LEN_RATES_EX];
	uint32_t u4IELength;
	uint8_t aucIEs[1];
};

struct PARAM_BSSID_LIST_EX {
	uint32_t u4NumberOfItems;	/*!< at least 1 */
	struct PARAM_BSSID_EX arBssid[1];
};

struct PARAM_WEP {
	uint32_t u4Length;	/*!< Length of structure */
	uint32_t u4KeyIndex;	/*!< 0: pairwise key, others group keys */
	uint32_t u4KeyLength;	/*!< Key length in bytes */
	uint8_t aucKeyMaterial[32];	/*!< Key content by above setting */
};

/*! \brief Key mapping of BSSID */
struct PARAM_KEY {
	uint32_t u4Length;	/*!< Length of structure */
	uint32_t u4KeyIndex;	/*!< KeyID */
	uint32_t u4KeyLength;	/*!< Key length in bytes */
	uint8_t arBSSID[PARAM_MAC_ADDR_LEN];	/*!< MAC address */
	uint64_t rKeyRSC;
	uint8_t ucBssIdx;
	uint8_t ucCipher;
	uint8_t aucKeyMaterial[32];	/*!< Key content by above setting */
	/* Following add to change the original windows structure */
};

struct PARAM_REMOVE_KEY {
	uint32_t u4Length;	/*!< Length of structure */
	uint32_t u4KeyIndex;	/*!< KeyID */
	uint8_t arBSSID[PARAM_MAC_ADDR_LEN];	/*!< MAC address */
	uint8_t ucBssIdx;
};

/*! \brief Default key */
struct PARAM_DEFAULT_KEY {
	uint8_t ucKeyID;
	uint8_t ucUnicast;
	uint8_t ucMulticast;
	uint8_t ucBssIdx;
};

#if CFG_SUPPORT_WAPI
enum ENUM_KEY_TYPE {
	ENUM_WPI_PAIRWISE_KEY = 0,
	ENUM_WPI_GROUP_KEY
};

enum ENUM_WPI_PROTECT_TYPE {
	ENUM_WPI_NONE,
	ENUM_WPI_RX,
	ENUM_WPI_TX,
	ENUM_WPI_RX_TX
};

struct PARAM_WPI_KEY {
	enum ENUM_KEY_TYPE eKeyType;
	enum ENUM_WPI_PROTECT_TYPE eDirection;
	uint8_t ucKeyID;
	uint8_t aucRsv[3];
	uint8_t aucAddrIndex[12];
	uint32_t u4LenWPIEK;
	uint8_t aucWPIEK[256];
	uint32_t u4LenWPICK;
	uint8_t aucWPICK[256];
	uint8_t aucPN[16];
	uint8_t ucBssIdx;
};
#endif

enum PARAM_POWER_MODE {
	Param_PowerModeCAM,
	Param_PowerModeMAX_PSP,
	Param_PowerModeFast_PSP,
	Param_PowerModeMax	/* Upper bound, not real case */
};

enum PARAM_DEVICE_POWER_STATE {
	ParamDeviceStateUnspecified = 0,
	ParamDeviceStateD0,
	ParamDeviceStateD1,
	ParamDeviceStateD2,
	ParamDeviceStateD3,
	ParamDeviceStateMaximum
};

struct PARAM_POWER_MODE_ {
	uint8_t ucBssIdx;
	enum PARAM_POWER_MODE ePowerMode;
};

#if CFG_SUPPORT_802_11D

/*! \brief The enumeration definitions for OID_IPN_MULTI_DOMAIN_CAPABILITY */
enum PARAM_MULTI_DOMAIN_CAPABILITY {
	ParamMultiDomainCapDisabled,
	ParamMultiDomainCapEnabled
};
#endif

struct COUNTRY_STRING_ENTRY {
	uint8_t aucCountryCode[2];
	uint8_t aucEnvironmentCode[2];
};

/* Power management related definition and enumerations */
#define UAPSD_NONE	0
#define UAPSD_AC0	(BIT(0) | BIT(4))
#define UAPSD_AC1	(BIT(1) | BIT(5))
#define UAPSD_AC2	(BIT(2) | BIT(6))
#define UAPSD_AC3	(BIT(3) | BIT(7))
#define UAPSD_ALL	(UAPSD_AC0 | UAPSD_AC1 | UAPSD_AC2 | UAPSD_AC3)

enum ENUM_POWER_SAVE_PROFILE {
	ENUM_PSP_CONTINUOUS_ACTIVE = 0,
	ENUM_PSP_CONTINUOUS_POWER_SAVE,
	ENUM_PSP_FAST_SWITCH,
	ENUM_PSP_NUM
};

struct LINK_SPEED_EX_ {
	int8_t cRssi;

	uint8_t fgIsLinkQualityValid;
	OS_SYSTIME rLinkQualityUpdateTime;
	int8_t cLinkQuality;

	uint8_t fgIsLinkRateValid;
	OS_SYSTIME rLinkRateUpdateTime;
	uint32_t u2LinkSpeed;

	uint8_t ucMediumBusyPercentage;
	uint8_t ucIsLQ0Rdy;
};

struct PARAM_LINK_SPEED_EX {
	struct LINK_SPEED_EX_ rLq[BSSID_NUM];
};

/*--------------------------------------------------------------*/
/*! \brief Set/Query authentication and encryption capability.  */
/*--------------------------------------------------------------*/
struct PARAM_AUTH_ENCRYPTION {
	enum ENUM_PARAM_AUTH_MODE eAuthModeSupported;
	enum ENUM_WEP_STATUS eEncryptStatusSupported;
};

struct PARAM_CAPABILITY {
	uint32_t u4Length;
	uint32_t u4Version;
	uint32_t u4NoOfAuthEncryptPairsSupported;
	struct PARAM_AUTH_ENCRYPTION
		arAuthenticationEncryptionSupported[1];
};

#define NL80211_KCK_LEN                 16
#define NL80211_KEK_LEN                 16
#define NL80211_REPLAY_CTR_LEN          8

struct PARAM_GTK_REKEY_DATA {
	uint8_t aucKek[NL80211_KEK_LEN];
	uint8_t aucKck[NL80211_KCK_LEN];
	uint8_t aucReplayCtr[NL80211_REPLAY_CTR_LEN];
	uint8_t ucBssIndex;
	uint8_t ucRsv[3];
	uint32_t u4Proto;
	uint32_t u4PairwiseCipher;
	uint32_t u4GroupCipher;
	uint32_t u4KeyMgmt;
	uint32_t u4MgmtGroupCipher;
};

struct PARAM_CUSTOM_MCR_RW_STRUCT {
	uint32_t u4McrOffset;
	uint32_t u4McrData;
};

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
/*
 * Description of Each Parameters :
 * ucReason :
 * 0 : Query Information of Thermal or Cal Data Length
 * 1 : Trigger FW do or don't All Cal
 * 2 : Dump Data to Host
 * 3 : Send Backupped Cal Data to FW
 * 4 : For Debug Use, Tell FW Print Cal Data (Rom or Ram)
 * ucAction :
 * 0 : Read Thermal Value
 * 1 : Ask the Cal Data Total Length (Rom and Ram)
 * 2 : Tell FW do All Cal
 * 3 : Tell FW don't do Cal
 * 4 : Dump Data to Host (Rom or Ram)
 * 5 : Send Backupped Cal Data to FW (Rom or Ram)
 * 6 : For Debug Use, Tell FW Print Cal Data (Rom or Ram)
 * ucNeedResp :
 * 0 : FW No Need to Response an EVENT
 * 1 : FW Need to Response an EVENT
 * ucFragNum :
 * Sequence Number
 * ucRomRam :
 * 0 : Operation for Rom Cal Data
 * 1 : Operation for Ram Cal Data
 * u4ThermalValue :
 * Field for filling the Thermal Value in FW
 * u4Address :
 * Dumpped Starting Address
 * Used for Dump and Send Cal Data Between Driver and FW
 * u4Length :
 * Memory Size need to allocated in Driver or Data Size in an EVENT
 * Used for Dump and Send Cal Data Between Driver and FW
 * u4RemainLength :
 * Remain Length need to Dump
 * Used for Dump and Send Cal Data Between Driver and FW
 */
struct PARAM_CAL_BACKUP_STRUCT_V2 {
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

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
struct BIN_CONTENT {
	uint16_t u2Addr;
	uint8_t ucValue;
	uint8_t ucReserved;
};

struct PARAM_CUSTOM_EFUSE_BUFFER_MODE {
	uint8_t ucSourceMode;
	uint8_t ucCount;
	uint8_t ucCmdType;
	uint8_t ucReserved;
	uint8_t aBinContent[MAX_EEPROM_BUFFER_SIZE];
};

struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T {
	uint8_t ucSourceMode;
	uint8_t ucContentFormat;
	uint16_t u2Count;
	uint8_t aBinContent[MAX_EEPROM_BUFFER_SIZE];
};

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
struct PARAM_CUSTOM_ACCESS_EFUSE {
	uint32_t u4Address;
	uint32_t u4Valid;
	uint8_t aucData[16];
};

struct PARAM_CUSTOM_EFUSE_FREE_BLOCK {
	uint8_t  ucGetFreeBlock;
	uint8_t  aucReserved[3];
};

struct PARAM_CUSTOM_GET_TX_POWER {
	uint8_t ucTxPwrType;
	uint8_t ucCenterChannel;
	uint8_t ucDbdcIdx; /* 0:Band 0, 1: Band1 */
	uint8_t ucBand; /* 0:G-band 1: A-band*/
	uint8_t ucReserved[4];
};

/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */

struct PARAM_CUSTOM_SET_TX_TARGET_POWER {
	int8_t cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
	int8_t cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	uint8_t ucTxTargetPwr;	/* Tx target power base for all*/
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

#if (CFG_SUPPORT_DFS_MASTER == 1)
struct PARAM_CUSTOM_SET_RDD_REPORT {
	uint8_t ucDbdcIdx; /* 0:Band 0, 1: Band1 */
};

struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE {
	/* 0:Switch channel, 1: Don't switch channel */
	uint8_t ucRadarDetectMode;
};
#endif

struct PARAM_CUSTOM_ACCESS_RX_STAT {
	uint32_t u4SeqNum;
	uint32_t u4TotalNum;
};

/* Ext DevInfo Tag */
enum EXT_ENUM_DEVINFO_TAG_HANDLE {
	DEV_INFO_ACTIVE = 0,
	DEV_INFO_BSSID,
	DEV_INFO_MAX_NUM
};

/*  STA record TLV tag */
enum EXT_ENUM_STAREC_TAG_HANDLE {
	STA_REC_BASIC = 0,
	STA_REC_RA,
	STA_REC_RA_COMMON_INFO,
	STA_REC_RA_UPDATE,
	STA_REC_BF,
	STA_REC_MAUNAL_ASSOC,
	STA_REC_BA = 6,
	STA_REC_MAX_NUM
};

#if CFG_SUPPORT_TX_BF
enum BF_ACTION_CATEGORY {
	BF_SOUNDING_OFF = 0,
	BF_SOUNDING_ON,
	BF_DATA_PACKET_APPLY,
	BF_PFMU_MEM_ALLOCATE,
	BF_PFMU_MEM_RELEASE,
	BF_PFMU_TAG_READ,
	BF_PFMU_TAG_WRITE,
	BF_PROFILE_READ,
	BF_PROFILE_WRITE,
	BF_PN_READ,
	BF_PN_WRITE,
	BF_PFMU_MEM_ALLOC_MAP_READ,
#if CFG_SUPPORT_TX_BF_FPGA
	BF_PFMU_SW_TAG_WRITE = 23
#endif
};

enum {
	DEVINFO_ACTIVE = 0,
	DEVINFO_MAX_NUM = 1,
};

enum {
	DEVINFO_ACTIVE_FEATURE = (1 << DEVINFO_ACTIVE),
	DEVINFO_MAX_NUM_FEATURE = (1 << DEVINFO_MAX_NUM)
};

enum {
	BSS_INFO_OWN_MAC = 0,
	BSS_INFO_BASIC = 1,
	BSS_INFO_RF_CH = 2,
	BSS_INFO_PM = 3,
	BSS_INFO_UAPSD = 4,
	BSS_INFO_ROAM_DETECTION = 5,
	BSS_INFO_LQ_RM = 6,
	BSS_INFO_EXT_BSS = 7,
	BSS_INFO_BROADCAST_INFO = 8,
	BSS_INFO_SYNC_MODE = 9,
	BSS_INFO_MAX_NUM
};

union PFMU_PROFILE_TAG1 {
	struct {
		/* [6:0] : 0 ~ 63 */
		uint32_t ucProfileID: 7;

		/* [7] : 0: iBF, 1: eBF */
		uint32_t ucTxBf: 1;

		/* [9:8] : 0/1/2/3: DW20/40/80/160NC */
		uint32_t ucDBW: 2;

		/* [10] : 0:SU, 1: MU */
		uint32_t ucSU_MU: 1;

		/* [11] : 0: default, 1: This profile number is invalid by SW */
		uint32_t ucInvalidProf:	1;

		/* [14:12] : RMSD value from CE */
		uint32_t ucRMSD: 3;

		/* [17 : 15] : column index : 0 ~ 5 */
		uint32_t ucMemAddr1ColIdx: 3;

		/* [23 : 18] : row index : 0 ~ 63 */
		uint32_t ucMemAddr1RowIdx: 6;

		/* [26 : 24] : column index : 0 ~ 5 */
		uint32_t ucMemAddr2ColIdx: 3;

		/* [31 : 27] : row index : 0 ~ 63 */
		uint32_t ucMemAddr2RowIdx: 5;

		/* [32] : MSB of row index */
		uint32_t ucMemAddr2RowIdxMsb: 1;

		/* [35 : 33] : column index : 0 ~ 5 */
		uint32_t ucMemAddr3ColIdx: 3;

		/* [41 : 36] : row index : 0 ~ 63 */
		uint32_t ucMemAddr3RowIdx: 6;

		/* [44 : 42] : column index : 0 ~ 5 */
		uint32_t ucMemAddr4ColIdx: 3;

		/* [50 : 45] : row index : 0 ~ 63 */
		uint32_t ucMemAddr4RowIdx: 6;

		/* [51] : Reserved */
		uint32_t ucReserved: 1;

		/* [53 : 52] : Nrow */
		uint32_t ucNrow: 2;

		/* [55 : 54] : Ncol */
		uint32_t ucNcol: 2;

		/* [57 : 56] : Ngroup */
		uint32_t ucNgroup: 2;

		/* [59 : 58] : 0/1/2 */
		uint32_t ucLM: 2;

		/* [61:60] : Code book */
		uint32_t ucCodeBook: 2;

		/* [62] : HtcExist */
		uint32_t ucHtcExist: 1;

		/* [63] : Reserved */
		uint32_t ucReserved1: 1;

		/* [71:64] : SNR_STS0 */
		uint32_t ucSNR_STS0: 8;

		/* [79:72] : SNR_STS1 */
		uint32_t ucSNR_STS1: 8;

		/* [87:80] : SNR_STS2 */
		uint32_t ucSNR_STS2: 8;

		/* [95:88] : SNR_STS3 */
		uint32_t ucSNR_STS3: 8;

		/* [103:96] : iBF LNA index */
		uint32_t ucIBfLnaIdx: 8;
	} rField;
	uint32_t au4RawData[4];
};

union PFMU_PROFILE_TAG2 {
	struct {
		uint32_t u2SmartAnt: 12;/* [11:0] : Smart Ant config */
		uint32_t ucReserved0: 3;/* [14:12] : Reserved */
		uint32_t ucSEIdx: 5;	/* [19:15] : SE index */
		uint32_t ucRMSDThd: 3;	/* [22:20] : RMSD Threshold */
		uint32_t ucReserved1: 1;/* [23] : Reserved */
		uint32_t ucMCSThL1SS: 4;/* [27:24] : MCS TH long 1SS */
		uint32_t ucMCSThS1SS: 4;/* [31:28] : MCS TH short 1SS */
		uint32_t ucMCSThL2SS: 4;/* [35:32] : MCS TH long 2SS */
		uint32_t ucMCSThS2SS: 4;/* [39:36] : MCS TH short 2SS */
		uint32_t ucMCSThL3SS: 4;/* [43:40] : MCS TH long 3SS */
		uint32_t ucMCSThS3SS: 4;/* [47:44] : MCS TH short 3SS */
		uint32_t uciBfTimeOut: 8;/* [55:48] : iBF timeout limit */
		uint32_t ucReserved2: 8;/* [63:56] : Reserved */
		uint32_t ucReserved3: 8;/* [71:64] : Reserved */
		uint32_t ucReserved4: 8;/* [79:72] : Reserved */
		uint32_t uciBfDBW: 2;	/* [81:80] : iBF desired DBW 0/1/2/3 :
					 *           BW20/40/80/160NC
					 */
		uint32_t uciBfNcol: 2;	/* [83:82] : iBF desired Ncol = 1 ~ 3 */
		uint32_t uciBfNrow: 2;	/* [85:84] : iBF desired Nrow = 1 ~ 4 */
		uint32_t u2Reserved5: 10;/* [95:86] : Reserved */
	} rField;
	uint32_t au4RawData[3];
};

union PFMU_DATA {
	struct {
		uint32_t u2Phi11: 9;
		uint32_t ucPsi21: 7;
		uint32_t u2Phi21: 9;
		uint32_t ucPsi31: 7;
		uint32_t u2Phi31: 9;
		uint32_t ucPsi41: 7;
		uint32_t u2Phi22: 9;
		uint32_t ucPsi32: 7;
		uint32_t u2Phi32: 9;
		uint32_t ucPsi42: 7;
		uint32_t u2Phi33: 9;
		uint32_t ucPsi43: 7;
		uint32_t u2dSNR00: 4;
		uint32_t u2dSNR01: 4;
		uint32_t u2dSNR02: 4;
		uint32_t u2dSNR03: 4;
		uint32_t u2Reserved: 16;
	} rField;
	uint32_t au4RawData[5];
};

struct PROFILE_TAG_READ {
	uint8_t ucTxBfCategory;
	uint8_t ucProfileIdx;
	u_int8_t fgBfer;
	uint8_t ucRsv;
};

struct PROFILE_TAG_WRITE {
	uint8_t ucTxBfCategory;
	uint8_t ucPfmuId;
	uint8_t ucBuffer[28];
};

struct PROFILE_DATA_READ {
	uint8_t ucTxBfCategory;
	uint8_t ucPfmuIdx;
	u_int8_t fgBFer;
	uint8_t ucReserved[3];
	uint8_t ucSubCarrIdxLsb;
	uint8_t ucSubCarrIdxMsb;
};

struct PROFILE_DATA_WRITE {
	uint8_t ucTxBfCategory;
	uint8_t ucPfmuIdx;
	uint8_t u2SubCarrIdxLsb;
	uint8_t u2SubCarrIdxMsb;
	union PFMU_DATA rTxBfPfmuData;
};

struct PROFILE_PN_READ {
	uint8_t ucTxBfCategory;
	uint8_t ucPfmuIdx;
	uint8_t ucReserved[2];
};

struct PROFILE_PN_WRITE {
	uint8_t ucTxBfCategory;
	uint8_t ucPfmuIdx;
	uint16_t u2bw;
	uint8_t ucBuf[32];
};

enum BF_SOUNDING_MODE {
	SU_SOUNDING = 0,
	MU_SOUNDING,
	SU_PERIODIC_SOUNDING,
	MU_PERIODIC_SOUNDING
};

struct EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL {
	uint8_t ucCmdCategoryID;
	uint8_t ucSuMuSndMode;
	uint8_t ucWlanIdx;
	uint32_t u4SoundingInterval;	/* By ms */
};

struct EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL {
	uint8_t ucCmdCategoryID;
	uint8_t ucSuMuSndMode;
	uint8_t ucWlanId[4];
	uint8_t ucStaNum;
	uint32_t u4SoundingInterval;	/* By ms */
};

/* Device information (Tag0) */
struct CMD_DEVINFO_ACTIVE {
	uint16_t u2Tag;		/* Tag = 0x00 */
	uint16_t u2Length;
	uint8_t ucActive;
	uint8_t ucBandNum;
	uint8_t aucOwnMacAddr[6];
	uint8_t aucReserve[4];
};

struct BSSINFO_BASIC {
	/* Basic BSS information (Tag1) */
	uint16_t u2Tag;		/* Tag = 0x01 */
	uint16_t u2Length;
	uint32_t u4NetworkType;
	uint8_t ucActive;
	uint8_t ucReserve0;
	uint16_t u2BcnInterval;
	uint8_t aucBSSID[6];
	uint8_t ucWmmIdx;
	uint8_t ucDtimPeriod;
	uint8_t ucBcMcWlanidx;	/* indicate which wlan-idx used for MC/BC
				 * transmission.
				 */
	uint8_t ucCipherSuit;
	uint8_t acuReserve[6];
};

struct TXBF_PFMU_STA_INFO {
	uint16_t u2PfmuId;	/* 0xFFFF means no access right for PFMU */
	uint8_t fgSU_MU;		/* 0 : SU, 1 : MU */
	uint8_t fgETxBfCap;	/* 0 : ITxBf, 1 : ETxBf */
	uint8_t ucSoundingPhy;	/* 0: legacy, 1: OFDM, 2: HT, 4: VHT */
	uint8_t ucNdpaRate;
	uint8_t ucNdpRate;
	uint8_t ucReptPollRate;
	uint8_t ucTxMode;	/* 0: legacy, 1: OFDM, 2: HT, 4: VHT */
	uint8_t ucNc;
	uint8_t ucNr;
	uint8_t ucCBW;		/* 0 : 20M, 1 : 40M, 2 : 80M, 3 : 80 + 80M */
	uint8_t ucTotMemRequire;
	uint8_t ucMemRequire20M;
	uint8_t ucMemRow0;
	uint8_t ucMemCol0;
	uint8_t ucMemRow1;
	uint8_t ucMemCol1;
	uint8_t ucMemRow2;
	uint8_t ucMemCol2;
	uint8_t ucMemRow3;
	uint8_t ucMemCol3;
	uint16_t u2SmartAnt;
	uint8_t ucSEIdx;
	uint8_t uciBfTimeOut;
	uint8_t uciBfDBW;
	uint8_t uciBfNcol;
	uint8_t uciBfNrow;
	uint8_t aucReserved[3];
};

struct STA_REC_UPD_ENTRY {
	struct TXBF_PFMU_STA_INFO rTxBfPfmuStaInfo;
	uint8_t aucAddr[PARAM_MAC_ADDR_LEN];
	uint8_t ucAid;
	uint8_t ucRsv;
};

struct STAREC_COMMON {
	/* Basic STA record (Group0) */
	uint16_t u2Tag;		/* Tag = 0x00 */
	uint16_t u2Length;
	uint32_t u4ConnectionType;
	uint8_t ucConnectionState;
	uint8_t ucIsQBSS;
	uint16_t u2AID;
	uint8_t aucPeerMacAddr[6];
	uint16_t u2Reserve1;
};

struct CMD_STAREC_BF {
	uint16_t u2Tag;		/* Tag = 0x02 */
	uint16_t u2Length;
	struct TXBF_PFMU_STA_INFO rTxBfPfmuInfo;
	uint8_t ucReserved[3];
};

/* QA tool: maunal assoc */
struct CMD_MANUAL_ASSOC_STRUCT {
	/*
	 *	uint8_t              ucBssIndex;
	 *	uint8_t              ucWlanIdx;
	 *	uint16_t             u2TotalElementNum;
	 *	uint32_t             u4Reserve;
	 */
	/* extension */
	uint16_t u2Tag;		/* Tag = 0x05 */
	uint16_t u2Length;
	uint8_t aucMac[MAC_ADDR_LEN];
	uint8_t ucType;
	uint8_t ucWtbl;
	uint8_t ucOwnmac;
	uint8_t ucMode;
	uint8_t ucBw;
	uint8_t ucNss;
	uint8_t ucPfmuId;
	uint8_t ucMarate;
	uint8_t ucSpeIdx;
	uint8_t ucaid;
};

struct TX_BF_SOUNDING_START {
	union {
		struct EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL
			rExtCmdExtBfSndPeriodicTriggerCtrl;
		struct EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL
			rExtCmdExtBfMuSndPeriodicTriggerCtrl;
	} rTxBfSounding;
};

struct TX_BF_SOUNDING_STOP {
	uint8_t ucTxBfCategory;
	uint8_t ucSndgStop;
	uint8_t ucReserved[2];
};

struct TX_BF_TX_APPLY {
	uint8_t ucTxBfCategory;
	uint8_t ucWlanId;
	uint8_t fgETxBf;
	uint8_t fgITxBf;
	uint8_t fgMuTxBf;
	uint8_t ucReserved[3];
};

struct TX_BF_PFMU_MEM_ALLOC {
	uint8_t ucTxBfCategory;
	uint8_t ucSuMuMode;
	uint8_t ucWlanIdx;
	uint8_t ucReserved;
};

struct TX_BF_PFMU_MEM_RLS {
	uint8_t ucTxBfCategory;
	uint8_t ucWlanId;
	uint8_t ucReserved[2];
};

#if CFG_SUPPORT_TX_BF_FPGA
struct TX_BF_PROFILE_SW_TAG_WRITE {
	uint8_t ucTxBfCategory;
	uint8_t ucLm;
	uint8_t ucNr;
	uint8_t ucNc;
	uint8_t ucBw;
	uint8_t ucCodebook;
	uint8_t ucgroup;
	uint8_t ucReserved;
};
#endif

union PARAM_CUSTOM_TXBF_ACTION_STRUCT {
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

struct PARAM_CUSTOM_STA_REC_UPD_STRUCT {
	uint8_t ucBssIndex;
	uint8_t ucWlanIdx;
	uint16_t u2TotalElementNum;
	uint8_t ucAppendCmdTLV;
	uint8_t ucMuarIdx;
	uint8_t aucReserve[2];
	uint32_t *prStaRec;
	struct CMD_STAREC_BF rCmdStaRecBf;
};

struct BSSINFO_ARGUMENT {
	uint8_t OwnMacIdx;
	uint8_t ucBssIndex;
	uint8_t Bssid[PARAM_MAC_ADDR_LEN];
	uint8_t ucBcMcWlanIdx;
	uint8_t ucPeerWlanIdx;
	uint32_t NetworkType;
	uint32_t u4ConnectionType;
	uint8_t CipherSuit;
	uint8_t Active;
	uint8_t WmmIdx;
	uint32_t u4BssInfoFeature;
	uint8_t aucBuffer[0];
};

struct PARAM_CUSTOM_PFMU_TAG_READ_STRUCT {
	union PFMU_PROFILE_TAG1 ru4TxBfPFMUTag1;
	union PFMU_PROFILE_TAG2 ru4TxBfPFMUTag2;
};

#if CFG_SUPPORT_MU_MIMO
struct PARAM_CUSTOM_SHOW_GROUP_TBL_ENTRY_STRUCT {
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

struct PARAM_CUSTOM_GET_QD_STRUCT {
	uint32_t u4EventId;
	uint32_t au4RawData[14];
};

struct MU_STRUCT_LQ_REPORT {
	int lq_report[NUM_OF_USER][NUM_OF_MODUL];
};

struct PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT {
	uint32_t u4EventId;
	struct MU_STRUCT_LQ_REPORT rEntry;
};

struct MU_GET_CALC_INIT_MCS {
	uint8_t ucgroupIdx;
	uint8_t ucRsv[3];
};

struct MU_SET_INIT_MCS {
	uint8_t ucNumOfUser;	/* zero-base: 0~3: means 1~2 users */
	uint8_t ucBandwidth;	/* zero-base: 0:20 hz 1:40 hz 2: 80 hz 3: 160 */
	uint8_t ucNssOfUser0;	/* zero-base: 0~1 means uesr0 use 1~2 ss,
				 *            if no use keep 0
				 */
	uint8_t ucNssOfUser1;	/* zero-base: 0~1 means uesr0 use 1~2 ss,
				 *            if no use keep 0
				 */
	uint8_t ucPfMuIdOfUser0;/* zero-base: for now, uesr0 use pf mu id 0 */
	uint8_t ucPfMuIdOfUser1;/* zero-base: for now, uesr1 use pf mu id 1 */
	uint8_t ucNumOfTxer;	/* 0~3: mean use 1~4 anntain, for now,
				 *      should fix 3
				 */
	uint8_t ucSpeIndex;	/* add new field to fill"special extension
				 * index" which replace reserve
				 */
	uint32_t u4GroupIndex;	/* 0~ :the index of group table entry for
				 *     calculation
				 */
};

struct MU_SET_CALC_LQ {
	uint8_t ucNumOfUser;	/* zero-base: 0~3: means 1~2 users */
	uint8_t ucBandwidth;	/* zero-base: 0:20 hz 1:40 hz 2: 80 hz 3: 160 */
	uint8_t ucNssOfUser0;	/* zero-base: 0~1 means uesr0 use 1~2 ss,
				 *            if no use keep 0
				 */
	uint8_t ucNssOfUser1;	/* zero-base: 0~1 means uesr0 use 1~2 ss,
				 *            if no use keep 0
				 */
	uint8_t ucPfMuIdOfUser0;/* zero-base: for now, uesr0 use pf mu id 0 */
	uint8_t ucPfMuIdOfUser1;/* zero-base: for now, uesr1 use pf mu id 1 */
	uint8_t ucNumOfTxer;	/* 0~3: mean use 1~4 anntain, for now,
				 *      should fix 3
				 */
	uint8_t ucSpeIndex;	/* add new field to fill"special extension
				 * index" which replace reserve
				 */
	uint32_t u4GroupIndex;	/* 0~ : the index of group table entry for
				 *      calculation
				 */
};

struct MU_GET_LQ {
	uint8_t ucType;
	uint8_t ucRsv[3];
};

struct MU_SET_SNR_OFFSET {
	uint8_t ucVal;
	uint8_t ucRsv[3];
};

struct MU_SET_ZERO_NSS {
	uint8_t ucVal;
	uint8_t ucRsv[3];
};

struct MU_SPEED_UP_LQ {
	uint32_t u4Val;
};

struct MU_SET_MU_TABLE {
	/* UINT_16  u2Type; */
	/* UINT_32  u4Length; */
	uint8_t aucMetricTable[NUM_MUT_NR_NUM * NUM_MUT_FEC *
			       NUM_MUT_MCS * NUM_MUT_INDEX];
};

struct MU_SET_GROUP {
	uint32_t u4GroupIndex;	/* Group Table Idx */
	uint32_t u4NumOfUser;
	uint32_t u4User0Ldpc;
	uint32_t u4User1Ldpc;
	uint32_t u4ShortGI;
	uint32_t u4Bw;
	uint32_t u4User0Nss;
	uint32_t u4User1Nss;
	uint32_t u4GroupId;
	uint32_t u4User0UP;
	uint32_t u4User1UP;
	uint32_t u4User0MuPfId;
	uint32_t u4User1MuPfId;
	uint32_t u4User0InitMCS;
	uint32_t u4User1InitMCS;
	uint8_t aucUser0MacAddr[PARAM_MAC_ADDR_LEN];
	uint8_t aucUser1MacAddr[PARAM_MAC_ADDR_LEN];
};

struct MU_GET_QD {
	uint8_t ucSubcarrierIndex;
	/* UINT_32 u4Length; */
	/* UINT_8 *prQd; */
};

struct MU_SET_ENABLE {
	uint8_t ucVal;
	uint8_t ucRsv[3];
};

struct MU_SET_GID_UP {
	uint32_t au4Gid[2];
	uint32_t au4Up[4];
};

struct MU_TRIGGER_MU_TX {
	uint8_t  fgIsRandomPattern;	/* is random pattern or not */
	uint32_t u4MsduPayloadLength0;	/* payload length of the MSDU for
					 * user 0
					 */
	uint32_t u4MsduPayloadLength1;	/* payload length of the MSDU for
					 * user 1
					 */
	uint32_t u4MuPacketCount;	/* MU TX count */
	uint32_t u4NumOfSTAs;		/* number of user in the MU TX */
	uint8_t   aucMacAddrs[2][6];	/* MAC address of users*/
};

struct PARAM_CUSTOM_MUMIMO_ACTION_STRUCT {
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

struct PARAM_CUSTOM_MEM_DUMP_STRUCT {
	uint32_t u4Address;
	uint32_t u4Length;
	uint32_t u4RemainLength;
#if CFG_SUPPORT_QA_TOOL
	uint32_t u4IcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
	uint8_t ucFragNum;
};

struct PARAM_CUSTOM_SW_CTRL_STRUCT {
	uint32_t u4Id;
	uint32_t u4Data;
};

struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT {
	uint16_t u2Id;
	uint8_t ucType;
	uint8_t ucRespType;
	uint16_t u2MsgSize;
	uint8_t aucReserved0[2];
	uint8_t aucCmd[CHIP_CONFIG_RESP_SIZE];
};

struct PARAM_CUSTOM_KEY_CFG_STRUCT {
	uint8_t aucKey[WLAN_CFG_KEY_LEN_MAX];
	uint8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];
	uint32_t u4Flag;
};

struct EEPROM_RW_INFO {
	uint8_t ucEepromIndex;
	uint8_t reserved;
	uint16_t u2EepromData;
};
struct NVRAM_RW_INFO {
	uint16_t u2NvIndex;
	uint16_t u2NvData;
};

struct PARAM_CUSTOM_EEPROM_RW_STRUCT {
	uint8_t ucMethod;
	union {
		struct EEPROM_RW_INFO rEeprom;
		struct NVRAM_RW_INFO rNvram;
	} info;
};

struct PARAM_CUSTOM_WMM_PS_TEST_STRUCT {
	uint8_t bmfgApsdEnAc;		/* b0~3: trigger-en AC0~3.
					 * b4~7: delivery-en AC0~3
					 */
	uint8_t ucIsEnterPsAtOnce;	/* enter PS immediately without 5 second
					 * guard after connected
					 */
	uint8_t ucIsDisableUcTrigger;	/* not to trigger UC on beacon TIM is
					 * matched (under U-APSD)
					 */
	uint8_t reserved;
	uint8_t ucBssIdx;
};

struct PARAM_CUSTOM_NOA_PARAM_STRUCT {
	uint32_t u4NoaDurationMs;
	uint32_t u4NoaIntervalMs;
	uint32_t u4NoaCount;
	uint8_t ucBssIdx;
};

struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT {
	uint32_t u4CTwindowMs;
	uint8_t ucBssIdx;
};

struct PARAM_CUSTOM_UAPSD_PARAM_STRUCT {
	uint8_t ucBssIdx;
	uint8_t fgEnAPSD;
	uint8_t fgEnAPSD_AcBe;
	uint8_t fgEnAPSD_AcBk;
	uint8_t fgEnAPSD_AcVo;
	uint8_t fgEnAPSD_AcVi;
	uint8_t ucMaxSpLen;
	uint8_t aucResv[2];
};

struct PARAM_CUSTOM_P2P_SET_STRUCT {
	uint32_t u4Enable;
	uint32_t u4Mode;
};

#define MAX_NUMBER_OF_ACL 20

enum ENUM_PARAM_CUSTOM_ACL_POLICY {
	PARAM_CUSTOM_ACL_POLICY_DISABLE,
	PARAM_CUSTOM_ACL_POLICY_ACCEPT,
	PARAM_CUSTOM_ACL_POLICY_DENY,
	PARAM_CUSTOM_ACL_POLICY_NUM
};

struct PARAM_CUSTOM_ACL_ENTRY {
	uint8_t aucAddr[MAC_ADDR_LEN];
	uint16_t u2Rsv;
};

struct PARAM_CUSTOM_ACL {
	enum ENUM_PARAM_CUSTOM_ACL_POLICY ePolicy;
	uint32_t u4Num;
	struct PARAM_CUSTOM_ACL_ENTRY rEntry[MAX_NUMBER_OF_ACL];
};

enum ENUM_CFG_SRC_TYPE {
	CFG_SRC_TYPE_EEPROM,
	CFG_SRC_TYPE_NVRAM,
	CFG_SRC_TYPE_UNKNOWN,
	CFG_SRC_TYPE_NUM
};

enum ENUM_EEPROM_TYPE {
	EEPROM_TYPE_NO,
	EEPROM_TYPE_PRESENT,
	EEPROM_TYPE_NUM
};

enum ENUM_ICAP_STATE {
	ICAP_STATE_INIT = 0,
	ICAP_STATE_START = 1,
	ICAP_STATE_QUERY_STATUS = 2,
	ICAP_STATE_FW_DUMPING = 3,
	ICAP_STATE_FW_DUMP_DONE = 4,
	ICAP_STATE_QA_TOOL_CAPTURE = 5,
	ICAP_STATE_NUM
};


struct PARAM_QOS_TSINFO {
	uint8_t ucTrafficType;	/* Traffic Type: 1 for isochronous 0 for
				 *               asynchronous
				 */
	uint8_t ucTid;		/* TSID: must be between 8 ~ 15 */
	uint8_t ucDirection;	/* direction */
	uint8_t ucAccessPolicy;	/* access policy */
	uint8_t ucAggregation;	/* aggregation */
	uint8_t ucApsd;		/* APSD */
	uint8_t ucuserPriority;	/* user priority */
	uint8_t ucTsInfoAckPolicy;	/* TSINFO ACK policy */
	uint8_t ucSchedule;	/* Schedule */
};

struct PARAM_QOS_TSPEC {
	struct PARAM_QOS_TSINFO rTsInfo;	/* TS info field */
	uint16_t u2NominalMSDUSize;	/* nominal MSDU size */
	uint16_t u2MaxMSDUsize;	/* maximum MSDU size */
	uint32_t u4MinSvcIntv;	/* minimum service interval */
	uint32_t u4MaxSvcIntv;	/* maximum service interval */
	uint32_t u4InactIntv;	/* inactivity interval */
	uint32_t u4SpsIntv;	/* suspension interval */
	uint32_t u4SvcStartTime;	/* service start time */
	uint32_t u4MinDataRate;	/* minimum Data rate */
	uint32_t u4MeanDataRate;	/* mean data rate */
	uint32_t u4PeakDataRate;	/* peak data rate */
	uint32_t u4MaxBurstSize;	/* maximum burst size */
	uint32_t u4DelayBound;	/* delay bound */
	uint32_t u4MinPHYRate;	/* minimum PHY rate */
	uint16_t u2Sba;		/* surplus bandwidth allowance */
	uint16_t u2MediumTime;	/* medium time */
	uint8_t  ucDialogToken;
};

struct PARAM_QOS_ADDTS_REQ_INFO {
	struct PARAM_QOS_TSPEC rTspec;
};

struct PARAM_VOIP_CONFIG {
	uint32_t u4VoipTrafficInterval;	/* 0: disable VOIP configuration */
};

/*802.11 Statistics Struct*/
struct PARAM_802_11_STATISTICS_STRUCT {
	uint8_t ucInvalid;
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
	union LARGE_INTEGER rMdrdyCnt;
	union LARGE_INTEGER rChnlIdleCnt;
	uint32_t u4HwAwakeDuration;
	uint32_t u4RstReason;
	uint64_t u8RstTime;
	uint32_t u4RoamFailCnt;
	uint64_t u8RoamFailTime;
	uint8_t u2TxDoneDelayIsARP;
	uint32_t u4ArriveDrvTick;
	uint32_t u4EnQueTick;
	uint32_t u4DeQueTick;
	uint32_t u4LeaveDrvTick;
	uint32_t u4CurrTick;
	uint64_t u8CurrTime;
};

/* Linux Network Device Statistics Struct */
struct PARAM_LINUX_NETDEV_STATISTICS {
	uint32_t u4RxPackets;
	uint32_t u4TxPackets;
	uint32_t u4RxBytes;
	uint32_t u4TxBytes;
	uint32_t u4RxErrors;
	uint32_t u4TxErrors;
	uint32_t u4Multicast;
};

struct PARAM_MTK_WIFI_TEST_STRUCT {
	uint32_t u4FuncIndex;
	uint32_t u4FuncData;
};

struct _RBIST_IQ_DATA_T {
	int32_t u4IQArray[4][2]; /* IQ_Array[WF][IQ] */
};

struct RECAL_DATA_T {
	uint32_t u4CalId;
	uint32_t u4CalAddr;
	uint32_t u4CalValue;
};

struct RECAL_INFO_T {
	u_int8_t fgDumped;
	uint32_t u4Count;
	struct RECAL_DATA_T *prCalArray;
};

struct ICAP_INFO_T {
	enum ENUM_ICAP_STATE eIcapState;
	uint32_t u4CapNode;

#if CFG_SUPPORT_QA_TOOL
	/* for MT6632/MT7668 file dump mode */
	uint16_t u2DumpIndex;
	uint32_t au4Offset[2][2];
	uint32_t au4IQData[256];

	/* for MT7663/Connad FW parser mode */
	uint32_t u4IQArrayIndex;
	uint32_t u4ICapEventCnt;	/* Count of packet getting from FW */
	uint32_t au4ICapDumpIndex[4][2];/* Count of packet sent to QA Tool,
					 * 4 WF * 2 I/Q
					 */
	struct _RBIST_IQ_DATA_T *prIQArray;
#endif
};

struct RF_TEST_CALIBRATION_T {
	uint32_t	u4FuncData;
	uint8_t	ucDbdcIdx;
	uint8_t	aucReserved[3];
};

struct TX_TONE_PARAM_T {
	uint8_t ucAntIndex;
	uint8_t ucToneType;
	uint8_t ucToneFreq;
	uint8_t ucDbdcIdx;
	int32_t i4DcOffsetI;
	int32_t i4DcOffsetQ;
	uint32_t u4Band;
};

struct CONTINUOUS_TX_PARAM_T {
	uint8_t ucCtrlCh;
	uint8_t ucCentralCh;
	uint8_t ucBW;
	uint8_t ucAntIndex;
	uint16_t u2RateCode;
	uint8_t ucBand;
	uint8_t ucTxfdMode;
};

struct TX_TONE_POWER_GAIN_T {
	uint8_t ucAntIndex;
	uint8_t ucTonePowerGain;
	uint8_t ucBand;
	uint8_t aucReserved[1];
};

struct EXT_CMD_RDD_ON_OFF_CTRL_T {
	uint8_t ucDfsCtrl;
	uint8_t ucRddIdx;
	uint8_t ucRddRxSel;
	uint8_t ucSetVal;
	uint8_t aucReserved[4];
};

struct SET_ADC_T {
	uint32_t  u4ChannelFreq;
	uint8_t	ucAntIndex;
	uint8_t	ucBW;
	uint8_t   ucSX;
	uint8_t	ucDbdcIdx;
	uint8_t	ucRunType;
	uint8_t	ucFType;
	uint8_t	aucReserved[2];		/* Reserving For future */
};

struct SET_RX_GAIN_T {
	uint8_t	ucLPFG;
	uint8_t   ucLNA;
	uint8_t	ucDbdcIdx;
	uint8_t	ucAntIndex;
};

struct SET_TTG_T {
	uint32_t  u4ChannelFreq;
	uint32_t  u4ToneFreq;
	uint8_t	ucTTGPwrIdx;
	uint8_t	ucDbdcIdx;
	uint8_t	ucXtalFreq;
	uint8_t	aucReserved[1];
};

struct TTG_ON_OFF_T {
	uint8_t	ucTTGEnable;
	uint8_t	ucDbdcIdx;
	uint8_t	ucAntIndex;
	uint8_t	aucReserved[1];
};

struct RBIST_CAP_START_T {
	uint32_t u4Trigger;
	uint32_t u4RingCapEn;
	uint32_t u4TriggerEvent;
	uint32_t u4CaptureNode;
	uint32_t u4CaptureLen;    /* Unit : IQ Sample */
	uint32_t u4CapStopCycle;  /* Unit : IQ Sample */
	uint32_t u4MacTriggerEvent;
	uint32_t u4SourceAddressLSB;
	uint32_t u4SourceAddressMSB;
	uint32_t u4BandIdx;
	uint32_t u4BW;
	uint32_t u4EnBitWidth;/* 0:32bit, 1:96bit, 2:128bit */
	uint32_t u4Architech;/* 0:on-chip, 1:on-the-fly */
	uint32_t u4PhyIdx;
	uint32_t u4EmiStartAddress;
	uint32_t u4EmiEndAddress;
	uint32_t u4EmiMsbAddress;
	uint32_t u4CapSource;
	uint32_t u4Reserved[2];
};

struct RBIST_DUMP_IQ_T {
	uint32_t u4WfNum;
	uint32_t u4IQType;
	uint32_t u4IcapCnt; /*IQ Sample Count*/
	uint32_t u4IcapDataLen;
	uint8_t *pucIcapData;
};


struct RBIST_DUMP_RAW_DATA_T {
	uint32_t u4Address;
	uint32_t u4AddrOffset;
	uint32_t u4Bank;
	uint32_t u4BankSize;/* Uint:Kbytes */
	uint32_t u4Reserved[8];
};

/* FuncIndex */
enum FUNC_IDX {
	RE_CALIBRATION = 0x01,
	CALIBRATION_BYPASS = 0x02,
	TX_TONE_START = 0x03,
	TX_TONE_STOP = 0x04,
	CONTINUOUS_TX_START = 0x05,
	CONTINUOUS_TX_STOP = 0x06,
	RF_AT_EXT_FUNCID_TX_TONE_RF_GAIN = 0x07,
	RF_AT_EXT_FUNCID_TX_TONE_DIGITAL_GAIN = 0x08,
	CAL_RESULT_DUMP_FLAG = 0x09,
	RDD_TEST_MODE  = 0x0A,
	SET_ICAP_CAPTURE_START = 0x0B,
	GET_ICAP_CAPTURE_STATUS = 0x0C,
	SET_ADC = 0x0D,
	SET_RX_GAIN = 0x0E,
	SET_TTG = 0x0F,
	TTG_ON_OFF = 0x10,
	GET_ICAP_RAW_DATA = 0x11
};

struct PARAM_MTK_WIFI_TEST_STRUCT_EXT_T {
	uint32_t u4FuncIndex;
	union {
		uint32_t u4FuncData;
		uint32_t u4CalDump;
		struct RF_TEST_CALIBRATION_T rCalParam;
		struct TX_TONE_PARAM_T rTxToneParam;
		struct CONTINUOUS_TX_PARAM_T rConTxParam;
		struct TX_TONE_POWER_GAIN_T rTxToneGainParam;
		struct RBIST_CAP_START_T rICapInfo;
		struct RBIST_DUMP_RAW_DATA_T rICapDump;
		struct EXT_CMD_RDD_ON_OFF_CTRL_T rRDDParam;
		struct SET_ADC_T rSetADC;
		struct SET_RX_GAIN_T rSetRxGain;
		struct SET_TTG_T rSetTTG;
		struct TTG_ON_OFF_T rTTGOnOff;
	} Data;
};

/* 802.11 Media stream constraints */
enum ENUM_MEDIA_STREAM_MODE {
	ENUM_MEDIA_STREAM_OFF,
	ENUM_MEDIA_STREAM_ON
};

/* for NDIS 5.1 Media Streaming Change */
struct PARAM_MEDIA_STREAMING_INDICATION {
	struct PARAM_STATUS_INDICATION rStatus;
	enum ENUM_MEDIA_STREAM_MODE eMediaStreamMode;
};

#define PARAM_PROTOCOL_ID_DEFAULT       0x00
#define PARAM_PROTOCOL_ID_TCP_IP        0x02
#define PARAM_PROTOCOL_ID_IPX           0x06
#define PARAM_PROTOCOL_ID_NBF           0x07
#define PARAM_PROTOCOL_ID_MAX           0x0F
#define PARAM_PROTOCOL_ID_MASK          0x0F

/* for NDIS OID_GEN_NETWORK_LAYER_ADDRESSES */
struct PARAM_NETWORK_ADDRESS_IP {
	uint16_t sin_port;
	uint32_t in_addr;
	uint8_t sin_zero[8];
};

struct PARAM_NETWORK_ADDRESS {
	uint16_t u2AddressLength;/* length in bytes of Address[] in this */
	uint16_t u2AddressType;	/* type of this address
				 * (PARAM_PROTOCOL_ID_XXX above)
				 */
	uint8_t aucAddress[1];	/* actually AddressLength bytes long */
};

/* The following is used with OID_GEN_NETWORK_LAYER_ADDRESSES to set network
 * layer addresses on an interface
 */

struct PARAM_NETWORK_ADDRESS_LIST {
	uint8_t ucBssIdx;
	uint32_t u4AddressCount;/* number of addresses following */
	uint16_t u2AddressType;	/* type of this address
				 * (NDIS_PROTOCOL_ID_XXX above)
				 */
	struct PARAM_NETWORK_ADDRESS
		arAddress[1];	/* actually AddressCount elements long */
};

#if CFG_SLT_SUPPORT

#define FIXED_BW_LG20       0x0000
#define FIXED_BW_UL20       0x2000
#define FIXED_BW_DL40       0x3000

#define FIXED_EXT_CHNL_U20  0x4000	/* For AGG register. */
#define FIXED_EXT_CHNL_L20  0xC000	/* For AGG regsiter. */

enum ENUM_MTK_LP_TEST_MODE {
	ENUM_MTK_LP_TEST_NORMAL,
	ENUM_MTK_LP_TEST_GOLDEN_SAMPLE,
	ENUM_MTK_LP_TEST_DUT,
	ENUM_MTK_LP_TEST_MODE_NUM
};

enum ENUM_MTK_SLT_FUNC_IDX {
	ENUM_MTK_SLT_FUNC_DO_NOTHING,
	ENUM_MTK_SLT_FUNC_INITIAL,
	ENUM_MTK_SLT_FUNC_RATE_SET,
	ENUM_MTK_SLT_FUNC_LP_SET,
	ENUM_MTK_SLT_FUNC_NUM
};

struct PARAM_MTK_SLT_LP_TEST_STRUCT {
	enum ENUM_MTK_LP_TEST_MODE rLpTestMode;
	uint32_t u4BcnRcvNum;
};

struct PARAM_MTK_SLT_TR_TEST_STRUCT {
	enum ENUM_PARAM_NETWORK_TYPE
	rNetworkType;	/* Network Type OFDM5G or OFDM2.4G */
	uint32_t u4FixedRate;	/* Fixed Rate including BW */
};

struct PARAM_MTK_SLT_INITIAL_STRUCT {
	uint8_t aucTargetMacAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2SiteID;
};

struct PARAM_MTK_SLT_TEST_STRUCT {
	enum ENUM_MTK_SLT_FUNC_IDX rSltFuncIdx;
	uint32_t u4Length;	/* Length of structure, */
	/* including myself */
	uint32_t u4FuncInfoLen;	/* Include following content */
	/* field and myself */
	union {
		struct PARAM_MTK_SLT_INITIAL_STRUCT rMtkInitTest;
		struct PARAM_MTK_SLT_LP_TEST_STRUCT rMtkLpTest;
		struct PARAM_MTK_SLT_TR_TEST_STRUCT rMtkTRTest;
	} unFuncInfoContent;

};

#endif

#if CFG_SUPPORT_MSP
/* Should by chip */
struct PARAM_SEC_CONFIG {
	u_int8_t fgWPIFlag;
	u_int8_t fgRV;
	u_int8_t fgIKV;
	u_int8_t fgRKV;

	u_int8_t fgRCID;
	u_int8_t fgRCA1;
	u_int8_t fgRCA2;
	u_int8_t fgEvenPN;

	uint8_t ucKeyID;
	uint8_t ucMUARIdx;
	uint8_t ucCipherSuit;
	uint8_t aucReserved[1];
};

struct PARAM_TX_CONFIG {
	uint8_t aucPA[6];
	u_int8_t fgSW;
	u_int8_t fgDisRxHdrTran;

	u_int8_t fgAADOM;
	uint8_t ucPFMUIdx;
	uint16_t u2PartialAID;

	u_int8_t fgTIBF;
	u_int8_t fgTEBF;
	u_int8_t fgIsHT;
	u_int8_t fgIsVHT;

	u_int8_t fgMesh;
	u_int8_t fgBAFEn;
	u_int8_t fgCFAck;
	u_int8_t fgRdgBA;

	u_int8_t fgRDG;
	u_int8_t fgIsPwrMgt;
	u_int8_t fgRTS;
	u_int8_t fgSMPS;

	u_int8_t fgTxopPS;
	u_int8_t fgDonotUpdateIPSM;
	u_int8_t fgSkipTx;
	u_int8_t fgLDPC;

	u_int8_t fgIsQoS;
	u_int8_t fgIsFromDS;
	u_int8_t fgIsToDS;
	u_int8_t fgDynBw;

	u_int8_t fgIsAMSDUCrossLG;
	u_int8_t fgCheckPER;
	u_int8_t fgIsGID63;
	u_int8_t fgIsHE;

	u_int8_t fgVhtTIBF;
	u_int8_t fgVhtTEBF;
	u_int8_t fgVhtLDPC;
	u_int8_t fgHeLDPC;
};

struct PARAM_KEY_CONFIG {
	uint8_t                 aucKey[32];
};

struct PARAM_PEER_RATE_INFO {
	uint8_t                 ucCounterMPDUFail;
	uint8_t                 ucCounterMPDUTx;
	uint8_t                 ucRateIdx;
	uint8_t                 ucReserved[1];

	uint16_t                au2RateCode[AUTO_RATE_NUM];
};

struct PARAM_PEER_BA_CONFIG {
	uint8_t			ucBaEn;
	uint8_t			ucRsv[3];
	uint32_t			u4BaWinSize;
};

struct PARAM_ANT_ID_CONFIG {
	uint8_t			ucANTIDSts0;
	uint8_t			ucANTIDSts1;
	uint8_t			ucANTIDSts2;
	uint8_t			ucANTIDSts3;
};

struct PARAM_PEER_CAP {
	struct PARAM_ANT_ID_CONFIG	rAntIDConfig;

	uint8_t			ucTxPowerOffset;
	uint8_t			ucCounterBWSelector;
	uint8_t			ucChangeBWAfterRateN;
	uint8_t			ucFrequencyCapability;
	uint8_t			ucSpatialExtensionIndex;

	u_int8_t			fgG2;
	u_int8_t			fgG4;
	u_int8_t			fgG8;
	u_int8_t			fgG16;

	uint8_t			ucMMSS;
	uint8_t			ucAmpduFactor;
	uint8_t			ucReserved[1];
};

struct PARAM_PEER_RX_COUNTER_ALL {
	uint8_t			ucRxRcpi0;
	uint8_t			ucRxRcpi1;
	uint8_t			ucRxRcpi2;
	uint8_t			ucRxRcpi3;

	uint8_t			ucRxCC0;
	uint8_t			ucRxCC1;
	uint8_t			ucRxCC2;
	uint8_t			ucRxCC3;

	u_int8_t			fgRxCCSel;
	uint8_t			ucCeRmsd;
	uint8_t			aucReserved[2];
};

struct PARAM_PEER_TX_COUNTER_ALL {
	uint16_t u2Rate1TxCnt;
	uint16_t u2Rate1FailCnt;
	uint16_t u2Rate2OkCnt;
	uint16_t u2Rate3OkCnt;
	uint16_t u2CurBwTxCnt;
	uint16_t u2CurBwFailCnt;
	uint16_t u2OtherBwTxCnt;
	uint16_t u2OtherBwFailCnt;
};

struct PARAM_HW_WLAN_INFO {
	uint32_t			u4Index;
	struct PARAM_TX_CONFIG	rWtblTxConfig;
	struct PARAM_SEC_CONFIG	rWtblSecConfig;
	struct PARAM_KEY_CONFIG	rWtblKeyConfig;
	struct PARAM_PEER_RATE_INFO	rWtblRateInfo;
	struct PARAM_PEER_BA_CONFIG  rWtblBaConfig;
	struct PARAM_PEER_CAP	rWtblPeerCap;
	struct PARAM_PEER_RX_COUNTER_ALL rWtblRxCounter;
	struct PARAM_PEER_TX_COUNTER_ALL rWtblTxCounter;
};

struct HW_TX_AMPDU_METRICS {
	uint32_t u4TxSfCnt;
	uint32_t u4TxAckSfCnt;
	uint32_t u2TxAmpduCnt;
	uint32_t u2TxRspBaCnt;
	uint16_t u2TxEarlyStopCnt;
	uint16_t u2TxRange1AmpduCnt;
	uint16_t u2TxRange2AmpduCnt;
	uint16_t u2TxRange3AmpduCnt;
	uint16_t u2TxRange4AmpduCnt;
	uint16_t u2TxRange5AmpduCnt;
	uint16_t u2TxRange6AmpduCnt;
	uint16_t u2TxRange7AmpduCnt;
	uint16_t u2TxRange8AmpduCnt;
	uint16_t u2TxRange9AmpduCnt;
#if (CFG_SUPPORT_802_11AX == 1)
	uint16_t u2TxRange10AmpduCnt;
	uint16_t u2TxRange11AmpduCnt;
	uint16_t u2TxRange12AmpduCnt;
	uint16_t u2TxRange13AmpduCnt;
	uint16_t u2TxRange14AmpduCnt;
	uint16_t u2TxRange15AmpduCnt;
	uint16_t u2TxRange16AmpduCnt;
#endif
};

struct HW_MIB_COUNTER {
	uint32_t u4RxFcsErrCnt;
	uint32_t u4RxFifoFullCnt;
	uint32_t u4RxMpduCnt;
	uint32_t u4RxAMPDUCnt;
	uint32_t u4RxTotalByte;
	uint32_t u4RxValidAMPDUSF;
	uint32_t u4RxValidByte;
	uint32_t u4ChannelIdleCnt;
	uint32_t u4RxVectorDropCnt;
	uint32_t u4DelimiterFailedCnt;
	uint32_t u4RxVectorMismatchCnt;
	uint32_t u4MdrdyCnt;
	uint32_t u4CCKMdrdyCnt;
	uint32_t u4OFDMLGMixMdrdy;
	uint32_t u4OFDMGreenMdrdy;
	uint32_t u4PFDropCnt;
	uint32_t u4RxLenMismatchCnt;
	uint32_t u4PCcaTime;
	uint32_t u4SCcaTime;
	uint32_t u4CcaNavTx;
	uint32_t u4PEDTime;
	uint32_t u4BeaconTxCnt;
	uint32_t au4BaMissedCnt[BSSID_NUM];
	uint32_t au4RtsTxCnt[BSSID_NUM];
	uint32_t au4FrameRetryCnt[BSSID_NUM];
	uint32_t au4FrameRetry2Cnt[BSSID_NUM];
	uint32_t au4RtsRetryCnt[BSSID_NUM];
	uint32_t au4AckFailedCnt[BSSID_NUM];
};

struct HW_MIB2_COUNTER {
	uint32_t u4Tx40MHzCnt;
	uint32_t u4Tx80MHzCnt;
	uint32_t u4Tx160MHzCnt;
};

struct PARAM_HW_MIB_INFO {
	uint32_t			u4Index;
	struct HW_MIB_COUNTER	rHwMibCnt;
	struct HW_MIB2_COUNTER	rHwMib2Cnt;
	struct HW_TX_AMPDU_METRICS	rHwTxAmpduMts;
};
#endif


/*--------------------------------------------------------------*/
/*! \brief For Fixed Rate Configuration (Registry)              */
/*--------------------------------------------------------------*/
enum ENUM_REGISTRY_FIXED_RATE {
	FIXED_RATE_NONE,
	FIXED_RATE_1M,
	FIXED_RATE_2M,
	FIXED_RATE_5_5M,
	FIXED_RATE_11M,
	FIXED_RATE_6M,
	FIXED_RATE_9M,
	FIXED_RATE_12M,
	FIXED_RATE_18M,
	FIXED_RATE_24M,
	FIXED_RATE_36M,
	FIXED_RATE_48M,
	FIXED_RATE_54M,
	FIXED_RATE_MCS0_20M_800NS,
	FIXED_RATE_MCS1_20M_800NS,
	FIXED_RATE_MCS2_20M_800NS,
	FIXED_RATE_MCS3_20M_800NS,
	FIXED_RATE_MCS4_20M_800NS,
	FIXED_RATE_MCS5_20M_800NS,
	FIXED_RATE_MCS6_20M_800NS,
	FIXED_RATE_MCS7_20M_800NS,
	FIXED_RATE_MCS0_20M_400NS,
	FIXED_RATE_MCS1_20M_400NS,
	FIXED_RATE_MCS2_20M_400NS,
	FIXED_RATE_MCS3_20M_400NS,
	FIXED_RATE_MCS4_20M_400NS,
	FIXED_RATE_MCS5_20M_400NS,
	FIXED_RATE_MCS6_20M_400NS,
	FIXED_RATE_MCS7_20M_400NS,
	FIXED_RATE_MCS0_40M_800NS,
	FIXED_RATE_MCS1_40M_800NS,
	FIXED_RATE_MCS2_40M_800NS,
	FIXED_RATE_MCS3_40M_800NS,
	FIXED_RATE_MCS4_40M_800NS,
	FIXED_RATE_MCS5_40M_800NS,
	FIXED_RATE_MCS6_40M_800NS,
	FIXED_RATE_MCS7_40M_800NS,
	FIXED_RATE_MCS32_800NS,
	FIXED_RATE_MCS0_40M_400NS,
	FIXED_RATE_MCS1_40M_400NS,
	FIXED_RATE_MCS2_40M_400NS,
	FIXED_RATE_MCS3_40M_400NS,
	FIXED_RATE_MCS4_40M_400NS,
	FIXED_RATE_MCS5_40M_400NS,
	FIXED_RATE_MCS6_40M_400NS,
	FIXED_RATE_MCS7_40M_400NS,
	FIXED_RATE_MCS32_400NS,
	FIXED_RATE_NUM
};

enum ENUM_BT_CMD {
	BT_CMD_PROFILE = 0,
	BT_CMD_UPDATE,
	BT_CMD_NUM
};

enum ENUM_BT_PROFILE {
	BT_PROFILE_CUSTOM = 0,
	BT_PROFILE_SCO,
	BT_PROFILE_ACL,
	BT_PROFILE_MIXED,
	BT_PROFILE_NO_CONNECTION,
	BT_PROFILE_NUM
};

struct PTA_PROFILE {
	enum ENUM_BT_PROFILE eBtProfile;
	union {
		uint8_t aucBTPParams[BT_PROFILE_PARAM_LEN];
		/*  0: sco reserved slot time,
		 *  1: sco idle slot time,
		 *  2: acl throughput,
		 *  3: bt tx power,
		 *  4: bt rssi
		 *  5: VoIP interval
		 *  6: BIT(0) Use this field, BIT(1) 0 apply single/ 1 dual PTA
		 *     setting.
		 */
		uint32_t au4Btcr[4];
	} u;
};

struct PTA_IPC {
	uint8_t ucCmd;
	uint8_t ucLen;
	union {
		struct PTA_PROFILE rProfile;
		uint8_t aucBTPParams[BT_PROFILE_PARAM_LEN];
	} u;
};

/*--------------------------------------------------------------*/
/*! \brief CFG80211 Scan Request Container                      */
/*--------------------------------------------------------------*/
struct PARAM_SCAN_REQUEST_EXT {
	struct PARAM_SSID rSsid;
	uint32_t u4IELength;
	uint8_t *pucIE;
	uint8_t ucBssIndex;
};

struct PARAM_SCAN_REQUEST_ADV {
	uint32_t u4SsidNum;
	struct PARAM_SSID rSsid[CFG_SCAN_SSID_MAX_NUM];
	uint8_t ucScanType;
	uint32_t u4IELength;
	uint8_t *pucIE;
	uint32_t u4ChannelNum;
	struct RF_CHANNEL_INFO
		arChannel[MAXIMUM_OPERATION_CHANNEL_LIST];
	uint8_t ucScnFuncMask;
	uint8_t aucRandomMac[MAC_ADDR_LEN];
	uint8_t ucBssIndex;
	uint32_t u4Flags;
};

/*--------------------------------------------------------------*/
/*! \brief CFG80211 Scheduled Scan Request Container            */
/*--------------------------------------------------------------*/
#if CFG_SUPPORT_SCHED_SCAN
struct PARAM_SCHED_SCAN_REQUEST {
	uint32_t u4SsidNum;         /* passed in the probe_reqs */
	struct PARAM_SSID arSsid[CFG_SCAN_HIDDEN_SSID_MAX_NUM];
	uint32_t u4MatchSsidNum;   /* matched for a scan request */
	struct PARAM_SSID arMatchSsid[CFG_SCAN_SSID_MATCH_MAX_NUM];
	int32_t ai4RssiThold[CFG_SCAN_SSID_MATCH_MAX_NUM];
	int32_t i4MinRssiThold;
	uint8_t ucScnFuncMask;
	uint8_t aucRandomMac[MAC_ADDR_LEN];
	uint8_t aucRandomMacMask[MAC_ADDR_LEN];
	uint32_t u4IELength;
	uint8_t *pucIE;
	uint16_t u2ScanInterval;	/* in second */
	uint8_t ucChnlNum;
	uint8_t *pucChannels;
	uint8_t ucBssIndex;
};
#endif /* CFG_SUPPORT_SCHED_SCAN */

#if CFG_SUPPORT_PASSPOINT
struct PARAM_HS20_SET_BSSID_POOL {
	u_int8_t fgIsEnable;
	uint8_t ucNumBssidPool;
	uint8_t arBSSID[8][PARAM_MAC_ADDR_LEN];
	uint8_t ucBssIndex;
};

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_SNIFFER
struct PARAM_CUSTOM_MONITOR_SET_STRUCT {
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

/*--------------------------------------------------------------*/
/*! \brief MTK Auto Channel Selection related Container         */
/*--------------------------------------------------------------*/
enum ENUM_SAFE_CH_MASK {
	ENUM_SAFE_CH_MASK_BAND_2G4 = 0,
	ENUM_SAFE_CH_MASK_BAND_5G_0 = 1,
	ENUM_SAFE_CH_MASK_BAND_5G_1 = 2,
	ENUM_SAFE_CH_MASK_BAND_5G_2 = 3,
	ENUM_SAFE_CH_MASK_MAX_NUM = 4,
};

struct LTE_SAFE_CHN_INFO {
	/* ENUM_SAFE_CH_MASK_MAX_NUM */
	uint32_t au4SafeChannelBitmask[ENUM_SAFE_CH_MASK_MAX_NUM];
};

struct PARAM_CHN_LOAD_INFO {
	/* Per-CHN Load */
	uint8_t ucChannel;
	uint16_t u2APNum;
	uint32_t u4Dirtiness;
	uint8_t ucReserved;
};

struct PARAM_CHN_RANK_INFO {
	uint8_t ucChannel;
	uint32_t u4Dirtiness;
	uint8_t ucReserved;
};

struct PARAM_GET_CHN_INFO {
	uint8_t ucRoleIndex;
	struct LTE_SAFE_CHN_INFO rLteSafeChnList;
	struct PARAM_CHN_LOAD_INFO rEachChnLoad[MAX_CHN_NUM];
	struct PARAM_CHN_RANK_INFO rChnRankList[MAX_CHN_NUM];
	uint8_t aucReserved[3];
};

struct PARAM_PREFER_CHN_INFO {
	uint8_t ucChannel;
	uint8_t aucReserved[3];
	uint32_t u4Dirtiness;
};

struct UMAC_STAT2_GET {
	uint16_t	u2PleRevPgHif0Group0;
	uint16_t	u2PleRevPgCpuGroup2;

	uint16_t	u2PseRevPgHif0Group0;
	uint16_t	u2PseRevPgHif1Group1;
	uint16_t	u2PseRevPgCpuGroup2;
	uint16_t	u2PseRevPgLmac0Group3;
	uint16_t	u2PseRevPgLmac1Group4;
	uint16_t	u2PseRevPgLmac2Group5;
	uint16_t	u2PseRevPgPleGroup6;

	uint16_t	u2PleSrvPgHif0Group0;
	uint16_t	u2PleSrvPgCpuGroup2;

	uint16_t	u2PseSrvPgHif0Group0;
	uint16_t	u2PseSrvPgHif1Group1;
	uint16_t	u2PseSrvPgCpuGroup2;
	uint16_t	u2PseSrvPgLmac0Group3;
	uint16_t	u2PseSrvPgLmac1Group4;
	uint16_t	u2PseSrvPgLmac2Group5;
	uint16_t	u2PseSrvPgPleGroup6;

	uint16_t	u2PleTotalPageNum;
	uint16_t	u2PleFreePageNum;
	uint16_t	u2PleFfaNum;

	uint16_t	u2PseTotalPageNum;
	uint16_t	u2PseFreePageNum;
	uint16_t	u2PseFfaNum;
};

struct CNM_STATUS {
	uint8_t              fgDbDcModeEn;
	uint8_t              ucChNumB0;
	uint8_t              ucChNumB1;
	uint8_t              usReserved;
};

struct CNM_CH_LIST {
	uint8_t              ucChNum[4];
};

struct EXT_CMD_SER_T {
	uint8_t ucAction;
	uint8_t ucSerSet;
	uint8_t ucDbdcIdx;
	uint8_t aucReserve[1];
};

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
struct HAL_FRAME_POWER_SET_T {
	int8_t icFramePowerDbm;
};

struct FRAME_POWER_CONFIG_INFO_T {
	struct HAL_FRAME_POWER_SET_T
		aicFramePowerConfig[TXPOWER_RATE_NUM][ENUM_BAND_NUM];
};

struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T {
	uint8_t ucTxPowerCategory;
	uint8_t ucBandIdx;
	uint8_t ucChBand;
	uint8_t u1Format;/*0:Legacy,1:HE format*/

	/* Rate power info */
	struct FRAME_POWER_CONFIG_INFO_T rRatePowerInfo;

	/* tx Power Max/Min Limit info */
	int8_t icPwrMaxBnd;
	int8_t icPwrMinBnd;
	int8_t ucReserved2;
};
#endif


struct PARAM_TXPOWER_BY_RATE_SET_T {
	uint8_t u1PhyMode;
	uint8_t u1TxRate;
	uint8_t u1BW;
	int8_t  i1TxPower;
};

#if CFG_SUPPORT_OSHARE
/* OSHARE Mode */
#define MAX_OSHARE_MODE_LENGTH		64
#define OSHARE_MODE_MAGIC_CODE		0x18
#define OSHARE_MODE_CMD_V1		0x1

struct OSHARE_MODE_T {
	uint8_t   cmdVersion; /* CMD version = OSHARE_MODE_CMD_V1 */
	uint8_t   cmdType; /* 1-set  0-query */
	uint8_t   magicCode; /* It's like CRC, OSHARE_MODE_MAGIC_CODE */
	uint8_t   cmdBufferLen; /* buffer length <= 64 */
	uint8_t   buffer[MAX_OSHARE_MODE_LENGTH];
};

struct OSHARE_MODE_SETTING_V1_T {
	uint8_t   osharemode; /* 0: disable, 1:Enable */
	uint8_t   reserved[7];
};
#endif

struct PARAM_WIFI_LOG_LEVEL_UI {
	uint32_t u4Version;
	uint32_t u4Module;
	uint32_t u4Enable;
};

struct PARAM_WIFI_LOG_LEVEL {
	uint32_t u4Version;
	uint32_t u4Module;
	uint32_t u4Level;
};

struct PARAM_GET_WIFI_TYPE {
	struct net_device *prNetDev;
	uint8_t arWifiTypeName[8];
};

enum ENUM_WIFI_LOG_LEVEL_VERSION_T {
	ENUM_WIFI_LOG_LEVEL_VERSION_V1 = 1,
	ENUM_WIFI_LOG_LEVEL_VERSION_NUM
};

enum ENUM_WIFI_LOG_LEVEL_T {
	ENUM_WIFI_LOG_LEVEL_DEFAULT = 0,
	ENUM_WIFI_LOG_LEVEL_MORE,
	ENUM_WIFI_LOG_LEVEL_EXTREME,
	ENUM_WIFI_LOG_LEVEL_NUM
};

enum ENUM_WIFI_LOG_MODULE_T {
	ENUM_WIFI_LOG_MODULE_DRIVER = 0,
	ENUM_WIFI_LOG_MODULE_FW,
	ENUM_WIFI_LOG_MODULE_NUM,
};

enum ENUM_WIFI_LOG_LEVEL_SUPPORT_T {
	ENUM_WIFI_LOG_LEVEL_SUPPORT_DISABLE = 0,
	ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE,
	ENUM_WIFI_LOG_LEVEL_SUPPORT_NUM
};

#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
struct PARAM_GET_LINK_QUALITY_INFO {
	uint8_t ucBssIdx;
	struct WIFI_LINK_QUALITY_INFO *prLinkQualityInfo;
};
#endif /* CFG_SUPPORT_LINK_QUALITY_MONITOR */

#if CFG_SUPPORT_MBO
struct PARAM_BSS_DISALLOWED_LIST {
	uint32_t u4NumBssDisallowed;
	/* MAX_FW_ROAMING_BLACKLIST_SIZE */
	uint8_t aucList[MAC_ADDR_LEN * 16];
};
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
/*--------------------------------------------------------------*/
/* Routines to set parameters or query information.             */
/*--------------------------------------------------------------*/
/***** Routines in wlan_oid.c *****/
uint32_t
wlanoidQueryNetworkTypesSupported(IN struct ADAPTER *prAdapter,
				  OUT void *pvQueryBuffer,
				  IN uint32_t u4QueryBufferLen,
				  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryNetworkTypeInUse(IN struct ADAPTER *prAdapter,
			     OUT void *pvQueryBuffer,
			     IN uint32_t u4QueryBufferLen,
			     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNetworkTypeInUse(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryBssid(IN struct ADAPTER *prAdapter,
		  OUT void *pvQueryBuffer,
		  IN uint32_t u4QueryBufferLen,
		  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetBssidListScan(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetBssidListScanExt(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetBssidListScanAdv(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryBssidList(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetBssid(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetConnect(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetSsid(IN struct ADAPTER *prAdapter,
	       IN void *pvSetBuffer,
	       IN uint32_t u4SetBufferLen,
	       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQuerySsid(IN struct ADAPTER *prAdapter,
		 OUT void *pvQueryBuffer,
		 IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryInfrastructureMode(IN struct ADAPTER *prAdapter,
			       OUT void *pvQueryBuffer,
			       IN uint32_t u4QueryBufferLen,
			       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetInfrastructureMode(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryAuthMode(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer,
		     IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetAuthMode(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

#if 0
uint32_t
wlanoidQueryPrivacyFilter(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetPrivacyFilter(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidSetEncryptionStatus(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryEncryptionStatus(IN struct ADAPTER *prAdapter,
			     IN void *pvQueryBuffer,
			     IN uint32_t u4QueryBufferLen,
			     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetAddWep(IN struct ADAPTER *prAdapter,
		 IN void *pvSetBuffer,
		 IN uint32_t u4SetBufferLen,
		 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetRemoveWep(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer,
		    IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetAddKey(IN struct ADAPTER *prAdapter,
		 IN void *pvSetBuffer,
		 IN uint32_t u4SetBufferLen,
		 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetRemoveKey(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer,
		    IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetReloadDefaults(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryCapability(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryFrequency(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetFrequency(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer,
		    IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryAtimWindow(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetAtimWindow(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetChannel(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidRssiMonitor(IN struct ADAPTER *prAdapter,
		   OUT void *pvQueryBuffer,
		   IN uint32_t u4QueryBufferLen,
		   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryRssi(IN struct ADAPTER *prAdapter,
		 OUT void *pvQueryBuffer,
		 IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryRssiTrigger(IN struct ADAPTER *prAdapter,
			OUT void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetRssiTrigger(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryRtsThreshold(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetRtsThreshold(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQuery802dot11PowerSaveProfile(IN struct ADAPTER
				     *prAdapter,
				     IN void *pvQueryBuffer,
				     IN uint32_t u4QueryBufferLen,
				     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSet802dot11PowerSaveProfile(IN struct ADAPTER
				   *prAdapter,
				   IN void *prSetBuffer,
				   IN uint32_t u4SetBufferLen,
				   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetPmkid(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidDelPmkid(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidFlushPmkid(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQuerySupportedRates(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryDesiredRates(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetDesiredRates(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryPermanentAddr(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuf,
			  IN uint32_t u4QueryBufLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryCurrentAddr(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuf,
			IN uint32_t u4QueryBufLen,
			OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryPermanentAddr(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuf,
			  IN uint32_t u4QueryBufLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryMaxLinkSpeed(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryLinkSpeed(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanQueryLinkSpeed(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen,
		       IN uint8_t fgIsOid);

uint32_t
wlanoidQueryLinkSpeedEx(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen);

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
uint32_t wlanoidSetEfusBufferMode(IN struct ADAPTER
				  *prAdapter,
				  IN void *pvSetBuffer,
				  IN uint32_t u4SetBufferLen,
				  OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidConnacSetEfusBufferMode(IN struct ADAPTER
					*prAdapter,
					IN void *pvSetBuffer,
					IN uint32_t u4SetBufferLen,
					OUT uint32_t *pu4SetInfoLen);

/* #if (CFG_EEPROM_PAGE_ACCESS == 1) */
uint32_t
wlanoidQueryProcessAccessEfuseRead(IN struct ADAPTER
				   *prAdapter,
				   IN void *pvSetBuffer,
				   IN uint32_t u4SetBufferLen,
				   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryProcessAccessEfuseWrite(IN struct ADAPTER
				    *prAdapter,
				    IN void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryEfuseFreeBlock(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryGetTxPower(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);
/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */
uint32_t
wlanoidQueryRxStatistics(IN struct ADAPTER *prAdapter,
			 IN void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidBssInfoBasic(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer,
		    IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidDevInfoActive(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidManualAssoc(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_TX_BF
uint32_t
wlanoidTxBfAction(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidMuMimoAction(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidStaRecUpdate(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidStaRecBFUpdate(IN struct ADAPTER *prAdapter,
			       IN void *pvSetBuffer,
			       IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
uint32_t
wlanoidSendCalBackupV2Cmd(IN struct ADAPTER *prAdapter,
			  IN void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen);

uint32_t
wlanoidSetCalBackup(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer,
		    IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryCalBackupV2(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen);
#endif

#if CFG_SUPPORT_SMART_GEAR
uint32_t
wlandioSetSGStatus(IN struct ADAPTER *prAdapter,
			IN uint8_t ucSGEnable,
			IN uint8_t ucSGSpcCmd,
			IN uint8_t ucNSS);
#endif

uint32_t
wlanoidQueryMcrRead(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer,
		    IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryMemDump(IN struct ADAPTER *prAdapter,
		    IN void *pvQueryBuffer,
		    IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetMcrWrite(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryDrvMcrRead(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetDrvMcrWrite(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQuerySwCtrlRead(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetSwCtrlWrite(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetChipConfig(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryChipConfig(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetKeyCfg(IN struct ADAPTER *prAdapter,
		 IN void *pvSetBuffer,
		 IN uint32_t u4SetBufferLen,
		 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryEepromRead(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetEepromWrite(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryRfTestRxStatus(IN struct ADAPTER *prAdapter,
			   IN void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryRfTestTxStatus(IN struct ADAPTER *prAdapter,
			   IN void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryOidInterfaceVersion(IN struct ADAPTER
				*prAdapter,
				IN void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryVendorId(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer,
		     IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryMulticastList(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetMulticastList(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryRcvError(IN struct ADAPTER *prAdapter,
		     IN void *pvQueryBuffer,
		     IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryRcvNoBuffer(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryRcvCrcError(IN struct ADAPTER *prAdapter,
			IN void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryStatistics(IN struct ADAPTER *prAdapter,
		       IN void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryBugReport(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

#ifdef LINUX

uint32_t
wlanoidQueryStatisticsForLinux(IN struct ADAPTER *prAdapter,
			       IN void *pvQueryBuffer,
			       IN uint32_t u4QueryBufferLen,
			       OUT uint32_t *pu4QueryInfoLen);

#endif

uint32_t
wlanoidQueryMediaStreamMode(IN struct ADAPTER *prAdapter,
			    IN void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetMediaStreamMode(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryRcvOk(IN struct ADAPTER *prAdapter,
		  IN void *pvQueryBuffer,
		  IN uint32_t u4QueryBufferLen,
		  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryXmitOk(IN struct ADAPTER *prAdapter,
		   IN void *pvQueryBuffer,
		   IN uint32_t u4QueryBufferLen,
		   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryXmitError(IN struct ADAPTER *prAdapter,
		      IN void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryXmitOneCollision(IN struct ADAPTER *prAdapter,
			     IN void *pvQueryBuffer,
			     IN uint32_t u4QueryBufferLen,
			     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryXmitMoreCollisions(IN struct ADAPTER *prAdapter,
			       IN void *pvQueryBuffer,
			       IN uint32_t u4QueryBufferLen,
			       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryXmitMaxCollisions(IN struct ADAPTER *prAdapter,
			      IN void *pvQueryBuffer,
			      IN uint32_t u4QueryBufferLen,
			      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetCurrentPacketFilter(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer,
			      IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidSetPacketFilter(struct ADAPTER *prAdapter,
				void *pvPacketFiltr,
				u_int8_t fgIsOid,
				void *pvSetBuffer,
				uint32_t u4SetBufferLen);

uint32_t
wlanoidQueryCurrentPacketFilter(IN struct ADAPTER
				*prAdapter,
				IN void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetAcpiDevicePowerState(IN struct ADAPTER *prAdapter,
			       IN void *pvSetBuffer,
			       IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryAcpiDevicePowerState(IN struct ADAPTER
				 *prAdapter,
				 IN void *pvQueryBuffer,
				 IN uint32_t u4QueryBufferLen,
				 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetDisassociate(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryFragThreshold(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetFragThreshold(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryAdHocMode(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetAdHocMode(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer,
		    IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryBeaconInterval(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetBeaconInterval(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetCurrentAddr(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
uint32_t
wlanoidSetCSUMOffload(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

uint32_t
wlanoidSetNetworkAddress(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryMaxFrameSize(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryMaxTotalSize(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetCurrentLookahead(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

/* RF Test related APIs */
uint32_t
wlanoidRftestSetTestMode(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidRftestSetTestIcapMode(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidRftestSetAbortTestMode(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer,
			      IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidRftestQueryAutoTest(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidRftestSetAutoTest(IN struct ADAPTER *prAdapter,
			 OUT void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidExtRfTestICapStart(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidExtRfTestICapStatus(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

void wlanoidRfTestICapRawDataProc(IN struct ADAPTER *prAdapter,
				  uint32_t u4CapStartAddr,
				  uint32_t u4TotalBufferSize);

#if CFG_SUPPORT_WAPI
uint32_t
wlanoidSetWapiMode(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetWapiAssocInfo(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetWapiKey(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);
#endif

#if CFG_ENABLE_WAKEUP_ON_LAN
uint32_t
wlanoidSetAddWakeupPattern(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetRemoveWakeupPattern(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer,
			      IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryEnableWakeup(IN struct ADAPTER *prAdapter,
			 OUT void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *u4QueryInfoLen);

uint32_t
wlanoidSetEnableWakeup(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidSetWiFiWmmPsTest(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetTxAmpdu(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetAddbaReject(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNvramRead(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNvramWrite(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryCfgSrcType(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryEepromType(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetCountryCode(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetScanMacOui(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen);

uint32_t wlanSendMemDumpCmd(IN struct ADAPTER *prAdapter,
			    IN void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen);

#if CFG_SLT_SUPPORT

uint32_t
wlanoidQuerySLTStatus(IN struct ADAPTER *prAdapter,
		      OUT void *pvQueryBuffer,
		      IN uint32_t u4QueryBufferLen,
		      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidUpdateSLTMode(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

#endif

uint32_t
wlanoidQueryWlanInfo(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer,
		     IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanQueryWlanInfo(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer,
		     IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen,
		     IN uint8_t fgIsOid);

uint32_t
wlanoidQueryMibInfo(IN struct ADAPTER *prAdapter,
		    OUT void *pvQueryBuffer,
		    IN uint32_t u4QueryBufferLen,
		    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanQueryMibInfo(IN struct ADAPTER *prAdapter,
		 IN void *pvQueryBuffer,
		 IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen,
		 IN uint8_t fgIsOid);

uint32_t
wlanoidSetFwLog2Host(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

#if 0
uint32_t
wlanoidSetNoaParam(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetOppPsParam(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetUApsdParam(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);
#endif

/*----------------------------------------------------------------------------*/
uint32_t
wlanoidSetBT(IN struct ADAPTER *prAdapter,
	     IN void *pvSetBuffer,
	     IN uint32_t u4SetBufferLen,
	     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryBT(IN struct ADAPTER *prAdapter,
	       OUT void *pvQueryBuffer,
	       IN uint32_t u4QueryBufferLen,
	       OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetTxPower(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);

#if 0
uint32_t
wlanoidQueryBtSingleAntenna(
	IN  struct ADAPTER *prAdapter,
	OUT void *pvQueryBuffer,
	IN  uint32_t u4QueryBufferLen,
	OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetBtSingleAntenna(
	IN  struct ADAPTER *prAdapter,
	IN  void *pvSetBuffer,
	IN  uint32_t u4SetBufferLen,
	OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetPta(
	IN  struct ADAPTER *prAdapter,
	IN  void *pvSetBuffer,
	IN  uint32_t u4SetBufferLen,
	OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryPta(
	IN  struct ADAPTER *prAdapter,
	OUT void *pvQueryBuffer,
	IN  uint32_t u4QueryBufferLen,
	OUT uint32_t *pu4QueryInfoLen);
#endif

#if CFG_ENABLE_WIFI_DIRECT
uint32_t
wlanoidSetP2pMode(IN struct ADAPTER *prAdapter,
		  IN void *pvSetBuffer,
		  IN uint32_t u4SetBufferLen,
		  OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidSetDefaultKey(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetGtkRekeyData(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_SCHED_SCAN
uint32_t
wlanoidSetStartSchedScan(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetStopSchedScan(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_SUPPORT_SCHED_SCAN */

#if CFG_M0VE_BA_TO_DRIVER
uint32_t wlanoidResetBAScoreboard(IN struct ADAPTER *prAdapter,
				  IN void *pvSetBuffer,
				  IN uint32_t u4SetBufferLen);
#endif

#if CFG_SUPPORT_BATCH_SCAN
uint32_t
wlanoidSetBatchScanReq(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryBatchScanResult(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);
#endif

#if CFG_SUPPORT_PASSPOINT
uint32_t
wlanoidSetHS20Info(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetHS20BssidPool(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_SNIFFER
uint32_t wlanoidSetMonitor(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidNotifyFwSuspend(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryCnm(
	IN struct ADAPTER *prAdapter,
	IN void *pvQueryBuffer,
	IN uint32_t u4QueryBufferLen,
	OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidPacketKeepAlive(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_DBDC
uint32_t
wlanoidSetDbdcEnable(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);
#endif /*#if CFG_SUPPORT_DBDC*/

uint32_t
wlanoidQuerySetTxTargetPower(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

#if (CFG_SUPPORT_DFS_MASTER == 1)
uint32_t
wlanoidQuerySetRddReport(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQuerySetRadarDetectMode(IN struct ADAPTER *prAdapter,
			       IN void *pvSetBuffer,
			       IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidLinkDown(IN struct ADAPTER *prAdapter,
		IN void *pvSetBuffer,
		IN uint32_t u4SetBufferLen,
		OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidDisableTdlsPs(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidSetSer(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidSerExtCmd(IN struct ADAPTER *prAdapter,
			  uint8_t ucAction,
			  uint8_t ucSerSet,
			  uint8_t ucDbdcIdx);

#if CFG_SUPPORT_NCHO
#define NCHO_CMD_MAX_LENGTH	128

uint32_t
wlanoidSetNchoRoamTrigger(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamTrigger(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamDelta(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamDelta(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamScnPeriod(IN struct ADAPTER *prAdapter,
			    IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamScnPeriod(IN struct ADAPTER *prAdapter,
			      OUT void *pvQueryBuffer,
			      IN uint32_t u4QueryBufferLen,
			      OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamScnChnl(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidAddNchoRoamScnChnl(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamScnChnl(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamScnCtrl(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamScnCtrl(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnChnlTime(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnChnlTime(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnHomeTime(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnHomeTime(IN struct ADAPTER *prAdapter,
			    OUT void *pvQueryBuffer,
			    IN uint32_t u4QueryBufferLen,
			    OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnHomeAwayTime(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer,
			      IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnHomeAwayTime(IN struct ADAPTER
				*prAdapter,
				OUT void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnNprobes(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnNprobes(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidGetNchoReassocInfo(IN struct ADAPTER *prAdapter,
			  OUT void *pvQueryBuffer,
			  IN uint32_t u4QueryBufferLen,
			  OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSendNchoActionFrameStart(IN struct ADAPTER *prAdapter,
				IN void *pvSetBuffer,
				IN uint32_t u4SetBufferLen,
				OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSendNchoActionFrameEnd(IN struct ADAPTER *prAdapter,
			      IN void *pvSetBuffer,
			      IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetNchoWesMode(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer,
		      IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoWesMode(IN struct ADAPTER *prAdapter,
			OUT void *pvQueryBuffer,
			IN uint32_t u4QueryBufferLen,
			OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoBand(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer,
		   IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoBand(IN struct ADAPTER *prAdapter,
		     OUT void *pvQueryBuffer,
		     IN uint32_t u4QueryBufferLen,
		     OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoDfsScnMode(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoDfsScnMode(IN struct ADAPTER *prAdapter,
			   OUT void *pvQueryBuffer,
			   IN uint32_t u4QueryBufferLen,
			   OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoEnable(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoEnable(IN struct ADAPTER *prAdapter,
		       OUT void *pvQueryBuffer,
		       IN uint32_t u4QueryBufferLen,
		       OUT uint32_t *pu4QueryInfoLen);

#endif /* CFG_SUPPORT_NCHO */

uint32_t
wlanoidAddRoamScnChnl(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);

uint32_t
wlanoidAbortScan(IN struct ADAPTER *prAdapter,
		 OUT void *pvQueryBuffer,
		 IN uint32_t u4QueryBufferLen,
		 OUT uint32_t *pu4QueryInfoLen);

uint32_t wlanoidSetDrvSer(IN struct ADAPTER *prAdapter,
			  IN void *pvSetBuffer,
			  IN uint32_t u4SetBufferLen,
			  OUT uint32_t *pu4SetInfoLen);
uint32_t wlanoidSetAmsduNum(IN struct ADAPTER *prAdapter,
			    IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen);
uint32_t wlanoidSetAmsduSize(IN struct ADAPTER *prAdapter,
			     IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

/* Show Consys debug information*/
uint32_t
wlanoidShowPdmaInfo(IN struct ADAPTER *prAdapter,
		    IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		    OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidShowPseInfo(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidShowPleInfo(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidShowCsrInfo(IN struct ADAPTER *prAdapter,
		   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		   OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidShowDmaschInfo(IN struct ADAPTER *prAdapter,
		      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		      OUT uint32_t *pu4SetInfoLen);
/* end Show Consys debug information*/

uint32_t
wlanoidSetTxPowerByRateManual(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
uint32_t
wlanoidQueryTxPowerInfo(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);
#endif

#if CFG_SUPPORT_MBO
uint32_t wlanoidBssDisallowedList(IN struct ADAPTER
				    *prAdapter,
				    IN void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen);

#endif

uint32_t wlanoidSetDrvRoamingPolicy(IN struct ADAPTER
				    *prAdapter,
				    IN void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_OSHARE
uint32_t
wlanoidSetOshareMode(IN struct ADAPTER *prAdapter,
		     IN void *pvSetBuffer,
		     IN uint32_t u4SetBufferLen,
		     OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidQueryWifiLogLevelSupport(IN struct ADAPTER *prAdapter,
				IN void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryWifiLogLevel(IN struct ADAPTER *prAdapter,
			 IN void *pvQueryBuffer,
			 IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetWifiLogLevel(IN struct ADAPTER *prAdapter,
		       IN void *pvSetBuffer,
		       IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_LOWLATENCY_MODE
uint32_t
wlanoidSetLowLatencyMode(IN struct ADAPTER *prAdapter,
			 IN void *pvSetBuffer,
			 IN uint32_t u4SetBufferLen,
			 OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#if CFG_SUPPORT_ANT_SWAP
uint32_t wlanoidQueryAntennaSwap(IN struct ADAPTER *prAdapter,
				OUT void *pvQueryBuffer,
				IN uint32_t u4QueryBufferLen,
				OUT uint32_t *pu4QueryInfoLen);
#endif


#if CFG_SUPPORT_EASY_DEBUG
uint32_t wlanoidSetFwParam(IN struct ADAPTER *prAdapter,
			   IN void *pvSetBuffer,
			   IN uint32_t u4SetBufferLen,
			   OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_SUPPORT_EASY_DEBUG */

uint32_t wlanoidUpdateFtIes(IN struct ADAPTER *prAdapter, IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidSync11kCapabilities(IN struct ADAPTER *prAdapter,
				    IN void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidSendNeighborRequest(IN struct ADAPTER *prAdapter,
				    IN void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidSendBTMQuery(IN struct ADAPTER *prAdapter, IN void *pvSetBuffer,
			     IN uint32_t u4SetBufferLen,
			     OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidPktProcessIT(struct ADAPTER *prAdapter, void *pvBuffer,
			     uint32_t u4BufferLen, uint32_t *pu4InfoLen);

uint32_t wlanoidFwEventIT(struct ADAPTER *prAdapter, void *pvBuffer,
			  uint32_t u4BufferLen, uint32_t *pu4InfoLen);

uint32_t wlanoidTspecOperation(IN struct ADAPTER *prAdapter, IN void *pvBuffer,
			       IN uint32_t u4BufferLen,
			       OUT uint32_t *pu4InfoLen);

uint32_t wlanoidDumpUapsdSetting(struct ADAPTER *prAdapter, void *pvBuffer,
				 uint32_t u4BufferLen, uint32_t *pu4InfoLen);

uint32_t wlanoidGetWifiType(IN struct ADAPTER *prAdapter,
			    IN void *pvSetBuffer,
			    IN uint32_t u4SetBufferLen,
			    OUT uint32_t *pu4SetInfoLen);

uint32_t wlanoidRfTestICapGetIQData(IN struct ADAPTER *prAdapter,
				    OUT void *pvSetBuffer,
				    IN uint32_t u4SetBufferLen,
				    OUT uint32_t *pu4SetInfoLen);


#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
uint32_t wlanoidGetLinkQualityInfo(IN struct ADAPTER *prAdapter,
				   IN void *pvSetBuffer,
				   IN uint32_t u4SetBufferLen,
				   OUT uint32_t *pu4SetInfoLen);
#endif /* CFG_SUPPORT_LINK_QUALITY_MONITOR */

#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
/* dynamic tx power control */
uint32_t wlanoidTxPowerControl(IN struct ADAPTER *prAdapter,
			       IN void *pvSetBuffer,
			       IN uint32_t u4SetBufferLen,
			       OUT uint32_t *pu4SetInfoLen);
#endif

uint32_t
wlanoidExternalAuthDone(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer,
			IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);
uint32_t
wlanoidIndicateBssInfo(IN struct ADAPTER *prAdapter,
			IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			OUT uint32_t *pu4SetInfoLen);

#endif /* _WLAN_OID_H */
