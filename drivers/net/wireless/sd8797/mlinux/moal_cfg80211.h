/** @file moal_cfg80211.h
  *
  * @brief This file contains the CFG80211 specific defines.
  *
  * Copyright (C) 2011-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

#ifndef _MOAL_CFG80211_H_
#define _MOAL_CFG80211_H_

#include    "moal_main.h"

/* Clear all key indexes */
#define KEY_INDEX_CLEAR_ALL             (0x0000000F)

/** RTS/FRAG disabled value */
#define MLAN_FRAG_RTS_DISABLED          (0xFFFFFFFF)

#ifndef WLAN_CIPHER_SUITE_WAPI
#define WLAN_CIPHER_SUITE_WAPI          0x00000020
#endif

/* define for custom ie operation */
#define MLAN_CUSTOM_IE_AUTO_IDX_MASK    0xffff
#define MLAN_CUSTOM_IE_DELETE_MASK      0x0
#define MLAN_CUSTOM_IE_NEW_MASK      	0x8000
#define TLV_TYPE_MGMT_IE                0x0169
#define MGMT_MASK_ASSOC_REQ             0x01
#define MGMT_MASK_REASSOC_REQ           0x04
#define MGMT_MASK_ASSOC_RESP            0x02
#define MGMT_MASK_REASSOC_RESP          0x08
#define MGMT_MASK_PROBE_REQ             0x10
#define MGMT_MASK_PROBE_RESP            0x20
#define MGMT_MASK_BEACON                0x100
#define MGMT_MASK_BEACON_WPS_P2P        0x8000
#define IE_MASK_WPS						0x0001
#define IE_MASK_P2P						0x0002
#define IE_MASK_WFD						0x0004

/**
 * If multiple wiphys are registered e.g. a regular netdev with
 * assigned ieee80211_ptr and you won't know whether it points
 * to a wiphy your driver has registered or not. Assign this to
 * something global to your driver to help determine whether
 * you own this wiphy or not.
 */
static const void *const mrvl_wiphy_privid = &mrvl_wiphy_privid;

/* Get the private structure from wiphy */
void *woal_get_wiphy_priv(struct wiphy *wiphy);

/* Get the private structure from net device */
void *woal_get_netdev_priv(struct net_device *dev);

t_u8 woal_band_cfg_to_ieee_band(t_u32 band);

int woal_cfg80211_change_virtual_intf(struct wiphy *wiphy,
				      struct net_device *dev,
				      enum nl80211_iftype type,
				      u32 * flags, struct vif_params *params);

int woal_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed);

int woal_cfg80211_add_key(struct wiphy *wiphy,
			  struct net_device *dev, t_u8 key_index,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36) || defined(COMPAT_WIRELESS)
			  bool pairwise,
#endif
			  const t_u8 * mac_addr, struct key_params *params);

int woal_cfg80211_del_key(struct wiphy *wiphy,
			  struct net_device *dev, t_u8 key_index,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36) || defined(COMPAT_WIRELESS)
			  bool pairwise,
#endif
			  const t_u8 * mac_addr);

int woal_cfg80211_set_bitrate_mask(struct wiphy *wiphy,
				   struct net_device *dev,
				   const u8 * peer,
				   const struct cfg80211_bitrate_mask *mask);

int woal_cfg80211_set_antenna(struct wiphy *wiphy, u32 tx_ant, u32 rx_ant);

#ifdef STA_CFG80211
#ifdef STA_SUPPORT
int woal_set_rf_channel(moal_private * priv,
			struct ieee80211_channel *chan,
			enum nl80211_channel_type channel_type);
mlan_status woal_inform_bss_from_scan_result(moal_private * priv,
					     mlan_ssid_bssid * ssid_bssid,
					     t_u8 wait_option);
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
int woal_cfg80211_set_channel(struct wiphy *wiphy,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34) || defined(COMPAT_WIRELESS)
			      struct net_device *dev,
#endif
			      struct ieee80211_channel *chan,
			      enum nl80211_channel_type channel_type);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37) || defined(COMPAT_WIRELESS)
int woal_cfg80211_set_default_key(struct wiphy *wiphy,
				  struct net_device *dev, t_u8 key_index,
				  bool ucast, bool mcast);
#else
int woal_cfg80211_set_default_key(struct wiphy *wiphy,
				  struct net_device *dev, t_u8 key_index);
#endif

void woal_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
				       struct wireless_dev *wdev,
#else
				       struct net_device *dev,
#endif
				       t_u16 frame_type, bool reg);

int woal_cfg80211_mgmt_tx(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
			  struct wireless_dev *wdev,
#else
			  struct net_device *dev,
#endif
			  struct ieee80211_channel *chan, bool offchan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
			  enum nl80211_channel_type channel_type,
			  bool channel_type_valid,
#endif
			  unsigned int wait, const u8 * buf, size_t len,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0) || defined(COMPAT_WIRELESS)
			  bool no_cck,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
			  bool dont_wait_for_ack,
#endif
			  u64 * cookie);

mlan_status woal_register_cfg80211(moal_private * priv);

extern struct ieee80211_supported_band cfg80211_band_2ghz;
extern struct ieee80211_supported_band cfg80211_band_5ghz;
extern const u32 cfg80211_cipher_suites[10];

#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
int woal_cfg80211_bss_role_cfg(moal_private * priv, t_u16 action,
			       t_u8 * bss_role);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
struct wireless_dev *woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
						    const char *name,
						    enum nl80211_iftype type,
						    u32 * flags,
						    struct vif_params *params);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
struct wireless_dev *woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
						    char *name,
						    enum nl80211_iftype type,
						    u32 * flags,
						    struct vif_params *params);
#else
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37) || defined(COMPAT_WIRELESS)
struct net_device *woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
						  char *name,
						  enum nl80211_iftype type,
						  u32 * flags,
						  struct vif_params *params);
#else
int woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
				   char *name, enum nl80211_iftype type,
				   u32 * flags, struct vif_params *params);
#endif
#endif
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
int woal_cfg80211_del_virtual_intf(struct wiphy *wiphy,
				   struct wireless_dev *wdev);
#else
int woal_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct net_device *dev);
#endif

#if defined(WIFI_DIRECT_SUPPORT)
/** Define kernel version for wifi direct */
#if !defined(COMPAT_WIRELESS)
#define WIFI_DIRECT_KERNEL_VERSION          KERNEL_VERSION(2, 6, 39)
#else
#define WIFI_DIRECT_KERNEL_VERSION          KERNEL_VERSION(2, 6, 33)
#endif /* COMPAT_WIRELESS */
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
/** Define for remain on channel duration timer */
#define MAX_REMAIN_ON_CHANNEL_DURATION      (1000 * 5)

int woal_cfg80211_init_p2p_client(moal_private * priv);

int woal_cfg80211_init_p2p_go(moal_private * priv);

int woal_cfg80211_deinit_p2p(moal_private * priv);

int woal_cfg80211_remain_on_channel_cfg(moal_private * priv,
					t_u8 wait_option, t_u8 remove,
					t_u8 * status,
					struct ieee80211_channel *chan,
					enum nl80211_channel_type channel_type,
					t_u32 duration);
int woal_uap_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
				  u8 * mac, struct station_info *stainfo);

void woal_remove_virtual_interface(moal_handle * handle);

#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */

#ifdef UAP_CFG80211
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
int woal_cfg80211_add_beacon(struct wiphy *wiphy,
			     struct net_device *dev,
			     struct cfg80211_ap_settings *params);

int woal_cfg80211_set_beacon(struct wiphy *wiphy,
			     struct net_device *dev,
			     struct cfg80211_beacon_data *params);
#else
int woal_cfg80211_add_beacon(struct wiphy *wiphy,
			     struct net_device *dev,
			     struct beacon_parameters *params);

int woal_cfg80211_set_beacon(struct wiphy *wiphy,
			     struct net_device *dev,
			     struct beacon_parameters *params);
#endif

int woal_cfg80211_del_beacon(struct wiphy *wiphy, struct net_device *dev);
int woal_cfg80211_del_station(struct wiphy *wiphy,
			      struct net_device *dev, u8 * mac_addr);
#endif

const t_u8 *woal_parse_ie_tlv(const t_u8 * ie, int len, t_u8 id);

void woal_clear_all_mgmt_ies(moal_private * priv);
int woal_cfg80211_mgmt_frame_ie(moal_private * priv,
				const t_u8 * beacon_ies, size_t beacon_ies_len,
				const t_u8 * proberesp_ies,
				size_t proberesp_ies_len,
				const t_u8 * assocresp_ies,
				size_t assocresp_ies_len,
				const t_u8 * probereq_ies,
				size_t probereq_ies_len, t_u16 mask);

t_u8 woal_is_any_interface_active(moal_handle * handle);

int woal_get_active_intf_freq(moal_handle * handle);

void woal_cfg80211_setup_ht_cap(struct ieee80211_sta_ht_cap *ht_info,
				t_u32 dev_cap, t_u8 * mcs_set);
int woal_cfg80211_assoc(moal_private * priv, void *sme);

#endif /* _MOAL_CFG80211_H_ */
