/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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


    \file wlan_nv.c

   \brief Contains collection of table default values to use in
          case a table is not found in NV

    $Id$

   ========================================================================== */

#ifndef WLAN_NV_C
#define WLAN_NV_C

#include "wlan_nv2.h"
#include "wlan_hal_msg.h"

const sHalNv nvDefaults =
{
    {
        0,                                                              // tANI_U16  productId;
        1,                                                              // tANI_U8   productBands;
        2,                                                              // tANI_U8   wlanNvRevId; //0: WCN1312, 1: WCN1314, 2: WCN3660
        1,                                                              // tANI_U8   numOfTxChains;
        1,                                                              // tANI_U8   numOfRxChains;
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { "\0" },
        0,                                                              // tANI_U8   couplerType;
        WLAN_NV_VERSION,                                                // tANI_U8   nvVersion;
    }, //fields

    {
        // NV_TABLE_RATE_POWER_SETTINGS
        {
            // typedef tANI_S16 tPowerdBm;
            //typedef tPowerdBm tRateGroupPwr[NUM_HAL_PHY_RATES];
            //tRateGroupPwr       pwrOptimum[NUM_RF_SUBBANDS];
            //2.4G
            {
                //802.11b Rates
                {1900},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {1900},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {1900},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {1900},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {1900},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {1900},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {1900},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                //11A 20MHz Rates
                {1700},    // HAL_PHY_RATE_11A_6_MBPS,
                {1700},    // HAL_PHY_RATE_11A_9_MBPS,
                {1700},    // HAL_PHY_RATE_11A_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_18_MBPS,
                {1600},    // HAL_PHY_RATE_11A_24_MBPS,
                {1550},    // HAL_PHY_RATE_11A_36_MBPS,
                {1550},    // HAL_PHY_RATE_11A_48_MBPS,
                {1550},    // HAL_PHY_RATE_11A_54_MBPS,

                //DUP 11A 40MHz Rates
                {1700},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11A_DUP_54_MBPS,
                
                //MCS Index #0-7(20/40MHz)
                {1700},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1300},     // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                //MCS Index #8-15(20/40MHz)
                {1700},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATURE_11AC
                //11AC rates
               //11A duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               //11ac 20MHZ NG, SG
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif

               //11ac 40MHZ NG, SG
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,

               //11ac 80MHZ NG, SG
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_2_4_GHZ
            // 5G Low
            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DUP 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_11AC
                ///11AC rates
               ///11A duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11ac 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               //11ac 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_5_LOW_GHZ
            // 5G Mid
            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DU P 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCSS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCSS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_111AC
                ///11CAC rates
               ///11Ad duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11a c 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               ///11a c 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    //     RF_SUBBAND_5_MID_GHZ
            // 5G High
            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DU P 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCSS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCSS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_11AC
                ///11CAC rates
               ///11Ad duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11a c 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               ///11a c 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_5_HIGH_GHZ,
            // 4.9G

            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DU P 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCSS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCSS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_11AC
                ///11CAC rates
               ///11Ad duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11a c 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               ///11a c 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_4_9_GHZ
        },

        // NV_TABLE_REGULATORY_DOMAINS
        {
            // typedef struct
            // {
            //     tANI_BOOLEAN enabled;
            //     tPowerdBm pwrLimit;
            // }sRegulatoryChannel;

            // typedef struct
            // {
            //     sRegulatoryChannel channels[NUM_RF_CHANNELS];
            //     uAbsPwrPrecision antennaGain[NUM_RF_SUBBANDS];
            //     uAbsPwrPrecision bRatePowerOffset[NUM_2_4GHZ_CHANNELS];
            // }sRegulatoryDomains;

            //sRegulatoryDomains  regDomains[NUM_REG_DOMAINS];


            {   // REG_DOMAIN_FCC start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 22},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 22},           //RF_CHAN_11,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_12,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_48,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_52,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_56,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_60,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DFS, 22},                //RF_CHAN_100,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_104,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_108,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_112,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_128,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_132,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_136,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},             //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */
                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    {0},  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_FCC end

            {   // REG_DOMAIN_ETSI start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 19},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 19},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_149,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_153,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_157,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_161,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_ETSI end

            {   // REG_DOMAIN_JAPAN start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_13,
                    {NV_CHANNEL_ENABLE, 18},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_52,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_56,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_60,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DFS, 22},               //RF_CHAN_100,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_104,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_108,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_112,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_116,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_120,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_124,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_128,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_132,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_136,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_JAPAN end

            {   // REG_DOMAIN_WORLD start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                                       //2.4GHz Band
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_52,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_56,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_60,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DFS, 22},               //RF_CHAN_100,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_104,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_108,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_112,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_116,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_120,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_124,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_128,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_132,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_136,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},           //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */
                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    {0},   // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_WORLD end

            {   // REG_DOMAIN_N_AMER_EXC_FCC start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_11,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_12,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            },   // REG_DOMAIN_N_AMER_EXC_FCC end

            {   // REG_DOMAIN_APAC start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    {0},   // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_APAC end

            {   // REG_DOMAIN_KOREA start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    {0},   // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_KOREA end

            {   // REG_DOMAIN_HI_5GHZ start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    {0},   // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_HI_5GHZ end

            {   // REG_DOMAIN_NO_5GHZ start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                                       //2.4GHz Band
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,
                }, //sRegulatoryChannel end

                {
                    {0},   // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            } // REG_DOMAIN_NO_5GHZ end
        },

        // NV_TABLE_DEFAULT_COUNTRY
        {
            // typedef struct
            // {
            //     tANI_U8 regDomain;                                      //from eRegDomainId
            //     tANI_U8 countryCode[NV_FIELD_COUNTRY_CODE_SIZE];    // string identifier
            // }sDefaultCountry;

            0,                  // regDomain
            { 'U', 'S', 'I' }   // countryCode
        },

        //NV_TABLE_TPC_POWER_TABLE
        {
            //ch 1
            {
                {
                    0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 2 , 2 , 3 , 3 , 4 , 5 , 5 , 6 , 7 , 8 , 8 , 9 , 10 , 10 , 11 , 13 , 14 , 15 , 17 ,
                    19 , 20 , 21 , 22 , 23 , 25 , 26 , 27 , 28 , 29 , 30 , 30 , 31 , 32 , 33 , 34 , 35 , 35 , 36 , 37 , 38 , 39 , 40 ,
                    40 , 41 , 42 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 48 , 48 , 49 , 49 , 50 , 50 , 51 , 51 , 52 , 52 , 53 , 53 ,
                    54 , 54 , 55 , 55 , 56 , 56 , 57 , 57 , 58 , 58 , 58 , 59 , 59 , 59 , 60 , 60 , 60 , 61 , 61 , 61 , 62 , 62 , 62 ,
                    63 , 63 , 63 , 64 , 64 , 65 , 65 , 65 , 66 , 66 , 66 , 67 , 67 , 67 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 70 , 70 ,
                    70 , 70 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 76 ,
                    76 , 76 , 76 , 76 , 77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 81 ,
                    81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 83 , 84 , 84 , 84 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    85 , 85 , 85 , 86 , 86 , 86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 88 , 88 , 89 ,
                    89 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 90 , 90 , 90 , 91 , 91 , 91 , 91 , 91 , 91 , 92 , 92 , 92 , 92 , 92 ,
                    92 , 92 , 93 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 95 , 95 , 96 , 96
                }
            }, //RF_CHAN_1

            //ch 2
            {
                {
                    0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 2 , 2 , 3 , 3 , 4 , 5 , 5 , 6 , 7 , 8 , 8 , 9 , 10 , 10 , 11 , 13 , 14 , 15 , 17 ,
                    19 , 20 , 21 , 22 , 23 , 25 , 26 , 27 , 28 , 29 , 30 , 30 , 31 , 32 , 33 , 34 , 35 , 35 , 36 , 37 , 38 , 39 , 40 ,
                    40 , 41 , 42 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 48 , 48 , 49 , 49 , 50 , 50 , 51 , 51 , 52 , 52 , 53 , 53 ,
                    54 , 54 , 55 , 55 , 56 , 56 , 57 , 57 , 58 , 58 , 58 , 59 , 59 , 59 , 60 , 60 , 60 , 61 , 61 , 61 , 62 , 62 , 62 ,
                    63 , 63 , 63 , 64 , 64 , 65 , 65 , 65 , 66 , 66 , 66 , 67 , 67 , 67 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 70 , 70 ,
                    70 , 70 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 76 ,
                    76 , 76 , 76 , 76 , 77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 81 ,
                    81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 83 , 84 , 84 , 84 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    85 , 85 , 85 , 86 , 86 , 86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 88 , 88 , 89 ,
                    89 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 90 , 90 , 90 , 91 , 91 , 91 , 91 , 91 , 91 , 92 , 92 , 92 , 92 , 92 ,
                    92 , 92 , 93 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 95 , 95 , 96 , 96
                }
            }, //RF_CHAN_2

            //ch 3
            {
                {
                    0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 2 , 2 , 3 , 3 , 4 , 5 , 5 , 6 , 7 , 8 , 8 , 9 , 10 , 10 , 11 , 13 , 14 , 15 , 17 ,
                    19 , 20 , 21 , 22 , 23 , 25 , 26 , 27 , 28 , 29 , 30 , 30 , 31 , 32 , 33 , 34 , 35 , 35 , 36 , 37 , 38 , 39 , 40 ,
                    40 , 41 , 42 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 48 , 48 , 49 , 49 , 50 , 50 , 51 , 51 , 52 , 52 , 53 , 53 ,
                    54 , 54 , 55 , 55 , 56 , 56 , 57 , 57 , 58 , 58 , 58 , 59 , 59 , 59 , 60 , 60 , 60 , 61 , 61 , 61 , 62 , 62 , 62 ,
                    63 , 63 , 63 , 64 , 64 , 65 , 65 , 65 , 66 , 66 , 66 , 67 , 67 , 67 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 70 , 70 ,
                    70 , 70 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 76 ,
                    76 , 76 , 76 , 76 , 77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 81 ,
                    81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 83 , 84 , 84 , 84 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    85 , 85 , 85 , 86 , 86 , 86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 88 , 88 , 89 ,
                    89 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 90 , 90 , 90 , 91 , 91 , 91 , 91 , 91 , 91 , 92 , 92 , 92 , 92 , 92 ,
                    92 , 92 , 93 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 95 , 95 , 96 , 96
                }
            }, //RF_CHAN_3

            //ch 4
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_4

            //ch 5
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_5

            //ch 6
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
             }
           }, //RF_CHAN_6

            //ch 7
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_7

            //ch 8
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_8

            //ch 9
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_9

            //ch 10
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_10

            //ch 11
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_11

            //ch 12
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_12

            //ch 13
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_13

            //ch 14
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_14

            //5200 base: ch240
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_240

            //5200 base: ch244
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_244

            //5200 base: ch248
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_248

            //5200 base: ch252
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_252

            //5200 base: ch208
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_208

            //5200 base: ch212
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_212

            //5200 base: ch216
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_216

            //5200 base: ch36
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_36

            //5200 base: ch40
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_40

            //5200 base: ch44
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_44

            //5200 base: ch48
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_48

            //5200 base: ch52
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_52

            //5200 base: ch56
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_56

            //5500: ch 60
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_60

           //5500: ch 64
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_64

            //5500: ch 100
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_100

            //5500: ch 104
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_104

           //5500: ch 108
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_108

            //5500: ch 112
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_112

            //5500: ch 116
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_116

            //5500: ch 120
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_120

            //5500: ch 124
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_124

            //5500: ch 128
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_128

            //5500: ch 132
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_132

            //5500: ch 136
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_136

            //5500: ch 140
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_140

#ifdef FEATURE_WLAN_CH144
            //5500: ch 144
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_144
#endif /* FEATURE_WLAN_CH144 */

            //5500: ch 149
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_149

            //5500: ch 153
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_153

            //5500: ch 157
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_157

            //5500: ch 161
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_161

            //5500: ch 165
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_165

            // CB starts
            //ch 3
            {
                {
                    0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 2 , 2 , 3 , 3 , 4 , 5 , 5 , 6 , 7 , 8 , 8 , 9 , 10 , 10 , 11 , 13 , 14 , 15 , 17 ,
                    19 , 20 , 21 , 22 , 23 , 25 , 26 , 27 , 28 , 29 , 30 , 30 , 31 , 32 , 33 , 34 , 35 , 35 , 36 , 37 , 38 , 39 , 40 ,
                    40 , 41 , 42 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 48 , 48 , 49 , 49 , 50 , 50 , 51 , 51 , 52 , 52 , 53 , 53 ,
                    54 , 54 , 55 , 55 , 56 , 56 , 57 , 57 , 58 , 58 , 58 , 59 , 59 , 59 , 60 , 60 , 60 , 61 , 61 , 61 , 62 , 62 , 62 ,
                    63 , 63 , 63 , 64 , 64 , 65 , 65 , 65 , 66 , 66 , 66 , 67 , 67 , 67 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 70 , 70 ,
                    70 , 70 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 76 ,
                    76 , 76 , 76 , 76 , 77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 81 ,
                    81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 83 , 84 , 84 , 84 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    85 , 85 , 85 , 86 , 86 , 86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 88 , 88 , 89 ,
                    89 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 90 , 90 , 90 , 91 , 91 , 91 , 91 , 91 , 91 , 92 , 92 , 92 , 92 , 92 ,
                    92 , 92 , 93 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 95 , 95 , 96 , 96
                }
            }, //RF_CHAN_BOND_3

            //ch 4
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_BOND_4

            //ch 5
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                     33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_BOND_5

            //ch 6
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                     86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_BOND_6

            //ch 7
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_BOND_7

            //ch 8
            {
                {
                    10 , 10 , 11 , 12 , 12 , 13 , 14 , 15 , 15 , 16 , 17 , 17 , 18 , 19 , 20 , 20 , 22 , 23 , 25 , 26 , 28 , 29 , 31 ,
                    33 , 34 , 36 , 37 , 38 , 40 , 41 , 42 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 52 , 53 , 54 , 55 , 55 , 56 ,
                    57 , 57 , 58 , 59 , 59 , 60 , 61 , 61 , 62 , 62 , 63 , 64 , 64 , 65 , 66 , 66 , 67 , 67 , 68 , 68 , 69 , 69 , 70 ,
                    70 , 71 , 71 , 71 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 75 , 75 , 76 , 76 , 76 , 77 , 77 , 77 , 78 ,
                    78 , 78 , 79 , 79 , 79 , 80 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 84 , 85 , 85 , 85 ,
                    86 , 86 , 86 , 86 , 87 , 87 , 87 , 87 , 88 , 88 , 88 , 88 , 88 , 89 , 89 , 89 , 89 , 90 , 90 , 90 , 90 , 91 , 91 ,
                    91 , 91 , 91 , 92 , 92 , 92 , 92 , 93 , 93 , 93 , 93 , 93 , 94 , 94 , 94 , 94 , 95 , 95 , 95 , 95 , 96 , 96 , 96 ,
                    96 , 97 , 97 , 97 , 97 , 97 , 98 , 98 , 98 , 98 , 98 , 98 , 98 , 99 , 99 , 99 , 99 , 99 , 99 , 100 , 100 , 100 ,
                    100 , 101 , 101 , 101 , 101 , 101 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 102 , 103 , 103 , 103 , 103 ,
                    103 , 103 , 104 , 104 , 104 , 104 , 104 , 105 , 105 , 105 , 105 , 105 , 106 , 106 , 106 , 106 , 106 , 106 , 107 ,
                    107 , 107 , 107 , 107 , 107 , 107 , 108 , 108 , 108 , 108 , 108 , 108 , 109 , 109 , 109 , 109 , 109 , 109 , 109 ,
                    110 , 110 , 110 , 110 , 110 , 110 , 110 , 110 , 111 , 111 , 111 , 111 , 111 , 112 , 112 , 112
                }
            }, //RF_CHAN_BOND_8

            //ch 9
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                    }
            }, //RF_CHAN_BOND_9

            //ch 10
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_BOND_10

            //ch 11
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 2 , 2 , 3 , 5 , 6 , 7 , 9 , 10 , 11 , 13 ,
                    14 , 15 , 16 , 17 , 18 , 20 , 21 , 22 , 22 , 23 , 23 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 29 , 29 ,
                    30 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 34 , 35 , 36 , 36 , 37 , 37 , 38 , 38 , 39 , 39 , 40 , 40 , 41 , 41 ,
                    42 , 42 , 43 , 43 , 44 , 44 , 45 , 45 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 50 , 51 , 51 ,
                    51 , 51 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 ,
                    58 , 58 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 ,
                    64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 69 ,
                    69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 ,
                    73 , 73 , 74 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 , 75 , 76 , 76 , 76 , 76 , 76 , 76 , 76 , 77 , 77 ,
                    77 , 77 , 77 , 77 , 78 , 78 , 78 , 78 , 78 , 78 , 78 , 79 , 79 , 79 , 79 , 79 , 79 , 80 , 80 , 80 , 80 , 80 , 80 ,
                    81 , 81 , 81 , 81 , 81 , 81 , 81 , 82 , 82 , 82 , 82 , 82 , 82 , 83 , 83 , 83 , 83 , 84 , 84 , 85 , 85
                }
            }, //RF_CHAN_BOND_11

            //5200 base: ch242
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_242

            //5200 base: ch246
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_246

            //5200 base: ch250
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_250

            //5200 base: ch210
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_210

            //5200 base: ch214
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_214

            //5200 base: ch38
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_38

            //5200 base: ch42
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_42

            //5200 base: ch46
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_46

            //5200 base: ch50
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_50

            //5200 base: ch54
            {
                {
                    0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 6,
                    6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 10 , 11 , 11 , 11 , 11 , 12 ,
                    12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 18 , 19 , 19 ,
                    19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 26 ,
                    26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 31 , 31 , 31 ,
                    31 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 ,
                    37 , 37 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 ,
                    40 , 40 , 41 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 ,
                    44 , 44 , 45 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 47 , 47 , 48 , 48 ,
                    48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 ,
                    51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 54 , 55 , 56 , 57 , 57 , 58 , 59 , 60
                }
            }, //RF_CHAN_BOND_54

            //5500: ch 58
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_58

            //5500: ch 62
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_62

            //5500: ch 102
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_102

            //5500: ch 106
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_106

            //5500: ch 110
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_110

            //5500: ch 114
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_114

            //5500: ch 118
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    0 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_118

            //5500: ch 122
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_122

            //5500: ch 126
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_126

            //5500: ch 130
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_130

            //5500: ch 134
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_134

            //5500: ch 138
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_138

#ifdef FEATURE_WLAN_CH144
            //5500: ch 142
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */

            //5500: ch 151
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_151

            //5500: ch 155
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_155

            //5500: ch 159
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_159

            //5500: ch 163
            {
                {
                    4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 10 , 10 , 10 , 11 , 11,
                    11 , 11 , 12 , 12 , 12 , 12 , 12 , 12 , 13 , 13 , 13 , 14 , 14 , 14 , 15 , 15 , 15 , 15 , 15 , 16 , 16 , 16 , 16,
                    16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 23 , 24,
                    24 , 24 , 25 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 31,
                    31 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 37 , 37 , 37,
                    37 , 38 , 38 , 38 , 38 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 43 , 43,
                    43 , 43 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 46 , 47 , 47 , 47 , 47 , 47 , 48 , 48,
                    48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52,
                    52 , 52 , 53 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56,
                    56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 , 59 , 59 , 60 , 60,
                    60 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 65 , 66 , 66
                }
            }, //RF_CHAN_BOND_163
        },

        //NV_TABLE_TPC_PDADC_OFFSETS
        {
            98,    // RF_CHAN_1
            101,   // RF_CHAN_2
            101,   // RF_CHAN_3
            100,   // RF_CHAN_4
            98,    // RF_CHAN_5
            97,    // RF_CHAN_6
            94,    // RF_CHAN_7
            94,    // RF_CHAN_8
            92,    // RF_CHAN_9
            90,    // RF_CHAN_10
            94,    // RF_CHAN_11
            95,    // RF_CHAN_12
            97,    // RF_CHAN_13
            104,   // RF_CHAN_14
            100,   // RF_CHAN_240
            100,   // RF_CHAN_244
            100,   // RF_CHAN_248
            100,   // RF_CHAN_252
            100,   // RF_CHAN_208
            100,   // RF_CHAN_212
            100,   // RF_CHAN_216
            100,   // RF_CHAN_36
            100,   // RF_CHAN_40
            100,   // RF_CHAN_44
            100,   // RF_CHAN_48
            100,   // RF_CHAN_52
            100,   // RF_CHAN_56
            100,   // RF_CHAN_60
            100,   // RF_CHAN_64
            100,   // RF_CHAN_100
            100,   // RF_CHAN_104
            100,   // RF_CHAN_108
            100,   // RF_CHAN_112
            100,   // RF_CHAN_116
            100,   // RF_CHAN_120
            100,   // RF_CHAN_124
            100,   // RF_CHAN_128
            100,   // RF_CHAN_132
            100,   // RF_CHAN_136
            100,   // RF_CHAN_140
#ifdef FEATURE_WLAN_CH144
            100,   // RF_CHAN_144
#endif /* FEATURE_WLAN_CH144 */
            100,   // RF_CHAN_149
            100,   // RF_CHAN_153
            100,   // RF_CHAN_157
            100,   // RF_CHAN_161
            100,   // RF_CHAN_165
            //CHANNEL BONDED CHANNELS
            100,   // RF_CHAN_BOND_3
            100,   // RF_CHAN_BOND_4
            100,   // RF_CHAN_BOND_5
            100,   // RF_CHAN_BOND_6
            100,   // RF_CHAN_BOND_7
            100,   // RF_CHAN_BOND_8
            100,   // RF_CHAN_BOND_9
            100,   // RF_CHAN_BOND_10
            100,   // RF_CHAN_BOND_11
            100,   // RF_CHAN_BOND_242
            100,   // RF_CHAN_BOND_246
            100,   // RF_CHAN_BOND_250
            100,   // RF_CHAN_BOND_210
            100,   // RF_CHAN_BOND_214
            100,   // RF_CHAN_BOND_38
            100,   // RF_CHAN_BOND_42
            100,   // RF_CHAN_BOND_46
            100,   // RF_CHAN_BOND_50
            100,   // RF_CHAN_BOND_54
            100,   // RF_CHAN_BOND_58
            100,   // RF_CHAN_BOND_62
            100,   // RF_CHAN_BOND_102
            100,   // RF_CHAN_BOND_106
            100,   // RF_CHAN_BOND_110
            100,   // RF_CHAN_BOND_114
            100,   // RF_CHAN_BOND_118
            100,   // RF_CHAN_BOND_122
            100,   // RF_CHAN_BOND_126
            100,   // RF_CHAN_BOND_130
            100,   // RF_CHAN_BOND_134
            100,   // RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
            100,   // RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */
            100,   // RF_CHAN_BOND_151
            100,   // RF_CHAN_BOND_155
            100,   // RF_CHAN_BOND_159
            100,   // RF_CHAN_BOND_163
        },

        //NV_TABLE_VIRTUAL_RATE
        // typedef tANI_S16 tPowerdBm;
        //typedef tPowerdBm tRateGroupPwr[NUM_HAL_PHY_RATES];
        //tRateGroupPwr       pwrOptimum[NUM_RF_SUBBANDS];
        {
            // 2.4G RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 5G Low RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 5G Middle RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 5G High RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 4.9G RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            }
        },

#if 0 //FIXME_PRIMA
        //NV_TABLE_CAL_MEMORY
        {
            0x7FFF,      // tANI_U16    process_monitor;
            0x00,        // tANI_U8     hdet_cal_code;
            0x00,        // tANI_U8     rxfe_gm_2;

            0x00,        // tANI_U8     tx_bbf_rtune;
            0x00,        // tANI_U8     pa_rtune_reg;
            0x00,        // tANI_U8     rt_code;
            0x00,        // tANI_U8     bias_rtune;

            0x00,        // tANI_U8     bb_bw1;
            0x00,        // tANI_U8     bb_bw2;
            { 0x00, 0x00 },        // tANI_U8     reserved[2];

            0x00,        // tANI_U8     bb_bw3;
            0x00,        // tANI_U8     bb_bw4;
            0x00,        // tANI_U8     bb_bw5;
            0x00,        // tANI_U8     bb_bw6;

            0x7FFF,      // tANI_U16    rcMeasured;
            0x00,        // tANI_U8     tx_bbf_ct;
            0x00,        // tANI_U8     tx_bbf_ctr;

            0x00,        // tANI_U8     csh_maxgain_reg;
            0x00,        // tANI_U8     csh_0db_reg;
            0x00,        // tANI_U8     csh_m3db_reg;
            0x00,        // tANI_U8     csh_m6db_reg;

            0x00,        // tANI_U8     cff_0db_reg;
            0x00,        // tANI_U8     cff_m3db_reg;
            0x00,        // tANI_U8     cff_m6db_reg;
            0x00,        // tANI_U8     rxfe_gpio_ctl_1;

            0x00,        // tANI_U8     mix_bal_cnt_2;
            0x00,        // tANI_S8     rxfe_lna_highgain_bias_ctl_delta;
            0x00,        // tANI_U8     rxfe_lna_load_ctune;
            0x00,        // tANI_U8     rxfe_lna_ngm_rtune;

            0x00,        // tANI_U8     rx_im2_i_cfg0;
            0x00,        // tANI_U8     rx_im2_i_cfg1;
            0x00,        // tANI_U8     rx_im2_q_cfg0;
            0x00,        // tANI_U8     rx_im2_q_cfg1;

            0x00,        // tANI_U8     pll_vfc_reg3_b0;
            0x00,        // tANI_U8     pll_vfc_reg3_b1;
            0x00,        // tANI_U8     pll_vfc_reg3_b2;
            0x00,        // tANI_U8     pll_vfc_reg3_b3;

            0x7FFF,        // tANI_U16    tempStart;
            0x7FFF,        // tANI_U16    tempFinish;

            { //txLoCorrections
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_1
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_2
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_3
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_4
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_5
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_6
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_7
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_8
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_9
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_10
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_11
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_12
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_13
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }  //RF_CHAN_14
            },        // tTxLoCorrections    txLoValues;

            { //sTxIQChannel
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_1
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_2
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_3
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_4
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_5
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_6
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_7
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_8
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_9
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_10
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_11
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_12
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_13
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }  //RF_CHAN_14
            },        // sTxIQChannel        txIqValues;

            { //sRxIQChannel
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_1
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_2
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_3
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_4
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_5
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_6
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_7
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_8
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_9
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_10
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_11
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_12
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_13
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }  //RF_CHAN_14
            },        // sRxIQChannel        rxIqValues;

            { // tTpcConfig          clpcData[MAX_TPC_CHANNELS]
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_1
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_2
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_3
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_4
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_5
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_6
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_7
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_8
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_9
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_10
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_11
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_12
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                },  // RF_CHAN_13
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }  // RF_CHAN_14
            },        // tTpcConfig          clpcData[MAX_TPC_CHANNELS];

            {
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_1: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_2: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_3: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_4: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_5: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_6: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_7: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_8: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_9: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_10: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_11: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_12: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_13: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }  // RF_CHAN_14: pdadc_offset, reserved[2]
            }        // tTpcParams          clpcParams[MAX_TPC_CHANNELS];

        }, //NV_TABLE_CAL_MEMORY
#endif
        //NV_TABLE_FW_CONFIG
        {
            0,   //skuID
            0,   //tpcMode2G
            0,   //tpcMode5G
            0,   //reserved1

            0,   //xPA2G
            0,   //xPA5G;
            0,   //paPolarityTx;
            0,   //paPolarityRx;

            0,   //xLNA2G;
            0,   //xLNA5G;
            0,   //xCoupler2G;
            0,   //xCoupler5G;

            0,   //xPdet2G;
            0,   //xPdet5G;
            0,   //enableDPD2G;
            1,   //enableDPD5G;

            1,   //pdadcSelect2G;
            1,   //pdadcSelect5GLow;
            1,   //pdadcSelect5GMid;
            1,   //pdadcSelect5GHigh;

            0,   //reserved2
            0,   //reserved3
            0,   //reserved4
        },


        //NV_TABLE_RSSI_CHANNEL_OFFSETS
        {
            //PHY_RX_CHAIN_0
            {
                //bRssiOffset
                {
                    240,    //RF_CHAN_1,
                    240,    //RF_CHAN_2,
                    240,    //RF_CHAN_3,
                    240,    //RF_CHAN_4,
                    240,    //RF_CHAN_5,
                    240,    //RF_CHAN_6,
                    240,    //RF_CHAN_7,
                    240,    //RF_CHAN_8,
                    240,    //RF_CHAN_9,
                    240,    //RF_CHAN_10,
                    240,    //RF_CHAN_11,
                    240,    //RF_CHAN_12,
                    240,    //RF_CHAN_13,
                    240,    //RF_CHAN_14,
                    180,    //RF_CHAN_240,
                    180,    //RF_CHAN_244,
                    180,    //RF_CHAN_248,
                    180,    //RF_CHAN_252,
                    180,    //RF_CHAN_208,
                    180,    //RF_CHAN_212,
                    180,    //RF_CHAN_216,
                    180,    //RF_CHAN_36,
                    180,    //RF_CHAN_40,
                    180,    //RF_CHAN_44,
                    180,    //RF_CHAN_48,
                    180,    //RF_CHAN_52,
                    180,    //RF_CHAN_56,
                    180,    //RF_CHAN_60,
                    180,    //RF_CHAN_64,
                    180,    //RF_CHAN_100,
                    180,    //RF_CHAN_104,
                    180,    //RF_CHAN_108,
                    180,    //RF_CHAN_112,
                    180,    //RF_CHAN_116,
                    180,    //RF_CHAN_120,
                    180,    //RF_CHAN_124,
                    180,    //RF_CHAN_128,
                    180,    //RF_CHAN_132,
                    180,    //RF_CHAN_136,
                    180,    //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    180,    //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */
                    180,    //RF_CHAN_149,
                    180,    //RF_CHAN_153,
                    180,    //RF_CHAN_157,
                    180,    //RF_CHAN_161,
                    180,    //RF_CHAN_165,
                    240,    //RF_CHAN_BOND_3
                    240,    //RF_CHAN_BOND_4
                    240,    //RF_CHAN_BOND_5
                    240,    //RF_CHAN_BOND_6
                    240,    //RF_CHAN_BOND_7
                    240,    //RF_CHAN_BOND_8
                    240,    //RF_CHAN_BOND_9
                    240,    //RF_CHAN_BOND_10
                    240,    //RF_CHAN_BOND_11
                    180,    //RF_CHAN_BOND_242
                    180,    //RF_CHAN_BOND_246
                    180,    //RF_CHAN_BOND_250
                    180,    //RF_CHAN_BOND_210
                    180,    //RF_CHAN_BOND_214
                    180,    //RF_CHAN_BOND_38,
                    180,    //RF_CHAN_BOND_42,
                    180,    //RF_CHAN_BOND_46,
                    180,    //RF_CHAN_BOND_50,
                    180,    //RF_CHAN_BOND_54
                    180,    //RF_CHAN_BOND_58
                    180,    //RF_CHAN_BOND_62
                    180,    //RF_CHAN_BOND_102
                    180,    //RF_CHAN_BOND_106
                    180,    //RF_CHAN_BOND_110
                    180,    //RF_CHAN_BOND_114
                    180,    //RF_CHAN_BOND_118
                    180,    //RF_CHAN_BOND_122
                    180,    //RF_CHAN_BOND_126
                    180,    //RF_CHAN_BOND_130
                    180,    //RF_CHAN_BOND_134
                    180,    //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    180,    //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */
                    180,    //RF_CHAN_BOND_151
                    180,    //RF_CHAN_BOND_155
                    180,    //RF_CHAN_BOND_159
                    180,    //RF_CHAN_BOND_163
                },

                //gnRssiOffset
                {
                    240,    //RF_CHAN_1,
                    240,    //RF_CHAN_2,
                    240,    //RF_CHAN_3,
                    240,    //RF_CHAN_4,
                    240,    //RF_CHAN_5,
                    240,    //RF_CHAN_6,
                    240,    //RF_CHAN_7,
                    240,    //RF_CHAN_8,
                    240,    //RF_CHAN_9,
                    240,    //RF_CHAN_10,
                    240,    //RF_CHAN_11,
                    240,    //RF_CHAN_12,
                    240,    //RF_CHAN_13,
                    240,    //RF_CHAN_14,
                    180,    //RF_CHAN_240,
                    180,    //RF_CHAN_244,
                    180,    //RF_CHAN_248,
                    180,    //RF_CHAN_252,
                    180,    //RF_CHAN_208,
                    180,    //RF_CHAN_212,
                    180,    //RF_CHAN_216,
                    180,    //RF_CHAN_36,
                    180,    //RF_CHAN_40,
                    180,    //RF_CHAN_44,
                    180,    //RF_CHAN_48,
                    180,    //RF_CHAN_52,
                    180,    //RF_CHAN_56,
                    180,    //RF_CHAN_60,
                    180,    //RF_CHAN_64,
                    180,    //RF_CHAN_100,
                    180,    //RF_CHAN_104,
                    180,    //RF_CHAN_108,
                    180,    //RF_CHAN_112,
                    180,    //RF_CHAN_116,
                    180,    //RF_CHAN_120,
                    180,    //RF_CHAN_124,
                    180,    //RF_CHAN_128,
                    180,    //RF_CHAN_132,
                    180,    //RF_CHAN_136,
                    180,    //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
                    180,    //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */
                    180,    //RF_CHAN_149,
                    180,    //RF_CHAN_153,
                    180,    //RF_CHAN_157,
                    180,    //RF_CHAN_161,
                    180,    //RF_CHAN_165,
                    240,    //RF_CHAN_BOND_3
                    240,    //RF_CHAN_BOND_4
                    240,    //RF_CHAN_BOND_5
                    240,    //RF_CHAN_BOND_6
                    240,    //RF_CHAN_BOND_7
                    240,    //RF_CHAN_BOND_8
                    240,    //RF_CHAN_BOND_9
                    240,    //RF_CHAN_BOND_10
                    240,    //RF_CHAN_BOND_11
                    180,    //RF_CHAN_BOND_242
                    180,    //RF_CHAN_BOND_246
                    180,    //RF_CHAN_BOND_250
                    180,    //RF_CHAN_BOND_210
                    180,    //RF_CHAN_BOND_214
                    180,    //RF_CHAN_BOND_38,
                    180,    //RF_CHAN_BOND_42,
                    180,    //RF_CHAN_BOND_46,
                    180,    //RF_CHAN_BOND_50,
                    180,    //RF_CHAN_BOND_54
                    180,    //RF_CHAN_BOND_58
                    180,    //RF_CHAN_BOND_62
                    180,    //RF_CHAN_BOND_102
                    180,    //RF_CHAN_BOND_106
                    180,    //RF_CHAN_BOND_110
                    180,    //RF_CHAN_BOND_114
                    180,    //RF_CHAN_BOND_118
                    180,    //RF_CHAN_BOND_122
                    180,    //RF_CHAN_BOND_126
                    180,    //RF_CHAN_BOND_130
                    180,    //RF_CHAN_BOND_134
                    180,    //RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
                    180,    //RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */
                    180,    //RF_CHAN_BOND_151
                    180,    //RF_CHAN_BOND_155
                    180,    //RF_CHAN_BOND_159
                    180,    //RF_CHAN_BOND_163
                },
            },
            //rsvd
            {
                //bRssiOffset
                {0},   // apply to all channles

                //gnRssiOffset
                {0}    // apply to all channles
            }
        },

        //NV_TABLE_HW_CAL_VALUES
        {
            0x0,             //validBmap
            {
                1400,        //psSlpTimeOvrHd2G;
                1400,        //psSlpTimeOvrHd5G;

                1600,        //psSlpTimeOvrHdxLNA5G;
                0,           //nv_TxBBFSel9MHz
                0,           //hwParam1
                0,           //hwParam2

                0x1B,        //custom_tcxo_reg8
                0xFF,        //custom_tcxo_reg9

                0,           //hwParam3;
                0,           //hwParam4;
                0,           //hwParam5;
                0,           //hwParam6;
                0,           //hwParam7;
                0,           //hwParam8;
                0,           //hwParam9;
                0,           //hwParam10;
                0,           //hwParam11;
            }
        },


        //NV_TABLE_ANTENNA_PATH_LOSS
        {
            280,  // RF_CHAN_1
            270,  // RF_CHAN_2
            270,  // RF_CHAN_3
            270,  // RF_CHAN_4
            270,  // RF_CHAN_5
            270,  // RF_CHAN_6
            280,  // RF_CHAN_7
            280,  // RF_CHAN_8
            290,  // RF_CHAN_9
            300,  // RF_CHAN_10
            300,  // RF_CHAN_11
            310,  // RF_CHAN_12
            310,  // RF_CHAN_13
            310,   // RF_CHAN_14
            280,  // RF_CHAN_240
            280,  // RF_CHAN_244
            280,   // RF_CHAN_248
            280,   // RF_CHAN_252
            280,   // RF_CHAN_208
            280,   // RF_CHAN_212
            280,   // RF_CHAN_216
            280,   // RF_CHAN_36
            280,   // RF_CHAN_40
            280,   // RF_CHAN_44
            280,   // RF_CHAN_48
            280,   // RF_CHAN_52
            280,   // RF_CHAN_56
            280,   // RF_CHAN_60
            280,   // RF_CHAN_64
            280,   // RF_CHAN_100
            280,   // RF_CHAN_104
            280,   // RF_CHAN_108
            280,   // RF_CHAN_112
            280,   // RF_CHAN_116
            280,   // RF_CHAN_120
            280,   // RF_CHAN_124
            280,   // RF_CHAN_128
            280,   // RF_CHAN_132
            280,   // RF_CHAN_136
            280,   // RF_CHAN_140
#ifdef FEATURE_WLAN_CH144
            280,   // RF_CHAN_144
#endif /* FEATURE_WLAN_CH144 */
            280,   // RF_CHAN_149
            280,   // RF_CHAN_153
            280,   // RF_CHAN_157
            280,   // RF_CHAN_161
            280,   // RF_CHAN_165
            //CHANNEL BONDED CHANNELS
            280,   // RF_CHAN_BOND_3
            280,   // RF_CHAN_BOND_4
            280,   // RF_CHAN_BOND_5
            280,   // RF_CHAN_BOND_6
            280,   // RF_CHAN_BOND_7
            280,   // RF_CHAN_BOND_8
            280,   // RF_CHAN_BOND_9
            280,   // RF_CHAN_BOND_10
            280,   // RF_CHAN_BOND_11
            280,   // RF_CHAN_BOND_242
            280,   // RF_CHAN_BOND_246
            280,   // RF_CHAN_BOND_250
            280,   // RF_CHAN_BOND_210
            280,   // RF_CHAN_BOND_214
            280,   // RF_CHAN_BOND_38
            280,   // RF_CHAN_BOND_42
            280,   // RF_CHAN_BOND_46
            280,   // RF_CHAN_BOND_50
            280,   // RF_CHAN_BOND_54
            280,   // RF_CHAN_BOND_58
            280,   // RF_CHAN_BOND_62
            280,   // RF_CHAN_BOND_102
            280,   // RF_CHAN_BOND_106
            280,   // RF_CHAN_BOND_110
            280,   // RF_CHAN_BOND_114
            280,   // RF_CHAN_BOND_118
            280,   // RF_CHAN_BOND_122
            280,   // RF_CHAN_BOND_126
            280,   // RF_CHAN_BOND_130
            280,   // RF_CHAN_BOND_134
            280,   // RF_CHAN_BOND_138
#ifdef FEATURE_WLAN_CH144
            280,   // RF_CHAN_BOND_142
#endif /* FEATURE_WLAN_CH144 */
            280,   // RF_CHAN_BOND_151
            280,   // RF_CHAN_BOND_155
            280,   // RF_CHAN_BOND_159
            280,   // RF_CHAN_BOND_163
        },

        //NV_TABLE_PACKET_TYPE_POWER_LIMITS
        {
            { 2150 }, // applied to all channels, MODE_802_11B
            { 1850 }, // applied to all channels,MODE_802_11AG
            { 1750 }  // applied to all channels,MODE_802_11N
        },

        //NV_TABLE_OFDM_CMD_PWR_OFFSET
        {
            0, 0
        },

        //NV_TABLE_TX_BB_FILTER_MODE
        {
            0
        }

    } // tables
};

const sHalNvV2 nvDefaultsV2 =
{
    {
        0,                                                              // tANI_U16  productId;
        1,                                                              // tANI_U8   productBands;
        2,                                                              // tANI_U8   wlanNvRevId; //0: WCN1312, 1: WCN1314, 2: WCN3660
        1,                                                              // tANI_U8   numOfTxChains;
        1,                                                              // tANI_U8   numOfRxChains;
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                         // tANI_U8   macAddr[NV_FIELD_MAC_ADDR_SIZE];
        { "\0" },
        0,                                                              // tANI_U8   couplerType;
        WLAN_NV_VERSION,                                                // tANI_U8   nvVersion;
    }, //fields

    {
        // NV_TABLE_RATE_POWER_SETTINGS
        {
            // typedef tANI_S16 tPowerdBm;
            //typedef tPowerdBm tRateGroupPwr[NUM_HAL_PHY_RATES];
            //tRateGroupPwr       pwrOptimum[NUM_RF_SUBBANDS];
            //2.4G
            {
                //802.11b Rates
                {1900},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {1900},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {1900},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {1900},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {1900},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {1900},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {1900},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                //11A 20MHz Rates
                {1700},    // HAL_PHY_RATE_11A_6_MBPS,
                {1700},    // HAL_PHY_RATE_11A_9_MBPS,
                {1700},    // HAL_PHY_RATE_11A_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_18_MBPS,
                {1600},    // HAL_PHY_RATE_11A_24_MBPS,
                {1550},    // HAL_PHY_RATE_11A_36_MBPS,
                {1550},    // HAL_PHY_RATE_11A_48_MBPS,
                {1550},    // HAL_PHY_RATE_11A_54_MBPS,

                //DUP 11A 40MHz Rates
                {1700},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                //MCS Index #0-7(20/40MHz)
                {1700},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1300},     // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                //MCS Index #8-15(20/40MHz)
                {1700},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1700},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1650},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATURE_11AC
                //11AC rates
               //11A duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               //11ac 20MHZ NG, SG
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                {0000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif

               //11ac 40MHZ NG, SG
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,

               //11ac 80MHZ NG, SG
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                {0000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_2_4_GHZ
            // 5G Low
            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DUP 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_11AC
                ///11AC rates
               ///11A duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11ac 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               //11ac 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_5_LOW_GHZ
            // 5G Mid
            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DU P 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCSS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCSS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_111AC
                ///11CAC rates
               ///11Ad duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11a c 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               ///11a c 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    //     RF_SUBBAND_5_MID_GHZ
            // 5G High
            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DU P 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCSS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCSS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_11AC
                ///11CAC rates
               ///11Ad duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11a c 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               ///11a c 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_5_HIGH_GHZ,
            // 4.9G

            {
                //802.11b Rates
                {0},    // HAL_PHY_RATE_11B_LONG_1_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_LONG_11_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_2_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_5_5_MBPS,
                {0},    // HAL_PHY_RATE_11B_SHORT_11_MBPS,

                ///11A 20MHz Rates
                {1600},    // HAL_PHY_RATE_11A_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_12_MBPS,
                {1550},    // HAL_PHY_RATE_11A_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_54_MBPS,

                ///DU P 11A 40MHz Rates
                {1600},    // HAL_PHY_RATE_11A_DUP_6_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_9_MBPS,
                {1600},    // HAL_PHY_RATE_11A_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11A_DUP_18_MBPS,
                {1550},    // HAL_PHY_RATE_11A_DUP_24_MBPS,
                {1450},    // HAL_PHY_RATE_11A_DUP_36_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_48_MBPS,
                {1400},    // HAL_PHY_RATE_11A_DUP_54_MBPS,

                ///MCSS Index #0-7(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_6_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_13_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_19_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_26_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_39_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_52_MBPS,
                {1350},    // HAL_PHY_RATE_MCS_1NSS_58_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_65_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_65_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_72_2_MBPS,

                ///MCSS Index #8-15(20/40MHz)
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_13_5_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_CB_27_MBPS,
                {1550},    // HAL_PHY_RATE_MCS_1NSS_CB_40_5_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_CB_54_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_CB_81_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_CB_108_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_CB_121_5_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_CB_135_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_15_MBPS,
                {1600},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_30_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_45_MBPS,
                {1500},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_60_MBPS,
                {1450},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_90_MBPS,
                {1400},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_120_MBPS,
                {1300},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_135_MBPS,
                {1200},    // HAL_PHY_RATE_MCS_1NSS_MM_SG_CB_150_MBPS,

#ifdef WLAN_FEATUURE_11AC
                ///11CAC rates
               ///11Ad duplicate 80MHz Rates
                {1700},    // HAL_PHY_RATE_11AC_DUP_6_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_9_MBPS,
                {1700},    // HAL_PHY_RATE_11AC_DUP_12_MBPS,
                {1650},    // HAL_PHY_RATE_11AC_DUP_18_MBPS,
                {1600},    // HAL_PHY_RATE_11AC_DUP_24_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_36_MBPS,
                {1550},    // HAL_PHY_RATE_11AC_DUP_48_MBPS,
                {1500},    // HAL_PHY_RATE_11AC_DUP_54_MBPS,

               ///11a c 20MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_6_5_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_13_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_19_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_26_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_39_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_52_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_65_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_NGI_78_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_NGI_86_5_MBPS,
#endif
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_7_2_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_14_4_MBPS,
                {1350},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_21_6_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_28_8_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_43_3_MBPS,
                {1200},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_57_7_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_72_2_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_CB_SGI_86_6_MBPS,
#ifdef WCN_PRONTO
                { 800},     // HAL_PHY_RATE_VHT_20MHZ_MCS_1NSS_SGI_96_1_MBPS,
#endif
               //11ac 40MHZ NG, SG
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_13_5_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_27_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_40_5_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_54_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_81_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_108_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_121_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_135_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_162_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_NGI_180_MBPS,
                {1400},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_15_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_30_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_45_MBPS,
                {1250},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_60_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_90_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_120_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_135_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_150_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_180_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_40MHZ_MCS_1NSS_CB_SGI_200_MBPS,


               ///11a c 80MHZ NG, SG
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_29_3_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_58_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_87_8_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_117_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_175_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_234_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_263_3_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_292_5_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_351_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_NGI_390_MBPS,
                {1300},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_32_5_MBPS,
                {1100},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_65_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_97_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_130_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_195_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_260_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_292_5_MBPS,
                {1000},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_325_MBPS,
                { 900},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_390_MBPS,
                { 800},    // HAL_PHY_RATE_VHT_80MHZ_MCS_1NSS_CB_SGI_433_3_MBPS,
#endif
            },  //    RF_SUBBAND_4_9_GHZ
        },

        // NV_TABLE_REGULATORY_DOMAINS
        {
            // typedef struct
            // {
            //     tANI_BOOLEAN enabled;
            //     tPowerdBm pwrLimit;
            // }sRegulatoryChannel;

            // typedef struct
            // {
            //     sRegulatoryChannel channels[NUM_RF_CHANNELS];
            //     uAbsPwrPrecision antennaGain[NUM_RF_SUBBANDS];
            //     uAbsPwrPrecision bRatePowerOffset[NUM_2_4GHZ_CHANNELS];
            // }sRegulatoryDomains;

            //sRegulatoryDomains  regDomains[NUM_REG_DOMAINS];


            {   // REG_DOMAIN_FCC start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 23},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 22},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 22},           //RF_CHAN_11,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_12,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},             //RF_CHAN_48,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_52,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_56,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_60,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DFS, 22},                //RF_CHAN_100,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_104,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_108,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_112,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_128,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_132,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_136,
                    {NV_CHANNEL_DFS, 24},                //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},             //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_FCC end

            {   // REG_DOMAIN_ETSI start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 19},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 19},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_149,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_153,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_157,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_161,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 23},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_ETSI end

            {   // REG_DOMAIN_JAPAN start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 20},           //RF_CHAN_13,
                    {NV_CHANNEL_ENABLE, 18},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_52,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_56,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_60,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DFS, 22},               //RF_CHAN_100,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_104,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_108,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_112,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_116,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_120,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_124,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_128,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_132,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_136,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_JAPAN end

            {   // REG_DOMAIN_WORLD start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                                       //2.4GHz Band
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_52,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_56,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_60,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DFS, 22},               //RF_CHAN_100,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_104,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_108,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_112,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_116,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_120,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_124,
                    {NV_CHANNEL_DFS, 0},                //RF_CHAN_128,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_132,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_136,
                    {NV_CHANNEL_DFS, 24},               //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_WORLD end

            {   // REG_DOMAIN_N_AMER_EXC_FCC start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_11,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_12,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 30},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            },   // REG_DOMAIN_N_AMER_EXC_FCC end

            {   // REG_DOMAIN_APAC start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 26},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 16},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_APAC end

            {   // REG_DOMAIN_KOREA start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 15},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_KOREA end

            {   // REG_DOMAIN_HI_5GHZ start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                    //2.4GHz Band, none CB
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 14},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,

                    //4.9GHz Band, none CB
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_240,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_244,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_248,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_252,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_208,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_212,
                    {NV_CHANNEL_DISABLE, 23},            //RF_CHAN_216,

                    //5GHz Low & Mid U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_36,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_40,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_44,
                    {NV_CHANNEL_ENABLE, 17},            //RF_CHAN_48,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_52,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_56,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_60,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_64,

                    //5GHz Mid Band - ETSI, none CB
                    {NV_CHANNEL_DISABLE, 22},            //RF_CHAN_100,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_104,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_108,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_112,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_116,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_120,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_124,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_128,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_132,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_136,
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_140,

                    //5GHz High U-NII Band, none CB
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_149,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_153,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_157,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_161,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_165,

                    //2.4GHz Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_3,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_4,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_5,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_6,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_7,
                    {NV_CHANNEL_ENABLE, 30},            //RF_CHAN_BOND_8,
                    {NV_CHANNEL_ENABLE, 22},            //RF_CHAN_BOND_9,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_10,
                    {NV_CHANNEL_ENABLE, 0},            //RF_CHAN_BOND_11,

                    // 4.9GHz Band, channel bonded channels
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_242,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_246,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_250,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_210,
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_214,

                    //5GHz Low & Mid U-NII Band, channel bonded channels
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_38,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_42,
                    {NV_CHANNEL_ENABLE, 20},            //RF_CHAN_BOND_46,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_50,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_54,
                    {NV_CHANNEL_ENABLE, 27},            //RF_CHAN_BOND_58,
                    {NV_CHANNEL_ENABLE, 25},            //RF_CHAN_BOND_62,

                    //5GHz Mid Band - ETSI, channel bonded channels
                    {NV_CHANNEL_DISABLE, 24},            //RF_CHAN_BOND_102
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_106
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_110
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_114
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_118
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_122
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_126
                    {NV_CHANNEL_DISABLE, 0},            //RF_CHAN_BOND_130
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_134
                    {NV_CHANNEL_DISABLE, 27},            //RF_CHAN_BOND_138

                    //5GHz High U-NII Band,  channel bonded channels
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_151,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_155,
                    {NV_CHANNEL_DISABLE, 30},            //RF_CHAN_BOND_159,
                    {NV_CHANNEL_DISABLE, 0},             //RF_CHAN_BOND_163
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            }, // REG_DOMAIN_HI_5GHZ end

            {   // REG_DOMAIN_NO_5GHZ start
                { //sRegulatoryChannel start
                    //enabled, pwrLimit
                                       //2.4GHz Band
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_1,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_2,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_3,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_4,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_5,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_6,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_7,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_8,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_9,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_10,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_11,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_12,
                    {NV_CHANNEL_ENABLE, 12},           //RF_CHAN_13,
                    {NV_CHANNEL_DISABLE, 0},           //RF_CHAN_14,
                }, //sRegulatoryChannel end

                {
                    { 0 },  // RF_SUBBAND_2_4_GHZ
                    {0},   // RF_SUBBAND_5_LOW_GHZ
                    {0},   // RF_SUBBAND_5_MID_GHZ
                    {0},   // RF_SUBBAND_5_HIGH_GHZ
                    {0}    // RF_SUBBAND_4_9_GHZ
                },

                { // bRatePowerOffset start
                    //2.4GHz Band
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                }, // bRatePowerOffset end

                { // gnRatePowerOffset start
                    //apply to all 2.4 and 5G channels
                    { 0 },                       //RF_CHAN_1,
                    { 0 },                       //RF_CHAN_2,
                    { 0 },                       //RF_CHAN_3,
                    { 0 },                       //RF_CHAN_4,
                    { 0 },                       //RF_CHAN_5,
                    { 0 },                       //RF_CHAN_6,
                    { 0 },                       //RF_CHAN_7,
                    { 0 },                       //RF_CHAN_8,
                    { 0 },                       //RF_CHAN_9,
                    { 0 },                       //RF_CHAN_10,
                    { 0 },                       //RF_CHAN_11,
                    { 0 },                       //RF_CHAN_12,
                    { 0 },                       //RF_CHAN_13,
                    { 0 },                       //RF_CHAN_14,
                } // gnRatePowerOffset end
            } // REG_DOMAIN_NO_5GHZ end
        },

        // NV_TABLE_DEFAULT_COUNTRY
        {
            // typedef struct
            // {
            //     tANI_U8 regDomain;                                      //from eRegDomainId
            //     tANI_U8 countryCode[NV_FIELD_COUNTRY_CODE_SIZE];    // string identifier
            // }sDefaultCountry;

            0,                  // regDomain
            { 'U', 'S', 'I' }   // countryCode
        },

        //NV_TABLE_TPC_POWER_TABLE
        {
            {
                {
                    0  , //0
                    41 , //1
                    43 , //2
                    45 , //3
                    47 , //4
                    49 , //5
                    51 , //6
                    53 , //7
                    55 , //8
                    56 , //9
                    58 , //10
                    59 , //11
                    60 , //12
                    62 , //13
                    63 , //14
                    64 , //15
                    65 , //16
                    67 , //17
                    68 , //18
                    69 , //19
                    70 , //20
                    71 , //21
                    72 , //22
                    73 , //23
                    74 , //24
                    75 , //25
                    75 , //26
                    76 , //27
                    77 , //28
                    78 , //29
                    78 , //30
                    79 , //31
                    80 , //32
                    81 , //33
                    82 , //34
                    82 , //35
                    83 , //36
                    83 , //37
                    84 , //38
                    85 , //39
                    86 , //40
                    86 , //41
                    87 , //42
                    88 , //43
                    89 , //44
                    89 , //45
                    90 , //46
                    91 , //47
                    91 , //48
                    92 , //49
                    92 , //50
                    93 , //51
                    93 , //52
                    94 , //53
                    94 , //54
                    95 , //55
                    95 , //56
                    95 , //57
                    96 , //58
                    96 , //59
                    97 , //60
                    97 , //61
                    98 , //62
                    98 , //63
                    98 , //64
                    99 , //65
                    99 , //66
                    99 , //67
                    100, //68
                    100, //69
                    100, //70
                    101, //71
                    101, //72
                    102, //73
                    102, //74
                    102, //75
                    102, //76
                    103, //77
                    103, //78
                    103, //79
                    103, //80
                    104, //81
                    104, //82
                    104, //83
                    104, //84
                    105, //85
                    105, //86
                    105, //87
                    105, //88
                    105, //89
                    106, //90
                    106, //91
                    106, //92
                    106, //93
                    106, //94
                    106, //95
                    106, //96
                    106, //97
                    106, //98
                    106, //99
                    106, //100
                    106, //101
                    106, //102
                    106, //103
                    106, //104
                    106, //105
                    107, //106
                    107, //107
                    107, //108
                    107, //109
                    107, //110
                    107, //111
                    107, //112
                    107, //113
                    107, //114
                    107, //115
                    107, //116
                    107, //117
                    107, //118
                    107, //119
                    107, //120
                    107, //121
                    107, //122
                    107, //123
                    107, //124
                    107, //125
                    107, //126
                    107, //127
                    107,
                }
            }, //RF_CHAN_1
            {
                {
                    0  , //0
                    41 , //1
                    43 , //2
                    45 , //3
                    47 , //4
                    49 , //5
                    51 , //6
                    52 , //7
                    54 , //8
                    56 , //9
                    57 , //10
                    59 , //11
                    60 , //12
                    61 , //13
                    62 , //14
                    64 , //15
                    65 , //16
                    66 , //17
                    67 , //18
                    68 , //19
                    69 , //20
                    70 , //21
                    71 , //22
                    72 , //23
                    73 , //24
                    74 , //25
                    75 , //26
                    75 , //27
                    76 , //28
                    77 , //29
                    78 , //30
                    79 , //31
                    79 , //32
                    80 , //33
                    81 , //34
                    82 , //35
                    82 , //36
                    83 , //37
                    84 , //38
                    85 , //39
                    85 , //40
                    86 , //41
                    87 , //42
                    88 , //43
                    88 , //44
                    89 , //45
                    89 , //46
                    90 , //47
                    91 , //48
                    91 , //49
                    92 , //50
                    92 , //51
                    93 , //52
                    93 , //53
                    94 , //54
                    94 , //55
                    95 , //56
                    95 , //57
                    96 , //58
                    96 , //59
                    96 , //60
                    97 , //61
                    97 , //62
                    98 , //63
                    98 , //64
                    98 , //65
                    99 , //66
                    99 , //67
                    99 , //68
                    100, //69
                    100, //70
                    101, //71
                    101, //72
                    101, //73
                    101, //74
                    102, //75
                    102, //76
                    102, //77
                    103, //78
                    103, //79
                    103, //80
                    104, //81
                    104, //82
                    104, //83
                    104, //84
                    105, //85
                    105, //86
                    105, //87
                    105, //88
                    105, //89
                    106, //90
                    106, //91
                    106, //92
                    106, //93
                    106, //94
                    106, //95
                    106, //96
                    106, //97
                    106, //98
                    106, //99
                    106, //100
                    106, //101
                    106, //102
                    107, //103
                    107, //104
                    107, //105
                    107, //106
                    107, //107
                    107, //108
                    107, //109
                    107, //110
                    107, //111
                    107, //112
                    107, //113
                    107, //114
                    107, //115
                    107, //116
                    107, //117
                    107, //118
                    107, //119
                    107, //120
                    107, //121
                    107, //122
                    107, //123
                    107, //124
                    107, //125
                    107, //126
                    107, //127
                    107,
                }
            }, //RF_CHAN_2
                {
                    {
                        0  , //0
                        41 , //1
                        43 , //2
                        45 , //3
                        47 , //4
                        49 , //5
                        51 , //6
                        52 , //7
                        54 , //8
                        55 , //9
                        57 , //10
                        58 , //11
                        60 , //12
                        61 , //13
                        62 , //14
                        64 , //15
                        65 , //16
                        66 , //17
                        67 , //18
                        68 , //19
                        69 , //20
                        70 , //21
                        71 , //22
                        72 , //23
                        73 , //24
                        74 , //25
                        75 , //26
                        75 , //27
                        76 , //28
                        77 , //29
                        78 , //30
                        78 , //31
                        79 , //32
                        80 , //33
                        81 , //34
                        82 , //35
                        82 , //36
                        83 , //37
                        84 , //38
                        84 , //39
                        85 , //40
                        86 , //41
                        87 , //42
                        87 , //43
                        88 , //44
                        89 , //45
                        89 , //46
                        90 , //47
                        90 , //48
                        91 , //49
                        91 , //50
                        92 , //51
                        93 , //52
                        93 , //53
                        94 , //54
                        94 , //55
                        94 , //56
                        95 , //57
                        95 , //58
                        96 , //59
                        96 , //60
                        97 , //61
                        97 , //62
                        97 , //63
                        98 , //64
                        98 , //65
                        99 , //66
                        99 , //67
                        99 , //68
                        100, //69
                        100, //70
                        100, //71
                        101, //72
                        101, //73
                        101, //74
                        102, //75
                        102, //76
                        102, //77
                        103, //78
                        103, //79
                        103, //80
                        103, //81
                        104, //82
                        104, //83
                        104, //84
                        104, //85
                        104, //86
                        105, //87
                        105, //88
                        105, //89
                        105, //90
                        105, //91
                        105, //92
                        105, //93
                        105, //94
                        105, //95
                        105, //96
                        105, //97
                        105, //98
                        106, //99
                        106, //100
                        106, //101
                        106, //102
                        106, //103
                        106, //104
                        106, //105
                        106, //106
                        106, //107
                        106, //108
                        106, //109
                        106, //110
                        106, //111
                        106, //112
                        106, //113
                        106, //114
                        106, //115
                        106, //116
                        106, //117
                        106, //118
                        106, //119
                        106, //120
                        106, //121
                        106, //122
                        106, //123
                        106, //124
                        106, //125
                        106, //126
                        106, //127
                        107,
                    }
                }, //RF_CHAN_3
                {
                    {
                        0  , //0
                        42 , //1
                        44 , //2
                        46 , //3
                        48 , //4
                        49 , //5
                        51 , //6
                        53 , //7
                        55 , //8
                        57 , //9
                        58 , //10
                        60 , //11
                        61 , //12
                        62 , //13
                        63 , //14
                        64 , //15
                        66 , //16
                        67 , //17
                        68 , //18
                        69 , //19
                        70 , //20
                        71 , //21
                        72 , //22
                        73 , //23
                        74 , //24
                        75 , //25
                        75 , //26
                        76 , //27
                        77 , //28
                        78 , //29
                        78 , //30
                        79 , //31
                        80 , //32
                        81 , //33
                        82 , //34
                        82 , //35
                        83 , //36
                        84 , //37
                        84 , //38
                        85 , //39
                        86 , //40
                        87 , //41
                        87 , //42
                        88 , //43
                        88 , //44
                        89 , //45
                        90 , //46
                        90 , //47
                        91 , //48
                        91 , //49
                        92 , //50
                        92 , //51
                        93 , //52
                        93 , //53
                        94 , //54
                        94 , //55
                        95 , //56
                        95 , //57
                        95 , //58
                        96 , //59
                        96 , //60
                        97 , //61
                        97 , //62
                        98 , //63
                        98 , //64
                        98 , //65
                        99 , //66
                        99 , //67
                        99 , //68
                        100, //69
                        100, //70
                        100, //71
                        101, //72
                        101, //73
                        101, //74
                        102, //75
                        102, //76
                        102, //77
                        103, //78
                        103, //79
                        103, //80
                        103, //81
                        104, //82
                        104, //83
                        104, //84
                        104, //85
                        104, //86
                        104, //87
                        104, //88
                        104, //89
                        105, //90
                        105, //91
                        105, //92
                        105, //93
                        105, //94
                        105, //95
                        105, //96
                        105, //97
                        105, //98
                        105, //99
                        105, //100
                        105, //101
                        105, //102
                        105, //103
                        105, //104
                        106, //105
                        106, //106
                        106, //107
                        106, //108
                        106, //109
                        106, //110
                        106, //111
                        106, //112
                        106, //113
                        106, //114
                        106, //115
                        106, //116
                        106, //117
                        106, //118
                        106, //119
                        106, //120
                        106, //121
                        106, //122
                        106, //123
                        106, //124
                        106, //125
                        106, //126
                        106, //127
                        106,
                    }
                }, //RF_CHAN_4
                {
                    {
                        0  , //0
                        41 , //1
                        43 , //2
                        45 , //3
                        47 , //4
                        49 , //5
                        51 , //6
                        53 , //7
                        54 , //8
                        56 , //9
                        57 , //10
                        59 , //11
                        60 , //12
                        62 , //13
                        63 , //14
                        65 , //15
                        66 , //16
                        67 , //17
                        68 , //18
                        69 , //19
                        69 , //20
                        71 , //21
                        72 , //22
                        72 , //23
                        73 , //24
                        74 , //25
                        75 , //26
                        76 , //27
                        77 , //28
                        78 , //29
                        79 , //30
                        79 , //31
                        80 , //32
                        81 , //33
                        82 , //34
                        83 , //35
                        83 , //36
                        84 , //37
                        85 , //38
                        86 , //39
                        87 , //40
                        87 , //41
                        88 , //42
                        89 , //43
                        89 , //44
                        90 , //45
                        91 , //46
                        91 , //47
                        92 , //48
                        92 , //49
                        93 , //50
                        93 , //51
                        94 , //52
                        94 , //53
                        95 , //54
                        95 , //55
                        96 , //56
                        96 , //57
                        96 , //58
                        97 , //59
                        97 , //60
                        98 , //61
                        98 , //62
                        98 , //63
                        99 , //64
                        99 , //65
                        100, //66
                        100, //67
                        100, //68
                        101, //69
                        101, //70
                        101, //71
                        102, //72
                        102, //73
                        102, //74
                        103, //75
                        103, //76
                        103, //77
                        103, //78
                        104, //79
                        104, //80
                        104, //81
                        104, //82
                        105, //83
                        105, //84
                        105, //85
                        105, //86
                        105, //87
                        105, //88
                        105, //89
                        105, //90
                        105, //91
                        106, //92
                        106, //93
                        106, //94
                        106, //95
                        106, //96
                        106, //97
                        106, //98
                        106, //99
                        106, //100
                        106, //101
                        106, //102
                        106, //103
                        106, //104
                        106, //105
                        106, //106
                        106, //107
                        106, //108
                        106, //109
                        106, //110
                        106, //111
                        106, //112
                        106, //113
                        106, //114
                        106, //115
                        106, //116
                        106, //117
                        106, //118
                        106, //119
                        106, //120
                        106, //121
                        106, //122
                        106, //123
                        106, //124
                        106, //125
                        106, //126
                        106, //127
                        106,
                    }
                }, //RF_CHAN_5
                {
                    {
                        0  , //0
                        41 , //1
                        43 , //2
                        45 , //3
                        47 , //4
                        49 , //5
                        51 , //6
                        53 , //7
                        55 , //8
                        56 , //9
                        58 , //10
                        59 , //11
                        61 , //12
                        62 , //13
                        63 , //14
                        64 , //15
                        65 , //16
                        66 , //17
                        68 , //18
                        69 , //19
                        70 , //20
                        71 , //21
                        72 , //22
                        73 , //23
                        74 , //24
                        75 , //25
                        76 , //26
                        77 , //27
                        77 , //28
                        78 , //29
                        79 , //30
                        80 , //31
                        80 , //32
                        81 , //33
                        82 , //34
                        83 , //35
                        83 , //36
                        84 , //37
                        85 , //38
                        86 , //39
                        87 , //40
                        87 , //41
                        88 , //42
                        89 , //43
                        89 , //44
                        90 , //45
                        91 , //46
                        91 , //47
                        92 , //48
                        92 , //49
                        93 , //50
                        93 , //51
                        94 , //52
                        94 , //53
                        95 , //54
                        95 , //55
                        96 , //56
                        96 , //57
                        97 , //58
                        97 , //59
                        98 , //60
                        98 , //61
                        98 , //62
                        99 , //63
                        99 , //64
                        100, //65
                        100, //66
                        100, //67
                        101, //68
                        101, //69
                        101, //70
                        102, //71
                        102, //72
                        102, //73
                        103, //74
                        103, //75
                        103, //76
                        103, //77
                        104, //78
                        104, //79
                        104, //80
                        104, //81
                        104, //82
                        105, //83
                        105, //84
                        105, //85
                        105, //86
                        105, //87
                        105, //88
                        105, //89
                        106, //90
                        106, //91
                        106, //92
                        106, //93
                        106, //94
                        106, //95
                        106, //96
                        106, //97
                        106, //98
                        106, //99
                        106, //100
                        106, //101
                        106, //102
                        106, //103
                        106, //104
                        106, //105
                        106, //106
                        106, //107
                        106, //108
                        106, //109
                        106, //110
                        107, //111
                        107, //112
                        107, //113
                        107, //114
                        107, //115
                        107, //116
                        107, //117
                        107, //118
                        107, //119
                        107, //120
                        107, //121
                        107, //122
                        107, //123
                        107, //124
                        107, //125
                        107, //126
                        107, //127
                        107,
                    }
                }, //RF_CHAN_6
                {
                    {
                        0  , //0
                        41 , //1
                        43 , //2
                        45 , //3
                        47 , //4
                        49 , //5
                        51 , //6
                        53 , //7
                        55 , //8
                        56 , //9
                        58 , //10
                        60 , //11
                        61 , //12
                        62 , //13
                        63 , //14
                        64 , //15
                        66 , //16
                        67 , //17
                        68 , //18
                        69 , //19
                        70 , //20
                        71 , //21
                        72 , //22
                        73 , //23
                        74 , //24
                        75 , //25
                        76 , //26
                        77 , //27
                        77 , //28
                        78 , //29
                        79 , //30
                        80 , //31
                        80 , //32
                        81 , //33
                        82 , //34
                        83 , //35
                        84 , //36
                        84 , //37
                        85 , //38
                        86 , //39
                        87 , //40
                        87 , //41
                        88 , //42
                        88 , //43
                        89 , //44
                        90 , //45
                        90 , //46
                        91 , //47
                        91 , //48
                        92 , //49
                        92 , //50
                        93 , //51
                        93 , //52
                        94 , //53
                        94 , //54
                        95 , //55
                        95 , //56
                        96 , //57
                        96 , //58
                        97 , //59
                        97 , //60
                        97 , //61
                        98 , //62
                        98 , //63
                        99 , //64
                        99 , //65
                        99 , //66
                        100, //67
                        100, //68
                        100, //69
                        101, //70
                        101, //71
                        101, //72
                        102, //73
                        102, //74
                        102, //75
                        103, //76
                        103, //77
                        103, //78
                        103, //79
                        104, //80
                        104, //81
                        104, //82
                        104, //83
                        104, //84
                        104, //85
                        105, //86
                        105, //87
                        105, //88
                        105, //89
                        105, //90
                        105, //91
                        105, //92
                        105, //93
                        105, //94
                        105, //95
                        105, //96
                        105, //97
                        106, //98
                        106, //99
                        106, //100
                        106, //101
                        106, //102
                        106, //103
                        106, //104
                        106, //105
                        106, //106
                        106, //107
                        106, //108
                        106, //109
                        106, //110
                        106, //111
                        106, //112
                        106, //113
                        106, //114
                        106, //115
                        106, //116
                        106, //117
                        106, //118
                        106, //119
                        106, //120
                        106, //121
                        106, //122
                        106, //123
                        106, //124
                        106, //125
                        106, //126
                        106, //127
                        106,
                    }
                }, //RF_CHAN_7
                {
                    {
                        0  , //0
                        40 , //1
                        42 , //2
                        45 , //3
                        47 , //4
                        49 , //5
                        51 , //6
                        52 , //7
                        54 , //8
                        56 , //9
                        58 , //10
                        59 , //11
                        61 , //12
                        62 , //13
                        63 , //14
                        65 , //15
                        66 , //16
                        67 , //17
                        68 , //18
                        69 , //19
                        70 , //20
                        71 , //21
                        72 , //22
                        73 , //23
                        74 , //24
                        75 , //25
                        76 , //26
                        77 , //27
                        77 , //28
                        78 , //29
                        79 , //30
                        80 , //31
                        81 , //32
                        81 , //33
                        82 , //34
                        83 , //35
                        84 , //36
                        85 , //37
                        86 , //38
                        86 , //39
                        87 , //40
                        88 , //41
                        89 , //42
                        89 , //43
                        90 , //44
                        91 , //45
                        91 , //46
                        92 , //47
                        92 , //48
                        93 , //49
                        93 , //50
                        94 , //51
                        94 , //52
                        95 , //53
                        95 , //54
                        96 , //55
                        96 , //56
                        97 , //57
                        97 , //58
                        97 , //59
                        98 , //60
                        98 , //61
                        99 , //62
                        99 , //63
                        99 , //64
                        100, //65
                        100, //66
                        100, //67
                        101, //68
                        101, //69
                        102, //70
                        102, //71
                        102, //72
                        103, //73
                        103, //74
                        103, //75
                        104, //76
                        104, //77
                        104, //78
                        104, //79
                        105, //80
                        105, //81
                        105, //82
                        105, //83
                        105, //84
                        105, //85
                        105, //86
                        105, //87
                        106, //88
                        106, //89
                        106, //90
                        106, //91
                        106, //92
                        106, //93
                        106, //94
                        106, //95
                        106, //96
                        106, //97
                        106, //98
                        106, //99
                        106, //100
                        106, //101
                        106, //102
                        106, //103
                        106, //104
                        107, //105
                        107, //106
                        107, //107
                        107, //108
                        107, //109
                        107, //110
                        107, //111
                        107, //112
                        107, //113
                        107, //114
                        107, //115
                        107, //116
                        107, //117
                        107, //118
                        107, //119
                        107, //120
                        107, //121
                        107, //122
                        107, //123
                        107, //124
                        107, //125
                        107, //126
                        107, //127
                        107,
                    }
                }, //RF_CHAN_8
                {
                    {
                        0  , //0
                        41 , //1
                        44 , //2
                        46 , //3
                        48 , //4
                        50 , //5
                        52 , //6
                        54 , //7
                        56 , //8
                        58 , //9
                        59 , //10
                        60 , //11
                        62 , //12
                        63 , //13
                        64 , //14
                        66 , //15
                        67 , //16
                        68 , //17
                        69 , //18
                        70 , //19
                        71 , //20
                        72 , //21
                        73 , //22
                        74 , //23
                        75 , //24
                        76 , //25
                        77 , //26
                        78 , //27
                        79 , //28
                        79 , //29
                        80 , //30
                        81 , //31
                        82 , //32
                        83 , //33
                        83 , //34
                        84 , //35
                        85 , //36
                        86 , //37
                        87 , //38
                        87 , //39
                        88 , //40
                        89 , //41
                        89 , //42
                        90 , //43
                        91 , //44
                        91 , //45
                        92 , //46
                        92 , //47
                        93 , //48
                        93 , //49
                        94 , //50
                        94 , //51
                        95 , //52
                        95 , //53
                        96 , //54
                        96 , //55
                        97 , //56
                        97 , //57
                        98 , //58
                        98 , //59
                        98 , //60
                        99 , //61
                        99 , //62
                        100, //63
                        100, //64
                        100, //65
                        101, //66
                        101, //67
                        101, //68
                        102, //69
                        102, //70
                        103, //71
                        103, //72
                        103, //73
                        104, //74
                        104, //75
                        104, //76
                        104, //77
                        105, //78
                        105, //79
                        105, //80
                        105, //81
                        105, //82
                        105, //83
                        106, //84
                        106, //85
                        106, //86
                        106, //87
                        106, //88
                        106, //89
                        106, //90
                        106, //91
                        106, //92
                        106, //93
                        106, //94
                        106, //95
                        106, //96
                        106, //97
                        106, //98
                        107, //99
                        107, //100
                        107, //101
                        107, //102
                        107, //103
                        107, //104
                        107, //105
                        107, //106
                        107, //107
                        107, //108
                        107, //109
                        107, //110
                        107, //111
                        107, //112
                        107, //113
                        107, //114
                        107, //115
                        107, //116
                        107, //117
                        107, //118
                        107, //119
                        107, //120
                        107, //121
                        107, //122
                        107, //123
                        107, //124
                        107, //125
                        107, //126
                        107, //127
                        107,
                    }
                }, //RF_CHAN_9
                {
                    {
                        0  , //0
                        41 , //1
                        43 , //2
                        47 , //3
                        48 , //4
                        50 , //5
                        52 , //6
                        53 , //7
                        55 , //8
                        57 , //9
                        58 , //10
                        60 , //11
                        62 , //12
                        63 , //13
                        64 , //14
                        65 , //15
                        67 , //16
                        68 , //17
                        69 , //18
                        70 , //19
                        71 , //20
                        72 , //21
                        73 , //22
                        74 , //23
                        75 , //24
                        76 , //25
                        77 , //26
                        77 , //27
                        78 , //28
                        79 , //29
                        80 , //30
                        81 , //31
                        82 , //32
                        83 , //33
                        84 , //34
                        85 , //35
                        85 , //36
                        86 , //37
                        87 , //38
                        88 , //39
                        89 , //40
                        89 , //41
                        90 , //42
                        90 , //43
                        91 , //44
                        92 , //45
                        92 , //46
                        93 , //47
                        94 , //48
                        94 , //49
                        95 , //50
                        95 , //51
                        96 , //52
                        96 , //53
                        96 , //54
                        97 , //55
                        97 , //56
                        98 , //57
                        98 , //58
                        99 , //59
                        99 , //60
                        99 , //61
                        100, //62
                        100, //63
                        101, //64
                        101, //65
                        102, //66
                        102, //67
                        102, //68
                        103, //69
                        103, //70
                        103, //71
                        104, //72
                        104, //73
                        104, //74
                        105, //75
                        105, //76
                        105, //77
                        105, //78
                        105, //79
                        106, //80
                        106, //81
                        106, //82
                        106, //83
                        106, //84
                        106, //85
                        106, //86
                        106, //87
                        106, //88
                        107, //89
                        107, //90
                        107, //91
                        107, //92
                        107, //93
                        107, //94
                        107, //95
                        107, //96
                        107, //97
                        107, //98
                        107, //99
                        107, //100
                        107, //101
                        107, //102
                        107, //103
                        107, //104
                        107, //105
                        107, //106
                        107, //107
                        107, //108
                        107, //109
                        107, //110
                        107, //111
                        107, //112
                        107, //113
                        107, //114
                        107, //115
                        107, //116
                        107, //117
                        107, //118
                        107, //119
                        107, //120
                        107, //121
                        107, //122
                        107, //123
                        107, //124
                        107, //125
                        107, //126
                        107, //127
                        107,
                    }
                }, //RF_CHAN_10
                {
                    {
                        0  , //0
                        42 , //1
                        44 , //2
                        47 , //3
                        49 , //4
                        51 , //5
                        52 , //6
                        54 , //7
                        55 , //8
                        57 , //9
                        58 , //10
                        60 , //11
                        61 , //12
                        63 , //13
                        64 , //14
                        65 , //15
                        66 , //16
                        67 , //17
                        69 , //18
                        70 , //19
                        71 , //20
                        72 , //21
                        73 , //22
                        74 , //23
                        75 , //24
                        76 , //25
                        77 , //26
                        77 , //27
                        78 , //28
                        79 , //29
                        80 , //30
                        81 , //31
                        82 , //32
                        82 , //33
                        83 , //34
                        84 , //35
                        85 , //36
                        86 , //37
                        86 , //38
                        87 , //39
                        88 , //40
                        89 , //41
                        90 , //42
                        90 , //43
                        91 , //44
                        91 , //45
                        92 , //46
                        92 , //47
                        93 , //48
                        93 , //49
                        94 , //50
                        94 , //51
                        95 , //52
                        96 , //53
                        96 , //54
                        97 , //55
                        97 , //56
                        97 , //57
                        98 , //58
                        98 , //59
                        99 , //60
                        99 , //61
                        100, //62
                        100, //63
                        100, //64
                        101, //65
                        101, //66
                        101, //67
                        102, //68
                        102, //69
                        102, //70
                        103, //71
                        103, //72
                        103, //73
                        103, //74
                        103, //75
                        103, //76
                        104, //77
                        104, //78
                        104, //79
                        104, //80
                        104, //81
                        104, //82
                        104, //83
                        104, //84
                        104, //85
                        104, //86
                        104, //87
                        105, //88
                        105, //89
                        105, //90
                        105, //91
                        105, //92
                        105, //93
                        105, //94
                        105, //95
                        105, //96
                        105, //97
                        105, //98
                        105, //99
                        105, //100
                        105, //101
                        105, //102
                        105, //103
                        105, //104
                        105, //105
                        105, //106
                        105, //107
                        105, //108
                        105, //109
                        105, //110
                        105, //111
                        105, //112
                        105, //113
                        105, //114
                        105, //115
                        105, //116
                        105, //117
                        105, //118
                        105, //119
                        105, //120
                        105, //121
                        105, //122
                        105, //123
                        105, //124
                        105, //125
                        105, //126
                        105, //127
                    }
                }, //RF_CHAN_11
                {
                    {
                        0  , //0
                        41 , //1
                        44 , //2
                        46 , //3
                        48 , //4
                        50 , //5
                        52 , //6
                        54 , //7
                        56 , //8
                        57 , //9
                        59 , //10
                        60 , //11
                        61 , //12
                        63 , //13
                        64 , //14
                        65 , //15
                        66 , //16
                        67 , //17
                        69 , //18
                        70 , //19
                        71 , //20
                        72 , //21
                        73 , //22
                        74 , //23
                        75 , //24
                        76 , //25
                        77 , //26
                        77 , //27
                        78 , //28
                        79 , //29
                        80 , //30
                        80 , //31
                        81 , //32
                        82 , //33
                        83 , //34
                        83 , //35
                        84 , //36
                        85 , //37
                        86 , //38
                        86 , //39
                        87 , //40
                        88 , //41
                        88 , //42
                        89 , //43
                        90 , //44
                        90 , //45
                        91 , //46
                        92 , //47
                        92 , //48
                        93 , //49
                        93 , //50
                        94 , //51
                        94 , //52
                        95 , //53
                        95 , //54
                        96 , //55
                        96 , //56
                        96 , //57
                        97 , //58
                        97 , //59
                        98 , //60
                        98 , //61
                        99 , //62
                        99 , //63
                        99 , //64
                        100, //65
                        100, //66
                        100, //67
                        101, //68
                        101, //69
                        101, //70
                        102, //71
                        102, //72
                        102, //73
                        103, //74
                        103, //75
                        103, //76
                        103, //77
                        103, //78
                        103, //79
                        103, //80
                        104, //81
                        104, //82
                        104, //83
                        104, //84
                        104, //85
                        104, //86
                        104, //87
                        104, //88
                        104, //89
                        104, //90
                        104, //91
                        104, //92
                        104, //93
                        105, //94
                        105, //95
                        105, //96
                        105, //97
                        105, //98
                        105, //99
                        105, //100
                        105, //101
                        105, //102
                        105, //103
                        105, //104
                        105, //105
                        105, //106
                        105, //107
                        105, //108
                        105, //109
                        105, //110
                        105, //111
                        105, //112
                        105, //113
                        105, //114
                        105, //115
                        105, //116
                        105, //117
                        105, //118
                        105, //119
                        105, //120
                        105, //121
                        105, //122
                        105, //123
                        105, //124
                        105, //125
                        105, //126
                        105, //127
                        105,
                    }
                }, //RF_CHAN_12
                {
                    {
                        0  , //0
                        42 , //1
                        44 , //2
                        46 , //3
                        48 , //4
                        50 , //5
                        52 , //6
                        54 , //7
                        56 , //8
                        58 , //9
                        59 , //10
                        60 , //11
                        61 , //12
                        63 , //13
                        64 , //14
                        65 , //15
                        66 , //16
                        68 , //17
                        69 , //18
                        70 , //19
                        71 , //20
                        72 , //21
                        73 , //22
                        74 , //23
                        75 , //24
                        75 , //25
                        76 , //26
                        77 , //27
                        78 , //28
                        79 , //29
                        80 , //30
                        80 , //31
                        81 , //32
                        82 , //33
                        83 , //34
                        83 , //35
                        84 , //36
                        85 , //37
                        86 , //38
                        86 , //39
                        87 , //40
                        88 , //41
                        89 , //42
                        89 , //43
                        90 , //44
                        91 , //45
                        91 , //46
                        92 , //47
                        93 , //48
                        93 , //49
                        94 , //50
                        94 , //51
                        95 , //52
                        95 , //53
                        96 , //54
                        96 , //55
                        97 , //56
                        97 , //57
                        97 , //58
                        98 , //59
                        98 , //60
                        99 , //61
                        99 , //62
                        100, //63
                        100, //64
                        100, //65
                        101, //66
                        101, //67
                        101, //68
                        102, //69
                        102, //70
                        102, //71
                        103, //72
                        103, //73
                        103, //74
                        103, //75
                        103, //76
                        103, //77
                        104, //78
                        104, //79
                        104, //80
                        104, //81
                        104, //82
                        104, //83
                        104, //84
                        104, //85
                        104, //86
                        104, //87
                        104, //88
                        104, //89
                        105, //90
                        105, //91
                        105, //92
                        105, //93
                        105, //94
                        105, //95
                        105, //96
                        105, //97
                        105, //98
                        105, //99
                        105, //100
                        105, //101
                        105, //102
                        105, //103
                        105, //104
                        105, //105
                        105, //106
                        105, //107
                        105, //108
                        105, //109
                        105, //110
                        105, //111
                        105, //112
                        105, //113
                        105, //114
                        105, //115
                        105, //116
                        105, //117
                        105, //118
                        105, //119
                        105, //120
                        105, //121
                        105, //122
                        105, //123
                        105, //124
                        105, //125
                        105, //126
                        105, //127
                        105,
                    }
                }, //RF_CHAN_13
                {
                    {
                       0,  //0
                       40,  //1
                       43,  //2
                       45,  //3
                       47,  //4
                       49,  //5
                       50,  //6
                       52,  //7
                       54,  //8
                       56,  //9
                       57,  //10
                       58,  //11
                       59,  //12
                       60,  //13
                       62,  //14
                       63,  //15
                       64,  //16
                       65,  //17
                       66,  //18
                       67,  //19
                       68,  //20
                       69,  //21
                       70,  //22
                       71,  //23
                       72,  //24
                       73,  //25
                       74,  //26
                       74,  //27
                       75,  //28
                       76,  //29
                       77,  //30
                       78,  //31
                       78,  //32
                       79,  //33
                       80,  //34
                       81,  //35
                       82,  //36
                       83,  //37
                       83,  //38
                       84,  //39
                       85,  //40
                       85,  //41
                       86,  //42
                       87,  //43
                       87,  //44
                       88,  //45
                       89,  //46
                       89,  //47
                       90,  //48
                       90,  //49
                       91,  //50
                       91,  //51
                       92,  //52
                       92,  //53
                       93,  //54
                       93,  //55
                       94,  //56
                       94,  //57
                       95,  //58
                       95,  //59
                       96,  //60
                       96,  //61
                       96,  //62
                       97,  //63
                       97,  //64
                       97,  //65
                       98,  //66
                       98,  //67
                       98,  //68
                       98,  //69
                       99,  //70
                       99,  //71
                       99,  //72
                       99,  //73
                       99,  //74
                       99,  //75
                       99,  //76
                       99,  //77
                       99,  //78
                       99,  //79
                       100,  //80
                       100,  //81
                       100,  //82
                       100,  //83
                       100,  //84
                       100,  //85
                       100,  //86
                       100,  //87
                       100,  //88
                       100,  //89
                       100,  //90
                       100,  //91
                       100,  //92
                       100,  //93
                       100,  //94
                       100,  //95
                       100,  //96
                       100,  //97
                       100, //98
                       100, //99
                       100, //100
                       100, //101
                       100, //102
                       100, //103
                       100, //104
                       100, //105
                       100, //106
                       100, //107
                       100, //108
                       100, //109
                       100, //110
                       100, //111
                       100, //112
                       100, //113
                       100, //114
                       100, //115
                       100, //116
                       100, //117
                       100, //118
                       100, //119
                       100, //120
                       100, //121
                       100, //122
                       100, //123
                       100, //124
                       100, //125
                       100, //126
                       100, //127
                       100,
                    }
                }, //RF_CHAN_14
        },

        //NV_TABLE_TPC_PDADC_OFFSETS
        {
            98,  // RF_CHAN_1
            101,  // RF_CHAN_2
            101,  // RF_CHAN_3
            100,  // RF_CHAN_4
            98,  // RF_CHAN_5
            97,  // RF_CHAN_6
            94,  // RF_CHAN_7
            94,  // RF_CHAN_8
            92,  // RF_CHAN_9
            90,  // RF_CHAN_10
            94,  // RF_CHAN_11
            95,  // RF_CHAN_12
            97,  // RF_CHAN_13
            104,   // RF_CHAN_14
            100,   // RF_CHAN_240
            100,   // RF_CHAN_244
            100,   // RF_CHAN_248
            100,   // RF_CHAN_252
            100,   // RF_CHAN_208
            100,   // RF_CHAN_212
            100,   // RF_CHAN_216
            100,   // RF_CHAN_36
            100,   // RF_CHAN_40
            100,   // RF_CHAN_44
            100,   // RF_CHAN_48
            100,   // RF_CHAN_52
            100,   // RF_CHAN_56
            100,   // RF_CHAN_60
            100,   // RF_CHAN_64
            100,   // RF_CHAN_100
            100,   // RF_CHAN_104
            100,   // RF_CHAN_108
            100,   // RF_CHAN_112
            100,   // RF_CHAN_116
            100,   // RF_CHAN_120
            100,   // RF_CHAN_124
            100,   // RF_CHAN_128
            100,   // RF_CHAN_132
            100,   // RF_CHAN_136
            100,   // RF_CHAN_140
            100,   // RF_CHAN_149
            100,   // RF_CHAN_153
            100,   // RF_CHAN_157
            100,   // RF_CHAN_161
            100,   // RF_CHAN_165
            //CHANNEL BONDED CHANNELS
            100,   // RF_CHAN_BOND_3
            100,   // RF_CHAN_BOND_4
            100,   // RF_CHAN_BOND_5
            100,   // RF_CHAN_BOND_6
            100,   // RF_CHAN_BOND_7
            100,   // RF_CHAN_BOND_8
            100,   // RF_CHAN_BOND_9
            100,   // RF_CHAN_BOND_10
            100,   // RF_CHAN_BOND_11
            100,   // RF_CHAN_BOND_242
            100,   // RF_CHAN_BOND_246
            100,   // RF_CHAN_BOND_250
            100,   // RF_CHAN_BOND_210
            100,   // RF_CHAN_BOND_214
            100,   // RF_CHAN_BOND_38
            100,   // RF_CHAN_BOND_42
            100,   // RF_CHAN_BOND_46
            100,   // RF_CHAN_BOND_50
            100,   // RF_CHAN_BOND_54
            100,   // RF_CHAN_BOND_58
            100,   // RF_CHAN_BOND_62
            100,   // RF_CHAN_BOND_102
            100,   // RF_CHAN_BOND_106
            100,   // RF_CHAN_BOND_110
            100,   // RF_CHAN_BOND_114
            100,   // RF_CHAN_BOND_118
            100,   // RF_CHAN_BOND_122
            100,   // RF_CHAN_BOND_126
            100,   // RF_CHAN_BOND_130
            100,   // RF_CHAN_BOND_134
            100,   // RF_CHAN_BOND_138
            100,   // RF_CHAN_BOND_151
            100,   // RF_CHAN_BOND_155
            100,   // RF_CHAN_BOND_159
            100,   // RF_CHAN_BOND_163
        },

        //NV_TABLE_VIRTUAL_RATE
        // typedef tANI_S16 tPowerdBm;
        //typedef tPowerdBm tRateGroupPwr[NUM_HAL_PHY_RATES];
        //tRateGroupPwr       pwrOptimum[NUM_RF_SUBBANDS];
        {
            // 2.4G RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 5G Low RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 5G Middle RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 5G High RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            },
            // 4.9G RF Subband
            {
                //802.11b Rates
                {100},    // HAL_PHY_VRATE_11A_54_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_65_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_72_2_MBPS,
                {100},    // HAL_PHY_VRATE_MCS_1NSS_CB_135_MBPS
                {100},    // HAL_PHY_VRATE_MCS_1NSS_MM_SG_CB_150_MBPS,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
                {100},    // RESERVED,
            }
        },

#if 0 //FIXME_PRIMA
        //NV_TABLE_CAL_MEMORY
        {
            0x7FFF,      // tANI_U16    process_monitor;
            0x00,        // tANI_U8     hdet_cal_code;
            0x00,        // tANI_U8     rxfe_gm_2;

            0x00,        // tANI_U8     tx_bbf_rtune;
            0x00,        // tANI_U8     pa_rtune_reg;
            0x00,        // tANI_U8     rt_code;
            0x00,        // tANI_U8     bias_rtune;

            0x00,        // tANI_U8     bb_bw1;
            0x00,        // tANI_U8     bb_bw2;
            { 0x00, 0x00 },        // tANI_U8     reserved[2];

            0x00,        // tANI_U8     bb_bw3;
            0x00,        // tANI_U8     bb_bw4;
            0x00,        // tANI_U8     bb_bw5;
            0x00,        // tANI_U8     bb_bw6;

            0x7FFF,      // tANI_U16    rcMeasured;
            0x00,        // tANI_U8     tx_bbf_ct;
            0x00,        // tANI_U8     tx_bbf_ctr;

            0x00,        // tANI_U8     csh_maxgain_reg;
            0x00,        // tANI_U8     csh_0db_reg;
            0x00,        // tANI_U8     csh_m3db_reg;
            0x00,        // tANI_U8     csh_m6db_reg;

            0x00,        // tANI_U8     cff_0db_reg;
            0x00,        // tANI_U8     cff_m3db_reg;
            0x00,        // tANI_U8     cff_m6db_reg;
            0x00,        // tANI_U8     rxfe_gpio_ctl_1;

            0x00,        // tANI_U8     mix_bal_cnt_2;
            0x00,        // tANI_S8     rxfe_lna_highgain_bias_ctl_delta;
            0x00,        // tANI_U8     rxfe_lna_load_ctune;
            0x00,        // tANI_U8     rxfe_lna_ngm_rtune;

            0x00,        // tANI_U8     rx_im2_i_cfg0;
            0x00,        // tANI_U8     rx_im2_i_cfg1;
            0x00,        // tANI_U8     rx_im2_q_cfg0;
            0x00,        // tANI_U8     rx_im2_q_cfg1;

            0x00,        // tANI_U8     pll_vfc_reg3_b0;
            0x00,        // tANI_U8     pll_vfc_reg3_b1;
            0x00,        // tANI_U8     pll_vfc_reg3_b2;
            0x00,        // tANI_U8     pll_vfc_reg3_b3;

            0x7FFF,        // tANI_U16    tempStart;
            0x7FFF,        // tANI_U16    tempFinish;

            { //txLoCorrections
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_1
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_2
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_3
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_4
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_5
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_6
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_7
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_8
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_9
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_10
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_11
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_12
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_13
                {
                    { 0x00, 0x00 }, // TX_GAIN_STEP_0
                    { 0x00, 0x00 }, // TX_GAIN_STEP_1
                    { 0x00, 0x00 }, // TX_GAIN_STEP_2
                    { 0x00, 0x00 }, // TX_GAIN_STEP_3
                    { 0x00, 0x00 }, // TX_GAIN_STEP_4
                    { 0x00, 0x00 }, // TX_GAIN_STEP_5
                    { 0x00, 0x00 }, // TX_GAIN_STEP_6
                    { 0x00, 0x00 }, // TX_GAIN_STEP_7
                    { 0x00, 0x00 }, // TX_GAIN_STEP_8
                    { 0x00, 0x00 }, // TX_GAIN_STEP_9
                    { 0x00, 0x00 }, // TX_GAIN_STEP_10
                    { 0x00, 0x00 }, // TX_GAIN_STEP_11
                    { 0x00, 0x00 }, // TX_GAIN_STEP_12
                    { 0x00, 0x00 }, // TX_GAIN_STEP_13
                    { 0x00, 0x00 }, // TX_GAIN_STEP_14
                    { 0x00, 0x00 }  // TX_GAIN_STEP_15
                }  //RF_CHAN_14
            },        // tTxLoCorrections    txLoValues;

            { //sTxIQChannel
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_1
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_2
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_3
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_4
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_5
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_6
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_7
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_8
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_9
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_10
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_11
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_12
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }, //RF_CHAN_13
                {
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // TX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // TX_GAIN_STEP_15
                }  //RF_CHAN_14
            },        // sTxIQChannel        txIqValues;

            { //sRxIQChannel
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_1
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_2
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_3
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_4
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_5
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_6
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_7
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_8
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_9
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_10
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_11
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_12
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }, //RF_CHAN_13
                {
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_0
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_1
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_2
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_3
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_4
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_5
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_6
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_7
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_8
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_9
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_10
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_11
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_12
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_13
                    { 0x0000, 0x0000, 0x0000 }, // RX_GAIN_STEP_14
                    { 0x0000, 0x0000, 0x0000 }  // RX_GAIN_STEP_15
                }  //RF_CHAN_14
            },        // sRxIQChannel        rxIqValues;

            { // tTpcConfig          clpcData[MAX_TPC_CHANNELS]
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_1
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_2
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_3
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_4
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_5
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_6
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_7
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_8
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_9
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_10
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_11
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }, // RF_CHAN_12
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                },  // RF_CHAN_13
                {
                    {
                        {
                            { 0x00, 0x00 }, //CAL_POINT_0
                            { 0x00, 0x00 }, //CAL_POINT_1
                            { 0x00, 0x00 }, //CAL_POINT_2
                            { 0x00, 0x00 }, //CAL_POINT_3
                            { 0x00, 0x00 }, //CAL_POINT_4
                            { 0x00, 0x00 }, //CAL_POINT_5
                            { 0x00, 0x00 }, //CAL_POINT_6
                            { 0x00, 0x00 }  //CAL_POINT_7
                        } // PHY_TX_CHAIN_0
                    } // empirical
                }  // RF_CHAN_14
            },        // tTpcConfig          clpcData[MAX_TPC_CHANNELS];

            {
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_1: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_2: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_3: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_4: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_5: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_6: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_7: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_8: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_9: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_10: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_11: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_12: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }, // RF_CHAN_13: pdadc_offset, reserved[2]
                { 0x0000, { 0x00, 0x00 } }  // RF_CHAN_14: pdadc_offset, reserved[2]
            }        // tTpcParams          clpcParams[MAX_TPC_CHANNELS];

        }, //NV_TABLE_CAL_MEMORY
#endif
        //NV_TABLE_FW_CONFIG
        {
            0,   //skuID
            0,   //tpcMode2G
            0,   //tpcMode5G
            0,   //reserved1

            0,   //xPA2G
            0,   //xPA5G;
            0,   //paPolarityTx;
            0,   //paPolarityRx;
                
            0,   //xLNA2G;
            0,   //xLNA5G;
            0,   //xCoupler2G;
            0,   //xCoupler5G;
                
            0,   //xPdet2G;
            0,   //xPdet5G;
            0,   //enableDPD2G;
            1,   //enableDPD5G;
                
            1,   //pdadcSelect2G;
            1,   //pdadcSelect5GLow;
            1,   //pdadcSelect5GMid;
            1,   //pdadcSelect5GHigh;

            0,   //reserved2
            0,   //reserved3
            0,   //reserved4
        },


        //NV_TABLE_RSSI_CHANNEL_OFFSETS
        {
            //PHY_RX_CHAIN_0
            {
                //bRssiOffset
                {300}, // apply to all channles

                //gnRssiOffset
                {300}  // apply to all channles
            },
            //rsvd
            {
                //bRssiOffset
                {0},   // apply to all channles

                //gnRssiOffset
                {0}    // apply to all channles
            }
        },

        //NV_TABLE_HW_CAL_VALUES
        {
            0x0,             //validBmap
            {
                1400,        //psSlpTimeOvrHd2G;
                1400,        //psSlpTimeOvrHd5G;
                
                1600,        //psSlpTimeOvrHdxLNA5G;
                0,           //nv_TxBBFSel9MHz 
                0,           //hwParam1
                0,           //hwParam2

                0x1B,        //custom_tcxo_reg8
                0xFF,        //custom_tcxo_reg9

                0,           //hwParam3;
                0,           //hwParam4;
                0,           //hwParam5;
                0,           //hwParam6;
                0,           //hwParam7;
                0,           //hwParam8;
                0,           //hwParam9;
                0,           //hwParam10;
                0,           //hwParam11;
            }
        },


        //NV_TABLE_ANTENNA_PATH_LOSS
        {
            280,  // RF_CHAN_1
            270,  // RF_CHAN_2
            270,  // RF_CHAN_3
            270,  // RF_CHAN_4
            270,  // RF_CHAN_5
            270,  // RF_CHAN_6
            280,  // RF_CHAN_7
            280,  // RF_CHAN_8
            290,  // RF_CHAN_9
            300,  // RF_CHAN_10
            300,  // RF_CHAN_11
            310,  // RF_CHAN_12
            310,  // RF_CHAN_13
            310,   // RF_CHAN_14
            280,  // RF_CHAN_240
            280,  // RF_CHAN_244
            280,   // RF_CHAN_248
            280,   // RF_CHAN_252
            280,   // RF_CHAN_208
            280,   // RF_CHAN_212
            280,   // RF_CHAN_216
            280,   // RF_CHAN_36
            280,   // RF_CHAN_40
            280,   // RF_CHAN_44
            280,   // RF_CHAN_48
            280,   // RF_CHAN_52
            280,   // RF_CHAN_56
            280,   // RF_CHAN_60
            280,   // RF_CHAN_64
            280,   // RF_CHAN_100
            280,   // RF_CHAN_104
            280,   // RF_CHAN_108
            280,   // RF_CHAN_112
            280,   // RF_CHAN_116
            280,   // RF_CHAN_120
            280,   // RF_CHAN_124
            280,   // RF_CHAN_128
            280,   // RF_CHAN_132
            280,   // RF_CHAN_136
            280,   // RF_CHAN_140
            280,   // RF_CHAN_149
            280,   // RF_CHAN_153
            280,   // RF_CHAN_157
            280,   // RF_CHAN_161
            280,   // RF_CHAN_165
            //CHANNEL BONDED CHANNELS
            280,   // RF_CHAN_BOND_3
            280,   // RF_CHAN_BOND_4
            280,   // RF_CHAN_BOND_5
            280,   // RF_CHAN_BOND_6
            280,   // RF_CHAN_BOND_7
            280,   // RF_CHAN_BOND_8
            280,   // RF_CHAN_BOND_9
            280,   // RF_CHAN_BOND_10
            280,   // RF_CHAN_BOND_11
            280,   // RF_CHAN_BOND_242
            280,   // RF_CHAN_BOND_246
            280,   // RF_CHAN_BOND_250
            280,   // RF_CHAN_BOND_210
            280,   // RF_CHAN_BOND_214
            280,   // RF_CHAN_BOND_38
            280,   // RF_CHAN_BOND_42
            280,   // RF_CHAN_BOND_46
            280,   // RF_CHAN_BOND_50
            280,   // RF_CHAN_BOND_54
            280,   // RF_CHAN_BOND_58
            280,   // RF_CHAN_BOND_62
            280,   // RF_CHAN_BOND_102
            280,   // RF_CHAN_BOND_106
            280,   // RF_CHAN_BOND_110
            280,   // RF_CHAN_BOND_114
            280,   // RF_CHAN_BOND_118
            280,   // RF_CHAN_BOND_122
            280,   // RF_CHAN_BOND_126
            280,   // RF_CHAN_BOND_130
            280,   // RF_CHAN_BOND_134
            280,   // RF_CHAN_BOND_138
            280,   // RF_CHAN_BOND_151
            280,   // RF_CHAN_BOND_155
            280,   // RF_CHAN_BOND_159
            280,   // RF_CHAN_BOND_163
        },

        //NV_TABLE_PACKET_TYPE_POWER_LIMITS
        {
            { 2150 }, // applied to all channels, MODE_802_11B
            { 1850 }, // applied to all channels,MODE_802_11AG
            { 1750 }  // applied to all channels,MODE_802_11N
        },

        //NV_TABLE_OFDM_CMD_PWR_OFFSET
        {
            0, 0
        },

        //NV_TABLE_TX_BB_FILTER_MODE
        {
            0
        }

    } // tables
};

#endif


