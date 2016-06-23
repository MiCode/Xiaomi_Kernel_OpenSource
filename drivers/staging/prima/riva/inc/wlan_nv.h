/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file wlan_nv.h

    \brief Types for NV implementation
           Anything that needs to be publicly available should
           be in this file

    $Id$


   ========================================================================== */

#if !defined( __WLAN_NV_H )
#define __WLAN_NV_H

#include "halLegacyPalTypes.h"
#include "halCompiler.h"

//From HAL/inc/halNv.h
typedef enum
{
    //Common Nv Fields
    NV_COMMON_PRODUCT_ID,               // 0
    NV_COMMON_PRODUCT_BANDS,            // 1
    NV_COMMON_NUM_OF_TX_CHAINS,         // 2
    NV_COMMON_NUM_OF_RX_CHAINS,         // 3
    NV_COMMON_MAC_ADDR,                 // 4
    NV_COMMON_MFG_SERIAL_NUMBER,        // 5
    NV_COMMON_WLAN_NV_REV_ID,           // 6
    NV_COMMON_COUPLER_TYPE,             // 7
    NV_COMMON_NV_VERSION,               // 8
    NV_COMMON_RESERVED,                 // 9

    NUM_NV_FIELDS,
    NV_MAX_FIELD = 0x7FFFFFFF  /* define as 4 bytes data */

}eNvField;


#define NV_FIELD_MAC_ADDR_SIZE      6
#define NV_FIELD_MFG_SN_SIZE        40
typedef enum
{
    PRODUCT_BAND_11_B_G     = 0,    //Gen6.0 is only this setting
    PRODUCT_BAND_11_A_B_G   = 1,
    PRODUCT_BAND_11_A       = 2,

    NUM_PRODUCT_BANDS,
    NUM_PRODUCT_BANDS_INVALID = 0x7FFFFFFF  /* define as 4 bytes data */
}eNvProductBands;           //NV_COMMON_PRODUCT_BANDS

#define EXTERNAL_PA         1
#define INTERNAL_PA         0

#define EXTERNAL_LNA        1
#define INTERNAL_LNA        0

#define EXTERNAL_COUPLER    1
#define INTERNAL_COUPLER    0

#define EXTERNAL_PDET       1
#define INTERNAL_PDET       0

#define DPD_ENABLED         1
#define DPD_DISABLED        0

#define TPC_MODE_OPEN_LOOP     0
#define TPC_MODE_SCPC          1
#define TPC_MODE_CLPC_MODE2    2
#define TPC_MODE_CLPC_MODE3    3

#define PA_POLARITY_TX_UNUSED   0
#define PA_POLARITY_TX_POSITIVE 1
#define PA_POLARITY_TX_NEGATIVE 2
#define PA_POLARITY_RX_UNUSED   0
#define PA_POLARITY_RX_POSITIVE 1
#define PA_POLARITY_RX_NEGATIVE 2

#define NV_VERSION_INVALID                 0xFF
#define NV_VERSION_11N_11AC_COUPER_TYPE    0
#define NV_VERSION_11N_11AC_FW_CONFIG      1
#define NV_VERSION_LPDC_FW_CONFIG          2
#ifdef FEATURE_WLAN_CH144
#define NV_VERSION_CH144_CONFIG            3
#endif /* FEATURE_WLAN_CH144 */

#ifdef WCN_PRONTO
#ifdef FEATURE_WLAN_CH144
#define WLAN_NV_VERSION     NV_VERSION_CH144_CONFIG
#else
#define WLAN_NV_VERSION     NV_VERSION_LPDC_FW_CONFIG
#endif /* FEATURE_WLAN_CH144 */
#else //WCN_PRONTO
#define WLAN_NV_VERSION     NV_VERSION_11N_11AC_FW_CONFIG
#endif //WCN_PRONTO

typedef PACKED_PRE struct PACKED_POST
{
    uint8   macAddr1[NV_FIELD_MAC_ADDR_SIZE];   /* Default, not change name for compatibility */
    uint8   macAddr2[NV_FIELD_MAC_ADDR_SIZE];
    uint8   macAddr3[NV_FIELD_MAC_ADDR_SIZE];
    uint8   macAddr4[NV_FIELD_MAC_ADDR_SIZE];
} sMacAddr;

typedef PACKED_PRE union PACKED_POST
{
    //common NV fields
    uint16  productId;
    uint8   productBands;
    uint8   wlanNvRevId;
    uint8   numOfTxChains;
    uint8   numOfRxChains;
    sMacAddr macAddr;
    uint8   mfgSN[NV_FIELD_MFG_SN_SIZE];
    uint8   couplerType;
    uint8   nvVersion;
} uNvFields;


//format of common part of nv
typedef PACKED_PRE struct PACKED_POST
{
    //always ensure fields are aligned to 32-bit boundaries
    uint16  productId;
    uint8   productBands;
    uint8   wlanNvRevId; //0: WCN1312, 1: WCN1314, 2: WCN3660

    uint8   numOfTxChains;
    uint8   numOfRxChains;
    uint8   macAddr[NV_FIELD_MAC_ADDR_SIZE];   /* Default, not change name for compatibility */
    uint8   macAddr2[NV_FIELD_MAC_ADDR_SIZE];
    uint8   macAddr3[NV_FIELD_MAC_ADDR_SIZE];
    uint8   macAddr4[NV_FIELD_MAC_ADDR_SIZE];
    uint8   mfgSN[NV_FIELD_MFG_SN_SIZE];
    uint8   couplerType;
    uint8   nvVersion;
} sNvFields;


//From wlanfw/inc/halPhyTypes.h

typedef int8 tPowerdBm;   //power in signed 8-bit integer, no decimal places

typedef PACKED_PRE union PACKED_POST
{
    uint32 measurement;      //measured values can be passed to pttApi, but are maintained to 2 decimal places internally
    int16  reported;         //used internally only - reported values only maintain 2 decimals places
}uAbsPwrPrecision;

typedef enum
{
    PHY_TX_CHAIN_0 = 0,

    NUM_PHY_MAX_TX_CHAINS = 1,
    PHY_MAX_TX_CHAINS = NUM_PHY_MAX_TX_CHAINS,
    PHY_ALL_TX_CHAINS,

    //possible tx chain combinations
    PHY_NO_TX_CHAINS,
    PHY_TX_CHAIN_INVALID = 0x7FFFFFFF  /* define as 4 bytes data */
}ePhyTxChains;

//From wlanfw/inc/halRfTypes.h

typedef enum
{
    REG_DOMAIN_FCC,
    REG_DOMAIN_ETSI,
    REG_DOMAIN_JAPAN,
    REG_DOMAIN_WORLD,
    REG_DOMAIN_N_AMER_EXC_FCC,
    REG_DOMAIN_APAC,
    REG_DOMAIN_KOREA,
    REG_DOMAIN_HI_5GHZ,
    REG_DOMAIN_NO_5GHZ,

    NUM_REG_DOMAINS,
    NUM_REG_DOMAINS_INVALID = 0x7FFFFFFF  /* define as 4 bytes data */
}eRegDomainId;

typedef enum
{
    RF_SUBBAND_2_4_GHZ      = 0,
    RF_SUBBAND_5_LOW_GHZ    = 1,    //Low & Mid U-NII
    RF_SUBBAND_5_MID_GHZ    = 2,    //ETSI
    RF_SUBBAND_5_HIGH_GHZ   = 3,    //High U-NII
    RF_SUBBAND_4_9_GHZ      = 4,    //Japanese


    NUM_RF_SUBBANDS,

    MAX_RF_SUBBANDS,
    INVALID_RF_SUBBAND,

    RF_BAND_2_4_GHZ = 0,
    RF_BAND_5_GHZ = 1,
    NUM_RF_BANDS,
    BOTH_RF_BANDS,
    RF_SUBBAND_INVALID = 0x7FFFFFFF  /* define as 4 bytes data */
}eRfSubBand;

typedef enum
{
    //2.4GHz Band
    RF_CHAN_1                 = 0,
    RF_CHAN_2,
    RF_CHAN_3,
    RF_CHAN_4,
    RF_CHAN_5,
    RF_CHAN_6,
    RF_CHAN_7,
    RF_CHAN_8,
    RF_CHAN_9,
    RF_CHAN_10,
    RF_CHAN_11,
    RF_CHAN_12,
    RF_CHAN_13,
    RF_CHAN_14,

    //4.9GHz Band
    RF_CHAN_240,
    RF_CHAN_244,
    RF_CHAN_248,
    RF_CHAN_252,
    RF_CHAN_208,
    RF_CHAN_212,
    RF_CHAN_216,

    //5GHz Low & Mid U-NII Band
    RF_CHAN_36,
    RF_CHAN_40,
    RF_CHAN_44,
    RF_CHAN_48,
    RF_CHAN_52,
    RF_CHAN_56,
    RF_CHAN_60,
    RF_CHAN_64,

    //5GHz Mid Band - ETSI & FCC
    RF_CHAN_100,
    RF_CHAN_104,
    RF_CHAN_108,
    RF_CHAN_112,
    RF_CHAN_116,
    RF_CHAN_120,
    RF_CHAN_124,
    RF_CHAN_128,
    RF_CHAN_132,
    RF_CHAN_136,
    RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
    RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */
    //5GHz High U-NII Band
    RF_CHAN_149,
    RF_CHAN_153,
    RF_CHAN_157,
    RF_CHAN_161,
    RF_CHAN_165,

    //CHANNEL BONDED CHANNELS
    RF_CHAN_BOND_3,
    RF_CHAN_BOND_4,
    RF_CHAN_BOND_5,
    RF_CHAN_BOND_6,
    RF_CHAN_BOND_7,
    RF_CHAN_BOND_8,
    RF_CHAN_BOND_9,
    RF_CHAN_BOND_10,
    RF_CHAN_BOND_11,
    RF_CHAN_BOND_242,    //4.9GHz Band
    RF_CHAN_BOND_246,
    RF_CHAN_BOND_250,
    RF_CHAN_BOND_210,
    RF_CHAN_BOND_214,
    RF_CHAN_BOND_38,    //5GHz Low & Mid U-NII Band
    RF_CHAN_BOND_42,
    RF_CHAN_BOND_46,
    RF_CHAN_BOND_50,
    RF_CHAN_BOND_54,
    RF_CHAN_BOND_58,
    RF_CHAN_BOND_62,
    RF_CHAN_BOND_102,    //5GHz Mid Band - ETSI & FCC
    RF_CHAN_BOND_106,
    RF_CHAN_BOND_110,
    RF_CHAN_BOND_114,
    RF_CHAN_BOND_118,
    RF_CHAN_BOND_122,
    RF_CHAN_BOND_126,
    RF_CHAN_BOND_130,
    RF_CHAN_BOND_134,
    RF_CHAN_BOND_138,
#ifdef FEATURE_WLAN_CH144
    RF_CHAN_BOND_142,
#endif /* FEATURE_WLAN_CH144 */
    RF_CHAN_BOND_151,    //5GHz High U-NII Band
    RF_CHAN_BOND_155,
    RF_CHAN_BOND_159,
    RF_CHAN_BOND_163,

    NUM_RF_CHANNELS,

    MIN_2_4GHZ_CHANNEL = RF_CHAN_1,
    MAX_2_4GHZ_CHANNEL = RF_CHAN_14,

    MIN_5GHZ_CHANNEL = RF_CHAN_240,
    MAX_5GHZ_CHANNEL = RF_CHAN_165,
    NUM_5GHZ_CHANNELS = (MAX_5GHZ_CHANNEL - MIN_5GHZ_CHANNEL + 1),

    MIN_20MHZ_RF_CHANNEL = RF_CHAN_1,
    MAX_20MHZ_RF_CHANNEL = RF_CHAN_165,
    NUM_20MHZ_RF_CHANNELS = (MAX_20MHZ_RF_CHANNEL - MIN_20MHZ_RF_CHANNEL + 1),

    MIN_40MHZ_RF_CHANNEL = RF_CHAN_BOND_3,
    MAX_40MHZ_RF_CHANNEL = RF_CHAN_BOND_163,
    NUM_40MHZ_RF_CHANNELS = (MAX_40MHZ_RF_CHANNEL - MIN_40MHZ_RF_CHANNEL + 1),

    MIN_CB_2_4GHZ_CHANNEL = RF_CHAN_BOND_3,
    MAX_CB_2_4GHZ_CHANNEL = RF_CHAN_BOND_11,

    MIN_CB_5GHZ_CHANNEL = RF_CHAN_BOND_242,
    MAX_CB_5GHZ_CHANNEL = RF_CHAN_BOND_163,

    NUM_TPC_2_4GHZ_CHANNELS = 14,
    NUM_TPC_5GHZ_CHANNELS = NUM_5GHZ_CHANNELS,

    INVALID_RF_CHANNEL = 0xBAD,
    RF_CHANNEL_INVALID_MAX_FIELD = 0x7FFFFFFF  /* define as 4 bytes data */
}eRfChannels;

typedef enum
{
   RF_CHAN_1_1 = RF_CHAN_1,
   RF_CHAN_2_1 = RF_CHAN_2,
   RF_CHAN_3_1 = RF_CHAN_3,
   RF_CHAN_4_1 = RF_CHAN_4,
   RF_CHAN_5_1 = RF_CHAN_5,
   RF_CHAN_6_1 = RF_CHAN_6,
   RF_CHAN_7_1 = RF_CHAN_7,
   RF_CHAN_8_1 = RF_CHAN_8,
   RF_CHAN_9_1 = RF_CHAN_9,
   RF_CHAN_10_1 = RF_CHAN_10,
   RF_CHAN_11_1 = RF_CHAN_11,
   RF_CHAN_12_1 = RF_CHAN_12,
   RF_CHAN_13_1 = RF_CHAN_13,
   RF_CHAN_14_1 = RF_CHAN_14,
// The above params are used for scripts.
   NUM_2_4GHZ_CHANNELS,
}eRfChannels_2_4GHz;
	
enum
{
   NV_CHANNEL_DISABLE,
   NV_CHANNEL_ENABLE,
   NV_CHANNEL_DFS,
   NV_CHANNEL_INVALID
};
typedef uint8 eNVChannelEnabledType;

typedef PACKED_PRE struct PACKED_POST
{
    eNVChannelEnabledType   enabled;
    tPowerdBm pwrLimit;
}sRegulatoryChannel;

typedef PACKED_PRE struct PACKED_POST
{
    sRegulatoryChannel channels[NUM_RF_CHANNELS];
    uAbsPwrPrecision antennaGain[NUM_RF_SUBBANDS];
    uAbsPwrPrecision bRatePowerOffset[NUM_2_4GHZ_CHANNELS];
    uAbsPwrPrecision gnRatePowerOffset[NUM_RF_CHANNELS];
}ALIGN_4 sRegulatoryDomains;

typedef PACKED_PRE struct PACKED_POST
{
    int16 bRssiOffset[NUM_RF_CHANNELS];
    int16 gnRssiOffset[NUM_RF_CHANNELS];
}ALIGN_4 sRssiChannelOffsets;

typedef PACKED_PRE struct PACKED_POST
{
    uint16 targetFreq;           //number in MHz
    uint16 channelNum;           //channel number as in the eRfChannels enumeration
    eRfSubBand band;               //band that this channel belongs to
}tRfChannelProps;

typedef enum
{
    MODE_802_11B    = 0,
    MODE_802_11AG   = 1,
    MODE_802_11N    = 2,
    NUM_802_11_MODES,
    MODE_802_11_INVALID = 0x7FFFFFFF  /* define as 4 bytes data */
} e80211Modes;

#define HW_CAL_VALUES_VALID_BMAP_UNUSED                             0   //Value
//Bit mask
#define HW_CAL_VALUES_VALID_BMAP_SLEEP_TIME_OVERHEAD_2G_MASK        0x1
#define HW_CAL_VALUES_VALID_BMAP_SLEEP_TIME_OVERHEAD_5G_MASK        0x2
#define HW_CAL_VALUES_VALID_BMAP_SLEEP_TIME_OVERHEAD_xLNA_5G_MASK   0x4
#define HW_CAL_VALUES_VALID_TXBBF_SEL_9MHZ_MASK                     0x8
#define HW_CAL_VALUES_VALID_CUSTOM_TCXO_REG8_MASK                   0x10
#define HW_CAL_VALUES_VALID_CUSTOM_TCXO_REG9_MASK                   0x20
#define HW_CAL_VALUES_VALID_ANTENNA_DIVERSITY_ENABLED_MASK          0x40
#define HW_CAL_VALUES_VALID_CUSTOM_RC_DELAY_MASK                    0x80

//From wlanfw/inc/halPhyCalMemory.h
typedef PACKED_PRE struct PACKED_POST
{
    uint16    psSlpTimeOvrHd2G;
    uint16    psSlpTimeOvrHd5G;

    uint16    psSlpTimeOvrHdxLNA5G;
    uint8     nv_TxBBFSel9MHz       : 1;
    uint8     hwParam1              : 7;
    uint8     hwParam2;
    
    uint16    custom_tcxo_reg8;
    uint16    custom_tcxo_reg9;
    
    uint32    hwParam3;
    uint32    hwParam4;
    uint32    hwParam5;
    uint32    hwParam6;
    uint32    hwParam7;
    uint32    hwParam8;
    uint32    hwParam9;
    uint32    hwParam10;
    uint32    hwParam11;
}sCalData;

typedef PACKED_PRE struct PACKED_POST
{
    uint32 validBmap;  //use eNvCalID
    sCalData calData;
}sHwCalValues;

typedef PACKED_PRE struct PACKED_POST
{
    uint32 txFirFilterMode;
}sTxBbFilterMode;

typedef PACKED_PRE struct PACKED_POST
{
    int16 ofdmPwrOffset;
    int16 rsvd;
}sOfdmCmdPwrOffset;

//From wlanfw/inc/halPhyCfg.h
typedef uint8 tTpcLutValue;

#define MAX_TPC_CAL_POINTS      (8)

typedef uint8 tPowerDetect;        //7-bit power detect reading

typedef PACKED_PRE struct PACKED_POST
{
    tPowerDetect pwrDetAdc;            //= SENSED_PWR register, which reports the 8-bit ADC
                                       // the stored ADC value gets shifted to 7-bits as the index to the LUT
    tPowerDetect adjustedPwrDet;       //7-bit value that goes into the LUT at the LUT[pwrDet] location
                                       //MSB set if extraPrecision.hi8_adjustedPwrDet is used
}tTpcCaldPowerPoint;

typedef tTpcCaldPowerPoint tTpcCaldPowerTable[NUM_PHY_MAX_TX_CHAINS][MAX_TPC_CAL_POINTS];

typedef PACKED_PRE struct PACKED_POST
{
    tTpcCaldPowerTable empirical;                      //calibrated power points
}tTpcConfig;

//From wlanfw/inc/phyTxPower.h
#ifndef TPC_MEM_POWER_LUT_DEPTH
#define TPC_MEM_POWER_LUT_DEPTH 256
#endif

typedef tTpcLutValue tTpcPowerTable[NUM_PHY_MAX_TX_CHAINS][TPC_MEM_POWER_LUT_DEPTH];

typedef PACKED_PRE struct PACKED_POST
{
    tTpcConfig *pwrSampled;             //points to CLPC data in calMemory
}tPhyTxPowerBand;

//From halPhyRates.h
typedef enum
{
    //802.11b Rates
    HAL_PHY_RATE_11B_LONG_1_MBPS,
    HAL_PHY_RATE_11B_LONG_2_MBPS,
    HAL_PHY_RATE_11B_LONG_5_5_MBPS,
    HAL_PHY_RATE_11B_LONG_11_MBPS,
    HAL_PHY_RATE_11B_SHORT_2_MBPS,
    HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
    HAL_PHY_RATE_11B_SHORT_11_MBPS,

    //Spica_Virgo 11A 20MHz Rates
    HAL_PHY_RATE_11A_6_MBPS,
    HAL_PHY_RATE_11A_9_MBPS,
    HAL_PHY_RATE_11A_12_MBPS,
    HAL_PHY_RATE_11A_18_MBPS,
    HAL_PHY_RATE_11A_24_MBPS,
    HAL_PHY_RATE_11A_36_MBPS,
    HAL_PHY_RATE_11A_48_MBPS,
    HAL_PHY_RATE_11A_54_MBPS,

    // 11A 20MHz Rates
    HAL_PHY_RATE_11A_DUP_6_MBPS,
    HAL_PHY_RATE_11A_DUP_9_MBPS,
    HAL_PHY_RATE_11A_DUP_12_MBPS,
    HAL_PHY_RATE_11A_DUP_18_MBPS,
    HAL_PHY_RATE_11A_DUP_24_MBPS,
    HAL_PHY_RATE_11A_DUP_36_MBPS,
    HAL_PHY_RATE_11A_DUP_48_MBPS,
    HAL_PHY_RATE_11A_DUP_54_MBPS,

    //MCS Index #0-7 (20/40MHz)
    HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
    HAL_PHY_RATE_MCS_1NSS_13_MBPS,
    HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
    HAL_PHY_RATE_MCS_1NSS_26_MBPS,
    HAL_PHY_RATE_MCS_1NSS_39_MBPS,
    HAL_PHY_RATE_MCS_1NSS_52_MBPS,
    HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
    HAL_PHY_RATE_MCS_1NSS_65_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

    //MCS Index #8-15 (20/40MHz)
    HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
    HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
    HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATURE_11AC
    /*11A duplicate 80MHz Rates*/
    HAL_PHY_RATE_11AC_DUP_6_MBPS,
    HAL_PHY_RATE_11AC_DUP_9_MBPS,
    HAL_PHY_RATE_11AC_DUP_12_MBPS,
    HAL_PHY_RATE_11AC_DUP_18_MBPS,
    HAL_PHY_RATE_11AC_DUP_24_MBPS,
    HAL_PHY_RATE_11AC_DUP_36_MBPS,
    HAL_PHY_RATE_11AC_DUP_48_MBPS,
    HAL_PHY_RATE_11AC_DUP_54_MBPS,

    /*11AC rate 20MHZ Normal GI*/
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_6_5_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_13_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_19_5_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_26_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_39_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_52_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_58_5_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_65_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_78_MBPS,
#ifdef WCN_PRONTO
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
    
    /*11AC rate 20MHZ Shortl GI*/
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_7_2_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_14_4_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_21_6_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_28_8_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_43_3_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_57_7_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_65_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_72_2_MBPS,
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
    HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
    
    /*11AC rates 40MHZ normal GI*/
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS ,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
    
    /*11AC rates 40MHZ short GI*/
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS ,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
    HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,
    
    /*11AC rates 80 MHZ normal GI*/
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS ,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
    
    /*11AC rates 80 MHZ short GI*/
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS ,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
    HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif //WLAN_FEATURE_11AC

    NUM_HAL_PHY_RATES,
    HAL_PHY_RATE_INVALID,
    MIN_RATE_INDEX = 0,
    MAX_RATE_INDEX = NUM_HAL_PHY_RATES - 1,
    HAL_PHY_RATE_INVALID_MAX_FIELD = 0x7FFFFFFF  /* define as 4 bytes data */
}eHalPhyRates;

#define NUM_RATE_POWER_GROUPS           NUM_HAL_PHY_RATES  //total number of rate power groups including the CB_RATE_POWER_OFFSET
typedef uAbsPwrPrecision tRateGroupPwr[NUM_HAL_PHY_RATES];

//From halNvTables.h
#define NV_FIELD_COUNTRY_CODE_SIZE  3
typedef PACKED_PRE struct PACKED_POST
{
    uint8 regDomain;                                  //from eRegDomainId
    uint8 countryCode[NV_FIELD_COUNTRY_CODE_SIZE];    // string identifier
}sDefaultCountry;


#define GF_PA_BIAS_SELECT_MASK         0X7 //(3 bits)
#define TSMC_PA_BIAS_SELECT_MASK       0x7 //(3 bits)

#define GF_PA_BIAS_SELECT_1            0X0
#define GF_PA_BIAS_SELECT_2            0X1

#define TSMC_PA_BIAS_SELECT_1          0X0
#define TSMC_PA_BIAS_SELECT_2          0X1
#define TSMC_PA_BIAS_SELECT_3          0x2


#define EXT_PA_CTRL_POLARITY_DEFAULT   0X0
#define EXT_PA_CTRL_POLARITY_VALID     0X80

#define EXT_PA_CTRL0_POLARITY_MASK     0X3
#define EXT_PA_CTRL0_POLARITY_OFFSET   0X0
#define EXT_PA_CTRL1_POLARITY_MASK     0XC
#define EXT_PA_CTRL1_POLARITY_OFFSET   0X2

#define EXT_PA_CTRL_POLARITY_ZERO      0X1
#define EXT_PA_CTRL_POLARITY_ONE       0X2

#define GF_TX_PWR_ADJUST_2G_MASK          0XF0000
#define GF_TX_PWR_ADJUST_2G_OFFSET        16
#define GF_TX_PWR_ADJUST_5G_LOW_MASK      0XF00000
#define GF_TX_PWR_ADJUST_5G_LOW_OFFSET    20
#define GF_TX_PWR_ADJUST_5G_MID_MASK      0XF000000
#define GF_TX_PWR_ADJUST_5G_MID_OFFSET    24
#define GF_TX_PWR_ADJUST_5G_HIGH_MASK     0XF0000000
#define GF_TX_PWR_ADJUST_5G_HIGH_OFFSET   28


typedef PACKED_PRE struct PACKED_POST
{
    uint8 skuID; 
    uint8 tpcMode2G;
    uint8 tpcMode5G;
    uint8 configItem1;

    uint8 xPA2G;
    uint8 xPA5G;
    uint8 extPaCtrl0Polarity;
    uint8 extPaCtrl1Polarity;

    uint8 xLNA2G;
    uint8 xLNA5G;
    uint8 xCoupler2G;
    uint8 xCoupler5G;

    uint8 xPdet2G;
    uint8 xPdet5G;
    uint8 enableDPD2G;
    uint8 enableDPD5G;

    uint8 pdadcSelect2G;
    uint8 pdadcSelect5GLow;
    uint8 pdadcSelect5GMid;
    uint8 pdadcSelect5GHigh;

    uint32 configItem2;
    uint32 configItem3;
    uint32 configItem4;
}sFwConfig;


#define NUM_RF_VR_RATE   13
typedef uAbsPwrPrecision tRateGroupPwrVR[NUM_RF_VR_RATE];

typedef PACKED_PRE union PACKED_POST
{
    tRateGroupPwr        pwrOptimum[NUM_RF_SUBBANDS];                         // NV_TABLE_RATE_POWER_SETTINGS
    sRegulatoryDomains   regDomains[NUM_REG_DOMAINS];                         // NV_TABLE_REGULATORY_DOMAINS
    sDefaultCountry      defaultCountryTable;                                 // NV_TABLE_DEFAULT_COUNTRY
    tTpcPowerTable       plutCharacterized[NUM_RF_CHANNELS];                  // NV_TABLE_TPC_POWER_TABLE
    int16             plutPdadcOffset[NUM_RF_CHANNELS];                       // NV_TABLE_TPC_PDADC_OFFSETS
    tRateGroupPwrVR      pwrOptimum_virtualRate[NUM_RF_SUBBANDS];             // NV_TABLE_VIRTUAL_RATE
    sFwConfig            fwConfig;                                             // NV_TABLE_FW_CONFIG
    sRssiChannelOffsets  rssiChanOffsets[2];                                  // NV_TABLE_RSSI_CHANNEL_OFFSETS
    sHwCalValues         hwCalValues;                                         // NV_TABLE_HW_CAL_VALUES
    int16             antennaPathLoss[NUM_RF_CHANNELS];                    // NV_TABLE_ANTENNA_PATH_LOSS
    int16             pktTypePwrLimits[NUM_802_11_MODES][NUM_RF_CHANNELS]; // NV_TABLE_PACKET_TYPE_POWER_LIMITS
    sOfdmCmdPwrOffset    ofdmCmdPwrOffset;                                    // NV_TABLE_OFDM_CMD_PWR_OFFSET
    sTxBbFilterMode      txbbFilterMode;                                      // NV_TABLE_TX_BB_FILTER_MODE
}ALIGN_4 uNvTables;

//From halPhy.h
typedef tPowerdBm tChannelPwrLimit;

typedef PACKED_PRE struct PACKED_POST
{
    uint8 chanId;
    tChannelPwrLimit pwr;
} ALIGN_4 tChannelListWithPower;

//From HAL/inc/halNvTables.h
typedef enum
{
    NV_FIELDS_IMAGE                 = 0,    //contains all fields

    NV_TABLE_RATE_POWER_SETTINGS    = 2,
    NV_TABLE_REGULATORY_DOMAINS     = 3,
    NV_TABLE_DEFAULT_COUNTRY        = 4,
    NV_TABLE_TPC_POWER_TABLE        = 5,
    NV_TABLE_TPC_PDADC_OFFSETS      = 6,
    NV_TABLE_HW_CAL_VALUES          = 7,
    NV_TABLE_RSSI_CHANNEL_OFFSETS   = 9,
    NV_TABLE_CAL_MEMORY             = 10,    //cal memory structure from halPhyCalMemory.h preceded by status
    NV_TABLE_FW_CONFIG              = 11,
    NV_TABLE_ANTENNA_PATH_LOSS          = 12,
    NV_TABLE_PACKET_TYPE_POWER_LIMITS   = 13,
    NV_TABLE_OFDM_CMD_PWR_OFFSET        = 14,
    NV_TABLE_TX_BB_FILTER_MODE          = 15,
    NV_TABLE_VIRTUAL_RATE               = 18,

    NUM_NV_TABLE_IDS,
    NV_ALL_TABLES                   = 0xFFF,
    NV_BINARY_IMAGE                 = 0x1000,
    NV_MAX_TABLE                    = 0x7FFFFFFF  /* define as 4 bytes data */
}eNvTable;

typedef PACKED_PRE struct PACKED_POST
{
    tRateGroupPwr        pwrOptimum[NUM_RF_SUBBANDS];                         // NV_TABLE_RATE_POWER_SETTINGS
    sRegulatoryDomains   regDomains[NUM_REG_DOMAINS];                         // NV_TABLE_REGULATORY_DOMAINS
    sDefaultCountry      defaultCountryTable;                                 // NV_TABLE_DEFAULT_COUNTRY
    tTpcPowerTable       plutCharacterized[NUM_RF_CHANNELS];                  // NV_TABLE_TPC_POWER_TABLE
    int16             plutPdadcOffset[NUM_RF_CHANNELS];                    // NV_TABLE_TPC_PDADC_OFFSETS
    tRateGroupPwrVR      pwrOptimum_virtualRate[NUM_RF_SUBBANDS];             // NV_TABLE_VIRTUAL_RATE
    sFwConfig           fwConfig;                                              // NV_TABLE_FW_CONFIG
    sRssiChannelOffsets  rssiChanOffsets[2];                                  // NV_TABLE_RSSI_CHANNEL_OFFSETS
    sHwCalValues         hwCalValues;                                         // NV_TABLE_HW_CAL_VALUES
    int16             antennaPathLoss[NUM_RF_CHANNELS];                    // NV_TABLE_ANTENNA_PATH_LOSS
    int16             pktTypePwrLimits[NUM_802_11_MODES][NUM_RF_CHANNELS]; // NV_TABLE_PACKET_TYPE_POWER_LIMITS
    sOfdmCmdPwrOffset    ofdmCmdPwrOffset;                                    // NV_TABLE_OFDM_CMD_PWR_OFFSET
    sTxBbFilterMode      txbbFilterMode;                                      // NV_TABLE_TX_BB_FILTER_MODE
}ALIGN_4 sNvTables;

typedef PACKED_PRE struct PACKED_POST
{
    sNvFields fields;
    sNvTables tables;
}ALIGN_4 sHalNv;

extern const sHalNv nvDefaults;

#endif

