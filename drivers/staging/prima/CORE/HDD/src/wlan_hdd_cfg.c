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

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  07/27/09    kanand Created module.

  ==========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/


#include <linux/firmware.h>
#include <linux/string.h>
#include <wlan_hdd_includes.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_cfg.h>
#include <linux/string.h>
#include <vos_types.h>
#include <csrApi.h>
#include <pmcApi.h>
#include <wlan_hdd_misc.h>

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
static void cbNotifySetRoamPrefer5GHz(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateRoamPrefer5GHz((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nRoamPrefer5GHz);
}

static void cbNotifySetImmediateRoamRssiDiff(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateImmediateRoamRssiDiff((tHalHandle)(pHddCtx->hHal),
                                    pHddCtx->cfg_ini->nImmediateRoamRssiDiff);
}

static void cbNotifySetRoamRssiDiff(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateRoamRssiDiff((tHalHandle)(pHddCtx->hHal),
                                    pHddCtx->cfg_ini->RoamRssiDiff);
}

static void cbNotifySetFastTransitionEnabled(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateFastTransitionEnabled((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->isFastTransitionEnabled);
}

static void cbNotifySetRoamIntraBand(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_setRoamIntraBand((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nRoamIntraBand);
}

static void cbNotifySetWESMode(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_UpdateWESMode((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->isWESModeEnabled);
}

static void cbNotifySetRoamScanNProbes(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateRoamScanNProbes((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nProbes);
}

static void cbNotifySetRoamScanHomeAwayTime(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
     sme_UpdateRoamScanHomeAwayTime((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nRoamScanHomeAwayTime, eANI_BOOLEAN_TRUE);
}
#endif

#ifdef FEATURE_WLAN_OKC
static void cbNotifySetOkcFeatureEnabled(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
}
#endif

#ifdef FEATURE_WLAN_LFR
static void NotifyIsFastRoamIniFeatureEnabled(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_UpdateIsFastRoamIniFeatureEnabled((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled );
}

static void NotifyIsMAWCIniFeatureEnabled(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    /* at the point this routine is called, the value in the cfg_ini table has already been updated */
    sme_UpdateIsMAWCIniFeatureEnabled((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->MAWCEnabled );
}
#endif

#ifdef FEATURE_WLAN_ESE
static void cbNotifySetEseFeatureEnabled(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_UpdateIsEseFeatureEnabled((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->isEseIniFeatureEnabled );
}
#endif

static void cbNotifySetFwRssiMonitoring(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_UpdateConfigFwRssiMonitoring((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->fEnableFwRssiMonitoring );
}

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
static void cbNotifySetNeighborLookupRssiThreshold(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_setNeighborLookupRssiThreshold((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nNeighborLookupRssiThreshold );
}

static void cbNotifySetNeighborScanPeriod(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_setNeighborScanPeriod((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nNeighborScanPeriod );
}

static void cbNotifySetNeighborResultsRefreshPeriod(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_setNeighborScanRefreshPeriod((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nNeighborResultsRefreshPeriod );
}

static void cbNotifySetEmptyScanRefreshPeriod(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_UpdateEmptyScanRefreshPeriod((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nEmptyScanRefreshPeriod);
}

static void cbNotifySetNeighborScanMinChanTime(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    // at the point this routine is called, the value in the cfg_ini table has already been updated
    sme_setNeighborScanMinChanTime((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nNeighborScanMinChanTime);
}

static void cbNotifySetNeighborScanMaxChanTime(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_setNeighborScanMaxChanTime((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->nNeighborScanMaxChanTime);
}
#endif

static void cbNotifySetEnableSSR(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateEnableSSR((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->enableSSR);
}

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
static void cbNotifyUpdateRoamScanOffloadEnabled(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateRoamScanOffloadEnabled((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->isRoamOffloadScanEnabled);
    if (0 == pHddCtx->cfg_ini->isRoamOffloadScanEnabled)
    {
        pHddCtx->cfg_ini->bFastRoamInConIniFeatureEnabled = 0;
        sme_UpdateEnableFastRoamInConcurrency((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->bFastRoamInConIniFeatureEnabled );
    }
}

static void cbNotifySetEnableFastRoamInConcurrency(hdd_context_t *pHddCtx, unsigned long NotifyId)
{
    sme_UpdateEnableFastRoamInConcurrency((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->bFastRoamInConIniFeatureEnabled );
}

#endif

REG_TABLE_ENTRY g_registry_table[] =
{
   REG_VARIABLE( CFG_RTS_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, RTSThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RTS_THRESHOLD_DEFAULT,
                 CFG_RTS_THRESHOLD_MIN,
                 CFG_RTS_THRESHOLD_MAX ),

   REG_VARIABLE( CFG_FRAG_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, FragmentationThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_FRAG_THRESHOLD_DEFAULT,
                 CFG_FRAG_THRESHOLD_MIN,
                 CFG_FRAG_THRESHOLD_MAX ),

   REG_VARIABLE( CFG_CALIBRATION_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Calibration,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_CALIBRATION_DEFAULT,
                 CFG_CALIBRATION_MIN,
                 CFG_CALIBRATION_MAX ),

   REG_VARIABLE( CFG_CALIBRATION_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, CalibrationPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_CALIBRATION_PERIOD_DEFAULT,
                 CFG_CALIBRATION_PERIOD_MIN,
                 CFG_CALIBRATION_PERIOD_MAX ),

   REG_VARIABLE( CFG_OPERATING_CHANNEL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, OperatingChannel,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_OPERATING_CHANNEL_DEFAULT,
                 CFG_OPERATING_CHANNEL_MIN,
                 CFG_OPERATING_CHANNEL_MAX ),

   REG_VARIABLE( CFG_SHORT_SLOT_TIME_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, ShortSlotTimeEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SHORT_SLOT_TIME_ENABLED_DEFAULT,
                 CFG_SHORT_SLOT_TIME_ENABLED_MIN,
                 CFG_SHORT_SLOT_TIME_ENABLED_MAX ),

   REG_VARIABLE( CFG_11D_SUPPORT_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Is11dSupportEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_11D_SUPPORT_ENABLED_DEFAULT,
                 CFG_11D_SUPPORT_ENABLED_MIN,
                 CFG_11D_SUPPORT_ENABLED_MAX ),

   REG_VARIABLE( CFG_11H_SUPPORT_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Is11hSupportEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_11H_SUPPORT_ENABLED_DEFAULT,
                 CFG_11H_SUPPORT_ENABLED_MIN,
                 CFG_11H_SUPPORT_ENABLED_MAX ),

   REG_VARIABLE( CFG_ENFORCE_11D_CHANNELS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnforce11dChannels,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_ENFORCE_11D_CHANNELS_DEFAULT,
                 CFG_ENFORCE_11D_CHANNELS_MIN,
                 CFG_ENFORCE_11D_CHANNELS_MAX ),

   REG_VARIABLE( CFG_COUNTRY_CODE_PRIORITY_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fSupplicantCountryCodeHasPriority,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_COUNTRY_CODE_PRIORITY_DEFAULT,
                 CFG_COUNTRY_CODE_PRIORITY_MIN,
                 CFG_COUNTRY_CODE_PRIORITY_MAX),

   REG_VARIABLE( CFG_ENFORCE_COUNTRY_CODE_MATCH_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnforceCountryCodeMatch,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_ENFORCE_COUNTRY_CODE_MATCH_DEFAULT,
                 CFG_ENFORCE_COUNTRY_CODE_MATCH_MIN,
                 CFG_ENFORCE_COUNTRY_CODE_MATCH_MAX ),

   REG_VARIABLE( CFG_ENFORCE_DEFAULT_DOMAIN_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnforceDefaultDomain,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_ENFORCE_DEFAULT_DOMAIN_DEFAULT,
                 CFG_ENFORCE_DEFAULT_DOMAIN_MIN,
                 CFG_ENFORCE_DEFAULT_DOMAIN_MAX ),

   REG_VARIABLE( CFG_GENERIC_ID1_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg1Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_ID1_DEFAULT,
                 CFG_GENERIC_ID1_MIN,
                 CFG_GENERIC_ID1_MAX ),

   REG_VARIABLE( CFG_GENERIC_ID2_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg2Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_ID2_DEFAULT,
                 CFG_GENERIC_ID2_MIN,
                 CFG_GENERIC_ID2_MAX ),

   REG_VARIABLE( CFG_GENERIC_ID3_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg3Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_ID3_DEFAULT,
                 CFG_GENERIC_ID3_MIN,
                 CFG_GENERIC_ID3_MAX ),

   REG_VARIABLE( CFG_GENERIC_ID4_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg4Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_ID4_DEFAULT,
                 CFG_GENERIC_ID4_MIN,
                 CFG_GENERIC_ID4_MAX ),

   REG_VARIABLE( CFG_GENERIC_ID5_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg5Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_ID5_DEFAULT,
                 CFG_GENERIC_ID5_MIN,
                 CFG_GENERIC_ID5_MAX ),

   REG_VARIABLE( CFG_GENERIC_VALUE1_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg1Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_VALUE1_DEFAULT,
                 CFG_GENERIC_VALUE1_MIN,
                 CFG_GENERIC_VALUE1_MAX ),

   REG_VARIABLE( CFG_GENERIC_VALUE2_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg2Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_VALUE2_DEFAULT,
                 CFG_GENERIC_VALUE2_MIN,
                 CFG_GENERIC_VALUE2_MAX ),

   REG_VARIABLE( CFG_GENERIC_VALUE3_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg3Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_VALUE3_DEFAULT,
                 CFG_GENERIC_VALUE3_MIN,
                 CFG_GENERIC_VALUE3_MAX ),

   REG_VARIABLE( CFG_GENERIC_VALUE4_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg4Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_VALUE4_DEFAULT,
                 CFG_GENERIC_VALUE4_MIN,
                 CFG_GENERIC_VALUE4_MAX ),

   REG_VARIABLE( CFG_GENERIC_VALUE5_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg5Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GENERIC_VALUE5_DEFAULT,
                 CFG_GENERIC_VALUE5_MIN,
                 CFG_GENERIC_VALUE5_MAX ),

   REG_VARIABLE( CFG_HEARTBEAT_THRESH_24_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, HeartbeatThresh24,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_HEARTBEAT_THRESH_24_DEFAULT,
                 CFG_HEARTBEAT_THRESH_24_MIN,
                 CFG_HEARTBEAT_THRESH_24_MAX ),

   REG_VARIABLE_STRING( CFG_POWER_USAGE_NAME, WLAN_PARAM_String,
                        hdd_config_t, PowerUsageControl,
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_POWER_USAGE_DEFAULT ),

   REG_VARIABLE( CFG_ENABLE_SUSPEND_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nEnableSuspend,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_SUSPEND_DEFAULT,
                 CFG_ENABLE_SUSPEND_MIN,
                 CFG_ENABLE_SUSPEND_MAX ),

   REG_VARIABLE( CFG_ENABLE_ENABLE_DRIVER_STOP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nEnableDriverStop,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_ENABLE_DRIVER_STOP_DEFAULT,
                 CFG_ENABLE_ENABLE_DRIVER_STOP_MIN,
                 CFG_ENABLE_ENABLE_DRIVER_STOP_MAX ),

   REG_VARIABLE( CFG_ENABLE_IMPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsImpsEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_IMPS_DEFAULT,
                 CFG_ENABLE_IMPS_MIN,
                 CFG_ENABLE_IMPS_MAX ),

   REG_VARIABLE( CFG_SSR_PANIC_ON_FAILURE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsSsrPanicOnFailure,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SSR_PANIC_ON_FAILURE_DEFAULT,
                 CFG_SSR_PANIC_ON_FAILURE_MIN,
                 CFG_SSR_PANIC_ON_FAILURE_MAX),

   REG_VARIABLE( CFG_ENABLE_LOGP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsLogpEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_LOGP_DEFAULT,
                 CFG_ENABLE_LOGP_MIN,
                 CFG_ENABLE_LOGP_MAX ),

   REG_VARIABLE( CFG_IMPS_MINIMUM_SLEEP_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nImpsMinSleepTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_IMPS_MINIMUM_SLEEP_TIME_DEFAULT,
                 CFG_IMPS_MINIMUM_SLEEP_TIME_MIN,
                 CFG_IMPS_MINIMUM_SLEEP_TIME_MAX ),

   REG_VARIABLE( CFG_IMPS_MAXIMUM_SLEEP_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nImpsMaxSleepTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_IMPS_MAXIMUM_SLEEP_TIME_DEFAULT,
                 CFG_IMPS_MAXIMUM_SLEEP_TIME_MIN,
                 CFG_IMPS_MAXIMUM_SLEEP_TIME_MAX ),

   REG_VARIABLE( CFG_DEFER_SCAN_TIME_INTERVAL, WLAN_PARAM_Integer,
                 hdd_config_t, nDeferScanTimeInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DEFER_SCAN_TIME_INTERVAL_DEFAULT,
                 CFG_DEFER_SCAN_TIME_INTERVAL_MIN,
                 CFG_DEFER_SCAN_TIME_INTERVAL_MAX ),

   REG_VARIABLE( CFG_IMPS_MODERATE_SLEEP_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nImpsModSleepTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_IMPS_MODERATE_SLEEP_TIME_DEFAULT,
                 CFG_IMPS_MODERATE_SLEEP_TIME_MIN,
                 CFG_IMPS_MODERATE_SLEEP_TIME_MAX ),

   REG_VARIABLE( CFG_ENABLE_BMPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsBmpsEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_BMPS_DEFAULT,
                 CFG_ENABLE_BMPS_MIN,
                 CFG_ENABLE_BMPS_MAX ),

   REG_VARIABLE( CFG_BMPS_MINIMUM_LI_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBmpsMinListenInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BMPS_MINIMUM_LI_DEFAULT,
                 CFG_BMPS_MINIMUM_LI_MIN,
                 CFG_BMPS_MINIMUM_LI_MAX ),

   REG_VARIABLE( CFG_BMPS_MAXIMUM_LI_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBmpsMaxListenInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BMPS_MAXIMUM_LI_DEFAULT,
                 CFG_BMPS_MAXIMUM_LI_MIN,
                 CFG_BMPS_MAXIMUM_LI_MAX ),

   REG_VARIABLE( CFG_BMPS_MODERATE_LI_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBmpsModListenInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BMPS_MODERATE_LI_DEFAULT,
                 CFG_BMPS_MODERATE_LI_MIN,
                 CFG_BMPS_MODERATE_LI_MAX ),

   REG_VARIABLE( CFG_ENABLE_AUTO_BMPS_TIMER_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsAutoBmpsTimerEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_AUTO_BMPS_TIMER_DEFAULT,
                 CFG_ENABLE_AUTO_BMPS_TIMER_MIN,
                 CFG_ENABLE_AUTO_BMPS_TIMER_MAX ),

   REG_VARIABLE( CFG_AUTO_BMPS_TIMER_VALUE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nAutoBmpsTimerValue,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AUTO_BMPS_TIMER_VALUE_DEFAULT,
                 CFG_AUTO_BMPS_TIMER_VALUE_MIN,
                 CFG_AUTO_BMPS_TIMER_VALUE_MAX ),

   REG_VARIABLE( CFG_DOT11_MODE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, dot11Mode,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_DOT11_MODE_DEFAULT,
                 CFG_DOT11_MODE_MIN,
                 CFG_DOT11_MODE_MAX ),

   REG_VARIABLE( CFG_CHANNEL_BONDING_MODE_24GHZ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nChannelBondingMode24GHz,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_CHANNEL_BONDING_MODE_DEFAULT,
                 CFG_CHANNEL_BONDING_MODE_MIN,
                 CFG_CHANNEL_BONDING_MODE_MAX),

   REG_VARIABLE( CFG_CHANNEL_BONDING_MODE_5GHZ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nChannelBondingMode5GHz,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_CHANNEL_BONDING_MODE_DEFAULT,
                 CFG_CHANNEL_BONDING_MODE_MIN,
                 CFG_CHANNEL_BONDING_MODE_MAX),

   REG_VARIABLE( CFG_MAX_RX_AMPDU_FACTOR_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, MaxRxAmpduFactor,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK ,
                 CFG_MAX_RX_AMPDU_FACTOR_DEFAULT,
                 CFG_MAX_RX_AMPDU_FACTOR_MIN,
                 CFG_MAX_RX_AMPDU_FACTOR_MAX),

   REG_VARIABLE( CFG_FIXED_RATE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, TxRate,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_FIXED_RATE_DEFAULT,
                 CFG_FIXED_RATE_MIN,
                 CFG_FIXED_RATE_MAX ),

   REG_VARIABLE( CFG_SHORT_GI_20MHZ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, ShortGI20MhzEnable,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SHORT_GI_20MHZ_DEFAULT,
                 CFG_SHORT_GI_20MHZ_MIN,
                 CFG_SHORT_GI_20MHZ_MAX ),

   REG_VARIABLE( CFG_BLOCK_ACK_AUTO_SETUP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, BlockAckAutoSetup,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_BLOCK_ACK_AUTO_SETUP_DEFAULT,
                 CFG_BLOCK_ACK_AUTO_SETUP_MIN,
                 CFG_BLOCK_ACK_AUTO_SETUP_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_COUNT_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, ScanResultAgeCount,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SCAN_RESULT_AGE_COUNT_DEFAULT,
                 CFG_SCAN_RESULT_AGE_COUNT_MIN,
                 CFG_SCAN_RESULT_AGE_COUNT_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_NCNPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeNCNPS,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SCAN_RESULT_AGE_TIME_NCNPS_DEFAULT,
                 CFG_SCAN_RESULT_AGE_TIME_NCNPS_MIN,
                 CFG_SCAN_RESULT_AGE_TIME_NCNPS_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_NCPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeNCPS,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SCAN_RESULT_AGE_TIME_NCPS_DEFAULT,
                 CFG_SCAN_RESULT_AGE_TIME_NCPS_MIN,
                 CFG_SCAN_RESULT_AGE_TIME_NCPS_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_CNPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeCNPS,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SCAN_RESULT_AGE_TIME_CNPS_DEFAULT,
                 CFG_SCAN_RESULT_AGE_TIME_CNPS_MIN,
                 CFG_SCAN_RESULT_AGE_TIME_CNPS_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_CPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeCPS,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SCAN_RESULT_AGE_TIME_CPS_DEFAULT,
                 CFG_SCAN_RESULT_AGE_TIME_CPS_MIN,
                 CFG_SCAN_RESULT_AGE_TIME_CPS_MAX ),

   REG_VARIABLE( CFG_RSSI_CATEGORY_GAP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRssiCatGap,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RSSI_CATEGORY_GAP_DEFAULT,
                 CFG_RSSI_CATEGORY_GAP_MIN,
                 CFG_RSSI_CATEGORY_GAP_MAX ),

   REG_VARIABLE( CFG_SHORT_PREAMBLE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsShortPreamble,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SHORT_PREAMBLE_DEFAULT,
                 CFG_SHORT_PREAMBLE_MIN,
                 CFG_SHORT_PREAMBLE_MAX ),

   REG_VARIABLE( CFG_IBSS_AUTO_BSSID_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsAutoIbssBssid,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_IBSS_AUTO_BSSID_DEFAULT,
                 CFG_IBSS_AUTO_BSSID_MIN,
                 CFG_IBSS_AUTO_BSSID_MAX ),

   REG_VARIABLE_STRING( CFG_IBSS_BSSID_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, IbssBssid,
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_IBSS_BSSID_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF0_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[0],
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF0_MAC_ADDR_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF1_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[1],
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF1_MAC_ADDR_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF2_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[2],
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF2_MAC_ADDR_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF3_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[3],
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF3_MAC_ADDR_DEFAULT ),

   REG_VARIABLE( CFG_AP_QOS_UAPSD_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, apUapsdEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_QOS_UAPSD_MODE_DEFAULT,
                 CFG_AP_QOS_UAPSD_MODE_MIN,
                 CFG_AP_QOS_UAPSD_MODE_MAX ),

   REG_VARIABLE_STRING( CFG_AP_COUNTRY_CODE, WLAN_PARAM_String,
                        hdd_config_t, apCntryCode,
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_AP_COUNTRY_CODE_DEFAULT ),

   REG_VARIABLE( CFG_AP_ENABLE_RANDOM_BSSID_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apRandomBssidEnabled,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_ENABLE_RANDOM_BSSID_DEFAULT,
                        CFG_AP_ENABLE_RANDOM_BSSID_MIN,
                        CFG_AP_ENABLE_RANDOM_BSSID_MAX ),

   REG_VARIABLE( CFG_AP_ENABLE_PROTECTION_MODE_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apProtEnabled,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_ENABLE_PROTECTION_MODE_DEFAULT,
                        CFG_AP_ENABLE_PROTECTION_MODE_MIN,
                        CFG_AP_ENABLE_PROTECTION_MODE_MAX ),

   REG_VARIABLE( CFG_AP_PROTECTION_MODE_NAME, WLAN_PARAM_HexInteger,
                        hdd_config_t, apProtection,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_PROTECTION_MODE_DEFAULT,
                        CFG_AP_PROTECTION_MODE_MIN,
                        CFG_AP_PROTECTION_MODE_MAX ),

   REG_VARIABLE( CFG_AP_OBSS_PROTECTION_MODE_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apOBSSProtEnabled,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_OBSS_PROTECTION_MODE_DEFAULT,
                        CFG_AP_OBSS_PROTECTION_MODE_MIN,
                        CFG_AP_OBSS_PROTECTION_MODE_MAX ),

   REG_VARIABLE( CFG_AP_STA_SECURITY_SEPERATION_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apDisableIntraBssFwd,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_STA_SECURITY_SEPERATION_DEFAULT,
                        CFG_AP_STA_SECURITY_SEPERATION_MIN,
                        CFG_AP_STA_SECURITY_SEPERATION_MAX ),

   REG_VARIABLE( CFG_FRAMES_PROCESSING_TH_MODE_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, MinFramesProcThres,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_FRAMES_PROCESSING_TH_DEFAULT,
                        CFG_FRAMES_PROCESSING_TH_MIN,
                        CFG_FRAMES_PROCESSING_TH_MAX ),

   REG_VARIABLE(CFG_SAP_CHANNEL_SELECT_START_CHANNEL , WLAN_PARAM_Integer,
                 hdd_config_t, apStartChannelNum,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_START_CHANNEL_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_START_CHANNEL_MIN,
                 CFG_SAP_CHANNEL_SELECT_START_CHANNEL_MAX ),

   REG_VARIABLE(CFG_SAP_CHANNEL_SELECT_END_CHANNEL , WLAN_PARAM_Integer,
                 hdd_config_t, apEndChannelNum,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_END_CHANNEL_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_END_CHANNEL_MIN,
                 CFG_SAP_CHANNEL_SELECT_END_CHANNEL_MAX ),

   REG_VARIABLE(CFG_SAP_CHANNEL_SELECT_OPERATING_BAND , WLAN_PARAM_Integer,
                 hdd_config_t, apOperatingBand,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_MIN,
                 CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_MAX ),

   REG_VARIABLE(CFG_SAP_AUTO_CHANNEL_SELECTION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, apAutoChannelSelection,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_AUTO_CHANNEL_SELECTION_DEFAULT,
                 CFG_SAP_AUTO_CHANNEL_SELECTION_MIN,
                 CFG_SAP_AUTO_CHANNEL_SELECTION_MAX ),

   REG_VARIABLE(CFG_ENABLE_LTE_COEX , WLAN_PARAM_Integer,
                 hdd_config_t, enableLTECoex,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_LTE_COEX_DEFAULT,
                 CFG_ENABLE_LTE_COEX_MIN,
                 CFG_ENABLE_LTE_COEX_MAX ),

   REG_VARIABLE( CFG_AP_KEEP_ALIVE_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, apKeepAlivePeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_KEEP_ALIVE_PERIOD_DEFAULT,
                 CFG_AP_KEEP_ALIVE_PERIOD_MIN,
                 CFG_AP_KEEP_ALIVE_PERIOD_MAX),

   REG_VARIABLE( CFG_GO_KEEP_ALIVE_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, goKeepAlivePeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GO_KEEP_ALIVE_PERIOD_DEFAULT,
                 CFG_GO_KEEP_ALIVE_PERIOD_MIN,
                 CFG_GO_KEEP_ALIVE_PERIOD_MAX),

   REG_VARIABLE( CFG_AP_LINK_MONITOR_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, apLinkMonitorPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_LINK_MONITOR_PERIOD_DEFAULT,
                 CFG_AP_LINK_MONITOR_PERIOD_MIN,
                 CFG_AP_LINK_MONITOR_PERIOD_MAX),

   REG_VARIABLE( CFG_GO_LINK_MONITOR_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, goLinkMonitorPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GO_LINK_MONITOR_PERIOD_DEFAULT,
                 CFG_GO_LINK_MONITOR_PERIOD_MIN,
                 CFG_GO_LINK_MONITOR_PERIOD_MAX),

   REG_VARIABLE(CFG_DISABLE_PACKET_FILTER , WLAN_PARAM_Integer,
                 hdd_config_t, disablePacketFilter,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DISABLE_PACKET_FILTER_DEFAULT,
                 CFG_DISABLE_PACKET_FILTER_MIN,
                 CFG_DISABLE_PACKET_FILTER_MAX ),

   REG_VARIABLE( CFG_BEACON_INTERVAL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBeaconInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_BEACON_INTERVAL_DEFAULT,
                 CFG_BEACON_INTERVAL_MIN,
                 CFG_BEACON_INTERVAL_MAX ),

   REG_VARIABLE( CFG_ENABLE_IDLE_SCAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nEnableIdleScan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_IDLE_SCAN_DEFAULT,
                 CFG_ENABLE_IDLE_SCAN_MIN,
                 CFG_ENABLE_IDLE_SCAN_MAX ),

   REG_VARIABLE( CFG_ROAMING_TIME_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nRoamingTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ROAMING_TIME_DEFAULT,
                 CFG_ROAMING_TIME_MIN,
                 CFG_ROAMING_TIME_MAX ),

   REG_VARIABLE( CFG_VCC_RSSI_TRIGGER_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nVccRssiTrigger,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_VCC_RSSI_TRIGGER_DEFAULT,
                 CFG_VCC_RSSI_TRIGGER_MIN,
                 CFG_VCC_RSSI_TRIGGER_MAX ),

   REG_VARIABLE( CFG_VCC_UL_MAC_LOSS_THRESH_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nVccUlMacLossThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_VCC_UL_MAC_LOSS_THRESH_DEFAULT,
                 CFG_VCC_UL_MAC_LOSS_THRESH_MIN,
                 CFG_VCC_UL_MAC_LOSS_THRESH_MAX ),

   REG_VARIABLE( CFG_PASSIVE_MAX_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nPassiveMaxChnTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_PASSIVE_MAX_CHANNEL_TIME_DEFAULT,
                 CFG_PASSIVE_MAX_CHANNEL_TIME_MIN,
                 CFG_PASSIVE_MAX_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_PASSIVE_MIN_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nPassiveMinChnTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_PASSIVE_MIN_CHANNEL_TIME_DEFAULT,
                 CFG_PASSIVE_MIN_CHANNEL_TIME_MIN,
                 CFG_PASSIVE_MIN_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MAX_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMaxChnTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_DEFAULT,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_MIN,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MIN_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMinChnTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_DEFAULT,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_MIN,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMaxChnTimeBtc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_DEFAULT,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_MIN,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_BTC_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMinChnTimeBtc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_DEFAULT,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_MIN,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_BTC_MAX ),

   REG_VARIABLE( CFG_RETRY_LIMIT_ZERO_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, retryLimitZero,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RETRY_LIMIT_ZERO_DEFAULT,
                 CFG_RETRY_LIMIT_ZERO_MIN,
                 CFG_RETRY_LIMIT_ZERO_MAX ),

   REG_VARIABLE( CFG_RETRY_LIMIT_ONE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, retryLimitOne,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RETRY_LIMIT_ONE_DEFAULT,
                 CFG_RETRY_LIMIT_ONE_MIN,
                 CFG_RETRY_LIMIT_ONE_MAX ),

   REG_VARIABLE( CFG_RETRY_LIMIT_TWO_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, retryLimitTwo,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RETRY_LIMIT_TWO_DEFAULT,
                 CFG_RETRY_LIMIT_TWO_MIN,
                 CFG_RETRY_LIMIT_TWO_MAX ),

   REG_VARIABLE( CFG_DISABLE_AGG_WITH_BTC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, disableAggWithBtc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DISABLE_AGG_WITH_BTC_DEFAULT,
                 CFG_DISABLE_AGG_WITH_BTC_MIN,
                 CFG_DISABLE_AGG_WITH_BTC_MAX ),

#ifdef WLAN_AP_STA_CONCURRENCY
   REG_VARIABLE( CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nPassiveMaxChnTimeConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_DEFAULT,
                 CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MIN,
                 CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MAX ),

   REG_VARIABLE( CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nPassiveMinChnTimeConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_DEFAULT,
                 CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MIN,
                 CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMaxChnTimeConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_DEFAULT,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MIN,
                 CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMinChnTimeConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_DEFAULT,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MIN,
                 CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MAX ),

   REG_VARIABLE( CFG_REST_TIME_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRestTimeConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_REST_TIME_CONC_DEFAULT,
                 CFG_REST_TIME_CONC_MIN,
                 CFG_REST_TIME_CONC_MAX ),

   REG_VARIABLE( CFG_NUM_STA_CHAN_COMBINED_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNumStaChanCombinedConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NUM_STA_CHAN_COMBINED_CONC_DEFAULT,
                 CFG_NUM_STA_CHAN_COMBINED_CONC_MIN,
                 CFG_NUM_STA_CHAN_COMBINED_CONC_MAX ),

   REG_VARIABLE( CFG_NUM_P2P_CHAN_COMBINED_CONC_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNumP2PChanCombinedConc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NUM_P2P_CHAN_COMBINED_CONC_DEFAULT,
                 CFG_NUM_P2P_CHAN_COMBINED_CONC_MIN,
                 CFG_NUM_P2P_CHAN_COMBINED_CONC_MAX ),
#endif

   REG_VARIABLE( CFG_MAX_PS_POLL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nMaxPsPoll,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MAX_PS_POLL_DEFAULT,
                 CFG_MAX_PS_POLL_MIN,
                 CFG_MAX_PS_POLL_MAX ),

    REG_VARIABLE( CFG_MAX_TX_POWER_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nTxPowerCap,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MAX_TX_POWER_DEFAULT,
                 CFG_MAX_TX_POWER_MIN,
                 CFG_MAX_TX_POWER_MAX ),

   REG_VARIABLE( CFG_LOW_GAIN_OVERRIDE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsLowGainOverride,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_LOW_GAIN_OVERRIDE_DEFAULT,
                 CFG_LOW_GAIN_OVERRIDE_MIN,
                 CFG_LOW_GAIN_OVERRIDE_MAX ),

   REG_VARIABLE( CFG_RSSI_FILTER_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRssiFilterPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RSSI_FILTER_PERIOD_DEFAULT,
                 CFG_RSSI_FILTER_PERIOD_MIN,
                 CFG_RSSI_FILTER_PERIOD_MAX ),

   REG_VARIABLE( CFG_IGNORE_DTIM_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIgnoreDtim,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_IGNORE_DTIM_DEFAULT,
                 CFG_IGNORE_DTIM_MIN,
                 CFG_IGNORE_DTIM_MAX ),

   REG_VARIABLE( CFG_MAX_LI_MODULATED_DTIM_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fMaxLIModulatedDTIM,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MAX_LI_MODULATED_DTIM_DEFAULT,
                 CFG_MAX_LI_MODULATED_DTIM_MIN,
                 CFG_MAX_LI_MODULATED_DTIM_MAX ),

   REG_VARIABLE( CFG_RX_ANT_CONFIGURATION_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nRxAnt,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_RX_ANT_CONFIGURATION_NAME_DEFAULT,
                CFG_RX_ANT_CONFIGURATION_NAME_MIN,
                CFG_RX_ANT_CONFIGURATION_NAME_MAX ),

   REG_VARIABLE( CFG_FW_HEART_BEAT_MONITORING_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableFwHeartBeatMonitoring,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_FW_HEART_BEAT_MONITORING_DEFAULT,
                CFG_FW_HEART_BEAT_MONITORING_MIN,
                CFG_FW_HEART_BEAT_MONITORING_MAX ),

   REG_VARIABLE( CFG_FW_BEACON_FILTERING_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableFwBeaconFiltering,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_FW_BEACON_FILTERING_DEFAULT,
                CFG_FW_BEACON_FILTERING_MIN,
                CFG_FW_BEACON_FILTERING_MAX ),

   REG_DYNAMIC_VARIABLE( CFG_FW_RSSI_MONITORING_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableFwRssiMonitoring,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_FW_RSSI_MONITORING_DEFAULT,
                CFG_FW_RSSI_MONITORING_MIN,
                CFG_FW_RSSI_MONITORING_MAX,
                cbNotifySetFwRssiMonitoring, 0 ),

   REG_VARIABLE( CFG_DATA_INACTIVITY_TIMEOUT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nDataInactivityTimeout,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_DATA_INACTIVITY_TIMEOUT_DEFAULT,
                CFG_DATA_INACTIVITY_TIMEOUT_MIN,
                CFG_DATA_INACTIVITY_TIMEOUT_MAX ),

   REG_VARIABLE( CFG_NTH_BEACON_FILTER_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nthBeaconFilter,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_NTH_BEACON_FILTER_DEFAULT,
                CFG_NTH_BEACON_FILTER_MIN,
                CFG_NTH_BEACON_FILTER_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WmmMode,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_MODE_DEFAULT,
                 CFG_QOS_WMM_MODE_MIN,
                 CFG_QOS_WMM_MODE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_80211E_ENABLED_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, b80211eIsEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_80211E_ENABLED_DEFAULT,
                 CFG_QOS_WMM_80211E_ENABLED_MIN,
                 CFG_QOS_WMM_80211E_ENABLED_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_UAPSD_MASK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, UapsdMask,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_UAPSD_MASK_DEFAULT,
                 CFG_QOS_WMM_UAPSD_MASK_MIN,
                 CFG_QOS_WMM_UAPSD_MASK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdVoSrvIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdVoSuspIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdViSrvIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdViSuspIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBeSrvIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBeSuspIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBkSrvIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBkSuspIntv,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_DEFAULT,
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MIN,
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MAX ),

#ifdef FEATURE_WLAN_ESE
   REG_VARIABLE( CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, InfraInactivityInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_DEFAULT,
                 CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MIN,
                 CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MAX),
   REG_DYNAMIC_VARIABLE( CFG_ESE_FEATURE_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isEseIniFeatureEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ESE_FEATURE_ENABLED_DEFAULT,
                 CFG_ESE_FEATURE_ENABLED_MIN,
                 CFG_ESE_FEATURE_ENABLED_MAX,
                 cbNotifySetEseFeatureEnabled, 0 ),
#endif // FEATURE_WLAN_ESE

#ifdef FEATURE_WLAN_LFR
   // flag to turn ON/OFF Legacy Fast Roaming
   REG_DYNAMIC_VARIABLE( CFG_LFR_FEATURE_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isFastRoamIniFeatureEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_LFR_FEATURE_ENABLED_DEFAULT,
                 CFG_LFR_FEATURE_ENABLED_MIN,
                 CFG_LFR_FEATURE_ENABLED_MAX,
                 NotifyIsFastRoamIniFeatureEnabled, 0 ),

   /* flag to turn ON/OFF Motion assistance for Legacy Fast Roaming */
   REG_DYNAMIC_VARIABLE( CFG_LFR_MAWC_FEATURE_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, MAWCEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_LFR_MAWC_FEATURE_ENABLED_DEFAULT,
                 CFG_LFR_MAWC_FEATURE_ENABLED_MIN,
                 CFG_LFR_MAWC_FEATURE_ENABLED_MAX,
                 NotifyIsMAWCIniFeatureEnabled, 0 ),

#endif // FEATURE_WLAN_LFR

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
   // flag to turn ON/OFF 11r and ESE FastTransition
   REG_DYNAMIC_VARIABLE( CFG_FAST_TRANSITION_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isFastTransitionEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT,
                 CFG_FAST_TRANSITION_ENABLED_NAME_MIN,
                 CFG_FAST_TRANSITION_ENABLED_NAME_MAX,
                 cbNotifySetFastTransitionEnabled, 0 ),

   /* Variable to specify the delta/difference between the RSSI of current AP
    * and roamable AP while roaming */
   REG_DYNAMIC_VARIABLE( CFG_ROAM_RSSI_DIFF_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, RoamRssiDiff,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ROAM_RSSI_DIFF_DEFAULT,
                 CFG_ROAM_RSSI_DIFF_MIN,
                 CFG_ROAM_RSSI_DIFF_MAX,
                 cbNotifySetRoamRssiDiff, 0),

   REG_DYNAMIC_VARIABLE( CFG_IMMEDIATE_ROAM_RSSI_DIFF_NAME, WLAN_PARAM_Integer,
                         hdd_config_t, nImmediateRoamRssiDiff,
                         VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                         CFG_IMMEDIATE_ROAM_RSSI_DIFF_DEFAULT,
                         CFG_IMMEDIATE_ROAM_RSSI_DIFF_MIN,
                         CFG_IMMEDIATE_ROAM_RSSI_DIFF_MAX,
                         cbNotifySetImmediateRoamRssiDiff, 0),

   REG_DYNAMIC_VARIABLE( CFG_ENABLE_WES_MODE_NAME, WLAN_PARAM_Integer,
                         hdd_config_t, isWESModeEnabled,
                         VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                         CFG_ENABLE_WES_MODE_NAME_DEFAULT,
                         CFG_ENABLE_WES_MODE_NAME_MIN,
                         CFG_ENABLE_WES_MODE_NAME_MAX,
                         cbNotifySetWESMode, 0),
#endif
#ifdef FEATURE_WLAN_OKC
   REG_DYNAMIC_VARIABLE( CFG_OKC_FEATURE_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isOkcIniFeatureEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_OKC_FEATURE_ENABLED_DEFAULT,
                 CFG_OKC_FEATURE_ENABLED_MIN,
                 CFG_OKC_FEATURE_ENABLED_MAX,
                 cbNotifySetOkcFeatureEnabled, 0 ),
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   REG_DYNAMIC_VARIABLE( CFG_ROAM_SCAN_OFFLOAD_ENABLED, WLAN_PARAM_Integer,
                         hdd_config_t, isRoamOffloadScanEnabled,
                         VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                         CFG_ROAM_SCAN_OFFLOAD_ENABLED_DEFAULT,
                         CFG_ROAM_SCAN_OFFLOAD_ENABLED_MIN,
                         CFG_ROAM_SCAN_OFFLOAD_ENABLED_MAX,
                         cbNotifyUpdateRoamScanOffloadEnabled, 0),
#endif
   REG_VARIABLE( CFG_QOS_WMM_PKT_CLASSIFY_BASIS_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, PktClassificationBasis,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_PKT_CLASSIFY_BASIS_DEFAULT,
                 CFG_QOS_WMM_PKT_CLASSIFY_BASIS_MIN,
                 CFG_QOS_WMM_PKT_CLASSIFY_BASIS_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_VO_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcVo,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_VO_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_VO_MIN,
                 CFG_QOS_WMM_INFRA_DIR_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcVo,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MIN,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcVo,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MIN,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcVo,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MIN,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcVo,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_VO_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_VO_MIN,
                 CFG_QOS_WMM_INFRA_SBA_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_VI_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcVi,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_VI_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_VI_MIN,
                 CFG_QOS_WMM_INFRA_DIR_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcVi,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MIN,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcVi,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MIN,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcVi,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MIN,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcVi,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_VI_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_VI_MIN,
                 CFG_QOS_WMM_INFRA_SBA_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_BE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcBe,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_BE_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_BE_MIN,
                 CFG_QOS_WMM_INFRA_DIR_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcBe,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MIN,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcBe,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MIN,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcBe,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MIN,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcBe,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_BE_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_BE_MIN,
                 CFG_QOS_WMM_INFRA_SBA_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_BK_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcBk,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_BK_DEFAULT,
                 CFG_QOS_WMM_INFRA_DIR_AC_BK_MIN,
                 CFG_QOS_WMM_INFRA_DIR_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcBk,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_DEFAULT,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MIN,
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcBk,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_DEFAULT,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MIN,
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcBk,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_DEFAULT,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MIN,
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcBk,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_BK_DEFAULT,
                 CFG_QOS_WMM_INFRA_SBA_AC_BK_MIN,
                 CFG_QOS_WMM_INFRA_SBA_AC_BK_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_BK_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqBkWeight,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_TL_WFQ_BK_WEIGHT_DEFAULT,
                 CFG_TL_WFQ_BK_WEIGHT_MIN,
                 CFG_TL_WFQ_BK_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_BE_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqBeWeight,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_TL_WFQ_BE_WEIGHT_DEFAULT,
                 CFG_TL_WFQ_BE_WEIGHT_MIN,
                 CFG_TL_WFQ_BE_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_VI_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqViWeight,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_TL_WFQ_VI_WEIGHT_DEFAULT,
                 CFG_TL_WFQ_VI_WEIGHT_MIN,
                 CFG_TL_WFQ_VI_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_VO_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqVoWeight,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_TL_WFQ_VO_WEIGHT_DEFAULT,
                 CFG_TL_WFQ_VO_WEIGHT_MIN,
                 CFG_TL_WFQ_VO_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_DELAYED_TRGR_FRM_INT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, DelayedTriggerFrmInt,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_TL_DELAYED_TRGR_FRM_INT_DEFAULT,
                 CFG_TL_DELAYED_TRGR_FRM_INT_MIN,
                 CFG_TL_DELAYED_TRGR_FRM_INT_MAX ),

   REG_VARIABLE( CFG_REORDER_TIME_BK_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, BkReorderAgingTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_REORDER_TIME_BK_DEFAULT,
                 CFG_REORDER_TIME_BK_MIN,
                 CFG_REORDER_TIME_BK_MAX ),

   REG_VARIABLE( CFG_REORDER_TIME_BE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, BeReorderAgingTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_REORDER_TIME_BE_DEFAULT,
                 CFG_REORDER_TIME_BE_MIN,
                 CFG_REORDER_TIME_BE_MAX ),

   REG_VARIABLE( CFG_REORDER_TIME_VI_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, ViReorderAgingTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_REORDER_TIME_VI_DEFAULT,
                 CFG_REORDER_TIME_VI_MIN,
                 CFG_REORDER_TIME_VI_MAX ),

   REG_VARIABLE( CFG_REORDER_TIME_VO_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, VoReorderAgingTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_REORDER_TIME_VO_DEFAULT,
                 CFG_REORDER_TIME_VO_MIN,
                 CFG_REORDER_TIME_VO_MAX ),

   REG_VARIABLE_STRING( CFG_WOWL_PATTERN_NAME, WLAN_PARAM_String,
                        hdd_config_t, wowlPattern,
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_WOWL_PATTERN_DEFAULT ),

   REG_VARIABLE( CFG_QOS_IMPLICIT_SETUP_ENABLED_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, bImplicitQosEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_IMPLICIT_SETUP_ENABLED_DEFAULT,
                 CFG_QOS_IMPLICIT_SETUP_ENABLED_MIN,
                 CFG_QOS_IMPLICIT_SETUP_ENABLED_MAX ),

   REG_VARIABLE( CFG_BTC_EXECUTION_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcExecutionMode,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_EXECUTION_MODE_DEFAULT,
                 CFG_BTC_EXECUTION_MODE_MIN,
                 CFG_BTC_EXECUTION_MODE_MAX ),

   REG_VARIABLE( CFG_BTC_DHCP_PROTECTION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcConsBtSlotsToBlockDuringDhcp,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_DHCP_PROTECTION_DEFAULT,
                 CFG_BTC_DHCP_PROTECTION_MIN,
                 CFG_BTC_DHCP_PROTECTION_MAX ),

   REG_VARIABLE( CFG_BTC_A2DP_DHCP_PROTECTION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcA2DPBtSubIntervalsDuringDhcp,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_A2DP_DHCP_PROTECTION_DEFAULT,
                 CFG_BTC_A2DP_DHCP_PROTECTION_MIN,
                 CFG_BTC_A2DP_DHCP_PROTECTION_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_INQ_BT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenInqBt,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_INQ_BT_DEFAULT,
                 CFG_BTC_STATIC_LEN_INQ_BT_MIN,
                 CFG_BTC_STATIC_LEN_INQ_BT_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_PAGE_BT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenPageBt,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_PAGE_BT_DEFAULT,
                 CFG_BTC_STATIC_LEN_PAGE_BT_MIN,
                 CFG_BTC_STATIC_LEN_PAGE_BT_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_CONN_BT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenConnBt,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_CONN_BT_DEFAULT,
                 CFG_BTC_STATIC_LEN_CONN_BT_MIN,
                 CFG_BTC_STATIC_LEN_CONN_BT_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_LE_BT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenLeBt,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_LE_BT_DEFAULT,
                 CFG_BTC_STATIC_LEN_LE_BT_MIN,
                 CFG_BTC_STATIC_LEN_LE_BT_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_INQ_WLAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenInqWlan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_INQ_WLAN_DEFAULT,
                 CFG_BTC_STATIC_LEN_INQ_WLAN_MIN,
                 CFG_BTC_STATIC_LEN_INQ_WLAN_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_PAGE_WLAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenPageWlan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_PAGE_WLAN_DEFAULT,
                 CFG_BTC_STATIC_LEN_PAGE_WLAN_MIN,
                 CFG_BTC_STATIC_LEN_PAGE_WLAN_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_CONN_WLAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenConnWlan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_CONN_WLAN_DEFAULT,
                 CFG_BTC_STATIC_LEN_CONN_WLAN_MIN,
                 CFG_BTC_STATIC_LEN_CONN_WLAN_MAX ),

   REG_VARIABLE( CFG_BTC_STATIC_LEN_LE_WLAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcStaticLenLeWlan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_STATIC_LEN_LE_WLAN_DEFAULT,
                 CFG_BTC_STATIC_LEN_LE_WLAN_MIN,
                 CFG_BTC_STATIC_LEN_LE_WLAN_MAX ),

   REG_VARIABLE( CFG_BTC_DYN_MAX_LEN_BT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcDynMaxLenBt,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_DYN_MAX_LEN_BT_DEFAULT,
                 CFG_BTC_DYN_MAX_LEN_BT_MIN,
                 CFG_BTC_DYN_MAX_LEN_BT_MAX ),

   REG_VARIABLE( CFG_BTC_DYN_MAX_LEN_WLAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcDynMaxLenWlan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_DYN_MAX_LEN_WLAN_DEFAULT,
                 CFG_BTC_DYN_MAX_LEN_WLAN_MIN,
                 CFG_BTC_DYN_MAX_LEN_WLAN_MAX ),

   REG_VARIABLE( CFG_BTC_MAX_SCO_BLOCK_PERC_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcMaxScoBlockPerc,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_MAX_SCO_BLOCK_PERC_DEFAULT,
                 CFG_BTC_MAX_SCO_BLOCK_PERC_MIN,
                 CFG_BTC_MAX_SCO_BLOCK_PERC_MAX ),

   REG_VARIABLE( CFG_BTC_DHCP_PROT_ON_A2DP_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcDhcpProtOnA2dp,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_DHCP_PROT_ON_A2DP_DEFAULT,
                 CFG_BTC_DHCP_PROT_ON_A2DP_MIN,
                 CFG_BTC_DHCP_PROT_ON_A2DP_MAX ),

   REG_VARIABLE( CFG_BTC_DHCP_PROT_ON_SCO_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcDhcpProtOnSco,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_DHCP_PROT_ON_SCO_DEFAULT,
                 CFG_BTC_DHCP_PROT_ON_SCO_MIN,
                 CFG_BTC_DHCP_PROT_ON_SCO_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V1_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[0],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V1_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[0],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V1_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[0],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V1_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[0],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V2_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[1],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V2_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[1],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V2_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[1],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V2_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[1],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V3_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[2],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V3_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[2],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V3_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[2],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V3_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[2],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V4_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[3],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V4_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[3],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V4_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[3],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V4_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[3],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V5_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[4],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V5_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[4],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V5_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[4],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V5_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[4],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V6_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[5],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V6_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[5],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V6_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[5],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V6_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[5],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V7_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[6],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V7_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[6],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V7_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[6],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V7_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[6],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V8_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[7],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V8_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[7],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V8_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[7],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V8_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[7],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V9_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[8],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V9_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[8],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V9_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[8],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V9_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[8],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V10_WAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWANFreq[9],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V10_WLAN_FREQ_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimWLANFreq[9],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_DEFAULT,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MIN,
                 CFG_MWS_COEX_VX_WLAN_FREQ_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V10_CONFIG_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig[9],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_V10_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexVictimConfig2[9],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_DEFAULT,
                 CFG_MWS_COEX_VX_CONFIG_MIN,
                 CFG_MWS_COEX_VX_CONFIG_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_MODEM_BACKOFF_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexModemBackoff,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_MODEM_BACKOFF_DEFAULT,
                 CFG_MWS_COEX_MODEM_BACKOFF_MIN,
                 CFG_MWS_COEX_MODEM_BACKOFF_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_CONFIG1_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexConfig[0],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_MIN,
                 CFG_MWS_COEX_CONFIGX_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_CONFIG2_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexConfig[1],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_MIN,
                 CFG_MWS_COEX_CONFIGX_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_CONFIG3_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexConfig[2],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_MIN,
                 CFG_MWS_COEX_CONFIGX_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_CONFIG4_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexConfig[3],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_MIN,
                 CFG_MWS_COEX_CONFIGX_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_CONFIG5_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexConfig[4],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_MIN,
                 CFG_MWS_COEX_CONFIGX_MAX ),

   REG_VARIABLE( CFG_MWS_COEX_CONFIG6_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, mwsCoexConfig[5],
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_DEFAULT,
                 CFG_MWS_COEX_CONFIGX_MIN,
                 CFG_MWS_COEX_CONFIGX_MAX ),

   REG_VARIABLE( CFG_SAR_POWER_BACKOFF_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, SARPowerBackoff,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAR_POWER_BACKOFF_DEFAULT,
                 CFG_SAR_POWER_BACKOFF_MIN,
                 CFG_SAR_POWER_BACKOFF_MAX ),

   REG_VARIABLE( CFG_AP_LISTEN_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nEnableListenMode,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_LISTEN_MODE_DEFAULT,
                 CFG_AP_LISTEN_MODE_MIN,
                 CFG_AP_LISTEN_MODE_MAX ),

   REG_VARIABLE( CFG_AP_AUTO_SHUT_OFF , WLAN_PARAM_Integer,
                 hdd_config_t, nAPAutoShutOff,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_AUTO_SHUT_OFF_DEFAULT,
                 CFG_AP_AUTO_SHUT_OFF_MIN,
                 CFG_AP_AUTO_SHUT_OFF_MAX ),

#if defined WLAN_FEATURE_VOWIFI
   REG_VARIABLE( CFG_RRM_ENABLE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fRrmEnable,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RRM_ENABLE_DEFAULT,
                 CFG_RRM_ENABLE_MIN,
                 CFG_RRM_ENABLE_MAX ),

   REG_VARIABLE( CFG_RRM_OPERATING_CHAN_MAX_DURATION_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nInChanMeasMaxDuration,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RRM_OPERATING_CHAN_MAX_DURATION_DEFAULT,
                 CFG_RRM_OPERATING_CHAN_MAX_DURATION_MIN,
                 CFG_RRM_OPERATING_CHAN_MAX_DURATION_MAX ),

   REG_VARIABLE( CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nOutChanMeasMaxDuration,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_DEFAULT,
                 CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_MIN,
                 CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_MAX ),

   REG_VARIABLE( CFG_RRM_MEAS_RANDOMIZATION_INTVL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRrmRandnIntvl,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_RRM_MEAS_RANDOMIZATION_INTVL_DEFAULT,
                 CFG_RRM_MEAS_RANDOMIZATION_INTVL_MIN,
                 CFG_RRM_MEAS_RANDOMIZATION_INTVL_MAX ),
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
   REG_VARIABLE( CFG_FT_RESOURCE_REQ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fFTResourceReqSupported,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_FT_RESOURCE_REQ_DEFAULT,
                 CFG_FT_RESOURCE_REQ_MIN,
                 CFG_FT_RESOURCE_REQ_MAX ),
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
   REG_DYNAMIC_VARIABLE( CFG_NEIGHBOR_SCAN_TIMER_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborScanPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT,
                 CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN,
                 CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX,
                 cbNotifySetNeighborScanPeriod, 0 ),

   REG_VARIABLE( CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborReassocRssiThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_DEFAULT,
                 CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_MIN,
                 CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_MAX ),

   REG_DYNAMIC_VARIABLE( CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborLookupRssiThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT,
                 CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN,
                 CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX,
                 cbNotifySetNeighborLookupRssiThreshold, 0 ),

   REG_VARIABLE_STRING( CFG_NEIGHBOR_SCAN_CHAN_LIST_NAME, WLAN_PARAM_String,
                        hdd_config_t, neighborScanChanList,
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_NEIGHBOR_SCAN_CHAN_LIST_DEFAULT ),

   REG_DYNAMIC_VARIABLE( CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborScanMinChanTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
                 CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX,
                 cbNotifySetNeighborScanMinChanTime, 0 ),

   REG_DYNAMIC_VARIABLE( CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborScanMaxChanTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN,
                 CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX,
                 cbNotifySetNeighborScanMaxChanTime, 0 ),

   REG_VARIABLE( CFG_11R_NEIGHBOR_REQ_MAX_TRIES_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nMaxNeighborReqTries,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_11R_NEIGHBOR_REQ_MAX_TRIES_DEFAULT,
                 CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MIN,
                 CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MAX),

   REG_DYNAMIC_VARIABLE( CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborResultsRefreshPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT,
                 CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN,
                 CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX,
                 cbNotifySetNeighborResultsRefreshPeriod, 0 ),

   REG_DYNAMIC_VARIABLE( CFG_EMPTY_SCAN_REFRESH_PERIOD_NAME, WLAN_PARAM_Integer,
                         hdd_config_t, nEmptyScanRefreshPeriod,
                         VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                         CFG_EMPTY_SCAN_REFRESH_PERIOD_DEFAULT,
                         CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN,
                         CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX,
                         cbNotifySetEmptyScanRefreshPeriod, 0 ),

    REG_VARIABLE( CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_NAME, WLAN_PARAM_Integer,
                          hdd_config_t, nNeighborInitialForcedRoamTo5GhEnable,
                          VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                          CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_DEFAULT,
                          CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_MIN,
                          CFG_NEIGHBOR_INITIAL_FORCED_ROAM_TO_5GH_ENABLE_MAX),

#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */

   REG_VARIABLE( CFG_QOS_WMM_BURST_SIZE_DEFN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, burstSizeDefinition,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_BURST_SIZE_DEFN_DEFAULT,
                 CFG_QOS_WMM_BURST_SIZE_DEFN_MIN,
                 CFG_QOS_WMM_BURST_SIZE_DEFN_MAX ),

   REG_VARIABLE( CFG_MCAST_BCAST_FILTER_SETTING_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, mcastBcastFilterSetting,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MCAST_BCAST_FILTER_SETTING_DEFAULT,
                 CFG_MCAST_BCAST_FILTER_SETTING_MIN,
                 CFG_MCAST_BCAST_FILTER_SETTING_MAX ),

   REG_VARIABLE( CFG_ENABLE_HOST_ARPOFFLOAD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fhostArpOffload,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_HOST_ARPOFFLOAD_DEFAULT,
                 CFG_ENABLE_HOST_ARPOFFLOAD_MIN,
                 CFG_ENABLE_HOST_ARPOFFLOAD_MAX ),

   REG_VARIABLE( CFG_ENABLE_HOST_NSOFFLOAD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fhostNSOffload,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_HOST_NSOFFLOAD_DEFAULT,
                 CFG_ENABLE_HOST_NSOFFLOAD_MIN,
                 CFG_ENABLE_HOST_NSOFFLOAD_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_TS_INFO_ACK_POLICY_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, tsInfoAckPolicy,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_QOS_WMM_TS_INFO_ACK_POLICY_DEFAULT,
                 CFG_QOS_WMM_TS_INFO_ACK_POLICY_MIN,
                 CFG_QOS_WMM_TS_INFO_ACK_POLICY_MAX ),

    REG_VARIABLE( CFG_SINGLE_TID_RC_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, bSingleTidRc,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_SINGLE_TID_RC_DEFAULT,
                  CFG_SINGLE_TID_RC_MIN,
                  CFG_SINGLE_TID_RC_MAX),

    REG_VARIABLE( CFG_DYNAMIC_PSPOLL_VALUE_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, dynamicPsPollValue,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_DYNAMIC_PSPOLL_VALUE_DEFAULT,
                  CFG_DYNAMIC_PSPOLL_VALUE_MIN,
                  CFG_DYNAMIC_PSPOLL_VALUE_MAX ),

   REG_VARIABLE( CFG_TELE_BCN_WAKEUP_EN_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, teleBcnWakeupEn,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_TELE_BCN_WAKEUP_EN_DEFAULT,
                  CFG_TELE_BCN_WAKEUP_EN_MIN,
                  CFG_TELE_BCN_WAKEUP_EN_MAX ),

    REG_VARIABLE( CFG_INFRA_STA_KEEP_ALIVE_PERIOD_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, infraStaKeepAlivePeriod,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_INFRA_STA_KEEP_ALIVE_PERIOD_DEFAULT,
                  CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MIN,
                  CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MAX),

    REG_VARIABLE( CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_NAME , WLAN_PARAM_Integer,
                  hdd_config_t, AddTSWhenACMIsOff,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_DEFAULT,
                  CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MIN,
                  CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MAX ),


    REG_VARIABLE( CFG_VALIDATE_SCAN_LIST_NAME , WLAN_PARAM_Integer,
                  hdd_config_t, fValidateScanList,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_VALIDATE_SCAN_LIST_DEFAULT,
                  CFG_VALIDATE_SCAN_LIST_MIN,
                  CFG_VALIDATE_SCAN_LIST_MAX ),

    REG_VARIABLE( CFG_NULLDATA_AP_RESP_TIMEOUT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nNullDataApRespTimeout,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_NULLDATA_AP_RESP_TIMEOUT_DEFAULT,
                CFG_NULLDATA_AP_RESP_TIMEOUT_MIN,
                CFG_NULLDATA_AP_RESP_TIMEOUT_MAX ),

    REG_VARIABLE( CFG_AP_DATA_AVAIL_POLL_PERIOD_NAME, WLAN_PARAM_Integer,
                hdd_config_t, apDataAvailPollPeriodInMs,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_AP_DATA_AVAIL_POLL_PERIOD_DEFAULT,
                CFG_AP_DATA_AVAIL_POLL_PERIOD_MIN,
                CFG_AP_DATA_AVAIL_POLL_PERIOD_MAX ),

   REG_VARIABLE( CFG_ENABLE_BTAMP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, enableBtAmp,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_BTAMP_DEFAULT,
                 CFG_ENABLE_BTAMP_MIN,
                 CFG_ENABLE_BTAMP_MAX ),

#ifdef WLAN_BTAMP_FEATURE
   REG_VARIABLE( CFG_BT_AMP_PREFERRED_CHANNEL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, preferredChannel,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BT_AMP_PREFERRED_CHANNEL_DEFAULT,
                 CFG_BT_AMP_PREFERRED_CHANNEL_MIN,
                 CFG_BT_AMP_PREFERRED_CHANNEL_MAX ),
#endif //WLAN_BTAMP_FEATURE
   REG_VARIABLE( CFG_BAND_CAPABILITY_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBandCapability,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BAND_CAPABILITY_DEFAULT,
                 CFG_BAND_CAPABILITY_MIN,
                 CFG_BAND_CAPABILITY_MAX ),

   REG_VARIABLE( CFG_ENABLE_BEACON_EARLY_TERMINATION_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableBeaconEarlyTermination,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_BEACON_EARLY_TERMINATION_DEFAULT,
                CFG_ENABLE_BEACON_EARLY_TERMINATION_MIN,
                CFG_ENABLE_BEACON_EARLY_TERMINATION_MAX ),
/* CFG_VOS_TRACE_ENABLE Parameters */
   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_BAP_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableBAP,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_TL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableTL,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_WDI_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableWDI,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_HDD_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableHDD,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_SME_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableSME,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_PE_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnablePE,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_PMC_NAME,  WLAN_PARAM_Integer,
                 hdd_config_t, vosTraceEnablePMC,
                 VAR_FLAGS_OPTIONAL,
                 CFG_VOS_TRACE_ENABLE_DEFAULT,
                 CFG_VOS_TRACE_ENABLE_MIN,
                 CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_WDA_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableWDA,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_SYS_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableSYS,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_VOSS_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableVOSS,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_SAP_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableSAP,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_VOS_TRACE_ENABLE_HDD_SAP_NAME, WLAN_PARAM_Integer,
                hdd_config_t, vosTraceEnableHDDSAP,
                VAR_FLAGS_OPTIONAL,
                CFG_VOS_TRACE_ENABLE_DEFAULT,
                CFG_VOS_TRACE_ENABLE_MIN,
                CFG_VOS_TRACE_ENABLE_MAX ),

   /* note that since the default value is out of range we cannot
      enable range check, otherwise we get a system log message */
   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_DAL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnableDAL,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_CTL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnableCTL,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_DAT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnableDAT,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_PAL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnablePAL,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_TELE_BCN_TRANS_LI_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnTransListenInterval,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_TELE_BCN_TRANS_LI_DEFAULT,
               CFG_TELE_BCN_TRANS_LI_MIN,
               CFG_TELE_BCN_TRANS_LI_MAX ),

   REG_VARIABLE( CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnTransLiNumIdleBeacons,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_DEFAULT,
               CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_MIN,
               CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_MAX ),

   REG_VARIABLE( CFG_TELE_BCN_MAX_LI_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnMaxListenInterval,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_TELE_BCN_MAX_LI_DEFAULT,
               CFG_TELE_BCN_MAX_LI_MIN,
               CFG_TELE_BCN_MAX_LI_MAX ),

   REG_VARIABLE( CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnMaxLiNumIdleBeacons,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_DEFAULT,
               CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_MIN,
               CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_MAX ),

   REG_VARIABLE( CFG_BCN_EARLY_TERM_WAKE_NAME, WLAN_PARAM_Integer,
               hdd_config_t, bcnEarlyTermWakeInterval,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_BCN_EARLY_TERM_WAKE_DEFAULT,
               CFG_BCN_EARLY_TERM_WAKE_MIN,
               CFG_BCN_EARLY_TERM_WAKE_MAX ),

   REG_VARIABLE( CFG_AP_DATA_AVAIL_POLL_PERIOD_NAME, WLAN_PARAM_Integer,
              hdd_config_t, apDataAvailPollPeriodInMs,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_AP_DATA_AVAIL_POLL_PERIOD_DEFAULT,
              CFG_AP_DATA_AVAIL_POLL_PERIOD_MIN,
              CFG_AP_DATA_AVAIL_POLL_PERIOD_MAX ),

   REG_VARIABLE( CFG_ENABLE_CLOSE_LOOP_NAME, WLAN_PARAM_Integer,
                hdd_config_t, enableCloseLoop,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_CLOSE_LOOP_DEFAULT,
                CFG_ENABLE_CLOSE_LOOP_MIN,
                CFG_ENABLE_CLOSE_LOOP_MAX ),

   REG_VARIABLE( CFG_ENABLE_BYPASS_11D_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableBypass11d,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_BYPASS_11D_DEFAULT,
              CFG_ENABLE_BYPASS_11D_MIN,
              CFG_ENABLE_BYPASS_11D_MAX ),

   REG_VARIABLE( CFG_ENABLE_DFS_CHNL_SCAN_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableDFSChnlScan,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_DFS_CHNL_SCAN_DEFAULT,
              CFG_ENABLE_DFS_CHNL_SCAN_MIN,
              CFG_ENABLE_DFS_CHNL_SCAN_MAX ),

   REG_VARIABLE( CFG_ENABLE_DFS_PNO_CHNL_SCAN_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableDFSPnoChnlScan,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_DFS_PNO_CHNL_SCAN_DEFAULT,
              CFG_ENABLE_DFS_PNO_CHNL_SCAN_MIN,
              CFG_ENABLE_DFS_PNO_CHNL_SCAN_MAX ),

   REG_VARIABLE( CFG_ENABLE_DYNAMIC_DTIM_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableDynamicDTIM,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_DYNAMIC_DTIM_DEFAULT,
              CFG_ENABLE_DYNAMIC_DTIM_MIN,
              CFG_ENABLE_DYNAMIC_DTIM_MAX ),

   REG_VARIABLE( CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableAutomaticTxPowerControl,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_DEFAULT,
              CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_MIN,
              CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_MAX ),

   REG_VARIABLE( CFG_SHORT_GI_40MHZ_NAME, WLAN_PARAM_Integer,
              hdd_config_t, ShortGI40MhzEnable,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_SHORT_GI_40MHZ_DEFAULT,
              CFG_SHORT_GI_40MHZ_MIN,
              CFG_SHORT_GI_40MHZ_MAX ),

   REG_DYNAMIC_VARIABLE( CFG_REPORT_MAX_LINK_SPEED, WLAN_PARAM_Integer,
                       hdd_config_t, reportMaxLinkSpeed,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_REPORT_MAX_LINK_SPEED_DEFAULT,
                       CFG_REPORT_MAX_LINK_SPEED_MIN,
                       CFG_REPORT_MAX_LINK_SPEED_MAX,
                       NULL, 0 ),

   REG_DYNAMIC_VARIABLE( CFG_LINK_SPEED_RSSI_HIGH, WLAN_PARAM_SignedInteger,
                       hdd_config_t, linkSpeedRssiHigh,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_LINK_SPEED_RSSI_HIGH_DEFAULT,
                       CFG_LINK_SPEED_RSSI_HIGH_MIN,
                       CFG_LINK_SPEED_RSSI_HIGH_MAX,
                       NULL, 0 ),

   REG_DYNAMIC_VARIABLE( CFG_LINK_SPEED_RSSI_MID, WLAN_PARAM_SignedInteger,
                       hdd_config_t, linkSpeedRssiMid,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_LINK_SPEED_RSSI_MID_DEFAULT,
                       CFG_LINK_SPEED_RSSI_MID_MIN,
                       CFG_LINK_SPEED_RSSI_MID_MAX,
                       NULL, 0 ),

   REG_DYNAMIC_VARIABLE( CFG_LINK_SPEED_RSSI_LOW, WLAN_PARAM_SignedInteger,
                       hdd_config_t, linkSpeedRssiLow,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_LINK_SPEED_RSSI_LOW_DEFAULT,
                       CFG_LINK_SPEED_RSSI_LOW_MIN,
                       CFG_LINK_SPEED_RSSI_LOW_MAX,
                       NULL, 0 ),

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
   REG_DYNAMIC_VARIABLE( CFG_ROAM_PREFER_5GHZ, WLAN_PARAM_Integer,
                       hdd_config_t, nRoamPrefer5GHz,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_ROAM_PREFER_5GHZ_DEFAULT,
                       CFG_ROAM_PREFER_5GHZ_MIN,
                       CFG_ROAM_PREFER_5GHZ_MAX,
                       cbNotifySetRoamPrefer5GHz, 0 ),

   REG_DYNAMIC_VARIABLE( CFG_ROAM_INTRA_BAND, WLAN_PARAM_Integer,
                       hdd_config_t, nRoamIntraBand,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_ROAM_INTRA_BAND_DEFAULT,
                       CFG_ROAM_INTRA_BAND_MIN,
                       CFG_ROAM_INTRA_BAND_MAX,
                       cbNotifySetRoamIntraBand, 0 ),

 REG_DYNAMIC_VARIABLE( CFG_ROAM_SCAN_N_PROBES, WLAN_PARAM_Integer,
                       hdd_config_t, nProbes,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_ROAM_SCAN_N_PROBES_DEFAULT,
                       CFG_ROAM_SCAN_N_PROBES_MIN,
                       CFG_ROAM_SCAN_N_PROBES_MAX,
                       cbNotifySetRoamScanNProbes, 0 ),

 REG_DYNAMIC_VARIABLE( CFG_ROAM_SCAN_HOME_AWAY_TIME, WLAN_PARAM_Integer,
                       hdd_config_t, nRoamScanHomeAwayTime,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_ROAM_SCAN_HOME_AWAY_TIME_DEFAULT,
                       CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN,
                       CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX,
                       cbNotifySetRoamScanHomeAwayTime, 0 ),

#endif

   REG_VARIABLE( CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_NAME, WLAN_PARAM_Integer,
              hdd_config_t, isP2pDeviceAddrAdministrated,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_DEFAULT,
              CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MIN,
              CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MAX ),

   REG_VARIABLE( CFG_ENABLE_MCC_ENABLED_NAME, WLAN_PARAM_Integer,
             hdd_config_t, enableMCC,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_ENABLE_MCC_ENABLED_DEFAULT,
             CFG_ENABLE_MCC_ENABLED_MIN,
             CFG_ENABLE_MCC_ENABLED_MAX ),

   REG_VARIABLE( CFG_ALLOW_MCC_GO_DIFF_BI_NAME, WLAN_PARAM_Integer,
             hdd_config_t, allowMCCGODiffBI,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_ALLOW_MCC_GO_DIFF_BI_DEFAULT,
             CFG_ALLOW_MCC_GO_DIFF_BI_MIN,
             CFG_ALLOW_MCC_GO_DIFF_BI_MAX ),

   REG_VARIABLE( CFG_THERMAL_MIGRATION_ENABLE_NAME, WLAN_PARAM_Integer,
              hdd_config_t, thermalMitigationEnable,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_THERMAL_MIGRATION_ENABLE_DEFAULT,
              CFG_THERMAL_MIGRATION_ENABLE_MIN,
              CFG_THERMAL_MIGRATION_ENABLE_MAX ),

   REG_VARIABLE( CFG_ENABLE_MODULATED_DTIM_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableModulatedDTIM,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_MODULATED_DTIM_DEFAULT,
              CFG_ENABLE_MODULATED_DTIM_MIN,
              CFG_ENABLE_MODULATED_DTIM_MAX ),

   REG_VARIABLE( CFG_MC_ADDR_LIST_ENABLE_NAME, WLAN_PARAM_Integer,
              hdd_config_t, fEnableMCAddrList,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_MC_ADDR_LIST_ENABLE_DEFAULT,
              CFG_MC_ADDR_LIST_ENABLE_MIN,
              CFG_MC_ADDR_LIST_ENABLE_MAX ),

#ifdef WLAN_FEATURE_11AC
   REG_VARIABLE( CFG_VHT_CHANNEL_WIDTH, WLAN_PARAM_Integer,
              hdd_config_t, vhtChannelWidth,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
              CFG_VHT_CHANNEL_WIDTH_DEFAULT,
              CFG_VHT_CHANNEL_WIDTH_MIN,
              CFG_VHT_CHANNEL_WIDTH_MAX),

   REG_VARIABLE( CFG_VHT_ENABLE_RX_MCS_8_9, WLAN_PARAM_Integer,
              hdd_config_t, vhtRxMCS,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
              CFG_VHT_ENABLE_RX_MCS_8_9_DEFAULT,
              CFG_VHT_ENABLE_RX_MCS_8_9_MIN,
              CFG_VHT_ENABLE_RX_MCS_8_9_MAX),

   REG_VARIABLE( CFG_VHT_ENABLE_TX_MCS_8_9, WLAN_PARAM_Integer,
              hdd_config_t, vhtTxMCS,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
              CFG_VHT_ENABLE_TX_MCS_8_9_DEFAULT,
              CFG_VHT_ENABLE_TX_MCS_8_9_MIN,
              CFG_VHT_ENABLE_TX_MCS_8_9_MAX),

   REG_VARIABLE( CFG_VHT_AMPDU_LEN_EXP_NAME, WLAN_PARAM_Integer,
              hdd_config_t, gVhtMaxAmpduLenExp,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_VHT_AMPDU_LEN_EXP_DEFAULT,
              CFG_VHT_AMPDU_LEN_EXP_MIN,
              CFG_VHT_AMPDU_LEN_EXP_MAX ),
#endif

   REG_VARIABLE( CFG_ENABLE_FIRST_SCAN_2G_ONLY_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableFirstScan2GOnly,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_FIRST_SCAN_2G_ONLY_DEFAULT,
              CFG_ENABLE_FIRST_SCAN_2G_ONLY_MIN,
              CFG_ENABLE_FIRST_SCAN_2G_ONLY_MAX ),

   REG_VARIABLE( CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_NAME, WLAN_PARAM_Integer,
              hdd_config_t, skipDfsChnlInP2pSearch,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_DEFAULT,
              CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_MIN,
              CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_MAX ),

   REG_VARIABLE( CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_NAME, WLAN_PARAM_Integer,
              hdd_config_t, ignoreDynamicDtimInP2pMode,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_DEFAULT,
              CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_MIN,
              CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_MAX ),

   REG_VARIABLE( CFG_NUM_BUFF_ADVERT_NAME, WLAN_PARAM_Integer,
              hdd_config_t,numBuffAdvert ,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
              CFG_NUM_BUFF_ADVERT_DEFAULT,
              CFG_NUM_BUFF_ADVERT_MIN,
              CFG_NUM_BUFF_ADVERT_MAX ),

   REG_VARIABLE( CFG_MCC_CONFIG_PARAM_NAME, WLAN_PARAM_Integer,
             hdd_config_t, configMccParam,
             VAR_FLAGS_OPTIONAL,
             CFG_MCC_CONFIG_PARAM_DEFAULT,
             CFG_MCC_CONFIG_PARAM_MIN,
             CFG_MCC_CONFIG_PARAM_MAX ),
   REG_VARIABLE( CFG_ENABLE_RX_STBC, WLAN_PARAM_Integer,
              hdd_config_t, enableRxSTBC,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_RX_STBC_DEFAULT,
              CFG_ENABLE_RX_STBC_MIN,
              CFG_ENABLE_RX_STBC_MAX ),
#ifdef FEATURE_WLAN_TDLS
   REG_VARIABLE( CFG_TDLS_SUPPORT_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableTDLSSupport,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_SUPPORT_ENABLE_DEFAULT,
              CFG_TDLS_SUPPORT_ENABLE_MIN,
              CFG_TDLS_SUPPORT_ENABLE_MAX ),

   REG_VARIABLE( CFG_TDLS_IMPLICIT_TRIGGER, WLAN_PARAM_Integer,
              hdd_config_t, fEnableTDLSImplicitTrigger,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_IMPLICIT_TRIGGER_DEFAULT,
              CFG_TDLS_IMPLICIT_TRIGGER_MIN,
              CFG_TDLS_IMPLICIT_TRIGGER_MAX ),

   REG_VARIABLE( CFG_TDLS_TX_STATS_PERIOD, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSTxStatsPeriod,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_TX_STATS_PERIOD_DEFAULT,
              CFG_TDLS_TX_STATS_PERIOD_MIN,
              CFG_TDLS_TX_STATS_PERIOD_MAX ),

   REG_VARIABLE( CFG_TDLS_TX_PACKET_THRESHOLD, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSTxPacketThreshold,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_TX_PACKET_THRESHOLD_DEFAULT,
              CFG_TDLS_TX_PACKET_THRESHOLD_MIN,
              CFG_TDLS_TX_PACKET_THRESHOLD_MAX ),

   REG_VARIABLE( CFG_TDLS_DISCOVERY_PERIOD, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSDiscoveryPeriod,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_DISCOVERY_PERIOD_DEFAULT,
              CFG_TDLS_DISCOVERY_PERIOD_MIN,
              CFG_TDLS_DISCOVERY_PERIOD_MAX ),

   REG_VARIABLE( CFG_TDLS_MAX_DISCOVERY_ATTEMPT, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSMaxDiscoveryAttempt,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_MAX_DISCOVERY_ATTEMPT_DEFAULT,
              CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MIN,
              CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MAX ),

   REG_VARIABLE( CFG_TDLS_IDLE_TIMEOUT, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSIdleTimeout,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_IDLE_TIMEOUT_DEFAULT,
              CFG_TDLS_IDLE_TIMEOUT_MIN,
              CFG_TDLS_IDLE_TIMEOUT_MAX ),

   REG_VARIABLE( CFG_TDLS_IDLE_PACKET_THRESHOLD, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSIdlePacketThreshold,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_IDLE_PACKET_THRESHOLD_DEFAULT,
              CFG_TDLS_IDLE_PACKET_THRESHOLD_MIN,
              CFG_TDLS_IDLE_PACKET_THRESHOLD_MAX ),

   REG_VARIABLE( CFG_TDLS_RSSI_HYSTERESIS, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSRSSIHysteresis,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_RSSI_HYSTERESIS_DEFAULT,
              CFG_TDLS_RSSI_HYSTERESIS_MIN,
              CFG_TDLS_RSSI_HYSTERESIS_MAX ),

   REG_VARIABLE( CFG_TDLS_RSSI_TRIGGER_THRESHOLD, WLAN_PARAM_SignedInteger,
              hdd_config_t, fTDLSRSSITriggerThreshold,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_RSSI_TRIGGER_THRESHOLD_DEFAULT,
              CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MIN,
              CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MAX ),

   REG_VARIABLE( CFG_TDLS_RSSI_TEARDOWN_THRESHOLD, WLAN_PARAM_SignedInteger,
              hdd_config_t, fTDLSRSSITeardownThreshold,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_DEFAULT,
              CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MIN,
              CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MAX ),

REG_VARIABLE( CFG_TDLS_QOS_WMM_UAPSD_MASK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, fTDLSUapsdMask,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_TDLS_QOS_WMM_UAPSD_MASK_DEFAULT,
                 CFG_TDLS_QOS_WMM_UAPSD_MASK_MIN,
                 CFG_TDLS_QOS_WMM_UAPSD_MASK_MAX ),

REG_VARIABLE( CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableTDLSBufferSta,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_DEFAULT,
              CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_MIN,
              CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_MAX ),

REG_VARIABLE( CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableTDLSOffChannel,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_DEFAULT,
              CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_MIN,
              CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_MAX ),

REG_VARIABLE( CFG_TDLS_PUAPSD_INACTIVITY_TIME, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSPuapsdInactivityTimer,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_PUAPSD_INACTIVITY_TIME_DEFAULT,
              CFG_TDLS_PUAPSD_INACTIVITY_TIME_MIN,
              CFG_TDLS_PUAPSD_INACTIVITY_TIME_MAX ),

REG_VARIABLE( CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSRxFrameThreshold,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_DEFAULT,
              CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_MIN,
              CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_MAX ),

REG_VARIABLE( CFG_TDLS_EXTERNAL_CONTROL, WLAN_PARAM_Integer,
              hdd_config_t, fTDLSExternalControl,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_EXTERNAL_CONTROL_DEFAULT,
              CFG_TDLS_EXTERNAL_CONTROL_MIN,
              CFG_TDLS_EXTERNAL_CONTROL_MAX ),

REG_VARIABLE( CFG_TDLS_WMM_MODE_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableTDLSWmmMode,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_WMM_MODE_ENABLE_DEFAULT,
              CFG_TDLS_WMM_MODE_ENABLE_MIN,
              CFG_TDLS_WMM_MODE_ENABLE_MAX ),

REG_VARIABLE( CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableTDLSScanCoexSupport,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE_DEFAULT,
              CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE_MIN,
              CFG_TDLS_SCAN_COEX_SUPPORT_ENABLE_MAX ),
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
REG_VARIABLE( CFG_LINK_LAYER_STATS_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableLLStats,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_LINK_LAYER_STATS_ENABLE_DEFAULT,
              CFG_LINK_LAYER_STATS_ENABLE_MIN,
              CFG_LINK_LAYER_STATS_ENABLE_MAX ),
#endif
#ifdef WLAN_FEATURE_EXTSCAN
REG_VARIABLE( CFG_EXTSCAN_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableEXTScan,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_EXTSCAN_ENABLE_DEFAULT,
              CFG_EXTSCAN_ENABLE_MIN,
              CFG_EXTSCAN_ENABLE_MAX ),
#endif

#ifdef WLAN_SOFTAP_VSTA_FEATURE
   REG_VARIABLE( CFG_VSTA_SUPPORT_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableVSTASupport,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_VSTA_SUPPORT_ENABLE_DEFAULT,
              CFG_VSTA_SUPPORT_ENABLE_MIN,
              CFG_VSTA_SUPPORT_ENABLE_MAX ),
#endif
   REG_VARIABLE( CFG_ENABLE_LPWR_IMG_TRANSITION_NAME, WLAN_PARAM_Integer,
             hdd_config_t, enableLpwrImgTransition,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_ENABLE_LPWR_IMG_TRANSITION_DEFAULT,
             CFG_ENABLE_LPWR_IMG_TRANSITION_MIN,
             CFG_ENABLE_LPWR_IMG_TRANSITION_MAX ),

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
   REG_VARIABLE( CFG_ACTIVEMODE_OFFLOAD_ENABLE, WLAN_PARAM_Integer,
              hdd_config_t, fEnableActiveModeOffload,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ACTIVEMODE_OFFLOAD_ENABLE_DEFAULT,
              CFG_ACTIVEMODE_OFFLOAD_ENABLE_MIN,
              CFG_ACTIVEMODE_OFFLOAD_ENABLE_MAX ),
#endif

   REG_VARIABLE( CFG_SCAN_AGING_PARAM_NAME, WLAN_PARAM_Integer,
             hdd_config_t, scanAgingTimeout,
             VAR_FLAGS_OPTIONAL,
             CFG_SCAN_AGING_PARAM_DEFAULT,
             CFG_SCAN_AGING_PARAM_MIN,
             CFG_SCAN_AGING_PARAM_MAX ),

   REG_VARIABLE( CFG_TX_LDPC_ENABLE_FEATURE, WLAN_PARAM_Integer,
              hdd_config_t, enableTxLdpc,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_TX_LDPC_ENABLE_FEATURE_DEFAULT,
              CFG_TX_LDPC_ENABLE_FEATURE_MIN,
              CFG_TX_LDPC_ENABLE_FEATURE_MAX ),

   REG_VARIABLE( CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_NAME, WLAN_PARAM_Integer,
             hdd_config_t, enableMCCAdaptiveScheduler,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_DEFAULT,
             CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_MIN,
             CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_MAX ),

   REG_VARIABLE( CFG_ANDRIOD_POWER_SAVE_NAME, WLAN_PARAM_Integer,
              hdd_config_t, isAndroidPsEn,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ANDRIOD_POWER_SAVE_DEFAULT,
              CFG_ANDRIOD_POWER_SAVE_MIN,
              CFG_ANDRIOD_POWER_SAVE_MAX),

   REG_VARIABLE( CFG_IBSS_ADHOC_CHANNEL_5GHZ_NAME, WLAN_PARAM_Integer,
              hdd_config_t, AdHocChannel5G,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_IBSS_ADHOC_CHANNEL_5GHZ_DEFAULT,
              CFG_IBSS_ADHOC_CHANNEL_5GHZ_MIN,
              CFG_IBSS_ADHOC_CHANNEL_5GHZ_MAX),

   REG_VARIABLE( CFG_IBSS_ADHOC_CHANNEL_24GHZ_NAME, WLAN_PARAM_Integer,
              hdd_config_t, AdHocChannel24G,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_IBSS_ADHOC_CHANNEL_24GHZ_DEFAULT,
              CFG_IBSS_ADHOC_CHANNEL_24GHZ_MIN,
              CFG_IBSS_ADHOC_CHANNEL_24GHZ_MAX),


#ifdef WLAN_FEATURE_11AC
   REG_VARIABLE( CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE, WLAN_PARAM_Integer,
             hdd_config_t, enableTxBF,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_DEFAULT,
             CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_MIN,
             CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_MAX ),

   REG_VARIABLE( CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED, WLAN_PARAM_Integer,
             hdd_config_t, txBFCsnValue,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_DEFAULT,
             CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_MIN,
             CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_MAX ),
   REG_VARIABLE( CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE, WLAN_PARAM_Integer,
             hdd_config_t, enableMuBformee,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_DEFAULT,
             CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_MIN,
             CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_MAX ),

#endif

   REG_VARIABLE( CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_NAME, WLAN_PARAM_Integer,
             hdd_config_t, sapAllowAllChannel,
             VAR_FLAGS_OPTIONAL,
             CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_DEFAULT,
             CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_MIN,
             CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_MAX ),

#ifdef WLAN_FEATURE_11AC
   REG_VARIABLE( CFG_DISABLE_LDPC_WITH_TXBF_AP, WLAN_PARAM_Integer,
             hdd_config_t, disableLDPCWithTxbfAP,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_DISABLE_LDPC_WITH_TXBF_AP_DEFAULT,
             CFG_DISABLE_LDPC_WITH_TXBF_AP_MIN,
             CFG_DISABLE_LDPC_WITH_TXBF_AP_MAX ),
#endif

   REG_VARIABLE_STRING( CFG_LIST_OF_NON_DFS_COUNTRY_CODE, WLAN_PARAM_String,
             hdd_config_t, listOfNonDfsCountryCode,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             (void *)CFG_LIST_OF_NON_DFS_COUNTRY_CODE_DEFAULT),

   REG_DYNAMIC_VARIABLE( CFG_ENABLE_SSR, WLAN_PARAM_Integer,
                hdd_config_t, enableSSR,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_SSR_DEFAULT,
                CFG_ENABLE_SSR_MIN,
                CFG_ENABLE_SSR_MAX,
                cbNotifySetEnableSSR, 0 ),

   REG_VARIABLE_STRING( CFG_LIST_OF_NON_11AC_COUNTRY_CODE, WLAN_PARAM_String,
             hdd_config_t, listOfNon11acCountryCode,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             (void *)CFG_LIST_OF_NON_11AC_COUNTRY_CODE_DEFAULT),

   REG_VARIABLE(CFG_MAX_MEDIUM_TIME, WLAN_PARAM_Integer,
             hdd_config_t, cfgMaxMediumTime,
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
             CFG_MAX_MEDIUM_TIME_STADEFAULT,
             CFG_MAX_MEDIUM_TIME_STAMIN,
             CFG_MAX_MEDIUM_TIME_STAMAX ),

   REG_VARIABLE( CFG_ENABLE_TRAFFIC_MONITOR, WLAN_PARAM_Integer,
                hdd_config_t, enableTrafficMonitor,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_TRAFFIC_MONITOR_DEFAULT,
                CFG_ENABLE_TRAFFIC_MONITOR_MIN,
                CFG_ENABLE_TRAFFIC_MONITOR_MAX),

   REG_VARIABLE( CFG_TRAFFIC_IDLE_TIMEOUT, WLAN_PARAM_Integer,
                hdd_config_t, trafficIdleTimeout,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_TRAFFIC_IDLE_TIMEOUT_DEFAULT,
                CFG_TRAFFIC_IDLE_TIMEOUT_MIN,
                CFG_TRAFFIC_IDLE_TIMEOUT_MAX),

#ifdef WLAN_FEATURE_11AC
   REG_VARIABLE( CFG_ENABLE_VHT_FOR_24GHZ_NAME, WLAN_PARAM_Integer,
             hdd_config_t, enableVhtFor24GHzBand,
             VAR_FLAGS_OPTIONAL,
             CFG_ENABLE_VHT_FOR_24GHZ_DEFAULT,
             CFG_ENABLE_VHT_FOR_24GHZ_MIN,
             CFG_ENABLE_VHT_FOR_24GHZ_MAX),
#endif

   REG_VARIABLE( CFG_SCAN_OFFLOAD_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fScanOffload,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_SCAN_OFFLOAD_DEFAULT,
                CFG_SCAN_OFFLOAD_DISABLE,
                CFG_SCAN_OFFLOAD_ENABLE ),

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   REG_DYNAMIC_VARIABLE( CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY, WLAN_PARAM_Integer,
                        hdd_config_t, bFastRoamInConIniFeatureEnabled,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_DEFAULT,
                        CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_MIN,
                        CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_MAX,
                        cbNotifySetEnableFastRoamInConcurrency, 0 ),
#endif

   REG_VARIABLE( CFG_ENABLE_ADAPT_RX_DRAIN_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnableAdaptRxDrain,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK ,
                 CFG_ENABLE_ADAPT_RX_DRAIN_DEFAULT,
                 CFG_ENABLE_ADAPT_RX_DRAIN_MIN,
                 CFG_ENABLE_ADAPT_RX_DRAIN_MAX),

   REG_VARIABLE( CFG_DYNAMIC_SPLIT_SCAN_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, dynSplitscan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DYNAMIC_SPLIT_SCAN_DEFAULT,
                 CFG_DYNAMIC_SPLIT_SCAN_MIN,
                 CFG_DYNAMIC_SPLIT_SCAN_MAX ),

   REG_VARIABLE( CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, txRxThresholdForSplitScan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_DEFAULT,
                 CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_MIN,
                 CFG_SPLIT_SCAN_TRAFFIC_MONITOR_THRSHLD_MAX ),

   REG_VARIABLE( CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, trafficMntrTmrForSplitScan,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_DEFAULT,
                 CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_MIN,
                 CFG_SPLIT_SCAN_TRAFFIC_MONITOR_TIMER_MAX ),

   REG_VARIABLE( CFG_FLEX_CONNECT_POWER_FACTOR_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, flexConnectPowerFactor,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_MINMAX,
                 CFG_FLEX_CONNECT_POWER_FACTOR_DEFAULT,
                 CFG_FLEX_CONNECT_POWER_FACTOR_MIN,
                 CFG_FLEX_CONNECT_POWER_FACTOR_MAX ),

   REG_VARIABLE( CFG_ENABLE_HEART_BEAT_OFFLOAD, WLAN_PARAM_Integer,
                 hdd_config_t, enableIbssHeartBeatOffload,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_HEART_BEAT_OFFLOAD_DEFAULT,
                 CFG_ENABLE_HEART_BEAT_OFFLOAD_MIN,
                 CFG_ENABLE_HEART_BEAT_OFFLOAD_MAX),

   REG_VARIABLE( CFG_ANTENNA_DIVERSITY_PARAM_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, antennaDiversity,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ANTENNA_DIVERSITY_PARAM_DEFAULT,
                 CFG_ANTENNA_DIVERSITY_PARAM_MIN,
                 CFG_ANTENNA_DIVERSITY_PARAM_MAX),

   REG_VARIABLE( CFG_ENABLE_SNR_MONITORING_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnableSNRMonitoring,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK ,
                 CFG_ENABLE_SNR_MONITORING_DEFAULT,
                 CFG_ENABLE_SNR_MONITORING_MIN,
                 CFG_ENABLE_SNR_MONITORING_MAX),

#ifdef FEATURE_WLAN_SCAN_PNO
   REG_VARIABLE( CFG_PNO_SCAN_SUPPORT, WLAN_PARAM_Integer,
                 hdd_config_t, configPNOScanSupport,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_PNO_SCAN_SUPPORT_DEFAULT,
                 CFG_PNO_SCAN_SUPPORT_DISABLE,
                 CFG_PNO_SCAN_SUPPORT_ENABLE),

   REG_VARIABLE( CFG_PNO_SCAN_TIMER_REPEAT_VALUE, WLAN_PARAM_Integer,
                 hdd_config_t, configPNOScanTimerRepeatValue,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_PNO_SCAN_TIMER_REPEAT_VALUE_DEFAULT,
                 CFG_PNO_SCAN_TIMER_REPEAT_VALUE_MIN,
                 CFG_PNO_SCAN_TIMER_REPEAT_VALUE_MAX),
#endif
   REG_VARIABLE( CFG_AMSDU_SUPPORT_IN_AMPDU_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, isAmsduSupportInAMPDU,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AMSDU_SUPPORT_IN_AMPDU_DEFAULT,
                 CFG_AMSDU_SUPPORT_IN_AMPDU_MIN,
                 CFG_AMSDU_SUPPORT_IN_AMPDU_MAX ),

   REG_VARIABLE( CFG_STRICT_5GHZ_PREF_BY_MARGIN , WLAN_PARAM_Integer,
                 hdd_config_t, nSelect5GHzMargin,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_STRICT_5GHZ_PREF_BY_MARGIN_DEFAULT,
                 CFG_STRICT_5GHZ_PREF_BY_MARGIN_MIN,
                 CFG_STRICT_5GHZ_PREF_BY_MARGIN_MAX ),

   REG_VARIABLE( CFG_COALESING_IN_IBSS_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, isCoalesingInIBSSAllowed,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_COALESING_IN_IBSS_DEFAULT,
                 CFG_COALESING_IN_IBSS_MIN,
                 CFG_COALESING_IN_IBSS_MAX ),

   REG_VARIABLE( CFG_DISABLE_ATH_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, cfgAthDisable,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DISABLE_ATH_DEFAULT,
                 CFG_DISABLE_ATH_MIN,
                 CFG_DISABLE_ATH_MAX ),
   REG_VARIABLE(CFG_BTC_ACTIVE_WLAN_LEN_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcActiveWlanLen,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_ACTIVE_WLAN_LEN_DEFAULT,
                CFG_BTC_ACTIVE_WLAN_LEN_MIN,
                CFG_BTC_ACTIVE_WLAN_LEN_MAX ),

   REG_VARIABLE(CFG_BTC_ACTIVE_BT_LEN_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcActiveBtLen,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_ACTIVE_BT_LEN_DEFAULT,
                CFG_BTC_ACTIVE_BT_LEN_MIN,
                CFG_BTC_ACTIVE_BT_LEN_MAX ),

   REG_VARIABLE(CFG_BTC_SAP_ACTIVE_WLAN_LEN_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcSapActiveWlanLen,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_SAP_ACTIVE_WLAN_LEN_DEFAULT,
                CFG_BTC_SAP_ACTIVE_WLAN_LEN_MIN,
                CFG_BTC_SAP_ACTIVE_WLAN_LEN_MAX ),

   REG_VARIABLE(CFG_BTC_SAP_ACTIVE_BT_LEN_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcSapActiveBtLen,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_SAP_ACTIVE_BT_LEN_DEFAULT,
                CFG_BTC_SAP_ACTIVE_BT_LEN_MIN,
                CFG_BTC_SAP_ACTIVE_BT_LEN_MAX ),

#ifdef MEMORY_DEBUG
   REG_VARIABLE(CFG_ENABLE_MEMORY_DEBUG_NAME, WLAN_PARAM_Integer,
                hdd_config_t, IsMemoryDebugSupportEnabled,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_MEMORY_DEBUG_DEFAULT,
                CFG_ENABLE_MEMORY_DEBUG_MIN,
                CFG_ENABLE_MEMORY_DEBUG_MAX ),
#endif

   REG_VARIABLE_STRING( CFG_OVERRIDE_COUNTRY_CODE, WLAN_PARAM_String,
                hdd_config_t, overrideCountryCode,
                VAR_FLAGS_OPTIONAL,
               (void *)CFG_OVERRIDE_COUNTRY_CODE_DEFAULT),

   REG_VARIABLE( CFG_ASD_PROBE_INTERVAL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, gAsdProbeInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ASD_PROBE_INTERVAL_DEFAULT,
                 CFG_ASD_PROBE_INTERVAL_MIN,
                 CFG_ASD_PROBE_INTERVAL_MAX),

   REG_VARIABLE( CFG_ASD_TRIGGER_THRESHOLD_NAME, WLAN_PARAM_SignedInteger,
                 hdd_config_t, gAsdTriggerThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ASD_TRIGGER_THRESHOLD_DEFAULT,
                 CFG_ASD_TRIGGER_THRESHOLD_MIN,
                 CFG_ASD_TRIGGER_THRESHOLD_MAX),

   REG_VARIABLE( CFG_ASD_RTT_RSSI_HYST_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, gAsdRTTRssiHystThreshold,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ASD_RTT_RSSI_HYST_THRESHOLD_DEFAULT,
                 CFG_ASD_RTT_RSSI_HYST_THRESHOLD_MIN,
                 CFG_ASD_RTT_RSSI_HYST_THRESHOLD_MAX),

   REG_VARIABLE( CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, debugP2pRemainOnChannel,
                 VAR_FLAGS_OPTIONAL,
                 CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_DEFAULT,
                 CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_MIN,
                 CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_MAX ),

   REG_VARIABLE(CFG_CTS2S_DURING_BTC_SCO_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcCTS2SduringSCO,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_CTS2S_DURING_BTC_SCO_DEFAULT,
                CFG_CTS2S_DURING_BTC_SCO_MIN,
                CFG_CTS2S_DURING_BTC_SCO_MAX ),

   REG_VARIABLE( CFG_ENABLE_DEBUG_CONNECT_ISSUE, WLAN_PARAM_Integer,
              hdd_config_t, gEnableDebugLog,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_ENABLE_DEBUG_CONNECT_ISSUE_DEFAULT,
              CFG_ENABLE_DEBUG_CONNECT_ISSUE_MIN ,
              CFG_ENABLE_DEBUG_CONNECT_ISSUE_MAX),

   REG_VARIABLE(CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nOBSSScanActiveDwellTime,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_DEFAULT,
                CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_MIN,
                CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_MAX ),

   REG_VARIABLE(CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nOBSSScanPassiveDwellTime,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_DEFAULT,
                CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_MIN,
                CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_MAX ),

   REG_VARIABLE(CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_NAME,
                WLAN_PARAM_Integer,
                hdd_config_t, nOBSSScanWidthTriggerInterval,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_DEFAULT,
                CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_MIN,
                CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_MAX ),

   REG_VARIABLE( CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_NAME, WLAN_PARAM_Integer,
                hdd_config_t, gEnableStrictRegulatoryForFCC,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_DEFAULT,
                CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_MIN,
                CFG_ENABLE_STRICT_REGULATORY_FOR_FCC_MAX ),

   REG_VARIABLE( CFG_ADVERTISE_CONCURRENT_OPERATION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, advertiseConcurrentOperation,
                 VAR_FLAGS_OPTIONAL,
                 CFG_ADVERTISE_CONCURRENT_OPERATION_DEFAULT,
                 CFG_ADVERTISE_CONCURRENT_OPERATION_MIN,
                 CFG_ADVERTISE_CONCURRENT_OPERATION_MAX ),

   REG_VARIABLE( CFG_DEFAULT_RATE_INDEX_24GH, WLAN_PARAM_Integer,
                 hdd_config_t, defaultRateIndex24Ghz,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DEFAULT_RATE_INDEX_24GH_DEFAULT,
                 CFG_DEFAULT_RATE_INDEX_24GH_MIN,
                 CFG_DEFAULT_RATE_INDEX_24GH_MAX ),

   REG_VARIABLE( CFG_SAP_DOT11_MODE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, sapDot11Mode,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                 CFG_SAP_DOT11_MODE_DEFAULT,
                 CFG_SAP_DOT11_MODE_MIN,
                 CFG_SAP_DOT11_MODE_MAX ),

   REG_VARIABLE(CFG_RA_FILTER_ENABLE_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgRAFilterEnable,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_RA_FILTER_ENABLE_DEFAULT,
                CFG_RA_FILTER_ENABLE_MIN,
                CFG_RA_FILTER_ENABLE_MAX ),

   REG_VARIABLE(CFG_RA_RATE_LIMIT_INTERVAL_NAME, WLAN_PARAM_Integer,
               hdd_config_t, cfgRARateLimitInterval,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_RA_RATE_LIMIT_INTERVAL_DEFAULT,
               CFG_RA_RATE_LIMIT_INTERVAL_MIN,
               CFG_RA_RATE_LIMIT_INTERVAL_MAX ),

   REG_VARIABLE( CFG_ROAMING_DFS_CHANNEL_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, allowDFSChannelRoam,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ROAMING_DFS_CHANNEL_DEFAULT,
                 CFG_ROAMING_DFS_CHANNEL_MIN,
                 CFG_ROAMING_DFS_CHANNEL_MAX ),

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
   REG_VARIABLE( CFG_WLAN_LOGGING_SUPPORT_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, wlanLoggingEnable,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_WLAN_LOGGING_SUPPORT_DEFAULT,
                 CFG_WLAN_LOGGING_SUPPORT_DISABLE,
                 CFG_WLAN_LOGGING_SUPPORT_ENABLE ),

   REG_VARIABLE( CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, wlanLoggingFEToConsole,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_DEFAULT,
                 CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_DISABLE,
                 CFG_WLAN_LOGGING_FE_CONSOLE_SUPPORT_ENABLE ),

   REG_VARIABLE( CFG_WLAN_LOGGING_NUM_BUF_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, wlanLoggingNumBuf,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_WLAN_LOGGING_NUM_BUF_DEFAULT,
                 CFG_WLAN_LOGGING_NUM_BUF_MIN,
                 CFG_WLAN_LOGGING_NUM_BUF_MAX ),
#endif //WLAN_LOGGING_SOCK_SVC_ENABLE

   REG_VARIABLE(CFG_INITIAL_DWELL_TIME_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nInitialDwellTime,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_INITIAL_DWELL_TIME_DEFAULT,
               CFG_INITIAL_DWELL_TIME_MIN,
               CFG_INITIAL_DWELL_TIME_MAX ),

   REG_VARIABLE(CFG_INITIAL_SCAN_SKIP_DFS_CH_NAME, WLAN_PARAM_Integer,
               hdd_config_t, initialScanSkipDFSCh,
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
               CFG_INITIAL_SCAN_SKIP_DFS_CH_DEFAULT,
               CFG_INITIAL_SCAN_SKIP_DFS_CH_MIN,
               CFG_INITIAL_SCAN_SKIP_DFS_CH_MAX),

   REG_VARIABLE(CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcFatalHidnSniffBlkGuidance,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_DEFAULT,
                CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_MIN,
                CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE_MAX ),

   REG_VARIABLE(CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcCriticalHidnSniffBlkGuidance,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_DEFAULT,
                CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_MIN,
                CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE_MAX ),

   REG_VARIABLE(CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcA2dpTxQueueThold,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_DEFAULT,
                CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_MIN,
                CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD_MAX ),

   REG_VARIABLE(CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_NAME, WLAN_PARAM_Integer,
                hdd_config_t, cfgBtcOppTxQueueThold,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_DEFAULT,
                CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_MIN,
                CFG_BTC_DYN_OPP_TX_QUEUE_THOLD_MAX ),

#ifdef WLAN_FEATURE_11W
   REG_VARIABLE(CFG_PMF_SA_QUERY_MAX_RETRIES_NAME, WLAN_PARAM_Integer,
                hdd_config_t, pmfSaQueryMaxRetries,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_PMF_SA_QUERY_MAX_RETRIES_DEFAULT,
                CFG_PMF_SA_QUERY_MAX_RETRIES_MIN,
                CFG_PMF_SA_QUERY_MAX_RETRIES_MAX ),

   REG_VARIABLE(CFG_PMF_SA_QUERY_RETRY_INTERVAL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, pmfSaQueryRetryInterval,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_PMF_SA_QUERY_RETRY_INTERVAL_DEFAULT,
                CFG_PMF_SA_QUERY_RETRY_INTERVAL_MIN,
                CFG_PMF_SA_QUERY_RETRY_INTERVAL_MAX ),
#endif

   REG_VARIABLE(CFG_DEFER_IMPS_FOR_TIME_NAME, WLAN_PARAM_Integer,
                hdd_config_t, deferImpsTime,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_DEFER_IMPS_FOR_TIME_DEFAULT,
                CFG_DEFER_IMPS_FOR_TIME_MIN,
                CFG_DEFER_IMPS_FOR_TIME_MAX),

   REG_VARIABLE(CFG_ENABLE_DEAUTH_BEFORE_CONNECTION, WLAN_PARAM_Integer,
                hdd_config_t, sendDeauthBeforeCon,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_DEFAULT,
                CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_MIN,
                CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_MAX),

   REG_VARIABLE(CFG_ENABLE_MAC_ADDR_SPOOFING, WLAN_PARAM_Integer,
                hdd_config_t, enableMacSpoofing,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_MAC_ADDR_SPOOFING_DEFAULT,
                CFG_ENABLE_MAC_ADDR_SPOOFING_MIN,
                CFG_ENABLE_MAC_ADDR_SPOOFING_MAX),

   REG_VARIABLE(CFG_ENABLE_CH_AVOID, WLAN_PARAM_Integer,
                 hdd_config_t, fenableCHAvoidance,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_CH_AVOID_DEFAULT,
                 CFG_ENABLE_CH_AVOID_MIN,
                 CFG_ENABLE_CH_AVOID_MAX ),

   REG_VARIABLE(CFG_MAX_CONCURRENT_CONNECTIONS_NAME, WLAN_PARAM_Integer,
                hdd_config_t, gMaxConcurrentActiveSessions,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_MAX_CONCURRENT_CONNECTIONS_DEFAULT,
                CFG_MAX_CONCURRENT_CONNECTIONS_MIN,
                CFG_MAX_CONCURRENT_CONNECTIONS_MAX ),
   REG_VARIABLE( CFG_ENABLE_DYNAMIC_WMMPS_NAME, WLAN_PARAM_Integer,
                hdd_config_t, enableDynamicWMMPS,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_ENABLE_DYNAMIC_WMM_PS_DEFAULT,
                CFG_ENABLE_DYNAMIC_WMM_PS_MIN,
                CFG_ENABLE_DYNAMIC_WMM_PS_MAX ),

   REG_VARIABLE( CFG_MAX_UAPSD_CONSEC_SP_NAME, WLAN_PARAM_Integer,
                hdd_config_t, maxUapsdConsecSP,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_MAX_UAPSD_CONSEC_SP_DEFAULT,
                CFG_MAX_UAPSD_CONSEC_SP_MIN,
                CFG_MAX_UAPSD_CONSEC_SP_MAX ),

   REG_VARIABLE( CFG_MAX_UAPSD_CONSEC_RX_CNT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, maxUapsdConsecRxCnt,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_MAX_UAPSD_CONSEC_RX_CNT_DEFAULT,
                CFG_MAX_UAPSD_CONSEC_RX_CNT_MIN,
                CFG_MAX_UAPSD_CONSEC_RX_CNT_MAX ),

   REG_VARIABLE( CFG_MAX_UAPSD_CONSEC_TX_CNT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, maxUapsdConsecTxCnt,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_MAX_UAPSD_CONSEC_TX_CNT_DEFAULT,
                CFG_MAX_UAPSD_CONSEC_TX_CNT_MIN,
                CFG_MAX_UAPSD_CONSEC_TX_CNT_MAX ),

   REG_VARIABLE( CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_NAME, WLAN_PARAM_Integer,
                hdd_config_t, uapsdConsecRxCntMeasWindow,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_DEFAULT,
                CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_MIN,
                CFG_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW_MAX ),

   REG_VARIABLE( CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_NAME, WLAN_PARAM_Integer,
                hdd_config_t, uapsdConsecTxCntMeasWindow,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_DEFAULT,
                CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_MIN,
                CFG_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW_MAX ),

   REG_VARIABLE( CFG_UAPSD_PSPOLL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, maxPsPollInWmmUapsdMode,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_UAPSD_PSPOLL_DEFAULT,
                CFG_UAPSD_PSPOLL_MIN,
                CFG_UAPSD_PSPOLL_MAX ),

   REG_VARIABLE( CFG_MAX_UAPSD_INACT_INTVL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, maxUapsdInactivityIntervals,
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                CFG_MAX_UAPSD_INACT_INTVL_DEFAULT,
                CFG_MAX_UAPSD_INACT_INTVL_MIN,
                CFG_MAX_UAPSD_INACT_INTVL_MAX ),

   REG_VARIABLE( CFG_DEBUG_DHCP, WLAN_PARAM_Integer,
                  hdd_config_t, enableDhcpDebug,
                  VAR_FLAGS_OPTIONAL |
                  VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_DEBUG_DHCP_DEFAULT,
                  CFG_DEBUG_DHCP_DISABLE,
                  CFG_DEBUG_DHCP_ENABLE ),

   REG_VARIABLE( CFG_BURST_MODE_BE_TXOP_VALUE, WLAN_PARAM_Integer,
                  hdd_config_t, burstModeTXOPValue,
                  VAR_FLAGS_OPTIONAL |
                  VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_BURST_MODE_BE_TXOP_VALUE_DEFAULT,
                  CFG_BURST_MODE_BE_TXOP_VALUE_MIN,
                  CFG_BURST_MODE_BE_TXOP_VALUE_MAX ),
   REG_VARIABLE( CFG_SAP_SCAN_BAND_PREFERENCE, WLAN_PARAM_Integer,
                  hdd_config_t, acsScanBandPreference,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK,
                  CFG_SAP_SCAN_BAND_PREFERENCE_DEFAULT,
                  CFG_SAP_SCAN_BAND_PREFERENCE_MIN,
                  CFG_SAP_SCAN_BAND_PREFERENCE_MAX ),

   REG_VARIABLE( CFG_ENABLE_DYNAMIC_RA_START_RATE_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, enableDynamicRAStartRate,
                  VAR_FLAGS_OPTIONAL |
                  VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_ENABLE_DYNAMIC_RA_START_RATE_DEFAULT,
                  CFG_ENABLE_DYNAMIC_RA_START_RATE_MIN,
                  CFG_ENABLE_DYNAMIC_RA_START_RATE_MAX),
};

/*
 * This function returns a pointer to the character after the occurence
 * of a new line character. It also modifies the original string by replacing
 * the '\n' character with the null character.
 * Function returns NULL if no new line character was found before end of
 * string was reached
 */
static char* get_next_line(char* str, char *str_end)
{
  char c;

  if( str == NULL || *str == '\0') {
    return NULL;
  }

  c = *str;
  while(c != '\n'  && c != '\0' && c != 0xd)  {
    str = str + 1;
    if (str > str_end)
    {
        return str;
    }
    c = *str;
  }

  if (c == '\0' ) {
    return NULL;
  }
  else
  {
    return (str+1);
  }

  return NULL;
}

// look for space. Ascii values to look are -
// 0x09 == horizontal tab
// 0x0a == Newline ("\n")
// 0x0b == vertical tab
// 0x0c == Newpage or feed form.
// 0x0d == carriage return (CR or "\r")
// Null ('\0') should not considered as space.
#define i_isspace(ch)  (((ch) >= 0x09 && (ch) <= 0x0d) || (ch) == ' ')

/*
 * This function trims any leading and trailing white spaces
 */
static char *i_trim(char *str)

{
   char *ptr;

   if(*str == '\0') return str;

   /* Find the first non white-space*/
   for (ptr = str; i_isspace(*ptr); ptr++);
      if (*ptr == '\0')
         return str;

   /* This is the new start of the string*/
   str = ptr;

   /* Find the last non white-space */
   ptr += strlen(ptr) - 1;
   for (; ptr != str && i_isspace(*ptr); ptr--);
      /* Null terminate the following character */
      ptr[1] = '\0';

   return str;
}


//Structure to store each entry in qcom_cfg.ini file
typedef struct
{
   char *name;
   char *value;
}tCfgIniEntry;

static VOS_STATUS hdd_apply_cfg_ini( hdd_context_t * pHddCtx,
    tCfgIniEntry* iniTable, unsigned long entries);

#ifdef WLAN_CFG_DEBUG
void dump_cfg_ini (tCfgIniEntry* iniTable, unsigned long entries)
{
   unsigned long i;

   for (i = 0; i < entries; i++) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "%s entry Name=[%s] value=[%s]",
           WLAN_INI_FILE, iniTable[i].name, iniTable[i].value);
     }
}
#endif

/*
 * This function reads the qcom_cfg.ini file and
 * parses each 'Name=Value' pair in the ini file
 */
VOS_STATUS hdd_parse_config_ini(hdd_context_t* pHddCtx)
{
   int status, i=0;
   /** Pointer for firmware image data */
   const struct firmware *fw = NULL;
   char *buffer, *line, *pTemp = NULL;
   size_t size;
   char *name, *value;
   /* cfgIniTable is static to avoid excess stack usage */
   static tCfgIniEntry cfgIniTable[MAX_CFG_INI_ITEMS];
   VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

   memset(cfgIniTable, 0, sizeof(cfgIniTable));

   status = request_firmware(&fw, WLAN_INI_FILE, pHddCtx->parent_dev);

   if(status)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: request_firmware failed %d",__func__, status);
      vos_status = VOS_STATUS_E_FAILURE;
      goto config_exit;
   }
   if(!fw || !fw->data || !fw->size)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: %s download failed",
             __func__, WLAN_INI_FILE);
      vos_status = VOS_STATUS_E_FAILURE;
      goto config_exit;
   }

   hddLog(LOGE, "%s: qcom_cfg.ini Size %zu", __func__, fw->size);

   buffer = (char*)vos_mem_malloc(fw->size);

   if(NULL == buffer) {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: kmalloc failure",__func__);
      release_firmware(fw);
      return VOS_STATUS_E_FAILURE;
   }
   pTemp = buffer;

   vos_mem_copy((void*)buffer,(void *)fw->data, fw->size);
   size = fw->size;

   while (buffer != NULL)
   {
      /*
       * get_next_line function used to modify the \n and \r delimiter
       * to \0 before returning, without checking if it is over parsing the
       * source buffer. So changed the function not to modify the buffer
       * passed to it and letting the calling/caller function to take
       * care of the return pointer validation and modification of the buffer.
       * So validating if the return pointer is not greater than the end
       * buffer address and modifying the buffer value.
       */
      line = get_next_line(buffer, (pTemp + (fw->size-1)));
      if(line > (pTemp + fw->size)) {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: INI file seems to be corrupted",
                  __func__);
         vos_status = VOS_STATUS_E_FAILURE;
         goto config_exit;
      }
      else if(line) {
         *(line - 1) = '\0';
      }

      buffer = i_trim(buffer);

      hddLog(LOG1, "%s: item", buffer);

      if(strlen((char*)buffer) == 0 || *buffer == '#')  {
         buffer = line;
         continue;
      }
      else if(strncmp(buffer, "END", 3) == 0 ) {
         break;
      }
      else
      {
         name = buffer;
         while(*buffer != '=' && *buffer != '\0')
            buffer++;
         if(*buffer != '\0') {
            *buffer++ = '\0';
            i_trim(name);
            if(strlen (name) != 0) {
               buffer = i_trim(buffer);
               if(strlen(buffer)>0) {
                  value = buffer;
                  while(!i_isspace(*buffer) && *buffer != '\0')
                     buffer++;
                  *buffer = '\0';
                  cfgIniTable[i].name= name;
                  cfgIniTable[i++].value= value;
                  if(i >= MAX_CFG_INI_ITEMS) {
                     hddLog(LOGE,"%s: Number of items in %s > %d",
                        __func__, WLAN_INI_FILE, MAX_CFG_INI_ITEMS);
                     break;
                  }
               }
            }
         }
      }
      buffer = line;
   }

   //Loop through the registry table and apply all these configs
   vos_status = hdd_apply_cfg_ini(pHddCtx, cfgIniTable, i);

config_exit:
   release_firmware(fw);
   vos_mem_free(pTemp);
   return vos_status;
}


static void print_hdd_cfg(hdd_context_t *pHddCtx)
{
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "*********Config values in HDD Adapter*******");
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [RTSThreshold] Value = %u",pHddCtx->cfg_ini->RTSThreshold) ;
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [OperatingChannel] Value = [%u]",pHddCtx->cfg_ini->OperatingChannel);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [PowerUsageControl] Value = [%s]",pHddCtx->cfg_ini->PowerUsageControl);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fIsImpsEnabled] Value = [%u]",pHddCtx->cfg_ini->fIsImpsEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [AutoBmpsTimerEnabled] Value = [%u]",pHddCtx->cfg_ini->fIsAutoBmpsTimerEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nAutoBmpsTimerValue] Value = [%u]",pHddCtx->cfg_ini->nAutoBmpsTimerValue);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nVccRssiTrigger] Value = [%u]",pHddCtx->cfg_ini->nVccRssiTrigger);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
            "Name = [gIbssBssid] Value =["MAC_ADDRESS_STR"]",
            MAC_ADDR_ARRAY(pHddCtx->cfg_ini->IbssBssid.bytes));

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
            "Name = [Intf0MacAddress] Value =["MAC_ADDRESS_STR"]",
            MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[0].bytes));

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
            "Name = [Intf1MacAddress] Value =["MAC_ADDRESS_STR"]",
            MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[1].bytes));

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
            "Name = [Intf2MacAddress] Value =["MAC_ADDRESS_STR"]",
            MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[2].bytes));

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
            "Name = [Intf3MacAddress] Value =["MAC_ADDRESS_STR"]",
            MAC_ADDR_ARRAY(pHddCtx->cfg_ini->intfMacAddr[3].bytes));

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApEnableUapsd] value = [%u]",pHddCtx->cfg_ini->apUapsdEnabled);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAPCntryCode] Value =[%c%c%c]",
      pHddCtx->cfg_ini->apCntryCode[0],pHddCtx->cfg_ini->apCntryCode[1],
      pHddCtx->cfg_ini->apCntryCode[2]);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableApProt] value = [%u]", pHddCtx->cfg_ini->apProtEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAPAutoShutOff] Value = [%u]", pHddCtx->cfg_ini->nAPAutoShutOff);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableListenMode] Value = [%u]", pHddCtx->cfg_ini->nEnableListenMode);
  VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApProtection] value = [%u]",pHddCtx->cfg_ini->apProtection);
  VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableApOBSSProt] value = [%u]",pHddCtx->cfg_ini->apOBSSProtEnabled);
  VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApAutoChannelSelection] value = [%u]",pHddCtx->cfg_ini->apAutoChannelSelection);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ChannelBondingMode] Value = [%u]",pHddCtx->cfg_ini->nChannelBondingMode24GHz);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ChannelBondingMode] Value = [%u]",pHddCtx->cfg_ini->nChannelBondingMode5GHz);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [dot11Mode] Value = [%u]",pHddCtx->cfg_ini->dot11Mode);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WmmMode] Value = [%u] ",pHddCtx->cfg_ini->WmmMode);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [UapsdMask] Value = [0x%x] ",pHddCtx->cfg_ini->UapsdMask);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [PktClassificationBasis] Value = [%u] ",pHddCtx->cfg_ini->PktClassificationBasis);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ImplicitQosIsEnabled] Value = [%u]",(int)pHddCtx->cfg_ini->bImplicitQosEnabled);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdVoSrvIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdVoSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdVoSuspIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdVoSuspIntv);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdViSrvIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdViSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdViSuspIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdViSuspIntv);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBeSrvIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdBeSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBeSuspIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdBeSuspIntv);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBkSrvIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdBkSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBkSuspIntv] Value = [%u] ",pHddCtx->cfg_ini->InfraUapsdBkSuspIntv);
#ifdef FEATURE_WLAN_ESE
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraInactivityInterval] Value = [%u] ",pHddCtx->cfg_ini->InfraInactivityInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [EseEnabled] Value = [%u] ",pHddCtx->cfg_ini->isEseIniFeatureEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [FastTransitionEnabled] Value = [%u] ",pHddCtx->cfg_ini->isFastTransitionEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gTxPowerCap] Value = [%u] dBm ",pHddCtx->cfg_ini->nTxPowerCap);
#endif
#ifdef FEATURE_WLAN_LFR
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [FastRoamEnabled] Value = [%u] ",pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [MAWCEnabled] Value = [%u] ",pHddCtx->cfg_ini->MAWCEnabled);
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [RoamRssiDiff] Value = [%u] ",pHddCtx->cfg_ini->RoamRssiDiff);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ImmediateRoamRssiDiff] Value = [%u] ",pHddCtx->cfg_ini->nImmediateRoamRssiDiff);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [isWESModeEnabled] Value = [%u] ",pHddCtx->cfg_ini->isWESModeEnabled);
#endif
#ifdef FEATURE_WLAN_OKC
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [OkcEnabled] Value = [%u] ",pHddCtx->cfg_ini->isOkcIniFeatureEnabled);
#endif
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcVo] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcVo] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcVo] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMeanDataRateAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcVo] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMinPhyRateAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcVo] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcVo);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcVi] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcVi] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcVi] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMeanDataRateAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcVi] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMinPhyRateAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcVi] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcVi);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcBe] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcBe] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcBe] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMeanDataRateAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcBe] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMinPhyRateAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcBe] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcBe);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcBk] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcBk] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcBk] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMeanDataRateAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcBk] Value = [0x%x] ",pHddCtx->cfg_ini->InfraMinPhyRateAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcBk] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcBk);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqBkWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqBkWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqBeWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqBeWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqViWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqViWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqVoWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqVoWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [DelayedTriggerFrmInt] Value = [%u] ",pHddCtx->cfg_ini->DelayedTriggerFrmInt);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [BkReorderAgingTime] Value = [%u] ",pHddCtx->cfg_ini->BkReorderAgingTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [BeReorderAgingTime] Value = [%u] ",pHddCtx->cfg_ini->BeReorderAgingTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ViReorderAgingTime] Value = [%u] ",pHddCtx->cfg_ini->ViReorderAgingTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [VoReorderAgingTime] Value = [%u] ",pHddCtx->cfg_ini->VoReorderAgingTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [mcastBcastFilterSetting] Value = [%u] ",pHddCtx->cfg_ini->mcastBcastFilterSetting);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fhostArpOffload] Value = [%u] ",pHddCtx->cfg_ini->fhostArpOffload);
#ifdef WLAN_FEATURE_VOWIFI_11R
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fFTResourceReqSupported] Value = [%u] ",pHddCtx->cfg_ini->fFTResourceReqSupported);
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborReassocRssiThreshold] Value = [%u] ",pHddCtx->cfg_ini->nNeighborReassocRssiThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborLookupRssiThreshold] Value = [%u] ",pHddCtx->cfg_ini->nNeighborLookupRssiThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanMinChanTime] Value = [%u] ",pHddCtx->cfg_ini->nNeighborScanMinChanTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanMaxChanTime] Value = [%u] ",pHddCtx->cfg_ini->nNeighborScanMaxChanTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nMaxNeighborRetries] Value = [%u] ",pHddCtx->cfg_ini->nMaxNeighborReqTries);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanPeriod] Value = [%u] ",pHddCtx->cfg_ini->nNeighborScanPeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanResultsRefreshPeriod] Value = [%u] ",pHddCtx->cfg_ini->nNeighborResultsRefreshPeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nEmptyScanRefreshPeriod] Value = [%u] ",pHddCtx->cfg_ini->nEmptyScanRefreshPeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborInitialForcedRoamTo5GhEnable] Value = [%u] ",pHddCtx->cfg_ini->nNeighborInitialForcedRoamTo5GhEnable);
#endif
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [burstSizeDefinition] Value = [0x%x] ",pHddCtx->cfg_ini->burstSizeDefinition);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [tsInfoAckPolicy] Value = [0x%x] ",pHddCtx->cfg_ini->tsInfoAckPolicy);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [rfSettlingTimeUs] Value = [%u] ",pHddCtx->cfg_ini->rfSettlingTimeUs);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [bSingleTidRc] Value = [%u] ",pHddCtx->cfg_ini->bSingleTidRc);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gDynamicPSPollvalue] Value = [%u] ",pHddCtx->cfg_ini->dynamicPsPollValue);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAddTSWhenACMIsOff] Value = [%u] ",pHddCtx->cfg_ini->AddTSWhenACMIsOff);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gValidateScanList] Value = [%u] ",pHddCtx->cfg_ini->fValidateScanList);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gStaKeepAlivePeriod] Value = [%u] ",pHddCtx->cfg_ini->infraStaKeepAlivePeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApDataAvailPollInterVal] Value = [%u] ",pHddCtx->cfg_ini->apDataAvailPollPeriodInMs);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableBtAmp] Value = [%u] ",pHddCtx->cfg_ini->enableBtAmp);
#ifdef WLAN_BTAMP_FEATURE
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [BtAmpPreferredChannel] Value = [%u] ",pHddCtx->cfg_ini->preferredChannel);
#endif //WLAN_BTAMP_FEATURE
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [BandCapability] Value = [%u] ",pHddCtx->cfg_ini->nBandCapability);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fEnableBeaconEarlyTermination] Value = [%u] ",pHddCtx->cfg_ini->fEnableBeaconEarlyTermination);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [teleBcnWakeupEnable] Value = [%u] ",pHddCtx->cfg_ini->teleBcnWakeupEn);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [transListenInterval] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnTransListenInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [transLiNumIdleBeacons] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnTransLiNumIdleBeacons);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [maxListenInterval] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnMaxListenInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [maxLiNumIdleBeacons] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnMaxLiNumIdleBeacons);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [bcnEarlyTermWakeInterval] Value = [%u] ",pHddCtx->cfg_ini->bcnEarlyTermWakeInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApDataAvailPollInterVal] Value = [%u] ",pHddCtx->cfg_ini->apDataAvailPollPeriodInMs);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableBypass11d] Value = [%u] ",pHddCtx->cfg_ini->enableBypass11d);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableDFSChnlScan] Value = [%u] ",pHddCtx->cfg_ini->enableDFSChnlScan);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableDFSPnoChnlScan] Value = [%u] ",pHddCtx->cfg_ini->enableDFSPnoChnlScan);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gReportMaxLinkSpeed] Value = [%u] ",pHddCtx->cfg_ini->reportMaxLinkSpeed);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [thermalMitigationEnable] Value = [%u] ",pHddCtx->cfg_ini->thermalMitigationEnable);
#ifdef WLAN_FEATURE_11AC
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gVhtChannelWidth] value = [%u]",pHddCtx->cfg_ini->vhtChannelWidth);
#endif
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [enableFirstScan2GOnly] Value = [%u] ",pHddCtx->cfg_ini->enableFirstScan2GOnly);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [skipDfsChnlInP2pSearch] Value = [%u] ",pHddCtx->cfg_ini->skipDfsChnlInP2pSearch);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ignoreDynamicDtimInP2pMode] Value = [%u] ",pHddCtx->cfg_ini->ignoreDynamicDtimInP2pMode);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [enableRxSTBC] Value = [%u] ",pHddCtx->cfg_ini->enableRxSTBC);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableLpwrImgTransition] Value = [%u] ",pHddCtx->cfg_ini->enableLpwrImgTransition);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableSSR] Value = [%u] ",pHddCtx->cfg_ini->enableSSR);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableVhtFor24GHzBand] Value = [%u] ",pHddCtx->cfg_ini->enableVhtFor24GHzBand);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableTrafficMonitor] Value = [%u] ", pHddCtx->cfg_ini->enableTrafficMonitor);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gTrafficIdleTimeout] Value = [%u] ", pHddCtx->cfg_ini->trafficIdleTimeout);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gFlexConnectPowerFactor] Value = [%u] ", pHddCtx->cfg_ini->flexConnectPowerFactor);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableIbssHeartBeatOffload] Value = [%u] ", pHddCtx->cfg_ini->enableIbssHeartBeatOffload);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAntennaDiversity] Value = [%u] ", pHddCtx->cfg_ini->antennaDiversity);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gGoLinkMonitorPeriod] Value = [%u]",pHddCtx->cfg_ini->goLinkMonitorPeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApLinkMonitorPeriod] Value = [%u]",pHddCtx->cfg_ini->apLinkMonitorPeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gGoKeepAlivePeriod] Value = [%u]",pHddCtx->cfg_ini->goKeepAlivePeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApKeepAlivePeriod]Value = [%u]",pHddCtx->cfg_ini->apKeepAlivePeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAmsduSupportInAMPDU] Value = [%u] ",pHddCtx->cfg_ini->isAmsduSupportInAMPDU);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nSelect5GHzMargin] Value = [%u] ",pHddCtx->cfg_ini->nSelect5GHzMargin);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gCoalesingInIBSS] Value = [%u] ",pHddCtx->cfg_ini->isCoalesingInIBSSAllowed);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [overrideCountryCode] Value = [%s] ",pHddCtx->cfg_ini->overrideCountryCode);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAsdProbeInterval] Value = [%u]",pHddCtx->cfg_ini->gAsdProbeInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAsdTriggerThreshold] Value = [%hhd]",pHddCtx->cfg_ini->gAsdTriggerThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAsdRTTRssiHystThreshold]Value = [%u]",pHddCtx->cfg_ini->gAsdRTTRssiHystThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gRoamtoDFSChannel] Value = [%u] ",pHddCtx->cfg_ini->allowDFSChannelRoam);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gMaxConcurrentActiveSessions] Value = [%u] ", pHddCtx->cfg_ini->gMaxConcurrentActiveSessions);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAcsScanBandPreference] Value = [%u] ",pHddCtx->cfg_ini->acsScanBandPreference);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gACSBandSwitchThreshold] value = [%u]\n",pHddCtx->cfg_ini->acsBandSwitchThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gDeferScanTimeInterval] value = [%u]\n",pHddCtx->cfg_ini->nDeferScanTimeInterval);
}



#define CFG_VALUE_MAX_LEN 256
#define CFG_ENTRY_MAX_LEN (32+CFG_VALUE_MAX_LEN)
VOS_STATUS hdd_cfg_get_config(hdd_context_t *pHddCtx, char *pBuf, int buflen)
{
   unsigned int idx;
   REG_TABLE_ENTRY *pRegEntry = g_registry_table;
   unsigned long cRegTableEntries  = sizeof(g_registry_table) / sizeof( g_registry_table[ 0 ]);
   v_U32_t value;
   char valueStr[CFG_VALUE_MAX_LEN];
   char configStr[CFG_ENTRY_MAX_LEN];
   char *fmt;
   void *pField;
   v_MACADDR_t *pMacAddr;
   char *pCur = pBuf;
   int curlen;

   // start with an empty string
   *pCur = '\0';

   for ( idx = 0; idx < cRegTableEntries; idx++, pRegEntry++ )
   {
      pField = ( (v_U8_t *)pHddCtx->cfg_ini) + pRegEntry->VarOffset;

      if ( ( WLAN_PARAM_Integer       == pRegEntry->RegType ) ||
           ( WLAN_PARAM_SignedInteger == pRegEntry->RegType ) ||
           ( WLAN_PARAM_HexInteger    == pRegEntry->RegType ) )
      {
         value = 0;
         memcpy( &value, pField, pRegEntry->VarSize );
         if ( WLAN_PARAM_HexInteger == pRegEntry->RegType )
         {
            fmt = "%x";
         }
         else if ( WLAN_PARAM_SignedInteger == pRegEntry->RegType )
         {
            fmt = "%d";
         }
         else
         {
            fmt = "%u";
         }
         snprintf(valueStr, CFG_VALUE_MAX_LEN, fmt, value);
      }
      else if ( WLAN_PARAM_String == pRegEntry->RegType )
      {
         snprintf(valueStr, CFG_VALUE_MAX_LEN, "%s", (char *)pField);
      }
      else if ( WLAN_PARAM_MacAddr == pRegEntry->RegType )
      {
         pMacAddr = (v_MACADDR_t *) pField;
         snprintf(valueStr, CFG_VALUE_MAX_LEN,
                  "%02x:%02x:%02x:%02x:%02x:%02x",
                  pMacAddr->bytes[0],
                  pMacAddr->bytes[1],
                  pMacAddr->bytes[2],
                  pMacAddr->bytes[3],
                  pMacAddr->bytes[4],
                  pMacAddr->bytes[5]);
      }
      else
      {
         snprintf(valueStr, CFG_VALUE_MAX_LEN, "(unhandled)");
      }
      curlen = scnprintf(configStr, CFG_ENTRY_MAX_LEN,
                        "%s=[%s]%s\n",
                        pRegEntry->RegName,
                        valueStr,
                        test_bit(idx, (void *)&pHddCtx->cfg_ini->bExplicitCfg) ?
                        "*" : "");

      // ideally we want to return the config to the application
      // however the config is too big so we just printk() for now
#ifdef RETURN_IN_BUFFER
      if (curlen <= buflen)
      {
         // copy string + '\0'
         memcpy(pCur, configStr, curlen+1);

         // account for addition;
         pCur += curlen;
         buflen -= curlen;
      }
      else
      {
         // buffer space exhausted, return what we have
         return VOS_STATUS_E_RESOURCES;
      }
#else
      printk(KERN_CRIT "%s", configStr);
#endif // RETURN_IN_BUFFER

}

#ifndef RETURN_IN_BUFFER
   // notify application that output is in system log
   snprintf(pCur, buflen, "WLAN configuration written to system log");
#endif // RETURN_IN_BUFFER

   return VOS_STATUS_SUCCESS;
}

static VOS_STATUS find_cfg_item (tCfgIniEntry* iniTable, unsigned long entries,
    char *name, char** value)
{
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   unsigned long i;

   for (i = 0; i < entries; i++) {
     if (strcmp(iniTable[i].name, name) == 0) {
       *value = iniTable[i].value;
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Found %s entry for Name=[%s] Value=[%s] ",
           WLAN_INI_FILE, name, *value);
       return VOS_STATUS_SUCCESS;
     }
   }

   return status;
}

static int parseHexDigit(char c)
{
  if (c >= '0' && c <= '9')
    return c-'0';
  if (c >= 'a' && c <= 'f')
    return c-'a'+10;
  if (c >= 'A' && c <= 'F')
    return c-'A'+10;

  return 0;
}


static VOS_STATUS hdd_apply_cfg_ini( hdd_context_t *pHddCtx, tCfgIniEntry* iniTable, unsigned long entries)
{
   VOS_STATUS match_status = VOS_STATUS_E_FAILURE;
   VOS_STATUS ret_status = VOS_STATUS_SUCCESS;
   unsigned int idx;
   void *pField;
   char *value_str = NULL;
   unsigned long len_value_str;
   char *candidate;
   v_U32_t value;
   v_S31_t svalue;
   void *pStructBase = pHddCtx->cfg_ini;
   REG_TABLE_ENTRY *pRegEntry = g_registry_table;
   unsigned long cRegTableEntries  = sizeof(g_registry_table) / sizeof( g_registry_table[ 0 ]);
   v_U32_t cbOutString;
   int i;
   int rv;

   // sanity test
   if (MAX_CFG_INI_ITEMS < cRegTableEntries)
   {
      hddLog(LOGE, "%s: MAX_CFG_INI_ITEMS too small, must be at least %ld",
             __func__, cRegTableEntries);
   }

   for ( idx = 0; idx < cRegTableEntries; idx++, pRegEntry++ )
   {
      //Calculate the address of the destination field in the structure.
      pField = ( (v_U8_t *)pStructBase )+ pRegEntry->VarOffset;

      match_status = find_cfg_item(iniTable, entries, pRegEntry->RegName, &value_str);

      if( (match_status != VOS_STATUS_SUCCESS) && ( pRegEntry->Flags & VAR_FLAGS_REQUIRED ) )
      {
         // If we could not read the cfg item and it is required, this is an error.
         hddLog(LOGE, "%s: Failed to read required config parameter %s",
            __func__, pRegEntry->RegName);
         ret_status = VOS_STATUS_E_FAILURE;
         break;
      }

      if ( ( WLAN_PARAM_Integer    == pRegEntry->RegType ) ||
           ( WLAN_PARAM_HexInteger == pRegEntry->RegType ) )
      {
         // If successfully read from the registry, use the value read.
         // If not, use the default value.
         if ( match_status == VOS_STATUS_SUCCESS && (WLAN_PARAM_Integer == pRegEntry->RegType)) {
            rv = kstrtou32(value_str, 10, &value);
            if (rv < 0) {
                hddLog(LOGE, "%s: Reg Parameter %s invalid. Enforcing default",
                       __func__, pRegEntry->RegName);
                value = pRegEntry->VarDefault;
            }
         }
         else if ( match_status == VOS_STATUS_SUCCESS && (WLAN_PARAM_HexInteger == pRegEntry->RegType)) {
            rv = kstrtou32(value_str, 16, &value);
            if (rv < 0) {
                hddLog(LOGE, "%s: Reg paramter %s invalid. Enforcing default",
                       __func__, pRegEntry->RegName);
                value = pRegEntry->VarDefault;
            }
         }
         else {
            value = pRegEntry->VarDefault;
         }

         // If this parameter needs range checking, do it here.
         if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK )
         {
            if ( value > pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum [%u > %lu]. Enforcing Maximum",
                      __func__, pRegEntry->RegName, value, pRegEntry->VarMax );
               value = pRegEntry->VarMax;
            }

            if ( value < pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum [%u < %lu]. Enforcing Minimum",
                      __func__, pRegEntry->RegName, value, pRegEntry->VarMin);
               value = pRegEntry->VarMin;
            }
         }
         // If this parameter needs range checking, do it here.
         else if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT )
         {
            if ( value > pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum [%u > %lu]. Enforcing Default= %lu",
                  __func__, pRegEntry->RegName, value, pRegEntry->VarMax, pRegEntry->VarDefault  );
               value = pRegEntry->VarDefault;
            }

            if ( value < pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum [%u < %lu]. Enforcing Default= %lu",
                  __func__, pRegEntry->RegName, value, pRegEntry->VarMin, pRegEntry->VarDefault  );
               value = pRegEntry->VarDefault;
            }
         }

         // Move the variable into the output field.
         memcpy( pField, &value, pRegEntry->VarSize );
      }
      else if ( WLAN_PARAM_SignedInteger == pRegEntry->RegType )
      {
         // If successfully read from the registry, use the value read.
         // If not, use the default value.
         if (VOS_STATUS_SUCCESS == match_status)
         {
            rv = kstrtos32(value_str, 10, &svalue);
            if (rv < 0) {
                hddLog(VOS_TRACE_LEVEL_WARN, "%s: Reg Parameter %s invalid. Enforcing Default",
                       __func__, pRegEntry->RegName);
                svalue = (v_S31_t)pRegEntry->VarDefault;
            }
         }
         else
         {
            svalue = (v_S31_t)pRegEntry->VarDefault;
         }

         // If this parameter needs range checking, do it here.
         if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK )
         {
            if ( svalue > (v_S31_t)pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum "
                      "[%d > %d]. Enforcing Maximum", __func__,
                      pRegEntry->RegName, svalue, (int)pRegEntry->VarMax );
               svalue = (v_S31_t)pRegEntry->VarMax;
            }

            if ( svalue < (v_S31_t)pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum "
                      "[%d < %d]. Enforcing Minimum",  __func__,
                      pRegEntry->RegName, svalue, (int)pRegEntry->VarMin);
               svalue = (v_S31_t)pRegEntry->VarMin;
            }
         }
         // If this parameter needs range checking, do it here.
         else if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT )
         {
            if ( svalue > (v_S31_t)pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum "
                      "[%d > %d]. Enforcing Default= %d",
                      __func__, pRegEntry->RegName, svalue,
                      (int)pRegEntry->VarMax,
                      (int)pRegEntry->VarDefault  );
               svalue = (v_S31_t)pRegEntry->VarDefault;
            }

            if ( svalue < (v_S31_t)pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum "
                      "[%d < %d]. Enforcing Default= %d",
                      __func__, pRegEntry->RegName, svalue,
                      (int)pRegEntry->VarMin,
                      (int)pRegEntry->VarDefault);
               svalue = pRegEntry->VarDefault;
            }
         }

         // Move the variable into the output field.
         memcpy( pField, &svalue, pRegEntry->VarSize );
      }
      // Handle string parameters
      else if ( WLAN_PARAM_String == pRegEntry->RegType )
      {
#ifdef WLAN_CFG_DEBUG
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "RegName = %s, VarOffset %u VarSize %u VarDefault %s",
            pRegEntry->RegName, pRegEntry->VarOffset, pRegEntry->VarSize, (char*)pRegEntry->VarDefault);
#endif

         if ( match_status == VOS_STATUS_SUCCESS)
         {
            len_value_str = strlen(value_str);

            if(len_value_str > (pRegEntry->VarSize - 1)) {
               hddLog(LOGE, "%s: Invalid Value=[%s] specified for Name=[%s] in %s",
                  __func__, value_str, pRegEntry->RegName, WLAN_INI_FILE);
               cbOutString = utilMin( strlen( (char *)pRegEntry->VarDefault ), pRegEntry->VarSize - 1 );
               memcpy( pField, (void *)(pRegEntry->VarDefault), cbOutString );
               ( (v_U8_t *)pField )[ cbOutString ] = '\0';
            }
            else
            {
               memcpy( pField, (void *)(value_str), len_value_str);
               ( (v_U8_t *)pField )[ len_value_str ] = '\0';
            }
         }
         else
         {
            // Failed to read the string parameter from the registry.  Use the default.
            cbOutString = utilMin( strlen( (char *)pRegEntry->VarDefault ), pRegEntry->VarSize - 1 );
            memcpy( pField, (void *)(pRegEntry->VarDefault), cbOutString );
            ( (v_U8_t *)pField )[ cbOutString ] = '\0';
         }
      }
      else if ( WLAN_PARAM_MacAddr == pRegEntry->RegType )
      {
         if(pRegEntry->VarSize != VOS_MAC_ADDR_SIZE) {
               hddLog(LOGE, "%s: Invalid VarSize %u for Name=[%s]",
                   __func__, pRegEntry->VarSize, pRegEntry->RegName);
            continue;
         }
         candidate = (char*)pRegEntry->VarDefault;
         if ( match_status == VOS_STATUS_SUCCESS) {
            len_value_str = strlen(value_str);
            if(len_value_str != (VOS_MAC_ADDR_SIZE*2)) {
               hddLog(LOGE, "%s: Invalid MAC addr [%s] specified for Name=[%s] in %s",
                  __func__, value_str, pRegEntry->RegName, WLAN_INI_FILE);
            }
            else
               candidate = value_str;
         }
         //parse the string and store it in the byte array
         for(i=0; i<VOS_MAC_ADDR_SIZE; i++)
         {
            ((char*)pField)[i] =
              (char)(parseHexDigit(candidate[i*2])*16 + parseHexDigit(candidate[i*2+1]));
         }
      }
      else
      {
         hddLog(LOGE, "%s: Unknown param type for name[%s] in registry table",
            __func__, pRegEntry->RegName);
      }

      // did we successfully parse a cfg item for this parameter?
      if( (match_status == VOS_STATUS_SUCCESS) &&
          (idx < MAX_CFG_INI_ITEMS) )
      {
         set_bit(idx, (void *)&pHddCtx->cfg_ini->bExplicitCfg);
      }
   }

   print_hdd_cfg(pHddCtx);

  return( ret_status );
}

eCsrPhyMode hdd_cfg_xlate_to_csr_phy_mode( eHddDot11Mode dot11Mode )
{
   switch (dot11Mode)
   {
      case (eHDD_DOT11_MODE_abg):
         return eCSR_DOT11_MODE_abg;
      case (eHDD_DOT11_MODE_11b):
         return eCSR_DOT11_MODE_11b;
      case (eHDD_DOT11_MODE_11g):
         return eCSR_DOT11_MODE_11g;
      default:
      case (eHDD_DOT11_MODE_11n):
         return eCSR_DOT11_MODE_11n;
      case (eHDD_DOT11_MODE_11g_ONLY):
         return eCSR_DOT11_MODE_11g_ONLY;
      case (eHDD_DOT11_MODE_11n_ONLY):
         return eCSR_DOT11_MODE_11n_ONLY;
      case (eHDD_DOT11_MODE_11b_ONLY):
         return eCSR_DOT11_MODE_11b_ONLY;
#ifdef WLAN_FEATURE_11AC
      case (eHDD_DOT11_MODE_11ac_ONLY):
         return eCSR_DOT11_MODE_11ac_ONLY;
      case (eHDD_DOT11_MODE_11ac):
         return eCSR_DOT11_MODE_11ac;
#endif
      case (eHDD_DOT11_MODE_AUTO):
         return eCSR_DOT11_MODE_AUTO;
   }

}

static void hdd_set_btc_config(hdd_context_t *pHddCtx)
{
   hdd_config_t *pConfig = pHddCtx->cfg_ini;
   tSmeBtcConfig btcParams;
   int i;

   sme_BtcGetConfig(pHddCtx->hHal, &btcParams);

   btcParams.btcExecutionMode = pConfig->btcExecutionMode;
   btcParams.btcConsBtSlotsToBlockDuringDhcp = pConfig->btcConsBtSlotsToBlockDuringDhcp;
   btcParams.btcA2DPBtSubIntervalsDuringDhcp = pConfig->btcA2DPBtSubIntervalsDuringDhcp;

   btcParams.btcStaticLenInqBt = pConfig->btcStaticLenInqBt;
   btcParams.btcStaticLenPageBt = pConfig->btcStaticLenPageBt;
   btcParams.btcStaticLenConnBt = pConfig->btcStaticLenConnBt;
   btcParams.btcStaticLenLeBt = pConfig->btcStaticLenLeBt;
   btcParams.btcStaticLenInqWlan = pConfig->btcStaticLenInqWlan;
   btcParams.btcStaticLenPageWlan = pConfig->btcStaticLenPageWlan;
   btcParams.btcStaticLenConnWlan = pConfig->btcStaticLenConnWlan;
   btcParams.btcStaticLenLeWlan = pConfig->btcStaticLenLeWlan;
   btcParams.btcDynMaxLenBt = pConfig->btcDynMaxLenBt;
   btcParams.btcDynMaxLenWlan = pConfig->btcDynMaxLenWlan;
   btcParams.btcMaxScoBlockPerc = pConfig->btcMaxScoBlockPerc;
   btcParams.btcDhcpProtOnA2dp = pConfig->btcDhcpProtOnA2dp;
   btcParams.btcDhcpProtOnSco = pConfig->btcDhcpProtOnSco;

   for (i = 0; i < 10; i++)
   {
      btcParams.mwsCoexVictimWANFreq[i] = pConfig->mwsCoexVictimWANFreq[i];
      btcParams.mwsCoexVictimWLANFreq[i] = pConfig->mwsCoexVictimWLANFreq[i];
      btcParams.mwsCoexVictimConfig[i] = pConfig->mwsCoexVictimConfig[i];
      btcParams.mwsCoexVictimConfig2[i] = pConfig->mwsCoexVictimConfig2[i];
   }
   for (i = 0; i < 6; i++)
   {
      btcParams.mwsCoexConfig[i] = pConfig->mwsCoexConfig[i];
   }
   btcParams.mwsCoexModemBackoff = pConfig->mwsCoexModemBackoff;
   btcParams.SARPowerBackoff = pConfig->SARPowerBackoff;

   sme_BtcSetConfig(pHddCtx->hHal, &btcParams);
}

static void hdd_set_power_save_config(hdd_context_t *pHddCtx, tSmeConfigParams *smeConfig)
{
   hdd_config_t *pConfig = pHddCtx->cfg_ini;

   tPmcBmpsConfigParams bmpsParams;

   sme_GetConfigPowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE, &bmpsParams);

   if (strcmp(pConfig->PowerUsageControl, "Min") == 0)
   {
      smeConfig->csrConfig.impsSleepTime   = pConfig->nImpsMinSleepTime;
      bmpsParams.bmpsPeriod                = pConfig->nBmpsMinListenInterval;
      bmpsParams.enableBeaconEarlyTermination = pConfig->fEnableBeaconEarlyTermination;
      bmpsParams.bcnEarlyTermWakeInterval  = pConfig->bcnEarlyTermWakeInterval;
   }
   if (strcmp(pConfig->PowerUsageControl, "Max") == 0)
   {
      smeConfig->csrConfig.impsSleepTime   = pConfig->nImpsMaxSleepTime;
      bmpsParams.bmpsPeriod                = pConfig->nBmpsMaxListenInterval;
      bmpsParams.enableBeaconEarlyTermination = pConfig->fEnableBeaconEarlyTermination;
      bmpsParams.bcnEarlyTermWakeInterval  = pConfig->bcnEarlyTermWakeInterval;
   }
   if (strcmp(pConfig->PowerUsageControl, "Mod") == 0)
   {
      smeConfig->csrConfig.impsSleepTime   = pConfig->nImpsModSleepTime;
      bmpsParams.bmpsPeriod                = pConfig->nBmpsModListenInterval;
      bmpsParams.enableBeaconEarlyTermination = pConfig->fEnableBeaconEarlyTermination;
      bmpsParams.bcnEarlyTermWakeInterval  = pConfig->bcnEarlyTermWakeInterval;
   }

   if (pConfig->fIsImpsEnabled)
   {
      sme_EnablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   }
   else
   {
      sme_DisablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   }

  /*If isAndroidPsEn is 1 then Host driver and above layers control the PS mechanism
    If Value set to 0 Driver/Core Stack internally control the Power saving mechanism */
   sme_SetHostPowerSave (pHddCtx->hHal, pConfig->isAndroidPsEn);

   if (pConfig->fIsBmpsEnabled)
   {
      sme_EnablePowerSave (pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   }
   else
   {
      sme_DisablePowerSave (pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   }

   bmpsParams.trafficMeasurePeriod = pConfig->nAutoBmpsTimerValue;

   if (sme_SetConfigPowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE, &bmpsParams)== eHAL_STATUS_FAILURE)
   {
      hddLog(LOGE, "SetConfigPowerSave failed to set BMPS params");
   }

   if(pConfig->fIsAutoBmpsTimerEnabled)
   {
      sme_StartAutoBmpsTimer(pHddCtx->hHal);
   }

}

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
static VOS_STATUS hdd_string_to_u8_array( char *str, tANI_U8 *intArray, tANI_U8 *len, tANI_U8 intArrayMaxLen )
{
   char *s = str;

   if( str == NULL || intArray == NULL || len == NULL )
   {
      return VOS_STATUS_E_INVAL;
   }
   *len = 0;

   while ( (s != NULL) && (*len < intArrayMaxLen) )
   {
      int val;
      //Increment length only if sscanf succesfully extracted one element.
      //Any other return value means error. Ignore it.
      if( sscanf(s, "%d", &val ) == 1 )
      {
         intArray[*len] = (tANI_U8) val;
         *len += 1;
      }
      s = strpbrk( s, "," );
      if( s )
         s++;
   }

   return VOS_STATUS_SUCCESS;

}
#endif


v_BOOL_t hdd_update_config_dat( hdd_context_t *pHddCtx )
{
   v_BOOL_t  fStatus = TRUE;
#ifdef WLAN_SOFTAP_VSTA_FEATURE
   tANI_U32 val;
#endif

   hdd_config_t *pConfig = pHddCtx->cfg_ini;
   tSirMacHTCapabilityInfo *htCapInfo;
   tANI_U32 temp32;
   tANI_U16 temp16;

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SHORT_GI_20MHZ,
      pConfig->ShortGI20MhzEnable, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_SHORT_GI_20MHZ to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_CAL_CONTROL, pConfig->Calibration,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_CAL_CONTROL to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_CAL_PERIOD, pConfig->CalibrationPeriod,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_CAL_PERIOD to CCM");
   }

   if ( 0 != pConfig->Cfg1Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg1Id, pConfig->Cfg1Value, NULL,
         eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg1Id to CCM");
      }

   }

   if ( 0 != pConfig->Cfg2Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg2Id, pConfig->Cfg2Value,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg2Id to CCM");
      }
   }

   if ( 0 != pConfig->Cfg3Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg3Id, pConfig->Cfg3Value,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg3Id to CCM");
      }
   }

   if ( 0 != pConfig->Cfg4Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg4Id, pConfig->Cfg4Value,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg4Id to CCM");
      }
   }

   if ( 0 != pConfig->Cfg5Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg5Id, pConfig->Cfg5Value,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg5Id to CCM");
      }
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_BA_AUTO_SETUP, pConfig->BlockAckAutoSetup,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_BA_AUTO_SETUP to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_FIXED_RATE, pConfig->TxRate,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_FIXED_RATE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_RX_AMPDU_FACTOR,
      pConfig->MaxRxAmpduFactor, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Could not pass on WNI_CFG_HT_AMPDU_PARAMS_MAX_RX_AMPDU_FACTOR to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SHORT_PREAMBLE, pConfig->fIsShortPreamble,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Could not pass on WNI_CFG_SHORT_PREAMBLE to CCM");
   }

   if (pConfig->fIsAutoIbssBssid)
   {
      if (ccmCfgSetStr(pHddCtx->hHal, WNI_CFG_BSSID, (v_U8_t *)"000000000000",
         sizeof(v_BYTE_t) * VOS_MAC_ADDR_SIZE, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE,"Could not pass on WNI_CFG_BSSID to CCM");
      }
   }
   else
   {
      if ( VOS_FALSE == vos_is_macaddr_group( &pConfig->IbssBssid ))
      {
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_MED,
                    "MAC Addr (IBSS BSSID) read from Registry is: " MAC_ADDRESS_STR,
                    MAC_ADDR_ARRAY(pConfig->IbssBssid.bytes));
         if (ccmCfgSetStr(pHddCtx->hHal, WNI_CFG_BSSID, pConfig->IbssBssid.bytes,
            sizeof(v_BYTE_t) * VOS_MAC_ADDR_SIZE, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
         {
            fStatus = FALSE;
            hddLog(LOGE,"Could not pass on WNI_CFG_BSSID to CCM");
         }
      }
      else
      {
         fStatus = FALSE;
         hddLog(LOGE,"Could not pass on WNI_CFG_BSSID to CCM");
      }
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PASSIVE_MINIMUM_CHANNEL_TIME,
        pConfig->nPassiveMinChnTime, NULL,
        eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_PASSIVE_MINIMUM_CHANNEL_TIME"
                     " to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME,
        pConfig->nPassiveMaxChnTime, NULL,
        eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME"
                     " to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_BEACON_INTERVAL, pConfig->nBeaconInterval,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_BEACON_INTERVAL to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_PS_POLL, pConfig->nMaxPsPoll,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_PS_POLL to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_CURRENT_RX_ANTENNA, pConfig-> nRxAnt, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_CURRENT_RX_ANTENNA configuration info to HAL"  );
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LOW_GAIN_OVERRIDE, pConfig->fIsLowGainOverride,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_LOW_GAIN_OVERRIDE to HAL");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RSSI_FILTER_PERIOD, pConfig->nRssiFilterPeriod,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_RSSI_FILTER_PERIOD to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, pConfig->fIgnoreDtim,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_IGNORE_DTIM configuration to CCM"  );
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_ENABLE_HEART_BEAT, pConfig->fEnableFwHeartBeatMonitoring,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_PS_HEART_BEAT configuration info to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_ENABLE_BCN_FILTER, pConfig->fEnableFwBeaconFiltering,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_PS_BCN_FILTER configuration info to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_ENABLE_RSSI_MONITOR, pConfig->fEnableFwRssiMonitoring,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_PS_RSSI_MONITOR configuration info to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT, pConfig->nDataInactivityTimeout,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT configuration info to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_NTH_BEACON_FILTER, pConfig->nthBeaconFilter,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_NTH_BEACON_FILTER configuration info to CCM");
   }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_LTE_COEX, pConfig->enableLTECoex,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_LTE_COEX to CCM");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_PHY_AGC_LISTEN_MODE, pConfig->nEnableListenMode,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_PHY_AGC_LISTEN_MODE to CCM");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_KEEP_ALIVE_TIMEOUT, pConfig->apKeepAlivePeriod,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_AP_KEEP_ALIVE_TIMEOUT to CCM");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_GO_KEEP_ALIVE_TIMEOUT, pConfig->goKeepAlivePeriod,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_GO_KEEP_ALIVE_TIMEOUT to CCM");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_LINK_MONITOR_TIMEOUT, pConfig->apLinkMonitorPeriod,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_AP_LINK_MONITOR_TIMEOUT to CCM");
     }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_GO_LINK_MONITOR_TIMEOUT, pConfig->goLinkMonitorPeriod,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_GO_LINK_MONITOR_TIMEOUT to CCM");
     }


#if defined WLAN_FEATURE_VOWIFI
    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RRM_ENABLED, pConfig->fRrmEnable,
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RRM_ENABLE configuration info to CCM");
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RRM_OPERATING_CHAN_MAX, pConfig->nInChanMeasMaxDuration,
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RRM_OPERATING_CHAN_MAX configuration info to CCM");
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RRM_NON_OPERATING_CHAN_MAX, pConfig->nOutChanMeasMaxDuration,
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RRM_OUT_CHAN_MAX configuration info to CCM");
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MCAST_BCAST_FILTER_SETTING, pConfig->mcastBcastFilterSetting,
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
#endif

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SINGLE_TID_RC, pConfig->bSingleTidRc,
                      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_SINGLE_TID_RC configuration info to CCM");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_WAKEUP_EN, pConfig->teleBcnWakeupEn,
                      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_WAKEUP_EN configuration info to CCM"  );
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_TRANS_LI, pConfig->nTeleBcnTransListenInterval,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_TRANS_LI configuration info to CCM"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_MAX_LI, pConfig->nTeleBcnMaxListenInterval,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_MAX_LI configuration info to CCM"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_TRANS_LI_IDLE_BCNS, pConfig->nTeleBcnTransLiNumIdleBeacons,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_TRANS_LI_IDLE_BCNS configuration info to CCM"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_MAX_LI_IDLE_BCNS, pConfig->nTeleBcnMaxLiNumIdleBeacons,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_MAX_LI_IDLE_BCNS configuration info to CCM"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RF_SETTLING_TIME_CLK, pConfig->rfSettlingTimeUs,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RF_SETTLING_TIME_CLK configuration info to CCM"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD, pConfig->infraStaKeepAlivePeriod,
                      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD configuration info to CCM"  );
     }
    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_DYNAMIC_PS_POLL_VALUE, pConfig->dynamicPsPollValue,
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_DYNAMIC_PS_POLL_VALUE configuration info to CCM"  );
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_NULLDATA_AP_RESP_TIMEOUT, pConfig->nNullDataApRespTimeout,
               NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_PS_NULLDATA_DELAY_TIMEOUT configuration info to CCM"  );
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD, pConfig->apDataAvailPollPeriodInMs,
               NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD configuration info to CCM"  );
    }
    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_FRAGMENTATION_THRESHOLD, pConfig->FragmentationThreshold,
                   NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_FRAGMENTATION_THRESHOLD configuration info to CCM"  );
    }
    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RTS_THRESHOLD, pConfig->RTSThreshold,
                        NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RTS_THRESHOLD configuration info to CCM"  );
    }

    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_11D_ENABLED, pConfig->Is11dSupportEnabled,
                        NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_11D_ENABLED configuration info to CCM"  );
    }
    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_HEART_BEAT_THRESHOLD, pConfig->HeartbeatThresh24,
                        NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_HEART_BEAT_THRESHOLD configuration info to CCM"  );
    }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD, pConfig->apDataAvailPollPeriodInMs,
               NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD configuration info to CCM"  );
   }

   if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_CLOSE_LOOP,
                   pConfig->enableCloseLoop, NULL, eANI_BOOLEAN_FALSE)
       ==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_CLOSE_LOOP to CCM");
   }

   if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TX_PWR_CTRL_ENABLE,
                   pConfig->enableAutomaticTxPowerControl, NULL, eANI_BOOLEAN_FALSE)
                   ==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TX_PWR_CTRL_ENABLE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SHORT_GI_40MHZ,
      pConfig->ShortGI40MhzEnable, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_SHORT_GI_40MHZ to CCM");
   }


     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_MC_ADDR_LIST, pConfig->fEnableMCAddrList,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_MC_ADDR_LIST to CCM");
     }

#ifdef WLAN_FEATURE_11AC
   /* Based on cfg.ini, update the Basic MCS set, RX/TX MCS map in the cfg.dat */
   /* valid values are 0(MCS0-7), 1(MCS0-8), 2(MCS0-9) */
   /* we update only the least significant 2 bits in the corresponding fields */
   if( (pConfig->dot11Mode == eHDD_DOT11_MODE_AUTO) ||
       (pConfig->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY) ||
       (pConfig->dot11Mode == eHDD_DOT11_MODE_11ac) )
   {
       {
           tANI_U32 temp = 0;

           ccmCfgGetInt(pHddCtx->hHal, WNI_CFG_VHT_BASIC_MCS_SET, &temp);
           temp = (temp & 0xFFFC) | pConfig->vhtRxMCS;

           if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_VHT_BASIC_MCS_SET,
                           temp, NULL, eANI_BOOLEAN_FALSE)
               ==eHAL_STATUS_FAILURE)
           {
               fStatus = FALSE;
               hddLog(LOGE, "Could not pass on WNI_CFG_VHT_BASIC_MCS_SET to CCM");
           }

           ccmCfgGetInt(pHddCtx->hHal, WNI_CFG_VHT_RX_MCS_MAP, &temp);
           temp = (temp & 0xFFFC) | pConfig->vhtRxMCS;

           if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_VHT_RX_MCS_MAP,
                           temp, NULL, eANI_BOOLEAN_FALSE)
               ==eHAL_STATUS_FAILURE)
           {
              fStatus = FALSE;
              hddLog(LOGE, "Could not pass on WNI_CFG_VHT_RX_MCS_MAP to CCM");
           }

           ccmCfgGetInt(pHddCtx->hHal, WNI_CFG_VHT_TX_MCS_MAP, &temp);
           temp = (temp & 0xFFFC) | pConfig->vhtTxMCS;

           if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_VHT_TX_MCS_MAP,
                           temp, NULL, eANI_BOOLEAN_FALSE)
               ==eHAL_STATUS_FAILURE)
           {
               fStatus = FALSE;
               hddLog(LOGE, "Could not pass on WNI_CFG_VHT_TX_MCS_MAP to CCM");
           }
           /* Currently shortGI40Mhz is used for shortGI80Mhz */
           if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_VHT_SHORT_GI_80MHZ,
               pConfig->ShortGI40MhzEnable, NULL, eANI_BOOLEAN_FALSE)
               == eHAL_STATUS_FAILURE)
           {
               fStatus = FALSE;
               hddLog(LOGE, "Could not pass WNI_VHT_SHORT_GI_80MHZ to CCM\n");
           }
           if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_VHT_AMPDU_LEN_EXPONENT,
               pConfig->gVhtMaxAmpduLenExp, NULL, eANI_BOOLEAN_FALSE)
               ==eHAL_STATUS_FAILURE)
           {
               fStatus = FALSE;
               hddLog(LOGE, "Could not pass on WNI_CFG_VHT_AMPDU_LEN_EXPONENT to CCM");
           }

       }
   }
#endif

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_NUM_BUFF_ADVERT,pConfig->numBuffAdvert,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_NUM_BUFF_ADVERT to CCM");
     }

     if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_HT_RX_STBC,
                     pConfig->enableRxSTBC, NULL, eANI_BOOLEAN_FALSE)
         ==eHAL_STATUS_FAILURE)
     {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on WNI_CFG_HT_RX_STBC to CCM");
     }

     ccmCfgGetInt(pHddCtx->hHal, WNI_CFG_HT_CAP_INFO, &temp32);
     temp16 = temp32 & 0xffff;
     htCapInfo = (tSirMacHTCapabilityInfo *)&temp16;
     htCapInfo->rxSTBC = pConfig->enableRxSTBC;
     temp32 = temp16;

     if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_HT_CAP_INFO,
                     temp32, NULL, eANI_BOOLEAN_FALSE)
         ==eHAL_STATUS_FAILURE)
     {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on WNI_CFG_HT_CAP_INFO to CCM");
     }

     if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_VHT_RXSTBC,
                     pConfig->enableRxSTBC, NULL, eANI_BOOLEAN_FALSE)
         ==eHAL_STATUS_FAILURE)
     {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on WNI_CFG_VHT_RXSTBC to CCM");
     }

#ifdef WLAN_SOFTAP_VSTA_FEATURE
     if(pConfig->fEnableVSTASupport)
     {
        ccmCfgGetInt(pHddCtx->hHal, WNI_CFG_ASSOC_STA_LIMIT, &val);
        if( val <= WNI_CFG_ASSOC_STA_LIMIT_STADEF)
            val = WNI_CFG_ASSOC_STA_LIMIT_STAMAX;
     }
     else
     {
            val = WNI_CFG_ASSOC_STA_LIMIT_STADEF;
     }
     if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ASSOC_STA_LIMIT, val,
         NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
     {
         fStatus = FALSE;
         hddLog(LOGE,"Failure: Could not pass on WNI_CFG_ASSOC_STA_LIMIT configuration info to CCM"  );
     }
#endif
   if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_LPWR_IMG_TRANSITION,
                   pConfig->enableLpwrImgTransition, NULL, eANI_BOOLEAN_FALSE)
       ==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_LPWR_IMG_TRANSITION to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, pConfig->enableMCCAdaptiveScheduler,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED to CCM");
   }
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_DISABLE_LDPC_WITH_TXBF_AP, pConfig->disableLDPCWithTxbfAP,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_DISABLE_LDPC_WITH_TXBF_AP to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_DYNAMIC_THRESHOLD_ZERO, pConfig->retryLimitZero,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_DYNAMIC_THRESHOLD_ZERO to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_DYNAMIC_THRESHOLD_ONE, pConfig->retryLimitOne,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_DYNAMIC_THRESHOLD_ONE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_DYNAMIC_THRESHOLD_TWO, pConfig->retryLimitTwo,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_DYNAMIC_THRESHOLD_TWO to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_MEDIUM_TIME, pConfig->cfgMaxMediumTime,
      NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_MEDIUM_TIME to CCM");
   }

#ifdef FEATURE_WLAN_TDLS

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TDLS_QOS_WMM_UAPSD_MASK,
                    pConfig->fTDLSUapsdMask, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TDLS_QOS_WMM_UAPSD_MASK to CCM");
   }

   if (TRUE == pConfig->fEnableTDLSScanCoexSupport)
   {
      /* TDLSScanCoexistance feature is supported when the DUT acts as only
       * the Sleep STA and hence explicitly disable the BufferSta capability
       * on the DUT. DUT's Buffer STA capability is explicitly disabled to
       * ensure that the TDLS peer shall not go to TDLS power save mode.
       */
      pConfig->fEnableTDLSBufferSta = FALSE;
   }
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TDLS_BUF_STA_ENABLED,
                    pConfig->fEnableTDLSBufferSta, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TDLS_BUF_STA_ENABLED to CCM");
   }
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TDLS_PUAPSD_INACT_TIME,
                    pConfig->fTDLSPuapsdInactivityTimer, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TDLS_PUAPSD_INACT_TIME to CCM");
   }
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TDLS_RX_FRAME_THRESHOLD,
                    pConfig->fTDLSRxFrameThreshold, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TDLS_RX_FRAME_THRESHOLD to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TDLS_OFF_CHANNEL_ENABLED,
                    pConfig->fEnableTDLSOffChannel, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TDLS_BUF_STA_ENABLED to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TDLS_WMM_MODE_ENABLED,
                    pConfig->fEnableTDLSWmmMode, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TDLS_WMM_MODE_ENABLED to CCM\n");
   }

#endif

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_ADAPT_RX_DRAIN,
                    pConfig->fEnableAdaptRxDrain, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_ADAPT_RX_DRAIN to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_FLEX_CONNECT_POWER_FACTOR,
                    pConfig->flexConnectPowerFactor, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on "
             "WNI_CFG_FLEX_CONNECT_POWER_FACTOR to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ANTENNA_DIVESITY,
                    pConfig->antennaDiversity, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ANTENNA_DIVESITY to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ATH_DISABLE,
                    pConfig->cfgAthDisable, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ATH_DISABLE to CCM");
   }

  if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_ACTIVE_WLAN_LEN,
                    pConfig->cfgBtcActiveWlanLen,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on WNI_BTC_ACTIVE_WLAN_LEN to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_ACTIVE_BT_LEN,
                    pConfig->cfgBtcActiveBtLen,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on WNI_BTC_ACTIVE_BT_LEN to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_SAP_ACTIVE_WLAN_LEN,
                    pConfig->cfgBtcSapActiveWlanLen,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on WNI_BTC_ACTIVE_WLAN_LEN to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_SAP_ACTIVE_BT_LEN,
                    pConfig->cfgBtcSapActiveBtLen,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on WNI_BTC_ACTIVE_BT_LEN to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ASD_PROBE_INTERVAL,
                    pConfig->gAsdProbeInterval, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ASD_PROBE_INTERVAL to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ASD_TRIGGER_THRESHOLD,
                    pConfig->gAsdTriggerThreshold, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ASD_TRIGGER_THRESHOLD to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ASD_RTT_RSSI_HYST_THRESHOLD,
                    pConfig->gAsdRTTRssiHystThreshold, NULL,
                    eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ASD_RSSI_HYST_THRESHOLD to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_DEBUG_P2P_REMAIN_ON_CHANNEL,
                    pConfig->debugP2pRemainOnChannel,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE,
              "Could not pass on WNI_CFG_DEBUG_P2P_REMAIN_ON_CHANNEL to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_CTS2S_DURING_SCO,
                    pConfig->cfgBtcCTS2SduringSCO,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on WNI_CFG_BTC_CTS2S_DURING_SCO to CCM");
   }

   if(ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_DEFAULT_RATE_INDEX_24GHZ,
                    defHddRateToDefCfgRate(pConfig->defaultRateIndex24Ghz),
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on WNI_CFG_DEFAULT_RATE_INDEX_24GHZ to"
                    " CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RA_FILTER_ENABLE, pConfig->cfgRAFilterEnable,
      NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_RA_FILTER_ENABLE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RA_RATE_LIMIT_INTERVAL, pConfig->cfgRARateLimitInterval,
      NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_RA_FILTER_ENABLE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE,
                    pConfig->cfgBtcFatalHidnSniffBlkGuidance,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
			       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on"
                     "WNI_CFG_BTC_FATAL_HID_NSNIFF_BLK_GUIDANCE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE,
                    pConfig->cfgBtcCriticalHidnSniffBlkGuidance,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
			       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on"
                    "WNI_CFG_BTC_CRITICAL_HID_NSNIFF_BLK_GUIDANCE to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD,
                    pConfig->cfgBtcA2dpTxQueueThold,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
			       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on"
                    "WNI_CFG_BTC_DYN_A2DP_TX_QUEUE_THOLD to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal,
                    WNI_CFG_BTC_DYN_OPP_TX_QUEUE_THOLD,
                    pConfig->cfgBtcOppTxQueueThold,
                    NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
			       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on"
                    "WNI_CFG_BTC_DYN_OPP_TX_QUEUE_THOLD to CCM");
   }

#ifdef WLAN_FEATURE_11W
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PMF_SA_QUERY_MAX_RETRIES,
                    pConfig->pmfSaQueryMaxRetries, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_SA_QUERY_MAX_RETRIES to CCM");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PMF_SA_QUERY_RETRY_INTERVAL,
                    pConfig->pmfSaQueryRetryInterval, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_SA_QUERY_RETRY_INTERVAL to CCM");
   }
#endif
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_UAPSD_CONSEC_SP,
                    pConfig->maxUapsdConsecSP, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_UAPSD_CONSEC_SP");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_UAPSD_CONSEC_RX_CNT,
                    pConfig->maxUapsdConsecRxCnt, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_UAPSD_CONSEC_RX_CNT");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_UAPSD_CONSEC_TX_CNT,
                    pConfig->maxUapsdConsecTxCnt, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_UAPSD_CONSEC_TX_CNT");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW,
                    pConfig->uapsdConsecTxCntMeasWindow, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_UAPSD_CONSEC_TX_CNT_MEAS_WINDOW");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW,
                    pConfig->uapsdConsecRxCntMeasWindow, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_UAPSD_CONSEC_RX_CNT_MEAS_WINDOW");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_PSPOLL_IN_WMM_UAPSD_PS_MODE,
                    pConfig->maxPsPollInWmmUapsdMode, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_PSPOLL_IN_WMM_UAPSD_PS_MODE");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_UAPSD_INACTIVITY_INTERVALS,
                    pConfig->maxUapsdInactivityIntervals, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_UAPSD_INACTIVITY_INTERVALS");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_DYNAMIC_WMMPS,
                    pConfig->enableDynamicWMMPS, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_DYNAMIC_WMMPS");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_BURST_MODE_BE_TXOP_VALUE,
                    pConfig->burstModeTXOPValue, NULL,
                    eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_BURST_MODE_BE_TXOP_VALUE ");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_DYNAMIC_RA_START_RATE,
               pConfig->enableDynamicRAStartRate,
               NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE, "Could not pass on"
               "WNI_CFG_ENABLE_DYNAMIC_RA_START_RATE to CCM");
   }
   return fStatus;
}


/**---------------------------------------------------------------------------

  \brief hdd_init_set_sme_config() -

   This function initializes the sme configuration parameters

  \param  - pHddCtx - Pointer to the HDD Adapter.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_set_sme_config( hdd_context_t *pHddCtx )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   eHalStatus halStatus;
   tpSmeConfigParams smeConfig;

   hdd_config_t *pConfig = pHddCtx->cfg_ini;

   smeConfig = vos_mem_malloc(sizeof(tSmeConfigParams));
    if (NULL == smeConfig)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s smeConfig allocation failed",__func__);
       return eHAL_STATUS_FAILED_ALLOC;
   }
   vos_mem_zero( smeConfig, sizeof( tSmeConfigParams ) );

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
              "%s bWmmIsEnabled=%d 802_11e_enabled=%d dot11Mode=%d", __func__,
              pConfig->WmmMode, pConfig->b80211eIsEnabled, pConfig->dot11Mode);

   // Config params obtained from the registry

   smeConfig->csrConfig.RTSThreshold             = pConfig->RTSThreshold;
   smeConfig->csrConfig.FragmentationThreshold   = pConfig->FragmentationThreshold;
   smeConfig->csrConfig.shortSlotTime            = pConfig->ShortSlotTimeEnabled;
   smeConfig->csrConfig.Is11dSupportEnabled      = pConfig->Is11dSupportEnabled;
   smeConfig->csrConfig.HeartbeatThresh24        = pConfig->HeartbeatThresh24;

   smeConfig->csrConfig.phyMode                  = hdd_cfg_xlate_to_csr_phy_mode ( pConfig->dot11Mode );

   if( (pConfig->dot11Mode == eHDD_DOT11_MODE_abg) ||
       (pConfig->dot11Mode == eHDD_DOT11_MODE_11b) ||
       (pConfig->dot11Mode == eHDD_DOT11_MODE_11g) ||
       (pConfig->dot11Mode == eHDD_DOT11_MODE_11b_ONLY) ||
       (pConfig->dot11Mode == eHDD_DOT11_MODE_11g_ONLY))
   {
       smeConfig->csrConfig.channelBondingMode24GHz  = 0;
       smeConfig->csrConfig.channelBondingMode5GHz  = 0;
   }
   else
   {
       smeConfig->csrConfig.channelBondingMode24GHz   = pConfig->nChannelBondingMode24GHz;
       smeConfig->csrConfig.channelBondingMode5GHz   = pConfig->nChannelBondingMode5GHz;
   }
   smeConfig->csrConfig.TxRate                   = pConfig->TxRate;
   smeConfig->csrConfig.nScanResultAgeCount      = pConfig->ScanResultAgeCount;
   smeConfig->csrConfig.scanAgeTimeNCNPS         = pConfig->nScanAgeTimeNCNPS;
   smeConfig->csrConfig.scanAgeTimeNCPS          = pConfig->nScanAgeTimeNCPS;
   smeConfig->csrConfig.scanAgeTimeCNPS          = pConfig->nScanAgeTimeCNPS;
   smeConfig->csrConfig.scanAgeTimeCPS           = pConfig->nScanAgeTimeCPS;
   smeConfig->csrConfig.AdHocChannel24           = pConfig->OperatingChannel;
   smeConfig->csrConfig.fEnforce11dChannels      = pConfig->fEnforce11dChannels;
   smeConfig->csrConfig.fSupplicantCountryCodeHasPriority     = pConfig->fSupplicantCountryCodeHasPriority;
   smeConfig->csrConfig.fEnforceCountryCodeMatch = pConfig->fEnforceCountryCodeMatch;
   smeConfig->csrConfig.fEnforceDefaultDomain    = pConfig->fEnforceDefaultDomain;
   smeConfig->csrConfig.bCatRssiOffset           = pConfig->nRssiCatGap;
   smeConfig->csrConfig.vccRssiThreshold         = pConfig->nVccRssiTrigger;
   smeConfig->csrConfig.vccUlMacLossThreshold    = pConfig->nVccUlMacLossThreshold;
   smeConfig->csrConfig.nRoamingTime             = pConfig->nRoamingTime;
   smeConfig->csrConfig.IsIdleScanEnabled        = pConfig->nEnableIdleScan;
   smeConfig->csrConfig.nInitialDwellTime        = pConfig->nInitialDwellTime;
   smeConfig->csrConfig.nActiveMaxChnTime        = pConfig->nActiveMaxChnTime;
   smeConfig->csrConfig.nActiveMinChnTime        = pConfig->nActiveMinChnTime;
   smeConfig->csrConfig.nPassiveMaxChnTime       = pConfig->nPassiveMaxChnTime;
   smeConfig->csrConfig.nPassiveMinChnTime       = pConfig->nPassiveMinChnTime;
   smeConfig->csrConfig.nActiveMaxChnTimeBtc     = pConfig->nActiveMaxChnTimeBtc;
   smeConfig->csrConfig.nActiveMinChnTimeBtc     = pConfig->nActiveMinChnTimeBtc;
   smeConfig->csrConfig.disableAggWithBtc        = pConfig->disableAggWithBtc;
#ifdef WLAN_AP_STA_CONCURRENCY
   smeConfig->csrConfig.nActiveMaxChnTimeConc    = pConfig->nActiveMaxChnTimeConc;
   smeConfig->csrConfig.nActiveMinChnTimeConc    = pConfig->nActiveMinChnTimeConc;
   smeConfig->csrConfig.nPassiveMaxChnTimeConc   = pConfig->nPassiveMaxChnTimeConc;
   smeConfig->csrConfig.nPassiveMinChnTimeConc   = pConfig->nPassiveMinChnTimeConc;
   smeConfig->csrConfig.nRestTimeConc            = pConfig->nRestTimeConc;
   smeConfig->csrConfig.nNumStaChanCombinedConc  = pConfig->nNumStaChanCombinedConc;
   smeConfig->csrConfig.nNumP2PChanCombinedConc  = pConfig->nNumP2PChanCombinedConc;

#endif
   smeConfig->csrConfig.Is11eSupportEnabled      = pConfig->b80211eIsEnabled;
   smeConfig->csrConfig.WMMSupportMode           = pConfig->WmmMode;

#if defined WLAN_FEATURE_VOWIFI
   smeConfig->rrmConfig.rrmEnabled = pConfig->fRrmEnable;
   smeConfig->rrmConfig.maxRandnInterval = pConfig->nRrmRandnIntvl;
#endif
   //Remaining config params not obtained from registry
   // On RF EVB beacon using channel 1.
#ifdef WLAN_FEATURE_11AC
    smeConfig->csrConfig.nVhtChannelWidth = pConfig->vhtChannelWidth;
    smeConfig->csrConfig.enableTxBF = pConfig->enableTxBF;
    smeConfig->csrConfig.txBFCsnValue = pConfig->txBFCsnValue;
    smeConfig->csrConfig.enableVhtFor24GHz = pConfig->enableVhtFor24GHzBand;
    /* Consider Mu-beamformee only if SU-beamformee is enabled */
    if ( pConfig->enableTxBF )
        smeConfig->csrConfig.enableMuBformee = pConfig->enableMuBformee;
    else
        smeConfig->csrConfig.enableMuBformee = 0;
#endif
   smeConfig->csrConfig.AdHocChannel5G            = pConfig->AdHocChannel5G;
   smeConfig->csrConfig.AdHocChannel24            = pConfig->AdHocChannel24G;
   smeConfig->csrConfig.ProprietaryRatesEnabled   = 0;
   smeConfig->csrConfig.HeartbeatThresh50         = 40;
   smeConfig->csrConfig.bandCapability            = pConfig->nBandCapability;
   if (pConfig->nBandCapability == eCSR_BAND_24)
   {
       smeConfig->csrConfig.Is11hSupportEnabled       = 0;
   } else {
       smeConfig->csrConfig.Is11hSupportEnabled       = pConfig->Is11hSupportEnabled;
   }
   smeConfig->csrConfig.cbChoice                  = 0;
   smeConfig->csrConfig.bgScanInterval            = 0;
   smeConfig->csrConfig.eBand                     = pConfig->nBandCapability;
   smeConfig->csrConfig.nTxPowerCap = pConfig->nTxPowerCap;
   smeConfig->csrConfig.fEnableBypass11d          = pConfig->enableBypass11d;
   smeConfig->csrConfig.fEnableDFSChnlScan        = pConfig->enableDFSChnlScan;
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
   smeConfig->csrConfig.nRoamPrefer5GHz           = pConfig->nRoamPrefer5GHz;
   smeConfig->csrConfig.nRoamIntraBand            = pConfig->nRoamIntraBand;
   smeConfig->csrConfig.nProbes                   = pConfig->nProbes;
   smeConfig->csrConfig.nRoamScanHomeAwayTime     = pConfig->nRoamScanHomeAwayTime;
#endif
   smeConfig->csrConfig.fFirstScanOnly2GChnl      = pConfig->enableFirstScan2GOnly;

   //FIXME 11d config is hardcoded
   if ( VOS_STA_SAP_MODE != hdd_get_conparam())
   {
      smeConfig->csrConfig.Csr11dinfo.Channels.numChannels = 0;

      /* if there is a requirement that HDD will control the default
       * channel list & country code (say from .ini file) we need to
       * add some logic here. Otherwise the default 11d info should
       * come from NV as per our current implementation */
   }
   else
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "AP country Code %s", pConfig->apCntryCode);

      if (memcmp(pConfig->apCntryCode, CFG_AP_COUNTRY_CODE_DEFAULT, 3) != 0)
         sme_setRegInfo(pHddCtx->hHal, pConfig->apCntryCode);
      sme_set11dinfo(pHddCtx->hHal, smeConfig);
   }
   hdd_set_power_save_config(pHddCtx, smeConfig);
   hdd_set_btc_config(pHddCtx);

#ifdef WLAN_FEATURE_VOWIFI_11R
   smeConfig->csrConfig.csr11rConfig.IsFTResourceReqSupported = pConfig->fFTResourceReqSupported;
#endif
#ifdef FEATURE_WLAN_LFR
   smeConfig->csrConfig.isFastRoamIniFeatureEnabled = pConfig->isFastRoamIniFeatureEnabled;
   smeConfig->csrConfig.MAWCEnabled = pConfig->MAWCEnabled;
#endif
#ifdef FEATURE_WLAN_ESE
   smeConfig->csrConfig.isEseIniFeatureEnabled = pConfig->isEseIniFeatureEnabled;
   if( pConfig->isEseIniFeatureEnabled )
   {
       pConfig->isFastTransitionEnabled = TRUE;
   }
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
   smeConfig->csrConfig.isFastTransitionEnabled = pConfig->isFastTransitionEnabled;
   smeConfig->csrConfig.RoamRssiDiff = pConfig->RoamRssiDiff;
   smeConfig->csrConfig.nImmediateRoamRssiDiff = pConfig->nImmediateRoamRssiDiff;
   smeConfig->csrConfig.isWESModeEnabled = pConfig->isWESModeEnabled;
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   smeConfig->csrConfig.isRoamOffloadScanEnabled = pConfig->isRoamOffloadScanEnabled;
   smeConfig->csrConfig.bFastRoamInConIniFeatureEnabled = pConfig->bFastRoamInConIniFeatureEnabled;

   if (0 == smeConfig->csrConfig.isRoamOffloadScanEnabled)
   {
       /* Disable roaming in concurrency if roam scan offload is disabled */
       smeConfig->csrConfig.bFastRoamInConIniFeatureEnabled = 0;
   }
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
   smeConfig->csrConfig.neighborRoamConfig.nNeighborReassocRssiThreshold = pConfig->nNeighborReassocRssiThreshold;
   smeConfig->csrConfig.neighborRoamConfig.nNeighborLookupRssiThreshold = pConfig->nNeighborLookupRssiThreshold;
   smeConfig->csrConfig.neighborRoamConfig.nNeighborScanMaxChanTime = pConfig->nNeighborScanMaxChanTime;
   smeConfig->csrConfig.neighborRoamConfig.nNeighborScanMinChanTime = pConfig->nNeighborScanMinChanTime;
   smeConfig->csrConfig.neighborRoamConfig.nNeighborScanTimerPeriod = pConfig->nNeighborScanPeriod;
   smeConfig->csrConfig.neighborRoamConfig.nMaxNeighborRetries = pConfig->nMaxNeighborReqTries;
   smeConfig->csrConfig.neighborRoamConfig.nNeighborResultsRefreshPeriod = pConfig->nNeighborResultsRefreshPeriod;
   smeConfig->csrConfig.neighborRoamConfig.nEmptyScanRefreshPeriod = pConfig->nEmptyScanRefreshPeriod;
   //Making Forced 5G roaming to tightly coupled with the gEnableFirstScan2GOnly=1 only.
   if(pConfig->enableFirstScan2GOnly)
   {
       smeConfig->csrConfig.neighborRoamConfig.nNeighborInitialForcedRoamTo5GhEnable = pConfig->nNeighborInitialForcedRoamTo5GhEnable;
   }
   hdd_string_to_u8_array( pConfig->neighborScanChanList,
                                        smeConfig->csrConfig.neighborRoamConfig.neighborScanChanList.channelList,
                                        &smeConfig->csrConfig.neighborRoamConfig.neighborScanChanList.numChannels,
                                        WNI_CFG_VALID_CHANNEL_LIST_LEN );
#endif

   smeConfig->csrConfig.addTSWhenACMIsOff = pConfig->AddTSWhenACMIsOff;
   smeConfig->csrConfig.fValidateList = pConfig->fValidateScanList;
   smeConfig->csrConfig.allowDFSChannelRoam = pConfig->allowDFSChannelRoam;
   //Enable/Disable MCC
   smeConfig->csrConfig.fEnableMCCMode = pConfig->enableMCC;
   smeConfig->csrConfig.fAllowMCCGODiffBI = pConfig->allowMCCGODiffBI;

   //Scan Results Aging Time out value
   smeConfig->csrConfig.scanCfgAgingTime = pConfig->scanAgingTimeout;

   smeConfig->csrConfig.enableTxLdpc = pConfig->enableTxLdpc;

   smeConfig->csrConfig.isAmsduSupportInAMPDU = pConfig->isAmsduSupportInAMPDU;
   smeConfig->csrConfig.nSelect5GHzMargin = pConfig->nSelect5GHzMargin;
   smeConfig->csrConfig.initialScanSkipDFSCh = pConfig->initialScanSkipDFSCh;

   smeConfig->csrConfig.isCoalesingInIBSSAllowed =
                       pHddCtx->cfg_ini->isCoalesingInIBSSAllowed;


   /* update SSR config */
   sme_UpdateEnableSSR((tHalHandle)(pHddCtx->hHal), pHddCtx->cfg_ini->enableSSR);
   /* Update the Directed scan offload setting */
   smeConfig->fScanOffload =  pHddCtx->cfg_ini->fScanOffload;

   smeConfig->csrConfig.scanBandPreference =
                     pHddCtx->cfg_ini->acsScanBandPreference;

   smeConfig->fEnableDebugLog = pHddCtx->cfg_ini->gEnableDebugLog;
   smeConfig->csrConfig.sendDeauthBeforeCon = pConfig->sendDeauthBeforeCon;

   smeConfig->fDeferIMPSTime = pHddCtx->cfg_ini->deferImpsTime;

   halStatus = sme_UpdateConfig( pHddCtx->hHal, smeConfig);
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      status = VOS_STATUS_E_FAILURE;
      hddLog(LOGE, "sme_UpdateConfig() return failure %d", halStatus);
   }

   vos_mem_free(smeConfig);
   return status;
}


/**---------------------------------------------------------------------------

  \brief hdd_execute_config_command() -

   This function executes an arbitrary configuration set command

  \param - pHddCtx - Pointer to the HDD Adapter.
  \parmm - command - a configuration command of the form:
                     <name>=<value>

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_execute_config_command(hdd_context_t *pHddCtx, char *command)
{
   size_t tableSize = sizeof(g_registry_table)/sizeof(g_registry_table[0]);
   REG_TABLE_ENTRY *pRegEntry;
   char *clone;
   char *pCmd;
   void *pField;
   char *name;
   char *value_str;
   v_U32_t value;
   v_S31_t svalue;
   size_t len_value_str;
   unsigned int idx;
   unsigned int i;
   VOS_STATUS vstatus;
   int rv;

   // assume failure until proven otherwise
   vstatus = VOS_STATUS_E_FAILURE;

   // clone the command so that we can manipulate it
   clone = kstrdup(command, GFP_ATOMIC);
   if (NULL == clone)
   {
      hddLog(LOGE, "%s: memory allocation failure, unable to process [%s]",
             __func__, command);
      return vstatus;
   }

   // 'clone' will point to the beginning of the string so it can be freed
   // 'pCmd' will be used to walk/parse the command
   pCmd = clone;

   // get rid of leading/trailing whitespace
   pCmd = i_trim(pCmd);
   if ('\0' == *pCmd)
   {
      // only whitespace
      hddLog(LOGE, "%s: invalid command, only whitespace:[%s]",
             __func__, command);
      goto done;
   }

   // parse the <name> = <value>
   name = pCmd;
   while (('=' != *pCmd) && ('\0' != *pCmd))
   {
      pCmd++;
   }
   if ('\0' == *pCmd)
   {
      // did not find '='
      hddLog(LOGE, "%s: invalid command, no '=':[%s]",
             __func__, command);
      goto done;
   }

   // replace '=' with NUL to terminate the <name>
   *pCmd++ = '\0';
   name = i_trim(name);
   if ('\0' == *name)
   {
      // did not find a name
      hddLog(LOGE, "%s: invalid command, no <name>:[%s]",
             __func__, command);
      goto done;
   }

   value_str = i_trim(pCmd);
   if ('\0' == *value_str)
   {
      // did not find a value
      hddLog(LOGE, "%s: invalid command, no <value>:[%s]",
             __func__, command);
      goto done;
   }

   // lookup the configuration item
   for (idx = 0; idx < tableSize; idx++)
   {
      if (0 == strcmp(name, g_registry_table[idx].RegName))
      {
         // found a match
         break;
      }
   }
   if (tableSize == idx)
   {
      // did not match the name
      hddLog(LOGE, "%s: invalid command, unknown configuration item:[%s]",
             __func__, command);
      goto done;
   }

   pRegEntry = &g_registry_table[idx];
   if (!(pRegEntry->Flags & VAR_FLAGS_DYNAMIC_CFG))
   {
      // does not support dynamic configuration
      hddLog(LOGE, "%s: invalid command, %s does not support "
             "dynamic configuration", __func__, name);
      goto done;
   }

   pField = ((v_U8_t *)pHddCtx->cfg_ini) + pRegEntry->VarOffset;

   switch (pRegEntry->RegType)
   {
   case WLAN_PARAM_Integer:
      rv = kstrtou32(value_str, 10, &value);
      if (rv < 0)
          goto done;
      if (value < pRegEntry->VarMin)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %u < min value %lu",
                __func__, value, pRegEntry->VarMin);
         goto done;
      }
      if (value > pRegEntry->VarMax)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %u > max value %lu",
                __func__, value, pRegEntry->VarMax);
         goto done;
      }
      memcpy(pField, &value, pRegEntry->VarSize);
      break;

   case WLAN_PARAM_HexInteger:
      rv = kstrtou32(value_str, 16, &value);
      if (rv < 0)
         goto done;
      if (value < pRegEntry->VarMin)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %x < min value %lx",
                __func__, value, pRegEntry->VarMin);
         goto done;
      }
      if (value > pRegEntry->VarMax)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %x > max value %lx",
                __func__, value, pRegEntry->VarMax);
         goto done;
      }
      memcpy(pField, &value, pRegEntry->VarSize);
      break;

   case WLAN_PARAM_SignedInteger:
      rv = kstrtos32(value_str, 10, &svalue);
      if (rv < 0)
         goto done;
      if (svalue < (v_S31_t)pRegEntry->VarMin)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %d < min value %d",
                __func__, svalue, (int)pRegEntry->VarMin);
         goto done;
      }
      if (svalue > (v_S31_t)pRegEntry->VarMax)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %d > max value %d",
                __func__, svalue, (int)pRegEntry->VarMax);
         goto done;
      }
      memcpy(pField, &svalue, pRegEntry->VarSize);
      break;

   case WLAN_PARAM_String:
      len_value_str = strlen(value_str);
      if (len_value_str > (pRegEntry->VarSize - 1))
      {
         // too big
         hddLog(LOGE,
                "%s: invalid command, string [%s] length "
                "%zu exceeds maximum length %u",
                __func__, value_str,
                len_value_str, (pRegEntry->VarSize - 1));
         goto done;
      }
      // copy string plus NUL
      memcpy(pField, value_str, (len_value_str + 1));
      break;

   case WLAN_PARAM_MacAddr:
      len_value_str = strlen(value_str);
      if (len_value_str != (VOS_MAC_ADDR_SIZE * 2))
      {
         // out of range
         hddLog(LOGE,
                "%s: invalid command, MAC address [%s] length "
                "%zu is not expected length %u",
                __func__, value_str,
                len_value_str, (VOS_MAC_ADDR_SIZE * 2));
         goto done;
      }
      //parse the string and store it in the byte array
      for (i = 0; i < VOS_MAC_ADDR_SIZE; i++)
      {
         ((char*)pField)[i] = (char)
            ((parseHexDigit(value_str[(i * 2)]) * 16) +
             parseHexDigit(value_str[(i * 2) + 1]));
      }
      break;

   default:
      goto done;
   }

   // if we get here, we had a successful modification
   vstatus = VOS_STATUS_SUCCESS;

   // config table has been modified, is there a notifier?
   if (NULL != pRegEntry->pfnDynamicNotify)
   {
      (pRegEntry->pfnDynamicNotify)(pHddCtx, pRegEntry->NotifyId);
   }

   // note that this item was explicitly configured
   if (idx < MAX_CFG_INI_ITEMS)
   {
      set_bit(idx, (void *)&pHddCtx->cfg_ini->bExplicitCfg);
   }
 done:
   kfree(clone);
   return vstatus;
}

/**---------------------------------------------------------------------------

  \brief hdd_is_okc_mode_enabled() -

   This function returns whether OKC mode is enabled or not

  \param - pHddCtx - Pointer to the HDD Adapter.

  \return - 1 for enabled, zero for disabled

  --------------------------------------------------------------------------*/

tANI_BOOLEAN hdd_is_okc_mode_enabled(hdd_context_t *pHddCtx)
{
    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: pHddCtx is NULL", __func__);
        return -EINVAL;
    }

#ifdef FEATURE_WLAN_OKC
    return pHddCtx->cfg_ini->isOkcIniFeatureEnabled;
#else
    return eANI_BOOLEAN_FALSE;
#endif
}
