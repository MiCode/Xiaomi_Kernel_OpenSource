/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 *
   Airgo Networks, Inc proprietary.
   All Rights Reserved, Copyright 2005
   This program is the confidential and proprietary product of Airgo Networks Inc.
   Any Unauthorized use, reproduction or transfer of this program is strictly prohibited.


   pttMsgApi.h: Contains messages to PTT Module for physical layer testing
   Author:  Mark Nelson
   Date:    6/21/05

   History -
   Date        Modified by              Modification Information
  --------------------------------------------------------------------------

 */

#ifndef PTT_MSG_API_H
#define PTT_MSG_API_H

#include "halCompiler.h"
#include "wlan_nv.h"
#include "wlan_phy.h"
#include "pttFrameGen.h"
#include "pttModule.h"

#include "halLegacyPalTypes.h"

typedef tANI_U8 tQWPTT_U8;
typedef tANI_S8 tQWPTT_S8;

typedef tANI_U16 tQWPTT_U16;
typedef tANI_S16 tQWPTT_S16;

typedef tANI_U32 tQWPTT_U32;
typedef tANI_S32 tQWPTT_S32;

typedef tANI_U8 tQWPTT_BYTE;
typedef tANI_S9 tQWPTT_S9;

typedef tANI_U8 tQWPTT_BOOLEAN;

#define PTT_MEM_ACCESS_MAX_SIZE 256

//Messages to/from socket or pttApi.c
typedef enum {
   PTT_MSG_TYPES_BEGIN = 0x3000,

   // Init
   PTT_MSG_INIT = PTT_MSG_TYPES_BEGIN, //extra: internal only

//NV Service
   PTT_MSG_GET_TPC_CAL_STATE_OBSOLETE = 0x3011,
   PTT_MSG_RESET_TPC_CAL_STATE_OBSOLETE = 0x3012,

   PTT_MSG_SET_NV_CKSUM_OBSOLETE = 0x3013,
   PTT_MSG_GET_NV_CKSUM_OBSOLETE = 0x3014,
   PTT_MSG_GET_NV_TABLE = 0x3016,
   PTT_MSG_SET_NV_TABLE = 0x3017,
   PTT_MSG_SET_NV_IMAGE_OBSOLETE = 0x3018,
   PTT_MSG_BLANK_NV = 0x3019,
   PTT_MSG_GET_NV_IMAGE_OBSOLETE = 0x301E,
   PTT_MSG_DEL_NV_TABLE = 0x301F,
   PTT_MSG_GET_NV_FIELD = 0x3020,
   PTT_MSG_SET_NV_FIELD = 0x3021,
   PTT_MSG_STORE_NV_TABLE = 0x3022,
   PTT_MSG_SET_REG_DOMAIN = 0x3023,

//new NV format Service
   PTT_MSG_GET_NV_BIN = 0x3030,
   PTT_MSG_SET_NV_BIN = 0x3031,
   PTT_MSG_GET_DICTIONARY = 0x3032,

//Device Register Access
   PTT_MSG_DBG_READ_REGISTER = 0x3040,
   PTT_MSG_DBG_WRITE_REGISTER = 0x3041,
   PTT_MSG_API_WRITE_REGISTER_OBSOLETE = 0x3042,
   PTT_MSG_API_READ_REGISTER_OBSOLETE = 0x3043,
   PTT_MSG_DBG_READ_MEMORY = 0x3044,
   PTT_MSG_DBG_WRITE_MEMORY = 0x3045,

//Device MAC Test Setup
   PTT_MSG_ENABLE_CHAINS = 0x304F,
   PTT_MSG_SET_CHANNEL = 0x3050,

//Tx Waveform Gen Service
   PTT_MSG_SET_WAVEFORM = 0x3071,
   PTT_MSG_SET_TX_WAVEFORM_GAIN = 0x3072,
   PTT_MSG_GET_WAVEFORM_POWER_ADC = 0x3073,
   PTT_MSG_START_WAVEFORM = 0x3074,
   PTT_MSG_STOP_WAVEFORM = 0x3075,
   PTT_MSG_SET_RX_WAVEFORM_GAIN = 0x3076,
   PTT_MSG_SET_TX_WAVEFORM_GAIN_PRIMA_V1 = 0x3077,

//Tx Frame Gen Service
   PTT_MSG_CONFIG_TX_PACKET_GEN = 0x3081,
   PTT_MSG_START_STOP_TX_PACKET_GEN = 0x3082,
   PTT_MSG_POLL_TX_PACKET_PROGRESS_OBSOLETE = 0x3083,
   PTT_MSG_FRAME_GEN_STOP_IND_OBSOLETE = 0x3088,
   PTT_MSG_QUERY_TX_STATUS = 0x3089,


//Tx Frame Power Service
   PTT_MSG_CLOSE_TPC_LOOP = 0x30A0,

//open loop service
   PTT_MSG_SET_PACKET_TX_GAIN_TABLE = 0x30A1,
   PTT_MSG_SET_PACKET_TX_GAIN_INDEX = 0x30A2,
   PTT_MSG_FORCE_PACKET_TX_GAIN = 0x30A3,

//closed loop(CLPC) service
   PTT_MSG_SET_PWR_INDEX_SOURCE = 0x30A4,
   PTT_MSG_SET_TX_POWER = 0x30A5,
   PTT_MSG_GET_TX_POWER_REPORT = 0x30A7,
   PTT_MSG_SAVE_TX_PWR_CAL_TABLE_OBSOLETE = 0x30A8,
   PTT_MSG_SET_POWER_LUT = 0x30A9,
   PTT_MSG_GET_POWER_LUT = 0x30AA,
   PTT_MSG_GET_PACKET_TX_GAIN_TABLE = 0x30AB,
   PTT_MSG_SAVE_TX_PWR_FREQ_TABLE_OBSOLETE = 0x30AC,
   PTT_MSG_CLPC_TEMP_COMPENSATION_OBSOLETE = 0x30AD,

//Rx Gain Service
   PTT_MSG_DISABLE_AGC_TABLES = 0x30D0,
   PTT_MSG_ENABLE_AGC_TABLES = 0x30D1,
   PTT_MSG_SET_AGC_TABLES_OBSOLETE = 0x30D2,
   PTT_MSG_GET_RX_RSSI = 0x30D3,
   PTT_MSG_GET_AGC_TABLE_OBSOLETE = 0x30D5,

//Rx Frame Catcher Service
   PTT_MSG_SET_RX_DISABLE_MODE = 0x30D4,
   PTT_MSG_GET_RX_PKT_COUNTS = 0x30E0,
   PTT_MSG_RESET_RX_PACKET_STATISTICS = 0x30E2,
   PTT_MSG_GET_UNI_CAST_MAC_PKT_RX_RSSI = 0x30E3,
   PTT_MSG_GET_UNI_CAST_MAC_PKT_RX_RSSI_CONFIG = 0x30E4,

//Rx Symbol Service
   PTT_MSG_GRAB_RAM = 0x30F0,
   PTT_MSG_GRAB_RAM_ONE_CHAIN_OBSOLETE = 0x30F1,

//Phy Calibration Service
   PTT_MSG_RX_IQ_CAL = 0x3100,
   PTT_MSG_RX_DCO_CAL = 0x3101,
   PTT_MSG_TX_CARRIER_SUPPRESS_CAL = 0x3102,
   PTT_MSG_TX_IQ_CAL = 0x3103,
   PTT_MSG_EXECUTE_INITIAL_CALS = 0x3104,
   PTT_MSG_HDET_CAL = 0x3105,
   PTT_MSG_VCO_LINEARITY_CAL_OBSOLETE = 0x3106,

//Phy Calibration Override Service
   PTT_MSG_SET_TX_CARRIER_SUPPRESS_CORRECT = 0x3110,
   PTT_MSG_GET_TX_CARRIER_SUPPRESS_CORRECT = 0x3111,
   PTT_MSG_SET_TX_IQ_CORRECT = 0x3112,
   PTT_MSG_GET_TX_IQ_CORRECT = 0x3113,
   PTT_MSG_SET_RX_IQ_CORRECT = 0x3114,
   PTT_MSG_GET_RX_IQ_CORRECT = 0x3115,
   PTT_MSG_SET_RX_DCO_CORRECT = 0x3116,
   PTT_MSG_GET_RX_DCO_CORRECT = 0x3117,
   PTT_MSG_SET_TX_IQ_PHASE_NV_TABLE_OBSOLETE = 0x3118,
   PTT_MSG_GET_HDET_CORRECT_OBSOLETE = 0x3119,

//RF Chip Access
   PTT_MSG_GET_TEMP_ADC = 0x3202,
   PTT_MSG_READ_RF_REG = 0x3203,
   PTT_MSG_WRITE_RF_REG = 0x3204,
   PTT_MSG_GET_RF_VERSION = 0x3205,

//Deep sleep support
   PTT_MSG_DEEP_SLEEP = 0x3220,
   PTT_MSG_READ_SIF_BAR4_REGISTER = 0x3221,
   PTT_MSG_WRITE_SIF_BAR4_REGISTER = 0x3222,
   PTT_MSG_ENTER_FULL_POWER = 0x3223,

//Misc
   PTT_MSG_SYSTEM_RESET = 0x32A0,   //is there any meaning for this in Gen6?
   PTT_MSG_LOG_DUMP = 0x32A1,
   PTT_MSG_GET_BUILD_RELEASE_NUMBER = 0x32A2,


//Messages for Socket App
   PTT_MSG_ADAPTER_DISABLED_RSP_OBSOLETE = 0x32A3,
   PTT_MSG_ENABLE_ADAPTER = 0x32A4,
   PTT_MSG_DISABLE_ADAPTER = 0x32A5,
   PTT_MSG_PAUSE_RSP_OBSOLETE = 0x32A6,
   PTT_MSG_CONTINUE_RSP_OBSOLETE = 0x32A7,

   PTT_MSG_HALPHY_INIT = 0x32A8,
   PTT_MSG_TEST_RXIQ_CAL = 0x32A9,
   PTT_MSG_START_TONE_GEN = 0x32AA,
   PTT_MSG_STOP_TONE_GEN = 0x32AB,
   PTT_MSG_RX_IM2_CAL = 0x32AC,
   PTT_MSG_SET_RX_IM2_CORRECT = 0x31AD,
   PTT_MSG_GET_RX_IM2_CORRECT = 0x31AE,
   PTT_MSG_TEST_DPD_CAL = 0x32AF,  // not handle
   PTT_MSG_SET_CALCONTROL_BITMAP = 0x32B0,

//[RY] specific new messages for PRIMA
   PTT_MSG_START_WAVEFORM_RF = 0x32B1,
   PTT_MSG_STOP_WAVEFORM_RF = 0x32B2,
   PTT_MSG_HKDAC_TX_IQ_CAL = 0x32B3,
   PTT_MSG_SET_HKADC_TX_IQ_CORRECT = 0x32B4,
   PTT_MSG_GET_HKADC_TX_IQ_CORRECT = 0x32B5,
   PTT_MSG_SET_DPD_CORRECT = 0x32B6,
   PTT_MSG_GET_DPD_CORRECT = 0x32B7,
   PTT_MSG_SET_WAVEFORM_RF = 0x32B8,
   PTT_MSG_LNA_BAND_CAL = 0x32B9,
   PTT_MSG_GET_LNA_BAND_CORRECT = 0x32BA,
   PTT_MSG_SET_LNA_BAND_CORRECT = 0x32BB,
   PTT_MSG_DPD_CAL = 0x32BC,

// Suffix'ed Message ID to differential from existing Message name.
// ===============================================================
   PTT_MSG_GET_NV_TABLE_PRIMA_V1 = 0x32BD,
   PTT_MSG_SET_NV_TABLE_PRIMA_V1 = 0x32BE,
   PTT_MSG_RX_IQ_CAL_PRIMA_V1 = 0x32BF,
   PTT_MSG_TX_IQ_CAL_PRIMA_V1 = 0x32C0,
   PTT_MSG_SET_TX_IQ_CORRECT_PRIMA_V1 = 0x32C1,
   PTT_MSG_GET_TX_IQ_CORRECT_PRIMA_V1 = 0x32C2,
   PTT_MSG_SET_RX_IQ_CORRECT_PRIMA_V1 = 0x32C3,
   PTT_MSG_GET_RX_IQ_CORRECT_PRIMA_V1 = 0x32C4,
   PTT_MSG_START_WAVEFORM_PRIMA_V1 = 0x32C5,
   PTT_MSG_FORCE_PACKET_TX_GAIN_PRIMA_V1 = 0x32C6,
   PTT_MSG_CLPC_CAL_SETUP_PRIMA_V1 = 0x32C7,
   PTT_MSG_CLPC_CAL_RESTORE_PRIMA_V1 = 0x32C8,
   PTT_MSG_CLOSE_TPC_LOOP_PRIMA_V1 = 0x32C9,
   PTT_MSG_SW_CLPC_CAL_PRIMA_V1 = 0x32CA,
   PTT_MSG_CLPC_CAL_EXTRA_MEASUREMENT_PRIMA_V1 = 0x32CB,
   PTT_MSG_PRIMA_GENERIC_CMD = 0x32CC,
   PTT_MSG_DIGITAL_PIN_CONNECTIVITY_TEST_RES = 0X32CD,

   PTT_MSG_EXIT = 0x32ff,
   PTT_MAX_MSG_ID = PTT_MSG_EXIT
} ePttMsgId;

enum 
{
   PTT_MSG_PRIMA_GENERIC_CMD_FAST_SET_CHANNEL = 0x0,
};

#define PTT_MSG_TYPES_BEGIN_30          PTT_MSG_TYPES_BEGIN
#define PTT_MSG_TYPES_BEGIN_31          PTT_MSG_TYPES_BEGIN + 0x100
#define PTT_MSG_TYPES_BEGIN_32          PTT_MSG_TYPES_BEGIN + 0x200

// for FTM PER feature
enum {
Legacy_FTM = 0,
FTM_PER_TX = 1,
FTM_PER_RX = 2,
};

#ifndef tANI_BOOLEAN
#define tANI_BOOLEAN tANI_U8
#endif



/******************************************************************************************************************
    PTT MESSAGES
******************************************************************************************************************/
//Init
typedef PACKED_PRE struct PACKED_POST {
   tPttModuleVariables ptt;
} tMsgPttMsgInit;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 tableSize;
   tANI_U32 chunkSize;
   eNvTable nvTable;
} tMsgPttGetNvTable;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 tableSize;
   tANI_U32 chunkSize;
   eNvTable nvTable;
} tMsgPttSetNvTable;

typedef PACKED_PRE struct PACKED_POST {
   eNvTable nvTable;
} tMsgPttDelNvTable;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttBlankNv;

typedef PACKED_PRE struct PACKED_POST {
   eNvField nvField;
   uNvFields fieldData;
} tMsgPttGetNvField;

typedef PACKED_PRE struct PACKED_POST {
   eNvField nvField;
   uNvFields fieldData;
} tMsgPttSetNvField;

typedef PACKED_PRE struct PACKED_POST {
   eNvTable nvTable;
} tMsgPttStoreNvTable;

typedef PACKED_PRE struct PACKED_POST {
   eRegDomainId regDomainId;
} tMsgPttSetRegDomain;

typedef PACKED_PRE struct PACKED_POST {
	tANI_U32 tableSize;
	tANI_U32 chunkSize;
	eNvTable nvTable;
	tANI_U8 nvData[MAX_NV_BIN_SIZE];
} tMsgPttGetNvBin;

typedef PACKED_PRE struct PACKED_POST {
	tANI_U32 tableSize;
	tANI_U32 chunkSize;
	eNvTable nvTable;
	tANI_U8 nvData[MAX_NV_BIN_SIZE];
} tMsgPttSetNvBin;

//Device Register Access
typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 regAddr;
   tANI_U32 regValue;
} tMsgPttDbgReadRegister;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 regAddr;
   tANI_U32 regValue;
} tMsgPttDbgWriteRegister;

#define PTT_READ_MEM_MAX 512
typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 memAddr;
   tANI_U32 nBytes;
   tANI_U32 pMemBuf[PTT_READ_MEM_MAX]; //caller should allocate space
} tMsgPttDbgReadMemory;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 memAddr;
   tANI_U32 nBytes;
   tANI_U32 pMemBuf[PTT_READ_MEM_MAX];
} tMsgPttDbgWriteMemory;

//Device MAC Test Setup
typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 chId;
   ePhyChanBondState cbState;
} tMsgPttSetChannel;

typedef PACKED_PRE struct PACKED_POST {
   ePhyChainSelect chainSelect;
} tMsgPttEnableChains;

typedef tIQSamples tWaveformSample;

//Tx Waveform Gen Service
typedef PACKED_PRE struct PACKED_POST {
   tWaveformSample waveform[MAX_TEST_WAVEFORM_SAMPLES];
   tANI_U16 numSamples;
   tANI_BOOLEAN clk80;
   tANI_U8 reserved[1];
} tMsgPttSetWaveform;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;
   tANI_U8 gain;
} tMsgPttSetTxWaveformGain;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;
   tANI_U32 gain;
} tMsgPttSetTxWaveformGain_PRIMA_V1;

typedef PACKED_PRE struct PACKED_POST {
   ePhyRxChains rxChain;
   tANI_U8 gain;
} tMsgPttSetRxWaveformGain;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsPowerAdcReadings txPowerAdc;
} tMsgPttGetWaveformPowerAdc;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttStopWaveform;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttClpcCalSetup_PRIMA_V1;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U16 setup_measure;
   tANI_U16 setup_txDmdPwrOffset;
   tANI_U16 measure_totalExtraPt;
   tANI_U16 measure_currentMeasurePtIdx;
   tANI_U8 plut[256];
} tMsgPttClpcCalExtraMeasurement_PRIMA_V1;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttClpcCalRestore_PRIMA_V1;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 startIndex;
   tANI_U32 numSamples;
} tMsgPttStartWaveform;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 startIndex;
   tANI_U32 numSamples;
} tMsgPttStartWaveform_PRIMA_V1;

// Added for PRIMA
typedef PACKED_PRE struct PACKED_POST {
   tWaveformSample waveform[MAX_TEST_WAVEFORM_SAMPLES];
   tANI_U16 numSamples;
   tANI_BOOLEAN clk80;
   tANI_U8 reserved[1];
} tMsgPttSetWaveformRF;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 startIndex;
   tANI_U32 numSamples;
} tMsgPttStartWaveformRF;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttStopWaveformRF;

//Tx Frame Gen Service
typedef PACKED_PRE struct PACKED_POST {
   sPttFrameGenParams frameParams;
} tMsgPttConfigTxPacketGen;

typedef PACKED_PRE struct PACKED_POST {
   tANI_BOOLEAN startStop;
   tANI_U8 reserved[3];
} tMsgPttStartStopTxPacketGen;

typedef PACKED_PRE struct PACKED_POST {
   sTxFrameCounters numFrames;
   tANI_BOOLEAN status;
   tANI_U8 reserved[3];
} tMsgPttQueryTxStatus;

//Tx Frame Power Service
typedef PACKED_PRE struct PACKED_POST {
   tANI_BOOLEAN tpcClose;
   tANI_U8 reserved[3];
} tMsgPttCloseTpcLoop;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 tpcClose;
} tMsgPttCloseTpcLoop_PRIMA_V1;


    //open loop service
typedef PACKED_PRE struct PACKED_POST {

   ePhyTxChains txChain;
   tANI_U8 minIndex;
   tANI_U8 maxIndex;
   tANI_U8 reserved[2];
   tANI_U8 gainTable[TPC_MEM_GAIN_LUT_DEPTH];
} tMsgPttSetPacketTxGainTable;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;
   tANI_U8 gainTable[TPC_MEM_GAIN_LUT_DEPTH];
} tMsgPttGetPacketTxGainTable;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U8 index;
   tANI_U8 reserved[3];
} tMsgPttSetPacketTxGainIndex;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;
   tANI_U8 gain;
   tANI_U8 reserved[3];
} tMsgPttForcePacketTxGain;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;
   tANI_U32 gain;
} tMsgPttForcePacketTxGain_PRIMA_V1;


typedef PACKED_PRE struct PACKED_POST {
   ePowerTempIndexSource indexSource;
} tMsgPttSetPwrIndexSource;

typedef PACKED_PRE struct PACKED_POST {
   t2Decimal dbmPwr;
   tANI_U8 reserved[2];
} tMsgPttSetTxPower;

typedef tTxPowerReport tMsgPttGetTxPowerReport;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;

   tANI_U8 minIndex;
   tANI_U8 maxIndex;
   tANI_U8 reserved[2];

   tANI_U8 powerLut[TPC_MEM_POWER_LUT_DEPTH];
} tMsgPttSetPowerLut;

typedef PACKED_PRE struct PACKED_POST {
   ePhyTxChains txChain;

   tANI_U8 powerLut[TPC_MEM_POWER_LUT_DEPTH];
} tMsgPttGetPowerLut;


//Rx Gain Service
typedef PACKED_PRE struct PACKED_POST {
   sRxChainsAgcDisable gains;
} tMsgPttDisableAgcTables;


typedef PACKED_PRE struct PACKED_POST {
   sRxChainsAgcEnable enables;
} tMsgPttEnableAgcTables;

typedef PACKED_PRE struct PACKED_POST {
   sRxChainsRssi rssi;
} tMsgPttGetRxRssi;

typedef PACKED_PRE struct PACKED_POST {
    sRxChainsRssi rssi;
}tMsgPttGetUnicastMacPktRxRssi;

typedef PACKED_PRE struct PACKED_POST {
    tANI_U32 conf;
}tMsgPttGetUnicastMacPktRxRssiConf_PRIMA_V1;

//Rx Frame Catcher Service
typedef PACKED_PRE struct PACKED_POST {
   sRxTypesDisabled disabled;
} tMsgPttSetRxDisableMode;

typedef PACKED_PRE struct PACKED_POST {
   sRxFrameCounters counters;
} tMsgPttGetRxPktCounts;


typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttResetRxPacketStatistics;





//ADC Sample Service
typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 startSample;        //index of first requested sample, 0 causes new capture
   tANI_U32 numSamples;         //number of samples to transfer to host
   eGrabRamSampleType sampleType;
   tGrabRamSample grabRam[MAX_REQUESTED_GRAB_RAM_SAMPLES];
} tMsgPttGrabRam;


//Phy Calibration Service
typedef PACKED_PRE struct PACKED_POST {
   sRxChainsIQCalValues calValues;
   eGainSteps gain;
} tMsgPttRxIqCal;

typedef PACKED_PRE struct PACKED_POST {
   tRxChainsDcoCorrections calValues;
   tANI_U8 gain;
} tMsgPttRxDcoCal;

typedef PACKED_PRE struct PACKED_POST {
   tRxChainsIm2Corrections calValues;
   eGainSteps gain;
   tANI_U8 im2CalOnly;
} tMsgPttRxIm2Cal;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsLoCorrections calValues;
   tANI_U8 reserve[2];
   eGainSteps gain;
} tMsgPttTxCarrierSuppressCal;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsIQCalValues calValues;
   tANI_U8 reserve[2];
   eGainSteps gain;
} tMsgPttTxIqCal;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsHKIQCalValues calValues;
   eGainSteps gain;
} tMsgPttHKdacTxIqCal;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 unused;
} tMsgPttExecuteInitialCals;

typedef PACKED_PRE struct PACKED_POST {
   sRfHdetCalValues hdetCalValues;
} tMsgPttHdetCal;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U16 clpcMode;
   tANI_U16 txCmdPwr;
   tANI_U16 pwrMax_pwrMin;
   tANI_U16 step;
   tANI_U8 plut[256];
} tMsgPttClpcSwCal_PRIMA_V1;


//Phy Calibration Override Service
typedef PACKED_PRE struct PACKED_POST {
   sTxChainsLoCorrections calValues;
   tANI_U8 reserve[2];
   eGainSteps gain;
} tMsgPttSetTxCarrierSuppressCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsLoCorrections calValues;
   tANI_U8 reserve[2];
   eGainSteps gain;
} tMsgPttGetTxCarrierSuppressCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsIQCalValues calValues;
   tANI_U8 reserve[2];
   eGainSteps gain;
} tMsgPttSetTxIqCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsIQCalValues calValues;
   tANI_U8 reserve[2];
   eGainSteps gain;
} tMsgPttGetTxIqCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsHKIQCalValues calValues;
   eGainSteps gain;
} tMsgPttHKdacSetTxIqCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsHKIQCalValues calValues;
   eGainSteps gain;
} tMsgPttHKdacGetTxIqCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sRxChainsIQCalValues calValues;
   eGainSteps gain;
} tMsgPttSetRxIqCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sRxChainsIQCalValues calValues;
   eGainSteps gain;
} tMsgPttGetRxIqCorrect;

typedef PACKED_PRE struct PACKED_POST {
   tRxChainsDcoCorrections calValues;
   tANI_U8 gain;
} tMsgPttSetRxDcoCorrect;

typedef PACKED_PRE struct PACKED_POST {
   tRxChainsDcoCorrections calValues;
   tANI_U8 gain;
} tMsgPttGetRxDcoCorrect;

typedef PACKED_PRE struct PACKED_POST {
   tRxChainsIm2Corrections calValues;
   tANI_U8 dummy;
} tMsgPttSetRxIm2Correct;

typedef PACKED_PRE struct PACKED_POST {
   tRxChainsIm2Corrections calValues;
   tANI_U8 dummy;
} tMsgPttGetRxIm2Correct;

typedef PACKED_PRE struct PACKED_POST {
   eRfTempSensor tempSensor;
   tTempADCVal tempAdc;
   tANI_U8 reserved[4 - sizeof(tTempADCVal)];
} tMsgPttGetTempAdc;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 addr;
   tANI_U32 mask;
   tANI_U32 shift;
   tANI_U32 value;
} tMsgPttReadRfField;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 addr;
   tANI_U32 mask;
   tANI_U32 shift;
   tANI_U32 value;
} tMsgPttWriteRfField;

//SIF bar4 Register Access
typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 sifRegAddr;
   tANI_U32 sifRegValue;
} tMsgPttReadSifBar4Register;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 sifRegAddr;
   tANI_U32 sifRegValue;
} tMsgPttWriteSifBar4Register;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttDeepSleep;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttEnterFullPower;

//Misc.
typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 notUsed;
} tMsgPttSystemReset;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 cmd;
   tANI_U32 arg1;
   tANI_U32 arg2;
   tANI_U32 arg3;
   tANI_U32 arg4;
} tMsgPttLogDump;

typedef PACKED_PRE struct PACKED_POST {
   sBuildReleaseParams relParams;
} tMsgPttGetBuildReleaseNumber;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 revId;
} tMsgPttGetRFVersion;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 option;             //dummy variable
} tMsgPttCalControlBitmap;

//#ifdef VERIFY_HALPHY_SIMV_MODEL


typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 option;             //dummy variable
} tMsgPttHalPhyInit;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 option;             //dummy variable
} tMsgPttRxIQTest;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsDPDCalValues calValues;
   eGainSteps gain;
} tMsgPttDpdCal;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U8 lutIdx;
   tANI_U8 band;
} tMsgPttStartToneGen;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 option;             //dummy variable
} tMsgPttStopToneGen;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsLnaBandCalValues calValues;
   eGainSteps gain;
} tMsgPttLnaBandCal;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsLnaBandCalValues calValues;
   eGainSteps gain;
} tMsgPttGetLnaBandCalCorrect;

typedef PACKED_PRE struct PACKED_POST {
   sTxChainsLnaBandCalValues calValues;
   eGainSteps gain;
} tMsgPttSetLnaBandCalCorrect;

typedef PACKED_PRE struct PACKED_POST {
    sTxChainsDPDCalValues calValues;
    eGainSteps gain;
}tMsgPttSetDPDCorrect;

typedef PACKED_PRE struct PACKED_POST {
    sTxChainsDPDCalValues calValues;
    eGainSteps gain;
}tMsgPttGetDPDCorrect;

typedef PACKED_PRE struct PACKED_POST {
   tQWPTT_U32 cmdIdx;
   tQWPTT_U32 param1;
   tQWPTT_U32 param2;
   tQWPTT_U32 param3;
   tQWPTT_U32 param4;
} tMsgPttPrimaGenericCmd;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U16 testID;
   tANI_U16 result;
} tMsgPttPinConnTestRes;
//#endif

/******************************************************************************************************************
    END OF PTT MESSAGES
******************************************************************************************************************/

typedef PACKED_PRE union PACKED_POST pttMsgUnion{
//typedef union pttMsgUnion {
   tMsgPttMsgInit MsgInit;
   tMsgPttGetNvTable GetNvTable;
   tMsgPttSetNvTable SetNvTable;
   tMsgPttDelNvTable DelNvTable;
   tMsgPttBlankNv BlankNv;
   tMsgPttStoreNvTable StoreNvTable;
   tMsgPttSetRegDomain SetRegDomain;
   tMsgPttGetNvField GetNvField;
   tMsgPttSetNvField SetNvField;
	tMsgPttGetNvBin GetNvBin;
	tMsgPttSetNvBin SetNvBin;
   tMsgPttDbgReadRegister DbgReadRegister;
   tMsgPttDbgWriteRegister DbgWriteRegister;
   tMsgPttDbgReadMemory DbgReadMemory;
   tMsgPttDbgWriteMemory DbgWriteMemory;
   tMsgPttEnableChains EnableChains;
   tMsgPttSetChannel SetChannel;
   tMsgPttSetWaveform SetWaveform;
   tMsgPttSetTxWaveformGain SetTxWaveformGain;
   tMsgPttSetTxWaveformGain_PRIMA_V1 SetTxWaveformGain_PRIMA_V1;
   tMsgPttGetWaveformPowerAdc GetWaveformPowerAdc;
   tMsgPttStartWaveform StartWaveform;
   tMsgPttStartWaveform_PRIMA_V1 StartWaveform_PRIMA_V1;
   tMsgPttStopWaveform StopWaveform;
   tMsgPttSetRxWaveformGain SetRxWaveformGain;
   tMsgPttConfigTxPacketGen ConfigTxPacketGen;
   tMsgPttStartStopTxPacketGen StartStopTxPacketGen;
   tMsgPttQueryTxStatus QueryTxStatus;
   tMsgPttCloseTpcLoop CloseTpcLoop;
   tMsgPttCloseTpcLoop_PRIMA_V1 CloseTpcLoop_PRIMA_V1;
   tMsgPttSetPacketTxGainTable SetPacketTxGainTable;
   tMsgPttGetPacketTxGainTable GetPacketTxGainTable;
   tMsgPttSetPacketTxGainIndex SetPacketTxGainIndex;
   tMsgPttForcePacketTxGain ForcePacketTxGain;
   tMsgPttForcePacketTxGain_PRIMA_V1 ForcePacketTxGain_PRIMA_V1;
   tMsgPttSetPwrIndexSource SetPwrIndexSource;
   tMsgPttSetTxPower SetTxPower;
   tMsgPttGetTxPowerReport GetTxPowerReport;
   tMsgPttSetPowerLut SetPowerLut;
   tMsgPttGetPowerLut GetPowerLut;
   tMsgPttDisableAgcTables DisableAgcTables;
   tMsgPttEnableAgcTables EnableAgcTables;
   tMsgPttGetRxRssi GetRxRssi;
   tMsgPttGetUnicastMacPktRxRssi GetUnicastMacPktRxRssi;
   tMsgPttGetUnicastMacPktRxRssiConf_PRIMA_V1 GetUnicastMacPktRxRssiConf_PRIMA_V1;
   tMsgPttSetRxDisableMode SetRxDisableMode;
   tMsgPttGetRxPktCounts GetRxPktCounts;
   tMsgPttResetRxPacketStatistics ResetRxPacketStatistics;
   tMsgPttGrabRam GrabRam;
   tMsgPttRxIqCal RxIqCal;
   tMsgPttRxDcoCal RxDcoCal;
   tMsgPttRxIm2Cal RxIm2Cal;

   tMsgPttExecuteInitialCals ExecuteInitialCals;
   tMsgPttTxCarrierSuppressCal TxCarrierSuppressCal;
   tMsgPttTxIqCal TxIqCal;
   tMsgPttHKdacTxIqCal HKdacTxIqCal;
   tMsgPttClpcCalSetup_PRIMA_V1 ClpcCalSetup_PRIMA_V1;
   tMsgPttClpcCalRestore_PRIMA_V1 ClpcCalRestore_PRIMA_V1;
   tMsgPttHdetCal HdetCal;
   tMsgPttClpcSwCal_PRIMA_V1 ClpcSwCal_PRIMA_V1;
   tMsgPttClpcCalExtraMeasurement_PRIMA_V1 ClpcCalExtraMeasurement_PRIMA_V1;
   tMsgPttSetTxCarrierSuppressCorrect SetTxCarrierSuppressCorrect;
   tMsgPttGetTxCarrierSuppressCorrect GetTxCarrierSuppressCorrect;
   tMsgPttSetTxIqCorrect SetTxIqCorrect;
   tMsgPttGetTxIqCorrect GetTxIqCorrect;
   tMsgPttSetRxIqCorrect SetRxIqCorrect;
   tMsgPttGetRxIqCorrect GetRxIqCorrect;
   tMsgPttSetRxDcoCorrect SetRxDcoCorrect;
   tMsgPttGetRxDcoCorrect GetRxDcoCorrect;
   tMsgPttSetRxIm2Correct SetRxIm2Correct;
   tMsgPttGetRxIm2Correct GetRxIm2Correct;
   tMsgPttHKdacSetTxIqCorrect HKdacSetTxIqCorrect;
   tMsgPttHKdacGetTxIqCorrect HKdacGetTxIqCorrect;

   tMsgPttGetTempAdc GetTempAdc;
   tMsgPttReadRfField ReadRfField;
   tMsgPttWriteRfField WriteRfField;
   tMsgPttCalControlBitmap SetCalControlBitmap;

//#ifdef VERIFY_HALPHY_SIMV_MODEL

   tMsgPttHalPhyInit InitOption;
   tMsgPttRxIQTest RxIQTest;
   tMsgPttDpdCal DpdCal;
   tMsgPttStartToneGen StartToneGen;
   tMsgPttStopToneGen StopToneGen;
//#endif
   tMsgPttDeepSleep DeepSleep;
   tMsgPttReadSifBar4Register ReadSifBar4Register;
   tMsgPttWriteSifBar4Register WriteSifBar4Register;
   tMsgPttEnterFullPower EnterFullPower;
   tMsgPttSystemReset SystemReset;
   tMsgPttLogDump LogDump;
   tMsgPttGetBuildReleaseNumber GetBuildReleaseNumber;
   tMsgPttGetRFVersion GetRFVersion;

//[RY] added for PRIMA
   tMsgPttSetWaveformRF SetWaveformRF;
   tMsgPttStopWaveformRF StopWaveformRF;
   tMsgPttStartWaveformRF StartWaveformRF;
   tMsgPttLnaBandCal LnaBandCal;
   tMsgPttGetLnaBandCalCorrect GetLnaBandCalCorrect;
   tMsgPttSetLnaBandCalCorrect SetLnaBandCalCorrect;
   tMsgPttGetDPDCorrect GetDPDCorrect;
   tMsgPttSetDPDCorrect SetDPDCorrect;
   tMsgPttDpdCal DPDCal;
   tMsgPttPrimaGenericCmd PrimaGenericCmd;
   tMsgPttPinConnTestRes PinConnTestRes;
} uPttMsgs;

typedef PACKED_PRE struct PACKED_POST {
   tANI_U16 msgId;
   tANI_U16 msgBodyLength;      //actually, the length of all the fields in this structure
   eQWPttStatus msgResponse;
   uPttMsgs msgBody;
} tPttMsgbuffer, *tpPttMsgbuffer;


typedef PACKED_PRE struct PACKED_POST {
   /*
    * success or failure
    */
   tANI_U32 status;
   tPttMsgbuffer pttMsgBuffer;
} tProcessPttRspParams, *tpProcessPttRspParams;

/* End of Ptt Parameters */

#endif
