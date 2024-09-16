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
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>

#include "gl_os.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "precomp.h"
#include "gl_cfg80211.h"
#include "gl_vendor.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* These values must sync from Wifi HAL
 * /hardware/libhardware_legacy/include/hardware_legacy/wifi_hal.h
 */
/* Basic infrastructure mode */
#define WIFI_FEATURE_INFRA              (0x0001)
/* Support for 5 GHz Band */
#define WIFI_FEATURE_INFRA_5G           (0x0002)
/* Support for GAS/ANQP */
#define WIFI_FEATURE_HOTSPOT            (0x0004)
/* Wifi-Direct */
#define WIFI_FEATURE_P2P                (0x0008)
/* Soft AP */
#define WIFI_FEATURE_SOFT_AP            (0x0010)
/* Google-Scan APIs */
#define WIFI_FEATURE_GSCAN              (0x0020)
/* Neighbor Awareness Networking */
#define WIFI_FEATURE_NAN                (0x0040)
/* Device-to-device RTT */
#define WIFI_FEATURE_D2D_RTT            (0x0080)
/* Device-to-AP RTT */
#define WIFI_FEATURE_D2AP_RTT           (0x0100)
/* Batched Scan (legacy) */
#define WIFI_FEATURE_BATCH_SCAN         (0x0200)
/* Preferred network offload */
#define WIFI_FEATURE_PNO                (0x0400)
/* Support for two STAs */
#define WIFI_FEATURE_ADDITIONAL_STA     (0x0800)
/* Tunnel directed link setup */
#define WIFI_FEATURE_TDLS               (0x1000)
/* Support for TDLS off channel */
#define WIFI_FEATURE_TDLS_OFFCHANNEL    (0x2000)
/* Enhanced power reporting */
#define WIFI_FEATURE_EPR                (0x4000)
/* Support for AP STA Concurrency */
#define WIFI_FEATURE_AP_STA             (0x8000)
/* Link layer stats collection */
#define WIFI_FEATURE_LINK_LAYER_STATS   (0x10000)
/* WiFi Logger */
#define WIFI_FEATURE_LOGGER             (0x20000)
/* WiFi PNO enhanced */
#define WIFI_FEATURE_HAL_EPNO           (0x40000)
/* RSSI Monitor */
#define WIFI_FEATURE_RSSI_MONITOR       (0x80000)
/* WiFi mkeep_alive */
#define WIFI_FEATURE_MKEEP_ALIVE        (0x100000)
/* ND offload configure */
#define WIFI_FEATURE_CONFIG_NDO         (0x200000)
/* Capture Tx transmit power levels */
#define WIFI_FEATURE_TX_TRANSMIT_POWER  (0x400000)
/* Enable/Disable firmware roaming */
#define WIFI_FEATURE_CONTROL_ROAMING    (0x800000)
/* Support Probe IE white listing */
#define WIFI_FEATURE_IE_WHITELIST       (0x1000000)
/* Support MAC & Probe Sequence Number randomization */
#define WIFI_FEATURE_SCAN_RAND          (0x2000000)
/* Support Tx Power Limit setting */
#define WIFI_FEATURE_SET_TX_POWER_LIMIT (0x4000000)
/* Support Using Body/Head Proximity for SAR */
#define WIFI_FEATURE_USE_BODY_HEAD_SAR  (0x8000000)

/* note: WIFI_FEATURE_GSCAN be enabled just for ACTS test item: scanner */
#define WIFI_HAL_FEATURE_SET ((WIFI_FEATURE_P2P) |\
			      (WIFI_FEATURE_SOFT_AP) |\
			      (WIFI_FEATURE_PNO) |\
			      (WIFI_FEATURE_TDLS) |\
			      (WIFI_FEATURE_RSSI_MONITOR) |\
			      (WIFI_FEATURE_CONTROL_ROAMING) |\
			      (WIFI_FEATURE_SET_TX_POWER_LIMIT)\
			      )

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

static struct nla_policy nla_parse_wifi_policy[WIFI_ATTRIBUTE_ROAMING_STATE + 1] = {
	[WIFI_ATTRIBUTE_BAND] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_NUM_CHANNELS] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_CHANNEL_LIST] = {.type = NLA_UNSPEC},

	[WIFI_ATTRIBUTE_NUM_FEATURE_SET] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_FEATURE_SET] = {.type = NLA_UNSPEC},
	[WIFI_ATTRIBUTE_PNO_RANDOM_MAC_OUI] = {.type = NLA_UNSPEC},
	[WIFI_ATTRIBUTE_NODFS_VALUE] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_COUNTRY_CODE] = {.type = NLA_STRING},

	[WIFI_ATTRIBUTE_MAX_RSSI] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_MIN_RSSI] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_RSSI_MONITOR_START] = {.type = NLA_U32},

	[WIFI_ATTRIBUTE_ROAMING_CAPABILITIES] = {.type = NLA_UNSPEC},
	[WIFI_ATTRIBUTE_ROAMING_BLACKLIST_NUM] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_ROAMING_BLACKLIST_BSSID] = {.type = NLA_UNSPEC},
	[WIFI_ATTRIBUTE_ROAMING_WHITELIST_NUM] = {.type = NLA_U32},
	[WIFI_ATTRIBUTE_ROAMING_WHITELIST_SSID] = {.type = NLA_UNSPEC},
	[WIFI_ATTRIBUTE_ROAMING_STATE] = {.type = NLA_U32},
};

#if CFG_SUPPORT_GSCN
static struct nla_policy nla_parse_gscan_policy[GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1] = {
	[GSCAN_ATTRIBUTE_NUM_BUCKETS] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BASE_PERIOD] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKETS_BAND] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKET_ID] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKET_PERIOD] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKET_CHANNELS] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_REPORT_THRESHOLD] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_REPORT_EVENTS] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_BSSID] = {.type = NLA_UNSPEC},
	[GSCAN_ATTRIBUTE_RSSI_LOW] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_RSSI_HIGH] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE] = {.type = NLA_U16},
	[GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE] = {.type = NLA_U32},
	[GSCAN_ATTRIBUTE_MIN_BREACHING] = {.type = NLA_U16},
	[GSCAN_ATTRIBUTE_NUM_AP] = {.type = NLA_U16},
	[GSCAN_ATTRIBUTE_HOTLIST_FLUSH] = {.type = NLA_U8},
	[GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH] = {.type = NLA_U8},
};
#endif

static struct nla_policy nla_parse_offloading_policy[MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC + 1] = {
	[MKEEP_ALIVE_ATTRIBUTE_ID] = {.type = NLA_U8},
	[MKEEP_ALIVE_ATTRIBUTE_IP_PKT] = {.type = NLA_UNSPEC},
	[MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN] = {.type = NLA_U16},
	[MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR] = {.type = NLA_UNSPEC},
	[MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR] = {.type = NLA_UNSPEC},
	[MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC] = {.type = NLA_U32},
};

static const struct nla_policy nla_get_acs_policy[
		WIFI_VENDOR_ATTR_ACS_MAX + 1] = {
	[WIFI_VENDOR_ATTR_ACS_HW_MODE] = { .type = NLA_U8 },
	[WIFI_VENDOR_ATTR_ACS_HT_ENABLED] = { .type = NLA_FLAG },
	[WIFI_VENDOR_ATTR_ACS_HT40_ENABLED] = { .type = NLA_FLAG },
	[WIFI_VENDOR_ATTR_ACS_VHT_ENABLED] = { .type = NLA_FLAG },
	[WIFI_VENDOR_ATTR_ACS_CHWIDTH] = { .type = NLA_U16 },
	[WIFI_VENDOR_ATTR_ACS_CH_LIST] = { .type = NLA_UNSPEC },
	[WIFI_VENDOR_ATTR_ACS_FREQ_LIST] = { .type = NLA_UNSPEC },
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

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

int mtk_cfg80211_vendor_get_channel_list(struct wiphy *wiphy, struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo;
	struct nlattr *attr;
	UINT_32 band = 0;
	UINT_8 ucNumOfChannel, i, j;
	RF_CHANNEL_INFO_T aucChannelList[64];
	UINT_32 num_channels;
	wifi_channel channels[64];
	struct sk_buff *skb;

	ASSERT(wiphy && wdev);
	if ((data == NULL) || !data_len)
		return -EINVAL;

	attr = (struct nlattr *)data;
	if (attr->nla_type == WIFI_ATTRIBUTE_BAND)
		band = nla_get_u32(attr);

	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;

	switch (band) {
	case 1: /* 2.4G band */
		rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			     64, &ucNumOfChannel, aucChannelList);
		break;
	case 2: /* 5G band without DFS channels */
		rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_5G, TRUE,
			     64, &ucNumOfChannel, aucChannelList);
		break;
	case 4: /* 5G band DFS channels only */
		rlmDomainGetDfsChnls(prGlueInfo->prAdapter, 64, &ucNumOfChannel, aucChannelList);
		break;
	default:
		ucNumOfChannel = 0;
		break;
	}

	kalMemZero(channels, sizeof(channels));
	for (i = 0, j = 0; i < ucNumOfChannel; i++) {
		/* We need to report frequency list to HAL */
		channels[j] = nicChannelNum2Freq(aucChannelList[i].ucChannelNum) / 1000;
		if (channels[j] == 0)
			continue;
		else {
			DBGLOG(REQ, TRACE, "channels[%d] = %d\n", j, channels[j]);
			j++;
		}
	}
	num_channels = j;
	DBGLOG(REQ, INFO, "Get channel list for band: %d, num_channels=%d\n", band, num_channels);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(channels));
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, WIFI_ATTRIBUTE_NUM_CHANNELS, num_channels) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb, WIFI_ATTRIBUTE_CHANNEL_LIST,
		(sizeof(wifi_channel) * num_channels), channels) < 0))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EFAULT;
}

int mtk_cfg80211_vendor_set_country_code(struct wiphy *wiphy, struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	struct nlattr *attr;
	UINT_8 country[2] = {0, 0};

	ASSERT(wiphy && wdev);
	if ((data == NULL) || (data_len == 0))
		return -EINVAL;

	attr = (struct nlattr *)data;
	if (attr->nla_type == WIFI_ATTRIBUTE_COUNTRY_CODE) {
		country[0] = *((PUINT_8)nla_data(attr));
		country[1] = *((PUINT_8)nla_data(attr) + 1);
	}

	DBGLOG(REQ, INFO, "Set country code: %c%c, iftype=%d\n", country[0], country[1], wdev->iftype);

	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode, country, 2, FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Set country code error: %x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

int mtk_cfg80211_vendor_set_scan_mac_oui(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct nlattr *attr;
	uint32_t i = 0;
	uint8_t ucMacOui[MAC_OUI_LEN];

	uint32_t u4BufLen = 0;

	ASSERT(wiphy);
	ASSERT(wdev);

	if (data == NULL || data_len <= 0) {
		DBGLOG(REQ, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	if (!prGlueInfo) {
		DBGLOG(REQ, ERROR, "Invalid glue info\n");
		return -EFAULT;
	}

	attr = (struct nlattr *)data;
	kalMemZero(ucMacOui, MAC_OUI_LEN);
	if (nla_type(attr) != WIFI_ATTRIBUTE_PNO_RANDOM_MAC_OUI) {
		DBGLOG(REQ, ERROR, "Set MAC oui type error(%u)\n",
			nla_type(attr));
		return -EINVAL;
	}

	if (nla_len(attr) != MAC_OUI_LEN) {
		DBGLOG(REQ, ERROR, "Set MAC oui length error(%u), %u needed\n",
			nla_len(attr), MAC_OUI_LEN);
		return -EINVAL;
	}

	for (i = 0; i < MAC_OUI_LEN; i++)
		ucMacOui[i] = *((uint8_t *)nla_data(attr) + i);

	DBGLOG(REQ, INFO, "Set MAC oui: %02x-%02x-%02x\n",
		ucMacOui[0], ucMacOui[1], ucMacOui[2]);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetScanMacOui,
		ucMacOui, MAC_OUI_LEN,
		FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Set MAC oui error: 0x%X\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

int mtk_cfg80211_vendor_get_roaming_capabilities(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	uint32_t maxNumOfList[2] = { MAX_FW_ROAMING_BLACKLIST_SIZE,
				     MAX_FW_ROAMING_WHITELIST_SIZE };
	struct sk_buff *skb;

	ASSERT(wiphy);

	DBGLOG(REQ, INFO,
		"Get roaming capabilities: max black/whitelist=%d/%d",
		maxNumOfList[0], maxNumOfList[1]);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(uint32_t) * 2);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, WIFI_ATTRIBUTE_ROAMING_CAPABILITIES,
				sizeof(uint32_t), &maxNumOfList[0]) < 0))
		goto nla_put_failure;
	if (unlikely(nla_put(skb, WIFI_ATTRIBUTE_ROAMING_CAPABILITIES,
				sizeof(uint32_t), &maxNumOfList[1]) < 0))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EFAULT;
}

int mtk_cfg80211_vendor_config_roaming(struct wiphy *wiphy,
				 struct wireless_dev *wdev, const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4ResultLen = 0;
	WLAN_STATUS rStatus;

	DBGLOG(REQ, INFO, "Receives roaming blacklist & whitelist with data_len=%d\n", data_len);
	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || (data_len == 0))
		return -EINVAL;

	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EINVAL;

	if (prGlueInfo->u4FWRoamingEnable == 0) {
		DBGLOG(REQ, INFO, "FWRoaming is disabled (FWRoamingEnable=%d)\n", prGlueInfo->u4FWRoamingEnable);
		return WLAN_STATUS_SUCCESS;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidConfigRoaming, (PVOID)data, data_len,
			   FALSE, FALSE, FALSE, FALSE, &u4ResultLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "%s config roaming error:%x\n", __func__, rStatus);
		return -EINVAL;
	}

	return WLAN_STATUS_SUCCESS;
}

int mtk_cfg80211_vendor_enable_roaming(struct wiphy *wiphy,
				 struct wireless_dev *wdev, const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct nlattr *attr;

	ASSERT(wiphy);	/* change to if (wiphy == NULL) then return? */
	ASSERT(wdev);	/* change to if (wiphy == NULL) then return? */

	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;

	attr = (struct nlattr *)data;
	if (attr->nla_type == WIFI_ATTRIBUTE_ROAMING_STATE)
		prGlueInfo->u4FWRoamingEnable = nla_get_u32(attr);

	DBGLOG(REQ, INFO, "FWK set FWRoamingEnable = %d\n", prGlueInfo->u4FWRoamingEnable);

	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_GSCN
int mtk_cfg80211_vendor_get_gscan_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Status = -EINVAL;
	PARAM_WIFI_GSCAN_CAPABILITIES_STRUCT_T rGscanCapabilities;
	struct sk_buff *skb;

	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);

	ASSERT(wiphy);
	ASSERT(wdev);
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(rGscanCapabilities));
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed:%x\n", __func__, i4Status);
		return -ENOMEM;
	}

	kalMemZero(&rGscanCapabilities, sizeof(rGscanCapabilities));

	/* GSCN capabilities return from driver not firmware */
	rGscanCapabilities.max_scan_cache_size = PSCAN_MAX_SCAN_CACHE_SIZE;
	rGscanCapabilities.max_scan_buckets = GSCAN_MAX_BUCKETS;
	rGscanCapabilities.max_ap_cache_per_scan = PSCAN_MAX_AP_CACHE_PER_SCAN;
	rGscanCapabilities.max_rssi_sample_size = 10;
	rGscanCapabilities.max_scan_reporting_threshold = GSCAN_MAX_REPORT_THRESHOLD;
	rGscanCapabilities.max_hotlist_bssids = MAX_HOTLIST_BSSIDS;
	rGscanCapabilities.max_hotlist_ssids = MAX_HOTLIST_SSIDS;
	rGscanCapabilities.max_significant_wifi_change_aps = MAX_SIGNIFICANT_CHANGE_APS;
	rGscanCapabilities.max_bssid_history_entries = PSCAN_MAX_AP_CACHE_PER_SCAN * PSCAN_MAX_SCAN_CACHE_SIZE;
	rGscanCapabilities.max_number_epno_networks = 0;
	rGscanCapabilities.max_number_epno_networks_by_ssid = 0;
	rGscanCapabilities.max_number_of_white_listed_ssid = 0;

	/* NLA_PUT_U32(skb, NL80211_ATTR_VENDOR_ID, GOOGLE_OUI); */
	/* NLA_PUT_U32(skb, NL80211_ATTR_VENDOR_SUBCMD, GSCAN_SUBCMD_GET_CAPABILITIES); */
	/* NLA_PUT(skb, GSCAN_ATTRIBUTE_CAPABILITIES, sizeof(rGscanCapabilities), &rGscanCapabilities); */
	if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_CAPABILITIES,
		sizeof(rGscanCapabilities), &rGscanCapabilities) < 0))
		goto nla_put_failure;

	i4Status = cfg80211_vendor_cmd_reply(skb);
	return i4Status;

nla_put_failure:
	kfree_skb(skb);
	return i4Status;
}

int mtk_cfg80211_vendor_set_config(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	/* CMD_GSCN_REQ_T rCmdGscnParam; */

	/* INT_32 i4Status = -EINVAL; */
	P_PARAM_WIFI_GSCAN_CMD_PARAMS prWifiScanCmd = NULL;
	struct nlattr *attr[GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD + 1];
	struct nlattr *pbucket, *pchannel;
	UINT_32 len_basic, len_bucket, len_channel;
	int i, j, k;
	uint_32 u4ArySize;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;

	prWifiScanCmd = (P_PARAM_WIFI_GSCAN_CMD_PARAMS) kalMemAlloc(sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS), VIR_MEM_TYPE);
	if (!prWifiScanCmd) {
		DBGLOG(REQ, ERROR, "Can not alloc memory for PARAM_WIFI_GSCAN_CMD_PARAMS\n");
		return -ENOMEM;
	}

	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	kalMemZero(prWifiScanCmd, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	kalMemZero(attr, sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD + 1));

	NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD, (struct nlattr *)(data - NLA_HDRLEN),
			nla_parse_gscan_policy);
	len_basic = 0;
	for (k = GSCAN_ATTRIBUTE_NUM_BUCKETS; k <= GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD; k++) {
		if (attr[k]) {
			switch (k) {
			case GSCAN_ATTRIBUTE_BASE_PERIOD:
				prWifiScanCmd->base_period = nla_get_u32(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_NUM_BUCKETS:
				u4ArySize = nla_get_u32(attr[k]);
				prWifiScanCmd->num_buckets =
					(u4ArySize <= GSCAN_MAX_BUCKETS)
					? u4ArySize : GSCAN_MAX_BUCKETS;
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				DBGLOG(REQ, TRACE, "attr=0x%x, num_buckets=%d nla_len=%d,\r\n",
				       *(UINT_32 *) attr[k], prWifiScanCmd->num_buckets, attr[k]->nla_len);
				break;
			}
		}
	}
	pbucket = (struct nlattr *)((UINT_8 *) data + len_basic);
	DBGLOG(REQ, TRACE, "+++basic attribute size=%d pbucket=%p\r\n", len_basic, pbucket);

	for (i = 0; i < prWifiScanCmd->num_buckets; i++) {
		if (NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD, (struct nlattr *)pbucket,
			nla_parse_gscan_policy) < 0)
			goto nla_put_failure;
		len_bucket = 0;
		for (k = GSCAN_ATTRIBUTE_NUM_BUCKETS; k <= GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD; k++) {
			if (attr[k] == NULL)
				continue;
			switch (k) {
			case GSCAN_ATTRIBUTE_BUCKETS_BAND:
				prWifiScanCmd->buckets[i].band = nla_get_u32(attr[k]);
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_BUCKET_ID:
				prWifiScanCmd->buckets[i].bucket = nla_get_u32(attr[k]);
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_BUCKET_PERIOD:
				prWifiScanCmd->buckets[i].period = nla_get_u32(attr[k]);
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT:
				prWifiScanCmd->buckets[i].step_count = nla_get_u32(attr[k]);
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD:
				prWifiScanCmd->buckets[i].max_period = nla_get_u32(attr[k]);
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_REPORT_EVENTS:
				prWifiScanCmd->buckets[i].report_events = nla_get_u32(attr[k]);
				/* parameter validity check */
				if (((prWifiScanCmd->buckets[i].report_events & REPORT_EVENTS_EACH_SCAN)
					!= REPORT_EVENTS_EACH_SCAN)
					&& ((prWifiScanCmd->buckets[i].report_events & REPORT_EVENTS_FULL_RESULTS)
					!= REPORT_EVENTS_FULL_RESULTS)
					&& ((prWifiScanCmd->buckets[i].report_events & REPORT_EVENTS_NO_BATCH)
					!= REPORT_EVENTS_NO_BATCH))
					prWifiScanCmd->buckets[i].report_events = REPORT_EVENTS_EACH_SCAN;
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS:
				prWifiScanCmd->buckets[i].num_channels = nla_get_u32(attr[k]);
				len_bucket += NLA_ALIGN(attr[k]->nla_len);
				DBGLOG(REQ, TRACE, "bucket%d: attr=0x%x, num_channels=%d nla_len=%d,\r\n",
				       i, *(UINT_32 *) attr[k], nla_get_u32(attr[k]), attr[k]->nla_len);
				break;
			}
		}
		pbucket = (struct nlattr *)((UINT_8 *) pbucket + NLA_HDRLEN);
		/* request.attr_start(i) as nested attribute */
		DBGLOG(REQ, TRACE, "+++pure bucket size=%d pbucket=%p\r\n", len_bucket, pbucket);
		pbucket = (struct nlattr *)((UINT_8 *) pbucket + len_bucket);
		/* pure bucket payload, not include channels */

		/*don't need to use nla_parse_nested to parse channels */
		/* the header of channel in bucket i */
		pchannel = (struct nlattr *)((UINT_8 *) pbucket + NLA_HDRLEN);
		for (j = 0; j < prWifiScanCmd->buckets[i].num_channels; j++) {
			prWifiScanCmd->buckets[i].channels[j].channel = nla_get_u32(pchannel);
			len_channel = NLA_ALIGN(pchannel->nla_len);
			DBGLOG(REQ, TRACE,
				"attr=0x%x, channel=%d,\r\n", *(UINT_32 *) pchannel, nla_get_u32(pchannel));

			pchannel = (struct nlattr *)((UINT_8 *) pchannel + len_channel);
		}
		pbucket = pchannel;
	}

	DBGLOG(REQ, INFO, "base_period=%d, num_buckets=%d, bucket0: %d %d %d %d %d %d\n",
		prWifiScanCmd->base_period, prWifiScanCmd->num_buckets,
		prWifiScanCmd->buckets[0].bucket, prWifiScanCmd->buckets[0].band,
		prWifiScanCmd->buckets[0].period, prWifiScanCmd->buckets[0].max_period,
		prWifiScanCmd->buckets[0].num_channels,	prWifiScanCmd->buckets[0].report_events);

	DBGLOG(REQ, TRACE, "bucket0: num_channels=%d, %d, %d; bucket1: num_channels=%d, %d, %d\n",
		prWifiScanCmd->buckets[0].num_channels,
		prWifiScanCmd->buckets[0].channels[0].channel, prWifiScanCmd->buckets[0].channels[1].channel,
		prWifiScanCmd->buckets[1].num_channels,
		prWifiScanCmd->buckets[1].channels[0].channel, prWifiScanCmd->buckets[1].channels[1].channel);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetGSCNParam,
			   prWifiScanCmd, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	kalMemFree(prWifiScanCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	return 0;

nla_put_failure:
	if (prWifiScanCmd != NULL)
		kalMemFree(prWifiScanCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	return -ENOMEM;
}

int mtk_cfg80211_vendor_set_scan_config(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;

	INT_32 i4Status = -ENOMEM;
	/*PARAM_WIFI_GSCAN_CMD_PARAMS rWifiScanCmd;*/
	P_PARAM_WIFI_GSCAN_CMD_PARAMS prWifiScanCmd = NULL;
	struct nlattr *attr[GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE + 1];
	/* UINT_32 num_scans = 0; */	/* another attribute */
	int k;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	/*kalMemZero(&rWifiScanCmd, sizeof(rWifiScanCmd));*/
	prWifiScanCmd = kalMemAlloc(sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS), VIR_MEM_TYPE);
	if (prWifiScanCmd == NULL)
		goto nla_put_failure;
	kalMemZero(prWifiScanCmd, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	kalMemZero(attr, sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE + 1));

	if (NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE,
		(struct nlattr *)(data - NLA_HDRLEN), nla_parse_gscan_policy) < 0)
		goto nla_put_failure;
	for (k = GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN; k <= GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE; k++) {
		if (attr[k]) {
			switch (k) {
			case GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN:
				prWifiScanCmd->max_ap_per_scan = nla_get_u32(attr[k]);
				break;
			case GSCAN_ATTRIBUTE_REPORT_THRESHOLD:
				prWifiScanCmd->report_threshold_percent = nla_get_u32(attr[k]);
				break;
			case GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE:
				prWifiScanCmd->report_threshold_num_scans = nla_get_u32(attr[k]);
				break;
			}
		}
	}
	/* parameter validity check */
	if (prWifiScanCmd->report_threshold_percent > 100)
		prWifiScanCmd->report_threshold_percent = 100;
	DBGLOG(REQ, TRACE, "attr=0x%x, attr2=0x%x ", *(UINT_32 *) attr[GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN],
	       *(UINT_32 *) attr[GSCAN_ATTRIBUTE_REPORT_THRESHOLD]);

	DBGLOG(REQ, INFO, "max_ap_per_scan=%d, report_threshold=%d num_scans=%d\r\n",
	       prWifiScanCmd->max_ap_per_scan, prWifiScanCmd->report_threshold_percent,
	       prWifiScanCmd->report_threshold_num_scans);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetGSCNConfig,
			   prWifiScanCmd, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	kalMemFree(prWifiScanCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	return 0;

nla_put_failure:
	if (prWifiScanCmd != NULL)
		kalMemFree(prWifiScanCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	return i4Status;
}

int mtk_cfg80211_vendor_set_significant_change(struct wiphy *wiphy, struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	INT_32 i4Status = -EINVAL;
	P_PARAM_WIFI_SIGNIFICANT_CHANGE prWifiChangeCmd = NULL;
	UINT_8 flush = 0;
	/* struct nlattr *attr[GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1]; */
	struct nlattr **attr = NULL;
	struct nlattr *paplist;
	int i, k;
	UINT_32 len_basic, len_aplist;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	for (i = 0; i < 6; i++)
		DBGLOG(REQ, LOUD, "0x%x 0x%x 0x%x 0x%x\r\n",
			*((UINT_32 *) data + i * 4), *((UINT_32 *) data + i * 4 + 1),
			*((UINT_32 *) data + i * 4 + 2), *((UINT_32 *) data + i * 4 + 3));
	prWifiChangeCmd = kalMemAlloc(sizeof(PARAM_WIFI_SIGNIFICANT_CHANGE), VIR_MEM_TYPE);
	if (prWifiChangeCmd == NULL)
		goto nla_put_failure;
	kalMemZero(prWifiChangeCmd, sizeof(PARAM_WIFI_SIGNIFICANT_CHANGE));
	attr = kalMemAlloc(sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1), VIR_MEM_TYPE);
	if (attr == NULL)
		goto nla_put_failure;
	kalMemZero(attr, sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1));

	if (NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH,
		(struct nlattr *)(data - NLA_HDRLEN), nla_parse_gscan_policy) < 0)
		goto nla_put_failure;
	len_basic = 0;
	for (k = GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE; k <= GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH; k++) {
		if (attr[k]) {
			switch (k) {
			case GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE:
				prWifiChangeCmd->rssi_sample_size = nla_get_u16(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE:
				prWifiChangeCmd->lost_ap_sample_size = nla_get_u16(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_MIN_BREACHING:
				prWifiChangeCmd->min_breaching = nla_get_u16(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_NUM_AP:
				prWifiChangeCmd->num_ap = nla_get_u16(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				DBGLOG(REQ, TRACE, "attr=0x%x, num_ap=%d nla_len=%d,\r\n",
				       *(UINT_32 *) attr[k], prWifiChangeCmd->num_ap, attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH:
				flush = nla_get_u8(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			}
		}
	}
	paplist = (struct nlattr *)((UINT_8 *) data + len_basic);
	DBGLOG(REQ, TRACE, "+++basic attribute size=%d flush=%d\r\n", len_basic, flush);

	if (paplist->nla_type == GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_BSSIDS)
		paplist = (struct nlattr *)((UINT_8 *) paplist + NLA_HDRLEN);

	for (i = 0; i < prWifiChangeCmd->num_ap; i++) {
		if (NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_RSSI_HIGH, (struct nlattr *)paplist,
				nla_parse_gscan_policy) < 0)
			goto nla_put_failure;
		paplist = (struct nlattr *)((UINT_8 *) paplist + NLA_HDRLEN);
		/* request.attr_start(i) as nested attribute */
		len_aplist = 0;
		for (k = GSCAN_ATTRIBUTE_BSSID; k <= GSCAN_ATTRIBUTE_RSSI_HIGH; k++) {
			if (attr[k]) {
				switch (k) {
				case GSCAN_ATTRIBUTE_BSSID:
					kalMemCopy(prWifiChangeCmd->ap[i].bssid, nla_data(attr[k]), sizeof(mac_addr));
					len_aplist += NLA_ALIGN(attr[k]->nla_len);
					break;
				case GSCAN_ATTRIBUTE_RSSI_LOW:
					prWifiChangeCmd->ap[i].low = nla_get_u32(attr[k]);
					len_aplist += NLA_ALIGN(attr[k]->nla_len);
					break;
				case GSCAN_ATTRIBUTE_RSSI_HIGH:
					prWifiChangeCmd->ap[i].high = nla_get_u32(attr[k]);
					len_aplist += NLA_ALIGN(attr[k]->nla_len);
					break;
				}
			}
		}
		if (((i + 1) % 4 == 0) || (i == prWifiChangeCmd->num_ap - 1))
			DBGLOG(REQ, TRACE, "ap[%d], len_aplist=%d\n", i, len_aplist);
		else
			DBGLOG(REQ, TRACE, "ap[%d], len_aplist=%d\t", i, len_aplist);
		paplist = (struct nlattr *)((UINT_8 *) paplist + len_aplist);
	}

	DBGLOG(REQ, TRACE,
		"flush=%d, rssi_sample_size=%d lost_ap_sample_size=%d min_breaching=%d",
		flush, prWifiChangeCmd->rssi_sample_size, prWifiChangeCmd->lost_ap_sample_size,
		prWifiChangeCmd->min_breaching);
	DBGLOG(REQ, TRACE,
		"ap[0].channel=%d low=%d high=%d, ap[1].channel=%d low=%d high=%d",
		prWifiChangeCmd->ap[0].channel, prWifiChangeCmd->ap[0].low, prWifiChangeCmd->ap[0].high,
		prWifiChangeCmd->ap[1].channel, prWifiChangeCmd->ap[1].low, prWifiChangeCmd->ap[1].high);
	kalMemFree(prWifiChangeCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_SIGNIFICANT_CHANGE));
	kalMemFree(attr, VIR_MEM_TYPE, sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1));
	return 0;

nla_put_failure:
	if (prWifiChangeCmd)
		kalMemFree(prWifiChangeCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_SIGNIFICANT_CHANGE));
	if (attr)
		kalMemFree(attr, VIR_MEM_TYPE,
			sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1));
	return i4Status;
}

int mtk_cfg80211_vendor_set_hotlist(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	CMD_SET_PSCAN_ADD_HOTLIST_BSSID rCmdPscnAddHotlist;

	INT_32 i4Status = -EINVAL;
	P_PARAM_WIFI_BSSID_HOTLIST prWifiHotlistCmd = NULL;
	UINT_8 flush = 0;
	/* struct nlattr *attr[GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1]; */
	struct nlattr **attr = NULL;
	struct nlattr *paplist;
	int i, k;
	UINT_32 len_basic, len_aplist;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	for (i = 0; i < 5; i++)
		DBGLOG(REQ, LOUD, "0x%x 0x%x 0x%x 0x%x\r\n",
			*((UINT_32 *) data + i * 4), *((UINT_32 *) data + i * 4 + 1),
			*((UINT_32 *) data + i * 4 + 2), *((UINT_32 *) data + i * 4 + 3));
	prWifiHotlistCmd = kalMemAlloc(sizeof(PARAM_WIFI_BSSID_HOTLIST), VIR_MEM_TYPE);
	if (prWifiHotlistCmd == NULL)
		goto nla_put_failure;
	kalMemZero(prWifiHotlistCmd, sizeof(PARAM_WIFI_BSSID_HOTLIST));
	attr = kalMemAlloc(sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1), VIR_MEM_TYPE);
	if (attr == NULL)
		goto nla_put_failure;
	kalMemZero(attr, sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1));

	if (NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_NUM_AP, (struct nlattr *)(data - NLA_HDRLEN),
				nla_parse_gscan_policy) < 0)
		goto nla_put_failure;
	len_basic = 0;
	for (k = GSCAN_ATTRIBUTE_HOTLIST_FLUSH; k <= GSCAN_ATTRIBUTE_NUM_AP; k++) {
		if (attr[k]) {
			switch (k) {
			case GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE:
				prWifiHotlistCmd->lost_ap_sample_size = nla_get_u32(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_NUM_AP:
				prWifiHotlistCmd->num_ap = nla_get_u16(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				DBGLOG(REQ, TRACE, "attr=0x%x, num_ap=%d nla_len=%d,\r\n",
				       *(UINT_32 *) attr[k], prWifiHotlistCmd->num_ap, attr[k]->nla_len);
				break;
			case GSCAN_ATTRIBUTE_HOTLIST_FLUSH:
				flush = nla_get_u8(attr[k]);
				len_basic += NLA_ALIGN(attr[k]->nla_len);
				break;
			}
		}
	}
	paplist = (struct nlattr *)((UINT_8 *) data + len_basic);
	DBGLOG(REQ, INFO, "+++basic attribute size=%d flush=%d\r\n", len_basic, flush);

	if (paplist->nla_type == GSCAN_ATTRIBUTE_HOTLIST_BSSIDS)
		paplist = (struct nlattr *)((UINT_8 *) paplist + NLA_HDRLEN);

	for (i = 0; i < prWifiHotlistCmd->num_ap; i++) {
		if (NLA_PARSE_NESTED(attr, GSCAN_ATTRIBUTE_RSSI_HIGH, (struct nlattr *)paplist,
				nla_parse_gscan_policy) < 0)
			goto nla_put_failure;
		paplist = (struct nlattr *)((UINT_8 *) paplist + NLA_HDRLEN);
		/* request.attr_start(i) as nested attribute */
		len_aplist = 0;
		for (k = GSCAN_ATTRIBUTE_BSSID; k <= GSCAN_ATTRIBUTE_RSSI_HIGH; k++) {
			if (attr[k]) {
				switch (k) {
				case GSCAN_ATTRIBUTE_BSSID:
					kalMemCopy(prWifiHotlistCmd->ap[i].bssid, nla_data(attr[k]), sizeof(mac_addr));
					len_aplist += NLA_ALIGN(attr[k]->nla_len);
					break;
				case GSCAN_ATTRIBUTE_RSSI_LOW:
					prWifiHotlistCmd->ap[i].low = nla_get_u32(attr[k]);
					len_aplist += NLA_ALIGN(attr[k]->nla_len);
					break;
				case GSCAN_ATTRIBUTE_RSSI_HIGH:
					prWifiHotlistCmd->ap[i].high = nla_get_u32(attr[k]);
					len_aplist += NLA_ALIGN(attr[k]->nla_len);
					break;
				}
			}
		}
		if (((i + 1) % 4 == 0) || (i == prWifiHotlistCmd->num_ap - 1))
			DBGLOG(REQ, TRACE, "ap[%d], len_aplist=%d\n", i, len_aplist);
		else
			DBGLOG(REQ, TRACE, "ap[%d], len_aplist=%d\t", i, len_aplist);
		paplist = (struct nlattr *)((UINT_8 *) paplist + len_aplist);
	}

	DBGLOG(REQ, INFO,
	"flush=%d, lost_ap_sample_size=%d, Hotlist:ap[0].channel=%d low=%d high=%d, ap[1].channel=%d low=%d high=%d",
		flush, prWifiHotlistCmd->lost_ap_sample_size,
		prWifiHotlistCmd->ap[0].channel, prWifiHotlistCmd->ap[0].low, prWifiHotlistCmd->ap[0].high,
		prWifiHotlistCmd->ap[1].channel, prWifiHotlistCmd->ap[1].low, prWifiHotlistCmd->ap[1].high);

	memcpy(&(rCmdPscnAddHotlist.aucMacAddr), &(prWifiHotlistCmd->ap[0].bssid), 6 * sizeof(UINT_8));
	rCmdPscnAddHotlist.ucFlags = (UINT_8) TRUE;
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemFree(prWifiHotlistCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_BSSID_HOTLIST));
	kalMemFree(attr, VIR_MEM_TYPE, sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1));
	return 0;

nla_put_failure:
	if (prWifiHotlistCmd)
		kalMemFree(prWifiHotlistCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_BSSID_HOTLIST));
	if (attr)
		kalMemFree(attr, VIR_MEM_TYPE,
			sizeof(struct nlattr *) * (GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH + 1));
	return i4Status;
}

int mtk_cfg80211_vendor_enable_scan(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_WIFI_GSCAN_ACTION_CMD_PARAMS rWifiScanActionCmd;

	INT_32 i4Status = -EINVAL;
	struct nlattr *attr;
	UINT_8 gGScanEn = 0;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	DBGLOG(REQ, TRACE, "vendor command: data_len=%d, data=0x%x 0x%x\r\n",
		data_len, *((UINT_32 *) data), *((UINT_32 *) data + 1));

	attr = (struct nlattr *)data;
	if (attr->nla_type == GSCAN_ATTRIBUTE_ENABLE_FEATURE)
		gGScanEn = nla_get_u32(attr);
	DBGLOG(REQ, INFO, "gGScanEn=%d\r\n", gGScanEn);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
	if (gGScanEn == TRUE)
		rWifiScanActionCmd.ucPscanAct = PSCAN_ACT_ENABLE;
	else
		rWifiScanActionCmd.ucPscanAct = PSCAN_ACT_DISABLE;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetGSCNAction,
			   &rWifiScanActionCmd,
			   sizeof(PARAM_WIFI_GSCAN_ACTION_CMD_PARAMS), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	return 0;

nla_put_failure:
	return i4Status;
}

int mtk_cfg80211_vendor_enable_full_scan_results(struct wiphy *wiphy, struct wireless_dev *wdev,
						 const void *data, int data_len)
{
	INT_32 i4Status = -EINVAL;
	struct nlattr *attr;
	UINT_8 gFullScanResultsEn = 0;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	DBGLOG(REQ, TRACE, "vendor command: data_len=%d, data=0x%x 0x%x\r\n",
		data_len, *((UINT_32 *) data), *((UINT_32 *) data + 1));

	attr = (struct nlattr *)data;
	if (attr->nla_type == GSCAN_ENABLE_FULL_SCAN_RESULTS)
		gFullScanResultsEn = nla_get_u32(attr);
	DBGLOG(REQ, INFO, "gFullScanResultsEn=%d\r\n", gFullScanResultsEn);

	return 0;

nla_put_failure:
	return i4Status;
}

int mtk_cfg80211_vendor_get_gscan_result(struct wiphy *wiphy, struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	/*WLAN_STATUS rStatus;*/
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_WIFI_GSCAN_GET_RESULT_PARAMS rGScanResultParm;

	INT_32 i4Status = -EINVAL;
	struct nlattr *attr;
	UINT_32 get_num = 0, real_num = 0;
	UINT_8 flush = 0;
	/*
	 * PARAM_WIFI_GSCAN_RESULT result[4], *pResult;
	 * struct sk_buff *skb;
	 */
	int i; /*int j;*/
	/*UINT_32 scan_id;*/

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	for (i = 0; i < 2; i++)
		DBGLOG(REQ, LOUD, "0x%x 0x%x 0x%x 0x%x\r\n", *((UINT_32 *) data + i * 4),
			*((UINT_32 *) data + i * 4 + 1), *((UINT_32 *) data + i * 4 + 2),
			*((UINT_32 *) data + i * 4 + 3));

	attr = (struct nlattr *)data;
	if (attr->nla_type == GSCAN_ATTRIBUTE_NUM_OF_RESULTS) {
		get_num = nla_get_u32(attr);
		attr = (struct nlattr *)((UINT_8 *) attr + attr->nla_len);
	}
	if (attr->nla_type == GSCAN_ATTRIBUTE_FLUSH_RESULTS) {
		flush = nla_get_u8(attr);
		attr = (struct nlattr *)((UINT_8 *) attr + attr->nla_len);
	}
	DBGLOG(REQ, TRACE, "number=%d, flush=%d\r\n", get_num, flush);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* parameter validity check */
	real_num = (get_num < PSCAN_MAX_AP_CACHE_PER_SCAN) ? get_num : PSCAN_MAX_AP_CACHE_PER_SCAN;
	get_num = real_num;

	if (flush)
		flush = TRUE;

	rGScanResultParm.get_num = get_num;
	rGScanResultParm.flush = flush;
	{
		kalIoctl(prGlueInfo,
			 wlanoidGetGSCNResult,
			 &rGScanResultParm,
			 sizeof(PARAM_WIFI_GSCAN_GET_RESULT_PARAMS), TRUE, TRUE, TRUE, FALSE, &u4BufLen);
	}
	DBGLOG(REQ, LOUD, "u4BufLen=%d\r\n", u4BufLen);
	return 0;

nla_put_failure:
	return i4Status;
}

int mtk_cfg80211_vendor_gscan_results(struct wiphy *wiphy, struct wireless_dev *wdev,
				      const void *data, int data_len, BOOLEAN complete, BOOLEAN compValue)
{
	P_PARAM_WIFI_GSCAN_RESULT_REPORT prGscnResult = NULL;
	UINT_32 u4SizeofGScanResults;
	P_PARAM_WIFI_GSCAN_RESULT prResults = NULL; /* similar to  WIFI_GSCAN_RESULT_T*/
	UINT_32 scan_id = 0;
	UINT_8 scan_flag = 0;
	UINT_32 real_num = 0;
	UINT_32 ch_bucket_mask = 0;
	INT_32 i4Status = -EINVAL;
	struct sk_buff *skb;
	struct nlattr *attr1, *attr2;

	ASSERT(data);
	prGscnResult = (P_PARAM_WIFI_GSCAN_RESULT_REPORT)data;
	u4SizeofGScanResults = data_len;

	if (prGscnResult) {
		scan_id = prGscnResult->u4ScanId;
		scan_flag = prGscnResult->ucScanFlag;
		ch_bucket_mask = prGscnResult->u4BucketMask;
		real_num = prGscnResult->u4NumOfResults;
	}
	if (complete)
		DBGLOG(SCN, INFO, "complete=%d, compValue=%d", complete, compValue);
	else
		DBGLOG(SCN, TRACE, "scan_id=%d 0x%x, bkt=0x%x, num=%d, u4SizeofGScanResults=%d\r\n",
			scan_id, scan_flag, ch_bucket_mask, real_num, u4SizeofGScanResults);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, u4SizeofGScanResults);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed:%x\n", __func__, i4Status);
		return -ENOMEM;
	}

	attr1 = nla_nest_start(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS);

	if (complete == TRUE) {
		/* NLA_PUT_U8(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE, 1); */
		{
			unsigned char __tmp = compValue;

			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE,
				sizeof(unsigned int), &__tmp) < 0))
				goto nla_put_failure;
		}
	} else {
		attr2 = nla_nest_start(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS);

		/*NLA_PUT_U32(skb, GSCAN_ATTRIBUTE_SCAN_ID, scan_id);*/
		{
			unsigned int __tmp = scan_id;

			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_ID, sizeof(unsigned int), &__tmp) < 0))
				goto nla_put_failure;
		}
		/*NLA_PUT_U8(skb, GSCAN_ATTRIBUTE_SCAN_FLAGS, 1);*/
		{
			unsigned char __tmp = scan_flag;

			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_FLAGS, sizeof(u8), &__tmp) < 0))
				goto nla_put_failure;
		}
		/*NLA_PUT_U32(skb, GSCAN_ATTRIBUTE_NUM_OF_RESULTS, real_num);*/
		{
			unsigned int __tmp = real_num;

			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_NUM_OF_RESULTS,
				sizeof(unsigned int), &__tmp) < 0))
				goto nla_put_failure;
		}

		{
			unsigned int __tmp = ch_bucket_mask;

			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_CH_BUCKET_BITMASK,
				sizeof(unsigned int), &__tmp) < 0))
				goto nla_put_failure;
		}

		if (prGscnResult)
			prResults = (P_PARAM_WIFI_GSCAN_RESULT) prGscnResult->rResult;
		if (prResults) {
			/*NLA_PUT(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS, sizeof(PARAM_WIFI_GSCAN_RESULT) * real_num,
			*		prResults);
			*/
			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS,
				sizeof(PARAM_WIFI_GSCAN_RESULT)*real_num, prResults) < 0))
				goto nla_put_failure;
		}

		if (attr2)
			nla_nest_end(skb, attr2);
	}

	if (attr1)
		nla_nest_end(skb, attr1);

	i4Status = cfg80211_vendor_cmd_reply(skb);
	if (i4Status)
		DBGLOG(REQ, ERROR, "i4Status=%d real_num=%d\n", i4Status, real_num);
	return real_num;

nla_put_failure:
	kfree_skb(skb);
	DBGLOG(REQ, ERROR, "nla_put_failure\n");
	return -ENOMEM;
}
#endif

int mtk_cfg80211_vendor_get_rtt_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Status = -EINVAL;
	PARAM_WIFI_RTT_CAPABILITIES rRttCapabilities;
	struct sk_buff *skb;

	DBGLOG(REQ, TRACE, "vendor command\r\n");

	ASSERT(wiphy);
	ASSERT(wdev);
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(rRttCapabilities));
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed:%x\n", __func__, i4Status);
		return -ENOMEM;
	}

	kalMemZero(&rRttCapabilities, sizeof(rRttCapabilities));

	/* RTT Capabilities return from driver not firmware */
	rRttCapabilities.rtt_one_sided_supported = 0;
	rRttCapabilities.rtt_ftm_supported = 0;
	rRttCapabilities.lci_support = 0;
	rRttCapabilities.lcr_support = 0;
	rRttCapabilities.preamble_support = 0;
	rRttCapabilities.bw_support = 0;

	if (unlikely(nla_put(skb, RTT_ATTRIBUTE_CAPABILITIES,
		sizeof(rRttCapabilities), &rRttCapabilities) < 0))
		goto nla_put_failure;

	i4Status = cfg80211_vendor_cmd_reply(skb);
	return i4Status;

nla_put_failure:
	kfree_skb(skb);
	return i4Status;
}

int mtk_cfg80211_vendor_llstats_get_info(struct wiphy *wiphy, struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	INT_32 i4Status = -EINVAL;
	WIFI_RADIO_STAT *pRadioStat;
	struct sk_buff *skb;
	UINT_32 u4BufLen = 0;

	ASSERT(wiphy);
	ASSERT(wdev);

	u4BufLen = sizeof(WIFI_RADIO_STAT) + sizeof(WIFI_IFACE_STAT);
	pRadioStat = kalMemAlloc(u4BufLen, VIR_MEM_TYPE);
	if (!pRadioStat) {
		DBGLOG(REQ, ERROR, "%s kalMemAlloc pRadioStat failed\n", __func__);
		return -ENOMEM;
	}
	kalMemZero(pRadioStat, u4BufLen);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, u4BufLen);
	if (!skb) {
		DBGLOG(REQ, TRACE, "%s allocate skb failed:%x\n", __func__, i4Status);
		return -ENOMEM;
	}

	/*
	 * rStatus = kalIoctl(prGlueInfo,
	 * wlanoidQueryStatistics,
	 * &rRadioStat,
	 * sizeof(rRadioStat),
	 * TRUE,
	 * TRUE,
	 * TRUE,
	 * FALSE,
	 * &u4BufLen);
	 */
	/* only for test */
	pRadioStat->radio = 10;
	pRadioStat->on_time = 11;
	pRadioStat->tx_time = 12;
	pRadioStat->num_channels = 4;

	/*NLA_PUT(skb, LSTATS_ATTRIBUTE_STATS, u4BufLen, pRadioStat);*/
	if (unlikely(nla_put(skb, LSTATS_ATTRIBUTE_STATS, u4BufLen, pRadioStat) < 0))
		goto nla_put_failure;

	i4Status = cfg80211_vendor_cmd_reply(skb);
	kalMemFree(pRadioStat, VIR_MEM_TYPE, u4BufLen);
	return -1; /* not support LLS now*/
	/* return i4Status; */

nla_put_failure:
	kfree_skb(skb);
	return i4Status;
}

int mtk_cfg80211_vendor_set_rssi_monitoring(struct wiphy *wiphy, struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;

	INT_32 i4Status = -EINVAL;
	PARAM_RSSI_MONITOR_T rRSSIMonitor;
	struct nlattr *attr[WIFI_ATTRIBUTE_RSSI_MONITOR_START + 1];
	UINT_32 i = 0;

	ASSERT(wiphy);
	ASSERT(wdev);

	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	kalMemZero(&rRSSIMonitor, sizeof(PARAM_RSSI_MONITOR_T));
	if ((data == NULL) || !data_len)
		goto nla_put_failure;
	kalMemZero(attr, sizeof(struct nlattr *) * (WIFI_ATTRIBUTE_RSSI_MONITOR_START + 1));

	if (NLA_PARSE_NESTED(attr, WIFI_ATTRIBUTE_RSSI_MONITOR_START,
		(struct nlattr *)(data - NLA_HDRLEN), nla_parse_wifi_policy) < 0) {
		DBGLOG(REQ, ERROR, "%s nla_parse_nested failed\n", __func__);
		goto nla_put_failure;
	}

	for (i = WIFI_ATTRIBUTE_MAX_RSSI; i <= WIFI_ATTRIBUTE_RSSI_MONITOR_START; i++) {
		if (attr[i]) {
			switch (i) {
			case WIFI_ATTRIBUTE_MAX_RSSI:
				rRSSIMonitor.max_rssi_value = nla_get_u32(attr[i]);
				break;
			case WIFI_ATTRIBUTE_MIN_RSSI:
				rRSSIMonitor.min_rssi_value = nla_get_u32(attr[i]);
				break;
			case WIFI_ATTRIBUTE_RSSI_MONITOR_START:
				rRSSIMonitor.enable = nla_get_u32(attr[i]);
				break;
			}
		}
	}

	DBGLOG(REQ, INFO, "mMax_rssi=%d, mMin_rssi=%d enable=%d\r\n",
	       rRSSIMonitor.max_rssi_value, rRSSIMonitor.min_rssi_value, rRSSIMonitor.enable);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidRssiMonitor,
			   &rRSSIMonitor, sizeof(PARAM_RSSI_MONITOR_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	return rStatus;

nla_put_failure:
	return i4Status;
}

int mtk_cfg80211_vendor_packet_keep_alive_start(struct wiphy *wiphy, struct wireless_dev *wdev,
						const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_16 u2IpPktLen = 0;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;

	INT_32 i4Status = -EINVAL;
	P_PARAM_PACKET_KEEPALIVE_T prPkt = NULL;
	struct nlattr *attr[MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC + 1];
	UINT_32 i = 0;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;

	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	prPkt = (P_PARAM_PACKET_KEEPALIVE_T) kalMemAlloc(sizeof(PARAM_PACKET_KEEPALIVE_T), VIR_MEM_TYPE);
	if (!prPkt) {
		DBGLOG(REQ, ERROR, "Can not alloc memory for PARAM_PACKET_KEEPALIVE_T\n");
		return -ENOMEM;
	}
	kalMemZero(prPkt, sizeof(PARAM_PACKET_KEEPALIVE_T));
	kalMemZero(attr, sizeof(struct nlattr *) * (MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC + 1));

	prPkt->enable = TRUE; /*start packet keep alive*/
	if (NLA_PARSE_NESTED(attr, MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC,
		(struct nlattr *)(data - NLA_HDRLEN), nla_parse_offloading_policy) < 0) {
		DBGLOG(REQ, ERROR, "%s nla_parse_nested failed\n", __func__);
		goto nla_put_failure;
	}

	for (i = MKEEP_ALIVE_ATTRIBUTE_ID; i <= MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC; i++) {
		if (attr[i]) {
			switch (i) {
			case MKEEP_ALIVE_ATTRIBUTE_ID:
				prPkt->index = nla_get_u8(attr[i]);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN:
				prPkt->u2IpPktLen = nla_get_u16(attr[i]);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_IP_PKT:
				u2IpPktLen = prPkt->u2IpPktLen <= 256 ? prPkt->u2IpPktLen : 256;
				kalMemCopy(prPkt->pIpPkt, nla_data(attr[i]), u2IpPktLen);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR:
				kalMemCopy(prPkt->ucSrcMacAddr, nla_data(attr[i]), sizeof(mac_addr));
				break;
			case MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR:
				kalMemCopy(prPkt->ucDstMacAddr, nla_data(attr[i]), sizeof(mac_addr));
				break;
			case MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC:
				prPkt->u4PeriodMsec = nla_get_u32(attr[i]);
				break;
			}
		}
	}

	DBGLOG(REQ, INFO, "enable=%d, index=%d, u2IpPktLen=%d u4PeriodMsec=%d\n",
		prPkt->enable, prPkt->index, prPkt->u2IpPktLen, prPkt->u4PeriodMsec);
	DBGLOG(REQ, TRACE, "prPkt->pIpPkt=0x%02x%02x%02x%02x, %02x%02x%02x%02x, %02x%02x%02x%02x, %02x%02x%02x%02x",
		prPkt->pIpPkt[0], prPkt->pIpPkt[1], prPkt->pIpPkt[2], prPkt->pIpPkt[3],
		prPkt->pIpPkt[4], prPkt->pIpPkt[5], prPkt->pIpPkt[6], prPkt->pIpPkt[7],
		prPkt->pIpPkt[8], prPkt->pIpPkt[9], prPkt->pIpPkt[10], prPkt->pIpPkt[11],
		prPkt->pIpPkt[12], prPkt->pIpPkt[13], prPkt->pIpPkt[14], prPkt->pIpPkt[15]);
	DBGLOG(REQ, TRACE, "prPkt->srcMAC=%02x:%02x:%02x:%02x:%02x:%02x, dstMAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
		prPkt->ucSrcMacAddr[0], prPkt->ucSrcMacAddr[1], prPkt->ucSrcMacAddr[2], prPkt->ucSrcMacAddr[3],
		prPkt->ucSrcMacAddr[4], prPkt->ucSrcMacAddr[5],
		prPkt->ucDstMacAddr[0], prPkt->ucDstMacAddr[1], prPkt->ucDstMacAddr[2], prPkt->ucDstMacAddr[3],
		prPkt->ucDstMacAddr[4], prPkt->ucDstMacAddr[5]);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidPacketKeepAlive,
			   prPkt, sizeof(PARAM_PACKET_KEEPALIVE_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	kalMemFree(prPkt, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return rStatus;

nla_put_failure:
	if (prPkt != NULL)
		kalMemFree(prPkt, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return i4Status;
}

int mtk_cfg80211_vendor_packet_keep_alive_stop(struct wiphy *wiphy, struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;

	INT_32 i4Status = -EINVAL;
	P_PARAM_PACKET_KEEPALIVE_T prPkt = NULL;
	struct nlattr *attr;

	ASSERT(wiphy);
	ASSERT(wdev);
	if ((data == NULL) || !data_len)
		goto nla_put_failure;

	DBGLOG(REQ, TRACE, "vendor command: data_len=%d\r\n", data_len);
	prPkt = (P_PARAM_PACKET_KEEPALIVE_T) kalMemAlloc(sizeof(PARAM_PACKET_KEEPALIVE_T), VIR_MEM_TYPE);
	if (!prPkt) {
		DBGLOG(REQ, ERROR, "Can not alloc memory for PARAM_PACKET_KEEPALIVE_T\n");
		return -ENOMEM;
	}
	kalMemZero(prPkt, sizeof(PARAM_PACKET_KEEPALIVE_T));

	prPkt->enable = FALSE;  /*stop packet keep alive*/
	attr = (struct nlattr *)data;
	if (attr->nla_type == MKEEP_ALIVE_ATTRIBUTE_ID)
		prPkt->index = nla_get_u8(attr);

	DBGLOG(REQ, INFO, "enable=%d, index=%d\r\n", prPkt->enable, prPkt->index);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidPacketKeepAlive,
			   prPkt, sizeof(PARAM_PACKET_KEEPALIVE_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	kalMemFree(prPkt, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return rStatus;

nla_put_failure:
	if (prPkt != NULL)
		kalMemFree(prPkt, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return i4Status;
}

int mtk_cfg80211_vendor_get_version(struct wiphy *wiphy, struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	struct sk_buff *skb;
	struct nlattr *attrlist;
	char verInfoBuf[64];
	UINT_32 u4CopySize = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);
	ASSERT(wdev);

	if ((data == NULL) || !data_len)
		return -ENOMEM;

	kalMemZero(verInfoBuf, 64);
	attrlist = (struct nlattr *)((UINT_8 *) data);
	if (attrlist->nla_type == LOGGER_ATTRIBUTE_DRIVER_VER) {
		char wifiDriverVersionStr[] = WIFI_MODULE "_" ANDROID_VER "_" RELEASE_DATE "_" SERIAL_NUMBER;
		UINT_32 u4StrSize = strlen(wifiDriverVersionStr);

		u4CopySize = (u4StrSize >= 64) ? 63 : u4StrSize;
		strncpy(verInfoBuf, wifiDriverVersionStr, u4CopySize);
	} else if (attrlist->nla_type == LOGGER_ATTRIBUTE_FW_VER) {
		WIFI_VER_INFO_T *prVerInfo;

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);
		prVerInfo = &(prGlueInfo->prAdapter->rVerInfo);
		sprintf(verInfoBuf, "%x.%x.%x", (prVerInfo->u2FwOwnVersion >> 8),
			(prVerInfo->u2FwOwnVersion & 0xff), (prVerInfo->u2FwOwnVersionExtend));
		u4CopySize = strlen(verInfoBuf);
	}

	if (u4CopySize <= 0)
		return -EFAULT;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, u4CopySize);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_nohdr(skb, u4CopySize, &verInfoBuf[0]) < 0))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EFAULT;
}

#if CFG_SUPPORT_GSCN
int mtk_cfg80211_vendor_event_complete_scan(struct wiphy *wiphy, struct wireless_dev *wdev, WIFI_SCAN_EVENT complete)
{
	struct sk_buff *skb;
	WIFI_SCAN_EVENT complete_scan;

	ASSERT(wiphy);
	ASSERT(wdev);

	DBGLOG(REQ, INFO, "vendor command complete=%d\r\n", complete);

	skb = cfg80211_vendor_event_alloc(wiphy, wdev, sizeof(complete), GSCAN_EVENT_COMPLETE_SCAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}
	complete_scan = WIFI_SCAN_RESULTS_AVAILABLE;
	/*NLA_PUT_U32(skb, GSCAN_EVENT_COMPLETE_SCAN, complete);*/
	{
		unsigned int __tmp = complete;

		if (unlikely(nla_put(skb, GSCAN_EVENT_COMPLETE_SCAN,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int mtk_cfg80211_vendor_event_scan_results_available(struct wiphy *wiphy, struct wireless_dev *wdev, UINT_32 num)
{
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(wdev);
	/* UINT_32 scan_result; */

	DBGLOG(REQ, INFO, "vendor command num=%d\r\n", num);

	skb = cfg80211_vendor_event_alloc(wiphy, wdev, sizeof(num), GSCAN_EVENT_SCAN_RESULTS_AVAILABLE, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}
	/* scan_result = 2; */
	/*NLA_PUT_U32(skb, GSCAN_EVENT_SCAN_RESULTS_AVAILABLE, num);*/
	{
		unsigned int __tmp = num;

		if (unlikely(nla_put(skb, GSCAN_EVENT_SCAN_RESULTS_AVAILABLE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int mtk_cfg80211_vendor_event_full_scan_results(struct wiphy *wiphy, struct wireless_dev *wdev,
						P_PARAM_WIFI_GSCAN_FULL_RESULT pdata, UINT_32 data_len)
{
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(wdev);
	ASSERT(pdata);
	DBGLOG(REQ, TRACE, "ssid=%s, bssid="MACSTR", rssi=%d, %d, capa=0x%x, ie_length=%d\n",
				HIDE(pdata->fixed.ssid),
				MAC2STR(pdata->fixed.bssid),
				pdata->fixed.rssi,
				pdata->fixed.channel,
				pdata->fixed.capability,
				pdata->ie_length);

	skb = cfg80211_vendor_event_alloc(wiphy, wdev, data_len, GSCAN_EVENT_FULL_SCAN_RESULTS, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	/* kalMemCopy(&full_result, pdata, sizeof(PARAM_WIFI_GSCAN_FULL_RESULT); */
	/*NLA_PUT(skb, GSCAN_EVENT_FULL_SCAN_RESULTS, sizeof(full_result), &full_result);*/
	if (unlikely(nla_put(skb, GSCAN_EVENT_FULL_SCAN_RESULTS,
		data_len, pdata) < 0))
		goto nla_put_failure;

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}
#endif

int mtk_cfg80211_vendor_event_significant_change_results(struct wiphy *wiphy, struct wireless_dev *wdev,
							 P_PARAM_WIFI_CHANGE_RESULT pdata, UINT_32 data_len)
{
	struct sk_buff *skb;
	PARAM_WIFI_CHANGE_RESULT result[2], *presult;

	ASSERT(wiphy);
	ASSERT(wdev);
	DBGLOG(REQ, TRACE, "vendor command\r\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	skb = cfg80211_vendor_event_alloc(wiphy, wdev, sizeof(PARAM_WIFI_CHANGE_RESULT),
					  GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_WIFI_CHANGE_RESULT),
					  GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS, GFP_KERNEL);
#endif

	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	presult = result;
	kalMemZero(presult, (sizeof(PARAM_WIFI_CHANGE_RESULT) * 2));
	/* only for test */
	kalMemCopy(presult->bssid, "213123", sizeof(mac_addr));
	presult->channel = 2437;
	presult->rssi[0] = -45;
	presult->rssi[1] = -46;
	presult++;
	presult->channel = 2439;
	presult->rssi[0] = -47;
	presult->rssi[1] = -48;
	/*NLA_PUT(skb, GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS, (sizeof(PARAM_WIFI_CHANGE_RESULT) * 2), result);*/
	if (unlikely(nla_put(skb, GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS,
		(sizeof(PARAM_WIFI_CHANGE_RESULT) * 2), result) < 0))
		goto nla_put_failure;

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int mtk_cfg80211_vendor_event_hotlist_ap_found(struct wiphy *wiphy, struct wireless_dev *wdev,
					       P_PARAM_WIFI_GSCAN_RESULT pdata, UINT_32 data_len)
{
	struct sk_buff *skb;
	PARAM_WIFI_GSCAN_RESULT result[2], *presult;

	ASSERT(wiphy);
	ASSERT(wdev);
	DBGLOG(REQ, TRACE, "vendor command\r\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	skb = cfg80211_vendor_event_alloc(wiphy, wdev, sizeof(PARAM_WIFI_GSCAN_RESULT),
					  GSCAN_EVENT_HOTLIST_RESULTS_FOUND, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_WIFI_GSCAN_RESULT),
					  GSCAN_EVENT_HOTLIST_RESULTS_FOUND, GFP_KERNEL);
#endif
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	presult = result;
	kalMemZero(presult, (sizeof(PARAM_WIFI_GSCAN_RESULT) * 2));
	/* only for test */
	kalMemCopy(presult->bssid, "123123", sizeof(mac_addr));
	presult->channel = 2441;
	presult->rssi = -45;
	presult++;
	presult->channel = 2443;
	presult->rssi = -47;
	/*NLA_PUT(skb, GSCAN_EVENT_HOTLIST_RESULTS_FOUND, (sizeof(PARAM_WIFI_GSCAN_RESULT) * 2), result);*/
	if (unlikely(nla_put(skb, GSCAN_EVENT_HOTLIST_RESULTS_FOUND,
		(sizeof(PARAM_WIFI_GSCAN_RESULT) * 2), result) < 0))
		goto nla_put_failure;

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int mtk_cfg80211_vendor_event_hotlist_ap_lost(struct wiphy *wiphy, struct wireless_dev *wdev,
					      P_PARAM_WIFI_GSCAN_RESULT pdata, UINT_32 data_len)
{
	struct sk_buff *skb;
	PARAM_WIFI_GSCAN_RESULT result[2], *presult;

	ASSERT(wiphy);
	ASSERT(wdev);
	DBGLOG(REQ, TRACE, "vendor command\r\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	skb = cfg80211_vendor_event_alloc(wiphy, wdev, sizeof(PARAM_WIFI_GSCAN_RESULT),
					  GSCAN_EVENT_HOTLIST_RESULTS_LOST, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_WIFI_GSCAN_RESULT),
					  GSCAN_EVENT_HOTLIST_RESULTS_LOST, GFP_KERNEL);
#endif
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	presult = result;
	kalMemZero(presult, (sizeof(PARAM_WIFI_GSCAN_RESULT) * 2));
	/* only for test */
	kalMemCopy(presult->bssid, "321321", sizeof(mac_addr));
	presult->channel = 2445;
	presult->rssi = -46;
	presult++;
	presult->channel = 2447;
	presult->rssi = -48;
	/*NLA_PUT(skb, GSCAN_EVENT_HOTLIST_RESULTS_LOST, (sizeof(PARAM_WIFI_GSCAN_RESULT) * 2), result);*/
	if (unlikely(nla_put(skb, GSCAN_EVENT_HOTLIST_RESULTS_LOST,
		(sizeof(PARAM_WIFI_GSCAN_RESULT) * 2), result) < 0))
		goto nla_put_failure;

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int mtk_cfg80211_vendor_event_rssi_beyond_range(struct wiphy *wiphy, struct wireless_dev *wdev, INT_32 rssi)
{
	struct sk_buff *skb;
	PARAM_RSSI_MONITOR_EVENT rRSSIEvt;
	P_BSS_INFO_T prAisBssInfo;
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);
	ASSERT(wdev);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "vendor command rssi=%d\r\n", rssi);
	kalMemZero(&rRSSIEvt, sizeof(PARAM_RSSI_MONITOR_EVENT));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	skb = cfg80211_vendor_event_alloc(wiphy, wdev, sizeof(PARAM_RSSI_MONITOR_EVENT),
			WIFI_EVENT_RSSI_MONITOR, GFP_KERNEL);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_RSSI_MONITOR_EVENT),
					  WIFI_EVENT_RSSI_MONITOR, GFP_KERNEL);
#endif

	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	prAisBssInfo = &(prGlueInfo->prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	kalMemCopy(rRSSIEvt.BSSID, prAisBssInfo->aucBSSID, sizeof(UINT_8) * MAC_ADDR_LEN);

	rRSSIEvt.version = 1; /* RSSI_MONITOR_EVT_VERSION = 1 */
	if (rssi > PARAM_WHQL_RSSI_MAX_DBM)
		rssi = PARAM_WHQL_RSSI_MAX_DBM;
	else if (rssi < -127)
		rssi = -127;
	rRSSIEvt.rssi = (INT_8)rssi;
	DBGLOG(REQ, INFO, "RSSI Event: version=%d, rssi=%d, BSSID=" MACSTR "\r\n",
		rRSSIEvt.version, rRSSIEvt.rssi, MAC2STR(rRSSIEvt.BSSID));

	/*NLA_PUT_U32(skb, GOOGLE_RSSI_MONITOR_EVENT, rssi);*/
	{
		/* unsigned int __tmp = rssi; */

		if (unlikely(nla_put(skb, WIFI_EVENT_RSSI_MONITOR,
			sizeof(PARAM_RSSI_MONITOR_EVENT), &rRSSIEvt) < 0))
			goto nla_put_failure;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int mtk_cfg80211_vendor_set_band(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct nlattr *attr;
	UINT_8 setBand = 0;
	ENUM_BAND_T band;

	ASSERT(wiphy);
	ASSERT(wdev);

	DBGLOG(REQ, TRACE, "%s()\n", __func__);

	if ((data == NULL) || !data_len)
		goto nla_put_failure;

	DBGLOG(REQ, TRACE, "vendor command: data_len=%d, data=0x%x 0x%x\r\n",
		data_len, *((UINT_32 *) data), *((UINT_32 *) data + 1));

	attr = (struct nlattr *)data;
	setBand = nla_get_u32(attr);
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, INFO, "Vendor Set Band value=%d\r\n", setBand);

	if (setBand == QCA_SETBAND_5G)
		band = BAND_5G;
	else if (setBand == QCA_SETBAND_2G)
		band = BAND_2G4;
	else
		band = BAND_NULL;

	prGlueInfo->prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX] = band;
	return 0;

nla_put_failure:
	return -1;

}

int mtk_cfg80211_vendor_set_roaming_policy(struct wiphy *wiphy, struct wireless_dev *wdev,
					const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	struct nlattr *attr;
	UINT_32 setRoaming = 0;
	UINT_32 u4BufLen = 0;

	ASSERT(wiphy);
	ASSERT(wdev);

	DBGLOG(REQ, TRACE, "%s()\n", __func__);

	if ((data == NULL) || !data_len)
		goto nla_put_failure;

	attr = (struct nlattr *)data;
	setRoaming = nla_get_u32(attr);
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, INFO, "vendor command: data_len=%d, data=0x%x 0x%x, policy = %d\n",
		data_len, *((UINT_32 *) data), *((UINT_32 *) data + 1), setRoaming);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetDrvRoamingPolicy,
			   &setRoaming, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	return 0;

nla_put_failure:
	return -1;

}

int mtk_cfg80211_vendor_get_supported_feature_set(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	uint32_t u4FeatureSet = WIFI_HAL_FEATURE_SET;
	P_GLUE_INFO_T prGlueInfo;
	P_REG_INFO_T prRegInfo;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(wdev);
	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;
	prRegInfo = &prGlueInfo->rRegInfo;
	if (!prRegInfo)
		return -EFAULT;

	if (prRegInfo->ucSupport5GBand)
		u4FeatureSet |= WIFI_FEATURE_INFRA_5G;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u4FeatureSet));
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(
	    nla_put_nohdr(skb, sizeof(u4FeatureSet), &u4FeatureSet) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		goto nla_put_failure;
	}

	DBGLOG(REQ, INFO, "supported feature set=0x%x\n", u4FeatureSet);

	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EFAULT;
}

int mtk_cfg80211_vendor_set_tx_power_scenario(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo;
	struct PARAM_TX_POWER_LIMIT rTxPwrLimit;
	struct nlattr *attr;
	uint32_t u4Scenario;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(wdev);
	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;

	attr = (struct nlattr *)data;
	if (attr->nla_type == WIFI_ATTRIBUTE_TX_POWER_SCENARIO)
		u4Scenario = nla_get_u32(attr);
	else
		return -EINVAL;

	kalMemZero(&rTxPwrLimit, sizeof(struct PARAM_TX_POWER_LIMIT));
	rTxPwrLimit.iScenario = (u4Scenario == UINT_MAX) ?
				-1 : (int32_t)u4Scenario;

	DBGLOG(REQ, INFO,
		"iScenario=%d, u4Scenario=%u, UINT_MAX=%u, iftype=%d\n",
		rTxPwrLimit.iScenario, u4Scenario, UINT_MAX,
		wdev->iftype);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetTxPowerLimit,
			   (PVOID)&rTxPwrLimit,
			   sizeof(struct PARAM_TX_POWER_LIMIT),
			   FALSE, FALSE, TRUE, FALSE, NULL);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(rStatus));
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(
	    nla_put_nohdr(skb, sizeof(rStatus), &rStatus) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		goto errHandleLabel;
	}

	DBGLOG(REQ, INFO, "rStatus=0x%x\n", rStatus);

	return cfg80211_vendor_cmd_reply(skb);

errHandleLabel:
	kfree_skb(skb);
	return -EFAULT;
}

int mtk_cfg80211_vendor_acs(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	P_GLUE_INFO_T prGlueInfo;
	struct nlattr *tb[WIFI_VENDOR_ATTR_ACS_MAX + 1] = { 0 };
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	bool ht_enabled, ht40_enabled, vht_enabled;
	uint8_t ch_width = 0;
	enum P2P_VENDOR_ACS_HW_MODE hw_mode;
	uint8_t *ch_list = NULL;
	uint8_t ch_list_count = 0;
	uint8_t i;
	uint32_t msg_size;
	struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest;
	P_RF_CHANNEL_INFO_T prRfChannelInfo;
	struct sk_buff *reply_skb;

	if (!wiphy || !wdev || !data || !data_len) {
		DBGLOG(REQ, ERROR, "input data null.\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (wdev->iftype == NL80211_IFTYPE_AP)
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	else
		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	if (!prGlueInfo) {
		DBGLOG(REQ, ERROR, "get glue structure fail.\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (NLA_PARSE(tb, WIFI_VENDOR_ATTR_ACS_MAX, data, data_len,
			nla_get_acs_policy)) {
		DBGLOG(REQ, ERROR, "parse acs attr fail.\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (!tb[WIFI_VENDOR_ATTR_ACS_HW_MODE]) {
		DBGLOG(REQ, ERROR, "attr hw_mode failed.\n");
		rStatus = -EINVAL;
		goto exit;
	}
	hw_mode = nla_get_u8(tb[WIFI_VENDOR_ATTR_ACS_HW_MODE]);

	if (tb[WIFI_VENDOR_ATTR_ACS_HT_ENABLED])
		ht_enabled =
			nla_get_flag(tb[WIFI_VENDOR_ATTR_ACS_HT_ENABLED]);
	else
		ht_enabled = 0;

	if (tb[WIFI_VENDOR_ATTR_ACS_HT40_ENABLED])
		ht40_enabled =
			nla_get_flag(tb[WIFI_VENDOR_ATTR_ACS_HT40_ENABLED]);
	else
		ht40_enabled = 0;

	if (tb[WIFI_VENDOR_ATTR_ACS_VHT_ENABLED])
		vht_enabled =
			nla_get_flag(tb[WIFI_VENDOR_ATTR_ACS_VHT_ENABLED]);
	else
		vht_enabled = 0;

	if (tb[WIFI_VENDOR_ATTR_ACS_CHWIDTH])
		ch_width = nla_get_u16(tb[WIFI_VENDOR_ATTR_ACS_CHWIDTH]);

	if (tb[WIFI_VENDOR_ATTR_ACS_CH_LIST]) {
		char *tmp = nla_data(tb[WIFI_VENDOR_ATTR_ACS_CH_LIST]);

		ch_list_count = nla_len(tb[WIFI_VENDOR_ATTR_ACS_CH_LIST]);
		if (ch_list_count) {
			ch_list = kalMemAlloc(sizeof(uint8_t) * ch_list_count,
					VIR_MEM_TYPE);
			if (ch_list == NULL) {
				DBGLOG(REQ, ERROR, "allocate ch_list fail.\n");
				rStatus = -ENOMEM;
				goto exit;
			}

			kalMemCopy(ch_list, tmp, ch_list_count);
		}
	} else if (tb[WIFI_VENDOR_ATTR_ACS_FREQ_LIST]) {
		uint32_t *freq =
			nla_data(tb[WIFI_VENDOR_ATTR_ACS_FREQ_LIST]);

		ch_list_count = nla_len(tb[WIFI_VENDOR_ATTR_ACS_FREQ_LIST]) /
				sizeof(uint32_t);
		if (ch_list_count) {
			ch_list = kalMemAlloc(sizeof(uint8_t) * ch_list_count,
					VIR_MEM_TYPE);
			if (ch_list == NULL) {
				DBGLOG(REQ, ERROR, "allocate ch_list fail.\n");
				rStatus = -ENOMEM;
				goto exit;
			}

			for (i = 0; i < ch_list_count; i++)
				ch_list[i] =
					ieee80211_frequency_to_channel(freq[i]);
		}
	}

	if (!ch_list_count) {
		DBGLOG(REQ, ERROR, "channel list count can NOT be 0\n");
		rStatus = -EINVAL;
		goto exit;
	}

	msg_size = sizeof(struct MSG_P2P_ACS_REQUEST) +
			(ch_list_count * sizeof(RF_CHANNEL_INFO_T));

	prMsgAcsRequest = cnmMemAlloc(prGlueInfo->prAdapter,
			RAM_TYPE_MSG, msg_size);

	if (prMsgAcsRequest == NULL) {
		DBGLOG(REQ, ERROR, "allocate msg acs req. fail.\n");
		rStatus = -ENOMEM;
		goto exit;
	}

	kalMemSet(prMsgAcsRequest, 0, msg_size);
	prMsgAcsRequest->rMsgHdr.eMsgId = MID_MNY_P2P_ACS;
	prMsgAcsRequest->fgIsHtEnable = ht_enabled;
	prMsgAcsRequest->fgIsHt40Enable = ht40_enabled;
	prMsgAcsRequest->fgIsVhtEnable = vht_enabled;
	switch (ch_width) {
	case 20:
		prMsgAcsRequest->eChnlBw = MAX_BW_20MHZ;
		break;
	case 40:
		prMsgAcsRequest->eChnlBw = MAX_BW_40MHZ;
		break;
	case 80:
		prMsgAcsRequest->eChnlBw = MAX_BW_80MHZ;
		break;
	case 160:
		prMsgAcsRequest->eChnlBw = MAX_BW_160MHZ;
		break;
	default:
		DBGLOG(REQ, ERROR, "unsupport width: %d.\n", ch_width);
		prMsgAcsRequest->eChnlBw = MAX_BW_UNKNOWN;
		break;
	}
	prMsgAcsRequest->eHwMode = hw_mode;
	prMsgAcsRequest->u4NumChannel = ch_list_count;

	for (i = 0; i < ch_list_count; i++) {
		/* Translate Freq from MHz to channel number. */
		prRfChannelInfo =
			&(prMsgAcsRequest->arChannelListInfo[i]);

		prRfChannelInfo->ucChannelNum = ch_list[i];

		if (prRfChannelInfo->ucChannelNum <= 14)
			prRfChannelInfo->eBand = BAND_2G4;
		else
			prRfChannelInfo->eBand = BAND_5G;

		/* Iteration. */
		prRfChannelInfo++;
	}

	mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(P_MSG_HDR_T) prMsgAcsRequest,
			MSG_SEND_METHOD_BUF);

exit:
	if (ch_list)
		kalMemFree(ch_list, VIR_MEM_TYPE,
				sizeof(uint8_t) * ch_list_count);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
				NLMSG_HDRLEN);
		if (reply_skb != NULL)
			return cfg80211_vendor_cmd_reply(reply_skb);
	}
	return rStatus;
}

int mtk_cfg80211_vendor_get_features(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	struct sk_buff *reply_skb;
	uint8_t feature_flags[(NUM_VENDOR_FEATURES + 7) / 8] = {0};
	uint8_t i;

	if (!wiphy || !wdev) {
		DBGLOG(REQ, ERROR, "input data null.\n");
		return -EINVAL;
	}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
	feature_flags[(VENDOR_FEATURE_SUPPORT_HW_MODE_ANY / 8)] |=
			(1 << (VENDOR_FEATURE_SUPPORT_HW_MODE_ANY % 8));
#endif

	for (i = 0; i < ((NUM_VENDOR_FEATURES + 7) / 8); i++) {
		DBGLOG(REQ, INFO, "Dump feature flags[%d]=0x%x.\n", i,
				feature_flags[i]);
	}

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			sizeof(feature_flags) + NLMSG_HDRLEN);

	if (!reply_skb)
		goto nla_put_failure;

	if (nla_put(reply_skb, WIFI_VENDOR_ATTR_FEATURE_FLAGS,
			sizeof(feature_flags), feature_flags))
		goto nla_put_failure;

	return cfg80211_vendor_cmd_reply(reply_skb);

nla_put_failure:
	kfree_skb(reply_skb);
	return -EINVAL;
}

