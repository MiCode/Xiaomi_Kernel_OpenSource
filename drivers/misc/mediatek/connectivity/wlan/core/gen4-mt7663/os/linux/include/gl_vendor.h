/*******************************************************************************
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
 ******************************************************************************/
/*
 * Log: gl_vendor.h
 *
 * 10 14 2014
 * add vendor declaration
 *
 *
 */

#ifndef _GL_VENDOR_H
#define _GL_VENDOR_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include "wlan_lib.h"
#include "gl_wext.h"


/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define GOOGLE_OUI 0x001A11
#define OUI_QCA 0x001374

#define NL80211_VENDOR_SUBCMD_GET_PREFER_FREQ_LIST 103
#define QCA_NL80211_VENDOR_SUBCMD_SETBAND 105

enum ANDROID_VENDOR_SUB_COMMAND {
	/* Don't use 0 as a valid subcommand */
	ANDROID_NL80211_SUBCMD_UNSPECIFIED,

	/* Define all vendor startup commands between 0x0 and 0x0FFF */
	ANDROID_NL80211_SUBCMD_WIFI_RANGE_START = 0x0001,
	ANDROID_NL80211_SUBCMD_WIFI_RANGE_END	= 0x0FFF,

	/* Define all GScan related commands between 0x1000 and 0x10FF */
	ANDROID_NL80211_SUBCMD_GSCAN_RANGE_START = 0x1000,
	ANDROID_NL80211_SUBCMD_GSCAN_RANGE_END	 = 0x10FF,

	/* Define all RTT related commands between 0x1100 and 0x11FF */
	ANDROID_NL80211_SUBCMD_RTT_RANGE_START = 0x1100,
	ANDROID_NL80211_SUBCMD_RTT_RANGE_END   = 0x11FF,

	ANDROID_NL80211_SUBCMD_LSTATS_RANGE_START = 0x1200,
	ANDROID_NL80211_SUBCMD_LSTATS_RANGE_END   = 0x12FF,

	/* Define all Logger related commands between 0x1400 and 0x14FF */
	ANDROID_NL80211_SUBCMD_DEBUG_RANGE_START = 0x1400,
	ANDROID_NL80211_SUBCMD_DEBUG_RANGE_END	 = 0x14FF,

	/* Define all wifi offload related commands between 0x1600 and 0x16FF */
	ANDROID_NL80211_SUBCMD_WIFI_OFFLOAD_RANGE_START = 0x1600,
	ANDROID_NL80211_SUBCMD_WIFI_OFFLOAD_RANGE_END	= 0x16FF,

	/* This is reserved for future usage */

};

enum WIFI_SUB_COMMAND {
	WIFI_SUBCMD_GET_CHANNEL_LIST = ANDROID_NL80211_SUBCMD_WIFI_RANGE_START,

	WIFI_SUBCMD_GET_FEATURE_SET,				/* 0x0002 */
	WIFI_SUBCMD_GET_FEATURE_SET_MATRIX,			/* 0x0003 */
	WIFI_SUBCMD_SET_PNO_RANDOM_MAC_OUI,			/* 0x0004 */
	WIFI_SUBCMD_NODFS_SET,					/* 0x0005 */
	WIFI_SUBCMD_SET_COUNTRY_CODE,				/* 0x0006 */
	WIFI_SUBCMD_SET_RSSI_MONITOR,				/* 0x0007 */

	/* Add more sub commands here */
	WIFI_SUBCMD_GET_ROAMING_CAPABILITIES,			/* 0x0008 */
	WIFI_SUBCMD_SET_ROAMING = 0x0009,			/* 0x0009 */
	WIFI_SUBCMD_CONFIG_ROAMING = 0x000a,			/* 0x000a */
	WIFI_SUBCMD_ENABLE_ROAMING,				/* 0x000b */

	WIFI_SUBCMD_SELECT_TX_POWER_SCENARIO,			/* 0x000c */
};

enum RTT_SUB_COMMAND {
	RTT_SUBCMD_SET_CONFIG = ANDROID_NL80211_SUBCMD_RTT_RANGE_START,
	RTT_SUBCMD_CANCEL_CONFIG,
	RTT_SUBCMD_GETCAPABILITY,
};

enum LSTATS_SUB_COMMAND {
	LSTATS_SUBCMD_GET_INFO = ANDROID_NL80211_SUBCMD_LSTATS_RANGE_START,
};

/* moved from wifi_logger.cpp */
enum DEBUG_SUB_COMMAND {
	LOGGER_START_LOGGING = ANDROID_NL80211_SUBCMD_DEBUG_RANGE_START,
	LOGGER_GET_VER
};

enum WIFI_OFFLOAD_SUB_COMMAND {
	WIFI_OFFLOAD_START_MKEEP_ALIVE =
		ANDROID_NL80211_SUBCMD_WIFI_OFFLOAD_RANGE_START,
	WIFI_OFFLOAD_STOP_MKEEP_ALIVE,
};

enum WIFI_VENDOR_EVENT {
	GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS,
	GSCAN_EVENT_HOTLIST_RESULTS_FOUND,
	GSCAN_EVENT_SCAN_RESULTS_AVAILABLE,
	GSCAN_EVENT_FULL_SCAN_RESULTS,
	RTT_EVENT_COMPLETE,
	GSCAN_EVENT_COMPLETE_SCAN,
	GSCAN_EVENT_HOTLIST_RESULTS_LOST,
	WIFI_EVENT_RSSI_MONITOR,
	WIFI_EVENT_MAGIC_PACKET_RECEIVED
};

enum WIFI_ATTRIBUTE {
	WIFI_ATTRIBUTE_BAND = 1,
	WIFI_ATTRIBUTE_NUM_CHANNELS,
	WIFI_ATTRIBUTE_CHANNEL_LIST,

	WIFI_ATTRIBUTE_NUM_FEATURE_SET,
	WIFI_ATTRIBUTE_FEATURE_SET,
	WIFI_ATTRIBUTE_PNO_RANDOM_MAC_OUI,
	WIFI_ATTRIBUTE_NODFS_VALUE,
	WIFI_ATTRIBUTE_COUNTRY_CODE,

	WIFI_ATTRIBUTE_MAX_RSSI,
	WIFI_ATTRIBUTE_MIN_RSSI,
	WIFI_ATTRIBUTE_RSSI_MONITOR_START,

	WIFI_ATTRIBUTE_ROAMING_CAPABILITIES,
	WIFI_ATTRIBUTE_ROAMING_BLACKLIST_NUM,
	WIFI_ATTRIBUTE_ROAMING_BLACKLIST_BSSID,
	WIFI_ATTRIBUTE_ROAMING_WHITELIST_NUM,
	WIFI_ATTRIBUTE_ROAMING_WHITELIST_SSID,
	WIFI_ATTRIBUTE_ROAMING_STATE,

	WIFI_ATTRIBUTE_TX_POWER_SCENARIO,
};

/* moved from wifi_logger.cpp */
enum LOGGER_ATTRIBUTE {
	LOGGER_ATTRIBUTE_DRIVER_VER,
	LOGGER_ATTRIBUTE_FW_VER
};

enum RTT_ATTRIBUTE {
	RTT_ATTRIBUTE_CAPABILITIES = 1,

	RTT_ATTRIBUTE_TARGET_CNT = 10,
	RTT_ATTRIBUTE_TARGET_INFO,
	RTT_ATTRIBUTE_TARGET_MAC,
	RTT_ATTRIBUTE_TARGET_TYPE,
	RTT_ATTRIBUTE_TARGET_PEER,
	RTT_ATTRIBUTE_TARGET_CHAN,
	RTT_ATTRIBUTE_TARGET_PERIOD,
	RTT_ATTRIBUTE_TARGET_NUM_BURST,
	RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST,
	RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM,
	RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR,
	RTT_ATTRIBUTE_TARGET_LCI,
	RTT_ATTRIBUTE_TARGET_LCR,
	RTT_ATTRIBUTE_TARGET_BURST_DURATION,
	RTT_ATTRIBUTE_TARGET_PREAMBLE,
	RTT_ATTRIBUTE_TARGET_BW,
	RTT_ATTRIBUTE_RESULTS_COMPLETE = 30,
	RTT_ATTRIBUTE_RESULTS_PER_TARGET,
	RTT_ATTRIBUTE_RESULT_CNT,
	RTT_ATTRIBUTE_RESULT
};

enum LSTATS_ATTRIBUTE {
	LSTATS_ATTRIBUTE_STATS = 2,
};

enum WIFI_MKEEP_ALIVE_ATTRIBUTE {
	MKEEP_ALIVE_ATTRIBUTE_ID = 1,
	MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN,
	MKEEP_ALIVE_ATTRIBUTE_IP_PKT,
	MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR,
	MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR,
	MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC
};

enum WIFI_SCAN_EVENT {
	WIFI_SCAN_BUFFER_FULL,
	WIFI_SCAN_COMPLETE,
};

/* QCA Vender CMD */
enum QCA_SET_BAND {
	QCA_SETBAND_AUTO,
	QCA_SETBAND_5G,
	QCA_SETBAND_2G,
};

enum WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST {
	WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST_INVALID,
	WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST_IFACE_TYPE,
	WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST_GET,
	WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST_LAST,
	WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST_MAX =
		WIFI_VENDOR_ATTR_PREFERRED_FREQ_LIST_LAST - 1
};

#define MAX_FW_ROAMING_BLACKLIST_SIZE	16
#define MAX_FW_ROAMING_WHITELIST_SIZE	8

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
#if CFG_SUPPORT_WAPI
extern uint8_t
keyStructBuf[1024];	/* add/remove key shared buffer */
#else
extern uint8_t
keyStructBuf[100];	/* add/remove key shared buffer */
#endif

/*******************************************************************************
 *                           MACROS
 *******************************************************************************
 */

#if KERNEL_VERSION(3, 5, 0) <= LINUX_VERSION_CODE
/*
 * #define NLA_PUT(skb, attrtype, attrlen, data) \
 *	do { \
 *		if (unlikely(nla_put(skb, attrtype, attrlen, data) < 0)) \
 *			goto nla_put_failure; \
 *	} while (0)
 *
 *#define NLA_PUT_TYPE(skb, type, attrtype, value) \
 *	do { \
 *		type __tmp = value; \
 *		NLA_PUT(skb, attrtype, sizeof(type), &__tmp); \
 *	} while (0)
 */
#define NLA_PUT(skb, attrtype, attrlen, data) \
	mtk_cfg80211_NLA_PUT(skb, attrtype, attrlen, data)

#define NLA_PUT_TYPE(skb, type, attrtype, value) \
	mtk_cfg80211_nla_put_type(skb, type, attrtype, value)

#define NLA_PUT_U8(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U8, attrtype, value)

#define NLA_PUT_U16(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U16, attrtype, value)

#define NLA_PUT_U32(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U32, attrtype, value)

#define NLA_PUT_U64(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U64, attrtype, value)

#endif

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
#define NLA_PARSE_NESTED(nlattr, maxtype, nla, policy)  \
	nla_parse_nested(nlattr, maxtype, nla, policy, NULL)
#define NLA_PARSE(tb, maxtype, head, len, policy) \
	nla_parse(tb, maxtype, head, len, policy, NULL)
#else
#define NLA_PARSE_NESTED(nlattr, maxtype, nla, policy)  \
	nla_parse_nested(nlattr, maxtype, nla, policy)
#define NLA_PARSE(tb, maxtype, head, len, policy) \
	nla_parse(tb, maxtype, head, len, policy)
#endif

/*******************************************************************************
 *				P R I V A T E   D A T A
 *
 *******************************************************************************
 */
struct PARAM_WIFI_CHANGE_RESULT {
	uint16_t flags;
	uint16_t channel;
	uint8_t bssid[6];	/* BSSID */
	int8_t rssi[8];	/* RSSI history in db */
};

struct PARAM_AP_THRESHOLD {
	uint8_t bssid[6];	/* AP BSSID */
	int32_t low;	/* low threshold */
	int32_t high;	/* high threshold */
	uint32_t channel;	/* channel hint */
};

/* channel operating width */
enum WIFI_CHANNEL_WIDTH {
	WIFI_CHAN_WIDTH_20 = 0,
	WIFI_CHAN_WIDTH_40 = 1,
	WIFI_CHAN_WIDTH_80 = 2,
	WIFI_CHAN_WIDTH_160 = 3,
	WIFI_CHAN_WIDTH_80P80 = 4,
	WIFI_CHAN_WIDTH_5 = 5,
	WIFI_CHAN_WIDTH_10 = 6,
	WIFI_CHAN_WIDTH_INVALID = -1
};

/* channel information */
struct WIFI_CHANNEL_INFO {
	enum WIFI_CHANNEL_WIDTH width;
	uint32_t center_freq;
	uint32_t center_freq0;
	uint32_t center_freq1;
};

/* channel statistics */
struct WIFI_CHANNEL_STAT {
	struct WIFI_CHANNEL_INFO channel;
	uint32_t on_time;
	uint32_t cca_busy_time;
};

/* radio statistics */
struct WIFI_RADIO_STAT {
	uint32_t radio;
	uint32_t on_time;
	uint32_t tx_time;
	uint32_t rx_time;
	uint32_t on_time_scan;
	uint32_t on_time_nbd;
	uint32_t on_time_gscan;
	uint32_t on_time_roam_scan;
	uint32_t on_time_pno_scan;
	uint32_t on_time_hs20;
	uint32_t num_channels;
	struct WIFI_CHANNEL_STAT channels[];
};

/* wifi rate */
struct WIFI_RATE {
	uint32_t preamble: 3;
	uint32_t nss: 2;
	uint32_t bw: 3;
	uint32_t rateMcsIdx: 8;

	uint32_t reserved: 16;
	uint32_t bitrate;
};

/* per rate statistics */
struct WIFI_RATE_STAT {
	struct WIFI_RATE rate;
	uint32_t tx_mpdu;
	uint32_t rx_mpdu;
	uint32_t mpdu_lost;
	uint32_t retries;
	uint32_t retries_short;
	uint32_t retries_long;
};

/*wifi_interface_link_layer_info*/
enum WIFI_CONNECTION_STATE {
	WIFI_DISCONNECTED = 0,
	WIFI_AUTHENTICATING = 1,
	WIFI_ASSOCIATING = 2,
	WIFI_ASSOCIATED = 3,
	WIFI_EAPOL_STARTED = 4,
	WIFI_EAPOL_COMPLETED = 5,
};

enum WIFI_ROAM_STATE {
	WIFI_ROAMING_IDLE = 0,
	WIFI_ROAMING_ACTIVE = 1,
};

enum WIFI_INTERFACE_MODE {
	WIFI_INTERFACE_STA = 0,
	WIFI_INTERFACE_SOFTAP = 1,
	WIFI_INTERFACE_IBSS = 2,
	WIFI_INTERFACE_P2P_CLIENT = 3,
	WIFI_INTERFACE_P2P_GO = 4,
	WIFI_INTERFACE_NAN = 5,
	WIFI_INTERFACE_MESH = 6,
	WIFI_INTERFACE_UNKNOWN = -1
};

struct WIFI_INTERFACE_LINK_LAYER_INFO {
	enum WIFI_INTERFACE_MODE mode;
	u8 mac_addr[6];
	enum WIFI_CONNECTION_STATE state;
	enum WIFI_ROAM_STATE roaming;
	u32 capabilities;
	u8 ssid[33];
	u8 bssid[6];
	u8 ap_country_str[3];
	u8 country_str[3];
};

/* access categories */
enum WIFI_TRAFFIC_AC {
	WIFI_AC_VO = 0,
	WIFI_AC_VI = 1,
	WIFI_AC_BE = 2,
	WIFI_AC_BK = 3,
	WIFI_AC_MAX = 4,
};

/* wifi peer type */
enum WIFI_PEER_TYPE {
	WIFI_PEER_STA,
	WIFI_PEER_AP,
	WIFI_PEER_P2P_GO,
	WIFI_PEER_P2P_CLIENT,
	WIFI_PEER_NAN,
	WIFI_PEER_TDLS,
	WIFI_PEER_INVALID,
};

/* per peer statistics */
struct WIFI_PEER_INFO {
	enum WIFI_PEER_TYPE type;
	uint8_t peer_mac_address[6];
	uint32_t capabilities;
	uint32_t num_rate;
	struct WIFI_RATE_STAT rate_stats[];
};

/* per access category statistics */
struct WIFI_WMM_AC_STAT_ {
	enum WIFI_TRAFFIC_AC ac;
	uint32_t tx_mpdu;
	uint32_t rx_mpdu;
	uint32_t tx_mcast;

	uint32_t rx_mcast;
	uint32_t rx_ampdu;
	uint32_t tx_ampdu;
	uint32_t mpdu_lost;
	uint32_t retries;
	uint32_t retries_short;
	uint32_t retries_long;
	uint32_t contention_time_min;
	uint32_t contention_time_max;
	uint32_t contention_time_avg;
	uint32_t contention_num_samples;
};

/* RTT Capabilities */
struct PARAM_WIFI_RTT_CAPABILITIES {
	/* if 1-sided rtt data collection is supported */
	uint8_t rtt_one_sided_supported;
	/* if ftm rtt data collection is supported */
	uint8_t rtt_ftm_supported;
	/* if initiator supports LCI request. Applies to 2-sided RTT */
	uint8_t lci_support;
	/* if initiator supports LCR request. Applies to 2-sided RTT */
	uint8_t lcr_support;
	/* bit mask indicates what preamble is supported by initiator */
	uint8_t preamble_support;
	/* bit mask indicates what BW is supported by initiator */
	uint8_t bw_support;
};

/* interface statistics */
struct WIFI_IFACE_STAT {
	struct WIFI_INTERFACE_LINK_LAYER_INFO info;
	uint32_t beacon_rx;
	uint32_t mgmt_rx;
	uint32_t mgmt_action_rx;
	uint32_t mgmt_action_tx;
	int32_t rssi_mgmt;
	int32_t rssi_data;
	int32_t rssi_ack;
	struct WIFI_WMM_AC_STAT_ ac[WIFI_AC_MAX];
	uint32_t num_peers;
	struct WIFI_PEER_INFO peer_info[];
};

enum ENUM_NLA_PUT_DATE_TYPE {
	NLA_PUT_DATE_U8 = 0,
	NLA_PUT_DATE_U16,
	NLA_PUT_DATE_U32,
	NLA_PUT_DATE_U64,
};

/* RSSI Monitoring */
struct PARAM_RSSI_MONITOR_T {
	bool enable;	/* 1=Start, 0=Stop*/
	int8_t max_rssi_value;
	int8_t min_rssi_value;
	uint8_t reserved[1];
	uint8_t reserved2[4]; /* reserved for MT6632 */
};

struct PARAM_RSSI_MONITOR_EVENT {
	uint8_t version;
	int8_t rssi;
	uint8_t BSSID[PARAM_MAC_ADDR_LEN];
};

/* Packet Keep Alive */
struct PARAM_PACKET_KEEPALIVE_T {
	bool enable;	/* 1=Start, 0=Stop*/
	uint8_t index;
	int16_t u2IpPktLen;
	uint8_t pIpPkt[256];
	uint8_t ucSrcMacAddr[PARAM_MAC_ADDR_LEN];
	uint8_t ucDstMacAddr[PARAM_MAC_ADDR_LEN];
	uint32_t u4PeriodMsec;
	uint8_t reserved[8]; /* reserved for MT6632 */
};

struct PARAM_BSS_MAC_OUI {
	uint8_t ucBssIndex;
	uint8_t ucMacOui[MAC_OUI_LEN];
};
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */



/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
int mtk_cfg80211_NLA_PUT(struct sk_buff *skb, int attrtype,
			 int attrlen, const void *data);

int mtk_cfg80211_nla_put_type(struct sk_buff *skb,
			      enum ENUM_NLA_PUT_DATE_TYPE type, int attrtype,
			      const void *value);

int mtk_cfg80211_vendor_get_capabilities(struct wiphy
		*wiphy, struct wireless_dev *wdev,
		const void *data, int data_len);

int mtk_cfg80211_vendor_set_config(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len);

int mtk_cfg80211_vendor_set_scan_config(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_set_significant_change(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);

int mtk_cfg80211_vendor_set_hotlist(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len);

int mtk_cfg80211_vendor_enable_scan(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len);

int mtk_cfg80211_vendor_enable_full_scan_results(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);

int mtk_cfg80211_vendor_get_scan_results(struct wiphy
		*wiphy, struct wireless_dev *wdev,
		const void *data, int data_len);

int mtk_cfg80211_vendor_get_channel_list(struct wiphy
		*wiphy, struct wireless_dev *wdev,
		const void *data, int data_len);

int mtk_cfg80211_vendor_set_country_code(struct wiphy
		*wiphy, struct wireless_dev *wdev,
		const void *data, int data_len);

int mtk_cfg80211_vendor_get_rtt_capabilities(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);

int mtk_cfg80211_vendor_llstats_get_info(struct wiphy
		*wiphy, struct wireless_dev *wdev,
		const void *data, int data_len);

int mtk_cfg80211_vendor_set_band(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);

int mtk_cfg80211_vendor_set_scan_mac_oui(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);

int mtk_cfg80211_vendor_set_roaming_policy(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);

int mtk_cfg80211_vendor_get_roaming_capabilities(
	struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);

int mtk_cfg80211_vendor_config_roaming(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len);

int mtk_cfg80211_vendor_enable_roaming(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len);

int mtk_cfg80211_vendor_set_rssi_monitoring(
	struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);

int mtk_cfg80211_vendor_packet_keep_alive_start(
	struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);

int mtk_cfg80211_vendor_packet_keep_alive_stop(
	struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);

int mtk_cfg80211_vendor_get_version(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len);

int mtk_cfg80211_vendor_event_rssi_beyond_range(
	struct wiphy *wiphy,
	struct wireless_dev *wdev, int rssi);

int mtk_cfg80211_vendor_get_supported_feature_set(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);

int mtk_cfg80211_vendor_set_tx_power_scenario(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);

#if CFG_SUPPORT_MAGIC_PKT_VENDOR_EVENT
int mtk_cfg80211_vendor_event_wowlan_magic_pkt(
	struct wiphy *wiphy,
	struct wireless_dev *wdev, uint32_t num);
#endif

int mtk_cfg80211_vendor_get_preferred_freq_list(struct wiphy
		*wiphy, struct wireless_dev *wdev, const void *data,
		int data_len);

#endif /* _GL_VENDOR_H */
