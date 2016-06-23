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

/******************************************************************************
*
* Name:  pmcApi.c
*
* Description: Routines that make up the Power Management Control (PMC) API.
*
*
******************************************************************************/

#include "palTypes.h"
#include "aniGlobal.h"
#include "palTimer.h"
#include "csrLinkList.h"
#include "smsDebug.h"
#include "pmcApi.h"
#include "pmc.h"
#include "cfgApi.h"
#include "smeInside.h"
#include "csrInsideApi.h"
#include "wlan_ps_wow_diag.h"
#include "wlan_qct_wda.h"
#include "limSessionUtils.h"
#include "csrInsideApi.h"

extern void pmcReleaseCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );

void pmcCloseDeferredMsgList(tpAniSirGlobal pMac);
void pmcCloseDeviceStateUpdateList(tpAniSirGlobal pMac);
void pmcCloseRequestStartUapsdList(tpAniSirGlobal pMac);
void pmcCloseRequestBmpsList(tpAniSirGlobal pMac);
void pmcCloseRequestFullPowerList(tpAniSirGlobal pMac);
void pmcClosePowerSaveCheckList(tpAniSirGlobal pMac);

/******************************************************************************
*
* Name:  pmcOpen
*
* Description:
*    Does a PMC open operation on the device.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - open successful
*    eHAL_STATUS_FAILURE - open not successful
*
******************************************************************************/
eHalStatus pmcOpen (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcOpen"));

    /* Initialize basic PMC information about device. */
    pMac->pmc.powerSource = BATTERY_POWER;
    pMac->pmc.pmcState = STOPPED;
    pMac->pmc.pmcReady = FALSE;

    /* Initialize Power Save Modes */
    pMac->pmc.impsEnabled = FALSE;
    pMac->pmc.autoBmpsEntryEnabled = FALSE;
    pMac->pmc.smpsEnabled = FALSE;
    pMac->pmc.uapsdEnabled = TRUE;
    pMac->pmc.bmpsEnabled = TRUE;
    pMac->pmc.standbyEnabled = TRUE;
    pMac->pmc.wowlEnabled = TRUE;
    pMac->pmc.rfSuppliesVotedOff= FALSE;

    vos_mem_set(&(pMac->pmc.bmpsConfig), sizeof(tPmcBmpsConfigParams), 0);
    vos_mem_set(&(pMac->pmc.impsConfig), sizeof(tPmcImpsConfigParams), 0);
    vos_mem_set(&(pMac->pmc.smpsConfig), sizeof(tPmcSmpsConfigParams), 0);

    /* Allocate a timer to use with IMPS. */
    if (vos_timer_init(&pMac->pmc.hImpsTimer, VOS_TIMER_TYPE_SW, pmcImpsTimerExpired, hHal) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot allocate timer for IMPS"));
        return eHAL_STATUS_FAILURE;
    }

    /* Allocate a timer used in Full Power State to measure traffic
       levels and determine when to enter BMPS. */
    if (!VOS_IS_STATUS_SUCCESS(vos_timer_init(&pMac->pmc.hTrafficTimer, 
                                VOS_TIMER_TYPE_SW, pmcTrafficTimerExpired, hHal)))
    {
        pmcLog(pMac, LOGE, FL("Cannot allocate timer for traffic measurement"));
        return eHAL_STATUS_FAILURE;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    /* Allocate a timer used to report current PMC state through periodic DIAG event */
    if (vos_timer_init(&pMac->pmc.hDiagEvtTimer, VOS_TIMER_TYPE_SW, pmcDiagEvtTimerExpired, hHal) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot allocate timer for diag event reporting"));
        return eHAL_STATUS_FAILURE;
    }
#endif

    //Initialize the default value for Bmps related config. 
    pMac->pmc.bmpsConfig.trafficMeasurePeriod = BMPS_TRAFFIC_TIMER_DEFAULT;
    pMac->pmc.bmpsConfig.bmpsPeriod = WNI_CFG_LISTEN_INTERVAL_STADEF;

    /* Allocate a timer used to schedule a deferred power save mode exit. */
    if (vos_timer_init(&pMac->pmc.hExitPowerSaveTimer, VOS_TIMER_TYPE_SW,
                      pmcExitPowerSaveTimerExpired, hHal) !=VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot allocate exit power save mode timer"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }
    
    /* Initialize lists for power save check routines and request full power callback routines. */
    if (csrLLOpen(pMac->hHdd, &pMac->pmc.powerSaveCheckList) != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot initialize power save check routine list"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }
    if (csrLLOpen(pMac->hHdd, &pMac->pmc.requestFullPowerList) != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot initialize request full power callback routine list"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    /* Initialize lists for request BMPS callback routines. */
    if (csrLLOpen(pMac->hHdd, &pMac->pmc.requestBmpsList) !=
      eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, "PMC: cannot initialize request BMPS callback routine list");
        return eHAL_STATUS_FAILURE;
    }

    /* Initialize lists for request start UAPSD callback routines. */
    if (csrLLOpen(pMac->hHdd, &pMac->pmc.requestStartUapsdList) != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, "PMC: cannot initialize request start UAPSD callback routine list");
        return eHAL_STATUS_FAILURE;
    }

    /* Initialize lists for device state update indication callback routines. */
    if (csrLLOpen(pMac->hHdd, &pMac->pmc.deviceStateUpdateIndList) != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, "PMC: cannot initialize device state update indication callback list");
        return eHAL_STATUS_FAILURE;
    }

    if (csrLLOpen(pMac->hHdd, &pMac->pmc.deferredMsgList) != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot initialize deferred msg list"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcStart
*
* Description:
*    Does a PMC start operation on the device.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - start successful
*    eHAL_STATUS_FAILURE - start not successful
*
******************************************************************************/
eHalStatus pmcStart (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSirMacHTMIMOPowerSaveState  htMimoPowerSaveState;

    pmcLog(pMac, LOG2, FL("Entering pmcStart"));

    /* Initialize basic PMC information about device. */
    pMac->pmc.pmcState = FULL_POWER;
    pMac->pmc.requestFullPowerPending = FALSE;
    pMac->pmc.uapsdSessionRequired = FALSE;
    pMac->pmc.wowlModeRequired = FALSE;
    pMac->pmc.wowlExitSrc = eWOWL_EXIT_USER;
    pMac->pmc.bmpsRequestedByHdd = FALSE;
    pMac->pmc.remainInPowerActiveTillDHCP = FALSE;
    pMac->pmc.remainInPowerActiveThreshold = 0;

    /* WLAN Switch initial states. */
    pMac->pmc.hwWlanSwitchState = ePMC_SWITCH_ON;
    pMac->pmc.swWlanSwitchState = ePMC_SWITCH_ON;

    /* No IMPS callback routine yet. */
    pMac->pmc.impsCallbackRoutine = NULL;

    /* No STANDBY callback routine yet. */
    pMac->pmc.standbyCallbackRoutine = NULL;

    /* No WOWL callback routine yet. */
    pMac->pmc.enterWowlCallbackRoutine = NULL;

    /* Initialize BMPS traffic counts. */
    pMac->pmc.cLastTxUnicastFrames = 0;
    pMac->pmc.cLastRxUnicastFrames = 0;
    pMac->pmc.ImpsReqFailed = VOS_FALSE;
    pMac->pmc.ImpsReqFailCnt = 0;
    pMac->pmc.ImpsReqTimerFailed = 0;
    pMac->pmc.ImpsReqTimerfailCnt = 0;

    /* Configure SMPS. */
    if (pMac->pmc.smpsEnabled && (pMac->pmc.powerSource != AC_POWER || pMac->pmc.smpsConfig.enterOnAc))
    {
        if (pMac->pmc.smpsConfig.mode == ePMC_DYNAMIC_SMPS)
            htMimoPowerSaveState = eSIR_HT_MIMO_PS_DYNAMIC;
        if (pMac->pmc.smpsConfig.mode == ePMC_STATIC_SMPS)
            htMimoPowerSaveState = eSIR_HT_MIMO_PS_STATIC;
    }
    else
        htMimoPowerSaveState = eSIR_HT_MIMO_PS_NO_LIMIT;
    
    if (pmcSendMessage(hHal, eWNI_PMC_SMPS_STATE_IND, &htMimoPowerSaveState,
                       sizeof(tSirMacHTMIMOPowerSaveState)) != eHAL_STATUS_SUCCESS)
        return eHAL_STATUS_FAILURE;

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    if (pmcStartDiagEvtTimer(hHal) != eHAL_STATUS_SUCCESS)
    {
       return eHAL_STATUS_FAILURE;
    }
#endif

#if defined(ANI_LOGDUMP)
    pmcDumpInit(hHal);
#endif

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcStop
*
* Description:
*    Does a PMC stop operation on the device.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - stop successful
*    eHAL_STATUS_FAILURE - stop not successful
*
******************************************************************************/
eHalStatus pmcStop (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tListElem *pEntry;
    tPmcDeferredMsg *pDeferredMsg;

    pmcLog(pMac, LOG2, FL("Entering pmcStop"));

    /* Cancel any running timers. */
    if (vos_timer_stop(&pMac->pmc.hImpsTimer) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot cancel IMPS timer"));
    }

    pmcStopTrafficTimer(hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    pmcStopDiagEvtTimer(hHal);
#endif

    if (vos_timer_stop(&pMac->pmc.hExitPowerSaveTimer) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot cancel exit power save mode timer"));
    }

    /* Do all the callbacks. */
    pmcDoCallbacks(hHal, eHAL_STATUS_FAILURE);
    pmcDoBmpsCallbacks(hHal, eHAL_STATUS_FAILURE);
    pMac->pmc.uapsdSessionRequired = FALSE;
    pmcDoStartUapsdCallbacks(hHal, eHAL_STATUS_FAILURE);
    pmcDoStandbyCallbacks(hHal, eHAL_STATUS_FAILURE);

    //purge the deferred msg list
    csrLLLock( &pMac->pmc.deferredMsgList );
    while( NULL != ( pEntry = csrLLRemoveHead( &pMac->pmc.deferredMsgList, eANI_BOOLEAN_FALSE ) ) )
    {
        pDeferredMsg = GET_BASE_ADDR( pEntry, tPmcDeferredMsg, link );
        vos_mem_free(pDeferredMsg);
    }
    csrLLUnlock( &pMac->pmc.deferredMsgList );

    /* PMC is stopped. */
    pMac->pmc.pmcState = STOPPED;
    pMac->pmc.pmcReady = FALSE;

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcClose
*
* Description:
*    Does a PMC close operation on the device.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - close successful
*    eHAL_STATUS_FAILURE - close not successful
*
******************************************************************************/
eHalStatus pmcClose (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcClose"));

    /* Free up allocated resources. */
    if (vos_timer_destroy(&pMac->pmc.hImpsTimer) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot deallocate IMPS timer"));
    }
    if (!VOS_IS_STATUS_SUCCESS(vos_timer_destroy(&pMac->pmc.hTrafficTimer)))
    {
        pmcLog(pMac, LOGE, FL("Cannot deallocate traffic timer"));
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    if (vos_timer_destroy(&pMac->pmc.hDiagEvtTimer) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot deallocate timer for diag event reporting"));
    }
#endif
    if (vos_timer_destroy(&pMac->pmc.hExitPowerSaveTimer) != VOS_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Cannot deallocate exit power save mode timer"));
    }

    /*
        The following list's entries are dynamically allocated so they need their own 
        cleanup function
    */
    pmcClosePowerSaveCheckList(pMac);
    pmcCloseRequestFullPowerList(pMac);
    pmcCloseRequestBmpsList(pMac);
    pmcCloseRequestStartUapsdList(pMac);
    pmcCloseDeviceStateUpdateList(pMac);
    pmcCloseDeferredMsgList(pMac);

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcSignalPowerEvent
*
* Description:
*    Signals to PMC that a power event has occurred.
*
* Parameters:
*    hHal - HAL handle for device
*    event - the event that has occurred
*
* Returns:
*    eHAL_STATUS_SUCCESS - signaling successful
*    eHAL_STATUS_FAILURE - signaling not successful
*
******************************************************************************/
eHalStatus pmcSignalPowerEvent (tHalHandle hHal, tPmcPowerEvent event)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
#ifndef GEN6_ONWARDS
    tSirMacHTMIMOPowerSaveState  htMimoPowerSaveState;
#endif

    pmcLog(pMac, LOG2, FL("Entering pmcSignalPowerEvent, event %d"), event);

    /* Take action based on the event being signaled. */
    switch (event)
    {
#ifndef GEN6_ONWARDS
    case ePMC_SYSTEM_HIBERNATE:
        return pmcEnterLowPowerState(hHal);

    case ePMC_SYSTEM_RESUME:
        return pmcExitLowPowerState(hHal);

    case ePMC_HW_WLAN_SWITCH_OFF:
        pMac->pmc.hwWlanSwitchState = ePMC_SWITCH_OFF;
        return pmcEnterLowPowerState(hHal);

    case ePMC_HW_WLAN_SWITCH_ON:
        pMac->pmc.hwWlanSwitchState = ePMC_SWITCH_ON;
        return pmcExitLowPowerState(hHal);

    case ePMC_SW_WLAN_SWITCH_OFF:
        pMac->pmc.swWlanSwitchState = ePMC_SWITCH_OFF;
        return pmcEnterLowPowerState(hHal);

    case ePMC_SW_WLAN_SWITCH_ON:
        pMac->pmc.swWlanSwitchState = ePMC_SWITCH_ON;
        return pmcExitLowPowerState(hHal);

    case ePMC_BATTERY_OPERATION:
        pMac->pmc.powerSource = BATTERY_POWER;

        /* Turn on SMPS. */
        if (pMac->pmc.smpsEnabled)
        {
            if (pMac->pmc.smpsConfig.mode == ePMC_DYNAMIC_SMPS)
                htMimoPowerSaveState = eSIR_HT_MIMO_PS_DYNAMIC;
            if (pMac->pmc.smpsConfig.mode == ePMC_STATIC_SMPS)
                htMimoPowerSaveState = eSIR_HT_MIMO_PS_STATIC;
            if (pmcSendMessage(hHal, eWNI_PMC_SMPS_STATE_IND, &htMimoPowerSaveState,
                               sizeof(tSirMacHTMIMOPowerSaveState)) != eHAL_STATUS_SUCCESS)   
                return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;

    case ePMC_AC_OPERATION:
        pMac->pmc.powerSource = AC_POWER;

        /* Turn off SMPS. */
        if (!pMac->pmc.smpsConfig.enterOnAc)
        {
            htMimoPowerSaveState = eSIR_HT_MIMO_PS_NO_LIMIT;
            if (pmcSendMessage(hHal, eWNI_PMC_SMPS_STATE_IND, &htMimoPowerSaveState,
                               sizeof(tSirMacHTMIMOPowerSaveState)) != eHAL_STATUS_SUCCESS)
                return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;
#endif //GEN6_ONWARDS
    default:
        pmcLog(pMac, LOGE, FL("Invalid event %d"), event);
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }
}


/******************************************************************************
*
* Name:  pmcSetConfigPowerSave
*
* Description:
*    Configures one of the power saving modes.
*
* Parameters:
*    hHal - HAL handle for device
*    psMode - the power saving mode to configure
*    pConfigParams - pointer to configuration parameters specific to the
*                    power saving mode
*
* Returns:
*    eHAL_STATUS_SUCCESS - configuration successful
*    eHAL_STATUS_FAILURE - configuration not successful
*
******************************************************************************/
eHalStatus pmcSetConfigPowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode, void *pConfigParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT
    WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);
#endif

    pmcLog(pMac, LOG2, FL("Entering pmcSetConfigPowerSave, power save mode %d"), psMode);

    /* Configure the specified power saving mode. */
    switch (psMode)
    {
    
    case ePMC_IDLE_MODE_POWER_SAVE:
        pMac->pmc.impsConfig = *(tpPmcImpsConfigParams)pConfigParams;
        pmcLog(pMac, LOG3, FL("IMPS configuration"));
        pmcLog(pMac, LOG3, "          enter on AC: %d",
               pMac->pmc.impsConfig.enterOnAc);
        break;

    case ePMC_BEACON_MODE_POWER_SAVE:
        pMac->pmc.bmpsConfig = *(tpPmcBmpsConfigParams)pConfigParams;
        pmcLog(pMac, LOG3, FL("BMPS configuration"));
        pmcLog(pMac, LOG3, "          enter on AC: %d",
               pMac->pmc.bmpsConfig.enterOnAc);
        pmcLog(pMac, LOG3, "          TX threshold: %d",
               pMac->pmc.bmpsConfig.txThreshold);
        pmcLog(pMac, LOG3, "          RX threshold: %d",
               pMac->pmc.bmpsConfig.rxThreshold);
        pmcLog(pMac, LOG3, "          traffic measurement period (ms): %d",
               pMac->pmc.bmpsConfig.trafficMeasurePeriod);
        pmcLog(pMac, LOG3, "          BMPS period: %d",
               pMac->pmc.bmpsConfig.bmpsPeriod);
        pmcLog(pMac, LOG3, "          beacons to forward code: %d",
               pMac->pmc.bmpsConfig.forwardBeacons);
        pmcLog(pMac, LOG3, "          value of N: %d",
               pMac->pmc.bmpsConfig.valueOfN);
        pmcLog(pMac, LOG3, "          use PS poll: %d",
               pMac->pmc.bmpsConfig.usePsPoll);
        pmcLog(pMac, LOG3, "          set PM on last frame: %d",
               pMac->pmc.bmpsConfig.setPmOnLastFrame);
        pmcLog(pMac, LOG3, "          value of enableBeaconEarlyTermination: %d",
               pMac->pmc.bmpsConfig.enableBeaconEarlyTermination);
        pmcLog(pMac, LOG3, "          value of bcnEarlyTermWakeInterval: %d",
               pMac->pmc.bmpsConfig.bcnEarlyTermWakeInterval);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
        vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
        psRequest.event_subtype = WLAN_BMPS_SET_CONFIG;
        /* possible loss of data due to mismatch but expectation is that
        values can reasonably be expected to fit in target widths */
        psRequest.bmps_auto_timer_duration = (v_U16_t)pMac->pmc.bmpsConfig.trafficMeasurePeriod;
        psRequest.bmps_period = (v_U16_t)pMac->pmc.bmpsConfig.bmpsPeriod; 

        WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif


        break;

    case ePMC_SPATIAL_MULTIPLEX_POWER_SAVE:
        pMac->pmc.smpsConfig = *(tpPmcSmpsConfigParams)pConfigParams;
        pmcLog(pMac, LOG3, FL("SMPS configuration"));
        pmcLog(pMac, LOG3, "          mode: %d", pMac->pmc.smpsConfig.mode);
        pmcLog(pMac, LOG3, "          enter on AC: %d",
               pMac->pmc.smpsConfig.enterOnAc);
        break;

    default:
        pmcLog(pMac, LOGE, FL("Invalid power save mode %d"), psMode);
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    //Send the power save config down to PE/HAL/FW if BMPS mode is being configured
    //and pmcReady has been invoked
    if(PMC_IS_READY(pMac) && psMode == ePMC_BEACON_MODE_POWER_SAVE)
    {
       if (pmcSendPowerSaveConfigMessage(hHal) != eHAL_STATUS_SUCCESS)
           return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcGetConfigPowerSave
*
* Description:
*    Get the config for the specified power save mode
*
* Parameters:
*    hHal - HAL handle for device
*    psMode - the power saving mode to configure
*    pConfigParams - pointer to configuration parameters specific to the
*                    power saving mode
*
* Returns:
*    eHAL_STATUS_SUCCESS - configuration successful
*    eHAL_STATUS_FAILURE - configuration not successful
*
******************************************************************************/
eHalStatus pmcGetConfigPowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode, void *pConfigParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcGetConfigPowerSave, power save mode %d"), psMode);

    /* Configure the specified power saving mode. */
    switch (psMode)
    {
    
    case ePMC_IDLE_MODE_POWER_SAVE:
        *(tpPmcImpsConfigParams)pConfigParams = pMac->pmc.impsConfig;
        break;

    case ePMC_BEACON_MODE_POWER_SAVE:
        *(tpPmcBmpsConfigParams)pConfigParams = pMac->pmc.bmpsConfig;
        break;

    case ePMC_SPATIAL_MULTIPLEX_POWER_SAVE:
        *(tpPmcSmpsConfigParams)pConfigParams = pMac->pmc.smpsConfig;
        break;

    default:
        pmcLog(pMac, LOGE, FL("Invalid power save mode %d"), psMode);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}
/******************************************************************************
*
* Name:  pmcEnablePowerSave
*
* Description:
*    Enables one of the power saving modes.
*
* Parameters:
*    hHal - HAL handle for device
*    psMode - the power saving mode to enable
*
* Returns:
*    eHAL_STATUS_SUCCESS - successfully enabled
*    eHAL_STATUS_FAILURE - not successfully enabled
*
******************************************************************************/
eHalStatus pmcEnablePowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSirMacHTMIMOPowerSaveState  htMimoPowerSaveState;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

    vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
    psRequest.event_subtype = WLAN_PS_MODE_ENABLE_REQ;
    psRequest.enable_disable_powersave_mode = psMode;

    WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif
    
    pmcLog(pMac, LOG2, FL("Entering pmcEnablePowerSave, power save mode %d"), psMode);

    /* Enable the specified power saving mode. */
    switch (psMode)
    {

    case ePMC_IDLE_MODE_POWER_SAVE:
        pMac->pmc.impsEnabled = TRUE;
        break;

    case ePMC_BEACON_MODE_POWER_SAVE:
        pMac->pmc.bmpsEnabled = TRUE;
        break;

    case ePMC_SPATIAL_MULTIPLEX_POWER_SAVE:
        pMac->pmc.smpsEnabled = TRUE;

        /* If PMC already started, then turn on SMPS. */
        if (pMac->pmc.pmcState != STOPPED)
            if (pMac->pmc.powerSource != AC_POWER ||
                pMac->pmc.smpsConfig.enterOnAc)
            {
                if (pMac->pmc.smpsConfig.mode == ePMC_DYNAMIC_SMPS)
                    htMimoPowerSaveState = eSIR_HT_MIMO_PS_DYNAMIC;
                if (pMac->pmc.smpsConfig.mode == ePMC_STATIC_SMPS)
                    htMimoPowerSaveState = eSIR_HT_MIMO_PS_STATIC;
                if (pmcSendMessage(hHal, eWNI_PMC_SMPS_STATE_IND, &htMimoPowerSaveState,
                                   sizeof(tSirMacHTMIMOPowerSaveState)) != eHAL_STATUS_SUCCESS)
                    return eHAL_STATUS_FAILURE;
            }
        break;

    case ePMC_UAPSD_MODE_POWER_SAVE:
        pMac->pmc.uapsdEnabled = TRUE;
        break;

    case ePMC_STANDBY_MODE_POWER_SAVE:
        pMac->pmc.standbyEnabled = TRUE;
        break;

    case ePMC_WOWL_MODE_POWER_SAVE:
        pMac->pmc.wowlEnabled = TRUE;
        break;

    default:
        pmcLog(pMac, LOGE, FL("Invalid power save mode %d"), psMode);
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}
/* ---------------------------------------------------------------------------
    \fn pmcStartAutoBmpsTimer
    \brief  Starts a timer that periodically polls all the registered
            module for entry into Bmps mode. This timer is started only if BMPS is
            enabled and whenever the device is in full power.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus pmcStartAutoBmpsTimer (tHalHandle hHal) 
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

   vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
   psRequest.event_subtype = WLAN_START_BMPS_AUTO_TIMER_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

   pmcLog(pMac, LOG2, FL("Entering pmcStartAutoBmpsTimer"));

   /* Check if BMPS is enabled. */
   if (!pMac->pmc.bmpsEnabled)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enable BMPS timer. BMPS is disabled");
      return eHAL_STATUS_FAILURE;
   }

   pMac->pmc.autoBmpsEntryEnabled = TRUE;

   /* Check if there is an Infra session. If there is no Infra session, timer will be started 
         when STA associates to AP */

   if (pmcShouldBmpsTimerRun(pMac))
   {
      if (pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod) != eHAL_STATUS_SUCCESS)
         return eHAL_STATUS_FAILURE;
   }



   return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcStopAutoBmpsTimer
    \brief  Stops the Auto BMPS Timer that was started using sme_startAutoBmpsTimer
            Stopping the timer does not cause a device state change. Only the timer
            is stopped. If "Full Power" is desired, use the pmcRequestFullPower API
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus pmcStopAutoBmpsTimer (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

   vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
   psRequest.event_subtype = WLAN_STOP_BMPS_AUTO_TIMER_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

   pmcLog(pMac, LOG2, FL("Entering pmcStopAutoBmpsTimer"));

   pMac->pmc.autoBmpsEntryEnabled = FALSE;
   /* If uapsd session is not required or HDD has not requested BMPS, stop the auto bmps timer.*/
   if (!pMac->pmc.uapsdSessionRequired && !pMac->pmc.bmpsRequestedByHdd)
      pmcStopTrafficTimer(hHal);

   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcDisablePowerSave
*
* Description:
*    Disables one of the power saving modes.
*
* Parameters:
*    hHal - HAL handle for device
*    psMode - the power saving mode to disable
*
* Returns:
*    eHAL_STATUS_SUCCESS - successfully disabled
*    eHAL_STATUS_FAILURE - not successfully disabled
*
******************************************************************************/
eHalStatus pmcDisablePowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSirMacHTMIMOPowerSaveState  htMimoPowerSaveState;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

    vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
    psRequest.event_subtype = WLAN_PS_MODE_DISABLE_REQ;
    psRequest.enable_disable_powersave_mode = psMode;

    WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

    pmcLog(pMac, LOG2, FL("Entering pmcDisablePowerSave, power save mode %d"), psMode);

    /* Disable the specified power saving mode. */
    switch (psMode)
    {

    case ePMC_IDLE_MODE_POWER_SAVE:
        pMac->pmc.impsEnabled = FALSE;
        break;

    case ePMC_BEACON_MODE_POWER_SAVE:
        pMac->pmc.bmpsEnabled = FALSE;
        break;

    case ePMC_SPATIAL_MULTIPLEX_POWER_SAVE:
        pMac->pmc.smpsEnabled = FALSE;

        /* Turn off SMPS. */
        htMimoPowerSaveState = eSIR_HT_MIMO_PS_NO_LIMIT;
        if (pmcSendMessage(hHal, eWNI_PMC_SMPS_STATE_IND, &htMimoPowerSaveState,
                           sizeof(tSirMacHTMIMOPowerSaveState)) != eHAL_STATUS_SUCCESS)
            return eHAL_STATUS_FAILURE;
        break;

    case ePMC_UAPSD_MODE_POWER_SAVE:
        pMac->pmc.uapsdEnabled = FALSE;
        break;

    case ePMC_STANDBY_MODE_POWER_SAVE:
        pMac->pmc.standbyEnabled = FALSE;
        break;

    case ePMC_WOWL_MODE_POWER_SAVE:
        pMac->pmc.wowlEnabled = FALSE;
        break;

    default:
        pmcLog(pMac, LOGE, FL("Invalid power save mode %d"), psMode);
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcQueryPowerState
*
* Description:
*    Returns the current power state of the device.
*
* Parameters:
*    hHal - HAL handle for device
*    pPowerState - pointer to location to return power state
*    pHwWlanSwitchState - pointer to location to return Hardware WLAN
*                         Switch state
*    pSwWlanSwitchState - pointer to location to return Software WLAN
*                         Switch state
*
* Returns:
*    eHAL_STATUS_SUCCESS - power state successfully returned
*    eHAL_STATUS_FAILURE - power state not successfully returned
*
******************************************************************************/
eHalStatus pmcQueryPowerState (tHalHandle hHal, tPmcPowerState *pPowerState,
                               tPmcSwitchState *pHwWlanSwitchState, tPmcSwitchState *pSwWlanSwitchState)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcQueryPowerState"));

    /* Return current power state based on PMC state. */
    if(pPowerState != NULL)
    {
        /* Return current power state based on PMC state. */
        switch (pMac->pmc.pmcState)
        {
    
        case FULL_POWER:
            *pPowerState = ePMC_FULL_POWER;
            break;

        default:
            *pPowerState = ePMC_LOW_POWER;
            break;
        }
    }

    /* Return current switch settings. */
    if(pHwWlanSwitchState != NULL)
       *pHwWlanSwitchState = pMac->pmc.hwWlanSwitchState;
    if(pSwWlanSwitchState != NULL)
       *pSwWlanSwitchState = pMac->pmc.swWlanSwitchState;

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcIsPowerSaveEnabled
*
* Description:
*    Checks if the device is able to enter one of the power save modes.
*    "Able to enter" means the power save mode is enabled for the device
*    and the host is using the correct power source for entry into the
*    power save mode.  This routine does not indicate whether the device
*    is actually in the power save mode at a particular point in time.
*
* Parameters:
*    hHal - HAL handle for device
*    psMode - the power saving mode
*
* Returns:
*    TRUE if device is able to enter the power save mode, FALSE otherwise
*
******************************************************************************/
tANI_BOOLEAN pmcIsPowerSaveEnabled (tHalHandle hHal, tPmcPowerSavingMode psMode)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcIsPowerSaveEnabled, power save mode %d"), psMode);

    /* Check ability to enter based on the specified power saving mode. */
    switch (psMode)
    {
    
    case ePMC_IDLE_MODE_POWER_SAVE:
        return pMac->pmc.impsEnabled && (pMac->pmc.powerSource != AC_POWER || pMac->pmc.impsConfig.enterOnAc);

    case ePMC_BEACON_MODE_POWER_SAVE:
        return pMac->pmc.bmpsEnabled;

    case ePMC_SPATIAL_MULTIPLEX_POWER_SAVE:
        return pMac->pmc.smpsEnabled && (pMac->pmc.powerSource != AC_POWER || pMac->pmc.smpsConfig.enterOnAc);

    case ePMC_UAPSD_MODE_POWER_SAVE:
        return pMac->pmc.uapsdEnabled;

    case ePMC_STANDBY_MODE_POWER_SAVE:
        return pMac->pmc.standbyEnabled;

    case ePMC_WOWL_MODE_POWER_SAVE:
        return pMac->pmc.wowlEnabled;
        break;

    default:
        pmcLog(pMac, LOGE, FL("Invalid power save mode %d"), psMode);
        PMC_ABORT;
        return FALSE;
    }
}


/******************************************************************************
*
* Name:  pmcRequestFullPower
*
* Description:
*    Request that the device be brought to full power state.
*
* Parameters:
*    hHal - HAL handle for device
*    callbackRoutine - routine to call when device actually achieves full
*                      power state if "eHAL_STATUS_PMC_PENDING" is returned
*    callbackContext - value to be passed as parameter to routine specified
*                      above
*    fullPowerReason -  Reason for requesting full power mode. This is used
*                       by PE to decide whether data null should be sent to
*                       AP when exiting BMPS mode. Caller should use the
*                       eSME_LINK_DISCONNECTED reason if link is disconnected
*                       and there is no need to tell the AP that we are going
*                       out of power save.
*
* Returns:
*    eHAL_STATUS_SUCCESS - device brought to full power state
*    eHAL_STATUS_FAILURE - device cannot be brought to full power state
*    eHAL_STATUS_PMC_PENDING - device is being brought to full power state,
*                              callbackRoutine will be called when completed
*
******************************************************************************/
eHalStatus pmcRequestFullPower (tHalHandle hHal, void (*callbackRoutine) (void *callbackContext, eHalStatus status),
                                void *callbackContext, tRequestFullPowerReason fullPowerReason)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tpRequestFullPowerEntry pEntry;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

    vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
    psRequest.event_subtype = WLAN_ENTER_FULL_POWER_REQ;
    psRequest.full_power_request_reason = fullPowerReason;
 
    WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

    pmcLog(pMac, LOG2, FL("Entering pmcRequestFullPower"));

    if( !PMC_IS_READY(pMac) )
    {
        pmcLog(pMac, LOGE, FL("Requesting Full Power when PMC not ready"));
        pmcLog(pMac, LOGE, FL("pmcReady = %d pmcState = %s"),
            pMac->pmc.pmcReady, pmcGetPmcStateStr(pMac->pmc.pmcState));
        return eHAL_STATUS_FAILURE;
    }

    /* If HDD is requesting full power, clear any buffered requests for WOWL and BMPS that were
       requested by HDD previously */
    if(SIR_IS_FULL_POWER_NEEDED_BY_HDD(fullPowerReason))
    {
        pMac->pmc.bmpsRequestedByHdd = FALSE;
        pMac->pmc.wowlModeRequired = FALSE;
    }

    /* If already in full power, just return. */
    if (pMac->pmc.pmcState == FULL_POWER)
        return eHAL_STATUS_SUCCESS;

    /* If in IMPS State, then cancel the timer. */
    if (pMac->pmc.pmcState == IMPS)
        if (vos_timer_stop(&pMac->pmc.hImpsTimer) != VOS_STATUS_SUCCESS)
        {
            pmcLog(pMac, LOGE, FL("Cannot cancel IMPS timer"));
        }
    /* Enter Request Full Power State. */
    if (pmcEnterRequestFullPowerState(hHal, fullPowerReason) != eHAL_STATUS_SUCCESS)
        return eHAL_STATUS_FAILURE;

    /* If able to enter Request Full Power State, then request is pending.
       Allocate entry for request full power callback routine list. */
    //If caller doesn't need a callback, simply waits up the chip.
    if( callbackRoutine )
    {
        pEntry = vos_mem_malloc(sizeof(tRequestFullPowerEntry));
        if ( NULL == pEntry )
        {
            pmcLog(pMac, LOGE,
                   FL("Cannot allocate memory for request full power routine list entry"));
            PMC_ABORT;
            return eHAL_STATUS_FAILURE;
        }

        /* Store routine and context in entry. */
        pEntry->callbackRoutine = callbackRoutine;
        pEntry->callbackContext = callbackContext;

        /* Add entry to list. */
        csrLLInsertTail(&pMac->pmc.requestFullPowerList, &pEntry->link, TRUE);
    }

    return eHAL_STATUS_PMC_PENDING;
}


/******************************************************************************
*
* Name:  pmcRequestImps
*
* Description:
*    Request that the device be placed in Idle Mode Power Save (IMPS).
*    The Common Scan/Roam Module makes this request.  The device will be
*    placed into IMPS for the specified amount of time, and then returned
*    to full power.
*
* Parameters:
*    hHal - HAL handle for device
*    impsPeriod - amount of time to remain in IMPS (milliseconds)
*    callbackRoutine - routine to call when IMPS period has finished and
*                      the device has been brought to full power
*    callbackContext - value to be passed as parameter to routine specified
*                      above
*
* Returns:
*    eHAL_STATUS_SUCCESS - device will enter IMPS
*    eHAL_STATUS_PMC_DISABLED - IMPS is disabled
*    eHAL_STATUS_PMC_NOT_NOW - another module is prohibiting entering IMPS
*                              at this time
*    eHAL_STATUS_PMC_AC_POWER - IMPS is disabled when host operating from
*                               AC power
*    eHAL_STATUS_PMC_ALREADY_IN_IMPS - device is already in IMPS
*    eHAL_STATUS_PMC_SYS_ERROR - system error that prohibits entering IMPS
*
******************************************************************************/
eHalStatus pmcRequestImps (tHalHandle hHal, tANI_U32 impsPeriod,
                           void (*callbackRoutine) (void *callbackContext, eHalStatus status),
                           void *callbackContext)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    eHalStatus status;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

    vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
    psRequest.event_subtype = WLAN_IMPS_ENTER_REQ;
    psRequest.imps_period = impsPeriod;

    WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif


    pmcLog(pMac, LOG2, FL("Entering pmcRequestImps"));

    status = pmcEnterImpsCheck( pMac );
    if( HAL_STATUS_SUCCESS( status ) )
    {
        /* Enter Request IMPS State. */
        status = pmcEnterRequestImpsState( hHal );
        if (HAL_STATUS_SUCCESS( status ))
    {
            /* Save the period and callback routine for when we need it. */
            pMac->pmc.impsPeriod = impsPeriod;
            pMac->pmc.impsCallbackRoutine = callbackRoutine;
            pMac->pmc.impsCallbackContext = callbackContext;

    }
        else
    {
            status = eHAL_STATUS_PMC_SYS_ERROR;
    }
    }

    return status;
}


/******************************************************************************
*
* Name:  pmcRegisterPowerSaveCheck
*
* Description:
*    Allows a routine to be registered so that the routine is called whenever
*    the device is about to enter one of the power save modes.  This routine
*    will say whether the device is allowed to enter the power save mode at
*    the time of the call.
*
* Parameters:
*    hHal - HAL handle for device
*    checkRoutine - routine to call before entering a power save mode, should
*                   return TRUE if the device is allowed to enter the power
*                   save mode, FALSE otherwise
*    checkContext - value to be passed as parameter to routine specified above
*
* Returns:
*    eHAL_STATUS_SUCCESS - successfully registered
*    eHAL_STATUS_FAILURE - not successfully registered
*
******************************************************************************/
eHalStatus pmcRegisterPowerSaveCheck (tHalHandle hHal, tANI_BOOLEAN (*checkRoutine) (void *checkContext),
                                      void *checkContext)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tpPowerSaveCheckEntry pEntry;

    pmcLog(pMac, LOG2, FL("Entering pmcRegisterPowerSaveCheck"));

    /* Allocate entry for power save check routine list. */
    pEntry = vos_mem_malloc(sizeof(tPowerSaveCheckEntry));
    if ( NULL == pEntry )
    {
        pmcLog(pMac, LOGE, FL("Cannot allocate memory for power save check routine list entry"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    /* Store routine and context in entry. */
    pEntry->checkRoutine = checkRoutine;
    pEntry->checkContext = checkContext;

    /* Add entry to list. */
    csrLLInsertTail(&pMac->pmc.powerSaveCheckList, &pEntry->link, FALSE);

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcDeregisterPowerSaveCheck
*
* Description:
*    Reregisters a routine that was previously registered with
*    pmcRegisterPowerSaveCheck.
*
* Parameters:
*    hHal - HAL handle for device
*    checkRoutine - routine to deregister
*
* Returns:
*    eHAL_STATUS_SUCCESS - successfully deregistered
*    eHAL_STATUS_FAILURE - not successfully deregistered
*
******************************************************************************/
eHalStatus pmcDeregisterPowerSaveCheck (tHalHandle hHal, tANI_BOOLEAN (*checkRoutine) (void *checkContext))
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tListElem *pEntry;
    tpPowerSaveCheckEntry pPowerSaveCheckEntry;

    pmcLog(pMac, LOG2, FL("Entering pmcDeregisterPowerSaveCheck"));

    /* Find entry in the power save check routine list that matches
       the specified routine and remove it. */
    pEntry = csrLLPeekHead(&pMac->pmc.powerSaveCheckList, FALSE);
    while (pEntry != NULL)
    {
        pPowerSaveCheckEntry = GET_BASE_ADDR(pEntry, tPowerSaveCheckEntry, link);
        if (pPowerSaveCheckEntry->checkRoutine == checkRoutine)
        {
            if (csrLLRemoveEntry(&pMac->pmc.powerSaveCheckList, pEntry, FALSE))
            {
                vos_mem_free(pPowerSaveCheckEntry);
            }
            else
            {
                pmcLog(pMac, LOGE, FL("Cannot remove power save check routine list entry"));
                return eHAL_STATUS_FAILURE;
            }
            return eHAL_STATUS_SUCCESS;
        }
        pEntry = csrLLNext(&pMac->pmc.powerSaveCheckList, pEntry, FALSE);
    }

    /* Could not find matching entry. */
    return eHAL_STATUS_FAILURE;
}


static void pmcProcessResponse( tpAniSirGlobal pMac, tSirSmeRsp *pMsg )
{
    tListElem *pEntry = NULL;
    tSmeCmd *pCommand = NULL;
    tANI_BOOLEAN fRemoveCommand = eANI_BOOLEAN_TRUE;

    pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    if(pEntry)
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);

        pmcLog(pMac, LOG2, FL("process message = %d"), pMsg->messageType);

    /* Process each different type of message. */
    switch (pMsg->messageType)
    {

    /* We got a response to our IMPS request.  */
    case eWNI_PMC_ENTER_IMPS_RSP:
        pmcLog(pMac, LOG2, FL("Rcvd eWNI_PMC_ENTER_IMPS_RSP with status = %d"), pMsg->statusCode);
            if( (eSmeCommandEnterImps != pCommand->command) && (eSmeCommandEnterStandby != pCommand->command) )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_ENTER_IMPS_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
        if(pMac->pmc.pmcState == REQUEST_IMPS)
        {
            /* Enter IMPS State if response indicates success. */
            if (pMsg->statusCode == eSIR_SME_SUCCESS)
            {
                pMac->pmc.ImpsReqFailed = VOS_FALSE;
                pmcEnterImpsState(pMac);
                if (!(pMac->pmc.ImpsReqFailed || pMac->pmc.ImpsReqTimerFailed) && pMac->pmc.ImpsReqFailCnt)
                {
                    pmcLog(pMac, LOGE,
                           FL("Response message to request to enter IMPS was failed %d times before success"),
                       pMac->pmc.ImpsReqFailCnt);
                       pMac->pmc.ImpsReqFailCnt = 0;
                }
            }

            /* If response is failure, then we stay in Full Power State and tell everyone that we aren't going into IMPS. */
            else
            {
                pMac->pmc.ImpsReqFailed = VOS_TRUE;
                if (!(pMac->pmc.ImpsReqFailCnt & 0xF))
                {
                    pmcLog(pMac, LOGE,
                           FL("Response message to request to enter IMPS indicates failure, status %x, FailCnt - %d"),
                       pMsg->statusCode, ++pMac->pmc.ImpsReqFailCnt);
                }
                else
                {
                    pMac->pmc.ImpsReqFailCnt++;
                }
                pmcEnterFullPowerState(pMac);
            }
        }
        else if (pMac->pmc.pmcState == REQUEST_STANDBY)
        {
            /* Enter STANDBY State if response indicates success. */
            if (pMsg->statusCode == eSIR_SME_SUCCESS)
            {
                pmcEnterStandbyState(pMac);
                pmcDoStandbyCallbacks(pMac, eHAL_STATUS_SUCCESS);
            }

            /* If response is failure, then we stay in Full Power State
               and tell everyone that we aren't going into STANDBY. */
            else
            {
                pmcLog(pMac, LOGE, "PMC: response message to request to enter "
                       "standby indicates failure, status %d", pMsg->statusCode);
                pmcEnterFullPowerState(pMac);
                pmcDoStandbyCallbacks(pMac, eHAL_STATUS_FAILURE);
            }
        }
        else
        {
            pmcLog(pMac, LOGE, "PMC: Enter IMPS rsp rcvd when device is "
               "in %d state", pMac->pmc.pmcState);
        }
        break;

    /* We got a response to our wake from IMPS request. */
    case eWNI_PMC_EXIT_IMPS_RSP:
            pmcLog(pMac, LOG2, FL("Rcvd eWNI_PMC_EXIT_IMPS_RSP with status = %d"), pMsg->statusCode);
            if( eSmeCommandExitImps != pCommand->command )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_EXIT_IMPS_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_FULL_POWER)
            {
                pmcLog(pMac, LOGE, FL("Got Exit IMPS Response Message while "
                   "in state %d"), pMac->pmc.pmcState);
                break;
            }

            /* Enter Full Power State. */
            if (pMsg->statusCode != eSIR_SME_SUCCESS)
            {
                pmcLog(pMac, LOGE, FL("Response message to request to exit "
                   "IMPS indicates failure, status %d"), pMsg->statusCode);
                if (vos_is_logp_in_progress(VOS_MODULE_ID_SME, NULL))
                {
                    pmcLog(pMac, LOGE, FL("SSR Is in progress do not send "
                                          "exit imps req again"));
                }
                else if( eHAL_STATUS_SUCCESS ==
                      pmcSendMessage(pMac, eWNI_PMC_EXIT_IMPS_REQ, NULL, 0) )
                {
                    fRemoveCommand = eANI_BOOLEAN_FALSE;
                    pMac->pmc.pmcState = REQUEST_FULL_POWER;
                    pmcLog(pMac, LOGE, FL("eWNI_PMC_EXIT_IMPS_REQ sent again"
                                          " to PE"));
                    break;
                }
            }
            pmcEnterFullPowerState(pMac);
        break;

    /* We got a response to our BMPS request.  */
    case eWNI_PMC_ENTER_BMPS_RSP:
            pmcLog(pMac, LOG2, FL("Rcvd eWNI_PMC_ENTER_BMPS_RSP with status = %d"), pMsg->statusCode);
            if( eSmeCommandEnterBmps != pCommand->command )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_ENTER_BMPS_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            pMac->pmc.bmpsRequestQueued = eANI_BOOLEAN_FALSE;
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_BMPS)
            {
                pmcLog(pMac, LOGE,
                       FL("Got Enter BMPS Response Message while in state %d"), pMac->pmc.pmcState);
                break;
            }

        /* Enter BMPS State if response indicates success. */
        if (pMsg->statusCode == eSIR_SME_SUCCESS)
        {
                pmcEnterBmpsState(pMac);
            /* Note: If BMPS was requested because of start UAPSD,
               there will no entries for BMPS callback routines and
               pmcDoBmpsCallbacks will be a No-Op*/
                pmcDoBmpsCallbacks(pMac, eHAL_STATUS_SUCCESS);
         }
        /* If response is failure, then we stay in Full Power State and tell everyone that we aren't going into BMPS. */
        else
        {
                pmcLog(pMac, LOGE,
                       FL("Response message to request to enter BMPS indicates failure, status %d"),
                   pMsg->statusCode);
                pmcEnterFullPowerState(pMac);
                //Do not call UAPSD callback here since it may be re-entered
                pmcDoBmpsCallbacks(pMac, eHAL_STATUS_FAILURE);
        }
        break;

    /* We got a response to our wake from BMPS request. */
    case eWNI_PMC_EXIT_BMPS_RSP:
            pmcLog(pMac, LOG2, FL("Rcvd eWNI_PMC_EXIT_BMPS_RSP with status = %d"), pMsg->statusCode);
            if( eSmeCommandExitBmps != pCommand->command )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_EXIT_BMPS_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_FULL_POWER)
            {
                pmcLog(pMac, LOGE,
                       FL("Got Exit BMPS Response Message while in state %d"), pMac->pmc.pmcState);
                break;
            }

            /* Enter Full Power State. */
            if (pMsg->statusCode != eSIR_SME_SUCCESS)
            {
                pmcLog(pMac, LOGP,
                       FL("Response message to request to exit BMPS indicates failure, status %d"),
                       pMsg->statusCode);
            }
            pmcEnterFullPowerState(pMac);
        break;

        /* We got a response to our Start UAPSD request.  */
        case eWNI_PMC_ENTER_UAPSD_RSP:
            pmcLog(pMac, LOG2, FL("Rcvd eWNI_PMC_ENTER_UAPSD_RSP with status = %d"), pMsg->statusCode);
            if( eSmeCommandEnterUapsd != pCommand->command )
        {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_ENTER_UAPSD_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_START_UAPSD)
            {
                pmcLog(pMac, LOGE,
                       FL("Got Enter Uapsd rsp Message while in state %d"), pMac->pmc.pmcState);
                break;
            }

         /* Enter UAPSD State if response indicates success. */
            if (pMsg->statusCode == eSIR_SME_SUCCESS) 
            {
                pmcEnterUapsdState(pMac);
                pmcDoStartUapsdCallbacks(pMac, eHAL_STATUS_SUCCESS);
         }
         /* If response is failure, then we try to put the chip back in
            BMPS mode*/
            else {
                pmcLog(pMac, LOGE, "PMC: response message to request to enter "
                   "UAPSD indicates failure, status %d", pMsg->statusCode);
                //Need to reset the UAPSD flag so pmcEnterBmpsState won't try to enter UAPSD.
                pMac->pmc.uapsdSessionRequired = FALSE;
                pmcEnterBmpsState(pMac);
                //UAPSD will not be retied in this case so tell requester we are done with failure
                pmcDoStartUapsdCallbacks(pMac, eHAL_STATUS_FAILURE);
         }
         break;

      /* We got a response to our Stop UAPSD request.  */
      case eWNI_PMC_EXIT_UAPSD_RSP:
         pmcLog(pMac, LOG2, FL("Rcvd eWNI_PMC_EXIT_UAPSD_RSP with status = %d"), pMsg->statusCode);
            if( eSmeCommandExitUapsd != pCommand->command )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_EXIT_UAPSD_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_STOP_UAPSD)
            {
                pmcLog(pMac, LOGE,
                       FL("Got Exit Uapsd rsp Message while in state %d"), pMac->pmc.pmcState);
                break;
            }

         /* Enter BMPS State */
         if (pMsg->statusCode != eSIR_SME_SUCCESS) {
            pmcLog(pMac, LOGP, "PMC: response message to request to exit "
               "UAPSD indicates failure, status %d", pMsg->statusCode);
         }
            pmcEnterBmpsState(pMac);
         break;

      /* We got a response to our enter WOWL request.  */
      case eWNI_PMC_ENTER_WOWL_RSP:

            if( eSmeCommandEnterWowl != pCommand->command )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_ENTER_WOWL_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_ENTER_WOWL)
            {
                pmcLog(pMac, LOGE, FL("Got eWNI_PMC_ENTER_WOWL_RSP while in state %s"),
                    pmcGetPmcStateStr(pMac->pmc.pmcState));
                break;
            }

         /* Enter WOWL State if response indicates success. */
         if (pMsg->statusCode == eSIR_SME_SUCCESS) {
                pmcEnterWowlState(pMac);
                pmcDoEnterWowlCallbacks(pMac, eHAL_STATUS_SUCCESS);
         }

         /* If response is failure, then we try to put the chip back in
            BMPS mode*/
         else {
            pmcLog(pMac, LOGE, "PMC: response message to request to enter "
               "WOWL indicates failure, status %d", pMsg->statusCode);
                pmcEnterBmpsState(pMac);
                pmcDoEnterWowlCallbacks(pMac, eHAL_STATUS_FAILURE);
         }
         break;

      /* We got a response to our exit WOWL request.  */
      case eWNI_PMC_EXIT_WOWL_RSP:

            if( eSmeCommandExitWowl != pCommand->command )
            {
                pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_EXIT_WOWL_RSP without request"));
                fRemoveCommand = eANI_BOOLEAN_FALSE;
                break;
            }
            /* Check that we are in the correct state for this message. */
            if (pMac->pmc.pmcState != REQUEST_EXIT_WOWL)
            {
                pmcLog(pMac, LOGE, FL("Got Exit WOWL rsp Message while in state %d"), pMac->pmc.pmcState);
                break;
            }

         /* Enter BMPS State */
         if (pMsg->statusCode != eSIR_SME_SUCCESS) {
            pmcLog(pMac, LOGP, "PMC: response message to request to exit "
               "WOWL indicates failure, status %d", pMsg->statusCode);
         }
            pmcEnterBmpsState(pMac);
         break;

    default:
        pmcLog(pMac, LOGE, FL("Invalid message type %d received"), pMsg->messageType);
        PMC_ABORT;
        break;
        }//switch

        if( fRemoveCommand )
        {
            if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
            {
                pmcReleaseCommand( pMac, pCommand );
                smeProcessPendingQueue( pMac );
            }
        }
    }
    else
    {
        pmcLog(pMac, LOGE, FL("message type %d received but no request is found"), pMsg->messageType);
    }
}


/******************************************************************************
*
* Name:  pmcMessageProcessor
*
* Description:
*    Process a message received by PMC.
*
* Parameters:
*    hHal - HAL handle for device
*    pMsg - pointer to received message
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcMessageProcessor (tHalHandle hHal, tSirSmeRsp *pMsg)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcMessageProcessor, message type %d"), pMsg->messageType);

    switch( pMsg->messageType )
    {
    case eWNI_PMC_EXIT_BMPS_IND:
    //When PMC needs to handle more indication from PE, they need to be added here.
    {
        /* Device left BMPS on its own. */
        pmcLog(pMac, LOGW, FL("Rcvd eWNI_PMC_EXIT_BMPS_IND with status = %d"), pMsg->statusCode);
        /* Check that we are in the correct state for this message. */
        switch(pMac->pmc.pmcState)
        {
        case BMPS:
        case REQUEST_START_UAPSD:
        case UAPSD:
        case REQUEST_STOP_UAPSD:
        case REQUEST_ENTER_WOWL:
        case WOWL:
        case REQUEST_EXIT_WOWL:
        case REQUEST_FULL_POWER:
            pmcLog(pMac, LOGW, FL("Got eWNI_PMC_EXIT_BMPS_IND while in state %d"), pMac->pmc.pmcState);
            break;
        default:
            pmcLog(pMac, LOGE, FL("Got eWNI_PMC_EXIT_BMPS_IND while in state %d"), pMac->pmc.pmcState);
            PMC_ABORT;
            break;
        }

        /* Enter Full Power State. */
        if (pMsg->statusCode != eSIR_SME_SUCCESS)
        {
            pmcLog(pMac, LOGP, FL("Exit BMPS indication indicates failure, status %d"), pMsg->statusCode);
        }
        else
        {
            tpSirSmeExitBmpsInd pExitBmpsInd = (tpSirSmeExitBmpsInd)pMsg;
            pmcEnterRequestFullPowerState(hHal, pExitBmpsInd->exitBmpsReason);
        }
        break;
    }

    default:
        pmcProcessResponse( pMac, pMsg );
        break;
    }

}


tANI_BOOLEAN pmcValidateConnectState( tHalHandle hHal )
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   if ( !csrIsInfraConnected( pMac ) )
   {
      pmcLog(pMac, LOGW, "PMC: STA not associated. BMPS cannot be entered");
      return eANI_BOOLEAN_FALSE;
   }

   //Cannot have other session
   if ( csrIsIBSSStarted( pMac ) )
   {
      pmcLog(pMac, LOGW, "PMC: IBSS started. BMPS cannot be entered");
      return eANI_BOOLEAN_FALSE;
   }
   if ( csrIsBTAMPStarted( pMac ) )
   {
      pmcLog(pMac, LOGW, "PMC: BT-AMP exists. BMPS cannot be entered");
      return eANI_BOOLEAN_FALSE;
   }
   if ((vos_concurrent_open_sessions_running()) &&
       (csrIsConcurrentInfraConnected( pMac ) ||
       (vos_get_concurrency_mode()& VOS_SAP) ||
       (vos_get_concurrency_mode()& VOS_P2P_GO)))
   {
      pmcLog(pMac, LOGW, "PMC: Multiple active sessions exists. BMPS cannot be entered");
      return eANI_BOOLEAN_FALSE;
   }
#ifdef FEATURE_WLAN_TDLS
   if (pMac->isTdlsPowerSaveProhibited)
   {
      pmcLog(pMac, LOGE, FL("TDLS peer(s) connected/discovery sent. Dont enter BMPS"));
      return eANI_BOOLEAN_FALSE;
   }
#endif
   return eANI_BOOLEAN_TRUE;
}

tANI_BOOLEAN pmcAllowImps( tHalHandle hHal )
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    //Cannot have other session like IBSS or BT AMP running
    if ( csrIsIBSSStarted( pMac ) )
    {
       pmcLog(pMac, LOGW, "PMC: IBSS started. IMPS cannot be entered");
       return eANI_BOOLEAN_FALSE;
    }
    if ( csrIsBTAMPStarted( pMac ) )
    {
       pmcLog(pMac, LOGW, "PMC: BT-AMP exists. IMPS cannot be entered");
       return eANI_BOOLEAN_FALSE;
    }

    //All sessions must be disconnected to allow IMPS
    if ( !csrIsAllSessionDisconnected( pMac ) )
    {
       pmcLog(pMac, LOGW, "PMC: Atleast one connected session. IMPS cannot be entered");
       return eANI_BOOLEAN_FALSE;
    }

    return eANI_BOOLEAN_TRUE;
}

/******************************************************************************
*
* Name:  pmcRequestBmps
*
* Description:
*    Request that the device be put in BMPS state.
*
* Parameters:
*    hHal - HAL handle for device
*    callbackRoutine - Callback routine invoked in case of success/failure
*    callbackContext - value to be passed as parameter to routine specified
*                      above
*
* Returns:
*    eHAL_STATUS_SUCCESS - device is in BMPS state
*    eHAL_STATUS_FAILURE - device cannot be brought to BMPS state
*    eHAL_STATUS_PMC_PENDING - device is being brought to BMPS state,
*
******************************************************************************/
eHalStatus pmcRequestBmps (
    tHalHandle hHal,
    void (*callbackRoutine) (void *callbackContext, eHalStatus status),
    void *callbackContext)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tpRequestBmpsEntry pEntry;
   eHalStatus status;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

   vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
   psRequest.event_subtype = WLAN_BMPS_ENTER_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

   pmcLog(pMac, LOG2, "PMC: entering pmcRequestBmps");

   /* If already in BMPS, just return. */
   if (pMac->pmc.pmcState == BMPS || REQUEST_START_UAPSD == pMac->pmc.pmcState || UAPSD == pMac->pmc.pmcState)
   {
      pmcLog(pMac, LOG2, "PMC: Device already in BMPS pmcState %d", pMac->pmc.pmcState);
      pMac->pmc.bmpsRequestedByHdd = TRUE;
      return eHAL_STATUS_SUCCESS;
   }
   
   status = pmcEnterBmpsCheck( pMac );
   if(HAL_STATUS_SUCCESS( status ))
   {
      /* If DUT exits from WoWL because of wake-up indication then it enters
       * into WoWL again. Disable WoWL only when user explicitly disables.
       */
      if(pMac->pmc.wowlModeRequired == FALSE && pMac->pmc.wowlExitSrc == eWOWL_EXIT_WAKEIND)
      {
          pMac->pmc.wowlModeRequired = TRUE;
      }

      status = pmcEnterRequestBmpsState(hHal);

      /* Enter Request BMPS State. */
      if ( HAL_STATUS_SUCCESS( status ) )
      {
         /* Remember that HDD requested BMPS. This flag will be used to put the
            device back into BMPS if any module other than HDD (e.g. CSR, QoS, or BAP)
            requests full power for any reason */
         pMac->pmc.bmpsRequestedByHdd = TRUE;

         /* If able to enter Request BMPS State, then request is pending.
            Allocate entry for request BMPS callback routine list. */
         pEntry = vos_mem_malloc(sizeof(tRequestBmpsEntry));
         if ( NULL == pEntry )
         {
            pmcLog(pMac, LOGE, "PMC: cannot allocate memory for request "
                  "BMPS routine list entry");
            return eHAL_STATUS_FAILURE;
         }

         /* Store routine and context in entry. */
         pEntry->callbackRoutine = callbackRoutine;
         pEntry->callbackContext = callbackContext;

         /* Add entry to list. */
         csrLLInsertTail(&pMac->pmc.requestBmpsList, &pEntry->link, FALSE);

         status = eHAL_STATUS_PMC_PENDING;
      }
      else
      {
         status = eHAL_STATUS_FAILURE;
      }
   }
   /* Retry to enter the BMPS if the
      status = eHAL_STATUS_PMC_NOT_NOW */
   else if (status == eHAL_STATUS_PMC_NOT_NOW)
   {
      pmcStopTrafficTimer(hHal);
      pmcLog(pMac, LOG1, FL("Can't enter BMPS+++"));
      if (pmcShouldBmpsTimerRun(pMac))
      {
         if (pmcStartTrafficTimer(pMac,
                                  pMac->pmc.bmpsConfig.trafficMeasurePeriod)
                                  != eHAL_STATUS_SUCCESS)
         {
            pmcLog(pMac, LOG1, FL("Cannot start BMPS Retry timer"));
         }
         pmcLog(pMac, LOG1,
                FL("BMPS Retry Timer already running or started"));
      }
   }

   return status;
}

/******************************************************************************
*
* Name:  pmcStartUapsd
*
* Description:
*    Request that the device be put in UAPSD state.
*
* Parameters:
*    hHal - HAL handle for device
*    callbackRoutine - Callback routine invoked in case of success/failure
*    callbackContext - value to be passed as parameter to routine specified
*                      above
*
* Returns:
*    eHAL_STATUS_SUCCESS - device is in UAPSD state
*    eHAL_STATUS_FAILURE - device cannot be brought to UAPSD state
*    eHAL_STATUS_PMC_PENDING - device is being brought to UAPSD state
*    eHAL_STATUS_PMC_DISABLED - UAPSD is disabled or BMPS mode is disabled
*
******************************************************************************/
eHalStatus pmcStartUapsd (
    tHalHandle hHal,
    void (*callbackRoutine) (void *callbackContext, eHalStatus status),
    void *callbackContext)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tpStartUapsdEntry pEntry;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

   vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
   psRequest.event_subtype = WLAN_UAPSD_START_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

   pmcLog(pMac, LOG2, "PMC: entering pmcStartUapsd");

   if( !PMC_IS_READY(pMac) )
   {
       pmcLog(pMac, LOGE, FL("Requesting UAPSD when PMC not ready"));
       pmcLog(pMac, LOGE, FL("pmcReady = %d pmcState = %s"),
           pMac->pmc.pmcReady, pmcGetPmcStateStr(pMac->pmc.pmcState));
       return eHAL_STATUS_FAILURE;
   }

   /* Check if BMPS is enabled. */
   if (!pMac->pmc.bmpsEnabled)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enter UAPSD. BMPS is disabled");
      return eHAL_STATUS_PMC_DISABLED;
   }

   /* Check if UAPSD is enabled. */
   if (!pMac->pmc.uapsdEnabled)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enter UAPSD. UAPSD is disabled");
      return eHAL_STATUS_PMC_DISABLED;
   }

   /* If already in UAPSD, just return. */
   if (pMac->pmc.pmcState == UAPSD)
      return eHAL_STATUS_SUCCESS;

   /* Check that we are associated. */
   if (!pmcValidateConnectState( pMac ))
   {
      pmcLog(pMac, LOGE, "PMC: STA not associated with an AP. UAPSD cannot be entered");
      return eHAL_STATUS_FAILURE;
   }

   /* Enter REQUEST_START_UAPSD State. */
   if (pmcEnterRequestStartUapsdState(hHal) != eHAL_STATUS_SUCCESS)
      return eHAL_STATUS_FAILURE;

   if( NULL != callbackRoutine )
   {
      /* If success then request is pending. Allocate entry for callback routine list. */
      pEntry = vos_mem_malloc(sizeof(tStartUapsdEntry));
      if ( NULL == pEntry )
      {
         pmcLog(pMac, LOGE, "PMC: cannot allocate memory for request "
            "start UAPSD routine list entry");
         return eHAL_STATUS_FAILURE;
      }

      /* Store routine and context in entry. */
      pEntry->callbackRoutine = callbackRoutine;
      pEntry->callbackContext = callbackContext;

      /* Add entry to list. */
      csrLLInsertTail(&pMac->pmc.requestStartUapsdList, &pEntry->link, FALSE);
   }

   return eHAL_STATUS_PMC_PENDING;
}

/******************************************************************************
*
* Name:  pmcStopUapsd
*
* Description:
*    Request that the device be put out of UAPSD state.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - device is put out of UAPSD and back in BMPS state
*    eHAL_STATUS_FAILURE - device cannot be brought out of UAPSD state
*
******************************************************************************/
eHalStatus pmcStopUapsd (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tANI_S8 sessionId;
   tCsrRoamSession *pSession = NULL;

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

   vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
   psRequest.event_subtype = WLAN_UAPSD_STOP_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif
   pmcLog(pMac, LOG2, "PMC: entering pmcStopUapsd");
   sessionId = csrGetInfraSessionId(pMac);
   if (-1 != sessionId)
   {
       pSession = CSR_GET_SESSION( pMac, sessionId );
   }


   if (pSession && pSession->pCurRoamProfile && pSession->pCurRoamProfile->uapsd_mask)
       pMac->pmc.uapsdSessionRequired = TRUE;
   else
       pMac->pmc.uapsdSessionRequired = FALSE;

   /* Nothing to be done if we are already out of UAPSD. This can happen if
      some other module (HDD, BT-AMP) requested Full Power.*/
   if (pMac->pmc.pmcState != UAPSD && pMac->pmc.pmcState != REQUEST_STOP_UAPSD)
   {
      pmcLog(pMac, LOGW, "PMC: Device is already out of UAPSD "
         "state. Current state is %d", pMac->pmc.pmcState);
      return eHAL_STATUS_SUCCESS;
   }

   /* Enter REQUEST_STOP_UAPSD State*/
   if (pmcEnterRequestStopUapsdState(hHal) != eHAL_STATUS_SUCCESS)
      return eHAL_STATUS_FAILURE;

   return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcRequestStandby
    \brief  Request that the device be put in standby.
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine - Callback routine invoked in case of success/failure
    \param  callbackContext - value to be passed as parameter to callback
    \return eHalStatus  
      eHAL_STATUS_SUCCESS - device is in Standby mode
      eHAL_STATUS_FAILURE - device cannot be put in standby mode
      eHAL_STATUS_PMC_PENDING - device is being put in standby mode
  ---------------------------------------------------------------------------*/
extern eHalStatus pmcRequestStandby (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

   vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
   psRequest.event_subtype = WLAN_ENTER_STANDBY_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);
#endif

   pmcLog(pMac, LOG2, "PMC: entering pmcRequestStandby");

   /* Check if standby is enabled. */
   if (!pMac->pmc.standbyEnabled)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enter standby. Standby is disabled");
      return eHAL_STATUS_PMC_DISABLED;
   }

   if( !PMC_IS_READY(pMac) )
   {
       pmcLog(pMac, LOGE, FL("Requesting standby when PMC not ready"));
       pmcLog(pMac, LOGE, FL("pmcReady = %d pmcState = %s"),
           pMac->pmc.pmcReady, pmcGetPmcStateStr(pMac->pmc.pmcState));
       return eHAL_STATUS_FAILURE;
   }

   /* If already in STANDBY, just return. */
   if (pMac->pmc.pmcState == STANDBY)
      return eHAL_STATUS_SUCCESS;

   
   if (csrIsIBSSStarted(pMac) || csrIsBTAMPStarted(pMac))
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, 
          "WLAN: IBSS or BT-AMP session present. Cannot honor standby request");
      return eHAL_STATUS_PMC_NOT_NOW;
   }

   /* Enter Request Standby State. */
   if (pmcEnterRequestStandbyState(hHal) != eHAL_STATUS_SUCCESS)
      return eHAL_STATUS_FAILURE;

   /* Save the callback routine for when we need it. */
   pMac->pmc.standbyCallbackRoutine = callbackRoutine;
   pMac->pmc.standbyCallbackContext = callbackContext;

   return eHAL_STATUS_PMC_PENDING;
}

/* ---------------------------------------------------------------------------
    \fn pmcRegisterDeviceStateUpdateInd
    \brief  Register a callback routine that is called whenever
            the device enters a new device state (Full Power, BMPS, UAPSD)
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine -  Callback routine to be registered
    \param  callbackContext -  Cookie to be passed back during callback
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully registered
            eHAL_STATUS_FAILURE - not successfully registered  
  ---------------------------------------------------------------------------*/
extern eHalStatus pmcRegisterDeviceStateUpdateInd (tHalHandle hHal, 
   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState),
   void *callbackContext)
{

    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tpDeviceStateUpdateIndEntry pEntry;

    pmcLog(pMac, LOG2, FL("Entering pmcRegisterDeviceStateUpdateInd"));

    /* Allocate entry for device power state update indication. */
    pEntry = vos_mem_malloc(sizeof(tDeviceStateUpdateIndEntry));
    if ( NULL == pEntry )
    {
        pmcLog(pMac, LOGE, FL("Cannot allocate memory for device power state update indication"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    /* Store routine in entry. */
    pEntry->callbackRoutine = callbackRoutine;
    pEntry->callbackContext = callbackContext;

    /* Add entry to list. */
    csrLLInsertTail(&pMac->pmc.deviceStateUpdateIndList, &pEntry->link, FALSE);

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcDeregisterDeviceStateUpdateInd
    \brief  Deregister a routine that was registered for device state changes
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine -  Callback routine to be deregistered
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully deregistered
            eHAL_STATUS_FAILURE - not successfully deregistered  
  ---------------------------------------------------------------------------*/
eHalStatus pmcDeregisterDeviceStateUpdateInd (tHalHandle hHal, 
   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState))
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tListElem *pEntry;
    tpDeviceStateUpdateIndEntry pDeviceStateUpdateIndEntry;

    pmcLog(pMac, LOG2, FL("Entering pmcDeregisterDeviceStateUpdateInd"));

    /* Find entry in the power save update routine list that matches
       the specified routine and remove it. */
    pEntry = csrLLPeekHead(&pMac->pmc.deviceStateUpdateIndList, FALSE);
    while (pEntry != NULL)
    {
        pDeviceStateUpdateIndEntry = GET_BASE_ADDR(pEntry, tDeviceStateUpdateIndEntry, link);
        if (pDeviceStateUpdateIndEntry->callbackRoutine == callbackRoutine)
        {
            if (!csrLLRemoveEntry(&pMac->pmc.deviceStateUpdateIndList, pEntry, FALSE))
            {
                pmcLog(pMac, LOGE, FL("Cannot remove device state update ind entry from list"));
                return eHAL_STATUS_FAILURE;
            }
            vos_mem_free(pDeviceStateUpdateIndEntry);
            return eHAL_STATUS_SUCCESS;
        }
        pEntry = csrLLNext(&pMac->pmc.deviceStateUpdateIndList, pEntry, FALSE);
    }

    /* Could not find matching entry. */
    return eHAL_STATUS_FAILURE;
}

/* ---------------------------------------------------------------------------
    \fn pmcReady
    \brief  fn to inform PMC that eWNI_SME_SYS_READY_IND has been sent to PE.
            This acts as a trigger to send a message to PE to update the power
            save related conig to FW. Note that if HDD configures any power save
            related stuff before this API is invoked, PMC will buffer all the 
            configutaion.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus pmcReady(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pmcLog(pMac, LOG2, FL("Entering pmcReady"));

    if(pMac->pmc.pmcState == STOPPED)
    {
        pmcLog(pMac, LOGP, FL("pmcReady is invoked even before pmcStart"));
        return eHAL_STATUS_FAILURE;
    }

    pMac->pmc.pmcReady = TRUE;
    if (pmcSendPowerSaveConfigMessage(hHal) != eHAL_STATUS_SUCCESS)
    {
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcWowlAddBcastPattern
    \brief  Add a pattern for Pattern Byte Matching in Wowl mode. Firmware will
            do a pattern match on these patterns when Wowl is enabled during BMPS
            mode. Note that Firmware performs the pattern matching only on 
            broadcast frames and while Libra is in BMPS mode.
    \param  hHal - The handle returned by macOpen.
    \param  pattern -  Pointer to the pattern to be added
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot add pattern
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcWowlAddBcastPattern (
    tHalHandle hHal, 
    tpSirWowlAddBcastPtrn pattern,
    tANI_U8 sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

#ifdef FEATURE_WLAN_DIAG_SUPPORT
    vos_log_powersave_wow_add_ptrn_pkt_type *log_ptr = NULL;
    WLAN_VOS_DIAG_LOG_ALLOC(log_ptr, vos_log_powersave_wow_add_ptrn_pkt_type, LOG_WLAN_POWERSAVE_WOW_ADD_PTRN_C);
#endif //#ifdef FEATURE_WLAN_DIAG_SUPPORT

    pmcLog(pMac, LOG2, "PMC: entering pmcWowlAddBcastPattern");

    if(pattern == NULL)
    {
        pmcLog(pMac, LOGE, FL("Null broadcast pattern being passed"));
        return eHAL_STATUS_FAILURE;
    }

    if( pSession == NULL)
    {
        pmcLog(pMac, LOGE, FL("Session not found "));
        return eHAL_STATUS_FAILURE;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT
    if( log_ptr )
    {
       log_ptr->pattern_id = pattern->ucPatternId;
       log_ptr->pattern_byte_offset = pattern->ucPatternByteOffset;
       log_ptr->pattern_size =
           (pattern->ucPatternSize <= VOS_LOG_MAX_WOW_PTRN_SIZE) ?
           pattern->ucPatternSize : VOS_LOG_MAX_WOW_PTRN_SIZE;
       log_ptr->pattern_mask_size =
          (pattern->ucPatternMaskSize <= VOS_LOG_MAX_WOW_PTRN_MASK_SIZE) ?
           pattern->ucPatternMaskSize : VOS_LOG_MAX_WOW_PTRN_MASK_SIZE;

       vos_mem_copy(log_ptr->pattern, pattern->ucPattern,
                    SIR_WOWL_BCAST_PATTERN_MAX_SIZE);
       /* 1 bit in the pattern mask denotes 1 byte of pattern hence pattern mask size is 1/8 */
       vos_mem_copy(log_ptr->pattern_mask, pattern->ucPatternMask,
                    SIR_WOWL_BCAST_PATTERN_MAX_SIZE >> 3);
    }

    WLAN_VOS_DIAG_LOG_REPORT(log_ptr);
#endif


    if(pattern->ucPatternId >= SIR_WOWL_BCAST_MAX_NUM_PATTERNS )
    {
        pmcLog(pMac, LOGE, FL("Pattern Id must range from 0 to %d"), SIR_WOWL_BCAST_MAX_NUM_PATTERNS-1);
        return eHAL_STATUS_FAILURE;
    }

    if( pMac->pmc.pmcState == STANDBY || pMac->pmc.pmcState == REQUEST_STANDBY )
    {
        pmcLog(pMac, LOGE, FL("Cannot add WoWL Pattern as chip is in %s state"),
           pmcGetPmcStateStr(pMac->pmc.pmcState));
        return eHAL_STATUS_FAILURE;
    }

    if( pMac->pmc.pmcState == IMPS || pMac->pmc.pmcState == REQUEST_IMPS )
    {
        pmcLog(pMac, LOGE, FL("Cannot add WoWL Pattern as chip is in %s state"),
           pmcGetPmcStateStr(pMac->pmc.pmcState));
        return eHAL_STATUS_FAILURE;
    }

    if( !csrIsConnStateConnected(pMac, sessionId) )
    {
        pmcLog(pMac, LOGE, FL("Cannot add WoWL Pattern session in %d state"),
           pSession->connectState);
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pattern->bssId, pSession->connectedProfile.bssid, sizeof(tSirMacAddr));

    if (pmcSendMessage(hHal, eWNI_PMC_WOWL_ADD_BCAST_PTRN, pattern, sizeof(tSirWowlAddBcastPtrn))
        != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Send of eWNI_PMC_WOWL_ADD_BCAST_PTRN to PE failed"));
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcWowlDelBcastPattern
    \brief  Delete a pattern that was added for Pattern Byte Matching.
    \param  hHal - The handle returned by macOpen.
    \param  pattern -  Pattern to be deleted
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot delete pattern
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcWowlDelBcastPattern (
    tHalHandle hHal, 
    tpSirWowlDelBcastPtrn pattern,
    tANI_U8  sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    WLAN_VOS_DIAG_EVENT_DEF(wowRequest, vos_event_wlan_powersave_wow_payload_type);

    vos_mem_zero(&wowRequest, sizeof(vos_event_wlan_powersave_wow_payload_type));
    wowRequest.event_subtype = WLAN_WOW_DEL_PTRN_REQ;
    wowRequest.wow_del_ptrn_id = pattern->ucPatternId; 

    WLAN_VOS_DIAG_EVENT_REPORT(&wowRequest, EVENT_WLAN_POWERSAVE_WOW);
#endif

    pmcLog(pMac, LOG2, "PMC: entering pmcWowlDelBcastPattern");

    if( NULL == pSession )
    {
        pmcLog(pMac, LOGE, FL("Session not found "));
        return eHAL_STATUS_FAILURE;
    }

    if(pattern->ucPatternId >= SIR_WOWL_BCAST_MAX_NUM_PATTERNS )
    {
        pmcLog(pMac, LOGE, FL("Pattern Id must range from 0 to %d"),
            SIR_WOWL_BCAST_MAX_NUM_PATTERNS-1);
        return eHAL_STATUS_FAILURE;
    }

    if(pMac->pmc.pmcState == STANDBY || pMac->pmc.pmcState == REQUEST_STANDBY)
    {
        pmcLog(pMac, LOGE, FL("Cannot delete WoWL Pattern as chip is in %s state"),
           pmcGetPmcStateStr(pMac->pmc.pmcState));
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pattern->bssId, pSession->connectedProfile.bssid, sizeof(tSirMacAddr));

    if( pMac->pmc.pmcState == IMPS || pMac->pmc.pmcState == REQUEST_IMPS )
    {
        eHalStatus status;

        vos_mem_copy(pattern->bssId, pSession->connectedProfile.bssid,
                     sizeof(tSirMacAddr));
        //Wake up the chip first
        status = pmcDeferMsg( pMac, eWNI_PMC_WOWL_DEL_BCAST_PTRN, 
                                    pattern, sizeof(tSirWowlDelBcastPtrn) );

        if( eHAL_STATUS_PMC_PENDING == status )
        {
            return eHAL_STATUS_SUCCESS;
        }
        else 
        {
            //either fail or already in full power
            if( !HAL_STATUS_SUCCESS( status ) )
            {
                return ( status );
            }
            //else let it through because it is in full power state
        }
    }

    if (pmcSendMessage(hHal, eWNI_PMC_WOWL_DEL_BCAST_PTRN, pattern, sizeof(tSirWowlDelBcastPtrn))
        != eHAL_STATUS_SUCCESS)
    {
        pmcLog(pMac, LOGE, FL("Send of eWNI_PMC_WOWL_DEL_BCAST_PTRN to PE failed"));
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcEnterWowl
    \brief  Request that the device be brought to full power state.
            Note 1: If "fullPowerReason" specificied in this API is set to
            eSME_FULL_PWR_NEEDED_BY_HDD, PMC will clear any "buffered wowl" requests
            and also clear any "buffered BMPS requests by HDD". Assumption is that since
            HDD is requesting full power, we need to undo any previous HDD requests for 
            BMPS (using sme_RequestBmps) or WoWL (using sme_EnterWoWL). If the reason is
            specified anything other than above, the buffered requests for BMPS and WoWL
            will not be cleared.
            Note 2: Requesting full power (no matter what the fullPowerReason is) doesn't
            disable the "auto bmps timer" (if it is enabled) or clear any "buffered uapsd
            request".
            Note 3: When the device finally enters Full Power PMC will start a timer 
            if any of the following holds true:
            - Auto BMPS mode is enabled
            - Uapsd request is pending
            - HDD's request for BMPS is pending
            - HDD's request for WoWL is pending
            On timer expiry PMC will attempt to put the device in BMPS mode if following 
            (in addition to those listed above) holds true:
            - Polling of all modules through the Power Save Check routine passes
            - STA is associated to an access point
    \param  hHal - The handle returned by macOpen.
    \param  - enterWowlCallbackRoutine Callback routine invoked in case of success/failure
    \param  - enterWowlCallbackContext -  Cookie to be passed back during callback
    \param  - wakeReasonIndCB Callback routine invoked for Wake Reason Indication
    \param  - wakeReasonIndCBContext -  Cookie to be passed back during callback
    \param  - fullPowerReason - Reason why this API is being invoked. SME needs to
              distinguish between BAP and HDD requests
    \return eHalStatus - status 
     eHAL_STATUS_SUCCESS - device brought to full power state
     eHAL_STATUS_FAILURE - device cannot be brought to full power state
     eHAL_STATUS_PMC_PENDING - device is being brought to full power state,
  ---------------------------------------------------------------------------*/
eHalStatus pmcEnterWowl ( 
    tHalHandle hHal, 
    void (*enterWowlCallbackRoutine) (void *callbackContext, eHalStatus status),
    void *enterWowlCallbackContext,
#ifdef WLAN_WAKEUP_EVENTS
    void (*wakeReasonIndCB) (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd),
    void *wakeReasonIndCBContext,
#endif // WLAN_WAKEUP_EVENTS
    tpSirSmeWowlEnterParams wowlEnterParams, tANI_U8 sessionId)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(wowRequest, vos_event_wlan_powersave_wow_payload_type);

   vos_mem_zero(&wowRequest, sizeof(vos_event_wlan_powersave_wow_payload_type));
   wowRequest.event_subtype = WLAN_WOW_ENTER_REQ;
   wowRequest.wow_type = 0;

   if(wowlEnterParams->ucMagicPktEnable)
   {
       wowRequest.wow_type |= 1;
       vos_mem_copy(wowRequest.wow_magic_pattern,
                   (tANI_U8 *)wowlEnterParams->magicPtrn, 6);
   }

   if(wowlEnterParams->ucPatternFilteringEnable)
   {
       wowRequest.wow_type |= 2;
   }
   WLAN_VOS_DIAG_EVENT_REPORT(&wowRequest, EVENT_WLAN_POWERSAVE_WOW);
#endif

   pmcLog(pMac, LOG2, FL("PMC: entering pmcEnterWowl"));

   if( NULL == pSession )
   {
       pmcLog(pMac, LOGE, FL("Session not found "));
       return eHAL_STATUS_FAILURE;
   }

   if( !PMC_IS_READY(pMac) )
   {
       pmcLog(pMac, LOGE, FL("Requesting WoWL when PMC not ready"));
       pmcLog(pMac, LOGE, FL("pmcReady = %d pmcState = %s"),
           pMac->pmc.pmcReady, pmcGetPmcStateStr(pMac->pmc.pmcState));
       return eHAL_STATUS_FAILURE;
   }

   /* Check if BMPS is enabled. */
   if (!pMac->pmc.bmpsEnabled)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enter WoWL. BMPS is disabled");
      return eHAL_STATUS_PMC_DISABLED;
   }

   /* Check if WoWL is enabled. */
   if (!pMac->pmc.wowlEnabled)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enter WoWL. WoWL is disabled");
      return eHAL_STATUS_PMC_DISABLED;
   }

   /* Check that we are associated with single Session. */
   if (!pmcValidateConnectState( pMac ))
   {
      pmcLog(pMac, LOGE, "PMC: Cannot enable WOWL. STA not associated "
             "with an Access Point in Infra Mode with single active session");
      return eHAL_STATUS_FAILURE;
   }

   /* Is there a pending UAPSD request? HDD should have triggered QoS
      module to do the necessary cleanup before triggring WOWL*/
   if(pMac->pmc.uapsdSessionRequired)
   {
      pmcLog(pMac, LOGE, "PMC: Cannot request WOWL. Pending UAPSD request");
      return eHAL_STATUS_FAILURE;
   }

   /* Check that entry into a power save mode is allowed at this time. */
   if (pMac->pmc.pmcState == FULL_POWER && !pmcPowerSaveCheck(hHal))
   {
      pmcLog(pMac, LOGE, "PMC: Power save check failed. WOWL request "
             "will not be accepted");
      return eHAL_STATUS_FAILURE;
   }

   vos_mem_copy(wowlEnterParams->bssId, pSession->connectedProfile.bssid,
               sizeof(tSirMacAddr));

   // To avoid race condition, set callback routines before sending message.
   /* cache the WOWL information */
   pMac->pmc.wowlEnterParams = *wowlEnterParams;
   pMac->pmc.enterWowlCallbackRoutine = enterWowlCallbackRoutine;
   pMac->pmc.enterWowlCallbackContext = enterWowlCallbackContext;
#ifdef WLAN_WAKEUP_EVENTS
   /* Cache the Wake Reason Indication callback information */
   pMac->pmc.wakeReasonIndCB = wakeReasonIndCB;
   pMac->pmc.wakeReasonIndCBContext = wakeReasonIndCBContext;
#endif // WLAN_WAKEUP_EVENTS

   /* Enter Request WOWL State. */
   if (pmcRequestEnterWowlState(hHal, wowlEnterParams) != eHAL_STATUS_SUCCESS)
      return eHAL_STATUS_FAILURE;

   pMac->pmc.wowlModeRequired = TRUE;

   /* By default set wowlExitSrc to eWOWL_EXIT_WAKEIND, so that device will
    * come out of WoWL only when user explicity disables WoWL.
    */
   pMac->pmc.wowlExitSrc = eWOWL_EXIT_WAKEIND;

   return eHAL_STATUS_PMC_PENDING;
}

/* ---------------------------------------------------------------------------
    \fn pmcExitWowl
    \brief  This is the SME API exposed to HDD to request exit from WoWLAN mode. 
            SME will initiate exit from WoWLAN mode and device will be put in BMPS 
            mode.
    \param  hHal - The handle returned by macOpen.
            wowlExitSrc - is wowl exiting because of wakeup pkt or user
                          explicitly disabling WoWL
    \return eHalStatus
            eHAL_STATUS_FAILURE  Device cannot exit WoWLAN mode.
            eHAL_STATUS_SUCCESS  Request accepted to exit WoWLAN mode. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcExitWowl (tHalHandle hHal, tWowlExitSource wowlExitSrc)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
   WLAN_VOS_DIAG_EVENT_DEF(wowRequest, vos_event_wlan_powersave_wow_payload_type);

   vos_mem_zero(&wowRequest, sizeof(vos_event_wlan_powersave_wow_payload_type));
   wowRequest.event_subtype = WLAN_WOW_EXIT_REQ;

   WLAN_VOS_DIAG_EVENT_REPORT(&wowRequest, EVENT_WLAN_POWERSAVE_WOW);
#endif

   pmcLog(pMac, LOG2, "PMC: entering pmcExitWowl");

   /* Clear any buffered command for entering WOWL */
   pMac->pmc.wowlModeRequired = FALSE;

   /* are we exiting from WoW because of wake indication
      or user disabling this feature */
   pMac->pmc.wowlExitSrc = wowlExitSrc;

   /* Enter REQUEST_EXIT_WOWL State*/
   if (pmcRequestExitWowlState(hHal) != eHAL_STATUS_SUCCESS)
      return eHAL_STATUS_FAILURE;

   if (eWOWL_EXIT_USER == wowlExitSrc)
   {
       /* Clear the callback routines */
       pMac->pmc.enterWowlCallbackRoutine = NULL;
       pMac->pmc.enterWowlCallbackContext = NULL;
#ifdef WLAN_WAKEUP_EVENTS
       pMac->pmc.wakeReasonIndCB = NULL;
       pMac->pmc.wakeReasonIndCBContext = NULL;
#endif
   }

   return eHAL_STATUS_SUCCESS;
}



/* ---------------------------------------------------------------------------
    \fn pmcSetHostOffload
    \brief  Set the host offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest - Pointer to the offload request.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcSetHostOffload (tHalHandle hHal, tpSirHostOffloadReq pRequest, 
                                   tANI_U8 sessionId)
{
    tpSirHostOffloadReq pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s: IP address = %d.%d.%d.%d", __func__,
        pRequest->params.hostIpv4Addr[0], pRequest->params.hostIpv4Addr[1],
        pRequest->params.hostIpv4Addr[2], pRequest->params.hostIpv4Addr[3]);

    if(NULL == pSession )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: SESSION not Found\n", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pRequestBuf = vos_mem_malloc(sizeof(tSirHostOffloadReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for host offload request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_copy(pRequest->bssId, pSession->connectedProfile.bssid,
                 sizeof(tSirMacAddr));

    vos_mem_copy(pRequestBuf, pRequest, sizeof(tSirHostOffloadReq));

    msg.type = WDA_SET_HOST_OFFLOAD;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_SET_HOST_OFFLOAD message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcSetKeepAlive
    \brief  Set the Keep Alive feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest - Pointer to the Keep Alive.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the keepalive.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcSetKeepAlive (tHalHandle hHal, tpSirKeepAliveReq pRequest, tANI_U8 sessionId)
{
    tpSirKeepAliveReq pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_LOW, "%s: "
                  "WDA_SET_KEEP_ALIVE message", __func__);

    if(pSession == NULL )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
           " Session not Found", __func__);
        return eHAL_STATUS_FAILURE;
    }
    pRequestBuf = vos_mem_malloc(sizeof(tSirKeepAliveReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                  "Not able to allocate memory for keep alive request",
                  __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_copy(pRequest->bssId, pSession->connectedProfile.bssid, sizeof(tSirMacAddr));
    vos_mem_copy(pRequestBuf, pRequest, sizeof(tSirKeepAliveReq));

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_LOW, "buff TP %d "
              "input TP %d ", pRequestBuf->timePeriod, pRequest->timePeriod);

    msg.type = WDA_SET_KEEP_ALIVE;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                  "Not able to post WDA_SET_KEEP_ALIVE message to WDA",
                  __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}


#ifdef WLAN_NS_OFFLOAD

/* ---------------------------------------------------------------------------
    \fn pmcSetNSOffload
    \brief  Set the host offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest - Pointer to the offload request.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcSetNSOffload (tHalHandle hHal, tpSirHostOffloadReq pRequest, 
                                 tANI_U8 sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tpSirHostOffloadReq pRequestBuf;
    vos_msg_t msg;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if( NULL == pSession )
    {
        pmcLog(pMac, LOGE, FL("Session not found "));
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pRequest->bssId, pSession->connectedProfile.bssid, 
                sizeof(tSirMacAddr));

    pRequestBuf = vos_mem_malloc(sizeof(tSirHostOffloadReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for NS offload request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }
    vos_mem_copy(pRequestBuf, pRequest, sizeof(tSirHostOffloadReq));

    msg.type = WDA_SET_NS_OFFLOAD;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post SIR_HAL_SET_HOST_OFFLOAD message to HAL", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

#endif //WLAN_NS_OFFLOAD


void pmcClosePowerSaveCheckList(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tpPowerSaveCheckEntry pPowerSaveCheckEntry;

    csrLLLock(&pMac->pmc.powerSaveCheckList);
    while ( (pEntry = csrLLRemoveHead(&pMac->pmc.powerSaveCheckList, FALSE)) )
    {
        pPowerSaveCheckEntry = GET_BASE_ADDR(pEntry, tPowerSaveCheckEntry, link);
        vos_mem_free(pPowerSaveCheckEntry);
    }
    csrLLUnlock(&pMac->pmc.powerSaveCheckList);
    csrLLClose(&pMac->pmc.powerSaveCheckList);
}


void pmcCloseRequestFullPowerList(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tpRequestFullPowerEntry pRequestFullPowerEntry;

    csrLLLock(&pMac->pmc.requestFullPowerList);
    while ( (pEntry = csrLLRemoveHead(&pMac->pmc.requestFullPowerList, FALSE)) )
    {
        pRequestFullPowerEntry = GET_BASE_ADDR(pEntry, tRequestFullPowerEntry, link);
        vos_mem_free(pRequestFullPowerEntry);
    }
    csrLLUnlock(&pMac->pmc.requestFullPowerList);
    csrLLClose(&pMac->pmc.requestFullPowerList);
}


void pmcCloseRequestBmpsList(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tpRequestBmpsEntry pRequestBmpsEntry;

    csrLLLock(&pMac->pmc.requestBmpsList);
    while ( (pEntry = csrLLRemoveHead(&pMac->pmc.requestBmpsList, FALSE)) )
    {
        pRequestBmpsEntry = GET_BASE_ADDR(pEntry, tRequestBmpsEntry, link);
        vos_mem_free(pRequestBmpsEntry);
    }
    csrLLUnlock(&pMac->pmc.requestBmpsList);
    csrLLClose(&pMac->pmc.requestBmpsList);
}


void pmcCloseRequestStartUapsdList(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tpStartUapsdEntry pStartUapsdEntry;

    csrLLLock(&pMac->pmc.requestStartUapsdList);
    while ( (pEntry = csrLLRemoveHead(&pMac->pmc.requestStartUapsdList, FALSE)) )
    {
        pStartUapsdEntry = GET_BASE_ADDR(pEntry, tStartUapsdEntry, link);
        vos_mem_free(pStartUapsdEntry);
    }
    csrLLUnlock(&pMac->pmc.requestStartUapsdList);
    csrLLClose(&pMac->pmc.requestStartUapsdList);
}


void pmcCloseDeviceStateUpdateList(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tpDeviceStateUpdateIndEntry pDeviceStateUpdateIndEntry;

    csrLLLock(&pMac->pmc.deviceStateUpdateIndList);
    while ( (pEntry = csrLLRemoveHead(&pMac->pmc.deviceStateUpdateIndList, FALSE)) )
    {
        pDeviceStateUpdateIndEntry = GET_BASE_ADDR(pEntry, tDeviceStateUpdateIndEntry, link);
        vos_mem_free(pDeviceStateUpdateIndEntry);
    }
    csrLLUnlock(&pMac->pmc.deviceStateUpdateIndList);
    csrLLClose(&pMac->pmc.deviceStateUpdateIndList);
}


void pmcCloseDeferredMsgList(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tPmcDeferredMsg *pDeferredMsg;

    csrLLLock(&pMac->pmc.deferredMsgList);
    while ( (pEntry = csrLLRemoveHead(&pMac->pmc.deferredMsgList, FALSE)) )
    {
        pDeferredMsg = GET_BASE_ADDR(pEntry, tPmcDeferredMsg, link);
        vos_mem_free(pDeferredMsg);
    }
    csrLLUnlock(&pMac->pmc.deferredMsgList);
    csrLLClose(&pMac->pmc.deferredMsgList);
}


#ifdef FEATURE_WLAN_SCAN_PNO

static tSirRetStatus 
pmcPopulateMacHeader( tpAniSirGlobal pMac,
                      tANI_U8* pBD,
                      tANI_U8 type,
                      tANI_U8 subType,
                      tSirMacAddr peerAddr,
                      tSirMacAddr selfMacAddr)
{
    tSirRetStatus   statusCode = eSIR_SUCCESS;
    tpSirMacMgmtHdr pMacHdr;
    
    /// Prepare MAC management header
    pMacHdr = (tpSirMacMgmtHdr) (pBD);

    // Prepare FC
    pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
    pMacHdr->fc.type    = type;
    pMacHdr->fc.subType = subType;

    // Prepare Address 1
    vos_mem_copy((tANI_U8 *) pMacHdr->da, (tANI_U8 *) peerAddr, sizeof( tSirMacAddr ));

    sirCopyMacAddr(pMacHdr->sa,selfMacAddr);

    // Prepare Address 3
    vos_mem_copy((tANI_U8 *) pMacHdr->bssId, (tANI_U8 *) peerAddr, sizeof( tSirMacAddr ));
    return statusCode;
} /*** pmcPopulateMacHeader() ***/


static tSirRetStatus
pmcPrepareProbeReqTemplate(tpAniSirGlobal pMac,
                           tANI_U8        nChannelNum,
                           tANI_U32       dot11mode,
                           tSirMacAddr    selfMacAddr,
                           tANI_U8        *pFrame,
                           tANI_U16       *pusLen)
{
    tDot11fProbeRequest pr;
    tANI_U32            nStatus, nBytes, nPayload;
    tSirRetStatus       nSirStatus;
    /*Bcast tx*/
    tSirMacAddr         bssId = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    // The scheme here is to fill out a 'tDot11fProbeRequest' structure
    // and then hand it off to 'dot11fPackProbeRequest' (for
    // serialization).  We start by zero-initializing the structure:
    vos_mem_set(( tANI_U8* )&pr, sizeof( pr ), 0);

    PopulateDot11fSuppRates( pMac, nChannelNum, &pr.SuppRates,NULL);

    if ( WNI_CFG_DOT11_MODE_11B != dot11mode )
    {
        PopulateDot11fExtSuppRates1( pMac, nChannelNum, &pr.ExtSuppRates );
    }

    
    if (IS_DOT11_MODE_HT(dot11mode))
    {
       PopulateDot11fHTCaps( pMac, NULL, &pr.HTCaps );
    }
    
    // That's it-- now we pack it.  First, how much space are we going to
    // need?
    nStatus = dot11fGetPackedProbeRequestSize( pMac, &pr, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "Failed to calculate the packed size f"
                  "or a Probe Request (0x%08x).", nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "There were warnings while calculating"
                  "the packed size for a Probe Request ("
                  "0x%08x).", nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );
  
    /* Prepare outgoing frame*/
    vos_mem_set(pFrame, nBytes, 0);

    // Next, we fill out the buffer descriptor:
    nSirStatus = pmcPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_REQ, bssId,selfMacAddr);

    if ( eSIR_SUCCESS != nSirStatus )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
        "Failed to populate the buffer descriptor for a Probe Request (%d).",
                nSirStatus );
        return nSirStatus;      // allocated!
    }

    // That done, pack the Probe Request:
    nStatus = dot11fPackProbeRequest( pMac, &pr, pFrame +
                                      sizeof( tSirMacMgmtHdr ),
                                      nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "Failed to pack a Probe Request (0x%08x).", nStatus );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
            "There were warnings while packing a Probe Request" );
    }

    *pusLen = nPayload + sizeof(tSirMacMgmtHdr); 
    return eSIR_SUCCESS;
} // End pmcPrepareProbeReqTemplate.


eHalStatus pmcSetPreferredNetworkList
(
    tHalHandle hHal, 
    tpSirPNOScanReq pRequest, 
    tANI_U8 sessionId, 
    preferredNetworkFoundIndCallback callbackRoutine, 
    void *callbackContext
)
{
    tpSirPNOScanReq pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_U8 ucDot11Mode;

    if (NULL == pSession)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: pSession is NULL", __func__);
        return eHAL_STATUS_FAILURE;
    }
    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
               "%s: SSID = 0x%08x%08x%08x%08x%08x%08x%08x%08x, "
               "0x%08x%08x%08x%08x%08x%08x%08x%08x", __func__,
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[0]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[4]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[8]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[12]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[16]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[20]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[24]),
               *((v_U32_t *) &pRequest->aNetworks[0].ssId.ssId[28]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[0]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[4]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[8]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[12]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[16]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[20]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[24]),
               *((v_U32_t *) &pRequest->aNetworks[1].ssId.ssId[28]));
    if (!pSession)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "%s: pSessionis NULL", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pRequestBuf = vos_mem_malloc(sizeof(tSirPNOScanReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for PNO request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_copy(pRequestBuf, pRequest, sizeof(tSirPNOScanReq));

    /*Must translate the mode first*/
    ucDot11Mode = (tANI_U8) csrTranslateToWNICfgDot11Mode(pMac, 
                                       csrFindBestPhyMode( pMac, pMac->roam.configParam.phyMode ));

    /*Prepare a probe request for 2.4GHz band and one for 5GHz band*/
    if (eSIR_SUCCESS == pmcPrepareProbeReqTemplate(pMac, SIR_PNO_24G_DEFAULT_CH,
                              ucDot11Mode, pSession->selfMacAddr,
                              pRequestBuf->p24GProbeTemplate,
                              &pRequestBuf->us24GProbeTemplateLen))
    {
        /* Append IE passed by supplicant(if any) to probe request */
        if ((0 < pRequest->us24GProbeTemplateLen) &&
            ((pRequestBuf->us24GProbeTemplateLen +
              pRequest->us24GProbeTemplateLen) < SIR_PNO_MAX_PB_REQ_SIZE ))
        {
            vos_mem_copy((tANI_U8 *)&pRequestBuf->p24GProbeTemplate +
                          pRequestBuf->us24GProbeTemplateLen,
                          (tANI_U8 *)&pRequest->p24GProbeTemplate,
                          pRequest->us24GProbeTemplateLen);
            pRequestBuf->us24GProbeTemplateLen +=
                                                pRequest->us24GProbeTemplateLen;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                   "%s: pRequest->us24GProbeTemplateLen = %d", __func__,
                    pRequest->us24GProbeTemplateLen);
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                   "%s: Extra ie discarded on 2.4G, IE length = %d Max IE length is %d",
                   __func__, pRequest->us24GProbeTemplateLen, SIR_PNO_MAX_PB_REQ_SIZE);
        }
    }

    if (eSIR_SUCCESS == pmcPrepareProbeReqTemplate(pMac, SIR_PNO_5G_DEFAULT_CH,
                               ucDot11Mode, pSession->selfMacAddr,
                               pRequestBuf->p5GProbeTemplate,
                               &pRequestBuf->us5GProbeTemplateLen))
    {
        /* Append IE passed by supplicant(if any) to probe request */
        if ((0 < pRequest->us5GProbeTemplateLen ) &&
            ((pRequestBuf->us5GProbeTemplateLen +
              pRequest->us5GProbeTemplateLen) < SIR_PNO_MAX_PB_REQ_SIZE ))
        {
            vos_mem_copy((tANI_U8 *)&pRequestBuf->p5GProbeTemplate +
                          pRequestBuf->us5GProbeTemplateLen,
                          (tANI_U8 *)&pRequest->p5GProbeTemplate,
                          pRequest->us5GProbeTemplateLen);
            pRequestBuf->us5GProbeTemplateLen += pRequest->us5GProbeTemplateLen;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                    "%s: pRequestBuf->us5GProbeTemplateLen = %d", __func__,
                     pRequest->us5GProbeTemplateLen);
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                   "%s: Extra IE discarded on 5G, IE length = %d Max IE length is %d",
                    __func__, pRequest->us5GProbeTemplateLen, SIR_PNO_MAX_PB_REQ_SIZE);
        }
    }

    msg.type     = WDA_SET_PNO_REQ;
    msg.reserved = 0;
    msg.bodyptr  = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_SET_PNO_REQ message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    /* Cache the Preferred Network Found Indication callback information */
    pMac->pmc.prefNetwFoundCB = callbackRoutine;
    pMac->pmc.preferredNetworkFoundIndCallbackContext = callbackContext;

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "-%s", __func__);

    return eHAL_STATUS_SUCCESS;
}

eHalStatus pmcSetRssiFilter(tHalHandle hHal,   v_U8_t        rssiThreshold)
{
    tpSirSetRSSIFilterReq pRequestBuf;
    vos_msg_t msg;


    pRequestBuf = vos_mem_malloc(sizeof(tSirSetRSSIFilterReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for PNO request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }


    pRequestBuf->rssiThreshold = rssiThreshold; 

    msg.type = WDA_SET_RSSI_FILTER_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_SET_PNO_REQ message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}


eHalStatus pmcUpdateScanParams(tHalHandle hHal, tCsrConfig *pRequest, tCsrChannel *pChannelList, tANI_U8 b11dResolved)
{
    tpSirUpdateScanParams pRequestBuf;
    vos_msg_t msg;
    int i;

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s started", __func__);

    pRequestBuf = vos_mem_malloc(sizeof(tSirUpdateScanParams));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for UpdateScanParams request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    // 
    // Fill pRequestBuf structure from pRequest
    //
    pRequestBuf->b11dEnabled    = pRequest->Is11eSupportEnabled;
    pRequestBuf->b11dResolved   = b11dResolved;
    pRequestBuf->ucChannelCount = 
        ( pChannelList->numChannels < SIR_PNO_MAX_NETW_CHANNELS_EX )?
        pChannelList->numChannels:SIR_PNO_MAX_NETW_CHANNELS_EX;

    for (i=0; i < pRequestBuf->ucChannelCount; i++)
    {    
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                  "%s: Channel List %d: %d", __FUNCTION__, i, pChannelList->channelList[i] );

        pRequestBuf->aChannels[i] = pChannelList->channelList[i];
    }
    pRequestBuf->usPassiveMinChTime = pRequest->nPassiveMinChnTime;
    pRequestBuf->usPassiveMaxChTime = pRequest->nPassiveMaxChnTime;
    pRequestBuf->usActiveMinChTime  = pRequest->nActiveMinChnTime;
    pRequestBuf->usActiveMaxChTime  = pRequest->nActiveMaxChnTime; 
    pRequestBuf->ucCBState          = PHY_SINGLE_CHANNEL_CENTERED;

    msg.type = WDA_UPDATE_SCAN_PARAMS_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_UPDATE_SCAN_PARAMS message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}
#endif // FEATURE_WLAN_SCAN_PNO

eHalStatus pmcSetPowerParams(tHalHandle hHal,   tSirSetPowerParamsReq*  pwParams, tANI_BOOLEAN forced)
{
    tSirSetPowerParamsReq* pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tpPESession     psessionEntry;

    psessionEntry = peGetValidPowerSaveSession(pMac);
    if (!forced && (psessionEntry == NULL))
    {
        return eHAL_STATUS_NOT_INITIALIZED;
    }

    pRequestBuf = vos_mem_malloc(sizeof(tSirSetPowerParamsReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for Power Paramrequest", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }


    vos_mem_copy(pRequestBuf, pwParams, sizeof(*pRequestBuf)); 


    msg.type = WDA_SET_POWER_PARAMS_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;

    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_SET_POWER_PARAMS_REQ message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
eHalStatus pmcGetFilterMatchCount
(
    tHalHandle hHal, 
    FilterMatchCountCallback callbackRoutine, 
    void *callbackContext,
    tANI_U8  sessionId
)
{
    tpSirRcvFltPktMatchRsp  pRequestBuf;
    vos_msg_t               msg;
    tpAniSirGlobal          pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
        "%s", __func__);

    if(NULL == pSession )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: Session not found ", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pRequestBuf = vos_mem_malloc(sizeof(tSirRcvFltPktMatchRsp));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: Not able to allocate "
                  "memory for Get PC Filter Match Count request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_copy(pRequestBuf->bssId, pSession->connectedProfile.bssid,
                 sizeof(tSirMacAddr));

    msg.type = WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;

    /* Cache the Packet Coalescing Filter Match Count callback information */
    if (NULL != pMac->pmc.FilterMatchCountCB)
    {
        // Do we need to check if the callback is in use? 
        // Because we are not sending the same message again when it is pending,
        // the only case when the callback is not NULL is that the previous message 
        //was timed out or failed.
        // So, it will be safe to set the callback in this case.
    }

    pMac->pmc.FilterMatchCountCB = callbackRoutine;
    pMac->pmc.FilterMatchCountCBContext = callbackContext;

    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
            "%s: Not able to post WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ "
            "message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}
#endif // WLAN_FEATURE_PACKET_FILTERING

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/* ---------------------------------------------------------------------------
    \fn pmcSetGTKOffload
    \brief  Set GTK offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pGtkOffload - Pointer to the GTK offload request.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcSetGTKOffload (tHalHandle hHal, tpSirGtkOffloadParams pGtkOffload, 
                                  tANI_U8 sessionId)
{
    tpSirGtkOffloadParams pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal   pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: KeyReplayCounter: %lld",
                __func__, pGtkOffload->ullKeyReplayCounter);

    if(NULL == pSession )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: Session not found ", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pRequestBuf = (tpSirGtkOffloadParams)vos_mem_malloc(sizeof(tSirGtkOffloadParams));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate "
                  "memory for GTK offload request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_copy(pGtkOffload->bssId, pSession->connectedProfile.bssid,
                 sizeof(tSirMacAddr));

    vos_mem_copy(pRequestBuf, pGtkOffload, sizeof(tSirGtkOffloadParams));

    msg.type = WDA_GTK_OFFLOAD_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post "
                  "SIR_HAL_SET_GTK_OFFLOAD message to HAL", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn pmcGetGTKOffload
    \brief  Get GTK offload information.
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine - Pointer to the GTK Offload Get Info response callback routine.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
eHalStatus pmcGetGTKOffload(tHalHandle hHal, GTKOffloadGetInfoCallback callbackRoutine, 
                                  void *callbackContext, tANI_U8 sessionId)
{
    tpSirGtkOffloadGetInfoRspParams  pRequestBuf;
    vos_msg_t               msg;
    tpAniSirGlobal          pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s: Entered",
                __func__);

    if(NULL == pSession )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                  "%s: Session not found ", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pRequestBuf = (tpSirGtkOffloadGetInfoRspParams)
                        vos_mem_malloc(sizeof (tSirGtkOffloadGetInfoRspParams));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate "
                  "memory for Get GTK offload request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_copy(pRequestBuf->bssId, pSession->connectedProfile.bssid, sizeof(tSirMacAddr)); 

    msg.type = WDA_GTK_OFFLOAD_GETINFO_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;

    /* Cache the Get GTK Offload callback information */
    if (NULL != pMac->pmc.GtkOffloadGetInfoCB)
    {
        // Do we need to check if the callback is in use? 
        // Because we are not sending the same message again when it is pending,
        // the only case when the callback is not NULL is that the previous message was timed out or failed.
        // So, it will be safe to set the callback in this case.
    }

    pMac->pmc.GtkOffloadGetInfoCB = callbackRoutine;
    pMac->pmc.GtkOffloadGetInfoCBContext = callbackContext;

    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_GTK_OFFLOAD_GETINFO_REQ message to WDA", 
                    __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}
#endif // WLAN_FEATURE_GTK_OFFLOAD

v_BOOL_t IsPmcImpsReqFailed (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    v_BOOL_t impsReqFailStatus;

    impsReqFailStatus = (pMac->pmc.ImpsReqFailed || pMac->pmc.ImpsReqTimerFailed);

    return impsReqFailStatus;

}

void pmcResetImpsFailStatus (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    pMac->pmc.ImpsReqFailed = VOS_FALSE;
    pMac->pmc.ImpsReqTimerFailed = VOS_FALSE;
}

#ifdef FEATURE_WLAN_BATCH_SCAN
/* -----------------------------------------------------------------------------
    \fn pmcSetBatchScanReq
    \brief  setting batch scan request in FW
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - session ID
    \param  callbackRoutine - Pointer to set batch scan request callback routine
    \param  callbackContext - Pointer to set batch scan request callback context
    \return eHalStatus
             eHAL_STATUS_FAILURE  Cannot set batch scan request
             eHAL_STATUS_SUCCESS  Request accepted.
 -----------------------------------------------------------------------------*/

eHalStatus pmcSetBatchScanReq(tHalHandle hHal, tSirSetBatchScanReq *pRequest,
    tANI_U8 sessionId, hddSetBatchScanReqCallback callbackRoutine,
    void *callbackContext)
{
    tpSirSetBatchScanReq pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pRequestBuf = vos_mem_malloc(sizeof(tSirSetBatchScanReq));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
          "%s: Not able to allocate memory for SET BATCH SCAN req", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    /* Cache HDD callback information*/
    pMac->pmc.setBatchScanReqCallback = callbackRoutine;
    pMac->pmc.setBatchScanReqCallbackContext = callbackContext;

    pRequestBuf->scanFrequency = pRequest->scanFrequency;
    pRequestBuf->numberOfScansToBatch = pRequest->numberOfScansToBatch;
    pRequestBuf->bestNetwork = pRequest->bestNetwork;
    pRequestBuf->rfBand = pRequest->rfBand;
    pRequestBuf->rtt = pRequest->rtt;

    msg.type     = WDA_SET_BATCH_SCAN_REQ;
    msg.reserved = 0;
    msg.bodyptr  = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
          "%s: Not able to post WDA_SET_BATCH_SCAN_REQ message to WDA",
          __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* -----------------------------------------------------------------------------
    \fn pmcTriggerBatchScanResultInd
    \brief  API to trigger batch scan results indications from FW
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - session ID
    \param  callbackRoutine - Pointer to get batch scan request callback routine
    \param  callbackContext - Pointer to get batch scan request callback context
    \return eHalStatus
             eHAL_STATUS_FAILURE  Cannot set batch scan request
             eHAL_STATUS_SUCCESS  Request accepted.
 -----------------------------------------------------------------------------*/

eHalStatus pmcTriggerBatchScanResultInd
(
    tHalHandle hHal, tSirTriggerBatchScanResultInd *pRequest, tANI_U8 sessionId,
    hddTriggerBatchScanResultIndCallback callbackRoutine, void *callbackContext
)
{
    tpSirTriggerBatchScanResultInd pRequestBuf;
    vos_msg_t msg;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pRequestBuf = vos_mem_malloc(sizeof(tSirTriggerBatchScanResultInd));
    if (NULL == pRequestBuf)
    {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
        "%s: Not able to allocate memory for WDA_TRIGGER_BATCH_SCAN_RESULT_IND",
        __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    /*HDD callback to be called after getting batch scan result ind from FW*/
    pMac->pmc.batchScanResultCallback = callbackRoutine;
    pMac->pmc.batchScanResultCallbackContext = callbackContext;

    pRequestBuf->param = pRequest->param;

    msg.type     = WDA_TRIGGER_BATCH_SCAN_RESULT_IND;
    msg.reserved = 0;
    msg.bodyptr  = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
          "%s: Not able to post WDA_TRIGGER_BATCH_SCAN_RESULT_IND message"
          " to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* -----------------------------------------------------------------------------
    \fn pmcStopBatchScanInd
    \brief  Stoping batch scan request in FW
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine - Pointer to stop batch scan request callback routine
    \return eHalStatus
             eHAL_STATUS_FAILURE  Cannot set batch scan request
             eHAL_STATUS_SUCCESS  Request accepted.
 -----------------------------------------------------------------------------*/

eHalStatus pmcStopBatchScanInd(tHalHandle hHal, tSirStopBatchScanInd *pRequest,
    tANI_U8 sessionId)
{
    tSirStopBatchScanInd *pRequestBuf;
    vos_msg_t msg;

    pRequestBuf = vos_mem_malloc(sizeof(tSirStopBatchScanInd));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
          "%s: Not able to allocate memory for STOP BATCH SCAN IND", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    pRequestBuf->param = pRequest->param;

    msg.type     = WDA_STOP_BATCH_SCAN_IND;
    msg.reserved = 0;
    msg.bodyptr  = pRequestBuf;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
          "%s: Not able to post WDA_TOP_BATCH_SCAN_IND message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

#endif
