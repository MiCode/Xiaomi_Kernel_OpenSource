/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 *        os/none/include/gl_p2p_ioctl.h#9
 */

/*! \file   gl_p2p_ioctl.h
 *    \brief  This file is for custom ioctls for Wi-Fi Direct only
 */


#ifndef _GL_P2P_IOCTL_H
#define _GL_P2P_IOCTL_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */
#include "wlan_oid.h"

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/* (WirelessExtension) Private I/O Controls */
#define IOC_P2P_CFG_DEVICE              (SIOCIWFIRSTPRIV+0)
#define IOC_P2P_PROVISION_COMPLETE      (SIOCIWFIRSTPRIV+2)
#define IOC_P2P_START_STOP_DISCOVERY    (SIOCIWFIRSTPRIV+4)
#define IOC_P2P_DISCOVERY_RESULTS       (SIOCIWFIRSTPRIV+5)
#define IOC_P2P_WSC_BEACON_PROBE_RSP_IE (SIOCIWFIRSTPRIV+6)
#define IOC_P2P_GO_WSC_IE               IOC_P2P_WSC_BEACON_PROBE_RSP_IE
#define IOC_P2P_CONNECT_DISCONNECT      (SIOCIWFIRSTPRIV+8)
#define IOC_P2P_PASSWORD_READY          (SIOCIWFIRSTPRIV+10)
/* #define IOC_P2P_SET_PWR_MGMT_PARAM      (SIOCIWFIRSTPRIV+12) */
#define IOC_P2P_SET_INT                 (SIOCIWFIRSTPRIV+12)
#define IOC_P2P_GET_STRUCT              (SIOCIWFIRSTPRIV+13)
#define IOC_P2P_SET_STRUCT              (SIOCIWFIRSTPRIV+14)
#define IOC_P2P_GET_REQ_DEVICE_INFO     (SIOCIWFIRSTPRIV+15)

#define PRIV_CMD_INT_P2P_SET            0

/* IOC_P2P_PROVISION_COMPLETE (iw_point . flags) */
#define P2P_PROVISIONING_SUCCESS        0
#define P2P_PROVISIONING_FAIL           1

/* IOC_P2P_START_STOP_DISCOVERY (iw_point . flags) */
#define P2P_STOP_DISCOVERY              0
#define P2P_START_DISCOVERY             1

/* IOC_P2P_CONNECT_DISCONNECT (iw_point . flags) */
#define P2P_CONNECT                     0
#define P2P_DISCONNECT                  1

/* IOC_P2P_START_STOP_DISCOVERY (scan_type) */
#define P2P_SCAN_FULL_AND_FIND          0
#define P2P_SCAN_FULL                   1
#define P2P_SCAN_SEARCH_AND_LISTEN      2
#define P2P_LISTEN                      3

/* IOC_P2P_GET_STRUCT/IOC_P2P_SET_STRUCT */
#define P2P_SEND_SD_RESPONSE            0
#define P2P_GET_SD_REQUEST              1
#define P2P_SEND_SD_REQUEST             2
#define P2P_GET_SD_RESPONSE             3
#define P2P_TERMINATE_SD_PHASE          4

#define CHN_DIRTY_WEIGHT_UPPERBOUND     4

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */
/*------------------------------------------------------------------------*/
/* Wireless Extension: Private I/O Control                                    */
/*------------------------------------------------------------------------*/

#if 0
struct iw_p2p_cfg_device_type {
	void __user *ssid;
	uint8_t ssid_len;
	uint8_t pri_device_type[8];
	uint8_t snd_device_type[8];
	void __user *device_name;
	uint8_t device_name_len;
	uint8_t intend;
	uint8_t persistence;
	uint8_t sec_mode;
	uint8_t ch;
	uint8_t ch_width;	/* 0: 20 Mhz  1:20/40 Mhz auto */
	uint8_t max_scb;
};

struct iw_p2p_hostapd_param {
	uint8_t cmd;
	uint8_t rsv[3];
	uint8_t sta_addr[6];
	void __user *data;
	uint16_t len;
};

struct iw_p2p_req_device_type {
	uint8_t scan_type;	/* 0: Full scan + Find
				 * 1: Full scan
				 * 2: Scan (Search +Listen)
				 * 3: Listen
				 * other : reserved
				 */
	uint8_t pri_device_type[8];
	void __user *probe_req_ie;
	uint16_t probe_req_len;
	void __user *probe_rsp_ie;
	uint16_t probe_rsp_len;
};

struct iw_p2p_connect_device {
	uint8_t sta_addr[6];
	/* 0: P2P Device, 1:GC, 2: GO */
	uint8_t p2pRole;
	/* 0: Don't needed provision, 1: doing the wsc provision first */
	uint8_t needProvision;
	/* 1: auth peer invitation request */
	uint8_t authPeer;
	/* Request Peer Device used config method */
	uint8_t intend_config_method;
};

struct iw_p2p_password_ready {
	uint8_t active_config_method;
	void __user *probe_req_ie;
	uint16_t probe_req_len;
	void __user *probe_rsp_ie;
	uint16_t probe_rsp_len;
};

struct iw_p2p_device_req {
	uint8_t name[33];
	uint32_t name_len;
	uint8_t device_addr[6];
	uint8_t device_type;
	int32_t config_method;
	int32_t active_config_method;
};

struct iw_p2p_transport_struct {
	uint32_t u4CmdId;
	uint32_t inBufferLength;
	uint32_t outBufferLength;
	uint8_t aucBuffer[16];
};

/* For Invitation */
struct iw_p2p_ioctl_invitation_struct {
	uint8_t aucDeviceID[6];
	/* BSSID */
	uint8_t aucGroupID[6];
	uint8_t aucSsid[32];
	uint32_t u4SsidLen;
	uint8_t ucReinvoke;
};

struct iw_p2p_ioctl_abort_invitation {
	uint8_t dev_addr[6];
};

struct iw_p2p_ioctl_invitation_indicate {
	uint8_t dev_addr[6];
	uint8_t group_bssid[6];
	/* peer device supported config method */
	int32_t config_method;
	/* for reinvoke */
	uint8_t dev_name[32];
	uint32_t name_len;
	/* for re-invoke, target operating channel */
	uint8_t operating_channel;
	/* invitation or re-invoke */
	uint8_t invitation_type;
};

struct iw_p2p_ioctl_invitation_status {
	uint32_t status_code;
};

/* For Formation */
struct iw_p2p_ioctl_start_formation {
	/* bssid */
	uint8_t dev_addr[6];
	/* 0: P2P Device, 1:GC, 2: GO */
	uint8_t role;
	/* 0: Don't needed provision, 1: doing the wsc provision first */
	uint8_t needProvision;
	/* 1: auth peer invitation request */
	uint8_t auth;
	/* Request Peer Device used config method */

	uint8_t config_method;
};
#endif
/* SET_STRUCT / GET_STRUCT */
enum ENUM_P2P_CMD_ID {
	P2P_CMD_ID_SEND_SD_RESPONSE = 0,	/* 0x00 (Set) */
	P2P_CMD_ID_GET_SD_REQUEST,	/* 0x01 (Get) */
	P2P_CMD_ID_SEND_SD_REQUEST,	/* 0x02 (Set) */
	P2P_CMD_ID_GET_SD_RESPONSE,	/* 0x03 (Get) */
	P2P_CMD_ID_TERMINATE_SD_PHASE,	/* 0x04 (Set) */
	/* CFG_SUPPORT_ANTI_PIRACY */
	P2P_CMD_ID_SEC_CHECK,	/* 0x05(Set) */
	P2P_CMD_ID_INVITATION,	/* 0x06 (Set) */
	P2P_CMD_ID_INVITATION_INDICATE,	/* 0x07 (Get) */
	P2P_CMD_ID_INVITATION_STATUS,	/* 0x08 (Get) */
	P2P_CMD_ID_INVITATION_ABORT,	/* 0x09 (Set) */
	P2P_CMD_ID_START_FORMATION,	/* 0x0A (Set) */
	P2P_CMD_ID_P2P_VERSION,	/* 0x0B (Set/Get) */
	P2P_CMD_ID_GET_CH_LIST = 12,	/* 0x0C (Get) */
	P2P_CMD_ID_GET_OP_CH = 14	/* 0x0E (Get) */
};

#if 0 /* not used though ... */
/* Service Discovery */
struct iw_p2p_cmd_send_sd_response {
	uint8_t rReceiverAddr[PARAM_MAC_ADDR_LEN];
	uint8_t fgNeedTxDoneIndication;
	uint8_t ucSeqNum;
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[0];	/*native 802.11 */
};

struct iw_p2p_cmd_get_sd_request {
	uint8_t rTransmitterAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[0];	/*native 802.11 */
};

struct iw_p2p_cmd_send_service_discovery_request {
	uint8_t rReceiverAddr[PARAM_MAC_ADDR_LEN];
	uint8_t fgNeedTxDoneIndication;
	uint8_t ucSeqNum;
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[0];	/*native 802.11 */
};

struct iw_p2p_cmd_get_sd_response {
	uint8_t rTransmitterAddr[PARAM_MAC_ADDR_LEN];
	uint16_t u2PacketLength;
	uint8_t aucPacketContent[0];	/*native 802.11 */
};

struct iw_p2p_cmd_terminate_sd_phase {
	uint8_t rPeerAddr[PARAM_MAC_ADDR_LEN];
};

struct iw_p2p_version {
	uint32_t u4Version;
};
#endif

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */
extern struct ieee80211_supported_band mtk_band_2ghz;
extern struct ieee80211_supported_band mtk_band_5ghz;

extern const uint32_t mtk_cipher_suites[6];


/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */
/* Macros used for cfg80211 */
#define RATETAB_ENT(_rate, _rateid, _flags) \
{                                       \
	.bitrate    = (_rate),              \
	.hw_value   = (_rateid),            \
	.flags      = (_flags),             \
}

#define CHAN2G(_channel, _freq, _flags)             \
{                                               \
	.band               = KAL_BAND_2GHZ,  \
	.center_freq        = (_freq),              \
	.hw_value           = (_channel),           \
	.flags              = (_flags),             \
	.max_antenna_gain   = 0,                    \
	.max_power          = 30,                   \
}

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0)
/*
 * TODO: function is os-related, while may depend on the purpose of function
 * to implement on other os
 */
#if 0
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_p2p_cfg80211_add_iface(struct wiphy *wiphy,
		const char *name,
		unsigned char name_assign_type,
		enum nl80211_iftype type,
		u32 *flags,
		struct vif_params *params);
#else
struct wireless_dev *mtk_p2p_cfg80211_add_iface(struct wiphy *wiphy,
		const char *name,
		enum nl80211_iftype type,
		u32 *flags,
		struct vif_params *params);
#endif

int
mtk_p2p_cfg80211_change_iface(struct wiphy *wiphy,
		struct net_device *ndev,
		enum nl80211_iftype type,
		u32 *flags,
		struct vif_params *params);

int mtk_p2p_cfg80211_del_iface(struct wiphy *wiphy,
		struct wireless_dev *wdev);

int
mtk_p2p_cfg80211_add_key(struct wiphy *wiphy,
		struct net_device *ndev,
		u8 key_index,
		bool pairwise,
		const u8 *mac_addr,
		struct key_params *params);

int
mtk_p2p_cfg80211_get_key(struct wiphy *wiphy,
		struct net_device *ndev,
		u8 key_index,
		bool pairwise,
		const u8 *mac_addr,
		void *cookie,
		void (*callback)(void *cookie, struct key_params *));

int
mtk_p2p_cfg80211_del_key(struct wiphy *wiphy,
		struct net_device *ndev,
		u8 key_index,
		bool pairwise,
		const u8 *mac_addr);

int
mtk_p2p_cfg80211_set_default_key(struct wiphy *wiphy,
		struct net_device *netdev,
		u8 key_index,
		bool unicast,
		bool multicast);

int
mtk_p2p_cfg80211_set_mgmt_key(struct wiphy *wiphy,
		struct net_device *dev,
		u8 key_index);

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_get_station(struct wiphy *wiphy,
		struct net_device *ndev,
		const u8 *mac,
		struct station_info *sinfo);
#else
int mtk_p2p_cfg80211_get_station(struct wiphy *wiphy,
		struct net_device *ndev,
		u8 *mac,
		struct station_info *sinfo);
#endif
int mtk_p2p_cfg80211_scan(struct wiphy *wiphy,
		struct cfg80211_scan_request *request);

void mtk_p2p_cfg80211_abort_scan(struct wiphy *wiphy,
		struct wireless_dev *wdev);

int mtk_p2p_cfg80211_set_wiphy_params(struct wiphy *wiphy,
		u32 changed);

int mtk_p2p_cfg80211_connect(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_connect_params *sme);

int mtk_p2p_cfg80211_disconnect(struct wiphy *wiphy,
		struct net_device *dev,
		u16 reason_code);

int mtk_p2p_cfg80211_join_ibss(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_ibss_params *params);

int mtk_p2p_cfg80211_leave_ibss(struct wiphy *wiphy,
		struct net_device *dev);

int mtk_p2p_cfg80211_set_txpower(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		enum nl80211_tx_power_setting type,
		int mbm);

int mtk_p2p_cfg80211_get_txpower(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		int *dbm);

int mtk_p2p_cfg80211_remain_on_channel(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		struct ieee80211_channel *chan,
		unsigned int duration,
		u64 *cookie);

int mtk_p2p_cfg80211_cancel_remain_on_channel(
		struct wiphy *wiphy,
		struct wireless_dev *wdev,
		u64 cookie);

int mtk_p2p_cfg80211_set_power_mgmt(struct wiphy *wiphy,
		struct net_device *dev,
		bool enabled,
		int timeout);

#if (CFG_SUPPORT_DFS_MASTER == 1)

#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_start_radar_detection(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_chan_def *chandef,
		unsigned int cac_time_ms);
#else
int mtk_p2p_cfg80211_start_radar_detection(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_chan_def *chandef);
#endif


#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_channel_switch(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_csa_settings *params);
#endif
#endif

int mtk_p2p_cfg80211_change_bss(struct wiphy *wiphy,
		struct net_device *dev,
		struct bss_parameters *params);

int mtk_p2p_cfg80211_deauth(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_deauth_request *req);

int mtk_p2p_cfg80211_disassoc(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_disassoc_request *req);

int mtk_p2p_cfg80211_start_ap(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_ap_settings *settings);

int mtk_p2p_cfg80211_change_beacon(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_beacon_data *info);

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_mgmt_tx(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		struct cfg80211_mgmt_tx_params *params,
		u64 *cookie);
#else
int mtk_p2p_cfg80211_mgmt_tx(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		struct ieee80211_channel *chan,
		bool offchan,
		unsigned int wait,
		const u8 *buf,
		size_t len,
		bool no_cck,
		bool dont_wait_for_ack,
		u64 *cookie);
#endif

#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy,
		struct net_device *dev,
		struct station_del_parameters *params);
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy,
		struct net_device *dev,
		const u8 *mac);
#else
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy,
		struct net_device *dev,
		u8 *mac);
#endif

int mtk_p2p_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		u64 cookie);

int mtk_p2p_cfg80211_stop_ap(struct wiphy *wiphy,
		struct net_device *dev);

int mtk_p2p_cfg80211_set_channel(struct wiphy *wiphy,
		struct cfg80211_chan_def *chandef);

void mtk_p2p_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
		struct wireless_dev *wdev,
		IN u16 frame_type,
		IN bool reg);

int
mtk_p2p_cfg80211_set_bitrate_mask(IN struct wiphy *wiphy,
		IN struct net_device *dev,
		IN const u8 *peer,
		IN const struct cfg80211_bitrate_mask *mask);

#ifdef CONFIG_NL80211_TESTMODE
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_testmode_cmd(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		void *data,
		int len);
#else
int mtk_p2p_cfg80211_testmode_cmd(struct wiphy *wiphy,
		void *data,
		int len);
#endif
int mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(IN struct wiphy *wiphy,
		IN void *data,
		IN int len);

int mtk_p2p_cfg80211_testmode_p2p_sigma_cmd(IN struct wiphy *wiphy,
		IN void *data,
		IN int len);

#if CFG_SUPPORT_WFD
int mtk_p2p_cfg80211_testmode_wfd_update_cmd(IN struct wiphy *wiphy,
		IN void *data,
				IN int len);
#endif

int mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd(IN struct wiphy *wiphy,
		IN void *data,
		IN int len);

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
int mtk_p2p_cfg80211_testmode_get_best_channel(IN struct wiphy *wiphy,
		IN void *data,
		IN int len);
#endif

int mtk_p2p_cfg80211_testmode_hotspot_config_cmd(IN struct wiphy *wiphy,
		IN void *data,
		IN int len);

#else
/* IGNORE KERNEL DEPENCY ERRORS*/
/*#error "Please ENABLE kernel config (CONFIG_NL80211_TESTMODE)
 * to support Wi-Fi Direct"
 */
#endif
#endif
#endif

/* I/O control handlers */
/*
 * TODO: function is os-related, while may depend on the purpose of function
 * to implement on other os
 */
#if 0
int
mtk_p2p_wext_get_priv(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_reconnect(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_auth(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_key(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_mlme_handler(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_powermode(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_get_powermode(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

/* Private Wireless I/O Controls takes use of iw_handler */
int
mtk_p2p_wext_set_local_dev_info(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_provision_complete(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_start_stop_discovery(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_discovery_results(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_wsc_ie(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_connect_disconnect(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_password_ready(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_request_dev_info(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_invitation_indicate(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_invitation_status(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_pm_param(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_ps_profile(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_network_address(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_int(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

/* Private Wireless I/O Controls for IOC_SET_STRUCT/IOC_GET_STRUCT */
int
mtk_p2p_wext_set_struct(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_get_struct(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

/* IOC_SET_STRUCT/IOC_GET_STRUCT: Service Discovery */
int
mtk_p2p_wext_get_service_discovery_request(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_get_service_discovery_response(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_send_service_discovery_request(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_send_service_discovery_response(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_terminate_service_discovery_phase(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

#if CFG_SUPPORT_ANTI_PIRACY
int
mtk_p2p_wext_set_sec_check_request(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_get_sec_check_response(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);
#endif

int
mtk_p2p_wext_set_noa_param(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_oppps_param(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_set_p2p_version(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

int
mtk_p2p_wext_get_p2p_version(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

void mtk_p2p_wext_set_Multicastlist(IN struct GLUE_INFO *prGlueInfo);

#if CFG_SUPPORT_P2P_RSSI_QUERY
int
mtk_p2p_wext_get_rssi(IN struct net_device *prDev,
		IN struct iw_request_info *info,
		IN OUT union iwreq_data *wrqu,
		IN OUT char *extra);

struct iw_statistics *mtk_p2p_wext_get_wireless_stats(
		struct net_device *prDev);

#endif

int
mtk_p2p_wext_set_txpow(IN struct net_device *prDev,
		IN struct iw_request_info *prIwrInfo,
		IN OUT union iwreq_data *prTxPow,
		IN char *pcExtra);
#endif
#endif /* _GL_P2P_IOCTL_H */
