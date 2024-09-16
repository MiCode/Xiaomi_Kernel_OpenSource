/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _GL_P2P_IOCTL_H
#define _GL_P2P_IOCTL_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#endif

#include "wlan_oid.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* Device private ioctl calls */
/* #define SIOCDEVPRIVATE  0x89F0*/
#define IOC_GET_PRIVATE_IOCTL_CMD	(SIOCDEVPRIVATE+1)

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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Wireless Extension: Private I/O Control                                    */
/*----------------------------------------------------------------------------*/
typedef struct iw_p2p_cfg_device_type {
	void __user *ssid;
	UINT_8 ssid_len;
	UINT_8 pri_device_type[8];
	UINT_8 snd_device_type[8];
	void __user *device_name;
	UINT_8 device_name_len;
	UINT_8 intend;
	UINT_8 persistence;
	UINT_8 sec_mode;
	UINT_8 ch;
	UINT_8 ch_width;	/* 0: 20 Mhz  1:20/40 Mhz auto */
	UINT_8 max_scb;
} IW_P2P_CFG_DEVICE_TYPE, *P_IW_P2P_CFG_DEVICE_TYPE;

typedef struct iw_p2p_hostapd_param {
	UINT_8 cmd;
	UINT_8 rsv[3];
	UINT_8 sta_addr[6];
	void __user *data;
	UINT_16 len;
} IW_P2P_HOSTAPD_PARAM, *P_IW_P2P_HOSTAPD_PARAM;

typedef struct iw_p2p_req_device_type {
	UINT_8 scan_type;	/* 0: Full scan + Find
				 * 1: Full scan
				 * 2: Scan (Search +Listen)
				 * 3: Listen
				 * other : reserved
				 */
	UINT_8 pri_device_type[8];
	void __user *probe_req_ie;
	UINT_16 probe_req_len;
	void __user *probe_rsp_ie;
	UINT_16 probe_rsp_len;
} IW_P2P_REQ_DEVICE_TYPE, *P_IW_P2P_REQ_DEVICE_TYPE;

typedef struct iw_p2p_connect_device {
	UINT_8 sta_addr[6];
	UINT_8 p2pRole;		/* 0: P2P Device, 1:GC, 2: GO */
	UINT_8 needProvision;	/* 0: Don't needed provision, 1: doing the wsc provision first */
	UINT_8 authPeer;	/* 1: auth peer invitation request */
	UINT_8 intend_config_method;	/* Request Peer Device used config method */
} IW_P2P_CONNECT_DEVICE, *P_IW_P2P_CONNECT_DEVICE;

typedef struct iw_p2p_password_ready {
	UINT_8 active_config_method;
	void __user *probe_req_ie;
	UINT_16 probe_req_len;
	void __user *probe_rsp_ie;
	UINT_16 probe_rsp_len;
} IW_P2P_PASSWORD_READY, *P_IW_P2P_PASSWORD_READY;

typedef struct iw_p2p_device_req {
	UINT_8 name[33];
	UINT_32 name_len;
	UINT_8 device_addr[6];
	UINT_8 device_type;
	INT_32 config_method;
	INT_32 active_config_method;
} IW_P2P_DEVICE_REQ, *P_IW_P2P_DEVICE_REQ;

typedef struct iw_p2p_transport_struct {
	UINT_32 u4CmdId;
	UINT_32 inBufferLength;
	UINT_32 outBufferLength;
	UINT_8 aucBuffer[16];
} IW_P2P_TRANSPORT_STRUCT, *P_IW_P2P_TRANSPORT_STRUCT;

/* For Invitation */
typedef struct iw_p2p_ioctl_invitation_struct {
	UINT_8 aucDeviceID[6];
	UINT_8 aucGroupID[6];	/* BSSID */
	UINT_8 aucSsid[32];
	UINT_32 u4SsidLen;
	UINT_8 ucReinvoke;
} IW_P2P_IOCTL_INVITATION_STRUCT, *P_IW_P2P_IOCTL_INVITATION_STRUCT;

typedef struct iw_p2p_ioctl_abort_invitation {
	UINT_8 dev_addr[6];
} IW_P2P_IOCTL_ABORT_INVITATION, *P_IW_P2P_IOCTL_ABORT_INVITATION;

typedef struct iw_p2p_ioctl_invitation_indicate {
	UINT_8 dev_addr[6];
	UINT_8 group_bssid[6];
	INT_32 config_method;	/* peer device supported config method */
	UINT_8 dev_name[32];	/* for reinvoke */
	UINT_32 name_len;
	UINT_8 operating_channel;	/* for re-invoke, target operating channel */
	UINT_8 invitation_type;	/* invitation or re-invoke */
} IW_P2P_IOCTL_INVITATION_INDICATE, *P_IW_P2P_IOCTL_INVITATION_INDICATE;

typedef struct iw_p2p_ioctl_invitation_status {
	UINT_32 status_code;
} IW_P2P_IOCTL_INVITATION_STATUS, *P_IW_P2P_IOCTL_INVITATION_STATUS;

/* For Formation */
typedef struct iw_p2p_ioctl_start_formation {
	UINT_8 dev_addr[6];	/* bssid */
	UINT_8 role;		/* 0: P2P Device, 1:GC, 2: GO */
	UINT_8 needProvision;	/* 0: Don't needed provision, 1: doing the wsc provision first */
	UINT_8 auth;		/* 1: auth peer invitation request */
	UINT_8 config_method;	/* Request Peer Device used config method */
} IW_P2P_IOCTL_START_FORMATION, *P_IW_P2P_IOCTL_START_FORMATION;

/* SET_STRUCT / GET_STRUCT */
typedef enum _ENUM_P2P_CMD_ID_T {
	P2P_CMD_ID_SEND_SD_RESPONSE = 0,	/* 0x00 (Set) */
	P2P_CMD_ID_GET_SD_REQUEST,	/* 0x01 (Get) */
	P2P_CMD_ID_SEND_SD_REQUEST,	/* 0x02 (Set) */
	P2P_CMD_ID_GET_SD_RESPONSE,	/* 0x03 (Get) */
	P2P_CMD_ID_TERMINATE_SD_PHASE,	/* 0x04 (Set) */
#if 1				/* CFG_SUPPORT_ANTI_PIRACY */
	P2P_CMD_ID_SEC_CHECK,	/* 0x05(Set) */
#endif
	P2P_CMD_ID_INVITATION,	/* 0x06 (Set) */
	P2P_CMD_ID_INVITATION_INDICATE,	/* 0x07 (Get) */
	P2P_CMD_ID_INVITATION_STATUS,	/* 0x08 (Get) */
	P2P_CMD_ID_INVITATION_ABORT,	/* 0x09 (Set) */
	P2P_CMD_ID_START_FORMATION,	/* 0x0A (Set) */
	P2P_CMD_ID_P2P_VERSION,	/* 0x0B (Set/Get) */
	P2P_CMD_ID_GET_CH_LIST = 12,	/* 0x0C (Get) */
	P2P_CMD_ID_GET_OP_CH = 14	/* 0x0E (Get) */
} ENUM_P2P_CMD_ID_T, *P_ENUM_P2P_CMD_ID_T;

/* Service Discovery */
typedef struct iw_p2p_cmd_send_sd_response {
	PARAM_MAC_ADDRESS rReceiverAddr;
	UINT_8 fgNeedTxDoneIndication;
	UINT_8 ucSeqNum;
	UINT_16 u2PacketLength;
	UINT_8 aucPacketContent[0];	/*native 802.11 */
} IW_P2P_CMD_SEND_SD_RESPONSE, *P_IW_P2P_CMD_SEND_SD_RESPONSE;

typedef struct iw_p2p_cmd_get_sd_request {
	PARAM_MAC_ADDRESS rTransmitterAddr;
	UINT_16 u2PacketLength;
	UINT_8 aucPacketContent[0];	/*native 802.11 */
} IW_P2P_CMD_GET_SD_REQUEST, *P_IW_P2P_CMD_GET_SD_REQUEST;

typedef struct iw_p2p_cmd_send_service_discovery_request {
	PARAM_MAC_ADDRESS rReceiverAddr;
	UINT_8 fgNeedTxDoneIndication;
	UINT_8 ucSeqNum;
	UINT_16 u2PacketLength;
	UINT_8 aucPacketContent[0];	/*native 802.11 */
} IW_P2P_CMD_SEND_SD_REQUEST, *P_IW_P2P_CMD_SEND_SD_REQUEST;

typedef struct iw_p2p_cmd_get_sd_response {
	PARAM_MAC_ADDRESS rTransmitterAddr;
	UINT_16 u2PacketLength;
	UINT_8 aucPacketContent[0];	/*native 802.11 */
} IW_P2P_CMD_GET_SD_RESPONSE, *P_IW_P2P_CMD_GET_SD_RESPONSE;

typedef struct iw_p2p_cmd_terminate_sd_phase {
	PARAM_MAC_ADDRESS rPeerAddr;
} IW_P2P_CMD_TERMINATE_SD_PHASE, *P_IW_P2P_CMD_TERMINATE_SD_PHASE;

typedef struct iw_p2p_version {
	UINT_32 u4Version;
} IW_P2P_VERSION, *P_IW_P2P_VERSION;

typedef enum _ENUM_TESTMODE_AVAILABLE_CHAN_ATTR {
	__NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_INVALID,
	NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_2G_BASE_1,
	NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_36,
	NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_52,
	NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_100,
	NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_149,
	__NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_AFTER_LAST,
	NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_MAX = __NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_AFTER_LAST - 1
} ENUM_TESTMODE_AVAILABLE_CHAN_ATTR;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
extern struct ieee80211_supported_band mtk_band_2ghz;
extern struct ieee80211_supported_band mtk_band_5ghz;
extern const UINT_32 mtk_cipher_suites[6];
#endif

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_change_iface(struct wiphy *wiphy,
				  struct net_device *ndev,
				  enum nl80211_iftype type, struct vif_params *params);
#else
int mtk_p2p_cfg80211_change_iface(struct wiphy *wiphy,
				  struct net_device *ndev,
				  enum nl80211_iftype type, u32 *flags, struct vif_params *params);
#endif
int mtk_p2p_cfg80211_add_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index, bool pairwise, const u8 *mac_addr, struct key_params *params);

int mtk_p2p_cfg80211_get_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index,
			     bool pairwise,
			     const u8 *mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *)
);
int mtk_cfg80211_add_station(struct wiphy *wiphy, struct net_device *ndev,
				const u8 *mac, struct station_parameters *params);

int mtk_cfg80211_change_station(struct wiphy *wiphy, struct net_device *ndev,
				const u8 *mac, struct station_parameters *params);

int mtk_p2p_cfg80211_del_key(struct wiphy *wiphy,
			     struct net_device *ndev, u8 key_index, bool pairwise, const u8 *mac_addr);

int
mtk_p2p_cfg80211_set_default_key(struct wiphy *wiphy,
				 struct net_device *netdev, u8 key_index, bool unicast, bool multicast);

int mtk_p2p_cfg80211_set_mgmt_key(struct wiphy *wiphy, struct net_device *dev, u8 key_index);

int mtk_p2p_cfg80211_get_station(struct wiphy *wiphy, struct net_device *ndev,
					const u8 *mac, struct station_info *sinfo);

int mtk_p2p_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed);

int mtk_p2p_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_connect_params *sme);

int mtk_p2p_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev, u16 reason_code);

int mtk_p2p_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ibss_params *params);

int mtk_p2p_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *dev);

int mtk_p2p_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev, bool enabled, int timeout);

int mtk_p2p_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params);

int mtk_p2p_cfg80211_remain_on_channel(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       struct ieee80211_channel *chan, unsigned int duration, u64 *cookie);

int mtk_p2p_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request);

void mtk_p2p_cfg80211_abort_scan(struct wiphy *wiphy, struct wireless_dev *wdev);

int mtk_p2p_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie);

int mtk_p2p_cfg80211_set_txpower(struct wiphy *wiphy,
				 struct wireless_dev *wdev, enum nl80211_tx_power_setting type, int mbm);

int mtk_p2p_cfg80211_get_txpower(struct wiphy *wiphy, struct wireless_dev *wdev, int *dbm);

int mtk_p2p_cfg80211_deauth(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_deauth_request *req);

int mtk_p2p_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_disassoc_request *req);

int mtk_p2p_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *settings);

int mtk_p2p_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_beacon_data *info);

int mtk_p2p_cfg80211_mgmt_tx(struct wiphy *wiphy,
			     struct wireless_dev *wdev,
			     struct cfg80211_mgmt_tx_params *params,
			     u64 *cookie);

int mtk_p2p_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, struct station_del_parameters *params);
#else
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, const u8 *mac);
#endif

int mtk_p2p_cfg80211_set_channel(struct wiphy *wiphy, struct cfg80211_chan_def *chandef);

void mtk_p2p_cfg80211_mgmt_frame_register(struct wiphy *wiphy, struct wireless_dev *wdev, u16 frame_type, bool reg);

int
mtk_p2p_cfg80211_set_bitrate_mask(IN struct wiphy *wiphy,
				  IN struct net_device *dev,
				  IN const u8 *peer, IN const struct cfg80211_bitrate_mask *mask);

#if CONFIG_NL80211_TESTMODE

int mtk_p2p_cfg80211_testmode_cmd(IN struct wiphy *wiphy, IN struct wireless_dev *wdev, IN void *data, IN int len);

int mtk_cfg80211_process_str_cmd(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 cmd, IN INT_32 len);

int mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(IN struct wiphy *wiphy, IN void *data, IN int len);

int mtk_p2p_cfg80211_testmode_p2p_sigma_cmd(IN struct wiphy *wiphy, IN void *data, IN int len);

#if CFG_SUPPORT_WFD
int mtk_p2p_cfg80211_testmode_wfd_update_cmd(IN struct wiphy *wiphy, IN void *data, IN int len);
#endif

int mtk_p2p_cfg80211_testmode_hotspot_block_cmd(IN struct wiphy *wiphy, IN void *data, IN int len);

int mtk_p2p_cfg80211_testmode_hotspot_config_cmd(IN struct wiphy *wiphy, IN void *data, IN int len);

#else
#error "Please ENABLE kernel config (CONFIG_NL80211_TESTMODE)"
#endif

#endif

/* I/O control handlers */

int
mtk_p2p_wext_get_priv(IN struct net_device *prDev,
		      IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_reconnect(IN struct net_device *prDev,
		       IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_auth(IN struct net_device *prDev,
		      IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_key(IN struct net_device *prDev,
		     IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_mlme_handler(IN struct net_device *prDev,
			  IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_powermode(IN struct net_device *prDev,
			   IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_get_powermode(IN struct net_device *prDev,
			   IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

/* Private Wireless I/O Controls takes use of iw_handler */
int
mtk_p2p_wext_set_local_dev_info(IN struct net_device *prDev,
				IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_provision_complete(IN struct net_device *prDev,
				    IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_start_stop_discovery(IN struct net_device *prDev,
				  IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_discovery_results(IN struct net_device *prDev,
			       IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_wsc_ie(IN struct net_device *prDev,
		    IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_connect_disconnect(IN struct net_device *prDev,
				IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_password_ready(IN struct net_device *prDev,
			    IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_request_dev_info(IN struct net_device *prDev,
			      IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_invitation_indicate(IN struct net_device *prDev,
				 IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_invitation_status(IN struct net_device *prDev,
			       IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_pm_param(IN struct net_device *prDev,
			  IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_ps_profile(IN struct net_device *prDev,
			    IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_network_address(IN struct net_device *prDev,
				 IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_int(IN struct net_device *prDev,
		     IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

/* Private Wireless I/O Controls for IOC_SET_STRUCT/IOC_GET_STRUCT */
int
mtk_p2p_wext_set_struct(IN struct net_device *prDev,
			IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_get_struct(IN struct net_device *prDev,
			IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

/* IOC_SET_STRUCT/IOC_GET_STRUCT: Service Discovery */
int
mtk_p2p_wext_get_service_discovery_request(IN struct net_device *prDev,
					   IN struct iw_request_info *info,
					   IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_get_service_discovery_response(IN struct net_device *prDev,
					    IN struct iw_request_info *info,
					    IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_send_service_discovery_request(IN struct net_device *prDev,
					    IN struct iw_request_info *info,
					    IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_send_service_discovery_response(IN struct net_device *prDev,
					     IN struct iw_request_info *info,
					     IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_terminate_service_discovery_phase(IN struct net_device *prDev,
					       IN struct iw_request_info *info,
					       IN OUT union iwreq_data *wrqu, IN OUT char *extra);

#if CFG_SUPPORT_ANTI_PIRACY
int
mtk_p2p_wext_set_sec_check_request(IN struct net_device *prDev,
				   IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_get_sec_check_response(IN struct net_device *prDev,
				    IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);
#endif

int
mtk_p2p_wext_set_noa_param(IN struct net_device *prDev,
			   IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_oppps_param(IN struct net_device *prDev,
			     IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_set_p2p_version(IN struct net_device *prDev,
			     IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

int
mtk_p2p_wext_get_p2p_version(IN struct net_device *prDev,
			     IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

void mtk_p2p_wext_set_Multicastlist(IN P_GLUE_INFO_T prGlueInfo);

#if CFG_SUPPORT_P2P_RSSI_QUERY
int
mtk_p2p_wext_get_rssi(IN struct net_device *prDev,
		      IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra);

struct iw_statistics *mtk_p2p_wext_get_wireless_stats(struct net_device *prDev);

#endif

int
mtk_p2p_wext_set_txpow(IN struct net_device *prDev,
		       IN struct iw_request_info *prIwrInfo, IN OUT union iwreq_data *prTxPow, IN char *pcExtra);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _GL_P2P_IOCTL_H */
