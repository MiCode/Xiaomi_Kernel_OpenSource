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
#if CFG_TC1_FEATURE
#include <tc1_partition.h>
#endif
#if CFG_CHIP_RESET_SUPPORT
#include "gl_rst.h"
#endif
#include "gl_vendor.h"
#include "gl_hook_api.h"
#if CFG_MTK_MCIF_WIFI_SUPPORT
#include "mddp.h"
#endif
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* #define MAX_IOREQ_NUM   10 */
struct semaphore g_halt_sem;
int g_u4HaltFlag;
int g_u4WlanInitFlag;
enum ENUM_NVRAM_STATE g_NvramFsm = NVRAM_STATE_INIT;

uint8_t g_aucNvram[MAX_CFG_FILE_WIFI_REC_SIZE];
struct wireless_dev *gprWdev[KAL_AIS_NUM];

#if CFG_SUPPORT_PERSIST_NETDEV
struct net_device *gprNetdev[KAL_AIS_NUM] = {};
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

/* For running on X86 UT environment */
#if defined(UT_TEST_MODE) && defined(CFG_BUILD_X86_PLATFORM)
phys_addr_t gConEmiPhyBase;
EXPORT_SYMBOL(gConEmiPhyBase);

unsigned long long gConEmiSize;
EXPORT_SYMBOL(gConEmiSize);
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
	/* UNII-1 */
	CHAN5G(36, 0),
	CHAN5G(40, 0),
	CHAN5G(44, 0),
	CHAN5G(48, 0),
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
	CHAN5G(153, 0),
	CHAN5G(157, 0),
	CHAN5G(161, 0),
	CHAN5G(165, 0)
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
	WLAN_CIPHER_SUITE_GCMP_256,
	WLAN_CIPHER_SUITE_BIP_GMAC_256, /* TODO, HW not support,
					* SW should handle integrity check
					*/
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

#if CFG_SUPPORT_WPA3
	.external_auth = mtk_cfg80211_external_auth,
#endif
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
#if CFG_SUPPORT_WPA3
	.external_auth = mtk_cfg80211_external_auth,
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_COUNTRY_CODE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_country_code
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_SET_PNO_RANDOM_MAC_OUI
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV
			| WIPHY_VENDOR_CMD_NEED_NETDEV
			| WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = mtk_cfg80211_vendor_set_scan_mac_oui
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_SETBAND
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_band
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
#if CFG_SUPPORT_MBO
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAM
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_roaming_param
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif

	},
#endif
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = WIFI_SUBCMD_SET_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_set_roaming_policy
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_GET_ROAMING_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_roaming_capabilities
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_CONFIG_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_config_roaming
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_SUBCMD_ENABLE_ROAMING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_enable_roaming
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = WIFI_OFFLOAD_STOP_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
		WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_packet_keep_alive_stop
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif

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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
#endif /* CFG_SUPPORT_P2P_PREFERRED_FREQ_LIST */
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = NL80211_VENDOR_SUBCMD_ACS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV
				| WIPHY_VENDOR_CMD_NEED_NETDEV
				| WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = mtk_cfg80211_vendor_acs
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	},
#endif
	{
		{
			.vendor_id = OUI_QCA,
			.subcmd = NL80211_VENDOR_SUBCMD_GET_FEATURES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV
				| WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_get_features
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif

	},
	/* Get Driver Memory Dump */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = LOGGER_DRIVER_MEM_DUMP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
				WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = mtk_cfg80211_vendor_driver_memory_dump
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		,
		.policy = VENDOR_CMD_RAW_DATA
#endif
	}
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
#if CFG_SUPPORT_DATA_STALL
	{
		.vendor_id = OUI_MTK,
		.subcmd = WIFI_EVENT_DRIVER_ERROR
	},
#endif
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
	{
		.vendor_id = OUI_QCA,
		.subcmd = NL80211_VENDOR_SUBCMD_ACS
	},
#endif

	{
		.vendor_id = OUI_MTK,
		.subcmd = WIFI_EVENT_GENERIC_RESPONSE
	},

#if CFG_SUPPORT_BIGDATA_PIP
	{
		.vendor_id = OUI_MTK,
		.subcmd = WIFI_EVENT_BIGDATA_PIP
	},
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
		      BIT(IEEE80211_STYPE_AUTH >> 4)
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
			| BIT(IEEE80211_STYPE_ACTION >> 4)
#if CFG_SUPPORT_SOFTAP_WPA3
			| BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			  BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			  BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			  BIT(IEEE80211_STYPE_AUTH >> 4) |
			  BIT(IEEE80211_STYPE_DEAUTH >> 4)
#endif
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

#if CFG_SUPPORT_RM_BEACON_REPORT_BY_SUPPLICANT
/* NL80211_FEATURE_DS_PARAM_SET_IE_IN_PROBES & NL80211_FEATURE_QUIET
 * support in linux kernet version => 3.18
 */
#if KERNEL_VERSION(3, 18, 0) > CFG80211_VERSION_CODE
#define NL80211_FEATURE_DS_PARAM_SET_IE_IN_PROBES BIT(19)
#define NL80211_FEATURE_QUIET BIT(21)
#endif
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

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    struct net_device *sb_dev)
{
	return mtk_wlan_ndev_select_queue(dev, skb);
}
#elif KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    struct net_device *sb_dev, select_queue_fallback_t fallback)
{
	return mtk_wlan_ndev_select_queue(dev, skb);
}
#elif KERNEL_VERSION(3, 14, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    void *accel_priv, select_queue_fallback_t fallback)
{
	return mtk_wlan_ndev_select_queue(dev, skb);
}
#elif KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb,
		    void *accel_priv)
{
	return mtk_wlan_ndev_select_queue(dev, skb);
}
#else
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb)
{
	return mtk_wlan_ndev_select_queue(dev, skb);
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
static void glLoadNvram(struct GLUE_INFO *prGlueInfo,
	struct REG_INFO *prRegInfo)
{
	struct WIFI_CFG_PARAM_STRUCT *prNvramSettings;

	ASSERT(prRegInfo);
	ASSERT(prGlueInfo);

	DBGLOG(INIT, INFO, "g_NvramFsm = %d\n", g_NvramFsm);
	if (g_NvramFsm != NVRAM_STATE_READY) {
		DBGLOG(INIT, WARN, "Nvram not available\n");
		return;
	}

	if (sizeof(struct WIFI_CFG_PARAM_STRUCT) >
					sizeof(g_aucNvram)) {
		DBGLOG(INIT, ERROR,
		"Size WIFI_CFG_PARAM_STRUCT %zu > size aucNvram %zu\n"
		, sizeof(struct WIFI_CFG_PARAM_STRUCT),
		sizeof(g_aucNvram));
		return;
	}

	prGlueInfo->fgNvramAvailable = TRUE;

	prRegInfo->prNvramSettings =
		(struct WIFI_CFG_PARAM_STRUCT *)&g_aucNvram[0];
	prNvramSettings = prRegInfo->prNvramSettings;

#if CFG_TC1_FEATURE
		TC1_FAC_NAME(FacReadWifiMacAddr)(prRegInfo->aucMacAddr);
		DBGLOG(INIT, INFO,
			"MAC address: " MACSTR, MAC2STR(prRegInfo->aucMacAddr));
#else
	/* load MAC Address */
	kalMemCopy(prRegInfo->aucMacAddr,
			prNvramSettings->aucMacAddress,
			PARAM_MAC_ADDR_LEN*sizeof(uint8_t));
#endif
		/* load country code */
		/* cast to wide characters */
		prRegInfo->au2CountryCode[0] =
			(uint16_t) prNvramSettings->aucCountryCode[0];
		prRegInfo->au2CountryCode[1] =
			(uint16_t) prNvramSettings->aucCountryCode[1];

	prRegInfo->ucSupport5GBand =
			prNvramSettings->ucSupport5GBand;

	prRegInfo->ucEnable5GBand = prNvramSettings->ucEnable5GBand;

	/* load regulation subbands */
	prRegInfo->eRegChannelListMap = 0;
	prRegInfo->ucRegChannelListIndex = 0;

	if (prRegInfo->eRegChannelListMap == REG_CH_MAP_CUSTOMIZED) {
		kalMemCopy(prRegInfo->rDomainInfo.rSubBand,
			prNvramSettings->aucRegSubbandInfo,
			MAX_SUBBAND_NUM*sizeof(uint8_t));
	}

	log_dbg(INIT, INFO, "u2Part1OwnVersion = %08x, u2Part1PeerVersion = %08x\n",
				 prNvramSettings->u2Part1OwnVersion,
				 prNvramSettings->u2Part1PeerVersion);
}

static void wlanFreeNetDev(void)
{
	uint32_t u4Idx = 0;

	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		if (gprWdev[u4Idx] && gprWdev[u4Idx]->netdev) {
			DBGLOG(INIT, INFO, "free_netdev wlan%d netdev start.\n",
					u4Idx);
			free_netdev(gprWdev[u4Idx]->netdev);
			DBGLOG(INIT, INFO, "free_netdev wlan%d netdev end.\n",
					u4Idx);
		}
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
#if CFG_SUPPORT_PERSIST_NETDEV
		else if (arWlanDevInfo[i].prDev == prDev)
			return i;
#endif
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
	}  else if (i4Cmd == SIOCDEVPRIVATE + 2) {
		ret = priv_support_ioctl(prDev, prIfReq, i4Cmd);
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
	struct GLUE_INFO *prGlueInfo;

	if (!prDev)
		return;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));

	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return;
	}

	/* Allow to receive all multicast for WOW */
	DBGLOG(INIT, TRACE, "flags: 0x%x\n", prDev->flags);
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
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(prDev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return;

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

	if (!prGlueInfo->u4ReadyFlag) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		up(&g_halt_sem);
		return;
	}

	DBGLOG(INIT, TRACE,
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
			up(&g_halt_sem);
			return;
		}

		/* Avoid race condition with kernel net subsystem */
		netif_addr_lock_bh(prDev);

		netdev_for_each_mc_addr(ha, prDev) {
			if (i < MAX_NUM_GROUP_ADDR) {
				kalMemCopy((prMCAddrList + i * ETH_ALEN),
					   GET_ADDR(ha), ETH_ALEN);
				i++;
			}
		}

		netif_addr_unlock_bh(prDev);

		up(&g_halt_sem);

		kalIoctlByBssIdx(prGlueInfo,
			 wlanoidSetMulticastList, prMCAddrList, (i * ETH_ALEN),
			 FALSE, FALSE, TRUE, &u4SetInfoLen,
			 ucBssIndex);

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

#if ARP_BRUST_OPTIMIZE
static u_int8_t data_is_ipv4_arp_pkt(struct sk_buff *skb)
{
	uint8_t *pPkt;
	uint16_t u2EtherType;

	pPkt = skb->data;
	u2EtherType = (pPkt[ETH_TYPE_LEN_OFFSET] << 8)
			     |(pPkt[ETH_TYPE_LEN_OFFSET + 1]);
	//DBGLOG(INIT, INFO, "u2EtherType=0x%x", u2EtherType);
	if (u2EtherType == ETH_P_ARP)
		return TRUE;
	else
		return FALSE;
}
static u_int8_t data_is_ipv4_arp_req(struct sk_buff *skb)
{
	uint8_t *pPkt;
	uint16_t u2OpCode;

	pPkt = skb->data;
	u2OpCode = (pPkt[ETH_HLEN+6] << 8)| pPkt[ETH_HLEN+7];
	//DBGLOG(INIT, INFO, "u2OPCode:0x%x\n", u2OpCode);
	if (u2OpCode == ARP_PRO_REQ)
		return TRUE;
	else
		return FALSE;
}
static uint32_t get_arp_tgt_ip(struct sk_buff *skb)
{
	uint8_t *pPkt;
	uint32_t u4TgtIp;

	pPkt = skb->data;
	u4TgtIp = *(uint32_t *)(pPkt + ETH_HLEN + ARP_TARGET_IP_OFFSET);
	return u4TgtIp;
}
static void arp_brust_opt_init(struct ADAPTER *adapter)
{
	struct arp_burst_stat *arp_b_s;

	if (adapter == NULL) {
		DBGLOG(INIT, WARN, "arp brust opt init fail\n");
		return;
	}
	arp_b_s = &(adapter->arp_b_stat);
	arp_b_s->begin = 0;
	arp_b_s->brust = 10;
	arp_b_s->brust_signify = 5;
	arp_b_s->drop_count = 0;
	arp_b_s->pass_count = 0;
	arp_b_s->pass_signify_count = 0;
	arp_b_s->interval = 100; //ms
	arp_b_s->apIp = 0;
	arp_b_s->gatewayIp = 0;
}
/* Xiaomi Add
 * RETURNS:
 * 0:Not limit
 *   packet isn't arp request or gateway and AP ip aren't 0
 * 1:limit based on brust count
 *   arp request and target ip is not gatway or AP IP
 * 2:limit based on gateway burst count
 *   arp request and target ip is gatway or AP IP
 */
static int process_pkt_action(struct ADAPTER *adapter, struct sk_buff *skb)
{
	uint32_t u4TgtIp = 0;
	struct arp_burst_stat *arp_b_s = &(adapter->arp_b_stat);

	if (arp_b_s->apIp == 0 && arp_b_s->gatewayIp == 0)
		return 0;
	if (data_is_ipv4_arp_pkt(skb) && data_is_ipv4_arp_req(skb)) {
		u4TgtIp = get_arp_tgt_ip(skb);

		if (arp_b_s->apIp == u4TgtIp || arp_b_s->gatewayIp == u4TgtIp)
			return 2;
		return 1;
	}
	return 0;
}
/* Xiaomi Add
 * RETURNS:
 * 0 means drop arp request
 * 1 means go ahead
 */
static int arp_rate_limit(struct ADAPTER *adapter, struct sk_buff *skb)
{
	struct arp_burst_stat *arp_b_s = NULL;
	int ret;
	int action;

	if (adapter == NULL)
		return 1;
	arp_b_s = &(adapter->arp_b_stat);
	action = process_pkt_action(adapter, skb);
	if (!action)
		return 1;
	// init fail, so don't drop arp request
	if (arp_b_s->interval == 0 || arp_b_s->brust == 0 || arp_b_s->brust_signify == 0) {
		DBGLOG_LIMITED(INIT, TRACE, "init fail\n");
		return 1;
	}
	if (arp_b_s->begin == 0)
		arp_b_s->begin = kalGetTimeTick();
	if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), arp_b_s->begin, arp_b_s->interval)) {
	//if (time_is_before_jiffies(arp_b_s->begin + arp_b_s->interval)) {
		if (arp_b_s->drop_count) {
			DBGLOG_LIMITED(INIT, WARN, "%s: arp too frequency, drop count: %d",
				__func__, arp_b_s->drop_count);
			arp_b_s->drop_count = 0;
		}
		arp_b_s->begin = kalGetTimeTick();
		arp_b_s->pass_count = 0;
		arp_b_s->pass_signify_count = 0;

	}
	if (action == 1) {
		if (arp_b_s->pass_count > arp_b_s->brust) {
			arp_b_s->drop_count++;
			ret = 0;
		} else {
			arp_b_s->pass_count++;
			ret = 1;
		}
	} else { //for action == 2
		if (arp_b_s->pass_signify_count > arp_b_s->brust_signify) {
			arp_b_s->drop_count++;
			ret = 0;
		} else {
			arp_b_s->pass_signify_count++;
			ret = 1;
		}

	}
	return ret;
}
#endif
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
netdev_tx_t wlanHardStartXmit(struct sk_buff *prSkb,
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
#if (CFG_SUPPORT_STATISTICS == 1)
	STATS_TX_TIME_ARRIVE(prSkb);
#endif
#if ARP_BRUST_OPTIMIZE
	if (!arp_rate_limit(prGlueInfo->prAdapter, prSkb)) {
		dev_kfree_skb(prSkb);
		return NETDEV_TX_OK;
	}
#endif

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
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate;

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prDev);
	kalMemCopy(&prNetDevPrivate->stats, &prDev->stats,
			sizeof(struct net_device_stats));
#if CFG_MTK_MCIF_WIFI_SUPPORT
	mddpGetMdStats(prDev);
#endif

	return (struct net_device_stats *) kalGetStats(prDev);
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

#if CFG_SUPPORT_RX_GRO
/*----------------------------------------------------------------------------*/
/*!
 * \brief A method of callback function for napi struct
 *
 * It just return false because driver indicate Rx packet directly.
 *
 * \param[in] napi      Pointer to struct napi_struct.
 * \param[in] budget    Polling time interval.
 *
 * \return false
 */
/*----------------------------------------------------------------------------*/
static int kal_napi_poll(struct napi_struct *napi, int budget)
{
	return 0;
}
#endif

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
#if CFG_SUPPORT_RX_GRO
	uint8_t ucBssIndex;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
#endif
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

#if CFG_SUPPORT_RX_GRO
	/* Register GRO function to kernel */
	ucBssIndex = wlanGetBssIdx(prDev);
	prDev->features |= NETIF_F_GRO;
	prDev->hw_features |= NETIF_F_GRO;
	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prDev);
	spin_lock_init(&prNetDevPrivate->napi_spinlock);
	prNetDevPrivate->napi.dev = prDev;
	netif_napi_add(prNetDevPrivate->napi.dev,
		&prNetDevPrivate->napi, kal_napi_poll, 64);
	DBGLOG(INIT, INFO,
		"GRO interface added successfully:%p\n", prDev);
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
	struct BSS_INFO *prAisBssInfo = NULL;

	/**********************************************************************
	 * Check if kernel passes valid data to us                            *
	 **********************************************************************
	 */
	if (!ndev || !addr) {
		DBGLOG(INIT, ERROR, "Set macaddr with ndev(%d) and addr(%d)\n",
		       (ndev == NULL) ? 0 : 1, (addr == NULL) ? 0 : 1);
		return -EINVAL;
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

	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	prAdapter = prGlueInfo->prAdapter;
	prAisBssInfo = aisGetAisBssInfo(prAdapter,
		wlanGetBssIdx(ndev));

	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr, sa->sa_data);
	COPY_MAC_ADDR(ndev->dev_addr, sa->sa_data);
	DBGLOG(INIT, INFO,
		"[wlan%d] Set connect random macaddr to " MACSTR ".\n",
		prAisBssInfo->ucBssIndex,
		MAC2STR(prAisBssInfo->aucOwnMacAddr));

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

	netif_tx_stop_all_queues(prDev);

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

	kalMemZero(aucChannelList, sizeof(aucChannelList));

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
					mtk_5ghz_channels[j].dfs_state =
					    (aucChannelList[i].eDFS) ?
					     NL80211_DFS_USABLE :
					     NL80211_DFS_UNAVAILABLE;
					if (mtk_5ghz_channels[j].dfs_state ==
							NL80211_DFS_USABLE)
						mtk_5ghz_channels[j].flags |=
							IEEE80211_CHAN_RADAR;
					else
						mtk_5ghz_channels[j].flags &=
							~IEEE80211_CHAN_RADAR;
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

#if CFG_SUPPORT_SAP_DFS_CHANNEL
static u_int8_t wlanIsAdjacentChnl(struct GL_P2P_INFO *prGlueP2pInfo,
		uint32_t u4CenterFreq, uint8_t ucBandWidth,
		enum ENUM_CHNL_EXT eBssSCO, uint8_t ucAdjacentChannel)
{
	uint32_t u4AdjacentFreq = 0;
	uint32_t u4BandWidth = 20;
	uint32_t u4StartFreq, u4EndFreq;
	struct ieee80211_channel *chnl = NULL;

	u4AdjacentFreq = nicChannelNum2Freq(ucAdjacentChannel) / 1000;

	DBGLOG(INIT, TRACE,
		"p2p: %p, center_freq: %d, bw: %d, sco: %d, ad_freq: %d",
		prGlueP2pInfo, u4CenterFreq, ucBandWidth, eBssSCO,
		u4AdjacentFreq);

	if (!prGlueP2pInfo)
		return FALSE;

	if (ucBandWidth == VHT_OP_CHANNEL_WIDTH_20_40 &&
			eBssSCO == CHNL_EXT_SCN)
		return FALSE;

	if (!u4CenterFreq)
		return FALSE;

	if (!u4AdjacentFreq)
		return FALSE;

	switch (ucBandWidth) {
	case VHT_OP_CHANNEL_WIDTH_20_40:
		u4BandWidth = 40;
		break;
	case VHT_OP_CHANNEL_WIDTH_80:
		u4BandWidth = 80;
		break;
	default:
		DBGLOG(INIT, WARN, "unsupported bandwidth: %d", ucBandWidth);
		return FALSE;
	}
	u4StartFreq = u4CenterFreq - u4BandWidth / 2 + 10;
	u4EndFreq = u4CenterFreq + u4BandWidth / 2 - 10;
	DBGLOG(INIT, TRACE, "bw: %d, s_freq: %d, e_freq: %d",
			u4BandWidth, u4StartFreq, u4EndFreq);
	if (u4AdjacentFreq < u4StartFreq || u4AdjacentFreq > u4EndFreq)
		return FALSE;

	/* check valid channel */
	chnl = ieee80211_get_channel(prGlueP2pInfo->prWdev->wiphy,
			u4AdjacentFreq);
	if (!chnl) {
		DBGLOG(INIT, WARN, "invalid channel for freq: %d",
				u4AdjacentFreq);
		return FALSE;
	}
	return TRUE;
}

void wlanUpdateDfsChannelTable(struct GLUE_INFO *prGlueInfo,
		uint8_t ucRoleIdx, uint8_t ucChannel, uint8_t ucBandWidth,
		enum ENUM_CHNL_EXT eBssSCO, uint32_t u4CenterFreq)
{
	struct GL_P2P_INFO *prGlueP2pInfo = NULL;
	uint8_t i, j;
	uint8_t ucNumOfChannel;
	struct RF_CHANNEL_INFO aucChannelList[
			ARRAY_SIZE(mtk_5ghz_channels)] = {};

	DBGLOG(INIT, INFO, "r: %d, chnl %u, b: %d, s: %d, freq: %d\n",
			ucRoleIdx, ucChannel, ucBandWidth, eBssSCO,
			u4CenterFreq);

	/* 1. Get current domain DFS channel list */
	rlmDomainGetDfsChnls(prGlueInfo->prAdapter,
		ARRAY_SIZE(mtk_5ghz_channels),
		&ucNumOfChannel, aucChannelList);

	if (ucRoleIdx >= 0 && ucRoleIdx < KAL_P2P_NUM)
		prGlueP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

	/* 2. Enable specific channel based on domain channel list */
	for (i = 0; i < ucNumOfChannel; i++) {
		for (j = 0; j < ARRAY_SIZE(mtk_5ghz_channels); j++) {
			if (aucChannelList[i].ucChannelNum !=
				mtk_5ghz_channels[j].hw_value)
				continue;

			if ((aucChannelList[i].ucChannelNum == ucChannel) ||
				wlanIsAdjacentChnl(prGlueP2pInfo,
					u4CenterFreq,
					ucBandWidth,
					eBssSCO,
					aucChannelList[i].ucChannelNum)) {
				mtk_5ghz_channels[j].dfs_state
					= NL80211_DFS_AVAILABLE;
				mtk_5ghz_channels[j].flags &=
					~IEEE80211_CHAN_RADAR;
				mtk_5ghz_channels[j].orig_flags &=
					~IEEE80211_CHAN_RADAR;
				DBGLOG(INIT, INFO,
					"ch (%d), force NL80211_DFS_AVAILABLE.\n",
					aucChannelList[i].ucChannelNum);
			} else {
				mtk_5ghz_channels[j].dfs_state
					= NL80211_DFS_USABLE;
				mtk_5ghz_channels[j].flags |=
					IEEE80211_CHAN_RADAR;
				mtk_5ghz_channels[j].orig_flags |=
					IEEE80211_CHAN_RADAR;
				DBGLOG(INIT, TRACE,
					"ch (%d), force NL80211_DFS_USABLE.\n",
					aucChannelList[i].ucChannelNum);
			}
		}
	}
}
#endif

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
	uint32_t u4Idx = 0;

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

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
				netdev_priv(gprWdev[u4Idx]->netdev);
			ASSERT(prNetDevPrivate->prGlueInfo == prGlueInfo);
			prNetDevPrivate->ucBssIdx = u4Idx;
#if CFG_ENABLE_UNIFY_WIPHY
			prNetDevPrivate->ucIsP2p = FALSE;
#endif
#if CFG_MTK_MCIF_WIFI_SUPPORT
			/* only wlan0 supports mddp */
			prNetDevPrivate->ucMddpSupport = (u4Idx == 0);
#else
			prNetDevPrivate->ucMddpSupport = FALSE;
#endif
			wlanBindBssIdxToNetInterface(prGlueInfo,
				     u4Idx,
				     (void *)gprWdev[u4Idx]->netdev);
#if CFG_SUPPORT_PERSIST_NETDEV
			if (gprNetdev[u4Idx]->reg_state == NETREG_REGISTERED)
				continue;
#endif
			if (register_netdev(gprWdev[u4Idx]->netdev)
				< 0) {
				DBGLOG(INIT, ERROR,
					"Register net_device %d failed\n",
					u4Idx);
				wlanClearDevIdx(
					gprWdev[u4Idx]->netdev);
				i4DevIdx = -1;
				break;
			}
		}

		if (i4DevIdx != -1)
			prGlueInfo->fgIsRegistered = TRUE;
		else {
			/* Unregister the registered netdev if one of netdev
			 * registered fail
			 */
			for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
				if (!gprWdev[u4Idx] || !gprWdev[u4Idx]->netdev)
					continue;
				if (gprWdev[u4Idx]->netdev->reg_state !=
						NETREG_REGISTERED)
					continue;
				wlanClearDevIdx(gprWdev[u4Idx]->netdev);
				unregister_netdev(gprWdev[u4Idx]->netdev);
			}
		}
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

#if !CFG_SUPPORT_PERSIST_NETDEV
	{
		uint32_t u4Idx = 0;

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			if (gprWdev[u4Idx] && gprWdev[u4Idx]->netdev) {
				wlanClearDevIdx(gprWdev[u4Idx]->netdev);
				unregister_netdev(gprWdev[u4Idx]->netdev);
			}
		}

		prGlueInfo->fgIsRegistered = FALSE;
	}
#endif

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
void wlanNvramSetState(enum ENUM_NVRAM_STATE state)
{
	g_NvramFsm = state;
}
enum ENUM_NVRAM_STATE wlanNvramGetState(void)
{
	return g_NvramFsm;
}
#if CFG_WLAN_ASSISTANT_NVRAM
static void wlanNvramUpdateOnTestMode(void)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	enum ENUM_NVRAM_STATE nvrmState;
	struct REG_INFO *prRegInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	/* <1> Sanity Check */
	if (u4WlanDevNum == 0) {
		DBGLOG(INIT, ERROR,
			   "wlanNvramUpdateOnTestMode invalid!!\n");
		return;
	}

	prGlueInfo = wlanGetGlueInfo();
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, WARN,
			   "prGlueInfo invalid!!\n");
		return;
	}
	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, WARN,
			   "prAdapter invalid!!\n");
		return;
	}
	prRegInfo = &prGlueInfo->rRegInfo;
	if (prRegInfo == NULL) {
		DBGLOG(INIT, WARN,
			   "prRegInfo invalid!!\n");
		return;
	}

	if (prAdapter->fgTestMode == FALSE) {
		DBGLOG(INIT, INFO,
			   "by-pass on Normal mode\n");
		return;
	}

	nvrmState = wlanNvramGetState();

	if (nvrmState == NVRAM_STATE_READY) {
		DBGLOG(RFTEST, INFO,
		"update nvram to fw on test mode!\n");

		if (kalIsConfigurationExist(prGlueInfo) == TRUE)
			wlanLoadManufactureData(prAdapter, prRegInfo);
		else
			DBGLOG(INIT, WARN, "%s: load manufacture data fail\n",
					   __func__);
	}
}
static uint8_t wlanNvramBufHandler(void *ctx,
			const char *buf,
			uint16_t length)
{
	DBGLOG(INIT, INFO, "buf = %p, length = %u\n", buf, length);
	if (buf == NULL || length <= 0)
		return -EFAULT;

	if (length > sizeof(g_aucNvram)) {
		DBGLOG(INIT, ERROR, "is over nvrm size %d\n",
			sizeof(g_aucNvram));
		return -EINVAL;
	}

	kalMemZero(&g_aucNvram, sizeof(g_aucNvram));
	if (copy_from_user(g_aucNvram, buf, length)) {
		DBGLOG(INIT, ERROR, "copy nvram fail\n");
		g_NvramFsm = NVRAM_STATE_INIT;
		return -EINVAL;
	}

	g_NvramFsm = NVRAM_STATE_READY;

	/*do nvram update on test mode then driver sent new NVRAM to FW*/
	wlanNvramUpdateOnTestMode();

	return 0;
}

#endif

static void wlanCreateWirelessDevice(void)
{
	struct wiphy *prWiphy = NULL;
	struct wireless_dev *prWdev[KAL_AIS_NUM] = {NULL};
	unsigned int u4SupportSchedScanFlag = 0;
	uint32_t u4Idx = 0;

	/* 4 <1.1> Create wireless_dev */
	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		prWdev[u4Idx] = kzalloc(sizeof(struct wireless_dev),
			GFP_KERNEL);
		if (!prWdev[u4Idx]) {
			DBGLOG(INIT, ERROR,
				"Allocating memory to wireless_dev context failed\n");
			return;
		}
		prWdev[u4Idx]->iftype = NL80211_IFTYPE_STATION;
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
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	/* In kernel 4.12 or newer,
	 * this is obsoletes - WIPHY_FLAG_SUPPORTS_SCHED_SCAN
	 */
	prWiphy->max_sched_scan_reqs = 1;
#else
	u4SupportSchedScanFlag            =
		WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
#endif
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

#if KERNEL_VERSION(3, 14, 0) < CFG80211_VERSION_CODE
	prWiphy->max_ap_assoc_sta = P2P_MAXIMUM_CLIENT_COUNT;
#endif

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

#if KERNEL_VERSION(4, 10, 0) < CFG80211_VERSION_CODE
	wiphy_ext_feature_set(prWiphy, NL80211_EXT_FEATURE_LOW_SPAN_SCAN);
#endif
	prWiphy->features |= NL80211_FEATURE_INACTIVITY_TIMER;

#if CFG_SUPPORT_WPA3
	prWiphy->features |= NL80211_FEATURE_SAE;
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
	prWiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	prWiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
	prWiphy->ap_sme_capa = 1;
#endif

#if CFG_ENABLE_OFFCHANNEL_TX
	prWiphy->flags |= WIPHY_FLAG_OFFCHAN_TX;
#endif /* CFG_ENABLE_OFFCHANNEL_TX */

#if CFG_SUPPORT_RM_BEACON_REPORT_BY_SUPPLICANT
	/* Enable following to indicate supplicant
	 * to support Beacon report feature
	 */
	prWiphy->features |= NL80211_FEATURE_DS_PARAM_SET_IE_IN_PROBES;
	prWiphy->features |= NL80211_FEATURE_QUIET;
#endif

	if (wiphy_register(prWiphy) < 0) {
		DBGLOG(INIT, ERROR, "wiphy_register error\n");
		goto free_wiphy;
	}
	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		prWdev[u4Idx]->wiphy = prWiphy;
		gprWdev[u4Idx] = prWdev[u4Idx];
	}
#if CFG_WLAN_ASSISTANT_NVRAM
	register_file_buf_handler(wlanNvramBufHandler, (void *)NULL,
			ENUM_BUF_TYPE_NVRAM);
#endif
	DBGLOG(INIT, INFO, "Create wireless device success\n");
	return;

free_wiphy:
	wiphy_free(prWiphy);
free_wdev:
	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++)
		kfree(prWdev[u4Idx]);
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
		if (wlanIsAisDev(gprP2pRoleWdev[i]->netdev)) {
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
	if (gprWdev[0]) {
#if CFG_ENABLE_UNIFY_WIPHY
		wiphy = wlanGetWiphy();
#else
		/* trunk doesn't do set_wiphy_dev, but trunk-ce1 does. */
		/* set_wiphy_dev(gprWdev->wiphy, NULL); */
		wiphy_unregister(gprWdev[0]->wiphy);
		wiphy_free(gprWdev[0]->wiphy);
#endif

		for (i = 0; i < KAL_AIS_NUM; i++) {
			kfree(gprWdev[i]);
			gprWdev[i] = NULL;
		}
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
	KAL_WAKE_LOCK_INIT(NULL, prGlueInfo->rIntrWakeLock,
			   "WLAN interrupt");
	KAL_WAKE_LOCK_INIT(NULL, prGlueInfo->rTimeoutWakeLock,
			   "WLAN timeout");
#endif
}

void wlanWakeLockUninit(struct GLUE_INFO *prGlueInfo)
{
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	if (KAL_WAKE_LOCK_ACTIVE(NULL, prGlueInfo->rIntrWakeLock))
		KAL_WAKE_UNLOCK(NULL, prGlueInfo->rIntrWakeLock);
	KAL_WAKE_LOCK_DESTROY(NULL, prGlueInfo->rIntrWakeLock);

	if (KAL_WAKE_LOCK_ACTIVE(NULL,
				 prGlueInfo->rTimeoutWakeLock))
		KAL_WAKE_UNLOCK(NULL, prGlueInfo->rTimeoutWakeLock);
	KAL_WAKE_LOCK_DESTROY(NULL, prGlueInfo->rTimeoutWakeLock);
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
struct wireless_dev *wlanNetCreate(void *pvData,
		void *pvDriverData)
{
	struct wireless_dev *prWdev = gprWdev[0];
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
	for (i = 0; i < KAL_AIS_NUM; i++) {
		if (gprWdev[i]
#if CFG_SUPPORT_PERSIST_NETDEV
			&& !gprNetdev[i]
#endif
			) {
			memset(gprWdev[i], 0, sizeof(struct wireless_dev));
			gprWdev[i]->wiphy = prWiphy;
			gprWdev[i]->iftype = NL80211_IFTYPE_STATION;
		}
	}

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
	if (!prDev)
		DBGLOG(INIT, ERROR, "unable to get struct dev for wlan\n");
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
		return NULL;
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

	for (i = 0; i < KAL_AIS_NUM; i++) {
		struct net_device *prDevHandler;

#if CFG_SUPPORT_PERSIST_NETDEV
		if (!gprNetdev[i]) {
			prDevHandler = alloc_netdev_mq(
				sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
				prInfName,
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
				NET_NAME_PREDICTABLE,
#endif
				ether_setup,
				CFG_MAX_TXQ_NUM);
			gprNetdev[i] = prDevHandler;
		} else
			prDevHandler = gprNetdev[i];
#else
		prDevHandler = alloc_netdev_mq(
			sizeof(struct NETDEV_PRIVATE_GLUE_INFO),
			prInfName,
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
			NET_NAME_PREDICTABLE,
#endif
			ether_setup,
			CFG_MAX_TXQ_NUM);
#endif /* end of CFG_SUPPORT_PERSIST_NETDEV */

		if (!prDevHandler) {
			DBGLOG(INIT, ERROR,
				"Allocating memory to net_device context failed\n");
			goto netcreate_err;
		}

		/* Device can help us to save at most 3000 packets,
		 * after we stopped queue
		 */
		prDevHandler->tx_queue_len = 3000;
		DBGLOG(INIT, INFO, "net_device prDev(0x%p) allocated\n",
			prDevHandler);

		/* 4 <3.1.1> Initialize net device varaiables */
		prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prDevHandler);
		prNetDevPrivate->prGlueInfo = prGlueInfo;

		prDevHandler->needed_headroom =
			NIC_TX_DESC_AND_PADDING_LENGTH +
			prChipInfo->txd_append_size;
		prDevHandler->netdev_ops = &wlan_netdev_ops;
#ifdef CONFIG_WIRELESS_EXT
		prDevHandler->wireless_handlers =
			&wext_handler_def;
#endif
		netif_carrier_off(prDevHandler);
		netif_tx_stop_all_queues(prDevHandler);
		kalResetStats(prDevHandler);

		/* 4 <3.1.2> co-relate with wiphy bi-directionally */
		prDevHandler->ieee80211_ptr = gprWdev[i];

		gprWdev[i]->netdev = prDevHandler;

		/* 4 <3.1.3> co-relate net device & prDev */
		SET_NETDEV_DEV(prDevHandler,
				wiphy_dev(prWdev->wiphy));

		/* 4 <3.1.4> set device to glue */
		prGlueInfo->prDev = prDev;

		/* 4 <3.2> Initialize glue variables */
		kalSetMediaStateIndicated(prGlueInfo,
			MEDIA_STATE_DISCONNECTED,
			i);
	}

	prGlueInfo->prDevHandler = gprWdev[0]->netdev;

#if CFG_SUPPORT_SNIFFER
	INIT_WORK(&(prGlueInfo->monWork), wlanMonWorkHandler);
#endif

	prGlueInfo->ePowerState = ParamDeviceStateD0;
#if !CFG_SUPPORT_PERSIST_NETDEV
	prGlueInfo->fgIsRegistered = FALSE;
#endif
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

	for (i = 0; i < MUTEX_NUM; i++)
		mutex_init(&prGlueInfo->arMutex[i]);

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
	prGlueInfo->prDevHandler = NULL;

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
void wlanNetDestroy(struct wireless_dev *prWdev)
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
#if CFG_SUPPORT_PERSIST_NETDEV
#ifdef CONFIG_WIRELESS_EXT
	{
		uint32_t u4Idx = 0;

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			if (gprWdev[u4Idx] && gprWdev[u4Idx]->netdev) {
				rtnl_lock();
				gprWdev[u4Idx]->netdev->wireless_handlers =
					NULL;
				rtnl_unlock();
			}
		}
	}
#endif
#else
	wlanFreeNetDev();
#endif
	/* gPrDev is assigned by prGlueInfo->prDevHandler,
	 * set NULL to this global variable.
	 */
	gPrDev = NULL;

}				/* end of wlanNetDestroy() */

void wlanSetSuspendMode(struct GLUE_INFO *prGlueInfo,
			u_int8_t fgEnable)
{
	struct net_device *prDev = NULL;
	uint32_t u4PacketFilter = 0;
	uint32_t u4SetInfoLen = 0;
	uint32_t u4Idx = 0;

	if (!prGlueInfo)
		return;


	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		prDev = wlanGetNetDev(prGlueInfo, u4Idx);
		if (!prDev)
			continue;

		/* new filter should not include p2p mask */
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
		u4PacketFilter =
			prGlueInfo->prAdapter->u4OsPacketFilter &
			(~PARAM_PACKET_FILTER_P2P_MASK);
#endif
		if (kalIoctl(prGlueInfo,
			wlanoidSetCurrentPacketFilter,
			&u4PacketFilter,
			sizeof(u4PacketFilter), FALSE, FALSE, TRUE,
			&u4SetInfoLen) != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "set packet filter failed.\n");

#if !CFG_SUPPORT_DROP_ALL_MC_PACKET
		if (fgEnable) {
			/* Prepare IPv6 RA packet when suspend */
			uint8_t MC_address[ETH_ALEN] = {0x33, 0x33, 0, 0, 0, 1};

			kalIoctl(prGlueInfo,
				wlanoidSetMulticastList, MC_address, ETH_ALEN,
				FALSE, FALSE, TRUE, &u4SetInfoLen);
		} else if (u4PacketFilter & PARAM_PACKET_FILTER_MULTICAST) {
			/* Prepare multicast address list when resume */
			struct netdev_hw_addr *ha;
			uint8_t *prMCAddrList = NULL;
			uint32_t i = 0;

			down(&g_halt_sem);
			if (g_u4HaltFlag) {
				up(&g_halt_sem);
				return;
			}

			prMCAddrList = kalMemAlloc(
				MAX_NUM_GROUP_ADDR * ETH_ALEN, VIR_MEM_TYPE);
			if (!prMCAddrList) {
				DBGLOG(INIT, WARN,
					"prMCAddrList memory alloc fail!\n");
				up(&g_halt_sem);
				continue;
			}

			/* Avoid race condition with kernel net subsystem */
			netif_addr_lock_bh(prDev);

			netdev_for_each_mc_addr(ha, prDev) {
				if (i < MAX_NUM_GROUP_ADDR) {
					kalMemCopy(
						(prMCAddrList + i * ETH_ALEN),
						ha->addr, ETH_ALEN);
					i++;
				}
			}

			netif_addr_unlock_bh(prDev);

			up(&g_halt_sem);

			kalIoctl(prGlueInfo, wlanoidSetMulticastList,
				prMCAddrList, (i * ETH_ALEN), FALSE, FALSE,
				TRUE, &u4SetInfoLen);

			kalMemFree(prMCAddrList, VIR_MEM_TYPE,
				MAX_NUM_GROUP_ADDR * ETH_ALEN);
		}
#endif
		kalSetNetAddressFromInterface(prGlueInfo, prDev, fgEnable);
		wlanNotifyFwSuspend(prGlueInfo, prDev, fgEnable);
	}
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

void reset_p2p_mode(struct GLUE_INFO *prGlueInfo)
{
	struct PARAM_CUSTOM_P2P_SET_STRUCT rSetP2P;
	uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	if (!prGlueInfo)
		return;

	rSetP2P.u4Enable = 0;
	rSetP2P.u4Mode = 0;

	p2pNetUnregister(prGlueInfo, FALSE);

	rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode,
			(void *) &rSetP2P,
			sizeof(struct PARAM_CUSTOM_P2P_SET_STRUCT),
			FALSE, FALSE, TRUE, &u4BufLen);

	if (rWlanStatus != WLAN_STATUS_SUCCESS)
		prGlueInfo->prAdapter->fgIsP2PRegistered = FALSE;

	DBGLOG(INIT, INFO,
			"ret = 0x%08x\n", (uint32_t) rWlanStatus);
}

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

	/* Remember original ifindex for reset case */
	if (kalIsResetting()) {
		struct GL_P2P_INFO *prP2PInfo = NULL;
		int i = 0;

		for (i = 0 ; i < KAL_P2P_NUM; i++) {
			prP2PInfo = prGlueInfo->prP2PInfo[i];

			if (!prP2PInfo || !prP2PInfo->prDevHandler)
				continue;

			/* Only restore sap part */
			if (prP2PInfo->prWdev->iftype != NL80211_IFTYPE_AP)
				continue;

			g_u4DevIdx[i] =
				prP2PInfo->prDevHandler->ifindex;
		}
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

	if ((!rSetP2P.u4Enable) && (kalIsResetting() == FALSE))
		p2pNetUnregister(prGlueInfo, FALSE);

	rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode,
			(void *) &rSetP2P,
			sizeof(struct PARAM_CUSTOM_P2P_SET_STRUCT),
			FALSE, FALSE, TRUE, &u4BufLen);

	DBGLOG(INIT, INFO,
			"ret = 0x%08x, p2p reg = %d, resetting = %d\n",
			(uint32_t) rWlanStatus,
			prGlueInfo->prAdapter->fgIsP2PRegistered,
			kalIsResetting());


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

		if (prAdapter->rWifiVar.ucEfuseBufferModeCal == TRUE) {
			int ret = 0;

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

			ret = kalSnprintf(aucEeprom, 32, "%s%x.bin",
				 apucEepromName[0], chip_id);
			if (ret < 0 || ret >= 32) {
				DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
						__LINE__, ret);
				goto label_exit;
			}

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

	if (prAdapter->fgIsSupportPowerOnSendBufferModeCMD == FALSE)
		return WLAN_STATUS_SUCCESS;

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
		(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T *)
		kalMemAlloc(sizeof(
			    struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T),
			    VIR_MEM_TYPE);
	if (prSetEfuseBufModeInfo == NULL)
		goto label_exit;
	kalMemZero(prSetEfuseBufModeInfo,
		   sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE_CONNAC_T));

	if (prAdapter->rWifiVar.ucEfuseBufferModeCal == TRUE) {
		int ret = 0;

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

		ret = kalSnprintf(aucEeprom, 32, "%s%x.bin",
			 apucEepromName[0], chip_id);
		if (ret < 0 || ret >= 32) {
			DBGLOG(INIT, ERROR,
				"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);
			goto label_exit;
		}

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

		/* 1 <4> Send CMD with bin file content */
		prGlueInfo = prAdapter->prGlueInfo;

		/* Update contents in local table */
		kalMemCopy(uacEEPROMImage, pucConfigBuf,
			   MAX_EEPROM_BUFFER_SIZE);

		if (u4ContentLen > MAX_EEPROM_BUFFER_SIZE)
			goto label_exit;

		kalMemCopy(prSetEfuseBufModeInfo->aBinContent, pucConfigBuf,
			   u4ContentLen);

		prSetEfuseBufModeInfo->ucSourceMode = 1;
	} else {
		/* eFuse mode */
		/* Only need to tell FW the content from, contents are directly
		 * from efuse
		 */
		prSetEfuseBufModeInfo->ucSourceMode = 0;
		u4ContentLen = 0;
	}
	prSetEfuseBufModeInfo->ucContentFormat = 0x1 |
			(prAdapter->rWifiVar.ucCalTimingCtrl << 4);
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

	return retWlanStat;
}
#if (CONFIG_WLAN_SERVICE == 1)
uint32_t wlanServiceInit(struct GLUE_INFO *prGlueInfo)
{

	struct service_test *prServiceTest;
	struct test_wlan_info *winfos;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(INIT, TRACE, "%s enter!\n", __func__);

	if (prGlueInfo == NULL)
		return WLAN_STATUS_FAILURE;

	prChipInfo = prGlueInfo->prAdapter->chip_info;
	prGlueInfo->rService.serv_id = SERV_HANDLE_TEST;
	prGlueInfo->rService.serv_handle
		= kalMemAlloc(sizeof(struct service_test), VIR_MEM_TYPE);
	if (prGlueInfo->rService.serv_handle == NULL) {
		DBGLOG(INIT, WARN,
			"prGlueInfo->rService.serv_handle memory alloc fail!\n");
			return WLAN_STATUS_FAILURE;
	}

	prServiceTest = (struct service_test *)prGlueInfo->rService.serv_handle;
	prServiceTest->test_winfo
		= kalMemAlloc(sizeof(struct test_wlan_info), VIR_MEM_TYPE);
	if (prServiceTest->test_winfo == NULL) {
		DBGLOG(INIT, WARN,
			"prServiceTest->test_winfo memory alloc fail!\n");
			goto label_exit;
	}
	winfos = prServiceTest->test_winfo;

	prServiceTest->test_winfo->net_dev = gPrDev;

	if (prChipInfo->asicGetChipID)
		prServiceTest->test_winfo->chip_id =
			prChipInfo->asicGetChipID(prGlueInfo->prAdapter);
	else
		prServiceTest->test_winfo->chip_id = prChipInfo->chip_id;

	DBGLOG(INIT, WARN,
			"%s chip_id = 0x%x\n", __func__,
			prServiceTest->test_winfo->chip_id);

#if (CFG_MTK_ANDROID_EMI == 1)
	prServiceTest->test_winfo->emi_phy_base = gConEmiPhyBase;
	prServiceTest->test_winfo->emi_phy_size = gConEmiSize;
#else
	DBGLOG(RFTEST, WARN, "Platform doesn't support EMI address\n");
#endif

	prServiceTest->test_op
		= kalMemAlloc(sizeof(struct test_operation), VIR_MEM_TYPE);
	if (prServiceTest->test_op == NULL) {
		DBGLOG(INIT, WARN,
			"prServiceTest->test_op memory alloc fail!\n");
			goto label_exit;
	}

	prServiceTest->engine_offload = true;
	winfos->oid_funcptr = (wlan_oid_handler_t) ServiceWlanOid;

	rStatus = mt_agent_init_service(&prGlueInfo->rService);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "%s init fail err:%d\n", __func__, rStatus);

	return rStatus;

label_exit:

	/* free memory */
	if (prGlueInfo->rService.serv_handle != NULL) {

		if (prServiceTest->test_winfo != NULL)
			kalMemFree(prServiceTest->test_winfo, VIR_MEM_TYPE,
				   sizeof(struct test_wlan_info));

		if (prServiceTest->test_op != NULL)
			kalMemFree(prServiceTest->test_op, VIR_MEM_TYPE,
				   sizeof(struct test_operation));

		kalMemFree(prGlueInfo->rService.serv_handle, VIR_MEM_TYPE,
			sizeof(struct service_test));
	}

	return WLAN_STATUS_FAILURE;
}
uint32_t wlanServiceExit(struct GLUE_INFO *prGlueInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct service_test *prServiceTest;

	DBGLOG(INIT, TRACE, "%s enter\n", __func__);

	if (prGlueInfo == NULL)
		return WLAN_STATUS_FAILURE;

	rStatus = mt_agent_exit_service(&prGlueInfo->rService);

	prServiceTest = (struct service_test *)prGlueInfo->rService.serv_handle;

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "wlanServiceExit fail err:%d\n", rStatus);

	if (prGlueInfo->rService.serv_handle) {
		if (prServiceTest->test_winfo)
			kalMemFree(prServiceTest->test_winfo,
			VIR_MEM_TYPE, sizeof(struct test_wlan_info));

		if (prServiceTest->test_op)
			kalMemFree(prServiceTest->test_op,
			VIR_MEM_TYPE, sizeof(struct test_operation));

		kalMemFree(prGlueInfo->rService.serv_handle,
		VIR_MEM_TYPE, sizeof(struct service_test));
	}

	prGlueInfo->rService.serv_id = 0;

	return rStatus;
}
#endif

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH

#define FW_LOG_CMD_ON_OFF        0
#define FW_LOG_CMD_SET_LEVEL     1
static uint32_t u4LogOnOffCache;
static uint32_t u4LogLevelCache = -1;

struct CMD_CONNSYS_FW_LOG {
	int32_t fgCmd;
	int32_t fgValue;
};

uint32_t
connsysFwLogControl(struct ADAPTER *prAdapter, void *pvSetBuffer,
	uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct CMD_CONNSYS_FW_LOG *prCmd;
	struct CMD_HEADER rCmdV1Header;
	struct CMD_FORMAT_V1 rCmd_v1;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	if ((prAdapter == NULL) || (pvSetBuffer == NULL)
		|| (pu4SetInfoLen == NULL))
		return WLAN_STATUS_FAILURE;

	/* init */
	*pu4SetInfoLen = sizeof(struct CMD_CONNSYS_FW_LOG);
	prCmd = (struct CMD_CONNSYS_FW_LOG *) pvSetBuffer;

	if (prCmd->fgCmd == FW_LOG_CMD_ON_OFF) {

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

		if (prCmd->fgValue == 1) /* other cases, send 'OFF=0' */
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

		/* keep in cache */
		u4LogOnOffCache = prCmd->fgValue;
	} else if (prCmd->fgCmd == FW_LOG_CMD_SET_LEVEL) {
		/*ENG_LOAD_OFFSET 1*/
		/*USERDEBUG_LOAD_OFFSET 2 */
		/*USER_LOAD_OFFSET 3 */
		int32_t u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

		DBGLOG(INIT, INFO, "FW_LOG_CMD_SET_LEVEL %d\n", prCmd->fgValue);
		switch (prCmd->fgValue) {
		case 0:
			u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
			break;
		case 1:
			u4LogLevel = ENUM_WIFI_LOG_LEVEL_MORE;
			break;
		case 2:
			u4LogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
			break;
		default:
			u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
			break;
		}
		wlanDbgSetLogLevelImpl(prAdapter,
					   ENUM_WIFI_LOG_LEVEL_VERSION_V1,
					   ENUM_WIFI_LOG_MODULE_DRIVER,
					   u4LogLevel);
		wlanDbgSetLogLevelImpl(prAdapter,
					   ENUM_WIFI_LOG_LEVEL_VERSION_V1,
					   ENUM_WIFI_LOG_MODULE_FW,
					   u4LogLevel);
		/* keep in cache */
		u4LogLevelCache = prCmd->fgValue;
	} else {
		DBGLOG(INIT, INFO, "command can not parse\n");
	}
	return WLAN_STATUS_SUCCESS;
}

static void consys_log_event_notification(int cmd, int value)
{
	struct CMD_CONNSYS_FW_LOG rFwLogCmd;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct net_device *prDev = gPrDev;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4BufLen;

	DBGLOG(INIT, INFO, "gPrDev=%p, cmd=%d, value=%d\n",
		gPrDev, cmd, value);

	if (cmd == FW_LOG_CMD_ON_OFF)
		u4LogOnOffCache = value;
	if (cmd == FW_LOG_CMD_SET_LEVEL)
		u4LogLevelCache = value;

	if (kalIsHalted()) { /* power-off */
		DBGLOG(INIT, INFO,
			"Power off return, u4LogOnOffCache=%d\n",
				u4LogOnOffCache);
		return;
	}

	prGlueInfo = (prDev != NULL) ?
		*((struct GLUE_INFO **) netdev_priv(prDev)) : NULL;
	DBGLOG(INIT, TRACE, "prGlueInfo=%p\n", prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, INFO,
			"prGlueInfo == NULL return, u4LogOnOffCache=%d\n",
				u4LogOnOffCache);
		return;
	}
	prAdapter = prGlueInfo->prAdapter;
	DBGLOG(INIT, TRACE, "prAdapter=%p\n", prAdapter);
	if (!prAdapter) {
		DBGLOG(INIT, INFO,
			"prAdapter == NULL return, u4LogOnOffCache=%d\n",
				u4LogOnOffCache);
		return;
	}

	kalMemZero(&rFwLogCmd, sizeof(rFwLogCmd));
	rFwLogCmd.fgCmd = cmd;
	rFwLogCmd.fgValue = value;

	rStatus = kalIoctl(prGlueInfo,
				   connsysFwLogControl,
				   &rFwLogCmd,
				   sizeof(struct CMD_CONNSYS_FW_LOG),
				   FALSE, FALSE, FALSE,
				   &u4BufLen);
}
#endif

static
void wlanOnPreAdapterStart(struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	struct REG_INFO **pprRegInfo,
	struct mt66xx_chip_info **pprChipInfo)
{
	uint32_t u4Idx = 0;
#if CFG_WMT_WIFI_PATH_SUPPORT
	int32_t i4RetVal = 0;
#endif

	DBGLOG(INIT, TRACE, "start.\n");
	prGlueInfo->u4ReadyFlag = 0;
#if CFG_MTK_ANDROID_WMT
	update_driver_loaded_status(prGlueInfo->u4ReadyFlag);
#endif
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	prAdapter->fgIsSupportCsumOffload = FALSE;
	prAdapter->u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
#endif

#if CFG_SUPPORT_CFG_FILE
	wlanGetConfig(prAdapter);
#endif

	/* Init Chip Capability */
	*pprChipInfo = prAdapter->chip_info;
	if ((*pprChipInfo)->asicCapInit)
		(*pprChipInfo)->asicCapInit(prAdapter);

	/* Default support 2.4/5G MIMO */
	prAdapter->rWifiFemCfg.u2WifiPath = (
			WLAN_FLAG_2G4_WF0 | WLAN_FLAG_5G_WF0 |
			WLAN_FLAG_2G4_WF1 | WLAN_FLAG_5G_WF1);

#if CFG_WMT_WIFI_PATH_SUPPORT
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
	*pprRegInfo = &prGlueInfo->rRegInfo;

	/* P_REG_INFO_T prRegInfo = (P_REG_INFO_T) kmalloc(
	 *				sizeof(REG_INFO_T), GFP_KERNEL);
	 */
	kalMemSet(*pprRegInfo, 0, sizeof(struct REG_INFO));

	/* Trigger the action of switching Pwr state to drv_own */
	prAdapter->fgIsFwOwn = TRUE;

	nicpmWakeUpWiFi(prAdapter);

	/* Load NVRAM content to REG_INFO_T */
	glLoadNvram(prGlueInfo, *pprRegInfo);

	/* kalMemCopy(&prGlueInfo->rRegInfo, prRegInfo,
	 *            sizeof(REG_INFO_T));
	 */

	(*pprRegInfo)->u4PowerMode = CFG_INIT_POWER_SAVE_PROF;
#if 0
		prRegInfo->fgEnArpFilter = TRUE;
#endif

	/* The Init value of u4WpaVersion/u4AuthAlg shall be
	 * DISABLE/OPEN, not zero!
	 */
	/* The Init value of u4CipherGroup/u4CipherPairwise shall be
	 * NONE, not zero!
	 */
	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		struct GL_WPA_INFO *prWpaInfo =
			aisGetWpaInfo(prAdapter, u4Idx);

		if (!prWpaInfo)
			continue;

		prWpaInfo->u4WpaVersion =
			IW_AUTH_WPA_VERSION_DISABLED;
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
		prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	}

	tasklet_init(&prGlueInfo->rRxTask, halRxTasklet,
			(unsigned long)prGlueInfo);
	tasklet_init(&prGlueInfo->rTxCompleteTask,
			halTxCompleteTasklet,
			(unsigned long)prGlueInfo);
}

static
void wlanOnPostAdapterStart(struct ADAPTER *prAdapter,
	struct GLUE_INFO *prGlueInfo)
{
	DBGLOG(INIT, TRACE, "start.\n");
	if (HAL_IS_TX_DIRECT(prAdapter)) {
		if (!prAdapter->fgTxDirectInited) {
			skb_queue_head_init(
					&prAdapter->rTxDirectSkbQueue);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
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
}

static int32_t wlanOnPreNetRegister(struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	struct mt66xx_chip_info *prChipInfo,
	struct WIFI_VAR *prWifiVar,
	const u_int8_t bAtResetFlow)
{
	uint32_t i;

	DBGLOG(INIT, TRACE, "start.\n");

	if (!bAtResetFlow) {
		/* change net device mtu from feature option */
		if (prWifiVar->u4MTU > 0 && prWifiVar->u4MTU <= ETH_DATA_LEN) {
			for (i = 0; i < KAL_AIS_NUM; i++)
				gprWdev[i]->netdev->mtu = prWifiVar->u4MTU;
		}
		INIT_DELAYED_WORK(&prGlueInfo->rRxPktDeAggWork,
				halDeAggRxPktWorker);
	}
	INIT_WORK(&prGlueInfo->rTxMsduFreeWork, kalFreeTxMsduWorker);

	prGlueInfo->main_thread = kthread_run(main_thread,
			prGlueInfo->prDevHandler, "main_thread");
#if CFG_SUPPORT_MULTITHREAD
	prGlueInfo->hif_thread = kthread_run(hif_thread,
			prGlueInfo->prDevHandler, "hif_thread");
	prGlueInfo->rx_thread = kthread_run(rx_thread,
			prGlueInfo->prDevHandler, "rx_thread");
#endif
	/* TODO the change schedule API shall be provided by OS glue
	 * layer
	 */
	/* Switch the Wi-Fi task priority to higher priority and change
	 * the scheduling method
	 */
	if (prGlueInfo->prAdapter->rWifiVar.ucThreadPriority > 0) {
#if KERNEL_VERSION(4, 19, 0) >= LINUX_VERSION_CODE
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
#endif
		DBGLOG(INIT, INFO,
		       "Set pri = %d, sched = %d\n",
		       prGlueInfo->prAdapter->rWifiVar.ucThreadPriority,
		       prGlueInfo->prAdapter->rWifiVar
		       .ucThreadScheduling);
	}

	if (!bAtResetFlow) {
		g_u4HaltFlag = 0;

#if CFG_SUPPORT_BUFFER_MODE
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
		if (prChipInfo->downloadBufferBin) {
			if (prChipInfo->downloadBufferBin(prAdapter) !=
					WLAN_STATUS_SUCCESS)
				return -1;
		}
#endif
#endif

#if CFG_SUPPORT_DBDC
		/* Update DBDC default setting */
		cnmInitDbdcSetting(prAdapter);
#endif /*CFG_SUPPORT_DBDC*/
	}

	/* send regulatory information to firmware */
	rlmDomainSendInfoToFirmware(prAdapter);

	/* set MAC address */
	if (!bAtResetFlow) {
		uint32_t rStatus = WLAN_STATUS_FAILURE;
		struct sockaddr MacAddr;
		uint32_t u4SetInfoLen = 0;
		struct net_device *prDevHandler;

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

		/* wlan1 */
		if (KAL_AIS_NUM > 1) {
			prDevHandler = wlanGetNetDev(prGlueInfo, 1);
			if (prDevHandler) {
				kalMemCopy(prDevHandler->dev_addr,
					&prAdapter->rWifiVar.aucMacAddress1,
					ETH_ALEN);
				kalMemCopy(prDevHandler->perm_addr,
					prDevHandler->dev_addr,
					ETH_ALEN);
#if CFG_SHOW_MACADDR_SOURCE
				DBGLOG(INIT, INFO,
					"MAC1 address: " MACSTR,
					MAC2STR(prDevHandler->dev_addr));
#endif
			}
		}
	}

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	/* set HW checksum offload */
	if (!bAtResetFlow && prAdapter->fgIsSupportCsumOffload) {
		uint32_t rStatus = WLAN_STATUS_FAILURE;
		uint32_t u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
		uint32_t u4SetInfoLen = 0;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetCSUMOffload,
				   (void *) &u4CSUMFlags,
				   sizeof(uint32_t),
				   FALSE, FALSE, TRUE, &u4SetInfoLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			for (i = 0; i < KAL_AIS_NUM; i++)
				gprWdev[i]->netdev->features |=
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
		uint32_t u4Idx = 0;

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			uint32_t rStatus = WLAN_STATUS_FAILURE;
			uint32_t u4SetInfoLen = 0;

			rStatus = kalIoctlByBssIdx(prGlueInfo,
					wlanoidSync11kCapabilities, NULL, 0,
					FALSE, FALSE, TRUE, &u4SetInfoLen,
					u4Idx);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, WARN,
					"[%d] Set 11k Capabilities fail 0x%x\n",
					u4Idx, rStatus);
		}
	}
#endif
	return 0;
}

static void wlanOnPostNetRegister(void)
{
	DBGLOG(INIT, TRACE, "start.\n");
	/* 4 <4> Register early suspend callback */
#if CFG_ENABLE_EARLY_SUSPEND
	glRegisterEarlySuspend(&wlan_early_suspend_desc,
			       wlan_early_suspend, wlan_late_resume);
#endif
	/* 4 <5> Register Notifier callback */
	wlanRegisterInetAddrNotifier();
}

static
void wlanOnP2pRegistration(struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	struct wireless_dev *prWdev)
{
	DBGLOG(INIT, TRACE, "start.\n");

#if (CFG_ENABLE_WIFI_DIRECT && CFG_MTK_ANDROID_WMT)
	register_set_p2p_mode_handler(set_p2p_mode_handler);
#endif

#if CFG_ENABLE_WIFI_DIRECT
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
}

static
int32_t wlanOnWhenProbeSuccess(struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	const u_int8_t bAtResetFlow)
{
	int32_t u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	DBGLOG(INIT, TRACE, "start.\n");

#if CFG_SUPPORT_EASY_DEBUG
	/* move before reading file
	 * wlanLoadDefaultCustomerSetting(prAdapter);
	 */
	wlanFeatureToFw(prGlueInfo->prAdapter, WLAN_CFG_DEFAULT);

	/*if driver backup Engineer Mode CFG setting before*/
	wlanResoreEmCfgSetting(prGlueInfo->prAdapter);
	wlanFeatureToFw(prGlueInfo->prAdapter, WLAN_CFG_EM);
#endif

#if CFG_SUPPORT_IOT_AP_BLACKLIST
	wlanCfgLoadIotApRule(prAdapter);
	wlanCfgDumpIotApRule(prAdapter);
#endif
	if (!bAtResetFlow) {
#if CFG_SUPPORT_AGPS_ASSIST
		kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_ON, NULL,
				0);
#endif

		wlanCfgSetSwCtrl(prGlueInfo->prAdapter);
		wlanCfgSetChip(prGlueInfo->prAdapter);
#if (CFG_SUPPORT_CONNINFRA == 1)
		wlanCfgSetChipSyncTime(prGlueInfo->prAdapter);
#endif
		wlanCfgSetCountryCode(prGlueInfo->prAdapter);
		kalPerMonInit(prGlueInfo);
#if CFG_MET_TAG_SUPPORT
		if (met_tag_init() != 0)
			DBGLOG(INIT, ERROR, "MET_TAG_INIT error!\n");
#endif
	}

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

	/* card is ready */
	prGlueInfo->u4ReadyFlag = 1;
#if CFG_MTK_ANDROID_WMT
		update_driver_loaded_status(prGlueInfo->u4ReadyFlag);
#endif
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
	consys_log_event_notification((int)FW_LOG_CMD_ON_OFF,
		u4LogOnOffCache);
	if (u4LogLevelCache != -1) {
		consys_log_event_notification((int)FW_LOG_CMD_SET_LEVEL,
			u4LogLevelCache);
	}
#endif

#if CFG_CHIP_RESET_HANG
	if (fgIsResetHangState == SER_L0_HANG_RST_TRGING) {
		DBGLOG(INIT, STATE, "[SER][L0] SET hang!\n");
			fgIsResetHangState = SER_L0_HANG_RST_HANG;
			fgIsResetting = TRUE;
	}
	DBGLOG(INIT, STATE, "[SER][L0] PASS!!\n");
#endif

#if CFG_MTK_MCIF_WIFI_SUPPORT
	mddpNotifyDrvMac(prGlueInfo->prAdapter);
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE
	wlanProbeSuccessForLowLatency(prAdapter);
#endif

#if (CFG_SUPPORT_CONNINFRA == 1)
	if (prAdapter->chip_info->checkbushang) {
		fw_log_bug_hang_register(prAdapter->chip_info->checkbushang);
	}
#endif

	wlanOnP2pRegistration(prGlueInfo, prAdapter, gprWdev[0]);
	if (prAdapter->u4HostStatusEmiOffset)
		kalSetSuspendFlagToEMI(prAdapter, FALSE);
	return 0;
}

void wlanOffStopWlanThreads(IN struct GLUE_INFO *prGlueInfo)
{
	DBGLOG(INIT, TRACE, "start.\n");

	if (prGlueInfo->main_thread == NULL &&
	    prGlueInfo->hif_thread == NULL &&
	    prGlueInfo->rx_thread == NULL) {
		DBGLOG(INIT, INFO,
			"Threads are already NULL, skip stop and free\n");
		return;
	}

#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&prGlueInfo->waitq_hif);
	wait_for_completion_interruptible(
		&prGlueInfo->rHifHaltComp);
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

	if (test_and_clear_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag) &&
			!completion_done(&prGlueInfo->rPendComp)) {
		struct GL_IO_REQ *prIoReq;

		DBGLOG(INIT, INFO, "Complete on-going ioctl as failure.\n");
		prIoReq = &(prGlueInfo->OidEntry);
		prIoReq->rStatus = WLAN_STATUS_FAILURE;
		complete(&prGlueInfo->rPendComp);
	}
}


#if CFG_CHIP_RESET_SUPPORT
/*----------------------------------------------------------------------------*/
/*!
 * \brief slight off procedure for chip reset
 *
 * \return
 * WLAN_STATUS_FAILURE - reset off fail
 * WLAN_STATUS_SUCCESS - reset off success
 */
/*----------------------------------------------------------------------------*/
static int32_t wlanOffAtReset(void)
{
	struct ADAPTER *prAdapter = NULL;
	struct net_device *prDev = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, INFO, "Driver Off during Reset\n");

	if (u4WlanDevNum > 0
		&& u4WlanDevNum <= CFG_MAX_WLAN_DEVICES) {
		prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	}

	if (prDev == NULL) {
		DBGLOG(INIT, ERROR, "prDev is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, INFO, "prGlueInfo is NULL\n");
		wlanFreeNetDev();
		return WLAN_STATUS_FAILURE;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, INFO, "prAdapter is NULL\n");
		wlanFreeNetDev();
		return WLAN_STATUS_FAILURE;
	}

	/* to avoid that wpa_supplicant/hostapd triogger new cfg80211 command */
	prGlueInfo->u4ReadyFlag = 0;
#if CFG_MTK_ANDROID_WMT
	update_driver_loaded_status(prGlueInfo->u4ReadyFlag);
#endif
	kalPerMonDestroy(prGlueInfo);

	/* complete possible pending oid, which may block wlanRemove some time
	 * and then whole chip reset may failed
	 */
	wlanReleasePendingOid(prGlueInfo->prAdapter, 1);

	flush_delayed_work(&workq);

	flush_delayed_work(&sched_workq);

	/* 4 <2> Mark HALT, notify main thread to stop, and clean up queued
	 *	 requests
	 */
	set_bit(GLUE_FLAG_HALT_BIT, &prGlueInfo->ulFlag);

	/* Stop works */
	flush_work(&prGlueInfo->rTxMsduFreeWork);

	wlanOffStopWlanThreads(prGlueInfo);

	wlanAdapterStop(prAdapter, TRUE);

	/* 4 <x> Stopping handling interrupt and free IRQ */
	glBusFreeIrq(prDev, prGlueInfo);

#if (CFG_SUPPORT_TRACE_TC4 == 1)
	wlanDebugTC4Uninit();
#endif

#if (CFG_SUPPORT_STATISTICS == 1)
	wlanWakeStaticsUninit();
#endif

	fgSimplifyResetFlow = TRUE;

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief A slight wlan on for chip reset
 *
 * \return
 * WLAN_STATUS_FAILURE - reset on fail
 * WLAN_STATUS_SUCCESS - reset on success
 */
/*----------------------------------------------------------------------------*/
static int32_t wlanOnAtReset(void)
{
	struct net_device *prDev = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t u4DisconnectReason = DISCONNECT_REASON_CODE_CHIPRESET;
	uint32_t u4Idx = 0;

	enum ENUM_PROBE_FAIL_REASON {
		BUS_INIT_FAIL,
		NET_CREATE_FAIL,
		BUS_SET_IRQ_FAIL,
		ADAPTER_START_FAIL,
		NET_REGISTER_FAIL,
		PROC_INIT_FAIL,
		FAIL_MET_INIT_PROCFS,
		FAIL_REASON_NUM
	} eFailReason = FAIL_REASON_NUM;

	DBGLOG(INIT, INFO, "Driver On during Reset\n");

	if (u4WlanDevNum > 0
		&& u4WlanDevNum <= CFG_MAX_WLAN_DEVICES) {
		prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;
	}

	if (prDev == NULL) {
		DBGLOG(INIT, ERROR, "prDev is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	if (prGlueInfo == NULL) {
		DBGLOG(INIT, INFO, "prGlueInfo is NULL\n");
		wlanFreeNetDev();
		return WLAN_STATUS_FAILURE;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, INFO, "prAdapter is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prGlueInfo->ulFlag = 0;
	fgSimplifyResetFlow = FALSE;
	do {
#if (CFG_SUPPORT_TRACE_TC4 == 1)
		wlanDebugTC4Init();
#endif
#if (CFG_SUPPORT_STATISTICS == 1)
		wlanWakeStaticsInit();
#endif
		/* wlanNetCreate partial process */
		QUEUE_INITIALIZE(&prGlueInfo->rCmdQueue);
		prGlueInfo->i4TxPendingCmdNum = 0;
		QUEUE_INITIALIZE(&prGlueInfo->rTxQueue);

		rStatus = glBusSetIrq(prDev, NULL, prGlueInfo);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "Set IRQ error\n");
			eFailReason = BUS_SET_IRQ_FAIL;
			break;
		}

		/* Trigger the action of switching Pwr state to drv_own */
		prAdapter->fgIsFwOwn = TRUE;

		/* wlanAdapterStart Section Start */
		rStatus = wlanAdapterStart(prAdapter,
					   &prGlueInfo->rRegInfo,
					   TRUE);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			eFailReason = ADAPTER_START_FAIL;
			break;
		}

		wlanOnPreNetRegister(prGlueInfo, prAdapter,
			prAdapter->chip_info,
			&prAdapter->rWifiVar,
			TRUE);

		/* Resend schedule scan */
		prAdapter->rWifiVar.rScanInfo.fgSchedScanning = FALSE;
#if CFG_SUPPORT_SCHED_SCAN
		if (prGlueInfo->prSchedScanRequest) {
			rStatus = kalIoctl(prGlueInfo, wlanoidSetStartSchedScan,
					prGlueInfo->prSchedScanRequest,
					sizeof(struct PARAM_SCHED_SCAN_REQUEST),
					false, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, WARN,
				"SCN: Start sched scan failed after chip reset 0x%x\n",
					rStatus);
		}
#endif

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			struct FT_IES *prFtIEs =
				aisGetFtIe(prAdapter, u4Idx);
			struct CONNECTION_SETTINGS *prConnSettings =
				aisGetConnSettings(prAdapter, u4Idx);

			kalMemZero(prFtIEs,
				sizeof(*prFtIEs));
			prConnSettings->fgIsScanReqIssued = FALSE;
		}

	} while (FALSE);

	if (rStatus == WLAN_STATUS_SUCCESS) {
		wlanOnWhenProbeSuccess(prGlueInfo, prAdapter, TRUE);
		DBGLOG(INIT, INFO, "reset success\n");

		/* Send disconnect */
		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidSetDisassociate,
				&u4DisconnectReason,
				0, FALSE, FALSE, TRUE, &u4BufLen,
				u4Idx);

			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(REQ, WARN,
					"disassociate error:%x\n", rStatus);
				continue;
			}
			DBGLOG(INIT, INFO,
				"%d inform disconnected\n", u4Idx);
		}
	} else {
		prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
		halPrintHifDbgInfo(prAdapter);
		DBGLOG(INIT, WARN, "Fail reason: %d\n", eFailReason);

		/* Remove error handling here, leave it to coming wlanRemove
		 * for full clean.
		 *
		 * If WMT being removed in the future, you should invoke
		 * wlanRemove directly from here
		 */
#if 0
		switch (eFailReason) {
		case ADAPTER_START_FAIL:
			glBusFreeIrq(prDev,
				*((struct GLUE_INFO **)
						netdev_priv(prDev)));
		/* fallthrough */
		case BUS_SET_IRQ_FAIL:
			wlanWakeLockUninit(prGlueInfo);
			wlanNetDestroy(prDev->ieee80211_ptr);
			/* prGlueInfo->prAdapter is released in
			 * wlanNetDestroy
			 */
			/* Set NULL value for local prAdapter as well */
			prAdapter = NULL;
			break;
		default:
			break;
		}
#endif
	}
	return rStatus;
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
		FAIL_BY_RESET,
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
	struct WIFI_VAR *prWifiVar;
	uint32_t u4Idx = 0;

#if CFG_MTK_MCIF_WIFI_SUPPORT
	mddpNotifyWifiOnStart();
#endif

#if CFG_CHIP_RESET_SUPPORT
	if (fgSimplifyResetFlow) {
		i4Status = wlanOnAtReset();
#if CFG_MTK_MCIF_WIFI_SUPPORT
		if (i4Status == WLAN_STATUS_SUCCESS)
			mddpNotifyWifiOnEnd();
#endif
		return i4Status;
	}
#endif

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

		DBGLOG(INIT, INFO, "enter wlanProbe\n");

		bRet = glBusInit(pvData);

#if (CFG_SUPPORT_TRACE_TC4 == 1)
		wlanDebugTC4Init();
#endif

#if (CFG_SUPPORT_STATISTICS == 1)
		wlanWakeStaticsInit();
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
		prWifiVar = &prAdapter->rWifiVar;

		wlanOnPreAdapterStart(prGlueInfo,
			prAdapter,
			&prRegInfo,
			&prChipInfo);

		if (wlanAdapterStart(prAdapter,
				     prRegInfo, FALSE) != WLAN_STATUS_SUCCESS)
			i4Status = -EIO;

		wlanOnPostAdapterStart(prAdapter, prGlueInfo);

		/* kfree(prRegInfo); */

		if (i4Status < 0) {
			eFailReason = ADAPTER_START_FAIL;
			break;
		}

		wlanOnPreNetRegister(prGlueInfo, prAdapter, prChipInfo,
			prWifiVar, FALSE);

		/* 4 <3> Register the card */
		i4DevIdx = wlanNetRegister(prWdev);
		if (i4DevIdx < 0) {
			i4Status = -ENXIO;
			DBGLOG(INIT, ERROR,
			       "wlanProbe: Cannot register the net_device context to the kernel\n");
			eFailReason = NET_REGISTER_FAIL;
			break;
		}

		wlanOnPostNetRegister();

		/* 4 <6> Initialize /proc filesystem */
#if WLAN_INCLUDE_PROC
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

#if CFG_MET_PACKET_TRACE_SUPPORT
	kalMetInit(prGlueInfo);
#endif

		kalWlanUeventInit();

#if CFG_ENABLE_BT_OVER_WIFI
	prGlueInfo->rBowInfo.fgIsNetRegistered = FALSE;
	prGlueInfo->rBowInfo.fgIsRegistered = FALSE;
	glRegisterAmpc(prGlueInfo);
#endif

#if (CONFIG_WLAN_SERVICE == 1)
	wlanServiceInit(prGlueInfo);
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

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			struct FT_IES *prFtIEs =
				aisGetFtIe(prAdapter, u4Idx);

			kalMemZero(prFtIEs,
				sizeof(*prFtIEs));
		}

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

	if (i4Status == 0 && kalIsResetting()) {
		DBGLOG(INIT, WARN, "Fake wlan on success due to reset.\n");
		eFailReason = FAIL_BY_RESET;
		i4Status = WLAN_STATUS_FAILURE;
	}

	if (i4Status == 0) {
		wlanOnWhenProbeSuccess(prGlueInfo, prAdapter, FALSE);
		DBGLOG(INIT, INFO,
		       "wlanProbe: probe success, feature set: 0x%llx, persistNetdev: %d\n",
		       wlanGetSupportedFeatureSet(prGlueInfo),
		       CFG_SUPPORT_PERSIST_NETDEV);
#if CFG_MTK_MCIF_WIFI_SUPPORT
		mddpNotifyWifiOnEnd();
#endif
#if ARP_BRUST_OPTIMIZE
		arp_brust_opt_init(prAdapter);
#endif
	} else {
		DBGLOG(INIT, ERROR, "wlanProbe: probe failed, reason:%d\n",
		       eFailReason);
		switch (eFailReason) {
		case FAIL_BY_RESET:
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
			wlanAdapterStop(prAdapter, FALSE);
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
	return i4Status;
}				/* end of wlanProbe() */

void
wlanOffNotifyCfg80211Disconnect(IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4Idx = 0;
	u_int8_t bNotify = FALSE;

	DBGLOG(INIT, TRACE, "start.\n");

	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		if (kalGetMediaStateIndicated(prGlueInfo,
			u4Idx) ==
		    MEDIA_STATE_CONNECTED) {
			struct net_device *prDevHandler =
				wlanGetNetDev(prGlueInfo, u4Idx);
			if (!prDevHandler)
				continue;
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 2, 0) <= CFG80211_VERSION_CODE)
			cfg80211_disconnected(
				prDevHandler, 0, NULL, 0,
				TRUE, GFP_KERNEL);
#else
			cfg80211_disconnected(
				prDevHandler, 0, NULL, 0,
				GFP_KERNEL);
#endif
			bNotify = TRUE;
		}
	}
	if (bNotify)
		kalMsleep(500);
}

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

	DBGLOG(INIT, INFO, "Remove wlan!\n");

	kalSetHalted(TRUE);

	/*reset NVRAM State to ready for the next wifi-no*/
	if (g_NvramFsm == NVRAM_STATE_SEND_TO_FW)
		g_NvramFsm = NVRAM_STATE_READY;


#if CFG_MTK_MCIF_WIFI_SUPPORT
	mddpNotifyWifiOffStart();
#endif

#if CFG_CHIP_RESET_SUPPORT
	/* During chip reset, use simplify remove flow first
	 * if anything goes wrong in wlanOffAtReset then goes to normal flow
	 */
	if (fgSimplifyResetFlow) {
		if (wlanOffAtReset() == WLAN_STATUS_SUCCESS) {
#if CFG_MTK_MCIF_WIFI_SUPPORT
			mddpNotifyWifiOffEnd();
#endif
			return;
		}
	}
#endif

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
		DBGLOG(INIT, INFO, "prGlueInfo is NULL\n");
		wlanFreeNetDev();
		return;
	}

#if (CONFIG_WLAN_SERVICE == 1)
	wlanServiceExit(prGlueInfo);
#endif

	/* to avoid that wpa_supplicant/hostapd triogger new cfg80211 command */
	prGlueInfo->u4ReadyFlag = 0;
#if CFG_MTK_ANDROID_WMT
	update_driver_loaded_status(prGlueInfo->u4ReadyFlag);
#endif

	/* Have tried to do scan done here, but the exception occurs for */
	/* the P2P scan. Keep the original design that scan done in the	 */
	/* p2pStop/wlanStop.						 */

#if WLAN_INCLUDE_PROC
	procRemoveProcfs();
#endif /* WLAN_INCLUDE_PROC */
#if WLAN_INCLUDE_SYS
	sysRemoveSysfs();
#endif /* WLAN_INCLUDE_SYS */

	prAdapter = prGlueInfo->prAdapter;
	kalPerMonDestroy(prGlueInfo);

	/* Unregister notifier callback */
	wlanUnregisterInetAddrNotifier();

	/*backup EM mode cfg setting*/
	wlanBackupEmCfgSetting(prAdapter);

	/* complete possible pending oid, which may block wlanRemove some time
	 * and then whole chip reset may failed
	 */
	if (kalIsResetting())
		wlanReleasePendingOid(prGlueInfo->prAdapter, 1);

	nicSerDeInit(prGlueInfo->prAdapter);

#if CFG_ENABLE_BT_OVER_WIFI
	if (prGlueInfo->rBowInfo.fgIsNetRegistered) {
		bowNotifyAllLinkDisconnected(prGlueInfo->prAdapter);
		/* wait 300ms for BoW module to send deauth */
		kalMsleep(300);
	}
#endif

	wlanOffNotifyCfg80211Disconnect(prGlueInfo);

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

	wlanOffStopWlanThreads(prGlueInfo);

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

	kalWlanUeventDeinit();

#if CFG_MET_TAG_SUPPORT
	if (GL_MET_TAG_UNINIT() != 0)
		DBGLOG(INIT, ERROR, "MET_TAG_UNINIT error!\n");
#endif

	/* 4 <4> wlanAdapterStop */
#if CFG_SUPPORT_AGPS_ASSIST
	kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_OFF, NULL,
			      0);
#endif

	wlanAdapterStop(prAdapter, FALSE);

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

#if (CFG_SUPPORT_STATISTICS == 1)
	wlanWakeStaticsUninit();
#endif

	/* 4 <6> Unregister the card */
	wlanNetUnregister(prDev->ieee80211_ptr);

	flush_delayed_work(&workq);

	/* 4 <7> Destroy the device */
	wlanNetDestroy(prDev->ieee80211_ptr);
	prDev = NULL;

	tasklet_kill(&prGlueInfo->rTxCompleteTask);
	tasklet_kill(&prGlueInfo->rRxTask);

	/* 4 <8> Unregister early suspend callback */
#if CFG_ENABLE_EARLY_SUSPEND
	glUnregisterEarlySuspend(&wlan_early_suspend_desc);
#endif

#if !CFG_SUPPORT_PERSIST_NETDEV
	{
		uint32_t u4Idx = 0;

		for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
			if (gprWdev[u4Idx] && gprWdev[u4Idx]->netdev)
				gprWdev[u4Idx]->netdev = NULL;
		}
	}
#endif

#if CFG_CHIP_RESET_SUPPORT
	fgIsResetting = FALSE;
#if (CFG_SUPPORT_CONNINFRA == 1)
	update_driver_reset_status(fgIsResetting);
#endif
#endif

#if CFG_MTK_MCIF_WIFI_SUPPORT
	mddpNotifyWifiOffEnd();
#endif
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

#if defined(UT_TEST_MODE) && defined(CFG_BUILD_X86_PLATFORM)
	/* Refer 6765 dts setting */
	char *ptr = NULL;

	gConEmiSize = 0x400000;
	ptr = kmalloc(gConEmiSize, GFP_KERNEL);
	if (!ptr) {
		DBGLOG(INIT, INFO,
		       "initWlan try to allocate 0x%x bytes memory error\n",
		       gConEmiSize);
		return -EINVAL;
	}
	memset(ptr, 0, gConEmiSize);
	gConEmiPhyBase = (phys_addr_t)ptr;
#endif



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
#if WLAN_INCLUDE_SYS
	sysInitFs();
#endif

	wlanCreateWirelessDevice();
	if (gprWdev[0] == NULL)
		return -ENOMEM;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(
			     wlanGetWiphy());
	if (gprWdev[0]) {
		/* P2PDev and P2PRole[0] share the same Wdev */
		if (glP2pCreateWirelessDevice(prGlueInfo) == TRUE)
			gprP2pWdev = gprP2pRoleWdev[0];
	}
	gPrDev = NULL;

#if CFG_MTK_ANDROID_WMT && (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_wmt_mpu_lock_aquire();
#endif
	ret = ((glRegisterBus(wlanProbe,
			      wlanRemove) == WLAN_STATUS_SUCCESS) ? 0 : -EIO);
#ifdef CONFIG_MTK_EMI
	/* Set WIFI EMI protection to consys permitted on system boot up */
	kalSetEmiMpuProtection(gConEmiPhyBase, true);
#endif
#if CFG_MTK_ANDROID_WMT && (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_wmt_mpu_lock_release();
#endif

	if (ret == -EIO) {
		kalUninitIOBuffer();
		return ret;
	}
#if (CFG_CHIP_RESET_SUPPORT)
	glResetInit(prGlueInfo);
#endif
	kalFbNotifierReg((struct GLUE_INFO *) wiphy_priv(
				 wlanGetWiphy()));
	wlanRegisterNetdevNotifier();

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	wifi_fwlog_event_func_register(consys_log_event_notification);
#endif

#if CFG_MTK_MCIF_WIFI_SUPPORT
	mddpInit();
#endif

	g_u4WlanInitFlag = 1;
	DBGLOG(INIT, INFO, "initWlan::End\n");

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
#if CFG_SUPPORT_PERSIST_NETDEV
	uint32_t u4Idx = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct wiphy *wiphy = NULL;

	wiphy = wlanGetWiphy();
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	for (u4Idx = 0; u4Idx < KAL_AIS_NUM; u4Idx++) {
		if (gprNetdev[u4Idx]) {
			wlanClearDevIdx(gprWdev[u4Idx]->netdev);
			DBGLOG(INIT, INFO, "Unregister wlan%d netdev start.\n",
					u4Idx);
			unregister_netdev(gprWdev[u4Idx]->netdev);
			DBGLOG(INIT, INFO, "Unregister wlan%d netdev end.\n",
					u4Idx);
			gprWdev[u4Idx]->netdev = gprNetdev[u4Idx] = NULL;
		}
	}

	prGlueInfo->fgIsRegistered = FALSE;

	DBGLOG(INIT, INFO, "Free wlan device..\n");
	wlanFreeNetDev();
#endif
	kalFbNotifierUnReg();
	wlanUnregisterNetdevNotifier();

	/* printk("remove %p\n", wlanRemove); */
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
#if WLAN_INCLUDE_SYS
	sysUninitSysFs();
#endif
#if defined(UT_TEST_MODE) && defined(CFG_BUILD_X86_PLATFORM)
	kfree((const void *)gConEmiPhyBase);
#endif

	g_u4WlanInitFlag = 0;
	DBGLOG(INIT, INFO, "exitWlan\n");

}				/* end of exitWlan() */

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
