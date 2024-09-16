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

/*
 * workaround for some ANR CRs. if suppliant is blocked longer than 10s, wifi hal will tell wifiMonitor
 * to teminate. for the case which can block supplicant 10s is to del key more than 5 times. the root cause
 * is that there is no resource in TC4, so del key command was not able to set, and then oid
 * timeout was happed. if we found the root cause why fw couldn't release TC resouce, we will remove this
 * workaround
 */
static UINT_8 gucKeyIndex = 255;

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
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
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

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if (type == NL80211_IFTYPE_STATION)
		eOpMode = NET_TYPE_INFRA;
	else if (type == NL80211_IFTYPE_ADHOC)
		eOpMode = NET_TYPE_IBSS;
	else
		return -EINVAL;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetInfrastructureMode,
			   &eOpMode, sizeof(eOpMode), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set infrastructure mode error:%x\n", rStatus);

	/* reset wpa info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
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
	UINT_8 tmp1[8];
	UINT_8 tmp2[8];

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(&rKey, sizeof(PARAM_KEY_T));

	rKey.u4KeyIndex = key_index;

	if (params->cipher) {
		switch (params->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			rKey.ucCipher = CIPHER_SUITE_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			rKey.ucCipher = CIPHER_SUITE_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			rKey.ucCipher = CIPHER_SUITE_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			rKey.ucCipher = CIPHER_SUITE_CCMP;
			break;
		case WLAN_CIPHER_SUITE_SMS4:
			rKey.ucCipher = CIPHER_SUITE_WPI;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			rKey.ucCipher = CIPHER_SUITE_BIP;
			break;
		default:
			ASSERT(FALSE);
		}
	}

	if (mac_addr) {
		COPY_MAC_ADDR(rKey.arBSSID, mac_addr);
		if ((rKey.arBSSID[0] == 0x00) && (rKey.arBSSID[1] == 0x00) && (rKey.arBSSID[2] == 0x00) &&
		    (rKey.arBSSID[3] == 0x00) && (rKey.arBSSID[4] == 0x00) && (rKey.arBSSID[5] == 0x00)) {
			rKey.arBSSID[0] = 0xff;
			rKey.arBSSID[1] = 0xff;
			rKey.arBSSID[2] = 0xff;
			rKey.arBSSID[3] = 0xff;
			rKey.arBSSID[4] = 0xff;
			rKey.arBSSID[5] = 0xff;
		}
		if (rKey.arBSSID[0] != 0xFF) {
			rKey.u4KeyIndex |= BIT(31);
			if ((rKey.arBSSID[0] != 0x00) || (rKey.arBSSID[1] != 0x00) || (rKey.arBSSID[2] != 0x00) ||
			    (rKey.arBSSID[3] != 0x00) || (rKey.arBSSID[4] != 0x00) || (rKey.arBSSID[5] != 0x00))
				rKey.u4KeyIndex |= BIT(30);
		}
	} else {
		rKey.arBSSID[0] = 0xff;
		rKey.arBSSID[1] = 0xff;
		rKey.arBSSID[2] = 0xff;
		rKey.arBSSID[3] = 0xff;
		rKey.arBSSID[4] = 0xff;
		rKey.arBSSID[5] = 0xff;
		/* rKey.u4KeyIndex |= BIT(31);//Enable BIT 31 will make tx use bc key id,should use pairwise key id 0 */
	}

	if (params->key) {
		/* rKey.aucKeyMaterial[0] = kalMemAlloc(params->key_len, VIR_MEM_TYPE); */
		kalMemCopy(rKey.aucKeyMaterial, params->key, params->key_len);
		if (params->key_len == 32) {
			kalMemCopy(tmp1, &params->key[16], 8);
			kalMemCopy(tmp2, &params->key[24], 8);
			kalMemCopy(&rKey.aucKeyMaterial[16], tmp2, 8);
			kalMemCopy(&rKey.aucKeyMaterial[24], tmp1, 8);
		}
	}

	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = ((ULONG)&(((P_PARAM_KEY_T) 0)->aucKeyMaterial)) + rKey.u4KeyLength;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, &rKey, rKey.u4Length, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

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

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

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

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(&rRemoveKey, sizeof(PARAM_REMOVE_KEY_T));
	if (mac_addr)
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
	else if (key_index <= gucKeyIndex) {	/* new operation, reset gucKeyIndex */
		gucKeyIndex = 255;
	} else {			/* bypass the next remove key operation */
		gucKeyIndex = key_index;
		return -EBUSY;
	}
	rRemoveKey.u4KeyIndex = key_index;
	rRemoveKey.u4Length = sizeof(PARAM_REMOVE_KEY_T);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRemoveKey, &rRemoveKey, rRemoveKey.u4Length, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "remove key error:%x\n", rStatus);
		if (rStatus == WLAN_STATUS_FAILURE && mac_addr) {
			i4Rslt = -EBUSY;
			gucKeyIndex = key_index;
		}
	} else {
		gucKeyIndex = 255;
		i4Rslt = 0;
	}

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

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

	/* not implemented */

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting set_default_mgmt_ke on an interface
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_default_mgmt_key(struct wiphy *wiphy, struct net_device *netdev, u8 key_index)
{
	return 0;
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
#define LINKSPEED_MAX_RANGE_11BGN 3000
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	PARAM_MAC_ADDRESS arBssid;
	UINT_32 u4BufLen;
	UINT_32 u4Rate = 0;
	UINT_32 u8diffTxBad, u8diffRetry;
	INT_32 i4Rssi = 0;
	PARAM_802_11_STATISTICS_STRUCT_T rStatistics;

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN, "incorrect BSSID: [ %pM ] currently connected BSSID[ %pM ]\n",
				   mac, arBssid);
		/* wificond will bring the mac address of the device itself */
		/* return -ENOENT; */
	}

	/* 2. fill TX rate */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryLinkSpeed, &u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE, FALSE, &u4BufLen);
		if ((rStatus != WLAN_STATUS_SUCCESS) || (u4Rate == 0)) {
			/* DBGLOG(REQ, WARN, "unable to retrieve link speed\n")); */
			goto put_cache_info;
		} else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
			sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
#else
			sinfo->filled |= STATION_INFO_TX_BITRATE;
#endif
			sinfo->txrate.legacy = u4Rate / 1000;	/* convert from 100bps to 100kbps */
			prGlueInfo->u4LinkSpeedCache = u4Rate / 1000;
		}
	}

	/* 3. fill RSSI */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryRssi, &i4Rssi, sizeof(i4Rssi), TRUE, FALSE, FALSE, FALSE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS || (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM)
		|| (i4Rssi == PARAM_WHQL_RSSI_MAX_DBM)) {
			/* DBGLOG(REQ, WARN, "unable to retrieve link speed\n"); */
			goto put_cache_info;
		} else {
			/* in the cfg80211 layer, the signal is a signed char variable. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
			sinfo->filled |= BIT(NL80211_STA_INFO_SIGNAL);
#else
			sinfo->filled |= STATION_INFO_SIGNAL;
#endif
			sinfo->signal = i4Rssi;	/* dBm */
			prGlueInfo->i4RssiCache = i4Rssi;
		}

		/* 4. Fill Tx OK and Tx Bad */
		{
			WLAN_STATUS rStatus;

			kalMemZero(&rStatistics, sizeof(rStatistics));
			/* Get Tx OK/Fail cnt from AIS statistic counter */
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidQueryStatisticsPL,
					   &rStatistics, sizeof(rStatistics), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, WARN, "unable to retrieive statistic\n"); */
				goto put_cache_info;
			} else {
				INT_32 i4RssiThreshold = -85;	/* set rssi threshold -85dBm */
				UINT_32 u4LinkspeedThreshold = 55;	/* set link speed threshold 5.5Mbps */
				BOOLEAN fgWeighted = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
				sinfo->filled |= BIT(NL80211_STA_INFO_TX_PACKETS);
				sinfo->filled |= BIT(NL80211_STA_INFO_TX_FAILED);
				sinfo->filled |= BIT(NL80211_STA_INFO_TX_BYTES);
				sinfo->filled |= BIT(NL80211_STA_INFO_RX_PACKETS);
				sinfo->filled |= BIT(NL80211_STA_INFO_RX_BYTES);
#else
				sinfo->filled |= STATION_INFO_TX_PACKETS;
				sinfo->filled |= STATION_INFO_TX_FAILED;
				sinfo->filled |= STATION_INFO_TX_BYTES;
				sinfo->filled |= STATION_INFO_RX_PACKETS;
				sinfo->filled |= STATION_INFO_RX_BYTES;
#endif

				/* calculate difference */
				u8diffTxBad = rStatistics.rFailedCount.QuadPart - prGlueInfo->u8Statistic[0];
				u8diffRetry = rStatistics.rRetryCount.QuadPart - prGlueInfo->u8Statistic[1];
				/* restore counters */
				prGlueInfo->u8Statistic[0] = rStatistics.rFailedCount.QuadPart;
				prGlueInfo->u8Statistic[1] = rStatistics.rRetryCount.QuadPart;

				/* check threshold is valid */
				if (prGlueInfo->fgPoorlinkValid) {
					if (prGlueInfo->i4RssiThreshold)
						i4RssiThreshold = prGlueInfo->i4RssiThreshold;
					if (prGlueInfo->u4LinkspeedThreshold)
						u4LinkspeedThreshold = prGlueInfo->u4LinkspeedThreshold;
				}
				/* add weighted to fail counter */
				if (sinfo->txrate.legacy < u4LinkspeedThreshold || sinfo->signal < i4RssiThreshold) {
					prGlueInfo->u8TotalFailCnt += (u8diffTxBad * 16 + u8diffRetry);
					fgWeighted = 1;
				} else {
					prGlueInfo->u8TotalFailCnt += u8diffTxBad;
				}
				/* report counters */
				/* prGlueInfo->rNetDevStats.tx_packets =
				 *	rStatistics.rTransmittedFragmentCount.QuadPart;
				 */
				prGlueInfo->rNetDevStats.tx_errors = prGlueInfo->u8TotalFailCnt;
				sinfo->tx_packets = prGlueInfo->rNetDevStats.tx_packets;
				sinfo->tx_failed = prGlueInfo->rNetDevStats.tx_errors;
				sinfo->tx_bytes = prGlueInfo->rNetDevStats.tx_bytes;
				sinfo->rx_packets = prGlueInfo->rNetDevStats.rx_packets;
				sinfo->rx_bytes = prGlueInfo->rNetDevStats.rx_bytes;
				/* Good Fail Bad Difference retry difference Linkspeed Rate Weighted */
				DBGLOG(REQ, TRACE,
				"Poorlink State TxOK:%lld %u RxOK:%u TxFail:%d Bad:%d Retry:%d TxBytes:%llu Rx:%llu",
					rStatistics.rTransmittedFragmentCount.QuadPart,
					sinfo->tx_packets, sinfo->rx_packets,
					sinfo->tx_failed,
					(int)u8diffTxBad,
					(int)u8diffRetry,
					sinfo->tx_bytes, sinfo->rx_bytes);
				DBGLOG(REQ, TRACE,
					"Rate(%d) Signal(%d) Weight(%d) QuadPart(%d)\n",
					sinfo->txrate.legacy,
					sinfo->signal,
					(int)fgWeighted,
					(int)rStatistics.rMultipleRetryCount.QuadPart);
			}
		}
	}
	return 0;

put_cache_info:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	if (!(BIT(NL80211_STA_INFO_TX_BITRATE) & sinfo->filled)) {
#else
	if (!(STATION_INFO_TX_BITRATE & sinfo->filled)) {
#endif
		DBGLOG(REQ, WARN, "last link speed, status=%d, rate=%d\n", rStatus, u4Rate);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
#else
		sinfo->filled |= STATION_INFO_TX_BITRATE;
#endif
		sinfo->txrate.legacy = prGlueInfo->u4LinkSpeedCache;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	if (!(BIT(NL80211_STA_INFO_SIGNAL) & sinfo->filled)) {
#else
	if (!(STATION_INFO_SIGNAL & sinfo->filled)) {
#endif
		DBGLOG(REQ, WARN, "last rssi\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_SIGNAL);
#else
		sinfo->filled |= STATION_INFO_SIGNAL;
#endif
		sinfo->signal = prGlueInfo->i4RssiCache;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	if (!(BIT(NL80211_STA_INFO_TX_PACKETS) & sinfo->filled)) {
#else
	if (!(STATION_INFO_TX_PACKETS & sinfo->filled)) {
#endif
		DBGLOG(REQ, WARN, "last statistic\n");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_FAILED);
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_BYTES);
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_BYTES);
#else
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->filled |= STATION_INFO_TX_FAILED;
		sinfo->filled |= STATION_INFO_TX_BYTES;
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->filled |= STATION_INFO_RX_BYTES;
#endif
		sinfo->tx_packets = prGlueInfo->rNetDevStats.tx_packets;
		sinfo->tx_failed = prGlueInfo->rNetDevStats.tx_errors;
		sinfo->tx_bytes = prGlueInfo->rNetDevStats.tx_bytes;
		sinfo->rx_packets = prGlueInfo->rNetDevStats.rx_packets;
		sinfo->rx_bytes = prGlueInfo->rNetDevStats.rx_bytes;
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
int mtk_cfg80211_change_station(struct wiphy *wiphy, struct net_device *ndev,
				const u8 *mac, struct station_parameters *params)
{
#if (CFG_SUPPORT_TDLS == 1)
	/*
	 * EX: In supplicant,
	 * (Supplicant) wpa_tdls_process_tpk_m3() ->
	 * (Supplicant) wpa_tdls_enable_link() ->
	 * (Supplicant) wpa_sm_tdls_peer_addset() ->
	 * (Supplicant) ..tdls_peer_addset() ->
	 * (Supplicant) wpa_supplicant_tdls_peer_addset() ->
	 * (Supplicant) wpa_drv_sta_add() ->
	 * (Supplicant) ..sta_add() ->
	 * (Supplicant) wpa_driver_nl80211_sta_add() ->
	 * (NL80211) nl80211_set_station() ->
	 * (Driver) mtk_cfg80211_change_station()
	 *
	 * if nl80211_set_station fails, supplicant will tear down the link.
	 */
	P_GLUE_INFO_T prGlueInfo;
	TDLS_CMD_PEER_UPDATE_T rCmdUpdate;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen, u4Temp;

	/* sanity check */
	if ((wiphy == NULL) || (mac == NULL) || (params == NULL))
		return -EINVAL;

	DBGLOG(TDLS, INFO, "%s: 0x%p 0x%x\n", __func__, params->supported_rates, params->sta_flags_set);

	if (!(params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER)))
		return -EOPNOTSUPP;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(ndev));
	if (prGlueInfo == NULL)
		return -EINVAL;

	/* TODO: check if we are station mode, not AP mode */

	/* init */
	kalMemZero(&rCmdUpdate, sizeof(rCmdUpdate));
	kalMemCopy(rCmdUpdate.aucPeerMac, mac, 6);

	if (ndev == prGlueInfo->prDevHandler) {
		DBGLOG(TDLS, INFO, "AIS network\n");
		rCmdUpdate.eNetworkType = NETWORK_TYPE_AIS_INDEX;
	} else {
		DBGLOG(TDLS, INFO, "P2P network\n");
		rCmdUpdate.eNetworkType = NETWORK_TYPE_P2P_INDEX;
	}

	if (params->supported_rates != NULL) {
		u4Temp = params->supported_rates_len;
		if (u4Temp > TDLS_CMD_PEER_UPDATE_SUP_RATE_MAX) {
			u4Temp = TDLS_CMD_PEER_UPDATE_SUP_RATE_MAX;
			DBGLOG(TDLS, ERROR, "%s sup rate too long: %d\n", __func__, params->supported_rates_len);
		}
		kalMemCopy(rCmdUpdate.aucSupRate, params->supported_rates, u4Temp);
		rCmdUpdate.u2SupRateLen = u4Temp;
	}

	/*
	 * In supplicant, only recognize WLAN_EID_QOS 46, not 0xDD WMM
	 * So force to support UAPSD here.
	 */
	rCmdUpdate.UapsdBitmap = 0x0F;	/*params->uapsd_queues; */
	rCmdUpdate.UapsdMaxSp = 0;	/*params->max_sp; */

	DBGLOG(TDLS, INFO, "%s: UapsdBitmap=0x%x UapsdMaxSp=%d\n",
			    __func__, rCmdUpdate.UapsdBitmap, rCmdUpdate.UapsdMaxSp);

	rCmdUpdate.u2Capability = params->capability;

	if (params->ext_capab != NULL) {
		u4Temp = params->ext_capab_len;
		if (u4Temp > TDLS_CMD_PEER_UPDATE_EXT_CAP_MAXLEN) {
			u4Temp = TDLS_CMD_PEER_UPDATE_EXT_CAP_MAXLEN;
			DBGLOG(TDLS, ERROR, "%s ext_capab too long: %d\n", __func__, params->ext_capab_len);
		}
		kalMemCopy(rCmdUpdate.aucExtCap, params->ext_capab, u4Temp);
		rCmdUpdate.u2ExtCapLen = u4Temp;
	}

	if (params->ht_capa != NULL) {
		DBGLOG(TDLS, INFO, "%s: peer is 11n device\n", __func__);

		rCmdUpdate.rHtCap.u2CapInfo = params->ht_capa->cap_info;
		rCmdUpdate.rHtCap.ucAmpduParamsInfo = params->ht_capa->ampdu_params_info;
		rCmdUpdate.rHtCap.u2ExtHtCapInfo = params->ht_capa->extended_ht_cap_info;
		rCmdUpdate.rHtCap.u4TxBfCapInfo = params->ht_capa->tx_BF_cap_info;
		rCmdUpdate.rHtCap.ucAntennaSelInfo = params->ht_capa->antenna_selection_info;
		kalMemCopy(rCmdUpdate.rHtCap.rMCS.arRxMask,
			   params->ht_capa->mcs.rx_mask, sizeof(rCmdUpdate.rHtCap.rMCS.arRxMask));
		rCmdUpdate.rHtCap.rMCS.u2RxHighest = params->ht_capa->mcs.rx_highest;
		rCmdUpdate.rHtCap.rMCS.ucTxParams = params->ht_capa->mcs.tx_params;
		/* use ht info in TDLS setup frames */
		rCmdUpdate.fgIsSupHt = FALSE;
	}

	/* update a TDLS peer record */
	rStatus = kalIoctl(prGlueInfo,
			   TdlsexPeerUpdate,
			   &rCmdUpdate, sizeof(TDLS_CMD_PEER_UPDATE_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TDLS, ERROR, "%s update error:%x\n", __func__, rStatus);
		return -EINVAL;
	}
#endif /* CFG_SUPPORT_TDLS */

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
#if (CFG_SUPPORT_TDLS == 1)
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	P_GLUE_INFO_T prGlueInfo;
	TDLS_CMD_PEER_ADD_T rCmdCreate;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	if ((wiphy == NULL) || (mac == NULL) || (params == NULL))
		return -EINVAL;

	/*
	 * wpa_sm_tdls_peer_addset(sm, peer->addr, 1, 0, 0, NULL, 0, NULL, NULL, 0,
	 * NULL, 0);
	 *
	 * wpa_sm_tdls_peer_addset(struct wpa_sm *sm, const u8 *addr, int add,
	 * u16 aid, u16 capability, const u8 *supp_rates,
	 * size_t supp_rates_len,
	 * const struct ieee80211_ht_capabilities *ht_capab,
	 * const struct ieee80211_vht_capabilities *vht_capab,
	 * u8 qosinfo, const u8 *ext_capab, size_t ext_capab_len)
	 *
	 * Only MAC address of the peer is valid.
	 */

	DBGLOG(TDLS, INFO, "%s: 0x%p %d\n", __func__, params->supported_rates, params->supported_rates_len);

	/* sanity check */
	if (!(params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER)))
		return -EOPNOTSUPP;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(ndev));

	if (prGlueInfo == NULL)
		return -EINVAL;

	/* TODO: check if we are station mode, not AP mode */

	/* init */
	kalMemZero(&rCmdCreate, sizeof(rCmdCreate));
	kalMemCopy(rCmdCreate.aucPeerMac, mac, 6);

	if (ndev == prGlueInfo->prDevHandler) {
		DBGLOG(TDLS, INFO, "AIS network\n");
		rCmdCreate.eNetworkType = NETWORK_TYPE_AIS_INDEX;
	} else {
		DBGLOG(TDLS, INFO, "P2P network\n");
		rCmdCreate.eNetworkType = NETWORK_TYPE_P2P_INDEX;
	}
#if 0
	rCmdCreate.eNetTypeIndex = NETWORK_TYPE_AIS_INDEX;

	rCmdCreate.u2CapInfo = params->capability;

	DBGLOG(TDLS, INFO, "<tdls_cmd> %s: capability = 0x%x\n", __func__, rCmdCreate.u2CapInfo);

	if ((params->supported_rates != NULL) && (params->supported_rates_len != 0)) {
		UINT32 u4Idx;

		DBGLOG(TDLS, INFO, "<tdls_cmd> %s: sup rate = 0x", __func__);

		rIeSup.ucId = ELEM_ID_SUP_RATES;
		rIeSup.ucLength = params->supported_rates_len;
		for (u4Idx = 0; u4Idx < rIeSup.ucLength; u4Idx++) {
			rIeSup.aucSupportedRates[u4Idx] = params->supported_rates[u4Idx];
			DBGLOG(TDLS, INFO, "%x ", rIeSup.aucSupportedRates[u4Idx]);
		}
		DBGLOG(TDLS, INFO, "\n");

		rateGetRateSetFromIEs(&rIeSup,
				      NULL,
				      &rCmdCreate.u2OperationalRateSet,
				      &rCmdCreate.u2BSSBasicRateSet, &rCmdCreate.fgIsUnknownBssBasicRate);
	}

	/* phy type */
#endif

	/* create a TDLS peer record */
	rStatus = kalIoctl(prGlueInfo,
			   TdlsexPeerAdd,
			   &rCmdCreate, sizeof(TDLS_CMD_PEER_ADD_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TDLS, ERROR, "%s create error:%x\n", __func__, rStatus);
		return -EINVAL;
	}
#endif /* CFG_SUPPORT_TDLS */

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
#if CFG_MULTI_SSID_SCAN
static PARAM_SCAN_REQUEST_ADV_T rScanRequest;
#else
static PARAM_SCAN_REQUEST_EXT_T rScanRequest;
#endif
int mtk_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	UINT_32 num_ssid = 0;
	UINT_32 old_num_ssid = 0;
	UINT_32 wildcard_flag = 0;
#if CFG_MULTI_SSID_SCAN
	UINT_32 i, u4ValidIdx;
#endif
/* PARAM_SCAN_REQUEST_EXT_T rScanRequest; */

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
	prBssInfo = &(prGlueInfo->prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	if ((!prGlueInfo->prAdapter->fgEnCfg80211Scan) &&
			(kalGetMediaStateIndicated(prGlueInfo) ==
			PARAM_MEDIA_STATE_CONNECTED)) {
		DBGLOG(SCN, WARN, "Disable cfg80211 scan.\n");
		return -EBUSY;
	}

#if CFG_MULTI_SSID_SCAN
	kalMemZero(&rScanRequest, sizeof(PARAM_SCAN_REQUEST_ADV_T));
	/* check if there is any pending scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo->prScanRequest != NULL\n");
		return -EBUSY;
	}

	old_num_ssid = num_ssid = (UINT_32)request->n_ssids;
	DBGLOG(REQ, TRACE, "request->n_ssids=%d\n", request->n_ssids);

	if (request->n_ssids == 0)
		rScanRequest.u4SsidNum = 0;
	else if (request->n_ssids <= (SCN_SSID_MAX_NUM + 1)) {
		u4ValidIdx = 0;
		for (i = 0; i < request->n_ssids; i++) {
			if ((request->ssids[i].ssid[0] == 0)
				|| (request->ssids[i].ssid_len == 0)) {
				num_ssid--; /* remove if this is a wildcard scan*/
				wildcard_flag |= (1 << i);
				DBGLOG(REQ, TRACE, "i=%d, num_ssid-- for wildcard scan\n", i);
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
#else
	kalMemZero(&rScanRequest, sizeof(PARAM_SCAN_REQUEST_EXT_T));

	/* check if there is any pending scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo->prScanRequest != NULL\n");
		return -EBUSY;
	}

	if (request->n_ssids == 0) {
		rScanRequest.rSsid.u4SsidLen = 0;
	} else if (request->n_ssids == 1) {
		/*wildcard ssid*/
		COPY_SSID(rScanRequest.rSsid.aucSsid, rScanRequest.rSsid.u4SsidLen, request->ssids[0].ssid,
			  request->ssids[0].ssid_len);
	} else if (request->n_ssids == 2) {
		DBGLOG(REQ, INFO, "mtk_cfg80211_scan,[0]ssid:%s, [0]ssid_len:%d [1]ssid:%s, [1]ssid_len:%d"
		, HIDE(request->ssids[0].ssid), request->ssids[0].ssid_len
		, HIDE(request->ssids[1].ssid), request->ssids[1].ssid_len);
		/*ssids[0]: specific ssid*/
		/*ssids[1]: wildcard ssid*/

		request->n_ssids--;
		COPY_SSID(rScanRequest.rSsid.aucSsid, rScanRequest.rSsid.u4SsidLen, request->ssids[0].ssid,
			  request->ssids[0].ssid_len);
	} else {
		DBGLOG(REQ, ERROR, "request to find %d SSIDs, but only support %d\n"
			, request->n_ssids, GL_CFG80211_SCAN_SSID_MAX_NUM);
		return -EINVAL;
	}
#endif
	DBGLOG(REQ, INFO, "mtk_cfg80211_scan(), n_ssids=%d, num_ssid=(%u=>%u), wildcard=0x%X\n"
			, request->n_ssids, old_num_ssid, num_ssid, wildcard_flag);

	kalScanParseRandomMac(prBssInfo, request, rScanRequest.aucRandomMac);

	if (request->ie_len > 0) {
		rScanRequest.u4IELength = request->ie_len;
		rScanRequest.pucIE = (PUINT_8) (request->ie);
	} else {
		rScanRequest.u4IELength = 0;
	}
#if 0
	prGlueInfo->prScanRequest = request;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssidListScanExt,
			   &rScanRequest, sizeof(PARAM_SCAN_REQUEST_EXT_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "scan error:%x\n", rStatus);
		prGlueInfo->prScanRequest = NULL;
		return -EINVAL;
	}

	/*prGlueInfo->prScanRequest = request;*/
#endif
	/* temp save request ieee80211_channel info */
	rScanRequest.puPartialScanReq = (PUINT_8)request;
	DBGLOG(REQ, TRACE, "mtk_cfg80211_scan request=%p\n", rScanRequest.puPartialScanReq);

	prGlueInfo->prScanRequest = request;

#if CFG_MULTI_SSID_SCAN
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssidListScanAdv,
			   &rScanRequest, sizeof(PARAM_SCAN_REQUEST_ADV_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);
#else
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssidListScanExt,
			   &rScanRequest, sizeof(PARAM_SCAN_REQUEST_EXT_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);
#endif

	if (rStatus != WLAN_STATUS_SUCCESS) {
		prGlueInfo->prScanRequest = NULL;
		rScanRequest.puPartialScanReq = NULL;
		DBGLOG(REQ, ERROR, "scan error:%x\n", rStatus);
		return -EINVAL;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for abort an ongoing scan. The driver shall
 *        indicate the status of the scan through cfg80211_scan_done()
 *
 * @param wiphy - pointer of wireless hardware description
 *        wdev - pointer of  wireless device state
 *
 */
/*----------------------------------------------------------------------------*/
void mtk_cfg80211_abort_scan(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	UINT_32 u4SetInfoLen = 0;
	WLAN_STATUS rStatus;
	P_GLUE_INFO_T prGlueInfo = NULL;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo,
					   wlanoidAbortScan,
					   NULL, 1, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, ERROR, "wlanoidAbortScan fail 0x%x\n", rStatus);
}

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
#if (CFG_REFACTORY_PMKSA == 0)
#if CFG_SUPPORT_OKC
	UINT_16 u2PmkIdCnt = 0;
#endif
#endif
	UINT_32 u4BufLen;
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;
	ENUM_PARAM_AUTH_MODE_T eAuthMode;
	UINT_32 cipher, u4AkmSuite = 0;
	PARAM_CONNECT_T rNewSsid;
	ENUM_PARAM_OP_MODE_T eOpMode;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
	struct wireless_dev *wdev = NULL;
	P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry;
	P_IEEE_802_11_MIB_T prMib;
	int i;


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

	DBGLOG(REQ, INFO, "[wlan] mtk_cfg80211_connect %p %zu\n", sme->ie, sme->ie_len);
	prConnSettings = &prGlueInfo->prAdapter->rWifiVar.rConnSettings;

	if (prGlueInfo->prAdapter->rWifiVar.rConnSettings.eOPMode > NET_TYPE_AUTO_SWITCH)
		eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		eOpMode = prGlueInfo->prAdapter->rWifiVar.rConnSettings.eOPMode;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetInfrastructureMode,
			   &eOpMode, sizeof(eOpMode), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

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
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
#endif

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
#if CFG_SUPPORT_802_11R
	case NL80211_AUTHTYPE_FT:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_FT;
		break;
#endif
	case NL80211_AUTHTYPE_SAE:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_SAE;
		break;
	default:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM | IW_AUTH_ALG_SHARED_KEY;
		break;
	}

	if (sme->crypto.n_ciphers_pairwise) {
		DBGLOG(RSN, INFO, "cipher pairwise (%x)\n", sme->crypto.ciphers_pairwise[0]);
		prGlueInfo->prAdapter->rWifiVar.rConnSettings.rRsnInfo.au4PairwiseKeyCipherSuite[0] =
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
		DBGLOG(RSN, INFO, "cipher group (%x)\n", sme->crypto.cipher_group);
		prGlueInfo->prAdapter->rWifiVar.rConnSettings.rRsnInfo.u4GroupKeyCipherSuite = sme->crypto.cipher_group;
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
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher group (%d)\n", sme->crypto.cipher_group);
			return -EINVAL;
		}
	}

	if (sme->crypto.n_akm_suites) {
		prGlueInfo->prAdapter->rWifiVar.rConnSettings.rRsnInfo.au4AuthKeyMgtSuite[0] =
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
				DBGLOG(REQ, WARN, "invalid auth mode (%d)\n", sme->crypto.akm_suites[0]);
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
#if CFG_SUPPORT_802_11W
			case WLAN_AKM_SUITE_8021X_SHA256:
				eAuthMode = AUTH_MODE_WPA2;
				u4AkmSuite = RSN_AKM_SUITE_802_1X_SHA256;
				break;
			case WLAN_AKM_SUITE_PSK_SHA256:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_PSK_SHA256;
				break;
#endif
#if CFG_SUPPORT_HOTSPOT_2_0
			case WLAN_AKM_SUITE_OSEN:
				eAuthMode = AUTH_MODE_WPA_OSEN;
				u4AkmSuite = WFA_AKM_SUITE_OSEN;
			break;
#endif
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
			case WLAN_AKM_SUITE_SAE:
				if (sme->auth_type == NL80211_AUTHTYPE_SAE)
					eAuthMode = AUTH_MODE_WPA3_SAE;
				else
					eAuthMode = AUTH_MODE_OPEN;
				u4AkmSuite = RSN_CIPHER_SUITE_SAE;
				break;

			case WLAN_AKM_SUITE_OWE:
				eAuthMode = AUTH_MODE_WPA3_OWE;
				u4AkmSuite = RSN_CIPHER_SUITE_OWE;
				break;
			default:
				DBGLOG(REQ, WARN, "invalid auth mode (%d)\n", sme->crypto.akm_suites[0]);
				return -EINVAL;
			}
#if CFG_SUPPORT_HOTSPOT_2_0
		} else if (prGlueInfo->rWpaInfo.u4WpaVersion
				== IW_AUTH_WPA_VERSION_DISABLED &&
				sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_OSEN) {
			eAuthMode = AUTH_MODE_WPA_OSEN;
			u4AkmSuite = WFA_AKM_SUITE_OSEN;
#endif
		}
	}

	if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED && eAuthMode != AUTH_MODE_WPA_OSEN) {
		switch (prGlueInfo->rWpaInfo.u4AuthAlg) {
		case IW_AUTH_ALG_OPEN_SYSTEM:
			eAuthMode = AUTH_MODE_OPEN;
			break;
#if CFG_SUPPORT_802_11R
		case IW_AUTH_ALG_FT:
			eAuthMode = AUTH_MODE_NON_RSN_FT;
			break;
#endif
		default:
			eAuthMode = AUTH_MODE_AUTO_SWITCH;
			break;
		}
	}

	prGlueInfo->rWpaInfo.fgPrivacyInvoke = sme->privacy;

	prGlueInfo->fgWpsActive = FALSE;
#if CFG_SUPPORT_HOTSPOT_2_0
	prGlueInfo->fgConnectHS20AP = FALSE;
#endif
#if (CFG_REFACTORY_PMKSA == 0)
	prConnSettings->fgOkcEnabled = FALSE;
	prConnSettings->fgOkcPmksaReady = FALSE;
#endif
	if (sme->ie && sme->ie_len > 0) {
		WLAN_STATUS rStatus;
		UINT_32 u4BufLen;
		PUINT_8 prDesiredIE = NULL;
		PUINT_8 pucIEStart = (PUINT_8)sme->ie;

#if CFG_SUPPORT_WPS2
		prDesiredIE = (PUINT_8) kalFindIeMatchMask(ELEM_ID_VENDOR,
				pucIEStart, sme->ie_len, NULL, 0, 0, NULL);
		/* If it's vendor IE, check if it's WPS IE. */
		if (prDesiredIE && prDesiredIE[1] >= 4) {
			if (memcmp(&prDesiredIE[2], "\x00\x50\xf2\x04", 4) == 0)
				prGlueInfo->fgWpsActive = TRUE;
		}
#endif
#if CFG_SUPPORT_WAPI
		if (wextSrchDesiredWAPIIE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetWapiAssocInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, FALSE, FALSE, &u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(SEC, WARN, "[wapi] set wapi assoc info error:%x\n", rStatus);
		}
#endif

		DBGLOG(REQ, TRACE, "[wlan] wlanoidSetWapiAssocInfo: .fgWapiMode = %d\n",
				   prGlueInfo->prAdapter->rWifiVar.rConnSettings.fgWapiMode);

#if CFG_SUPPORT_HOTSPOT_2_0
		if (wextSrchDesiredHS20IE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* Do nothing */
				/* printk(KERN_INFO "[HS20] set HS20 assoc info error:%lx\n", rStatus); */
			}
		} else if (wextSrchDesiredOsenIE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			/* we can reuse aucHS20AssocInfoIE because hs20 indication IE is not present when OSEN exist */
			kalMemCopy(prGlueInfo->aucHS20AssocInfoIE, prDesiredIE, IE_SIZE(prDesiredIE));
			prGlueInfo->u2HS20AssocInfoIELen = (UINT_16)IE_SIZE(prDesiredIE);
		}
#endif
		if (wextSrchDesiredWPAIE(pucIEStart, sme->ie_len, 0x30,
					 (uint8_t **) &prDesiredIE)) {
			RSN_INFO_T rRsnInfo;

			if (rsnParseRsnIE(prGlueInfo->prAdapter,
			    (P_RSN_INFO_ELEM_T)prDesiredIE, &rRsnInfo)) {
#if CFG_SUPPORT_802_11W
				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
					prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_OPTIONAL;
					if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPR)
						prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_REQUIRED;
				} else
					prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
#endif
			}
		}
#if (CFG_REFACTORY_PMKSA == 0)
#if CFG_SUPPORT_OKC
		wextSrchOkcAndPMKID(pucIEStart, sme->ie_len, (PUINT_8 *)&prDesiredIE, &prConnSettings->fgOkcEnabled);

		if (prDesiredIE)
			u2PmkIdCnt = *(PUINT_16)prDesiredIE;
		DBGLOG(REQ, INFO, "u2PmkIdCnt %d\n", u2PmkIdCnt);

		if (u2PmkIdCnt != 0 && sme->bssid && !EQUAL_MAC_ADDR("\x0\x0\x0\x0\x0\x0", sme->bssid) &&
		    IS_UCAST_MAC_ADDR(sme->bssid)) {
			PARAM_PMKID_T rPmkid;

			rPmkid.u4Length = (UINT_32)(sizeof(rPmkid) | (1 << 31));
			rPmkid.u4BSSIDInfoCount = 1;
			kalMemCopy(rPmkid.arBSSIDInfo[0].arBSSID, sme->bssid, MAC_ADDR_LEN);
			kalMemCopy(rPmkid.arBSSIDInfo[0].arPMKID, prDesiredIE+2, IW_PMKID_LEN);
			rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetPmkid,
				   (PVOID)&rPmkid, rPmkid.u4Length, FALSE, FALSE, FALSE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(REQ, WARN, "failed to add OKC PMKID\n");
		}
#endif
#endif
	}

	/* Fill WPA info - mfp setting */
	/* Must put after paring RSNE from upper layer
	* for prWpaInfo->ucRSNMfpCap assignment
	*/
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
	case NL80211_MFP_REQUIRED:
		prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_REQUIRED;
		break;
	default:
		prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
		break;
	}
	DBGLOG(REQ, INFO, "MFP=%d\n", prGlueInfo->rWpaInfo.u4Mfp);
#endif

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAuthMode, &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set auth mode error:%x\n", rStatus);

	prMib = &prGlueInfo->prAdapter->rMib;
	/* Enable the specific AKM suite only. */
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prMib->
				dot11RSNAConfigAuthenticationSuitesTable[i];
		if (prEntry->dot11RSNAConfigAuthenticationSuite ==
		    u4AkmSuite)
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
									TRUE;
		else
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled =
									FALSE;
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
			   wlanoidSetEncryptionStatus,
			   &eEncStatus, sizeof(eEncStatus), FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set encryption mode error:%x\n", rStatus);

	if (sme->key_len != 0 && prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		P_PARAM_WEP_T prWepKey = (P_PARAM_WEP_T) wepBuf;

		prWepKey->u4Length = 12 + sme->key_len;
		prWepKey->u4KeyLength = (UINT_32) sme->key_len;
		prWepKey->u4KeyIndex = (UINT_32) sme->key_idx;
		prWepKey->u4KeyIndex |= BIT(31);
		if (prWepKey->u4KeyLength > 32) {
			DBGLOG(REQ, ERROR, "Too long key length (%u)\n", prWepKey->u4KeyLength);
			return -EINVAL;
		}
		kalMemCopy(prWepKey->aucKeyMaterial, sme->key, prWepKey->u4KeyLength);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetAddWep,
				   prWepKey, prWepKey->u4Length, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, ERROR, "wlanoidSetAddWep fail 0x%x\n", rStatus);
			return -EFAULT;
		}
	}

	if (sme->channel)
		rNewSsid.u4CenterFreq = sme->channel->center_freq;
	else
		rNewSsid.u4CenterFreq = 0;
	rNewSsid.pucBssid = (UINT_8 *)sme->bssid;
	rNewSsid.pucSsid = (UINT_8 *)sme->ssid;
	rNewSsid.u4SsidLen = sme->ssid_len;
	/* Check former assocIE to prevent memory leakage in situations like
	 * upper layer requests connection without disconnecting first, ...
	 */
	if (prConnSettings->assocIeLen > 0) {
		kalMemFree(prConnSettings->pucAssocIEs, VIR_MEM_TYPE,
			   prConnSettings->assocIeLen);
		prConnSettings->assocIeLen = 0;
	}

	if (sme->ie_len > 0) {
		prConnSettings->pucAssocIEs =
			kalMemAlloc(sme->ie_len, VIR_MEM_TYPE);
		if (prConnSettings->pucAssocIEs) {
			prConnSettings->assocIeLen = sme->ie_len;
			kalMemCopy(prConnSettings->pucAssocIEs,
				   sme->ie, prConnSettings->assocIeLen);
		} else {
			DBGLOG(INIT, INFO,
			       "allocate memory for prConnSettings->pucAssocIEs failed!\n");
			return -ENOMEM;
		}
	}
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetConnect,
			   (PVOID)(&rNewSsid), sizeof(PARAM_CONNECT_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "set SSID:%x\n", rStatus);
		return -EINVAL;
	}

	return 0;
}

#if CFG_SUPPORT_WPA3
int mtk_cfg80211_external_auth(struct wiphy *wiphy,
			 struct net_device *ndev,
			 struct cfg80211_external_auth_params *params)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4BufLen;
	struct PARAM_EXTERNAL_AUTH auth;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	if (!prGlueInfo)
		DBGLOG(REQ, WARN,
		       "SAE-confirm failed with invalid prGlueInfo\n");

	COPY_MAC_ADDR(auth.bssid, params->bssid);
	auth.status = params->status;
	auth.ucBssIdx = (uint8_t)NETWORK_TYPE_AIS_INDEX;
	rStatus = kalIoctl(prGlueInfo, wlanoidExternalAuthDone, (void *)&auth,
			   sizeof(auth), FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "SAE-confirm failed with: %d\n", rStatus);

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
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

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
	struct ieee80211_channel *channel = NULL;

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* set channel */
	if (params->chandef.chan)
		channel = params->chandef.chan;
	if (channel) {
		u4ChnlFreq = nicChannelNum2Freq(channel->hw_value);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetFrequency,
				   &u4ChnlFreq, sizeof(u4ChnlFreq), FALSE, FALSE, FALSE, FALSE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	/* set SSID */
	kalMemCopy(rNewSsid.aucSsid, params->ssid, params->ssid_len);
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSsid,
			   (PVOID)(&rNewSsid), sizeof(PARAM_SSID_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

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

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

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
	PARAM_POWER_MODE ePowerMode;

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if CFG_SUPPORT_DBG_POWERMODE
	if (prGlueInfo->prAdapter->fgEnDbgPowerMode) {
		DBGLOG(REQ, WARN, "Force power mode enabled, ignore this enable command: %d\n", enabled);
		return 0;
	}
#endif
	if (enabled) {
		if (timeout == -1)
			ePowerMode = Param_PowerModeFast_PSP;
		else
			ePowerMode = Param_PowerModeMAX_PSP;
	} else {
		ePowerMode = Param_PowerModeCAM;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSet802dot11PowerSaveProfile,
			   &ePowerMode, sizeof(ePowerMode), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set_power_mgmt error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}
#if (CFG_REFACTORY_PMKSA == 0)
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

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prPmkid = (P_PARAM_PMKID_T) kalMemAlloc(8 + sizeof(PARAM_BSSID_INFO_T), VIR_MEM_TYPE);
	if (!prPmkid) {
		DBGLOG(REQ, ERROR, "Can not alloc memory for IW_PMKSA_ADD\n");
		return -ENOMEM;
	}

	prPmkid->u4Length = 8 + sizeof(PARAM_BSSID_INFO_T);
	prPmkid->u4BSSIDInfoCount = 1;
	kalMemCopy(prPmkid->arBSSIDInfo->arBSSID, pmksa->bssid, 6);
	kalMemCopy(prPmkid->arBSSIDInfo->arPMKID, pmksa->pmkid, IW_PMKID_LEN);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "add pmkid error:%x\n", rStatus);
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

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prPmkid = (P_PARAM_PMKID_T) kalMemAlloc(8, VIR_MEM_TYPE);
	if (!prPmkid) {
		DBGLOG(REQ, ERROR, "Can not alloc memory for IW_PMKSA_FLUSH\n");
		return -ENOMEM;
	}

	prPmkid->u4Length = 8;
	prPmkid->u4BSSIDInfoCount = 0;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "flush pmkid error:%x\n", rStatus);
	kalMemFree(prPmkid, VIR_MEM_TYPE, 8);

	return 0;
}
#else
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
int mtk_cfg80211_set_pmksa(struct wiphy *wiphy,
		   struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	PARAM_PMKID_T pmkid;
	UINT_8 ucBssIndex = 0;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "mtk_cfg80211_set_pmksa " MACSTR " pmk\n",
		MAC2STR(pmksa->bssid));

	COPY_MAC_ADDR(pmkid.arBSSID, pmksa->bssid);
	kalMemCopy(pmkid.arPMKID, pmksa->pmkid, IW_PMKID_LEN);
	pmkid.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, &pmkid,
			   sizeof(PARAM_PMKID_T),
			   FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

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
int mtk_cfg80211_del_pmksa(struct wiphy *wiphy,
			struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	PARAM_PMKID_T pmkid;
	UINT_8 ucBssIndex = 0;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "mtk_cfg80211_del_pmksa " MACSTR "\n",
		MAC2STR(pmksa->bssid));

	COPY_MAC_ADDR(pmkid.arBSSID, pmksa->bssid);
	kalMemCopy(pmkid.arPMKID, pmksa->pmkid, IW_PMKID_LEN);
	pmkid.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidDelPmkid, &pmkid,
			   sizeof(PARAM_PMKID_T),
			   FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

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
int mtk_cfg80211_flush_pmksa(struct wiphy *wiphy,
			     struct net_device *ndev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo, wlanoidFlushPmkid, NULL, 0,
			   FALSE, FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "flush pmkid error:%x\n", rStatus);

	return 0;
}
#endif

void mtk_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
				      IN struct wireless_dev *wdev,
				      IN u16 frame_type, IN bool reg)
{
#if 0
	P_MSG_P2P_MGMT_FRAME_REGISTER_T prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) NULL;
#endif
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	do {

		DBGLOG(REQ, LOUD, "mtk_cfg80211_mgmt_frame_register\n");

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

		switch (frame_type) {
		case MAC_FRAME_PROBE_REQ:
			if (reg) {
				prGlueInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(REQ, LOUD, "Open packet filer probe request\n");
			} else {
				prGlueInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(REQ, LOUD, "Close packet filer probe request\n");
			}
			break;
		case MAC_FRAME_ACTION:
			if (reg) {
				prGlueInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(REQ, LOUD, "Open packet filer action frame.\n");
			} else {
				prGlueInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(REQ, LOUD, "Close packet filer action frame.\n");
			}
			break;
		default:
			DBGLOG(REQ, TRACE, "Ask frog to add code for mgmt:%x\n", frame_type);
			break;
		}

		if (prGlueInfo->prAdapter != NULL) {
			/* prGlueInfo->ulFlag |= GLUE_FLAG_FRAME_FILTER_AIS; */
			set_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT, &prGlueInfo->ulFlag);

			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);

			if (in_interrupt())
				DBGLOG(REQ, TRACE, "It is in interrupt level\n");
		}
#if 0

		prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) cnmMemAlloc(prGlueInfo->prAdapter,
										    RAM_TYPE_MSG,
										    sizeof
										    (MSG_P2P_MGMT_FRAME_REGISTER_T));

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
		if ((wiphy == NULL) || (wdev == NULL) || (chan == NULL) || (cookie == NULL))
			break;

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

#if 1
		DBGLOG(REQ, INFO, "--> %s()\n", __func__);
#endif

		*cookie = prGlueInfo->u8Cookie++;

		prMsgChnlReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_REMAIN_ON_CHANNEL_T));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}
		kalMemZero(prMsgChnlReq, sizeof(MSG_REMAIN_ON_CHANNEL_T));

		prMsgChnlReq->rMsgHdr.eMsgId = MID_MNY_AIS_REMAIN_ON_CHANNEL;
		prMsgChnlReq->u8Cookie = *cookie;
		prMsgChnlReq->u4DurationMs = duration;

		prMsgChnlReq->ucChannelNum = nicFreq2ChannelNum(chan->center_freq * 1000);

		switch (chan->band) {
		case IEEE80211_BAND_2GHZ:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		case IEEE80211_BAND_5GHZ:
			prMsgChnlReq->eBand = BAND_5G;
			break;
		default:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		}

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
		if ((wiphy == NULL) || (wdev == NULL))
			break;

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

#if 1
		DBGLOG(REQ, INFO, "--> %s()\n", __func__);
#endif

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
int
mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
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
#if 1
		DBGLOG(REQ, INFO, "--> %s()\n", __func__);
#endif

		if ((wiphy == NULL) || (wdev == NULL) || (params == 0) || (cookie == NULL))
			break;

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		*cookie = prGlueInfo->u8Cookie++;

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

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

	/* not implemented */

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for handling sched_scan start/stop request
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/

int
mtk_cfg80211_sched_scan_start(IN struct wiphy *wiphy,
			      IN struct net_device *ndev, IN struct cfg80211_sched_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 i, u4BufLen;
	P_PARAM_SCHED_SCAN_REQUEST prSchedScanRequest;
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	UINT_32 num = 0;
#endif

	DBGLOG(REQ, INFO, "--> %s() n_ssid:%d , match_set:%d\n", __func__, request->n_ssids, request->n_match_sets);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prBssInfo = &(prGlueInfo->prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	if ((!prGlueInfo->prAdapter->fgEnCfg80211Scan) &&
			(kalGetMediaStateIndicated(prGlueInfo) ==
			PARAM_MEDIA_STATE_CONNECTED)) {
		DBGLOG(SCN, WARN, "Disable cfg80211 sched scan.\n");
		return -EBUSY;
	}

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL || prGlueInfo->prSchedScanRequest != NULL) {
		DBGLOG(SCN, ERROR, "(prGlueInfo->prScanRequest != NULL || prGlueInfo->prSchedScanRequest != NULL)\n");
		return -EBUSY;
	} else if (request == NULL || request->n_match_sets > CFG_SCAN_SSID_MATCH_MAX_NUM) {
		DBGLOG(SCN, ERROR, "(request == NULL || request->n_match_sets > CFG_SCAN_SSID_MATCH_MAX_NUM)\n");
		/* invalid scheduled scan request */
		return -EINVAL;
	} else if (/* !request->n_ssids || */!request->n_match_sets) {
		/* invalid scheduled scan request */
		return -EINVAL;
	}
#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	else if (!request->n_ssids || request->n_ssids > CFG_SCAN_HIDDEN_SSID_MAX_NUM) {
		/* invalid scheduled scan request */
		return -EINVAL;
	}
#endif

	prSchedScanRequest = (P_PARAM_SCHED_SCAN_REQUEST) kalMemAlloc(sizeof(PARAM_SCHED_SCAN_REQUEST), VIR_MEM_TYPE);
	if (prSchedScanRequest == NULL) {
		DBGLOG(SCN, ERROR, "(prSchedScanRequest == NULL) kalMemAlloc fail\n");
		return -ENOMEM;
	}

	kalMemZero(prSchedScanRequest, sizeof(PARAM_SCHED_SCAN_REQUEST));

#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
	/*passed in the probe_reqs in active scans*/
	for (i = 0; i < request->n_ssids; i++) {
		if (request->ssids && (&(request->ssids[i]))) {
			DBGLOG(SCN, TRACE, "ssids : (%d)[%s]\n", i, HIDE(request->ssids[i].ssid));
			/*driver ingored the null ssid*/
			if (request->ssids[i].ssid_len == 0)
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


	if (kalSchedScanParseRandomMac(prBssInfo, request,
		prSchedScanRequest->aucRandomMac, prSchedScanRequest->aucRandomMacMask)) {
		prSchedScanRequest->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	}


	for (i = 0; i < request->n_match_sets; i++) {
		if (request->match_sets && (&(request->match_sets[i]))) {
			DBGLOG(SCN, TRACE, "match : (%d)[%s]\n", i,
				HIDE(request->match_sets[i].ssid.ssid));
			/*driver ingored the null ssid*/
			if (request->match_sets[i].ssid.ssid_len == 0)
				DBGLOG(SCN, WARN, "ignore the null ssid, index:%d\n", i);
			else {
				COPY_SSID(prSchedScanRequest->arMatchSsid[num].aucSsid,
					  prSchedScanRequest->arMatchSsid[num].u4SsidLen,
					  request->match_sets[i].ssid.ssid, request->match_sets[i].ssid.ssid_len);
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
		}
	}
#endif
	prSchedScanRequest->u4IELength = request->ie_len;
	if (request->ie_len > 0)
		prSchedScanRequest->pucIE = (PUINT_8) (request->ie);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	prSchedScanRequest->u2ScanInterval = (UINT_16) (request->scan_plans[0].interval);
#else
	prSchedScanRequest->u2ScanInterval = (UINT_16) (request->interval);
#endif

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetStartSchedScan,
			   prSchedScanRequest, sizeof(PARAM_SCHED_SCAN_REQUEST), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	kalMemFree(prSchedScanRequest, VIR_MEM_TYPE, sizeof(PARAM_SCHED_SCAN_REQUEST));

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(SCN, ERROR, "scheduled scan error:%x\n", rStatus);
		return -EINVAL;
	}

	prGlueInfo->prSchedScanRequest = request;

	return 0;
}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy,
				 IN struct net_device *ndev,
				 IN u64 reqid)
#else
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy, IN struct net_device *ndev)
#endif
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prSchedScanRequest == NULL) {
		DBGLOG(SCN, ERROR, "prGlueInfo->prSchedScanRequest == NULL\n");
		return -EBUSY;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetStopSchedScan, NULL, 0, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(SCN, ERROR, "scheduled scan error, rStatus: %d\n", rStatus);
		return -EINVAL;
	}

	/* 1. reset first for newly incoming request */
	/* GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV); */
	if (prGlueInfo->prSchedScanRequest != NULL)
		prGlueInfo->prSchedScanRequest = NULL;
	/* GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV); */

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
#if CFG_SUPPORT_HOTSPOT_2_0
	PUINT_8 prDesiredIE = NULL;
#endif
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, req->bss->bssid)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN, "incorrect BSSID: [ %pM ] currently connected BSSID[ %pM ]\n",
				   req->bss->bssid, arBssid);
		return -ENOENT;
	}

	if (req->ie && req->ie_len > 0) {
#if CFG_SUPPORT_HOTSPOT_2_0
		if (wextSrchDesiredHS20IE((PUINT_8) req->ie, req->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* Do nothing */
				/* printk(KERN_INFO "[HS20] set HS20 assoc info error:%lx\n", rStatus); */
			}
		}
#endif
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssid,
			   (PVOID) req->bss->bssid, MAC_ADDR_LEN, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set BSSID:%x\n", rStatus);
		return -EINVAL;
	}

	return 0;
}

#if CONFIG_NL80211_TESTMODE
/*
 * #define NLA_PUT(skb, attrtype, attrlen, data) \
 * do { \
 *     if (unlikely(nla_put(skb, attrtype, attrlen, data) < 0)) \
 *         goto nla_put_failure; \
 * } while (0)
 *
 * #define NLA_PUT_TYPE(skb, type, attrtype, value) \
 * do { \
 *    type __tmp = value; \
 *        NLA_PUT(skb, attrtype, sizeof(type), &__tmp); \
 * } while (0)
 *
 * #define NLA_PUT_U8(skb, attrtype, value) \
 *    NLA_PUT_TYPE(skb, u8, attrtype, value)
 *
 * #define NLA_PUT_U16(skb, attrtype, value) \
 *    NLA_PUT_TYPE(skb, u16, attrtype, value)
 *
 * #define NLA_PUT_U32(skb, attrtype, value) \
 *    NLA_PUT_TYPE(skb, u32, attrtype, value)
 *
 * #define NLA_PUT_U64(skb, attrtype, value) \
 *    NLA_PUT_TYPE(skb, u64, attrtype, value)
 */
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

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	if (data && len) {
		prParams = (P_NL80211_DRIVER_SET_KEY_EXTS) data;
	} else {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_set_key_ext, data is NULL\n");
		return -EINVAL;
	}

	if (prParams)
		prIWEncExt = (struct iw_encode_exts *)&prParams->ext;

	if (prIWEncExt->alg == IW_ENCODE_ALG_SMS4) {
		/* KeyID */
		prWpiKey->ucKeyID = prParams->key_index;
		prWpiKey->ucKeyID--;
		if (prWpiKey->ucKeyID > 1) {
			/* key id is out of range */
			/* printk(KERN_INFO "[wapi] add key error: key_id invalid %d\n", prWpiKey->ucKeyID); */
			return -EINVAL;
		}

		if (prIWEncExt->key_len != 32) {
			/* key length not valid */
			/* printk(KERN_INFO "[wapi] add key error: key_len invalid %d\n", prIWEncExt->key_len); */
			return -EINVAL;
		}
		/* printk(KERN_INFO "[wapi] %d ext_flags %d\n", prEnc->flags, prIWEncExt->ext_flags); */

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
				   wlanoidSetWapiKey,
				   prWpiKey, sizeof(PARAM_WPI_KEY_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

		if (rstatus != WLAN_STATUS_SUCCESS) {
			/* printk(KERN_INFO "[wapi] add key error:%lx\n", rStatus); */
			fgIsValid = -EFAULT;
		}

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

	P_NL80211_DRIVER_GET_STA_STATISTICS_PARAMS prParams = NULL;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(QM, INFO, "--> %s()\n", __func__);

	if (data && len) {
		prParams = (P_NL80211_DRIVER_GET_STA_STATISTICS_PARAMS) data;
	} else {
		DBGLOG(QM, ERROR, "mtk_cfg80211_testmode_get_sta_statistics, data is NULL\n");
		return -EINVAL;
	}
	/*
	 * if (!prParams->aucMacAddr) {
	 *     DBGLOG(QM, INFO, "%s MAC Address is NULL\n", __func__);
	 *     return -EINVAL;
	 * }
	 */
	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(PARAM_GET_STA_STA_STATISTICS) + 1);

	if (!skb) {
		DBGLOG(QM, ERROR, "%s allocate skb failed:%x\n", __func__, rStatus);
		return -ENOMEM;
	}

	DBGLOG(QM, TRACE, "Get [ %pM ] STA statistics\n", prParams->aucMacAddr);

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
	COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, prParams->aucMacAddr);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStaStatistics,
			   &rQueryStaStatistics, sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, TRUE, &u4BufLen);

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

	/*NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_INVALID, 0);*/
	{
		unsigned char __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_INVALID, sizeof(unsigned char), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_VERSION, NL80211_DRIVER_TESTMODE_VERSION);*/
	{
		unsigned char __tmp = NL80211_DRIVER_TESTMODE_VERSION;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_VERSION, sizeof(unsigned char), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE, u4LinkScore); */
	{
		unsigned int __tmp = u4LinkScore;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT(skb, NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN, prParams->aucMacAddr);*/
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN, &prParams->aucMacAddr) < 0))
		goto nla_put_failure;

	/*NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_FLAG, rQueryStaStatistics.u4Flag);*/
	{
		unsigned int __tmp = rQueryStaStatistics.u4Flag;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_FLAG,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}


	/* FW part STA link status */
	/*NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_PER, rQueryStaStatistics.ucPer);*/
	{
		unsigned char __tmp = rQueryStaStatistics.ucPer;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_PER, sizeof(unsigned char), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_RSSI, rQueryStaStatistics.ucRcpi);*/
	{
		unsigned char __tmp = rQueryStaStatistics.ucRcpi;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_RSSI, sizeof(unsigned char), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_PHY_MODE, rQueryStaStatistics.u4PhyMode);*/
	{
		unsigned int __tmp = rQueryStaStatistics.u4PhyMode;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_PHY_MODE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT_U16(skb, NL80211_TESTMODE_STA_STATISTICS_TX_RATE, rQueryStaStatistics.u2LinkSpeed);*/
	{
		unsigned short __tmp = rQueryStaStatistics.u2LinkSpeed;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TX_RATE,
			sizeof(unsigned short), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT, rQueryStaStatistics.u4TxFailCount);*/
	{
		unsigned int __tmp = rQueryStaStatistics.u4TxFailCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT, rQueryStaStatistics.u4TxLifeTimeoutCount);*/
	{
		unsigned int __tmp = rQueryStaStatistics.u4TxLifeTimeoutCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME, rQueryStaStatistics.u4TxAverageAirTime);*/
	{
		unsigned int __tmp = rQueryStaStatistics.u4TxAverageAirTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Driver part link status */
	/*NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT, rQueryStaStatistics.u4TxTotalCount);*/
	{
		unsigned int __tmp = rQueryStaStatistics.u4TxTotalCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT,
	 *    rQueryStaStatistics.u4TxExceedThresholdCount);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.u4TxExceedThresholdCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME,
	 * rQueryStaStatistics.u4TxAverageProcessTime);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.u4TxAverageProcessTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		unsigned int __tmp = rQueryStaStatistics.u4TxMaxTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_MAX_PROCESS_TIME,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}


	{
		unsigned int __tmp = rQueryStaStatistics.u4TxAverageHifTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_HIF_PROCESS_TIME,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		unsigned int __tmp = rQueryStaStatistics.u4TxMaxHifTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_MAX_HIF_PROCESS_TIME,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_ENQUEUE, rQueryStaStatistics.u4EnqueueCounter);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.u4EnqueueCounter;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_ENQUEUE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_DEQUEUE, rQueryStaStatistics.u4DequeueCounter);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.u4DequeueCounter;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_DEQUEUE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_STA_ENQUEUE, rQueryStaStatistics.u4EnqueueStaCounter);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.u4EnqueueStaCounter;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_STA_ENQUEUE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_STA_DEQUEUE, rQueryStaStatistics.u4DequeueStaCounter);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.u4DequeueStaCounter;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_STA_DEQUEUE,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_CNT, rQueryStaStatistics.IsrCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.IsrCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_PASS_CNT, rQueryStaStatistics.IsrPassCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.IsrPassCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_PASS_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_TASK_CNT, rQueryStaStatistics.TaskIsrCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.TaskIsrCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_TASK_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}

	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_AB_CNT, rQueryStaStatistics.IsrAbnormalCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.IsrAbnormalCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_AB_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_SW_CNT, rQueryStaStatistics.IsrSoftWareCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.IsrSoftWareCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_SW_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 * NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_TX_CNT, rQueryStaStatistics.IsrTxCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.IsrTxCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_TX_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/*
	 *NLA_PUT_U32(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_RX_CNT, rQueryStaStatistics.IsrRxCnt);
	 */
	{
		unsigned int __tmp = rQueryStaStatistics.IsrRxCnt;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_IRQ_RX_CNT,
			sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}
	/* Network counter */
	/*
	 * NLA_PUT(skb,
	 * NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
	 * sizeof(rQueryStaStatistics.au4TcResourceEmptyCount), rQueryStaStatistics.au4TcResourceEmptyCount);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
		sizeof(rQueryStaStatistics.au4TcResourceEmptyCount), &rQueryStaStatistics.au4TcResourceEmptyCount) < 0))
		goto nla_put_failure;
	/*
	 * NLA_PUT(skb,
	 *      NL80211_TESTMODE_STA_STATISTICS_NO_TC_ARRAY,
	 *      sizeof(rQueryStaStatistics.au4DequeueNoTcResource), rQueryStaStatistics.au4DequeueNoTcResource);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_NO_TC_ARRAY,
		sizeof(rQueryStaStatistics.au4DequeueNoTcResource), &rQueryStaStatistics.au4DequeueNoTcResource) < 0))
		goto nla_put_failure;
	/*
	 * NLA_PUT(skb,
	 * NL80211_TESTMODE_STA_STATISTICS_RB_ARRAY,
	 * sizeof(rQueryStaStatistics.au4TcResourceBackCount), rQueryStaStatistics.au4TcResourceBackCount);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_RB_ARRAY,
		sizeof(rQueryStaStatistics.au4TcResourceBackCount), &rQueryStaStatistics.au4TcResourceBackCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_USED_BFCT_ARRAY,
		sizeof(rQueryStaStatistics.au4TcResourceUsedCount), &rQueryStaStatistics.au4TcResourceUsedCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_WANTED_BFCT_ARRAY,
		sizeof(rQueryStaStatistics.au4TcResourceWantedCount),
		&rQueryStaStatistics.au4TcResourceWantedCount) < 0))
		goto nla_put_failure;

	/* Sta queue length */
	/*
	 * NLA_PUT(skb,
	 * NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,
	 * sizeof(rQueryStaStatistics.au4TcQueLen), rQueryStaStatistics.au4TcQueLen);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,
		sizeof(rQueryStaStatistics.au4TcQueLen), &rQueryStaStatistics.au4TcQueLen) < 0))
		goto nla_put_failure;


	/* Global QM counter */
	/*
	 * NLA_PUT(skb,
	 *     NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
	 *     sizeof(rQueryStaStatistics.au4TcAverageQueLen), rQueryStaStatistics.au4TcAverageQueLen);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
		sizeof(rQueryStaStatistics.au4TcAverageQueLen), &rQueryStaStatistics.au4TcAverageQueLen) < 0))
		goto nla_put_failure;

	/*
	 * NLA_PUT(skb,
	 *     NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,
	 *     sizeof(rQueryStaStatistics.au4TcCurrentQueLen), rQueryStaStatistics.au4TcCurrentQueLen);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,
		sizeof(rQueryStaStatistics.au4TcCurrentQueLen), &rQueryStaStatistics.au4TcCurrentQueLen) < 0))
		goto nla_put_failure;


	/* Reserved field */
	/*
	 * NLA_PUT(skb,
	 * NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,
	 * sizeof(rQueryStaStatistics.au4Reserved), rQueryStaStatistics.au4Reserved);
	 */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,
		sizeof(rQueryStaStatistics.au4Reserved), &rQueryStaStatistics.au4Reserved) < 0))
		goto nla_put_failure;

	i4Status = cfg80211_testmode_reply(skb);
	skb = NULL;

nla_put_failure:
	if (skb != NULL)
		kfree_skb(skb);
	return i4Status;
}

int
mtk_cfg80211_testmode_set_packet_filter(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = rStatus;
	UINT_32 u4BufLen = 0;
	P_NL80211_DRIVER_RXFILTER_PARAMS prParams = NULL;

	PACKET_DROP_T cmdBuf;
	P_PACKET_DROP_T pCmdHeader = NULL;
	P_PACKET_DROP_SETTING_V1_T pCmdData = NULL;
	UINT_64 u64Filter = 0;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	if (data && len)
		prParams = (P_NL80211_DRIVER_RXFILTER_PARAMS)data;
	else {
		DBGLOG(QM, ERROR, "mtk_cfg80211_testmode_set_packet_filter, data is NULL or len is 0\n");
		return -EINVAL;
	}


	kalMemZero(&cmdBuf, sizeof(cmdBuf));

	pCmdHeader = &cmdBuf;
	pCmdHeader->cmdVersion = 0;
	pCmdHeader->cmdType = 0;
	pCmdHeader->magicCode = 0x72;
	pCmdHeader->cmdBufferLen = MAX_PACKET_DROP_LENGTH;

	pCmdData = (P_PACKET_DROP_SETTING_V1_T)&(pCmdHeader->buffer[0]);

	u64Filter = prParams->Ipv4FilterHigh;
	u64Filter = u64Filter<<32;
	u64Filter &= 0xffffffff00000000;
	pCmdData->Drop_IPv4.bytes = u64Filter|prParams->Ipv4FilterLow;

	/*bit0~bit8
	*
	*pCmdData->Drop_IPv4.bytes |= 0xFE;
	*struct {
	*			UINT_64    all:1;
	*			UINT_64    MDNS:1;
	*			UINT_64    LLMNR:1;
	*			UINT_64    BROWSER:1;
	*			UINT_64    CAPWAP:1;
	*			UINT_64    DNS:1;
	*			UINT_64    NBNS:1;
	*			UINT_64    SSDP:1;
	*			UINT_64    others:1;
	*		} UDPbits;
	*/

	u64Filter = prParams->Ipv6FilterHigh;
	u64Filter = u64Filter<<32;
	u64Filter &= 0xffffffff00000000;
	pCmdData->Drop_IPv6.bytes = u64Filter|prParams->Ipv6FilterLow;

	/* bit0 only
	*pCmdData->Drop_IPv6.bytes |= 0x01;
	*struct {
	*		UINT_64    all:1;
	*		} bits;
	*/

	u64Filter = prParams->SnapFilterHigh;
	u64Filter = u64Filter<<32;
	u64Filter &= 0xffffffff00000000;
	pCmdData->Drop_SNAP.bytes = u64Filter|prParams->SnapFilterLow;

	/*bit0~bit4
	*pCmdData->Drop_SNAP.bytes |= 0x0E;
	* struct {
	*			UINT_64    all:1;
	*			UINT_64    CDP:1;
	*			UINT_64    STP:1;
	*			UINT_64    XID:1;
	*			UINT_64    others:1;
	*		} bits;
	*/
	pr_info("wlan_gen2: mtk_cfg80211_testmode_set_packet_filter\n Drop_IPv4(%02llx)\nDrop_IPv6(%02llx)\nDrop_SNAP(%02llx)\n",
		pCmdData->Drop_IPv4.bytes, pCmdData->Drop_IPv6.bytes, pCmdData->Drop_SNAP.bytes);

	rStatus = kalIoctl(prGlueInfo,
					wlanoidSetRxPacketFilterPriv,
					&cmdBuf,
					sizeof(PACKET_DROP_T),
					FALSE,
					FALSE,
					TRUE,
					FALSE,
					&u4BufLen);

	/*printk("rStatus = %08x\n",rStatus);*/

	if (rStatus != WLAN_STATUS_SUCCESS)
		i4Status = -EFAULT;

	return i4Status;
}

int
mtk_cfg80211_testmode_get_link_detection(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{

	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = -EINVAL;
	UINT_32 u4BufLen;

	PARAM_802_11_STATISTICS_STRUCT_T rStatistics;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(QM, INFO, "--> %s()\n", __func__);

	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(PARAM_GET_STA_STA_STATISTICS) + 1);

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__, rStatus);
		return -ENOMEM;
	}

	kalMemZero(&rStatistics, sizeof(rStatistics));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStatistics,
			   &rStatistics, sizeof(rStatistics), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	/* NLA_PUT_U8(skb, NL80211_TESTMODE_STA_STATISTICS_INVALID, 0); */
	{
		unsigned char __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_INVALID, sizeof(unsigned char), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_FAIL_CNT, rStatistics.rFailedCount.QuadPart); */
	{
		u64 __tmp = rStatistics.rFailedCount.QuadPart;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_LINK_TX_FAIL_CNT,
			sizeof(u64), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_RETRY_CNT, rStatistics.rRetryCount.QuadPart); */
	{
		u64 __tmp = rStatistics.rFailedCount.QuadPart;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_LINK_TX_RETRY_CNT,
			sizeof(u64), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_MULTI_RETRY_CNT, rStatistics.rMultipleRetryCount.QuadPart); */
	{
		u64 __tmp = rStatistics.rMultipleRetryCount.QuadPart;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_LINK_TX_MULTI_RETRY_CNT,
			sizeof(u64), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_ACK_FAIL_CNT, rStatistics.rACKFailureCount.QuadPart); */
	{
		u64 __tmp = rStatistics.rACKFailureCount.QuadPart;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_LINK_ACK_FAIL_CNT,
			sizeof(u64), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_FCS_ERR_CNT, rStatistics.rFCSErrorCount.QuadPart); */
	{
		u64 __tmp = rStatistics.rFCSErrorCount.QuadPart;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_LINK_FCS_ERR_CNT,
			sizeof(u64), &__tmp) < 0))
			goto nla_put_failure;
	}


	i4Status = cfg80211_testmode_reply(skb);
	skb = NULL;

nla_put_failure:
	if (skb != NULL)
		kfree_skb(skb);
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

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

	if (data && len)
		prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) data;

	if (prParams) {
		if (prParams->set == 1) {
			rstatus = kalIoctl(prGlueInfo,
					   (PFN_OID_HANDLER_FUNC) wlanoidSetSwCtrlWrite,
					   &prParams->adr, (UINT_32) 8, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
		}
	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

#if CFG_SUPPORT_HOTSPOT_2_0
int mtk_cfg80211_testmode_hs20_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct wpa_driver_hs20_data_s *prParams = NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	if (data && len) {
		prParams = (struct wpa_driver_hs20_data_s *)data;

		DBGLOG(REQ, TRACE, "[%s] Cmd Type (%d)\n", __func__, prParams->CmdType);
	}

	if (prParams) {
		int i;

		switch (prParams->CmdType) {
		case HS20_CMD_ID_SET_BSSID_POOL:
			DBGLOG(REQ, TRACE, "fgBssidPoolIsEnable=%d, ucNumBssidPool=%d\n",
				prParams->hs20_set_bssid_pool.fgBssidPoolIsEnable,
				prParams->hs20_set_bssid_pool.ucNumBssidPool);
			for (i = 0; i < prParams->hs20_set_bssid_pool.ucNumBssidPool; i++) {
				DBGLOG(REQ, TRACE, "[%d][ %pM ]\n", i,
					(prParams->hs20_set_bssid_pool.arBssidPool[i]));
			}
			rstatus = kalIoctl(prGlueInfo,
					(PFN_OID_HANDLER_FUNC) wlanoidSetHS20BssidPool,
					&prParams->hs20_set_bssid_pool,
					sizeof(struct param_hs20_set_bssid_pool),
					FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
			break;
		default:
			DBGLOG(REQ, TRACE, "[%s] Unknown Cmd Type (%d)\n", __func__, prParams->CmdType);
			rstatus = WLAN_STATUS_FAILURE;

		}

	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}
#endif

int mtk_cfg80211_testmode_set_poorlink_param(IN struct wiphy *wiphy, IN void *data, IN int len,
					      IN P_GLUE_INFO_T prGlueInfo)
{
	int fgIsValid = 0;
	P_NL80211_DRIVER_POORLINK_PARAMS prParams = NULL;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	if (data && len) {
		prParams = (P_NL80211_DRIVER_POORLINK_PARAMS) data;
	} else {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_set_poorlink_param, data is NULL\n");
		return -EINVAL;
	}
	if (prParams->ucLinkSpeed)
		prGlueInfo->u4LinkspeedThreshold = prParams->ucLinkSpeed * 10;
	if (prParams->cRssi)
		prGlueInfo->i4RssiThreshold = prParams->cRssi;
	if (!prGlueInfo->fgPoorlinkValid)
		prGlueInfo->fgPoorlinkValid = 1;
#if 0
	DBGLOG(REQ, TRACE, "poorlink set param valid(%d)rssi(%d)linkspeed(%d)\n",
	       prGlueInfo->fgPoorlinkValid, prGlueInfo->i4RssiThreshold, prGlueInfo->u4LinkspeedThreshold);
#endif

	return fgIsValid;

}

int mtk_cfg80211_testmode_cmd(IN struct wiphy *wiphy, IN struct wireless_dev *wdev, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_MODE_PARAMS prParams = NULL;
	INT_32 i4Status = -EINVAL;


	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	if (data && len) {
		prParams = (P_NL80211_DRIVER_TEST_MODE_PARAMS) data;
	} else {
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
	case TESTMODE_CMD_ID_SUSPEND:
		{
			P_NL80211_DRIVER_SUSPEND_PARAMS prParams = (P_NL80211_DRIVER_SUSPEND_PARAMS) data;

			if (prParams->suspend == 1) {
				wlanHandleSystemSuspend();
				if (prGlueInfo->prAdapter->fgIsP2PRegistered)
					p2pHandleSystemSuspend();
				i4Status = 0;
			} else if (prParams->suspend == 0) {
				wlanHandleSystemResume();
				if (prGlueInfo->prAdapter->fgIsP2PRegistered)
					p2pHandleSystemResume();
				i4Status = 0;
			}
			break;
		}
	case TESTMODE_CMD_ID_RXFILTER:
		i4Status = mtk_cfg80211_testmode_set_packet_filter(wiphy, data, len, prGlueInfo);
		break;
	case TESTMODE_CMD_ID_STATISTICS:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
		break;
	case TESTMODE_CMD_ID_LINK_DETECT:
		i4Status = mtk_cfg80211_testmode_get_link_detection(wiphy, data, len, prGlueInfo);
		break;
	case TESTMODE_CMD_ID_POORLINK:
		i4Status = mtk_cfg80211_testmode_set_poorlink_param(wiphy, data, len, prGlueInfo);
		break;

#if CFG_SUPPORT_HOTSPOT_2_0
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
		DBGLOG(REQ, INFO, "--> %s() prParams->index=%d, status=%d\n"
			, __func__, prParams->index, i4Status);

	return i4Status;
}

int mtk_cfg80211_testmode_get_scan_done(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{
#define NL80211_TESTMODE_P2P_SCANDONE_INVALID 0
#define NL80211_TESTMODE_P2P_SCANDONE_STATUS 1
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = -EINVAL, READY_TO_BEAM = 0;

	/* P_NL80211_DRIVER_GET_STA_STATISTICS_PARAMS prParams = NULL; */
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(QM, INFO, "--> %s()\n", __func__);

	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(UINT_32));
	READY_TO_BEAM =
	    (UINT_32) (prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.
		       fgIsGOInitialDone) &
	    (!prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsScanRequest);
	DBGLOG(QM, TRACE,
	       "NFC:GOInitialDone[%d] and P2PScanning[%d]\n",
		prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsGOInitialDone,
		prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo.fgIsScanRequest);

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__, rStatus);
		return -ENOMEM;
	}

	/* NLA_PUT_U8(skb, NL80211_TESTMODE_P2P_SCANDONE_INVALID, 0); */
	{
		unsigned char __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_INVALID, sizeof(unsigned char), &__tmp) < 0))
			goto nla_put_failure;
	}
	/* NLA_PUT_U32(skb, NL80211_TESTMODE_P2P_SCANDONE_STATUS, READY_TO_BEAM); */
	{
		unsigned int __tmp = READY_TO_BEAM;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_STATUS, sizeof(unsigned int), &__tmp) < 0))
			goto nla_put_failure;
	}


	i4Status = cfg80211_testmode_reply(skb);
	skb = NULL;

nla_put_failure:
	if (skb != NULL)
		kfree_skb(skb);
	return i4Status;
}

#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief cfg80211 suspend callback, will be invoked in wiphy_suspend
 *
 * @param wiphy: pointer to wiphy
 *		wow: pointer to cfg80211_wowlan
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int	mtk_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(REQ, TRACE, "--> %s()\n", __func__);

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

	DBGLOG(SCN, TRACE, "--> %s()\n", __func__);

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

	if (i > 0) {
		DBGLOG(SCN, INFO, "pending %d sched scan results\n", i);
		kalMemZero(&pprBssDesc[0], i * sizeof(P_BSS_DESC_T));
	}
end:
	kalHaltUnlock();
	return 0;
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
					FALSE, FALSE, FALSE, &u4InfoBufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "update Ft IE failed\n");
	return 0;
}

INT_32 mtk_cfg80211_process_str_cmd(P_GLUE_INFO_T prGlueInfo, PUINT_8 cmd, INT_32 len)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4SetInfoLen = 0;

	DBGLOG(REQ, INFO, "--> %s()\n", __func__);

	if (kalStrniCmp(cmd, "tdls-ps ", 8) == 0) {
#if CFG_SUPPORT_TDLS
		DBGLOG(TDLS, INFO, "disable tdls-ps\n");
		rStatus = kalIoctl(prGlueInfo,
					   wlanoidDisableTdlsPs,
					   (PVOID)(cmd+8), 1, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
#else
		DBGLOG(REQ, WARN, "not support tdls\n");
		return -EOPNOTSUPP;
#endif
	} else if (kalStrniCmp(cmd, "SETALWAYSSCANSTATE ", 19) == 0) {

		rStatus = kalIoctl(prGlueInfo,
					wlanoidSetAlwaysScan,
					(PVOID)(cmd+19),
					sizeof(UINT_8),
					FALSE,
					FALSE,
					TRUE,
					FALSE,
					&u4SetInfoLen);
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
						FALSE,
						&u4SetInfoLen);
		if (rStatus == WLAN_STATUS_SUCCESS && prGlueInfo) {
			P_ADAPTER_T prAdapter = prGlueInfo->prAdapter;

			if (prAdapter) {
				prAdapter->fgEnOshareMode = pCmdData->osharemode;
				DBGLOG(REQ, INFO, "Set osharemode=%u\n", prAdapter->fgEnOshareMode);
			}
		}
#else
		DBGLOG(REQ, WARN, "not support OSHAREMOD\n");
		return -EOPNOTSUPP;
#endif
	} else if (kalStrniCmp(cmd, "NEIGHBOR-REQUEST", 16) == 0) {
		PUINT_8 pucSSID = NULL;
		UINT_32 u4SSIDLen = 0;

		if (len > 16 && (kalStrniCmp(cmd+16, " SSID=", 6) == 0)) {
			pucSSID = cmd + 22;
			u4SSIDLen = len - 22;
			DBGLOG(REQ, INFO, "cmd=%s, ssid len %u, ssid=%s\n", cmd, u4SSIDLen, HIDE(pucSSID));
		}
		rStatus = kalIoctl(prGlueInfo, wlanoidSendNeighborRequest,
				   (PVOID)pucSSID, u4SSIDLen, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
	} else if (kalStrniCmp(cmd, "BSS-TRANSITION-QUERY", 20) == 0) {
		PUINT_8 pucReason = NULL;

		if (len > 20 && (kalStrniCmp(cmd+20, " reason=", 8) == 0))
			pucReason = cmd + 28;
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSendBTMQuery,
				   (PVOID)pucReason, 1, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

	} else
		return -EOPNOTSUPP;

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;

	return -EINVAL;
}

