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

#ifndef WLAN_QCT_WDI_CFG_H
#define WLAN_QCT_WDI_CFG_H

/*===========================================================================

         W L A N   D E V I C E   A B S T R A C T I O N   L A Y E R 
              C O N F I G U R A T I O N   D E F I N E S
                         E X T E R N A L   A P I                
                   
DESCRIPTION
  This file contains the configuration defines to be used by the UMAC for
  setting up the config parameters in DAL.
 
  !! The values in here should be an identical match of the HAL defines
  by the same name !! 
  
      
  Copyright (c) 2010 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
08/19/10    lti     Created module.

===========================================================================*/

/*-------------------------------------------------------------------------
  Preprocessor definitions and constants
-------------------------------------------------------------------------*/
#define WDI_MAX_CFG_LENGTH                  0x06

/*-------------------------------------------------------------------------
  Configuration Parameter IDs
-------------------------------------------------------------------------*/
#define WDI_CFG_STA_ID                             0
#define WDI_CFG_CURRENT_TX_ANTENNA                 1
#define WDI_CFG_CURRENT_RX_ANTENNA                 2
#define WDI_CFG_LOW_GAIN_OVERRIDE                  3
#define WDI_CFG_POWER_STATE_PER_CHAIN              4
#define WDI_CFG_CAL_PERIOD                         5
#define WDI_CFG_CAL_CONTROL                        6
#define WDI_CFG_PROXIMITY                          7
#define WDI_CFG_NETWORK_DENSITY                    8
#define WDI_CFG_MAX_MEDIUM_TIME                    9
#define WDI_CFG_MAX_MPDUS_IN_AMPDU                 10
#define WDI_CFG_RTS_THRESHOLD                      11
#define WDI_CFG_SHORT_RETRY_LIMIT                  12
#define WDI_CFG_LONG_RETRY_LIMIT                   13
#define WDI_CFG_FRAGMENTATION_THRESHOLD            14
#define WDI_CFG_DYNAMIC_THRESHOLD_ZERO             15
#define WDI_CFG_DYNAMIC_THRESHOLD_ONE              16
#define WDI_CFG_DYNAMIC_THRESHOLD_TWO              17
#define WDI_CFG_FIXED_RATE                         18
#define WDI_CFG_RETRYRATE_POLICY                   19
#define WDI_CFG_RETRYRATE_SECONDARY                20
#define WDI_CFG_RETRYRATE_TERTIARY                 21
#define WDI_CFG_FORCE_POLICY_PROTECTION            22
#define WDI_CFG_FIXED_RATE_MULTICAST_24GHZ         23
#define WDI_CFG_FIXED_RATE_MULTICAST_5GHZ          24
#define WDI_CFG_DEFAULT_RATE_INDEX_24GHZ           25
#define WDI_CFG_DEFAULT_RATE_INDEX_5GHZ            26
#define WDI_CFG_MAX_BA_SESSIONS                    27
#define WDI_CFG_PS_DATA_INACTIVITY_TIMEOUT         28
#define WDI_CFG_PS_ENABLE_BCN_FILTER               29
#define WDI_CFG_PS_ENABLE_RSSI_MONITOR             30
#define WDI_CFG_NUM_BEACON_PER_RSSI_AVERAGE        31
#define WDI_CFG_STATS_PERIOD                       32
#define WDI_CFG_CFP_MAX_DURATION                   33

/*-------------------------------------------------------------------------
  Configuration Parameter min, max, defaults
-------------------------------------------------------------------------*/

/* WDI_CFG_CURRENT_TX_ANTENNA */
#define WDI_CFG_CURRENT_TX_ANTENNA_STAMIN          1
#define WDI_CFG_CURRENT_TX_ANTENNA_STAMAX          1
#define WDI_CFG_CURRENT_TX_ANTENNA_STADEF          1

/* WDI_CFG_CURRENT_RX_ANTENNA */
#define WDI_CFG_CURRENT_RX_ANTENNA_STAMIN          1
#define WDI_CFG_CURRENT_RX_ANTENNA_STAMAX          2
#define WDI_CFG_CURRENT_RX_ANTENNA_STADEF          1

/* WDI_CFG_LOW_GAIN_OVERRIDE */
#define WDI_CFG_LOW_GAIN_OVERRIDE_STAMIN           0
#define WDI_CFG_LOW_GAIN_OVERRIDE_STAMAX           1
#define WDI_CFG_LOW_GAIN_OVERRIDE_STADEF           0

/* WDI_CFG_POWER_STATE_PER_CHAIN */
#define WDI_CFG_POWER_STATE_PER_CHAIN_STAMIN             0
#define WDI_CFG_POWER_STATE_PER_CHAIN_STAMAX             65535
#define WDI_CFG_POWER_STATE_PER_CHAIN_STADEF             785
#define WDI_CFG_POWER_STATE_PER_CHAIN_OFF                0
#define WDI_CFG_POWER_STATE_PER_CHAIN_ON                 1
#define WDI_CFG_POWER_STATE_PER_CHAIN_TX                 2
#define WDI_CFG_POWER_STATE_PER_CHAIN_RX                 3
#define WDI_CFG_POWER_STATE_PER_CHAIN_MASK               15
#define WDI_CFG_POWER_STATE_PER_CHAIN_CHAIN_0_OFFSET     0
#define WDI_CFG_POWER_STATE_PER_CHAIN_CHAIN_1_OFFSET     4
#define WDI_CFG_POWER_STATE_PER_CHAIN_CHAIN_2_OFFSET     8

/* WDI_CFG_CAL_PERIOD */
#define WDI_CFG_CAL_PERIOD_STAMIN                  2
#define WDI_CFG_CAL_PERIOD_STAMAX                  10
#define WDI_CFG_CAL_PERIOD_STADEF                  5

/* WDI_CFG_CAL_CONTROL */
#define WDI_CFG_CAL_CONTROL_STAMIN    0
#define WDI_CFG_CAL_CONTROL_STAMAX    1
#define WDI_CFG_CAL_CONTROL_STADEF    0

/* WDI_CFG_PROXIMITY */
#define WDI_CFG_PROXIMITY_STAMIN    0
#define WDI_CFG_PROXIMITY_STAMAX    1
#define WDI_CFG_PROXIMITY_STADEF    0
#define WDI_CFG_PROXIMITY_OFF       0
#define WDI_CFG_PROXIMITY_ON        1

/* WDI_CFG_NETWORK_DENSITY */
#define WDI_CFG_NETWORK_DENSITY_STAMIN    0
#define WDI_CFG_NETWORK_DENSITY_STAMAX    3
#define WDI_CFG_NETWORK_DENSITY_STADEF    3
#define WDI_CFG_NETWORK_DENSITY_LOW       0
#define WDI_CFG_NETWORK_DENSITY_MEDIUM    1
#define WDI_CFG_NETWORK_DENSITY_HIGH      2
#define WDI_CFG_NETWORK_DENSITY_ADAPTIVE  3

/* WDI_CFG_MAX_MEDIUM_TIME */
#define WDI_CFG_MAX_MEDIUM_TIME_STAMIN    0
#define WDI_CFG_MAX_MEDIUM_TIME_STAMAX    65535
#define WDI_CFG_MAX_MEDIUM_TIME_STADEF    1024

/* WDI_CFG_MAX_MPDUS_IN_AMPDU */
#define WDI_CFG_MAX_MPDUS_IN_AMPDU_STAMIN    0
#define WDI_CFG_MAX_MPDUS_IN_AMPDU_STAMAX    65535
#define WDI_CFG_MAX_MPDUS_IN_AMPDU_STADEF    64

/* WDI_CFG_RTS_THRESHOLD */
#define WDI_CFG_RTS_THRESHOLD_STAMIN               0
#define WDI_CFG_RTS_THRESHOLD_STAMAX               2347
#define WDI_CFG_RTS_THRESHOLD_STADEF               2347

/* WDI_CFG_SHORT_RETRY_LIMIT */
#define WDI_CFG_SHORT_RETRY_LIMIT_STAMIN    0
#define WDI_CFG_SHORT_RETRY_LIMIT_STAMAX    255
#define WDI_CFG_SHORT_RETRY_LIMIT_STADEF    6

/* WDI_CFG_LONG_RETRY_LIMIT */
#define WDI_CFG_LONG_RETRY_LIMIT_STAMIN    0
#define WDI_CFG_LONG_RETRY_LIMIT_STAMAX    255
#define WDI_CFG_LONG_RETRY_LIMIT_STADEF    6

/* WDI_CFG_FRAGMENTATION_THRESHOLD */
#define WDI_CFG_FRAGMENTATION_THRESHOLD_STAMIN    256
#define WDI_CFG_FRAGMENTATION_THRESHOLD_STAMAX    8000
#define WDI_CFG_FRAGMENTATION_THRESHOLD_STADEF    8000

/* WDI_CFG_DYNAMIC_THRESHOLD_ZERO */
#define WDI_CFG_DYNAMIC_THRESHOLD_ZERO_STAMIN    0
#define WDI_CFG_DYNAMIC_THRESHOLD_ZERO_STAMAX    255
#define WDI_CFG_DYNAMIC_THRESHOLD_ZERO_STADEF    2

/* WDI_CFG_DYNAMIC_THRESHOLD_ONE */
#define WDI_CFG_DYNAMIC_THRESHOLD_ONE_STAMIN    0
#define WDI_CFG_DYNAMIC_THRESHOLD_ONE_STAMAX    255
#define WDI_CFG_DYNAMIC_THRESHOLD_ONE_STADEF    4

/* WDI_CFG_DYNAMIC_THRESHOLD_TWO */
#define WDI_CFG_DYNAMIC_THRESHOLD_TWO_STAMIN    0
#define WDI_CFG_DYNAMIC_THRESHOLD_TWO_STAMAX    255
#define WDI_CFG_DYNAMIC_THRESHOLD_TWO_STADEF    6

/* WDI_CFG_FIXED_RATE */
#define WDI_CFG_FIXED_RATE_STAMIN                        0
#define WDI_CFG_FIXED_RATE_STAMAX                        31
#define WDI_CFG_FIXED_RATE_STADEF                        0
#define WDI_CFG_FIXED_RATE_AUTO                          0
#define WDI_CFG_FIXED_RATE_1MBPS                         1
#define WDI_CFG_FIXED_RATE_2MBPS                         2
#define WDI_CFG_FIXED_RATE_5_5MBPS                       3
#define WDI_CFG_FIXED_RATE_11MBPS                        4
#define WDI_CFG_FIXED_RATE_6MBPS                         5
#define WDI_CFG_FIXED_RATE_9MBPS                         6
#define WDI_CFG_FIXED_RATE_12MBPS                        7
#define WDI_CFG_FIXED_RATE_18MBPS                        8
#define WDI_CFG_FIXED_RATE_24MBPS                        9
#define WDI_CFG_FIXED_RATE_36MBPS                        10
#define WDI_CFG_FIXED_RATE_48MBPS                        11
#define WDI_CFG_FIXED_RATE_54MBPS                        12
#define WDI_CFG_FIXED_RATE_6_5MBPS_MCS0_20MHZ_SIMO       13
#define WDI_CFG_FIXED_RATE_13MBPS_MCS1_20MHZ_SIMO        14
#define WDI_CFG_FIXED_RATE_19_5MBPS_MCS2_20MHZ_SIMO      15
#define WDI_CFG_FIXED_RATE_26MBPS_MCS3_20MHZ_SIMO        16
#define WDI_CFG_FIXED_RATE_39MBPS_MCS4_20MHZ_SIMO        17
#define WDI_CFG_FIXED_RATE_52MBPS_MCS5_20MHZ_SIMO        18
#define WDI_CFG_FIXED_RATE_58_5MBPS_MCS6_20MHZ_SIMO      19
#define WDI_CFG_FIXED_RATE_65MBPS_MCS7_20MHZ_SIMO        20
#define WDI_CFG_FIXED_RATE_7_2MBPS_MCS0_20MHZ_SIMO_SGI   21
#define WDI_CFG_FIXED_RATE_14_4MBPS_MCS1_20MHZ_SIMO_SGI  22
#define WDI_CFG_FIXED_RATE_21_7MBPS_MCS2_20MHZ_SIMO_SGI  23
#define WDI_CFG_FIXED_RATE_28_9MBPS_MCS3_20MHZ_SIMO_SGI  24
#define WDI_CFG_FIXED_RATE_43_3MBPS_MCS4_20MHZ_SIMO_SGI  25
#define WDI_CFG_FIXED_RATE_57_8MBPS_MCS5_20MHZ_SIMO_SGI  26
#define WDI_CFG_FIXED_RATE_65MBPS_MCS6_20MHZ_SIMO_SGI    27
#define WDI_CFG_FIXED_RATE_72_2MBPS_MCS7_20MHZ_SIMO_SGI  28
#define WDI_CFG_FIXED_RATE_0_25MBPS_SLR_20MHZ_SIMO       29
#define WDI_CFG_FIXED_RATE_0_5MBPS_SLR_20MHZ_SIMO        30
#define WDI_CFG_FIXED_RATE_68_25MBPS_QC_PROP_20MHZ_SIMO  31

/* WDI_CFG_RETRYRATE_POLICY */
#define WDI_CFG_RETRYRATE_POLICY_STAMIN         0
#define WDI_CFG_RETRYRATE_POLICY_STAMAX         255
#define WDI_CFG_RETRYRATE_POLICY_STADEF         4
#define WDI_CFG_RETRYRATE_POLICY_MIN_SUPPORTED  0
#define WDI_CFG_RETRYRATE_POLICY_PRIMARY        1
#define WDI_CFG_RETRYRATE_POLICY_RESERVED       2
#define WDI_CFG_RETRYRATE_POLICY_CLOSEST        3
#define WDI_CFG_RETRYRATE_POLICY_AUTOSELECT     4
#define WDI_CFG_RETRYRATE_POLICY_MAX            5

/* WDI_CFG_RETRYRATE_SECONDARY */
#define WDI_CFG_RETRYRATE_SECONDARY_STAMIN    0
#define WDI_CFG_RETRYRATE_SECONDARY_STAMAX    255
#define WDI_CFG_RETRYRATE_SECONDARY_STADEF    0

/* WDI_CFG_RETRYRATE_TERTIARY */
#define WDI_CFG_RETRYRATE_TERTIARY_STAMIN    0
#define WDI_CFG_RETRYRATE_TERTIARY_STAMAX    255
#define WDI_CFG_RETRYRATE_TERTIARY_STADEF    0

/* WDI_CFG_FORCE_POLICY_PROTECTION */
#define WDI_CFG_FORCE_POLICY_PROTECTION_STAMIN     0
#define WDI_CFG_FORCE_POLICY_PROTECTION_STAMAX     5
#define WDI_CFG_FORCE_POLICY_PROTECTION_STADEF     5
#define WDI_CFG_FORCE_POLICY_PROTECTION_DISABLE    0
#define WDI_CFG_FORCE_POLICY_PROTECTION_CTS        1
#define WDI_CFG_FORCE_POLICY_PROTECTION_RTS        2
#define WDI_CFG_FORCE_POLICY_PROTECTION_DUAL_CTS   3
#define WDI_CFG_FORCE_POLICY_PROTECTION_RTS_ALWAYS 4
#define WDI_CFG_FORCE_POLICY_PROTECTION_AUTO       5

/* WDI_CFG_FIXED_RATE_MULTICAST_24GHZ */
#define WDI_CFG_FIXED_RATE_MULTICAST_24GHZ_STAMIN    0
#define WDI_CFG_FIXED_RATE_MULTICAST_24GHZ_STAMAX    31
#define WDI_CFG_FIXED_RATE_MULTICAST_24GHZ_STADEF    1

/* WDI_CFG_FIXED_RATE_MULTICAST_5GHZ */
#define WDI_CFG_FIXED_RATE_MULTICAST_5GHZ_STAMIN    0
#define WDI_CFG_FIXED_RATE_MULTICAST_5GHZ_STAMAX    31
#define WDI_CFG_FIXED_RATE_MULTICAST_5GHZ_STADEF    5

/* WDI_CFG_DEFAULT_RATE_INDEX_24GHZ */
#define WDI_CFG_DEFAULT_RATE_INDEX_24GHZ_STAMIN    0
#define WDI_CFG_DEFAULT_RATE_INDEX_24GHZ_STAMAX    31
#define WDI_CFG_DEFAULT_RATE_INDEX_24GHZ_STADEF    1

/* WDI_CFG_DEFAULT_RATE_INDEX_5GHZ */
#define WDI_CFG_DEFAULT_RATE_INDEX_5GHZ_STAMIN    0
#define WDI_CFG_DEFAULT_RATE_INDEX_5GHZ_STAMAX    11
#define WDI_CFG_DEFAULT_RATE_INDEX_5GHZ_STADEF    5

/* WDI_CFG_MAX_BA_SESSIONS */
#define WDI_CFG_MAX_BA_SESSIONS_STAMIN    0
#define WDI_CFG_MAX_BA_SESSIONS_STAMAX    64
#define WDI_CFG_MAX_BA_SESSIONS_STADEF    16

/* WDI_CFG_PS_DATA_INACTIVITY_TIMEOUT */
#define WDI_CFG_PS_DATA_INACTIVITY_TIMEOUT_STAMIN    1
#define WDI_CFG_PS_DATA_INACTIVITY_TIMEOUT_STAMAX    255
#define WDI_CFG_PS_DATA_INACTIVITY_TIMEOUT_STADEF    20

/* WDI_CFG_PS_ENABLE_BCN_FILTER */
#define WDI_CFG_PS_ENABLE_BCN_FILTER_STAMIN    0
#define WDI_CFG_PS_ENABLE_BCN_FILTER_STAMAX    1
#define WDI_CFG_PS_ENABLE_BCN_FILTER_STADEF    1

/* WDI_CFG_PS_ENABLE_RSSI_MONITOR */
#define WDI_CFG_PS_ENABLE_RSSI_MONITOR_STAMIN    0
#define WDI_CFG_PS_ENABLE_RSSI_MONITOR_STAMAX    1
#define WDI_CFG_PS_ENABLE_RSSI_MONITOR_STADEF    1

/* WDI_CFG_NUM_BEACON_PER_RSSI_AVERAGE */
#define WDI_CFG_NUM_BEACON_PER_RSSI_AVERAGE_STAMIN    1
#define WDI_CFG_NUM_BEACON_PER_RSSI_AVERAGE_STAMAX    20
#define WDI_CFG_NUM_BEACON_PER_RSSI_AVERAGE_STADEF    20

/* WDI_CFG_STATS_PERIOD */
#define WDI_CFG_STATS_PERIOD_STAMIN    1
#define WDI_CFG_STATS_PERIOD_STAMAX    10
#define WDI_CFG_STATS_PERIOD_STADEF    10

/* WDI_CFG_CFP_MAX_DURATION */
#define WDI_CFG_CFP_PERIOD_STAMIN                  0
#define WDI_CFG_CFP_PERIOD_STAMAX                  255
#define WDI_CFG_CFP_PERIOD_STADEF                  1

#endif /*WLAN_QCT_WDI_CFG_H*/
