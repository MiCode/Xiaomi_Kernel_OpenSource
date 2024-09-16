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
** Id: @(#) gl_cfg80211.c@@
*/

/*! \file   gl_cfg80211.c
*    \brief  Main routines for supporintg MT6620 cfg80211 control interface
*
*    This file contains the support routines of Linux driver for MediaTek Inc. 802.11
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
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_change_iface(struct wiphy *wiphy,
			  struct net_device *ndev,
			  enum nl80211_iftype type,
			  struct vif_params *params)
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_change_iface do nothing\n");
		return -EINVAL;
	}
#endif

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

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set infrastructure mode error:0x%x\n",
			rStatus);

	/* reset wpa info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo.ucRSNMfpCap = 0;
	prGlueInfo->rWpaInfo.u4CipherGroupMgmt = IW_AUTH_CIPHER_NONE;
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
#if CFG_SUPPORT_REPLAY_DETECTION
	P_BSS_INFO_T prBssInfo = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	UINT_8 ucCheckZeroKey = 0;
	UINT_8 i = 0;
#endif

	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	/* const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR; */

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(RSN, ERROR,
			"chip resetting, mtk_cfg80211_add_key do nothing\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if DBG
	DBGLOG(RSN, INFO, "mtk_cfg80211_add_key\n");
	if (mac_addr) {
		DBGLOG(RSN, INFO,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, INFO, "keyIdx = %d pairwise = %d null mac\n", key_index, pairwise);
	}
	DBGLOG(RSN, INFO, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, INFO, params->key, params->key_len);
#endif

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
#if 0
		case WLAN_CIPHER_SUITE_WEP128:
			rKey.ucCipher = CIPHER_SUITE_WEP128;
			break;
#endif
		case WLAN_CIPHER_SUITE_TKIP:
			rKey.ucCipher = CIPHER_SUITE_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			rKey.ucCipher = CIPHER_SUITE_CCMP;
			break;
#if 0
		case WLAN_CIPHER_SUITE_GCMP:
			rKey.ucCipher = CIPHER_SUITE_GCMP;
			break;
		case WLAN_CIPHER_SUITE_CCMP_256:
			rKey.ucCipher = CIPHER_SUITE_CCMP256;
			break;
#endif
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

	if (pairwise) {
		ASSERT(mac_addr);
		rKey.u4KeyIndex |= BIT(31);
		rKey.u4KeyIndex |= BIT(30);
		COPY_MAC_ADDR(rKey.arBSSID, mac_addr);

		/* reset KCK, KEK, EAPOL Replay counter */
		kalMemZero(prGlueInfo->rWpaInfo.aucKek, NL80211_KEK_LEN);
		kalMemZero(prGlueInfo->rWpaInfo.aucKck, NL80211_KCK_LEN);
		kalMemZero(prGlueInfo->rWpaInfo.aucReplayCtr, NL80211_REPLAY_CTR_LEN);

	} else {		/* Group key */
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	if (params->key) {

#if CFG_SUPPORT_REPLAY_DETECTION
		for (i = 0; i < params->key_len; i++) {
			if (params->key[i] == 0x00)
				ucCheckZeroKey++;
		}

		if (ucCheckZeroKey == params->key_len)
			return 0;
#endif
		if (params->key_len > sizeof(rKey.aucKeyMaterial))
			return -EINVAL;

		kalMemCopy(rKey.aucKeyMaterial, params->key, params->key_len);
		if (rKey.ucCipher == CIPHER_SUITE_TKIP) {
			kalMemCopy(tmp1, &params->key[16], 8);
			kalMemCopy(tmp2, &params->key[24], 8);
			kalMemCopy(&rKey.aucKeyMaterial[16], tmp2, 8);
			kalMemCopy(&rKey.aucKeyMaterial[24], tmp1, 8);
		}
	}

	rKey.ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = ((ULONG)&(((P_PARAM_KEY_T) 0)->aucKeyMaterial)) + rKey.u4KeyLength;

#if CFG_SUPPORT_REPLAY_DETECTION
	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);

	prDetRplyInfo = &prBssInfo->rDetRplyInfo;

	if ((!pairwise) && ((params->cipher == WLAN_CIPHER_SUITE_TKIP) || (params->cipher == WLAN_CIPHER_SUITE_CCMP))) {
		if ((prDetRplyInfo->ucCurKeyId == key_index) &&
			(!kalMemCmp(prDetRplyInfo->aucKeyMaterial, params->key, params->key_len))) {
			DBGLOG(RSN, TRACE, "M3/G1, KeyID and KeyValue equal.\n");
			DBGLOG(RSN, TRACE, "hit group key reinstall case, so no update BC/MC PN.\n");
		} else {
			kalMemCopy(prDetRplyInfo->arReplayPNInfo[key_index].auPN, params->seq, params->seq_len);
			prDetRplyInfo->ucCurKeyId = key_index;
			prDetRplyInfo->u4KeyLength = params->key_len;
			kalMemCopy(prDetRplyInfo->aucKeyMaterial, params->key, params->key_len);
		}

		prDetRplyInfo->fgKeyRscFresh = TRUE;
	}
#endif

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
		     const u8 *mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *)
)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_cfg80211_get_key\n");
#endif
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

#if CFG_CHIP_RESET_SUPPORT
	if (g_u4HaltFlag) {
		DBGLOG(INIT, WARN, "wlan is halt, skip key deletion\n");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

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
	rRemoveKey.u4KeyIndex = key_index;
	rRemoveKey.u4Length = sizeof(PARAM_REMOVE_KEY_T);
	if (mac_addr) {
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
		rRemoveKey.u4KeyIndex |= BIT(30);
	}
	if ((prGlueInfo->prAdapter == NULL) || (prGlueInfo->prAdapter
		->prAisBssInfo == NULL))
		return i4Rslt;

	rRemoveKey.ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRemoveKey, &rRemoveKey, rRemoveKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(RSN, WARN, "remove key error:0x%x\n", rStatus);
	else
		i4Rslt = 0;

#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_set_default_key\n");
		return -EINVAL;
	}
#endif
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* For STA, should wep set the default key !! */
#if DBG
	DBGLOG(RSN, INFO, "mtk_cfg80211_set_default_key\n");
	DBGLOG(RSN, INFO, "keyIdx = %d unicast = %d multicast = %d\n", key_index, unicast, multicast);
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

	rDefaultKey.ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

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
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip get station\n");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, " entry_conut = %d\n", rst_data.entry_conut);
#endif

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check input MAC address */
	/* On Android O, this might be wlan0 address */
	if (UNEQUAL_MAC_ADDR(arBssid, mac) && UNEQUAL_MAC_ADDR(prGlueInfo->prAdapter->rWifiVar.aucMacAddress, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
			"incorrect BSSID: [" MACSTR "] currently connected BSSID[" MACSTR "]\n",
			MAC2STR(mac), MAC2STR(arBssid));
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, " entry_conut = %d\n",
						rst_data.entry_conut);
#endif
		return -ENOENT;
	}

	/* 2. fill TX rate */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryLinkSpeed, &u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE, &u4BufLen);

#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
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
	}

	/* 3. fill RSSI */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryRssi, &i4Rssi, sizeof(i4Rssi), TRUE, FALSE, FALSE, &u4BufLen);

#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_SIGNAL);
#else
		sinfo->filled |= STATION_INFO_SIGNAL;
#endif

		if ((rStatus != WLAN_STATUS_SUCCESS) || (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM)
		    || (i4Rssi == PARAM_WHQL_RSSI_MAX_DBM)) {
			DBGLOG(REQ, WARN, "last rssi\n");
			sinfo->signal = prGlueInfo->i4RssiCache;
		} else {
			sinfo->signal = i4Rssi;	/* dBm */
			prGlueInfo->i4RssiCache = i4Rssi;
		}
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	if (prDevStats) {
		/* 4. fill RX_PACKETS */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_PACKETS);
#else
		sinfo->filled |= STATION_INFO_RX_PACKETS;
#endif
		sinfo->rx_packets = prDevStats->rx_packets;

		/* 5. fill TX_PACKETS */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_PACKETS);
#else
		sinfo->filled |= STATION_INFO_TX_PACKETS;
#endif
		sinfo->tx_packets = prDevStats->tx_packets;

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
			DBGLOG(REQ, INFO, "BSSID: [" MACSTR "] TxFailCount %d LifeTimeOut %d\n",
				MAC2STR(arBssid), rQueryStaStatistics.u4TxFailCount,
				rQueryStaStatistics.u4TxLifeTimeoutCount);

			u4TotalError = rQueryStaStatistics.u4TxFailCount + rQueryStaStatistics.u4TxLifeTimeoutCount;
			prDevStats->tx_errors += u4TotalError;
		}
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_FAILED);
#else
		sinfo->filled |= STATION_INFO_TX_FAILED;
#endif
		sinfo->tx_failed = prDevStats->tx_errors;
	}
#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, " entry_conut = %d\n", rst_data.entry_conut);
#endif
	return 0;
}
#else
int mtk_cfg80211_get_station(struct wiphy *wiphy, struct net_device *ndev, u8 *mac, struct station_info *sinfo)
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip get station\n");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, " entry_conut = %d\n", rst_data.entry_conut);
#endif

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR "] currently connected BSSID[" MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, " entry_conut = %d\n",
						rst_data.entry_conut);
#endif
		return -ENOENT;
	}

	/* 2. fill TX rate */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryLinkSpeed, &u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE, &u4BufLen);

		sinfo->filled |= STATION_INFO_TX_BITRATE;

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
	}

	/* 3. fill RSSI */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryRssi, &i4Rssi, sizeof(i4Rssi), TRUE, FALSE, FALSE, &u4BufLen);

		sinfo->filled |= STATION_INFO_SIGNAL;

		if ((rStatus != WLAN_STATUS_SUCCESS) || (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM)
		    || (i4Rssi == PARAM_WHQL_RSSI_MAX_DBM)) {
			DBGLOG(REQ, WARN, "last rssi\n");
			sinfo->signal = prGlueInfo->i4RssiCache;
		} else {
			sinfo->signal = i4Rssi;	/* dBm */
			prGlueInfo->i4RssiCache = i4Rssi;
		}
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	if (prDevStats) {
		/* 4. fill RX_PACKETS */
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->rx_packets = prDevStats->rx_packets;

		/* 5. fill TX_PACKETS */
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->tx_packets = prDevStats->tx_packets;

		/* 6. fill TX_FAILED */
		kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, arBssid);
		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryStaStatistics,
				   &rQueryStaStatistics, sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "unable to get sta statistics: status[0x%x]\n", rStatus);
		} else {
			DBGLOG(REQ, INFO, "BSSID: [" MACSTR "] TxFailCount %d LifeTimeOut %d\n",
			       MAC2STR(arBssid), rQueryStaStatistics.u4TxFailCount,
			       rQueryStaStatistics.u4TxLifeTimeoutCount);

			u4TotalError = rQueryStaStatistics.u4TxFailCount + rQueryStaStatistics.u4TxLifeTimeoutCount;
			prDevStats->tx_errors += u4TotalError;
		}
		sinfo->filled |= STATION_INFO_TX_FAILED;
		sinfo->tx_failed = prDevStats->tx_errors;
	}
#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
	return 0;
}
#endif
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_cfg80211_get_link_statistics\n");
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR "] currently connected BSSID[" MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
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
int mtk_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 i, u4BufLen;
	PARAM_SCAN_REQUEST_ADV_T rScanRequest;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
	kalMemZero(&rScanRequest, sizeof(rScanRequest));

	if (!wlanGetHifState(prGlueInfo))
		return -EINVAL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip scan");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL) {
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
		return -EBUSY;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, WARN, "prGlueInfo->u4ReadyFlag == 0\n");
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
	}

	if (atomic_read(&prGlueInfo->cfgSuspend)) {
		DBGLOG(REQ, WARN, "In suspend block cfg80211 ops\n");
		return -EFAULT;
	}

	if (request->n_ssids == 0) {
		rScanRequest.u4SsidNum = 0;
	} else if (request->n_ssids <= SCN_SSID_MAX_NUM) {
		rScanRequest.u4SsidNum = request->n_ssids;

		for (i = 0; i < request->n_ssids; i++) {
			COPY_SSID(rScanRequest.rSsid[i].aucSsid,
				  rScanRequest.rSsid[i].u4SsidLen, request->ssids[i].ssid, request->ssids[i].ssid_len);
		}
	} else {
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
		return -EINVAL;
	}

	rScanRequest.u4IELength = request->ie_len;
	if (request->ie_len > 0)
		rScanRequest.pucIE = (PUINT_8) (request->ie);

#if CFG_SCAN_CHANNEL_SPECIFIED
	DBGLOG(REQ, INFO, "scan channel num = %d\n", request->n_channels);

	if (request->n_channels > MAXIMUM_OPERATION_CHANNEL_LIST) {
		DBGLOG(REQ, WARN, "scan channel num (%d) exceeds %d, do a full scan instead\n",
			   request->n_channels, MAXIMUM_OPERATION_CHANNEL_LIST);
		rScanRequest.ucChannelListNum = 0;
	} else {
		rScanRequest.ucChannelListNum = request->n_channels;
		for (i = 0; i < request->n_channels; i++) {
			rScanRequest.arChnlInfoList[i].eBand =
				kalCfg80211ToMtkBand(request->channels[i]->band);
			rScanRequest.arChnlInfoList[i].u4CenterFreq1 = request->channels[i]->center_freq;
			rScanRequest.arChnlInfoList[i].u4CenterFreq2 = 0;
			rScanRequest.arChnlInfoList[i].u2PriChnlFreq = request->channels[i]->center_freq;
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
			rScanRequest.arChnlInfoList[i].ucChnlBw = request->scan_width;
#else
			rScanRequest.arChnlInfoList[i].ucChnlBw = 0;
#endif
			rScanRequest.arChnlInfoList[i].ucChannelNum =
				ieee80211_frequency_to_channel(request->channels[i]->center_freq);
		}
	}
#endif
	/* 2018/04/18 frog: The point should be ready before doing IOCTL. */
	prGlueInfo->prScanRequest = request;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetBssidListScanAdv,
			   &rScanRequest, sizeof(PARAM_SCAN_REQUEST_ADV_T), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "scan error:0x%x\n", rStatus);
		/* 2018/04/18 frog: Remove pointer if IOCTL fail. */
		prGlueInfo->prScanRequest = NULL;
		return -EINVAL;
	}

#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

	return 0;
}

static UINT_8 wepBuf[48];
#if CFG_SUPPORT_CFG80211_AUTH
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting auth to
 *        the ESS with the specified parameters
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_auth(struct wiphy *wiphy, struct net_device *ndev,
			struct cfg80211_auth_request *req)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 rStatus;
	UINT_32 u4BufLen;
	PARAM_CONNECT_T rNewSsid;
	ENUM_PARAM_OP_MODE_T eOpMode;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif
	P_PARAM_WEP_T prWepKey;
	/*Is auth parameter needed to be updated to AIS.*/
	UINT_8 fgNewAuthParam = FALSE;
	P_STA_RECORD_T prStaRec = NULL;

#if CFG_CHIP_RESET_SUPPORT
		if (checkResetState()) {
			DBGLOG(REQ, ERROR,
				"chip resetting, mtk_cfg80211_auth do nothing\n");
			return -EINVAL;
		}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
#if KERNEL_VERSION(4, 10, 0) > CFG80211_VERSION_CODE
	if (req->sae_data_len != 0)
		DBGLOG(REQ, INFO, "[wlan] mtk_cfg80211_auth %p %zu\n",
			req->sae_data, req->sae_data_len);
#else
	if (req->auth_data_len != 0)
		DBGLOG(REQ, INFO, "[wlan] mtk_cfg80211_auth %p %zu\n",
			req->auth_data, req->auth_data_len);
#endif
	DBGLOG(REQ, INFO, "auth to  BSS [" MACSTR "]\n",
		MAC2STR((PUINT_8)req->bss->bssid));
	DBGLOG(REQ, INFO, "auth_type:%d\n", req->auth_type);

	prConnSettings = &prGlueInfo->prAdapter->rWifiVar.rConnSettings;

	/* <1>Set OP mode */
	if (prGlueInfo->prAdapter->rWifiVar.rConnSettings.eOPMode >
		NET_TYPE_AUTO_SWITCH)
		eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		eOpMode = prGlueInfo->prAdapter->rWifiVar.rConnSettings.eOPMode;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetInfrastructureMode, &eOpMode,
		sizeof(eOpMode), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "wlanoidSetInfrastructureMode fail 0x%x\n",
			rStatus);
		return -EFAULT;
	}

	/*<2> Set  Auth data */
	prConnSettings->ucAuthDataLen = 0;
#if KERNEL_VERSION(4, 10, 0) > CFG80211_VERSION_CODE
	if (req->sae_data_len != 0) {
		if (req->sae_data_len > AUTH_DATA_MAX_LEN) {
			DBGLOG(INIT, WARN,
				"request auth with unexpected length:%d\n",
				req->sae_data_len);
			return -EFAULT;
		}

		kalMemCopy(prConnSettings->aucAuthData, req->sae_data,
			req->sae_data_len);
		prConnSettings->ucAuthDataLen = req->sae_data_len;

		DBGLOG(INIT, INFO,
			"Dump auth data in connectSettings, auth len:%d\n",
			prConnSettings->ucAuthDataLen);
		DBGLOG_MEM8(REQ, INFO, prConnSettings->aucAuthData,
			req->sae_data_len);
	}
#else
	if (req->auth_data_len != 0) {
		if (req->auth_data_len > AUTH_DATA_MAX_LEN) {
			DBGLOG(INIT, WARN,
				"request auth with unexpected length:%d\n",
				req->auth_data_len);
			return -EFAULT;
		}

		kalMemCopy(prConnSettings->aucAuthData, req->auth_data,
			req->auth_data_len);
		prConnSettings->ucAuthDataLen = req->auth_data_len;

		DBGLOG(INIT, INFO,
			"Dump auth data in connectSettings, auth len:%d\n",
			prConnSettings->ucAuthDataLen);
		DBGLOG_MEM8(REQ, INFO, prConnSettings->aucAuthData,
			req->auth_data_len);
	}
#endif
	/*<2> Set ChannelNum */
	if (req->bss->channel->center_freq) {
		prConnSettings->ucChannelNum =
			nicFreq2ChannelNum(
				req->bss->channel->center_freq * 1000);
		DBGLOG(RSN, INFO,
			"set prConnSettings->ucChannelNum:%d\n",
			prConnSettings->ucChannelNum);
	} else {
		prConnSettings->ucChannelNum = 0;
		DBGLOG(RSN, INFO,
			"req->bss->channel->center_freq is NULL.\n");
	}


#if CFG_SUPPORT_REPLAY_DETECTION
	/* reset Detect replay information */
	prDetRplyInfo = &prGlueInfo->prDetRplyInfo;
	kalMemZero(prDetRplyInfo, sizeof(struct SEC_DETECT_REPLAY_INFO));
#endif

	/* Reset WPA info */
	prGlueInfo->rWpaInfo.u4AuthAlg = 0;

	switch (req->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		if (!(prGlueInfo->rWpaInfo.u4AuthAlg & AUTH_TYPE_OPEN_SYSTEM))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo.u4AuthAlg |= AUTH_TYPE_OPEN_SYSTEM;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		if (!(prGlueInfo->rWpaInfo.u4AuthAlg & AUTH_TYPE_SHARED_KEY))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo.u4AuthAlg |= AUTH_TYPE_SHARED_KEY;
		break;
	case NL80211_AUTHTYPE_SAE:
		if (!(prGlueInfo->rWpaInfo.u4AuthAlg & AUTH_TYPE_SAE))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo.u4AuthAlg |= AUTH_TYPE_SAE;
		break;
	default:
		DBGLOG(REQ, WARN,
			"Auth type: %ld not support, use default OPEN system\n",
			req->auth_type);
		prGlueInfo->rWpaInfo.u4AuthAlg |= AUTH_TYPE_OPEN_SYSTEM;
		break;
	}
	DBGLOG(REQ, INFO, "Auth Algorithm : %ld\n",
		prGlueInfo->rWpaInfo.u4AuthAlg);

	if (req->key_len != 0) {
		/* NL80211 only set the Tx wep key while connect,
		 * the max 4 wep key set prior via add key cmd
		 */

		if (!(prGlueInfo->rWpaInfo.u4AuthAlg & AUTH_TYPE_SHARED_KEY))
			DBGLOG(REQ, WARN, "Auth Algorithm : %ld with wep key\n",
			prGlueInfo->rWpaInfo.u4AuthAlg);

		prWepKey = (P_PARAM_WEP_T) wepBuf;

		kalMemZero(prWepKey, sizeof(PARAM_WEP_T));
		prWepKey->u4Length = OFFSET_OF(PARAM_WEP_T, aucKeyMaterial) +
			req->key_len;
		prWepKey->u4KeyLength = (UINT_32) req->key_len;
		prWepKey->u4KeyIndex = (UINT_32) req->key_idx;
		prWepKey->u4KeyIndex |= IS_TRANSMIT_KEY;
		if (prWepKey->u4KeyLength > MAX_KEY_LEN) {
			DBGLOG(REQ, WARN, "Too long key length (%u)\n",
				prWepKey->u4KeyLength);
			return -EINVAL;
		}
		kalMemCopy(prWepKey->aucKeyMaterial, req->key,
			prWepKey->u4KeyLength);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetAddWep, prWepKey,
			prWepKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "wlanoidSetAddWep fail 0x%x\n",
				rStatus);
			return -EFAULT;
		}
	}
	kalMemZero(&rNewSsid, sizeof(PARAM_CONNECT_T));

	if (rNewSsid.pucBssid != (PUINT_8)req->bss->bssid) {
		fgNewAuthParam = TRUE;
		rNewSsid.pucBssid = (PUINT_8)req->bss->bssid;
	}
	/* rNewSsid.pucSsid = (uint8_t *)sme->ssid;*/
	/* rNewSsid.u4SsidLen = sme->ssid_len;*/

	DBGLOG(REQ, INFO, "auth to  BSS [" MACSTR "],UpperReq [" MACSTR "]\n",
		MAC2STR(rNewSsid.pucBssid),
		MAC2STR((uint8_t *)req->bss->bssid));

	prConnSettings->fgIsSendAssoc = FALSE;
	if (!prConnSettings->fgIsConnInitialized /*|| fgNewAuthParam*/) {
		/* [TODO] to consider if bssid/auth_alg changed
		 * (need to update to AIS)
		 */
		if (fgNewAuthParam)
			DBGLOG(REQ, WARN, "auth param update\n");
			rStatus = kalIoctl(prGlueInfo, wlanoidSetConnect,
				(void *)&rNewSsid, sizeof(PARAM_CONNECT_T),
				FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "set SSID:%x\n", rStatus);
			return -EINVAL;
		}
	} else {
		/* skip join initial flow
		 * when it has been completed with the same auth parameters
		 */
		prStaRec = cnmGetStaRecByAddress(prGlueInfo->prAdapter,
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex,
			rNewSsid.pucBssid);
		if (prStaRec)
			saaSendAuthAssoc(prGlueInfo->prAdapter, prStaRec);
		else
			DBGLOG(REQ, WARN,
				"can't send auth since can't find StaRec\n");
	}

	return 0;
}
#endif

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
	ENUM_PARAM_AUTH_MODE_T eAuthMode;
	UINT_32 cipher;
	PARAM_CONNECT_T rNewSsid;
	BOOLEAN fgCarryWPSIE = FALSE;
	ENUM_PARAM_OP_MODE_T eOpMode;
	UINT_32 i, u4AkmSuite = 0;
	P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry;
#if CFG_SUPPORT_REPLAY_DETECTION
	P_BSS_INFO_T prBssInfo = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, WARN, "wlan is halt, skip conn.");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(INIT, ERROR, "Adapter is not ready\n");
		return -EINVAL;
	}

	if (ndev == NULL) {
		DBGLOG(REQ, ERROR, "ndev is NULL\n");
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
		return -EINVAL;
	}

	if (atomic_read(&prGlueInfo->cfgSuspend)) {
		DBGLOG(REQ, WARN, "In suspend block cfg80211 ops\n");
		return -EFAULT;
	}

	/* printk("[wlan]mtk_cfg80211_connect\n"); */
	if (prGlueInfo->prAdapter->rWifiVar.rConnSettings.eOPMode > NET_TYPE_AUTO_SWITCH)
		eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		eOpMode = prGlueInfo->prAdapter->rWifiVar.rConnSettings.eOPMode;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetInfrastructureMode, &eOpMode, sizeof(eOpMode), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO,
		       "wlanoidSetInfrastructureMode fail 0x%x\n", rStatus);
		return -EFAULT;
	}

	/* after set operation mode, key table are cleared */

	/* reset wpa info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
	prGlueInfo->rWpaInfo.fgPrivacyInvoke = FALSE;

#if CFG_SUPPORT_REPLAY_DETECTION
	/* reset Detect replay information */
	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);

	prDetRplyInfo = &prBssInfo->rDetRplyInfo;
	kalMemZero(prDetRplyInfo, sizeof(struct SEC_DETECT_REPLAY_INFO));
#endif


#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4CipherGroupMgmt = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	switch (sme->mfp) {
	case NL80211_MFP_NO:
		prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
		break;
	case NL80211_MFP_REQUIRED:
		prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_REQUIRED;
		break;
	default:
		prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
		break;
	}
	/* DBGLOG(SCN, INFO, ("MFP=%d\n", prGlueInfo->rWpaInfo.u4Mfp)); */
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
	default:
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM | IW_AUTH_ALG_SHARED_KEY;
		break;
	}

	if (sme->crypto.n_ciphers_pairwise) {
		DBGLOG(RSN, INFO, "[wlan] cipher pairwise (%x)\n", sme->crypto.ciphers_pairwise[0]);

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
#if CFG_SUPPORT_SUITB
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_GCMP256;
			break;
#endif
		default:
			DBGLOG(REQ, WARN, "invalid cipher pairwise (%d)\n", sme->crypto.ciphers_pairwise[0]);
#if CFG_CHIP_RESET_SUPPORT
			rst_data.entry_conut--;
			DBGLOG(INIT, TRACE, "entry_conut = %d\n",
						rst_data.entry_conut);
#endif
			return -EINVAL;
		}
	}

	if (sme->crypto.cipher_group) {
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
#if CFG_SUPPORT_SUITB
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			break;
#endif
		default:
			DBGLOG(REQ, WARN, "invalid cipher group (%d)\n", sme->crypto.cipher_group);
#if CFG_CHIP_RESET_SUPPORT
			rst_data.entry_conut--;
			DBGLOG(INIT, TRACE, "entry_conut = %d\n",
						rst_data.entry_conut);
#endif
			return -EINVAL;
		}
	}

	/* DBGLOG(SCN, INFO, ("akm_suites=%x\n", sme->crypto.akm_suites[0])); */
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
				DBGLOG(REQ, WARN, "invalid Akm Suite (%d)\n", sme->crypto.akm_suites[0]);
#if CFG_CHIP_RESET_SUPPORT
				rst_data.entry_conut--;
				DBGLOG(INIT, TRACE, "entry_conut = %d\n",
							rst_data.entry_conut);
#endif
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
#if CFG_SUPPORT_SUITB
			case WLAN_AKM_SUITE_8021X_SUITE_B:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;

			case WLAN_AKM_SUITE_8021X_SUITE_B_192:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;
#endif
#if CFG_SUPPORT_OWE
			case WLAN_AKM_SUITE_OWE:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_OWE;
				break;
#endif
			default:
				DBGLOG(REQ, WARN, "invalid Akm Suite (%d)\n", sme->crypto.akm_suites[0]);
#if CFG_CHIP_RESET_SUPPORT
				rst_data.entry_conut--;
				DBGLOG(INIT, TRACE, "entry_conut = %d\n",
							rst_data.entry_conut);
#endif
				return -EINVAL;
			}
		}
	}

	if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		eAuthMode = (prGlueInfo->rWpaInfo.u4AuthAlg == IW_AUTH_ALG_OPEN_SYSTEM) ?
		    AUTH_MODE_OPEN : AUTH_MODE_AUTO_SWITCH;
	}

	prGlueInfo->rWpaInfo.fgPrivacyInvoke = sme->privacy;
	prGlueInfo->fgWpsActive = FALSE;

#if CFG_SUPPORT_PASSPOINT
	prGlueInfo->fgConnectHS20AP = FALSE;
#endif /* CFG_SUPPORT_PASSPOINT */

	prGlueInfo->non_wfa_vendor_ie_len = 0;
	if (sme->ie && sme->ie_len > 0) {
		WLAN_STATUS rStatus;
		UINT_32 u4BufLen;
		PUINT_8 prDesiredIE = NULL;
		PUINT_8 pucIEStart = (PUINT_8)sme->ie;
#if CFG_SUPPORT_WAPI
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetWapiAssocInfo, pucIEStart, sme->ie_len, FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(SEC, WARN,
			"[wapi] set wapi assoc info error:0x%x\n", rStatus);
#endif
#if CFG_SUPPORT_WPS2
		if (wextSrchDesiredWPSIE(pucIEStart, sme->ie_len, 0xDD, (PUINT_8 *) &prDesiredIE)) {
			prGlueInfo->fgWpsActive = TRUE;
			fgCarryWPSIE = TRUE;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetWSCAssocInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(SEC, WARN,
				"[WSC] set WSC assoc info error:0x%x\n",
				rStatus);
		}
#endif
#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "[HS20] set HS20 assoc info error:%lx\n", rStatus);
#endif
		}
		if (wextSrchDesiredInterworkingIE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetInterworkingInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "[HS20] set Interworking assoc info error:%lx\n", rStatus);
#endif
		}
		if (wextSrchDesiredRoamingConsortiumIE(pucIEStart, sme->ie_len, (PUINT_8 *) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRoamingConsortiumIEInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "[HS20] set RoamingConsortium assoc info error:%lx\n", rStatus);
#endif
		}
#endif /* CFG_SUPPORT_PASSPOINT */
		if (wextSrchDesiredWPAIE(pucIEStart, sme->ie_len, 0x30, (PUINT_8 *) &prDesiredIE)) {
			RSN_INFO_T rRsnInfo;

			if (rsnParseRsnIE(prGlueInfo->prAdapter, (P_RSN_INFO_ELEM_T)prDesiredIE, &rRsnInfo)) {
#if CFG_SUPPORT_802_11W
				/* Fill RSNE MFP Cap */
				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
					prGlueInfo->rWpaInfo.u4CipherGroupMgmt
						= rRsnInfo
						.u4GroupMgmtKeyCipherSuite;
					prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_OPTIONAL;
					if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPR)
						prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_REQUIRED;
				} else
					prGlueInfo->rWpaInfo.ucRSNMfpCap =
						RSN_AUTH_MFP_DISABLED;
#endif
			}
		}

		/* Find non-wfa vendor specific ies set from upper layer */
		if (cfg80211_get_non_wfa_vendor_ie(prGlueInfo, pucIEStart,
			sme->ie_len) > 0) {
			DBGLOG(AIS, INFO, "Found non-wfa vendor ie (len=%u)\n",
				   prGlueInfo->non_wfa_vendor_ie_len);
		}
	}

	/* clear WSC Assoc IE buffer in case WPS IE is not detected */
	if (fgCarryWPSIE == FALSE) {
		kalMemZero(&prGlueInfo->aucWSCAssocInfoIE, 200);
		prGlueInfo->u2WSCAssocInfoIELen = 0;
	}

		/*Fill WPA info - mfp setting */
		/* Must put after paring RSNE from upper layer
		* for prGlueInfo->rWpaInfo.ucRSNMfpCap assignment
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
			if (prGlueInfo->rWpaInfo.ucRSNMfpCap ==
				RSN_AUTH_MFP_OPTIONAL)
				prGlueInfo->rWpaInfo.u4Mfp =
				IW_AUTH_MFP_OPTIONAL;
			else if (prGlueInfo->rWpaInfo.ucRSNMfpCap ==
				RSN_AUTH_MFP_REQUIRED)
				DBGLOG(REQ, ERROR,
				"param(DISABLED) conflict with cap(REQUIRED)\n");
			break;
		case NL80211_MFP_REQUIRED:
			prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_REQUIRED;
			break;
		default:
			prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
			break;
		}
#endif

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAuthMode, &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set auth mode error:0x%x\n", rStatus);

	/* Enable the specific AKM suite only. */
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prGlueInfo->prAdapter->rMib.dot11RSNAConfigAuthenticationSuitesTable[i];

		if (prEntry->dot11RSNAConfigAuthenticationSuite == u4AkmSuite) {
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = TRUE;
			/* printk("match AuthenticationSuite = 0x%x", u4AkmSuite); */
		} else {
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = FALSE;
		}
	}

	cipher = prGlueInfo->rWpaInfo.u4CipherGroup | prGlueInfo->rWpaInfo.u4CipherPairwise;

	if (1 /* prGlueInfo->rWpaInfo.fgPrivacyInvoke */) {
#if CFG_SUPPORT_SUITB
		if (cipher & IW_AUTH_CIPHER_GCMP256) {
			eEncStatus = ENUM_ENCRYPTION4_ENABLED;
		} else
#endif
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
		DBGLOG(REQ, WARN, "set encryption mode error:0x%x\n", rStatus);

	if (sme->key_len != 0 && prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		/* NL80211 only set the Tx wep key while connect, the max 4 wep key set prior via add key cmd */
		P_PARAM_WEP_T prWepKey = (P_PARAM_WEP_T) wepBuf;

		kalMemZero(prWepKey, sizeof(PARAM_WEP_T));
		prWepKey->u4Length = OFFSET_OF(PARAM_WEP_T, aucKeyMaterial) + sme->key_len;
		prWepKey->u4KeyLength = (UINT_32) sme->key_len;
		prWepKey->u4KeyIndex = (UINT_32) sme->key_idx;
		prWepKey->u4KeyIndex |= IS_TRANSMIT_KEY;
		if (prWepKey->u4KeyLength > MAX_KEY_LEN) {
			DBGLOG(REQ, WARN, "Too long key length (%u)\n",
				prWepKey->u4KeyLength);

#if CFG_CHIP_RESET_SUPPORT
			rst_data.entry_conut--;
			DBGLOG(INIT, TRACE, "entry_conut = %d\n",
					rst_data.entry_conut);
#endif
			return -EINVAL;
		}
		kalMemCopy(prWepKey->aucKeyMaterial, sme->key, prWepKey->u4KeyLength);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetAddWep, prWepKey, prWepKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "wlanoidSetAddWep fail 0x%x\n",
				rStatus);

#if CFG_CHIP_RESET_SUPPORT
			rst_data.entry_conut--;
			DBGLOG(INIT, TRACE, "entry_conut = %d\n",
					rst_data.entry_conut);
#endif
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
#if CFG_CHIP_RESET_SUPPORT
		rst_data.entry_conut--;
		DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
		return -EINVAL;
	}
#if 0
	if (sme->bssid != NULL && 1 /* prGlueInfo->fgIsBSSIDSet */) {
		/* connect by BSSID */
		if (sme->ssid_len > 0) {
			P_CONNECTION_SETTINGS_T prConnSettings = NULL;

			prConnSettings = &(prGlueInfo->prAdapter->rWifiVar.rConnSettings);
			/* prGlueInfo->fgIsSSIDandBSSIDSet = TRUE; */
			COPY_SSID(prConnSettings->aucSSID, prConnSettings->ucSSIDLen, sme->ssid, sme->ssid_len);
		}
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetBssid,
				   (PVOID) sme->bssid, MAC_ADDR_LEN, FALSE, FALSE, TRUE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "set BSSID:%lx\n", rStatus);
			return -EINVAL;
		}
	} else if (sme->ssid_len > 0) {
		/* connect by SSID */
		COPY_SSID(rNewSsid.aucSsid, rNewSsid.u4SsidLen, sme->ssid, sme->ssid_len);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetSsid,
				   (PVOID)&rNewSsid, sizeof(PARAM_SSID_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "set SSID:%lx\n", rStatus);
			return -EINVAL;
		}
	}
#endif
#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
	return 0;
}

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_disconnect do nothing\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if (atomic_read(&prGlueInfo->cfgSuspend)) {
		DBGLOG(REQ, WARN, "In suspend block cfg80211 ops\n");
		return -EFAULT;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

#if CFG_SUPPORT_CFG80211_AUTH
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to deauth from
 *        currently connected ESS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_deauth(struct wiphy *wiphy, struct net_device *ndev,
			struct cfg80211_deauth_request *req)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if CFG_CHIP_RESET_SUPPORT
		if (checkResetState()) {
			DBGLOG(REQ, ERROR,
				"chip resetting, mtk_cfg80211_change_iface do nothing\n");
			return -EINVAL;
		}
#endif

	DBGLOG(REQ, INFO, "mtk_cfg80211_deauth\n");
#if CFG_SUPPORT_CFG80211_AUTH
	/* The BSS from cfg80211_ops.assoc must give back to
	 * cfg80211_send_rx_assoc() or to cfg80211_assoc_timeout().
	 * To ensure proper refcounting, new association requests
	 * while already associating must be rejected.
	 */
	if (prGlueInfo->prAdapter->rWifiVar.rConnSettings.bss) {
		DBGLOG(REQ, INFO, "assoc timeout notify\n");
		/* ops caller have already hold the mutex. */
		cfg80211_assoc_timeout(ndev,
			prGlueInfo->prAdapter->rWifiVar.rConnSettings.bss);
		DBGLOG(REQ, INFO, "assoc timeout notify, Done\n");
		prGlueInfo->prAdapter->rWifiVar.rConnSettings.bss = NULL;
	}
#endif
	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0,
			FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}
#endif

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
	UINT_32 u4BufLen = 0, u4SsidLen = 0;
	WLAN_STATUS rStatus;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_join_ibss do nothing\n");
		return -EINVAL;
	}
#endif

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
	if (params->ssid_len > PARAM_MAX_LEN_SSID)
		u4SsidLen = PARAM_MAX_LEN_SSID;
	else
		u4SsidLen = params->ssid_len;

	kalMemCopy(rNewSsid.aucSsid, params->ssid, u4SsidLen);
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSsid, (PVOID)&rNewSsid, sizeof(PARAM_SSID_T), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set SSID:0x%x\n", rStatus);
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_leave_ibss do nothing\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:0x%x\n", rStatus);
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

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, WARN, "prGlueInfo->u4ReadyFlag == 0\n");
		return -EFAULT;
	}

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip set pwr mgmt\n");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

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
#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set_power_mgmt error:0x%x\n", rStatus);
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_set_pmksa do nothing\n");
		return -EINVAL;
	}
#endif

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
		DBGLOG(INIT, INFO, "add pmkid error:0x%x\n", rStatus);
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip flush pmksa\n");
		return WLAN_STATUS_FAILURE;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

	prPmkid->u4Length = 8;
	prPmkid->u4BSSIDInfoCount = 0;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "flush pmkid error:0x%x\n", rStatus);
	kalMemFree(prPmkid, VIR_MEM_TYPE, 8);

#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif
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
	UINT_32 u4BufLen;
	P_PARAM_GTK_REKEY_DATA prGtkData;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Rslt = -EINVAL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_set_rekey_data\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* if disable offload, we store key data here, and enable rekey offload when enter wow */
	if (!prGlueInfo->prAdapter->rWifiVar.ucEapolOffload) {
		kalMemZero(prGlueInfo->rWpaInfo.aucKek, NL80211_KEK_LEN);
		kalMemZero(prGlueInfo->rWpaInfo.aucKck, NL80211_KCK_LEN);
		kalMemZero(prGlueInfo->rWpaInfo.aucReplayCtr, NL80211_REPLAY_CTR_LEN);
		kalMemCopy(prGlueInfo->rWpaInfo.aucKek, data->kek, NL80211_KEK_LEN);
		kalMemCopy(prGlueInfo->rWpaInfo.aucKck, data->kck, NL80211_KCK_LEN);
		kalMemCopy(prGlueInfo->rWpaInfo.aucReplayCtr, data->replay_ctr, NL80211_REPLAY_CTR_LEN);

		return 0;
	}

	prGtkData =
	    (P_PARAM_GTK_REKEY_DATA) kalMemAlloc(sizeof(PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);

	if (!prGtkData)
		return WLAN_STATUS_SUCCESS;

	DBGLOG(RSN, INFO, "cfg80211_set_rekey_data size(%d)\n",
	       (uint32_t) sizeof(struct cfg80211_gtk_rekey_data));

	DBGLOG(RSN, INFO, "kek\n");
	DBGLOG_MEM8(PF, ERROR, (PUINT_8)data->kek, NL80211_KEK_LEN);
	DBGLOG(RSN, INFO, "kck\n");
	DBGLOG_MEM8(PF, ERROR, (PUINT_8)data->kck, NL80211_KCK_LEN);
	DBGLOG(RSN, INFO, "replay count\n");
	DBGLOG_MEM8(PF, ERROR, (PUINT_8)data->replay_ctr, NL80211_REPLAY_CTR_LEN);


#if 0
	kalMemCopy(prGtkData, data, sizeof(*data));
#else
	kalMemCopy(prGtkData->aucKek, data->kek, NL80211_KEK_LEN);
	kalMemCopy(prGtkData->aucKck, data->kck, NL80211_KCK_LEN);
	kalMemCopy(prGtkData->aucReplayCtr, data->replay_ctr, NL80211_REPLAY_CTR_LEN);
#endif

	prGtkData->ucBssIndex = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

	prGtkData->u4Proto = NL80211_WPA_VERSION_2;
	if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA)
		prGtkData->u4Proto = NL80211_WPA_VERSION_1;

	if (prGlueInfo->rWpaInfo.u4CipherPairwise == IW_AUTH_CIPHER_TKIP)
		prGtkData->u4PairwiseCipher = BIT(3);
	else if (prGlueInfo->rWpaInfo.u4CipherPairwise == IW_AUTH_CIPHER_CCMP)
		prGtkData->u4PairwiseCipher = BIT(4);
	else {
		kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));
		return WLAN_STATUS_SUCCESS;
	}

	if (prGlueInfo->rWpaInfo.u4CipherGroup == IW_AUTH_CIPHER_TKIP)
		prGtkData->u4GroupCipher    = BIT(3);
	else if (prGlueInfo->rWpaInfo.u4CipherGroup == IW_AUTH_CIPHER_CCMP)
		prGtkData->u4GroupCipher    = BIT(4);
	else {
		kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));
		return WLAN_STATUS_SUCCESS;
	}

	prGtkData->u4KeyMgmt = prGlueInfo->rWpaInfo.u4KeyMgmt;
	prGtkData->u4MgmtGroupCipher = 0;

	prGtkData->ucRekeyMode = GTK_REKEY_CMD_MODE_OFFLOAD_ON;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetGtkRekeyData,
			   prGtkData, sizeof(PARAM_GTK_REKEY_DATA), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "set GTK rekey data error:0x%x\n", rStatus);
	else
		i4Rslt = 0;

	kalMemFree(prGtkData, VIR_MEM_TYPE, sizeof(PARAM_GTK_REKEY_DATA));

	return i4Rslt;
}

void mtk_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
				      IN struct wireless_dev *wdev, IN u16 frame_type, IN bool reg)
{
#if 0
	P_MSG_P2P_MGMT_FRAME_REGISTER_T prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) NULL;
#endif
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(INIT, WARN, "wlan is halt, skip mgmt reg.");
		return;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

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

#if CFG_CHIP_RESET_SUPPORT
	rst_data.entry_conut--;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#endif

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
				   struct ieee80211_channel *chan, unsigned int duration, u64 *cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_REMAIN_ON_CHANNEL_T prMsgChnlReq = (P_MSG_REMAIN_ON_CHANNEL_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_remain_on_channel do nothing\n");
		return -EINVAL;
	}
#endif

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		    || (chan == NULL)
		    || (cookie == NULL)) {
			break;
		}

		prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		if (!wlanGetHifState(prGlueInfo))
			break;

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
int mtk_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_CANCEL_REMAIN_ON_CHANNEL_T prMsgChnlAbort = (P_MSG_CANCEL_REMAIN_ON_CHANNEL_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_cancel_remain_on_channel do nothing\n");
		return -EINVAL;
	}
#endif

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
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_mgmt_tx do nothing\n");
		return -EINVAL;
	}
#endif

	do {
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
#else
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 struct ieee80211_channel *channel, bool offscan,
			 unsigned int wait,
			 const u8 *buf, size_t len, bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_MGMT_TX_REQUEST_T prMsgTxReq = (P_MSG_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucFrameBuf = (PUINT_8) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_mgmt_tx do nothing\n");
		return -EINVAL;
	}
#endif

	do {
		if ((wiphy == NULL)
		    || (buf == NULL)
		    || (len == 0)
		    || (wdev == NULL)
		    || (cookie == NULL)) {
			break;
		}

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

		prMgmtFrame = cnmMgtPktAlloc(prGlueInfo->prAdapter, (UINT_32) (len + MAC_TX_RESERVED_FIELD));
		prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
		if (prMsgTxReq->prMgmtMsduInfo == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgTxReq->u8Cookie = *cookie;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_AIS_MGMT_TX;

		pucFrameBuf = (PUINT_8) ((ULONG) prMgmtFrame->prPacket + MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, buf, len);

		prMgmtFrame->u2FrameLength = len;

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
#endif
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
int mtk_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting,mtk_cfg80211_mgmt_tx_cancel_wait do nothing\n");
		return -EINVAL;
	}
#endif

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	/* not implemented */

	return -EINVAL;
}

#ifdef CONFIG_NL80211_TESTMODE

#if CFG_SUPPORT_PASSPOINT
int mtk_cfg80211_testmode_hs20_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct wpa_driver_hs20_data_s *prParams = NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_testmode_hs20_cmd do nothing\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (data && len)
		prParams = (struct wpa_driver_hs20_data_s *)data;

	if (prParams) {
		int i;

		DBGLOG(INIT, INFO, "[%s] Cmd Type (%d)\n",
		__func__, prParams->CmdType);
		switch (prParams->CmdType) {
		case HS20_CMD_ID_SET_BSSID_POOL:
			DBGLOG(INIT, INFO,
			       "[%s] fgBssidPoolIsEnable (%d)\n", __func__,
			       prParams->hs20_set_bssid_pool.fgBssidPoolIsEnable);
			DBGLOG(INIT, INFO,
			       "[%s] ucNumBssidPool (%d)\n", __func__, prParams->hs20_set_bssid_pool.ucNumBssidPool);

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
	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;

	P_PARAM_KEY_T prWpiKey = (P_PARAM_KEY_T) keyStructBuf;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_testmode_set_key_ext do nothing\n");
		return -EINVAL;
	}
#endif

	memset(keyStructBuf, 0, sizeof(keyStructBuf));

	ASSERT(wiphy);

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (data == NULL || len == 0) {
		DBGLOG(INIT, TRACE, "%s data or len is invalid\n", __func__);
		return -EINVAL;
	}

	prParams = (P_NL80211_DRIVER_SET_KEY_EXTS) data;
	prIWEncExt = (struct iw_encode_exts *)&prParams->ext;

	if (prIWEncExt->alg == IW_ENCODE_ALG_SMS4) {
		/* KeyID */
		prWpiKey->u4KeyIndex = prParams->key_index;
		prWpiKey->u4KeyIndex--;
		if (prWpiKey->u4KeyIndex > 1) {
			/* key id is out of range */
			
			return -EINVAL;
		}

		if (prIWEncExt->key_len != 32) {
			/* key length not valid */
			
			return -EINVAL;
		}
		prWpiKey->u4KeyLength = prIWEncExt->key_len;

		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_SET_TX_KEY &&
			!(prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY)) {
			/* WAI seems set the STA group key with IW_ENCODE_EXT_SET_TX_KEY !!!!*/
			/* Ignore the group case */
			prWpiKey->u4KeyIndex |= BIT(30);
			prWpiKey->u4KeyIndex |= BIT(31);
			/* BSSID */
			memcpy(prWpiKey->arBSSID, prIWEncExt->addr, 6);
		} else {
			COPY_MAC_ADDR(prWpiKey->arBSSID, aucBCAddr);
		}

		/* PN */
		/* memcpy(prWpiKey->rKeyRSC, prIWEncExt->tx_seq, IW_ENCODE_SEQ_MAX_SIZE * 2); */

		memcpy(prWpiKey->aucKeyMaterial, prIWEncExt->key, 32);

		prWpiKey->u4Length = sizeof(PARAM_KEY_T);
		prWpiKey->ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
		prWpiKey->ucCipher = CIPHER_SUITE_WPI;

		rstatus = kalIoctl(prGlueInfo,
				   wlanoidSetAddKey, prWpiKey, sizeof(PARAM_KEY_T), FALSE, FALSE, TRUE, &u4BufLen);

		if (rstatus != WLAN_STATUS_SUCCESS) {
			
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
	UINT_32 u4BufLen;
	UINT_32 u4LinkScore;
	UINT_32 u4TotalError;
	UINT_32 u4TxExceedThresholdCount;
	UINT_32 u4TxTotalCount;

	P_NL80211_DRIVER_GET_STA_STATISTICS_PARAMS prParams = NULL;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	struct sk_buff *skb;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
		"chip resetting, mtk_cfg80211_testmode_get_sta_statistics do nothing\n");
	}
#endif

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
		DBGLOG(QM, TRACE, "%s allocate skb failed:%lx\n", __func__, rStatus);
		return -ENOMEM;
	}
	DBGLOG(QM, TRACE, "Get [" MACSTR "] STA statistics\n", MAC2STR(prParams->aucMacAddr));

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

	{
		u8 __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_INVALID, sizeof(u8), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u8 __tmp = NL80211_DRIVER_TESTMODE_VERSION;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_VERSION, sizeof(u8), &__tmp) < 0))
			goto nla_put_failure;
	}
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN, prParams->aucMacAddr) < 0))
		goto nla_put_failure;
	{
		u8 __tmp = u4LinkScore;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE, sizeof(u8), &__tmp) < 0))
			goto nla_put_failure;
	}
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN, prParams->aucMacAddr) < 0))
		goto nla_put_failure;
	{
		u32 __tmp = rQueryStaStatistics.u4Flag;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_FLAG, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* FW part STA link status */
	{
		u8 __tmp = rQueryStaStatistics.ucPer;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_PER, sizeof(u8), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u8 __tmp = rQueryStaStatistics.ucRcpi;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_RSSI, sizeof(u8), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4PhyMode;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_PHY_MODE, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u16 __tmp = rQueryStaStatistics.u2LinkSpeed;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TX_RATE, sizeof(u16), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxFailCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxLifeTimeoutCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageAirTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Driver part link status */
	{
		u32 __tmp = rQueryStaStatistics.u4TxTotalCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxExceedThresholdCount;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageProcessTime;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME, sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Network counter */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
		sizeof(rQueryStaStatistics.au4TcResourceEmptyCount), rQueryStaStatistics.au4TcResourceEmptyCount) < 0))
		goto nla_put_failure;

	/* Sta queue length */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,
		sizeof(rQueryStaStatistics.au4TcQueLen), rQueryStaStatistics.au4TcQueLen) < 0))
		goto nla_put_failure;

	/* Global QM counter */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
		sizeof(rQueryStaStatistics.au4TcAverageQueLen), rQueryStaStatistics.au4TcAverageQueLen) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,
		sizeof(rQueryStaStatistics.au4TcCurrentQueLen), rQueryStaStatistics.au4TcCurrentQueLen) < 0))
		goto nla_put_failure;

	/* Reserved field */
	if (unlikely(nla_put(skb, NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,
		sizeof(rQueryStaStatistics.au4Reserved), rQueryStaStatistics.au4Reserved) < 0))
		goto nla_put_failure;

	return cfg80211_testmode_reply(skb);

nla_put_failure:
	/* nal_put_skb_fail */
		kfree_skb(skb);
		return -EFAULT;
}

int mtk_cfg80211_testmode_sw_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_SW_CMD_PARAMS prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_sw_cmd\n");
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (data && len)
		prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) data;

	if (prParams) {
		if (prParams->set == 1) {
			rstatus = kalIoctl(prGlueInfo,
					   (PFN_OID_HANDLER_FUNC) wlanoidSetSwCtrlWrite,
					   &prParams->adr, (UINT_32) 8, FALSE, FALSE, TRUE, &u4SetInfoLen);
		}
	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

static int mtk_wlan_cfg_testmode_cmd(struct wiphy *wiphy, void *data, int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_MODE_PARAMS prParams = NULL;
	INT_32 i4Status;

	ASSERT(wiphy);
	DBGLOG(INIT, INFO, "-->%s()\n", __func__);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_wlan_cfg_testmode_cmd\n");
#endif

	if (!data || !len) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_cmd null data\n");
		return -EINVAL;
	}

	if (!wiphy) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_cmd null wiphy\n");
		return -EINVAL;
	}

	prGlueInfo = (P_GLUE_INFO_T)wiphy_priv(wiphy);
	prParams = (P_NL80211_DRIVER_TEST_MODE_PARAMS)data;

	/* Clear the version byte */
	prParams->index = prParams->index & ~BITS(24, 31);

	switch (prParams->index) {
	case TESTMODE_CMD_ID_SW_CMD:	/* SW cmd */
		i4Status = mtk_cfg80211_testmode_sw_cmd(wiphy, data, len);
		break;
	case TESTMODE_CMD_ID_WAPI:	/* WAPI */
#if CFG_SUPPORT_WAPI
		i4Status = mtk_cfg80211_testmode_set_key_ext(wiphy, data, len);
#endif
		break;
	case 0x10:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
		break;

#if CFG_SUPPORT_PASSPOINT
	case TESTMODE_CMD_ID_HS20:
		i4Status = mtk_cfg80211_testmode_hs20_cmd(wiphy, data, len);
		break;
#endif /* CFG_SUPPORT_PASSPOINT */

	default:
		i4Status = -EINVAL;
		break;
	}

	if (i4Status != 0)
		DBGLOG(REQ, TRACE, "prParams->index=%d, status=%d\n",
		       prParams->index, i4Status);

	return i4Status;
}

#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy, struct wireless_dev *wdev,
			      void *data, int len)
{
	ASSERT(wdev);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_cmd\n");
#endif

	return mtk_wlan_cfg_testmode_cmd(wiphy, data, len);
}
#else
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy, void *data, int len)
{
#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_cmd\n");
#endif

	return mtk_wlan_cfg_testmode_cmd(wiphy, data, len);
}
#endif
#endif

int
mtk_cfg80211_sched_scan_start(IN struct wiphy *wiphy,
			      IN struct net_device *ndev, IN struct cfg80211_sched_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 i, u4BufLen;
	P_PARAM_SCHED_SCAN_REQUEST prSchedScanRequest;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_sched_scan_start do nothing\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL || prGlueInfo->prSchedScanRequest != NULL) {
		DBGLOG(SCN, INFO, "(prGlueInfo->prScanRequest != NULL || prGlueInfo->prSchedScanRequest != NULL)\n");
		return -EBUSY;
	} else if (request == NULL || request->n_match_sets > CFG_SCAN_SSID_MATCH_MAX_NUM) {
		DBGLOG(SCN, INFO, "(request == NULL || request->n_match_sets > CFG_SCAN_SSID_MATCH_MAX_NUM)\n");
		/* invalid scheduled scan request */
		return -EINVAL;
	} else if (!request->n_ssids || !request->n_match_sets) {
		/* invalid scheduled scan request */
		return -EINVAL;
	}

	prSchedScanRequest = (P_PARAM_SCHED_SCAN_REQUEST) kalMemAlloc(sizeof(PARAM_SCHED_SCAN_REQUEST), VIR_MEM_TYPE);
	if (prSchedScanRequest == NULL) {
		DBGLOG(SCN, INFO, "(prSchedScanRequest == NULL) kalMemAlloc fail\n");
		return -ENOMEM;
	}

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

	prSchedScanRequest->u4IELength = request->ie_len;
	if (request->ie_len > 0)
		prSchedScanRequest->pucIE = (PUINT_8) (request->ie);

#if KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE
	prSchedScanRequest->u2ScanInterval = (UINT_16) (request->scan_plans->interval);
#else
	prSchedScanRequest->u2ScanInterval = (UINT_16) (request->interval);
#endif
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetStartSchedScan,
			   prSchedScanRequest, sizeof(PARAM_SCHED_SCAN_REQUEST), FALSE, FALSE, TRUE, &u4BufLen);

	kalMemFree(prSchedScanRequest, VIR_MEM_TYPE, sizeof(PARAM_SCHED_SCAN_REQUEST));

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "scheduled scan error:%x\n", rStatus);
		return -EINVAL;
	}

	prGlueInfo->prSchedScanRequest = request;

	return 0;
}

int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy, IN struct net_device *ndev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_sched_scan_stop do nothing\n");
		return -EINVAL;
	}
#endif

	/* check if there is any pending scan/sched_scan not yet finished */
	if (prGlueInfo->prSchedScanRequest == NULL)
		return -EBUSY;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetStopSchedScan, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_FAILURE) {
		DBGLOG(REQ, WARN, "scheduled scan error:0x%x\n", rStatus);
		return -EINVAL;
	}

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
	UINT_8 arBssid[PARAM_MAC_ADDR_LEN];
#if CFG_SUPPORT_PASSPOINT
	PUINT_8 prDesiredIE = NULL;
#endif /* CFG_SUPPORT_PASSPOINT */
	UINT_32 rStatus;
	UINT_32 u4BufLen;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR,
			"chip resetting, mtk_cfg80211_assoc do nothing\n");
		return -EINVAL;
	}
#endif

#if CFG_SUPPORT_CFG80211_AUTH
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;
	ENUM_PARAM_AUTH_MODE_T eAuthMode;
	UINT_32 cipher;
	UINT_32 i, u4AkmSuite;
	P_DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY prEntry;
	P_CONNECTION_SETTINGS_T prConnSettings = NULL;
	PUINT_8 prDesiredIE = NULL;
	PUINT_8 pucIEStart = NULL;
	RSN_INFO_T rRsnInfo;
	P_STA_RECORD_T prStaRec = NULL;
#endif
	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if CFG_SUPPORT_CFG80211_AUTH
	prConnSettings = &prGlueInfo->prAdapter->rWifiVar.rConnSettings;

	/* [todo]temp use for indicate rx assoc resp, may need to be modified */

	/* The BSS from cfg80211_ops.assoc must give back to
	 * cfg80211_send_rx_assoc() or to cfg80211_assoc_timeout().
	 * To ensure proper refcounting, new association requests
	 * while already associating must be rejected.
	 */
	if (prConnSettings->bss)
		return -ENOENT;
	prConnSettings->bss = req->bss;
#endif
	DBGLOG(REQ, INFO, "mtk_cfg80211_assoc, media state:%d\n",
				prGlueInfo->eParamMediaStateIndicated);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	if (prGlueInfo->eParamMediaStateIndicated ==
		PARAM_MEDIA_STATE_CONNECTED) {
		wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
			&arBssid[0], sizeof(arBssid), &u4BufLen);

		/* 1. check BSSID */
		if (UNEQUAL_MAC_ADDR(arBssid, req->bss->bssid)) {
			/* wrong MAC address */
			DBGLOG(REQ, WARN, "incorrect BSSID: [" MACSTR
				"] currently connected BSSID[" MACSTR "]\n",
				MAC2STR(req->bss->bssid), MAC2STR(arBssid));
			return -ENOENT;
		}
	}
#if CFG_SUPPORT_CFG80211_AUTH
	/* <1> Reset WPA info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4CipherGroupMgmt = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo.ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
#endif
	prGlueInfo->rWpaInfo.ucRsneLen = 0;

	/* 2.Fill WPA version */
	if (req->crypto.wpa_versions & NL80211_WPA_VERSION_1)
		prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_WPA;
	else if (req->crypto.wpa_versions & NL80211_WPA_VERSION_2)
		prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_WPA2;
	else
		prGlueInfo->rWpaInfo.u4WpaVersion =
					IW_AUTH_WPA_VERSION_DISABLED;
	DBGLOG(REQ, INFO, "wpa ver=%d\n", prGlueInfo->rWpaInfo.u4WpaVersion);

	/* 3.Fill pairwise cipher suite */
	if (req->crypto.n_ciphers_pairwise) {
		DBGLOG(RSN, INFO, "[wlan] cipher pairwise (%x)\n",
			req->crypto.ciphers_pairwise[0]);

		prGlueInfo->prAdapter->rWifiVar.rConnSettings.rRsnInfo
			.au4PairwiseKeyCipherSuite[0] =
			req->crypto.ciphers_pairwise[0];
		switch (req->crypto.ciphers_pairwise[0]) {
		case WLAN_CIPHER_SUITE_WEP40:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			prGlueInfo->rWpaInfo.u4CipherPairwise =
				IW_AUTH_CIPHER_GCMP256;
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher pairwise (%d)\n",
				req->crypto.ciphers_pairwise[0]);
			return -EINVAL;
		}
	}

	/* 4. Fill group cipher suite */
	if (req->crypto.cipher_group) {
		DBGLOG(RSN, INFO, "[wlan] cipher group (%x)\n",
			req->crypto.cipher_group);
		prGlueInfo->prAdapter->rWifiVar.rConnSettings.rRsnInfo
			.u4GroupKeyCipherSuite = req->crypto.cipher_group;
		switch (req->crypto.cipher_group) {
		case WLAN_CIPHER_SUITE_WEP40:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			prGlueInfo->rWpaInfo.u4CipherGroup =
				IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			break;
		default:
			DBGLOG(REQ, WARN, "invalid cipher group (%d)\n",
				req->crypto.cipher_group);
			return -EINVAL;
		}
	}

	/* 5. Fill encryption status */
	cipher = prGlueInfo->rWpaInfo.u4CipherGroup |
			prGlueInfo->rWpaInfo.u4CipherPairwise;
	if (1 /* prGlueInfo->rWpaInfo.fgPrivacyInvoke */) {
		if (cipher & IW_AUTH_CIPHER_GCMP256) {
			eEncStatus = ENUM_ENCRYPTION4_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_CCMP) {
			eEncStatus = ENUM_ENCRYPTION3_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_TKIP) {
			eEncStatus = ENUM_ENCRYPTION2_ENABLED;
		} else if (cipher & (IW_AUTH_CIPHER_WEP104 |
					IW_AUTH_CIPHER_WEP40)) {
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

	rStatus = kalIoctl(prGlueInfo, wlanoidSetEncryptionStatus, &eEncStatus,
		sizeof(eEncStatus), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set encryption mode error:%x\n", rStatus);

	/* 6. Fill AKM suites */
	u4AkmSuite = 0;
	eAuthMode = 0;
	DBGLOG(REQ, INFO, "request numbers of Akm Suite:%d\n",
		req->crypto.n_akm_suites);
	for (i = 0; i < req->crypto.n_akm_suites; i++)
		DBGLOG(REQ, INFO, "request Akm Suite[%d]:%d\n",
			i, req->crypto.akm_suites[i]);

	if (req->crypto.n_akm_suites) {
		prGlueInfo->prAdapter->rWifiVar.rConnSettings.rRsnInfo
			.au4AuthKeyMgtSuite[0] = req->crypto.akm_suites[0];
		DBGLOG(REQ, INFO, "Akm Suite:%d\n", req->crypto.akm_suites[0]);

		if (prGlueInfo->rWpaInfo.u4WpaVersion ==
			IW_AUTH_WPA_VERSION_WPA) {
			switch (req->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				eAuthMode = AUTH_MODE_WPA;
				u4AkmSuite = WPA_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				eAuthMode = AUTH_MODE_WPA_PSK;
				u4AkmSuite = WPA_AKM_SUITE_PSK;
				break;
			default:
				DBGLOG(REQ, WARN, "invalid Akm Suite (%08x)\n",
					req->crypto.akm_suites[0]);
				return -EINVAL;
			}
		} else if (prGlueInfo->rWpaInfo.u4WpaVersion ==
			IW_AUTH_WPA_VERSION_WPA2) {
			switch (req->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				eAuthMode = AUTH_MODE_WPA2;
				u4AkmSuite = RSN_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_PSK;
				break;
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
			case WLAN_AKM_SUITE_8021X_SUITE_B:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;

			case WLAN_AKM_SUITE_8021X_SUITE_B_192:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;
#if CFG_SUPPORT_SAE
			/* Need to add in WPA also? */
			case WLAN_AKM_SUITE_SAE:
				eAuthMode = AUTH_MODE_WPA2_SAE;
				u4AkmSuite = RSN_AKM_SUITE_SAE;
			break;
#endif
#if CFG_SUPPORT_OWE
			case WLAN_AKM_SUITE_OWE:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_OWE;
			break;
#endif
			default:
				DBGLOG(REQ, WARN, "invalid Akm Suite (%08x)\n",
					req->crypto.akm_suites[0]);
				return -EINVAL;
			}
		}
	}
	if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		eAuthMode = (prGlueInfo->rWpaInfo.u4AuthAlg ==
			IW_AUTH_ALG_OPEN_SYSTEM) ?
			AUTH_MODE_OPEN : AUTH_MODE_AUTO_SWITCH;
	}

	DBGLOG(REQ, INFO, "set auth mode:%d, akm suite:0x%x\n",
		eAuthMode, u4AkmSuite);

	/* 6.1 Set auth mode*/
	rStatus = kalIoctl(prGlueInfo, wlanoidSetAuthMode, &eAuthMode,
			sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set auth mode error:%x\n", rStatus);

	/* 6.2 Enable the specific AKM suite only. */
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prGlueInfo->prAdapter
			->rMib.dot11RSNAConfigAuthenticationSuitesTable[i];

		if (prEntry->dot11RSNAConfigAuthenticationSuite == u4AkmSuite) {
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled
				= TRUE;
			DBGLOG(REQ, INFO, "match AuthenticationSuite = 0x%x",
				u4AkmSuite);
		} else {
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled
				= FALSE;
		}
	}
#endif

	/* 7. Parsing desired ie from upper layer */
	prGlueInfo->fgWpsActive = FALSE;

	if (req->ie && req->ie_len > 0) {
#if CFG_SUPPORT_CFG80211_AUTH
		pucIEStart = (PUINT_8)req->ie;
#endif

#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE((PUINT_8) req->ie, req->ie_len,
			(PPUINT_8) & prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE),
					   FALSE, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, TRACE, ("[HS20] set HS20 assoc "
				 * "info error:%x\n", rStatus));
				 */
			}
		}

		if (wextSrchDesiredInterworkingIE((PUINT_8) req->ie,
			req->ie_len, (PPUINT_8) & prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetInterworkingInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, TRACE, ("[HS20] set Interworking"
				 * " assoc info error:%x\n", rStatus));
				 */
			}
		}

		if (wextSrchDesiredRoamingConsortiumIE((PUINT_8) req->ie,
			req->ie_len, (PPUINT_8) & prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRoamingConsortiumIEInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE), FALSE, FALSE, TRUE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, TRACE, ("[HS20] set
				 * RoamingConsortium assoc info error:%x\n",
				 * rStatus));
				 */
			}
		}
#endif /* CFG_SUPPORT_PASSPOINT */
#if CFG_SUPPORT_CFG80211_AUTH
		if (wextSrchDesiredWPAIE(pucIEStart, req->ie_len, 0x30,
			(PPUINT_8) & prDesiredIE)) {
			if (rsnParseRsnIE(prGlueInfo->prAdapter,
				(P_RSN_INFO_ELEM_T)prDesiredIE, &rRsnInfo)) {
#if CFG_SUPPORT_802_11W
				/* Fill RSNE MFP Cap */
				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
					prGlueInfo->rWpaInfo.u4CipherGroupMgmt
						= rRsnInfo
						.u4GroupMgmtKeyCipherSuite;
					prGlueInfo->rWpaInfo.ucRSNMfpCap =
						RSN_AUTH_MFP_OPTIONAL;
					if (rRsnInfo.u2RsnCap &
						ELEM_WPA_CAP_MFPR)
						prGlueInfo->rWpaInfo.ucRSNMfpCap
						= RSN_AUTH_MFP_REQUIRED;
				} else
					prGlueInfo->rWpaInfo.ucRSNMfpCap =
							RSN_AUTH_MFP_DISABLED;
#endif
			prGlueInfo->rWpaInfo.ucRsneLen = rRsnInfo.ucRsneLen;

			/* Fill RSNE PMKID Count and List */
			prConnSettings->rRsnInfo.u2PmkidCnt =
				rRsnInfo.u2PmkidCnt;
			if (rRsnInfo.u2PmkidCnt > 0)
				kalMemCopy(prConnSettings
					->rRsnInfo.aucPmkidList,
					rRsnInfo.aucPmkidList,
					(rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN));

			}
		}

#if CFG_SUPPORT_OWE
		/* Gen OWE IE */
		if (wextSrchDesiredWPAIE(pucIEStart, req->ie_len, 0xff,
			(PPUINT_8) & prDesiredIE)) {
			UINT_8 ucLength = (*(prDesiredIE+1)+2);

			kalMemCopy(&prGlueInfo->prAdapter
				->rWifiVar.rConnSettings.rOweInfo,
				prDesiredIE, ucLength);

			DBGLOG(REQ, INFO, "DUMP OWE INFO, EID %x length %x\n",
				*prDesiredIE, ucLength);
			DBGLOG_MEM8(REQ, INFO, &prGlueInfo->prAdapter
				->rWifiVar.rConnSettings.rOweInfo, ucLength);
		} else {
			kalMemSet(&prGlueInfo->prAdapter
				->rWifiVar.rConnSettings.rOweInfo,
				0, sizeof(struct OWE_INFO_T));
		}
#endif
#endif
	}
	/* Fill WPA info - mfp setting */
	/* Must put after paring RSNE from upper layer
	* for prGlueInfo->rWpaInfo.ucRSNMfpCap assignment
	*/
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
	if (req->use_mfp)
		prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_REQUIRED;
	else {
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
	}
	/* DBGLOG(REQ, INFO, "MFP=%d\n", prGlueInfo->rWpaInfo.u4Mfp); */
#endif

#if CFG_SUPPORT_CFG80211_AUTH
	/*[TODO]may to check if assoc parameters change as cfg80211_auth*/
	prConnSettings->fgIsSendAssoc = TRUE;
	if (!prConnSettings->fgIsConnInitialized) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetBssid,
			(void *) req->bss->bssid, MAC_ADDR_LEN,
			FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "set BSSID:%x\n", rStatus);
			return -EINVAL;
		}
	} else { /* skip join initial flow when it has been completed*/
		prStaRec = cnmGetStaRecByAddress(prGlueInfo->prAdapter,
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex,
			req->bss->bssid);

		if (prStaRec)
			saaSendAuthAssoc(prGlueInfo->prAdapter, prStaRec);
		else
			DBGLOG(REQ, WARN,
				"can't send auth since can't find StaRec\n");
	}
#else
	rStatus = kalIoctl(prGlueInfo, wlanoidSetBssid,
			(void *)req->bss->bssid, MAC_ADDR_LEN,
			FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set BSSID:0x%x\n", rStatus);
		return -EINVAL;
	}
#endif
	return 0;
}

#if CFG_SUPPORT_NFC_BEAM_PLUS

int mtk_cfg80211_testmode_get_scan_done(IN struct wiphy *wiphy, IN void *data, IN int len, IN P_GLUE_INFO_T prGlueInfo)
{
#define NL80211_TESTMODE_P2P_SCANDONE_INVALID 0
#define NL80211_TESTMODE_P2P_SCANDONE_STATUS 1

#ifdef CONFIG_NL80211_TESTMODE
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Status = -EINVAL, READY_TO_BEAM = 0;

	struct sk_buff *skb = NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_get_scan_done\n");
#endif

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
		DBGLOG(QM, TRACE, "%s allocate skb failed:%lx\n", __func__, rStatus);
		return -ENOMEM;
	}
	{
		u8 __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_INVALID, sizeof(u8), &__tmp) < 0)) {
			kfree_skb(skb);
			goto nla_put_failure;
		}
	}
	{
		u32 __tmp = READY_TO_BEAM;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_STATUS, sizeof(u32), &__tmp) < 0)) {
			kfree_skb(skb);
			goto nla_put_failure;
		}
	}

	i4Status = cfg80211_testmode_reply(skb);

nla_put_failure:
	return i4Status;

#else
	DBGLOG(QM, WARN, "CONFIG_NL80211_TESTMODE not enabled\n");
	return -EINVAL;
#endif

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
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_change_station\n");
		return -EINVAL;
	}
#endif

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
#else
int
mtk_cfg80211_change_station(struct wiphy *wiphy, struct net_device *ndev, u8 *mac, struct station_parameters *params)
{

	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	P_GLUE_INFO_T prGlueInfo = NULL;
	CMD_PEER_UPDATE_T rCmdUpdate;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen, u4Temp;
	ADAPTER_T *prAdapter;
	P_BSS_INFO_T prAisBssInfo;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_change_station\n");
		return -EINVAL;
	}
#endif

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
#endif
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
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_add_station\n");
		return -EINVAL;
	}
#endif

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
#else
int mtk_cfg80211_add_station(struct wiphy *wiphy, struct net_device *ndev, u8 *mac, struct station_parameters *params)
{
	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	P_GLUE_INFO_T prGlueInfo = NULL;
	CMD_PEER_ADD_T rCmdCreate;
	ADAPTER_T *prAdapter;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_add_station\n");
		return -EINVAL;
	}
#endif

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
#endif
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
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
int mtk_cfg80211_del_station(struct wiphy *wiphy, struct net_device *ndev, struct station_del_parameters *params)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	const u8 *mac = params->mac ? params->mac : bcast_addr;
	P_GLUE_INFO_T prGlueInfo = NULL;
	ADAPTER_T *prAdapter;
	STA_RECORD_T *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_del_station\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;

	/* For kernel 3.18 modification, we trasfer to local buff to query sta */
	memset(deleteMac, 0, MAC_ADDR_LEN);
	memcpy(deleteMac, mac, MAC_ADDR_LEN);

	prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy, struct net_device *ndev, const u8 *mac)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	P_GLUE_INFO_T prGlueInfo = NULL;
	ADAPTER_T *prAdapter;
	STA_RECORD_T *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_del_station\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;

	/* For kernel 3.18 modification, we trasfer to local buff to query sta */
	memset(deleteMac, 0, MAC_ADDR_LEN);
	memcpy(deleteMac, mac, MAC_ADDR_LEN);

	prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#endif
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy, struct net_device *ndev, u8 *mac)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	P_GLUE_INFO_T prGlueInfo = NULL;
	ADAPTER_T *prAdapter;
	STA_RECORD_T *prStaRec;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_del_station\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;

	prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) prAdapter->prAisBssInfo->ucBssIndex, mac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#endif

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
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       bool initiator, const u8 *buf, size_t len)
{
	GLUE_INFO_T *prGlueInfo;
	TDLS_CMD_LINK_MGT_T rCmdMgt;
	UINT_32 u4BufLen;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_tdls_mgmt\n");
		return -EINVAL;
	}
#endif

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

	if  (len > TDLS_SEC_BUF_LENGTH) {
		DBGLOG(REQ, WARN, "%s:len > TDLS_SEC_BUF_LENGTH\n", __func__);
		return -EINVAL;
	}

	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);

	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt, sizeof(TDLS_CMD_LINK_MGT_T), FALSE, FALSE, FALSE,
		 /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
		 &u4BufLen);
	return 0;

}
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       const u8 *buf, size_t len)
{
	GLUE_INFO_T *prGlueInfo;
	TDLS_CMD_LINK_MGT_T rCmdMgt;
	UINT_32 u4BufLen;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_tdls_mgmt\n");
		return -EINVAL;
	}
#endif

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

#else
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
					u8 *peer, u8 action_code, u8 dialog_token,
					u16 status_code, const u8 *buf, size_t len)
{
	GLUE_INFO_T *prGlueInfo;
	TDLS_CMD_LINK_MGT_T rCmdMgt;
	UINT_32 u4BufLen;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_tdls_mgmt\n");
		return -EINVAL;
	}
#endif

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
	if	(len > TDLS_SEC_BUF_LENGTH)
		DBGLOG(REQ, WARN, "In mtk_cfg80211_tdls_mgmt , len > TDLS_SEC_BUF_LENGTH, please check\n");
	else
		kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);

	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt, sizeof(TDLS_CMD_LINK_MGT_T), FALSE, FALSE, FALSE,
		 /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
		 &u4BufLen);
	return 0;

}
#endif
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
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
			   const u8 *peer, enum nl80211_tdls_operation oper)
{

	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen;
	ADAPTER_T *prAdapter;
	TDLS_CMD_LINK_OPER_T rCmdOper;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_tdls_oper\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper = oper;

	if (oper == NL80211_TDLS_DISABLE_LINK) {
		/* [ALPS03767042] wlan: fix TDLS 5.3 test issue
		 * [Detail]
		 * Timing issue of data direct path design
		 *   - Data sent directly through HW (new design )
		 *   - Command sent to FW to process (original design)
		 * Issue occurs while
		 *   - Tear down packet sent by wlanHardStartXmit(),
		 *       but not real sent out
		 *   - CMD_ID_REMOVE_STA_RECORD sent to FW to disable TDLS link
		 * [Solution]
		 * Short-term
		 *   - Delay TDLS disable link to let tear down data package
		 *     to send
		 * Long-term
		 *   - Enhance the TDLS flow to guarantee TX can send out
		 *     successfully
		 */
		DBGLOG(TDLS, INFO, "NL80211_TDLS_DISABLE_LINK, kalMsleep(20)");
		kalMsleep(20);
	}

	kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper,
		sizeof(TDLS_CMD_LINK_OPER_T),
		FALSE, FALSE, FALSE, &u4BufLen);

	return 0;
}
#else
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev, u8 *peer, enum nl80211_tdls_operation oper)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen;
	ADAPTER_T *prAdapter;
	TDLS_CMD_LINK_OPER_T rCmdOper;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_tdls_oper\n");
		return -EINVAL;
	}
#endif

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper = oper;

	if (oper == NL80211_TDLS_DISABLE_LINK) {
		/* [ALPS03767042] wlan: fix TDLS 5.3 test issue
		 * [Detail]
		 * Timing issue of data direct path design
		 *   - Data sent directly through HW (new design )
		 *   - Command sent to FW to process (original design)
		 * Issue occurs while
		 *   - Tear down packet sent by wlanHardStartXmit(),
		 *       but not real sent out
		 *   - CMD_ID_REMOVE_STA_RECORD sent to FW to disable TDLS link
		 * [Solution]
		 * Short-term
		 *   - Delay TDLS disable link to let tear down data package
		 *     to send
		 * Long-term
		 *   - Enhance the TDLS flow to guarantee TX can send out
		 *     successfully
		 */
		DBGLOG(TDLS, INFO, "NL80211_TDLS_DISABLE_LINK, kalMsleep(20)");
		kalMsleep(20);
	}

	kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper,
		sizeof(TDLS_CMD_LINK_OPER_T),
		FALSE, FALSE, FALSE, &u4BufLen);

	return 0;
}
#endif
#endif
#if (CFG_SUPPORT_SINGLE_SKU == 1)

#if (CFG_BUILT_IN_DRIVER == 0)
bool is_world_regdom(char *alpha2)
{
	if (!alpha2)
		return false;

	return (alpha2[0] == '0') && (alpha2[1] == '0');
}
#endif

enum regd_state regd_state_machine(IN struct regulatory_request *pRequest)
{
	switch (pRequest->initiator) {
	case NL80211_REGDOM_SET_BY_USER:
		DBGLOG(RLM, INFO, "regd_state_machine: SET_BY_USER\n");

		return rlmDomainStateTransition(REGD_STATE_SET_COUNTRY_USER, pRequest);

	case NL80211_REGDOM_SET_BY_DRIVER:
		DBGLOG(RLM, INFO, "regd_state_machine: SET_BY_DRIVER\n");

		return rlmDomainStateTransition(REGD_STATE_SET_COUNTRY_DRIVER, pRequest);

	case NL80211_REGDOM_SET_BY_CORE:
		DBGLOG(RLM, INFO, "regd_state_machine: NL80211_REGDOM_SET_BY_CORE\n");

		return rlmDomainStateTransition(REGD_STATE_SET_WW_CORE, pRequest);

	case NL80211_REGDOM_SET_BY_COUNTRY_IE:
		DBGLOG(RLM, WARN, "regd_state_machine: SET_BY_COUNTRY_IE\n");

		return rlmDomainStateTransition(REGD_STATE_SET_COUNTRY_IE, pRequest);

	default:
		return rlmDomainStateTransition(REGD_STATE_INVALID, pRequest);
	}
}


void
mtk_apply_custom_regulatory(IN struct wiphy *pWiphy,
							IN const struct ieee80211_regdomain *pRegdom)
{
	u32 band_idx, ch_idx;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;

	DBGLOG(RLM, INFO, "%s()\n", __func__);

	/* to reset cha->flags*/
	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		sband = pWiphy->bands[band_idx];
		if (!sband)
			continue;

		for (ch_idx = 0; ch_idx < sband->n_channels; ch_idx++) {
			chan = &sband->channels[ch_idx];

			/*reset chan->flags*/
			chan->flags = 0;
		}

	}

	/* update to kernel */
	wiphy_apply_custom_regulatory(pWiphy, pRegdom);

#if KERNEL_VERSION(4, 3, 0) <= CFG80211_VERSION_CODE
	/*Fix Kernel 4.3 and later bug for parser domain info*/
	for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
		sband = pWiphy->bands[band_idx];
		if (!sband)
			continue;

		for (ch_idx = 0; ch_idx < sband->n_channels; ch_idx++) {
			chan = &sband->channels[ch_idx];

			if (chan->flags & IEEE80211_CHAN_NO_20MHZ)
				chan->flags |= IEEE80211_CHAN_DISABLED;
		}
	}
#endif
}

void
mtk_reg_notify(IN struct wiphy *pWiphy,
			   IN struct regulatory_request *pRequest)
{
	P_GLUE_INFO_T prGlueInfo;
	P_ADAPTER_T prAdapter;
	u8 send_cmd_request = 0;
	enum regd_state old_state;


	if (!pWiphy) {
		DBGLOG(RLM, ERROR, "%s(): pWiphy = NULL.\n", __func__);
		return;
	}

	if (g_u4HaltFlag) {
		DBGLOG(RLM, WARN, "wlan is halt, skip reg callback\n");
		return;
	}

	/*
	 * Magic flow for driver to send inband command after kernel's calling reg_notifier callback
	 */
	if (!pRequest) {

		/*triggered by our driver in wlan initial process.*/

		if (rlmDomainIsCtrlStateEqualTo(REGD_STATE_INIT)) {
			if (rlmDomainIsUsingLocalRegDomainDataBase()) {

				DBGLOG(RLM, WARN, "County Code is not assigned. Use default WW.\n");
				goto DOMAIN_SEND_CMD;

			} else {
				DBGLOG(RLM, ERROR, "Invalid REG state happened. state = 0x%x\n",
									rlmDomainGetCtrlState());
				return;
			}
		} else if ((rlmDomainIsCtrlStateEqualTo(REGD_STATE_SET_WW_CORE))
					|| (rlmDomainIsCtrlStateEqualTo(REGD_STATE_SET_COUNTRY_USER))
					|| (rlmDomainIsCtrlStateEqualTo(REGD_STATE_SET_COUNTRY_DRIVER))) {

			send_cmd_request = 1;

			goto DOMAIN_SEND_CMD;
		} else {
			DBGLOG(RLM, ERROR, "Invalid REG state happened. state = 0x%x\n", rlmDomainGetCtrlState());
			return;
		}
	}


	/*
	 * Ignore the CORE's WW setting when using local data base of regulatory rules
	 */
	if ((pRequest->initiator == NL80211_REGDOM_SET_BY_CORE) &&
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
		(pWiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY))
#else
		(pWiphy->regulatory_flags & REGULATORY_CUSTOM_REG))
#endif
		return;/*Ignore the CORE's WW setting*/


	/*
	 * State machine transition
	 */
	DBGLOG(RLM, INFO, "request->alpha2=%s, initiator=%x, intersect=%d\n",
			pRequest->alpha2, pRequest->initiator, pRequest->intersect);

	old_state = rlmDomainGetCtrlState();
	regd_state_machine(pRequest);

	if (rlmDomainGetCtrlState() == old_state) {
		if (((old_state == REGD_STATE_SET_COUNTRY_USER) || (old_state == REGD_STATE_SET_COUNTRY_DRIVER))
			&& (!(rlmDomainIsSameCountryCode(pRequest->alpha2, sizeof(pRequest->alpha2)))))
			DBGLOG(RLM, INFO, "Set by user to NEW country code\n");
		else
			/* Change to same state or same country, ignore */
			return;
	} else if (rlmDomainIsCtrlStateEqualTo(REGD_STATE_INVALID)) {
		DBGLOG(RLM, ERROR, "\n%s():\n---> WARNING. Transit to invalid state.\n", __func__);
		DBGLOG(RLM, ERROR, "---> WARNING.\n ");
		rlmDomainAssert(0);
#if 0
		return; /*error state*/
#endif
	}

	/*
	 * Set country code
	 */
	if (pRequest->initiator != NL80211_REGDOM_SET_BY_DRIVER) {
		rlmDomainSetCountryCode(pRequest->alpha2, sizeof(pRequest->alpha2));
	} else {

		/*SET_BY_DRIVER*/

		if (rlmDomainIsEfuseUsed()) {
			if (!rlmDomainIsUsingLocalRegDomainDataBase())
				DBGLOG(RLM, WARN, "[WARNING!!!] Local DB must be used if country code from efuse.\n");
		} else {
			/* iwpriv case */
			if (rlmDomainIsUsingLocalRegDomainDataBase() &&
				(!rlmDomainIsEfuseUsed())) {
				/*iwpriv set country but local data base*/
				u32 country_code = rlmDomainGetTempCountryCode();

				rlmDomainSetCountryCode((char *)&country_code, sizeof(country_code));
			} else {
				/*iwpriv set country but query CRDA*/
				rlmDomainSetCountryCode(pRequest->alpha2, sizeof(pRequest->alpha2));
			}
		}
	}

	rlmDomainSetDfsRegion(pRequest->dfs_region);

DOMAIN_SEND_CMD:
	DBGLOG(RLM, INFO, "g_mtk_regd_control.alpha2 = 0x%x\n", rlmDomainGetCountryCode());

	/*
	 * Check if using customized regulatory rule
	 */
	if (rlmDomainIsUsingLocalRegDomainDataBase()) {
		const struct ieee80211_regdomain *pRegdom;
		u32 country_code = rlmDomainGetCountryCode();
		char alpha2[4];

		/*fetch regulatory rules from local data base*/
		alpha2[0] = country_code & 0xFF;
		alpha2[1] = (country_code >> 8) & 0xFF;
		alpha2[2] = (country_code >> 16) & 0xFF;
		alpha2[3] = (country_code >> 24) & 0xFF;

		pRegdom = rlmDomainSearchRegdomainFromLocalDataBase(alpha2);
		if (!pRegdom) {
			DBGLOG(RLM, ERROR,
			"%s(): Cannot find the correct country = %u\n",
			__func__, rlmDomainGetCountryCode());

			rlmDomainAssert(0);
			return;
		}


		mtk_apply_custom_regulatory(pWiphy, pRegdom);
	}


	/*
	 * Parsing channels
	 */
	if (send_cmd_request)
		rlmDomainParsingChannel(rlmDomainGetRefWiphy());
	else
		rlmDomainParsingChannel(pWiphy);/*real regd update*/


	/*
	 * Always use the wlan GlueInfo as parameter,
	 * because P2P stores it as a different way
	 * and I do not want to make a detection about
	 * which wiphy, wlan wiphy or p2p wiphy is.
	 */

	prGlueInfo = rlmDomainGetGlueInfo();

	/*
	 * Prepare to send channel information to firmware
	 */
	if (!prGlueInfo)
		return; /*interface is not up yet.*/

	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter)
		return; /*interface is not up yet.*/


	/*
	 * Check if firmawre support single sku
	 */
	if (!regd_is_single_sku_en())
		return; /*no need to send information to firmware due to firmware is not supported*/


	/*
	 * Send commands to firmware
	 */
	prAdapter->rWifiVar.rConnSettings.u2CountryCode = (UINT_16)rlmDomainGetCountryCode();
	rlmDomainSendCmd(prAdapter, FALSE);
}

void
cfg80211_regd_set_wiphy(IN struct wiphy *prWiphy)
{
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
#if KERNEL_VERSION(4, 3, 0) <= CFG80211_VERSION_CODE
	u32 band_idx, ch_idx;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;
#endif
#endif

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "cfg80211_regd_set_wiphy\n");
		return;
	}
#endif

	/*
	 * register callback
	 */
	prWiphy->reg_notifier = mtk_reg_notify;


	/*
	 * clear REGULATORY_CUSTOM_REG flag
	 */
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	/*tells kernel that assign WW as default*/
	prWiphy->flags &= ~(WIPHY_FLAG_CUSTOM_REGULATORY);
#else
	prWiphy->regulatory_flags &= ~(REGULATORY_CUSTOM_REG);

	/*ignore the hint from IE*/
	prWiphy->regulatory_flags |= REGULATORY_COUNTRY_IE_IGNORE;

#ifdef CFG_SUPPORT_DISABLE_BCN_HINTS
	/*disable beacon hint to avoid channel flag be changed*/
	prWiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
#endif
#endif


	/*
	 * set REGULATORY_CUSTOM_REG flag
	 */
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	/*tells kernel that assign WW as default*/
	prWiphy->flags |= (WIPHY_FLAG_CUSTOM_REGULATORY);
#else
	prWiphy->regulatory_flags |= (REGULATORY_CUSTOM_REG);
#endif
	/* assigned a defautl one */
	if (rlmDomainGetLocalDefaultRegd()) {
		wiphy_apply_custom_regulatory(prWiphy, rlmDomainGetLocalDefaultRegd());

#if KERNEL_VERSION(4, 3, 0) <= CFG80211_VERSION_CODE
		/*Fix Kernel 4.3 and later bug for parser domain info*/
		for (band_idx = 0; band_idx < KAL_NUM_BANDS; band_idx++) {
			sband = prWiphy->bands[band_idx];
			if (!sband)
				continue;

			for (ch_idx = 0; ch_idx < sband->n_channels; ch_idx++) {
				chan = &sband->channels[ch_idx];

				if (chan->flags & IEEE80211_CHAN_NO_20MHZ)
					chan->flags |= IEEE80211_CHAN_DISABLED;
			}
		}
#endif

	}
#endif


	/*
	 * Initialize regd control information
	 */
	rlmDomainResetCtrlInfo();
}

#else
void
cfg80211_regd_set_wiphy(IN struct wiphy *prWiphy)
{
}
#endif

int mtk_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	P_GLUE_INFO_T prGlueInfo;
	ADAPTER_T *prAdapter;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState()) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_suspend\n");
		return -EINVAL;
	}
#endif

	DBGLOG(REQ, INFO, "CFG80211 suspend CB\n");
	if (!wlanGetGlueInfo()) {
		DBGLOG(REQ, ERROR, "NIC does not exist!\n");
		return 0;
	}

	prGlueInfo = (P_GLUE_INFO_T) wiphy_priv(wiphy);

	atomic_set(&prGlueInfo->cfgSuspend, 1);
	DBGLOG(REQ, EVENT, "cfg80211 ops block\n");

	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, WARN, "Wow:%d, WowEnable:%d, AdvPws:%d, state:%d\n",
		prGlueInfo->prAdapter->rWifiVar.ucWow, prGlueInfo->prAdapter->rWowCtrl.fgWowEnable,
		prGlueInfo->prAdapter->rWifiVar.ucAdvPws,
		kalGetMediaStateIndicated(prGlueInfo));

	/* 1) wifi cfg "Wow" must be true,
	 * 2) wow is disable
	 * 3) AdvPws is disable
	 * 4) WIfI connected => execute link down flow
	 */
	if (prGlueInfo->prAdapter->rWifiVar.ucWow
		&& !prGlueInfo->prAdapter->rWowCtrl.fgWowEnable
		&& !prGlueInfo->prAdapter->rWifiVar.ucAdvPws) {
		if (kalGetMediaStateIndicated(prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
			DBGLOG(REQ, WARN, "CFG80211 suspend link down\n");
			rStatus = kalIoctl(prGlueInfo, wlanoidLinkDown, NULL, 0, TRUE, FALSE, FALSE, &u4BufLen);
		}
	}

	/* In current design, only support AIS connection during suspend only.
	 * It need to add flow to deactive P2P (GC/GO) link during suspend flow.
	 * Otherwise, MT7668 would fail to enter deep sleep.
	 */
	p2pProcessPreSuspendFlow(prAdapter);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "cfg 80211 suspend fail!\n");
		return -EINVAL;
	}
	return 0;
}

/*-----------------------------------------------------------------------*/
/*!
 * @brief This function goes through the provided ies buffer, and
 *        collects those non-wfa vendor specific ies into driver's
 *        internal buffer (non_wfa_vendor_ie_buf), to be sent in
 *        AssocReq in AIS mode.
 *        The non-wfa vendor specific ies are those with ie_id = 0xdd
 *        and ouis are different from wfa's oui. (i.e., it could be
 *        customer's vendor ie ...etc.
 *
 * @param prGlueInfo    driver's private glueinfo
 *        ies           ie buffer
 *        len           length of ie
 *
 * @retval length of the non_wfa vendor ie
 */
/*-----------------------------------------------------------------------*/
int cfg80211_get_non_wfa_vendor_ie(P_GLUE_INFO_T prGlueInfo, u8 *ies, int len)
{
	const u8 *pos = ies, *end = ies+len;
	struct ieee80211_vendor_ie *ie;
	int ie_oui = 0;
	u32 *ret_len, max_len;
	u8 *w_pos;

	if (!prGlueInfo || !ies || !len)
		return 0;
	w_pos = prGlueInfo->non_wfa_vendor_ie_buf;
	ret_len = &prGlueInfo->non_wfa_vendor_ie_len;
	max_len = sizeof(prGlueInfo->non_wfa_vendor_ie_buf);

	while (pos < end) {
		pos = cfg80211_find_ie(WLAN_EID_VENDOR_SPECIFIC, pos,
				       end - pos);
		if (!pos)
			break;

		ie = (struct ieee80211_vendor_ie *)pos;

		/* make sure we can access ie->len */
		BUILD_BUG_ON(offsetof(struct ieee80211_vendor_ie, len) != 1);

		if (ie->len < sizeof(*ie))
			goto cont;

		ie_oui = ie->oui[0] << 16 | ie->oui[1] << 8 | ie->oui[2];
		/*
		 * If oui is other than: 0x0050f2 & 0x506f9a,
		 * we consider it is non-wfa oui.
		 */
		if (ie_oui != WLAN_OUI_MICROSOFT && ie_oui != WLAN_OUI_WFA) {
			/*
			 * if remaining buf len is capable, we copy
			 * this ie to the buf.
			 */
			if (max_len-(*ret_len) >= ie->len+2) {
				DBGLOG(AIS, TRACE,
					   "vendor ie(len=%d, oui=0x%06x)\n",
					   ie->len, ie_oui);
				memcpy(w_pos, pos, ie->len+2);
				w_pos += (ie->len+2);
				(*ret_len) += ie->len+2;
			} else {
				/* Otherwise we give an error msg
				 * and return.
				 */
				DBGLOG(AIS, ERROR,
					"insufficient buf for vendor ie, exit!\n");
				break;
			}
		}
cont:
		pos += 2 + ie->len;
	}
	return *ret_len;
}



