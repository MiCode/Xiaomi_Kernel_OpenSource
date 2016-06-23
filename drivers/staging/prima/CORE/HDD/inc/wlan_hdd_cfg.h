/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#if !defined( HDD_CONFIG_H__ )
#define HDD_CONFIG_H__

/**===========================================================================

  \file  hdd_Config.h

  \brief Android WLAN Adapter Configuration functions

               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.

  ==========================================================================*/

/* $HEADER$ */

/*--------------------------------------------------------------------------- 
  Include files
  -------------------------------------------------------------------------*/ 
#include <wlan_hdd_includes.h>
#include <wlan_hdd_wmm.h>
#include <vos_types.h>
#include <csrApi.h>

//Number of items that can be configured
#define MAX_CFG_INI_ITEMS   512

// Defines for all of the things we read from the configuration (registry).

#define CFG_RTS_THRESHOLD_NAME                 "RTSThreshold"
#define CFG_RTS_THRESHOLD_MIN                  WNI_CFG_RTS_THRESHOLD_STAMIN // min is 0, meaning always use RTS.
#define CFG_RTS_THRESHOLD_MAX                  WNI_CFG_RTS_THRESHOLD_STAMAX // max is the max frame size
#define CFG_RTS_THRESHOLD_DEFAULT              WNI_CFG_RTS_THRESHOLD_STADEF

#define CFG_FRAG_THRESHOLD_NAME                "gFragmentationThreshold"
#define CFG_FRAG_THRESHOLD_MIN                 WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN 
#define CFG_FRAG_THRESHOLD_MAX                 WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX
#define CFG_FRAG_THRESHOLD_DEFAULT             WNI_CFG_FRAGMENTATION_THRESHOLD_STADEF

#define CFG_CALIBRATION_NAME                   "gCalibration"
#define CFG_CALIBRATION_MIN                    ( 0 ) 
#define CFG_CALIBRATION_MAX                    ( 1 )
#define CFG_CALIBRATION_MAC_DEFAULT            ( 1 )
#define CFG_CALIBRATION_DEFAULT                CFG_CALIBRATION_MAC_DEFAULT

#define CFG_CALIBRATION_PERIOD_NAME            "gCalibrationPeriod"
#define CFG_CALIBRATION_PERIOD_MIN             ( 2 ) 
#define CFG_CALIBRATION_PERIOD_MAX             ( 10 )
#define CFG_CALIBRATION_PERIOD_MAC_DEFAULT     ( 5 )
#define CFG_CALIBRATION_PERIOD_DEFAULT         CFG_CALIBRATION_PERIOD_MAC_DEFAULT          

#define CFG_OPERATING_CHANNEL_NAME             "gOperatingChannel"
#define CFG_OPERATING_CHANNEL_MIN              ( 0 )
#define CFG_OPERATING_CHANNEL_MAX              ( 14 )
#define CFG_OPERATING_CHANNEL_DEFAULT          ( 1 )

#define CFG_SHORT_SLOT_TIME_ENABLED_NAME       "gShortSlotTimeEnabled"
#define CFG_SHORT_SLOT_TIME_ENABLED_MIN        WNI_CFG_SHORT_SLOT_TIME_STAMIN
#define CFG_SHORT_SLOT_TIME_ENABLED_MAX        WNI_CFG_SHORT_SLOT_TIME_STAMAX
#define CFG_SHORT_SLOT_TIME_ENABLED_DEFAULT    WNI_CFG_SHORT_SLOT_TIME_STADEF

#define CFG_11D_SUPPORT_ENABLED_NAME           "g11dSupportEnabled"
#define CFG_11D_SUPPORT_ENABLED_MIN            WNI_CFG_11D_ENABLED_STAMIN 
#define CFG_11D_SUPPORT_ENABLED_MAX            WNI_CFG_11D_ENABLED_STAMAX 
#define CFG_11D_SUPPORT_ENABLED_DEFAULT        WNI_CFG_11D_ENABLED_STADEF    // Default is ON 

#define CFG_11H_SUPPORT_ENABLED_NAME           "g11hSupportEnabled"
#define CFG_11H_SUPPORT_ENABLED_MIN            ( 0 )
#define CFG_11H_SUPPORT_ENABLED_MAX            ( 1 )
#define CFG_11H_SUPPORT_ENABLED_DEFAULT        ( 1 ) // Default is ON 

#define CFG_ENFORCE_11D_CHANNELS_NAME          "gEnforce11dChannel"
#define CFG_ENFORCE_11D_CHANNELS_MIN           ( 0 )
#define CFG_ENFORCE_11D_CHANNELS_MAX           ( 1 )
#define CFG_ENFORCE_11D_CHANNELS_DEFAULT       ( 0 )

//COUNTRY Code Priority 
#define CFG_COUNTRY_CODE_PRIORITY_NAME         "gCountryCodePriority"
#define CFG_COUNTRY_CODE_PRIORITY_MIN          ( 0 )
#define CFG_COUNTRY_CODE_PRIORITY_MAX          ( 1 )
#define CFG_COUNTRY_CODE_PRIORITY_DEFAULT      ( 1 )

#define CFG_ENFORCE_COUNTRY_CODE_MATCH_NAME    "gEnforceCountryCodeMatch"
#define CFG_ENFORCE_COUNTRY_CODE_MATCH_MIN     ( 0 )
#define CFG_ENFORCE_COUNTRY_CODE_MATCH_MAX     ( 1 )
#define CFG_ENFORCE_COUNTRY_CODE_MATCH_DEFAULT ( 0 )

#define CFG_ENFORCE_DEFAULT_DOMAIN_NAME        "gEnforceDefaultDomain"
#define CFG_ENFORCE_DEFAULT_DOMAIN_MIN         ( 0 )
#define CFG_ENFORCE_DEFAULT_DOMAIN_MAX         ( 1 ) 
#define CFG_ENFORCE_DEFAULT_DOMAIN_DEFAULT     ( 0 )

#define CFG_GENERIC_ID1_NAME                   "gCfg1Id"
#define CFG_GENERIC_ID1_MIN                    ( 0 )
#define CFG_GENERIC_ID1_MAX                    ( 0xffffffff )
#define CFG_GENERIC_ID1_DEFAULT                ( 0 )

#define CFG_GENERIC_ID2_NAME                   "gCfg2Id"
#define CFG_GENERIC_ID2_MIN                    ( 0 )
#define CFG_GENERIC_ID2_MAX                    ( 0xffffffff )
#define CFG_GENERIC_ID2_DEFAULT                ( 0 )

#define CFG_GENERIC_ID3_NAME                   "gCfg3Id"
#define CFG_GENERIC_ID3_MIN                    ( 0 )
#define CFG_GENERIC_ID3_MAX                    ( 0xffffffff )
#define CFG_GENERIC_ID3_DEFAULT                ( 0 )

#define CFG_GENERIC_ID4_NAME                   "gCfg4Id"
#define CFG_GENERIC_ID4_MIN                    ( 0 )
#define CFG_GENERIC_ID4_MAX                    ( 0xffffffff )
#define CFG_GENERIC_ID4_DEFAULT                ( 0 )

#define CFG_GENERIC_ID5_NAME                   "gCfg5Id"
#define CFG_GENERIC_ID5_MIN                    ( 0 )
#define CFG_GENERIC_ID5_MAX                    ( 0xffffffff )
#define CFG_GENERIC_ID5_DEFAULT                ( 0 )

#define CFG_GENERIC_VALUE1_NAME                "gCfg1Value"
#define CFG_GENERIC_VALUE1_MIN                 ( 0 )
#define CFG_GENERIC_VALUE1_MAX                 ( 0xffffffff )
#define CFG_GENERIC_VALUE1_DEFAULT             ( 0 )

#define CFG_GENERIC_VALUE2_NAME                "gCfg2Value"
#define CFG_GENERIC_VALUE2_MIN                 ( 0 )
#define CFG_GENERIC_VALUE2_MAX                 ( 0xffffffff )
#define CFG_GENERIC_VALUE2_DEFAULT             ( 0 )

#define CFG_GENERIC_VALUE3_NAME                "gCfg3Value"
#define CFG_GENERIC_VALUE3_MIN                 ( 0 )
#define CFG_GENERIC_VALUE3_MAX                 ( 0xffffffff )
#define CFG_GENERIC_VALUE3_DEFAULT             ( 0 )

#define CFG_GENERIC_VALUE4_NAME                "gCfg4Value"
#define CFG_GENERIC_VALUE4_MIN                 ( 0 )
#define CFG_GENERIC_VALUE4_MAX                 ( 0xffffffff )
#define CFG_GENERIC_VALUE4_DEFAULT             ( 0 )

#define CFG_GENERIC_VALUE5_NAME                "gCfg5Value"
#define CFG_GENERIC_VALUE5_MIN                 ( 0 )
#define CFG_GENERIC_VALUE5_MAX                 ( 0xffffffff )
#define CFG_GENERIC_VALUE5_DEFAULT             ( 0 )

#define CFG_HEARTBEAT_THRESH_24_NAME           "gHeartbeat24"
#define CFG_HEARTBEAT_THRESH_24_MIN            WNI_CFG_HEART_BEAT_THRESHOLD_STAMIN
#define CFG_HEARTBEAT_THRESH_24_MAX            WNI_CFG_HEART_BEAT_THRESHOLD_STAMAX
#define CFG_HEARTBEAT_THRESH_24_DEFAULT        WNI_CFG_HEART_BEAT_THRESHOLD_STADEF

#define CFG_POWER_USAGE_NAME                   "gPowerUsage"
#define CFG_POWER_USAGE_MIN                    "Min" //Minimum Power Save
#define CFG_POWER_USAGE_MAX                    "Max" //Maximum Power Save
#define CFG_POWER_USAGE_DEFAULT                "Mod" //Moderate Power Save

//Enable suspend on Android
#define CFG_ENABLE_SUSPEND_NAME                "gEnableSuspend"
#define CFG_ENABLE_SUSPEND_MIN                 ( 0 ) //No support for suspend
#define CFG_ENABLE_SUSPEND_MAX                 ( 3 ) //Map to Deep Sleep
#define CFG_ENABLE_SUSPEND_DEFAULT             ( 1 ) //Map to Standby

//Driver start/stop command mappings
#define CFG_ENABLE_ENABLE_DRIVER_STOP_NAME     "gEnableDriverStop"
#define CFG_ENABLE_ENABLE_DRIVER_STOP_MIN      ( 0 ) //No support for stop
#define CFG_ENABLE_ENABLE_DRIVER_STOP_MAX      ( 2 ) //Map to Deep Sleep
#define CFG_ENABLE_ENABLE_DRIVER_STOP_DEFAULT  ( 0 )

#define CFG_WOWL_PATTERN_NAME                  "gWowlPattern"
#define CFG_WOWL_PATTERN_DEFAULT               ""

//IMPS = IdleModePowerSave
#define CFG_ENABLE_IMPS_NAME                   "gEnableImps"
#define CFG_ENABLE_IMPS_MIN                    ( 0 )
#define CFG_ENABLE_IMPS_MAX                    ( 1 )
#define CFG_ENABLE_IMPS_DEFAULT                ( 1 )

//SSR = SubSytemRestart
#define CFG_SSR_PANIC_ON_FAILURE_NAME          "gSsrPanicOnFailure"
#define CFG_SSR_PANIC_ON_FAILURE_MIN           ( 0 )
#define CFG_SSR_PANIC_ON_FAILURE_MAX           ( 1 )
#define CFG_SSR_PANIC_ON_FAILURE_DEFAULT       ( 0 )

#define CFG_IMPS_MINIMUM_SLEEP_TIME_NAME       "gImpsMinSleepTime" 
#define CFG_IMPS_MINIMUM_SLEEP_TIME_MIN        ( 0 )
#define CFG_IMPS_MINIMUM_SLEEP_TIME_MAX        ( 65535 )
#define CFG_IMPS_MINIMUM_SLEEP_TIME_DEFAULT    ( 5 )
    
#define CFG_IMPS_MODERATE_SLEEP_TIME_NAME      "gImpsModSleepTime"
#define CFG_IMPS_MODERATE_SLEEP_TIME_MIN       ( 0 )
#define CFG_IMPS_MODERATE_SLEEP_TIME_MAX       ( 65535 )
#define CFG_IMPS_MODERATE_SLEEP_TIME_DEFAULT   ( 10)

#define CFG_IMPS_MAXIMUM_SLEEP_TIME_NAME       "gImpsMaxSleepTime"
#define CFG_IMPS_MAXIMUM_SLEEP_TIME_MIN        ( 0 )
#define CFG_IMPS_MAXIMUM_SLEEP_TIME_MAX        ( 65535 )
#define CFG_IMPS_MAXIMUM_SLEEP_TIME_DEFAULT    ( 15 )

/*If there is scan on STA interface back to back with
 *time diff nDeferScanTimeInterval, driver will not
 *issue a new scan. Driver will return cached result to kernel.
 *the interval is in msec
 */
#define CFG_DEFER_SCAN_TIME_INTERVAL            "gDeferScanTimeInterval"
#define CFG_DEFER_SCAN_TIME_INTERVAL_MIN        ( 0 )
#define CFG_DEFER_SCAN_TIME_INTERVAL_MAX        ( 65535 )
#define CFG_DEFER_SCAN_TIME_INTERVAL_DEFAULT    ( 2000  )

//BMPS = BeaconModePowerSave
#define CFG_ENABLE_BMPS_NAME                   "gEnableBmps"
#define CFG_ENABLE_BMPS_MIN                    ( 0 )
#define CFG_ENABLE_BMPS_MAX                    ( 1 )
#define CFG_ENABLE_BMPS_DEFAULT                ( 1 )

#define CFG_BMPS_MINIMUM_LI_NAME               "gBmpsMinListenInterval"
#define CFG_BMPS_MINIMUM_LI_MIN                ( 1 )
#define CFG_BMPS_MINIMUM_LI_MAX                ( 65535 )
#define CFG_BMPS_MINIMUM_LI_DEFAULT            ( 1 )
    
#define CFG_BMPS_MODERATE_LI_NAME              "gBmpsModListenInterval"
#define CFG_BMPS_MODERATE_LI_MIN               ( 1 )
#define CFG_BMPS_MODERATE_LI_MAX               ( 65535 )
#define CFG_BMPS_MODERATE_LI_DEFAULT           ( 1 )

#define CFG_BMPS_MAXIMUM_LI_NAME               "gBmpsMaxListenInterval"
#define CFG_BMPS_MAXIMUM_LI_MIN                ( 1 )
#define CFG_BMPS_MAXIMUM_LI_MAX                ( 65535 )
#define CFG_BMPS_MAXIMUM_LI_DEFAULT            ( 1 )

// gEnableAutoBmpsTimer has been previously published as an externally
// configurable parameter. See analysis of CR 178211 for detailed info
// on why we want to *always* set this to 1 i.e. we no longer want
// this parameter to be configurable. the clean solution would be for 
// users to not define this item in winreg so that the default value 
// (which needs to be changed to 1) gets picked up but we cannot rely on that 
// since this item has been published already hence the proposed
// solution to change the name of the item along with the change in the
// default value. also we could decide to not read this item from registry
// but leaving open the option of being able to configure this item for
// ASW's internal use
#define CFG_ENABLE_AUTO_BMPS_TIMER_NAME        "gEnableAutoBmpsTimer_INTERNAL"
#define CFG_ENABLE_AUTO_BMPS_TIMER_MIN         ( 0 )
#define CFG_ENABLE_AUTO_BMPS_TIMER_MAX         ( 1 )
#define CFG_ENABLE_AUTO_BMPS_TIMER_DEFAULT     ( 1 )

#define CFG_ENABLE_DYNAMIC_RA_START_RATE_NAME    "gEnableDynamicRAstartRate"
#define CFG_ENABLE_DYNAMIC_RA_START_RATE_MIN     ( 0 )
#define CFG_ENABLE_DYNAMIC_RA_START_RATE_MAX     ( 1 )
#define CFG_ENABLE_DYNAMIC_RA_START_RATE_DEFAULT ( 0 )

#define CFG_AUTO_BMPS_TIMER_VALUE_NAME         "gAutoBmpsTimerValue" 
#define CFG_AUTO_BMPS_TIMER_VALUE_MIN          ( 1000 )
#define CFG_AUTO_BMPS_TIMER_VALUE_MAX          ( 4294967295UL )
#define CFG_AUTO_BMPS_TIMER_VALUE_DEFAULT      ( 1000 )        
    
#define CFG_MAX_RX_AMPDU_FACTOR_NAME           "gMaxRxAmpduFactor"   
#define CFG_MAX_RX_AMPDU_FACTOR_MIN            WNI_CFG_MAX_RX_AMPDU_FACTOR_STAMIN 
#define CFG_MAX_RX_AMPDU_FACTOR_MAX            WNI_CFG_MAX_RX_AMPDU_FACTOR_STAMAX 
#define CFG_MAX_RX_AMPDU_FACTOR_DEFAULT        WNI_CFG_MAX_RX_AMPDU_FACTOR_STADEF 

//Configuration added to enable/disable CTS2SELF in
//Adaptive RX drain feature
#define CFG_ENABLE_ADAPT_RX_DRAIN_NAME     "gEnableAdaptRxDrain"
#define CFG_ENABLE_ADAPT_RX_DRAIN_MIN       WNI_CFG_ENABLE_ADAPT_RX_DRAIN_STAMIN
#define CFG_ENABLE_ADAPT_RX_DRAIN_MAX       WNI_CFG_ENABLE_ADAPT_RX_DRAIN_STAMAX
#define CFG_ENABLE_ADAPT_RX_DRAIN_DEFAULT   WNI_CFG_ENABLE_ADAPT_RX_DRAIN_STADEF

typedef enum
{
    eHDD_DOT11_MODE_AUTO = 0, //covers all things we support
    eHDD_DOT11_MODE_abg,      //11a/b/g only, no HT, no proprietary
    eHDD_DOT11_MODE_11b,
    eHDD_DOT11_MODE_11g,
    eHDD_DOT11_MODE_11n,
    eHDD_DOT11_MODE_11g_ONLY,
    eHDD_DOT11_MODE_11n_ONLY,
    eHDD_DOT11_MODE_11b_ONLY,
#ifdef WLAN_FEATURE_11AC
    eHDD_DOT11_MODE_11ac_ONLY,
    eHDD_DOT11_MODE_11ac,
#endif
}eHddDot11Mode;

#define CFG_DOT11_MODE_NAME                    "gDot11Mode"
#define CFG_DOT11_MODE_MIN                     eHDD_DOT11_MODE_AUTO
#ifdef WLAN_FEATURE_11AC
#define CFG_DOT11_MODE_MAX                     eHDD_DOT11_MODE_11ac
#define CFG_DOT11_MODE_DEFAULT                 eHDD_DOT11_MODE_11ac
#else
#define CFG_DOT11_MODE_MAX                     eHDD_DOT11_MODE_11b_ONLY
#define CFG_DOT11_MODE_DEFAULT                 eHDD_DOT11_MODE_11n
#endif

#define CFG_SAP_DOT11_MODE_NAME                "gSapDot11Mode"
#define CFG_SAP_DOT11_MODE_MIN                 eHDD_DOT11_MODE_AUTO
#ifdef WLAN_FEATURE_11AC
#define CFG_SAP_DOT11_MODE_MAX                 eHDD_DOT11_MODE_11ac
#define CFG_SAP_DOT11_MODE_DEFAULT             eHDD_DOT11_MODE_11ac
#else
#define CFG_SAP_DOT11_MODE_MAX                 eHDD_DOT11_MODE_11b_ONLY
#define CFG_SAP_DOT11_MODE_DEFAULT             eHDD_DOT11_MODE_11n
#endif

#define CFG_CHANNEL_BONDING_MODE_24GHZ_NAME    "gChannelBondingMode24GHz"
#define CFG_CHANNEL_BONDING_MODE_MIN           WNI_CFG_CHANNEL_BONDING_MODE_STAMIN 
#define CFG_CHANNEL_BONDING_MODE_MAX           WNI_CFG_CHANNEL_BONDING_MODE_STAMAX 
#define CFG_CHANNEL_BONDING_MODE_DEFAULT       WNI_CFG_CHANNEL_BONDING_MODE_STADEF 

#define CFG_CHANNEL_BONDING_MODE_5GHZ_NAME     "gChannelBondingMode5GHz"
#define CFG_CHANNEL_BONDING_MODE_MIN           WNI_CFG_CHANNEL_BONDING_MODE_STAMIN 
#define CFG_CHANNEL_BONDING_MODE_MAX           WNI_CFG_CHANNEL_BONDING_MODE_STAMAX 
#define CFG_CHANNEL_BONDING_MODE_DEFAULT       WNI_CFG_CHANNEL_BONDING_MODE_STADEF 

#define CFG_FIXED_RATE_NAME                    "gFixedRate"
#define CFG_FIXED_RATE_MIN                     WNI_CFG_FIXED_RATE_STAMIN
#define CFG_FIXED_RATE_MAX                     WNI_CFG_FIXED_RATE_STAMAX
#define CFG_FIXED_RATE_DEFAULT                 WNI_CFG_FIXED_RATE_STADEF 
 
#define CFG_SHORT_GI_20MHZ_NAME                "gShortGI20Mhz"
#define CFG_SHORT_GI_20MHZ_MIN                 WNI_CFG_SHORT_GI_20MHZ_STAMIN
#define CFG_SHORT_GI_20MHZ_MAX                 WNI_CFG_SHORT_GI_20MHZ_STAMAX 
#define CFG_SHORT_GI_20MHZ_DEFAULT             WNI_CFG_SHORT_GI_20MHZ_STADEF 

#define CFG_BLOCK_ACK_AUTO_SETUP_NAME          "gBlockAckAutoSetup"
#define CFG_BLOCK_ACK_AUTO_SETUP_MIN           ( 0 )
#define CFG_BLOCK_ACK_AUTO_SETUP_MAX           ( 1 )
#define CFG_BLOCK_ACK_AUTO_SETUP_DEFAULT       ( 1 )

#define CFG_SCAN_RESULT_AGE_COUNT_NAME         "gScanResultAgeCount"
#define CFG_SCAN_RESULT_AGE_COUNT_MIN          ( 1 )
#define CFG_SCAN_RESULT_AGE_COUNT_MAX          ( 100 )
#define CFG_SCAN_RESULT_AGE_COUNT_DEFAULT      ( 3 )

//All in seconds
//Not Connect, No Power Save
#define CFG_SCAN_RESULT_AGE_TIME_NCNPS_NAME    "gScanResultAgeNCNPS"
#define CFG_SCAN_RESULT_AGE_TIME_NCNPS_MIN     ( 10 )
#define CFG_SCAN_RESULT_AGE_TIME_NCNPS_MAX     ( 10000 )
#define CFG_SCAN_RESULT_AGE_TIME_NCNPS_DEFAULT ( 50 )
//Not Connect, Power Save
#define CFG_SCAN_RESULT_AGE_TIME_NCPS_NAME     "gScanResultAgeNCPS"
#define CFG_SCAN_RESULT_AGE_TIME_NCPS_MIN      ( 10 )
#define CFG_SCAN_RESULT_AGE_TIME_NCPS_MAX      ( 10000 )
#define CFG_SCAN_RESULT_AGE_TIME_NCPS_DEFAULT  ( 300 )
//Connect, No Power Save
#define CFG_SCAN_RESULT_AGE_TIME_CNPS_NAME     "gScanResultAgeCNPS"
#define CFG_SCAN_RESULT_AGE_TIME_CNPS_MIN      ( 10 )
#define CFG_SCAN_RESULT_AGE_TIME_CNPS_MAX      ( 10000 )
#define CFG_SCAN_RESULT_AGE_TIME_CNPS_DEFAULT  ( 150 )
//Connect, Power Save
#define CFG_SCAN_RESULT_AGE_TIME_CPS_NAME      "gScanResultAgeCPS"
#define CFG_SCAN_RESULT_AGE_TIME_CPS_MIN       ( 10 )
#define CFG_SCAN_RESULT_AGE_TIME_CPS_MAX       ( 10000 )
#define CFG_SCAN_RESULT_AGE_TIME_CPS_DEFAULT   ( 600 )

#define CFG_RSSI_CATEGORY_GAP_NAME             "gRssiCatGap"
#define CFG_RSSI_CATEGORY_GAP_MIN              ( 5 )  
#define CFG_RSSI_CATEGORY_GAP_MAX              ( 100 )  
#define CFG_RSSI_CATEGORY_GAP_DEFAULT          ( 5 )

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
#define CFG_ROAM_PREFER_5GHZ                   "gRoamPrefer5GHz"
#define CFG_ROAM_PREFER_5GHZ_MIN              ( 0 )  
#define CFG_ROAM_PREFER_5GHZ_MAX              ( 1 )  
#define CFG_ROAM_PREFER_5GHZ_DEFAULT          ( 1 )

/*
    To enable, set gRoamIntraBand=1 (Roaming within band)
    To disable, set gRoamIntraBand=0 (Roaming across band)
*/
#define CFG_ROAM_INTRA_BAND                   "gRoamIntraBand"
#define CFG_ROAM_INTRA_BAND_MIN               ( 0 )
#define CFG_ROAM_INTRA_BAND_MAX               ( 1 )
#define CFG_ROAM_INTRA_BAND_DEFAULT           ( 0 )
#endif

#define CFG_SHORT_PREAMBLE_NAME                "gShortPreamble"
#define CFG_SHORT_PREAMBLE_MIN                 WNI_CFG_SHORT_PREAMBLE_STAMIN
#define CFG_SHORT_PREAMBLE_MAX                 WNI_CFG_SHORT_PREAMBLE_STAMAX
#define CFG_SHORT_PREAMBLE_DEFAULT             WNI_CFG_SHORT_PREAMBLE_STADEF

#define CFG_IBSS_AUTO_BSSID_NAME               "gAutoIbssBssid"
#define CFG_IBSS_AUTO_BSSID_MIN                WNI_CFG_IBSS_AUTO_BSSID_STAMIN
#define CFG_IBSS_AUTO_BSSID_MAX                WNI_CFG_IBSS_AUTO_BSSID_STAMAX
#define CFG_IBSS_AUTO_BSSID_DEFAULT            WNI_CFG_IBSS_AUTO_BSSID_STADEF

#define CFG_IBSS_BSSID_NAME                    "gIbssBssid"
#define CFG_IBSS_BSSID_MIN                     "000000000000"
#define CFG_IBSS_BSSID_MAX                     "ffffffffffff"
#define CFG_IBSS_BSSID_DEFAULT                 "000AF5040506"

#define CFG_INTF0_MAC_ADDR_NAME                  "Intf0MacAddress"
#define CFG_INTF0_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF0_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF0_MAC_ADDR_DEFAULT               "000AF5898980"

#define CFG_INTF1_MAC_ADDR_NAME                  "Intf1MacAddress"
#define CFG_INTF1_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF1_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF1_MAC_ADDR_DEFAULT               "000AF5898981"

#define CFG_INTF2_MAC_ADDR_NAME                  "Intf2MacAddress"
#define CFG_INTF2_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF2_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF2_MAC_ADDR_DEFAULT               "000AF5898982"

#define CFG_INTF3_MAC_ADDR_NAME                  "Intf3MacAddress"
#define CFG_INTF3_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF3_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF3_MAC_ADDR_DEFAULT               "000AF5898983"

#define CFG_AP_QOS_UAPSD_MODE_NAME             "gEnableApUapsd" // ACs to setup U-APSD for at assoc
#define CFG_AP_QOS_UAPSD_MODE_MIN              ( 0 )
#define CFG_AP_QOS_UAPSD_MODE_MAX              ( 1 ) 
#define CFG_AP_QOS_UAPSD_MODE_DEFAULT          ( 1 )   

#define CFG_AP_COUNTRY_CODE                    "gAPCntryCode"
#define CFG_AP_COUNTRY_CODE_MIN                "USI"
#define CFG_AP_COUNTRY_CODE_MAX                "USI"
#define CFG_AP_COUNTRY_CODE_DEFAULT            "FFF"

#define CFG_AP_ENABLE_RANDOM_BSSID_NAME            "gEnableApRandomBssid"
#define CFG_AP_ENABLE_RANDOM_BSSID_MIN             ( 0 )
#define CFG_AP_ENABLE_RANDOM_BSSID_MAX             ( 1 )
#define CFG_AP_ENABLE_RANDOM_BSSID_DEFAULT         ( 0 )

#define CFG_AP_ENABLE_PROTECTION_MODE_NAME            "gEnableApProt"
#define CFG_AP_ENABLE_PROTECTION_MODE_MIN             ( 0 )
#define CFG_AP_ENABLE_PROTECTION_MODE_MAX             ( 1 )
#define CFG_AP_ENABLE_PROTECTION_MODE_DEFAULT         ( 1 )

// Bit map for CFG_AP_PROTECTION_MODE_DEFAULT
// LOWER byte for associated stations
// UPPER byte for overlapping stations
// each byte will have the following info
// bit15 bit14 bit13     bit12  bit11 bit10    bit9     bit8
// OBSS  RIFS  LSIG_TXOP NON_GF HT20  FROM_11G FROM_11B FROM_11A
// bit7  bit6  bit5      bit4   bit3  bit2     bit1     bit0
// OBSS  RIFS  LSIG_TXOP NON_GF HT_20 FROM_11G FROM_11B FROM_11A
#define CFG_AP_PROTECTION_MODE_NAME            "gApProtection"
#define CFG_AP_PROTECTION_MODE_MIN             ( 0x0 )
#define CFG_AP_PROTECTION_MODE_MAX             ( 0xFFFF )
#define CFG_AP_PROTECTION_MODE_DEFAULT         ( 0xBFFF )

#define CFG_AP_OBSS_PROTECTION_MODE_NAME       "gEnableApOBSSProt" 
#define CFG_AP_OBSS_PROTECTION_MODE_MIN        ( 0 )
#define CFG_AP_OBSS_PROTECTION_MODE_MAX        ( 1 ) 
#define CFG_AP_OBSS_PROTECTION_MODE_DEFAULT    ( 0 )   

#define CFG_AP_STA_SECURITY_SEPERATION_NAME    "gDisableIntraBssFwd"
#define CFG_AP_STA_SECURITY_SEPERATION_MIN     ( 0 )
#define CFG_AP_STA_SECURITY_SEPERATION_MAX     ( 1 ) 
#define CFG_AP_STA_SECURITY_SEPERATION_DEFAULT ( 0 )   

#define CFG_AP_LISTEN_MODE_NAME               "gEnablePhyAgcListenMode" 
#define CFG_AP_LISTEN_MODE_MIN                (0)
#define CFG_AP_LISTEN_MODE_MAX                (128) 
#define CFG_AP_LISTEN_MODE_DEFAULT            (128)   

#define CFG_AP_AUTO_SHUT_OFF                "gAPAutoShutOff"
#define CFG_AP_AUTO_SHUT_OFF_MIN            ( 0 )
#define CFG_AP_AUTO_SHUT_OFF_MAX            ( 4294967295UL )
#define CFG_AP_AUTO_SHUT_OFF_DEFAULT        ( 0 )

#define CFG_FRAMES_PROCESSING_TH_MODE_NAME     "gMinFramesProcThres"
#define CFG_FRAMES_PROCESSING_TH_MIN           ( 0 )
#define CFG_FRAMES_PROCESSING_TH_MAX           ( 39 )
#define CFG_FRAMES_PROCESSING_TH_DEFAULT       ( 0 )

#define CFG_SAP_CHANNEL_SELECT_START_CHANNEL    "gAPChannelSelectStartChannel"
#define CFG_SAP_CHANNEL_SELECT_START_CHANNEL_MIN                (0)
#define CFG_SAP_CHANNEL_SELECT_START_CHANNEL_MAX                (0xFF)
#define CFG_SAP_CHANNEL_SELECT_START_CHANNEL_DEFAULT            (0)

#define CFG_SAP_CHANNEL_SELECT_END_CHANNEL "gAPChannelSelectEndChannel"
#define CFG_SAP_CHANNEL_SELECT_END_CHANNEL_MIN                  (0)
#define CFG_SAP_CHANNEL_SELECT_END_CHANNEL_MAX                  (0xFF)
#define CFG_SAP_CHANNEL_SELECT_END_CHANNEL_DEFAULT              (11)

#define CFG_SAP_CHANNEL_SELECT_OPERATING_BAND "gAPChannelSelectOperatingBand"
#define CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_MIN       (0)
#define CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_MAX               (0x5)
#define CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_DEFAULT           (0)

#define CFG_DISABLE_PACKET_FILTER "gDisablePacketFilter"
#define CFG_DISABLE_PACKET_FILTER_MIN       (0)
#define CFG_DISABLE_PACKET_FILTER_MAX               (0x1)
#define CFG_DISABLE_PACKET_FILTER_DEFAULT           (0)

#define CFG_ENABLE_LTE_COEX              "gEnableLTECoex"
#define CFG_ENABLE_LTE_COEX_MIN               ( 0 )
#define CFG_ENABLE_LTE_COEX_MAX               ( 1 )
#define CFG_ENABLE_LTE_COEX_DEFAULT           ( 0 )

#define CFG_AP_KEEP_ALIVE_PERIOD_NAME          "gApKeepAlivePeriod"
#define CFG_AP_KEEP_ALIVE_PERIOD_MIN           ( 3 )
#define CFG_AP_KEEP_ALIVE_PERIOD_MAX           ( 20 )
#define CFG_AP_KEEP_ALIVE_PERIOD_DEFAULT       ( 5 )

#define CFG_GO_KEEP_ALIVE_PERIOD_NAME          "gGoKeepAlivePeriod"
#define CFG_GO_KEEP_ALIVE_PERIOD_MIN           ( 3 )
#define CFG_GO_KEEP_ALIVE_PERIOD_MAX           ( 20 )
#define CFG_GO_KEEP_ALIVE_PERIOD_DEFAULT       ( 5 )

#define CFG_AP_LINK_MONITOR_PERIOD_NAME          "gApLinkMonitorPeriod"
#define CFG_AP_LINK_MONITOR_PERIOD_MIN           ( 3 )
#define CFG_AP_LINK_MONITOR_PERIOD_MAX           ( 50 )
#define CFG_AP_LINK_MONITOR_PERIOD_DEFAULT       ( 10 )

/* gGoLinkMonitorPeriod is period where link is idle and where
 * we send NULL frame
 */
#define CFG_GO_LINK_MONITOR_PERIOD_NAME          "gGoLinkMonitorPeriod"
#define CFG_GO_LINK_MONITOR_PERIOD_MIN           ( 3 )
#define CFG_GO_LINK_MONITOR_PERIOD_MAX           ( 50 )
#define CFG_GO_LINK_MONITOR_PERIOD_DEFAULT       ( 10 )


#define CFG_BEACON_INTERVAL_NAME               "gBeaconInterval"
#define CFG_BEACON_INTERVAL_MIN                WNI_CFG_BEACON_INTERVAL_STAMIN
#define CFG_BEACON_INTERVAL_MAX                WNI_CFG_BEACON_INTERVAL_STAMAX
#define CFG_BEACON_INTERVAL_DEFAULT            WNI_CFG_BEACON_INTERVAL_STADEF

//Additional Handoff related Parameters  
#define CFG_ENABLE_IDLE_SCAN_NAME             "gEnableIdleScan"      
#define CFG_ENABLE_IDLE_SCAN_MIN              ( 0 )
#define CFG_ENABLE_IDLE_SCAN_MAX              ( 1 )
#define CFG_ENABLE_IDLE_SCAN_DEFAULT          ( 1 ) 

#define CFG_ROAMING_TIME_NAME                 "gRoamingTime"
#define CFG_ROAMING_TIME_MIN                  ( 0 )
#define CFG_ROAMING_TIME_MAX                  ( 4294967UL )
#define CFG_ROAMING_TIME_DEFAULT              ( 10 )

#define CFG_VCC_RSSI_TRIGGER_NAME             "gVccRssiTrigger"
#define CFG_VCC_RSSI_TRIGGER_MIN              ( 0 )
#define CFG_VCC_RSSI_TRIGGER_MAX              ( 80 ) 
#define CFG_VCC_RSSI_TRIGGER_DEFAULT          ( 80 )

#define CFG_VCC_UL_MAC_LOSS_THRESH_NAME       "gVccUlMacLossThresh"
#define CFG_VCC_UL_MAC_LOSS_THRESH_MIN        ( 0 )  
#define CFG_VCC_UL_MAC_LOSS_THRESH_MAX        ( 9 )
#define CFG_VCC_UL_MAC_LOSS_THRESH_DEFAULT    ( 9 )
   
#define CFG_PASSIVE_MAX_CHANNEL_TIME_NAME      "gPassiveMaxChannelTime"
#define CFG_PASSIVE_MAX_CHANNEL_TIME_MIN       ( 0 )
#define CFG_PASSIVE_MAX_CHANNEL_TIME_MAX       ( 10000 )
#define CFG_PASSIVE_MAX_CHANNEL_TIME_DEFAULT   ( 110 )

#define CFG_PASSIVE_MIN_CHANNEL_TIME_NAME      "gPassiveMinChannelTime"
#define CFG_PASSIVE_MIN_CHANNEL_TIME_MIN       ( 0 )
#define CFG_PASSIVE_MIN_CHANNEL_TIME_MAX       ( 10000 )
#define CFG_PASSIVE_MIN_CHANNEL_TIME_DEFAULT   ( 60 )

#define CFG_ACTIVE_MAX_CHANNEL_TIME_NAME       "gActiveMaxChannelTime"
#define CFG_ACTIVE_MAX_CHANNEL_TIME_MIN        ( 0 )
#define CFG_ACTIVE_MAX_CHANNEL_TIME_MAX        ( 10000 )
#define CFG_ACTIVE_MAX_CHANNEL_TIME_DEFAULT    ( 40 )

#define CFG_ACTIVE_MIN_CHANNEL_TIME_NAME       "gActiveMinChannelTime"
#define CFG_ACTIVE_MIN_CHANNEL_TIME_MIN        ( 0 )
#define CFG_ACTIVE_MIN_CHANNEL_TIME_MAX        ( 10000 )
#define CFG_ACTIVE_MIN_CHANNEL_TIME_DEFAULT    ( 20 )

#define CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_NAME       "gActiveMaxChannelTimeBtc"
#define CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_MIN        ( 0 )
#define CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_MAX        ( 10000 )
#define CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_DEFAULT    ( 120 )

#define CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_NAME       "gActiveMinChannelTimeBtc"
#define CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_MIN        ( 0 )
#define CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_MAX        ( 10000 )
#define CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_DEFAULT    ( 60 )

#define CFG_RETRY_LIMIT_ZERO_NAME       "gRetryLimitZero"
#define CFG_RETRY_LIMIT_ZERO_MIN        ( 0 )
#define CFG_RETRY_LIMIT_ZERO_MAX        ( 15 )
#define CFG_RETRY_LIMIT_ZERO_DEFAULT    ( 5 )

#define CFG_RETRY_LIMIT_ONE_NAME       "gRetryLimitOne"
#define CFG_RETRY_LIMIT_ONE_MIN        ( 0 )
#define CFG_RETRY_LIMIT_ONE_MAX        ( 15 )
#define CFG_RETRY_LIMIT_ONE_DEFAULT    ( 10 )

#define CFG_RETRY_LIMIT_TWO_NAME       "gRetryLimitTwo"
#define CFG_RETRY_LIMIT_TWO_MIN        ( 0 )
#define CFG_RETRY_LIMIT_TWO_MAX        ( 15 )
#define CFG_RETRY_LIMIT_TWO_DEFAULT    ( 15 )

#define CFG_DISABLE_AGG_WITH_BTC_NAME       "gDisableAggWithBTC"
#define CFG_DISABLE_AGG_WITH_BTC_MIN        ( 0 )
#define CFG_DISABLE_AGG_WITH_BTC_MAX        ( 1 )
#define CFG_DISABLE_AGG_WITH_BTC_DEFAULT    ( 1 )

#ifdef WLAN_AP_STA_CONCURRENCY

#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_NAME      "gPassiveMaxChannelTimeConc"
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MIN       ( 0 )
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MAX       ( 10000 )
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_DEFAULT   ( 110 )

#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_NAME      "gPassiveMinChannelTimeConc"
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MIN       ( 0 )
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MAX       ( 10000 )
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_DEFAULT   ( 60 )

#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_NAME       "gActiveMaxChannelTimeConc"
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MIN        ( 0 )
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MAX        ( 10000 )
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_DEFAULT    ( 40 )

#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_NAME       "gActiveMinChannelTimeConc"
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MIN        ( 0 )
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MAX        ( 10000 )
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_DEFAULT    ( 20 )

#define CFG_REST_TIME_CONC_NAME                     "gRestTimeConc"
#define CFG_REST_TIME_CONC_MIN                      ( 0 )
#define CFG_REST_TIME_CONC_MAX                      ( 10000 )
#define CFG_REST_TIME_CONC_DEFAULT                  ( 100 )

#define CFG_NUM_STA_CHAN_COMBINED_CONC_NAME             "gNumStaChanCombinedConc"
#define CFG_NUM_STA_CHAN_COMBINED_CONC_MIN              ( 1 )
#define CFG_NUM_STA_CHAN_COMBINED_CONC_MAX              ( 255 )
#define CFG_NUM_STA_CHAN_COMBINED_CONC_DEFAULT          ( 3 )

#define CFG_NUM_P2P_CHAN_COMBINED_CONC_NAME             "gNumP2PChanCombinedConc"
#define CFG_NUM_P2P_CHAN_COMBINED_CONC_MIN              ( 1 )
#define CFG_NUM_P2P_CHAN_COMBINED_CONC_MAX              ( 255 )
#define CFG_NUM_P2P_CHAN_COMBINED_CONC_DEFAULT          ( 1 )

#endif

#define CFG_MAX_PS_POLL_NAME                   "gMaxPsPoll"
#define CFG_MAX_PS_POLL_MIN                    WNI_CFG_MAX_PS_POLL_STAMIN
#define CFG_MAX_PS_POLL_MAX                    WNI_CFG_MAX_PS_POLL_STAMAX
#define CFG_MAX_PS_POLL_DEFAULT                WNI_CFG_MAX_PS_POLL_STADEF

#define CFG_MAX_TX_POWER_NAME                   "gTxPowerCap"
#define CFG_MAX_TX_POWER_MIN                    WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMIN
#define CFG_MAX_TX_POWER_MAX                    WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMAX
//Not to use CFG default because if no registry setting, this is ignored by SME.
#define CFG_MAX_TX_POWER_DEFAULT                WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMAX


#define CFG_LOW_GAIN_OVERRIDE_NAME             "gLowGainOverride"
#define CFG_LOW_GAIN_OVERRIDE_MIN              WNI_CFG_LOW_GAIN_OVERRIDE_STAMIN
#define CFG_LOW_GAIN_OVERRIDE_MAX              WNI_CFG_LOW_GAIN_OVERRIDE_STAMAX
#define CFG_LOW_GAIN_OVERRIDE_DEFAULT          WNI_CFG_LOW_GAIN_OVERRIDE_STADEF

#define CFG_RSSI_FILTER_PERIOD_NAME            "gRssiFilterPeriod"
#define CFG_RSSI_FILTER_PERIOD_MIN             WNI_CFG_RSSI_FILTER_PERIOD_STAMIN
#define CFG_RSSI_FILTER_PERIOD_MAX             WNI_CFG_RSSI_FILTER_PERIOD_STAMAX
// Increased this value for Non-ESE AP. This is cause FW RSSI Monitoring
// the consumer of this value is ON by default. So to impact power numbers
// we are setting this to a high value.
#define CFG_RSSI_FILTER_PERIOD_DEFAULT         WNI_CFG_RSSI_FILTER_PERIOD_STADEF

#define CFG_IGNORE_DTIM_NAME                   "gIgnoreDtim"
#define CFG_IGNORE_DTIM_MIN                    WNI_CFG_IGNORE_DTIM_STAMIN
#define CFG_IGNORE_DTIM_MAX                    WNI_CFG_IGNORE_DTIM_STAMAX
#define CFG_IGNORE_DTIM_DEFAULT                WNI_CFG_IGNORE_DTIM_STADEF

#define CFG_MAX_LI_MODULATED_DTIM_NAME         "gMaxLIModulatedDTIM"
#define CFG_MAX_LI_MODULATED_DTIM_MIN          ( 1 )
#define CFG_MAX_LI_MODULATED_DTIM_MAX          ( 10 )
#define CFG_MAX_LI_MODULATED_DTIM_DEFAULT      ( 10 )

#define CFG_RX_ANT_CONFIGURATION_NAME          "gNumRxAnt"
#define CFG_RX_ANT_CONFIGURATION_NAME_MIN      ( 1 )
#define CFG_RX_ANT_CONFIGURATION_NAME_MAX      ( 2 )
#define CFG_RX_ANT_CONFIGURATION_NAME_DEFAULT  ( 2 )

#define CFG_FW_HEART_BEAT_MONITORING_NAME      "gEnableFWHeartBeatMonitoring"
#define CFG_FW_HEART_BEAT_MONITORING_MIN       ( 0 )
#define CFG_FW_HEART_BEAT_MONITORING_MAX       ( 1 )
#define CFG_FW_HEART_BEAT_MONITORING_DEFAULT   ( 1 )

#define CFG_FW_BEACON_FILTERING_NAME           "gEnableFWBeaconFiltering"
#define CFG_FW_BEACON_FILTERING_MIN            ( 0 )
#define CFG_FW_BEACON_FILTERING_MAX            ( 1 )
#define CFG_FW_BEACON_FILTERING_DEFAULT        ( 1 )

#define CFG_FW_RSSI_MONITORING_NAME            "gEnableFWRssiMonitoring"
#define CFG_FW_RSSI_MONITORING_MIN             ( 0 )
#define CFG_FW_RSSI_MONITORING_MAX             ( 1 )
#define CFG_FW_RSSI_MONITORING_DEFAULT         ( 1 )

#define CFG_DATA_INACTIVITY_TIMEOUT_NAME       "gDataInactivityTimeout"
#define CFG_DATA_INACTIVITY_TIMEOUT_MIN        ( 1 )
#define CFG_DATA_INACTIVITY_TIMEOUT_MAX        ( 255 )
#define CFG_DATA_INACTIVITY_TIMEOUT_DEFAULT    ( 20 )

#define CFG_NTH_BEACON_FILTER_NAME             "gNthBeaconFilter"
#define CFG_NTH_BEACON_FILTER_MIN              ( WNI_CFG_NTH_BEACON_FILTER_STAMIN )
#define CFG_NTH_BEACON_FILTER_MAX              ( WNI_CFG_NTH_BEACON_FILTER_STAMAX )
#define CFG_NTH_BEACON_FILTER_DEFAULT          ( WNI_CFG_NTH_BEACON_FILTER_STADEF )

#define CFG_RF_SETTLING_TIME_CLK_NAME          "rfSettlingTimeUs"
#define CFG_RF_SETTLING_TIME_CLK_MIN           ( 0 )
#define CFG_RF_SETTLING_TIME_CLK_MAX           ( 60000 )
#define CFG_RF_SETTLING_TIME_CLK_DEFAULT       ( 1500 )

#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_NAME          "gStaKeepAlivePeriod"
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MIN           ( 0 )
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MAX           ( 65535)
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_DEFAULT       ( 0 )

//WMM configuration
#define CFG_QOS_WMM_MODE_NAME                             "WmmIsEnabled"
#define CFG_QOS_WMM_MODE_MIN                               (0)
#define CFG_QOS_WMM_MODE_MAX                               (2) //HDD_WMM_NO_QOS
#define CFG_QOS_WMM_MODE_DEFAULT                           (0) //HDD_WMM_AUTO

#define CFG_QOS_WMM_80211E_ENABLED_NAME                   "80211eIsEnabled"
#define CFG_QOS_WMM_80211E_ENABLED_MIN                     (0)
#define CFG_QOS_WMM_80211E_ENABLED_MAX                     (1) 
#define CFG_QOS_WMM_80211E_ENABLED_DEFAULT                 (0) 

#define CFG_QOS_WMM_UAPSD_MASK_NAME                        "UapsdMask" // ACs to setup U-APSD for at assoc
#define CFG_QOS_WMM_UAPSD_MASK_MIN                         (0x00)
#define CFG_QOS_WMM_UAPSD_MASK_MAX                         (0xFF) 
#define CFG_QOS_WMM_UAPSD_MASK_DEFAULT                     (0xaa)
#define CFG_QOS_WMM_UAPSD_MASK_DYMANIC_WMM_PS_DEFAULT      (0x0)
#define CFG_QOS_WMM_UAPSD_MASK_LEGACY_WMM_PS_DEFAULT       (0x0)

#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_NAME           "InfraUapsdVoSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MAX             (4294967295UL )
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_DEFAULT         (20)

#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_NAME           "InfraUapsdVoSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MAX             (4294967295UL )
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_DEFAULT         (2000)

#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_NAME           "InfraUapsdViSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MAX             (4294967295UL) 
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_DEFAULT         (300)

#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_NAME           "InfraUapsdViSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MAX             (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_DEFAULT         (2000)

#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_NAME           "InfraUapsdBeSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MAX             (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_DEFAULT         (300)

#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_NAME           "InfraUapsdBeSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MAX             (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_DEFAULT         (2000)

#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_NAME           "InfraUapsdBkSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MAX             (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_DEFAULT         (300)

#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_NAME           "InfraUapsdBkSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MIN             (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MAX             (4294967295UL)             
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_DEFAULT         (2000)

#ifdef FEATURE_WLAN_ESE
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_NAME         "InfraInactivityInterval"
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MIN           (0)
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_DEFAULT       (0) //disabled

#define CFG_ESE_FEATURE_ENABLED_NAME                       "EseEnabled"
#define CFG_ESE_FEATURE_ENABLED_MIN                         (0)
#define CFG_ESE_FEATURE_ENABLED_MAX                         (1)
#define CFG_ESE_FEATURE_ENABLED_DEFAULT                     (0) //disabled
#endif // FEATURE_WLAN_ESE

#ifdef FEATURE_WLAN_LFR
#define CFG_LFR_FEATURE_ENABLED_NAME                       "FastRoamEnabled"
#define CFG_LFR_FEATURE_ENABLED_MIN                         (0)
#define CFG_LFR_FEATURE_ENABLED_MAX                         (1)
#define CFG_LFR_FEATURE_ENABLED_DEFAULT                     (0) //disabled

#define CFG_LFR_MAWC_FEATURE_ENABLED_NAME                   "MAWCEnabled"
#define CFG_LFR_MAWC_FEATURE_ENABLED_MIN                    (0)
#define CFG_LFR_MAWC_FEATURE_ENABLED_MAX                    (1)
#define CFG_LFR_MAWC_FEATURE_ENABLED_DEFAULT                (0) /* disabled */
#endif // FEATURE_WLAN_LFR

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
// This flag will control fasttransition in case of 11r and ESE.
// Basically with this the whole neighbor roam, pre-auth, reassoc
// can be turned ON/OFF. 
// With this turned OFF 11r will completely not work.
// For 11r this flag has to be ON.
// For ESE fastroam will not work.
#define CFG_FAST_TRANSITION_ENABLED_NAME                    "FastTransitionEnabled"
#define CFG_FAST_TRANSITION_ENABLED_NAME_MIN                (0)
#define CFG_FAST_TRANSITION_ENABLED_NAME_MAX                (1)
#define CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT            (1) //Enabled

/* This parameter is used to decide whether to Roam or not.
 * AP1 is the currently associated AP and AP2 is chosen for roaming.
 * The Roaming will happen only if AP2 has better Signal Quality and it has a RSSI better than AP1
 * in terms of RoamRssiDiff,and RoamRssiDiff is the number of units (typically measured in dB) AP2
 * is better than AP1. 
 * This check is not done if the value is Zero */
#define CFG_ROAM_RSSI_DIFF_NAME                             "RoamRssiDiff"
#define CFG_ROAM_RSSI_DIFF_MIN                              (0)
#define CFG_ROAM_RSSI_DIFF_MAX                              (30)
#define CFG_ROAM_RSSI_DIFF_DEFAULT                          (5)

/*
 * Following a scan and if potential roam candidate(s) are found,
 * then determine whether to register for reassoc threshold or roam 
 * immediately based on this configuration parameter. If the RSSI 
 * of any available candidate is better than the currently associated 
 * AP by at least gImmediateRoamRssiDiff, then being to roam 
 * immediately. 
 * NOTE: Value of 0 means that immediate roaming is enabled by default
 */
#define CFG_IMMEDIATE_ROAM_RSSI_DIFF_NAME                   "gImmediateRoamRssiDiff"
#define CFG_IMMEDIATE_ROAM_RSSI_DIFF_MIN                    (0)
#define CFG_IMMEDIATE_ROAM_RSSI_DIFF_MAX                    (125)
#define CFG_IMMEDIATE_ROAM_RSSI_DIFF_DEFAULT                (0)

/*This parameter is used to set Wireless Extended Security Mode.*/
#define CFG_ENABLE_WES_MODE_NAME                            "gWESModeEnabled"
#define CFG_ENABLE_WES_MODE_NAME_MIN                        (0)
#define CFG_ENABLE_WES_MODE_NAME_MAX                        (1)
#define CFG_ENABLE_WES_MODE_NAME_DEFAULT                    (0)

#define CFG_ROAM_SCAN_N_PROBES                             "gRoamScanNProbes"
#define CFG_ROAM_SCAN_N_PROBES_MIN                          (1)
#define CFG_ROAM_SCAN_N_PROBES_MAX                          (10)
#define CFG_ROAM_SCAN_N_PROBES_DEFAULT                      (2)

#define CFG_ROAM_SCAN_HOME_AWAY_TIME                        "gRoamScanHomeAwayTime"
#define CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN                    (3)
#define CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX                    (300)
#define CFG_ROAM_SCAN_HOME_AWAY_TIME_DEFAULT                (CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN)
#endif /* (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR) */

#ifdef FEATURE_WLAN_OKC
#define CFG_OKC_FEATURE_ENABLED_NAME                       "OkcEnabled"
#define CFG_OKC_FEATURE_ENABLED_MIN                        (0)
#define CFG_OKC_FEATURE_ENABLED_MAX                        (1)
#define CFG_OKC_FEATURE_ENABLED_DEFAULT                    (1)
#endif

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED                       "gRoamScanOffloadEnabled"
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED_MIN                   (0)
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED_MAX                   (1)
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED_DEFAULT               (1)
#endif

#define CFG_QOS_WMM_PKT_CLASSIFY_BASIS_NAME                "PktClassificationBasis" // DSCP or 802.1Q
#define CFG_QOS_WMM_PKT_CLASSIFY_BASIS_MIN                  (0)
#define CFG_QOS_WMM_PKT_CLASSIFY_BASIS_MAX                  (1)
#define CFG_QOS_WMM_PKT_CLASSIFY_BASIS_DEFAULT              (0) //DSCP

/* default TSPEC parameters for AC_VO */
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_NAME                   "InfraDirAcVo"
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_MIN                     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_MAX                     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_DEFAULT                 (3) //WLAN_QCT_CUST_WMM_TSDIR_BOTH

#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_NAME         "InfraNomMsduSizeAcVo"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MIN           (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MAX           (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_DEFAULT       (0x80D0)

#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_NAME        "InfraMeanDataRateAcVo"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_DEFAULT      (0x14500)

#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_NAME          "InfraMinPhyRateAcVo"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MIN            (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MAX            (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_DEFAULT        (0x5B8D80)

#define CFG_QOS_WMM_INFRA_SBA_AC_VO_NAME                   "InfraSbaAcVo"
#define CFG_QOS_WMM_INFRA_SBA_AC_VO_MIN                     (0x2001)
#define CFG_QOS_WMM_INFRA_SBA_AC_VO_MAX                     (0xFFFF)
#define CFG_QOS_WMM_INFRA_SBA_AC_VO_DEFAULT                 (0x2001)

/* default TSPEC parameters for AC_VI */
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_NAME                   "InfraDirAcVi"
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_MIN                     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_MAX                     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_DEFAULT                 (3) //WLAN_QCT_CUST_WMM_TSDIR_BOTH

#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_NAME         "InfraNomMsduSizeAcVi"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MIN           (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MAX           (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_DEFAULT       (0x85DC)

#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_NAME        "InfraMeanDataRateAcVi"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_DEFAULT      (0x57E40)

#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_NAME          "InfraMinPhyRateAcVi"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MIN            (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MAX            (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_DEFAULT        (0x5B8D80)

#define CFG_QOS_WMM_INFRA_SBA_AC_VI_NAME                   "InfraSbaAcVi"
#define CFG_QOS_WMM_INFRA_SBA_AC_VI_MIN                     (0x2001)
#define CFG_QOS_WMM_INFRA_SBA_AC_VI_MAX                     (0xFFFF)
#define CFG_QOS_WMM_INFRA_SBA_AC_VI_DEFAULT                 (0x2001)

/* default TSPEC parameters for AC_BE*/
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_NAME                   "InfraDirAcBe"
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_MIN                     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_MAX                     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_DEFAULT                 (3) //WLAN_QCT_CUST_WMM_TSDIR_BOTH

#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_NAME         "InfraNomMsduSizeAcBe"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MIN           (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MAX           (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_DEFAULT       (0x85DC)

#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_NAME        "InfraMeanDataRateAcBe"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_DEFAULT      (0x493E0)

#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_NAME          "InfraMinPhyRateAcBe"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MIN            (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MAX            (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_DEFAULT        (0x5B8D80)

#define CFG_QOS_WMM_INFRA_SBA_AC_BE_NAME                   "InfraSbaAcBe"
#define CFG_QOS_WMM_INFRA_SBA_AC_BE_MIN                     (0x2001)
#define CFG_QOS_WMM_INFRA_SBA_AC_BE_MAX                     (0xFFFF)
#define CFG_QOS_WMM_INFRA_SBA_AC_BE_DEFAULT                 (0x2001)

/* default TSPEC parameters for AC_Bk*/
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_NAME                   "InfraDirAcBk"
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_MIN                     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_MAX                     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_DEFAULT                 (3) //WLAN_QCT_CUST_WMM_TSDIR_BOTH

#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_NAME         "InfraNomMsduSizeAcBk"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MIN           (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MAX           (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_DEFAULT       (0x85DC)

#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_NAME        "InfraMeanDataRateAcBk"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_DEFAULT      (0x493E0)

#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_NAME          "InfraMinPhyRateAcBk"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MIN            (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MAX            (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_DEFAULT        (0x5B8D80)

#define CFG_QOS_WMM_INFRA_SBA_AC_BK_NAME                   "InfraSbaAcBk"
#define CFG_QOS_WMM_INFRA_SBA_AC_BK_MIN                     (0x2001)
#define CFG_QOS_WMM_INFRA_SBA_AC_BK_MAX                     (0xFFFF)
#define CFG_QOS_WMM_INFRA_SBA_AC_BK_DEFAULT                 (0x2001)

// TL configuration
#define CFG_TL_WFQ_BK_WEIGHT_NAME                           "WfqBkWeight"
#define CFG_TL_WFQ_BK_WEIGHT_MIN                            1
#define CFG_TL_WFQ_BK_WEIGHT_MAX                            0xFF
#define CFG_TL_WFQ_BK_WEIGHT_DEFAULT                        1

#define CFG_TL_WFQ_BE_WEIGHT_NAME                           "WfqBeWeight"
#define CFG_TL_WFQ_BE_WEIGHT_MIN                            1
#define CFG_TL_WFQ_BE_WEIGHT_MAX                            0xFF
#define CFG_TL_WFQ_BE_WEIGHT_DEFAULT                        3

#define CFG_TL_WFQ_VI_WEIGHT_NAME                           "WfqViWeight"
#define CFG_TL_WFQ_VI_WEIGHT_MIN                            1
#define CFG_TL_WFQ_VI_WEIGHT_MAX                            0xFF
#define CFG_TL_WFQ_VI_WEIGHT_DEFAULT                        5

#define CFG_TL_WFQ_VO_WEIGHT_NAME                           "WfqVoWeight"
#define CFG_TL_WFQ_VO_WEIGHT_MIN                            1
#define CFG_TL_WFQ_VO_WEIGHT_MAX                            0xFF
#define CFG_TL_WFQ_VO_WEIGHT_DEFAULT                        7

#define CFG_TL_DELAYED_TRGR_FRM_INT_NAME                   "DelayedTriggerFrmInt"
#define CFG_TL_DELAYED_TRGR_FRM_INT_MIN                     1
#define CFG_TL_DELAYED_TRGR_FRM_INT_MAX                     (4294967295UL)
#define CFG_TL_DELAYED_TRGR_FRM_INT_DEFAULT                 3000

#define CFG_REORDER_TIME_BK_NAME                           "BkReorderTime"
#define CFG_REORDER_TIME_BK_MIN                            30
#define CFG_REORDER_TIME_BK_MAX                            1000
#define CFG_REORDER_TIME_BK_DEFAULT                        100

#define CFG_REORDER_TIME_BE_NAME                           "BeReorderTime"
#define CFG_REORDER_TIME_BE_MIN                            30
#define CFG_REORDER_TIME_BE_MAX                            1000
#define CFG_REORDER_TIME_BE_DEFAULT                        100

#define CFG_REORDER_TIME_VI_NAME                           "ViReorderTime"
#define CFG_REORDER_TIME_VI_MIN                            30
#define CFG_REORDER_TIME_VI_MAX                            1000
#define CFG_REORDER_TIME_VI_DEFAULT                        100

#define CFG_REORDER_TIME_VO_NAME                           "VoReorderTime"
#define CFG_REORDER_TIME_VO_MIN                            30
#define CFG_REORDER_TIME_VO_MAX                            1000
#define CFG_REORDER_TIME_VO_DEFAULT                        40

#if defined WLAN_FEATURE_VOWIFI
#define CFG_RRM_ENABLE_NAME                              "gRrmEnable"
#define CFG_RRM_ENABLE_MIN                               (0)
#define CFG_RRM_ENABLE_MAX                               (1)  
#define CFG_RRM_ENABLE_DEFAULT                           (0)

#define CFG_RRM_OPERATING_CHAN_MAX_DURATION_NAME         "gRrmOperChanMax" //section 11.10.3 IEEE std. 802.11k-2008
#define CFG_RRM_OPERATING_CHAN_MAX_DURATION_MIN          (0)             //Maxduration = 2^(maxDuration - 4) * bcnIntvl.
#define CFG_RRM_OPERATING_CHAN_MAX_DURATION_MAX          (8)  
#define CFG_RRM_OPERATING_CHAN_MAX_DURATION_DEFAULT      (3)             //max duration = 2^-1 * bcnIntvl (50% of bcn intvl)

#define CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_NAME     "gRrmNonOperChanMax" //Same as above.
#define CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_MIN      (0)
#define CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_MAX      (8)  
#define CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_DEFAULT  (3)

#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_NAME            "gRrmRandnIntvl"
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_MIN             (10)
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_MAX             (100)  
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_DEFAULT         (100)
#endif

#define CFG_QOS_IMPLICIT_SETUP_ENABLED_NAME                 "ImplicitQosIsEnabled"
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_MIN                  (0)
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_MAX                  (1) 
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_DEFAULT              (1)

#define CFG_ENABLE_LOGP_NAME                                "gEnableLogp"
#define CFG_ENABLE_LOGP_MIN                                 ( 0 )
#define CFG_ENABLE_LOGP_MAX                                 ( 1 )
#define CFG_ENABLE_LOGP_DEFAULT                             ( 0 )

#define CFG_BTC_EXECUTION_MODE_NAME                         "BtcExecutionMode"
#define CFG_BTC_EXECUTION_MODE_MIN                          ( 0 )
#define CFG_BTC_EXECUTION_MODE_MAX                          ( 5 )
#define CFG_BTC_EXECUTION_MODE_DEFAULT                      ( 0 )

#define CFG_BTC_DHCP_PROTECTION_NAME                         "BtcConsBtSlotToBlockDuringDhcp"
#define CFG_BTC_DHCP_PROTECTION_MIN                          ( 0 )
#define CFG_BTC_DHCP_PROTECTION_MAX                          ( 0xFF )
#define CFG_BTC_DHCP_PROTECTION_DEFAULT                      ( 0 )

#define CFG_BTC_A2DP_DHCP_PROTECTION_NAME                    "BtcA2DPDhcpProtectLevel"
#define CFG_BTC_A2DP_DHCP_PROTECTION_MIN                     ( 0 )
#define CFG_BTC_A2DP_DHCP_PROTECTION_MAX                     ( 0xFF )
#define CFG_BTC_A2DP_DHCP_PROTECTION_DEFAULT                 ( 7 )

#define CFG_BTC_STATIC_LEN_INQ_BT_NAME       "btcStaticLenInqBt"
#define CFG_BTC_STATIC_LEN_INQ_BT_MIN        ( 5000 )
#define CFG_BTC_STATIC_LEN_INQ_BT_MAX        ( 500000 )
#define CFG_BTC_STATIC_LEN_INQ_BT_DEFAULT    ( 120000 )

#define CFG_BTC_STATIC_LEN_PAGE_BT_NAME      "btcStaticLenPageBt"
#define CFG_BTC_STATIC_LEN_PAGE_BT_MIN       ( 5000 )
#define CFG_BTC_STATIC_LEN_PAGE_BT_MAX       ( 500000 )
#define CFG_BTC_STATIC_LEN_PAGE_BT_DEFAULT   ( 120000 )

#define CFG_BTC_STATIC_LEN_CONN_BT_NAME      "btcStaticLenConnBt"
#define CFG_BTC_STATIC_LEN_CONN_BT_MIN       ( 5000 )
#define CFG_BTC_STATIC_LEN_CONN_BT_MAX       ( 500000 )
#define CFG_BTC_STATIC_LEN_CONN_BT_DEFAULT   ( 120000 )

#define CFG_BTC_STATIC_LEN_LE_BT_NAME        "btcStaticLenLeBt"
#define CFG_BTC_STATIC_LEN_LE_BT_MIN         ( 5000 )
#define CFG_BTC_STATIC_LEN_LE_BT_MAX         ( 500000 )
#define CFG_BTC_STATIC_LEN_LE_BT_DEFAULT     ( 120000 )
  
#define CFG_BTC_STATIC_LEN_INQ_WLAN_NAME     "btcStaticLenInqWlan"
#define CFG_BTC_STATIC_LEN_INQ_WLAN_MIN      ( 0 )
#define CFG_BTC_STATIC_LEN_INQ_WLAN_MAX      ( 500000 )
#define CFG_BTC_STATIC_LEN_INQ_WLAN_DEFAULT  ( 30000 )

#define CFG_BTC_STATIC_LEN_PAGE_WLAN_NAME    "btcStaticLenPageWlan"
#define CFG_BTC_STATIC_LEN_PAGE_WLAN_MIN     ( 0 )
#define CFG_BTC_STATIC_LEN_PAGE_WLAN_MAX     ( 500000 )
#define CFG_BTC_STATIC_LEN_PAGE_WLAN_DEFAULT ( 30000 )

#define CFG_BTC_STATIC_LEN_CONN_WLAN_NAME    "btcStaticLenConnWlan"
#define CFG_BTC_STATIC_LEN_CONN_WLAN_MIN     ( 0 )
#define CFG_BTC_STATIC_LEN_CONN_WLAN_MAX     ( 500000 )
#define CFG_BTC_STATIC_LEN_CONN_WLAN_DEFAULT ( 30000 )

#define CFG_BTC_STATIC_LEN_LE_WLAN_NAME      "btcStaticLenLeWlan"
#define CFG_BTC_STATIC_LEN_LE_WLAN_MIN       ( 0 )
#define CFG_BTC_STATIC_LEN_LE_WLAN_MAX       ( 500000 )
#define CFG_BTC_STATIC_LEN_LE_WLAN_DEFAULT   ( 30000 )

#define CFG_BTC_DYN_MAX_LEN_BT_NAME          "btcDynMaxLenBt"
#define CFG_BTC_DYN_MAX_LEN_BT_MIN           ( 25000 )
#define CFG_BTC_DYN_MAX_LEN_BT_MAX           ( 500000 )
#define CFG_BTC_DYN_MAX_LEN_BT_DEFAULT       ( 250000 )

#define CFG_BTC_DYN_MAX_LEN_WLAN_NAME        "btcDynMaxLenWlan"
#define CFG_BTC_DYN_MAX_LEN_WLAN_MIN         ( 15000 )
#define CFG_BTC_DYN_MAX_LEN_WLAN_MAX         ( 500000 )
#define CFG_BTC_DYN_MAX_LEN_WLAN_DEFAULT     ( 45000 )

#define CFG_BTC_MAX_SCO_BLOCK_PERC_NAME      "btcMaxScoBlockPerc"
#define CFG_BTC_MAX_SCO_BLOCK_PERC_MIN       ( 0 )
#define CFG_BTC_MAX_SCO_BLOCK_PERC_MAX       ( 100 )
#define CFG_BTC_MAX_SCO_BLOCK_PERC_DEFAULT   ( 1 )

#define CFG_BTC_DHCP_PROT_ON_A2DP_NAME       "btcDhcpProtOnA2dp"
#define CFG_BTC_DHCP_PROT_ON_A2DP_MIN        ( 0 )
#define CFG_BTC_DHCP_PROT_ON_A2DP_MAX        ( 1 )
#define CFG_BTC_DHCP_PROT_ON_A2DP_DEFAULT    ( 1 )

#define CFG_BTC_DHCP_PROT_ON_SCO_NAME        "btcDhcpProtOnSco"
#define CFG_BTC_DHCP_PROT_ON_SCO_MIN         ( 0 )
#define CFG_BTC_DHCP_PROT_ON_SCO_MAX         ( 1 )
#define CFG_BTC_DHCP_PROT_ON_SCO_DEFAULT     ( 0 )

#define CFG_MWS_COEX_V1_WAN_FREQ_NAME        "mwsCoexVictim1WANFreq"
#define CFG_MWS_COEX_VX_WAN_FREQ_MIN         ( 0 )
#define CFG_MWS_COEX_VX_WAN_FREQ_MAX         ( 0xFFFFFFFF )
#define CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT     ( 0 )

#define CFG_MWS_COEX_V1_WLAN_FREQ_NAME       "mwsCoexVictim1WLANFreq"
#define CFG_MWS_COEX_VX_WLAN_FREQ_MIN        ( 0 )
#define CFG_MWS_COEX_VX_WLAN_FREQ_MAX        ( 0xFFFFFFFF )
#define CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT    ( 0 )

#define CFG_MWS_COEX_V1_CONFIG_NAME          "mwsCoexVictim1Config"
#define CFG_MWS_COEX_V1_CONFIG2_NAME         "mwsCoexVictim1Config2"
#define CFG_MWS_COEX_VX_CONFIG_MIN           ( 0 )
#define CFG_MWS_COEX_VX_CONFIG_MAX           ( 0xFFFFFFFF )
#define CFG_MWS_COEX_VX_CONFIG_DEFAULT       ( 0 )

#define CFG_MWS_COEX_V2_WAN_FREQ_NAME        "mwsCoexVictim2WANFreq"
#define CFG_MWS_COEX_V2_WLAN_FREQ_NAME       "mwsCoexVictim2WLANFreq"
#define CFG_MWS_COEX_V2_CONFIG_NAME          "mwsCoexVictim2Config"
#define CFG_MWS_COEX_V2_CONFIG2_NAME         "mwsCoexVictim2Config2"
#define CFG_MWS_COEX_V3_WAN_FREQ_NAME        "mwsCoexVictim3WANFreq"
#define CFG_MWS_COEX_V3_WLAN_FREQ_NAME       "mwsCoexVictim3WLANFreq"
#define CFG_MWS_COEX_V3_CONFIG_NAME          "mwsCoexVictim3Config"
#define CFG_MWS_COEX_V3_CONFIG2_NAME         "mwsCoexVictim3Config2"
#define CFG_MWS_COEX_V4_WAN_FREQ_NAME        "mwsCoexVictim4WANFreq"
#define CFG_MWS_COEX_V4_WLAN_FREQ_NAME       "mwsCoexVictim4WLANFreq"
#define CFG_MWS_COEX_V4_CONFIG_NAME          "mwsCoexVictim4Config"
#define CFG_MWS_COEX_V4_CONFIG2_NAME         "mwsCoexVictim4Config2"
#define CFG_MWS_COEX_V5_WAN_FREQ_NAME        "mwsCoexVictim5WANFreq"
#define CFG_MWS_COEX_V5_WLAN_FREQ_NAME       "mwsCoexVictim5WLANFreq"
#define CFG_MWS_COEX_V5_CONFIG_NAME          "mwsCoexVictim5Config"
#define CFG_MWS_COEX_V5_CONFIG2_NAME         "mwsCoexVictim5Config2"
#define CFG_MWS_COEX_V6_WAN_FREQ_NAME        "mwsCoexVictim6WANFreq"
#define CFG_MWS_COEX_V6_WLAN_FREQ_NAME       "mwsCoexVictim6WLANFreq"
#define CFG_MWS_COEX_V6_CONFIG_NAME          "mwsCoexVictim6Config"
#define CFG_MWS_COEX_V6_CONFIG2_NAME         "mwsCoexVictim6Config2"
#define CFG_MWS_COEX_V7_WAN_FREQ_NAME        "mwsCoexVictim7WANFreq"
#define CFG_MWS_COEX_V7_WLAN_FREQ_NAME       "mwsCoexVictim7WLANFreq"
#define CFG_MWS_COEX_V7_CONFIG_NAME          "mwsCoexVictim7Config"
#define CFG_MWS_COEX_V7_CONFIG2_NAME         "mwsCoexVictim7Config2"
#define CFG_MWS_COEX_V8_WAN_FREQ_NAME        "mwsCoexVictim8WANFreq"
#define CFG_MWS_COEX_V8_WLAN_FREQ_NAME       "mwsCoexVictim8WLANFreq"
#define CFG_MWS_COEX_V8_CONFIG_NAME          "mwsCoexVictim8Config"
#define CFG_MWS_COEX_V8_CONFIG2_NAME         "mwsCoexVictim8Config2"
#define CFG_MWS_COEX_V9_WAN_FREQ_NAME        "mwsCoexVictim9WANFreq"
#define CFG_MWS_COEX_V9_WLAN_FREQ_NAME       "mwsCoexVictim9WLANFreq"
#define CFG_MWS_COEX_V9_CONFIG_NAME          "mwsCoexVictim9Config"
#define CFG_MWS_COEX_V9_CONFIG2_NAME         "mwsCoexVictim9Config2"
#define CFG_MWS_COEX_V10_WAN_FREQ_NAME       "mwsCoexVictim10WANFreq"
#define CFG_MWS_COEX_V10_WLAN_FREQ_NAME      "mwsCoexVictim10WLANFreq"
#define CFG_MWS_COEX_V10_CONFIG_NAME         "mwsCoexVictim10Config"
#define CFG_MWS_COEX_V10_CONFIG2_NAME        "mwsCoexVictim10Config2"

#define CFG_MWS_COEX_MODEM_BACKOFF_NAME      "mwsCoexModemBackoff"
#define CFG_MWS_COEX_MODEM_BACKOFF_MIN       ( 0 )
#define CFG_MWS_COEX_MODEM_BACKOFF_MAX       ( 0xFFFFFFFF )
#define CFG_MWS_COEX_MODEM_BACKOFF_DEFAULT   ( 0 )

#define CFG_MWS_COEX_CONFIG1_NAME            "mwsCoexConfig1"
#define CFG_MWS_COEX_CONFIGX_MIN             ( 0 )
#define CFG_MWS_COEX_CONFIGX_MAX             ( 0xFFFFFFFF )
#define CFG_MWS_COEX_CONFIGX_DEFAULT         ( 0 )
#define CFG_MWS_COEX_CONFIG2_NAME            "mwsCoexConfig2"
#define CFG_MWS_COEX_CONFIG3_NAME            "mwsCoexConfig3"
#define CFG_MWS_COEX_CONFIG4_NAME            "mwsCoexConfig4"
#define CFG_MWS_COEX_CONFIG5_NAME            "mwsCoexConfig5"
#define CFG_MWS_COEX_CONFIG6_NAME            "mwsCoexConfig6"

#define CFG_SAR_POWER_BACKOFF_NAME           "SARPowerBackoff"
#define CFG_SAR_POWER_BACKOFF_MIN            ( 0 )
#define CFG_SAR_POWER_BACKOFF_MAX            ( 0xFFFFFFFF )
#define CFG_SAR_POWER_BACKOFF_DEFAULT        ( 13 )

#if defined WLAN_FEATURE_VOWIFI_11R
#define CFG_FT_RESOURCE_REQ_NAME                        "gFTResourceReqSupported"
#define CFG_FT_RESOURCE_REQ_MIN                         (0)
#define CFG_FT_RESOURCE_REQ_MAX                         (1)
#define CFG_FT_RESOURCE_REQ_DEFAULT                     (0)
#endif

#define CFG_TELE_BCN_TRANS_LI_NAME                   "telescopicBeaconTransListenInterval"
#define CFG_TELE_BCN_TRANS_LI_MIN                    ( 0 )
#define CFG_TELE_BCN_TRANS_LI_MAX                    ( 7 )
#define CFG_TELE_BCN_TRANS_LI_DEFAULT                ( 3 )

#define CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_NAME     "telescopicBeaconTransListenIntervalNumIdleBcns"
#define CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_MIN      ( 5 )
#define CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_MAX      ( 255 )
#define CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_DEFAULT  ( 10 )

#define CFG_TELE_BCN_MAX_LI_NAME                     "telescopicBeaconMaxListenInterval"
#define CFG_TELE_BCN_MAX_LI_MIN                      ( 0 )
#define CFG_TELE_BCN_MAX_LI_MAX                      ( 7 )
#define CFG_TELE_BCN_MAX_LI_DEFAULT                  ( 5 )

#define CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_NAME       "telescopicBeaconMaxListenIntervalNumIdleBcns"
#define CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_MIN        ( 5 )
#define CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_MAX        ( 255 )
#define CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_DEFAULT    ( 15 )

#define CFG_BCN_EARLY_TERM_WAKE_NAME                 "beaconEarlyTerminationWakeInterval"
#define CFG_BCN_EARLY_TERM_WAKE_MIN                  ( 2 )
#define CFG_BCN_EARLY_TERM_WAKE_MAX                  ( 255 )
#define CFG_BCN_EARLY_TERM_WAKE_DEFAULT              ( 3 )

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_NAME             "gNeighborScanTimerPeriod"
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN              (3)
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX              (300)
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT          (200)

#define CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_NAME              "gNeighborReassocThreshold"
#define CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_MIN               (10)
#define CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_MAX               (125)
#define CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_DEFAULT           (83)

#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_NAME      "gNeighborLookupThreshold"
#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN       (0)
#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX       (120)
#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT   (0) /*A Zero value indicates the host to calculate
                                                           Adaptive thresold based on the minimum
                                                           supported data rate.*/

#define CFG_NEIGHBOR_SCAN_CHAN_LIST_NAME                      "gNeighborScanChannelList"
#define CFG_NEIGHBOR_SCAN_CHAN_LIST_DEFAULT                   ""

#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_NAME                  "gNeighborScanChannelMinTime"
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN                   (10)   
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX                   (40)   
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT               (20)   

#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_NAME                  "gNeighborScanChannelMaxTime"
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN                   (3)
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX                   (300)
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT               (30)   

#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_NAME           "gMaxNeighborReqTries"
#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MIN            (1)
#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MAX            (4)
#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_DEFAULT        (3)


#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_NAME         "gNeighborScanRefreshPeriod"
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN          (1000)
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX          (60000)
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT      (20000)

#define CFG_EMPTY_SCAN_REFRESH_PERIOD_NAME         "gEmptyScanRefreshPeriod"
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN          (0)
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX          (60000)
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_DEFAULT      (0)

#define CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_NAME      "gNeighborInitialForcedRoamTo5GhEnable"
#define CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_MIN       (0)
#define CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_MAX       (1)
#define CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_DEFAULT   (0)

#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */

#define CFG_QOS_WMM_BURST_SIZE_DEFN_NAME                        "burstSizeDefinition" 
#define CFG_QOS_WMM_BURST_SIZE_DEFN_MIN                         (0)
#define CFG_QOS_WMM_BURST_SIZE_DEFN_MAX                         (1)
#define CFG_QOS_WMM_BURST_SIZE_DEFN_DEFAULT                     (0)

#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_NAME                        "tsInfoAckPolicy" 
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_MIN                         (0x00)
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_MAX                         (0x01)
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_DEFAULT                     (0x00)

#define CFG_SINGLE_TID_RC_NAME                             "SingleTIDRC"
#define CFG_SINGLE_TID_RC_MIN                               (0) // Seperate replay counter for all TID
#define CFG_SINGLE_TID_RC_MAX                               (1) // Single replay counter for all TID 
#define CFG_SINGLE_TID_RC_DEFAULT                           (1) 
#define CFG_MCAST_BCAST_FILTER_SETTING_NAME          "McastBcastFilter"
#define CFG_MCAST_BCAST_FILTER_SETTING_MIN           (0)
#define CFG_MCAST_BCAST_FILTER_SETTING_MAX           (3)
#define CFG_MCAST_BCAST_FILTER_SETTING_DEFAULT       (0)

#define CFG_DYNAMIC_PSPOLL_VALUE_NAME          "gDynamicPSPollvalue"
#define CFG_DYNAMIC_PSPOLL_VALUE_MIN           (0)
#define CFG_DYNAMIC_PSPOLL_VALUE_MAX           (255)
#define CFG_DYNAMIC_PSPOLL_VALUE_DEFAULT       (0)

#define CFG_TELE_BCN_WAKEUP_EN_NAME            "gTelescopicBeaconWakeupEn"
#define CFG_TELE_BCN_WAKEUP_EN_MIN             (0)
#define CFG_TELE_BCN_WAKEUP_EN_MAX             (1)
#define CFG_TELE_BCN_WAKEUP_EN_DEFAULT         (0)

#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_NAME                 "gAddTSWhenACMIsOff"
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MIN                  (0)
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MAX                  (1) //Send AddTs even when ACM is not set for the AC
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_DEFAULT              (0)


#define CFG_VALIDATE_SCAN_LIST_NAME                 "gValidateScanList"
#define CFG_VALIDATE_SCAN_LIST_MIN                  (0)
#define CFG_VALIDATE_SCAN_LIST_MAX                  (1) 
#define CFG_VALIDATE_SCAN_LIST_DEFAULT              (0)

#define CFG_NULLDATA_AP_RESP_TIMEOUT_NAME       "gNullDataApRespTimeout"
#define CFG_NULLDATA_AP_RESP_TIMEOUT_MIN        ( WNI_CFG_PS_NULLDATA_AP_RESP_TIMEOUT_STAMIN )
#define CFG_NULLDATA_AP_RESP_TIMEOUT_MAX        ( WNI_CFG_PS_NULLDATA_AP_RESP_TIMEOUT_STAMAX )
#define CFG_NULLDATA_AP_RESP_TIMEOUT_DEFAULT    ( WNI_CFG_PS_NULLDATA_AP_RESP_TIMEOUT_STADEF )

#define CFG_AP_DATA_AVAIL_POLL_PERIOD_NAME      "gApDataAvailPollInterval"
#define CFG_AP_DATA_AVAIL_POLL_PERIOD_MIN       ( WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD_STAMIN )
#define CFG_AP_DATA_AVAIL_POLL_PERIOD_MAX       ( WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD_STAMAX )
#define CFG_AP_DATA_AVAIL_POLL_PERIOD_DEFAULT   ( WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD_STADEF )

#define CFG_ENABLE_HOST_ARPOFFLOAD_NAME         "hostArpOffload"
#define CFG_ENABLE_HOST_ARPOFFLOAD_MIN          ( 0 )
#define CFG_ENABLE_HOST_ARPOFFLOAD_MAX          ( 1 )
#define CFG_ENABLE_HOST_ARPOFFLOAD_DEFAULT      ( 0 )

#define CFG_ENABLE_HOST_NSOFFLOAD_NAME         "hostNSOffload"
#define CFG_ENABLE_HOST_NSOFFLOAD_MIN          ( 0 )
#define CFG_ENABLE_HOST_NSOFFLOAD_MAX          ( 1 )
#define CFG_ENABLE_HOST_NSOFFLOAD_DEFAULT      ( 0 )


#define CFG_ENABLE_BTAMP_NAME                   "gEnableBtAmp"
#define CFG_ENABLE_BTAMP_MIN                    ( 0 )
#define CFG_ENABLE_BTAMP_MAX                    ( 1 )
#define CFG_ENABLE_BTAMP_DEFAULT                ( 0 )

#ifdef WLAN_BTAMP_FEATURE
#define CFG_BT_AMP_PREFERRED_CHANNEL_NAME          "BtAmpPreferredChannel"
#define CFG_BT_AMP_PREFERRED_CHANNEL_MIN           (1)
#define CFG_BT_AMP_PREFERRED_CHANNEL_MAX           (11)
#define CFG_BT_AMP_PREFERRED_CHANNEL_DEFAULT       (1)
#endif //WLAN_BTAMP_FEATURE

#define CFG_BAND_CAPABILITY_NAME          "BandCapability"
#define CFG_BAND_CAPABILITY_MIN           (0)
#define CFG_BAND_CAPABILITY_MAX           (2)
#define CFG_BAND_CAPABILITY_DEFAULT       (1)

#define CFG_ENABLE_BEACON_EARLY_TERMINATION_NAME          "enableBeaconEarlyTermination"
#define CFG_ENABLE_BEACON_EARLY_TERMINATION_MIN           ( 0 )
#define CFG_ENABLE_BEACON_EARLY_TERMINATION_MAX           ( 1 )
#define CFG_ENABLE_BEACON_EARLY_TERMINATION_DEFAULT       ( 0 )

#define CFG_ENABLE_CLOSE_LOOP_NAME                 "gEnableCloseLoop"
#define CFG_ENABLE_CLOSE_LOOP_MIN                  WNI_CFG_FIXED_RATE_STAMIN
#define CFG_ENABLE_CLOSE_LOOP_MAX                  WNI_CFG_FIXED_RATE_STAMAX
#define CFG_ENABLE_CLOSE_LOOP_DEFAULT              WNI_CFG_FIXED_RATE_STADEF

#define CFG_ENABLE_BYPASS_11D_NAME                 "gEnableBypass11d"
#define CFG_ENABLE_BYPASS_11D_MIN                  ( 0 )
#define CFG_ENABLE_BYPASS_11D_MAX                  ( 1 )
#define CFG_ENABLE_BYPASS_11D_DEFAULT              ( 1 )

#define CFG_ENABLE_DFS_CHNL_SCAN_NAME              "gEnableDFSChnlScan"
#define CFG_ENABLE_DFS_CHNL_SCAN_MIN               ( 0 )
#define CFG_ENABLE_DFS_CHNL_SCAN_MAX               ( 1 )
#define CFG_ENABLE_DFS_CHNL_SCAN_DEFAULT           ( 1 )

#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_NAME              "gEnableDFSPnoChnlScan"
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_MIN               ( 0 )
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_MAX               ( 1 )
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_DEFAULT           ( 1 )

typedef enum
{
    eHDD_LINK_SPEED_REPORT_ACTUAL = 0,
    eHDD_LINK_SPEED_REPORT_MAX = 1,
    eHDD_LINK_SPEED_REPORT_MAX_SCALED = 2,
}eHddLinkSpeedReportType;
#ifdef WLAN_FEATURE_11AC
#define CFG_VHT_CHANNEL_WIDTH                "gVhtChannelWidth"
#define CFG_VHT_CHANNEL_WIDTH_MIN            ( 0 )
#define CFG_VHT_CHANNEL_WIDTH_MAX            ( 2 )
#define CFG_VHT_CHANNEL_WIDTH_DEFAULT        ( 2 )

#define CFG_VHT_ENABLE_RX_MCS_8_9               "gVhtRxMCS"
#define CFG_VHT_ENABLE_RX_MCS_8_9_MIN           ( 0 )
#define CFG_VHT_ENABLE_RX_MCS_8_9_MAX           ( 2 )
#define CFG_VHT_ENABLE_RX_MCS_8_9_DEFAULT       ( 0 )

#define CFG_VHT_ENABLE_TX_MCS_8_9               "gVhtTxMCS"
#define CFG_VHT_ENABLE_TX_MCS_8_9_MIN           ( 0 )
#define CFG_VHT_ENABLE_TX_MCS_8_9_MAX           ( 2 )
#define CFG_VHT_ENABLE_TX_MCS_8_9_DEFAULT       ( 0 )

#endif

#define CFG_REPORT_MAX_LINK_SPEED                  "gReportMaxLinkSpeed"
#define CFG_REPORT_MAX_LINK_SPEED_MIN              ( eHDD_LINK_SPEED_REPORT_ACTUAL )
#define CFG_REPORT_MAX_LINK_SPEED_MAX              ( eHDD_LINK_SPEED_REPORT_MAX_SCALED )
#define CFG_REPORT_MAX_LINK_SPEED_DEFAULT          ( eHDD_LINK_SPEED_REPORT_MAX_SCALED )

/*
 * RSSI Thresholds
 * Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 */
#define CFG_LINK_SPEED_RSSI_HIGH                   "gLinkSpeedRssiHigh"
#define CFG_LINK_SPEED_RSSI_HIGH_MIN               ( -127 )
#define CFG_LINK_SPEED_RSSI_HIGH_MAX               (  0 )
#define CFG_LINK_SPEED_RSSI_HIGH_DEFAULT           ( -55 )

#define CFG_LINK_SPEED_RSSI_MID                    "gLinkSpeedRssiMed"
#define CFG_LINK_SPEED_RSSI_MID_MIN                ( -127 )
#define CFG_LINK_SPEED_RSSI_MID_MAX                (  0 )
#define CFG_LINK_SPEED_RSSI_MID_DEFAULT            ( -65 )

#define CFG_LINK_SPEED_RSSI_LOW                    "gLinkSpeedRssiLow"
#define CFG_LINK_SPEED_RSSI_LOW_MIN                ( -127 )
#define CFG_LINK_SPEED_RSSI_LOW_MAX                (  0 )
#define CFG_LINK_SPEED_RSSI_LOW_DEFAULT            ( -80 )

#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_NAME                "isP2pDeviceAddrAdministrated"
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MIN                 ( 0 )
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MAX                 ( 1 )
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_DEFAULT             ( 1 )


#define CFG_ENABLE_SSR                      "gEnableSSR"
#define CFG_ENABLE_SSR_MIN                  ( 0 )
#define CFG_ENABLE_SSR_MAX                  ( 1 )
#define CFG_ENABLE_SSR_DEFAULT              ( 1 )

#define CFG_BTC_ACTIVE_WLAN_LEN_NAME           "btcActiveWlanLen"
#define CFG_BTC_ACTIVE_WLAN_LEN_MIN            ( 0 )
#define CFG_BTC_ACTIVE_WLAN_LEN_MAX            ( 250000 )
#define CFG_BTC_ACTIVE_WLAN_LEN_DEFAULT        ( 60000 )

#define CFG_BTC_ACTIVE_BT_LEN_NAME             "btcActiveBtLen"
#define CFG_BTC_ACTIVE_BT_LEN_MIN              ( 0 )
#define CFG_BTC_ACTIVE_BT_LEN_MAX              ( 250000 )
#define CFG_BTC_ACTIVE_BT_LEN_DEFAULT          ( 90000 )

#define CFG_BTC_SAP_ACTIVE_WLAN_LEN_NAME       "btcSapActiveWlanLen"
#define CFG_BTC_SAP_ACTIVE_WLAN_LEN_MIN        ( 0 )
#define CFG_BTC_SAP_ACTIVE_WLAN_LEN_MAX        ( 250000 )
#define CFG_BTC_SAP_ACTIVE_WLAN_LEN_DEFAULT    ( 30000 )

#define CFG_BTC_SAP_ACTIVE_BT_LEN_NAME         "btcSapActiveBtLen"
#define CFG_BTC_SAP_ACTIVE_BT_LEN_MIN          ( 0 )
#define CFG_BTC_SAP_ACTIVE_BT_LEN_MAX          ( 250000 )
#define CFG_BTC_SAP_ACTIVE_BT_LEN_DEFAULT      ( 30000 )

#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_NAME                "gObssScanActiveDwellTime"
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_MIN                 ( 5 )
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_MAX                 ( 1000 )
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_DEFAULT             ( 10 )

#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_NAME                "gObssScanPassiveDwellTime"
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_MIN                 ( 10 )
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_MAX                 ( 1000 )
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_DEFAULT             ( 20 )

#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_NAME            "gObssScanWidthTriggerInterval"
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_MIN             ( 10 )
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_MAX             ( 900 )
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_DEFAULT         ( 200 )

/* In cfg.dat 1=1MBPS, 2=2MBPS, 3=5_5MBPS, 4=11MBPS, 5=6MBPS, 6=9MBPS,
 * 7=12MBPS, 8=18MBPS, 9=24MBPS. But 6=9MBPS and 8=18MBPS are not basic
 * 11g rates and should not be set by gDefaultRateIndex24Ghz. So instead
 * of using index 1-9 we will use 1-7 and if user set gDefaultRateIndex24Ghz=6
 * set 7=12MBPS in CFG and if user set gDefaultRateIndex24Ghz=7 set
 * 9=24MBPS in CFG.
*/

#define HDD_DEFAULT_RATE_12MBPS                   6
#define HDD_DEFAULT_RATE_24MBPS                   7
#define CFG_DEFAULT_RATE_12MBPS                   7
#define CFG_DEFAULT_RATE_24MBPS                   9
#define CFG_DEFAULT_RATE_INDEX_24GH               "gDefaultRateIndex24Ghz"
#define CFG_DEFAULT_RATE_INDEX_24GH_MIN           ( 1 )
#define CFG_DEFAULT_RATE_INDEX_24GH_MAX           ( 7 )
#define CFG_DEFAULT_RATE_INDEX_24GH_DEFAULT       ( 1 )

static __inline tANI_U32 defHddRateToDefCfgRate( tANI_U32 defRateIndex )
{
    switch(defRateIndex){
       case HDD_DEFAULT_RATE_12MBPS:
            return CFG_DEFAULT_RATE_12MBPS;
            break;
       case HDD_DEFAULT_RATE_24MBPS:
            return CFG_DEFAULT_RATE_24MBPS;
            break;
       default:
            return defRateIndex;
    }
}
/*
 * VOS Trace Enable Control
 * Notes:
 *  the MIN/MAX/DEFAULT values apply for all modules
 *  the DEFAULT value is outside the valid range.  if the DEFAULT
 *    value is not overridden, then no change will be made to the
 *    "built in" default values compiled into the code
 *  values are a bitmap indicating which log levels are to enabled
 *    (must match order of vos_trace_level enumerations)
 *    00000001  FATAL
 *    00000010  ERROR
 *    00000100  WARN
 *    00001000  INFO
 *    00010000  INFO HIGH
 *    00100000  INFO MED
 *    01000000  INFO LOW
 *    10000000  DEBUG
 *
 *  hence a value of 0xFF would set all bits (enable all logs)
 */

#define CFG_VOS_TRACE_ENABLE_BAP_NAME     "vosTraceEnableBAP"
#define CFG_VOS_TRACE_ENABLE_TL_NAME      "vosTraceEnableTL"
#define CFG_VOS_TRACE_ENABLE_WDI_NAME     "vosTraceEnableWDI"
#define CFG_VOS_TRACE_ENABLE_HDD_NAME     "vosTraceEnableHDD"
#define CFG_VOS_TRACE_ENABLE_SME_NAME     "vosTraceEnableSME"
#define CFG_VOS_TRACE_ENABLE_PE_NAME      "vosTraceEnablePE"
#define CFG_VOS_TRACE_ENABLE_PMC_NAME     "vosTraceEnablePMC"
#define CFG_VOS_TRACE_ENABLE_WDA_NAME     "vosTraceEnableWDA"
#define CFG_VOS_TRACE_ENABLE_SYS_NAME     "vosTraceEnableSYS"
#define CFG_VOS_TRACE_ENABLE_VOSS_NAME    "vosTraceEnableVOSS"
#define CFG_VOS_TRACE_ENABLE_SAP_NAME     "vosTraceEnableSAP"
#define CFG_VOS_TRACE_ENABLE_HDD_SAP_NAME "vosTraceEnableHDDSAP"

#define CFG_VOS_TRACE_ENABLE_MIN          (0)
#define CFG_VOS_TRACE_ENABLE_MAX          (0xff)
#define CFG_VOS_TRACE_ENABLE_DEFAULT      (0xffff)

/*
 * WDI Trace Enable Control
 * Notes:
 *  the MIN/MAX/DEFAULT values apply for all modules
 *  the DEFAULT value is outside the valid range.  if the DEFAULT
 *    value is not overridden, then no change will be made to the
 *    "built in" default values compiled into the code
 *  values are a bitmap indicating which log levels are to enabled
 *    (must match order of wpt_tracelevel enumerations)
 *    00000001  FATAL
 *    00000010  ERROR
 *    00000100  WARN
 *    00001000  INFO
 *    00010000  INFO HIGH
 *    00100000  INFO MED
 *    01000000  INFO LOW
 *
 *  hence a value of 0x7F would set all bits (enable all logs)
 */
#define CFG_WDI_TRACE_ENABLE_DAL_NAME     "wdiTraceEnableDAL"
#define CFG_WDI_TRACE_ENABLE_CTL_NAME     "wdiTraceEnableCTL"
#define CFG_WDI_TRACE_ENABLE_DAT_NAME     "wdiTraceEnableDAT"
#define CFG_WDI_TRACE_ENABLE_PAL_NAME     "wdiTraceEnablePAL"
#define CFG_WDI_TRACE_ENABLE_MIN          (0)
#define CFG_WDI_TRACE_ENABLE_MAX          (0x7f)
#define CFG_WDI_TRACE_ENABLE_DEFAULT      (0xffffffff)

#define HDD_MCASTBCASTFILTER_FILTER_NONE                       0x00
#define HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST              0x01
#define HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST              0x02
#define HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST    0x03
#define HDD_MULTICAST_FILTER_LIST                              0x04

/*
 *
 * SAP Auto Channel Enable
 * Notes:
 * Auto Channel selection for SAP configuration
 * 0 - Disable Auto Channel
 * 1 - Enable auto channel selection in auto mode.
 *     When enable auto channel, channel provided by Supplicant will be ignored.
 *
 * Default configuration: Auto channel is disabled.
 */

#define CFG_SAP_AUTO_CHANNEL_SELECTION_NAME       "gApAutoChannelSelection"

#define CFG_SAP_AUTO_CHANNEL_SELECTION_MIN        ( 0 )
#define CFG_SAP_AUTO_CHANNEL_SELECTION_MAX        ( 1 )
#define CFG_SAP_AUTO_CHANNEL_SELECTION_DEFAULT    ( 0 )

/* ACS Scan band preference
 * 0 -- No preference
 * 1 -- Scan 2.4G first
 * 2 -- Scan 5G first
*/
#define CFG_SAP_SCAN_BAND_PREFERENCE              "gAcsScanBandPreference"
#define CFG_SAP_SCAN_BAND_PREFERENCE_MIN          (0)
#define CFG_SAP_SCAN_BAND_PREFERENCE_MAX          (2)
#define CFG_SAP_SCAN_BAND_PREFERENCE_DEFAULT      (0)
#define CFG_ACS_BAND_SWITCH_THRESHOLD             "gACSBandSwitchThreshold"
#define CFG_ACS_BAND_SWITCH_THRESHOLD_MIN         (0)
#define CFG_ACS_BAND_SWITCH_THRESHOLD_MAX         (4444)
/* 2 BSS, maximus RSSI -90 */
#define CFG_ACS_BAND_SWITCH_THRESHOLD_DEFAULT     (296)

/*BMPS Logic
 * Notes:
 * 1 - Then Host driver and above layers control the PS mechanism
 * 0 - Diver/Core Stack internally control the Power saving mechanism
 */
#define CFG_ANDRIOD_POWER_SAVE_NAME      "isAndroidPsEn"
#define CFG_ANDRIOD_POWER_SAVE_MIN       ( 0 )
#define CFG_ANDRIOD_POWER_SAVE_MAX       ( 1 )
#define CFG_ANDRIOD_POWER_SAVE_DEFAULT   ( 0 )


/*
 * Enable Dynamic DTIM
 * Options
 * 0 -Disable DynamicDTIM
 * 1 to 5 - SLM will switch to DTIM specified here when host suspends and 
 *          switch DTIM1 when host resumes */
#define CFG_ENABLE_DYNAMIC_DTIM_NAME            "gEnableDynamicDTIM"
#define CFG_ENABLE_DYNAMIC_DTIM_MIN        ( 0 )
#define CFG_ENABLE_DYNAMIC_DTIM_MAX        ( 5 )
#define CFG_ENABLE_DYNAMIC_DTIM_DEFAULT    ( 0 )

/*
 * Enable First Scan 2G Only
 * Options
 * 0 - Disable First Scan 2G Option
 * 1 - Enable First Scan 2G Option
 */
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_NAME            "gEnableFirstScan2GOnly"
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_MIN        ( 0 )
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_MAX        ( 1 )
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_DEFAULT    ( 0 )

/*
 * Skip DFS Channel in case of P2P Search
 * Options
 * 0 - Don't Skip DFS Channel in case of P2P Search
 * 1 - Skip DFS Channel in case of P2P Search
 */
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_NAME       "gSkipDfsChannelInP2pSearch"
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_MIN        ( 0 )
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_MAX        ( 1 )
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_DEFAULT    ( 1 )

/*
 * Ignore Dynamic Dtim in case of P2P
 * Options
 * 0 - Consider Dynamic Dtim incase of P2P
 * 1 - Ignore Dynamic Dtim incase of P2P
 */
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_NAME       "gIgnoreDynamicDtimInP2pMode"
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_MIN        ( 0 )
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_MAX        ( 1 )
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_DEFAULT    ( 0 )


#define CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_NAME  "gEnableAutomaticTxPowerControl"
#define CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_MIN        ( 0 )
#define CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_MAX        ( 1 )
#define CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_DEFAULT    ( 1 )

#define CFG_SHORT_GI_40MHZ_NAME                "gShortGI40Mhz"
#define CFG_SHORT_GI_40MHZ_MIN                 0
#define CFG_SHORT_GI_40MHZ_MAX                 1
#define CFG_SHORT_GI_40MHZ_DEFAULT             1

/*
 * Enable / Disable MCC feature
 * Default: Enable
 */
#define CFG_ENABLE_MCC_ENABLED_NAME             "gEnableMCCMode"
#define CFG_ENABLE_MCC_ENABLED_MIN              ( 0 )
#define CFG_ENABLE_MCC_ENABLED_MAX              ( 1 )
#define CFG_ENABLE_MCC_ENABLED_DEFAULT          ( 1 )

/*
 * Allow GO in MCC mode to accept different beacon interval than STA's.
 * Added for Wi-Fi Cert. 5.1.12
 * Default: gAllowMCCGODiffBI = 2
 * If gAllowMCCGODiffBI = 1  // Set to 1 for WFA certification. GO Beacon
 *                              interval is not changed. MCC GO
 *                              doesn't work well in optimized way.
 *                              In worst scenario, it may invite STA
 *                              disconnection.
 * gAllowMCCGODiffBI = 2     //If set to 2 workaround 1 disassoc all the clients
 *                             and update beacon Interval
 * gAllowMCCGODiffBI = 3     //If set to 3 tear down the P2P link in auto/ 
 *                             Non-autonomous -GO case
 * gAllowMCCGODiffBI = 4     //If set to 4 don't disconnect the P2P client
 *                             in autonomous/Non-autonomous -GO case update
 *                             the BI dynamically
 */
#define CFG_ALLOW_MCC_GO_DIFF_BI_NAME           "gAllowMCCGODiffBI"
#define CFG_ALLOW_MCC_GO_DIFF_BI_MIN            ( 0 )
#define CFG_ALLOW_MCC_GO_DIFF_BI_MAX            ( 4 )
#define CFG_ALLOW_MCC_GO_DIFF_BI_DEFAULT        ( 4 )

/*
 * Enable/Disable Thermal Mitigation feature
 * Default: Disable
 */
#define CFG_THERMAL_MIGRATION_ENABLE_NAME      "gThermalMitigationEnable"   
#define CFG_THERMAL_MIGRATION_ENABLE_MIN       ( 0 ) 
#define CFG_THERMAL_MIGRATION_ENABLE_MAX       ( 1 ) 
#define CFG_THERMAL_MIGRATION_ENABLE_DEFAULT   ( 0 ) 

/*
 * Enable/Disable Modulated DTIM feature
 * Default: Disable
 */
#define CFG_ENABLE_MODULATED_DTIM_NAME       "gEnableModulatedDTIM"
#define CFG_ENABLE_MODULATED_DTIM_MIN        ( 0 )
#define CFG_ENABLE_MODULATED_DTIM_MAX        ( 5 )
#define CFG_ENABLE_MODULATED_DTIM_DEFAULT    ( 0 )

/*
 * Enable/Disable Multicast MAC Address List feature
 * Default: Disable
 */
#define CFG_MC_ADDR_LIST_ENABLE_NAME          "gMCAddrListEnable"
#define CFG_MC_ADDR_LIST_ENABLE_MIN           ( 0 )
#define CFG_MC_ADDR_LIST_ENABLE_MAX           ( 1 )
#define CFG_MC_ADDR_LIST_ENABLE_DEFAULT       ( 0 )

/* Set number of buffers to be advertised during ADDBA negotiation*/
#define CFG_NUM_BUFF_ADVERT_NAME              "gNumBuffAdvert"
#define CFG_NUM_BUFF_ADVERT_MIN               ( 0 )
#define CFG_NUM_BUFF_ADVERT_MAX               ( 128 )
#define CFG_NUM_BUFF_ADVERT_DEFAULT           ( 64 )

/*
 * Allow MCC to modify config
 */
#define CFG_MCC_CONFIG_PARAM_NAME          "gMccAllowCfgModify"
#define CFG_MCC_CONFIG_PARAM_MIN           ( 0x0000 )
#define CFG_MCC_CONFIG_PARAM_MAX           ( 0x01ff )
#define CFG_MCC_CONFIG_PARAM_DEFAULT       ( 0x000C )

#define CFG_ENABLE_RX_STBC                       "gEnableRXSTBC"
#define CFG_ENABLE_RX_STBC_MIN                   ( 0 )
#define CFG_ENABLE_RX_STBC_MAX                   ( 1 )
#define CFG_ENABLE_RX_STBC_DEFAULT               ( 1 )

/* 
 * Enable/Disable vsta based on MAX Assoc limit 
 * defined in WCNSS_qcom_cfg.ini.
 */
#ifdef WLAN_SOFTAP_VSTA_FEATURE
#define CFG_VSTA_SUPPORT_ENABLE               "gEnableVSTASupport"
#define CFG_VSTA_SUPPORT_ENABLE_MIN           ( 0 )
#define CFG_VSTA_SUPPORT_ENABLE_MAX           ( 1 )
#define CFG_VSTA_SUPPORT_ENABLE_DEFAULT       ( 0 )
#endif

#ifdef FEATURE_WLAN_TDLS
#define CFG_TDLS_SUPPORT_ENABLE                     "gEnableTDLSSupport"
#define CFG_TDLS_SUPPORT_ENABLE_MIN                 ( 0 )
#define CFG_TDLS_SUPPORT_ENABLE_MAX                 ( 1 )
#define CFG_TDLS_SUPPORT_ENABLE_DEFAULT             ( 0 )

#define CFG_TDLS_IMPLICIT_TRIGGER                   "gEnableTDLSImplicitTrigger"
#define CFG_TDLS_IMPLICIT_TRIGGER_MIN               ( 0 )
#define CFG_TDLS_IMPLICIT_TRIGGER_MAX               ( 1 )
#define CFG_TDLS_IMPLICIT_TRIGGER_DEFAULT           ( 0 )

#define CFG_TDLS_TX_STATS_PERIOD                    "gTDLSTxStatsPeriod"
#define CFG_TDLS_TX_STATS_PERIOD_MIN                ( 10 )
#define CFG_TDLS_TX_STATS_PERIOD_MAX                ( 4294967295UL )
#define CFG_TDLS_TX_STATS_PERIOD_DEFAULT            ( 5000 )

#define CFG_TDLS_TX_PACKET_THRESHOLD                "gTDLSTxPacketThreshold"
#define CFG_TDLS_TX_PACKET_THRESHOLD_MIN            ( 0 )
#define CFG_TDLS_TX_PACKET_THRESHOLD_MAX            ( 4294967295UL )
#define CFG_TDLS_TX_PACKET_THRESHOLD_DEFAULT        ( 100 )

#define CFG_TDLS_DISCOVERY_PERIOD                   "gTDLSDiscoveryPeriod"
#define CFG_TDLS_DISCOVERY_PERIOD_MIN               ( 5000 )
#define CFG_TDLS_DISCOVERY_PERIOD_MAX               ( 4294967295UL )
#define CFG_TDLS_DISCOVERY_PERIOD_DEFAULT           ( 20000 )

#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT              "gTDLSMaxDiscoveryAttempt"
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MIN          ( 1 )
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MAX          ( 100 )
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT_DEFAULT      ( 3 )

#define CFG_TDLS_IDLE_TIMEOUT                       "gTDLSIdleTimeout"
#define CFG_TDLS_IDLE_TIMEOUT_MIN                   ( 2000 )
#define CFG_TDLS_IDLE_TIMEOUT_MAX                   ( 40000 )
#define CFG_TDLS_IDLE_TIMEOUT_DEFAULT               ( 5000 )

#define CFG_TDLS_IDLE_PACKET_THRESHOLD              "gTDLSIdlePacketThreshold"
#define CFG_TDLS_IDLE_PACKET_THRESHOLD_MIN          ( 0 )
#define CFG_TDLS_IDLE_PACKET_THRESHOLD_MAX          ( 40000 )
#define CFG_TDLS_IDLE_PACKET_THRESHOLD_DEFAULT      ( 5 )

#define CFG_TDLS_RSSI_HYSTERESIS                    "gTDLSRssiHysteresis"
#define CFG_TDLS_RSSI_HYSTERESIS_MIN                ( 0 )
#define CFG_TDLS_RSSI_HYSTERESIS_MAX                ( 100 )
#define CFG_TDLS_RSSI_HYSTERESIS_DEFAULT            ( 100 )

#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD             "gTDLSRSSITriggerThreshold"
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MIN         ( -120 )
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MAX         ( 0 )
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD_DEFAULT     ( -75 )

#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD            "gTDLSRSSITeardownThreshold"
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MIN        ( -120 )
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MAX        ( 0 )
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_DEFAULT    ( -75 )

#define CFG_TDLS_QOS_WMM_UAPSD_MASK_NAME            "gTDLSUapsdMask" // ACs to setup U-APSD for TDLS Sta
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_MIN             (0)
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_MAX             (15)
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_DEFAULT         (0)

#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE          "gEnableTDLSBufferSta"
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_MIN      (0)
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_MAX      (1)
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_DEFAULT  (1)

#define CFG_TDLS_PUAPSD_INACTIVITY_TIME             "gTDLSPuapsdInactivityTime"
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME_MIN         (0)
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME_MAX         (10)
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME_DEFAULT     (0)

#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD          "gTDLSPuapsdRxFrameThreshold"
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_MIN      (10)
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_MAX      (20)
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_DEFAULT  (10)

#define CFG_TDLS_EXTERNAL_CONTROL                   "gTDLSExternalControl"
#define CFG_TDLS_EXTERNAL_CONTROL_MIN               (0)
#define CFG_TDLS_EXTERNAL_CONTROL_MAX               (1)
#define CFG_TDLS_EXTERNAL_CONTROL_DEFAULT           (0)

#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE          "gEnableTDLSOffChannel"
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_MIN      (0)
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_MAX      (1)
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_DEFAULT  (0)

#define CFG_TDLS_WMM_MODE_ENABLE                     "gEnableTDLSWmmMode"
#define CFG_TDLS_WMM_MODE_ENABLE_MIN                 (0)
#define CFG_TDLS_WMM_MODE_ENABLE_MAX                 (1)
#define CFG_TDLS_WMM_MODE_ENABLE_DEFAULT             (0)

#define CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE            "gEnableTDLSScanCoexistence"
#define CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE_MIN        (0)
#define CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE_MAX        (1)
#define CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE_DEFAULT    (0)
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
#define CFG_LINK_LAYER_STATS_ENABLE                  "gEnableLLStats"
#define CFG_LINK_LAYER_STATS_ENABLE_MIN              (0)
#define CFG_LINK_LAYER_STATS_ENABLE_MAX              (1)
#define CFG_LINK_LAYER_STATS_ENABLE_DEFAULT          (0)
#endif

#ifdef WLAN_FEATURE_EXTSCAN
#define CFG_EXTSCAN_ENABLE                  "gEnableEXTScan"
#define CFG_EXTSCAN_ENABLE_MIN              (0)
#define CFG_EXTSCAN_ENABLE_MAX              (1)
#define CFG_EXTSCAN_ENABLE_DEFAULT          (0)
#endif

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
#define CFG_ACTIVEMODE_OFFLOAD_ENABLE         "gEnableActiveModeOffload"
#define CFG_ACTIVEMODE_OFFLOAD_ENABLE_MIN     ( 0 )
#define CFG_ACTIVEMODE_OFFLOAD_ENABLE_MAX     ( 1 )
#define CFG_ACTIVEMODE_OFFLOAD_ENABLE_DEFAULT ( 1 )
#endif

/* Enable/Disable LPWR Image(cMEM uBSP) Transition */
#define CFG_ENABLE_LPWR_IMG_TRANSITION_NAME        "gEnableLpwrImgTransition"
#define CFG_ENABLE_LPWR_IMG_TRANSITION_MIN         ( 0 )
#define CFG_ENABLE_LPWR_IMG_TRANSITION_MAX         ( 1 )
#define CFG_ENABLE_LPWR_IMG_TRANSITION_DEFAULT     ( 1 )


/*
 * Scan Aging timeout value in seconds
 */
#define CFG_SCAN_AGING_PARAM_NAME          "gScanAgingTime"
#define CFG_SCAN_AGING_PARAM_MIN           ( 0 )
#define CFG_SCAN_AGING_PARAM_MAX           ( 200 )
#define CFG_SCAN_AGING_PARAM_DEFAULT       ( 60 )

/* Config Param to enable the txLdpc capability 
 * 0 - disable 
 * 1 - HT LDPC enable
 * 2 - VHT LDPC enable
 * 3 - HT & VHT LDPC enable */
#define CFG_TX_LDPC_ENABLE_FEATURE         "gTxLdpcEnable"
#define CFG_TX_LDPC_ENABLE_FEATURE_MIN     ( 0 )
#define CFG_TX_LDPC_ENABLE_FEATURE_MAX     ( 3 )
#define CFG_TX_LDPC_ENABLE_FEATURE_DEFAULT ( 0 )

/*
 * Enable / Disable MCC Adaptive Scheduler feature
 * Default: Enable
 */
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_NAME             "gEnableMCCAdaptiveScheduler"      
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_MIN              ( 0 )
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_MAX              ( 1 )
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_DEFAULT          ( 1 )

#ifdef WLAN_FEATURE_11AC
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE         "gTxBFEnable"
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_MIN     ( WNI_CFG_VHT_SU_BEAMFORMEE_CAP_STAMIN )
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_MAX     ( WNI_CFG_VHT_SU_BEAMFORMEE_CAP_STAMAX )
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_DEFAULT ( WNI_CFG_VHT_SU_BEAMFORMEE_CAP_STADEF )

#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED         "gTxBFCsnValue"
#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_MIN     ( WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_STAMIN )
#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_MAX     ( WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_STAMAX - 1 )
#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_DEFAULT ( WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_STAMAX - 1 )

#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE         "gEnableMuBformee"
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_MIN     ( 0 )
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_MAX     ( 1 )
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_DEFAULT ( 1 )

#define CFG_VHT_AMPDU_LEN_EXP_NAME          "gVhtMaxAmpduLenExp"
#define CFG_VHT_AMPDU_LEN_EXP_MIN           ( 0 )
#define CFG_VHT_AMPDU_LEN_EXP_MAX           ( 7 )
#define CFG_VHT_AMPDU_LEN_EXP_DEFAULT       ( 3 )

#endif


/*
 * SAP ALLOW All Channels
 */
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_NAME          "gSapAllowAllChannel"
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_MIN           ( 0 )
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_MAX           ( 1 )
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_DEFAULT       ( 0 )


#ifdef WLAN_FEATURE_11AC
#define CFG_DISABLE_LDPC_WITH_TXBF_AP             "gDisableLDPCWithTxbfAP"
#define CFG_DISABLE_LDPC_WITH_TXBF_AP_MIN         ( 0 )
#define CFG_DISABLE_LDPC_WITH_TXBF_AP_MAX         ( 1 )
#define CFG_DISABLE_LDPC_WITH_TXBF_AP_DEFAULT     ( 0 )
#endif

#define CFG_LIST_OF_NON_DFS_COUNTRY_CODE                    "gListOfNonDfsCountryCode"
#define CFG_LIST_OF_NON_DFS_COUNTRY_CODE_DEFAULT            "JO,MA"

/*
 * IBSS Operating Channels for 2.4G and 5GHz channels
 */
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_NAME          "gAdHocChannel5G"
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_MIN           ( 36 )
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_MAX           ( 165 )
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_DEFAULT       ( 44 )

#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_NAME         "gAdHocChannel24G"
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_MIN          ( 1 )
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_MAX          ( 14 )
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_DEFAULT      ( 6 )

#define CFG_LIST_OF_NON_11AC_COUNTRY_CODE           "gListOfNon11acCountryCode"
#define CFG_LIST_OF_NON_11AC_COUNTRY_CODE_DEFAULT   "RU,UA,ZA"

/* Parameter to control VHT support in 2.4 GHz band */
#define CFG_ENABLE_VHT_FOR_24GHZ_NAME             "gEnableVhtFor24GHzBand"
#define CFG_ENABLE_VHT_FOR_24GHZ_MIN              (0)
#define CFG_ENABLE_VHT_FOR_24GHZ_MAX              (1)
#define CFG_ENABLE_VHT_FOR_24GHZ_DEFAULT          (0)


#define CFG_MAX_MEDIUM_TIME                      "gMaxMediumTime"
#define CFG_MAX_MEDIUM_TIME_STAMIN               WNI_CFG_MAX_MEDIUM_TIME_STAMIN
#define CFG_MAX_MEDIUM_TIME_STAMAX               WNI_CFG_MAX_MEDIUM_TIME_STAMAX
#define CFG_MAX_MEDIUM_TIME_STADEFAULT           WNI_CFG_MAX_MEDIUM_TIME_STADEF

/*
 * SAP/P2P-GO mode traffic monitor
 */
#define CFG_ENABLE_TRAFFIC_MONITOR                "gEnableTrafficMonitor"
#define CFG_ENABLE_TRAFFIC_MONITOR_MIN            ( 0 )
#define CFG_ENABLE_TRAFFIC_MONITOR_MAX            ( 1 )
#define CFG_ENABLE_TRAFFIC_MONITOR_DEFAULT        ( 0 )

#define CFG_TRAFFIC_IDLE_TIMEOUT                  "gTrafficIdleTimeout"
#define CFG_TRAFFIC_IDLE_TIMEOUT_MIN              ( 3000 )
#define CFG_TRAFFIC_IDLE_TIMEOUT_MAX              ( 10000 )
#define CFG_TRAFFIC_IDLE_TIMEOUT_DEFAULT          ( 5000 )

/*
 * SCAN Offload
 */
#define CFG_SCAN_OFFLOAD_NAME                     "gEnableDirectedScanOffload"
#define CFG_SCAN_OFFLOAD_DISABLE                  ( 0 )
#define CFG_SCAN_OFFLOAD_ENABLE                   ( 1 )
#define CFG_SCAN_OFFLOAD_DEFAULT                  ( CFG_SCAN_OFFLOAD_DISABLE )

/*
 * Enable legacy fast roaming (LFR) on STA link during concurrent sessions
 */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY          "gEnableFastRoamInConcurrency"
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_MIN      ( 0 )
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_MAX      ( 1 )
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_DEFAULT  ( 1 )
#endif

/*
 * FlexConnectPowerFactor parameter
 * Default: Disable (0)
 */
#define CFG_FLEX_CONNECT_POWER_FACTOR_NAME          "gFlexConnectPowerFactor"
#define CFG_FLEX_CONNECT_POWER_FACTOR_MIN           ( 0 )
#define CFG_FLEX_CONNECT_POWER_FACTOR_MAX           ( 9 )
#define CFG_FLEX_CONNECT_POWER_FACTOR_DEFAULT       ( 0 )

/*
 * Enable heart beat monitoring offload to FW
 */
#define CFG_ENABLE_HEART_BEAT_OFFLOAD          "gEnableIbssHeartBeatOffload"
#define CFG_ENABLE_HEART_BEAT_OFFLOAD_MIN      ( 0 )
#define CFG_ENABLE_HEART_BEAT_OFFLOAD_MAX      ( 1 )
#define CFG_ENABLE_HEART_BEAT_OFFLOAD_DEFAULT  ( 1 )

#define CFG_ANTENNA_DIVERSITY_PARAM_NAME          "gAntennaDiversity"
#define CFG_ANTENNA_DIVERSITY_PARAM_MIN           ( 0 )
#define CFG_ANTENNA_DIVERSITY_PARAM_MAX           ( 3 )
#define CFG_ANTENNA_DIVERSITY_PARAM_DEFAULT       ( 0 )

#define CFG_ENABLE_SNR_MONITORING_NAME              "gEnableSNRMonitoring"
#define CFG_ENABLE_SNR_MONITORING_MIN               ( 0 )
#define CFG_ENABLE_SNR_MONITORING_MAX               ( 1 )
#define CFG_ENABLE_SNR_MONITORING_DEFAULT           ( 0 )
//Macro to enable/disable dynamic timer
#define CFG_DYNAMIC_SPLIT_SCAN_NAME                    "gEnableDynSplitScan"
#define CFG_DYNAMIC_SPLIT_SCAN_MIN                     ( 0 )
#define CFG_DYNAMIC_SPLIT_SCAN_MAX                     ( 1 )
#define CFG_DYNAMIC_SPLIT_SCAN_DEFAULT                 ( 1 )

//Macro to monitor the packet count
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_NAME    "gSplitScanTxRxThreshold"
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_MIN     ( 10 )
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_MAX     ( 100 )
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_DEFAULT ( 50 )

//Macro to handle the monitor timer value in milliseconds
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_NAME      "gSplitScanTxRxTimer"
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_MIN       ( 1000 )
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_MAX       ( 10000 )
#define CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_DEFAULT   ( 5000 )

#ifdef FEATURE_WLAN_SCAN_PNO
#define CFG_PNO_SCAN_SUPPORT                         "gPNOScanSupport"
#define CFG_PNO_SCAN_SUPPORT_ENABLE                  ( 1 )
#define CFG_PNO_SCAN_SUPPORT_DISABLE                 ( 0 )
#define CFG_PNO_SCAN_SUPPORT_DEFAULT                 ( 1 )

#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE              "gPNOScanTimerRepeatValue"
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE_DEFAULT      ( 6 )
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE_MIN          ( 0 )
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE_MAX          ( 0xffffffff )
#endif

/* Not supporting this feature in pronto */
#define CFG_AMSDU_SUPPORT_IN_AMPDU_NAME                "gAmsduSupportInAMPDU"
#define CFG_AMSDU_SUPPORT_IN_AMPDU_MIN                 (0)
#define CFG_AMSDU_SUPPORT_IN_AMPDU_MAX                 (0)
#define CFG_AMSDU_SUPPORT_IN_AMPDU_DEFAULT             (0) //disabled

/* Prefer connecting to 5G AP even if its RSSI is lower by
 gSelect5GHzMargin dBm than 2.4G AP.
This feature requires the dependent cfg.ini "gRoamPrefer5GHz" set to 1 */
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN                 "gSelect5GHzMargin"
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN_MIN             (0)
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN_MAX             (60)
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN_DEFAULT         (0) //set 0 to disable

#define CFG_COALESING_IN_IBSS_NAME                "gCoalesingInIBSS"
#define CFG_COALESING_IN_IBSS_MIN                 (0)
#define CFG_COALESING_IN_IBSS_MAX                 (1)
#define CFG_COALESING_IN_IBSS_DEFAULT             (0) //disabled

#define CFG_DISABLE_ATH_NAME                       "gAthDisable"
#define CFG_DISABLE_ATH_MIN                        (0)
#define CFG_DISABLE_ATH_MAX                        (1)
#define CFG_DISABLE_ATH_DEFAULT                    (0)

//Enable Memory Debug
#ifdef MEMORY_DEBUG
#define CFG_ENABLE_MEMORY_DEBUG_NAME             "gEnableMemoryDebug"
#define CFG_ENABLE_MEMORY_DEBUG_MIN              (0)
#define CFG_ENABLE_MEMORY_DEBUG_MAX              (1)
#define CFG_ENABLE_MEMORY_DEBUG_DEFAULT          (0)
#endif

#define CFG_OVERRIDE_COUNTRY_CODE                "gStaCountryCode"
#define CFG_OVERRIDE_COUNTRY_CODE_DEFAULT        "000"

/*Once the period expires the RSSI will be compared with Threshold
 * and if needed trigger events will be raised
 */
#define CFG_ASD_PROBE_INTERVAL_NAME                     "gAsdProbeInterval"
#define CFG_ASD_PROBE_INTERVAL_DEFAULT                  (50)
#define CFG_ASD_PROBE_INTERVAL_MIN                      (10)
#define CFG_ASD_PROBE_INTERVAL_MAX                      (100)

/* RSSI Threshold used to trigger probing activity/selection process*/
#define CFG_ASD_TRIGGER_THRESHOLD_NAME                  "gAsdTriggerThreshold"
#define CFG_ASD_TRIGGER_THRESHOLD_DEFAULT               (-75)
#define CFG_ASD_TRIGGER_THRESHOLD_MIN                   (-120)
#define CFG_ASD_TRIGGER_THRESHOLD_MAX                   (0)

/*RSSI Hysteresis Threshold for RSSI-RTT*/
#define CFG_ASD_RTT_RSSI_HYST_THRESHOLD_NAME             "gAsdRTTRssiHystThreshold"
#define CFG_ASD_RTT_RSSI_HYST_THRESHOLD_DEFAULT          (50)
#define CFG_ASD_RTT_RSSI_HYST_THRESHOLD_MIN              (0)
#define CFG_ASD_RTT_RSSI_HYST_THRESHOLD_MAX              (100)

//Enable debug for remain on channel issues
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_NAME    "gDebugP2pRemainOnChannel"
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_DEFAULT ( 0 )
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_MIN     ( 0 )
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_MAX     ( 1 )

#define CFG_CTS2S_DURING_BTC_SCO_NAME           "btcCTS2SduringSCO"
#define CFG_CTS2S_DURING_BTC_SCO_DEFAULT        WNI_CFG_BTC_CTS2S_DURING_SCO_STADEF
#define CFG_CTS2S_DURING_BTC_SCO_MIN            WNI_CFG_BTC_CTS2S_DURING_SCO_STAMIN
#define CFG_CTS2S_DURING_BTC_SCO_MAX            WNI_CFG_BTC_CTS2S_DURING_SCO_STAMAX


/*
 * Connection related log Enable/Disable.
 * 0x1 - Enable mgmt pkt logs (no probe req/rsp).
 * 0x2 - Enable EAPOL pkt logs.
 * 0x4 - Enable DHCP pkt logs.
 * 0x0 - Disable all the above connection related logs.
 */
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE             "gEnableDebugLog"
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE_MIN         (0)
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE_MAX         (0xFF)
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE_DEFAULT     (0)

#define CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_NAME                "gEnableStrictRegulatoryForFCC"
#define CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_MIN                 ( 0 )
#define CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_MAX                 ( 1 )
#define CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_DEFAULT             ( 0 )

#define CFG_ADVERTISE_CONCURRENT_OPERATION_NAME    "gAdvertiseConcurrentOperation"
#define CFG_ADVERTISE_CONCURRENT_OPERATION_DEFAULT ( 1 )
#define CFG_ADVERTISE_CONCURRENT_OPERATION_MIN     ( 0 )
#define CFG_ADVERTISE_CONCURRENT_OPERATION_MAX     ( 1 )

#define CFG_RA_FILTER_ENABLE_NAME               "gRAFilterEnable"
#define CFG_RA_FILTER_ENABLE_DEFAULT            (0)
#define CFG_RA_FILTER_ENABLE_MIN                (0)
#define CFG_RA_FILTER_ENABLE_MAX                (1)

#define CFG_RA_RATE_LIMIT_INTERVAL_NAME         "gRARateLimitInterval"
#define CFG_RA_RATE_LIMIT_INTERVAL_DEFAULT      (60)
#define CFG_RA_RATE_LIMIT_INTERVAL_MIN          (0)
#define CFG_RA_RATE_LIMIT_INTERVAL_MAX          (60)

#define CFG_ROAMING_DFS_CHANNEL_NAME                "gAllowDFSChannelRoam"
#define CFG_ROAMING_DFS_CHANNEL_MIN                 (0)
#define CFG_ROAMING_DFS_CHANNEL_MAX                 (1)
#define CFG_ROAMING_DFS_CHANNEL_DEFAULT             (0)


#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
//Enable WLAN Logging to app space
#define CFG_WLAN_LOGGING_SUPPORT_NAME            "wlanLoggingEnable"
#define CFG_WLAN_LOGGING_SUPPORT_ENABLE          ( 1 )
#define CFG_WLAN_LOGGING_SUPPORT_DISABLE         ( 0 )
#define CFG_WLAN_LOGGING_SUPPORT_DEFAULT         ( 1 )

//Enable FATAL and ERROR logs for kmsg console
#define CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_NAME     "wlanLoggingFEToConsole"
#define CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_ENABLE   ( 1 )
#define CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_DISABLE  ( 0 )
#define CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_DEFAULT  ( 1 )

//Number of buffers to be used for WLAN logging
#define CFG_WLAN_LOGGING_NUM_BUF_NAME     "wlanLoggingNumBuf"
#define CFG_WLAN_LOGGING_NUM_BUF_MIN      ( 4  )
#define CFG_WLAN_LOGGING_NUM_BUF_MAX      ( 64 )
#define CFG_WLAN_LOGGING_NUM_BUF_DEFAULT  ( 32 )
#endif //WLAN_LOGGING_SOCK_SVC_ENABLE

#define CFG_INITIAL_DWELL_TIME_NAME            "gInitialDwellTime"
#define CFG_INITIAL_DWELL_TIME_DEFAULT         (0)
#define CFG_INITIAL_DWELL_TIME_MIN             (0)
#define CFG_INITIAL_DWELL_TIME_MAX             (100)

#define CFG_INITIAL_SCAN_SKIP_DFS_CH_NAME      "gInitialScanSkipDFSCh"
#define CFG_INITIAL_SCAN_SKIP_DFS_CH_DEFAULT   (1)
#define CFG_INITIAL_SCAN_SKIP_DFS_CH_MIN       (0)
#define CFG_INITIAL_SCAN_SKIP_DFS_CH_MAX       (1)

#define CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_NAME           "btcFatalHidnSniffBlkGuidance"
#define CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_MIN            ( 0 )
#define CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_MAX            ( 2 )
#define CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_DEFAULT        ( 2 )

#define CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_NAME        "btcCriticalHidnSniffBlkGuidance"
#define CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_MIN         ( 0 )
#define CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_MAX         ( 1 )
#define CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_DEFAULT     ( 1 )

#define CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_NAME                 "btcA2dpTxQueueThold"
#define CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_MIN                  ( 0 )
#define CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_MAX                  ( 1 )
#define CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_DEFAULT              ( 0 )

#define CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_NAME                  "btcOppTxQueueThold"
#define CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_MIN                   ( 0 )
#define CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_MAX                   ( 3 )
#define CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_DEFAULT               ( 1 )

#ifdef WLAN_FEATURE_11W
#define CFG_PMF_SA_QUERY_MAX_RETRIES_NAME                    "pmfSaQueryMaxRetries"
#define CFG_PMF_SA_QUERY_MAX_RETRIES_DEFAULT                 ( 5  )
#define CFG_PMF_SA_QUERY_MAX_RETRIES_MIN                     ( 0  )
#define CFG_PMF_SA_QUERY_MAX_RETRIES_MAX                     ( 20 )

#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_NAME                 "pmfSaQueryRetryInterval"
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_DEFAULT              ( 200  )
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_MIN                  ( 0    )
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_MAX                  ( 2000 )
#endif

#define CFG_DEFER_IMPS_FOR_TIME_NAME                         "gDeferIMPStime"
#define CFG_DEFER_IMPS_FOR_TIME_MIN                          (0)
#define CFG_DEFER_IMPS_FOR_TIME_MAX                          (500)
#define CFG_DEFER_IMPS_FOR_TIME_DEFAULT                      (200)

/* If last disconnection was due to HB failure and we reconnect
 * to same AP next time, send Deauth before starting connection
 */
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION                  "gSendDeauthBeforeCon"
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_MIN              (0)
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_MAX              (1)
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_DEFAULT          (0)

#define CFG_ENABLE_MAC_ADDR_SPOOFING                         "gEnableMacAddrSpoof"
#define CFG_ENABLE_MAC_ADDR_SPOOFING_MIN                     (0)
#define CFG_ENABLE_MAC_ADDR_SPOOFING_MAX                     (1)
#define CFG_ENABLE_MAC_ADDR_SPOOFING_DEFAULT                 (0)

#define CFG_ENABLE_CH_AVOID                                  "gEnableChannelAvoidance"
#define CFG_ENABLE_CH_AVOID_MIN                              ( 0 )
#define CFG_ENABLE_CH_AVOID_MAX                              ( 1 )
#define CFG_ENABLE_CH_AVOID_DEFAULT                          ( 1 )

#define CFG_MAX_CONCURRENT_CONNECTIONS_NAME       "gMaxConcurrentActiveSessions"
#define CFG_MAX_CONCURRENT_CONNECTIONS_DEFAULT    ( 2 )
#define CFG_MAX_CONCURRENT_CONNECTIONS_MIN        ( 1 )
#define CFG_MAX_CONCURRENT_CONNECTIONS_MAX        ( 4 )

//Enable Dynamic WMM PS support
#define CFG_ENABLE_DYNAMIC_WMMPS_NAME        "gEnableDynamicWMMPS"
#define CFG_ENABLE_DYNAMIC_WMM_PS_MIN        (0)
#define CFG_ENABLE_DYNAMIC_WMM_PS_MAX        (1)
#define CFG_ENABLE_DYNAMIC_WMM_PS_DEFAULT    (1)

/*
 * If the number of consecutive SPs for which the firmware has sent
 * trigger frame (because more data bit was set to 1 at end of SP)
 * when dynamic WMM PS support is enabled exceeds this value then
 * firmware will come out of power save mode
 */
#define CFG_MAX_UAPSD_CONSEC_SP_NAME             "gMaxUapsdConsecSP"
#define CFG_MAX_UAPSD_CONSEC_SP_DEFAULT          ( 10 )
#define CFG_MAX_UAPSD_CONSEC_SP_MIN              ( 0 )
#define CFG_MAX_UAPSD_CONSEC_SP_MAX              ( 500 )

/*
 * If the number of frames received in the "gUapsdConsecRxCntMeasWindow"
 * exceeds this value then firmware will come out of power save mode.
 * Comes into effect only when dynamic WMM PS is enabled.
 */
#define CFG_MAX_UAPSD_CONSEC_RX_CNT_NAME         "gMaxUapsdConsecRxCnt"
#define CFG_MAX_UAPSD_CONSEC_RX_CNT_DEFAULT      ( 50 )
#define CFG_MAX_UAPSD_CONSEC_RX_CNT_MIN          ( 0 )
#define CFG_MAX_UAPSD_CONSEC_RX_CNT_MAX          ( 5000 )

/*
 * If the number of frames transmitted in the "gUapsdConsecTxCntMeasWindow"
 * exceeds this value then firmware will come out of power save mode.
 * Comes into effect only when dynamic WMM PS is enabled.
 */
#define CFG_MAX_UAPSD_CONSEC_TX_CNT_NAME         "gMaxUapsdConsecTxCnt"
#define CFG_MAX_UAPSD_CONSEC_TX_CNT_DEFAULT      ( 50 )
#define CFG_MAX_UAPSD_CONSEC_TX_CNT_MIN          ( 0 )
#define CFG_MAX_UAPSD_CONSEC_TX_CNT_MAX          ( 5000 )

// Read the description in "gMaxUapsdConsecRxCnt", the interval is in msec
#define CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_NAME "gUapsdConsecRxCntMeasWindow"
#define CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_DEFAULT      ( 500 )
#define CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_MIN          ( 50 )
#define CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_MAX          ( 50000 )

// Read the description in "gMaxUapsdConsecTxCnt", the interval is in msec
#define CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_NAME "gUapsdConsecTxCntMeasWindow"
#define CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_DEFAULT      ( 500 )
#define CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_MIN          ( 50 )
#define CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_MAX          ( 50000 )

/*
 * number of PS poll frames that can be sent by the firmware to retrieve
 * Data before the firmware comes out of power save mode for an AC
 * which is neither Delivery or Trigger enabled when dynamic WMM PS
 * is configured.
 * Comes into effect only when dynamic WMM PS is enabled.
*/
#define CFG_UAPSD_PSPOLL_NAME          "gMaxPsPollInWmmUapsdMode"
#define CFG_UAPSD_PSPOLL_MIN           (0)
#define CFG_UAPSD_PSPOLL_MAX           (255)
#define CFG_UAPSD_PSPOLL_DEFAULT       (0)

/*
 * If the number of itervals for which the firmware detects no activity
 * on the WMM PS enabled ACs exceeds this value then the firmware will
 * go to power save mode. The length of the interval is dynamically
 * calculated by the firmware depending on the configured service
 * intervals.
 * Comes into effect only when dynamic WMM PS is enabled.
 */
#define CFG_MAX_UAPSD_INACT_INTVL_NAME       "gMaxUapsdInactivityIntervals"
#define CFG_MAX_UAPSD_INACT_INTVL_MIN        (1)
#define CFG_MAX_UAPSD_INACT_INTVL_MAX        (255)
#define CFG_MAX_UAPSD_INACT_INTVL_DEFAULT    (10)

/*
 * This will enable DHCP packet logging for debugging purpose
 * The log level is set as INFO.
 */
#define CFG_DEBUG_DHCP                          "gEnableDhcpDebug"
#define CFG_DEBUG_DHCP_DISABLE                  ( 0 )
#define CFG_DEBUG_DHCP_ENABLE                   ( 1 )
#define CFG_DEBUG_DHCP_DEFAULT                  ( CFG_DEBUG_DHCP_DISABLE )

#define CFG_BURST_MODE_BE_TXOP_VALUE           "gBurstModeTXOPValue"
#define CFG_BURST_MODE_BE_TXOP_VALUE_MIN       ( 0 )
#define CFG_BURST_MODE_BE_TXOP_VALUE_MAX       ( 12288 )
#define CFG_BURST_MODE_BE_TXOP_VALUE_DEFAULT   ( 0 )

/*--------------------------------------------------------------------------- 
  Type declarations
  -------------------------------------------------------------------------*/ 

typedef struct
{
   //Bitmap to track what is explicitly configured
   DECLARE_BITMAP(bExplicitCfg, MAX_CFG_INI_ITEMS);

   //Config parameters
   v_U32_t       RTSThreshold;
   v_U32_t       FragmentationThreshold;
   v_U32_t       nCheckForHangTime;
   v_U32_t       Calibration;
   v_U32_t       CalibrationPeriod;
   v_U8_t        OperatingChannel;
   v_BOOL_t      ShortSlotTimeEnabled;
   v_BOOL_t      Is11dSupportEnabled;
   v_BOOL_t      Is11hSupportEnabled;
   v_U32_t       nDeferScanTimeInterval;
   v_BOOL_t      fEnforce11dChannels;
   v_BOOL_t      fSupplicantCountryCodeHasPriority;
   v_BOOL_t      fEnforceCountryCodeMatch;
   v_BOOL_t      fEnforceDefaultDomain;
   v_U32_t       Cfg1Id;
   v_U32_t       Cfg2Id;
   v_U32_t       Cfg3Id;
   v_U32_t       Cfg4Id;
   v_U32_t       Cfg5Id;
   v_U32_t       Cfg1Value;
   v_U32_t       Cfg2Value;
   v_U32_t       Cfg3Value;
   v_U32_t       Cfg4Value;
   v_U32_t       Cfg5Value;
   v_U32_t       HeartbeatThresh24;
   char          PowerUsageControl[4];
   v_U8_t        nEnableSuspend;
   v_U8_t        nEnableDriverStop;
   v_BOOL_t      fIsImpsEnabled;
   v_BOOL_t      fIsSsrPanicOnFailure;
   v_BOOL_t      fIsLogpEnabled;
   v_U8_t        btcExecutionMode;
   v_U8_t        btcConsBtSlotsToBlockDuringDhcp;
   v_U8_t        btcA2DPBtSubIntervalsDuringDhcp;
   v_U32_t       btcStaticLenInqBt;
   v_U32_t       btcStaticLenPageBt;
   v_U32_t       btcStaticLenConnBt;
   v_U32_t       btcStaticLenLeBt;
   v_U32_t       btcStaticLenInqWlan;
   v_U32_t       btcStaticLenPageWlan;
   v_U32_t       btcStaticLenConnWlan;
   v_U32_t       btcStaticLenLeWlan;
   v_U32_t       btcDynMaxLenBt;
   v_U32_t       btcDynMaxLenWlan;
   v_U32_t       btcMaxScoBlockPerc;
   v_U32_t       btcDhcpProtOnA2dp;
   v_U32_t       btcDhcpProtOnSco;
   v_U32_t       mwsCoexVictimWANFreq[10];
   v_U32_t       mwsCoexVictimWLANFreq[10];
   v_U32_t       mwsCoexVictimConfig[10];
   v_U32_t       mwsCoexVictimConfig2[10];
   v_U32_t       mwsCoexModemBackoff;
   v_U32_t       mwsCoexConfig[6];
   v_U32_t       SARPowerBackoff;
   v_U32_t       nImpsModSleepTime;
   v_U32_t       nImpsMaxSleepTime;
   v_U32_t       nImpsMinSleepTime;
   v_BOOL_t      fIsBmpsEnabled;
   v_U32_t       nBmpsModListenInterval;
   v_U32_t       nBmpsMaxListenInterval;
   v_U32_t       nBmpsMinListenInterval;
   v_BOOL_t      fIsAutoBmpsTimerEnabled;
   v_U32_t       nAutoBmpsTimerValue;
   eHddDot11Mode dot11Mode;
   v_U32_t       nChannelBondingMode24GHz;
   v_U32_t       nChannelBondingMode5GHz;
   v_U32_t       MaxRxAmpduFactor;
   v_U32_t       nBAAgingTimerInterval;
   v_U16_t       TxRate;
   v_U32_t       AdaptiveThresholdAlgo;
   v_U32_t       ShortGI20MhzEnable;
   v_U32_t       BlockAckAutoSetup;
   v_U32_t       ScanResultAgeCount;
   v_U32_t       nScanAgeTimeNCNPS;
   v_U32_t       nScanAgeTimeNCPS;
   v_U32_t       nScanAgeTimeCNPS;
   v_U32_t       nScanAgeTimeCPS;
   v_U8_t        nRssiCatGap;
   v_BOOL_t      fIsShortPreamble;
   v_BOOL_t      fIsAutoIbssBssid;
   v_MACADDR_t   IbssBssid;
   v_U32_t       AdHocChannel5G;
   v_U32_t       AdHocChannel24G;
   v_U8_t        intfAddrMask;
   v_MACADDR_t   intfMacAddr[VOS_MAX_CONCURRENCY_PERSONA];

   v_BOOL_t      apUapsdEnabled;
   v_BOOL_t      apRandomBssidEnabled;
   v_BOOL_t      apProtEnabled;
   v_U16_t       apProtection;
   v_BOOL_t      apOBSSProtEnabled;
   v_U8_t        MinFramesProcThres;
   v_U8_t        apCntryCode[4];
   v_BOOL_t      apDisableIntraBssFwd;
   v_U8_t        nEnableListenMode;
   v_U32_t       nAPAutoShutOff;
   v_U8_t        apStartChannelNum;
   v_U8_t        apEndChannelNum;
   v_U8_t        apOperatingBand;
   v_BOOL_t      apAutoChannelSelection;
   v_U8_t        enableLTECoex;
   v_U32_t       apKeepAlivePeriod;
   v_U32_t       goKeepAlivePeriod;
   v_U32_t       apLinkMonitorPeriod;
   v_U32_t       goLinkMonitorPeriod;
   v_U32_t       nBeaconInterval;
   v_U8_t        nTxPowerCap;   //In dBm
   v_BOOL_t      fIsLowGainOverride;
   v_U8_t        disablePacketFilter;
#if defined WLAN_FEATURE_VOWIFI
   v_BOOL_t      fRrmEnable;
   v_U8_t        nInChanMeasMaxDuration;
   v_U8_t        nOutChanMeasMaxDuration;
   v_U16_t       nRrmRandnIntvl;
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
   //Vowifi 11r params
   v_BOOL_t      fFTResourceReqSupported;
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
   v_U16_t       nNeighborScanPeriod;
   v_U8_t        nNeighborReassocRssiThreshold;
   v_U8_t        nNeighborLookupRssiThreshold;
   v_U8_t        neighborScanChanList[WNI_CFG_VALID_CHANNEL_LIST_LEN];
   v_U16_t       nNeighborScanMinChanTime; 
   v_U16_t       nNeighborScanMaxChanTime; 
   v_U16_t       nMaxNeighborReqTries;
   v_U16_t       nNeighborResultsRefreshPeriod;
   v_U16_t       nEmptyScanRefreshPeriod;
   v_U8_t        nNeighborInitialForcedRoamTo5GhEnable;
#endif

   //Additional Handoff params
   v_BOOL_t       nEnableIdleScan;
   v_U32_t        nRoamingTime;
   v_U16_t        nVccRssiTrigger;
   v_U32_t        nVccUlMacLossThreshold;

   v_U32_t        nPassiveMinChnTime;    //in units of milliseconds
   v_U32_t        nPassiveMaxChnTime;    //in units of milliseconds
   v_U32_t        nActiveMinChnTime;     //in units of milliseconds
   v_U32_t        nActiveMaxChnTime;     //in units of milliseconds

   v_U32_t        nInitialDwellTime;     //in units of milliseconds

   v_U32_t        nActiveMinChnTimeBtc;     //in units of milliseconds
   v_U32_t        nActiveMaxChnTimeBtc;     //in units of milliseconds
#ifdef WLAN_AP_STA_CONCURRENCY
   v_U32_t        nPassiveMinChnTimeConc;    //in units of milliseconds
   v_U32_t        nPassiveMaxChnTimeConc;    //in units of milliseconds
   v_U32_t        nActiveMinChnTimeConc;     //in units of milliseconds
   v_U32_t        nActiveMaxChnTimeConc;     //in units of milliseconds
   v_U32_t        nRestTimeConc;             //in units of milliseconds
   v_U8_t         nNumStaChanCombinedConc;   //number of channels combined for
                                             //STA in each split scan operation
   v_U8_t         nNumP2PChanCombinedConc;   //number of channels combined for
                                             //P2P in each split scan operation
#endif

   v_U8_t         nMaxPsPoll;

   v_U8_t         nRssiFilterPeriod;
   v_BOOL_t       fIgnoreDtim;
   v_U8_t         fMaxLIModulatedDTIM;

   v_U8_t         nRxAnt;
   v_U8_t         fEnableFwHeartBeatMonitoring;
   v_U8_t         fEnableFwBeaconFiltering;
   v_BOOL_t       fEnableFwRssiMonitoring;
   v_U8_t         nDataInactivityTimeout;
   v_U8_t         nthBeaconFilter;

   //WMM QoS Configuration
   hdd_wmm_user_mode_t          WmmMode;
   v_BOOL_t                     b80211eIsEnabled;
   v_U8_t                       UapsdMask;    // what ACs to setup U-APSD for at assoc
   v_U32_t                      InfraUapsdVoSrvIntv;
   v_U32_t                      InfraUapsdVoSuspIntv;
   v_U32_t                      InfraUapsdViSrvIntv;
   v_U32_t                      InfraUapsdViSuspIntv;
   v_U32_t                      InfraUapsdBeSrvIntv;
   v_U32_t                      InfraUapsdBeSuspIntv;
   v_U32_t                      InfraUapsdBkSrvIntv;
   v_U32_t                      InfraUapsdBkSuspIntv;
#ifdef FEATURE_WLAN_LFR
   v_BOOL_t                     isFastRoamIniFeatureEnabled;
   v_BOOL_t                     MAWCEnabled;
#endif
#ifdef FEATURE_WLAN_ESE
   v_U32_t                      InfraInactivityInterval;
   v_BOOL_t                     isEseIniFeatureEnabled;
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
   v_BOOL_t                     isFastTransitionEnabled;
   v_U8_t                       RoamRssiDiff;
   v_U8_t                       nImmediateRoamRssiDiff;
   v_BOOL_t                     isWESModeEnabled;
#endif
#ifdef FEATURE_WLAN_OKC
   v_BOOL_t                     isOkcIniFeatureEnabled;
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   v_BOOL_t                     isRoamOffloadScanEnabled;
#endif
   hdd_wmm_classification_t     PktClassificationBasis; // DSCP or 802.1Q
   v_BOOL_t                     bImplicitQosEnabled;

   /* default TSPEC parameters for AC_VO */
   sme_QosWmmDirType            InfraDirAcVo;
   v_U16_t                      InfraNomMsduSizeAcVo;
   v_U32_t                      InfraMeanDataRateAcVo;
   v_U32_t                      InfraMinPhyRateAcVo;
   v_U16_t                      InfraSbaAcVo;

   /* default TSPEC parameters for AC_VI */
   sme_QosWmmDirType            InfraDirAcVi;
   v_U16_t                      InfraNomMsduSizeAcVi;
   v_U32_t                      InfraMeanDataRateAcVi;
   v_U32_t                      InfraMinPhyRateAcVi;
   v_U16_t                      InfraSbaAcVi;

   /* default TSPEC parameters for AC_BE */
   sme_QosWmmDirType            InfraDirAcBe;
   v_U16_t                      InfraNomMsduSizeAcBe;
   v_U32_t                      InfraMeanDataRateAcBe;
   v_U32_t                      InfraMinPhyRateAcBe;
   v_U16_t                      InfraSbaAcBe;

   /* default TSPEC parameters for AC_BK */
   sme_QosWmmDirType            InfraDirAcBk;
   v_U16_t                      InfraNomMsduSizeAcBk;
   v_U32_t                      InfraMeanDataRateAcBk;
   v_U32_t                      InfraMinPhyRateAcBk;
   v_U16_t                      InfraSbaAcBk;

   /* TL related configuration */
   v_U8_t                       WfqBkWeight;
   v_U8_t                       WfqBeWeight;
   v_U8_t                       WfqViWeight;
   v_U8_t                       WfqVoWeight;
   v_U32_t                      DelayedTriggerFrmInt;
   v_U16_t                      BkReorderAgingTime;
   v_U16_t                      BeReorderAgingTime;
   v_U16_t                      ViReorderAgingTime;
   v_U16_t                      VoReorderAgingTime;

   /* Wowl pattern */
   char                        wowlPattern[1024];         

   /* Control for Replay counetr. value 1 means 
      single replay counter for all TID*/
   v_BOOL_t                    bSingleTidRc;
   v_U8_t                      mcastBcastFilterSetting;
   v_BOOL_t                    fhostArpOffload;
   v_BOOL_t                    fhostNSOffload;
   v_BOOL_t                    burstSizeDefinition;
   v_U8_t                      tsInfoAckPolicy;
   
   /* RF Settling Time Clock */
   v_U32_t                     rfSettlingTimeUs;
   v_U8_t                      enableBtAmp;
#ifdef WLAN_BTAMP_FEATURE
   v_U8_t                      preferredChannel;
#endif //WLAN_BTAMP_FEATURE

   v_U8_t                      dynamicPsPollValue;
   v_BOOL_t                    AddTSWhenACMIsOff;
   v_BOOL_t                    fValidateScanList;

   v_U32_t                     infraStaKeepAlivePeriod;
   v_U8_t                      nNullDataApRespTimeout;
   v_U8_t                      nBandCapability;

   v_U32_t                     apDataAvailPollPeriodInMs;
   v_BOOL_t                    fEnableBeaconEarlyTermination;
   v_BOOL_t                    teleBcnWakeupEn;

/* VOS Trace Control*/
   v_U16_t                     vosTraceEnableBAP;
   v_U16_t                     vosTraceEnableTL;
   v_U16_t                     vosTraceEnableWDI;
   v_U16_t                     vosTraceEnableHDD;
   v_U16_t                     vosTraceEnableSME;
   v_U16_t                     vosTraceEnablePE;
   v_U16_t                     vosTraceEnablePMC;
   v_U16_t                     vosTraceEnableWDA;
   v_U16_t                     vosTraceEnableSYS;
   v_U16_t                     vosTraceEnableVOSS;
   v_U16_t                     vosTraceEnableSAP;
   v_U16_t                     vosTraceEnableHDDSAP;

   /* WDI Trace Control */
   v_U32_t                     wdiTraceEnableDAL;
   v_U32_t                     wdiTraceEnableCTL;
   v_U32_t                     wdiTraceEnableDAT;
   v_U32_t                     wdiTraceEnablePAL;
   v_U16_t                     nTeleBcnTransListenInterval;
   v_U16_t                     nTeleBcnMaxListenInterval;
   v_U16_t                     nTeleBcnTransLiNumIdleBeacons;
   v_U16_t                     nTeleBcnMaxLiNumIdleBeacons;
   v_U8_t                      bcnEarlyTermWakeInterval;
   v_U32_t                     enableCloseLoop;
   v_U8_t                      enableBypass11d;
   v_U8_t                      enableDFSChnlScan;
   v_U8_t                      enableDFSPnoChnlScan;
   v_U8_t                      enableDynamicDTIM;
   v_U8_t                      enableAutomaticTxPowerControl;
   v_U8_t                      ShortGI40MhzEnable;
   eHddLinkSpeedReportType     reportMaxLinkSpeed;
   v_S31_t                     linkSpeedRssiHigh;
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
   v_BOOL_t                    nRoamPrefer5GHz;
   v_BOOL_t                    nRoamIntraBand;
   v_U8_t                      nProbes;
   v_U16_t                     nRoamScanHomeAwayTime;
#endif
   v_S31_t                     linkSpeedRssiMid;
   v_S31_t                     linkSpeedRssiLow;
   v_U8_t                      enableMCC;
   v_U8_t                      allowMCCGODiffBI;
   v_BOOL_t                    isP2pDeviceAddrAdministrated;
   v_U8_t                      thermalMitigationEnable;
#ifdef WLAN_FEATURE_11AC
   v_U8_t                      vhtChannelWidth;
   v_U8_t                      vhtRxMCS;
   v_U8_t                      vhtTxMCS;
   v_BOOL_t                    enableTxBF;
   v_U8_t                      txBFCsnValue;
   v_BOOL_t                    enableMuBformee;
   v_U8_t                      gVhtMaxAmpduLenExp;
#endif
   v_U8_t                      enableModulatedDTIM;
   v_U32_t                     fEnableMCAddrList;
   v_BOOL_t                    enableFirstScan2GOnly;
   v_BOOL_t                    skipDfsChnlInP2pSearch;
   v_BOOL_t                    ignoreDynamicDtimInP2pMode;
   v_U16_t                     configMccParam;
   v_U32_t                     numBuffAdvert;
   v_BOOL_t                    enableRxSTBC;
#ifdef FEATURE_WLAN_TDLS       
   v_BOOL_t                    fEnableTDLSSupport;
   v_BOOL_t                    fEnableTDLSImplicitTrigger;
   v_U32_t                     fTDLSTxStatsPeriod;
   v_U32_t                     fTDLSTxPacketThreshold;
   v_U32_t                     fTDLSDiscoveryPeriod;
   v_U32_t                     fTDLSMaxDiscoveryAttempt;
   v_U32_t                     fTDLSIdleTimeout;
   v_U32_t                     fTDLSIdlePacketThreshold;
   v_U32_t                     fTDLSRSSIHysteresis;
   v_S31_t                     fTDLSRSSITriggerThreshold;
   v_S31_t                     fTDLSRSSITeardownThreshold;
   v_U32_t                     fTDLSUapsdMask;    // what ACs to setup U-APSD for TDLS
   v_U32_t                     fEnableTDLSBufferSta;
   v_U32_t                     fTDLSPuapsdInactivityTimer;
   v_U32_t                     fTDLSRxFrameThreshold;
   v_BOOL_t                    fTDLSExternalControl;
   v_U32_t                     fEnableTDLSOffChannel;
   v_U32_t                     fEnableTDLSWmmMode;
   v_BOOL_t                    fEnableTDLSScanCoexSupport;
#endif
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
   v_BOOL_t                    fEnableLLStats;
#endif
#ifdef WLAN_FEATURE_EXTSCAN
   v_BOOL_t                    fEnableEXTScan;
#endif
#ifdef WLAN_SOFTAP_VSTA_FEATURE
   v_BOOL_t                    fEnableVSTASupport;
#endif
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
   v_BOOL_t                    fEnableActiveModeOffload;
#endif
   v_U32_t                     enableLpwrImgTransition;
   v_U8_t                      scanAgingTimeout;
   v_BOOL_t                    enableTxLdpc;
   v_U8_t                      disableLDPCWithTxbfAP;
   v_U8_t                      enableMCCAdaptiveScheduler;
   v_BOOL_t                    isAndroidPsEn;
   v_BOOL_t                    sapAllowAllChannel;
   v_U8_t                      retryLimitZero;
   v_U8_t                      retryLimitOne;
   v_U8_t                      retryLimitTwo;
   v_U8_t                      disableAggWithBtc;
   char                        listOfNonDfsCountryCode[128];
   v_BOOL_t                    enableSSR;
   char                        listOfNon11acCountryCode[128];
   v_U32_t                     cfgMaxMediumTime;
   v_U8_t                      enableTrafficMonitor;
   v_U32_t                     trafficIdleTimeout;
   v_BOOL_t                    enableVhtFor24GHzBand;
   v_U8_t                      fScanOffload;
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   /* Flag indicating whether legacy fast roam during concurrency is enabled in cfg.ini or not */
   v_BOOL_t                    bFastRoamInConIniFeatureEnabled;
#endif
   v_BOOL_t                    fEnableAdaptRxDrain;
   //TX and RX traffic threshold for split scan
   v_U8_t                      txRxThresholdForSplitScan;
   v_U8_t                      dynSplitscan;   //Enable/Disable dynamic
                                                 //splitscan
   //Traffic monitor timer for split scan
   v_U32_t                     trafficMntrTmrForSplitScan;
   v_U8_t                      flexConnectPowerFactor;
   v_BOOL_t                    enableIbssHeartBeatOffload;
   v_U32_t                     antennaDiversity;
   v_BOOL_t                    fEnableSNRMonitoring;
   /*PNO related parameters */
#ifdef FEATURE_WLAN_SCAN_PNO
   v_BOOL_t                    configPNOScanSupport;
   v_U32_t                     configPNOScanTimerRepeatValue;
#endif
   v_U8_t                      isAmsduSupportInAMPDU;
   v_U8_t                      nSelect5GHzMargin;
   v_U8_t                      isCoalesingInIBSSAllowed;
   v_BOOL_t                    cfgAthDisable;
   v_U32_t                     cfgBtcActiveWlanLen;
   v_U32_t                     cfgBtcActiveBtLen;
   v_U32_t                     cfgBtcSapActiveWlanLen;
   v_U32_t                     cfgBtcSapActiveBtLen;
#ifdef MEMORY_DEBUG
   v_BOOL_t                    IsMemoryDebugSupportEnabled;
#endif
   char                        overrideCountryCode[4];
   v_U32_t                     gAsdProbeInterval;
   v_S7_t                      gAsdTriggerThreshold;
   v_U32_t                     gAsdRTTRssiHystThreshold;
   v_BOOL_t                    debugP2pRemainOnChannel;
   v_U32_t                     cfgBtcCTS2SduringSCO;
   v_U32_t                     gEnableDebugLog;
   v_U16_t                     nOBSSScanActiveDwellTime;
   v_U16_t                     nOBSSScanPassiveDwellTime;
   v_U16_t                     nOBSSScanWidthTriggerInterval;
   v_BOOL_t                    gEnableStrictRegulatoryForFCC;
   v_BOOL_t                    advertiseConcurrentOperation;
   v_U32_t                     defaultRateIndex24Ghz;
   eHddDot11Mode               sapDot11Mode;
   v_BOOL_t                    cfgRAFilterEnable;
   v_U16_t                     cfgRARateLimitInterval;
   v_U8_t                      allowDFSChannelRoam;

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   //WLAN Logging
   v_U32_t                     wlanLoggingEnable;
   v_U32_t                     wlanLoggingFEToConsole;
   v_U32_t                     wlanLoggingNumBuf;
#endif

   v_BOOL_t                    initialScanSkipDFSCh;
   v_U32_t                     cfgBtcFatalHidnSniffBlkGuidance;
   v_U32_t                     cfgBtcCriticalHidnSniffBlkGuidance;
   v_U32_t                     cfgBtcA2dpTxQueueThold;
   v_U32_t                     cfgBtcOppTxQueueThold;

#ifdef WLAN_FEATURE_11W
   v_U32_t                     pmfSaQueryMaxRetries;
   v_U32_t                     pmfSaQueryRetryInterval;
#endif
   v_U32_t                     deferImpsTime;
   v_BOOL_t                    sendDeauthBeforeCon;
   v_BOOL_t                    enableMacSpoofing;
   v_BOOL_t                    fenableCHAvoidance;
   v_U8_t                      gMaxConcurrentActiveSessions;

   v_U32_t                     enableDynamicWMMPS;
   v_U32_t                     maxUapsdConsecSP;
   v_U32_t                     maxUapsdConsecRxCnt;
   v_U32_t                     maxUapsdConsecTxCnt;
   v_U32_t                     uapsdConsecRxCntMeasWindow;
   v_U32_t                     uapsdConsecTxCntMeasWindow;
   v_U32_t                     maxPsPollInWmmUapsdMode;
   v_U32_t                     maxUapsdInactivityIntervals;
   v_U8_t                      enableDhcpDebug; /* Enable/Disable dhcp debug */
   v_U32_t                     burstModeTXOPValue;
   v_U8_t                      acsScanBandPreference;
   v_U16_t                     acsBandSwitchThreshold;
   v_U32_t                     enableDynamicRAStartRate;
} hdd_config_t;
/*--------------------------------------------------------------------------- 
  Function declarations and documenation
  -------------------------------------------------------------------------*/ 
VOS_STATUS hdd_parse_config_ini(hdd_context_t *pHddCtx);
VOS_STATUS hdd_set_sme_config( hdd_context_t *pHddCtx );
v_BOOL_t hdd_update_config_dat ( hdd_context_t *pHddCtx );
VOS_STATUS hdd_cfg_get_config(hdd_context_t *pHddCtx, char *pBuf, int buflen);
eCsrPhyMode hdd_cfg_xlate_to_csr_phy_mode( eHddDot11Mode dot11Mode );
VOS_STATUS hdd_execute_config_command(hdd_context_t *pHddCtx, char *command);
tANI_BOOLEAN hdd_is_okc_mode_enabled(hdd_context_t *pHddCtx);

#define VAR_OFFSET( _Struct, _Var ) (offsetof(_Struct, _Var))
#define VAR_SIZE( _Struct, _Var ) (sizeof(((_Struct *)0)->_Var))

#define VAR_FLAGS_NONE         (      0 )
#define VAR_FLAGS_REQUIRED     ( 1 << 0 )   // bit 0 is Required or Optional
#define VAR_FLAGS_OPTIONAL     ( 0 << 0 )

#define VAR_FLAGS_RANGE_CHECK  ( 1 << 1 )   // bit 1 tells if range checking is required.
                                            // If less than MIN, assume MIN.
                                            // If greater than MAX, assume MAX.

#define VAR_FLAGS_RANGE_CHECK_ASSUME_MINMAX ( VAR_FLAGS_RANGE_CHECK )

#define VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT ( 1 << 2 )  // bit 2 is range checking that assumes the DEFAULT value
                                                         // If less than MIN, assume DEFAULT,
                                                         // If grateer than MAX, assume DEFAULT.

#define VAR_FLAGS_DYNAMIC_CFG ( 1 << 3 )  // Bit 3 indicates that
                                          // the config item can be
                                          // modified dynamicially
                                          // on a running system

typedef enum 
{
  WLAN_PARAM_Integer,
  WLAN_PARAM_SignedInteger,
  WLAN_PARAM_HexInteger,
  WLAN_PARAM_String,
  WLAN_PARAM_MacAddr,
}WLAN_PARAMETER_TYPE;

#define REG_VARIABLE( _Name, _Type,  _Struct, _VarName,          \
                      _Flags, _Default, _Min, _Max )             \
{                                                                \
  ( _Name ),                                                     \
  ( _Type ),                                                     \
  ( _Flags ),                                                    \
  VAR_OFFSET( _Struct, _VarName ),                               \
  VAR_SIZE( _Struct, _VarName ),                                 \
  ( _Default ),                                                  \
  ( _Min ),                                                      \
  ( _Max ),                                                      \
  NULL,                                                          \
  0                                                              \
}

#define REG_DYNAMIC_VARIABLE( _Name, _Type,  _Struct, _VarName,  \
                              _Flags, _Default, _Min, _Max,      \
                              _CBFunc, _CBParam )                \
{                                                                \
  ( _Name ),                                                     \
  ( _Type ),                                                     \
  ( VAR_FLAGS_DYNAMIC_CFG | ( _Flags ) ),                        \
  VAR_OFFSET( _Struct, _VarName ),                               \
  VAR_SIZE( _Struct, _VarName ),                                 \
  ( _Default ),                                                  \
  ( _Min ),                                                      \
  ( _Max ),                                                      \
  ( _CBFunc ),                                                   \
  ( _CBParam )                                                   \
}

#define REG_VARIABLE_STRING( _Name, _Type,  _Struct, _VarName,   \
                             _Flags, _Default )                  \
{                                                                \
  ( _Name ),                                                     \
  ( _Type ),                                                     \
  ( _Flags ),                                                    \
  VAR_OFFSET( _Struct, _VarName ),                               \
  VAR_SIZE( _Struct, _VarName ),                                 \
  (unsigned long)( _Default ),                                   \
  0,                                                             \
  0,                                                             \
  NULL,                                                          \
  0                                                              \
}

typedef struct tREG_TABLE_ENTRY {

  char*               RegName;            // variable name in the qcom_cfg.ini file
  WLAN_PARAMETER_TYPE RegType;            // variable type in the hdd_config_t structure
  unsigned long       Flags;              // Specify optional parms and if RangeCheck is performed
  unsigned short      VarOffset;          // offset to field from the base address of the structure
  unsigned short      VarSize;            // size (in bytes) of the field
  unsigned long       VarDefault;         // default value to use
  unsigned long       VarMin;             // minimum value, for range checking
  unsigned long       VarMax;             // maximum value, for range checking
                                          // Dynamic modification notifier
  void (*pfnDynamicNotify)(hdd_context_t *pHddCtx, unsigned long NotifyId);
  unsigned long       NotifyId;           // Dynamic modification identifier
} REG_TABLE_ENTRY;

static __inline unsigned long utilMin( unsigned long a, unsigned long b )
{
  return( ( a < b ) ? a : b );
}

#endif
