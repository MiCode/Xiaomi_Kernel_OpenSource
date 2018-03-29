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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

static struct nla_policy nla_parse_wifi_policy[WIFI_ATTRIBUTE_RSSI_MONITOR_START + 1] = {
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
};

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

static struct nla_policy nla_parse_offloading_policy[MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC + 1] = {
	[MKEEP_ALIVE_ATTRIBUTE_ID] = {.type = NLA_U8},
	[MKEEP_ALIVE_ATTRIBUTE_IP_PKT] = {.type = NLA_UNSPEC},
	[MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN] = {.type = NLA_U16},
	[MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR] = {.type = NLA_UNSPEC},
	[MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR] = {.type = NLA_UNSPEC},
	[MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC] = {.type = NLA_U32},
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
int mtk_cfg80211_NLA_PUT(struct sk_buff *skb, int attrtype, int attrlen, const void *data)
{
	if (unlikely(nla_put(skb, attrtype, attrlen, data) < 0))
		return 0;
	return 1;
}

int mtk_cfg80211_nla_put_type(struct sk_buff *skb, ENUM_NLA_PUT_DATE_TYPE type, int attrtype, const void *value)
{
	u8 u8data = 0;
	u16 u16data = 0;
	u32 u32data = 0;
	u64 u64data = 0;

	switch (type) {
	case NLA_PUT_DATE_U8:
		u8data = *(u8 *)value;
		return mtk_cfg80211_NLA_PUT(skb, attrtype, sizeof(u8), &u8data);
	case NLA_PUT_DATE_U16:
		u16data = *(u16 *)value;
		return mtk_cfg80211_NLA_PUT(skb, attrtype, sizeof(u16), &u16data);
	case NLA_PUT_DATE_U32:
		u32data = *(u32 *)value;
		return mtk_cfg80211_NLA_PUT(skb, attrtype, sizeof(u32), &u32data);
	case NLA_PUT_DATE_U64:
		u64data = *(u64 *)value;
		return mtk_cfg80211_NLA_PUT(skb, attrtype, sizeof(u64), &u64data);
	default:
		break;
	}
	return 0;
}

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

	rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode, country, 2, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Set country code error: %x\n", rStatus);
		return -EFAULT;
	}

	return 0;
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
	/*NLA_PUT(skb, GSCAN_ATTRIBUTE_CAPABILITIES, sizeof(rGscanCapabilities), &rGscanCapabilities);*/
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

	nla_parse_nested(attr, GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD, (struct nlattr *)(data - NLA_HDRLEN),
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
				prWifiScanCmd->num_buckets = nla_get_u32(attr[k]);
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
		if (nla_parse_nested(attr, GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD, (struct nlattr *)pbucket,
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
			   prWifiScanCmd, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS), FALSE, FALSE, TRUE, &u4BufLen);
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

	if (nla_parse_nested(attr, GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE,
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
			   prWifiScanCmd, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS), FALSE, FALSE, TRUE, &u4BufLen);
	kalMemFree(prWifiScanCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	return 0;

nla_put_failure:
	if (prWifiScanCmd != NULL)
		kalMemFree(prWifiScanCmd, VIR_MEM_TYPE, sizeof(PARAM_WIFI_GSCAN_CMD_PARAMS));
	return i4Status;
}
#endif

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

	if (nla_parse_nested(attr, GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH,
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
		if (nla_parse_nested(attr, GSCAN_ATTRIBUTE_RSSI_HIGH, (struct nlattr *)paplist,
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

	if (nla_parse_nested(attr, GSCAN_ATTRIBUTE_NUM_AP, (struct nlattr *)(data - NLA_HDRLEN),
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
		if (nla_parse_nested(attr, GSCAN_ATTRIBUTE_RSSI_HIGH, (struct nlattr *)paplist,
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

#if CFG_SUPPORT_GSCN
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
			   sizeof(PARAM_WIFI_GSCAN_ACTION_CMD_PARAMS), FALSE, FALSE, TRUE, &u4BufLen);

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
	/*PARAM_WIFI_GSCAN_RESULT result[4], *pResult;
	struct sk_buff *skb;*/
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

	kalIoctl(prGlueInfo,
		wlanoidGetGSCNResult,
		&rGScanResultParm, sizeof(PARAM_WIFI_GSCAN_GET_RESULT_PARAMS), TRUE, TRUE, TRUE, &u4BufLen);

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
					prResults);*/
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
	rRttCapabilities.rtt_ftm_supported = 1;
	rRttCapabilities.lci_support = 1;
	rRttCapabilities.lcr_support = 1;
	rRttCapabilities.preamble_support = 0x07;
	rRttCapabilities.bw_support = 0x1c;

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

	/*rStatus = kalIoctl(prGlueInfo,
	   wlanoidQueryStatistics,
	   &rRadioStat,
	   sizeof(rRadioStat),
	   TRUE,
	   TRUE,
	   TRUE,
	   FALSE,
	   &u4BufLen); */
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

	if (nla_parse_nested(attr, WIFI_ATTRIBUTE_RSSI_MONITOR_START,
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
			   &rRSSIMonitor, sizeof(PARAM_RSSI_MONITOR_T), FALSE, FALSE, TRUE, &u4BufLen);
	return rStatus;

nla_put_failure:
	return i4Status;
}

int mtk_cfg80211_vendor_packet_keep_alive_start(struct wiphy *wiphy, struct wireless_dev *wdev,
						const void *data, int data_len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
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
	if (nla_parse_nested(attr, MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC,
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
				kalMemCopy(prPkt->pIpPkt, nla_data(attr[i]), prPkt->u2IpPktLen);
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
			   prPkt, sizeof(PARAM_PACKET_KEEPALIVE_T), FALSE, FALSE, TRUE, &u4BufLen);
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
			   prPkt, sizeof(PARAM_PACKET_KEEPALIVE_T), FALSE, FALSE, TRUE, &u4BufLen);
	kalMemFree(prPkt, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return rStatus;

nla_put_failure:
	if (prPkt != NULL)
		kalMemFree(prPkt, VIR_MEM_TYPE, sizeof(PARAM_PACKET_KEEPALIVE_T));
	return i4Status;
}

#if CFG_SUPPORT_GSCN
int mtk_cfg80211_vendor_event_complete_scan(struct wiphy *wiphy, struct wireless_dev *wdev, WIFI_SCAN_EVENT complete)
{
	struct sk_buff *skb;
	WIFI_SCAN_EVENT complete_scan;

	ASSERT(wiphy);
	ASSERT(wdev);

	DBGLOG(REQ, INFO, "vendor command complete=%d\r\n", complete);

	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(complete), GSCAN_EVENT_COMPLETE_SCAN, GFP_KERNEL);
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

	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(num), GSCAN_EVENT_SCAN_RESULTS_AVAILABLE, GFP_KERNEL);
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
				pdata->fixed.ssid,
				MAC2STR(pdata->fixed.bssid),
				pdata->fixed.rssi,
				pdata->fixed.channel,
				pdata->fixed.capability,
				pdata->ie_length);

	skb = cfg80211_vendor_event_alloc(wiphy, data_len, GSCAN_EVENT_FULL_SCAN_RESULTS, GFP_KERNEL);
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

	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_WIFI_CHANGE_RESULT),
					  GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	presult = result;
	kalMemZero(presult, (sizeof(PARAM_WIFI_CHANGE_RESULT) * 2));
	/* only for test */
	kalMemCopy(presult->bssid, "213123", sizeof(mac_addr));
	presult->channel = 2437;
	presult->rssi[0] = -40;
	presult->rssi[1] = -50;
	presult++;
	presult->channel = 2412;
	presult->rssi[0] = -50;
	presult->rssi[1] = -60;
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

	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_WIFI_GSCAN_RESULT),
					  GSCAN_EVENT_HOTLIST_RESULTS_FOUND, GFP_KERNEL);
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

	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_WIFI_GSCAN_RESULT),
					  GSCAN_EVENT_HOTLIST_RESULTS_LOST, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	presult = result;
	kalMemZero(presult, (sizeof(PARAM_WIFI_GSCAN_RESULT) * 2));
	/* only for test */
	kalMemCopy(presult->bssid, "321321", sizeof(mac_addr));
	presult->channel = 2442;
	presult->rssi = -40;
	presult++;
	presult->channel = 2447;
	presult->rssi = -50;
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

	skb = cfg80211_vendor_event_alloc(wiphy, sizeof(PARAM_RSSI_MONITOR_EVENT),
					  WIFI_EVENT_RSSI_MONITOR, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	prAisBssInfo = &(prGlueInfo->prAdapter->rWifiVar.arBssInfoPool[NETWORK_TYPE_AIS]);
	kalMemCopy(rRSSIEvt.BSSID, prAisBssInfo->aucBSSID, sizeof(UINT_8) * MAC_ADDR_LEN);

	rRSSIEvt.version = 1; /* RSSI_MONITOR_EVT_VERSION = 1 */
	if (rssi > PARAM_WHQL_RSSI_MAX_DBM)
		rssi = PARAM_WHQL_RSSI_MAX_DBM;
	else if (rssi < -120)
		rssi = -120;
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

