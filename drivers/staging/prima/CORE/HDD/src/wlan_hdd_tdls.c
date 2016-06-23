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

/**========================================================================

  \file  wlan_hdd_tdls.c

  \brief WLAN Host Device Driver implementation for TDLS

  ========================================================================*/

#include <wlan_hdd_includes.h>
#include <wlan_hdd_hostapd.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_cfg80211.h"
#include "vos_sched.h"

static tANI_S32 wlan_hdd_get_tdls_discovery_peer_cnt(tdlsCtx_t *pHddTdlsCtx);
static tANI_S32 wlan_hdd_tdls_peer_reset_discovery_processed(tdlsCtx_t *pHddTdlsCtx);
static void wlan_hdd_tdls_timers_destroy(tdlsCtx_t *pHddTdlsCtx);
static void wlan_hdd_tdls_peer_timers_destroy(tdlsCtx_t *pHddTdlsCtx);
int wpa_tdls_is_allowed_force_peer(tdlsCtx_t *pHddTdlsCtx, u8 *mac);
#ifdef CONFIG_TDLS_IMPLICIT
static void wlan_hdd_tdls_pre_setup(struct work_struct *work);
#endif

#ifndef WLAN_FEATURE_TDLS_DEBUG
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_INFO
#else
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_WARN
#endif

static u8 wlan_hdd_tdls_hash_key (u8 *mac)
{
    int i;
    u8 key = 0;

    for (i = 0; i < 6; i++)
       key ^= mac[i];

    return key;
}

#ifdef CONFIG_TDLS_IMPLICIT
static void wlan_hdd_tdls_pre_setup_init_work(tdlsCtx_t * pHddTdlsCtx,
                                              hddTdlsPeer_t *curr_candidate)
{
    if (TDLS_CTX_MAGIC != pHddTdlsCtx->magic)
    {
        pHddTdlsCtx->curr_candidate = curr_candidate;
        pHddTdlsCtx->magic = TDLS_CTX_MAGIC;

        schedule_work(&pHddTdlsCtx->implicit_setup);
    }
}
#endif

static v_VOID_t wlan_hdd_tdls_start_peer_discover_timer(tdlsCtx_t *pHddTdlsCtx,
                                                        tANI_BOOLEAN mutexLock,
                                                        v_U32_t discoveryExpiry)
{
    hdd_station_ctx_t *pHddStaCtx;
    hdd_context_t *pHddCtx;

    if ((NULL == pHddTdlsCtx) || (NULL == pHddTdlsCtx->pAdapter) )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddTdlsCtx or pAdapter points to NULL"));
        return;
    }

    pHddCtx = WLAN_HDD_GET_CTX( pHddTdlsCtx->pAdapter );

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    if ( mutexLock )
    {
       mutex_lock(&pHddCtx->tdls_lock);
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pHddTdlsCtx->pAdapter);
#ifdef FEATURE_WLAN_TDLS_INTERNAL
    wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                &pHddTdlsCtx->peerDiscoverTimer,
                                discoveryExpiry);
#endif
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "beacon rssi: %d",
               pHddTdlsCtx->ap_rssi);

    if ( mutexLock )
        mutex_unlock(&pHddCtx->tdls_lock);

    return;
}

#ifdef TDLS_USE_SEPARATE_DISCOVERY_TIMER
static v_VOID_t wlan_hdd_tdls_discover_peer_cb( v_PVOID_t userData )
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;
    hdd_station_ctx_t *pHddStaCtx;
    hdd_context_t *pHddCtx;
    tdlsCtx_t *pHddTdlsCtx = (tdlsCtx_t *)userData;
    int discover_req_sent = 0;
    v_U32_t discover_expiry = TDLS_SUB_DISCOVERY_PERIOD;
    tANI_BOOLEAN doMutexLock = eANI_BOOLEAN_TRUE;

    if ((NULL == pHddTdlsCtx) || (NULL == pHddTdlsCtx->pAdapter) )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddTdlsCtx or pAdapter points to NULL"));
        return;
    }

    if (WLAN_HDD_ADAPTER_MAGIC != pHddTdlsCtx->pAdapter->magic)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL("pAdapter  has invalid magic"));
        return;
    }
    pHddCtx = WLAN_HDD_GET_CTX( pHddTdlsCtx->pAdapter );

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pHddTdlsCtx->pAdapter);

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ", __func__);

    if (0 == pHddTdlsCtx->discovery_peer_cnt)
        pHddTdlsCtx->discovery_peer_cnt = wlan_hdd_get_tdls_discovery_peer_cnt(pHddTdlsCtx);

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%d " MAC_ADDRESS_STR " %d %d, %d %d %d %d", i,
                       MAC_ADDR_ARRAY(curr_peer->peerMac),
                       curr_peer->discovery_processed,
                       discover_req_sent,
                       curr_peer->tdls_support,
                       curr_peer->link_status,
                       curr_peer->discovery_attempt,
                       pHddTdlsCtx->threshold_config.discovery_tries_n);

            if (discover_req_sent < TDLS_MAX_DISCOVER_REQS_PER_TIMER) {
                if (!curr_peer->discovery_processed) {

                    curr_peer->discovery_processed = 1;
                    discover_req_sent++;
                    pHddTdlsCtx->discovery_peer_cnt--;

                    if ((eTDLS_CAP_UNKNOWN == curr_peer->tdls_support) &&
                        (eTDLS_LINK_IDLE == curr_peer->link_status) &&
                         (curr_peer->tx_pkt >=
                             pHddTdlsCtx->threshold_config.tx_packet_n)) {

                        if (curr_peer->discovery_attempt <
                            pHddTdlsCtx->threshold_config.discovery_tries_n) {
                            sme_SendTdlsMgmtFrame(WLAN_HDD_GET_HAL_CTX(pHddTdlsCtx->pAdapter),
                                                  pHddTdlsCtx->pAdapter->sessionId,
                                                  curr_peer->peerMac,
                                                  WLAN_TDLS_DISCOVERY_REQUEST,
                                                  1, 0, 0, NULL, 0, 0);
                            curr_peer->discovery_attempt++;
                        }
                        else
                        {
                           VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                                     "%s: Maximum Discovery retries reached", __func__);
                           curr_peer->tdls_support = eTDLS_CAP_NOT_SUPPORTED;
                        }

                   }
                }
            }
            else
                goto exit_loop;
        }
    }
exit_loop:

    if (0 != pHddTdlsCtx->discovery_peer_cnt) {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "discovery_peer_cnt is %d , Starting SUB_DISCOVERY_TIMER",
                  pHddTdlsCtx->discovery_peer_cnt);
        discover_expiry = TDLS_SUB_DISCOVERY_PERIOD;
        doMutexLock = eANI_BOOLEAN_FALSE;
        goto done;
    }
    discover_expiry = pHddTdlsCtx->threshold_config.discovery_period_t;

    wlan_hdd_tdls_peer_reset_discovery_processed(pHddTdlsCtx);

    mutex_unlock(&pHddCtx->tdls_lock);

    /* Commenting out the following function as it was introducing
     * a race condition when pHddTdlsCtx is deleted. Also , this
     * function is consuming more time in the timer callback.
     * RSSI based trigger needs to revisit this part of the code.
     */

    /*
     * wlan_hdd_get_rssi(pAdapter, &pHddTdlsCtx->ap_rssi);
     */

done:
    wlan_hdd_tdls_start_peer_discover_timer(pHddTdlsCtx, doMutexLock, discover_expiry);

    if ( !doMutexLock )
        mutex_unlock(&pHddCtx->tdls_lock);
    return;
}
#endif

static v_VOID_t wlan_hdd_tdls_update_peer_cb( v_PVOID_t userData )
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;
    tdlsCtx_t *pHddTdlsCtx = (tdlsCtx_t *)userData;
    hdd_context_t *pHddCtx;

    if ((NULL == pHddTdlsCtx) || (NULL == pHddTdlsCtx->pAdapter) )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddTdlsCtx or pAdapter points to NULL"));
        return;
    }
    if (WLAN_HDD_ADAPTER_MAGIC != pHddTdlsCtx->pAdapter->magic)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL("pAdapter  has invalid magic"));
        return;
    }
    pHddCtx = WLAN_HDD_GET_CTX( pHddTdlsCtx->pAdapter );

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: " MAC_ADDRESS_STR " link_status %d"
                       " tdls_support %d", __func__, MAC_ADDR_ARRAY(curr_peer->peerMac),
                       curr_peer->link_status, curr_peer->tdls_support);

            if (eTDLS_CAP_SUPPORTED == curr_peer->tdls_support) {
                VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "tx %d, rx %d (thr.pkt %d/idle %d), rssi %d (thr.trig %d/hys %d/tear %d)",
                       curr_peer->tx_pkt, curr_peer->rx_pkt,
                        pHddTdlsCtx->threshold_config.tx_packet_n,
                        pHddTdlsCtx->threshold_config.idle_packet_n,
                        curr_peer->rssi,
                        pHddTdlsCtx->threshold_config.rssi_trigger_threshold,
                        pHddTdlsCtx->threshold_config.rssi_hysteresis,
                        pHddTdlsCtx->threshold_config.rssi_teardown_threshold);

                if ((eTDLS_LINK_IDLE == curr_peer->link_status) ||
                    (eTDLS_LINK_DISCOVERING == curr_peer->link_status)){

                    if (pHddCtx->cfg_ini->fTDLSExternalControl &&
                        (FALSE == curr_peer->isForcedPeer)) {
                        continue;
                    }

                    if (curr_peer->tx_pkt >=
                            pHddTdlsCtx->threshold_config.tx_packet_n) {

                        if (HDD_MAX_NUM_TDLS_STA > wlan_hdd_tdlsConnectedPeers(pHddTdlsCtx->pAdapter))
                        {

                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "Tput trigger TDLS pre-setup");
#ifdef CONFIG_TDLS_IMPLICIT
                            wlan_hdd_tdls_pre_setup_init_work(pHddTdlsCtx, curr_peer);
#endif
                        }
                        else
                        {
                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                                      "%s: Maximum peer connected already! %d",
                                      __func__, wlan_hdd_tdlsConnectedPeers(pHddTdlsCtx->pAdapter) );
                        }
                        goto next_peer;
                    }
                }
                else  if (eTDLS_LINK_CONNECTED == curr_peer->link_status) {
                    if ((tANI_S32)curr_peer->rssi <
                        (tANI_S32)pHddTdlsCtx->threshold_config.rssi_teardown_threshold) {

                                VOS_TRACE( VOS_MODULE_ID_HDD,
                                           VOS_TRACE_LEVEL_WARN,
                                           "Tear down - low RSSI: " MAC_ADDRESS_STR "!",
                                           MAC_ADDR_ARRAY(curr_peer->peerMac));
#ifdef CONFIG_TDLS_IMPLICIT
                        wlan_hdd_tdls_indicate_teardown(pHddTdlsCtx->pAdapter,
                                                        curr_peer,
                                                        eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON);
#endif
                        goto next_peer;
                    }

                    /* Only teardown based on non zero idle packet threshold, to address a use
                     * case where this threshold does not get consider for TEAR DOWN.
                     */

                    if (( 0 != pHddTdlsCtx->threshold_config.idle_packet_n ) &&
                        ((curr_peer->tx_pkt <
                            pHddTdlsCtx->threshold_config.idle_packet_n) &&
                        (curr_peer->rx_pkt <
                            pHddTdlsCtx->threshold_config.idle_packet_n))) {
                        if (VOS_TIMER_STATE_RUNNING !=
                                vos_timer_getCurrentState(&curr_peer->peerIdleTimer)) {
                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                                    "Tx/Rx Idle timer start: " MAC_ADDRESS_STR "!",
                                       MAC_ADDR_ARRAY(curr_peer->peerMac));
                            wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                                        &curr_peer->peerIdleTimer,
                                                        pHddTdlsCtx->threshold_config.idle_timeout_t);
                        }
                    } else {
                        if (VOS_TIMER_STATE_RUNNING ==
                                vos_timer_getCurrentState(&curr_peer->peerIdleTimer)) {
                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                                    "Tx/Rx Idle timer stop: " MAC_ADDRESS_STR "!",
                                       MAC_ADDR_ARRAY(curr_peer->peerMac));
                            vos_timer_stop( &curr_peer->peerIdleTimer);
                        }
                    }

//                    if (curr_peer->rssi <
//                            (pHddTdlsCtx->threshold_config.rssi_hysteresis +
//                                pHddTdlsCtx->ap_rssi)) {
//
//#ifdef CONFIG_TDLS_IMPLICIT
//                        cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
//                                                   curr_peer->peerMac,
//                                                   NL80211_TDLS_TEARDOWN, FALSE,
//                                                   GFP_KERNEL);
//#endif
//                    }
                }
            } else if (eTDLS_CAP_UNKNOWN == curr_peer->tdls_support) {

                if (pHddCtx->cfg_ini->fTDLSExternalControl &&
                    (FALSE == curr_peer->isForcedPeer)) {
                    continue;
                }
                if (!TDLS_IS_CONNECTED(curr_peer)) {
                    if (curr_peer->tx_pkt >=
                            pHddTdlsCtx->threshold_config.tx_packet_n) {

                        /* Ignore discovery attempt if External Control is enabled, that
                         * is, peer is forced. In that case, continue discovery attempt
                         * regardless attempt count
                         */
                        if (curr_peer->isForcedPeer || curr_peer->discovery_attempt++ <
                                 pHddTdlsCtx->threshold_config.discovery_tries_n) {
                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "TDLS UNKNOWN discover ");
#ifdef CONFIG_TDLS_IMPLICIT
                            wlan_hdd_tdls_pre_setup_init_work(pHddTdlsCtx, curr_peer);
#endif
                        }
                        else
                        {
                            curr_peer->tdls_support = eTDLS_CAP_NOT_SUPPORTED;
                            wlan_hdd_tdls_set_peer_link_status(
                                                    curr_peer,
                                                    eTDLS_LINK_IDLE,
                                                    eTDLS_LINK_NOT_SUPPORTED);

                        }
                    }
                }
            }

next_peer:
            curr_peer->tx_pkt = 0;
            curr_peer->rx_pkt = 0;
        }
    }

    wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                &pHddTdlsCtx->peerUpdateTimer,
                                pHddTdlsCtx->threshold_config.tx_period_t);
    mutex_unlock(&pHddCtx->tdls_lock);
}

static v_VOID_t wlan_hdd_tdls_idle_cb( v_PVOID_t userData )
{
#ifdef CONFIG_TDLS_IMPLICIT
    hddTdlsPeer_t *curr_peer = (hddTdlsPeer_t *)userData;
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx;

    if (NULL == curr_peer)
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              FL("Invalid tdls idle timer expired"));
      return;
    }
    pHddTdlsCtx = curr_peer->pHddTdlsCtx;

    if ((NULL == pHddTdlsCtx) || (NULL == pHddTdlsCtx->pAdapter) )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddTdlsCtx or pAdapter points to NULL"));
        return;
    }

    pHddCtx = WLAN_HDD_GET_CTX( pHddTdlsCtx->pAdapter );

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
              "%s: Tx/Rx Idle " MAC_ADDRESS_STR " tx_pkt: %d, rx_pkt: %d, idle_packet_n: %d",
              __func__, MAC_ADDR_ARRAY(curr_peer->peerMac),
              curr_peer->tx_pkt,
              curr_peer->rx_pkt,
              curr_peer->pHddTdlsCtx->threshold_config.idle_packet_n);

    mutex_lock(&pHddCtx->tdls_lock);

    /* Check tx/rx statistics on this tdls link for recent activities and
     * then decide whether to tear down the link or keep it.
     */
    if ((curr_peer->tx_pkt >= curr_peer->pHddTdlsCtx->threshold_config.idle_packet_n) || (curr_peer->rx_pkt >= curr_peer->pHddTdlsCtx->threshold_config.idle_packet_n))
    {
      /* this tdls link got back to normal, so keep it */
      VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
               "%s: tdls link to " MAC_ADDRESS_STR " back to normal, will stay",
               __func__, MAC_ADDR_ARRAY(curr_peer->peerMac));
    }
    else
    {
      /* this tdls link needs to get torn down */
      VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                "%s: trigger tdls link to " MAC_ADDRESS_STR " down",
                __func__, MAC_ADDR_ARRAY(curr_peer->peerMac));

      wlan_hdd_tdls_indicate_teardown(curr_peer->pHddTdlsCtx->pAdapter,
                                      curr_peer,
                                      eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON);
    }
    mutex_unlock(&pHddCtx->tdls_lock);
#endif
}

static v_VOID_t wlan_hdd_tdls_discovery_timeout_peer_cb(v_PVOID_t userData)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *tmp;
    struct list_head *pos, *q;
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx;

    pHddTdlsCtx = (tdlsCtx_t *)userData;

    if ((NULL == pHddTdlsCtx) || (NULL == pHddTdlsCtx->pAdapter) )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddTdlsCtx or pAdapter points to NULL"));
        return;
    }

    if (WLAN_HDD_ADAPTER_MAGIC != pHddTdlsCtx->pAdapter->magic)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL("pAdapter  has invalid magic"));
        return;
    }
    pHddCtx = WLAN_HDD_GET_CTX( pHddTdlsCtx->pAdapter );

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            if (eTDLS_LINK_DISCOVERING == tmp->link_status)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                           "%s: " MAC_ADDRESS_STR " to idle state", __func__,
                           MAC_ADDR_ARRAY(tmp->peerMac));
                           wlan_hdd_tdls_set_peer_link_status(
                                                  tmp,
                                                  eTDLS_LINK_IDLE,
                                                  eTDLS_LINK_NOT_SUPPORTED);
            }
        }
    }

    pHddTdlsCtx->discovery_sent_cnt = 0;
    wlan_hdd_tdls_check_power_save_prohibited(pHddTdlsCtx->pAdapter);

    mutex_unlock(&pHddCtx->tdls_lock);

    wlan_hdd_tdls_check_bmps(pHddTdlsCtx->pAdapter);

    return;
}

static v_VOID_t wlan_hdd_tdls_initiator_wait_cb( v_PVOID_t userData )
{
    hddTdlsPeer_t *curr_peer = (hddTdlsPeer_t *)userData;
    tdlsCtx_t   *pHddTdlsCtx;

    if ( NULL == curr_peer )
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("curr_peer is NULL"));
        return;
    }

    pHddTdlsCtx = curr_peer->pHddTdlsCtx;

    if ( NULL == pHddTdlsCtx )
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
        return;
    }

    WLANTL_ResumeDataTx( (WLAN_HDD_GET_CTX(pHddTdlsCtx->pAdapter))->pvosContext,
                           (v_U8_t *)&curr_peer->staId);
}

static void wlan_hdd_tdls_free_list(tdlsCtx_t *pHddTdlsCtx)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *tmp;
    struct list_head *pos, *q;

    if (NULL == pHddTdlsCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
       return;
    }

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            list_del(pos);
            vos_mem_free(tmp);
            tmp = NULL;
        }
    }
}

static void wlan_hdd_tdls_schedule_scan(struct work_struct *work)
{
    tdls_scan_context_t *scan_ctx =
          container_of(work, tdls_scan_context_t, tdls_scan_work.work);

    if (NULL == scan_ctx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("scan_ctx is NULL"));
        return;
    }

    if (unlikely(TDLS_CTX_MAGIC != scan_ctx->magic))
        return;

    scan_ctx->attempt++;

    wlan_hdd_cfg80211_scan(scan_ctx->wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                           scan_ctx->dev,
#endif
                           scan_ctx->scan_request);
}

/* initialize TDLS global context */
void  wlan_hdd_tdls_init(hdd_context_t *pHddCtx )
{
    v_U8_t staIdx;

    pHddCtx->connected_peer_count = 0;

    pHddCtx->tdls_scan_ctxt.magic = 0;
    pHddCtx->tdls_scan_ctxt.attempt = 0;
    pHddCtx->tdls_scan_ctxt.reject = 0;
    pHddCtx->tdls_scan_ctxt.scan_request = NULL;

    for (staIdx = 0; staIdx < HDD_MAX_NUM_TDLS_STA; staIdx++)
    {
         pHddCtx->tdlsConnInfo[staIdx].staId = 0;
         pHddCtx->tdlsConnInfo[staIdx].sessionId = 255;
         vos_mem_zero(&pHddCtx->tdlsConnInfo[staIdx].peerMac,
                                            sizeof(v_MACADDR_t)) ;
    }

    if (FALSE == pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger)
    {
        pHddCtx->tdls_mode = eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY;
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s TDLS Implicit trigger not enabled!", __func__);
    }
    else
    {
        pHddCtx->tdls_mode = eTDLS_SUPPORT_ENABLED;
    }
}

int wlan_hdd_sta_tdls_init(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
    tdlsCtx_t *pHddTdlsCtx;
    int i;

    if (NULL == pHddCtx)
        return -1;

    mutex_lock(&pHddCtx->tdls_lock);

    if ((FALSE == pHddCtx->cfg_ini->fEnableTDLSSupport) ||
        (FALSE == sme_IsFeatureSupportedByFW(TDLS)))
    {
        pHddCtx->tdls_mode = eTDLS_SUPPORT_NOT_ENABLED;
        pAdapter->sessionCtx.station.pHddTdlsCtx = NULL;
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s TDLS not enabled (%d) or FW doesn't support (%d)!",
        __func__, pHddCtx->cfg_ini->fEnableTDLSSupport,
        sme_IsFeatureSupportedByFW(TDLS));
        mutex_unlock(&pHddCtx->tdls_lock);
        return 0;
    }
    /* TDLS is supported only in STA / P2P Client modes,
     * hence the check for TDLS support in a specific Device mode.
     * Do not return a failure rather do not continue further
     * with the initialization as tdls_init would be called
     * during the open adapter for a p2p interface at which point
     * the device mode would be a P2P_DEVICE. The point here is to
     * continue initialization for STA / P2P Client modes.
     * TDLS exit also check for the device mode for clean up hence
     * there is no issue even if success is returned.
     */
    if (0 == WLAN_HDD_IS_TDLS_SUPPORTED_ADAPTER(pAdapter))
    {
        mutex_unlock(&pHddCtx->tdls_lock);
        return 0;
    }
    /* Check for the valid pHddTdlsCtx. If valid do not further
     * allocate the memory, rather continue with the initialization.
     * If tdls_initialization would get reinvoked  without tdls_exit
     * getting invoked (SSR) there is no point to further proceed
     * with the memory allocations.
     */
    if (NULL == pAdapter->sessionCtx.station.pHddTdlsCtx)
    {
        pHddTdlsCtx = vos_mem_malloc(sizeof(tdlsCtx_t));

        if (NULL == pHddTdlsCtx) {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s malloc failed!", __func__);
            pAdapter->sessionCtx.station.pHddTdlsCtx = NULL;
            mutex_unlock(&pHddCtx->tdls_lock);
            return -1;
        }
        /* initialize TDLS pAdater context */
        vos_mem_zero(pHddTdlsCtx, sizeof(tdlsCtx_t));
#ifdef TDLS_USE_SEPARATE_DISCOVERY_TIMER
        vos_timer_init(&pHddTdlsCtx->peerDiscoverTimer,
                VOS_TIMER_TYPE_SW,
                wlan_hdd_tdls_discover_peer_cb,
                pHddTdlsCtx);
#endif

        vos_timer_init(&pHddTdlsCtx->peerUpdateTimer,
                VOS_TIMER_TYPE_SW,
                wlan_hdd_tdls_update_peer_cb,
                pHddTdlsCtx);
        vos_timer_init(&pHddTdlsCtx->peerDiscoveryTimeoutTimer,
                VOS_TIMER_TYPE_SW,
                wlan_hdd_tdls_discovery_timeout_peer_cb,
                pHddTdlsCtx);

        pAdapter->sessionCtx.station.pHddTdlsCtx = pHddTdlsCtx;
    }

    pHddTdlsCtx = pAdapter->sessionCtx.station.pHddTdlsCtx;

    sme_SetTdlsPowerSaveProhibited(WLAN_HDD_GET_HAL_CTX(pAdapter), 0);

    pHddTdlsCtx->pAdapter = pAdapter;

    for (i = 0; i < 256; i++)
    {
        INIT_LIST_HEAD(&pHddTdlsCtx->peer_list[i]);
    }

    pHddTdlsCtx->curr_candidate = NULL;
    pHddTdlsCtx->magic = 0;

    /* remember configuration even if it is not used right now. it could be used later */
    pHddTdlsCtx->threshold_config.tx_period_t = pHddCtx->cfg_ini->fTDLSTxStatsPeriod;
    pHddTdlsCtx->threshold_config.tx_packet_n = pHddCtx->cfg_ini->fTDLSTxPacketThreshold;
    pHddTdlsCtx->threshold_config.discovery_period_t = pHddCtx->cfg_ini->fTDLSDiscoveryPeriod;
    pHddTdlsCtx->threshold_config.discovery_tries_n = pHddCtx->cfg_ini->fTDLSMaxDiscoveryAttempt;
    pHddTdlsCtx->threshold_config.idle_timeout_t = pHddCtx->cfg_ini->fTDLSIdleTimeout;
    pHddTdlsCtx->threshold_config.idle_packet_n = pHddCtx->cfg_ini->fTDLSIdlePacketThreshold;
    pHddTdlsCtx->threshold_config.rssi_hysteresis = pHddCtx->cfg_ini->fTDLSRSSIHysteresis;
    pHddTdlsCtx->threshold_config.rssi_trigger_threshold = pHddCtx->cfg_ini->fTDLSRSSITriggerThreshold;
    pHddTdlsCtx->threshold_config.rssi_teardown_threshold = pHddCtx->cfg_ini->fTDLSRSSITeardownThreshold;

    INIT_WORK(&pHddTdlsCtx->implicit_setup, wlan_hdd_tdls_pre_setup);
    INIT_DELAYED_WORK(&pHddCtx->tdls_scan_ctxt.tdls_scan_work, wlan_hdd_tdls_schedule_scan);
    mutex_unlock(&pHddCtx->tdls_lock);

    return 0;
}

void wlan_hdd_tdls_exit(hdd_adapter_t *pAdapter)
{
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx;

    /*
     * NOTE: The Callers of this function should ensure to acquire the
     * tdls_lock to avoid any concurrent access to the Adapter.
     */

    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    if (NULL == pHddCtx || NULL == pHddCtx->cfg_ini)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null", __func__);
        return ;
    }

    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: LOGP in Progress. Ignore!!!", __func__);
        return ;
    }

    pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    if (NULL == pHddTdlsCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
        return;
    }
#ifdef WLAN_OPEN_SOURCE
    cancel_work_sync(&pHddTdlsCtx->implicit_setup);
    cancel_delayed_work_sync(&pHddCtx->tdls_scan_ctxt.tdls_scan_work);
#endif

    /* must stop timer here before freeing peer list, because peerIdleTimer is
    part of peer list structure. */
    wlan_hdd_tdls_timers_destroy(pHddTdlsCtx);
    wlan_hdd_tdls_free_list(pHddTdlsCtx);

    wlan_hdd_tdls_free_scan_request(&pHddCtx->tdls_scan_ctxt);
    pHddTdlsCtx->magic = 0;
    pHddTdlsCtx->pAdapter = NULL;
    vos_mem_free(pHddTdlsCtx);
    pAdapter->sessionCtx.station.pHddTdlsCtx = NULL;
    pHddTdlsCtx = NULL;
}

/* stop all monitoring timers per Adapter */
static void wlan_hdd_tdls_monitor_timers_stop(tdlsCtx_t *pHddTdlsCtx)
{
#ifdef TDLS_USE_SEPARATE_DISCOVERY_TIMER
    vos_timer_stop(&pHddTdlsCtx->peerDiscoverTimer);
#endif
    vos_timer_stop(&pHddTdlsCtx->peerUpdateTimer);
    vos_timer_stop(&pHddTdlsCtx->peerDiscoveryTimeoutTimer);
}

/* stop all per peer timers */
static void wlan_hdd_tdls_peer_timers_stop(tdlsCtx_t *pHddTdlsCtx)
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;
    for (i = 0; i < 256; i++)
    {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);
            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: " MAC_ADDRESS_STR " -> stop idle timer",
                       __func__,
                       MAC_ADDR_ARRAY(curr_peer->peerMac));
            vos_timer_stop ( &curr_peer->peerIdleTimer );
            vos_timer_stop( &curr_peer->initiatorWaitTimeoutTimer );
        }
    }
}

/* stop all the tdls timers running */
static void wlan_hdd_tdls_timers_stop(tdlsCtx_t *pHddTdlsCtx)
{
    wlan_hdd_tdls_monitor_timers_stop(pHddTdlsCtx);
    wlan_hdd_tdls_peer_timers_stop(pHddTdlsCtx);
}

static void wlan_hdd_tdls_monitor_timers_destroy(tdlsCtx_t *pHddTdlsCtx)
{
#ifdef TDLS_USE_SEPARATE_DISCOVERY_TIMER
    vos_timer_stop(&pHddTdlsCtx->peerDiscoverTimer);
    vos_timer_destroy(&pHddTdlsCtx->peerDiscoverTimer);
#endif
    vos_timer_stop(&pHddTdlsCtx->peerUpdateTimer);
    vos_timer_destroy(&pHddTdlsCtx->peerUpdateTimer);
    vos_timer_stop(&pHddTdlsCtx->peerDiscoveryTimeoutTimer);
    vos_timer_destroy(&pHddTdlsCtx->peerDiscoveryTimeoutTimer);
}
/*Free all the timers related to the TDLS peer */
static void wlan_hdd_tdls_peer_timers_destroy(tdlsCtx_t *pHddTdlsCtx)
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;
    for (i = 0; i < 256; i++)
    {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: " MAC_ADDRESS_STR " -> destroy idle timer",
                       __func__,
                       MAC_ADDR_ARRAY(curr_peer->peerMac));
            vos_timer_stop ( &curr_peer->peerIdleTimer );
            vos_timer_destroy ( &curr_peer->peerIdleTimer );
            vos_timer_stop(&curr_peer->initiatorWaitTimeoutTimer);
            vos_timer_destroy(&curr_peer->initiatorWaitTimeoutTimer);
        }
    }

}

/* destroy all the tdls timers running */
static void wlan_hdd_tdls_timers_destroy(tdlsCtx_t *pHddTdlsCtx)
{
    wlan_hdd_tdls_monitor_timers_destroy(pHddTdlsCtx);
    wlan_hdd_tdls_peer_timers_destroy(pHddTdlsCtx);
}

/* if mac address exist, return pointer
   if mac address doesn't exist, create a list and add, return pointer
   return NULL if fails to get new mac address
*/
hddTdlsPeer_t *wlan_hdd_tdls_get_peer(hdd_adapter_t *pAdapter, u8 *mac)
{
    struct list_head *head;
    hddTdlsPeer_t *peer;
    u8 key;
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    /* if already there, just update */
    peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
    if (peer != NULL)
    {
        return peer;
    }

    /* not found, allocate and add the list */
    peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));
    if (NULL == peer) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s peer malloc failed!", __func__);
        return NULL;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);

    if (NULL == pHddTdlsCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 FL("pHddTdlsCtx is NULL"));
        vos_mem_free(peer);
        mutex_unlock(&pHddCtx->tdls_lock);
        return NULL;
    }

    key = wlan_hdd_tdls_hash_key(mac);
    head = &pHddTdlsCtx->peer_list[key];

    vos_mem_zero(peer, sizeof(hddTdlsPeer_t));
    vos_mem_copy(peer->peerMac, mac, sizeof(peer->peerMac));
    peer->pHddTdlsCtx = pHddTdlsCtx;

    vos_timer_init(&peer->peerIdleTimer,
                    VOS_TIMER_TYPE_SW,
                    wlan_hdd_tdls_idle_cb,
                    peer);

    vos_timer_init(&peer->initiatorWaitTimeoutTimer,
                    VOS_TIMER_TYPE_SW,
                    wlan_hdd_tdls_initiator_wait_cb,
                    peer);

    list_add_tail(&peer->node, head);
    mutex_unlock(&pHddCtx->tdls_lock);

    return peer;
}

int wlan_hdd_tdls_set_cap(hdd_adapter_t *pAdapter,
                                   u8* mac,
                                   tTDLSCapType cap)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    curr_peer->tdls_support = cap;

    return 0;
}

void wlan_hdd_tdls_set_peer_link_status(hddTdlsPeer_t *curr_peer,
                                        tTDLSLinkStatus status,
                                        tTDLSLinkReason reason)
{
    /*EXT TDLS*/
    tANI_S32 state = 0;
    tANI_S32 res = 0;
    /*EXT TDLS*/
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return;
    }

    hddLog(VOS_TRACE_LEVEL_WARN, "tdls set peer " MAC_ADDRESS_STR " link status to %u",
            MAC_ADDR_ARRAY(curr_peer->peerMac), status);

    curr_peer->link_status = status;

    /*EXT TDLS*/
    if (curr_peer->isForcedPeer && curr_peer->state_change_notification)
    {
        /*save the reason for any further query*/
        curr_peer->reason = reason;
        wlan_hdd_tdls_get_wifi_hal_state(curr_peer, &state, &res);

        (curr_peer->state_change_notification)(
                                          curr_peer->peerMac,
                                          state,
                                          res,
                                          curr_peer->pHddTdlsCtx->pAdapter);

    }
    /*EXT TDLS*/

}

void wlan_hdd_tdls_set_link_status(hdd_adapter_t *pAdapter,
                                   u8* mac,
                                   tTDLSLinkStatus linkStatus,
                                   tTDLSLinkReason reason)
{

    /*EXT TDLS*/
    tANI_S32 state = 0;
    tANI_S32 res = 0;
    /*EXT TDLS*/
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return;
    }

    curr_peer->link_status= linkStatus;

    /*EXT TDLS*/

    if (curr_peer->isForcedPeer && curr_peer->state_change_notification)
    {
        /*save the reason for any further query*/
        curr_peer->reason = reason;
        wlan_hdd_tdls_get_wifi_hal_state(curr_peer, &state, &res);

        (*curr_peer->state_change_notification)(mac,
                                             state,
                                             res,
                                             curr_peer->pHddTdlsCtx->pAdapter);

    }
    /*EXT TDLS*/
    return;
}

int wlan_hdd_tdls_recv_discovery_resp(hdd_adapter_t *pAdapter, u8 *mac)
{
    hddTdlsPeer_t *curr_peer;
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx;

    if ( NULL == pHddTdlsCtx )
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
        return -1;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pHddTdlsCtx->pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);

    if (NULL == curr_peer)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("curr_peer is NULL"));
        return -1;
    }

    if (pHddTdlsCtx->discovery_sent_cnt)
        pHddTdlsCtx->discovery_sent_cnt--;

    mutex_lock(&pHddCtx->tdls_lock);

    wlan_hdd_tdls_check_power_save_prohibited(pAdapter);

    mutex_unlock(&pHddCtx->tdls_lock);
    if (0 == pHddTdlsCtx->discovery_sent_cnt)
    {
        vos_timer_stop(&pHddTdlsCtx->peerDiscoveryTimeoutTimer);
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
               "Discovery(%u) Response from " MAC_ADDRESS_STR " link_status %d",
               pHddTdlsCtx->discovery_sent_cnt, MAC_ADDR_ARRAY(curr_peer->peerMac),
               curr_peer->link_status);

    if (eTDLS_LINK_DISCOVERING == curr_peer->link_status)
    {
        /* Since we are here, it means Throughput threshold is alredy met. Make sure RSSI
           threshold is also met before setting up TDLS link*/
        if ((tANI_S32) curr_peer->rssi > (tANI_S32) pHddTdlsCtx->threshold_config.rssi_trigger_threshold)
        {
            wlan_hdd_tdls_set_peer_link_status(curr_peer,
                                               eTDLS_LINK_DISCOVERED,
                                               eTDLS_LINK_SUCCESS);
            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
            "Rssi Threshold met: "MAC_ADDRESS_STR" rssi = %d threshold= %d" ,
             MAC_ADDR_ARRAY(curr_peer->peerMac), curr_peer->rssi,
             pHddTdlsCtx->threshold_config.rssi_trigger_threshold);
            cfg80211_tdls_oper_request(pAdapter->dev, curr_peer->peerMac, NL80211_TDLS_SETUP, FALSE, GFP_KERNEL);
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, 
            "Rssi Threshold not met: "MAC_ADDRESS_STR" rssi = %d threshold = %d ",
            MAC_ADDR_ARRAY(curr_peer->peerMac), curr_peer->rssi,
            pHddTdlsCtx->threshold_config.rssi_trigger_threshold);

            /* if RSSI threshold is not met then allow further discovery
             * attempts by decrementing count for the last attempt
             */
            if (curr_peer->discovery_attempt)
                curr_peer->discovery_attempt--;

            wlan_hdd_tdls_set_peer_link_status(curr_peer,
                                               eTDLS_LINK_IDLE,
                                               eTDLS_LINK_UNSPECIFIED);
        }
    }
    else
    {
        wlan_hdd_tdls_check_bmps(pAdapter);
    }

    curr_peer->tdls_support = eTDLS_CAP_SUPPORTED;
    return 0;
}

int wlan_hdd_tdls_set_peer_caps(hdd_adapter_t *pAdapter,
                                u8 *mac,
                                tCsrStaParams *StaParams,
                                tANI_BOOLEAN isBufSta,
                                tANI_BOOLEAN isOffChannelSupported)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    curr_peer->uapsdQueues = StaParams->uapsd_queues;
    curr_peer->maxSp = StaParams->max_sp;
    curr_peer->isBufSta = isBufSta;
    curr_peer->isOffChannelSupported = isOffChannelSupported;

    vos_mem_copy(curr_peer->supported_channels,
                 StaParams->supported_channels,
                 StaParams->supported_channels_len);

    curr_peer->supported_channels_len =
               StaParams->supported_channels_len;

    vos_mem_copy(curr_peer->supported_oper_classes,
                 StaParams->supported_oper_classes,
                 StaParams->supported_oper_classes_len);

    curr_peer->supported_oper_classes_len =
               StaParams->supported_oper_classes_len;
    return 0;
}

int wlan_hdd_tdls_get_link_establish_params(hdd_adapter_t *pAdapter, u8 *mac,
                                            tCsrTdlsLinkEstablishParams* tdlsLinkEstablishParams)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    tdlsLinkEstablishParams->isResponder = curr_peer->is_responder;
    tdlsLinkEstablishParams->uapsdQueues = curr_peer->uapsdQueues;
    tdlsLinkEstablishParams->maxSp = curr_peer->maxSp;
    tdlsLinkEstablishParams->isBufSta = curr_peer->isBufSta;
    tdlsLinkEstablishParams->isOffChannelSupported =
                                 curr_peer->isOffChannelSupported;

    vos_mem_copy(tdlsLinkEstablishParams->supportedChannels,
                 curr_peer->supported_channels,
                 curr_peer->supported_channels_len);

    tdlsLinkEstablishParams->supportedChannelsLen =
                 curr_peer->supported_channels_len;

    vos_mem_copy(tdlsLinkEstablishParams->supportedOperClasses,
                 curr_peer->supported_oper_classes,
                 curr_peer->supported_oper_classes_len);

    tdlsLinkEstablishParams->supportedOperClassesLen =
                 curr_peer->supported_oper_classes_len;
    return 0;
}

int wlan_hdd_tdls_set_rssi(hdd_adapter_t *pAdapter, u8 *mac, tANI_S8 rxRssi)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    curr_peer->rssi = rxRssi;

    return 0;
}

int wlan_hdd_tdls_set_responder(hdd_adapter_t *pAdapter, u8 *mac, tANI_U8 responder)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    curr_peer->is_responder = responder;

    return 0;
}

int wlan_hdd_tdls_get_responder(hdd_adapter_t *pAdapter, u8 *mac)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    return (curr_peer->is_responder);
}

int wlan_hdd_tdls_set_signature(hdd_adapter_t *pAdapter, u8 *mac, tANI_U8 uSignature)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    curr_peer->signature = uSignature;

    return 0;
}


void wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac)
{
    memcpy(mac, skb->data, 6);
}

void wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac)
{
    memcpy(mac, skb->data+6, 6);
}

int wlan_hdd_tdls_increment_pkt_count(hdd_adapter_t *pAdapter, u8 *mac, u8 tx)
{
    hddTdlsPeer_t *curr_peer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (eTDLS_SUPPORT_ENABLED != pHddCtx->tdls_mode)
        return -1;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 FL("curr_peer is NULL"));
        return -1;
    }

    if (tx)
        curr_peer->tx_pkt++;
    else
        curr_peer->rx_pkt++;

    return 0;
}

static int wlan_hdd_tdls_check_config(tdls_config_params_t *config)
{
    if (config->tdls > 2)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 1st argument %d. <0...2>", __func__, config->tdls);
        return -1;
    }
    if (config->tx_period_t < CFG_TDLS_TX_STATS_PERIOD_MIN ||
        config->tx_period_t > CFG_TDLS_TX_STATS_PERIOD_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 2nd argument %d. <%d...%ld>", __func__, config->tx_period_t,
            CFG_TDLS_TX_STATS_PERIOD_MIN, CFG_TDLS_TX_STATS_PERIOD_MAX);
        return -1;
    }
    if (config->tx_packet_n < CFG_TDLS_TX_PACKET_THRESHOLD_MIN ||
        config->tx_packet_n > CFG_TDLS_TX_PACKET_THRESHOLD_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 3rd argument %d. <%d...%ld>", __func__, config->tx_packet_n,
            CFG_TDLS_TX_PACKET_THRESHOLD_MIN, CFG_TDLS_TX_PACKET_THRESHOLD_MAX);
        return -1;
    }
    if (config->discovery_period_t < CFG_TDLS_DISCOVERY_PERIOD_MIN ||
        config->discovery_period_t > CFG_TDLS_DISCOVERY_PERIOD_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 4th argument %d. <%d...%ld>", __func__, config->discovery_period_t,
            CFG_TDLS_DISCOVERY_PERIOD_MIN, CFG_TDLS_DISCOVERY_PERIOD_MAX);
        return -1;
    }
    if (config->discovery_tries_n < CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MIN ||
        config->discovery_tries_n > CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 5th argument %d. <%d...%d>", __func__, config->discovery_tries_n,
            CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MIN, CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MAX);
        return -1;
    }
    if (config->idle_timeout_t < CFG_TDLS_IDLE_TIMEOUT_MIN ||
        config->idle_timeout_t > CFG_TDLS_IDLE_TIMEOUT_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 6th argument %d. <%d...%d>", __func__, config->idle_timeout_t,
            CFG_TDLS_IDLE_TIMEOUT_MIN, CFG_TDLS_IDLE_TIMEOUT_MAX);
        return -1;
    }
    if (config->idle_packet_n < CFG_TDLS_IDLE_PACKET_THRESHOLD_MIN ||
        config->idle_packet_n > CFG_TDLS_IDLE_PACKET_THRESHOLD_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 7th argument %d. <%d...%d>", __func__, config->idle_packet_n,
            CFG_TDLS_IDLE_PACKET_THRESHOLD_MIN, CFG_TDLS_IDLE_PACKET_THRESHOLD_MAX);
        return -1;
    }
    if (config->rssi_hysteresis < CFG_TDLS_RSSI_HYSTERESIS_MIN ||
        config->rssi_hysteresis > CFG_TDLS_RSSI_HYSTERESIS_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 8th argument %d. <%d...%d>", __func__, config->rssi_hysteresis,
            CFG_TDLS_RSSI_HYSTERESIS_MIN, CFG_TDLS_RSSI_HYSTERESIS_MAX);
        return -1;
    }
    if (config->rssi_trigger_threshold < CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MIN ||
        config->rssi_trigger_threshold > CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 9th argument %d. <%d...%d>", __func__, config->rssi_trigger_threshold,
            CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MIN, CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MAX);
        return -1;
    }
    if (config->rssi_teardown_threshold < CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MIN ||
        config->rssi_teardown_threshold > CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MAX)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s invalid 10th argument %d. <%d...%d>", __func__, config->rssi_teardown_threshold,
            CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MIN, CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MAX);
        return -1;
    }
    return 0;
}

int wlan_hdd_tdls_set_params(struct net_device *dev, tdls_config_params_t *config)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    eTDLSSupportMode req_tdls_mode;

    if (NULL == pHddTdlsCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("TDLS not enabled!"));
        return -1;
    }

    if (wlan_hdd_tdls_check_config(config) != 0)
    {
        return -1;
    }

    /* config->tdls is mapped to 0->1, 1->2, 2->3 */
    req_tdls_mode = config->tdls + 1;
    if (pHddCtx->tdls_mode == req_tdls_mode)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s already in mode %d", __func__, config->tdls);
        return -1;
    }

    /* copy the configuration only when given tdls mode is implicit trigger enable */
    if (eTDLS_SUPPORT_ENABLED == req_tdls_mode)
    {
        memcpy(&pHddTdlsCtx->threshold_config, config, sizeof(tdls_config_params_t));
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "iw set tdls params: %d %d %d %d %d %d %d %d %d %d",
            config->tdls,
            config->tx_period_t,
            config->tx_packet_n,
            config->discovery_period_t,
            config->discovery_tries_n,
            config->idle_timeout_t,
            config->idle_packet_n,
            config->rssi_hysteresis,
            config->rssi_trigger_threshold,
            config->rssi_teardown_threshold);

    wlan_hdd_tdls_set_mode(pHddCtx, req_tdls_mode, TRUE);

    return 0;
}

int wlan_hdd_tdls_set_sta_id(hdd_adapter_t *pAdapter, u8 *mac, u8 staId)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    curr_peer->staId = staId;

    return 0;
}

int wlan_hdd_tdls_set_force_peer(hdd_adapter_t *pAdapter, u8 *mac,
                                 tANI_BOOLEAN forcePeer)
{
    hddTdlsPeer_t *curr_peer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if ((NULL == pHddCtx)) return -1;

    mutex_lock(&pHddCtx->tdls_lock);

    curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, FALSE);
    if (curr_peer == NULL)
        goto error;
    curr_peer->isForcedPeer = forcePeer;

    mutex_unlock(&pHddCtx->tdls_lock);
    return 0;
error:
    mutex_unlock(&pHddCtx->tdls_lock);
    return -1;
}

/* if peerMac is found, then it returns pointer to hddTdlsPeer_t
   otherwise, it returns NULL
*/
hddTdlsPeer_t *wlan_hdd_tdls_find_peer(hdd_adapter_t *pAdapter, u8 *mac,
                                       tANI_BOOLEAN mutexLock)
{
    u8 key;
    struct list_head *pos;
    struct list_head *head;
    hddTdlsPeer_t *curr_peer;
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    if ( mutexLock )
    {
       mutex_lock(&pHddCtx->tdls_lock);
    }
    pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    if (NULL == pHddTdlsCtx)
    {
        if ( mutexLock )
            mutex_unlock(&pHddCtx->tdls_lock);
        return NULL;
    }

    key = wlan_hdd_tdls_hash_key(mac);

    head = &pHddTdlsCtx->peer_list[key];

    list_for_each(pos, head) {
        curr_peer = list_entry (pos, hddTdlsPeer_t, node);
        if (!memcmp(mac, curr_peer->peerMac, 6)) {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "findTdlsPeer: found staId %d", curr_peer->staId);
            if ( mutexLock )
                mutex_unlock(&pHddCtx->tdls_lock);
            return curr_peer;
        }
    }
    if ( mutexLock )
        mutex_unlock(&pHddCtx->tdls_lock);
    return NULL;
}

hddTdlsPeer_t *wlan_hdd_tdls_find_all_peer(hdd_context_t *pHddCtx, u8 *mac)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter = NULL;
    tdlsCtx_t *pHddTdlsCtx = NULL;
    hddTdlsPeer_t *curr_peer= NULL;
    VOS_STATUS status = 0;

    mutex_lock(&pHddCtx->tdls_lock);
    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;

        pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
        if (NULL != pHddTdlsCtx)
        {
            curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, FALSE);
            if (curr_peer)
            {
                mutex_unlock(&pHddCtx->tdls_lock);
                return curr_peer;
            }
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    mutex_unlock(&pHddCtx->tdls_lock);
    return curr_peer;
}


int wlan_hdd_tdls_reset_peer(hdd_adapter_t *pAdapter, u8 *mac)
{
    hdd_context_t *pHddCtx;
    hddTdlsPeer_t *curr_peer;

    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    curr_peer = wlan_hdd_tdls_get_peer(pAdapter, mac);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return -1;
    }

    wlan_hdd_tdls_set_peer_link_status(curr_peer,
                                       eTDLS_LINK_IDLE,
                                       eTDLS_LINK_UNSPECIFIED);
    curr_peer->staId = 0;

    /* Throughput Monitor shall disable the split scan when
     * TDLS scan coexistance is disabled.At this point of time
     * since TDLS scan coexistance is not meeting the criteria
     * to be operational, explicitly make it false to enable
     * throughput monitor takes the control of split scan.
     */
    if (pHddCtx->isTdlsScanCoexistence == TRUE)
    {
        pHddCtx->isTdlsScanCoexistence = FALSE;
    }

    if(eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode) {
        vos_timer_stop( &curr_peer->peerIdleTimer );
    }
    return 0;
}

/* Caller has to take the lock before calling this function */
static void wlan_tdd_tdls_reset_tx_rx(tdlsCtx_t *pHddTdlsCtx)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *tmp;
    struct list_head *pos, *q;

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            tmp->tx_pkt = 0;
            tmp->rx_pkt = 0;
        }
    }

    return ;
}

/* Caller has to take the lock before calling this function */
static tANI_S32 wlan_hdd_tdls_peer_reset_discovery_processed(tdlsCtx_t *pHddTdlsCtx)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *tmp;
    struct list_head *pos, *q;

    pHddTdlsCtx->discovery_peer_cnt = 0;

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            tmp->discovery_processed = 0;
        }
    }

    return 0;
}

static tANI_S32 wlan_hdd_get_tdls_discovery_peer_cnt(tdlsCtx_t *pHddTdlsCtx)
{
    int i;
    struct list_head *head;
    struct list_head *pos, *q;
    int discovery_peer_cnt=0;
    hddTdlsPeer_t *tmp;

    /*
     * This function expects the callers to acquire the Mutex.
     */

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "%s, %d, " MAC_ADDRESS_STR, __func__, i,
                      MAC_ADDR_ARRAY(tmp->peerMac));
            discovery_peer_cnt++;
        }
    }
    return discovery_peer_cnt;
}

tANI_U16 wlan_hdd_tdlsConnectedPeers(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    return pHddCtx->connected_peer_count;
}

hddTdlsPeer_t *wlan_hdd_tdls_get_first_connected_peer(hdd_adapter_t *pAdapter)
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer = NULL;
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return NULL;
    }

    mutex_lock(&pHddCtx->tdls_lock);
    pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    if (NULL == pHddTdlsCtx) {
        mutex_unlock(&pHddCtx->tdls_lock);
        return NULL;
    }
    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each(pos, head) {
            curr_peer= list_entry (pos, hddTdlsPeer_t, node);
            if (curr_peer && (curr_peer->link_status == eTDLS_LINK_CONNECTED))
            {
               mutex_unlock(&pHddCtx->tdls_lock);
               VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                          "%s: " MAC_ADDRESS_STR " eTDLS_LINK_CONNECTED",
                           __func__, MAC_ADDR_ARRAY(curr_peer->peerMac));
               return curr_peer;
            }
        }
    }
    mutex_unlock(&pHddCtx->tdls_lock);
    return NULL;
}

int wlan_hdd_tdls_get_all_peers(hdd_adapter_t *pAdapter, char *buf, int buflen)
{
    int i;
    int len, init_len;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;
    tdlsCtx_t *pHddTdlsCtx;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    init_len = buflen;
    len = scnprintf(buf, buflen, "\n%-18s%-3s%-4s%-3s%-5s\n",
            "MAC", "Id", "cap", "up", "RSSI");
    buf += len;
    buflen -= len;
    /*                           1234567890123456789012345678901234567 */
    len = scnprintf(buf, buflen, "---------------------------------\n");
    buf += len;
    buflen -= len;

    mutex_lock(&pHddCtx->tdls_lock);
    pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    if (NULL == pHddTdlsCtx) {
        mutex_unlock(&pHddCtx->tdls_lock);
        len = scnprintf(buf, buflen, "TDLS not enabled\n");
        return len;
    }
    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each(pos, head) {
            curr_peer= list_entry (pos, hddTdlsPeer_t, node);

            if (buflen < 32+1)
                break;
            len = scnprintf(buf, buflen,
                MAC_ADDRESS_STR"%3d%4s%3s%5d\n",
                MAC_ADDR_ARRAY(curr_peer->peerMac),
                curr_peer->staId,
                (curr_peer->tdls_support == eTDLS_CAP_SUPPORTED) ? "Y":"N",
                TDLS_IS_CONNECTED(curr_peer) ? "Y":"N",
                curr_peer->rssi);
            buf += len;
            buflen -= len;
        }
    }
    mutex_unlock(&pHddCtx->tdls_lock);
    return init_len-buflen;
}

void wlan_hdd_tdls_connection_callback(hdd_adapter_t *pAdapter)
{
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if ((NULL == pHddCtx) || (NULL == pHddTdlsCtx))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
               FL("pHddCtx or  pHddTdlsCtx points to NULL"));
       return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
    "%s, update %d discover %d", __func__,
        pHddTdlsCtx->threshold_config.tx_period_t,
        pHddTdlsCtx->threshold_config.discovery_period_t);

    if (eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode)
    {
       wlan_hdd_tdls_peer_reset_discovery_processed(pHddTdlsCtx);
       pHddTdlsCtx->discovery_sent_cnt = 0;
       wlan_hdd_tdls_check_power_save_prohibited(pHddTdlsCtx->pAdapter);

#ifdef FEATURE_WLAN_TDLS_INTERNAL
       wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                   &pHddTdlsCtx->peerDiscoverTimer,
                                   pHddTdlsCtx->threshold_config.discovery_period_t);
#endif
       wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                   &pHddTdlsCtx->peerUpdateTimer,
                                   pHddTdlsCtx->threshold_config.tx_period_t);
    }
    mutex_unlock(&pHddCtx->tdls_lock);

}

void wlan_hdd_tdls_disconnection_callback(hdd_adapter_t *pAdapter)
{
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (NULL == pHddCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddCtx points to NULL"));
       return;
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"%s", __func__);

    mutex_lock(&pHddCtx->tdls_lock);
    if (NULL == pHddTdlsCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
        mutex_unlock(&pHddCtx->tdls_lock);
        return;
    }
    pHddTdlsCtx->discovery_sent_cnt = 0;
    wlan_hdd_tdls_check_power_save_prohibited(pHddTdlsCtx->pAdapter);

    wlan_hdd_tdls_monitor_timers_stop(pHddTdlsCtx);
    wlan_hdd_tdls_peer_timers_destroy(pHddTdlsCtx);
    wlan_hdd_tdls_free_list(pHddTdlsCtx);

    mutex_unlock(&pHddCtx->tdls_lock);
}

void wlan_hdd_tdls_mgmt_completion_callback(hdd_adapter_t *pAdapter, tANI_U32 statusCode)
{
    pAdapter->mgmtTxCompletionStatus = statusCode;
    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"%s: Mgmt TX Completion %d",
               __func__, statusCode);
    complete(&pAdapter->tdls_mgmt_comp);
}

void wlan_hdd_tdls_increment_peer_count(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    pHddCtx->connected_peer_count++;
    wlan_hdd_tdls_check_power_save_prohibited(pAdapter);

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "%s: %d",
               __func__, pHddCtx->connected_peer_count);

    mutex_unlock(&pHddCtx->tdls_lock);
}

void wlan_hdd_tdls_decrement_peer_count(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    if (pHddCtx->connected_peer_count)
        pHddCtx->connected_peer_count--;
    wlan_hdd_tdls_check_power_save_prohibited(pAdapter);

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "%s: %d",
               __func__, pHddCtx->connected_peer_count);

    mutex_unlock(&pHddCtx->tdls_lock);

}

void wlan_hdd_tdls_check_bmps(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    hddTdlsPeer_t *curr_peer;

    if ((NULL == pHddCtx) || (NULL == pHddTdlsCtx))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               FL(" pHddCtx or  pHddTdlsCtx points to NULL"));
       return;
    }

    curr_peer = wlan_hdd_tdls_is_progress(pHddCtx, NULL, 0);
    if (NULL != curr_peer)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                "%s: tdls in progress. Dont check for BMPS " MAC_ADDRESS_STR,
                __func__, MAC_ADDR_ARRAY (curr_peer->peerMac));
        return;
    }

    if ((TDLS_CTX_MAGIC != pHddCtx->tdls_scan_ctxt.magic) &&
        (0 == pHddCtx->connected_peer_count) &&
        (0 == pHddTdlsCtx->discovery_sent_cnt))
    {
        if (FALSE == sme_IsPmcBmps(WLAN_HDD_GET_HAL_CTX(pAdapter)))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: No TDLS peer connected/discovery sent. Enable BMPS",
                       __func__);
            hdd_enable_bmps_imps(pHddCtx);
        }
    }
    else
    {
        if (TRUE == sme_IsPmcBmps(WLAN_HDD_GET_HAL_CTX(pAdapter)))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: TDLS peer connected. Disable BMPS", __func__);
            hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_INFRA_STATION);
        }
    }
    return;
}

u8 wlan_hdd_tdls_is_peer_progress(hdd_adapter_t *pAdapter, u8 *mac)
{
    hddTdlsPeer_t *curr_peer;

    curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
    if (curr_peer == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: curr_peer is NULL", __func__);
        return 0;
    }

    return (eTDLS_LINK_CONNECTING == curr_peer->link_status);
}

/* return pointer to hddTdlsPeer_t if TDLS is ongoing. Otherwise return NULL.
 * mac - if NULL check for all the peer list, otherwise, skip this mac when skip_self is TRUE
 * skip_self - if TRUE, skip this mac. otherwise, check all the peer list. if
   mac is NULL, this argument is ignored, and check for all the peer list.
 */
static hddTdlsPeer_t *wlan_hdd_tdls_find_progress_peer(hdd_adapter_t *pAdapter, u8 *mac, u8 skip_self)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *curr_peer;
    struct list_head *pos;
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);;

    if (NULL == pHddTdlsCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
        return NULL;
    }

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each(pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);
            if (skip_self && mac && !memcmp(mac, curr_peer->peerMac, 6)) {
                continue;
            }
            else
            {
                if (eTDLS_LINK_CONNECTING == curr_peer->link_status)
                {
                  VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                            "%s:" MAC_ADDRESS_STR " eTDLS_LINK_CONNECTING",
                            __func__, MAC_ADDR_ARRAY(curr_peer->peerMac));
                  return curr_peer;
                }
            }
        }
    }
    return NULL;
}

hddTdlsPeer_t *wlan_hdd_tdls_is_progress(hdd_context_t *pHddCtx, u8 *mac, u8 skip_self)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter = NULL;
    tdlsCtx_t *pHddTdlsCtx = NULL;
    hddTdlsPeer_t *curr_peer= NULL;
    VOS_STATUS status = 0;

    mutex_lock(&pHddCtx->tdls_lock);

    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;

        pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
        if (NULL != pHddTdlsCtx)
        {
            curr_peer = wlan_hdd_tdls_find_progress_peer(pAdapter, mac, skip_self);
            if (curr_peer)
            {
                mutex_unlock(&pHddCtx->tdls_lock);
                return curr_peer;
            }
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    mutex_unlock(&pHddCtx->tdls_lock);
    return NULL;
}

static void wlan_hdd_tdls_implicit_disable(tdlsCtx_t *pHddTdlsCtx)
{
    wlan_hdd_tdls_timers_stop(pHddTdlsCtx);
}

static void wlan_hdd_tdls_implicit_enable(tdlsCtx_t *pHddTdlsCtx)
{
    wlan_hdd_tdls_peer_reset_discovery_processed(pHddTdlsCtx);
    pHddTdlsCtx->discovery_sent_cnt = 0;
    wlan_tdd_tdls_reset_tx_rx(pHddTdlsCtx);
    wlan_hdd_tdls_check_power_save_prohibited(pHddTdlsCtx->pAdapter);


#ifdef FEATURE_WLAN_TDLS_INTERNAL
    wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                &pHddTdlsCtx->peerDiscoverTimer,
                                pHddTdlsCtx->threshold_config.discovery_period_t);
#endif
    wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                &pHddTdlsCtx->peerUpdateTimer,
                                pHddTdlsCtx->threshold_config.tx_period_t);
}

void wlan_hdd_tdls_set_mode(hdd_context_t *pHddCtx,
                            eTDLSSupportMode tdls_mode,
                            v_BOOL_t bUpdateLast)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    VOS_STATUS status;
    hdd_adapter_t *pAdapter;
    tdlsCtx_t *pHddTdlsCtx;

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"%s mode %d", __func__, (int)tdls_mode);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    mutex_lock(&pHddCtx->tdls_lock);

    if (pHddCtx->tdls_mode == tdls_mode)
    {
        hddLog(TDLS_LOG_LEVEL, "%s already in mode %d", __func__, (int)tdls_mode);
        mutex_unlock(&pHddCtx->tdls_lock);
        return;
    }

    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );

    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
       pAdapter = pAdapterNode->pAdapter;
       pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
       if (NULL != pHddTdlsCtx)
       {
           if(eTDLS_SUPPORT_ENABLED == tdls_mode)
               wlan_hdd_tdls_implicit_enable(pHddTdlsCtx);
           else if((eTDLS_SUPPORT_DISABLED == tdls_mode) ||
                   (eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY == tdls_mode))
               wlan_hdd_tdls_implicit_disable(pHddTdlsCtx);
       }
       status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
       pAdapterNode = pNext;
    }
    if(bUpdateLast)
    {
        pHddCtx->tdls_mode_last = tdls_mode;
    }
    else
    {
        pHddCtx->tdls_mode_last = pHddCtx->tdls_mode;
    }
    pHddCtx->tdls_mode = tdls_mode;

    mutex_unlock(&pHddCtx->tdls_lock);
}

static void __wlan_hdd_tdls_pre_setup(struct work_struct *work)
{
    tdlsCtx_t *pHddTdlsCtx =
       container_of(work, tdlsCtx_t, implicit_setup);
    hdd_context_t *pHddCtx;
    hddTdlsPeer_t *curr_peer;
    hddTdlsPeer_t *temp_peer;
    int status;

    if (NULL == pHddTdlsCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddTdlsCtx is NULL"));
        return;
    }

    if (unlikely(TDLS_CTX_MAGIC != pHddTdlsCtx->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: tdls magic no missmatch %u", __func__, pHddTdlsCtx->magic);
        return;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pHddTdlsCtx->pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }

    curr_peer = pHddTdlsCtx->curr_candidate;

    if (NULL == curr_peer)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("curr_peer is NULL"));

        return;
    }

    if (TRUE == sme_IsPmcBmps(WLAN_HDD_GET_HAL_CTX(pHddTdlsCtx->pAdapter)))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"%s: Disable BMPS", __func__);
        hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_INFRA_STATION);
    }

    temp_peer = wlan_hdd_tdls_is_progress(pHddCtx, NULL, 0);
    if (NULL != temp_peer)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "%s: " MAC_ADDRESS_STR " ongoing. pre_setup ignored",
            __func__, MAC_ADDR_ARRAY(temp_peer->peerMac));
        goto done;
    }

    if (eTDLS_CAP_UNKNOWN != curr_peer->tdls_support)
        wlan_hdd_tdls_set_peer_link_status(curr_peer,
                                           eTDLS_LINK_DISCOVERING,
                                           eTDLS_LINK_SUCCESS);

    status = wlan_hdd_cfg80211_send_tdls_discover_req(pHddTdlsCtx->pAdapter->wdev.wiphy,
                                            pHddTdlsCtx->pAdapter->dev,
                                            curr_peer->peerMac);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "%s: " MAC_ADDRESS_STR " discovery could not sent",
            __func__, MAC_ADDR_ARRAY(curr_peer->peerMac));
        if (eTDLS_CAP_UNKNOWN != curr_peer->tdls_support)
            wlan_hdd_tdls_set_peer_link_status(curr_peer,
                                               eTDLS_LINK_IDLE,
                                               eTDLS_LINK_UNSPECIFIED);
        goto done;
    }

    pHddTdlsCtx->discovery_sent_cnt++;

    mutex_lock(&pHddCtx->tdls_lock);

    wlan_hdd_tdls_check_power_save_prohibited(pHddTdlsCtx->pAdapter);

    wlan_hdd_tdls_timer_restart(pHddTdlsCtx->pAdapter,
                                &pHddTdlsCtx->peerDiscoveryTimeoutTimer,
                                pHddTdlsCtx->threshold_config.tx_period_t - TDLS_DISCOVERY_TIMEOUT_BEFORE_UPDATE);

    mutex_unlock(&pHddCtx->tdls_lock);
    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "%s: discovery count %u timeout %u msec",
               __func__, pHddTdlsCtx->discovery_sent_cnt,
               pHddTdlsCtx->threshold_config.tx_period_t - TDLS_DISCOVERY_TIMEOUT_BEFORE_UPDATE);

done:
    pHddTdlsCtx->curr_candidate = NULL;
    pHddTdlsCtx->magic = 0;
    return;
}


static void wlan_hdd_tdls_pre_setup(struct work_struct *work)
{
    vos_ssr_protect(__func__);
    __wlan_hdd_tdls_pre_setup(work);
    vos_ssr_unprotect(__func__);

    return;
}

tANI_U32 wlan_hdd_tdls_discovery_sent_cnt(hdd_context_t *pHddCtx)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter = NULL;
    tdlsCtx_t *pHddTdlsCtx = NULL;
    VOS_STATUS status = 0;
    tANI_U32 count = 0;

    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;

        pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
        if (NULL != pHddTdlsCtx)
        {
            count = count + pHddTdlsCtx->discovery_sent_cnt;
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }
    return count;
}

void wlan_hdd_tdls_check_power_save_prohibited(hdd_adapter_t *pAdapter)
{
    tdlsCtx_t *pHddTdlsCtx = WLAN_HDD_GET_TDLS_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if ((NULL == pHddTdlsCtx) || (NULL == pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL(" pHddCtx or  pHddTdlsCtx points to NULL"));
        return;
    }

    if ((0 == pHddCtx->connected_peer_count) &&
        (0 == wlan_hdd_tdls_discovery_sent_cnt(pHddCtx)))
    {
        sme_SetTdlsPowerSaveProhibited(WLAN_HDD_GET_HAL_CTX(pHddTdlsCtx->pAdapter), 0);
        return;
    }
    sme_SetTdlsPowerSaveProhibited(WLAN_HDD_GET_HAL_CTX(pHddTdlsCtx->pAdapter), 1);
    return;
}

void wlan_hdd_tdls_free_scan_request (tdls_scan_context_t *tdls_scan_ctx)
{
    if (NULL == tdls_scan_ctx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("tdls_scan_ctx is NULL"));
        return;
    }

    tdls_scan_ctx->attempt = 0;
    tdls_scan_ctx->reject = 0;
    tdls_scan_ctx->magic = 0;
    tdls_scan_ctx->scan_request = NULL;
        return;
}

int wlan_hdd_tdls_copy_scan_context(hdd_context_t *pHddCtx,
                            struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request)
{
    tdls_scan_context_t *scan_ctx;

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    scan_ctx = &pHddCtx->tdls_scan_ctxt;

    scan_ctx->wiphy = wiphy;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
    scan_ctx->dev = dev;
#endif

    scan_ctx->scan_request = request;
    return 0;
}

static void wlan_hdd_tdls_scan_init_work(hdd_context_t *pHddCtx,
                                struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                struct net_device *dev,
#endif
                                struct cfg80211_scan_request *request,
                                unsigned long delay)
{
    if (TDLS_CTX_MAGIC != pHddCtx->tdls_scan_ctxt.magic)
    {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
        wlan_hdd_tdls_copy_scan_context(pHddCtx, wiphy, dev, request);
#else
        wlan_hdd_tdls_copy_scan_context(pHddCtx, wiphy, request);
#endif
        pHddCtx->tdls_scan_ctxt.attempt = 0;
        pHddCtx->tdls_scan_ctxt.magic = TDLS_CTX_MAGIC;
    }
    schedule_delayed_work(&pHddCtx->tdls_scan_ctxt.tdls_scan_work, delay);
}

/* return negative = caller should stop and return error code immediately
   return 0 = caller should stop and return success immediately
   return 1 = caller can continue to scan
 */
int wlan_hdd_tdls_scan_callback (hdd_adapter_t *pAdapter,
                                struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                struct net_device *dev,
#endif
                                struct cfg80211_scan_request *request)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    u16 connectedTdlsPeers;
    hddTdlsPeer_t *curr_peer, *connected_peer;
    unsigned long delay;
    hdd_config_t  *cfg_param = pHddCtx->cfg_ini;

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return 0;
    }

    /* if tdls is not enabled, then continue scan */
    if (eTDLS_SUPPORT_NOT_ENABLED == pHddCtx->tdls_mode)
        return 1;
    curr_peer = wlan_hdd_tdls_is_progress(pHddCtx, NULL, 0);
    if (NULL != curr_peer)
    {
        if (pHddCtx->tdls_scan_ctxt.reject++ >= TDLS_MAX_SCAN_REJECT)
        {
            pHddCtx->tdls_scan_ctxt.reject = 0;
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: " MAC_ADDRESS_STR ". scan rejected %d. force it to idle",
                    __func__, MAC_ADDR_ARRAY (curr_peer->peerMac), pHddCtx->tdls_scan_ctxt.reject);

            wlan_hdd_tdls_set_peer_link_status (curr_peer,
                                                eTDLS_LINK_IDLE,
                                                eTDLS_LINK_UNSPECIFIED);
            return 1;
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                "%s: tdls in progress. scan rejected %d",
                __func__, pHddCtx->tdls_scan_ctxt.reject);
        return -EBUSY;
    }

    /* tdls teardown is ongoing */
    if (eTDLS_SUPPORT_DISABLED == pHddCtx->tdls_mode)
    {
        connectedTdlsPeers = wlan_hdd_tdlsConnectedPeers(pAdapter);
        if (connectedTdlsPeers && (pHddCtx->tdls_scan_ctxt.attempt < TDLS_MAX_SCAN_SCHEDULE))
        {
            delay = (unsigned long)(TDLS_DELAY_SCAN_PER_CONNECTION*connectedTdlsPeers);
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: tdls disabled, but still connected_peers %d attempt %d. schedule scan %lu msec",
                    __func__, connectedTdlsPeers, pHddCtx->tdls_scan_ctxt.attempt, delay);

            wlan_hdd_tdls_scan_init_work (pHddCtx, wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                          dev,
#endif
                                          request,
                                          msecs_to_jiffies(delay));
            /* scan should not continue */
            return 0;
        }
        /* no connected peer or max retry reached, scan continue */
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                "%s: tdls disabled. connected_peers %d attempt %d. scan allowed",
                __func__, connectedTdlsPeers, pHddCtx->tdls_scan_ctxt.attempt);
        return 1;
    }
    /* while tdls is up, first time scan */
    else if (eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode ||
        eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY == pHddCtx->tdls_mode)
    {
        connectedTdlsPeers = wlan_hdd_tdlsConnectedPeers(pAdapter);

        /* check the TDLS link and Scan coexistance Capability */
        if ( (TRUE == pHddCtx->cfg_ini->fEnableTDLSScanCoexSupport) &&
             (TRUE == sme_IsFeatureSupportedByFW(TDLS_SCAN_COEXISTENCE)) &&
             (connectedTdlsPeers == 1) )
        {
            /* get connected peer information */
            connected_peer = wlan_hdd_tdls_get_first_connected_peer(pAdapter);
            if (NULL == connected_peer) {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                    "%s: Invalid connected_peer, Continue Scanning", __func__);
                /* scan should continue */
                return 1;
            }
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      ("%s: TDLS Scan Co-exist supported connectedTdlsPeers =%d buffersta =%d"),
                       __func__,connectedTdlsPeers,connected_peer->isBufSta);

            if (connected_peer->isBufSta)
            {
                 pHddCtx->isTdlsScanCoexistence = TRUE;
                 if ((cfg_param->dynSplitscan) && (!pHddCtx->issplitscan_enabled))
                 {
                           pHddCtx->issplitscan_enabled = TRUE;
                           sme_enable_disable_split_scan(
                                               WLAN_HDD_GET_HAL_CTX(pAdapter),
                                               cfg_param->nNumStaChanCombinedConc,
                                               cfg_param->nNumP2PChanCombinedConc);
                 }
                 VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                           ("%s:%d TDLS Scan Co-exist supported splitscan_enabled =%d "),
                            __func__, __LINE__, pHddCtx->issplitscan_enabled);
                 return 1;
            }

        }
        else
        {
            /* Throughput Monitor shall disable the split scan when
             * TDLS scan coexistance is disabled.At this point of time
             * since TDLS scan coexistance is not meeting the criteria
             * to be operational, explicitly make it false to enable
             * throughput monitor takes the control of split scan.
             */
            pHddCtx->isTdlsScanCoexistence = FALSE;
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  ("%s: TDLS Scan Co-exist not supported connectedTdlsPeers =%d"
                   " TDLSScanCoexSupport param =%d TDLS_SCAN_COEXISTENCE =%d"),
                    __func__, connectedTdlsPeers,
                    pHddCtx->cfg_ini->fEnableTDLSScanCoexSupport,
                    sme_IsFeatureSupportedByFW(TDLS_SCAN_COEXISTENCE));

        /* disable implicit trigger logic & tdls operatoin */
        wlan_hdd_tdls_set_mode(pHddCtx, eTDLS_SUPPORT_DISABLED, FALSE);
        /* fall back to the implementation of teardown the peers on the scan
         * when the number of connected peers are more than one. TDLS Scan
         * coexistance feature is exercised only when a single peer is
         * connected and the DUT shall not advertize the Buffer Sta capability,
         * so that the peer shall not go to the TDLS power save
         */

        if (connectedTdlsPeers)
        {
            tANI_U8 staIdx;
            hddTdlsPeer_t *curr_peer;

            for (staIdx = 0; staIdx < HDD_MAX_NUM_TDLS_STA; staIdx++)
            {
                if (pHddCtx->tdlsConnInfo[staIdx].staId)
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                                   ("%s: indicate TDLS teadown (staId %d)"), __func__, pHddCtx->tdlsConnInfo[staIdx].staId) ;

#ifdef CONFIG_TDLS_IMPLICIT
                    curr_peer = wlan_hdd_tdls_find_all_peer(pHddCtx, pHddCtx->tdlsConnInfo[staIdx].peerMac.bytes);
                    if(curr_peer)
                        wlan_hdd_tdls_indicate_teardown(curr_peer->pHddTdlsCtx->pAdapter, curr_peer, eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON);
#endif
                }
            }
            /* schedule scan */
            delay = (unsigned long)(TDLS_DELAY_SCAN_PER_CONNECTION*connectedTdlsPeers);

            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: tdls enabled (mode %d), connected_peers %d. schedule scan %lu msec",
                    __func__, pHddCtx->tdls_mode, wlan_hdd_tdlsConnectedPeers(pAdapter),
                    delay);

            wlan_hdd_tdls_scan_init_work (pHddCtx, wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                          dev,
#endif
                                          request,
                                          msecs_to_jiffies(delay));
            /* scan should not continue */
            return 0;
        }
        /* no connected peer, scan continue */
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                "%s: tdls_mode %d, and no tdls connection. scan allowed",
                 __func__, pHddCtx->tdls_mode);
    }
    return 1;
}

void wlan_hdd_tdls_scan_done_callback(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if(0 != (wlan_hdd_validate_context(pHddCtx)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddCtx is not valid"));
        return;
    }


    /* free allocated memory at scan time */
    wlan_hdd_tdls_free_scan_request (&pHddCtx->tdls_scan_ctxt);

    /* if tdls was enabled before scan, re-enable tdls mode */
    if(eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode_last ||
       eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY == pHddCtx->tdls_mode_last)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       ("%s: revert tdls mode %d"), __func__, pHddCtx->tdls_mode_last);

        wlan_hdd_tdls_set_mode(pHddCtx, pHddCtx->tdls_mode_last, FALSE);
    }
    wlan_hdd_tdls_check_bmps(pAdapter);
}

void wlan_hdd_tdls_timer_restart(hdd_adapter_t *pAdapter,
                                 vos_timer_t *timer,
                                 v_U32_t expirationTime)
{
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if (NULL == pHddStaCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("pHddStaCtx is NULL"));
        return;
    }

    /* Check whether driver load unload is in progress */
    if(vos_is_load_unload_in_progress( VOS_MODULE_ID_VOSS, NULL))
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Driver load/unload is in progress.", __func__);
       return;
    }

    if (hdd_connIsConnected(pHddStaCtx))
    {
        vos_timer_stop(timer);
        vos_timer_start(timer, expirationTime);
    }
}
void wlan_hdd_tdls_indicate_teardown(hdd_adapter_t *pAdapter,
                                           hddTdlsPeer_t *curr_peer,
                                           tANI_U16 reason)
{
    hdd_context_t *pHddCtx;

    if (NULL == pAdapter || NULL == curr_peer)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("parameters passed are invalid"));
        return;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (eTDLS_LINK_CONNECTED != curr_peer->link_status)
        return;

    /* Throughput Monitor shall disable the split scan when
     * TDLS scan coexistance is disabled.At this point of time
     * since TDLS scan coexistance is not meeting the criteria
     * to be operational, explicitly make it false to enable
     * throughput monitor takes the control of split scan.
     */
    if (pHddCtx && (pHddCtx->isTdlsScanCoexistence == TRUE))
    {
        pHddCtx->isTdlsScanCoexistence = FALSE;
    }

    wlan_hdd_tdls_set_peer_link_status(curr_peer,
                                       eTDLS_LINK_TEARING,
                                       eTDLS_LINK_UNSPECIFIED);
    cfg80211_tdls_oper_request(pAdapter->dev,
                               curr_peer->peerMac,
                               NL80211_TDLS_TEARDOWN,
                               reason,
                               GFP_KERNEL);
}


/*EXT TDLS*/
int wlan_hdd_set_callback(hddTdlsPeer_t *curr_peer,
                         cfg80211_exttdls_callback callback)
{

    hdd_context_t *pHddCtx;
    hdd_adapter_t   *pAdapter;

    if (!curr_peer) return -1;

    pAdapter = curr_peer->pHddTdlsCtx->pAdapter;
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if ((NULL == pHddCtx)) return -1;

    mutex_lock(&pHddCtx->tdls_lock);

    curr_peer->state_change_notification = callback;

    mutex_unlock(&pHddCtx->tdls_lock);
    return 0;


}

void wlan_hdd_tdls_get_wifi_hal_state(hddTdlsPeer_t *curr_peer,
                                      tANI_S32 *state,
                                      tANI_S32 *reason)
{
    *reason = curr_peer->reason;

    switch(curr_peer->link_status)
    {
        case eTDLS_LINK_IDLE:
        case eTDLS_LINK_DISCOVERED:
            *state = WIFI_TDLS_ENABLED;
            break;
        case eTDLS_LINK_DISCOVERING:
        case eTDLS_LINK_CONNECTING:
            *state = WIFI_TDLS_TRYING;
            break;
        case eTDLS_LINK_CONNECTED:
            *state = WIFI_TDLS_ESTABLISHED;
            break;
        case eTDLS_LINK_TEARING:
            *state = WIFI_TDLS_DROPPED;
            break;
    }

}

int wlan_hdd_tdls_get_status(hdd_adapter_t *pAdapter,
                             tANI_U8* mac,
                             tANI_S32 *state,
                             tANI_S32 *reason)
{

    hddTdlsPeer_t *curr_peer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
    if (curr_peer == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("curr_peer is NULL"));

        *state = WIFI_TDLS_DISABLED;
        *reason = eTDLS_LINK_UNSPECIFIED;
    }
    else
    {
        if (pHddCtx->cfg_ini->fTDLSExternalControl &&
           (FALSE == curr_peer->isForcedPeer))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      FL("curr_peer is not Forced"));
            *state = WIFI_TDLS_DISABLED;
            *reason = eTDLS_LINK_UNSPECIFIED;
        }
        else
        {
            wlan_hdd_tdls_get_wifi_hal_state(curr_peer, state, reason);
        }
    }
    return (0);
}

/*EXT TDLS*/

