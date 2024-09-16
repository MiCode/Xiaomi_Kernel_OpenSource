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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/wlan_lib.h#3
*/

/*! \file   "wlan_lib.h"
*    \brief  The declaration of the functions of the wlanAdpater objects
*
*    Detail description.
*/


#ifndef _WLAN_LIB_H
#define _WLAN_LIB_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "CFG_Wifi_File.h"
#include "rlm_domain.h"
#include "nic_init_cmd_event.h"

#if CFG_SUPPORT_CSI
struct CSI_DATA_T;
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define MAX_NUM_GROUP_ADDR                      32	/* max number of group addresses */
#define AUTO_RATE_NUM 8
#define AR_RATE_TABLE_ENTRY_MAX 25
#define AR_RATE_ENTRY_INDEX_NULL 0x80

#define TX_CS_TCP_UDP_GEN        BIT(1)
#define TX_CS_IP_GEN             BIT(0)

#define CSUM_OFFLOAD_EN_TX_TCP      BIT(0)
#define CSUM_OFFLOAD_EN_TX_UDP      BIT(1)
#define CSUM_OFFLOAD_EN_TX_IP       BIT(2)
#define CSUM_OFFLOAD_EN_RX_TCP      BIT(3)
#define CSUM_OFFLOAD_EN_RX_UDP      BIT(4)
#define CSUM_OFFLOAD_EN_RX_IPv4     BIT(5)
#define CSUM_OFFLOAD_EN_RX_IPv6     BIT(6)
#define CSUM_OFFLOAD_EN_TX_MASK     BITS(0, 2)
#define CSUM_OFFLOAD_EN_ALL         BITS(0, 6)

/* TCP, UDP, IP Checksum */
#define RX_CS_TYPE_UDP           BIT(7)
#define RX_CS_TYPE_TCP           BIT(6)
#define RX_CS_TYPE_IPv6          BIT(5)
#define RX_CS_TYPE_IPv4          BIT(4)

#define RX_CS_STATUS_UDP         BIT(3)
#define RX_CS_STATUS_TCP         BIT(2)
#define RX_CS_STATUS_IP          BIT(0)

#define CSUM_NOT_SUPPORTED      0x0

#define TXPWR_USE_PDSLOPE 0

/* NVRAM error code definitions */
#define NVRAM_ERROR_VERSION_MISMATCH        BIT(1)
#define NVRAM_ERROR_INVALID_TXPWR           BIT(2)
#define NVRAM_ERROR_INVALID_DPD             BIT(3)
#define NVRAM_ERROR_INVALID_MAC_ADDR        BIT(4)
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#define NVRAM_POWER_LIMIT_TABLE_INVALID     BIT(5)
#endif

#define NUM_TC_RESOURCE_TO_STATISTICS       4

#define WLAN_CFG_ARGV_MAX 23
#define WLAN_CFG_ARGV_MAX_LONG 22 /* for WOW, 2+20 */
#define WLAN_CFG_ENTRY_NUM_MAX 200 /* 128 */
#define WLAN_CFG_KEY_LEN_MAX 32	/* include \x00  EOL */
#define WLAN_CFG_VALUE_LEN_MAX 32	/* include \x00 EOL */
#define WLAN_CFG_FLAG_SKIP_CB BIT(0)
#define WLAN_CFG_FILE_BUF_SIZE 2048

#define WLAN_CFG_REC_ENTRY_NUM_MAX 200
#define WLAN_CFG_REC_FLAG_BIT BIT(0)


#define WLAN_CFG_SET_CHIP_LEN_MAX 10
#define WLAN_CFG_SET_DEBUG_LEVEL_LEN_MAX 10
#define WLAN_CFG_SET_SW_CTRL_LEN_MAX 10


#define WLAN_OID_TIMEOUT_THRESHOLD                  2000	/* OID timeout (in ms) */
#define WLAN_OID_TIMEOUT_THRESHOLD_MAX             10000	/* OID max timeout (in ms) */
#define WLAN_OID_TIMEOUT_THRESHOLD_IN_RESETTING      300	/* OID timeout during chip-resetting  (in ms) */

#define WLAN_OID_NO_ACK_THRESHOLD                   3

#define WLAN_THREAD_TASK_PRIORITY        0	/* If not setting the priority, 0 is the default */
#define WLAN_THREAD_TASK_NICE            (-10)	/* If not setting the nice, -10 is the default */

#define WLAN_TX_STATS_LOG_TIMEOUT                   30000
#define WLAN_TX_STATS_LOG_DURATION                  1500

/* Define for wifi path usage */
#define WLAN_FLAG_2G4_WF0					BIT(0)	/*1: support, 0: NOT support */
#define WLAN_FLAG_5G_WF0					BIT(1)	/*1: support, 0: NOT support */
#define WLAN_FLAG_2G4_WF1					BIT(2)	/*1: support, 0: NOT support */
#define WLAN_FLAG_5G_WF1					BIT(3)	/*1: support, 0: NOT support */
#define WLAN_FLAG_2G4_COANT_SUPPORT			BIT(4)	/*1: support, 0: NOT support */
#define WLAN_FLAG_2G4_COANT_PATH			BIT(5)	/*1: WF1, 0:WF0 */
#define WLAN_FLAG_5G_COANT_SUPPORT			BIT(6)	/*1: support, 0: NOT support */
#define WLAN_FLAG_5G_COANT_PATH				BIT(7)	/*1: WF1, 0:WF0 */

#if CFG_SUPPORT_EASY_DEBUG

#define MAX_CMD_ITEM_MAX			4	/* Max item per cmd. */
#define MAX_CMD_NAME_MAX_LENGTH		32	/* Max name string length */
#define MAX_CMD_VALUE_MAX_LENGTH	32	/* Max value string length */
#define MAX_CMD_TYPE_LENGTH			1
#define MAX_CMD_STRING_LENGTH		1
#define MAX_CMD_VALUE_LENGTH		1
#define MAX_CMD_RESERVE_LENGTH		1

#define CMD_FORMAT_V1_LENGTH	\
	(MAX_CMD_NAME_MAX_LENGTH + MAX_CMD_VALUE_MAX_LENGTH + \
	MAX_CMD_TYPE_LENGTH + MAX_CMD_STRING_LENGTH + MAX_CMD_VALUE_LENGTH + MAX_CMD_RESERVE_LENGTH)

#define MAX_CMD_BUFFER_LENGTH	(CMD_FORMAT_V1_LENGTH * MAX_CMD_ITEM_MAX)

#if 1
#define ED_STRING_SITE	0
#define ED_VALUE_SITE		1


#else
#define ED_ITEMTYPE_SITE	0
#define ED_STRING_SITE		1
#define ED_VALUE_SITE		2
#endif

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
#define ACS_AP_RSSI_LEVEL_HIGH -50
#define ACS_AP_RSSI_LEVEL_LOW -80
#define ACS_DIRTINESS_LEVEL_HIGH 52
#define ACS_DIRTINESS_LEVEL_MID 40
#define ACS_DIRTINESS_LEVEL_LOW 32
#endif

#if CFG_WOW_SUPPORT
#define INVALID_WOW_WAKE_UP_REASON 255
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
#define KEEP_FULL_PWR_TRAFFIC_REPORT_BIT BIT(0)
#define KEEP_FULL_PWR_NOISE_HISTOGRAM_BIT BIT(1)
#endif

typedef enum _CMD_VER_T {
	CMD_VER_1, /* Type[2]+String[32]+Value[32] */
	CMD_VER_2 /* for furtur define. */
} CMD_VER_T, *P_CMD_VER_T;


#if 0
typedef enum _ENUM_AIS_REQUEST_TYPE_T {
	AIS_REQUEST_SCAN,
	AIS_REQUEST_RECONNECT,
	AIS_REQUEST_ROAMING_SEARCH,
	AIS_REQUEST_ROAMING_CONNECT,
	AIS_REQUEST_REMAIN_ON_CHANNEL,
	AIS_REQUEST_NUM
} ENUM_AIS_REQUEST_TYPE_T;
#endif
typedef enum _CMD_TYPE_T {
	CMD_TYPE_QUERY,
	CMD_TYPE_SET
} CMD_TYPE_T, *P_CMD_TYPE_T;


#define ITEM_TYPE_DEC	1
#define ITEM_TYPE_HEX	2
#define ITEM_TYPE_STR	3

typedef enum _CMD_DEFAULT_SETTING_VALUE {
	CMD_PNO_ENABLE,
	CMD_PNO_SCAN_PERIOD,
	CMD_SCN_CHANNEL_PLAN,
	CMD_SCN_DWELL_TIME,
	CMD_SCN_STOP_SCAN,
	CMD_MAX,
} CMD_DEFAULT_SETTING_VALUE;

typedef enum _CMD_DEFAULT_STR_SETTING_VALUE {
	CMD_STR_TEST_STR,
	CMD_STR_MAX,
} CMD_DEFAULT_STR_SETTING_VALUE;

typedef struct _CMD_FORMAT_V1_T {
	UINT_8 itemType;
	UINT_8 itemStringLength;
	UINT_8 itemValueLength;
	UINT_8 Reserved;
	UINT_8 itemString[MAX_CMD_NAME_MAX_LENGTH];
	UINT_8 itemValue[MAX_CMD_VALUE_MAX_LENGTH];
} CMD_FORMAT_V1_T, *P_CMD_FORMAT_V1_T;

typedef struct _CMD_HEADER_T {
	CMD_VER_T	cmdVersion;
	CMD_TYPE_T	cmdType;
	UINT_8	itemNum;
	UINT_16	cmdBufferLen;
	UINT_8	buffer[MAX_CMD_BUFFER_LENGTH];
} CMD_HEADER_T, *P_CMD_HEADER_T;

typedef struct _CFG_DEFAULT_SETTING_TABLE_T {
	UINT_32 itemNum;
	const char *String;
	UINT_8 itemType;
	UINT_32 defaultValue;
	UINT_32 minValue;
	UINT_32 maxValue;
} CFG_DEFAULT_SETTING_TABLE_T, *P_CFG_DEFAULT_SETTING_TABLE_T;

typedef struct _CFG_DEFAULT_SETTING_STR_TABLE_T {
	UINT_32 itemNum;
	const char *String;
	UINT_8 itemType;
	const char *DefString;
	UINT_16 minLen;
	UINT_16 maxLen;
} CFG_DEFAULT_SETTING_STR_TABLE_T, *P_CFG_DEFAULT_SETTING_STR_TABLE_T;

typedef struct _CFG_QUERY_FORMAT_T {
	UINT_32 Length;
	UINT_32 Value;
	UINT_32 Type;
	PUINT_32 ptr;
} CFG_QUERY_FORMAT_T, *P_CFG_QUERY_FORMAT_T;

/*Globol Configure define */
typedef struct _CFG_SETTING_T {
	UINT_8	PnoEnable;
	UINT_32 PnoScanPeriod;
	UINT_8 ScnChannelPlan;
	UINT_16 ScnDwellTime;
	UINT_8 ScnStopScan;
	UINT_8 TestStr[80];

} CFG_SETTING_T, *P_CFG_SETTING_T;

#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef WLAN_STATUS(*PFN_OID_HANDLER_FUNC) (IN P_ADAPTER_T prAdapter,
					    IN PVOID pvBuf, IN UINT_32 u4BufLen, OUT PUINT_32 pu4OutInfoLen);

typedef enum _ENUM_CSUM_TYPE_T {
	CSUM_TYPE_IPV4,
	CSUM_TYPE_IPV6,
	CSUM_TYPE_TCP,
	CSUM_TYPE_UDP,
	CSUM_TYPE_NUM
} ENUM_CSUM_TYPE_T, *P_ENUM_CSUM_TYPE_T;

typedef enum _ENUM_CSUM_RESULT_T {
	CSUM_RES_NONE,
	CSUM_RES_SUCCESS,
	CSUM_RES_FAILED,
	CSUM_RES_NUM
} ENUM_CSUM_RESULT_T, *P_ENUM_CSUM_RESULT_T;

typedef enum _ENUM_PHY_MODE_T {
	ENUM_PHY_2G4_CCK,
	ENUM_PHY_2G4_OFDM_BPSK,
	ENUM_PHY_2G4_OFDM_QPSK,
	ENUM_PHY_2G4_OFDM_16QAM,
	ENUM_PHY_2G4_OFDM_48M,
	ENUM_PHY_2G4_OFDM_54M,
	ENUM_PHY_2G4_HT20_BPSK,
	ENUM_PHY_2G4_HT20_QPSK,
	ENUM_PHY_2G4_HT20_16QAM,
	ENUM_PHY_2G4_HT20_MCS5,
	ENUM_PHY_2G4_HT20_MCS6,
	ENUM_PHY_2G4_HT20_MCS7,
	ENUM_PHY_2G4_HT40_BPSK,
	ENUM_PHY_2G4_HT40_QPSK,
	ENUM_PHY_2G4_HT40_16QAM,
	ENUM_PHY_2G4_HT40_MCS5,
	ENUM_PHY_2G4_HT40_MCS6,
	ENUM_PHY_2G4_HT40_MCS7,
	ENUM_PHY_5G_OFDM_BPSK,
	ENUM_PHY_5G_OFDM_QPSK,
	ENUM_PHY_5G_OFDM_16QAM,
	ENUM_PHY_5G_OFDM_48M,
	ENUM_PHY_5G_OFDM_54M,
	ENUM_PHY_5G_HT20_BPSK,
	ENUM_PHY_5G_HT20_QPSK,
	ENUM_PHY_5G_HT20_16QAM,
	ENUM_PHY_5G_HT20_MCS5,
	ENUM_PHY_5G_HT20_MCS6,
	ENUM_PHY_5G_HT20_MCS7,
	ENUM_PHY_5G_HT40_BPSK,
	ENUM_PHY_5G_HT40_QPSK,
	ENUM_PHY_5G_HT40_16QAM,
	ENUM_PHY_5G_HT40_MCS5,
	ENUM_PHY_5G_HT40_MCS6,
	ENUM_PHY_5G_HT40_MCS7,
	ENUM_PHY_MODE_NUM
} ENUM_PHY_MODE_T, *P_ENUM_PHY_MODE_T;

typedef enum _ENUM_POWER_SAVE_POLL_MODE_T {
	ENUM_POWER_SAVE_POLL_DISABLE,
	ENUM_POWER_SAVE_POLL_LEGACY_NULL,
	ENUM_POWER_SAVE_POLL_QOS_NULL,
	ENUM_POWER_SAVE_POLL_NUM
} ENUM_POWER_SAVE_POLL_MODE_T, *P_ENUM_POWER_SAVE_POLL_MODE_T;

typedef enum _ENUM_AC_TYPE_T {
	ENUM_AC_TYPE_AC0,
	ENUM_AC_TYPE_AC1,
	ENUM_AC_TYPE_AC2,
	ENUM_AC_TYPE_AC3,
	ENUM_AC_TYPE_AC4,
	ENUM_AC_TYPE_AC5,
	ENUM_AC_TYPE_AC6,
	ENUM_AC_TYPE_BMC,
	ENUM_AC_TYPE_NUM
} ENUM_AC_TYPE_T, *P_ENUM_AC_TYPE_T;

typedef enum _ENUM_ADV_AC_TYPE_T {
	ENUM_ADV_AC_TYPE_RX_NSW,
	ENUM_ADV_AC_TYPE_RX_PTA,
	ENUM_ADV_AC_TYPE_RX_SP,
	ENUM_ADV_AC_TYPE_TX_PTA,
	ENUM_ADV_AC_TYPE_TX_RSP,
	ENUM_ADV_AC_TYPE_NUM
} ENUM_ADV_AC_TYPE_T, *P_ENUM_ADV_AC_TYPE_T;

typedef enum _ENUM_REG_CH_MAP_T {
	REG_CH_MAP_COUNTRY_CODE,
	REG_CH_MAP_TBL_IDX,
	REG_CH_MAP_CUSTOMIZED,
	REG_CH_MAP_NUM
} ENUM_REG_CH_MAP_T, *P_ENUM_REG_CH_MAP_T;

typedef enum _ENUM_FEATURE_OPTION_T {
	FEATURE_DISABLED,
	FEATURE_ENABLED,
	FEATURE_FORCE_ENABLED
} ENUM_FEATURE_OPTION_T, *P_ENUM_FEATURE_OPTION_T;

/* This enum is for later added feature options which use command reserved field as option switch */
typedef enum _ENUM_FEATURE_OPTION_IN_CMD_T {
	FEATURE_OPT_CMD_AUTO,
	FEATURE_OPT_CMD_DISABLED,
	FEATURE_OPT_CMD_ENABLED,
	FEATURE_OPT_CMD_FORCE_ENABLED
} ENUM_FEATURE_OPTION_IN_CMD_T, *P_ENUM_FEATURE_OPTION_IN_CMD_T;

#define DEBUG_MSG_SIZE_MAX 1200
enum {
	DEBUG_MSG_ID_UNKNOWN = 0x00,
	DEBUG_MSG_ID_PRINT = 0x01,
	DEBUG_MSG_ID_FWLOG = 0x02,
	DEBUG_MSG_ID_END
};

enum {
	DEBUG_MSG_TYPE_UNKNOWN = 0x00,
	DEBUG_MSG_TYPE_MEM8 = 0x01,
	DEBUG_MSG_TYPE_MEM32 = 0x02,
	DEBUG_MSG_TYPE_ASCII = 0x03,
	DEBUG_MSG_TYPE_BINARY = 0x04,
	DEBUG_MSG_TYPE_END
};

#define CHIP_CONFIG_RESP_SIZE 320
enum {
	CHIP_CONFIG_TYPE_WO_RESPONSE = 0x00,
	CHIP_CONFIG_TYPE_MEM8 = 0x01,
	CHIP_CONFIG_TYPE_MEM32 = 0x02,
	CHIP_CONFIG_TYPE_ASCII = 0x03,
	CHIP_CONFIG_TYPE_BINARY = 0x04,
	CHIP_CONFIG_TYPE_DRV_PASSTHROUGH = 0x05,
	CHIP_CONFIG_TYPE_END
};

typedef struct _SET_TXPWR_CTRL_T {
	INT_8 c2GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c2GHotspotPwrOffset;
	INT_8 c2GP2pPwrOffset;
	INT_8 c2GBowPwrOffset;
	INT_8 c5GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c5GHotspotPwrOffset;
	INT_8 c5GP2pPwrOffset;
	INT_8 c5GBowPwrOffset;
	UINT_8 ucConcurrencePolicy;	/* TX power policy when concurrence
					*  in the same channel
					*  0: Highest power has priority
					*  1: Lowest power has priority
					*/
	INT_8 acReserved1[3];	/* Must be zero */

	/* Power limit by channel for all data rates */
	INT_8 acTxPwrLimit2G[14];	/* Channel 1~14, Unit: 0.5dBm */
	INT_8 acTxPwrLimit5G[4];	/* UNII 1~4 */
	INT_8 acReserved2[2];	/* Must be zero */
} SET_TXPWR_CTRL_T, *P_SET_TXPWR_CTRL_T;

#if CFG_WOW_SUPPORT

typedef struct _WOW_WAKE_HIF_T {
	UINT_8		ucWakeupHif;	/* use in-band signal to wakeup system, ENUM_HIF_TYPE */
	UINT_8		ucGpioPin;		/* GPIO Pin */
	UINT_8		ucTriggerLvl;	/* refer to PF_WAKEUP_CMD_BIT0_OUTPUT_MODE_EN */
	UINT_32		u4GpioInterval;/* non-zero means output reverse wakeup signal after delay time */
	UINT_8		aucResv[5];
} WOW_WAKE_HIF_T, *P_WOW_WAKE_HIF_T;

typedef struct _WOW_PORT_T {
	UINT_8 ucIPv4UdpPortCnt;
	UINT_8 ucIPv4TcpPortCnt;
	UINT_8 ucIPv6UdpPortCnt;
	UINT_8 ucIPv6TcpPortCnt;
	UINT_16 ausIPv4UdpPort[MAX_TCP_UDP_PORT];
	UINT_16 ausIPv4TcpPort[MAX_TCP_UDP_PORT];
	UINT_16 ausIPv6UdpPort[MAX_TCP_UDP_PORT];
	UINT_16 ausIPv6TcpPort[MAX_TCP_UDP_PORT];
} WOW_PORT_T, *P_WOW_PORT_T;

typedef struct _WOW_CTRL_T {
	UINT_8 fgWowEnable;	/* 0: disable, 1: wow enable */
	UINT_8 ucScenarioId;	/* just a profile ID */
	UINT_8 ucBlockCount;
	UINT_8 aucReserved1[1];
	WOW_WAKE_HIF_T astWakeHif[2];
	WOW_PORT_T stWowPort;
	UINT_8 ucReason;
} WOW_CTRL_T, *P_WOW_CTRL_T;

#define MDNS_NAME_MAX_LEN	100
#define MDNS_RESPONSE_RECORD_MAX_LEN	500
#define MDNS_TXT_RR_DATA_MAX_LEN		300
#define MDNS_SRV_RR_LEN			16
#define	MDNS_TXT_RR_MAX_LEN		310
#define MDNS_QUESTION_NAME_MAX_LEN	102
#define MDNS_SERVICE_NAME_MAX_LEN	102
#define MDNS_A_RR_LEN		14

struct MDNS_PARAM_T {
	UINT_8 ucCmd;
	UINT_32 u4RecordId;
	UINT_8 ucQueryName[MDNS_NAME_MAX_LEN];
	UINT_16 u2QueryNameLen;
	UINT_16 u2QueryType;
	UINT_16 u2QueryClass;
	UINT_8 ucResponseRecord[MDNS_RESPONSE_RECORD_MAX_LEN];
	UINT_16 u2ResponseRecordLen;
	UINT_8 ucServiceName[MDNS_NAME_MAX_LEN];
	UINT_16 u2ServiceNameLen;
	UINT_8 ucServiceType[MDNS_NAME_MAX_LEN];
	UINT_16 u2ServiceTypeLen;
	UINT_8 ucDomainName[MDNS_NAME_MAX_LEN];
	UINT_16 u2DomainNameLen;
	UINT_8 ucHostName[MDNS_NAME_MAX_LEN];
	UINT_16 u2HostNameLen;
	UINT_32 u4Port;
	UINT_8 ucTxtRecord[MDNS_TXT_RR_DATA_MAX_LEN];
	UINT_16 u2TxtRecordLen;
};

struct WLAN_MAC_HEADER_QoS_T {
	UINT_16 u2FrameCtrl;
	UINT_16 DurationID;
	UINT_8 aucAddr1[MAC_ADDR_LEN];
	UINT_8 aucAddr2[MAC_ADDR_LEN];
	UINT_8 aucAddr3[MAC_ADDR_LEN];
	UINT_16 u2SeqCtrl;
	UINT_16 u2QosCtrl;
};

#define UDP_HEADER_LENGTH 8
#define IPV4_HEADER_LENGTH 20

struct MDNS_ANSWER_RR {
	UINT_8 data[MDNS_RESPONSE_RECORD_MAX_LEN];
	UINT_16 data_length;
};

struct MDNS_SRV_RR {
	UINT_8 data[MDNS_SRV_RR_LEN];
	UINT_16 data_length;
};

struct MDNS_TXT_RR {
	UINT_8 data[MDNS_TXT_RR_MAX_LEN];
	UINT_16 data_length;
};

struct MDNS_QUESTION {
	UINT_8 name[MDNS_QUESTION_NAME_MAX_LEN];
	UINT_8 name_length;
	UINT_8 label_count;
	UINT_16 class;
	UINT_16 type;
};

struct MDNS_Template_Record {
	UINT_32 u4RecordId;
	struct MDNS_QUESTION mdnsQuestionTemplate;
	struct MDNS_ANSWER_RR mdnsResponseRecord;
	UINT_8 ucServiceName[MDNS_SERVICE_NAME_MAX_LEN];
	UINT_16 ServiceNameLen;
	struct MDNS_TXT_RR mdnsTxtRecord;
	struct MDNS_SRV_RR mdnsSrvRecord;
	BOOLEAN bIsUsed;
};

struct CMD_MDNS_PARAM_T {
	UINT_8 ucCmd;
	UINT_32 u4RecordId;
	struct WLAN_MAC_HEADER_QoS_T aucMdnsMacHdr;
	UINT_8 aucMdnsIPHdr[IPV4_HEADER_LENGTH];
	UINT_8 aucMdnsUdpHdr[UDP_HEADER_LENGTH];
	UINT_8 mdnsARecord[MDNS_A_RR_LEN];
	struct MDNS_Template_Record mdnsQueryRespTemplate;
};
#endif

typedef enum _ENUM_NVRAM_MTK_FEATURE_T {
	MTK_FEATURE_2G_256QAM_DISABLED = 0,
	MTK_FEATURE_NUM
} ENUM_NVRAM_MTK_FEATURES_T, *P_ENUM_NVRAM_MTK_FEATURES_T;

/* For storing driver initialization value from glue layer */
typedef struct _REG_INFO_T {
	UINT_32 u4SdBlockSize;	/* SDIO block size */
	UINT_32 u4SdBusWidth;	/* SDIO bus width. 1 or 4 */
	UINT_32 u4SdClockRate;	/* SDIO clock rate. (in unit of HZ) */
	UINT_32 u4StartFreq;	/* Start Frequency for Ad-Hoc network : in unit of KHz */
	UINT_32 u4AdhocMode;	/* Default mode for Ad-Hoc network : ENUM_PARAM_AD_HOC_MODE_T */
	UINT_32 u4RddStartFreq;
	UINT_32 u4RddStopFreq;
	UINT_32 u4RddTestMode;
	UINT_32 u4RddShutFreq;
	UINT_32 u4RddDfs;
	INT_32 i4HighRssiThreshold;
	INT_32 i4MediumRssiThreshold;
	INT_32 i4LowRssiThreshold;
	INT_32 au4TxPriorityTag[ENUM_AC_TYPE_NUM];
	INT_32 au4RxPriorityTag[ENUM_AC_TYPE_NUM];
	INT_32 au4AdvPriorityTag[ENUM_ADV_AC_TYPE_NUM];
	UINT_32 u4FastPSPoll;
	UINT_32 u4PTA;		/* 0: disable, 1: enable */
	UINT_32 u4TXLimit;	/* 0: disable, 1: enable */
	UINT_32 u4SilenceWindow;	/* range: 100 - 625, unit: us */
	UINT_32 u4TXLimitThreshold;	/* range: 250 - 1250, unit: us */
	UINT_32 u4PowerMode;
	UINT_32 fgEnArpFilter;
	UINT_32 u4PsCurrentMeasureEn;
	UINT_32 u4UapsdAcBmp;
	UINT_32 u4MaxSpLen;
	UINT_32 fgDisOnlineScan;	/* 0: enable online scan, non-zero: disable online scan */
	UINT_32 fgDisBcnLostDetection;	/* 0: enable online scan, non-zero: disable online scan */
	UINT_32 u4FixedRate;	/* 0: automatic, non-zero: fixed rate */
	UINT_32 u4ArSysParam0;
	UINT_32 u4ArSysParam1;
	UINT_32 u4ArSysParam2;
	UINT_32 u4ArSysParam3;
	UINT_32 fgDisRoaming;	/* 0:enable roaming 1:disable */

	/* NVRAM - MP Data -START- */
#if 1
	UINT_16 u2Part1OwnVersion;
	UINT_16 u2Part1PeerVersion;
#endif
	UINT_8 aucMacAddr[6];
	UINT_16 au2CountryCode[4];	/* Country code (in ISO 3166-1 expression, ex: "US", "TW")  */
	TX_PWR_PARAM_T rTxPwr;
	UINT_8 aucEFUSE[144];
	UINT_8 ucTxPwrValid;
	UINT_8 ucSupport5GBand;
	UINT_8 fg2G4BandEdgePwrUsed;
	INT_8 cBandEdgeMaxPwrCCK;
	INT_8 cBandEdgeMaxPwrOFDM20;
	INT_8 cBandEdgeMaxPwrOFDM40;
	ENUM_REG_CH_MAP_T eRegChannelListMap;
	UINT_8 ucRegChannelListIndex;
	DOMAIN_INFO_ENTRY rDomainInfo;
	RSSI_PATH_COMPASATION_T rRssiPathCompasation;
	UINT_8 ucRssiPathCompasationUsed;
	/* NVRAM - MP Data -END- */

	/* NVRAM - Functional Data -START- */
	UINT_8 uc2G4BwFixed20M;
	UINT_8 uc5GBwFixed20M;
	UINT_8 ucEnable5GBand;
	UINT_8 ucGpsDesense;
	UINT_8 ucRxDiversity;
	/* NVRAM - Functional Data -END- */

	P_NEW_EFUSE_MAPPING2NVRAM_T prOldEfuseMapping;

	UINT_8 aucNvram[512];
	P_WIFI_CFG_PARAM_STRUCT prNvramSettings;

} REG_INFO_T, *P_REG_INFO_T;

/* for divided firmware loading */
typedef struct _FWDL_SECTION_INFO_T {
#if 0
	UINT_32 u4Offset;
	UINT_32 u4Reserved;
	UINT_32 u4Length;
	UINT_32 u4DestAddr;
#endif
	UINT_32 u4DestAddr;
	UINT_8 ucChipInfo;
	UINT_8 ucFeatureSet;
	UINT_8 ucEcoCode;
	UINT_8 aucReserved[9];
	UINT_8 aucBuildDate[16];
	UINT_32 u4Length;
} FWDL_SECTION_INFO_T, *P_FWDL_SECTION_INFO_T;

typedef struct _FIRMWARE_DIVIDED_DOWNLOAD_T {
#if 0
	UINT_32 u4Signature;
	UINT_32 u4CRC;		/* CRC calculated without first 8 bytes included */
	UINT_32 u4NumOfEntries;
	UINT_32 u4Reserved;
	FWDL_SECTION_INFO_T arSection[];
#endif
	FWDL_SECTION_INFO_T arSection[2];
} FIRMWARE_DIVIDED_DOWNLOAD_T, *P_FIRMWARE_DIVIDED_DOWNLOAD_T;

#if (CFG_UMAC_GENERATION >= 0x20)
#define LEN_4_BYTE_CRC	(4)

typedef struct _tailer_format_tag {
	UINT_32 addr;
	UINT_8 chip_info;
	UINT_8 feature_set;
	UINT_8 eco_code;
	UINT_8 ram_version[10];
	UINT_8 ram_built_date[15];
	UINT_32 len;

} tailer_format_t;

typedef struct _fw_image_tailer_tag {
	tailer_format_t ilm_info;
	tailer_format_t dlm_info;
} fw_image_tailer_t;
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
typedef struct _tailer_format_tag_2 {
	UINT_32 crc;
	UINT_32 addr;
	UINT_32 block_size;
	UINT_32 real_size;
	UINT_8  chip_info;
	UINT_8  feature_set;
	UINT_8  eco_code;
	UINT_8  ram_version[10];
	UINT_8  ram_built_date[15];
	UINT_32 len;
} tailer_format_t_2;
typedef struct _fw_image_tailer_tag_2 {
	tailer_format_t_2 ilm_info;
	tailer_format_t_2 dlm_info;
} fw_image_tailer_t_2;
typedef struct _fw_image_tailer_check {
	UINT_8	chip_info;
	UINT_8	feature_set;
	UINT_8	eco_code;
	UINT_8	ram_version[10];
	UINT_8	ram_built_date[15];
	UINT_32 len;
} fw_image_tailer_check;
#endif
typedef struct _PATCH_FORMAT_T {
	UINT_8 aucBuildDate[16];
	UINT_8 aucPlatform[4];
	UINT_32 u4SwHwVersion;
	UINT_32 u4PatchVersion;
	UINT_16 u2CRC;		/* CRC calculated for image only */
	UINT_8 ucPatchImage[0];
} PATCH_FORMAT_T, *P_PATCH_FORMAT_T;

#if 0
#define DATA_MODE_BIT_SHFT_ENCRYPT_MODE       (0)	/* bit0 */
#define DATA_MODE_MASK_ENCRYPT_MODE           (0x01 << DATA_MODE_BIT_SHFT_ENCRYPT_MODE)	/* bit0 */

#define DATA_MODE_BIT_SHFT_KEY_INDEX          (1)	/* bit[2:1] */
#define DATA_MODE_MASK_KEY_INDEX              (0x03 << DATA_MODE_BIT_SHFT_KEY_INDEX)	/* bit[2:1] */

#define DATA_MODE_BIT_SHFT_RESET_IV           (3)	/* bit3 */
#define DATA_MODE_MASK_RESET_IV               (0x1 << DATA_MODE_BIT_SHFT_RESET_IV)

#define DATA_MODE_BIT_SHFT_WORKING_PDA_OPTION (4)	/* bit4 */
#define DATA_MODE_MASK_WORKING_PDA_OPTION     (0x1 << DATA_MODE_BIT_SHFT_WORKING_PDA_OPTION)

#define DATA_MODE_BIT_SHFT_NEED_ACK           (31)	/* bit31 */
#define DATA_MODE_MASK_NEED_ACK               (0x01 << DATA_MODE_BIT_SHFT_NEED_ACK)	/* bit31 */
#endif

/* PDA - Patch Decryption Accelerator */
#define PDA_N9                 0
#define PDA_CR4                1

#define CR4_FWDL_SECTION_NUM   HIF_CR4_FWDL_SECTION_NUM
#define IMG_DL_STATUS_PORT_IDX HIF_IMG_DL_STATUS_PORT_IDX

typedef enum _ENUM_IMG_DL_IDX_T {
	IMG_DL_IDX_N9_FW,
	IMG_DL_IDX_CR4_FW,
	IMG_DL_IDX_PATCH
} ENUM_IMG_DL_IDX_T, *P_ENUM_IMG_DL_IDX_T;

#endif

typedef struct _PARAM_MCR_RW_STRUCT_T {
	UINT_32 u4McrOffset;
	UINT_32 u4McrData;
} PARAM_MCR_RW_STRUCT_T, *P_PARAM_MCR_RW_STRUCT_T;

/* per access category statistics */
typedef struct _WIFI_WMM_AC_STAT_T {
	UINT_32 u4TxMsdu;
	UINT_32 u4RxMsdu;
	UINT_32 u4TxDropMsdu;
	UINT_32 u4TxFailMsdu;
	UINT_32 u4TxRetryMsdu;
} WIFI_WMM_AC_STAT_T, *P_WIFI_WMM_AC_STAT_T;

typedef struct _TX_VECTOR_BBP_LATCH_T {
	UINT_32 u4TxVector1;
	UINT_32 u4TxVector2;
	UINT_32 u4TxVector4;
} TX_VECTOR_BBP_LATCH_T, *P_TX_VECTOR_BBP_LATCH_T;

typedef struct _MIB_INFO_STAT_T {
	UINT_32 u4RxMpduCnt;
	UINT_32 u4FcsError;
	UINT_32 u4RxFifoFull;
	UINT_32 u4AmpduTxSfCnt;
	UINT_32 u4AmpduTxAckSfCnt;
	UINT_16 u2TxRange1AmpduCnt;
	UINT_16 u2TxRange2AmpduCnt;
	UINT_16 u2TxRange3AmpduCnt;
	UINT_16 u2TxRange4AmpduCnt;
	UINT_16 u2TxRange5AmpduCnt;
	UINT_16 u2TxRange6AmpduCnt;
	UINT_16 u2TxRange7AmpduCnt;
	UINT_16 u2TxRange8AmpduCnt;
} MIB_INFO_STAT_T, *P_MIB_INFO_STAT_T;

typedef struct _PARAM_GET_STA_STATISTICS {
	/* Per-STA statistic */
	UINT_8 aucMacAddr[MAC_ADDR_LEN];

	UINT_32 u4Flag;

	UINT_8 ucReadClear;
	UINT_8 ucLlsReadClear;

	/* From driver */
	UINT_32 u4TxTotalCount;
	UINT_32 u4TxExceedThresholdCount;

	UINT_32 u4TxMaxTime;
	UINT_32 u4TxAverageProcessTime;

	UINT_32 u4RxTotalCount;

	UINT_32 au4TcResourceEmptyCount[NUM_TC_RESOURCE_TO_STATISTICS];
	UINT_32 au4TcQueLen[NUM_TC_RESOURCE_TO_STATISTICS];

	/* From FW */
	UINT_8 ucPer;		/* base: 128 */
	UINT_8 ucRcpi;
	UINT_32 u4PhyMode;
	UINT_16 u2LinkSpeed;	/* unit is 0.5 Mbits */

	UINT_32 u4TxFailCount;
	UINT_32 u4TxLifeTimeoutCount;

	UINT_32 u4TxAverageAirTime;
	UINT_32 u4TransmitCount;	/* Transmit in the air (wtbl) */
	UINT_32 u4TransmitFailCount;	/* Transmit without ack/ba in the air (wtbl) */

	WIFI_WMM_AC_STAT_T arLinkStatistics[AC_NUM];	/*link layer statistics */

	/* Global queue management statistic */
	UINT_32 au4TcAverageQueLen[NUM_TC_RESOURCE_TO_STATISTICS];
	UINT_32 au4TcCurrentQueLen[NUM_TC_RESOURCE_TO_STATISTICS];

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
	UINT_8 ucResetCounter;
	BOOLEAN fgIsForceTxStream;
	BOOLEAN fgIsForceSeOff;

	/* Reserved fields */
	UINT_8 au4Reserved[20];
} PARAM_GET_STA_STA_STATISTICS, *P_PARAM_GET_STA_STATISTICS;

typedef struct _PARAM_GET_BSS_STATISTICS {
	/* Per-STA statistic */
	UINT_8 aucMacAddr[MAC_ADDR_LEN];

	UINT_32 u4Flag;

	UINT_8 ucReadClear;

	UINT_8 ucLlsReadClear;

	UINT_8 ucBssIndex;

	/* From driver */
	UINT_32 u4TxTotalCount;
	UINT_32 u4TxExceedThresholdCount;

	UINT_32 u4TxMaxTime;
	UINT_32 u4TxAverageProcessTime;

	UINT_32 u4RxTotalCount;

	UINT_32 au4TcResourceEmptyCount[NUM_TC_RESOURCE_TO_STATISTICS];
	UINT_32 au4TcQueLen[NUM_TC_RESOURCE_TO_STATISTICS];

	/* From FW */
	UINT_8 ucPer;		/* base: 128 */
	UINT_8 ucRcpi;
	UINT_32 u4PhyMode;
	UINT_16 u2LinkSpeed;	/* unit is 0.5 Mbits */

	UINT_32 u4TxFailCount;
	UINT_32 u4TxLifeTimeoutCount;

	UINT_32 u4TxAverageAirTime;
	UINT_32 u4TransmitCount;	/* Transmit in the air (wtbl) */
	UINT_32 u4TransmitFailCount;	/* Transmit without ack/ba in the air (wtbl) */

	WIFI_WMM_AC_STAT_T arLinkStatistics[AC_NUM];	/*link layer statistics */

	/* Global queue management statistic */
	UINT_32 au4TcAverageQueLen[NUM_TC_RESOURCE_TO_STATISTICS];
	UINT_32 au4TcCurrentQueLen[NUM_TC_RESOURCE_TO_STATISTICS];

	/* Reserved fields */
	UINT_8 au4Reserved[32];	/* insufficient for LLS?? */
} PARAM_GET_BSS_STATISTICS, *P_PARAM_GET_BSS_STATISTICS;

typedef struct _PARAM_GET_DRV_STATISTICS {
	INT_32 i4TxPendingFrameNum;
	INT_32 i4TxPendingSecurityFrameNum;
	INT_32 i4TxPendingCmdNum;
	INT_32 i4PendingFwdFrameCount;		/* sync i4PendingFwdFrameCount in _TX_CTRL_T */
	UINT_32 u4MsduNumElem;			/* sync pad->rTxCtrl.rFreeMsduInfoList.u4NumElem */
	UINT_32 u4TxMgmtTxringQueueNumElem;	/* sync pad->rTxCtrl.rTxMgmtTxingQueue.u4NumElem */

	UINT_32 u4RxFreeSwRfbMsduNumElem;	/* sync pad->prRxCtrl.rFreeSwRfbList.u4NumElem */
	UINT_32 u4RxReceivedRfbNumElem;		/* sync pad->prRxCtrl.rReceivedRfbList.u4NumElem */
	UINT_32 u4RxIndicatedNumElem;		/* sync pad->prRxCtrl.rIndicatedRfbList.u4NumElem */
} PARAM_GET_DRV_STATISTICS, *P_PARAM_GET_DRV_STATISTICS;

typedef struct _NET_INTERFACE_INFO_T {
	UINT_8 ucBssIndex;
	PVOID pvNetInterface;
} NET_INTERFACE_INFO_T, *P_NET_INTERFACE_INFO_T;

typedef enum _ENUM_TX_RESULT_CODE_T {
	TX_RESULT_SUCCESS = 0,
	TX_RESULT_LIFE_TIMEOUT,
	TX_RESULT_RTS_ERROR,
	TX_RESULT_MPDU_ERROR,
	TX_RESULT_AGING_TIMEOUT,
	TX_RESULT_FLUSHED,
	TX_RESULT_BIP_ERROR,
	TX_RESULT_UNSPECIFIED_ERROR,
	TX_RESULT_DROPPED_IN_DRIVER = 32,
	TX_RESULT_DROPPED_IN_FW,
	TX_RESULT_QUEUE_CLEARANCE,
	TX_RESULT_NUM
} ENUM_TX_RESULT_CODE_T, *P_ENUM_TX_RESULT_CODE_T;

struct _WLAN_CFG_ENTRY_T {
	UINT_8 aucKey[WLAN_CFG_KEY_LEN_MAX];
	UINT_8 aucValue[WLAN_CFG_VALUE_LEN_MAX];
	WLAN_CFG_SET_CB pfSetCb;
	PVOID pPrivate;
	UINT_32 u4Flags;
};

struct _WLAN_CFG_T {
	UINT_32 u4WlanCfgEntryNumMax;
	UINT_32 u4WlanCfgKeyLenMax;
	UINT_32 u4WlanCfgValueLenMax;
	WLAN_CFG_ENTRY_T arWlanCfgBuf[WLAN_CFG_ENTRY_NUM_MAX];
};

struct _WLAN_CFG_REC_T {
	UINT_32 u4WlanCfgEntryNumMax;
	UINT_32 u4WlanCfgKeyLenMax;
	UINT_32 u4WlanCfgValueLenMax;
	WLAN_CFG_ENTRY_T arWlanCfgBuf[WLAN_CFG_REC_ENTRY_NUM_MAX];
};


typedef enum _ENUM_MAX_BANDWIDTH_SETTING_T {
	MAX_BW_20MHZ = 0,
	MAX_BW_40MHZ,
	MAX_BW_80MHZ,
	MAX_BW_160MHZ,
	MAX_BW_80_80_MHZ
} ENUM_MAX_BANDWIDTH_SETTING, *P_ENUM_MAX_BANDWIDTH_SETTING_T;

typedef struct _TX_PACKET_INFO {
	UINT_8 ucPriorityParam;
	UINT_32 u4PacketLen;
	UINT_8 aucEthDestAddr[MAC_ADDR_LEN];
	UINT_16 u2Flag;

#if 0
	BOOLEAN fgIs1X;
	BOOLEAN fgIsPAL;
	BOOLEAN fgIs802_3;
	BOOLEAN fgIsVlanExists;
	BOOLEAN fgIsDhcp;
	BOOLEAN fgIsArp;
#endif
} TX_PACKET_INFO, *P_TX_PACKET_INFO;

typedef enum _ENUM_TX_PROFILING_TAG_T {
	TX_PROF_TAG_OS_TO_DRV = 0,
	TX_PROF_TAG_DRV_ENQUE,
	TX_PROF_TAG_DRV_DEQUE,
	TX_PROF_TAG_DRV_TX_DONE,
	TX_PROF_TAG_MAC_TX_DONE
} ENUM_TX_PROFILING_TAG_T, *P_ENUM_TX_PROFILING_TAG_T;

enum ENUM_WF_PATH_FAVOR_T {
	ENUM_WF_NON_FAVOR = 0xff,
	ENUM_WF_0_ONE_STREAM_PATH_FAVOR = 0,
	ENUM_WF_1_ONE_STREAM_PATH_FAVOR = 1,
	ENUM_WF_0_1_TWO_STREAM_PATH_FAVOR = 2,
	ENUM_WF_0_1_DUP_STREAM_PATH_FAVOR = 3,
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
#define BUILD_SIGN(ch0, ch1, ch2, ch3) \
	((UINT_32)(UINT_8)(ch0) | ((UINT_32)(UINT_8)(ch1) << 8) |   \
	((UINT_32)(UINT_8)(ch2) << 16) | ((UINT_32)(UINT_8)(ch3) << 24))

#define MTK_WIFI_SIGNATURE BUILD_SIGN('M', 'T', 'K', 'W')

#define IS_FEATURE_ENABLED(_ucFeature) \
	(((_ucFeature) == FEATURE_ENABLED) || ((_ucFeature) == FEATURE_FORCE_ENABLED))
#define IS_FEATURE_FORCE_ENABLED(_ucFeature) ((_ucFeature) == FEATURE_FORCE_ENABLED)
#define IS_FEATURE_DISABLED(_ucFeature) ((_ucFeature) == FEATURE_DISABLED)

/* This macro is for later added feature options which use command reserved field as option switch */
/* 0: AUTO
 * 1: Disabled
 * 2: Enabled
 * 3: Force disabled
 */
#define FEATURE_OPT_IN_COMMAND(_ucFeature) ((_ucFeature) + 1)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

P_ADAPTER_T wlanAdapterCreate(IN P_GLUE_INFO_T prGlueInfo);

VOID wlanAdapterDestroy(IN P_ADAPTER_T prAdapter);

VOID wlanCardEjected(IN P_ADAPTER_T prAdapter);

VOID wlanIST(IN P_ADAPTER_T prAdapter);

BOOL wlanISR(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgGlobalIntrCtrl);

WLAN_STATUS wlanProcessCommandQueue(IN P_ADAPTER_T prAdapter, IN P_QUE_T prCmdQue);

WLAN_STATUS wlanSendCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

#if CFG_SUPPORT_MULTITHREAD
WLAN_STATUS wlanSendCommandMthread(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

WLAN_STATUS wlanTxCmdMthread(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanTxCmdDoneMthread(IN P_ADAPTER_T prAdapter);

VOID wlanClearTxCommandQueue(IN P_ADAPTER_T prAdapter);

VOID wlanClearTxCommandDoneQueue(IN P_ADAPTER_T prAdapter);

VOID wlanClearDataQueue(IN P_ADAPTER_T prAdapter);

VOID wlanClearRxToOsQueue(IN P_ADAPTER_T prAdapter);
#endif

VOID wlanReleaseCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

VOID wlanReleasePendingOid(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID wlanReleasePendingCMDbyBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

VOID wlanReturnPacketDelaySetupTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID wlanReturnPacket(IN P_ADAPTER_T prAdapter, IN PVOID pvPacket);

WLAN_STATUS
wlanQueryInformation(IN P_ADAPTER_T prAdapter,
		     IN PFN_OID_HANDLER_FUNC pfOidQryHandler,
		     IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
wlanSetInformation(IN P_ADAPTER_T prAdapter,
		   IN PFN_OID_HANDLER_FUNC pfOidSetHandler,
		   IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS wlanAdapterStart(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo);

WLAN_STATUS wlanAdapterStop(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanCheckWifiFunc(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgRdyChk);

#if CFG_SUPPORT_WAPI
BOOLEAN wlanQueryWapiMode(IN P_ADAPTER_T prAdapter);
#endif

VOID wlanReturnRxPacket(IN PVOID pvAdapter, IN PVOID pvPacket);

VOID wlanRxSetBroadcast(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableBroadcast);

BOOLEAN wlanIsHandlerNeedHwAccess(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo);

VOID wlanSetPromiscuousMode(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnablePromiscuousMode);
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
WLAN_STATUS wlanImageSectionDownloadStage(IN P_ADAPTER_T prAdapter,
					  IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength,
					  UINT_8 ucSectionNumber, IN ENUM_IMG_DL_IDX_T eDlIdx,
				OUT PUINT_8 ucIsCompressed,
				OUT P_INIT_CMD_WIFI_DECOMPRESSION_START prFwImageInFo);
#else
WLAN_STATUS wlanImageSectionDownloadStage(IN P_ADAPTER_T prAdapter,
					  IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength,
					  UINT_8 ucSectionNumber, IN ENUM_IMG_DL_IDX_T eDlIdx);

#endif

#if CFG_ENABLE_FW_DOWNLOAD
WLAN_STATUS wlanImageSectionConfig(IN P_ADAPTER_T prAdapter,
				   IN UINT_32 u4DestAddr, IN UINT_32 u4ImgSecSize, IN UINT_32 u4DataMode,
				   IN ENUM_IMG_DL_IDX_T eDlIdx);

WLAN_STATUS wlanImageSectionDownload(IN P_ADAPTER_T prAdapter, IN UINT_32 u4ImgSecSize, IN PUINT_8 pucImgSecBuf);

WLAN_STATUS wlanImageQueryStatus(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanImageSectionDownloadStatus(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum);
#define wlanConfigWifiFuncStatus wlanImageSectionDownloadStatus

WLAN_STATUS wlanConfigWifiFunc(IN P_ADAPTER_T prAdapter,
			       IN BOOLEAN fgEnable, IN UINT_32 u4StartAddress, IN UINT_8 ucPDA);

UINT_32 wlanCRC32(PUINT_8 buf, UINT_32 len);
#endif

WLAN_STATUS wlanSendDummyCmd(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgIsReqTxRsrc);

WLAN_STATUS wlanSendNicPowerCtrlCmd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPowerMode);

WLAN_STATUS wlanKeepFullPwr(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnable);

BOOLEAN wlanIsHandlerAllowedInRFTest(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo);

WLAN_STATUS wlanProcessQueuedSwRfb(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfbListHead);

WLAN_STATUS wlanProcessQueuedMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

BOOLEAN wlanoidTimeoutCheck(IN P_ADAPTER_T prAdapter, IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN UINT_32 u4Timeout);

VOID wlanoidClearTimeoutCheck(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanUpdateNetworkAddress(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanUpdateBasicConfig(IN P_ADAPTER_T prAdapter);

BOOLEAN wlanQueryTestMode(IN P_ADAPTER_T prAdapter);

BOOLEAN wlanProcessTxFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket);

/* Security Frame Handling */
BOOLEAN wlanProcessSecurityFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket);

VOID wlanSecurityFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanSecurityFrameTxTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
VOID wlanClearScanningResult(IN P_ADAPTER_T prAdapter);

VOID wlanClearBssInScanningResult(IN P_ADAPTER_T prAdapter, IN PUINT_8 arBSSID);

#if CFG_TEST_WIFI_DIRECT_GO
VOID wlanEnableP2pFunction(IN P_ADAPTER_T prAdapter);

VOID wlanEnableATGO(IN P_ADAPTER_T prAdapter);
#endif

/*----------------------------------------------------------------------------*/
/* NIC Capability Retrieve by Polling                                         */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryNicCapability(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* PD MCR Retrieve by Polling                                                 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryPdMcr(IN P_ADAPTER_T prAdapter, IN P_PARAM_MCR_RW_STRUCT_T prMcrRdInfo);

/*----------------------------------------------------------------------------*/
/* Loading Manufacture Data                                                   */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanLoadManufactureData(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo);

/*----------------------------------------------------------------------------*/
/* Media Stream Mode                                                          */
/*----------------------------------------------------------------------------*/
BOOLEAN wlanResetMediaStreamMode(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* Timer Timeout Check (for Glue Layer)                                       */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanTimerTimeoutCheck(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* Mailbox Message Check (for Glue Layer)                                     */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessMboxMessage(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* TX Pending Packets Handling (for Glue Layer)                               */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanEnqueueTxPacket(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prNativePacket);

WLAN_STATUS wlanFlushTxPendingPackets(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanTxPendingPackets(IN P_ADAPTER_T prAdapter, IN OUT PBOOLEAN pfgHwAccess);

/*----------------------------------------------------------------------------*/
/* Low Power Acquire/Release (for Glue Layer)                                 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanAcquirePowerControl(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanReleasePowerControl(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* Pending Packets Number Reporting (for Glue Layer)                          */
/*----------------------------------------------------------------------------*/
UINT_32 wlanGetTxPendingFrameCount(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* ACPI state inquiry (for Glue Layer)                                        */
/*----------------------------------------------------------------------------*/
ENUM_ACPI_STATE_T wlanGetAcpiState(IN P_ADAPTER_T prAdapter);

VOID wlanSetAcpiState(IN P_ADAPTER_T prAdapter, IN ENUM_ACPI_STATE_T ePowerState);

VOID wlanDefTxPowerCfg(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* get ECO version from Revision ID register (for Win32)                      */
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetEcoVersion(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* get Rom version                     */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetRomVersion(IN P_ADAPTER_T prAdapter);
/*----------------------------------------------------------------------------*/
/* set preferred band configuration corresponding to network type             */
/*----------------------------------------------------------------------------*/
VOID wlanSetPreferBandByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_BAND_T eBand, IN UINT_8 ucBssIndex);

/*----------------------------------------------------------------------------*/
/* get currently operating channel information                                */
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetChannelNumberByNetwork(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

/*----------------------------------------------------------------------------*/
/* check for system configuration to generate message on scan list            */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanCheckSystemConfiguration(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* query bss statistics information from driver and firmware                  */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryBssStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

/*----------------------------------------------------------------------------*/
/* dump per-BSS statistics            */
/*----------------------------------------------------------------------------*/
VOID wlanDumpBssStatistics(IN P_ADAPTER_T prAdapter, UINT_8 ucBssIndex);

/*----------------------------------------------------------------------------*/
/* query sta statistics information from driver and firmware                  */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
_wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen,
			  OUT PUINT_32 pu4QueryInfoLen, IN BOOLEAN fgIsOid);

/*----------------------------------------------------------------------------*/
/* query NIC resource information from chip and reset Tx resource for normal operation        */
/*----------------------------------------------------------------------------*/
VOID wlanQueryNicResourceInformation(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanQueryNicCapabilityV2(IN P_ADAPTER_T prAdapter);

VOID wlanUpdateNicResourceInformation(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* GET/SET BSS index mapping for network interfaces                                                    */
/*----------------------------------------------------------------------------*/
VOID wlanBindNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucNetInterfaceIndex, IN PVOID pvNetInterface);

VOID wlanBindBssIdxToNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex, IN PVOID pvNetInterface);

UINT_8 wlanGetBssIdxByNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvNetInterface);

PVOID wlanGetNetInterfaceByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex);

/* for windows as windows glue cannot see through P_ADAPTER_T */
UINT_8 wlanGetAisBssIndex(IN P_ADAPTER_T prAdapter);

VOID wlanInitFeatureOption(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetSwCtrl(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetChip(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetDebugLevel(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetCountryCode(IN P_ADAPTER_T prAdapter);

P_WLAN_CFG_ENTRY_T wlanCfgGetEntry(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, BOOLEAN fgGetCfgRec);

WLAN_STATUS
wlanCfgGet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, PCHAR pucValueDef, UINT_32 u4Flags);

UINT_32 wlanCfgGetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4ValueDef);

INT_32 wlanCfgGetInt32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, INT_32 i4ValueDef);

WLAN_STATUS wlanCfgSetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4Value);

WLAN_STATUS wlanCfgSet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, UINT_32 u4Flags);

WLAN_STATUS
wlanCfgSetCb(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, WLAN_CFG_SET_CB pfSetCb, void *pPrivate, UINT_32 u4Flags);

#if CFG_SUPPORT_EASY_DEBUG

WLAN_STATUS wlanCfgParse(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, BOOLEAN isFwConfig);
VOID wlanFeatureToFw(IN P_ADAPTER_T prAdapter);
#endif

VOID wlanLoadDefaultCustomerSetting(IN P_ADAPTER_T prAdapter);




WLAN_STATUS wlanCfgInit(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, UINT_32 u4Flags);

WLAN_STATUS wlanCfgParseArgument(CHAR *cmdLine, INT_32 *argc, CHAR *argv[]);

#if CFG_WOW_SUPPORT
WLAN_STATUS wlanCfgParseArgumentLong(CHAR *cmdLine, INT_32 *argc, CHAR *argv[]);
#endif

INT_32 wlanHexToNum(CHAR c);
INT_32 wlanHexToByte(PCHAR hex);

INT_32 wlanHwAddrToBin(PCHAR txt, UINT_8 *addr);

BOOLEAN wlanIsChipNoAck(IN P_ADAPTER_T prAdapter);

BOOLEAN wlanIsChipRstRecEnabled(IN P_ADAPTER_T prAdapter);

BOOLEAN wlanIsChipAssert(IN P_ADAPTER_T prAdapter);

VOID wlanChipRstPreAct(IN P_ADAPTER_T prAdapter);


VOID wlanTxProfilingTagPacket(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag);

VOID wlanTxProfilingTagMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_PROFILING_TAG_T eTag);
#if CFG_ASSERT_DUMP
VOID wlanCorDumpTimerReset(IN P_ADAPTER_T prAdapter, BOOLEAN fgIsResetN9);

VOID wlanN9CorDumpTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID wlanCr4CorDumpTimeOut(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);
#endif
#endif /* _WLAN_LIB_H */


BOOL wlanGetWlanIdxByAddress(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucAddr, OUT PUINT_8 pucIndex);

PUINT_8 wlanGetStaAddrByWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucIndex);

P_WLAN_CFG_ENTRY_T wlanCfgGetEntryByIndex(IN P_ADAPTER_T prAdapter, const UINT_8 ucIdx, UINT_32 flag);

WLAN_STATUS wlanGetStaIdxByWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucIndex, OUT PUINT_8 pucStaIdx);

/*----------------------------------------------------------------------------*/
/* update per-AC statistics for LLS                */
/*----------------------------------------------------------------------------*/
VOID wlanUpdateTxStatistics(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, BOOLEAN fgTxDrop);

VOID wlanUpdateRxStatistics(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS wlanTriggerStatsLog(IN P_ADAPTER_T prAdapter, IN UINT_32 u4DurationInMs);

WLAN_STATUS wlanDhcpTxDone(IN P_ADAPTER_T prAdapter,
			   IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS wlanArpTxDone(IN P_ADAPTER_T prAdapter,
			  IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS wlan1xTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo,
	IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS wlanDownloadFW(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanWakeUpWiFi(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanDownloadPatch(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanGetPatchInfo(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanPowerOffWifi(IN P_ADAPTER_T prAdapter);

VOID wlanPrintVersion(P_ADAPTER_T prAdapter);
WLAN_STATUS wlanAccessRegister(IN P_ADAPTER_T prAdapter,
			IN UINT_32 u4Addr, IN UINT_32 *pru4Result, IN UINT_32 u4Data,
			IN UINT_8 ucSetQuery);

WLAN_STATUS wlanAccessRegisterStatus(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum,
			IN UINT_8 ucSetQuery, IN PVOID prEvent, IN UINT_32 u4EventLen);

WLAN_STATUS wlanSetChipEcoInfo(IN P_ADAPTER_T prAdapter);

VOID wlanNotifyFwSuspend(P_GLUE_INFO_T prGlueInfo, struct net_device *prDev, BOOLEAN fgSuspend);

VOID wlanClearPendingInterrupt(IN P_ADAPTER_T prAdapter);

#if (MTK_WCN_HIF_SDIO && CFG_WMT_WIFI_PATH_SUPPORT)
extern INT_32 mtk_wcn_wmt_wifi_fem_cfg_report(PVOID pvInfoBuf);
#endif

UINT_8  wlanGetAntPathType(IN P_ADAPTER_T prAdapter, IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor);

UINT_8 wlanGetSpeIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN enum ENUM_WF_PATH_FAVOR_T eWfPathFavor);

UINT_8 wlanGetSupportNss(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

INT_32 wlanGetFileContent(P_ADAPTER_T prAdapter,
	const PUINT_8 pcFileName, PUINT_8 pucBuf,
	UINT_32 u4MaxFileLen, PUINT_32 pu4ReadFileLen, BOOL bReqFw);

#if CFG_SUPPORT_ANT_SELECT
WLAN_STATUS wlanUpdateExtInfo(IN P_ADAPTER_T prAdapter);
#endif

#if CFG_SUPPORT_CSI
bool wlanPushCSIData(P_ADAPTER_T prAdapter, struct CSI_DATA_T *prCSIData);
bool wlanPopCSIData(P_ADAPTER_T prAdapter, struct CSI_DATA_T *prCSIData);
VOID
wlanApplyCSIToneMask(
	UINT_8 ucRxMode,
	UINT_8 ucCBW,
	UINT_8 ucDBW,
	UINT_8 ucPrimaryChIdx,
	INT_16 *ai2IData,
	INT_16 *ai2QData);

VOID
wlanShiftCSI(
	UINT_8 ucRxMode,
	UINT_8 ucCBW,
	UINT_8 ucDBW,
	UINT_8 ucPrimaryChIdx,
	INT_16 *ai2IData,
	INT_16 *ai2QData,
	INT_16 *ai2ShiftIData,
	INT_16 *ai2ShiftQData);
#endif

int wlanSuspendRekeyOffload(P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRekeyMode);
VOID wlanSuspendPmHandle(P_GLUE_INFO_T prGlueInfo);
VOID wlanResumePmHandle(P_GLUE_INFO_T prGlueInfo);

void disconnect_sta(P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec);
