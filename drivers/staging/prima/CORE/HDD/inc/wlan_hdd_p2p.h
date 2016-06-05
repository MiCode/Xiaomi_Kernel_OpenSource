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

#ifndef __P2P_H
#define __P2P_H
/**===========================================================================

\file         wlan_hdd_p2p.h

\brief       Linux HDD P2P include file

==========================================================================*/
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#define ACTION_FRAME_TX_TIMEOUT 2000
#define WAIT_CANCEL_REM_CHAN    1000
#define WAIT_REM_CHAN_READY     1000
#define WAIT_CHANGE_CHANNEL_FOR_OFFCHANNEL_TX 3000
#define READY_EVENT_PROPOGATE_TIME 2

#define ACTION_FRAME_DEFAULT_WAIT 200

#define ESTIMATED_ROC_DUR_REQD_FOR_ACTION_TX 20

#define WLAN_HDD_GET_TYPE_FRM_FC(__fc__)         (((__fc__) & 0x0F) >> 2)
#define WLAN_HDD_GET_SUBTYPE_FRM_FC(__fc__)      (((__fc__) & 0xF0) >> 4)
#define WLAN_HDD_80211_FRM_DA_OFFSET             4
#define P2P_WILDCARD_SSID_LEN                    7
#define P2P_WILDCARD_SSID                        "DIRECT-"
#define ACTION_FRAME_RSP_WAIT 500
#define ACTION_FRAME_ACK_WAIT 300
#ifdef WLAN_FEATURE_11W
#define WLAN_HDD_SET_WEP_FRM_FC(__fc__)     ( (__fc__) = ((__fc__) | 0x40))
#endif //WLAN_FEATURE_11W

#define HDD_P2P_MAX_ROC_DURATION            1000

enum hdd_rx_flags {
    HDD_RX_FLAG_DECRYPTED        = 1 << 0,
    HDD_RX_FLAG_MMIC_STRIPPED    = 1 << 1,
    HDD_RX_FLAG_IV_STRIPPED      = 1 << 2,
};


#define P2P_POWER_SAVE_TYPE_OPPORTUNISTIC        (1 << 0)
#define P2P_POWER_SAVE_TYPE_PERIODIC_NOA         (1 << 1)
#define P2P_POWER_SAVE_TYPE_SINGLE_NOA           (1 << 2)

#ifdef WLAN_FEATURE_P2P_DEBUG
typedef enum  { P2P_NOT_ACTIVE,
                P2P_GO_NEG_PROCESS,
                P2P_GO_NEG_COMPLETED,
                P2P_CLIENT_CONNECTING_STATE_1,
                P2P_GO_COMPLETED_STATE,
                P2P_CLIENT_CONNECTED_STATE_1,
                P2P_CLIENT_DISCONNECTED_STATE,
                P2P_CLIENT_CONNECTING_STATE_2,
                P2P_CLIENT_COMPLETED_STATE
               }tP2PConnectionStatus;

extern tP2PConnectionStatus globalP2PConnectionStatus;
#endif

typedef struct p2p_app_setP2pPs{
   tANI_U8     opp_ps;
   tANI_U32     ctWindow;
   tANI_U8     count;
   tANI_U32     duration;
   tANI_U32    interval;
   tANI_U32    single_noa_duration;
   tANI_U8      psSelection;
}p2p_app_setP2pPs_t;

int wlan_hdd_cfg80211_remain_on_channel( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
                                struct wireless_dev *wdev,
#else
                                struct net_device *dev,
#endif
                                struct ieee80211_channel *chan,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
                                enum nl80211_channel_type channel_type,
#endif
                                unsigned int duration, u64 *cookie );

int wlan_hdd_cfg80211_cancel_remain_on_channel( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
                                       struct wireless_dev *wdev,
#else
                                       struct net_device *dev,
#endif
                                       u64 cookie );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
int wlan_hdd_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy, 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
                                          struct wireless_dev *wdev,
#else
                                          struct net_device *dev,
#endif
                                          u64 cookie);
#endif

int hdd_setP2pPs( struct net_device *dev, void *msgData );
int hdd_setP2pOpps( struct net_device *dev, tANI_U8 *command );
int hdd_setP2pNoa( struct net_device *dev, tANI_U8 *command );

void __hdd_indicate_mgmt_frame(hdd_adapter_t *pAdapter,
                            tANI_U32 nFrameLength, tANI_U8* pbFrames,
                            tANI_U8 frameType,
                            tANI_U32 rxChan, tANI_S8 rxRssi);
void hdd_remainChanReadyHandler( hdd_adapter_t *pAdapter );
void hdd_sendActionCnf( hdd_adapter_t *pAdapter, tANI_BOOLEAN actionSendSuccess );
int wlan_hdd_check_remain_on_channel(hdd_adapter_t *pAdapter);
VOS_STATUS wlan_hdd_cancel_existing_remain_on_channel(hdd_adapter_t *pAdapter);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
                     struct cfg80211_mgmt_tx_params *params, u64 *cookie);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
int wlan_hdd_mgmt_tx( struct wiphy *wiphy, struct wireless_dev *wdev,
                     struct ieee80211_channel *chan, bool offchan,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
                     enum nl80211_channel_type channel_type,
                     bool channel_type_valid,
#endif
                     unsigned int wait,
                     const u8 *buf, size_t len,  bool no_cck,
                     bool dont_wait_for_ack, u64 *cookie );
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
int wlan_hdd_mgmt_tx( struct wiphy *wiphy, struct net_device *dev,
                     struct ieee80211_channel *chan, bool offchan,
                     enum nl80211_channel_type channel_type,
                     bool channel_type_valid, unsigned int wait,
                     const u8 *buf, size_t len,  bool no_cck,
                     bool dont_wait_for_ack, u64 *cookie );
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
int wlan_hdd_mgmt_tx( struct wiphy *wiphy, struct net_device *dev,
                     struct ieee80211_channel *chan, bool offchan,
                     enum nl80211_channel_type channel_type,
                     bool channel_type_valid, unsigned int wait,
                     const u8 *buf, size_t len, u64 *cookie );
#else
int wlan_hdd_mgmt_tx( struct wiphy *wiphy, struct net_device *dev,
                     struct ieee80211_channel *chan,
                     enum nl80211_channel_type channel_type,
                     bool channel_type_valid,
                     const u8 *buf, size_t len, u64 *cookie );
#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
struct wireless_dev* wlan_hdd_add_virtual_intf(
                  struct wiphy *wiphy, const char *name,
                  enum nl80211_iftype type,
                  u32 *flags, struct vif_params *params );
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
struct wireless_dev* wlan_hdd_add_virtual_intf(
                  struct wiphy *wiphy, char *name, enum nl80211_iftype type,
                  u32 *flags, struct vif_params *params );
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
struct net_device* wlan_hdd_add_virtual_intf(
                  struct wiphy *wiphy, char *name, enum nl80211_iftype type,
                  u32 *flags, struct vif_params *params );
#else
int wlan_hdd_add_virtual_intf( struct wiphy *wiphy, char *name,
                               enum nl80211_iftype type,
                               u32 *flags, struct vif_params *params );
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
int wlan_hdd_del_virtual_intf( struct wiphy *wiphy, struct wireless_dev *wdev );
#else
int wlan_hdd_del_virtual_intf( struct wiphy *wiphy, struct net_device *dev );
#endif
void hdd_p2p_roc_work_queue(struct work_struct *work);
#endif // __P2P_H
