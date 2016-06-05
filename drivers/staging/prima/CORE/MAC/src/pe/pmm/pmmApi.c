/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/*
 * This file pmmApi.cc contains functions related to the API exposed
 * by power management module
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "palTypes.h"
#include "wniCfg.h"

#include "sirCommon.h"
#include "aniGlobal.h"

#include "schApi.h"
#include "limApi.h"
#include "limSendMessages.h"
#include "cfgApi.h"
#include "limSessionUtils.h"
#include "limFT.h"


#include "pmmApi.h"
#include "pmmDebug.h"
#include "sirApi.h"
#include "wmmApsd.h"

#include "limSendSmeRspMessages.h"
#include "limTimerUtils.h"
#include "limTrace.h"
#include "limUtils.h"
#include "VossWrapper.h"
#ifdef INTEGRATION_READY
#include "vos_status.h" //VOS_STATUS
#include "vos_mq.h"     //vos_mq_post_message()
#endif

#include "wlan_qct_wda.h"

#define LIM_ADMIT_MASK_FLAG_ACBE 1
#define LIM_ADMIT_MASK_FLAG_ACBK 2
#define LIM_ADMIT_MASK_FLAG_ACVI 4
#define LIM_ADMIT_MASK_FLAG_ACVO 8

// --------------------------------------------------------------------
/**
 * pmmInitialize
 *
 * FUNCTION:
 * Initialize PMM module
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param mode
 * @param rate
 * @return None
 */

tSirRetStatus
pmmInitialize(tpAniSirGlobal pMac)
{


    pmmResetStats(pMac);

    pMac->pmm.gPmmBeaconInterval = WNI_CFG_BEACON_INTERVAL_STADEF;
    pMac->pmm.gPmmState = ePMM_STATE_READY;



    pMac->pmm.inMissedBeaconScenario = FALSE;

    return eSIR_SUCCESS;
}

// --------------------------------------------------------------------
/**
 * pmmResetStats
 *
 * FUNCTION:
 * Resets the statistics
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac
 *
 * @return None
 */

void
pmmResetStats(void *pvMac)
{
    tpAniSirGlobal pMac = (tpAniSirGlobal)pvMac;

    pMac->pmm.BmpsmaxSleepTime = 0;
    pMac->pmm.BmpsavgSleepTime = 0;
    pMac->pmm.BmpsminSleepTime = 0;
    pMac->pmm.BmpscntSleep = 0;

    pMac->pmm.BmpsmaxTimeAwake = 0;
    pMac->pmm.BmpsavgTimeAwake = 0;
    pMac->pmm.BmpsminTimeAwake = 0;
    pMac->pmm.BmpscntAwake = 0;

    pMac->pmm.BmpsWakeupTimeStamp = 0;
    pMac->pmm.BmpsSleepTimeStamp = 0;

    pMac->pmm.BmpsHalReqFailCnt = 0;
    pMac->pmm.BmpsInitFailCnt = 0;
    pMac->pmm.BmpsInitFailCnt= 0;
    pMac->pmm.BmpsInvStateCnt= 0;
    pMac->pmm.BmpsPktDrpInSleepMode= 0;
    pMac->pmm.BmpsReqInInvalidRoleCnt= 0;
    pMac->pmm.BmpsSleeReqFailCnt= 0;
    pMac->pmm.BmpsWakeupIndCnt= 0;

    pMac->pmm.ImpsWakeupTimeStamp = 0;
    pMac->pmm.ImpsSleepTimeStamp = 0;
    pMac->pmm.ImpsMaxTimeAwake = 0;
    pMac->pmm.ImpsMinTimeAwake = 0;
    pMac->pmm.ImpsAvgTimeAwake = 0;
    pMac->pmm.ImpsCntAwake = 0;

    pMac->pmm.ImpsCntSleep = 0;
    pMac->pmm.ImpsMaxSleepTime = 0;
    pMac->pmm.ImpsMinSleepTime = 0;
    pMac->pmm.ImpsAvgSleepTime = 0;

    pMac->pmm.ImpsSleepErrCnt = 0;
    pMac->pmm.ImpsWakeupErrCnt = 0;
    pMac->pmm.ImpsLastErr = 0;
    pMac->pmm.ImpsInvalidStateCnt = 0;

    return;
}



// --------------------------------------------------------------------
/**
 * pmmInitBmpsResponseHandler
 *
 * FUNCTION:
 * This function processes the SIR_HAL_ENTER_BMPS_RSP from HAL.
 * If the response is successful, it puts PMM in ePMM_STATE_BMP_SLEEP state
 * and sends back success response to PMC.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 * @return None
 */

void pmmInitBmpsResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg )
{


    tPmmState nextState = pMac->pmm.gPmmState;
    tSirResultCodes retStatus = eSIR_SME_SUCCESS;
    tpPESession     psessionEntry;
    tpEnterBmpsParams pEnterBmpsParams;

    /* we need to process all the deferred messages enqueued since
     * the initiating the SIR_HAL_ENTER_BMPS_REQ.
    */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    if(pMac->pmm.gPmmState != ePMM_STATE_BMPS_WT_INIT_RSP)
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmBmps: Received 'InitPwrSaveRsp' while in incorrect state: %d"),
            pMac->pmm.gPmmState);)

        retStatus = eSIR_SME_INVALID_PMM_STATE;
        pmmBmpsUpdateInvalidStateCnt(pMac);
        goto failure;
    }

    if (NULL == limMsg->bodyptr)
    {
        PELOGE(pmmLog(pMac, LOGE, FL("pmmBmps: Received SIR_HAL_ENTER_BMPS_RSP with NULL "));)
        nextState = ePMM_STATE_READY;
        retStatus = eSIR_SME_BMPS_REQ_FAILED;
        goto failure;
    }
    pEnterBmpsParams = (tpEnterBmpsParams)(limMsg->bodyptr);

    //if response is success, then set PMM to BMPS_SLEEP state and send response back to PMC.
    //If response is failure, then send the response back to PMC and reset its state.
    if(pEnterBmpsParams->status == eHAL_STATUS_SUCCESS)
    {
        pmmLog(pMac, LOG1,
            FL("pmmBmps: Received successful response from HAL to enter BMPS_POWER_SAVE "));

        pMac->pmm.gPmmState = ePMM_STATE_BMPS_SLEEP;

        // Update sleep statistics
        pmmUpdatePwrSaveStats(pMac);

        // Disable background scan mode
        pMac->sys.gSysEnableScanMode = false;

        if (pMac->lim.gLimTimersCreated)
        {
            /* Disable heartbeat timer as well */
            if(pMac->lim.limTimers.gLimHeartBeatTimer.pMac)
            {
                MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, NO_SESSION, eLIM_HEART_BEAT_TIMER));
                tx_timer_deactivate(&pMac->lim.limTimers.gLimHeartBeatTimer);
            }
        }
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_BMPS_RSP,  retStatus, 0, 0);
    }
    else
    {
        //if init req failed, then go back to WAKEUP state.
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmBmps: BMPS_INIT_PWR_SAVE_REQ failed, informing SME"));)

        pmmBmpsUpdateInitFailureCnt(pMac);
        nextState = ePMM_STATE_READY;
        retStatus = eSIR_SME_BMPS_REQ_FAILED;
        goto failure;
    }
    return;

failure:
    psessionEntry = peGetValidPowerSaveSession(pMac);
    if(psessionEntry != NULL)
    {
       if (pMac->lim.gLimTimersCreated && pMac->lim.limTimers.gLimHeartBeatTimer.pMac)
       {
           if(VOS_TRUE != tx_timer_running(&pMac->lim.limTimers.gLimHeartBeatTimer))
           {
               PELOGE(pmmLog(pMac, LOGE, FL("Unexpected heartbeat timer not running"));)
               limReactivateHeartBeatTimer(pMac, psessionEntry);
           }
       }
    }

    //Generate an error response back to PMC
    pMac->pmm.gPmmState = nextState;
    pmmBmpsUpdateSleepReqFailureCnt(pMac);
    limSendSmeRsp(pMac, eWNI_PMC_ENTER_BMPS_RSP, retStatus, 0, 0);
    return;

}

// --------------------------------------------------------------------
/**
 * pmmExitBmpsRequestHandler
 *
 * FUNCTION:
 * This function will send the wakeup message to HAL
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac  pointer to Global Mac structure.

 * @return None
 */

void pmmExitBmpsRequestHandler(tpAniSirGlobal pMac, tpExitBmpsInfo pExitBmpsInfo)
{
    tSirResultCodes respStatus = eSIR_SME_SUCCESS;

    tPmmState origState = pMac->pmm.gPmmState;

    if (NULL == pExitBmpsInfo)
    {
        respStatus = eSIR_SME_BMPS_REQ_REJECT;
        PELOGW(pmmLog(pMac, LOGW, FL("pmmBmps: Rcvd EXIT_BMPS with NULL body"));)
        goto failure;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_BMPS_REQ_EVENT,
                       peGetValidPowerSaveSession(pMac), 0,
                       (tANI_U16)pExitBmpsInfo->exitBmpsReason);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /* PMC is not aware of Background scan, which is done in
     * BMPS mode while Nth Beacon is delivered. Essentially, PMC
     * can request the device to get out of power-save while
     * background scanning is happening. since, the device is already
     * out of powersave, just inform that device is out of powersave
     */
    if(limIsSystemInScanState(pMac))
    {
        PELOGW(pmmLog(pMac, LOGW, 
            FL("pmmBmps: Device is already awake and scanning, returning success to PMC "));)
        limSendSmeRsp(pMac, eWNI_PMC_EXIT_BMPS_RSP, respStatus, 0, 0);
        return;
    }

    /* send wakeup request, only when in sleep state */
    PELOGW(pmmLog(pMac, LOGW, FL("pmmBmps: Sending eWNI_PMC_EXIT_BMPS_REQ to HAL"));)

    if ((pMac->pmm.gPmmState == ePMM_STATE_BMPS_SLEEP) ||
         (pMac->pmm.gPmmState == ePMM_STATE_UAPSD_SLEEP))
    {
        /* Store the reason code for exiting BMPS. This value will be
         * checked when PMM receives SIR_HAL_EXIT_BMPS_RSP from HAL
         */
        pMac->pmm.gPmmExitBmpsReasonCode = pExitBmpsInfo->exitBmpsReason;
        vos_mem_free(pExitBmpsInfo);

        PELOGW(pmmLog(pMac, LOGW, 
            FL("pmmBmps: Rcvd EXIT_BMPS with reason code%d "), pMac->pmm.gPmmExitBmpsReasonCode);)


        // Set PMM to BMPS_WT_WAKEUP_RSP state
        pMac->pmm.gPmmState = ePMM_STATE_BMPS_WT_WAKEUP_RSP;
        if(pmmSendChangePowerSaveMsg(pMac) !=  eSIR_SUCCESS)
        {
            /* Wakeup request failed */
            respStatus = eSIR_SME_BMPS_REQ_REJECT;
            pmmBmpsUpdateHalReqFailureCnt(pMac);
            goto failure;
        }
        else
        {
            pmmLog(pMac, LOG1,
                          FL("pmmBmps: eWNI_PMC_EXIT_BMPS_REQ was successfully sent to HAL"));
        }
    }
    else
    {
        PELOGE(pmmLog(pMac, LOGE, 
                      FL("pmmBmps: eWNI_PMC_EXIT_BMPS_REQ received in invalid state: %d"),
            pMac->pmm.gPmmState );)

        respStatus = eSIR_SME_INVALID_PMM_STATE;
        pmmBmpsUpdateInvalidStateCnt(pMac);
        vos_mem_free(pExitBmpsInfo);
        goto failure;
    }
    return;

failure:
    pMac->pmm.gPmmState = origState;
    pmmBmpsUpdateWakeupReqFailureCnt(pMac);
    limSendSmeRsp(pMac, eWNI_PMC_EXIT_BMPS_RSP, respStatus, 0, 0);
}


// --------------------------------------------------------------------
/**
 * pmmInitBmpsPwrSave
 *
 * FUNCTION:
 * This function process the eWNI_PMC_ENTER_PMC_REQ from PMC.
 * It checks for certain conditions before it puts PMM into
 * BMPS power save state: ePMM_STATE_BMPS_WT_INIT_RSP
 * It also invokes pmmSendInitPowerSaveMsg() to send ENTER_BMPS_REQ
 * to HAL.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param mode can be either 0(sleep mode) or 1 (active mode)
 * @param pMac  pointer to Global Mac structure.

 * @return None
 */


void pmmInitBmpsPwrSave(tpAniSirGlobal pMac)
{
    tSirRetStatus   retStatus = eSIR_SUCCESS;
    tSirResultCodes respStatus = eSIR_SME_SUCCESS;
    tpPESession     psessionEntry;

    tPmmState origState = pMac->pmm.gPmmState;

    if((psessionEntry = peGetValidPowerSaveSession(pMac))== NULL)
    {
        respStatus = eSIR_SME_BMPS_REQ_REJECT;
        goto failure;
    }
#ifndef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
    // sending beacon filtering information down to HAL
    if (limSendBeaconFilterInfo(pMac, psessionEntry) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGE, FL("Fail to send Beacon Filter Info "));
    }
#else
    if(!IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
    {
        if (limSendBeaconFilterInfo(pMac, psessionEntry) != eSIR_SUCCESS)
        {
            pmmLog(pMac, LOGE, FL("Fail to send Beacon Filter Info "));
        }
    }
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_BMPS_REQ_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if ( ((pMac->pmm.gPmmState != ePMM_STATE_READY) &&
         (pMac->pmm.gPmmState != ePMM_STATE_BMPS_WAKEUP)) ||
         limIsSystemInScanState(pMac) ||
         limIsChanSwitchRunning(pMac) ||
         limIsInQuietDuration(pMac) )
    {
        PELOGE(pmmLog(pMac, LOGE, 
            FL("pmmBmps: BMPS Request received in invalid state PMM=%d, SME=%d, rejecting the initpwrsave request"),
            pMac->pmm.gPmmState, pMac->lim.gLimSmeState);)

        respStatus = eSIR_SME_INVALID_PMM_STATE;
        pmmBmpsUpdateInvalidStateCnt(pMac);
        goto failure;
    }

    //If we are in a missed beacon scenario, we should not be attempting to enter BMPS as heartbeat probe is going on
    if(pMac->pmm.inMissedBeaconScenario)
    {
       if (pMac->lim.gLimTimersCreated && pMac->lim.limTimers.gLimHeartBeatTimer.pMac)
       {
           if(VOS_TRUE != tx_timer_running(&pMac->lim.limTimers.gLimHeartBeatTimer))
           {
               PELOGE(pmmLog(pMac, LOGE, FL("Unexpected heartbeat timer not running"));)
               limReactivateHeartBeatTimer(pMac, psessionEntry);
           }
       }
       respStatus = eSIR_SME_BMPS_REQ_REJECT;
       goto failure;
    }

    /* At this point, device is associated and PMM is not in BMPS_SLEEP state. 
     * Heartbeat timer not running is an indication that PE have detected a
     * loss of link. In this case, reject BMPS request. 
     */
     /* TODO : We need to have a better check. This check is not valid */
#if 0     
    if ( (pMac->sys.gSysEnableLinkMonitorMode) && (pMac->lim.limTimers.gLimHeartBeatTimer.pMac) )
    {
        if(VOS_TRUE != tx_timer_running(&pMac->lim.limTimers.gLimHeartBeatTimer)) 
        {
            PELOGE(pmmLog(pMac, LOGE, 
                FL("Reject BMPS_REQ because HeartBeatTimer is not running. "));)
            respStatus = eSIR_SME_BMPS_REQ_FAILED;
            goto failure;
        }
    }
#endif

    //If the following function returns SUCCESS, then PMM will wait for an explicit
    //response message from softmac.

    //changing PMM state before posting message to HAL, as this is a synchronous call to HAL
    pMac->pmm.gPmmState = ePMM_STATE_BMPS_WT_INIT_RSP;
    if((retStatus = pmmSendInitPowerSaveMsg(pMac,psessionEntry)) != eSIR_SUCCESS)
    {
        PELOGE(pmmLog(pMac, LOGE, 
            FL("pmmBmps: Init Power Save Request Failed: Sending Response: %d"),
            retStatus);)

        respStatus = eSIR_SME_BMPS_REQ_REJECT;
        pmmBmpsUpdateHalReqFailureCnt(pMac);
        goto failure;
    }
    //Update the powerSave sessionId
    pMac->pmm.sessionId = psessionEntry->peSessionId;
    return;

failure:

    // Change the state back to original state
    pMac->pmm.gPmmState =origState;
    limSendSmeRsp(pMac, eWNI_PMC_ENTER_BMPS_RSP, respStatus, 0, 0);

    // update the BMPS pwr save Error Stats
    pmmBmpsUpdateSleepReqFailureCnt(pMac);
    return;
}


/**
 * pmmSendChangePowerSaveMsg()
 *
 *FUNCTION:
 * This function is called to send SIR_HAL_EXIT_BMPS_REQ to HAL.
 * This message will trigger HAL to program HW to wake up.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac  pointer to Global Mac structure.
 * @return success if message send is ok, else false.
 */
tSirRetStatus pmmSendChangePowerSaveMsg(tpAniSirGlobal pMac)
{
    tSirRetStatus  retStatus = eSIR_SUCCESS;
    tpExitBmpsParams  pExitBmpsParams;
    tSirMsgQ msgQ;
    tpPESession psessionEntry;
    tANI_U8  currentOperatingChannel = limGetCurrentOperatingChannel(pMac);

    pExitBmpsParams = vos_mem_malloc(sizeof(*pExitBmpsParams));
    if ( NULL == pExitBmpsParams )
    {
        pmmLog(pMac, LOGW, FL("Failed to allocate memory"));
        retStatus = eSIR_MEM_ALLOC_FAILED;
        return retStatus;
    }

    if((psessionEntry = peGetValidPowerSaveSession(pMac)) == NULL )
    {
        retStatus = eSIR_FAILURE;
        vos_mem_free(pExitBmpsParams);
        return retStatus;
    }

    vos_mem_set( (tANI_U8 *)pExitBmpsParams, sizeof(*pExitBmpsParams), 0);
    msgQ.type = WDA_EXIT_BMPS_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pExitBmpsParams;
    msgQ.bodyval = 0;

    /* If reason for full power is disconnecting (ie. link is
     * disconnected) or becasue of channel switch or full power requested 
     * because of beacon miss and connected on DFS channel 
     * then we should not send data null.
     * For all other reason code, send data null.
     */
    if ( !(SIR_IS_FULL_POWER_REASON_DISCONNECTED(pMac->pmm.gPmmExitBmpsReasonCode) ||
          ( (eSME_MISSED_BEACON_IND_RCVD == pMac->pmm.gPmmExitBmpsReasonCode) && 
             limIsconnectedOnDFSChannel(currentOperatingChannel))))
        pExitBmpsParams->sendDataNull = 1;

    pExitBmpsParams->bssIdx = psessionEntry->bssIdx;
   
    /* we need to defer any incoming messages until we
     * get a WDA_EXIT_BMPS_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, msgQ.type));
    retStatus = wdaPostCtrlMsg( pMac, &msgQ);
    if( eSIR_SUCCESS != retStatus )
    {
        PELOGE(pmmLog( pMac, LOGE, FL("Sending WDA_EXIT_BMPS_REQ failed, reason=%X "), retStatus );)
        vos_mem_free(pExitBmpsParams);
        return retStatus;
    }

    pmmLog(pMac, LOG1, FL("WDA_EXIT_BMPS_REQ has been successfully sent to HAL"));
    return retStatus;
}


/**
 * pmmSendInitPowerSaveMsg()
 *
 *FUNCTION:
 * This function is called to send ENTER_BMPS_REQ message to HAL.
 * This message is sent to intialize the process of bringing the
 * station into power save state.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac  pointer to Global Mac structure.
 * @param mode The Power Save or Active State
 *
 * @return success if message send is ok, else false.
 */

tSirRetStatus  pmmSendInitPowerSaveMsg(tpAniSirGlobal pMac,tpPESession psessionEntry)
{
    tSirRetStatus   retCode = eSIR_SUCCESS;
    tSirMsgQ msgQ;
    tpEnterBmpsParams pBmpsParams = NULL;
    tANI_U32    rssiFilterPeriod = 5;
    tANI_U32    numBeaconPerRssiAverage = 20;
    tANI_U32    bRssiFilterEnable = FALSE;

    if(psessionEntry->currentBssBeaconCnt == 0 )
    {
        PELOGE(pmmLog( pMac, LOGE,  FL("Beacon count is zero, can not retrieve the TSF, failing the Enter Bmps Request"));)
        return eSIR_FAILURE;
    }

    pBmpsParams = vos_mem_malloc(sizeof(tEnterBmpsParams));
    if ( NULL == pBmpsParams )
    {
        pmmLog(pMac, LOGP, "PMM: Not able to allocate memory for Enter Bmps");
        return eSIR_FAILURE;
    }

    pMac->pmm.inMissedBeaconScenario = FALSE;
    pBmpsParams->respReqd = TRUE;

    pBmpsParams->tbtt = psessionEntry->lastBeaconTimeStamp;
    pBmpsParams->dtimCount = psessionEntry->lastBeaconDtimCount;
    pBmpsParams->dtimPeriod = psessionEntry->lastBeaconDtimPeriod;
    pBmpsParams->bssIdx = psessionEntry->bssIdx;

    /* TODO: Config parameters (Rssi filter period, FW RSSI Monitoring
       and Number of beacons per RSSI average) values sent down to FW during
       initial exchange (driver load) is same as ENTER_BMPS_REQ.
       Sending these values again in ENTER_BMPS_REQ is not required
       (can be removed). This is kept as-is for now to support
       backward compatibility with the older host running on new FW. */

    if(wlan_cfgGetInt(pMac, WNI_CFG_RSSI_FILTER_PERIOD, &rssiFilterPeriod) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for Rssi filter period"));
    pBmpsParams->rssiFilterPeriod = (tANI_U8)rssiFilterPeriod;

    if(wlan_cfgGetInt(pMac, WNI_CFG_PS_ENABLE_RSSI_MONITOR, &bRssiFilterEnable) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for Rssi monitor enable flag"));
    pBmpsParams->bRssiFilterEnable = bRssiFilterEnable;

    /* The numBeaconPerRssiAverage should be less than
       the max allowed (default set to 20 in CFG) */
    if(wlan_cfgGetInt(pMac, WNI_CFG_NUM_BEACON_PER_RSSI_AVERAGE, &numBeaconPerRssiAverage) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for num beacon per rssi"));

    pBmpsParams->numBeaconPerRssiAverage =
            (tANI_U8)GET_MIN_VALUE((tANI_U8) numBeaconPerRssiAverage, WNI_CFG_NUM_BEACON_PER_RSSI_AVERAGE_STAMAX);

    pmmLog (pMac, LOG1,
        "%s: RssiFilterInfo..%d %x %x", __func__, (int)pBmpsParams->bRssiFilterEnable,
        (unsigned int)pBmpsParams->rssiFilterPeriod, (unsigned int)pBmpsParams->numBeaconPerRssiAverage);

    msgQ.type = WDA_ENTER_BMPS_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pBmpsParams;
    msgQ.bodyval = 0;

    pmmLog( pMac, LOG1,
        FL( "pmmBmps: Sending WDA_ENTER_BMPS_REQ" ));

    /* we need to defer any incoming messages until we get a
     * WDA_ENTER_BMPS_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));
    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        vos_mem_free(pBmpsParams);
        PELOGE(pmmLog( pMac, LOGE,
            FL("Posting WDA_ENTER_BMPS_REQ to HAL failed, reason=%X"),
            retCode );)
    }

    return retCode;
}

/**
 * pmmSendPowerSaveCfg()
 *
 *FUNCTION:
 * This function is called to send power save configurtion.
 *
 *NOTE:
 *
 * @param pMac  pointer to Global Mac structure.
 * @param mode The Power Save or Active State
 *
 * @return success if message send is ok, else false.
 */
tSirRetStatus pmmSendPowerSaveCfg(tpAniSirGlobal pMac, tpSirPowerSaveCfg pUpdatedPwrSaveCfg)
{
    tSirRetStatus   retCode = eSIR_SUCCESS;
    tSirMsgQ    msgQ;
    tANI_U32    listenInterval;
    tANI_U32    HeartBeatCount = 1;
    tANI_U32    maxPsPoll;
    tANI_U32    numBeaconPerRssiAverage;
    tANI_U32    minRssiThreshold;
    tANI_U32    nthBeaconFilter;
    tANI_U32    broadcastFrameFilter;
    tANI_U32    rssiFilterPeriod;
    tANI_U32    ignoreDtim;

    if (NULL == pUpdatedPwrSaveCfg)
        goto returnFailure;

    if(pMac->lim.gLimSmeState != eLIM_SME_IDLE_STATE  )
    {
        pmmLog(pMac, LOGE,
            FL("pmmCfg: Power Save Configuration received in invalid global sme state %d"),
            pMac->lim.gLimSmeState);
        retCode = eSIR_SME_INVALID_STATE;
        goto returnFailure;
    }

    // Get power save configuration CFG values
    if(wlan_cfgGetInt(pMac, WNI_CFG_LISTEN_INTERVAL, &listenInterval) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for listen interval"));
    pUpdatedPwrSaveCfg->listenInterval = (tANI_U16)listenInterval;

    if(wlan_cfgGetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, &HeartBeatCount) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for heart beat thresh"));

    pMac->lim.gLimHeartBeatCount = HeartBeatCount;
    pUpdatedPwrSaveCfg->HeartBeatCount = HeartBeatCount;

    if(wlan_cfgGetInt(pMac, WNI_CFG_NTH_BEACON_FILTER, &nthBeaconFilter) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for Nth beacon filter"));
    pUpdatedPwrSaveCfg->nthBeaconFilter = nthBeaconFilter;

    if(wlan_cfgGetInt(pMac, WNI_CFG_MAX_PS_POLL, &maxPsPoll) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for max poll"));
    pUpdatedPwrSaveCfg->maxPsPoll = maxPsPoll;

    if(wlan_cfgGetInt(pMac, WNI_CFG_MIN_RSSI_THRESHOLD, &minRssiThreshold) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for min RSSI Threshold"));
    pUpdatedPwrSaveCfg->minRssiThreshold = minRssiThreshold;

    if(wlan_cfgGetInt(pMac, WNI_CFG_NUM_BEACON_PER_RSSI_AVERAGE, &numBeaconPerRssiAverage) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for num beacon per rssi"));
    pUpdatedPwrSaveCfg->numBeaconPerRssiAverage = (tANI_U8) numBeaconPerRssiAverage;

    if(wlan_cfgGetInt(pMac, WNI_CFG_RSSI_FILTER_PERIOD, &rssiFilterPeriod) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for Rssi filter period"));
    pUpdatedPwrSaveCfg->rssiFilterPeriod = (tANI_U8) rssiFilterPeriod;

    if(wlan_cfgGetInt(pMac, WNI_CFG_BROADCAST_FRAME_FILTER_ENABLE, &broadcastFrameFilter) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for Nth beacon filter"));
    pUpdatedPwrSaveCfg->broadcastFrameFilter = (tANI_U8) broadcastFrameFilter;

    if(wlan_cfgGetInt(pMac, WNI_CFG_IGNORE_DTIM, &ignoreDtim) != eSIR_SUCCESS)
        pmmLog(pMac, LOGP, FL("pmmCfg: cfgGet failed for ignoreDtim"));
    pUpdatedPwrSaveCfg->ignoreDtim = (tANI_U8) ignoreDtim;

    //Save a copy of the CFG in global pmm context.
    vos_mem_copy( (tANI_U8 *) &pMac->pmm.gPmmCfg,  pUpdatedPwrSaveCfg, sizeof(tSirPowerSaveCfg));


    msgQ.type = WDA_PWR_SAVE_CFG;
    msgQ.reserved = 0;
    msgQ.bodyptr = pUpdatedPwrSaveCfg;
    msgQ.bodyval = 0;

    pmmLog( pMac, LOG1, FL( "pmmBmps: Sending WDA_PWR_SAVE_CFG to HAL"));
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, msgQ.type));
    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        pmmLog( pMac, LOGP,
            FL("Posting WDA_PWR_SAVE_CFG to HAL failed, reason=%X"),
            retCode );
        goto returnFailure;
    }
    return retCode;

returnFailure:

    /* In case of failure, we need to free the memory */
    if (NULL != pUpdatedPwrSaveCfg)
    {
        vos_mem_free(pUpdatedPwrSaveCfg);
    }
    return retCode;
}

/**
 * pmmExitBmpsResponseHandler
 *
 *FUNCTION:
 * This function processes the Wakeup Rsp from HAL and if successfull,
 * sends a respnose back to PMC layer.
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac  pointer to Global Mac structure.
 * @param rspStatus Status of the response, Success or an error code.
 *
 * @return none.
 */
void pmmExitBmpsResponseHandler(tpAniSirGlobal pMac,  tpSirMsgQ limMsg)
{
    tpExitBmpsParams  pExitBmpsRsp;
    eHalStatus  rspStatus;
    tANI_U8 PowersavesessionId;
    tpPESession psessionEntry;
    tSirResultCodes retStatus = eSIR_SME_SUCCESS;

    /* Copy the power save sessionId to the local variable */
    PowersavesessionId = pMac->pmm.sessionId;

    /* we need to process all the deferred messages enqueued since
     * the initiating the SIR_HAL_EXIT_BMPS_REQ.
     */

    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    if((psessionEntry = peFindSessionBySessionId(pMac,PowersavesessionId))==NULL)
    {
        pmmLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
        return;
    }


    if (NULL == limMsg->bodyptr)
    {
        pmmLog(pMac, LOGE, FL("Received SIR_HAL_EXIT_BMPS_RSP with NULL "));
        return;
    }
    pExitBmpsRsp = (tpExitBmpsParams)(limMsg->bodyptr);
    rspStatus = pExitBmpsRsp->status;

    if(pMac->pmm.gPmmState != ePMM_STATE_BMPS_WT_WAKEUP_RSP)
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("Received SIR_HAL_EXIT_BMPS_RSP while in incorrect state: %d"),
            pMac->pmm.gPmmState);)

        retStatus = eSIR_SME_INVALID_PMM_STATE;
        pmmBmpsUpdateInvalidStateCnt(pMac);
    }
    else
    {
        PELOGW(pmmLog(pMac, LOGW, FL("Received SIR_HAL_EXIT_BMPS_RSP in correct state. "));)
    }

    /* PE is going to wakeup irrespective of whether
     * SIR_HAL_EXIT_BMPS_REQ was successful or not
     */
    switch (rspStatus)
    {
        case eHAL_STATUS_SUCCESS:
            retStatus = eSIR_SME_SUCCESS;
            pMac->pmm.gPmmState = ePMM_STATE_BMPS_WAKEUP;
            /* Update wakeup statistics */
            pmmUpdateWakeupStats(pMac);
            break;

        default:
            {
                /* PE is going to be awake irrespective of whether EXIT_BMPS_REQ
                 * failed or not. This is mainly to eliminate the dead-lock condition
                 * But, PMC will be informed about the error.
                 */
                retStatus = eSIR_SME_BMPS_REQ_FAILED;
                pMac->pmm.gPmmState = ePMM_STATE_BMPS_SLEEP;
                pmmBmpsUpdateWakeupReqFailureCnt(pMac);
            }
            break;

    }

    // turn on background scan
    pMac->sys.gSysEnableScanMode = true;

    // send response to PMC
   if(IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) )
   {
       limSendSmeRsp(pMac, eWNI_PMC_EXIT_BMPS_RSP, retStatus,
                  psessionEntry->smeSessionId, psessionEntry->transactionId);
   }
   else
   {
       limSendSmeRsp(pMac, eWNI_PMC_EXIT_BMPS_RSP, retStatus, 0, 0);
   }

    if ( pMac->pmm.gPmmExitBmpsReasonCode == eSME_MISSED_BEACON_IND_RCVD)
    {
        PELOGW(pmmLog(pMac, LOGW, FL("Rcvd SIR_HAL_EXIT_BMPS_RSP with MISSED_BEACON"));)
        pmmMissedBeaconHandler(pMac);
    }
    else if(pMac->pmm.inMissedBeaconScenario)
    {
        PELOGW(pmmLog(pMac, LOGW, FL("Rcvd SIR_HAL_EXIT_BMPS_RSP in missed beacon scenario but reason code not correct"));)
        pmmMissedBeaconHandler(pMac);
    }
    else
    {
        // Enable heartbeat timer
        limReactivateHeartBeatTimer(pMac, psessionEntry);
    }
    return;
}


/**
 * pmmMissedBeaconHandler()
 *
 *FUNCTION:
 *  This function is called when PMM receives an eWNI_PMC_EXIT_BMPS_REQ
 *  with reason code being eSME_MISSED_BEACON_IND_RCVD.
 *
 *NOTE:
 * @param pMac  pointer to Global Mac structure.
 * @return none
 */
void pmmMissedBeaconHandler(tpAniSirGlobal pMac)
{
    tANI_U8 pwrSaveSessionId;
    tANI_U32 beaconInterval = 0;
    tANI_U32 heartBeatInterval = pMac->lim.gLimHeartBeatCount;
    tpPESession psessionEntry;
    
    /* Copy the power save sessionId to the local variable */
    pwrSaveSessionId = pMac->pmm.sessionId;

    if((psessionEntry = peFindSessionBySessionId(pMac,pwrSaveSessionId))==NULL)
    {
        pmmLog(pMac, LOGE,FL("Session Does not exist for given sessionID"));
        return;
    }


    PELOGE(pmmLog(pMac, LOG1, FL("The device woke up due to MISSED BEACON "));)

    /* Proceed only if HeartBeat timer is created */
    if((pMac->lim.limTimers.gLimHeartBeatTimer.pMac) &&
       (pMac->lim.gLimTimersCreated))
    {
        if (wlan_cfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL, &beaconInterval) != eSIR_SUCCESS)
            pmmLog(pMac, LOG1, FL("Fail to get BEACON_INTERVAL value"));

        /* Change timer to reactivate it in future */
        heartBeatInterval= SYS_MS_TO_TICKS(beaconInterval * heartBeatInterval);

        if( tx_timer_change(&pMac->lim.limTimers.gLimHeartBeatTimer,
                            (tANI_U32)heartBeatInterval, 0) != TX_SUCCESS)
        {
            pmmLog(pMac, LOG1, FL("Fail to change HeartBeat timer"));
        }

        /* update some statistics */
        if(LIM_IS_CONNECTION_ACTIVE(psessionEntry))
        {
            if(psessionEntry->LimRxedBeaconCntDuringHB < MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL)
                pMac->lim.gLimHeartBeatBeaconStats[psessionEntry->LimRxedBeaconCntDuringHB]++;
            else
                pMac->lim.gLimHeartBeatBeaconStats[0]++;
        }

        /* To handle the missed beacon failure, message is being posted to self as if the
         * actual timer has expired. This is done to make sure that there exists one
         * common entry and exit points
         */
        //limResetHBPktCount(pMac); // 090805: Where did this come from?
        limResetHBPktCount(psessionEntry); // 090805: This is what it SHOULD be.  If we even need it.
        pmmSendMessageToLim(pMac, SIR_LIM_HEART_BEAT_TIMEOUT);
    }
    else
    {
        PELOGE(pmmLog(pMac, LOGE, FL("HeartBeat Timer is not created, cannot re-activate"));)
    }

    return;
}


/**
 * pmmExitBmpsIndicationHandler
 *
 *FUNCTION:
 * This function sends a Power Save Indication. back to PMC layer.
 * This indication is originated from softmac and will occur in the following two
 * scenarios:
 * 1) When softmac is in sleep state and wakes up to parse TIM and finds that
 *     AP has the data pending for this STA, then it sends this indication to let PMC know
 *    that it is going to be awake and pass the control over to PMC
 * 2) When softmac is in sleep state and wakes up to parse TIM and determines that
 *     current TIM is DTIM and AP has buffered broadcast/multicast frames.
 *    In this scenario, softmac needs to remain awake for broadcast/multicast frames and it
 *    sends an indication to PMC that it is awake and passes the control over to PMC.
 * 3) If station is awake and 'fEnablePwrSaveImmediately' flag is set, then softmac will transmit all
 *     frames in its queues and go to sleep. Before going to sleep it sends the notification to PMC that
 *     it is going to sleep.
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac  pointer to Global Mac structure.
 * @param rspStatus Status of the response, Success or an error code.
 *
 * @return none.
 */

void pmmExitBmpsIndicationHandler(tpAniSirGlobal pMac, tANI_U8 mode, eHalStatus rspStatus)
{

    tANI_U32 beaconInterval = 0;
    tANI_U32 heartBeatInterval = pMac->lim.gLimHeartBeatCount;
    tANI_U8  powersavesessionId;
    tpPESession psessionEntry;

    /* Copy the power save sessionId to the local variable */
    powersavesessionId = pMac->pmm.sessionId;

    psessionEntry = peFindSessionBySessionId(pMac,powersavesessionId);

    if(psessionEntry == NULL)
    {
      PELOGE(pmmLog(pMac, LOGE,
             FL("Session does Not exist with given sessionId :%d "),powersavesessionId);)
      return;
    }

    /* Since, the hardware is already wokenup, PE also wakesup and informs
     * the upper layers that the system is waking up. Hence always Success is
     * sent in the reason code for the message sent to PMC
     */

    PELOGW(pmmLog(pMac, LOGW, 
           FL("pmmBmps: Received SIR_HAL_EXIT_BMPS_IND from HAL, Exiting BMPS sleep mode")); )


    pMac->pmm.gPmmState = ePMM_STATE_BMPS_WAKEUP;
    pmmUpdateWakeupStats(pMac);

    /* turn on background scan */
    pMac->sys.gSysEnableScanMode = true;

    pmmBmpsUpdateWakeupIndCnt(pMac);

    /* Inform SME about the system awake state */
    limSendSmeRsp(pMac,
                  eWNI_PMC_EXIT_BMPS_IND,
                  eSIR_SME_SUCCESS, 0, 0);

    switch(rspStatus)
    {

        /* The SoftMAC sends wakeup indication even when Heart-Beat timer expired
         * The PE should start taking action against this as soon as it identifies
         * that the SoftMAC has identified heart-beat miss
         */
        case eHAL_STATUS_HEARTBEAT_TMOUT:
            {
                pmmLog(pMac, LOG1,
                              FL("pmmBmps: The device woke up due to HeartBeat Timeout"));

                /* Proceed only if HeartBeat timer is created */
                if((pMac->lim.limTimers.gLimHeartBeatTimer.pMac) &&
                   (pMac->lim.gLimTimersCreated))
                {

                    /* Read the beacon interval from sessionTable */
                    beaconInterval = psessionEntry->beaconParams.beaconInterval;

                    /* Change timer to reactivate it in future */
                    heartBeatInterval= SYS_MS_TO_TICKS(beaconInterval * heartBeatInterval);

                    if(tx_timer_change(&pMac->lim.limTimers.gLimHeartBeatTimer,
                                       (tANI_U32)heartBeatInterval, 0) != TX_SUCCESS)
                    {
                        pmmLog(pMac, LOG1,
                               FL("pmmBmps: Unable to change HeartBeat timer"));
                    }

                    /* update some statistics */
                    if(LIM_IS_CONNECTION_ACTIVE(psessionEntry))
                    {
                        if(psessionEntry->LimRxedBeaconCntDuringHB < MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL)
                            pMac->lim.gLimHeartBeatBeaconStats[psessionEntry->LimRxedBeaconCntDuringHB]++;
                        else
                            pMac->lim.gLimHeartBeatBeaconStats[0]++;
                    }

                    /* To handle the heartbeat failure, message is being posted to self as if the
                     * actual timer has expired. This is done to make sure that there exists one
                     * common entry and exit points
                     */
                    pmmSendMessageToLim(pMac, SIR_LIM_HEART_BEAT_TIMEOUT);

                }
                else
                {

                    PELOGE(pmmLog(pMac, LOGE, 
                           FL("pmmBmps: HeartBeat Timer is not created, cannot re-activate"));)
                }
            }
            break;

        case eHAL_STATUS_NTH_BEACON_DELIVERY:
            break;

        default:
            break;

    }

    return;

}


// --------------------------------------------------------------------
/**
 * pmmProcessMessage
 *
 * FUNCTION:  Processes the next received Power Management message
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

void pmmProcessMessage(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    switch (pMsg->type)
    {
        case eWNI_PMC_PWR_SAVE_CFG:
        {
            tpSirPowerSaveCfg pPSCfg;
            tSirMbMsg *pMbMsg = (tSirMbMsg *)pMsg->bodyptr;

            pPSCfg = vos_mem_malloc(sizeof(tSirPowerSaveCfg));
            if ( NULL == pPSCfg )
            {
                pmmLog(pMac, LOGP, "PMM: Not able to allocate memory for PMC Config");
            }
            (void) vos_mem_copy(pPSCfg, pMbMsg->data, sizeof(tSirPowerSaveCfg));
            (void) pmmSendPowerSaveCfg(pMac, pPSCfg);
        }
            break;

        case eWNI_PMC_ENTER_BMPS_REQ:
            pmmInitBmpsPwrSave(pMac);
            break;

        case WDA_ENTER_BMPS_RSP:
            pmmInitBmpsResponseHandler(pMac, pMsg);
            break;

        case eWNI_PMC_EXIT_BMPS_REQ:
        {
            tpExitBmpsInfo  pExitBmpsInfo;
            tSirMbMsg      *pMbMsg = (tSirMbMsg *)pMsg->bodyptr;

            pExitBmpsInfo = vos_mem_malloc(sizeof(tExitBmpsInfo));
            if ( NULL == pExitBmpsInfo )
            {
                pmmLog(pMac, LOGP, "PMM: Failed to allocate memory for Exit BMPS Info ");
            }
            (void) vos_mem_copy(pExitBmpsInfo, pMbMsg->data, sizeof(tExitBmpsInfo));
            (void) pmmExitBmpsRequestHandler(pMac, pExitBmpsInfo);
        }
            break;

        case WDA_EXIT_BMPS_RSP:
            pmmExitBmpsResponseHandler(pMac, pMsg);
            break;

        case WDA_EXIT_BMPS_IND:
            pmmExitBmpsIndicationHandler(pMac, SIR_PM_ACTIVE_MODE, (eHalStatus)pMsg->bodyval);
            break;

        case eWNI_PMC_ENTER_IMPS_REQ:
            pmmEnterImpsRequestHandler(pMac);
            break;

        case WDA_ENTER_IMPS_RSP:
            pmmEnterImpsResponseHandler(pMac, (eHalStatus)pMsg->bodyval);
            break;

        case eWNI_PMC_EXIT_IMPS_REQ:
            pmmExitImpsRequestHandler(pMac);
            break;

        case WDA_EXIT_IMPS_RSP:
            pmmExitImpsResponseHandler(pMac, (eHalStatus)pMsg->bodyval);
            break;

        case eWNI_PMC_ENTER_UAPSD_REQ:
            pmmEnterUapsdRequestHandler(pMac);
            break;

        case WDA_ENTER_UAPSD_RSP:
            pmmEnterUapsdResponseHandler(pMac, pMsg);
            break;

        case eWNI_PMC_EXIT_UAPSD_REQ:
            pmmExitUapsdRequestHandler(pMac);
            break;

        case WDA_EXIT_UAPSD_RSP:
            pmmExitUapsdResponseHandler(pMac, pMsg);
            break;

        case eWNI_PMC_WOWL_ADD_BCAST_PTRN:
            pmmSendWowlAddBcastPtrn(pMac, pMsg);
            break;

        case eWNI_PMC_WOWL_DEL_BCAST_PTRN:
            pmmSendWowlDelBcastPtrn(pMac, pMsg);
            break;

        case eWNI_PMC_ENTER_WOWL_REQ:
            pmmEnterWowlRequestHandler(pMac, pMsg);
            break;

        case WDA_WOWL_ENTER_RSP:
            pmmEnterWowlanResponseHandler(pMac, pMsg);
            break;

        case eWNI_PMC_EXIT_WOWL_REQ:
            pmmExitWowlanRequestHandler(pMac);
            break;

        case WDA_WOWL_EXIT_RSP:
            pmmExitWowlanResponseHandler(pMac, pMsg);
            break;
#ifdef WLAN_FEATURE_PACKET_FILTERING
        case WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP:
            pmmFilterMatchCountResponseHandler(pMac, pMsg);
            break;
#endif // WLAN_FEATURE_PACKET_FILTERING


#ifdef WLAN_FEATURE_GTK_OFFLOAD
        case WDA_GTK_OFFLOAD_GETINFO_RSP:
            pmmGTKOffloadGetInfoResponseHandler(pMac, pMsg);
            break;
#endif // WLAN_FEATURE_GTK_OFFLOAD

        default:
            PELOGW(pmmLog(pMac, LOGW, 
                FL("PMM: Unknown message in pmmMsgQ type %d, potential memory leak!!"),
                pMsg->type);)
    }

    if (NULL != pMsg->bodyptr)
    {
        vos_mem_free(pMsg->bodyptr);
        pMsg->bodyptr = NULL;
    }
}






// --------------------------------------------------------------------
/**
 * pmmPostMessage
 *
 * FUNCTION:
 * Post a message to the pmm message queue
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMsg pointer to message
 * @return None
 */

tSirRetStatus
pmmPostMessage(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    VOS_STATUS vosStatus;
    vosStatus = vos_mq_post_message(VOS_MQ_ID_PE, (vos_msg_t *) pMsg);
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        pmmLog(pMac, LOGP, FL("vos_mq_post_message failed with status code %d"), vosStatus);
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
}





/**
 * pmmUpdatePwrSaveStats
 *
 * FUNCTION:  updated BMPS stats, when Station is going into power save state.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac pointer to Global MAC Structure.
 * @return None
 */

void pmmUpdatePwrSaveStats(tpAniSirGlobal pMac)
{
/*
    tANI_U64 TimeAwake = 0;

    pMac->pmm.BmpsSleepTimeStamp = vos_timer_get_system_ticks();

    if (pMac->pmm.BmpsWakeupTimeStamp)
        TimeAwake = (pMac->pmm.BmpsSleepTimeStamp - pMac->pmm.BmpsWakeupTimeStamp) /10;
    else
        TimeAwake = 0; // very first time

    if (TimeAwake > pMac->pmm.BmpsmaxTimeAwake)
    {
        pMac->pmm.BmpsmaxTimeAwake = TimeAwake;
    }

    if ((!pMac->pmm.BmpsminTimeAwake) || (TimeAwake < pMac->pmm.BmpsminTimeAwake))
    {
        pMac->pmm.BmpsminTimeAwake = TimeAwake;
    }

    pMac->pmm.BmpsavgTimeAwake = ( ( (pMac->pmm.BmpsavgTimeAwake * pMac->pmm.BmpscntSleep) + TimeAwake ) / (pMac->pmm.BmpscntSleep + 1) );

*/
    pMac->pmm.BmpscntSleep++;
    return;
}




/**
 * pmmUpdatePwrSaveStats
 *
 * FUNCTION:  updated BMPS stats, when Station is waking up.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac pointer to Global MAC Structure.
 * @return None
 */

void pmmUpdateWakeupStats(tpAniSirGlobal pMac)
{
/*

        tANI_U64 SleepTime = 0;

        pMac->pmm.BmpsWakeupTimeStamp = vos_timer_get_system_ticks();
        SleepTime = (pMac->pmm.BmpsWakeupTimeStamp - pMac->pmm.BmpsSleepTimeStamp) / 10;

        if (SleepTime > pMac->pmm.BmpsmaxSleepTime)
        {
            pMac->pmm.BmpsmaxSleepTime = SleepTime;
        }

        if ((!pMac->pmm.BmpsminSleepTime) || (SleepTime < pMac->pmm.BmpsminSleepTime))
        {
            pMac->pmm.BmpsminSleepTime = SleepTime;
        }

        pMac->pmm.BmpsavgSleepTime = ( ( (pMac->pmm.BmpsavgSleepTime * pMac->pmm.BmpscntAwake) + SleepTime ) / (pMac->pmm.BmpscntAwake + 1) );

*/
        pMac->pmm.BmpscntAwake++;
        return;
}

// --------------------------------------------------------------------
/**
 * pmmEnterImpsRequestHandler
 *
 * FUNCTION:
 * This function sends the idle mode power save request from host device
 * drive to HAL. This function is called from pmmProcessMsg()
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param   Global handle to MAC
 * @return  None
 */
void pmmEnterImpsRequestHandler (tpAniSirGlobal pMac)
{

    tSirResultCodes resultCode = eSIR_SME_SUCCESS;
    tSirRetStatus   retStatus = eSIR_SUCCESS;
    tPmmState       origState = pMac->pmm.gPmmState;

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_IMPS_REQ_EVENT, peGetValidPowerSaveSession(pMac), 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /*Returns True even single active session present */
    if(peIsAnySessionActive(pMac))
    {
        /* Print active pesession and tracedump once in every 16
         * continous error.
         */
        if (!(pMac->pmc.ImpsReqFailCnt & 0xF))
        {
            pePrintActiveSession(pMac);
            vosTraceDumpAll(pMac,0,0,100,0);
        }
        resultCode = eSIR_SME_INVALID_STATE;
        pmmLog(pMac, LOGE, FL("Session is active go to failure resultCode = "
               "eSIR_SME_INVALID_STATE (%d)"),resultCode);
        goto failure;
    }

    if ( ((pMac->pmm.gPmmState != ePMM_STATE_READY) &&
          (pMac->pmm.gPmmState != ePMM_STATE_IMPS_WAKEUP)) ||
         ((pMac->lim.gLimSmeState != eLIM_SME_IDLE_STATE) &&
          (pMac->lim.gLimSmeState != eLIM_SME_JOIN_FAILURE_STATE)) ||
         (pMac->lim.gLimMlmState != eLIM_MLM_IDLE_STATE) ||
         limIsChanSwitchRunning (pMac) ||
         limIsInQuietDuration (pMac) )
    {
        PELOGE(pmmLog(pMac, LOGE, 
              FL("pmmImps: PMM State = %d, Global MLM State = %d, Global SME State = %d, rejecting the sleep mode request"),
              pMac->pmm.gPmmState, pMac->lim.gLimMlmState, pMac->lim.gLimSmeState);)

        resultCode = eSIR_SME_INVALID_PMM_STATE;
        pmmImpsUpdateErrStateStats(pMac);
        goto failure;
    }

    // change PE state and send the request to HAL
    pMac->pmm.gPmmState = ePMM_STATE_IMPS_WT_SLEEP_RSP;
    if( (retStatus = pmmImpsSendChangePwrSaveMsg(pMac, SIR_PM_SLEEP_MODE)) != eSIR_SUCCESS)
    {
        PELOGE(pmmLog(pMac, LOGE, 
               FL("pmmImps: IMPS Sleep Request failed: sending response: %x"), retStatus);)

        resultCode = eSIR_SME_IMPS_REQ_FAILED;
        goto failure;
    }
    else
    {
        pmmLog(pMac, LOG1,
               FL("pmmImps: Waiting for SoftMac response for IMPS request"));
    }
    return;

failure:
    pMac->pmm.gPmmState = origState;
    pmmImpsUpdateSleepErrStats(pMac, retStatus);

    limSendSmeRsp(pMac,
                  eWNI_PMC_ENTER_IMPS_RSP,
                  resultCode, 0, 0);

}

// --------------------------------------------------------------------
/**
 * pmmEnterImpsResponseHandler
 *
 * FUNCTION:
 * This function receives the response from HAL layer for the idle mode
 * power save request sent. The function is also responsible for checking
 * the correctness of the system state before configuring the new state
 * on success. This function is called by pmmProcessMsg()
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param   Global handle to MAC, Status code
 * @return  None
 */
void pmmEnterImpsResponseHandler (tpAniSirGlobal pMac, eHalStatus rspStatus)
{
    tPmmState nextState = pMac->pmm.gPmmState;
    tSirResultCodes resultCode = eSIR_SME_SUCCESS;
    static int failCnt = 0;

    /* we need to process all the deferred messages enqueued since
     * the initiating the WDA_ENTER_IMPS_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    if(pMac->pmm.gPmmState != ePMM_STATE_IMPS_WT_SLEEP_RSP)
    {
        PELOGE(pmmLog(pMac, LOGE, 
               FL("pmmImps: Receives IMPS sleep rsp in invalid state: %d"),
               pMac->pmm.gPmmState);)

        resultCode = eSIR_SME_INVALID_PMM_STATE;
        pmmImpsUpdateErrStateStats(pMac);

        goto failure;
    }

    if(eHAL_STATUS_SUCCESS == rspStatus)
    {
        //if success, change the state to IMPS sleep mode
        pMac->pmm.gPmmState = ePMM_STATE_IMPS_SLEEP;

        pmmLog(pMac, LOG1,
            FL("pmmImps: Received successful WDA_ENTER_IMPS_RSP from HAL"));

        //update power save statistics
        pmmImpsUpdatePwrSaveStats(pMac);

        limSendSmeRsp(pMac,
                      eWNI_PMC_ENTER_IMPS_RSP,
                      resultCode, 0, 0);
    }
    else
    {
        // go back to previous state if request failed
        nextState = ePMM_STATE_READY;
        resultCode = eSIR_SME_CANNOT_ENTER_IMPS;
        goto failure;
    }
    return;

failure:
    if (!(failCnt & 0xF))
         PELOGE(pmmLog(pMac, LOGE,
           FL("pmmImpsSleepRsp failed, Ret Code: %d, next state will be: %d"),
           rspStatus,
           pMac->pmm.gPmmState);)

    failCnt++;
    pmmImpsUpdateSleepErrStats(pMac, rspStatus);

    pMac->pmm.gPmmState = nextState;

    limSendSmeRsp(pMac,
                  eWNI_PMC_ENTER_IMPS_RSP,
                  resultCode, 0, 0);
}


// --------------------------------------------------------------------
/**
 * pmmExitImpsRequestHandler
 *
 * FUNCTION:
 * This function is called by pmmProcessMsg(). The function sends a request
 * to HAL to wakeup the device from idle mode power save mode.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param   Global handle to MAC
 * @return  None
 */
void pmmExitImpsRequestHandler (tpAniSirGlobal pMac)
{
    tSirRetStatus retStatus = eSIR_SUCCESS;
    tSirResultCodes resultCode = eSIR_SME_SUCCESS;

    tPmmState origState = pMac->pmm.gPmmState;

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_IMPS_REQ_EVENT, peGetValidPowerSaveSession(pMac), 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if (ePMM_STATE_IMPS_SLEEP == pMac->pmm.gPmmState)
    {
        pMac->pmm.gPmmState = ePMM_STATE_IMPS_WT_WAKEUP_RSP;
        if( (retStatus = pmmImpsSendChangePwrSaveMsg(pMac, SIR_PM_ACTIVE_MODE)) !=
            eSIR_SUCCESS)
        {
            PELOGE(pmmLog(pMac, LOGE, 
                   FL("pmmImps: Wakeup request message sent to SoftMac failed"));)
            resultCode = eSIR_SME_IMPS_REQ_FAILED;
            goto failure;
        }
    }
    else
    {
        // PE in invalid state 
        PELOGE(pmmLog(pMac, LOGE, 
                      FL("pmmImps: Wakeup Req received in invalid state: %d"),
                      pMac->pmm.gPmmState);)

        resultCode = eSIR_SME_INVALID_PMM_STATE;
        pmmImpsUpdateErrStateStats(pMac);

        goto failure;
    }
    return;

failure:
    PELOGE(pmmLog (pMac, LOGE, 
                   FL("pmmImps: Changing to IMPS wakeup mode failed, Ret Code: %d, Next State: %d"),
                   retStatus, pMac->pmm.gPmmState);)

    pMac->pmm.gPmmState = origState;
    pmmImpsUpdateWakeupErrStats(pMac, retStatus);

    limSendSmeRsp(pMac,
                  eWNI_PMC_EXIT_IMPS_RSP,
                  resultCode, 0, 0);
}


// --------------------------------------------------------------------
/**
 * pmmExitImpsResponseHandler
 *
 * FUNCTION:
 * This function receives the response from HAL layer for the idle mode
 * power save request sent. The function is also responsible for checking
 * the correctness of the system state before configuring the new state
 * on success. This function is called by pmmProcessMsg()
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param  Global handle to MAC
 * @return None
 */
void pmmExitImpsResponseHandler(tpAniSirGlobal pMac, eHalStatus rspStatus)
{
    tSirResultCodes resultCode = eSIR_SME_SUCCESS;

    /* we need to process all the deferred messages enqueued since
     * the initiating the WDA_EXIT_IMPS_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    if (pMac->pmm.gPmmState != ePMM_STATE_IMPS_WT_WAKEUP_RSP)
    {
        PELOGE(pmmLog(pMac, LOGE, 
                      FL("pmmImps: Received 'Wakeup' response in invalid state: %d"),
                      pMac->pmm.gPmmState);)

        resultCode = eSIR_SME_INVALID_PMM_STATE;
        pmmImpsUpdateErrStateStats(pMac);
    }

    switch(rspStatus)
    {
    case eHAL_STATUS_SUCCESS:
        {
            resultCode = eSIR_SME_SUCCESS;
            pMac->pmm.gPmmState = ePMM_STATE_IMPS_WAKEUP;
            pmmLog(pMac, LOG1,
                          FL("pmmImps: Received WDA_EXIT_IMPS_RSP with Successful response from HAL"));
            //update power save statistics
            pmmImpsUpdateWakeupStats(pMac);
        }
        break;

        default:
            {
                resultCode = eSIR_SME_IMPS_REQ_FAILED;
                /* Set the status back to IMPS SLEEP as we failed
                 * to come out of sleep
                 */
                pMac->pmm.gPmmState = ePMM_STATE_IMPS_SLEEP;
                PELOGW(pmmLog(pMac, LOGW, 
                              FL("pmmImps: Received WDA_EXIT_IMPS_RSP with Failure Status from HAL"));)
                // update th power save error stats
                pmmImpsUpdateWakeupErrStats(pMac, rspStatus);
            }
            break;

    }



    limSendSmeRsp(pMac,
                  eWNI_PMC_EXIT_IMPS_RSP,
                  resultCode, 0, 0);
    return;

}

// --------------------------------------------------------------------
/**
 * pmmEnterUapsdRequestHandler
 *
 * FUNCTION:
 * This function process the eWNI_PMC_ENTER_UAPSD_REQ from PMC,
 * checks the correctness of the system state before configuring
 * PMM to the new ePMM_STATE_UAPSD_WT_SLEEP_RSP state, and invokes
 * invokes pmmUapsdSendChangePwrSaveMsg() to send
 * WDA_ENTER_UAPSD_REQ to HAL.
 *
 * NOTE:
 *
 * @param       Global handle to MAC
 * @return      None
 */
void pmmEnterUapsdRequestHandler (tpAniSirGlobal pMac)
{
    tSirResultCodes resultCode = eSIR_SME_SUCCESS;
    tSirRetStatus   retStatus = eSIR_SUCCESS;

    tPmmState origState = pMac->pmm.gPmmState;

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_UAPSD_REQ_EVENT, peGetValidPowerSaveSession(pMac), 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if ( (pMac->pmm.gPmmState != ePMM_STATE_BMPS_SLEEP) ||
         limIsSystemInScanState(pMac) )
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmUapsd: PMM State = %d, Global MLM State = %d, Global SME State = %d, rejecting the sleep mode request"),
            pMac->pmm.gPmmState, pMac->lim.gLimMlmState, pMac->lim.gLimSmeState);)

        resultCode = eSIR_SME_INVALID_PMM_STATE;
        goto failure;
    }

    pMac->pmm.gPmmState = ePMM_STATE_UAPSD_WT_SLEEP_RSP;

    if( (retStatus = pmmUapsdSendChangePwrSaveMsg(pMac, SIR_PM_SLEEP_MODE)) != eSIR_SUCCESS)
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmUapsd: HAL_ENTER_UAPSD_REQ failed with response: %x"), retStatus);)
        if (retStatus == eSIR_PMM_INVALID_REQ)
            resultCode = eSIR_SME_UAPSD_REQ_INVALID;
        else
            resultCode = eSIR_SME_UAPSD_REQ_FAILED;
        goto failure;
    }

    pmmLog(pMac, LOG1, FL("pmmUapsd: Waiting for WDA_ENTER_UAPSD_RSP "));
    return;

failure:
    pMac->pmm.gPmmState = origState;
    limSendSmeRsp(pMac, eWNI_PMC_ENTER_UAPSD_RSP, resultCode, 0, 0);
    return;
}


// --------------------------------------------------------------------
/**
 * pmmEnterUapsdResponseHandler
 *
 * FUNCTION:
 * This function processes the SIR_HAL_ENTER_UAPSD_RSP from HAL.
 * If the response is successful, it puts PMM into ePMM_STATE_UAPSD_SLEEP
 * state and sends back success response to PMC.
 *
 * NOTE:
 *
 * @param  limMsg
 * @return None
 */
void pmmEnterUapsdResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpUapsdParams    pUapsdRspMsg;
    tSirResultCodes  retStatus = eSIR_SME_SUCCESS;

    tANI_U8 PowersavesessionId;
    tpPESession psessionEntry;

    /* we need to process all the deferred messages enqueued since
     * the initiating the SIR_HAL_ENTER_UAPSD_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    /* Copy the power save sessionId to the local variable */
    PowersavesessionId = pMac->pmm.sessionId;

    if (NULL == limMsg->bodyptr)
    {
        PELOGE(pmmLog(pMac, LOGE, FL("pmmUapsd: Received SIR_HAL_ENTER_UAPSD_RSP with NULL "));)
        return;
    }

    pUapsdRspMsg = (tpUapsdParams)(limMsg->bodyptr);

    if((psessionEntry = peFindSessionBySessionId(pMac,PowersavesessionId))==NULL)
    {
        pmmLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
        return;
    }

    if(pMac->pmm.gPmmState != ePMM_STATE_UAPSD_WT_SLEEP_RSP)
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmUapsd: Received SIR_HAL_ENTER_UAPSD_RSP while in incorrect state: %d"),
            pMac->pmm.gPmmState);)
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_UAPSD_RSP, eSIR_SME_INVALID_PMM_STATE, 0, 0);        
        return;
    }

    if(pUapsdRspMsg->status == eHAL_STATUS_SUCCESS)
    {
        PELOGW(pmmLog(pMac, LOGW,
            FL("pmmUapsd: Received successful response from HAL to enter UAPSD mode "));)
        pMac->pmm.gPmmState = ePMM_STATE_UAPSD_SLEEP;
    }
    else
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmUapsd: SIR_HAL_ENTER_UAPSD_RSP failed, informing SME"));)
        pMac->pmm.gPmmState = ePMM_STATE_BMPS_SLEEP;
        retStatus = eSIR_SME_UAPSD_REQ_FAILED;
    }

    if(IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION))
    {
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_UAPSD_RSP, retStatus, 
                        psessionEntry->smeSessionId, psessionEntry->transactionId);
    }
    else
    {
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_UAPSD_RSP, retStatus, 0, 0);
    }

    return;
}


// --------------------------------------------------------------------
/**
 * pmmExitUapsdRequestHandler
 *
 * FUNCTION:
 * This function process the eWNI_PMC_EXIT_UAPSD_REQ from PMC,
 * checks the correctness of the system state before configuring
 * PMM to the new ePMM_STATE_UAPSD_WT_WAKEUP_RSP state, and
 * invokes pmmUapsdSendChangePwrSaveMsg() to send
 * SIR_HAL_EXIT_UAPSD_REQ to HAL.
 *
 * NOTE:
 *
 * @param       Global handle to MAC
 * @return      None
 */
void pmmExitUapsdRequestHandler(tpAniSirGlobal pMac)
{
    tSirRetStatus retStatus = eSIR_SUCCESS;
    tSirResultCodes resultCode = eSIR_SME_SUCCESS;

    tPmmState origState = pMac->pmm.gPmmState;

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_UAPSD_REQ_EVENT, peGetValidPowerSaveSession(pMac), 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if (ePMM_STATE_UAPSD_SLEEP == pMac->pmm.gPmmState)
    {
        pMac->pmm.gPmmState = ePMM_STATE_UAPSD_WT_WAKEUP_RSP;
        if( (retStatus = pmmUapsdSendChangePwrSaveMsg(pMac, SIR_PM_ACTIVE_MODE)) !=
                                                eSIR_SUCCESS)
        {
            PELOGE(pmmLog(pMac, LOGE,
                FL("pmmUapsd: sending EXIT_UAPSD to HAL failed "));)
            resultCode = eSIR_SME_UAPSD_REQ_FAILED;
            goto failure;
        }
    }
    else
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("pmmUapsd: Rcv EXIT_UAPSD from PMC in invalid state: %d"),
            pMac->pmm.gPmmState);)

        resultCode = eSIR_SME_INVALID_PMM_STATE;
        goto failure;
    }
    return;

failure:
    pMac->pmm.gPmmState = origState;
    PELOGE(pmmLog(pMac, LOGE,
        FL("pmmUapsd: Waking up from UAPSD mode failed, Ret Code: %d, Next State: %d"),
        retStatus, pMac->pmm.gPmmState);)
    limSendSmeRsp(pMac, eWNI_PMC_EXIT_UAPSD_RSP, resultCode, 0, 0);
}


// --------------------------------------------------------------------
/**
 * pmmExitUapsdResponseHandler
 *
 * FUNCTION:
 * This function receives the SIR_HAL_EXIT_UAPSD_RSP from HAL and is
 * responsible for checking the correctness of the system state
 * before configuring PMM to the new ePMM_STATE_BMPS_SLEEP state
 * and send eWNI_PMC_EXIT_UAPSD_RSP to PMC.
 *
 * NOTE:
 *
 * @param       Global handle to MAC
 * @return      None
 */
void pmmExitUapsdResponseHandler(tpAniSirGlobal pMac,  tpSirMsgQ limMsg)
{
    tSirResultCodes resultCode = eSIR_SME_SUCCESS;
    tANI_U8 PowersavesessionId;
    tpPESession psessionEntry;
    tUapsdParams  *pUapsdExitRspParams;

    /* we need to process all the deferred messages enqueued since
     * the initiating the SIR_HAL_EXIT_UAPSD_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    if (pMac->pmm.gPmmState != ePMM_STATE_UAPSD_WT_WAKEUP_RSP)
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("Received HAL_EXIT_UAPSD_RSP in invalid state: %d"),
            pMac->pmm.gPmmState);)
        limSendSmeRsp(pMac, eWNI_PMC_EXIT_UAPSD_RSP, eSIR_SME_INVALID_PMM_STATE, 0, 0);
        return;
    }
    pUapsdExitRspParams = (tUapsdParams *)(limMsg->bodyptr);

    PowersavesessionId = pMac->pmm.sessionId;
    if((psessionEntry = peFindSessionBySessionId(pMac,PowersavesessionId))==NULL)
    {
        pmmLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
        return;
    }

    if(NULL == pUapsdExitRspParams )
    {
        PELOGE(pmmLog(pMac, LOGE,
            FL("Received HAL_EXIT_UAPSD_RSP message with zero parameters:"));)
            limSendSmeRsp(pMac, eWNI_PMC_EXIT_UAPSD_RSP, eSIR_SME_UAPSD_REQ_FAILED, 0, 0);
        return;
    }
    switch(pUapsdExitRspParams->status)
    {
        case eHAL_STATUS_SUCCESS:
            resultCode = eSIR_SME_SUCCESS;
            PELOGW(pmmLog(pMac, LOGW,
                FL("Received SIR_HAL_EXIT_UAPSD_RSP with Successful response "));)
            break;
        default:
            resultCode = eSIR_SME_UAPSD_REQ_FAILED;
            PELOGE(pmmLog(pMac, LOGW,
                FL("Received SIR_HAL_EXIT_UAPSD_RSP with Failure Status"));)
            break;
    }

    pMac->pmm.gPmmState = ePMM_STATE_BMPS_SLEEP;

    if(IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION))
    {
        limSendSmeRsp(pMac, eWNI_PMC_EXIT_UAPSD_RSP, resultCode, psessionEntry->smeSessionId,
                      psessionEntry->transactionId);
    }
    else
    {
        limSendSmeRsp(pMac, eWNI_PMC_EXIT_UAPSD_RSP, resultCode, 0, 0);
    }
    return;
}

/** ------------------------------------------------------------
\fn      pmmSendWowlAddBcastPtrn
\brief   This function sends a SIR_HAL_WOWL_ADD_BCAST_PTRN
\        message to HAL.
\param   tpAniSirGlobal  pMac
\param   tpSirMsgQ       pMsg
\return  None
  --------------------------------------------------------------*/
void pmmSendWowlAddBcastPtrn(tpAniSirGlobal pMac,  tpSirMsgQ pMsg)
{
    tpSirWowlAddBcastPtrn  pBcastPtrn;
    tSirMbMsg              *pMbMsg = (tSirMbMsg *)pMsg->bodyptr;
    tSirRetStatus          retCode = eSIR_SUCCESS;
    tSirMsgQ               msgQ;

    pBcastPtrn = vos_mem_malloc(sizeof(*pBcastPtrn));
    if ( NULL == pBcastPtrn )
    {
        pmmLog(pMac, LOGP, FL("Fail to allocate memory for WoWLAN Add Bcast Pattern "));
        return;
    }
    (void) vos_mem_copy(pBcastPtrn, pMbMsg->data, sizeof(*pBcastPtrn));

    if (NULL == pBcastPtrn)
    {
        pmmLog(pMac, LOGE, FL("Add broadcast pattern message is NULL "));
        return;
    }

    msgQ.type = WDA_WOWL_ADD_BCAST_PTRN;
    msgQ.reserved = 0;
    msgQ.bodyptr = pBcastPtrn;
    msgQ.bodyval = 0;

    pmmLog(pMac, LOG1, FL( "Sending WDA_WOWL_ADD_BCAST_PTRN to HAL"));
#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_WOWL_ADD_BCAST_PTRN_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        if (pBcastPtrn != NULL)
            vos_mem_free(pBcastPtrn);
        pmmLog( pMac, LOGP, FL("Posting WDA_WOWL_ADD_BCAST_PTRN failed, reason=%X"), retCode );
    }
    return;
}

/** ------------------------------------------------------------
\fn      pmmSendWowlDelBcastPtrn
\brief   This function sends a SIR_HAL_WOWL_DEL_BCAST_PTRN
\        message to HAL.
\param   tpAniSirGlobal  pMac
\param   tpSirMsgQ       pMsg
\return  None
  --------------------------------------------------------------*/
void pmmSendWowlDelBcastPtrn(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    tpSirWowlDelBcastPtrn   pDeletePtrn;
    tSirMbMsg               *pMbMsg = (tSirMbMsg *)pMsg->bodyptr;
    tSirRetStatus           retCode = eSIR_SUCCESS;
    tSirMsgQ                msgQ;

    pDeletePtrn = vos_mem_malloc(sizeof(*pDeletePtrn));
    if ( NULL == pDeletePtrn )
    {
        pmmLog(pMac, LOGP, FL("Fail to allocate memory for WoWLAN Delete Bcast Pattern "));
        return;
    }
    (void) vos_mem_copy(pDeletePtrn, pMbMsg->data, sizeof(*pDeletePtrn));

    if (NULL == pDeletePtrn)
    {
        pmmLog(pMac, LOGE, FL("Delete broadcast pattern message is NULL "));
        return;
    }

    msgQ.type = WDA_WOWL_DEL_BCAST_PTRN;
    msgQ.reserved = 0;
    msgQ.bodyptr = pDeletePtrn;
    msgQ.bodyval = 0;

    pmmLog(pMac, LOG1, FL( "Sending WDA_WOWL_DEL_BCAST_PTRN"));
#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_WOWL_DEL_BCAST_PTRN_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        if (NULL != pDeletePtrn)
            vos_mem_free(pDeletePtrn);
        pmmLog( pMac, LOGP, FL("Posting WDA_WOWL_DEL_BCAST_PTRN failed, reason=%X"), retCode );
    }
    return;
}

/** ---------------------------------------------------------
\fn      pmmEnterWowlRequestHandler
\brief   LIM process the eWNI_PMC_ENTER_WOWL_REQ message, and
\        invokes pmmSendWowlEnterRequest() to send
\        WDA_WOWL_ENTER_REQ message to HAL.
\param   tpAniSirGlobal  pMac
\param   tpSirMsgQ       pMsg
\return  None
 ------------------------------------------------------------*/
void pmmEnterWowlRequestHandler(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    tpSirSmeWowlEnterParams  pSmeWowlParams = NULL;
    tpSirHalWowlEnterParams  pHalWowlParams = NULL;
    tSirRetStatus  retCode = eSIR_SUCCESS;
    tANI_U32  cfgValue = 0;
    tSirMbMsg *pMbMsg = (tSirMbMsg *)pMsg->bodyptr;
    tpPESession pSessionEntry = NULL;
    tANI_U8  peSessionId = 0;

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ENTER_WOWL_REQ_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    pSmeWowlParams = (tpSirSmeWowlEnterParams)(pMbMsg->data);
    if (NULL == pSmeWowlParams)
    {
        pmmLog(pMac, LOGE,
               FL("NULL message received"));
        return;
    }

    pSessionEntry = peFindSessionByBssid(pMac, pSmeWowlParams->bssId,
                                         &peSessionId);
    if (NULL == pSessionEntry)
    {
        pmmLog(pMac, LOGE,
               FL("session does not exist for given BSSId"));
        goto end;
    }
    pMac->pmm.sessionId = peSessionId;

// Need to fix it ASAP - TBH
#if 0
    if (pMac->lim.gLimSmeState != eLIM_SME_LINK_EST_STATE)
    {
        pmmLog(pMac, LOGE, FL("Rcvd PMC_ENTER_WOWL_REQ when station is not associated "));
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_WOWL_RSP, eSIR_SME_STA_NOT_ASSOCIATED, 0, 0);
        goto end;
    }
#endif


    if ((pMac->pmm.gPmmState != ePMM_STATE_BMPS_SLEEP) && (pMac->pmm.gPmmState != ePMM_STATE_WOWLAN))
    {
        pmmLog(pMac, LOGE, FL("Rcvd PMC_ENTER_WOWL_REQ in invalid Power Save state "));
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_WOWL_RSP, eSIR_SME_INVALID_PMM_STATE, 0, 0);
        goto end;
    }

    pHalWowlParams = vos_mem_malloc(sizeof(*pHalWowlParams));
    if ( NULL == pHalWowlParams )
    {
        pmmLog(pMac, LOGP, FL("Fail to allocate memory for Enter Wowl Request "));
        goto end;
    }
    (void) vos_mem_set((tANI_U8 *)pHalWowlParams, sizeof(*pHalWowlParams), 0);

    // fill in the message field
    pHalWowlParams->ucMagicPktEnable = pSmeWowlParams->ucMagicPktEnable;
    pHalWowlParams->ucPatternFilteringEnable = pSmeWowlParams->ucPatternFilteringEnable;
    (void)vos_mem_copy( (tANI_U8 *)pHalWowlParams->magicPtrn, (tANI_U8 *)pSmeWowlParams->magicPtrn,
                         sizeof(tSirMacAddr) );

#ifdef WLAN_WAKEUP_EVENTS
    pHalWowlParams->ucWoWEAPIDRequestEnable = pSmeWowlParams->ucWoWEAPIDRequestEnable;
    pHalWowlParams->ucWoWEAPOL4WayEnable = pSmeWowlParams->ucWoWEAPOL4WayEnable;
    pHalWowlParams->ucWowNetScanOffloadMatch = pSmeWowlParams->ucWowNetScanOffloadMatch;
    pHalWowlParams->ucWowGTKRekeyError = pSmeWowlParams->ucWowGTKRekeyError;
    pHalWowlParams->ucWoWBSSConnLoss = pSmeWowlParams->ucWoWBSSConnLoss;
#endif // WLAN_WAKEUP_EVENTS

    pHalWowlParams->bssIdx = pSessionEntry->bssIdx;

    if(wlan_cfgGetInt(pMac, WNI_CFG_WOWLAN_UCAST_PATTERN_FILTER_ENABLE, &cfgValue) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGP, FL("cfgGet failed for WNI_CFG_WOWLAN_UCAST_PATTERN_FILTER_ENABLE"));
        goto end;
    }
    pHalWowlParams->ucUcastPatternFilteringEnable = (tANI_U8)cfgValue;

    if(wlan_cfgGetInt(pMac, WNI_CFG_WOWLAN_CHANNEL_SWITCH_ENABLE, &cfgValue) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGP, FL("cfgGet failed for WNI_CFG_WOWLAN_CHANNEL_SWITCH_ENABLE"));
        goto end;
    }
    pHalWowlParams->ucWowChnlSwitchRcv = (tANI_U8)cfgValue;

    if(wlan_cfgGetInt(pMac, WNI_CFG_WOWLAN_DEAUTH_ENABLE, &cfgValue) != eSIR_SUCCESS)
    {
       pmmLog(pMac, LOGP, FL("cfgGet failed for WNI_CFG_WOWLAN_DEAUTH_ENABLE "));
       goto end;
    }
    pHalWowlParams->ucWowDeauthRcv = (tANI_U8)cfgValue;

    if(wlan_cfgGetInt(pMac, WNI_CFG_WOWLAN_DISASSOC_ENABLE, &cfgValue) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGP, FL("cfgGet failed for WNI_CFG_WOWLAN_DEAUTH_ENABLE "));
        goto end;
    }
    pHalWowlParams->ucWowDisassocRcv = (tANI_U8)cfgValue;

    if(wlan_cfgGetInt(pMac, WNI_CFG_WOWLAN_MAX_MISSED_BEACON, &cfgValue) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGP, FL("cfgGet failed for WNI_CFG_WOWLAN_MAX_MISSED_BEACON "));
        goto end;
    }
    pHalWowlParams->ucWowMaxMissedBeacons = (tANI_U8)cfgValue;

    if(wlan_cfgGetInt(pMac, WNI_CFG_WOWLAN_MAX_SLEEP_PERIOD, &cfgValue) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGP, FL("cfgGet failed for WNI_CFG_WOWLAN_MAX_SLEEP_PERIOD "));
        goto end;
    }
    pHalWowlParams->ucWowMaxSleepUsec = (tANI_U8)cfgValue;

    //Send message to HAL
    if( eSIR_SUCCESS != (retCode = pmmSendWowlEnterRequest( pMac, pHalWowlParams)))
    {
        pmmLog(pMac, LOGE, FL("Send ENTER_WOWL_REQ to HAL failed, reasonCode %d "), retCode);
        limSendSmeRsp(pMac, eWNI_PMC_ENTER_WOWL_RSP, eSIR_SME_WOWL_ENTER_REQ_FAILED, 0, 0);
        goto end;
    }
    return;

end:
    if (pHalWowlParams != NULL)
        vos_mem_free(pHalWowlParams);
    return;
}


/** ------------------------------------------------------------
\fn      pmmSendWowlEnterRequest
\brief   LIM sends a WDA_WOWL_ENTER_REQ message to HAL with
\        the message structure pHalWowlParams.  HAL shall later
\        send a WDA_WOWL_ENTER_RSP with the same pointer
\        to the message structure back to PMM.
\param   tpAniSirGlobal           pMac
\param   tpSirHalWowlEnterParams  pHalWowlParams
\return  tSirRetStatus
  --------------------------------------------------------------*/
tSirRetStatus pmmSendWowlEnterRequest(tpAniSirGlobal pMac, tpSirHalWowlEnterParams pHalWowlParams)
{
    tSirRetStatus             retCode = eSIR_SUCCESS;
    tSirMsgQ                  msgQ;

    if (NULL == pHalWowlParams)
        return eSIR_FAILURE;

    msgQ.type = WDA_WOWL_ENTER_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pHalWowlParams;
    msgQ.bodyval = 0;

    /* Defer any incoming message until we get
     * a WDA_WOWL_ENTER_RSP from HAL
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    retCode = wdaPostCtrlMsg(pMac, &msgQ);
    if( eSIR_SUCCESS != retCode )
    {
        pmmLog( pMac, LOGE, FL("Posting WDA_WOWL_ENTER_REQ failed, reason=%X"), retCode );
        return retCode;
    }
    return retCode;
}

/** ---------------------------------------------------------
\fn      pmmEnterWowlanResponseHandler
\brief   LIM process the WDA_WOWL_ENTER_RSP message.
\        and sends eWNI_PMC_ENTER_WOWL_RSP to SME.
\param   tpAniSirGlobal  pMac
\param   tpSirMsgQ       limMsg
\return  None
 ------------------------------------------------------------*/
void pmmEnterWowlanResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpSirHalWowlEnterParams  pWowlEnterParams;
    eHalStatus               rspStatus;
    tSirResultCodes          smeRspCode = eSIR_SME_SUCCESS;

    /* we need to process all the deferred messages enqueued
     * since the initiating the WDA_WOWL_ENTER_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    pWowlEnterParams = (tpSirHalWowlEnterParams)(limMsg->bodyptr);
    if (NULL == pWowlEnterParams)
    {
        pmmLog(pMac, LOGE, FL("Recvd WDA_WOWL_ENTER_RSP with NULL msg "));
        smeRspCode = eSIR_SME_WOWL_ENTER_REQ_FAILED;
    }
    else
    {
        rspStatus = pWowlEnterParams->status;

        if(rspStatus == eHAL_STATUS_SUCCESS)
        {
            pmmLog(pMac, LOGW, FL("Rcv successful response from HAL to enter WOWLAN "));
            pMac->pmm.gPmmState = ePMM_STATE_WOWLAN;
        }
        else
        {
            pmmLog(pMac, LOGE, FL("HAL enter WOWLAN failed, informing SME"));
            smeRspCode = eSIR_SME_WOWL_ENTER_REQ_FAILED;
        }
    }

    limSendSmeRsp(pMac, eWNI_PMC_ENTER_WOWL_RSP, smeRspCode, 0, 0);
    return;
}

/** ---------------------------------------------------------
\fn      pmmExitWowlanRequestHandler
\brief   PE process the eWNI_PMC_EXIT_WOWL_REQ message.
\        and sends WDA_WOWL_EXIT_REQ to HAL.
\param   tpAniSirGlobal  pMac
\return  None
 ------------------------------------------------------------*/
void pmmExitWowlanRequestHandler(tpAniSirGlobal pMac)
{
    tSirRetStatus retStatus = eSIR_SUCCESS;
    tSirResultCodes smeRspCode = eSIR_SME_SUCCESS;
    tpPESession pSessionEntry;
    tpSirHalWowlExitParams  pHalWowlMsg = NULL;
    tANI_U8            PowersavesessionId = 0;

    PowersavesessionId = pMac->pmm.sessionId;

    if((pSessionEntry = peFindSessionBySessionId(pMac,PowersavesessionId)) == NULL )
    {
        PELOGW(pmmLog(pMac, LOGE, FL("pmmWowl : failed to allocate memory"));)
        smeRspCode = eSIR_SME_WOWL_EXIT_REQ_FAILED;
        goto failure;
    }

    pHalWowlMsg = vos_mem_malloc(sizeof(*pHalWowlMsg));
    if ( NULL == pHalWowlMsg )
    {
        pmmLog(pMac, LOGP, FL("Fail to allocate memory for WoWLAN Add Bcast Pattern "));
        smeRspCode = eSIR_SME_WOWL_EXIT_REQ_FAILED;
        goto failure;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_EXIT_WOWL_REQ_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    if ( pMac->pmm.gPmmState != ePMM_STATE_WOWLAN )
    {
        pmmLog(pMac, LOGE,
            FL("Exit WOWLAN Request received in invalid state PMM=%d "),
            pMac->pmm.gPmmState);
        smeRspCode = eSIR_SME_INVALID_PMM_STATE;
        goto failure;
    }

    (void) vos_mem_set((tANI_U8 *)pHalWowlMsg, sizeof(*pHalWowlMsg), 0);
    pHalWowlMsg->bssIdx = pSessionEntry->bssIdx;

    if((retStatus = pmmSendExitWowlReq(pMac, pHalWowlMsg)) != eSIR_SUCCESS)
    {
        pmmLog(pMac, LOGE,
            FL("Fail to send WDA_WOWL_EXIT_REQ, reason code %d"),
            retStatus);
        smeRspCode = eSIR_SME_WOWL_EXIT_REQ_FAILED;
        goto failure;
    }
    return;

failure:
    if (pHalWowlMsg != NULL)
       vos_mem_free(pHalWowlMsg);
    limSendSmeRsp(pMac, eWNI_PMC_EXIT_WOWL_RSP, smeRspCode, 0, 0);
    return;
}

/** ---------------------------------------------------------
\fn      pmmSendExitWowlReq
\brief   This function sends the WDA_WOWL_EXIT_REQ
\        message to HAL.
\param   tpAniSirGlobal  pMac
\return  None
 ------------------------------------------------------------*/
tSirRetStatus  pmmSendExitWowlReq(tpAniSirGlobal pMac, tpSirHalWowlExitParams pHalWowlParams)
{
    tSirRetStatus  retCode = eSIR_SUCCESS;
    tSirMsgQ       msgQ;

    if (NULL == pHalWowlParams)
        return eSIR_FAILURE;

    msgQ.type = WDA_WOWL_EXIT_REQ;
    msgQ.reserved = 0;
    msgQ.bodyptr = pHalWowlParams;
    msgQ.bodyval = 0;

    pmmLog(pMac, LOGW, FL("Sending WDA_WOWL_EXIT_REQ"));

    /* we need to defer any incoming messages until
     * we get a WDA_WOWL_EXIT_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
        pmmLog( pMac, LOGE,
            FL("Posting WDA_WOWL_EXIT_REQ failed, reason=%X"),
            retCode );

    return retCode;
}

/** ---------------------------------------------------------
\fn      pmmExitWowlanResponseHandler
\brief   This function process the WDA_WOWL_EXIT_RSP message.
\        and sends back eWNI_PMC_EXIT_WOWL_RSP to SME.
\param   tpAniSirGlobal  pMac
\return  None
 ------------------------------------------------------------*/
void pmmExitWowlanResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{

    tpSirHalWowlExitParams  pHalWowlRspMsg;
    eHalStatus   rspStatus = eHAL_STATUS_FAILURE;

    /* we need to process all the deferred messages enqueued
     * since the initiating the WDA_WOWL_EXIT_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    pHalWowlRspMsg = (tpSirHalWowlExitParams)(limMsg->bodyptr);
    if (NULL == pHalWowlRspMsg)
    {
        pmmLog(pMac, LOGE, FL("Recvd WDA_WOWL_ENTER_RSP with NULL msg "));
    }
    else
    {
        // restore PMM state to BMPS mode
        pMac->pmm.gPmmState = ePMM_STATE_BMPS_SLEEP;
        rspStatus = pHalWowlRspMsg->status;
    }

    if( rspStatus == eHAL_STATUS_SUCCESS)
    {
        pmmLog(pMac, LOGW, FL("Rcvd successful rsp from HAL to exit WOWLAN "));
        limSendSmeRsp(pMac, eWNI_PMC_EXIT_WOWL_RSP, eSIR_SME_SUCCESS, 0, 0);
    }
    else
    {
        pmmLog(pMac, LOGE, FL("Rcvd failure rsp from HAL to exit WOWLAN "));
        limSendSmeRsp(pMac, eWNI_PMC_EXIT_WOWL_RSP, eSIR_SME_WOWL_EXIT_REQ_FAILED, 0, 0);
    }
    return;
}


// --------------------------------------------------------------------
/**
 * pmmImpsSendChangePwrSaveMsg
 *
 * FUNCTION:
 * This function is called to toggle the Idle mode power save mode
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param   Global handle to MAC
 * @param   mode to be configured
 * @return  None
 */

tSirRetStatus pmmImpsSendChangePwrSaveMsg(tpAniSirGlobal pMac, tANI_U8 mode)
{
    tSirRetStatus retStatus = eSIR_SUCCESS;
    tSirMsgQ msgQ;

    if (SIR_PM_SLEEP_MODE == mode)
    {
        msgQ.type = WDA_ENTER_IMPS_REQ;
        pmmLog (pMac, LOG1, FL("Sending WDA_ENTER_IMPS_REQ to HAL"));
    }
    else
    {
        msgQ.type = WDA_EXIT_IMPS_REQ;
        pmmLog (pMac, LOG1, FL("Sending WDA_EXIT_IMPS_REQ to HAL"));
    }

    msgQ.reserved = 0;
    msgQ.bodyptr = NULL;
    msgQ.bodyval = 0;

    /* we need to defer any incoming messages until we get a
     * WDA_ENTER_IMPS_REQ or WDA_EXIT_IMPS_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, msgQ.type));
    retStatus = wdaPostCtrlMsg(pMac, &msgQ);
    if ( eSIR_SUCCESS != retStatus )
    {
        PELOGE(pmmLog(pMac, LOGE, 
            FL("WDA_ENTER/EXIT_IMPS_REQ to HAL failed, reason=%X"), retStatus);)
    }

    return retStatus;
}

// --------------------------------------------------------------------
/**
 * pmmUapsdSendChangePwrSaveMsg
 *
 * FUNCTION:
 * This function is called to send either WDA_ENTER_UAPSD_REQ
 * or WDA_EXIT_UAPSD_REQ to HAL.
 *
 * NOTE:
 *
 * @param   pMac     Global handle to MAC
 * @param   mode     mode to be configured
 * @return  tSirRetStatus
 */
tSirRetStatus pmmUapsdSendChangePwrSaveMsg (tpAniSirGlobal pMac, tANI_U8 mode)
{
    tSirRetStatus retStatus = eSIR_SUCCESS;
    tpUapsdParams pUapsdParams = NULL;
    tSirMsgQ msgQ;
    tpPESession pSessionEntry;
    tpExitUapsdParams pExitUapsdParams = NULL;

    if((pSessionEntry = peGetValidPowerSaveSession(pMac)) == NULL )
    {
        PELOGW(pmmLog(pMac, LOGW, FL("pmmUapsd : failed to allocate memory"));)
        retStatus = eSIR_FAILURE;
        return retStatus;
    }

    if (SIR_PM_SLEEP_MODE == mode)
    {
        pUapsdParams = vos_mem_malloc(sizeof(tUapsdParams));
        if ( NULL == pUapsdParams )
        {
            PELOGW(pmmLog(pMac, LOGW, FL("pmmUapsd : failed to allocate memory"));)
            retStatus = eSIR_MEM_ALLOC_FAILED;
            return retStatus;
        }

        vos_mem_set( (tANI_U8 *)pUapsdParams, sizeof(tUapsdParams), 0);
        msgQ.type = WDA_ENTER_UAPSD_REQ;
        msgQ.bodyptr = pUapsdParams;

        /*
        * An AC is delivery enabled AC if the bit for that AC is set into the
        * gAcAdmitMask[SIR_MAC_DIRECTION_DLINK],it is not set then we will take Static values.
        */

        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] & LIM_ADMIT_MASK_FLAG_ACBE)
        {
            pUapsdParams->beDeliveryEnabled = LIM_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcDeliveryEnableMask);
        }
        else
        {
            pUapsdParams->beDeliveryEnabled = LIM_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcBitmask);
        }
        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] & LIM_ADMIT_MASK_FLAG_ACBK)
        {
            pUapsdParams->bkDeliveryEnabled = LIM_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcDeliveryEnableMask);
        }
        else
        {
            pUapsdParams->bkDeliveryEnabled = LIM_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcBitmask);
        }
        if  ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] & LIM_ADMIT_MASK_FLAG_ACVI)
        {
            pUapsdParams->viDeliveryEnabled = LIM_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcDeliveryEnableMask);
        }
        else
        {
            pUapsdParams->viDeliveryEnabled = LIM_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcBitmask);
        }

        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] & LIM_ADMIT_MASK_FLAG_ACVO)
        {
            pUapsdParams->voDeliveryEnabled = LIM_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcDeliveryEnableMask);
        }
        else
        {
            pUapsdParams->voDeliveryEnabled = LIM_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcBitmask);
        }

        /*
        * An AC is trigger enabled AC if the bit for that AC is set into the
        * gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK],it is not set then we will take Static values.
        */

        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] & LIM_ADMIT_MASK_FLAG_ACBE)
        {
             pUapsdParams->beTriggerEnabled = LIM_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcTriggerEnableMask);
        }
        else
        {
             pUapsdParams->beTriggerEnabled = LIM_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcBitmask);
        }
        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] & LIM_ADMIT_MASK_FLAG_ACBK)
        {
             pUapsdParams->bkTriggerEnabled = LIM_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcTriggerEnableMask);
        }
        else
        {
             pUapsdParams->bkTriggerEnabled = LIM_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcBitmask);
        }
        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] & LIM_ADMIT_MASK_FLAG_ACVI)
        {
             pUapsdParams->viTriggerEnabled = LIM_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcTriggerEnableMask);
        }
        else
        {
             pUapsdParams->viTriggerEnabled = LIM_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcBitmask);
        }

        if ( pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] & LIM_ADMIT_MASK_FLAG_ACVO)
        {
             pUapsdParams->voTriggerEnabled = LIM_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcTriggerEnableMask);
        }
        else
        {
             pUapsdParams->voTriggerEnabled = LIM_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcBitmask);
        }

        pUapsdParams->bssIdx = pSessionEntry->bssIdx;

        PELOGW(pmmLog(pMac, LOGW,
                      FL("UAPSD Mask:  static = 0x%x, DeliveryEnabled = 0x%x, TriggerEnabled = 0x%x "),
            pMac->lim.gUapsdPerAcBitmask,
            pMac->lim.gUapsdPerAcDeliveryEnableMask,
            pMac->lim.gUapsdPerAcTriggerEnableMask);)

        PELOGW(pmmLog(pMac, LOGW, FL("Delivery Enabled: BK=%d, BE=%d, Vi=%d, Vo=%d "),
            pUapsdParams->bkDeliveryEnabled, 
            pUapsdParams->beDeliveryEnabled, 
            pUapsdParams->viDeliveryEnabled, 
            pUapsdParams->voDeliveryEnabled);)

        PELOGW(pmmLog(pMac, LOGW, FL("Trigger Enabled: BK=%d, BE=%d, Vi=%d, Vo=%d "),
            pUapsdParams->bkTriggerEnabled, 
            pUapsdParams->beTriggerEnabled, 
            pUapsdParams->viTriggerEnabled, 
            pUapsdParams->voTriggerEnabled);)

        if (pUapsdParams->bkDeliveryEnabled == 0 &&
            pUapsdParams->beDeliveryEnabled == 0 &&
            pUapsdParams->viDeliveryEnabled == 0 &&
            pUapsdParams->voDeliveryEnabled == 0 &&
            pUapsdParams->bkTriggerEnabled == 0 &&
            pUapsdParams->beTriggerEnabled == 0 &&
            pUapsdParams->viTriggerEnabled == 0 &&
            pUapsdParams->voTriggerEnabled == 0)
        {
            limLog(pMac, LOGW, FL("No Need to enter UAPSD since Trigger "
                 "Enabled and Delivery Enabled Mask is zero for all ACs"));
            retStatus = eSIR_PMM_INVALID_REQ;
            return retStatus;
        }

        PELOGW(pmmLog (pMac, LOGW, FL("pmmUapsd: Sending WDA_ENTER_UAPSD_REQ to HAL"));)
    }
    else
    {
        pExitUapsdParams = vos_mem_malloc(sizeof(tExitUapsdParams));
        if ( NULL == pExitUapsdParams )
        {
            PELOGW(pmmLog(pMac, LOGW, FL("pmmUapsd : failed to allocate memory"));)
            retStatus = eSIR_MEM_ALLOC_FAILED;
            return retStatus;
        }

        vos_mem_set( (tANI_U8 *)pExitUapsdParams, sizeof(tExitUapsdParams), 0);
        msgQ.type = WDA_EXIT_UAPSD_REQ;
        msgQ.bodyptr = pExitUapsdParams;
        pExitUapsdParams->bssIdx = pSessionEntry->bssIdx;
        PELOGW(pmmLog (pMac, LOGW, FL("pmmUapsd: Sending WDA_EXIT_UAPSD_REQ to HAL"));)
    }

    /* we need to defer any incoming messages until we get a
     * WDA_ENTER/EXIT_UAPSD_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

    msgQ.reserved = 0;
    msgQ.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, msgQ.type));
    retStatus = wdaPostCtrlMsg(pMac, &msgQ);
    if ( eSIR_SUCCESS != retStatus )
    {
        PELOGE(pmmLog(pMac, LOGE, 
            FL("pmmUapsd: WDA_ENTER/EXIT_UAPSD_REQ to HAL failed, reason=%X"),
            retStatus);)
        if (SIR_PM_SLEEP_MODE == mode)
            vos_mem_free(pUapsdParams);
        else
            vos_mem_free(pExitUapsdParams);
    }

    return retStatus;
}


// --------------------------------------------------------------------
/**
 * pmmUpdateImpsPwrSaveStats
 *
 * FUNCTION:
 * This function is called to update the power save statistics in MAC
 * for Idle mode power save
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param   Global handle to MAC
 * @return  None
 */

void pmmImpsUpdatePwrSaveStats(tpAniSirGlobal pMac)
{
/*
    tANI_U64 TimeAwake = 0;

    pMac->pmm.ImpsSleepTimeStamp = vos_timer_get_system_ticks();

    if (pMac->pmm.ImpsWakeupTimeStamp)
    {
        TimeAwake = (pMac->pmm.ImpsSleepTimeStamp - pMac->pmm.ImpsWakeupTimeStamp) / 10 ;
    }
    else
    {
        TimeAwake = 0;
    }

    if (TimeAwake > pMac->pmm.ImpsMaxTimeAwake)
    {
        pMac->pmm.ImpsMaxTimeAwake = TimeAwake;
    }

    if ((!pMac->pmm.ImpsMinTimeAwake) || (TimeAwake < pMac->pmm.ImpsMinTimeAwake))
    {
        pMac->pmm.ImpsMinTimeAwake = TimeAwake;
    }

    pMac->pmm.ImpsAvgTimeAwake = ((pMac->pmm.ImpsAvgTimeAwake * pMac->pmm.ImpsCntSleep) + TimeAwake) / (pMac->pmm.ImpsCntSleep + 1);

*/
    (pMac->pmm.ImpsCntSleep)++;

    return;
}


// --------------------------------------------------------------------
/**
 * pmmImpsUpdateWakeupStats
 *
 * FUNCTION:
 * This function is called to update the Wake up statistics in MAC
 * for Idle mode power save
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None
 *
 * NOTE:
 *
 * @param   Global handle to MAC
 * @return  None
 */

void pmmImpsUpdateWakeupStats (tpAniSirGlobal pMac)
{
/*
    tANI_U64 SleepTime = 0;

    pMac->pmm.ImpsWakeupTimeStamp = vos_timer_get_system_ticks();

    SleepTime = (pMac->pmm.ImpsWakeupTimeStamp - pMac->pmm.ImpsSleepTimeStamp) / 10;

    if (SleepTime > pMac->pmm.ImpsMaxSleepTime)
    {
        pMac->pmm.ImpsMaxSleepTime = SleepTime;
    }

    if ((!pMac->pmm.ImpsMinSleepTime) || (SleepTime < pMac->pmm.ImpsMinSleepTime))
    {
        pMac->pmm.ImpsMinSleepTime = SleepTime;
    }

    pMac->pmm.ImpsAvgSleepTime = ( ( (pMac->pmm.ImpsAvgSleepTime * pMac->pmm.ImpsCntAwake) + SleepTime) / (pMac->pmm.ImpsCntAwake + 1));

*/
    (pMac->pmm.ImpsCntAwake)++;

    return;
}

// Collects number of times error occurred while going to sleep mode
void pmmImpsUpdateSleepErrStats(tpAniSirGlobal pMac,
                                tSirRetStatus retStatus)
{
    pMac->pmm.ImpsSleepErrCnt++;
    pMac->pmm.ImpsLastErr = retStatus;
    return;
}

// Collects number of times error occurred while waking up from sleep mode
void pmmImpsUpdateWakeupErrStats(tpAniSirGlobal pMac,
                                 tSirRetStatus retStatus)
{
    pMac->pmm.ImpsWakeupErrCnt++;
    pMac->pmm.ImpsLastErr = retStatus;
    return;
}


// Collects number of times the system has received request or
// response in an invalid state
void pmmImpsUpdateErrStateStats(tpAniSirGlobal pMac)
{
    pMac->pmm.ImpsInvalidStateCnt++;
    return;
}

// Collects number of packets dropped while in IMPS mode
void pmmImpsUpdatePktDropStats(tpAniSirGlobal pMac)
{

    pMac->pmm.ImpsPktDrpInSleepMode++;
    return;
}

// Collects number of packets dropped while in BMPS mode
void pmmBmpsUpdatePktDropStats(tpAniSirGlobal pMac)
{

    pMac->pmm.BmpsPktDrpInSleepMode++;
    return;
}

// Collects statistics for number of times BMPS init failed
void pmmBmpsUpdateInitFailureCnt(tpAniSirGlobal pMac)
{

    pMac->pmm.BmpsInitFailCnt++;
    return;
}

// Collects statistics for number of times sleep request failed
void pmmBmpsUpdateSleepReqFailureCnt(tpAniSirGlobal pMac)
{

    pMac->pmm.BmpsSleeReqFailCnt++;
    return;
}

// Collects statistics for number of times Wakeup request failed
void pmmBmpsUpdateWakeupReqFailureCnt(tpAniSirGlobal pMac)
{

    pMac->pmm.BmpsWakeupReqFailCnt++;
    return;
}

// Collects statistics for number of times request / response received in invalid state
void pmmBmpsUpdateInvalidStateCnt(tpAniSirGlobal pMac)
{

    pMac->pmm.BmpsInvStateCnt++;
    return;
}

// Collects statistics for number of times wakeup indications received
void pmmBmpsUpdateWakeupIndCnt(tpAniSirGlobal pMac)
{
    pMac->pmm.BmpsWakeupIndCnt++;
    return;
}

// Collects statistics for number of times wakeup indications received
void pmmBmpsUpdateHalReqFailureCnt(tpAniSirGlobal pMac)
{
    pMac->pmm.BmpsHalReqFailCnt++;
    return;
}

// Collects statistics for number of times requests received from HDD in
// invalid device role
void pmmBmpsUpdateReqInInvalidRoleCnt(tpAniSirGlobal pMac)
{
    pMac->pmm.BmpsReqInInvalidRoleCnt++;
    return;
}

#if 0
// Update the sleep statistics
void pmmUpdateDroppedPktStats(tpAniSirGlobal pMac)
{
    switch (pMac->pmm.gPmmState)
    {
    case ePMM_STATE_BMPS_SLEEP:
        pmmBmpsUpdatePktDropStats(pMac);
        break;

    case ePMM_STATE_IMPS_SLEEP:
        pmmImpsUpdatePktDropStats(pMac);
        break;

    default:
        break;
    }
    return;

}
#endif

// Resets PMM state ePMM_STATE_READY
void pmmResetPmmState(tpAniSirGlobal pMac)
{
    pMac->pmm.gPmmState = ePMM_STATE_READY;
    
    pMac->pmm.inMissedBeaconScenario = FALSE;
    return;
}

/* Sends Background scan message back to Lim */
void pmmSendMessageToLim(tpAniSirGlobal pMac,
                         tANI_U32 msgId)
{
    tSirMsgQ limMsg;
    tANI_U32 statusCode;

    limMsg.type = (tANI_U16) msgId;
    limMsg.bodyptr = NULL;
    limMsg.bodyval = 0;

    if ((statusCode = limPostMsgApi(pMac, &limMsg)) != eSIR_SUCCESS)
    {
          PELOGW(pmmLog(pMac, LOGW,
            FL("posting message %X to LIM failed, reason=%d"),
            limMsg.type, statusCode);)
    }
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
void pmmFilterMatchCountResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpSirRcvFltPktMatchRsp  pRcvFltPktMatchCntRsp;
    eHalStatus              rspStatus;
    tSirResultCodes         smeRspCode = eSIR_SME_SUCCESS;

    /* we need to process all the deferred messages enqueued
     * since the initiating the WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    pRcvFltPktMatchCntRsp = (tpSirRcvFltPktMatchRsp)(limMsg->bodyptr);
    if (NULL == pRcvFltPktMatchCntRsp)
    {
        pmmLog(pMac, LOGE, FL("Received "
            "WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP with NULL msg "));
        smeRspCode = eSIR_SME_PC_FILTER_MATCH_COUNT_REQ_FAILED;
    }
    else
    {
        rspStatus = pRcvFltPktMatchCntRsp->status;
        if (eHAL_STATUS_SUCCESS == rspStatus)
        {
            pmmLog(pMac, LOGE, FL("Rcv successful response from HAL to get "
                "Packet Coalescing Filter Match Count"));
        }
        else
        {
            pmmLog(pMac, LOGE, FL("HAL failed to get Packet Coalescing "
                "Filter Match Count, informing SME"));
            smeRspCode = eSIR_SME_PC_FILTER_MATCH_COUNT_REQ_FAILED;
        }
    }

    limSendSmeRsp(pMac, eWNI_PMC_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP, 
                  smeRspCode, 0, 0);
    return;
}
#endif // WLAN_FEATURE_PACKET_FILTERING

#ifdef WLAN_FEATURE_GTK_OFFLOAD
void pmmGTKOffloadGetInfoResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpSirGtkOffloadGetInfoRspParams  pGtkOffloadGetInfoRspParams;
    eHalStatus                       rspStatus;
    tSirResultCodes                  smeRspCode = eSIR_SME_SUCCESS;

    /* we need to process all the deferred messages enqueued
     * since the initiating the WDA_GTK_OFFLOAD_GETINFO_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    pGtkOffloadGetInfoRspParams = (tpSirGtkOffloadGetInfoRspParams)(limMsg->bodyptr);
    if (NULL == pGtkOffloadGetInfoRspParams)
    {
        pmmLog(pMac, LOGE, FL("Received WDA_GTK_OFFLOAD_GETINFO_RSP with NULL msg "));
        smeRspCode = eSIR_SME_GTK_OFFLOAD_GETINFO_REQ_FAILED;
    }
    else
    {
        rspStatus = pGtkOffloadGetInfoRspParams->ulStatus;
        if(rspStatus == eHAL_STATUS_SUCCESS)
        {
            pmmLog(pMac, LOGW, FL("Rcv successful response from HAL to get GTK Offload Information"));
        }
        else
        {
            pmmLog(pMac, LOGE, FL("HAL failed to get GTK Offload Information, informing SME"));
            smeRspCode = eSIR_SME_GTK_OFFLOAD_GETINFO_REQ_FAILED;
        }
    }

    limSendSmeRsp(pMac, eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP, smeRspCode, 0, 0);
    return;
}
#endif // WLAN_FEATURE_GTK_OFFLOAD
