/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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
 *
 * This file limRoamingAlgo.cc contains the code for LIM
 * algorithms.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniCfgSta.h"
#include "cfgApi.h"
#include "limTypes.h"
#include "limTimerUtils.h"
#include "limTrace.h"



/** ----------------------------------------------------------------------
\fn      limSelectsBackgroundScanMode() 
\brief   This function is called by limIsBackgroundScanAllowed(). 
\        Here LIM decides whether we shall enforce this background 
\        scan or let HAL decide whether to proceed with the background 
\        scan as HAL sees fits.  LIM shall enforce background scan if:
\        1) station is not in link established state
\        2) station is in link established state, but there has been
\           max number of consecutive background scan failure.
\ 
\param   tpAniSirGlobal  pMac
\return  none
\ ------------------------------------------------------------------------- */
tSirBackgroundScanMode limSelectsBackgroundScanMode(tpAniSirGlobal pMac)
{
    tANI_U32    cfgVal;

    if (wlan_cfgGetInt(pMac, WNI_CFG_MAX_CONSECUTIVE_BACKGROUND_SCAN_FAILURE, &cfgVal) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Fail to get WNI_CFG_MAX_CONSECUTIVE_BACKGROUND_SCAN_FAILURE value"));
        return eSIR_NORMAL_BACKGROUND_SCAN;
    }
  
    if (cfgVal == 0)
        return eSIR_NORMAL_BACKGROUND_SCAN;

    /* If the "number of consecutive background scan failure"
     * exceeds the maximum allowed, then LIM shall trigger an
     * aggressive background scan.
     */
    if (pMac->lim.gLimNumOfConsecutiveBkgndScanFailure >= cfgVal)
    {
        pMac->lim.gLimNumOfForcedBkgndScan += 1;
        limLog(pMac, LOGE,
               FL("Had %d consec scan fail(when expect < %d). Trigger AGGRESSIVE bkgnd scan."),
               pMac->lim.gLimNumOfConsecutiveBkgndScanFailure, cfgVal);
        return eSIR_AGGRESSIVE_BACKGROUND_SCAN;
    }

    return eSIR_NORMAL_BACKGROUND_SCAN;
}


/** -----------------------------------------------------------
\fn      limIsBackgroundScanAllowed
\brief   This function determines if background scan should be
\        allowed. It is called by limTriggerBackgroundScan(). 
\param   tpAniSirGlobal  pMac
\return  none
\ ------------------------------------------------------------- */
static tANI_U8  limIsBackgroundScanAllowed(tpAniSirGlobal pMac)
{
    // if we are in the middle of a scan already, skip the background scan
    if (limIsSystemInScanState(pMac) ||
        (pMac->lim.gLimBackgroundScanDisable) ||
        (pMac->lim.gLimForceBackgroundScanDisable) ||
        (pMac->lim.gLimBackgroundScanTerminate))
        return false;

    //need to do background scan in IBSS mode.
    if (pMac->lim.gLimSystemRole == eLIM_STA_IN_IBSS_ROLE)
    {
        if (pMac->lim.gLimSmeState != eLIM_SME_NORMAL_STATE)
            return false;
        return true;
    }

    // If station is not in link established state, then skip background scan
    if ( (pMac->lim.gLimSystemRole == eLIM_STA_ROLE) && (pMac->lim.gLimSmeState != eLIM_SME_LINK_EST_STATE) )
        return false;

    /* now that we have checked for scan state, check for other transitional
     * states which should not be interrupted by scans
     */
    if ((! (pMac->lim.gLimSmeState == eLIM_SME_IDLE_STATE) ) &&
        (! (pMac->lim.gLimSmeState == eLIM_SME_JOIN_FAILURE_STATE) ) &&
        (! (pMac->lim.gLimSmeState == eLIM_SME_LINK_EST_STATE) ) )
        return false;

    return true;
}


/** ---------------------------------------------------------------
\fn      limTriggerBackgroundScan()
\brief   This function is called upon background scan interval
\        when there is an exisiting link with an AP.
\        SME_SCAN_REQ is issued to SME state machine with Active
\        scanning is performed on one channel at a time.
\ 
\        Assumptions:
\        Valid channel list at CFG is either populated by Roaming 
\        algorithm upon determining/selecting a regulatory domain 
\        or by default has all 36 possible channels.
\
\param   tpAniSirGlobal  pMac
\return  none
\ ----------------------------------------------------------------- */
void limTriggerBackgroundScan(tpAniSirGlobal pMac)
{
    tANI_U32    len   = WNI_CFG_BG_SCAN_CHANNEL_LIST_LEN;
    tANI_U32    ssidLen = SIR_MAC_MAX_SSID_LENGTH;
    tSirMacChanNum   bgScanChannelList[WNI_CFG_BG_SCAN_CHANNEL_LIST_LEN];
    tSirSmeScanReq   smeScanReq;
    tSirMacAddr      bcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tSirBackgroundScanMode   backgroundScan;

    PELOG1(limLog(pMac, LOG1, FL("Background Scan: %d success, %d consec fail "),
        pMac->lim.gLimNumOfBackgroundScanSuccess,  pMac->lim.gLimNumOfConsecutiveBkgndScanFailure);)

    if (! limIsBackgroundScanAllowed(pMac))
    {
        PELOG1(limLog(pMac, LOG1, FL("Skipping Background Scan "));)
        return;
    }

    // Get background scan channel list from CFG
    if (wlan_cfgGetStr(pMac, WNI_CFG_BG_SCAN_CHANNEL_LIST,
                  (tANI_U8 *) bgScanChannelList,
                  (tANI_U32 *) &len) != eSIR_SUCCESS)
    {
        /**
         * Could not get Valid channel list from CFG.
         * Log error.
         */
        PELOGE(limLog(pMac, LOGE, FL("could not retrieve valid channel list"));)

        return;
    }

    // Time to perform background scan. Prepare and issue
    // SME_SCAN_REQ to SME State machine
    smeScanReq.messageType = eWNI_SME_SCAN_REQ;
    smeScanReq.length      = sizeof(tSirSmeScanReq);
    smeScanReq.bssType     = eSIR_INFRASTRUCTURE_MODE;
    vos_mem_copy( (tANI_U8 *) smeScanReq.bssId,
                  (tANI_U8 *) &bcAddr, sizeof(tSirMacAddr));
 
    if (wlan_cfgGetStr(pMac, WNI_CFG_SSID,
                    (tANI_U8 *) (smeScanReq.ssId[0].ssId),
                    (tANI_U32 *) &ssidLen) != eSIR_SUCCESS)
    {
        /// Could not get SSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve SSID"));
    }
    smeScanReq.ssId[0].length = (tANI_U8) ssidLen;
    smeScanReq.numSsid = 1;

    smeScanReq.scanType    = eSIR_ACTIVE_SCAN;
    smeScanReq.sessionId = 0;

    if (wlan_cfgGetInt(pMac, WNI_CFG_ACTIVE_MINIMUM_CHANNEL_TIME,
                  &smeScanReq.minChannelTime) != eSIR_SUCCESS)
    {
        /// Could not get minChlTime value from CFG. Log error.
        PELOGE(limLog(pMac, LOGE, FL("could not retrieve minChlTime value"));)

        return;
    }

    if (wlan_cfgGetInt(pMac, WNI_CFG_ACTIVE_MAXIMUM_CHANNEL_TIME,
                  &smeScanReq.maxChannelTime) != eSIR_SUCCESS)
    {
        /// Could not get maxChlTime value from CFG. Log error.
        PELOGE(limLog(pMac, LOGE, FL("could not retrieve maxChlTime value"));)

        return;
    }

    smeScanReq.returnAfterFirstMatch = 0;
    smeScanReq.returnUniqueResults   = 1;

    //At the first channel scan, clear the cached results 
    if(pMac->lim.gLimBackgroundScanChannelId == 0)
    {
        smeScanReq.returnFreshResults    = SIR_BG_SCAN_PURGE_RESUTLS|SIR_BG_SCAN_RETURN_FRESH_RESULTS;
    }
    else
    {
        smeScanReq.returnFreshResults    = SIR_BG_SCAN_RETURN_FRESH_RESULTS;
    }
   

    smeScanReq.channelList.numChannels = 1;
    if (pMac->lim.gLimBackgroundScanChannelId >= len)
    {
        pMac->lim.gLimBackgroundScanChannelId = 0;

        PELOGE(limLog(pMac, LOGE, FL("Skipping Background Scan since the channel list is exhausted."));)
        PELOGE(limLog(pMac, LOGE, FL("SME should send WNI_CFG_BACKGROUND_SCAN_PERIOD indication to start the background scan again."));)

        /* Stop the BG scan timer here. SME should send WNI_CFG_BACKGROUND_SCAN_PERIOD 
         * indication to start the background scan again.
         */
        if (TX_TIMER_VALID(pMac->lim.limTimers.gLimBackgroundScanTimer))
        {
            MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, NO_SESSION, eLIM_BACKGROUND_SCAN_TIMER));
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimBackgroundScanTimer)
                            != TX_SUCCESS)
            {
                // Could not deactivate BackgroundScanTimer timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to deactivate BackgroundScanTimer timer"));
            }
        }

        pMac->lim.gLimBackgroundScanTerminate = TRUE;

        PELOGE(limLog(pMac, LOGE, FL("Send dummy scan with returnFreshResults as 0 to report BG scan results to SME."));)
        return;
    }
    smeScanReq.channelList.channelNumber[0] =
              bgScanChannelList[pMac->lim.gLimBackgroundScanChannelId++];

    smeScanReq.uIEFieldLen = 0;
    smeScanReq.uIEFieldOffset = sizeof(tSirSmeScanReq);
    
    backgroundScan = limSelectsBackgroundScanMode(pMac);
    PELOG1(limLog(pMac, LOG1, FL("Performing (mode %s (%d)) Background Scan"),
           lim_BackgroundScanModetoString(backgroundScan),
           backgroundScan);)
    smeScanReq.backgroundScanMode = backgroundScan;
    
    //determine whether to send the results or not, If so, notify the BG scan results to SME
    if (pMac->lim.gLimBackgroundScanChannelId >= len)
    {
        pMac->lim.gLimReportBackgroundScanResults = TRUE;
    }
    
    limPostSmeMessage(pMac,
                      eWNI_SME_SCAN_REQ,
                      (tANI_U32 *) &smeScanReq);
} /*** limTriggerBackgroundScan() ***/


/** ----------------------------------------------------------------------
\fn      limAbortBackgroundScan
\brief   This function aborts background scan and send scan 
\        response to SME. 
\param   tpAniSirGlobal  pMac
\return  none
\ ------------------------------------------------------------------------- */
void limAbortBackgroundScan(tpAniSirGlobal pMac)
{
    tANI_U16  scanRspLen = 8;

    if(pMac->lim.gLimBackgroundScanTerminate == FALSE) 
    {
        limLog(pMac, LOGE, FL("Abort Background Scan "));
        if (TX_TIMER_VALID(pMac->lim.limTimers.gLimBackgroundScanTimer))
        {
            limDeactivateAndChangeTimer(pMac, eLIM_BACKGROUND_SCAN_TIMER); 
        }

        pMac->lim.gLimBackgroundScanTerminate = TRUE;
        pMac->lim.gLimBackgroundScanStarted = FALSE;

        if (pMac->lim.gLimSmeScanResultLength == 0)
            limSendSmeScanRsp(pMac, scanRspLen, eSIR_SME_SUCCESS, 0, 0);
        else
        {
            scanRspLen = sizeof(tSirSmeScanRsp) +
                         pMac->lim.gLimSmeScanResultLength -
                         sizeof(tSirBssDescription);
            limSendSmeScanRsp(pMac, scanRspLen, eSIR_SME_SUCCESS, 0, 0);
        }       
    }

    // reset background scan variables
    pMac->lim.gLimBackgroundScanChannelId = 0;   
    return;
}

