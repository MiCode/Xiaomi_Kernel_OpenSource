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
** Log: gl_vendor.h
**
** 10 14 2014
** add vendor declaration
**
 *
*/

#ifndef _GL_VENDOR_H
#define _GL_VENDOR_H

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
#include <linux/ieee80211.h>
#include <net/cfg80211.h>

#include "gl_os.h"

#include "wlan_lib.h"
#include "gl_wext.h"
#include <linux/can/netlink.h>
#include <net/netlink.h>

#if CFG_SUPPORT_WAPI
extern UINT_8 keyStructBuf[1024];	/* add/remove key shared buffer */
#else
extern UINT_8 keyStructBuf[100];	/* add/remove key shared buffer */
#endif
/* workaround for some ANR CRs. if suppliant is blocked longer than 10s, wifi hal will tell wifiMonitor
* to teminate. for the case which can block supplicant 10s is to del key more than 5 times. the root cause
* is that there is no resource in TC4, so del key command was not able to set, and then oid
* timeout was happed. if we found the root cause why fw couldn't release TC resouce, we will remove this
* workaround
*/


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define GOOGLE_OUI 0x001A11

typedef enum {
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

} ANDROID_VENDOR_SUB_COMMAND;

typedef enum {
	WIFI_SUBCMD_GET_CHANNEL_LIST = ANDROID_NL80211_SUBCMD_WIFI_RANGE_START,

	WIFI_SUBCMD_GET_FEATURE_SET,                     /* 0x0002 */
	WIFI_SUBCMD_GET_FEATURE_SET_MATRIX,              /* 0x0003 */
	WIFI_SUBCMD_SET_PNO_RANDOM_MAC_OUI,              /* 0x0004 */
	WIFI_SUBCMD_NODFS_SET,                           /* 0x0005 */
	WIFI_SUBCMD_SET_COUNTRY_CODE,                    /* 0x0006 */
	WIFI_SUBCMD_SET_RSSI_MONITOR,			 /* 0x0007 */

	WIFI_SUBCMD_GET_ROAMING_CAPABILITIES,            /* 0x0008 */
	WIFI_SUBCMD_CONFIG_ROAMING = 0x000a,		 /* 0x000a */
	WIFI_SUBCMD_ENABLE_ROAMING = 0x000b,		 /* 0x000b */
	WIFI_SUBCMD_SELECT_TX_POWER_SCENARIO,		 /* 0x000c */
	/* Add more sub commands here */

} WIFI_SUB_COMMAND;

typedef enum {
	GSCAN_SUBCMD_GET_CAPABILITIES = ANDROID_NL80211_SUBCMD_GSCAN_RANGE_START,

	GSCAN_SUBCMD_SET_CONFIG,						  /* 0x1001 */
	GSCAN_SUBCMD_SET_SCAN_CONFIG,					  /* 0x1002 */
	GSCAN_SUBCMD_ENABLE_GSCAN,						  /* 0x1003 */
	GSCAN_SUBCMD_GET_SCAN_RESULTS,					  /* 0x1004 */
	GSCAN_SUBCMD_SCAN_RESULTS,						  /* 0x1005 */

	GSCAN_SUBCMD_SET_HOTLIST,						  /* 0x1006 */

	GSCAN_SUBCMD_SET_SIGNIFICANT_CHANGE_CONFIG,	  /* 0x1007 */
	GSCAN_SUBCMD_ENABLE_FULL_SCAN_RESULTS,			  /* 0x1008 */
	/* Add more sub commands here */

} GSCAN_SUB_COMMAND;

typedef enum {
	LSTATS_SUBCMD_GET_INFO = ANDROID_NL80211_SUBCMD_LSTATS_RANGE_START,
} LSTATS_SUB_COMMAND;


/* moved from wifi_logger.cpp */
enum DEBUG_SUB_COMMAND {
	LOGGER_START_LOGGING = ANDROID_NL80211_SUBCMD_DEBUG_RANGE_START,
	LOGGER_GET_VER
};

/* moved from wifi_logger.cpp */
enum LOGGER_ATTRIBUTE {
	LOGGER_ATTRIBUTE_DRIVER_VER,
	LOGGER_ATTRIBUTE_FW_VER
};

typedef enum {
	GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS,
	GSCAN_EVENT_HOTLIST_RESULTS_FOUND,
	GSCAN_EVENT_SCAN_RESULTS_AVAILABLE,
	GSCAN_EVENT_FULL_SCAN_RESULTS,
	RTT_EVENT_COMPLETE,
	GSCAN_EVENT_COMPLETE_SCAN,
	GSCAN_EVENT_HOTLIST_RESULTS_LOST
} WIFI_VENDOR_EVENT;

typedef enum {
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
	WIFI_ATTRIBUTE_ROAMING_STATE

} WIFI_ATTRIBUTE;

typedef enum {
	GSCAN_ATTRIBUTE_CAPABILITIES = 1,

	GSCAN_ATTRIBUTE_NUM_BUCKETS = 10,
	GSCAN_ATTRIBUTE_BASE_PERIOD,
	GSCAN_ATTRIBUTE_BUCKETS_BAND,
	GSCAN_ATTRIBUTE_BUCKET_ID,
	GSCAN_ATTRIBUTE_BUCKET_PERIOD,
	GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS,
	GSCAN_ATTRIBUTE_BUCKET_CHANNELS,
	GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN,
	GSCAN_ATTRIBUTE_REPORT_THRESHOLD,
	GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE,

	GSCAN_ATTRIBUTE_ENABLE_FEATURE = 20,
	GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE,	/* indicates no more results */
	GSCAN_ATTRIBUTE_FLUSH_FEATURE,	/* Flush all the configs */
	GSCAN_ENABLE_FULL_SCAN_RESULTS,
	GSCAN_ATTRIBUTE_REPORT_EVENTS,

	GSCAN_ATTRIBUTE_NUM_OF_RESULTS = 30,
	GSCAN_ATTRIBUTE_FLUSH_RESULTS,
	GSCAN_ATTRIBUTE_SCAN_RESULTS,	/* flat array of wifi_scan_result */
	GSCAN_ATTRIBUTE_SCAN_ID,	/* indicates scan number */
	GSCAN_ATTRIBUTE_SCAN_FLAGS, /* indicates if scan was aborted */
	GSCAN_ATTRIBUTE_AP_FLAGS,	/* flags on significant change event */

	GSCAN_ATTRIBUTE_SSID = 40,
	GSCAN_ATTRIBUTE_BSSID,
	GSCAN_ATTRIBUTE_CHANNEL,
	GSCAN_ATTRIBUTE_RSSI,
	GSCAN_ATTRIBUTE_TIMESTAMP,
	GSCAN_ATTRIBUTE_RTT,
	GSCAN_ATTRIBUTE_RTTSD,

	GSCAN_ATTRIBUTE_HOTLIST_BSSIDS = 50,
	GSCAN_ATTRIBUTE_RSSI_LOW,
	GSCAN_ATTRIBUTE_RSSI_HIGH,
	GSCAN_ATTRIBUTE_HOTLIST_ELEM,
	GSCAN_ATTRIBUTE_HOTLIST_FLUSH,

	GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE = 60,
	GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE,
	GSCAN_ATTRIBUTE_MIN_BREACHING,
	GSCAN_ATTRIBUTE_NUM_AP,
	GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_BSSIDS,
	GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH

} GSCAN_ATTRIBUTE;

typedef enum {
	LSTATS_ATTRIBUTE_STATS = 2,
} LSTATS_ATTRIBUTE;

typedef enum {
	WIFI_BAND_UNSPECIFIED,
	WIFI_BAND_BG = 1,		/* 2.4 GHz */
	WIFI_BAND_A = 2,		/* 5 GHz without DFS */
	WIFI_BAND_A_DFS = 4,		/* 5 GHz DFS only */
	WIFI_BAND_A_WITH_DFS = 6,	/* 5 GHz with DFS */
	WIFI_BAND_ABG = 3,		/* 2.4 GHz + 5 GHz; no DFS */
	WIFI_BAND_ABG_WITH_DFS = 7, /* 2.4 GHz + 5 GHz with DFS */
} WIFI_BAND;

typedef enum {
	WIFI_SCAN_BUFFER_FULL,
	WIFI_SCAN_COMPLETE,
} WIFI_SCAN_EVENT;

#define GSCAN_MAX_REPORT_THRESHOLD   1024000
#define GSCAN_MAX_CHANNELS                 8
#define GSCAN_MAX_BUCKETS                  8
#define MAX_HOTLIST_APS                   16
#define MAX_SIGNIFICANT_CHANGE_APS        16
#define PSCAN_MAX_SCAN_CACHE_SIZE         16
#define PSCAN_MAX_AP_CACHE_PER_SCAN       16
#define PSCAN_VERSION                      1

#define MAX_BUFFERED_GSCN_RESULTS 5



#define GSCAN_MAX_REPORT_THRESHOLD   1024000
#define GSCAN_MAX_CHANNELS                 8
#define GSCAN_MAX_BUCKETS                  8
#define MAX_HOTLIST_APS                   16
#define MAX_SIGNIFICANT_CHANGE_APS        16
#define PSCAN_MAX_SCAN_CACHE_SIZE         16
#define PSCAN_MAX_AP_CACHE_PER_SCAN       16
#define PSCAN_VERSION                      1

#define MAX_BUFFERED_GSCN_RESULTS 5

#define MAX_FW_ROAMING_BLACKLIST_SIZE	16
#define MAX_FW_ROAMING_WHITELIST_SIZE	16
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
typedef UINT_64 wifi_timestamp;	/* In microseconds (us) */
typedef UINT_64 wifi_timespan;	/* In nanoseconds  (ns) */

typedef UINT_8 mac_addr[6];
typedef UINT_32 wifi_channel;	/* indicates channel frequency in MHz */
typedef INT_32 wifi_rssi;

/*******************************************************************************
*                           MACROS
********************************************************************************
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
#define NLA_PUT(skb, attrtype, attrlen, data) mtk_cfg80211_NLA_PUT(skb, attrtype, attrlen, data)

#define NLA_PUT_TYPE(skb, type, attrtype, value) mtk_cfg80211_nla_put_type(skb, type, attrtype, value)

#define NLA_PUT_U8(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U8, attrtype, value)

#define NLA_PUT_U16(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U16, attrtype, value)

#define NLA_PUT_U32(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U32, attrtype, value)

#define NLA_PUT_U64(skb, attrtype, value) \
	NLA_PUT_TYPE(skb, NLA_PUT_DATE_U64, attrtype, value)

#endif

/********************************************************************************
*				P R I V A T E   D A T A
*
********************************************************************************/

typedef struct _PARAM_WIFI_GSCAN_GET_RESULT_PARAMS {
	UINT_32 get_num;
	UINT_8 flush;
} PARAM_WIFI_GSCAN_GET_RESULT_PARAMS, *P_PARAM_WIFI_GSCAN_GET_RESULT_PARAMS;

typedef struct _PARAM_WIFI_GSCAN_ACTION_CMD_PARAMS {
	UINT_8 ucPscanAct;
	UINT_8 aucReserved[3];
} PARAM_WIFI_GSCAN_ACTION_CMD_PARAMS, *P_PARAM_WIFI_GSCAN_ACTION_CMD_PARAMS;

typedef struct _PARAM_WIFI_GSCAN_CAPABILITIES_STRUCT_T {
	UINT_32 max_scan_cache_size;	/*total space allocated for scan (in bytes) */
	UINT_32 max_scan_buckets;	/*maximum number of channel buckets */
	UINT_32 max_ap_cache_per_scan;	/*maximum number of APs that can be stored per scan */
	UINT_32 max_rssi_sample_size;	/*number of RSSI samples used for averaging RSSI */
	UINT_32 max_scan_reporting_threshold;	/*max possible report_threshold as described in wifi_scan_cmd_params */
	UINT_32 max_hotlist_aps;	/*maximum number of entries for hotlist APs */
	UINT_32 max_significant_wifi_change_aps;	/*maximum number of entries for significant wifi change APs */
	UINT_32 max_bssid_history_entries;	/*number of BSSID/RSSI entries that device can hold */
} PARAM_WIFI_GSCAN_CAPABILITIES_STRUCT_T, *P_PARAM_WIFI_GSCAN_CAPABILITIES_STRUCT_T;

typedef struct _PARAM_WIFI_GSCAN_CHANNEL_SPEC {
	UINT_32 channel;
	UINT_32 dwellTimeMs;
	UINT_32 passive;
	/* Add channel class */
} PARAM_WIFI_GSCAN_CHANNEL_SPEC, *P_PARAM_WIFI_GSCAN_CHANNEL_SPEC;

typedef struct _PARAM_WIFI_GSCAN_BUCKET_SPEC {
	UINT_32 bucket;		/* bucket index, 0 based */
	WIFI_BAND band;		/*when UNSPECIFIED, use channel lis */
	UINT_32 period;	/* desired period, in millisecond; if this is too  low, the firmware should choose to generate
			 *  results as fast as it can instead of failing the command
			 */
	/* report_events semantics -
	 *  0 => report only when scan history is % full
	 *  1 => same as 0 + report a scan completion event after scanning this bucket
	 *  2 => same as 1 + forward scan results (beacons/probe responses + IEs) in real time to HAL
	 *  3 => same as 2 + forward scan results (beacons/probe responses + IEs) in real time to
	 *  supplicant as well (optional) .
	 */
	UINT_8 report_events;

	UINT_32 num_channels;
	PARAM_WIFI_GSCAN_CHANNEL_SPEC channels[GSCAN_MAX_CHANNELS];
	/* channels to scan; these may include DFS channels */
} PARAM_WIFI_GSCAN_BUCKET_SPEC, *P_PARAM_WIFI_GSCAN_BUCKET_SPEC;

typedef struct _PARAM_WIFI_GSCAN_CMD_PARAMS {
	UINT_32 base_period;	/* base timer period in ms */
	UINT_32 max_ap_per_scan;
	/* number of APs to store in each scan in the
	* BSSID/RSSI history buffer (keep the highest RSSI APs)
	*/
	UINT_32 report_threshold;	/* in %, when scan buffer is this much full, wake up AP */
	UINT_32 num_scans;
	UINT_32 num_buckets;
	PARAM_WIFI_GSCAN_BUCKET_SPEC buckets[GSCAN_MAX_BUCKETS];
} PARAM_WIFI_GSCAN_CMD_PARAMS, *P_PARAM_WIFI_GSCAN_CMD_PARAMS;

typedef struct _PARAM_WIFI_GSCAN_RESULT {
	wifi_timestamp ts;	/* time since boot (in microsecond) when the result was */
	/* retrieved */
	UINT_8 ssid[32 + 1];	/* null terminated */
	mac_addr bssid;
	wifi_channel channel;	/* channel frequency in MHz */
	wifi_rssi rssi;		/* in db */
	wifi_timespan rtt;	/* in nanoseconds */
	wifi_timespan rtt_sd;	/* standard deviation in rtt */
	UINT_16 beacon_period;	/* period advertised in the beacon */
	UINT_16 capability;	/* capabilities advertised in the beacon */
	UINT_32 ie_length;	/* size of the ie_data blob */
	UINT_8 ie_data[1];	/* blob of all the information elements found in the */
	/* beacon; this data should be a packed list of */
	/* wifi_information_element objects, one after the other. */
	/* other fields */
} PARAM_WIFI_GSCAN_RESULT, *P_PARAM_WIFI_GSCAN_RESULT;

	   /* Significant wifi change */
#if 0
	typedef struct _PARAM_WIFI_CHANGE_RESULT {
		mac_addr bssid;	/* BSSID */
		wifi_channel channel;	/* channel frequency in MHz */
		UINT_32 num_rssi;	/* number of rssi samples */
		wifi_rssi rssi[8];	/* RSSI history in db */
	} PARAM_WIFI_CHANGE_RESULT, *P_PARAM_WIFI_CHANGE_RESULT;
#endif

typedef struct _PARAM_WIFI_CHANGE_RESULT {
	UINT_16 flags;
	UINT_16 channel;
	mac_addr bssid;	/* BSSID */
	INT_8 rssi[8];	/* RSSI history in db */
} PARAM_WIFI_CHANGE_RESULT, *P_PARAM_WIFI_CHANGE_RESULT;

typedef struct _PARAM_AP_THRESHOLD {
	mac_addr bssid;	/* AP BSSID */
	wifi_rssi low;	/* low threshold */
	wifi_rssi high;	/* high threshold */
	wifi_channel channel;	/* channel hint */
} PARAM_AP_THRESHOLD, *P_PARAM_AP_THRESHOLD;

typedef struct _PARAM_WIFI_BSSID_HOTLIST {
	UINT_32 lost_ap_sample_size;
	UINT_32 num_ap;	/* number of hotlist APs */
	PARAM_AP_THRESHOLD ap[MAX_HOTLIST_APS];	/* hotlist APs */
} PARAM_WIFI_BSSID_HOTLIST, *P_PARAM_WIFI_BSSID_HOTLIST;

typedef struct _PARAM_WIFI_SIGNIFICANT_CHANGE {
	UINT_16 rssi_sample_size;	/* number of samples for averaging RSSI */
	UINT_16 lost_ap_sample_size;	/* number of samples to confirm AP loss */
	UINT_16 min_breaching;	/* number of APs breaching threshold */
	UINT_16 num_ap;	/* max 64 */
	PARAM_AP_THRESHOLD ap[MAX_SIGNIFICANT_CHANGE_APS];
} PARAM_WIFI_SIGNIFICANT_CHANGE, *P_PARAM_WIFI_SIGNIFICANT_CHANGE;

/* channel operating width */
typedef enum {
	WIFI_CHAN_WIDTH_20 = 0,
	WIFI_CHAN_WIDTH_40 = 1,
	WIFI_CHAN_WIDTH_80 = 2,
	WIFI_CHAN_WIDTH_160 = 3,
	WIFI_CHAN_WIDTH_80P80 = 4,
	WIFI_CHAN_WIDTH_5 = 5,
	WIFI_CHAN_WIDTH_10 = 6,
	WIFI_CHAN_WIDTH_INVALID = -1
} WIFI_CHANNEL_WIDTH;

/* channel information */
typedef struct {
	WIFI_CHANNEL_WIDTH width;
	UINT_32 center_freq;
	UINT_32 center_freq0;
	UINT_32 center_freq1;
} WIFI_CHANNEL_INFO;

/* channel statistics */
typedef struct {
	WIFI_CHANNEL_INFO channel;
	UINT_32 on_time;
	UINT_32 cca_busy_time;
} WIFI_CHANNEL_STAT;

/* radio statistics */
typedef struct {
	UINT_32 radio;
	UINT_32 on_time;
	UINT_32 tx_time;
	UINT_32 rx_time;
	UINT_32 on_time_scan;
	UINT_32 on_time_nbd;
	UINT_32 on_time_gscan;
	UINT_32 on_time_roam_scan;
	UINT_32 on_time_pno_scan;
	UINT_32 on_time_hs20;
	UINT_32 num_channels;
	WIFI_CHANNEL_STAT channels[];
} WIFI_RADIO_STAT;

/* wifi rate */
typedef struct {
	UINT_32 preamble:3;
	UINT_32 nss:2;
	UINT_32 bw:3;
	UINT_32 rateMcsIdx:8;

	UINT_32 reserved:16;
	UINT_32 bitrate;
} WIFI_RATE;

/* per rate statistics */
typedef struct {
	WIFI_RATE rate;
	UINT_32 tx_mpdu;
	UINT_32 rx_mpdu;
	UINT_32 mpdu_lost;
	UINT_32 retries;
	UINT_32 retries_short;
	UINT_32 retries_long;
} WIFI_RATE_STAT;

/*wifi_interface_link_layer_info*/
typedef enum {
	WIFI_DISCONNECTED = 0,
	WIFI_AUTHENTICATING = 1,
	WIFI_ASSOCIATING = 2,
	WIFI_ASSOCIATED = 3,
	WIFI_EAPOL_STARTED = 4,
	WIFI_EAPOL_COMPLETED = 5,
} WIFI_CONNECTION_STATE;

typedef enum {
	WIFI_ROAMING_IDLE = 0,
	WIFI_ROAMING_ACTIVE = 1,
} WIFI_ROAM_STATE;

typedef enum {
	WIFI_INTERFACE_STA = 0,
	WIFI_INTERFACE_SOFTAP = 1,
	WIFI_INTERFACE_IBSS = 2,
	WIFI_INTERFACE_P2P_CLIENT = 3,
	WIFI_INTERFACE_P2P_GO = 4,
	WIFI_INTERFACE_NAN = 5,
	WIFI_INTERFACE_MESH = 6,
	WIFI_INTERFACE_UNKNOWN = -1
} WIFI_INTERFACE_MODE;

typedef struct {
	WIFI_INTERFACE_MODE mode;
	u8 mac_addr[6];
	WIFI_CONNECTION_STATE state;
	WIFI_ROAM_STATE roaming;
	u32 capabilities;
	u8 ssid[33];
	u8 bssid[6];
	u8 ap_country_str[3];
	u8 country_str[3];
} WIFI_INTERFACE_LINK_LAYER_INFO;

/* access categories */
typedef enum {
	WIFI_AC_VO = 0,
	WIFI_AC_VI = 1,
	WIFI_AC_BE = 2,
	WIFI_AC_BK = 3,
	WIFI_AC_MAX = 4,
} WIFI_TRAFFIC_AC;

/* wifi peer type */
typedef enum {
	WIFI_PEER_STA,
	WIFI_PEER_AP,
	WIFI_PEER_P2P_GO,
	WIFI_PEER_P2P_CLIENT,
	WIFI_PEER_NAN,
	WIFI_PEER_TDLS,
	WIFI_PEER_INVALID,
} WIFI_PEER_TYPE;

/* per peer statistics */
typedef struct {
	WIFI_PEER_TYPE type;
	UINT_8 peer_mac_address[6];
	UINT_32 capabilities;
	UINT_32 num_rate;
	WIFI_RATE_STAT rate_stats[];
} WIFI_PEER_INFO;

/* per access category statistics */
typedef struct {
	WIFI_TRAFFIC_AC ac;
	UINT_32 tx_mpdu;
	UINT_32 rx_mpdu;
	UINT_32 tx_mcast;

	UINT_32 rx_mcast;
	UINT_32 rx_ampdu;
	UINT_32 tx_ampdu;
	UINT_32 mpdu_lost;
	UINT_32 retries;
	UINT_32 retries_short;
	UINT_32 retries_long;
	UINT_32 contention_time_min;
	UINT_32 contention_time_max;
	UINT_32 contention_time_avg;
	UINT_32 contention_num_samples;
} WIFI_WMM_AC_STAT;

/* interface statistics */
typedef struct {
	WIFI_INTERFACE_LINK_LAYER_INFO info;
	UINT_32 beacon_rx;
	UINT_32 mgmt_rx;
	UINT_32 mgmt_action_rx;
	UINT_32 mgmt_action_tx;
	wifi_rssi rssi_mgmt;
	wifi_rssi rssi_data;
	wifi_rssi rssi_ack;
	WIFI_WMM_AC_STAT ac[WIFI_AC_MAX];
	UINT_32 num_peers;
	WIFI_PEER_INFO peer_info[];
} WIFI_IFACE_STAT;


typedef enum _ENUM_NLA_PUT_DATE_TYPE {
	NLA_PUT_DATE_U8 = 0,
	NLA_PUT_DATE_U16,
	NLA_PUT_DATE_U32,
	NLA_PUT_DATE_U64,
} ENUM_NLA_PUT_DATE_TYPE;


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/



/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
int mtk_cfg80211_NLA_PUT(struct sk_buff *skb, int attrtype, int attrlen, const void *data);

int mtk_cfg80211_nla_put_type(struct sk_buff *skb, ENUM_NLA_PUT_DATE_TYPE type, int attrtype, const void *value);

int mtk_cfg80211_vendor_get_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_set_config(struct wiphy *wiphy, struct wireless_dev *wdev,
				   const void *data, int data_len);

int mtk_cfg80211_vendor_set_scan_config(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_set_significant_change(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_set_hotlist(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_enable_scan(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_enable_full_scan_results(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_get_scan_results(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_get_channel_list(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_set_country_code(struct wiphy *wiphy, struct wireless_dev *wdev,
					 const void *data, int data_len);

int mtk_cfg80211_vendor_get_roaming_capabilities(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);

int mtk_cfg80211_vendor_config_roaming(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);

int mtk_cfg80211_vendor_enable_roaming(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len);


int mtk_cfg80211_vendor_llstats_get_info(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len);

int mtk_cfg80211_vendor_event_complete_scan(struct wiphy *wiphy, struct wireless_dev *wdev,
					WIFI_SCAN_EVENT complete);

int mtk_cfg80211_vendor_event_scan_results_available(struct wiphy *wiphy, struct wireless_dev *wdev,
					UINT_32 num);

int mtk_cfg80211_vendor_event_full_scan_results(struct wiphy *wiphy, struct wireless_dev *wdev,
					P_PARAM_WIFI_GSCAN_RESULT pdata, UINT_32 data_len);

int mtk_cfg80211_vendor_event_significant_change_results(struct wiphy *wiphy, struct wireless_dev *wdev,
					P_PARAM_WIFI_CHANGE_RESULT pdata, UINT_32 data_len);

int mtk_cfg80211_vendor_event_hotlist_ap_found(struct wiphy *wiphy, struct wireless_dev *wdev,
					P_PARAM_WIFI_GSCAN_RESULT pdata, UINT_32 data_len);

int mtk_cfg80211_vendor_event_hotlist_ap_lost(struct wiphy *wiphy, struct wireless_dev *wdev,
					P_PARAM_WIFI_GSCAN_RESULT pdata, UINT_32 data_len);

int mtk_cfg80211_vendor_get_supported_feature_set(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);
int mtk_cfg80211_vendor_set_tx_power_scenario(
	struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int data_len);
int mtk_cfg80211_vendor_get_version(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len);
int mtk_cfg80211_vendor_set_scan_mac_oui(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);
#endif				/* _GL_VENDOR_H */
