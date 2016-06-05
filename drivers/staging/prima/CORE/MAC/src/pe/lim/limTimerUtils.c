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
 * This file limTimerUtils.cc contains the utility functions
 * LIM uses for handling various timers.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "pmmApi.h"
#include "limApi.h"

// default value 5000 ms for background scan period when it is disabled
#define LIM_BACKGROUND_SCAN_PERIOD_DEFAULT_MS    5000
// channel Switch Timer in ticks
#define LIM_CHANNEL_SWITCH_TIMER_TICKS           1
// Lim Quite timer in ticks
#define LIM_QUIET_TIMER_TICKS                    100
// Lim Quite BSS timer interval in ticks
#define LIM_QUIET_BSS_TIMER_TICK                 100
// Lim KeepAlive timer default (3000)ms
#define LIM_KEEPALIVE_TIMER_MS                   3000
// Lim JoinProbeRequest Retry  timer default (200)ms
#define LIM_JOIN_PROBE_REQ_TIMER_MS              200
#define LIM_AUTH_RETRY_TIMER_MS              60


//default beacon interval value used in HB timer interval calculation
#define LIM_HB_TIMER_BEACON_INTERVAL             100

/* This timer is a periodic timer which expires at every 1 sec to
   convert  ACTIVE DFS channel to DFS channels */
#define ACTIVE_TO_PASSIVE_CONVERISON_TIMEOUT     1000

/**
 * limCreateTimers()
 *
 *FUNCTION:
 * This function is called upon receiving
 * 1. SME_START_REQ for STA in ESS role
 * 2. SME_START_BSS_REQ for AP role & STA in IBSS role
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 *
 * @return None
 */

v_UINT_t
limCreateTimers(tpAniSirGlobal pMac)
{
    tANI_U32 cfgValue, i=0;
    tANI_U32 cfgValue1;

    PELOG1(limLog(pMac, LOG1, FL("Creating Timers used by LIM module in Role %d"), pMac->lim.gLimSystemRole);)

    if (wlan_cfgGetInt(pMac, WNI_CFG_ACTIVE_MINIMUM_CHANNEL_TIME,
                  &cfgValue) != eSIR_SUCCESS)
    {
        /**
         * Could not get MinChannelTimeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGP, FL("could not retrieve MinChannelTimeout value"));
    }
    cfgValue = SYS_MS_TO_TICKS(cfgValue);

    // Create MIN/MAX channel timers and activate them later
    if (tx_timer_create(&pMac->lim.limTimers.gLimMinChannelTimer,
                        "MIN CHANNEL TIMEOUT",
                        limTimerHandler, SIR_LIM_MIN_CHANNEL_TIMEOUT,
                        cfgValue, 0,
                        TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        /// Could not start min channel timer.
        // Log error
        limLog(pMac, LOGP, FL("could not create MIN channel timer"));
        return TX_TIMER_ERROR;
    }
    PELOG2(limLog(pMac, LOG2, FL("Created MinChannelTimer"));)

    /* Periodic probe request timer value is half of the Min channel
     * timer. Probe request sends periodically till min/max channel
     * timer expires
     */

    cfgValue1 = cfgValue/2 ;
    if( cfgValue1 >= 1)
    {
        // Create periodic probe request timer and activate them later
        if (tx_timer_create(&pMac->lim.limTimers.gLimPeriodicProbeReqTimer,
                           "Periodic Probe Request Timer",
                           limTimerHandler, SIR_LIM_PERIODIC_PROBE_REQ_TIMEOUT,
                           cfgValue1, 0,
                           TX_NO_ACTIVATE) != TX_SUCCESS)
        {
           /// Could not start Periodic Probe Req timer.
           // Log error
           limLog(pMac, LOGP, FL("could not create periodic probe timer"));
           goto err_timer;
        }
     }


    if (wlan_cfgGetInt(pMac, WNI_CFG_ACTIVE_MAXIMUM_CHANNEL_TIME,
                  &cfgValue) != eSIR_SUCCESS)
    {
        /**
         * Could not get MAXChannelTimeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGP,
               FL("could not retrieve MAXChannelTimeout value"));
    }
    cfgValue = SYS_MS_TO_TICKS(cfgValue);

    /* Limiting max numm of probe req for each channel scan */
    pMac->lim.maxProbe = (cfgValue/cfgValue1);

    if (tx_timer_create(&pMac->lim.limTimers.gLimMaxChannelTimer,
                        "MAX CHANNEL TIMEOUT",
                        limTimerHandler, SIR_LIM_MAX_CHANNEL_TIMEOUT,
                        cfgValue, 0,
                        TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        /// Could not start max channel timer.
        // Log error
        limLog(pMac, LOGP, FL("could not create MAX channel timer"));

        goto err_timer;
    }
    PELOG2(limLog(pMac, LOG2, FL("Created MaxChannelTimer"));)

    if (pMac->lim.gLimSystemRole != eLIM_AP_ROLE)
    {
        // Create Channel Switch Timer
        if (tx_timer_create(&pMac->lim.limTimers.gLimChannelSwitchTimer,
                            "CHANNEL SWITCH TIMER",
                            limChannelSwitchTimerHandler,
                            0,                         // expiration_input
                            LIM_CHANNEL_SWITCH_TIMER_TICKS,  // initial_ticks
                            0,                         // reschedule_ticks
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("failed to create Channel Switch timer"));
            goto err_timer;
        }

        //
        // Create Quiet Timer
        // This is used on the STA to go and shut-off
        // Tx/Rx "after" the specified quiteInterval
        //
        if (tx_timer_create(&pMac->lim.limTimers.gLimQuietTimer,
                            "QUIET TIMER",
                            limQuietTimerHandler,
                            SIR_LIM_QUIET_TIMEOUT,     // expiration_input
                            LIM_QUIET_TIMER_TICKS,     // initial_ticks
                            0,                         // reschedule_ticks
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("failed to create Quiet Begin Timer"));
            goto err_timer;
        }

        //
        // Create Quiet BSS Timer
        // After the specified quiteInterval, determined by
        // gLimQuietTimer, this timer, gLimQuietBssTimer,
        // trigger and put the STA to sleep for the specified
        // gLimQuietDuration
        //
        if (tx_timer_create(&pMac->lim.limTimers.gLimQuietBssTimer,
                            "QUIET BSS TIMER",
                            limQuietBssTimerHandler,
                            SIR_LIM_QUIET_BSS_TIMEOUT, // expiration_input
                            LIM_QUIET_BSS_TIMER_TICK,  // initial_ticks
                            0,                         // reschedule_ticks
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("failed to create Quiet Begin Timer"));
            goto err_timer;
        }

        if (wlan_cfgGetInt(pMac, WNI_CFG_JOIN_FAILURE_TIMEOUT,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get JoinFailureTimeout value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve JoinFailureTimeout value"));
        }
        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        // Create Join failure timer and activate it later
        if (tx_timer_create(&pMac->lim.limTimers.gLimJoinFailureTimer,
                        "JOIN FAILURE TIMEOUT",
                        limTimerHandler, SIR_LIM_JOIN_FAIL_TIMEOUT,
                        cfgValue, 0,
                        TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not create Join failure timer.
            // Log error
            limLog(pMac, LOGP, FL("could not create Join failure timer"));

            goto err_timer;
        }

        //Send unicast probe req frame every 200 ms
        if ((tx_timer_create(&pMac->lim.limTimers.gLimPeriodicJoinProbeReqTimer,
                        "Periodic Join Probe Request Timer",
                        limTimerHandler, SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT,
                        SYS_MS_TO_TICKS(LIM_JOIN_PROBE_REQ_TIMER_MS), 0,
                        TX_NO_ACTIVATE)) != TX_SUCCESS)
        {
            /// Could not create Periodic Join Probe Request timer.
            // Log error
            limLog(pMac, LOGP, FL("could not create Periodic Join Probe Request timer"));
            goto err_timer;
        }
        //Send Auth frame every 60 ms
        if ((tx_timer_create(&pMac->lim.limTimers.gLimPeriodicAuthRetryTimer,
                        "Periodic AUTH Timer",
                        limTimerHandler, SIR_LIM_AUTH_RETRY_TIMEOUT,
                        SYS_MS_TO_TICKS(LIM_AUTH_RETRY_TIMER_MS), 0,
                        TX_NO_ACTIVATE)) != TX_SUCCESS)
        {
            /// Could not create Periodic Join Probe Request timer.
            // Log error
            limLog(pMac, LOGP, FL("could not create Periodic AUTH Timer timer"));
            goto err_timer;
        }
        if (wlan_cfgGetInt(pMac, WNI_CFG_ASSOCIATION_FAILURE_TIMEOUT,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get AssocFailureTimeout value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve AssocFailureTimeout value"));
        }
        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        // Create Association failure timer and activate it later
        if (tx_timer_create(&pMac->lim.limTimers.gLimAssocFailureTimer,
                        "ASSOC FAILURE TIMEOUT",
                        limAssocFailureTimerHandler, LIM_ASSOC,
                        cfgValue, 0,
                        TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not create Assoc failure timer.
            // Log error
            limLog(pMac, LOGP,
               FL("could not create Association failure timer"));

            goto err_timer;
        }
        if (wlan_cfgGetInt(pMac, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get ReassocFailureTimeout value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve ReassocFailureTimeout value"));
        }
        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        // Create Association failure timer and activate it later
        if (tx_timer_create(&pMac->lim.limTimers.gLimReassocFailureTimer,
                            "REASSOC FAILURE TIMEOUT",
                            limAssocFailureTimerHandler, LIM_REASSOC,
                            cfgValue, 0,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not create Reassoc failure timer.
            // Log error
            limLog(pMac, LOGP,
               FL("could not create Reassociation failure timer"));

            goto err_timer;
        }

        if (wlan_cfgGetInt(pMac, WNI_CFG_ADDTS_RSP_TIMEOUT, &cfgValue) != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("Fail to get WNI_CFG_ADDTS_RSP_TIMEOUT "));

        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        // Create Addts response timer and activate it later
        if (tx_timer_create(&pMac->lim.limTimers.gLimAddtsRspTimer,
                            "ADDTS RSP TIMEOUT",
                            limAddtsResponseTimerHandler,
                            SIR_LIM_ADDTS_RSP_TIMEOUT,
                            cfgValue, 0,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not create Auth failure timer.
            // Log error
            limLog(pMac, LOGP, FL("could not create Addts response timer"));

            goto err_timer;
        }

        if (wlan_cfgGetInt(pMac, WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get AuthFailureTimeout value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve AuthFailureTimeout value"));
        }
        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        // Create Auth failure timer and activate it later
        if (tx_timer_create(&pMac->lim.limTimers.gLimAuthFailureTimer,
                            "AUTH FAILURE TIMEOUT",
                            limTimerHandler,
                            SIR_LIM_AUTH_FAIL_TIMEOUT,
                            cfgValue, 0,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not create Auth failure timer.
            // Log error
            limLog(pMac, LOGP, FL("could not create Auth failure timer"));

            goto err_timer;
        }

        if (wlan_cfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get BEACON_INTERVAL value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve BEACON_INTERVAL value"));
        }
        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        if (tx_timer_create(&pMac->lim.limTimers.gLimHeartBeatTimer,
                            "Heartbeat TIMEOUT",
                            limTimerHandler,
                            SIR_LIM_HEART_BEAT_TIMEOUT,
                            cfgValue,
                            0,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not start Heartbeat timer.
            // Log error
            limLog(pMac, LOGP,
               FL("call to create heartbeat timer failed"));
            goto err_timer;
        }

        if (wlan_cfgGetInt(pMac, WNI_CFG_PROBE_AFTER_HB_FAIL_TIMEOUT,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get PROBE_AFTER_HB_FAILURE
             * value from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve PROBE_AFTER_HB_FAIL_TIMEOUT value"));
        }

        // Change timer to reactivate it in future
        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        if (tx_timer_create(&pMac->lim.limTimers.gLimProbeAfterHBTimer,
                            "Probe after Heartbeat TIMEOUT",
                            limTimerHandler,
                            SIR_LIM_PROBE_HB_FAILURE_TIMEOUT,
                            cfgValue,
                            0,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            // Could not creat wt-probe-after-HeartBeat-failure timer.
            // Log error
            limLog(pMac, LOGP,
                   FL("unable to create ProbeAfterHBTimer"));
            goto err_timer;
        }

        if (wlan_cfgGetInt(pMac, WNI_CFG_BACKGROUND_SCAN_PERIOD,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get Background scan period value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve Background scan period value"));
        }

        /*
         * setting period to zero means disabling background scans when associated
         * the way we do this is to set a flag indicating this and keeping
         * the timer running, since it will be used for PDU leak workarounds
         * as well as background scanning during SME idle states
         */
        if (cfgValue == 0)
        {
            cfgValue = LIM_BACKGROUND_SCAN_PERIOD_DEFAULT_MS;
            pMac->lim.gLimBackgroundScanDisable = true;
        }
        else
            pMac->lim.gLimBackgroundScanDisable = false;

        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        if (tx_timer_create(&pMac->lim.limTimers.gLimBackgroundScanTimer,
                            "Background scan TIMEOUT",
                            limTimerHandler,
                            SIR_LIM_CHANNEL_SCAN_TIMEOUT,
                            cfgValue,
                            cfgValue,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not start background scan timer.
            // Log error
            limLog(pMac, LOGP,
               FL("call to create background scan timer failed"));
            goto err_timer;
        }
    }

    /**
     * Create keepalive timer and  activate it right away for AP role
     */

    if (wlan_cfgGetInt(pMac, WNI_CFG_KEEPALIVE_TIMEOUT,
                  &cfgValue) != eSIR_SUCCESS)
    {
        /**
         * Could not get keepalive timeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGP,
               FL("could not retrieve keepalive timeout value"));
    }

    // A value of zero implies keep alive should be disabled
    if (cfgValue == 0)
    {
        cfgValue = LIM_KEEPALIVE_TIMER_MS;
        pMac->sch.keepAlive = 0;
    } else
        pMac->sch.keepAlive = 1;


    cfgValue = SYS_MS_TO_TICKS(cfgValue + SYS_TICK_DUR_MS - 1);

    if (tx_timer_create(&pMac->lim.limTimers.gLimKeepaliveTimer,
                        "KEEPALIVE_TIMEOUT",
                        limKeepaliveTmerHandler,
                        0,
                        cfgValue,
                        cfgValue,
                        (pMac->lim.gLimSystemRole == eLIM_AP_ROLE) ?
                         TX_AUTO_ACTIVATE : TX_NO_ACTIVATE)
                  != TX_SUCCESS)
    {
        // Cannot create keepalive timer.  Log error.
        limLog(pMac, LOGP, FL("Cannot create keepalive timer."));
        goto err_timer;
    }

    /**
     * Create all CNF_WAIT Timers upfront
     */

    if (wlan_cfgGetInt(pMac, WNI_CFG_WT_CNF_TIMEOUT,
                  &cfgValue) != eSIR_SUCCESS)
    {
        /**
         * Could not get CNF_WAIT timeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGP,
               FL("could not retrieve CNF timeout value"));
    }
    cfgValue = SYS_MS_TO_TICKS(cfgValue);

    for (i=0; i<pMac->lim.maxStation; i++)
    {
        if (tx_timer_create(&pMac->lim.limTimers.gpLimCnfWaitTimer[i],
                            "CNF_MISS_TIMEOUT",
                            limCnfWaitTmerHandler,
                            (tANI_U32)i,
                            cfgValue,
                            0,
                            TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            // Cannot create timer.  Log error.
            limLog(pMac, LOGP, FL("Cannot create CNF wait timer."));
            goto err_timer;
        }
    }

    /*
    ** Alloc and init table for the preAuth timer list
    **
    **/

    // get max number of Preauthentication
    if (wlan_cfgGetInt(pMac, WNI_CFG_MAX_NUM_PRE_AUTH,
             &cfgValue) != eSIR_SUCCESS)
    {
        /*
        ** Could not get max preauth value
        ** from CFG. Log error.
        **/
        limLog(pMac, LOGP,
               FL("could not retrieve mac preauth value"));
    }
    pMac->lim.gLimPreAuthTimerTable.numEntry = cfgValue;
    pMac->lim.gLimPreAuthTimerTable.pTable = vos_mem_vmalloc(cfgValue*sizeof(tLimPreAuthNode));
    if(pMac->lim.gLimPreAuthTimerTable.pTable == NULL)
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed!"));
        goto err_timer;
    }

    limInitPreAuthTimerTable(pMac, &pMac->lim.gLimPreAuthTimerTable);
    PELOG1(limLog(pMac, LOG1, FL("alloc and init table for preAuth timers"));)


    {
        /**
         * Create OLBC cache aging timer
         */
        if (wlan_cfgGetInt(pMac, WNI_CFG_OLBC_DETECT_TIMEOUT,
                      &cfgValue) != eSIR_SUCCESS)
        {
            /**
             * Could not get OLBC detect timeout value
             * from CFG. Log error.
             */
            limLog(pMac, LOGP,
               FL("could not retrieve OLBD detect timeout value"));
        }

        cfgValue = SYS_MS_TO_TICKS(cfgValue);

        if (tx_timer_create(
                &pMac->lim.limTimers.gLimUpdateOlbcCacheTimer,
                "OLBC UPDATE CACHE TIMEOUT",
                limUpdateOlbcCacheTimerHandler,
                SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT,
                cfgValue,
                cfgValue,
                TX_NO_ACTIVATE) != TX_SUCCESS)
        {
            // Cannot create update OLBC cache timer
            // Log error
            limLog(pMac, LOGP, FL("Cannot create update OLBC cache timer"));
            goto err_timer;
        }
    }
#ifdef WLAN_FEATURE_VOWIFI_11R
    // In future we need to use the auth timer, cause
    // the pre auth session will be introduced before sending
    // Auth frame.
    // We need to go off channel and come back to home channel
    cfgValue = 1000;
    cfgValue = SYS_MS_TO_TICKS(cfgValue);

    if (tx_timer_create(&pMac->lim.limTimers.gLimFTPreAuthRspTimer,
                                    "FT PREAUTH RSP TIMEOUT",
                                    limTimerHandler, SIR_LIM_FT_PREAUTH_RSP_TIMEOUT,
                                    cfgValue, 0,
                                    TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        // Could not create Join failure timer.
        // Log error
        limLog(pMac, LOGP, FL("could not create Join failure timer"));
        goto err_timer;
    }
#endif

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
    cfgValue = 5000;
    cfgValue = SYS_MS_TO_TICKS(cfgValue);

    if (tx_timer_create(&pMac->lim.limTimers.gLimEseTsmTimer,
                                    "ESE TSM Stats TIMEOUT",
                                    limTimerHandler, SIR_LIM_ESE_TSM_TIMEOUT,
                                    cfgValue, 0,
                                    TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        // Could not create Join failure timer.
        // Log error
        limLog(pMac, LOGP, FL("could not create Join failure timer"));
        goto err_timer;
    }
#endif /* FEATURE_WLAN_ESE && !FEATURE_WLAN_ESE_UPLOAD */

    cfgValue = 1000;
    cfgValue = SYS_MS_TO_TICKS(cfgValue);
    if (tx_timer_create(&pMac->lim.limTimers.gLimDisassocAckTimer,
                                    "DISASSOC ACK TIMEOUT",
                                    limTimerHandler, SIR_LIM_DISASSOC_ACK_TIMEOUT,
                                    cfgValue, 0,
                                    TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not DISASSOC ACK TIMEOUT timer"));
        goto err_timer;
    }

    cfgValue = 1000;
    cfgValue = SYS_MS_TO_TICKS(cfgValue);
    if (tx_timer_create(&pMac->lim.limTimers.gLimDeauthAckTimer,
                                    "DISASSOC ACK TIMEOUT",
                                    limTimerHandler, SIR_LIM_DEAUTH_ACK_TIMEOUT,
                                    cfgValue, 0,
                                    TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not create DEAUTH ACK TIMEOUT timer"));
        goto err_timer;
    }

    cfgValue = LIM_INSERT_SINGLESHOTNOA_TIMEOUT_VALUE; // (> no of BI* no of TUs per BI * 1TU in msec + p2p start time offset*1 TU in msec = 2*100*1.024 + 5*1.024 = 204.8 + 5.12 = 209.20)
    cfgValue = SYS_MS_TO_TICKS(cfgValue);
    if (tx_timer_create(&pMac->lim.limTimers.gLimP2pSingleShotNoaInsertTimer,
                                    "Single Shot NOA Insert timeout",
                                    limTimerHandler, SIR_LIM_INSERT_SINGLESHOT_NOA_TIMEOUT,
                                    cfgValue, 0,
                                    TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not create Single Shot NOA Insert Timeout timer"));
        goto err_timer;
    }

    cfgValue = ACTIVE_TO_PASSIVE_CONVERISON_TIMEOUT;
    cfgValue = SYS_MS_TO_TICKS(cfgValue);
    if (tx_timer_create(&pMac->lim.limTimers.gLimActiveToPassiveChannelTimer,
                                  "ACTIVE TO PASSIVE CHANNEL", limTimerHandler,
                 SIR_LIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE, cfgValue, 0,
                 TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        limLog(pMac, LOGW,FL("could not create timer for passive channel to active channel"));
        goto err_timer;
    }


    return TX_SUCCESS;

    err_timer:
        tx_timer_delete(&pMac->lim.limTimers.gLimDeauthAckTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimDisassocAckTimer);
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
        tx_timer_delete(&pMac->lim.limTimers.gLimEseTsmTimer);
#endif /* FEATURE_WLAN_ESE && !FEATURE_WLAN_ESE_UPLOAD */
        tx_timer_delete(&pMac->lim.limTimers.gLimFTPreAuthRspTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimUpdateOlbcCacheTimer);
        while(((tANI_S32)--i) >= 0)
        {
            tx_timer_delete(&pMac->lim.limTimers.gpLimCnfWaitTimer[i]);
        }
        tx_timer_delete(&pMac->lim.limTimers.gLimKeepaliveTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimBackgroundScanTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimProbeAfterHBTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimHeartBeatTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimAuthFailureTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimAddtsRspTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimReassocFailureTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimAssocFailureTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimJoinFailureTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimPeriodicJoinProbeReqTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimPeriodicAuthRetryTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimQuietBssTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimQuietTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimChannelSwitchTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimMaxChannelTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimPeriodicProbeReqTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimMinChannelTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimP2pSingleShotNoaInsertTimer);
        tx_timer_delete(&pMac->lim.limTimers.gLimActiveToPassiveChannelTimer);

        if(NULL != pMac->lim.gLimPreAuthTimerTable.pTable)
        {
            vos_mem_vfree(pMac->lim.gLimPreAuthTimerTable.pTable);
            pMac->lim.gLimPreAuthTimerTable.pTable = NULL;
        }

        return TX_TIMER_ERROR;

} /****** end limCreateTimers() ******/



/**
 * limTimerHandler()
 *
 *FUNCTION:
 * This function is called upon
 * 1. MIN_CHANNEL, MAX_CHANNEL timer expiration during scanning
 * 2. JOIN_FAILURE timer expiration while joining a BSS
 * 3. AUTH_FAILURE timer expiration while authenticating with a peer
 * 4. Heartbeat timer expiration on STA
 * 5. Background scan timer expiration on STA
 * 6. AID release, Pre-auth cleanup and Link monitoring timer
 *    expiration on AP
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  param - Message corresponding to the timer that expired
 *
 * @return None
 */

void
limTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tANI_U32         statusCode;
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    // Prepare and post message to LIM Message Queue

    msg.type = (tANI_U16) param;
    msg.bodyptr = NULL;
    msg.bodyval = 0;

    if ((statusCode = limPostMsgApiHighPri(pMac, &msg)) != eSIR_SUCCESS)
        limLog(pMac, LOGE,
               FL("posting message %X to LIM failed, reason=%d"),
               msg.type, statusCode);
} /****** end limTimerHandler() ******/


/**
 * limAddtsResponseTimerHandler()
 *
 *FUNCTION:
 * This function is called upon Addts response timer expiration on sta
 *
 *LOGIC:
 * Message SIR_LIM_ADDTS_RSP_TIMEOUT is posted to gSirLimMsgQ
 * when this function is executed.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  param - pointer to pre-auth node
 *
 * @return None
 */

void
limAddtsResponseTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    // Prepare and post message to LIM Message Queue

    msg.type = SIR_LIM_ADDTS_RSP_TIMEOUT;
    msg.bodyval = param;
    msg.bodyptr = NULL;

    limPostMsgApi(pMac, &msg);
} /****** end limAuthResponseTimerHandler() ******/


/**
 * limAuthResponseTimerHandler()
 *
 *FUNCTION:
 * This function is called upon Auth response timer expiration on AP
 *
 *LOGIC:
 * Message SIR_LIM_AUTH_RSP_TIMEOUT is posted to gSirLimMsgQ
 * when this function is executed.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  param - pointer to pre-auth node
 *
 * @return None
 */

void
limAuthResponseTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    // Prepare and post message to LIM Message Queue

    msg.type = SIR_LIM_AUTH_RSP_TIMEOUT;
    msg.bodyptr = NULL;
    msg.bodyval = (tANI_U32)param;

    limPostMsgApi(pMac, &msg);
} /****** end limAuthResponseTimerHandler() ******/



/**
 * limAssocFailureTimerHandler()
 *
 *FUNCTION:
 * This function is called upon Re/Assoc failure timer expiration
 * on STA
 *
 *LOGIC:
 * Message SIR_LIM_ASSOC_FAIL_TIMEOUT is posted to gSirLimMsgQ
 * when this function is executed.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  param - Indicates whether this is assoc or reassoc
 *                 failure timeout
 * @return None
 */

void
limAssocFailureTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    if((LIM_REASSOC == param) &&
       (NULL != pMac->lim.pSessionEntry) &&
       (pMac->lim.pSessionEntry->limMlmState == eLIM_MLM_WT_FT_REASSOC_RSP_STATE))
    {
        limLog(pMac, LOGE, FL("Reassoc timeout happened"));
#ifdef FEATURE_WLAN_ESE
        if (((pMac->lim.pSessionEntry->isESEconnection) &&
             (pMac->lim.reAssocRetryAttempt <
             (LIM_MAX_REASSOC_RETRY_LIMIT - 1)))||
            ((!pMac->lim.pSessionEntry->isESEconnection) &&
             (pMac->lim.reAssocRetryAttempt < LIM_MAX_REASSOC_RETRY_LIMIT))
           )
#else
        if (pMac->lim.reAssocRetryAttempt < LIM_MAX_REASSOC_RETRY_LIMIT)
#endif
        {
            limSendRetryReassocReqFrame(pMac, pMac->lim.pSessionEntry->pLimMlmReassocRetryReq, pMac->lim.pSessionEntry);
            pMac->lim.reAssocRetryAttempt++;
            limLog(pMac, LOGW, FL("Reassoc request retry is sent %d times"), pMac->lim.reAssocRetryAttempt);
            return;
        }
        else
        {
            limLog(pMac, LOGW, FL("Reassoc request retry MAX(%d) reached"), LIM_MAX_REASSOC_RETRY_LIMIT);
            if(NULL != pMac->lim.pSessionEntry->pLimMlmReassocRetryReq)
            {
                vos_mem_free( pMac->lim.pSessionEntry->pLimMlmReassocRetryReq);
                pMac->lim.pSessionEntry->pLimMlmReassocRetryReq = NULL;
            }
        }
    }
#endif
    // Prepare and post message to LIM Message Queue

    msg.type = SIR_LIM_ASSOC_FAIL_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;

    limPostMsgApi(pMac, &msg);
} /****** end limAssocFailureTimerHandler() ******/


/**
 * limUpdateOlbcCacheTimerHandler()
 *
 *FUNCTION:
 * This function is called upon update olbc cache timer expiration
 * on STA
 *
 *LOGIC:
 * Message SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT is posted to gSirLimMsgQ
 * when this function is executed.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param
 *
 * @return None
 */
void
limUpdateOlbcCacheTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    // Prepare and post message to LIM Message Queue

    msg.type = SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT;
    msg.bodyval = 0;
    msg.bodyptr = NULL;

    limPostMsgApi(pMac, &msg);
} /****** end limUpdateOlbcCacheTimerHandler() ******/

/**
 * limDeactivateAndChangeTimer()
 *
 *FUNCTION:
 * This function is called to deactivate and change a timer
 * for future re-activation
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  timerId - enum of timer to be deactivated and changed
 *                   This enum is defined in limUtils.h file
 *
 * @return None
 */

void
limDeactivateAndChangeTimer(tpAniSirGlobal pMac, tANI_U32 timerId)
{
    tANI_U32    val=0, val1=0;
    tpPESession  psessionEntry;

    switch (timerId)
    {
        case eLIM_ADDTS_RSP_TIMER:
            pMac->lim.gLimAddtsRspTimerCount++;
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimAddtsRspTimer) != TX_SUCCESS)
            {
                // Could not deactivate AddtsRsp Timer
                // Log error
                limLog(pMac, LOGP,
                       FL("Unable to deactivate AddtsRsp timer"));
            }
            break;

        case eLIM_MIN_CHANNEL_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimMinChannelTimer)
                                         != TX_SUCCESS)
            {
                // Could not deactivate min channel timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("Unable to deactivate min channel timer"));
            }

#if 0
            // If a background was triggered via Quiet BSS,
            // then we need to adjust the MIN and MAX channel
            // timer's accordingly to the Quiet duration that
            // was specified
            if( eLIM_QUIET_RUNNING == pMac->lim.gLimSpecMgmt.quietState &&
                pMac->lim.gLimTriggerBackgroundScanDuringQuietBss )
            {
                // gLimQuietDuration is already cached in units of
                // system ticks. No conversion is reqd...
                val = pMac->lim.gLimSpecMgmt.quietDuration;
            }
            else
            {
#endif
                if(pMac->lim.gpLimMlmScanReq)
                {
                    val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->minChannelTime);
                    if (pMac->btc.btcScanCompromise)
                    {
                        if (pMac->lim.gpLimMlmScanReq->minChannelTimeBtc)
                        {
                            val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->minChannelTimeBtc);
                            limLog(pMac, LOG1, FL("Using BTC Min Active Scan time"));
                        }
                        else
                        {
                            limLog(pMac, LOGE, FL("BTC Active Scan Min Time is Not Set"));
                        }
                    }
                }
                else
                {
                    limLog(pMac, LOGE, FL(" gpLimMlmScanReq is NULL "));
                    //No need to change min timer. This is not a scan
                    break;
                }
#if 0
            }
#endif

            if (tx_timer_change(&pMac->lim.limTimers.gLimMinChannelTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change min channel timer.
                // Log error
                limLog(pMac, LOGP, FL("Unable to change min channel timer"));
            }

            break;

        case eLIM_PERIODIC_PROBE_REQ_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimPeriodicProbeReqTimer)
                                         != TX_SUCCESS)
            {
                // Could not deactivate min channel timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("Unable to deactivate periodic timer"));
            }
            if(pMac->lim.gpLimMlmScanReq)
            {
                val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->minChannelTime)/2;
                if (pMac->btc.btcScanCompromise)
                {
                    if (pMac->lim.gpLimMlmScanReq->minChannelTimeBtc)
                    {
                        val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->minChannelTimeBtc)/2;
                        limLog(pMac, LOG1, FL("Using BTC Min Active Scan time"));
                    }
                    else
                    {
                        limLog(pMac, LOGE, FL("BTC Active Scan Min Time is Not Set"));
                    }
                }
            }
            /*If val is 0 it means min Channel timer is 0 so take the value from maxChannelTimer*/
            if (!val)
            {

                if(pMac->lim.gpLimMlmScanReq)
                {
                    val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->maxChannelTime)/2;
                    if (pMac->btc.btcScanCompromise)
                    {
                        if (pMac->lim.gpLimMlmScanReq->maxChannelTimeBtc)
                        {
                            val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->maxChannelTimeBtc)/2;
                            limLog(pMac, LOG1, FL("Using BTC Max Active Scan time"));
                        }
                        else
                        {
                            limLog(pMac, LOGE, FL("BTC Active Scan Max Time is Not Set"));
                        }
                    }
                }
                else
                {
                    limLog(pMac, LOGE, FL(" gpLimMlmScanReq is NULL "));
                    //No need to change max timer. This is not a scan
                    break;
                }
            }
            if (val)
            {
                if (tx_timer_change(&pMac->lim.limTimers.gLimPeriodicProbeReqTimer,
                                    val, 0) != TX_SUCCESS)
                {
                    // Could not change min channel timer.
                    // Log error
                    limLog(pMac, LOGE, FL("Unable to change periodic timer"));
                }
            }
            else
            {
                limLog(pMac, LOGE, FL("Do not change gLimPeriodicProbeReqTimer values,"
                       "value = %d minchannel time = %d"
                       "maxchannel time = %d"), val,
                       pMac->lim.gpLimMlmScanReq->minChannelTime,
                       pMac->lim.gpLimMlmScanReq->maxChannelTime);
            }

            break;

        case eLIM_MAX_CHANNEL_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimMaxChannelTimer)
                                      != TX_SUCCESS)
            {
                // Could not deactivate max channel timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("Unable to deactivate max channel timer"));
            }

            // If a background was triggered via Quiet BSS,
            // then we need to adjust the MIN and MAX channel
            // timer's accordingly to the Quiet duration that
            // was specified
            if (pMac->lim.gLimSystemRole != eLIM_AP_ROLE)
            {
#if 0

                if( eLIM_QUIET_RUNNING == pMac->lim.gLimSpecMgmt.quietState &&
                    pMac->lim.gLimTriggerBackgroundScanDuringQuietBss )
                {
                    // gLimQuietDuration is already cached in units of
                    // system ticks. No conversion is reqd...
                    val = pMac->lim.gLimSpecMgmt.quietDuration;
                }
                else
                {
#endif
                    if(pMac->lim.gpLimMlmScanReq)
                    {
                        val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->maxChannelTime);
                        if (pMac->btc.btcScanCompromise)
                        {
                            if (pMac->lim.gpLimMlmScanReq->maxChannelTimeBtc)
                            {
                                val = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->maxChannelTimeBtc);
                                limLog(pMac, LOG1, FL("Using BTC Max Active Scan time"));
                            }
                            else
                            {
                                limLog(pMac, LOGE, FL("BTC Active Scan Max Time is Not Set"));
                            }
                        }
                    }
                    else
                    {
                        limLog(pMac, LOGE, FL(" gpLimMlmScanReq is NULL "));
                        //No need to change max timer. This is not a scan
                        break;
                    }
#if 0
                }
#endif
            }

            if (tx_timer_change(&pMac->lim.limTimers.gLimMaxChannelTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change max channel timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("Unable to change max channel timer"));
            }

            break;

        case eLIM_JOIN_FAIL_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimJoinFailureTimer)
                                         != TX_SUCCESS)
            {
                /**
                 * Could not deactivate Join Failure
                 * timer. Log error.
                 */
                limLog(pMac, LOGP,
                       FL("Unable to deactivate Join Failure timer"));
            }

            if (wlan_cfgGetInt(pMac, WNI_CFG_JOIN_FAILURE_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get JoinFailureTimeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve JoinFailureTimeout value"));
            }
            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gLimJoinFailureTimer,
                                val, 0) != TX_SUCCESS)
            {
                /**
                 * Could not change Join Failure
                 * timer. Log error.
                 */
                limLog(pMac, LOGP,
                       FL("Unable to change Join Failure timer"));
            }

            break;

        case eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimPeriodicJoinProbeReqTimer)
                                         != TX_SUCCESS)
            {
                // Could not deactivate periodic join req Times.
                limLog(pMac, LOGP,
                       FL("Unable to deactivate periodic join request timer"));
            }

            val = SYS_MS_TO_TICKS(LIM_JOIN_PROBE_REQ_TIMER_MS);
            if (tx_timer_change(&pMac->lim.limTimers.gLimPeriodicJoinProbeReqTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change periodic join req times.
                // Log error
                limLog(pMac, LOGP, FL("Unable to change periodic join request timer"));
            }

            break;

        case eLIM_AUTH_FAIL_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimAuthFailureTimer)
                                              != TX_SUCCESS)
            {
                // Could not deactivate Auth failure timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("Unable to deactivate auth failure timer"));
            }

            // Change timer to reactivate it in future
            if (wlan_cfgGetInt(pMac, WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get AuthFailureTimeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve AuthFailureTimeout value"));
            }
            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gLimAuthFailureTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change Authentication failure timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("unable to change Auth failure timer"));
            }

            break;

        case eLIM_AUTH_RETRY_TIMER:

            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimPeriodicAuthRetryTimer)
                                         != TX_SUCCESS)
            {
                // Could not deactivate Auth Retry Timer.
                limLog(pMac, LOGP,
                       FL("Unable to deactivate Auth Retry timer"));
            }
            if ((psessionEntry = peFindSessionBySessionId(pMac,
                pMac->lim.limTimers.gLimPeriodicAuthRetryTimer.sessionId))
                                                                      == NULL)
            {
                   limLog(pMac, LOGP,
                     FL("session does not exist for given SessionId : %d"),
                     pMac->lim.limTimers.gLimPeriodicAuthRetryTimer.sessionId);
                   break;
            }
            /* 3/5 of the beacon interval*/
            val = psessionEntry->beaconParams.beaconInterval * 3/5;
            val = SYS_MS_TO_TICKS(val);
            if (tx_timer_change(&pMac->lim.limTimers.gLimPeriodicAuthRetryTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change Auth Retry timer.
                // Log error
                limLog(pMac, LOGP, FL("Unable to change Auth Retry timer"));
            }

            break;

        case eLIM_ASSOC_FAIL_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimAssocFailureTimer) !=
                                    TX_SUCCESS)
            {
                // Could not deactivate Association failure timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to deactivate Association failure timer"));
            }

            // Change timer to reactivate it in future
            if (wlan_cfgGetInt(pMac, WNI_CFG_ASSOCIATION_FAILURE_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get AssocFailureTimeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve AssocFailureTimeout value"));
            }
            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gLimAssocFailureTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change Association failure timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("unable to change Assoc failure timer"));
            }

            break;

        case eLIM_REASSOC_FAIL_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimReassocFailureTimer) !=
                                    TX_SUCCESS)
            {
                // Could not deactivate Reassociation failure timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to deactivate Reassoc failure timer"));
            }

            // Change timer to reactivate it in future
            if (wlan_cfgGetInt(pMac, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get ReassocFailureTimeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve ReassocFailureTimeout value"));
            }
            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gLimReassocFailureTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change Reassociation failure timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to change Reassociation failure timer"));
            }

            break;

        case eLIM_HEART_BEAT_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimHeartBeatTimer) !=
                                    TX_SUCCESS)
            {
                // Could not deactivate Heartbeat timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("unable to deactivate Heartbeat timer"));
            }
            else
            {
                limLog(pMac, LOGW, FL("Deactivated heartbeat link monitoring"));
            }

            if (wlan_cfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get BEACON_INTERVAL value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                       FL("could not retrieve BEACON_INTERVAL value"));
            }

            if (wlan_cfgGetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, &val1) !=
                                                          eSIR_SUCCESS)
                limLog(pMac, LOGP,
                   FL("could not retrieve heartbeat failure value"));

            // Change timer to reactivate it in future
            val = SYS_MS_TO_TICKS(val * val1);

            if (tx_timer_change(&pMac->lim.limTimers.gLimHeartBeatTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change HeartBeat timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("unable to change HeartBeat timer"));
            }
            else
            {
                limLog(pMac, LOGW, FL("HeartBeat timer value is changed = %u"), val);
            }
            break;

        case eLIM_PROBE_AFTER_HB_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimProbeAfterHBTimer) !=
                                    TX_SUCCESS)
            {
                // Could not deactivate Heartbeat timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to deactivate probeAfterHBTimer"));
            }
            else
            {
                limLog(pMac, LOG1, FL("Deactivated probe after hb timer"));
            }

            if (wlan_cfgGetInt(pMac, WNI_CFG_PROBE_AFTER_HB_FAIL_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get PROBE_AFTER_HB_FAILURE
                 * value from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve PROBE_AFTER_HB_FAIL_TIMEOUT value"));
            }

            // Change timer to reactivate it in future
            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gLimProbeAfterHBTimer,
                                val, 0) != TX_SUCCESS)
            {
                // Could not change HeartBeat timer.
                // Log error
                limLog(pMac, LOGP,
                       FL("unable to change ProbeAfterHBTimer"));
            }
            else
            {
                limLog(pMac, LOGW, FL("Probe after HB timer value is changed = %u"), val);
            }

            break;

        case eLIM_KEEPALIVE_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimKeepaliveTimer)
                            != TX_SUCCESS)
            {
                // Could not deactivate Keepalive timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to deactivate KeepaliveTimer timer"));
            }

            // Change timer to reactivate it in future

            if (wlan_cfgGetInt(pMac, WNI_CFG_KEEPALIVE_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get keepalive timeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve keepalive timeout value"));
            }
            if (val == 0)
            {
                val = 3000;
                pMac->sch.keepAlive = 0;
            } else
                pMac->sch.keepAlive = 1;



            val = SYS_MS_TO_TICKS(val + SYS_TICK_DUR_MS - 1);

            if (tx_timer_change(&pMac->lim.limTimers.gLimKeepaliveTimer,
                                val, val) != TX_SUCCESS)
            {
                // Could not change KeepaliveTimer timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to change KeepaliveTimer timer"));
            }

            break;

        case eLIM_BACKGROUND_SCAN_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimBackgroundScanTimer)
                            != TX_SUCCESS)
            {
                // Could not deactivate BackgroundScanTimer timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to deactivate BackgroundScanTimer timer"));
            }

            // Change timer to reactivate it in future
            if (wlan_cfgGetInt(pMac, WNI_CFG_BACKGROUND_SCAN_PERIOD,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get Background scan period value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve Background scan period value"));
            }
            if (val == 0)
            {
                val = LIM_BACKGROUND_SCAN_PERIOD_DEFAULT_MS;
                pMac->lim.gLimBackgroundScanDisable = true;
            }
            else
                pMac->lim.gLimBackgroundScanDisable = false;

            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gLimBackgroundScanTimer,
                                val, val) != TX_SUCCESS)
            {
                // Could not change BackgroundScanTimer timer.
                // Log error
                limLog(pMac, LOGP,
                   FL("unable to change BackgroundScanTimer timer"));
            }

            break;

#if 0
        case eLIM_CHANNEL_SWITCH_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimChannelSwitchTimer) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGP, FL("tx_timer_deactivate failed!"));
                return;
            }

            if (tx_timer_change(&pMac->lim.limTimers.gLimChannelSwitchTimer,
                        pMac->lim.gLimChannelSwitch.switchTimeoutValue,
                                    0) != TX_SUCCESS)
            {
                limLog(pMac, LOGP, FL("tx_timer_change failed "));
                return;
            }
            break;
#endif

        case eLIM_LEARN_DURATION_TIMER:
            break;

#if 0
        case eLIM_QUIET_BSS_TIMER:
            if (TX_SUCCESS !=
            tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietBssTimer))
            {
                limLog( pMac, LOGE,
                  FL("Unable to de-activate gLimQuietBssTimer! Will attempt to activate anyway..."));
            }

            // gLimQuietDuration appears to be in units of ticks
            // Use it as is
            if (TX_SUCCESS !=
                tx_timer_change( &pMac->lim.limTimers.gLimQuietBssTimer,
                  pMac->lim.gLimSpecMgmt.quietDuration,
                  0))
            {
                limLog( pMac, LOGE,
                  FL("Unable to change gLimQuietBssTimer! Will still attempt to activate anyway..."));
            }
            break;

        case eLIM_QUIET_TIMER:
            if( TX_SUCCESS != tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietTimer))
            {
                limLog( pMac, LOGE,
                    FL( "Unable to deactivate gLimQuietTimer! Will still attempt to re-activate anyway..." ));
            }

            // Set the NEW timeout value, in ticks
            if( TX_SUCCESS != tx_timer_change( &pMac->lim.limTimers.gLimQuietTimer,
                              SYS_MS_TO_TICKS(pMac->lim.gLimSpecMgmt.quietTimeoutValue), 0))
            {
                limLog( pMac, LOGE,
                    FL( "Unable to change gLimQuietTimer! Will still attempt to re-activate anyway..." ));
            }
            break;
#endif

#if 0
        case eLIM_WPS_OVERLAP_TIMER:
            {
            // Restart Learn Interval timer

              tANI_U32 WPSOverlapTimer = SYS_MS_TO_TICKS(LIM_WPS_OVERLAP_TIMER_MS);

              if (tx_timer_deactivate(
                     &pMac->lim.limTimers.gLimWPSOverlapTimerObj.gLimWPSOverlapTimer) != TX_SUCCESS)
              {
                  // Could not deactivate Learn Interval timer.
                  // Log error
                  limLog(pMac, LOGP,
                         FL("Unable to deactivate WPS overlap timer"));
              }

              if (tx_timer_change(
                         &pMac->lim.limTimers.gLimWPSOverlapTimerObj.gLimWPSOverlapTimer,
                         WPSOverlapTimer, 0) != TX_SUCCESS)
              {
                  // Could not change Learn Interval timer.
                  // Log error
                  limLog(pMac, LOGP, FL("Unable to change WPS overlap timer"));

                  return;
              }

              limLog( pMac, LOGE,
                  FL("Setting WPS overlap TIMER to %d ticks"),
                  WPSOverlapTimer);
            }
            break;
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
        case eLIM_FT_PREAUTH_RSP_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimFTPreAuthRspTimer) != TX_SUCCESS)
            {
                /**
                ** Could not deactivate Join Failure
                ** timer. Log error.
                **/
                limLog(pMac, LOGP, FL("Unable to deactivate Preauth response Failure timer"));
                return;
            }
            val = 1000;
            val = SYS_MS_TO_TICKS(val);
            if (tx_timer_change(&pMac->lim.limTimers.gLimFTPreAuthRspTimer,
                                                val, 0) != TX_SUCCESS)
            {
                /**
                * Could not change Join Failure
                * timer. Log error.
                */
                limLog(pMac, LOGP, FL("Unable to change Join Failure timer"));
                return;
            }
            break;
#endif
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
         case eLIM_TSM_TIMER:
             if (tx_timer_deactivate(&pMac->lim.limTimers.gLimEseTsmTimer)
                                                                != TX_SUCCESS)
             {
                 limLog(pMac, LOGE, FL("Unable to deactivate TSM timer"));
             }
             break;
#endif /* FEATURE_WLAN_ESE && !FEATURE_WLAN_ESE_UPLOAD */

    case eLIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimActiveToPassiveChannelTimer) != TX_SUCCESS)
            {
                /**
                ** Could not deactivate Active to passive channel timer.
                ** Log error.
                **/
                limLog(pMac, LOGP, FL("Unable to Deactivate "
                                      "Active to passive channel timer"));
                return;
            }
            val = ACTIVE_TO_PASSIVE_CONVERISON_TIMEOUT;
            val = SYS_MS_TO_TICKS(val);
            if (tx_timer_change(&pMac->lim.limTimers.gLimActiveToPassiveChannelTimer,
                                                val, 0) != TX_SUCCESS)
            {
                /**
                * Could not change timer to check scan type for passive channel.
                * timer. Log error.
                */
                limLog(pMac, LOGP, FL("Unable to change timer"));
                return;
            }
            break;

    case eLIM_DISASSOC_ACK_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimDisassocAckTimer) != TX_SUCCESS)
            {
                /**
                ** Could not deactivate Join Failure
                ** timer. Log error.
                **/
                limLog(pMac, LOGP, FL("Unable to deactivate Disassoc ack timer"));
                return;
            }
            val = 1000;
            val = SYS_MS_TO_TICKS(val);
            if (tx_timer_change(&pMac->lim.limTimers.gLimDisassocAckTimer,
                                                val, 0) != TX_SUCCESS)
            {
                /**
                * Could not change Join Failure
                * timer. Log error.
                */
                limLog(pMac, LOGP, FL("Unable to change timer"));
                return;
            }
            break;

    case eLIM_DEAUTH_ACK_TIMER:
            if (tx_timer_deactivate(&pMac->lim.limTimers.gLimDeauthAckTimer) != TX_SUCCESS)
            {
                /**
                ** Could not deactivate Join Failure
                ** timer. Log error.
                **/
                limLog(pMac, LOGP, FL("Unable to deactivate Deauth ack timer"));
                return;
            }
            val = 1000;
            val = SYS_MS_TO_TICKS(val);
            if (tx_timer_change(&pMac->lim.limTimers.gLimDeauthAckTimer,
                                                val, 0) != TX_SUCCESS)
            {
                /**
                * Could not change Join Failure
                * timer. Log error.
                */
                limLog(pMac, LOGP, FL("Unable to change timer"));
                return;
            }
            break;

    case eLIM_INSERT_SINGLESHOT_NOA_TIMER:
        if (tx_timer_deactivate(&pMac->lim.limTimers.gLimP2pSingleShotNoaInsertTimer) != TX_SUCCESS)
        {
            /**
       ** Could not deactivate SingleShot NOA Insert
       ** timer. Log error.
       **/
            limLog(pMac, LOGP, FL("Unable to deactivate SingleShot NOA Insert timer"));
            return;
        }
        val = LIM_INSERT_SINGLESHOTNOA_TIMEOUT_VALUE;
        val = SYS_MS_TO_TICKS(val);
        if (tx_timer_change(&pMac->lim.limTimers.gLimP2pSingleShotNoaInsertTimer,
                                            val, 0) != TX_SUCCESS)
        {
            /**
       * Could not change Single Shot NOA Insert
       * timer. Log error.
       */
            limLog(pMac, LOGP, FL("Unable to change timer"));
            return;
        }
        break;

        default:
            // Invalid timerId. Log error
            break;
    }
} /****** end limDeactivateAndChangeTimer() ******/



/**---------------------------------------------------------------
\fn     limHeartBeatDeactivateAndChangeTimer
\brief  This function deactivates and changes the heart beat
\       timer, eLIM_HEART_BEAT_TIMER.
\
\param pMac
\param psessionEntry
\return None
------------------------------------------------------------------*/
void
limHeartBeatDeactivateAndChangeTimer(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
   tANI_U32    val, val1;

   if (NULL == psessionEntry)
   {
       limLog(pMac, LOGE, FL("%s: received session id NULL."
                   " Heartbeat timer config failed"), __func__);
       return;
   }

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
   if(IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
      return;
#endif

   if (tx_timer_deactivate(&pMac->lim.limTimers.gLimHeartBeatTimer) != TX_SUCCESS)
      limLog(pMac, LOGP, FL("Fail to deactivate HeartBeatTimer "));

   /* HB Timer sessionisation: In case of 2 or more sessions, the HB interval keeps
      changing. to avoid this problem, HeartBeat interval is made constant, by
      fixing beacon interval to 100ms immaterial of the beacon interval of the session */

   //val = psessionEntry->beaconParams.beaconInterval;
   val = LIM_HB_TIMER_BEACON_INTERVAL;

   if (wlan_cfgGetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, &val1) != eSIR_SUCCESS)
      limLog(pMac, LOGP, FL("Fail to get WNI_CFG_HEART_BEAT_THRESHOLD "));

   PELOGW(limLog(pMac,LOGW,
            FL("HB Timer Int.=100ms * %d, Beacon Int.=%dms,Session Id=%d "),
            val1, psessionEntry->beaconParams.beaconInterval,
            psessionEntry->peSessionId);)

   /* The HB timer timeout value of 4 seconds (40 beacon intervals) is not
    * enough to judge the peer device inactivity when 32 peers are connected.
    * Hence increasing the HB timer timeout to
    * HBtimeout = (TBTT * num_beacons * num_peers)
    */
   if (eSIR_IBSS_MODE == psessionEntry->bssType &&
         pMac->lim.gLimNumIbssPeers > 0)
   {
      val1 = val1 * pMac->lim.gLimNumIbssPeers;
   }

   // Change timer to reactivate it in future
   val = SYS_MS_TO_TICKS(val * val1);

   if (tx_timer_change(&pMac->lim.limTimers.gLimHeartBeatTimer, val, 0) != TX_SUCCESS)
      limLog(pMac, LOGP, FL("Fail to change HeartBeatTimer"));

} /****** end limHeartBeatDeactivateAndChangeTimer() ******/


/**---------------------------------------------------------------
\fn     limReactivateHeartBeatTimer
\brief  This function s called to deactivate, change and
\       activate a timer.
\
\param pMac - Pointer to Global MAC structure
\param psessionEntry
\return None
------------------------------------------------------------------*/
void
limReactivateHeartBeatTimer(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("%s: received session id NULL."
                   " Heartbeat timer config failed"), __func__);
        return;
    }

    PELOG3(limLog(pMac, LOG3, FL("Rxed Heartbeat. Count=%d"), psessionEntry->LimRxedBeaconCntDuringHB);)

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
    if(IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
    {
       limLog(pMac, LOGW, FL("Active offload feature is enabled, FW takes care of HB monitoring"));
       return;
    }
#endif

    limHeartBeatDeactivateAndChangeTimer(pMac, psessionEntry);

    //only start the hearbeat-timer if the timeout value is non-zero
    if(pMac->lim.limTimers.gLimHeartBeatTimer.initScheduleTimeInMsecs > 0)
    {
       /*
        * There is increasing need to limit the apps wakeup due to WLAN
        * activity. During HB monitoring, the beacons from peer are sent to
        * the host causing the host to wakeup. Hence, offloading the HB
        * monitoring to LMAC
        */
       if (psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE &&
             IS_IBSS_HEARTBEAT_OFFLOAD_FEATURE_ENABLE)
       {
          if (tx_timer_deactivate(&pMac->lim.limTimers.gLimHeartBeatTimer)!= TX_SUCCESS)
          {
             limLog(pMac, LOGP,FL("IBSS HeartBeat Offloaded, Could not deactivate Heartbeat timer"));
          }
          else
          {
             limLog(pMac, LOGE, FL("IBSS HeartBeat Offloaded, Deactivated heartbeat link monitoring"));
          }
       }
       else
       {
          if (tx_timer_activate(&pMac->lim.limTimers.gLimHeartBeatTimer)!= TX_SUCCESS)
          {
             limLog(pMac, LOGP,FL("could not activate Heartbeat timer"));
          }
          else
          {
             limLog(pMac, LOGW, FL("Reactivated heartbeat link monitoring"));
          }
       }
       limResetHBPktCount(psessionEntry);
    }

} /****** end limReactivateHeartBeatTimer() ******/


/**
 * limActivateHearBeatTimer()
 *
 *
 * @brief: This function is called to activate heartbeat timer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 * @note   staId for eLIM_AUTH_RSP_TIMER is auth Node Index.
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  psessionEntry - Session Entry
 *
 * @return TX_SUCCESS - timer is activated
 *         errors - fail to start the timer
 */
v_UINT_t limActivateHearBeatTimer(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    v_UINT_t status = TX_TIMER_ERROR;

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
    if(IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
       return (TX_SUCCESS);
#endif

    if(TX_AIRGO_TMR_SIGNATURE == pMac->lim.limTimers.gLimHeartBeatTimer.tmrSignature)
    {
        //consider 0 interval a ok case
        if( pMac->lim.limTimers.gLimHeartBeatTimer.initScheduleTimeInMsecs )
        {
           if (psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE &&
               IS_IBSS_HEARTBEAT_OFFLOAD_FEATURE_ENABLE)
           {
              /* HB offload in IBSS mode */
              status = tx_timer_deactivate(&pMac->lim.limTimers.gLimHeartBeatTimer);
              if (TX_SUCCESS != status)
              {
                 PELOGE(limLog(pMac, LOGE,
                 FL("IBSS HB Offload, Could not deactivate HB timer status(%d)"),
                 status);)
              }
              else
              {
                 PELOGE(limLog(pMac, LOGE,
                 FL("%s] IBSS HB Offloaded, Heartbeat timer deactivated"),
                 __func__);)
              }

           }
           else
           {
              status = tx_timer_activate(&pMac->lim.limTimers.gLimHeartBeatTimer);
              if ( TX_SUCCESS != status )
              {
                 PELOGE(limLog(pMac, LOGE,
                 FL("could not activate Heartbeat timer status(%d)"), status);)
              }
              else
              {
                 PELOGE(limLog(pMac, LOGW,
                 FL("%s] Activated Heartbeat timer status(%d)"), __func__, status);)
              }
           }
        }
        else
        {
            status = TX_SUCCESS;
        }
    }

    return (status);
}



/**
 * limDeactivateAndChangePerStaIdTimer()
 *
 *
 * @brief: This function is called to deactivate and change a per STA timer
 * for future re-activation
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 * @note   staId for eLIM_AUTH_RSP_TIMER is auth Node Index.
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  timerId - enum of timer to be deactivated and changed
 *                   This enum is defined in limUtils.h file
 * @param  staId   - staId
 *
 * @return None
 */

void
limDeactivateAndChangePerStaIdTimer(tpAniSirGlobal pMac, tANI_U32 timerId, tANI_U16 staId)
{
    tANI_U32    val;

    switch (timerId)
    {
        case eLIM_CNF_WAIT_TIMER:

            if (tx_timer_deactivate(&pMac->lim.limTimers.gpLimCnfWaitTimer[staId])
                            != TX_SUCCESS)
            {
                 limLog(pMac, LOGP,
                       FL("unable to deactivate CNF wait timer"));

            }

            // Change timer to reactivate it in future

            if (wlan_cfgGetInt(pMac, WNI_CFG_WT_CNF_TIMEOUT,
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get cnf timeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve cnf timeout value"));
            }
            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pMac->lim.limTimers.gpLimCnfWaitTimer[staId],
                                val, val) != TX_SUCCESS)
            {
                // Could not change cnf timer.
                // Log error
                limLog(pMac, LOGP, FL("unable to change cnf wait timer"));
            }

            break;

        case eLIM_AUTH_RSP_TIMER:
        {
            tLimPreAuthNode *pAuthNode;

            pAuthNode = limGetPreAuthNodeFromIndex(pMac, &pMac->lim.gLimPreAuthTimerTable, staId);

            if (pAuthNode == NULL)
            {
                limLog(pMac, LOGP, FL("Invalid Pre Auth Index passed :%d"), staId);
                break;
            }

            if (tx_timer_deactivate(&pAuthNode->timer) != TX_SUCCESS)
            {
                // Could not deactivate auth response timer.
                // Log error
                limLog(pMac, LOGP, FL("unable to deactivate auth response timer"));
            }

            // Change timer to reactivate it in future

            if (wlan_cfgGetInt(pMac, WNI_CFG_AUTHENTICATE_RSP_TIMEOUT, &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get auth rsp timeout value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP,
                   FL("could not retrieve auth response timeout value"));
            }

            val = SYS_MS_TO_TICKS(val);

            if (tx_timer_change(&pAuthNode->timer, val, 0) != TX_SUCCESS)
            {
                // Could not change auth rsp timer.
                // Log error
                limLog(pMac, LOGP, FL("unable to change auth rsp timer"));
            }
        }
            break;


        default:
            // Invalid timerId. Log error
            break;

    }
}


/**
 * limActivateCnfTimer()
 *
 *FUNCTION:
 * This function is called to activate a per STA timer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  StaId   - staId
 *
 * @return None
 */

void limActivateCnfTimer(tpAniSirGlobal pMac, tANI_U16 staId, tpPESession psessionEntry)
{
    pMac->lim.limTimers.gpLimCnfWaitTimer[staId].sessionId = psessionEntry->peSessionId;
    if (tx_timer_activate(&pMac->lim.limTimers.gpLimCnfWaitTimer[staId])
                != TX_SUCCESS)
    {
                limLog(pMac, LOGP,
                   FL("could not activate cnf wait timer"));
    }
}

/**
 * limActivateAuthRspTimer()
 *
 *FUNCTION:
 * This function is called to activate a per STA timer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  id      - id
 *
 * @return None
 */

void limActivateAuthRspTimer(tpAniSirGlobal pMac, tLimPreAuthNode *pAuthNode)
{
    if (tx_timer_activate(&pAuthNode->timer) != TX_SUCCESS)
    {
        /// Could not activate auth rsp timer.
        // Log error
        limLog(pMac, LOGP,
               FL("could not activate auth rsp timer"));
    }
}


/**
 * limAssocCnfWaitTmerHandler()
 *
 *FUNCTION:
 *        This function post a message to send a disassociate frame out.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param
 *
 * @return None
 */

void
limCnfWaitTmerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tANI_U32         statusCode;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    msg.type = SIR_LIM_CNF_WAIT_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;

    if ((statusCode = limPostMsgApi(pMac, &msg)) != eSIR_SUCCESS)
            limLog(pMac, LOGE,
        FL("posting to LIM failed, reason=%d"), statusCode);

}

/**
 * limKeepaliveTmerHandler()
 *
 *FUNCTION:
 *        This function post a message to send a NULL data frame.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param
 *
 * @return None
 */

void
limKeepaliveTmerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tANI_U32         statusCode;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    msg.type = SIR_LIM_KEEPALIVE_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;

    if ((statusCode = limPostMsgApi(pMac, &msg)) != eSIR_SUCCESS)
        limLog(pMac, LOGE,
               FL("posting to LIM failed, reason=%d"), statusCode);

}

void
limChannelSwitchTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    limLog(pMac, LOG1,
        FL("ChannelSwitch Timer expired.  Posting msg to LIM "));

    msg.type = SIR_LIM_CHANNEL_SWITCH_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;

    limPostMsgApi(pMac, &msg);
}

void
limQuietTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    msg.type = SIR_LIM_QUIET_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;

    limLog(pMac, LOG1,
        FL("Post SIR_LIM_QUIET_TIMEOUT msg. "));
    limPostMsgApi(pMac, &msg);
}

void
limQuietBssTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    msg.type = SIR_LIM_QUIET_BSS_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;
    limLog(pMac, LOG1,
        FL("Post SIR_LIM_QUIET_BSS_TIMEOUT msg. "));
    limPostMsgApi(pMac, &msg);
}
#if 0
void
limWPSOverlapTimerHandler(void *pMacGlobal, tANI_U32 param)
{
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    msg.type = SIR_LIM_WPS_OVERLAP_TIMEOUT;
    msg.bodyval = (tANI_U32)param;
    msg.bodyptr = NULL;
    PELOG1(limLog(pMac, LOG1,
        FL("Post SIR_LIM_WPS_OVERLAP_TIMEOUT msg. "));)
    limPostMsgApi(pMac, &msg);
}
#endif

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
/* ACTIVE_MODE_HB_OFFLOAD */
/**
 * limMissedBeaconInActiveMode()
 *
 *FUNCTION:
 * This function handle beacon miss indication from FW
 * in Active mode.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  param - Msg Type
 *
 * @return None
 */
void
limMissedBeaconInActiveMode(void *pMacGlobal, tpPESession psessionEntry)
{
    tANI_U32         statusCode;
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    // Prepare and post message to LIM Message Queue
    if(IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
    {
       msg.type = (tANI_U16) SIR_LIM_HEART_BEAT_TIMEOUT;
       msg.bodyptr = psessionEntry;
       msg.bodyval = 0;
       limLog(pMac, LOGE,
                 FL("Heartbeat failure from Riva"));
       if ((statusCode = limPostMsgApi(pMac, &msg)) != eSIR_SUCCESS)
          limLog(pMac, LOGE,
                 FL("posting message %X to LIM failed, reason=%d"),
                 msg.type, statusCode);
    }
}
#endif
