/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: @(#) gl_cfg80211.c@@
*/

/*
 * ! \file   gl_cfg80211.c
 *  \brief  Main routines for supporintg MT6620 cfg80211 control interface
 *
 *  This file contains the support routines of Linux driver for MediaTek Inc. 802.11
 *  Wireless LAN Adapters.
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
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "precomp.h"
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>
#include "gl_cfg80211.h"

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

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for change STA type between
 *        1. Infrastructure Client (Non-AP STA)
 *        2. Ad-Hoc IBSS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
int
mtk_cfg80211_change_iface(struct wiphy *wiphy,
			  struct net_device *ndev, enum nl80211_iftype type, struct vif_params *params)
#else
int
mtk_cfg80211_change_iface(struct wiphy *wiphy,
			  struct net_device *ndev, enum nl80211_iftype type, u32 *flags, struct vif_params *params)
#endif
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	ENUM_PARAM_OP_MODE_T eOpMode;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if (type == NL80211_IFTYPE_STATION)
		eOpMode = NET_TYPE_INFRA;
	else if (type == NL80211_IFTYPE_ADHOC)
		eOpMode = NET_TYPE_IBSS;
	else
		return -EINVAL;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetInfrastructureMode, &eOpMode, sizeof(eOpMode), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN,
			"set infrastructure mode error:%x\n", rStatus);
	}

	/* reset wpa info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for adding key
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_add_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index, bool pairwise, const u8 *mac_addr, struct key_params *params)
{
	PARAM_KEY_T rKey;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Rslt = -EINVAL;
	UINT_32 u4BufLen = 0;
	UINT_8 tmp1[8], tmp2[8];

	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if DBG
	DBGLOG(RSN, TRACE, "mtk_cfg80211_add_key\n");
	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE, "keyIdx = %d pairwise = %d null mac\n", key_index, pairwise);
	}
	DBGLOG(RSN, TRACE, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, TRACE, params->key, params->key_len);
#endif

	kalMemZero(&rKey, sizeof(PARAM_KEY_T));

	rKey.u4KeyIndex = key_index;

	if (mac_addr) {
		if (EQUAL_MAC_ADDR(mac_addr, aucZeroMacAddr))
			COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
		else
			COPY_MAC_ADDR(rKey.arBSSID, mac_addr);

		if (pairwise) {
			/* if (!((rKey.arBSSID[0] */
			/*	& rKey.arBSSID[1] */
			/*	& rKey.arBSSID[2] */
			/*	& rKey.arBSSID[3] */
			/*	& rKey.arBSSID[4] */
			/*	& rKey.arBSSID[5]) == 0xFF)) { */
			/*	rKey.u4KeyIndex |= BIT(31); */
			/* } */
			rKey.u4KeyIndex |= BIT(31);
			rKey.u4KeyIndex |= BIT(30);
		}
	} else {		/* Group key */
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	if (params->key) {
		kalMemCopy(rKey.aucKeyMaterial, params->key, params->key_len);
		if (params->key_len == 32) {
			kalMemCopy(tmp1, &params->key[16], 8);
			kalMemCopy(tmp2, &params->key[24], 8);
			kalMemCopy(&rKey.aucKeyMaterial[16], tmp2, 8);
			kalMemCopy(&rKey.aucKeyMaterial[24], tmp1, 8);
		}
	}

	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = OFFSET_OF(PARAM_KEY_T, aucKeyMaterial) + rKey.u4KeyLength;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, &rKey, rKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting key for specified STA
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_get_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index,
		     bool pairwise,
		     const u8 *mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *))
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	/* not implemented */

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for removing key for specified STA
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_del_key(struct wiphy *wiphy, struct net_device *ndev, u8 key_index, bool pairwise, const u8 *mac_addr)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	PARAM_REMOVE_KEY_T rRemoveKey;
	UINT_32 u4BufLen = 0;
	INT_32 i4Rslt = -EINVAL;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if DBG
	DBGLOG(RSN, TRACE, "mtk_cfg80211_del_key\n");
	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE, "keyIdx = %d pairwise = %d null mac\n", key_index, pairwise);
	}
#endif

	kalMemZero(&rRemoveKey, sizeof(PARAM_REMOVE_KEY_T));
	if (mac_addr)
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
	rRemoveKey.u4KeyIndex = key_index;
	rRemoveKey.u4Length = sizeof(PARAM_REMOVE_KEY_T);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRemoveKey, &rRemoveKey, rRemoveKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		/* ToDo:: DBGLOG */
		DBGLOG(REQ, WARN, "remove key error:%x\n", rStatus);
	} else
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting default key on an interface
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *ndev, u8 key_index, bool unicast, bool multicast)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_DEFAULT_KEY_T rDefaultKey;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Rst = -EINVAL;
	UINT_32 u4BufLen = 0;
	BOOLEAN fgDef = FALSE, fgMgtDef = FALSE;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if DBG
	DBGLOG(RSN, TRACE, "mtk_cfg80211_set_default_key\n");
	DBGLOG(RSN, TRACE, "keyIdx = %d unicast = %d multicast = %d\n", key_index, unicast, multicast);
#endif

	rDefaultKey.ucKeyID = key_index;
	rDefaultKey.ucUnicast = unicast;
	rDefaultKey.ucMulticast = multicast;
	if (rDefaultKey.ucUnicast && !rDefaultKey.ucMulticast)
		return WLAN_STATUS_SUCCESS;

	if (rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgDef = TRUE;

	if (!rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgMgtDef = TRUE;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetDefaultKey,
			   &rDefaultKey, sizeof(PARAM_DEFAULT_KEY_T), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rst = 0;

	return i4Rst;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting station information such as RSSI
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_get_station(struct wiphy *wiphy, struct net_device *ndev, const u8 *mac, struct station_info *sinfo)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	PARAM_MAC_ADDRESS arBssid;
	UINT_32 u4BufLen, u4Rate;
	INT_32 i4Rssi;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	UINT_32 u4TotalError;
	struct net_device_stats *prDevStats;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check MAC address */
	/* Should be currently connected BSSID or device itself
	 * wificond will bring the MAC address of device itself to retrieve TX/RX statistics
	 */
	if (UNEQUAL_MAC_ADDR(arBssid, mac) && UNEQUAL_MAC_ADDR(ndev->dev_addr, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "Incorrect MAC addr[" MACSTR "], device[" MACSTR "], currently connected BSSID[" MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(ndev->dev_addr), MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill TX rate */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
		return 0;
	}
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryLinkSpeed, &u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE, &u4BufLen);


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
#else
		sinfo->filled |= STATION_INFO_TX_BITRATE;
#endif

	if ((rStatus != WLAN_STATUS_SUCCESS) || (u4Rate == 0)) {
		/*
		 *  DBGLOG(REQ, WARN, "unable to retrieve link speed\n"));
		 */
		DBGLOG(REQ, WARN, "last link speed\n");
		sinfo->txrate.legacy = prGlueInfo->u4LinkSpeedCache;
	} else {
		/*
		 *  sinfo->filled |= STATION_INFO_TX_BITRATE;
		 */
		sinfo->txrate.legacy = u4Rate / 1000;	/* convert from 100bps to 100kbps */
		prGlueInfo->u4LinkSpeedCache = u4Rate / 1000;
	}

	/* 3. fill RSSI */
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryRssi, &i4Rssi, sizeof(i4Rssi), TRUE, FALSE, FALSE, &u4BufLen);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	sinfo->filled |= BIT(NL80211_STA_INFO_SIGNAL);
#else
	sinfo->filled |= STATION_INFO_SIGNAL;
#endif

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "Query RSSI failed, use last RSSI %d\n", prGlueInfo->i4RssiCache);
		sinfo->signal = prGlueInfo->i4RssiCache ? prGlueInfo->i4RssiCache : PARAM_WHQL_RSSI_INITIAL_DBM;
	} else if (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM || i4Rssi == PARAM_WHQL_RSSI_MAX_DBM) {
		DBGLOG(REQ, WARN, "RSSI abnormal, use last RSSI %d\n", prGlueInfo->i4RssiCache);
		sinfo->signal = prGlueInfo->i4RssiCache ? prGlueInfo->i4RssiCache : i4Rssi;
	} else {
		sinfo->signal = i4Rssi;	/* dBm */
		prGlueInfo->i4RssiCache = i4Rssi;
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	if (prDevStats) {
		/* 4. fill RX_PACKETS */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_BYTES64);
#else
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->filled |= STATION_INFO_RX_BYTES64;
#endif
		sinfo->rx_packets = prDevStats->rx_packets;
		sinfo->rx_bytes = prDevStats->rx_bytes;

		/* 5. fill TX_PACKETS */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_BYTES64);
#else
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->filled |= STATION_INFO_TX_BYTES64;
#endif
		sinfo->tx_packets = prDevStats->tx_packets;
		sinfo->tx_bytes = prDevStats->tx_bytes;

		/* 6. fill TX_FAILED */
		kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, arBssid);
		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryStaStatistics,
				   &rQueryStaStatistics, sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "unable to retrieve link speed,status code = %d\n", rStatus);
		} else {
			DBGLOG(REQ, INFO,
				"BSSID: [" MACSTR "] TxFail:%u TxTimeOut:%u, TxOK:%u RxOK:%u\n",
				MAC2STR(arBssid), rQueryStaStatistics.u4TxFailCount,
				rQueryStaStatistics.u4TxLifeTimeoutCount,
				sinfo->tx_packets, sinfo->rx_packets);

			u4TotalError = rQueryStaStatistics.u4TxFailCount + rQueryStaStatistics.u4TxLifeTimeoutCount;
			prDevStats->tx_errors += u4TotalError;
		}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_FAILED);
#else
		sinfo->filled |= STATION_INFO_TX_FAILED;
#endif
		sinfo->tx_failed = prDevStats->tx_errors;
	}
#if (CFG_SUPPORT_DEBUG_STATISTICS == 1)
	wlanPktStatusDebugDumpInfo(prGlueInfo->prAdapter);
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting statistics for Link layer statistics
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*------------------------------------------------------------------------*/
int mtk_cfg80211_get_link_statistics(struct wiphy *wiphy, struct net_device *ndev, u8 *mac, struct station_info *sinfo)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	PARAM_MAC_ADDRESS arBssid;
	UINT_32 u4BufLen;
	INT_32 i4Rssi;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	PARAM_GET_BSS_STATISTICS rQueryBssStatistics;
	struct net_device_stats *prDevStats;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	UINT_8 ucBssIndex;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR "] currently connected BSSID[" MACSTR "]\n",
			MAC2STR(mac),
			MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill RSSI */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryRssi, &i4Rssi, sizeof(i4Rssi), TRUE, FALSE, FALSE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN, "unable to retrieve rssi\n");
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	/*3. get link layer statistics from Driver and FW */
	if (prDevStats) {
		/* 3.1 get per-STA link statistics */
		kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, arBssid);
		rQueryStaStatistics.ucLlsReadClear = FALSE;	/* dont clear for get BSS statistic */

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryStaStatistics,
				   &rQueryStaStatistics, sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN, "unable to retrieve per-STA link statistics\n");

		/*3.2 get per-BSS link statistics */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			/* get Bss Index from ndev */
			prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(ndev);
			ASSERT(prNetDevPrivate->prGlueInfo == prGlueInfo);
			ucBssIndex = prNetDevPrivate->ucBssIdx;

			kalMemZero(&rQueryBssStatistics, sizeof(rQueryBssStatistics));
			rQueryBssStatistics.ucBssIndex = ucBssIndex;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidQueryBssStatistics,
					   &rQueryBssStatistics,
					   sizeof(rQueryBssStatistics), TRUE, FALSE, TRUE, &u4BufLen);
		} else {
			DBGLOG(REQ, WARN, "unable to retrieve per-BSS link statistics\n");
		}

	}

	return 0;
}
/*----------------------------------------------------------------------------*/ /*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_get_channel_info(P_GLUE_INFO_T prGlueInfo, struct cfg80211_scan_request *request)
{
	struct ieee80211_channel *prchannelinfo;
	P_PARTIAL_SCAN_INFO prPartialScanChannel = NULL;
	ENUM_BAND_T eBand;
	UINT_8 ucChannel;
	P_ADAPTER_T prAdapter;
	UINT_8 i = 0;
	UINT_8 j = 0;
	UINT_8 channel_num = 0;
	UINT_8 channel_counts = 0;

	if (prGlueInfo == NULL)
		return 0;
	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return 0;

	prPartialScanChannel = (P_PARTIAL_SCAN_INFO)&prGlueInfo->rScanChannelInfo;


	if (cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel) == TRUE) {
		DBGLOG(REQ, TRACE, "partial scan not normal scan\n");
		return 0;
	}

	channel_counts = request->n_channels;
	DBGLOG(REQ, TRACE, "partial scan channel_counts=%d\n", channel_counts);
	/* 1. partial scan, max channels is 6
	* 2. some scan case, it will set band info to driver and not set channel info
	*    so, supplicant will set all channel with this scan
	*    for this case, partial scan must to check to avoid this scan
	*    channel num is MAXIMUM_OPERATION_CHANNEL_LIST
	* 3. some scan case, it will set band info to supplicant, not to driver,
	*    partial scan should handle this case.
	*    for this cse, band info include 2.4G and 5G band.
	*    channel num is channel num(2.4G or 5G band)
	*
	*    so, for partial scan limit channel num less MAXIMUM_OPERATION_CHANNEL_LIST
	*/
	if (channel_counts > 25)
		return 0;

	while (j < channel_counts) {

		prchannelinfo = request->channels[j];
		DBGLOG(REQ, TRACE, "partial scan set channel band=%d\n", prchannelinfo->band);

		switch (prchannelinfo->band) {
		case KAL_BAND_2GHZ:
			prPartialScanChannel->arChnlInfoList[i].eBand = BAND_2G4;
			break;

		case KAL_BAND_5GHZ:
			prPartialScanChannel->arChnlInfoList[i].eBand = BAND_5G;
			break;

		default:
			j++;
			continue;
		}

		DBGLOG(REQ, TRACE, "set channel channel_center_freq =%d\n",
			prchannelinfo->center_freq);

		channel_num = (UINT_8)nicFreq2ChannelNum(
			prchannelinfo->center_freq * 1000);

		DBGLOG(REQ, TRACE, "partial scan set channel channel_num=%d\n",
			channel_num);
		prPartialScanChannel->arChnlInfoList[i].ucChannelNum = channel_num;

		j++;
		i++;
	}
	DBGLOG(REQ, INFO, "partial scan set channel i=%d\n", i);
	if (i > 0) {
		prPartialScanChannel->ucChannelListNum = i;
		/*prGlueInfo->pucScanChannel = (PUINT_8)prPartialScanChannel;*/
		DBGLOG(REQ, TRACE, "partial scan set pucScanChannel\n");
		return 1;
	}

	kalMemSet(prPartialScanChannel, 0, sizeof(PARTIAL_SCAN_INFO));
	prGlueInfo->pucScanChannel = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to do a scan
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_scan(struct wiphy *wiphy,
		      struct cfg80211_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 i, u4BufLen;
	UINT_8 ucSetChannelFlag = 0;
	P_ADAPTER_T prAdapter = NULL;

	PARAM_SCAN_REQUEST_ADV_T rScanRequest;
	struct _PARAM_SCAN_RANDOM_MAC_ADDR_T *prScanRandMacAddr = NULL;
	UINT_32 num_ssid = 0, u4ValidIdx;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL)
		return -EBUSY;

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;

	if ((!prAdapter->fgEnCfg80211Scan) &&
		(kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED)) {
		DBGLOG(REQ, INFO, "mtk_cfg80211_scan LowLatency mode reject scan\n");
		return -EBUSY;
	}

	kalMemZero(&rScanRequest, sizeof(PARAM_SCAN_REQUEST_ADV_T));
	prScanRandMacAddr = &(rScanRequest.rScanRandomMacAddr);

	num_ssid = (UINT_32)request->n_ssids;
	DBGLOG(REQ, INFO, "request->n_ssids=%d", request->n_ssids);

	if (request->n_ssids == 0) {
		rScanRequest.u4SsidNum = 0;
	} else if (request->n_ssids <= (SCN_SSID_MAX_NUM + 1)) {
		u4ValidIdx = 0;
		for (i = 0; i < request->n_ssids; i++) {
			if ((request->ssids[i].ssid[0] == 0)
				|| (request->ssids[i].ssid_len == 0)) {
				num_ssid--; /* remove if this is a wildcard scan*/
				DBGLOG(REQ, INFO, "i=%d, num_ssid-- for wildcard scan\n", i);
				continue;
			}
			COPY_SSID(rScanRequest.rSsid[u4ValidIdx].aucSsid,
				rScanRequest.rSsid[u4ValidIdx].u4SsidLen,
				request->ssids[i].ssid, request->ssids[i].ssid_len);
			DBGLOG(REQ, INFO, "i=%d, u4ValidIdx=%d, aucSsid=%s, u4SsidLen=%d\n",
				i, u4ValidIdx,
				rScanRequest.rSsid[u4ValidIdx].aucSsid, rScanRequest.rSsid[u4ValidIdx].u4SsidLen);

			u4ValidIdx++;
			if (u4ValidIdx == SCN_SSID_MAX_NUM) {
				DBGLOG(REQ, INFO, "i=%d, u4ValidIdx is SCN_SSID_MAX_NUM\n", i);
				break;
			}
		}
		rScanRequest.u4SsidNum = u4ValidIdx; /* real SSID number to firmware */
	} else {
		DBGLOG(REQ, ERROR, "request->n_ssids:%d\n", request->n_ssids);
		return -EINVAL;
	}
	DBGLOG(REQ, INFO, "mtk_cfg80211_scan(), n_ssids=%d, num_ssid=%d\n", request->n_ssids, num_ssid);
	/*
	 *  Only request MAC address randomization when station is not associated.
	 *  FW SCAN Module will check whether station is not associated.
	 */
	if (kalScanParseRandomMac(prGlueInfo, request, prScanRandMacAddr->aucRandomMac))
		prScanRandMacAddr->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	if (request->ie_len > 0) {
		rScanRequest.u4IELength = request->ie_len;
		rScanRequest.pucIE = (PUINT_8) (request->ie);
	}

	/* temp get request ieee80211_channel info */
	if ((prGlueInfo->pucScanChannel == NULL) &&
		(prGlueInfo->rScanChannelInfo.ucChannelListNum == 0)) {
		DBGLOG(REQ, TRACE, "there is not any partial scan\n");
		if ((request->n_channels > 0) && (request->n_channels < MAXIMUM_OPERATION_CHANNEL_LIST)) {
			ucSetChannelFlag = mtk_cfg80211_get_channel_info(prGlueInfo, request);
			rScanRequest.ucSetChannel = ucSetChannelFlag;
			DBGLOG(REQ, TRACE, "partial scan get channel return %d\n", ucSetChannelFlag);
			}

	}

	prGlueInfo->prScanRequest = request;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssidListScanAdv,
			   &rScanRequest, sizeof(PARAM_SCAN_REQUEST_ADV_T), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		prGlueInfo->prScanRequest = NULL;
		DBGLOG(REQ, ERROR, "scan error:%x\n", rStatus);
		if (ucSetChannelFlag) {
			/*need reset pucScanChannel and memset rScanChannelInfo*/
			kalMemSet(&(prGlueInfo->rScanChannelInfo), 0, sizeof(PARTIAL_SCAN_INFO));
			prGlueInfo->pucScanChannel = NULL;

		}

		return -EINVAL;
	}

	return 0;
}

#if CFG_SUPPORT_ABORT_SCAN
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to abort a scan
 *
 * @param
 * struct wiphy *wiphy:wireless hardware description
 * struct wireless_dev *wdev:wireless device state
 *
 * @retval void
 */
/*----------------------------------------------------------------------------*/
void mtk_cfg80211_abort_scan(struct wiphy *wiphy,
		      struct wireless_dev *wdev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_32 u4BufLen = 0;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (prGlueInfo &&
	    prGlueInfo->ucAbortScanCnt < ABORT_SCAN_COUNT &&
	    prGlueInfo->u4LastNormalScanTime != 0 &&
	    !(CHECK_FOR_TIMEOUT(kalGetTimeTick(),
				prGlueInfo->u4LastNormalScanTime,
				SEC_TO_SYSTIME(ABORT_SCAN_INTERVAl)))) {

		rStatus = kalIoctl(prGlueInfo, wlanoidSetAbortScan, NULL, 0,
				   FALSE, FALSE, TRUE, &u4BufLen);
	}

	DBGLOG(REQ, INFO, "ABORT SCAN %s!\n", rStatus ? "Failed" : "Successed");
}
#endif

static UINT_8 wepBuf[48];

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to connect to
 *        the ESS with the specified parameters
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_connect(struct wiphy *wiphy, struct net_device *ndev, struct cfg80211_connect_params *sme)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;
	ENUM_PARAM_AUTH_MODE_T eAuthMode = AUTH_MODE_NUM;
	UINT_32 cipher;
	PARAM_CONNECT_T rNewSsid;
	ENUM_PARAM_OP_MODE_T eOpMode;
	UINT_32 i, u4AkmSuite = 0;
	P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
	struct wireless_dev *wdev = NULL;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if (ndev == NULL) {
		DBGLOG(REQ, ERROR, "ndev is NULL\n");
		return -EINVAL;
	}
	wdev = ndev->ieee80211_ptr;

	/* Supplicant requests connecting during driver do disconnecting,
	 * it will cause to install key fail, error is -67(link has been
	 * servered).
	 * Beacaus driver disconnected is done, but cfg80211 is disconnecting.
	 * Reject this request.Supplicant will issue the connecting request again.
	 */
	if (wdev->current_bss &&
		kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_DISCONNECTED) {
		DBGLOG(REQ, WARN, "Reject this connecting request\n");
		return -EALREADY;
	}

	prConnSettings = &prGlueInfo->prAdapter->rWifiVar.rConnSettings;

	if (prConnSettings->eOPMode > NET_TYPE_AUTO_SWITCH)
		eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		eOpMode = prConnSettings->eOPMode;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetInfrastructureMode, &eOpMode, sizeof(eOpMode), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "wlanoidSetInfrastructureMode fail 0x%x\n", rStatus);
		return -EFAULT;
	}

	/* after set operation mode, key table are cleared */

	/* reset wpa info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_REPLAY_DETECTION
	/* reset Detect replay information */
	prDetRplyInfo = &prGlueInfo->prDetRplyInfo;
	kalMemZero(prDetRplyInfo, sizeof(struct GL_DETECT_REPLAY_INFO));
#endif

#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
#endif

	DBGLOG(REQ, INFO,
	       "sme:Flag=%08x Type=%x Ver=%x Na=%d Nc=%d Pri=%d Len=%d\n",
	       sme->flags, sme->auth_type,
	       sme->crypto.wpa_versions,
	       sme->crypto.n_akm_suites,
	       sme->crypto.n_ciphers_pairwise,
	       sme->privacy, sme->ie_len);

	if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_1)
		prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_WPA;
	else if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_2)
		prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_WPA2;
	else
		prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

	switch (sme->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_SHARED_KEY;
		break;
	case NL80211_AUTHTYPE_FT:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_FT;
		break;
	case NL80211_AUTHTYPE_SAE:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_SAE;
		break;
	default:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM | IW_AUTH_ALG_SHARED_KEY;
		break;
	}

	if (sme->crypto.n_ciphers_pairwise) {
		DBGLOG(SCN, INFO, "[wlan] cipher pairwise (%x)\n", sme->crypto.ciphers_pairwise[0]);

		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[0] =
		    sme->crypto.ciphers_pairwise[0];
		switch (sme->crypto.ciphers_pairwise[0]) {
		case WLAN_CIPHER_SUITE_WEP40:
			prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_CCMP;
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher pairwise (%d)\n", sme->crypto.ciphers_pairwise[0]);
			return -EINVAL;
		}
	}

	if (sme->crypto.cipher_group) {
		prConnSettings->rRsnInfo.u4GroupKeyCipherSuite = sme->crypto.cipher_group;
		switch (sme->crypto.cipher_group) {
		case WLAN_CIPHER_SUITE_WEP40:
			prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_CCMP;
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher group (%d)\n", sme->crypto.cipher_group);
			return -EINVAL;
		}
	}
	/* DBGLOG(SCN, INFO, ("akm_suites=%x\n", sme->crypto.akm_suites[0])); */
	if (sme->crypto.n_akm_suites) {
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0] =
		    sme->crypto.akm_suites[0];
		if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA) {
			switch (sme->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				eAuthMode = AUTH_MODE_WPA;
				u4AkmSuite = WPA_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				eAuthMode = AUTH_MODE_WPA_PSK;
				u4AkmSuite = WPA_AKM_SUITE_PSK;
				break;
			default:
				DBGLOG(REQ, WARN, "invalid akm (%u)\n", sme->crypto.akm_suites[0]);
				return -EINVAL;
			}
		} else if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA2) {
			switch (sme->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				eAuthMode = AUTH_MODE_WPA2;
				u4AkmSuite = RSN_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_PSK;
				break;
#if CFG_SUPPORT_802_11R
			case WLAN_AKM_SUITE_FT_8021X:
				eAuthMode = AUTH_MODE_WPA2_FT;
				u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
				break;
			case WLAN_AKM_SUITE_FT_PSK:
				eAuthMode = AUTH_MODE_WPA2_FT_PSK;
				u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
				break;
#endif
#if CFG_SUPPORT_802_11W
				/* Notice:: Need kernel patch!! */
			case WLAN_AKM_SUITE_8021X_SHA256:
				eAuthMode = AUTH_MODE_WPA2;
				u4AkmSuite = RSN_AKM_SUITE_802_1X_SHA256;
				break;
			case WLAN_AKM_SUITE_PSK_SHA256:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_PSK_SHA256;
				break;
#endif
			case WLAN_AKM_SUITE_SAE:
				eAuthMode = AUTH_MODE_WPA3_SAE;
				u4AkmSuite = RSN_CIPHER_SUITE_SAE;
				break;
			case WLAN_AKM_SUITE_OWE:
				DBGLOG(REQ, WARN, "Akm Suite = OWE 0x000FAC12\n");
				eAuthMode = AUTH_MODE_WPA3_OWE;
				u4AkmSuite = RSN_CIPHER_SUITE_OWE;
				break;
			default:
				DBGLOG(REQ, WARN, "invalid wpa2 akm (%u)\n", sme->crypto.akm_suites[0]);
				return -EINVAL;
			}
		}
	}

	if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		switch (prGlueInfo->rWpaInfo.u4AuthAlg) {
		case IW_AUTH_ALG_OPEN_SYSTEM:
			eAuthMode = AUTH_MODE_OPEN;
			break;
		case IW_AUTH_ALG_FT:
			eAuthMode = AUTH_MODE_NON_RSN_FT;
			break;
		default:
			eAuthMode = AUTH_MODE_AUTO_SWITCH;
			break;
		}
	}

	prGlueInfo->rWpaInfo.fgPrivacyInvoke = sme->privacy;
	prGlueInfo->fgWpsActive = FALSE;

#if CFG_SUPPORT_PASSPOINT
	prGlueInfo->fgConnectHS20AP = FALSE;
#endif /* CFG_SUPPORT_PASSPOINT */
	prConnSettings->fgOkcEnabled = FALSE;
	prConnSettings->fgOkcPmksaReady = FALSE;
	if (sme->ie && sme->ie_len > 0) {
		WLAN_STATUS rStatus;
		UINT_32 u4BufLen;
		PUINT_8 prDesiredIE = NULL;
		PUINT_8 pucIEStart = (PUINT_8)sme->ie;

		/*We need to check the length of IEs*/
		if (sme->ie_len > MSDU_MAX_LENGTH) {
			DBGLOG(REQ, ERROR, "IE len exceeds limit %d\n", sme->ie_len);
			return -EINVAL;
		}
#if CFG_SUPPORT_WAPI
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetWapiAssocInfo, pucIEStart, sme->ie_len, FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, TRACE, "[wapi] set wapi assoc info error:%x\n", rStatus);
#endif
#if CFG_SUPPORT_WPS2
		if (wextSrchDesiredWPSIE(pucIEStart, sme->ie_len, 0xDD, (PUINT_8 *) &prDesiredIE)) {
			prGlueInfo->fgWpsActive = TRUE;
		}
#endif
#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "[HS20] set HS20 assoc info error:%x\n", rStatus);
#endif
		}
		if (wextSrchDesiredInterworkingIE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetInterworkingInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "[HS20] set Interworking assoc info error:%x\n", rStatus);
#endif
		}
		if (wextSrchDesiredRoamingConsortiumIE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRoamingConsortiumIEInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "[HS20] set RoamingConsortium assoc info error:%x\n", rStatus);
#endif
		}
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_802_11W
		if (wextSrchDesiredWPAIE(pucIEStart, sme->ie_len, 0x30,
					 (PUINT_8 *) &prDesiredIE)) {
			RSN_INFO_T rRsnInfo;

			if (rsnParseRsnIE(prGlueInfo->prAdapter,
			    (P_RSN_INFO_ELEM_T)prDesiredIE, &rRsnInfo)) {

				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
					prGlueInfo->rWpaInfo.ucRSNMfpCap =
							RSN_AUTH_MFP_OPTIONAL;
					if (rRsnInfo.u2RsnCap &
					    ELEM_WPA_CAP_MFPR)
						prGlueInfo->rWpaInfo.ucRSNMfpCap =
							RSN_AUTH_MFP_REQUIRED;
				} else
					prGlueInfo->rWpaInfo.ucRSNMfpCap =
							RSN_AUTH_MFP_DISABLED;
			}
		}
#endif

#if CFG_SUPPORT_OKC
		i = 0;
		wextSrchOkcAndPMKID(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE, (PUINT_8)&i);
		prConnSettings->fgOkcEnabled = !!i;
		if (prConnSettings->fgOkcEnabled) {
			UINT_16 u2PmkIdCnt = 0;
			PUINT_8 pucBssid = sme->bssid ? (PUINT_8)sme->bssid : (PUINT_8)sme->bssid_hint;

			if (prDesiredIE)
				u2PmkIdCnt = *(PUINT_16)prDesiredIE;
			DBGLOG(REQ, TRACE, "u2PmkIdCnt %d, has-bssid %d\n", u2PmkIdCnt, !!pucBssid);
			if (u2PmkIdCnt && pucBssid && !EQUAL_MAC_ADDR("\x0\x0\x0\x0\x0\x0", pucBssid) &&
				IS_UCAST_MAC_ADDR(pucBssid)) {
				PARAM_PMKID_T rPmkid;

				rPmkid.u4Length = (UINT_32)(sizeof(rPmkid) | (1 << 31));
				rPmkid.u4BSSIDInfoCount = 1;
				kalMemCopy(rPmkid.arBSSIDInfo[0].arBSSID, pucBssid, MAC_ADDR_LEN);
				kalMemCopy(rPmkid.arBSSIDInfo[0].arPMKID, prDesiredIE + 2, IW_PMKID_LEN);
				rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, (PVOID)&rPmkid,
						   sizeof(rPmkid), FALSE, FALSE, FALSE, &u4BufLen);
				if (rStatus != WLAN_STATUS_SUCCESS)
					DBGLOG(REQ, WARN, "failed to add OKC PMKID\n");
			}
		}
#endif
		/*We need to free buffer first*/
		if (prConnSettings->assocIeLen > 0) {
			kalMemFree(prConnSettings->pucAssocIEs, VIR_MEM_TYPE,
				   prConnSettings->assocIeLen);
			prConnSettings->assocIeLen = 0;
		}
		/*Do mem allocate*/
		if (prConnSettings->assocIeLen == 0) {
			prConnSettings->pucAssocIEs =
				kalMemAlloc(sme->ie_len, VIR_MEM_TYPE);
			prConnSettings->assocIeLen = sme->ie_len;
		}
		if (prConnSettings->pucAssocIEs)
			kalMemCopy(prConnSettings->pucAssocIEs,
				   sme->ie, prConnSettings->assocIeLen);
		else {
			DBGLOG(INIT, INFO,
			       "allocate memory for AssocIEs failed!\n");
			prConnSettings->assocIeLen = 0;
			return -ENOMEM;
		}
	}

#if CFG_SUPPORT_802_11W
		switch (sme->mfp) {
		case NL80211_MFP_NO:
			prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
			/* Change Mfp parameter from DISABLED to OPTIONAL
			* if upper layer set MFPC = 1 in RSNE
			* since upper layer can't bring MFP OPTIONAL information
			* to driver by sme->mfp
			*/
			if (prGlueInfo->rWpaInfo.ucRSNMfpCap == RSN_AUTH_MFP_OPTIONAL)
				prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_OPTIONAL;
			else if (prGlueInfo->rWpaInfo.ucRSNMfpCap ==
						RSN_AUTH_MFP_REQUIRED)
				DBGLOG(REQ, WARN,
					"mfp parameter(DISABLED) conflict with mfp cap(REQUIRED)\n");
			break;
		/* There's no OPTIONAL defination in both kernel and wpa_supplicant cfg80211 driver */
		case NL80211_MFP_REQUIRED:
			prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_REQUIRED;
			break;
		default:
			prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
			break;
		}
		/* DBGLOG(SCN, INFO, ("MFP=%d\n", prGlueInfo->rWpaInfo.u4Mfp)); */
#endif

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAuthMode, &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set auth mode error:%x\n", rStatus);

	/* Enable the specific AKM suite only. */
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prGlueInfo->prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[i];

		if (prEntry->dot11RSNAConfigAuthenticationSuite == u4AkmSuite)
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = TRUE;
		else
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = FALSE;
	}

	cipher = prGlueInfo->rWpaInfo.u4CipherGroup | prGlueInfo->rWpaInfo.u4CipherPairwise;

	if (prGlueInfo->rWpaInfo.fgPrivacyInvoke) {
		if (cipher & IW_AUTH_CIPHER_CCMP) {
			eEncStatus = ENUM_ENCRYPTION3_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_TKIP) {
			eEncStatus = ENUM_ENCRYPTION2_ENABLED;
		} else if (cipher & (IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40)) {
			eEncStatus = ENUM_ENCRYPTION1_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_NONE) {
			if (prGlueInfo->rWpaInfo.fgPrivacyInvoke)
				eEncStatus = ENUM_ENCRYPTION1_ENABLED;
			else
				eEncStatus = ENUM_ENCRYPTION_DISABLED;
		} else {
			eEncStatus = ENUM_ENCRYPTION_DISABLED;
		}
	} else {
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetEncryptionStatus, &eEncStatus, sizeof(eEncStatus), FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set encryption mode error:%x\n", rStatus);

	if (sme->key_len != 0 && prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		P_PARAM_WEP_T prWepKey = (P_PARAM_WEP_T) wepBuf;

		kalMemSet(prWepKey, 0, sizeof(PARAM_WEP_T));
		prWepKey->u4Length = 12 + sme->key_len;
		prWepKey->u4KeyLength = (UINT_32) sme->key_len;
		prWepKey->u4KeyIndex = (UINT_32) sme->key_idx;
		prWepKey->u4KeyIndex |= BIT(31);
		if (prWepKey->u4KeyLength > 32) {
			DBGLOG(REQ, WARN, "Too long key length (%u)\n", prWepKey->u4KeyLength);
			return -EINVAL;
		}
		kalMemCopy(prWepKey->aucKeyMaterial, sme->key, prWepKey->u4KeyLength);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetAddWep, prWepKey, prWepKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, ERROR, "wlanoidSetAddWep fail 0x%x\n", rStatus);
			return -EFAULT;
		}
	}

	rNewSsid.u4CenterFreq = sme->channel ? sme->channel->center_freq : 0;
	rNewSsid.pucBssid = (UINT_8 *)sme->bssid;
	rNewSsid.pucSsid = (UINT_8 *)sme->ssid;
	rNewSsid.u4SsidLen = sme->ssid_len;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetConnect, (PVOID)&rNewSsid, sizeof(PARAM_CONNECT_T),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set SSID:%x\n", rStatus);
		return -EINVAL;
	}
	return 0;
}

#if (defined(NL80211_ATTR_EXTERNAL_AUTH_SUPPORT) \
	|| LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0))
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for receiveing external auth result
 *
 * @param
 *
 * @retval always successful
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_external_auth(struct wiphy *wiphy,
			 struct net_device *ndev,
			 struct cfg80211_external_auth_params *params)
{
	struct _GLUE_INFO_T *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4BufLen;
	struct PARAM_EXTERNAL_AUTH auth;

	prGlueInfo = (struct _GLUE_INFO_T *) wiphy_priv(wiphy);
	if (!prGlueInfo)
		DBGLOG(REQ, WARN,
		       "SAE-confirm failed with invalid prGlueInfo\n");

	COPY_MAC_ADDR(auth.bssid, params->bssid);
	auth.status = params->status;
	auth.ucBssIdx = wlanGetBssIdx(ndev);
	rStatus = kalIoctl(prGlueInfo, wlanoidExternalAuthDone, (void *)&auth,
			   sizeof(auth), FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "SAE-confirm failed with: 0x%x\n", rStatus);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to disconnect from
 *        currently connected ESS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *ndev, u16 reason_code)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct wireless_dev *wdev = NULL;
	P_BSS_INFO_T prAisBssInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	if (ndev == NULL) {
		DBGLOG(REQ, ERROR, "ndev is NULL\n");
		return -EINVAL;
	}
	wdev = ndev->ieee80211_ptr;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if ((prGlueInfo == NULL) || (prGlueInfo->prAdapter == NULL)) {
		DBGLOG(REQ, ERROR,
			"mtk_cfg80211_disconnect, prGlueInfo ==NULL!!\n");
		return -EFAULT;
	}

	prAisBssInfo = (prGlueInfo->prAdapter)->prAisBssInfo;
	if (prAisBssInfo == NULL) {
		DBGLOG(REQ, ERROR,
			"mtk_cfg80211_disconnect, prAisBssInfo ==NULL!!\n");
		return -EFAULT;
	}

	DBGLOG(REQ, INFO, "Current_bss %d ssid_len %d StateIndicated %d %d\n",
	       (wdev->current_bss == NULL) ? 0 : 1, wdev->ssid_len,
	       prAisBssInfo->eConnectionStateIndicated,
	       prGlueInfo->eParamMediaStateIndicated);

	/* Check if kernel state = connected but driver state = disconnected.
	 * If yes, report disconnected to kernel directly since driver has
	 * already stayed in disconnected state.
	 */
	if ((wdev->current_bss && wdev->ssid_len > 0) &&
		prAisBssInfo->eConnectionStateIndicated
			== PARAM_MEDIA_STATE_DISCONNECTED) {
		DBGLOG(REQ, ERROR,
			"Kernel & driver have differnet conn state!!\n");
		cfg80211_disconnected(prGlueInfo->prDevHandler, reason_code,
				NULL, 0,
				WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY,
				GFP_KERNEL);
		return 0;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

#if CFG_SUPPORT_REPORT_MISC
	set_bit(EXT_SRC_DISCONNCT_BIT,
	&(prGlueInfo->prAdapter->rReportMiscSet.ulExtSrcFlag));

	kalSetReportMiscEvent(prGlueInfo);
#endif

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to join an IBSS group
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *ndev, struct cfg80211_ibss_params *params)
{
	PARAM_SSID_T rNewSsid;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4ChnlFreq;	/* Store channel or frequency information */
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* set channel */
	if (params->channel_fixed) {
		u4ChnlFreq = params->chandef.center_freq1;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetFrequency,
				   &u4ChnlFreq, sizeof(u4ChnlFreq), FALSE, FALSE, FALSE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	/* set SSID */
	kalMemCopy(rNewSsid.aucSsid, params->ssid, params->ssid_len);
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSsid, (PVOID)&rNewSsid, sizeof(PARAM_SSID_T), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set SSID:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to leave from IBSS group
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *ndev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to configure
 *        WLAN power managemenet
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *ndev, bool enabled, int timeout)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	PARAM_POWER_MODE_T rPowerMode;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;

	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;

	if (enabled) {
		if (timeout == -1)
			rPowerMode.ePowerMode = Param_PowerModeFast_PSP;
		else
			rPowerMode.ePowerMode = Param_PowerModeMAX_PSP;
	} else {
		rPowerMode.ePowerMode = Param_PowerModeCAM;
	}

	rPowerMode.ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSet802dot11PowerSaveProfile,
			   &rPowerMode, sizeof(PARAM_POWER_MODE_T), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set_power_mgmt error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cache
 *        a PMKID for a BSSID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	P_PARAM_PMKID_T prPmkid;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prPmkid = (P_PARAM_PMKID_T) kalMemAlloc(8 + sizeof(PARAM_BSSID_INFO_T), VIR_MEM_TYPE);
	if (!prPmkid) {
		DBGLOG(INIT, INFO, "Can not alloc memory for IW_PMKSA_ADD\n");
		return -ENOMEM;
	}

	prPmkid->u4Length = 8 + sizeof(PARAM_BSSID_INFO_T);
	prPmkid->u4BSSIDInfoCount = 1;
	kalMemCopy(prPmkid->arBSSIDInfo->arBSSID, pmksa->bssid, 6);
	kalMemCopy(prPmkid->arBSSIDInfo->arPMKID, pmksa->pmkid, IW_PMKID_LEN);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);
	kalMemFree(prPmkid, VIR_MEM_TYPE, 8 + sizeof(PARAM_BSSID_INFO_T));

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to remove
 *        a cached PMKID for a BSSID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to flush
 *        all cached PMKID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *ndev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	P_PARAM_PMKID_T prPmkid;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prPmkid = (P_PARAM_PMKID_T) kalMemAlloc(8, VIR_MEM_TYPE);
	if (!prPmkid) {
		DBGLOG(INIT, INFO, "Can not alloc memory for IW_PMKSA_FLUSH\n");
		return -ENOMEM;
	}

	prPmkid->u4Length = 8;
	prPmkid->u4BSSIDInfoCount = 0;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "flush pmkid error:%x\n", rStatus);
	kalMemFree(prPmkid, VIR_MEM_TYPE, 8);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting the rekey data
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_rekey_data(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_gtk_rekey_data *data)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	P_PARAM_GTK_REKEY_DATA prGtkData;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prGtkData = (P_PARAM_GTK_REKEY_DATA) kalMemAlloc(sizeof(PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);
	if (!prGtkData) {
		DBGLOG(INIT, INFO, "Can not alloc memory for PARAM_GTK_REKEY_DATA\n");
		return -ENOMEM;
	}

	DBGLOG(RSN, TRACE, "cfg80211_set_rekey_data!\n");

	kalMemCopy(prGtkData, data, sizeof(PARAM_GTK_REKEY_DATA));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetGtkRekeyData,
			   prGtkData, sizeof(PARAM_GTK_REKEY_DATA), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "set GTK rekey data error:%x\n", rStatus);
	kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));

	return 0;
}

void mtk_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
				      IN struct wireless_dev *wdev,
				      IN u16 frame_type, IN bool reg)
{
#if 0
	P_MSG_P2P_MGMT_FRAME_REGISTER_T prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) NULL;
#endif
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	do {

		DBGLOG(INIT, TRACE, "mtk_cfg80211_mgmt_frame_register\n");

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

		switch (frame_type) {
		case MAC_FRAME_PROBE_REQ:
			if (reg) {
				prGlueInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(INIT, TRACE, "Open packet filer probe request\n");
			} else {
				prGlueInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(INIT, TRACE, "Close packet filer probe request\n");
			}
			break;
		case MAC_FRAME_ACTION:
			if (reg) {
				prGlueInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(INIT, TRACE, "Open packet filer action frame.\n");
			} else {
				prGlueInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(INIT, TRACE, "Close packet filer action frame.\n");
			}
			break;
		default:
			DBGLOG(INIT, TRACE, "Ask frog to add code for mgmt:%x\n", frame_type);
			break;
		}

		if (prGlueInfo->prAdapter != NULL) {

			set_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT, &prGlueInfo->ulFlag);

			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);

			if (in_interrupt())
				DBGLOG(INIT, TRACE, "It is in interrupt level\n");
		}
#if 0

		prMgmtFrameRegister =
		    (P_MSG_P2P_MGMT_FRAME_REGISTER_T) cnmMemAlloc(prGlueInfo->prAdapter,
								  RAM_TYPE_MSG, sizeof(MSG_P2P_MGMT_FRAME_REGISTER_T));

		if (prMgmtFrameRegister == NULL) {
			ASSERT(FALSE);
			break;
		}

		prMgmtFrameRegister->rMsgHdr.eMsgId = MID_MNY_P2P_MGMT_FRAME_REGISTER;

		prMgmtFrameRegister->u2FrameType = frame_type;
		prMgmtFrameRegister->fgIsRegister = reg;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMgmtFrameRegister, MSG_SEND_METHOD_BUF);

#endif

	} while (FALSE);

}				/* mtk_cfg80211_mgmt_frame_register */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to stay on a
 *        specified channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_remain_on_channel(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   struct ieee80211_channel *chan,
				   unsigned int duration, u64 *cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_REMAIN_ON_CHANNEL_T prMsgChnlReq = (P_MSG_REMAIN_ON_CHANNEL_T) NULL;

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		    || (chan == NULL)
		    || (cookie == NULL)) {
			break;
		}

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

#if 1
		DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

		*cookie = prGlueInfo->u8Cookie++;

		prMsgChnlReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_REMAIN_ON_CHANNEL_T));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgChnlReq->rMsgHdr.eMsgId = MID_MNY_AIS_REMAIN_ON_CHANNEL;
		prMsgChnlReq->u8Cookie = *cookie;
		prMsgChnlReq->u4DurationMs = duration;

		prMsgChnlReq->ucChannelNum = nicFreq2ChannelNum(chan->center_freq * 1000);

		switch (chan->band) {
		case KAL_BAND_2GHZ:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		case KAL_BAND_5GHZ:
			prMsgChnlReq->eBand = BAND_5G;
			break;
		default:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		}

		prMsgChnlReq->eSco = CHNL_EXT_SCN;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlReq, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cancel staying
 *        on a specified channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  u64 cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_CANCEL_REMAIN_ON_CHANNEL_T prMsgChnlAbort = (P_MSG_CANCEL_REMAIN_ON_CHANNEL_T) NULL;

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		    ) {
			break;
		}

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		prMsgChnlAbort =
		    cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_CANCEL_REMAIN_ON_CHANNEL_T));

		if (prMsgChnlAbort == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgChnlAbort->rMsgHdr.eMsgId = MID_MNY_AIS_CANCEL_REMAIN_ON_CHANNEL;
		prMsgChnlAbort->u8Cookie = cookie;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlAbort, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to send a management frame
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			struct cfg80211_mgmt_tx_params *params,
			u64 *cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_MGMT_TX_REQUEST_T prMsgTxReq = (P_MSG_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucFrameBuf = (PUINT_8) NULL;

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (params == 0) || (cookie == NULL))
			break;

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		*cookie = prGlueInfo->u8Cookie++;

		DBGLOG(REQ, INFO, "%s\n", __func__);

		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_MGMT_TX_REQUEST_T));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgTxReq->fgNoneCckRate = FALSE;
		prMsgTxReq->fgIsWaitRsp = TRUE;

		prMgmtFrame = cnmMgtPktAlloc(prGlueInfo->prAdapter, (UINT_32) (params->len + MAC_TX_RESERVED_FIELD));
		prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
		if (prMsgTxReq->prMgmtMsduInfo == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgTxReq->u8Cookie = *cookie;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_AIS_MGMT_TX;

		pucFrameBuf = (PUINT_8) ((ULONG) prMgmtFrame->prPacket + MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, params->buf, params->len);

		prMgmtFrame->u2FrameLength = params->len;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgTxReq, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	if ((i4Rslt != 0) && (prMsgTxReq != NULL)) {
		if (prMsgTxReq->prMgmtMsduInfo != NULL)
			cnmMgtPktFree(prGlueInfo->prAdapter, prMsgTxReq->prMgmtMsduInfo);

		cnmMemFree(prGlueInfo->prAdapter, prMsgTxReq);
	}

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cancel the wait time
 *        from transmitting a management frame on another channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     u64 cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	/* not implemented */

	return -EINVAL;
}

#if CONFIG_NL80211_TESTMODE

#if CFG_SUPPORT_PASSPOINT
int mtk_cfg80211_testmode_hs20_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct wpa_driver_hs20_data_s *prParams = NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (data && len)
		prParams = (struct wpa_driver_hs20_data_s *)data;

	if (prParams) {
		int i;

		DBGLOG(INIT, INFO, "[%s] Cmd Type (%d)\n", __func__, prParams->CmdType);
		switch (prParams->CmdType) {
		case HS20_CMD_ID_SET_BSSID_POOL:
			DBGLOG(INIT, INFO,
				"[%s] fgBssidPoolIsEnable (%d)\n", __func__,
			       prParams->hs20_set_bssid_pool.fgBssidPoolIsEnable);
			DBGLOG(INIT, INFO,
				"[%s] ucNumBssidPool (%d)\n", __func__,
				prParams->hs20_set_bssid_pool.ucNumBssidPool);

			for (i = 0; i < prParams->hs20_set_bssid_pool.ucNumBssidPool; i++) {
				DBGLOG(INIT, INFO, "[%s][%d][" MACSTR "]\n", __func__, i,
				       MAC2STR(prParams->hs20_set_bssid_pool.arBssidPool[i]));
			}
			rstatus = kalIoctl(prGlueInfo,
					   (PFN_OID_HANDLER_FUNC) wlanoidSetHS20BssidPool,
					   &prParams->hs20_set_bssid_pool,
					   sizeof(struct param_hs20_set_bssid_pool), FALSE, FALSE, TRUE, &u4SetInfoLen);
			break;
		default:
			DBGLOG(INIT, INFO, "[%s] Unknown Cmd Type (%d)\n", __func__, prParams->CmdType);
			rstatus = WLAN_STATUS_FAILURE;

		}

	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_WAPI
int mtk_cfg80211_testmode_set_key_ext(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_SET_KEY_EXTS prParams = (P_NL80211_DRIVER_SET_KEY_EXTS) NULL;
	struct iw_encode_exts *prIWEncExt = (struct iw_encode_exts *)NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4BufLen = 0;

	P_PARAM_WPI_KEY_T prWpiKey = (P_PARAM_WPI_KEY_T) keyStructBuf;

	memset(keyStructBuf, 0, sizeof(keyStructBuf));

	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (data && len)
		prParams = (P_NL80211_DRIVER_SET_KEY_EXTS) data;

	if (prParams)
		prIWEncExt = (struct iw_encode_exts *)&prParams->ext;
	else
		return -EINVAL;

	if ((prIWEncExt != NULL) && (prIWEncExt->alg == IW_ENCODE_ALG_SMS4)) {
		/* KeyID */
		prWpiKey->ucKeyID = prParams->key_index;
		prWpiKey->ucKeyID--;
		if (prWpiKey->ucKeyID > 1) {
			/* key id is out of range */
			return -EINVAL;
		}

		if (prIWEncExt->key_len != 32) {
			/* key length not valid */
			return -EINVAL;
		}

		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			prWpiKey->eKeyType = ENUM_WPI_GROUP_KEY;
			prWpiKey->eDirection = ENUM_WPI_RX;
		} else if (prIWEncExt->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
			prWpiKey->eKeyType = ENUM_WPI_PAIRWISE_KEY;
			prWpiKey->eDirection = ENUM_WPI_RX_TX;
		}
/* #if CFG_SUPPORT_WAPI */
		/* handle_sec_msg_final(prIWEncExt->key, 32, prIWEncExt->key, NULL); */
/* #endif */
		/* PN */
		memcpy(prWpiKey->aucPN, prIWEncExt->tx_seq, IW_ENCODE_SEQ_MAX_SIZE);
		memcpy(prWpiKey->aucPN + IW_ENCODE_SEQ_MAX_SIZE, prIWEncExt->rx_seq, IW_ENCODE_SEQ_MAX_SIZE);

		/* BSSID */
		memcpy(prWpiKey->aucAddrIndex, prIWEncExt->addr, 6);

		memcpy(prWpiKey->aucWPIEK, prIWEncExt->key, 16);
		prWpiKey->u4LenWPIEK = 16;

		memcpy(prWpiKey->aucWPICK, &prIWEncExt->key[16], 16);
		prWpiKey->u4LenWPICK = 16;

		rstatus = kalIoctl(prGlueInfo,
				   wlanoidSetWapiKey, prWpiKey, sizeof(PARAM_WPI_KEY_T), FALSE, FALSE, TRUE, &u4BufLen);

		if (rstatus != WLAN_STATUS_SUCCESS)
			fgIsValid = -EFAULT;

	}
	return fgIsValid;
}
#endif

int
mtk_cfg80211_testmode_get_sta_statistics(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = -EINVAL;
	UINT_32 u4BufLen;
	UINT_32 u4LinkScore;
	UINT_32 u4TotalError;
	UINT_32 u4TxExceedThresholdCount;
	UINT_32 u4TxTotalCount;
	UINT_8 u1buf = 0;

	P_NL80211_DRIVER_GET_STA_STATISTICS_PARAMS prParams = NULL;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	if (data && len)
		prParams = (P_NL80211_DRIVER_GET_STA_STATISTICS_PARAMS) data;

	if (!prParams) {
		DBGLOG(QM, TRACE, "%s prParams is NULL\n", __func__);
		return -EINVAL;
	}

	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(PARAM_GET_STA_STA_STATISTICS) + 1);

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__, rStatus);
		return -ENOMEM;
	}
	DBGLOG(QM, TRACE, "Get [" MACSTR "] STA statistics\n",
		MAC2STR(prParams->aucMacAddr));

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
	COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, prParams->aucMacAddr);
	rQueryStaStatistics.ucReadClear = TRUE;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStaStatistics,
			   &rQueryStaStatistics, sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);

	/* Calcute Link Score */
	u4TxExceedThresholdCount = rQueryStaStatistics.u4TxExceedThresholdCount;
	u4TxTotalCount = rQueryStaStatistics.u4TxTotalCount;
	u4TotalError = rQueryStaStatistics.u4TxFailCount + rQueryStaStatistics.u4TxLifeTimeoutCount;

	/* u4LinkScore 10~100 , ExceedThreshold ratio 0~90 only */
	/* u4LinkScore 0~9    , Drop packet ratio 0~9 and all packets exceed threshold */
	if (u4TxTotalCount) {
		if (u4TxExceedThresholdCount <= u4TxTotalCount)
			u4LinkScore = (90 - ((u4TxExceedThresholdCount * 90) / u4TxTotalCount));
		else
			u4LinkScore = 0;
	} else {
		u4LinkScore = 90;
	}

	u4LinkScore += 10;

	if (u4LinkScore == 10) {

		if (u4TotalError <= u4TxTotalCount)
			u4LinkScore = (10 - ((u4TotalError * 10) / u4TxTotalCount));
		else
			u4LinkScore = 0;

	}

	if (u4LinkScore > 100)
		u4LinkScore = 100;

	do {
		u1buf = 0;

		if (!NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_INVALID, &u1buf))
			break;
		u1buf = NL80211_DRIVER_TESTMODE_VERSION;
		if (!NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_VERSION, &u1buf))
			break;
		if (!NLA_PUT(skb, NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN, prParams->aucMacAddr))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE, &u4LinkScore))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_FLAG, &rQueryStaStatistics.u4Flag))
			break;

		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_ENQUEUE, &rQueryStaStatistics.u4EnqueueCounter))
			break;

		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_DEQUEUE, &rQueryStaStatistics.u4DequeueCounter))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_STA_ENQUEUE,
				&rQueryStaStatistics.u4EnqueueStaCounter))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_STA_DEQUEUE,
				&rQueryStaStatistics.u4DequeueStaCounter))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_CNT,
				&rQueryStaStatistics.IsrCnt))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_PASS_CNT,
				&rQueryStaStatistics.IsrPassCnt))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_TASK_CNT,
				&rQueryStaStatistics.TaskIsrCnt))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_AB_CNT,
				&rQueryStaStatistics.IsrAbnormalCnt))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_SW_CNT,
				&rQueryStaStatistics.IsrSoftWareCnt))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_TX_CNT,
				&rQueryStaStatistics.IsrTxCnt))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_RX_CNT,
				&rQueryStaStatistics.IsrRxCnt))
			break;
		/* FW part STA link status */
		if (!NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_PER, &rQueryStaStatistics.ucPer))
			break;
		if (!NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_RSSI, &rQueryStaStatistics.ucRcpi))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_PHY_MODE, &rQueryStaStatistics.u4PhyMode))
			break;
		if (!NLA_PUT_U16(skb, NL80211_TESTMODE_STA_STATISTICS_TX_RATE, &rQueryStaStatistics.u2LinkSpeed))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT, &rQueryStaStatistics.u4TxFailCount))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT,
			&rQueryStaStatistics.u4TxLifeTimeoutCount))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME,
			&rQueryStaStatistics.u4TxAverageAirTime))
			break;
		/* Driver part link status */
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT, &rQueryStaStatistics.u4TxTotalCount))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT,
			&rQueryStaStatistics.u4TxExceedThresholdCount))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME,
			&rQueryStaStatistics.u4TxAverageProcessTime))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_MAX_PROCESS_TIME,
				&rQueryStaStatistics.u4TxMaxTime))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_HIF_PROCESS_TIME,
				&rQueryStaStatistics.u4TxAverageHifTime))
			break;
		if (!NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_MAX_HIF_PROCESS_TIME,
				&rQueryStaStatistics.u4TxMaxHifTime))
			break;
		/* Network counter */
		if (!NLA_PUT(skb,
		NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
			sizeof(rQueryStaStatistics.au4TcResourceEmptyCount),
			rQueryStaStatistics.au4TcResourceEmptyCount))
			break;
		if (!NLA_PUT(skb,
				NL80211_TESTMODE_STA_STATISTICS_NO_TC_ARRAY,
			sizeof(rQueryStaStatistics.au4DequeueNoTcResource),
			rQueryStaStatistics.au4DequeueNoTcResource))
			break;
		if (!NLA_PUT(skb,
				NL80211_TESTMODE_STA_STATISTICS_RB_ARRAY,
			sizeof(rQueryStaStatistics.au4TcResourceBackCount),
			rQueryStaStatistics.au4TcResourceBackCount))
			break;

		if (!NLA_PUT(skb,
				NL80211_TESTMODE_STA_STATISTICS_USED_TC_PGCT_ARRAY,
			sizeof(rQueryStaStatistics.au4TcResourceUsedPageCount),
			rQueryStaStatistics.au4TcResourceUsedPageCount))
			break;
		if (!NLA_PUT(skb,
				NL80211_TESTMODE_STA_STATISTICS_WANTED_TC_PGCT_ARRAY,
			sizeof(rQueryStaStatistics.au4TcResourceWantedPageCount),
			rQueryStaStatistics.au4TcResourceWantedPageCount))
			break;

		/* Sta queue length */
		if (!NLA_PUT(skb,
		NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,
			sizeof(rQueryStaStatistics.au4TcQueLen), rQueryStaStatistics.au4TcQueLen))
			break;
		/* Global QM counter */
		if (!NLA_PUT(skb,
		NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
			sizeof(rQueryStaStatistics.au4TcAverageQueLen), rQueryStaStatistics.au4TcAverageQueLen))
			break;
		if (!NLA_PUT(skb,
		NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,
			sizeof(rQueryStaStatistics.au4TcCurrentQueLen), rQueryStaStatistics.au4TcCurrentQueLen))
			break;
		/* Reserved field */
		if (!NLA_PUT(skb,
		NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,
			sizeof(rQueryStaStatistics.au4Reserved), rQueryStaStatistics.au4Reserved))
			break;
		i4Status = cfg80211_testmode_reply(skb);
	} while (0);

	return i4Status;
}

int
mtk_cfg80211_testmode_get_link_detection(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{

	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = -EINVAL;
	UINT_32 u4BufLen;
	UINT_8 u1buf = 0;

	PARAM_802_11_STATISTICS_STRUCT_T rStatistics;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(PARAM_GET_STA_STA_STATISTICS) + 1);

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__, rStatus);
		return -ENOMEM;
	}

	kalMemZero(&rStatistics, sizeof(rStatistics));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStatistics,
			   &rStatistics, sizeof(rStatistics), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "query statistics error:%x\n", rStatus);

	do {
		if (!NLA_PUT_U8(skb, NL80211_TESTMODE_LINK_INVALID, &u1buf))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_FAIL_CNT, &rStatistics.rFailedCount.QuadPart))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_RETRY_CNT, &rStatistics.rRetryCount.QuadPart))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_MULTI_RETRY_CNT,
				&rStatistics.rMultipleRetryCount.QuadPart))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_ACK_FAIL_CNT, &rStatistics.rACKFailureCount.QuadPart))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_FCS_ERR_CNT, &rStatistics.rFCSErrorCount.QuadPart))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_CNT, &rStatistics.rTransmittedFragmentCount.QuadPart))
			break;
		if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_RX_CNT, &rStatistics.rReceivedFragmentCount.QuadPart))
			break;

		i4Status = cfg80211_testmode_reply(skb);
	} while (0);

	return i4Status;
}

int mtk_cfg80211_testmode_sw_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_SW_CMD_PARAMS prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	if (data && len)
		prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) data;

	if (prParams) {
		if (prParams->set == 1) {
			rstatus = kalIoctl(prGlueInfo,
					   (PFN_OID_HANDLER_FUNC) wlanoidSetSwCtrlWrite,
					   &prParams->adr, (UINT_32) 8, FALSE, FALSE, TRUE, &u4SetInfoLen);
		}
	}
	DBGLOG(REQ, TRACE, "prParams=%p, status=%u\n", prParams, rstatus);

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

int mtk_cfg80211_testmode_cmd(IN struct wiphy *wiphy, IN struct wireless_dev *wdev, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_MODE_PARAMS prParams = NULL;
	INT_32 i4Status = -EINVAL;

	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	if (data && len)
		prParams = (P_NL80211_DRIVER_TEST_MODE_PARAMS) data;
	else {
		DBGLOG(REQ, ERROR, "data is NULL\n");
		return i4Status;
	}

	/* Clear the version byte */
	prParams->index = prParams->index & ~BITS(24, 31);

	switch (prParams->index) {
	case TESTMODE_CMD_ID_SW_CMD: /* SW cmd */
		i4Status = mtk_cfg80211_testmode_sw_cmd(wiphy, data, len);
		break;
#if CFG_SUPPORT_WAPI
	case TESTMODE_CMD_ID_WAPI: /* WAPI */
		i4Status = mtk_cfg80211_testmode_set_key_ext(wiphy, data, len);
		break;
#endif
	case 0x10:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
		break;
	case 0x20:
		i4Status = mtk_cfg80211_testmode_get_link_detection(wiphy, data, len, prGlueInfo);
		break;
#if CFG_SUPPORT_PASSPOINT
	case TESTMODE_CMD_ID_HS20:
		i4Status = mtk_cfg80211_testmode_hs20_cmd(wiphy, data, len);
		break;
#endif
	case TESTMODE_CMD_ID_STR_CMD:
		i4Status = mtk_cfg80211_process_str_cmd(prGlueInfo,
				(PUINT_8)(prParams+1), len - sizeof(*prParams));
		break;
	default:
		i4Status = -EINVAL;
		break;
	}
	if (i4Status != 0)
		DBGLOG(REQ, TRACE, "prParams->index=%d, status=%d\n", prParams->index, i4Status);

	return i4Status;
}
#endif

int
mtk_cfg80211_sched_scan_start(IN struct wiphy *wiphy,
			      IN struct net_device *ndev, IN struct cfg80211_sched_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 i, u4BufLen;
	P_PARAM_SCHED_SCAN_REQUEST prSchedScanRequest;
	P_SCAN_INFO_T prScanInfo;
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	UINT_32 num = 0;
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prSchedScanRequest != NULL) {
		DBGLOG(SCN, ERROR, "prGlueInfo->prSchedScanRequest != NULL\n");
		return -EBUSY;
	} else if (request == NULL || request->n_match_sets > CFG_SCAN_SSID_MATCH_MAX_NUM) {
		DBGLOG(SCN, ERROR, "(request == NULL || request->n_match_sets > CFG_SCAN_SSID_MATCH_MAX_NUM)\n");
		/* invalid scheduled scan request */
		return -EINVAL;
	} else if (!request->n_match_sets) {
		/* invalid scheduled scan request */
		DBGLOG(SCN, ERROR, "n_match_sets is 0\n");
		return -EINVAL;
	}
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	else if (!request->n_ssids || request->n_ssids > CFG_SCAN_HIDDEN_SSID_MAX_NUM) {
		/* invalid scheduled scan request */
		DBGLOG(SCN, ERROR, "invalid n_ssids=%d\n", request->n_ssids);
		return -EINVAL;
	}
#endif
	DBGLOG(REQ, INFO, "--> %s() n_ssid:%d , match_set:%d\n", __func__, request->n_ssids, request->n_match_sets);

	prSchedScanRequest = (P_PARAM_SCHED_SCAN_REQUEST) kalMemAlloc(sizeof(PARAM_SCHED_SCAN_REQUEST), VIR_MEM_TYPE);
	if (prSchedScanRequest == NULL) {
		DBGLOG(SCN, ERROR, "(prSchedScanRequest == NULL) kalMemAlloc fail\n");
		return -ENOMEM;
	}
	kalMemZero(prSchedScanRequest, sizeof(PARAM_SCHED_SCAN_REQUEST));

#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	/* passed in the probe_reqs in active scans */
	if (request->ssids) {
		for (i = 0; i < request->n_ssids; i++) {
			DBGLOG(SCN, TRACE, "ssids : (%d)[%s]\n", i, request->ssids[i].ssid);
			/* driver ignored the null ssid */
			if (request->ssids[i].ssid_len == 0 || request->ssids[i].ssid[0] == 0)
				DBGLOG(SCN, WARN, "ignore the null ssid, index:%d\n", i);
			else {
				COPY_SSID(prSchedScanRequest->arSsid[num].aucSsid,
					  prSchedScanRequest->arSsid[num].u4SsidLen,
					  request->ssids[i].ssid, request->ssids[i].ssid_len);
				num++;
			}
		}
	}
	prSchedScanRequest->u4SsidNum = num;
	num = 0;
	if (request->match_sets) {
		for (i = 0; i < request->n_match_sets; i++) {
			DBGLOG(SCN, TRACE, "match : (%d)[%s]\n", i, request->match_sets[i].ssid.ssid);
			/* driver ignored the null ssid */
			if (request->match_sets[i].ssid.ssid_len == 0
				|| request->match_sets[i].ssid.ssid[0] == 0)
				DBGLOG(SCN, WARN, "ignore the null ssid, index:%d\n", i);
			else {
				COPY_SSID(prSchedScanRequest->arMatchSsid[num].aucSsid,
					  prSchedScanRequest->arMatchSsid[num].u4SsidLen,
					  request->match_sets[i].ssid.ssid,
					  request->match_sets[i].ssid.ssid_len);
				prSchedScanRequest->acRssiThresold[i] = (INT_8)request->match_sets[i].rssi_thold;
				num++;
			}
		}
	}
	prSchedScanRequest->u4MatchSsidNum = num;
#else
	prSchedScanRequest->u4SsidNum = request->n_match_sets;
	for (i = 0; i < request->n_match_sets; i++) {
		if (request->match_sets == NULL || &(request->match_sets[i]) == NULL) {
			prSchedScanRequest->arSsid[i].u4SsidLen = 0;
		} else {
			COPY_SSID(prSchedScanRequest->arSsid[i].aucSsid,
				  prSchedScanRequest->arSsid[i].u4SsidLen,
				  request->match_sets[i].ssid.ssid, request->match_sets[i].ssid.ssid_len);
			prSchedScanRequest->acRssiThresold[i] = 0;/* (INT_8)request->match_sets[i].rssi_thold;*/
		}
	}
#endif

	prSchedScanRequest->u4IELength = request->ie_len;
	if (request->ie_len > 0) {
		prSchedScanRequest->pucIE = kalMemAlloc(request->ie_len, VIR_MEM_TYPE);
		if (prSchedScanRequest->pucIE == NULL) {
			DBGLOG(SCN, ERROR, "prSchedScanRequest->pucIE kalMemAlloc fail\n");
		} else {
			kalMemZero(prSchedScanRequest->pucIE, request->ie_len);
			kalMemCopy(prSchedScanRequest->pucIE, (PUINT_8)request->ie, request->ie_len);
		}
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	prSchedScanRequest->u2ScanInterval = (UINT_16) (request->scan_plans[0].interval);
#else
	prSchedScanRequest->u2ScanInterval = (UINT_16) (request->interval);
#endif
	prSchedScanRequest->ucChnlNum = (UINT_8)request->n_channels;
	prSchedScanRequest->pucChannels = kalMemAlloc(request->n_channels, VIR_MEM_TYPE);
	if (!prSchedScanRequest->pucChannels) {
		DBGLOG(SCN, ERROR, "prSchedScanRequest->pucChannels kalMemAlloc fail\n");
		prSchedScanRequest->ucChnlNum = 0;
	} else
		for (i = 0; i < request->n_channels; i++)
			prSchedScanRequest->pucChannels[i] =
				nicFreq2ChannelNum(request->channels[i]->center_freq * 1000);
	if (kalSchedScanParseRandomMac(ndev, request)) {
		prSchedScanRequest->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetStartSchedScan,
			   prSchedScanRequest, sizeof(PARAM_SCHED_SCAN_REQUEST), FALSE, FALSE, TRUE, &u4BufLen);

	kalMemFree(prSchedScanRequest->pucChannels, VIR_MEM_TYPE, request->n_channels);
	kalMemFree(prSchedScanRequest->pucIE, VIR_MEM_TYPE, request->ie_len);
	kalMemFree(prSchedScanRequest, VIR_MEM_TYPE, sizeof(PARAM_SCHED_SCAN_REQUEST));

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "scheduled scan error:%x\n", rStatus);
		prScanInfo = &(prGlueInfo->prAdapter->rWifiVar.rScanInfo);
		prScanInfo->fgNloScanning = FALSE;
		return -EINVAL;
	}

	prGlueInfo->prSchedScanRequest = request;

	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy, IN struct net_device *ndev)
#else
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy, IN struct net_device *ndev, IN UINT_64 reqid)
#endif
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prSchedScanRequest == NULL)
		return -EBUSY;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetStopSchedScan, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_FAILURE) {
		DBGLOG(REQ, WARN, "scheduled scan error:%x\n", rStatus);
		return -EINVAL;
	}

	if (prGlueInfo->prSchedScanRequest != NULL)
		prGlueInfo->prSchedScanRequest = NULL;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for handling association request
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_assoc(struct wiphy *wiphy, struct net_device *ndev, struct cfg80211_assoc_request *req)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MAC_ADDRESS arBssid;
#if CFG_SUPPORT_PASSPOINT
	PUINT_8 prDesiredIE = NULL;
#endif /* CFG_SUPPORT_PASSPOINT */
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, req->bss->bssid)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR "] currently connected BSSID[" MACSTR "]\n",
			MAC2STR(req->bss->bssid),
			MAC2STR(arBssid));
		return -ENOENT;
	}

	if (req->ie && req->ie_len > 0) {
#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE((PUINT_8) req->ie, req->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/*
				 * DBGLOG(REQ, TRACE,
				 *     ("[HS20] set HS20 assoc info error:%lx\n", rStatus));
				 */
			}
		}

		if (wextSrchDesiredInterworkingIE((PUINT_8) req->ie, req->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetInterworkingInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/*
				 * DBGLOG(REQ, TRACE,
				 *     ("[HS20] set Interworking assoc info error:%lx\n", rStatus));
				 */
			}
		}

		if (wextSrchDesiredRoamingConsortiumIE((PUINT_8) req->ie, req->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRoamingConsortiumIEInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/*
				 * DBGLOG(REQ, TRACE,
				 *     ("[HS20] set RoamingConsortium assoc info error:%lx\n", rStatus));
				 */
			}
		}
#endif /* CFG_SUPPORT_PASSPOINT */
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssid, (PVOID) req->bss->bssid, MAC_ADDR_LEN, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set BSSID:%x\n", rStatus);
		return -EINVAL;
	}

	return 0;
}

#if CFG_SUPPORT_NFC_BEAM_PLUS

int mtk_cfg80211_testmode_get_scan_done(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{
#define NL80211_TESTMODE_P2P_SCANDONE_INVALID 0
#define NL80211_TESTMODE_P2P_SCANDONE_STATUS 1

	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = -EINVAL, READY_TO_BEAM = 0;
	UINT_8 u1Buf = 0;

	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(UINT_32));
	/* READY_TO_BEAM = */
	/* (UINT_32)(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsGOInitialDone) */
	/* &(!prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsScanRequest); */
	READY_TO_BEAM = 1;
	/* DBGLOG(QM, TRACE, */
	/* ("NFC:GOInitialDone[%d] and P2PScanning[%d]\n", */
	/* prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsGOInitialDone, */
	/* prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsScanRequest)); */

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__, rStatus);
		return -ENOMEM;
	}

	if (!NLA_PUT_U8(skb, NL80211_TESTMODE_P2P_SCANDONE_INVALID, &u1Buf))
		return i4Status;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_P2P_SCANDONE_STATUS, &READY_TO_BEAM))
		return i4Status;

	i4Status = cfg80211_testmode_reply(skb);
	return i4Status;
}

#endif

#if CFG_SUPPORT_TDLS

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for changing a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_change_station(struct wiphy *wiphy, struct net_device *ndev, const u8 *mac,
				struct station_parameters *params)
{

	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	P_GLUE_INFO_T prGlueInfo = NULL;
	CMD_PEER_UPDATE_T rCmdUpdate;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen, u4Temp;
	ADAPTER_T *prAdapter;
	P_BSS_INFO_T prAisBssInfo;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;
	prAisBssInfo = prAdapter->prAisBssInfo;

	if (params == NULL)
		return 0;
	else if (params->supported_rates == NULL)
		return 0;

	/* init */
	kalMemZero(&rCmdUpdate, sizeof(rCmdUpdate));
	kalMemCopy(rCmdUpdate.aucPeerMac, mac, 6);

	if (params->supported_rates != NULL) {

		u4Temp = params->supported_rates_len;
		if (u4Temp > CMD_PEER_UPDATE_SUP_RATE_MAX)
			u4Temp = CMD_PEER_UPDATE_SUP_RATE_MAX;
		kalMemCopy(rCmdUpdate.aucSupRate, params->supported_rates, u4Temp);
		rCmdUpdate.u2SupRateLen = u4Temp;
	}

	/*
	 * In supplicant, only recognize WLAN_EID_QOS 46, not 0xDD WMM
	 * So force to support UAPSD here.
	 */
	rCmdUpdate.UapsdBitmap = 0x0F;	/*params->uapsd_queues; */
	rCmdUpdate.UapsdMaxSp = 0;	/*params->max_sp; */

	rCmdUpdate.u2Capability = params->capability;

	if (params->ext_capab != NULL) {

		u4Temp = params->ext_capab_len;
		if (u4Temp > CMD_PEER_UPDATE_EXT_CAP_MAXLEN)
			u4Temp = CMD_PEER_UPDATE_EXT_CAP_MAXLEN;
		kalMemCopy(rCmdUpdate.aucExtCap, params->ext_capab, u4Temp);
		rCmdUpdate.u2ExtCapLen = u4Temp;
	}

	if (params->ht_capa != NULL) {

		rCmdUpdate.rHtCap.u2CapInfo = params->ht_capa->cap_info;
		rCmdUpdate.rHtCap.ucAmpduParamsInfo = params->ht_capa->ampdu_params_info;
		rCmdUpdate.rHtCap.u2ExtHtCapInfo = params->ht_capa->extended_ht_cap_info;
		rCmdUpdate.rHtCap.u4TxBfCapInfo = params->ht_capa->tx_BF_cap_info;
		rCmdUpdate.rHtCap.ucAntennaSelInfo = params->ht_capa->antenna_selection_info;
		kalMemCopy(rCmdUpdate.rHtCap.rMCS.arRxMask,
			   params->ht_capa->mcs.rx_mask, sizeof(rCmdUpdate.rHtCap.rMCS.arRxMask));

		rCmdUpdate.rHtCap.rMCS.u2RxHighest = params->ht_capa->mcs.rx_highest;
		rCmdUpdate.rHtCap.rMCS.ucTxParams = params->ht_capa->mcs.tx_params;
		rCmdUpdate.fgIsSupHt = TRUE;
	}
	/* vht */

	if (params->vht_capa != NULL) {
		/* rCmdUpdate.rVHtCap */
		/* rCmdUpdate.rVHtCap */
	}

	/* update a TDLS peer record */
	/* sanity check */
	if ((params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER)))
		rCmdUpdate.eStaType = STA_TYPE_DLS_PEER;
	rStatus = kalIoctl(prGlueInfo, cnmPeerUpdate, &rCmdUpdate, sizeof(CMD_PEER_UPDATE_T), FALSE, FALSE, FALSE,
			   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;
	/* for Ch Sw AP prohibit case */
	if (prAisBssInfo->fgTdlsIsChSwProhibited) {
		/* disable TDLS ch sw function */

		rStatus = kalIoctl(prGlueInfo,
				   TdlsSendChSwControlCmd,
				   &TdlsSendChSwControlCmd, sizeof(CMD_TDLS_CH_SW_T), FALSE, FALSE, FALSE,
				   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for adding a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_add_station(struct wiphy *wiphy, struct net_device *ndev,
				const u8 *mac, struct station_parameters *params)
{
	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	P_GLUE_INFO_T prGlueInfo = NULL;
	CMD_PEER_ADD_T rCmdCreate;
	ADAPTER_T *prAdapter;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	/* init */
	kalMemZero(&rCmdCreate, sizeof(rCmdCreate));
	kalMemCopy(rCmdCreate.aucPeerMac, mac, 6);

	/* create a TDLS peer record */
	if ((params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER))) {
		rCmdCreate.eStaType = STA_TYPE_DLS_PEER;
		rStatus = kalIoctl(prGlueInfo, cnmPeerAdd, &rCmdCreate, sizeof(CMD_PEER_ADD_T), FALSE, FALSE, FALSE,
				   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for deleting a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 *
 * @other
 *		must implement if you have add_station().
 */
/*----------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
int mtk_cfg80211_del_station(struct wiphy *wiphy, struct net_device *ndev, struct station_del_parameters *params)
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy, struct net_device *ndev, const u8 *mac)
#endif
{
/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	P_GLUE_INFO_T prGlueInfo = NULL;
	ADAPTER_T *prAdapter;
	STA_RECORD_T *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL\n");
		return -1;
	}

	prAdapter = prGlueInfo->prAdapter;
	/* For kernel 3.18 modification, we trasfer to local buff to query sta */
	memset(deleteMac, 0, MAC_ADDR_LEN);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	memcpy(&deleteMac, params->mac, MAC_ADDR_LEN);
#else
	memcpy(&deleteMac, mac, MAC_ADDR_LEN);
#endif

	prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to transmit a TDLS data frame from nl80211.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[in]
* \param[in]
* \param[in] buf includes RSN IE + FT IE + Lifetimeout IE
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       bool initiator, const u8 *buf, size_t len)
{

	GLUE_INFO_T *prGlueInfo;
	TDLS_CMD_LINK_MGT_T rCmdMgt;
	UINT_32 u4BufLen;

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	prGlueInfo = (GLUE_INFO_T *) wiphy_priv(wiphy);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);
	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);

	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt, sizeof(TDLS_CMD_LINK_MGT_T), FALSE, FALSE, FALSE,
		 /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
		 &u4BufLen);
	return 0;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to hadel TDLS link from nl80211.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
* \param[in]
* \param[in]
* \param[in] buf includes RSN IE + FT IE + Lifetimeout IE
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
	const u8 *peer, enum nl80211_tdls_operation oper)
{

	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen;
	ADAPTER_T *prAdapter;
	TDLS_CMD_LINK_OPER_T rCmdOper;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper = oper;

	kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper, sizeof(TDLS_CMD_LINK_OPER_T), FALSE, FALSE, FALSE,
		 /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
		 &u4BufLen);
	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief cfg80211 suspend callback, will be invoked in wiphy_suspend.
 *
 * @param wiphy: pointer to wiphy
 *        wow:   pointer to cfg80211_wowlan
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int	mtk_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted() || !wiphy)
		goto end;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	set_bit(SUSPEND_FLAG_FOR_WAKEUP_REASON, &prGlueInfo->prAdapter->ulSuspendFlag);
	set_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME, &prGlueInfo->prAdapter->ulSuspendFlag);
end:
	kalHaltUnlock();
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief cfg80211 resume callback, will be invoked in wiphy_resume.
 *
 * @param wiphy: pointer to wiphy
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_resume(struct wiphy *wiphy)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_BSS_DESC_T *pprBssDesc = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_8 i = 0;

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted() || !wiphy)
		goto end;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	prAdapter = prGlueInfo->prAdapter;
	clear_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME, &prAdapter->ulSuspendFlag);
	pprBssDesc = &prAdapter->rWifiVar.rScanInfo.rNloParam.aprPendingBssDescToInd[0];
	for (; i < SCN_SSID_MATCH_MAX_NUM; i++) {
		if (pprBssDesc[i] == NULL)
			break;
		if (pprBssDesc[i]->u2RawLength == 0)
			continue;
		kalIndicateBssInfo(prGlueInfo,
						   (PUINT_8) pprBssDesc[i]->aucRawBuf,
						   pprBssDesc[i]->u2RawLength,
						   pprBssDesc[i]->ucChannelNum,
						   RCPI_TO_dBm(pprBssDesc[i]->ucRCPI));
	}
	DBGLOG(SCN, INFO, "pending %d sched scan results\n", i);
	if (i > 0)
		kalMemZero(&pprBssDesc[0], i * sizeof(P_BSS_DESC_T));
end:
	kalHaltUnlock();
	return 0;
}

INT_32 mtk_cfg80211_process_str_cmd(P_GLUE_INFO_T prGlueInfo, PUINT_8 cmd, INT_32 len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4SetInfoLen = 0;

	if (strncasecmp(cmd, "tdls-ps ", 8) == 0) {
#if CFG_SUPPORT_TDLS
		rStatus = kalIoctl(prGlueInfo,
					   wlanoidDisableTdlsPs,
					   (PVOID)(cmd+8), 1, FALSE, FALSE, TRUE, &u4SetInfoLen);
#else
		DBGLOG(REQ, WARN, "not support tdls\n");
		return -EOPNOTSUPP;
#endif
	} else if (strncasecmp(cmd, "NEIGHBOR-REQUEST", 16) == 0) {
		PUINT_8 pucSSID = NULL;
		UINT_32 u4SSIDLen = 0;

		if (len > 16 && (strncasecmp(cmd+16, " SSID=", 6) == 0)) {
			pucSSID = cmd + 22;
			u4SSIDLen = len - 22;
			DBGLOG(REQ, INFO, "cmd=%s, ssid len %u, ssid=%s\n", cmd, u4SSIDLen, pucSSID);
		}
		rStatus = kalIoctl(prGlueInfo, wlanoidSendNeighborRequest,
				   (PVOID)pucSSID, u4SSIDLen, FALSE, FALSE, TRUE, &u4SetInfoLen);
	} else if (strncasecmp(cmd, "BSS-TRANSITION-QUERY", 20) == 0) {
		PUINT_8 pucReason = NULL;

		if (len > 20 && (strncasecmp(cmd+20, " reason=", 8) == 0))
			pucReason = cmd + 28;
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSendBTMQuery,
				   (PVOID)pucReason, 1, FALSE, FALSE, TRUE, &u4SetInfoLen);
	} else if (strncasecmp(cmd, "O-SAR-ENABLE", 12) == 0) {
		UINT_32 u2SarMode = 0;
		INT_8 ret = -1;

		DBGLOG(REQ, INFO, "cmd=%s\n", cmd);
		do {
			if (len <= 13) {
				DBGLOG(REQ, ERROR, "len <= 13 (%d).\n", len);
				break;
			}

			ret = kstrtouint(cmd+13, 0, &u2SarMode);
			if (ret) {
				DBGLOG(REQ, ERROR, "string to int fail %d.\n", ret);
				break;
			}

			DBGLOG(REQ, INFO, "u2SarMode=%d\n", u2SarMode);

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSendSarEnable,
					   (PVOID)&u2SarMode,
					   sizeof(u2SarMode),
					   FALSE,
					   FALSE,
					   TRUE,
					   &u4SetInfoLen);

		} while (FALSE);

	} else if (kalStrniCmp(cmd, "OSHAREMOD ", 10) == 0) {
#if CFG_SUPPORT_OSHARE
		struct OSHARE_MODE_T cmdBuf;
		struct OSHARE_MODE_T *pCmdHeader = NULL;
		struct OSHARE_MODE_SETTING_V1_T *pCmdData = NULL;

		kalMemZero(&cmdBuf, sizeof(cmdBuf));

		pCmdHeader = &cmdBuf;
		pCmdHeader->cmdVersion = OSHARE_MODE_CMD_V1;
		pCmdHeader->cmdType = 1; /*1-set   0-query*/
		pCmdHeader->magicCode = OSHARE_MODE_MAGIC_CODE;
		pCmdHeader->cmdBufferLen = MAX_OSHARE_MODE_LENGTH;

		pCmdData = (struct OSHARE_MODE_SETTING_V1_T *)&(pCmdHeader->buffer[0]);
		pCmdData->osharemode = *(PUINT_8)(cmd + 10) - '0';

		DBGLOG(REQ, INFO, "cmd=%s, osharemode=%u\n", cmd, pCmdData->osharemode);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetOshareMode,
				   &cmdBuf,
				   sizeof(struct OSHARE_MODE_T),
				   FALSE,
				   FALSE,
				   TRUE,
				   &u4SetInfoLen);
		if (rStatus == WLAN_STATUS_SUCCESS)
			prGlueInfo->prAdapter->fgEnOshareMode = pCmdData->osharemode;
#else
		DBGLOG(REQ, WARN, "not support OSHAREMOD\n");
		return -EOPNOTSUPP;
#endif

	} else
		return -EOPNOTSUPP;

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;

	return -EINVAL;
}

int mtk_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *dev,
				 struct cfg80211_update_ft_ies_params *ftie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4InfoBufLen = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

#if !CFG_SUPPORT_802_11R
	DBGLOG(OID, INFO, "802.11R is not enabled\n");
	return 0;
#endif
	if (!wiphy)
		return -1;
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	rStatus = kalIoctl(prGlueInfo, wlanoidUpdateFtIes, (PVOID)ftie, sizeof(*ftie), FALSE,
					FALSE, FALSE, &u4InfoBufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "update Ft IE failed\n");
	return 0;
}

