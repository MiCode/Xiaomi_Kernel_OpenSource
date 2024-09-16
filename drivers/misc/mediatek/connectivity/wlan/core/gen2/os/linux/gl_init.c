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

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ********************************************************************************
 */
#include "gl_os.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "gl_cfg80211.h"
#include "precomp.h"
#if CFG_SUPPORT_AGPS_ASSIST
#include "gl_kal.h"
#endif
#if CFG_TC1_FEATURE
#include <tc1_partition.h>
#endif
#include "gl_vendor.h"

#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* #define MAX_IOREQ_NUM   10 */

BOOLEAN fgIsUnderSuspend;
BOOLEAN fgNvramAvailable;
UINT_8 g_aucNvram[CFG_FILE_WIFI_REC_SIZE];

#if CFG_ENABLE_WIFI_DIRECT
spinlock_t g_p2p_lock;
int g_u4P2PEnding;
int g_u4P2POnOffing;
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* Tasklet mechanism is like buttom-half in Linux. We just want to
 * send a signal to OS for interrupt defer processing. All resources
 * are NOT allowed reentry, so txPacket, ISR-DPC and ioctl must avoid preempty.
 */
typedef struct _WLANDEV_INFO_T {
	struct net_device *prDev;
} WLANDEV_INFO_T, *P_WLANDEV_INFO_T;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

#define CHAN2G(_channel, _freq, _flags)         \
{                                           \
	.band               = IEEE80211_BAND_2GHZ,  \
	.center_freq        = (_freq),              \
	.hw_value           = (_channel),           \
	.flags              = (_flags),             \
	.max_antenna_gain   = 0,                    \
	.max_power          = 30,                   \
}

static struct ieee80211_channel mtk_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

#define CHAN5G(_channel, _flags)                    \
{                                               \
	.band               = IEEE80211_BAND_5GHZ,      \
	.center_freq        = (((_channel >= 182) && (_channel <= 196)) ? \
				 (4000 + (5 * (_channel))) : (5000 + (5 * (_channel)))),  \
	.hw_value           = (_channel),               \
	.flags              = (_flags),                 \
	.max_antenna_gain   = 0,                        \
	.max_power          = 30,                       \
}

static struct ieee80211_channel mtk_5ghz_channels[] = {
	/* UNII-1 */
	CHAN5G(34, 0), CHAN5G(36, 0),
	CHAN5G(38, 0), CHAN5G(40, 0),
	CHAN5G(42, 0), CHAN5G(44, 0),
	CHAN5G(46, 0), CHAN5G(48, 0),
	/* UNII-2 */
	CHAN5G(52, IEEE80211_CHAN_RADAR),
	CHAN5G(56, IEEE80211_CHAN_RADAR),
	CHAN5G(60, IEEE80211_CHAN_RADAR),
	CHAN5G(64, IEEE80211_CHAN_RADAR),
	/* UNII-2e */
	CHAN5G(100, IEEE80211_CHAN_RADAR),
	CHAN5G(104, IEEE80211_CHAN_RADAR),
	CHAN5G(108, IEEE80211_CHAN_RADAR),
	CHAN5G(112, IEEE80211_CHAN_RADAR),
	CHAN5G(116, IEEE80211_CHAN_RADAR),
	CHAN5G(120, IEEE80211_CHAN_RADAR),
	CHAN5G(124, IEEE80211_CHAN_RADAR),
	CHAN5G(128, IEEE80211_CHAN_RADAR),
	CHAN5G(132, IEEE80211_CHAN_RADAR),
	CHAN5G(136, IEEE80211_CHAN_RADAR),
	CHAN5G(140, IEEE80211_CHAN_RADAR),
	CHAN5G(144, IEEE80211_CHAN_RADAR),
	/* UNII-3 */
	CHAN5G(149, 0),
	CHAN5G(153, 0), CHAN5G(157, 0),
	CHAN5G(161, 0), CHAN5G(165, 0),
	CHAN5G(169, 0), CHAN5G(173, 0),
	CHAN5G(184, 0), CHAN5G(188, 0),
	CHAN5G(192, 0), CHAN5G(196, 0),
	CHAN5G(200, 0), CHAN5G(204, 0),
	CHAN5G(208, 0), CHAN5G(212, 0),
	CHAN5G(216, 0),
};

#define RATETAB_ENT(_rate, _rateid, _flags) \
{                                           \
	.bitrate    = (_rate),              \
	.hw_value   = (_rateid),            \
	.flags      = (_flags),             \
}

/* for cfg80211 - rate table */
static struct ieee80211_rate mtk_rates[] = {
	RATETAB_ENT(10, 0x1000, 0),
	RATETAB_ENT(20, 0x1001, 0),
	RATETAB_ENT(55, 0x1002, 0),
	RATETAB_ENT(110, 0x1003, 0),	/* 802.11b */
	RATETAB_ENT(60, 0x2000, 0),
	RATETAB_ENT(90, 0x2001, 0),
	RATETAB_ENT(120, 0x2002, 0),
	RATETAB_ENT(180, 0x2003, 0),
	RATETAB_ENT(240, 0x2004, 0),
	RATETAB_ENT(360, 0x2005, 0),
	RATETAB_ENT(480, 0x2006, 0),
	RATETAB_ENT(540, 0x2007, 0),	/* 802.11a/g */
};

#define mtk_a_rates         (mtk_rates + 4)
#define mtk_a_rates_size    (ARRAY_SIZE(mtk_rates) - 4)
#define mtk_g_rates         (mtk_rates + 0)
#define mtk_g_rates_size    (ARRAY_SIZE(mtk_rates) - 0)

#define WLAN_MCS_INFO                                 \
{                                                       \
	.rx_mask        = {0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0},\
	.rx_highest     = 0,                                \
	.tx_params      = IEEE80211_HT_MCS_TX_DEFINED,      \
}

#define WLAN_HT_CAP                                   \
{                                                       \
	.ht_supported   = true,                             \
	.cap            = IEEE80211_HT_CAP_SUP_WIDTH_20_40  \
		    | IEEE80211_HT_CAP_SM_PS            \
		    | IEEE80211_HT_CAP_GRN_FLD          \
		    | IEEE80211_HT_CAP_SGI_20           \
		    | IEEE80211_HT_CAP_SGI_40,          \
	.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,       \
	.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_NONE,   \
	.mcs            = WLAN_MCS_INFO,                  \
}

/**********************************************************
* Public for both legacy Wi-Fi and P2P to access
**********************************************************/
struct ieee80211_supported_band mtk_band_2ghz = {
	.band = IEEE80211_BAND_2GHZ,
	.channels = mtk_2ghz_channels,
	.n_channels = ARRAY_SIZE(mtk_2ghz_channels),
	.bitrates = mtk_g_rates,
	.n_bitrates = mtk_g_rates_size,
	.ht_cap = WLAN_HT_CAP,
};

struct ieee80211_supported_band mtk_band_5ghz = {
	.band = IEEE80211_BAND_5GHZ,
	.channels = mtk_5ghz_channels,
	.n_channels = ARRAY_SIZE(mtk_5ghz_channels),
	.bitrates = mtk_a_rates,
	.n_bitrates = mtk_a_rates_size,
	.ht_cap = WLAN_HT_CAP,
};

const UINT_32 mtk_cipher_suites[] = {
	/* keep WEP first, it may be removed below */
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,

	/* keep last -- depends on hw flags! */
	WLAN_CIPHER_SUITE_AES_CMAC,
	WLAN_CIPHER_SUITE_NO_GROUP_ADDR
};

/*********************************************************/

/* NIC interface name */
#define NIC_INF_NAME    "wlan%d"	/* interface name */
/* support to change debug module info dynamically */
UINT_8 aucDebugModule[DBG_MODULE_NUM];

UINT_32 u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;
UINT_32 u4FwLogLevel = ENUM_WIFI_LOG_LEVEL_OFF;

/* 4 2007/06/26, mikewu, now we don't use this, we just fix the number of wlan device to 1 */
static WLANDEV_INFO_T arWlanDevInfo[CFG_MAX_WLAN_DEVICES] = { {0} };

static UINT_32 u4WlanDevNum;	/* How many NICs coexist now */

/**20150205 added work queue for sched_scan to avoid cfg80211 stop schedule scan dead loack**/
struct delayed_work sched_workq;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#if CFG_ENABLE_WIFI_DIRECT
static SUB_MODULE_HANDLER rSubModHandler[SUB_MODULE_NUM] = { {NULL} };
#endif

static struct cfg80211_ops mtk_wlan_ops = {
	.suspend = mtk_cfg80211_suspend,
	.resume	= mtk_cfg80211_resume,
	.change_virtual_intf = mtk_cfg80211_change_iface,
	.add_key = mtk_cfg80211_add_key,
	.get_key = mtk_cfg80211_get_key,
	.del_key = mtk_cfg80211_del_key,
	.set_default_key = mtk_cfg80211_set_default_key,
	.set_default_mgmt_key = mtk_cfg80211_set_default_mgmt_key,
	.get_station = mtk_cfg80211_get_station,
	.change_station = mtk_cfg80211_change_station,
	.add_station = mtk_cfg80211_add_station,
	.del_station = mtk_cfg80211_del_station,
	.scan = mtk_cfg80211_scan,
	.abort_scan = mtk_cfg80211_abort_scan,
	.connect = mtk_cfg80211_connect,
	.disconnect = mtk_cfg80211_disconnect,
	.join_ibss = mtk_cfg80211_join_ibss,
	.leave_ibss = mtk_cfg80211_leave_ibss,
	.set_power_mgmt = mtk_cfg80211_set_power_mgmt,
	.set_pmksa = mtk_cfg80211_set_pmksa,
	.del_pmksa = mtk_cfg80211_del_pmksa,
	.flush_pmksa = mtk_cfg80211_flush_pmksa,
	.assoc = mtk_cfg80211_assoc,
	/* Action Frame TX/RX */
	.remain_on_channel = mtk_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = mtk_cfg80211_cancel_remain_on_channel,
	.mgmt_tx = mtk_cfg80211_mgmt_tx,
/* .mgmt_tx_cancel_wait        = mtk_cfg80211_mgmt_tx_cancel_wait, */
	.mgmt_frame_register = mtk_cfg80211_mgmt_frame_register,
#ifdef CONFIG_NL80211_TESTMODE
	.testmode_cmd = mtk_cfg80211_testmode_cmd,
#endif
#if (CFG_SUPPORT_TDLS == 1)
	.tdls_mgmt = TdlsexCfg80211TdlsMgmt,
	.tdls_oper = TdlsexCfg80211TdlsOper,
#endif /* CFG_SUPPORT_TDLS */
#if 1	/* Remove schedule_scan because we need more verification for NLO */
	.sched_scan_start = mtk_cfg80211_sched_scan_start,
	.sched_scan_stop = mtk_cfg80211_sched_scan_stop,
#endif
	.update_ft_ies = mtk_cfg80211_update_ft_ies,
#if CFG_SUPPORT_WPA3
	.external_auth = mtk_cfg80211_external_auth,
#endif
};


static const struct wiphy_vendor_command mtk_wlan_vendor_ops[] = {
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_GET_CHANNEL_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_channel_list
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_COUNTRY_CODE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_country_code
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_PNO_RANDOM_MAC_OUI
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV
			| WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_scan_mac_oui
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_GET_ROAMING_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_roaming_capabilities
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_CONFIG_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_config_roaming
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_ENABLE_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_enable_roaming
	},
	/* GSCAN */
#if CFG_SUPPORT_GSCN
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_gscan_capabilities
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_SET_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_config
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_SET_SCAN_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_scan_config
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_ENABLE_GSCAN
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_enable_scan
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_ENABLE_FULL_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_enable_full_scan_results
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_GET_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_gscan_result
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_SET_SIGNIFICANT_CHANGE_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_significant_change
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_SET_HOTLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_hotlist
	},
#endif
	/* RTT */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = RTT_SUBCMD_GETCAPABILITY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_rtt_capabilities
	},
	/* Link Layer Statistics */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = LSTATS_SUBCMD_GET_INFO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_llstats_get_info
	},
	/* RSSI Monitoring */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_RSSI_MONITOR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_rssi_monitoring
	},
	/* Packet Keep Alive */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_OFFLOAD_START_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_packet_keep_alive_start
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_OFFLOAD_STOP_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_packet_keep_alive_stop
	},
	/* Get Driver Version or Firmware Version */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = LOGGER_GET_VER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_version
	},
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_SETBAND
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_band
	},
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_roaming_policy
	},
	/* Get Supported Feature Set */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_GET_FEATURE_SET
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_supported_feature_set
	},
	/* Set Tx Power Scenario */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SELECT_TX_POWER_SCENARIO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_tx_power_scenario
	},
};

static const struct nl80211_vendor_cmd_info mtk_wlan_vendor_events[] = {
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = GSCAN_EVENT_HOTLIST_RESULTS_FOUND
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = GSCAN_EVENT_SCAN_RESULTS_AVAILABLE
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = GSCAN_EVENT_FULL_SCAN_RESULTS
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = RTT_EVENT_COMPLETE
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = GSCAN_EVENT_COMPLETE_SCAN
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = GSCAN_EVENT_HOTLIST_RESULTS_LOST
	},
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = WIFI_EVENT_RSSI_MONITOR
	},
};

/* There isn't a lot of sense in it, but you can transmit anything you like */
static const struct ieee80211_txrx_stypes
	mtk_cfg80211_ais_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_ADHOC] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_STATION] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		      BIT(IEEE80211_STYPE_AUTH >> 4)
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_PROBE_REQ >> 4) | BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_AP_VLAN] = {
		/* copy AP */
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		      BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		      BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		      BIT(IEEE80211_STYPE_AUTH >> 4) |
		      BIT(IEEE80211_STYPE_DEAUTH >> 4) | BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) | BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_PROBE_REQ >> 4) | BIT(IEEE80211_STYPE_ACTION >> 4)
	}
};

#ifdef CONFIG_PM
static const struct wiphy_wowlan_support mtk_wlan_wowlan_support = {
	.flags = WIPHY_WOWLAN_DISCONNECT | WIPHY_WOWLAN_ANY,
};
#endif

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Override the implementation of select queue
*
* \param[in] dev Pointer to struct net_device
* \param[in] skb Pointer to struct skb_buff
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
unsigned int _cfg80211_classify8021d(struct sk_buff *skb)
{
	unsigned int dscp = 0;

	/* skb->priority values from 256->263 are magic values
	 * directly indicate a specific 802.1d priority.  This is
	 * to allow 802.1d priority to be passed directly in from
	 * tags
	 */

	if (skb->priority >= 256 && skb->priority <= 263)
		return skb->priority - 256;
	switch (skb->protocol) {
	case htons(ETH_P_IP):
		dscp = ip_hdr(skb)->tos & 0xfc;
		break;
	}
	return dscp >> 5;
}

static const UINT_16 au16Wlan1dToQueueIdx[8] = { 1, 0, 0, 1, 2, 2, 3, 3 };

#if KERNEL_VERSION(4, 19, 0) <= CFG80211_VERSION_CODE
static UINT_16 wlanSelectQueue(struct net_device *dev, struct sk_buff *skb,
				struct net_device *sb_dev, select_queue_fallback_t fallback)
#else
static UINT_16 wlanSelectQueue(struct net_device *dev, struct sk_buff *skb,
				void *accel_priv, select_queue_fallback_t fallback)
#endif
{
	skb->priority = _cfg80211_classify8021d(skb);

	return au16Wlan1dToQueueIdx[skb->priority];
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Load NVRAM data and translate it into REG_INFO_T
*
* \param[in]  prGlueInfo Pointer to struct GLUE_INFO_T
* \param[out] prRegInfo  Pointer to struct REG_INFO_T
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static void glLoadNvram(IN P_GLUE_INFO_T prGlueInfo, OUT P_REG_INFO_T prRegInfo)
{
	UINT_32 i, j, len, startAddr;
	UINT_8 aucTmp[2];
	PUINT_8 pucDest;

	ASSERT(prGlueInfo);
	ASSERT(prRegInfo);

	if ((!prGlueInfo) || (!prRegInfo))
		return;

	DBGLOG(INIT, INFO, "fgNvramAvailable = %u\n", fgNvramAvailable);
	prGlueInfo->fgNvramAvailable = fgNvramAvailable;
	if (!prGlueInfo->fgNvramAvailable) {
		DBGLOG(INIT, WARN, "Nvram not available\n");
		return;
	}

	/* load MAC Address */
#if CFG_TC1_FEATURE
	TC1_FAC_NAME(FacReadWifiMacAddr) ((unsigned char *)prRegInfo->aucMacAddr);

#else
	for (i = 0; i < PARAM_MAC_ADDR_LEN; i += sizeof(UINT_16)) {
		kalCfgDataRead16(prGlueInfo,
				 OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucMacAddress) + i,
					 (PUINT_16) (((PUINT_8) prRegInfo->aucMacAddr) + i));
	}
#endif

	/* load country code */
	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucCountryCode[0]), (PUINT_16) aucTmp);

	/* cast to wide characters */
	prRegInfo->au2CountryCode[0] = (UINT_16) aucTmp[0];
	prRegInfo->au2CountryCode[1] = (UINT_16) aucTmp[1];

	/* load default normal TX power */
	for (i = 0; i < sizeof(TX_PWR_PARAM_T); i += sizeof(UINT_16)) {
		kalCfgDataRead16(prGlueInfo,
				 OFFSET_OF(WIFI_CFG_PARAM_STRUCT, rTxPwr) + i,
				 (PUINT_16) (((PUINT_8)&(prRegInfo->rTxPwr)) + i));
	}

	/* load feature flags */
	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, ucTxPwrValid), (PUINT_16) aucTmp);
	prRegInfo->ucTxPwrValid = aucTmp[0];
	prRegInfo->ucSupport5GBand = aucTmp[1];

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, uc2G4BwFixed20M), (PUINT_16) aucTmp);
	prRegInfo->uc2G4BwFixed20M = aucTmp[0];
	prRegInfo->uc5GBwFixed20M = aucTmp[1];

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, ucEnable5GBand), (PUINT_16) aucTmp);
	prRegInfo->ucEnable5GBand = aucTmp[0];

	/* load EFUSE overriding part */
	for (i = 0; i < sizeof(prRegInfo->aucEFUSE); i += sizeof(UINT_16)) {
		kalCfgDataRead16(prGlueInfo,
				 OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucEFUSE) + i,
				 (PUINT_16) (((PUINT_8)&(prRegInfo->aucEFUSE)) + i));
	}

	/* load band edge tx power control */
	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, fg2G4BandEdgePwrUsed), (PUINT_16) aucTmp);
		prRegInfo->fg2G4BandEdgePwrUsed = (BOOLEAN) aucTmp[0];
	if (aucTmp[0]) {
		prRegInfo->cBandEdgeMaxPwrCCK = (INT_8) aucTmp[1];
			kalCfgDataRead16(prGlueInfo,
				 OFFSET_OF(WIFI_CFG_PARAM_STRUCT, cBandEdgeMaxPwrOFDM20), (PUINT_16) aucTmp);
		prRegInfo->cBandEdgeMaxPwrOFDM20 = (INT_8) aucTmp[0];
		prRegInfo->cBandEdgeMaxPwrOFDM40 = (INT_8) aucTmp[1];
	}

	/* load regulation subbands */
	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, ucRegChannelListMap), (PUINT_16) aucTmp);
	prRegInfo->eRegChannelListMap = (ENUM_REG_CH_MAP_T) aucTmp[0];
	prRegInfo->ucRegChannelListIndex = aucTmp[1];

	if (prRegInfo->eRegChannelListMap == REG_CH_MAP_CUSTOMIZED) {
		for (i = 0; i < MAX_SUBBAND_NUM; i++) {
			pucDest = (PUINT_8)&prRegInfo->rDomainInfo.rSubBand[i];
			for (j = 0; j < 6; j += sizeof(UINT_16)) {
				kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucRegSubbandInfo)
						 + (i * 6 + j), (PUINT_16) aucTmp);

				*pucDest++ = aucTmp[0];
				*pucDest++ = aucTmp[1];
			}
		}
	}
	/* load RSSI compensation */
	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, uc2GRssiCompensation), (PUINT_16) aucTmp);
	prRegInfo->uc2GRssiCompensation = aucTmp[0];
	prRegInfo->uc5GRssiCompensation = aucTmp[1];

	kalCfgDataRead16(prGlueInfo,
			 OFFSET_OF(WIFI_CFG_PARAM_STRUCT, fgRssiCompensationValidbit), (PUINT_16) aucTmp);
	prRegInfo->fgRssiCompensationValidbit = aucTmp[0];
	prRegInfo->ucRxAntennanumber = aucTmp[1];

#if CFG_SUPPORT_TX_POWER_BACK_OFF
	kalCfgDataRead(prGlueInfo,
		OFFSET_OF(WIFI_CFG_PARAM_STRUCT, fgRlmMitigatedPwrByChByMode),
		sizeof(UINT_8),
		(PUINT_16) aucTmp);
		prRegInfo->fgRlmMitigatedPwrByChByMode = aucTmp[0];

	/* load Tx Power offset perchannel per mode 40 : MAXNUM_MITIGATED_PWR_BY_CH_BY_MODE */
	kalCfgDataRead(prGlueInfo,
		OFFSET_OF(WIFI_CFG_PARAM_STRUCT, arRlmMitigatedPwrByChByMode),
		sizeof(MITIGATED_PWR_BY_CH_BY_MODE)*40,
		(PUINT_16) prRegInfo->arRlmMitigatedPwrByChByMode);

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT,
		bTxPowerLimitEnable2G), (PUINT_16) aucTmp);
	prRegInfo->bTxPowerLimitEnable2G = (BOOLEAN)aucTmp[0];
	prRegInfo->cTxBackOffMaxPower2G = aucTmp[1];

	/* load TxPower for 5G Band from nvram */
	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT,
		bTxPowerLimitEnable5G), (PUINT_16) aucTmp);
	prRegInfo->bTxPowerLimitEnable5G = (BOOLEAN)aucTmp[0];
	prRegInfo->cTxBackOffMaxPower5G = aucTmp[1];


#endif
#if CFG_SUPPORT_FCC_POWER_BACK_OFF
	/* load FCC cert. parameters */
	kalCfgDataRead(prGlueInfo,
		       OFFSET_OF(WIFI_CFG_PARAM_STRUCT, rFccTxPwrAdjust),
		       sizeof(FCC_TX_PWR_ADJUST),
		       (PUINT_16)&prRegInfo->rFccTxPwrAdjust);

	/* load FCC Channel Band Edge */
	kalCfgDataRead16(prGlueInfo,
		OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucChannelBandEdge[0]), (PUINT_16) aucTmp);
	prRegInfo->aucChannelBandEdge[0] = (UINT_8) aucTmp[0];
	prRegInfo->aucChannelBandEdge[1] = (UINT_8) aucTmp[1];

	DBGLOG(INIT, TRACE, "rFccTxPwrAdjust offset:%ld, value:%d, %d, %d, %d, [%d, %d], [%d, %d], [%d, %d]\n",
	       OFFSET_OF(WIFI_CFG_PARAM_STRUCT, rFccTxPwrAdjust),
	       prRegInfo->rFccTxPwrAdjust.fgFccTxPwrAdjust,
	       prRegInfo->rFccTxPwrAdjust.uOffsetCCK,
	       prRegInfo->rFccTxPwrAdjust.uOffsetHT20,
	       prRegInfo->rFccTxPwrAdjust.uOffsetHT40,
	       prRegInfo->rFccTxPwrAdjust.aucChannelCCK[0],
	       prRegInfo->rFccTxPwrAdjust.aucChannelCCK[1],
	       prRegInfo->rFccTxPwrAdjust.aucChannelHT20[0],
	       prRegInfo->rFccTxPwrAdjust.aucChannelHT20[1],
	       prRegInfo->rFccTxPwrAdjust.aucChannelHT40[0],
	       prRegInfo->rFccTxPwrAdjust.aucChannelHT40[1]);

	DBGLOG(INIT, TRACE, "FCC Tx power channel band edge [%d, %d]\n",
		prRegInfo->aucChannelBandEdge[0], prRegInfo->aucChannelBandEdge[1]);
#endif
	startAddr = OFFSET_OF(REG_INFO_T, aucMacAddr);
	len = sizeof(REG_INFO_T);
	DBGLOG_MEM8_IE_ONE_LINE(INIT, INFO, "RegInfo", (PUINT_8)prRegInfo + startAddr, len);
}

#if CFG_ENABLE_WIFI_DIRECT
/*----------------------------------------------------------------------------*/
/*!
* \brief called by txthread, run sub module init function
*
* \param[in]  prGlueInfo Pointer to struct GLUE_INFO_T
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanSubModRunInit(P_GLUE_INFO_T prGlueInfo)
{
	/*now, we only have p2p module */
	if (rSubModHandler[P2P_MODULE].fgIsInited == FALSE) {
		rSubModHandler[P2P_MODULE].subModInit(prGlueInfo);
		rSubModHandler[P2P_MODULE].fgIsInited = TRUE;
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief called by txthread, run sub module exit function
*
* \param[in]  prGlueInfo Pointer to struct GLUE_INFO_T
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanSubModRunExit(P_GLUE_INFO_T prGlueInfo)
{
	/*now, we only have p2p module */
	if (rSubModHandler[P2P_MODULE].fgIsInited == TRUE) {
		rSubModHandler[P2P_MODULE].subModExit(prGlueInfo);
		rSubModHandler[P2P_MODULE].fgIsInited = FALSE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief set sub module init flag, force TxThread to run sub modle init
*
* \param[in]  prGlueInfo Pointer to struct GLUE_INFO_T
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanSubModInit(P_GLUE_INFO_T prGlueInfo)
{
	/* 4  Mark HALT, notify main thread to finish current job */
	prGlueInfo->ulFlag |= GLUE_FLAG_SUB_MOD_INIT;
	/* wake up main thread */
	wake_up_interruptible(&prGlueInfo->waitq);
	/* wait main thread  finish sub module INIT */
	wait_for_completion_interruptible(&prGlueInfo->rSubModComp);

#if 0
	if (prGlueInfo->prAdapter->fgIsP2PRegistered)
		p2pNetRegister(prGlueInfo);
#endif

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief set sub module exit flag, force TxThread to run sub modle exit
*
* \param[in]  prGlueInfo Pointer to struct GLUE_INFO_T
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanSubModExit(P_GLUE_INFO_T prGlueInfo)
{
#if 0
	if (prGlueInfo->prAdapter->fgIsP2PRegistered)
		p2pNetUnregister(prGlueInfo);
#endif

	/* 4  Mark HALT, notify main thread to finish current job */
	prGlueInfo->ulFlag |= GLUE_FLAG_SUB_MOD_EXIT;
	/* wake up main thread */
	wake_up_interruptible(&prGlueInfo->waitq);
	/* wait main thread finish sub module EXIT */
	wait_for_completion_interruptible(&prGlueInfo->rSubModComp);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief set by sub module, indicate sub module is already inserted
*
* \param[in]  rSubModInit, function pointer point to sub module init function
* \param[in]  rSubModExit,  function pointer point to sub module exit function
* \param[in]  eSubModIdx,  sub module index
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
wlanSubModRegisterInitExit(SUB_MODULE_INIT rSubModInit, SUB_MODULE_EXIT rSubModExit, ENUM_SUB_MODULE_IDX_T eSubModIdx)
{
	rSubModHandler[eSubModIdx].subModInit = rSubModInit;
	rSubModHandler[eSubModIdx].subModExit = rSubModExit;
	rSubModHandler[eSubModIdx].fgIsInited = FALSE;
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief check wlan is launched or not
*
* \param[in]  (none)
*
* \return TRUE, wlan is already started
*             FALSE, wlan is not started yet
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanIsLaunched(VOID)
{
	struct net_device *prDev = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;

	/* 4 <0> Sanity check */
	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0)
		return FALSE;

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;

	ASSERT(prDev);
	if (prDev == NULL)
		return FALSE;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);
	if (prGlueInfo == NULL)
		return FALSE;

	return prGlueInfo->prAdapter->fgIsWlanLaunched;
}

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Export wlan GLUE_INFO_T pointer to p2p module
*
* \param[in]  prGlueInfo Pointer to struct GLUE_INFO_T
*
* \return TRUE: get GlueInfo pointer successfully
*            FALSE: wlan is not started yet
*/
/*---------------------------------------------------------------------------*/
BOOLEAN wlanExportGlueInfo(P_GLUE_INFO_T *prGlueInfoExpAddr)
{
	struct net_device *prDev = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;

	if (u4WlanDevNum == 0)
		return FALSE;

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	if (prDev == NULL)
		return FALSE;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	if (prGlueInfo == NULL)
		return FALSE;

	if (prGlueInfo->prAdapter->fgIsWlanLaunched == FALSE)
		return FALSE;

	*prGlueInfoExpAddr = prGlueInfo;
	return TRUE;
}

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Release prDev from wlandev_array and free tasklet object related to it.
*
* \param[in] prDev  Pointer to struct net_device
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static void wlanClearDevIdx(struct net_device *prDev)
{
	int i;

	ASSERT(prDev);

	for (i = 0; i < CFG_MAX_WLAN_DEVICES; i++) {
		if (arWlanDevInfo[i].prDev == prDev) {
			arWlanDevInfo[i].prDev = NULL;
			u4WlanDevNum--;
		}
	}

}				/* end of wlanClearDevIdx() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Allocate an unique interface index, net_device::ifindex member for this
*        wlan device. Store the net_device in wlandev_array, and initialize
*        tasklet object related to it.
*
* \param[in] prDev  Pointer to struct net_device
*
* \retval >= 0      The device number.
* \retval -1        Fail to get index.
*/
/*----------------------------------------------------------------------------*/
static int wlanGetDevIdx(struct net_device *prDev)
{
	int i;

	ASSERT(prDev);

	for (i = 0; i < CFG_MAX_WLAN_DEVICES; i++) {
		if (arWlanDevInfo[i].prDev == (struct net_device *)NULL) {
			/* Reserve 2 bytes space to store one digit of
			 * device number and NULL terminator.
			 */
			arWlanDevInfo[i].prDev = prDev;
			u4WlanDevNum++;
			return i;
		}
	}

	return -1;
}				/* end of wlanGetDevIdx() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A method of struct net_device, a primary SOCKET interface to configure
*        the interface lively. Handle an ioctl call on one of our devices.
*        Everything Linux ioctl specific is done here. Then we pass the contents
*        of the ifr->data to the request message handler.
*
* \param[in] prDev      Linux kernel netdevice
*
* \param[in] prIfReq    Our private ioctl request structure, typed for the generic
*                       struct ifreq so we can use ptr to function
*
* \param[in] cmd        Command ID
*
* \retval 0  The IOCTL command is executed successfully.
* \retval <0 The execution of IOCTL command is failed.
*/
/*----------------------------------------------------------------------------*/
int wlanDoIOCTL(struct net_device *prDev, struct ifreq *prIfReq, int i4Cmd)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	int ret = 0;

	DBGLOG(INIT, INFO, "wlanDoIOCTL Cmd= 0x%04x\n", i4Cmd);
	/* Verify input parameters for the following functions */
	ASSERT(prDev && prIfReq);
	if (!prDev || !prIfReq) {
		DBGLOG(INIT, ERROR, "Invalid input data\n");
		return -EINVAL;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL\n");
		return -EFAULT;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(INIT, ERROR, "Adapter is not ready\n");
		return -EINVAL;
	}

	if ((i4Cmd >= SIOCIWFIRST) && (i4Cmd < SIOCIWFIRSTPRIV)) {
		/* 0x8B00 ~ 0x8BDF, wireless extension region */
		ret = wext_support_ioctl(prDev, prIfReq, i4Cmd);
	} else if ((i4Cmd >= SIOCIWFIRSTPRIV) && (i4Cmd < SIOCIWLASTPRIV)) {
		/* 0x8BE0 ~ 0x8BFF, private ioctl region */
		ret = priv_support_ioctl(prDev, prIfReq, i4Cmd);
	} else if (i4Cmd == SIOCDEVPRIVATE + 1) {
		ret = priv_support_driver_cmd(prDev, prIfReq, i4Cmd);
	} else {
		DBGLOG(INIT, WARN, "Unexpected ioctl command: 0x%04x\n", i4Cmd);
		ret = -EOPNOTSUPP;
	}
	if (ret != 0)
		DBGLOG(INIT, WARN, "wlanDoIOCTL Ret err: %d\n", ret);

	return ret;
}				/* end of wlanDoIOCTL() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is to set multicast list and set rx mode.
*
* \param[in] prDev  Pointer to struct net_device
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/

static struct delayed_work workq;
struct net_device *gPrDev;
static BOOLEAN fgIsWorkMcStart = FALSE;
static BOOLEAN fgIsWorkMcEverInit = FALSE;
static struct wireless_dev *gprWdev;

static UINT_8 wlanNvramBufHandler(PVOID ctx, const CHAR *buf, UINT_16 length)
{
	DBGLOG(INIT, INFO, "buf = %p, length = %u\n", buf, length);
	if (buf == NULL || length <= 0 || length > sizeof(g_aucNvram))
		return -EFAULT;

	if (copy_from_user(g_aucNvram, buf, length)) {
		DBGLOG(INIT, ERROR, "copy nvram fail\n");
		fgNvramAvailable = FALSE;
		return -EINVAL;
	}

	fgNvramAvailable = TRUE;
	return 0;
}

static void createWirelessDevice(void)
{
	struct wiphy *prWiphy = NULL;
	struct wireless_dev *prWdev = NULL;
#if CFG_SUPPORT_PERSIST_NETDEV
	struct net_device *prNetDev = NULL;
#endif

	/* <1.1> Create wireless_dev */
	prWdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!prWdev) {
		DBGLOG(INIT, ERROR, "Allocating memory to wireless_dev context failed\n");
		return;
	}


	/* <1.2> Create wiphy */
	prWiphy = wiphy_new(&mtk_wlan_ops, sizeof(GLUE_INFO_T));
	if (!prWiphy) {
		DBGLOG(INIT, ERROR, "Allocating memory to wiphy device failed\n");
		goto free_wdev;
	}

	/* <1.3> configure wireless_dev & wiphy */
	prWdev->iftype = NL80211_IFTYPE_STATION;
#if CFG_MULTI_SSID_SCAN
	prWiphy->max_scan_ssids   = SCN_SSID_MAX_NUM + 1;
#else
	prWiphy->max_scan_ssids   = 1;
#endif
	prWiphy->max_scan_ie_len = 512;
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	prWiphy->max_sched_scan_ssids	  = CFG_SCAN_HIDDEN_SSID_MAX_NUM;
#else
	prWiphy->max_sched_scan_ssids	  = CFG_SCAN_SSID_MAX_NUM;
#endif
	prWiphy->max_match_sets		= CFG_SCAN_SSID_MATCH_MAX_NUM;
	prWiphy->max_sched_scan_ie_len	= CFG_CFG80211_IE_BUF_LEN;

	/* prWiphy->interface_modes	= BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_ADHOC); */
	prWiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_ADHOC) |
					BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO);

	prWiphy->iface_combinations = p_mtk_sta_iface_combos;
	prWiphy->n_iface_combinations = mtk_sta_iface_combos_num;

	prWiphy->bands[IEEE80211_BAND_2GHZ] = &mtk_band_2ghz;
	/*
	 * always assign 5Ghz bands here, if the chip is not support 5Ghz,
	 * bands[IEEE80211_BAND_5GHZ] will be assign to NULL
	 */
	prWiphy->bands[IEEE80211_BAND_5GHZ] = &mtk_band_5ghz;
	prWiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	prWiphy->cipher_suites = mtk_cipher_suites;
	prWiphy->n_cipher_suites = ARRAY_SIZE(mtk_cipher_suites);
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	prWiphy->flags = WIPHY_FLAG_SUPPORTS_FW_ROAM
			| WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	/* In kernel 4.12 or newer,
	 * this is obsoletes - WIPHY_FLAG_SUPPORTS_SCHED_SCAN
	 */
	prWiphy->max_sched_scan_reqs = 1;
#else
	prWiphy->flags = WIPHY_FLAG_SUPPORTS_FW_ROAM
			| WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL
			| WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
#endif
#if CFG_SUPPORT_AAA && CFG_SUPPORT_WPA3
	prWiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
#endif
	prWiphy->regulatory_flags = REGULATORY_CUSTOM_REG;
#if CFG_SUPPORT_TDLS
	TDLSEX_WIPHY_FLAGS_INIT(prWiphy->flags);
#endif /* CFG_SUPPORT_TDLS */
	prWiphy->max_remain_on_channel_duration = 500;
	prWiphy->mgmt_stypes = mtk_cfg80211_ais_default_mgmt_stypes;
#if CFG_SUPPORT_WPA3
	prWiphy->features |= NL80211_FEATURE_SAE;
#endif

#if (CFG_SUPPORT_SCAN_RANDOM_MAC && \
	(KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE))
	prWiphy->features |= NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;
	prWiphy->features |= NL80211_FEATURE_SCHED_SCAN_RANDOM_MAC_ADDR;
#endif

	prWiphy->vendor_commands = mtk_wlan_vendor_ops;
	prWiphy->n_vendor_commands = sizeof(mtk_wlan_vendor_ops) / sizeof(struct wiphy_vendor_command);
	prWiphy->vendor_events = mtk_wlan_vendor_events;
	prWiphy->n_vendor_events = ARRAY_SIZE(mtk_wlan_vendor_events);

	/* <1.4> wowlan support */
#ifdef CONFIG_PM
	prWiphy->wowlan = &mtk_wlan_wowlan_support;
#endif
#ifdef CONFIG_CFG80211_WEXT
	 /* <1.5> Use wireless extension to replace IOCTL */
	prWiphy->wext = &wext_handler_def;
#endif

	if (wiphy_register(prWiphy) < 0) {
		DBGLOG(INIT, ERROR, "wiphy_register error\n");
		goto free_wiphy;
	}
	prWdev->wiphy = prWiphy;

#if CFG_SUPPORT_PERSIST_NETDEV
	/* <2> allocate and register net_device */
	prNetDev = alloc_netdev_mq(sizeof(P_GLUE_INFO_T), NIC_INF_NAME, NET_NAME_PREDICTABLE,
				   ether_setup, CFG_MAX_TXQ_NUM);
	if (!prNetDev) {
		DBGLOG(INIT, ERROR, "Allocating memory to net_device context failed\n");
		goto unregister_wiphy;
	}

	*((P_GLUE_INFO_T *) netdev_priv(prNetDev)) = (P_GLUE_INFO_T) wiphy_priv(prWiphy);

	prNetDev->netdev_ops = &wlan_netdev_ops;
#ifdef CONFIG_WIRELESS_EXT
	prNetDev->wireless_handlers = &wext_handler_def;
#endif
	netif_carrier_off(prNetDev);
	netif_tx_stop_all_queues(prNetDev);

	/* <2.1> co-relate with wireless_dev bi-directionally */
	prNetDev->ieee80211_ptr = prWdev;
	prWdev->netdev = prNetDev;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	prNetDev->features = NETIF_F_HW_CSUM;
#endif

	/* <2.2> co-relate net device & device tree */
	SET_NETDEV_DEV(prNetDev, wiphy_dev(prWiphy));

	/* <2.3> register net_device */
	if (register_netdev(prWdev->netdev) < 0) {
		DBGLOG(INIT, ERROR, "Register net_device failed\n");
		goto unregister_wiphy;
	}
#endif /* CFG_SUPPORT_PERSIST_NETDEV */

	register_file_buf_handler(wlanNvramBufHandler, (PVOID)NULL, ENUM_BUF_TYPE_NVRAM);
	gprWdev = prWdev;
	DBGLOG(INIT, INFO, "Create wireless device success\n");
	return;

#if CFG_SUPPORT_PERSIST_NETDEV
unregister_wiphy:
	wiphy_unregister(prWiphy);
#endif
free_wiphy:
	wiphy_free(prWiphy);
free_wdev:
	kfree(prWdev);
}

static void destroyWirelessDevice(void)
{
#if CFG_SUPPORT_PERSIST_NETDEV
	unregister_netdev(gprWdev->netdev);
	free_netdev(gprWdev->netdev);
#endif
	wiphy_unregister(gprWdev->wiphy);
	wiphy_free(gprWdev->wiphy);
	kfree(gprWdev);
	gprWdev = NULL;
}

static void wlanSetMulticastList(struct net_device *prDev)
{
	gPrDev = prDev;
	schedule_delayed_work(&workq, 0);
}

/*
 * FIXME: Since we cannot sleep in the wlanSetMulticastList, we arrange
 * another workqueue for sleeping. We don't want to block
 * tx_thread, so we can't let tx_thread to do this
 */

static void wlanSetMulticastListWorkQueue(struct work_struct *work)
{

	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4PacketFilter = 0;
	UINT_32 u4SetInfoLen;
	struct net_device *prDev = gPrDev;

	fgIsWorkMcStart = TRUE;

	DBGLOG(INIT, TRACE, "wlanSetMulticastListWorkQueue\n");

	if (kalHaltLock(KAL_HALT_LOCK_TIMEOUT_NORMAL_CASE))
		return;
	if (kalIsHalted()) {
		fgIsWorkMcStart = FALSE;
		kalHaltUnlock();
		return;
	}

	prGlueInfo = (prDev != NULL) ? *((P_GLUE_INFO_T *) netdev_priv(prDev)) : NULL;
	ASSERT(prDev);
	ASSERT(prGlueInfo);
	if (!prDev || !prGlueInfo) {
		DBGLOG(INIT, WARN, "abnormal dev or skb: prDev(0x%p), prGlueInfo(0x%p)\n", prDev, prGlueInfo);
		fgIsWorkMcStart = FALSE;
		kalHaltUnlock();
		return;
	}

	if (prDev->flags & IFF_PROMISC)
		u4PacketFilter |= PARAM_PACKET_FILTER_PROMISCUOUS;

	if (prDev->flags & IFF_BROADCAST)
		u4PacketFilter |= PARAM_PACKET_FILTER_BROADCAST;

	if (prDev->flags & IFF_MULTICAST) {
		if ((prDev->flags & IFF_ALLMULTI) ||
		    (netdev_mc_count(prDev) > MAX_NUM_GROUP_ADDR)) {

			u4PacketFilter |= PARAM_PACKET_FILTER_ALL_MULTICAST;
		} else {
			u4PacketFilter |= PARAM_PACKET_FILTER_MULTICAST;
		}
	}

	kalHaltUnlock();

	if (kalIoctl(prGlueInfo,
		     wlanoidSetCurrentPacketFilter,
		     &u4PacketFilter,
		     sizeof(u4PacketFilter), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen) != WLAN_STATUS_SUCCESS) {
		fgIsWorkMcStart = FALSE;
		DBGLOG(INIT, ERROR, "wlanSetMulticastListWorkQueue kalIoctl u4PacketFilter=%d\n", u4PacketFilter);
		return;
	}

	if (u4PacketFilter & PARAM_PACKET_FILTER_MULTICAST) {
		/* Prepare multicast address list */
		struct netdev_hw_addr *ha;
		PUINT_8 prMCAddrList = NULL;
		UINT_32 i = 0;

		if (kalHaltLock(KAL_HALT_LOCK_TIMEOUT_NORMAL_CASE))
			return;
		if (kalIsHalted()) {
			fgIsWorkMcStart = FALSE;
			kalHaltUnlock();
			/*DBGLOG(INIT, WARN, "wlanSetMulticastListWorkQueue g_u4HaltFlag=%d\n", g_u4HaltFlag);*/
			return;
		}

		prMCAddrList = kalMemAlloc(MAX_NUM_GROUP_ADDR * ETH_ALEN, VIR_MEM_TYPE);

		/* Avoid race condition with kernel net subsystem */
		netif_addr_lock_bh(prDev);

		netdev_for_each_mc_addr(ha, prDev) {
			if ((i < MAX_NUM_GROUP_ADDR) && (ha != NULL)) {
				memcpy((prMCAddrList + i * ETH_ALEN), ha->addr, ETH_ALEN);
				i++;
			}
		}

		netif_addr_unlock_bh(prDev);

		kalHaltUnlock();

		kalIoctl(prGlueInfo,
			 wlanoidSetMulticastList,
			 prMCAddrList, (i * ETH_ALEN), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		kalMemFree(prMCAddrList, VIR_MEM_TYPE, MAX_NUM_GROUP_ADDR * ETH_ALEN);
	}

	fgIsWorkMcStart = FALSE;

}				/* end of wlanSetMulticastList() */

/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate scheduled scan has been stopped
*
* \param[in]
*           prGlueInfo
*
* \return
*           None
*/
/*----------------------------------------------------------------------------*/
VOID wlanSchedScanStoppedWorkQueue(struct work_struct *work)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct net_device *prDev = gPrDev;

	prGlueInfo = (prDev != NULL) ? *((P_GLUE_INFO_T *) netdev_priv(prDev)) : NULL;
	if (!prGlueInfo) {
		DBGLOG(SCN, ERROR, "prGlueInfo == NULL unexpected\n");
		return;
	}

	/* 2. indication to cfg80211 */
	/* 20150205 change cfg80211_sched_scan_stopped to work queue due to sched_scan_mtx dead lock issue */
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	cfg80211_sched_scan_stopped(priv_to_wiphy(prGlueInfo), 0);
#else
	cfg80211_sched_scan_stopped(priv_to_wiphy(prGlueInfo));
#endif
	DBGLOG(SCN, INFO,
		"cfg80211_sched_scan_stopped event send done\n");

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is TX entry point of NET DEVICE.
*
* \param[in] prSkb  Pointer of the sk_buff to be sent
* \param[in] prDev  Pointer to struct net_device
*
* \retval NETDEV_TX_OK - on success.
* \retval NETDEV_TX_BUSY - on failure, packet will be discarded by upper layer.
*/
/*----------------------------------------------------------------------------*/
#define STATS_ENV_TX_DETECT_TIMEOUT_SEC 20

int wlanHardStartXmit(struct sk_buff *prSkb, struct net_device *prDev)
{
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));

	P_QUE_ENTRY_T prQueueEntry = NULL;
	P_QUE_T prTxQueue = NULL;
	UINT_16 u2QueueIdx = 0;
	OS_SYSTIME rCurTime;
	UINT_32 u4NumPending;
	UINT_32 u4SkbLen = 0;
#if (CFG_SUPPORT_TDLS_DBG == 1)
	UINT16 u2Identifier = 0;
#endif

#if CFG_BOW_TEST
	UINT_32 i;
#endif

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prSkb);
	ASSERT(prDev);
	ASSERT(prGlueInfo);
	prGlueInfo->u8SkbToDriver++;

#if (CFG_SUPPORT_TDLS_DBG == 1)
	{
		UINT8 *pkt = prSkb->data;

		if ((*(pkt + 12) == 0x08) && (*(pkt + 13) == 0x00)) {
			/* ip */
			u2Identifier = ((*(pkt + 18)) << 8) | (*(pkt + 19));
			/* u2TdlsTxSeq[u4TdlsTxSeqId ++] = u2Identifier; */
			DBGLOG(INIT, INFO, "<s> %d\n", u2Identifier);
		}
	}
#endif
	/* check if WiFi is halt */
	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(INIT, INFO, "GLUE_FLAG_HALT skip tx\n");
		dev_kfree_skb(prSkb);
		prGlueInfo->u8SkbFreed++;
		return NETDEV_TX_OK;
	}
#if CFG_SUPPORT_HOTSPOT_2_0
	if (prGlueInfo->fgIsDad) {
		/* kalPrint("[Passpoint R2] Due to ipv4_dad...TX is forbidden\n"); */
		dev_kfree_skb(prSkb);
		prGlueInfo->u8SkbFreed++;
		return NETDEV_TX_OK;
	}
	if (prGlueInfo->fgIs6Dad) {
		/* kalPrint("[Passpoint R2] Due to ipv6_dad...TX is forbidden\n"); */
		dev_kfree_skb(prSkb);
		prGlueInfo->u8SkbFreed++;
		return NETDEV_TX_OK;
	}
#endif

	STATS_TX_TIME_ARRIVE(prSkb);
	prQueueEntry = (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prSkb);
	prTxQueue = &prGlueInfo->rTxQueue;

#if CFG_BOW_TEST
	DBGLOG(BOW, TRACE, "sk_buff->len: %d\n", prSkb->len);
	DBGLOG(BOW, TRACE, "sk_buff->data_len: %d\n", prSkb->data_len);
	DBGLOG(BOW, TRACE, "sk_buff->data:\n");

	for (i = 0; i < prSkb->len; i++) {
		DBGLOG(BOW, TRACE, "%4x", prSkb->data[i]);

		if ((i + 1) % 16 == 0)
			DBGLOG(BOW, TRACE, "\n");
	}

	DBGLOG(BOW, TRACE, "\n");
#endif

	u4SkbLen = prSkb->len;

	if (wlanProcessSecurityFrame(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb) == FALSE) {

		/* non-1x packets */

#if CFG_DBG_GPIO_PINS
		{
			/* TX request from OS */
			mtk_wcn_stp_debug_gpio_assert(IDX_TX_REQ, DBG_TIE_LOW);
			kalUdelay(1);
			mtk_wcn_stp_debug_gpio_assert(IDX_TX_REQ, DBG_TIE_HIGH);
		}
#endif

		u2QueueIdx = skb_get_queue_mapping(prSkb);
		ASSERT(u2QueueIdx < CFG_MAX_TXQ_NUM);

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
		GLUE_SET_PKT_ARRIVAL_TIME(prSkb, kalGetTimeTick());
#endif
		wlanFillTimestamp(prGlueInfo->prAdapter, (PVOID)prSkb, PHASE_XMIT_RCV);
		GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
		if (u2QueueIdx < CFG_MAX_TXQ_NUM)
			GLUE_INC_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][u2QueueIdx]);

		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
		QUEUE_INSERT_TAIL(prTxQueue, prQueueEntry);
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
/* GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingFrameNum); */
/* GLUE_INC_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][u2QueueIdx]); */

		if (u2QueueIdx < CFG_MAX_TXQ_NUM) {
			if (prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][u2QueueIdx] >=
			    CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD) {
				DBGLOG(TX, INFO, "netif_stop_subqueue for wlan0, Queue len: %d\n",
					prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][u2QueueIdx]);

				netif_stop_subqueue(prDev, u2QueueIdx);

#if (CONF_HIF_LOOPBACK_AUTO == 1)
			prGlueInfo->rHifInfo.HifLoopbkFlg |= 0x01;
#endif /* CONF_HIF_LOOPBACK_AUTO */
			}
		}
	} else {
		/* printk("is security frame\n"); */

		GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
	}

	DBGLOG(TX, EVENT, "\n+++++ pending frame %d len = %d +++++\n", prGlueInfo->i4TxPendingFrameNum, u4SkbLen);
	prGlueInfo->rNetDevStats.tx_bytes += u4SkbLen;
	prGlueInfo->rNetDevStats.tx_packets++;
	if (netif_carrier_ok(prDev))
		kalPerMonStart(prGlueInfo);

	/* set GLUE_FLAG_TXREQ_BIT */

	/* pr->u4Flag |= GLUE_FLAG_TXREQ; */
	/* wake_up_interruptible(&prGlueInfo->waitq); */
	kalSetEvent(prGlueInfo);

	/* Maybe the Packet is pending on queue and on TC resource */
	u4NumPending = prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][0]
		+ prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][1]
		+ prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][2]
		+ prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][3];

	GET_CURRENT_SYSTIME(&rCurTime);
	if (CHECK_FOR_TIMEOUT(rCurTime, prGlueInfo->prAdapter->rStasEnvReportDetectTime,
			  SEC_TO_SYSTIME(STATS_ENV_TX_DETECT_TIMEOUT_SEC)) && u4NumPending > 0) {
		DBGLOG(TX, WARN, "%s rCurTime=%u PendingPKT Num(%u %u %u %u)TC4 resource (%d %d %d %d %d)\n",
				__func__, rCurTime,
				prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][0],
				prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][1],
				prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][2],
				prGlueInfo->ai4TxPendingFrameNumPerQueue[NETWORK_TYPE_AIS_INDEX][3],
				prGlueInfo->prAdapter->rTxCtrl.rTc.aucFreeBufferCount[TC0_INDEX],
				prGlueInfo->prAdapter->rTxCtrl.rTc.aucFreeBufferCount[TC1_INDEX],
				prGlueInfo->prAdapter->rTxCtrl.rTc.aucFreeBufferCount[TC2_INDEX],
				prGlueInfo->prAdapter->rTxCtrl.rTc.aucFreeBufferCount[TC3_INDEX],
				prGlueInfo->prAdapter->rTxCtrl.rTc.aucFreeBufferCount[TC4_INDEX]);
	}

	/* For Linux, we'll always return OK FLAG, because we'll free this skb by ourself */
	return NETDEV_TX_OK;
}				/* end of wlanHardStartXmit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A method of struct net_device, to get the network interface statistical
*        information.
*
* Whenever an application needs to get statistics for the interface, this method
* is called. This happens, for example, when ifconfig or netstat -i is run.
*
* \param[in] prDev      Pointer to struct net_device.
*
* \return net_device_stats buffer pointer.
*/
/*----------------------------------------------------------------------------*/
struct net_device_stats *wlanGetStats(IN struct net_device *prDev)
{
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));

#if 0
	WLAN_STATUS rStatus;
	UINT_32 u4XmitError = 0;
	UINT_32 u4XmitOk = 0;
	UINT_32 u4RecvError = 0;
	UINT_32 u4RecvOk = 0;
	UINT_32 u4BufLen;

	ASSERT(prDev);

	/* @FIX ME: need a more clear way to do this */

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryXmitError, &u4XmitError, sizeof(UINT_32), TRUE, TRUE, TRUE, &u4BufLen);

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryXmitOk, &u4XmitOk, sizeof(UINT_32), TRUE, TRUE, TRUE, &u4BufLen);
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRcvOk, &u4RecvOk, sizeof(UINT_32), TRUE, TRUE, TRUE, &u4BufLen);
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryRcvError, &u4RecvError, sizeof(UINT_32), TRUE, TRUE, TRUE, &u4BufLen);
	prGlueInfo->rNetDevStats.rx_packets = u4RecvOk;
	prGlueInfo->rNetDevStats.tx_packets = u4XmitOk;
	prGlueInfo->rNetDevStats.tx_errors = u4XmitError;
	prGlueInfo->rNetDevStats.rx_errors = u4RecvError;
	/* prGlueInfo->rNetDevStats.rx_bytes   = rCustomNetDevStats.u4RxBytes; */
	/* prGlueInfo->rNetDevStats.tx_bytes   = rCustomNetDevStats.u4TxBytes; */
	/* prGlueInfo->rNetDevStats.rx_errors  = rCustomNetDevStats.u4RxErrors; */
	/* prGlueInfo->rNetDevStats.multicast  = rCustomNetDevStats.u4Multicast; */
#endif
	/* prGlueInfo->rNetDevStats.rx_packets = 0; */
	/* prGlueInfo->rNetDevStats.tx_packets = 0; */
	prGlueInfo->rNetDevStats.tx_errors = 0;
	prGlueInfo->rNetDevStats.rx_errors = 0;
	/* prGlueInfo->rNetDevStats.rx_bytes   = 0; */
	/* prGlueInfo->rNetDevStats.tx_bytes   = 0; */
	prGlueInfo->rNetDevStats.rx_errors = 0;
	prGlueInfo->rNetDevStats.multicast = 0;

	return &prGlueInfo->rNetDevStats;

}				/* end of wlanGetStats() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A function for prDev->init
*
* \param[in] prDev      Pointer to struct net_device.
*
* \retval 0         The execution of wlanInit succeeds.
* \retval -ENXIO    No such device.
*/
/*----------------------------------------------------------------------------*/
static int wlanInit(struct net_device *prDev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	if (fgIsWorkMcEverInit == FALSE) {
		if (!prDev)
			return -ENXIO;

		prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
		INIT_DELAYED_WORK(&workq, wlanSetMulticastListWorkQueue);

		/* 20150205 work queue for sched_scan */
		INIT_DELAYED_WORK(&sched_workq, wlanSchedScanStoppedWorkQueue);

		fgIsWorkMcEverInit = TRUE;
	}

	return 0;		/* success */
}				/* end of wlanInit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A function for prDev->uninit
*
* \param[in] prDev      Pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static void wlanUninit(struct net_device *prDev)
{

}				/* end of wlanUninit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A function for prDev->open
*
* \param[in] prDev      Pointer to struct net_device.
*
* \retval 0     The execution of wlanOpen succeeds.
* \retval < 0   The execution of wlanOpen failed.
*/
/*----------------------------------------------------------------------------*/
static int wlanOpen(struct net_device *prDev)
{
	ASSERT(prDev);

	netif_tx_start_all_queues(prDev);

	return 0;		/* success */
}				/* end of wlanOpen() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A function for prDev->stop
*
* \param[in] prDev      Pointer to struct net_device.
*
* \retval 0     The execution of wlanStop succeeds.
* \retval < 0   The execution of wlanStop failed.
*/
/*----------------------------------------------------------------------------*/
static int wlanStop(struct net_device *prDev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));

	/* CFG80211 down */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prGlueInfo->prScanRequest) {
		kalCfg80211ScanDone(prGlueInfo->prScanRequest, TRUE);
		prGlueInfo->prScanRequest = NULL;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	netif_tx_stop_all_queues(prDev);

	return 0;		/* success */
}				/* end of wlanStop() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update channel table for cfg80211 based on current country domain
 *
 * \param[in] prGlueInfo      Pointer to glue info
 *
 * \return   none
 */
/*----------------------------------------------------------------------------*/
VOID wlanUpdateChannelTable(P_GLUE_INFO_T prGlueInfo)
{
	UINT_8 i, j;
	UINT_8 ucNumOfChannel;
	RF_CHANNEL_INFO_T aucChannelList[ARRAY_SIZE(mtk_2ghz_channels) + ARRAY_SIZE(mtk_5ghz_channels)];

	/* 1. Disable all channels */
	for (i = 0; i < ARRAY_SIZE(mtk_2ghz_channels); i++) {
		mtk_2ghz_channels[i].flags |= IEEE80211_CHAN_DISABLED;
		mtk_2ghz_channels[i].orig_flags |= IEEE80211_CHAN_DISABLED;
	}

	for (i = 0; i < ARRAY_SIZE(mtk_5ghz_channels); i++) {
		mtk_5ghz_channels[i].flags |= IEEE80211_CHAN_DISABLED;
		mtk_5ghz_channels[i].orig_flags |= IEEE80211_CHAN_DISABLED;
	}

	/* 2. Get current domain channel list */
	rlmDomainGetChnlList(prGlueInfo->prAdapter,
			     BAND_NULL, FALSE,
			     ARRAY_SIZE(mtk_2ghz_channels) + ARRAY_SIZE(mtk_5ghz_channels),
			     &ucNumOfChannel, aucChannelList);

	/* 3. Enable specific channel based on domain channel list */
	for (i = 0; i < ucNumOfChannel; i++) {
		switch (aucChannelList[i].eBand) {
		case BAND_2G4:
			for (j = 0; j < ARRAY_SIZE(mtk_2ghz_channels); j++) {
				if (mtk_2ghz_channels[j].hw_value == aucChannelList[i].ucChannelNum) {
					mtk_2ghz_channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
					mtk_2ghz_channels[j].orig_flags &= ~IEEE80211_CHAN_DISABLED;
					break;
				}
			}
			break;

		case BAND_5G:
			for (j = 0; j < ARRAY_SIZE(mtk_5ghz_channels); j++) {
				if (mtk_5ghz_channels[j].hw_value == aucChannelList[i].ucChannelNum) {
					mtk_5ghz_channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
					mtk_5ghz_channels[j].orig_flags &= ~IEEE80211_CHAN_DISABLED;
					mtk_5ghz_channels[j].dfs_state =
					    (aucChannelList[i].eDFS) ?
					     NL80211_DFS_USABLE :
					     NL80211_DFS_UNAVAILABLE;
					break;
				}
			}
			break;

		default:
			DBGLOG(INIT, WARN, "Unknown band %d\n", aucChannelList[i].eBand);
			break;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Register the device to the kernel and return the index.
*
* \param[in] prDev      Pointer to struct net_device.
*
* \retval 0     The execution of wlanNetRegister succeeds.
* \retval < 0   The execution of wlanNetRegister failed.
*/
/*----------------------------------------------------------------------------*/
static INT_32 wlanNetRegister(struct wireless_dev *prWdev)
{
	P_GLUE_INFO_T prGlueInfo;
	INT_32 i4DevIdx = -1;
	int ret = 0;

	ASSERT(prWdev);

	do {
		if (!prWdev)
			break;

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(prWdev->wiphy);
		i4DevIdx = wlanGetDevIdx(prWdev->netdev);
		if (i4DevIdx < 0) {
			DBGLOG(INIT, ERROR, "net_device number exceed!\n");
			break;
		}

#if !CFG_SUPPORT_PERSIST_NETDEV
		ret = register_netdev(prWdev->netdev);
		if (ret < 0) {
			DBGLOG(INIT, ERROR, "Register net_device failed, ret = %d\n", ret);

			wlanClearDevIdx(prWdev->netdev);
			i4DevIdx = -1;
		}
#endif
		if (i4DevIdx != -1)
			prGlueInfo->fgIsRegistered = TRUE;

	} while (FALSE);

	return i4DevIdx;	/* success */
}				/* end of wlanNetRegister() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief A method of struct net_device, to set the randomized mac address
 *
 * This method is called before Wifi Framework requests a new conenction with
 * enabled feature "Connected Random Mac".
 *
 * \param[in] ndev	Pointer to struct net_device.
 * \param[in] addr	Randomized Mac address passed from WIFI framework.
 *
 * \return int.
 */
/*----------------------------------------------------------------------------*/
static int wlanSetMacAddress(struct net_device *ndev, void *addr)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prAisBssInfo = NULL;
	struct sockaddr *sa = NULL;

	/**********************************************************************
	 * Check if kernel passes valid data to us                            *
	 **********************************************************************
	 */
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(ndev));
	if (!ndev || !addr || !prGlueInfo) {
		DBGLOG(INIT, ERROR,
		       "Set macaddr with ndev(%d), glue(%d) and addr(%d)\n",
		       (ndev == NULL) ? 0 : 1, (prGlueInfo == NULL) ? 0 : 1,
		       (addr == NULL) ? 0 : 1);
		return WLAN_STATUS_INVALID_DATA;
	}
	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter) {
		DBGLOG(INIT, ERROR, "Invalid prAdapter\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	if (!prAdapter) {
		DBGLOG(INIT, ERROR, "Invalid prAisBssInfo\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	/**********************************************************************
	 * 1. Change OwnMacAddr which will be updated to FW through           *
	 *    rlmActivateNetwork later.                                       *
	 * 2. Change dev_addr stored in kernel to notify framework that the   *
	 *    mac addr has been changed and what the new value is.            *
	 **********************************************************************
	 */
	sa = (struct sockaddr *)addr;

	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr, sa->sa_data);
	COPY_MAC_ADDR(prGlueInfo->prDevHandler->dev_addr, sa->sa_data);
	DBGLOG(INIT, INFO, "Set connect random macaddr to " MACSTR ".\n",
	       MAC2STR(prAisBssInfo->aucOwnMacAddr));

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanSetMacAddr() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Unregister the device from the kernel
*
* \param[in] prWdev      Pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID wlanNetUnregister(struct wireless_dev *prWdev)
{
	P_GLUE_INFO_T prGlueInfo;

	if (!prWdev) {
		DBGLOG(INIT, ERROR, "wlanNetUnregister: The device context is NULL\n");
		return;
	}
	DBGLOG(INIT, TRACE, "unregister net_dev(0x%p)\n", prWdev->netdev);
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(prWdev->wiphy);
	wlanClearDevIdx(prWdev->netdev);
#if !CFG_SUPPORT_PERSIST_NETDEV
	unregister_netdev(prWdev->netdev);
#endif
	prGlueInfo->fgIsRegistered = FALSE;

	DBGLOG(INIT, INFO, "unregister wireless_dev(0x%p), ifindex=%d\n", prWdev, prWdev->netdev->ifindex);

}				/* end of wlanNetUnregister() */

static const struct net_device_ops wlan_netdev_ops = {
	.ndo_open = wlanOpen,
	.ndo_stop = wlanStop,
	.ndo_set_rx_mode = wlanSetMulticastList,
	.ndo_get_stats = wlanGetStats,
	.ndo_do_ioctl = wlanDoIOCTL,
	.ndo_start_xmit = wlanHardStartXmit,
	.ndo_init = wlanInit,
	.ndo_uninit = wlanUninit,
	.ndo_select_queue = wlanSelectQueue,
	.ndo_set_mac_address = wlanSetMacAddress,
};

/*----------------------------------------------------------------------------*/
/*!
* \brief A method for creating Linux NET4 struct net_device object and the
*        private data(prGlueInfo and prAdapter). Setup the IO address to the HIF.
*        Assign the function pointer to the net_device object
*
* \param[in] pvData     Memory address for the device
*
* \retval Not null      The wireless_dev object.
* \retval NULL          Fail to create wireless_dev object
*/
/*----------------------------------------------------------------------------*/
static struct lock_class_key rSpinKey[SPIN_LOCK_NUM];
static struct wireless_dev *wlanNetCreate(PVOID pvData)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct wireless_dev *prWdev = gprWdev;
	UINT_32 i;
	struct device *prDev;

	if (!prWdev) {
		DBGLOG(INIT, ERROR, "Allocating memory to wireless_dev context failed\n");
		return NULL;
	}
	/* 4 <1> co-relate wiphy & prDev */
#if MTK_WCN_HIF_SDIO
	mtk_wcn_hif_sdio_get_dev(*((MTK_WCN_HIF_SDIO_CLTCTX *) pvData), &prDev);
#else
	/* prDev = &((struct sdio_func *) pvData)->dev; //samp */
	prDev = pvData;		/* samp */
#endif
	if (!prDev)
		DBGLOG(INIT, WARN, "unable to get struct dev for wlan\n");
	/*
	 * don't set prDev as parent of wiphy->dev, because we have done device_add
	 * in driver init. if we set parent here, parent will be not able to know this child,
	 * and may occurs a KE in device_shutdown, to free wiphy->dev, because his parent
	 * has been freed.
	 */
	/*set_wiphy_dev(prWdev->wiphy, prDev);*/

#if !CFG_SUPPORT_PERSIST_NETDEV
	/* 4 <3> Initialize Glue structure */
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(prWdev->wiphy);
	kalMemZero(prGlueInfo, sizeof(GLUE_INFO_T));
	/* 4 <3.1> Create net device */
	prGlueInfo->prDevHandler = alloc_netdev_mq(sizeof(P_GLUE_INFO_T), NIC_INF_NAME,
						   NET_NAME_PREDICTABLE, ether_setup, CFG_MAX_TXQ_NUM);

	/* Device can help us to save at most 3000 packets, after we stopped queue */
	if (prGlueInfo->prDevHandler != NULL)
		prGlueInfo->prDevHandler->tx_queue_len = 3000;
	if (!prGlueInfo->prDevHandler) {
		DBGLOG(INIT, ERROR, "Allocating memory to net_device context failed\n");
		return NULL;
	}
	DBGLOG(INIT, INFO, "net_device prDev(0x%p) allocated ifindex=%d\n",
			prGlueInfo->prDevHandler, prGlueInfo->prDevHandler->ifindex);

	/* 4 <3.1.1> Initialize net device variables */
	*((P_GLUE_INFO_T *) netdev_priv(prGlueInfo->prDevHandler)) = prGlueInfo;

	prGlueInfo->prDevHandler->netdev_ops = &wlan_netdev_ops;
#ifdef CONFIG_WIRELESS_EXT
	prGlueInfo->prDevHandler->wireless_handlers = &wext_handler_def;
#endif
	netif_carrier_off(prGlueInfo->prDevHandler);
	netif_tx_stop_all_queues(prGlueInfo->prDevHandler);

	/* 4 <3.1.2> co-relate with wiphy bi-directionally */
	prGlueInfo->prDevHandler->ieee80211_ptr = prWdev;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	prGlueInfo->prDevHandler->features = NETIF_F_HW_CSUM;
#endif
	prWdev->netdev = prGlueInfo->prDevHandler;

	/* 4 <3.1.3> co-relate net device & prDev */
	/*SET_NETDEV_DEV(prGlueInfo->prDevHandler, wiphy_dev(prWdev->wiphy));*/
	SET_NETDEV_DEV(prGlueInfo->prDevHandler, prDev);
#else /* CFG_SUPPORT_PERSIST_NETDEV */
	prGlueInfo->prDevHandler = gprWdev->netdev;
#endif /* CFG_SUPPORT_PERSIST_NETDEV */

	/* 4 <3.2> Initialize Glue variables */
	prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED;
	prGlueInfo->fgIsMacAddrOverride = FALSE;
	prGlueInfo->fgIsRegistered = FALSE;
	prGlueInfo->prScanRequest = NULL;
	prGlueInfo->puScanChannel = NULL;

	/*Full2Partial*/
	/*init update full scan to partial scan varable*/
	prGlueInfo->u4LastFullScanTime = 0;
	prGlueInfo->ucTrScanType = 0;
	kalMemSet(prGlueInfo->ucChannelNum, 0, FULL_SCAN_MAX_CHANNEL_NUM);
	prGlueInfo->puFullScan2PartialChannel = NULL;

#if CFG_SUPPORT_HOTSPOT_2_0
	/* Init DAD */
	prGlueInfo->fgIsDad = FALSE;
	prGlueInfo->fgIs6Dad = FALSE;
	kalMemZero(prGlueInfo->aucDADipv4, 4);
	kalMemZero(prGlueInfo->aucDADipv6, 16);
#endif

	init_completion(&prGlueInfo->rScanComp);
	init_completion(&prGlueInfo->rHaltComp);
	init_completion(&prGlueInfo->rPendComp);
#if CFG_SUPPORT_MULTITHREAD
	init_completion(&prGlueInfo->rRxHaltComp);
#endif
#if CFG_ENABLE_WIFI_DIRECT
	init_completion(&prGlueInfo->rP2pReq);
	init_completion(&prGlueInfo->rSubModComp);
#endif
#if CFG_SUPPORT_NCHO
	init_completion(&prGlueInfo->rAisChGrntComp);
#endif

	/* initialize timer for OID timeout checker */
	kalOsTimerInitialize(prGlueInfo, kalTimeoutHandler);

	for (i = 0; i < SPIN_LOCK_NUM; i++) {
		spin_lock_init(&prGlueInfo->rSpinLock[i]);
		lockdep_set_class(&prGlueInfo->rSpinLock[i], &rSpinKey[i]);
	}

	/* initialize semaphore for ioctl */
	sema_init(&prGlueInfo->ioctl_sem, 1);

	glSetHifInfo(prGlueInfo, (ULONG) pvData);

	/* 4 <8> Init Queues */
	init_waitqueue_head(&prGlueInfo->waitq);
#if CFG_SUPPORT_MULTITHREAD
	init_waitqueue_head(&prGlueInfo->waitq_rx);
#endif
	QUEUE_INITIALIZE(&prGlueInfo->rCmdQueue);
	QUEUE_INITIALIZE(&prGlueInfo->rTxQueue);

	/* 4 <4> Create Adapter structure */
	prGlueInfo->prAdapter = (P_ADAPTER_T) wlanAdapterCreate(prGlueInfo);

	if (!prGlueInfo->prAdapter) {
		DBGLOG(INIT, ERROR, "Allocating memory to adapter failed\n");
		return NULL;
	}
	KAL_WAKE_LOCK_INIT(prAdapter, prGlueInfo->rAhbIsrWakeLock, "WLAN AHB ISR");

#if CFG_SUPPORT_MULTITHREAD
	/*Check wake_lock is available, maybe wake_lock was un-released before using it.*/
	if (KAL_WAKE_LOCK_ACTIVE(prAdapter, &prGlueInfo->rTimeoutWakeLock) != 0)
		DBGLOG(INIT, ERROR, "rTimeoutWakeLock = %d is invalid.\n"
		, KAL_WAKE_LOCK_ACTIVE(prAdapter, &prGlueInfo->rTimeoutWakeLock));

	KAL_WAKE_LOCK_INIT(prAdapter, prGlueInfo->rTimeoutWakeLock, "Wlan Timeout");
	DBGLOG(INIT, INFO, "rTimeoutWakeLock init addr=%p\n", &prGlueInfo->rTimeoutWakeLock);
#endif
#if CFG_SUPPORT_PERSIST_NETDEV
	dev_open(prGlueInfo->prDevHandler);
	netif_carrier_off(prGlueInfo->prDevHandler);
	netif_tx_stop_all_queues(prGlueInfo->prDevHandler);
#endif

	return prWdev;
}				/* end of wlanNetCreate() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Destroying the struct net_device object and the private data.
*
* \param[in] prWdev      Pointer to struct wireless_dev.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID wlanNetDestroy(struct wireless_dev *prWdev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prWdev);

	if (!prWdev) {
		DBGLOG(INIT, ERROR, "wlanNetDestroy: The device context is NULL\n");
		return;
	}

	/* prGlueInfo is allocated with net_device */
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(prWdev->wiphy);
	ASSERT(prGlueInfo);

	/* destroy kal OS timer */
	kalCancelTimer(prGlueInfo);

	glClearHifInfo(prGlueInfo);

	wlanAdapterDestroy(prGlueInfo->prAdapter);
	prGlueInfo->prAdapter = NULL;

#if CFG_SUPPORT_PERSIST_NETDEV
	/* take the net_device to down state */
	dev_close(prGlueInfo->prDevHandler);
#else
	/* Free net_device and private data prGlueInfo, which are allocated by alloc_netdev(). */
	free_netdev(prWdev->netdev);
#endif

	/* gPrDev is assigned by prGlueInfo->prDevHandler,
	 * set NULL to this global variable.
	 */
	gPrDev = NULL;
}				/* end of wlanNetDestroy() */

#ifndef CONFIG_X86
UINT_8 g_aucBufIpAddr[32] = { 0 };
static void wlanNotifyFwSuspend(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgSuspend)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_32 u4SetInfoLen;

	rStatus = kalIoctl(prGlueInfo,
			wlanoidNotifyFwSuspend,
			(PVOID)&fgSuspend,
			sizeof(fgSuspend),
			FALSE,
			FALSE,
			TRUE,
			FALSE,
			&u4SetInfoLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "wlanNotifyFwSuspend fail\n");
}

void wlanHandleSystemSuspend(void)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct net_device *prDev = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_8 ip[4] = { 0 };
	UINT_32 u4NumIPv4 = 0;
#ifdef CONFIG_IPV6
	UINT_8 ip6[16] = { 0 };	/* FIX ME: avoid to allocate large memory in stack */
	UINT_32 u4NumIPv6 = 0;
#endif
	UINT_32 i;
	P_PARAM_NETWORK_ADDRESS_IP prParamIpAddr;
#if CFG_SUPPORT_DROP_MC_PACKET
	UINT_32 u4PacketFilter = 0;
	UINT_32 u4SetInfoLen = 0;
#endif
	DBGLOG(INIT, INFO, "******wlan System Suspend*******.\n");
	/* <1> Sanity check and acquire the net_device */
	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, ERROR, "wlanEarlySuspend u4WlanDevNum==0 invalid!!\n");
		return;
	}
	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;

	fgIsUnderSuspend = true;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);

#if CFG_SUPPORT_DROP_MC_PACKET
	/* new filter should not include p2p mask */
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	u4PacketFilter = prGlueInfo->prAdapter->u4OsPacketFilter & (~PARAM_PACKET_FILTER_P2P_MASK);
#endif
	if (kalIoctl(prGlueInfo,
		 wlanoidSetCurrentPacketFilter,
		 &u4PacketFilter,
		 sizeof(u4PacketFilter), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "set packet filter failed.\n");
	}
#endif
	if (!prDev || !(prDev->ip_ptr) ||
	    !((struct in_device *)(prDev->ip_ptr))->ifa_list ||
	    !(&(((struct in_device *)(prDev->ip_ptr))->ifa_list->ifa_local))) {
		DBGLOG(INIT, INFO, "ip is not available.\n");
		goto notify_suspend;
	}
	kalMemCopy(ip, &(((struct in_device *)(prDev->ip_ptr))->ifa_list->ifa_local), sizeof(ip));

	/* todo: traverse between list to find whole sets of IPv4 addresses */
	if (!((ip[0] == 0) && (ip[1] == 0) && (ip[2] == 0) && (ip[3] == 0)))
		u4NumIPv4++;
#ifdef CONFIG_IPV6
	if (!prDev || !(prDev->ip6_ptr) ||
	    !((struct in_device *)(prDev->ip6_ptr))->ifa_list ||
	    !(&(((struct in_device *)(prDev->ip6_ptr))->ifa_list->ifa_local))) {
		DBGLOG(INIT, INFO, "ipv6 is not available.\n");
		goto notify_suspend;
	}
	kalMemCopy(ip6, &(((struct in_device *)(prDev->ip6_ptr))->ifa_list->ifa_local), sizeof(ip6));
	DBGLOG(INIT, INFO, "ipv6 is %d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d\n",
			    ip6[0], ip6[1], ip6[2], ip6[3],
			    ip6[4], ip6[5], ip6[6], ip6[7],
			    ip6[8], ip6[9], ip6[10], ip6[11], ip6[12], ip6[13], ip6[14], ip6[15]
	       );

	/* todo: traverse between list to find whole sets of IPv6 addresses */
	if (!((ip6[0] == 0) && (ip6[1] == 0) && (ip6[2] == 0) && (ip6[3] == 0) && (ip6[4] == 0) && (ip6[5] == 0))) {
		/* Do nothing */
		/* u4NumIPv6++; */
	}
#endif

	/* <7> set up the ARP filter */
	{
		UINT_32 u4SetInfoLen = 0;
		UINT_32 u4Len = OFFSET_OF(PARAM_NETWORK_ADDRESS_LIST, arAddress);
		P_PARAM_NETWORK_ADDRESS_LIST prParamNetAddrList = (P_PARAM_NETWORK_ADDRESS_LIST) g_aucBufIpAddr;
		P_PARAM_NETWORK_ADDRESS prParamNetAddr = prParamNetAddrList->arAddress;

		kalMemZero(g_aucBufIpAddr, sizeof(g_aucBufIpAddr));

		prParamNetAddrList->u4AddressCount = u4NumIPv4;
#ifdef CONFIG_IPV6
		prParamNetAddrList->u4AddressCount += u4NumIPv6;
#endif
		prParamNetAddrList->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
		for (i = 0; i < u4NumIPv4; i++) {
			prParamNetAddr->u2AddressLength = sizeof(PARAM_NETWORK_ADDRESS_IP);	/* 4;; */
			prParamNetAddr->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
			prParamIpAddr = (P_PARAM_NETWORK_ADDRESS_IP) prParamNetAddr->aucAddress;
			kalMemCopy(&prParamIpAddr->in_addr, ip, sizeof(ip));
			prParamNetAddr =
			    (P_PARAM_NETWORK_ADDRESS) ((ULONG) prParamNetAddr + sizeof(PARAM_NETWORK_ADDRESS));
			u4Len += OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress) + sizeof(PARAM_NETWORK_ADDRESS);
		}
/* u4NumIPv6 never accumulate
 * #ifdef CONFIG_IPV6
 *		for (i = 0; i < u4NumIPv6; i++) {
 *			prParamNetAddr->u2AddressLength = 6;
 *			prParamNetAddr->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
 *			kalMemCopy(prParamNetAddr->aucAddress, ip6, sizeof(ip6));
 *			prParamNetAddr = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prParamNetAddr + sizeof(ip6));
 *			u4Len += OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress) + sizeof(ip6);
 *		}
 * #endif
 */
		ASSERT(u4Len <= sizeof(g_aucBufIpAddr));

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNetworkAddress,
				   (PVOID) prParamNetAddrList, u4Len, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
	}

notify_suspend:
	DBGLOG(INIT, INFO, "IP: %d.%d.%d.%d, rStatus: %u\n", ip[0], ip[1], ip[2], ip[3], rStatus);
	wlanNotifyFwSuspend(prGlueInfo, TRUE);
}

void wlanHandleSystemResume(void)
{
	struct net_device *prDev = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_8 ip[4] = { 0 };
#ifdef CONFIG_IPV6
	UINT_8 ip6[16] = { 0 };	/* FIX ME: avoid to allocate large memory in stack */
#endif
	EVENT_AIS_BSS_INFO_T rParam;
	UINT_32 u4BufLen = 0;
#if CFG_SUPPORT_DROP_MC_PACKET
	UINT_32 u4PacketFilter = 0;
	UINT_32 u4SetInfoLen = 0;
#endif
	DBGLOG(INIT, INFO, "************wlan System Resume.\n");
	/* <1> Sanity check and acquire the net_device */
	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, ERROR, "wlanLateResume u4WlanDevNum==0 invalid!!\n");
		return;
	}
	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	/* ASSERT(prDev); */

	fgIsUnderSuspend = false;

	if (!prDev) {
		DBGLOG(INIT, INFO, "prDev == NULL!!!\n");
		return;
	}
	/* <3> acquire the prGlueInfo */
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);

#if CFG_SUPPORT_DROP_MC_PACKET
	/* new filter should not include p2p mask */
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	u4PacketFilter = prGlueInfo->prAdapter->u4OsPacketFilter & (~PARAM_PACKET_FILTER_P2P_MASK);
#endif
	if (kalIoctl(prGlueInfo,
		 wlanoidSetCurrentPacketFilter,
		 &u4PacketFilter,
		 sizeof(u4PacketFilter), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "set packet filter failed.\n");
	}
#endif
	/*
	 * We will receive the event in rx, we will check if the status is the same in driver
	 * and FW, if not the same, trigger disconnetion procedure.
	 */

	kalMemZero(&rParam, sizeof(EVENT_AIS_BSS_INFO_T));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryBSSInfo,
			   &rParam, sizeof(EVENT_AIS_BSS_INFO_T), TRUE, TRUE, TRUE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "Query BSSinfo fail 0x%x!!\n", rStatus);

	/* <2> get the IPv4 address */
	if (!(prDev->ip_ptr) ||
	    !((struct in_device *)(prDev->ip_ptr))->ifa_list ||
	    !(&(((struct in_device *)(prDev->ip_ptr))->ifa_list->ifa_local))) {
		DBGLOG(INIT, INFO, "ip is not avalible.\n");

		rStatus = WLAN_STATUS_FAILURE;

		goto notify_resume;
	}
	/* <4> copy the IPv4 address */
	kalMemCopy(ip, &(((struct in_device *)(prDev->ip_ptr))->ifa_list->ifa_local), sizeof(ip));

#ifdef CONFIG_IPV6
	/* <5> get the IPv6 address */
	if (!prDev || !(prDev->ip6_ptr) ||
	    !((struct in_device *)(prDev->ip6_ptr))->ifa_list ||
	    !(&(((struct in_device *)(prDev->ip6_ptr))->ifa_list->ifa_local))) {
		DBGLOG(INIT, INFO, "ipv6 is not avalible.\n");

		rStatus = WLAN_STATUS_FAILURE;

		goto notify_resume;
	}
	/* <6> copy the IPv6 address */
	kalMemCopy(ip6, &(((struct in_device *)(prDev->ip6_ptr))->ifa_list->ifa_local), sizeof(ip6));
	DBGLOG(INIT, INFO, "ipv6 is %d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d\n",
			    ip6[0], ip6[1], ip6[2], ip6[3],
			    ip6[4], ip6[5], ip6[6], ip6[7],
			    ip6[8], ip6[9], ip6[10], ip6[11], ip6[12], ip6[13], ip6[14], ip6[15]
	       );
#endif
	/* <7> clear the ARP filter */
	{
		UINT_32 u4SetInfoLen = 0;
/* UINT_8 aucBuf[32] = {0}; */
		UINT_32 u4Len = sizeof(PARAM_NETWORK_ADDRESS_LIST);
		P_PARAM_NETWORK_ADDRESS_LIST prParamNetAddrList = (P_PARAM_NETWORK_ADDRESS_LIST) g_aucBufIpAddr;
		/* aucBuf; */

		kalMemZero(g_aucBufIpAddr, sizeof(g_aucBufIpAddr));

		prParamNetAddrList->u4AddressCount = 0;
		prParamNetAddrList->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;

		ASSERT(u4Len <= sizeof(g_aucBufIpAddr /*aucBuf */));
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNetworkAddress,
				   (PVOID) prParamNetAddrList, u4Len, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
	}

notify_resume:
	DBGLOG(INIT, INFO, "Query BSS result: %d %d %d, IP: %d.%d.%d.%d, rStatus: %u\n",
		       rParam.eConnectionState, rParam.eCurrentOPMode, rParam.fgIsNetActive,
		       ip[0], ip[1], ip[2], ip[3], rStatus);

	wlanNotifyFwSuspend(prGlueInfo, FALSE);
}
#endif /* ! CONFIG_X86 */

void reset_p2p_mode(P_GLUE_INFO_T prGlueInfo)
{
	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	if (!prGlueInfo)
		return;

	rSetP2P.u4Enable = 0;
	rSetP2P.u4Mode = 0;

	p2pNetUnregister(prGlueInfo, FALSE);

	rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode, (PVOID)&rSetP2P,
		sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rWlanStatus != WLAN_STATUS_SUCCESS)
		prGlueInfo->prAdapter->fgIsP2PRegistered = FALSE;

	DBGLOG(INIT, INFO,
			"ret = 0x%08x\n", (uint32_t) rWlanStatus);
}

int set_p2p_mode_handler(struct net_device *netdev, PARAM_CUSTOM_P2P_SET_STRUCT_T p2pmode)
{
#if 0
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(netdev));
	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	rSetP2P.u4Enable = p2pmode.u4Enable;
	rSetP2P.u4Mode = p2pmode.u4Mode;

	if (!rSetP2P.u4Enable)
		p2pNetUnregister(prGlueInfo, TRUE);

	rWlanStatus = kalIoctl(prGlueInfo,
			       wlanoidSetP2pMode,
			       (PVOID) &rSetP2P,
			       sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	DBGLOG(INIT, INFO, "ret = %d\n", rWlanStatus);
	if (rSetP2P.u4Enable)
		p2pNetRegister(prGlueInfo, TRUE);

	return 0;

#else

	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(netdev));
	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P;
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	BOOLEAN fgIsP2PEnding;
	UINT_32 u4BufLen = 0;

	GLUE_SPIN_LOCK_DECLARATION();

	if (!prGlueInfo)
		return -1;

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(INIT, ERROR, "adapter is not ready\n");
		return -1;
	}

	DBGLOG(INIT, INFO, "%u %u\n", (UINT_32) p2pmode.u4Enable, (UINT_32) p2pmode.u4Mode);

	/* avoid remove & p2p off command simultaneously */
	GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
	fgIsP2PEnding = g_u4P2PEnding;
	g_u4P2POnOffing = 1;
	GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);

	if (fgIsP2PEnding == 1) {
		/* skip the command if we are removing */
		GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
		g_u4P2POnOffing = 0;
		GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);
		return 0;
	}

	/* Resetting p2p mode if registered to avoid launch KE */
	if (p2pmode.u4Enable
		&& prGlueInfo->prAdapter->fgIsP2PRegistered
		&& !kalIsResetting()) {
		DBGLOG(INIT, WARN, "Resetting p2p mode\n");
		reset_p2p_mode(prGlueInfo);
	}

	rSetP2P.u4Enable = p2pmode.u4Enable;
	rSetP2P.u4Mode = p2pmode.u4Mode;

#if !CFG_SUPPORT_PERSIST_NETDEV
	if ((!rSetP2P.u4Enable) && (fgIsResetting == FALSE))
		p2pNetUnregister(prGlueInfo, TRUE);
#endif
	/* move out to caller to avoid kalIoctrl & suspend/resume deadlock problem ALPS00844864 */
	/*
	 * Scenario:
	 * 1. System enters suspend/resume but not yet enter wlanearlysuspend()
	 * or wlanlateresume();
	 *
	 * 2. System switches to do PRIV_CMD_P2P_MODE and execute kalIoctl()
	 * and get g_halt_sem then do glRegisterEarlySuspend() or
	 * glUnregisterEarlySuspend();
	 *
	 * But system suspend/resume procedure is not yet finished so we
	 * suspend;
	 *
	 * 3. System switches back to do suspend/resume procedure and execute
	 * kalIoctl(). But driver does not yet release g_halt_sem so system
	 * suspend in wlanearlysuspend() or wlanlateresume();
	 *
	 *  ==> deadlock occurs.
	 */

	rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode, (PVOID) &rSetP2P,/* pu4IntBuf[0]is used as input SubCmd */
			       sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
#if !CFG_SUPPORT_PERSIST_NETDEV
	/*
	 * Need to check fgIsP2PRegistered, in case of whole chip reset.
	 * in this case, kalIOCTL return success always,
	 * and prGlueInfo->prP2pInfo may be NULL
	 */
	if ((rSetP2P.u4Enable) && (prGlueInfo->prAdapter->fgIsP2PRegistered) && (fgIsResetting == FALSE))
		p2pNetRegister(prGlueInfo, TRUE);
#endif
	GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
	g_u4P2POnOffing = 0;
	GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);
	return 0;
#endif
}

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH

#define FW_LOG_CMD_ON_OFF        0
#define FW_LOG_CMD_SET_LEVEL     1
static uint32_t u4LogOnOffCache = -1;

static void consys_log_event_notification(int cmd, int value)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	struct net_device *prDev = gPrDev;
#if 0 /* gen2? */
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T rCmdV1Header;
	struct _CMD_FORMAT_V1_T rCmd_v1;
#endif

	DBGLOG(INIT, INFO, "gPrDev=%p, cmd=%d, value=%d\n",
		gPrDev, cmd, value);

	if (kalIsHalted()) { /* power-off */
		u4LogOnOffCache = value;
		DBGLOG(INIT, INFO,
			"Power off return, u4LogOnOffCache=%d\n",
				u4LogOnOffCache);
		return;
	}

	prGlueInfo = (prDev != NULL) ?
		*((P_GLUE_INFO_T *) netdev_priv(prDev)) : NULL;
	DBGLOG(INIT, TRACE, "prGlueInfo=%p\n", prGlueInfo);
	if (!prGlueInfo) {
		u4LogOnOffCache = value;
		DBGLOG(INIT, INFO,
			"prGlueInfo == NULL return, u4LogOnOffCache=%d\n",
				u4LogOnOffCache);
		return;
	}
	prAdapter = prGlueInfo->prAdapter;
	DBGLOG(INIT, TRACE, "prAdapter=%p\n", prAdapter);
	if (!prAdapter) {
		u4LogOnOffCache = value;
		DBGLOG(INIT, INFO,
		"prAdapter == NULL return, u4LogOnOffCache=%d\n", u4LogOnOffCache);
		return;
	}

	if (cmd == FW_LOG_CMD_ON_OFF) {
#if 0 /* gen2? */
		uint8_t onoff[1] = {'0'};
#endif
		DBGLOG(INIT, TRACE, "FW_LOG_CMD_ON_OFF\n");
#if 0 /* gen2? */
		rCmdV1Header.cmdType = CMD_TYPE_SET;
		rCmdV1Header.cmdVersion = CMD_VER_1;
		rCmdV1Header.cmdBufferLen = 0;
		rCmdV1Header.itemNum = 0;

		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		kalMemSet(&rCmd_v1, 0, sizeof(struct _CMD_FORMAT_V1));

		rCmd_v1.itemType = ITEM_TYPE_STR;

		/*send string format to firmware */
		rCmd_v1.itemStringLength = kalStrLen("EnableDbgLog");
		kalMemZero(rCmd_v1.itemString, MAX_CMD_NAME_MAX_LENGTH);
		kalMemCopy(rCmd_v1.itemString, "EnableDbgLog",
			rCmd_v1.itemStringLength);

		if (value == 1) /* other cases, send 'OFF=0' */
			onoff[0] = '1';
		rCmd_v1.itemValueLength = 1;
		kalMemZero(rCmd_v1.itemValue, MAX_CMD_VALUE_MAX_LENGTH);
		kalMemCopy(rCmd_v1.itemValue, &onoff, 1);

		DBGLOG(INIT, WARN, "Send key word (%s) WITH (%s) to firmware\n",
				rCmd_v1.itemString, rCmd_v1.itemValue);

		kalMemCopy(((struct _CMD_FORMAT_V1_T *)rCmdV1Header.buffer),
				&rCmd_v1,  sizeof(struct _CMD_FORMAT_V1_T));

		rCmdV1Header.cmdBufferLen += sizeof(struct _CMD_FORMAT_V1_T);
		rCmdV1Header.itemNum = 1;

		rStatus = wlanSendSetQueryCmd(
				prAdapter, /* prAdapter */
				CMD_ID_GET_SET_CUSTOMER_CFG, /* 0x70 */
				TRUE,  /* fgSetQuery */
				FALSE, /* fgNeedResp */
				FALSE, /* fgIsOid */
				NULL,  /* pfCmdDoneHandler*/
				NULL,  /* pfCmdTimeoutHandler */
				sizeof(struct _CMD_HEADER_T), /* u4SetQueryInfoLen */
				(PUINT_8)&rCmdV1Header,  /* pucInfoBuffer */
				NULL,  /* pvSetQueryBuffer */
				0      /* u4SetQueryBufferLen */
			);

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO,
				"[Fail]kalIoctl wifiSefCFG fail 0x%x\n",
					rStatus);
#endif
		/* keep in cache */
		u4LogOnOffCache = value;
	} else if (cmd == FW_LOG_CMD_SET_LEVEL) {
		/*ENG_LOAD_OFFSET 1*/
		/*USERDEBUG_LOAD_OFFSET 2 */
		/*USER_LOAD_OFFSET 3 */
		DBGLOG(INIT, INFO, "FW_LOG_CMD_SET_LEVEL\n");
	} else {
		DBGLOG(INIT, INFO, "command can not parse\n");
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Wlan probe function. This function probes and initializes the device.
*
* \param[in] pvData     data passed by bus driver init function
*                           _HIF_EHPI: NULL
*                           _HIF_SDIO: sdio bus driver handle
*
* \retval 0 Success
* \retval negative value Failed
*/
/*----------------------------------------------------------------------------*/
static INT_32 wlanProbe(PVOID pvData)
{
	struct wireless_dev *prWdev = NULL;
	enum probe_fail_reason {
		BUS_INIT_FAIL,
		NET_CREATE_FAIL,
		BUS_SET_IRQ_FAIL,
		ADAPTER_START_FAIL,
		NET_REGISTER_FAIL,
		PROC_INIT_FAIL,
		FAIL_REASON_NUM
	} eFailReason;
	P_WLANDEV_INFO_T prWlandevInfo = NULL;
	INT_32 i4DevIdx = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	INT_32 i4Status = 0;
	BOOLEAN bRet = FALSE;

	eFailReason = FAIL_REASON_NUM;
	do {
		/* 4 <1> Initialize the IO port of the interface */
		/*  GeorgeKuo: pData has different meaning for _HIF_XXX:
		 * _HIF_EHPI: pointer to memory base variable, which will be
		 *      initialized by glBusInit().
		 * _HIF_SDIO: bus driver handle
		 */
		bRet = glBusInit(pvData);
		wlanDebugInit();
		/* Cannot get IO address from interface */
		if (bRet == FALSE) {
			DBGLOG(INIT, ERROR, KERN_ALERT "wlanProbe: glBusInit() fail\n");
			i4Status = -EIO;
			eFailReason = BUS_INIT_FAIL;
			break;
		}
		/* 4 <2> Create network device, Adapter, KalInfo, prDevHandler(netdev) */
		prWdev = wlanNetCreate(pvData);
		if (prWdev == NULL) {
			DBGLOG(INIT, ERROR, "wlanProbe: No memory for dev and its private\n");
			i4Status = -ENOMEM;
			eFailReason = NET_CREATE_FAIL;
			break;
		}
		/* 4 <2.5> Set the ioaddr to HIF Info */
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(prWdev->wiphy);
		gPrDev = prGlueInfo->prDevHandler;

		/* 4 <4> Setup IRQ */
		prWlandevInfo = &arWlanDevInfo[i4DevIdx];

		i4Status = glBusSetIrq(prWdev->netdev, NULL, *((P_GLUE_INFO_T *) netdev_priv(prWdev->netdev)));

		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "wlanProbe: Set IRQ error\n");
			eFailReason = BUS_SET_IRQ_FAIL;
			break;
		}

		prGlueInfo->i4DevIdx = i4DevIdx;

		prAdapter = prGlueInfo->prAdapter;

		prGlueInfo->u4ReadyFlag = 0;

		/* default set the FW roaming enable state to 'on' */
		prGlueInfo->u4FWRoamingEnable = 1;

		/*Check the build variant*/
#if defined(WLAN_ENG_LOAD)
		prGlueInfo->rBuildVarint = MTK_BUILD_VAR_ENG;
#elif defined(WLAN_USERDEBUG_LOAD)
		prGlueInfo->rBuildVarint = MTK_BUILD_VAR_USERDEUB;
#elif defined(WLAN_USER_LOAD)
		prGlueInfo->rBuildVarint = MTK_BUILD_VAR_USER;
#else
		prGlueInfo->rBuildVarint = 0;
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		prAdapter->u4CSUMFlags = (CSUM_OFFLOAD_EN_TX_TCP | CSUM_OFFLOAD_EN_TX_UDP | CSUM_OFFLOAD_EN_TX_IP);
#endif
#if CFG_SUPPORT_CFG_FILE
		wlanCfgInit(prAdapter, NULL, 0, 0);
#ifdef ENABLED_IN_ENGUSERDEBUG
		{
			PUINT_8 pucConfigBuf;
			UINT_32 u4ConfigReadLen;

			pucConfigBuf = (PUINT_8) kalMemAlloc(WLAN_CFG_FILE_BUF_SIZE, VIR_MEM_TYPE);
			u4ConfigReadLen = 0;
			DBGLOG(INIT, LOUD, "CFG_FILE: Read File...\n");
			if (pucConfigBuf) {
				kalMemZero(pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE);
				if (kalReadToFile("/data/misc/wifi.cfg",
						pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE, &u4ConfigReadLen) == 0) {
					DBGLOG(INIT, LOUD, "CFG_FILE: Read /data/misc/wifi.cfg\n");

				} else if (kalReadToFile("/data/misc/wifi/wifi.cfg",
						pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE, &u4ConfigReadLen) == 0) {
					DBGLOG(INIT, LOUD, "CFG_FILE: Read /data/misc/wifi/wifi.cfg\n");
				} else if (kalReadToFile("/etc/firmware/wifi.cfg",
						pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE, &u4ConfigReadLen) == 0) {
					DBGLOG(INIT, LOUD, "CFG_FILE: Read /etc/firmware/wifi.cfg\n");
				}

				if (pucConfigBuf[0] != '\0' && u4ConfigReadLen > 0)
					wlanCfgInit(prAdapter, pucConfigBuf, u4ConfigReadLen, 0);
				kalMemFree(pucConfigBuf, VIR_MEM_TYPE, WLAN_CFG_FILE_BUF_SIZE);
			}	/* pucConfigBuf */
		}
#endif
#endif
		/* 4 <5> Start Device */
		/*  */
#if CFG_ENABLE_FW_DOWNLOAD
		DBGLOG(INIT, TRACE, "start to download firmware...\n");

		/* before start adapter, we need to open and load firmware */
		{
			UINT_32 u4FwSize = 0;
			PVOID prFwBuffer = NULL;
			P_REG_INFO_T prRegInfo = &prGlueInfo->rRegInfo;

			/* P_REG_INFO_T prRegInfo = (P_REG_INFO_T) kmalloc(sizeof(REG_INFO_T), GFP_KERNEL); */
			kalMemSet(prRegInfo, 0, sizeof(REG_INFO_T));
			prRegInfo->u4StartAddress = CFG_FW_START_ADDRESS;
			prRegInfo->u4LoadAddress = CFG_FW_LOAD_ADDRESS;
			/* Load NVRAM content to REG_INFO_T */
			glLoadNvram(prGlueInfo, prRegInfo);
#if CFG_TC10_FEATURE
			if (sysMacAddrOverride(prGlueInfo->rRegInfo.aucMacAddr))
				DBGLOG(INIT, ERROR, "Read MAC addr failed from /efs/\n");
			else
				DBGLOG(INIT, INFO, " MAC addr %pM\n",
					prGlueInfo->rRegInfo.aucMacAddr);
#endif
#if CFG_SUPPORT_CFG_FILE
#ifdef ENABLED_IN_ENGUSERDEBUG
			wlanCfgApply(prAdapter);
#endif
#endif

			/* kalMemCopy(&prGlueInfo->rRegInfo, prRegInfo, sizeof(REG_INFO_T)); */

			prRegInfo->u4PowerMode = CFG_INIT_POWER_SAVE_PROF;
			prRegInfo->fgEnArpFilter = TRUE;

			if (kalFirmwareImageMapping(prGlueInfo, &prFwBuffer, &u4FwSize) == NULL) {
				i4Status = -EIO;
				DBGLOG(INIT, ERROR, "kalFirmwareImageMapping fail!\n");
				goto bailout;
			} else {

				if (wlanAdapterStart(prAdapter, prRegInfo, prFwBuffer,
					u4FwSize) != WLAN_STATUS_SUCCESS) {
					i4Status = -EIO;
				}
			}

			kalFirmwareImageUnmapping(prGlueInfo, NULL, prFwBuffer);

bailout:
			/* kfree(prRegInfo); */

			DBGLOG(INIT, TRACE, "download firmware status = %d\n", i4Status);

			if (i4Status < 0) {
				GL_HIF_INFO_T *HifInfo;
				UINT_32 u4FwCnt;

				DBGLOG(INIT, WARN, "CONNSYS FW CPUINFO:\n");
				HifInfo = &prAdapter->prGlueInfo->rHifInfo;
				for (u4FwCnt = 0; u4FwCnt < 16; u4FwCnt++)
					DBGLOG(INIT, WARN, "0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR));
				/* CONSYS_REG_READ(CONSYS_CPUPCR_REG) */

				/* dump HIF/DMA registers, if fgIsBusAccessFailed is FALSE, otherwise, */
				/* dump HIF register may be hung */
				if (!fgIsBusAccessFailed)
					HifRegDump(prGlueInfo->prAdapter);
/* if (prGlueInfo->rHifInfo.DmaOps->DmaRegDump != NULL) */
/* prGlueInfo->rHifInfo.DmaOps->DmaRegDump(&prGlueInfo->rHifInfo); */
				eFailReason = ADAPTER_START_FAIL;
				break;
			}
		}
#else
		/* P_REG_INFO_T prRegInfo = (P_REG_INFO_T) kmalloc(sizeof(REG_INFO_T), GFP_KERNEL); */
		kalMemSet(&prGlueInfo->rRegInfo, 0, sizeof(REG_INFO_T));
		P_REG_INFO_T prRegInfo = &prGlueInfo->rRegInfo;

		/* Load NVRAM content to REG_INFO_T */
		glLoadNvram(prGlueInfo, prRegInfo);

		prRegInfo->u4PowerMode = CFG_INIT_POWER_SAVE_PROF;

		if (wlanAdapterStart(prAdapter, prRegInfo, NULL, 0) != WLAN_STATUS_SUCCESS) {
			i4Status = -EIO;
			eFailReason = ADAPTER_START_FAIL;
			break;
		}
#endif
		if (prAdapter->fgEnable5GBand == FALSE)
			prWdev->wiphy->bands[IEEE80211_BAND_5GHZ] = NULL;
		else
			prWdev->wiphy->bands[IEEE80211_BAND_5GHZ] = &mtk_band_5ghz;

		prGlueInfo->main_thread = kthread_run(tx_thread, prGlueInfo->prDevHandler, "tx_thread");
#if CFG_SUPPORT_MULTITHREAD
		prGlueInfo->rx_thread = kthread_run(rx_thread, prGlueInfo->prDevHandler, "rx_thread");
#endif
		kalChangeSchedParams(prGlueInfo, TRUE);
		kalSetHalted(FALSE);
#if CFG_SUPPORT_ROAMING_ENC
		/* adjust roaming threshold */
		{
			WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
			CMD_ROAMING_INFO_T rRoamingInfo;
			UINT_32 u4SetInfoLen = 0;

			prAdapter->fgIsRoamingEncEnabled = TRUE;

			/* suggestion from Tsaiyuan.Hsu */
			kalMemZero(&rRoamingInfo, sizeof(CMD_ROAMING_INFO_T));
			rRoamingInfo.fgIsFastRoamingApplied = TRUE;

			DBGLOG(INIT, TRACE, "Enable roaming enhance function\n");

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRoamingInfo,
					   &rRoamingInfo, sizeof(rRoamingInfo), TRUE, TRUE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR, "set roaming advance info fail 0x%x\n", rStatus);
		}
#endif /* CFG_SUPPORT_ROAMING_ENC */

#if (CFG_SUPPORT_TXR_ENC == 1)
		/* adjust tx rate switch threshold */
		rlmTxRateEnhanceConfig(prGlueInfo->prAdapter);
#endif /* CFG_SUPPORT_TXR_ENC */

		/* set MAC address */
		{
			WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
			struct sockaddr MacAddr;
			UINT_32 u4SetInfoLen = 0;

			kalMemZero(MacAddr.sa_data, sizeof(MacAddr.sa_data));
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidQueryCurrentAddr,
					   &MacAddr.sa_data,
					   PARAM_MAC_ADDR_LEN, TRUE, TRUE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, WARN, "set MAC addr fail 0x%x\n", rStatus);
				prGlueInfo->u4ReadyFlag = 0;
			} else {
				ether_addr_copy(prGlueInfo->prDevHandler->dev_addr, (const u8 *)&(MacAddr.sa_data));
				ether_addr_copy(prGlueInfo->prDevHandler->perm_addr,
					prGlueInfo->prDevHandler->dev_addr);

				/* card is ready */
				prGlueInfo->u4ReadyFlag = 1;
#if CFG_SHOW_MACADDR_SOURCE
				DBGLOG(INIT, INFO, "MAC address: %pM ", (&MacAddr.sa_data));
#endif
			}
		}
#ifdef FW_CFG_SUPPORT
		{
			if (wlanFwArrayCfg(prAdapter) != WLAN_STATUS_FAILURE)
				DBGLOG(INIT, INFO, "FW Array Cfg done!");
		}
#ifdef ENABLED_IN_ENGUSERDEBUG
		{
			if (wlanFwFileCfg(prAdapter) != WLAN_STATUS_FAILURE)
				DBGLOG(INIT, INFO, "FW File Cfg done!");
		}
#endif
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		/* set HW checksum offload */
		{
			WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
			UINT_32 u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
			UINT_32 u4SetInfoLen = 0;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetCSUMOffload,
					   (PVOID) &u4CSUMFlags,
					   sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, WARN, "set HW checksum offload fail 0x%x\n", rStatus);
		}
#endif

#if CFG_SUPPORT_EMI_DEBUG
		{
			/* set Driver Read EMI	*/
			WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
			CMD_DRIVER_DUMP_EMI_LOG_T rDriverDumpEmiLog;
			UINT_32 u4SetInfoLen = 0;

			/*ENG_LOAD_OFFSET 1*/
			/*USERDEBUG_LOAD_OFFSET 2 */
			/*USER_LOAD_OFFSET 3 */

			kalMemZero(&rDriverDumpEmiLog, sizeof(CMD_DRIVER_DUMP_EMI_LOG_T));
			rDriverDumpEmiLog.fgIsDriverDumpEmiLogEnable = TRUE | (1 << prGlueInfo->rBuildVarint);

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetEnableDumpEMILog,
					   (PVOID)&rDriverDumpEmiLog,
					   sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, WARN, "set Driver read EMI address fail 0x%x\n", rStatus);
		}
#endif
#if CFG_SUPPORT_802_11K
		{
			WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
			UINT_32 u4SetInfoLen = 0;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSync11kCapbilities,
					   NULL,
					   0, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, WARN, "set 11k Capabilities fail 0x%x\n", rStatus);
		}
#endif
		/* 4 <3> Register the card */
		DBGLOG(INIT, TRACE, "wlanNetRegister...\n");
		i4DevIdx = wlanNetRegister(prWdev);
		if (i4DevIdx < 0) {
			i4Status = -ENXIO;
			DBGLOG(INIT, ERROR, "wlanProbe: Cannot register the net_device context to the kernel\n");
			eFailReason = NET_REGISTER_FAIL;
			break;
		}

		wlanRegisterNotifier();
		/* 4 <6> Initialize /proc filesystem */
#ifdef WLAN_INCLUDE_PROC
		DBGLOG(INIT, TRACE, "init procfs...\n");
		i4Status = procCreateFsEntry(prGlueInfo);
		if (i4Status < 0) {
			DBGLOG(INIT, ERROR, "wlanProbe: init procfs failed\n");
			eFailReason = PROC_INIT_FAIL;
			break;
		}
#endif /* WLAN_INCLUDE_PROC */

#if WLAN_INCLUDE_SYS
		i4Status = sysCreateFsEntry(prGlueInfo);
		if (i4Status < 0) {
			DBGLOG(INIT, ERROR, "wlanProbe: init sysfs failed\n");
			eFailReason = PROC_INIT_FAIL;
			break;
		}
#endif /* WLAN_INCLUDE_SYS */

#ifdef FW_CFG_SUPPORT
		i4Status = cfgCreateProcEntry(prGlueInfo);
		if (i4Status < 0) {
			DBGLOG(INIT, ERROR, "fw cfg proc failed\n");
			break;
		}
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		prGlueInfo->rBowInfo.fgIsNetRegistered = FALSE;
		prGlueInfo->rBowInfo.fgIsRegistered = FALSE;
		glRegisterAmpc(prGlueInfo);
#endif

#if CFG_ENABLE_WIFI_DIRECT
		DBGLOG(INIT, TRACE, "wlanSubModInit...\n");

		/* wlan is launched */
		prGlueInfo->prAdapter->fgIsWlanLaunched = TRUE;
		/* if p2p module is inserted, notify tx_thread to init p2p network */
		if (rSubModHandler[P2P_MODULE].subModInit)
			wlanSubModInit(prGlueInfo);
		/* register set_p2p_mode handler to mtk_wmt_wifi */
		register_set_p2p_mode_handler(set_p2p_mode_handler);
#endif
#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
		if (glIsChipNeedWakelock(prGlueInfo))
			KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter, prGlueInfo->prAdapter->rApWakeLock, "WLAN AP");
#endif
		kalMemZero(&prGlueInfo->rFtIeForTx, sizeof(prGlueInfo->rFtIeForTx));
	} while (FALSE);

	if (i4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "%s eFailReasone:%d\n", __func__, eFailReason);
		switch (eFailReason) {
		case PROC_INIT_FAIL:
			wlanNetUnregister(prWdev);
			set_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag);
			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);
			/* wait main thread stops */
			wait_for_completion_interruptible(&prGlueInfo->rHaltComp);
			KAL_WAKE_LOCK_DESTROY(prAdapter, prAdapter->rTxThreadWakeLock);
			wlanAdapterStop(prAdapter);
			glBusFreeIrq(prWdev->netdev, *((P_GLUE_INFO_T *) netdev_priv(prWdev->netdev)));
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rAhbIsrWakeLock);
#if CFG_SUPPORT_MULTITHREAD
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rTimeoutWakeLock);
#endif
			wlanNetDestroy(prWdev);
			/* prGlueInfo->prAdapter is released in wlanNetDestroy */
			/* Set NULL value for local prAdapter as well */
			prAdapter = NULL;
			break;
		case NET_REGISTER_FAIL:
			set_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag);
			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);
			/* wait main thread stops */
			wait_for_completion_interruptible(&prGlueInfo->rHaltComp);
			KAL_WAKE_LOCK_DESTROY(prAdapter, prAdapter->rTxThreadWakeLock);
			wlanAdapterStop(prAdapter);
			glBusFreeIrq(prWdev->netdev, *((P_GLUE_INFO_T *) netdev_priv(prWdev->netdev)));
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rAhbIsrWakeLock);
#if CFG_SUPPORT_MULTITHREAD
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rTimeoutWakeLock);
#endif
			wlanNetDestroy(prWdev);
			/* prGlueInfo->prAdapter is released in wlanNetDestroy */
			/* Set NULL value for local prAdapter as well */
			prAdapter = NULL;
			break;
		case ADAPTER_START_FAIL:
			glBusFreeIrq(prWdev->netdev, *((P_GLUE_INFO_T *) netdev_priv(prWdev->netdev)));
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rAhbIsrWakeLock);
#if CFG_SUPPORT_MULTITHREAD
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rTimeoutWakeLock);
#endif
			wlanNetDestroy(prWdev);
			/* prGlueInfo->prAdapter is released in wlanNetDestroy */
			/* Set NULL value for local prAdapter as well */
			prAdapter = NULL;
			break;
		case BUS_SET_IRQ_FAIL:
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rAhbIsrWakeLock);
#if CFG_SUPPORT_MULTITHREAD
			KAL_WAKE_LOCK_DESTROY(prAdapter, prGlueInfo->rTimeoutWakeLock);
#endif
			wlanNetDestroy(prWdev);
			/* prGlueInfo->prAdapter is released in wlanNetDestroy */
			/* Set NULL value for local prAdapter as well */
			prAdapter = NULL;
			break;
		case NET_CREATE_FAIL:
			break;
		case BUS_INIT_FAIL:
			break;
		default:
			break;
		}
	}
#if CFG_ENABLE_WIFI_DIRECT
	{
		GLUE_SPIN_LOCK_DECLARATION();

		GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
		g_u4P2PEnding = 0;
		GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);
	}
#endif
#if CFG_SUPPORT_AGPS_ASSIST
	if (i4Status == WLAN_STATUS_SUCCESS)
		kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_ON, NULL, 0);
#endif
#if (CFG_SUPPORT_MET_PROFILING == 1)
	{
		int iMetInitRet = WLAN_STATUS_FAILURE;

		if (i4Status == WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, TRACE, "init MET procfs...\n");
			iMetInitRet = kalMetInitProcfs(prGlueInfo);
			if (iMetInitRet < 0)
				DBGLOG(INIT, ERROR, "wlanProbe: init MET procfs failed\n");
		}
	}
#endif
	if (i4Status == WLAN_STATUS_SUCCESS) {
		/*Init performance monitor structure */
		kalPerMonInit(prGlueInfo);
		/* probe ok */
		DBGLOG(INIT, TRACE, "wlanProbe ok\n");

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
		/* sync log status with firmware */
		if (u4LogOnOffCache != -1) /* -1: connsysD does not set */
			consys_log_event_notification((int)FW_LOG_CMD_ON_OFF,
				u4LogOnOffCache);
#endif

		update_driver_loaded_status(TRUE);
	} else {
		/*
		 * we don't care the return value of mtk_wcn_set_connsys_power_off_flag,
		 * because even this function returns
		 * error, we can also call core dump but only core dump failed.
		 */
		GL_RESET_TRIGGER(prAdapter, RST_FLAG_DO_CORE_DUMP);
		/* probe failed */
		DBGLOG(INIT, ERROR, "wlanProbe failed\n");
	}

	if (u4FwLogLevel > ENUM_WIFI_LOG_LEVEL_OFF) {
		if (prAdapter)
			wlanDbgSetLogLevelImpl(prAdapter, ENUM_WIFI_LOG_LEVEL_VERSION_V1,
					ENUM_WIFI_LOG_MODULE_FW, u4FwLogLevel);
	}

	return i4Status;
}				/* end of wlanProbe() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A method to stop driver operation and release all resources. Following
*        this call, no frame should go up or down through this interface.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static VOID wlanRemove(VOID)
{
#define KAL_WLAN_REMOVE_TIMEOUT_MSEC			3000
	struct net_device *prDev = NULL;
	P_WLANDEV_INFO_T prWlandevInfo = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DBGLOG(INIT, LOUD, "Remove wlan!\n");

	/* 4 <0> Sanity check */
	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, ERROR, "0 == u4WlanDevNum\n");
		return;
	}
	/* unregister set_p2p_mode handler to mtk_wmt_wifi */
	register_set_p2p_mode_handler(NULL);

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	prWlandevInfo = &arWlanDevInfo[u4WlanDevNum - 1];

	ASSERT(prDev);
	if (prDev == NULL) {
		DBGLOG(INIT, ERROR, "NULL == prDev\n");
		return;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "NULL == prGlueInfo\n");
		free_netdev(prDev);
		return;
	}

	kalPerMonDestroy(prGlueInfo);
	/* 4 <3> Remove /proc filesystem. */
#ifdef FW_CFG_SUPPORT
	cfgRemoveProcEntry();
#endif
#ifdef WLAN_INCLUDE_PROC
	procRemoveProcfs();
#endif /* WLAN_INCLUDE_PROC */
#if WLAN_INCLUDE_SYS
	sysRemoveSysfs();
#endif /* WLAN_INCLUDE_SYS */

#if CFG_ENABLE_WIFI_DIRECT
	/* avoid remove & p2p off command simultaneously */
	{
		BOOLEAN fgIsP2POnOffing;

		GLUE_SPIN_LOCK_DECLARATION();

		GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
		g_u4P2PEnding = 1;
		fgIsP2POnOffing = g_u4P2POnOffing;
		GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);

		DBGLOG(INIT, TRACE, "waiting for fgIsP2POnOffing...\n");

		/* History: cannot use down() here, sometimes we cannot come back here */
		/* waiting for p2p off command finishes, we cannot skip the remove */
		while (1) {
			if (fgIsP2POnOffing == 0)
				break;
			GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
			fgIsP2POnOffing = g_u4P2POnOffing;
			GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);
		}
	}
	if (prGlueInfo->prAdapter->fgIsP2PRegistered) {
		DBGLOG(INIT, INFO, "p2pNetUnregister...\n");
#if !CFG_SUPPORT_PERSIST_NETDEV
		/* Release rtnl_lock() */
		complete(&prGlueInfo->rP2pReq);
		p2pNetUnregister(prGlueInfo, FALSE);
#endif
	}
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	if (prGlueInfo->rBowInfo.fgIsNetRegistered) {
		bowNotifyAllLinkDisconnected(prGlueInfo->prAdapter);
		/* wait 300ms for BoW module to send deauth */
		kalMsleep(300);
	}
#endif

	/* 4 <1> Stopping handling interrupt and free IRQ */
	DBGLOG(INIT, TRACE, "free IRQ...\n");
	glBusFreeIrq(prDev, *((P_GLUE_INFO_T *) netdev_priv(prDev)));

	kalSetHalted(TRUE);	/* before flush_delayed_work() */
	if (fgIsWorkMcStart == TRUE) {
		DBGLOG(INIT, TRACE, "flush_delayed_work...\n");
		flush_delayed_work(&workq);	/* flush_delayed_work_sync is deprecated */
	}

	/* Prevent memory leakage */
	prConnSettings = &prGlueInfo->prAdapter->rWifiVar.rConnSettings;
	if (prConnSettings && prConnSettings->assocIeLen > 0) {
		kalMemFree(prConnSettings->pucAssocIEs, VIR_MEM_TYPE,
			   prConnSettings->assocIeLen);
		prConnSettings->assocIeLen = 0;
	}
	flush_delayed_work(&sched_workq);

	DBGLOG(INIT, INFO, "down g_halt_sem...\n");
	kalHaltLock(KAL_WLAN_REMOVE_TIMEOUT_MSEC);
#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
	if (glIsChipNeedWakelock(prGlueInfo))
		KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, prGlueInfo->prAdapter->rApWakeLock);
#endif

/* flush_delayed_work_sync(&workq); */
/* flush_delayed_work(&workq); */ /* flush_delayed_work_sync is deprecated */

	/* 4 <2> Mark HALT, notify main thread to stop, and clean up queued requests */
/* prGlueInfo->u4Flag |= GLUE_FLAG_HALT; */
	set_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag);
#if CFG_SUPPORT_MULTITHREAD
	DBGLOG(INIT, TRACE, "waiting for rx_thread stop...\n");
	wake_up_interruptible(&prGlueInfo->waitq_rx);
	wait_for_completion_interruptible(&prGlueInfo->rRxHaltComp);
#endif

	DBGLOG(INIT, TRACE, "waiting for tx_thread stop...\n");

	/* wake up main thread */
	wake_up_interruptible(&prGlueInfo->waitq);

	DBGLOG(INIT, TRACE, "wait_for_completion_interruptible\n");

	/* wait main thread stops */
	wait_for_completion(&prGlueInfo->rHaltComp);

	DBGLOG(INIT, TRACE, "wlan thread(s) stopped\n");

	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, prGlueInfo->prAdapter->rTxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, prGlueInfo->rAhbIsrWakeLock);
#if CFG_SUPPORT_MULTITHREAD
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, prGlueInfo->rTimeoutWakeLock);
	DBGLOG(INIT, INFO, "remove rTimeoutWakeLock = %p\n", &prGlueInfo->rTimeoutWakeLock);

#endif

	kalMemSet(&(prGlueInfo->prAdapter->rWlanInfo), 0, sizeof(WLAN_INFO_T));

	/* prGlueInfo->rHifInfo.main_thread = NULL; */
	prGlueInfo->main_thread = NULL;
#if CFG_SUPPORT_MULTITHREAD
	prGlueInfo->rx_thread = NULL;
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	if (prGlueInfo->rBowInfo.fgIsRegistered)
		glUnregisterAmpc(prGlueInfo);
#endif

#if (CFG_SUPPORT_MET_PROFILING == 1)
	kalMetRemoveProcfs();
#endif

	/* Force to do DMA reset */
	DBGLOG(INIT, TRACE, "glResetHif\n");
	glResetHif(prGlueInfo);

	/* 4 <4> wlanAdapterStop */
	prAdapter = prGlueInfo->prAdapter;
#if CFG_SUPPORT_AGPS_ASSIST
	kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_OFF, NULL, 0);
#endif

	wlanAdapterStop(prAdapter);
	DBGLOG(INIT, TRACE, "Number of Stalled Packets = %d\n", prGlueInfo->i4TxPendingFrameNum);

#if CFG_ENABLE_WIFI_DIRECT
	prGlueInfo->prAdapter->fgIsWlanLaunched = FALSE;
	if (prGlueInfo->prAdapter->fgIsP2PRegistered) {
		DBGLOG(INIT, INFO, "p2pRemove...\n");
		p2pRemove(prGlueInfo);
	}
#endif

	/* 4 <5> Release the Bus */
	glBusRelease(prDev);

	kalHaltUnlock();
	wlanDebugUninit();
	/* 4 <6> Unregister the card */
	wlanNetUnregister(prDev->ieee80211_ptr);

	/* 4 <7> Destroy the device */
	wlanNetDestroy(prDev->ieee80211_ptr);
	prDev = NULL;

	DBGLOG(INIT, LOUD, "wlanUnregisterNotifier...\n");
	wlanUnregisterNotifier();

	update_driver_loaded_status(FALSE);

	DBGLOG(INIT, INFO, "wlanRemove ok\n");
}				/* end of wlanRemove() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Driver entry point when the driver is configured as a Linux Module, and
*        is called once at module load time, by the user-level modutils
*        application: insmod or modprobe.
*
* \retval 0     Success
*/
/*----------------------------------------------------------------------------*/
/* 1 Module Entry Point */
static int initWlan(void)
{
	int ret = 0, i;

	/* Set the initial debug level of each module */
#if DBG
	for (i = 0; i < DBG_MODULE_NUM; i++)
		aucDebugModule[i] = DBG_CLASS_MASK; /* enable all */
#else
	for (i = 0; i < DBG_MODULE_NUM; i++)
		aucDebugModule[i] = DBG_CLASS_ERROR | DBG_CLASS_WARN | DBG_CLASS_INFO | DBG_CLASS_STATE;
#endif /* DBG */

	DBGLOG(INIT, INFO, "initWlan\n");

	spin_lock_init(&g_p2p_lock);

	fgNvramAvailable = FALSE;
	/* memory pre-allocation */
	kalInitIOBuffer();
	procInitFs();
#if WLAN_INCLUDE_SYS
	sysInitFs();
#endif
	createWirelessDevice();
	if (gprWdev)
		glP2pCreateWirelessDevice((P_GLUE_INFO_T) wiphy_priv(gprWdev->wiphy));
	gPrDev = NULL;

	ret = ((glRegisterBus(wlanProbe, wlanRemove) == WLAN_STATUS_SUCCESS) ? 0 : -EIO);

	if (ret == -EIO) {
		kalUninitIOBuffer();
		return ret;
	}
#if (CFG_CHIP_RESET_SUPPORT)
	glResetInit();
#endif

	/* Register framebuffer notifier client */
	if (gprWdev)
		kalFbNotifierReg((P_GLUE_INFO_T) wiphy_priv(gprWdev->wiphy));

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	wifi_fwlog_event_func_register(consys_log_event_notification);
#endif

	/* Set the initial DEBUG CLASS of each module */
	return ret;
}				/* end of initWlan() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Driver exit point when the driver as a Linux Module is removed. Called
*        at module unload time, by the user level modutils application: rmmod.
*        This is our last chance to clean up after ourselves.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
/* 1 Module Leave Point */
static VOID exitWlan(void)
{
	DBGLOG(INIT, INFO, "exitWlan\n");

	/* Unregister framebuffer notifier client*/
	kalFbNotifierUnReg();

#if CFG_CHIP_RESET_SUPPORT
	glResetUninit();
#endif
	destroyWirelessDevice();
	glP2pDestroyWirelessDevice();

	glUnregisterBus(wlanRemove);

	/* free pre-allocated memory */
	kalUninitIOBuffer();

	DBGLOG(INIT, INFO, "exitWlan\n");
	procUninitProcFs();
#if WLAN_INCLUDE_SYS
	sysUninitSysFs();
#endif

}				/* end of exitWlan() */

#if CFG_SUPPORT_SET_CAM_BY_PROC
VOID nicConfigProcSetCamCfgWrite(BOOLEAN enabled)
{
	struct net_device *prDev = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	PARAM_POWER_MODE ePowerMode;
	BOOLEAN fgEnCmdEvent;
	UINT_8 ucBssIndex;
	CMD_PS_PROFILE_T arPowerSaveMode[NETWORK_TYPE_INDEX_NUM];

	/* 4 <1> Sanity Check */
	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, ERROR, "wlanLateResume u4WlanDevNum invalid!!\n");
		return;
	}

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	if (!prDev)
		return;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	if (!prGlueInfo)
		return;

	prAdapter = prGlueInfo->prAdapter;
	if ((!prAdapter) || (!&(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX])))
		return;

	fgEnCmdEvent = FALSE;
	ucBssIndex = NETWORK_TYPE_AIS_INDEX;
	arPowerSaveMode[ucBssIndex].ucNetTypeIndex = ucBssIndex;

	if (enabled) {
		nicForceSetCAM(TRUE);
		ePowerMode = Param_PowerModeCAM;
		arPowerSaveMode[ucBssIndex].ucPsProfile = (UINT_8) ePowerMode;
		DBGLOG(INIT, INFO, "Enable CAM BssIndex:%d, PowerMode:%d\n",
		       ucBssIndex, arPowerSaveMode[ucBssIndex].ucPsProfile);
	} else {
		nicForceSetCAM(FALSE);
		arPowerSaveMode[ucBssIndex].ucPsProfile =
				prAdapter->rWlanInfo.arPowerSaveMode[ucBssIndex].ucPsProfile;
		DBGLOG(INIT, INFO, "Disable CAM BssIndex:%d, PowerMode:%d\n",
		       ucBssIndex, arPowerSaveMode[ucBssIndex].ucPsProfile);
	}

	wlanSendSetQueryCmd(prAdapter,
			    CMD_ID_POWER_SAVE_MODE,
			    TRUE,
			    FALSE,
			    (fgEnCmdEvent ? TRUE : FALSE),
			    (fgEnCmdEvent ? nicCmdEventSetCommon : NULL),
			    (fgEnCmdEvent ? nicOidCmdTimeoutCommon : NULL),
			    sizeof(CMD_PS_PROFILE_T),
			    (PUINT_8)&(arPowerSaveMode[ucBssIndex]),
			    NULL, sizeof(PARAM_POWER_MODE));
}
#endif

#ifdef MTK_WCN_BUILT_IN_DRIVER

int mtk_wcn_wlan_gen2_init(void)
{
	return initWlan();
}
EXPORT_SYMBOL(mtk_wcn_wlan_gen2_init);

void mtk_wcn_wlan_gen2_exit(void)
{
	return exitWlan();
}
EXPORT_SYMBOL(mtk_wcn_wlan_gen2_exit);

#else

module_init(initWlan);
module_exit(exitWlan);

#endif

MODULE_AUTHOR(NIC_AUTHOR);
MODULE_DESCRIPTION(NIC_DESC);
MODULE_SUPPORTED_DEVICE(NIC_NAME);
MODULE_LICENSE("GPL");
