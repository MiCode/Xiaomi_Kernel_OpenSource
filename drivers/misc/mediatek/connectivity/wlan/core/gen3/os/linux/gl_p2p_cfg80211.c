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
 * Id: @(#) gl_p2p_cfg80211.c@@
 */

/*
 * ! \file   gl_p2p_cfg80211.c
 * \brief  Main routines of Linux driver interface for Wi-Fi Direct
 *   using cfg80211 interface
 *
 *  This file contains the main routines of Linux driver for MediaTek Inc. 802.11
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

#include "config.h"

#if CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>

#include "precomp.h"
#include "gl_cfg80211.h"
#include "gl_p2p_ioctl.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wformat"
#endif

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

static BOOLEAN __channel_format_switch(IN struct ieee80211_channel *channel,
				       IN enum nl80211_channel_type channel_type,
				       IN P_RF_CHANNEL_INFO_T prRfChnlInfo,
				       IN P_ENUM_CHNL_EXT_T prChnlSco)
{
	BOOLEAN fgIsValid = FALSE;

	do {
		if (channel == NULL)
			break;

		if (prRfChnlInfo) {
			prRfChnlInfo->ucChannelNum = nicFreq2ChannelNum(channel->center_freq * 1000);

			switch (channel->band) {
			case KAL_BAND_2GHZ:
				prRfChnlInfo->eBand = BAND_2G4;
				break;
			case KAL_BAND_5GHZ:
				prRfChnlInfo->eBand = BAND_5G;
				break;
			default:
				prRfChnlInfo->eBand = BAND_2G4;
				break;
			}

		}

		if (prChnlSco) {

			switch (channel_type) {
			case NL80211_CHAN_NO_HT:
				*prChnlSco = CHNL_EXT_SCN;
				break;
			case NL80211_CHAN_HT20:
				*prChnlSco = CHNL_EXT_SCN;
				break;
			case NL80211_CHAN_HT40MINUS:
				*prChnlSco = CHNL_EXT_SCA;
				break;
			case NL80211_CHAN_HT40PLUS:
				*prChnlSco = CHNL_EXT_SCB;
				break;
			default:
				ASSERT(FALSE);
				*prChnlSco = CHNL_EXT_SCN;
				break;
			}
		}

		fgIsValid = TRUE;

	} while (FALSE);

	return fgIsValid;
}				/* __channel_format_switch */


INT_32 mtk_Netdev_To_RoleIdx(P_GL_P2P_INFO_T prGlP2pInfo, struct net_device *ndev, PUINT_8 pucRoleIdx)
{
	INT_32 i4Ret = -1;
	UINT_32 u4Idx = 0;

	if (pucRoleIdx == NULL)
		return i4Ret;

	for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
		if (prGlP2pInfo->aprRoleHandler[u4Idx] == ndev) {
			*pucRoleIdx = (UINT_8) u4Idx;
			i4Ret = 0;
		}
	}
#if  1
	i4Ret = 0;
	*pucRoleIdx = 0;
#endif
	return i4Ret;

}				/* mtk_Netdev_To_RoleIdx */

struct wireless_dev *
mtk_p2p_cfg80211_add_iface(struct wiphy *wiphy,
			   const char *name,
			   enum nl80211_iftype type, u32 *flags, struct vif_params *params)
{
	/* 2 TODO: Fit kernel 3.10 modification */
#if 0
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	struct net_device *prNewNetDevice = NULL;
	UINT_32 u4Idx = 0;
	P_GL_P2P_INFO_T prP2pInfo = (P_GL_P2P_INFO_T) NULL;

	do {
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		if (prGlueInfo == NULL)
			break;

		prP2pInfo = prGlueInfo->prP2PInfo;

		for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
			if (prP2pInfo->aprRoleHandler[u4Idx] == NULL)
				p2pRoleFsmInit(prGlueInfo->prAdapter, u4Idx);
			else if (prP2pInfo->aprRoleHandler[u4Idx] == prP2pInfo->prDevHandler)
				break;
		}

		if (u4Idx == 2) {
			/* Role port full. */
			break;
		}

		prNewNetDevice = alloc_netdev_mq(sizeof(P_GLUE_INFO_T), name,
						NET_NAME_PREDICTABLE, ether_setup, CFG_MAX_TXQ_NUM);

		if (prNewNetDevice == NULL)
			break;

		prP2pInfo->aprRoleHandler[u4Idx] = prNewNetDevice;

		*((P_GLUE_INFO_T *) netdev_priv(prNewNetDevice)) = prGlueInfo;

		ether_addr_copy(prNewNetDevice->perm_addr, prGlueInfo->prAdapter->rWifiVar.aucInterfaceAddress);
		prNewNetDevice->dev_addr = prNewNetDevice->perm_addr;

		prNewNetDevice->netdev_ops = &p2p_netdev_ops;

#if (MTK_WCN_HIF_SDIO == 0)
		SET_NETDEV_DEV(prNewNetDevice, &(prGlueInfo->rHifInfo.func->dev));
#endif

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
		prNewNetDevice->ieee80211_ptr = &(prP2pInfo->wdev);
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		prNewNetDevice->features = NETIF_F_IP_CSUM;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	} while (FALSE);

	return prNewNetDevice;

#else
	return NULL;
#endif
}				/* mtk_p2p_cfg80211_add_iface */

int mtk_p2p_cfg80211_del_iface(struct wiphy *wiphy,
			       struct wireless_dev *wdev)
{
	/* TODO: */
	return 0;
}				/* mtk_p2p_cfg80211_del_iface */

int mtk_p2p_cfg80211_add_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index, bool pairwise, const u8 *mac_addr, struct key_params *params)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P2P_PARAM_KEY_T rKey;
	UINT_8 ucRoleIdx = 0;
	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, ndev, &ucRoleIdx) != 0)
		return -EINVAL;
#if 1
	/* DBG */
	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_add_key\n");
	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE, "keyIdx = %d pairwise = %d null mac\n", key_index, pairwise);
	}
	DBGLOG(RSN, TRACE, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, TRACE, params->key, params->key_len);
#endif

	if (params->key_len > 32) {
		DBGLOG(RSN, WARN, "key_len [%d] is invalid!\n",
			params->key_len);
		return -EINVAL;
	}

	/* Todo:: By Cipher to set the key */

	kalMemZero(&rKey, sizeof(P2P_PARAM_KEY_T));

	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rKey.ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

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

	if (mac_addr) {
		if (EQUAL_MAC_ADDR(mac_addr, aucZeroMacAddr))
			COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
		else
			COPY_MAC_ADDR(rKey.arBSSID, mac_addr);

		if (pairwise) {
			/*
			 * if (!((rKey.arBSSID[0]
			 *    & rKey.arBSSID[1]
			 *    & rKey.arBSSID[2]
			 *    & rKey.arBSSID[3]
			 *    & rKey.arBSSID[4]
			 *    & rKey.arBSSID[5]) == 0xFF)) {
			 *    rKey.u4KeyIndex |= BIT(31);
			 * }
			 */
			rKey.u4KeyIndex |= BIT(31); /* Tx */
			rKey.u4KeyIndex |= BIT(30); /* Pairwise */
		}
	} else {
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	if (params->key)
		kalMemCopy(rKey.aucKeyMaterial, params->key, params->key_len);
	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = ((ULONG)&(((P_P2P_PARAM_KEY_T) 0)->aucKeyMaterial)) + rKey.u4KeyLength;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAddP2PKey, &rKey, rKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_add_key i4Rslt=%d\n", i4Rslt);

	return i4Rslt;
}

int mtk_p2p_cfg80211_get_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index,
			     bool pairwise,
			     const u8 *mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *))
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_del_key(struct wiphy *wiphy,
			     struct net_device *ndev, u8 key_index, bool pairwise, const u8 *mac_addr)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_REMOVE_KEY_T rRemoveKey;
	INT_32 i4Rslt = -EINVAL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	UINT_8 ucRoleIdx = 0;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, ndev, &ucRoleIdx) < 0)
		return -EINVAL;
#if 1
	/* DBG */
	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_del_key\n");
	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE, "keyIdx = %d pairwise = %d null mac\n", key_index, pairwise);
	}
#endif

	kalMemZero(&rRemoveKey, sizeof(PARAM_REMOVE_KEY_T));

	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rRemoveKey.ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	if (mac_addr)
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
	rRemoveKey.u4KeyIndex = key_index;
	rRemoveKey.u4Length = sizeof(PARAM_REMOVE_KEY_T);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRemoveP2PKey, &rRemoveKey, rRemoveKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting the default mgmt key index
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_p2p_cfg80211_set_mgmt_key(struct wiphy *wiphy, struct net_device *dev, u8 key_index)
{
	DBGLOG(RSN, INFO, "mtk_p2p_cfg80211_set_mgmt_key, kid:%d\n", key_index);

	return 0;
}


int
mtk_p2p_cfg80211_set_default_key(struct wiphy *wiphy,
				 struct net_device *netdev, u8 key_index, bool unicast, bool multicast)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	/* PARAM_DEFAULT_KEY_T rDefaultKey; */
	UINT_8 ucRoleIdx = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Rst = -EINVAL;
	/* UINT_32 u4BufLen = 0; */
	/* BOOLEAN fgDef = FALSE, fgMgtDef = FALSE; */

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, netdev, &ucRoleIdx) != 0)
		return -EINVAL;
#if 1
	/* DBG */
	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_set_default_key\n");
	DBGLOG(RSN, TRACE, "keyIdx = %d unicast = %d multicast = %d\n", key_index, unicast, multicast);
#endif
#if 0
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rDefaultKey.ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	rDefaultKey.ucKeyID = key_index;
	rDefaultKey.ucUnicast = unicast;
	rDefaultKey.ucMulticast = multicast;
	if (rDefaultKey.ucUnicast && !rDefaultKey.ucMulticast)
		return -EINVAL;

	if (rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgDef = TRUE;

	if (!rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgMgtDef = TRUE;
#endif
#if 0
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetDefaultP2PKey,
			   &rDefaultKey, sizeof(PARAM_DEFAULT_KEY_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
#endif

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rst = 0;

	return i4Rst;
}

int mtk_p2p_cfg80211_get_station(struct wiphy *wiphy, struct net_device *ndev,
				const u8 *mac, struct station_info *sinfo)
{
	INT_32 i4RetRslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;
	P2P_STATION_INFO_T rP2pStaInfo;

	ASSERT(wiphy);

	do {
		if ((wiphy == NULL) || (ndev == NULL) || (sinfo == NULL) || (mac == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_get_station\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prP2pGlueInfo = prGlueInfo->prP2PInfo;

		sinfo->filled = 0;

		/* Get station information. */
		/* 1. Inactive time? */
		p2pFuncGetStationInfo(prGlueInfo->prAdapter, (PUINT_8)mac, &rP2pStaInfo);

		/* Inactive time. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		sinfo->filled |= BIT(NL80211_STA_INFO_INACTIVE_TIME);
#else
		sinfo->filled |= STATION_INFO_INACTIVE_TIME;
#endif
		sinfo->inactive_time = rP2pStaInfo.u4InactiveTime;
		sinfo->generation = prP2pGlueInfo->i4Generation;

		i4RetRslt = 0;
	} while (FALSE);

	return i4RetRslt;
}

int mtk_p2p_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;
	P_MSG_P2P_SCAN_REQUEST_T prMsgScanRequest = (P_MSG_P2P_SCAN_REQUEST_T) NULL;
	UINT_32 u4MsgSize = 0, u4Idx = 0;
	INT_32 i4RetRslt = -EINVAL;
	P_RF_CHANNEL_INFO_T prRfChannelInfo = (P_RF_CHANNEL_INFO_T) NULL;
	P_P2P_SSID_STRUCT_T prSsidStruct = (P_P2P_SSID_STRUCT_T) NULL;
	struct ieee80211_channel *prChannel = NULL;
	struct cfg80211_ssid *prSsid = NULL;
	UINT_8 ucBssIdx = 0;

	/* [---------Channel---------] [---------SSID---------][---------IE---------] */

	do {
		if ((wiphy == NULL) || (request == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		prP2pGlueInfo = prGlueInfo->prP2PInfo;

		if (prP2pGlueInfo == NULL) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_scan\n");

		if (prP2pGlueInfo->prScanRequest != NULL) {
			/* There have been a scan request on-going processing. */
			DBGLOG(P2P, TRACE, "There have been a scan request on-going processing.\n");
			break;
		}

		prP2pGlueInfo->prScanRequest = request;

		/* Should find out why the n_channels so many? */
		if (request->n_channels > MAXIMUM_OPERATION_CHANNEL_LIST) {
			request->n_channels = MAXIMUM_OPERATION_CHANNEL_LIST;
			DBGLOG(P2P, TRACE, "Channel list exceed the maximun support.\n");
		}
		/* TODO: */
		/* Find a way to distinct DEV port scan & ROLE port scan.
		 */
		ucBssIdx = P2P_DEV_BSS_INDEX;
		DBGLOG(P2P, TRACE, "Device Port Scan.\n");

		u4MsgSize = sizeof(MSG_P2P_SCAN_REQUEST_T) +
		    (request->n_channels * sizeof(RF_CHANNEL_INFO_T)) +
		    (request->n_ssids * sizeof(PARAM_SSID_T)) + request->ie_len;

		prMsgScanRequest = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, u4MsgSize);

		if (prMsgScanRequest == NULL) {
			DBGLOG(P2P, TRACE, "Allocate MsgScanRequest failed\n");
			i4RetRslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, TRACE, "Generating scan request message.\n");

		prMsgScanRequest->rMsgHdr.eMsgId = MID_MNY_P2P_DEVICE_DISCOVERY;
		prMsgScanRequest->eScanType = SCAN_TYPE_ACTIVE_SCAN;
		prMsgScanRequest->ucBssIdx = ucBssIdx;

		DBGLOG(P2P, INFO, "Requesting channel number:%d.\n", request->n_channels);

		for (u4Idx = 0; u4Idx < request->n_channels; u4Idx++) {
			/* Translate Freq from MHz to channel number. */
			prRfChannelInfo = &(prMsgScanRequest->arChannelListInfo[u4Idx]);
			prChannel = request->channels[u4Idx];

			prRfChannelInfo->ucChannelNum = nicFreq2ChannelNum(prChannel->center_freq * 1000);
			DBGLOG(P2P, TRACE, "Scanning Channel: %d, freq: %d\n",
			       prRfChannelInfo->ucChannelNum, prChannel->center_freq);
			switch (prChannel->band) {
			case KAL_BAND_2GHZ:
				prRfChannelInfo->eBand = BAND_2G4;
				break;
			case KAL_BAND_5GHZ:
				prRfChannelInfo->eBand = BAND_5G;
				break;
			default:
				DBGLOG(P2P, TRACE, "UNKNOWN Band info from supplicant\n");
				prRfChannelInfo->eBand = BAND_NULL;
				break;
			}

			/* Iteration. */
			prRfChannelInfo++;
		}
		prMsgScanRequest->u4NumChannel = request->n_channels;

		DBGLOG(P2P, TRACE, "Finish channel list.\n");

		/* SSID */
		prSsid = request->ssids;
		prSsidStruct = (P_P2P_SSID_STRUCT_T) prRfChannelInfo;
		if (request->n_ssids) {
			ASSERT((ULONG) prSsidStruct == (ULONG)&(prMsgScanRequest->arChannelListInfo[u4Idx]));
			prMsgScanRequest->prSSID = prSsidStruct;
		}

		for (u4Idx = 0; u4Idx < request->n_ssids; u4Idx++) {
			COPY_SSID(prSsidStruct->aucSsid,
				  prSsidStruct->ucSsidLen, request->ssids->ssid, request->ssids->ssid_len);

			prSsidStruct++;
			prSsid++;
		}

		prMsgScanRequest->i4SsidNum = request->n_ssids;

		DBGLOG(P2P, TRACE, "Finish SSID list:%d.\n", request->n_ssids);

		/* IE BUFFERS */
		prMsgScanRequest->pucIEBuf = (PUINT_8) prSsidStruct;
		if (request->ie_len) {
			kalMemCopy(prMsgScanRequest->pucIEBuf, request->ie, request->ie_len);
			prMsgScanRequest->u4IELen = request->ie_len;
		} else {
			prMsgScanRequest->u4IELen = 0;
		}

		DBGLOG(P2P, TRACE, "Finish IE Buffer.\n");

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgScanRequest, MSG_SEND_METHOD_BUF);

		i4RetRslt = 0;
	} while (FALSE);

	return i4RetRslt;
}				/* mtk_p2p_cfg80211_scan */

int mtk_p2p_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = NULL;

	do {
		if (wiphy == NULL)
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_set_wiphy_params\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (changed & WIPHY_PARAM_RETRY_SHORT) {
			/* TODO: */
			DBGLOG(P2P, TRACE, "The RETRY short param is changed.\n");
		}

		if (changed & WIPHY_PARAM_RETRY_LONG) {
			/* TODO: */
			DBGLOG(P2P, TRACE, "The RETRY long param is changed.\n");
		}

		if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
			/* TODO: */
			DBGLOG(P2P, TRACE, "The RETRY fragmentation threshold is changed.\n");
		}

		if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
			/* TODO: */
			DBGLOG(P2P, TRACE, "The RETRY RTS threshold is changed.\n");
		}

		if (changed & WIPHY_PARAM_COVERAGE_CLASS) {
			/* TODO: */
			DBGLOG(P2P, TRACE, "The coverage class is changed???\n");
		}

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_set_wiphy_params */

int mtk_p2p_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ibss_params *params)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_set_txpower(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 enum nl80211_tx_power_setting type, int mbm)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_get_txpower(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 int *dbm)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *ndev, bool enabled, int timeout)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_POWER_MODE ePowerMode;
	PARAM_POWER_MODE_T rPowerMode;
	UINT_32 u4Leng;
	UINT_8 ucRoleIdx;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (enabled)
		ePowerMode = Param_PowerModeFast_PSP;
	else
		ePowerMode = Param_PowerModeCAM;

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_set_power_mgmt ps=%d.\n", enabled);

	if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, ndev, &ucRoleIdx) != 0)
		return -EINVAL;

	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rPowerMode.ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	rPowerMode.ePowerMode = ePowerMode;

	/* p2p_set_power_save */
	kalIoctl(prGlueInfo,
		 wlanoidSet802dot11PowerSaveProfile, &rPowerMode, sizeof(rPowerMode), FALSE, FALSE, TRUE, &u4Leng);

	return 0;
}

int mtk_p2p_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *settings)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	P_MSG_P2P_START_AP_T prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;
	UINT_8 ucRoleIdx = 0;

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_start_ap.\n");

	do {
		if ((wiphy == NULL) || (settings == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &ucRoleIdx) < 0)
			break;

		mtk_p2p_cfg80211_set_channel(wiphy, &settings->chandef);

		prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
									    RAM_TYPE_MSG,
									    (sizeof(MSG_P2P_BEACON_UPDATE_T)
									     +
									     settings->beacon.head_len +
									     settings->beacon.tail_len));

		if (prP2pBcnUpdateMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prP2pBcnUpdateMsg->ucRoleIndex = ucRoleIdx;
		prP2pBcnUpdateMsg->rMsgHdr.eMsgId = MID_MNY_P2P_BEACON_UPDATE;
		pucBuffer = prP2pBcnUpdateMsg->aucBuffer;

		if (settings->beacon.head_len != 0) {
			kalMemCopy(pucBuffer, settings->beacon.head, settings->beacon.head_len);

			prP2pBcnUpdateMsg->u4BcnHdrLen = settings->beacon.head_len;

			prP2pBcnUpdateMsg->pucBcnHdr = pucBuffer;

			pucBuffer = pucBuffer + settings->beacon.head_len;
		} else {
			prP2pBcnUpdateMsg->u4BcnHdrLen = 0;

			prP2pBcnUpdateMsg->pucBcnHdr = NULL;
		}

		if (settings->beacon.tail_len != 0) {
			kalMemCopy(pucBuffer, settings->beacon.tail, settings->beacon.tail_len);

			prP2pBcnUpdateMsg->u4BcnBodyLen = settings->beacon.tail_len;

			prP2pBcnUpdateMsg->pucBcnBody = pucBuffer;
		} else {
			prP2pBcnUpdateMsg->u4BcnBodyLen = 0;
			prP2pBcnUpdateMsg->pucBcnBody = NULL;
		}

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pBcnUpdateMsg, MSG_SEND_METHOD_BUF);

		prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) cnmMemAlloc(prGlueInfo->prAdapter,
								     RAM_TYPE_MSG, sizeof(MSG_P2P_START_AP_T));
		if (prP2pStartAPMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prP2pStartAPMsg->rMsgHdr.eMsgId = MID_MNY_P2P_START_AP;

		prP2pStartAPMsg->u4BcnInterval = settings->beacon_interval;

		prP2pStartAPMsg->u4DtimPeriod = settings->dtim_period;

		switch (settings->hidden_ssid) {
		case NL80211_HIDDEN_SSID_ZERO_LEN:
			prP2pStartAPMsg->eHiddenSsidType = ENUM_HIDDEN_SSID_ZERO_LEN;
			break;
		case NL80211_HIDDEN_SSID_ZERO_CONTENTS:
			prP2pStartAPMsg->eHiddenSsidType = ENUM_HIDDEN_SSID_ZERO_CONTENT;
			break;
		default:
			prP2pStartAPMsg->eHiddenSsidType = ENUM_HIDDEN_SSID_NONE;
			break;
		}

		prP2pStartAPMsg->fgIsPrivacy = settings->privacy;

		prP2pStartAPMsg->ucRoleIdx = ucRoleIdx;

		kalP2PSetRole(prGlueInfo, 2);

		COPY_SSID(prP2pStartAPMsg->aucSsid, prP2pStartAPMsg->u2SsidLen, settings->ssid, settings->ssid_len);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pStartAPMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_start_ap */

/*
 * struct cfg80211_beacon_data {
 *    const u8 *head, *tail;
 *    const u8 *beacon_ies;
 *    const u8 *proberesp_ies;
 *    const u8 *assocresp_ies;
 *    const u8 *probe_resp;
 *
 *    size_t head_len, tail_len;
 *    size_t beacon_ies_len;
 *    size_t proberesp_ies_len;
 *    size_t assocresp_ies_len;
 *    size_t probe_resp_len;
 * };
 */

int mtk_p2p_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_beacon_data *beacon)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;
	UINT_8 ucRoleIdx = 0;

	do {
		if ((wiphy == NULL) || (beacon == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_change_beacon.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &ucRoleIdx) < 0)
			break;

		if ((beacon->head_len != 0) || (beacon->tail_len != 0)) {
			prP2pBcnUpdateMsg =
			    (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
								    RAM_TYPE_MSG,
								    (sizeof(MSG_P2P_BEACON_UPDATE_T)
								     + beacon->head_len + beacon->tail_len));

			if (prP2pBcnUpdateMsg == NULL) {
				ASSERT(FALSE);
				i4Rslt = -ENOMEM;
				break;
			}

			prP2pBcnUpdateMsg->ucRoleIndex = ucRoleIdx;
			prP2pBcnUpdateMsg->rMsgHdr.eMsgId = MID_MNY_P2P_BEACON_UPDATE;
			pucBuffer = prP2pBcnUpdateMsg->aucBuffer;

			if (beacon->head_len != 0) {
				kalMemCopy(pucBuffer, beacon->head, beacon->head_len);

				prP2pBcnUpdateMsg->u4BcnHdrLen = beacon->head_len;

				prP2pBcnUpdateMsg->pucBcnHdr = pucBuffer;

				pucBuffer = pucBuffer + beacon->head_len;
			} else {
				prP2pBcnUpdateMsg->u4BcnHdrLen = 0;

				prP2pBcnUpdateMsg->pucBcnHdr = NULL;
			}

			if (beacon->tail_len != 0) {
				kalMemCopy(pucBuffer, beacon->tail, beacon->tail_len);

				prP2pBcnUpdateMsg->u4BcnBodyLen = beacon->tail_len;

				prP2pBcnUpdateMsg->pucBcnBody = pucBuffer;
			} else {
				prP2pBcnUpdateMsg->u4BcnBodyLen = 0;
				prP2pBcnUpdateMsg->pucBcnBody = NULL;
			}

			kalP2PSetRole(prGlueInfo, 2);

			mboxSendMsg(prGlueInfo->prAdapter,
				    MBOX_ID_0, (P_MSG_HDR_T) prP2pBcnUpdateMsg, MSG_SEND_METHOD_BUF);

		}
	} while (FALSE);
	return i4Rslt;
}				/* mtk_p2p_cfg80211_change_beacon */


int mtk_p2p_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;

	UINT_8 u4Idx = 0;
	UINT_32 rStatus;
	UINT_32 u4BufLen;

	do {
		if (wiphy == NULL) {
			DBGLOG(P2P, ERROR, "wiphy == NULL\n");
			break;
		}

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_stop_ap.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (prGlueInfo == NULL) {
			DBGLOG(P2P, ERROR, "prGlueInfo == NULL\n");
			break;
		}

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &u4Idx) < 0) {
			DBGLOG(P2P, ERROR,
				"mtk_Netdev_To_RoleIdx fail idx(%u).\n", u4Idx);
			break;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidStopApRole,
			&u4Idx, sizeof(unsigned char),
			FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(P2P, ERROR, "stop ap fail 0x%x\n", rStatus);
			break;
		}

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_stop_ap */

/* TODO: */
int mtk_p2p_cfg80211_deauth(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_deauth_request *req)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */
	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_deauth.\n");

	return -EINVAL;
}				/* mtk_p2p_cfg80211_deauth */

/* TODO: */
int mtk_p2p_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_disassoc_request *req)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_disassoc.\n");

	/* not implemented yet */

	return -EINVAL;
}				/* mtk_p2p_cfg80211_disassoc */

int mtk_p2p_cfg80211_remain_on_channel(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       struct ieee80211_channel *chan,
				       unsigned int duration, u64 *cookie)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	P_MSG_P2P_CHNL_REQUEST_T prChnlReqMsg = (P_MSG_P2P_CHNL_REQUEST_T) NULL;

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_remain_on_channel\n");

	do {
		if ((wiphy == NULL) ||
		    /* (dev == NULL) || */
		    (chan == NULL) || (cookie == NULL)) {
			break;
		}

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		*cookie = prGlueP2pInfo->u8Cookie++;

		prChnlReqMsg = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CHNL_REQUEST_T));

		if (prChnlReqMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, INFO, "Cookie: 0x%llx, duration: %d\n", *cookie, duration);

		prChnlReqMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_REQ;
		prChnlReqMsg->u8Cookie = *cookie;
		prChnlReqMsg->u4Duration = duration;
		prChnlReqMsg->eChnlReqType = CH_REQ_TYPE_P2P_LISTEN;

		__channel_format_switch(chan,
					NL80211_CHAN_NO_HT,
					&prChnlReqMsg->rChannelInfo, &prChnlReqMsg->eChnlSco);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prChnlReqMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

/* mtk_p2p_cfg80211_remain_on_channel */

int mtk_p2p_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      u64 cookie)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	P_MSG_P2P_CHNL_ABORT_T prMsgChnlAbort = (P_MSG_P2P_CHNL_ABORT_T) NULL;

	do {
		if (wiphy == NULL)
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		prMsgChnlAbort = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CHNL_ABORT_T));

		if (prMsgChnlAbort == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, INFO, "P2P: cancel remain on channel 0x%llx\n", cookie);

		prMsgChnlAbort->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_ABORT;
		prMsgChnlAbort->u8Cookie = cookie;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlAbort, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_cancel_remain_on_channel */

int mtk_p2p_cfg80211_mgmt_tx(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				struct cfg80211_mgmt_tx_params *params,
				u64 *cookie)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_MGMT_TX_REQUEST_T prMsgTxReq = (P_MSG_P2P_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucFrameBuf = (PUINT_8) NULL;
	PUINT_64 pu8GlCookie = (PUINT_64) NULL;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;
	struct net_device *dev = NULL;

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (params == 0) || (cookie == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		dev = wdev->netdev;
		if (mtk_Netdev_To_RoleIdx(prGlueP2pInfo, dev, &ucRoleIdx) < 0) {
			/* Device Interface. */
			ucBssIdx = P2P_DEV_BSS_INDEX;
		} else {
			ASSERT(ucRoleIdx < KAL_P2P_NUM);
			/* Role Interface. */
			if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) != WLAN_STATUS_SUCCESS) {
				/* Can't find BSS index. */
				break;
			}
		}

		*cookie = prGlueP2pInfo->u8Cookie++;
		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_mgmt_tx, cookie: 0x%llx\n", *cookie);

		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_MGMT_TX_REQUEST_T));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		if (params->offchan) {
			DBGLOG(P2P, INFO, "   Off channel TRUE\n");
			prMsgTxReq->fgIsOffChannel = TRUE;

			__channel_format_switch(params->chan,
						NL80211_CHAN_NO_HT,
						&prMsgTxReq->rChannelInfo, &prMsgTxReq->eChnlExt);
		} else {
			prMsgTxReq->fgIsOffChannel = FALSE;
		}

		if (params->no_cck)
			prMsgTxReq->fgNoneCckRate = TRUE;
		else
			prMsgTxReq->fgNoneCckRate = FALSE;

		if (params->dont_wait_for_ack)
			prMsgTxReq->fgIsWaitRsp = FALSE;
		else
			prMsgTxReq->fgIsWaitRsp = TRUE;
		prMgmtFrame =
		    cnmMgtPktAlloc(prGlueInfo->prAdapter,
				(INT_32) (params->len + sizeof(UINT_64) + MAC_TX_RESERVED_FIELD));
		prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
		if (prMsgTxReq->prMgmtMsduInfo == NULL) {
			/* ASSERT(FALSE); */
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgTxReq->u8Cookie = *cookie;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_P2P_MGMT_TX;
		prMsgTxReq->ucBssIdx = ucBssIdx;

		pucFrameBuf = (PUINT_8) ((ULONG) prMgmtFrame->prPacket + MAC_TX_RESERVED_FIELD);

		pu8GlCookie = (PUINT_64) ((ULONG) prMgmtFrame->prPacket + (ULONG)params->len + MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, params->buf, params->len);

		*pu8GlCookie = *cookie;

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
}				/* mtk_p2p_cfg80211_mgmt_tx */

int mtk_p2p_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 u64 cookie)
{
	INT_32 i4Rslt = -EINVAL;

	return i4Rslt;
}				/* mtk_p2p_cfg80211_mgmt_tx_cancel_wait */

int mtk_p2p_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	switch (params->use_cts_prot) {
	case -1:
		DBGLOG(P2P, TRACE, "CTS protection no change\n");
		break;
	case 0:
		DBGLOG(P2P, TRACE, "CTS protection disable.\n");
		break;
	case 1:
		DBGLOG(P2P, TRACE, "CTS protection enable\n");
		break;
	default:
		DBGLOG(P2P, TRACE, "CTS protection unknown\n");
		break;
	}

	switch (params->use_short_preamble) {
	case -1:
		DBGLOG(P2P, TRACE, "Short prreamble no change\n");
		break;
	case 0:
		DBGLOG(P2P, TRACE, "Short prreamble disable.\n");
		break;
	case 1:
		DBGLOG(P2P, TRACE, "Short prreamble enable\n");
		break;
	default:
		DBGLOG(P2P, TRACE, "Short prreamble unknown\n");
		break;
	}

#if 0
	/* not implemented yet */
	p2pFuncChangeBssParam(prGlueInfo->prAdapter,
			      prBssInfo->fgIsProtection,
			      prBssInfo->fgIsShortPreambleAllowed, prBssInfo->fgUseShortSlotTime,
			      /* Basic rates */
			      /* basic rates len */
			      /* ap isolate */
			      /* ht opmode. */
	    );
#else
	i4Rslt = 0;
#endif

	return i4Rslt;
}				/* mtk_p2p_cfg80211_change_bss */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, struct station_del_parameters *params)
#else
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, const u8 *mac)
#endif
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	UINT_8 aucBcMac[] = BC_MAC_ADDR;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	const UINT_8 *mac = NULL;
#endif

	UINT_8 ucRoleIdx = 0;

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		if (params->mac == NULL)
			mac = aucBcMac;
		else
			mac = params->mac;
#else
		if (mac == NULL)
			mac = aucBcMac;
#endif

		DBGLOG(P2P, INFO, "Del_station: %pM.\n", mac);

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &ucRoleIdx) < 0)
			break;
		/*
		 * prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(MSG_P2P_CONNECTION_ABORT_T),
		 * VIR_MEM_TYPE);
		 */
		prDisconnectMsg =
		    (P_MSG_P2P_CONNECTION_ABORT_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							       sizeof(MSG_P2P_CONNECTION_ABORT_T));

		if (prDisconnectMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prDisconnectMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_ABORT;
		prDisconnectMsg->ucRoleIdx = ucRoleIdx;
		COPY_MAC_ADDR(prDisconnectMsg->aucTargetID, mac);
		prDisconnectMsg->fgSendDeauth = TRUE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		if (params->reason_code == 0)
			prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
		else
			prDisconnectMsg->u2ReasonCode = params->reason_code;
		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_del_station ReasonCode = %u\n", prDisconnectMsg->u2ReasonCode);
#else
		prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
#endif

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prDisconnectMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;

}				/* mtk_p2p_cfg80211_del_station */

int mtk_p2p_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_connect_params *sme)
{
	UINT_8 ucRoleIdx = 0;
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_MSG_P2P_CONNECTION_REQUEST_T prConnReqMsg = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	P_BSS_INFO_T prP2pBssInfo = NULL;
	struct wireless_dev *wdev = NULL;

	do {
		if ((wiphy == NULL) || (dev == NULL) || (sme == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_connect.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		/* Supplicant asks to do connect while driver is disconnecting,
		 * it will lead to install key fail, error is -67 (link has been servered).
		 * Reject this request.Supplicant will handle the connecting request again.
		 */
		wdev = dev->ieee80211_ptr;
		prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(dev);
		ASSERT_BREAK((prGlueInfo != NULL) && (prNetDevPrivate != NULL));
		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prNetDevPrivate->ucBssIdx);

		if (wdev->current_bss && (prP2pBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED)) {
			DBGLOG(P2P, WARN, "Reject this connecting request\n");
			return -EALREADY;
		}

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &ucRoleIdx) < 0)
			break;

		prConnReqMsg =
		    (P_MSG_P2P_CONNECTION_REQUEST_T) cnmMemAlloc(prGlueInfo->prAdapter,
								 RAM_TYPE_MSG,
								 (sizeof(MSG_P2P_CONNECTION_REQUEST_T) + sme->ie_len));

		if (prConnReqMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prConnReqMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;
		prConnReqMsg->ucRoleIdx = ucRoleIdx;

		COPY_SSID(prConnReqMsg->rSsid.aucSsid, prConnReqMsg->rSsid.ucSsidLen, sme->ssid, sme->ssid_len);

		COPY_MAC_ADDR(prConnReqMsg->aucBssid, sme->bssid);

		DBGLOG(P2P, TRACE, "Assoc Req IE Buffer Length:%zu\n", sme->ie_len);
		kalMemCopy(prConnReqMsg->aucIEBuf, sme->ie, sme->ie_len);
		prConnReqMsg->u4IELen = sme->ie_len;

		kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_NONE);

		if (sme->crypto.n_ciphers_pairwise) {
			switch (sme->crypto.ciphers_pairwise[0]) {
			case WLAN_CIPHER_SUITE_WEP40:
			case WLAN_CIPHER_SUITE_WEP104:
				kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_WEP40);
				break;
			case WLAN_CIPHER_SUITE_TKIP:
				kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_TKIP);
				break;
			case WLAN_CIPHER_SUITE_CCMP:
			case WLAN_CIPHER_SUITE_AES_CMAC:
				kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_CCMP);
				break;
			default:
				DBGLOG(REQ, WARN, "invalid cipher pairwise (%d)\n", sme->crypto.ciphers_pairwise[0]);
				kalMemFree(prConnReqMsg, VIR_MEM_TYPE,
						(sizeof(MSG_P2P_CONNECTION_REQUEST_T) + sme->ie_len));
				return -EINVAL;
			}
		}

		__channel_format_switch(sme->channel,
					NL80211_CHAN_NO_HT,
					&prConnReqMsg->rChannelInfo,
					&prConnReqMsg->eChnlSco);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prConnReqMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_connect */

int mtk_p2p_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev, u16 reason_code)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	UINT_8 aucBCAddr[] = BC_MAC_ADDR;
	UINT_8 ucRoleIdx = 0;

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_disconnect.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &ucRoleIdx) < 0)
			break;
/* prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(P_MSG_P2P_CONNECTION_ABORT_T), VIR_MEM_TYPE); */
		prDisconnMsg =
		    (P_MSG_P2P_CONNECTION_ABORT_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							       sizeof(MSG_P2P_CONNECTION_ABORT_T));

		if (prDisconnMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prDisconnMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_ABORT;
		prDisconnMsg->ucRoleIdx = ucRoleIdx;
		prDisconnMsg->u2ReasonCode = reason_code;
		prDisconnMsg->fgSendDeauth = TRUE;
		COPY_MAC_ADDR(prDisconnMsg->aucTargetID, aucBCAddr);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prDisconnMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_disconnect */

#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
int
mtk_p2p_cfg80211_change_iface(IN struct wiphy *wiphy,
			      IN struct net_device *ndev,
			      IN enum nl80211_iftype type, IN struct vif_params *params)
#else
int
mtk_p2p_cfg80211_change_iface(IN struct wiphy *wiphy,
			      IN struct net_device *ndev,
			      IN enum nl80211_iftype type, IN u32 *flags, IN struct vif_params *params)
#endif
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_SWITCH_OP_MODE_T prSwitchModeMsg = (P_MSG_P2P_SWITCH_OP_MODE_T) NULL;
	UINT_8 ucRoleIdx = 0;

	do {
		if ((wiphy == NULL) || (ndev == NULL))
			break;

		DBGLOG(P2P, TRACE, "Change interface type to %d\n", type);

		if (ndev->ieee80211_ptr)
			ndev->ieee80211_ptr->iftype = type;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, ndev, &ucRoleIdx) != 0) {
			DBGLOG(P2P, TRACE, "Device Interface no need to change interface type.\n");
			return 0;
		}
		/* Switch OP MOde. */
		prSwitchModeMsg =
		    (P_MSG_P2P_SWITCH_OP_MODE_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							     sizeof(MSG_P2P_SWITCH_OP_MODE_T));

		if (prSwitchModeMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prSwitchModeMsg->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
		prSwitchModeMsg->ucRoleIdx = ucRoleIdx;

		switch (type) {
		case NL80211_IFTYPE_P2P_CLIENT:
		case NL80211_IFTYPE_STATION:
			prSwitchModeMsg->eOpMode = OP_MODE_INFRASTRUCTURE;
			kalP2PSetRole(prGlueInfo, 1);
			break;
		case NL80211_IFTYPE_AP:
		case NL80211_IFTYPE_P2P_GO:
			prSwitchModeMsg->eOpMode = OP_MODE_ACCESS_POINT;
			kalP2PSetRole(prGlueInfo, 2);
			break;
		default:
			prSwitchModeMsg->eOpMode = OP_MODE_P2P_DEVICE;
			kalP2PSetRole(prGlueInfo, 0);
			break;
		}

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prSwitchModeMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;

	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_change_iface */

int mtk_p2p_cfg80211_set_channel(IN struct wiphy *wiphy,
				 struct cfg80211_chan_def *chandef)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	RF_CHANNEL_INFO_T rRfChnlInfo;
	UINT_8 ucRoleIdx = 0;
	struct net_device *dev = NULL;

	do {
		if ((wiphy == NULL) || (chandef == NULL))
			break;
		dev = (struct net_device *)wiphy_dev(wiphy);

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		__channel_format_switch(chandef->chan, 0, &rRfChnlInfo, NULL);

		if (mtk_Netdev_To_RoleIdx(prGlueInfo->prP2PInfo, dev, &ucRoleIdx) < 0)
			break;

		p2pFuncSetChannel(prGlueInfo->prAdapter, ucRoleIdx, &rRfChnlInfo);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_set_channel */

int
mtk_p2p_cfg80211_set_bitrate_mask(IN struct wiphy *wiphy,
				  IN struct net_device *dev,
				  IN const u8 *peer, IN const struct cfg80211_bitrate_mask *mask)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	do {
		if ((wiphy == NULL) || (dev == NULL) || (mask == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_set_bitrate_mask\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		/* TODO: Set bitrate mask of the peer? */

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_set_bitrate_mask */

void mtk_p2p_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  IN u16 frame_type, IN bool reg)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	do {
		if (wiphy == NULL)
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_mgmt_frame_register\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		switch (frame_type) {
		case MAC_FRAME_PROBE_REQ:
			if (reg) {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(P2P, TRACE, "Open packet filer probe request\n");
			} else {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(P2P, TRACE, "Close packet filer probe request\n");
			}
			break;
		case MAC_FRAME_ACTION:
			if (reg) {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(P2P, TRACE, "Open packet filer action frame.\n");
			} else {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(P2P, TRACE, "Close packet filer action frame.\n");
			}
			break;
#if CFG_SUPPORT_SOFTAP_WPA3
		case MAC_FRAME_AUTH:
			if (reg) {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter
					|= PARAM_PACKET_FILTER_AUTH;
				DBGLOG(P2P, TRACE, "Open packet filer auth request\n");
			} else {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter
					&= ~PARAM_PACKET_FILTER_AUTH;
				DBGLOG(P2P, TRACE, "Close packet filer auth request\n");
			}
			break;
		case MAC_FRAME_ASSOC_REQ:
			if (reg) {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter
					|= PARAM_PACKET_FILTER_ASSOC_REQ;
				DBGLOG(P2P, TRACE, "Open packet filer assoc request\n");
			} else {
				prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter
					&= ~PARAM_PACKET_FILTER_ASSOC_REQ;
				DBGLOG(P2P, TRACE, "Close packet filer assoc request\n");
			}
			break;
#endif

		default:
			DBGLOG(P2P, ERROR, "Ask frog to add code for mgmt:%x\n", frame_type);
			break;
		}

		set_bit(GLUE_FLAG_FRAME_FILTER_BIT, &prGlueInfo->ulFlag);

		/* wake up main thread */
		wake_up_interruptible(&prGlueInfo->waitq);

		if (in_interrupt())
			DBGLOG(P2P, TRACE, "It is in interrupt level\n");
	} while (FALSE);

}				/* mtk_p2p_cfg80211_mgmt_frame_register */

#if CONFIG_NL80211_TESTMODE

int mtk_p2p_cfg80211_testmode_cmd(IN struct wiphy *wiphy, IN struct wireless_dev *wdev, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_PARAMS prParams = NULL;
	INT_32 i4Status = -EINVAL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (data && len) {
		prParams = (P_NL80211_DRIVER_TEST_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "data is NULL\n");
		return i4Status;
	}

	if (prParams->index >> 24 == 0x01) {
		/* New version */
		prParams->index = prParams->index & ~BITS(24, 31);
	} else {
		/* Old version */
		i4Status = mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(wiphy, data, len);
		return i4Status;
	}

	switch (prParams->index) {
	case 1:	/* P2P Simga */
#if CFG_SUPPORT_HOTSPOT_OPTIMIZATION
		{
			P_NL80211_DRIVER_SW_CMD_PARAMS prParamsCmd;

			prParamsCmd = (P_NL80211_DRIVER_SW_CMD_PARAMS) data;

			if ((prParamsCmd->adr & 0xffff0000) == 0xffff0000) {
				i4Status = mtk_p2p_cfg80211_testmode_sw_cmd(wiphy, data, len);
				break;
			}
		}
#endif
		i4Status = mtk_p2p_cfg80211_testmode_p2p_sigma_cmd(wiphy, data, len);
		break;
#if CFG_SUPPORT_WFD
	case 2:	/* WFD */
		/* use normal driver command wifi_display */
		/* i4Status = mtk_p2p_cfg80211_testmode_wfd_update_cmd(wiphy, data, len); */
		break;
#endif
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	case 3:	/* Hotspot Client Management */
		i4Status = mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd(wiphy, data, len);
		break;
#endif
	case 0x10:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
		break;
#if CFG_SUPPORT_NFC_BEAM_PLUS
	case 0x11: /* NFC Beam + Indication */
		if (data && len) {
			P_NL80211_DRIVER_SET_NFC_PARAMS prParams = (P_NL80211_DRIVER_SET_NFC_PARAMS) data;

			DBGLOG(P2P, INFO, "NFC: BEAM[%d]\n", prParams->NFC_Enable);
		}
		break;
	case 0x12: /* NFC Beam + Indication */
		DBGLOG(P2P, INFO, "NFC: Polling\n");
		i4Status = mtk_cfg80211_testmode_get_scan_done(wiphy, data, len, prGlueInfo);
		break;
#endif

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	case 0x33:
		i4Status = mtk_p2p_cfg80211_testmode_hotspot_config_cmd(wiphy, data, len);
		break;
#endif
	default:
		i4Status = -EINVAL;
		break;
	}

	DBGLOG(P2P, TRACE, "prParams->index=%d, status=%d\n", prParams->index, i4Status);

	return i4Status;
}

int mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	NL80211_DRIVER_TEST_PRE_PARAMS rParams;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	/* P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T)NULL; */
	UINT_32 index_mode;
	UINT_32 index;
	INT_32 value;
	int status = 0;
	UINT_32 u4Leng;
	UINT_8 ucBssIdx;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	kalMemZero(&rParams, sizeof(NL80211_DRIVER_TEST_PRE_PARAMS));

	prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo;
	/* prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings; */

	if (data && len)
		memcpy(&rParams, data, len);

	DBGLOG(P2P, TRACE, "NL80211_ATTR_TESTDATA, idx_mode=%d idx=%d value=%u\n",
	       rParams.idx_mode, rParams.idx, rParams.value);

	index_mode = rParams.idx_mode;
	index = rParams.idx;
	value = rParams.value;

	/* 3 FIX ME: Add p2p role index selection */
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, 0, &ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	switch (index) {
	case 0:		/* Listen CH */
		break;
	case 1:		/* P2p mode */
		break;
	case 4:		/* Noa duration */
		prP2pSpecificBssInfo->rNoaParam.u4NoaDurationMs = value;
		/* only to apply setting when setting NOA count */
		/* status = mtk_p2p_wext_set_noa_param(prDev, info, wrqu, (char *)&prP2pSpecificBssInfo->rNoaParam); */
		break;
	case 5:		/* Noa interval */
		prP2pSpecificBssInfo->rNoaParam.u4NoaIntervalMs = value;
		/* only to apply setting when setting NOA count */
		/* status = mtk_p2p_wext_set_noa_param(prDev, info, wrqu, (char *)&prP2pSpecificBssInfo->rNoaParam); */
		break;
	case 6:		/* Noa count */
		prP2pSpecificBssInfo->rNoaParam.u4NoaCount = value;
		/* status = mtk_p2p_wext_set_noa_param(prDev, info, wrqu, (char *)&prP2pSpecificBssInfo->rNoaParam); */
		break;
	case 100:		/* Oper CH */
		/* 20110920 - frog: User configurations are placed in ConnSettings. */
		/* prP2pConnSettings->ucOperatingChnl = value; */
		break;
	case 101:		/* Local config Method, for P2P SDK */
		/* prP2pConnSettings->u2LocalConfigMethod = value; */
		break;
	case 102:		/* Sigma P2p reset */
		/* kalMemZero(prP2pConnSettings->aucTargetDevAddr, MAC_ADDR_LEN); */
		/* prP2pConnSettings->eConnectionPolicy = ENUM_P2P_CONNECTION_POLICY_AUTO; */
		/* p2pFsmUninit(prGlueInfo->prAdapter); */
		/* p2pFsmInit(prGlueInfo->prAdapter); */
		break;
	case 103:		/* WPS MODE */
		kalP2PSetWscMode(prGlueInfo, value);
		break;
	case 104:		/* P2p send persence, duration */
		break;
	case 105:		/* P2p send persence, interval */
		break;
	case 106:		/* P2P set sleep  */
		{
			PARAM_POWER_MODE_T rPowerMode;

			rPowerMode.ePowerMode = Param_PowerModeMAX_PSP;
			rPowerMode.ucBssIdx = ucBssIdx;

			kalIoctl(prGlueInfo,
				 wlanoidSet802dot11PowerSaveProfile,
				 &rPowerMode, sizeof(rPowerMode), FALSE, FALSE, TRUE, &u4Leng);
		}
		break;
	case 107:		/* P2P set opps, CTWindowl */
		prP2pSpecificBssInfo->rOppPsParam.u4CTwindowMs = value;
		/*
		 * status = mtk_p2p_wext_set_oppps_param(prDev, info, wrqu,
		 * (char *)&prP2pSpecificBssInfo->rOppPsParam);
		 */
		break;
	case 108:		/* p2p_set_power_save */
		{
			PARAM_POWER_MODE_T rPowerMode;

			rPowerMode.ePowerMode = value;
			rPowerMode.ucBssIdx = ucBssIdx;

			kalIoctl(prGlueInfo,
				 wlanoidSet802dot11PowerSaveProfile,
				 &rPowerMode, sizeof(rPowerMode), FALSE, FALSE, TRUE, &u4Leng);
		}
		break;
	default:
		break;
	}

	return status;

}

int mtk_p2p_cfg80211_testmode_p2p_sigma_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_P2P_SIGMA_PARAMS prParams = (P_NL80211_DRIVER_P2P_SIGMA_PARAMS) NULL;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	/* P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T)NULL; */
	UINT_32 index;
	INT_32 value;
	int status = 0;
	UINT_32 u4Leng;
	UINT_8 ucBssIdx;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo;
	/* prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings; */

	if (data && len) {
		prParams = (P_NL80211_DRIVER_P2P_SIGMA_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "data is NULL\n");
		return -EINVAL;
	}

	index = (INT_32) prParams->idx;
	value = (INT_32) prParams->value;

	DBGLOG(P2P, TRACE, "NL80211_ATTR_TESTDATA, idx=%u value=%u\n",
	       prParams->idx, prParams->value);

	/* 3 FIX ME: Add p2p role index selection */
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, 0, &ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	switch (index) {
	case 0:		/* Listen CH */
		break;
	case 1:		/* P2p mode */
		break;
	case 4:		/* Noa duration */
		prP2pSpecificBssInfo->rNoaParam.u4NoaDurationMs = value;
		/* only to apply setting when setting NOA count */
		/* status = mtk_p2p_wext_set_noa_param(prDev, info, wrqu, (char *)&prP2pSpecificBssInfo->rNoaParam); */
		break;
	case 5:		/* Noa interval */
		prP2pSpecificBssInfo->rNoaParam.u4NoaIntervalMs = value;
		/* only to apply setting when setting NOA count */
		/* status = mtk_p2p_wext_set_noa_param(prDev, info, wrqu, (char *)&prP2pSpecificBssInfo->rNoaParam); */
		break;
	case 6:		/* Noa count */
		prP2pSpecificBssInfo->rNoaParam.u4NoaCount = value;
		/* status = mtk_p2p_wext_set_noa_param(prDev, info, wrqu, (char *)&prP2pSpecificBssInfo->rNoaParam); */
		break;
	case 100:		/* Oper CH */
		/* 20110920 - frog: User configurations are placed in ConnSettings. */
		/* prP2pConnSettings->ucOperatingChnl = value; */
		break;
	case 101:		/* Local config Method, for P2P SDK */
		/* prP2pConnSettings->u2LocalConfigMethod = value; */
		break;
	case 102:		/* Sigma P2p reset */
		/* kalMemZero(prP2pConnSettings->aucTargetDevAddr, MAC_ADDR_LEN); */
		/* prP2pConnSettings->eConnectionPolicy = ENUM_P2P_CONNECTION_POLICY_AUTO; */
		break;
	case 103:		/* WPS MODE */
		kalP2PSetWscMode(prGlueInfo, value);
		break;
	case 104:		/* P2p send persence, duration */
		break;
	case 105:		/* P2p send persence, interval */
		break;
	case 106:		/* P2P set sleep  */
		{
			PARAM_POWER_MODE_T rPowerMode;

			rPowerMode.ePowerMode = Param_PowerModeMAX_PSP;
			rPowerMode.ucBssIdx = ucBssIdx;

			kalIoctl(prGlueInfo,
				 wlanoidSet802dot11PowerSaveProfile,
				 &rPowerMode, sizeof(rPowerMode), FALSE, FALSE, TRUE, &u4Leng);
		}
		break;
	case 107:		/* P2P set opps, CTWindowl */
		prP2pSpecificBssInfo->rOppPsParam.u4CTwindowMs = value;
		/*
		 * status = mtk_p2p_wext_set_oppps_param(prDev, info, wrqu,
		 * (char *)&prP2pSpecificBssInfo->rOppPsParam);
		 */
		break;
	case 108:		/* p2p_set_power_save */
		{
			PARAM_POWER_MODE_T rPowerMode;

			rPowerMode.ePowerMode = value;
			rPowerMode.ucBssIdx = ucBssIdx;

			kalIoctl(prGlueInfo,
				 wlanoidSet802dot11PowerSaveProfile,
				 &rPowerMode, sizeof(rPowerMode), FALSE, FALSE, TRUE, &u4Leng);
		}
		break;
	case 109:		/* Max Clients */
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
		kalP2PSetMaxClients(prGlueInfo, value);
#endif
		break;
	case 110:		/* Hotspot WPS mode */
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
		kalIoctl(prGlueInfo, wlanoidSetP2pWPSmode, &value, sizeof(value), FALSE, FALSE, TRUE, &u4Leng);
#endif
		break;
	default:
		break;
	}

	return status;

}

#if CFG_SUPPORT_WFD && 0
/* obsolete/decrepated */
int mtk_p2p_cfg80211_testmode_wfd_update_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_WFD_PARAMS prParams = (P_NL80211_DRIVER_WFD_PARAMS) NULL;
	int status = 0;
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
	P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T prMsgWfdCfgUpdate = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	prParams = (P_NL80211_DRIVER_WFD_PARAMS) data;

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_testmode_wfd_update_cmd\n");

#if 1

	DBGLOG(P2P, INFO, "WFD Enable:%x\n", prParams->WfdEnable);
	DBGLOG(P2P, INFO, "WFD Session Available:%x\n", prParams->WfdSessionAvailable);
	DBGLOG(P2P, INFO, "WFD Couple Sink Status:%x\n", prParams->WfdCoupleSinkStatus);
	/* aucReserved0[2] */
	DBGLOG(P2P, INFO, "WFD Device Info:%x\n", prParams->WfdDevInfo);
	DBGLOG(P2P, INFO, "WFD Control Port:%x\n", prParams->WfdControlPort);
	DBGLOG(P2P, INFO, "WFD Maximum Throughput:%x\n", prParams->WfdMaximumTp);
	DBGLOG(P2P, INFO, "WFD Extend Capability:%x\n", prParams->WfdExtendCap);
	DBGLOG(P2P, INFO, "WFD Couple Sink Addr " MACSTR "\n", MAC2STR(prParams->WfdCoupleSinkAddress));
	DBGLOG(P2P, INFO, "WFD Associated BSSID " MACSTR "\n", MAC2STR(prParams->WfdAssociatedBssid));
	/* UINT_8 aucVideolp[4]; */
	/* UINT_8 aucAudiolp[4]; */
	DBGLOG(P2P, INFO, "WFD Video Port:%x\n", prParams->WfdVideoPort);
	DBGLOG(P2P, INFO, "WFD Audio Port:%x\n", prParams->WfdAudioPort);
	DBGLOG(P2P, INFO, "WFD Flag:%x\n", prParams->WfdFlag);
	DBGLOG(P2P, INFO, "WFD Policy:%x\n", prParams->WfdPolicy);
	DBGLOG(P2P, INFO, "WFD State:%x\n", prParams->WfdState);
	/* UINT_8 aucWfdSessionInformationIE[24*8]; */
	DBGLOG(P2P, INFO, "WFD Session Info Length:%x\n", prParams->WfdSessionInformationIELen);
	/* UINT_8 aucReserved1[2]; */
	DBGLOG(P2P, INFO, "WFD Primary Sink Addr " MACSTR "\n", MAC2STR(prParams->aucWfdPrimarySinkMac));
	DBGLOG(P2P, INFO, "WFD Secondary Sink Addr " MACSTR "\n", MAC2STR(prParams->aucWfdSecondarySinkMac));
	DBGLOG(P2P, INFO, "WFD Advanced Flag:%x\n", prParams->WfdAdvanceFlag);
	DBGLOG(P2P, INFO, "WFD Sigma mode:%x\n", prParams->WfdSigmaMode);
	/* UINT_8 aucReserved2[64]; */
	/* UINT_8 aucReserved3[64]; */
	/* UINT_8 aucReserved4[64]; */

#endif

	prWfdCfgSettings = &(prGlueInfo->prAdapter->rWifiVar.rWfdConfigureSettings);

	kalMemCopy(&prWfdCfgSettings->u4WfdCmdType, &prParams->WfdCmdType, sizeof(WFD_CFG_SETTINGS_T));

	prMsgWfdCfgUpdate = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_WFD_CONFIG_SETTINGS_CHANGED_T));

	if (prMsgWfdCfgUpdate == NULL) {
		ASSERT(FALSE);
		return status;
	}

	prMsgWfdCfgUpdate->rMsgHdr.eMsgId = MID_MNY_P2P_WFD_CFG_UPDATE;
	prMsgWfdCfgUpdate->prWfdCfgSettings = prWfdCfgSettings;

	mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgWfdCfgUpdate, MSG_SEND_METHOD_BUF);
#if 0				/* Test Only */
/* prWfdCfgSettings->ucWfdEnable = 1; */
/* prWfdCfgSettings->u4WfdFlag |= WFD_FLAGS_DEV_INFO_VALID; */
	prWfdCfgSettings->u4WfdFlag |= WFD_FLAGS_DEV_INFO_VALID;
	prWfdCfgSettings->u2WfdDevInfo = 123;
	prWfdCfgSettings->u2WfdControlPort = 456;
	prWfdCfgSettings->u2WfdMaximumTp = 789;

	prWfdCfgSettings->u4WfdFlag |= WFD_FLAGS_SINK_INFO_VALID;
	prWfdCfgSettings->ucWfdCoupleSinkStatus = 0xAB;
	{
		UINT_8 aucTestAddr[MAC_ADDR_LEN] = { 0x77, 0x66, 0x55, 0x44, 0x33, 0x22 };

		COPY_MAC_ADDR(prWfdCfgSettings->aucWfdCoupleSinkAddress, aucTestAddr);
	}

	prWfdCfgSettings->u4WfdFlag |= WFD_FLAGS_EXT_CAPABILITY_VALID;
	prWfdCfgSettings->u2WfdExtendCap = 0xCDE;

#endif

	return status;

}
#endif /*  CFG_SUPPORT_WFD */

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER

int mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_hotspot_block_PARAMS prParams = (P_NL80211_DRIVER_hotspot_block_PARAMS) NULL;
	int fgIsValid = 0;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (data && len)
		prParams = (P_NL80211_DRIVER_hotspot_block_PARAMS) data;

	if (!prParams)
		return fgIsValid;

	DBGLOG(P2P, INFO, "%s" MACSTR "\n", prParams->ucblocked?"Block":"Unblock", MAC2STR(prParams->aucBssid));

	fgIsValid = kalP2PSetBlackList(prGlueInfo, prParams->aucBssid, prParams->ucblocked);

	return fgIsValid;

}

#endif

int mtk_p2p_cfg80211_testmode_sw_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_SW_CMD_PARAMS prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, TRACE, "--> %s()\n", __func__);

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


#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
int mtk_p2p_cfg80211_testmode_hotspot_config_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_P2P_SIGMA_PARAMS prParams = (P_NL80211_DRIVER_P2P_SIGMA_PARAMS) NULL;

	UINT_32 index;
	UINT_32 value;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (data && len) {
		prParams = (P_NL80211_DRIVER_P2P_SIGMA_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "data is NULL or len is 0\n");
		return -EINVAL;
	}

	index = prParams->idx;
	value = prParams->value;

	DBGLOG(P2P, TRACE, "HOTSPOT CONFIG, idx=%u value=%u\n",
	       prParams->idx, prParams->value);
	switch (index) {
	case 1:		/* Max Clients */
		kalP2PSetMaxClients(prGlueInfo, value);
		break;
	default:
		break;
	}

	return 0;
}
#endif /*CFG_SUPPORT_HOTSPOT_WPS_MANAGER*/

#endif /* CONFIG_NL80211_TESTMODE */

#endif /* CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
