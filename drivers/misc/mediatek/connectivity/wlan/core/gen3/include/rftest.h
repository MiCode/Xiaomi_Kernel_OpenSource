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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/rftest.h#1
*/

/*
 * ! \file   "rftest.h"
 *  \brief  definitions for RF Productino test
 *
 */

#ifndef _RFTEST_H
#define _RFTEST_H

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
/* Table Version */
#define RF_AUTO_TEST_FUNCTION_TABLE_VERSION 0x01000001

/* Power */
#define RF_AT_PARAM_POWER_MASK      BITS(0, 7)
#define RF_AT_PARAM_POWER_MAX       RF_AT_PARAM_POWER_MASK

/* Rate */
#define RF_AT_PARAM_RATE_MCS_MASK   BIT(31)
#define RF_AT_PARAM_RATE_MASK       BITS(0, 7)
#define RF_AT_PARAM_RATE_CCK_MAX    3
#define RF_AT_PARAM_RATE_1M         0
#define RF_AT_PARAM_RATE_2M         1
#define RF_AT_PARAM_RATE_5_5M       2
#define RF_AT_PARAM_RATE_11M        3
#define RF_AT_PARAM_RATE_6M         4
#define RF_AT_PARAM_RATE_9M         5
#define RF_AT_PARAM_RATE_12M        6
#define RF_AT_PARAM_RATE_18M        7
#define RF_AT_PARAM_RATE_24M        8
#define RF_AT_PARAM_RATE_36M        9
#define RF_AT_PARAM_RATE_48M        10
#define RF_AT_PARAM_RATE_54M        11

/* Antenna */
#define RF_AT_PARAM_ANTENNA_ID_MASK BITS(0, 7)
#define RF_AT_PARAM_ANTENNA_ID_MAX  1

/* Packet Length */
#define RF_AT_PARAM_TX_80211HDR_BYTE_MAX     (32)
#define RF_AT_PARAM_TX_80211PAYLOAD_BYTE_MAX (2048)

#define RF_AT_PARAM_TX_PKTLEN_BYTE_DEFAULT  1024
#define RF_AT_PARAM_TX_PKTLEN_BYTE_MAX  \
	((UINT_16)(RF_AT_PARAM_TX_80211HDR_BYTE_MAX + RF_AT_PARAM_TX_80211PAYLOAD_BYTE_MAX))

/* Packet Count */
#define RF_AT_PARAM_TX_PKTCNT_DEFAULT    1000
#define RF_AT_PARAM_TX_PKTCNT_UNLIMITED  0

/* Packet Interval */
#define RF_AT_PARAM_TX_PKT_INTERVAL_US_DEFAULT  50

/* ALC */
#define RF_AT_PARAM_ALC_DISABLE     0
#define RF_AT_PARAM_ALC_ENABLE      1

/* TXOP */
#define RF_AT_PARAM_TXOP_DEFAULT    0
#define RF_AT_PARAM_TXOPQUE_QMASK   BITS(16, 31)
#define RF_AT_PARAM_TXOPQUE_TMASK   BITS(0, 15)
#define RF_AT_PARAM_TXOPQUE_AC0     (0<<16)
#define RF_AT_PARAM_TXOPQUE_AC1     (1<<16)
#define RF_AT_PARAM_TXOPQUE_AC2     (2<<16)
#define RF_AT_PARAM_TXOPQUE_AC3     (3<<16)
#define RF_AT_PARAM_TXOPQUE_AC4     (4<<16)
#define RF_AT_PARAM_TXOPQUE_QOFFSET 16

/* Retry Limit */
#define RF_AT_PARAM_TX_RETRY_DEFAULT    0
#define RF_AT_PARAM_TX_RETRY_MAX        6

/* QoS Queue */
#define RF_AT_PARAM_QOSQUE_AC0      0
#define RF_AT_PARAM_QOSQUE_AC1      1
#define RF_AT_PARAM_QOSQUE_AC2      2
#define RF_AT_PARAM_QOSQUE_AC3      3
#define RF_AT_PARAM_QOSQUE_AC4      4
#define RF_AT_PARAM_QOSQUE_DEFAULT  RF_AT_PARAM_QOSQUE_AC0

/* Bandwidth */
#define RF_AT_PARAM_BANDWIDTH_20MHZ             0
#define RF_AT_PARAM_BANDWIDTH_40MHZ             1
#define RF_AT_PARAM_BANDWIDTH_U20_IN_40MHZ      2
#define RF_AT_PARAM_BANDWIDTH_D20_IN_40MHZ      3
#define RF_AT_PARAM_BANDWIDTH_DEFAULT   RF_AT_PARAM_BANDWIDTH_20MHZ

/* GI (Guard Interval) */
#define RF_AT_PARAM_GI_800NS    0
#define RF_AT_PARAM_GI_400NS    1
#define RF_AT_PARAM_GI_DEFAULT  RF_AT_PARAM_GI_800NS

/* STBC */
#define RF_AT_PARAM_STBC_DISABLE    0
#define RF_AT_PARAM_STBC_ENABLE     1

/* RIFS */
#define RF_AT_PARAM_RIFS_DISABLE    0
#define RF_AT_PARAM_RIFS_ENABLE     1

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* Function ID List */
typedef enum _ENUM_RF_AT_FUNCID_T {
	RF_AT_FUNCID_VERSION = 0,
	RF_AT_FUNCID_COMMAND,
	RF_AT_FUNCID_POWER,
	RF_AT_FUNCID_RATE,
	RF_AT_FUNCID_PREAMBLE,
	RF_AT_FUNCID_ANTENNA,
	RF_AT_FUNCID_PKTLEN,
	RF_AT_FUNCID_PKTCNT,
	RF_AT_FUNCID_PKTINTERVAL,
	RF_AT_FUNCID_TEMP_COMPEN,
	RF_AT_FUNCID_TXOPLIMIT,
	RF_AT_FUNCID_ACKPOLICY,
	RF_AT_FUNCID_PKTCONTENT,
	RF_AT_FUNCID_RETRYLIMIT,
	RF_AT_FUNCID_QUEUE,
	RF_AT_FUNCID_BANDWIDTH,
	RF_AT_FUNCID_GI,
	RF_AT_FUNCID_STBC,
	RF_AT_FUNCID_CHNL_FREQ,
	RF_AT_FUNCID_RIFS,
	RF_AT_FUNCID_TRSW_TYPE,
	RF_AT_FUNCID_RF_SX_SHUTDOWN,
	RF_AT_FUNCID_PLL_SHUTDOWN,
	RF_AT_FUNCID_SLOW_CLK_MODE,
	RF_AT_FUNCID_ADC_CLK_MODE,
	RF_AT_FUNCID_MEASURE_MODE,
	RF_AT_FUNCID_VOLT_COMPEN,
	RF_AT_FUNCID_DPD_TX_GAIN,
	RF_AT_FUNCID_DPD_MODE,
	RF_AT_FUNCID_TSSI_MODE,
	RF_AT_FUNCID_TX_GAIN_CODE,
	RF_AT_FUNCID_TX_PWR_MODE,

	/* Query command */
	RF_AT_FUNCID_TXED_COUNT = 32,
	RF_AT_FUNCID_TXOK_COUNT,
	RF_AT_FUNCID_RXOK_COUNT,
	RF_AT_FUNCID_RXERROR_COUNT,
	RF_AT_FUNCID_RESULT_INFO,
	RF_AT_FUNCID_TRX_IQ_RESULT,
	RF_AT_FUNCID_TSSI_RESULT,
	RF_AT_FUNCID_DPD_RESULT,
	RF_AT_FUNCID_RXV_DUMP,
	RF_AT_FUNCID_RX_PHY_STATIS,
	RF_AT_FUNCID_MEASURE_RESULT,
	RF_AT_FUNCID_TEMP_SENSOR,
	RF_AT_FUNCID_VOLT_SENSOR,
	RF_AT_FUNCID_READ_EFUSE,
	RF_AT_FUNCID_RX_RSSI,
	RF_AT_FUNCID_FW_INFO,
	RF_AT_FUNCID_DRV_INFO,
	RF_AT_FUNCID_PWR_DETECTOR,
	RF_AT_FUNCID_WBRSSI_IBSSI,

	/* Set command */
	RF_AT_FUNCID_SET_DPD_RESULT = 64,
	RF_AT_FUNCID_SET_CW_MODE,
	RF_AT_FUNCID_SET_JAPAN_CH14_FILTER,
	RF_AT_FUNCID_WRITE_EFUSE,
	RF_AT_FUNCID_SET_MAC_ADDRESS,
	RF_AT_FUNCID_SET_TA,
	RF_AT_FUNCID_SET_RX_MATCH_RULE,

	/*80211AC & Jmode */
	RF_AT_FUNCID_SET_CBW = 71,
	RF_AT_FUNCID_SET_DBW,
	RF_AT_FUNCID_SET_PRIMARY_CH,
	RF_AT_FUNCID_SET_ENCODE_MODE,
	RF_AT_FUNCID_SET_J_MODE,

	/*ICAP command */
	RF_AT_FUNCID_SET_ICAP_CONTENT = 80,
	RF_AT_FUNCID_SET_ICAP_MODE,
	RF_AT_FUNCID_SET_ICAP_STARTCAP,
	RF_AT_FUNCID_SET_ICAP_SIZE = 83,
#if CFG_SUPPORT_QA_TOOL
	RF_AT_FUNCID_SET_ICAP_TRIGGER_OFFSET,
#else
	RF_AT_FUNCID_SET_ICAP_TRIGGER_EVENT,
#endif
	RF_AT_FUNCID_QUERY_ICAP_DUMP_FILE = 85,
#if CFG_SUPPORT_QA_TOOL
	/* 2G 5G Band */
	RF_AT_FUNCID_SET_BAND = 90,

	/* Reset Counter */
	RF_AT_FUNCID_RESETTXRXCOUNTER = 91,

	/* FAGC RSSI Path */
	RF_AT_FUNCID_FAGC_RSSI_PATH = 92,

	/* Set RX Filter Packet Length */
	RF_AT_FUNCID_RX_FILTER_PKT_LEN = 93,

	/* Tone */
	RF_AT_FUNCID_SET_TONE_RF_GAIN = 96,
	RF_AT_FUNCID_SET_TONE_DIGITAL_GAIN = 97,
	RF_AT_FUNCID_SET_TONE_TYPE = 98,
	RF_AT_FUNCID_SET_TONE_DC_OFFSET = 99,
	RF_AT_FUNCID_SET_TONE_BW = 100,

	/* MT6632 Add */
	RF_AT_FUNCID_SET_MAC_HEADER = 101,
	RF_AT_FUNCID_SET_SEQ_CTRL = 102,
	RF_AT_FUNCID_SET_PAYLOAD = 103,
	RF_AT_FUNCID_SET_DBDC_BAND_IDX = 104,
	RF_AT_FUNCID_SET_BYPASS_CAL_STEP = 105,

	/* Set RX Path */
	RF_AT_FUNCID_SET_RX_PATH = 106,

	/* Set Frequency Offset */
	RF_AT_FUNCID_SET_FRWQ_OFFSET = 107,

	/* Get Frequency Offset */
	RF_AT_FUNCID_GET_FREQ_OFFSET = 108,

	/* Set RXV Debug Index */
	RF_AT_FUNCID_SET_RXV_INDEX = 109,

	/* Set Test Mode DBDC Enable */
	RF_AT_FUNCID_SET_DBDC_ENABLE = 110,

	/* Get Test Mode DBDC Enable */
	RF_AT_FUNCID_GET_DBDC_ENABLE = 111,

	/* Set ICAP Ring Capture */
	RF_AT_FUNCID_SET_ICAP_RING = 112,

	/* Set TX Path */
	RF_AT_FUNCID_SET_TX_PATH = 113,

	/* Set Nss */
	RF_AT_FUNCID_SET_NSS = 114,

	/* Set TX Antenna Mask */
	RF_AT_FUNCID_SET_ANTMASK = 115,

	/* TMR set command */
	RF_AT_FUNCID_SET_TMR_ROLE = 116,
	RF_AT_FUNCID_SET_TMR_MODULE = 117,
	RF_AT_FUNCID_SET_TMR_DBM = 118,
	RF_AT_FUNCID_SET_TMR_ITER = 119,

	/* Set ADC For IRR Feature */
	RF_AT_FUNCID_SET_ADC = 120,

	/* Set RX Gain For IRR Feature */
	RF_AT_FUNCID_SET_RX_GAIN = 121,

	/* Set TTG For IRR Feature */
	RF_AT_FUNCID_SET_TTG = 122,

	/* Set TTG ON/OFF For IRR Feature */
	RF_AT_FUNCID_TTG_ON_OFF = 123,

	/* Set TSSI for QA Tool Setting */
	RF_AT_FUNCID_SET_TSSI = 124,

	/* Set Recal Cal Step */
	RF_AT_FUNCID_SET_RECAL_CAL_STEP = 125,

	/* Set iBF/eBF enable */
	RF_AT_FUNCID_SET_IBF_ENABLE = 126,
	RF_AT_FUNCID_SET_EBF_ENABLE = 127,

	/* Set MPS Setting */
	RF_AT_FUNCID_SET_MPS_SIZE = 128,
	RF_AT_FUNCID_SET_MPS_SEQ_DATA = 129,
	RF_AT_FUNCID_SET_MPS_PAYLOAD_LEN = 130,
	RF_AT_FUNCID_SET_MPS_PKT_CNT = 131,
	RF_AT_FUNCID_SET_MPS_PWR_GAIN = 132,
	RF_AT_FUNCID_SET_MPS_NSS = 133,
	RF_AT_FUNCID_SET_MPS_PACKAGE_BW = 134
#endif
} ENUM_RF_AT_FUNCID_T;

/* Command */
typedef enum _ENUM_RF_AT_COMMAND_T {
	RF_AT_COMMAND_STOPTEST = 0,
	RF_AT_COMMAND_STARTTX,
	RF_AT_COMMAND_STARTRX,
	RF_AT_COMMAND_RESET,
	RF_AT_COMMAND_OUTPUT_POWER,	/* Payload */
	RF_AT_COMMAND_LO_LEAKAGE,	/* Local freq is renamed to Local leakage */
	RF_AT_COMMAND_CARRIER_SUPPR,	/* OFDM (LTF/STF), CCK (PI,PI/2) */
	RF_AT_COMMAND_TRX_IQ_CAL,
	RF_AT_COMMAND_TSSI_CAL,
	RF_AT_COMMAND_DPD_CAL,
	RF_AT_COMMAND_CW,
	RF_AT_COMMAND_ICAP,
	RF_AT_COMMAND_RDD,
	RF_AT_COMMAND_CH_SWITCH_FOR_ICAP,
	RF_AT_COMMAND_RESET_DUMP_NAME,
	RF_AT_COMMAND_SINGLE_TONE,
	RF_AT_COMMAND_RDD_OFF,
	RF_AT_COMMAND_NUM
} ENUM_RF_AT_COMMAND_T;

/* Preamble */
typedef enum _ENUM_RF_AT_PREAMBLE_T {
	RF_AT_PREAMBLE_NORMAL = 0,
	RF_AT_PREAMBLE_CCK_SHORT,
	RF_AT_PREAMBLE_11N_MM,
	RF_AT_PREAMBLE_11N_GF,
	RF_AT_PREAMBLE_11AC,
	RF_AT_PREAMBLE_NUM
} ENUM_RF_AT_PREAMBLE_T;

/* Ack Policy */
typedef enum _ENUM_RF_AT_ACK_POLICY_T {
	RF_AT_ACK_POLICY_NORMAL = 0,
	RF_AT_ACK_POLICY_NOACK,
	RF_AT_ACK_POLICY_NOEXPLICTACK,
	RF_AT_ACK_POLICY_BLOCKACK,
	RF_AT_ACK_POLICY_NUM
} ENUM_RF_AT_ACK_POLICY_T;

typedef enum _ENUM_RF_AUTOTEST_STATE_T {
	RF_AUTOTEST_STATE_STANDBY = 0,
	RF_AUTOTEST_STATE_TX,
	RF_AUTOTEST_STATE_RX,
	RF_AUTOTEST_STATE_RESET,
	RF_AUTOTEST_STATE_OUTPUT_POWER,
	RF_AUTOTEST_STATE_LOCA_FREQUENCY,
	RF_AUTOTEST_STATE_CARRIER_SUPRRESION,
	RF_AUTOTEST_STATE_NUM
} ENUM_RF_AUTOTEST_STATE_T;

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

WLAN_STATUS rftestSetATInfo(IN P_ADAPTER_T prAdapter, UINT_32 u4FuncIndex, UINT_32 u4FuncData);

WLAN_STATUS
rftestQueryATInfo(IN P_ADAPTER_T prAdapter,
		  UINT_32 u4FuncIndex, UINT_32 u4FuncData, OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen);

WLAN_STATUS rftestSetFrequency(IN P_ADAPTER_T prAdapter, IN UINT_32 u4FreqInKHz, IN PUINT_32 pu4SetInfoLen);

#endif /* _RFTEST_H */
