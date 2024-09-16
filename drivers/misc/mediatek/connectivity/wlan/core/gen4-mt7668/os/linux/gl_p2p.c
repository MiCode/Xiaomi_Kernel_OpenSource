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
** Id: @(#) gl_p2p.c@@
*/

/*! \file   gl_p2p.c
*    \brief  Main routines of Linux driver interface for Wi-Fi Direct
*
*    This file contains the main routines of Linux driver for MediaTek Inc. 802.11
*    Wireless LAN Adapters.
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/poll.h>

#include <linux/kmod.h>


#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"

/* #include <net/cfg80211.h> */
#include "gl_p2p_ioctl.h"

#include "precomp.h"
#include "gl_vendor.h"
#include "gl_cfg80211.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define ARGV_MAX_NUM        (4)

/*For CFG80211 - wiphy parameters*/
#define MAX_SCAN_LIST_NUM   (1)
#define MAX_SCAN_IE_LEN     (512)

#if 0
#define RUNNING_P2P_MODE 0
#define RUNNING_AP_MODE 1
#define RUNNING_DUAL_AP_MODE 2
#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

struct net_device *g_P2pPrDev;
struct wireless_dev *gprP2pWdev;
struct wireless_dev *gprP2pRoleWdev[KAL_P2P_NUM];
struct net_device *gPrP2pDev[KAL_P2P_NUM];

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
static struct cfg80211_ops mtk_p2p_ops = {
#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0)
	/* Froyo */
	.add_virtual_intf = mtk_p2p_cfg80211_add_iface,
	.change_virtual_intf = mtk_p2p_cfg80211_change_iface,	/* 1 st */
	.del_virtual_intf = mtk_p2p_cfg80211_del_iface,
	.change_bss = mtk_p2p_cfg80211_change_bss,
	.scan = mtk_p2p_cfg80211_scan,
	.remain_on_channel = mtk_p2p_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = mtk_p2p_cfg80211_cancel_remain_on_channel,
	.mgmt_tx = mtk_p2p_cfg80211_mgmt_tx,
	.mgmt_tx_cancel_wait = mtk_p2p_cfg80211_mgmt_tx_cancel_wait,
	.connect = mtk_p2p_cfg80211_connect,
	.disconnect = mtk_p2p_cfg80211_disconnect,
	.deauth = mtk_p2p_cfg80211_deauth,
	.disassoc = mtk_p2p_cfg80211_disassoc,
	.start_ap = mtk_p2p_cfg80211_start_ap,
	.change_beacon = mtk_p2p_cfg80211_change_beacon,
	.stop_ap = mtk_p2p_cfg80211_stop_ap,
	.set_wiphy_params = mtk_p2p_cfg80211_set_wiphy_params,
	.del_station = mtk_p2p_cfg80211_del_station,
	.set_bitrate_mask = mtk_p2p_cfg80211_set_bitrate_mask,
	.mgmt_frame_register = mtk_p2p_cfg80211_mgmt_frame_register,
	.get_station = mtk_p2p_cfg80211_get_station,
	.add_key = mtk_p2p_cfg80211_add_key,
	.get_key = mtk_p2p_cfg80211_get_key,
	.del_key = mtk_p2p_cfg80211_del_key,
	.set_default_key = mtk_p2p_cfg80211_set_default_key,
	.set_default_mgmt_key = mtk_p2p_cfg80211_set_mgmt_key,
	.join_ibss = mtk_p2p_cfg80211_join_ibss,
	.leave_ibss = mtk_p2p_cfg80211_leave_ibss,
	.set_tx_power = mtk_p2p_cfg80211_set_txpower,
	.get_tx_power = mtk_p2p_cfg80211_get_txpower,
	.set_power_mgmt = mtk_p2p_cfg80211_set_power_mgmt,
#if (CFG_SUPPORT_DFS_MASTER == 1)
	.start_radar_detection = mtk_p2p_cfg80211_start_radar_detection,
#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
	.channel_switch = mtk_p2p_cfg80211_channel_switch,
#endif
#endif
#ifdef CONFIG_NL80211_TESTMODE
	.testmode_cmd = mtk_p2p_cfg80211_testmode_cmd,
#endif
#endif
};
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE

static const struct wiphy_vendor_command mtk_p2p_vendor_ops[] = {
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
};

#endif

/* There isn't a lot of sense in it, but you can transmit anything you like */
static const struct ieee80211_txrx_stypes
mtk_cfg80211_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_ADHOC] = {
				  .tx = 0xffff,
				  .rx = BIT(IEEE80211_STYPE_ACTION >> 4)
				  },
	[NL80211_IFTYPE_STATION] = {
				    .tx = 0xffff,
				    .rx = BIT(IEEE80211_STYPE_ACTION >> 4) | BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
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

#endif


static const struct iw_priv_args rP2PIwPrivTable[] = {
#if 0
	{
	 .cmd = IOC_P2P_CFG_DEVICE,
	 .set_args = IW_PRIV_TYPE_BYTE | (__u16) sizeof(IW_P2P_CFG_DEVICE_TYPE),
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_CFG_DEVICE"}
	,
	{
	 .cmd = IOC_P2P_START_STOP_DISCOVERY,
	 .set_args = IW_PRIV_TYPE_BYTE | (__u16) sizeof(IW_P2P_REQ_DEVICE_TYPE),
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_DISCOVERY"}
	,
	{
	 .cmd = IOC_P2P_DISCOVERY_RESULTS,
	 .set_args = IW_PRIV_TYPE_NONE,
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_RESULT"}
	,
	{
	 .cmd = IOC_P2P_WSC_BEACON_PROBE_RSP_IE,
	 .set_args = IW_PRIV_TYPE_BYTE | (__u16) sizeof(IW_P2P_HOSTAPD_PARAM),
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_WSC_IE"}
	,
	{
	 .cmd = IOC_P2P_CONNECT_DISCONNECT,
	 .set_args = IW_PRIV_TYPE_BYTE | (__u16) sizeof(IW_P2P_CONNECT_DEVICE),
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_CONNECT"}
	,
	{
	 .cmd = IOC_P2P_PASSWORD_READY,
	 .set_args = IW_PRIV_TYPE_BYTE | (__u16) sizeof(IW_P2P_PASSWORD_READY),
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_PASSWD_RDY"}
	,
	{
	 .cmd = IOC_P2P_GET_STRUCT,
	 .set_args = IW_PRIV_TYPE_NONE,
	 .get_args = 256,
	 .name = "P2P_GET_STRUCT"}
	,
	{
	 .cmd = IOC_P2P_SET_STRUCT,
	 .set_args = 256,
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "P2P_SET_STRUCT"}
	,
	{
	 .cmd = IOC_P2P_GET_REQ_DEVICE_INFO,
	 .set_args = IW_PRIV_TYPE_NONE,
	 .get_args = IW_PRIV_TYPE_BYTE | (__u16) sizeof(IW_P2P_DEVICE_REQ),
	 .name = "P2P_GET_REQDEV"}
	,
	{
	 /* SET STRUCT sub-ioctls commands */
	 .cmd = PRIV_CMD_OID,
	 .set_args = 256,
	 .get_args = IW_PRIV_TYPE_NONE,
	 .name = "set_oid"}
	,
	{
	 /* GET STRUCT sub-ioctls commands */
	 .cmd = PRIV_CMD_OID,
	 .set_args = IW_PRIV_TYPE_NONE,
	 .get_args = 256,
	 .name = "get_oid"}
	,
#endif
	 {IOCTL_GET_DRIVER, IW_PRIV_TYPE_CHAR | 2000, IW_PRIV_TYPE_CHAR | 2000, "driver"},
};

#if 0
const struct iw_handler_def mtk_p2p_wext_handler_def = {
	.num_standard = (__u16) sizeof(rP2PIwStandardHandler) / sizeof(iw_handler),
/* .num_private        = (__u16)sizeof(rP2PIwPrivHandler)/sizeof(iw_handler), */
	.num_private_args = (__u16) sizeof(rP2PIwPrivTable) / sizeof(struct iw_priv_args),
	.standard = rP2PIwStandardHandler,
/* .private            = rP2PIwPrivHandler, */
	.private_args = rP2PIwPrivTable,
#if CFG_SUPPORT_P2P_RSSI_QUERY
	.get_wireless_stats = mtk_p2p_wext_get_wireless_stats,
#else
	.get_wireless_stats = NULL,
#endif
};
#endif

#ifdef CONFIG_PM
static const struct wiphy_wowlan_support mtk_p2p_wowlan_support = {
	.flags = WIPHY_WOWLAN_DISCONNECT | WIPHY_WOWLAN_ANY,
};
#endif

static const struct ieee80211_iface_limit mtk_p2p_sta_go_limits[] = {
	{
		.max = 3,
		.types = BIT(NL80211_IFTYPE_STATION),
	},

	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
};

#if ((CFG_SUPPORT_DFS_MASTER == 1) && (KERNEL_VERSION(3, 17, 0) > CFG80211_VERSION_CODE))
static const struct ieee80211_iface_limit mtk_ap_limits[] = {
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_AP),
	},
};
#endif

static const struct ieee80211_iface_combination
mtk_iface_combinations_sta[] = {
	{
#ifdef CFG_NUM_DIFFERENT_CHANNELS_STA
		.num_different_channels = CFG_NUM_DIFFERENT_CHANNELS_STA,
#else
		.num_different_channels = 2,
#endif /* CFG_NUM_DIFFERENT_CHANNELS_STA */
		.max_interfaces = 3,
		/*.beacon_int_infra_match = true,*/
		.limits = mtk_p2p_sta_go_limits,
		.n_limits = 1, /* include p2p */
	},
};

static const struct ieee80211_iface_combination
mtk_iface_combinations_p2p[] = {
	{
#ifdef CFG_NUM_DIFFERENT_CHANNELS_P2P
		.num_different_channels = CFG_NUM_DIFFERENT_CHANNELS_P2P,
#else
		.num_different_channels = 2,
#endif /* CFG_NUM_DIFFERENT_CHANNELS_P2P */
		.max_interfaces = 3,
		/*.beacon_int_infra_match = true,*/
		.limits = mtk_p2p_sta_go_limits,
		.n_limits = ARRAY_SIZE(mtk_p2p_sta_go_limits), /* include p2p */
	},
#if ((CFG_SUPPORT_DFS_MASTER == 1) && (KERNEL_VERSION(3, 17, 0) > CFG80211_VERSION_CODE))
	/* ONLY for passing checks in cfg80211_can_use_iftype_chan before linux-3.17.0 */
	{
		.num_different_channels = 1,
		.max_interfaces = 1,
		.limits = mtk_ap_limits,
		.n_limits = ARRAY_SIZE(mtk_ap_limits),
		.radar_detect_widths = BIT(NL80211_CHAN_WIDTH_20_NOHT) |
				       BIT(NL80211_CHAN_WIDTH_20) |
				       BIT(NL80211_CHAN_WIDTH_40) |
				       BIT(NL80211_CHAN_WIDTH_80) |
				       BIT(NL80211_CHAN_WIDTH_80P80),
	},
#endif
};


const struct ieee80211_iface_combination *p_mtk_iface_combinations_sta = mtk_iface_combinations_sta;
const INT_32 mtk_iface_combinations_sta_num = ARRAY_SIZE(mtk_iface_combinations_sta);

const struct ieee80211_iface_combination *p_mtk_iface_combinations_p2p = mtk_iface_combinations_p2p;
const INT_32 mtk_iface_combinations_p2p_num = ARRAY_SIZE(mtk_iface_combinations_p2p);

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/* Net Device Hooks */
static int p2pOpen(IN struct net_device *prDev);

static int p2pStop(IN struct net_device *prDev);

static struct net_device_stats *p2pGetStats(IN struct net_device *prDev);

static void p2pSetMulticastList(IN struct net_device *prDev);

static int p2pHardStartXmit(IN struct sk_buff *prSkb, IN struct net_device *prDev);

static int p2pSetMACAddress(IN struct net_device *prDev, void *addr);

static int p2pDoIOCTL(struct net_device *prDev, struct ifreq *prIFReq, int i4Cmd);


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
static int p2pInit(struct net_device *prDev)
{
	if (!prDev)
		return -ENXIO;

	return 0;		/* success */
}				/* end of p2pInit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief A function for prDev->uninit
*
* \param[in] prDev      Pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static void p2pUninit(IN struct net_device *prDev)
{
}				/* end of p2pUninit() */

const struct net_device_ops p2p_netdev_ops = {
	.ndo_open = p2pOpen,
	.ndo_stop = p2pStop,
	.ndo_set_mac_address = p2pSetMACAddress,
	.ndo_set_rx_mode = p2pSetMulticastList,
	.ndo_get_stats = p2pGetStats,
	.ndo_do_ioctl = p2pDoIOCTL,
	.ndo_start_xmit = p2pHardStartXmit,
	/* .ndo_select_queue       = p2pSelectQueue, */
	.ndo_select_queue = wlanSelectQueue,
	.ndo_init = p2pInit,
	.ndo_uninit = p2pUninit,
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Allocate memory for P2P_INFO, GL_P2P_INFO, P2P_CONNECTION_SETTINGS
*                                          P2P_SPECIFIC_BSS_INFO, P2P_FSM_INFO
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2PAllocInfo(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucIdex)
{
	P_ADAPTER_T prAdapter = NULL;
	P_WIFI_VAR_T prWifiVar = NULL;
	/* UINT_32 u4Idx = 0; */

	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(P2P, ERROR, "prGlueInfo error\n");
		return FALSE;
	}

	prAdapter = prGlueInfo->prAdapter;
	prWifiVar = &(prAdapter->rWifiVar);

	ASSERT(prAdapter);
	ASSERT(prWifiVar);

	do {
		if (prGlueInfo->prP2PInfo[ucIdex] == NULL) {
			/*alloc memory for p2p info */
			prGlueInfo->prP2PInfo[ucIdex] = kalMemAlloc(sizeof(GL_P2P_INFO_T), VIR_MEM_TYPE);

			if (ucIdex == 0) {
				/*printk("[CHECK!]p2PAllocInfo : Alloc Common part only first interface\n");*/
				prGlueInfo->prP2PDevInfo = kalMemAlloc(sizeof(GL_P2P_DEV_INFO_T), VIR_MEM_TYPE);
				prAdapter->prP2pInfo = kalMemAlloc(sizeof(P2P_INFO_T), VIR_MEM_TYPE);
				prWifiVar->prP2pDevFsmInfo = kalMemAlloc(sizeof(P2P_DEV_FSM_INFO_T), VIR_MEM_TYPE);
			}
			prWifiVar->prP2PConnSettings[ucIdex] =
				kalMemAlloc(sizeof(P2P_CONNECTION_SETTINGS_T), VIR_MEM_TYPE);
			prWifiVar->prP2pSpecificBssInfo[ucIdex] =
				kalMemAlloc(sizeof(P2P_SPECIFIC_BSS_INFO_T), VIR_MEM_TYPE);

			/* TODO: It can be moved to the interface been created. */
#if 0
			    for (u4Idx = 0; u4Idx < BSS_P2P_NUM; u4Idx++) {
				prWifiVar->aprP2pRoleFsmInfo[u4Idx] =
				kalMemAlloc(sizeof(P2P_ROLE_FSM_INFO_T), VIR_MEM_TYPE);
			    }
#endif
		} else {
			ASSERT(prAdapter->prP2pInfo != NULL);
			ASSERT(prWifiVar->prP2PConnSettings[ucIdex] != NULL);
			/* ASSERT(prWifiVar->prP2pFsmInfo != NULL); */
			ASSERT(prWifiVar->prP2pSpecificBssInfo[ucIdex] != NULL);
		}
		/*MUST set memory to 0 */
		kalMemZero(prGlueInfo->prP2PInfo[ucIdex], sizeof(GL_P2P_INFO_T));
		if (ucIdex == 0) {
			if (prGlueInfo->prP2PDevInfo)
				kalMemZero(prGlueInfo->prP2PDevInfo,
				sizeof(GL_P2P_DEV_INFO_T));
			if (prAdapter->prP2pInfo)
				kalMemZero(prAdapter->prP2pInfo,
				sizeof(P2P_INFO_T));
			if (prWifiVar->prP2pDevFsmInfo)
				kalMemZero(prWifiVar->prP2pDevFsmInfo,
				sizeof(P2P_DEV_FSM_INFO_T));
		}
		kalMemZero(prWifiVar->prP2PConnSettings[ucIdex], sizeof(P2P_CONNECTION_SETTINGS_T));
/* kalMemZero(prWifiVar->prP2pFsmInfo, sizeof(P2P_FSM_INFO_T)); */
		kalMemZero(prWifiVar->prP2pSpecificBssInfo[ucIdex], sizeof(P2P_SPECIFIC_BSS_INFO_T));

	} while (FALSE);

	if (!prGlueInfo->prP2PDevInfo)
		DBGLOG(P2P, ERROR, "prP2PDevInfo error\n");
	else
		DBGLOG(P2P, INFO, "prP2PDevInfo ok\n");

	if (!prGlueInfo->prP2PInfo[ucIdex])
		DBGLOG(P2P, ERROR, "prP2PInfo error\n");
	else
		DBGLOG(P2P, INFO, "prP2PInfo ok\n");



	/* chk if alloc successful or not */
	if (prGlueInfo->prP2PInfo[ucIdex] && prGlueInfo->prP2PDevInfo && prAdapter->prP2pInfo &&
		prWifiVar->prP2PConnSettings[ucIdex] &&
/* prWifiVar->prP2pFsmInfo && */
	    prWifiVar->prP2pSpecificBssInfo[ucIdex])
		return TRUE;


	DBGLOG(P2P, ERROR, "[fail!]p2PAllocInfo :fail\n");

	if (prWifiVar->prP2pSpecificBssInfo[ucIdex]) {
		kalMemFree(prWifiVar->prP2pSpecificBssInfo[ucIdex], VIR_MEM_TYPE, sizeof(P2P_SPECIFIC_BSS_INFO_T));

		prWifiVar->prP2pSpecificBssInfo[ucIdex] = NULL;
	}
/* if (prWifiVar->prP2pFsmInfo) { */
/* kalMemFree(prWifiVar->prP2pFsmInfo, VIR_MEM_TYPE, sizeof(P2P_FSM_INFO_T)); */

/* prWifiVar->prP2pFsmInfo = NULL; */
/* } */
	if (prWifiVar->prP2PConnSettings[ucIdex]) {
		kalMemFree(prWifiVar->prP2PConnSettings[ucIdex], VIR_MEM_TYPE, sizeof(P2P_CONNECTION_SETTINGS_T));

		prWifiVar->prP2PConnSettings[ucIdex] = NULL;
	}
	if (prGlueInfo->prP2PDevInfo) {
		kalMemFree(prGlueInfo->prP2PDevInfo, VIR_MEM_TYPE, sizeof(GL_P2P_DEV_INFO_T));

		prGlueInfo->prP2PDevInfo = NULL;
	}
	if (prGlueInfo->prP2PInfo[ucIdex]) {
		kalMemFree(prGlueInfo->prP2PInfo[ucIdex], VIR_MEM_TYPE, sizeof(GL_P2P_INFO_T));

		prGlueInfo->prP2PInfo[ucIdex] = NULL;
	}
	if (prAdapter->prP2pInfo) {
		kalMemFree(prAdapter->prP2pInfo, VIR_MEM_TYPE, sizeof(P2P_INFO_T));

		prAdapter->prP2pInfo = NULL;
	}
	return FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Free memory for P2P_INFO, GL_P2P_INFO, P2P_CONNECTION_SETTINGS
*                                          P2P_SPECIFIC_BSS_INFO, P2P_FSM_INFO
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2PFreeInfo(P_GLUE_INFO_T prGlueInfo)
{

	UINT_8	i;

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);

	/* free memory after p2p module is ALREADY unregistered */
	if (prGlueInfo->prAdapter->fgIsP2PRegistered == FALSE) {

		kalMemFree(prGlueInfo->prAdapter->prP2pInfo, VIR_MEM_TYPE, sizeof(P2P_INFO_T));

		for (i = 0; i < KAL_P2P_NUM; i++)
			if (prGlueInfo->prP2PInfo[i] != NULL) {
				kalMemFree(prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings[i], VIR_MEM_TYPE,
					   sizeof(P2P_CONNECTION_SETTINGS_T));

				prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings[i] = NULL;
				kalMemFree(prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo[i], VIR_MEM_TYPE,
					   sizeof(P2P_SPECIFIC_BSS_INFO_T));

				prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo[i] = NULL;
			}

		kalMemFree(prGlueInfo->prP2PDevInfo, VIR_MEM_TYPE, sizeof(GL_P2P_DEV_INFO_T));

		for (i = 0; i < KAL_P2P_NUM; i++) {

			if (prGlueInfo->prP2PInfo[i] != NULL) {

#if (CFG_SUPPORT_DFS_MASTER == 1)
				if (prGlueInfo->prP2PInfo[i]->chandef) {
					if (prGlueInfo->prP2PInfo[i]
							->chandef->chan) {
						cnmMemFree(prGlueInfo
								->prAdapter,
							   prGlueInfo
								->prP2PInfo[i]
								->chandef
								->chan);
						prGlueInfo->prP2PInfo[i]
							->chandef->chan = NULL;
					}
					cnmMemFree(prGlueInfo->prAdapter,
						prGlueInfo->prP2PInfo[i]
						->chandef);
					prGlueInfo->prP2PInfo[i]
						->chandef = NULL;
				}
#endif

				kalMemFree(prGlueInfo->prP2PInfo[i], VIR_MEM_TYPE, sizeof(GL_P2P_INFO_T));
				prGlueInfo->prP2PInfo[i] = NULL;
			}
		}

/* kalMemFree(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo, VIR_MEM_TYPE, sizeof(P2P_FSM_INFO_T)); */

		kalMemFree(prGlueInfo->prAdapter->rWifiVar.prP2pDevFsmInfo, VIR_MEM_TYPE, sizeof(P2P_DEV_FSM_INFO_T));

		/*Reomve p2p bss scan list */
		scanRemoveAllP2pBssDesc(prGlueInfo->prAdapter);

		/*reset all pointer to NULL */
		/*prGlueInfo->prP2PInfo[0] = NULL;*/
		prGlueInfo->prAdapter->prP2pInfo = NULL;
/* prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo = NULL; */
		prGlueInfo->prAdapter->rWifiVar.prP2pDevFsmInfo = NULL;

		return TRUE;
	} else {
		return FALSE;
	}

}



BOOLEAN p2pNetRegister(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgIsRtnlLockAcquired)
{
	BOOLEAN fgDoRegister = FALSE;
	BOOLEAN fgRollbackRtnlLock = FALSE;
	BOOLEAN ret;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prGlueInfo->prAdapter->rP2PNetRegState == ENUM_NET_REG_STATE_UNREGISTERED) {
		prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_REGISTERING;
		fgDoRegister = TRUE;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	if (!fgDoRegister)
		return TRUE;

	if (fgIsRtnlLockAcquired && rtnl_is_locked()) {
		fgRollbackRtnlLock = TRUE;
		rtnl_unlock();
	}

	/* net device initialize */
	netif_carrier_off(prGlueInfo->prP2PInfo[0]->prDevHandler);
	netif_tx_stop_all_queues(prGlueInfo->prP2PInfo[0]->prDevHandler);

	/* register for net device */
	if (register_netdev(prGlueInfo->prP2PInfo[0]->prDevHandler) < 0) {
		DBGLOG(INIT, WARN, "unable to register netdevice for p2p\n");

		free_netdev(prGlueInfo->prP2PInfo[0]->prDevHandler);

		ret = FALSE;
	} else {
		prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_REGISTERED;
		gPrP2pDev[0] = prGlueInfo->prP2PInfo[0]->prDevHandler;
		ret = TRUE;
	}

	if (prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum == RUNNING_DUAL_AP_MODE) {
		/* net device initialize */
		netif_carrier_off(prGlueInfo->prP2PInfo[1]->prDevHandler);
		netif_tx_stop_all_queues(prGlueInfo->prP2PInfo[1]->prDevHandler);

		/* register for net device */
		if (register_netdev(prGlueInfo->prP2PInfo[1]->prDevHandler) < 0) {
			DBGLOG(INIT, WARN, "unable to register netdevice for p2p\n");

			free_netdev(prGlueInfo->prP2PInfo[1]->prDevHandler);

			ret = FALSE;
		} else {
			prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_REGISTERED;
			gPrP2pDev[1] = prGlueInfo->prP2PInfo[1]->prDevHandler;
			ret = TRUE;
		}


		DBGLOG(P2P, INFO, "P2P 2nd interface work\n");
	}
	if (fgRollbackRtnlLock)
		rtnl_lock();

	return ret;
}

BOOLEAN p2pNetUnregister(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgIsRtnlLockAcquired)
{
	BOOLEAN fgDoUnregister = FALSE;
	BOOLEAN fgRollbackRtnlLock = FALSE;
	UINT_8 ucRoleIdx;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prGlueInfo->prAdapter->rP2PNetRegState == ENUM_NET_REG_STATE_REGISTERED) {
		prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_UNREGISTERING;
		fgDoUnregister = TRUE;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	if (!fgDoUnregister)
		return TRUE;

	/* prepare for removal */
	if (prGlueInfo->prP2PInfo[0]->prDevHandler != prGlueInfo->prP2PInfo[0]->aprRoleHandler) {
		if (netif_carrier_ok(prGlueInfo->prP2PInfo[0]->aprRoleHandler))
			netif_carrier_off(prGlueInfo->prP2PInfo[0]->aprRoleHandler);

		netif_tx_stop_all_queues(prGlueInfo->prP2PInfo[0]->aprRoleHandler);
	}

	if (netif_carrier_ok(prGlueInfo->prP2PInfo[0]->prDevHandler))
		netif_carrier_off(prGlueInfo->prP2PInfo[0]->prDevHandler);

	netif_tx_stop_all_queues(prGlueInfo->prP2PInfo[0]->prDevHandler);

	if (fgIsRtnlLockAcquired && rtnl_is_locked()) {
		fgRollbackRtnlLock = TRUE;
		rtnl_unlock();
	}
	/* Here are functions which need rtnl_lock */
	if (prGlueInfo->prP2PInfo[0]->prDevHandler != prGlueInfo->prP2PInfo[0]->aprRoleHandler) {
		DBGLOG(INIT, INFO, "unregister p2p[0]\n");
		unregister_netdev(prGlueInfo->prP2PInfo[0]->aprRoleHandler);
	}
	DBGLOG(INIT, INFO, "unregister p2pdev\n");
	unregister_netdev(prGlueInfo->prP2PInfo[0]->prDevHandler);

	/* unregister the netdev and index > 0 */
	if (prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum >= 2) {
		for (ucRoleIdx = 1; ucRoleIdx < BSS_P2P_NUM; ucRoleIdx++) {
			/* prepare for removal */
			if (netif_carrier_ok(prGlueInfo->prP2PInfo[ucRoleIdx]->prDevHandler))
				netif_carrier_off(prGlueInfo->prP2PInfo[ucRoleIdx]->prDevHandler);

			netif_tx_stop_all_queues(prGlueInfo->prP2PInfo[ucRoleIdx]->prDevHandler);

			if (fgIsRtnlLockAcquired && rtnl_is_locked()) {
				fgRollbackRtnlLock = TRUE;
				rtnl_unlock();
			}
			/* Here are functions which need rtnl_lock */

			unregister_netdev(prGlueInfo->prP2PInfo[ucRoleIdx]->prDevHandler);
		}
	}

	if (fgRollbackRtnlLock)
		rtnl_lock();

	prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_UNREGISTERED;

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Register for cfg80211 for Wi-Fi Direct
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN glRegisterP2P(P_GLUE_INFO_T prGlueInfo, const char *prDevName, const char *prDevName2, UINT_8 ucApMode)
{
	P_ADAPTER_T prAdapter = NULL;
	P_GL_HIF_INFO_T prHif = NULL;
	PARAM_MAC_ADDRESS rMacAddr;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPriv = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	BOOLEAN fgIsApMode = FALSE;
	UINT_8  ucRegisterNum = 1, i = 0;
	struct wireless_dev *prP2pWdev;
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	struct device *prDev;
#endif
	const char *prSetDevName;

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prHif = &prGlueInfo->rHifInfo;
	ASSERT(prHif);

	if ((ucApMode == RUNNING_AP_MODE) || (ucApMode == RUNNING_DUAL_AP_MODE || (ucApMode == RUNNING_P2P_AP_MODE))) {
		fgIsApMode = TRUE;
		if ((ucApMode == RUNNING_DUAL_AP_MODE) || (ucApMode == RUNNING_P2P_AP_MODE)) {
			ucRegisterNum = 2;
			glP2pCreateWirelessDevice(prGlueInfo);
		}
	}
	prSetDevName = prDevName;

	do {
		if ((ucApMode == RUNNING_P2P_AP_MODE) && (i == 0)) {
			prSetDevName = prDevName;
			fgIsApMode = FALSE;
		} else if ((ucApMode == RUNNING_P2P_AP_MODE) && (i == 1)) {
			prSetDevName = prDevName2;
			fgIsApMode = TRUE;
		}
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
		if (!gprP2pRoleWdev[i]) {
			DBGLOG(P2P, ERROR, "gl_p2p, wireless device is not exist\n");
			return FALSE;
		}
#endif
		prP2pWdev = gprP2pRoleWdev[i];
		DBGLOG(INIT, INFO, "glRegisterP2P(%d)\n", i);
		/*0. allocate p2pinfo */
		if (!p2PAllocInfo(prGlueInfo, i)) {
			DBGLOG(INIT, WARN, "Allocate memory for p2p FAILED\n");
			ASSERT(0);
			return FALSE;
		}
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211

		/* 1.1 fill wiphy parameters */
		glGetHifDev(prHif, &prDev);
		if (!prDev)
			DBGLOG(INIT, INFO, "unable to get struct dev for p2p\n");

		prGlueInfo->prP2PInfo[i]->prWdev = prP2pWdev;
		/*prGlueInfo->prP2PInfo[i]->prRoleWdev[0] = prP2pWdev;*//* TH3 multiple P2P */

		ASSERT(prP2pWdev);
		ASSERT(prP2pWdev->wiphy);
		ASSERT(prDev);
		ASSERT(prGlueInfo->prAdapter);

		set_wiphy_dev(prP2pWdev->wiphy, prDev);

		if (!prGlueInfo->prAdapter->fgEnable5GBand)
			prP2pWdev->wiphy->bands[BAND_5G] = NULL;




		/* 2.2 wdev initialization */
		if (fgIsApMode)
			prP2pWdev->iftype = NL80211_IFTYPE_AP;
		else
			prP2pWdev->iftype = NL80211_IFTYPE_P2P_CLIENT;

#endif /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */


		/* 3. allocate netdev */
#if KERNEL_VERSION(3, 17, 0) <= CFG80211_VERSION_CODE
		prGlueInfo->prP2PInfo[i]->prDevHandler =
			alloc_netdev_mq(sizeof(NETDEV_PRIVATE_GLUE_INFO), prSetDevName,
					NET_NAME_PREDICTABLE, ether_setup, CFG_MAX_TXQ_NUM);
#else
		prGlueInfo->prP2PInfo[i]->prDevHandler =
		    alloc_netdev_mq(sizeof(NETDEV_PRIVATE_GLUE_INFO), prSetDevName,
				ether_setup, CFG_MAX_TXQ_NUM);
#endif
		if (!prGlueInfo->prP2PInfo[i]->prDevHandler) {
			DBGLOG(INIT, WARN, "unable to allocate netdevice for p2p\n");

			goto err_alloc_netdev;
		}

		/* 4. setup netdev */
		/* 4.1 Point to shared glue structure */
	/* *((P_GLUE_INFO_T *) netdev_priv(prGlueInfo->prP2PInfo->prDevHandler)) = prGlueInfo; */
		prNetDevPriv = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(prGlueInfo->prP2PInfo[i]->prDevHandler);
		prNetDevPriv->prGlueInfo = prGlueInfo;

		/* 4.2 fill hardware address */
		COPY_MAC_ADDR(rMacAddr, prAdapter->rMyMacAddr);
		rMacAddr[0] |= 0x2;
		rMacAddr[0] ^= i << 2;	/* change to local administrated address */
		kalMemCopy(prGlueInfo->prP2PInfo[i]->prDevHandler->dev_addr, rMacAddr, ETH_ALEN);
		kalMemCopy(prGlueInfo->prP2PInfo[i]->prDevHandler->perm_addr,
			prGlueInfo->prP2PInfo[i]->prDevHandler->dev_addr,
			   ETH_ALEN);

		/* 4.3 register callback functions */
		prGlueInfo->prP2PInfo[i]->prDevHandler->needed_headroom += NIC_TX_HEAD_ROOM;
		prGlueInfo->prP2PInfo[i]->prDevHandler->netdev_ops = &p2p_netdev_ops;
	/* prGlueInfo->prP2PInfo->prDevHandler->wireless_handlers    = &mtk_p2p_wext_handler_def; */

#if defined(_HIF_SDIO)
#if (MTK_WCN_HIF_SDIO == 0)
		SET_NETDEV_DEV(prGlueInfo->prP2PInfo[i]->prDevHandler, &(prHif->func->dev));
#endif
#endif

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
		prGlueInfo->prP2PInfo[i]->prDevHandler->ieee80211_ptr = prP2pWdev;
		prP2pWdev->netdev = prGlueInfo->prP2PInfo[i]->prDevHandler;
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		/* set HW checksum offload */
		if (prAdapter->fgIsSupportCsumOffload)
			prGlueInfo->prP2PInfo[i]->prDevHandler->features = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
									   NETIF_F_RXCSUM;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

		kalResetStats(prGlueInfo->prP2PInfo[i]->prDevHandler);


#if 0
		/* 7. net device initialize */
		netif_carrier_off(prGlueInfo->prP2PInfo->prDevHandler);
		netif_tx_stop_all_queues(prGlueInfo->prP2PInfo->prDevHandler);

		/* 8. register for net device */
		if (register_netdev(prGlueInfo->prP2PInfo->prDevHandler) < 0) {
			DBGLOG(INIT, WARN, "unable to register netdevice for p2p\n");

			goto err_reg_netdev;
		}
#endif

		/* 8. set p2p net device register state */
		prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_UNREGISTERED;

		/* 9. setup running mode */
		/* prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->fgIsApMode = fgIsApMode; */

		/* 10. finish */
		/* 13. bind netdev pointer to netdev index */
		wlanBindBssIdxToNetInterface(prGlueInfo, p2pDevFsmInit(prAdapter),
			(PVOID) prGlueInfo->prP2PInfo[i]->prDevHandler);

		prGlueInfo->prP2PInfo[i]->aprRoleHandler = prGlueInfo->prP2PInfo[i]->prDevHandler;


		DBGLOG(P2P, INFO, "check prDevHandler = %p\n", prGlueInfo->prP2PInfo[i]->prDevHandler);
		DBGLOG(P2P, INFO, "aprRoleHandler = %p\n", prGlueInfo->prP2PInfo[i]->aprRoleHandler);

		prNetDevPriv->ucBssIdx = p2pRoleFsmInit(prAdapter, i);
		/* 11. Currently wpasupplicant can't support create interface. */
		/* so initial the corresponding data structure here. */
		wlanBindBssIdxToNetInterface(prGlueInfo, prNetDevPriv->ucBssIdx,
					     (PVOID) prGlueInfo->prP2PInfo[i]->aprRoleHandler);

		/* 13. bind netdev pointer to netdev index */
		/* wlanBindNetInterface(prGlueInfo, NET_DEV_P2P_IDX, (PVOID)prGlueInfo->prP2PInfo->prDevHandler); */

		/* 12. setup running mode */
		p2pFuncInitConnectionSettings(prAdapter, prAdapter->rWifiVar.prP2PConnSettings[i], fgIsApMode);

		/* Active network too early would cause HW not able to sleep.
		 * Defer the network active time.
		 */
	/* nicActivateNetwork(prAdapter, NETWORK_TYPE_P2P_INDEX); */

		i++;
	} while (i < ucRegisterNum);

	if ((ucApMode == RUNNING_DUAL_AP_MODE) || (ucApMode == RUNNING_P2P_AP_MODE))
		prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum = 2;
	else
		prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum = 1;


	return TRUE;
#if 0
err_reg_netdev:
	free_netdev(prGlueInfo->prP2PInfo->prDevHandler);
#endif
err_alloc_netdev:
	return FALSE;
}				/* end of glRegisterP2P() */


BOOLEAN glP2pCreateWirelessDevice(P_GLUE_INFO_T prGlueInfo)
{
	struct wiphy *prWiphy = NULL;
	struct wireless_dev *prWdev = NULL;
	UINT_8	idx = 0;
	u32 band_idx, ch_idx;
	struct ieee80211_supported_band *sband = NULL;
	struct ieee80211_channel *chan = NULL;

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	for (idx = 0; idx < KAL_P2P_NUM; idx++) {
		if (!gprP2pRoleWdev[idx])
			break;
	}
	if (idx == KAL_P2P_NUM) {
		DBGLOG(INIT, ERROR, "Fail to create wdev\n");
		return FALSE;
	}

	prWdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!prWdev) {
		DBGLOG(P2P, ERROR, "allocate p2p wireless device fail, no memory\n");
		return FALSE;
	}
	/* 1. allocate WIPHY */
	prWiphy = wiphy_new(&mtk_p2p_ops, sizeof(P_GLUE_INFO_T));
	if (!prWiphy) {
		DBGLOG(P2P, ERROR, "unable to allocate wiphy for p2p\n");
		goto free_wdev;
	}

	prWiphy->interface_modes = BIT(NL80211_IFTYPE_AP)
	    | BIT(NL80211_IFTYPE_P2P_CLIENT)
	    | BIT(NL80211_IFTYPE_P2P_GO)
	    | BIT(NL80211_IFTYPE_STATION);

	prWiphy->software_iftypes |= BIT(NL80211_IFTYPE_P2P_DEVICE);

	prWiphy->iface_combinations = p_mtk_iface_combinations_p2p;
	prWiphy->n_iface_combinations = mtk_iface_combinations_p2p_num;

	prWiphy->bands[KAL_BAND_2GHZ] = &mtk_band_2ghz;
	prWiphy->bands[KAL_BAND_5GHZ] = &mtk_band_5ghz;

	/*
	 * Clear flags in ieee80211_channel before p2p registers to resolve
	 * overriding flags issue. For example, when country is changed to US,
	 * both WW and US flags are applied. The issue flow is:
	 *
	 * 1. Register wlan wiphy (wiphy_register()@core.c)
	 *    chan->orig_flags = chan->flags = 0
	 * 2. Apply WW regdomain (handle_channel()@reg.c):
	 *    chan->flags = chan->orig_flags|reg_channel_flags = 0|WW_channel_flags
	 * 3. Register p2p wiphy:
	 *    chan->orig_flags = chan->flags = WW channel flags
	 * 4. Apply US regdomain:
	 *    chan->flags = chan->orig_flags|reg_channel_flags
	 *                = WW_channel_flags|US_channel_flags
	 *                  (Unexpected! It includes WW flags)
	 */

	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		sband = prWiphy->bands[band_idx];
		if (!sband)
			continue;
		for (ch_idx = 0; ch_idx < sband->n_channels; ch_idx++) {
			chan = &sband->channels[ch_idx];
			chan->flags = 0;
		}
	}

	prWiphy->mgmt_stypes = mtk_cfg80211_default_mgmt_stypes;
	prWiphy->max_remain_on_channel_duration = 5000;
	prWiphy->n_cipher_suites = 5;
	prWiphy->cipher_suites = mtk_cipher_suites;
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	prWiphy->flags = WIPHY_FLAG_CUSTOM_REGULATORY | WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL | WIPHY_FLAG_HAVE_AP_SME;
#else
#if (CFG_SUPPORT_DFS_MASTER == 1)
	prWiphy->flags = WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL | WIPHY_FLAG_HAVE_AP_SME | WIPHY_FLAG_HAS_CHANNEL_SWITCH;
	prWiphy->max_num_csa_counters = 2;
#else
	prWiphy->flags = WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL | WIPHY_FLAG_HAVE_AP_SME;
#endif
	prWiphy->regulatory_flags = REGULATORY_CUSTOM_REG;
#endif
	prWiphy->ap_sme_capa = 1;

	prWiphy->max_scan_ssids = MAX_SCAN_LIST_NUM;
	prWiphy->max_scan_ie_len = MAX_SCAN_IE_LEN;
	prWiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
	prWiphy->vendor_commands = mtk_p2p_vendor_ops;
	prWiphy->n_vendor_commands = sizeof(mtk_p2p_vendor_ops) / sizeof(struct wiphy_vendor_command);
#endif

#ifdef CONFIG_PM
#if KERNEL_VERSION(3, 9, 0) > CFG80211_VERSION_CODE
	prWiphy->wowlan = &mtk_p2p_wowlan_support;
#endif
#endif

	cfg80211_regd_set_wiphy(prWiphy);

	/* 2.1 set priv as pointer to glue structure */
	*((P_GLUE_INFO_T *) wiphy_priv(prWiphy)) = prGlueInfo;
	/* Here are functions which need rtnl_lock */
	if (wiphy_register(prWiphy) < 0) {
		DBGLOG(INIT, WARN, "fail to register wiphy for p2p\n");
		goto free_wiphy;
	}
	prWdev->wiphy = prWiphy;
	DBGLOG(INIT, INFO, "Registered gprP2pRoleWdev[%d]=0x%p\n", idx, prWdev);
	gprP2pRoleWdev[idx] = prWdev;

	return TRUE;

free_wiphy:
	wiphy_free(prWiphy);
free_wdev:
	kfree(prWdev);
#endif
	return FALSE;
}

void glP2pDestroyWirelessDevice(void)
{
#if 0
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	set_wiphy_dev(gprP2pWdev->wiphy, NULL);

	wiphy_unregister(gprP2pWdev->wiphy);
	wiphy_free(gprP2pWdev->wiphy);
	kfree(gprP2pWdev);
	gprP2pWdev = NULL;
#endif
#else
	int i = 0;

	/* Move set_wiphy_dev(wiphy, NULL) in wlanNetDestroy */

	wiphy_unregister(gprP2pWdev->wiphy);
	wiphy_free(gprP2pWdev->wiphy);
	kfree(gprP2pWdev);

	for (i = 0; i < KAL_P2P_NUM; i++) {

		if (gprP2pRoleWdev[i] == NULL)
			continue;

		if (i != 0) { /* The P2P is always in index 0 and shares Wiphy with P2PWdev */
			DBGLOG(INIT, INFO, "glP2pDestroyWirelessDevice (%p)\n", gprP2pRoleWdev[i]->wiphy);
			set_wiphy_dev(gprP2pRoleWdev[i]->wiphy, NULL);
			wiphy_unregister(gprP2pRoleWdev[i]->wiphy);
			wiphy_free(gprP2pRoleWdev[i]->wiphy);
		}
		if (gprP2pRoleWdev[i] && (gprP2pWdev != gprP2pRoleWdev[i]))
			kfree(gprP2pRoleWdev[i]);
		gprP2pRoleWdev[i] = NULL;
	}

	gprP2pWdev = NULL;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Unregister Net Device for Wi-Fi Direct
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN glUnregisterP2P(P_GLUE_INFO_T prGlueInfo)
{
	UINT_8 ucRoleIdx;
	P_ADAPTER_T prAdapter;
	P_GL_P2P_INFO_T prP2PInfo;

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;

	/* 4 <1> Uninit P2P dev FSM */
	/* Uninit P2P device FSM */
	p2pDevFsmUninit(prAdapter);

	/* 4 <2> Uninit P2P role FSM */
	for (ucRoleIdx = 0; ucRoleIdx < BSS_P2P_NUM; ucRoleIdx++) {
		if (P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx))
			p2pRoleFsmUninit(prGlueInfo->prAdapter, ucRoleIdx);
	}

	/* 4 <3> Free Wiphy & netdev */
	for (ucRoleIdx = 0; ucRoleIdx < BSS_P2P_NUM; ucRoleIdx++) {
		prP2PInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		if (prP2PInfo == NULL)
			continue;
		/* For P2P interfaces, prDevHandler points to the net_device of p2p0 interface.            */
		/* And aprRoleHandler points to the net_device of p2p virtual interface (i.e., p2p1)       */
		/* when it was created. And when p2p virtual interface is deleted, aprRoleHandler will     */
		/* change to point to prDevHandler. Hence, when aprRoleHandler & prDevHandler are pointing */
		/* to different addresses, it means vif p2p1 exists. Otherwise it means p2p1 was           */
		/* already deleted. */
		if ((prP2PInfo->aprRoleHandler != NULL) &&
			(prP2PInfo->aprRoleHandler != prP2PInfo->prDevHandler)) {
			/* This device is added by the P2P, and use ndev->destructor to free. */
			prP2PInfo->aprRoleHandler = NULL;
			DBGLOG(P2P, INFO, "aprRoleHandler idx %d set NULL\n", ucRoleIdx);
		}

		if (prP2PInfo->prDevHandler) {
			free_netdev(prP2PInfo->prDevHandler);
			prP2PInfo->prDevHandler = NULL;
		}
	}

	/* 4 <4> Free P2P internal memory */
	if (!p2PFreeInfo(prGlueInfo)) {
		DBGLOG(INIT, WARN, "Free memory for p2p FAILED\n");
		ASSERT(0);
		return FALSE;
	}

	return TRUE;

#if 0
	p2pFsmUninit(prGlueInfo->prAdapter);

	nicDeactivateNetwork(prGlueInfo->prAdapter, NETWORK_TYPE_P2P_INDEX);

#if 0
	/* Release command, mgmt and security frame belong to P2P network in
	 *   prGlueInfo->prCmdQue
	 *   prAdapter->rPendingCmdQueue
	 *   prAdapter->rTxCtrl.rTxMgmtTxingQueue
	 *  To ensure there is no pending CmdDone/TxDone handler to be executed after p2p module is removed.
	 */

	/* Clear CmdQue */
	kalClearMgmtFramesByBssIdx(prGlueInfo, NETWORK_TYPE_P2P_INDEX);
	kalClearSecurityFramesByBssIdx(prGlueInfo, NETWORK_TYPE_P2P_INDEX);
	/* Clear PendingCmdQue */
	wlanReleasePendingCMDbyBssIdx(prGlueInfo->prAdapter, NETWORK_TYPE_P2P_INDEX);
	/* Clear PendingTxMsdu */
	nicFreePendingTxMsduInfoByBssIdx(prGlueInfo->prAdapter, NETWORK_TYPE_P2P_INDEX);
#endif

#if 0
	/* prepare for removal */
	if (netif_carrier_ok(prGlueInfo->prP2PInfo[0]->prDevHandler))
		netif_carrier_off(prGlueInfo->prP2PInfo[0]->prDevHandler);

	netif_tx_stop_all_queues(prGlueInfo->prP2PInfo[0]->prDevHandler);

	/* netdevice unregistration & free */
	unregister_netdev(prGlueInfo->prP2PInfo[0]->prDevHandler);
#endif
	free_netdev(prGlueInfo->prP2PInfo[0]->prDevHandler);
	prGlueInfo->prP2PInfo[0]->prDevHandler = NULL;

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#if 0
	wiphy_unregister(prGlueInfo->prP2PInfo[0]->wdev.wiphy);
#endif
	wiphy_free(prGlueInfo->prP2PInfo[0]->wdev.wiphy);
	prGlueInfo->prP2PInfo[0]->wdev.wiphy = NULL;
#endif

	/* Free p2p memory */
#if 1
	if (!p2PFreeInfo(prGlueInfo)) {
		DBGLOG(INIT, WARN, "Free memory for p2p FAILED\n");
		ASSERT(0);
		return FALSE;
	}
#endif
	return TRUE;

#else
	return 0;
#endif
}				/* end of glUnregisterP2P() */

/* Net Device Hooks */
/*----------------------------------------------------------------------------*/
/*!
 * \brief A function for net_device open (ifup)
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \retval 0     The execution succeeds.
 * \retval < 0   The execution failed.
 */
/*----------------------------------------------------------------------------*/
static int p2pOpen(IN struct net_device *prDev)
{
/* P_GLUE_INFO_T prGlueInfo = NULL; */
/* P_ADAPTER_T prAdapter = NULL; */
/* P_MSG_P2P_FUNCTION_SWITCH_T prFuncSwitch; */

	ASSERT(prDev);

#if 0				/* Move after device name set. (mtk_p2p_set_local_dev_info) */
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	/* 1. switch P2P-FSM on */
	/* 1.1 allocate for message */
	prFuncSwitch = (P_MSG_P2P_FUNCTION_SWITCH_T) cnmMemAlloc(prAdapter,
								 RAM_TYPE_MSG, sizeof(MSG_P2P_FUNCTION_SWITCH_T));

	if (!prFuncSwitch) {
		ASSERT(0);	/* Can't trigger P2P FSM */
		return -ENOMEM;
	}

	/* 1.2 fill message */
	prFuncSwitch->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
	prFuncSwitch->fgIsFuncOn = TRUE;

	/* 1.3 send message */
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prFuncSwitch, MSG_SEND_METHOD_BUF);
#endif

	/* 2. carrier on & start TX queue */
	/*DFS todo 20161220_DFS*/
#if (CFG_SUPPORT_DFS_MASTER == 1)
	if (prDev->ieee80211_ptr->iftype != NL80211_IFTYPE_AP) {
		netif_carrier_on(prDev);
		netif_tx_start_all_queues(prDev);
	}
#else
	netif_carrier_on(prDev);
	netif_tx_start_all_queues(prDev);
#endif

	return 0;		/* success */
}				/* end of p2pOpen() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief A function for net_device stop (ifdown)
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \retval 0     The execution succeeds.
 * \retval < 0   The execution failed.
 */
/*----------------------------------------------------------------------------*/
static int p2pStop(IN struct net_device *prDev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_GL_P2P_DEV_INFO_T prP2pGlueDevInfo = (P_GL_P2P_DEV_INFO_T) NULL;
	struct cfg80211_scan_request *prScanRequest = NULL;
/* P_MSG_P2P_FUNCTION_SWITCH_T prFuncSwitch; */

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prP2pGlueDevInfo = prGlueInfo->prP2PDevInfo;
	ASSERT(prP2pGlueDevInfo);

	/* 0. Do the scan done and set parameter to abort if the scan pending */
	/* Default : P2P dev */

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if ((prP2pGlueDevInfo->prScanRequest != NULL) &&
		(prP2pGlueDevInfo->prScanRequest->wdev
			== prDev->ieee80211_ptr)) {
		prScanRequest = prP2pGlueDevInfo->prScanRequest;
		kalCfg80211ScanDone(prScanRequest, TRUE);
		prP2pGlueDevInfo->prScanRequest = NULL;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	/* 1. stop TX queue */
	netif_tx_stop_all_queues(prDev);
#if 0
	/* 2. switch P2P-FSM off */
	/* 2.1 allocate for message */
	prFuncSwitch = (P_MSG_P2P_FUNCTION_SWITCH_T) cnmMemAlloc(prAdapter,
								 RAM_TYPE_MSG, sizeof(MSG_P2P_FUNCTION_SWITCH_T));

	if (!prFuncSwitch) {
		ASSERT(0);	/* Can't trigger P2P FSM */
		return -ENOMEM;
	}

	/* 2.2 fill message */
	prFuncSwitch->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
	prFuncSwitch->fgIsFuncOn = FALSE;

	/* 2.3 send message */
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prFuncSwitch, MSG_SEND_METHOD_BUF);
#endif
	/* 3. stop queue and turn off carrier */
	/*prGlueInfo->prP2PInfo[0]->eState = PARAM_MEDIA_STATE_DISCONNECTED;*//* TH3 multiple P2P */

	netif_tx_stop_all_queues(prDev);
	if (netif_carrier_ok(prDev))
		netif_carrier_off(prDev);

	return 0;
}				/* end of p2pStop() */

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
struct net_device_stats *p2pGetStats(IN struct net_device *prDev)
{
	return (struct net_device_stats *)kalGetStats(prDev);
}				/* end of p2pGetStats() */

static void p2pSetMulticastList(IN struct net_device *prDev)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	prGlueInfo = (prDev != NULL) ? *((P_GLUE_INFO_T *) netdev_priv(prDev)) : NULL;

	ASSERT(prDev);
	ASSERT(prGlueInfo);
	if (!prDev || !prGlueInfo) {
		DBGLOG(INIT, WARN, " abnormal dev or skb: prDev(0x%p), prGlueInfo(0x%p)\n", prDev, prGlueInfo);
		return;
	}

#if CFG_CHIP_RESET_SUPPORT
	if (g_u4HaltFlag || checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip p2pSetMulticastList\n");
		return;
	}
#endif

	g_P2pPrDev = prDev;

	/* 4  Mark HALT, notify main thread to finish current job */
	set_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT, &prGlueInfo->ulFlag);
	/* wake up main thread */
	wake_up_interruptible(&prGlueInfo->waitq);

}				/* p2pSetMulticastList */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is to set multicast list and set rx mode.
 *
 * \param[in] prDev  Pointer to struct net_device
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void mtk_p2p_wext_set_Multicastlist(P_GLUE_INFO_T prGlueInfo)
{
	UINT_32 u4SetInfoLen = 0;
	UINT_32 u4McCount;
	struct net_device *prDev = g_P2pPrDev;

	prGlueInfo = (prDev != NULL) ? *((P_GLUE_INFO_T *) netdev_priv(prDev)) : NULL;

	ASSERT(prDev);
	ASSERT(prGlueInfo);
	if (!prDev || !prGlueInfo) {
		DBGLOG(INIT, WARN, " abnormal dev or skb: prDev(0x%p), prGlueInfo(0x%p)\n", prDev, prGlueInfo);
		return;
	}

	if (prDev->flags & IFF_PROMISC)
		prGlueInfo->prP2PDevInfo->u4PacketFilter |= PARAM_PACKET_FILTER_PROMISCUOUS;

	if (prDev->flags & IFF_BROADCAST)
		prGlueInfo->prP2PDevInfo->u4PacketFilter |= PARAM_PACKET_FILTER_BROADCAST;
	u4McCount = netdev_mc_count(prDev);

	if (prDev->flags & IFF_MULTICAST) {
		if ((prDev->flags & IFF_ALLMULTI) || (u4McCount > MAX_NUM_GROUP_ADDR))
			prGlueInfo->prP2PDevInfo->u4PacketFilter |= PARAM_PACKET_FILTER_ALL_MULTICAST;
		else
			prGlueInfo->prP2PDevInfo->u4PacketFilter |= PARAM_PACKET_FILTER_MULTICAST;
	}

	if (prGlueInfo->prP2PDevInfo->u4PacketFilter & PARAM_PACKET_FILTER_MULTICAST) {
		/* Prepare multicast address list */
		struct netdev_hw_addr *ha;
		UINT_32 i = 0;

		netdev_for_each_mc_addr(ha, prDev) {
			/* Check mc count before accessing to ha to prevent from
			 * kernel crash. One example crash log reported:
			 *   Unable to handle kernel NULL pointer dereference at
			 *   virtual address 00000008
			 *   PC is at mtk_p2p_wext_set_Multicastlist+0x150/0x22c
			 */
			if (i == u4McCount || !ha)
				break;
			if (i < MAX_NUM_GROUP_ADDR) {
				COPY_MAC_ADDR(&(prGlueInfo->prP2PDevInfo->aucMCAddrList[i]), GET_ADDR(ha));
				i++;
			}
		}

		DBGLOG(P2P, TRACE, "SEt Multicast Address List\n");

		if (i >= MAX_NUM_GROUP_ADDR)
			return;
		wlanoidSetP2PMulticastList(prGlueInfo->prAdapter,
					   &(prGlueInfo->prP2PDevInfo->aucMCAddrList[0]),
					   (i * ETH_ALEN), &u4SetInfoLen);

	}

}				/* end of p2pSetMulticastList() */

/*----------------------------------------------------------------------------*/
/*!
 * * \brief This function is TX entry point of NET DEVICE.
 * *
 * * \param[in] prSkb  Pointer of the sk_buff to be sent
 * * \param[in] prDev  Pointer to struct net_device
 * *
 * * \retval NETDEV_TX_OK - on success.
 * * \retval NETDEV_TX_BUSY - on failure, packet will be discarded by upper layer.
 */
/*----------------------------------------------------------------------------*/
int p2pHardStartXmit(IN struct sk_buff *prSkb, IN struct net_device *prDev)
{
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_8 ucBssIndex;

	ASSERT(prSkb);
	ASSERT(prDev);

#if CFG_CHIP_RESET_SUPPORT
	if (g_u4HaltFlag || checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip p2pHardStartXmit\n");
		return NETDEV_TX_BUSY;
	}
#endif

	prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(prDev);
	prGlueInfo = prNetDevPrivate->prGlueInfo;
	ucBssIndex = prNetDevPrivate->ucBssIdx;

	kalResetPacket(prGlueInfo, (P_NATIVE_PACKET) prSkb);

	kalHardStartXmit(prSkb, prDev, prGlueInfo, ucBssIndex);

	return NETDEV_TX_OK;
}				/* end of p2pHardStartXmit() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief A method of struct net_device, a primary SOCKET interface to configure
 *        the interface lively. Handle an ioctl call on one of our devices.
 *        Everything Linux ioctl specific is done here. Then we pass the contents
 *        of the ifr->data to the request message handler.
 *
 * \param[in] prDev      Linux kernel netdevice
 *
 * \param[in] prIFReq    Our private ioctl request structure, typed for the generic
 *                       struct ifreq so we can use ptr to function
 *
 * \param[in] cmd        Command ID
 *
 * \retval WLAN_STATUS_SUCCESS The IOCTL command is executed successfully.
 * \retval OTHER The execution of IOCTL command is failed.
 */
/*----------------------------------------------------------------------------*/

int p2pDoIOCTL(struct net_device *prDev, struct ifreq *prIfReq, int i4Cmd)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	int ret = 0;
	/* char *prExtraBuf = NULL; */
	/* UINT_32 u4ExtraSize = 0; */
	struct iwreq *prIwReq = (struct iwreq *)prIfReq;
	struct iw_request_info rIwReqInfo;
	/* fill rIwReqInfo */
	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	ASSERT(prDev && prIfReq);

#if CFG_CHIP_RESET_SUPPORT
	if (g_u4HaltFlag || checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip p2pDoIOCTL\n");
		return -EFAULT;
	}
#endif

	DBGLOG(P2P, ERROR, "p2pDoIOCTL In %x %x\n", i4Cmd, SIOCDEVPRIVATE);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	if (!prGlueInfo) {
		DBGLOG(P2P, ERROR, "prGlueInfo is NULL\n");
		return -EFAULT;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(P2P, ERROR, "Adapter is not ready\n");
		return -EINVAL;
	}

	if (i4Cmd == IOCTL_GET_DRIVER)
		ret = priv_support_driver_cmd(prDev, prIfReq, i4Cmd);
	else if (i4Cmd == SIOCGIWPRIV)
		ret = mtk_p2p_wext_get_priv(prDev, &rIwReqInfo, &(prIwReq->u), NULL);
#ifdef CFG_ANDROID_AOSP_PRIV_CMD
	else if (i4Cmd == SIOCDEVPRIVATE + 1)
		ret = android_private_support_driver_cmd(prDev, prIfReq, i4Cmd);
#elif defined CFG_ALPS_ANDROID_AOSP_PRIV_CMD
	else if (i4Cmd == SIOCDEVPRIVATE + 1)
		ret = alps_android_private_support_driver_cmd(prDev,
			prIfReq, i4Cmd);
#endif
	else {
		DBGLOG(INIT, WARN, "Unexpected ioctl command: 0x%04x\n", i4Cmd);
		ret = -1;
	}

#if 0
	/* fill rIwReqInfo */
	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	switch (i4Cmd) {
	case SIOCSIWENCODEEXT:
		/* Set Encryption Material after 4-way handshaking is done */
		if (prIwReq->u.encoding.pointer) {
			u4ExtraSize = prIwReq->u.encoding.length;
			prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);

			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			if (copy_from_user(prExtraBuf, prIwReq->u.encoding.pointer, prIwReq->u.encoding.length))
				ret = -EFAULT;
		} else if (prIwReq->u.encoding.length != 0) {
			ret = -EINVAL;
			break;
		}

		if (ret == 0)
			ret = mtk_p2p_wext_set_key(prDev, &rIwReqInfo, &(prIwReq->u), prExtraBuf);

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
		prExtraBuf = NULL;
		break;

	case SIOCSIWMLME:
		/* IW_MLME_DISASSOC used for disconnection */
		if (prIwReq->u.data.length != sizeof(struct iw_mlme)) {
			DBGLOG(INIT, INFO, "MLME buffer strange:%d\n", prIwReq->u.data.length);
			ret = -EINVAL;
			break;
		}

		if (!prIwReq->u.data.pointer) {
			ret = -EINVAL;
			break;
		}

		prExtraBuf = kalMemAlloc(sizeof(struct iw_mlme), VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(prExtraBuf, prIwReq->u.data.pointer, sizeof(struct iw_mlme)))
			ret = -EFAULT;
		else
			ret = mtk_p2p_wext_mlme_handler(prDev, &rIwReqInfo, &(prIwReq->u), prExtraBuf);

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, sizeof(struct iw_mlme));
		prExtraBuf = NULL;
		break;

	case SIOCGIWPRIV:
		/* This ioctl is used to list all IW privilege ioctls */
		ret = mtk_p2p_wext_get_priv(prDev, &rIwReqInfo, &(prIwReq->u), NULL);
		break;

	case SIOCGIWSCAN:
		ret = mtk_p2p_wext_discovery_results(prDev, &rIwReqInfo, &(prIwReq->u), NULL);
		break;

	case SIOCSIWAUTH:
		ret = mtk_p2p_wext_set_auth(prDev, &rIwReqInfo, &(prIwReq->u), NULL);
		break;

	case IOC_P2P_CFG_DEVICE:
	case IOC_P2P_PROVISION_COMPLETE:
	case IOC_P2P_START_STOP_DISCOVERY:
	case IOC_P2P_DISCOVERY_RESULTS:
	case IOC_P2P_WSC_BEACON_PROBE_RSP_IE:
	case IOC_P2P_CONNECT_DISCONNECT:
	case IOC_P2P_PASSWORD_READY:
	case IOC_P2P_GET_STRUCT:
	case IOC_P2P_SET_STRUCT:
	case IOC_P2P_GET_REQ_DEVICE_INFO:
#if 0
		ret = rP2PIwPrivHandler[i4Cmd - SIOCIWFIRSTPRIV](prDev,
									&rIwReqInfo,
									&(prIwReq->u),
									(char *)&(prIwReq->u));
#endif
		break;
#if CFG_SUPPORT_P2P_RSSI_QUERY
	case SIOCGIWSTATS:
		ret = mtk_p2p_wext_get_rssi(prDev, &rIwReqInfo, &(prIwReq->u), NULL);
		break;
#endif
	default:
		ret = -ENOTTY;
	}
#endif /* 0 */

	return ret;
}				/* end of p2pDoIOCTL() */


/*----------------------------------------------------------------------------*/
/*!
 * \brief To override p2p interface address
 *
 * \param[in] prDev Net device requested.
 * \param[in] addr  Pointer to address
 *
 * \retval 0 For success.
 * \retval -E2BIG For user's buffer size is too small.
 * \retval -EFAULT For fail.
 *
 */
/*----------------------------------------------------------------------------*/
int p2pSetMACAddress(IN struct net_device *prDev, void *addr)
{
	P_ADAPTER_T prAdapter = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct sockaddr *sa = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	P_BSS_INFO_T prDevBssInfo = NULL;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;
	P_GL_P2P_INFO_T prP2pInfo = NULL;

	ASSERT(prDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	if (!prDev || !addr) {
		DBGLOG(INIT, ERROR, "Set macaddr with ndev(%d) and addr(%d)\n",
		       (prDev == NULL) ? 0 : 1, (addr == NULL) ? 0 : 1);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prDev, &ucRoleIdx) != 0) {
		DBGLOG(INIT, ERROR, "can't find the matched dev");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter,
		ucRoleIdx, &ucBssIdx) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "can't find the matched bss");
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(INIT, ERROR, "bss is not active\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	prDevBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		P2P_DEV_BSS_INDEX);
	if (!prDevBssInfo) {
		DBGLOG(INIT, ERROR, "dev bss is not active\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	prP2pInfo = prGlueInfo->prP2PInfo[0];
	if (!prP2pInfo) {
		DBGLOG(INIT, ERROR, "p2p info is null\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	sa = (struct sockaddr *)addr;

	COPY_MAC_ADDR(prBssInfo->aucOwnMacAddr, sa->sa_data);
	COPY_MAC_ADDR(prDev->dev_addr, sa->sa_data);

	if (prP2pInfo->prDevHandler == prDev) {
		COPY_MAC_ADDR(prAdapter->rWifiVar.aucDeviceAddress,
			sa->sa_data);
		COPY_MAC_ADDR(prDevBssInfo->aucOwnMacAddr, sa->sa_data);
		DBGLOG(INIT, INFO,
			"[%d][%d] Set random macaddr to " MACSTR ".\n",
			ucBssIdx,
			prDevBssInfo->ucBssIndex,
			MAC2STR(prDevBssInfo->aucOwnMacAddr));
	} else {
		DBGLOG(INIT, INFO,
			"[%d] Set random macaddr to " MACSTR ".\n",
			ucBssIdx,
			MAC2STR(prBssInfo->aucOwnMacAddr));
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief To report the private supported IOCTLs table to user space.
 *
 * \param[in] prDev Net device requested.
 * \param[out] prIfReq Pointer to ifreq structure, content is copied back to
 *                  user space buffer in gl_iwpriv_table.
 *
 * \retval 0 For success.
 * \retval -E2BIG For user's buffer size is too small.
 * \retval -EFAULT For fail.
 *
 */
/*----------------------------------------------------------------------------*/
int
mtk_p2p_wext_get_priv(IN struct net_device *prDev,
		      IN struct iw_request_info *info, IN OUT union iwreq_data *wrqu, IN OUT char *extra)
{
	struct iw_point *prData = (struct iw_point *)&wrqu->data;
	UINT_16 u2BufferSize = 0;

	ASSERT(prDev);

	u2BufferSize = prData->length;

	/* update our private table size */
	prData->length = (__u16) sizeof(rP2PIwPrivTable) / sizeof(struct iw_priv_args);

	if (u2BufferSize < prData->length)
		return -E2BIG;

	if (prData->length) {
		if (copy_to_user(prData->pointer, rP2PIwPrivTable, sizeof(rP2PIwPrivTable)))
			return -EFAULT;
	}

	return 0;
}				/* end of mtk_p2p_wext_get_priv() */

