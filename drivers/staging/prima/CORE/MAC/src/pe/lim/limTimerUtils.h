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
 *
 * This file limTimerUtils.h contains the utility definitions
 * LIM uses for timer handling.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_TIMER_UTILS_H
#define __LIM_TIMER_UTILS_H

#include "limTypes.h"


// Timer related functions
enum
{
    eLIM_MIN_CHANNEL_TIMER,
    eLIM_MAX_CHANNEL_TIMER,
    eLIM_JOIN_FAIL_TIMER,
    eLIM_AUTH_FAIL_TIMER,
    eLIM_AUTH_RESP_TIMER,
    eLIM_ASSOC_FAIL_TIMER,
    eLIM_REASSOC_FAIL_TIMER,
    eLIM_PRE_AUTH_CLEANUP_TIMER,
    eLIM_HEART_BEAT_TIMER,
    eLIM_BACKGROUND_SCAN_TIMER,
    eLIM_KEEPALIVE_TIMER,
    eLIM_CNF_WAIT_TIMER,
    eLIM_AUTH_RSP_TIMER,
    eLIM_UPDATE_OLBC_CACHE_TIMER,
    eLIM_PROBE_AFTER_HB_TIMER,
    eLIM_ADDTS_RSP_TIMER,
    eLIM_CHANNEL_SWITCH_TIMER,
    eLIM_LEARN_DURATION_TIMER,
    eLIM_QUIET_TIMER,
    eLIM_QUIET_BSS_TIMER,
    eLIM_WPS_OVERLAP_TIMER,
#ifdef WLAN_FEATURE_VOWIFI_11R
    eLIM_FT_PREAUTH_RSP_TIMER,
#endif
    eLIM_PERIODIC_PROBE_REQ_TIMER,
#ifdef FEATURE_WLAN_ESE
    eLIM_TSM_TIMER,
#endif
    eLIM_DISASSOC_ACK_TIMER,
    eLIM_DEAUTH_ACK_TIMER,
    eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER,
    eLIM_INSERT_SINGLESHOT_NOA_TIMER,
    eLIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE,
    eLIM_AUTH_RETRY_TIMER
};

#define LIM_DISASSOC_DEAUTH_ACK_TIMEOUT         500
#define LIM_INSERT_SINGLESHOTNOA_TIMEOUT_VALUE  500


// Timer Handler functions
v_UINT_t limCreateTimers(tpAniSirGlobal);
void limTimerHandler(void *, tANI_U32);
void limAuthResponseTimerHandler(void *, tANI_U32);
void limAssocFailureTimerHandler(void *, tANI_U32);
void limReassocFailureTimerHandler(void *, tANI_U32);

void limDeactivateAndChangeTimer(tpAniSirGlobal, tANI_U32);
void limHeartBeatDeactivateAndChangeTimer(tpAniSirGlobal, tpPESession);
void limReactivateHeartBeatTimer(tpAniSirGlobal, tpPESession);
void limDummyPktExpTimerHandler(void *, tANI_U32);
void limCnfWaitTmerHandler(void *, tANI_U32);
void limKeepaliveTmerHandler(void *, tANI_U32);
void limDeactivateAndChangePerStaIdTimer(tpAniSirGlobal, tANI_U32, tANI_U16);
void limActivateCnfTimer(tpAniSirGlobal, tANI_U16, tpPESession);
void limActivateAuthRspTimer(tpAniSirGlobal, tLimPreAuthNode *);
void limUpdateOlbcCacheTimerHandler(void *, tANI_U32);
void limAddtsResponseTimerHandler(void *, tANI_U32);
void limChannelSwitchTimerHandler(void *, tANI_U32);
void limQuietTimerHandler(void *, tANI_U32);
void limQuietBssTimerHandler(void *, tANI_U32);
void limCBScanIntervalTimerHandler(void *, tANI_U32);
void limCBScanDurationTimerHandler(void *, tANI_U32);
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
 * @param  psessionEntry - Pointer to PE session entry
 *
 * @return TX_SUCCESS - timer is activated
 *         errors - fail to start the timer
 */
v_UINT_t limActivateHearBeatTimer(tpAniSirGlobal pMac, tpPESession psessionEntry);

#if 0
void limWPSOverlapTimerHandler(void *pMacGlobal, tANI_U32 param);
#endif
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
void limMissedBeaconInActiveMode(void *pMacGlobal, tpPESession psessionEntry);
#endif
#endif /* __LIM_TIMER_UTILS_H */
