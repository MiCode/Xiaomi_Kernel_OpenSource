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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/wlan_oid.h#4
*/

/*! \file   "wlan_oid.h"
*    \brief This file contains the declairation file of the WLAN OID processing routines
*	   of Windows driver for MediaTek Inc. 802.11 Wireless LAN Adapters.
*/


#ifndef _WLAN_OID_H
#define _WLAN_OID_H

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

#define PARAM_EEPROM_READ_METHOD_READ           1
#define PARAM_EEPROM_READ_METHOD_GETSIZE        0

#define PARAM_WHQL_RSSI_MAX_DBM                 (-10)
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
#define PARAM_PACKET_FILTER_P2P_MASK             0xC0000000
#define PARAM_PACKET_FILTER_PROBE_REQ           0x80000000
#define PARAM_PACKET_FILTER_ACTION_FRAME      0x40000000
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
#define PARAM_CAL_DATA_DUMP_MAX_NUM		300
#endif

#define BT_PROFILE_PARAM_LEN        8

#define EFUSE_ADDR_MAX  0x3BF	/* Based on EEPROM layout 20160120 */
#if CFG_SUPPORT_BUFFER_MODE

/* For MT7668 */
#define EFUSE_CONTENT_BUFFER_START        0x03A
#define EFUSE_CONTENT_BUFFER_END          0x1D9
#define EFUSE_CONTENT_BUFFER_SIZE		  (EFUSE_CONTENT_BUFFER_END - EFUSE_CONTENT_BUFFER_START + 1)

#define DEFAULT_EFUSE_MACADDR_OFFSET	  4


/* For MT6632 */
#define EFUSE_CONTENT_SIZE			16

#define EFUSE_BLOCK_SIZE 16
#define EEPROM_SIZE 1184
#define MAX_EEPROM_BUFFER_SIZE	1200
#endif /* CFG_SUPPORT_BUFFER_MODE */

#if CFG_SUPPORT_TX_BF
#define TXBF_CMD_NEED_TO_RESPONSE(u4TxBfCmdId) (u4TxBfCmdId == BF_PFMU_TAG_READ || \
						u4TxBfCmdId == BF_PROFILE_READ)
#endif /* CFG_SUPPORT_TX_BF */
#define MU_CMD_NEED_TO_RESPONSE(u4MuCmdId) (u4MuCmdId == MU_GET_CALC_INIT_MCS || \
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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Parameters of User Configuration which match to NDIS5.1                    */
/*----------------------------------------------------------------------------*/
/* NDIS_802_11_AUTHENTICATION_MODE */
typedef enum _ENUM_PARAM_AUTH_MODE_T {
	AUTH_MODE_OPEN,		/*!< Open system */
	AUTH_MODE_SHARED,	/*!< Shared key */
	AUTH_MODE_AUTO_SWITCH,	/*!< Either open system or shared key */
	AUTH_MODE_WPA,
	AUTH_MODE_WPA_PSK,
	AUTH_MODE_WPA_NONE,	/*!< For Ad hoc */
	AUTH_MODE_WPA2,
	AUTH_MODE_WPA2_PSK,
#if CFG_SUPPORT_CFG80211_AUTH
	AUTH_MODE_WPA2_SAE,
#endif
	AUTH_MODE_NUM		/*!< Upper bound, not real case */
} ENUM_PARAM_AUTH_MODE_T, *P_ENUM_PARAM_AUTH_MODE_T;

/* NDIS_802_11_ENCRYPTION_STATUS *//* Encryption types */
typedef enum _ENUM_WEP_STATUS_T {
	ENUM_WEP_ENABLED,
	ENUM_ENCRYPTION1_ENABLED = ENUM_WEP_ENABLED,
	ENUM_WEP_DISABLED,
	ENUM_ENCRYPTION_DISABLED = ENUM_WEP_DISABLED,
	ENUM_WEP_KEY_ABSENT,
	ENUM_ENCRYPTION1_KEY_ABSENT = ENUM_WEP_KEY_ABSENT,
	ENUM_WEP_NOT_SUPPORTED,
	ENUM_ENCRYPTION_NOT_SUPPORTED = ENUM_WEP_NOT_SUPPORTED,
	ENUM_ENCRYPTION2_ENABLED,
	ENUM_ENCRYPTION2_KEY_ABSENT,
	ENUM_ENCRYPTION3_ENABLED,
	ENUM_ENCRYPTION3_KEY_ABSENT,
#if CFG_SUPPORT_SUITB
	ENUM_ENCRYPTION4_ENABLED,
	ENUM_ENCRYPTION4_KEY_ABSENT,
#endif
	ENUM_ENCRYPTION_NUM
} ENUM_PARAM_ENCRYPTION_STATUS_T, *P_ENUM_PARAM_ENCRYPTION_STATUS_T;

typedef UINT_8 PARAM_MAC_ADDRESS[PARAM_MAC_ADDR_LEN];

typedef UINT_32 PARAM_KEY_INDEX;
typedef UINT_64 PARAM_KEY_RSC;
typedef INT_32 PARAM_RSSI;

typedef UINT_32 PARAM_FRAGMENTATION_THRESHOLD;
typedef UINT_32 PARAM_RTS_THRESHOLD;

typedef UINT_8 PARAM_RATES[PARAM_MAX_LEN_RATES];
typedef UINT_8 PARAM_RATES_EX[PARAM_MAX_LEN_RATES_EX];

typedef enum _ENUM_PARAM_PHY_TYPE_T {
	PHY_TYPE_802_11ABG = 0,	/*!< Can associated with 802.11abg AP,
				* Scan dual band.
				*/
	PHY_TYPE_802_11BG,	/*!< Can associated with 802_11bg AP,
				*  Scan single band and not report 802_11a BSSs.
				*/
	PHY_TYPE_802_11G,	/*!< Can associated with 802_11g only AP,
				* Scan single band and not report 802_11ab BSSs.
				*/
	PHY_TYPE_802_11A,	/*!< Can associated with 802_11a only AP,
				* Scan single band and not report 802_11bg BSSs.
				*/
	PHY_TYPE_802_11B,	/*!< Can associated with 802_11b only AP
				* Scan single band and not report 802_11ag BSSs.
				*/
	PHY_TYPE_NUM		/* 5 */
} ENUM_PARAM_PHY_TYPE_T, *P_ENUM_PARAM_PHY_TYPE_T;

typedef enum _ENUM_PARAM_OP_MODE_T {
	NET_TYPE_IBSS = 0,	/*!< Try to merge/establish an AdHoc, do periodic SCAN for merging. */
	NET_TYPE_INFRA,		/*!< Try to join an Infrastructure, do periodic SCAN for joining. */
	NET_TYPE_AUTO_SWITCH,	/*!< Try to join an Infrastructure, if fail then try to merge or */
				/*  establish an AdHoc, do periodic SCAN for joining or merging. */
	NET_TYPE_DEDICATED_IBSS,	/*!< Try to merge an AdHoc first, */
					/* if fail then establish AdHoc permanently, no more SCAN. */
	NET_TYPE_NUM		/* 4 */
} ENUM_PARAM_OP_MODE_T, *P_ENUM_PARAM_OP_MODE_T;

typedef struct _PARAM_SSID_T {
	UINT_32 u4SsidLen;	/*!< SSID length in bytes. Zero length is broadcast(any) SSID */
	UINT_8 aucSsid[PARAM_MAX_LEN_SSID];
} PARAM_SSID_T, *P_PARAM_SSID_T;

typedef struct _PARAM_CONNECT_T {
	UINT_32 u4SsidLen;	/*!< SSID length in bytes. Zero length is broadcast(any) SSID */
	UINT_8 *pucSsid;
	UINT_8 *pucBssid;
	UINT_32 u4CenterFreq;
} PARAM_CONNECT_T, *P_PARAM_CONNECT_T;

/* This is enum defined for user to select an AdHoc Mode */
typedef enum _ENUM_PARAM_AD_HOC_MODE_T {
	AD_HOC_MODE_11B = 0,	/*!< Create 11b IBSS if we support 802.11abg/802.11bg. */
	AD_HOC_MODE_MIXED_11BG,	/*!< Create 11bg mixed IBSS if we support 802.11abg/802.11bg/802.11g. */
	AD_HOC_MODE_11G,	/*!< Create 11g only IBSS if we support 802.11abg/802.11bg/802.11g. */
	AD_HOC_MODE_11A,	/*!< Create 11a only IBSS if we support 802.11abg. */
	AD_HOC_MODE_NUM		/* 4 */
} ENUM_PARAM_AD_HOC_MODE_T, *P_ENUM_PARAM_AD_HOC_MODE_T;

typedef enum _ENUM_PARAM_MEDIA_STATE_T {
	PARAM_MEDIA_STATE_CONNECTED,
	PARAM_MEDIA_STATE_DISCONNECTED,
	PARAM_MEDIA_STATE_TO_BE_INDICATED	/* for following MSDN re-association behavior */
} ENUM_PARAM_MEDIA_STATE_T, *P_ENUM_PARAM_MEDIA_STATE_T;

typedef enum _ENUM_PARAM_NETWORK_TYPE_T {
	PARAM_NETWORK_TYPE_FH,
	PARAM_NETWORK_TYPE_DS,
	PARAM_NETWORK_TYPE_OFDM5,
	PARAM_NETWORK_TYPE_OFDM24,
	PARAM_NETWORK_TYPE_AUTOMODE,
	PARAM_NETWORK_TYPE_NUM	/*!< Upper bound, not real case */
} ENUM_PARAM_NETWORK_TYPE_T, *P_ENUM_PARAM_NETWORK_TYPE_T;

typedef struct _PARAM_NETWORK_TYPE_LIST {
	UINT_32 NumberOfItems;	/*!< At least 1 */
	ENUM_PARAM_NETWORK_TYPE_T eNetworkType[1];
} PARAM_NETWORK_TYPE_LIST, *PPARAM_NETWORK_TYPE_LIST;

typedef enum _ENUM_PARAM_PRIVACY_FILTER_T {
	PRIVACY_FILTER_ACCEPT_ALL,
	PRIVACY_FILTER_8021xWEP,
	PRIVACY_FILTER_NUM
} ENUM_PARAM_PRIVACY_FILTER_T, *P_ENUM_PARAM_PRIVACY_FILTER_T;

typedef enum _ENUM_RELOAD_DEFAULTS {
	ENUM_RELOAD_WEP_KEYS
} PARAM_RELOAD_DEFAULTS, *P_PARAM_RELOAD_DEFAULTS;

typedef struct _PARAM_PM_PACKET_PATTERN {
	UINT_32 Priority;	/* Importance of the given pattern. */
	UINT_32 Reserved;	/* Context information for transports. */
	UINT_32 MaskSize;	/* Size in bytes of the pattern mask. */
	UINT_32 PatternOffset;	/* Offset from beginning of this */
	/* structure to the pattern bytes. */
	UINT_32 PatternSize;	/* Size in bytes of the pattern. */
	UINT_32 PatternFlags;	/* Flags (TBD). */
} PARAM_PM_PACKET_PATTERN, *P_PARAM_PM_PACKET_PATTERN;


/* Combine ucTpTestMode and ucSigmaTestMode in one flag */
/* ucTpTestMode == 0, for normal driver */
/* ucTpTestMode == 1, for pure throughput test mode (ex: RvR) */
/* ucTpTestMode == 2, for sigma TGn/TGac/PMF */
/* ucTpTestMode == 3, for sigma WMM PS */
typedef enum _ENUM_TP_TEST_MODE_T {
	ENUM_TP_TEST_MODE_NORMAL = 0,
	ENUM_TP_TEST_MODE_THROUGHPUT,
	ENUM_TP_TEST_MODE_SIGMA_AC_N_PMF,
	ENUM_TP_TEST_MODE_SIGMA_WMM_PS,
	ENUM_TP_TEST_MODE_NUM
} ENUM_TP_TEST_MODE_T, *P_ENUM_TP_TEST_MODE_T;

/*--------------------------------------------------------------*/
/*! \brief Struct definition to indicate specific event.                */
/*--------------------------------------------------------------*/
typedef enum _ENUM_STATUS_TYPE_T {
	ENUM_STATUS_TYPE_AUTHENTICATION,
	ENUM_STATUS_TYPE_MEDIA_STREAM_MODE,
	ENUM_STATUS_TYPE_CANDIDATE_LIST,
	ENUM_STATUS_TYPE_NUM	/*!< Upper bound, not real case */
} ENUM_STATUS_TYPE_T, *P_ENUM_STATUS_TYPE_T;

typedef struct _PARAM_802_11_CONFIG_FH_T {
	UINT_32 u4Length;	/*!< Length of structure */
	UINT_32 u4HopPattern;	/*!< Defined as 802.11 */
	UINT_32 u4HopSet;	/*!< to one if non-802.11 */
	UINT_32 u4DwellTime;	/*!< In unit of Kusec */
} PARAM_802_11_CONFIG_FH_T, *P_PARAM_802_11_CONFIG_FH_T;

typedef struct _PARAM_802_11_CONFIG_T {
	UINT_32 u4Length;	/*!< Length of structure */
	UINT_32 u4BeaconPeriod;	/*!< In unit of Kusec */
	UINT_32 u4ATIMWindow;	/*!< In unit of Kusec */
	UINT_32 u4DSConfig;	/*!< Channel frequency in unit of kHz */
	PARAM_802_11_CONFIG_FH_T rFHConfig;
} PARAM_802_11_CONFIG_T, *P_PARAM_802_11_CONFIG_T;

typedef struct _PARAM_STATUS_INDICATION_T {
	ENUM_STATUS_TYPE_T eStatusType;
} PARAM_STATUS_INDICATION_T, *P_PARAM_STATUS_INDICATION_T;

typedef struct _PARAM_AUTH_REQUEST_T {
	UINT_32 u4Length;	/*!< Length of this struct */
	PARAM_MAC_ADDRESS arBssid;
	UINT_32 u4Flags;	/*!< Definitions are as follows */
} PARAM_AUTH_REQUEST_T, *P_PARAM_AUTH_REQUEST_T;

typedef struct _PARAM_AUTH_EVENT_T {
	PARAM_STATUS_INDICATION_T rStatus;
	PARAM_AUTH_REQUEST_T arRequest[1];
} PARAM_AUTH_EVENT_T, *P_PARAM_AUTH_EVENT_T;

/*! \brief Capabilities, privacy, rssi and IEs of each BSSID */
typedef struct _PARAM_BSSID_EX_T {
	UINT_32 u4Length;	/*!< Length of structure */
	PARAM_MAC_ADDRESS arMacAddress;	/*!< BSSID */
	UINT_8 Reserved[2];
	PARAM_SSID_T rSsid;	/*!< SSID */
	UINT_32 u4Privacy;	/*!< Need WEP encryption */
	PARAM_RSSI rRssi;	/*!< in dBm */
	ENUM_PARAM_NETWORK_TYPE_T eNetworkTypeInUse;
	PARAM_802_11_CONFIG_T rConfiguration;
	ENUM_PARAM_OP_MODE_T eOpMode;
	PARAM_RATES_EX rSupportedRates;
	UINT_32 u4IELength;
	UINT_8 aucIEs[1];
} PARAM_BSSID_EX_T, *P_PARAM_BSSID_EX_T;

typedef struct _PARAM_BSSID_LIST_EX {
	UINT_32 u4NumberOfItems;	/*!< at least 1 */
	PARAM_BSSID_EX_T arBssid[1];
} PARAM_BSSID_LIST_EX_T, *P_PARAM_BSSID_LIST_EX_T;

typedef struct _PARAM_WEP_T {
	UINT_32 u4Length;	/*!< Length of structure */
	UINT_32 u4KeyIndex;	/*!< 0: pairwise key, others group keys */
	UINT_32 u4KeyLength;	/*!< Key length in bytes */
	UINT_8 aucKeyMaterial[32];	/*!< Key content by above setting */
} PARAM_WEP_T, *P_PARAM_WEP_T;

/*! \brief Key mapping of BSSID */
typedef struct _PARAM_KEY_T {
	UINT_32 u4Length;	/*!< Length of structure */
	UINT_32 u4KeyIndex;	/*!< KeyID */
	UINT_32 u4KeyLength;	/*!< Key length in bytes */
	PARAM_MAC_ADDRESS arBSSID;	/*!< MAC address */
	PARAM_KEY_RSC rKeyRSC;
	UINT_8 ucBssIdx;
	UINT_8 ucCipher;
	UINT_8 aucKeyMaterial[32];	/*!< Key content by above setting */
	/* Following add to change the original windows structure */
} PARAM_KEY_T, *P_PARAM_KEY_T;

typedef struct _PARAM_REMOVE_KEY_T {
	UINT_32 u4Length;	/*!< Length of structure */
	UINT_32 u4KeyIndex;	/*!< KeyID */
	PARAM_MAC_ADDRESS arBSSID;	/*!< MAC address */
	UINT_8 ucBssIdx;
} PARAM_REMOVE_KEY_T, *P_PARAM_REMOVE_KEY_T;

/*! \brief Default key */
typedef struct _PARAM_DEFAULT_KEY_T {
	UINT_8 ucKeyID;
	UINT_8 ucUnicast;
	UINT_8 ucMulticast;
	UINT_8 ucBssIdx;
} PARAM_DEFAULT_KEY_T, *P_PARAM_DEFAULT_KEY_T;

#if CFG_SUPPORT_WAPI
typedef enum _ENUM_KEY_TYPE {
	ENUM_WPI_PAIRWISE_KEY = 0,
	ENUM_WPI_GROUP_KEY
} ENUM_KEY_TYPE;

typedef enum _ENUM_WPI_PROTECT_TYPE {
	ENUM_WPI_NONE,
	ENUM_WPI_RX,
	ENUM_WPI_TX,
	ENUM_WPI_RX_TX
} ENUM_WPI_PROTECT_TYPE;

typedef struct _PARAM_WPI_KEY_T {
	ENUM_KEY_TYPE eKeyType;
	ENUM_WPI_PROTECT_TYPE eDirection;
	UINT_8 ucKeyID;
	UINT_8 aucRsv[3];
	UINT_8 aucAddrIndex[12];
	UINT_32 u4LenWPIEK;
	UINT_8 aucWPIEK[256];
	UINT_32 u4LenWPICK;
	UINT_8 aucWPICK[256];
	UINT_8 aucPN[16];
} PARAM_WPI_KEY_T, *P_PARAM_WPI_KEY_T;
#endif

typedef enum _PARAM_POWER_MODE {
	Param_PowerModeCAM,
	Param_PowerModeMAX_PSP,
	Param_PowerModeFast_PSP,
	Param_PowerModeMax	/* Upper bound, not real case */
} PARAM_POWER_MODE, *PPARAM_POWER_MODE;

typedef enum _PARAM_DEVICE_POWER_STATE {
	ParamDeviceStateUnspecified = 0,
	ParamDeviceStateD0,
	ParamDeviceStateD1,
	ParamDeviceStateD2,
	ParamDeviceStateD3,
	ParamDeviceStateMaximum
} PARAM_DEVICE_POWER_STATE, *PPARAM_DEVICE_POWER_STATE;

#if CFG_SUPPORT_FW_DBG_LEVEL_CTRL
/* FW debug control level related definition and enumerations */
#define FW_DBG_LEVEL_DONT_SET   0
#define FW_DBG_LEVEL_ERROR      (1 << 0)
#define FW_DBG_LEVEL_WARN       (1 << 1)
#define FW_DBG_LEVEL_STATE      (1 << 2)
#define FW_DBG_LEVEL_INFO       (1 << 3)
#define FW_DBG_LEVEL_LOUD       (1 << 4)
#endif

typedef struct _PARAM_POWER_MODE_T {
	UINT_8 ucBssIdx;
	PARAM_POWER_MODE ePowerMode;
} PARAM_POWER_MODE_T, *P_PARAM_POWER_MODE_T;

#if CFG_SUPPORT_802_11D

/*! \brief The enumeration definitions for OID_IPN_MULTI_DOMAIN_CAPABILITY */
typedef enum _PARAM_MULTI_DOMAIN_CAPABILITY {
	ParamMultiDomainCapDisabled,
	ParamMultiDomainCapEnabled
} PARAM_MULTI_DOMAIN_CAPABILITY, *P_PARAM_MULTI_DOMAIN_CAPABILITY;
#endif

typedef struct _COUNTRY_STRING_ENTRY {
	UINT_8 aucCountryCode[2];
	UINT_8 aucEnvironmentCode[2];
} COUNTRY_STRING_ENTRY, *P_COUNTRY_STRING_ENTRY;

/* Power management related definition and enumerations */
#define UAPSD_NONE                              0
#define UAPSD_AC0                               (BIT(0) | BIT(4))
#define UAPSD_AC1                               (BIT(1) | BIT(5))
#define UAPSD_AC2                               (BIT(2) | BIT(6))
#define UAPSD_AC3                               (BIT(3) | BIT(7))
#define UAPSD_ALL                               (UAPSD_AC0 | UAPSD_AC1 | UAPSD_AC2 | UAPSD_AC3)

typedef enum _ENUM_POWER_SAVE_PROFILE_T {
	ENUM_PSP_CONTINUOUS_ACTIVE = 0,
	ENUM_PSP_CONTINUOUS_POWER_SAVE,
	ENUM_PSP_FAST_SWITCH,
	ENUM_PSP_NUM
} ENUM_POWER_SAVE_PROFILE_T, *PENUM_POWER_SAVE_PROFILE_T;

/*--------------------------------------------------------------*/
/*! \brief Set/Query testing type.                              */
/*--------------------------------------------------------------*/
typedef struct _PARAM_802_11_TEST_T {
	UINT_32 u4Length;
	UINT_32 u4Type;
	union {
		PARAM_AUTH_EVENT_T AuthenticationEvent;
		PARAM_RSSI RssiTrigger;
	} u;
} PARAM_802_11_TEST_T, *P_PARAM_802_11_TEST_T;

/*--------------------------------------------------------------*/
/*! \brief Set/Query authentication and encryption capability.  */
/*--------------------------------------------------------------*/
typedef struct _PARAM_AUTH_ENCRYPTION_T {
	ENUM_PARAM_AUTH_MODE_T eAuthModeSupported;
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncryptStatusSupported;
} PARAM_AUTH_ENCRYPTION_T, *P_PARAM_AUTH_ENCRYPTION_T;

typedef struct _PARAM_CAPABILITY_T {
	UINT_32 u4Length;
	UINT_32 u4Version;
	UINT_32 u4NoOfPMKIDs;
	UINT_32 u4NoOfAuthEncryptPairsSupported;
	PARAM_AUTH_ENCRYPTION_T arAuthenticationEncryptionSupported[1];
} PARAM_CAPABILITY_T, *P_PARAM_CAPABILITY_T;

typedef UINT_8 PARAM_PMKID_VALUE[16];

typedef struct _PARAM_BSSID_INFO_T {
	PARAM_MAC_ADDRESS arBSSID;
	PARAM_PMKID_VALUE arPMKID;
} PARAM_BSSID_INFO_T, *P_PARAM_BSSID_INFO_T;

typedef struct _PARAM_PMKID_T {
	UINT_32 u4Length;
	UINT_32 u4BSSIDInfoCount;
	PARAM_BSSID_INFO_T arBSSIDInfo[1];
} PARAM_PMKID_T, *P_PARAM_PMKID_T;

/*! \brief PMKID candidate lists. */
typedef struct _PARAM_PMKID_CANDIDATE_T {
	PARAM_MAC_ADDRESS arBSSID;
	UINT_32 u4Flags;
} PARAM_PMKID_CANDIDATE_T, *P_PARAM_PMKID_CANDIDATE_T;

/* #ifdef LINUX */
typedef struct _PARAM_PMKID_CANDIDATE_LIST_T {
	UINT_32 u4Version;	/*!< Version */
	UINT_32 u4NumCandidates;	/*!< How many candidates follow */
	PARAM_PMKID_CANDIDATE_T arCandidateList[1];
} PARAM_PMKID_CANDIDATE_LIST_T, *P_PARAM_PMKID_CANDIDATE_LIST_T;
/* #endif */

#define NL80211_KCK_LEN                 16
#define NL80211_KEK_LEN                 16
#define NL80211_REPLAY_CTR_LEN          8
#define NL80211_KEYRSC_LEN		8

typedef struct _PARAM_GTK_REKEY_DATA {
	UINT_8 aucKek[NL80211_KEK_LEN];
	UINT_8 aucKck[NL80211_KCK_LEN];
	UINT_8 aucReplayCtr[NL80211_REPLAY_CTR_LEN];
	UINT_8 ucBssIndex;
	UINT_8 ucRekeyMode;
	UINT_8 ucCurKeyId;
	UINT_8 ucRsv;
	UINT_32 u4Proto;
	UINT_32 u4PairwiseCipher;
	UINT_32 u4GroupCipher;
	UINT_32 u4KeyMgmt;
	UINT_32 u4MgmtGroupCipher;
} PARAM_GTK_REKEY_DATA, *P_PARAM_GTK_REKEY_DATA;

typedef struct _PARAM_CUSTOM_MCR_RW_STRUCT_T {
	UINT_32 u4McrOffset;
	UINT_32 u4McrData;
} PARAM_CUSTOM_MCR_RW_STRUCT_T, *P_PARAM_CUSTOM_MCR_RW_STRUCT_T;

#define COEX_CTRL_BUF_LEN 460
#define COEX_INFO_LEN 115

/* CMD_COEX_CTRL & EVENT_COEX_CTRL */
/************************************************/
/*  UINT_32 u4SubCmd : Coex Ctrl Sub Command    */
/*  UINT_8 aucBuffer : Reserve for Sub Command  */
/*                        Data Structure        */
/************************************************/
struct PARAM_COEX_CTRL {
	UINT_32 u4SubCmd;
	UINT_8  aucBuffer[COEX_CTRL_BUF_LEN];
};

/* Isolation Structure */
/************************************************/
/*  UINT_32 u4IsoPath : WF Path (WF0/WF1)       */
/*  UINT_32 u4Channel : WF Channel              */
/*  UINT_32 u4Band    : WF Band (Band0/Band1)(Not used now)   */
/*  UINT_32 u4Isolation  : Isolation value      */
/************************************************/
struct PARAM_COEX_ISO_DETECT {
	UINT_32 u4IsoPath;
	UINT_32 u4Channel;
	/*UINT_32 u4Band;*/
	UINT_32 u4Isolation;
};

/* Coex Info Structure */
/************************************************/
/*  char   cCoexInfo[];                        */
/************************************************/
struct PARAM_COEX_GET_INFO {
	UINT_32   u4CoexInfo[COEX_INFO_LEN];
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
typedef struct _PARAM_CAL_BACKUP_STRUCT_V2_T {
	UINT_8	ucReason;
	UINT_8	ucAction;
	UINT_8	ucNeedResp;
	UINT_8	ucFragNum;
	UINT_8	ucRomRam;
	UINT_32	u4ThermalValue;
	UINT_32 u4Address;
	UINT_32	u4Length;
	UINT_32	u4RemainLength;
} PARAM_CAL_BACKUP_STRUCT_V2_T, *P_PARAM_CAL_BACKUP_STRUCT_V2_T;
#endif

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
typedef struct _BIN_CONTENT_T {
	UINT_16 u2Addr;
	UINT_8 ucValue;
	UINT_8 ucReserved;
} BIN_CONTENT_T, *P_BIN_CONTENT_T;

typedef struct _PARAM_CUSTOM_EFUSE_BUFFER_MODE_T {
	UINT_8 ucSourceMode;
	UINT_8 ucCount;
	UINT_8 ucCmdType;
	UINT_8 ucReserved;
	UINT_8 aBinContent[MAX_EEPROM_BUFFER_SIZE];
} PARAM_CUSTOM_EFUSE_BUFFER_MODE_T, *P_PARAM_CUSTOM_EFUSE_BUFFER_MODE_T;

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
typedef struct _PARAM_CUSTOM_ACCESS_EFUSE_T {
	UINT_32 u4Address;
	UINT_32 u4Valid;
	UINT_8 aucData[16];
} PARAM_CUSTOM_ACCESS_EFUSE_T, *P_PARAM_CUSTOM_ACCESS_EFUSE_T;


typedef struct _PARAM_CUSTOM_EFUSE_FREE_BLOCK_T {
	UINT_8  ucGetFreeBlock;
	UINT_8  aucReserved[3];
} PARAM_CUSTOM_EFUSE_FREE_BLOCK_T, *P_PARAM_CUSTOM_EFUSE_FREE_BLOCK_T;


typedef struct _PARAM_CUSTOM_GET_TX_POWER_T {
	UINT_8 ucTxPwrType;
	UINT_8 ucCenterChannel;
	UINT_8 ucDbdcIdx; /* 0:Band 0, 1: Band1 */
	UINT_8 ucBand; /* 0:G-band 1: A-band*/
	UINT_8 ucReserved[4];
} PARAM_CUSTOM_GET_TX_POWER_T, *P_PARAM_CUSTOM_GET_TX_POWER_T;


/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */

typedef struct _PARAM_CUSTOM_SET_TX_TARGET_POWER_T {
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
} PARAM_CUSTOM_SET_TX_TARGET_POWER_T, *P_PARAM_CUSTOM_SET_TX_TARGET_POWER_T;

#if (CFG_SUPPORT_DFS_MASTER == 1)
typedef struct _PARAM_CUSTOM_SET_RDD_REPORT_T {
	UINT_8 ucDbdcIdx; /* 0:Band 0, 1: Band1 */
} PARAM_CUSTOM_SET_RDD_REPORT_T, *P_PARAM_CUSTOM_SET_RDD_REPORT_T;

struct PARAM_CUSTOM_SET_RADAR_DETECT_MODE {
	UINT_8 ucRadarDetectMode; /* 0:Switch channel, 1: Don't switch channel */
};
#endif

typedef struct _PARAM_CUSTOM_ACCESS_RX_STAT {
	UINT_32 u4SeqNum;
	UINT_32 u4TotalNum;
} PARAM_CUSTOM_ACCESS_RX_STAT, *P_PARAM_CUSTOM_ACCESS_RX_STAT;

/* Ext DevInfo Tag */
typedef enum _EXT_ENUM_DEVINFO_TAG_HANDLE_T {
	DEV_INFO_ACTIVE = 0,
	DEV_INFO_BSSID,
	DEV_INFO_MAX_NUM
} EXT_ENUM_TAG_DEVINFO_HANDLE_T;

/*  STA record TLV tag */
typedef enum _EXT_ENUM_STAREC_TAG_HANDLE_T {
	STA_REC_BASIC = 0,
	STA_REC_RA,
	STA_REC_RA_COMMON_INFO,
	STA_REC_RA_UPDATE,
	STA_REC_BF,
	STA_REC_MAUNAL_ASSOC,
	STA_REC_BA = 6,
	STA_REC_MAX_NUM
} EXT_ENUM_TAG_STAREC_HANDLE_T;

#if CFG_SUPPORT_TX_BF
typedef enum _BF_ACTION_CATEGORY {
	BF_SOUNDING_OFF = 0,
	BF_SOUNDING_ON,
	BF_HW_CTRL,
	BF_DATA_PACKET_APPLY,
	BF_PFMU_MEM_ALLOCATE,
	BF_PFMU_MEM_RELEASE,
	BF_PFMU_TAG_READ,
	BF_PFMU_TAG_WRITE,
	BF_PROFILE_READ,
	BF_PROFILE_WRITE,
	BF_PN_READ,
	BF_PN_WRITE,
	BF_PFMU_MEM_ALLOC_MAP_READ
} BF_ACTION_CATEGORY;

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

typedef union _PFMU_PROFILE_TAG1 {
	struct {
		UINT_32 ucProfileID:7;	/* [6:0] : 0 ~ 63 */
		UINT_32 ucTxBf:1;	/* [7] : 0: iBF, 1: eBF */
		UINT_32 ucDBW:2;	/* [9:8] : 0/1/2/3: DW20/40/80/160NC */
		UINT_32 ucSU_MU:1;	/* [10] : 0:SU, 1: MU */
		UINT_32 ucInvalidProf:1;	/* [11] : 0:default, 1: This profile number is invalid by SW */
		UINT_32 ucRMSD:3;	/* [14:12] : RMSD value from CE */
		UINT_32 ucMemAddr1ColIdx:3;	/* [17 : 15] : column index : 0 ~ 5 */
		UINT_32 ucMemAddr1RowIdx:6;	/* [23 : 18] : row index : 0 ~ 63 */
		UINT_32 ucMemAddr2ColIdx:3;	/* [26 : 24] : column index : 0 ~ 5 */
		UINT_32 ucMemAddr2RowIdx:5;	/* [31 : 27] : row index : 0 ~ 63 */
		UINT_32 ucMemAddr2RowIdxMsb:1;	/* [32] : MSB of row index */
		UINT_32 ucMemAddr3ColIdx:3;	/* [35 : 33] : column index : 0 ~ 5 */
		UINT_32 ucMemAddr3RowIdx:6;	/* [41 : 36] : row index : 0 ~ 63 */
		UINT_32 ucMemAddr4ColIdx:3;	/* [44 : 42] : column index : 0 ~ 5 */
		UINT_32 ucMemAddr4RowIdx:6;	/* [50 : 45] : row index : 0 ~ 63 */
		UINT_32 ucReserved:1;	/* [51] : Reserved */
		UINT_32 ucNrow:2;	/* [53 : 52] : Nrow */
		UINT_32 ucNcol:2;	/* [55 : 54] : Ncol */
		UINT_32 ucNgroup:2;	/* [57 : 56] : Ngroup */
		UINT_32 ucLM:2;	/* [59 : 58] : 0/1/2 */
		UINT_32 ucCodeBook:2;	/* [61:60] : Code book */
		UINT_32 ucHtcExist:1;	/* [62] : HtcExist */
		UINT_32 ucReserved1:1;	/* [63] : Reserved */
		UINT_32 ucSNR_STS0:8;	/* [71:64] : SNR_STS0 */
		UINT_32 ucSNR_STS1:8;	/* [79:72] : SNR_STS1 */
		UINT_32 ucSNR_STS2:8;	/* [87:80] : SNR_STS2 */
		UINT_32 ucSNR_STS3:8;	/* [95:88] : SNR_STS3 */
		UINT_32 ucIBfLnaIdx:8;	/* [103:96] : iBF LNA index */
	} rField;
	UINT_32 au4RawData[4];
} PFMU_PROFILE_TAG1, *P_PFMU_PROFILE_TAG1;

typedef union _PFMU_PROFILE_TAG2 {
	struct {
		UINT_32 u2SmartAnt:12;	/* [11:0] : Smart Ant config */
		UINT_32 ucReserved0:3;	/* [14:12] : Reserved */
		UINT_32 ucSEIdx:5;	/* [19:15] : SE index */
		UINT_32 ucRMSDThd:3;	/* [22:20] : RMSD Threshold */
		UINT_32 ucReserved1:1;	/* [23] : Reserved */
		UINT_32 ucMCSThL1SS:4;	/* [27:24] : MCS TH long 1SS */
		UINT_32 ucMCSThS1SS:4;	/* [31:28] : MCS TH short 1SS */
		UINT_32 ucMCSThL2SS:4;	/* [35:32] : MCS TH long 2SS */
		UINT_32 ucMCSThS2SS:4;	/* [39:36] : MCS TH short 2SS */
		UINT_32 ucMCSThL3SS:4;	/* [43:40] : MCS TH long 3SS */
		UINT_32 ucMCSThS3SS:4;	/* [47:44] : MCS TH short 3SS */
		UINT_32 uciBfTimeOut:8;	/* [55:48] : iBF timeout limit */
		UINT_32 ucReserved2:8;	/* [63:56] : Reserved */
		UINT_32 ucReserved3:8;	/* [71:64] : Reserved */
		UINT_32 ucReserved4:8;	/* [79:72] : Reserved */
		UINT_32 uciBfDBW:2;	/* [81:80] : iBF desired DBW 0/1/2/3 : BW20/40/80/160NC */
		UINT_32 uciBfNcol:2;	/* [83:82] : iBF desired Ncol = 1 ~ 3 */
		UINT_32 uciBfNrow:2;	/* [85:84] : iBF desired Nrow = 1 ~ 4 */
		UINT_32 u2Reserved5:10;	/* [95:86] : Reserved */
	} rField;
	UINT_32 au4RawData[3];
} PFMU_PROFILE_TAG2, *P_PFMU_PROFILE_TAG2;

typedef union _PFMU_DATA {
	struct {
		UINT_32 u2Phi11:9;
		UINT_32 ucPsi21:7;
		UINT_32 u2Phi21:9;
		UINT_32 ucPsi31:7;
		UINT_32 u2Phi31:9;
		UINT_32 ucPsi41:7;
		UINT_32 u2Phi22:9;
		UINT_32 ucPsi32:7;
		UINT_32 u2Phi32:9;
		UINT_32 ucPsi42:7;
		UINT_32 u2Phi33:9;
		UINT_32 ucPsi43:7;
		UINT_32 u2dSNR00:4;
		UINT_32 u2dSNR01:4;
		UINT_32 u2dSNR02:4;
		UINT_32 u2dSNR03:4;
		UINT_32 u2Reserved:16;
	} rField;
	UINT_32 au4RawData[5];
} PFMU_DATA, *P_PFMU_DATA;

typedef struct _PROFILE_TAG_READ_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucProfileIdx;
	BOOLEAN fgBfer;
	UINT_8 ucRsv;
} PROFILE_TAG_READ_T, *P_PROFILE_TAG_READ_T;

typedef struct _PROFILE_TAG_WRITE_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucPfmuId;
	UINT_8 ucBuffer[28];
} PROFILE_TAG_WRITE_T, *P_PROFILE_TAG_WRITE_T;

typedef struct _PROFILE_DATA_READ_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucPfmuIdx;
	BOOLEAN fgBFer;
	UINT_8 ucReserved[3];
	UINT_8 ucSubCarrIdxLsb;
	UINT_8 ucSubCarrIdxMsb;
} PROFILE_DATA_READ_T, *P_PROFILE_DATA_READ_T;

typedef struct _PROFILE_DATA_WRITE_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucPfmuIdx;
	UINT_8 u2SubCarrIdxLsb;
	UINT_8 u2SubCarrIdxMsb;
	PFMU_DATA rTxBfPfmuData;
} PROFILE_DATA_WRITE_T, *P_PROFILE_DATA_WRITE_T;

typedef struct _PROFILE_PN_READ_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucPfmuIdx;
	UINT_8 ucReserved[2];
} PROFILE_PN_READ_T, *P_PROFILE_PN_READ_T;

typedef struct _PROFILE_PN_WRITE_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucPfmuIdx;
	UINT_16 u2bw;
	UINT_8 ucBuf[32];
} PROFILE_PN_WRITE_T, *P_PROFILE_PN_WRITE_T;

typedef enum _BF_SOUNDING_MODE {
	SU_SOUNDING = 0,
	MU_SOUNDING,
	SU_PERIODIC_SOUNDING,
	MU_PERIODIC_SOUNDING,
	AUTO_SU_PERIODIC_SOUNDING
} BF_SOUNDING_MODE;

typedef struct _EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL_T {
	UINT_8 ucCmdCategoryID;
	UINT_8 ucSuMuSndMode;
	UINT_8 ucWlanIdx;
	UINT_32 u4SoundingInterval;	/* By ms */
} EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL_T, *P_EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL_T;

typedef struct _EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL_T {
	UINT_8 ucCmdCategoryID;
	UINT_8 ucSuMuSndMode;
	UINT_8 ucWlanId[4];
	UINT_8 ucStaNum;
	UINT_32 u4SoundingInterval;	/* By ms */
} EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL_T, *P_EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL_T;

/* Device information (Tag0) */
typedef struct _CMD_DEVINFO_ACTIVE_T {
	UINT_16 u2Tag;		/* Tag = 0x00 */
	UINT_16 u2Length;
	UINT_8 ucActive;
	UINT_8 ucBandNum;
	UINT_8 aucOwnMacAddr[6];
	UINT_8 aucReserve[4];
} CMD_DEVINFO_ACTIVE_T, *P_CMD_DEVINFO_ACTIVE_T;

typedef struct _BSSINFO_BASIC_T {
	/* Basic BSS information (Tag1) */
	UINT_16 u2Tag;		/* Tag = 0x01 */
	UINT_16 u2Length;
	UINT_32 u4NetworkType;
	UINT_8 ucActive;
	UINT_8 ucReserve0;
	UINT_16 u2BcnInterval;
	UINT_8 aucBSSID[6];
	UINT_8 ucWmmIdx;
	UINT_8 ucDtimPeriod;
	UINT_8 ucBcMcWlanidx;	/* indicate which wlan-idx used for MC/BC transmission. */
	UINT_8 ucCipherSuit;
	UINT_8 acuReserve[6];
} CMD_BSSINFO_BASIC_T, *P_CMD_BSSINFO_BASIC_T;

typedef struct _TXBF_PFMU_STA_INFO {
	UINT_16 u2PfmuId;	/* 0xFFFF means no access right for PFMU */
	UINT_8 fgSU_MU;		/* 0 : SU, 1 : MU */
	UINT_8 fgETxBfCap;	/* 0 : ITxBf, 1 : ETxBf */
	UINT_8 ucSoundingPhy;	/* 0: legacy, 1: OFDM, 2: HT, 4: VHT */
	UINT_8 ucNdpaRate;
	UINT_8 ucNdpRate;
	UINT_8 ucReptPollRate;
	UINT_8 ucTxMode;	/* 0: legacy, 1: OFDM, 2: HT, 4: VHT */
	UINT_8 ucNc;
	UINT_8 ucNr;
	UINT_8 ucCBW;		/* 0 : 20M, 1 : 40M, 2 : 80M, 3 : 80 + 80M */
	UINT_8 ucTotMemRequire;
	UINT_8 ucMemRequire20M;
	UINT_8 ucMemRow0;
	UINT_8 ucMemCol0;
	UINT_8 ucMemRow1;
	UINT_8 ucMemCol1;
	UINT_8 ucMemRow2;
	UINT_8 ucMemCol2;
	UINT_8 ucMemRow3;
	UINT_8 ucMemCol3;
	UINT_16 u2SmartAnt;
	UINT_8 ucSEIdx;
	UINT_8 uciBfTimeOut;
	UINT_8 uciBfDBW;
	UINT_8 uciBfNcol;
	UINT_8 uciBfNrow;
	UINT_8 aucReserved[3];
} TXBF_PFMU_STA_INFO, *P_TXBF_PFMU_STA_INFO;

typedef struct _STA_REC_UPD_ENTRY_T {
	TXBF_PFMU_STA_INFO rTxBfPfmuStaInfo;
	UINT_8 aucAddr[PARAM_MAC_ADDR_LEN];
	UINT_8 ucAid;
	UINT_8 ucRsv;
} STA_REC_UPD_ENTRY_T, *P_STA_REC_UPD_ENTRY_T;

typedef struct _STAREC_COMMON_T {
	/* Basic STA record (Group0) */
	UINT_16 u2Tag;		/* Tag = 0x00 */
	UINT_16 u2Length;
	UINT_32 u4ConnectionType;
	UINT_8 ucConnectionState;
	UINT_8 ucIsQBSS;
	UINT_16 u2AID;
	UINT_8 aucPeerMacAddr[6];
	UINT_16 u2Reserve1;
} CMD_STAREC_COMMON_T, *P_CMD_STAREC_COMMON_T;

typedef struct _CMD_STAREC_BF {
	UINT_16 u2Tag;		/* Tag = 0x02 */
	UINT_16 u2Length;
	TXBF_PFMU_STA_INFO rTxBfPfmuInfo;
	UINT_8 ucReserved[3];
} CMD_STAREC_BF, *P_CMD_STAREC_BF;

/* QA tool: maunal assoc */
typedef struct _CMD_MANUAL_ASSOC_STRUCT_T {
/*
*	UINT_8              ucBssIndex;
*	UINT_8              ucWlanIdx;
*	UINT_16             u2TotalElementNum;
*	UINT_32             u4Reserve;
*/
	/* extension */
	UINT_16 u2Tag;		/* Tag = 0x05 */
	UINT_16 u2Length;
	UINT_8 aucMac[MAC_ADDR_LEN];
	UINT_8 ucType;
	UINT_8 ucWtbl;
	UINT_8 ucOwnmac;
	UINT_8 ucMode;
	UINT_8 ucBw;
	UINT_8 ucNss;
	UINT_8 ucPfmuId;
	UINT_8 ucMarate;
	UINT_8 ucSpeIdx;
	UINT_8 ucaid;
} CMD_MANUAL_ASSOC_STRUCT_T, *P_CMD_MANUAL_ASSOC_STRUCT_T;

typedef struct _TX_BF_SOUNDING_START_T {
	union {
		EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL_T rExtCmdExtBfSndPeriodicTriggerCtrl;
		EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL_T rExtCmdExtBfMuSndPeriodicTriggerCtrl;
	} rTxBfSounding;
} TX_BF_SOUNDING_START_T, *P_TX_BF_SOUNDING_START_T;

typedef struct _TX_BF_SOUNDING_STOP_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucSndgStop;
	UINT_8 ucReserved[2];
} TX_BF_SOUNDING_STOP_T, *P_TX_BF_SOUNDING_STOP_T;

typedef struct _TX_BF_TX_APPLY_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucWlanId;
	UINT_8 fgETxBf;
	UINT_8 fgITxBf;
	UINT_8 fgMuTxBf;
	UINT_8 ucReserved[3];
} TX_BF_TX_APPLY_T, *P_TX_BF_TX_APPLY_T;

typedef struct _TX_BF_PFMU_MEM_ALLOC_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucSuMuMode;
	UINT_8 ucWlanIdx;
	UINT_8 ucReserved;
} TX_BF_PFMU_MEM_ALLOC_T, *P_TX_BF_PFMU_MEM_ALLOC_T;

typedef struct _TX_BF_PFMU_MEM_RLS_T {
	UINT_8 ucTxBfCategory;
	UINT_8 ucWlanId;
	UINT_8 ucReserved[2];
} TX_BF_PFMU_MEM_RLS_T, *P_TX_BF_PFMU_MEM_RLS_T;

typedef union _PARAM_CUSTOM_TXBF_ACTION_STRUCT_T {
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
} PARAM_CUSTOM_TXBF_ACTION_STRUCT_T, *P_PARAM_CUSTOM_TXBF_ACTION_STRUCT_T;

typedef struct _PARAM_CUSTOM_STA_REC_UPD_STRUCT_T {
	UINT_8 ucBssIndex;
	UINT_8 ucWlanIdx;
	UINT_16 u2TotalElementNum;
	UINT_8 ucAppendCmdTLV;
	UINT_8 ucMuarIdx;
	UINT_8 aucReserve[2];
	UINT_32 *prStaRec;
	CMD_STAREC_BF rCmdStaRecBf;
} PARAM_CUSTOM_STA_REC_UPD_STRUCT_T, *P_PARAM_CUSTOM_STA_REC_UPD_STRUCT_T;

typedef struct _BSSINFO_ARGUMENT_T {
	UCHAR OwnMacIdx;
	UINT_8 ucBssIndex;
	UINT_8 Bssid[PARAM_MAC_ADDR_LEN];
	UINT_8 ucBcMcWlanIdx;
	UINT_8 ucPeerWlanIdx;
	UINT_32 NetworkType;
	UINT_32 u4ConnectionType;
	UINT_8 CipherSuit;
	UINT_8 Active;
	UINT_8 WmmIdx;
	UINT_32 u4BssInfoFeature;
	UINT_8 aucBuffer[0];
} BSSINFO_ARGUMENT_T, *P_BSSINFO_ARGUMENT_T;

typedef struct _PARAM_CUSTOM_PFMU_TAG_READ_STRUCT_T {
	PFMU_PROFILE_TAG1 ru4TxBfPFMUTag1;
	PFMU_PROFILE_TAG2 ru4TxBfPFMUTag2;
} PARAM_CUSTOM_PFMU_TAG_READ_STRUCT_T, *P_PARAM_CUSTOM_PFMU_TAG_READ_STRUCT_T;

#if CFG_SUPPORT_MU_MIMO
typedef struct _PARAM_CUSTOM_SHOW_GROUP_TBL_ENTRY_STRUCT_T {
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
} PARAM_CUSTOM_SHOW_GROUP_TBL_ENTRY_STRUCT_T, *P_PARAM_CUSTOM_SHOW_GROUP_TBL_ENTRY_STRUCT_T;

typedef struct _PARAM_CUSTOM_GET_QD_STRUCT_T {
	UINT_32 u4EventId;
	UINT_32 au4RawData[14];
} PARAM_CUSTOM_GET_QD_STRUCT_T, *P_PARAM_CUSTOM_GET_QD_STRUCT_T;

typedef struct _MU_STRUCT_LQ_REPORT {
	int lq_report[NUM_OF_USER][NUM_OF_MODUL];
} MU_STRUCT_LQ_REPORT, *P_MU_STRUCT_LQ_REPORT;

typedef struct _PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT_T {
	UINT_32 u4EventId;
	MU_STRUCT_LQ_REPORT rEntry;
} PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT_T, *P_PARAM_CUSTOM_GET_MU_CALC_LQ_STRUCT_T;

typedef struct _MU_GET_CALC_INIT_MCS_T {
	UINT_8 ucgroupIdx;
	UINT_8 ucRsv[3];
} MU_GET_CALC_INIT_MCS_T, *P_MU_GET_CALC_INIT_MCS_T;

typedef struct _MU_SET_INIT_MCS_T {
	UINT_8 ucNumOfUser;	/* zero-base : 0~3: means 1~2 users */
	UINT_8 ucBandwidth;	/* zero-base : 0:20 hz 1:40 hz 2: 80 hz 3: 160 */
	UINT_8 ucNssOfUser0;	/* zero-base : 0~1 means uesr0 use 1~2 ss , if no use keep 0 */
	UINT_8 ucNssOfUser1;	/* zero-base : 0~1 means uesr0 use 1~2 ss , if no use keep 0 */
	UINT_8 ucPfMuIdOfUser0;	/* zero-base : for now, uesr0 use pf mu id 0 */
	UINT_8 ucPfMuIdOfUser1;	/* zero-base : for now, uesr1 use pf mu id 1 */
	UINT_8 ucNumOfTxer;	/* 0~3: mean use 1~4 anntain, for now, should fix 3 */
	UINT_8 ucSpeIndex;	/*add new field to fill special extension index which replace reserve */
	UINT_32 u4GroupIndex;	/* 0~ :the index of group table entry for calculation */
} MU_SET_INIT_MCS_T, *P_MU_SET_INIT_MCS_T;

typedef struct _MU_SET_CALC_LQ_T {
	UINT_8 ucNumOfUser;	/* zero-base : 0~3: means 1~2 users */
	UINT_8 ucBandwidth;	/* zero-base : 0:20 hz 1:40 hz 2: 80 hz 3: 160 */
	UINT_8 ucNssOfUser0;	/* zero-base : 0~1 means uesr0 use 1~2 ss , if no use keep 0 */
	UINT_8 ucNssOfUser1;	/* zero-base : 0~1 means uesr0 use 1~2 ss , if no use keep 0 */
	UINT_8 ucPfMuIdOfUser0;	/* zero-base : for now, uesr0 use pf mu id 0 */
	UINT_8 ucPfMuIdOfUser1;	/* zero-base : for now, uesr1 use pf mu id 1 */
	UINT_8 ucNumOfTxer;	/* 0~3: mean use 1~4 anntain, for now, should fix 3 */
	UINT_8 ucSpeIndex;	/*add new field to fill special extension index which replace reserve */
	UINT_32 u4GroupIndex;	/* 0~ :the index of group table entry for calculation */
} MU_SET_CALC_LQ_T, *P_MU_SET_CALC_LQ_T;

typedef struct _MU_GET_LQ_T {
	UINT_8 ucType;
	UINT_8 ucRsv[3];
} MU_GET_LQ_T, *P_MU_GET_LQ_T;

typedef struct _MU_SET_SNR_OFFSET_T {
	UINT_8 ucVal;
	UINT_8 ucRsv[3];
} MU_SET_SNR_OFFSET_T, *P_MU_SET_SNR_OFFSET_T;

typedef struct _MU_SET_ZERO_NSS_T {
	UINT_8 ucVal;
	UINT_8 ucRsv[3];
} MU_SET_ZERO_NSS_T, *P_MU_SET_ZERO_NSS_T;

typedef struct _MU_SPEED_UP_LQ_T {
	UINT_32 u4Val;
} MU_SPEED_UP_LQ_T, *P_MU_SPEED_UP_LQ_T;

typedef struct _MU_SET_MU_TABLE_T {
	/* UINT_16  u2Type; */
	/* UINT_32  u4Length; */
	UINT_8 aucMetricTable[NUM_MUT_NR_NUM * NUM_MUT_FEC * NUM_MUT_MCS * NUM_MUT_INDEX];
} MU_SET_MU_TABLE_T, *P_MU_SET_MU_TABLE_T;

typedef struct _MU_SET_GROUP_T {
	UINT_32 u4GroupIndex;	/* Group Table Idx */
	UINT_32 u4NumOfUser;
	UINT_32 u4User0Ldpc;
	UINT_32 u4User1Ldpc;
	UINT_32 u4ShortGI;
	UINT_32 u4Bw;
	UINT_32 u4User0Nss;
	UINT_32 u4User1Nss;
	UINT_32 u4GroupId;
	UINT_32 u4User0UP;
	UINT_32 u4User1UP;
	UINT_32 u4User0MuPfId;
	UINT_32 u4User1MuPfId;
	UINT_32 u4User0InitMCS;
	UINT_32 u4User1InitMCS;
	UINT_8 aucUser0MacAddr[PARAM_MAC_ADDR_LEN];
	UINT_8 aucUser1MacAddr[PARAM_MAC_ADDR_LEN];
} MU_SET_GROUP_T, *P_MU_SET_GROUP_T;

typedef struct _MU_GET_QD_T {
	UINT_8 ucSubcarrierIndex;
	/* UINT_32 u4Length; */
	/* UINT_8 *prQd; */
} MU_GET_QD_T, *P_MU_GET_QD_T;

typedef struct _MU_SET_ENABLE_T {
	UINT_8 ucVal;
	UINT_8 ucRsv[3];
} MU_SET_ENABLE_T, *P_MU_SET_ENABLE_T;

typedef struct _MU_SET_GID_UP_T {
	UINT_32 au4Gid[2];
	UINT_32 au4Up[4];
} MU_SET_GID_UP_T, *P_MU_SET_GID_UP_T;

typedef struct _MU_TRIGGER_MU_TX_T {
	UINT_8  fgIsRandomPattern; /* is random pattern or not */
	UINT_32 u4MsduPayloadLength0; /* payload length of the MSDU for user 0 */
	UINT_32 u4MsduPayloadLength1; /* payload length of the MSDU for user 1 */
	UINT_32 u4MuPacketCount; /* MU TX count */
	UINT_32 u4NumOfSTAs; /* number of user in the MU TX */
	UINT_8   aucMacAddrs[2][6]; /* MAC address of users*/
} MU_TRIGGER_MU_TX_T, *P_MU_TRIGGER_MU_TX_T;

typedef struct _PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T {
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
} PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T, *P_PARAM_CUSTOM_MUMIMO_ACTION_STRUCT_T;
#endif /* CFG_SUPPORT_MU_MIMO */
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

typedef struct _PARAM_CUSTOM_MEM_DUMP_STRUCT_T {
	UINT_32 u4Address;
	UINT_32 u4Length;
	UINT_32 u4RemainLength;
#if CFG_SUPPORT_QA_TOOL
	UINT_32 u4IcapContent;
#endif				/* CFG_SUPPORT_QA_TOOL */
	UINT_8 ucFragNum;
} PARAM_CUSTOM_MEM_DUMP_STRUCT_T, *P_PARAM_CUSTOM_MEM_DUMP_STRUCT_T;

typedef struct _PARAM_CUSTOM_SW_CTRL_STRUCT_T {
	UINT_32 u4Id;
	UINT_32 u4Data;
} PARAM_CUSTOM_SW_CTRL_STRUCT_T, *P_PARAM_CUSTOM_SW_CTRL_STRUCT_T;

typedef struct _PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T {
	UINT_16 u2Id;
	UINT_8 ucType;
	UINT_8 ucRespType;
	UINT_16 u2MsgSize;
	UINT_8 aucReserved0[2];
	UINT_8 aucCmd[CHIP_CONFIG_RESP_SIZE];
} PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T, *P_PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T;

typedef struct _PARAM_CUSTOM_KEY_CFG_STRUCT_T {
	UINT_8 aucKey[WLAN_CFG_KEY_LEN_MAX];
	UINT_8 aucValue[WLAN_CFG_VALUE_LEN_MAX];
} PARAM_CUSTOM_KEY_CFG_STRUCT_T, *P_PARAM_CUSTOM_KEY_CFG_STRUCT_T;

typedef struct _PARAM_CUSTOM_EEPROM_RW_STRUCT_T {
	UINT_8 ucEepromMethod;	/* For read only read: 1, query size: 0 */
	UINT_8 ucEepromIndex;
	UINT_8 reserved;
	UINT_16 u2EepromData;
} PARAM_CUSTOM_EEPROM_RW_STRUCT_T, *P_PARAM_CUSTOM_EEPROM_RW_STRUCT_T,
PARAM_CUSTOM_NVRAM_RW_STRUCT_T, *P_PARAM_CUSTOM_NVRAM_RW_STRUCT_T;

typedef struct _PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T {
	UINT_8 bmfgApsdEnAc;	/* b0~3: trigger-en AC0~3. b4~7: delivery-en AC0~3 */
	UINT_8 ucIsEnterPsAtOnce;	/* enter PS immediately without 5 second guard after connected */
	UINT_8 ucIsDisableUcTrigger;	/* not to trigger UC on beacon TIM is matched (under U-APSD) */
	UINT_8 reserved;
} PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T, *P_PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T;

typedef struct _PARAM_CUSTOM_NOA_PARAM_STRUCT_T {
	UINT_32 u4NoaDurationMs;
	UINT_32 u4NoaIntervalMs;
	UINT_32 u4NoaCount;
	UINT_8 ucBssIdx;
} PARAM_CUSTOM_NOA_PARAM_STRUCT_T, *P_PARAM_CUSTOM_NOA_PARAM_STRUCT_T;

typedef struct _PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T {
	UINT_8 ucBssIdx;
	UINT_8 ucLegcyPS;
	UINT_8 ucOppPs;
	UINT_8 aucResv[1];
	UINT_32 u4CTwindowMs;
} PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T, *P_PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T;

typedef struct _PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T {
	UINT_8 ucBssIdx;
	UINT_8 fgEnAPSD;
	UINT_8 fgEnAPSD_AcBe;
	UINT_8 fgEnAPSD_AcBk;
	UINT_8 fgEnAPSD_AcVo;
	UINT_8 fgEnAPSD_AcVi;
	UINT_8 ucMaxSpLen;
	UINT_8 aucResv[2];
} PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T, *P_PARAM_CUSTOM_UAPSD_PARAM_STRUCT_T;

typedef struct _PARAM_CUSTOM_P2P_SET_STRUCT_T {
	UINT_32 u4Enable;
	UINT_32 u4Mode;
} PARAM_CUSTOM_P2P_SET_STRUCT_T, *P_PARAM_CUSTOM_P2P_SET_STRUCT_T;

#define MAX_NUMBER_OF_ACL 20

typedef enum _ENUM_PARAM_CUSTOM_ACL_POLICY_T {
	PARAM_CUSTOM_ACL_POLICY_DISABLE,
	PARAM_CUSTOM_ACL_POLICY_ACCEPT,
	PARAM_CUSTOM_ACL_POLICY_DENY,
	PARAM_CUSTOM_ACL_POLICY_NUM
} ENUM_PARAM_CUSTOM_ACL_POLICY_T, *P_ENUM_PARAM_CUSTOM_ACL_POLICY_T;

typedef struct _PARAM_CUSTOM_ACL_ENTRY {
	UINT_8 aucAddr[MAC_ADDR_LEN];
	UINT_16 u2Rsv;
} PARAM_CUSTOM_ACL_ENTRY, *PPARAM_CUSTOM_ACL_ENTRY;

typedef struct _PARAM_CUSTOM_ACL {
	ENUM_PARAM_CUSTOM_ACL_POLICY_T ePolicy;
	UINT_32 u4Num;
	PARAM_CUSTOM_ACL_ENTRY rEntry[MAX_NUMBER_OF_ACL];
} PARAM_CUSTOM_ACL, *PPARAM_CUSTOM_ACL;

typedef enum _ENUM_CFG_SRC_TYPE_T {
	CFG_SRC_TYPE_EEPROM,
	CFG_SRC_TYPE_NVRAM,
	CFG_SRC_TYPE_UNKNOWN,
	CFG_SRC_TYPE_NUM
} ENUM_CFG_SRC_TYPE_T, *P_ENUM_CFG_SRC_TYPE_T;

typedef enum _ENUM_EEPROM_TYPE_T {
	EEPROM_TYPE_NO,
	EEPROM_TYPE_PRESENT,
	EEPROM_TYPE_NUM
} ENUM_EEPROM_TYPE_T, *P_ENUM_EEPROM_TYPE_T;

typedef struct _PARAM_QOS_TSINFO {
	UINT_8 ucTrafficType;	/* Traffic Type: 1 for isochronous 0 for asynchronous */
	UINT_8 ucTid;		/* TSID: must be between 8 ~ 15 */
	UINT_8 ucDirection;	/* direction */
	UINT_8 ucAccessPolicy;	/* access policy */
	UINT_8 ucAggregation;	/* aggregation */
	UINT_8 ucApsd;		/* APSD */
	UINT_8 ucuserPriority;	/* user priority */
	UINT_8 ucTsInfoAckPolicy;	/* TSINFO ACK policy */
	UINT_8 ucSchedule;	/* Schedule */
} PARAM_QOS_TSINFO, *P_PARAM_QOS_TSINFO;

typedef struct _PARAM_QOS_TSPEC {
	PARAM_QOS_TSINFO rTsInfo;	/* TS info field */
	UINT_16 u2NominalMSDUSize;	/* nominal MSDU size */
	UINT_16 u2MaxMSDUsize;	/* maximum MSDU size */
	UINT_32 u4MinSvcIntv;	/* minimum service interval */
	UINT_32 u4MaxSvcIntv;	/* maximum service interval */
	UINT_32 u4InactIntv;	/* inactivity interval */
	UINT_32 u4SpsIntv;	/* suspension interval */
	UINT_32 u4SvcStartTime;	/* service start time */
	UINT_32 u4MinDataRate;	/* minimum Data rate */
	UINT_32 u4MeanDataRate;	/* mean data rate */
	UINT_32 u4PeakDataRate;	/* peak data rate */
	UINT_32 u4MaxBurstSize;	/* maximum burst size */
	UINT_32 u4DelayBound;	/* delay bound */
	UINT_32 u4MinPHYRate;	/* minimum PHY rate */
	UINT_16 u2Sba;		/* surplus bandwidth allowance */
	UINT_16 u2MediumTime;	/* medium time */
} PARAM_QOS_TSPEC, *P_PARAM_QOS_TSPEC;

typedef struct _PARAM_QOS_ADDTS_REQ_INFO {
	PARAM_QOS_TSPEC rTspec;
} PARAM_QOS_ADDTS_REQ_INFO, *P_PARAM_QOS_ADDTS_REQ_INFO;

typedef struct _PARAM_VOIP_CONFIG {
	UINT_32 u4VoipTrafficInterval;	/* 0: disable VOIP configuration */
} PARAM_VOIP_CONFIG, *P_PARAM_VOIP_CONFIG;

/*802.11 Statistics Struct*/
typedef struct _PARAM_802_11_STATISTICS_STRUCT_T {
	UINT_32 u4Length;	/* Length of structure */
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
	LARGE_INTEGER rTKIPLocalMICFailures;
	LARGE_INTEGER rTKIPICVErrors;
	LARGE_INTEGER rTKIPCounterMeasuresInvoked;
	LARGE_INTEGER rTKIPReplays;
	LARGE_INTEGER rCCMPFormatErrors;
	LARGE_INTEGER rCCMPReplays;
	LARGE_INTEGER rCCMPDecryptErrors;
	LARGE_INTEGER rFourWayHandshakeFailures;
	LARGE_INTEGER rWEPUndecryptableCount;
	LARGE_INTEGER rWEPICVErrorCount;
	LARGE_INTEGER rDecryptSuccessCount;
	LARGE_INTEGER rDecryptFailureCount;
} PARAM_802_11_STATISTICS_STRUCT_T, *P_PARAM_802_11_STATISTICS_STRUCT_T;

/* Linux Network Device Statistics Struct */
typedef struct _PARAM_LINUX_NETDEV_STATISTICS_T {
	UINT_32 u4RxPackets;
	UINT_32 u4TxPackets;
	UINT_32 u4RxBytes;
	UINT_32 u4TxBytes;
	UINT_32 u4RxErrors;
	UINT_32 u4TxErrors;
	UINT_32 u4Multicast;
} PARAM_LINUX_NETDEV_STATISTICS_T, *P_PARAM_LINUX_NETDEV_STATISTICS_T;

typedef struct _PARAM_MTK_WIFI_TEST_STRUCT_T {
	UINT_32 u4FuncIndex;
	UINT_32 u4FuncData;
} PARAM_MTK_WIFI_TEST_STRUCT_T, *P_PARAM_MTK_WIFI_TEST_STRUCT_T;

/* 802.11 Media stream constraints */
typedef enum _ENUM_MEDIA_STREAM_MODE {
	ENUM_MEDIA_STREAM_OFF,
	ENUM_MEDIA_STREAM_ON
} ENUM_MEDIA_STREAM_MODE, *P_ENUM_MEDIA_STREAM_MODE;

/* for NDIS 5.1 Media Streaming Change */
typedef struct _PARAM_MEDIA_STREAMING_INDICATION {
	PARAM_STATUS_INDICATION_T rStatus;
	ENUM_MEDIA_STREAM_MODE eMediaStreamMode;
} PARAM_MEDIA_STREAMING_INDICATION, *P_PARAM_MEDIA_STREAMING_INDICATION;

#define PARAM_PROTOCOL_ID_DEFAULT       0x00
#define PARAM_PROTOCOL_ID_TCP_IP        0x02
#define PARAM_PROTOCOL_ID_IPX           0x06
#define PARAM_PROTOCOL_ID_NBF           0x07
#define PARAM_PROTOCOL_ID_MAX           0x0F
#define PARAM_PROTOCOL_ID_MASK          0x0F

/* for NDIS OID_GEN_NETWORK_LAYER_ADDRESSES */
typedef struct _PARAM_NETWORK_ADDRESS_IP {
	UINT_16 sin_port;
	UINT_32 in_addr;
	UINT_8 sin_zero[8];
} PARAM_NETWORK_ADDRESS_IP, *P_PARAM_NETWORK_ADDRESS_IP;

typedef struct _PARAM_NETWORK_ADDRESS {
	UINT_16 u2AddressLength;	/* length in bytes of Address[] in this */
	UINT_16 u2AddressType;	/* type of this address (PARAM_PROTOCOL_ID_XXX above) */
	UINT_8 aucAddress[1];	/* actually AddressLength bytes long */
} PARAM_NETWORK_ADDRESS, *P_PARAM_NETWORK_ADDRESS;

/* The following is used with OID_GEN_NETWORK_LAYER_ADDRESSES to set network layer addresses on an interface */

typedef struct _PARAM_NETWORK_ADDRESS_LIST {
	UINT_8 ucBssIdx;
	UINT_32 u4AddressCount;	/* number of addresses following */
	UINT_16 u2AddressType;	/* type of this address (NDIS_PROTOCOL_ID_XXX above) */
	PARAM_NETWORK_ADDRESS arAddress[1];	/* actually AddressCount elements long */
} PARAM_NETWORK_ADDRESS_LIST, *P_PARAM_NETWORK_ADDRESS_LIST;

#if CFG_SLT_SUPPORT

#define FIXED_BW_LG20       0x0000
#define FIXED_BW_UL20       0x2000
#define FIXED_BW_DL40       0x3000

#define FIXED_EXT_CHNL_U20  0x4000	/* For AGG register. */
#define FIXED_EXT_CHNL_L20  0xC000	/* For AGG regsiter. */

typedef enum _ENUM_MTK_LP_TEST_MODE_T {
	ENUM_MTK_LP_TEST_NORMAL,
	ENUM_MTK_LP_TEST_GOLDEN_SAMPLE,
	ENUM_MTK_LP_TEST_DUT,
	ENUM_MTK_LP_TEST_MODE_NUM
} ENUM_MTK_LP_TEST_MODE_T, *P_ENUM_MTK_LP_TEST_MODE_T;

typedef enum _ENUM_MTK_SLT_FUNC_IDX_T {
	ENUM_MTK_SLT_FUNC_DO_NOTHING,
	ENUM_MTK_SLT_FUNC_INITIAL,
	ENUM_MTK_SLT_FUNC_RATE_SET,
	ENUM_MTK_SLT_FUNC_LP_SET,
	ENUM_MTK_SLT_FUNC_NUM
} ENUM_MTK_SLT_FUNC_IDX_T, *P_ENUM_MTK_SLT_FUNC_IDX_T;

typedef struct _PARAM_MTK_SLT_LP_TEST_STRUCT_T {
	ENUM_MTK_LP_TEST_MODE_T rLpTestMode;
	UINT_32 u4BcnRcvNum;
} PARAM_MTK_SLT_LP_TEST_STRUCT_T, *P_PARAM_MTK_SLT_LP_TEST_STRUCT_T;

typedef struct _PARAM_MTK_SLT_TR_TEST_STRUCT_T {
	ENUM_PARAM_NETWORK_TYPE_T rNetworkType;	/* Network Type OFDM5G or OFDM2.4G */
	UINT_32 u4FixedRate;	/* Fixed Rate including BW */
} PARAM_MTK_SLT_TR_TEST_STRUCT_T, *P_PARAM_MTK_SLT_TR_TEST_STRUCT_T;

typedef struct _PARAM_MTK_SLT_INITIAL_STRUCT_T {
	UINT_8 aucTargetMacAddr[PARAM_MAC_ADDR_LEN];
	UINT_16 u2SiteID;
} PARAM_MTK_SLT_INITIAL_STRUCT_T, *P_PARAM_MTK_SLT_INITIAL_STRUCT_T;

typedef struct _PARAM_MTK_SLT_TEST_STRUCT_T {
	ENUM_MTK_SLT_FUNC_IDX_T rSltFuncIdx;
	UINT_32 u4Length;	/* Length of structure, */
				/* including myself */
	UINT_32 u4FuncInfoLen;	/* Include following content */
				/* field and myself */
	union {
		PARAM_MTK_SLT_INITIAL_STRUCT_T rMtkInitTest;
		PARAM_MTK_SLT_LP_TEST_STRUCT_T rMtkLpTest;
		PARAM_MTK_SLT_TR_TEST_STRUCT_T rMtkTRTest;
	} unFuncInfoContent;

} PARAM_MTK_SLT_TEST_STRUCT_T, *P_PARAM_MTK_SLT_TEST_STRUCT_T;

#endif

#if CFG_SUPPORT_MSP
/* Should by chip */
typedef struct _PARAM_SEC_CONFIG_T {
	BOOL fgWPIFlag;
	BOOL fgRV;
	BOOL fgIKV;
	BOOL fgRKV;

	BOOL fgRCID;
	BOOL fgRCA1;
	BOOL fgRCA2;
	BOOL fgEvenPN;

	UINT_8 ucKeyID;
	UINT_8 ucMUARIdx;
	UINT_8 ucCipherSuit;
	UINT_8 aucReserved[1];
} PARAM_SEC_CONFIG_T, *P_PARAM_SEC_CONFIG_T;

typedef struct _PARAM_TX_CONFIG_T {
	UINT_8 aucPA[6];
	BOOL fgSW;
	BOOL fgDisRxHdrTran;

	BOOL fgAADOM;
	UINT_8 ucPFMUIdx;
	UINT_16 u2PartialAID;

	BOOL fgTIBF;
	BOOL fgTEBF;
	BOOL fgIsHT;
	BOOL fgIsVHT;

	BOOL fgMesh;
	BOOL fgBAFEn;
	BOOL fgCFAck;
	BOOL fgRdgBA;

	BOOL fgRDG;
	BOOL fgIsPwrMgt;
	BOOL fgRTS;
	BOOL fgSMPS;

	BOOL fgTxopPS;
	BOOL fgDonotUpdateIPSM;
	BOOL fgSkipTx;
	BOOL fgLDPC;

	BOOL fgIsQoS;
	BOOL fgIsFromDS;
	BOOL fgIsToDS;
	BOOL fgDynBw;

	BOOL fgIsAMSDUCrossLG;
	BOOL fgCheckPER;
	BOOL fgIsGID63;
	UINT_8 aucReserved[1];

#if (1 /* CFG_SUPPORT_VHT == 1 */)
	BOOL fgVhtTIBF;
	BOOL fgVhtTEBF;
	BOOL fgVhtLDPC;
	UINT_8 aucReserved2[1];
#endif
} PARAM_TX_CONFIG_T, *P_PARAM_TX_CONFIG_T;

typedef struct _PARAM_KEY_CONFIG_T {
	UINT_8                 aucKey[32];
} PARAM_KEY_CONFIG_T, *P_PARAM_KEY_CONFIG_T;

typedef struct _PARAM_PEER_RATE_INFO_T {
	UINT_8                 ucCounterMPDUFail;
	UINT_8                 ucCounterMPDUTx;
	UINT_8                 ucRateIdx;
	UINT_8                 ucReserved[1];

	UINT_16                au2RateCode[AUTO_RATE_NUM];
} PARAM_PEER_RATE_INFO_T, *P_PARAM_PEER_RATE_INFO_T;

typedef struct _PARAM_PEER_BA_CONFIG_T {
	UINT_8			ucBaEn;
	UINT_8			ucRsv[3];
	UINT_32			u4BaWinSize;
} PARAM_PEER_BA_CONFIG_T, *P_PARAM_PEER_BA_CONFIG_T;

typedef struct _PARAM_ANT_ID_CONFIG_T {
	UINT_8			ucANTIDSts0;
	UINT_8			ucANTIDSts1;
	UINT_8			ucANTIDSts2;
	UINT_8			ucANTIDSts3;
} PARAM_ANT_ID_CONFIG_T, *P_PARAM_ANT_ID_CONFIG_T;

typedef struct _PARAM_PEER_CAP_T {
	PARAM_ANT_ID_CONFIG_T	rAntIDConfig;

	UINT_8			ucTxPowerOffset;
	UINT_8			ucCounterBWSelector;
	UINT_8			ucChangeBWAfterRateN;
	UINT_8			ucFrequencyCapability;
	UINT_8			ucSpatialExtensionIndex;

	BOOL			fgG2;
	BOOL			fgG4;
	BOOL			fgG8;
	BOOL			fgG16;

	UINT_8			ucMMSS;
	UINT_8			ucAmpduFactor;
	UINT_8			ucReserved[1];
} PARAM_PEER_CAP_T, *P_PARAM_PEER_CAP_T;

typedef struct _PARAM_PEER_RX_COUNTER_ALL_T {
	UINT_8			ucRxRcpi0;
	UINT_8			ucRxRcpi1;
	UINT_8			ucRxRcpi2;
	UINT_8			ucRxRcpi3;

	UINT_8			ucRxCC0;
	UINT_8			ucRxCC1;
	UINT_8			ucRxCC2;
	UINT_8			ucRxCC3;

	BOOL			fgRxCCSel;
	UINT_8			ucCeRmsd;
	UINT_8			aucReserved[2];
} PARAM_PEER_RX_COUNTER_ALL_T, *P_PARAM_PEER_RX_COUNTER_ALL_T;

typedef struct _PARAM_PEER_TX_COUNTER_ALL_T {
	UINT_16 u2Rate1TxCnt;
	UINT_16 u2Rate1FailCnt;
	UINT_16 u2Rate2OkCnt;
	UINT_16 u2Rate3OkCnt;
	UINT_16 u2CurBwTxCnt;
	UINT_16 u2CurBwFailCnt;
	UINT_16 u2OtherBwTxCnt;
	UINT_16 u2OtherBwFailCnt;
} PARAM_PEER_TX_COUNTER_ALL_T, *P_PARAM_PEER_TX_COUNTER_ALL_T;

typedef struct _PARAM_HW_WLAN_INFO_T {
	UINT_32			u4Index;
	PARAM_TX_CONFIG_T	rWtblTxConfig;
	PARAM_SEC_CONFIG_T	rWtblSecConfig;
	PARAM_KEY_CONFIG_T	rWtblKeyConfig;
	PARAM_PEER_RATE_INFO_T	rWtblRateInfo;
	PARAM_PEER_BA_CONFIG_T  rWtblBaConfig;
	PARAM_PEER_CAP_T	rWtblPeerCap;
	PARAM_PEER_RX_COUNTER_ALL_T rWtblRxCounter;
	PARAM_PEER_TX_COUNTER_ALL_T rWtblTxCounter;
} PARAM_HW_WLAN_INFO_T, *P_PARAM_HW_WLAN_INFO_T;

typedef struct _HW_TX_AMPDU_METRICS_T {
	UINT_32 u4TxSfCnt;
	UINT_32 u4TxAckSfCnt;
	UINT_32 u2TxAmpduCnt;
	UINT_32 u2TxRspBaCnt;
	UINT_16 u2TxEarlyStopCnt;
	UINT_16 u2TxRange1AmpduCnt;
	UINT_16 u2TxRange2AmpduCnt;
	UINT_16 u2TxRange3AmpduCnt;
	UINT_16 u2TxRange4AmpduCnt;
	UINT_16 u2TxRange5AmpduCnt;
	UINT_16 u2TxRange6AmpduCnt;
	UINT_16 u2TxRange7AmpduCnt;
	UINT_16 u2TxRange8AmpduCnt;
	UINT_16 u2TxRange9AmpduCnt;
} HW_TX_AMPDU_METRICS_T, *P_HW_TX_AMPDU_METRICS_T;

typedef struct _HW_MIB_COUNTER_T {
	UINT_32 u4RxFcsErrCnt;
	UINT_32 u4RxFifoFullCnt;
	UINT_32 u4RxMpduCnt;
	UINT_32 u4RxAMPDUCnt;
	UINT_32 u4RxTotalByte;
	UINT_32 u4RxValidAMPDUSF;
	UINT_32 u4RxValidByte;
	UINT_32 u4ChannelIdleCnt;
	UINT_32 u4RxVectorDropCnt;
	UINT_32 u4DelimiterFailedCnt;
	UINT_32 u4RxVectorMismatchCnt;
	UINT_32 u4MdrdyCnt;
	UINT_32 u4CCKMdrdyCnt;
	UINT_32 u4OFDMLGMixMdrdy;
	UINT_32 u4OFDMGreenMdrdy;
	UINT_32 u4PFDropCnt;
	UINT_32 u4RxLenMismatchCnt;
	UINT_32 u4PCcaTime;
	UINT_32 u4SCcaTime;
	UINT_32 u4CcaNavTx;
	UINT_32 u4PEDTime;
	UINT_32 u4BeaconTxCnt;
	UINT_32 au4BaMissedCnt[BSSID_NUM];
	UINT_32 au4RtsTxCnt[BSSID_NUM];
	UINT_32 au4FrameRetryCnt[BSSID_NUM];
	UINT_32 au4FrameRetry2Cnt[BSSID_NUM];
	UINT_32 au4RtsRetryCnt[BSSID_NUM];
	UINT_32 au4AckFailedCnt[BSSID_NUM];
} HW_MIB_COUNTER_T, *P_HW_MIB_COUNTER_T;

typedef struct _HW_MIB2_COUNTER_T {
	UINT_32 u4Tx40MHzCnt;
	UINT_32 u4Tx80MHzCnt;
	UINT_32 u4Tx160MHzCnt;
} HW_MIB2_COUNTER_T, *P_HW_MIB2_COUNTER_T;

typedef struct _PARAM_HW_MIB_INFO_T {
	UINT_32			u4Index;
	HW_MIB_COUNTER_T	rHwMibCnt;
	HW_MIB2_COUNTER_T	rHwMib2Cnt;
	HW_TX_AMPDU_METRICS_T	rHwTxAmpduMts;
} PARAM_HW_MIB_INFO_T, *P_PARAM_HW_MIB_INFO_T;
#endif

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
struct PARAM_TX_MCS_INFO {
	UINT_8		ucStaIndex;
	UINT_16		au2TxRateCode[MCS_INFO_SAMPLE_CNT];
	UINT_8		aucTxRatePer[MCS_INFO_SAMPLE_CNT];
};
#endif

struct PARAM_CMD_GET_TXPWR_TBL {
	UINT_8 ucDbdcIdx;
	UINT_8 ucCenterCh;
	struct POWER_LIMIT tx_pwr_tbl[TXPWR_TBL_NUM];
};

enum ENUM_TXPWR_TYPE {
	DSSS = 0,
	OFDM_24G,
	OFDM_5G,
	HT20,
	HT40,
	VHT20,
	VHT40,
	VHT80,
	TXPWR_TYPE_NUM,
};

enum ENUM_STREAM_MODE {
	STREAM_SISO,
	STREAM_CDD,
	STREAM_MIMO,
	STREAM_NUM
};

struct txpwr_table_entry {
	char mcs[STREAM_NUM][8];
	unsigned int idx;
};

struct txpwr_table {
	char phy_mode[8];
	struct txpwr_table_entry *tables;
	int n_tables;
};

/*--------------------------------------------------------------*/
/*! \brief For Fixed Rate Configuration (Registry)              */
/*--------------------------------------------------------------*/
typedef enum _ENUM_REGISTRY_FIXED_RATE_T {
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
} ENUM_REGISTRY_FIXED_RATE_T, *P_ENUM_REGISTRY_FIXED_RATE_T;

typedef enum _ENUM_BT_CMD_T {
	BT_CMD_PROFILE = 0,
	BT_CMD_UPDATE,
	BT_CMD_NUM
} ENUM_BT_CMD_T;

typedef enum _ENUM_BT_PROFILE_T {
	BT_PROFILE_CUSTOM = 0,
	BT_PROFILE_SCO,
	BT_PROFILE_ACL,
	BT_PROFILE_MIXED,
	BT_PROFILE_NO_CONNECTION,
	BT_PROFILE_NUM
} ENUM_BT_PROFILE_T;

typedef struct _PTA_PROFILE_T {
	ENUM_BT_PROFILE_T eBtProfile;
	union {
		UINT_8 aucBTPParams[BT_PROFILE_PARAM_LEN];
		/*  0: sco reserved slot time,
		*  1: sco idle slot time,
		*  2: acl throughput,
		*   3: bt tx power,
		*   4: bt rssi
		*   5: VoIP interval
		*   6: BIT(0) Use this field, BIT(1) 0 apply single/ 1 dual PTA setting.
		*/
		UINT_32 au4Btcr[4];
	} u;
} PTA_PROFILE_T, *P_PTA_PROFILE_T;

typedef struct _PTA_IPC_T {
	UINT_8 ucCmd;
	UINT_8 ucLen;
	union {
		PTA_PROFILE_T rProfile;
		UINT_8 aucBTPParams[BT_PROFILE_PARAM_LEN];
	} u;
} PARAM_PTA_IPC_T, *P_PARAM_PTA_IPC_T, PTA_IPC_T, *P_PTA_IPC_T;

/*--------------------------------------------------------------*/
/*! \brief CFG80211 Scan Request Container                      */
/*--------------------------------------------------------------*/
typedef struct _PARAM_SCAN_REQUEST_EXT_T {
	PARAM_SSID_T rSsid;
	UINT_32 u4IELength;
	PUINT_8 pucIE;
} PARAM_SCAN_REQUEST_EXT_T, *P_PARAM_SCAN_REQUEST_EXT_T;

typedef struct _PARAM_SCAN_REQUEST_ADV_T {
	UINT_32 u4SsidNum;
	PARAM_SSID_T rSsid[CFG_SCAN_SSID_MAX_NUM];
	UINT_32 u4IELength;
	PUINT_8 pucIE;
#if CFG_SCAN_CHANNEL_SPECIFIED
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
#endif
} PARAM_SCAN_REQUEST_ADV_T, *P_PARAM_SCAN_REQUEST_ADV_T;

/*--------------------------------------------------------------*/
/*! \brief CFG80211 Scheduled Scan Request Container            */
/*--------------------------------------------------------------*/
typedef struct _PARAM_SCHED_SCAN_REQUEST_T {
	UINT_32 u4SsidNum;
	PARAM_SSID_T arSsid[CFG_SCAN_SSID_MATCH_MAX_NUM];
	UINT_32 u4IELength;
	PUINT_8 pucIE;
	UINT_16 u2ScanInterval;	/* in milliseconds */
} PARAM_SCHED_SCAN_REQUEST, *P_PARAM_SCHED_SCAN_REQUEST;

#if CFG_SUPPORT_PASSPOINT
typedef struct _PARAM_HS20_SET_BSSID_POOL {
	BOOLEAN fgIsEnable;
	UINT_8 ucNumBssidPool;
	PARAM_MAC_ADDRESS arBSSID[8];
} PARAM_HS20_SET_BSSID_POOL, *P_PARAM_HS20_SET_BSSID_POOL;

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_SNIFFER
typedef struct _PARAM_CUSTOM_MONITOR_SET_STRUCT_T {
	UINT_8 ucEnable;
	UINT_8 ucBand;
	UINT_8 ucPriChannel;
	UINT_8 ucSco;
	UINT_8 ucChannelWidth;
	UINT_8 ucChannelS1;
	UINT_8 ucChannelS2;
	UINT_8 aucResv[9];
} PARAM_CUSTOM_MONITOR_SET_STRUCT_T, *P_PARAM_CUSTOM_MONITOR_SET_STRUCT_T;
#endif

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
/*--------------------------------------------------------------*/
/*! \brief MTK Auto Channel Selection related Container         */
/*--------------------------------------------------------------*/
typedef struct _LTE_SAFE_CHN_INFO_T {
	UINT_32 au4SafeChannelBitmask[5]; /* NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_MAX */
} LTE_SAFE_CHN_INFO_T, *P_CMD_LTE_SAFE_CHN_INFO_T;

typedef struct _PARAM_CHN_LOAD_INFO {
	/* Per-CHN Load */
	UINT_8 ucChannel;
	UINT_16 u2APNum;
	UINT_32 u4Dirtiness;
	UINT_8 ucReserved;
} PARAM_CHN_LOAD_INFO, *P_PARAM_CHN_LOAD_INFO;

typedef struct _PARAM_CHN_RANK_INFO {
	UINT_8 ucChannel;
	UINT_32 u4Dirtiness;
	UINT_8 ucReserved;
} PARAM_CHN_RANK_INFO, *P_PARAM_CHN_RANK_INFO;

typedef struct _PARAM_GET_CHN_INFO {
	LTE_SAFE_CHN_INFO_T rLteSafeChnList;
	PARAM_CHN_LOAD_INFO rEachChnLoad[MAX_CHN_NUM];
	BOOLEAN fgDataReadyBit;
	PARAM_CHN_RANK_INFO rChnRankList[MAX_CHN_NUM];
	UINT_8 aucReserved[3];
} PARAM_GET_CHN_INFO, *P_PARAM_GET_CHN_INFO;

typedef struct _PARAM_PREFER_CHN_INFO {
	UINT_8 ucChannel;
	UINT_16 u2APNumScore;
	UINT_8 ucReserved;
} PARAM_PREFER_CHN_INFO, *P_PARAM_PREFER_CHN_INFO;
#endif


typedef struct _UMAC_STAT2_GET_T {
	UINT_16	u2PleRevPgHif0Group0;
	UINT_16	u2PleRevPgCpuGroup2;

	UINT_16	u2PseRevPgHif0Group0;
	UINT_16	u2PseRevPgHif1Group1;
	UINT_16	u2PseRevPgCpuGroup2;
	UINT_16	u2PseRevPgLmac0Group3;
	UINT_16	u2PseRevPgLmac1Group4;
	UINT_16	u2PseRevPgLmac2Group5;
	UINT_16	u2PseRevPgPleGroup6;

	UINT_16	u2PleSrvPgHif0Group0;
	UINT_16	u2PleSrvPgCpuGroup2;

	UINT_16	u2PseSrvPgHif0Group0;
	UINT_16	u2PseSrvPgHif1Group1;
	UINT_16	u2PseSrvPgCpuGroup2;
	UINT_16	u2PseSrvPgLmac0Group3;
	UINT_16	u2PseSrvPgLmac1Group4;
	UINT_16	u2PseSrvPgLmac2Group5;
	UINT_16	u2PseSrvPgPleGroup6;

	UINT_16	u2PleTotalPageNum;
	UINT_16	u2PleFreePageNum;
	UINT_16	u2PleFfaNum;

	UINT_16	u2PseTotalPageNum;
	UINT_16	u2PseFreePageNum;
	UINT_16	u2PseFfaNum;
} UMAC_STAT2_GET_T, *P_UMAC_STAT2_GET_T;

typedef struct _CNM_STATUS_T {
	UINT_8              fgDbDcModeEn;
	UINT_8              ucChNumB0;
	UINT_8              ucChNumB1;
	UINT_8              usReserved;
} CNM_STATUS_T, *P_CNM_STATUS_T;

typedef struct _CNM_CH_LIST_T {
	UINT_8              ucChNum[4];
} CNM_CH_LIST_T, *P_CNM_CH_LIST_T;

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
/*--------------------------------------------------------------*/
/* Routines to set parameters or query information.             */
/*--------------------------------------------------------------*/
/***** Routines in wlan_oid.c *****/
WLAN_STATUS
wlanoidQueryNetworkTypesSupported(IN P_ADAPTER_T prAdapter,
				  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryNetworkTypeInUse(IN P_ADAPTER_T prAdapter,
			     OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetNetworkTypeInUse(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryBssid(IN P_ADAPTER_T prAdapter,
		  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetBssidListScan(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetBssidListScanExt(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetBssidListScanAdv(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryBssidList(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetBssid(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetConnect(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetSsid(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQuerySsid(IN P_ADAPTER_T prAdapter,
		 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryInfrastructureMode(IN P_ADAPTER_T prAdapter,
			       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetInfrastructureMode(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryAuthMode(IN P_ADAPTER_T prAdapter,
		     OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetAuthMode(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if 0
WLAN_STATUS
wlanoidQueryPrivacyFilter(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetPrivacyFilter(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

WLAN_STATUS
wlanoidSetEncryptionStatus(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryEncryptionStatus(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetAddWep(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetRemoveWep(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetAddKey(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetRemoveKey(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetReloadDefaults(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetTest(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryCapability(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryFrequency(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetFrequency(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryAtimWindow(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetAtimWindow(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetChannel(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryRssi(IN P_ADAPTER_T prAdapter,
		 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
_wlanoidQueryRssi(IN P_ADAPTER_T prAdapter,
		 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
		 OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid);

WLAN_STATUS
wlanoidQueryRssiTrigger(IN P_ADAPTER_T prAdapter,
			OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetRssiTrigger(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryRtsThreshold(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetRtsThreshold(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQuery802dot11PowerSaveProfile(IN P_ADAPTER_T prAdapter,
				     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSet802dot11PowerSaveProfile(IN P_ADAPTER_T prAdapter,
				   IN PVOID prSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryPmkid(IN P_ADAPTER_T prAdapter,
		  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetPmkid(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQuerySupportedRates(IN P_ADAPTER_T prAdapter,
			   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryDesiredRates(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetDesiredRates(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryPermanentAddr(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryCurrentAddr(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryPermanentAddr(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryLinkSpeed(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
WLAN_STATUS wlanoidSetEfusBufferMode(IN P_ADAPTER_T prAdapter,
				     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

/*#if (CFG_EEPROM_PAGE_ACCESS == 1)*/
WLAN_STATUS
wlanoidQueryProcessAccessEfuseRead(IN P_ADAPTER_T prAdapter,
					IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryProcessAccessEfuseWrite(IN P_ADAPTER_T prAdapter,
					IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryEfuseFreeBlock(IN P_ADAPTER_T prAdapter,
					IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryGetTxPower(IN P_ADAPTER_T prAdapter,
					IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
/*#endif*/

#endif /* CFG_SUPPORT_BUFFER_MODE */
WLAN_STATUS
wlanoidQueryRxStatistics(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidBssInfoBasic(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidDevInfoActive(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidManualAssoc(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if CFG_SUPPORT_TX_BF
WLAN_STATUS
wlanoidTxBfAction(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
		  OUT PUINT_32 pu4SetInfoLen);
WLAN_STATUS wlanoidMuMimoAction(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
				OUT PUINT_32 pu4SetInfoLen);
WLAN_STATUS wlanoidStaRecUpdate(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
				OUT PUINT_32 pu4SetInfoLen);
WLAN_STATUS wlanoidStaRecBFUpdate(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen,
				  OUT PUINT_32 pu4SetInfoLen);
#endif /* CFG_SUPPORT_TX_BF */
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
WLAN_STATUS
wlanoidSendCalBackupV2Cmd(IN P_ADAPTER_T prAdapter, IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen);

WLAN_STATUS
wlanoidSetCalBackup(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryCalBackupV2(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);
#endif

WLAN_STATUS
wlanoidQueryMcrRead(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryMemDump(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetMcrWrite(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryDrvMcrRead(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetDrvMcrWrite(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQuerySwCtrlRead(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetSwCtrlWrite(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetChipConfig(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryChipConfig(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetKeyCfg(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryEepromRead(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetEepromWrite(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryRfTestRxStatus(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryRfTestTxStatus(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryOidInterfaceVersion(IN P_ADAPTER_T prAdapter,
				IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryVendorId(IN P_ADAPTER_T prAdapter,
		     OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryMulticastList(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetMulticastList(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryRcvError(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryRcvNoBuffer(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryRcvCrcError(IN P_ADAPTER_T prAdapter,
			IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryStatistics(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryCoexIso(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryCoexGetInfo(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

#ifdef LINUX

WLAN_STATUS
wlanoidQueryStatisticsForLinux(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

#endif

WLAN_STATUS
wlanoidQueryMediaStreamMode(IN P_ADAPTER_T prAdapter,
			    IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetMediaStreamMode(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryRcvOk(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryXmitOk(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryXmitError(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryXmitOneCollision(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryXmitMoreCollisions(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryXmitMaxCollisions(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetCurrentPacketFilter(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryCurrentPacketFilter(IN P_ADAPTER_T prAdapter,
				IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetAcpiDevicePowerState(IN P_ADAPTER_T prAdapter,
			       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryAcpiDevicePowerState(IN P_ADAPTER_T prAdapter,
				 IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetDisassociate(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryFragThreshold(IN P_ADAPTER_T prAdapter,
			  OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetFragThreshold(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryAdHocMode(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetAdHocMode(IN P_ADAPTER_T prAdapter,
		    IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryBeaconInterval(IN P_ADAPTER_T prAdapter,
			   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetBeaconInterval(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetCurrentAddr(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
WLAN_STATUS
wlanoidSetCSUMOffload(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

WLAN_STATUS
wlanoidSetNetworkAddress(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryMaxFrameSize(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryMaxTotalSize(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetCurrentLookahead(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

/* RF Test related APIs */
WLAN_STATUS
wlanoidRftestSetTestMode(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidRftestSetTestIcapMode(IN P_ADAPTER_T prAdapter,
			     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidRftestSetAbortTestMode(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidRftestQueryAutoTest(IN P_ADAPTER_T prAdapter,
			   OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidRftestSetAutoTest(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if CFG_SUPPORT_WAPI
WLAN_STATUS
wlanoidSetWapiMode(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetWapiAssocInfo(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetWapiKey(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

#if CFG_SUPPORT_WPS2
WLAN_STATUS
wlanoidSetWSCAssocInfo(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

#if CFG_ENABLE_WAKEUP_ON_LAN
WLAN_STATUS
wlanoidSetAddWakeupPattern(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetRemoveWakeupPattern(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryEnableWakeup(IN P_ADAPTER_T prAdapter,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 u4QueryInfoLen);

WLAN_STATUS
wlanoidSetEnableWakeup(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

WLAN_STATUS
wlanoidSetWiFiWmmPsTest(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetTxAmpdu(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetAddbaReject(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryNvramRead(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetNvramWrite(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryCfgSrcType(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidQueryEepromType(IN P_ADAPTER_T prAdapter,
		       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetCountryCode(IN P_ADAPTER_T prAdapter,
		      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS wlanSendMemDumpCmd(IN P_ADAPTER_T prAdapter, IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen);

#if CFG_SLT_SUPPORT

WLAN_STATUS
wlanoidQuerySLTStatus(IN P_ADAPTER_T prAdapter,
		      OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidUpdateSLTMode(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
WLAN_STATUS
wlanoidAdvCtrl(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
_wlanoidAdvCtrl(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
	OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid);

#endif

WLAN_STATUS
wlanoidQueryWlanInfo(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
_wlanoidQueryWlanInfo(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
	OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid);


WLAN_STATUS
wlanoidQueryMibInfo(IN P_ADAPTER_T prAdapter,
	OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);
#if CFG_SUPPORT_LAST_SEC_MCS_INFO
WLAN_STATUS
wlanoidTxMcsInfo(IN P_ADAPTER_T prAdapter,
	IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);
#endif

WLAN_STATUS
wlanoidSetFwLog2Host(
	IN P_ADAPTER_T prAdapter,
	IN PVOID pvSetBuffer,
	IN UINT_32 u4SetBufferLen,
	OUT PUINT_32 pu4SetInfoLen);

#if 0
WLAN_STATUS
wlanoidSetNoaParam(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetOppPsParam(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetUApsdParam(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidSetBT(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryBT(IN P_ADAPTER_T prAdapter,
	       OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetTxPower(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if 0
WLAN_STATUS
wlanoidQueryBtSingleAntenna(
	IN  P_ADAPTER_T prAdapter,
	OUT PVOID       pvQueryBuffer,
	IN  UINT_32     u4QueryBufferLen,
	OUT PUINT_32    pu4QueryInfoLen);

WLAN_STATUS
wlanoidSetBtSingleAntenna(
	IN  P_ADAPTER_T prAdapter,
	IN  PVOID       pvSetBuffer,
	IN  UINT_32     u4SetBufferLen,
	OUT PUINT_32    pu4SetInfoLen);

WLAN_STATUS
wlanoidSetPta(
	IN  P_ADAPTER_T prAdapter,
	IN  PVOID       pvSetBuffer,
	IN  UINT_32     u4SetBufferLen,
	OUT PUINT_32    pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryPta(
	IN  P_ADAPTER_T prAdapter,
	OUT PVOID       pvQueryBuffer,
	IN  UINT_32     u4QueryBufferLen,
	OUT PUINT_32    pu4QueryInfoLen);
#endif

#if CFG_ENABLE_WIFI_DIRECT
WLAN_STATUS
wlanoidSetP2pMode(IN P_ADAPTER_T prAdapter,
		  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

WLAN_STATUS
wlanoidSetDefaultKey(IN P_ADAPTER_T prAdapter,
		     IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetGtkRekeyData(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetStartSchedScan(IN P_ADAPTER_T prAdapter,
			 IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetStopSchedScan(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if CFG_M0VE_BA_TO_DRIVER
WLAN_STATUS wlanoidResetBAScoreboard(IN P_ADAPTER_T prAdapter, IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen);
#endif

#if CFG_SUPPORT_BATCH_SCAN
WLAN_STATUS
wlanoidSetBatchScanReq(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQueryBatchScanResult(IN P_ADAPTER_T prAdapter,
			    OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);
#endif

#if CFG_SUPPORT_PASSPOINT
WLAN_STATUS
wlanoidSetHS20Info(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetInterworkingInfo(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetRoamingConsortiumIEInfo(IN P_ADAPTER_T prAdapter,
				  IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidSetHS20BssidPool(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_SNIFFER
WLAN_STATUS wlanoidSetMonitor(IN P_ADAPTER_T prAdapter,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

WLAN_STATUS
wlanoidNotifyFwSuspend(IN P_ADAPTER_T prAdapter,
				IN PVOID pvSetBuffer,
				IN UINT_32 u4SetBufferLen,
				OUT PUINT_32 pu4SetInfoLen);
#if CFG_SUPPORT_DBDC
WLAN_STATUS
wlanoidSetDbdcEnable(
		IN P_ADAPTER_T prAdapter,
		IN PVOID pvSetBuffer,
		IN UINT_32 u4SetBufferLen,
		OUT PUINT_32 pu4SetInfoLen);
#endif /*#if CFG_SUPPORT_DBDC*/

WLAN_STATUS
wlanoidQuerySetTxTargetPower(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if (CFG_SUPPORT_DFS_MASTER == 1)
WLAN_STATUS
wlanoidQuerySetRddReport(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidQuerySetRadarDetectMode(IN P_ADAPTER_T prAdapter,
			IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
WLAN_STATUS
wlanoidQueryLteSafeChannel(IN P_ADAPTER_T prAdapter,
			   IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);
WLAN_STATUS
wlanCalculateAllChannelDirtiness(IN P_ADAPTER_T prAdapter);
VOID
wlanInitChnLoadInfoChannelList(IN P_ADAPTER_T prAdapter);
UINT_8
wlanGetChannelIndex(IN UINT_8 channel);
UINT_8
wlanGetChannelNumFromIndex(IN UINT_8 ucIdx);
VOID
wlanSortChannel(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS
wlanoidLinkDown(IN P_ADAPTER_T prAdapter,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
WLAN_STATUS
wlanoidSetCSIControl(
	IN P_ADAPTER_T prAdapter,
	IN PVOID pvSetBuffer,
	IN UINT_32 u4SetBufferLen,
	OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidGetTxPwrTbl(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvQueryBuffer,
		   IN UINT_32 u4QueryBufferLen,
		   OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
wlanoidEnableRoaming(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer,
		   IN UINT_32 u4SetBufferLen,
		   OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
wlanoidConfigRoaming(IN P_ADAPTER_T prAdapter,
		   IN PVOID pvSetBuffer,
		   IN UINT_32 u4SetBufferLen,
		   OUT PUINT_32 pu4SetInfoLen);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _WLAN_OID_H */
