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

#ifndef __HDD_TDSL_H
#define __HDD_TDSL_H
/**===========================================================================

\file         wlan_hdd_tdls.h

\brief       Linux HDD TDLS include file
==========================================================================*/

#ifdef FEATURE_WLAN_TDLS

#define MAX_NUM_TDLS_PEER           3

#define TDLS_SUB_DISCOVERY_PERIOD   100

#define TDLS_MAX_DISCOVER_REQS_PER_TIMER 1

#define TDLS_DISCOVERY_PERIOD       3600000

#define TDLS_TX_STATS_PERIOD        3600000

#define TDLS_IMPLICIT_TRIGGER_PKT_THRESHOLD     100

#define TDLS_RX_IDLE_TIMEOUT        5000

#define TDLS_RSSI_TRIGGER_HYSTERESIS 50

/* before UpdateTimer expires, we want to timeout discovery response.
should not be more than 2000 */
#define TDLS_DISCOVERY_TIMEOUT_BEFORE_UPDATE     1000

#define TDLS_CTX_MAGIC 0x54444c53    // "TDLS"

#define TDLS_MAX_SCAN_SCHEDULE          10
#define TDLS_MAX_SCAN_REJECT            5
#define TDLS_DELAY_SCAN_PER_CONNECTION 100

#define TDLS_IS_CONNECTED(peer)  \
        ((eTDLS_LINK_CONNECTED == (peer)->link_status) || \
         (eTDLS_LINK_TEARING == (peer)->link_status))

/* TDLS Off Channel Bandwidth Offset */
#define TDLS_OFF_CHANNEL_BW_OFFSET  0

/* TDLS Channel Switch Request */
#define TDLS_CHANNEL_SWITCH_ENABLE  1
#define TDLS_CHANNEL_SWITCH_DISABLE 2

typedef struct
{
    tANI_U32    tdls;
    tANI_U32    tx_period_t;
    tANI_U32    tx_packet_n;
    tANI_U32    discovery_period_t;
    tANI_U32    discovery_tries_n;
    tANI_U32    idle_timeout_t;
    tANI_U32    idle_packet_n;
    tANI_U32    rssi_hysteresis;
    tANI_S32    rssi_trigger_threshold;
    tANI_S32    rssi_teardown_threshold;
} tdls_config_params_t;

typedef struct
{
    struct wiphy *wiphy;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
    struct net_device *dev;
#endif
    struct cfg80211_scan_request *scan_request;
    int magic;
    int attempt;
    int reject;
    struct delayed_work tdls_scan_work;
} tdls_scan_context_t;

typedef enum {
    eTDLS_SUPPORT_NOT_ENABLED = 0,
    eTDLS_SUPPORT_DISABLED, /* suppress implicit trigger and not respond to the peer */
    eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY, /* suppress implicit trigger, but respond to the peer */
    eTDLS_SUPPORT_ENABLED, /* implicit trigger */
} eTDLSSupportMode;

typedef enum eTDLSCapType{
    eTDLS_CAP_NOT_SUPPORTED = -1,
    eTDLS_CAP_UNKNOWN = 0,
    eTDLS_CAP_SUPPORTED = 1,
} tTDLSCapType;

typedef enum eTDLSLinkStatus {
    eTDLS_LINK_IDLE = 0,
    eTDLS_LINK_DISCOVERING,
    eTDLS_LINK_DISCOVERED,
    eTDLS_LINK_CONNECTING,
    eTDLS_LINK_CONNECTED,
    eTDLS_LINK_TEARING,
} tTDLSLinkStatus;


typedef enum {
    eTDLS_LINK_SUCCESS,                              /* Success */
    eTDLS_LINK_UNSPECIFIED           = -1,           /* Unspecified reason */
    eTDLS_LINK_NOT_SUPPORTED         = -2,           /* Remote side doesn't support TDLS */
    eTDLS_LINK_UNSUPPORTED_BAND      = -3,           /* Remote side doesn't support this band */
    eTDLS_LINK_NOT_BENEFICIAL        = -4,           /* Going to AP is better than going direct */
    eTDLS_LINK_DROPPED_BY_REMOTE     = -5            /* Remote side doesn't want it anymore */
} tTDLSLinkReason;

typedef struct {
    int channel;                        /* channel hint, in channel number (NOT frequency ) */
    int global_operating_class;         /* operating class to use */
    int max_latency_ms;                 /* max latency that can be tolerated by apps */
    int min_bandwidth_kbps;             /* bandwidth required by apps, in kilo bits per second */
} tdls_req_params_t;

typedef enum {
    WIFI_TDLS_DISABLED,                 /* TDLS is not enabled, or is disabled now */
    WIFI_TDLS_ENABLED,                  /* TDLS is enabled, but not yet tried */
    WIFI_TDLS_TRYING,                   /* Direct link is being attempted (optional) */
    WIFI_TDLS_ESTABLISHED,              /* Direct link is established */
    WIFI_TDLS_ESTABLISHED_OFF_CHANNEL,  /* Direct link is established using MCC */
    WIFI_TDLS_DROPPED,                  /* Direct link was established, but is now dropped */
    WIFI_TDLS_FAILED                    /* Direct link failed */
} tdls_state_t;

typedef int (*cfg80211_exttdls_callback)(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                         const tANI_U8* mac,
#else
                                         tANI_U8* mac,
#endif
                                         tANI_S32 state,
                                         tANI_S32 reason,
                                         void *ctx);
typedef struct {
    tANI_U16    period;
    tANI_U16    bytes;
} tdls_tx_tput_config_t;

typedef struct {
    tANI_U16    period;
    tANI_U16    tries;
} tdls_discovery_config_t;

typedef struct {
    tANI_U16    timeout;
} tdls_rx_idle_config_t;

typedef struct {
    tANI_U16    rssi_thres;
} tdls_rssi_config_t;

struct _hddTdlsPeer_t;

typedef struct {
    struct list_head peer_list[256];
    hdd_adapter_t   *pAdapter;
#ifdef TDLS_USE_SEPARATE_DISCOVERY_TIMER
    vos_timer_t     peerDiscoverTimer;
#endif
    vos_timer_t     peerUpdateTimer;
    vos_timer_t     peerDiscoveryTimeoutTimer;
    tdls_config_params_t threshold_config;
    tANI_S32        discovery_peer_cnt;
    tANI_U32        discovery_sent_cnt;
    tANI_S8         ap_rssi;
    struct _hddTdlsPeer_t  *curr_candidate;
    struct work_struct implicit_setup;
    v_U32_t            magic;
} tdlsCtx_t;

typedef struct _hddTdlsPeer_t {
    struct list_head node;
    tdlsCtx_t   *pHddTdlsCtx;
    tSirMacAddr peerMac;
    tANI_U16    staId ;
    tANI_S8     rssi;
    tTDLSCapType     tdls_support;
    tTDLSLinkStatus  link_status;
    tANI_U8     signature;
    tANI_U8     is_responder;
    tANI_U8     discovery_processed;
    tANI_U16    discovery_attempt;
    tANI_U16    tx_pkt;
    tANI_U16    rx_pkt;
    tANI_U8     uapsdQueues;
    tANI_U8     qos;
    tANI_U8     maxSp;
    tANI_U8     isBufSta;
    tANI_U8     isOffChannelSupported;
    tANI_U8     supported_channels_len;
    tANI_U8     supported_channels[SIR_MAC_MAX_SUPP_CHANNELS];
    tANI_U8     supported_oper_classes_len;
    tANI_U8     supported_oper_classes[SIR_MAC_MAX_SUPP_OPER_CLASSES];
    vos_timer_t     peerIdleTimer;
    vos_timer_t     initiatorWaitTimeoutTimer;
    tANI_BOOLEAN isForcedPeer;
    /*EXT TDLS*/
    tTDLSLinkReason reason;
    cfg80211_exttdls_callback state_change_notification;
    tANI_BOOLEAN   isOffChannelConfigured;
    tANI_BOOLEAN   isOffChannelEstablished;
    tdls_req_params_t peerParams;
} hddTdlsPeer_t;

typedef struct {
    /* Session ID */
    tANI_U8 sessionId;
    /*TDLS peer station id */
    v_U8_t staId;
    /* TDLS peer mac Address */
    v_MACADDR_t peerMac;
} tdlsConnInfo_t;

int wlan_hdd_sta_tdls_init(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_init(hdd_context_t * pHddCtx);

void wlan_hdd_tdls_exit(hdd_adapter_t *pAdapter, tANI_BOOLEAN mutexLock);

void wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac);

void wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac);

int wlan_hdd_tdls_increment_pkt_count(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                      const u8 *mac,
#else
                                      u8 *mac,
#endif
                                      u8 tx);

int wlan_hdd_tdls_set_sta_id(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                             const u8 *mac,
#else
                             u8 *mac,
#endif
                             u8 staId);

hddTdlsPeer_t *wlan_hdd_tdls_find_peer(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                       const u8 *mac,
#else
                                       u8 *mac,
#endif
                                       tANI_BOOLEAN mutexLock);

hddTdlsPeer_t *wlan_hdd_tdls_find_all_peer(hdd_context_t *pHddCtx,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                           const u8 *mac
#else
                                           u8 *mac
#endif
);

int wlan_hdd_tdls_get_link_establish_params(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                            const u8 *mac,
#else
                                            u8 *mac,
#endif
                                            tCsrTdlsLinkEstablishParams* tdlsLinkEstablishParams);
hddTdlsPeer_t *wlan_hdd_tdls_get_peer(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                      const u8 *mac
#else
                                      u8 *mac
#endif
                                     );

int wlan_hdd_tdls_set_cap(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                          const u8* mac,
#else
                          u8* mac,
#endif
                          tTDLSCapType cap);

void wlan_hdd_tdls_set_peer_link_status(hddTdlsPeer_t *curr_peer,
                                        tTDLSLinkStatus status,
                                        tTDLSLinkReason reason);
void wlan_hdd_tdls_set_link_status(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                   const u8 *mac,
#else
                                   u8 *mac,
#endif
                                   tTDLSLinkStatus linkStatus,
                                   tTDLSLinkReason reason);

int wlan_hdd_tdls_recv_discovery_resp(hdd_adapter_t *pAdapter, u8 *mac);

int wlan_hdd_tdls_set_peer_caps(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                const u8 *mac,
#else
                                u8 *mac,
#endif
                                tCsrStaParams *StaParams,
                                tANI_BOOLEAN isBufSta,
                                tANI_BOOLEAN isOffChannelSupported);

int wlan_hdd_tdls_set_rssi(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                           const u8 *mac,
#else
                           u8 *mac,
#endif
                           tANI_S8 rxRssi);

int wlan_hdd_tdls_set_responder(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                const u8 *mac,
#else
                                u8 *mac,
#endif
                                tANI_U8 responder);

int wlan_hdd_tdls_get_responder(hdd_adapter_t *pAdapter, u8 *mac);

int wlan_hdd_tdls_set_signature(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                const u8 *mac,
#else
                                u8 *mac,
#endif
                                tANI_U8 uSignature);

int wlan_hdd_tdls_set_params(struct net_device *dev, tdls_config_params_t *config);

int wlan_hdd_tdls_reset_peer(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                             const u8 *mac
#else
                             u8 *mac
#endif
                             );

tANI_U16 wlan_hdd_tdlsConnectedPeers(hdd_adapter_t *pAdapter);

int wlan_hdd_tdls_get_all_peers(hdd_adapter_t *pAdapter, char *buf, int buflen);

void wlan_hdd_tdls_connection_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_disconnection_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_mgmt_completion_callback(hdd_adapter_t *pAdapter, tANI_U32 statusCode);

void wlan_hdd_tdls_increment_peer_count(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_decrement_peer_count(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_check_bmps(hdd_adapter_t *pAdapter);

u8 wlan_hdd_tdls_is_peer_progress(hdd_adapter_t *pAdapter, u8 *mac);

hddTdlsPeer_t *wlan_hdd_tdls_is_progress(hdd_context_t *pHddCtx,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                         const u8 *mac,
#else
                                         u8 *mac,
#endif
                                         u8 skip_self, tANI_BOOLEAN mutexLock);

void wlan_hdd_tdls_set_mode(hdd_context_t *pHddCtx,
                            eTDLSSupportMode tdls_mode,
                            v_BOOL_t bUpdateLast);

tANI_U32 wlan_hdd_tdls_discovery_sent_cnt(hdd_context_t *pHddCtx);

void wlan_hdd_tdls_check_power_save_prohibited(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_free_scan_request (tdls_scan_context_t *tdls_scan_ctx);

int wlan_hdd_tdls_copy_scan_context(hdd_context_t *pHddCtx,
                            struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request);

int wlan_hdd_tdls_scan_callback (hdd_adapter_t *pAdapter,
                                struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                struct net_device *dev,
#endif
                                struct cfg80211_scan_request *request);

void wlan_hdd_tdls_scan_done_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_timer_restart(hdd_adapter_t *pAdapter,
                                 vos_timer_t *timer,
                                 v_U32_t expirationTime);
void wlan_hdd_tdls_indicate_teardown(hdd_adapter_t *pAdapter,
                                     hddTdlsPeer_t *curr_peer,
                                     tANI_U16 reason);

int wlan_hdd_tdls_set_force_peer(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                 const u8 *mac,
#else
                                 u8 *mac,
#endif
                                 tANI_BOOLEAN forcePeer);
int wlan_hdd_tdls_extctrl_deconfig_peer(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                        const u8 *peer
#else
                                        u8 *peer
#endif
);
int wlan_hdd_tdls_extctrl_config_peer(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                      const u8 *peer,
#else
                                      u8 *peer,
#endif
                                      tdls_req_params_t *tdls_peer_params,
                                      cfg80211_exttdls_callback callback);
void hdd_tdls_notify_mode_change(hdd_adapter_t *pAdapter,
                                 hdd_context_t *pHddCtx);
void wlan_hdd_tdls_disable_offchan_and_teardown_links(hdd_context_t *pHddCtx);
/*EXT TDLS*/
int wlan_hdd_tdls_get_status(hdd_adapter_t *pAdapter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                             const tANI_U8* mac,
#else
                             tANI_U8* mac,
#endif
                             tANI_S32 *state,
                             tANI_S32 *reason);
void wlan_hdd_tdls_get_wifi_hal_state(hddTdlsPeer_t *curr_peer,
                                      tANI_S32 *state,
                                      tANI_S32 *reason);
int wlan_hdd_set_callback(hddTdlsPeer_t *curr_peer,
                          cfg80211_exttdls_callback callback);
int hdd_set_tdls_scan_type(hdd_adapter_t *pAdapter,
                   tANI_U8 *ptr);

// tdlsoffchan
hddTdlsPeer_t *wlan_hdd_tdls_get_connected_peer(hdd_adapter_t *pAdapter);

int wlan_hdd_validate_tdls_context(hdd_context_t *pHddCtx, tdlsCtx_t *pTdlsCtx);

void wlan_hdd_tdls_reenable(hdd_context_t *pHddCtx);
#else
static inline void hdd_tdls_notify_mode_change(hdd_adapter_t *pAdapter,
                                               hdd_context_t *pHddCtx)
{
}
static inline void
wlan_hdd_tdls_disable_offchan_and_teardown_links(hdd_context_t *pHddCtx)
{
}
static inline void
wlan_hdd_tdls_reenable(hdd_context_t *pHddCtx)
{
}
#endif
void wlan_hdd_tdls_update_rx_pkt_cnt_n_rssi(hdd_adapter_t *pAdapter,
        u8 *mac, v_S7_t rssiAvg);


tdlsConnInfo_t *wlan_hdd_get_conn_info(hdd_context_t *pHddCtx,
                                       tANI_U8 idx);

v_VOID_t wlan_hdd_tdls_initiator_wait_cb(v_PVOID_t userData);

#endif // __HDD_TDSL_H
