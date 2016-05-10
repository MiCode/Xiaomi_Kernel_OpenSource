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
/**=========================================================================
Copyright (c) 2013 Qualcomm Technologies, Inc. All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.
  ========================================================================*/
/** ------------------------------------------------------------------------- *
* Copyright (c) 2012 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
    ------------------------------------------------------------------------- *


    \file wlan_nv2.h

    \brief Types for NV implementation
           Anything that needs to be publicly available should
           be in this file

    $Id$

    Copyright (C) 2006 Airgo Networks, Incorporated


   ========================================================================== */

#if !defined( __WLAN_NV2_H )
#define __WLAN_NV2_H

#include "halLegacyPalTypes.h"
#include "halCompiler.h"
#include "wlan_nv.h"

/* From here, NV2 No CH144 support reduced structure
 * This structure will be used for NV2 backward compatibility */

typedef enum
{
    //2.4GHz Band
    RF_CHAN_1_V2                 = 0,
    RF_CHAN_2_V2                 = 1,
    RF_CHAN_3_V2                 = 2,
    RF_CHAN_4_V2                 = 3,
    RF_CHAN_5_V2                 = 4,
    RF_CHAN_6_V2                 = 5,
    RF_CHAN_7_V2                 = 6,
    RF_CHAN_8_V2                 = 7,
    RF_CHAN_9_V2                 = 8,
    RF_CHAN_10_V2                = 9,
    RF_CHAN_11_V2                = 10,
    RF_CHAN_12_V2                = 11,
    RF_CHAN_13_V2                = 12,
    RF_CHAN_14_V2                = 13,

    //4.9GHz Band
    RF_CHAN_240_V2               = 14,
    RF_CHAN_244_V2               = 15,
    RF_CHAN_248_V2               = 16,
    RF_CHAN_252_V2               = 17,
    RF_CHAN_208_V2               = 18,
    RF_CHAN_212_V2               = 19,
    RF_CHAN_216_V2               = 20,

    //5GHz Low & Mid U-NII Band
    RF_CHAN_36_V2                = 21,
    RF_CHAN_40_V2                = 22,
    RF_CHAN_44_V2                = 23,
    RF_CHAN_48_V2                = 24,
    RF_CHAN_52_V2                = 25,
    RF_CHAN_56_V2                = 26,
    RF_CHAN_60_V2                = 27,
    RF_CHAN_64_V2                = 28,

    //5GHz Mid Band - ETSI & FCC
    RF_CHAN_100_V2               = 29,
    RF_CHAN_104_V2               = 30,
    RF_CHAN_108_V2               = 31,
    RF_CHAN_112_V2               = 32,
    RF_CHAN_116_V2               = 33,
    RF_CHAN_120_V2               = 34,
    RF_CHAN_124_V2               = 35,
    RF_CHAN_128_V2               = 36,
    RF_CHAN_132_V2               = 37,
    RF_CHAN_136_V2               = 38,
    RF_CHAN_140_V2               = 39,

    //5GHz High U-NII Band
    RF_CHAN_149_V2               = 40,
    RF_CHAN_153_V2               = 41,
    RF_CHAN_157_V2               = 42,
    RF_CHAN_161_V2               = 43,
    RF_CHAN_165_V2               = 44,

    //CHANNEL BONDED CHANNELS
    RF_CHAN_BOND_3_V2            = 45,
    RF_CHAN_BOND_4_V2            = 46,
    RF_CHAN_BOND_5_V2            = 47,
    RF_CHAN_BOND_6_V2            = 48,
    RF_CHAN_BOND_7_V2            = 49,
    RF_CHAN_BOND_8_V2            = 50,
    RF_CHAN_BOND_9_V2            = 51,
    RF_CHAN_BOND_10_V2           = 52,
    RF_CHAN_BOND_11_V2           = 53,
    RF_CHAN_BOND_242_V2          = 54,    //4.9GHz Band
    RF_CHAN_BOND_246_V2          = 55,
    RF_CHAN_BOND_250_V2          = 56,
    RF_CHAN_BOND_210_V2          = 57,
    RF_CHAN_BOND_214_V2          = 58,
    RF_CHAN_BOND_38_V2           = 59,    //5GHz Low & Mid U-NII Band
    RF_CHAN_BOND_42_V2           = 60,
    RF_CHAN_BOND_46_V2           = 61,
    RF_CHAN_BOND_50_V2           = 62,
    RF_CHAN_BOND_54_V2           = 63,
    RF_CHAN_BOND_58_V2           = 64,
    RF_CHAN_BOND_62_V2           = 65,
    RF_CHAN_BOND_102_V2          = 66,    //5GHz Mid Band - ETSI & FCC
    RF_CHAN_BOND_106_V2          = 67,
    RF_CHAN_BOND_110_V2          = 68,
    RF_CHAN_BOND_114_V2          = 69,
    RF_CHAN_BOND_118_V2          = 70,
    RF_CHAN_BOND_122_V2          = 71,
    RF_CHAN_BOND_126_V2          = 72,
    RF_CHAN_BOND_130_V2          = 73,
    RF_CHAN_BOND_134_V2          = 74,
    RF_CHAN_BOND_138_V2          = 75,
    RF_CHAN_BOND_151_V2          = 76,    //5GHz High U-NII Band
    RF_CHAN_BOND_155_V2          = 77,
    RF_CHAN_BOND_159_V2          = 78,
    RF_CHAN_BOND_163_V2          = 79,

    NUM_RF_CHANNELS_V2,

    MIN_2_4GHZ_CHANNEL_V2 = RF_CHAN_1_V2,
    MAX_2_4GHZ_CHANNEL_V2 = RF_CHAN_14_V2,

    MIN_5GHZ_CHANNEL_V2 = RF_CHAN_240_V2,
    MAX_5GHZ_CHANNEL_V2 = RF_CHAN_165_V2,
    NUM_5GHZ_CHANNELS_V2 = (MAX_5GHZ_CHANNEL_V2 - MIN_5GHZ_CHANNEL_V2 + 1),

    MIN_20MHZ_RF_CHANNEL_V2 = RF_CHAN_1_V2,
    MAX_20MHZ_RF_CHANNEL_V2 = RF_CHAN_165_V2,
    NUM_20MHZ_RF_CHANNELS_V2 = (MAX_20MHZ_RF_CHANNEL_V2 - MIN_20MHZ_RF_CHANNEL_V2 + 1),

    MIN_40MHZ_RF_CHANNEL_V2 = RF_CHAN_BOND_3_V2,
    MAX_40MHZ_RF_CHANNEL_V2 = RF_CHAN_BOND_163_V2,
    NUM_40MHZ_RF_CHANNELS_V2 = (MAX_40MHZ_RF_CHANNEL_V2 - MIN_40MHZ_RF_CHANNEL_V2 + 1),

    MIN_CB_2_4GHZ_CHANNEL_V2 = RF_CHAN_BOND_3_V2,
    MAX_CB_2_4GHZ_CHANNEL_V2 = RF_CHAN_BOND_11_V2,

    MIN_CB_5GHZ_CHANNEL_V2 = RF_CHAN_BOND_242_V2,
    MAX_CB_5GHZ_CHANNEL_V2 = RF_CHAN_BOND_163_V2,

    NUM_TPC_2_4GHZ_CHANNELS_V2 = 14,
    NUM_TPC_5GHZ_CHANNELS_V2 = NUM_5GHZ_CHANNELS_V2,

    INVALID_RF_CHANNEL_V2 = 0xBAD,
    RF_CHANNEL_INVALID_MAX_FIELD_V2 = 0x7FFFFFFF  /* define as 4 bytes data */
}eRfChannelsV2;

typedef PACKED_PRE struct PACKED_POST
{
    sRegulatoryChannel channels[NUM_RF_CHANNELS_V2];
    uAbsPwrPrecision antennaGain[NUM_RF_SUBBANDS];
    uAbsPwrPrecision bRatePowerOffset[NUM_2_4GHZ_CHANNELS];
    uAbsPwrPrecision gnRatePowerOffset[NUM_RF_CHANNELS_V2];
}ALIGN_4 sRegulatoryDomainsV2;

typedef PACKED_PRE struct PACKED_POST
{
    int16 bRssiOffset[NUM_RF_CHANNELS_V2];
    int16 gnRssiOffset[NUM_RF_CHANNELS_V2];
}ALIGN_4 sRssiChannelOffsetsV2;

typedef PACKED_PRE union PACKED_POST
{
    tRateGroupPwr        pwrOptimum[NUM_RF_SUBBANDS];                         // NV_TABLE_RATE_POWER_SETTINGS
    sRegulatoryDomainsV2   regDomains[NUM_REG_DOMAINS];                         // NV_TABLE_REGULATORY_DOMAINS
    sDefaultCountry      defaultCountryTable;                                 // NV_TABLE_DEFAULT_COUNTRY
    tTpcPowerTable       plutCharacterized[NUM_RF_CHANNELS_V2];                  // NV_TABLE_TPC_POWER_TABLE
    int16             plutPdadcOffset[NUM_RF_CHANNELS_V2];                       // NV_TABLE_TPC_PDADC_OFFSETS
    tRateGroupPwrVR      pwrOptimum_virtualRate[NUM_RF_SUBBANDS];             // NV_TABLE_VIRTUAL_RATE
    sFwConfig            fwConfig;                                             // NV_TABLE_FW_CONFIG
    sRssiChannelOffsetsV2  rssiChanOffsets[2];                                  // NV_TABLE_RSSI_CHANNEL_OFFSETS
    sHwCalValues         hwCalValues;                                         // NV_TABLE_HW_CAL_VALUES
    int16             antennaPathLoss[NUM_RF_CHANNELS_V2];                    // NV_TABLE_ANTENNA_PATH_LOSS
    int16             pktTypePwrLimits[NUM_802_11_MODES][NUM_RF_CHANNELS_V2]; // NV_TABLE_PACKET_TYPE_POWER_LIMITS
    sOfdmCmdPwrOffset    ofdmCmdPwrOffset;                                    // NV_TABLE_OFDM_CMD_PWR_OFFSET
    sTxBbFilterMode      txbbFilterMode;                                      // NV_TABLE_TX_BB_FILTER_MODE
}ALIGN_4 uNvTablesV2;

typedef PACKED_PRE struct PACKED_POST
{
    tRateGroupPwr        pwrOptimum[NUM_RF_SUBBANDS];                         // NV_TABLE_RATE_POWER_SETTINGS
    sRegulatoryDomainsV2   regDomains[NUM_REG_DOMAINS];                         // NV_TABLE_REGULATORY_DOMAINS
    sDefaultCountry      defaultCountryTable;                                 // NV_TABLE_DEFAULT_COUNTRY
    tTpcPowerTable       plutCharacterized[NUM_RF_CHANNELS_V2];                  // NV_TABLE_TPC_POWER_TABLE
    int16             plutPdadcOffset[NUM_RF_CHANNELS_V2];                    // NV_TABLE_TPC_PDADC_OFFSETS
    tRateGroupPwrVR      pwrOptimum_virtualRate[NUM_RF_SUBBANDS];             // NV_TABLE_VIRTUAL_RATE
    sFwConfig           fwConfig;                                              // NV_TABLE_FW_CONFIG
    sRssiChannelOffsetsV2  rssiChanOffsets[2];                                  // NV_TABLE_RSSI_CHANNEL_OFFSETS
    sHwCalValues         hwCalValues;                                         // NV_TABLE_HW_CAL_VALUES
    int16             antennaPathLoss[NUM_RF_CHANNELS_V2];                    // NV_TABLE_ANTENNA_PATH_LOSS
    int16             pktTypePwrLimits[NUM_802_11_MODES][NUM_RF_CHANNELS_V2]; // NV_TABLE_PACKET_TYPE_POWER_LIMITS
    sOfdmCmdPwrOffset    ofdmCmdPwrOffset;                                    // NV_TABLE_OFDM_CMD_PWR_OFFSET
    sTxBbFilterMode      txbbFilterMode;                                      // NV_TABLE_TX_BB_FILTER_MODE
}ALIGN_4 sNvTablesV2;

typedef PACKED_PRE struct PACKED_POST
{
    sNvFields fields;
    sNvTablesV2 tables;
}ALIGN_4 sHalNvV2;

extern const sHalNvV2 nvDefaultsV2;

#endif

