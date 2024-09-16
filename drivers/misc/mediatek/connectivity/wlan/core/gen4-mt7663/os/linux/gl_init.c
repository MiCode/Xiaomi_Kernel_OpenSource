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
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux
 *      /gl_init.c#11
 */

/*! \file   gl_init.c
 *    \brief  Main routines of Linux driver
 *
 *    This file contains the main routines of Linux driver for MediaTek Inc.
 *    802.11 Wireless LAN Adapters.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "gl_cfg80211.h"
#include "precomp.h"
#if CFG_SUPPORT_AGPS_ASSIST
#include "gl_kal.h"
#endif
#include "gl_vendor.h"
#if CFG_THERMAL_API_SUPPORT
#include "mtk_ts_wmt.h"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* #define MAX_IOREQ_NUM   10 */
struct semaphore g_halt_sem;
int g_u4HaltFlag;

struct wireless_dev *gprWdev;
#if CFG_THERMAL_API_SUPPORT
u_int8_t g_fgIsWifiEnabled = FALSE;
static int mtk_wcn_temp_query_ctrl(void);
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* Tasklet mechanism is like buttom-half in Linux. We just want to
 * send a signal to OS for interrupt defer processing. All resources
 * are NOT allowed reentry, so txPacket, ISR-DPC and ioctl must avoid preempty.
 */
struct WLANDEV_INFO {
	struct net_device *prDev;
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

MODULE_AUTHOR(NIC_AUTHOR);
MODULE_DESCRIPTION(NIC_DESC);
MODULE_SUPPORTED_DEVICE(NIC_NAME);

/* MODULE_LICENSE("MTK Propietary"); */
MODULE_LICENSE("Dual BSD/GPL");

#ifdef CFG_DRIVER_INF_NAME_CHANGE
char *gprifnamesta = "";
char *gprifnamep2p = "";
char *gprifnameap = "";
module_param_named(sta, gprifnamesta, charp, 0000);
module_param_named(p2p, gprifnamep2p, charp, 0000);
module_param_named(ap, gprifnameap, charp, 0000);
#endif /* CFG_DRIVER_INF_NAME_CHANGE */

/* NIC interface name */
#define NIC_INF_NAME    "wlan%d"

#ifdef CFG_DRIVER_INF_NAME_CHANGE
/* Kernel IFNAMESIZ is 16, we use 5 in case some protocol might auto gen
 * interface name,
 */
/* in that case, the interface name might have risk of over kernel's IFNAMESIZ
 */
#define CUSTOM_IFNAMESIZ 5
#endif /* CFG_DRIVER_INF_NAME_CHANGE */

#if CFG_SUPPORT_SNIFFER
#define NIC_MONITOR_INF_NAME	"radiotap%d"
#endif

uint8_t aucDebugModule[DBG_MODULE_NUM];
uint32_t au4LogLevel[ENUM_WIFI_LOG_MODULE_NUM] = {ENUM_WIFI_LOG_LEVEL_DEFAULT};

/* 4 2007/06/26, mikewu, now we don't use this, we just fix the number of wlan
 *               device to 1
 */
static struct WLANDEV_INFO
	arWlanDevInfo[CFG_MAX_WLAN_DEVICES] = { {0} };

static uint32_t
u4WlanDevNum;	/* How many NICs coexist now */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
u_int8_t	g_fgIsCalDataBackuped = FALSE;
#endif

/* 20150205 added work queue for sched_scan to avoid cfg80211 stop schedule scan
 *          dead loack
 */
struct delayed_work sched_workq;

#define CFG_EEPRM_FILENAME    "EEPROM"
#define FILE_NAME_MAX     64

#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
static uint8_t *apucEepromName[] = {
	(uint8_t *) CFG_EEPRM_FILENAME "_MT",
	NULL
};
#endif

int CFG80211_Suspend(struct wiphy *wiphy,
		     struct cfg80211_wowlan *wow)
{
	DBGLOG(INIT, INFO, "CFG80211 suspend CB\n");

	return 0;
}

int CFG80211_Resume(struct wiphy *wiphy)
{
	DBGLOG(INIT, INFO, "CFG80211 resume CB\n");

	return 0;
}

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

#define CHAN2G(_channel, _freq, _flags)		\
{						\
	.band               = KAL_BAND_2GHZ,	\
	.center_freq        = (_freq),		\
	.hw_value           = (_channel),	\
	.flags              = (_flags),		\
	.max_antenna_gain   = 0,		\
	.max_power          = 30,		\
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

#define CHAN5G(_channel, _flags)					\
{									\
	.band               = KAL_BAND_5GHZ,				\
	.center_freq        =						\
		(((_channel >= 182) && (_channel <= 196)) ?		\
		(4000 + (5 * (_channel))) : (5000 + (5 * (_channel)))),	\
	.hw_value           = (_channel),				\
	.flags              = (_flags),					\
	.max_antenna_gain   = 0,					\
	.max_power          = 30,					\
}
static struct ieee80211_channel mtk_5ghz_channels[] = {
	CHAN5G(36, 0), CHAN5G(40, 0),
	CHAN5G(44, 0), CHAN5G(48, 0),
	CHAN5G(52, 0), CHAN5G(56, 0),
	CHAN5G(60, 0), CHAN5G(64, 0),
	CHAN5G(100, 0), CHAN5G(104, 0),
	CHAN5G(108, 0), CHAN5G(112, 0),
	CHAN5G(116, 0), CHAN5G(120, 0),
	CHAN5G(124, 0), CHAN5G(128, 0),
	CHAN5G(132, 0), CHAN5G(136, 0),
	CHAN5G(140, 0), CHAN5G(144, 0),
	CHAN5G(149, 0), CHAN5G(153, 0),
	CHAN5G(157, 0), CHAN5G(161, 0),
	CHAN5G(165, 0),

};

#define RATETAB_ENT(_rate, _rateid, _flags)	\
{						\
	.bitrate    = (_rate),			\
	.hw_value   = (_rateid),		\
	.flags      = (_flags),			\
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

#define WLAN_MCS_INFO						\
{								\
	.rx_mask        = {0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0},	\
	.rx_highest     = 0,					\
	.tx_params      = IEEE80211_HT_MCS_TX_DEFINED,		\
}

#define WLAN_VHT_MCS_INFO					\
{								\
	.rx_mcs_map     = 0xFFFA,				\
	.rx_highest     = cpu_to_le16(867),			\
	.tx_mcs_map     = 0xFFFA,				\
	.tx_highest     = cpu_to_le16(867),			\
}


#define WLAN_HT_CAP						\
{								\
	.ht_supported   = true,					\
	.cap            = IEEE80211_HT_CAP_SUP_WIDTH_20_40	\
			| IEEE80211_HT_CAP_SM_PS		\
			| IEEE80211_HT_CAP_GRN_FLD		\
			| IEEE80211_HT_CAP_SGI_20		\
			| IEEE80211_HT_CAP_SGI_40,		\
	.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,		\
	.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_NONE,	\
	.mcs            = WLAN_MCS_INFO,			\
}

#define WLAN_VHT_CAP							\
{									\
	.vht_supported  = true,						\
	.cap            = IEEE80211_VHT_CAP_RXLDPC			\
			| IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK	\
			| IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454	\
			| IEEE80211_VHT_CAP_RXLDPC			\
			| IEEE80211_VHT_CAP_SHORT_GI_80			\
			| IEEE80211_VHT_CAP_TXSTBC			\
			| IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE	\
			| IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE,	\
	.vht_mcs        = WLAN_VHT_MCS_INFO,				\
}

/* public for both Legacy Wi-Fi / P2P access */
struct ieee80211_supported_band mtk_band_2ghz = {
	.band = KAL_BAND_2GHZ,
	.channels = mtk_2ghz_channels,
	.n_channels = ARRAY_SIZE(mtk_2ghz_channels),
	.bitrates = mtk_g_rates,
	.n_bitrates = mtk_g_rates_size,
	.ht_cap = WLAN_HT_CAP,
};

/* public for both Legacy Wi-Fi / P2P access */
struct ieee80211_supported_band mtk_band_5ghz = {
	.band = KAL_BAND_5GHZ,
	.channels = mtk_5ghz_channels,
	.n_channels = ARRAY_SIZE(mtk_5ghz_channels),
	.bitrates = mtk_a_rates,
	.n_bitrates = mtk_a_rates_size,
	.ht_cap = WLAN_HT_CAP,
	.vht_cap = WLAN_VHT_CAP,
};

const uint32_t mtk_cipher_suites[] = {
	/* keep WEP first, it may be removed below */
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,

	/* keep last -- depends on hw flags! */
	WLAN_CIPHER_SUITE_AES_CMAC,
	WLAN_CIPHER_SUITE_NO_GROUP_ADDR
};

#if (CFG_ENABLE_UNIFY_WIPHY == 0)
static struct cfg80211_ops mtk_wlan_ops = {
	.suspend = mtk_cfg80211_suspend,
	.resume = mtk_cfg80211_resume,
	.change_virtual_intf = mtk_cfg80211_change_iface,
	.add_key = mtk_cfg80211_add_key,
	.get_key = mtk_cfg80211_get_key,
	.del_key = mtk_cfg80211_del_key,
	.set_default_key = mtk_cfg80211_set_default_key,
	.get_station = mtk_cfg80211_get_station,
#if CFG_SUPPORT_TDLS
	.change_station = mtk_cfg80211_change_station,
	.add_station = mtk_cfg80211_add_station,
	.del_station = mtk_cfg80211_del_station,
#endif
	.scan = mtk_cfg80211_scan,
#if KERNEL_VERSION(4, 5, 0) <= CFG80211_VERSION_CODE
	.abort_scan = mtk_cfg80211_abort_scan,
#endif
	.connect = mtk_cfg80211_connect,
#if CFG_SUPPORT_CFG80211_AUTH
	.deauth = mtk_cfg80211_deauth,
#endif
	.disconnect = mtk_cfg80211_disconnect,
	.join_ibss = mtk_cfg80211_join_ibss,
	.leave_ibss = mtk_cfg80211_leave_ibss,
	.set_power_mgmt = mtk_cfg80211_set_power_mgmt,
	.set_pmksa = mtk_cfg80211_set_pmksa,
	.del_pmksa = mtk_cfg80211_del_pmksa,
	.flush_pmksa = mtk_cfg80211_flush_pmksa,
#if CONFIG_SUPPORT_GTK_REKEY
	.set_rekey_data = mtk_cfg80211_set_rekey_data,
#endif
#if CFG_SUPPORT_CFG80211_AUTH
	.auth = mtk_cfg80211_auth,
#endif
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
#if CFG_SUPPORT_SCHED_SCAN
	.sched_scan_start = mtk_cfg80211_sched_scan_start,
	.sched_scan_stop = mtk_cfg80211_sched_scan_stop,
#endif /* CFG_SUPPORT_SCHED_SCAN */
#if CFG_SUPPORT_TDLS
	.tdls_oper = mtk_cfg80211_tdls_oper,
	.tdls_mgmt = mtk_cfg80211_tdls_mgmt,
#endif
	.update_ft_ies = mtk_cfg80211_update_ft_ies,
};
#else /* CFG_ENABLE_UNIFY_WIPHY */
static struct cfg80211_ops mtk_cfg_ops = {
	.add_virtual_intf = mtk_cfg_add_iface,
	.del_virtual_intf = mtk_cfg_del_iface,
	.change_virtual_intf = mtk_cfg_change_iface,
	.add_key = mtk_cfg_add_key,
	.get_key = mtk_cfg_get_key,
	.del_key = mtk_cfg_del_key,
	.set_default_mgmt_key = mtk_cfg_set_default_mgmt_key,
	.set_default_key = mtk_cfg_set_default_key,
	.get_station = mtk_cfg_get_station,
#if CFG_SUPPORT_TDLS
	.change_station = mtk_cfg_change_station,
	.add_station = mtk_cfg_add_station,
	.tdls_oper = mtk_cfg_tdls_oper,
	.tdls_mgmt = mtk_cfg_tdls_mgmt,
#endif
	.del_station = mtk_cfg_del_station,	/* AP/P2P use this function */
	.scan = mtk_cfg_scan,
#if KERNEL_VERSION(4, 5, 0) <= CFG80211_VERSION_CODE
	.abort_scan = mtk_cfg_abort_scan,
#endif
#if CFG_SUPPORT_SCHED_SCAN
	.sched_scan_start = mtk_cfg_sched_scan_start,
	.sched_scan_stop = mtk_cfg_sched_scan_stop,
#endif /* CFG_SUPPORT_SCHED_SCAN */

	.connect = mtk_cfg_connect,
#if CFG_SUPPORT_CFG80211_AUTH
	.deauth = mtk_cfg_deauth,
#endif
	.disconnect = mtk_cfg_disconnect,
	.join_ibss = mtk_cfg_join_ibss,
	.leave_ibss = mtk_cfg_leave_ibss,
	.set_power_mgmt = mtk_cfg_set_power_mgmt,
	.set_pmksa = mtk_cfg_set_pmksa,
	.del_pmksa = mtk_cfg_del_pmksa,
	.flush_pmksa = mtk_cfg_flush_pmksa,
#if CONFIG_SUPPORT_GTK_REKEY
	.set_rekey_data = mtk_cfg_set_rekey_data,
#endif
	.suspend = mtk_cfg_suspend,
	.resume = mtk_cfg_resume,
#if CFG_SUPPORT_CFG80211_AUTH
	.auth = mtk_cfg80211_auth,
#endif
	.assoc = mtk_cfg_assoc,

	/* Action Frame TX/RX */
	.remain_on_channel = mtk_cfg_remain_on_channel,
	.cancel_remain_on_channel = mtk_cfg_cancel_remain_on_channel,
	.mgmt_tx = mtk_cfg_mgmt_tx,
	/* .mgmt_tx_cancel_wait        = mtk_cfg80211_mgmt_tx_cancel_wait, */
	.mgmt_frame_register = mtk_cfg_mgmt_frame_register,

#ifdef CONFIG_NL80211_TESTMODE
	.testmode_cmd = mtk_cfg_testmode_cmd,
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
	.start_radar_detection = mtk_cfg_start_radar_detection,
#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
	.channel_switch = mtk_cfg_channel_switch,
#endif
#endif

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0)
	.change_bss = mtk_cfg_change_bss,
	.mgmt_tx_cancel_wait = mtk_cfg_mgmt_tx_cancel_wait,
	.deauth = mtk_cfg_deauth,
	.disassoc = mtk_cfg_disassoc,
	.start_ap = mtk_cfg_start_ap,
	.change_beacon = mtk_cfg_change_beacon,
	.stop_ap = mtk_cfg_stop_ap,
	.set_wiphy_params = mtk_cfg_set_wiphy_params,
	.set_bitrate_mask = mtk_cfg_set_bitrate_mask,
	.set_tx_power = mtk_cfg_set_txpower,
	.get_tx_power = mtk_cfg_get_txpower,
#endif
	.update_ft_ies = mtk_cfg80211_update_ft_ies,
};
#endif	/* CFG_ENABLE_UNIFY_WIPHY */

#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE

static const struct wiphy_vendor_command
	mtk_wlan_vendor_ops[] = {
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_GET_CHANNEL_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_channel_list
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_COUNTRY_CODE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
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
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_SETBAND
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_band
	},
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = WIFI_SUBCMD_SET_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_roaming_policy
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_GET_ROAMING_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_roaming_capabilities
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_CONFIG_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_config_roaming
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_ENABLE_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_enable_roaming
	},
	/* RTT */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = RTT_SUBCMD_GETCAPABILITY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
		WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_rtt_capabilities
	},
	/* Link Layer Statistics */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = LSTATS_SUBCMD_GET_INFO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
		WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_llstats_get_info
	},
	/* RSSI Monitoring */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_RSSI_MONITOR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
		WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_rssi_monitoring
	},
	/* Packet Keep Alive */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_OFFLOAD_START_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
		WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_packet_keep_alive_start
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_OFFLOAD_STOP_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
		WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_packet_keep_alive_stop
	},
	/* Get Driver Version or Firmware Version */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = LOGGER_GET_VER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_version
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
#if CFG_SUPPORT_P2P_PREFERRED_FREQ_LIST
	/* P2P get preferred freq list */
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = NL80211_VENDOR_SUBCMD_GET_PREFER_FREQ_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV
				| WIPHY_VENDOR_CMD_NEED_NETDEV
				| WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = mtk_cfg80211_vendor_get_preferred_freq_list
	},
#endif /* CFG_SUPPORT_P2P_PREFERRED_FREQ_LIST */
};

static const struct nl80211_vendor_cmd_info
	mtk_wlan_vendor_events[] = {
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
#if CFG_SUPPORT_MAGIC_PKT_VENDOR_EVENT
	{
		.vendor_id = GOOGLE_OUI,
		.subcmd = WIFI_EVENT_MAGIC_PACKET_RECEIVED
	}
#endif
};
#endif

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
		      BIT(IEEE80211_STYPE_BEACON >> 4)
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		      BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_AP_VLAN] = {
		/* copy AP */
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) |
		BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		      BIT(IEEE80211_STYPE_ACTION >> 4)
	}
};

#ifdef CONFIG_PM
static const struct wiphy_wowlan_support mtk_wlan_wowlan_support = {
	.flags = WIPHY_WOWLAN_MAGIC_PKT | WIPHY_WOWLAN_DISCONNECT |
		 WIPHY_WOWLAN_ANY,
};
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if 0
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
#endif

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    struct net_device *sb_dev, select_queue_fallback_t fallback)
{
	return mtk_wlan_ndev_select_queue(skb);
}
#elif KERNEL_VERSION(3, 14, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    void *accel_priv, select_queue_fallback_t fallback)
{
	return mtk_wlan_ndev_select_queue(skb);
}
#elif KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    void *accel_priv)
{
	return mtk_wlan_ndev_select_queue(skb);
}
#else
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb)
{
	return mtk_wlan_ndev_select_queue(skb);
}
#endif

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
static void glLoadNvram(IN struct GLUE_INFO *prGlueInfo,
			OUT struct REG_INFO *prRegInfo)
{
	ASSERT(prGlueInfo);
	ASSERT(prRegInfo);
	DBGLOG(INIT, INFO, "glLoadNvram start\n");
	if ((!prGlueInfo) || (!prRegInfo))
		return;

	/* load full NVRAM */
	prGlueInfo->fgNvramAvailable = FALSE;
	if (kalCfgDataRead(prGlueInfo, 0,
			   sizeof(prRegInfo->aucNvram),
			   (uint16_t *)prRegInfo->aucNvram) == TRUE) {

		struct WIFI_CFG_PARAM_STRUCT *prNvramSettings;

		if (sizeof(struct WIFI_CFG_PARAM_STRUCT) >
		    sizeof(prRegInfo->aucNvram)) {
			DBGLOG(INIT, ERROR,
			       "Size WIFI_CFG_PARAM_STRUCT %zu > size aucNvram %zu\n"
			       , sizeof(struct WIFI_CFG_PARAM_STRUCT),
			       sizeof(prRegInfo->aucNvram));
			return;
		}
#if CFG_SUPPORT_NVRAM_5G
		if (sizeof(struct NEW_EFUSE_MAPPING2NVRAM) >
		    sizeof(prRegInfo->aucEFUSE)) {
			DBGLOG(INIT, ERROR,
			       "Size NEW_EFUSE_MAPPING2NVRAM %zu >size aucEFUSE %zu\n"
			       , sizeof(struct NEW_EFUSE_MAPPING2NVRAM),
			       sizeof(prRegInfo->aucEFUSE));
			return;
		}
#endif

		prRegInfo->prNvramSettings =
			(struct WIFI_CFG_PARAM_STRUCT *)&prRegInfo->aucNvram;
		prNvramSettings = prRegInfo->prNvramSettings;

#if CFG_SUPPORT_NVRAM_5G
		/* load EFUSE overriding part */
		kalMemCopy(prRegInfo->aucEFUSE,
			   prNvramSettings->EfuseMapping.aucEFUSE,
			   sizeof(prRegInfo->aucEFUSE));
		prRegInfo->prOldEfuseMapping =
			(struct NEW_EFUSE_MAPPING2NVRAM *)&prRegInfo->aucEFUSE;
#else
		/* load EFUSE overriding part */
		kalMemCopy(prRegInfo->aucEFUSE, prNvramSettings->aucEFUSE,
			   sizeof(prRegInfo->aucEFUSE));
#endif

		/* load MAC Address */
		kalMemCopy(prRegInfo->aucMacAddr,
			   prNvramSettings->aucMacAddress,
			   PARAM_MAC_ADDR_LEN * sizeof(uint8_t));

		/* load country code */
		/* cast to wide characters */
		prRegInfo->au2CountryCode[0] =
			(uint16_t) prNvramSettings->aucCountryCode[0];
		prRegInfo->au2CountryCode[1] =
			(uint16_t) prNvramSettings->aucCountryCode[1];

		/* load default normal TX power */
		kalMemCopy(&prRegInfo->rTxPwr, &prNvramSettings->rTxPwr,
			   sizeof(struct TX_PWR_PARAM));

		/* load feature flags */
		prRegInfo->ucTxPwrValid = prNvramSettings->ucTxPwrValid;

		prRegInfo->ucSupport5GBand =
			prNvramSettings->ucSupport5GBand;

		prRegInfo->uc2G4BwFixed20M =
			prNvramSettings->uc2G4BwFixed20M;

		prRegInfo->uc5GBwFixed20M = prNvramSettings->uc5GBwFixed20M;

		prRegInfo->ucEnable5GBand = prNvramSettings->ucEnable5GBand;

		prRegInfo->ucRxDiversity = prNvramSettings->ucRxDiversity;

		prRegInfo->ucRssiPathCompasationUsed =
			prNvramSettings->fgRssiCompensationVaildbit;

		prRegInfo->ucGpsDesense = prNvramSettings->ucGpsDesense;

		/* load band edge tx power control */
		prRegInfo->fg2G4BandEdgePwrUsed =
			prNvramSettings->fg2G4BandEdgePwrUsed;

		if (prRegInfo->prNvramSettings->fg2G4BandEdgePwrUsed) {
			prRegInfo->cBandEdgeMaxPwrCCK =
				prNvramSettings->cBandEdgeMaxPwrCCK;

			prRegInfo->cBandEdgeMaxPwrOFDM20 =
				prNvramSettings->cBandEdgeMaxPwrOFDM20;

			prRegInfo->cBandEdgeMaxPwrOFDM40 =
				prNvramSettings->cBandEdgeMaxPwrOFDM40;
		}

		/* load regulation subbands */
		prRegInfo->eRegChannelListMap =
					(enum ENUM_REG_CH_MAP)
					prNvramSettings->ucRegChannelListMap;
		prRegInfo->ucRegChannelListIndex =
			prNvramSettings->ucRegChannelListIndex;

		if (prRegInfo->eRegChannelListMap ==
		    REG_CH_MAP_CUSTOMIZED) {
			kalMemCopy(prRegInfo->rDomainInfo.rSubBand,
				   prNvramSettings->aucRegSubbandInfo,
				   MAX_SUBBAND_NUM * sizeof(uint8_t));
		}

		/* load rssiPathCompensation */
		kalMemCopy(&prRegInfo->rRssiPathCompasation,
			   &prNvramSettings->rRssiPathCompensation,
			   sizeof(struct RSSI_PATH_COMPASATION));

		prGlueInfo->fgNvramAvailable = TRUE;
		DBGLOG(INIT, INFO, "glLoadNvram end\n");
	} else {
		DBGLOG(INIT, INFO, "glLoadNvram fail\n");
	}

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Release prDev from wlandev_array and free tasklet object related to
 *	  it.
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
 * \brief Allocate an unique interface index, net_device::ifindex member for
 *	  this wlan device. Store the net_device in wlandev_array, and
 *	  initialize tasklet object related to it.
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
 *        Everything Linux ioctl specific is done here. Then we pass the
 *	  contents of the ifr->data to the request message handler.
 *
 * \param[in] prDev      Linux kernel netdevice
 *
 * \param[in] prIFReq    Our private ioctl request structure, typed for the
 *			 generic
 *                       struct ifreq so we can use ptr to function
 *
 * \param[in] cmd        Command ID
 *
 * \retval WLAN_STATUS_SUCCESS The IOCTL command is executed successfully.
 * \retval OTHER The execution of IOCTL command is failed.
 */
/*----------------------------------------------------------------------------*/
int wlanDoIOCTL(struct net_device *prDev,
		struct ifreq *prIfReq, int i4Cmd)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int ret = 0;

	/* Verify input parameters for the following functions */
	ASSERT(prDev && prIfReq);
	if (!prDev || !prIfReq) {
		DBGLOG(INIT, ERROR, "Invalid input data\n");
		return -EINVAL;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
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
	} else if ((i4Cmd >= SIOCIWFIRSTPRIV)
		   && (i4Cmd < SIOCIWLASTPRIV)) {
		/* 0x8BE0 ~ 0x8BFF, private ioctl region */
		ret = priv_support_ioctl(prDev, prIfReq, i4Cmd);
	} else if (i4Cmd == SIOCDEVPRIVATE + 1) {
#ifdef CFG_ANDROID_AOSP_PRIV_CMD
		ret = android_private_support_driver_cmd(prDev, prIfReq, i4Cmd);
#else
		ret = priv_support_driver_cmd(prDev, prIfReq, i4Cmd);
#endif /* CFG_ANDROID_AOSP_PRIV_CMD */
	} else {
		DBGLOG(INIT, WARN, "Unexpected ioctl command: 0x%04x\n",
		       i4Cmd);
		ret = -EOPNOTSUPP;
	}

	return ret;
}				/* end of wlanDoIOCTL() */

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
struct GLUE_INFO *wlanGetGlueInfo(void)
{
	struct net_device *prDev = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	if (u4WlanDevNum == 0)
		return NULL;

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	if (prDev == NULL)
		return NULL;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));

	return prGlueInfo;
}

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

static void wlanSetMulticastList(struct net_device *prDev)
{
	/* Allow to receive all multicast for WOW */
	DBGLOG(INIT, TRACE, "wlanSetMulticastList\n");
	prDev->flags |= (IFF_MULTICAST | IFF_ALLMULTI);
	gPrDev = prDev;
	schedule_delayed_work(&workq, 0);
}

/* FIXME: Since we cannot sleep in the wlanSetMulticastList, we arrange
 * another workqueue for sleeping. We don't want to block
 * main_thread, so we can't let tx_thread to do this
 */

static void wlanSetMulticastListWorkQueue(
	struct work_struct *work)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4PacketFilter = 0;
	uint32_t u4SetInfoLen;
	struct net_device *prDev = gPrDev;

	down(&g_halt_sem);
	if (g_u4HaltFlag) {
		up(&g_halt_sem);
		return;
	}

	prGlueInfo = (prDev != NULL) ? *((struct GLUE_INFO **)
					 netdev_priv(prDev)) : NULL;
	ASSERT(prDev);
	ASSERT(prGlueInfo);
	if (!prDev || !prGlueInfo) {
		DBGLOG(INIT, WARN,
		       "abnormal dev or skb: prDev(0x%p), prGlueInfo(0x%p)\n",
		       prDev, prGlueInfo);
		up(&g_halt_sem);
		return;
	}

	DBGLOG(INIT, INFO,
	       "wlanSetMulticastListWorkQueue prDev->flags:0x%x\n",
	       prDev->flags);

	if (prDev->flags & IFF_PROMISC)
		u4PacketFilter |= PARAM_PACKET_FILTER_PROMISCUOUS;

	if (prDev->flags & IFF_BROADCAST)
		u4PacketFilter |= PARAM_PACKET_FILTER_BROADCAST;

	if (prDev->flags & IFF_MULTICAST) {
		if ((prDev->flags & IFF_ALLMULTI)
		    || (netdev_mc_count(prDev) > MAX_NUM_GROUP_ADDR))
			u4PacketFilter |= PARAM_PACKET_FILTER_ALL_MULTICAST;
		else
			u4PacketFilter |= PARAM_PACKET_FILTER_MULTICAST;
	}

	up(&g_halt_sem);

	if (kalIoctl(prGlueInfo,
		     wlanoidSetCurrentPacketFilter,
		     &u4PacketFilter,
		     sizeof(u4PacketFilter), FALSE, FALSE, TRUE,
		     &u4SetInfoLen) != WLAN_STATUS_SUCCESS) {
		return;
	}

	if (u4PacketFilter & PARAM_PACKET_FILTER_MULTICAST) {
		/* Prepare multicast address list */
		struct netdev_hw_addr *ha;
		uint8_t *prMCAddrList = NULL;
		uint32_t i = 0;

		down(&g_halt_sem);
		if (g_u4HaltFlag) {
			up(&g_halt_sem);
			return;
		}

		prMCAddrList = kalMemAlloc(MAX_NUM_GROUP_ADDR * ETH_ALEN,
					   VIR_MEM_TYPE);
		if (!prMCAddrList) {
			DBGLOG(INIT, WARN, "prMCAddrList memory alloc fail!\n");
			return;
		}

		netdev_for_each_mc_addr(ha, prDev) {
			if (i < MAX_NUM_GROUP_ADDR) {
				kalMemCopy((prMCAddrList + i * ETH_ALEN),
					   GET_ADDR(ha), ETH_ALEN);
				i++;
			}
		}

		up(&g_halt_sem);

		kalIoctl(prGlueInfo,
			 wlanoidSetMulticastList, prMCAddrList, (i * ETH_ALEN),
			 FALSE, FALSE, TRUE, &u4SetInfoLen);

		kalMemFree(prMCAddrList, VIR_MEM_TYPE,
			   MAX_NUM_GROUP_ADDR * ETH_ALEN);
	}

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
void wlanSchedScanStoppedWorkQueue(struct work_struct *work)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct net_device *prDev = gPrDev;

	DBGLOG(SCN, INFO, "wlanSchedScanStoppedWorkQueue\n");
	prGlueInfo = (prDev != NULL) ? *((struct GLUE_INFO **)
					 netdev_priv(prDev)) : NULL;
	if (!prGlueInfo) {
		DBGLOG(SCN, INFO, "prGlueInfo == NULL unexpected\n");
		return;
	}

	/* 2. indication to cfg80211 */
	/* 20150205 change cfg80211_sched_scan_stopped to work queue due to
	 * sched_scan_mtx dead lock issue
	 */
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	cfg80211_sched_scan_stopped(priv_to_wiphy(prGlueInfo), 0);
#else
	cfg80211_sched_scan_stopped(priv_to_wiphy(prGlueInfo));
#endif
	DBGLOG(SCN, INFO,
	       "cfg80211_sched_scan_stopped event send done WorkQueue thread return from wlanSchedScanStoppedWorkQueue\n");
	return;

}

/* FIXME: Since we cannot sleep in the wlanSetMulticastList, we arrange
 * another workqueue for sleeping. We don't want to block
 * main_thread, so we can't let tx_thread to do this
 */

void p2pSetMulticastListWorkQueueWrapper(struct GLUE_INFO
		*prGlueInfo)
{

	ASSERT(prGlueInfo);

	if (!prGlueInfo) {
		DBGLOG(INIT, WARN,
		       "abnormal dev or skb: prGlueInfo(0x%p)\n", prGlueInfo);
		return;
	}
#if CFG_ENABLE_WIFI_DIRECT
	if (prGlueInfo->prAdapter->fgIsP2PRegistered)
		mtk_p2p_wext_set_Multicastlist(prGlueInfo);
#endif

} /* end of p2pSetMulticastListWorkQueueWrapper() */

/*----------------------------------------------------------------------------*/
/*
 * \brief This function is TX entry point of NET DEVICE.
 *
 * \param[in] prSkb  Pointer of the sk_buff to be sent
 * \param[in] prDev  Pointer to struct net_device
 *
 * \retval NETDEV_TX_OK - on success.
 * \retval NETDEV_TX_BUSY - on failure, packet will be discarded by upper layer.
 */
/*----------------------------------------------------------------------------*/
int wlanHardStartXmit(struct sk_buff *prSkb,
		      struct net_device *prDev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(prDev));
	uint8_t ucBssIndex;

	ASSERT(prSkb);
	ASSERT(prDev);
	ASSERT(prGlueInfo);

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			  netdev_priv(prDev);
	ASSERT(prNetDevPrivate->prGlueInfo == prGlueInfo);
	ucBssIndex = prNetDevPrivate->ucBssIdx;

#if CFG_SUPPORT_PASSPOINT
	if (prGlueInfo->fgIsDad) {
		/* kalPrint("[Passpoint R2] Due to ipv4_dad...TX is forbidden\n"
		 *         );
		 */
		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}
	if (prGlueInfo->fgIs6Dad) {
		/* kalPrint("[Passpoint R2] Due to ipv6_dad...TX is forbidden\n"
		 *          );
		 */
		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_CHIP_RESET_SUPPORT
	if (!wlanIsDriverReady(prGlueInfo)) {
		DBGLOG(INIT, WARN,
			"u4ReadyFlag:%u, kalIsResetting():%d, dropping the packet\n",
			prGlueInfo->u4ReadyFlag, kalIsResetting());

		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}
#endif


	kalResetPacket(prGlueInfo, (void *) prSkb);

	STATS_TX_TIME_ARRIVE(prSkb);

	if (kalHardStartXmit(prSkb, prDev, prGlueInfo,
			     ucBssIndex) == WLAN_STATUS_SUCCESS) {
		/* Successfully enqueue to Tx queue */
		/* Successfully enqueue to Tx queue */
		if (netif_carrier_ok(prDev))
			kalPerMonStart(prGlueInfo);
	}

	/* For Linux, we'll always return OK FLAG, because we'll free this skb
	 * by ourself
	 */
	return NETDEV_TX_OK;
}				/* end of wlanHardStartXmit() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief A method of struct net_device, to get the network interface
 *        statistical information.
 *
 * Whenever an application needs to get statistics for the interface, this
 * method is called. This happens, for example, when ifconfig or netstat -i
 * is run.
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \return net_device_stats buffer pointer.
 */
/*----------------------------------------------------------------------------*/
struct net_device_stats *wlanGetStats(IN struct net_device
				      *prDev)
{
	return (struct net_device_stats *)kalGetStats(prDev);
}				/* end of wlanGetStats() */

void wlanDebugInit(void)
{
	/* Set the initial debug level of each module */
#if DBG
	/* enable all */
	wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX, DBG_CLASS_MASK);
#else
#ifdef CFG_DEFAULT_DBG_LEVEL
	wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
			      CFG_DEFAULT_DBG_LEVEL);
#else
	wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
			      DBG_LOG_LEVEL_DEFAULT);
#endif
#endif /* DBG */

	LOG_FUNC("Reset ALL DBG module log level to DEFAULT!");

}

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
	struct GLUE_INFO *prGlueInfo = NULL;

	if (!prDev)
		return -ENXIO;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	INIT_DELAYED_WORK(&workq, wlanSetMulticastListWorkQueue);

	/* 20150205 work queue for sched_scan */
	INIT_DELAYED_WORK(&sched_workq,
			  wlanSchedScanStoppedWorkQueue);

	/* 20161024 init wow port setting */
#if CFG_WOW_SUPPORT
	kalWowInit(prGlueInfo);
#endif

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
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct sockaddr *sa = NULL;

	/**********************************************************************
	 * Check if kernel passes valid data to us                            *
	 **********************************************************************
	 */
	if (!ndev || !addr) {
		DBGLOG(INIT, ERROR, "Set macaddr with ndev(%d) and addr(%d)\n",
		       (ndev == NULL) ? 0 : 1, (addr == NULL) ? 0 : 1);
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
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(ndev));
	prAdapter = prGlueInfo->prAdapter;

	COPY_MAC_ADDR(prAdapter->prAisBssInfo->aucOwnMacAddr, sa->sa_data);
	COPY_MAC_ADDR(prGlueInfo->prDevHandler->dev_addr, sa->sa_data);
	DBGLOG(INIT, INFO, "Set connect random macaddr to " MACSTR ".\n",
	       MAC2STR(prAdapter->prAisBssInfo->aucOwnMacAddr));

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanSetMacAddr() */

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
	struct GLUE_INFO *prGlueInfo = NULL;
	ASSERT(prDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	napi_enable(&prGlueInfo->rNapi);
#endif /* CFG_RX_NAPI_SUPPORT */

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
	struct GLUE_INFO *prGlueInfo = NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));

	/* CFG80211 down */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prGlueInfo->prScanRequest) {
		kalCfg80211ScanDone(prGlueInfo->prScanRequest, TRUE);
		prGlueInfo->prScanRequest = NULL;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
	/* zero clear old acs information */
	kalMemZero(&(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo),
		sizeof(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo));
	wlanInitChnLoadInfoChannelList(prGlueInfo->prAdapter);
#endif

	netif_tx_stop_all_queues(prDev);

#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	if (skb_queue_len(&prGlueInfo->rRxNapiSkbQ)) {

		struct sk_buff *skb;

		DBGLOG(INIT, WARN, "NAPI Remain pkts %d\n",
			skb_queue_len(&prGlueInfo->rRxNapiSkbQ));

		while ((skb = skb_dequeue(&prGlueInfo->rRxNapiSkbQ)) != NULL)
			kfree_skb(skb);
	}
	napi_disable(&prGlueInfo->rNapi);
#endif /* CFG_RX_NAPI_SUPPORT */

	return 0;		/* success */
}				/* end of wlanStop() */

#if CFG_SUPPORT_SNIFFER
static int wlanMonOpen(struct net_device *prDev)
{
	ASSERT(prDev);

	netif_tx_start_all_queues(prDev);

	return 0;		/* success */
}

static int wlanMonStop(struct net_device *prDev)
{
	ASSERT(prDev);

	netif_tx_stop_all_queues(prDev);

	return 0;		/* success */
}

static const struct net_device_ops wlan_mon_netdev_ops = {
	.ndo_open = wlanMonOpen,
	.ndo_stop = wlanMonStop,
};

void wlanMonWorkHandler(struct work_struct *work)
{
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = container_of(work, struct GLUE_INFO, monWork);

	if (prGlueInfo->fgIsEnableMon) {
		if (prGlueInfo->prMonDevHandler)
			return;
#if KERNEL_VERSION(3, 18, 0) <= LINUX_VERSION_CODE
		prGlueInfo->prMonDevHandler =
			alloc_netdev_mq(sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
					NIC_MONITOR_INF_NAME,
					NET_NAME_PREDICTABLE, ether_setup,
					CFG_MAX_TXQ_NUM);
#else
		prGlueInfo->prMonDevHandler =
			alloc_netdev_mq(sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
					NIC_MONITOR_INF_NAME,
					ether_setup, CFG_MAX_TXQ_NUM);
#endif
		if (prGlueInfo->prMonDevHandler == NULL) {
			DBGLOG(INIT, ERROR,
			       "wlanMonWorkHandler: Allocated prMonDevHandler context FAIL.\n");
			return;
		}

		((struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(
			 prGlueInfo->prMonDevHandler))->prGlueInfo = prGlueInfo;
		prGlueInfo->prMonDevHandler->type =
			ARPHRD_IEEE80211_RADIOTAP;
		prGlueInfo->prMonDevHandler->netdev_ops =
			&wlan_mon_netdev_ops;
		netif_carrier_off(prGlueInfo->prMonDevHandler);
		netif_tx_stop_all_queues(prGlueInfo->prMonDevHandler);
		kalResetStats(prGlueInfo->prMonDevHandler);

		if (register_netdev(prGlueInfo->prMonDevHandler) < 0) {
			DBGLOG(INIT, ERROR,
			       "wlanMonWorkHandler: Registered prMonDevHandler context FAIL.\n");
			free_netdev(prGlueInfo->prMonDevHandler);
			prGlueInfo->prMonDevHandler = NULL;
		}
		DBGLOG(INIT, INFO,
		       "wlanMonWorkHandler: Registered prMonDevHandler context DONE.\n");
	} else {
		if (prGlueInfo->prMonDevHandler) {
			unregister_netdev(prGlueInfo->prMonDevHandler);
			prGlueInfo->prMonDevHandler = NULL;
			DBGLOG(INIT, INFO,
			       "wlanMonWorkHandler: unRegistered prMonDevHandler context DONE.\n");
		}
	}
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief Update Channel table for cfg80211 for Wi-Fi Direct based on current
 *        country code
 *
 * \param[in] prGlueInfo      Pointer to glue info
 *
 * \return   none
 */
/*----------------------------------------------------------------------------*/
void wlanUpdateChannelTable(struct GLUE_INFO *prGlueInfo)
{
	uint8_t i, j;
	uint8_t ucNumOfChannel;
	struct RF_CHANNEL_INFO aucChannelList[ARRAY_SIZE(
			mtk_2ghz_channels) + ARRAY_SIZE(mtk_5ghz_channels)];

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
			     ARRAY_SIZE(mtk_2ghz_channels) + ARRAY_SIZE(
				     mtk_5ghz_channels),
			     &ucNumOfChannel, aucChannelList);

	/* 3. Enable specific channel based on domain channel list */
	for (i = 0; i < ucNumOfChannel; i++) {
		switch (aucChannelList[i].eBand) {
		case BAND_2G4:
			for (j = 0; j < ARRAY_SIZE(mtk_2ghz_channels); j++) {
				if (mtk_2ghz_channels[j].hw_value ==
				    aucChannelList[i].ucChannelNum) {
					mtk_2ghz_channels[j].flags &=
						~IEEE80211_CHAN_DISABLED;
					mtk_2ghz_channels[j].orig_flags &=
						~IEEE80211_CHAN_DISABLED;
					break;
				}
			}
			break;

		case BAND_5G:
			for (j = 0; j < ARRAY_SIZE(mtk_5ghz_channels); j++) {
				if (mtk_5ghz_channels[j].hw_value ==
				    aucChannelList[i].ucChannelNum) {
					mtk_5ghz_channels[j].flags &=
						~IEEE80211_CHAN_DISABLED;
					mtk_5ghz_channels[j].orig_flags &=
						~IEEE80211_CHAN_DISABLED;
					break;
				}
			}
			break;

		default:
			DBGLOG(INIT, WARN, "Unknown band %d\n",
			       aucChannelList[i].eBand);
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
static int32_t wlanNetRegister(struct wireless_dev *prWdev)
{
	struct GLUE_INFO *prGlueInfo;
	int32_t i4DevIdx = -1;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prWdev);

	do {
		if (!prWdev)
			break;

		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(prWdev->wiphy);
		prAdapter = prGlueInfo->prAdapter;
		i4DevIdx = wlanGetDevIdx(prWdev->netdev);
		if (i4DevIdx < 0) {
			DBGLOG(INIT, ERROR, "net_device number exceeds!\n");
			break;
		}

		if (prAdapter && prAdapter->rWifiVar.ucWow)
			kalInitDevWakeup(prGlueInfo->prAdapter,
				wiphy_dev(prWdev->wiphy));

		if (register_netdev(prWdev->netdev) < 0) {
			DBGLOG(INIT, ERROR, "Register net_device failed\n");
			wlanClearDevIdx(prWdev->netdev);
			i4DevIdx = -1;
		}

#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
#if IS_ENABLED(CFG_GRO_SUPPORT)
		/* Register GRO function to kernel */
		prWdev->netdev->features |= NETIF_F_GRO;
		prWdev->netdev->hw_features |= NETIF_F_GRO;
#endif /* CFG_GRO_SUPPORT */
		netif_napi_add(prWdev->netdev, &prGlueInfo->rNapi,
			kalRxNapiPoll, NAPI_POLL_WEIGHT);
		skb_queue_head_init(&prGlueInfo->rRxNapiSkbQ);
		if (prGlueInfo->prAdapter->rWifiVar.ucRxNapiEnable)
			kalRxNapiSetEnable(prGlueInfo, TRUE);
#endif /* CFG_RX_NAPI_SUPPORT */

#if 1
		prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
				  netdev_priv(prGlueInfo->prDevHandler);
		ASSERT(prNetDevPrivate->prGlueInfo == prGlueInfo);
		prNetDevPrivate->ucBssIdx =
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
#if CFG_ENABLE_UNIFY_WIPHY
		prNetDevPrivate->ucIsP2p = FALSE;
#endif
		wlanBindBssIdxToNetInterface(prGlueInfo,
			     prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex,
			     (void *) prWdev->netdev);
#else
		wlanBindBssIdxToNetInterface(prGlueInfo,
			     prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex,
			     (void *) prWdev->netdev);
		/* wlanBindNetInterface(prGlueInfo, NET_DEV_WLAN_IDX,
		 *                      (PVOID)prWdev->netdev);
		 */
#endif
		if (i4DevIdx != -1)
			prGlueInfo->fgIsRegistered = TRUE;
	} while (FALSE);

	return i4DevIdx;	/* success */
}				/* end of wlanNetRegister() */


/*----------------------------------------------------------------------------*/
/*!
 * \brief Unregister the device from the kernel
 *
 * \param[in] prWdev      Pointer to struct net_device.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
static void wlanNetUnregister(struct wireless_dev *prWdev)
{
	struct GLUE_INFO *prGlueInfo;

	if (!prWdev) {
		DBGLOG(INIT, ERROR, "The device context is NULL\n");
		return;
	}

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(prWdev->wiphy);

#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	kalRxNapiSetEnable(prGlueInfo, FALSE);
	netif_napi_del(&prGlueInfo->rNapi);
#endif /* CFG_RX_NAPI_SUPPORT */

	wlanClearDevIdx(prWdev->netdev);
	unregister_netdev(prWdev->netdev);

	prGlueInfo->fgIsRegistered = FALSE;

#if CFG_SUPPORT_SNIFFER
	if (prGlueInfo->prMonDevHandler) {
		unregister_netdev(prGlueInfo->prMonDevHandler);
		/* FIXME: Why not free_netdev()? */
		prGlueInfo->prMonDevHandler = NULL;
	}
	prGlueInfo->fgIsEnableMon = FALSE;
#endif

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

#if CFG_ENABLE_UNIFY_WIPHY
const struct net_device_ops *wlanGetNdevOps(void)
{
	return &wlan_netdev_ops;
}
#endif

static void wlanCreateWirelessDevice(void)
{
	struct wiphy *prWiphy = NULL;
	struct wireless_dev *prWdev = NULL;
	unsigned int u4SupportSchedScanFlag = 0;

	/* 4 <1.1> Create wireless_dev */
	prWdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!prWdev) {
		DBGLOG(INIT, ERROR,
		       "Allocating memory to wireless_dev context failed\n");
		return;
	}
	/* 4 <1.2> Create wiphy */
#if CFG_ENABLE_UNIFY_WIPHY
	prWiphy = wiphy_new(&mtk_cfg_ops, sizeof(struct GLUE_INFO));
#else
	prWiphy = wiphy_new(&mtk_wlan_ops,
			    sizeof(struct GLUE_INFO));
#endif

	if (!prWiphy) {
		DBGLOG(INIT, ERROR,
		       "Allocating memory to wiphy device failed\n");
		goto free_wdev;
	}

	/* 4 <1.3> configure wireless_dev & wiphy */
	prWdev->iftype = NL80211_IFTYPE_STATION;
	prWiphy->iface_combinations = p_mtk_iface_combinations_sta;
	prWiphy->n_iface_combinations =
		mtk_iface_combinations_sta_num;
	prWiphy->max_scan_ssids = SCN_SSID_MAX_NUM +
				  1; /* include one wildcard ssid */
	prWiphy->max_scan_ie_len = 512;
#if CFG_SUPPORT_SCHED_SCAN
	prWiphy->max_sched_scan_ssids     =
		CFG_SCAN_HIDDEN_SSID_MAX_NUM;
	prWiphy->max_match_sets           =
		CFG_SCAN_SSID_MATCH_MAX_NUM;
	prWiphy->max_sched_scan_ie_len    = CFG_CFG80211_IE_BUF_LEN;
	u4SupportSchedScanFlag            =
		WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
#endif /* CFG_SUPPORT_SCHED_SCAN */
	prWiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
				   BIT(NL80211_IFTYPE_ADHOC);
	prWiphy->bands[KAL_BAND_2GHZ] = &mtk_band_2ghz;
	/* always assign 5Ghz bands here, if the chip is not support 5Ghz,
	 *  bands[KAL_BAND_5GHZ] will be assign to NULL
	 */
	prWiphy->bands[KAL_BAND_5GHZ] = &mtk_band_5ghz;
	prWiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	prWiphy->cipher_suites = (const u32 *)mtk_cipher_suites;
	prWiphy->n_cipher_suites = ARRAY_SIZE(mtk_cipher_suites);
	prWiphy->flags = WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL
			| u4SupportSchedScanFlag;

#if CFG_SUPPORT_802_11R && CFG_SUPPORT_CFG80211_AUTH
	prWiphy->features |= NL80211_FEATURE_DS_PARAM_SET_IE_IN_PROBES;
	prWiphy->features |= NL80211_FEATURE_QUIET;
	prWiphy->features |= NL80211_FEATURE_TX_POWER_INSERTION;
#endif /* CFG_SUPPORT_ROAMING */

#if (CFG_SUPPORT_ROAMING == 1)
	prWiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
#endif /* CFG_SUPPORT_ROAMING */

#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	prWiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#else
	prWiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#if (CFG_SUPPORT_DFS_MASTER == 1)
	prWiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
	prWiphy->max_num_csa_counters = 2;
#endif
#endif /* CFG_SUPPORT_DFS_MASTER */
#endif
#if (CFG_SUPPORT_SAE == 1)
	prWiphy->features |= NL80211_FEATURE_SAE;
#endif /* CFG_SUPPORT_DFS_MASTER */

	cfg80211_regd_set_wiphy(prWiphy);

#if (CFG_SUPPORT_TDLS == 1)
	TDLSEX_WIPHY_FLAGS_INIT(prWiphy->flags);
#endif /* CFG_SUPPORT_TDLS */
	prWiphy->max_remain_on_channel_duration = 5000;
	prWiphy->mgmt_stypes = mtk_cfg80211_ais_default_mgmt_stypes;

#if (CFG_SUPPORT_SCAN_RANDOM_MAC && \
	(KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE))
	prWiphy->features |= NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;
	prWiphy->features |= NL80211_FEATURE_SCHED_SCAN_RANDOM_MAC_ADDR;
#endif

#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
	prWiphy->vendor_commands = mtk_wlan_vendor_ops;
	prWiphy->n_vendor_commands = sizeof(mtk_wlan_vendor_ops) /
				     sizeof(struct wiphy_vendor_command);
	prWiphy->vendor_events = mtk_wlan_vendor_events;
	prWiphy->n_vendor_events = ARRAY_SIZE(
					   mtk_wlan_vendor_events);
#endif
	/* 4 <1.4> wowlan support */
#ifdef CONFIG_PM
#if KERNEL_VERSION(3, 11, 0) <= CFG80211_VERSION_CODE
	prWiphy->wowlan = &mtk_wlan_wowlan_support;
#else
	kalMemCopy(&prWiphy->wowlan, &mtk_wlan_wowlan_support,
		   sizeof(struct wiphy_wowlan_support));
#endif
#endif

#ifdef CONFIG_CFG80211_WEXT
	/* 4 <1.5> Use wireless extension to replace IOCTL */

#if CFG_ENABLE_UNIFY_WIPHY
	prWiphy->wext = NULL;
#else
	prWiphy->wext = &wext_handler_def;
#endif
#endif
	/* initialize semaphore for halt control */
	sema_init(&g_halt_sem, 1);

#if CFG_ENABLE_UNIFY_WIPHY
	prWiphy->iface_combinations = p_mtk_iface_combinations_p2p;
	prWiphy->n_iface_combinations =
		mtk_iface_combinations_p2p_num;

	prWiphy->interface_modes |= BIT(NL80211_IFTYPE_AP) |
				    BIT(NL80211_IFTYPE_P2P_CLIENT) |
				    BIT(NL80211_IFTYPE_P2P_GO) |
				    BIT(NL80211_IFTYPE_STATION);
	prWiphy->software_iftypes |= BIT(NL80211_IFTYPE_P2P_DEVICE);
	prWiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
			  WIPHY_FLAG_HAVE_AP_SME;
	prWiphy->ap_sme_capa = 1;
#endif
	if (wiphy_register(prWiphy) < 0) {
		DBGLOG(INIT, ERROR, "wiphy_register error\n");
		goto free_wiphy;
	}
	prWdev->wiphy = prWiphy;
	gprWdev = prWdev;
	DBGLOG(INIT, INFO, "Create wireless device success\n");
	return;

free_wiphy:
	wiphy_free(prWiphy);
free_wdev:
	kfree(prWdev);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Destroy all wdev (including the P2P device), and unregister wiphy
 *
 * \param (none)
 *
 * \return (none)
 *
 */
/*----------------------------------------------------------------------------*/
static void wlanDestroyAllWdev(void)
{
#if CFG_ENABLE_UNIFY_WIPHY
	/* There is only one wiphy, avoid that double free the wiphy */
	struct wiphy *wiphy = NULL;
#endif
#if CFG_ENABLE_WIFI_DIRECT
	int i = 0;
#endif

#if CFG_ENABLE_WIFI_DIRECT
	/* free P2P wdev */
	for (i = 0; i < KAL_P2P_NUM; i++) {
		if (gprP2pRoleWdev[i] == NULL)
			continue;
#if CFG_ENABLE_UNIFY_WIPHY
		if (gprP2pRoleWdev[i] == gprWdev) {
			/* This is AIS/AP Interface */
			gprP2pRoleWdev[i] = NULL;
			continue;
		}
#endif
		/* Do wiphy_unregister here. Take care the case that the
		 * gprP2pRoleWdev[i] is created by the cfg80211 add iface ops,
		 * And the base P2P dev is in the gprP2pWdev.
		 * Expect that new created gprP2pRoleWdev[i] is freed in
		 * unregister_netdev/mtk_vif_destructor. And gprP2pRoleWdev[i]
		 * is reset as gprP2pWdev in mtk_vif_destructor.
		 */
		if (gprP2pRoleWdev[i] == gprP2pWdev)
			gprP2pWdev = NULL;

#if CFG_ENABLE_UNIFY_WIPHY
		wiphy = gprP2pRoleWdev[i]->wiphy;
#else
		set_wiphy_dev(gprP2pRoleWdev[i]->wiphy, NULL);
		wiphy_unregister(gprP2pRoleWdev[i]->wiphy);
		wiphy_free(gprP2pRoleWdev[i]->wiphy);
#endif

		kfree(gprP2pRoleWdev[i]);
		gprP2pRoleWdev[i] = NULL;
	}

	if (gprP2pWdev != NULL) {
		/* This case is that gprP2pWdev isn't equal to gprP2pRoleWdev[0]
		 * . The gprP2pRoleWdev[0] is created in the p2p cfg80211 add
		 * iface ops. The two wdev use the same wiphy. Don't double
		 * free the same wiphy.
		 * This part isn't expect occur. Because p2pNetUnregister should
		 * unregister_netdev the new created wdev, and gprP2pRoleWdev[0]
		 * is reset as gprP2pWdev.
		 */
#if CFG_ENABLE_UNIFY_WIPHY
		wiphy = gprP2pWdev->wiphy;
#endif

		kfree(gprP2pWdev);
		gprP2pWdev = NULL;
	}
#endif	/* CFG_ENABLE_WIFI_DIRECT */

	/* free AIS wdev */
	if (gprWdev) {
#if CFG_ENABLE_UNIFY_WIPHY
		wiphy = gprWdev->wiphy;
#else
		/* trunk doesn't do set_wiphy_dev, but trunk-ce1 does. */
		/* set_wiphy_dev(gprWdev->wiphy, NULL); */
		wiphy_unregister(gprWdev->wiphy);
		wiphy_free(gprWdev->wiphy);
#endif
		kfree(gprWdev);
		gprWdev = NULL;
	}

#if CFG_ENABLE_UNIFY_WIPHY
	/* unregister & free wiphy */
	if (wiphy) {
		/* set_wiphy_dev(wiphy, NULL): set the wiphy->dev->parent = NULL
		 * The trunk-ce1 does this, but the trunk seems not.
		 */
		/* set_wiphy_dev(wiphy, NULL); */
		wiphy_unregister(wiphy);
		wiphy_free(wiphy);
	}
#endif
}

void wlanWakeLockInit(struct GLUE_INFO *prGlueInfo)
{
#ifdef CONFIG_ANDROID
	KAL_WAKE_LOCK_INIT(NULL, &prGlueInfo->rIntrWakeLock,
			   "WLAN interrupt");
	KAL_WAKE_LOCK_INIT(NULL, &prGlueInfo->rTimeoutWakeLock,
			   "WLAN timeout");
#endif
}

void wlanWakeLockUninit(struct GLUE_INFO *prGlueInfo)
{
#if CFG_ENABLE_WAKE_LOCK
	if (KAL_WAKE_LOCK_ACTIVE(NULL, &prGlueInfo->rIntrWakeLock))
		KAL_WAKE_UNLOCK(NULL, &prGlueInfo->rIntrWakeLock);
	KAL_WAKE_LOCK_DESTROY(NULL, &prGlueInfo->rIntrWakeLock);

	if (KAL_WAKE_LOCK_ACTIVE(NULL,
				 &prGlueInfo->rTimeoutWakeLock))
		KAL_WAKE_UNLOCK(NULL, &prGlueInfo->rTimeoutWakeLock);
	KAL_WAKE_LOCK_DESTROY(NULL, &prGlueInfo->rTimeoutWakeLock);
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief A method for creating Linux NET4 struct net_device object and the
 *        private data(prGlueInfo and prAdapter). Setup the IO address to the
 *        HIF.
 *        Assign the function pointer to the net_device object
 *
 * \param[in] pvData     Memory address for the device
 *
 * \retval Not null      The wireless_dev object.
 * \retval NULL          Fail to create wireless_dev object
 */
/*----------------------------------------------------------------------------*/
static struct lock_class_key rSpinKey[SPIN_LOCK_NUM];
static struct wireless_dev *wlanNetCreate(void *pvData,
		void *pvDriverData)
{
	struct wireless_dev *prWdev = gprWdev;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t i;
	struct device *prDev;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	struct mt66xx_chip_info *prChipInfo;
#if CFG_ENABLE_UNIFY_WIPHY
	struct wiphy *prWiphy = NULL;
#endif

	uint8_t *prInfName = NULL;

	if (prWdev == NULL) {
		DBGLOG(INIT, ERROR,
		       "No wireless dev exist, abort power on\n");
		return NULL;
	}

#if CFG_ENABLE_UNIFY_WIPHY
	/* The gprWdev is created at initWlan() and isn't reset when the
	 * disconnection occur. That cause some issue.
	 */
	prWiphy = prWdev->wiphy;
	memset(prWdev, 0, sizeof(struct wireless_dev));
	prWdev->wiphy = prWiphy;
	prWdev->iftype = NL80211_IFTYPE_STATION;
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	/* XXX: ref from cfg80211_regd_set_wiphy().
	 * The error case: Sometimes after unplug/plug usb, the wlan0 STA can't
	 * scan correctly (FW doesn't do scan). The usb_probe message:
	 * "mtk_reg_notify:(RLM ERROR) Invalid REG state happened. state = 0x6".
	 */
	if (rlmDomainGetCtrlState() == REGD_STATE_INVALID)
		rlmDomainResetCtrlInfo(TRUE);
#endif
#endif	/* CFG_ENABLE_UNIFY_WIPHY */

	/* 4 <1.3> co-relate wiphy & prDev */
	glGetDev(pvData, &prDev);
	if (!prDev) {
		DBGLOG(INIT, ERROR, "unable to get struct dev for wlan\n");
		return NULL;
	}
	/* Some kernel API (ex: cfg80211_get_drvinfo) will use wiphy_dev().
	 * Without set_wiphy_dev(prWdev->wiphy, prDev), those API will crash.
	 */
	set_wiphy_dev(prWdev->wiphy, prDev);

	/* 4 <2> Create Glue structure */
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(prWdev->wiphy);
	kalMemZero(prGlueInfo, sizeof(struct GLUE_INFO));

	/* 4 <2.1> Create Adapter structure */
	prAdapter = (struct ADAPTER *) wlanAdapterCreate(
			    prGlueInfo);

	if (!prAdapter) {
		DBGLOG(INIT, ERROR,
		       "Allocating memory to adapter failed\n");
		glClearHifInfo(prGlueInfo);
		goto netcreate_err;
	}

	prChipInfo = ((struct mt66xx_hif_driver_data *)
		      pvDriverData)->chip_info;
	prAdapter->chip_info = prChipInfo;
	prGlueInfo->prAdapter = prAdapter;

	/* 4 <3> Initialize Glue structure */
	/* 4 <3.1> Create net device */

#ifdef CFG_DRIVER_INF_NAME_CHANGE

	if (kalStrLen(gprifnamesta) > 0) {
		prInfName = kalStrCat(gprifnamesta, "%d");
		DBGLOG(INIT, WARN, "Station ifname customized, use %s\n",
		       prInfName);
	} else
#endif /* CFG_DRIVER_INF_NAME_CHANGE */
		prInfName = NIC_INF_NAME;

#if KERNEL_VERSION(3, 18, 0) <= LINUX_VERSION_CODE
	prGlueInfo->prDevHandler =
		alloc_netdev_mq(sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
			prInfName, NET_NAME_PREDICTABLE, ether_setup,
			CFG_MAX_TXQ_NUM);
#else
	prGlueInfo->prDevHandler =
		alloc_netdev_mq(sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
				prInfName,
				ether_setup, CFG_MAX_TXQ_NUM);
#endif

	if (!prGlueInfo->prDevHandler) {
		DBGLOG(INIT, ERROR,
		       "Allocating memory to net_device context failed\n");
		goto netcreate_err;
	}
	DBGLOG(INIT, INFO, "net_device prDev(0x%p) allocated\n",
	       prGlueInfo->prDevHandler);

	/* Device can help us to save at most 3000 packets, after we stopped
	** queue
	*/
	prGlueInfo->prDevHandler->tx_queue_len = 3000;

	/* 4 <3.1.1> Initialize net device varaiables */
#if 1
	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			  netdev_priv(prGlueInfo->prDevHandler);
	prNetDevPrivate->prGlueInfo = prGlueInfo;
#else
	*((struct GLUE_INFO **) netdev_priv(
		  prGlueInfo->prDevHandler)) = prGlueInfo;
#endif
	prGlueInfo->prDevHandler->needed_headroom +=
	NIC_TX_HEAD_ROOM;
/*
		NIC_TX_DESC_AND_PADDING_LENGTH +
		prChipInfo->txd_append_size;
*/
	prGlueInfo->prDevHandler->netdev_ops = &wlan_netdev_ops;
#ifdef CONFIG_WIRELESS_EXT
	prGlueInfo->prDevHandler->wireless_handlers =
		&wext_handler_def;
#endif
	netif_carrier_off(prGlueInfo->prDevHandler);
	netif_tx_stop_all_queues(prGlueInfo->prDevHandler);
	kalResetStats(prGlueInfo->prDevHandler);

#if CFG_SUPPORT_SNIFFER
	INIT_WORK(&(prGlueInfo->monWork), wlanMonWorkHandler);
#endif

	/* 4 <3.1.2> co-relate with wiphy bi-directionally */
	prGlueInfo->prDevHandler->ieee80211_ptr = prWdev;

	prWdev->netdev = prGlueInfo->prDevHandler;

	/* 4 <3.1.3> co-relate net device & prDev */
	SET_NETDEV_DEV(prGlueInfo->prDevHandler,
		       wiphy_dev(prWdev->wiphy));

	/* 4 <3.1.4> set device to glue */
	prGlueInfo->prDev = prDev;

	/* 4 <3.2> Initialize glue variables */
	prGlueInfo->eParamMediaStateIndicated =
		PARAM_MEDIA_STATE_DISCONNECTED;
	prGlueInfo->ePowerState = ParamDeviceStateD0;
	prGlueInfo->fgIsRegistered = FALSE;
	prGlueInfo->prScanRequest = NULL;
	prGlueInfo->prSchedScanRequest = NULL;


#if CFG_SUPPORT_PASSPOINT
	/* Init DAD */
	prGlueInfo->fgIsDad = FALSE;
	prGlueInfo->fgIs6Dad = FALSE;
	kalMemZero(prGlueInfo->aucDADipv4, 4);
	kalMemZero(prGlueInfo->aucDADipv6, 16);
#endif /* CFG_SUPPORT_PASSPOINT */

	init_completion(&prGlueInfo->rScanComp);
	init_completion(&prGlueInfo->rHaltComp);
	init_completion(&prGlueInfo->rPendComp);

#if CFG_SUPPORT_MULTITHREAD
	init_completion(&prGlueInfo->rHifHaltComp);
	init_completion(&prGlueInfo->rRxHaltComp);
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

	for (i = 0; i < MUTEX_NUM; i++) {
		mutex_init(&prGlueInfo->arMutex[i]);
		lockdep_set_subclass(&prGlueInfo->arMutex[i], i);
	}

	/* initialize semaphore for ioctl */
	sema_init(&prGlueInfo->ioctl_sem, 1);

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
	/* initialize SDIO read-write pattern control */
	prGlueInfo->fgEnSdioTestPattern = FALSE;
	prGlueInfo->fgIsSdioTestInitialized = FALSE;
#endif

	/* 4 <8> Init Queues */
	init_waitqueue_head(&prGlueInfo->waitq);
	QUEUE_INITIALIZE(&prGlueInfo->rCmdQueue);
	prGlueInfo->i4TxPendingCmdNum = 0;
	QUEUE_INITIALIZE(&prGlueInfo->rTxQueue);
	glSetHifInfo(prGlueInfo, (unsigned long) pvData);

	/* Init wakelock */
	wlanWakeLockInit(prGlueInfo);

	/* main thread is created in this function */
#if CFG_SUPPORT_MULTITHREAD
	init_waitqueue_head(&prGlueInfo->waitq_rx);
	init_waitqueue_head(&prGlueInfo->waitq_hif);

	prGlueInfo->u4TxThreadPid = 0xffffffff;
	prGlueInfo->u4RxThreadPid = 0xffffffff;
	prGlueInfo->u4HifThreadPid = 0xffffffff;
#endif

	return prWdev;

netcreate_err:
	if (prAdapter != NULL) {
		wlanAdapterDestroy(prAdapter);
		prAdapter = NULL;
	}

	if (prGlueInfo->prDevHandler != NULL) {
		free_netdev(prGlueInfo->prDevHandler);
		prGlueInfo->prDevHandler = NULL;
	}

	return NULL;
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
static void wlanNetDestroy(struct wireless_dev *prWdev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prWdev);

	if (!prWdev) {
		DBGLOG(INIT, ERROR, "The device context is NULL\n");
		return;
	}

	/* prGlueInfo is allocated with net_device */
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(prWdev->wiphy);
	ASSERT(prGlueInfo);

	/* prWdev: base AIS dev
	 * Because the interface dev (ex: usb_device) would be free
	 * after un-plug event. Should set the wiphy->dev->parent which
	 * pointer to the interface dev to NULL. Otherwise, the corresponding
	 * system operation (poweroff, suspend) might reference it.
	 * set_wiphy_dev(wiphy, NULL): set the wiphy->dev->parent = NULL
	 * The set_wiphy_dev(prWdev->wiphy, prDev) is done in wlanNetCreate.
	 * But that is after wiphy_register, and will cause exception in
	 * wiphy_unregister(), if do not set_wiphy_dev(wiphy, NULL).
	 */
	set_wiphy_dev(prWdev->wiphy, NULL);

	/* destroy kal OS timer */
	kalCancelTimer(prGlueInfo);

	glClearHifInfo(prGlueInfo);

	wlanAdapterDestroy(prGlueInfo->prAdapter);
	prGlueInfo->prAdapter = NULL;

	/* Free net_device and private data, which are allocated by
	 * alloc_netdev().
	 */
	free_netdev(prWdev->netdev);

	/* gPrDev is assigned by prGlueInfo->prDevHandler,
	 * set NULL to this global variable.
	 */
	gPrDev = NULL;

}				/* end of wlanNetDestroy() */

void wlanSetSuspendMode(struct GLUE_INFO *prGlueInfo,
			u_int8_t fgEnable)
{
	struct net_device *prDev = NULL;
#if CFG_SUPPORT_DROP_MC_PACKET
	uint32_t u4PacketFilter = 0;
	uint32_t u4SetInfoLen = 0;
#endif

	if (!prGlueInfo)
		return;

	prDev = prGlueInfo->prDevHandler;
	if (!prDev)
		return;

#if CFG_SUPPORT_DROP_MC_PACKET
	/* new filter should not include p2p mask */
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	u4PacketFilter = prGlueInfo->prAdapter->u4OsPacketFilter &
			 (~PARAM_PACKET_FILTER_P2P_MASK);
#endif
	if (kalIoctl(prGlueInfo,
		     wlanoidSetCurrentPacketFilter,
		     &u4PacketFilter,
		     sizeof(u4PacketFilter), FALSE, FALSE, TRUE,
		     &u4SetInfoLen) != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "set packet filter failed.\n");
#endif
	kalSetNetAddressFromInterface(prGlueInfo, prDev, fgEnable);
	wlanNotifyFwSuspend(prGlueInfo, prDev, fgEnable);
}

#if CFG_ENABLE_EARLY_SUSPEND
static struct early_suspend wlan_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
};

static void wlan_early_suspend(struct early_suspend *h)
{
	struct net_device *prDev = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	/* 4 <1> Sanity Check */
	if ((u4WlanDevNum == 0)
	    && (u4WlanDevNum > CFG_MAX_WLAN_DEVICES)) {
		DBGLOG(INIT, ERROR,
		       "wlanLateResume u4WlanDevNum==0 invalid!!\n");
		return;
	}

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	if (!prDev)
		return;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	if (!prGlueInfo)
		return;

	DBGLOG(INIT, INFO, "********<%s>********\n", __func__);

	if (prGlueInfo->fgIsInSuspendMode == TRUE) {
		DBGLOG(INIT, INFO, "%s: Already in suspend mode, SKIP!\n",
		       __func__);
		return;
	}

	prGlueInfo->fgIsInSuspendMode = TRUE;

	wlanSetSuspendMode(prGlueInfo, TRUE);
	p2pSetSuspendMode(prGlueInfo, TRUE);
}

static void wlan_late_resume(struct early_suspend *h)
{
	struct net_device *prDev = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	/* 4 <1> Sanity Check */
	if ((u4WlanDevNum == 0)
	    && (u4WlanDevNum > CFG_MAX_WLAN_DEVICES)) {
		DBGLOG(INIT, ERROR,
		       "wlanLateResume u4WlanDevNum==0 invalid!!\n");
		return;
	}

	prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	if (!prDev)
		return;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	if (!prGlueInfo)
		return;

	DBGLOG(INIT, INFO, "********<%s>********\n", __func__);

	if (prGlueInfo->fgIsInSuspendMode == FALSE) {
		DBGLOG(INIT, INFO, "%s: Not in suspend mode, SKIP!\n",
		       __func__);
		return;
	}

	prGlueInfo->fgIsInSuspendMode = FALSE;

	/* 4 <2> Set suspend mode for each network */
	wlanSetSuspendMode(prGlueInfo, FALSE);
	p2pSetSuspendMode(prGlueInfo, FALSE);
}
#endif

#if (CFG_MTK_ANDROID_WMT || WLAN_INCLUDE_PROC)

int set_p2p_mode_handler(struct net_device *netdev,
			 struct PARAM_CUSTOM_P2P_SET_STRUCT p2pmode)
{
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(netdev));
	struct PARAM_CUSTOM_P2P_SET_STRUCT rSetP2P;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	if (!prGlueInfo)
		return -1;

#if (CFG_MTK_ANDROID_WMT)
	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(INIT, ERROR, "adapter is not ready\n");
		return -1;
	}
#endif /*CFG_MTK_ANDROID_WMT*/

	rSetP2P.u4Enable = p2pmode.u4Enable;
	rSetP2P.u4Mode = p2pmode.u4Mode;

	if ((!rSetP2P.u4Enable) && (kalIsResetting() == FALSE))
		p2pNetUnregister(prGlueInfo, FALSE);

	rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode,
			(void *) &rSetP2P,
			sizeof(struct PARAM_CUSTOM_P2P_SET_STRUCT),
			FALSE, FALSE, TRUE, &u4BufLen);

	DBGLOG(INIT, INFO, "set_p2p_mode_handler ret = 0x%08x\n",
	       (uint32_t) rWlanStatus);


	/* Need to check fgIsP2PRegistered, in case of whole chip reset.
	 * in this case, kalIOCTL return success always,
	 * and prGlueInfo->prP2PInfo[0] may be NULL
	 */
	if ((rSetP2P.u4Enable)
	    && (prGlueInfo->prAdapter->fgIsP2PRegistered)
	    && (kalIsResetting() == FALSE))
		p2pNetRegister(prGlueInfo, FALSE);

	return 0;
}

#endif

#if CFG_SUPPORT_EASY_DEBUG
/*----------------------------------------------------------------------------*/
/*!
 * \brief parse config from wifi.cfg
 *
 * \param[in] prAdapter
 *
 * \retval VOID
 */
/*----------------------------------------------------------------------------*/
void wlanGetParseConfig(struct ADAPTER *prAdapter)
{
	uint8_t *pucConfigBuf;
	uint32_t u4ConfigReadLen;

	wlanCfgInit(prAdapter, NULL, 0, 0);
	pucConfigBuf = (uint8_t *) kalMemAlloc(
			       WLAN_CFG_FILE_BUF_SIZE, VIR_MEM_TYPE);
	kalMemZero(pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE);
	u4ConfigReadLen = 0;
	if (pucConfigBuf) {
		if (kalRequestFirmware("wifi.cfg", pucConfigBuf,
			   WLAN_CFG_FILE_BUF_SIZE, &u4ConfigReadLen,
			   prAdapter->prGlueInfo->prDev) == 0) {
			/* ToDo:: Nothing */
		} else if (kalReadToFile("/data/misc/wifi.cfg",
			   pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE,
			   &u4ConfigReadLen) == 0) {
			/* ToDo:: Nothing */
		} else if (kalReadToFile("/data/misc/wifi/wifi.cfg",
			   pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE,
			   &u4ConfigReadLen) == 0) {
			/* ToDo:: Nothing */
		} else if (kalReadToFile("/storage/sdcard0/wifi.cfg",
			   pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE,
			   &u4ConfigReadLen) == 0) {
			/* ToDo:: Nothing */
		}

		if (pucConfigBuf[0] != '\0' && u4ConfigReadLen > 0)
			wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigReadLen,
				     TRUE);

		kalMemFree(pucConfigBuf, VIR_MEM_TYPE,
			   WLAN_CFG_FILE_BUF_SIZE);
	}			/* pucConfigBuf */
}


#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief get config from wifi.cfg
 *
 * \param[in] prAdapter
 *
 * \retval VOID
 */
/*----------------------------------------------------------------------------*/
void wlanGetConfig(struct ADAPTER *prAdapter)
{
	uint8_t *pucConfigBuf;
	uint32_t u4ConfigReadLen;

	wlanCfgInit(prAdapter, NULL, 0, 0);
	pucConfigBuf = (uint8_t *) kalMemAlloc(
			       WLAN_CFG_FILE_BUF_SIZE, VIR_MEM_TYPE);
	kalMemZero(pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE);
	u4ConfigReadLen = 0;
	if (pucConfigBuf) {
#ifdef CFG_FILE_NAME
		if (kalRequestFirmware(CFG_FILE_NAME, pucConfigBuf,
			   WLAN_CFG_FILE_BUF_SIZE, &u4ConfigReadLen,
			   prAdapter->prGlueInfo->prDev) == 0) {
		} else
#endif
		if (kalRequestFirmware("wifi.cfg", pucConfigBuf,
			   WLAN_CFG_FILE_BUF_SIZE, &u4ConfigReadLen,
			   prAdapter->prGlueInfo->prDev) == 0) {
			/* ToDo:: Nothing */
		} else if (kalReadToFile("/data/misc/wifi/wifi.cfg",
			   pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE,
			   &u4ConfigReadLen) == 0) {
			/* ToDo:: Nothing */
		} else if (kalReadToFile("/storage/sdcard0/wifi.cfg",
			   pucConfigBuf, WLAN_CFG_FILE_BUF_SIZE,
			   &u4ConfigReadLen) == 0) {
			/* ToDo:: Nothing */
		}

		if (pucConfigBuf[0] != '\0' && u4ConfigReadLen > 0)
			wlanCfgInit(prAdapter,
				pucConfigBuf, u4ConfigReadLen, 0);

		kalMemFree(pucConfigBuf, VIR_MEM_TYPE,
			   WLAN_CFG_FILE_BUF_SIZE);
	}			/* pucConfigBuf */
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief this function send buffer bin EEPROB_MTxxxx.bin to FW.
 *
 * \param[in] prAdapter
 *
 * \retval WLAN_STATUS_SUCCESS Success
 * \retval WLAN_STATUS_FAILURE Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanDownloadBufferBin(struct ADAPTER *prAdapter)
{
	struct GLUE_INFO *prGlueInfo = NULL;
#if (CFG_FW_Report_Efuse_Address)
	uint16_t u2InitAddr = prAdapter->u4EfuseStartAddress;
#else
	uint16_t u2InitAddr = EFUSE_CONTENT_BUFFER_START;
#endif
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_CUSTOM_EFUSE_BUFFER_MODE *prSetEfuseBufModeInfo
			= NULL;
	uint32_t u4ContentLen;
	uint8_t *pucConfigBuf = NULL;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t chip_id;
	uint8_t aucEeprom[32];
	uint32_t retWlanStat = WLAN_STATUS_FAILURE;

#if CFG_EFUSE_AUTO_MODE_SUPPORT
	uint32_t u4Efuse_addr = 0;
	struct PARAM_CUSTOM_ACCESS_EFUSE *prAccessEfuseInfo
			= NULL;
#endif

	if (prAdapter->fgIsSupportPowerOnSendBufferModeCMD ==
	    TRUE) {
		DBGLOG(INIT, INFO, "Start Efuse Buffer Mode ..\n");
		DBGLOG(INIT, INFO, "ucEfuseBUfferModeCal is %x\n",
		       prAdapter->rWifiVar.ucEfuseBufferModeCal);

		prChipInfo = prAdapter->chip_info;
		chip_id = prChipInfo->chip_id;
		prGlueInfo = prAdapter->prGlueInfo;

		if (prGlueInfo == NULL || prGlueInfo->prDev == NULL)
			goto label_exit;

		/* allocate memory for buffer mode info */
		prSetEfuseBufModeInfo =
			(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE *)
			kalMemAlloc(sizeof(
				    struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
				    VIR_MEM_TYPE);
		if (prSetEfuseBufModeInfo == NULL)
			goto label_exit;
		kalMemZero(prSetEfuseBufModeInfo,
			   sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE));

#if CFG_EFUSE_AUTO_MODE_SUPPORT
		/* allocate memory for Access Efuse Info */
		prAccessEfuseInfo =
			(struct PARAM_CUSTOM_ACCESS_EFUSE *)
			kalMemAlloc(sizeof(
				    struct PARAM_CUSTOM_ACCESS_EFUSE),
				    VIR_MEM_TYPE);
		if (prAccessEfuseInfo == NULL)
			goto label_exit;
		kalMemZero(prAccessEfuseInfo,
			   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));

		if (prAdapter->rWifiVar.ucEfuseBufferModeCal == LOAD_AUTO) {
			prAccessEfuseInfo->u4Address = (u4Efuse_addr /
				EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
			rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseRead,
				prAccessEfuseInfo,
				sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				TRUE, TRUE, TRUE, &u4BufLen);
			if (prGlueInfo->prAdapter->aucEepromVaule[1]
				== (chip_id>>8)) {
				prAdapter->rWifiVar.ucEfuseBufferModeCal
					= LOAD_EFUSE;
				DBGLOG(INIT, STATE,
					"[EFUSE AUTO] EFUSE Mode\n");
			} else {
				prAdapter->rWifiVar.ucEfuseBufferModeCal
					= LOAD_EEPROM_BIN;
				DBGLOG(INIT, STATE,
					"[EFUSE AUTO] Buffer Mode\n");
			}
		}
#endif

		if (prAdapter->rWifiVar.ucEfuseBufferModeCal
			== LOAD_EEPROM_BIN) {
			/* Buffer mode */
			/* Only in buffer mode need to access bin file */
			/* 1 <1> Load bin file*/
			pucConfigBuf = (uint8_t *) kalMemAlloc(
					MAX_EEPROM_BUFFER_SIZE, VIR_MEM_TYPE);
			if (pucConfigBuf == NULL)
				goto label_exit;

			kalMemZero(pucConfigBuf, MAX_EEPROM_BUFFER_SIZE);

			/* 1 <2> Construct EEPROM binary name */
			kalMemZero(aucEeprom, sizeof(aucEeprom));

			snprintf(aucEeprom, 32, "%s%x.bin",
				 apucEepromName[0], chip_id);

			/* 1 <3> Request buffer bin */
			if (kalRequestFirmware(aucEeprom, pucConfigBuf,
			    MAX_EEPROM_BUFFER_SIZE, &u4ContentLen,
			    prGlueInfo->prDev) == 0) {
				DBGLOG(INIT, INFO, "request file done\n");
			} else {
				DBGLOG(INIT, INFO, "can't find file\n");
				goto label_exit;
			}

			/* 1 <4> Send CMD with bin file content */
			prGlueInfo = prAdapter->prGlueInfo;

			/* Update contents in local table */
			kalMemCopy(uacEEPROMImage, pucConfigBuf,
				   MAX_EEPROM_BUFFER_SIZE);

			/* copy to the command buffer */
#if (CFG_FW_Report_Efuse_Address)
			u4ContentLen = (prAdapter->u4EfuseEndAddress) -
				       (prAdapter->u4EfuseStartAddress) + 1;
#else
			u4ContentLen = EFUSE_CONTENT_BUFFER_SIZE;
#endif
			if (u4ContentLen > MAX_EEPROM_BUFFER_SIZE)
				goto label_exit;
			kalMemCopy(prSetEfuseBufModeInfo->aBinContent,
				   &pucConfigBuf[u2InitAddr], u4ContentLen);

			prSetEfuseBufModeInfo->ucSourceMode = 1;
		} else {
			/* eFuse mode */
			/* Only need to tell FW the content from, contents are
			 * directly from efuse
			 */
			prSetEfuseBufModeInfo->ucSourceMode = 0;
		}
		prSetEfuseBufModeInfo->ucCmdType = 0x1 |
				   (prAdapter->rWifiVar.ucCalTimingCtrl << 4);
		prSetEfuseBufModeInfo->ucCount   =
			0xFF; /* ucCmdType 1 don't care the ucCount */

		rStatus = kalIoctl(prGlueInfo, wlanoidSetEfusBufferMode,
				(void *)prSetEfuseBufModeInfo,
				sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
				FALSE, TRUE, TRUE, &u4BufLen);
	}

	retWlanStat = WLAN_STATUS_SUCCESS;

label_exit:

	/* free memory */
	if (prSetEfuseBufModeInfo != NULL)
		kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE,
			   sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE));

	if (pucConfigBuf != NULL)
		kalMemFree(pucConfigBuf, VIR_MEM_TYPE,
			   MAX_EEPROM_BUFFER_SIZE);

#if CFG_EFUSE_AUTO_MODE_SUPPORT
	if (prAccessEfuseInfo != NULL)
		kalMemFree(prAccessEfuseInfo, VIR_MEM_TYPE,
			sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
#endif

	return retWlanStat;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief this function send buffer bin EEPROB_MTxxxx.bin to FW.
 *
 * \param[in] prAdapter
 *
 * \retval WLAN_STATUS_SUCCESS Success
 * \retval WLAN_STATUS_FAILURE Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanConnacDownloadBufferBin(struct ADAPTER
				     *prAdapter)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T
		*prSetEfuseBufModeInfo = NULL;
	uint32_t u4ContentLen;
	uint8_t *pucConfigBuf = NULL;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t chip_id;
	uint8_t aucEeprom[32];
	uint32_t retWlanStat = WLAN_STATUS_FAILURE;
	struct CAP_BUFFER_MODE_INFO_T *prBufferModeInfo = NULL;

#if CFG_EFUSE_AUTO_MODE_SUPPORT
	uint32_t u4Efuse_addr = 0;
	struct PARAM_CUSTOM_ACCESS_EFUSE *prAccessEfuseInfo
			= NULL;
#endif

	if (prAdapter->fgIsSupportPowerOnSendBufferModeCMD == FALSE)
		return WLAN_STATUS_SUCCESS;

	DBGLOG(INIT, INFO, "Start Efuse Buffer Mode ..\n");
	DBGLOG(INIT, INFO, "ucEfuseBUfferModeCal is %x\n",
	       prAdapter->rWifiVar.ucEfuseBufferModeCal);

	prChipInfo = prAdapter->chip_info;
	chip_id = prChipInfo->chip_id;
	prGlueInfo = prAdapter->prGlueInfo;
	prBufferModeInfo = &prAdapter->rBufferModeInfo;

	if (prGlueInfo == NULL || prGlueInfo->prDev == NULL)
		goto label_exit;

	/* allocate memory for buffer mode info */
	prSetEfuseBufModeInfo =
		(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T *)
		kalMemAlloc(sizeof(
			    struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T),
			    VIR_MEM_TYPE);
	if (prSetEfuseBufModeInfo == NULL)
		goto label_exit;
	kalMemZero(prSetEfuseBufModeInfo,
		   sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T));

#if CFG_EFUSE_AUTO_MODE_SUPPORT
	/* allocate memory for Access Efuse Info */
	prAccessEfuseInfo =
		(struct PARAM_CUSTOM_ACCESS_EFUSE *)
			kalMemAlloc(sizeof(
				struct PARAM_CUSTOM_ACCESS_EFUSE),
					VIR_MEM_TYPE);
	if (prAccessEfuseInfo == NULL)
		goto label_exit;
	kalMemZero(prAccessEfuseInfo,
		sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));

	if (prAdapter->rWifiVar.ucEfuseBufferModeCal == LOAD_AUTO) {
		prAccessEfuseInfo->u4Address = (u4Efuse_addr /
			EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
		rStatus = kalIoctl(prGlueInfo,
			wlanoidQueryProcessAccessEfuseRead,
			prAccessEfuseInfo,
			sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
			TRUE, TRUE, TRUE, &u4BufLen);
		if (prGlueInfo->prAdapter->aucEepromVaule[1]
			== (chip_id>>8)) {
			prAdapter->rWifiVar.ucEfuseBufferModeCal
				= LOAD_EFUSE;
			DBGLOG(INIT, STATE,
				"[EFUSE AUTO] EFUSE Mode\n");
		} else {
			prAdapter->rWifiVar.ucEfuseBufferModeCal
				= LOAD_EEPROM_BIN;
			DBGLOG(INIT, STATE,
				"[EFUSE AUTO] Buffer Mode\n");
		}
	}
#endif

	if (prAdapter->rWifiVar.ucEfuseBufferModeCal == LOAD_EEPROM_BIN) {
		/* Buffer mode */
		/* Only in buffer mode need to access bin file */
		/* 1 <1> Load bin file*/
		pucConfigBuf = (uint8_t *) kalMemAlloc(
				       MAX_EEPROM_BUFFER_SIZE, VIR_MEM_TYPE);
		if (pucConfigBuf == NULL)
			goto label_exit;

		kalMemZero(pucConfigBuf, MAX_EEPROM_BUFFER_SIZE);

		/* 1 <2> Construct EEPROM binary name */
		kalMemZero(aucEeprom, sizeof(aucEeprom));

		snprintf(aucEeprom, 32, "%s%x.bin",
			 apucEepromName[0], chip_id);

		/* 1 <3> Request buffer bin */
		if (kalRequestFirmware(aucEeprom, pucConfigBuf,
		    MAX_EEPROM_BUFFER_SIZE, &u4ContentLen, prGlueInfo->prDev)
		    == 0) {
			DBGLOG(INIT, INFO, "request file done\n");
		} else {
			DBGLOG(INIT, INFO, "can't find file\n");
			goto label_exit;
		}
		DBGLOG(INIT, INFO, "u4ContentLen = %d\n", u4ContentLen);

		if (u4ContentLen > MAX_EEPROM_BUFFER_SIZE) {
			DBGLOG(INIT, ERROR, "u4ContentLen: %d > %d\n",
			       u4ContentLen, MAX_EEPROM_BUFFER_SIZE);
			goto label_exit;
		}

		/* Update contents in local table */
		kalMemCopy(uacEEPROMImage, pucConfigBuf,
			   u4ContentLen);

		/* 1 <4> Send CMD with bin file content */
		if (prBufferModeInfo->ucVersion != 0x02) {
			DBGLOG(INIT, ERROR, "Not support version\n");
			goto label_exit;
		}

		if (prBufferModeInfo->ucFormatSupportBitmap &
		    BIT(CONTENT_FORMAT_WHOLE_CONTENT)) {
			if (u4ContentLen > prBufferModeInfo->u2EfuseTotalSize) {
				DBGLOG(INIT, ERROR, "u4ContentLen: %d > %d\n",
				       u4ContentLen,
				       prBufferModeInfo->u2EfuseTotalSize);
				goto label_exit;
			}

			kalMemCopy(prSetEfuseBufModeInfo->aBinContent,
				   pucConfigBuf, u4ContentLen);
		} else if (prBufferModeInfo->ucFormatSupportBitmap &
			   BIT(CONTENT_FORMAT_MULTIPLE_SECTIONS)) {
			/* TODO */
			DBGLOG(INIT, ERROR, "Not support yet!\n");
			goto label_exit;
		} else {
			DBGLOG(INIT, ERROR,
			       "Not support legacy BIN_CONTENT mode\n");
			goto label_exit;
		}

		prSetEfuseBufModeInfo->ucSourceMode = SOURCE_MODE_BUFFER_MODE;
#if CFG_RCPI_COMPENSATION
		wlanLoadBufferbinRxFELoss(prAdapter);
#endif
#if CFG_SUPPORT_HW_1T2R
		wlanLoadBufferbin1T2R(prAdapter);
#endif
	} else {
		/* eFuse mode */
		/* Only need to tell FW the content from, contents are directly
		 * from efuse
		 */
		prSetEfuseBufModeInfo->ucSourceMode = SOURCE_MODE_EFUSE;
		u4ContentLen = 0;
#if CFG_RCPI_COMPENSATION
		wlanLoadEfuseRxFELoss(prAdapter);
#endif
#if CFG_SUPPORT_HW_1T2R
		wlanLoadEfuse1T2R(prAdapter);
#endif
	}
	prSetEfuseBufModeInfo->ucContentFormat = CONTENT_FORMAT_WHOLE_CONTENT |
			(prAdapter->rWifiVar.ucCalTimingCtrl <<
			 CMD_TYPE_CAL_TIME_REDUCTION_SHFT);
	prSetEfuseBufModeInfo->u2Count = u4ContentLen;

	rStatus = kalIoctl(prGlueInfo, wlanoidConnacSetEfusBufferMode,
		(void *)prSetEfuseBufModeInfo,
		OFFSET_OF(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T,
		aBinContent) + u4ContentLen,
		FALSE, TRUE, TRUE, &u4BufLen);

	retWlanStat = WLAN_STATUS_SUCCESS;

label_exit:

	/* free memory */
	if (prSetEfuseBufModeInfo != NULL)
		kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE,
			sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T));

	if (pucConfigBuf != NULL)
		kalMemFree(pucConfigBuf, VIR_MEM_TYPE,
			   MAX_EEPROM_BUFFER_SIZE);

#if CFG_EFUSE_AUTO_MODE_SUPPORT
	if (prAccessEfuseInfo != NULL)
		kalMemFree(prAccessEfuseInfo, VIR_MEM_TYPE,
			sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
#endif

	return retWlanStat;
}

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH

#define FW_LOG_CMD_ON_OFF        0
#define FW_LOG_CMD_SET_LEVEL     1
static uint32_t u4LogOnOffCache = -1;

static void consys_log_event_notification(int cmd, int value)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct net_device *prDev = gPrDev;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER rCmdV1Header;
	struct CMD_FORMAT_V1 rCmd_v1;

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
		*((struct GLUE_INFO **) netdev_priv(prDev)) : NULL;
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
			"prAdapter == NULL return, u4LogOnOffCache=%d\n",
				u4LogOnOffCache);
		return;
	}

	if (cmd == FW_LOG_CMD_ON_OFF) {

		/*EvtDrvnLogEn 0/1*/
		uint8_t onoff[1] = {'0'};

		DBGLOG(INIT, TRACE, "FW_LOG_CMD_ON_OFF\n");

		rCmdV1Header.cmdType = CMD_TYPE_SET;
		rCmdV1Header.cmdVersion = CMD_VER_1;
		rCmdV1Header.cmdBufferLen = 0;
		rCmdV1Header.itemNum = 0;

		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		kalMemSet(&rCmd_v1, 0, sizeof(struct CMD_FORMAT_V1));

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

		DBGLOG(INIT, INFO, "Send key word (%s) WITH (%s) to firmware\n",
				rCmd_v1.itemString, rCmd_v1.itemValue);

		kalMemCopy(((struct CMD_FORMAT_V1 *)rCmdV1Header.buffer),
				&rCmd_v1,  sizeof(struct CMD_FORMAT_V1));

		rCmdV1Header.cmdBufferLen += sizeof(struct CMD_FORMAT_V1);
		rCmdV1Header.itemNum = 1;

		rStatus = wlanSendSetQueryCmd(
				prAdapter, /* prAdapter */
				CMD_ID_GET_SET_CUSTOMER_CFG, /* 0x70 */
				TRUE,  /* fgSetQuery */
				FALSE, /* fgNeedResp */
				FALSE, /* fgIsOid */
				NULL,  /* pfCmdDoneHandler*/
				NULL,  /* pfCmdTimeoutHandler */
				sizeof(struct CMD_HEADER),
				(uint8_t *)&rCmdV1Header, /* pucInfoBuffer */
				NULL,  /* pvSetQueryBuffer */
				0      /* u4SetQueryBufferLen */
			);

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO,
				"[Fail]kalIoctl wifiSefCFG fail 0x%x\n",
					rStatus);

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
static int32_t wlanProbe(void *pvData, void *pvDriverData)
{
	struct wireless_dev *prWdev = NULL;
	enum ENUM_PROBE_FAIL_REASON {
		BUS_INIT_FAIL,
		NET_CREATE_FAIL,
		BUS_SET_IRQ_FAIL,
		ADAPTER_START_FAIL,
		NET_REGISTER_FAIL,
		PROC_INIT_FAIL,
		FAIL_MET_INIT_PROCFS,
		FAIL_REASON_NUM
	} eFailReason;
	struct WLANDEV_INFO *prWlandevInfo = NULL;
	int32_t i4DevIdx = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4Status = 0;
	u_int8_t bRet = FALSE;
	u_int8_t i = 0;
	struct REG_INFO *prRegInfo;
	struct mt66xx_chip_info *prChipInfo;
#if CFG_SUPPORT_REPLAY_DETECTION
	u_int8_t ucRpyDetectOffload;
#endif

#if CFG_THERMAL_API_SUPPORT
	struct wcn_platform_bridge pbridge;
#endif

#if (MTK_WCN_HIF_SDIO && CFG_WMT_WIFI_PATH_SUPPORT)
	int32_t i4RetVal = 0;
#endif
	int32_t u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

#if 0
	uint8_t *pucConfigBuf = NULL, pucCfgBuf = NULL;
	uint32_t u4ConfigReadLen = 0;
#endif
	eFailReason = FAIL_REASON_NUM;
	do {
		/* 4 <1> Initialize the IO port of the interface */
		/*  GeorgeKuo: pData has different meaning for _HIF_XXX:
		 * _HIF_EHPI: pointer to memory base variable, which will be
		 *      initialized by glBusInit().
		 * _HIF_SDIO: bus driver handle
		 */

		DBGLOG(INIT, STATE, "enter wlanProbe\n");

		bRet = glBusInit(pvData);

#if (CFG_SUPPORT_TRACE_TC4 == 1)
		wlanDebugTC4Init();
#endif
		/* Cannot get IO address from interface */
		if (bRet == FALSE) {
			DBGLOG(INIT, ERROR, "wlanProbe: glBusInit() fail\n");
			i4Status = -EIO;
			eFailReason = BUS_INIT_FAIL;
			break;
		}
		/* 4 <2> Create network device, Adapter, KalInfo,
		 *       prDevHandler(netdev)
		 */
		prWdev = wlanNetCreate(pvData, pvDriverData);
		if (prWdev == NULL) {
			DBGLOG(INIT, ERROR,
			       "wlanProbe: No memory for dev and its private\n");
			i4Status = -ENOMEM;
			eFailReason = NET_CREATE_FAIL;
			break;
		}
		/* 4 <2.5> Set the ioaddr to HIF Info */
		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(prWdev->wiphy);
		if (prGlueInfo == NULL) {
			DBGLOG(INIT, ERROR,
				"wlanProbe: get wiphy_priv() fail\n");
			i4Status = -EFAULT;
			break;
		}

		gPrDev = prGlueInfo->prDevHandler;

		/* 4 <4> Setup IRQ */
		prWlandevInfo = &arWlanDevInfo[i4DevIdx];

		i4Status = glBusSetIrq(prWdev->netdev, NULL, prGlueInfo);

		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "wlanProbe: Set IRQ error\n");
			eFailReason = BUS_SET_IRQ_FAIL;
			break;
		}

		prGlueInfo->i4DevIdx = i4DevIdx;

		prAdapter = prGlueInfo->prAdapter;

		prGlueInfo->u4ReadyFlag = 0;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		prAdapter->fgIsSupportCsumOffload = FALSE;
		prAdapter->u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
#endif

#if CFG_SUPPORT_CFG_FILE
		wlanGetConfig(prAdapter);
#endif

		/* Init Chip Capability */
		prChipInfo = prAdapter->chip_info;
		if (prChipInfo->asicCapInit)
			prChipInfo->asicCapInit(prAdapter);


		/* Default support 2.4/5G MIMO */
		prAdapter->rWifiFemCfg.u2WifiPath = (
				WLAN_FLAG_2G4_WF0 | WLAN_FLAG_5G_WF0 |
				WLAN_FLAG_2G4_WF1 | WLAN_FLAG_5G_WF1);

#if (MTK_WCN_HIF_SDIO && CFG_WMT_WIFI_PATH_SUPPORT)
		i4RetVal = mtk_wcn_wmt_wifi_fem_cfg_report((
					void *)&prAdapter->rWifiFemCfg);
		if (i4RetVal)
			DBGLOG(INIT, ERROR, "Get WifiPath from WMT drv fail\n");
		else
			DBGLOG(INIT, INFO,
			       "Get WifiPath from WMT drv success, WifiPath=0x%x\n",
			       prAdapter->rWifiFemCfg.u2WifiPath);
#endif
		/* 4 <5> Start Device */
		prRegInfo = &prGlueInfo->rRegInfo;

		/* P_REG_INFO_T prRegInfo = (P_REG_INFO_T) kmalloc(
		 *				sizeof(REG_INFO_T), GFP_KERNEL);
		 */
		kalMemSet(prRegInfo, 0, sizeof(struct REG_INFO));

		/* Trigger the action of switching Pwr state to drv_own */
		prAdapter->fgIsFwOwn = TRUE;

		/* Load NVRAM content to REG_INFO_T */
		glLoadNvram(prGlueInfo, prRegInfo);

		/* kalMemCopy(&prGlueInfo->rRegInfo, prRegInfo,
		 *            sizeof(REG_INFO_T));
		 */

		prRegInfo->u4PowerMode = CFG_INIT_POWER_SAVE_PROF;
#if 0
		prRegInfo->fgEnArpFilter = TRUE;
#endif
		/* The Init value of u4WpaVersion/u4AuthAlg shall be
		 * DISABLE/OPEN, not zero!
		 */
		/* The Init value of u4CipherGroup/u4CipherPairwise shall be
		 * NONE, not zero!
		 */
		prGlueInfo->rWpaInfo.u4WpaVersion =
			IW_AUTH_WPA_VERSION_DISABLED;
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
		prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;

		tasklet_init(&prGlueInfo->rRxTask, halRxTasklet,
			     (unsigned long)prGlueInfo);
		tasklet_init(&prGlueInfo->rTxCompleteTask,
			     halTxCompleteTasklet, (unsigned long)prGlueInfo);

		if (wlanAdapterStart(prAdapter,
				     prRegInfo) != WLAN_STATUS_SUCCESS)
			i4Status = -EIO;

		if (HAL_IS_TX_DIRECT(prAdapter)) {
			if (!prAdapter->fgTxDirectInited) {
				skb_queue_head_init(
						&prAdapter->rTxDirectSkbQueue);

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
				timer_setup(&prAdapter->rTxDirectSkbTimer,
					nicTxDirectTimerCheckSkbQ, 0);

				timer_setup(&prAdapter->rTxDirectHifTimer,
					nicTxDirectTimerCheckHifQ, 0);
#else
				init_timer(&prAdapter->rTxDirectSkbTimer);
				prAdapter->rTxDirectSkbTimer.data =
						(unsigned long)prGlueInfo;
				prAdapter->rTxDirectSkbTimer.function =
						nicTxDirectTimerCheckSkbQ;

				init_timer(&prAdapter->rTxDirectHifTimer);
				prAdapter->rTxDirectHifTimer.data =
						(unsigned long)prGlueInfo;
				prAdapter->rTxDirectHifTimer.function =
					nicTxDirectTimerCheckHifQ;
#endif
				prAdapter->fgTxDirectInited = TRUE;
			}
		}

		/* kfree(prRegInfo); */

		if (i4Status < 0) {
			eFailReason = ADAPTER_START_FAIL;
			break;
		}

		INIT_WORK(&prGlueInfo->rTxMsduFreeWork, kalFreeTxMsduWorker);

		INIT_DELAYED_WORK(&prGlueInfo->rRxPktDeAggWork,
				halDeAggRxPktWorker);


		prGlueInfo->main_thread = kthread_run(main_thread,
				prGlueInfo->prDevHandler, "main_thread");
#if CFG_SUPPORT_MULTITHREAD
		prGlueInfo->hif_thread = kthread_run(hif_thread,
				prGlueInfo->prDevHandler, "hif_thread");
		prGlueInfo->rx_thread = kthread_run(rx_thread,
				prGlueInfo->prDevHandler, "rx_thread");
		HAL_AGG_THREAD(prGlueInfo->prAdapter);
#endif


		/* TODO the change schedule API shall be provided by OS glue
		 * layer
		 */
		/* Switch the Wi-Fi task priority to higher priority and change
		 * the scheduling method
		 */
		if (prGlueInfo->prAdapter->rWifiVar.ucThreadPriority > 0) {
			struct sched_param param = {
				.sched_priority = prGlueInfo->prAdapter
				->rWifiVar.ucThreadPriority
			};
			sched_setscheduler(prGlueInfo->main_thread,
					   prGlueInfo->prAdapter->rWifiVar
					   .ucThreadScheduling, &param);
#if CFG_SUPPORT_MULTITHREAD
			sched_setscheduler(prGlueInfo->hif_thread,
						prGlueInfo->prAdapter->rWifiVar
						.ucThreadScheduling, &param);
			sched_setscheduler(prGlueInfo->rx_thread,
						prGlueInfo->prAdapter->rWifiVar
						.ucThreadScheduling, &param);
#endif
			DBGLOG(INIT, INFO,
			       "Set pri = %d, sched = %d\n",
			       prGlueInfo->prAdapter->rWifiVar.ucThreadPriority,
			       prGlueInfo->prAdapter->rWifiVar
			       .ucThreadScheduling);
		}

		g_u4HaltFlag = 0;

#if CFG_SUPPORT_BUFFER_MODE
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
		if (prChipInfo->downloadBufferBin) {
			if (prChipInfo->downloadBufferBin(prAdapter) !=
			    WLAN_STATUS_SUCCESS){
				DBGLOG(INIT, ERROR,
					"wlanProbe: downloadBufferBin fail\n");
				return -1;
			}
		}
#endif
#endif

#if CFG_SUPPORT_DBDC
		/* Update DBDC default setting */
		cnmInitDbdcSetting(prAdapter);
#endif /*CFG_SUPPORT_DBDC*/

		/* send regulatory information to firmware */
		rlmDomainSendInfoToFirmware(prAdapter);

		/* set MAC address */
		{
			uint32_t rStatus = WLAN_STATUS_FAILURE;
			struct sockaddr MacAddr;
			uint32_t u4SetInfoLen = 0;

			rStatus = kalIoctl(prGlueInfo, wlanoidQueryCurrentAddr,
					   &MacAddr.sa_data, PARAM_MAC_ADDR_LEN,
					   TRUE, TRUE, TRUE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, WARN, "set MAC addr fail 0x%x\n",
								rStatus);
			} else {
				kalMemCopy(prGlueInfo->prDevHandler->dev_addr,
					   &MacAddr.sa_data, ETH_ALEN);
				kalMemCopy(prGlueInfo->prDevHandler->perm_addr,
					   prGlueInfo->prDevHandler->dev_addr,
					   ETH_ALEN);

#if CFG_SHOW_MACADDR_SOURCE
				DBGLOG(INIT, INFO, "MAC address: " MACSTR,
				MAC2STR(prAdapter->rWifiVar.aucMacAddress));
#endif
			}
		}

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		/* set HW checksum offload */
		if (prAdapter->fgIsSupportCsumOffload) {
			uint32_t rStatus = WLAN_STATUS_FAILURE;
			uint32_t u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
			uint32_t u4SetInfoLen = 0;

			rStatus = kalIoctl(prGlueInfo, wlanoidSetCSUMOffload,
					   (void *) &u4CSUMFlags,
					   sizeof(uint32_t),
					   FALSE, FALSE, TRUE, &u4SetInfoLen);

			if (rStatus == WLAN_STATUS_SUCCESS) {
				prGlueInfo->prDevHandler->features =
							NETIF_F_IP_CSUM |
							NETIF_F_IPV6_CSUM |
							NETIF_F_RXCSUM;
			} else {
				DBGLOG(INIT, WARN,
				       "set HW checksum offload fail 0x%x\n",
				       rStatus);
				prAdapter->fgIsSupportCsumOffload = FALSE;
			}
		}
#endif
#if CFG_SUPPORT_802_11K
		{
			uint32_t rStatus = WLAN_STATUS_FAILURE;
			uint32_t u4SetInfoLen = 0;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSync11kCapabilities, NULL, 0,
					   FALSE, FALSE, TRUE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, WARN,
				       "RRM: Set 11k Capabilities fail 0x%x\n",
				       rStatus);
		}
#endif

		/* 4 <3> Register the card */
		i4DevIdx = wlanNetRegister(prWdev);
		if (i4DevIdx < 0) {
			i4Status = -ENXIO;
			DBGLOG(INIT, ERROR,
			       "wlanProbe: Cannot register the net_device context to the kernel\n");
			eFailReason = NET_REGISTER_FAIL;
			break;
		}
		/* 4 <4> Register early suspend callback */
#if CFG_ENABLE_EARLY_SUSPEND
		glRegisterEarlySuspend(&wlan_early_suspend_desc,
				       wlan_early_suspend, wlan_late_resume);
#endif

		/* 4 <5> Register Notifier callback */
		wlanRegisterNotifier();

		/* 4 <6> Initialize /proc filesystem */
#if WLAN_INCLUDE_PROC
		i4Status = procCreateFsEntry(prGlueInfo);
		if (i4Status < 0) {
			DBGLOG(INIT, ERROR, "wlanProbe: init procfs failed\n");
			eFailReason = PROC_INIT_FAIL;
			break;
		}
#endif /* WLAN_INCLUDE_PROC */

#if CFG_MET_PACKET_TRACE_SUPPORT
		kalMetInit(prGlueInfo);
#endif

#if CFG_ENABLE_BT_OVER_WIFI
		prGlueInfo->rBowInfo.fgIsNetRegistered = FALSE;
		prGlueInfo->rBowInfo.fgIsRegistered = FALSE;
		glRegisterAmpc(prGlueInfo);
#endif

#if (CFG_ENABLE_WIFI_DIRECT && CFG_MTK_ANDROID_WMT)
		register_set_p2p_mode_handler(set_p2p_mode_handler);
#elif (CFG_ENABLE_WIFI_DIRECT)
		if (prAdapter->rWifiVar.u4RegP2pIfAtProbe) {
			struct PARAM_CUSTOM_P2P_SET_STRUCT rSetP2P;

			rSetP2P.u4Enable = 1;

#ifdef CFG_DRIVER_INITIAL_RUNNING_MODE
			rSetP2P.u4Mode = CFG_DRIVER_INITIAL_RUNNING_MODE;
#else
			rSetP2P.u4Mode = RUNNING_P2P_MODE;
#endif /* CFG_DRIVER_RUNNING_MODE */
			if (set_p2p_mode_handler(prWdev->netdev, rSetP2P) == 0)
				DBGLOG(INIT, INFO,
					"%s: p2p device registered\n",
					__func__);
			else
				DBGLOG(INIT, ERROR,
					"%s: Failed to register p2p device\n",
					__func__);
		}
#endif
#if (CFG_MET_PACKET_TRACE_SUPPORT == 1)
		DBGLOG(INIT, TRACE, "init MET procfs...\n");
		i4Status = kalMetInitProcfs(prGlueInfo);
		if (i4Status < 0) {
			DBGLOG(INIT, ERROR,
			       "wlanProbe: init MET procfs failed\n");
			eFailReason = FAIL_MET_INIT_PROCFS;
			break;
		}
#endif
		kalMemZero(&prGlueInfo->rFtIeForTx,
			   sizeof(prGlueInfo->rFtIeForTx));

		/* Configure 5G band for registered wiphy */
		if (prAdapter->fgEnable5GBand)
			prWdev->wiphy->bands[KAL_BAND_5GHZ] = &mtk_band_5ghz;
		else
			prWdev->wiphy->bands[KAL_BAND_5GHZ] = NULL;

		for (i = 0 ; i < KAL_P2P_NUM; i++) {
			if (gprP2pRoleWdev[i] == NULL)
				continue;

			if (prAdapter->fgEnable5GBand)
				gprP2pRoleWdev[i]->wiphy->bands[KAL_BAND_5GHZ] =
				&mtk_band_5ghz;
			else
				gprP2pRoleWdev[i]->wiphy->bands[KAL_BAND_5GHZ] =
				NULL;
		}
	} while (FALSE);

	if (i4Status == 0) {
#if CFG_SUPPORT_AGPS_ASSIST
		kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_ON, NULL,
				      0);
#endif
#if CFG_SUPPORT_EASY_DEBUG
		/* move before reading file
		 * wlanLoadDefaultCustomerSetting(prAdapter);
		 */
		wlanFeatureToFw(prGlueInfo->prAdapter);
#endif
		wlanCfgSetSwCtrl(prGlueInfo->prAdapter);
		wlanCfgSetChip(prGlueInfo->prAdapter);
		wlanCfgSetCountryCode(prGlueInfo->prAdapter);
		kalPerMonInit(prGlueInfo);
#if CFG_MET_TAG_SUPPORT
		if (met_tag_init() != 0)
			DBGLOG(INIT, ERROR, "MET_TAG_INIT error!\n");
#endif

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
		/* Calibration Backup Flow */
		if (!g_fgIsCalDataBackuped) {
			if (rlmTriggerCalBackup(prGlueInfo->prAdapter,
			    g_fgIsCalDataBackuped) == WLAN_STATUS_FAILURE) {
				DBGLOG(RFTEST, INFO,
				       "Error : Boot Time Wi-Fi Enable Fail........\n");
				return -1;
			}

			g_fgIsCalDataBackuped = TRUE;
		} else {
			if (rlmTriggerCalBackup(prGlueInfo->prAdapter,
			    g_fgIsCalDataBackuped) == WLAN_STATUS_FAILURE) {
				DBGLOG(RFTEST, INFO,
				       "Error : Normal Wi-Fi Enable Fail........\n");
				return -1;
			}
		}
#endif

#if CFG_SUPPORT_REPLAY_DETECTION
		ucRpyDetectOffload = prAdapter->rWifiVar.ucRpyDetectOffload;

		if (ucRpyDetectOffload == FEATURE_ENABLED) {
			DBGLOG(INIT, INFO,
				"Send CMD to enable Replay Detection offload feature\n");
				wlanSuspendRekeyOffload(prAdapter->prGlueInfo,
				GTK_REKEY_CMD_MODE_RPY_OFFLOAD_ON);
		} else {
			DBGLOG(INIT, INFO,
				"Send CMD to disable Replay Detection offload feature\n");
				wlanSuspendRekeyOffload(prAdapter->prGlueInfo,
				GTK_REKEY_CMD_MODE_RPY_OFFLOAD_OFF);
		}
#endif

		/* card is ready */
		prGlueInfo->u4ReadyFlag = 1;

		kalSetHalted(FALSE);
		wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW,
					 &u4LogLevel);
		if (u4LogLevel > ENUM_WIFI_LOG_LEVEL_DEFAULT)
			wlanDbgSetLogLevelImpl(prAdapter,
					       ENUM_WIFI_LOG_LEVEL_VERSION_V1,
					       ENUM_WIFI_LOG_MODULE_FW,
					       u4LogLevel);

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
		/* sync log status with firmware */
		if (u4LogOnOffCache != -1) /* -1: connsysD does not set */
			consys_log_event_notification((int)FW_LOG_CMD_ON_OFF,
				u4LogOnOffCache);
#endif
		DBGLOG(INIT, STATE, "wlanProbe: probe success\n");
#if CFG_CHIP_RESET_HANG
		if (fgIsResetHangState == SER_L0_HANG_RST_TRGING) {
			DBGLOG(INIT, STATE, "[SER][L0] SET SQC hang!\n");
			fgIsResetHangState = SER_L0_HANG_RST_HANG;
			fgIsResetting = TRUE;
		}
#endif

	} else {
		DBGLOG(INIT, ERROR, "wlanProbe: probe failed, reason:%d\n",
		       eFailReason);
		switch (eFailReason) {
		case FAIL_MET_INIT_PROCFS:
			kalMetRemoveProcfs();
		case PROC_INIT_FAIL:
			wlanNetUnregister(prWdev);
		case NET_REGISTER_FAIL:
			set_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag);
			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);
			/* wait main thread stops */
			wait_for_completion_interruptible(
							&prGlueInfo->rHaltComp);
			wlanAdapterStop(prAdapter);
		/* fallthrough */
		case ADAPTER_START_FAIL:
			glBusFreeIrq(prWdev->netdev,
				*((struct GLUE_INFO **)
						netdev_priv(prWdev->netdev)));
		/* fallthrough */
		case BUS_SET_IRQ_FAIL:
			wlanWakeLockUninit(prGlueInfo);
			wlanNetDestroy(prWdev);
			/* prGlueInfo->prAdapter is released in
			 * wlanNetDestroy
			 */
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
#if CFG_THERMAL_API_SUPPORT
	if (i4Status == 0)
		g_fgIsWifiEnabled = TRUE;

	pbridge.thermal_query_cb = mtk_wcn_temp_query_ctrl;
	wcn_export_platform_bridge_register(&pbridge);
#endif

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
static void wlanRemove(void)
{
	struct net_device *prDev = NULL;
	struct WLANDEV_INFO *prWlandevInfo = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	u_int8_t fgResult = FALSE;

	DBGLOG(INIT, STATE, "Remove wlan!\n");

	kalSetHalted(TRUE);

	/* 4 <0> Sanity check */
	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, ERROR, "u4WlanDevNum = 0\n");
		return;
	}
#if (CFG_ENABLE_WIFI_DIRECT && CFG_MTK_ANDROID_WMT)
	register_set_p2p_mode_handler(NULL);
#endif
	if (u4WlanDevNum > 0
	    && u4WlanDevNum <= CFG_MAX_WLAN_DEVICES) {
		prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
		prWlandevInfo = &arWlanDevInfo[u4WlanDevNum - 1];
	}

	ASSERT(prDev);
	if (prDev == NULL) {
		DBGLOG(INIT, ERROR, "prDev is NULL\n");
		return;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, STATE, "prGlueInfo is NULL\n");
		free_netdev(prDev);
		return;
	}

	/* to avoid that wpa_supplicant/hostapd triogger new cfg80211 command */
	prGlueInfo->u4ReadyFlag = 0;

	/* Have tried to do scan done here, but the exception occurs for */
	/* the P2P scan. Keep the original design that scan done in the	 */
	/* p2pStop/wlanStop.						 */

#if WLAN_INCLUDE_PROC
	procRemoveProcfs();
#endif /* WLAN_INCLUDE_PROC */

	prAdapter = prGlueInfo->prAdapter;
	kalPerMonDestroy(prGlueInfo);

#if CFG_SUPPORT_SER
#if defined(_HIF_USB)
	cnmTimerStopTimer(prAdapter, &prAdapter->rSerSyncTimer);
#endif  /* _HIF_USB */
#endif  /* CFG_SUPPORT_SER */

	/* complete possible pending oid, which may block wlanRemove some time
	 * and then whole chip reset may failed
	 */
	if (kalIsResetting())
		wlanReleasePendingOid(prGlueInfo->prAdapter, 1);

#if CFG_ENABLE_BT_OVER_WIFI
	if (prGlueInfo->rBowInfo.fgIsNetRegistered) {
		bowNotifyAllLinkDisconnected(prGlueInfo->prAdapter);
		/* wait 300ms for BoW module to send deauth */
		kalMsleep(300);
	}
#endif

	if (prGlueInfo->eParamMediaStateIndicated ==
	    PARAM_MEDIA_STATE_CONNECTED) {

#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 2, 0) <= CFG80211_VERSION_CODE)
		cfg80211_disconnected(prGlueInfo->prDevHandler, 0, NULL, 0,
				      TRUE, GFP_KERNEL);
#else
		cfg80211_disconnected(prGlueInfo->prDevHandler, 0, NULL, 0,
				      GFP_KERNEL);
#endif
		kalMsleep(500);
	}

	flush_delayed_work(&workq);

	/* 20150205 work queue for sched_scan */

	flush_delayed_work(&sched_workq);

	down(&g_halt_sem);
	g_u4HaltFlag = 1;
	up(&g_halt_sem);

	/* 4 <2> Mark HALT, notify main thread to stop, and clean up queued
	 *       requests
	 */
	set_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag);

	/* Stop works */
	flush_work(&prGlueInfo->rTxMsduFreeWork);
	cancel_delayed_work_sync(&prGlueInfo->rRxPktDeAggWork);

#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&prGlueInfo->waitq_hif);
	wait_for_completion_interruptible(
		&prGlueInfo->rHifHaltComp);
	HAL_AGG_THREAD_WAKE_UP(prGlueInfo->prAdapter);
	wake_up_interruptible(&prGlueInfo->waitq_rx);
	wait_for_completion_interruptible(&prGlueInfo->rRxHaltComp);
#endif

	/* wake up main thread */
	wake_up_interruptible(&prGlueInfo->waitq);
	/* wait main thread stops */
	wait_for_completion_interruptible(&prGlueInfo->rHaltComp);

	DBGLOG(INIT, INFO, "wlan thread stopped\n");

	/* prGlueInfo->rHifInfo.main_thread = NULL; */
	prGlueInfo->main_thread = NULL;
#if CFG_SUPPORT_MULTITHREAD
	prGlueInfo->hif_thread = NULL;
	prGlueInfo->rx_thread = NULL;

	prGlueInfo->u4TxThreadPid = 0xffffffff;
	prGlueInfo->u4HifThreadPid = 0xffffffff;
#endif

	if (HAL_IS_TX_DIRECT(prAdapter)) {
		if (prAdapter->fgTxDirectInited) {
			del_timer_sync(&prAdapter->rTxDirectSkbTimer);
			del_timer_sync(&prAdapter->rTxDirectHifTimer);
		}
	}

	/* Destroy wakelock */
	wlanWakeLockUninit(prGlueInfo);

	kalMemSet(&(prGlueInfo->prAdapter->rWlanInfo), 0,
		  sizeof(struct WLAN_INFO));

#if CFG_ENABLE_WIFI_DIRECT
	if (prGlueInfo->prAdapter->fgIsP2PRegistered) {
		DBGLOG(INIT, INFO, "p2pNetUnregister...\n");
		p2pNetUnregister(prGlueInfo, FALSE);
		DBGLOG(INIT, INFO, "p2pRemove...\n");
		/*p2pRemove must before wlanAdapterStop */
		p2pRemove(prGlueInfo);
	}
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	if (prGlueInfo->rBowInfo.fgIsRegistered)
		glUnregisterAmpc(prGlueInfo);
#endif

#if (CFG_MET_PACKET_TRACE_SUPPORT == 1)
	kalMetRemoveProcfs();
#endif

#if CFG_MET_TAG_SUPPORT
	if (GL_MET_TAG_UNINIT() != 0)
		DBGLOG(INIT, ERROR, "MET_TAG_UNINIT error!\n");
#endif

	/* 4 <4> wlanAdapterStop */
#if CFG_SUPPORT_AGPS_ASSIST
	kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_OFF, NULL,
			      0);
#endif

	wlanAdapterStop(prAdapter);

	HAL_LP_OWN_SET(prAdapter, &fgResult);
	DBGLOG(INIT, INFO, "HAL_LP_OWN_SET(%d)\n",
	       (uint32_t) fgResult);

	/* 4 <x> Stopping handling interrupt and free IRQ */
	glBusFreeIrq(prDev, prGlueInfo);

	/* 4 <5> Release the Bus */
	glBusRelease(prDev);

#if (CFG_SUPPORT_TRACE_TC4 == 1)
	wlanDebugTC4Uninit();
#endif
	/* 4 <6> Unregister the card */
	wlanNetUnregister(prDev->ieee80211_ptr);

	/* 4 <7> Destroy the device */
	wlanNetDestroy(prDev->ieee80211_ptr);
	prDev = NULL;

	tasklet_kill(&prGlueInfo->rTxCompleteTask);
	tasklet_kill(&prGlueInfo->rRxTask);

	/* 4 <8> Unregister early suspend callback */
#if CFG_ENABLE_EARLY_SUSPEND
	glUnregisterEarlySuspend(&wlan_early_suspend_desc);
#endif

	gprWdev->netdev = NULL;

	/* 4 <9> Unregister notifier callback */
	wlanUnregisterNotifier();

#if CFG_CHIP_RESET_SUPPORT & !CFG_WMT_RESET_API_SUPPORT
	fgIsResetting = FALSE;
#endif
#if CFG_THERMAL_API_SUPPORT
	g_fgIsWifiEnabled = FALSE;
	wcn_export_platform_bridge_unregister();
#endif

	DBGLOG(INIT, STATE, "end\n");

}				/* end of wlanRemove() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Driver entry point when the driver is configured as a Linux Module,
 *        and is called once at module load time, by the user-level modutils
 *        application: insmod or modprobe.
 *
 * \retval 0     Success
 */
/*----------------------------------------------------------------------------*/
/* 1 Module Entry Point */
static int initWlan(void)
{
	int ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, INFO, "initWlan\n");

#ifdef CFG_DRIVER_INF_NAME_CHANGE

	if (kalStrLen(gprifnamesta) > CUSTOM_IFNAMESIZ ||
	    kalStrLen(gprifnamep2p) > CUSTOM_IFNAMESIZ ||
	    kalStrLen(gprifnameap) > CUSTOM_IFNAMESIZ) {
		DBGLOG(INIT, ERROR, "custom infname len illegal > %d\n",
		       CUSTOM_IFNAMESIZ);
		return -EINVAL;
	}

#endif /*  CFG_DRIVER_INF_NAME_CHANGE */

	wlanDebugInit();

	/* memory pre-allocation */
#if CFG_PRE_ALLOCATION_IO_BUFFER
	kalInitIOBuffer(TRUE);
#else
	kalInitIOBuffer(FALSE);
#endif


#if WLAN_INCLUDE_PROC
	procInitFs();
#endif

	wlanCreateWirelessDevice();
	if (gprWdev == NULL)
		return -ENOMEM;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(
			     gprWdev->wiphy);
	if (gprWdev) {
		/* P2PDev and P2PRole[0] share the same Wdev */
		if (glP2pCreateWirelessDevice(prGlueInfo) == TRUE)
			gprP2pWdev = gprP2pRoleWdev[0];
	}
	gPrDev = NULL;

	ret = ((glRegisterBus(wlanProbe,
			      wlanRemove) == WLAN_STATUS_SUCCESS) ? 0 : -EIO);

	if (ret == -EIO) {
		kalUninitIOBuffer();
		return ret;
	}
#if (CFG_CHIP_RESET_SUPPORT)
	glResetInit(prGlueInfo);
#endif
	kalFbNotifierReg((struct GLUE_INFO *) wiphy_priv(
				 gprWdev->wiphy));

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	wifi_fwlog_event_func_register(consys_log_event_notification);
#endif

#if CFG_MTK_ANDROID_EMI
	/* Set WIFI EMI protection to consys permitted on system boot up */
	kalSetEmiMpuProtection(gConEmiPhyBase, WIFI_EMI_MEM_OFFSET,
			       WIFI_EMI_MEM_SIZE, true);
#endif
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
static void exitWlan(void)
{
	kalFbNotifierUnReg();
#if CFG_CHIP_RESET_SUPPORT
	glResetUninit();
#endif

	glUnregisterBus(wlanRemove);

	/* free pre-allocated memory */
	kalUninitIOBuffer();

	/* For single wiphy case, it's hardly to free wdev & wiphy in 2 func.
	 * So that, use wlanDestroyAllWdev to replace wlanDestroyWirelessDevice
	 * and glP2pDestroyWirelessDevice.
	 */
	wlanDestroyAllWdev();

#if WLAN_INCLUDE_PROC
	procUninitProcFs();
#endif
	DBGLOG(INIT, INFO, "exitWlan\n");

}				/* end of exitWlan() */

#if CFG_THERMAL_API_SUPPORT


/*----------------------------------------------------------------------------*/
/*!
* \brief export API for thermal
*
* \retval >0 Success
* \retval -1000 invalid
*/
/*----------------------------------------------------------------------------*/
static int mtk_wcn_temp_query_ctrl(void)
{
	struct net_device *prDev = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int i4Temperature = 0;
#ifdef CFG_GET_TEMPURATURE
	uint32_t u4BufLen;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
#endif
	/* 1. check if wifi is enabled */
	if (!g_fgIsWifiEnabled)
		return -1000;

	ASSERT(u4WlanDevNum <= CFG_MAX_WLAN_DEVICES);
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, INFO, "0 == u4WlanDevNum\n");
		return -1000;
	}

	if (u4WlanDevNum > 0 && u4WlanDevNum <= CFG_MAX_WLAN_DEVICES)
		prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;

	ASSERT(prDev);
	if (prDev == NULL) {
		DBGLOG(INIT, INFO, "NULL == prDev\n");
		return -1000;
	}
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, INFO, "NULL == prGlueInfo\n");
		return -1000;
	}
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);
#ifdef CFG_GET_TEMPURATURE
	rStatus = kalIoctl(prGlueInfo,
					wlanoidGetTemperature,
					&i4Temperature,
					sizeof(i4Temperature),
					TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "Query ucTemperature failed!\n");
		return -1000;
	}
#else
	return -1000;
#endif
	return i4Temperature;

}
#endif

#if ((MTK_WCN_HIF_SDIO == 1) && (CFG_BUILT_IN_DRIVER == 1)) || \
	((MTK_WCN_HIF_AXI == 1) && (CFG_BUILT_IN_DRIVER == 1))

int mtk_wcn_wlan_gen4_init(void)
{
	return initWlan();
}
EXPORT_SYMBOL(mtk_wcn_wlan_gen4_init);

void mtk_wcn_wlan_gen4_exit(void)
{
	return exitWlan();
}
EXPORT_SYMBOL(mtk_wcn_wlan_gen4_exit);

#elif ((MTK_WCN_HIF_SDIO == 0) && (CFG_BUILT_IN_DRIVER == 1))

device_initcall(initWlan);

#else

module_init(initWlan);
module_exit(exitWlan);

#endif
